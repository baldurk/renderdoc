/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_NOSTDOUT
#include "catch.hpp"
#include "api/replay/renderdoc_replay.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"

struct AppVeyorListener : Catch::TestEventListenerBase
{
  using TestEventListenerBase::TestEventListenerBase;    // inherit constructor

  bool enabled = false;
  std::string hostname;
  uint16_t port = 0;

  virtual void testRunStarting(Catch::TestRunInfo const &testRunInfo)
  {
    const char *url = Process::GetEnvVariable("APPVEYOR_API_URL");

    if(url)
    {
      if(strncmp(url, "http://", 7))
        return;

      url += 7;

      const char *sep = strchr(url, ':');

      if(!sep)
        return;

      hostname = std::string(url, sep);

      url = sep + 1;

      port = 0;
      while(*url >= '0' && *url <= '9')
      {
        port *= 10;
        port += int((*url) - '0');
        url++;
      }

      Network::Socket *sock = Network::CreateClientSocket(hostname.c_str(), port, 10);

      if(sock)
        enabled = true;

      SAFE_DELETE(sock);
    }
  }

  std::string curTest;
  std::vector<std::string> sectionStack;

  virtual void testCaseStarting(Catch::TestCaseInfo const &testInfo) { curTest = testInfo.name; }
  virtual void sectionStarting(Catch::SectionInfo const &sectionInfo)
  {
    if(curTest == sectionInfo.name)
      return;

    sectionStack.push_back(sectionInfo.name);

    if(enabled)
    {
      Network::Socket *sock = Network::CreateClientSocket(hostname.c_str(), port, 10);

      if(sock)
      {
        std::string req = MakeHTTPRequest();
        sock->SendDataBlocking(req.c_str(), (uint32_t)req.size());
      }

      SAFE_DELETE(sock);
    }
  }

  std::string errorList;

  virtual bool assertionEnded(Catch::AssertionStats const &assertionStats)
  {
    using namespace Catch;

    if(!assertionStats.assertionResult.isOk())
    {
      std::ostringstream msg;
      msg << assertionStats.assertionResult.getSourceInfo() << ": ";

      switch(assertionStats.assertionResult.getResultType())
      {
        case ResultWas::ExpressionFailed: msg << "Failed"; break;
        case ResultWas::ThrewException: msg << "Threw exception"; break;
        case ResultWas::FatalErrorCondition: msg << "Fatal error'd"; break;
        case ResultWas::DidntThrowException: msg << "Didn't throw expected exception"; break;
        case ResultWas::ExplicitFailure: msg << "Explicitly failed"; break;

        case ResultWas::Ok:
        case ResultWas::Info:
        case ResultWas::Warning:
        case ResultWas::Unknown:
        case ResultWas::FailureBit:
        case ResultWas::Exception: break;
      }

      if(assertionStats.infoMessages.size() >= 1)
        msg << " with message(s):";
      for(auto it = assertionStats.infoMessages.begin(); it != assertionStats.infoMessages.end(); ++it)
        msg << "\n" << it->message;

      if(assertionStats.assertionResult.hasExpression())
      {
        msg << "\n  " << assertionStats.assertionResult.getExpressionInMacro()
            << "\nwith expansion:\n  " << assertionStats.assertionResult.getExpandedExpression()
            << "\n";
      }

      errorList += msg.str();
    }

    return true;
  }

  virtual void sectionEnded(Catch::SectionStats const &sectionStats)
  {
    if(curTest == sectionStats.sectionInfo.name)
      return;

    if(enabled)
    {
      Network::Socket *sock = Network::CreateClientSocket(hostname.c_str(), port, 10);

      if(sock)
      {
        std::string req = MakeHTTPRequest(sectionStats.durationInSeconds * 1000.0,
                                          sectionStats.assertions.allOk());
        sock->SendDataBlocking(req.c_str(), (uint32_t)req.size());
      }

      errorList.clear();

      SAFE_DELETE(sock);
    }

    sectionStack.pop_back();
  }

private:
  std::string MakeHTTPRequest(double msDuration = -1.0, bool passed = false)
  {
    std::string json;

    bool update = msDuration >= 0.0;

    const char *outcome = "Running";

    if(update)
      outcome = passed ? "Passed" : "Failed";

    std::string testName;
    for(const std::string &section : sectionStack)
    {
      if(!testName.empty())
        testName += " > ";
      testName += section;
    }

    json = StringFormat::Fmt(R"(
{
    "testName": "%s",
    "testFramework": "Catch.hpp",
    "fileName": "%s",
    "outcome": "%s",
    "durationMilliseconds": "%.0f",
    "ErrorMessage": "%s",
    "ErrorStackTrace": "",
    "StdOut": "",
    "StdErr": ""
})",
                             testName.c_str(), curTest.c_str(), outcome,
                             RDCMAX(msDuration * 1000.0, 0.0), escape(trim(errorList)).c_str());

    std::string http;
    http += StringFormat::Fmt("%s /api/tests HTTP/1.1\r\n", update ? "PUT" : "POST");
    http += StringFormat::Fmt("Host: %s\r\n", hostname.c_str());
    http += "Connection: close\r\n";
    http += "Content-Type: application/json\r\n";
    http += StringFormat::Fmt("Content-Length: %zu\r\n", json.size());
    http += "User-Agent: Catch.hpp appveyor updater\r\n";
    http += "\r\n";
    return http + json;
  }

  std::string escape(const std::string &input)
  {
    std::string ret = input;
    size_t i = ret.find_first_of("\"\n\\", 0);
    while(i != std::string::npos)
    {
      if(ret[i] == '"')
        ret.replace(i, 1, "\\\"");
      else if(ret[i] == '\\')
        ret.replace(i, 1, "\\\\");
      else if(ret[i] == '\n')
        ret.replace(i, 1, "\\n");

      i = ret.find_first_of("\"\n\\", i + 2);
    }

    return ret;
  }
};
CATCH_REGISTER_LISTENER(AppVeyorListener)

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

std::ostream *stream = NULL;

namespace Catch
{
std::ostream &cout()
{
  return *stream;
}
std::ostream &cerr()
{
  return *stream;
}
std::ostream &clog()
{
  return *stream;
}
}

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunUnitTests(const rdcstr &command,
                                                                 const rdcarray<rdcstr> &args)
{
  LogOutputter logbuf;
  std::ostream logstream(&logbuf);
  stream = &logstream;

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