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


#include "maths/vec.h"
#include "d3d11_manager.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "shaders/dxbc_debug.h"
#include "maths/formatpacking.h"
#include "data/resource.h"
#include "serialise/serialiser.h"
#include "common/string_utils.h"

#include "driver/d3d11/d3d11_resources.h"
#include "driver/d3d11/d3d11_renderstate.h"

void D3D11DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
												const vector<DXBC::CBufferVariable> &invars, vector<ShaderVariable> &outvars,
											  const vector<byte> &data)
{
	using namespace DXBC;
	using namespace ShaderDebug;

	size_t o = offset;

	for(size_t v=0; v < invars.size(); v++)
	{
		size_t vec = o + invars[v].descriptor.offset/16;
		size_t comp = (invars[v].descriptor.offset - (invars[v].descriptor.offset&~0xf))/4;
		size_t sz = RDCMAX(1U, invars[v].type.descriptor.bytesize/16);

		offset = vec + sz;

		string basename = prefix + invars[v].name;
		
		uint32_t rows = invars[v].type.descriptor.rows;
		uint32_t cols = invars[v].type.descriptor.cols;
		uint32_t elems = RDCMAX(1U,invars[v].type.descriptor.elements);

		if(!invars[v].type.members.empty())
		{
			char buf[64] = {0};
			StringFormat::snprintf(buf, 63, "[%d]", elems);

			ShaderVariable var;
			var.name = basename;
			var.rows = var.columns = 0;
			var.type = eVar_Float;
			
			std::vector<ShaderVariable> varmembers;

			if(elems > 1)
			{
				for(uint32_t i=0; i < elems; i++)
				{
					StringFormat::snprintf(buf, 63, "[%d]", i);

					if(flatten)
					{
						FillCBufferVariables(basename + buf + ".", vec, flatten, invars[v].type.members, outvars, data);
					}
					else
					{					
						ShaderVariable vr;
						vr.name = basename + buf;
						vr.rows = vr.columns = 0;
						vr.type = eVar_Float;

						std::vector<ShaderVariable> mems;

						FillCBufferVariables("", vec, flatten, invars[v].type.members, mems, data);

						vr.members = mems;

						varmembers.push_back(vr);
					}
				}
			}
			else
			{
				if(flatten)
					FillCBufferVariables(basename + ".", vec, flatten, invars[v].type.members, outvars, data);
				else
					FillCBufferVariables("", vec, flatten, invars[v].type.members, varmembers, data);
			}

			if(!flatten)
			{
				var.members = varmembers;
				outvars.push_back(var);
			}

			continue;
		}
		
		size_t elemByteSize = 4;
		VarType type = eVar_Float;
		switch(invars[v].type.descriptor.type)
		{
			case VARTYPE_INT:
				type = eVar_Int;
				break;
			case VARTYPE_FLOAT:
				type = eVar_Float;
				break;
			case VARTYPE_BOOL:
			case VARTYPE_UINT:
			case VARTYPE_UINT8:
				type = eVar_UInt;
				break;
			case VARTYPE_DOUBLE:
				elemByteSize = 8;
				type = eVar_Double;
				break;
			default:
				RDCFATAL("Unexpected type in constant buffer");
		}
		
		bool columnMajor = invars[v].type.descriptor.varClass == CLASS_MATRIX_COLUMNS;

		size_t outIdx = vec;
		if(!flatten)
		{
			outIdx = outvars.size();
			outvars.resize(RDCMAX(outIdx+1, outvars.size()));
		}
		else
		{
			if(columnMajor)
				outvars.resize(RDCMAX(outIdx+cols*elems, outvars.size()));
			else
				outvars.resize(RDCMAX(outIdx+rows*elems, outvars.size()));
		}
		
		size_t dataOffset = vec*16 + comp*4;

		if(outvars[outIdx].name.count > 0)
		{
			RDCASSERT(flatten);

			RDCASSERT(outvars[vec].rows == 1);
			RDCASSERT(outvars[vec].columns == comp);
			RDCASSERT(rows == 1);

			string combinedName = outvars[outIdx].name.elems;
			combinedName += ", " + basename;
			outvars[outIdx].name = combinedName;
			outvars[outIdx].rows = 1;
			outvars[outIdx].columns += cols;

			if(dataOffset < data.size())
			{
				const byte *d = &data[dataOffset];

				memcpy(&outvars[outIdx].value.uv[comp], d, RDCMIN(data.size()-dataOffset, elemByteSize*cols));
			}
		}
		else
		{
			outvars[outIdx].name = basename;
			outvars[outIdx].rows = 1;
			outvars[outIdx].type = type;
			outvars[outIdx].columns = cols;

			ShaderVariable &var = outvars[outIdx];

			bool isArray = invars[v].type.descriptor.elements > 1;

			if(rows*elems == 1)
			{
				if(dataOffset < data.size())
				{
					const byte *d = &data[dataOffset];

					memcpy(&outvars[outIdx].value.uv[flatten ? comp : 0], d, RDCMIN(data.size()-dataOffset, elemByteSize*cols));
				}
			}
			else if(!isArray && !flatten)
			{
				outvars[outIdx].rows = rows;

				if(dataOffset < data.size())
				{
					const byte *d = &data[dataOffset];

					RDCASSERT(rows <= 4 && rows*cols <= 16);

					if(columnMajor)
					{
						uint32_t tmp[16] = {0};

						// matrices always have 4 columns, for padding reasons (the same reason arrays
						// put every element on a new vec4)
						for(uint32_t r=0; r < rows; r++)
						{
							size_t srcoffs = 4*elemByteSize*r;
							size_t dstoffs = cols*elemByteSize*r;
							memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
											RDCMIN(data.size()-dataOffset + srcoffs, elemByteSize*cols));
						}

						// transpose
						for(size_t r=0; r < rows; r++)
							for(size_t c=0; c < cols; c++)
								outvars[outIdx].value.uv[r*cols+c] = tmp[c*rows+r];
					}
					else // CLASS_MATRIX_ROWS or other data not to transpose.
					{
						// matrices always have 4 columns, for padding reasons (the same reason arrays
						// put every element on a new vec4)
						for(uint32_t r=0; r < rows; r++)
						{
							size_t srcoffs = 4*elemByteSize*r;
							size_t dstoffs = cols*elemByteSize*r;
							memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
											RDCMIN(data.size()-dataOffset + srcoffs, elemByteSize*rows*cols));
						}
					}
				}
			}
			else if(rows*elems > 1)
			{
				char buf[64] = {0};

				var.name = outvars[outIdx].name;

				vector<ShaderVariable> varmembers;
				vector<ShaderVariable> *out = &outvars;
				size_t rowCopy = 1;

				uint32_t registers = rows; 
				uint32_t regLen = cols;
				const char *regName = "row";

				if(!flatten)
				{
					var.rows = 0;
					var.columns = 0;
					outIdx = 0;
					out = &varmembers;
					varmembers.resize(elems);
					rowCopy = rows;
					rows = 1;
					registers = 1;
				}
				else
				{
					if(columnMajor)
					{
						registers = cols;
						regLen = rows;
						regName = "col";
					}
				}

				string base = outvars[outIdx].name.elems;

				for(size_t r=0; r < registers*elems; r++)
				{
					if(isArray && registers > 1)
						StringFormat::snprintf(buf, 63, "[%d].%hs%d", r/registers, regName, r%registers);
					else if(registers > 1)
						StringFormat::snprintf(buf, 63, ".%hs%d", regName, r);
					else
						StringFormat::snprintf(buf, 63, "[%d]", r);

					(*out)[outIdx+r].name = base + buf;
					(*out)[outIdx+r].rows = (uint32_t)rowCopy;
					(*out)[outIdx+r].type = type;
					(*out)[outIdx+r].columns = regLen;
					
					size_t dataOffset = (vec+r*rowCopy)*16;

					if(dataOffset < data.size())
					{
						const byte *d = &data[dataOffset];

						memcpy(&((*out)[outIdx+r].value.uv[0]), d, RDCMIN(data.size()-dataOffset, elemByteSize*rowCopy*regLen));

						if(!flatten && columnMajor)
						{
							ShaderVariable tmp = (*out)[outIdx];
							// transpose
							for(size_t r=0; r < rows; r++)
								for(size_t c=0; c < cols; c++)
									(*out)[outIdx].value.uv[r*cols+c] = tmp.value.uv[c*rows+r];
						}
					}
				}

				if(!flatten)
					var.members = varmembers;
			}
		}
	}
}

void D3D11DebugManager::FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars, vector<ShaderVariable> &outvars,
											 bool flattenVec4s, const vector<byte> &data)
{
	size_t zero = 0;

	vector<ShaderVariable> v;
	FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

	outvars.reserve(v.size());
	for(size_t i=0; i < v.size(); i++)
		outvars.push_back(v[i]);
}

ShaderDebug::State D3D11DebugManager::CreateShaderDebugState(ShaderDebugTrace &trace, int quadIdx, DXBC::DXBCFile *dxbc, vector<byte> *cbufData)
{
	using namespace DXBC;
	using namespace ShaderDebug;

	State initialState = State(quadIdx, &trace, dxbc, m_WrappedDevice);

	// use pixel shader here to get inputs
	
	uint32_t maxReg = 0;
	for(size_t i=0; i < dxbc->m_InputSig.size(); i++)
		maxReg = RDCMAX(maxReg, dxbc->m_InputSig[i].regIndex);

	create_array(trace.inputs, maxReg+1);
	for(size_t i=0; i < dxbc->m_InputSig.size(); i++)
	{
		char buf[64] = {0};
		
		SigParameter &sig = dxbc->m_InputSig[i];

		StringFormat::snprintf(buf, 63, "v%d", sig.regIndex);
		
		ShaderVariable v;

		v.name = StringFormat::Fmt("%hs (%hs)", buf, sig.semanticIdxName.elems);
		v.rows = 1;
		v.columns = 
			sig.regChannelMask & 0x8 ? 4 :
			sig.regChannelMask & 0x4 ? 3 : 
			sig.regChannelMask & 0x2 ? 2 :
			sig.regChannelMask & 0x1 ? 1 :
			0;

		if(sig.compType == eCompType_UInt)
			v.type = eVar_UInt;
		else if(sig.compType == eCompType_SInt)
			v.type = eVar_Int;

		if(trace.inputs[sig.regIndex].columns == 0)
			trace.inputs[sig.regIndex] = v;
		else
			trace.inputs[sig.regIndex].columns = RDCMAX(trace.inputs[sig.regIndex].columns, v.columns);
	}
	
	uint32_t specialOutputs = 0;
	maxReg = 0;
	for(size_t i=0; i < dxbc->m_OutputSig.size(); i++)
	{
		if(dxbc->m_OutputSig[i].regIndex == ~0U)
			specialOutputs++;
		else
			maxReg = RDCMAX(maxReg, dxbc->m_OutputSig[i].regIndex);
	}
	
	create_array(initialState.outputs, maxReg+1 + specialOutputs);
	for(size_t i=0; i < dxbc->m_OutputSig.size(); i++)
	{
		SigParameter &sig = dxbc->m_OutputSig[i];

		if(sig.regIndex == ~0U)
			continue;

		char buf[64] = {0};

		StringFormat::snprintf(buf, 63, "o%d", sig.regIndex);
		
		ShaderVariable v;
		
		v.name = StringFormat::Fmt("%hs (%hs)", buf, sig.semanticIdxName.elems);
		v.rows = 1;
		v.columns = 
			sig.regChannelMask & 0x8 ? 4 :
			sig.regChannelMask & 0x4 ? 3 : 
			sig.regChannelMask & 0x2 ? 2 :
			sig.regChannelMask & 0x1 ? 1 :
			0;

		if(initialState.outputs[sig.regIndex].columns == 0)
			initialState.outputs[sig.regIndex] = v;
		else
			initialState.outputs[sig.regIndex].columns = RDCMAX(initialState.outputs[sig.regIndex].columns, v.columns);
	}

	for(size_t i=0; i < dxbc->m_OutputSig.size(); i++)
	{
		SigParameter &sig = dxbc->m_OutputSig[i];

		if(sig.regIndex != ~0U)
			continue;
		
		ShaderVariable v;
		
		     if(sig.systemValue == eAttr_OutputControlPointIndex)			v.name = "vOutputControlPointID";
		else if(sig.systemValue == eAttr_DepthOutput)						v.name = "oDepth";
		else if(sig.systemValue == eAttr_DepthOutputLessEqual)				v.name = "oDepthLessEqual";
		else if(sig.systemValue == eAttr_DepthOutputGreaterEqual)			v.name = "oDepthGreaterEqual";
		else if(sig.systemValue == eAttr_MSAACoverage)						v.name = "oMask";
		//if(sig.systemValue == TYPE_OUTPUT_CONTROL_POINT)							str = "oOutputControlPoint";
		else
		{
			RDCERR("Unhandled output: %hs (%d)", sig.semanticName, sig.systemValue);
			continue;
		}

		v.rows = 1;
		v.columns = 
			sig.regChannelMask & 0x8 ? 4 :
			sig.regChannelMask & 0x4 ? 3 : 
			sig.regChannelMask & 0x2 ? 2 :
			sig.regChannelMask & 0x1 ? 1 :
			0;

		initialState.outputs[maxReg+i] = v;
	}

	create_array(trace.cbuffers, dxbc->m_CBuffers.size());
	for(size_t i=0; i < dxbc->m_CBuffers.size(); i++)
	{
		if(dxbc->m_CBuffers[i].descriptor.type != CBuffer::Descriptor::TYPE_CBUFFER)
			continue;

		vector<ShaderVariable> vars;

		FillCBufferVariables(dxbc->m_CBuffers[i].variables, vars, true, cbufData[i]);

		trace.cbuffers[i] = vars;

		for(int32_t c=0; c < trace.cbuffers[i].count; c++)
			trace.cbuffers[i][c].name = StringFormat::Fmt("cb%u[%u] (%hs)", (uint32_t)i, (uint32_t)c, trace.cbuffers[i][c].name.elems);
	}

	initialState.Init();

	return initialState;
}

void D3D11DebugManager::CreateShaderGlobalState(ShaderDebug::GlobalState &global, uint32_t UAVStartSlot, ID3D11UnorderedAccessView **UAVs, ID3D11ShaderResourceView **SRVs)
{
	for(int i=0; UAVs != NULL && i+UAVStartSlot < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		int dsti = i+UAVStartSlot;
		if(UAVs[i])
		{
			ID3D11Resource *res = NULL;
			UAVs[i]->GetResource(&res);
			
			global.uavs[dsti].hiddenCounter = GetStructCount(UAVs[i]);

			D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
			UAVs[i]->GetDesc(&udesc);

			if(udesc.Format != DXGI_FORMAT_UNKNOWN)
			{
				ResourceFormat fmt = MakeResourceFormat(udesc.Format);

				global.uavs[dsti].format.byteWidth = fmt.compByteWidth;
				global.uavs[dsti].format.numComps = fmt.compCount;
				global.uavs[dsti].format.fmt = fmt.compType;

				if(udesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
					global.uavs[dsti].format.byteWidth = 11;
				if(udesc.Format == DXGI_FORMAT_R10G10B10A2_UINT || udesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
					global.uavs[dsti].format.byteWidth = 10;
			}

			if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
			{
				global.uavs[dsti].firstElement = udesc.Buffer.FirstElement;
				global.uavs[dsti].numElements = udesc.Buffer.NumElements;
			}

			if(res)
			{
				if(WrappedID3D11Buffer::IsAlloc(res))
				{
					global.uavs[dsti].data = GetBufferData((ID3D11Buffer *)res, 0, 0);
				}
				else
				{
					RDCERR("UAVs of textures currently not supported in shader debugging");
				}
			}

			SAFE_RELEASE(res);
		}
	}

	for(int i=0; SRVs != NULL && i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		if(SRVs[i])
		{
			ID3D11Resource *res = NULL;
			SRVs[i]->GetResource(&res);
			
			D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
			SRVs[i]->GetDesc(&sdesc);

			if(sdesc.Format != DXGI_FORMAT_UNKNOWN)
			{
				ResourceFormat fmt = MakeResourceFormat(sdesc.Format);

				global.srvs[i].format.byteWidth = fmt.compByteWidth;
				global.srvs[i].format.numComps = fmt.compCount;
				global.srvs[i].format.fmt = fmt.compType;

				if(sdesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
					global.srvs[i].format.byteWidth = 11;
				if(sdesc.Format == DXGI_FORMAT_R10G10B10A2_UINT || sdesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
					global.srvs[i].format.byteWidth = 10;
			}

			if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
			{
				// I know this isn't what the docs say, but as best as I can tell
				// this is how it's used.
				global.srvs[i].firstElement = sdesc.Buffer.FirstElement;
				global.srvs[i].numElements = sdesc.Buffer.NumElements;
			}
			else if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
			{
				global.srvs[i].firstElement = sdesc.BufferEx.FirstElement;
				global.srvs[i].numElements = sdesc.BufferEx.NumElements;
			}

			if(res)
			{
				if(WrappedID3D11Buffer::IsAlloc(res))
				{
					global.srvs[i].data = GetBufferData((ID3D11Buffer *)res, 0, 0);
				}
			}

			SAFE_RELEASE(res);
		}
	}
}
		
// struct that saves pointers as we iterate through to where we ultimately
// want to copy the data to
struct DataOutput
{
	DataOutput(int regster, int element, int numWords) { reg = regster; elem = element; numwords = numWords; }

	int reg;
	int elem;

	int numwords;
};

struct DebugHit
{
	uint32_t numHits;
	float posx; float posy;
	float depth;
	uint32_t primitive;
	uint32_t rawdata; // arbitrary, depending on shader
};

ShaderDebugTrace D3D11DebugManager::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	using namespace DXBC;
	using namespace ShaderDebug;

	ShaderDebugTrace empty;

	m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_WithoutDraw);
	
	ID3D11VertexShader *stateVS = NULL;
	m_WrappedContext->VSGetShader(&stateVS, NULL, NULL);

	WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

	SAFE_RELEASE(stateVS);

	if(!vs)
		return empty;

	DXBCFile *dxbc = vs->GetDXBC();

	if(!dxbc)
		return empty;

	D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();
	
	vector<D3D11_INPUT_ELEMENT_DESC> inputlayout = m_WrappedDevice->GetLayoutDesc(rs->IA.Layout);

	set<UINT> vertexbuffers;
	uint32_t trackingOffs[32] = {0};

	// need special handling for other step rates
	for(size_t i=0; i < inputlayout.size(); i++)
	{
		RDCASSERT(inputlayout[i].InstanceDataStepRate <= 1);

		UINT slot = RDCCLAMP(inputlayout[i].InputSlot, 0U, UINT(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT-1));

		vertexbuffers.insert(slot);

		if(inputlayout[i].AlignedByteOffset == ~0U)
		{
			inputlayout[i].AlignedByteOffset = trackingOffs[slot];
		}
		else
		{
			trackingOffs[slot] = inputlayout[i].AlignedByteOffset;
		}

		ResourceFormat fmt = MakeResourceFormat(inputlayout[i].Format);

		trackingOffs[slot] += fmt.compByteWidth * fmt.compCount;
	}

	vector<byte> vertData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	vector<byte> instData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	
	for(auto it=vertexbuffers.begin(); it != vertexbuffers.end(); ++it)
	{
		UINT i = *it;
		if(rs->IA.VBs[i])
		{
			vertData[i] = GetBufferData(rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i]*(vertOffset+idx), rs->IA.Strides[i]);
			instData[i] = GetBufferData(rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i]*(instOffset+instid), rs->IA.Strides[i]);
		}
	}

	vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		if(rs->VS.ConstantBuffers[i])
			cbufData[i] = GetBufferData(rs->VS.ConstantBuffers[i], rs->VS.CBOffsets[i]*sizeof(Vec4f), 0);

	ShaderDebugTrace ret;
	
	GlobalState global;
	CreateShaderGlobalState(global, 0, NULL, rs->VS.SRVs);
	State initialState = CreateShaderDebugState(ret, -1, dxbc, cbufData);

	for(int32_t i=0; i < ret.inputs.count; i++)
	{
		if(dxbc->m_InputSig[i].systemValue == eAttr_None ||
			dxbc->m_InputSig[i].systemValue == eAttr_Position) // SV_Position seems to get promoted automatically, but it's invalid for vertex input
		{
			const D3D11_INPUT_ELEMENT_DESC *el = NULL;

			string signame = strlower(string(dxbc->m_InputSig[i].semanticName.elems));

			for(size_t l=0; l < inputlayout.size(); l++)
			{
				string layoutname = strlower(string(inputlayout[l].SemanticName));

				if(signame == layoutname &&
					dxbc->m_InputSig[i].semanticIndex == inputlayout[l].SemanticIndex)
				{
					el = &inputlayout[l];
					break;
				}
				if(signame == layoutname + ToStr::Get(inputlayout[l].SemanticIndex))
				{
					el = &inputlayout[l];
					break;
				}
			}

			RDCASSERT(el);

			if(!el)
				continue;

			byte *srcData = NULL;
			size_t dataSize = 0;
			
			if(el->InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA)
			{
				if(vertData[el->InputSlot].size() >= el->AlignedByteOffset)
				{
					srcData = &vertData[el->InputSlot][el->AlignedByteOffset];
					dataSize = vertData[el->InputSlot].size()-el->AlignedByteOffset;
				}
			}
			else
			{
				if(instData[el->InputSlot].size() >= el->AlignedByteOffset)
				{
					srcData = &instData[el->InputSlot][el->AlignedByteOffset];
					dataSize = instData[el->InputSlot].size()-el->AlignedByteOffset;
				}
			}

			ResourceFormat fmt = MakeResourceFormat(el->Format);

			// more data needed than is provided
			if(dxbc->m_InputSig[i].compCount > fmt.compCount)
			{
				ret.inputs[i].value.u.w = 1;

				if(fmt.compType == eCompType_Float)
					ret.inputs[i].value.f.w = 1.0f;
			}

			// interpret special formats
			if(fmt.special)
			{
				Vec3f *v3 = (Vec3f *)ret.inputs[i].value.fv;
				Vec4f *v4 = (Vec4f *)ret.inputs[i].value.fv;

				// only pull in all or nothing from these,
				// if there's only e.g. 3 bytes remaining don't read and unpack some of
				// a 4-byte special format
				size_t packedsize = 4;
				if (fmt.specialFormat == eSpecial_B8G8R8A8 || fmt.specialFormat == eSpecial_B5G5R5A1 ||
					fmt.specialFormat == eSpecial_B5G6R5 || fmt.specialFormat == eSpecial_B4G4R4A4)
					packedsize = 2;

				if(srcData == NULL || packedsize > dataSize)
				{
					ret.inputs[i].value.u.x = 
						ret.inputs[i].value.u.y = 
						ret.inputs[i].value.u.z = 
						ret.inputs[i].value.u.w = 0;
				}
				else if (fmt.specialFormat == eSpecial_B8G8R8A8)
				{
					ret.inputs[i].value.f.x = float(srcData[2])/255.0f;
					ret.inputs[i].value.f.y = float(srcData[1])/255.0f;
					ret.inputs[i].value.f.z = float(srcData[0])/255.0f;
					ret.inputs[i].value.f.w = float(srcData[3])/255.0f;
				}
				else if (fmt.specialFormat == eSpecial_B5G5R5A1)
				{
					uint16_t packed = ((uint16_t *)srcData)[0];
					*v4 = ConvertFromB5G5R5A1(packed);
				}
				else if (fmt.specialFormat == eSpecial_B5G6R5)
				{
					uint16_t packed = ((uint16_t *)srcData)[0];
					*v3 = ConvertFromB5G6R5(packed);
				}
				else if (fmt.specialFormat == eSpecial_B4G4R4A4)
				{
					uint16_t packed = ((uint16_t *)srcData)[0];
					*v4 = ConvertFromB4G4R4A4(packed);
				}
				else if (fmt.specialFormat == eSpecial_R10G10B10A2)
				{
					uint32_t packed = ((uint32_t *)srcData)[0];

					if (fmt.compType == eCompType_UInt)
					{
						ret.inputs[i].value.u.z = (packed >> 0) & 0x3ff;
						ret.inputs[i].value.u.y = (packed >> 10) & 0x3ff;
						ret.inputs[i].value.u.x = (packed >> 20) & 0x3ff;
						ret.inputs[i].value.u.w = (packed >> 30) & 0x003;
					}
					else
					{
						*v4 = ConvertFromR10G10B10A2(packed);
					}
				}
				else if (fmt.special && fmt.specialFormat == eSpecial_R11G11B10)
				{
					uint32_t packed = ((uint32_t *)srcData)[0];
					*v3 = ConvertFromR11G11B10(packed);
				}
			}
			else
			{
				for(uint32_t c=0; c < fmt.compCount; c++)
				{
					if(srcData == NULL || fmt.compByteWidth > dataSize)
					{
						ret.inputs[i].value.uv[c] = 0;
						continue;
					}

					dataSize -= fmt.compByteWidth;

					if(fmt.compByteWidth == 1)
					{
						byte *src = srcData+c*fmt.compByteWidth;

						if(fmt.compType == eCompType_UInt)
							ret.inputs[i].value.uv[c] = *src;
						else if(fmt.compType == eCompType_SInt)
							ret.inputs[i].value.iv[c] = *((int8_t *)src);
						else if(fmt.compType == eCompType_UNorm)
							ret.inputs[i].value.fv[c] = float(*src)/255.0f;
						else if(fmt.compType == eCompType_SNorm)
						{
							signed char *schar = (signed char *)src;

							// -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
							if(*schar == -128)
								ret.inputs[i].value.fv[c] = -1.0f;
							else
								ret.inputs[i].value.fv[c] = float(*schar)/127.0f;
						}
						else
							RDCERR("Unexpected component type");
					}
					else if(fmt.compByteWidth == 2)
					{
						uint16_t *src = (uint16_t *)(srcData+c*fmt.compByteWidth);

						if(fmt.compType == eCompType_Float)
							ret.inputs[i].value.fv[c] = ConvertFromHalf(*src);
						else if(fmt.compType == eCompType_UInt)
							ret.inputs[i].value.uv[c] = *src;
						else if(fmt.compType == eCompType_SInt)
							ret.inputs[i].value.iv[c] = *((int16_t *)src);
						else if(fmt.compType == eCompType_UNorm)
							ret.inputs[i].value.fv[c] = float(*src)/float(UINT16_MAX);
						else if(fmt.compType == eCompType_SNorm)
						{
							int16_t *sint = (int16_t *)src;

							// -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
							if(*sint == -32768)
								ret.inputs[i].value.fv[c] = -1.0f;
							else
								ret.inputs[i].value.fv[c] = float(*sint)/32767.0f;
						}
						else
							RDCERR("Unexpected component type");
					}
					else if(fmt.compByteWidth == 4)
					{
						uint32_t *src = (uint32_t *)(srcData+c*fmt.compByteWidth);

						if(fmt.compType == eCompType_Float ||
							fmt.compType == eCompType_UInt ||
							fmt.compType == eCompType_SInt)
							memcpy(&ret.inputs[i].value.uv[c], src, 4);
						else
							RDCERR("Unexpected component type");
					}
				}
			}
		}
		else if(dxbc->m_InputSig[i].systemValue == eAttr_VertexIndex)
		{
			if(dxbc->m_InputSig[i].compType == eCompType_Float)
				ret.inputs[i].value.f.x = 
					ret.inputs[i].value.f.y = 
					ret.inputs[i].value.f.z = 
					ret.inputs[i].value.f.w = (float)vertid;
			else
				ret.inputs[i].value.u.x = 
					ret.inputs[i].value.u.y = 
					ret.inputs[i].value.u.z = 
					ret.inputs[i].value.u.w = vertid;
		}
		else if(dxbc->m_InputSig[i].systemValue == eAttr_InstanceIndex)
		{
			if(dxbc->m_InputSig[i].compType == eCompType_Float)
				ret.inputs[i].value.f.x = 
					ret.inputs[i].value.f.y = 
					ret.inputs[i].value.f.z = 
					ret.inputs[i].value.f.w = (float)instid;
			else
				ret.inputs[i].value.u.x = 
					ret.inputs[i].value.u.y = 
					ret.inputs[i].value.u.z = 
					ret.inputs[i].value.u.w = instid;
		}
		else
		{
			RDCERR("Unhandled system value semantic on VS input");
		}
	}

	State last;

	vector<ShaderDebugState> states;

	states.push_back((State)initialState);
	
	while(true)
	{
		if(initialState.Finished())
			break;

		initialState = initialState.GetNext(global, NULL);

		states.push_back((State)initialState);
	}

	ret.states = states;

	return ret;
}

ShaderDebugTrace D3D11DebugManager::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y)
{
	using namespace DXBC;
	using namespace ShaderDebug;

	ShaderDebugTrace empty;
	
	m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_WithoutDraw);

	ID3D11PixelShader *statePS = NULL;
	m_WrappedContext->PSGetShader(&statePS, NULL, NULL);

	WrappedID3D11Shader<ID3D11PixelShader> *ps = (WrappedID3D11Shader<ID3D11PixelShader> *)statePS;

	SAFE_RELEASE(statePS);

	if(!ps)
		return empty;
	
	D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();
	
	DXBCFile *dxbc = ps->GetDXBC();

	if(!dxbc)
		return empty;

	vector<DataOutput> initialValues;

	string extractHlsl = "struct PSInput\n{\n";

	int structureStride = 0;
	
	if(dxbc->m_InputSig.empty())
	{
		extractHlsl += "float4 input_dummy : SV_Position;\n";

		initialValues.push_back(DataOutput(-1, 0, 4));

		structureStride += 4;
	}

	vector<string> floatInputs;

	for(size_t i=0; i < dxbc->m_InputSig.size(); i++)
	{
		extractHlsl += "  ";
		if(dxbc->m_InputSig[i].compType == eCompType_Float)
			extractHlsl += "float";
		else if(dxbc->m_InputSig[i].compType == eCompType_SInt)
			extractHlsl += "int";
		else if(dxbc->m_InputSig[i].compType == eCompType_UInt)
			extractHlsl += "uint";
		else
			RDCERR("Unexpected input signature type: %d", dxbc->m_InputSig[i].compType);
		
		int numCols = 
			(dxbc->m_InputSig[i].regChannelMask & 0x1 ? 1 : 0) +
			(dxbc->m_InputSig[i].regChannelMask & 0x2 ? 1 : 0) +
			(dxbc->m_InputSig[i].regChannelMask & 0x4 ? 1 : 0) +
			(dxbc->m_InputSig[i].regChannelMask & 0x8 ? 1 : 0);

		structureStride += 4*numCols;

		string name = dxbc->m_InputSig[i].semanticIdxName.elems;
		
		extractHlsl += ToStr::Get((uint32_t)numCols) + " input_" + name + " : " + name;
		
		if(dxbc->m_InputSig[i].compType == eCompType_Float)
			floatInputs.push_back("input_" + name);

		extractHlsl += ";\n";
		
		int firstElem = 
			dxbc->m_InputSig[i].regChannelMask & 0x1 ? 0 :
			dxbc->m_InputSig[i].regChannelMask & 0x2 ? 1 :
			dxbc->m_InputSig[i].regChannelMask & 0x4 ? 2 :
			dxbc->m_InputSig[i].regChannelMask & 0x8 ? 3 :
			-1;

		initialValues.push_back(DataOutput(dxbc->m_InputSig[i].regIndex, firstElem, numCols));
	}

	extractHlsl += "};\n\n";

	uint32_t overdrawLevels = 100; // maximum number of overdraw levels

	extractHlsl += "struct PSInitialData { uint hit; float3 pos; uint prim; PSInput IN; float derivValid; PSInput INddx; PSInput INddy; };\n\n";
	extractHlsl += "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u0);\n\n";
	extractHlsl += "void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : SV_PrimitiveID)\n{\n";
	extractHlsl += "if(abs(debug_pixelPos.x - " + ToStr::Get(x) + ".5) < 2 && abs(debug_pixelPos.y - " + ToStr::Get(y) + ".5) < 2) {\n";
	extractHlsl += "uint idx = 0;\n";
	extractHlsl += "InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n";
	extractHlsl += "if(idx < " + ToStr::Get(overdrawLevels) + ") {\n";
	extractHlsl += "PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";
	extractHlsl += "PSInitialBuffer[idx].prim = prim;\n";
	extractHlsl += "PSInitialBuffer[idx].IN = IN;\n";
	extractHlsl += "PSInitialBuffer[idx].derivValid = ddx(debug_pixelPos.x);\n";
	extractHlsl += "PSInitialBuffer[idx].INddx = (PSInput)0;\n";
	extractHlsl += "PSInitialBuffer[idx].INddy = (PSInput)0;\n";
	for(size_t i=0; i < floatInputs.size(); i++)
	{
		const string &name = floatInputs[i];
		extractHlsl += "PSInitialBuffer[idx].INddx." + name + " = ddx(IN." + name + ");\n";
		extractHlsl += "PSInitialBuffer[idx].INddy." + name + " = ddy(IN." + name + ");\n";
	}
	extractHlsl += "}\n}\n}";

	ID3D11PixelShader *extract = MakePShader(extractHlsl.c_str(), "ExtractInputsPS", "ps_5_0");

							// uint hit;	    float3 pos;		 uint prim;        float derivValid;    PSInput IN, INddx, INddy;
	uint32_t structStride = sizeof(uint32_t) + sizeof(float)*3 + sizeof(uint32_t) + sizeof(float) + structureStride*3;

	HRESULT hr = S_OK;
	
	D3D11_BUFFER_DESC bdesc;
	bdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bdesc.CPUAccessFlags = 0;
	bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bdesc.Usage = D3D11_USAGE_DEFAULT;
	bdesc.StructureByteStride = structStride;
	bdesc.ByteWidth = bdesc.StructureByteStride * overdrawLevels;

	ID3D11Buffer *initialBuf = NULL;
	hr = m_pDevice->CreateBuffer(&bdesc, NULL, &initialBuf);

	if(FAILED(hr))
	{
		RDCERR("Failed to create buffer %08x", hr);
		return empty;
	}

	bdesc.BindFlags = 0;
	bdesc.MiscFlags = 0;
	bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	bdesc.Usage = D3D11_USAGE_STAGING;
	bdesc.StructureByteStride = 0;
	
	ID3D11Buffer *stageBuf = NULL;
	hr = m_pDevice->CreateBuffer(&bdesc, NULL, &stageBuf);

	if(FAILED(hr))
	{
		RDCERR("Failed to create buffer %08x", hr);
		return empty;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
	uavdesc.Format = DXGI_FORMAT_UNKNOWN;
	uavdesc.Buffer.FirstElement = 0;
	uavdesc.Buffer.Flags = 0;
	uavdesc.Buffer.NumElements = overdrawLevels;
	uavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	ID3D11UnorderedAccessView *initialUAV = NULL;
	hr = m_pDevice->CreateUnorderedAccessView(initialBuf, &uavdesc, &initialUAV);

	if(FAILED(hr))
	{
		RDCERR("Failed to create buffer %08x", hr);
		return empty;
	}

	UINT zero = 0;
	m_pImmediateContext->ClearUnorderedAccessViewUint(initialUAV, &zero);

	UINT count = (UINT)-1;
	ID3D11DepthStencilView *depthView = NULL;
	m_pImmediateContext->OMGetRenderTargets(0, NULL, &depthView);
	m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, depthView, 0, 1, &initialUAV, &count);
	m_pImmediateContext->PSSetShader(extract, NULL, 0);

	SAFE_RELEASE(depthView);
	
	m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

	m_pImmediateContext->CopyResource(stageBuf, initialBuf);

	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = m_pImmediateContext->Map(stageBuf, 0, D3D11_MAP_READ, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Failed to map stage buff %08x", hr);
		return empty;
	}

	byte *initialData = new byte[bdesc.ByteWidth];
	memcpy(initialData, mapped.pData, bdesc.ByteWidth);

	m_pImmediateContext->Unmap(stageBuf, 0);
	
	SAFE_RELEASE(initialUAV);
	SAFE_RELEASE(initialBuf);
	SAFE_RELEASE(stageBuf);

	SAFE_RELEASE(extract);

	DebugHit *buf = (DebugHit *)initialData;

	if(buf[0].numHits == 0)
	{
		RDCLOG("No hit for this event");
		return empty;
	}

	// if we encounter multiple hits at our destination pixel co-ord (or any other) we
	// really need to check depth state here, but that's difficult so skip it for now
	// we can iterate over the hits and get the depth of each from the second element
	// in each struct, but we also need the test depth AND need to be able to resolve
	// the depth test in the same way for each fragment.
	//
	// For now, just take the first. Later need to modify buf to point at the data of
	// the actual passing fragment.
	// also with alpha blending on we'd need to be able to pick the right one anyway.
	// so really here we just need to be able to get the depth result of each hit and
	// let the user choose, since multiple might pass & apply.


	// our debugging quad. Order is TL, TR, BL, BR
	State quad[4];

	// figure out the TL pixel's coords. Assume even top left (towards 0,0)
	int xTL = x&(~1);
	int yTL = y&(~1);

	// get the index of our desired pixel
	int destIdx = (x-xTL) + 2*(y-yTL);

	vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		if(rs->PS.ConstantBuffers[i])
			cbufData[i] = GetBufferData(rs->PS.ConstantBuffers[i], rs->PS.CBOffsets[i]*sizeof(Vec4f), 0);

	D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS;

	if(rs->OM.DepthStencilState)
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		rs->OM.DepthStencilState->GetDesc(&desc);
		depthFunc = desc.DepthFunc;
	}

	DebugHit *winner = NULL;

	for(size_t i=0; i < buf[0].numHits; i++)
	{
		DebugHit *hit = (DebugHit *)(initialData+i*structStride);

		// only interested in destination pixel
		if(hit->posx != (float)x + 0.5 || hit->posy != (float)y + 0.5)
			continue;

		if(winner == NULL || depthFunc == D3D11_COMPARISON_ALWAYS || depthFunc == D3D11_COMPARISON_NEVER ||
			depthFunc == D3D11_COMPARISON_NOT_EQUAL || depthFunc == D3D11_COMPARISON_EQUAL)
		{
			winner = hit;
			continue;
		}

		if(
			(depthFunc == D3D11_COMPARISON_LESS && hit->depth < winner->depth) ||
			(depthFunc == D3D11_COMPARISON_LESS_EQUAL && hit->depth <= winner->depth) ||
			(depthFunc == D3D11_COMPARISON_GREATER && hit->depth > winner->depth) ||
			(depthFunc == D3D11_COMPARISON_GREATER_EQUAL && hit->depth >= winner->depth)
		  )
		{
			winner = hit;
		}
	}

	if(winner == NULL)
	{
		RDCLOG("Couldn't find any pixels that passed depth test at target co-ordinates");
		return empty;
	}
	
	ShaderDebugTrace traces[4];
	
	GlobalState global;
	CreateShaderGlobalState(global, rs->OM.UAVStartSlot, rs->OM.UAVs, rs->PS.SRVs);

	{
		DebugHit *hit = winner;

		State initialState = CreateShaderDebugState(traces[destIdx], destIdx, dxbc, cbufData);

		uint32_t *data = &hit->rawdata;

		for(size_t i=0; i < initialValues.size(); i++)
		{
			int32_t *rawout = NULL;

			if(initialValues[i].reg >= 0)
			{
				rawout = &traces[destIdx].inputs[initialValues[i].reg].value.iv[initialValues[i].elem];

				memcpy(rawout, data, initialValues[i].numwords*4);
			}

			data += initialValues[i].numwords;
		}

		for(int i=0; i < 4; i++)
		{
			if(i != destIdx)
				traces[i] = traces[destIdx];
			quad[i] = initialState;
			quad[i].SetTrace(&traces[i]);
		}

		float *ddx = (float *)data;

		// ddx(SV_Position.x) MUST be 1.0
		if(*ddx != 1.0f)
		{
			RDCERR("Derivatives invalid");
			return empty;
		}

		ddx++;

		for(size_t i=0; i < initialValues.size(); i++)
		{
			if(initialValues[i].reg >= 0)
			{
				// left
				if(destIdx == 0 || destIdx == 2)
				{
					for(int w=0; w < initialValues[i].numwords; w++)
					{
						traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] += ddx[w];
						traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] += ddx[w];
					}
				}
				else
				{
					for(int w=0; w < initialValues[i].numwords; w++)
					{
						traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] -= ddx[w];
						traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] -= ddx[w];
					}
				}
			}

			ddx += initialValues[i].numwords;
		}

		float *ddy = ddx;

		for(size_t i=0; i < initialValues.size(); i++)
		{
			if(initialValues[i].reg >= 0)
			{
				// top
				if(destIdx == 0 || destIdx == 1)
				{
					for(int w=0; w < initialValues[i].numwords; w++)
					{
						traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] += ddy[w];
						traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] += ddy[w];
					}
				}
				else
				{
					for(int w=0; w < initialValues[i].numwords; w++)
					{
						traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] -= ddy[w];
						traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem+w] -= ddy[w];
					}
				}
			}

			ddy += initialValues[i].numwords;
		}
	}
	
	vector<ShaderDebugState> states;

	states.push_back((State)quad[destIdx]);
	
	// simulate lockstep until all threads are finished
	bool finished = true;
	do
	{
		for(size_t i = 0; i < 4; i++)
		{
			if(!quad[i].Finished())
				quad[i] = quad[i].GetNext(global, quad);
		}
		
		states.push_back((State)quad[destIdx]);

		finished = quad[destIdx].Finished();
	}
	while(!finished);

	traces[destIdx].states = states;

	return traces[destIdx];
}

ShaderDebugTrace D3D11DebugManager::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	using namespace DXBC;
	using namespace ShaderDebug;

	ShaderDebugTrace empty;
	
	m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_WithoutDraw);
	
	ID3D11ComputeShader *stateCS = NULL;
	m_WrappedContext->CSGetShader(&stateCS, NULL, NULL);

	WrappedID3D11Shader<ID3D11ComputeShader> *cs = (WrappedID3D11Shader<ID3D11ComputeShader> *)stateCS;

	SAFE_RELEASE(stateCS);

	if(!cs)
		return empty;

	DXBCFile *dxbc = cs->GetDXBC();

	if(!dxbc)
		return empty;

	D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();

	vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		if(rs->CS.ConstantBuffers[i])
			cbufData[i] = GetBufferData(rs->CS.ConstantBuffers[i], rs->CS.CBOffsets[i]*sizeof(Vec4f), 0);
	
	ShaderDebugTrace ret;
		
	GlobalState global;
	CreateShaderGlobalState(global, 0, rs->CS.UAVs, rs->CS.SRVs);
	State initialState = CreateShaderDebugState(ret, -1, dxbc, cbufData);
	
	for(int i=0; i < 3; i++)
	{
		initialState.semantics.GroupID[i] = groupid[i];
		initialState.semantics.ThreadID[i] = threadid[i];
	}

	vector<ShaderDebugState> states;

	states.push_back((State)initialState);
	
	while(true)
	{
		if(initialState.Finished())
			break;

		initialState = initialState.GetNext(global, NULL);

		states.push_back((State)initialState);
	}

	ret.states = states;

	return ret;
}

void D3D11DebugManager::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, float pixel[4])
{
	m_pImmediateContext->OMSetRenderTargets(1, &m_DebugRender.PickPixelRT, NULL);
	
	float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	
	m_pImmediateContext->ClearRenderTargetView(m_DebugRender.PickPixelRT, color);

	D3D11_VIEWPORT viewport;
	RDCEraseEl(viewport);

	int oldW = GetWidth(), oldH = GetHeight();

	SetOutputDimensions(100, 100);

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = 100;
	viewport.Height = 100;

	m_pImmediateContext->RSSetViewports(1, &viewport);

	{
		TextureDisplay texDisplay;

		texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
		texDisplay.HDRMul = -1.0f;
		texDisplay.linearDisplayAsGamma = true;
		texDisplay.mip = mip;
		texDisplay.CustomShader = ResourceId();
		texDisplay.sliceFace = sliceFace;
		texDisplay.rangemin = 0.0f;
		texDisplay.rangemax = 1.0f;
		texDisplay.scale = 1.0f;
		texDisplay.texid = texture;
		texDisplay.rawoutput = true;
		texDisplay.offx = -float(x);
		texDisplay.offy = -float(y);

		RenderTexture(texDisplay);
	}

	D3D11_BOX box;
	box.front = 0;
	box.back = 1;
	box.left = 0;
	box.right = 1;
	box.top = 0;
	box.bottom = 1;

	ID3D11Resource *res = NULL;
	m_DebugRender.PickPixelRT->GetResource(&res);

	m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickPixelStageTex, 0, 0, 0, 0, res, 0, &box);

	SAFE_RELEASE(res);

	D3D11_MAPPED_SUBRESOURCE mapped;
	mapped.pData = NULL;
	HRESULT hr = m_pImmediateContext->Map(m_DebugRender.PickPixelStageTex, 0, D3D11_MAP_READ, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Failed to map stage buff %08x", hr);
	}

	float *pix = (float *)mapped.pData;

	if(pix == NULL)
	{
		RDCERR("Failed to map pick-pixel staging texture.");
	}
	else
	{
		pixel[0] = pix[0];
		pixel[1] = pix[1];
		pixel[2] = pix[2];
		pixel[3] = pix[3];
	}

	SetOutputDimensions(oldW, oldH);

	m_pImmediateContext->Unmap(m_DebugRender.PickPixelStageTex, 0);
}

// from MSDN
struct DDS_PIXELFORMAT {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwFourCC;
  DWORD dwRGBBitCount;
  DWORD dwRBitMask;
  DWORD dwGBitMask;
  DWORD dwBBitMask;
  DWORD dwABitMask;
};

struct DDS_HEADER {
  DWORD           dwSize;
  DWORD           dwFlags;
  DWORD           dwHeight;
  DWORD           dwWidth;
  DWORD           dwPitchOrLinearSize;
  DWORD           dwDepth;
  DWORD           dwMipMapCount;
  DWORD           dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  DWORD           dwCaps;
  DWORD           dwCaps2;
  DWORD           dwCaps3;
  DWORD           dwCaps4;
  DWORD           dwReserved2;
};

struct DDS_HEADER_DXT10 {
  DXGI_FORMAT              dxgiFormat;
  D3D10_RESOURCE_DIMENSION resourceDimension;
  UINT                     miscFlag;
  UINT                     arraySize;
  UINT                     reserved;
};

#define DDSD_CAPS			0x1
#define DDSD_HEIGHT			0x2
#define DDSD_WIDTH			0x4
#define DDSD_PITCH			0x8
#define DDSD_PIXELFORMAT	0x1000
#define DDSD_MIPMAPCOUNT	0x20000
#define DDSD_LINEARSIZE		0x80000
#define DDSD_DEPTH			0x800000

#define DDSCAPS_COMPLEX		0x8
#define DDSCAPS_MIPMAP		0x400000
#define DDSCAPS_TEXTURE		0x1000

#define DDSCAPS2_CUBEMAP	0xff00 // d3d10+ requires all cubemap faces
#define DDSCAPS2_VOLUME		0x200000

#define DDPF_ALPHAPIXELS	0x1
#define DDPF_ALPHA			0x2
#define DDPF_FOURCC			0x4
#define DDPF_RGB			0x40
#define DDPF_YUV			0x200
#define DDPF_LUMINANCE		0x20000
#define DDPF_RGBA			(DDPF_RGB|DDPF_ALPHAPIXELS)

bool D3D11DebugManager::SaveTexture(ResourceId id, uint32_t saveMip, wstring path)
{
	WrappedID3D11Texture2D *wrapTex = (WrappedID3D11Texture2D *)WrappedID3D11Texture2D::m_TextureList[id].m_Texture;

	if(path.find(L".dds") != wstring::npos)
	{
		D3D11_TEXTURE2D_DESC desc = {0};
		wrapTex->GetDesc(&desc);

		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;

		ID3D11Texture2D *dummyTex = NULL;

		HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &dummyTex);

		if(FAILED(hr))
		{
			RDCERR("Couldn't create staging texture to save. %08x", hr);
			return false;
		}

		m_pImmediateContext->CopyResource(dummyTex, wrapTex->GetReal());

		DWORD magic = 0x20534444;
		DDS_HEADER header;
		DDS_HEADER_DXT10 headerDXT10;
		RDCEraseEl(header);
		RDCEraseEl(headerDXT10);

		header.dwSize = sizeof(DDS_HEADER);

		header.ddspf.dwSize = sizeof(DDS_PIXELFORMAT);

		header.dwWidth = desc.Width;
		header.dwHeight = desc.Height;
		header.dwDepth = 0;
		header.dwMipMapCount = desc.MipLevels;

		header.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		if(desc.MipLevels > 1)
			header.dwFlags |= DDSD_MIPMAPCOUNT;
		if(IsDepthFormat(desc.Format))
			header.dwFlags |= DDSD_DEPTH;
		if(IsBlockFormat(desc.Format))
			header.dwFlags |= DDSD_LINEARSIZE;
		else
			header.dwFlags |= DDSD_PITCH;

		header.dwCaps = DDSCAPS_TEXTURE;
		/*
		// spec compliant, but seems to confuse DirectX Texture Tool :(
		if(desc.MipLevels > 1)
			header.dwCaps |= DDSCAPS_MIPMAP;
		if(desc.MipLevels > 1 || desc.ArraySize > 1)
			header.dwCaps |= DDSCAPS_COMPLEX;
		*/
		if(desc.ArraySize > 1)
			header.dwCaps |= DDSCAPS_COMPLEX;

		header.dwCaps2 = desc.ArraySize > 1 ? DDSCAPS2_VOLUME : 0;

		headerDXT10.dxgiFormat = GetTypedFormat(desc.Format);
		headerDXT10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
		headerDXT10.arraySize = desc.ArraySize;

		if(desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
		{
			header.dwCaps2 = DDSCAPS2_CUBEMAP;
			headerDXT10.arraySize /= 6;
		}

		if(IsBlockFormat(desc.Format))
		{
			int blockSize = GetFormatBPP(desc.Format) / 8;
			header.dwPitchOrLinearSize = RDCMAX(1U, ((desc.Width+3)/4)) * blockSize;
		}
		else if(desc.Format == DXGI_FORMAT_R8G8_B8G8_UNORM || 
				desc.Format == DXGI_FORMAT_G8R8_G8B8_UNORM)
		{
			header.dwPitchOrLinearSize = ((desc.Width+1) >> 1) * 4;
		}
		else
		{
			header.dwPitchOrLinearSize = (desc.Width * GetFormatBPP(desc.Format) + 7) / 8;
		}


		bool dx10Header = false;

		// special case a couple of formats to write out non-DX10 style, for
		// backwards compatibility
		switch(desc.Format)
		{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
			{
				header.ddspf.dwFlags = DDPF_RGBA;
				header.ddspf.dwRGBBitCount = 32;
				header.ddspf.dwRBitMask = 0x000000ff;
				header.ddspf.dwGBitMask = 0x0000ff00;
				header.ddspf.dwBBitMask = 0x00ff0000;
				header.ddspf.dwABitMask = 0xff000000;
				break;
			}
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '1');
				break;
			}
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '3');
				break;
			}
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '5');
				break;
			}
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '4', 'U');
				break;
			}
			case DXGI_FORMAT_BC4_SNORM:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '4', 'S');
				break;
			}
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('A', 'T', 'I', '2');
				break;
			}
			case DXGI_FORMAT_BC5_SNORM:
			{
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '5', 'S');
				break;
			}
			default:
			{
				// just write out DX10 header
				header.ddspf.dwFlags = DDPF_FOURCC;
				header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', '1', '0');

				dx10Header = true;
				break;
			}
		}
		
		FILE *f = FileIO::fopen(path.c_str(), L"wb");

		if(f)
		{
			FileIO::fwrite(&magic, sizeof(magic), 1, f);
			FileIO::fwrite(&header, sizeof(header), 1, f);
			if(dx10Header)
				FileIO::fwrite(&headerDXT10, sizeof(headerDXT10), 1, f);

			UINT i=0;
			for(UINT slice=0; slice < RDCMAX(1U,desc.ArraySize); slice++)
			{
				for(UINT mip=0; mip < RDCMAX(1U,desc.MipLevels); mip++)
				{
					D3D11_MAPPED_SUBRESOURCE mapped;
					hr = m_pImmediateContext->Map(dummyTex, i, D3D11_MAP_READ, 0, &mapped);

					if(FAILED(hr))
					{
						RDCERR("Couldn't map subresource. %08x", hr);
						FileIO::fclose(f);
						return false;
					}
					
					byte *data = (byte *)mapped.pData;

					UINT numRows = (desc.Height>>mip);
					UINT pitch = (header.dwPitchOrLinearSize>>mip);

					// pitch/rows are in blocks, not pixels, for block formats.
					if(IsBlockFormat(desc.Format))
					{
						numRows = RDCMAX(1U, numRows/4);
						// at least one block
						pitch = RDCMAX(pitch, GetFormatBPP(desc.Format)/8);
					}

					for(UINT row=0; row < numRows; row++)
					{
						FileIO::fwrite(data, 1, pitch, f);

						data += mapped.RowPitch;
					}
					
					m_pImmediateContext->Unmap(dummyTex, i);

					i++;
				}
			}
		}

		FileIO::fclose(f);

		SAFE_RELEASE(dummyTex);

		return true;
	}

	RDCERR("Unknown file-type");

	return false;
}

byte *D3D11DebugManager::GetTextureData(ResourceId id, uint32_t arrayIdx, uint32_t mip, size_t &dataSize)
{
	ID3D11Resource *dummyTex = NULL;

	uint32_t subresource = 0;
	uint32_t mips = 0;
	
	dataSize = 0;
	size_t bytesize = 0;

	if(WrappedID3D11Texture1D::m_TextureList.find(id) != WrappedID3D11Texture1D::m_TextureList.end())
	{
		WrappedID3D11Texture1D *wrapTex = (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[id].m_Texture;

		D3D11_TEXTURE1D_DESC desc = {0};
		wrapTex->GetDesc(&desc);

		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;

		ID3D11Texture1D *d = NULL;
		
		mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);

		if(mip >= mips || arrayIdx >= desc.ArraySize) return NULL;

		subresource = arrayIdx*mips + mip;

		HRESULT hr = m_WrappedDevice->CreateTexture1D(&desc, NULL, &d);

		dummyTex = d;

		if(FAILED(hr))
		{
			RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
			return NULL;
		}
		
		bytesize = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

		m_pImmediateContext->CopyResource(UNWRAP(WrappedID3D11Texture1D, d), wrapTex->GetReal());
	}
	else if(WrappedID3D11Texture2D::m_TextureList.find(id) != WrappedID3D11Texture2D::m_TextureList.end())
	{
		WrappedID3D11Texture2D *wrapTex = (WrappedID3D11Texture2D *)WrappedID3D11Texture2D::m_TextureList[id].m_Texture;

		D3D11_TEXTURE2D_DESC desc = {0};
		wrapTex->GetDesc(&desc);

		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;

		ID3D11Texture2D *d = NULL;
		
		mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

		if(mip >= mips || arrayIdx >= desc.ArraySize) return NULL;

		subresource = arrayIdx*mips + mip;

		HRESULT hr = m_WrappedDevice->CreateTexture2D(&desc, NULL, &d);

		dummyTex = d;

		if(FAILED(hr))
		{
			RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
			return NULL;
		}
		
		bytesize = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

		m_pImmediateContext->CopyResource(UNWRAP(WrappedID3D11Texture2D, d), wrapTex->GetReal());
	}
	else if(WrappedID3D11Texture3D::m_TextureList.find(id) != WrappedID3D11Texture3D::m_TextureList.end())
	{
		WrappedID3D11Texture3D *wrapTex = (WrappedID3D11Texture3D *)WrappedID3D11Texture3D::m_TextureList[id].m_Texture;

		D3D11_TEXTURE3D_DESC desc = {0};
		wrapTex->GetDesc(&desc);

		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;

		ID3D11Texture3D *d = NULL;
		
		mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);

		if(mip >= mips) return NULL;

		subresource = mip;

		HRESULT hr = m_WrappedDevice->CreateTexture3D(&desc, NULL, &d);

		dummyTex = d;

		if(FAILED(hr))
		{
			RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
			return NULL;
		}
		
		bytesize = GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip);

		m_pImmediateContext->CopyResource(UNWRAP(WrappedID3D11Texture3D, d), wrapTex->GetReal());
	}

	MapIntercept intercept;
	
	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	HRESULT hr = m_pImmediateContext->Map(m_ResourceManager->UnwrapResource(dummyTex), subresource, D3D11_MAP_READ, 0, &mapped);

	byte *ret = NULL;

	if(SUCCEEDED(hr))
	{
		ret = new byte[bytesize];
		dataSize = bytesize;
		intercept.InitWrappedResource(dummyTex, subresource, ret);
		intercept.SetD3D(mapped);
		intercept.CopyFromD3D();
	}
	else
	{
		RDCERR("Couldn't map staging texture to retrieve data. %08x", hr);
	}

	SAFE_RELEASE(dummyTex);

	return ret;
}

void D3D11DebugManager::FillTimers(uint32_t frameID, uint32_t &eventStart, rdctype::array<FetchDrawcall> &draws, vector<GPUTimer> &timers, int &reuseIdx)
{
	const D3D11_QUERY_DESC qdesc = { D3D11_QUERY_TIMESTAMP, 0 };

	if(draws.count == 0) return;

	for(int32_t i=0; i < draws.count; i++)
	{
		FetchDrawcall &d = draws[i];
		FillTimers(frameID, eventStart, d.children, timers, reuseIdx);

		if(d.events.count == 0) continue;

		GPUTimer *timer = NULL;

		if(reuseIdx == -1)
		{
			timers.push_back(GPUTimer());

			timer = &timers.back();
			timer->drawcall = &d;
		}
		else
		{
			timer = &timers[reuseIdx++];
		}
		
		HRESULT hr = S_OK;
		
		if(reuseIdx == -1)
		{
			hr = m_pDevice->CreateQuery(&qdesc, &timer->before);
			RDCASSERT(SUCCEEDED(hr));
			hr = m_pDevice->CreateQuery(&qdesc, &timer->after);
			RDCASSERT(SUCCEEDED(hr));
		}

		m_WrappedDevice->ReplayLog(frameID, eventStart, d.eventID, eReplay_WithoutDraw);

		m_pImmediateContext->Flush();
		
		m_pImmediateContext->End(timer->before);
		m_WrappedDevice->ReplayLog(frameID, eventStart, d.eventID, eReplay_OnlyDraw);
		m_pImmediateContext->End(timer->after);
		
		eventStart = d.eventID+1;
	}
}

void D3D11DebugManager::TimeDrawcalls(rdctype::array<FetchDrawcall> &arr)
{
	SCOPED_TIMER("Drawcall timing");

	vector<GPUTimer> timers;

	D3D11_QUERY_DESC disjointdesc = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
	ID3D11Query *disjoint = NULL;

	D3D11_QUERY_DESC qdesc = { D3D11_QUERY_TIMESTAMP, 0 };
	ID3D11Query *start = NULL;

	HRESULT hr = S_OK;

	hr = m_pDevice->CreateQuery(&disjointdesc, &disjoint);
	if(FAILED(hr))
	{
		RDCERR("Failed to create disjoint query %08x", hr);
		return;
	}

	hr = m_pDevice->CreateQuery(&qdesc, &start);
	if(FAILED(hr))
	{
		RDCERR("Failed to create start query %08x", hr);
		return;
	}

	for(int loop=0; loop < 1; loop++)
	{
		{
			m_pImmediateContext->Begin(disjoint);

			m_pImmediateContext->End(start);

			uint32_t ev = 0;
			int reuse = loop == 0 ? -1 : 0;
			FillTimers(0, ev, arr, timers, reuse);

			m_pImmediateContext->End(disjoint);
		}

		{
			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
			do
			{
				hr = m_pImmediateContext->GetData(disjoint, &disjointData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
			} while(hr == S_FALSE);
			RDCASSERT(hr == S_OK);

			RDCASSERT(!disjointData.Disjoint);

			double ticksToSecs = double(disjointData.Frequency);

			UINT64 a=0;
			m_pImmediateContext->GetData(start, &a, sizeof(UINT64), 0);

			for(size_t i=0; i < timers.size(); i++)
			{
				hr = m_pImmediateContext->GetData(timers[i].before, &a, sizeof(UINT64), 0);
				RDCASSERT(hr == S_OK);

				UINT64 b=0;
				hr = m_pImmediateContext->GetData(timers[i].after, &b, sizeof(UINT64), 0);
				RDCASSERT(hr == S_OK);

				timers[i].drawcall->duration = (double(b-a)/ticksToSecs);

				a = b;
			}
		}
	}

	for(size_t i=0; i < timers.size(); i++)
	{
		SAFE_RELEASE(timers[i].before);
		SAFE_RELEASE(timers[i].after);
	}

	SAFE_RELEASE(disjoint);
	SAFE_RELEASE(start);
}

ResourceId D3D11DebugManager::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	TextureShaderDetails details = GetShaderDetails(texid, false);

	CreateCustomShaderTex(details.texWidth, details.texHeight);

	m_pImmediateContext->OMSetRenderTargets(1, &m_CustomShaderRTV, NULL);

	float clr[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pImmediateContext->ClearRenderTargetView(m_CustomShaderRTV, clr);

	D3D11_VIEWPORT viewport;
	RDCEraseEl(viewport);

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)details.texWidth;
	viewport.Height = (float)details.texHeight;

	m_pImmediateContext->RSSetViewports(1, &viewport);

	TextureDisplay disp;
	disp.Red = disp.Green = disp.Blue = disp.Alpha = true;
	disp.offx = 0.0f;
	disp.offy = 0.0f;
	disp.CustomShader = shader;
	disp.texid = texid;
	disp.lightBackgroundColour = disp.darkBackgroundColour = FloatVector(0,0,0,0);
	disp.HDRMul = -1.0f;
	disp.linearDisplayAsGamma = true;
	disp.mip = mip;
	disp.overlay = eTexOverlay_None;
	disp.rangemin = 0.0f;
	disp.rangemax = 1.0f;
	disp.rawoutput = false;
	disp.scale = 1.0f;
	disp.sliceFace = 0;

	SetOutputDimensions(details.texWidth, details.texHeight);

	RenderTexture(disp);

	return m_CustomShaderResourceId;
}

void D3D11DebugManager::CreateCustomShaderTex(uint32_t w, uint32_t h)
{
	D3D11_TEXTURE2D_DESC texdesc;

	texdesc.ArraySize = 1;
	texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texdesc.CPUAccessFlags = 0;
	texdesc.MipLevels = 1;
	texdesc.MiscFlags = 0;
	texdesc.SampleDesc.Count = 1;
	texdesc.SampleDesc.Quality = 0;
	texdesc.Usage = D3D11_USAGE_DEFAULT;
	texdesc.Width = w;
	texdesc.Height = h;
	texdesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	if(m_CustomShaderTex)
	{
		D3D11_TEXTURE2D_DESC customTexDesc;
		m_CustomShaderTex->GetDesc(&customTexDesc);

		if(customTexDesc.Width == w && customTexDesc.Height == h)
			return;
		
		SAFE_RELEASE(m_CustomShaderRTV);
		SAFE_RELEASE(m_CustomShaderTex);
	}

	HRESULT hr = m_WrappedDevice->CreateTexture2D(&texdesc, NULL, &m_CustomShaderTex);

	if(FAILED(hr))
	{
		RDCERR("Failed to create custom shader tex %08x", hr);
	}
	else
	{
		WrappedID3D11Texture2D *wrapped = (WrappedID3D11Texture2D *)m_CustomShaderTex;
		hr = m_pDevice->CreateRenderTargetView(wrapped->GetReal(), NULL, &m_CustomShaderRTV);
		
		if(FAILED(hr))
			RDCERR("Failed to create custom shader rtv %08x", hr);

		m_CustomShaderResourceId = GetIDForResource(m_CustomShaderTex);
	}
}

ResourceId D3D11DebugManager::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID)
{
	TextureShaderDetails details = GetShaderDetails(texid, false);

	ResourceId id = texid;

	D3D11_TEXTURE2D_DESC realTexDesc;
	realTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
	realTexDesc.Usage = D3D11_USAGE_DEFAULT;
	realTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	realTexDesc.ArraySize = 1;
	realTexDesc.MipLevels = 1;
	realTexDesc.CPUAccessFlags = 0;
	realTexDesc.MiscFlags = 0;
	realTexDesc.SampleDesc.Count = 1;
	realTexDesc.SampleDesc.Quality = 0;
	realTexDesc.Width = details.texWidth;
	realTexDesc.Height = details.texHeight;

	if(details.texType == eTexType_2D)
	{
		realTexDesc.SampleDesc.Count = details.sampleCount;
		realTexDesc.SampleDesc.Quality = details.sampleQuality;
	}

	D3D11RenderState old = *m_WrappedContext->GetCurrentPipelineState();

	D3D11_TEXTURE2D_DESC customTexDesc;
	RDCEraseEl(customTexDesc);
	if(m_OverlayRenderTex)
		m_OverlayRenderTex->GetDesc(&customTexDesc);

	WrappedID3D11Texture2D *wrappedCustomRenderTex = (WrappedID3D11Texture2D *)m_OverlayRenderTex;

	// need to recreate backing custom render tex
	if(realTexDesc.Width != customTexDesc.Width ||
		realTexDesc.Height != customTexDesc.Height ||
		realTexDesc.Format != customTexDesc.Format ||
		realTexDesc.SampleDesc.Count != customTexDesc.SampleDesc.Count ||
		realTexDesc.SampleDesc.Quality != customTexDesc.SampleDesc.Quality)
	{
		SAFE_RELEASE(m_OverlayRenderTex);
		m_OverlayResourceId = ResourceId();

		ID3D11Texture2D *customRenderTex = NULL;
		HRESULT hr = m_WrappedDevice->CreateTexture2D(&realTexDesc, NULL, &customRenderTex);
		if(FAILED(hr))
		{
			RDCERR("Failed to create custom render tex %08x", hr);
			return ResourceId();
		}
		wrappedCustomRenderTex = (WrappedID3D11Texture2D *)customRenderTex;

		m_OverlayRenderTex = wrappedCustomRenderTex;
		m_OverlayResourceId = wrappedCustomRenderTex->GetResourceID();
	}

	ID3D11Texture2D *preDrawDepth = NULL;
	ID3D11Texture2D *renderDepth = NULL;

	ID3D11DepthStencilView *dsView = NULL;

	m_pImmediateContext->OMGetRenderTargets(0, NULL, &dsView);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsViewDesc;
	RDCEraseEl(dsViewDesc);
	if(dsView)
	{
		ID3D11Texture2D *realDepth = NULL;

		dsView->GetResource((ID3D11Resource **)&realDepth);

		dsView->GetDesc(&dsViewDesc);

		SAFE_RELEASE(dsView);

		D3D11_TEXTURE2D_DESC desc;

		realDepth->GetDesc(&desc);

		HRESULT hr = S_OK;

		hr = m_pDevice->CreateTexture2D(&desc, NULL, &preDrawDepth);
		if(FAILED(hr))
		{
			RDCERR("Failed to create preDrawDepth %08x", hr);
			SAFE_RELEASE(realDepth);
			return m_OverlayResourceId;
		}
		hr = m_pDevice->CreateTexture2D(&desc, NULL, &renderDepth);
		if(FAILED(hr))
		{
			RDCERR("Failed to create renderDepth %08x", hr);
			SAFE_RELEASE(realDepth);
			return m_OverlayResourceId;
		}

		m_pImmediateContext->CopyResource(preDrawDepth, realDepth);

		SAFE_RELEASE(realDepth);
	}

	D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
	rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDesc.Texture2D.MipSlice = 0;

	if(realTexDesc.SampleDesc.Count > 1 ||
		realTexDesc.SampleDesc.Quality > 0)
	{
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}

	ID3D11RenderTargetView *rtv = NULL;
	HRESULT hr = m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex->GetReal(), &rtDesc, &rtv);
	if(FAILED(hr))
	{
		RDCERR("Failed to create custom render tex RTV %08x", hr);
		return m_OverlayResourceId;
	}

	FLOAT black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pImmediateContext->ClearRenderTargetView(rtv, black);

	if(renderDepth)
	{
		m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);	

		hr = m_pDevice->CreateDepthStencilView(renderDepth, &dsViewDesc, &dsView);
		if(FAILED(hr))
		{
			RDCERR("Failed to create renderDepth DSV %08x", hr);
			return m_OverlayResourceId;
		}
	}

	m_pImmediateContext->OMSetRenderTargets(1, &rtv, dsView);

	SAFE_RELEASE(dsView);

	D3D11_DEPTH_STENCIL_DESC desc;

	desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.DepthEnable = TRUE;
	desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = desc.StencilWriteMask = 0xff;

	if(overlay == eTexOverlay_NaN ||
		overlay == eTexOverlay_Clipping)
	{
		// just need the basic texture
	}
	else if(overlay == eTexOverlay_Drawcall)
	{
		m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

		desc.DepthEnable = FALSE;
		desc.StencilEnable = FALSE;

		ID3D11DepthStencilState *os = NULL;
		hr = m_pDevice->CreateDepthStencilState(&desc, &os);
		if(FAILED(hr))
		{
			RDCERR("Failed to create drawcall depth stencil state %08x", hr);
			return m_OverlayResourceId;
		}

		m_pImmediateContext->OMSetDepthStencilState(os, 0);

		m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

		ID3D11RasterizerState *rs = NULL;
		{
			D3D11_RASTERIZER_DESC desc;

			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_NONE;
			desc.FrontCounterClockwise = FALSE;
			desc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
			desc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
			desc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			desc.DepthClipEnable = FALSE;
			desc.ScissorEnable = FALSE;
			desc.MultisampleEnable = FALSE;
			desc.AntialiasedLineEnable = FALSE;

			hr = m_pDevice->CreateRasterizerState(&desc, &rs);
			if(FAILED(hr))
			{
				RDCERR("Failed to create drawcall rast state %08x", hr);
				return m_OverlayResourceId;
			}
		}

		float clearColour[] = { 0.0f, 0.0f, 0.0f, 0.5f };
		m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);
		
		float overlayConsts[] = { 0.8f, 0.1f, 0.8f, 1.0f };
		ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

		m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

		m_pImmediateContext->RSSetState(rs);

		m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		SAFE_RELEASE(os);
		SAFE_RELEASE(rs);
	}
	else if(overlay == eTexOverlay_ViewportScissor)
	{
		m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
		m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

		desc.DepthEnable = FALSE;
		desc.StencilEnable = FALSE;

		ID3D11DepthStencilState *os = NULL;
		hr = m_pDevice->CreateDepthStencilState(&desc, &os);
		if(FAILED(hr))
		{
			RDCERR("Failed to create drawcall depth stencil state %08x", hr);
			return m_OverlayResourceId;
		}

		m_pImmediateContext->OMSetDepthStencilState(os, 0);

		m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

		ID3D11RasterizerState *rs = NULL;
		ID3D11RasterizerState *rs2 = NULL;
		{
			D3D11_RASTERIZER_DESC desc;

			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_NONE;
			desc.FrontCounterClockwise = FALSE;
			desc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
			desc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
			desc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			desc.DepthClipEnable = FALSE;
			desc.ScissorEnable = FALSE;
			desc.MultisampleEnable = FALSE;
			desc.AntialiasedLineEnable = FALSE;

			hr = m_pDevice->CreateRasterizerState(&desc, &rs);
			if(FAILED(hr))
			{
				RDCERR("Failed to create drawcall rast state %08x", hr);
				return m_OverlayResourceId;
			}
			
			desc.ScissorEnable = TRUE;

			hr = m_pDevice->CreateRasterizerState(&desc, &rs2);
			if(FAILED(hr))
			{
				RDCERR("Failed to create drawcall rast state %08x", hr);
				return m_OverlayResourceId;
			}
		}

		float clearColour[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);
		
		float overlayConsts[] = { 0.15f, 0.3f, 0.6f, 0.3f };
		ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

		m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

		m_pImmediateContext->RSSetState(rs);

		m_pImmediateContext->Draw(3, 0);
		
		float overlayConsts2[] = { 0.5f, 0.6f, 0.8f, 0.3f };
		buf = MakeCBuffer(overlayConsts2, sizeof(overlayConsts2));

		m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

		m_pImmediateContext->RSSetState(rs2);

		m_pImmediateContext->Draw(3, 0);

		SAFE_RELEASE(os);
		SAFE_RELEASE(rs);
		SAFE_RELEASE(rs2);
	}
	else if(overlay == eTexOverlay_Wireframe)
	{
		m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

		desc.DepthEnable = FALSE;

		ID3D11DepthStencilState *os = NULL;
		hr = m_pDevice->CreateDepthStencilState(&desc, &os);
		if(FAILED(hr))
		{
			RDCERR("Failed to create wireframe depth state %08x", hr);
			return m_OverlayResourceId;
		}

		m_pImmediateContext->OMSetDepthStencilState(os, 0);

		m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

		ID3D11RasterizerState *rs = NULL;
		{
			D3D11_RASTERIZER_DESC desc;

			m_pImmediateContext->RSGetState(&rs);

			if(rs)
			{
				rs->GetDesc(&desc);
			}
			else
			{
				desc.FillMode = D3D11_FILL_SOLID;
				desc.CullMode = D3D11_CULL_BACK;
				desc.FrontCounterClockwise = FALSE;
				desc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
				desc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
				desc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
				desc.DepthClipEnable = TRUE;
				desc.ScissorEnable = FALSE;
				desc.MultisampleEnable = FALSE;
				desc.AntialiasedLineEnable = FALSE;
			}

			SAFE_RELEASE(rs);

			desc.FillMode = D3D11_FILL_WIREFRAME;
			desc.CullMode = D3D11_CULL_NONE;

			hr = m_pDevice->CreateRasterizerState(&desc, &rs);
			if(FAILED(hr))
			{
				RDCERR("Failed to create wireframe rast state %08x", hr);
				return m_OverlayResourceId;
			}
		}

		float overlayConsts[] = { 200.0f/255.0f, 255.0f/255.0f, 0.0f/255.0f, 0.0f };
		m_pImmediateContext->ClearRenderTargetView(rtv, overlayConsts);

		overlayConsts[3] = 1.0f;
		ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

		m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

		m_pImmediateContext->RSSetState(rs);

		m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		SAFE_RELEASE(os);
		SAFE_RELEASE(rs);
	}
	else if(preDrawDepth)
	{
		D3D11_DEPTH_STENCIL_DESC cur = {0};

		UINT stencilRef = 0;

		{
			ID3D11DepthStencilState *os = NULL;
			m_pImmediateContext->OMGetDepthStencilState(&os, &stencilRef);

			if(os)
			{
				os->GetDesc(&cur);
				SAFE_RELEASE(os);
			}
			else
			{
				cur.DepthFunc = D3D11_COMPARISON_LESS; // default depth func
				cur.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
				cur.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
				cur.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
				cur.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
				cur.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
				cur.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
				cur.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
				cur.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			}
		}

		if(overlay == eTexOverlay_DepthBoth ||
			overlay == eTexOverlay_StencilBoth)
		{
			ID3D11DepthStencilState *os = NULL;

			D3D11_DEPTH_STENCIL_DESC d = desc;

			if(overlay == eTexOverlay_DepthBoth)
			{
				desc.DepthEnable = d.DepthEnable = TRUE;
				desc.StencilEnable = d.StencilEnable = FALSE;

				switch(cur.DepthFunc)
				{
				case D3D11_COMPARISON_ALWAYS:
					d.DepthFunc = D3D11_COMPARISON_NEVER;
					break;
				case D3D11_COMPARISON_NEVER:
					d.DepthFunc = D3D11_COMPARISON_ALWAYS;
					break;

				case D3D11_COMPARISON_EQUAL:
					d.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
					break;
				case D3D11_COMPARISON_NOT_EQUAL:
					d.DepthFunc = D3D11_COMPARISON_EQUAL;
					break;

				case D3D11_COMPARISON_LESS:
					d.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
					break;
				case D3D11_COMPARISON_GREATER_EQUAL:
					d.DepthFunc = D3D11_COMPARISON_LESS;
					break;

				case D3D11_COMPARISON_GREATER:
					d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
					break;
				case D3D11_COMPARISON_LESS_EQUAL:
					d.DepthFunc = D3D11_COMPARISON_GREATER;
					break;
				}
			}
			else if(overlay == eTexOverlay_StencilBoth)
			{
				desc.DepthEnable = d.DepthEnable = FALSE;
				desc.StencilEnable = d.StencilEnable = TRUE;

				d.FrontFace = cur.FrontFace;
				d.BackFace = cur.BackFace;
				desc.StencilReadMask = d.StencilReadMask = cur.StencilReadMask;
				desc.StencilWriteMask = d.StencilWriteMask = cur.StencilWriteMask;

				switch(cur.FrontFace.StencilFunc)
				{
				case D3D11_COMPARISON_ALWAYS:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
					break;
				case D3D11_COMPARISON_NEVER:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
					break;

				case D3D11_COMPARISON_EQUAL:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
					break;
				case D3D11_COMPARISON_NOT_EQUAL:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
					break;

				case D3D11_COMPARISON_LESS:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
					break;
				case D3D11_COMPARISON_GREATER_EQUAL:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_LESS;
					break;

				case D3D11_COMPARISON_GREATER:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
					break;
				case D3D11_COMPARISON_LESS_EQUAL:
					d.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
					break;
				}

				switch(cur.BackFace.StencilFunc)
				{
				case D3D11_COMPARISON_ALWAYS:
					d.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
					break;
				case D3D11_COMPARISON_NEVER:
					d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
					break;

				case D3D11_COMPARISON_EQUAL:
					d.BackFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
					break;
				case D3D11_COMPARISON_NOT_EQUAL:
					d.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
					break;

				case D3D11_COMPARISON_LESS:
					d.BackFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
					break;
				case D3D11_COMPARISON_GREATER_EQUAL:
					d.BackFace.StencilFunc = D3D11_COMPARISON_LESS;
					break;

				case D3D11_COMPARISON_GREATER:
					d.BackFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
					break;
				case D3D11_COMPARISON_LESS_EQUAL:
					d.BackFace.StencilFunc = D3D11_COMPARISON_GREATER;
					break;
				}
			}

			SAFE_RELEASE(os);
			hr = m_pDevice->CreateDepthStencilState(&d, &os);
			if(FAILED(hr))
			{
				RDCERR("Failed to create depth/stencil overlay depth state %08x", hr);
				return m_OverlayResourceId;
			}

			m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

			m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

			float redConsts[] = { 255.0f/255.0f, 0.0f/255.0f, 0.0f/255.0f, 255.0f/255.0f };

			ID3D11Buffer *buf = MakeCBuffer(redConsts, sizeof(redConsts));

			m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

			m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

			m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

			SAFE_RELEASE(os);

			m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);

			d = desc;

			if(overlay == eTexOverlay_DepthBoth)
			{
				d.DepthFunc = cur.DepthFunc;
			}
			else if(overlay == eTexOverlay_StencilBoth)
			{
				d.FrontFace = cur.FrontFace;
				d.BackFace = cur.BackFace;
			}

			hr = m_pDevice->CreateDepthStencilState(&d, &os);
			if(FAILED(hr))
			{
				RDCERR("Failed to create depth/stencil overlay depth state 2 %08x", hr);
				return m_OverlayResourceId;
			}

			m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

			float greenConsts[] = { 0.0f/255.0f, 255.0f/255.0f, 0.0f/255.0f, 255.0f/255.0f };

			buf = MakeCBuffer(greenConsts, sizeof(greenConsts));

			m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

			m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

			m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

			SAFE_RELEASE(os);
		}
	}

	SAFE_RELEASE(rtv);

	SAFE_RELEASE(renderDepth);
	SAFE_RELEASE(preDrawDepth);

	old.ApplyState(m_WrappedContext);

	return m_OverlayResourceId;
}

vector<PixelModification> D3D11DebugManager::PixelHistory(uint32_t frameID, vector<uint32_t> events, ResourceId target, uint32_t x, uint32_t y)
{
	vector<PixelModification> history;

	if(events.empty())
		return history;
	
	TextureShaderDetails details = GetShaderDetails(target, true);
	
	if(details.texFmt == DXGI_FORMAT_UNKNOWN)
		return history;
	
	SCOPED_TIMER("D3D11DebugManager::PixelHistory");

	// needed for comparison with viewports
	float xf = (float)x;
	float yf = (float)y;

	RDCDEBUG("Checking Pixel History on %llx (%u, %u) with %u possible events", target, x, y, (uint32_t)events.size());

	// these occlusion queries are run with every test possible disabled
	vector<ID3D11Query*> occl;
	occl.reserve(events.size());

	ID3D11Query *testQueries[6] = {0}; // one query for each test we do per-drawcall

	// reserve 6 pixels per draw on average. Pre/Shaderout/Post required at minimum,
	// and hopefully overdraw within draws will average to only 2 times.
	uint32_t pixstoreSlots = (uint32_t)(events.size() * 6);

	// define a texture that we can copy before/after results into
	D3D11_TEXTURE2D_DESC pixstoreDesc = {
		RDCMIN(2048U, AlignUp16(pixstoreSlots)),
		RDCMAX(1U, pixstoreSlots / 2048),
		1U,
		1U,
		details.texFmt,
		{ 1, 0 },
		D3D11_USAGE_STAGING,
		0,
		D3D11_CPU_ACCESS_READ,
		0,
	};

	ID3D11Texture2D *pixstore = NULL;
	m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &pixstore);
	
	ID3D11Resource *targetres = NULL;
	
	     if(WrappedID3D11Texture1D::m_TextureList.find(target) != WrappedID3D11Texture1D::m_TextureList.end())
				 targetres = ((WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[target].m_Texture)->GetReal();
	else if(WrappedID3D11Texture2D::m_TextureList.find(target) != WrappedID3D11Texture2D::m_TextureList.end())
				 targetres = ((WrappedID3D11Texture2D *)WrappedID3D11Texture2D::m_TextureList[target].m_Texture)->GetReal();
	else if(WrappedID3D11Texture3D::m_TextureList.find(target) != WrappedID3D11Texture3D::m_TextureList.end())
				 targetres = ((WrappedID3D11Texture3D *)WrappedID3D11Texture3D::m_TextureList[target].m_Texture)->GetReal();

	// while issuing the above queries we can check to see which tests are enabled so we don't
	// bother checking if depth testing failed if the depth test was disabled
	vector<uint32_t> flags(events.size());
	enum {
		TestEnabled_BackfaceCulling = 1<<0,
		TestEnabled_DepthClip       = 1<<1,
		TestEnabled_Scissor         = 1<<2,
		TestEnabled_DepthTesting    = 1<<3,
		TestEnabled_StencilTesting  = 1<<4,

		// important to know if blending is enabled or not as we currently skip a bunch of stuff
		// and only pay attention to the final passing fragment if blending is off
		Blending_Enabled            = 1<<5,
		
		// additional flags we can trivially detect on the CPU for edge cases
		TestMustFail_Scissor        = 1<<6, // if the scissor is enabled, pixel lies outside all regions (could be only one)
		TestMustPass_Scissor        = 1<<7, // if the scissor is enabled, pixel lies inside all regions (could be only one)
		TestMustFail_DepthTesting   = 1<<8, // if the comparison func is NEVER
		TestMustFail_StencilTesting = 1<<9, // if the comparison func is NEVER for both faces, or one face is backface culled and the other is NEVER
	};

#if 1
	BOOL occlData = 0;
	const D3D11_QUERY_DESC occlDesc = { D3D11_QUERY_OCCLUSION_PREDICATE, 0 };
#else
	UINT64 occlData = 0;
	const D3D11_QUERY_DESC occlDesc = { D3D11_QUERY_OCCLUSION, 0 };
#endif

	HRESULT hr = S_OK;

	for(size_t i=0; i < events.size(); i++)
	{
		ID3D11Query *q = NULL;
		m_pDevice->CreateQuery(&occlDesc, &q);
		occl.push_back(q);
	}
	
	for(size_t i=0; i < ARRAY_COUNT(testQueries); i++)
		m_pDevice->CreateQuery(&occlDesc, &testQueries[i]);

	D3D11_BLEND_DESC nopBlendStateDesc = {0}; // no blending enabled anywhere, 0 write mask
	ID3D11BlendState *nopBlendState = NULL;
	m_pDevice->CreateBlendState(&nopBlendStateDesc, &nopBlendState);

	D3D11_DEPTH_STENCIL_DESC nopDSStateDesc = { // no tests enabled, no writing enabled
		FALSE, // DepthEnable;
		D3D11_DEPTH_WRITE_MASK_ZERO, // DepthWriteMask;
		D3D11_COMPARISON_ALWAYS, // DepthFunc;
		FALSE, // StencilEnable;
		0, // StencilReadMask;
		0, // StencilWriteMask;
		{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS }, // FrontFace;
		{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS }, // BackFace;
	};
	ID3D11DepthStencilState *nopDSState = NULL;
	m_pDevice->CreateDepthStencilState(&nopDSStateDesc, &nopDSState);

	m_WrappedDevice->ReplayLog(frameID, 0, events[0], eReplay_WithoutDraw);

	D3D11_VIEWPORT viewport = { (float)x, (float)y, 10.0f, 10.0f, 0.0f, 1.0f }; // 1x1 viewport of our destination pixel

	ID3D11RasterizerState *curRS = NULL;
	ID3D11RasterizerState *newRS = NULL;
	ID3D11DepthStencilState *newDS = NULL;
	ID3D11PixelShader *curPS = NULL;
	ID3D11ClassInstance *curInst[D3D11_SHADER_MAX_INTERFACES] = { NULL };
	UINT curNumInst = 0;
	UINT curNumViews = 0;
	UINT curNumScissors = 0;
	D3D11_VIEWPORT curViewports[16] = {0};
	D3D11_RECT curScissors[16] = {0};
	D3D11_RECT newScissors[16] = {0};
	ID3D11BlendState *curBS = NULL;
	float blendFactor[4] = {0};
	UINT curSample = 0;
	ID3D11DepthStencilState *curDS = NULL;
	UINT stencilRef = 0;

	for(size_t ev=0; ev < events.size(); ev++)
	{
		curNumInst = D3D11_SHADER_MAX_INTERFACES;
		curNumScissors = curNumViews = 16;

		m_pImmediateContext->RSGetState(&curRS);
		m_pImmediateContext->OMGetBlendState(&curBS, blendFactor, &curSample);
		m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
		m_pImmediateContext->PSGetShader(&curPS, curInst, &curNumInst);
		m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
		m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);

		// defaults (mostly)
		// disable tests/clips and enable scissor as we need it to clip visibility to just our pixel
		// TODO determine if a pixel would have been scissor clipped.
		D3D11_RASTERIZER_DESC rd = {
			/*FillMode =*/ D3D11_FILL_SOLID,
			/*CullMode =*/ D3D11_CULL_NONE,
			/*FrontCounterClockwise =*/ FALSE,
			/*DepthBias =*/ D3D11_DEFAULT_DEPTH_BIAS,
			/*DepthBiasClamp =*/ D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
			/*SlopeScaledDepthBias =*/ D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			/*DepthClipEnable =*/ FALSE,
			/*ScissorEnable =*/ TRUE,
			/*MultisampleEnable =*/ FALSE,
			/*AntialiasedLineEnable =*/ FALSE,
		};

		D3D11_RASTERIZER_DESC rsDesc;
		RDCEraseEl(rsDesc);

		if(curRS)
		{
			curRS->GetDesc(&rsDesc);

			rd = rsDesc;

			if(rd.CullMode != D3D11_CULL_NONE)
				flags[ev] |= TestEnabled_BackfaceCulling;
			if(rd.DepthClipEnable)
				flags[ev] |= TestEnabled_DepthClip;
			if(rd.ScissorEnable)
				flags[ev] |= TestEnabled_Scissor;

			rd.CullMode = D3D11_CULL_NONE;
			rd.DepthClipEnable = FALSE;

			rd.ScissorEnable = TRUE;
		}
		else
		{
			rsDesc.CullMode = D3D11_CULL_BACK;
			rsDesc.ScissorEnable = FALSE;

			// defaults
			flags[ev] |= (TestEnabled_BackfaceCulling|TestEnabled_DepthClip);
		}

		if(curDS)
		{
			D3D11_DEPTH_STENCIL_DESC dsDesc;
			curDS->GetDesc(&dsDesc);

			if(dsDesc.DepthEnable)
			{
				if(dsDesc.DepthFunc != D3D11_COMPARISON_ALWAYS)
					flags[ev] |= TestEnabled_DepthTesting;

				if(dsDesc.DepthFunc == D3D11_COMPARISON_NEVER)
					flags[ev] |= TestMustFail_DepthTesting;
			}
			
			if(dsDesc.StencilEnable)
			{
				if(dsDesc.FrontFace.StencilFunc != D3D11_COMPARISON_ALWAYS || dsDesc.BackFace.StencilFunc != D3D11_COMPARISON_ALWAYS)
					flags[ev] |= TestEnabled_StencilTesting;

				if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER && dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
					flags[ev] |= TestMustFail_StencilTesting;

				if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER && rsDesc.CullMode == D3D11_CULL_BACK                   )
					flags[ev] |= TestMustFail_StencilTesting;

				if(rsDesc.CullMode == D3D11_CULL_FRONT                    && dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
					flags[ev] |= TestMustFail_StencilTesting;
			}
		}
		else
		{
			// defaults
			flags[ev] |= TestEnabled_DepthTesting;
		}

		if(rsDesc.ScissorEnable)
		{
			// see if we can find at least one scissor region this pixel could fall into
			bool inRegion = false;
			bool inAllRegions = true;

			for(UINT i=0; i < curNumScissors && i < curNumViews; i++)
			{
				if(xf >= curViewports[i].TopLeftX + float(curScissors[i].left) &&
					 yf >= curViewports[i].TopLeftY + float(curScissors[i].top) &&
					 xf < curViewports[i].TopLeftX + RDCMIN(curViewports[i].Width, float(curScissors[i].right)) &&
					 yf < curViewports[i].TopLeftY + RDCMIN(curViewports[i].Height, float(curScissors[i].bottom))
					)
				{
					inRegion = true;
				}
				else
				{
					inAllRegions = false;
				}
			}

			if(!inRegion)
				flags[ev] |= TestMustFail_Scissor;
			if(inAllRegions)
				flags[ev] |= TestMustPass_Scissor;
		}

		if(curBS)
		{
			D3D11_BLEND_DESC desc;
			curBS->GetDesc(&desc);

			if(desc.IndependentBlendEnable)
			{
				for(int i=0; i < 8; i++)
				{
					if(desc.RenderTarget[i].BlendEnable)
					{
						flags[ev] |= Blending_Enabled;
						break;
					}
				}
			}
			else
			{
				if(desc.RenderTarget[0].BlendEnable)
					flags[ev] |= Blending_Enabled;
			}
		}
		else
		{
			// no blending enabled by default
		}

		m_pDevice->CreateRasterizerState(&rd, &newRS);
		m_pImmediateContext->RSSetState(newRS);

		m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

		m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
		m_pImmediateContext->OMSetDepthStencilState(nopDSState, stencilRef);

		for(UINT i=0; i < curNumViews; i++)
		{
			// calculate scissor, relative to this viewport, that encloses only (x,y) pixel

			// if (x,y) pixel isn't in viewport, make empty rect)
			if(xf < curViewports[i].TopLeftX ||
				yf < curViewports[i].TopLeftY ||
				xf >= curViewports[i].TopLeftX + curViewports[i].Width ||
				yf >= curViewports[i].TopLeftY + curViewports[i].Height)
			{
				newScissors[i].left = newScissors[i].top = newScissors[i].bottom = newScissors[i].right = 0;
			}
			else
			{
				newScissors[i].left = LONG(xf - curViewports[i].TopLeftX);
				newScissors[i].top = LONG(yf - curViewports[i].TopLeftY);
				newScissors[i].right = newScissors[i].left+1;
				newScissors[i].bottom = newScissors[i].top+1;
			}
		}

		// scissor every viewport
		m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

		D3D11_BOX srcbox = { x, y, 0, x+1, y+1, 1 };

		// figure out where this event lies in the pixstore texture
		UINT storex = UINT(ev % (2048/3));
		UINT storey = UINT(ev / (2048/3));

		m_pImmediateContext->CopySubresourceRegion(pixstore, 0, storex*3 + 0, storey, 0, targetres, 0, &srcbox);

		m_pImmediateContext->Begin(occl[ev]);

		m_WrappedDevice->ReplayLog(frameID, 0, events[ev], eReplay_OnlyDraw);

		m_pImmediateContext->End(occl[ev]);

		// TODO, if drawcall has side-effects (i.e UAVs bound) replay from start of log here?

		m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);
		m_pImmediateContext->RSSetState(curRS);
		m_pImmediateContext->RSSetScissorRects(curNumScissors, curScissors);
		m_pImmediateContext->OMSetBlendState(curBS, blendFactor, curSample);
		m_pImmediateContext->OMSetDepthStencilState(curDS, stencilRef);

		for(UINT i=0; i < curNumInst; i++)
			SAFE_RELEASE(curInst[i]);

		SAFE_RELEASE(curPS);
		SAFE_RELEASE(curRS);
		SAFE_RELEASE(newRS);
		SAFE_RELEASE(curBS);
		SAFE_RELEASE(curDS);

		// deliberately include drawcall in this range so that it gets replayed with correct
		// state (otherwise this draw e.g. wouldn't write depth when it should)
		if(ev < events.size()-1)
			m_WrappedDevice->ReplayLog(frameID, events[ev], events[ev+1], eReplay_WithoutDraw);
		else
			m_WrappedDevice->ReplayLog(frameID, events[ev], events[ev], eReplay_OnlyDraw);
		
		m_pImmediateContext->CopySubresourceRegion(pixstore, 0, storex*3 + 1, storey, 0, targetres, 0, &srcbox);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {0};
	m_pImmediateContext->Map(pixstore, 0, D3D11_MAP_READ, 0, &mapped);

	byte *pixstoreData = (byte *)mapped.pData;
	
	bool depthStencilTex = details.texType == eTexType_Depth || 
		details.texType == eTexType_DepthMS ||
		details.texType == eTexType_Stencil ||
		details.texType == eTexType_StencilMS;

	for(size_t i=0; i < occl.size(); i++)
	{
		do
		{
			hr = m_pImmediateContext->GetData(occl[i], &occlData, sizeof(occlData), 0);
		} while(hr == S_FALSE);
		RDCASSERT(hr == S_OK);

		const FetchDrawcall *draw = m_WrappedDevice->GetDrawcall(frameID, events[i]);

		bool clear = (draw->flags & eDraw_Clear);

		if(occlData > 0 || clear)
		{
			PixelModification mod;
			RDCEraseEl(mod);

			mod.eventID = events[i];

			ResourceFormat fmt = MakeResourceFormat(details.texFmt);
			
			if(!fmt.special && fmt.compCount > 0 && fmt.compByteWidth > 0)
			{
				// figure out where this event lies in the pixstore texture
				uint32_t storex = uint32_t(i % (2048/3));
				uint32_t storey = uint32_t(i / (2048/3));

				byte *rowdata = pixstoreData + mapped.RowPitch * storey;
				byte *data = rowdata + fmt.compCount * fmt.compByteWidth * storex * 3;

				if(fmt.compType == eCompType_SInt)
				{
					// need to get correct sign, but otherwise just copy

					if(fmt.compByteWidth == 1)
					{
						int8_t *d = (int8_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.preMod.value_i[c] = d[c];
					}
					else if(fmt.compByteWidth == 2)
					{
						int16_t *d = (int16_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.preMod.value_i[c] = d[c];
					}
					else if(fmt.compByteWidth == 4)
					{
						int32_t *d = (int32_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.preMod.value_i[c] = d[c];
					}

					data = rowdata + fmt.compCount * fmt.compByteWidth * (storex * 3 + 1);
					
					if(fmt.compByteWidth == 1)
					{
						int8_t *d = (int8_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.postMod.value_i[c] = d[c];
					}
					else if(fmt.compByteWidth == 2)
					{
						int16_t *d = (int16_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.postMod.value_i[c] = d[c];
					}
					else if(fmt.compByteWidth == 4)
					{
						int32_t *d = (int32_t*)data;
						for(uint32_t c=0; c < fmt.compCount; c++)
							mod.postMod.value_i[c] = d[c];
					}
				}
				else
				{
					for(uint32_t c=0; c < fmt.compCount; c++)
						memcpy(&mod.preMod.value_u[c], data + fmt.compByteWidth * c, fmt.compByteWidth);

					data = rowdata + fmt.compCount * fmt.compByteWidth * (storex * 3 + 1);

					for(uint32_t c=0; c < fmt.compCount; c++)
						memcpy(&mod.postMod.value_u[c], data + fmt.compByteWidth * c, fmt.compByteWidth);

					if(fmt.compType == eCompType_Float && fmt.compByteWidth == 2)
					{
						for(uint32_t c=0; c < fmt.compCount; c++)
						{
							mod.preMod.value_f[c]  = ConvertFromHalf(uint16_t(mod.preMod.value_u[c]));
							mod.postMod.value_f[c] = ConvertFromHalf(uint16_t(mod.postMod.value_u[c]));
						}
					}
					else if(fmt.compType == eCompType_UNorm)
					{
						// only 32bit unorm format is depth, handled separately
						float maxVal = fmt.compByteWidth == 2 ? 65535.0f : 255.0f;

						RDCASSERT(fmt.compByteWidth < 4);

						for(uint32_t c=0; c < fmt.compCount; c++)
						{
							mod.preMod.value_f[c]  = float(mod.preMod.value_u[c])/maxVal;
							mod.postMod.value_f[c] = float(mod.postMod.value_u[c])/maxVal;
						}
					}
					else if(fmt.compType == eCompType_UNorm_SRGB)
					{
						RDCASSERT(fmt.compByteWidth == 1);

						for(uint32_t c=0; c < RDCMIN(fmt.compCount, 3U); c++)
						{
							mod.preMod.value_f[c]  = ConvertFromSRGB8(mod.preMod.value_u[c]&0xff);
							mod.postMod.value_f[c] = ConvertFromSRGB8(mod.postMod.value_u[c]&0xff);
						}

						// alpha is not SRGB'd
						if(fmt.compCount == 4)
						{
							mod.preMod.value_f[3] = float(mod.preMod.value_u[3]&0xff)/255.0f;
							mod.postMod.value_f[3] = float(mod.postMod.value_u[3]&0xff)/255.0f;
						}
					}
					else if(fmt.compType == eCompType_SNorm && fmt.compByteWidth == 2)
					{
						for(uint32_t c=0; c < fmt.compCount; c++)
						{
							mod.preMod.value_f[c]  = float(mod.preMod.value_u[c]);
							mod.postMod.value_f[c] = float(mod.postMod.value_u[c]);
						}
					}
					else if(fmt.compType == eCompType_SNorm && fmt.compByteWidth == 1)
					{
						for(uint32_t c=0; c < fmt.compCount; c++)
						{
							int8_t *d = (int8_t *)&mod.preMod.value_u[c];

							if(*d == -128)
								mod.preMod.value_f[c] = -1.0f;
							else
								mod.preMod.value_f[c] = float(*d)/127.0f;

							d = (int8_t *)&mod.preMod.value_u[c];

							if(*d == -128)
								mod.preMod.value_f[c] = -1.0f;
							else
								mod.preMod.value_f[c] = float(*d)/127.0f;
						}
					}
					else if(fmt.compType == eCompType_SNorm && fmt.compByteWidth == 2)
					{
						for(uint32_t c=0; c < fmt.compCount; c++)
						{
							int16_t *d = (int16_t *)&mod.preMod.value_u[c];

							if(*d == -32768)
								mod.preMod.value_f[c] = -1.0f;
							else
								mod.preMod.value_f[c] = float(*d)/32767.0f;

							d = (int16_t *)&mod.preMod.value_u[c];

							if(*d == -32768)
								mod.preMod.value_f[c] = -1.0f;
							else
								mod.preMod.value_f[c] = float(*d)/32767.0f;
						}
					}
				}
			}

			// complex case - need to determine how many fragments from this draw wrote to the pixel and generate a
			// PixelModification event for all of them.
			//
			// Is there any value in doing this anyway for non-blended cases where other fragments are completely discarded?
			if(flags[i] & Blending_Enabled)
			{
			}
			
			mod.shaderOut.value_f[0] = mod.postMod.value_f[0];
			mod.shaderOut.value_f[1] = mod.postMod.value_f[1];
			mod.shaderOut.value_f[2] = mod.postMod.value_f[2];
			mod.shaderOut.value_f[3] = mod.postMod.value_f[3];

			if((draw->flags & eDraw_Clear) == 0)
			{
				if(flags[i] & TestMustFail_DepthTesting)
					mod.depthTestFailed = true;
				if(flags[i] & TestMustFail_StencilTesting)
					mod.stencilTestFailed = true;
				if(flags[i] & TestMustFail_Scissor)
					mod.scissorClipped = true;

				m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

				curNumScissors = curNumViews = 16;
				m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
				m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);
				m_pImmediateContext->RSGetState(&curRS);
				m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
				blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 1.0f;
				curSample = ~0U;

				D3D11_RASTERIZER_DESC rdesc = {
					/*FillMode =*/ D3D11_FILL_SOLID,
					/*CullMode =*/ D3D11_CULL_BACK,
					/*FrontCounterClockwise =*/ FALSE,
					/*DepthBias =*/ D3D11_DEFAULT_DEPTH_BIAS,
					/*DepthBiasClamp =*/ D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
					/*SlopeScaledDepthBias =*/ D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
					/*DepthClipEnable =*/ TRUE,
					/*ScissorEnable =*/ FALSE,
					/*MultisampleEnable =*/ FALSE,
					/*AntialiasedLineEnable =*/ FALSE,
				};
				if(curRS)
					curRS->GetDesc(&rdesc);

				D3D11_DEPTH_STENCIL_DESC dsdesc = {
					/*DepthEnable =*/ TRUE,
					/*DepthWriteMask =*/ D3D11_DEPTH_WRITE_MASK_ALL,
					/*DepthFunc =*/ D3D11_COMPARISON_LESS,
					/*StencilEnable =*/ FALSE,
					/*StencilReadMask =*/ D3D11_DEFAULT_STENCIL_READ_MASK,
					/*StencilWriteMask =*/ D3D11_DEFAULT_STENCIL_WRITE_MASK,
					/*FrontFace =*/ { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
					/*BackFace =*/ { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
				};

				if(curDS)
					curDS->GetDesc(&dsdesc);

				for(UINT v=0; v < curNumViews; v++)
				{
					// calculate scissor, relative to this viewport, that encloses only (x,y) pixel

					// if (x,y) pixel isn't in viewport, make empty rect)
					if(xf < curViewports[v].TopLeftX ||
						yf < curViewports[v].TopLeftY ||
						xf >= curViewports[v].TopLeftX + curViewports[v].Width ||
						yf >= curViewports[v].TopLeftY + curViewports[v].Height)
					{
						newScissors[v].left = newScissors[v].top = newScissors[v].bottom = newScissors[v].right = 0;
					}
					else
					{
						newScissors[v].left = LONG(xf - curViewports[v].TopLeftX);
						newScissors[v].top = LONG(yf - curViewports[v].TopLeftY);
						newScissors[v].right = newScissors[v].left+1;
						newScissors[v].bottom = newScissors[v].top+1;
					}
				}

				// for each test we only disable pipeline rejection tests that fall *after* it.
				// e.g. to get an idea if a pixel failed backface culling or not, we enable only backface
				// culling and disable everything else (since it happens first).
				// For depth testing, we leave all tests enabled up to then - as we only want to know which
				// pixels were rejected by the depth test, not pixels that might have passed the depth test
				// had they not been discarded earlier by backface culling or depth clipping.
				
				// test shader discard
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					// leave depth clip mode as normal
					// leave backface culling mode as normal

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(nopDSState, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

					m_pImmediateContext->Begin(testQueries[3]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[3]);

					SAFE_RELEASE(newRS);
					
					// This full replay seems to work around a D3D/driver bug where sometimes tests will fail
					// wrongly. It seems something to do with small scissor rects interacting with small triangles,
					// but it's not clear (there's definitely a bug as disabling scissor and just looking at # pixels
					// passing overall in the draw changes based on the scissor rect that is set).
					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_Full);
				}

				if(flags[i] & TestEnabled_BackfaceCulling)
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					rd.DepthClipEnable = FALSE;
					// leave backface culling mode as normal

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(nopDSState, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

					m_pImmediateContext->Begin(testQueries[0]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[0]);

					SAFE_RELEASE(newRS);
				}

				if(flags[i] & TestEnabled_DepthClip)
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					// leave depth clip mode as normal
					// leave backface culling mode as normal

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(nopDSState, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

					m_pImmediateContext->Begin(testQueries[1]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[1]);

					SAFE_RELEASE(newRS);
				}

				// only check scissor if test is enabled and we don't know if it's pass or fail yet
				if((flags[i] & (TestEnabled_Scissor|TestMustPass_Scissor|TestMustFail_Scissor)) == TestEnabled_Scissor)
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					// leave depth clip mode as normal
					// leave backface culling mode as normal

					// newScissors has scissor regions calculated to hit our target pixel on every viewport, but we must
					// intersect that with the original scissors regions for correct testing behaviour.
					// This amounts to making any scissor region that doesn't overlap with the target pixel empty.
					//
					// Note that in the case of only one scissor region we can trivially detect pass/fail of the test against
					// our pixel on the CPU so we won't come in here (see check above against MustFail/MustPass). So we will
					// only do this in the case where we have multiple scissor regions/viewports, some intersecting the pixel
					// and some not. So we make the not intersecting scissor regions empty so our occlusion query tests to see
					// if any pixels were written to the "passing" viewports
					D3D11_RECT intersectScissors[16] = {0};
					memcpy(intersectScissors, newScissors, sizeof(intersectScissors));

					for(UINT s=0; s < curNumScissors; s++)
					{
						if(curScissors[i].left > newScissors[i].left ||
							curScissors[i].right < newScissors[i].right ||
							curScissors[i].top > newScissors[i].top ||
							curScissors[i].bottom < newScissors[i].bottom)
						{
							// scissor region from the log doesn't touch our target pixel, make empty.
							intersectScissors[i].left = intersectScissors[i].right = intersectScissors[i].top = intersectScissors[i].bottom = 0;
						}
					}

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(nopDSState, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumScissors, intersectScissors);

					m_pImmediateContext->Begin(testQueries[2]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[2]);

					SAFE_RELEASE(newRS);
				}

				if(flags[i] & TestEnabled_DepthTesting)
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					// leave depth clip mode as normal
					// leave backface culling mode as normal

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					D3D11_DEPTH_STENCIL_DESC dsd = dsdesc;

					dsd.StencilEnable = FALSE;
					dsd.StencilReadMask = 0;
					dsd.StencilWriteMask = 0;

					m_pDevice->CreateDepthStencilState(&dsd, &newDS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

					m_pImmediateContext->Begin(testQueries[4]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[4]);

					SAFE_RELEASE(newRS);
					SAFE_RELEASE(newDS);
				}

				if(flags[i] & TestEnabled_StencilTesting)
				{
					D3D11_RASTERIZER_DESC rd = rdesc;

					rd.ScissorEnable = TRUE;
					rd.DepthClipEnable = FALSE;
					rd.CullMode = D3D11_CULL_NONE;

					m_pDevice->CreateRasterizerState(&rd, &newRS);

					// leave depthstencil testing exactly as is, because a depth-fail means
					// stencil isn't run
					m_pDevice->CreateDepthStencilState(&dsdesc, &newDS);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_WithoutDraw);

					m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
					m_pImmediateContext->OMSetBlendState(nopBlendState, blendFactor, curSample);
					m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
					m_pImmediateContext->RSSetState(newRS);
					m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

					m_pImmediateContext->Begin(testQueries[5]);

					m_WrappedDevice->ReplayLog(frameID, 0, events[i], eReplay_OnlyDraw);

					m_pImmediateContext->End(testQueries[5]);

					SAFE_RELEASE(newRS);
					SAFE_RELEASE(newDS);
				}

				SAFE_RELEASE(curRS);

				// we check these in the order defined, as a positive from the backface cull test
				// will invalidate tests later (as they will also be backface culled)

				do 
				{

					if(flags[i] & TestEnabled_BackfaceCulling)
					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[0], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.backfaceCulled = (occlData == 0);

						if(mod.backfaceCulled)
							break;
					}

					if(flags[i] & TestEnabled_DepthClip)
					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[1], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.depthClipped = (occlData == 0);

						if(mod.depthClipped)
							break;
					}

					if(!mod.backfaceCulled && (flags[i] & (TestEnabled_Scissor|TestMustPass_Scissor|TestMustFail_Scissor)) == TestEnabled_Scissor)
					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[2], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.scissorClipped = (occlData == 0);

						if(mod.scissorClipped)
							break;
					}

					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[3], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.shaderDiscarded = (occlData == 0);

						if(mod.shaderDiscarded)
							break;
					}

					if(flags[i] & TestEnabled_DepthTesting)
					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[4], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.depthTestFailed = (occlData == 0);

						if(mod.depthTestFailed)
							break;
					}

					if(flags[i] & TestEnabled_StencilTesting)
					{
						do
						{
							hr = m_pImmediateContext->GetData(testQueries[5], &occlData, sizeof(occlData), 0);
						} while(hr == S_FALSE);
						RDCASSERT(hr == S_OK);

						mod.stencilTestFailed = (occlData == 0);

						if(mod.stencilTestFailed)
							break;
					}
				} while (0);
			}
			
			history.push_back(mod);

			RDCDEBUG("Event %u is visible", events[i]);
			if(sizeof(occlData) == sizeof(UINT64)) // if we've changed from OCCLUSION_PREDICATE to OCCLUSION for debugging
				RDCDEBUG("   %llu samples visible", occlData);
		}

		SAFE_RELEASE(occl[i]);
	}

	m_pImmediateContext->Unmap(pixstore, 0);
	
	for(size_t i=0; i < ARRAY_COUNT(testQueries); i++)
		SAFE_RELEASE(testQueries[i]);

	SAFE_RELEASE(nopBlendState);
	SAFE_RELEASE(nopDSState);

	SAFE_RELEASE(pixstore);

	return history;
}
