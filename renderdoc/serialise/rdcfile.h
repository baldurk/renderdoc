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

#pragma once

#include "core/core.h"
#include "streamio.h"

enum class ContainerError
{
  NoError = 0,
  FileNotFound,
  FileIO,
  Corrupt,
  UnsupportedVersion,
};

extern const char *SectionTypeNames[];

struct RDCThumb
{
  const byte *pixels = NULL;
  uint32_t len = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  FileType format = FileType::JPG;
};

struct ExtThumbnailHeader
{
  uint16_t width;
  uint16_t height;
  uint32_t len;
  FileType format;
};

class RDCFile
{
public:
  // version number of overall file format or chunk organisation. If the contents/meaning/order of
  // chunks have changed this does not need to be bumped, there are version numbers within each
  // API that interprets the stream that can be bumped.
  static const uint32_t SERIALISE_VERSION = 0x00000101;

  // this must never be changed - files before this were in the v0.x series and didn't have embedded
  // version numbers
  static const uint32_t V1_0_VERSION = 0x00000100;
  static const uint32_t V1_1_VERSION = 0x00000101;

  ~RDCFile();

  // opens an existing file for read and/or modification. Error if file doesn't exist
  void Open(const char *filename);
  void Open(const std::vector<byte> &buffer);

  bool CopyFileTo(const char *filename);

  // Sets the parameters of an RDCFile in memory.
  void SetData(RDCDriver driver, const char *driverName, uint64_t machineIdent,
               const RDCThumb *thumb);

  // creates a new file with current properties, file will be overwritten if it already exists
  void Create(const char *filename);

  ContainerError ErrorCode() const { return m_Error; }
  std::string ErrorString() const { return m_ErrorString; }
  RDCDriver GetDriver() const { return m_Driver; }
  const std::string &GetDriverName() const { return m_DriverName; }
  uint64_t GetMachineIdent() const { return m_MachineIdent; }
  const RDCThumb &GetThumbnail() const { return m_Thumb; }
  int SectionIndex(SectionType type) const;
  int SectionIndex(const char *name) const;
  int NumSections() const { return int(m_Sections.size()); }
  const SectionProperties &GetSectionProperties(int index) const { return m_Sections[index]; }
  StreamReader *ReadSection(int index) const;
  StreamWriter *WriteSection(const SectionProperties &props);

  // Only valid if GetDriver returns RDCDriver::Image, passes over the underlying FILE * for use
  // loading the image directly, since the RDC container isn't there to read from a section.
  FILE *StealImageFileHandle(std::string &filename);

private:
  void Init(StreamReader &reader);

  FILE *m_File = NULL;
  std::string m_Filename;
  std::vector<byte> m_Buffer;

  SectionProperties m_CurrentWritingProps;

  uint32_t m_SerVer = 0;

  RDCDriver m_Driver = RDCDriver::Unknown;
  std::string m_DriverName;
  uint64_t m_MachineIdent = 0;
  RDCThumb m_Thumb;

  ContainerError m_Error = ContainerError::NoError;
  std::string m_ErrorString;

  struct SectionLocation
  {
    uint64_t headerOffset;
    uint64_t dataOffset;
    uint64_t diskLength;
  };

  std::vector<SectionProperties> m_Sections;
  std::vector<SectionLocation> m_SectionLocations;
  std::vector<std::vector<byte>> m_MemorySections;
};
