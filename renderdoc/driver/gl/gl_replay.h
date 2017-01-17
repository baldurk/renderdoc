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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "gl_common.h"

using std::pair;
using std::map;

class WrappedOpenGL;
struct CounterContext;
struct DrawcallTreeNode;

struct GLPostVSData
{
  struct StageData
  {
    GLuint buf;
    PrimitiveTopology topo;

    uint32_t numVerts;
    uint32_t vertStride;
    uint32_t instStride;

    bool useIndices;
    GLuint idxBuf;
    uint32_t idxByteWidth;

    bool hasPosOut;

    float nearPlane;
    float farPlane;
  } vsin, vsout, gsout;

  GLPostVSData()
  {
    RDCEraseEl(vsin);
    RDCEraseEl(vsout);
    RDCEraseEl(gsout);
  }

  const StageData &GetStage(MeshDataStage type)
  {
    if(type == eMeshDataStage_VSOut)
      return vsout;
    else if(type == eMeshDataStage_GSOut)
      return gsout;
    else
      RDCERR("Unexpected mesh data stage!");

    return vsin;
  }
};

class GLReplay : public IReplayDriver
{
public:
  GLReplay();

  void SetProxy(bool p) { m_Proxy = p; }
  bool IsRemoteProxy() { return m_Proxy; }
  void Shutdown();

  void SetDriver(WrappedOpenGL *d) { m_pDriver = d; }
  APIProperties GetAPIProperties();

  vector<ResourceId> GetBuffers();
  FetchBuffer GetBuffer(ResourceId id);

  vector<ResourceId> GetTextures();
  FetchTexture GetTexture(ResourceId id);
  ShaderReflection *GetShader(ResourceId shader, string entryPoint);

  vector<DebugMessage> GetDebugMessages();

  vector<EventUsage> GetUsage(ResourceId id);

  FetchFrameRecord GetFrameRecord();

  void SavePipelineState();
  D3D11PipelineState GetD3D11PipelineState() { return D3D11PipelineState(); }
  D3D12PipelineState GetD3D12PipelineState() { return D3D12PipelineState(); }
  GLPipelineState GetGLPipelineState() { return m_CurPipelineState; }
  VulkanPipelineState GetVulkanPipelineState() { return VulkanPipelineState(); }
  void FreeTargetResource(ResourceId id);

  void ReadLogInitialisation();
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  vector<uint32_t> GetPassEvents(uint32_t eventID);

  vector<WindowingSystem> GetSupportedWindowSystems()
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

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColour(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventID);
  void InitPostVSBuffers(const vector<uint32_t> &passEvents);

  ResourceId GetLiveID(ResourceId id);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 FormatComponentType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    FormatComponentType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret);
  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  vector<uint32_t> EnumerateCounters();
  void DescribeCounter(uint32_t counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<uint32_t> &counters);

  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  void BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                         ShaderStageType type, ResourceId *id, string *errors);
  void BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                         ShaderStageType type, ResourceId *id, string *errors);
  void FreeCustomShader(ResourceId id);

  enum TexDisplayFlags
  {
    eTexDisplay_BlendAlpha = 0x1,
    eTexDisplay_MipShift = 0x2,
  };

  bool RenderTexture(TextureDisplay cfg);
  bool RenderTextureInternal(TextureDisplay cfg, int flags);

  void RenderCheckerboard(Vec3f light, Vec3f dark);

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                         uint32_t y, uint32_t slice, uint32_t mip,
                                         uint32_t sampleIdx, FormatComponentType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, FormatComponentType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId id, FormatComponentType typeHint, TextureDisplayOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, FormatComponentType typeHint);

  ResourceId CreateProxyTexture(const FetchTexture &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const FetchBuffer &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  bool IsRenderOutput(ResourceId id);

  void FileChanged() {}
  void InitCallstackResolver();
  bool HasCallstacks();
  Callstack::StackResolver *GetCallstackResolver();

  // called before any context is created, to init any counters
  static void PreContextInitCounters();
  // called after any context is destroyed, to do corresponding shutdown of counters
  static void PostContextShutdownCounters();

  void SetReplayData(GLWindowingData data);

  bool IsReplayContext(void *ctx) { return m_ReplayCtx.ctx == NULL || ctx == m_ReplayCtx.ctx; }
private:
  void FillCBufferValue(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, bool rowMajor,
                        uint32_t offs, uint32_t matStride, const vector<byte> &data,
                        ShaderVariable &outVar);
  void FillCBufferVariables(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, string prefix,
                            const rdctype::array<ShaderConstant> &variables,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  void CreateCustomShaderTex(uint32_t w, uint32_t h);
  void SetupOverlayPipeline(GLuint Program, GLuint Pipeline, GLuint fragProgram);

  void CopyArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                          GLint arraySize, GLint samples, GLenum intFormat);
  void CopyTex2DMSToArray(GLuint destArray, GLuint srcMS, GLint width, GLint height,
                          GLint arraySize, GLint samples, GLenum intFormat);

  struct OutputWindow : public GLWindowingData
  {
    struct
    {
      // used to blit from defined FBO (VAOs not shared)
      GLuint emptyVAO;

      // textures for the below FBO. Resize with the window
      GLuint backbuffer;
      GLuint depthstencil;

      // this FBO is on the debug GL context, not the window's GL context
      // when rendering a texture or mesh etc, we render onto this FBO on
      // the debug GL context, then blit onto the default framebuffer
      // on the window's GL context.
      // This is so we don't have to re-create any non-shared resource we
      // need for debug rendering on the window's GL context.
      GLuint windowFBO;

      // this FBO is the same as the above, but on the replay context,
      // for any cases where we need to use the replay context (like
      // re-rendering a draw).
      GLuint replayFBO;

      // read FBO for blit to window
      GLuint readFBO;
    } BlitData;

    int width, height;
  };

  // any objects that are shared between contexts, we just initialise
  // once
  struct DebugRenderData
  {
    float outWidth, outHeight;

    int glslVersion;

    // min/max data
    GLuint minmaxTileResult;          // tile result buffer
    GLuint minmaxResult;              // Vec4f[2] final result buffer
    GLuint histogramBuf;              // uint32_t * num buckets buffer
    GLuint minmaxResultProgram[3];    // float/uint/sint tile result -> final result program
    GLuint minmaxTileProgram[64];     // RESTYPE indexed (see debuguniforms.h, 1d/2d/3d etc |
                                      // uint/sint) src tex -> tile result buf program
    GLuint histogramProgram[64];      // RESTYPE indexed (see debuguniforms.h, 1d/2d/3d etc |
                                      // uint/sint) src tex -> histogram result buf program

    GLuint outlineQuadProg;

    GLuint texDisplayPipe;
    GLuint texDisplayVSProg;
    GLuint texDisplayProg[3];    // float/uint/sint

    GLuint customFBO;
    GLuint customTex;
    ResourceId CustomShaderTexID;

    static const int maxMeshPicks = 500;

    GLuint meshPickProgram;
    GLuint pickIBBuf, pickVBBuf;
    uint32_t pickIBSize, pickVBSize;
    GLuint pickResultBuf;

    GLuint MS2Array, Array2MS;

    GLuint pointSampler;
    GLuint pointNoMipSampler;
    GLuint linearSampler;

    GLuint genericUBO;

    GLuint checkerProg;

    GLuint fixedcolFSProg;

    GLuint meshProg;
    GLuint meshgsProg;
    GLuint trisizeProg;

    GLuint meshVAO;
    GLuint axisVAO;
    GLuint frustumVAO;
    GLuint triHighlightVAO;

    GLuint axisFrustumBuffer;
    GLuint triHighlightBuffer;

    GLuint feedbackObj;
    GLuint feedbackQuery;
    GLuint feedbackBuffer;

    GLuint pickPixelTex;
    GLuint pickPixelFBO;

    GLuint quadoverdrawFSProg;
    GLuint quadoverdrawResolveProg;

    GLuint overlayTex;
    GLuint overlayFBO;
    GLuint overlayPipe;
    GLint overlayTexWidth, overlayTexHeight, overlayTexSamples;

    GLuint UBOs[3];

    GLuint emptyVAO;
  } DebugData;

  bool m_Degraded;

  FloatVector InterpretVertex(byte *data, uint32_t vert, const MeshDisplay &cfg, byte *end,
                              bool useidx, bool &valid);

  // simple cache for when we need buffer data for highlighting
  // vertices, typical use will be lots of vertices in the same
  // mesh, not jumping back and forth much between meshes.
  struct HighlightCache
  {
    HighlightCache() : EID(0), buf(), offs(0), stage(eMeshDataStage_Unknown), useidx(false) {}
    uint32_t EID;
    ResourceId buf;
    uint64_t offs;
    MeshDataStage stage;
    bool useidx;

    vector<byte> data;
    vector<uint32_t> indices;
  } m_HighlightCache;

  // eventID -> data
  map<uint32_t, GLPostVSData> m_PostVSData;

  void InitDebugData();
  void DeleteDebugData();

  void CheckGLSLVersion(const char *sl, int &glslVersion);

  // called after the context is created, to init any counters
  void PostContextInitCounters();
  // called before the context is destroyed, to shutdown any counters
  void PreContextShutdownCounters();

  void FillTimers(CounterContext &ctx, const DrawcallTreeNode &drawnode,
                  const vector<uint32_t> &counters);

  GLuint CreateShaderProgram(const vector<string> &vs, const vector<string> &fs,
                             const vector<string> &gs);
  GLuint CreateShaderProgram(const vector<string> &vs, const vector<string> &fs);
  GLuint CreateCShaderProgram(const vector<string> &cs);

  void InitOutputWindow(OutputWindow &outwin);
  void CreateOutputWindowBackbuffer(OutputWindow &outwin, bool depth);

  GLWindowingData m_ReplayCtx;
  uint64_t m_DebugID;
  OutputWindow *m_DebugCtx;

  void MakeCurrentReplayContext(GLWindowingData *ctx);
  void SwapBuffers(GLWindowingData *ctx);
  void CloseReplayContext();

  uint64_t m_OutputWindowID;
  map<uint64_t, OutputWindow> m_OutputWindows;

  bool m_Proxy;

  void CacheTexture(ResourceId id);

  map<ResourceId, FetchTexture> m_CachedTextures;

  WrappedOpenGL *m_pDriver;

  GLPipelineState m_CurPipelineState;
};
