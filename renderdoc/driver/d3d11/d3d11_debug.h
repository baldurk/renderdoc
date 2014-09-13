/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include <d3d11.h>

using std::pair;

#include "api/replay/renderdoc_replay.h"

#include "driver/d3d11/shaders/dxbc_debug.h"

class Camera;
class Vec3f;

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;

class D3D11ResourceManager;

namespace ShaderDebug { struct GlobalState; }

struct GPUTimer
{
	ID3D11Query *before;
	ID3D11Query *after;
	FetchDrawcall *drawcall;
};

struct PostVSData
{
	struct StageData
	{
		ID3D11Buffer *buf;
		D3D11_PRIMITIVE_TOPOLOGY topo;

		uint32_t numVerts;
		uint32_t numPrims;
		uint32_t posOffset;
		uint32_t vertStride;

		float nearPlane;
		float farPlane;
	} vsin, vsout, gsout;

	PostVSData()
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

class D3D11DebugManager
{
	public:
		D3D11DebugManager(WrappedID3D11Device *wrapper);
		~D3D11DebugManager();

		uint64_t MakeOutputWindow(void *w, bool depth);
		void DestroyOutputWindow(uint64_t id);
		bool CheckResizeOutputWindow(uint64_t id);
		void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
		void ClearOutputWindowColour(uint64_t id, float col[4]);
		void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
		void BindOutputWindow(uint64_t id, bool depth);
		bool IsOutputWindowVisible(uint64_t id);
		void FlipOutputWindow(uint64_t id);

		void SetOutputDimensions(int w, int h) { m_width = w; m_height = h; }
		int GetWidth() { return m_width; }
		int GetHeight() { return m_height; }
		
		void InitPostVSBuffers(uint32_t frameID, uint32_t eventID);
		PostVSData GetPostVSBuffers(uint32_t frameID, uint32_t eventID);
		PostVSMeshData GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage);

		uint32_t GetStructCount(ID3D11UnorderedAccessView *uav);
		vector<byte> GetBufferData(ID3D11Buffer *buff, uint32_t offset, uint32_t len);
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len);

		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, size_t &dataSize);
		
		void FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars, vector<ShaderVariable> &outvars,
								  bool flattenVec4s, const vector<byte> &data);

		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval);
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram);

		void CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray);
		void CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS);

		void TimeDrawcalls(rdctype::array<FetchDrawcall> &arr);

		bool SaveTexture(ResourceId tex, uint32_t saveMip, wstring path);

		void RenderText(float x, float y, float size, const char *textfmt, ...);
		void RenderMesh(uint32_t frameID, const vector<uint32_t> &events, MeshDisplay cfg);

		ID3D11Buffer *MakeCBuffer(float *data, size_t size);
		
		string GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags, const char *profile, ID3DBlob **srcblob);
		ID3D11VertexShader *MakeVShader(const char *source, const char *entry, const char *profile,
										int numInputDescs = 0,
										D3D11_INPUT_ELEMENT_DESC *inputs = NULL, ID3D11InputLayout **ret = NULL, vector<byte> *blob = NULL);
		ID3D11GeometryShader *MakeGShader(const char *source, const char *entry, const char *profile);
		ID3D11PixelShader *MakePShader(const char *source, const char *entry, const char *profile);
		ID3D11ComputeShader *MakeCShader(const char *source, const char *entry, const char *profile);

		void BuildShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);

		ID3D11Buffer *MakeCBuffer(UINT size);

		bool RenderTexture(TextureDisplay cfg);

		void RenderCheckerboard(Vec3f light, Vec3f dark);

		void RenderHighlightBox(float w, float h, float scale);
		
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y);
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y);
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4]);
			
		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents);
		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip);
			
		// don't need to differentiate arrays as we treat everything
		// as an array (potentially with only one element).
		enum TextureType
		{
			eTexType_1D = 1,
			eTexType_2D,
			eTexType_3D,
			eTexType_Depth,
			eTexType_Stencil,
			eTexType_DepthMS,
			eTexType_StencilMS,
			eTexType_Cube,
			eTexType_2DMS,
			eTexType_Max
		};

		struct TextureShaderDetails
		{
			TextureShaderDetails()
			{
				texFmt = DXGI_FORMAT_UNKNOWN;
				texWidth = 0;
				texHeight = 0;
				texDepth = 0;
				texMips = 0;
				texArraySize = 0;

				sampleCount = 1;
				sampleQuality = 0;

				texType = eTexType_2D;

				srvResource = NULL;
				previewCopy = NULL;

				RDCEraseEl(srv);
			}

			DXGI_FORMAT texFmt;
			UINT texWidth;
			UINT texHeight;
			UINT texDepth;
			UINT texMips;
			UINT texArraySize;

			UINT sampleCount;
			UINT sampleQuality;

			TextureType texType;

			ID3D11Resource *srvResource;
			ID3D11Resource *previewCopy;

			ID3D11ShaderResourceView *srv[eTexType_Max];
		};

		TextureShaderDetails GetShaderDetails(ResourceId id, bool rawOutput);
	private:
		struct CacheElem
		{
			CacheElem(ResourceId id_, bool raw_)
				: created(false), id(id_), raw(raw_), srvResource(NULL)
			{
				srv[0] = srv[1] = srv[2] = NULL;
			}

			void Release()
			{
				SAFE_RELEASE(srvResource);
				SAFE_RELEASE(srv[0]);
				SAFE_RELEASE(srv[1]);
				SAFE_RELEASE(srv[2]);
			}

			bool created;
			ResourceId id;
			bool raw;
			ID3D11Resource *srvResource;
			ID3D11ShaderResourceView *srv[3];
		};

		static const int NUM_CACHED_SRVS = 64;

		std::list<CacheElem> m_ShaderItemCache;

		CacheElem &GetCachedElem(ResourceId id, bool raw);

		int m_width, m_height;

		WrappedID3D11Device *m_WrappedDevice;
		WrappedID3D11DeviceContext *m_WrappedContext;

		D3D11ResourceManager *m_ResourceManager;

		ID3D11Device *m_pDevice;
		ID3D11DeviceContext *m_pImmediateContext;

		IDXGIFactory *m_pFactory;
		
		struct OutputWindow
		{
			HWND wnd;
			IDXGISwapChain* swap;
			ID3D11RenderTargetView* rtv;
			ID3D11DepthStencilView* dsv;

			WrappedID3D11Device *dev;

			void MakeRTV();
			void MakeDSV();

			int width, height;
		};

		uint64_t m_OutputWindowID;
		map<uint64_t, OutputWindow> m_OutputWindows;

		static const uint32_t m_ShaderCacheVersion = 2;
		bool m_ShaderCacheDirty, m_CacheShaders;
		map<uint32_t, ID3DBlob*> m_ShaderCache;

		static const int m_SOBufferSize = 16*1024*1024;
		ID3D11Buffer *m_SOBuffer;
		ID3D11Buffer *m_SOStagingBuffer;
		ID3D11Query *m_SOStatsQuery;
		// <frame,event> -> data
		map<pair<uint32_t,uint32_t>, PostVSData> m_PostVSData;

		ID3D11Texture2D *m_OverlayRenderTex;
		ResourceId m_OverlayResourceId;

		ID3D11Texture2D* m_CustomShaderTex;
		ID3D11RenderTargetView* m_CustomShaderRTV;
		ResourceId m_CustomShaderResourceId;

		ID3D11BlendState *m_WireframeHelpersBS;
		ID3D11RasterizerState *m_WireframeHelpersRS, *m_WireframeHelpersCullCCWRS, *m_WireframeHelpersCullCWRS;
		ID3D11RasterizerState *m_SolidHelpersRS;
		
		// this gets updated to pull the elements we want out of the buffers
		ID3D11InputLayout *m_MeshDisplayLayout;
		int m_MeshDisplayNULLVB;

		// whenever this changes (just a dumb pointer, not ref-owned)
		ID3D11InputLayout *m_PrevMeshInputLayout;
		
		ID3D11Buffer *m_AxisHelper;
		ID3D11Buffer *m_FrustumHelper;
		ID3D11Buffer *m_TriHighlightHelper;
		
		bool InitStreamOut();
		void ShutdownStreamOut();

		// font/text rendering
		bool InitFontRendering();
		void ShutdownFontRendering();

		void RenderTextInternal(float x, float y, float size, const char *text);

		void CreateCustomShaderTex(uint32_t w, uint32_t h);

		void PixelHistoryDepthCopySubresource(bool depthbound, ID3D11Texture2D *uavres, ID3D11UnorderedAccessView *uav, ID3D11Resource *depthres,
																					ID3D11ShaderResourceView **copyDepthSRV, ID3D11ShaderResourceView **copyStencilSRV,
																					ID3D11Buffer *srcxyCBuf, ID3D11Buffer *storexyCBuf, uint32_t x, uint32_t y);

		static const int FONT_TEX_WIDTH = 4096;
		static const int FONT_TEX_HEIGHT = 48;
		static const int FONT_MAX_CHARS = 256;

		static const uint32_t STAGE_BUFFER_BYTE_SIZE = 4*1024*1024;

		struct FontData
		{
			FontData() { RDCEraseMem(this, sizeof(FontData)); }
			~FontData()
			{
				SAFE_RELEASE(Layout);
				SAFE_RELEASE(Tex);
				SAFE_RELEASE(CBuffer);
				SAFE_RELEASE(CharBuffer);
				SAFE_RELEASE(VS);
				SAFE_RELEASE(PS);
			}
			
			ID3D11InputLayout *Layout;
			ID3D11ShaderResourceView *Tex;
			ID3D11Buffer *CBuffer;
			ID3D11Buffer *CharBuffer;
			ID3D11VertexShader *VS;
			ID3D11PixelShader *PS;
		} m_Font;

		struct DebugRenderData
		{
			DebugRenderData() { RDCEraseMem(this, sizeof(DebugRenderData)); }
			~DebugRenderData()
			{
				SAFE_RELEASE(StageBuffer);

				SAFE_RELEASE(PosBuffer);
				SAFE_RELEASE(OutlineStripVB);
				SAFE_RELEASE(RastState);
				SAFE_RELEASE(BlendState);
				SAFE_RELEASE(NopBlendState);
				SAFE_RELEASE(PointSampState);
				SAFE_RELEASE(LinearSampState);
				SAFE_RELEASE(NoDepthState);
				SAFE_RELEASE(LEqualDepthState);
				SAFE_RELEASE(NopDepthState);
				SAFE_RELEASE(AllPassDepthState);
				SAFE_RELEASE(AllPassIncrDepthState);
				SAFE_RELEASE(StencIncrEqDepthState);

				SAFE_RELEASE(GenericLayout);
				SAFE_RELEASE(GenericHomogLayout);
				SAFE_RELEASE(GenericVSCBuffer);
				SAFE_RELEASE(GenericGSCBuffer);
				SAFE_RELEASE(GenericPSCBuffer);
				SAFE_RELEASE(GenericVS);
				SAFE_RELEASE(TexDisplayPS);
				SAFE_RELEASE(CheckerboardPS);
				SAFE_RELEASE(MeshVS);
				SAFE_RELEASE(MeshGS);
				SAFE_RELEASE(MeshPS);
				SAFE_RELEASE(FullscreenVS);
				SAFE_RELEASE(WireframeVS);
				SAFE_RELEASE(WireframeHomogVS);
				SAFE_RELEASE(WireframePS);
				SAFE_RELEASE(OverlayPS);

				SAFE_RELEASE(CopyMSToArrayPS);
				SAFE_RELEASE(CopyArrayToMSPS);
				SAFE_RELEASE(FloatCopyMSToArrayPS);
				SAFE_RELEASE(FloatCopyArrayToMSPS);
				SAFE_RELEASE(DepthCopyMSToArrayPS);
				SAFE_RELEASE(DepthCopyArrayToMSPS);
				SAFE_RELEASE(PixelHistoryUnusedCS);
				SAFE_RELEASE(PixelHistoryDepthCopyCS);
				SAFE_RELEASE(PrimitiveIDPS);

				SAFE_RELEASE(QuadOverdrawPS);
				SAFE_RELEASE(QOResolvePS);

				SAFE_RELEASE(tileResultBuff);
				SAFE_RELEASE(resultBuff);
				SAFE_RELEASE(resultStageBuff);
				
				for(int i=0; i < 3; i++)
				{
					SAFE_RELEASE(tileResultUAV[i]);
					SAFE_RELEASE(resultUAV[i]);
					SAFE_RELEASE(tileResultSRV[i]);
				}
				
				for(int i=0; i < ARRAY_COUNT(TileMinMaxCS); i++)
				{
					for(int j=0; j < 3; j++)
					{
						SAFE_RELEASE(TileMinMaxCS[i][j]);
						SAFE_RELEASE(HistogramCS[i][j]);

						if(i == 0)
							SAFE_RELEASE(ResultMinMaxCS[j]);
					}
				}

				SAFE_RELEASE(histogramBuff);
				SAFE_RELEASE(histogramStageBuff);
				
				SAFE_RELEASE(histogramUAV);

				SAFE_DELETE_ARRAY(MeshVSBytecode);

				SAFE_RELEASE(PickPixelRT);
				SAFE_RELEASE(PickPixelStageTex);

				for(int i=0; i < ARRAY_COUNT(PublicCBuffers); i++)
				{
					SAFE_RELEASE(PublicCBuffers[i]);
				}
			}

			ID3D11Buffer *StageBuffer;

			ID3D11Buffer *PosBuffer, *OutlineStripVB;
			ID3D11RasterizerState *RastState;
			ID3D11SamplerState *PointSampState, *LinearSampState;
			ID3D11BlendState *BlendState, *NopBlendState;
			ID3D11DepthStencilState *NoDepthState, *LEqualDepthState, *NopDepthState,
			                        *AllPassDepthState, *AllPassIncrDepthState, *StencIncrEqDepthState;

			ID3D11InputLayout *GenericLayout, *GenericHomogLayout;
			ID3D11Buffer *GenericVSCBuffer;
			ID3D11Buffer *GenericGSCBuffer;
			ID3D11Buffer *GenericPSCBuffer;
			ID3D11Buffer *PublicCBuffers[20];
			ID3D11VertexShader *GenericVS, *WireframeVS, *MeshVS, *WireframeHomogVS, *FullscreenVS;
			ID3D11GeometryShader *MeshGS;
			ID3D11PixelShader *TexDisplayPS, *OverlayPS, *WireframePS, *MeshPS, *CheckerboardPS;
			ID3D11PixelShader *CopyMSToArrayPS, *CopyArrayToMSPS;
			ID3D11PixelShader *FloatCopyMSToArrayPS, *FloatCopyArrayToMSPS;
			ID3D11PixelShader *DepthCopyMSToArrayPS, *DepthCopyArrayToMSPS;
			ID3D11ComputeShader *PixelHistoryUnusedCS, *PixelHistoryDepthCopyCS;
			ID3D11PixelShader *PrimitiveIDPS;

			ID3D11PixelShader *QuadOverdrawPS, *QOResolvePS;
			
			ID3D11Buffer *tileResultBuff, *resultBuff, *resultStageBuff;
			ID3D11UnorderedAccessView *tileResultUAV[3], *resultUAV[3];
			ID3D11ShaderResourceView *tileResultSRV[3];
			ID3D11ComputeShader *TileMinMaxCS[eTexType_Max][3]; // uint, sint, float
			ID3D11ComputeShader *HistogramCS[eTexType_Max][3]; // uint, sint, float
			ID3D11ComputeShader *ResultMinMaxCS[3];
			ID3D11Buffer *histogramBuff, *histogramStageBuff;
			ID3D11UnorderedAccessView *histogramUAV;

			byte *MeshVSBytecode;
			uint32_t MeshVSBytelen;

			int publicCBufIdx;

			ID3D11RenderTargetView *PickPixelRT;
			ID3D11Texture2D *PickPixelStageTex;
		} m_DebugRender;

		bool InitDebugRendering();

		ShaderDebug::State CreateShaderDebugState(ShaderDebugTrace &trace, int quadIdx, DXBC::DXBCFile *dxbc, vector<byte> *cbufData);
		void CreateShaderGlobalState(ShaderDebug::GlobalState &global, DXBC::DXBCFile *dxbc, uint32_t UAVStartSlot, ID3D11UnorderedAccessView **UAVs, ID3D11ShaderResourceView **SRVs);
		void FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
								  const vector<DXBC::CBufferVariable> &invars, vector<ShaderVariable> &outvars,
								  const vector<byte> &data);
		friend struct ShaderDebugState;

		void FillTimers(uint32_t frameID, uint32_t &eventStart, rdctype::array<FetchDrawcall> &draws, vector<GPUTimer> &timers, int &reuseIdx);
		
		void FillCBuffer(ID3D11Buffer *buf, float *data, size_t size);
};