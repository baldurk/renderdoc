/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2024 Baldur Karlsson
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

#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_NOSTDOUT

#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "catch.hpp"

// since we force use of ToStr for everything and don't allow using catch's stringstream (so that
// enums get forwarded to ToStr) we need to implement ToStr for one of Catch's structs.
template <>
rdcstr DoStringise(const Catch::SourceLineInfo &el)
{
  return StringFormat::Fmt("%s:%zu", el.file, el.line);
}

class LogOutputter : public std::stringbuf
{
public:
  LogOutputter() {}
  virtual int sync() override
  {
    std::string msg = this->str();
    OSUtility::WriteOutput(OSUtility::Output_DebugMon, msg.c_str());
    OSUtility::WriteOutput(OSUtility::Output_StdOut, msg.c_str());
    this->str("");
    return 0;
  }

  // force a sync on every output
  virtual std::streamsize xsputn(const char *s, std::streamsize n) override
  {
    std::streamsize ret = std::stringbuf::xsputn(s, n);
    sync();
    return ret;
  }
};

std::ostream *catch_stream = NULL;

namespace Catch
{
std::ostream &cout()
{
  return *catch_stream;
}
std::ostream &cerr()
{
  return *catch_stream;
}
std::ostream &clog()
{
  return *catch_stream;
}
}

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunUnitTests(const rdcstr &command,
                                                                 const rdcarray<rdcstr> &args)
{
  LogOutputter logbuf;
  std::ostream logstream(&logbuf);
  catch_stream = &logstream;

  Catch::Session session;

  session.configData().name = "RenderDoc";
  session.configData().shouldDebugBreak = OSUtility::DebuggerPresent();

  const char **argv = new const char *[args.size() + 1];
  argv[0] = command.c_str();
  for(size_t i = 0; i < args.size(); i++)
    argv[i + 1] = args[i].c_str();

  int ret = session.applyCommandLine(args.count() + 1, argv);

  delete[] argv;

  // command line error
  if(ret != 0)
    return ret;

  int numFailed = session.run();

  // Note that on unices only the lower 8 bits are usually used, clamping
  // the return value to 255 prevents false negative when some multiple
  // of 256 tests has failed
  return (numFailed < 0xff ? numFailed : 0xff);
}

#else

#include "api/replay/apidefs.h"
#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunUnitTests(const rdcstr &command,
                                                                 const rdcarray<rdcstr> &args)
{
  return 0;
}

#endif    // ENABLED(ENABLE_UNIT_TESTS)
