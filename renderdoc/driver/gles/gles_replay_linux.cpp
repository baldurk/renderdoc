#include "gles_replay.h"
#include "gles_driver.h"

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
    RDCDEBUG("Creating an GLES replay device");

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
