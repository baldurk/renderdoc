/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "3rdparty/lz4/lz4.h"
#include "3rdparty/stb/stb_image_resize.h"
#include "common/common.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "serialise/rdcfile.h"

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

  RDCThumbnailProvider() : m_iRefcount(1), m_Inited(false) { InterlockedIncrement(&numProviders); }
  virtual ~RDCThumbnailProvider()
  {
    delete[] m_Thumb.pixels;
    InterlockedDecrement(&numProviders);
  }
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

    std::vector<byte> captureHeader(buf, buf + numRead);

    delete[] buf;

    {
      RDCFile rdc;
      rdc.Open(captureHeader);

      m_Thumb = rdc.GetThumbnail();

      // we don't care about the error code (which would come from the truncated file), we just care
      // if we got the thumbnail
      if(m_Thumb.len > 0 && m_Thumb.width > 0 && m_Thumb.height > 0 && m_Thumb.pixels)
      {
        buf = new byte[m_Thumb.len];
        memcpy(buf, m_Thumb.pixels, m_Thumb.len);
        m_Thumb.pixels = buf;
      }
      else
      {
        ReadLegacyCaptureThumb(captureHeader);
      }
    }

    m_Inited = true;

    return S_OK;
  }

  void STDMETHODCALLTYPE ReadLegacyCaptureThumb(std::vector<byte> &captureHeader)
  {
    // we want to support old capture files, so we decode the thumbnail by hand here with the
    // old header.
    const uint32_t MAGIC_HEADER = MAKE_FOURCC('R', 'D', 'O', 'C');

    byte *readPtr = captureHeader.data();
    byte *readEnd = readPtr + captureHeader.size();

    if(captureHeader.size() < sizeof(MAGIC_HEADER) ||
       memcmp(&MAGIC_HEADER, readPtr, sizeof(MAGIC_HEADER)))
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
        std::vector<byte> uncompressed;

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

      byte *pixels = new byte[thumbLen];
      memcpy(pixels, readPtr, thumbLen);

      m_Thumb.width = (uint16_t)thumbWidth;
      m_Thumb.height = (uint16_t)thumbHeight;
      m_Thumb.len = thumbLen;
      m_Thumb.pixels = pixels;
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

    if(m_Thumb.len == 0)
    {
      RDCERR("Problem opening file");
      return E_NOTIMPL;
    }

    const byte *jpgbuf = m_Thumb.pixels;
    size_t thumblen = m_Thumb.len;
    uint32_t thumbwidth = m_Thumb.width, thumbheight = m_Thumb.height;

    if(jpgbuf == NULL)
      return E_NOTIMPL;

    int w = thumbwidth;
    int h = thumbheight;
    int comp = 3;
    byte *thumbpixels =
        jpgd::decompress_jpeg_image_from_memory(jpgbuf, (int)thumblen, &w, &h, &comp, 3);

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
                              bi.bV5Height, 0, 3, -1, 0);

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
