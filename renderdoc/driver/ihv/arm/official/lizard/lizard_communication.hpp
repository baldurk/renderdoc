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

#ifndef LIB_LIZARD_COMMUNICATION_HPP
#define LIB_LIZARD_COMMUNICATION_HPP

#include <atomic>
#include <functional>
#include <map>
#include <thread>
#include <vector>

#include "gator_api.hpp"

namespace lizard
{
class CommunicationThread
{
public:
  CommunicationThread(GatorApi &gatorApi);
  ~CommunicationThread(void);

  void start(void);
  void stop(void);

private:
  void worker(void);

  std::atomic<bool> m_shouldWork;
  GatorApi &m_gatorApi;
  std::thread m_thread;
};

} /* namespace lizard */

#endif /* LIB_LIZARD_COMMUNICATION_HPP */
