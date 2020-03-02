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

#ifndef LIB_GATORD_XML_READER_HPP
#define LIB_GATORD_XML_READER_HPP

#include <string>
#include <vector>

#include "lizard_counter.hpp"

namespace lizard
{
namespace GatordXML
{
struct Configuration
{
  std::string name;
  uint32_t event;
  uint32_t cores;
};

std::vector<Configuration> parseConfiguration(const void *xmlData, size_t xmlSize);
std::vector<std::string> parseCounters(const void *xmlData, size_t xmlSize);

struct Event
{
  std::string title;
  std::string name;
  std::string description;
  uint32_t event;
  std::string counter;
  uint8_t eventClass;
  uint8_t display;
  LizardCounter::UnitType units;
  double multiplier;
};
struct EventCategory
{
  std::string name;
  std::vector<Event> events;
};

std::vector<EventCategory> parseEvents(const void *xmlData, size_t xmlSize);

struct CapturedCounter
{
  uint32_t key;
  std::string type;
  uint32_t event;
};

std::vector<CapturedCounter> parseCapturedCounters(const void *xmlData, size_t xmlSize);
}

} /* namespace lizard */

#endif /* LIB_GATORD_XML_READER_HPP */
