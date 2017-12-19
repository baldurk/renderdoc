/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "renderdoccmd.h"
#include <app/renderdoc_app.h>
#include <renderdocshim.h>
#include <windows.h>
#include <string>
#include <vector>
#include "miniz/miniz.h"
#include "resource.h"

#include <Psapi.h>
#include <tlhelp32.h>

using std::string;
using std::wstring;
using std::vector;

static std::string conv(const std::wstring &str)
{
  std::string ret;
  // worst case each char takes 4 bytes to encode
  ret.resize(str.size() * 4 + 1);

  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &ret[0], (int)ret.size(), NULL, NULL);

  ret.resize(strlen(ret.c_str()));

  return ret;
}

static std::wstring conv(const std::string &str)
{
  std::wstring ret;
  ret.resize(str.size() + 1);

  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &ret[0], int(ret.size() + 1));

  ret.resize(wcslen(ret.c_str()));

  return ret;
}

HINSTANCE hInstance = NULL;

#if defined(RELEASE)
#define CRASH_HANDLER 1
#else
#define CRASH_HANDLER 0
#endif

#if CRASH_HANDLER
// breakpad
#include "breakpad/client/windows/crash_generation/client_info.h"
#include "breakpad/client/windows/crash_generation/crash_generation_server.h"
#include "breakpad/common/windows/http_upload.h"

using google_breakpad::ClientInfo;
using google_breakpad::CrashGenerationServer;

bool exitServer = false;

wstring wdump = L"";
std::vector<google_breakpad::CustomInfoEntry> customInfo;

static void _cdecl OnClientCrashed(void *context, const ClientInfo *client_info,
                                   const wstring *dump_path)
{
  if(dump_path)
  {
    wdump = *dump_path;

    google_breakpad::CustomClientInfo custom = client_info->GetCustomInfo();

    for(size_t i = 0; i < custom.count; i++)
      customInfo.push_back(custom.entries[i]);
  }

  exitServer = true;
}

static void _cdecl OnClientExited(void *context, const ClientInfo *client_info)
{
  exitServer = true;
}
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if(msg == WM_CLOSE)
  {
    DestroyWindow(hwnd);
    return 0;
  }
  if(msg == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Daemonise()
{
  // nothing really to do, windows version of renderdoccmd is already 'detached'
}

void DisplayRendererPreview(IReplayController *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height)
{
  RECT wr = {0, 0, (LONG)width, (LONG)height};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

  HWND wnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"renderdoccmd", L"renderdoccmd", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                            NULL, NULL, hInstance, NULL);

  if(wnd == NULL)
    return;

  ShowWindow(wnd, SW_SHOW);
  UpdateWindow(wnd);

  IReplayOutput *out = renderer->CreateOutput(WindowingSystem::Win32, wnd, ReplayOutputType::Texture);

  out->SetTextureDisplay(displayCfg);

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));
  while(true)
  {
    // Check to see if any messages are waiting in the queue
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      // Translate the message and dispatch it to WindowProc()
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // If the message is WM_QUIT, exit the while loop
    if(msg.message == WM_QUIT)
      break;

    // set to random event beyond the end of the frame to ensure output is marked as dirty
    renderer->SetFrameEvent(10000000, true);
    out->Display();

    Sleep(40);
  }

  DestroyWindow(wnd);
}

struct UpgradeCommand : public Command
{
  UpgradeCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser) { parser.add<string>("path", 0, ""); }
  virtual const char *Description() { return "Internal use only!"; }
  virtual bool IsInternalOnly() { return true; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    wstring wide_path = conv(parser.get<string>("path"));

    // Wait for UI to exit
    Sleep(3000);

    mz_zip_archive zip;
    ZeroMemory(&zip, sizeof(zip));

    bool successful = false;
    wstring failReason;

    mz_bool b = mz_zip_reader_init_file(&zip, "./update.zip", 0);

    if(b)
    {
      mz_uint numfiles = mz_zip_reader_get_num_files(&zip);

      // first create directories
      for(mz_uint i = 0; i < numfiles; i++)
      {
        if(mz_zip_reader_is_file_a_directory(&zip, i))
        {
          mz_zip_archive_file_stat zstat;
          mz_zip_reader_file_stat(&zip, i, &zstat);

          const char *fn = zstat.m_filename;
          // skip first directory because it's RenderDoc_Version_Bitness/
          fn = strchr(fn, '/');
          if(fn)
            fn++;

          if(fn && *fn)
          {
            wchar_t conv[MAX_PATH] = {0};
            wchar_t *wfn = conv;

            // I know the zip only contains ASCII chars, just upcast
            while(*fn)
              *(wfn++) = wchar_t(*(fn++));

            wstring target = wide_path + conv;

            wfn = &target[0];

            // convert slashes because CreateDirectory barfs on
            // proper slashes.
            while(*(wfn++))
            {
              if(*wfn == L'/')
                *wfn = L'\\';
            }

            CreateDirectoryW(target.c_str(), NULL);
          }
        }
      }

      // next make sure we can get read+write access to every file. If not
      // one might be in use, but we definitely can't update it
      successful = true;

      for(mz_uint i = 0; successful && i < numfiles; i++)
      {
        if(!mz_zip_reader_is_file_a_directory(&zip, i))
        {
          mz_zip_archive_file_stat zstat;
          mz_zip_reader_file_stat(&zip, i, &zstat);

          const char *fn = zstat.m_filename;
          // skip first directory because it's RenderDoc_Version_Bitness/
          fn = strchr(fn, '/');
          if(fn)
            fn++;

          if(fn && *fn)
          {
            wchar_t conv[MAX_PATH] = {0};
            wchar_t *wfn = conv;

            // I know the zip only contains ASCII chars, just upcast
            while(*fn)
              *(wfn++) = wchar_t(*(fn++));

            wstring target = wide_path + conv;

            wfn = &target[0];

            // convert slashes just to be consistent
            while(*(wfn++))
            {
              if(*wfn == L'/')
                *wfn = L'\\';
            }

            FILE *f = NULL;
            _wfopen_s(&f, target.c_str(), L"a+");
            if(!f)
            {
              failReason = L"\"Couldn't modify an install file - likely file is in use.\"";
              successful = false;
            }
            else
            {
              fclose(f);
            }
          }
        }
      }

      for(mz_uint i = 0; successful && i < numfiles; i++)
      {
        if(!mz_zip_reader_is_file_a_directory(&zip, i))
        {
          mz_zip_archive_file_stat zstat;
          mz_zip_reader_file_stat(&zip, i, &zstat);

          const char *fn = zstat.m_filename;
          // skip first directory because it's RenderDoc_Version_Bitness/
          fn = strchr(fn, '/');
          if(fn)
            fn++;

          if(fn && *fn)
          {
            wchar_t conv[MAX_PATH] = {0};
            wchar_t *wfn = conv;

            // I know the zip only contains ASCII chars, just upcast
            while(*fn)
              *(wfn++) = wchar_t(*(fn++));

            wstring target = wide_path + conv;

            wfn = &target[0];

            // convert slashes just to be consistent
            while(*(wfn++))
            {
              if(*wfn == L'/')
                *wfn = L'\\';
            }

            mz_zip_reader_extract_to_wfile(&zip, i, target.c_str(), 0);
          }
        }
      }
    }
    else
    {
      failReason = L"\"Failed to open update .zip file - possibly corrupted.\"";
    }

    // run original UI exe and tell it an update succeeded
    wstring cmdline = L"\"";
    cmdline += wide_path;
    cmdline += L"/qrenderdoc.exe\" ";
    if(successful)
      cmdline += L"--updatedone";
    else
      cmdline += L"--updatefailed " + failReason;

    wchar_t *paramsAlloc = new wchar_t[512];

    ZeroMemory(paramsAlloc, sizeof(wchar_t) * 512);

    wcscpy_s(paramsAlloc, sizeof(wchar_t) * 511, cmdline.c_str());

    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));

    CreateProcessW(NULL, paramsAlloc, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    if(pi.dwProcessId != 0)
    {
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }

    delete[] paramsAlloc;

    return 0;
  }
};

#if CRASH_HANDLER
struct CrashHandlerCommand : public Command
{
  CrashHandlerCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Internal use only!"; }
  virtual bool IsInternalOnly() { return true; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    CrashGenerationServer *crashServer = NULL;

    wchar_t tempPath[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH - 1, tempPath);

    Sleep(100);

    wstring dumpFolder = tempPath;
    dumpFolder += L"RenderDoc/dumps";

    CreateDirectoryW(dumpFolder.c_str(), NULL);

    crashServer = new CrashGenerationServer(L"\\\\.\\pipe\\RenderDocBreakpadServer", NULL, NULL,
                                            NULL, OnClientCrashed, NULL, OnClientExited, NULL, NULL,
                                            NULL, true, &dumpFolder);

    if(!crashServer->Start())
    {
      delete crashServer;
      crashServer = NULL;
      return 1;
    }

    HANDLE readyEvent = CreateEventA(NULL, TRUE, FALSE, "RENDERDOC_CRASHHANDLE");

    if(readyEvent != NULL)
    {
      SetEvent(readyEvent);

      CloseHandle(readyEvent);
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while(!exitServer)
    {
      // Check to see if any messages are waiting in the queue
      while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      {
        // Translate the message and dispatch it to WindowProc()
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // If the message is WM_QUIT, exit the while loop
      if(msg.message == WM_QUIT)
        break;

      Sleep(100);
    }

    delete crashServer;
    crashServer = NULL;

    std::wstring wlogpath;

    if(!wdump.empty())
    {
      string report = "{\n";

      for(size_t i = 0; i < customInfo.size(); i++)
      {
        wstring name = customInfo[i].name;
        wstring val = customInfo[i].value;

        if(name == L"logpath")
        {
          wlogpath = val;
        }
        else if(name == L"ptime")
        {
          // breakpad uptime, ignore.
        }
        else
        {
          report += "  \"" + string(name.begin(), name.end()) + "\": \"" +
                    string(val.begin(), val.end()) + "\",\n";
        }
      }

      rdcstr reportPath;

      RENDERDOC_CreateBugReport(conv(wlogpath).c_str(), conv(wdump).c_str(), reportPath);

      for(size_t i = 0; i < reportPath.size(); i++)
        if(reportPath[i] == '\\')
          reportPath[i] = '/';

      report += "  \n\"report\": \"" + std::string(reportPath) + "\"\n";
      report += "}\n";

      {
        wstring destjson = dumpFolder + L"\\report.json";

        FILE *f = NULL;
        _wfopen_s(&f, destjson.c_str(), L"w");
        fputs(report.c_str(), f);
        fclose(f);

        wchar_t *paramsAlloc = new wchar_t[512];

        ZeroMemory(paramsAlloc, sizeof(wchar_t) * 512);

        GetModuleFileNameW(NULL, paramsAlloc, 511);

        wchar_t *lastSlash = wcsrchr(paramsAlloc, '\\');

        if(lastSlash)
          *lastSlash = 0;

        std::wstring exepath = paramsAlloc;

        ZeroMemory(paramsAlloc, sizeof(wchar_t) * 512);

        _snwprintf_s(paramsAlloc, 511, 511, L"%s/qrenderdoc.exe --crash %s", exepath.c_str(),
                     destjson.c_str());

        PROCESS_INFORMATION pi;
        STARTUPINFOW si;
        ZeroMemory(&pi, sizeof(pi));
        ZeroMemory(&si, sizeof(si));

        BOOL success =
            CreateProcessW(NULL, paramsAlloc, NULL, NULL, FALSE, 0, NULL, exepath.c_str(), &si, &pi);

        if(success && pi.hProcess)
        {
          WaitForSingleObject(pi.hProcess, INFINITE);
        }

        if(pi.hProcess)
          CloseHandle(pi.hProcess);
        if(pi.hThread)
          CloseHandle(pi.hThread);

        std::wstring wreport = conv(std::string(report));

        DeleteFileW(wreport.c_str());
      }
    }

    if(!wdump.empty())
      DeleteFileW(wdump.c_str());

    if(!wlogpath.empty())
      DeleteFileW(wlogpath.c_str());

    return 0;
  }
};
#endif

struct GlobalHookCommand : public Command
{
  GlobalHookCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<string>("match", 0, "");
    parser.add<string>("logfile", 0, "");
    parser.add<string>("debuglog", 0, "");
    parser.add<string>("capopts", 0, "");
  }
  virtual const char *Description() { return "Internal use only!"; }
  virtual bool IsInternalOnly() { return true; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    wstring wpathmatch = conv(parser.get<string>("match"));
    string logfile = parser.get<string>("logfile");
    string debuglog = parser.get<string>("debuglog");

    CaptureOptions cmdopts;
    readCapOpts(parser.get<string>("capopts").c_str(), &cmdopts);

    // make sure the user doesn't accidentally run this with 'a' as a parameter or something.
    // "a.exe" is over 4 characters so this limit should not be a problem.
    if(wpathmatch.length() < 4)
    {
      std::cerr
          << "globalhook path match is too short/general. Danger of matching too many processes!"
          << std::endl;
      return 1;
    }

    wchar_t rdocpath[1024];

    // fetch path to our matching renderdoc.dll
    HMODULE rdoc = GetModuleHandleA("renderdoc.dll");

    if(rdoc == NULL)
    {
      std::cerr << "globalhook couldn't find renderdoc.dll!" << std::endl;
      return 1;
    }

    GetModuleFileNameW(rdoc, rdocpath, _countof(rdocpath) - 1);
    FreeLibrary(rdoc);

    // Create stdin pipe from parent program, to stay open until requested to close
    HANDLE pipe = GetStdHandle(STD_INPUT_HANDLE);

    if(pipe == INVALID_HANDLE_VALUE)
    {
      std::cerr << "globalhook couldn't open stdin pipe.\n" << std::endl;
      return 1;
    }

    HANDLE datahandle = OpenFileMappingA(FILE_MAP_READ, FALSE, GLOBAL_HOOK_DATA_NAME);

    if(datahandle != NULL)
    {
      CloseHandle(pipe);
      CloseHandle(datahandle);
      std::cerr << "globalhook found pre-existing global data, not creating second global hook."
                << std::endl;
      return 1;
    }

    datahandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ShimData),
                                    GLOBAL_HOOK_DATA_NAME);

    if(datahandle)
    {
      ShimData *shimdata = (ShimData *)MapViewOfFile(datahandle, FILE_MAP_WRITE | FILE_MAP_READ, 0,
                                                     0, sizeof(ShimData));

      if(shimdata)
      {
        memset(shimdata, 0, sizeof(ShimData));

        wcsncpy_s(shimdata->pathmatchstring, wpathmatch.c_str(), _TRUNCATE);
        wcsncpy_s(shimdata->rdocpath, rdocpath, _TRUNCATE);
        strncpy_s(shimdata->logfile, logfile.c_str(), _TRUNCATE);
        strncpy_s(shimdata->debuglog, debuglog.c_str(), _TRUNCATE);
        memcpy(shimdata->opts, &cmdopts, sizeof(CaptureOptions));

        static_assert(sizeof(CaptureOptions) <= sizeof(shimdata->opts),
                      "ShimData options is too small");

        // wait until a write comes in over the pipe
        char buf[16] = {0};
        DWORD read = 0;
        ReadFile(pipe, buf, 16, &read, NULL);

        UnmapViewOfFile(shimdata);
      }
      else
      {
        std::cerr << "globalhook couldn't map global data store." << std::endl;
      }

      CloseHandle(datahandle);
    }
    else
    {
      std::cerr << "globalhook couldn't create global data store." << std::endl;
    }

    CloseHandle(pipe);

    return 0;
  }
};

// from http://stackoverflow.com/q/29939893/4070143
// and http://stackoverflow.com/a/4570213/4070143

std::string getParentExe()
{
  DWORD pid = GetCurrentProcessId();
  HANDLE h = NULL;
  PROCESSENTRY32 pe = {0};
  DWORD ppid = 0;
  pe.dwSize = sizeof(PROCESSENTRY32);
  h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(Process32First(h, &pe))
  {
    do
    {
      if(pe.th32ProcessID == pid)
      {
        ppid = pe.th32ParentProcessID;
        break;
      }
    } while(Process32Next(h, &pe));
  }
  CloseHandle(h);

  if(ppid == 0)
    return "";

  h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ppid);
  if(h)
  {
    char buf[MAX_PATH];
    if(GetModuleFileNameExA(h, 0, buf, MAX_PATH))
    {
      CloseHandle(h);
      return buf;
    }
    CloseHandle(h);
  }

  return "";
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                    _In_ int nShowCmd)
{
  LPWSTR *wargv;
  int argc;

  if(AttachConsole(ATTACH_PARENT_PROCESS))
  {
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    std::cout.sync_with_stdio();
    std::cerr.sync_with_stdio();
  }

  wargv = CommandLineToArgvW(GetCommandLine(), &argc);

  std::vector<std::string> argv;

  argv.resize(argc);
  for(size_t i = 0; i < argv.size(); i++)
    argv[i] = conv(wstring(wargv[i]));

  if(argv.empty())
    argv.push_back("renderdoccmd");

  LocalFree(wargv);

  // if launched from cmd.exe, be friendly and redirect output
  std::string parent = getParentExe();
  for(size_t i = 0; i < parent.length(); i++)
  {
    parent[i] = tolower(parent[i]);
    if(parent[i] == '\\')
      parent[i] = '/';
  }

  if(strstr(parent.c_str(), "/cmd.exe") && AttachConsole(ATTACH_PARENT_PROCESS))
  {
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
  }

  hInstance = hInst;

  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = 0;
  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = L"renderdoccmd";
  wc.hIconSm = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));

  if(!RegisterClassEx(&wc))
  {
    return 1;
  }

  GlobalEnvironment env;

  // perform an upgrade of the UI
  add_command("upgrade", new UpgradeCommand(env));

#if CRASH_HANDLER
  // special WIN32 option for launching the crash handler
  add_command("crashhandle", new CrashHandlerCommand(env));
#endif

  // this installs a global windows hook pointing at renderdocshim*.dll that filters all running
  // processes and loads renderdoc.dll in the target one. In any other process it unloads as soon as
  // possible
  add_command("globalhook", new GlobalHookCommand(env));

  return renderdoccmd(env, argv);
}
