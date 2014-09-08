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

// it's recommended that you auto-generate this file with hookset.pl, but you can manually edit it
// if you prefer.
////////////////////////////////////////////////////

// dllexport functions
#define DLLExportHooks() \
    HookInit(glBindTexture); \
    HookInit(glBlendFunc); \
    HookInit(glClear); \
    HookInit(glClearColor); \
    HookInit(glClearDepth); \
    HookInit(glColorMask); \
    HookInit(glCullFace); \
    HookInit(glDepthFunc); \
    HookInit(glDepthMask); \
    HookInit(glStencilFunc); \
    HookInit(glStencilMask); \
    HookInit(glStencilOp); \
    HookInit(glDisable); \
    HookInit(glDrawBuffer); \
    HookInit(glDrawElements); \
    HookInit(glDrawArrays); \
    HookInit(glEnable); \
    HookInit(glFlush); \
    HookInit(glFinish); \
    HookInit(glFrontFace); \
    HookInit(glGenTextures); \
    HookInit(glDeleteTextures); \
    HookInit(glGetError); \
    HookInit(glIsEnabled); \
    HookInit(glGetTexLevelParameteriv); \
    HookInit(glGetTexLevelParameterfv); \
    HookInit(glGetTexParameterfv); \
    HookInit(glGetTexParameteriv); \
    HookInit(glGetTexImage); \
    HookInit(glGetBooleanv); \
    HookInit(glGetFloatv); \
    HookInit(glGetDoublev); \
    HookInit(glGetIntegerv); \
    HookInit(glGetString); \
    HookInit(glHint); \
    HookInit(glPixelStorei); \
    HookInit(glPixelStoref); \
    HookInit(glPolygonMode); \
    HookInit(glPolygonOffset); \
    HookInit(glReadPixels); \
    HookInit(glReadBuffer); \
    HookInit(glScissor); \
    HookInit(glTexImage1D); \
    HookInit(glTexImage2D); \
    HookInit(glTexImage3D); \
    HookInit(glTexSubImage1D); \
    HookInit(glTexSubImage2D); \
    HookInit(glTexParameterf); \
    HookInit(glTexParameterfv); \
    HookInit(glTexParameteri); \
    HookInit(glTexParameteriv); \
    HookInit(glViewport); \



// wgl extensions
#define HookCheckWGLExtensions() \



// glx extensions
#define HookCheckGLXExtensions() \



// gl extensions
#define HookCheckGLExtensions() \
    HookExtension(PFNGLACTIVETEXTUREPROC, glActiveTexture); \
    HookExtension(PFNGLTEXSTORAGE1DPROC, glTexStorage1D); \
    HookExtension(PFNGLTEXSTORAGE2DPROC, glTexStorage2D); \
    HookExtension(PFNGLTEXSTORAGE3DPROC, glTexStorage3D); \
    HookExtension(PFNGLTEXSUBIMAGE3DPROC, glTexSubImage3D); \
    HookExtension(PFNGLCOMPRESSEDTEXIMAGE1DPROC, glCompressedTexImage1D); \
    HookExtension(PFNGLCOMPRESSEDTEXIMAGE2DPROC, glCompressedTexImage2D); \
    HookExtension(PFNGLCOMPRESSEDTEXIMAGE3DPROC, glCompressedTexImage3D); \
    HookExtension(PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC, glCompressedTexSubImage1D); \
    HookExtension(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC, glCompressedTexSubImage2D); \
    HookExtension(PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC, glCompressedTexSubImage3D); \
    HookExtension(PFNGLTEXBUFFERRANGEPROC, glTexBufferRange); \
    HookExtension(PFNGLTEXTUREVIEWPROC, glTextureView); \
    HookExtension(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap); \
    HookExtension(PFNGLCOPYIMAGESUBDATAPROC, glCopyImageSubData); \
    HookExtension(PFNGLGETINTERNALFORMATIVPROC, glGetInternalformativ); \
    HookExtension(PFNGLGETINTERNALFORMATI64VPROC, glGetInternalformati64v); \
    HookExtension(PFNGLGETBUFFERPARAMETERIVPROC, glGetBufferParameteriv); \
    HookExtension(PFNGLGETSTRINGIPROC, glGetStringi); \
    HookExtension(PFNGLGETBOOLEANI_VPROC, glGetBooleani_v); \
    HookExtension(PFNGLGETINTEGERI_VPROC, glGetIntegeri_v); \
    HookExtension(PFNGLGETFLOATI_VPROC, glGetFloati_v); \
    HookExtension(PFNGLGETDOUBLEI_VPROC, glGetDoublei_v); \
    HookExtension(PFNGLGETINTEGER64I_VPROC, glGetInteger64i_v); \
    HookExtension(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus); \
    HookExtension(PFNGLBLENDCOLORPROC, glBlendColor); \
    HookExtension(PFNGLBLENDFUNCIPROC, glBlendFunci); \
    HookExtension(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate); \
    HookExtension(PFNGLBLENDFUNCSEPARATEIPROC, glBlendFuncSeparatei); \
    HookExtension(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate); \
    HookExtension(PFNGLBLENDEQUATIONSEPARATEIPROC, glBlendEquationSeparatei); \
    HookExtension(PFNGLSTENCILFUNCSEPARATEPROC, glStencilFuncSeparate); \
    HookExtension(PFNGLSTENCILMASKSEPARATEPROC, glStencilMaskSeparate); \
    HookExtension(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate); \
    HookExtension(PFNGLCOLORMASKIPROC, glColorMaski); \
    HookExtension(PFNGLSAMPLEMASKIPROC, glSampleMaski); \
    HookExtension(PFNGLDEPTHRANGEPROC, glDepthRange); \
    HookExtension(PFNGLDEPTHRANGEFPROC, glDepthRangef); \
    HookExtension(PFNGLDEPTHRANGEARRAYVPROC, glDepthRangeArrayv); \
    HookExtension(PFNGLDEPTHBOUNDSEXTPROC, glDepthBoundsEXT); \
    HookExtension(PFNGLCREATESHADERPROC, glCreateShader); \
    HookExtension(PFNGLDELETESHADERPROC, glDeleteShader); \
    HookExtension(PFNGLSHADERSOURCEPROC, glShaderSource); \
    HookExtension(PFNGLCOMPILESHADERPROC, glCompileShader); \
    HookExtension(PFNGLCREATESHADERPROGRAMVPROC, glCreateShaderProgramv); \
    HookExtension(PFNGLGETSHADERIVPROC, glGetShaderiv); \
    HookExtension(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog); \
    HookExtension(PFNGLCREATEPROGRAMPROC, glCreateProgram); \
    HookExtension(PFNGLDELETEPROGRAMPROC, glDeleteProgram); \
    HookExtension(PFNGLATTACHSHADERPROC, glAttachShader); \
    HookExtension(PFNGLDETACHSHADERPROC, glDetachShader); \
    HookExtension(PFNGLRELEASESHADERCOMPILERPROC, glReleaseShaderCompiler); \
    HookExtension(PFNGLLINKPROGRAMPROC, glLinkProgram); \
    HookExtension(PFNGLPROGRAMPARAMETERIPROC, glProgramParameteri); \
    HookExtension(PFNGLPROGRAMUNIFORM1IPROC, glProgramUniform1i); \
    HookExtension(PFNGLPROGRAMUNIFORM1FVPROC, glProgramUniform1fv); \
    HookExtension(PFNGLPROGRAMUNIFORM1IVPROC, glProgramUniform1iv); \
    HookExtension(PFNGLPROGRAMUNIFORM1UIVPROC, glProgramUniform1uiv); \
    HookExtension(PFNGLPROGRAMUNIFORM2FVPROC, glProgramUniform2fv); \
    HookExtension(PFNGLPROGRAMUNIFORM3FVPROC, glProgramUniform3fv); \
    HookExtension(PFNGLPROGRAMUNIFORM4FVPROC, glProgramUniform4fv); \
    HookExtension(PFNGLUSEPROGRAMPROC, glUseProgram); \
    HookExtension(PFNGLUSEPROGRAMSTAGESPROC, glUseProgramStages); \
    HookExtension(PFNGLVALIDATEPROGRAMPROC, glValidateProgram); \
    HookExtension(PFNGLGETPROGRAMIVPROC, glGetProgramiv); \
    HookExtension(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog); \
    HookExtension(PFNGLGETPROGRAMINTERFACEIVPROC, glGetProgramInterfaceiv); \
    HookExtension(PFNGLGETPROGRAMRESOURCEINDEXPROC, glGetProgramResourceIndex); \
    HookExtension(PFNGLGETPROGRAMRESOURCEIVPROC, glGetProgramResourceiv); \
    HookExtension(PFNGLGETPROGRAMRESOURCENAMEPROC, glGetProgramResourceName); \
    HookExtension(PFNGLGENPROGRAMPIPELINESPROC, glGenProgramPipelines); \
    HookExtension(PFNGLBINDPROGRAMPIPELINEPROC, glBindProgramPipeline); \
    HookExtension(PFNGLDELETEPROGRAMPIPELINESPROC, glDeleteProgramPipelines); \
    HookExtension(PFNGLGETPROGRAMPIPELINEIVPROC, glGetProgramPipelineiv); \
    HookExtension(PFNGLGETPROGRAMPIPELINEINFOLOGPROC, glGetProgramPipelineInfoLog); \
    HookExtension(PFNGLVALIDATEPROGRAMPIPELINEPROC, glValidateProgramPipeline); \
    HookExtension(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback); \
    HookExtensionAlias(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback, glDebugMessageCallbackARB); \
    HookExtension(PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl); \
    HookExtension(PFNGLDEBUGMESSAGEINSERTPROC, glDebugMessageInsert); \
    HookExtension(PFNGLPUSHDEBUGGROUPPROC, glPushDebugGroup); \
    HookExtension(PFNGLPOPDEBUGGROUPPROC, glPopDebugGroup); \
    HookExtension(PFNGLGETOBJECTLABELPROC, glGetObjectLabel); \
    HookExtension(PFNGLOBJECTLABELPROC, glObjectLabel); \
    HookExtension(PFNGLENABLEIPROC, glEnablei); \
    HookExtension(PFNGLDISABLEIPROC, glDisablei); \
    HookExtension(PFNGLISENABLEDIPROC, glIsEnabledi); \
    HookExtension(PFNGLGENBUFFERSPROC, glGenBuffers); \
    HookExtension(PFNGLBINDBUFFERPROC, glBindBuffer); \
    HookExtension(PFNGLDRAWBUFFERSPROC, glDrawBuffers); \
    HookExtension(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers); \
    HookExtension(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer); \
    HookExtension(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture); \
    HookExtension(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D); \
    HookExtension(PFNGLFRAMEBUFFERTEXTURELAYERPROC, glFramebufferTextureLayer); \
    HookExtension(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers); \
    HookExtension(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC, glGetFramebufferAttachmentParameteriv); \
    HookExtension(PFNGLFENCESYNCPROC, glFenceSync); \
    HookExtension(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync); \
    HookExtension(PFNGLWAITSYNCPROC, glWaitSync); \
    HookExtension(PFNGLDELETESYNCPROC, glDeleteSync); \
    HookExtension(PFNGLGENQUERIESPROC, glGenQueries); \
    HookExtension(PFNGLBEGINQUERYPROC, glBeginQuery); \
    HookExtension(PFNGLENDQUERYPROC, glEndQuery); \
    HookExtension(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v); \
    HookExtension(PFNGLGETQUERYOBJECTUIVPROC, glGetQueryObjectuiv); \
    HookExtension(PFNGLDELETEQUERIESPROC, glDeleteQueries); \
    HookExtension(PFNGLBUFFERDATAPROC, glBufferData); \
    HookExtension(PFNGLBUFFERSTORAGEPROC, glBufferStorage); \
    HookExtension(PFNGLBUFFERSUBDATAPROC, glBufferSubData); \
    HookExtension(PFNGLCOPYBUFFERSUBDATAPROC, glCopyBufferSubData); \
    HookExtension(PFNGLBINDBUFFERBASEPROC, glBindBufferBase); \
    HookExtension(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange); \
    HookExtension(PFNGLMAPBUFFERPROC, glMapBuffer); \
    HookExtension(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange); \
    HookExtension(PFNGLUNMAPBUFFERPROC, glUnmapBuffer); \
    HookExtension(PFNGLDELETEBUFFERSPROC, glDeleteBuffers); \
    HookExtension(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData); \
    HookExtension(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays); \
    HookExtension(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray); \
    HookExtension(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays); \
    HookExtension(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer); \
    HookExtension(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer); \
    HookExtension(PFNGLVERTEXATTRIBBINDINGPROC, glVertexAttribBinding); \
    HookExtension(PFNGLVERTEXATTRIBFORMATPROC, glVertexAttribFormat); \
    HookExtension(PFNGLVERTEXATTRIBIFORMATPROC, glVertexAttribIFormat); \
    HookExtension(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation); \
    HookExtension(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray); \
    HookExtension(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray); \
    HookExtension(PFNGLGETVERTEXATTRIBIVPROC, glGetVertexAttribiv); \
    HookExtension(PFNGLGETVERTEXATTRIBPOINTERVPROC, glGetVertexAttribPointerv); \
    HookExtension(PFNGLBINDVERTEXBUFFERPROC, glBindVertexBuffer); \
    HookExtension(PFNGLVERTEXBINDINGDIVISORPROC, glVertexBindingDivisor); \
    HookExtension(PFNGLGETCOMPRESSEDTEXIMAGEPROC, glGetCompressedTexImage); \
    HookExtension(PFNGLGENSAMPLERSPROC, glGenSamplers); \
    HookExtension(PFNGLBINDSAMPLERPROC, glBindSampler); \
    HookExtension(PFNGLDELETESAMPLERSPROC, glDeleteSamplers); \
    HookExtension(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri); \
    HookExtension(PFNGLSAMPLERPARAMETERFPROC, glSamplerParameterf); \
    HookExtension(PFNGLSAMPLERPARAMETERIVPROC, glSamplerParameteriv); \
    HookExtension(PFNGLSAMPLERPARAMETERFVPROC, glSamplerParameterfv); \
    HookExtension(PFNGLSAMPLERPARAMETERIIVPROC, glSamplerParameterIiv); \
    HookExtension(PFNGLSAMPLERPARAMETERIUIVPROC, glSamplerParameterIuiv); \
    HookExtension(PFNGLPATCHPARAMETERIPROC, glPatchParameteri); \
    HookExtension(PFNGLPATCHPARAMETERFVPROC, glPatchParameterfv); \
    HookExtension(PFNGLCLEARBUFFERFVPROC, glClearBufferfv); \
    HookExtension(PFNGLCLEARBUFFERIVPROC, glClearBufferiv); \
    HookExtension(PFNGLCLEARBUFFERUIVPROC, glClearBufferuiv); \
    HookExtension(PFNGLCLEARBUFFERFIPROC, glClearBufferfi); \
    HookExtension(PFNGLSCISSORARRAYVPROC, glScissorArrayv); \
    HookExtension(PFNGLSCISSORINDEXEDPROC, glScissorIndexed); \
    HookExtension(PFNGLSCISSORINDEXEDVPROC, glScissorIndexedv); \
    HookExtension(PFNGLVIEWPORTINDEXEDFPROC, glViewportIndexedf); \
    HookExtension(PFNGLVIEWPORTINDEXEDFVPROC, glViewportIndexedfv); \
    HookExtension(PFNGLVIEWPORTARRAYVPROC, glViewportArrayv); \
    HookExtension(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation); \
    HookExtension(PFNGLGETUNIFORMINDICESPROC, glGetUniformIndices); \
    HookExtension(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex); \
    HookExtension(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation); \
    HookExtension(PFNGLGETACTIVEUNIFORMPROC, glGetActiveUniform); \
    HookExtension(PFNGLGETACTIVEUNIFORMBLOCKIVPROC, glGetActiveUniformBlockiv); \
    HookExtension(PFNGLGETACTIVEUNIFORMSIVPROC, glGetActiveUniformsiv); \
    HookExtension(PFNGLGETACTIVEATTRIBPROC, glGetActiveAttrib); \
    HookExtension(PFNGLGETUNIFORMFVPROC, glGetUniformfv); \
    HookExtension(PFNGLGETUNIFORMIVPROC, glGetUniformiv); \
    HookExtension(PFNGLGETUNIFORMUIVPROC, glGetUniformuiv); \
    HookExtension(PFNGLGETUNIFORMDVPROC, glGetUniformdv); \
    HookExtension(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding); \
    HookExtension(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv); \
    HookExtension(PFNGLUNIFORM1FPROC, glUniform1f); \
    HookExtension(PFNGLUNIFORM1IPROC, glUniform1i); \
    HookExtension(PFNGLUNIFORM1UIPROC, glUniform1ui); \
    HookExtension(PFNGLUNIFORM1FVPROC, glUniform1fv); \
    HookExtension(PFNGLUNIFORM1IVPROC, glUniform1iv); \
    HookExtension(PFNGLUNIFORM1UIVPROC, glUniform1uiv); \
    HookExtension(PFNGLUNIFORM2FVPROC, glUniform2fv); \
    HookExtension(PFNGLUNIFORM3FVPROC, glUniform3fv); \
    HookExtension(PFNGLUNIFORM4FVPROC, glUniform4fv); \
    HookExtension(PFNGLDRAWRANGEELEMENTSPROC, glDrawRangeElements); \
    HookExtension(PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC, glDrawArraysInstancedBaseInstance); \
    HookExtension(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced); \
    HookExtension(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced); \
    HookExtension(PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC, glDrawElementsInstancedBaseInstance); \
    HookExtension(PFNGLDRAWELEMENTSBASEVERTEXPROC, glDrawElementsBaseVertex); \
    HookExtension(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC, glDrawElementsInstancedBaseVertex); \
    HookExtension(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC, glDrawElementsInstancedBaseVertexBaseInstance); \
    HookExtension(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer); \
    HookExtension(PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC, glCheckNamedFramebufferStatusEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC, glCompressedTextureImage1DEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC, glCompressedTextureImage2DEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC, glCompressedTextureImage3DEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC, glCompressedTextureSubImage1DEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC, glCompressedTextureSubImage2DEXT); \
    HookExtension(PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC, glCompressedTextureSubImage3DEXT); \
    HookExtension(PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC, glFramebufferDrawBuffersEXT); \
    HookExtension(PFNGLGENERATETEXTUREMIPMAPEXTPROC, glGenerateTextureMipmapEXT); \
    HookExtension(PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC, glGetCompressedTextureImageEXT); \
    HookExtension(PFNGLGETNAMEDBUFFERSUBDATAEXTPROC, glGetNamedBufferSubDataEXT); \
    HookExtension(PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC, glGetNamedBufferParameterivEXT); \
    HookExtension(PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC, glGetNamedFramebufferAttachmentParameterivEXT); \
    HookExtension(PFNGLGETTEXTUREIMAGEEXTPROC, glGetTextureImageEXT); \
    HookExtension(PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC, glGetTextureLevelParameterivEXT); \
    HookExtension(PFNGLMAPNAMEDBUFFEREXTPROC, glMapNamedBufferEXT); \
    HookExtension(PFNGLMAPNAMEDBUFFERRANGEEXTPROC, glMapNamedBufferRangeEXT); \
    HookExtension(PFNGLNAMEDBUFFERDATAEXTPROC, glNamedBufferDataEXT); \
    HookExtension(PFNGLNAMEDBUFFERSTORAGEEXTPROC, glNamedBufferStorageEXT); \
    HookExtension(PFNGLNAMEDBUFFERSUBDATAEXTPROC, glNamedBufferSubDataEXT); \
    HookExtension(PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC, glNamedCopyBufferSubDataEXT); \
    HookExtension(PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC, glNamedFramebufferTextureEXT); \
    HookExtension(PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC, glNamedFramebufferTexture2DEXT); \
    HookExtension(PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC, glNamedFramebufferTextureLayerEXT); \
    HookExtension(PFNGLTEXTUREBUFFERRANGEEXTPROC, glTextureBufferRangeEXT); \
    HookExtension(PFNGLTEXTUREIMAGE1DEXTPROC, glTextureImage1DEXT); \
    HookExtension(PFNGLTEXTUREIMAGE2DEXTPROC, glTextureImage2DEXT); \
    HookExtension(PFNGLTEXTUREIMAGE3DEXTPROC, glTextureImage3DEXT); \
    HookExtension(PFNGLTEXTUREPARAMETERFEXTPROC, glTextureParameterfEXT); \
    HookExtension(PFNGLTEXTUREPARAMETERFVEXTPROC, glTextureParameterfvEXT); \
    HookExtension(PFNGLTEXTUREPARAMETERIEXTPROC, glTextureParameteriEXT); \
    HookExtension(PFNGLTEXTUREPARAMETERIVEXTPROC, glTextureParameterivEXT); \
    HookExtension(PFNGLTEXTURESTORAGE1DEXTPROC, glTextureStorage1DEXT); \
    HookExtension(PFNGLTEXTURESTORAGE2DEXTPROC, glTextureStorage2DEXT); \
    HookExtension(PFNGLTEXTURESTORAGE3DEXTPROC, glTextureStorage3DEXT); \
    HookExtension(PFNGLTEXTURESUBIMAGE1DEXTPROC, glTextureSubImage1DEXT); \
    HookExtension(PFNGLTEXTURESUBIMAGE2DEXTPROC, glTextureSubImage2DEXT); \
    HookExtension(PFNGLTEXTURESUBIMAGE3DEXTPROC, glTextureSubImage3DEXT); \
    HookExtension(PFNGLUNMAPNAMEDBUFFEREXTPROC, glUnmapNamedBufferEXT); \
    HookExtension(PFNGLBINDTEXTUREPROC, glBindTexture); \
    HookExtension(PFNGLBLENDFUNCPROC, glBlendFunc); \
    HookExtension(PFNGLCLEARPROC, glClear); \
    HookExtension(PFNGLCLEARCOLORPROC, glClearColor); \
    HookExtension(PFNGLCLEARDEPTHPROC, glClearDepth); \
    HookExtension(PFNGLCOLORMASKPROC, glColorMask); \
    HookExtension(PFNGLCULLFACEPROC, glCullFace); \
    HookExtension(PFNGLDEPTHFUNCPROC, glDepthFunc); \
    HookExtension(PFNGLDEPTHMASKPROC, glDepthMask); \
    HookExtension(PFNGLSTENCILFUNCPROC, glStencilFunc); \
    HookExtension(PFNGLSTENCILMASKPROC, glStencilMask); \
    HookExtension(PFNGLSTENCILOPPROC, glStencilOp); \
    HookExtension(PFNGLDISABLEPROC, glDisable); \
    HookExtension(PFNGLDRAWBUFFERPROC, glDrawBuffer); \
    HookExtension(PFNGLDRAWELEMENTSPROC, glDrawElements); \
    HookExtension(PFNGLDRAWARRAYSPROC, glDrawArrays); \
    HookExtension(PFNGLENABLEPROC, glEnable); \
    HookExtension(PFNGLFLUSHPROC, glFlush); \
    HookExtension(PFNGLFINISHPROC, glFinish); \
    HookExtension(PFNGLFRONTFACEPROC, glFrontFace); \
    HookExtension(PFNGLGENTEXTURESPROC, glGenTextures); \
    HookExtension(PFNGLDELETETEXTURESPROC, glDeleteTextures); \
    HookExtension(PFNGLGETERRORPROC, glGetError); \
    HookExtension(PFNGLISENABLEDPROC, glIsEnabled); \
    HookExtension(PFNGLGETTEXLEVELPARAMETERIVPROC, glGetTexLevelParameteriv); \
    HookExtension(PFNGLGETTEXLEVELPARAMETERFVPROC, glGetTexLevelParameterfv); \
    HookExtension(PFNGLGETTEXPARAMETERFVPROC, glGetTexParameterfv); \
    HookExtension(PFNGLGETTEXPARAMETERIVPROC, glGetTexParameteriv); \
    HookExtension(PFNGLGETTEXIMAGEPROC, glGetTexImage); \
    HookExtension(PFNGLGETBOOLEANVPROC, glGetBooleanv); \
    HookExtension(PFNGLGETFLOATVPROC, glGetFloatv); \
    HookExtension(PFNGLGETDOUBLEVPROC, glGetDoublev); \
    HookExtension(PFNGLGETINTEGERVPROC, glGetIntegerv); \
    HookExtension(PFNGLGETSTRINGPROC, glGetString); \
    HookExtension(PFNGLHINTPROC, glHint); \
    HookExtension(PFNGLPIXELSTOREIPROC, glPixelStorei); \
    HookExtension(PFNGLPIXELSTOREFPROC, glPixelStoref); \
    HookExtension(PFNGLPOLYGONMODEPROC, glPolygonMode); \
    HookExtension(PFNGLPOLYGONOFFSETPROC, glPolygonOffset); \
    HookExtension(PFNGLREADPIXELSPROC, glReadPixels); \
    HookExtension(PFNGLREADBUFFERPROC, glReadBuffer); \
    HookExtension(PFNGLSCISSORPROC, glScissor); \
    HookExtension(PFNGLTEXIMAGE1DPROC, glTexImage1D); \
    HookExtension(PFNGLTEXIMAGE2DPROC, glTexImage2D); \
    HookExtension(PFNGLTEXIMAGE3DPROC, glTexImage3D); \
    HookExtension(PFNGLTEXSUBIMAGE1DPROC, glTexSubImage1D); \
    HookExtension(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D); \
    HookExtension(PFNGLTEXPARAMETERFPROC, glTexParameterf); \
    HookExtension(PFNGLTEXPARAMETERFVPROC, glTexParameterfv); \
    HookExtension(PFNGLTEXPARAMETERIPROC, glTexParameteri); \
    HookExtension(PFNGLTEXPARAMETERIVPROC, glTexParameteriv); \
    HookExtension(PFNGLVIEWPORTPROC, glViewport); \



// dllexport functions
#define DefineDLLExportHooks() \
    HookWrapper2(void, glBindTexture, GLenum, target, GLuint, texture); \
    HookWrapper2(void, glBlendFunc, GLenum, sfactor, GLenum, dfactor); \
    HookWrapper1(void, glClear, GLbitfield, mask); \
    HookWrapper4(void, glClearColor, GLfloat, red, GLfloat, green, GLfloat, blue, GLfloat, alpha); \
    HookWrapper1(void, glClearDepth, GLdouble, depth); \
    HookWrapper4(void, glColorMask, GLboolean, red, GLboolean, green, GLboolean, blue, GLboolean, alpha); \
    HookWrapper1(void, glCullFace, GLenum, mode); \
    HookWrapper1(void, glDepthFunc, GLenum, func); \
    HookWrapper1(void, glDepthMask, GLboolean, flag); \
    HookWrapper3(void, glStencilFunc, GLenum, func, GLint, ref, GLuint, mask); \
    HookWrapper1(void, glStencilMask, GLuint, mask); \
    HookWrapper3(void, glStencilOp, GLenum, fail, GLenum, zfail, GLenum, zpass); \
    HookWrapper1(void, glDisable, GLenum, cap); \
    HookWrapper1(void, glDrawBuffer, GLenum, mode); \
    HookWrapper4(void, glDrawElements, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices); \
    HookWrapper3(void, glDrawArrays, GLenum, mode, GLint, first, GLsizei, count); \
    HookWrapper1(void, glEnable, GLenum, cap); \
    HookWrapper0(void, glFlush); \
    HookWrapper0(void, glFinish); \
    HookWrapper1(void, glFrontFace, GLenum, mode); \
    HookWrapper2(void, glGenTextures, GLsizei, n, GLuint *, textures); \
    HookWrapper2(void, glDeleteTextures, GLsizei, n, const GLuint *, textures); \
    HookWrapper0(GLenum, glGetError); \
    HookWrapper1(GLboolean, glIsEnabled, GLenum, cap); \
    HookWrapper4(void, glGetTexLevelParameteriv, GLenum, target, GLint, level, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetTexLevelParameterfv, GLenum, target, GLint, level, GLenum, pname, GLfloat *, params); \
    HookWrapper3(void, glGetTexParameterfv, GLenum, target, GLenum, pname, GLfloat *, params); \
    HookWrapper3(void, glGetTexParameteriv, GLenum, target, GLenum, pname, GLint *, params); \
    HookWrapper5(void, glGetTexImage, GLenum, target, GLint, level, GLenum, format, GLenum, type, void *, pixels); \
    HookWrapper2(void, glGetBooleanv, GLenum, pname, GLboolean *, data); \
    HookWrapper2(void, glGetFloatv, GLenum, pname, GLfloat *, data); \
    HookWrapper2(void, glGetDoublev, GLenum, pname, GLdouble *, data); \
    HookWrapper2(void, glGetIntegerv, GLenum, pname, GLint *, data); \
    HookWrapper1(const GLubyte *, glGetString, GLenum, name); \
    HookWrapper2(void, glHint, GLenum, target, GLenum, mode); \
    HookWrapper2(void, glPixelStorei, GLenum, pname, GLint, param); \
    HookWrapper2(void, glPixelStoref, GLenum, pname, GLfloat, param); \
    HookWrapper2(void, glPolygonMode, GLenum, face, GLenum, mode); \
    HookWrapper2(void, glPolygonOffset, GLfloat, factor, GLfloat, units); \
    HookWrapper7(void, glReadPixels, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, void *, pixels); \
    HookWrapper1(void, glReadBuffer, GLenum, mode); \
    HookWrapper4(void, glScissor, GLint, x, GLint, y, GLsizei, width, GLsizei, height); \
    HookWrapper8(void, glTexImage1D, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper9(void, glTexImage2D, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper10(void, glTexImage3D, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper7(void, glTexSubImage1D, GLenum, target, GLint, level, GLint, xoffset, GLsizei, width, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper9(void, glTexSubImage2D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper3(void, glTexParameterf, GLenum, target, GLenum, pname, GLfloat, param); \
    HookWrapper3(void, glTexParameterfv, GLenum, target, GLenum, pname, const GLfloat *, params); \
    HookWrapper3(void, glTexParameteri, GLenum, target, GLenum, pname, GLint, param); \
    HookWrapper3(void, glTexParameteriv, GLenum, target, GLenum, pname, const GLint *, params); \
    HookWrapper4(void, glViewport, GLint, x, GLint, y, GLsizei, width, GLsizei, height); \



// wgl extensions
#define DefineWGLExtensionHooks() \



// glx extensions
#define DefineGLXExtensionHooks() \



// gl extensions
#define DefineGLExtensionHooks() \
    HookWrapper1(void, glActiveTexture, GLenum, texture); \
    HookWrapper4(void, glTexStorage1D, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width); \
    HookWrapper5(void, glTexStorage2D, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width, GLsizei, height); \
    HookWrapper6(void, glTexStorage3D, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth); \
    HookWrapper11(void, glTexSubImage3D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper7(void, glCompressedTexImage1D, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLint, border, GLsizei, imageSize, const void *, data); \
    HookWrapper8(void, glCompressedTexImage2D, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLsizei, imageSize, const void *, data); \
    HookWrapper9(void, glCompressedTexImage3D, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth, GLint, border, GLsizei, imageSize, const void *, data); \
    HookWrapper7(void, glCompressedTexSubImage1D, GLenum, target, GLint, level, GLint, xoffset, GLsizei, width, GLenum, format, GLsizei, imageSize, const void *, data); \
    HookWrapper9(void, glCompressedTexSubImage2D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLsizei, imageSize, const void *, data); \
    HookWrapper11(void, glCompressedTexSubImage3D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLsizei, imageSize, const void *, data); \
    HookWrapper5(void, glTexBufferRange, GLenum, target, GLenum, internalformat, GLuint, buffer, GLintptr, offset, GLsizeiptr, size); \
    HookWrapper8(void, glTextureView, GLuint, texture, GLenum, target, GLuint, origtexture, GLenum, internalformat, GLuint, minlevel, GLuint, numlevels, GLuint, minlayer, GLuint, numlayers); \
    HookWrapper1(void, glGenerateMipmap, GLenum, target); \
    HookWrapper15(void, glCopyImageSubData, GLuint, srcName, GLenum, srcTarget, GLint, srcLevel, GLint, srcX, GLint, srcY, GLint, srcZ, GLuint, dstName, GLenum, dstTarget, GLint, dstLevel, GLint, dstX, GLint, dstY, GLint, dstZ, GLsizei, srcWidth, GLsizei, srcHeight, GLsizei, srcDepth); \
    HookWrapper5(void, glGetInternalformativ, GLenum, target, GLenum, internalformat, GLenum, pname, GLsizei, bufSize, GLint *, params); \
    HookWrapper5(void, glGetInternalformati64v, GLenum, target, GLenum, internalformat, GLenum, pname, GLsizei, bufSize, GLint64 *, params); \
    HookWrapper3(void, glGetBufferParameteriv, GLenum, target, GLenum, pname, GLint *, params); \
    HookWrapper2(const GLubyte *, glGetStringi, GLenum, name, GLuint, index); \
    HookWrapper3(void, glGetBooleani_v, GLenum, target, GLuint, index, GLboolean *, data); \
    HookWrapper3(void, glGetIntegeri_v, GLenum, target, GLuint, index, GLint *, data); \
    HookWrapper3(void, glGetFloati_v, GLenum, target, GLuint, index, GLfloat *, data); \
    HookWrapper3(void, glGetDoublei_v, GLenum, target, GLuint, index, GLdouble *, data); \
    HookWrapper3(void, glGetInteger64i_v, GLenum, target, GLuint, index, GLint64 *, data); \
    HookWrapper1(GLenum, glCheckFramebufferStatus, GLenum, target); \
    HookWrapper4(void, glBlendColor, GLfloat, red, GLfloat, green, GLfloat, blue, GLfloat, alpha); \
    HookWrapper3(void, glBlendFunci, GLuint, buf, GLenum, src, GLenum, dst); \
    HookWrapper4(void, glBlendFuncSeparate, GLenum, sfactorRGB, GLenum, dfactorRGB, GLenum, sfactorAlpha, GLenum, dfactorAlpha); \
    HookWrapper5(void, glBlendFuncSeparatei, GLuint, buf, GLenum, srcRGB, GLenum, dstRGB, GLenum, srcAlpha, GLenum, dstAlpha); \
    HookWrapper2(void, glBlendEquationSeparate, GLenum, modeRGB, GLenum, modeAlpha); \
    HookWrapper3(void, glBlendEquationSeparatei, GLuint, buf, GLenum, modeRGB, GLenum, modeAlpha); \
    HookWrapper4(void, glStencilFuncSeparate, GLenum, face, GLenum, func, GLint, ref, GLuint, mask); \
    HookWrapper2(void, glStencilMaskSeparate, GLenum, face, GLuint, mask); \
    HookWrapper4(void, glStencilOpSeparate, GLenum, face, GLenum, sfail, GLenum, dpfail, GLenum, dppass); \
    HookWrapper5(void, glColorMaski, GLuint, index, GLboolean, r, GLboolean, g, GLboolean, b, GLboolean, a); \
    HookWrapper2(void, glSampleMaski, GLuint, maskNumber, GLbitfield, mask); \
    HookWrapper2(void, glDepthRange, GLdouble, near, GLdouble, far); \
    HookWrapper2(void, glDepthRangef, GLfloat, n, GLfloat, f); \
    HookWrapper3(void, glDepthRangeArrayv, GLuint, first, GLsizei, count, const GLdouble *, v); \
    HookWrapper2(void, glDepthBoundsEXT, GLclampd, zmin, GLclampd, zmax); \
    HookWrapper1(GLuint, glCreateShader, GLenum, type); \
    HookWrapper1(void, glDeleteShader, GLuint, shader); \
    HookWrapper4(void, glShaderSource, GLuint, shader, GLsizei, count, const GLchar *const*, string, const GLint *, length); \
    HookWrapper1(void, glCompileShader, GLuint, shader); \
    HookWrapper3(GLuint, glCreateShaderProgramv, GLenum, type, GLsizei, count, const GLchar *const*, strings); \
    HookWrapper3(void, glGetShaderiv, GLuint, shader, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetShaderInfoLog, GLuint, shader, GLsizei, bufSize, GLsizei *, length, GLchar *, infoLog); \
    HookWrapper0(GLuint, glCreateProgram); \
    HookWrapper1(void, glDeleteProgram, GLuint, program); \
    HookWrapper2(void, glAttachShader, GLuint, program, GLuint, shader); \
    HookWrapper2(void, glDetachShader, GLuint, program, GLuint, shader); \
    HookWrapper0(void, glReleaseShaderCompiler); \
    HookWrapper1(void, glLinkProgram, GLuint, program); \
    HookWrapper3(void, glProgramParameteri, GLuint, program, GLenum, pname, GLint, value); \
    HookWrapper3(void, glProgramUniform1i, GLuint, program, GLint, location, GLint, v0); \
    HookWrapper4(void, glProgramUniform1fv, GLuint, program, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper4(void, glProgramUniform1iv, GLuint, program, GLint, location, GLsizei, count, const GLint *, value); \
    HookWrapper4(void, glProgramUniform1uiv, GLuint, program, GLint, location, GLsizei, count, const GLuint *, value); \
    HookWrapper4(void, glProgramUniform2fv, GLuint, program, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper4(void, glProgramUniform3fv, GLuint, program, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper4(void, glProgramUniform4fv, GLuint, program, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper1(void, glUseProgram, GLuint, program); \
    HookWrapper3(void, glUseProgramStages, GLuint, pipeline, GLbitfield, stages, GLuint, program); \
    HookWrapper1(void, glValidateProgram, GLuint, program); \
    HookWrapper3(void, glGetProgramiv, GLuint, program, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetProgramInfoLog, GLuint, program, GLsizei, bufSize, GLsizei *, length, GLchar *, infoLog); \
    HookWrapper4(void, glGetProgramInterfaceiv, GLuint, program, GLenum, programInterface, GLenum, pname, GLint *, params); \
    HookWrapper3(GLuint, glGetProgramResourceIndex, GLuint, program, GLenum, programInterface, const GLchar *, name); \
    HookWrapper8(void, glGetProgramResourceiv, GLuint, program, GLenum, programInterface, GLuint, index, GLsizei, propCount, const GLenum *, props, GLsizei, bufSize, GLsizei *, length, GLint *, params); \
    HookWrapper6(void, glGetProgramResourceName, GLuint, program, GLenum, programInterface, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLchar *, name); \
    HookWrapper2(void, glGenProgramPipelines, GLsizei, n, GLuint *, pipelines); \
    HookWrapper1(void, glBindProgramPipeline, GLuint, pipeline); \
    HookWrapper2(void, glDeleteProgramPipelines, GLsizei, n, const GLuint *, pipelines); \
    HookWrapper3(void, glGetProgramPipelineiv, GLuint, pipeline, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetProgramPipelineInfoLog, GLuint, pipeline, GLsizei, bufSize, GLsizei *, length, GLchar *, infoLog); \
    HookWrapper1(void, glValidateProgramPipeline, GLuint, pipeline); \
    HookWrapper2(void, glDebugMessageCallback, GLDEBUGPROC, callback, const void *, userParam); \
    HookWrapper6(void, glDebugMessageControl, GLenum, source, GLenum, type, GLenum, severity, GLsizei, count, const GLuint *, ids, GLboolean, enabled); \
    HookWrapper6(void, glDebugMessageInsert, GLenum, source, GLenum, type, GLuint, id, GLenum, severity, GLsizei, length, const GLchar *, buf); \
    HookWrapper4(void, glPushDebugGroup, GLenum, source, GLuint, id, GLsizei, length, const GLchar *, message); \
    HookWrapper0(void, glPopDebugGroup); \
    HookWrapper5(void, glGetObjectLabel, GLenum, identifier, GLuint, name, GLsizei, bufSize, GLsizei *, length, GLchar *, label); \
    HookWrapper4(void, glObjectLabel, GLenum, identifier, GLuint, name, GLsizei, length, const GLchar *, label); \
    HookWrapper2(void, glEnablei, GLenum, target, GLuint, index); \
    HookWrapper2(void, glDisablei, GLenum, target, GLuint, index); \
    HookWrapper2(GLboolean, glIsEnabledi, GLenum, target, GLuint, index); \
    HookWrapper2(void, glGenBuffers, GLsizei, n, GLuint *, buffers); \
    HookWrapper2(void, glBindBuffer, GLenum, target, GLuint, buffer); \
    HookWrapper2(void, glDrawBuffers, GLsizei, n, const GLenum *, bufs); \
    HookWrapper2(void, glGenFramebuffers, GLsizei, n, GLuint *, framebuffers); \
    HookWrapper2(void, glBindFramebuffer, GLenum, target, GLuint, framebuffer); \
    HookWrapper4(void, glFramebufferTexture, GLenum, target, GLenum, attachment, GLuint, texture, GLint, level); \
    HookWrapper5(void, glFramebufferTexture2D, GLenum, target, GLenum, attachment, GLenum, textarget, GLuint, texture, GLint, level); \
    HookWrapper5(void, glFramebufferTextureLayer, GLenum, target, GLenum, attachment, GLuint, texture, GLint, level, GLint, layer); \
    HookWrapper2(void, glDeleteFramebuffers, GLsizei, n, const GLuint *, framebuffers); \
    HookWrapper4(void, glGetFramebufferAttachmentParameteriv, GLenum, target, GLenum, attachment, GLenum, pname, GLint *, params); \
    HookWrapper2(GLsync, glFenceSync, GLenum, condition, GLbitfield, flags); \
    HookWrapper3(GLenum, glClientWaitSync, GLsync, sync, GLbitfield, flags, GLuint64, timeout); \
    HookWrapper3(void, glWaitSync, GLsync, sync, GLbitfield, flags, GLuint64, timeout); \
    HookWrapper1(void, glDeleteSync, GLsync, sync); \
    HookWrapper2(void, glGenQueries, GLsizei, n, GLuint *, ids); \
    HookWrapper2(void, glBeginQuery, GLenum, target, GLuint, id); \
    HookWrapper1(void, glEndQuery, GLenum, target); \
    HookWrapper3(void, glGetQueryObjectui64v, GLuint, id, GLenum, pname, GLuint64 *, params); \
    HookWrapper3(void, glGetQueryObjectuiv, GLuint, id, GLenum, pname, GLuint *, params); \
    HookWrapper2(void, glDeleteQueries, GLsizei, n, const GLuint *, ids); \
    HookWrapper4(void, glBufferData, GLenum, target, GLsizeiptr, size, const void *, data, GLenum, usage); \
    HookWrapper4(void, glBufferStorage, GLenum, target, GLsizeiptr, size, const void *, data, GLbitfield, flags); \
    HookWrapper4(void, glBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, const void *, data); \
    HookWrapper5(void, glCopyBufferSubData, GLenum, readTarget, GLenum, writeTarget, GLintptr, readOffset, GLintptr, writeOffset, GLsizeiptr, size); \
    HookWrapper3(void, glBindBufferBase, GLenum, target, GLuint, index, GLuint, buffer); \
    HookWrapper5(void, glBindBufferRange, GLenum, target, GLuint, index, GLuint, buffer, GLintptr, offset, GLsizeiptr, size); \
    HookWrapper2(void *, glMapBuffer, GLenum, target, GLenum, access); \
    HookWrapper4(void *, glMapBufferRange, GLenum, target, GLintptr, offset, GLsizeiptr, length, GLbitfield, access); \
    HookWrapper1(GLboolean, glUnmapBuffer, GLenum, target); \
    HookWrapper2(void, glDeleteBuffers, GLsizei, n, const GLuint *, buffers); \
    HookWrapper4(void, glGetBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, void *, data); \
    HookWrapper2(void, glGenVertexArrays, GLsizei, n, GLuint *, arrays); \
    HookWrapper1(void, glBindVertexArray, GLuint, array); \
    HookWrapper2(void, glDeleteVertexArrays, GLsizei, n, const GLuint *, arrays); \
    HookWrapper6(void, glVertexAttribPointer, GLuint, index, GLint, size, GLenum, type, GLboolean, normalized, GLsizei, stride, const void *, pointer); \
    HookWrapper5(void, glVertexAttribIPointer, GLuint, index, GLint, size, GLenum, type, GLsizei, stride, const void *, pointer); \
    HookWrapper2(void, glVertexAttribBinding, GLuint, attribindex, GLuint, bindingindex); \
    HookWrapper5(void, glVertexAttribFormat, GLuint, attribindex, GLint, size, GLenum, type, GLboolean, normalized, GLuint, relativeoffset); \
    HookWrapper4(void, glVertexAttribIFormat, GLuint, attribindex, GLint, size, GLenum, type, GLuint, relativeoffset); \
    HookWrapper3(void, glBindAttribLocation, GLuint, program, GLuint, index, const GLchar *, name); \
    HookWrapper1(void, glEnableVertexAttribArray, GLuint, index); \
    HookWrapper1(void, glDisableVertexAttribArray, GLuint, index); \
    HookWrapper3(void, glGetVertexAttribiv, GLuint, index, GLenum, pname, GLint *, params); \
    HookWrapper3(void, glGetVertexAttribPointerv, GLuint, index, GLenum, pname, void **, pointer); \
    HookWrapper4(void, glBindVertexBuffer, GLuint, bindingindex, GLuint, buffer, GLintptr, offset, GLsizei, stride); \
    HookWrapper2(void, glVertexBindingDivisor, GLuint, bindingindex, GLuint, divisor); \
    HookWrapper3(void, glGetCompressedTexImage, GLenum, target, GLint, level, void *, img); \
    HookWrapper2(void, glGenSamplers, GLsizei, count, GLuint *, samplers); \
    HookWrapper2(void, glBindSampler, GLuint, unit, GLuint, sampler); \
    HookWrapper2(void, glDeleteSamplers, GLsizei, count, const GLuint *, samplers); \
    HookWrapper3(void, glSamplerParameteri, GLuint, sampler, GLenum, pname, GLint, param); \
    HookWrapper3(void, glSamplerParameterf, GLuint, sampler, GLenum, pname, GLfloat, param); \
    HookWrapper3(void, glSamplerParameteriv, GLuint, sampler, GLenum, pname, const GLint *, param); \
    HookWrapper3(void, glSamplerParameterfv, GLuint, sampler, GLenum, pname, const GLfloat *, param); \
    HookWrapper3(void, glSamplerParameterIiv, GLuint, sampler, GLenum, pname, const GLint *, param); \
    HookWrapper3(void, glSamplerParameterIuiv, GLuint, sampler, GLenum, pname, const GLuint *, param); \
    HookWrapper2(void, glPatchParameteri, GLenum, pname, GLint, value); \
    HookWrapper2(void, glPatchParameterfv, GLenum, pname, const GLfloat *, values); \
    HookWrapper3(void, glClearBufferfv, GLenum, buffer, GLint, drawbuffer, const GLfloat *, value); \
    HookWrapper3(void, glClearBufferiv, GLenum, buffer, GLint, drawbuffer, const GLint *, value); \
    HookWrapper3(void, glClearBufferuiv, GLenum, buffer, GLint, drawbuffer, const GLuint *, value); \
    HookWrapper4(void, glClearBufferfi, GLenum, buffer, GLint, drawbuffer, GLfloat, depth, GLint, stencil); \
    HookWrapper3(void, glScissorArrayv, GLuint, first, GLsizei, count, const GLint *, v); \
    HookWrapper5(void, glScissorIndexed, GLuint, index, GLint, left, GLint, bottom, GLsizei, width, GLsizei, height); \
    HookWrapper2(void, glScissorIndexedv, GLuint, index, const GLint *, v); \
    HookWrapper5(void, glViewportIndexedf, GLuint, index, GLfloat, x, GLfloat, y, GLfloat, w, GLfloat, h); \
    HookWrapper2(void, glViewportIndexedfv, GLuint, index, const GLfloat *, v); \
    HookWrapper3(void, glViewportArrayv, GLuint, first, GLsizei, count, const GLfloat *, v); \
    HookWrapper2(GLint, glGetUniformLocation, GLuint, program, const GLchar *, name); \
    HookWrapper4(void, glGetUniformIndices, GLuint, program, GLsizei, uniformCount, const GLchar *const*, uniformNames, GLuint *, uniformIndices); \
    HookWrapper2(GLuint, glGetUniformBlockIndex, GLuint, program, const GLchar *, uniformBlockName); \
    HookWrapper2(GLint, glGetAttribLocation, GLuint, program, const GLchar *, name); \
    HookWrapper7(void, glGetActiveUniform, GLuint, program, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLint *, size, GLenum *, type, GLchar *, name); \
    HookWrapper4(void, glGetActiveUniformBlockiv, GLuint, program, GLuint, uniformBlockIndex, GLenum, pname, GLint *, params); \
    HookWrapper5(void, glGetActiveUniformsiv, GLuint, program, GLsizei, uniformCount, const GLuint *, uniformIndices, GLenum, pname, GLint *, params); \
    HookWrapper7(void, glGetActiveAttrib, GLuint, program, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLint *, size, GLenum *, type, GLchar *, name); \
    HookWrapper3(void, glGetUniformfv, GLuint, program, GLint, location, GLfloat *, params); \
    HookWrapper3(void, glGetUniformiv, GLuint, program, GLint, location, GLint *, params); \
    HookWrapper3(void, glGetUniformuiv, GLuint, program, GLint, location, GLuint *, params); \
    HookWrapper3(void, glGetUniformdv, GLuint, program, GLint, location, GLdouble *, params); \
    HookWrapper3(void, glUniformBlockBinding, GLuint, program, GLuint, uniformBlockIndex, GLuint, uniformBlockBinding); \
    HookWrapper4(void, glUniformMatrix4fv, GLint, location, GLsizei, count, GLboolean, transpose, const GLfloat *, value); \
    HookWrapper2(void, glUniform1f, GLint, location, GLfloat, v0); \
    HookWrapper2(void, glUniform1i, GLint, location, GLint, v0); \
    HookWrapper2(void, glUniform1ui, GLint, location, GLuint, v0); \
    HookWrapper3(void, glUniform1fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper3(void, glUniform1iv, GLint, location, GLsizei, count, const GLint *, value); \
    HookWrapper3(void, glUniform1uiv, GLint, location, GLsizei, count, const GLuint *, value); \
    HookWrapper3(void, glUniform2fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper3(void, glUniform3fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper3(void, glUniform4fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper6(void, glDrawRangeElements, GLenum, mode, GLuint, start, GLuint, end, GLsizei, count, GLenum, type, const void *, indices); \
    HookWrapper5(void, glDrawArraysInstancedBaseInstance, GLenum, mode, GLint, first, GLsizei, count, GLsizei, instancecount, GLuint, baseinstance); \
    HookWrapper4(void, glDrawArraysInstanced, GLenum, mode, GLint, first, GLsizei, count, GLsizei, instancecount); \
    HookWrapper5(void, glDrawElementsInstanced, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, instancecount); \
    HookWrapper6(void, glDrawElementsInstancedBaseInstance, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, instancecount, GLuint, baseinstance); \
    HookWrapper5(void, glDrawElementsBaseVertex, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLint, basevertex); \
    HookWrapper6(void, glDrawElementsInstancedBaseVertex, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, instancecount, GLint, basevertex); \
    HookWrapper7(void, glDrawElementsInstancedBaseVertexBaseInstance, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, instancecount, GLint, basevertex, GLuint, baseinstance); \
    HookWrapper10(void, glBlitFramebuffer, GLint, srcX0, GLint, srcY0, GLint, srcX1, GLint, srcY1, GLint, dstX0, GLint, dstY0, GLint, dstX1, GLint, dstY1, GLbitfield, mask, GLenum, filter); \
    HookWrapper2(GLenum, glCheckNamedFramebufferStatusEXT, GLuint, framebuffer, GLenum, target); \
    HookWrapper8(void, glCompressedTextureImage1DEXT, GLuint, texture, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLint, border, GLsizei, imageSize, const void *, bits); \
    HookWrapper9(void, glCompressedTextureImage2DEXT, GLuint, texture, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLsizei, imageSize, const void *, bits); \
    HookWrapper10(void, glCompressedTextureImage3DEXT, GLuint, texture, GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth, GLint, border, GLsizei, imageSize, const void *, bits); \
    HookWrapper8(void, glCompressedTextureSubImage1DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLsizei, width, GLenum, format, GLsizei, imageSize, const void *, bits); \
    HookWrapper10(void, glCompressedTextureSubImage2DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLsizei, imageSize, const void *, bits); \
    HookWrapper12(void, glCompressedTextureSubImage3DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLsizei, imageSize, const void *, bits); \
    HookWrapper3(void, glFramebufferDrawBuffersEXT, GLuint, framebuffer, GLsizei, n, const GLenum *, bufs); \
    HookWrapper2(void, glGenerateTextureMipmapEXT, GLuint, texture, GLenum, target); \
    HookWrapper4(void, glGetCompressedTextureImageEXT, GLuint, texture, GLenum, target, GLint, lod, void *, img); \
    HookWrapper4(void, glGetNamedBufferSubDataEXT, GLuint, buffer, GLintptr, offset, GLsizeiptr, size, void *, data); \
    HookWrapper3(void, glGetNamedBufferParameterivEXT, GLuint, buffer, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetNamedFramebufferAttachmentParameterivEXT, GLuint, framebuffer, GLenum, attachment, GLenum, pname, GLint *, params); \
    HookWrapper6(void, glGetTextureImageEXT, GLuint, texture, GLenum, target, GLint, level, GLenum, format, GLenum, type, void *, pixels); \
    HookWrapper5(void, glGetTextureLevelParameterivEXT, GLuint, texture, GLenum, target, GLint, level, GLenum, pname, GLint *, params); \
    HookWrapper2(void *, glMapNamedBufferEXT, GLuint, buffer, GLenum, access); \
    HookWrapper4(void *, glMapNamedBufferRangeEXT, GLuint, buffer, GLintptr, offset, GLsizeiptr, length, GLbitfield, access); \
    HookWrapper4(void, glNamedBufferDataEXT, GLuint, buffer, GLsizeiptr, size, const void *, data, GLenum, usage); \
    HookWrapper4(void, glNamedBufferStorageEXT, GLuint, buffer, GLsizeiptr, size, const void *, data, GLbitfield, flags); \
    HookWrapper4(void, glNamedBufferSubDataEXT, GLuint, buffer, GLintptr, offset, GLsizeiptr, size, const void *, data); \
    HookWrapper5(void, glNamedCopyBufferSubDataEXT, GLuint, readBuffer, GLuint, writeBuffer, GLintptr, readOffset, GLintptr, writeOffset, GLsizeiptr, size); \
    HookWrapper4(void, glNamedFramebufferTextureEXT, GLuint, framebuffer, GLenum, attachment, GLuint, texture, GLint, level); \
    HookWrapper5(void, glNamedFramebufferTexture2DEXT, GLuint, framebuffer, GLenum, attachment, GLenum, textarget, GLuint, texture, GLint, level); \
    HookWrapper5(void, glNamedFramebufferTextureLayerEXT, GLuint, framebuffer, GLenum, attachment, GLuint, texture, GLint, level, GLint, layer); \
    HookWrapper6(void, glTextureBufferRangeEXT, GLuint, texture, GLenum, target, GLenum, internalformat, GLuint, buffer, GLintptr, offset, GLsizeiptr, size); \
    HookWrapper9(void, glTextureImage1DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper10(void, glTextureImage2DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper11(void, glTextureImage3DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth, GLint, border, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper4(void, glTextureParameterfEXT, GLuint, texture, GLenum, target, GLenum, pname, GLfloat, param); \
    HookWrapper4(void, glTextureParameterfvEXT, GLuint, texture, GLenum, target, GLenum, pname, const GLfloat *, params); \
    HookWrapper4(void, glTextureParameteriEXT, GLuint, texture, GLenum, target, GLenum, pname, GLint, param); \
    HookWrapper4(void, glTextureParameterivEXT, GLuint, texture, GLenum, target, GLenum, pname, const GLint *, params); \
    HookWrapper5(void, glTextureStorage1DEXT, GLuint, texture, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width); \
    HookWrapper6(void, glTextureStorage2DEXT, GLuint, texture, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width, GLsizei, height); \
    HookWrapper7(void, glTextureStorage3DEXT, GLuint, texture, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width, GLsizei, height, GLsizei, depth); \
    HookWrapper8(void, glTextureSubImage1DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLsizei, width, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper10(void, glTextureSubImage2DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper12(void, glTextureSubImage3DEXT, GLuint, texture, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper1(GLboolean, glUnmapNamedBufferEXT, GLuint, buffer); \




