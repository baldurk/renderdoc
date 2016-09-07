#include "gles_replay.h"

#include <dlfcn.h>
#include "gles_driver.h"
#include "official/egl_func_typedefs.h"

#define REAL(NAME) NAME ##_real
#define LOAD_SYM(NAME) REAL(NAME) = (PFN_##NAME)dlsym(RTLD_NEXT, #NAME)
#define DEF_FUNC(NAME) static PFN_##NAME REAL(NAME) = NULL

DEF_FUNC(eglSwapBuffers);

static PFN_eglGetProcAddress eglGetProcAddress_real = NULL;

void GLESReplay::SwapBuffers(GLESWindowingData* data)
{
    printf("CALL: %s\n", __FUNCTION__);
    REAL(eglSwapBuffers)(data->eglDisplay, data->surface);
    printf("ERR: %d\n", m_pDriver->m_Real.glGetError());
}

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
    RDCDEBUG("Creating an GLES replay device");

    eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(RTLD_NEXT, "eglGetProcAddress");
    LOAD_SYM(eglSwapBuffers);

    GLESInitParams initParams;
    RDCDriver driverType = RDC_OpenGL;
    string driverName = "OpenGL";
    uint64_t machineIdent = 0;
    if(logfile)
    {
        auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
        if(status != eReplayCreate_Success)
            return status;
    }

    WrappedGLES* gles = new WrappedGLES(logfile, GetRealGLFunctions(), initParams);
    *driver = gles->GetReplay();
    return eReplayCreate_Success;
}
