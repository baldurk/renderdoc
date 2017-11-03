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

#include "3rdparty/catch/catch.hpp"
#include "api/replay/renderdoc_replay.h"
#include "api/replay/version.h"
#include "common/common.h"
#include "core/android.h"
#include "core/core.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "serialise/string_utils.h"

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
    RDCLOGFILE(log);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text)
{
  rdclog_int(LogType::Comment, "EXT", "external", 0, "%s", text);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogType type, const char *project,
                                                                const char *file, unsigned int line,
                                                                const char *text)
{
  rdclog_int(type, project ? project : "UNK?", file ? file : "unknown", line, "%s", text);

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

extern "C" RENDERDOC_API void RENDERDOC_CC
RENDERDOC_InitGlobalEnv(GlobalEnvironment env, const rdctype::array<rdctype::str> &args)
{
  std::vector<std::string> argsVec;
  argsVec.reserve(args.size());
  for(const rdctype::str &a : args)
    argsVec.push_back(a.c_str());

  RenderDoc::Inst().ProcessGlobalEnvironment(env, argsVec);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs,
                                                                             bool crashed)
{
  if(RenderDoc::Inst().GetCrashHandler() == NULL)
    return;

  if(exceptionPtrs)
  {
    RenderDoc::Inst().GetCrashHandler()->WriteMinidump(exceptionPtrs);
  }
  else
  {
    if(!crashed)
    {
      RDCLOG("Writing crash log");
    }

    RenderDoc::Inst().GetCrashHandler()->WriteMinidump();

    if(!crashed)
    {
      RenderDoc::Inst().RecreateCrashHandler();
    }
  }
}

extern "C" RENDERDOC_API ReplaySupport RENDERDOC_CC RENDERDOC_SupportLocalReplay(
    const char *logfile, rdctype::str *driver, rdctype::str *recordMachineIdent)
{
  ICaptureFile *file = RENDERDOC_OpenCaptureFile(logfile);

  if(driver)
    *driver = file->DriverName();

  if(recordMachineIdent)
    *recordMachineIdent = file->RecordedMachineIdent();

  ReplaySupport support = file->LocalReplaySupport();

  file->Shutdown();

  return support;
}

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, IReplayController **rend)
{
  ICaptureFile *file = RENDERDOC_OpenCaptureFile(logfile);

  ReplayStatus ret = file->OpenStatus();

  if(ret != ReplayStatus::Succeeded)
  {
    file->Shutdown();
    return ret;
  }

  std::tie(ret, *rend) = file->OpenCapture(progress);

  file->Shutdown();

  return ret;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                           const rdctype::array<EnvironmentModification> &env, const char *logfile,
                           const CaptureOptions &opts, bool waitForExit)
{
  return Process::LaunchAndInjectIntoProcess(app, workingDir, cmdLine, env, logfile, opts,
                                             waitForExit != 0);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts)
{
  *opts = CaptureOptions();
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                     const char *logfile,
                                                                     const CaptureOptions &opts)
{
  return Process::StartGlobalHook(pathmatch, logfile, opts);
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

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_InjectIntoProcess(uint32_t pid, const rdctype::array<EnvironmentModification> &env,
                            const char *logfile, const CaptureOptions &opts, bool waitForExit)
{
  return Process::InjectIntoProcess(pid, env, logfile, opts, waitForExit != 0);
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_GetThumbnail(const char *filename,
                                                                  FileType type, uint32_t maxsize,
                                                                  rdctype::array<byte> *buf)
{
  ICaptureFile *file = RENDERDOC_OpenCaptureFile(filename);

  if(file->OpenStatus() != ReplayStatus::Succeeded)
  {
    file->Shutdown();
    return false;
  }

  *buf = file->GetThumbnail(type, maxsize);
  file->Shutdown();
  return true;
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
  string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  // initial case is we're called with 0, start with the first port.
  // otherwise we're called with the last successful ident, so increment
  // before continuing to enumerate.
  if(nextIdent == 0)
    nextIdent = RenderDoc_FirstTargetControlPort;
  else
    nextIdent++;

  uint32_t lastIdent = RenderDoc_LastTargetControlPort;
  if(host != NULL && Android::IsHostADB(host))
  {
    int index = 0;
    std::string deviceID;
    Android::extractDeviceIDAndIndex(host, index, deviceID);

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

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(const char *listenhost,
                                                                        uint32_t port,
                                                                        volatile bool *killReplay)
{
  bool dummy = false;

  if(killReplay == NULL)
    killReplay = &dummy;

  if(listenhost == NULL || listenhost[0] == 0)
    listenhost = "0.0.0.0";

  if(port == 0)
    port = RENDERDOC_GetDefaultRemoteServerPort();

  RenderDoc::Inst().BecomeRemoteServer(listenhost, (uint16_t)port, *killReplay);
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

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_NeedVulkanLayerRegistration(
    VulkanLayerFlags *flagsPtr, rdctype::array<rdctype::str> *myJSONsPtr,
    rdctype::array<rdctype::str> *otherJSONsPtr)
{
  VulkanLayerFlags flags = VulkanLayerFlags::NoFlags;
  std::vector<std::string> myJSONs;
  std::vector<std::string> otherJSONs;

  bool ret = RenderDoc::Inst().NeedVulkanLayerRegistration(flags, myJSONs, otherJSONs);

  if(flagsPtr)
    *flagsPtr = flags;

  if(myJSONsPtr)
  {
    myJSONsPtr->resize(myJSONs.size());
    for(size_t i = 0; i < myJSONs.size(); i++)
      (*myJSONsPtr)[i] = myJSONs[i];
  }

  if(otherJSONsPtr)
  {
    otherJSONsPtr->resize(otherJSONs.size());
    for(size_t i = 0; i < otherJSONs.size(); i++)
      (*otherJSONsPtr)[i] = otherJSONs[i];
  }

  return ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel)
{
  RenderDoc::Inst().UpdateVulkanLayerRegistration(systemLevel);
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
      case ResourceFormatType::BC1: return fmt.srgbCorrected ? "BC1_SRGB" : "BC1_UNORM";
      case ResourceFormatType::BC2: return fmt.srgbCorrected ? "BC2_SRGB" : "BC2_UNORM";
      case ResourceFormatType::BC3: return fmt.srgbCorrected ? "BC3_SRGB" : "BC3_UNORM";
      case ResourceFormatType::BC4:
        return fmt.compType == CompType::UNorm ? "BC4_UNORM" : "BC4_SNORM";
      case ResourceFormatType::BC5:
        return fmt.compType == CompType::UNorm ? "BC5_UNORM" : "BC5_SNORM";
      case ResourceFormatType::BC6:
        return fmt.compType == CompType::UNorm ? "BC6_UFLOAT" : "BC6_SFLOAT";
      case ResourceFormatType::BC7: return fmt.srgbCorrected ? "BC7_SRGB" : "BC7_UNORM";
      case ResourceFormatType::ETC2: return fmt.srgbCorrected ? "ETC2_SRGB" : "ETC_UNORM";
      case ResourceFormatType::EAC:
      {
        if(fmt.compCount == 1)
          return fmt.compType == CompType::UNorm ? "EAC_R_UNORM" : "EAC_R_SNORM";
        else
          return fmt.compType == CompType::UNorm ? "EAC_RG_UNORM" : "EAC_RG_SNORM";
      }
      case ResourceFormatType::ASTC:
        return fmt.srgbCorrected ? "ASTC_SRGB" : "ASTC_UNORM";
      // 10:10:10 A2 is the only format that can have all the usual format types (unorm, snorm,
      // etc). So we break and handle it like any other format below.
      case ResourceFormatType::R10G10B10A2:
        ret = fmt.bgraOrder ? "B10G10R10A2" : "R10G10B10A2";
        break;
      case ResourceFormatType::R11G11B10: return "R11G11B10_FLOAT";
      case ResourceFormatType::R5G6B5: return fmt.bgraOrder ? "R5G6B5_UNORM" : "B5G6R5_UNORM";
      case ResourceFormatType::R5G5B5A1: return fmt.bgraOrder ? "R5G5B5A1_UNORM" : "R5G5B5A1_UNORM";
      case ResourceFormatType::R9G9B9E5: return "R9G9B9E5_FLOAT";
      case ResourceFormatType::R4G4B4A4: return fmt.bgraOrder ? "R4G4B4A4_UNORM" : "B4G4R4A4_UNORM";
      case ResourceFormatType::R4G4: return "R4G4_UNORM";
      case ResourceFormatType::D16S8: return "D16S8";
      case ResourceFormatType::D24S8: return "D24S8";
      case ResourceFormatType::D32S8: return "D32S8";
      case ResourceFormatType::S8: return "S8";
      case ResourceFormatType::YUV: return "YUV";
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

    if(fmt.bgraOrder)
      std::swap(comps[0], comps[2]);

    for(uint32_t i = 0; i < fmt.compCount; i++)
      ret += StringFormat::Fmt("%c%u", comps[i], fmt.compByteWidth * 8);
  }

  if(fmt.srgbCorrected)
    return ret + "_SRGB";

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
    case CompType::Depth:
      // we already special-cased depth component type above to be Dx instead of Rx
      return ret;
  }

  // should never get here
  RDCERR("Unhandled format component type");
  return ret + "_UNKNOWN";
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_ResourceFormatName(const ResourceFormat &fmt,
                                                                        rdctype::str &name)
{
  name = ResourceFormatName(fmt);
}