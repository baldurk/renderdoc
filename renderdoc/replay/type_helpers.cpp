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

#include "type_helpers.h"

#include "api/replay/renderdoc_replay.h"

#include <vector>
#include <string>
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"

template<>
string ToStrHelper<false, rdctype::str>::Get(const rdctype::str &el)
{
	return string(el.elems, el.elems+el.count);
}

template<>
string ToStrHelper<false, ResourceId>::Get(const ResourceId &el)
{
	char tostrBuf[256] = {0};

	StringFormat::snprintf(tostrBuf, 255, "ID %llu", el.id);

	return tostrBuf;
}

namespace rdctype
{
str &str::operator =(const std::string &in)
{
	Delete();
	count = (int32_t)in.size();
	if(count == 0)
	{
		elems = (char*)allocate(sizeof(char));
		elems[0] = 0;
	}
	else
	{
		elems = (char*)allocate(sizeof(char)*(count+1));
		memcpy(elems, &in[0], sizeof(char)*in.size());
		elems[count] = 0;
	}
	return *this;
}

str &str::operator =(const char *const in)
{
	Delete();
	count = (int32_t)strlen(in);
	if(count == 0)
	{
		elems = (char*)allocate(sizeof(char));
		elems[0] = 0;
	}
	else
	{
		elems = (char*)allocate(sizeof(char)*(count+1));
		memcpy(elems, &in[0], sizeof(char)*count);
		elems[count] = 0;
	}
	return *this;
}

}; // namespace rdctype
