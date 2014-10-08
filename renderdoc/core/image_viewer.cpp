/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Baldur Karlsson
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

#include "core/core.h"
#include "replay/replay_driver.h"
#include "replay/type_helpers.h"

#include "stb/stb_image.h"
#include "common/dds_readwrite.h"

class ImageViewer : public IReplayDriver
{
	public:
		ImageViewer(IReplayDriver *proxy, const wchar_t *filename, ResourceId texID)
			: m_Proxy(proxy)
		{
			if(m_Proxy == NULL) RDCERR("Unexpectedly NULL proxy at creation of ImageViewer");

			m_Props.pipelineType = ePipelineState_D3D11;
			
			FetchFrameRecord record;
			record.frameInfo.fileOffset = 0;
			record.frameInfo.firstEvent = 1;
			record.frameInfo.frameNumber = 1;
			record.frameInfo.immContextId = ResourceId();

			create_array_uninit(record.drawcallList, 1);
			RDCEraseEl(record.drawcallList[0]);
			record.drawcallList[0].context = record.frameInfo.immContextId;
			record.drawcallList[0].drawcallID = 1;
			record.drawcallList[0].eventID = 1;
			record.drawcallList[0].name = filename;
			
			create_array_uninit(m_PipelineState.m_OM.RenderTargets, 1);
			m_PipelineState.m_OM.RenderTargets[0].Resource = texID;

			m_FrameRecord.push_back(record);
		}

		virtual ~ImageViewer()
		{
			m_Proxy->Shutdown();
			m_Proxy = NULL;
		}

		bool IsRemoteProxy() { return true; }
		void Shutdown() { delete this; }

		// pass through necessary operations to proxy
		uint64_t MakeOutputWindow(void *w, bool depth) { return m_Proxy->MakeOutputWindow(w, depth); }
		void DestroyOutputWindow(uint64_t id) { m_Proxy->DestroyOutputWindow(id); }
		bool CheckResizeOutputWindow(uint64_t id) { return m_Proxy->CheckResizeOutputWindow(id); }
		void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h) { m_Proxy->GetOutputWindowDimensions(id, w, h); }
		void ClearOutputWindowColour(uint64_t id, float col[4]) { m_Proxy->ClearOutputWindowColour(id, col); }
		void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil) { m_Proxy->ClearOutputWindowDepth(id, depth, stencil); }
		void BindOutputWindow(uint64_t id, bool depth) { m_Proxy->BindOutputWindow(id, depth); }
		bool IsOutputWindowVisible(uint64_t id) { return m_Proxy->IsOutputWindowVisible(id); }
		void FlipOutputWindow(uint64_t id) { m_Proxy->FlipOutputWindow(id); }
		void RenderCheckerboard(Vec3f light, Vec3f dark) { m_Proxy->RenderCheckerboard(light, dark); }
		void RenderHighlightBox(float w, float h, float scale) { m_Proxy->RenderHighlightBox(w, h, scale); }
		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval) 
		{ return m_Proxy->GetMinMax(texid, sliceFace, mip, sample, minval, maxval); }
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
		{ return m_Proxy->GetHistogram(texid, sliceFace, mip, sample, minval, maxval, channels, histogram); }
		bool RenderTexture(TextureDisplay cfg) { return m_Proxy->RenderTexture(cfg); }
		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
		{ m_Proxy->PickPixel(texture, x, y, sliceFace, mip, sample, pixel); }
		void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
		{ m_Proxy->BuildCustomShader(source, entry, compileFlags, type, id, errors); }
		void FreeCustomShader(ResourceId id) { m_Proxy->FreeTargetResource(id); }
		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip) { return m_Proxy->ApplyCustomShader(shader, texid, mip); }
		vector<ResourceId> GetTextures() { return m_Proxy->GetTextures(); }
		FetchTexture GetTexture(ResourceId id) { return m_Proxy->GetTexture(id); }
		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize)
		{ return m_Proxy->GetTextureData(tex, arrayIdx, mip, resolve, forceRGBA8unorm, blackPoint, whitePoint, dataSize); }

		// handle a couple of operations ourselves to return a simple fake log
		APIProperties GetAPIProperties() { return m_Props; }
		vector<FetchFrameRecord> GetFrameRecord() { return m_FrameRecord; }
		D3D11PipelineState GetD3D11PipelineState() { return m_PipelineState; }
		
		// other operations are dropped/ignored, to avoid confusion
		void ReadLogInitialisation() {}
		void RenderMesh(uint32_t frameID, const vector<uint32_t> &events, MeshDisplay cfg) {}
		vector<ResourceId> GetBuffers() { return vector<ResourceId>(); }
		FetchBuffer GetBuffer(ResourceId id) { FetchBuffer ret; RDCEraseEl(ret); return ret; }
		void SavePipelineState() {}
		GLPipelineState GetGLPipelineState() { return GLPipelineState(); }
		void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv) {}
		void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType) {}
		vector<EventUsage> GetUsage(ResourceId id) { return vector<EventUsage>(); }
		bool IsRenderOutput(ResourceId id) { return false; }
		ResourceId GetLiveID(ResourceId id) { return id; }
		void TimeDrawcalls(rdctype::array<FetchDrawcall> &arr) {}
		void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data) {}
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len) { return vector<byte>(); }
		void InitPostVSBuffers(uint32_t frameID, uint32_t eventID) {}
		PostVSMeshData GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage) { PostVSMeshData ret; RDCEraseEl(ret); return ret; }
		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents) { return ResourceId(); }
		ShaderReflection *GetShader(ResourceId id) { return NULL; }
		bool HasCallstacks() { return false; }
		void InitCallstackResolver() {}
		Callstack::StackResolver *GetCallstackResolver() { return NULL; }
		void FreeTargetResource(ResourceId id) {}
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t sampleIdx)
		{ return vector<PixelModification>(); }
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
		{ ShaderDebugTrace ret; RDCEraseEl(ret); return ret; }
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
		{ ShaderDebugTrace ret; RDCEraseEl(ret); return ret; }
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
		{ ShaderDebugTrace ret; RDCEraseEl(ret); return ret; }
		void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors) {}
		void ReplaceResource(ResourceId from, ResourceId to) {}
		void RemoveReplacement(ResourceId id) {}

		// these are proxy functions, and will never be used
		ResourceId CreateProxyTexture(FetchTexture templateTex)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
			return ResourceId();
		}

		void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
		}

	private:
		APIProperties m_Props;
		vector<FetchFrameRecord> m_FrameRecord;
		D3D11PipelineState m_PipelineState;
		IReplayDriver *m_Proxy;
};

ReplayCreateStatus IMG_CreateReplayDevice(const wchar_t *logfile, IReplayDriver **driver)
{
	FILE *f = FileIO::fopen(logfile, L"rb");

	if(!f)
		return eReplayCreate_FileIOFailed;
		
	FetchTexture texDetails;

	ResourceFormat rgba8_unorm;
	rgba8_unorm.compByteWidth = 1;
	rgba8_unorm.compCount = 4;
	rgba8_unorm.compType = eCompType_UNorm;
	rgba8_unorm.special = false;

	ResourceFormat rgba32_float = rgba8_unorm;
	rgba32_float.compByteWidth = 4;
	rgba32_float.compType = eCompType_Float;

	texDetails.creationFlags = eTextureCreate_SwapBuffer|eTextureCreate_RTV;
	texDetails.cubemap = false;
	texDetails.customName = true;
	texDetails.name = logfile;
	texDetails.ID = ResourceId();
	texDetails.byteSize = 0;
	texDetails.msQual = 0;
	texDetails.msSamp = 1;

	// reasonable defaults for everything but dds
	texDetails.numSubresources = 1;
	texDetails.dimension = 2;
	texDetails.arraysize = 1;
	texDetails.depth = 1;
	texDetails.mips = 1;

	byte *data = NULL;
	size_t datasize = 0;

	bool dds = false;

	if(stbi_is_hdr_from_file(f))
	{
		texDetails.format = rgba32_float;

		FileIO::fseek64(f, 0, SEEK_SET);

		int ignore = 0;
		data = (byte *)stbi_loadf_from_file(f, (int *)&texDetails.width, (int *)&texDetails.height, &ignore, 4);
		datasize = texDetails.width*texDetails.height*4*sizeof(float);
	}
	else if(is_dds_file(f))
	{
		dds = true;
	}
	else
	{
		int ignore = 0;
		int ret = stbi_info_from_file(f, (int *)&texDetails.width, (int *)&texDetails.height, &ignore);

		// just in case (we shouldn't have come in here if this weren't true), make sure
		// the format is supported
		if(ret == 0 ||
			 texDetails.width == 0 || texDetails.width == ~0U ||
			 texDetails.height == 0 || texDetails.height == ~0U)
		{
			return eReplayCreate_APIUnsupported;
		}

		texDetails.format = rgba8_unorm;

		data = stbi_load_from_file(f, (int *)&texDetails.width, (int *)&texDetails.height, &ignore, 4);
		datasize = texDetails.width*texDetails.height*4*sizeof(byte);
	}
	
	// if we don't have data at this point (and we're not a dds file) then the
	// file was corrupted and we failed to load it
	if(!dds && data == NULL)
	{
		return eReplayCreate_FileCorrupted;
	}
	
	IReplayDriver *proxy = NULL;
	auto status = RenderDoc::Inst().CreateReplayDriver(RDC_Unknown, NULL, &proxy);
	
	if(status != eReplayCreate_Success || !proxy)
	{
		if(proxy) proxy->Shutdown();
		return status;
	}

	ResourceId id;

	if(!dds)
	{
		id = proxy->CreateProxyTexture(texDetails);

		proxy->SetProxyTextureData(id, 0, 0, data, datasize);
		free(data);
	}
	else
	{
		FileIO::fseek64(f, 0, SEEK_SET);
		dds_data read_data = load_dds_from_file(f);
		
		if(read_data.subdata == NULL)
		{
			proxy->Shutdown();
			return eReplayCreate_FileCorrupted;
		}

		texDetails.cubemap = read_data.cubemap;
		texDetails.arraysize = read_data.slices;
		texDetails.width = read_data.width;
		texDetails.height = read_data.height;
		texDetails.depth = read_data.depth;
		texDetails.mips = read_data.mips;
		texDetails.numSubresources = texDetails.arraysize*texDetails.mips;
		texDetails.format = read_data.format;
		                         texDetails.dimension = 1;
		if(texDetails.width > 1) texDetails.dimension = 2;
		if(texDetails.depth > 1) texDetails.dimension = 3;

		id = proxy->CreateProxyTexture(texDetails);

		for(uint32_t i=0; i < texDetails.numSubresources; i++)
		{
			proxy->SetProxyTextureData(id, i/texDetails.mips, i%texDetails.mips, read_data.subdata[i], (size_t)read_data.subsizes[i]);

			delete[] read_data.subdata[i];
		}

		delete[] read_data.subdata;
		delete[] read_data.subsizes;
	}

	*driver = new ImageViewer(proxy, logfile, id);

	FileIO::fclose(f);

	return eReplayCreate_Success;
}

static DriverRegistration IMGDriverRegistration(RDC_Image, L"Image", &IMG_CreateReplayDevice);
