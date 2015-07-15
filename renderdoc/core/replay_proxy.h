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

#include "os/os_specific.h"
#include "serialise/serialiser.h"
#include "socket_helpers.h"
#include "replay/replay_driver.h"

enum CommandPacketType
{
	eCommand_SetCtxFilter,
	eCommand_ReplayLog,

	eCommand_GetTextures,
	eCommand_GetTexture,
	eCommand_GetBuffers,
	eCommand_GetBuffer,
	eCommand_GetShader,
	eCommand_GetDebugMessages,

	eCommand_GetBufferData,
	eCommand_GetTextureData,

	eCommand_SavePipelineState,
	eCommand_GetUsage,
	eCommand_GetLiveID,
	eCommand_GetFrameRecord,

	eCommand_FreeResource,
	eCommand_HasResolver,

	eCommand_FetchCounters,
	eCommand_EnumerateCounters,
	eCommand_DescribeCounter,
	eCommand_FillCBufferVariables,

	eCommand_InitPostVS,
	eCommand_GetPostVS,

	eCommand_InitStackResolver,
	eCommand_HasStackResolver,
	eCommand_GetAddressDetails,

	eCommand_BuildTargetShader,
	eCommand_ReplaceResource,
	eCommand_RemoveReplacement,

	eCommand_DebugVertex,
	eCommand_DebugPixel,
	eCommand_DebugThread,

	eCommand_RenderOverlay,

	eCommand_GetAPIProperties,
	
	eCommand_PixelHistory,
};

// This class implements IReplayDriver and StackResolver. On the local machine where the UI
// is, this can then act like a full local replay by farming out over the network to a remote
// replay where necessary to implement some functions, and using a local proxy where necessary.
//
// This class is also used on the remote replay just so we can re-use the serialisation logic
// across the network before and after implementing the IRemoteDriver parts.
class ProxySerialiser : public IReplayDriver, Callstack::StackResolver
{
	public:
		ProxySerialiser(Network::Socket *sock, IReplayDriver *proxy)
			: m_Socket(sock), m_Proxy(proxy), m_Remote(NULL), m_ReplayHost(false)
		{
			m_FromReplaySerialiser = NULL;
			m_ToReplaySerialiser = new Serialiser(NULL, Serialiser::WRITING, false);
			m_RemoteHasResolver = false;
		}

		ProxySerialiser(Network::Socket *sock, IRemoteDriver *remote)
			: m_Socket(sock), m_Proxy(NULL), m_Remote(remote), m_ReplayHost(true)
		{
			m_ToReplaySerialiser = NULL;
			m_FromReplaySerialiser = new Serialiser(NULL, Serialiser::WRITING, false);
			m_RemoteHasResolver = false;
		}

		virtual ~ProxySerialiser();

		bool IsRemoteProxy() { return !m_ReplayHost; }
		void Shutdown() { delete this; }
		
		void ReadLogInitialisation() {}
		
		uint64_t MakeOutputWindow(void *w, bool depth)
		{
			if(m_Proxy)
				return m_Proxy->MakeOutputWindow(w, depth);
			return 0;
		}
		void DestroyOutputWindow(uint64_t id)
		{
			if(m_Proxy)
				return m_Proxy->DestroyOutputWindow(id);
		}
		bool CheckResizeOutputWindow(uint64_t id)
		{
			if(m_Proxy)
				return m_Proxy->CheckResizeOutputWindow(id);
			return false;
		}
		void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
		{
			if(m_Proxy)
				return m_Proxy->GetOutputWindowDimensions(id, w, h);
		}
		void ClearOutputWindowColour(uint64_t id, float col[4])
		{
			if(m_Proxy)
				return m_Proxy->ClearOutputWindowColour(id, col);
		}
		void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
		{
			if(m_Proxy)
				return m_Proxy->ClearOutputWindowDepth(id, depth, stencil);
		}
		void BindOutputWindow(uint64_t id, bool depth)
		{
			if(m_Proxy)
				return m_Proxy->BindOutputWindow(id, depth);
		}
		bool IsOutputWindowVisible(uint64_t id)
		{
			if(m_Proxy)
				return m_Proxy->IsOutputWindowVisible(id);
			return false;
		}
		void FlipOutputWindow(uint64_t id)
		{
			if(m_Proxy)
				return m_Proxy->FlipOutputWindow(id);
		}
		
		void RenderCheckerboard(Vec3f light, Vec3f dark)
		{
			if(m_Proxy)
				return m_Proxy->RenderCheckerboard(light, dark);
		}
		
		void RenderHighlightBox(float w, float h, float scale)
		{
			if(m_Proxy)
				return m_Proxy->RenderHighlightBox(w, h, scale);
		}
		
		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
		{
			if(m_Proxy)
			{
				EnsureTexCached(texid, sliceFace, mip);
				return m_Proxy->GetMinMax(m_ProxyTextureIds[texid], sliceFace, mip, sample, minval, maxval);
			}

			return false;
		}

		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
		{
			if(m_Proxy)
			{
				EnsureTexCached(texid, sliceFace, mip);
				return m_Proxy->GetHistogram(m_ProxyTextureIds[texid], sliceFace, mip, sample, minval, maxval, channels, histogram);
			}

			return false;
		}

		bool RenderTexture(TextureDisplay cfg)
		{
			if(m_Proxy)
			{
				EnsureTexCached(cfg.texid, cfg.sliceFace, cfg.mip);
				cfg.texid = m_ProxyTextureIds[cfg.texid];
				return m_Proxy->RenderTexture(cfg);
			}

			return false;
		}

		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
		{
			if(m_Proxy)
			{
				EnsureTexCached(texture, sliceFace, mip);
				m_Proxy->PickPixel(m_ProxyTextureIds[texture], x, y, sliceFace, mip, sample, pixel);
			}
		}
			
		void RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
		{
			if(m_Proxy && cfg.position.buf != ResourceId())
			{
				EnsureBufCached(cfg.position.buf);
				cfg.position.buf = m_ProxyBufferIds[cfg.position.buf];
				
				if(cfg.second.buf != ResourceId())
				{
					EnsureBufCached(cfg.second.buf);
					cfg.second.buf = m_ProxyBufferIds[cfg.second.buf];
				}
				
				if(cfg.position.idxbuf != ResourceId())
				{
					EnsureBufCached(cfg.position.idxbuf);
					cfg.position.idxbuf = m_ProxyBufferIds[cfg.position.idxbuf];
				}

				vector<MeshFormat> secDraws = secondaryDraws;

				for(size_t i=0; i < secDraws.size(); i++)
				{
					if(secDraws[i].buf != ResourceId())
					{
						EnsureBufCached(secDraws[i].buf);
						secDraws[i].buf = m_ProxyBufferIds[secDraws[i].buf];
					}
					if(secDraws[i].idxbuf != ResourceId())
					{
						EnsureBufCached(secDraws[i].idxbuf);
						secDraws[i].idxbuf = m_ProxyBufferIds[secDraws[i].idxbuf];
					}
				}

				m_Proxy->RenderMesh(frameID, eventID, secDraws, cfg);
			}
		}

		uint32_t PickVertex(uint32_t frameID, uint32_t eventID, MeshDisplay cfg, uint32_t x, uint32_t y)
		{
			if(m_Proxy && cfg.position.buf != ResourceId())
			{
				EnsureBufCached(cfg.position.buf);
				cfg.position.buf = m_ProxyBufferIds[cfg.position.buf];

				if(cfg.second.buf != ResourceId())
				{
					EnsureBufCached(cfg.second.buf);
					cfg.second.buf = m_ProxyBufferIds[cfg.second.buf];
				}

				if(cfg.position.idxbuf != ResourceId())
				{
					EnsureBufCached(cfg.position.idxbuf);
					cfg.position.idxbuf = m_ProxyBufferIds[cfg.position.idxbuf];
				}

				return m_Proxy->PickVertex(frameID, eventID, cfg, x, y);
			}

			return ~0U;
		}
		
		void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
		{
			if(m_Proxy)
			{
				m_Proxy->BuildCustomShader(source, entry, compileFlags, type, id, errors);
			}
			else
			{
				if(id) *id = ResourceId();
				if(errors) *errors = "Unsupported BuildShader call on proxy without local renderer";
			}
		}
		
		void FreeCustomShader(ResourceId id)
		{
			if(m_Proxy)
				m_Proxy->FreeTargetResource(id);
		}

		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
		{
			if(m_Proxy)
			{
				EnsureTexCached(texid, 0, mip);
				texid = m_ProxyTextureIds[texid];
				ResourceId customResourceId = m_Proxy->ApplyCustomShader(shader, texid, mip);
				m_LocalTextures.insert(customResourceId);
				m_ProxyTextureIds[customResourceId] = customResourceId;
				return customResourceId;
			}

			return ResourceId();
		}

		bool Tick();

		vector<ResourceId> GetBuffers();
		FetchBuffer GetBuffer(ResourceId id);

		vector<ResourceId> GetTextures();
		FetchTexture GetTexture(ResourceId id);

		APIProperties GetAPIProperties();

		vector<DebugMessage> GetDebugMessages();
		
		void SavePipelineState();
		D3D11PipelineState GetD3D11PipelineState() { return m_D3D11PipelineState; }
		GLPipelineState GetGLPipelineState() { return m_GLPipelineState; }
		
		void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
		void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
		
		vector<EventUsage> GetUsage(ResourceId id);
		vector<FetchFrameRecord> GetFrameRecord();
		
		bool IsRenderOutput(ResourceId id);
		
		ResourceId GetLiveID(ResourceId id);
		
		vector<uint32_t> EnumerateCounters();
		void DescribeCounter(uint32_t counterID, CounterDescription &desc);
		vector<CounterResult> FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counterID);
		
		void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data);
		
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len);
		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize);
		
		void InitPostVSBuffers(uint32_t frameID, uint32_t eventID);
		MeshFormat GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage);
		
		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents);

		ShaderReflection *GetShader(ResourceId id);
		
		bool HasCallstacks();
		void InitCallstackResolver();
		Callstack::StackResolver *GetCallstackResolver();
		// implementing Callstack::StackResolver
		Callstack::AddressDetails GetAddr(uint64_t addr);
		
		void FreeTargetResource(ResourceId id);
			
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx);
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive);
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
		
		void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);

		void FileChanged() {}

		// will never be used
		ResourceId CreateProxyTexture(FetchTexture templateTex)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
			return ResourceId();
		}

		void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
		}

		ResourceId CreateProxyBuffer(FetchBuffer templateBuf)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
			return ResourceId();
		}

		void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
		{
			RDCERR("Calling proxy-render functions on a proxy serialiser");
		}

	private:
		bool SendReplayCommand(CommandPacketType type);

		void EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip);
		void EnsureBufCached(ResourceId bufid);

		struct TextureCacheEntry
		{
			ResourceId replayid;
			uint32_t arrayIdx;
			uint32_t mip;

			bool operator <(const TextureCacheEntry &o) const
			{
				if(replayid != o.replayid)
					return replayid < o.replayid;
				if(arrayIdx != o.arrayIdx)
					return arrayIdx < o.arrayIdx;
				return mip < o.mip;
			}
		};
		set<TextureCacheEntry> m_TextureProxyCache;
		set<ResourceId> m_LocalTextures;
		map<ResourceId, ResourceId> m_ProxyTextureIds;
		
		set<ResourceId> m_BufferProxyCache;
		map<ResourceId, ResourceId> m_ProxyBufferIds;
		
		map<ResourceId, ResourceId> m_LiveIDs;

		map<ResourceId, ShaderReflection *> m_ShaderReflectionCache;

		Network::Socket *m_Socket;
		Serialiser *m_FromReplaySerialiser;
		Serialiser *m_ToReplaySerialiser;
		IReplayDriver *m_Proxy;
		IRemoteDriver *m_Remote;
		bool m_ReplayHost;

		bool m_RemoteHasResolver;

		APIProperties m_APIProperties;
		D3D11PipelineState m_D3D11PipelineState;
		GLPipelineState m_GLPipelineState;
};
