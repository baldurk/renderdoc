/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "rdcfile.h"
#include <errno.h>
#include "3rdparty/jpeg-compressor/jpge.h"
#include "3rdparty/stb/stb_image.h"
#include "api/replay/version.h"
#include "common/dds_readwrite.h"
#include "lz4io.h"
#include "zstdio.h"

// not provided by tinyexr, just do by hand
bool is_exr_file(FILE *f)
{
  FileIO::fseek64(f, 0, SEEK_SET);

  const uint32_t openexr_magic = MAKE_FOURCC(0x76, 0x2f, 0x31, 0x01);

  uint32_t magic = 0;
  size_t bytesRead = FileIO::fread(&magic, 1, sizeof(magic), f);

  FileIO::fseek64(f, 0, SEEK_SET);

  return bytesRead == sizeof(magic) && magic == openexr_magic;
}

/*

 -----------------------------
 File format for version 0x100:

 RDCHeader
 {
   uint64_t MAGIC_HEADER;

   uint32_t version = 0x00000100;
   uint32_t headerLength; // length of this header, from the start of the file. Allows adding new
                          // fields without breaking compatibilty
   char progVersion[16]; // string "v0.34" or similar with 0s after the string

   // thumbnail
   uint16_t thumbWidth;
   uint16_t thumbHeight; // thumbnail width and height. If 0x0, no thumbnail data
   uint32_t thumbLength; // number of bytes in thumbnail array below
   byte thumbData[ thumbLength ]; // JPG compressed thumbnail

   // where was the capture created
   uint64_t machineIdent;

   uint32_t driverID; // the RDCDriver used for this log
   uint8_t driverNameLength; // length in bytes of the driver name including null terminator
   char driverName[ driverNameLength ]; // the driver name in ASCII. Useful if the current
                                        // implementation doesn't recognise the driver ID above
 }

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
     char sectionVersion[]; // section version, as decimal string. May be 0 when not necessary.
     char newline = '\n';
     char sectionName[]; // UTF-8 string name of section.
     char newline = '\n';

     // sectionName is an arbitrary string.
     //
     // No two sections may have the same section type or section name. Any file
     // with duplicates is ill-formed and it's undefined how the file is interpreted.

     byte sectiondata[ atoi(length) ]; // section data
   }
   else if(isASCII == '\0')
   {
     byte zero[3]; // pad out the above character with 0 bytes. Reserved for future use
     uint32_t sectionType; // section type enum, see SectionType. Could be SectionType::Unknown
     uint64_t sectionCompressedLength;   // byte length of the actual section data on disk
     uint64_t sectionUncompressedLength; // byte length of the section data after decompression.
                                         // If the section isn't compressed this will be equal to
                                         // sectionLength
     uint64_t sectionVersion; // section version number.
                              // The meaning of this is section specific and may be 0 if a version
                              // isn't needed. Most commonly it's used for the frame capture section
                              // to store the version of the data within.
     uint32_t sectionFlags; // section flags - e.g. is compressed or not.
     uint32_t sectionNameLength; // byte length of the string below (minimum 1, for null terminator)
     char sectionName[sectionNameLength]; // UTF-8 string name of section, optional.

     byte sectiondata[length]; // actual contents of the section
   }
 };

 // remainder of the file is tightly packed/unaligned section structures.
 // The first section must always be the actual frame capture data in
 // binary form, other sections can follow in any order
 Section sections[];

*/

static const uint32_t MAGIC_HEADER = MAKE_FOURCC('R', 'D', 'O', 'C');

namespace
{
struct FileHeader
{
  FileHeader()
  {
    magic = MAGIC_HEADER;
    version = RDCFile::SERIALISE_VERSION;
    headerLength = 0;
    RDCEraseEl(progVersion);
    char ver[] = MAJOR_MINOR_VERSION_STRING " xxxxxx";
    char *hash = strstr(ver, "xxxxxx");
    memcpy(hash, GitVersionHash, 6);

    memcpy(progVersion, ver, RDCMIN(sizeof(progVersion), sizeof(ver)));
  }

  uint64_t magic;

  uint32_t version;
  uint32_t headerLength;

  // string "v0.34" or similar with 0s after the string
  char progVersion[16];
};

struct BinaryThumbnail
{
  // thumbnail width and height. If 0x0, no thumbnail data
  uint16_t width;
  uint16_t height;
  // number of bytes in thumbnail array below
  uint32_t length;
  // JPG compressed thumbnail
  byte data[1];
};

struct CaptureMetaData
{
  // where was the capture created
  uint64_t machineIdent = 0;

  // the RDCDriver used for this log
  RDCDriver driverID = RDCDriver::Unknown;
  // length in bytes of the driver name
  uint8_t driverNameLength = 1;
  // the driver name in ASCII. Useful if the current implementation doesn't recognise the driver
  // ID above
  char driverName[1] = {0};
};

struct BinarySectionHeader
{
  // 0x0
  byte isASCII;
  // 0x0, 0x0, 0x0
  byte zero[3];
  // section type enum, see SectionType. Could be SectionType::Unknown
  SectionType sectionType;
  // byte length of the actual section data on disk
  uint64_t sectionCompressedLength;
  // byte length of the section data after decompression, could be equal to sectionLength if the
  // section is not compressed
  uint64_t sectionUncompressedLength;
  // section version number, with a section specific meaning - could be 0 if not needed.
  uint64_t sectionVersion;
  // section flags - e.g. is compressed or not.
  SectionFlags sectionFlags;
  // byte length of the string below (could be 0)
  uint32_t sectionNameLength;
  // actually sectionNameLength, but at least 1 for null terminator
  char name[1];

  // char name[sectionNameLength];
  // byte data[sectionLength];
};
};

#define SETERROR(error, ...)                        \
  {                                                 \
    m_ErrorString = StringFormat::Fmt(__VA_ARGS__); \
    RDCERR("%s", m_ErrorString.c_str());            \
    m_Error = error;                                \
  }

#define RETURNERROR(error, ...)   \
  {                               \
    SETERROR(error, __VA_ARGS__); \
    return;                       \
  }

RDCFile::~RDCFile()
{
  if(m_File)
    FileIO::fclose(m_File);

  if(m_Thumb.pixels)
    delete[] m_Thumb.pixels;
}

void RDCFile::Open(const char *path)
{
  // silently fail when opening the empty string, to allow 'releasing' a capture file by opening an
  // empty path.
  if(path == NULL || path[0] == 0)
  {
    RETURNERROR(ContainerError::FileNotFound, "Invalid file path specified");
  }

  RDCLOG("Opening RDCFile %s", path);

  // ensure section header is compiled correctly
  RDCCOMPILE_ASSERT(offsetof(BinarySectionHeader, name) == sizeof(uint32_t) * 10,
                    "BinarySectionHeader size has changed or contains padding");

  m_File = FileIO::fopen(path, "rb");
  m_Filename = path;

  if(!m_File)
  {
    RETURNERROR(ContainerError::FileNotFound, "Can't open capture file '%s' for read - errno %d",
                path, errno);
  }

  // try to identify if this is an image
  {
    int x = 0, y = 0, comp = 0;
    int ret = stbi_info_from_file(m_File, &x, &y, &comp);

    FileIO::fseek64(m_File, 0, SEEK_SET);

    if(is_dds_file(m_File))
      ret = x = y = comp = 1;

    if(is_exr_file(m_File))
      ret = x = y = comp = 1;

    FileIO::fseek64(m_File, 0, SEEK_SET);

    if(ret == 1 && x > 0 && y > 0 && comp > 0)
    {
      m_Driver = RDCDriver::Image;
      m_DriverName = "Image";
      m_MachineIdent = 0;
      return;
    }
  }

  FileIO::fseek64(m_File, 0, SEEK_END);
  uint64_t fileSize = FileIO::ftell64(m_File);
  FileIO::fseek64(m_File, 0, SEEK_SET);

  StreamReader reader(m_File, fileSize, Ownership::Nothing);

  Init(reader);
}

void RDCFile::Open(const std::vector<byte> &buffer)
{
  m_Buffer = buffer;
  m_File = NULL;

  StreamReader reader(m_Buffer);

  Init(reader);
}

void RDCFile::Init(StreamReader &reader)
{
  RDCDEBUG("Opened capture file for read");

  // read the first part of the file header
  FileHeader header;
  reader.Read(header);

  if(reader.IsErrored())
  {
    RETURNERROR(ContainerError::FileIO, "I/O error reading magic number");
  }

  if(header.magic != MAGIC_HEADER)
  {
    RETURNERROR(ContainerError::Corrupt, "Invalid capture file. Expected magic %08x, got %08x.",
                MAGIC_HEADER, (uint32_t)header.magic);
  }

  m_SerVer = header.version;

  if(m_SerVer != SERIALISE_VERSION && m_SerVer != V1_0_VERSION)
  {
    if(header.version < V1_0_VERSION)
    {
      RDCEraseEl(header.progVersion);
      memcpy(header.progVersion, "v0.x", sizeof("v0.x"));
    }

    RETURNERROR(
        ContainerError::UnsupportedVersion,
        "Capture file from wrong version. This program (v%s) uses logfile version %u, this file is "
        "logfile version %u captured on %s.",
        MAJOR_MINOR_VERSION_STRING, SERIALISE_VERSION, header.version, header.progVersion);
  }

  BinaryThumbnail thumb;
  reader.Read(&thumb, offsetof(BinaryThumbnail, data));

  if(reader.IsErrored())
  {
    RETURNERROR(ContainerError::FileIO, "I/O error reading thumbnail header");
  }

  // check the thumbnail size is sensible
  if(thumb.length > 10 * 1024 * 1024)
  {
    RETURNERROR(ContainerError::Corrupt, "Thumbnail byte length invalid: %u", thumb.length);
  }

  byte *thumbData = new byte[thumb.length];
  reader.Read(thumbData, thumb.length);

  if(reader.IsErrored())
  {
    delete[] thumbData;
    RETURNERROR(ContainerError::FileIO, "I/O error reading thumbnail data");
  }

  CaptureMetaData meta;
  reader.Read(&meta, offsetof(CaptureMetaData, driverName));

  if(reader.IsErrored())
  {
    delete[] thumbData;
    RETURNERROR(ContainerError::FileIO, "I/O error reading capture metadata");
  }

  if(meta.driverNameLength == 0)
  {
    delete[] thumbData;
    RETURNERROR(ContainerError::Corrupt,
                "Driver name length is invalid, must be at least 1 to contain NULL terminator");
  }

  char *driverName = new char[meta.driverNameLength];
  reader.Read(driverName, meta.driverNameLength);

  if(reader.IsErrored())
  {
    delete[] thumbData;
    delete[] driverName;
    RETURNERROR(ContainerError::FileIO, "I/O error reading driver name");
  }

  driverName[meta.driverNameLength - 1] = '\0';

  m_Driver = meta.driverID;
  m_DriverName = driverName;
  m_MachineIdent = meta.machineIdent;
  m_Thumb.width = thumb.width;
  m_Thumb.height = thumb.height;
  m_Thumb.len = thumb.length;
  m_Thumb.format = FileType::JPG;

  if(m_Thumb.len > 0 && m_Thumb.width > 0 && m_Thumb.height > 0)
  {
    m_Thumb.pixels = thumbData;
    thumbData = NULL;
  }

  delete[] thumbData;
  delete[] driverName;

  if(reader.GetOffset() > header.headerLength)
  {
    RETURNERROR(ContainerError::FileIO, "I/O error seeking to end of header");
  }

  reader.SkipBytes(header.headerLength - (uint32_t)reader.GetOffset());

  while(!reader.AtEnd())
  {
    BinarySectionHeader sectionHeader = {0};
    byte *reading = (byte *)&sectionHeader;

    uint64_t headerOffset = reader.GetOffset();

    reader.Read(*reading);
    reading++;

    if(reader.IsErrored())
      break;

    if(sectionHeader.isASCII == 'A')
    {
      // ASCII section
      char c = 0;
      reader.Read(c);
      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Invalid ASCII data section '%hhx'", c);

      if(reader.AtEnd())
        RETURNERROR(ContainerError::Corrupt, "Invalid truncated ASCII data section");

      uint64_t length = 0;

      c = '0';

      while(!reader.IsErrored() && c != '\n')
      {
        reader.Read(c);

        if(c == '\n' || reader.IsErrored())
          break;

        length *= 10;
        length += int(c - '0');
      }

      if(reader.IsErrored() || reader.AtEnd())
        RETURNERROR(ContainerError::Corrupt, "Invalid truncated ASCII data section");

      uint32_t type = 0;

      c = '0';

      while(!reader.AtEnd() && c != '\n')
      {
        reader.Read(c);

        if(c == '\n' || reader.IsErrored())
          break;

        type *= 10;
        type += int(c - '0');
      }

      if(reader.IsErrored() || reader.AtEnd())
        RETURNERROR(ContainerError::Corrupt, "Invalid truncated ASCII data section");

      uint64_t version = 0;

      c = '0';

      while(!reader.AtEnd() && c != '\n')
      {
        reader.Read(c);

        if(c == '\n' || reader.IsErrored())
          break;

        version *= 10;
        version += int(c - '0');
      }

      if(reader.IsErrored() || reader.AtEnd())
        RETURNERROR(ContainerError::Corrupt, "Invalid truncated ASCII data section");

      std::string name;

      c = 0;

      while(!reader.AtEnd() && c != '\n')
      {
        reader.Read(c);

        if(c == 0 || c == '\n' || reader.IsErrored())
          break;

        name.push_back(c);
      }

      if(reader.IsErrored() || reader.AtEnd())
        RETURNERROR(ContainerError::Corrupt, "Invalid truncated ASCII data section");

      SectionProperties props;
      props.flags = SectionFlags::ASCIIStored;
      props.type = (SectionType)type;
      props.name = name;
      props.version = version;
      props.compressedSize = length;
      props.uncompressedSize = length;

      SectionLocation loc;
      loc.headerOffset = headerOffset;
      loc.dataOffset = reader.GetOffset();
      loc.diskLength = length;

      reader.SkipBytes(loc.diskLength);

      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Error seeking past ASCII section '%s' data",
                    name.c_str());

      m_Sections.push_back(props);
      m_SectionLocations.push_back(loc);
    }
    else if(sectionHeader.isASCII == 0x0)
    {
      // -1 because we've already read the isASCII byte
      reader.Read(reading, offsetof(BinarySectionHeader, name) - 1);

      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Error reading binary section header");

      SectionProperties props;
      props.flags = sectionHeader.sectionFlags;
      props.type = sectionHeader.sectionType;
      props.compressedSize = sectionHeader.sectionCompressedLength;
      props.uncompressedSize = sectionHeader.sectionUncompressedLength;
      props.version = sectionHeader.sectionVersion;

      if(sectionHeader.sectionNameLength == 0 || sectionHeader.sectionNameLength > 2 * 1024)
      {
        RETURNERROR(ContainerError::Corrupt, "Invalid section name length %u",
                    sectionHeader.sectionNameLength);
      }

      props.name.resize(sectionHeader.sectionNameLength - 1);

      reader.Read(&props.name[0], sectionHeader.sectionNameLength - 1);

      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Error reading binary section header");

      reader.SkipBytes(1);

      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Error reading binary section header");

      SectionLocation loc;
      loc.headerOffset = headerOffset;
      loc.dataOffset = reader.GetOffset();
      loc.diskLength = sectionHeader.sectionCompressedLength;

      m_Sections.push_back(props);
      m_SectionLocations.push_back(loc);

      reader.SkipBytes(loc.diskLength);

      if(reader.IsErrored())
        RETURNERROR(ContainerError::Corrupt, "Error seeking past binary section '%s' data",
                    props.name.c_str());
    }
    else
    {
      RETURNERROR(ContainerError::Corrupt, "Unrecognised section type '%hhx'", sectionHeader.isASCII);
    }
  }

  if(SectionIndex(SectionType::FrameCapture) == -1)
  {
    RETURNERROR(ContainerError::Corrupt, "Capture file doesn't have a frame capture");
  }

  int index = SectionIndex(SectionType::ExtendedThumbnail);
  if(index >= 0)
  {
    StreamReader *thumbReader = ReadSection(index);
    if(thumbReader)
    {
      ExtThumbnailHeader thumbHeader;
      if(thumbReader->Read(thumbHeader))
      {
        thumbData = new byte[thumbHeader.len];
        bool succeeded = thumbReader->Read(thumbData, thumbHeader.len) && !thumbReader->IsErrored();
        if(succeeded && (uint32_t)thumbHeader.format < (uint32_t)FileType::Count)
        {
          m_Thumb.width = thumbHeader.width;
          m_Thumb.height = thumbHeader.height;
          m_Thumb.len = thumbHeader.len;
          m_Thumb.format = thumbHeader.format;
          delete[] m_Thumb.pixels;
          m_Thumb.pixels = thumbData;
        }
        else
        {
          delete[] thumbData;
        }
        thumbData = NULL;
      }
      delete thumbReader;
    }
  }
}

bool RDCFile::CopyFileTo(const char *filename)
{
  if(!m_File)
    return false;

  // remember our position and close the file
  uint64_t prevPos = FileIO::ftell64(m_File);
  FileIO::fclose(m_File);

  // try to move to the new location
  bool success = FileIO::Copy(m_Filename.c_str(), filename, true);

  // if it succeeded, update our filename
  if(success)
    m_Filename = filename;

  // re-open the file (either the new one, or the old one if it failed) and re-seek
  m_File = FileIO::fopen(m_Filename.c_str(), "rb");
  FileIO::fseek64(m_File, prevPos, SEEK_SET);

  return success;
}

void RDCFile::SetData(RDCDriver driver, const char *driverName, uint64_t machineIdent,
                      const RDCThumb *thumb)
{
  m_Driver = driver;
  m_DriverName = driverName;
  m_MachineIdent = machineIdent;
  if(thumb)
  {
    m_Thumb = *thumb;

    byte *pixels = new byte[m_Thumb.len];
    memcpy(pixels, thumb->pixels, m_Thumb.len);

    m_Thumb.pixels = pixels;
  }
}

void RDCFile::Create(const char *filename)
{
  m_File = FileIO::fopen(filename, "wb");
  m_Filename = filename;

  RDCDEBUG("creating RDC file.");

  if(!m_File)
  {
    RETURNERROR(ContainerError::FileIO, "Can't open capture file '%s' for write, errno %d",
                filename, errno);
  }

  RDCDEBUG("Opened capture file for write");

  FileHeader header;    // automagically initialised with correct data apart from length

  BinaryThumbnail thumbHeader = {0};

  thumbHeader.width = m_Thumb.width;
  thumbHeader.height = m_Thumb.height;
  const byte *jpgPixels = m_Thumb.pixels;
  thumbHeader.length = m_Thumb.len;

  byte *jpgBuffer = NULL;
  if(m_Thumb.format != FileType::JPG && m_Thumb.width > 0 && m_Thumb.height > 0)
  {
    // the primary thumbnail must be in JPG format, must perform conversion
    const byte *rawPixels = NULL;
    byte *rawBuffer = NULL;
    int w = (int)m_Thumb.width;
    int h = (int)m_Thumb.height;
    int comp = 3;

    if(m_Thumb.format == FileType::Raw)
    {
      rawPixels = m_Thumb.pixels;
    }
    else
    {
      rawBuffer = stbi_load_from_memory(m_Thumb.pixels, (int)m_Thumb.len, &w, &h, &comp, 3);
      rawPixels = rawBuffer;
    }

    if(rawPixels)
    {
      int len = w * h * comp;
      jpgBuffer = new byte[len];
      jpge::params p;
      p.m_quality = 90;
      jpge::compress_image_to_jpeg_file_in_memory(jpgBuffer, len, w, h, comp, rawPixels, p);
      thumbHeader.length = (uint32_t)len;
      jpgPixels = jpgBuffer;
    }
    else
    {
      thumbHeader.width = 0;
      thumbHeader.height = 0;
      thumbHeader.length = 0;
      jpgPixels = NULL;
    }
    if(rawBuffer)
      stbi_image_free(rawBuffer);
  }

  CaptureMetaData meta;
  meta.driverID = m_Driver;
  meta.machineIdent = m_MachineIdent;
  meta.driverNameLength = uint8_t(m_DriverName.size() + 1);

  header.headerLength = sizeof(FileHeader) + offsetof(BinaryThumbnail, data) + thumbHeader.length +
                        offsetof(CaptureMetaData, driverName) + meta.driverNameLength;

  {
    StreamWriter writer(m_File, Ownership::Nothing);

    writer.Write(header);
    writer.Write(&thumbHeader, offsetof(BinaryThumbnail, data));

    if(thumbHeader.length > 0)
      writer.Write(jpgPixels, thumbHeader.length);

    writer.Write(&meta, offsetof(CaptureMetaData, driverName));

    writer.Write(m_DriverName.c_str(), meta.driverNameLength);

    delete[] jpgBuffer;
    if(writer.IsErrored())
    {
      RETURNERROR(ContainerError::FileIO, "Error writing file header");
    }
  }

  // re-open as read-only now.
  FileIO::fclose(m_File);
  m_File = FileIO::fopen(filename, "rb");
  FileIO::fseek64(m_File, 0, SEEK_END);
}

int RDCFile::SectionIndex(SectionType type) const
{
  // Unknown is not a real type, any arbitrary sections with names will be listed as unknown, so
  // don't return a false-positive index. This allows us to skip some special cases outside
  if(type == SectionType::Unknown)
    return -1;

  for(size_t i = 0; i < m_Sections.size(); i++)
    if(m_Sections[i].type == type)
      return int(i);

  return -1;
}

int RDCFile::SectionIndex(const char *name) const
{
  for(size_t i = 0; i < m_Sections.size(); i++)
    if(m_Sections[i].name == name)
      return int(i);

  // last ditch, see if name is a known section type and search for that type. This should have been
  // normalised on write, but maybe it didn't
  for(SectionType s : values<SectionType>())
    if(ToStr(s) == name)
      return SectionIndex(s);

  return -1;
}

StreamReader *RDCFile::ReadSection(int index) const
{
  if(m_Error != ContainerError::NoError)
    return new StreamReader(StreamReader::InvalidStream);

  if(m_File == NULL)
  {
    if(index < (int)m_MemorySections.size())
      return new StreamReader(m_MemorySections[index]);

    RDCERR("Section %d is not available in memory.", index);
    return new StreamReader(StreamReader::InvalidStream);
  }

  const SectionProperties &props = m_Sections[index];
  SectionLocation offsetSize = m_SectionLocations[index];
  FileIO::fseek64(m_File, offsetSize.dataOffset, SEEK_SET);

  StreamReader *fileReader = new StreamReader(m_File, offsetSize.diskLength, Ownership::Nothing);

  StreamReader *compReader = NULL;

  if(props.flags & SectionFlags::LZ4Compressed)
  {
    // the user will delete the compressed reader, and then it will delete the compressor and the
    // file reader
    compReader = new StreamReader(new LZ4Decompressor(fileReader, Ownership::Stream),
                                  props.uncompressedSize, Ownership::Stream);
  }
  else if(props.flags & SectionFlags::ZstdCompressed)
  {
    compReader = new StreamReader(new ZSTDDecompressor(fileReader, Ownership::Stream),
                                  props.uncompressedSize, Ownership::Stream);
  }

  // if we're compressing return that writer, otherwise return the file writer directly
  return compReader ? compReader : fileReader;
}

StreamWriter *RDCFile::WriteSection(const SectionProperties &props)
{
  if(m_Error != ContainerError::NoError)
    return new StreamWriter(StreamWriter::InvalidStream);

  RDCASSERT((size_t)props.type < (size_t)SectionType::Count);

  if(m_File == NULL)
  {
    // if we have no file to write to, we just cache it in memory for future use (e.g. later writing
    // to disk via the CaptureFile interface wih structured data for the frame capture section)
    StreamWriter *w = new StreamWriter(64 * 1024);

    w->AddCloseCallback([this, props, w]() {
      m_MemorySections.push_back(std::vector<byte>(w->GetData(), w->GetData() + w->GetOffset()));

      m_Sections.push_back(props);
      m_Sections.back().compressedSize = m_Sections.back().uncompressedSize =
          m_MemorySections.back().size();
    });

    return w;
  }

  // re-open the file as read-write
  {
    uint64_t offs = FileIO::ftell64(m_File);
    FileIO::fclose(m_File);
    m_File = FileIO::fopen(m_Filename.c_str(), "r+b");

    if(m_File == NULL)
    {
      RDCERR("Couldn't re-open file as read/write to write section.");
      m_File = FileIO::fopen(m_Filename.c_str(), "rb");
      if(m_File)
        FileIO::fseek64(m_File, offs, SEEK_SET);
      return new StreamWriter(StreamWriter::InvalidStream);
    }

    FileIO::fseek64(m_File, offs, SEEK_SET);
  }

  if(m_Sections.empty() && props.type != SectionType::FrameCapture)
  {
    RDCERR("The first section written must be frame capture data.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  if(!m_CurrentWritingProps.name.empty())
  {
    RDCERR("Only one section can be written at once.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  std::string name = props.name;
  SectionType type = props.type;

  // normalise names for known sections
  if(type != SectionType::Unknown && type < SectionType::Count)
    name = ToStr(type);

  if(name.empty())
  {
    RDCERR("Sections must have a name, either auto-populated from a known type or specified.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  // For handling a section that does exist, it depends on the section type:
  // - For frame capture, then we just write to a new file since we want it
  //   to be first. Once the writing is done, copy across any other sections
  //   after it.
  // - For non-frame capture, we remove the existing section and move up any
  //   sections that were after it. Then just return a new writer that appends

  // we store this callback here so that we can execute it after any post-section-writing header
  // fixups. We need to be able to fixup any pre-existing sections that got shifted around.
  StreamCloseCallback modifySectionCallback;

  if(SectionIndex(type) >= 0 || SectionIndex(name.c_str()) >= 0)
  {
    if(type == SectionType::FrameCapture || name == ToStr(SectionType::FrameCapture))
    {
      // simple case - if there are no other sections then we can just overwrite the existing frame
      // capture.
      if(NumSections() == 1)
      {
        // seek to the start of where the section is.
        FileIO::fseek64(m_File, m_SectionLocations[0].headerOffset, SEEK_SET);

        uint64_t oldLength = m_SectionLocations[0].diskLength;

        // after writing, we need to be sure to fixup the size (in case we wrote less data).
        modifySectionCallback = [this, oldLength]() {
          if(oldLength > m_SectionLocations[0].diskLength)
          {
            FileIO::ftruncateat(
                m_File, m_SectionLocations[0].dataOffset + m_SectionLocations[0].diskLength);
          }
        };
      }
      else
      {
        FILE *origFile = m_File;

        // save the sections
        std::vector<SectionProperties> origSections = m_Sections;
        std::vector<SectionLocation> origSectionLocations = m_SectionLocations;

        SectionLocation oldCaptureLocation = m_SectionLocations[0];

        // remove section 0, the frame capture, since it will be fixed up separately
        origSections.erase(origSections.begin());
        origSectionLocations.erase(origSectionLocations.begin());

        std::string tempFilename = FileIO::GetTempFolderFilename() + "capture_rewrite.rdc";

        // create the file, this will overwrite m_File with the new file and file header using the
        // existing loaded metadata
        Create(tempFilename.c_str());

        // after we've written the frame capture, we need to copy over the other sections into the
        // temporary file and finally move the temporary file over the top of the existing file.
        modifySectionCallback = [this, origFile, origSections, origSectionLocations, tempFilename]() {
          // seek to write after the frame capture
          FileIO::fseek64(
              m_File, m_SectionLocations[0].dataOffset + m_SectionLocations[0].diskLength, SEEK_SET);

          // write the old sections
          for(size_t i = 0; i < origSections.size(); i++)
          {
            SectionLocation loc = origSectionLocations[i];

            FileIO::fseek64(origFile, loc.headerOffset, SEEK_SET);

            uint64_t newHeaderOffset = FileIO::ftell64(m_File);

            // update the offsets to where they are in the new file
            if(newHeaderOffset > loc.headerOffset)
            {
              uint64_t delta = newHeaderOffset - loc.headerOffset;

              loc.headerOffset += delta;
              loc.dataOffset += delta;
            }
            else if(newHeaderOffset < loc.headerOffset)
            {
              uint64_t delta = loc.headerOffset - newHeaderOffset;

              loc.headerOffset -= delta;
              loc.dataOffset -= delta;
            }

            uint64_t headerLen = loc.dataOffset - loc.headerOffset;

            // copy header and data together
            StreamWriter writer(m_File, Ownership::Nothing);
            StreamReader reader(origFile, headerLen + loc.diskLength, Ownership::Nothing);

            m_Sections.push_back(origSections[i]);
            m_SectionLocations.push_back(loc);

            StreamTransfer(&writer, &reader, NULL);
          }

          // close the file writing to the temp location
          FileIO::fclose(m_File);

          // move the temp file over the original
          FileIO::Move(tempFilename.c_str(), m_Filename.c_str(), true);

          // re-open the file after it's been overwritten.
          m_File = FileIO::fopen(m_Filename.c_str(), "r+b");
        };

        // fall through - we'll write to m_File immediately after the file header
      }

      // the new section data for the framecapture will be pushed on after writing. Any others will
      // be re-added in the fixup step above
      m_Sections.clear();
      m_SectionLocations.clear();
    }
    else
    {
      // we're writing some section after the frame capture. We'll do this in-place by reading the
      // other sections out to memory (assuming that they are mostly small, and even if they are
      // somewhat large, it's still much better to leave the frame capture (which should dominate
      // file size) on disk where it is.
      int index = SectionIndex(type);

      if(index < 0)
        index = SectionIndex(name.c_str());

      RDCASSERT(index >= 0);

      std::vector<bytebuf> origSectionData;
      std::vector<uint64_t> origHeaderSizes;

      uint64_t overwriteLocation = m_SectionLocations[index].headerOffset;
      uint64_t oldLength = m_SectionLocations[index].diskLength;

      // erase the target section. The others will be moved up to match
      m_Sections.erase(m_Sections.begin() + index);
      m_SectionLocations.erase(m_SectionLocations.begin() + index);

      origSectionData.reserve(NumSections() - index);
      origHeaderSizes.reserve(NumSections() - index);

      // go through all subsequent sections after this one in the file, read them into memory.
      // this could be optimised since we're going to write them back out below, we could do this
      // just with an in-memory window large enough.
      for(int i = index; i < NumSections(); i++)
      {
        const SectionLocation &loc = m_SectionLocations[i];

        FileIO::fseek64(m_File, loc.headerOffset, SEEK_SET);

        uint64_t headerLen = loc.dataOffset - loc.headerOffset;

        // read header and data together
        StreamReader reader(m_File, headerLen + loc.diskLength, Ownership::Nothing);

        origHeaderSizes.push_back(headerLen);
        origSectionData.push_back(bytebuf());

        bytebuf &data = origSectionData.back();
        data.resize((size_t)reader.GetSize());
        reader.Read(data.data(), data.size());
      }

      // we write the sections now over where the old section used to be, so the newly written
      // section is last in the file. This means if the same section is updated over and over, it
      // doesn't require moving any sections once it's already at the end.

      // seek to write to where the removed section started
      FileIO::fseek64(m_File, overwriteLocation, SEEK_SET);

      // write the old sections
      for(size_t i = 0; i < origSectionData.size(); i++)
      {
        // update the offsets to where they are in the new file
        m_SectionLocations[index + i].headerOffset = FileIO::ftell64(m_File);
        m_SectionLocations[index + i].dataOffset =
            m_SectionLocations[index + i].headerOffset + origHeaderSizes[i];

        // write the data
        StreamWriter writer(m_File, Ownership::Nothing);
        writer.Write(origSectionData[i].data(), origSectionData[i].size());
      }

      // after writing, we need to be sure to fixup the size (in case we wrote less data).
      modifySectionCallback = [this, oldLength]() {
        if(oldLength > m_SectionLocations.back().diskLength)
        {
          FileIO::ftruncateat(
              m_File, m_SectionLocations.back().dataOffset + m_SectionLocations.back().diskLength);
        }
      };

      // fall through - we now write to m_File with the new section wherever we left off after the
      // moved sections.
    }
  }
  else
  {
    // we're adding a new section - seek to the end of the file to append it
    FileIO::fseek64(m_File, 0, SEEK_END);
  }

  uint64_t headerOffset = FileIO::ftell64(m_File);

  size_t numWritten;

  // write section header
  BinarySectionHeader header = {// IsASCII
                                '\0',
                                // zero
                                {0, 0, 0},
                                // sectionType
                                type,
                                // sectionCompressedLength
                                0,
                                // sectionUncompressedLength
                                0,
                                // sectionVersion
                                props.version,
                                // sectionFlags
                                props.flags,
                                // sectionNameLength
                                uint32_t(name.length() + 1)};

  // write the header then name
  numWritten = FileIO::fwrite(&header, 1, offsetof(BinarySectionHeader, name), m_File);
  numWritten += FileIO::fwrite(name.c_str(), 1, name.size() + 1, m_File);

  if(numWritten != offsetof(BinarySectionHeader, name) + name.size() + 1)
  {
    SETERROR(ContainerError::FileIO, "Error seeking to end of file, errno %d", errno);
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  // create a writer for writing to disk. It shouldn't close the file
  StreamWriter *fileWriter = new StreamWriter(m_File, Ownership::Nothing);

  StreamWriter *compWriter = NULL;

  if(props.flags & SectionFlags::LZ4Compressed)
  {
    // the user will delete the compressed writer, and then it will delete the compressor and the
    // file writer
    compWriter =
        new StreamWriter(new LZ4Compressor(fileWriter, Ownership::Stream), Ownership::Stream);
  }
  else if(props.flags & SectionFlags::ZstdCompressed)
  {
    compWriter =
        new StreamWriter(new ZSTDCompressor(fileWriter, Ownership::Stream), Ownership::Stream);
  }

  uint64_t dataOffset = FileIO::ftell64(m_File);

  m_CurrentWritingProps = props;
  m_CurrentWritingProps.name = name;

  // register a destroy callback to tidy up the section at the end
  fileWriter->AddCloseCallback([this, type, name, headerOffset, dataOffset, fileWriter, compWriter]() {
    FileIO::fflush(m_File);

    // the offset of the file writer is how many bytes were written to disk - the compressed length.
    uint64_t compressedLength = fileWriter->GetOffset();

    // if there was no compression, this is also the uncompressed length.
    uint64_t uncompressedLength = compressedLength;
    if(compWriter)
      uncompressedLength = compWriter->GetOffset();

    RDCLOG("Finishing write to section %u (%s). Compressed from %llu bytes to %llu", type,
           name.c_str(), uncompressedLength, compressedLength);

    // finish up the properties and add to list of sections
    m_CurrentWritingProps.compressedSize = compressedLength;
    m_CurrentWritingProps.uncompressedSize = uncompressedLength;

    m_Sections.push_back(m_CurrentWritingProps);
    SectionLocation loc;
    loc.headerOffset = headerOffset;
    loc.dataOffset = dataOffset;
    loc.diskLength = compressedLength;
    m_SectionLocations.push_back(loc);

    m_CurrentWritingProps = SectionProperties();

    FileIO::fseek64(m_File, headerOffset + offsetof(BinarySectionHeader, sectionCompressedLength),
                    SEEK_SET);

    size_t bytesWritten = FileIO::fwrite(&compressedLength, 1, sizeof(uint64_t), m_File);
    bytesWritten += FileIO::fwrite(&uncompressedLength, 1, sizeof(uint64_t), m_File);

    if(bytesWritten != 2 * sizeof(uint64_t))
    {
      RETURNERROR(ContainerError::FileIO, "Error applying fixup to section header, errno %d", errno);
    }

    FileIO::fflush(m_File);
  });

  if(modifySectionCallback)
    fileWriter->AddCloseCallback(modifySectionCallback);

  // finally once we're done, re-open the file as read-only again
  fileWriter->AddCloseCallback([this]() {
    // remember our position and close the file
    uint64_t prevPos = FileIO::ftell64(m_File);
    FileIO::fclose(m_File);

    // re-open the file and re-seek
    m_File = FileIO::fopen(m_Filename.c_str(), "rb");
    FileIO::fseek64(m_File, prevPos, SEEK_SET);
  });

  // if we're compressing return that writer, otherwise return the file writer directly
  return compWriter ? compWriter : fileWriter;
}

FILE *RDCFile::StealImageFileHandle(std::string &filename)
{
  if(m_Driver != RDCDriver::Image)
  {
    RDCERR("Can't steal image file handle for non-image RDCFile");
    return NULL;
  }

  filename = m_Filename;

  FILE *ret = m_File;
  m_File = NULL;
  return ret;
}
