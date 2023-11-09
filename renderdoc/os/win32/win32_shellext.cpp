/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

// in this file we define the shell extension that renderdoc sets up to be able to
// display thumbnails of captures in windows explorer. We register as a thumbnail
// provider and either the installer or the UI installs the appropriate registry keys.

#include <thumbcache.h>
#include <windows.h>
#include "common/common.h"
#include "common/dds_readwrite.h"
#include "compressonator/CMP_Core.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "lz4/lz4.h"
#include "maths/formatpacking.h"
#include "maths/half_convert.h"
#include "serialise/rdcfile.h"

#include "stb/stb_image_resize2.h"

// {5D6BF029-A6BA-417A-8523-120492B1DCE3}
static const GUID CLSID_RDCThumbnailProvider = {0x5d6bf029,
                                                0xa6ba,
                                                0x417a,
                                                {0x85, 0x23, 0x12, 0x4, 0x92, 0xb1, 0xdc, 0xe3}};

unsigned int numProviders = 0;

struct RDCThumbnailProvider : public IThumbnailProvider, IInitializeWithStream
{
  unsigned int m_iRefcount;
  bool m_Inited;
  RDCThumb m_Thumb;
  read_dds_data m_ddsData;

  RDCThumbnailProvider() : m_iRefcount(1), m_Inited(false) { InterlockedIncrement(&numProviders); }
  virtual ~RDCThumbnailProvider() { InterlockedDecrement(&numProviders); }
  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0)
      delete this;
    return ret;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == CLSID_RDCThumbnailProvider)
    {
      *ppvObject = (void *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (void *)(IUnknown *)(IThumbnailProvider *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(IThumbnailProvider))
    {
      *ppvObject = (void *)(IThumbnailProvider *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(IInitializeWithStream))
    {
      *ppvObject = (void *)(IInitializeWithStream *)this;
      AddRef();
      return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
  }

  virtual HRESULT STDMETHODCALLTYPE Initialize(IStream *pstream, DWORD grfMode)
  {
    if(m_Inited)
      return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    byte *buf = new byte[2 * 1024 * 1024 + 1];
    ULONG numRead = 0;
    HRESULT hr = pstream->Read(buf, 2 * 1024 * 1024, &numRead);

    if(hr != S_OK && hr != S_FALSE)
    {
      delete[] buf;
      return E_INVALIDARG;
    }

    RDCDEBUG("RDCThumbnailProvider Initialize read %d bytes from file", numRead);

    bytebuf captureHeader(buf, numRead);

    delete[] buf;

    if(is_dds_file(captureHeader.data(), numRead))
    {
      STATSTG stats = {};
      pstream->Stat(&stats, STATFLAG_DEFAULT);
      const uint32_t size = (uint32_t)stats.cbSize.QuadPart;
      const size_t offset = captureHeader.size();
      ULONG ddsNumRead = 0;

      // read rest of file
      if(size > captureHeader.size())
      {
        captureHeader.resize(size);
        pstream->Read(captureHeader.begin() + offset, (ULONG)(size - offset), &ddsNumRead);
      }

      StreamReader reader(captureHeader.data(), (ULONG)size);
      m_ddsData = {};
      RDResult res = load_dds_from_file(&reader, m_ddsData);

      if(res != ResultCode::Succeeded)
      {
        return E_INVALIDARG;
      }
      // Ignore volume slices and mip maps. We want to convert the first slice of the image as
      // bitmap.
      m_Thumb.height = (uint16_t)m_ddsData.height;
      m_Thumb.width = (uint16_t)m_ddsData.width;

      // size of slice 0
      size_t len = m_ddsData.subresources[0].second;
      m_Thumb.pixels.resize(len);
      // slice 0 data
      memcpy(m_Thumb.pixels.data(), m_ddsData.buffer.data() + m_ddsData.subresources[0].first, len);
      m_Thumb.format = FileType::DDS;
    }
    else
    {
      RDCFile rdc;
      rdc.Open(captureHeader);

      m_Thumb = rdc.GetThumbnail();

      // we don't care about the error code (which would come from the truncated file), we just care
      // if we got the thumbnail
      if(m_Thumb.pixels.empty() || m_Thumb.width == 0 || m_Thumb.height == 0)
      {
        ReadLegacyCaptureThumb(captureHeader);
      }
    }

    m_Inited = true;

    return S_OK;
  }

  void STDMETHODCALLTYPE ReadLegacyCaptureThumb(bytebuf &captureHeader)
  {
    // we want to support old capture files, so we decode the thumbnail by hand here with the
    // old header.
    const uint32_t MAGIC_HEADER = MAKE_FOURCC('R', 'D', 'O', 'C');

    byte *readPtr = captureHeader.data();
    byte *readEnd = readPtr + captureHeader.size();

    if(captureHeader.size() < sizeof(MAGIC_HEADER) ||
       memcmp(&MAGIC_HEADER, readPtr, sizeof(MAGIC_HEADER)) != 0)
    {
      RDCDEBUG("Legacy header did not have expected magic number");
      return;
    }

    // uint64_t MAGIC_HEADER
    readPtr += sizeof(uint64_t);

    if(readPtr + sizeof(uint32_t) >= readEnd)
      return;

    uint32_t version = 0;
    memcpy(&version, readPtr, sizeof(version));

    // uint64_t version
    readPtr += sizeof(uint64_t);

    if(version == 0x31)
    {
      readPtr += sizeof(uint64_t);    // uint64_t filesize

      if(readPtr + sizeof(uint64_t) >= readEnd)
        return;

      uint64_t resolveDBSize = 0;
      memcpy(&resolveDBSize, readPtr, sizeof(resolveDBSize));
      readPtr += sizeof(resolveDBSize);

      if(resolveDBSize > 0)
      {
        readPtr += resolveDBSize;
        readPtr = (byte *)AlignUp<uintptr_t>((uintptr_t)readPtr, 16);
      }

      // now readPtr points to data

      if(readPtr >= readEnd)
        return;
    }
    else if(version == 0x32)
    {
      if(readPtr >= readEnd)
        return;

      // only support a binary capture section as the first section
      if(*readPtr != '0')
      {
        RDCDEBUG("Unsupported IsASCII value %x", (uint32_t)*readPtr);
        return;
      }

      readPtr += sizeof(byte) * 4;    // isASCII and 3 padding bytes

      if(readPtr + sizeof(uint32_t) >= readEnd)
        return;

      uint32_t sectionFlags = 0;
      memcpy(&sectionFlags, readPtr, sizeof(sectionFlags));
      readPtr += sizeof(sectionFlags);

      readPtr += sizeof(uint32_t);    // uint32_t sectionType
      readPtr += sizeof(uint32_t);    // uint32_t sectionLength

      if(readPtr + sizeof(uint32_t) >= readEnd)
        return;

      uint32_t sectionNameLength = 0;
      memcpy(&sectionNameLength, readPtr, sizeof(sectionNameLength));
      readPtr += sizeof(sectionNameLength);

      readPtr += sectionNameLength;

      // eSectionFlag_LZ4Compressed
      if(sectionFlags & 0x2)
      {
        bytebuf uncompressed;

        LZ4_streamDecode_t lZ4Decomp = {};
        LZ4_setStreamDecode(&lZ4Decomp, NULL, 0);

        // decompress all the complete blocks we have
        while(readPtr < readEnd)
        {
          int32_t compSize = 0;
          memcpy(&compSize, readPtr, sizeof(compSize));
          readPtr += sizeof(compSize);

          // break if this block is not complete, as we should have enough by now.
          if(readPtr + compSize > readEnd)
            break;

          size_t off = uncompressed.size();
          const size_t BlockSize = 64 * 1024;
          uncompressed.resize(off + BlockSize);

          int32_t decompSize = LZ4_decompress_safe_continue(
              &lZ4Decomp, (const char *)readPtr, (char *)&uncompressed[off], compSize, BlockSize);

          readPtr += compSize;

          uncompressed.resize(off + decompSize);
        }

        captureHeader = uncompressed;

        readPtr = captureHeader.data();
        readEnd = readPtr + captureHeader.size();
      }
    }
    else
    {
      RDCDEBUG("Unsupported legacy version %x", version);
      return;
    }

    byte *dataStart = readPtr;

    // now we're at the first chunk. It should be THUMBNAIL_DATA
    const uint16_t THUMBNAIL_DATA = 2;

    if(readPtr + sizeof(uint16_t) >= readEnd)
      return;

    uint16_t chunkID = 0;
    memcpy(&chunkID, readPtr, sizeof(chunkID));
    readPtr += sizeof(chunkID);

    if((chunkID & 0x3fff) != THUMBNAIL_DATA)
    {
      RDCDEBUG("Unsupported chunk type %hu", chunkID);
      return;
    }

    readPtr += sizeof(uint32_t);    // uint32_t chunkSize

    if(readPtr + sizeof(bool) + sizeof(uint32_t) * 3 >= readEnd)
      return;

    // contents we care about
    bool hasThumbnail = false;
    memcpy(&hasThumbnail, readPtr, sizeof(hasThumbnail));
    readPtr += sizeof(hasThumbnail);

    if(!hasThumbnail)
    {
      RDCDEBUG("File does not have thumbnail");
      return;
    }

    uint32_t thumbWidth = 0;
    memcpy(&thumbWidth, readPtr, sizeof(thumbWidth));
    readPtr += sizeof(thumbWidth);

    uint32_t thumbHeight = 0;
    memcpy(&thumbHeight, readPtr, sizeof(thumbHeight));
    readPtr += sizeof(thumbHeight);

    uint32_t thumbLen = 0;
    memcpy(&thumbLen, readPtr, sizeof(thumbLen));
    readPtr += sizeof(thumbLen);

    // serialise version 0x00000031 had only 16-byte alignment
    const size_t BufferAlignment = (version == 0x31) ? 16 : 64;

    // buffer follows. First we need to align relative to the start of the data
    size_t offs = readPtr - dataStart;
    size_t alignedOffs = AlignUp(offs, BufferAlignment);

    readPtr += (alignedOffs - offs);

    if(uint32_t(readEnd - readPtr) >= thumbLen)
    {
      RDCDEBUG("Got %ux%u thumbnail, %u pixels", thumbWidth, thumbHeight, thumbLen);

      m_Thumb.pixels.resize(thumbLen);
      memcpy(m_Thumb.pixels.data(), readPtr, thumbLen);

      m_Thumb.width = (uint16_t)thumbWidth;
      m_Thumb.height = (uint16_t)thumbHeight;
    }
    else
    {
      RDCDEBUG("Thumbnail length %u is impossible or truncated with %llu remaining bytes", thumbLen,
               uint64_t(readEnd - readPtr));
    }
  }

  virtual HRESULT STDMETHODCALLTYPE GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
  {
    RDCDEBUG("RDCThumbnailProvider GetThumbnail %d", cx);

    if(!m_Inited)
    {
      RDCERR("Not initialized");
      return E_NOTIMPL;
    }

    if(m_Thumb.pixels.empty())
    {
      RDCERR("Problem opening file");
      return E_NOTIMPL;
    }

    uint32_t thumbwidth = m_Thumb.width, thumbheight = m_Thumb.height;
    byte *thumbpixels = NULL;

    if(m_Thumb.format == FileType::JPG)
    {
      int w = thumbwidth;
      int h = thumbheight;
      int comp = 3;
      thumbpixels = jpgd::decompress_jpeg_image_from_memory(
          m_Thumb.pixels.data(), (int)m_Thumb.pixels.size(), &w, &h, &comp, 3);
    }
    else
    {
      thumbwidth = m_ddsData.width;
      thumbheight = m_ddsData.height;
      const ResourceFormatType resourceType = m_ddsData.format.type;
      const uint32_t decompressedStride = AlignUp4(thumbwidth) * 4;
      const uint32_t decompressedBlockWidth = 16;    // in bytes (4 byte/pixel)
      const uint32_t decompressedBlockMaxSize = 64;
      const uint32_t compressedBlockSize = m_ddsData.format.ElementSize();

      bool blockCompressed = false;

      // check supported formats
      switch(resourceType)
      {
        case ResourceFormatType::BC1:
        case ResourceFormatType::BC2:
        case ResourceFormatType::BC3:
        case ResourceFormatType::BC4:
        case ResourceFormatType::BC5:
        case ResourceFormatType::BC6:
        case ResourceFormatType::BC7: blockCompressed = true; break;
        case ResourceFormatType::Regular:
        case ResourceFormatType::D32S8:
        case ResourceFormatType::D24S8:
        case ResourceFormatType::D16S8:
        case ResourceFormatType::A8:
        case ResourceFormatType::R10G10B10A2:
        case ResourceFormatType::R11G11B10:
        case ResourceFormatType::R9G9B9E5:
        case ResourceFormatType::R4G4B4A4:
        case ResourceFormatType::R5G6B5:
        case ResourceFormatType::R5G5B5A1: blockCompressed = false; break;
        default:
          // not supported
          return E_NOTIMPL;
      }

      // Decompressed DDS, 3 byte/pixel without alpha
      thumbpixels = (byte *)malloc(AlignUp4(thumbheight) * AlignUp4(thumbwidth) * 3);

      if(blockCompressed)
      {
        bytebuf decompressed;    // Decompressed DDS, 4 byte/pixel
        decompressed.resize(AlignUp4(thumbheight) * AlignUp4(thumbwidth) * 4);
        unsigned char decompressedBlock[decompressedBlockMaxSize];
        unsigned char greenBlock[16];
        uint16_t decompressedBC6[48];
        const byte *compBlockStart = m_Thumb.pixels.data();

        for(uint32_t blockY = 0; blockY < AlignUp4(thumbheight) / 4; blockY++)
        {
          for(uint32_t blockX = 0; blockX < AlignUp4(thumbwidth) / 4; blockX++)
          {
            uint32_t decompressedBlockStart =
                blockY * 4 * decompressedStride + blockX * decompressedBlockWidth;
            switch(resourceType)
            {
              case ResourceFormatType::BC1:
                DecompressBlockBC1(compBlockStart, decompressedBlock, NULL);
                break;
              case ResourceFormatType::BC2:
                DecompressBlockBC2(compBlockStart, decompressedBlock, NULL);
                break;
              case ResourceFormatType::BC3:
                DecompressBlockBC3(compBlockStart, decompressedBlock, NULL);
                break;
              case ResourceFormatType::BC4:
                DecompressBlockBC4(compBlockStart, decompressedBlock, NULL);
                break;
              case ResourceFormatType::BC5:
                DecompressBlockBC5(compBlockStart, decompressedBlock, greenBlock, NULL);
                break;
              case ResourceFormatType::BC6:
                // Compressonator handles UF16/SF16 signed/unsigned cases. Returns signed half-float
                DecompressBlockBC6(compBlockStart, decompressedBC6, NULL);
                break;
              case ResourceFormatType::BC7:
                DecompressBlockBC7(compBlockStart, decompressedBlock, NULL);
                break;
              default: return E_NOTIMPL;    // other formats
            }

            unsigned char *decompPointer = decompressedBlock;
            unsigned char *decompImagePointer = decompressed.data() + decompressedBlockStart;
            for(int i = 0; i < 4; i++)
            {
              switch(resourceType)
              {
                case ResourceFormatType::BC1:
                case ResourceFormatType::BC2:
                case ResourceFormatType::BC3:
                case ResourceFormatType::BC7:
                  memcpy(decompImagePointer, decompPointer,
                         16);    // copy one stride of the decompressed Block
                  decompPointer += 16;
                  break;
                case ResourceFormatType::BC4:    // copy the color of the red channel into rgb
                                                 // channels.
                  for(int pixelInStride = 0; pixelInStride < 4; pixelInStride++)
                  {
                    decompImagePointer[pixelInStride * 4] = decompPointer[pixelInStride];
                    decompImagePointer[(pixelInStride * 4) + 1] = decompPointer[pixelInStride];
                    decompImagePointer[(pixelInStride * 4) + 2] = decompPointer[pixelInStride];
                  }
                  decompPointer += 4;
                  break;
                case ResourceFormatType::BC5:    // copy red and green channels.
                  for(int pixelInStride = 0; pixelInStride < 4; pixelInStride++)
                  {
                    decompImagePointer[pixelInStride * 4] =
                        decompressedBlock[pixelInStride + (i * 4)];    // copy red channel
                    decompImagePointer[(pixelInStride * 4) + 1] =
                        greenBlock[pixelInStride + (i * 4)];    // copy green channel
                  }
                  break;
                case ResourceFormatType::BC6:
                  for(int pixelInStride = 0; pixelInStride < 4; pixelInStride++)
                  {
                    // compute Pixel Index for the decompressedBC6 buffer. One block stide = 12
                    // floats.
                    int pixelIndex = pixelInStride * 3 + (i * 12);
                    // decompressed BC6 block uses 16:16:16 color format. Convert to 8:8:8:8
                    uint16_t r = decompressedBC6[pixelIndex];
                    uint16_t g = decompressedBC6[pixelIndex + 1];
                    uint16_t b = decompressedBC6[pixelIndex + 2];
                    // convert to half float
                    float redF = ConvertFromHalf(r);
                    float greenF = ConvertFromHalf(g);
                    float blueF = ConvertFromHalf(b);
                    // clamp to 0..1
                    redF = RDCCLAMP(redF, 0.0f, 1.0f);
                    greenF = RDCCLAMP(greenF, 0.0f, 1.0f);
                    blueF = RDCCLAMP(blueF, 0.0f, 1.0f);
                    // scale to 0..255
                    decompImagePointer[pixelInStride * 4] = (unsigned char)(redF * 255);
                    decompImagePointer[(pixelInStride * 4) + 1] = (unsigned char)(greenF * 255);
                    decompImagePointer[(pixelInStride * 4) + 2] = (unsigned char)(blueF * 255);
                  }
                  break;
                default: return E_NOTIMPL;    // other formats
              }
              decompImagePointer += decompressedStride;
            }
            compBlockStart += compressedBlockSize;
          }
        }

        byte *decompRead = decompressed.data();
        byte *imgWrite = thumbpixels;
        // Iterate over pixels (4byte/pixel in decompressed, 3byte/pixel in thumbpixels)
        for(uint32_t y = 0; y < thumbheight; y++)
        {
          for(uint32_t x = 0; x < thumbwidth; x++)
          {
            memcpy(imgWrite + (thumbwidth * y + x) * 3, decompRead + decompressedStride * y + x * 4,
                   3);
          }
        }
      }
      else
      {
        // read data as non-compressed
        const byte *src = m_Thumb.pixels.data();
        byte *dst = thumbpixels;

        uint32_t texelSize = m_ddsData.format.ElementSize();

        // account for padding
        if(resourceType == ResourceFormatType::D32S8)
          texelSize = 8;
        else if(resourceType == ResourceFormatType::D16S8)
          texelSize = 4;

        for(uint32_t y = 0; y < thumbheight; y++)
        {
          for(uint32_t x = 0; x < thumbwidth; x++)
          {
            FloatVector rgba;

            if(resourceType == ResourceFormatType::D32S8)
              rgba.x = rgba.y = rgba.z = *(float *)src;
            else if(resourceType == ResourceFormatType::D24S8)
              rgba.x = rgba.y = rgba.z = float((*(uint32_t *)src) >> 8) / 16777215.0f;
            else if(resourceType == ResourceFormatType::D16S8)
              rgba.x = rgba.y = rgba.z = float(*(uint16_t *)src) / 65535.0f;
            else
              rgba = DecodeFormattedComponents(m_ddsData.format, src);

            if(resourceType == ResourceFormatType::A8)
              rgba.y = rgba.z = rgba.x;

            dst[0] = byte(RDCCLAMP(rgba.x, 0.0f, 1.0f) * 255.0f);
            dst[1] = byte(RDCCLAMP(rgba.y, 0.0f, 1.0f) * 255.0f);
            dst[2] = byte(RDCCLAMP(rgba.z, 0.0f, 1.0f) * 255.0f);

            src += texelSize;
            dst += 3;
          }
        }
      }
    }

    float aspect = float(thumbwidth) / float(thumbheight);

    BITMAPV5HEADER bi;
    RDCEraseEl(bi);
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = RDCMIN((LONG)cx, (LONG)thumbwidth);
    bi.bV5Height = (LONG)(bi.bV5Width / aspect);
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    if(cx != thumbwidth)
    {
      byte *resizedpixels = (byte *)malloc(3 * bi.bV5Width * bi.bV5Height);

      stbir_resize_uint8_srgb(thumbpixels, thumbwidth, thumbheight, 0, resizedpixels, bi.bV5Width,
                              bi.bV5Height, 0, STBIR_RGB);

      free(thumbpixels);

      thumbpixels = resizedpixels;
    }

    HDC dc = ::CreateCompatibleDC(0);

    RGBQUAD *pArgb;
    *phbmp = ::CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&pArgb, NULL, 0);

    if(*phbmp)
    {
      DWORD *pBits = (DWORD *)pArgb;
      for(int y = bi.bV5Height - 1; y >= 0; y--)
      {
        for(int x = 0; x < bi.bV5Width; x++)
        {
          byte *srcPixel = &thumbpixels[3 * (x + bi.bV5Width * y)];

          *pBits = 0xff << 24 | (srcPixel[0] << 16) | (srcPixel[1] << 8) | (srcPixel[2] << 0);

          pBits++;
        }
      }
    }

    free(thumbpixels);

    DeleteObject(dc);

    *pdwAlpha = WTSAT_ARGB;

    return S_OK;
  }
};

struct RDCThumbnailProviderFactory : public IClassFactory
{
  unsigned int m_iRefcount;

  RDCThumbnailProviderFactory() : m_iRefcount(1), locked(false) {}
  virtual ~RDCThumbnailProviderFactory() {}
  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0)
      delete this;
    return ret;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(IClassFactory))
    {
      *ppvObject = (void *)(IClassFactory *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (void *)(IUnknown *)this;
      AddRef();
      return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
  {
    if(pUnkOuter != NULL)
    {
      return CLASS_E_NOAGGREGATION;
    }

    if(riid == CLSID_RDCThumbnailProvider)
    {
      *ppvObject = (void *)(new RDCThumbnailProvider());
      return S_OK;
    }
    else if(riid == __uuidof(IThumbnailProvider))
    {
      *ppvObject = (void *)(new RDCThumbnailProvider());
      return S_OK;
    }
    else if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (void *)(IUnknown *)(IThumbnailProvider *)(new RDCThumbnailProvider());
      return S_OK;
    }

    *ppvObject = NULL;

    return E_NOINTERFACE;
  }

  virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock)
  {
    locked = (fLock == TRUE);
    return S_OK;
  }

  bool locked;
};

_Check_return_ STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, LPVOID *ppv)
{
  if(rclsid == CLSID_RDCThumbnailProvider)
  {
    if(ppv)
      *ppv = (LPVOID)(new RDCThumbnailProviderFactory);
    return S_OK;
  }

  if(ppv)
    *ppv = NULL;

  return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
  if(numProviders > 0)
    return S_FALSE;

  return S_OK;
}
