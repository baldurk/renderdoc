/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include <vector>
#include <string>
using std::string;
using std::vector;

#include <stdint.h>

enum SPIRVShaderStage
{
	eSPIRVVertex,
	eSPIRVTessControl,
	eSPIRVTessEvaluation,
	eSPIRVGeometry,
	eSPIRVFragment,
	eSPIRVCompute,
	eSPIRVGeneric,
	eSPIRVInvalid,
};

void InitSPIRVCompiler();
void ShutdownSPIRVCompiler();

struct SPVInstruction;

struct ShaderReflection;
struct ShaderBindpointMapping;

struct SPVModule
{
	SPVModule();
	~SPVModule();

	uint32_t moduleVersion;
	uint32_t generator;

	string sourceLang; uint32_t sourceVer;

	vector<SPVInstruction*> operations; // all operations (including those that don't generate an ID)

	vector<SPVInstruction*> ids; // pointers indexed by ID

	vector<SPVInstruction*> sourceexts; // source extensions
	vector<SPVInstruction*> entries; // entry points
	vector<SPVInstruction*> globals; // global variables
	vector<SPVInstruction*> funcs; // functions
	vector<SPVInstruction*> structs; // struct types

	string m_Disassembly;
	
	SPVInstruction *GetByID(uint32_t id);
	void Disassemble();

	void MakeReflection(ShaderReflection *reflection, ShaderBindpointMapping *mapping);
};

string CompileSPIRV(SPIRVShaderStage shadType, const vector<string> &sources, vector<uint32_t> &spirv);
void ParseSPIRV(uint32_t *spirv, size_t spirvLength, SPVModule &module);
