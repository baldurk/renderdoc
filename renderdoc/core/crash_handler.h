/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

// currently breakpad crash-handler is only available on windows
#if ENABLED(RDOC_RELEASE) && ENABLED(RDOC_WIN32) && RENDERDOC_OFFICIAL_BUILD

#define RDOC_CRASH_HANDLER OPTION_ON

// breakpad
#include "breakpad/client/windows/common/ipc_protocol.h"
#include "breakpad/client/windows/handler/exception_handler.h"

class CrashHandler : public ICrashHandler
{
public:
  CrashHandler(ICrashHandler *existing)
  {
    m_ExHandler = NULL;

    google_breakpad::AppMemoryList mem;

    _CrtSetReportMode(_CRT_ASSERT, 0);

    if(existing)
    {
      CrashHandler *crash = ((CrashHandler *)existing);
      m_PipeName = crash->m_PipeName;
      mem = crash->m_ExHandler->QueryRegisteredAppMemory();
      RDCLOG("Re-using crash-handling server %s", m_PipeName.c_str());
      SAFE_DELETE(existing);
    }
    else
    {
      m_PipeName = NewPipeName();
      CreateCrashHandlingServer();
    }

    ///////////////////

    rdcstr dumpFolder = FileIO::GetTempFolderFilename() + "RenderDoc\\dumps\\a";
    FileIO::CreateParentDirectory(dumpFolder);
    dumpFolder.pop_back();
    dumpFolder.pop_back();

    MINIDUMP_TYPE dumpType = MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory);

    static google_breakpad::CustomInfoEntry breakpadCustomInfo[] = {
        google_breakpad::CustomInfoEntry(L"version", L""),
        google_breakpad::CustomInfoEntry(L"logpath", L""),
        google_breakpad::CustomInfoEntry(L"gitcommit", L""),
        google_breakpad::CustomInfoEntry(L"replaycrash",
                                         RenderDoc::Inst().IsReplayApp() ? L"1" : L"0"),
    };

    rdcwstr wideStr = StringFormat::UTF82Wide(rdcstr(FULL_VERSION_STRING));
    breakpadCustomInfo[0].set_value(wideStr.c_str());
    wideStr = StringFormat::UTF82Wide(rdcstr(RDCGETLOGFILE()));
    breakpadCustomInfo[1].set_value(wideStr.c_str());
    wideStr = StringFormat::UTF82Wide(rdcstr(GitVersionHash));
    breakpadCustomInfo[2].set_value(wideStr.c_str());

    google_breakpad::CustomClientInfo custom = {&breakpadCustomInfo[0],
                                                ARRAY_COUNT(breakpadCustomInfo)};

    RDCLOG("Connecting to server %s", m_PipeName.c_str());

    m_ExHandler = new google_breakpad::ExceptionHandler(
        StringFormat::UTF82Wide(dumpFolder).c_str(), NULL, NULL, NULL,
        google_breakpad::ExceptionHandler::HANDLER_ALL, dumpType,
        StringFormat::UTF82Wide(m_PipeName).c_str(), &custom);

    if(!m_ExHandler->IsOutOfProcess())
    {
      RDCWARN("Couldn't connect to existing breakpad server");

      SAFE_DELETE(m_ExHandler);

      m_PipeName = NewPipeName();

      CreateCrashHandlingServer();

      m_ExHandler = new google_breakpad::ExceptionHandler(
          StringFormat::UTF82Wide(dumpFolder).c_str(), NULL, NULL, NULL,
          google_breakpad::ExceptionHandler::HANDLER_ALL, dumpType,
          StringFormat::UTF82Wide(m_PipeName).c_str(), &custom);

      if(!m_ExHandler->IsOutOfProcess())
        RDCERR("Couldn't launch and connect to new breakpad server");
    }

    m_ExHandler->set_handle_debug_exceptions(true);

    for(size_t i = 0; i < mem.size(); i++)
      m_ExHandler->RegisterAppMemory((void *)mem[i].ptr, mem[i].length);
  }

  void CreateCrashHandlingServer()
  {
    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    RDCEraseEl(pi);
    RDCEraseEl(si);

    // hide the console window
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    HANDLE waitEvent = CreateEventA(NULL, TRUE, FALSE, "RENDERDOC_CRASHHANDLE");

    rdcstr dllpath;
    FileIO::GetLibraryFilename(dllpath);

    rdcstr cmdline = "\"";
    cmdline += get_dirname(dllpath);
    cmdline += "/renderdoccmd.exe\" crashhandle --pipe ";
    cmdline += m_PipeName;

    rdcwstr params = StringFormat::UTF82Wide(cmdline);

    BOOL ret = CreateProcessW(NULL, params.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL,
                              NULL, &si, &pi);

    if(!ret)
      RDCERR("Failed to create crashhandle server: %d", GetLastError());

    {
      SCOPED_TIMER("Waiting for crash handling server");
      WaitForSingleObject(waitEvent, 400);
    }

    CloseHandle(waitEvent);

    RDCLOG("Created crash-handling server %s", m_PipeName.c_str());
  }

  virtual ~CrashHandler() { SAFE_DELETE(m_ExHandler); }
  void RegisterMemoryRegion(void *mem, size_t size) { m_ExHandler->RegisterAppMemory(mem, size); }
  void UnregisterMemoryRegion(void *mem) { m_ExHandler->UnregisterAppMemory(mem); }
private:
  rdcstr m_PipeName;
  google_breakpad::ExceptionHandler *m_ExHandler;

  rdcstr NewPipeName()
  {
    return StringFormat::Fmt("\\\\.\\pipe\\RenderDocBreakpadServer%llu", Timing::GetTick());
  }
};

#else

#define RDOC_CRASH_HANDLER OPTION_OFF

#endif
