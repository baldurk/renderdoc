/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
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

#include <stdint.h>

namespace lizard
{
void writeLEInt(uint8_t *buf, uint32_t v)
{
  buf[0] = (v >> 0) & 0xFF;
  buf[1] = (v >> 8) & 0xFF;
  buf[2] = (v >> 16) & 0xFF;
  buf[3] = (v >> 24) & 0xFF;
}

uint32_t readLEInt(uint8_t *buffer)
{
  return (buffer[0] << 0) | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

/**
 * Utility function to decode a SLEB128 value.
 * Source: https://llvm.org/doxygen/LEB128_8h_source.html
 */
int64_t decodeSLEB128(const uint8_t *p, unsigned *n = nullptr, const uint8_t *end = nullptr,
                      const char **error = nullptr)
{
  const uint8_t *orig_p = p;
  int64_t Value = 0;
  unsigned Shift = 0;
  uint8_t Byte;
  if(error)
    *error = nullptr;
  do
  {
    if(end && p == end)
    {
      if(error)
        *error = "malformed sleb128, extends past end";
      if(n)
        *n = (unsigned)(p - orig_p);
      return 0;
    }
    Byte = *p++;
    Value |= (uint64_t(Byte & 0x7f) << Shift);
    Shift += 7;
  } while(Byte >= 128);
  // Sign extend negative numbers if needed.
  if(Shift < 64 && (Byte & 0x40))
    Value |= (-1ULL) << Shift;
  if(n)
    *n = (unsigned)(p - orig_p);
  return Value;
}

} /* namespace lizard */
