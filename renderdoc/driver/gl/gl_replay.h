/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

class AMDCounters;
class IntelGlCounters;
class WrappedOpenGL;
struct GLCounterContext;

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
  DriverInformation GetDriverInfo() { return m_DriverInfo; }
  APIProperties GetAPIProperties();

  ResourceDescription &GetResourceDesc(ResourceId id);
  const std::vector<ResourceDescription> &GetResources();

  std::vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  std::vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry);

  std::vector<std::string> GetDisassemblyTargets();
  std::string DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                const std::string &target);

  std::vector<DebugMessage> GetDebugMessages();

  std::vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();

  void SavePipelineState(uint32_t eventId);
  const D3D11Pipe::State *GetD3D11PipelineState() { return NULL; }
  const D3D12Pipe::State *GetD3D12PipelineState() { return NULL; }
  const GLPipe::State *GetGLPipelineState() { return &m_CurPipelineState; }
  const VKPipe::State *GetVulkanPipelineState() { return NULL; }
  void FreeTargetResource(ResourceId id);

  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  const SDFile &GetStructuredFile();

  std::vector<uint32_t> GetPassEvents(uint32_t eventId);

  std::vector<WindowingSystem> GetSupportedWindowSystems();

  AMDRGPControl *GetRGPControl() { return NULL; }
  uint64_t MakeOutputWindow(WindowingData window, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h);
  void GetOutputWindowData(uint64_t id, bytebuf &retData);
  void ClearOutputWindowColor(uint64_t id, FloatVector col);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventId);
  void InitPostVSBuffers(const std::vector<uint32_t> &passEvents);

  ResourceId GetLiveID(ResourceId id);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    std::vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                              MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret);
  void GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                      const GetTextureDataParams &params, bytebuf &data);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  std::vector<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  std::vector<CounterResult> FetchCounters(const std::vector<GPUCounter> &counters);

  void RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings() { return {ShaderEncoding::GLSL}; }
  rdcarray<ShaderEncoding> GetTargetShaderEncodings() { return {ShaderEncoding::GLSL}; }
  void BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  void BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  void FreeCustomShader(ResourceId id);

  enum TexDisplayFlags
  {
    eTexDisplay_BlendAlpha = 0x1,
    eTexDisplay_MipShift = 0x2,
  };

  bool RenderTexture(TextureDisplay cfg);
  bool RenderTextureInternal(TextureDisplay cfg, int flags);

  void RenderCheckerboard();

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
                            rdcarray<ShaderVariable> &outvars, const bytebuf &data);

  std::vector<PixelModification> PixelHistory(std::vector<EventUsage> events, ResourceId target,
                                              uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                                              uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventId, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height, const MeshDisplay &cfg,
                      uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId id, CompType typeHint, DebugOverlay overlay, uint32_t eventId,
                           const std::vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);
  bool NeedRemapForFetch(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  bool IsRenderOutput(ResourceId id);

  void FileChanged() {}
  void SetReplayData(GLWindowingData data);

  bool IsReplayContext(void *ctx) { return m_ReplayCtx.ctx == NULL || ctx == m_ReplayCtx.ctx; }
private:
  void OpenGLFillCBufferVariables(GLuint prog, bool bufferBacked, std::string prefix,
                                  const rdcarray<ShaderConstant> &variables,
                                  rdcarray<ShaderVariable> &outvars, const bytebuf &data);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, bool stencil, float *minval, float *maxval);

  void CreateCustomShaderTex(uint32_t w, uint32_t h);
  bool CreateOverlayProgram(GLuint Program, GLuint Pipeline, GLuint fragShader,
                            GLuint fragShaderSPIRV);

  void CopyArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                          GLint arraySize, GLint samples, GLenum intFormat, uint32_t selectedSlice);
  void CopyTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                          GLint arraySize, GLint samples, GLenum intFormat);
  void CopyDepthArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                               GLint arraySize, GLint samples, GLenum intFormat,
                               uint32_t selectedSlice);
  void CopyDepthTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                               GLint arraySize, GLint samples, GLenum intFormat);

  struct OutputWindow : public GLWindowingData
  {
    OutputWindow(const GLWindowingData &data) : GLWindowingData(data) {}
    OutputWindow() {}
    struct
    {
      // used to blit from defined FBO (VAOs not shared)
      GLuint emptyVAO = 0;

      // textures for the below FBO. Resize with the window
      GLuint backbuffer = 0;
      GLuint depthstencil = 0;

      // this FBO is on the debug GL context, not the window's GL context
      // when rendering a texture or mesh etc, we render onto this FBO on
      // the debug GL context, then blit onto the default framebuffer
      // on the window's GL context.
      // This is so we don't have to re-create any non-shared resource we
      // need for debug rendering on the window's GL context.
      GLuint windowFBO = 0;

      // this FBO is the same as the above, but on the replay context,
      // for any cases where we need to use the replay context (like
      // re-rendering a draw).
      GLuint replayFBO = 0;

      // read FBO for blit to window
      GLuint readFBO = 0;
    } BlitData;

    WindowingSystem system = WindowingSystem::Headless;
    int width = 1, height = 1;
  };

  enum class TextureSamplerMode
  {
    Point,
    PointNoMip,
    Linear,
  };

  struct TextureSamplerState
  {
    GLenum minFilter = eGL_NEAREST;
    GLenum magFilter = eGL_NEAREST;
    GLenum wrapS = eGL_CLAMP_TO_EDGE;
    GLenum wrapT = eGL_CLAMP_TO_EDGE;
    GLenum compareMode = eGL_NONE;
  };

  // sets the desired parameters, and returns the previous ones ready to restore
  TextureSamplerState SetSamplerParams(GLenum target, GLuint texname, TextureSamplerMode mode);
  void RestoreSamplerParams(GLenum target, GLuint texname, TextureSamplerState state);

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
    GLuint minmaxTileProgram[64];     // RESTYPE indexed (see glsl_ubos.h, 1d/2d/3d etc |
                                      // uint/sint) src tex -> tile result buf program
    GLuint histogramProgram[64];      // RESTYPE indexed (see glsl_ubos.h, 1d/2d/3d etc |
                                      // uint/sint) src tex -> histogram result buf program

    GLuint texDisplayVertexShader;
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
    GLuint DepthMS2Array, DepthArray2MS;

    GLuint checkerProg;

    GLuint fixedcolFragShader;
    GLuint fixedcolFragShaderSPIRV;

    // 0 = both floats, 1 = position doubles, 2 = secondary doubles, 3 = both doubles
    GLuint meshProg[4];
    GLuint meshgsProg[4];
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
    uint64_t feedbackBufferSize = 32 * 1024 * 1024;

    GLuint pickPixelTex;
    GLuint pickPixelFBO;

    GLuint dummyTexBuffer;
    GLuint dummyTexBufferStore;

    GLuint quadoverdrawFragShader;
    GLuint quadoverdrawFragShaderSPIRV;
    GLuint quadoverdrawResolveProg;

    ResourceId overlayTexId;
    GLuint overlayTex;
    GLuint overlayFBO;
    GLuint overlayProg;
    GLint overlayTexWidth, overlayTexHeight, overlayTexSamples;

    GLuint UBOs[3];

    GLuint emptyVAO;
  } DebugData;

  bool m_Degraded;

  HighlightCache m_HighlightCache;

  // eventId -> data
  std::map<uint32_t, GLPostVSData> m_PostVSData;

  void ClearPostVSCache();

  // cache the previous data returned
  ResourceId m_GetTexturePrevID;
  byte *m_GetTexturePrevData[16];

  GLuint CreateMeshProgram(GLuint vs, GLuint fs, GLuint gs = 0);
  void ConfigureTexDisplayProgramBindings(GLuint program);
  void BindUBO(GLuint program, const char *name, GLuint binding);

  void InitDebugData();
  void DeleteDebugData();

  void CheckGLSLVersion(const char *sl, int &glslVersion);

  void FillTimers(GLCounterContext &ctx, const DrawcallDescription &drawnode,
                  const std::vector<GPUCounter> &counters);

  GLuint CreateShader(GLenum shaderType, const std::string &src);
  GLuint CreateSPIRVShader(GLenum shaderType, const std::string &src);
  GLuint CreateShaderProgram(const std::string &vs, const std::string &fs,
                             const std::string &gs = "");
  GLuint CreateShaderProgram(GLuint vs, GLuint fs, GLuint gs = 0);
  GLuint CreateCShaderProgram(const std::string &cs);

  void InitOutputWindow(OutputWindow &outwin);
  void CreateOutputWindowBackbuffer(OutputWindow &outwin, bool depth);

  GLWindowingData m_ReplayCtx;
  uint64_t m_DebugID;
  OutputWindow *m_DebugCtx;

  void MakeCurrentReplayContext(GLWindowingData *ctx);
  void SwapBuffers(GLWindowingData *ctx);
  void CloseReplayContext();

  uint64_t m_OutputWindowID;
  std::map<uint64_t, OutputWindow> m_OutputWindows;

  bool m_Proxy;

  void CacheTexture(ResourceId id);

  std::map<ResourceId, TextureDescription> m_CachedTextures;

  WrappedOpenGL *m_pDriver;

  std::vector<ResourceDescription> m_Resources;
  std::map<ResourceId, size_t> m_ResourceIdx;

  GLPipe::State m_CurPipelineState;

  DriverInformation m_DriverInfo;

  // AMD counter instance
  AMDCounters *m_pAMDCounters = NULL;

  void FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex, std::vector<uint32_t> *eventIDs,
                     const DrawcallDescription &drawnode);

  std::vector<CounterResult> FetchCountersAMD(const std::vector<GPUCounter> &counters);

  // Intel counter instance
  IntelGlCounters *m_pIntelCounters = NULL;

  void FillTimersIntel(uint32_t *eventStartID, uint32_t *sampleIndex,
                       std::vector<uint32_t> *eventIDs, const DrawcallDescription &drawnode);

  std::vector<CounterResult> FetchCountersIntel(const std::vector<GPUCounter> &counters);
};
