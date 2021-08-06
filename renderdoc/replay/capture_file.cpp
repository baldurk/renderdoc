/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "jpeg-compressor/jpge.h"
#include "replay/replay_controller.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "stb/stb_image.h"
#include "stb/stb_image_resize.h"
#include "stb/stb_image_write.h"

static void writeToBytebuf(void *context, void *data, int size)
{
  bytebuf *buf = (bytebuf *)context;
  buf->append((byte *)data, size);
}

static RDCDriver driverFromName(const rdcstr &driverName)
{
  for(int d = (int)RDCDriver::Unknown; d < (int)RDCDriver::MaxBuiltin; d++)
  {
    if(driverName == ToStr((RDCDriver)d))
      return (RDCDriver)d;
  }

  return RDCDriver::Unknown;
}

static RDCThumb convertThumb(FileType thumbType, uint32_t thumbWidth, uint32_t thumbHeight,
                             const bytebuf &thumbData)
{
  RDCThumb ret;

  if(thumbWidth > 0xffff || thumbHeight > 0xffff)
    return ret;

  ret.width = thumbWidth & 0xffff;
  ret.height = thumbHeight & 0xffff;

  byte *decoded = NULL;

  if(thumbType == FileType::JPG)
  {
    ret.pixels = thumbData;
  }
  else
  {
    int ignore = 0;
    decoded =
        stbi_load_from_memory(thumbData.data(), thumbData.count(), &ignore, &ignore, &ignore, 3);

    if(decoded == NULL)
    {
      RDCERR("Couldn't decode provided thumbnail");
      return ret;
    }
  }

  if(decoded)
  {
    int len = ret.width * ret.height * 3;
    ret.pixels.resize(len);

    jpge::params p;
    p.m_quality = 90;
    jpge::compress_image_to_jpeg_file_in_memory(ret.pixels.data(), len, (int)ret.width,
                                                (int)ret.height, 3, decoded, p);

    ret.pixels.resize(len);

    free(decoded);
  }

  return ret;
}

class CaptureFile : public ICaptureFile
{
public:
  CaptureFile();
  virtual ~CaptureFile();

  ReplayStatus OpenFile(const rdcstr &filename, const rdcstr &filetype,
                        RENDERDOC_ProgressCallback progress);
  ReplayStatus OpenBuffer(const bytebuf &buffer, const rdcstr &filetype,
                          RENDERDOC_ProgressCallback progress);
  bool CopyFileTo(const rdcstr &filename);
  rdcstr ErrorString() { return m_ErrorString; }
  void Shutdown() { delete this; }
  ReplaySupport LocalReplaySupport() { return m_Support; }
  rdcstr DriverName() { return m_DriverName; }
  rdcstr RecordedMachineIdent() { return m_Ident; }
  uint64_t TimestampBase() { return m_RDC ? m_RDC->GetTimestampBase() : 0; }
  double TimestampFrequency() { return m_RDC ? m_RDC->GetTimestampFrequency() : 1.0; }
  rdcpair<ReplayStatus, IReplayController *> OpenCapture(const ReplayOptions &opts,
                                                         RENDERDOC_ProgressCallback progress);

  void SetMetadata(const rdcstr &driverName, uint64_t machineIdent, FileType thumbType,
                   uint32_t thumbWidth, uint32_t thumbHeight, const bytebuf &thumbData,
                   uint64_t timeBase, double timeFreq);

  ReplayStatus Convert(const rdcstr &filename, const rdcstr &filetype, const SDFile *file,
                       RENDERDOC_ProgressCallback progress);

  rdcarray<CaptureFileFormat> GetCaptureFileFormats()
  {
    return RenderDoc::Inst().GetCaptureFileFormats();
  }

  rdcarray<GPUDevice> GetAvailableGPUs() { return RenderDoc::Inst().GetAvailableGPUs(); }
  const SDFile &GetStructuredData()
  {
    // decompile to structured data on demand.
    InitStructuredData();

    return m_StructuredData;
  }

  void SetStructuredData(const SDFile &file)
  {
    m_StructuredData.version = file.version;

    m_StructuredData.chunks.reserve(file.chunks.size());

    for(SDChunk *obj : file.chunks)
      m_StructuredData.chunks.push_back(obj->Duplicate());

    m_StructuredData.buffers.reserve(file.buffers.size());

    for(bytebuf *buf : file.buffers)
      m_StructuredData.buffers.push_back(new bytebuf(*buf));
  }

  Thumbnail GetThumbnail(FileType type, uint32_t maxsize);

  // ICaptureAccess

  int32_t GetSectionCount();
  int32_t FindSectionByName(const rdcstr &name);
  int32_t FindSectionByType(SectionType type);
  SectionProperties GetSectionProperties(int32_t index);
  bytebuf GetSectionContents(int32_t index);
  bool WriteSection(const SectionProperties &props, const bytebuf &contents);

  bool HasCallstacks();
  bool InitResolver(bool interactive, RENDERDOC_ProgressCallback progress);
  rdcarray<rdcstr> GetResolve(const rdcarray<uint64_t> &callstack);

private:
  ReplayStatus Init();

  void InitStructuredData(RENDERDOC_ProgressCallback progress = RENDERDOC_ProgressCallback());

  RDCFile *m_RDC = NULL;
  Callstack::StackResolver *m_Resolver = NULL;

  SDFile m_StructuredData;

  rdcstr m_DriverName, m_Ident, m_ErrorString;
  ReplaySupport m_Support = ReplaySupport::Unsupported;
};

CaptureFile::CaptureFile()
{
}

CaptureFile::~CaptureFile()
{
  SAFE_DELETE(m_RDC);
  SAFE_DELETE(m_Resolver);
}

ReplayStatus CaptureFile::OpenFile(const rdcstr &filename, const rdcstr &filetype,
                                   RENDERDOC_ProgressCallback progress)
{
  CaptureImporter importer = RenderDoc::Inst().GetCaptureImporter(filetype);

  if(importer)
  {
    ReplayStatus ret;

    {
      StreamReader reader(FileIO::fopen(filename, FileIO::ReadBinary));
      SAFE_DELETE(m_RDC);
      m_RDC = new RDCFile;
      ret = importer(filename, reader, m_RDC, m_StructuredData, progress);
    }

    if(ret != ReplayStatus::Succeeded)
    {
      m_ErrorString = StringFormat::Fmt("Importer '%s' failed to import file.", filetype.c_str());
      SAFE_DELETE(m_RDC);
      return ret;
    }
  }
  else
  {
    if(filetype != "" && filetype != "rdc")
      RDCWARN("Opening file with unrecognised filetype '%s' - treating as 'rdc'", filetype.c_str());

    if(progress)
      progress(0.0f);

    SAFE_DELETE(m_RDC);
    m_RDC = new RDCFile;
    m_RDC->Open(filename);

    if(progress)
      progress(1.0f);
  }

  return Init();
}

ReplayStatus CaptureFile::OpenBuffer(const bytebuf &buffer, const rdcstr &filetype,
                                     RENDERDOC_ProgressCallback progress)
{
  CaptureImporter importer = RenderDoc::Inst().GetCaptureImporter(filetype);

  if(importer)
  {
    ReplayStatus ret;

    {
      StreamReader reader(buffer);
      SAFE_DELETE(m_RDC);
      m_RDC = new RDCFile;
      ret = importer(rdcstr(), reader, m_RDC, m_StructuredData, progress);
    }

    if(ret != ReplayStatus::Succeeded)
    {
      m_ErrorString = StringFormat::Fmt("Importer '%s' failed to import file.", filetype.c_str());
      SAFE_DELETE(m_RDC);
      return ret;
    }
  }
  else
  {
    if(filetype != "" && filetype != "rdc")
      RDCWARN("Opening file with unrecognised filetype '%s' - treating as 'rdc'", filetype.c_str());

    if(progress)
      progress(0.0f);

    SAFE_DELETE(m_RDC);
    m_RDC = new RDCFile;
    m_RDC->Open(buffer);

    if(progress)
      progress(1.0f);
  }

  return Init();
}

bool CaptureFile::CopyFileTo(const rdcstr &filename)
{
  if(m_RDC)
    return m_RDC->CopyFileTo(filename);

  return false;
}

ReplayStatus CaptureFile::Init()
{
  if(!m_RDC)
    return ReplayStatus::InternalError;

  m_ErrorString = m_RDC->ErrorString();

  switch(m_RDC->ErrorCode())
  {
    case ContainerError::FileNotFound: return ReplayStatus::FileNotFound;
    case ContainerError::FileIO: return ReplayStatus::FileIOFailed;
    case ContainerError::Corrupt: return ReplayStatus::FileCorrupted;
    case ContainerError::UnsupportedVersion: return ReplayStatus::FileIncompatibleVersion;
    case ContainerError::NoError:
    {
      RDCDriver driverType = m_RDC->GetDriver();
      m_DriverName = m_RDC->GetDriverName();

      uint64_t fileMachineIdent = m_RDC->GetMachineIdent();

      m_Support = RenderDoc::Inst().HasReplayDriver(driverType) ? ReplaySupport::Supported
                                                                : ReplaySupport::Unsupported;

      if(fileMachineIdent != 0)
      {
        uint64_t machineIdent = OSUtility::GetMachineIdent();

        m_Ident = OSUtility::MakeMachineIdentString(fileMachineIdent);

        if((machineIdent & OSUtility::MachineIdent_OS_Mask) !=
           (fileMachineIdent & OSUtility::MachineIdent_OS_Mask))
          m_Support = ReplaySupport::SuggestRemote;
      }

      // can't open files without a capture in them (except images, which are special)
      if(driverType != RDCDriver::Image && m_RDC->SectionIndex(SectionType::FrameCapture) == -1)
        m_Support = ReplaySupport::Unsupported;

      return ReplayStatus::Succeeded;
    }
  }

  // all container errors should be handled and returned above
  return ReplayStatus::InternalError;
}

void CaptureFile::InitStructuredData(RENDERDOC_ProgressCallback progress /*= RENDERDOC_ProgressCallback()*/)
{
  if(m_StructuredData.chunks.empty() && m_RDC && m_RDC->SectionIndex(SectionType::FrameCapture) >= 0)
  {
    StructuredProcessor proc = RenderDoc::Inst().GetStructuredProcessor(m_RDC->GetDriver());

    RenderDoc::Inst().SetProgressCallback<LoadProgress>(progress);

    if(proc)
      proc(m_RDC, m_StructuredData);
    else
      RDCERR("Can't get structured data for driver %s", m_RDC->GetDriverName().c_str());

    RenderDoc::Inst().SetProgressCallback<LoadProgress>(RENDERDOC_ProgressCallback());
  }
}

rdcpair<ReplayStatus, IReplayController *> CaptureFile::OpenCapture(const ReplayOptions &opts,
                                                                    RENDERDOC_ProgressCallback progress)
{
  if(!m_RDC || m_RDC->ErrorCode() != ContainerError::NoError)
    return rdcpair<ReplayStatus, IReplayController *>(ReplayStatus::InternalError, NULL);

  ReplayController *render = new ReplayController();
  ReplayStatus ret;

  LogReplayOptions(opts);

  RenderDoc::Inst().SetProgressCallback<LoadProgress>(progress);

  ret = render->CreateDevice(m_RDC, opts);

  RenderDoc::Inst().SetProgressCallback<LoadProgress>(RENDERDOC_ProgressCallback());

  if(ret != ReplayStatus::Succeeded)
  {
    render->Shutdown();
    render = NULL;
  }

  return rdcpair<ReplayStatus, IReplayController *>(ret, render);
}

void CaptureFile::SetMetadata(const rdcstr &driverName, uint64_t machineIdent, FileType thumbType,
                              uint32_t thumbWidth, uint32_t thumbHeight, const bytebuf &thumbData,
                              uint64_t timeBase, double timeFreq)
{
  if(m_RDC)
  {
    RDCERR("Cannot set metadata on file that's already opened.");
    return;
  }

  RDCThumb *thumb = NULL;
  RDCThumb th;

  if(!thumbData.empty())
  {
    th = convertThumb(thumbType, thumbWidth, thumbHeight, thumbData);
    thumb = &th;
  }

  RDCDriver driver = driverFromName(driverName);

  if(driver == RDCDriver::Unknown)
  {
    RDCERR("Unrecognised driver name '%s'.", driverName.c_str());
    return;
  }

  m_RDC = new RDCFile;
  m_RDC->SetData(driver, driverName, machineIdent, thumb, timeBase, timeFreq);
}

ReplayStatus CaptureFile::Convert(const rdcstr &filename, const rdcstr &filetype,
                                  const SDFile *file, RENDERDOC_ProgressCallback progress)
{
  if(!m_RDC)
  {
    RDCERR("Data missing for creation of file, set metadata first.");
    return ReplayStatus::FileCorrupted;
  }

  // make sure progress is valid so we don't have to check it everywhere
  if(!progress)
    progress = [](float) {};

  // we have two separate steps that can take time - fetching the structured data, and then
  // exporting or writing to RDC
  RENDERDOC_ProgressCallback fetchProgress = [progress](float p) { progress(p * 0.5f); };
  RENDERDOC_ProgressCallback exportProgress = [progress](float p) { progress(0.5f + p * 0.5f); };

  CaptureExporter exporter = RenderDoc::Inst().GetCaptureExporter(filetype);

  if(exporter)
  {
    if(file)
    {
      return exporter(filename, *m_RDC, *file, exportProgress);
    }
    else
    {
      InitStructuredData(fetchProgress);

      return exporter(filename, *m_RDC, GetStructuredData(), exportProgress);
    }
  }

  if(filetype != "" && filetype != "rdc")
    RDCWARN("Converting file to unrecognised filetype '%s' - treating as 'rdc'", filetype.c_str());

  RDCFile output;

  output.SetData(m_RDC->GetDriver(), m_RDC->GetDriverName(), m_RDC->GetMachineIdent(),
                 &m_RDC->GetThumbnail(), m_RDC->GetTimestampBase(), m_RDC->GetTimestampFrequency());

  output.Create(filename);

  if(output.ErrorCode() != ContainerError::NoError)
  {
    switch(output.ErrorCode())
    {
      case ContainerError::FileNotFound: return ReplayStatus::FileNotFound;
      case ContainerError::FileIO: return ReplayStatus::FileIOFailed;
      default: break;
    }
    return ReplayStatus::InternalError;
  }

  bool success = true;

  // when we don't have a frame capture section, write it from the structured data.
  int frameCaptureIndex = m_RDC->SectionIndex(SectionType::FrameCapture);

  if(frameCaptureIndex == -1)
  {
    if(file == NULL)
    {
      InitStructuredData(fetchProgress);
      file = &m_StructuredData;
    }

    SectionProperties frameCapture;
    frameCapture.flags = SectionFlags::ZstdCompressed;
    frameCapture.type = SectionType::FrameCapture;
    frameCapture.name = ToStr(frameCapture.type);
    frameCapture.version = file->version;

    StreamWriter *writer = output.WriteSection(frameCapture);

    WriteSerialiser ser(writer, Ownership::Nothing);

    ser.WriteStructuredFile(*file, exportProgress);

    writer->Finish();

    success = success && !writer->IsErrored();

    delete writer;
  }
  else
  {
    // otherwise write it straight, but compress it to zstd
    SectionProperties props = m_RDC->GetSectionProperties(frameCaptureIndex);
    props.flags = SectionFlags::ZstdCompressed;

    StreamWriter *writer = output.WriteSection(props);
    StreamReader *reader = m_RDC->ReadSection(frameCaptureIndex);

    StreamTransfer(writer, reader, progress);

    writer->Finish();

    success = success && !writer->IsErrored() && !reader->IsErrored();

    delete reader;
    delete writer;
  }

  if(!success)
    return ReplayStatus::FileIOFailed;

  // write all other sections
  for(int i = 0; i < m_RDC->NumSections(); i++)
  {
    const SectionProperties &props = m_RDC->GetSectionProperties(i);

    if(props.type == SectionType::FrameCapture)
      continue;

    StreamWriter *writer = output.WriteSection(props);
    StreamReader *reader = m_RDC->ReadSection(i);

    StreamTransfer(writer, reader, NULL);

    writer->Finish();

    success = success && !writer->IsErrored() && !reader->IsErrored();

    delete reader;
    delete writer;

    if(!success)
      return ReplayStatus::FileIOFailed;
  }

  return ReplayStatus::Succeeded;
}

Thumbnail CaptureFile::GetThumbnail(FileType type, uint32_t maxsize)
{
  Thumbnail ret;
  ret.type = type;

  if(m_RDC == NULL)
    return ret;

  const RDCThumb &thumb = m_RDC->GetThumbnail();

  uint32_t thumbwidth = thumb.width, thumbheight = thumb.height;

  if(thumb.pixels.empty())
    return ret;

  bytebuf buf;

  // if the desired output is the format of stored thumbnail and either there's no max size or it's
  // already satisfied, return the data directly
  if(type == thumb.format && (maxsize == 0 || (maxsize > thumbwidth && maxsize > thumbheight)))
  {
    buf = thumb.pixels;
  }
  else
  {
    // otherwise we need to decode, resample maybe, and re-encode

    int w = (int)thumbwidth;
    int h = (int)thumbheight;
    int comp = 3;
    const byte *thumbpixels = NULL;
    byte *allocatedBuffer = NULL;
    switch(thumb.format)
    {
      case FileType::JPG:
        allocatedBuffer = jpgd::decompress_jpeg_image_from_memory(
            thumb.pixels.data(), (int)thumb.pixels.size(), &w, &h, &comp, 3);
        thumbpixels = allocatedBuffer;
        break;

      case FileType::Raw: thumbpixels = thumb.pixels.data(); break;

      default:
        allocatedBuffer =
            stbi_load_from_memory(thumb.pixels.data(), (int)thumb.pixels.size(), &w, &h, &comp, 3);
        if(allocatedBuffer == NULL)
        {
          RDCERR("Couldn't decode provided thumbnail");
          return ret;
        }
        thumbpixels = allocatedBuffer;
        break;
    }

    if(maxsize != 0)
    {
      uint32_t clampedWidth = RDCMIN(maxsize, thumbwidth);
      uint32_t clampedHeight = RDCMIN(maxsize, thumbheight);

      if(clampedWidth != thumbwidth || clampedHeight != thumbheight)
      {
        // preserve aspect ratio, take the smallest scale factor and multiply both
        float scaleX = float(clampedWidth) / float(thumbwidth);
        float scaleY = float(clampedHeight) / float(thumbheight);

        if(scaleX < scaleY)
          clampedHeight = uint32_t(scaleX * thumbheight);
        else if(scaleY < scaleX)
          clampedWidth = uint32_t(scaleY * thumbwidth);

        byte *resizedpixels = (byte *)malloc(3 * clampedWidth * clampedHeight);

        stbir_resize_uint8_srgb(thumbpixels, thumbwidth, thumbheight, 0, resizedpixels,
                                clampedWidth, clampedHeight, 0, 3, -1, 0);

        free(allocatedBuffer);

        allocatedBuffer = resizedpixels;
        thumbpixels = resizedpixels;
        thumbwidth = clampedWidth;
        thumbheight = clampedHeight;
      }
    }

    switch(type)
    {
      case FileType::Raw:
      {
        buf.assign(thumbpixels, thumbwidth * thumbheight * 3);
        break;
      }
      case FileType::JPG:
      {
        int len = thumbwidth * thumbheight * 3;
        buf.resize(len);
        jpge::params p;
        p.m_quality = 90;
        jpge::compress_image_to_jpeg_file_in_memory(buf.data(), len, (int)thumbwidth,
                                                    (int)thumbheight, 3, thumbpixels, p);
        buf.resize(len);
        break;
      }
      case FileType::PNG:
      {
        stbi_write_png_to_func(&writeToBytebuf, &buf, (int)thumbwidth, (int)thumbheight, 3,
                               thumbpixels, 0);
        break;
      }
      case FileType::TGA:
      {
        stbi_write_tga_to_func(&writeToBytebuf, &buf, (int)thumbwidth, (int)thumbheight, 3,
                               thumbpixels);
        break;
      }
      case FileType::BMP:
      {
        stbi_write_bmp_to_func(&writeToBytebuf, &buf, (int)thumbwidth, (int)thumbheight, 3,
                               thumbpixels);
        break;
      }
      default:
      {
        RDCERR("Unsupported file type %d in thumbnail fetch", type);
        free(allocatedBuffer);
        ret.width = 0;
        ret.height = 0;
        return ret;
      }
    }

    free(allocatedBuffer);
  }

  ret.data.swap(buf);
  ret.width = thumbwidth;
  ret.height = thumbheight;

  return ret;
}

int32_t CaptureFile::GetSectionCount()
{
  if(!m_RDC)
    return 0;

  return m_RDC->NumSections();
}

int32_t CaptureFile::FindSectionByName(const rdcstr &name)
{
  if(!m_RDC)
    return -1;

  return m_RDC->SectionIndex(name);
}

int32_t CaptureFile::FindSectionByType(SectionType type)
{
  if(!m_RDC)
    return -1;

  return m_RDC->SectionIndex(type);
}

SectionProperties CaptureFile::GetSectionProperties(int32_t index)
{
  if(!m_RDC || index < 0 || index >= m_RDC->NumSections())
    return SectionProperties();

  return m_RDC->GetSectionProperties(index);
}

bytebuf CaptureFile::GetSectionContents(int32_t index)
{
  bytebuf ret;

  if(!m_RDC || index < 0 || index >= m_RDC->NumSections())
    return ret;

  StreamReader *reader = m_RDC->ReadSection(index);

  ret.resize((size_t)reader->GetSize());
  bool success = reader->Read(ret.data(), reader->GetSize());

  delete reader;

  if(!success)
    ret.clear();

  return ret;
}

bool CaptureFile::WriteSection(const SectionProperties &props, const bytebuf &contents)
{
  StreamWriter *writer = m_RDC->WriteSection(props);
  if(!writer)
    return false;

  writer->Write(contents.data(), contents.size());

  writer->Finish();

  delete writer;

  return true;
}

bool CaptureFile::HasCallstacks()
{
  return m_RDC && m_RDC->SectionIndex(SectionType::ResolveDatabase) >= 0;
}

bool CaptureFile::InitResolver(bool interactive, RENDERDOC_ProgressCallback progress)
{
  if(!HasCallstacks())
  {
    RDCERR("Capture has no callstacks - can't initialise resolver.");
    return false;
  }

  if(progress)
    progress(0.001f);

  int idx = m_RDC->SectionIndex(SectionType::ResolveDatabase);

  if(idx < 0)
    return false;

  StreamReader *reader = m_RDC->ReadSection(idx);

  bytebuf buf;
  buf.resize((size_t)reader->GetSize());
  bool success = reader->Read(buf.data(), reader->GetSize());

  delete reader;

  if(!success)
  {
    RDCERR("Failed to read resolve database.");
    return false;
  }

  if(progress)
    progress(0.002f);

  m_Resolver = Callstack::MakeResolver(interactive, buf.data(), buf.size(), progress);

  if(!m_Resolver)
  {
    RDCERR("Couldn't create callstack resolver - capture possibly from another platform.");
    return false;
  }

  return true;
}

rdcarray<rdcstr> CaptureFile::GetResolve(const rdcarray<uint64_t> &callstack)
{
  rdcarray<rdcstr> ret;

  if(callstack.empty())
    return ret;

  if(!m_Resolver)
  {
    ret = {""};
    return ret;
  }

  ret.reserve(callstack.size());
  for(uint64_t frame : callstack)
  {
    Callstack::AddressDetails info = m_Resolver->GetAddr(frame);
    ret.push_back(info.formattedString());
  }

  return ret;
}

extern "C" RENDERDOC_API ICaptureFile *RENDERDOC_CC RENDERDOC_OpenCaptureFile()
{
  return new CaptureFile();
}
