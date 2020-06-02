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

#ifndef GATOR_MESSAGE_H
#define GATOR_MESSAGE_H

#include <string>
#include <vector>
#include "stdint.h"

namespace lizard
{
class GatorMessage
{
public:
  GatorMessage();
  GatorMessage(const std::vector<uint8_t> &in, uint8_t type);
  ~GatorMessage();
  int64_t getPackedInt();
  std::string getGatorString();
  bool hasRemaining();
  bool hasData();
  void setType(uint8_t type);
  uint8_t getType();
  void setData(const std::vector<uint8_t> &data);
  const std::vector<uint8_t> &getData();

private:
  std::vector<uint8_t> m_in;
  uint8_t m_type;
  unsigned int m_pos;
};

} /* namespace lizard */

#endif    // GATOR_MESSAGE_H
