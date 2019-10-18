/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "llvm_decoder.h"

namespace LLVMBC
{
};    // namespace LLVMBC

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Check LLVM bitreader", "[llvm]")
{
  SECTION("Check simple reading of bytes")
  {
    byte bits[] = {0x01, 0x02, 0x40, 0x80, 0xff};

    LLVMBC::BitReader b(bits, sizeof(bits));

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);

    // ensure we can read it all out again in whole bytes
    for(size_t i = 0; i < sizeof(bits); i++)
    {
      byte val = b.Read<byte>();
      CHECK(val == bits[i]);
      if(i + 1 < sizeof(bits))
        CHECK(!b.AtEndOfStream());
      else
        CHECK(b.AtEndOfStream());
      CHECK(b.ByteOffset() == i + 1);
      CHECK(b.BitOffset() == (i + 1) * 8);
    }
  }

  SECTION("Check seeking within the stream")
  {
    byte bits[] = {0x01, 0x4f, 0x8c, 0xff};
    byte val;

    LLVMBC::BitReader b(bits, sizeof(bits));

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);

    b.SeekByte(4);

    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == 4);
    CHECK(b.BitOffset() == 32);

    b.SeekBit(32);

    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == 4);
    CHECK(b.BitOffset() == 32);

    b.SeekBit(29);

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 3);
    CHECK(b.BitOffset() == 29);

    val = b.fixed<byte>(3);

    CHECK(val == 0x7);
    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == 4);
    CHECK(b.BitOffset() == 32);

    b.SeekBit(0);

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);
  }

  SECTION("Check with empty bitstream")
  {
    byte bits[] = {0};

    LLVMBC::BitReader b(bits, 0);

    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);
  }

  SECTION("Check out of bounds behaviour")
  {
    byte bits[] = {0x40, 0x80, 0xff};

    LLVMBC::BitReader b(bits, sizeof(bits));

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);

    // first read is fully satisfied, we get the value we expect
    uint32_t val1 = b.fixed<uint32_t>(17);
    CHECK(val1 == 0x18040);

    // second read is partially out of bounds, we should read all 0s
    uint32_t val2 = b.fixed<uint32_t>(16);
    CHECK(val2 == 0);

    // should be exactly at the end of the stream
    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == sizeof(bits));
    CHECK(b.BitOffset() == sizeof(bits) * 8);
  }

  SECTION("Check fixed encoding")
  {
    // 0x96 = 0b 1001 0110
    // 0xF0 = 0b 1111 0000
    // 0xA5 = 0b 1010 0101
    // 0x3C = 0b 0011 1100

    // we pad out with 0s so we don't read off the end of the stream when reading up to 4 32-bit
    // values
    byte bits[] = {
        // dword 1
        0x96, 0xf0, 0xA5, 0x3C,
        // padding dword
        0x00, 0x00, 0x00, 0x00,
        // padding dword
        0x00, 0x00, 0x00, 0x00,
        // padding dword
        0x00, 0x00, 0x00, 0x00,
    };

    LLVMBC::BitReader b(bits, sizeof(bits));

    // for each of the bit widths, 1 to 32, read 4 values.
    // This should decode from the LSB to MSB in the bitstream - in the commented values above that
    // is right-to-left then top-to-bottom
    uint32_t expected[32][4] = {
        // i_1
        {0x00, 0x01, 0x01, 0x00},
        // i_2
        {0x02, 0x01, 0x01, 0x02},
        // i_3
        {0x06, 0x02, 0x02, 0x00},
        // i_4
        {0x06, 0x09, 0x00, 0x0f},
        // i_5
        {0x16, 0x04, 0x1C, 0x0B},
        // i_6
        {0x16, 0x02, 0x1F, 0x29},
        // i_7
        {0x16, 0x61, 0x17, 0x65},
        // i_8
        {0x96, 0xF0, 0xA5, 0x3C},

        // i_9
        {0x0096, 0x00F8, 0x0129, 0x0007},
        // i_10
        {0x0096, 0x017C, 0x03CA, 0x0000},
        // i_11
        {0x0096, 0x04BE, 0x00F2, 0x0000},
        // i_12
        {0x0096, 0x0A5F, 0x003C, 0x0000},
        // i_13
        {0x1096, 0x052F, 0x000F, 0x0000},
        // i_14
        {0x3096, 0x3297, 0x0003, 0x0000},
        // i_15
        {0x7096, 0x794B, 0x0000, 0x0000},
        // i_16
        {0xF096, 0x3CA5, 0x0000, 0x0000},

        // i_17
        {0x0001F096, 0x00001E52},
        // i_18
        {0x0001F096, 0x00000F29},
        // i_19
        {0x0005F096, 0x00000794},
        // i_20
        {0x0005F096, 0x000003CA},
        // i_21
        {0x0005F096, 0x000001E5},
        // i_22
        {0x0025F096, 0x000000F2},
        // i_23
        {0x0025F096, 0x00000079},
        // i_24
        {0x00A5F096, 0x0000003C},
        // i_25
        {0x00A5F096, 0x0000001E},
        // i_26
        {0x00A5F096, 0x0000000F},
        // i_27
        {0x04A5F096, 0x00000007},
        // i_28
        {0x0CA5F096, 0x00000003},
        // i_29
        {0x1CA5F096, 0x00000001},
        // i_30
        {0x3CA5F096, 0x00000000},
        // i_31
        {0x3CA5F096, 0x00000000},
        // i_32
        {0x3CA5F096, 0x00000000},
    };

    for(size_t i = 0; i < 32; i++)
    {
      b.SeekBit(0);
      uint32_t read;

      INFO("Bit width: " << (i + 1));

      read = b.fixed<uint32_t>(i + 1);
      CHECK(read == expected[i][0]);

      read = b.fixed<uint32_t>(i + 1);
      CHECK(read == expected[i][1]);

      read = b.fixed<uint32_t>(i + 1);
      CHECK(read == expected[i][2]);

      read = b.fixed<uint32_t>(i + 1);
      CHECK(read == expected[i][3]);
    }

    // should be exactly at the end of the stream
    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == sizeof(bits));
    CHECK(b.BitOffset() == sizeof(bits) * 8);
  }

  SECTION("Check variable encoding")
  {
    SECTION("Single chunk, no extension")
    {
      // just set as many bits as we can in one chunk, so all 1s except the MSB

      byte bits[] = {
          // i_vbr0 (padding)
          0,
          // i_vbr1 (padding)
          0,
          // i_vbr2
          0x01,
          // i_vbr3
          0x03,
          // i_vbr4
          0x07,
          // i_vbr5
          0x0f,
          // i_vbr6
          0x1f,
          // i_vbr7
          0x3f,
          // i_vbr8
          0x7f,
      };

      LLVMBC::BitReader b(bits, sizeof(bits));

      for(size_t i = 2; i <= 8; i++)
      {
        INFO("VBR group size: " << i);
        b.SeekByte(i);

        uint64_t val = b.vbr<uint64_t>(i);
        CHECK(val == bits[i]);
      }

      // should be exactly at the end of the stream
      CHECK(b.AtEndOfStream());
      CHECK(b.ByteOffset() == sizeof(bits));
      CHECK(b.BitOffset() == sizeof(bits) * 8);
    }

    SECTION("Two chunks, one extension")
    {
      // set all bits that we can from two chunks - that means the first chunk is all 1s, the second
      // is all 1s except the leading 0

      byte bits[] = {
          // i_vbr0 (padding)
          0, 0,
          // i_vbr1 (padding)
          0, 0,
          // i_vbr2
          0x07, 0x00,    // 0b 01 11
          // i_vbr3
          0x1f, 0x00,    // 0b 011 111
          // i_vbr4
          0x7f, 0x00,    // 0b 0111 1111
          // i_vbr5
          0xff, 0x01,    // 0b 01111 11111
          // i_vbr6
          0xff, 0x07,    // 0b 011111 111111
          // i_vbr7
          0xff, 0x1f,    // 0b 0111111 1111111
          // i_vbr8
          0xff, 0x7f,    // 0b 01111111 11111111
      };

      uint64_t expected[] = {
          0, 0,
          // i_vbr2
          0x0003,
          // i_vbr3
          0x000f,
          // i_vbr4
          0x003f,
          // i_vbr5
          0x00ff,
          // i_vbr6
          0x03ff,
          // i_vbr7
          0x0fff,
          // i_vbr8
          0x3fff,
      };

      LLVMBC::BitReader b(bits, sizeof(bits));

      for(size_t i = 2; i <= 8; i++)
      {
        INFO("VBR group size: " << i);
        b.SeekByte(i * 2);

        uint64_t val = b.vbr<uint64_t>(i);
        CHECK(val == expected[i]);
      }

      // should be exactly at the end of the stream
      CHECK(b.AtEndOfStream());
      CHECK(b.ByteOffset() == sizeof(bits));
      CHECK(b.BitOffset() == sizeof(bits) * 8);
    }

    SECTION("Five chunks, four extensions")
    {
      // set an alternating 10 pattern from the top bit. Each group except the last has a leading 1

      byte bits[] = {
          // i_vbr0 (padding)
          0, 0, 0, 0, 0,
          // i_vbr1 (padding)
          0, 0, 0, 0, 0,
          // i_vbr2
          0xBB, 0x01, 0x00, 0x00, 0x00,    // 0b 01 10 11 10 11
          // i_vbr3
          0xB6, 0x2D, 0x00, 0x00, 0x00,    // 0b 010 110 110 110 110
          // i_vbr4
          0xAD, 0xAD, 0x05, 0x00, 0x00,    // 0b 0101 1010 1101 1010 1101
          // i_vbr5
          0x5A, 0x6B, 0xAD, 0x00, 0x00,    // 0b 01010 11010 11010 11010 11010
          // i_vbr6
          0xB5, 0x5A, 0xAB, 0x15, 0x00,    // 0b 010101 101010 110101 101010 110101
          // i_vbr7
          0x6A, 0xB5, 0x5A, 0xAD, 0x02,    // 0b 0101010 1101010 1101010 1101010 1101010
          // i_vbr8
          0xD5, 0xAA, 0xD5, 0xAA, 0x55,    // 0b 01010101 10101010 11010101 10101010 11010101
      };

      uint64_t expected[] = {
          0, 0,
          // i_vbr2
          0x0000000015ULL,    // 0b 1 0 1 0 1
          // i_vbr3
          0x00000002AAULL,    // 0b 10 10 10 10 10
          // i_vbr4
          0x0000005555ULL,    // 0b 101 010 101 010 101
          // i_vbr5
          0x00000AAAAAULL,    // 0b 1010 1010 1010 1010 1010
          // i_vbr6
          0x0001555555ULL,    // 0b 10101 01010 10101 01010 10101
          // i_vbr7
          0x002AAAAAAAULL,    // 0b 101010 101010 101010 101010 101010
          // i_vbr8
          0x0555555555ULL,    // 0b 1010101 0101010 1010101 0101010 1010101
      };

      LLVMBC::BitReader b(bits, sizeof(bits));

      for(size_t i = 2; i <= 8; i++)
      {
        INFO("VBR group size: " << i);
        b.SeekByte(i * 5);

        uint64_t val = b.vbr<uint64_t>(i);
        CHECK(val == expected[i]);
      }

      // should be exactly at the end of the stream
      CHECK(b.AtEndOfStream());
      CHECK(b.ByteOffset() == sizeof(bits));
      CHECK(b.BitOffset() == sizeof(bits) * 8);
    }

    SECTION("Check signed vbr decoding")
    {
      // we don't check every possible bit width since this is decoded the same as vbr except for a
      // post-check and shift. Instead we use vbr4 since it's convenient for hex literals
      byte bits[] = {
          0x04,                // 0b 0100 = +2
          0x05,                // 0b 0101 = -2
          0xBA, 0x9E, 0x68,    // 0b 0110 1000 1001 1110 1011 1010 = +98765
          0xBB, 0x9E, 0x68,    // 0b 0110 1000 1001 1110 1011 1011 = -98765
          // INT64_MAX. 64-bits encoded in 3-bit groups is 22 groups, so 22 * 4-bit encoded groups
          // is 88 bits, meaning 11 bytes
          0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
          // INT64_MIN. Same as above but with the LSB set to 1
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
          // one more value just to check that we didn't overrun above
          0x06,    // 0b 0110 = +3
      };

      LLVMBC::BitReader b(bits, sizeof(bits));

      int64_t val;

      val = b.svbr<int64_t>(4);
      CHECK(val == 2);

      val = b.svbr<int64_t>(4);
      CHECK(val == 0);

      val = b.svbr<int64_t>(4);
      CHECK(val == -2);

      val = b.svbr<int64_t>(4);
      CHECK(val == 0);

      val = b.svbr<int64_t>(4);
      CHECK(val == 98765);

      val = b.svbr<int64_t>(4);
      CHECK(val == -98765);

      val = b.svbr<int64_t>(4);
      CHECK(val == INT64_MAX);

      val = b.svbr<int64_t>(4);
      CHECK(val == -INT64_MAX);

      val = b.svbr<int64_t>(4);
      CHECK(val == 3);

      val = b.svbr<int64_t>(4);
      CHECK(val == 0);

      // should be exactly at the end of the stream
      CHECK(b.AtEndOfStream());
      CHECK(b.ByteOffset() == sizeof(bits));
      CHECK(b.BitOffset() == sizeof(bits) * 8);
    }
  }

  SECTION("Check char6 encoding")
  {
    byte bits[64] = {};
    for(size_t i = 0; i < sizeof(bits); i++)
      bits[i] = i & 0xff;

    // this is the char6 encoding
    const char string[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";

    RDCCOMPILE_ASSERT(sizeof(string) - 1 == sizeof(bits),
                      "bits byte array and string should be same size.");

    LLVMBC::BitReader b(bits, sizeof(bits));

    for(size_t i = 0; i < sizeof(bits); i++)
    {
      char c = b.c6();
      // for simplicity we read padding too
      byte pad = b.fixed<byte>(2);

      CHECK(c == string[i]);
      CHECK(pad == 0);
    }
  }

  SECTION("Check 32-bit aligning")
  {
    byte bits[] = {
        // first i_4 value
        0x04,
        // padding for alignment
        0x00, 0x00, 0x00,

        // second two i_4 values
        0xF5,
        // i_24 value
        0xCA, 0x99, 0x23,

        // no padding - already aligned

        // i_6 value and i_2 value
        0xBF,
    };

    LLVMBC::BitReader b(bits, sizeof(bits));

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);

    uint32_t val;

    // first read is fully satisfied, we get the value we expect
    val = b.fixed<uint32_t>(4);
    CHECK(val == 0x4);
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 4);

    b.align32bits();

    CHECK(b.ByteOffset() == 4);
    CHECK(b.BitOffset() == 32);

    val = b.fixed<uint32_t>(4);
    CHECK(val == 0x5);

    val = b.fixed<uint32_t>(4);
    CHECK(val == 0xf);

    val = b.fixed<uint32_t>(24);
    CHECK(val == 0x2399CA);

    CHECK(b.ByteOffset() == 8);
    CHECK(b.BitOffset() == 64);

    // should be a no-op because we're already aligned
    b.align32bits();

    CHECK(b.ByteOffset() == 8);
    CHECK(b.BitOffset() == 64);

    val = b.fixed<uint32_t>(6);
    CHECK(val == 0x3f);

    val = b.fixed<uint32_t>(2);
    CHECK(val == 0x2);

    // should be exactly at the end of the stream
    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == sizeof(bits));
    CHECK(b.BitOffset() == sizeof(bits) * 8);
  }

  SECTION("Check blob fetch")
  {
    // size = 16 bytes for encoded data and first blob, 70 bytes for second blob, 2 bytes trailing
    // padding
    byte bits[16 + 70 + 2] = {
        // first vbr_6 length
        0x06,
        // padding for alignment
        0x00, 0x00, 0x00,

        // blob data
        0xF5, 0x00, 0xCA, 0x40, 0x99, 0x23,

        // padding for trailing alignment
        0x00, 0x00,

        // i_20 dummy to get us to the point where two vbr_6 chunks would be aligned
        // we choose a length of 70, which is 0b10 00110, then vbr_6 encoded it becomes
        // 0b000010 100110 which is 0xA6, over 12 bits. That leaves 4 bits in the upper part of
        // the last byte of the i_20, and the remaining 8 in the next byte
        0x5B, 0xC2, 0x64, 0x0A,
    };

    LLVMBC::BitReader b(bits, sizeof(bits));

    CHECK(!b.AtEndOfStream());
    CHECK(b.ByteOffset() == 0);
    CHECK(b.BitOffset() == 0);

    const byte *ptr = NULL;
    size_t size = 0;

    b.ReadBlob(ptr, size);

    CHECK(size == 6);
    CHECK(ptr == &bits[4]);

    uint32_t val = b.fixed<uint32_t>(20);
    CHECK(val == 0x4C25B);

    ptr = NULL;
    size = 0;

    b.ReadBlob(ptr, size);

    CHECK(size == 70);
    CHECK(ptr == &bits[16]);

    // should be exactly at the end of the stream
    CHECK(b.AtEndOfStream());
    CHECK(b.ByteOffset() == sizeof(bits));
    CHECK(b.BitOffset() == sizeof(bits) * 8);
  }
}

#endif