#pragma once

#include <list>
#include "common/common.h"
#include "common/timing.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "gles_common.h"
#include "gles_hookset.h"

#include "gles_chunks.h"
#include "gles_resources.h"
#include "gles_manager.h"

class GLESReplay;

struct GLESInitParams : public RDCInitParams
{
    GLESInitParams()
    {
    }

    ReplayCreateStatus Serialise()
    {
        printf("CALL: %s\n", __FUNCTION__);
        uint32_t debug = 22;
        m_pSerialiser->Serialise("DEBUG", debug);
        return eReplayCreate_Success;
    }
};

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

class WrappedGLES : public IFrameCapturer
{
public:
    WrappedGLES(const char *logfile, const GLHookSet &funcs, GLESInitParams &initParams);
    virtual ~WrappedGLES();

    IMPLEMENT_FUNCTION_SERIALISED(void,  glClear(GLbitfield mask));
    IMPLEMENT_FUNCTION_SERIALISED(void,  glClearColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
    IMPLEMENT_FUNCTION_SERIALISED(void,  glViewport (GLint x, GLint y, GLsizei width, GLsizei height));
    IMPLEMENT_FUNCTION_SERIALISED(GLenum, glGetError());

    void GetDisplay(EGLNativeDisplayType display_id) { /* TODO */ }
    void CreateContext(void);
    bool SwapBuffers(EGLDisplay dpy, EGLSurface surface);

    void StartFrameCapture(void *dev, void *wnd);
    bool EndFrameCapture(void *dev, void *wnd);

    GLESReplay *GetReplay() { return m_Replay; }
    Serialiser *GetSerialiser() { return m_pSerialiser; }

    static const char *GetChunkName(uint32_t idx);

    void ReadLogInitialisation(void);
    void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

private:
    void ProcessChunk(uint64_t offset, GLESChunkType context);
    void ContextEndFrame(void);

    GLESResourceManager *GetResourceManager() { return m_ResourceManager; }

    GLESResourceManager *m_ResourceManager;

    ResourceId m_ContextResourceID;
    GLESResourceRecord *m_ContextRecord;

    GLESReplay *m_Replay;
public:
    const GLHookSet &m_Real;
private:

    // internals
    GLESInitParams m_InitParams;
    Serialiser *m_pSerialiser;
    LogState m_State;
    uint32_t m_FrameCounter;
};
