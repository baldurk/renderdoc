/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "serialiser.h"
#include <errno.h>
#include "3rdparty/lz4/lz4.h"
#include "common/timing.h"
#include "core/core.h"
#include "serialise/string_utils.h"

#if ENABLED(RDOC_MSVS)
// warning C4422: 'snprintf' : too many arguments passed for format string
// false positive as VS is trying to parse renderdoc's custom format strings
#pragma warning(disable : 4422)
#endif

#if ENABLED(RDOC_DEVEL)

int64_t Chunk::m_LiveChunks = 0;
int64_t Chunk::m_TotalMem = 0;
int64_t Chunk::m_MaxChunks = 0;

#endif

const uint32_t Serialiser::MAGIC_HEADER = MAKE_FOURCC('R', 'D', 'O', 'C');
const uint64_t Serialiser::BufferAlignment = 64;

// based on blockStreaming_doubleBuffer.c in lz4 examples
struct CompressedFileIO
{
  // large block size
  static const size_t BlockSize = 64 * 1024;

  CompressedFileIO(FILE *f)
  {
    m_F = f;
    LZ4_resetStream(&m_LZ4Comp);
    LZ4_setStreamDecode(&m_LZ4Decomp, NULL, 0);
    m_CompressedSize = m_UncompressedSize = 0;
    m_PageIdx = m_PageOffset = 0;
    m_PageData = 0;

    m_CompressSize = LZ4_COMPRESSBOUND(BlockSize);
    m_CompressBuf = new byte[m_CompressSize];
  }

  ~CompressedFileIO() { SAFE_DELETE_ARRAY(m_CompressBuf); }
  uint64_t GetCompressedSize() { return m_CompressedSize; }
  uint64_t GetUncompressedSize() { return m_UncompressedSize; }
  // write out some data - accumulate into the input pages, then
  // when a page is full call Flush() to flush it out to disk
  void Write(const void *data, size_t len)
  {
    if(data == NULL || len == 0)
      return;

    m_UncompressedSize += (uint64_t)len;

    const byte *src = (const byte *)data;

    size_t remainder = 0;

    // loop continually, writing up to BlockSize out of what remains of data
    do
    {
      remainder = 0;

      // if we're about to copy more than the page, copy only
      // what will fit, then copy the remainder after flushing
      if(m_PageOffset + len > BlockSize)
      {
        remainder = len - (BlockSize - m_PageOffset);
        len = BlockSize - m_PageOffset;
      }

      memcpy(m_InPages[m_PageIdx] + m_PageOffset, src, len);
      m_PageOffset += len;

      if(remainder > 0)
      {
        Flush();    // this will swap the input pages and reset the page offset

        src += len;
        len = remainder;
      }
    } while(remainder > 0);
  }

  // flush out the current page to disk
  void Flush()
  {
    // m_PageOffset is the amount written, usually equal to BlockSize except the last block.
    int32_t compSize = LZ4_compress_fast_continue(&m_LZ4Comp, (const char *)m_InPages[m_PageIdx],
                                                  (char *)m_CompressBuf, (int)m_PageOffset,
                                                  (int)m_CompressSize, 1);

    if(compSize < 0)
    {
      RDCERR("Error compressing: %i", compSize);
      return;
    }

    FileIO::fwrite(&compSize, sizeof(compSize), 1, m_F);
    FileIO::fwrite(m_CompressBuf, 1, compSize, m_F);

    m_CompressedSize += uint64_t(compSize) + sizeof(int32_t);

    m_PageOffset = 0;
    m_PageIdx = 1 - m_PageIdx;
  }

  // Reset back to 0, only makes sense when reading as writing can't be undone
  void Reset()
  {
    LZ4_setStreamDecode(&m_LZ4Decomp, NULL, 0);
    m_CompressedSize = m_UncompressedSize = 0;
    m_PageIdx = 0;
    m_PageOffset = 0;
  }

  // read out some data - if the input page is empty we fill
  // the next page with data from disk
  void Read(byte *data, size_t len)
  {
    if(data == NULL || len == 0)
      return;

    m_UncompressedSize += (uint64_t)len;

    // loop continually, writing up to BlockSize out of what remains of data
    do
    {
      size_t readamount = len;

      // if we're about to copy more than the page, copy only
      // what will fit, then copy the remainder after refilling
      if(readamount > m_PageData)
        readamount = m_PageData;

      if(readamount > 0)
      {
        memcpy(data, m_InPages[m_PageIdx] + m_PageOffset, readamount);

        m_PageOffset += readamount;
        m_PageData -= readamount;

        data += readamount;
        len -= readamount;
      }

      if(len > 0)
        FillBuffer();    // this will swap the input pages and reset the page offset
    } while(len > 0);
  }

  void FillBuffer()
  {
    int32_t compSize = 0;

    FileIO::fread(&compSize, sizeof(compSize), 1, m_F);
    size_t numRead = FileIO::fread(m_CompressBuf, 1, compSize, m_F);

    m_CompressedSize += uint64_t(compSize);

    m_PageIdx = 1 - m_PageIdx;

    int32_t decompSize = LZ4_decompress_safe_continue(
        &m_LZ4Decomp, (const char *)m_CompressBuf, (char *)m_InPages[m_PageIdx], compSize, BlockSize);

    if(decompSize < 0)
    {
      RDCERR("Error decompressing: %i (%i / %i)", decompSize, int(numRead), compSize);
      return;
    }

    m_PageOffset = 0;
    m_PageData = decompSize;
  }

  static void Decompress(byte *destBuf, const byte *srcBuf, size_t len)
  {
    LZ4_streamDecode_t lz4;
    LZ4_setStreamDecode(&lz4, NULL, 0);

    const byte *srcBufEnd = srcBuf + len;

    while(srcBuf + 4 < srcBufEnd)
    {
      const int32_t *compSize = (const int32_t *)srcBuf;
      srcBuf = (const byte *)(compSize + 1);

      if(srcBuf + *compSize > srcBufEnd)
        break;

      int32_t decompSize = LZ4_decompress_safe_continue(&lz4, (const char *)srcBuf, (char *)destBuf,
                                                        *compSize, BlockSize);

      if(decompSize < 0)
        return;

      srcBuf += *compSize;
      destBuf += decompSize;
    }
  }

  LZ4_stream_t m_LZ4Comp;
  LZ4_streamDecode_t m_LZ4Decomp;
  FILE *m_F;
  uint64_t m_CompressedSize, m_UncompressedSize;

  byte m_InPages[2][BlockSize];
  size_t m_PageIdx, m_PageOffset, m_PageData;

  byte *m_CompressBuf;
  size_t m_CompressSize;
};

Chunk::Chunk(Serialiser *ser, uint32_t chunkType, bool temporary)
{
  m_Length = (uint32_t)ser->GetOffset();

  RDCASSERT(ser->GetOffset() < 0xffffffff);

  m_ChunkType = chunkType;

  m_Temporary = temporary;

  if(ser->HasAlignedData())
  {
    m_Data = Serialiser::AllocAlignedBuffer(m_Length);
    m_AlignedData = true;
  }
  else
  {
    m_Data = new byte[m_Length];
    m_AlignedData = false;
  }

  memcpy(m_Data, ser->GetRawPtr(0), m_Length);

  if(ser->GetDebugText())
    m_DebugStr = ser->GetDebugStr();

  ser->Rewind();

#if ENABLED(RDOC_DEVEL)
  int64_t newval = Atomic::Inc64(&m_LiveChunks);
  Atomic::ExchAdd64(&m_TotalMem, m_Length);

  if(newval > m_MaxChunks)
  {
    int breakpointme = 0;
    (void)breakpointme;
  }

  m_MaxChunks = RDCMAX(newval, m_MaxChunks);
#endif
}

Chunk *Chunk::Duplicate()
{
  Chunk *ret = new Chunk();
  ret->m_DebugStr = m_DebugStr;
  ret->m_Length = m_Length;
  ret->m_ChunkType = m_ChunkType;
  ret->m_Temporary = m_Temporary;
  ret->m_AlignedData = m_AlignedData;

  if(m_AlignedData)
    ret->m_Data = Serialiser::AllocAlignedBuffer(m_Length);
  else
    ret->m_Data = new byte[m_Length];

  memcpy(ret->m_Data, m_Data, m_Length);

#if ENABLED(RDOC_DEVEL)
  int64_t newval = Atomic::Inc64(&m_LiveChunks);
  Atomic::ExchAdd64(&m_TotalMem, m_Length);

  if(newval > m_MaxChunks)
  {
    int breakpointme = 0;
    (void)breakpointme;
  }

  m_MaxChunks = RDCMAX(newval, m_MaxChunks);
#endif

  return ret;
}

Chunk::~Chunk()
{
#if ENABLED(RDOC_DEVEL)
  Atomic::Dec64(&m_LiveChunks);
  Atomic::ExchAdd64(&m_TotalMem, -int64_t(m_Length));
#endif

  if(m_AlignedData)
  {
    if(m_Data)
      Serialiser::FreeAlignedBuffer(m_Data);

    m_Data = NULL;
  }
  else
  {
    SAFE_DELETE_ARRAY(m_Data);
  }
}

/*

 -----------------------------
 File format for version 0x31:

 uint64_t MAGIC_HEADER;
 uint64_t version = 0x00000031;
 uint64_t filesize;
 uint64_t resolveDBSize;

 if(resolveDBSize > 0)
 {
   byte resolveDB[resolveDBSize];
   byte paddingTo16Alignment[];
 }

 byte captureData[]; // remainder of the file

 -----------------------------
 File format for version 0x32:

 uint64_t MAGIC_HEADER;
 uint64_t version = 0x00000032;

 1 or more sections:

 Section
 {
   char isASCII = '\0' or 'A'; // indicates the section is ASCII or binary. ASCII allows for easy
 appending by hand/script
   if(isASCII == 'A')
   {
     // ASCII sections are discouraged for tools, but useful for hand-editing by just
     // appending a simple text file
     char newline = '\n';
     char length[]; // length of just section data below, as decimal string
     char newline = '\n';
     char sectionType[]; // section type, see SectionType enum, as decimal string.
     char newline = '\n';
     char sectionName[]; // UTF-8 string name of section.
     char newline = '\n';
     // sectionName is an arbitrary string. If two sections exist with the
     // exact same name, the last one to occur in the file will be taken, the
     // others ignored.

     byte sectiondata[ interpret(length) ]; // section data
   }
   else if(isASCII == '\0')
   {
     byte zero[3]; // pad out the above character with 0 bytes. Reserved for future use
     uint32_t sectionFlags; // section flags - e.g. is compressed or not.
     uint32_t sectionType; // section type enum, see SectionType. Could be eSectionType_Unknown
     uint32_t sectionLength; // byte length of the actual section data
     uint32_t sectionNameLength; // byte length of the string below (minimum 1, for null terminator)
     char sectionName[sectionNameLength]; // UTF-8 string name of section, optional.

     byte sectiondata[length]; // actual contents of the section

     // note: compressed sections will contain the uncompressed length as a uint64_t
     // before the compressed data.
   }
 };

 // remainder of the file is tightly packed/unaligned section structures.
 // The first section must always be the actual frame capture data in
 // binary form
 Section sections[];

*/

struct FileHeader
{
  FileHeader()
  {
    magic = Serialiser::MAGIC_HEADER;
    version = Serialiser::SERIALISE_VERSION;
  }

  uint64_t magic;
  uint64_t version;
};

struct BinarySectionHeader
{
  byte isASCII;                             // 0x0
  byte zero[3];                             // 0x0, 0x0, 0x0
  Serialiser::SectionFlags sectionFlags;    // section flags - e.g. is compressed or not.
  Serialiser::SectionType
      sectionType;           // section type enum, see SectionType. Could be eSectionType_Unknown
  uint32_t sectionLength;    // byte length of the actual section data
  uint32_t sectionNameLength;    // byte length of the string below (could be 0)
  char name[1];                  // actually sectionNameLength, but at least 1 for null terminator

  // char name[sectionNameLength];
  // byte data[sectionLength];
};

#define RETURNCORRUPT(...)           \
  {                                  \
    RDCERR(__VA_ARGS__);             \
    m_ErrorCode = eSerError_Corrupt; \
    m_HasError = true;               \
    return;                          \
  }

Serialiser::Serialiser(size_t length, const byte *memoryBuf, bool fileheader)
    : m_pCallstack(NULL), m_pResolver(NULL), m_Buffer(NULL)
{
  m_ResolverThread = 0;

  // ensure binary sizes of enums
  RDCCOMPILE_ASSERT(sizeof(SectionFlags) == sizeof(uint32_t), "Section flags not in uint32");
  RDCCOMPILE_ASSERT(sizeof(SectionType) == sizeof(uint32_t), "Section type not in uint32");

  RDCCOMPILE_ASSERT(offsetof(BinarySectionHeader, name) == sizeof(uint32_t) * 5,
                    "BinarySectionHeader size has changed or contains padding");

  Reset();

  m_Mode = READING;
  m_DebugEnabled = false;

  m_FileSize = 0;

  if(!fileheader)
  {
    m_BufferSize = length;
    m_CurrentBufferSize = (size_t)m_BufferSize;
    m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);

    m_SerVer = SERIALISE_VERSION;

    memcpy(m_Buffer, memoryBuf, m_CurrentBufferSize);
    return;
  }

  FileHeader *header = (FileHeader *)memoryBuf;

  if(length < sizeof(FileHeader))
  {
    RDCERR("Can't read from in-memory buffer, truncated header");
    m_ErrorCode = eSerError_Corrupt;
    m_HasError = true;
    return;
  }

  if(header->magic != MAGIC_HEADER)
  {
    char magicRef[5] = {0};
    char magicFile[5] = {0};
    memcpy(magicRef, &MAGIC_HEADER, sizeof(uint32_t));
    memcpy(magicFile, &header->magic, sizeof(uint32_t));
    RDCWARN("Invalid in-memory buffer. Expected magic %s, got %s", magicRef, magicFile);

    m_ErrorCode = eSerError_Corrupt;
    m_HasError = true;
    return;
  }

  const byte *memoryBufEnd = memoryBuf + length;

  m_SerVer = header->version;

  if(header->version == 0x00000031)    // backwards compatibility
  {
    memoryBuf += sizeof(FileHeader);

    if(length < sizeof(FileHeader) + sizeof(uint64_t) * 2)
    {
      RDCERR("Can't read from in-memory buffer, truncated header");
      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    uint64_t *fileSize = (uint64_t *)memoryBuf;
    memoryBuf += sizeof(uint64_t);

    uint64_t *resolveDBSize = (uint64_t *)memoryBuf;
    memoryBuf += sizeof(uint64_t);

    if(*fileSize < length)
    {
      RDCERR("Overlong in-memory buffer. Expected length 0x016llx, got 0x016llx", *fileSize, length);

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    // for in-memory case we don't need to load up the resolve db

    Section *frameCap = new Section();
    frameCap->type = eSectionType_FrameCapture;
    frameCap->flags = eSectionFlag_None;
    frameCap->fileoffset = 0;    // irrelevant
    frameCap->name = "renderdoc/internal/framecapture";
    frameCap->size = uint64_t(memoryBufEnd - memoryBuf);

    memoryBuf = AlignUpPtr(memoryBuf + *resolveDBSize, 16);

    m_Sections.push_back(frameCap);
    m_KnownSections[eSectionType_FrameCapture] = frameCap;
  }
  else if(header->version == SERIALISE_VERSION)
  {
    memoryBuf += sizeof(FileHeader);

    // when loading in-memory we only care about the first section, which should be binary
    const BinarySectionHeader *sectionHeader = (const BinarySectionHeader *)memoryBuf;

    // verify validity
    if(memoryBuf + offsetof(BinarySectionHeader, name) >= memoryBufEnd)
    {
      RDCERR("Truncated binary section header");

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    if(sectionHeader->isASCII != 0 || sectionHeader->zero[0] != 0 || sectionHeader->zero[1] != 0 ||
       sectionHeader->zero[2] != 0)
    {
      RDCERR("Unexpected non-binary section first in capture when loading in-memory");

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    if(sectionHeader->sectionType != eSectionType_FrameCapture)
    {
      RDCERR("Expected first section to be frame capture, got type %x", sectionHeader->sectionType);

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    memoryBuf += offsetof(BinarySectionHeader, name);
    memoryBuf += sectionHeader->sectionNameLength;    // skip name

    if(memoryBuf >= memoryBufEnd)
    {
      RDCERR("Truncated binary section header");

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      return;
    }

    Section *frameCap = new Section();
    frameCap->fileoffset = 0;    // irrelevant
    frameCap->data.assign(memoryBuf, memoryBufEnd);
    frameCap->name = sectionHeader->name;
    frameCap->type = sectionHeader->sectionType;
    frameCap->flags = sectionHeader->sectionFlags;

    uint64_t *uncompLength = (uint64_t *)memoryBuf;

    memoryBuf += sizeof(uint64_t);

    if(memoryBuf >= memoryBufEnd)
    {
      RDCERR("Truncated binary section header");

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;

      SAFE_DELETE(frameCap);
      return;
    }

    frameCap->size = *uncompLength;

    m_KnownSections[eSectionType_FrameCapture] = frameCap;
    m_Sections.push_back(frameCap);
  }
  else
  {
    RDCERR(
        "Capture file from wrong version. This program is on logfile version %llu, file is logfile "
        "version %llu",
        SERIALISE_VERSION, header->version);

    m_ErrorCode = eSerError_UnsupportedVersion;
    m_HasError = true;
    return;
  }

  m_BufferSize = m_KnownSections[eSectionType_FrameCapture]->size;
  m_CurrentBufferSize = (size_t)m_BufferSize;
  m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);

  if(m_KnownSections[eSectionType_FrameCapture]->flags & eSectionFlag_LZ4Compressed)
  {
    CompressedFileIO::Decompress(m_Buffer, memoryBuf, memoryBufEnd - memoryBuf);
  }
  else
  {
    memcpy(m_Buffer, memoryBuf, m_CurrentBufferSize);
  }
}

Serialiser::Serialiser(const char *path, Mode mode, bool debugMode, uint64_t sizeHint)
    : m_pCallstack(NULL), m_pResolver(NULL), m_Buffer(NULL)
{
  m_ResolverThread = 0;

  Reset();

  m_Filename = path ? path : "";

  m_Mode = mode;
  m_DebugEnabled = debugMode;

  m_FileSize = 0;

  FileHeader header;

  if(mode == READING)
  {
    m_ReadFileHandle = FileIO::fopen(m_Filename.c_str(), "rb");

    if(!m_ReadFileHandle)
    {
      RDCERR("Can't open capture file '%s' for read - errno %d", m_Filename.c_str(), errno);
      m_ErrorCode = eSerError_FileIO;
      m_HasError = true;
      return;
    }

    FileIO::fseek64(m_ReadFileHandle, 0, SEEK_END);

    m_FileSize = FileIO::ftell64(m_ReadFileHandle);

    FileIO::fseek64(m_ReadFileHandle, 0, SEEK_SET);

    RDCDEBUG("Opened capture file for read");

    FileIO::fread(&header, 1, sizeof(FileHeader), m_ReadFileHandle);

    if(header.magic != MAGIC_HEADER)
    {
      RDCWARN("Invalid capture file. Expected magic %08x, got %08x", MAGIC_HEADER,
              (uint32_t)header.magic);

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      FileIO::fclose(m_ReadFileHandle);
      m_ReadFileHandle = 0;
      return;
    }

    m_SerVer = header.version;

    if(header.version == 0x00000031)    // backwards compatibility
    {
      uint64_t headerRemainder[2];

      FileIO::fread(&headerRemainder, 1, sizeof(headerRemainder), m_ReadFileHandle);

      FileIO::fseek64(m_ReadFileHandle, 0, SEEK_END);

      uint64_t realLength = FileIO::ftell64(m_ReadFileHandle);
      if(headerRemainder[0] != realLength)
      {
        RDCERR("Truncated/overlong capture file. Expected length 0x016llx, got 0x016llx",
               headerRemainder[0], realLength);

        m_ErrorCode = eSerError_Corrupt;
        m_HasError = true;
        FileIO::fclose(m_ReadFileHandle);
        m_ReadFileHandle = 0;
        return;
      }

      FileIO::fseek64(m_ReadFileHandle, sizeof(FileHeader) + sizeof(headerRemainder), SEEK_SET);

      size_t resolveDBSize = (size_t)headerRemainder[1];

      if(resolveDBSize > 0)
      {
        Section *resolveDB = new Section();
        resolveDB->type = eSectionType_ResolveDatabase;
        resolveDB->flags = eSectionFlag_None;
        resolveDB->fileoffset = sizeof(FileHeader) + sizeof(headerRemainder);
        resolveDB->name = "renderdoc/internal/resolvedb";
        resolveDB->data.resize(resolveDBSize);

        // read resolve DB entirely into memory
        FileIO::fread(&resolveDB->data[0], 1, resolveDBSize, m_ReadFileHandle);

        m_Sections.push_back(resolveDB);
        m_KnownSections[eSectionType_ResolveDatabase] = resolveDB;
      }

      // seek to frame capture data
      FileIO::fseek64(m_ReadFileHandle,
                      AlignUp16(sizeof(FileHeader) + sizeof(headerRemainder) + resolveDBSize),
                      SEEK_SET);

      Section *frameCap = new Section();
      frameCap->type = eSectionType_FrameCapture;
      frameCap->flags = eSectionFlag_None;
      frameCap->fileoffset = FileIO::ftell64(m_ReadFileHandle);
      frameCap->name = "renderdoc/internal/framecapture";
      frameCap->size = realLength - frameCap->fileoffset;

      m_Sections.push_back(frameCap);
      m_KnownSections[eSectionType_FrameCapture] = frameCap;
    }
    else if(header.version == SERIALISE_VERSION)
    {
      while(!FileIO::feof(m_ReadFileHandle))
      {
        BinarySectionHeader sectionHeader = {0};
        byte *reading = (byte *)&sectionHeader;

        FileIO::fread(reading, 1, 1, m_ReadFileHandle);
        reading++;

        if(FileIO::feof(m_ReadFileHandle))
          break;

        if(sectionHeader.isASCII == 'A')
        {
          // ASCII section
          char c = 0;
          FileIO::fread(&c, 1, 1, m_ReadFileHandle);
          if(c != '\n')
            RETURNCORRUPT("Invalid ASCII data section '%hhx'", c);

          if(FileIO::feof(m_ReadFileHandle))
            RETURNCORRUPT("Invalid truncated ASCII data section");

          uint64_t length = 0;

          c = '0';

          while(!FileIO::feof(m_ReadFileHandle) && c != '\n')
          {
            c = '0';
            FileIO::fread(&c, 1, 1, m_ReadFileHandle);

            if(c == '\n')
              break;

            length *= 10;
            length += int(c - '0');
          }

          if(FileIO::feof(m_ReadFileHandle))
            RETURNCORRUPT("Invalid truncated ASCII data section");

          union
          {
            uint32_t u32;
            SectionType t;
          } type;

          type.u32 = 0;

          c = '0';

          while(!FileIO::feof(m_ReadFileHandle) && c != '\n')
          {
            c = '0';
            FileIO::fread(&c, 1, 1, m_ReadFileHandle);

            if(c == '\n')
              break;

            type.u32 *= 10;
            type.u32 += int(c - '0');
          }

          if(FileIO::feof(m_ReadFileHandle))
            RETURNCORRUPT("Invalid truncated ASCII data section");

          string name;

          c = 0;

          while(!FileIO::feof(m_ReadFileHandle) && c != '\n')
          {
            c = 0;
            FileIO::fread(&c, 1, 1, m_ReadFileHandle);

            if(c == 0 || c == '\n')
              break;

            name.push_back(c);
          }

          if(FileIO::feof(m_ReadFileHandle))
            RETURNCORRUPT("Invalid truncated ASCII data section");

          Section *sect = new Section();
          sect->flags = eSectionFlag_ASCIIStored;
          sect->type = type.t;
          sect->name = name;
          sect->size = length;
          sect->data.resize((size_t)length);
          sect->fileoffset = FileIO::ftell64(m_ReadFileHandle);

          FileIO::fread(&sect->data[0], 1, (size_t)length, m_ReadFileHandle);

          if(sect->type != eSectionType_Unknown && sect->type < eSectionType_Num)
            m_KnownSections[sect->type] = sect;
          m_Sections.push_back(sect);
        }
        else if(sectionHeader.isASCII == 0x0)
        {
          FileIO::fread(reading, 1, offsetof(BinarySectionHeader, name) - 1, m_ReadFileHandle);

          Section *sect = new Section();
          sect->flags = sectionHeader.sectionFlags;
          sect->type = sectionHeader.sectionType;
          sect->name.resize(sectionHeader.sectionNameLength - 1);
          sect->size = sectionHeader.sectionLength;

          FileIO::fread(&sect->name[0], 1, sectionHeader.sectionNameLength - 1, m_ReadFileHandle);
          char nullterm = 0;
          FileIO::fread(&nullterm, 1, 1, m_ReadFileHandle);

          sect->fileoffset = FileIO::ftell64(m_ReadFileHandle);

          if(sect->flags & eSectionFlag_LZ4Compressed)
          {
            sect->compressedReader = new CompressedFileIO(m_ReadFileHandle);
            FileIO::fread(&sect->size, 1, sizeof(uint64_t), m_ReadFileHandle);

            sect->fileoffset += sizeof(uint64_t);
          }

          if(sect->type != eSectionType_Unknown && sect->type < eSectionType_Num)
            m_KnownSections[sect->type] = sect;
          m_Sections.push_back(sect);

          // if section isn't frame capture data and is small enough, read it all into memory now,
          // otherwise skip
          if(sect->type != eSectionType_FrameCapture && sectionHeader.sectionLength < 4 * 1024 * 1024)
          {
            sect->data.resize(sectionHeader.sectionLength);
            FileIO::fread(&sect->data[0], 1, sectionHeader.sectionLength, m_ReadFileHandle);
          }
          else
          {
            if(sectionHeader.sectionLength == 0xffffffff)
            {
              RDCWARN(
                  "Section length 0xFFFFFFFF - assuming truncated value! Seeking to end of file, "
                  "discarding any remaining sections.");
              FileIO::fseek64(m_ReadFileHandle, 0, SEEK_END);
            }
            else
            {
              FileIO::fseek64(m_ReadFileHandle, sectionHeader.sectionLength, SEEK_CUR);
            }
          }
        }
        else
        {
          RETURNCORRUPT("Unrecognised section type '%hhx'", sectionHeader.isASCII);
        }
      }
    }
    else
    {
      RDCERR(
          "Capture file from wrong version. This program is logfile version %llu, file is logfile "
          "version %llu",
          SERIALISE_VERSION, header.version);

      m_ErrorCode = eSerError_UnsupportedVersion;
      m_HasError = true;
      FileIO::fclose(m_ReadFileHandle);
      m_ReadFileHandle = 0;
      return;
    }

    if(m_KnownSections[eSectionType_FrameCapture] == NULL)
    {
      RDCERR("Capture file doesn't have a frame capture");

      m_ErrorCode = eSerError_Corrupt;
      m_HasError = true;
      FileIO::fclose(m_ReadFileHandle);
      m_ReadFileHandle = 0;
      return;
    }

    m_BufferSize = m_KnownSections[eSectionType_FrameCapture]->size;
    m_CurrentBufferSize = (size_t)RDCMIN(m_BufferSize, (uint64_t)64 * 1024);
    m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
    m_ReadOffset = 0;

    FileIO::fseek64(m_ReadFileHandle, m_KnownSections[eSectionType_FrameCapture]->fileoffset,
                    SEEK_SET);

    // read initial buffer of data
    ReadFromFile(0, m_CurrentBufferSize);
  }
  else
  {
    m_SerVer = SERIALISE_VERSION;

    if(m_Filename != "")
    {
      m_BufferSize = 0;
      m_BufferHead = m_Buffer = NULL;
    }
    else
    {
      m_BufferSize = sizeHint;
      m_BufferHead = m_Buffer = AllocAlignedBuffer((size_t)m_BufferSize);
    }
  }
}

void Serialiser::Reset()
{
  if(m_ResolverThread != 0)
  {
    m_ResolverThreadKillSignal = true;

    Threading::JoinThread(m_ResolverThread);
    Threading::CloseThread(m_ResolverThread);
    m_ResolverThread = 0;
  }

  m_pUserData = NULL;

  m_DebugText = "";
  m_DebugTextWriting = false;

  RDCEraseEl(m_KnownSections);

  m_HasError = false;
  m_ErrorCode = eSerError_None;

  m_Mode = NONE;

  m_Indent = 0;

  SAFE_DELETE(m_pCallstack);
  SAFE_DELETE(m_pResolver);
  if(m_Buffer)
  {
    FreeAlignedBuffer(m_Buffer);
    m_Buffer = NULL;
  }

  m_ChunkLookup = NULL;

  m_AlignedData = false;

  m_ReadFileHandle = NULL;

  m_ReadOffset = 0;

  m_BufferHead = m_Buffer = NULL;
  m_CurrentBufferSize = 0;
  m_BufferSize = 0;
}

Serialiser::~Serialiser()
{
  if(m_ResolverThread != 0)
  {
    m_ResolverThreadKillSignal = true;
    Threading::JoinThread(m_ResolverThread);
    Threading::CloseThread(m_ResolverThread);
    m_ResolverThread = 0;
  }

  if(m_ReadFileHandle)
  {
    FileIO::fclose(m_ReadFileHandle);
    m_ReadFileHandle = 0;
  }

  for(size_t i = 0; i < m_Sections.size(); i++)
  {
    SAFE_DELETE(m_Sections[i]->compressedReader);
    SAFE_DELETE(m_Sections[i]);
  }

  for(size_t i = 0; i < m_Chunks.size(); i++)
  {
    if(m_Chunks[i]->IsTemporary())
      SAFE_DELETE(m_Chunks[i]);
  }

  m_Chunks.clear();

  SAFE_DELETE(m_pResolver);
  SAFE_DELETE(m_pCallstack);
  if(m_Buffer)
  {
    FreeAlignedBuffer(m_Buffer);
    m_Buffer = NULL;
  }
  m_Buffer = NULL;
  m_BufferHead = NULL;
}

void Serialiser::WriteBytes(const byte *buf, size_t nBytes)
{
  if(m_HasError)
  {
    RDCERR("Writing bytes with error state serialiser");
    return;
  }

  if(m_Buffer + m_BufferSize < m_BufferHead + nBytes + 8)
  {
    // reallocate
    while(m_Buffer + m_BufferSize < m_BufferHead + nBytes + 8)
    {
      m_BufferSize += 128 * 1024;
    }

    byte *newBuf = AllocAlignedBuffer((size_t)m_BufferSize);

    size_t curUsed = m_BufferHead - m_Buffer;

    memcpy(newBuf, m_Buffer, curUsed);

    FreeAlignedBuffer(m_Buffer);

    m_Buffer = newBuf;
    m_BufferHead = newBuf + curUsed;
  }

  memcpy(m_BufferHead, buf, nBytes);

  m_BufferHead += nBytes;
}

void *Serialiser::ReadBytes(size_t nBytes)
{
  if(m_HasError)
  {
    RDCERR("Reading bytes with error state serialiser");
    return NULL;
  }

  // if we would read off the end of our current window
  if(m_BufferHead + nBytes > m_Buffer + m_CurrentBufferSize)
  {
    // store old buffer and the read data, so we can move it into the new buffer
    byte *oldBuffer = m_Buffer;

    // always keep at least a certain window behind what we read.
    size_t backwardsWindow = RDCMIN((size_t)64, size_t(m_BufferHead - m_Buffer));

    byte *currentData = m_BufferHead - backwardsWindow;
    size_t currentDataSize = m_CurrentBufferSize - (m_BufferHead - m_Buffer) + backwardsWindow;

    size_t BufferOffset = m_BufferHead - m_Buffer;

    // if we are reading more than our current buffer size, expand the buffer size
    if(nBytes + backwardsWindow > m_CurrentBufferSize)
    {
      // very conservative resizing - don't do "double and add" - to avoid
      // a 1GB buffer being read and needing to allocate 2GB. The cost is we
      // will reallocate a bit more often
      m_CurrentBufferSize = nBytes + backwardsWindow;
      m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
    }

    // move the unread data into the buffer
    memmove(m_Buffer, currentData, currentDataSize);

    if(BufferOffset > backwardsWindow)
    {
      m_ReadOffset += BufferOffset - backwardsWindow;
      m_BufferHead = m_Buffer + backwardsWindow;
    }
    else
    {
      m_BufferHead = m_Buffer + BufferOffset;
    }

    // if there's anything left of the file to read in, do so now
    ReadFromFile(currentDataSize, RDCMIN(m_CurrentBufferSize - currentDataSize,
                                         size_t(m_BufferSize - m_ReadOffset - currentDataSize)));

    if(oldBuffer != m_Buffer)
      FreeAlignedBuffer(oldBuffer);
  }

  void *ret = m_BufferHead;

  m_BufferHead += nBytes;

  RDCASSERT(m_BufferHead <= m_Buffer + m_CurrentBufferSize);

  return ret;
}

void Serialiser::ReadFromFile(uint64_t bufferOffs, size_t length)
{
  RDCASSERT(m_ReadFileHandle);

  if(m_ReadFileHandle == NULL)
    return;

  Section *s = m_KnownSections[eSectionType_FrameCapture];

  RDCASSERT(s);

  if(s->flags & eSectionFlag_LZ4Compressed)
  {
    RDCASSERT(s->compressedReader);
    s->compressedReader->Read(m_Buffer + bufferOffs, length);
  }
  else
  {
    FileIO::fread(m_Buffer + bufferOffs, 1, length, m_ReadFileHandle);
  }
}

byte *Serialiser::AllocAlignedBuffer(size_t size, size_t alignment)
{
  byte *rawAlloc = NULL;

#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try
#endif
  {
    rawAlloc = new byte[size + sizeof(byte *) + alignment];
  }
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  catch(std::bad_alloc &)
  {
    rawAlloc = NULL;
  }
#endif

  if(rawAlloc == NULL)
    RDCFATAL("Allocation for %llu bytes failed", (uint64_t)size);

  RDCASSERT(rawAlloc);

  byte *alignedAlloc = (byte *)AlignUp((size_t)(rawAlloc + sizeof(byte *)), alignment);

  byte **realPointer = (byte **)alignedAlloc;
  realPointer[-1] = rawAlloc;

  return alignedAlloc;
}

void Serialiser::FreeAlignedBuffer(byte *buf)
{
  if(buf == NULL)
    return;

  byte **realPointer = (byte **)buf;
  byte *rawAlloc = realPointer[-1];

  delete[] rawAlloc;
}

void Serialiser::SetPersistentBlock(uint64_t offs)
{
  // as long as this is called immediately after pushing the chunk context at the
  // offset, we will always have the start in memory, as we keep 64 bytes of
  // a backwards window even if we had to shift the currently in-memory bytes
  // while reading the chunk header
  RDCASSERT(m_ReadOffset <= offs);

  // also can't persistent block ahead of where we are
  RDCASSERT(offs < (m_BufferHead - m_Buffer) + m_ReadOffset);

  // ensure sane offset
  RDCASSERT(offs < m_BufferSize);

  size_t persistentSize = (size_t)(m_BufferSize - offs);

  // allocate our persistent buffer
  byte *newBuf = AllocAlignedBuffer(persistentSize);

  // save where buffer head was as file-offset
  uint64_t prevOffs = uint64_t(m_BufferHead - m_Buffer) + m_ReadOffset;

  // find the range of the persistent block that we have in memory
  byte *persistentBase = m_Buffer + (offs - m_ReadOffset);
  size_t persistentInMemory =
      RDCMIN(persistentSize, size_t(m_CurrentBufferSize - (offs - m_ReadOffset)));

  memcpy(newBuf, persistentBase, persistentInMemory);

  FreeAlignedBuffer(m_Buffer);

  m_CurrentBufferSize = persistentSize;
  m_Buffer = newBuf;
  m_ReadOffset = offs;

  // set the head back to where it was
  m_BufferHead = m_Buffer + (prevOffs - offs);

  // if we didn't read everything, read the rest
  if(persistentInMemory < persistentSize)
  {
    ReadFromFile(persistentInMemory, persistentSize - persistentInMemory);
  }

  RDCASSERT(m_ReadFileHandle);

  // close the file handle
  FileIO::fclose(m_ReadFileHandle);
  m_ReadFileHandle = 0;
}

void Serialiser::SetOffset(uint64_t offs)
{
  if(m_HasError)
  {
    RDCERR("Setting offset with error state serialiser");
    return;
  }

  // if we're jumping back before our in-memory window just reset the window
  // and load it all in from scratch.
  if(m_Mode == READING && offs < m_ReadOffset)
  {
    // if we're reading from file, only support rewinding all the way to the start
    RDCASSERT(m_ReadFileHandle == NULL || offs == 0);

    if(m_ReadFileHandle)
    {
      Section *s = m_KnownSections[eSectionType_FrameCapture];
      RDCASSERT(s);
      FileIO::fseek64(m_ReadFileHandle, s->fileoffset, SEEK_SET);

      if(s->flags & eSectionFlag_LZ4Compressed)
      {
        RDCASSERT(s->compressedReader);
        s->compressedReader->Reset();
      }
    }

    FreeAlignedBuffer(m_Buffer);

    m_CurrentBufferSize = (size_t)RDCMIN(m_BufferSize, (uint64_t)64 * 1024);
    m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
    m_ReadOffset = offs;

    ReadFromFile(0, m_CurrentBufferSize);
  }

  RDCASSERT(m_BufferHead && m_Buffer && offs <= GetSize());
  m_BufferHead = m_Buffer + offs - m_ReadOffset;
  m_Indent = 0;
}

void Serialiser::InitCallstackResolver()
{
  if(m_pResolver == NULL && m_ResolverThread == 0 &&
     m_KnownSections[eSectionType_ResolveDatabase] != NULL)
  {
    m_ResolverThreadKillSignal = false;
    m_ResolverThread = Threading::CreateThread(&Serialiser::CreateResolver, (void *)this);
  }
}

void Serialiser::SetCallstack(uint64_t *levels, size_t numLevels)
{
  if(m_pCallstack == NULL)
    m_pCallstack = Callstack::Create();

  m_pCallstack->Set(levels, numLevels);
}

void Serialiser::CreateResolver(void *ths)
{
  Serialiser *ser = (Serialiser *)ths;

  string dir = dirname(ser->m_Filename);

  Section *s = ser->m_KnownSections[Serialiser::eSectionType_ResolveDatabase];
  RDCASSERT(s);

  ser->m_pResolver = Callstack::MakeResolver((char *)&s->data[0], s->data.size(), dir,
                                             &ser->m_ResolverThreadKillSignal);
}

void Serialiser::FlushToDisk()
{
  SCOPED_TIMER("File writing");

  if(m_Filename != "" && !m_HasError && m_Mode == WRITING)
  {
    RDCDEBUG("writing capture files");

    if(m_DebugEnabled && !m_DebugText.empty())
    {
      FILE *dbgFile = FileIO::fopen((m_Filename + ".txt").c_str(), "wb");

      if(!dbgFile)
      {
        RDCERR("Can't open debug capture file '%s'", (m_Filename + ".txt").c_str());
      }
      else
      {
        const char *str = m_DebugText.c_str();
        size_t len = m_DebugText.length();
        const size_t chunkSize = 10 * 1024 * 1024;
        while(len > 0)
        {
          size_t writeSize = RDCMIN(len, chunkSize);
          size_t written = FileIO::fwrite(str, 1, writeSize, dbgFile);

          RDCASSERT(written == writeSize);

          str += writeSize;
          len -= writeSize;
        }

        FileIO::fclose(dbgFile);
      }
    }

    FILE *binFile = FileIO::fopen(m_Filename.c_str(), "w+b");

    if(!binFile)
    {
      RDCERR("Can't open capture file '%s' for write, errno %d", m_Filename.c_str(), errno);
      m_ErrorCode = eSerError_FileIO;
      m_HasError = true;
      return;
    }

    RDCDEBUG("Opened capture file for write");

    FileHeader header;    // automagically initialised with correct data

    // write header
    FileIO::fwrite(&header, 1, sizeof(FileHeader), binFile);

    static const byte padding[BufferAlignment] = {0};

    uint64_t compressedSizeOffset = 0;
    uint64_t uncompressedSizeOffset = 0;

    // write frame capture section header
    {
      const char sectionName[] = "renderdoc/internal/framecapture";

      BinarySectionHeader section = {0};
      section.isASCII = 0;                                // redundant but explicit
      section.sectionNameLength = sizeof(sectionName);    // includes null terminator
      section.sectionType = eSectionType_FrameCapture;
      section.sectionFlags = eSectionFlag_LZ4Compressed;
      section.sectionLength =
          0;    // will be fixed up later, to avoid having to compress everything into memory

      compressedSizeOffset = FileIO::ftell64(binFile) + offsetof(BinarySectionHeader, sectionLength);

      FileIO::fwrite(&section, 1, offsetof(BinarySectionHeader, name), binFile);
      FileIO::fwrite(sectionName, 1, sizeof(sectionName), binFile);

      uint64_t len = 0;    // will be fixed up later
      uncompressedSizeOffset = FileIO::ftell64(binFile);
      FileIO::fwrite(&len, 1, sizeof(uint64_t), binFile);
    }

    CompressedFileIO fwriter(binFile);

    // track offset so we can add padding. The padding is relative
    // to the start of the decompressed buffer, so we start it from 0
    uint64_t offs = 0;
    uint64_t alignedoffs = 0;

    // write frame capture contents
    for(size_t i = 0; i < m_Chunks.size(); i++)
    {
      Chunk *chunk = m_Chunks[i];

      alignedoffs = AlignUp(offs, BufferAlignment);

      if(offs != alignedoffs && chunk->IsAligned())
      {
        uint16_t chunkIdx = 0;    // write a '0' chunk that indicates special behaviour
        fwriter.Write(&chunkIdx, sizeof(chunkIdx));
        offs += sizeof(chunkIdx);

        uint8_t controlByte = 0;    // control byte 0 indicates padding
        fwriter.Write(&controlByte, sizeof(controlByte));
        offs += sizeof(controlByte);

        offs++;    // we will have to write out a byte indicating how much padding exists, so add 1
        alignedoffs = AlignUp(offs, BufferAlignment);

        RDCCOMPILE_ASSERT(BufferAlignment < 0x100,
                          "Buffer alignment must be less than 256");    // with a byte at most
                                                                        // indicating how many bytes
                                                                        // to pad,
        // this is our maximal representable alignment

        uint8_t padLength = (alignedoffs - offs) & 0xff;
        fwriter.Write(&padLength, sizeof(padLength));

        // we might have padded with the control bytes, so only write some bytes if we need to
        if(padLength > 0)
        {
          fwriter.Write(padding, size_t(alignedoffs - offs));
          offs += alignedoffs - offs;
        }
      }

      fwriter.Write(chunk->GetData(), chunk->GetLength());

      offs += chunk->GetLength();

      if(chunk->IsTemporary())
        SAFE_DELETE(chunk);
    }

    fwriter.Flush();

    m_Chunks.clear();

    // fixup section size
    {
      uint32_t compsize = 0;
      uint64_t uncompsize = 0;

      uint64_t curoffs = FileIO::ftell64(binFile);

      FileIO::fseek64(binFile, compressedSizeOffset, SEEK_SET);

      uint64_t realCompSize = fwriter.GetCompressedSize();

      if(realCompSize > 0xffffffff)
      {
        RDCERR("Compressed file size %llu exceeds representable capture size! May cause corruption",
               realCompSize);
        realCompSize = 0xffffffffULL;
      }

      compsize = uint32_t(realCompSize);
      FileIO::fwrite(&compsize, 1, sizeof(compsize), binFile);

      FileIO::fseek64(binFile, uncompressedSizeOffset, SEEK_SET);

      uncompsize = fwriter.GetUncompressedSize();
      FileIO::fwrite(&uncompsize, 1, sizeof(uncompsize), binFile);

      FileIO::fseek64(binFile, curoffs, SEEK_SET);

      RDCLOG("Compressed frame capture data from %llu to %llu", fwriter.GetUncompressedSize(),
             fwriter.GetCompressedSize());
    }

    char *symbolDB = NULL;
    size_t symbolDBSize = 0;

    if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks ||
       RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws)
    {
      // get symbol database
      Callstack::GetLoadedModules(symbolDB, symbolDBSize);

      symbolDB = new char[symbolDBSize];
      symbolDBSize = 0;

      Callstack::GetLoadedModules(symbolDB, symbolDBSize);
    }

    // write symbol database section
    if(symbolDB)
    {
      const char sectionName[] = "renderdoc/internal/resolvedb";

      BinarySectionHeader section = {0};
      section.isASCII = 0;                                // redundant but explicit
      section.sectionNameLength = sizeof(sectionName);    // includes null terminator
      section.sectionType = eSectionType_ResolveDatabase;
      section.sectionLength = (uint32_t)symbolDBSize;

      FileIO::fwrite(&section, 1, offsetof(BinarySectionHeader, name), binFile);
      FileIO::fwrite(sectionName, 1, sizeof(sectionName), binFile);

      // write actual data
      FileIO::fwrite(symbolDB, 1, symbolDBSize, binFile);

      SAFE_DELETE_ARRAY(symbolDB);
    }

    // write the machine identifier as an ASCII section
    {
      const char sectionName[] = "renderdoc/internal/machineid";

      uint64_t machineID = OSUtility::GetMachineIdent();

      BinarySectionHeader section = {0};
      section.isASCII = 0;                                // redundant but explicit
      section.sectionNameLength = sizeof(sectionName);    // includes null terminator
      section.sectionType = eSectionType_MachineID;
      section.sectionFlags = eSectionFlag_None;
      section.sectionLength = sizeof(machineID);

      FileIO::fwrite(&section, 1, offsetof(BinarySectionHeader, name), binFile);
      FileIO::fwrite(sectionName, 1, sizeof(sectionName), binFile);
      FileIO::fwrite(&machineID, 1, sizeof(machineID), binFile);
    }

    FileIO::fclose(binFile);
  }
}

void Serialiser::DebugPrint(const char *fmt, ...)
{
  if(m_HasError)
  {
    RDCERR("Debug printing with error state serialiser");
    return;
  }

  char tmpBuf[1024];

  va_list args;
  va_start(args, fmt);
  StringFormat::vsnprintf(tmpBuf, 1023, fmt, args);
  tmpBuf[1023] = '\0';
  va_end(args);

  m_DebugText += GetIndent();
  m_DebugText += tmpBuf;
}

uint32_t Serialiser::PushContext(const char *name, const char *typeName, uint32_t chunkIdx,
                                 bool smallChunk)
{
  // if writing, and chunkidx isn't 0 (debug non-scope), then either we're nested
  // or we should be writing into the start of the serialiser. A serialiser should
  // only ever have one chunk in it
  RDCASSERT(m_Mode < WRITING || m_Indent > 0 || GetOffset() == 0 || chunkIdx == 0);

  // we should not be pushing contexts directly into a file serialiser
  RDCASSERT(m_Mode < WRITING || m_Filename.empty());

  if(m_Mode >= WRITING)
  {
    if(chunkIdx > 0)
    {
      uint16_t c = chunkIdx & 0x3fff;
      RDCASSERT(chunkIdx <= 0x3fff);

      /////////////////

      Callstack::Stackwalk *call = NULL;

      if(m_Indent == 0)
      {
        if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks &&
           !RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws)
        {
          call = Callstack::Collect();

          RDCASSERT(call->NumLevels() < 0xff);
        }
      }

      if(call)
        c |= 0x8000;
      if(smallChunk)
        c |= 0x4000;

      WriteFrom(c);

      if(call)
      {
        uint8_t numLevels = call->NumLevels() & 0xff;
        WriteFrom(numLevels);

        if(call->NumLevels())
        {
          WriteBytes((byte *)call->GetAddrs(), sizeof(uint64_t) * numLevels);
        }

        SAFE_DELETE(call);
      }

      // will be fixed up in PopContext
      if(smallChunk)
      {
        uint16_t chunkSize = 0xbeeb;
        m_ChunkFixups.push_back(0x8000000000000000ULL | GetOffset());
        WriteFrom(chunkSize);
      }
      else
      {
        uint32_t chunkSize = 0xbeebfeed;
        m_ChunkFixups.push_back(GetOffset() & ~0x8000000000000000ULL);
        WriteFrom(chunkSize);
      }
    }

    if(m_DebugTextWriting)
    {
      if(typeName)
        DebugPrint("%s = %s (%d)\n", name, typeName, chunkIdx);
      else
        DebugPrint("%s (%d)\n", name, chunkIdx);
      DebugPrint("{\n");
    }
  }
  else
  {
    if(m_Indent == 0)
    {
      // reset debug text
      m_DebugText = "";
    }

    if(chunkIdx > 0)
    {
      uint16_t c = 0;
      ReadInto(c);

      // chunk index 0 is not allowed in normal situations.
      // allows us to indicate some control bytes
      while(c == 0)
      {
        uint8_t *controlByte = (uint8_t *)ReadBytes(1);

        if(*controlByte == 0x0)
        {
          // padding
          uint8_t *padLength = (uint8_t *)ReadBytes(1);

          // might have padded with these 5 control bytes,
          // so a pad length of 0 IS VALID.
          if(*padLength > 0)
          {
            ReadBytes((size_t)*padLength);
          }
        }
        else
        {
          RDCERR("Unexpected control byte: %x", (uint32_t)*controlByte);
        }

        ReadInto(c);
      }

      chunkIdx = c & 0x3fff;
      bool callstack = (c & 0x8000) > 0;
      bool smallchunk = (c & 0x4000) > 0;

      /////////////////

      if(m_Indent == 0)
      {
        if(callstack)
        {
          uint8_t callLen = 0;
          ReadInto(callLen);

          uint64_t *calls = (uint64_t *)ReadBytes(callLen * sizeof(uint64_t));
          SetCallstack(calls, callLen);
        }
        else
        {
          SetCallstack(NULL, 0);
        }
      }

      /////////////////

      if(smallchunk)
      {
        uint16_t miniSize = 0xbeeb;
        ReadInto(miniSize);

        m_LastChunkLen = miniSize;
      }
      else
      {
        uint32_t chunkSize = 0xbeebfeed;
        ReadInto(chunkSize);

        m_LastChunkLen = chunkSize;
      }
    }

    if(!name && m_ChunkLookup)
      name = m_ChunkLookup(chunkIdx);

    if(m_DebugTextWriting)
    {
      if(typeName)
        DebugPrint("%s = %s\n", name ? name : "Unknown", typeName);
      else
        DebugPrint("%s\n", name ? name : "Unknown");
      DebugPrint("{\n");
    }
  }

  m_Indent++;

  return chunkIdx;
}

void Serialiser::PopContext(uint32_t chunkIdx)
{
  m_Indent = RDCMAX(m_Indent - 1, 0);

  if(m_Mode >= WRITING)
  {
    if(chunkIdx > 0 && m_Mode == WRITING)
    {
      // fix up the latest PushContext (guaranteed to match this one as Pushes and Pops match)
      RDCASSERT(!m_ChunkFixups.empty());

      uint64_t chunkOffset = m_ChunkFixups.back();
      m_ChunkFixups.pop_back();

      bool smallchunk = (chunkOffset & 0x8000000000000000ULL) > 0;
      chunkOffset &= ~0x8000000000000000ULL;

      uint64_t curOffset = GetOffset();

      RDCASSERT(curOffset > chunkOffset);

      uint64_t chunkLength =
          (curOffset - chunkOffset) - (smallchunk ? sizeof(uint16_t) : sizeof(uint32_t));

      RDCASSERT(chunkLength < 0xffffffff);

      uint32_t chunklen = (uint32_t)chunkLength;

      byte *head = m_BufferHead;
      SetOffset(chunkOffset);
      if(smallchunk)
      {
        uint16_t miniSize = (chunklen & 0xffff);
        RDCASSERT(chunklen <= 0xffff);
        WriteFrom(miniSize);
      }
      else
      {
        WriteFrom(chunklen);
      }
      m_BufferHead = head;
    }

    if(m_DebugTextWriting)
      DebugPrint("}\n");
  }
  else
  {
    if(m_DebugTextWriting)
      DebugPrint("}\n");
  }
}

/////////////////////////////////////////////////////////////
// Serialise functions

/////////////////////////////////////////////////////////////
// generic

void Serialiser::SerialiseString(const char *name, string &el)
{
  uint32_t len = (uint32_t)el.length();

  Serialise(NULL, len);

  if(m_Mode == READING)
    el.resize(len);

  if(m_Mode >= WRITING)
  {
    WriteBytes((byte *)el.c_str(), len);

    if(m_DebugTextWriting)
    {
      string s = el;
      if(s.length() > 64)
        s = s.substr(0, 60) + "...";
      DebugPrint("%s: \"%s\"\n", name, s.c_str());
    }
  }
  else
  {
    memcpy(&el[0], ReadBytes(len), len);

    if(m_DebugTextWriting)
    {
      string s = el;
      if(s.length() > 64)
        s = s.substr(0, 60) + "...";
      DebugPrint("%s: \"%s\"\n", name, s.c_str());
    }
  }
}

void Serialiser::Insert(Chunk *chunk)
{
  m_Chunks.push_back(chunk);

  m_DebugText += chunk->GetDebugString();
}

void Serialiser::AlignNextBuffer(const size_t alignment)
{
  // on new logs, we don't have to align. This code will be deleted once backwards-compat is dropped
  if(m_Mode >= WRITING || m_SerVer >= 0x00000032)
    return;

  // this is a super hack but it's the easiest way to align a buffer to a larger pow2 alignment
  // than the default 16-bytes, while still able to be backwards compatible with old logs that
  // weren't so aligned. We know that SerialiseBuffer will align to the nearest 16-byte boundary
  // after serialising 4 bytes of length, so we pad up to exactly 4 bytes before the desired
  // alignment, then after the 4 byte length there's nothing for the other padding to do.
  //
  // Note the chunk still needs to be aligned when the memory is allocated - this just ensures
  // the offset from the start is also aligned

  uint32_t len = 0;

  if(m_Mode >= WRITING)
  {
    // add sizeof(uint32_t) since we'll be serialising out how much padding is here,
    // then another sizeof(uint32_t) so we're aligning the offset after the buffer's
    // serialised length
    uint64_t curoffs = GetOffset() + sizeof(uint32_t) * 2;
    uint64_t alignedoffs = AlignUp(curoffs, (uint64_t)alignment);

    len = uint32_t(alignedoffs - curoffs);
  }

  // avoid dynamically allocating
  RDCASSERT(alignment <= 128);
  byte padding[128] = {0};

  if(m_Mode >= WRITING)
  {
    WriteFrom(len);
    WriteBytes(&padding[0], (size_t)len);
  }
  else
  {
    ReadInto(len);
    ReadBytes(len);
  }
}

void Serialiser::SerialiseBuffer(const char *name, byte *&buf, size_t &len)
{
  uint32_t bufLen = (uint32_t)len;

  if(m_Mode >= WRITING)
  {
    WriteFrom(bufLen);

    // ensure byte alignment
    uint64_t offs = GetOffset();
    uint64_t alignedoffs = AlignUp(offs, BufferAlignment);

    if(offs != alignedoffs)
    {
      static const byte padding[BufferAlignment] = {0};
      WriteBytes(&padding[0], (size_t)(alignedoffs - offs));
    }

    RDCASSERT((GetOffset() % BufferAlignment) == 0);

    WriteBytes(buf, bufLen);

    m_AlignedData = true;
  }
  else
  {
    ReadInto(bufLen);

    // ensure byte alignment
    uint64_t offs = GetOffset();

    // serialise version 0x00000031 had only 16-byte alignment
    uint64_t alignedoffs = AlignUp(offs, m_SerVer == 0x00000031 ? 16 : BufferAlignment);

    if(offs != alignedoffs)
    {
      ReadBytes((size_t)(alignedoffs - offs));
    }

    if(buf == NULL)
      buf = new byte[bufLen];
    memcpy(buf, ReadBytes(bufLen), bufLen);
  }

  len = (size_t)bufLen;

  if(m_DebugTextWriting && name && name[0])
  {
    const char *ellipsis = "...";

    uint32_t lbuf[4];

    memcpy(lbuf, buf, RDCMIN(len, 4 * sizeof(uint32_t)));

    if(bufLen <= 16)
    {
      ellipsis = "   ";
    }

    DebugPrint("%s: RawBuffer % 5d:< 0x%08x 0x%08x 0x%08x 0x%08x %s>\n", name, bufLen, lbuf[0],
               lbuf[1], lbuf[2], lbuf[3], ellipsis);
  }
}

template <>
void Serialiser::Serialise(const char *name, string &el)
{
  SerialiseString(name, el);
}

// floats need aligned reads
template <>
void Serialiser::ReadInto(float &f)
{
  if(m_HasError)
  {
    RDCERR("Reading into with error state serialiser");
    return;
  }

  char *data = (char *)ReadBytes(sizeof(float));

  memcpy(&f, data, sizeof(float));
}

/////////////////////////////////////////////////////////////
// String conversions for debug log.

/////////////////////////////////////////////////////////////
// Basic types

template <>
string ToStrHelper<false, void *>::Get(void *const &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "0x%p", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, int64_t>::Get(const int64_t &el)
{
  char tostrBuf[256] = {0};

  StringFormat::snprintf(tostrBuf, 255, "%lld", el);

  return tostrBuf;
}

// this is super ugly, but I don't see a way around it - on other
// platforms size_t is typedef'd in such a way that the uint32_t or
// uint64_t specialisation will kick in. On apple, we need a
// specific size_t overload
#if ENABLED(RDOC_APPLE)
template <>
string ToStrHelper<false, size_t>::Get(const size_t &el)
{
  char tostrBuf[256] = {0};

  StringFormat::snprintf(tostrBuf, 255, "%llu", (uint64_t)el);

  return tostrBuf;
}
#endif

template <>
string ToStrHelper<false, uint64_t>::Get(const uint64_t &el)
{
  char tostrBuf[256] = {0};

  StringFormat::snprintf(tostrBuf, 255, "%llu", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, uint32_t>::Get(const uint32_t &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%u", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, char>::Get(const char &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "'%c'", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, wchar_t>::Get(const wchar_t &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "'%lc'", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, byte>::Get(const byte &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%08hhb", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, uint16_t>::Get(const uint16_t &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%04d", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, int32_t>::Get(const int &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%d", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, int16_t>::Get(const short &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%04d", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, float>::Get(const float &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%0.4f", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, double>::Get(const double &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "%0.4lf", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, bool>::Get(const bool &el)
{
  if(el)
    return "True";

  return "False";
}
