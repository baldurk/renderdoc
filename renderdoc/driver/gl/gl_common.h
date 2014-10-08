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
	HGLRC ctx;
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

#include "api/replay/renderdoc_replay.h"

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the debugbreak.
#define GLNOTIMP(...) RDCDEBUG("OpenGL not implemented - " __VA_ARGS__)

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) ret func; bool CONCAT(Serialise_, func);

class WrappedOpenGL;

size_t BufferIdx(GLenum buf);
GLenum BufferEnum(size_t idx);

size_t ShaderIdx(GLenum buf);
GLenum ShaderBit(size_t idx);
GLenum ShaderEnum(size_t idx);

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
	TEXSTORAGE1D,
	TEXSTORAGE2D,
	TEXSTORAGE3D,
	TEXSUBIMAGE1D,
	TEXSUBIMAGE2D,
	TEXSUBIMAGE3D,
	TEXSUBIMAGE1D_COMPRESSED,
	TEXSUBIMAGE2D_COMPRESSED,
	TEXSUBIMAGE3D_COMPRESSED,
	TEXBUFFER_RANGE,
	PIXELSTORE,
	TEXPARAMETERF,
	TEXPARAMETERFV,
	TEXPARAMETERI,
	TEXPARAMETERIV,
	GENERATE_MIPMAP,
	COPY_SUBIMAGE,
	TEXTURE_VIEW,

	CREATE_SHADER,
	CREATE_PROGRAM,
	CREATE_SHADERPROGRAM,
	COMPILESHADER,
	SHADERSOURCE,
	ATTACHSHADER,
	DETACHSHADER,
	USEPROGRAM,
	PROGRAMPARAMETER,
	BINDATTRIB_LOCATION,
	UNIFORM_BLOCKBIND,
	PROGRAMUNIFORM_VECTOR,
	LINKPROGRAM,

	GEN_PROGRAMPIPE,
	USE_PROGRAMSTAGES,
	BIND_PROGRAMPIPE,

	FENCE_SYNC,
	CLIENTWAIT_SYNC,
	WAIT_SYNC,

	GEN_QUERIES,
	BEGIN_QUERY,
	END_QUERY,

	CLEAR_COLOR,
	CLEAR_DEPTH,
	CLEAR,
	CLEARBUFFERF,
	CLEARBUFFERI,
	CLEARBUFFERUI,
	CLEARBUFFERFI,
	POLYGON_MODE,
	POLYGON_OFFSET,
	CULL_FACE,
	HINT,
	ENABLE,
	DISABLE,
	ENABLEI,
	DISABLEI,
	FRONT_FACE,
	BLEND_FUNC,
	BLEND_FUNCI,
	BLEND_COLOR,
	BLEND_FUNC_SEP,
	BLEND_FUNC_SEPI,
	BLEND_EQ_SEP,
	BLEND_EQ_SEPI,
	STENCIL_OP,
	STENCIL_OP_SEP,
	STENCIL_FUNC,
	STENCIL_FUNC_SEP,
	STENCIL_MASK,
	STENCIL_MASK_SEP,
	COLOR_MASK,
	COLOR_MASKI,
	SAMPLE_MASK,
	DEPTH_FUNC,
	DEPTH_MASK,
	DEPTH_RANGE,
	DEPTH_RANGEARRAY,
	DEPTH_BOUNDS,
	PATCH_PARAMI,
	PATCH_PARAMFV,
	VIEWPORT,
	VIEWPORT_ARRAY,
	SCISSOR,
	SCISSOR_ARRAY,
	BINDVERTEXARRAY,
	BINDVERTEXBUFFER,
	VERTEXDIVISOR,
	UNIFORM_MATRIX,
	UNIFORM_VECTOR,
	DRAWARRAYS,
	DRAWARRAYS_INSTANCED,
	DRAWARRAYS_INSTANCEDBASEINSTANCE,
	DRAWELEMENTS,
	DRAWRANGEELEMENTS,
	DRAWELEMENTS_INSTANCED,
	DRAWELEMENTS_INSTANCEDBASEINSTANCE,
	DRAWELEMENTS_BASEVERTEX,
	DRAWELEMENTS_INSTANCEDBASEVERTEX,
	DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE,

	GEN_FRAMEBUFFERS,
	FRAMEBUFFER_TEX,
	FRAMEBUFFER_TEX2D,
	FRAMEBUFFER_TEXLAYER,
	READ_BUFFER,
	BIND_FRAMEBUFFER,
	DRAW_BUFFER,
	DRAW_BUFFERS,
	BLIT_FRAMEBUFFER,

	GEN_SAMPLERS,
	SAMPLER_PARAMETERI,
	SAMPLER_PARAMETERF,
	SAMPLER_PARAMETERIV,
	SAMPLER_PARAMETERFV,
	SAMPLER_PARAMETERIIV,
	SAMPLER_PARAMETERIUIV,
	BIND_SAMPLER,

	GEN_BUFFER,
	BIND_BUFFER,
	BIND_BUFFER_BASE,
	BIND_BUFFER_RANGE,
	BUFFERSTORAGE,
	BUFFERDATA,
	BUFFERSUBDATA,
	COPYBUFFERSUBDATA,
	UNMAP,
	GEN_VERTEXARRAY,
	BIND_VERTEXARRAY,
	VERTEXATTRIBPOINTER,
	VERTEXATTRIBIPOINTER,
	ENABLEVERTEXATTRIBARRAY,
	DISABLEVERTEXATTRIBARRAY,
	VERTEXATTRIBFORMAT,
	VERTEXATTRIBIFORMAT,
	VERTEXATTRIBBINDING,
	
	OBJECT_LABEL,
	BEGIN_EVENT,
	SET_MARKER,
	END_EVENT,

	CAPTURE_SCOPE,
	CONTEXT_CAPTURE_HEADER,
	CONTEXT_CAPTURE_FOOTER,

	NUM_OPENGL_CHUNKS,
};
