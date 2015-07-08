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

#include "common/common.h"

#include "spirv_common.h"

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/GlslangToSpv.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

void DisassembleSPIRV(SPIRVShaderStage shadType, const vector<uint32_t> &spirv, string &disasm)
{
	if(shadType >= eSPIRVInvalid)
		return;

	// temporary function until we build our own structure from the SPIR-V
	const char *header[] = {
		"Vertex Shader",
		"Tessellation Control Shader",
		"Tessellation Evaluation Shader",
		"Geometry Shader",
		"Fragment Shader",
		"Compute Shader",
	};

	disasm = header[(int)shadType];
	disasm += " SPIR-V raw stream:\n\n";

	for(size_t i=0; i < spirv.size(); i++)
		disasm += StringFormat::Fmt("    %08x\n", spirv[i]);
}
