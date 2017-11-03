/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "3rdparty/stb/stb_image.h"
#include "api/replay/version.h"
#include "common/dds_readwrite.h"
#include "lz4io.h"
#include "zstdio.h"

const char *SectionTypeNames[] = {
    // unknown
    "",
    // FrameCapture
    "renderdoc/internal/framecapture",
    // ResolveDatabase
    "renderdoc/internal/resolvedb",
    // FrameBookmarks
    "renderdoc/ui/bookmarks",
    // Notes
    "renderdoc/ui/notes",
};

RDCCOMPILE_ASSERT(ARRAY_COUNT(SectionTypeNames) == (size_t)SectionType::Count,
                  "Missing section name");

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
    char ver[] = MAJOR_MINOR_VERSION_STRING;
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
  RDCDriver driverID = RDC_Unknown;
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

#define RETURNCORRUPT(...)             \
  {                                    \
    RDCERR(__VA_ARGS__);               \
    m_Error = ContainerError::Corrupt; \
    return;                            \
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
  RDCLOG("Opening RDCFile %s", path);

  // ensure section header is compiled correctly
  RDCCOMPILE_ASSERT(offsetof(BinarySectionHeader, name) == sizeof(uint32_t) * 10,
                    "BinarySectionHeader size has changed or contains padding");

  m_File = FileIO::fopen(path, "rb");
  m_Filename = path;

  if(!m_File)
  {
    RDCERR("Can't open capture file '%s' for read - errno %d", path, errno);
    m_Error = ContainerError::FileNotFound;
    return;
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
      m_Driver = RDC_Image;
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
    RDCERR("I/O error reading magic number");
    m_Error = ContainerError::FileIO;
    return;
  }

  if(header.magic != MAGIC_HEADER)
  {
    RDCWARN("Invalid capture file. Expected magic %08x, got %08x.", MAGIC_HEADER,
            (uint32_t)header.magic);

    m_Error = ContainerError::Corrupt;
    return;
  }

  m_SerVer = header.version;

  if(m_SerVer != SERIALISE_VERSION)
  {
    RDCERR(
        "Capture file from wrong version. This program (v%s) is logfile version %llu, file is "
        "logfile version %llu capture on %s.",
        SERIALISE_VERSION, header.version, MAJOR_MINOR_VERSION_STRING, header.progVersion);

    m_Error = ContainerError::UnsupportedVersion;
    return;
  }

  BinaryThumbnail thumb;
  reader.Read(&thumb, offsetof(BinaryThumbnail, data));

  if(reader.IsErrored())
  {
    RDCERR("I/O error reading thumbnail header");
    m_Error = ContainerError::FileIO;
    return;
  }

  // check the thumbnail size is sensible
  if(thumb.length > 10 * 1024 * 1024)
  {
    RETURNCORRUPT("Thumbnail byte length invalid: %u", thumb.length);
  }

  byte *thumbData = new byte[thumb.length];
  reader.Read(thumbData, thumb.length);

  if(reader.IsErrored())
  {
    RDCERR("I/O error reading thumbnail data");
    delete[] thumbData;
    m_Error = ContainerError::FileIO;
    return;
  }

  CaptureMetaData meta;
  reader.Read(&meta, offsetof(CaptureMetaData, driverName));

  if(reader.IsErrored())
  {
    RDCERR("I/O error reading capture metadata");
    delete[] thumbData;
    m_Error = ContainerError::FileIO;
    return;
  }

  if(meta.driverNameLength == 0)
  {
    delete[] thumbData;
    RETURNCORRUPT("Driver name length is invalid, must be at least 1 to contain NULL terminator");
  }

  char *driverName = new char[meta.driverNameLength];
  reader.Read(driverName, meta.driverNameLength);

  if(reader.IsErrored())
  {
    RDCERR("I/O error reading driver name");
    delete[] thumbData;
    delete[] driverName;
    m_Error = ContainerError::FileIO;
    return;
  }

  driverName[meta.driverNameLength - 1] = '\0';

  m_Driver = meta.driverID;
  m_DriverName = driverName;
  m_MachineIdent = meta.machineIdent;
  m_Thumb.width = thumb.width;
  m_Thumb.height = thumb.height;
  m_Thumb.len = thumb.length;

  if(m_Thumb.len > 0 && m_Thumb.width > 0 && m_Thumb.height > 0)
  {
    m_Thumb.pixels = thumbData;
    thumbData = NULL;
  }

  delete[] thumbData;
  delete[] driverName;

  if(reader.GetOffset() > header.headerLength)
  {
    RDCERR("I/O error seeking to end of header");
    m_Error = ContainerError::FileIO;
    return;
  }

  reader.SkipBytes(header.headerLength - (uint32_t)reader.GetOffset());

  while(!reader.AtEnd())
  {
    BinarySectionHeader sectionHeader = {0};
    byte *reading = (byte *)&sectionHeader;

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
        RETURNCORRUPT("Invalid ASCII data section '%hhx'", c);

      if(reader.AtEnd())
        RETURNCORRUPT("Invalid truncated ASCII data section");

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
        RETURNCORRUPT("Invalid truncated ASCII data section");

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
        RETURNCORRUPT("Invalid truncated ASCII data section");

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
        RETURNCORRUPT("Invalid truncated ASCII data section");

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
        RETURNCORRUPT("Invalid truncated ASCII data section");

      SectionProperties props;
      props.flags = SectionFlags::ASCIIStored;
      props.type = (SectionType)type;
      props.name = name;
      props.version = version;
      props.compressedSize = length;
      props.uncompressedSize = length;

      SectionLocation loc;
      loc.offs = reader.GetOffset();
      loc.diskLength = length;

      reader.SkipBytes(loc.diskLength);

      if(reader.IsErrored())
        RETURNCORRUPT("Error seeking past ASCII section '%s' data", name.c_str());

      m_Sections.push_back(props);
      m_SectionLocations.push_back(loc);
    }
    else if(sectionHeader.isASCII == 0x0)
    {
      // -1 because we've already read the isASCII byte
      reader.Read(reading, offsetof(BinarySectionHeader, name) - 1);

      if(reader.IsErrored())
        RETURNCORRUPT("Error reading binary section header");

      SectionProperties props;
      props.flags = sectionHeader.sectionFlags;
      props.type = sectionHeader.sectionType;
      props.compressedSize = sectionHeader.sectionCompressedLength;
      props.uncompressedSize = sectionHeader.sectionUncompressedLength;
      props.version = sectionHeader.sectionVersion;

      if(sectionHeader.sectionNameLength == 0 || sectionHeader.sectionNameLength > 2 * 1024)
      {
        RETURNCORRUPT("Invalid section name length %u", sectionHeader.sectionNameLength);
      }

      props.name.resize(sectionHeader.sectionNameLength - 1);

      reader.Read(&props.name[0], sectionHeader.sectionNameLength - 1);

      if(reader.IsErrored())
        RETURNCORRUPT("Error reading binary section header");

      reader.SkipBytes(1);

      if(reader.IsErrored())
        RETURNCORRUPT("Error reading binary section header");

      SectionLocation loc;
      loc.offs = reader.GetOffset();
      loc.diskLength = sectionHeader.sectionCompressedLength;

      m_Sections.push_back(props);
      m_SectionLocations.push_back(loc);

      reader.SkipBytes(loc.diskLength);

      if(reader.IsErrored())
        RETURNCORRUPT("Error seeking past binary section '%s' data", props.name.c_str());
    }
    else
    {
      RETURNCORRUPT("Unrecognised section type '%hhx'", sectionHeader.isASCII);
    }
  }

  if(SectionIndex(SectionType::FrameCapture) == -1)
  {
    RETURNCORRUPT("Capture file doesn't have a frame capture");
  }
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
  m_File = FileIO::fopen(filename, "w+b");

  RDCDEBUG("creating RDC file.");

  if(!m_File)
  {
    RDCERR("Can't open capture file '%s' for write, errno %d", filename, errno);
    m_Error = ContainerError::FileIO;
    return;
  }

  RDCDEBUG("Opened capture file for write");

  FileHeader header;    // automagically initialised with correct data apart from length

  BinaryThumbnail thumbHeader = {0};

  thumbHeader.width = m_Thumb.width;
  thumbHeader.height = m_Thumb.height;
  thumbHeader.length = m_Thumb.len;

  CaptureMetaData meta;
  meta.driverID = m_Driver;
  meta.machineIdent = m_MachineIdent;
  meta.driverNameLength = uint8_t(m_DriverName.size() + 1);

  header.headerLength = sizeof(FileHeader) + offsetof(BinaryThumbnail, data) + thumbHeader.length +
                        offsetof(CaptureMetaData, driverName) + meta.driverNameLength;

  StreamWriter writer(m_File, Ownership::Nothing);

  writer.Write(header);
  writer.Write(&thumbHeader, offsetof(BinaryThumbnail, data));

  if(thumbHeader.length > 0)
    writer.Write(m_Thumb.pixels, thumbHeader.length);

  writer.Write(&meta, offsetof(CaptureMetaData, driverName));

  writer.Write(m_DriverName.c_str(), meta.driverNameLength);

  if(writer.IsErrored())
  {
    RDCERR("Error writing file header");
    m_Error = ContainerError::FileIO;
    return;
  }
}

int RDCFile::SectionIndex(SectionType type) const
{
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
  FileIO::fseek64(m_File, offsetSize.offs, SEEK_SET);

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

  // only handle the case of writing a section that doesn't exist yet.
  // For handling a section that does exist, it depends on the section type:
  // - For frame capture, then we just write to a new file since we want it
  //   to be first. Once the writing is done, copy across any other sections
  //   after it.
  // - For non-frame capture, we remove the existing section and move up any
  //   sections that were after it. Then just return a new writer that appends

  if(props.type != SectionType::Unknown)
  {
    if(SectionIndex(props.type) >= 0)
    {
      RDCERR("Replacing sections is currently not supported.");
      return new StreamWriter(StreamWriter::InvalidStream);
    }
    RDCASSERT((size_t)props.type < (size_t)SectionType::Count);
  }

  if(SectionIndex(props.name.c_str()) >= 0)
  {
    RDCERR("Replacing sections is currently not supported.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

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

  if(m_Sections.empty() && props.type != SectionType::FrameCapture)
  {
    RDCERR("The first section written must be frame capture data.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  if(m_CurrentWritingProps.type != SectionType::Count)
  {
    RDCERR("Only one section can be written at once.");
    return new StreamWriter(StreamWriter::InvalidStream);
  }

  std::string name = props.name;
  SectionType type = props.type;

  // normalise names for known sections
  if(props.type != SectionType::Unknown)
    name = SectionTypeNames[(size_t)type];

  FileIO::fseek64(m_File, 0, SEEK_END);

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
    RDCERR("Error seeking to end of file, errno %d", errno);
    m_Error = ContainerError::FileIO;
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

  m_CurrentWritingProps = props;
  m_CurrentWritingProps.name = name;

  // register a destroy callback to tidy up the section at the end
  fileWriter->AddCloseCallback([this, type, name, headerOffset, fileWriter, compWriter]() {

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

    m_CurrentWritingProps = SectionProperties();

    FileIO::fseek64(m_File, headerOffset + offsetof(BinarySectionHeader, sectionCompressedLength),
                    SEEK_SET);

    size_t bytesWritten = FileIO::fwrite(&compressedLength, 1, sizeof(uint64_t), m_File);
    bytesWritten += FileIO::fwrite(&uncompressedLength, 1, sizeof(uint64_t), m_File);

    if(bytesWritten != 2 * sizeof(uint64_t))
    {
      RDCERR("Error applying fixup to section header, errno %d", errno);
      m_Error = ContainerError::FileIO;
      return;
    }
  });

  // if we're compressing return that writer, otherwise return the file writer directly
  return compWriter ? compWriter : fileWriter;
}

FILE *RDCFile::StealImageFileHandle(std::string &filename)
{
  if(m_Driver != RDC_Image)
  {
    RDCERR("Can't steal image file handle for non-image RDCFile");
    return NULL;
  }

  filename = m_Filename;

  FILE *ret = m_File;
  m_File = NULL;
  return ret;
}
