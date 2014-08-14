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

#include "basic_types.h"

#include "replay_enums.h"

struct ShaderVariable
{
	ShaderVariable()
	{
		name = ""; rows = columns = 0;
		type = eVar_Float;
		for(int i=0; i < 16; i++) value.uv[i] = 0;
	}
	ShaderVariable(const char *n, float x, float y, float z, float w)
	{
		name = n; rows = 1; columns = 4;
		for(int i=0; i < 16; i++) value.uv[i] = 0;
		type = eVar_Float;
		value.f.x = x; value.f.y = y; value.f.z = z; value.f.w = w;
	}
	ShaderVariable(const char *n, int x, int y, int z, int w)
	{
		name = n; rows = 1; columns = 4;
		for(int i=0; i < 16; i++) value.uv[i] = 0;
		type = eVar_Int;
		value.i.x = x; value.i.y = y; value.i.z = z; value.i.w = w;
	}
	ShaderVariable(const char *n, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
	{
		name = n; rows = 1; columns = 4;
		for(int i=0; i < 16; i++) value.uv[i] = 0;
		type = eVar_UInt;
		value.u.x = x; value.u.y = y; value.u.z = z; value.u.w = w;
	}

	uint32_t rows, columns;
	rdctype::str name;

	VarType type;

	union
	{
		struct
		{
			float x, y, z, w;
		} f;
		float fv[16];

		struct 
		{
			int32_t x, y, z, w;
		} i;
		int32_t iv[16];

		struct 
		{
			uint32_t x, y, z, w;
		} u;
		uint32_t uv[16];
	} value;

	rdctype::array<ShaderVariable> members;
};

struct ShaderDebugState
{
	rdctype::array<ShaderVariable> registers;
	rdctype::array<ShaderVariable> outputs;
	
	rdctype::array< rdctype::array<ShaderVariable> > indexableTemps;

	uint32_t nextInstruction;
};

struct ShaderDebugTrace
{
	rdctype::array<ShaderVariable> inputs;
	rdctype::array< rdctype::array<ShaderVariable> > cbuffers;

	rdctype::array<ShaderDebugState> states;
};

struct SigParameter
{
	SigParameter()
		: semanticIndex(0), needSemanticIndex(false), regIndex(0),
		systemValue(eAttr_None), compType(eCompType_Float),
		regChannelMask(0), channelUsedMask(0), compCount(0), stream(0)
	{ }
	
	rdctype::str	varName;
	rdctype::str	semanticName;
	uint32_t			semanticIndex;
	rdctype::str	semanticIdxName;

	bool32					needSemanticIndex;

	uint32_t			regIndex;
	SystemAttribute	systemValue;

	FormatComponentType compType;

	uint8_t				regChannelMask;
	uint8_t				channelUsedMask;
	uint32_t			compCount;
	uint32_t			stream;
};

struct ShaderConstant;

struct ShaderVariableType
{
	struct
	{
		VarType       type;
		uint32_t      rows;
		uint32_t      cols;
		uint32_t      elements;
		bool32        rowMajorStorage;
		rdctype::str name;
	} descriptor;

	rdctype::array<ShaderConstant> members;
};

struct ShaderConstant
{
	rdctype::str name;
	struct
	{
		uint32_t vec;
		uint32_t comp;
	} reg;
	ShaderVariableType type;
};

struct ConstantBlock
{
	rdctype::str name;
	rdctype::array<ShaderConstant> variables;
	bool32 bufferBacked;
	int32_t bindPoint;
};

struct ShaderResource
{
	bool32 IsSampler;
	bool32 IsTexture;
	bool32 IsSRV;
	bool32 IsUAV;
	
	ShaderResourceType resType;

	rdctype::str    name;
	ShaderVariableType variableType;
	int32_t bindPoint;
};

struct ShaderDebugChunk
{
	ShaderDebugChunk() : compileFlags(0) {}

	rdctype::str entryFunc;

	uint32_t compileFlags;

	rdctype::array< rdctype::pair<rdctype::str, rdctype::str> > files; // <filename, source>
};

struct ShaderReflection
{
	ShaderDebugChunk DebugInfo;
	rdctype::str Disassembly;

	rdctype::array<SigParameter> InputSig;
	rdctype::array<SigParameter> OutputSig;
	
	rdctype::array<ConstantBlock> ConstantBlocks; // sparse - index indicates bind point

	rdctype::array<ShaderResource> Resources; // non-sparse, since bind points can overlap.
	
	// TODO expand this to encompass shader subroutines.
	rdctype::array<rdctype::str> Interfaces;
};

struct BindpointMap
{
	int32_t bind;
	bool32 used;
};

struct ShaderBindpointMapping
{
	rdctype::array<BindpointMap> ConstantBlocks;
	rdctype::array<BindpointMap> Resources;
};
