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

// typed enum so that templates will pick up specialisations
#define GLenum RDCGLenum

#include "gl_enum.h"

#include "official/glcorearb.h"
#include "official/glext.h"

#if defined(WIN32)
#include "official/wglext.h"

struct GLWindowingData
{
	HDC DC;
	HGLRC RC;
	HWND wnd;
};

#elif defined(LINUX)
// cheeky way to prevent GL/gl.h from being included, as we want to use
// glcorearb.h from above
#define __gl_h_
#include <GL/glx.h>

#include "official/glxext.h"

struct GLWindowingData
{
	Display *dpy;
	GLXContext ctx;
	GLXDrawable wnd;
};

#else
#error "Unknown platform"
#endif

#include "replay/renderdoc.h"

#define GLNOTIMP(...) RDCDEBUG("OpenGL not implemented - " __VA_ARGS__)

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) ret func; bool CONCAT(Serialise_, func);

class WrappedOpenGL;

ResourceFormat MakeResourceFormat(WrappedOpenGL &gl, GLenum target, GLenum fmt);
GLenum MakeGLFormat(WrappedOpenGL &gl, GLenum target, ResourceFormat fmt);

#include "serialise/serialiser.h"
#include "core/core.h"

enum GLChunkType
{
	DEVICE_INIT = FIRST_CHUNK_ID,

	GEN_TEXTURE,
	BIND_TEXTURE,
	ACTIVE_TEXTURE,
	TEXSTORAGE2D,
	TEXSUBIMAGE2D,
	PIXELSTORE,
	TEXPARAMETERI,
	GENERATE_MIPMAP,

	CREATE_SHADER,
	CREATE_PROGRAM,
	COMPILESHADER,
	SHADERSOURCE,
	ATTACHSHADER,
	LINKPROGRAM,

	// legacy/immediate mode chunks
	LIGHTFV,
	MATERIALFV,
	GENLISTS,
	NEWLIST,
	ENDLIST,
	CALLLIST,
	SHADEMODEL,
	BEGIN,
	END,
	VERTEX3F,
	NORMAL3F,
	PUSHMATRIX,
	POPMATRIX,
	MATRIXMODE,
	LOADIDENTITY,
	FRUSTUM,
	TRANSLATEF,
	ROTATEF,
	//

	CLEAR_COLOR,
	CLEAR_DEPTH,
	CLEAR,
	CLEARBUFFERF,
	CLEARBUFFERI,
	CLEARBUFFERUI,
	CLEARBUFFERFI,
	CULL_FACE,
	ENABLE,
	DISABLE,
	FRONT_FACE,
	BLEND_FUNC,
	BLEND_COLOR,
	BLEND_FUNC_SEP,
	BLEND_FUNC_SEPI,
	BLEND_EQ_SEP,
	BLEND_EQ_SEPI,
	DEPTH_FUNC,
	VIEWPORT,
	VIEWPORT_ARRAY,
	USEPROGRAM,
	BINDVERTEXARRAY,
	UNIFORM_MATRIX,
	UNIFORM_VECTOR,
	DRAWARRAYS,
	DRAWARRAYS_INSTANCEDBASEDINSTANCE,

	GEN_FRAMEBUFFERS,
	FRAMEBUFFER_TEX,
	BIND_FRAMEBUFFER,
	BLIT_FRAMEBUFFER,

	BIND_SAMPLER,

	GEN_BUFFER,
	BIND_BUFFER,
	BIND_BUFFER_BASE,
	BIND_BUFFER_RANGE,
	BUFFERDATA,
	GEN_VERTEXARRAY,
	BIND_VERTEXARRAY,
	VERTEXATTRIBPOINTER,
	ENABLEVERTEXATTRIBARRAY,
	DELETE_VERTEXARRAY,
	DELETE_BUFFER,
	
	OBJECT_LABEL,
	BEGIN_EVENT,
	SET_MARKER,
	END_EVENT,

	CAPTURE_SCOPE,
	CONTEXT_CAPTURE_HEADER,
	CONTEXT_CAPTURE_FOOTER,

	NUM_OPENGL_CHUNKS,
};
