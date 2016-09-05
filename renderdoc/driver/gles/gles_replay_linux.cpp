#include "gles_replay.h"

#include <dlfcn.h>
#include "gles_driver.h"
#include "official/egl_func_typedefs.h"

static PFN_eglGetProcAddress eglGetProcAddress_real = NULL;

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
    RDCDEBUG("Creating an GLES replay device");

    eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(RTLD_NEXT, "eglGetProcAddress");


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
