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
#include "common/string_utils.h"

using std::string;

wstring Callstack::AddressDetails::formattedString(const char *commonPath)
{
	wchar_t fmt[512] = {0};

	const wchar_t *f = filename.c_str();

	if(commonPath)
	{
		wstring common = strlower(widen(string(commonPath)));
		wstring fn = strlower(filename.substr(0, common.length()));

		if(common == fn)
		{
			f += common.length();
		}
	}

	if(line > 0)
		swprintf(fmt, 511, L"%ls line %d", function.c_str(), line);
	else
		swprintf(fmt, 511, L"%ls", function.c_str());

	return fmt;
}