/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include "common/common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "serialise/serialiser.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "replay/replay_renderer.h"
#include "api/replay/renderdoc_replay.h"

extern "C" RENDERDOC_API float RENDERDOC_CC Maths_HalfToFloat(uint16_t half)
{
	return ConvertFromHalf(half);
}

extern "C" RENDERDOC_API uint16_t RENDERDOC_CC Maths_FloatToHalf(float f)
{
	return ConvertToHalf(f);
}

extern "C" RENDERDOC_API void RENDERDOC_CC Maths_CameraArcball(float dist, const FloatVector &rot, FloatVector *pos, FloatVector *fwd, FloatVector *right)
{
	Camera c;
	c.Arcball(dist, Vec3f(rot.x, rot.y, rot.z));
	
	Vec3f p = c.GetPosition();
	Vec3f f = c.GetForward();
	Vec3f r = c.GetRight();

	pos->x = p.x;
	pos->y = p.y;
	pos->z = p.z;

	fwd->x = f.x;
	fwd->y = f.y;
	fwd->z = f.z;

	right->x = r.x;
	right->y = r.y;
	right->z = r.z;
}

extern "C" RENDERDOC_API void RENDERDOC_CC Maths_CameraFPSLook(const FloatVector &lookpos, const FloatVector &rot, FloatVector *pos, FloatVector *fwd, FloatVector *right)
{
	Camera c;
	c.fpsLook(Vec3f(lookpos.x, lookpos.y, lookpos.z), Vec3f(rot.x, rot.y, rot.z));
	
	Vec3f p = c.GetPosition();
	Vec3f f = c.GetForward();
	Vec3f r = c.GetRight();

	pos->x = p.x;
	pos->y = p.y;
	pos->z = p.z;

	fwd->x = f.x;
	fwd->y = f.y;
	fwd->z = f.z;

	right->x = r.x;
	right->y = r.y;
	right->z = r.z;
}

extern "C" RENDERDOC_API
int RENDERDOC_CC RENDERDOC_GetAPIVersion()
{
	return RENDERDOC_API_VERSION;
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_Shutdown()
{
	RenderDoc::Inst().Shutdown();
	LibraryHooks::GetInstance().RemoveHooks();
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_LogText(const char *text)
{
	RDCLOG("%s", text);
}

extern "C" RENDERDOC_API
const char* RENDERDOC_CC RENDERDOC_GetLogFile()
{
	return RDCGETLOGFILE();
}

extern "C" RENDERDOC_API
bool32 RENDERDOC_CC RENDERDOC_GetCapture(uint32_t idx, char *logfile, uint32_t *pathlength, uint64_t *timestamp)
{
	vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();

	if(idx >= (uint32_t)caps.size())
	{
		if(logfile) logfile[0] = 0;
		if(pathlength) *pathlength = 0;
		if(timestamp) *timestamp = 0;
		return false;
	}

	CaptureData &c = caps[idx];

	if(logfile)
		memcpy(logfile, c.path.c_str(), sizeof(char)*(c.path.size()+1));
	if(pathlength)
		*pathlength = uint32_t(c.path.size()+1);
	if(timestamp)
		*timestamp = c.timestamp;

	return true;
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs, bool32 crashed)
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

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_UnloadCrashHandler()
{
	RenderDoc::Inst().UnloadCrashHandler();
}

extern "C" RENDERDOC_API
bool32 RENDERDOC_CC RENDERDOC_SupportLocalReplay(const char *logfile, rdctype::str *driver)
{
	if(logfile == NULL)
		return false;

	RDCDriver driverType = RDC_Unknown;
	string driverName = "";
	RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, NULL);

	if(driver) *driver = driverName;

	return RenderDoc::Inst().HasReplayDriver(driverType);
}

extern "C" RENDERDOC_API
ReplayCreateStatus RENDERDOC_CC RENDERDOC_CreateReplayRenderer(const char *logfile, float *progress, ReplayRenderer **rend)
{
	if(rend == NULL) return eReplayCreate_InternalError;

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

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SetLogFile(const char *logfile)
{
	RDCLOG("Using logfile %s", logfile);
	RenderDoc::Inst().SetLogFile(logfile);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SetCaptureOptions(const CaptureOptions *opts)
{
	RDCLOG("Setting capture options");
	RenderDoc::Inst().SetCaptureOptions(opts);
}

extern "C" RENDERDOC_API
uint32_t RENDERDOC_CC RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
									 const char *logfile, const CaptureOptions *opts, bool32 waitForExit)
{
	return Process::CreateAndInjectIntoProcess(app, workingDir, cmdLine, logfile, opts, waitForExit != 0);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions *opts)
{
	Process::StartGlobalHook(pathmatch, logfile, opts);
}

extern "C" RENDERDOC_API
uint32_t RENDERDOC_CC RENDERDOC_InjectIntoProcess(uint32_t pid, const char *logfile, const CaptureOptions *opts, bool32 waitForExit)
{
	return Process::InjectIntoProcess(pid, logfile, opts, waitForExit != 0);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SetActiveWindow(void *device, void *wndHandle)
{
	RenderDoc::Inst().SetActiveWindow(device, wndHandle);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_TriggerCapture()
{
	RenderDoc::Inst().TriggerCapture();
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_StartFrameCapture(void *device, void *wndHandle)
{
	RenderDoc::Inst().StartFrameCapture(device, wndHandle);
}

extern "C" RENDERDOC_API
bool32 RENDERDOC_CC RENDERDOC_EndFrameCapture(void *device, void *wndHandle)
{
	return RenderDoc::Inst().EndFrameCapture(device, wndHandle);
}

extern "C" RENDERDOC_API
uint32_t RENDERDOC_CC RENDERDOC_GetOverlayBits()
{
	return RenderDoc::Inst().GetOverlayBits();
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_MaskOverlayBits(uint32_t And, uint32_t Or)
{
	RenderDoc::Inst().MaskOverlayBits(And, Or);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SetFocusToggleKeys(KeyButton *keys, int num)
{
	RenderDoc::Inst().SetFocusKeys(keys, num);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SetCaptureKeys(KeyButton *keys, int num)
{
	RenderDoc::Inst().SetCaptureKeys(keys, num);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_QueueCapture(uint32_t frameNumber)
{
	RenderDoc::Inst().QueueCapture(frameNumber);
}

extern "C" RENDERDOC_API
bool32 RENDERDOC_CC RENDERDOC_GetThumbnail(const char *filename, byte *buf, uint32_t &len)
{
	Serialiser ser(filename, Serialiser::READING, false);

	if(ser.HasError())
		return false;

	ser.Rewind();

	int chunkType = ser.PushContext(NULL, 1, false);

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
	
	if(buf == NULL)
	{
		len = (uint32_t)thumblen;
		return true;
	}

	if(thumblen > len)
	{
		delete[] jpgbuf;
		return false;
	}

	memcpy(buf, jpgbuf, thumblen);

	delete[] jpgbuf;

	return true;
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem)
{
	rdctype::array<char>::deallocate(mem);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_InitRemoteAccess(uint32_t *ident)
{
	if(ident) *ident = RenderDoc::Inst().GetRemoteAccessIdent();
}

extern "C" RENDERDOC_API
uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteConnections(const char *host, uint32_t *idents)
{
	if(idents == NULL)
		return RenderDoc_LastCaptureNetworkPort-RenderDoc_FirstCaptureNetworkPort+1;

	string s = "localhost";
	if(host != NULL && host[0] != '\0')
		s = host;

	uint32_t numIdents = 0;

	for(uint16_t ident = RenderDoc_FirstCaptureNetworkPort; ident <= RenderDoc_LastCaptureNetworkPort; ident++)
	{
		Network::Socket *sock = Network::CreateClientSocket(s.c_str(), ident, 250);

		if(sock)
		{
			*idents = (uint32_t)ident;
			idents++;
			numIdents++;
			SAFE_DELETE(sock);
		}
	}

	return numIdents;
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_SpawnReplayHost(volatile bool32 *killReplay)
{
	bool32 dummy = false;

	if(killReplay == NULL) killReplay = &dummy;

	RenderDoc::Inst().BecomeReplayHost(*killReplay);
}
