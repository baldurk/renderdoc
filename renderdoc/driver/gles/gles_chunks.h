/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

/* Usage:
 *  define a macro with two arguments:
 *   #define CHUNK_FUNC(ID, TEXT) ...
 *  Then call the CHUNKS(CHUNK_FUNC) where you want to porcess it
 */
#define CHUNKS(F) \
  F(DEVICE_INIT = FIRST_CHUNK_ID, "WrappedGLES::Initialisation") \
  \
  F(GEN_TEXTURE, "glGenTextures") \
  F(CREATE_TEXTURE, "glCreateTextures") /*unused*/ \
  F(BIND_TEXTURE, "glBindTexture") \
  F(BIND_TEXTURES, "glBindTextures") /*unused*/ \
  F(BIND_MULTI_TEX, "glBindMultiTexture") /*unused*/ \
  F(BIND_TEXTURE_UNIT, "glBindTextureUnit") /*unused*/ \
  F(BIND_IMAGE_TEXTURE, "glBindImageTexture") \
  F(BIND_IMAGE_TEXTURES, "glBindImageTextures") /*unused*/ \
  F(ACTIVE_TEXTURE, "glActiveTexture") \
  F(TEXSTORAGE1D, "glTexStorage1D") \
  F(TEXSTORAGE2D, "glTexStorage2D") \
  F(TEXSTORAGE3D, "glTexStorage3D") \
  F(TEXSTORAGE2DMS, "glTexStorage2DMultisample") \
  F(TEXSTORAGE3DMS, "glTexStorage3DMultisample") \
  F(TEXIMAGE1D, "glTexImage1D") /*unused*/ \
  F(TEXIMAGE2D, "glTexImage2D") \
  F(TEXIMAGE3D, "glTexImage3D") \
  F(TEXSUBIMAGE1D, "glTexSubImage1D") /*unused*/ \
  F(TEXSUBIMAGE2D, "glTexSubImage2D") \
  F(TEXSUBIMAGE3D, "glTexSubImage3D") \
  F(TEXIMAGE1D_COMPRESSED, "glCompressedTexImage1D") /*unused*/ \
  F(TEXIMAGE2D_COMPRESSED, "glCompressedTexImage2D") \
  F(TEXIMAGE3D_COMPRESSED, "glCompressedTexImage3D") \
  F(TEXSUBIMAGE1D_COMPRESSED, "glCompressedTexSubImage1D") /*unused*/ \
  F(TEXSUBIMAGE2D_COMPRESSED, "glCompressedTexSubImage2D") \
  F(TEXSUBIMAGE3D_COMPRESSED, "glCompressedTexSubImage3D") \
  F(TEXBUFFER, "glTexBuffer") \
  F(TEXBUFFER_RANGE, "glTexBufferRange") \
  F(PIXELSTORE, "glPixelStore") \
  F(TEXPARAMETERF, "glTexParameterf") \
  F(TEXPARAMETERFV, "glTexParameterfv") \
  F(TEXPARAMETERI, "glTexParameteri") \
  F(TEXPARAMETERIV, "glTexParameteriv") \
  F(TEXPARAMETERIIV, "glTexParameterIiv") \
  F(TEXPARAMETERIUIV, "glTexParameterIuiv") \
  F(GENERATE_MIPMAP, "glGenerateMipmap") \
  F(COPY_SUBIMAGE, "glCopyImageSubData") \
  F(COPY_IMAGE1D, "glCopyTexImage1D") /*unused*/ \
  F(COPY_IMAGE2D, "glCopyTexImage2D") \
  F(COPY_SUBIMAGE1D, "glCopyTexSubImage1D") /*unused*/ \
  F(COPY_SUBIMAGE2D, "glCopyTexSubImage2D") \
  F(COPY_SUBIMAGE3D, "glCopyTexSubImage3D") \
  F(TEXTURE_VIEW, "glTextureView") \
  \
  F(CREATE_SHADER, "glCreateShader") \
  F(CREATE_PROGRAM, "glCreateProgram") \
  F(CREATE_SHADERPROGRAM, "glCreateShaderProgramv") \
  F(COMPILESHADER, "glCompileShader") \
  F(SHADERSOURCE, "glShaderSource") \
  F(ATTACHSHADER, "glAttachShader") \
  F(DETACHSHADER, "glDetachShader") \
  F(USEPROGRAM, "glUseProgram") \
  F(PROGRAMPARAMETER, "glProgramParameter") \
  F(FEEDBACK_VARYINGS, "glTransformFeedbackVaryings") \
  F(BINDATTRIB_LOCATION, "glBindAttribLocation") \
  F(BINDFRAGDATA_LOCATION, "glBindFragDataLocation") \
  F(BINDFRAGDATA_LOCATION_INDEXED, "glBindFragDataLocationIndexed") \
  F(UNIFORM_BLOCKBIND, "glUniformBlockBinding") \
  F(STORAGE_BLOCKBIND, "glShaderStorageBlockBinding") /*unused*/ \
  F(UNIFORM_SUBROUTINE, "glUniformSubroutinesuiv") /*unused*/ \
  F(PROGRAMUNIFORM_VECTOR, "glProgramUniformVector*") \
  F(PROGRAMUNIFORM_MATRIX, "glProgramUniformMatrix*") \
  F(LINKPROGRAM, "glLinkProgram") \
  \
  F(NAMEDSTRING, "glNamedStringARB") /*unused*/ \
  F(DELETENAMEDSTRING, "glDeleteNamedStringARB") /*unused*/ \
  F(COMPILESHADERINCLUDE, "glCompileShaderIncludeARB") /*unused*/ \
  \
  F(GEN_FEEDBACK, "glGenTransformFeedbacks") \
  F(CREATE_FEEDBACK, "glCreateTransformFeedbacks") /*unused*/ \
  F(BIND_FEEDBACK, "glBindTransformFeedback") \
  F(BEGIN_FEEDBACK, "glBeginTransformFeedback") \
  F(END_FEEDBACK, "glEndTransformFeedback") \
  F(PAUSE_FEEDBACK, "glPauseTransformFeedback") \
  F(RESUME_FEEDBACK, "glResumeTransformFeedback") \
  \
  F(GEN_PROGRAMPIPE, "glGenProgramPipelines") \
  F(CREATE_PROGRAMPIPE, "glCreateProgramPipelines") /*unused*/ \
  F(USE_PROGRAMSTAGES, "glUseProgramStages") \
  F(BIND_PROGRAMPIPE, "glBindProgramPipeline") \
  \
  F(FENCE_SYNC, "glFenceSync") \
  F(CLIENTWAIT_SYNC, "glClientWaitSync") \
  F(WAIT_SYNC, "glWaitSync") \
  \
  F(GEN_QUERIES, "glGenQueries") \
  F(CREATE_QUERIES, "glCreateQueries") /*unused*/ \
  F(BEGIN_QUERY, "glBeginQuery") \
  F(BEGIN_QUERY_INDEXED, "glBeginQueryIndexed") /*unused*/ \
  F(END_QUERY, "glEndQuery") \
  F(END_QUERY_INDEXED, "glEndQueryIndexed") /*unused*/ \
  F(BEGIN_CONDITIONAL, "glBeginConditional") \
  F(END_CONDITIONAL, "glEndConditional") \
  F(QUERY_COUNTER, "glQueryCounter") \
  \
  F(CLEAR_COLOR, "glClearColor") \
  F(CLEAR_DEPTH, "glClearDepth") \
  F(CLEAR_STENCIL, "glClearStencil") \
  F(CLEAR, "glClear") \
  F(CLEARBUFFERF, "glClearBufferfv") \
  F(CLEARBUFFERI, "glClearBufferiv") \
  F(CLEARBUFFERUI, "glClearBufferuiv") \
  F(CLEARBUFFERFI, "glClearBufferfi") \
  F(CLEARBUFFERDATA, "glClearBufferData") /*unused*/ \
  F(CLEARBUFFERSUBDATA, "glClearBufferSubData") /*unused*/ \
  F(CLEARTEXIMAGE, "glClearTexImage") /*unused*/ \
  F(CLEARTEXSUBIMAGE, "glClearTexSubImage") /*unused*/ \
  F(POLYGON_MODE, "glPolygonMode") \
  F(POLYGON_OFFSET, "glPolygonOffset") \
  F(POLYGON_OFFSET_CLAMP, "glPolygonOffsetClampEXT") \
  F(CULL_FACE, "glCullFace") \
  F(HINT, "glHint") \
  F(ENABLE, "glEnable") \
  F(DISABLE, "glDisable") \
  F(ENABLEI, "glEnablei") \
  F(DISABLEI, "glDisablei") \
  F(FRONT_FACE, "glFrontFace") \
  F(BLEND_FUNC, "glBlendFunc") \
  F(BLEND_FUNCI, "glBlendFunci") \
  F(BLEND_COLOR, "glBlendColor") \
  F(BLEND_FUNC_SEP, "glBlendFuncSeparate") \
  F(BLEND_FUNC_SEPI, "glBlendFuncSeparatei") \
  F(BLEND_EQ, "glBlendEquation") \
  F(BLEND_EQI, "glBlendEquationi") \
  F(BLEND_EQ_SEP, "glBlendEquationSeparate") \
  F(BLEND_EQ_SEPI, "glBlendEquationSeparatei") \
  F(BLEND_BARRIER, "glBlendBarrier") \
  F(LOGIC_OP, "glLogicOp") /*unused*/ \
  F(STENCIL_OP, "glStencilOp") \
  F(STENCIL_OP_SEP, "glStencilOpSeparate") \
  F(STENCIL_FUNC, "glStencilFunc") \
  F(STENCIL_FUNC_SEP, "glStencilFuncSeparate") \
  F(STENCIL_MASK, "glStencilMask") \
  F(STENCIL_MASK_SEP, "glStencilMaskSeparate") \
  F(COLOR_MASK, "glColorMask") \
  F(COLOR_MASKI, "glColorMaski") \
  F(SAMPLE_MASK, "glSampleMaski") \
  F(SAMPLE_COVERAGE, "glSampleCoverage") \
  F(MIN_SAMPLE_SHADING, "glMinSampleShading") \
  F(RASTER_SAMPLES, "glRasterSamplesEXT") \
  F(DEPTH_FUNC, "glDepthFunc") \
  F(DEPTH_MASK, "glDepthMask") \
  F(DEPTH_RANGE, "glDepthRange") /*unused*/ \
  F(DEPTH_RANGEF, "glDepthRangef") \
  F(DEPTH_RANGE_IDX, "glDepthRangeIndexed") \
  F(DEPTH_RANGEARRAY, "glDepthRangeArrayv") \
  F(DEPTH_BOUNDS, "glDepthBounds") /*unused*/ \
  F(CLIP_CONTROL, "glClipControl") /*unused*/ \
  F(PROVOKING_VERTEX, "glProvokingVertex") /*unused*/ \
  F(PRIMITIVE_RESTART, "glPrimitiveRestartIndex") /*unused*/ \
  F(PRIMITIVE_BOUNDING_BOX, "glPrimitiveBoundingBox") \
  F(PATCH_PARAMI, "glPatchParameteri") \
  F(PATCH_PARAMFV, "glPatchParameterfv") /*unused*/ \
  F(LINE_WIDTH, "glLineWidth") \
  F(POINT_SIZE, "glPointSize") /*unused*/ \
  F(POINT_PARAMF, "glPointParameterf") /*unused*/ \
  F(POINT_PARAMFV, "glPointParameterfv") /*unused*/ \
  F(POINT_PARAMI, "glPointParameteri") /*unused*/ \
  F(POINT_PARAMIV, "glPointParameteriv") /*unused*/ \
  F(VIEWPORT, "glViewport") \
  F(VIEWPORT_ARRAY, "glViewportArrayv") \
  F(SCISSOR, "glScissor") \
  F(SCISSOR_ARRAY, "glScissorArrayv") \
  F(BIND_VERTEXBUFFER, "glBindVertexBuffer") \
  F(BIND_VERTEXBUFFERS, "glBindVertexBuffers") /*unused*/ \
  F(VERTEXBINDINGDIVISOR, "glVertexBindingDivisor") \
  F(DISPATCH_COMPUTE, "glDispatchCompute") \
  F(DISPATCH_COMPUTE_GROUP_SIZE, "glDispatchComputeGroupSizeARB") /*unused*/ \
  F(DISPATCH_COMPUTE_INDIRECT, "glDispatchComputeIndirect") \
  F(MEMORY_BARRIER, "glMemoryBarrier") \
  F(MEMORY_BARRIER_BY_REGION, "glMemoryBarrierByRegion") \
  F(TEXTURE_BARRIER, "glTextureBarrier") /*unused*/ \
  F(DRAWARRAYS, "glDrawArrays") \
  F(DRAWARRAYS_INDIRECT, "glDrawArraysIndirect") \
  F(DRAWARRAYS_INSTANCED, "glDrawArraysInstanced") \
  F(DRAWARRAYS_INSTANCEDBASEINSTANCE, "glDrawArraysInstancedBaseInstance") \
  F(DRAWELEMENTS, "glDrawElements") \
  F(DRAWELEMENTS_INDIRECT, "glDrawElementsIndirect") \
  F(DRAWRANGEELEMENTS, "glDrawRangeElements") \
  F(DRAWRANGEELEMENTSBASEVERTEX, "glDrawRangeElementsBaseVertex") \
  F(DRAWELEMENTS_INSTANCED, "glDrawElementsInstanced") \
  F(DRAWELEMENTS_INSTANCEDBASEINSTANCE, "glDrawElementsInstancedBaseInstance") \
  F(DRAWELEMENTS_BASEVERTEX, "glDrawElementsBaseVertex") \
  F(DRAWELEMENTS_INSTANCEDBASEVERTEX, "glDrawElementsInstancedBaseVertex") \
  F(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE, "glDrawElementsInstancedBaseVertexBaseInstance") \
  F(DRAW_FEEDBACK, "glDrawTransformFeedback") /*unused*/ \
  F(DRAW_FEEDBACK_INSTANCED, "glDrawTransformFeedbackInstanced") /*unused*/ \
  F(DRAW_FEEDBACK_STREAM, "glDrawTransformFeedbackStream") /*unused*/ \
  F(DRAW_FEEDBACK_STREAM_INSTANCED, "glDrawTransformFeedbackStreamInstanced") /*unused*/ \
  F(MULTI_DRAWARRAYS, "glMultiDrawArrays") \
  F(MULTI_DRAWELEMENTS, "glMultiDrawElements") \
  F(MULTI_DRAWELEMENTSBASEVERTEX, "glMultiDrawElementsBaseVertex") \
  F(MULTI_DRAWARRAYS_INDIRECT, "glMultiDrawArraysIndirect") \
  F(MULTI_DRAWELEMENTS_INDIRECT, "glMultiDrawElementsIndirect") \
  F(MULTI_DRAWARRAYS_INDIRECT_COUNT, "glMultiDrawArraysIndirectCountARB") /*unused*/ \
  F(MULTI_DRAWELEMENTS_INDIRECT_COUNT, "glMultiDrawElementsIndirectCountARB") /*unused*/ \
  \
  F(GEN_FRAMEBUFFERS, "glGenFramebuffers") \
  F(CREATE_FRAMEBUFFERS, "glCreateFramebuffers") /*unused*/ \
  F(FRAMEBUFFER_TEX, "glFramebufferTexture") \
  F(FRAMEBUFFER_TEX1D, "glFramebufferTexture1D") /*unused*/ \
  F(FRAMEBUFFER_TEX2D, "glFramebufferTexture2D") \
  F(FRAMEBUFFER_TEX3D, "glFramebufferTexture3D") \
  F(FRAMEBUFFER_RENDBUF, "glFramebufferRenderbuffer") \
  F(FRAMEBUFFER_TEXLAYER, "glFramebufferTextureLayer") \
  F(FRAMEBUFFER_PARAM, "glFramebufferParameteri") \
  F(READ_BUFFER, "glReadBuffer") \
  F(BIND_FRAMEBUFFER, "glBindFramebuffer") \
  F(DRAW_BUFFER, "glDrawBuffer") /*unused*/ \
  F(DRAW_BUFFERS, "glDrawBuffers") \
  F(BLIT_FRAMEBUFFER, "glBlitFramebuffer") \
  \
  F(GEN_RENDERBUFFERS, "glGenRenderbuffers") \
  F(CREATE_RENDERBUFFERS, "glCreateRenderbuffers") /*unused*/ \
  F(RENDERBUFFER_STORAGE, "glRenderbufferStorage") \
  F(RENDERBUFFER_STORAGEMS, "glRenderbufferStorageMultisample") \
  \
  F(GEN_SAMPLERS, "glGenSamplers") \
  F(CREATE_SAMPLERS, "glCreateSamplers") /*unused*/ \
  F(SAMPLER_PARAMETERI, "glSamplerParameteri") \
  F(SAMPLER_PARAMETERF, "glSamplerParameterf") \
  F(SAMPLER_PARAMETERIV, "glSamplerParameteriv") \
  F(SAMPLER_PARAMETERFV, "glSamplerParameterfv") \
  F(SAMPLER_PARAMETERIIV, "glSamplerParameterIiv") \
  F(SAMPLER_PARAMETERIUIV, "glSamplerParameterIuiv") \
  F(BIND_SAMPLER, "glBindSampler") \
  F(BIND_SAMPLERS, "glBindSamplers") /*unused*/ \
  \
  F(GEN_BUFFER, "glGenBuffers") \
  F(CREATE_BUFFER, "glCreateBuffers") /*unused*/ \
  F(BIND_BUFFER, "glBindBuffer") \
  F(BIND_BUFFER_BASE, "glBindBufferBase") \
  F(BIND_BUFFER_RANGE, "glBindBufferRange") \
  F(BIND_BUFFERS_BASE, "glBindBuffersBase") /*unused*/ \
  F(BIND_BUFFERS_RANGE, "glBindBuffersRange") /*unused*/ \
  F(BUFFERSTORAGE, "glBufferStorage") \
  F(BUFFERDATA, "glBufferData") \
  F(BUFFERSUBDATA, "glBufferSubData") \
  F(COPYBUFFERSUBDATA, "glCopyBufferSubData") \
  F(UNMAP, "glUnmapBuffer") \
  F(FLUSHMAP, "glFlushMappedBufferRange") \
  F(GEN_VERTEXARRAY, "glGenVertexArrays") \
  F(CREATE_VERTEXARRAY, "glCreateVertexArrays") /*unused*/ \
  F(BIND_VERTEXARRAY, "glBindVertexArray") \
  F(VERTEXATTRIB_GENERIC, "glVertexAttrib*") \
  F(VERTEXATTRIBPOINTER, "glVertexAttribPointer") \
  F(VERTEXATTRIBIPOINTER, "glVertexAttribIPointer") /*unused*/ \
  F(VERTEXATTRIBLPOINTER, "glVertexAttribLPointer") /*unused*/ \
  F(ENABLEVERTEXATTRIBARRAY, "glEnableVertexAttribArray") \
  F(DISABLEVERTEXATTRIBARRAY, "glDisableVertexAttribArray") \
  F(VERTEXATTRIBFORMAT, "glVertexAttribFormat") \
  F(VERTEXATTRIBIFORMAT, "glVertexAttribIFormat") \
  F(VERTEXATTRIBLFORMAT, "glVertexAttribLFormat") /*unused*/ \
  F(VERTEXATTRIBDIVISOR, "glVertexAttribDivisor") \
  F(VERTEXATTRIBBINDING, "glVertexAttribBinding") \
  \
  F(VAO_ELEMENT_BUFFER, "glVertexArrayElementBuffer") /*unused*/ \
  F(FEEDBACK_BUFFER_BASE, "glTransformFeedbackBufferBase") /*unused*/ \
  F(FEEDBACK_BUFFER_RANGE, "glTransformFeedbackBufferRange") /*unused*/ \
  \
  F(OBJECT_LABEL, "glObjectLabel") \
  F(BEGIN_EVENT, "glPushDebugGroup") \
  F(SET_MARKER, "glDebugMessageInsert") \
  F(END_EVENT, "glPopDebugGroup") \
  \
  F(DEBUG_MESSAGES, "DebugMessageList") \
  \
  F(CAPTURE_SCOPE, "Capture") \
  F(CONTEXT_CAPTURE_HEADER, "BeginCapture") \
  F(CONTEXT_CAPTURE_FOOTER, "EndCapture") \

