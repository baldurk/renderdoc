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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "jpeg-compressor/jpge.h"
#include "replay/replay_controller.h"
#include "serialise/serialiser.h"
#include "stb/stb_image_resize.h"
#include "stb/stb_image_write.h"

static void writeToByteVector(void *context, void *data, int size)
{
  std::vector<byte> *vec = (std::vector<byte> *)context;
  byte *start = (byte *)data;
  byte *end = start + size;
  vec->insert(vec->end(), start, end);
}

class CaptureFile : public ICaptureFile
{
public:
  CaptureFile(const char *f);
  virtual ~CaptureFile() {}
  void Shutdown() { delete this; }
  ReplayStatus OpenStatus() { return m_Status; }
  const char *Filename() { return m_Filename.c_str(); }
  ReplaySupport LocalReplaySupport() { return m_Support; }
  const char *DriverName() { return m_DriverName.c_str(); }
  const char *RecordedMachineIdent() { return m_Ident.c_str(); }
  rdctype::pair<ReplayStatus, IReplayController *> OpenCapture(float *progress);

  rdctype::array<byte> GetThumbnail(FileType type, uint32_t maxsize);

private:
  std::string m_Filename, m_DriverName, m_Ident;
  RDCDriver m_DriverType;
  ReplayStatus m_Status;
  ReplaySupport m_Support;
};

CaptureFile::CaptureFile(const char *f)
{
  m_Filename = f;

  m_DriverType = RDC_Unknown;
  uint64_t fileMachineIdent = 0;
  m_Status = RenderDoc::Inst().FillInitParams(Filename(), m_DriverType, m_DriverName,
                                              fileMachineIdent, NULL);

  if(m_Status != ReplayStatus::Succeeded)
  {
    m_Support = ReplaySupport::Unsupported;
  }
  else
  {
    m_Support = RenderDoc::Inst().HasReplayDriver(m_DriverType) ? ReplaySupport::Supported
                                                                : ReplaySupport::Unsupported;

    if(fileMachineIdent != 0)
    {
      uint64_t machineIdent = OSUtility::GetMachineIdent();

      m_Ident = OSUtility::MakeMachineIdentString(fileMachineIdent);

      if((machineIdent & OSUtility::MachineIdent_OS_Mask) !=
         (fileMachineIdent & OSUtility::MachineIdent_OS_Mask))
        m_Support = ReplaySupport::SuggestRemote;
    }
  }
}

rdctype::pair<ReplayStatus, IReplayController *> CaptureFile::OpenCapture(float *progress)
{
  if(m_Status != ReplayStatus::Succeeded)
    return rdctype::make_pair<ReplayStatus, IReplayController *>(m_Status, NULL);

  ReplayController *render = new ReplayController();
  ReplayStatus ret;

  RenderDoc::Inst().SetProgressPtr(progress);

  ret = render->CreateDevice(Filename());

  RenderDoc::Inst().SetProgressPtr(NULL);

  if(ret != ReplayStatus::Succeeded)
    SAFE_DELETE(render);

  return rdctype::make_pair<ReplayStatus, IReplayController *>(ret, render);
}

rdctype::array<byte> CaptureFile::GetThumbnail(FileType type, uint32_t maxsize)
{
  rdctype::array<byte> buf;

  Serialiser ser(Filename(), Serialiser::READING, false);

  if(ser.HasError())
    return buf;

  ser.Rewind();

  int chunkType = ser.PushContext(NULL, NULL, 1, false);

  if(chunkType != THUMBNAIL_DATA)
    return buf;

  bool HasThumbnail = false;
  ser.Serialise(NULL, HasThumbnail);

  if(!HasThumbnail)
    return buf;

  byte *jpgbuf = NULL;
  size_t thumblen = 0;
  uint32_t thumbwidth = 0, thumbheight = 0;
  {
    ser.Serialise("ThumbWidth", thumbwidth);
    ser.Serialise("ThumbHeight", thumbheight);
    ser.SerialiseBuffer("ThumbnailPixels", jpgbuf, thumblen);
  }

  if(jpgbuf == NULL)
    return buf;

  // if the desired output is jpg and either there's no max size or it's already satisfied,
  // return the data directly
  if(type == FileType::JPG && (maxsize == 0 || (maxsize > thumbwidth && maxsize > thumbheight)))
  {
    create_array_init(buf, thumblen, jpgbuf);
  }
  else
  {
    // otherwise we need to decode, resample maybe, and re-encode

    int w = (int)thumbwidth;
    int h = (int)thumbheight;
    int comp = 3;
    byte *thumbpixels =
        jpgd::decompress_jpeg_image_from_memory(jpgbuf, (int)thumblen, &w, &h, &comp, 3);

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

        free(thumbpixels);

        thumbpixels = resizedpixels;
        thumbwidth = clampedWidth;
        thumbheight = clampedHeight;
      }
    }

    std::vector<byte> encodedBytes;

    switch(type)
    {
      case FileType::JPG:
      {
        int len = thumbwidth * thumbheight * 3;
        encodedBytes.resize(len);
        jpge::params p;
        p.m_quality = 90;
        jpge::compress_image_to_jpeg_file_in_memory(&encodedBytes[0], len, (int)thumbwidth,
                                                    (int)thumbheight, 3, thumbpixels, p);
        encodedBytes.resize(len);
        break;
      }
      case FileType::PNG:
      {
        stbi_write_png_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels, 0);
        break;
      }
      case FileType::TGA:
      {
        stbi_write_tga_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels);
        break;
      }
      case FileType::BMP:
      {
        stbi_write_bmp_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels);
        break;
      }
      default:
      {
        RDCERR("Unsupported file type %d in thumbnail fetch", type);
        free(thumbpixels);
        delete[] jpgbuf;
        return buf;
      }
    }

    buf = encodedBytes;

    free(thumbpixels);
  }

  delete[] jpgbuf;

  return buf;
}

extern "C" RENDERDOC_API ICaptureFile *RENDERDOC_CC RENDERDOC_OpenCaptureFile(const char *logfile)
{
  return new CaptureFile(logfile);
}