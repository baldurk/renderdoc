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

#include "gl_common.h"
#include "gl_hookset.h"
#include "gl_manager.h"

struct GLRenderState
{
	GLRenderState(const GLHookSet *funcs, Serialiser *ser);
	~GLRenderState();

	void FetchState();
	void ApplyState();
	void Clear();

	//
	uint32_t Tex2D[128];
	GLenum ActiveTexture;
	uint32_t BufferBindings[10];
	struct IdxRangeBuffer
	{
		uint32_t name;
		uint64_t start;
		uint64_t size;
	} AtomicCounter[8], ShaderStorage[8], TransformFeedback[8], UniformBinding[128];
	//

	void Serialise(LogState state, GLResourceManager *rm);
private:
	Serialiser *m_pSerialiser;
	const GLHookSet *m_Real;
};