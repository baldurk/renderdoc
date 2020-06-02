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

#include "gatord_xml_reader.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "gator_constants.hpp"

#include <pugixml/pugixml.hpp>

namespace lizard
{
static void reportXmlError(pugi::xml_parse_result result, const void *xmlData, size_t xmlSize)
{
  std::cout << "XML [] parsed with errors: ";
  std::cout << "Error description: " << result.description() << "\n";
  std::cout << "Error offset: " << result.offset << " (error at [..."
            << (static_cast<const char *>(xmlData) + result.offset) << "]\n\n";
}

static uint8_t strToEventClass(const std::string &eventClass)
{
  if(eventClass == "absolute")
  {
    return CLASS_ABSOLUTE;
  }
  else if(eventClass == "activity")
  {
    return CLASS_ACTIVITY;
  }
  else if(eventClass == "delta")
  {
    return CLASS_DELTA;
  }
  else if(eventClass == "incident")
  {
    return CLASS_INCIDENT;
  }
  else
  {
    return CLASS_UNKNOWN;
  }
}

static uint8_t strToEventDisplay(const std::string &eventDisplay)
{
  if(eventDisplay == "accumulate")
  {
    return DISPLAY_ACCUMULATE;
  }
  else if(eventDisplay == "average")
  {
    return DISPLAY_AVERAGE;
  }
  else if(eventDisplay == "maximum")
  {
    return DISPLAY_MAXIMUM;
  }
  else if(eventDisplay == "minimum")
  {
    return DISPLAY_MINIMUM;
  }
  else if(eventDisplay == "hertz")
  {
    return DISPLAY_HERTZ;
  }
  else
  {
    return DISPLAY_UNKNOWN;
  }
}

static LizardCounter::UnitType strToEventUnits(const std::string &eventUnits)
{
  if(eventUnits == "B")
  {
    return LizardCounter::UNITS_BYTE;
  }
  else if(eventUnits == "Hz")
  {
    return LizardCounter::UNITS_HZ;
  }
  else if(eventUnits == "MHz")
  {
    return LizardCounter::UNITS_MHZ;
  }
  else if(eventUnits == "pages")
  {
    return LizardCounter::UNITS_PAGES;
  }
  else if(eventUnits == "s")
  {
    return LizardCounter::UNITS_S;
  }
  else if(eventUnits == "V")
  {
    return LizardCounter::UNITS_V;
  }
  else if(eventUnits == "mV")
  {
    return LizardCounter::UNITS_MV;
  }
  else if(eventUnits == "°C")
  {
    return LizardCounter::UNITS_CELSIUS;
  }
  else if(eventUnits == "RPM")
  {
    return LizardCounter::UNITS_RPM;
  }
  else
  {
    return LizardCounter::UNITS_UNKNOWN;
  }
}

std::vector<GatordXML::Configuration> GatordXML::parseConfiguration(const void *xmlData,
                                                                    size_t xmlSize)
{
  std::vector<GatordXML::Configuration> configurations;

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xmlData, xmlSize);

  if(result)
  {
    for(pugi::xml_node &cfg : doc.child("configurations").children("configuration"))
    {
      std::string name = cfg.attribute("counter").as_string();
      uint32_t event = cfg.attribute("event").as_uint();
      uint32_t cores = cfg.attribute("cores").as_uint();

      assert(name.size() > 0);

      configurations.push_back({name, event, cores});
    }
  }
  else
  {
    reportXmlError(result, xmlData, xmlSize);
  }

  return configurations;
}

std::vector<std::string> GatordXML::parseCounters(const void *xmlData, size_t xmlSize)
{
  std::vector<std::string> counters;

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xmlData, xmlSize);

  if(result)
  {
    for(pugi::xml_node &cfg : doc.child("counters").children("counter"))
    {
      std::string name = cfg.attribute("name").as_string();
      assert(name.size() > 0);
      counters.push_back(name);
    }
  }
  else
  {
    reportXmlError(result, xmlData, xmlSize);
  }

  return counters;
}

std::vector<GatordXML::EventCategory> GatordXML::parseEvents(const void *xmlData, size_t xmlSize)
{
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xmlData, xmlSize);

  if(!result)
  {
    reportXmlError(result, xmlData, xmlSize);
    return {};
  }

  std::vector<GatordXML::EventCategory> eventCategories;

  for(pugi::xml_node &categoryNode : doc.child("events").children("category"))
  {
    GatordXML::EventCategory category;
    category.name = categoryNode.attribute("name").as_string();
    assert(category.name.size() > 0);

    for(pugi::xml_node &eventNode : categoryNode.children("event"))
    {
      GatordXML::Event event;
      event.name = eventNode.attribute("name").as_string();
      event.title = eventNode.attribute("title").as_string();
      event.counter = eventNode.attribute("counter").as_string();
      event.description = eventNode.attribute("description").as_string();
      event.event = eventNode.attribute("event").as_uint();
      event.eventClass = strToEventClass(eventNode.attribute("class").as_string());
      event.display = strToEventDisplay(eventNode.attribute("display").as_string());
      event.units = strToEventUnits(eventNode.attribute("units").as_string());
      event.multiplier = eventNode.attribute("multiplier").as_double();
      if(event.multiplier == 0)
      {
        event.multiplier = 1;
      }
      assert(event.name.size() > 0);

      category.events.push_back(event);
    }

    eventCategories.push_back(category);
  }

  return eventCategories;
}

std::vector<GatordXML::CapturedCounter> GatordXML::parseCapturedCounters(const void *xmlData,
                                                                         size_t xmlSize)
{
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xmlData, xmlSize);

  if(!result)
  {
    reportXmlError(result, xmlData, xmlSize);
    return {};
  }

  std::vector<GatordXML::CapturedCounter> capturedCounters;

  for(pugi::xml_node &counterNode : doc.child("captured").child("counters"))
  {
    GatordXML::CapturedCounter counter;
    counter.key = counterNode.attribute("key").as_int();
    counter.type = counterNode.attribute("type").as_string();
    counter.event = counterNode.attribute("event").as_int();
    capturedCounters.push_back(counter);
  }

  return capturedCounters;
}

} /* namespace lizard */
