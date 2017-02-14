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

#include <sstream>
#include "api/replay/renderdoc_replay.h"
#include "api/replay/version.h"
#include "common/common.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "jpeg-compressor/jpge.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "replay/replay_renderer.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "stb/stb_image_resize.h"
#include "stb/stb_image_write.h"

// these entry points are for the replay/analysis side - not for the application.

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_NumVerticesPerPrimitive(PrimitiveTopology topology)
{
  // strips/loops/fans have the same number of indices for a single primitive
  // as their list friends
  switch(topology)
  {
    default:
    case eTopology_Unknown: break;
    case eTopology_PointList: return 1;
    case eTopology_LineList:
    case eTopology_LineStrip:
    case eTopology_LineLoop: return 2;
    case eTopology_TriangleList:
    case eTopology_TriangleStrip:
    case eTopology_TriangleFan: return 3;
    case eTopology_LineList_Adj:
    case eTopology_LineStrip_Adj: return 4;
    case eTopology_TriangleList_Adj:
    case eTopology_TriangleStrip_Adj: return 6;
    case eTopology_PatchList_1CPs:
    case eTopology_PatchList_2CPs:
    case eTopology_PatchList_3CPs:
    case eTopology_PatchList_4CPs:
    case eTopology_PatchList_5CPs:
    case eTopology_PatchList_6CPs:
    case eTopology_PatchList_7CPs:
    case eTopology_PatchList_8CPs:
    case eTopology_PatchList_9CPs:
    case eTopology_PatchList_10CPs:
    case eTopology_PatchList_11CPs:
    case eTopology_PatchList_12CPs:
    case eTopology_PatchList_13CPs:
    case eTopology_PatchList_14CPs:
    case eTopology_PatchList_15CPs:
    case eTopology_PatchList_16CPs:
    case eTopology_PatchList_17CPs:
    case eTopology_PatchList_18CPs:
    case eTopology_PatchList_19CPs:
    case eTopology_PatchList_20CPs:
    case eTopology_PatchList_21CPs:
    case eTopology_PatchList_22CPs:
    case eTopology_PatchList_23CPs:
    case eTopology_PatchList_24CPs:
    case eTopology_PatchList_25CPs:
    case eTopology_PatchList_26CPs:
    case eTopology_PatchList_27CPs:
    case eTopology_PatchList_28CPs:
    case eTopology_PatchList_29CPs:
    case eTopology_PatchList_30CPs:
    case eTopology_PatchList_31CPs:
    case eTopology_PatchList_32CPs: return uint32_t(topology - eTopology_PatchList_1CPs + 1);
  }

  return 0;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_VertexOffset(PrimitiveTopology topology,
                                                                     uint32_t primitive)
{
  // strips/loops/fans have the same number of indices for a single primitive
  // as their list friends
  switch(topology)
  {
    default:
    case eTopology_Unknown:
    case eTopology_PointList:
    case eTopology_LineList:
    case eTopology_TriangleList:
    case eTopology_LineList_Adj:
    case eTopology_TriangleList_Adj:
    case eTopology_PatchList_1CPs:
    case eTopology_PatchList_2CPs:
    case eTopology_PatchList_3CPs:
    case eTopology_PatchList_4CPs:
    case eTopology_PatchList_5CPs:
    case eTopology_PatchList_6CPs:
    case eTopology_PatchList_7CPs:
    case eTopology_PatchList_8CPs:
    case eTopology_PatchList_9CPs:
    case eTopology_PatchList_10CPs:
    case eTopology_PatchList_11CPs:
    case eTopology_PatchList_12CPs:
    case eTopology_PatchList_13CPs:
    case eTopology_PatchList_14CPs:
    case eTopology_PatchList_15CPs:
    case eTopology_PatchList_16CPs:
    case eTopology_PatchList_17CPs:
    case eTopology_PatchList_18CPs:
    case eTopology_PatchList_19CPs:
    case eTopology_PatchList_20CPs:
    case eTopology_PatchList_21CPs:
    case eTopology_PatchList_22CPs:
    case eTopology_PatchList_23CPs:
    case eTopology_PatchList_24CPs:
    case eTopology_PatchList_25CPs:
    case eTopology_PatchList_26CPs:
    case eTopology_PatchList_27CPs:
    case eTopology_PatchList_28CPs:
    case eTopology_PatchList_29CPs:
    case eTopology_PatchList_30CPs:
    case eTopology_PatchList_31CPs:
    case eTopology_PatchList_32CPs:
      // for all lists, it's just primitive * Topology_NumVerticesPerPrimitive(topology)
      break;
    case eTopology_LineStrip:
    case eTopology_LineLoop:
    case eTopology_TriangleStrip:
    case eTopology_LineStrip_Adj:
      // for strips, each new vertex creates a new primitive
      return primitive;
    case eTopology_TriangleStrip_Adj:
      // triangle strip with adjacency is a special case as every other
      // vert is purely for adjacency so it's doubled
      return primitive * 2;
    case eTopology_TriangleFan: RDCERR("Cannot get VertexOffset for triangle fan!"); break;
  }

  return primitive * Topology_NumVerticesPerPrimitive(topology);
}

extern "C" RENDERDOC_API float RENDERDOC_CC Maths_HalfToFloat(uint16_t half)
{
  return ConvertFromHalf(half);
}

extern "C" RENDERDOC_API uint16_t RENDERDOC_CC Maths_FloatToHalf(float f)
{
  return ConvertToHalf(f);
}

extern "C" RENDERDOC_API Camera *RENDERDOC_CC Camera_InitArcball()
{
  return new Camera(Camera::eType_Arcball);
}

extern "C" RENDERDOC_API Camera *RENDERDOC_CC Camera_InitFPSLook()
{
  return new Camera(Camera::eType_FPSLook);
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_Shutdown(Camera *c)
{
  delete c;
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetPosition(Camera *c, float x, float y, float z)
{
  c->SetPosition(Vec3f(x, y, z));
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetFPSRotation(Camera *c, float x, float y, float z)
{
  c->SetFPSRotation(Vec3f(x, y, z));
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_SetArcballDistance(Camera *c, float dist)
{
  c->SetArcballDistance(dist);
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_ResetArcball(Camera *c)
{
  c->ResetArcball();
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_RotateArcball(Camera *c, float ax, float ay,
                                                                float bx, float by)
{
  c->RotateArcball(Vec2f(ax, ay), Vec2f(bx, by));
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_GetBasis(Camera *c, FloatVector *pos,
                                                           FloatVector *fwd, FloatVector *right,
                                                           FloatVector *up)
{
  Vec3f p = c->GetPosition();
  Vec3f f = c->GetForward();
  Vec3f r = c->GetRight();
  Vec3f u = c->GetUp();

  pos->x = p.x;
  pos->y = p.y;
  pos->z = p.z;

  fwd->x = f.x;
  fwd->y = f.y;
  fwd->z = f.z;

  right->x = r.x;
  right->y = r.y;
  right->z = r.z;

  up->x = u.x;
  up->y = u.y;
  up->z = u.z;
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetVersionString()
{
  return RENDERDOC_VERSION_STRING;
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetCommitHash()
{
  return GIT_COMMIT_HASH;
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

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_MakeEnvironmentModificationList(int numElems)
{
  return new Process::EnvironmentModification[numElems + 1];    // last one is a terminator
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetEnvironmentModification(
    void *mem, int idx, const char *variable, const char *value, EnvironmentModificationType type,
    EnvironmentSeparator separator)
{
  Process::EnvironmentModification *mods = (Process::EnvironmentModification *)mem;

  Process::ModificationType modType = Process::eEnvModification_Replace;

  if(type == eEnvMod_Append)
    modType = Process::ModificationType(Process::eEnvModification_AppendPlatform + (int)separator);
  if(type == eEnvMod_Prepend)
    modType = Process::ModificationType(Process::eEnvModification_PrependPlatform + (int)separator);

  mods[idx] = Process::EnvironmentModification(modType, variable, value);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeEnvironmentModificationList(void *mem)
{
  Process::EnvironmentModification *mods = (Process::EnvironmentModification *)mem;
  delete[] mods;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const char *log)
{
  if(log)
    RDCLOGFILE(log);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text)
{
  rdclog_int(RDCLog_Comment, "EXT", "external", 0, "%s", text);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogMessageType type,
                                                                const char *project, const char *file,
                                                                unsigned int line, const char *text)
{
  RDCCOMPILE_ASSERT(
      (int)eLogType_First == (int)RDCLog_First && (int)eLogType_NumTypes == (int)eLogType_NumTypes,
      "Log type enum is out of sync");
  rdclog_int((LogType)type, project ? project : "UNK?", file ? file : "unknown", line, "%s", text);

#if ENABLED(DEBUGBREAK_ON_ERROR_LOG)
  if(type == eLogType_Error)
    RDCBREAK();
#endif

  if(type == eLogType_Fatal)
    RDCDUMP();
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetLogFile()
{
  return RDCGETLOGFILE();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs,
                                                                             bool32 crashed)
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
  if(logfile == NULL)
    return eReplaySupport_Unsupported;

  RDCDriver driverType = RDC_Unknown;
  string driverName = "";
  uint64_t fileMachineIdent = 0;
  RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, fileMachineIdent, NULL);

  if(driver)
    *driver = driverName;

  bool supported = RenderDoc::Inst().HasReplayDriver(driverType);

  if(!supported)
    return eReplaySupport_Unsupported;

  if(fileMachineIdent != 0)
  {
    uint64_t machineIdent = OSUtility::GetMachineIdent();

    if(recordMachineIdent)
      *recordMachineIdent = OSUtility::MakeMachineIdentString(fileMachineIdent);

    if((machineIdent & OSUtility::MachineIdent_OS_Mask) !=
       (fileMachineIdent & OSUtility::MachineIdent_OS_Mask))
      return eReplaySupport_SuggestRemote;
  }

  return eReplaySupport_Supported;
}

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, ReplayRenderer **rend)
{
  if(rend == NULL)
    return eReplayCreate_InternalError;

  RenderDoc::Inst().SetProgressPtr(progress);

  ReplayRenderer *render = new ReplayRenderer();

  if(!render)
  {
    RenderDoc::Inst().SetProgressPtr(NULL);
    return eReplayCreate_InternalError;
  }

  ReplayCreateStatus ret = render->CreateDevice(logfile);

  if(ret != eReplayCreate_Success)
  {
    delete render;
    RenderDoc::Inst().SetProgressPtr(NULL);
    return ret;
  }

  *rend = render;

  RenderDoc::Inst().SetProgressPtr(NULL);
  return eReplayCreate_Success;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine, void *env,
                           const char *logfile, const CaptureOptions *opts, bool32 waitForExit)
{
  return Process::LaunchAndInjectIntoProcess(app, workingDir, cmdLine,
                                             (Process::EnvironmentModification *)env, logfile, opts,
                                             waitForExit != 0);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts)
{
  *opts = CaptureOptions();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                     const char *logfile,
                                                                     const CaptureOptions *opts)
{
  Process::StartGlobalHook(pathmatch, logfile, opts);
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_InjectIntoProcess(
    uint32_t pid, void *env, const char *logfile, const CaptureOptions *opts, bool32 waitForExit)
{
  return Process::InjectIntoProcess(pid, (Process::EnvironmentModification *)env, logfile, opts,
                                    waitForExit != 0);
}

static void writeToByteVector(void *context, void *data, int size)
{
  std::vector<byte> *vec = (std::vector<byte> *)context;
  byte *start = (byte *)data;
  byte *end = start + size;
  vec->insert(vec->end(), start, end);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_GetThumbnail(const char *filename,
                                                                    FileType type, uint32_t maxsize,
                                                                    rdctype::array<byte> *buf)
{
  Serialiser ser(filename, Serialiser::READING, false);

  if(ser.HasError())
    return false;

  ser.Rewind();

  int chunkType = ser.PushContext(NULL, NULL, 1, false);

  if(chunkType != THUMBNAIL_DATA)
    return false;

  bool HasThumbnail = false;
  ser.Serialise(NULL, HasThumbnail);

  if(!HasThumbnail)
    return false;

  byte *jpgbuf = NULL;
  size_t thumblen = 0;
  uint32_t thumbwidth = 0, thumbheight = 0;
  {
    ser.Serialise("ThumbWidth", thumbwidth);
    ser.Serialise("ThumbHeight", thumbheight);
    ser.SerialiseBuffer("ThumbnailPixels", jpgbuf, thumblen);
  }

  if(jpgbuf == NULL)
    return false;

  // if the desired output is jpg and either there's no max size or it's already satisfied,
  // return the data directly
  if(type == eFileType_JPG && (maxsize == 0 || (maxsize > thumbwidth && maxsize > thumbheight)))
  {
    create_array_init(*buf, thumblen, jpgbuf);
  }
  else
  {
    // otherwise we need to decode, resample maybe, and re-encode

    int w = (int)thumbwidth;
    int h = (int)thumbheight;
    int comp = 3;
    byte *thumbpixels =
        jpgd::decompress_jpeg_image_from_memory(jpgbuf, (int)thumblen, &w, &h, &comp, 3);

    if(maxsize != 0)
    {
      uint32_t clampedWidth = RDCMIN(maxsize, thumbwidth);
      uint32_t clampedHeight = RDCMIN(maxsize, thumbheight);

      if(clampedWidth != thumbwidth || clampedHeight != thumbheight)
      {
        // preserve aspect ratio, take the smallest scale factor and multiply both
        float scaleX = float(clampedWidth) / float(thumbwidth);
        float scaleY = float(clampedHeight) / float(thumbheight);

        if(scaleX < scaleY)
          clampedHeight = uint32_t(scaleX * thumbheight);
        else if(scaleY < scaleX)
          clampedWidth = uint32_t(scaleY * thumbwidth);

        byte *resizedpixels = (byte *)malloc(3 * clampedWidth * clampedHeight);

        stbir_resize_uint8_srgb(thumbpixels, thumbwidth, thumbheight, 0, resizedpixels,
                                clampedWidth, clampedHeight, 0, 3, -1, 0);

        free(thumbpixels);

        thumbpixels = resizedpixels;
        thumbwidth = clampedWidth;
        thumbheight = clampedHeight;
      }
    }

    std::vector<byte> encodedBytes;

    switch(type)
    {
      case eFileType_JPG:
      {
        int len = thumbwidth * thumbheight * 3;
        encodedBytes.resize(len);
        jpge::params p;
        p.m_quality = 90;
        jpge::compress_image_to_jpeg_file_in_memory(&encodedBytes[0], len, (int)thumbwidth,
                                                    (int)thumbheight, 3, thumbpixels, p);
        encodedBytes.resize(len);
        break;
      }
      case eFileType_PNG:
      {
        stbi_write_png_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels, 0);
        break;
      }
      case eFileType_TGA:
      {
        stbi_write_tga_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels);
        break;
      }
      case eFileType_BMP:
      {
        stbi_write_bmp_to_func(&writeToByteVector, &encodedBytes, (int)thumbwidth, (int)thumbheight,
                               3, thumbpixels);
        break;
      }
      default:
      {
        RDCERR("Unsupported file type %d in thumbnail fetch", type);
        free(thumbpixels);
        delete[] jpgbuf;
        return false;
      }
    }

    *buf = encodedBytes;

    free(thumbpixels);
  }

  delete[] jpgbuf;

  return true;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem)
{
  rdctype::array<char>::deallocate(mem);
}

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz)
{
  return rdctype::array<char>::allocate((size_t)sz);
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
    if(nextIdent == RenderDoc_FirstTargetControlPort)
      nextIdent += RenderDoc_AndroidPortOffset;
    lastIdent += RenderDoc_AndroidPortOffset;

    s = "127.0.0.1";

    // could parse out an (optional) device name from host+4 here.
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
  return ~0U;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetDefaultRemoteServerPort()
{
  return RenderDoc_RemoteServerPort;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(const char *listenhost,
                                                                        uint32_t port,
                                                                        volatile bool32 *killReplay)
{
  bool32 dummy = false;

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

namespace Android
{
bool IsHostADB(const char *hostname)
{
  return !strncmp(hostname, "adb:", 4);
}
string adbExecCommand(const string &args)
{
  string adbExePath = RenderDoc::Inst().GetConfigSetting("adbExePath");
  Process::ProcessResult result;
  Process::LaunchProcess(adbExePath.c_str(), "", args.c_str(), &result);
  RDCLOG("COMMAND: adb %s", args.c_str());
  if(result.strStdout.length())
    // This could be an error (i.e. no package), or just regular output from adb devices.
    RDCLOG("STDOUT:\n%s", result.strStdout.c_str());
  return result.strStdout;
}
void adbForwardPorts()
{
  adbExecCommand(StringFormat::Fmt("forward tcp:%i tcp:%i",
                                   RenderDoc_RemoteServerPort + RenderDoc_AndroidPortOffset,
                                   RenderDoc_RemoteServerPort));
  adbExecCommand(StringFormat::Fmt("forward tcp:%i tcp:%i",
                                   RenderDoc_FirstTargetControlPort + RenderDoc_AndroidPortOffset,
                                   RenderDoc_FirstTargetControlPort));
}
uint32_t StartAndroidPackageForCapture(const char *host, const char *package)
{
  string packageName = basename(string(package));    // Remove leading '/' if any

  adbExecCommand("shell am force-stop " + packageName);
  adbForwardPorts();
  adbExecCommand("shell setprop debug.vulkan.layers VK_LAYER_RENDERDOC_Capture");
  adbExecCommand("shell pm grant " + packageName +
                 " android.permission.WRITE_EXTERNAL_STORAGE");    // Creating the capture file
  adbExecCommand("shell pm grant " + packageName +
                 " android.permission.READ_EXTERNAL_STORAGE");    // Reading the capture thumbnail
  adbExecCommand("shell monkey -p " + packageName + " -c android.intent.category.LAUNCHER 1");

  uint32_t ret = RenderDoc_FirstTargetControlPort + RenderDoc_AndroidPortOffset;
  uint32_t elapsed = 0,
           timeout = 1000 *
                     RDCMAX(5, atoi(RenderDoc::Inst().GetConfigSetting("MaxConnectTimeout").c_str()));
  while(elapsed < timeout)
  {
    // Check if the target app has started yet and we can connect to it.
    TargetControl *control = RENDERDOC_CreateTargetControl(host, ret, "testConnection", false);
    if(control)
    {
      TargetControl_Shutdown(control);
      break;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  // Let the app pickup the setprop before we turn it back off for replaying.
  adbExecCommand("shell setprop debug.vulkan.layers :");

  return ret;
}
}

using namespace Android;
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdctype::str *deviceList)
{
  string adbStdout = adbExecCommand("devices");

  using namespace std;
  istringstream stdoutStream(adbStdout);
  string ret;
  string line;
  while(getline(stdoutStream, line))
  {
    vector<string> tokens;
    split(line, tokens, '\t');
    if(tokens.size() == 2 && trim(tokens[1]) == "device")
    {
      if(ret.length())
        ret += ",";
      ret += tokens[0];
    }
  }

  if(ret.size())
    adbForwardPorts();    // Forward the ports so we can see if a remoteserver/captured app is
                          // already running.

  *deviceList = ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer()
{
  adbExecCommand("shell am force-stop org.renderdoc.renderdoccmd");
  adbForwardPorts();
  adbExecCommand("shell setprop debug.vulkan.layers :");
  adbExecCommand(
      "shell am start -n org.renderdoc.renderdoccmd/.Loader -e renderdoccmd remoteserver");
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_NeedVulkanLayerRegistration(
    uint32_t *flags, rdctype::array<rdctype::str> *myJSONs, rdctype::array<rdctype::str> *otherJSONs)
{
  // stub

  return false;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool elevate)
{
  // stub
}