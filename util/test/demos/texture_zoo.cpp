/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <algorithm>
#include "test_common.h"

namespace TextureZoo
{
void MakePixel(byte *data, const TexConfig &cfg, uint32_t x, uint32_t y, uint32_t z, uint32_t mip,
               uint32_t slice)
{
  // each 3D slice cycles the x
  x += z;
  x %= std::max(1U, texWidth >> mip);

  if(cfg.data == DataType::Float || cfg.data == DataType::UNorm || cfg.data == DataType::SNorm)
  {
    // start points for each component
    const float vals[] = {
        0.1f,
        0.35f,
        0.6f,
        0.85f,
    };

    for(uint32_t c = 0; c < cfg.componentCount; c++)
    {
      uint32_t idx = c;

      // pixels off the diagonal invert the colors
      if(x != y)
        idx = 3 - idx;

      // subsequent slices add a coarse checkerboard pattern of inverted colors
      if((slice % 2 > 0) && (((x / 2) % 2) != ((y / 2) % 2)))
        idx = 3 - idx;

      float f = vals[idx];

      // subsequent mips are shifted up a bit
      f += 0.075f * mip;

      // Signed normals are negative
      if(cfg.data == DataType::SNorm)
        f = -f;

      // if it's a full float, just copy
      if(cfg.componentBytes == 4)
      {
        memcpy(data, &f, cfg.componentBytes);
      }
      else if(cfg.componentBytes == 2)
      {
        uint16_t h;
        if(cfg.data == DataType::Float)
          h = MakeHalf(f);
        else if(cfg.data == DataType::UNorm)
          h = uint16_t(f * 0xffff);
        else if(cfg.data == DataType::SNorm)
          h = f < 0 ? int16_t(roundf(f * 0x8000)) : int16_t(roundf(f * 0x7fff));
        memcpy(data, &h, cfg.componentBytes);
      }
      else if(cfg.componentBytes == 1)
      {
        uint8_t b;
        if(cfg.data == DataType::UNorm)
          b = uint8_t(f * 0xff);
        else if(cfg.data == DataType::SNorm)
          b = f < 0 ? int8_t(roundf(f * 0x80)) : int8_t(roundf(f * 0x7f));
        memcpy(data, &b, cfg.componentBytes);
      }
      else
      {
        TEST_ERROR("Unexpected component bytes %d in float", cfg.componentBytes);
      }

      data += cfg.componentBytes;
    }
  }
  else if(cfg.data == DataType::UInt || cfg.data == DataType::SInt)
  {
    // same pattern as above but with integer values
    const int32_t vals[] = {
        10,
        40,
        70,
        100,
    };

    for(uint32_t c = 0; c < cfg.componentCount; c++)
    {
      uint32_t idx = c;

      // pixels off the diagonal invert the colors
      if(x != y)
        idx = 3 - idx;

      if((slice % 3 > 0) && (((x / 2) % 2) != ((y / 2) % 2)))
        idx = 3 - idx;

      int32_t val = vals[idx];

      val += 10 * mip;

      // Signed ints are negative
      if(cfg.data == DataType::SInt)
        val = -val;

      // because the values are below one byte and we're little-endian we can just copy the
      // right number of bytes from val
      memcpy(data, &val, cfg.componentBytes);

      data += cfg.componentBytes;
    }
  }
}

void MakeData(TexData &data, const TexConfig &cfg, Vec4i dimensions, uint32_t mip, uint32_t slice)
{
  uint32_t mipWidth = std::max(1, dimensions.x >> mip);
  uint32_t mipHeight = std::max(1, dimensions.y >> mip);
  uint32_t mipDepth = std::max(1, dimensions.z >> mip);

  if(cfg.type == TextureType::Unknown)
  {
    data = TexData();
    return;
  }
  else if(cfg.type == TextureType::Regular)
  {
    uint32_t pixelPitch = cfg.componentBytes * cfg.componentCount;
    data.rowPitch = pixelPitch * mipWidth;
    data.slicePitch = data.rowPitch * mipHeight;

    data.byteData.resize(data.slicePitch * mipDepth);

    byte *out = data.byteData.data();

    for(uint32_t z = 0; z < mipDepth; z++)
    {
      for(uint32_t y = 0; y < mipHeight; y++)
      {
        for(uint32_t x = 0; x < mipWidth; x++)
        {
          MakePixel(out, cfg, x, y, z, mip, slice);
          out += pixelPitch;
        }
      }
    }
  }
  else
  {
    bool bc1 = false, bc2alpha = false, bc3alpha = false, bc6 = false, bc7 = false, sharedExp = false;
    int bc4channels = 0;
    uint32_t nybblePattern = 0;
    bool rgb5 = false;
    int alphabitPlace = 0;
    bool rgb10a2 = false;

    switch(cfg.type)
    {
      case TextureType::BC1: bc1 = true; break;
      case TextureType::BC2:
        bc1 = true;
        bc2alpha = true;
        break;
      case TextureType::BC3:
        bc1 = true;
        bc3alpha = true;
        break;
      case TextureType::BC4: bc4channels = 1; break;
      case TextureType::BC5: bc4channels = 2; break;
      case TextureType::BC6: bc6 = true; break;
      case TextureType::BC7: bc7 = true; break;
      case TextureType::R9G9B9E5: sharedExp = true; break;
      case TextureType::G4R4: nybblePattern = 0x12; break;
      case TextureType::A4R4G4B4: nybblePattern = 0x3214; break;
      case TextureType::R4G4B4A4: nybblePattern = 0x4321; break;
      case TextureType::R5G6B5:
        rgb5 = true;
        alphabitPlace = 0;
        break;
      case TextureType::R5G5B5A1:
        rgb5 = true;
        alphabitPlace = 1;
        break;
      case TextureType::A1R5G5B5:
        rgb5 = true;
        alphabitPlace = 2;
        break;
      case TextureType::RGB10A2: rgb10a2 = true; break;
      default: data = TexData(); return;
    }

    // get float data so we can do the best possible job of truncating to the desired bit width
    TexConfig floatcfg = {TextureType::Regular, 4, 4, DataType::Float};
    TexData floatdata;

    if(rgb10a2 && cfg.data == DataType::UInt)
      floatcfg.data = cfg.data;

    MakeData(floatdata, floatcfg, dimensions, mip, slice);

    Vec4f *srcPixels = (Vec4f *)floatdata.byteData.data();
    Vec4i *srcPixelsI = (Vec4i *)floatdata.byteData.data();

    if(rgb10a2)
    {
      uint32_t pixelPitch = 4;
      data.rowPitch = pixelPitch * mipWidth;
      data.slicePitch = data.rowPitch * mipHeight;

      data.byteData.resize(data.slicePitch * mipDepth);

      uint32_t *out = (uint32_t *)data.byteData.data();
      for(uint32_t z = 0; z < mipDepth; z++)
      {
        for(uint32_t y = 0; y < mipHeight; y++)
        {
          for(uint32_t x = 0; x < mipWidth; x++)
          {
            uint32_t encodedPixel = 0;

            if(cfg.data == DataType::UInt)
            {
              int32_t rgba[4];
              rgba[0] = srcPixelsI[y * mipWidth + x].x;
              rgba[1] = srcPixelsI[y * mipWidth + x].y;
              rgba[2] = srcPixelsI[y * mipWidth + x].z;
              rgba[3] = srcPixelsI[y * mipWidth + x].w;

              encodedPixel |= (rgba[0] & 0x3ff) << 0;
              encodedPixel |= (rgba[1] & 0x3ff) << 10;
              encodedPixel |= (rgba[2] & 0x3ff) << 20;
              encodedPixel |= (std::min(rgba[3], 3) & 0x3) << 30;
            }
            else
            {
              float rgba[4];
              rgba[0] = srcPixels[y * mipWidth + x].x;
              rgba[1] = srcPixels[y * mipWidth + x].y;
              rgba[2] = srcPixels[y * mipWidth + x].z;
              rgba[3] = srcPixels[y * mipWidth + x].w;

              encodedPixel |= uint32_t(round(rgba[0] * 0x3ff)) << 0;
              encodedPixel |= uint32_t(round(rgba[1] * 0x3ff)) << 10;
              encodedPixel |= uint32_t(round(rgba[2] * 0x3ff)) << 20;
              encodedPixel |= uint32_t(round(rgba[3] * 0x3)) << 30;
            }

            *out = encodedPixel;
            out++;
          }
        }

        srcPixels += mipWidth * mipHeight;
        srcPixelsI += mipWidth * mipHeight;
      }
    }
    else if(nybblePattern || rgb5)
    {
      uint32_t pixelPitch = 2;
      data.rowPitch = pixelPitch * mipWidth;
      data.slicePitch = data.rowPitch * mipHeight;

      data.byteData.resize(data.slicePitch * mipDepth);

      uint8_t *out = data.byteData.data();

      for(uint32_t z = 0; z < mipDepth; z++)
      {
        for(uint32_t y = 0; y < mipHeight; y++)
        {
          for(uint32_t x = 0; x < mipWidth; x++)
          {
            float rgb[4];
            rgb[0] = srcPixels[y * mipWidth + x].x;
            rgb[1] = srcPixels[y * mipWidth + x].y;
            rgb[2] = srcPixels[y * mipWidth + x].z;
            rgb[3] = srcPixels[y * mipWidth + x].w;

            if(rgb5)
            {
              bool alpha = rgb[3] >= 0.5f;

              uint16_t encodedPixel = 0;

              if(alphabitPlace == 0)
              {
                encodedPixel |= uint16_t(rgb[0] * 31) << 0;
                encodedPixel |= uint16_t(rgb[1] * 63) << 5;
                encodedPixel |= uint16_t(rgb[2] * 31) << 11;
              }
              else
              {
                encodedPixel |= uint16_t(rgb[0] * 31) << 0;
                encodedPixel |= uint16_t(rgb[1] * 31) << 5;
                encodedPixel |= uint16_t(rgb[2] * 31) << 10;

                if(alphabitPlace == 1)
                {
                  if(alpha)
                    encodedPixel |= 0x8000;
                }
                else
                {
                  encodedPixel <<= 1;
                  if(alpha)
                    encodedPixel |= 0x1;
                }
              }

              memcpy(out, &encodedPixel, sizeof(encodedPixel));
              out += 2;
            }
            else
            {
              uint8_t encodedPixel = 0;

              encodedPixel |= uint8_t(rgb[((nybblePattern & 0x000f) >> 0) - 1] * 15) << 0;
              encodedPixel |= uint8_t(rgb[((nybblePattern & 0x00f0) >> 4) - 1] * 15) << 4;

              *out = encodedPixel;
              out++;

              if(nybblePattern & 0xff00)
              {
                encodedPixel = 0;
                encodedPixel |= uint8_t(rgb[((nybblePattern & 0x0f00) >> 8) - 1] * 15) << 0;
                encodedPixel |= uint8_t(rgb[((nybblePattern & 0xf000) >> 12) - 1] * 15) << 4;

                *out = encodedPixel;
                out++;
              }
            }
          }
        }

        srcPixels += mipWidth * mipHeight;
      }
    }
    else if(sharedExp)
    {
      uint32_t pixelPitch = 4;
      data.rowPitch = pixelPitch * mipWidth;
      data.slicePitch = data.rowPitch * mipHeight;

      data.byteData.resize(data.slicePitch * mipDepth);

      uint32_t *out = (uint32_t *)data.byteData.data();

      for(uint32_t z = 0; z < mipDepth; z++)
      {
        for(uint32_t y = 0; y < mipHeight; y++)
        {
          for(uint32_t x = 0; x < mipWidth; x++)
          {
            float rgb[3];
            rgb[0] = srcPixels[y * mipWidth + x].x;
            rgb[1] = srcPixels[y * mipWidth + x].y;
            rgb[2] = srcPixels[y * mipWidth + x].z;

            uint32_t encodedPixel = 0;

            int exp = -10;
            // we pick the highest exponent, losing bits off the bottom of any value that
            // needs a lower one, rather than picking a lower one and having to saturate
            // values that need a higher one
            for(int channel = 0; channel < 3; channel++)
            {
              int e = 0;
              frexpf(rgb[channel], &e);
              exp = std::max(exp, e);
            }

            for(int channel = 0; channel < 3; channel++)
              encodedPixel |= uint32_t(rgb[channel] * 511.0 / (1 << exp)) << (9 * channel);

            encodedPixel |= (exp + 15) << 27;

            *out = encodedPixel;
            out++;
          }
        }

        srcPixels += mipWidth * mipHeight;
      }
    }
    else
    {
      // these don't change, but make the code easier to read
      const uint32_t blockWidth = 4;
      const uint32_t blockHeight = 4;

      uint32_t blockSize;

      // 0.5 byte per pixel
      if(cfg.type == TextureType::BC1 || cfg.type == TextureType::BC4)
        blockSize = 8;
      else
        blockSize = 16;

      data.rowPitch = blockSize * std::max(1U, mipWidth / blockWidth);
      data.slicePitch = data.rowPitch * std::max(1U, mipHeight / blockHeight);

      data.byteData.resize(data.slicePitch * mipDepth);

      byte *out = (byte *)data.byteData.data();

      const Vec4f invalid(999001.0f, 999002.0f, -999003.0f, -999004.0f);

      // compress each slice separately
      for(uint32_t z = 0; z < mipDepth; z++)
      {
        // block compressed - iterate over the pixels in block size
        for(uint32_t y = 0; y < mipHeight; y += blockHeight)
        {
          for(uint32_t x = 0; x < mipWidth; x += blockWidth)
          {
            Vec4f blockPixels[blockWidth * blockHeight] = {};

            // copy all the in-range pixels into the block data
            for(uint32_t by = 0; by < blockHeight; by++)
            {
              for(uint32_t bx = 0; bx < blockWidth; bx++)
              {
                if(x + bx >= mipWidth || y + by >= mipHeight)
                {
                  blockPixels[by * blockWidth + bx] = invalid;
                }
                else
                {
                  blockPixels[by * blockWidth + bx] = srcPixels[(y + by) * mipWidth + (x + bx)];
                }
              }
            }

            // we should have at most two unique pixels. The pattern is structured to allow
            // that, since any other colour can't be uniquely represented in all compressed
            // formats (even interpolated values)
            Vec4f a = invalid, b = invalid;
            uint32_t bc1bitmask = 0;
            uint64_t bc4bitmask = 0;

            // BC1 and BC4 both share A = 0 and B = 0 codes
            enum class BCCode : uint64_t
            {
              A = 0,
              B = 1,
            };

            // iterate the pixels in the block in ascending bitmask order
            for(uint32_t p = 0; p < blockWidth * blockHeight; p++)
            {
              if(blockPixels[p] == invalid)
              {
                // out of bounds pixel (think of a 2x2 mip), store as A - whatever A is.
                bc1bitmask |= uint32_t(BCCode::A) << (p * 2);
                bc4bitmask |= uint64_t(BCCode::A) << (p * 3);
              }
              else if(a == invalid)
              {
                // A hasn't been found yet, let's use this pixel for that
                a = blockPixels[p];
                bc1bitmask |= uint32_t(BCCode::A) << (p * 2);
                bc4bitmask |= uint64_t(BCCode::A) << (p * 3);
              }
              else if(blockPixels[p] == a)
              {
                // if A has been found then re-use it before assigning to B
                bc1bitmask |= uint32_t(BCCode::A) << (p * 2);
                bc4bitmask |= uint64_t(BCCode::A) << (p * 3);
              }
              else if(b == invalid)
              {
                // B hasn't been found yet, let's use this pixel for that
                b = blockPixels[p];
                bc1bitmask |= uint32_t(BCCode::B) << (p * 2);
                bc4bitmask |= uint64_t(BCCode::B) << (p * 3);
              }
              else if(blockPixels[p] == b)
              {
                bc1bitmask |= uint32_t(BCCode::B) << (p * 2);
                bc4bitmask |= uint64_t(BCCode::B) << (p * 3);
              }
              else
              {
                TEST_ERROR("Found pixel that isn't A, or B!");
              }
            }

            byte a8[4], b8[4];
            uint16_t aHalf[4], bHalf[4];
            int16_t *aHalfS = (int16_t *)aHalf;
            int16_t *bHalfS = (int16_t *)bHalf;

            uint16_t a565 = 0;
            uint16_t b565 = 0;

            if(cfg.data == DataType::SNorm)
            {
              int8_t *ia8 = (int8_t *)a8;
              int8_t *ib8 = (int8_t *)b8;

              ia8[0] = int8_t(round(a.x * -127.0f));
              ia8[1] = int8_t(round(a.y * -127.0f));
              ia8[2] = int8_t(round(a.z * -127.0f));
              ia8[3] = int8_t(round(a.w * -127.0f));

              ib8[0] = int8_t(round(b.x * -127.0f));
              ib8[1] = int8_t(round(b.y * -127.0f));
              ib8[2] = int8_t(round(b.z * -127.0f));
              ib8[3] = int8_t(round(b.w * -127.0f));

              aHalf[0] = MakeHalf(-a.x);
              aHalf[1] = MakeHalf(-a.y);
              aHalf[2] = MakeHalf(-a.z);
              aHalf[3] = MakeHalf(-a.w);

              bHalf[0] = MakeHalf(-b.x);
              bHalf[1] = MakeHalf(-b.y);
              bHalf[2] = MakeHalf(-b.z);
              bHalf[3] = MakeHalf(-b.w);
            }
            else
            {
              a8[0] = byte(round(a.x * 255.0f));
              a8[1] = byte(round(a.y * 255.0f));
              a8[2] = byte(round(a.z * 255.0f));
              a8[3] = byte(round(a.w * 255.0f));

              // red
              a565 |= byte(round(a.x * 31.0f)) << 11;
              // green
              a565 |= byte(round(a.y * 63.0f)) << 5;
              // blue
              a565 |= byte(round(a.z * 31.0f)) << 0;

              b8[0] = byte(round(b.x * 255.0f));
              b8[1] = byte(round(b.y * 255.0f));
              b8[2] = byte(round(b.z * 255.0f));
              b8[3] = byte(round(b.w * 255.0f));

              // red
              b565 |= byte(round(b.x * 31.0f)) << 11;
              // green
              b565 |= byte(round(b.y * 63.0f)) << 5;
              // blue
              b565 |= byte(round(b.z * 31.0f)) << 0;

              aHalf[0] = MakeHalf(a.x);
              aHalf[1] = MakeHalf(a.y);
              aHalf[2] = MakeHalf(a.z);
              aHalf[3] = MakeHalf(a.w);

              bHalf[0] = MakeHalf(b.x);
              bHalf[1] = MakeHalf(b.y);
              bHalf[2] = MakeHalf(b.z);
              bHalf[3] = MakeHalf(b.w);
            }

            struct BC1
            {
              uint16_t a565;
              uint16_t b565;
              uint32_t bitmask;
            };

            static_assert(sizeof(BC1) == 8, "BC1 struct is mis-sized");

            struct BC4
            {
              uint64_t a : 8;
              uint64_t b : 8;
              uint64_t bitmask : 48;
            };

            static_assert(sizeof(BC4) == 8, "BC4 struct is mis-sized");

            if(bc2alpha)
            {
              uint64_t alphaBits = 0;

              for(uint32_t p = 0; p < blockWidth * blockHeight; p++)
              {
                BCCode code = BCCode((bc1bitmask & (0x3 << (p * 2))) >> (p * 2));
                if(code == BCCode::A)
                  alphaBits |= uint64_t(a8[3] >> 4) << (p * 4);
                else if(code == BCCode::B)
                  alphaBits |= uint64_t(b8[3] >> 4) << (p * 4);
              }

              memcpy(out, &alphaBits, sizeof(alphaBits));
              out += sizeof(alphaBits);
            }
            else if(bc3alpha)
            {
              // basically the same layout just a different meaning for codes above 1, which
              // we
              // don't use
              BC4 *alpha = (BC4 *)out;
              alpha->a = a8[3];
              alpha->b = b8[3];
              alpha->bitmask = bc4bitmask;
              out += sizeof(BC4);
            }

            if(bc1)
            {
              BC1 *rgb = (BC1 *)out;
              // we don't care about color0 <= color1 order
              rgb->a565 = a565;
              rgb->b565 = b565;
              rgb->bitmask = bc1bitmask;
              out += sizeof(BC1);
            }

            for(int ch = 0; ch < bc4channels; ch++)
            {
              BC4 *alpha = (BC4 *)out;
              alpha->a = a8[ch];
              alpha->b = b8[ch];
              alpha->bitmask = bc4bitmask;
              out += sizeof(BC4);
            }

            uint64_t bc67indexbits = 0;

            if(bc6 || bc7)
            {
              for(uint32_t p = 0; p < blockWidth * blockHeight; p++)
              {
                BCCode code = BCCode((bc1bitmask & (0x3 << (p * 2))) >> (p * 2));

                if(p == 0)
                {
                  // the first colour we came across should have been assigned code A. We
                  // require this, because we're missing a bit from the first index
                  TEST_ASSERT(code == BCCode::A, "First code must be code A when encoding BC6");
                }
                else
                {
                  if(code == BCCode::A)
                  {
                    bc67indexbits |= uint64_t(0) << ((p * 4) - 1);
                  }
                  else if(code == BCCode::B)
                  {
                    bc67indexbits |= uint64_t(15) << ((p * 4) - 1);
                  }
                }
              }
            }

            if(bc6)
            {
              byte mode = 0x03;
              // mode 3: no transformed endpoints, 0 partition bits, 10 endpoint bits per
              // channel, no delta bits.

              uint16_t bias = 0;

              if(cfg.data == DataType::SNorm)
              {
                // final quantize step, the absolute value gets scaled a little
                for(int ch = 0; ch < 3; ch++)
                {
                  bool negA = (aHalf[ch] & 0x8000) != 0;
                  bool negB = (bHalf[ch] & 0x8000) != 0;

                  int16_t valA = int16_t(((aHalf[ch] & 0x7fff) * 32) / 31);
                  int16_t valB = int16_t(((bHalf[ch] & 0x7fff) * 32) / 31);

                  aHalfS[ch] = (negA ? -valA : valA);
                  bHalfS[ch] = (negB ? -valB : valB);
                }

                bias = 63;
              }
              else
              {
                // final quantize step, such that max representable half float is 65504.0
                // (which gets mapped to 0xffff)
                for(int ch = 0; ch < 3; ch++)
                {
                  aHalf[ch] = uint32_t(aHalf[ch] * 64) / 31;
                  bHalf[ch] = uint32_t(bHalf[ch] * 64) / 31;
                }

                bias = 15;
              }

              uint64_t colorbits = 0;

              byte colorbit65 = 0;

              // 10 bits for each value, RGB for A then RGB for B
              colorbits |= uint64_t((aHalf[0] + bias) >> 6) << 0;
              colorbits |= uint64_t((aHalf[1] + bias) >> 6) << 10;
              colorbits |= uint64_t((aHalf[2] + bias) >> 6) << 20;

              colorbits |= uint64_t((bHalf[0] + bias) >> 6) << 30;
              colorbits |= uint64_t((bHalf[1] + bias) >> 6) << 40;
              colorbits |= uint64_t((bHalf[2] + bias) >> 6) << 50;    // overflows by 1 bit

              colorbit65 = (bHalf[2] >> 15) & 0x1;

              uint64_t block[2];

              // first 64 bits are mode, and 59 of the color bits.
              block[0] = mode << 0;
              block[0] |= colorbits << 5;

              // second 64-bit is the top bit of the colors bits, then the index bits
              block[1] = (bc67indexbits << 1) | colorbit65;

              memcpy(out, block, sizeof(block));
              out += sizeof(block);
            }

#define ROUND_7BIT(x) ((x) >> 1)
#define LO_BIT(x) ((x)&0x1)

            if(bc7)
            {
              byte mode = 0x40;
              // x1000000 = mode 6: no partition bits, no rotation bits, no index selection
              // bit.
              // 7 color bits, 7 alpha bits, 1 endpoint p-bit, 0 shared p-bits, 4 index bits,
              // 0 secondary index bits

              // color is stored R0, R1, G0, G1, B0, B1 because we only have one subset
              uint64_t colorbits = 0;
              colorbits |= uint64_t(ROUND_7BIT(a8[0])) << 0;
              colorbits |= uint64_t(ROUND_7BIT(b8[0])) << 7;
              colorbits |= uint64_t(ROUND_7BIT(a8[1])) << 14;
              colorbits |= uint64_t(ROUND_7BIT(b8[1])) << 21;
              colorbits |= uint64_t(ROUND_7BIT(a8[2])) << 28;
              colorbits |= uint64_t(ROUND_7BIT(b8[2])) << 35;

              uint64_t alphabits = 0;
              alphabits |= uint64_t(ROUND_7BIT(a8[3])) << 0;
              alphabits |= uint64_t(ROUND_7BIT(b8[3])) << 7;

              byte endpointA = 0;
              byte endpointB = 0;
              // take a vote, if more than two of the original values have the low bit set,
              // set
              // the endpoint. The tie-break is towards zero because we're wanting *more* than
              // two (so exactly two means 0)

              if(LO_BIT(a8[0]) + LO_BIT(a8[1]) + LO_BIT(a8[2]) + LO_BIT(a8[3]) > 2)
                endpointA = 1;

              if(LO_BIT(b8[0]) + LO_BIT(b8[1]) + LO_BIT(b8[2]) + LO_BIT(b8[3]) > 2)
                endpointB = 1;

              uint64_t block[2];

              // first 64 bits are mode, color, alpha, and endpoint A
              block[0] = mode << 0;
              block[0] |= colorbits << 7;
              block[0] |= alphabits << (7 + 42);
              block[0] |= uint64_t(endpointA & 0x1) << (7 + 42 + 14);

              // second 64-bit is endpoint B, then the index bits
              block[1] = (bc67indexbits << 1) | endpointB;

              memcpy(out, block, sizeof(block));
              out += sizeof(block);
            }
          }
        }

        srcPixels += floatdata.slicePitch / sizeof(Vec4f);
      }
    }
  }
}

};    // namespace TextureZoo
