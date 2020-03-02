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

#ifndef LIB_LIZARD_HPP
#define LIB_LIZARD_HPP

#include <cstdint>
#include <vector>

#include <lizard_counter.hpp>

namespace lizard
{
class GatorApi;
class CommunicationThread;
class HwcPipeApi;
class HwcPipeThread;

class Lizard
{
public:
  Lizard(void);

  ~Lizard(void);

  /**
   * Set the gatord hostname and port number to connect to.
   *
   * :param hostname: should be an ipv4 compatible address or hostname.
   * :param port: port number.
   */
  bool configure(const char *hostname, uint32_t port);

  /**
   * Query the list of available counters into an array.
   *
   * Usage:
   * {
   *   Lizard lzd; // lzd should be configured after initialization!
   *
   *   int count = lzd.availableCountersCount();
   *   std::vector<LizardCounter> counters;
   *   counters.reserve(count);
   *
   *   int copied = lzd.availableCounters(&counters[0], count));
   * }
   *
   * :param outCounter: output array of available counters.
   * :param arraySize: size (element count) of the `outCounter` array.
   * :returns: the number of `LizardCounter` elements copied into the `outCounter` array.
   */
  uint32_t availableCounters(LizardCounter *outCounters, uint32_t arraySize);

  /**
   * Query the list of available counters (non-copy).
   *
   * Usage:
   * {
   *   Lizard lzd; // lzd should be configured after initialization!
   *
   *   int count = lzd.availableCountersCount();
   *   LizardCounter *counterslzd.availableCounters();
   * }
   *
   * :returns: a pointer to a `LizardCounter` array containing maximum `availableCountersCount()`
   * elements.
   */
  const LizardCounter *availableCounters(void) const { return &m_availableCounters[0]; }

  /**
   * Query the number of available counters;
   *
   * Usage:
   *  See `availableCounters`.
   *
   * :returns: number of `LizardCounter` elements available.
   */
  uint32_t availableCountersCount(void) const;

  /**
   * Enable a set of counters based on Id values.
   *
   * Usage:
   * {
   *   Lizard lzd; // lzd should be configured after initialization!
   *
   *   std::vector<LizardCounter> counters;
   *   // fill counters with required counters.
   *   // ...
   *   // Enable the first three counters.
   *   std::vector<LizardCounterId> ids;
   *   ids.reserve(3);
   *   lizard::countersToIds(&counters[0], &ids[0], 3);
   *
   *   lzd.enableCounters(&ids[0], 3);
   * }
   *
   * :param counterIds: array of `LizardCounterId` values to enable.
   * :param arraySize: size (element count) of the `counterIds` array.
   */
  void enableCounters(const LizardCounterId *counterIds, uint32_t arraySize);

  void disableCounters(const LizardCounterId *counterIds, uint32_t arraySize);

  /**
   * Start the capture of the enabled counters.
   *
   * This operation does not block. Underlying communication is done in a different thread.
   *
   * :returns: true if the capture was started correctly, otherwise false.
   */
  bool startCapture(void);

  /**
   * Stops the capture.
   *
   * Captured data can be accessed via another method.
   */
  void endCapture(void);

  LizardCounterData *readCounter(const LizardCounterId counterId) const;

  size_t readCounterInt(const LizardCounterId counterId, int64_t *values) const;
  size_t readCounterDouble(const LizardCounterId counterId, double *values) const;

  const LizardCounter *getCounterInfo(const LizardCounterId counterId) const;

private:
  bool configureGatord(const char *hostname, uint32_t port);
  bool configureHwcPipe(void);
  bool startGatord(void);
  void stopGatord(void);
  bool startHwcPipe(void);
  void stopHwcPipe(void);

  uint32_t m_idCounter;

  std::vector<LizardCounter> m_availableCounters;
  std::vector<bool> m_enabledCounters;

  GatorApi *m_gatorApi;
  CommunicationThread *m_comm;

  HwcPipeApi *m_HwcPipeApi;
  HwcPipeThread *m_HwcPipe_comm;

  bool m_configuredGatord;
  bool m_configuredHwcPipe;

  LizardCounterDataStore m_dataStore;
};

} /* namepsace lizard */

#endif /* LIB_LIZARD_HPP */
