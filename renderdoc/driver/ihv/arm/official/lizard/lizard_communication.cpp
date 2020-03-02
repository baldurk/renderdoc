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

#include "lizard_communication.hpp"

#include "gator_api.hpp"
#include "gator_message.hpp"

namespace lizard
{
CommunicationThread::CommunicationThread(GatorApi &gatorApi) : m_gatorApi(gatorApi), m_thread()
{
}

CommunicationThread::~CommunicationThread()
{
}

void CommunicationThread::start(void)
{
  m_shouldWork = true;
  m_gatorApi.startCapture();
  m_thread = std::thread(&CommunicationThread::worker, this);
}

void CommunicationThread::stop(void)
{
  m_shouldWork = false;
  m_gatorApi.stopCapture();
  m_thread.join();
}

void CommunicationThread::worker(void)
{
  bool hasData = false;

  while(m_shouldWork != false || hasData)
  {
    GatorMessage message;
    GatorApi::MessageResult result = m_gatorApi.readMessage(message);
    hasData = result == GatorApi::MessageResult::SUCCESS;
    if(hasData)
    {
      m_gatorApi.processMessage(message);
    }
  }
}
}
