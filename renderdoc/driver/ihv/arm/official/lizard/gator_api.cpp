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

#include "gator_api.hpp"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include "gator_constants.hpp"
#include "gator_message.hpp"
#include "gatord_xml_reader.hpp"
#include "message_util.hpp"

namespace lizard
{
static const std::string ATTR_TYPE = "type";

static const std::string TAG_REQUEST = "request";

static const std::string VALUE_CAPTURED = "captured";
static const std::string VALUE_CONFIGURATION = "configuration";
static const std::string VALUE_COUNTERS = "counters";
static const std::string VALUE_DEFAULTS = "defaults";
static const std::string VALUE_EVENTS = "events";

GatorApi::GatorApi(char *hostname, const uint32_t port,
                   std::vector<LizardCounter> &availableCounters, LizardCounterDataStore &dataStore)
    : m_host(hostname),
      m_port(port),
      m_connection(NULL),
      m_availableCounters(availableCounters),
      m_data(dataStore)
{
}

GatorApi::~GatorApi()
{
  if(m_host)
  {
    free(m_host);
  }

  if(m_connection)
  {
    delete m_connection;
    m_connection = NULL;
  }
}

bool GatorApi::createConnection()
{
  m_connection = lizard::Socket::createConnection(m_host, m_port);
  if(m_connection == NULL)
  {
    return false;
  }
  return true;
}

void GatorApi::destroyConnection()
{
  Socket::destroyConnection(m_connection);
  m_connection = NULL;
}

bool GatorApi::init(uint32_t &counterId)
{
  std::string eventsXml = requestEvents();
  std::string countersXml = requestCounters();

  // Store all events supported by gatord
  std::vector<GatordXML::EventCategory> gatordEvents =
      GatordXML::parseEvents(eventsXml.c_str(), eventsXml.size());
  // Store available counters provided by gatord
  std::vector<std::string> gatordAvailableCounters =
      GatordXML::parseCounters(countersXml.c_str(), countersXml.size());

  std::vector<std::string>::const_iterator countersStart = gatordAvailableCounters.begin();
  std::vector<std::string>::const_iterator countersEnd = gatordAvailableCounters.end();

  uint32_t counterNum = counterId;

  for(GatordXML::EventCategory &category : gatordEvents)
  {
    for(GatordXML::Event &event : category.events)
    {
      if(std::find(countersStart, countersEnd, event.counter) != countersEnd)
      {
        // found the counter
        LizardCounter::ClassType classType = event.eventClass == CLASS_ABSOLUTE
                                                 ? LizardCounter::CLASS_ABSOLUTE
                                                 : LizardCounter::CLASS_DELTA;
        m_availableCounters.emplace_back(++counterId, event.counter.c_str(), event.name.c_str(),
                                         event.title.c_str(), event.description.c_str(),
                                         category.name.c_str(), event.multiplier, event.units,
                                         classType, LizardCounter::SOURCE_GATORD);
      }
    }
  }

  return counterId > counterNum;
}

bool GatorApi::setupCapturedCounters()
{
  std::string capturedXml = requestCaptured();
  std::vector<GatordXML::CapturedCounter> capturedCounters =
      GatordXML::parseCapturedCounters(capturedXml.c_str(), capturedXml.size());

  if(capturedCounters.empty())
  {
    return false;
  }

  for(GatordXML::CapturedCounter captured : capturedCounters)
  {
    for(LizardCounter &counter : m_availableCounters)
    {
      if(captured.type == counter.key())
      {
        counter.setInternalKey(captured.key);
        break;
      }
    }
  }

  return true;
}

bool GatorApi::sendConfiguration(const std::vector<LizardCounter> &enabledCounters)
{
  std::string xml = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  xml += "<configurations revision=\"3\">\n";
  for(uint32_t idx = 0; idx < enabledCounters.size(); idx++)
  {
    const lizard::LizardCounter &cnt = enabledCounters[idx];
    xml += "<configuration counter=\"" + std::string(cnt.key()) + "\" />\n";
  }
  xml += "</configurations>";
  sendXml(xml);
  uint8_t response = getResponse();
  if(response != RESPONSE_ACK)
  {
    return false;
  }
  return true;
}

bool GatorApi::sendSession()
{
  std::string xml = std::string(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<session call_stack_unwinding=\"no\" "
      "parse_debug_info=\"no\" version=\"1\" high_resolution=\"no\" buffer_mode=\"streaming\" "
      "sample_rate=\"normal\" duration=\"0\" target_address=\"localhost\" live_rate=\"100\" "
      "stop_gator=\"no\">\n<energy_capture version=\"1\" type=\"none\">\n<channel id=\"0\" "
      "resistance=\"20\" power=\"yes\"/>\n</energy_capture></session>\n");
  sendXml(xml);
  uint8_t response = getResponse();
  if(response != RESPONSE_ACK)
  {
    return false;
  }
  return true;
}

void GatorApi::sendDisconnect()
{
  sendCommand(COMMAND_DISCONNECT);
  getResponse();
}

std::string GatorApi::requestCounters()
{
  requestXml(VALUE_COUNTERS);
  return getXmlResponse();
}

std::string GatorApi::requestEvents()
{
  requestXml(VALUE_EVENTS);
  return getXmlResponse();
}

std::string GatorApi::requestConfiguration()
{
  requestXml(VALUE_CONFIGURATION);
  return getXmlResponse();
}

std::string GatorApi::requestDefaults()
{
  requestXml(VALUE_DEFAULTS);
  return getXmlResponse();
}

std::string GatorApi::requestCaptured()
{
  requestXml(VALUE_CAPTURED);
  return getXmlResponse();
}

void GatorApi::requestXml(const std::string &attributeValue)
{
  std::string xml = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml += "<" + TAG_REQUEST + " " + ATTR_TYPE + "=" + attributeValue + "/>";

  uint8_t header[5];
  header[0] = COMMAND_REQUEST_XML;
  writeLEInt(header + 1, xml.size());

  size_t byteSent;
  m_connection->send(header, sizeof(header), &byteSent);
  m_connection->send(xml.c_str(), xml.size(), &byteSent);
}

void GatorApi::sendXml(const std::string &xml)
{
  uint8_t header[5];
  header[0] = COMMAND_DELIVER_XML;
  writeLEInt(header + 1, xml.size());

  size_t byteSent;
  m_connection->send(header, sizeof(header), &byteSent);
  m_connection->send(xml.c_str(), xml.size(), &byteSent);
}

uint8_t GatorApi::getResponse()
{
  uint8_t responseType;
  uint32_t responseSize;
  getResponseHeader(&responseType, &responseSize);
  return responseType;
}

std::string GatorApi::getXmlResponse()
{
  uint8_t responseType;
  uint32_t responseSize;
  getResponseHeader(&responseType, &responseSize);

  if(responseType != RESPONSE_XML)
    return "";

  std::vector<char> responseXml(responseSize);

  size_t bytesRecv;
  m_connection->receiveAll(responseXml.data(), responseSize, &bytesRecv);

  std::string xml(responseXml.begin(), responseXml.end());

  return xml;
}

void GatorApi::sendCommand(uint8_t command)
{
  uint8_t message[5];
  message[0] = command;
  writeLEInt(message + 1, 0);
  size_t byteSent;
  m_connection->send(message, sizeof(message), &byteSent);
}

Socket::Result GatorApi::getResponseHeader(uint8_t *responseType, uint32_t *responseSize)
{
  uint8_t responseHeader[5];
  size_t bytesRecv;
  Socket::Result result = m_connection->receive(responseHeader, sizeof(responseHeader), &bytesRecv);
  if(result == Socket::Result::SUCCESS)
  {
    *responseType = responseHeader[0];
    *responseSize = readLEInt(responseHeader + 1);
  }
  return result;
}

bool GatorApi::sendVersion()
{
  char *msg_version = (char *)GATOR_PROTOCOL_VERSION;
  char *msg_streamline = (char *)STREAMLINE;

  size_t bytesSent;
  m_connection->send(msg_version, strlen(msg_version), &bytesSent);
  m_connection->send(msg_streamline, strlen(msg_streamline), &bytesSent);

  // "GATOR 670\n"
  size_t bytesRecv = 0;
  size_t size = 10;
  char buffer[size];
  m_connection->receiveAll(buffer, sizeof(buffer), &bytesRecv);

  std::string gator_pattern = "GATOR ";
  if(bytesRecv != size || std::strncmp(buffer, gator_pattern.c_str(), gator_pattern.length()) != 0)
  {
    return false;
  }

  sendCommand(COMMAND_PING);
  uint8_t response = getResponse();
  if(response != RESPONSE_ACK)
  {
    return false;
  }

  return true;
}

bool GatorApi::resendConfiguration(const std::vector<LizardCounter> &enabledGatorCounters)
{
  if(enabledGatorCounters.empty())
  {
    return false;
  }

  if(!createConnection())
  {
    return false;
  }

  if(!sendVersion())
  {
    destroyConnection();
    return false;
  }

  if(!sendConfiguration(enabledGatorCounters))
  {
    sendDisconnect();
    destroyConnection();
    return false;
  }

  sendDisconnect();
  destroyConnection();

  return true;
}

bool GatorApi::startSession()
{
  if(!createConnection())
  {
    return false;
  }
  if(!sendVersion())
  {
    destroyConnection();
    return false;
  }
  if(!sendSession())
  {
    sendDisconnect();
    destroyConnection();
    return false;
  }

  if(!setupCapturedCounters())
  {
    sendDisconnect();
    destroyConnection();
  }

  return true;
}

void GatorApi::startCapture()
{
  sendCommand(COMMAND_APC_START);
}

void GatorApi::stopCapture()
{
  sendCommand(COMMAND_APC_STOP);
}

GatorApi::MessageResult GatorApi::readMessage(GatorMessage &message)
{
  uint8_t responseType;
  uint32_t responseSize;

  Socket::Result result = getResponseHeader(&responseType, &responseSize);
  if(result == Socket::Result::SUCCESS)
  {
    message.setType(responseType);
    if(responseSize > 0)
    {
      std::vector<uint8_t> response(responseSize);
      size_t bytesRecv;
      result = m_connection->receiveAll(response.data(), responseSize, &bytesRecv);
      if(result == Socket::Result::SUCCESS)
      {
        message.setData(response);
        return GatorApi::MessageResult::SUCCESS;
      }
    }
  }
  return GatorApi::MessageResult::ERROR;
}

void GatorApi::processMessage(GatorMessage &message)
{
  if(!message.getData().empty() && message.getType() == RESPONSE_APC_DATA &&
     message.getPackedInt() == FRAME_BLOCK_COUNTER)
  {
    processBlockCounter(message);
  }
}

void GatorApi::processBlockCounter(GatorMessage &message)
{
  (void) message.getPackedInt(); // skip first item in message

  while(message.hasRemaining())
  {
    int64_t key = message.getPackedInt();
    int64_t value = message.getPackedInt();

    if(isValidKey(key))
    {
      Value v;
      v.as_int = value;
      for(LizardCounter counter : m_availableCounters)
      {
        if(counter.internalKey() == (uint64_t)key)
        {
          m_data.addValue(counter.id(), v);
        }
      }
    }
  }
}

bool GatorApi::isValidKey(int64_t key)
{
  return key > 2;
}

} /* namespace lizard */
