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

#include "gator_message.hpp"
#include "message_util.hpp"

namespace lizard
{
GatorMessage::GatorMessage() : m_type(0), m_pos(0)
{
}

GatorMessage::GatorMessage(const std::vector<uint8_t> &in, uint8_t type)
    : m_in(in), m_type(type), m_pos(0)
{
}

GatorMessage::~GatorMessage()
{
}

void GatorMessage::setType(uint8_t type)
{
  m_type = type;
}

uint8_t GatorMessage::getType()
{
  return m_type;
}

void GatorMessage::setData(const std::vector<uint8_t> &data)
{
  m_in = data;
}

const std::vector<uint8_t> &GatorMessage::getData()
{
  return m_in;
}

bool GatorMessage::hasData()
{
  return !m_in.empty();
}

bool GatorMessage::hasRemaining()
{
  return m_pos < (m_in.size() - 1);
}

int64_t GatorMessage::getPackedInt()
{
  unsigned int sizeRead;
  int64_t value = decodeSLEB128(&m_in[m_pos], &sizeRead);
  m_pos += sizeRead;
  return value;
}

std::string GatorMessage::getGatorString()
{
  unsigned int sizeRead;
  int64_t stringLength = decodeSLEB128(&m_in[m_pos], &sizeRead);
  m_pos += sizeRead;
  std::string result(m_in.begin() + m_pos, m_in.begin() + m_pos + stringLength);
  m_pos += stringLength;
  return result;
}

} /* namespace lizard */
