/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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


#include "common/common.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "api/app/renderdoc_app.h"
#include "dxbc_inspect.h"
#include "dxbc_sdbg.h"
#include "dxbc_spdb.h"

using std::make_pair;

namespace DXBC
{
	
struct RDEFCBufferVariable
{
	uint32_t nameOffset;

	uint32_t startOffset;				// start offset in bytes of this variable in the cbuffer
	uint32_t size;					// size in bytes of this type
	uint32_t flags;

	uint32_t typeOffset;				// offset to a RDEFCBufferType
	uint32_t defaultValueOffset;		// offset to [size] bytes where the default value can be found, or 0 for no default value

	uint32_t unknown[4];				// this is only present for RDEFHeader.targetVersion >= 0x500. In earlier versions, this is not in the file.
};

struct RDEFCBuffer
{
	uint32_t nameOffset;				// relative to the same offset base position as others in this chunk - after FourCC and chunk length.

	DXBC::CountOffset variables;
	uint32_t size;					// size in bytes of this cbuffer
	uint32_t flags;
	uint32_t type;

	// followed immediately by [variables.count] RDEFCBufferVariables
};

// mostly for nested structures
struct RDEFCBufferChildType
{
	uint32_t nameOffset;
	uint32_t typeOffset;				// offset to a RDEFCBufferType
	uint32_t memberOffset;			// byte offset in the parent structure - not a file offset
};

struct RDEFCBufferType
{
	uint16_t varClass;				// D3D_SHADER_VARIABLE_CLASS
	uint16_t varType;					// D3D_SHADER_VARIABLE_TYPE

	uint16_t rows;
	uint16_t cols;

	uint16_t numElems;
	uint16_t numMembers;

	uint32_t memberOffset;			// offset to [numMembers] RDEFCBufferChildTypes that point to the member types

	// my own guessing - not in wine structures
	// looks like these are only present for RD11 shaders
	uint32_t unknown[4];

	uint32_t nameOffset;				// offset to type name
};

// this isn't a proper chunk, it's the file header before all the chunks.
struct FileHeader
{
	uint32_t fourcc;					// "DXBC"
	uint32_t hashValue[4];			// unknown hash function and data
	uint32_t unknown;
	uint32_t fileLength;
	uint32_t numChunks;
	// uint32 chunkOffsets[numChunks]; follows
};

struct RDEFHeader
{
	uint32_t fourcc;					// "RDEF"
	uint32_t chunkLength;				// length of this chunk

	//////////////////////////////////////////////////////
	// offsets are relative to this position in the file.
	// NOT the end of this structure. Note this differs
	// from the SDBG chunk, but matches the SIGN chunks
	
	// note that these two actually come in the opposite order after
	// this header. So cbuffers offset will be higher than resources
	// offset
	CountOffset cbuffers;
	CountOffset resources;

    uint16_t targetVersion;			// 0x0500 is the latest.
    uint16_t targetShaderStage;		// 0xffff for pixel shaders, 0xfffe for vertex shaders

    uint32_t flags;
    uint32_t creatorOffset;			// null terminated ascii string

	uint32_t unknown[8];				// this is only present for targetVersion >= 0x500. In earlier versions, this is not in the file.
};

struct RDEFResource
{
	uint32_t nameOffset;			// relative to the same offset base position as others in this chunk - after FourCC and chunk length.

	uint32_t type;
	uint32_t retType;
	uint32_t dimension;
	int32_t sampleCount;
	uint32_t bindPoint;
	uint32_t bindCount;
	uint32_t flags;
};

struct SIGNHeader
{
	uint32_t fourcc;					// "ISGN", "OSGN, "OSG5", "PCSG"
	uint32_t chunkLength;				// length of this chunk

	//////////////////////////////////////////////////////
	// offsets are relative to this position in the file.
	// NOT the end of this structure. Note this differs
	// from the SDBG chunk, but matches the RDEF chunk
	
    uint32_t numElems;
    uint32_t unknown;

	// followed by SIGNElement elements[numElems]; - note that SIGNElement's size depends on the type.
	// for OSG5 you should use SIGNElement7
};

struct PRIVHeader
{
	uint32_t fourcc;					// "PRIV"
	uint32_t chunkLength;				// length of this chunk

	GUID debugInfoGUID;         // GUID/magic number, since PRIV data could be used for something else.
	                            // Set to the value of RENDERDOC_ShaderDebugMagicValue from
	                            // renderdoc_app.h which can also be used as a GUID to set the path
	                            // at runtime via SetPrivateData (see documentation)
	
	static const GUID RENDERDOC_ShaderDebugMagicValue;
	
	void* data;
};

const GUID PRIVHeader::RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

struct SIGNElement
{
	uint32_t nameOffset;				// relative to the same offset base position as others in similar chunks - after FourCC and chunk length.

	uint32_t semanticIdx;
	uint32_t systemType;
	uint32_t componentType;
	uint32_t registerNum;

	byte mask;
	byte rwMask;
	uint16_t unused;
};

struct SIGNElement7
{
	uint32_t stream;
	SIGNElement elem;
};

static const uint32_t STATSizeDX10 = 29*4; // either 29 uint32s
static const uint32_t STATSizeDX11 = 37*4; // or 37 uint32s

static const uint32_t FOURCC_DXBC = MAKE_FOURCC('D', 'X', 'B', 'C');
static const uint32_t FOURCC_RDEF = MAKE_FOURCC('R', 'D', 'E', 'F');
static const uint32_t FOURCC_RD11 = MAKE_FOURCC('R', 'D', '1', '1');
static const uint32_t FOURCC_STAT = MAKE_FOURCC('S', 'T', 'A', 'T');
static const uint32_t FOURCC_SHEX = MAKE_FOURCC('S', 'H', 'E', 'X');
static const uint32_t FOURCC_SHDR = MAKE_FOURCC('S', 'H', 'D', 'R');
static const uint32_t FOURCC_SDBG = MAKE_FOURCC('S', 'D', 'B', 'G');
static const uint32_t FOURCC_SPDB = MAKE_FOURCC('S', 'P', 'D', 'B');
static const uint32_t FOURCC_ISGN = MAKE_FOURCC('I', 'S', 'G', 'N');
static const uint32_t FOURCC_OSGN = MAKE_FOURCC('O', 'S', 'G', 'N');
static const uint32_t FOURCC_OSG5 = MAKE_FOURCC('O', 'S', 'G', '5');
static const uint32_t FOURCC_PCSG = MAKE_FOURCC('P', 'C', 'S', 'G');
static const uint32_t FOURCC_Aon9 = MAKE_FOURCC('A', 'o', 'n', '9');
static const uint32_t FOURCC_PRIV = MAKE_FOURCC('P', 'R', 'I', 'V');

int TypeByteSize(VariableType t)
{
	switch(t)
	{
		case VARTYPE_UINT8:
			return 1;
		case VARTYPE_BOOL:
		case VARTYPE_INT:
		case VARTYPE_FLOAT:
		case VARTYPE_UINT:
			return 4;
		case VARTYPE_DOUBLE:
			return 8;
		// 'virtual' type. Just return 1
		case VARTYPE_INTERFACE_POINTER:
			return 1;
		default:
			RDCERR("Trying to take size of undefined type %d", t);
			return 1;
	}
}

SystemAttribute GetSystemValue(uint32_t systemValue)
{
	enum DXBC_SVSemantic
	{
		SVNAME_UNDEFINED = 0,
		SVNAME_POSITION,
		SVNAME_CLIP_DISTANCE,
		SVNAME_CULL_DISTANCE,
		SVNAME_RENDER_TARGET_ARRAY_INDEX,
		SVNAME_VIEWPORT_ARRAY_INDEX,
		SVNAME_VERTEX_ID,
		SVNAME_PRIMITIVE_ID,
		SVNAME_INSTANCE_ID,
		SVNAME_IS_FRONT_FACE,
		SVNAME_SAMPLE_INDEX,

		// following are non-contiguous
		SVNAME_FINAL_QUAD_EDGE_TESSFACTOR,
		SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR = SVNAME_FINAL_QUAD_EDGE_TESSFACTOR+4,
		SVNAME_FINAL_TRI_EDGE_TESSFACTOR = SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR+2,
		SVNAME_FINAL_TRI_INSIDE_TESSFACTOR = SVNAME_FINAL_TRI_EDGE_TESSFACTOR+3,
		SVNAME_FINAL_LINE_DETAIL_TESSFACTOR,
		SVNAME_FINAL_LINE_DENSITY_TESSFACTOR,

		SVNAME_TARGET = 64,
		SVNAME_DEPTH,
		SVNAME_COVERAGE,
		SVNAME_DEPTH_GREATER_EQUAL,
		SVNAME_DEPTH_LESS_EQUAL,
	};
	
	switch(systemValue)
	{
		case SVNAME_UNDEFINED:
			return eAttr_None;
		case SVNAME_POSITION:
			return eAttr_Position;
		case SVNAME_CLIP_DISTANCE:
			return eAttr_ClipDistance;
		case SVNAME_CULL_DISTANCE:
			return eAttr_CullDistance;
		case SVNAME_RENDER_TARGET_ARRAY_INDEX:
			return eAttr_RTIndex;
		case SVNAME_VIEWPORT_ARRAY_INDEX:
			return eAttr_ViewportIndex;
		case SVNAME_VERTEX_ID:
			return eAttr_VertexIndex;
		case SVNAME_PRIMITIVE_ID:
			return eAttr_PrimitiveIndex;
		case SVNAME_INSTANCE_ID:
			return eAttr_InstanceIndex;
		case SVNAME_IS_FRONT_FACE:
			return eAttr_IsFrontFace;
		case SVNAME_SAMPLE_INDEX:
			return eAttr_MSAASampleIndex;
		case SVNAME_FINAL_QUAD_EDGE_TESSFACTOR:
			return eAttr_OuterTessFactor;
		case SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR:
			return eAttr_InsideTessFactor;
		case SVNAME_FINAL_TRI_EDGE_TESSFACTOR:
			return eAttr_OuterTessFactor;
		case SVNAME_FINAL_TRI_INSIDE_TESSFACTOR:
			return eAttr_InsideTessFactor;
		case SVNAME_FINAL_LINE_DETAIL_TESSFACTOR:
			return eAttr_OuterTessFactor;
		case SVNAME_FINAL_LINE_DENSITY_TESSFACTOR:
			return eAttr_InsideTessFactor;
		case SVNAME_TARGET:
			return eAttr_ColourOutput;
		case SVNAME_DEPTH:
			return eAttr_DepthOutput;
		case SVNAME_COVERAGE:
			return eAttr_MSAACoverage;
		case SVNAME_DEPTH_GREATER_EQUAL:
			return eAttr_DepthOutputGreaterEqual;
		case SVNAME_DEPTH_LESS_EQUAL:
			return eAttr_DepthOutputLessEqual;
	}

	return eAttr_None;
}

string TypeName(CBufferVariableType::Descriptor desc)
{
	string ret;

	char *type = "";
	switch(desc.type)
	{
		case VARTYPE_BOOL:
			type = "bool"; break;
		case VARTYPE_INT:
			type = "int"; break;
		case VARTYPE_FLOAT:
			type = "float"; break;
		case VARTYPE_DOUBLE:
			type = "double"; break;
		case VARTYPE_UINT:
			type = "uint"; break;
		case VARTYPE_UINT8:
			type = "ubyte"; break;
		case VARTYPE_VOID:
			type = "void"; break;
		case VARTYPE_INTERFACE_POINTER:
			type = "interface"; break;
		default:
			RDCERR("Unexpected type in RDEF variable type %d", type);
	}

	if(desc.varClass == CLASS_OBJECT)
		RDCERR("Unexpected object in RDEF variable type");
	else if(desc.varClass == CLASS_INTERFACE_CLASS)
		RDCERR("Unexpected iface class in RDEF variable type");
	else if(desc.varClass == CLASS_INTERFACE_POINTER)
		ret = type;
	else if(desc.varClass == CLASS_STRUCT)
		ret = "<unnamed>";
	else
	{
		char buf[64] = {0};

		if(desc.rows > 1)
		{
			StringFormat::snprintf(buf, 63, "%s%dx%d", type, desc.rows, desc.cols);

			if(desc.varClass == CLASS_MATRIX_ROWS)
			{
				ret = "row_major ";
				ret += buf;
			}
			else
			{
				ret = buf;
			}
		}
		else if(desc.cols > 1)
		{
			StringFormat::snprintf(buf, 63, "%s%d", type, desc.cols);

			ret = buf;
		}
		else
		{
			ret = type;
		}
	}

	return ret;
}

CBufferVariableType DXBCFile::ParseRDEFType(RDEFHeader *h, char *chunkContents, uint32_t typeOffset)
{
	if(m_Variables.find(typeOffset) != m_Variables.end())
		return m_Variables[typeOffset];

	RDEFCBufferType *type = (RDEFCBufferType *)(chunkContents + typeOffset);

	CBufferVariableType ret;

	ret.descriptor.varClass = (VariableClass)type->varClass;
	ret.descriptor.cols = type->cols;
	ret.descriptor.elements = type->numElems;
	ret.descriptor.members = type->numMembers;
	ret.descriptor.rows = type->rows;
	ret.descriptor.type = (VariableType)type->varType;
	
	ret.descriptor.name = TypeName(ret.descriptor);

	if(ret.descriptor.name == "interface")
	{
		if(h->targetVersion >= 0x500 && type->nameOffset > 0)
		{
			ret.descriptor.name += " " + string(chunkContents + type->nameOffset);
		}
		else
		{
			char buf[64] = {0};
			StringFormat::snprintf(buf, 63, "unnamed_iface_0x%08x", typeOffset);
			ret.descriptor.name += " " + string(buf);
		}
	}
	
	// rename unnamed structs to have valid identifiers as type name
	if(ret.descriptor.name.find("<unnamed>") != string::npos)
	{
		if(h->targetVersion >= 0x500 && type->nameOffset > 0)
		{
			ret.descriptor.name = chunkContents + type->nameOffset;
		}
		else
		{
			char buf[64] = {0};
			StringFormat::snprintf(buf, 63, "unnamed_struct_0x%08x", typeOffset);
			ret.descriptor.name = buf;
		}
	}

	if(type->memberOffset)
	{
		RDEFCBufferChildType *members = (RDEFCBufferChildType *)(chunkContents + type->memberOffset);

		ret.members.reserve(type->numMembers);

		ret.descriptor.bytesize = 0;

		for(int32_t j=0; j < type->numMembers; j++)
		{
			CBufferVariable v;

			v.name = chunkContents + members[j].nameOffset;
			v.type = ParseRDEFType(h, chunkContents, members[j].typeOffset);
			v.descriptor.offset = members[j].memberOffset;
			
			ret.descriptor.bytesize += v.type.descriptor.bytesize;

			// N/A
			v.descriptor.flags = 0;
			v.descriptor.startTexture = 0;
			v.descriptor.numTextures = 0;
			v.descriptor.startSampler = 0;
			v.descriptor.numSamplers = 0;
			v.descriptor.defaultValue.clear();

			ret.members.push_back(v);
		}
	}
	else
	{
		// matrices take up a full vector for each column or row depending which is major, regardless of the other dimension
		if(ret.descriptor.varClass == CLASS_MATRIX_COLUMNS)
			ret.descriptor.bytesize = TypeByteSize(ret.descriptor.type)*ret.descriptor.cols*4*RDCMAX(1U,ret.descriptor.elements);
		else if(ret.descriptor.varClass == CLASS_MATRIX_ROWS)
			ret.descriptor.bytesize = TypeByteSize(ret.descriptor.type)*ret.descriptor.rows*4*RDCMAX(1U,ret.descriptor.elements);
		else
			ret.descriptor.bytesize = TypeByteSize(ret.descriptor.type)*ret.descriptor.rows*ret.descriptor.cols*RDCMAX(1U,ret.descriptor.elements);
	}

	m_Variables[typeOffset] = ret;
	return ret;
}

bool DXBCFile::CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength)
{
	FileHeader *header = (FileHeader *)ByteCode;

	char *data = (char *)ByteCode; // just for convenience

	if(header->fourcc != FOURCC_DXBC)
		return false;

	if(header->fileLength != (uint32_t)ByteCodeLength)
		return false;

	uint32_t *chunkOffsets = (uint32_t *)(header+1); // right after the header

	for (uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
	{
		uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

		if(*fourcc == FOURCC_SDBG)
		{
			return true;
		}
		else if(*fourcc == FOURCC_SPDB)
		{
			return true;
		}
	}

	return false;
}

string DXBCFile::GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength)
{
	string debugPath;
	FileHeader *header = (FileHeader *)ByteCode;

	char *data = (char *)ByteCode; // just for convenience

	if(header->fourcc != FOURCC_DXBC)
		return debugPath;

	if(header->fileLength != (uint32_t)ByteCodeLength)
		return debugPath;

	uint32_t *chunkOffsets = (uint32_t *)(header+1); // right after the header

	for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
	{
		uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

		if(*fourcc == FOURCC_PRIV)
		{
			PRIVHeader *privHeader = (PRIVHeader *)fourcc;
			if(privHeader->debugInfoGUID == PRIVHeader::RENDERDOC_ShaderDebugMagicValue)
			{
				const char* pathData = (char*)&privHeader->data;
				size_t pathLength = strnlen(pathData, privHeader->chunkLength);

				if(privHeader->chunkLength == (sizeof(GUID) + pathLength + 1))
				{
					debugPath.append(pathData, pathData + pathLength);
					return debugPath;
				}
			}
		}
	}

	return debugPath;
}

DXBCFile::DXBCFile(const void *ByteCode, size_t ByteCodeLength)
{
	m_DebugInfo = NULL;

	m_Disassembled = false;

	RDCASSERT(ByteCodeLength < UINT32_MAX);

	m_ShaderBlob.resize(ByteCodeLength);
	memcpy(&m_ShaderBlob[0], ByteCode, m_ShaderBlob.size());

	char *data = (char *)&m_ShaderBlob[0]; // just for convenience

	FileHeader *header = (FileHeader *)&m_ShaderBlob[0];

	if(header->fourcc != FOURCC_DXBC)
		return;

	if(header->fileLength != (uint32_t)ByteCodeLength)
		return;

	// default to vertex shader to support blobs without RDEF chunks (e.g. used with
	// input layouts if they're super stripped down)
	m_Type = D3D11_ShaderType_Vertex;

	bool rdefFound = false;

	uint32_t *chunkOffsets = (uint32_t *)(header+1); // right after the header

	for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
	{
		uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
		uint32_t *chunkSize = (uint32_t *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t));

		char *chunkContents = (char *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t)*2);

		if(*fourcc == FOURCC_RDEF)
		{
			RDEFHeader *h = (RDEFHeader *)fourcc;

			rdefFound = true;

			if(h->targetVersion >= 0x500)
			{
				RDCASSERT(h->unknown[0] == FOURCC_RD11);
			}

			if(h->targetShaderStage == 0xffff)
				m_Type = D3D11_ShaderType_Pixel;
			else if(h->targetShaderStage == 0xfffe)
				m_Type = D3D11_ShaderType_Vertex;
			
			else if(h->targetShaderStage == 0x4753) // 'GS'
				m_Type = D3D11_ShaderType_Geometry;

			else if(h->targetShaderStage == 0x4853) // 'HS'
				m_Type = D3D11_ShaderType_Hull;
			else if(h->targetShaderStage == 0x4453) // 'DS'
				m_Type = D3D11_ShaderType_Domain;
			else if(h->targetShaderStage == 0x4353) // 'CS'
				m_Type = D3D11_ShaderType_Compute;

			m_Resources.reserve(h->resources.count);

			// we have to use this map to match up cbuffers to their bind point, as
			// it's not guaranteed that the resources and cbuffers will come in the
			// same order. However it's possible for two cbuffers to have the same
			// name, so in that case we assume they will come in matching order
			// and just append _ to subsequent cbuffers with the same name.
			map<string, uint32_t> cbufferSlots;
			uint32_t maxCBufferSlot = 0;

			set<string> cbuffernames;

			for(int32_t i = 0; i < h->resources.count; i++)
			{
				RDEFResource *res = (RDEFResource *)(chunkContents + h->resources.offset + i*sizeof(RDEFResource));

				ShaderInputBind desc;

				desc.name = chunkContents + res->nameOffset;
				desc.type = (ShaderInputBind::InputType)res->type;
				desc.bindPoint = res->bindPoint;
				desc.bindCount = res->bindCount;
				desc.flags = res->flags;
				desc.retType = (ShaderInputBind::RetType)res->retType;
				desc.dimension = (ShaderInputBind::Dimension)res->dimension;
				desc.numSamples = res->sampleCount;

				if(desc.numSamples == ~0 &&
					desc.retType != ShaderInputBind::RETTYPE_MIXED &&
					desc.retType != ShaderInputBind::RETTYPE_UNKNOWN &&
					desc.retType != ShaderInputBind::RETTYPE_CONTINUED)
				{
					// uint, uint2, uint3, uint4 seem to be in these bits of flags.
					desc.numSamples = 1 + ((desc.flags&0xC) >> 2);
				}

				if(desc.type == ShaderInputBind::TYPE_CBUFFER)
				{
					string cname = desc.name;

					while(cbuffernames.find(cname) != cbuffernames.end())
						cname += "_";

					cbuffernames.insert(cname);

					cbufferSlots[cname] = desc.bindPoint;
					maxCBufferSlot = RDCMAX(maxCBufferSlot, desc.bindPoint);
				}

				m_Resources.push_back(desc);
			}

			// Expand out any array resources. We deliberately place these at the end of the resources
			// array, so that any non-array resources can be picked up first before any arrays.
			//
			// The reason for this is that an array element could refer to an un-used alias in a bind
			// point, and an individual non-array resoruce will always refer to the used alias (an
			// un-used individual resource will be omitted entirely from the reflection
			for(size_t i=0; i < m_Resources.size(); )
			{
				if(m_Resources[i].bindCount > 1)
				{
					ShaderInputBind desc = m_Resources[i];
					m_Resources.erase(m_Resources.begin()+i);

					string rname = desc.name;
					uint32_t arraySize = desc.bindCount;

					desc.bindCount = 1;

					for(uint32_t a=0; a < arraySize; a++)
					{
						desc.name = StringFormat::Fmt("%s[%u]", rname.c_str(), a);
						m_Resources.push_back(desc);
						desc.bindPoint++;
					}

					// continue from the i'th element again since
					// we just removed it.
					continue;
				}

				i++;
			}

			cbuffernames.clear();

			if(h->cbuffers.count > 0)
				m_CBuffers.resize(maxCBufferSlot+1);

			for(int32_t i = 0; i < h->cbuffers.count; i++)
			{
				RDEFCBuffer *cbuf = (RDEFCBuffer *)(chunkContents + h->cbuffers.offset + i*sizeof(RDEFCBuffer));

				CBuffer cb;

				// I have no real justification for this, it seems some cbuffers are included that are
				// empty and have nameOffset = 0, fxc seems to skip them so I'll do the same.
				// See github issue #122
				if(cbuf->nameOffset == 0) continue;

				cb.name = chunkContents + cbuf->nameOffset;

				cb.descriptor.name = chunkContents + cbuf->nameOffset;
				cb.descriptor.byteSize = cbuf->size;
				cb.descriptor.type = (CBuffer::Descriptor::Type)cbuf->type;
				cb.descriptor.flags = cbuf->flags;
				cb.descriptor.numVars = cbuf->variables.count;

				cb.variables.reserve(cbuf->variables.count);

				size_t varStride = sizeof(RDEFCBufferVariable);

				if(h->targetVersion < 0x500)
				{
					size_t extraData = sizeof(((RDEFCBufferVariable *)0)->unknown);

					varStride -= extraData;

					// it seems in rare circumstances, this data is present even for targetVersion < 0x500.
					// use a heuristic to check if the lower stride would cause invalid-looking data
					// for variables. See github issue #122
					if(cbuf->variables.count > 1)
					{
						RDEFCBufferVariable *var = (RDEFCBufferVariable *)(chunkContents + cbuf->variables.offset + varStride);

						if(var->nameOffset > ByteCodeLength)
						{
							varStride += extraData;
						}
					}
				}

				for(int32_t vi = 0; vi < cbuf->variables.count; vi++)
				{
					RDEFCBufferVariable *var = (RDEFCBufferVariable *)(chunkContents + cbuf->variables.offset + vi*varStride);

					RDCASSERT(var->nameOffset < ByteCodeLength);

					CBufferVariable v;

					v.name = chunkContents + var->nameOffset;

					v.descriptor.defaultValue.resize(var->size);

					if(var->defaultValueOffset && var->defaultValueOffset != ~0U)
					{
						memcpy(&v.descriptor.defaultValue[0], chunkContents + var->defaultValueOffset, var->size);
					}

					v.descriptor.name = v.name;
					//v.descriptor.bytesize = var->size; // size with cbuffer padding
					v.descriptor.offset = var->startOffset;
					v.descriptor.flags = var->flags;

					v.descriptor.startTexture = (uint32_t)-1;
					v.descriptor.startSampler = (uint32_t)-1;
					v.descriptor.numSamplers = 0;
					v.descriptor.numTextures = 0;

					v.type = ParseRDEFType(h, chunkContents, var->typeOffset);

					cb.variables.push_back(v);
				}

				string cname = cb.name;

				while(cbuffernames.find(cname) != cbuffernames.end())
					cname += "_";

				cbuffernames.insert(cname);

				if(cb.descriptor.type == CBuffer::Descriptor::TYPE_CBUFFER)
				{
					RDCASSERT(cbufferSlots.find(cname) != cbufferSlots.end());
					m_CBuffers[cbufferSlots[cname]] = cb;
				}
				else if(cb.descriptor.type == CBuffer::Descriptor::TYPE_RESOURCE_BIND_INFO)
				{
					RDCASSERT(cb.variables.size() == 1 && cb.variables[0].name == "$Element");
					m_ResourceBinds[cb.name] = cb.variables[0].type;
				}
				else if(cb.descriptor.type == CBuffer::Descriptor::TYPE_INTERFACE_POINTERS)
				{
					m_Interfaces = cb;
				}
				else
				{
					RDCDEBUG("Unused information, buffer %d: %s", cb.descriptor.type, cb.descriptor.name.c_str());
				}
			}
		}
		else if(*fourcc == FOURCC_STAT)
		{
			if( *chunkSize == STATSizeDX10 )
			{
				memcpy(&m_ShaderStats, chunkContents, STATSizeDX10);
				m_ShaderStats.version = ShaderStatistics::STATS_DX10;
			}
			else if( *chunkSize == STATSizeDX11 )
			{
				memcpy(&m_ShaderStats, chunkContents, STATSizeDX11);
				m_ShaderStats.version = ShaderStatistics::STATS_DX11;
			}
			else
			{
				RDCERR("Unexpected Unexpected STAT chunk version");
			}
		}
		else if(*fourcc == FOURCC_SHEX || *fourcc == FOURCC_SHDR)
		{
			m_HexDump.resize(*chunkSize / sizeof(uint32_t));
			memcpy((uint32_t *)&m_HexDump[0], chunkContents, *chunkSize);
		}
	}

	// get type/version that's used regularly and cheap to fetch
	FetchTypeVersion();

	// didn't find an rdef means reflection information was stripped.
	// Attempt to reverse engineer basic info from declarations
	if(!rdefFound)
	{
		// need to disassemble now to guess resources
		DisassembleHexDump();

		GuessResources();
	}
	
	for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
	{
		uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
		//uint32_t *chunkSize = (uint32_t *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t));

		char *chunkContents = (char *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t)*2);

		if(*fourcc == FOURCC_ISGN || *fourcc == FOURCC_OSGN || *fourcc == FOURCC_OSG5 || *fourcc == FOURCC_PCSG)
		{
			SIGNHeader *sign = (SIGNHeader *)fourcc;

			vector<SigParameter> *sig = NULL;

			bool input = false;
			bool output = false;
			bool patch = false;

			if(*fourcc == FOURCC_ISGN)
			{
				sig = &m_InputSig;
				input = true;
			}
			if(*fourcc == FOURCC_OSGN || *fourcc == FOURCC_OSG5)
			{
				sig = &m_OutputSig;
				output = true;
			}
			if(*fourcc == FOURCC_PCSG)
			{
				sig = &m_PatchConstantSig;
				patch = true;
			}

			RDCASSERT(sig && sig->empty());

			SIGNElement *el = (SIGNElement *)(sign+1);
			SIGNElement7 *el7 = (SIGNElement7 *)el;
			
			for(uint32_t signIdx = 0; signIdx < sign->numElems; signIdx++)
			{
				SigParameter desc;

				if(*fourcc == FOURCC_OSG5)
				{
					desc.stream = el7->stream;

					el = &el7->elem;
				}
				
				ComponentType compType = (ComponentType)el->componentType;
				desc.compType = eCompType_Float;
				if(compType == COMPONENT_TYPE_UINT32)
					desc.compType = eCompType_UInt;
				else if(compType == COMPONENT_TYPE_SINT32)
					desc.compType = eCompType_SInt;
				else if(compType != COMPONENT_TYPE_FLOAT32)
					RDCERR("Unexpected component type in signature");

				desc.regChannelMask = (uint8_t)(el->mask&0xff);
				desc.channelUsedMask = (uint8_t)(el->rwMask&0xff);
				desc.regIndex = el->registerNum;
				desc.semanticIndex = el->semanticIdx;
				desc.semanticName = chunkContents + el->nameOffset;
				desc.systemValue = GetSystemValue(el->systemType);
				desc.compCount = 
					(desc.regChannelMask & 0x1 ? 1 : 0) +
					(desc.regChannelMask & 0x2 ? 1 : 0) +
					(desc.regChannelMask & 0x4 ? 1 : 0) +
					(desc.regChannelMask & 0x8 ? 1 : 0);

				RDCASSERT(m_Type != (D3D11_ShaderType)-1);

				// pixel shader outputs with registers are always targets
				if(m_Type == D3D11_ShaderType_Pixel && output && desc.systemValue == eAttr_None &&
					desc.regIndex >= 0 && desc.regIndex <= 16)
					desc.systemValue = eAttr_ColourOutput;

				// check system value semantics
				if(desc.systemValue == eAttr_None)
				{
					if(!_stricmp(desc.semanticName.elems, "SV_Position"))
						desc.systemValue = eAttr_Position;
					if(!_stricmp(desc.semanticName.elems, "SV_ClipDistance"))
						desc.systemValue = eAttr_ClipDistance;
					if(!_stricmp(desc.semanticName.elems, "SV_CullDistance"))
						desc.systemValue = eAttr_CullDistance;
					if(!_stricmp(desc.semanticName.elems, "SV_RenderTargetArrayIndex"))
						desc.systemValue = eAttr_RTIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_ViewportArrayIndex"))
						desc.systemValue = eAttr_ViewportIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_VertexID"))
						desc.systemValue = eAttr_VertexIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_PrimitiveID"))
						desc.systemValue = eAttr_PrimitiveIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_InstanceID"))
						desc.systemValue = eAttr_InstanceIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_DispatchThreadID"))
						desc.systemValue = eAttr_DispatchThreadIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_GroupID"))
						desc.systemValue = eAttr_GroupIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_GroupIndex"))
						desc.systemValue = eAttr_GroupFlatIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_GroupThreadID"))
						desc.systemValue = eAttr_GroupThreadIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_GSInstanceID"))
						desc.systemValue = eAttr_GSInstanceIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_OutputControlPointID"))
						desc.systemValue = eAttr_OutputControlPointIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_DomainLocation"))
						desc.systemValue = eAttr_DomainLocation;
					if(!_stricmp(desc.semanticName.elems, "SV_IsFrontFace"))
						desc.systemValue = eAttr_IsFrontFace;
					if(!_stricmp(desc.semanticName.elems, "SV_SampleIndex"))
						desc.systemValue = eAttr_MSAASampleIndex;
					if(!_stricmp(desc.semanticName.elems, "SV_TessFactor"))
						desc.systemValue = eAttr_OuterTessFactor;
					if(!_stricmp(desc.semanticName.elems, "SV_InsideTessFactor"))
						desc.systemValue = eAttr_InsideTessFactor;
					if(!_stricmp(desc.semanticName.elems, "SV_Target"))
						desc.systemValue = eAttr_ColourOutput;
					if(!_stricmp(desc.semanticName.elems, "SV_Depth"))
						desc.systemValue = eAttr_DepthOutput;
					if(!_stricmp(desc.semanticName.elems, "SV_Coverage"))
						desc.systemValue = eAttr_MSAACoverage;
					if(!_stricmp(desc.semanticName.elems, "SV_DepthGreaterEqual"))
						desc.systemValue = eAttr_DepthOutputGreaterEqual;
					if(!_stricmp(desc.semanticName.elems, "SV_DepthLessEqual"))
						desc.systemValue = eAttr_DepthOutputLessEqual;
				}

				RDCASSERT(desc.systemValue != eAttr_None || desc.regIndex >= 0);

				sig->push_back(desc);

				el++;
				el7++;
			}
			
			for(uint32_t i = 0; i < sign->numElems; i++)
			{
				SigParameter &a = (*sig)[i];

				for(uint32_t j = 0; j < sign->numElems; j++)
				{
					SigParameter &b = (*sig)[j];
					if(i != j && !strcmp(a.semanticName.elems, b.semanticName.elems))
					{
						a.needSemanticIndex = true;
						break;
					}
				}

				string semanticIdxName = a.semanticName.elems;
				if(a.needSemanticIndex)
					semanticIdxName += ToStr::Get(a.semanticIndex);

				a.semanticIdxName = semanticIdxName;
			}
		}
		else if(*fourcc == FOURCC_Aon9) // 10Level9 most likely
		{
			char *c = (char *)fourcc;
			RDCWARN("Unknown chunk: %c%c%c%c", c[0], c[1], c[2], c[3]);
		}
		else if(*fourcc == FOURCC_SDBG)
		{
			m_DebugInfo = new SDBGChunk(fourcc);
		}
		else if(*fourcc == FOURCC_SPDB)
		{
			m_DebugInfo = new SPDBChunk(fourcc);
		}
	}
}

void DXBCFile::GuessResources()
{
	char buf[64] = {0};

	for(size_t i=0; i < m_Declarations.size(); i++)
	{
		ASMDecl &dcl = m_Declarations[i];

		switch(dcl.declaration)
		{
			case OPCODE_DCL_SAMPLER:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_SAMPLER);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "sampler%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_SAMPLER;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = dcl.samplerMode == SAMPLER_MODE_COMPARISON ? 2 : 0;
				desc.retType = ShaderInputBind::RETTYPE_UNKNOWN;
				desc.dimension = ShaderInputBind::DIM_UNKNOWN;
				desc.numSamples = 0;

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_RESOURCE:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "texture%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_TEXTURE;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 0;
				desc.retType = (ShaderInputBind::RetType)dcl.resType[0];
				desc.dimension = dcl.dim == RESOURCE_DIMENSION_BUFFER ? ShaderInputBind::DIM_BUFFER :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE1D ? ShaderInputBind::DIM_TEXTURE1D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2D ? ShaderInputBind::DIM_TEXTURE2D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE3D ? ShaderInputBind::DIM_TEXTURE3D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURECUBE ? ShaderInputBind::DIM_TEXTURECUBE :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE1DARRAY ? ShaderInputBind::DIM_TEXTURE1DARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY ? ShaderInputBind::DIM_TEXTURE2DARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURECUBEARRAY ? ShaderInputBind::DIM_TEXTURECUBEARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DMS ? ShaderInputBind::DIM_TEXTURE2DMS :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY ? ShaderInputBind::DIM_TEXTURE2DARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY ? ShaderInputBind::DIM_TEXTURE2DMSARRAY :
								 ShaderInputBind::DIM_UNKNOWN;
				desc.numSamples = dcl.sampleCount;

				RDCASSERT(desc.dimension != ShaderInputBind::DIM_UNKNOWN);

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_RESOURCE_RAW:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_RESOURCE || dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "bytebuffer%u", idx);

				desc.name = buf;
				desc.type = dcl.operand.type == TYPE_RESOURCE ? ShaderInputBind::TYPE_BYTEADDRESS : ShaderInputBind::TYPE_UAV_RWBYTEADDRESS;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 0;
				desc.retType = ShaderInputBind::RETTYPE_MIXED;
				desc.dimension = ShaderInputBind::DIM_BUFFER;
				desc.numSamples = 0;

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_RESOURCE_STRUCTURED:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "structuredbuffer%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_STRUCTURED;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 0;
				desc.retType = ShaderInputBind::RETTYPE_MIXED;
				desc.dimension = ShaderInputBind::DIM_BUFFER;
				desc.numSamples = dcl.stride;

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "uav%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_UAV_RWSTRUCTURED; // doesn't seem to be anything that determines append vs consume vs rwstructured
				if(dcl.hasCounter)
					desc.type = ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 0;
				desc.retType = ShaderInputBind::RETTYPE_MIXED;
				desc.dimension = ShaderInputBind::DIM_BUFFER;
				desc.numSamples = dcl.stride;

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
				RDCASSERT(dcl.operand.indices.size() == 1);
				RDCASSERT(dcl.operand.indices[0].absolute);
				
				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

				StringFormat::snprintf(buf, 63, "uav%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_UAV_RWTYPED;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 0;
				desc.retType = (ShaderInputBind::RetType)dcl.resType[0];
				desc.dimension = dcl.dim == RESOURCE_DIMENSION_TEXTURE1D ? ShaderInputBind::DIM_TEXTURE1D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2D ? ShaderInputBind::DIM_TEXTURE2D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE3D ? ShaderInputBind::DIM_TEXTURE3D :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURECUBE ? ShaderInputBind::DIM_TEXTURECUBE :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE1DARRAY ? ShaderInputBind::DIM_TEXTURE1DARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY ? ShaderInputBind::DIM_TEXTURE2DARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURECUBEARRAY ? ShaderInputBind::DIM_TEXTURECUBEARRAY :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DMS ? ShaderInputBind::DIM_TEXTURE2DMS :
								 dcl.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY ? ShaderInputBind::DIM_TEXTURE2DARRAY :
								 ShaderInputBind::DIM_UNKNOWN;
				desc.numSamples = (uint32_t)-1;

				m_Resources.push_back(desc);

				break;
			}
			case OPCODE_DCL_CONSTANT_BUFFER:
			{
				ShaderInputBind desc;

				RDCASSERT(dcl.operand.type == TYPE_CONSTANT_BUFFER);
				RDCASSERT(dcl.operand.indices.size() == 2);
				RDCASSERT(dcl.operand.indices[0].absolute && dcl.operand.indices[1].absolute);

				uint32_t idx = (uint32_t)dcl.operand.indices[0].index;
				uint32_t numVecs = (uint32_t)dcl.operand.indices[1].index;

				StringFormat::snprintf(buf, 63, "cbuffer%u", idx);

				desc.name = buf;
				desc.type = ShaderInputBind::TYPE_CBUFFER;
				desc.bindPoint = idx;
				desc.bindCount = 1;
				desc.flags = 1;
				desc.retType = ShaderInputBind::RETTYPE_UNKNOWN;
				desc.dimension = ShaderInputBind::DIM_UNKNOWN;
				desc.numSamples = 0;

				m_Resources.push_back(desc);

				CBuffer cb;

				cb.name = desc.name;

				cb.descriptor.name = cb.name;
				cb.descriptor.byteSize = numVecs*4;
				cb.descriptor.type = CBuffer::Descriptor::TYPE_CBUFFER;
				cb.descriptor.flags = 0;
				cb.descriptor.numVars = numVecs;

				cb.variables.reserve(numVecs);

				for(uint32_t v = 0; v < numVecs; v++)
				{
					CBufferVariable var;

					StringFormat::snprintf(buf, 63, "cb%u_v%u", desc.bindPoint, v);

					var.name = buf;

					var.descriptor.defaultValue.resize(4*sizeof(float));

					var.descriptor.name = var.name;
					var.descriptor.offset = 4*sizeof(float)*v;
					var.descriptor.flags = 0;

					var.descriptor.startTexture = (uint32_t)-1;
					var.descriptor.startSampler = (uint32_t)-1;
					var.descriptor.numSamplers = 0;
					var.descriptor.numTextures = 0;

					var.type.descriptor.bytesize = 4*sizeof(float);
					var.type.descriptor.rows = 1;
					var.type.descriptor.cols = 4;
					var.type.descriptor.elements = 0;
					var.type.descriptor.members = 0;
					var.type.descriptor.type = VARTYPE_FLOAT;
					var.type.descriptor.varClass = CLASS_VECTOR;
					var.type.descriptor.name = TypeName(var.type.descriptor);

					cb.variables.push_back(var);
				}

				m_CBuffers.resize(RDCMAX((uint32_t)m_CBuffers.size(), desc.bindPoint+1));
				m_CBuffers[desc.bindPoint] = cb;

				break;
			}
		}
	}
}

SDBGChunk::SDBGChunk(void *data)
{
	m_HasDebugInfo = false;

	{
		uint32_t *raw = (uint32_t *)data;

		if(raw[0] != FOURCC_SDBG)
			return;

		m_RawData.resize(raw[1]);
		memcpy(&m_RawData[0], &raw[2], m_RawData.size());
	}

	char *dbgData = (char *)&m_RawData[0];

	m_Header = *( (SDBGHeader *)dbgData );

	m_ShaderFlags = m_Header.shaderFlags;

	char *dbgPostHeader = dbgData+sizeof(SDBGHeader);

	SDBGFileHeader *FileHeaders = (SDBGFileHeader *)(dbgPostHeader+m_Header.files.offset);
	SDBGAsmInstruction *Instructions = (SDBGAsmInstruction *)(dbgPostHeader+m_Header.instructions.offset);
	SDBGVariable *Variables = (SDBGVariable *)(dbgPostHeader+m_Header.variables.offset);
	SDBGInputRegister *Inputs = (SDBGInputRegister *)(dbgPostHeader+m_Header.inputRegisters.offset);
	SDBGSymbol *SymbolTable = (SDBGSymbol *)(dbgPostHeader+m_Header.symbolTable.offset);
	SDBGScope *Scopes = (SDBGScope *)(dbgPostHeader+m_Header.scopes.offset);
	SDBGType *Types = (SDBGType *)(dbgPostHeader+m_Header.types.offset);
	int32_t *Int32DB = (int32_t *)(dbgPostHeader+m_Header.int32DBOffset);

	m_FileHeaders = vector<SDBGFileHeader>(FileHeaders, FileHeaders + m_Header.files.count);
	m_Instructions = vector<SDBGAsmInstruction>(Instructions, Instructions + m_Header.instructions.count);
	m_Variables = vector<SDBGVariable>(Variables, Variables + m_Header.variables.count);
	m_Inputs = vector<SDBGInputRegister>(Inputs, Inputs + m_Header.inputRegisters.count);
	m_SymbolTable = vector<SDBGSymbol>(SymbolTable, SymbolTable + m_Header.symbolTable.count);
	m_Scopes = vector<SDBGScope>(Scopes, Scopes + m_Header.scopes.count);
	m_Types = vector<SDBGType>(Types, Types + m_Header.types.count);
	m_Int32Database = vector<int32_t>(Int32DB, Int32DB + (m_Header.asciiDBOffset-m_Header.int32DBOffset)/sizeof(int32_t));

	char *asciiDatabase = dbgPostHeader+m_Header.asciiDBOffset;

	m_CompilerSig = asciiDatabase+m_Header.compilerSigOffset;
	m_Profile = asciiDatabase+m_Header.profileOffset;
	m_Entry = asciiDatabase+m_Header.entryFuncOffset;

	for(size_t i=0; i < m_FileHeaders.size(); i++)
	{
		string filename = string(asciiDatabase+m_FileHeaders[i].filenameOffset, m_FileHeaders[i].filenameLen);
		string source = string(asciiDatabase+m_FileHeaders[i].sourceOffset, m_FileHeaders[i].sourceLen);

		this->Files.push_back(make_pair(filename, source));
	}

	// successful grab of info
	m_HasDebugInfo = true;
}

void SDBGChunk::GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx, int32_t &lineNum) const
{
	if(instruction < m_Instructions.size())
	{
		int32_t symID = m_Instructions[instruction].symbol;
		if(symID > 0 && symID < (int32_t)m_SymbolTable.size())
		{
			const SDBGSymbol &sym = m_SymbolTable[symID];

			fileIdx = sym.fileID;
			lineNum = sym.lineNum-1;
		}
	}
}

string SDBGChunk::GetSymbolName(int symbolID)
{
	RDCASSERT(symbolID >= 0 && symbolID < (int)m_SymbolTable.size());

	SDBGSymbol &sym = m_SymbolTable[symbolID];

	return GetSymbolName(sym.symbol.offset, sym.symbol.count);
}

string SDBGChunk::GetSymbolName(int32_t symbolOffset, int32_t symbolLength)
{
	RDCASSERT(symbolOffset < m_Header.compilerSigOffset);
	RDCASSERT(symbolOffset+symbolLength <= m_Header.compilerSigOffset);
	
	int32_t offset = sizeof(m_Header)+m_Header.asciiDBOffset+symbolOffset;

	return string(&m_RawData[offset], symbolLength);
}

uint32_t ReadVarLenUInt(byte *&s)
{
	// check top two bits. 0b00 (or 0b01) means we just return the byte value.
	// 0b10 means we take this byte as high-end byte and next as the low-end
	// in a word. (with top two bits masked off)
	// 0b11 similar, but for a dword.

	if((*s & 0xC0) == 0x00 || (*s & 0xC0) == 0x40)
	{
		return (uint32_t)*(s++);
	}
	else if((*s & 0xC0) == 0x80)
	{
		byte first = *(s++); first &= 0x7f;
		byte second = *(s++);

		return (uint32_t(first)<<8) | uint32_t(second);
	}
	else if((*s & 0xC0) == 0xC0)
	{
		byte first = *(s++); first &= 0x3f;
		byte second = *(s++);
		byte third = *(s++);
		byte fourth = *(s++);

		return (uint32_t(first)<<24) | (uint32_t(second)<<16) | (uint32_t(third)<<8) | uint32_t(fourth);
	}
	else
	{
		// impossible
		return 0;
	}
}

SPDBChunk::SPDBChunk(void *chunk)
{
	m_HasDebugInfo = false;

	uint32_t firstInstructionOffset = 0;

	byte *data = NULL;

	uint32_t spdblength;
	{
		uint32_t *raw = (uint32_t *)chunk;

		if(raw[0] != FOURCC_SPDB)
			return;

		spdblength = raw[1];

		data = (byte *)&raw[2];
	}
	
	m_ShaderFlags = 0;

	FileHeaderPage *header = (FileHeaderPage *)data;

	if(memcmp(header->identifier, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0", sizeof(header->identifier)))
	{
		RDCWARN("Unexpected SPDB type");
		return;
	}
	
	RDCASSERT(header->PageCount*header->PageSize == spdblength);
	
	const byte **pages = new const byte*[header->PageCount];
	for(uint32_t i=0; i < header->PageCount; i++)
		pages[i] = &data[i*header->PageSize];
	
	uint32_t rootdirCount = header->PagesForByteSize(header->RootDirSize);
	uint32_t rootDirIndicesCount = header->PagesForByteSize(rootdirCount*sizeof(uint32_t));

	PageMapping rootdirIndicesMapping(pages, header->PageSize, header->RootDirectory, rootDirIndicesCount);
	const byte *rootdirIndices = rootdirIndicesMapping.Data();
	
	PageMapping directoryMapping(pages, header->PageSize, (uint32_t *)rootdirIndices, rootdirCount);
	const uint32_t *dirContents = (const uint32_t *)directoryMapping.Data();
	
	vector<PDBStream> streams;

	streams.resize(*dirContents); dirContents++;

	for(size_t i=0; i < streams.size(); i++)
	{
		streams[i].byteLength = *dirContents; dirContents++;
	}

	for(size_t i=0; i < streams.size(); i++)
	{
		if(streams[i].byteLength == 0) continue;

		for(uint32_t p=0; p < header->PagesForByteSize(streams[i].byteLength); p++)
		{
			streams[i].pageIndices.push_back(*dirContents); dirContents++;
		}
	}

	RDCASSERT(streams.size() > 1);
	
	// stream 1: GUID + stream names
	PageMapping guidMapping(pages, header->PageSize, &streams[1].pageIndices[0], (uint32_t)streams[1].pageIndices.size());
	GuidPageHeader *guid = (GuidPageHeader *)guidMapping.Data();

	uint32_t *hashtable = (uint32_t *)(guid->Strings + guid->StringBytes);

	uint32_t numSetBits		= hashtable[0]; hashtable++;
	uint32_t maxBit			= hashtable[0]; hashtable++;
	uint32_t setBitsetWords = hashtable[0]; hashtable++;
	uint32_t *setBitset		= hashtable;    hashtable += setBitsetWords;
	RDCASSERT(hashtable[0] == 0);			hashtable++;

	map<string,uint32_t> StreamNames;

	uint32_t numset=0;
	for(uint32_t i=0; i < maxBit; i++)
	{
		if((setBitset[ (i/32) ] & (1 << (i%32) )) != 0)
		{
			uint32_t strOffs = hashtable[0]; hashtable++;
			uint32_t stream  = hashtable[0]; hashtable++;

			char *streamName = guid->Strings + strOffs;

			StreamNames[streamName] = stream;

			numset++;
		}
	}
	RDCASSERT(numset == numSetBits);

	for(auto it=StreamNames.begin(); it != StreamNames.end(); ++it)
	{
		if(!strncmp(it->first.c_str(), "/src/files/", 11))
		{
			PDBStream &s = streams[it->second];
			PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0], (uint32_t)s.pageIndices.size());

			char *filename = (char *)it->first.c_str();
			filename += sizeof("/src/files/")-1;

			Files.push_back( make_pair(filename, (char *)fileContents.Data()) );
		}
	}
	
	vector<Function> functions;

	if(streams.size() >= 5)
	{
		PDBStream &s = streams[4];
		PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0], (uint32_t)s.pageIndices.size());

		byte *bytes = (byte *)fileContents.Data();
		byte *end = bytes + s.byteLength;

		// seems to be accurate, but we'll just iterate to end
		//uint32_t *u32 = (uint32_t *)bytes;
		//uint32_t numFuncs = u32[6];

		// skip header
		bytes += 57;

		while(bytes < end)
		{
			Function f;
			memcpy(&f, bytes, 11); bytes += 11;
			f.funcName = (const char *)bytes; bytes += 1+f.funcName.length();

			while(*bytes) { f.things.push_back(*(int8_t*)bytes); bytes++; }

			functions.push_back(f);
		}
	}

	{
		Function mainFunc;
		mainFunc.funcName = "entrypoint";

		functions.push_back(mainFunc);
	}

	map<uint32_t,string> Names;

	if(StreamNames.find("/names") != StreamNames.end())
	{
		PDBStream &s = streams[StreamNames["/names"]];
		PageMapping namesMapping(pages, header->PageSize, &s.pageIndices[0], (uint32_t)s.pageIndices.size());
		const uint32_t *contents = (const uint32_t *)namesMapping.Data();

		RDCASSERT(contents[0] == 0xeffeeffe && contents[1] == 1);

		uint32_t StringBytes = contents[2];
		char *Strings = (char *)&contents[3];

		contents += 3;

		contents = (uint32_t *)((byte *)contents + StringBytes);

		uint32_t numHashes = contents[0]; contents++;

		for(uint32_t i=0; i < numHashes; i++)
		{
			uint32_t idx = contents[0]; contents++;

			if(idx != 0)
				Names[idx] = Strings+idx;
		}
	}
	
	vector<DBIModule> modules;
	
	{
		PageMapping dbiMapping(pages, header->PageSize, &streams[3].pageIndices[0], (uint32_t)streams[3].pageIndices.size());
		DBIHeader *dbi = (DBIHeader *)dbiMapping.Data();
		
		RDCASSERT(dbi->sig == 0xffffffff);
		RDCASSERT(dbi->ver == 19990903);

		byte *cur = (byte *)(dbi+1);
		byte *end = cur + dbi->gpmodiSize;
		while(cur < end)
		{
			DBIModule *mod = (DBIModule *)cur;
			cur += sizeof(DBIModule) - sizeof(string)*2;

			char *moduleName = (char *)cur;
			cur += strlen(moduleName)+1;

			char *objectName = (char *)cur;
			cur += strlen(objectName)+1;

			// align up to DWORD boundary
			while((uintptr_t)cur & 0x3)
				cur++;

			DBIModule m;
			memcpy(&m, mod, sizeof(DBIModule) - sizeof(string)*2);
			m.moduleName = moduleName;
			m.objectName = objectName;

			modules.push_back(m);
		}
		RDCASSERT(cur == end);
	}

	vector<FuncCallLineNumbers> funcCalls;

	map<uint32_t, int32_t> FileMapping; // mapping from hash chunk to index in Files[], or -1
		
	for(size_t m=0; m < modules.size(); m++)
	{
		if(modules[m].stream == -1)
			continue;

		PDBStream &s = streams[modules[m].stream];
		PageMapping modMapping(pages, header->PageSize, &s.pageIndices[0], (uint32_t)s.pageIndices.size());
		uint32_t *moduledata = (uint32_t *)modMapping.Data();
		
		RDCASSERT(moduledata[0] == 4);

		byte *cur = (byte *)&moduledata[1];
		byte *end = (byte *)moduledata + modules[m].cbSyms;
		while(cur < end)
		{
			uint16_t *sym = (uint16_t *)cur;

			uint16_t len = sym[0];
			uint16_t type = sym[1]; len -= sizeof(uint16_t); // len includes type uint16, subtract for ease of use

			cur += sizeof(uint16_t) * 2;

			byte *contents = cur;

			if(type == 0x1110)
			{
				ProcHeader *proc = (ProcHeader *)contents;
				//char *name = (char *)(proc + 1);

				firstInstructionOffset = proc->Offset;

				//RDCDEBUG("Got global procedure start %s %x -> %x", name, proc->Offset, proc->Offset+proc->Length);
			}
			else if(type == 0x113c)
			{
				CompilandDetails *details = (CompilandDetails *)contents;
				char *compilerString = (char *)&details->CompilerSig;

				memcpy(&m_CompilandDetails, details, sizeof(CompilandDetails)-sizeof(string));
				m_CompilandDetails.CompilerSig = compilerString;

				/*
				RDCDEBUG("CompilandDetails: %s (%d.%d.%d.%d)", compilerString,
						details->FrontendVersion.Major, details->FrontendVersion.Minor,
						details->FrontendVersion.Build, details->FrontendVersion.QFE);*/

				// for hlsl/fxc
				//RDCASSERT(details->Language == 16 && details->Platform == 256);
			}
			else if(type == 0x113d)
			{
				// for hlsl/fxc?
				//RDCASSERT(contents[0] == 0x1);
				char *key = (char *)contents + 1;
				while(key[0])
				{
					char *value = key + strlen(key) + 1;

					//RDCDEBUG("CompilandEnv: %s = \"%s\"", key, value);

					if(!strcmp(key, "hlslEntry"))
					{
						m_Entry = value;
					}
					else if(!strcmp(key, "hlslTarget"))
					{
						m_Profile = value;
					}
					else if(!strcmp(key, "hlslFlags"))
					{
						if(value[0] == '0' && value[1] == 'x')
						{
							int i = 2;
							
							m_ShaderFlags = 0;

							while(value[i] != 0)
							{
								uint32_t digit = 0;
								if(value[i] >= '0' && value[i] <= '9')
									digit = (uint32_t)(value[i]-'0');
								if(value[i] >= 'a' && value[i] <= 'f')
									digit = (uint32_t)(value[i]-'a');
								if(value[i] >= 'A' && value[i] <= 'F')
									digit = (uint32_t)(value[i]-'A');

								m_ShaderFlags <<= 4;
								m_ShaderFlags |= digit;

								i++;
							}
						}
					}
					else if(!strcmp(key, "hlslDefines"))
					{
						string cmdlineDefines = "// Command line defines:\n\n";

						char *c = value;

						while(*c)
						{
							// skip whitespace
							while(*c && (*c == ' ' || *c == '\t' || *c == '\n'))
								c++;

							if(*c == 0) break;

							// start of a definition
							if(c[0] == '/' && c[1] == 'D')
							{
								c += 2;
								// skip whitespace
								while(*c && (*c == ' ' || *c == '\t' || *c == '\n')) c++;

								if(*c == 0) break;

								char *defstart = c;
								char *defend = strchr(c, '=');

								if(defend == 0) break;

								c = defend+1;

								char *valstart = c;

								// skip to end or next whitespace
								while(*c && *c != ' ' && *c != '\t' && *c != '\n') c++;

								char *valend = c;

								cmdlineDefines += "#define ";
								cmdlineDefines += string(defstart, defend) + " " + string(valstart, valend);
								cmdlineDefines += "\n";
							}
						}

						Files.push_back(make_pair("@cmdline", cmdlineDefines));
					}
					
					key = value + strlen(value) + 1;
				}
			}
			else if(type == 0x114D)
			{
				//RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));

				FuncCallLineNumbers func;
				func.fileOffs = 0;
				func.baseLineNum = 0;

				byte *iterator = (byte *)contents;
				byte *callend = contents + len;

				//uint32_t *adsf = (uint32_t *)iterator;

				//RDCDEBUG("funcdef for %s (%x) flags??=0x%x offset/length??=0x%x", functions[adsf[2]&0xfff].funcName.c_str(), adsf[2], adsf[0], adsf[1]);
				iterator += 3*sizeof(uint32_t);

				bool working = true;
				uint32_t currentBytes = firstInstructionOffset;
				uint32_t currentLine = 0;
				uint32_t currentColStart = 1;
				uint32_t currentColEnd = 100000;
				
				while(iterator < callend)
				{
					FuncCallBytestreamOpcodes opcode = (FuncCallBytestreamOpcodes)*iterator;
					iterator++;

					if(opcode == PrologueEnd ||
						 opcode == EpilogueBegin)
					{
						//uint32_t value = ReadVarLenUInt(iterator);
						//RDCDEBUG("type %02x: unk=%02x", opcode, value);

						if(opcode == EpilogueBegin)
						{
							//RDCDEBUG("                          (endloc: %04x - %u [%u,%u] )", currentBytes, currentLine, currentColStart, currentColEnd);
							working = false;
						}
					}
					else if(opcode == FunctionEndNoAdvance)
					{
						uint32_t value = ReadVarLenUInt(iterator);
						
						//RDCDEBUG("                      type %02x: %02x: adjust line by 4(?!) & bytes by %x", opcode, value, value);

						if(working)
						{
							InstructionLocation loc;
							loc.offset = currentBytes;
							loc.line = currentLine;
							loc.colStart = currentColStart;
							loc.colEnd = currentColEnd;
							func.locations.push_back(loc);

							loc.offset = currentBytes+ value;
							loc.funcEnd = true;
							func.locations.push_back(loc);

							//RDCDEBUG("                          (loc: %04x - %u [%u,%u] )", currentBytes, currentLine, currentColStart, currentColEnd);
						}

						currentBytes += value;
					}
					else if(opcode == AdvanceBytesAndLines)
					{
						uint32_t value = (uint32_t)*iterator; iterator++;

						uint32_t byteMod = (value &0xf);
						uint32_t lineMod = (value >>4);

						currentBytes += byteMod;
						currentLine += lineMod/2;

						//RDCDEBUG("                      type %02x: %02x: adjust line by %u & bytes by %x", type, value, lineMod/2, byteMod);

					}
					else if(opcode == EndOfFunction)
					{
						//RDCDEBUG("type %02x:", opcode);

						uint32_t retlen = ReadVarLenUInt(iterator);
						uint32_t byteAdv = ReadVarLenUInt(iterator);

						//RDCDEBUG("           retlen=%x, byteAdv=%x", retlen, byteAdv);

						currentBytes += byteAdv;

						if(working)
						{
							InstructionLocation loc;
							loc.offset = currentBytes;
							loc.line = currentLine;
							loc.colStart = currentColStart;
							loc.colEnd = currentColEnd;
							func.locations.push_back(loc);

							loc.offset = currentBytes+retlen;
							loc.funcEnd = true;
							func.locations.push_back(loc);
						}

						currentBytes += retlen;
					}
					else if(opcode == SetByteOffset)
					{
						currentBytes = ReadVarLenUInt(iterator);
						//RDCDEBUG("                      type %02x: start at byte offset %x", opcode, currentBytes);
					}
					else if(opcode == AdvanceBytes)
					{
						uint32_t offs = ReadVarLenUInt(iterator);

						currentBytes += offs;

						//RDCDEBUG("                      type %02x: advance %x bytes", opcode, offs);

						if(working)
						{
							InstructionLocation loc;
							loc.offset = currentBytes;
							loc.line = currentLine;
							loc.colStart = currentColStart;
							loc.colEnd = currentColEnd;
							func.locations.push_back(loc);

							//RDCDEBUG("                          (loc: %04x - %u [%u,%u] )", currentBytes, currentLine, currentColStart, currentColEnd);
						}
					}
					else if(opcode == AdvanceLines)
					{
						uint32_t linesAdv = ReadVarLenUInt(iterator);

						if(linesAdv&0x1)
							currentLine -= (linesAdv/2);
						else
							currentLine += (linesAdv/2);

						//RDCDEBUG("                      type %02x: advance %u (%u) lines", opcode, linesAdv/2, linesAdv);
					}
					else if(opcode == ColumnStart)
					{
						currentColStart = ReadVarLenUInt(iterator);
						//RDCDEBUG("                      type %02x: col < %u", opcode, currentColStart);
					}
					else if(opcode == ColumnEnd)
					{
						currentColEnd = ReadVarLenUInt(iterator);
						//RDCDEBUG("                      type %02x: col > %u", opcode, currentColEnd);
					}
					else if(opcode == EndStream)
					{
						while(*iterator == 0 && iterator < callend) iterator++;
						//RDCASSERT(iterator == callend); // seems like this isn't always true
					}
					else
					{
						RDCDEBUG("Unrecognised: %02x", opcode);
						break;
					}
				}

				if(func.locations.size() == 1)
				{
					// not sure what this means, but it seems to just be intended to match
					// one instruction offset and we don't have an 0xc to 'cap' things off.
					// just insert a dummy location at the same file line but with a slightly
					// higher offset so we have a valid range to match against
					auto loc = func.locations[0];
					loc.offset++;
					func.locations.push_back(loc);
				}

				RDCASSERT(func.locations.size() > 1);
				
				funcCalls.push_back(func);

				//RDCDEBUG("Lost %d bytes after we stopped processing", callend - iterator);
			}
			else if(type == 0x113E)
			{
			// not currently used
			/*
				//RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));
				
				RegisterVariableAssign *var = (RegisterVariableAssign *)contents;

				string funcName = "undefined";

				if((size_t)(var->func&0xfff) < functions.size())
					funcName = functions[var->func&0xfff].funcName;

	 			//RDCDEBUG("     in %s (%x) flags??=%04x, %s:", funcName.c_str(), var->func, var->unkflags, var->name);

				byte *afterName = (byte *)var->name + (strlen(var->name) + 1);

				byte *end = contents + len;

				// seems to always be 0s
				while(afterName < end)
				{
					RDCASSERT(*afterName == 0);
					afterName++;
				}
				*/
			}
			else if(type == 0x1150)
			{
			// not currently used
			/*
				RDCASSERT(len %4 == 0);
				//RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));

				RegisterVariableAssignComponent *comp = (RegisterVariableAssignComponent *)contents;

				OperandType t = comp->Type();
				const char *type = "";
				switch(t)
				{
					case TYPE_TEMP: type = "r"; break;
					case TYPE_INPUT: type = "v"; break;
					case TYPE_OUTPUT: type = "o"; break;
					case TYPE_INDEXABLE_TEMP: type = "x"; break;
					case TYPE_INPUT_THREAD_ID: type = "globalIdx"; break;
					case TYPE_INPUT_THREAD_ID_IN_GROUP: type = "localIdx"; break;
					case TYPE_INPUT_THREAD_GROUP_ID: type = "groupIdx"; break;
					default: break;
				}

				uint16_t destComp = comp->destComp;
				if(len == 24)
					destComp = comp->unkE;
				if(len > 24)
				{
					uint16_t *end = (uint16_t *)(contents + len);

					destComp = end[-1];
				}

				char comps[] = "xyzw";

				//RDCDEBUG("%s%d.%c (%x, %x) <- <above>.%c @ 0x%x", type, (destComp)>>4, comps[(destComp&0xf)>>2], comp->destComp, comp->unkE, comps[(comp->srcComp&0xf)>>2], comp->instrOffset);

	 			//RDCDEBUG("     A:%04x B:%04x C:%04x D:%04x", comp->unkA, comp->unkB, comp->unkC, comp->unkD);
				//RDCDEBUG("     E(d):%04x", comp->unkE);
				
				uint32_t *extra = (uint32_t *)(comp+1);

				for(uint16_t l=20; l < len; l+=4)
				{
					//RDCDEBUG("     %08x", extra[0]);
					extra++;
				}
				*/
			}
			else if(type == 0x114E)
			{
				RDCASSERT(len == 0);
				//RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));
			}
			else if(type == 0x0006)
			{
				//RDCDEBUG("end");
			}
			else
			{
				//RDCDEBUG("(unexpected?) 0x%04x", uint32_t(type));
			}

			cur += len;
		}
		RDCASSERT(cur == end);
		
		end = cur + modules[m].cbLines;

		while(cur < end)
		{
			uint16_t *type = (uint16_t *)cur;

			if(*type == 0xF4) // hash
			{
				uint32_t *len = (uint32_t *)(type+2);

				cur = (byte *)(len + 1);

				byte *start = cur;
				while(cur < start + *len)
				{
					uint32_t *hashData = (uint32_t *)cur; cur += sizeof(uint32_t);
					uint16_t *unknown = (uint16_t *)cur; cur += sizeof(uint16_t);

					uint32_t chunkOffs = uint32_t((byte *)hashData - start);

					uint32_t nameoffset = hashData[0];

					// if this is 0, we don't have a hash
					if(*unknown)
					{
						byte hash[16];
						memcpy(hash, cur, sizeof(hash));
						cur += sizeof(hash);

						int32_t fileIdx = -1;
						
						for(size_t i=0; i < Files.size(); i++)
						{
							if(!_stricmp(Files[i].first.c_str(), Names[nameoffset].c_str()))
							{
								fileIdx = (int32_t)i;
								break;
							}
						}

						FileMapping[chunkOffs] = fileIdx;
					}
					else
					{
						// this is a 'virtual' file. Create a source file that we can map lines to just for something,
						// as we won't be able to reliably get the real source lines back. The PDB lies convincingly about the
						// source according to #line
						if(Names.find(nameoffset) != Names.end())
						{
							string name = Names[nameoffset];
							Files.push_back( make_pair(name, "") );

							FileMapping[chunkOffs] = (int32_t)Files.size()-1;
						}
						else
						{
							RDCERR("Processing SPDB chunk, encountered nameoffset %d that doesn't correspond to any name.", nameoffset);

							FileMapping[chunkOffs] = -1;
						}
					}

					unknown = (uint16_t *)cur; cur += sizeof(uint16_t);
					// another unknown
				}
				RDCASSERT(cur == start + *len);
			}
			else if(*type == 0xF2)
			{
				uint32_t *len = (uint32_t *)(type+2);

				cur = (byte *)(len + 1);

				byte *start = cur;

				LineNumbersHeader *hdr = (LineNumbersHeader *)cur;

				cur = (byte *)(hdr + 1);

				bool hasExtra = (hdr->flags & 0x1);
				
				while(cur < start + *len)
				{
					FileLineNumbers *file = (FileLineNumbers *)cur;
					cur = (byte *)(file + 1);
					
					uint32_t *linedata = (uint32_t *)cur;

					cur += (sizeof(uint32_t) + sizeof(uint32_t)) * file->numLines;
					if(hasExtra)
						cur += (sizeof(uint16_t) + sizeof(uint16_t)) * file->numLines;
					
					int32_t fileIdx = -1;

					if(FileMapping.find(file->fileIdx) == FileMapping.end())
					{
						RDCERR("SPDB chunk - line numbers file references index %u not encountered in file mapping", file->fileIdx);
					}
					else
					{
						fileIdx = FileMapping[file->fileIdx];
					}

					for(uint32_t l=0; l < file->numLines; l++)
					{
						uint32_t offs    = linedata[0];
						uint32_t lineNum = linedata[1]&0x00fffff;
						//uint32_t unknown = linedata[1]>>24;

						linedata += 2;

						m_LineNumbers[offs] = make_pair(fileIdx, lineNum);

						//RDCDEBUG("Offset %x is line %d", offs, lineNum);
					}

					uint16_t *extraData = (uint16_t *)linedata;
					
					for(uint32_t l=0; l < file->numLines; l++)
					{
						//uint16_t unkA = extraData[0];
						//uint16_t unkB = extraData[1];

						extraData += 2;
					}

					RDCASSERT((byte *)extraData == cur);
				}
				RDCASSERT(cur == start + *len);
			}
			else if(*type == 0xF6)
			{
				uint32_t *len = (uint32_t *)(type+2);

				cur = (byte *)(len + 1);

				uint32_t *calls = (uint32_t *)cur;
				uint32_t *callsend = (uint32_t *)(cur + *len);

				// 0 seems to indicate no files, 1 indicates files but we don't need
				// to care as we can just handle this below.
				//RDCDEBUG("start: %x", calls[0]);
				calls++;

				int idx = 0;
				while(calls < callsend)
				{
					// some kind of control bytes? they have n file mappings following but I'm not sure what
					// they mean
					if(calls[0] <= 0xf)
					{
						calls += 1 + calls[0];
					}
					else
					{
						// function call - 3 uint32s: (function idx | 0x1000, FileMapping idx, line # of start of function)
						
						//RDCDEBUG("Call to %s(%x) - file %x, line %d", functions[calls[0]&0xfff].funcName.c_str(), calls[0], calls[1], calls[2]);
						
						funcCalls[idx].fileOffs = calls[1];
						funcCalls[idx].baseLineNum = calls[2];

						idx++;

						calls += 3;
					}
				}

				cur += *len;
			}
			else
			{
				break;
			}
		}
	}

	for(size_t i=0; i < funcCalls.size(); i++)
	{
		RDCASSERT(funcCalls[i].locations.size() > 1);

		if(funcCalls[i].locations.empty() || funcCalls[i].locations.size() == 1)
		{
			RDCWARN("Skipping patching function call with %d locations", funcCalls[i].locations.size());
			continue;
		}

		RDCASSERT(FileMapping.find(funcCalls[i].fileOffs) != FileMapping.end());

		if(FileMapping.find(funcCalls[i].fileOffs) == FileMapping.end())
		{
			RDCWARN("Got function call patch with fileoffs %x - skipping", funcCalls[i].fileOffs);
			continue;
		}

		//RDCDEBUG("Function call %d", i);

		for(size_t j=0; j < funcCalls[i].locations.size()-1; j++)
		{
			auto &loc = funcCalls[i].locations[j];
			auto &locNext = funcCalls[i].locations[j+1];

			// don't apply between function end and next section (if there is one)
			if(loc.funcEnd)
				continue;

			int nPatched = 0;

			for(auto it=m_LineNumbers.begin(); it != m_LineNumbers.end(); ++it)
			{
				if(it->first >= loc.offset && it->first < locNext.offset)
				{
					int32_t fileIdx = FileMapping[funcCalls[i].fileOffs];

					/*
					RDCDEBUG("Patching offset %x between [%x,%x] from (%d,%u) to (%d,%u [%u->%u])",
						it->first, loc.offset, locNext.offset,
						it->second.first, it->second.second,
						fileIdx, loc.line+funcCalls[i].baseLineNum,
						loc.colStart, loc.colEnd);
						*/

					it->second.first = fileIdx;
					it->second.second = loc.line+funcCalls[i].baseLineNum;
					nPatched++;
				}
			}

			/*
			if(nPatched == 0)
				RDCDEBUG("Can't find anything between offsets %x,%x as desired", loc.offset, locNext.offset);*/
		}
	}

	delete[] pages;

	m_HasDebugInfo = true;
}

void SPDBChunk::GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx, int32_t &lineNum) const
{
	for(auto it=m_LineNumbers.begin(); it != m_LineNumbers.end(); ++it)
	{
		if((uintptr_t)it->first <= offset)
		{
			fileIdx = it->second.first;
			lineNum = it->second.second-1; // 0-indexed
		}
		else
		{
			return;
		}
	}
}

}; // namespace DXBC