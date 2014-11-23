/******************************************************************************
 * The MIT License (MIT)
 * 
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
#include "serialise/string_utils.h"

#include <stdarg.h>

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

int Wide2UTF8(wchar_t chr, char mbchr[4])
{
	//U+00000 -> U+00007F 1 byte  0xxxxxxx
	//U+00080 -> U+0007FF 2 bytes 110xxxxx 10xxxxxx
	//U+00800 -> U+00FFFF 3 bytes 1110xxxx 10xxxxxx 10xxxxxx
	//U+10000 -> U+1FFFFF 4 bytes 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

	if(chr > 0x10FFFF)
		chr = 0xFFFD; // replacement character

	if(chr <= 0x7f)
	{
		mbchr[0] = (char)chr;
		return 1;
	}
	else if(chr <= 0x7ff)
	{
		mbchr[1] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[0] = 0xC0 | (char)(chr & 0x1f);
		return 2;
	}
	else if(chr <= 0xffff)
	{
		mbchr[2] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[1] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[0] = 0xE0 | (char)(chr & 0x0f);chr >>= 4;
		return 3;
	}
	else
	{
		// invalid codepoints above 0x10FFFF were replaced above
		mbchr[3] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[2] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[1] = 0x80 | (char)(chr & 0x3f);chr >>= 6;
		mbchr[0] = 0xF0 | (char)(chr & 0x07);chr >>= 3;
		return 4;
	}
}

}; // namespace StringFormat

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