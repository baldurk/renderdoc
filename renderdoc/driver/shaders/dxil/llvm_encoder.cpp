/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Baldur Karlsson
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

#include "llvm_encoder.h"
#include "os/os_specific.h"
#include "llvm_common.h"

namespace LLVMBC
{
BitcodeWriter::BitcodeWriter(bytebuf &buf) : b(buf)
{
  b.Write(LLVMBC::BitcodeMagic);
}

BitcodeWriter::~BitcodeWriter()
{
}
};

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

#include "llvm_decoder.h"

TEST_CASE("Check LLVM bitwriter", "[llvm]")
{
  bytebuf bits;

  SECTION("Check simple writing of bytes")
  {
    LLVMBC::BitWriter w(bits);

    w.Write<byte>(0x01);
    w.Write<byte>(0x02);
    w.Write<byte>(0x40);
    w.Write<byte>(0x80);
    w.Write<byte>(0xff);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(r.Read<byte>() == 0x01);
    CHECK(r.Read<byte>() == 0x02);
    CHECK(r.Read<byte>() == 0x40);
    CHECK(r.Read<byte>() == 0x80);
    CHECK(r.Read<byte>() == 0xff);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check fixed encoding")
  {
    uint32_t val = 0x3CA5F096;

    LLVMBC::BitWriter w(bits);

    for(uint32_t i = 0; i < 32; i++)
      w.fixed(i + 1, val);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    for(uint32_t i = 0; i < 32; i++)
    {
      CHECK(r.fixed<uint32_t>(i + 1) == (val & ((1ULL << (i + 1)) - 1)));
    }

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check variable encoding")
  {
    LLVMBC::BitWriter w(bits);

    w.vbr<uint32_t>(8, 0x12);
    w.vbr<uint32_t>(6, 0x12);
    w.vbr<uint32_t>(5, 0x12);
    w.vbr<uint32_t>(4, 0x12);
    w.vbr<uint32_t>(3, 0x12);

    w.vbr<uint32_t>(8, 0x12345678);
    w.vbr<uint32_t>(6, 0x12345678);
    w.vbr<uint32_t>(5, 0x12345678);
    w.vbr<uint32_t>(4, 0x12345678);
    w.vbr<uint32_t>(3, 0x12345678);

    w.vbr<uint64_t>(8, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(6, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(5, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(4, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(3, 0x123456789ABCDEFULL);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(r.vbr<uint32_t>(8) == 0x12);
    CHECK(r.vbr<uint32_t>(6) == 0x12);
    CHECK(r.vbr<uint32_t>(5) == 0x12);
    CHECK(r.vbr<uint32_t>(4) == 0x12);
    CHECK(r.vbr<uint32_t>(3) == 0x12);

    CHECK(r.vbr<uint32_t>(8) == 0x12345678);
    CHECK(r.vbr<uint32_t>(6) == 0x12345678);
    CHECK(r.vbr<uint32_t>(5) == 0x12345678);
    CHECK(r.vbr<uint32_t>(4) == 0x12345678);
    CHECK(r.vbr<uint32_t>(3) == 0x12345678);

    CHECK(r.vbr<uint64_t>(8) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(6) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(5) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(4) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(3) == 0x123456789ABCDEFULL);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check signed vbr encoding")
  {
    LLVMBC::BitWriter w(bits);

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x12));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x12));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x1234));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x1234));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x12345678));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x12345678));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(INT_MAX));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-INT_MAX));

    w.align32bits();

    CHECK(bits.size() == 28);

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x12);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x12);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x1234);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x1234);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x12345678);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x12345678);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == INT_MAX);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -INT_MAX);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check char6 encoding")
  {
    LLVMBC::BitWriter w(bits);

    const char string[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";

    for(size_t i = 0; i < strlen(string); i++)
      w.c6(string[i]);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    for(size_t i = 0; i < strlen(string); i++)
      CHECK(r.c6() == string[i]);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check blobs")
  {
    bytebuf foo = {0x01, 0x02, 0x40, 0x80, 0xff};
    for(byte i = 0; i < 250; i++)
      foo.push_back(i);

    foo.push_back(0x80);
    foo.push_back(0x70);
    foo.push_back(0x60);

    LLVMBC::BitWriter w(bits);

    w.WriteBlob(foo);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    const byte *ptr = NULL;
    size_t len = 0;
    r.ReadBlob(ptr, len);

    r.align32bits();

    CHECK(r.AtEndOfStream());

    REQUIRE(len == foo.size());
    CHECK(bytebuf(ptr, len) == foo);
  }
}

#endif
