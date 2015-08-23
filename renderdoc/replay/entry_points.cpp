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
#include "replay/replay_renderer.h"
#include "api/replay/renderdoc_replay.h"

// these entry points are for the replay/analysis side - not for the application.

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_NumVerticesPerPrimitive(PrimitiveTopology topology)
{
	// strips/loops/fans have the same number of indices for a single primitive
	// as their list friends
	switch(topology)
	{
		default:
		case eTopology_Unknown:
			break;
		case eTopology_PointList:
			return 1;
		case eTopology_LineList:
		case eTopology_LineStrip:
		case eTopology_LineLoop:
			return 2;
		case eTopology_TriangleList:
		case eTopology_TriangleStrip:
		case eTopology_TriangleFan:
			return 3;
		case eTopology_LineList_Adj:
		case eTopology_LineStrip_Adj:
			return 4;
		case eTopology_TriangleList_Adj:
		case eTopology_TriangleStrip_Adj:
			return 6;
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
			return uint32_t(topology - eTopology_PatchList_1CPs + 1);
	}

	return 0;
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC Topology_VertexOffset(PrimitiveTopology topology, uint32_t primitive)
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
			return primitive*2;
		case eTopology_TriangleFan:
			RDCERR("Cannot get VertexOffset for triangle fan!");
			break;
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

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_RotateArcball(Camera *c, float ax, float ay, float bx, float by)
{
	c->RotateArcball(Vec2f(ax, ay), Vec2f(bx, by));
}

extern "C" RENDERDOC_API void RENDERDOC_CC Camera_GetBasis(Camera *c, FloatVector *pos, FloatVector *fwd, FloatVector *right, FloatVector *up)
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
uint32_t RENDERDOC_CC RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
									 const char *logfile, const CaptureOptions *opts, bool32 waitForExit)
{
	return Process::LaunchAndInjectIntoProcess(app, workingDir, cmdLine, logfile, opts, waitForExit != 0);
}

extern "C" RENDERDOC_API
void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts)
{
	*opts = CaptureOptions();
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
void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz)
{
	return rdctype::array<char>::allocate((size_t)sz);
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
