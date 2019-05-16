/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "android/android.h"
#include "api/replay/renderdoc_replay.h"
#include "api/replay/version.h"
#include "common/common.h"
#include "core/core.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "miniz/miniz.h"
#include "strings/string_utils.h"

// these entry points are for the replay/analysis side - not for the application.

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_NumVerticesPerPrimitive(Topology topology)
{
  // strips/loops/fans have the same number of indices for a single primitive
  // as their list friends
  switch(topology)
  {
    default:
    case Topology::Unknown: break;
    case Topology::PointList: return 1;
    case Topology::LineList:
    case Topology::LineStrip:
    case Topology::LineLoop: return 2;
    case Topology::TriangleList:
    case Topology::TriangleStrip:
    case Topology::TriangleFan: return 3;
    case Topology::LineList_Adj:
    case Topology::LineStrip_Adj: return 4;
    case Topology::TriangleList_Adj:
    case Topology::TriangleStrip_Adj: return 6;
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs: return PatchList_Count(topology);
  }

  return 0;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_VertexOffset(Topology topology,
                                                                      uint32_t primitive)
{
  // strips/loops/fans have the same number of indices for a single primitive
  // as their list friends
  switch(topology)
  {
    default:
    case Topology::Unknown:
    case Topology::PointList:
    case Topology::LineList:
    case Topology::TriangleList:
    case Topology::LineList_Adj:
    case Topology::TriangleList_Adj:
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs:
      // for all lists, it's just primitive * Topology_NumVerticesPerPrimitive(topology)
      break;
    case Topology::LineStrip:
    case Topology::LineLoop:
    case Topology::TriangleStrip:
    case Topology::LineStrip_Adj:
      // for strips, each new vertex creates a new primitive
      return primitive;
    case Topology::TriangleStrip_Adj:
      // triangle strip with adjacency is a special case as every other
      // vert is purely for adjacency so it's doubled
      return primitive * 2;
    case Topology::TriangleFan: RDCERR("Cannot get VertexOffset for triangle fan!"); break;
  }

  return primitive * RENDERDOC_NumVerticesPerPrimitive(topology);
}

extern "C" RENDERDOC_API float RENDERDOC_CC RENDERDOC_HalfToFloat(uint16_t half)
{
  return ConvertFromHalf(half);
}

extern "C" RENDERDOC_API uint16_t RENDERDOC_CC RENDERDOC_FloatToHalf(float f)
{
  return ConvertToHalf(f);
}

extern "C" RENDERDOC_API ICamera *RENDERDOC_CC RENDERDOC_InitCamera(CameraType type)
{
  return new Camera(type);
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetVersionString()
{
  return MAJOR_MINOR_VERSION_STRING;
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetCommitHash()
{
  return GitVersionHash;
}

extern "C" RENDERDOC_API DriverInformation RENDERDOC_CC RENDERDOC_GetDriverInformation(GraphicsAPI api)
{
  return RenderDoc::Inst().GetDriverInformation(api);
}

extern "C" RENDERDOC_API uint64_t RENDERDOC_CC RENDERDOC_GetCurrentProcessMemoryUsage()
{
  return Process::GetMemoryUsage();
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetConfigSetting(const char *name)
{
  return RenderDoc::Inst().GetConfigSetting(name).c_str();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetConfigSetting(const char *name,
                                                                      const char *value)
{
  RenderDoc::Inst().SetConfigSetting(name, value);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetColors(FloatVector darkChecker,
                                                               FloatVector lightChecker,
                                                               bool darkTheme)
{
  RenderDoc::Inst().SetDarkCheckerboardColor(
      Vec4f(darkChecker.x, darkChecker.y, darkChecker.z, darkChecker.w));
  RenderDoc::Inst().SetLightCheckerboardColor(
      Vec4f(lightChecker.x, lightChecker.y, lightChecker.z, lightChecker.w));
  RenderDoc::Inst().SetDarkTheme(darkTheme);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const char *log)
{
  if(log)
  {
    RDCLOGFILE(log);

    // need to recreate the crash handler to propagate the new log filename.
    if(RenderDoc::Inst().GetCrashHandler() != NULL)
      RenderDoc::Inst().RecreateCrashHandler();
  }
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text)
{
  rdclog_direct(Timing::GetUTCTime(), Process::GetCurrentPID(), LogType::Comment, "EXT", "external",
                0, "%s", text);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogType type, const char *project,
                                                                const char *file, unsigned int line,
                                                                const char *text)
{
  rdclog_direct(Timing::GetUTCTime(), Process::GetCurrentPID(), type, project ? project : "UNK?",
                file ? file : "unknown", line, "%s", text);

#if ENABLED(DEBUGBREAK_ON_ERROR_LOG)
  if(type == LogType::Error)
    RDCBREAK();
#endif

  if(type == LogType::Fatal)
    RDCDUMP();
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetLogFile()
{
  return RDCGETLOGFILE();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_InitGlobalEnv(GlobalEnvironment env,
                                                                   const rdcarray<rdcstr> &args)
{
  std::vector<std::string> argsVec;
  argsVec.reserve(args.size());
  for(const rdcstr &a : args)
    argsVec.push_back(a.c_str());

  RenderDoc::Inst().ProcessGlobalEnvironment(env, argsVec);

  if(RenderDoc::Inst().GetCrashHandler() == NULL)
    return;

  for(const rdcstr &s : args)
  {
    if(s == "--crash")
    {
      RenderDoc::Inst().UnloadCrashHandler();
      return;
    }
  }

  RenderDoc::Inst().RecreateCrashHandler();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CreateBugReport(const char *logfile,
                                                                     const char *dumpfile,
                                                                     rdcstr &report)
{
  mz_zip_archive zip;
  RDCEraseEl(zip);

  report = FileIO::GetTempFolderFilename() + "/renderdoc_report.zip";

  FileIO::Delete(report.c_str());

  mz_zip_writer_init_file(&zip, report.c_str(), 0);

  if(dumpfile && dumpfile[0])
    mz_zip_writer_add_file(&zip, "minidump.dmp", dumpfile, NULL, 0, MZ_BEST_COMPRESSION);

  if(logfile && logfile[0])
  {
    std::string contents = FileIO::logfile_readall(logfile);
    mz_zip_writer_add_mem(&zip, "error.log", contents.data(), contents.length(), MZ_BEST_COMPRESSION);
  }

  mz_zip_writer_finalize_archive(&zip);
  mz_zip_writer_end(&zip);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_RegisterMemoryRegion(void *base, size_t size)
{
  ICrashHandler *handler = RenderDoc::Inst().GetCrashHandler();

  if(handler)
    handler->RegisterMemoryRegion(base, size);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UnregisterMemoryRegion(void *base)
{
  ICrashHandler *handler = RenderDoc::Inst().GetCrashHandler();

  if(handler)
    handler->UnregisterMemoryRegion(base);
}

extern "C" RENDERDOC_API ExecuteResult RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                           const rdcarray<EnvironmentModification> &env, const char *capturefile,
                           const CaptureOptions &opts, bool waitForExit)
{
  return Process::LaunchAndInjectIntoProcess(app, workingDir, cmdLine, env, capturefile, opts,
                                             waitForExit != 0);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts)
{
  *opts = CaptureOptions();
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                     const char *capturefile,
                                                                     const CaptureOptions &opts)
{
  return Process::StartGlobalHook(pathmatch, capturefile, opts);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StopGlobalHook()
{
  Process::StopGlobalHook();
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsGlobalHookActive()
{
  return Process::IsGlobalHookActive();
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_CanGlobalHook()
{
  return Process::CanGlobalHook();
}

extern "C" RENDERDOC_API ExecuteResult RENDERDOC_CC
RENDERDOC_InjectIntoProcess(uint32_t pid, const rdcarray<EnvironmentModification> &env,
                            const char *capturefile, const CaptureOptions &opts, bool waitForExit)
{
  return Process::InjectIntoProcess(pid, env, capturefile, opts, waitForExit != 0);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem)
{
  free((void *)mem);
}

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz)
{
  return malloc((size_t)sz);
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteTargets(const char *host,
                                                                                uint32_t nextIdent)
{
  std::string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  // initial case is we're called with 0, start with the first port.
  // otherwise we're called with the last successful ident, so increment
  // before continuing to enumerate.
  if(nextIdent == 0)
    nextIdent = RenderDoc_FirstTargetControlPort;
  else
    nextIdent++;

  bool isAndroid = false;
  uint32_t lastIdent = RenderDoc_LastTargetControlPort;
  if(host != NULL && Android::IsHostADB(host))
  {
    int index = 0;
    std::string deviceID;
    Android::ExtractDeviceIDAndIndex(host, index, deviceID);

    isAndroid = true;

    // each subsequent device gets a new range of ports. The deviceID isn't needed since we already
    // forwarded the ports to the right devices.
    if(nextIdent == RenderDoc_FirstTargetControlPort)
      nextIdent += RenderDoc_AndroidPortOffset * (index + 1);
    lastIdent += RenderDoc_AndroidPortOffset * (index + 1);

    s = "127.0.0.1";
  }

  for(; nextIdent <= lastIdent; nextIdent++)
  {
    Network::Socket *sock = Network::CreateClientSocket(s.c_str(), (uint16_t)nextIdent, 250);

    if(sock)
    {
      if(isAndroid)
      {
        Threading::Sleep(100);
        (void)sock->IsRecvDataWaiting();
        if(!sock->Connected())
        {
          SAFE_DELETE(sock);
          return 0;
        }
      }

      SAFE_DELETE(sock);
      return nextIdent;
    }
  }

  // tried all idents remaining and found nothing
  return 0;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetDefaultRemoteServerPort()
{
  return RenderDoc_RemoteServerPort;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(
    const char *listenhost, uint32_t port, RENDERDOC_KillCallback killReplay,
    RENDERDOC_PreviewWindowCallback previewWindow)
{
  if(listenhost == NULL || listenhost[0] == 0)
    listenhost = "0.0.0.0";

  // ensure a sensible default if no callback is provided, that just never kills
  if(!killReplay)
    killReplay = []() { return false; };

  // ditto for preview windows
  if(!previewWindow)
    previewWindow = [](bool, const rdcarray<WindowingSystem> &) {
      WindowingData ret = {WindowingSystem::Unknown};
      return ret;
    };

  if(port == 0)
    port = RENDERDOC_GetDefaultRemoteServerPort();

  RenderDoc::Inst().BecomeRemoteServer(listenhost, (uint16_t)port, killReplay, previewWindow);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartSelfHostCapture(const char *dllname)
{
  void *module = Process::LoadModule(dllname);

  if(module == NULL)
    return;

  pRENDERDOC_GetAPI get =
      (pRENDERDOC_GetAPI)Process::GetFunctionAddress(module, "RENDERDOC_GetAPI");

  if(get == NULL)
    return;

  RENDERDOC_API_1_0_0 *rdoc = NULL;

  get(eRENDERDOC_API_Version_1_0_0, (void **)&rdoc);

  if(rdoc == NULL)
    return;

  rdoc->StartFrameCapture(NULL, NULL);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EndSelfHostCapture(const char *dllname)
{
  void *module = Process::LoadModule(dllname);

  if(module == NULL)
    return;

  pRENDERDOC_GetAPI get =
      (pRENDERDOC_GetAPI)Process::GetFunctionAddress(module, "RENDERDOC_GetAPI");

  if(get == NULL)
    return;

  RENDERDOC_API_1_0_0 *rdoc = NULL;

  get(eRENDERDOC_API_Version_1_0_0, (void **)&rdoc);

  if(rdoc == NULL)
    return;

  rdoc->EndFrameCapture(NULL, NULL);
}

extern "C" RENDERDOC_API bool RENDERDOC_CC
RENDERDOC_NeedVulkanLayerRegistration(VulkanLayerRegistrationInfo *info)
{
  VulkanLayerFlags flags = VulkanLayerFlags::NoFlags;
  std::vector<std::string> myJSONs;
  std::vector<std::string> otherJSONs;

  bool ret = RenderDoc::Inst().NeedVulkanLayerRegistration(flags, myJSONs, otherJSONs);

  if(info)
  {
    info->flags = flags;

    info->myJSONs.resize(myJSONs.size());
    for(size_t i = 0; i < myJSONs.size(); i++)
      info->myJSONs[i] = myJSONs[i];

    info->otherJSONs.resize(otherJSONs.size());
    for(size_t i = 0; i < otherJSONs.size(); i++)
      info->otherJSONs[i] = otherJSONs[i];
  }

  return ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel)
{
  RenderDoc::Inst().UpdateVulkanLayerRegistration(systemLevel);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateInstalledVersionNumber()
{
#if ENABLED(RDOC_WIN32)
  HKEY key = NULL;

  LSTATUS ret =
      RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                      0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &key, NULL);

  if(ret != ERROR_SUCCESS)
  {
    if(key)
      RegCloseKey(key);

    return;
  }

  bool done = false;

  char guidName[256] = {};
  for(DWORD idx = 0; ret == ERROR_SUCCESS && !done; idx++)
  {
    // enumerate all the uninstall keys
    ret = RegEnumKeyA(key, idx, guidName, sizeof(guidName) - 1);

    if(ret == ERROR_NO_MORE_ITEMS)
    {
      break;
    }
    else if(ret != ERROR_SUCCESS)
    {
      break;
    }

    // open the key as we'll need it for RegSetValueExA
    HKEY subkey = NULL;
    ret = RegCreateKeyExA(key, guidName, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &subkey, NULL);

    if(ret == ERROR_SUCCESS && subkey)
    {
      char DisplayName[256] = {};
      char Publisher[256] = {};
      DWORD len = sizeof(DisplayName) - 1;

      // fetch DisplayName and Publisher values
      ret = RegGetValueA(subkey, NULL, "DisplayName", RRF_RT_ANY, NULL, DisplayName, &len);

      // allow the value to silently not exist
      if(ret != ERROR_SUCCESS)
      {
        DisplayName[0] = 0;
        ret = ERROR_SUCCESS;
      }

      len = sizeof(Publisher) - 1;
      ret = RegGetValueA(subkey, NULL, "Publisher", RRF_RT_ANY, NULL, Publisher, &len);

      if(ret != ERROR_SUCCESS)
      {
        Publisher[0] = 0;
        ret = ERROR_SUCCESS;
      }

      // if this is our key, set the version number
      if(!strcmp(DisplayName, "RenderDoc") && !strcmp(Publisher, "Baldur Karlsson"))
      {
        DWORD Version = (RENDERDOC_VERSION_MAJOR << 24) | (RENDERDOC_VERSION_MINOR << 16);
        DWORD VersionMajor = RENDERDOC_VERSION_MAJOR;
        DWORD VersionMinor = RENDERDOC_VERSION_MINOR;
        std::string DisplayVersion = MAJOR_MINOR_VERSION_STRING ".0";

        RegSetValueExA(subkey, "Version", 0, REG_DWORD, (const BYTE *)&Version, sizeof(Version));
        RegSetValueExA(subkey, "VersionMajor", 0, REG_DWORD, (const BYTE *)&VersionMajor,
                       sizeof(VersionMajor));
        RegSetValueExA(subkey, "VersionMinor", 0, REG_DWORD, (const BYTE *)&VersionMinor,
                       sizeof(VersionMinor));
        RegSetValueExA(subkey, "DisplayVersion", 0, REG_SZ, (const BYTE *)DisplayVersion.c_str(),
                       (DWORD)DisplayVersion.size() + 1);
        done = true;
      }
    }

    if(subkey)
      RegCloseKey(subkey);
  }

  if(key)
    RegCloseKey(key);

#endif
}

static std::string ResourceFormatName(const ResourceFormat &fmt)
{
  std::string ret;

  if(fmt.Special())
  {
    switch(fmt.type)
    {
      case ResourceFormatType::Regular: break;
      case ResourceFormatType::Undefined: return "Undefined";
      case ResourceFormatType::BC1:
        if(fmt.compType == CompType::Typeless)
          return "BC1_TYPELESS";
        return fmt.SRGBCorrected() ? "BC1_SRGB" : "BC1_UNORM";
      case ResourceFormatType::BC2:
        if(fmt.compType == CompType::Typeless)
          return "BC2_TYPELESS";
        return fmt.SRGBCorrected() ? "BC2_SRGB" : "BC2_UNORM";
      case ResourceFormatType::BC3:
        if(fmt.compType == CompType::Typeless)
          return "BC3_TYPELESS";
        return fmt.SRGBCorrected() ? "BC3_SRGB" : "BC3_UNORM";
      case ResourceFormatType::BC4:
        if(fmt.compType == CompType::Typeless)
          return "BC4_TYPELESS";
        return fmt.compType == CompType::UNorm ? "BC4_UNORM" : "BC4_SNORM";
      case ResourceFormatType::BC5:
        if(fmt.compType == CompType::Typeless)
          return "BC5_TYPELESS";
        return fmt.compType == CompType::UNorm ? "BC5_UNORM" : "BC5_SNORM";
      case ResourceFormatType::BC6:
        if(fmt.compType == CompType::Typeless)
          return "BC6_TYPELESS";
        return fmt.compType == CompType::UNorm ? "BC6_UFLOAT" : "BC6_SFLOAT";
      case ResourceFormatType::BC7:
        if(fmt.compType == CompType::Typeless)
          return "BC7_TYPELESS";
        return fmt.SRGBCorrected() ? "BC7_SRGB" : "BC7_UNORM";
      case ResourceFormatType::ETC2:
      {
        if(fmt.compCount == 4)
          return fmt.SRGBCorrected() ? "ETC2_RGB8A1_SRGB" : "ETC2_RGB8A1_UNORM";
        else
          return fmt.SRGBCorrected() ? "ETC2_RGB8_SRGB" : "ETC2_RGB8_UNORM";
      }
      case ResourceFormatType::EAC:
      {
        if(fmt.compCount == 1)
          return fmt.compType == CompType::UNorm ? "EAC_R11_UNORM" : "EAC_R11_SNORM";
        else if(fmt.compCount == 2)
          return fmt.compType == CompType::UNorm ? "EAC_RG11_UNORM" : "EAC_RG11_SNORM";
        else
          return fmt.SRGBCorrected() ? "ETC2_EAC_RGBA8_SRGB" : "ETC2_EAC_RGBA8_UNORM";
      }
      case ResourceFormatType::ASTC:
        return fmt.SRGBCorrected() ? "ASTC_SRGB" : "ASTC_UNORM";
      // 10:10:10 A2 is the only format that can have all the usual format types (unorm, snorm,
      // etc). So we break and handle it like any other format below.
      case ResourceFormatType::R10G10B10A2:
        ret = fmt.BGRAOrder() ? "B10G10R10A2" : "R10G10B10A2";
        break;
      case ResourceFormatType::R11G11B10: return "R11G11B10_FLOAT";
      case ResourceFormatType::R5G6B5: return fmt.BGRAOrder() ? "R5G6B5_UNORM" : "B5G6R5_UNORM";
      case ResourceFormatType::R5G5B5A1:
        return fmt.BGRAOrder() ? "R5G5B5A1_UNORM" : "B5G5R5A1_UNORM";
      case ResourceFormatType::R9G9B9E5: return "R9G9B9E5_FLOAT";
      case ResourceFormatType::R4G4B4A4:
        return fmt.BGRAOrder() ? "R4G4B4A4_UNORM" : "B4G4R4A4_UNORM";
      case ResourceFormatType::R4G4: return "R4G4_UNORM";
      case ResourceFormatType::D16S8:
        return fmt.compType == CompType::Typeless ? "D16S8_TYPELESS" : "D16S8";
      case ResourceFormatType::D24S8:
        return fmt.compType == CompType::Typeless ? "D24S8_TYPELESS" : "D24S8";
      case ResourceFormatType::D32S8:
        return fmt.compType == CompType::Typeless ? "D32S8_TYPELESS" : "D32S8";
      case ResourceFormatType::S8: return "S8";
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
      {
        int yuvbits = 0;

        switch(fmt.type)
        {
          case ResourceFormatType::YUV8: yuvbits = 8; break;
          case ResourceFormatType::YUV10: yuvbits = 10; break;
          case ResourceFormatType::YUV12: yuvbits = 12; break;
          case ResourceFormatType::YUV16: yuvbits = 16; break;
          default: break;
        }

        uint32_t planeCount = fmt.YUVPlaneCount();
        uint32_t subsampling = fmt.YUVSubsampling();

        // special case formats that don't match the FOURCC format
        if(yuvbits == 8 && planeCount == 2 && subsampling == 420)
          return "NV12";
        if(yuvbits == 8 && planeCount == 1 && subsampling == 444)
          return "AYUV";
        if(yuvbits == 8 && planeCount == 1 && subsampling == 422)
          return "YUY2";

        switch(subsampling)
        {
          case 444:
            if(planeCount == 1)
              return StringFormat::Fmt("Y4%02u", yuvbits);
            else if(planeCount == 2)
              return StringFormat::Fmt("P4%02u", yuvbits);
            else
              return StringFormat::Fmt("YUV_%uPlane_%ubit", planeCount, yuvbits);
          case 422:
            if(planeCount == 1)
              return StringFormat::Fmt("Y2%02u", yuvbits);
            else if(planeCount == 2)
              return StringFormat::Fmt("P2%02u", yuvbits);
            else
              return StringFormat::Fmt("YUV_%uPlane_%ubit", planeCount, yuvbits);
          case 420:
            if(planeCount == 1)
              return StringFormat::Fmt("Y0%02u", yuvbits);
            else if(planeCount == 2)
              return StringFormat::Fmt("P0%02u", yuvbits);
            else
              return StringFormat::Fmt("YUV_%uPlane_%ubit", planeCount, yuvbits);
          default: RDCERR("Unexpected YUV Subsampling amount %u", subsampling);
        }

        return StringFormat::Fmt("YUV_%u_%uPlane_%ubit", subsampling, planeCount, yuvbits);
      }
      case ResourceFormatType::PVRTC: return "PVRTC";
    }
  }
  else if(fmt.compType == CompType::Depth)
  {
    ret = StringFormat::Fmt("D%u", fmt.compByteWidth * 8);
  }
  else
  {
    char comps[] = "RGBA";

    if(fmt.BGRAOrder())
      std::swap(comps[0], comps[2]);

    for(uint32_t i = 0; i < fmt.compCount; i++)
      ret += StringFormat::Fmt("%c%u", comps[i], fmt.compByteWidth * 8);
  }

  switch(fmt.compType)
  {
    case CompType::Typeless: return ret + "_TYPELESS";
    case CompType::Float:
    case CompType::Double: return ret + "_FLOAT";
    case CompType::UNorm: return ret + "_UNORM";
    case CompType::SNorm: return ret + "_SNORM";
    case CompType::UInt: return ret + "_UINT";
    case CompType::SInt: return ret + "_SINT";
    case CompType::UScaled: return ret + "_USCALED";
    case CompType::SScaled: return ret + "_SSCALED";
    case CompType::UNormSRGB: return ret + "_SRGB";
    case CompType::Depth:
      // we already special-cased depth component type above to be Dx instead of Rx
      return ret;
  }

  // should never get here
  RDCERR("Unhandled format component type");
  return ret + "_UNKNOWN";
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_ResourceFormatName(const ResourceFormat &fmt,
                                                                        rdcstr &name)
{
  name = ResourceFormatName(fmt);
}

static void TestPrintMsg(const std::string &msg)
{
  OSUtility::WriteOutput(OSUtility::Output_DebugMon, msg.c_str());
  OSUtility::WriteOutput(OSUtility::Output_StdErr, msg.c_str());
}

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunFunctionalTests(int pythonMinorVersion,
                                                                       const rdcarray<rdcstr> &args)
{
#if ENABLED(RDOC_WIN32)
  const char *moduledir = "/pymodules";
  const char *modulename = "renderdoc.pyd";
  std::string pythonlibs[] = {"python3?.dll"};
#elif ENABLED(RDOC_LINUX)
  const char *moduledir = "";
  const char *modulename = "renderdoc.so";
  // we don't care about pymalloc or not
  std::string pythonlibs[] = {"libpython3.?m.so.1.0", "libpython3.?.so.1.0", "libpython3.?m.so",
                              "libpython3.?.so"};
#else
  const char *moduledir = "";
  const char *modulename = "";
  std::string pythonlibs[] = {};
  TestPrintMsg(
      "Running functional tests not directly supported on this platform.\n"
      "Try running util/test/run_tests.py manually.\n");
  return 1;
#endif

  std::string libPath;
  FileIO::GetLibraryFilename(libPath);

  libPath = get_dirname(libPath);
  std::string modulePath = libPath + moduledir;

  std::string moduleFilename = modulePath + "/" + modulename;

  if(!FileIO::exists(moduleFilename.c_str()))
  {
    TestPrintMsg(StringFormat::Fmt("Couldn't locate python module at %s\n", moduleFilename.c_str()));
    return 1;
  }

  // if we've been built either on windows or on linux from within the project root, going up two
  // directories from the library will put us at the project root. This is the most common scenario
  // and we don't add handling for locating the script elsewhere as in that case the user can run it
  // directly. This is just intended as a useful shortcut for common cases.
  std::string scriptPath = libPath + "/../../util/test/run_tests.py";

  if(!FileIO::exists(scriptPath.c_str()))
  {
    TestPrintMsg(StringFormat::Fmt("Couldn't locate run_tests.py script at %s\n", scriptPath.c_str()));
    return 1;
  }

  void *handle = NULL;

  for(std::string py : pythonlibs)
  {
    // patch up the python minor version
    char *ver = strchr(&py[0], '?');
    *ver = char('0' + pythonMinorVersion);

    handle = Process::LoadModule(py.c_str());
    if(handle)
    {
      RDCLOG("Loaded python from %s", py.c_str());
      break;
    }
  }

  if(!handle)
  {
    TestPrintMsg("Couldn't locate python 3.6 library\n");
    return 1;
  }

  typedef int(RENDERDOC_CC * PFN_Py_Main)(int, wchar_t **);

  PFN_Py_Main mainFunc = (PFN_Py_Main)Process::GetFunctionAddress(handle, "Py_Main");

  if(!mainFunc)
  {
    TestPrintMsg("Couldn't get Py_Main in python library\n");
    return 1;
  }

  std::vector<std::wstring> wideArgs(args.size());

  for(size_t i = 0; i < args.size(); i++)
    wideArgs[i] = StringFormat::UTF82Wide(args[i]);

  // insert fake arguments to point at the script and our modules
  wideArgs.insert(wideArgs.begin(),
                  {
                      L"python",
                      // specify script path
                      StringFormat::UTF82Wide(scriptPath),
                      // specify native library path
                      L"--renderdoc", StringFormat::UTF82Wide(libPath),
                      // specify python module path
                      L"--pyrenderdoc", StringFormat::UTF82Wide(modulePath),
                      // force in-process as we can't fork out to python to pass args
                      L"--in-process",
                  });

  std::vector<wchar_t *> wideArgStrings(wideArgs.size());

  for(size_t i = 0; i < wideArgs.size(); i++)
    wideArgStrings[i] = &wideArgs[i][0];

  return mainFunc((int)wideArgStrings.size(), wideArgStrings.data());
}
