#define RENDERDOC_WINDOWING_XLIB 1

#include "gles_replay.h"

#include "gles_driver.h"
#include "official/egl_func_typedefs.h"
#include <dlfcn.h>

APIProperties GLESReplay::GetAPIProperties()
{
    APIProperties ret;

    ret.pipelineType = ePipelineState_OpenGL;
    ret.degraded = false;

    return ret;
}

vector<ResourceId> GLESReplay::GetBuffers()
{
    vector<ResourceId> ret;

    return ret;
}

FetchBuffer GLESReplay::GetBuffer(ResourceId id)
{
    FetchBuffer ret;
    return ret;
}

vector<ResourceId> GLESReplay::GetTextures()
{
    vector<ResourceId> ret;
    return ret;
}

FetchTexture GLESReplay::GetTexture(ResourceId id)
{
    FetchTexture ret;
    return ret;
}

vector<DebugMessage> GLESReplay::GetDebugMessages()
{
    vector<DebugMessage> ret;
    return ret;
}

ShaderReflection *GLESReplay::GetShader(ResourceId shader, string entryPoint)
{
    return NULL;
}

vector<EventUsage> GLESReplay::GetUsage(ResourceId id)
{
    vector<EventUsage> ret;
    return ret;
}

void GLESReplay::SavePipelineState()
{
}

FetchFrameRecord GLESReplay::GetFrameRecord()
{
    FetchFrameRecord ret;
    return ret;
}

void GLESReplay::ReadLogInitialisation()
{
    m_pDriver->ReadLogInitialisation();
}

void GLESReplay::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
  GLNOTIMP("SetContextFilter");
}

void GLESReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
    m_pDriver->ReplayLog(0, endEventID, replayType);
}

vector<uint32_t> GLESReplay::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> passEvents;
   return passEvents;
}

ResourceId GLESReplay::GetLiveID(ResourceId id)
{
    ResourceId retId;
    return retId;
}

void GLESReplay::InitPostVSBuffers(const vector<uint32_t> &passEvents)
{
}

void GLESReplay::InitPostVSBuffers(uint32_t eventID)
{
}

MeshFormat GLESReplay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
    MeshFormat ret;
    return ret;
}

void GLESReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret)
{
}

byte *GLESReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                               FormatComponentType typeHint, bool resolve, bool forceRGBA8unorm,
                               float blackPoint, float whitePoint, size_t &dataSize)
{
    dataSize = 0;
    return NULL;
}

void GLESReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                 ShaderStageType type, ResourceId *id, string *errors)
{
}

void GLESReplay::ReplaceResource(ResourceId from, ResourceId to)
{
}

void GLESReplay::RemoveReplacement(ResourceId id)
{
}

void GLESReplay::FreeTargetResource(ResourceId id)
{
}

vector<uint32_t> GLESReplay::EnumerateCounters()
{
    vector<uint32_t> ret;
//  ret.push_back(eCounter_EventGPUDuration);
    return ret;
}

void GLESReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
}

vector<CounterResult> GLESReplay::FetchCounters(const vector<uint32_t> &counters)
{
    vector<CounterResult> ret;
    return ret;
}

void GLESReplay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                    vector<ShaderVariable> &outvars, const vector<byte> &data)
{
}

vector<PixelModification> GLESReplay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                 uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                                                 uint32_t sampleIdx, FormatComponentType typeHint)
{
  GLNOTIMP("GLESReplay::PixelHistory");
  return vector<PixelModification>();
}

ShaderDebugTrace GLESReplay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                       uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  GLNOTIMP("DebugVertex");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLESReplay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                      uint32_t primitive)
{
  GLNOTIMP("DebugPixel");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLESReplay::DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
  GLNOTIMP("DebugThread");
  return ShaderDebugTrace();
}

ResourceId GLESReplay::RenderOverlay(ResourceId texid, FormatComponentType typeHint,
                                   TextureDisplayOverlay overlay, uint32_t eventID,
                                   const vector<uint32_t> &passEvents)
{
    ResourceId id;
    return id;
}

bool GLESReplay::IsRenderOutput(ResourceId id)
{
    return false;
}

void GLESReplay::FileChanged()
{
}

void GLESReplay::InitCallstackResolver()
{
  m_pDriver->GetSerialiser()->InitCallstackResolver();
}

bool GLESReplay::HasCallstacks()
{
  return m_pDriver->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *GLESReplay::GetCallstackResolver()
{
  return m_pDriver->GetSerialiser()->GetCallstackResolver();
}

bool GLESReplay::IsRemoteProxy()
{
    return false;
}

vector<WindowingSystem> GLESReplay::GetSupportedWindowSystems()
{
    vector<WindowingSystem> ret;
    // only Xlib supported for GLX. We can't report XCB here since we need
    // the Display, and that can't be obtained from XCB. The application is
    // free to use XCB internally but it would have to create a hybrid and
    // initialise XCB out of Xlib, to be able to provide the display and
    // drawable to us.
    ret.push_back(eWindowingSystem_Xlib);
    return ret;
}

#define REAL(NAME) NAME ##_real
#define LOAD_SYM(NAME) REAL(NAME) = (PFN_##NAME)dlsym(RTLD_NEXT, #NAME)
#define DEF_FUNC(NAME) static PFN_##NAME REAL(NAME) = NULL

DEF_FUNC(eglBindAPI);
DEF_FUNC(eglGetDisplay);
DEF_FUNC(eglInitialize);
DEF_FUNC(eglChooseConfig);
DEF_FUNC(eglGetConfigAttrib);
DEF_FUNC(eglCreateContext);
DEF_FUNC(eglCreateWindowSurface);
DEF_FUNC(eglQuerySurface);
DEF_FUNC(eglMakeCurrent);

uint64_t GLESReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
    Display *display = NULL;
    Drawable draw = 0;

    if(system == eWindowingSystem_Xlib)
    {
        XlibWindowData *xlib = (XlibWindowData *)data;

        display = xlib->display;
        draw = xlib->window;
    }
    else
    {
        RDCERR("Unexpected window system %u", system);
    }

    LOAD_SYM(eglBindAPI);
    LOAD_SYM(eglGetDisplay);
    LOAD_SYM(eglInitialize);
    LOAD_SYM(eglChooseConfig);
    LOAD_SYM(eglGetConfigAttrib);
    LOAD_SYM(eglCreateContext);
    LOAD_SYM(eglCreateWindowSurface);
    LOAD_SYM(eglQuerySurface);
    LOAD_SYM(eglMakeCurrent);

    REAL(eglBindAPI)(EGL_OPENGL_ES_API);


    EGLDisplay egl_display = eglGetDisplay_real(display);
    if (!egl_display) {
        printf("Error: eglGetDisplay() failed\n");
        return -1;
    }

    int egl_major;
    int egl_minor;

    if (!eglInitialize_real(egl_display, &egl_major, &egl_minor)) {
        printf("Error: eglInitialize() failed\n");
         return -1;
    }

    static const EGLint attribs[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;

    if (!REAL(eglChooseConfig)(egl_display, attribs, &config, 1, &num_configs)) {
        printf("Error: couldn't get an EGL visual config\n");
        return -1;
    }

    EGLint vid;
    if (!REAL(eglGetConfigAttrib)(egl_display, config, EGL_NATIVE_VISUAL_ID, &vid)) {
        printf("Error: eglGetConfigAttrib() failed\n");
        return -1;
    }

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLContext ctx = REAL(eglCreateContext)(egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
    EGLSurface surface = REAL(eglCreateWindowSurface)(egl_display, config, (EGLNativeWindowType)draw, NULL);

    OutputWindow outputWin;
    outputWin.context = ctx;
    outputWin.surface = surface;
    outputWin.display = display;

    REAL(eglQuerySurface)(egl_display, surface, EGL_HEIGHT, &outputWin.height);
    REAL(eglQuerySurface)(egl_display, surface, EGL_WIDTH, &outputWin.width);


    printf("New output window (%dx%d)\n", outputWin.height, outputWin.width);
    MakeCurrentReplayContext(&outputWin);

    uint64_t windowId = m_outputWindowIds++;
    m_outputWindows[windowId] = outputWin;

    return windowId;
}

void GLESReplay::DestroyOutputWindow(uint64_t id)
{
}

bool GLESReplay::CheckResizeOutputWindow(uint64_t id)
{
    return false;
}

void GLESReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
}

void GLESReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
}

void GLESReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
}

void GLESReplay::BindOutputWindow(uint64_t id, bool depth)
{
}

bool GLESReplay::IsOutputWindowVisible(uint64_t id)
{
    return false;
}

void GLESReplay::FlipOutputWindow(uint64_t id)
{
}

bool GLESReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                           FormatComponentType typeHint, float *minval, float *maxval)
{
    return false;
}
bool GLESReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                          FormatComponentType typeHint, float minval, float maxval, bool channels[4],
                             vector<uint32_t> &histogram)
{
    return false;
}


ResourceId GLESReplay::CreateProxyTexture(const FetchTexture &templateTex)
{
    ResourceId id;
    return id;
}

ResourceId GLESReplay::CreateProxyBuffer(const FetchBuffer &templateBuf)
{
    ResourceId id;
    return id;
}

void GLESReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
}

void GLESReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
/*
  GLuint buf = m_pDriver->GetResourceManager()->GetCurrentResource(bufid).name;

  m_pDriver->glNamedBufferSubDataEXT(buf, 0, dataSize, data);
*/
}

void GLESReplay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg)
{
}

bool GLESReplay::RenderTexture(TextureDisplay cfg)
{
  //return RenderTextureInternal(cfg, true);
    return false;
}

void GLESReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                   ShaderStageType type, ResourceId *id, string *errors)
{
}

ResourceId GLESReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                                        uint32_t sampleIdx, FormatComponentType typeHint)
{
    ResourceId id;
    return id;
}

void GLESReplay::FreeCustomShader(ResourceId id)
{
    if(id == ResourceId())
        return;

  //m_pDriver->glDeleteProgram(m_pDriver->GetResourceManager()->GetCurrentResource(id).name);
}

void GLESReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
}

void GLESReplay::RenderHighlightBox(float w, float h, float scale)
{
}

void GLESReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                            uint32_t sample, FormatComponentType typeHint, float pixel[4])
{
}


uint32_t GLESReplay::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
    return ~0U;
}


void GLESReplay::MakeCurrentReplayContext(GLESWindowingData* windowData)
{
    REAL(eglMakeCurrent)(windowData->display, windowData->surface, windowData->surface, windowData->context);
}


static DriverRegistration GLDriverRegistration(RDC_OpenGL, "GLES", &GLES_CreateReplayDevice);
