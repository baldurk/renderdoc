/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "os/os_specific.h"
#include <stdarg.h>
#include "serialise/string_utils.h"

using std::string;

int utf8printf(char *buf, size_t bufsize, const char *fmt, va_list args);

namespace StringFormat
{
int snprintf(char *str, size_t bufSize, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int ret = StringFormat::vsnprintf(str, bufSize, fmt, args);

  va_end(args);

  return ret;
}

int vsnprintf(char *str, size_t bufSize, const char *format, va_list args)
{
  return ::utf8printf(str, bufSize, format, args);
}

string Fmt(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  va_list args2;
  va_copy(args2, args);

  int size = StringFormat::vsnprintf(NULL, 0, format, args2);

  char *buf = new char[size + 1];
  StringFormat::vsnprintf(buf, size + 1, format, args);
  buf[size] = 0;

  va_end(args);
  va_end(args2);

  string ret = buf;

  delete[] buf;

  return ret;
}

int Wide2UTF8(wchar_t chr, char mbchr[4])
{
  // U+00000 -> U+00007F 1 byte  0xxxxxxx
  // U+00080 -> U+0007FF 2 bytes 110xxxxx 10xxxxxx
  // U+00800 -> U+00FFFF 3 bytes 1110xxxx 10xxxxxx 10xxxxxx
  // U+10000 -> U+1FFFFF 4 bytes 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

  // upcast to uint32_t, so we do the same processing on windows where
  // sizeof(wchar_t) == 2
  uint32_t wc = (uint32_t)chr;

  if(wc > 0x10FFFF)
    wc = 0xFFFD;    // replacement character

  if(wc <= 0x7f)
  {
    mbchr[0] = (char)wc;
    return 1;
  }
  else if(wc <= 0x7ff)
  {
    mbchr[1] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[0] = 0xC0 | (char)(wc & 0x1f);
    return 2;
  }
  else if(wc <= 0xffff)
  {
    mbchr[2] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[1] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[0] = 0xE0 | (char)(wc & 0x0f);
    wc >>= 4;
    return 3;
  }
  else
  {
    // invalid codepoints above 0x10FFFF were replaced above
    mbchr[3] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[2] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[1] = 0x80 | (char)(wc & 0x3f);
    wc >>= 6;
    mbchr[0] = 0xF0 | (char)(wc & 0x07);
    wc >>= 3;
    return 4;
  }
}

};    // namespace StringFormat

string Callstack::AddressDetails::formattedString(const char *commonPath)
{
  char fmt[512] = {0};

  const char *f = filename.c_str();

  if(commonPath)
  {
    string common = strlower(string(commonPath));
    string fn = strlower(filename.substr(0, common.length()));

    if(common == fn)
    {
      f += common.length();
    }
  }

  if(line > 0)
    StringFormat::snprintf(fmt, 511, "%s line %d", function.c_str(), line);
  else
    StringFormat::snprintf(fmt, 511, "%s", function.c_str());

  return fmt;
}

string OSUtility::MakeMachineIdentString(uint64_t ident)
{
  string ret = "";

  if(ident & MachineIdent_Windows)
    ret += "Windows ";
  else if(ident & MachineIdent_Linux)
    ret += "Linux ";
  else if(ident & MachineIdent_macOS)
    ret += "macOS ";
  else if(ident & MachineIdent_Android)
    ret += "Android ";
  else if(ident & MachineIdent_iOS)
    ret += "iOS ";

  if(ident & MachineIdent_Arch_x86)
    ret += "x86 ";
  else if(ident & MachineIdent_Arch_ARM)
    ret += "ARM ";

  if(ident & MachineIdent_32bit)
    ret += "32-bit ";
  else if(ident & MachineIdent_64bit)
    ret += "64-bit ";

  switch(ident & MachineIdent_GPU_Mask)
  {
    case MachineIdent_GPU_ARM: ret += "ARM GPU "; break;
    case MachineIdent_GPU_AMD: ret += "AMD GPU "; break;
    case MachineIdent_GPU_IMG: ret += "Imagination GPU "; break;
    case MachineIdent_GPU_Intel: ret += "Intel GPU "; break;
    case MachineIdent_GPU_NV: ret += "nVidia GPU "; break;
    case MachineIdent_GPU_QUALCOMM: ret += "QUALCOMM GPU "; break;
    case MachineIdent_GPU_Samsung: ret += "Samsung GPU "; break;
    case MachineIdent_GPU_Verisilicon: ret += "Verisilicon GPU "; break;
    default: break;
  }

  return ret;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Test Process functions", "[osspecific]")
{
  SECTION("Environment Variables")
  {
    const char *var = Process::GetEnvVariable("TMP");

    if(!var)
      var = Process::GetEnvVariable("TEMP");

    if(!var)
      var = Process::GetEnvVariable("HOME");

    CHECK(var);
    CHECK(strlen(var) > 1);

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK_FALSE(var);

    EnvironmentModification mod;
    mod.name = "__renderdoc__unit_test_var";
    mod.value = "test_value";
    mod.sep = EnvSep::SemiColon;
    mod.mod = EnvMod::Append;

    Process::RegisterEnvironmentModification(mod);
    Process::ApplyEnvironmentModification();

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK(var);
    CHECK(var == std::string("test_value"));

    Process::RegisterEnvironmentModification(mod);
    Process::ApplyEnvironmentModification();

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK(var);
    CHECK(var == std::string("test_value;test_value"));

    mod.sep = EnvSep::Colon;

    Process::RegisterEnvironmentModification(mod);
    Process::ApplyEnvironmentModification();

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK(var);
    CHECK(var == std::string("test_value;test_value:test_value"));

    mod.value = "prepend";
    mod.sep = EnvSep::SemiColon;
    mod.mod = EnvMod::Prepend;

    Process::RegisterEnvironmentModification(mod);
    Process::ApplyEnvironmentModification();

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK(var);
    CHECK(var == std::string("prepend;test_value;test_value:test_value"));

    mod.value = "reset";
    mod.sep = EnvSep::SemiColon;
    mod.mod = EnvMod::Set;

    Process::RegisterEnvironmentModification(mod);
    Process::ApplyEnvironmentModification();

    var = Process::GetEnvVariable("__renderdoc__unit_test_var");

    CHECK(var);
    CHECK(var == std::string("reset"));
  };

  SECTION("Timing")
  {
    double freq = Timing::GetTickFrequency();
    REQUIRE(freq > 0.0);

    {
      uint64_t startTick = Timing::GetTick();
      CHECK(startTick > 0);

      uint64_t firstTick = Timing::GetTick();

      Threading::Sleep(500);

      uint64_t lastTick = Timing::GetTick();

      double milliseconds1 = double(firstTick - startTick) / freq;
      double milliseconds2 = double(lastTick - firstTick) / freq;

      CHECK(milliseconds1 > 0.0);
      CHECK(milliseconds1 < 1.0);

      CHECK(milliseconds2 > 490.0);
      CHECK(milliseconds2 < 510.0);
    }

    // timestamp as of the creation of this test
    CHECK(Timing::GetUnixTimestamp() > 1504519614);
  };

  SECTION("Bit counting")
  {
    SECTION("32-bits")
    {
      uint32_t val = 0;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 32);
      }

      val = 1;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 31);
      }

      val <<= 1;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 30);
      }

      val <<= 4;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 26);
      }

      val++;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 26);
      }

      val += 5;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 26);
      }

      val += 1000;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 21);
      }

      val *= 3;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 20);
      }

      val *= 200000;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 2);
      }
    };

#if ENABLED(RDOC_X64)
    SECTION("64-bits")
    {
      uint64_t val = 0;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 64);
      }

      val = 1;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 63);
      }

      val <<= 1;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 62);
      }

      val <<= 4;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 58);
      }

      val++;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 58);
      }

      val += 5;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 58);
      }

      val += 1000;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 53);
      }

      val *= 3;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 52);
      }

      val *= 200000;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 34);
      }

      val *= 1000000;

      {
        INFO("val is " << val);
        CHECK(Bits::CountLeadingZeroes(val) == 14);
      }
    };
#endif
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)