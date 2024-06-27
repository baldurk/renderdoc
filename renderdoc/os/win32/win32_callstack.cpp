/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#define DBGHELP_TRANSLATE_TCHAR

// must be separate so that it's included first and not sorted by clang-format
#include <windows.h>

#include <Psapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <tchar.h>
#include <algorithm>
#include "common/formatting.h"
#include "core/core.h"
#include "core/settings.h"
#include "dbghelp/dbghelp.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

#include "dia2_stubs.h"

#include <string>

// magic value that the config vars default to. If we see this, we'll upgrade the old .ini
#define UNINITIALISED_VAR ":uninit:var:"

RDOC_CONFIG(rdcarray<rdcstr>, Win32_Callstacks_IgnoreList, {},
            "Modules which are ignored when resolving callstacks on Windows.");
RDOC_CONFIG(rdcstr, Win32_Callstacks_MSDIAPath, UNINITIALISED_VAR, "The path to the msdia dll.");

struct AddrInfo
{
  rdcstr funcName;
  rdcstr fileName;
  unsigned long lineNum = 0;
};

typedef BOOL(CALLBACK *PSYM_ENUMMODULES_CALLBACK64W)(__in PCWSTR ModuleName, __in DWORD64 BaseOfDll,
                                                     __in_opt PVOID UserContext);

typedef BOOL(WINAPI *PSYMINITIALIZEW)(HANDLE, PCTSTR, BOOL);
typedef BOOL(WINAPI *PSYMREFRESHMODULELIST)(HANDLE);
typedef BOOL(WINAPI *PSYMENUMERATEMODULES64W)(HANDLE, PSYM_ENUMMODULES_CALLBACK64W, PVOID);
typedef BOOL(WINAPI *PSYMGETMODULEINFO64W)(HANDLE, DWORD64, PIMAGEHLP_MODULEW64);
typedef BOOL(WINAPI *PSYMFINDFILEINPATHW)(__in HANDLE hprocess, __in_opt PCWSTR SearchPath,
                                          __in PCWSTR FileName, __in_opt PVOID id, __in DWORD two,
                                          __in DWORD three, __in DWORD flags,
                                          __out_ecount(MAX_PATH + 1) PWSTR FoundFile,
                                          __in_opt PFINDFILEINPATHCALLBACKW callback,
                                          __in_opt PVOID context);

PSYMINITIALIZEW dynSymInitializeW = NULL;
PSYMREFRESHMODULELIST dynSymRefreshModuleList = NULL;
PSYMENUMERATEMODULES64W dynSymEnumerateModules64W = NULL;
PSYMGETMODULEINFO64W dynSymGetModuleInfo64W = NULL;
PSYMFINDFILEINPATHW dynSymFindFileInPathW = NULL;

namespace DIA2
{
struct Module
{
  Module(IDiaDataSource *src = NULL, IDiaSession *sess = NULL) : pSource(src), pSession(sess) {}
  IDiaDataSource *pSource;
  IDiaSession *pSession;
};

rdcarray<Module> modules;

rdcwstr GetSymSearchPath()
{
  std::wstring sympath;

  DWORD len = GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", NULL, 0);

  if(len == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
  {
    // set up a default sympath to look up MS's symbol servers and cache them locally in
    // RenderDoc's appdata folder.
    PWSTR appDataPath;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_UNEXPAND,
                         NULL, &appDataPath);
    rdcwstr appdata = appDataPath;
    CoTaskMemFree(appDataPath);

    sympath = L".;";
    sympath += appdata.c_str();
    sympath += L"\\renderdoc\\symbols;SRV*";
    sympath += appdata.c_str();
    sympath += L"\\renderdoc\\symbols\\symsrv*http://msdl.microsoft.com/download/symbols";

    return sympath.c_str();
  }

  sympath.resize(len + 1);
  GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", &sympath[0], len);
  return sympath.c_str();
}

rdcstr LookupModule(const rdcstr &modName, GUID guid, DWORD age)
{
  rdcstr ret = modName;

  rdcstr pdbName = get_basename(ret);

  if(pdbName.find(".pdb") == -1 && pdbName.find(".PDB") == -1)
  {
    if(pdbName.contains('.'))
      pdbName = strip_extension(pdbName) + ".pdb";
  }

  if(dynSymFindFileInPathW != NULL)
  {
    rdcwstr sympath = GetSymSearchPath();

    wchar_t path[MAX_PATH + 1] = {0};
    BOOL found = dynSymFindFileInPathW(GetCurrentProcess(), sympath.c_str(),
                                       StringFormat::UTF82Wide(pdbName).c_str(), &guid, age, 0,
                                       SSRVOPT_GUIDPTR, path, NULL, NULL);
    DWORD err = GetLastError();
    (void)err;    // for debugging only

    if(found == TRUE && path[0] != 0)
      ret = StringFormat::Wide2UTF8(path);
  }

  return ret;
}

rdcwstr msdiapath = L"msdia140.dll";

HRESULT MakeDiaDataSource(IDiaDataSource **source)
{
  // might need to CoInitialize on this thread
  CoInitialize(NULL);

  // try creating the object just normally, hopefully it's registered
  HRESULT hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IDiaDataSource), (void **)source);

  if(SUCCEEDED(hr))
    return hr;

  // if not registered, try loading the DLL from the given path
  HMODULE mod = LoadLibraryW(msdiapath.c_str());

  if(mod == NULL)
  {
    RDCDEBUG("Couldn't load DIA from '%ls'", msdiapath.c_str());
    return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
  }

  // if it loaded, we should be able to manually load up DIA

  // thanks to https://stackoverflow.com/a/2466264/4070143 for details of how to 'manually' create
  // a COM object from msdia

  typedef decltype(&DllGetClassObject) PFN_DllGetClassObject;

  PFN_DllGetClassObject getobj = (PFN_DllGetClassObject)GetProcAddress(mod, "DllGetClassObject");

  if(!getobj)
    return HRESULT_FROM_WIN32(GetLastError());

  IClassFactory *pClassFactory;
  hr = getobj(__uuidof(DiaSource), IID_IClassFactory, (void **)&pClassFactory);
  if(FAILED(hr))
    return hr;

  if(!pClassFactory)
    return E_NOINTERFACE;

  hr = pClassFactory->CreateInstance(NULL, __uuidof(IDiaDataSource), (void **)source);
  if(FAILED(hr))
    return hr;

  if(*source == NULL)
    return E_UNEXPECTED;

  pClassFactory->Release();

  return S_OK;
}

uint32_t GetModule(const rdcwstr &pdbName, GUID guid, DWORD age)
{
  Module m(NULL, NULL);

  HRESULT hr = MakeDiaDataSource(&m.pSource);

  if(FAILED(hr))
  {
    return 0;
  }

  // check this pdb is the one we expected from our chunk
  if(guid.Data1 == 0 && guid.Data2 == 0)
  {
    hr = m.pSource->loadDataFromPdb(pdbName.c_str());
  }
  else
  {
    hr = m.pSource->loadAndValidateDataFromPdb(pdbName.c_str(), &guid, 0, age);
  }

  if(SUCCEEDED(hr))
  {
    // open the session
    hr = m.pSource->openSession(&m.pSession);
    if(FAILED(hr))
    {
      m.pSource->Release();
      return 0;
    }

    modules.push_back(m);

    return uint32_t(modules.size());
  }

  m.pSource->Release();

  return 0;
}

void Release(uint32_t module)
{
  if(module > 0 && module <= modules.size())
  {
    SAFE_RELEASE(modules[module - 1].pSession);
    SAFE_RELEASE(modules[module - 1].pSource);
  }
}

void SetBaseAddress(uint32_t module, uint64_t addr)
{
  if(module > 0 && module <= modules.size())
    modules[module - 1].pSession->put_loadAddress(addr);
}

AddrInfo GetAddr(uint32_t module, uint64_t addr)
{
  AddrInfo ret;
  ZeroMemory(&ret, sizeof(ret));

  if(module > 0 && module <= modules.size())
  {
    SymTagEnum tag = SymTagFunction;
    IDiaSymbol *pFunc = NULL;
    HRESULT hr = modules[module - 1].pSession->findSymbolByVA(addr, tag, &pFunc);

    if(hr != S_OK)
    {
      if(pFunc)
        pFunc->Release();

      // try again looking for public symbols
      tag = SymTagPublicSymbol;
      hr = modules[module - 1].pSession->findSymbolByVA(addr, tag, &pFunc);

      if(hr != S_OK)
      {
        if(pFunc)
          pFunc->Release();
        return ret;
      }
    }

    DWORD opts = 0;
    opts |= UNDNAME_NO_LEADING_UNDERSCORES;
    opts |= UNDNAME_NO_MS_KEYWORDS;
    opts |= UNDNAME_NO_FUNCTION_RETURNS;
    opts |= UNDNAME_NO_ALLOCATION_MODEL;
    opts |= UNDNAME_NO_ALLOCATION_LANGUAGE;
    opts |= UNDNAME_NO_THISTYPE;
    opts |= UNDNAME_NO_ACCESS_SPECIFIERS;
    opts |= UNDNAME_NO_THROW_SIGNATURES;
    opts |= UNDNAME_NO_MEMBER_TYPE;
    opts |= UNDNAME_NO_RETURN_UDT_MODEL;
    opts |= UNDNAME_32_BIT_DECODE;
    opts |= UNDNAME_NO_LEADING_UNDERSCORES;

    // first try undecorated name
    BSTR file;
    hr = pFunc->get_undecoratedNameEx(opts, &file);

    // if not, just try name
    if(hr != S_OK)
    {
      hr = pFunc->get_name(&file);

      if(hr != S_OK)
      {
        pFunc->Release();
        SysFreeString(file);
        return ret;
      }

      ret.funcName = StringFormat::Wide2UTF8(file);
    }
    else
    {
      ret.funcName = StringFormat::Wide2UTF8(file);

      int voidoffs = ret.funcName.find("(void)");

      // remove stupid (void) for empty parameters
      if(voidoffs >= 0)
      {
        ret.funcName.erase(voidoffs + 1, ~0U);
        ret.funcName.push_back(')');
      }
    }

    pFunc->Release();
    pFunc = NULL;

    SysFreeString(file);

    // find the line numbers touched by this address.
    IDiaEnumLineNumbers *lines = NULL;
    hr = modules[module - 1].pSession->findLinesByVA(addr, DWORD(4), &lines);
    if(FAILED(hr))
    {
      if(lines)
        lines->Release();
      return ret;
    }

    IDiaLineNumber *line = NULL;
    ULONG count = 0;

    // just take the first one
    if(SUCCEEDED(lines->Next(1, &line, &count)) && count == 1)
    {
      IDiaSourceFile *dia_source = NULL;
      hr = line->get_sourceFile(&dia_source);
      if(FAILED(hr))
      {
        line->Release();
        lines->Release();
        if(dia_source)
          dia_source->Release();
        return ret;
      }

      hr = dia_source->get_fileName(&file);
      if(FAILED(hr))
      {
        line->Release();
        lines->Release();
        dia_source->Release();
        return ret;
      }

      ret.fileName = StringFormat::Wide2UTF8(file);

      SysFreeString(file);

      dia_source->Release();
      dia_source = NULL;

      DWORD line_num = 0;
      hr = line->get_lineNumber(&line_num);
      if(FAILED(hr))
      {
        line->Release();
        lines->Release();
        return ret;
      }

      ret.lineNum = line_num;

      line->Release();
    }

    lines->Release();
  }

  return ret;
}

void Init()
{
  CoInitialize(NULL);

  if(dynSymInitializeW)
    dynSymInitializeW(GetCurrentProcess(), GetSymSearchPath().c_str(), TRUE);
}

};    // namespace DIA2

class Win32Callstack : public Callstack::Stackwalk
{
public:
  Win32Callstack();
  Win32Callstack(DWORD64 *calls, size_t numLevels);
  ~Win32Callstack();

  void Set(DWORD64 *calls, size_t numLevels);

  size_t NumLevels() const { return m_AddrStack.size(); }
  const uint64_t *GetAddrs() const { return &m_AddrStack[0]; }
private:
  Win32Callstack(const Callstack::Stackwalk &other);

  void Collect();

  rdcarray<DWORD64> m_AddrStack;
};

class Win32CallstackResolver : public Callstack::StackResolver
{
public:
  Win32CallstackResolver(bool interactive, byte *moduleDB, size_t DBSize,
                         RENDERDOC_ProgressCallback progress);
  ~Win32CallstackResolver();

  Callstack::AddressDetails GetAddr(uint64_t addr);

private:
  rdcstr pdbBrowse(rdcstr startingPoint);

  struct Module
  {
    rdcstr name;
    DWORD64 base;
    DWORD size;

    uint32_t moduleId;
  };

  rdcarray<rdcstr> pdbRememberedPaths;
  rdcarray<rdcstr> pdbIgnores;
  rdcarray<Module> modules;

  char pipeMessageBuf[2048];
};

///////////////////////////////////////////////////

void *renderdocBase = NULL;
uint32_t renderdocSize = 0;

// gives us an address to identify this dll with
static int dllLocator = 0;

static bool InitDbgHelp()
{
  static bool doinit = true;
  static bool ret = false;

  if(!doinit)
    return ret;

  doinit = false;

  HMODULE module = NULL;

  // can't reliably co-exist with dbghelp already being used in the process
  if(GetModuleHandleA("dbghelp.dll") != NULL)
  {
    RDCLOG(
        "dbghelp.dll is already loaded, can't guarantee thread-safety against application use. "
        "Callstack collection disabled");
    ret = false;
    return false;
  }
  else
  {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_BASE_NAME) ".dll"), path, MAX_PATH - 1);

    wchar_t *slash = wcsrchr(path, '\\');

    if(slash)
    {
      *slash = 0;
    }
    else
    {
      slash = wcsrchr(path, '/');

      if(slash == 0)
      {
        ret = false;
        return false;
      }

      *slash = 0;
    }
#if ENABLED(RDOC_X64)
    wcscat_s(path, L"/dbghelp.dll");
#else
    wcscat_s(path, L"/dbghelp.dll");
#endif

    module = LoadLibraryW(path);
  }

  if(!module)
  {
    RDCWARN("Couldn't open dbghelp.dll");
    ret = false;
    return false;
  }

  dynSymInitializeW = (PSYMINITIALIZEW)GetProcAddress(module, "SymInitializeW");
  dynSymEnumerateModules64W =
      (PSYMENUMERATEMODULES64W)GetProcAddress(module, "SymEnumerateModulesW64");
  dynSymRefreshModuleList = (PSYMREFRESHMODULELIST)GetProcAddress(module, "SymRefreshModuleList");
  dynSymGetModuleInfo64W = (PSYMGETMODULEINFO64W)GetProcAddress(module, "SymGetModuleInfoW64");
  dynSymFindFileInPathW = (PSYMFINDFILEINPATHW)GetProcAddress(module, "SymFindFileInPathW");

  if(!dynSymInitializeW || !dynSymRefreshModuleList || !dynSymEnumerateModules64W ||
     !dynSymGetModuleInfo64W)
  {
    RDCERR("Couldn't get some dbghelp function");
    ret = false;
    return ret;
  }

  dynSymInitializeW(GetCurrentProcess(), L".", TRUE);

  HMODULE hModule = NULL;
  GetModuleHandleEx(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCTSTR)&dllLocator, &hModule);

  if(hModule != NULL)
  {
    MODULEINFO modinfo = {0};

    BOOL result = GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(modinfo));

    if(result != FALSE)
    {
      renderdocBase = modinfo.lpBaseOfDll;
      renderdocSize = modinfo.SizeOfImage;
    }
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    DIA2::Init();
  }

  ret = true;
  return ret;
}

///////////////////////////////////////////////////

struct CV_INFO_PDB70
{
  DWORD CvSignature;
  GUID Signature;
  DWORD Age;
  char PdbFileName[1024];
};

struct EnumBuf
{
  byte *bufPtr;
  size_t size;
};

struct EnumModChunk
{
  DWORD64 base;
  DWORD size;
  DWORD age;
  GUID guid;
  size_t imageNameLen;
  // WCHAR* imageName; // follows (null terminated)
};

BOOL CALLBACK EnumModule(PCWSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext)
{
  EnumBuf *buf = (EnumBuf *)UserContext;

  IMAGEHLP_MODULEW64 ModInfo;
  ZeroMemory(&ModInfo, sizeof(IMAGEHLP_MODULEW64));
  ModInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);
  BOOL res = dynSymGetModuleInfo64W(GetCurrentProcess(), BaseOfDll, &ModInfo);
  DWORD err = GetLastError();

  if(!res)
  {
    RDCERR("Couldn't get module info for %ls: %d", ModuleName, err);
    return FALSE;
  }

  EnumModChunk chunk;

  chunk.base = BaseOfDll;
  chunk.size = ModInfo.ImageSize;

  // can't get symbol the easy way, let's walk the PE structure.
  // Thanks to http://msdn.microsoft.com/en-us/library/ms809762.aspx
  // and also http://www.debuginfo.com/articles/debuginfomatch.html
  if(ModInfo.PdbSig70.Data1 == 0 && ModInfo.SymType == SymPdb)
  {
    BYTE *addr32 = (BYTE *)BaseOfDll;

#ifndef WIN64
    RDCASSERT((BaseOfDll & 0xffffffff00000000ULL) ==
              0x0ULL);    // we're downcasting technically, make sure.
#endif

    PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)addr32;

    char *PE00 = (char *)(addr32 + dosheader->e_lfanew);
    PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00 + 4);
    PIMAGE_OPTIONAL_HEADER optHeader =
        (PIMAGE_OPTIONAL_HEADER)((BYTE *)fileHeader + sizeof(IMAGE_FILE_HEADER));

    DWORD dbgOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
    PIMAGE_DEBUG_DIRECTORY debugDir = (PIMAGE_DEBUG_DIRECTORY)(addr32 + dbgOffset);

    CV_INFO_PDB70 *pdb70Data = (CV_INFO_PDB70 *)(addr32 + debugDir->AddressOfRawData);

    chunk.age = pdb70Data->Age;
    chunk.guid = pdb70Data->Signature;
  }
  else
  {
    chunk.age = ModInfo.PdbAge;
    chunk.guid = ModInfo.PdbSig70;
  }

  WCHAR *pdb = ModInfo.CVData;

  if(pdb[0] == 0)
    pdb = ModInfo.ImageName;

  chunk.imageNameLen = wcslen(pdb) + 1;    // include null terminator

  if(buf->bufPtr)
  {
    memcpy(buf->bufPtr, &chunk, sizeof(EnumModChunk));
    buf->bufPtr += sizeof(EnumModChunk);
    memcpy(buf->bufPtr, pdb, chunk.imageNameLen * sizeof(WCHAR));
    buf->bufPtr += chunk.imageNameLen * sizeof(WCHAR);
  }

  buf->size += sizeof(EnumModChunk) + chunk.imageNameLen * sizeof(WCHAR);

  return TRUE;
}

void Win32Callstack::Collect()
{
  rdcarray<PVOID> stack32;

  stack32.resize(64);

  USHORT num = RtlCaptureStackBackTrace(0, 63, &stack32[0], NULL);

  stack32.resize(num);

  while(!stack32.empty() && (uint64_t)stack32[0] >= (uint64_t)renderdocBase &&
        (uint64_t)stack32[0] <= (uint64_t)renderdocBase + renderdocSize)
  {
    stack32.erase(0, 1);
  }

  m_AddrStack.resize(stack32.size());
  for(size_t i = 0; i < stack32.size(); i++)
    m_AddrStack[i] = (DWORD64)stack32[i];
}

Win32Callstack::Win32Callstack()
{
  bool ret = InitDbgHelp();

  if(ret && renderdocBase != NULL)
    Collect();
}

Win32Callstack::Win32Callstack(DWORD64 *calls, size_t numLevels)
{
  Set(calls, numLevels);
}

void Win32Callstack::Set(DWORD64 *calls, size_t numLevels)
{
  m_AddrStack.resize(numLevels);
  for(size_t i = 0; i < numLevels; i++)
    m_AddrStack[i] = calls[i];
}

Win32Callstack::~Win32Callstack()
{
}

rdcstr Win32CallstackResolver::pdbBrowse(rdcstr startingPoint)
{
  OPENFILENAMEW ofn;
  RDCEraseMem(&ofn, sizeof(ofn));

  wchar_t outBuf[MAX_PATH * 2];
  wcscpy_s(outBuf, StringFormat::UTF82Wide(startingPoint).c_str());

  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.lpstrTitle = L"Locate PDB File";
  ofn.lpstrFilter = L"PDB File\0*.pdb\0";
  ofn.lpstrFile = outBuf;
  ofn.nMaxFile = MAX_PATH * 2 - 1;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;    // | OFN_ENABLEINCLUDENOTIFY | OFN_ENABLEHOOK

  BOOL ret = GetOpenFileNameW(&ofn);

  if(ret == FALSE)
    return "";

  return StringFormat::Wide2UTF8(outBuf);
}

Win32CallstackResolver::Win32CallstackResolver(bool interactive, byte *moduleDB, size_t DBSize,
                                               RENDERDOC_ProgressCallback progress)
{
  if(Win32_Callstacks_MSDIAPath() == UNINITIALISED_VAR)
  {
    RDCLOG("Updating callstack resolve config from ini");

    rdcwstr configPath = StringFormat::UTF82Wide(FileIO::GetAppFolderFilename("config.ini"));
    {
      FILE *f = NULL;
      _wfopen_s(&f, configPath.c_str(), L"a");
      if(f)
        fclose(f);
    }

    DWORD sz = 2048;
    wchar_t *inputBuf = new wchar_t[sz];

    for(;;)
    {
      DWORD read =
          GetPrivateProfileStringW(L"renderdoc", L"ignores", NULL, inputBuf, sz, configPath.c_str());

      if(read == sz - 1)
      {
        sz *= 2;
        delete[] inputBuf;
        inputBuf = new wchar_t[sz];
        continue;
      }

      break;
    }

    rdcstr ignores = StringFormat::Wide2UTF8(inputBuf);

    {
      DWORD read = GetPrivateProfileStringW(L"renderdoc", L"msdiapath", NULL, inputBuf, sz,
                                            configPath.c_str());

      if(read > 0)
        DIA2::msdiapath = inputBuf;
    }

    delete[] inputBuf;

    split(ignores, pdbIgnores, ';');
  }
  else
  {
    pdbIgnores = Win32_Callstacks_IgnoreList();
    DIA2::msdiapath = StringFormat::UTF82Wide(Win32_Callstacks_MSDIAPath());
  }

  // check we can create an IDiaDataSource
  {
    IDiaDataSource *source = NULL;
    HRESULT hr = DIA2::MakeDiaDataSource(&source);

    if(FAILED(hr) || source == NULL)
    {
// try a bunch of common locations to try and find it without prompting the user.
// These are just best-guess based on the defaults, since actually locating VS itself is super
// complex in its own right.
// We only check 2017 locations since 2015 and before seemed to register the dll like you'd
// expect.
#if ENABLED(RDOC_X64)
#define DIA140 L"bin\\amd64\\msdia140.dll"
#else
#define DIA140 L"bin\\msdia140.dll"
#endif
      const wchar_t *DIApaths[] = {
          // try to see if it's just in the PATH somewhere
          L"msdia140.dll",
          // otherwise try each VS2017/2019/2022 SKU
          L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\DIA SDK\\" DIA140,
          L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\DIA SDK\\" DIA140,
          L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\DIA SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\DIA SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\DIA SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\DIA "
          L"SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\DIA SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\DIA SDK\\" DIA140,
          L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\DIA "
          L"SDK\\" DIA140,
      };

      for(size_t i = 0; i < ARRAY_COUNT(DIApaths); i++)
      {
        SAFE_RELEASE(source);

        DIA2::msdiapath = DIApaths[i];

        hr = DIA2::MakeDiaDataSource(&source);

        if(SUCCEEDED(hr) && source != NULL)
          break;
      }

      while(FAILED(hr) || source == NULL)
      {
        SAFE_RELEASE(source);

        // can't manually locate msdia if we're non-interactive
        if(!interactive)
          return;

        int ret = MessageBoxW(NULL,
                              L"Couldn't initialise DIA - it may not be registered. "
                              L"In VS2017 and above, DIA is not registered by default.\n\n"
                              L"Please locate msdia140.dll on your system, it may be in a DIA SDK "
                              L"subdirectory under your VS2017 installation.",
                              L"msdia140.dll not registered", MB_OKCANCEL);

        if(ret == IDCANCEL)
          return;

        OPENFILENAMEW ofn;
        RDCEraseMem(&ofn, sizeof(ofn));

        wchar_t outBuf[MAX_PATH * 2] = {};

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.lpstrTitle = L"Locate msdia140.dll";
        ofn.lpstrFilter = L"msdia140.dll\0msdia140.dll\0";
        ofn.lpstrFile = outBuf;
        ofn.nMaxFile = MAX_PATH * 2 - 1;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        BOOL opened = GetOpenFileNameW(&ofn);

        if(opened == FALSE)
          return;

        DIA2::msdiapath = outBuf;

        hr = DIA2::MakeDiaDataSource(&source);
      }
    }

    SAFE_RELEASE(source);
  }

  byte *chunks = moduleDB + 8;
  byte *end = chunks + DBSize - 8;

  EnumModChunk *chunk = (EnumModChunk *)(chunks);
  WCHAR *modName = (WCHAR *)(chunks + sizeof(EnumModChunk));

  // loop over all our modules
  for(; chunks < end; chunks += sizeof(EnumModChunk) + (chunk->imageNameLen) * sizeof(WCHAR))
  {
    chunk = (EnumModChunk *)chunks;
    modName = (WCHAR *)(chunks + sizeof(EnumModChunk));

    if(progress)
      progress(float(chunks - moduleDB) / float(end - moduleDB));

    Module m;

    m.name = StringFormat::Wide2UTF8(modName);
    m.base = chunk->base;
    m.size = chunk->size;
    m.moduleId = 0;

    if(pdbIgnores.contains(m.name))
    {
      RDCWARN("Not attempting to get symbols for %s", m.name.c_str());

      modules.push_back(m);
      continue;
    }

    // get default pdb (this also looks up symbol server etc)
    // Always done in unicode
    rdcstr defaultPdb = DIA2::LookupModule(m.name, chunk->guid, chunk->age);

    // strip newline
    if(defaultPdb != "" && defaultPdb[defaultPdb.length() - 1] == '\n')
      defaultPdb.pop_back();

    // if we didn't even get a default pdb we'll have to prompt first time through
    bool failed = false;

    if(defaultPdb == "")
    {
      defaultPdb = strlower(get_basename(m.name));

      int it = defaultPdb.find(".dll");
      if(it >= 0)
      {
        defaultPdb[it + 1] = 'p';
        defaultPdb[it + 2] = 'd';
        defaultPdb[it + 3] = 'b';
      }

      it = defaultPdb.find(".exe");
      if(it >= 0)
      {
        defaultPdb[it + 1] = 'p';
        defaultPdb[it + 2] = 'd';
        defaultPdb[it + 3] = 'b';
      }
      failed = true;
    }

    rdcstr pdbName = defaultPdb;

    int fallbackIdx = -1;

    while(m.moduleId == 0)
    {
      if(failed)
      {
        fallbackIdx++;
        // try one of the folders we've been given, just in case the symbols
        // are there
        if(fallbackIdx < (int)pdbRememberedPaths.size())
        {
          pdbName = pdbRememberedPaths[fallbackIdx] + "\\" + get_basename(pdbName);
        }
        else
        {
          pdbName = get_dirname(defaultPdb) + "\\" + get_basename(defaultPdb);

          // prompt for new pdbName, unless it's renderdoc or dbghelp, or we're non-interactive
          if(pdbName.contains("renderdoc.") || pdbName.contains("dbghelp.") ||
             pdbName.contains("symsrv.") || !interactive)
            pdbName = "";
          else
            pdbName = pdbBrowse(pdbName);

          // user cancelled, just don't load this pdb
          if(pdbName == "")
            break;
        }

        failed = false;
      }

      m.moduleId = DIA2::GetModule(StringFormat::UTF82Wide(pdbName), chunk->guid, chunk->age);

      if(m.moduleId == 0)
      {
        failed = true;
      }
      else
      {
        if(fallbackIdx >= (int)pdbRememberedPaths.size())
        {
          rdcstr dir = get_dirname(pdbName);
          if(!pdbRememberedPaths.contains(dir))
            pdbRememberedPaths.push_back(dir);
        }
      }
    }

    // didn't load the pdb? go to the next module.
    if(m.moduleId == 0)
    {
      modules.push_back(m);    // still add the module, with 0 module id

      RDCWARN("Couldn't get symbols for %s", m.name.c_str());

      // silently ignore renderdoc.dll, dbghelp.dll, and symsrv.dll without asking to permanently
      // ignore
      if(m.name.contains("renderdoc.") || m.name.contains("dbghelp.") || m.name.contains("symsrv."))
        continue;

      // if we're not interactive, just continue
      if(!interactive)
        continue;

      rdcstr text = StringFormat::Fmt("Do you want to permanently ignore this file?\nPath: %s",
                                      m.name.c_str());

      int ret = MessageBoxA(NULL, text.c_str(), "Ignore this pdb?", MB_YESNO);

      if(ret == IDYES)
        pdbIgnores.push_back(m.name);

      continue;
    }

    if(progress)
      progress(RDCMIN(1.0f, float(chunks - moduleDB) / float(end - moduleDB)));

    DIA2::SetBaseAddress(m.moduleId, chunk->base);

    RDCLOG("Loaded Symbols for %s", m.name.c_str());

    modules.push_back(m);
  }

  SDObject *ignoreList = RenderDoc::Inst().SetConfigSetting("Win32.Callstacks.IgnoreList");
  ignoreList->DeleteChildren();
  ignoreList->ReserveChildren(pdbIgnores.size());
  for(rdcstr &i : pdbIgnores)
    ignoreList->AddAndOwnChild(makeSDString("$el"_lit, i));
  RenderDoc::Inst().SetConfigSetting("Win32.Callstacks.MSDIAPath")->data.str =
      StringFormat::Wide2UTF8(DIA2::msdiapath);

  RENDERDOC_SaveConfigSettings();
}

Win32CallstackResolver::~Win32CallstackResolver()
{
  for(size_t i = 0; i < modules.size(); i++)
    DIA2::Release(modules[i].moduleId);
}

Callstack::AddressDetails Win32CallstackResolver::GetAddr(DWORD64 addr)
{
  AddrInfo info;

  info.fileName = "Unknown";
  info.funcName = StringFormat::Fmt("0x%08llx", addr);

  for(size_t i = 0; i < modules.size(); i++)
  {
    DWORD64 base = modules[i].base;
    DWORD size = modules[i].size;
    if(addr > base && addr < base + size)
    {
      if(modules[i].moduleId != 0)
        info = DIA2::GetAddr(modules[i].moduleId, addr);

      // if we didn't get a filename, default to the module name
      if(modules[i].moduleId == 0 || info.fileName[0] == 0)
        info.fileName = modules[i].name;

      if(modules[i].moduleId == 0 || info.funcName[0] == 0)
      {
        // if we didn't get a function name, at least indicate
        // the module it came from, and an offset
        info.funcName = get_basename(info.fileName);

        int offs = info.funcName.find(".pdb");

        if(offs >= 0)
        {
          info.funcName.erase(offs + 1, ~0U);

          if(i == 0)
            info.funcName += "exe";
          else
            info.funcName += "dll";
        }

        info.funcName = StringFormat::Fmt("%s+0x%08llx", info.funcName.c_str(), addr - base);
      }

      break;
    }
  }

  Callstack::AddressDetails ret;
  ret.filename = info.fileName;
  ret.function = info.funcName;
  ret.line = info.lineNum;

  return ret;
}

////////////////////////////////////////////////////////////////////
// implement public interface

namespace Callstack
{
void Init()
{
  // if we're capturing, need to initialise immediately to claim ownership and be ready to collect
  // callstacks. On replay we can do this later when needed.
  if(!RenderDoc::Inst().IsReplayApp())
    ::InitDbgHelp();
}

Stackwalk *Collect()
{
  return new Win32Callstack();
}

Stackwalk *Create()
{
  return new Win32Callstack(NULL, 0);
}

StackResolver *MakeResolver(bool interactive, byte *moduleDB, size_t DBSize,
                            RENDERDOC_ProgressCallback progress)
{
  if(DBSize < 8 || memcmp(moduleDB, "WN32CALL", 8) != 0)
  {
    RDCWARN("Can't load callstack resolve for this log. Possibly from another platform?");
    return NULL;
  }

  // initialise dbghelp if we haven't already
  ::InitDbgHelp();

  return new Win32CallstackResolver(interactive, moduleDB, DBSize, progress);
}

bool GetLoadedModules(byte *buf, size_t &size)
{
  EnumBuf e;
  e.bufPtr = buf;
  e.size = 0;

  if(buf)
  {
    memcpy(buf, "WN32CALL", 8);

    e.bufPtr += 8;
  }

  e.size += 8;

  bool inited = InitDbgHelp();

  if(inited)
  {
    dynSymRefreshModuleList(GetCurrentProcess());
    dynSymEnumerateModules64W(GetCurrentProcess(), &EnumModule, &e);
  }

  size = e.size;

  return true;
}
};    // namespace Callstack
