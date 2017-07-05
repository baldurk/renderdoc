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
struct GLCounterContext;
struct DrawcallTreeNode;

struct GLPostVSData
{
  struct InstData
  {
    uint32_t numVerts = 0;
    uint32_t bufOffset = 0;
  };

  struct StageData
  {
    GLuint buf = 0;
    Topology topo = Topology::Unknown;

    uint32_t vertStride = 0;

    // simple case - uniform
    uint32_t numVerts = 0;
    uint32_t instStride = 0;

    // complex case - expansion per instance
    std::vector<InstData> instData;

    bool useIndices = false;
    GLuint idxBuf = 0;
    uint32_t idxByteWidth = 0;

    bool hasPosOut = false;

    float nearPlane = 0.0f;
    float farPlane = 0.0f;
  } vsin, vsout, gsout;

  const StageData &GetStage(MeshDataStage type)
  {
    if(type == MeshDataStage::VSOut)
      return vsout;
    else if(type == MeshDataStage::GSOut)
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
  BufferDescription GetBuffer(ResourceId id);

  vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);
  ShaderReflection *GetShader(ResourceId shader, string entryPoint);

  vector<string> GetDisassemblyTargets();
  string DisassembleShader(const ShaderReflection *refl, const string &target);

  vector<DebugMessage> GetDebugMessages();

  vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();

  void SavePipelineState();
  D3D11Pipe::State GetD3D11PipelineState() { return D3D11Pipe::State(); }
  D3D12Pipe::State GetD3D12PipelineState() { return D3D12Pipe::State(); }
  GLPipe::State GetGLPipelineState() { return m_CurPipelineState; }
  VKPipe::State GetVulkanPipelineState() { return VKPipe::State(); }
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
    ret.push_back(WindowingSystem::Xlib);
    return ret;
  }

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventID);
  void InitPostVSBuffers(const vector<uint32_t> &passEvents);

  ResourceId GetLiveID(ResourceId id);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret);
  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  vector<GPUCounter> EnumerateCounters();
  void DescribeCounter(GPUCounter counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<GPUCounter> &counters);

  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
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
                                         uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId id, CompType typeHint, DebugOverlay overlay, uint32_t eventID,
                           const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
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
    OutputWindow(const GLWindowingData &data) : GLWindowingData(data) {}
    OutputWindow() {}
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
    std::vector<GLuint> feedbackQueries;
    GLuint feedbackBuffer;
    uint32_t feedbackBufferSize = 32 * 1024 * 1024;

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

  HighlightCache m_HighlightCache;

  // eventID -> data
  map<uint32_t, GLPostVSData> m_PostVSData;

  // cache the previous data returned
  ResourceId m_GetTexturePrevID;
  byte *m_GetTexturePrevData[16];

  void InitDebugData();
  void DeleteDebugData();

  void CheckGLSLVersion(const char *sl, int &glslVersion);

  // called after the context is created, to init any counters
  void PostContextInitCounters();
  // called before the context is destroyed, to shutdown any counters
  void PreContextShutdownCounters();

  void FillTimers(GLCounterContext &ctx, const DrawcallTreeNode &drawnode,
                  const vector<GPUCounter> &counters);

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

  map<ResourceId, TextureDescription> m_CachedTextures;

  WrappedOpenGL *m_pDriver;

  GLPipe::State m_CurPipelineState;
};

const GLHookSet &GetRealGLFunctions();
GLPlatform &GetGLPlatform();
