/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

template <typename BitType, typename FlagType, typename SignedType>
class BitFlagIterator
{
private:
  FlagType flags;

public:
  inline BitFlagIterator() : flags(0) {}
  inline BitFlagIterator(FlagType mask) : flags(mask) {}
  static inline BitFlagIterator begin(FlagType mask) { return BitFlagIterator(mask); }
  static inline BitFlagIterator end() { return BitFlagIterator(0); }
  inline BitType operator*() const { return (BitType)(flags & (FlagType)(-(SignedType)flags)); }
  inline BitFlagIterator &operator++()
  {
    flags ^= **this;
    return *this;
  }
  inline bool operator==(const BitFlagIterator &rhs) const { return flags == rhs.flags; }
  inline bool operator!=(const BitFlagIterator &rhs) const { return flags != rhs.flags; }
  inline BitFlagIterator &operator=(const BitFlagIterator &rhs) { flags = rhs.flags; }
};
