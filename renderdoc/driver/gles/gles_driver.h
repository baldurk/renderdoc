/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#pragma once

#include <list>
#include "common/common.h"
#include "common/timing.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "gles_common.h"
#include "gles_hookset.h"
#include "gles_manager.h"
#include "gles_renderstate.h"
#include "gles_replay.h"
#include "gles_resources.h"

using std::list;

class SafeTextureBinder
{
public:
    SafeTextureBinder(const GLHookSet &hooks, GLuint texture, GLenum target)
      : m_Real(hooks)
      , m_target(TextureTarget(target))
    {
      m_Real.glGetIntegerv(TextureBinding(m_target), &m_previous);
      m_Real.glBindTexture(m_target, texture);
    }

    ~SafeTextureBinder()
    {
      m_Real.glBindTexture(m_target, m_previous);
    }
private:
    SafeTextureBinder(const SafeTextureBinder&);
    SafeTextureBinder& operator=(const SafeTextureBinder&);

    const GLHookSet &m_Real;
    GLenum m_target;
    GLint m_previous;
};

class SafeVAOBinder
{
public:
    SafeVAOBinder(const GLHookSet &hooks, GLuint vao)
      : m_Real(hooks)
    {
      m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, &m_previous);
      m_Real.glBindVertexArray(vao);
    }

    ~SafeVAOBinder()
    {
      m_Real.glBindVertexArray(m_previous);
    }
private:
    SafeVAOBinder(const SafeVAOBinder&);
    SafeVAOBinder& operator=(const SafeVAOBinder&);

    const GLHookSet &m_Real;
    GLint m_previous;
};

class SafeBufferBinder
{
public:
    SafeBufferBinder(const GLHookSet &hooks, GLenum target, GLuint buffer)
      : m_Real(hooks)
      , m_target(target)
      , m_active(true)
    {
      m_Real.glGetIntegerv(BufferBinding(m_target), &m_previous);
      m_Real.glBindBuffer(m_target, buffer);
    }

    SafeBufferBinder(const GLHookSet &hooks)
      : m_Real(hooks)
      , m_target(eGL_NONE)
      , m_previous(0)
      , m_active(false)
    {
    }

    void saveBinding(GLenum target, GLuint buffer)
    {
      m_target = target;
      m_Real.glGetIntegerv(BufferBinding(m_target), &m_previous);
      m_Real.glBindBuffer(m_target, buffer);
    }

    ~SafeBufferBinder()
    {
      if (m_active)
        m_Real.glBindBuffer(m_target, m_previous);
    }
private:
    SafeBufferBinder(const SafeBufferBinder&);
    SafeBufferBinder& operator=(const SafeBufferBinder&);

    const GLHookSet &m_Real;
    GLenum m_target;
    GLint m_previous;
    bool m_active;
};

class SafeFramebufferBinder
{
public:
    SafeFramebufferBinder(const GLHookSet &hooks, GLenum target, GLuint framebuffer)
      : SafeFramebufferBinder(hooks)
    {
      m_Real.glBindFramebuffer(target, framebuffer);
    }

    SafeFramebufferBinder(const GLHookSet &hooks, GLuint drawFramebuffer, GLuint readFramebuffer)
      : SafeFramebufferBinder(hooks)
    {
      m_Real.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFramebuffer);
      m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, readFramebuffer);
    }

    ~SafeFramebufferBinder()
    {
      m_Real.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, m_prevDrawFramebuffer);
      m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, m_prevReadFramebuffer);
    }

private:
    SafeFramebufferBinder(const GLHookSet &hooks)
      : m_Real(hooks)
      , m_prevDrawFramebuffer(0)
      , m_prevReadFramebuffer(0)
    {
      m_Real.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&m_prevDrawFramebuffer);
      m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&m_prevReadFramebuffer);
    }

    SafeFramebufferBinder(const SafeFramebufferBinder&);
    SafeFramebufferBinder& operator=(const SafeFramebufferBinder&);

    const GLHookSet &m_Real;
    GLuint m_prevDrawFramebuffer;
    GLuint m_prevReadFramebuffer;
};

template<GLenum FramebufferTarget>
class SafeFramebufferBinderBase
{
public:
  SafeFramebufferBinderBase(const GLHookSet &hooks, GLuint framebuffer)
      : m_Real(hooks)
      , m_previous(0)
    {
      m_Real.glGetIntegerv(FramebufferBinding(FramebufferTarget), (GLint *)&m_previous);
      m_Real.glBindFramebuffer(FramebufferTarget, framebuffer);
    }

    ~SafeFramebufferBinderBase()
    {
      m_Real.glBindFramebuffer(FramebufferTarget, m_previous);
    }

private:
    SafeFramebufferBinderBase(const SafeFramebufferBinderBase&);
    SafeFramebufferBinderBase& operator=(const SafeFramebufferBinderBase&);

    const GLHookSet &m_Real;
    GLuint m_previous;
};

typedef SafeFramebufferBinderBase<eGL_DRAW_FRAMEBUFFER> SafeDrawFramebufferBinder;
typedef SafeFramebufferBinderBase<eGL_READ_FRAMEBUFFER> SafeReadFramebufferBinder;

class SafeRenderbufferBinder
{
public:
  SafeRenderbufferBinder(const GLHookSet &hooks, GLuint renderbuffer)
      : m_Real(hooks)
      , m_previous(0)
    {
      m_Real.glGetIntegerv(eGL_RENDERBUFFER_BINDING, (GLint *)&m_previous);
      m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, renderbuffer);
    }

    ~SafeRenderbufferBinder()
    {
      m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, m_previous);
    }

private:
    SafeRenderbufferBinder(const SafeRenderbufferBinder&);
    SafeRenderbufferBinder& operator=(const SafeRenderbufferBinder&);

    const GLHookSet &m_Real;
    GLuint m_previous;
};

struct GLESInitParams : public RDCInitParams
{
  GLESInitParams();
  ReplayCreateStatus Serialise();

  uint32_t colorBits;
  uint32_t depthBits;
  uint32_t stencilBits;
  uint32_t isSRGB;
  uint32_t multiSamples;
  uint32_t width;
  uint32_t height;

  static const uint32_t GL_SERIALISE_VERSION = 0x0000011;

  // backwards compatibility for old logs described at the declaration of this array
  static const uint32_t GL_NUM_SUPPORTED_OLD_VERSIONS = 1;
  static const uint32_t GL_OLD_VERSIONS[GL_NUM_SUPPORTED_OLD_VERSIONS];

  // version number internal to opengl stream
  uint32_t SerialiseVersion;
};

enum CaptureFailReason
{
  CaptureSucceeded = 0,
  CaptureFailed_UncappedUnmap,
};

struct DrawcallTreeNode
{
  DrawcallTreeNode() {}
  explicit DrawcallTreeNode(const FetchDrawcall &d) : draw(d) {}
  FetchDrawcall draw;
  vector<DrawcallTreeNode> children;

  DrawcallTreeNode &operator=(const FetchDrawcall &d)
  {
    *this = DrawcallTreeNode(d);
    return *this;
  }

  vector<FetchDrawcall> Bake()
  {
    vector<FetchDrawcall> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].draw;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

struct Replacement
{
  Replacement(ResourceId i, GLResource r) : id(i), res(r) {}
  ResourceId id;
  GLResource res;
};

class WrappedGLES : public IFrameCapturer
{
private:
  GLHookSet m_Real;
  GLHookSet initRealWrapper(const GLHookSet& hooks);

  friend class GLESReplay;
  friend class GLResourceManager;

  vector<DebugMessage> m_DebugMessages;
  void Serialise_DebugMessages();
  vector<DebugMessage> GetDebugMessages();

  GLDEBUGPROC m_RealDebugFunc;
  const void *m_RealDebugFuncParam;
  string m_DebugMsgContext;

  bool m_SuppressDebugMessages;

  void DebugSnoop(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                  const GLchar *message);
  static void APIENTRY DebugSnoopStatic(GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const GLchar *message, const void *userParam)
  {
    WrappedGLES *me = (WrappedGLES *)userParam;

    if(me->m_SuppressDebugMessages)
      return;

    me->DebugSnoop(source, type, id, severity, length, message);
  }

  // checks if the given object has tons of updates. If so it's probably
  // in the vein of "one global object, updated per-draw as necessary", or it's just
  // really high traffic, in which case we just want to save the state of it at frame
  // start, then track changes while frame capturing
  bool RecordUpdateCheck(GLResourceRecord *record);

  // internals
  Serialiser *m_pSerialiser;
  LogState m_State;
  bool m_AppControlledCapture;

  GLESReplay m_Replay;

  GLESInitParams m_InitParams;

  map<uint64_t, GLESWindowingData> m_ActiveContexts;

  vector<GLESWindowingData> m_LastContexts;

  bool m_ActiveQueries[8];
  bool m_ActiveConditional;
  bool m_ActiveFeedback;

  ResourceId m_DeviceResourceID;
  GLResourceRecord *m_DeviceRecord;

  ResourceId m_ContextResourceID;
  GLResourceRecord *m_ContextRecord;

  set<ResourceId> m_MissingTracks;

  GLResourceManager *m_ResourceManager;

  uint32_t m_FrameCounter;
  uint32_t m_FailedFrame;
  CaptureFailReason m_FailedReason;
  uint32_t m_Failures;

  CaptureFailReason m_FailureReason;
  bool m_SuccessfulCapture;

  PerformanceTimer m_FrameTimer;
  vector<double> m_FrameTimes;
  double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

  set<ResourceId> m_HighTrafficResources;

  // we store two separate sets of maps, since for an explicit glMemoryBarrier
  // we need to flush both types of maps, but for implicit sync points we only
  // want to consider coherent maps, and since that happens often we want it to
  // be as efficient as possible.
  set<GLResourceRecord *> m_CoherentMaps;
  set<GLResourceRecord *> m_PersistentMaps;

  // this function iterates over all the maps, checking for any changes between
  // the shadow pointers, and propogates that to 'real' GL
  void PersistentMapMemoryBarrier(const set<GLResourceRecord *> &maps);

  // this function is called at any point that could possibly pick up a change
  // in a coherent persistent mapped buffer, to propogate changes across. In most
  // cases hopefully m_CoherentMaps will be empty so this will amount to an inlined
  // check and jump
  inline void CoherentMapImplicitBarrier()
  {
    if(!m_CoherentMaps.empty())
      PersistentMapMemoryBarrier(m_CoherentMaps);
  }

  vector<FetchFrameInfo> m_CapturedFrames;
  FetchFrameRecord m_FrameRecord;
  vector<FetchDrawcall *> m_Drawcalls;

  // replay

  vector<FetchAPIEvent> m_CurEvents, m_Events;
  bool m_AddedDrawcall;

  uint64_t m_CurChunkOffset;
  uint32_t m_CurEventID, m_CurDrawcallID;
  uint32_t m_FirstEventID;
  uint32_t m_LastEventID;

  DrawcallTreeNode m_ParentDrawcall;

  list<DrawcallTreeNode *> m_DrawcallStack;

  map<ResourceId, vector<EventUsage> > m_ResourceUses;

  // buffer used
  vector<byte> m_ScratchBuf;

  struct BufferData
  {
    GLResource resource;
    GLenum curType;
    uint64_t size;
  };

  map<ResourceId, BufferData> m_Buffers;

  struct TextureData
  {
    TextureData()
        : curType(eGL_NONE),
          dimension(0),
          emulated(false),
          view(false),
          width(0),
          height(0),
          depth(0),
          samples(0),
          creationFlags(0),
          internalFormat(eGL_NONE),
          renderbufferReadTex(0)
    {
      renderbufferFBOs[0] = renderbufferFBOs[1] = 0;
    }
    GLResource resource;
    GLenum curType;
    GLint dimension;
    bool emulated, view;
    GLint width, height, depth, samples;
    uint32_t creationFlags;
    GLenum internalFormat;

    // since renderbuffers cannot be read from, we have to create a texture of identical
    // size/format,
    // and define FBOs for blitting to it - the renderbuffer is attached to the first FBO and the
    // texture is
    // bound to the second.
    GLuint renderbufferReadTex;
    GLuint renderbufferFBOs[2];

    // since compressed textures can not be read back we have to store them during the uploading.
    // target-> level -> data
    map<int, map<int, vector<byte> > > compressedData;
  };

  map<ResourceId, TextureData> m_Textures;

  struct ShaderData
  {
    ShaderData() : type(eGL_NONE), prog(0) {}
    GLenum type;
    vector<string> sources;
    vector<string> includepaths;
    ShaderReflection reflection;
    GLuint prog;

    void Compile(WrappedGLES &gl);
  };

  struct ProgramData
  {
    ProgramData() : linked(false) { RDCEraseEl(stageShaders); }
    vector<ResourceId> shaders;

    map<GLint, GLint> locationTranslate;

    bool linked;
    ResourceId stageShaders[6];
  };

  struct PipelineData
  {
    PipelineData()
    {
      RDCEraseEl(stagePrograms);
      RDCEraseEl(stageShaders);
    }

    struct ProgramUse
    {
      ProgramUse(ResourceId id_, GLbitfield use_) : id(id_), use(use_) {}
      ResourceId id;
      GLbitfield use;
    };

    ResourceId stagePrograms[6];
    ResourceId stageShaders[6];
  };

  map<ResourceId, ShaderData> m_Shaders;
  map<ResourceId, ProgramData> m_Programs;
  map<ResourceId, PipelineData> m_Pipelines;
  vector<pair<ResourceId, Replacement> > m_DependentReplacements;

  GLuint m_FakeBB_FBO;
  GLuint m_FakeBB_Color;
  GLuint m_FakeBB_DepthStencil;
  GLuint m_FakeVAO;
  GLuint m_FakeIdxBuf;
  GLsizeiptr m_FakeIdxSize;

  ResourceId m_FakeVAOID;

  Serialiser *GetSerialiser() { return m_pSerialiser; }
  uint32_t GetLogVersion() { return m_InitParams.SerialiseVersion; }
  void ProcessChunk(uint64_t offset, GLChunkType context);
  void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
  void ContextProcessChunk(uint64_t offset, GLChunkType chunk);
  void AddUsage(const FetchDrawcall &d);
  void AddDrawcall(const FetchDrawcall &d, bool hasEvents);
  void AddEvent(GLChunkType type, string description, ResourceId ctx = ResourceId());

  void Serialise_CaptureScope(uint64_t offset);
  bool HasSuccessfulCapture(CaptureFailReason &reason)
  {
    reason = m_FailureReason;
    return m_SuccessfulCapture && m_ContextRecord->NumChunks() > 0;
  }
  void AttemptCapture();
  bool Serialise_BeginCaptureFrame(bool applyInitialState);
  void BeginCaptureFrame();
  void FinishCapture();
  void ContextEndFrame();

  void CleanupCapture();
  void FreeCaptureData();

  struct ContextData
  {
    ContextData()
    {
      ctx = NULL;

      built = ready = false;
      attribsCreate = false;
      version = 0;
      isCore = false;
      Program = GeneralUBO = StringUBO = GlyphUBO = 0;
      GlyphTexture = DummyVAO = 0;
      CharSize = CharAspect = 0.0f;
      RDCEraseEl(m_TextureRecord);
      RDCEraseEl(m_BufferRecord);
      m_VertexArrayRecord = m_FeedbackRecord = m_DrawFramebufferRecord = NULL;
      m_ReadFramebufferRecord = NULL;
      m_Renderbuffer = ResourceId();
      m_TextureUnit = 0;
      m_ProgramPipeline = m_Program = 0;
    }

    void *ctx;

    bool built;
    bool ready;

    int version;
    bool attribsCreate;
    bool isCore;

    // map from window handle void* to uint64_t unix timestamp with
    // the last time a window was seen/associated with this context.
    // Decays after a few seconds since there's no good explicit
    // 'remove' type call for GL, only wglCreateContext/wglMakeCurrent
    map<void *, uint64_t> windows;

    // a window is only associated with one context at once, so any
    // time we associate a window, it broadcasts to all other
    // contexts to let them know to remove it
    void UnassociateWindow(void *surface);
    void AssociateWindow(WrappedGLES *gl, void *surface);

    void CreateDebugData(const GLHookSet &gl);

    bool Legacy() { return version < 20; }
    bool Modern() { return !Legacy(); }
    GLuint Program;
    GLuint GeneralUBO, StringUBO, GlyphUBO;
    GLuint GlyphTexture;
    GLuint DummyVAO;

    float CharSize;
    float CharAspect;

    // extensions
    vector<string> glExts;
    string glExtsString;

    // state
    static const int numberOfTextureTargetTypes = 8;
    GLResourceRecord *m_TextureRecord[32][numberOfTextureTargetTypes];
    GLResourceRecord *m_BufferRecord[16];
    GLResourceRecord *m_VertexArrayRecord;
    GLResourceRecord *m_FeedbackRecord;
    GLResourceRecord *m_DrawFramebufferRecord;
    GLResourceRecord *m_ReadFramebufferRecord;
    ResourceId m_Renderbuffer;
    GLint m_TextureUnit;
    GLuint m_ProgramPipeline;
    GLuint m_Program;

    GLResourceRecord *GetActiveTexRecord(GLenum target) { return m_TextureRecord[m_TextureUnit][TextureTargetIndex(target)]; }
    GLResourceRecord *GetActiveBufferRecord(GLenum target) { return m_BufferRecord[BufferIdx(target)]; }
  };

  map<void *, ContextData> m_ContextData;

  ContextData &GetCtxData();
  GLuint GetUniformProgram();

  void MakeValidContextCurrent(GLESWindowingData &prevctx, void *favourWnd);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);
  void FreeTargetResource(ResourceId id);

  struct QueuedInitialStateFetch
  {
    GLResource res;
    byte *blob;

    bool operator<(const QueuedInitialStateFetch &o) const { return res.Context < o.res.Context; }
  };

  vector<QueuedInitialStateFetch> m_QueuedInitialFetches;

  void QueuePrepareInitialState(GLResource res, byte *blob);

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  void RenderOverlayText(float x, float y, const char *fmt, ...);
  void RenderOverlayStr(float x, float y, const char *str);

  struct BackbufferImage
  {
    BackbufferImage() : jpgbuf(NULL), len(0), thwidth(0), thheight(0) {}
    ~BackbufferImage() { SAFE_DELETE_ARRAY(jpgbuf); }
    byte *jpgbuf;
    size_t len;
    uint32_t thwidth;
    uint32_t thheight;
  };

  BackbufferImage *SaveBackbufferImage();
  map<void *, BackbufferImage *> m_BackbufferImages;

  vector<string> globalExts;

  vector<byte *> m_localDataBuffers;
  void clearLocalDataBuffers();

  void writeFakeVertexAttribPointer(GLsizei count);

  template<typename TP, typename TF>
  bool Serialise_Common_glTexParameter_v(GLenum target, GLenum pname, const TP *params, TF GLHookSet::*function);

  template <typename TP, typename TF>
  void Common_glTexParameter_v(GLenum target, GLenum pname, const TP *params, TF GLHookSet::*function, const GLChunkType chunkType);

  // no copy semantics
  WrappedGLES(const WrappedGLES &);
  WrappedGLES &operator=(const WrappedGLES &);

public:
  WrappedGLES(const char *logfile, const GLHookSet &funcs);
  virtual ~WrappedGLES();

  void enableAPIDebug(bool enable);
  static const char *GetChunkName(uint32_t idx);
  GLResourceManager *GetResourceManager() { return m_ResourceManager; }
  ResourceId GetDeviceResourceID() { return m_DeviceResourceID; }
  ResourceId GetContextResourceID() { return m_ContextResourceID; }
  GLESReplay *GetReplay() { return &m_Replay; }
  void *GetCtx();

  const GLHookSet &GetHookset() { return m_Real; }
  void SetDebugMsgContext(const char *context) { m_DebugMsgContext = context; }
  void AddDebugMessage(DebugMessage msg)
  {
    if(m_State < WRITING)
      m_DebugMessages.push_back(msg);
  }
  void AddDebugMessage(DebugMessageCategory c, DebugMessageSeverity sv, DebugMessageSource src,
                       std::string d);

  void AddMissingTrack(ResourceId id) { m_MissingTracks.insert(id); }
  // replay interface
  void Initialise(GLESInitParams &params);
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
  void ReadLogInitialisation();

  GLuint GetFakeBBFBO() { return m_FakeBB_FBO; }
  GLuint GetFakeVAO() { return m_FakeVAO; }
  FetchFrameRecord &GetFrameRecord() { return m_FrameRecord; }
  FetchAPIEvent GetEvent(uint32_t eventID);

  const DrawcallTreeNode &GetRootDraw() { return m_ParentDrawcall; }
  const FetchDrawcall *GetDrawcall(uint32_t eventID);

  void SuppressDebugMessages(bool suppress) { m_SuppressDebugMessages = suppress; }
  vector<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }
  void CreateContext(GLESWindowingData winData, void *shareContext, GLESInitParams initParams,
                     bool core, bool attribsCreate);
  void RegisterContext(GLESWindowingData winData, void *shareContext, bool core, bool attribsCreate);
  void DeleteContext(void *contextHandle);
  void ActivateContext(GLESWindowingData winData);
  void WindowSize(void *surface, uint32_t w, uint32_t h);
  void SwapBuffers(void *surface);

  void StartFrameCapture(void *dev, void *surface);
  bool EndFrameCapture(void *dev, void *surface);

  #include "gles_driver_function_serialize_defs.inl"

  void dumpCurrentState(const char * filename);
  // TODO pantos
  bool Serialise_glFenceSync(GLsync real, GLenum condition, GLbitfield flags);
  bool Serialise_glCreateShader(GLuint real, GLenum type);
  bool Serialise_glCreateShaderProgramv(GLuint real, GLenum type, GLsizei count,
                                        const GLchar *const *strings);
  bool Serialise_glCreateProgram(GLuint real);

  bool Serialise_glFramebufferTexture(GLuint framebuffer, GLenum target, GLenum attachment,
                                      GLuint texture, GLint level);
  bool Serialise_glFramebufferTexture2D(GLuint framebuffer, GLenum target, GLenum attachment,
                                        GLenum textarget, GLuint texture,
                                        GLint level);
  bool Serialise_glFramebufferTexture3DOES(GLuint framebuffer, GLenum target, GLenum attachment,
                                           GLenum textarget, GLuint texture,
                                           GLint level, GLint zoffset);
  bool Serialise_glFramebufferTextureLayer(GLuint framebuffer, GLenum target,
                                           GLenum attachment, GLuint texture,
                                           GLint level, GLint layer);
  bool Serialise_glFramebufferParameteri(GLuint framebuffer, GLenum target,
                                         GLenum pname, GLint param);
  bool Serialise_glDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs);
  bool Serialise_glReadBuffer(GLuint framebuffer, GLenum mode);
  bool Serialise_glFramebufferRenderbuffer(GLuint framebuffer, GLenum target, GLenum attachment,
                                           GLenum renderbuffertarget,
                                           GLuint renderbuffer);
  bool Serialise_glRenderbufferStorage(GLuint renderbuffer, GLenum target,
                                       GLenum internalformat, GLsizei width,
                                       GLsizei height);
  bool Serialise_glRenderbufferStorageMultisample(GLuint renderbuffer, GLenum target,
                                                  GLsizei samples, GLenum internalformat,
                                                  GLsizei width, GLsizei height);
  bool Serialise_glBlitFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                   GLint srcX0, GLint srcY0, GLint srcX1,
                                   GLint srcY1, GLint dstX0, GLint dstY0,
                                   GLint dstX1, GLint dstY1, GLbitfield mask,
                                   GLenum filter);

  bool Serialise_glVertexAttrib(GLuint index, int count, GLenum type, GLboolean normalized,
                                const void *value, int attribtype);

  bool Serialise_glVertexAttribPointerEXT(GLuint vaobj, GLuint buffer,
                                          GLuint index, GLint size, GLenum type,
                                          GLboolean normalized, GLsizei stride, const void *pointer, size_t dataSize,
                                          bool isInteger);
  enum AttribType
  {
    Attrib_GLfloat = 0x02,
    Attrib_GLint = 0x07,
    Attrib_GLuint = 0x08,
    Attrib_typemask = 0x0f,

    Attrib_I = 0x20,
  };


  enum UniformType
  {
    UNIFORM_UNKNOWN,

    VEC1fv,
    VEC1iv,
    VEC1uiv,

    VEC2fv,
    VEC2iv,
    VEC2uiv,

    VEC3fv,
    VEC3iv,
    VEC3uiv,

    VEC4fv,
    VEC4iv,
    VEC4uiv,

    MAT2fv,
    MAT2x3fv,
    MAT2x4fv,
    MAT3fv,
    MAT3x2fv,
    MAT3x4fv,
    MAT4fv,
    MAT4x2fv,
    MAT4x3fv,
  };

  bool Serialise_glProgramUniformMatrix(GLuint program, GLint location, GLsizei count,
                                        GLboolean transpose, const void *value, UniformType type);
  bool Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count,
                                        const void *value, UniformType type);

  // utility handling functions for glDraw*Elements* to handle pointers to indices being
  // passed directly, with no index buffer bound. It's not allowed in core profile but
  // it's fairly common and not too hard to support
  byte *Common_preElements(GLsizei Count, GLenum Type, uint64_t &IdxOffset);
  void Common_postElements(byte *idxDelete);

  // final check function to ensure we don't try and render with no index buffer bound
  bool Check_preElements();


  // EXT_direct_state_access

  // there's a lot of duplicated code in some of these variants, between
  // EXT_dsa, ARB_dsa, non-dsa and for textures the MultiTex variants etc.
  // So we make a Common_ function similar to the Serialise_ function based
  // on the EXT_dsa interface, which takes the function parameters and a
  // GLResourceRecord* which does all the common tasks between all of these
  // functions.

  void Common_glGenerateTextureMipmapEXT(GLResourceRecord *record, GLenum target);

  void Common_glCopyTextureImage1DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                      GLenum internalformat, GLint x, GLint y, GLsizei width,
                                      GLint border);
  void Common_glCopyTextureImage2DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                      GLenum internalformat, GLint x, GLint y, GLsizei width,
                                      GLsizei height, GLint border);
  void Common_glCopyTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                         GLint xoffset, GLint x, GLint y, GLsizei width);
  void Common_glCopyTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLint x, GLint y,
                                         GLsizei width, GLsizei height);
  void Common_glCopyTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                         GLint y, GLsizei width, GLsizei height);

  void Common_glTextureBufferEXT(ResourceId id, GLenum target, GLenum internalformat, GLuint buffer);
  void Common_glTextureBufferRangeEXT(ResourceId id, GLenum target, GLenum internalformat,
                                      GLuint buffer, GLintptr offset, GLsizeiptr size);

  void Common_glTextureImage1DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat,
                                  GLsizei width, GLint border, GLenum format, GLenum type,
                                  const void *pixels);
  void Common_glTextureImage2DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat,
                                  GLsizei width, GLsizei height, GLint border, GLenum format,
                                  GLenum type, const void *pixels);
  void Common_glTextureImage3DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat,
                                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                  GLenum format, GLenum type, const void *pixels);
  void Common_glCompressedTextureImage1DEXT(ResourceId id, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLint border,
                                            GLsizei imageSize, const void *bits);
  void Common_glCompressedTextureImage2DEXT(ResourceId id, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLint border, GLsizei imageSize, const void *bits);
  void Common_glCompressedTextureImage3DEXT(ResourceId id, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLsizei depth, GLint border, GLsizei imageSize,
                                            const void *bits);

  void Common_glTextureStorage1DEXT(ResourceId id, GLenum target, GLsizei levels,
                                    GLenum internalformat, GLsizei width);
  void Common_glTextureStorage2DEXT(ResourceId id, GLenum target, GLsizei levels,
                                    GLenum internalformat, GLsizei width, GLsizei height);
  void Common_glTextureStorage3DEXT(ResourceId id, GLenum target, GLsizei levels,
                                    GLenum internalformat, GLsizei width, GLsizei height,
                                    GLsizei depth);
  void Common_glTextureStorage2DMultisampleEXT(ResourceId id, GLenum target, GLsizei samples,
                                               GLenum internalformat, GLsizei width, GLsizei height,
                                               GLboolean fixedsamplelocations);
  void Common_glTextureStorage3DMultisampleEXT(ResourceId id, GLenum target, GLsizei samples,
                                               GLenum internalformat, GLsizei width, GLsizei height,
                                               GLsizei depth, GLboolean fixedsamplelocations);

  void Common_glTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                     GLint xoffset, GLsizei width, GLenum format, GLenum type,
                                     const void *pixels);
  void Common_glTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     GLenum format, GLenum type, const void *pixels);
  void Common_glTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                     GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                     const void *pixels);
  void Common_glCompressedTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                               GLint xoffset, GLsizei width, GLenum format,
                                               GLsizei imageSize, const void *bits);
  void Common_glCompressedTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLsizei width,
                                               GLsizei height, GLenum format, GLsizei imageSize,
                                               const void *bits);
  void Common_glCompressedTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint zoffset,
                                               GLsizei width, GLsizei height, GLsizei depth,
                                               GLenum format, GLsizei imageSize, const void *bits);

  void Common_glTextureParameterfEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                     GLfloat param);
  void Common_glTextureParameterfvEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                      const GLfloat *params);
  void Common_glTextureParameteriEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                     GLint param);
  void Common_glTextureParameterivEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                      const GLint *params);
  void Common_glTextureParameterIivEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                       const GLint *params);
  void Common_glTextureParameterIuivEXT(GLResourceRecord *record, GLenum target, GLenum pname,
                                        const GLuint *params);

  void Common_glNamedBufferStorageEXT(ResourceId id, GLsizeiptr size, const void *data,
                                      GLbitfield flags);

  bool Serialise_Common_glBlendBarrier(bool isExtension);
  void Common_glBlendBarrier(bool isExtension);

  // GLES - GL compatibility like methods
  void glGetTexImage(GLenum target, GLenum texType, GLuint texname, GLint mip, GLenum fmt, GLenum type, GLint width, GLint height, void *ret);
  void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
  void glGetNamedBufferSubDataEXT(GLuint buffer, GLenum target, GLintptr offset, GLsizeiptr size, void *data);
  void Compat_glBufferStorageEXT (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
  void Compat_glTextureStorage2DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
  void Compat_glTextureStorage3DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
  void * Compat_glMapBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
  void Compat_glFlushMappedBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length);
  void Compat_glDrawArraysInstancedBaseInstanceEXT(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
  void Compat_glDrawElementsInstancedBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance);
  void Compat_glDrawElementsInstancedBaseVertexBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);

};

class ScopedDebugContext
{
public:
  ScopedDebugContext(WrappedGLES *gl, const char *fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    buf[1023] = 0;
    StringFormat::vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    m_GL = gl;
    m_GL->SetDebugMsgContext(buf);
  }

  ~ScopedDebugContext() { m_GL->SetDebugMsgContext(""); }
private:
  WrappedGLES *m_GL;
};
