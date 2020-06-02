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

#ifndef GATOR_API_H
#define GATOR_API_H

#include <vector>

#include "../../gator_message.hpp"
#include "lizard_counter.hpp"
#include "socket.hpp"

namespace lizard
{
#define GATOR_PROTOCOL_VERSION "VERSION 671\n";
#define STREAMLINE "STREAMLINE\n";

class GatorApi
{
public:
  enum MessageResult
  {
    SUCCESS,
    ERROR,
  };

  GatorApi(char *host, const uint32_t port, std::vector<LizardCounter> &availableCounters,
           LizardCounterDataStore &dataStore);
  ~GatorApi();
  bool createConnection();
  void destroyConnection();
  bool init(uint32_t &counterId);
  bool sendVersion();
  void startCapture();
  void stopCapture();
  void sendDisconnect();
  MessageResult readMessage(GatorMessage &message);
  void processMessage(GatorMessage &message);
  bool resendConfiguration(const std::vector<LizardCounter> &enabledGatorCounters);
  bool startSession();

private:
  bool setupCapturedCounters();
  bool sendConfiguration(const std::vector<LizardCounter> &enabledCounters);
  bool sendSession();
  void requestXml(const std::string &attributeValue);
  void sendXml(const std::string &xml);
  uint8_t getResponse();
  std::string getXmlResponse();
  Socket::Result getResponseHeader(uint8_t *responseType, uint32_t *responseSize);
  void sendCommand(uint8_t command);
  std::string requestCounters();
  std::string requestEvents();
  std::string requestConfiguration();
  std::string requestDefaults();
  std::string requestCaptured();
  void processBlockCounter(GatorMessage &message);
  bool isValidKey(int64_t key);

  char *m_host;
  const uint32_t m_port;
  Socket *m_connection;
  std::vector<LizardCounter> &m_availableCounters;
  LizardCounterDataStore &m_data;
};

} /* namespace lizard */

#endif    // GATOR_API_H
