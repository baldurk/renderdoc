/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "common/common.h"

namespace LLVMBC
{
class BitWriter
{
public:
  BitWriter(bytebuf &buf) : m_Bits(buf), m_PartialBitOffset(0) {}
  ~BitWriter() { RDCASSERTEQUAL(m_PartialBitOffset, 0); }
  void c6(char c)
  {
    byte val = 62;
    if(c >= 'a' && c <= 'z')
      val = byte(c - 'a') + 0;
    else if(c >= 'A' && c <= 'Z')
      val = byte(c - 'A') + 26;
    else if(c >= '0' && c <= '9')
      val = byte(c - '0') + 52;
    else if(c == '.')
      val = 62;
    else if(c == '_')
      val = 63;
    else
      RDCERR("Unexpected 6-bit char: %x", (uint32_t)c);

    WriteBits(&val, 6);
  }

  template <typename T>
  void Write(const T &t)
  {
    WriteBits((const byte *)&t, sizeof(t) * 8);
  }

  template <typename T>
  void fixed(size_t bitWidth, const T &t)
  {
    byte scratch[8] = {};
    memcpy(scratch, &t, sizeof(T));

    WriteBits(scratch, bitWidth);
  }

  template <typename T>
  void vbr(size_t groupBitSize, const T &t)
  {
    const byte hibit = 1 << (groupBitSize - 1);
    const byte lobits = hibit - 1;

    uint64_t scratch = uint64_t(t);

    RDCASSERT(groupBitSize > 1 && "chunk size must be greater than 1");
    RDCASSERT(groupBitSize <= 8 && "Only chunk sizes up to 8 supported");

    do
    {
      // take the bits we can encode
      byte val = byte(scratch & lobits);

      // if there are bits remaining set the high bit in the group
      if(scratch > val)
        val |= hibit;

      // write the group
      WriteBits(&val, groupBitSize);

      // consume the written bits
      scratch >>= (groupBitSize - 1);

      // loop while there are still bits remaining in scratch
    } while(scratch);
  }

  static uint64_t svbr(int64_t var)
  {
    if(var >= 0)
      return uint64_t(var) << 1;
    // negative numbers, set the low bit
    else
      return uint64_t(-var) << 1 | 0x1;
  }

  void WriteBlob(const bytebuf &blob) { WriteBlob(blob.data(), blob.size()); }
  void WriteBlob(const byte *blob, size_t len)
  {
    // write the blob length
    vbr<size_t>(6, len);

    // align to dword boundary
    align32bits();

    // write the blob
    WriteBits(blob, len * 8);

    // align again
    align32bits();
  }

  void align32bits()
  {
    byte val[3] = {};

    // finish current byte, if needed
    if(m_PartialBitOffset > 0)
      WriteBits(val, 8 - m_PartialBitOffset);

    size_t numBytes = m_Bits.size();
    size_t alignedNumBytes = AlignUp4(numBytes);

    // write any bytes needed to align to 32-bit
    if(alignedNumBytes > numBytes)
      WriteBits(val, 8 * (alignedNumBytes - numBytes));
  }

  size_t GetByteOffset()
  {
    RDCASSERT(m_PartialBitOffset == 0);
    return m_Bits.size();
  }

  void PatchLengthWord(size_t offset, uint32_t length)
  {
    if(offset + 4 <= m_Bits.size())
      memcpy(m_Bits.data() + offset, &length, sizeof(length));
  }

private:
  void WriteBits(const byte *buf, size_t bufBitSize)
  {
    byte b = *buf;
    if(bufBitSize < 8)
    {
      // mask off any upper bits (in case less than a byte is coming in)
      b &= (1 << bufBitSize) - 1;
    }

    size_t bitShift = 0;

    // if we have to partially fill a byte, do that now
    if(m_PartialBitOffset > 0)
    {
      // how many bits remain in the partial byte?
      const size_t avail = 8 - m_PartialBitOffset;

      // add on what comes in
      m_Partial |= b << m_PartialBitOffset;

      // how many bits are we going to write?
      const size_t numPartialBits = RDCMIN(avail, bufBitSize);

      // update the partial offset and set the shift for later
      m_PartialBitOffset += numPartialBits;
      bitShift = numPartialBits;

      // we consumed some bits, update buffer size
      bufBitSize -= numPartialBits;
    }

    // if we finished the partial, push it
    if(m_PartialBitOffset == 8)
    {
      m_Bits.push_back(m_Partial);
      m_PartialBitOffset = 0;
    }

    // if we finished with the partial write, stop now
    if(bufBitSize == 0)
      return;

    // write any whole bytes that we have now
    while(bufBitSize >= 8)
    {
      if(bitShift > 0)
      {
        // if we used part of the first byte, we need to piece together the next byte to write from
        // two input bytes
        byte writeByte = b >> bitShift;
        buf++;

        // how many remaining bits are there in the next byte
        const size_t remainingBits = bufBitSize - (8 - bitShift);

        b = *buf;
        // mask as necessary
        if(remainingBits < 8)
          b &= (1 << remainingBits) - 1;

        writeByte |= b << (8 - bitShift);

        m_Bits.push_back(writeByte);
      }
      else
      {
        // otherwise we can just push the byte
        m_Bits.push_back(b);

        // how many remaining bits are there in the next byte
        const size_t remainingBits = bufBitSize - 8;

        buf++;
        b = *buf;
        // mask as necessary
        if(remainingBits < 8)
          b &= (1 << remainingBits) - 1;
      }

      bufBitSize -= 8;
    }

    // if there are any remaining bits, they'll need to go into the partial byte
    if(bufBitSize > 0)
    {
      m_PartialBitOffset = bufBitSize;

      // if the partial bits we've used of this byte plus the remaining bytes are less than 8, we
      // have what we need in b it just needs to be shifted
      if(bufBitSize + bitShift <= 8)
      {
        m_Partial = b >> bitShift;
      }
      else
      {
        // otherwise we need to take the remaining bits in b and add the last few bits from the next
        // byte
        m_Partial = b >> bitShift;
        bufBitSize -= (8 - bitShift);

        buf++;
        b = *buf;
        b &= (1 << bufBitSize) - 1;

        m_Partial |= b << (8 - bitShift);
      }
    }
  }

  byte m_Partial;
  size_t m_PartialBitOffset;
  bytebuf &m_Bits;
};

};    // namespace LLVMBC
