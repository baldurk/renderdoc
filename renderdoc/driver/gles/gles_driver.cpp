#include "gles_driver.h"

#include "serialise/serialiser.h"
#include "serialise/string_utils.h"

#include "gles_chunks.h"
#include "gles_replay.h"

const char *GLESChunkNames[] = {
    "WrappedGLES::Init",

    "glClear",
    "glClearColor",
    "glViewport",

    "EndCapture",
};

WrappedGLES::WrappedGLES(const char *logfile, const GLHookSet &funcs, GLESInitParams &initParams)
    : m_Real(funcs)
    , m_InitParams(initParams)
{
    m_FrameCounter = 0;
    m_Replay = new GLESReplay(this);
    if(RenderDoc::Inst().IsReplayApp())
    {
        m_State = READING;
        if (logfile)
        {
            m_pSerialiser = new Serialiser(logfile, Serialiser::READING, false);
        }
        else
        {
            byte dummy[4];
            m_pSerialiser = new Serialiser(4, dummy, false);
        }
/*
        // once GL driver is more tested, this can be disabled
        if (m_Real.glDebugMessageCallback)
        {
            m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
            m_Real.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
*/
    }
    else
    {
        m_State = WRITING_IDLE;
        m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, false);

        m_ResourceManager = new GLESResourceManager(m_State, m_pSerialiser, this);

        m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
        m_ContextRecord->DataInSerialiser = false;
        m_ContextRecord->Length = 0;
        m_ContextRecord->SpecialResource = true;
    }

    m_pSerialiser->SetDebugText(true);
    m_pSerialiser->SetChunkNameLookup(&GetChunkName);
}

WrappedGLES::~WrappedGLES()
{
    SAFE_DELETE(m_pSerialiser);
}

void WrappedGLES::StartFrameCapture(void *dev, void *wnd)
{
    printf("CALLED: %s\n", __FUNCTION__);
    if(m_State != WRITING_IDLE)
        return;

    RenderDoc::Inst().SetCurrentDriver(RDC_OpenGL);

    m_State = WRITING_CAPFRAME;
}

bool WrappedGLES::EndFrameCapture(void *dev, void *wnd)
{
    printf("CALLED: %s\n", __FUNCTION__);
    if(m_State != WRITING_CAPFRAME)
        return true;

    ContextEndFrame();
    byte *jpgbuf = NULL;
    int len = 0;
    uint32_t thwidth = 0;
    uint32_t thheight = 0;

    Serialiser *fileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(
        m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

    GetResourceManager()->InsertReferencedChunks(fileSerialiser);

    {
      RDCDEBUG("Getting Resource Record");

      GLESResourceRecord *record = m_ResourceManager->GetResourceRecord(m_ContextResourceID);

      RDCDEBUG("Accumulating context resource list");

      map<int32_t, Chunk *> recordlist;
      record->Insert(recordlist);

      RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
        fileSerialiser->Insert(it->second);

      RDCDEBUG("Done");
    }

    fileSerialiser->FlushToDisk();

    RenderDoc::Inst().SuccessfullyWrittenLog();

    SAFE_DELETE(fileSerialiser);

    m_State = WRITING_IDLE;

    return true;
}

const char *WrappedGLES::GetChunkName(uint32_t idx)
{
  if(idx == CREATE_PARAMS)
    return "Create Params";
  if(idx == THUMBNAIL_DATA)
    return "Thumbnail Data";
  if(idx == DRIVER_INIT_PARAMS)
    return "Driver Init Params";
  if(idx == INITIAL_CONTENTS)
    return "Initial Contents";
  if(idx < FIRST_CHUNK_ID || idx >= NUM_GLES_CHUNKS)
    return "<unknown>";
  printf("Chunk info --> %d %d\n", idx, idx - FIRST_CHUNK_ID);
  return GLESChunkNames[idx - FIRST_CHUNK_ID];
}

void WrappedGLES::ReadLogInitialisation()
{
    m_pSerialiser->Rewind();

    for (;;) {
        uint64_t offset = m_pSerialiser->GetOffset();
        GLESChunkType context = (GLESChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
/*
        if(context == CAPTURE_SCOPE)
        {
            // immediately read rest of log into memory
            m_pSerialiser->SetPersistentBlock(offset);
        }
*/
        ProcessChunk(offset, context);

        m_pSerialiser->PopContext(context);

        RenderDoc::Inst().SetProgress(FileInitialRead, float(offset) / float(m_pSerialiser->GetSize()));
/*
        if(context == CAPTURE_SCOPE)
        {
            GetResourceManager()->ApplyInitialContents();

            ContextReplayLog(READING, 0, 0, false);
        }

        if(context == CAPTURE_SCOPE)
            break;
*/
        if(m_pSerialiser->AtEnd())
            break;
    }
}

void WrappedGLES::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
    printf("____ %s (%d, %d, %d)\n", __FUNCTION__, startEventID, endEventID, replayType);
    m_pSerialiser->Rewind();
    for (;;) {
        uint64_t offset = m_pSerialiser->GetOffset();
        GLESChunkType context = (GLESChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

        ProcessChunk(offset, context);

        m_pSerialiser->PopContext(context);

        if(m_pSerialiser->AtEnd())
            break;
    }

}

void WrappedGLES::ProcessChunk(unsigned long offset, GLESChunkType chunk)
{
    printf("CALL: %s (%lu) (%d, %s)\n", __FUNCTION__, offset, chunk, GetChunkName(chunk));
    switch (chunk)
    {
        case CLEAR: Serialise_glClear(0); break;
        case CLEAR_COLOR: Serialise_glClearColor(0, 0, 0, 0); break;
        case VIEWPORT: Serialise_glViewport(0, 0, 0, 0); break;
        case CONTEXT_CAPTURE_FOOTER:
        {
            bool HasCallstack = false;
            m_pSerialiser->Serialise("HasCallstack", HasCallstack);

            if(HasCallstack)
            {
                uint32_t numLevels = 0;
                uint64_t *stack = NULL;

                m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

                m_pSerialiser->SetCallstack(stack, numLevels);

                SAFE_DELETE_ARRAY(stack);
            }

            break;
        }
        default:
        {
            // ignore system chunks
            if((int)chunk == (int)INITIAL_CONTENTS)
                GetResourceManager()->Serialise_InitialState(ResourceId(), GLESResource(MakeNullResource));
            else if((int)chunk < (int)FIRST_CHUNK_ID)
                m_pSerialiser->SkipCurrentChunk();
            else
                printf("Unknown chunk: %d\n", chunk);
        }
    }
}

void WrappedGLES::CreateContext(void)
{
    RenderDoc::Inst().AddDeviceFrameCapturer(this, this);
    RenderDoc::Inst().AddFrameCapturer(this, this, this);
}

bool WrappedGLES::SwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    printf("CALL: %s\n", __FUNCTION__);
    if(m_State == WRITING_IDLE)
        RenderDoc::Inst().Tick();

    m_FrameCounter++;

    bool activeWindow = RenderDoc::Inst().IsActiveWindow(NULL, NULL);

    // kill any current capture that isn't application defined
    if(m_State == WRITING_CAPFRAME)
    {
        RenderDoc::Inst().EndFrameCapture(NULL, NULL);
    }


    bool shouldTrigger = RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter);
    printf(" TRIGGER: %d (%d)\n", shouldTrigger ? 1 : 0, m_FrameCounter);
    if (shouldTrigger && (m_State == WRITING_IDLE))
    {
        RenderDoc::Inst().StartFrameCapture(NULL, NULL);
    }

    return true;
}

void WrappedGLES::ContextEndFrame(void)
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);

  bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
  m_pSerialiser->Serialise("HasCallstack", HasCallstack);

  if(HasCallstack)
  {
    Callstack::Stackwalk *call = Callstack::Collect();

    uint32_t numLevels = (uint32_t)call->NumLevels();
    uint64_t *stack = (uint64_t *)call->GetAddrs();

    m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

    delete call;
  }

  m_ContextRecord->AddChunk(scope.Get());
}
