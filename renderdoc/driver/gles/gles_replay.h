#pragma once

#include "core/core.h"
#include "replay/replay_driver.h"

class WrappedGLES;

class GLESReplay : public IReplayDriver
{
public:
    GLESReplay(WrappedGLES* gles)
        : m_pDriver(gles)
     {}

    virtual void Shutdown() { }
    virtual APIProperties GetAPIProperties();
    virtual vector<ResourceId> GetBuffers();
    virtual FetchBuffer GetBuffer(ResourceId id);
    virtual vector<ResourceId> GetTextures();
    virtual FetchTexture GetTexture(ResourceId id);
    virtual vector<DebugMessage> GetDebugMessages();
    virtual ShaderReflection *GetShader(ResourceId shader, string entryPoint);
    virtual vector<EventUsage> GetUsage(ResourceId id);
    virtual void SavePipelineState();

    virtual D3D11PipelineState GetD3D11PipelineState() { return D3D11PipelineState(); }
    virtual GLPipelineState GetGLPipelineState() { return GLPipelineState(); } /* TODO */
    virtual VulkanPipelineState GetVulkanPipelineState() { return VulkanPipelineState(); }

    virtual FetchFrameRecord GetFrameRecord();
    virtual void ReadLogInitialisation();
    virtual void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
    virtual void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
    virtual vector<uint32_t> GetPassEvents(uint32_t eventID);
    virtual ResourceId GetLiveID(ResourceId id);
    virtual void InitPostVSBuffers(uint32_t eventID);
    virtual void InitPostVSBuffers(const vector<uint32_t> &passEvents);
    virtual MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);
    virtual void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret);
    virtual byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                FormatComponentType typeHint, bool resolve, bool forceRGBA8unorm,
                                float blackPoint, float whitePoint, size_t &dataSize);
    virtual void BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                   ShaderStageType type, ResourceId *id, string *errors);
    virtual void ReplaceResource(ResourceId from, ResourceId to);
    virtual void RemoveReplacement(ResourceId id);
    virtual void FreeTargetResource(ResourceId id);
    virtual vector<uint32_t> EnumerateCounters();
    virtual void DescribeCounter(uint32_t counterID, CounterDescription &desc);
    virtual vector<CounterResult> FetchCounters(const vector<uint32_t> &counterID);

    virtual void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

    virtual vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                                 uint32_t y, uint32_t slice, uint32_t mip,
                                                 uint32_t sampleIdx, FormatComponentType typeHint);
    virtual ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                                         uint32_t instOffset, uint32_t vertOffset);
    virtual ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                        uint32_t primitive);
    virtual ShaderDebugTrace DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
    virtual ResourceId RenderOverlay(ResourceId id, FormatComponentType typeHint, TextureDisplayOverlay overlay,
                                     uint32_t eventID, const vector<uint32_t> &passEvents);

    virtual bool IsRenderOutput(ResourceId id);
    virtual void FileChanged();
    virtual void InitCallstackResolver();
    virtual bool HasCallstacks();
    virtual Callstack::StackResolver *GetCallstackResolver();
    virtual bool IsRemoteProxy();
    virtual vector<WindowingSystem> GetSupportedWindowSystems();
    virtual uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
    virtual void DestroyOutputWindow(uint64_t id);
    virtual bool CheckResizeOutputWindow(uint64_t id);
    virtual void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
    virtual void ClearOutputWindowColour(uint64_t id, float col[4]);
    virtual void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
    virtual void BindOutputWindow(uint64_t id, bool depth);
    virtual bool IsOutputWindowVisible(uint64_t id);
    virtual void FlipOutputWindow(uint64_t id);


   virtual bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                           FormatComponentType typeHint, float *minval, float *maxval);
   virtual bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                             FormatComponentType typeHint, float minval, float maxval, bool channels[4],
                             vector<uint32_t> &histogram);
   virtual ResourceId CreateProxyTexture(const FetchTexture &templateTex);
   virtual void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize);
   virtual ResourceId CreateProxyBuffer(const FetchBuffer &templateBuf);
   virtual void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);
   virtual void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);
   virtual bool RenderTexture(TextureDisplay cfg);
   virtual void BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                   ShaderStageType type, ResourceId *id, string *errors);
   virtual ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                                        uint32_t sampleIdx, FormatComponentType typeHint);
   virtual void FreeCustomShader(ResourceId id);
   virtual void RenderCheckerboard(Vec3f light, Vec3f dark);
   virtual void RenderHighlightBox(float w, float h, float scale);
   virtual void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                            uint32_t sample, FormatComponentType typeHint, float pixel[4]);
   virtual uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

private:
    WrappedGLES* m_pDriver;
};

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver);
