/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

class WrappedOpenGL;

typedef std::function<void *(const char *)> PlatformGetProcAddr;

// We need to disable clang-format since this struct is programmatically parsed
// clang-format off
struct GLDispatchTable
{
  // This function can be used to populate the dispatch table, fully or partiall. Any NULL function
  // will be passed to the callback to get a pointer
  void PopulateWithCallback(PlatformGetProcAddr lookupFunc);

  // These functions are called after fully populating the hookset, to emulate functions that are
  // one way or another unsupported but we can software emulate them and want to assume their
  // presence.
  void EmulateUnsupportedFunctions();
  void EmulateRequiredExtensions();
  void DriverForEmulation(WrappedOpenGL *driver);

  // first we list all the core functions. 1.1 functions are separate under 'dllexport' for
  // different handling on windows. Extensions come after.
  // Any Core functions that are semantically identical to extension variants are listed as
  // 'aliases' such that if the 'alias' is requested via *GetProcAddress, the core function
  // will be returned and used.
  
  PFNGLBINDTEXTUREPROC glBindTexture;
  PFNGLBLENDFUNCPROC glBlendFunc;
  PFNGLCLEARPROC glClear;
  PFNGLCLEARCOLORPROC glClearColor;
  PFNGLCLEARDEPTHPROC glClearDepth;
  PFNGLCLEARSTENCILPROC glClearStencil;
  PFNGLCOLORMASKPROC glColorMask;
  PFNGLCULLFACEPROC glCullFace;
  PFNGLDEPTHFUNCPROC glDepthFunc;
  PFNGLDEPTHMASKPROC glDepthMask;
  PFNGLDEPTHRANGEPROC glDepthRange;
  PFNGLSTENCILFUNCPROC glStencilFunc;
  PFNGLSTENCILMASKPROC glStencilMask;
  PFNGLSTENCILOPPROC glStencilOp;
  PFNGLDISABLEPROC glDisable;
  PFNGLDRAWBUFFERPROC glDrawBuffer;
  PFNGLDRAWELEMENTSPROC glDrawElements;
  PFNGLDRAWARRAYSPROC glDrawArrays;
  PFNGLENABLEPROC glEnable;
  PFNGLFLUSHPROC glFlush;
  PFNGLFINISHPROC glFinish;
  PFNGLFRONTFACEPROC glFrontFace;
  PFNGLGENTEXTURESPROC glGenTextures;
  PFNGLDELETETEXTURESPROC glDeleteTextures;
  PFNGLISENABLEDPROC glIsEnabled;
  PFNGLISTEXTUREPROC glIsTexture;
  PFNGLGETERRORPROC glGetError;
  PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
  PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
  PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
  PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
  PFNGLGETTEXIMAGEPROC glGetTexImage;
  PFNGLGETBOOLEANVPROC glGetBooleanv;
  PFNGLGETFLOATVPROC glGetFloatv;
  PFNGLGETDOUBLEVPROC glGetDoublev;
  PFNGLGETINTEGERVPROC glGetIntegerv;
  PFNGLGETPOINTERVPROC glGetPointerv;    // aliases glGetPointervKHR
  PFNGLGETSTRINGPROC glGetString;
  PFNGLHINTPROC glHint;
  PFNGLLOGICOPPROC glLogicOp;
  PFNGLPIXELSTOREIPROC glPixelStorei;
  PFNGLPIXELSTOREFPROC glPixelStoref;
  PFNGLPOLYGONMODEPROC glPolygonMode;
  PFNGLPOLYGONOFFSETPROC glPolygonOffset;
  PFNGLPOINTSIZEPROC glPointSize;
  PFNGLLINEWIDTHPROC glLineWidth;
  PFNGLREADPIXELSPROC glReadPixels;
  PFNGLREADBUFFERPROC glReadBuffer;
  PFNGLSCISSORPROC glScissor;
  PFNGLTEXIMAGE1DPROC glTexImage1D;
  PFNGLTEXIMAGE2DPROC glTexImage2D;
  PFNGLTEXSUBIMAGE1DPROC glTexSubImage1D;
  PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
  PFNGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
  PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
  PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
  PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
  PFNGLTEXPARAMETERFPROC glTexParameterf;
  PFNGLTEXPARAMETERFVPROC glTexParameterfv;
  PFNGLTEXPARAMETERIPROC glTexParameteri;
  PFNGLTEXPARAMETERIVPROC glTexParameteriv;
  PFNGLVIEWPORTPROC glViewport;
  PFNGLACTIVETEXTUREPROC glActiveTexture;    // aliases glActiveTextureARB
  PFNGLTEXSTORAGE1DPROC glTexStorage1D;    // aliases glTexStorage1DEXT
  PFNGLTEXSTORAGE2DPROC glTexStorage2D;    // aliases glTexStorage2DEXT
  PFNGLTEXSTORAGE3DPROC glTexStorage3D;    // aliases glTexStorage3DEXT
  PFNGLTEXSTORAGE2DMULTISAMPLEPROC glTexStorage2DMultisample;
  PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample;    // aliases glTexStorage3DMultisampleOES
  PFNGLTEXIMAGE3DPROC glTexImage3D;    // aliases glTexImage3DEXT, glTexImage3DOES
  PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;    // aliases glTexSubImage3DOES
  PFNGLTEXBUFFERPROC glTexBuffer;    // aliases glTexBufferARB, glTexBufferEXT, glTexBufferOES
  PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample;
  PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample;
  PFNGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;    // aliases glCompressedTexImage1DARB
  PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;    // aliases glCompressedTexImage2DARB
  PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;    // aliases glCompressedTexImage3DARB, glCompressedTexImage3DOES
  PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;    // aliases glCompressedTexSubImage1DARB
  PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;    // aliases glCompressedTexSubImage2DARB
  PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;    // aliases glCompressedTexSubImage3DARB, glCompressedTexSubImage3DOES
  PFNGLTEXBUFFERRANGEPROC glTexBufferRange;        // aliases glTexBufferRangeEXT, glTexBufferRangeOES
  PFNGLTEXTUREVIEWPROC glTextureView;              // aliases glTextureViewEXT, glTextureViewOES
  PFNGLTEXPARAMETERIIVPROC glTexParameterIiv;      // aliases glTexParameterIivEXT, glTexParameterIivOES
  PFNGLTEXPARAMETERIUIVPROC glTexParameterIuiv;    // aliases glTexParameterIuivEXT, glTexParameterIuivOES
  PFNGLGENERATEMIPMAPPROC glGenerateMipmap;        // aliases glGenerateMipmapEXT
  PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData;    // aliases glCopyImageSubDataEXT, glCopyImageSubDataOES
  PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;    // aliases glCopyTexSubImage3DOES
  PFNGLGETINTERNALFORMATIVPROC glGetInternalformativ;
  PFNGLGETINTERNALFORMATI64VPROC glGetInternalformati64v;
  PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;    // aliases glGetBufferParameterivARB
  PFNGLGETBUFFERPARAMETERI64VPROC glGetBufferParameteri64v;
  PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;    // aliases glGetBufferPointervARB, glGetBufferPointervOES
  PFNGLGETFRAGDATAINDEXPROC glGetFragDataIndex;
  PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation;    // aliases glGetFragDataLocationEXT
  PFNGLGETSTRINGIPROC glGetStringi;
  PFNGLGETBOOLEANI_VPROC glGetBooleani_v;
  PFNGLGETINTEGERI_VPROC glGetIntegeri_v;
  PFNGLGETFLOATI_VPROC glGetFloati_v;      // aliases glGetFloati_vEXT, glGetFloati_vOES, glGetFloati_vNV
  PFNGLGETDOUBLEI_VPROC glGetDoublei_v;    // aliases glGetDoublei_vEXT
  PFNGLGETINTEGER64I_VPROC glGetInteger64i_v;
  PFNGLGETINTEGER64VPROC glGetInteger64v;
  PFNGLGETSHADERIVPROC glGetShaderiv;
  PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
  PFNGLGETSHADERPRECISIONFORMATPROC glGetShaderPrecisionFormat;
  PFNGLGETSHADERSOURCEPROC glGetShaderSource;
  PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
  PFNGLGETPROGRAMIVPROC glGetProgramiv;
  PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
  PFNGLGETPROGRAMINTERFACEIVPROC glGetProgramInterfaceiv;
  PFNGLGETPROGRAMRESOURCEINDEXPROC glGetProgramResourceIndex;
  PFNGLGETPROGRAMRESOURCEIVPROC glGetProgramResourceiv;
  PFNGLGETPROGRAMRESOURCENAMEPROC glGetProgramResourceName;
  PFNGLGETPROGRAMPIPELINEIVPROC glGetProgramPipelineiv;              // aliases glGetProgramPipelineivEXT
  PFNGLGETPROGRAMPIPELINEINFOLOGPROC glGetProgramPipelineInfoLog;    // aliases glGetProgramPipelineInfoLogEXT
  PFNGLGETPROGRAMBINARYPROC glGetProgramBinary;
  PFNGLGETPROGRAMRESOURCELOCATIONPROC glGetProgramResourceLocation;
  PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC glGetProgramResourceLocationIndex;
  PFNGLGETPROGRAMSTAGEIVPROC glGetProgramStageiv;
  PFNGLGETGRAPHICSRESETSTATUSPROC glGetGraphicsResetStatus;    // aliases glGetGraphicsResetStatusARB, glGetGraphicsResetStatusEXT
  PFNGLGETOBJECTLABELPROC glGetObjectLabel;            // aliases glGetObjectLabelKHR
  PFNGLGETOBJECTLABELEXTPROC glGetObjectLabelEXT;
  PFNGLGETOBJECTPTRLABELPROC glGetObjectPtrLabel;      // aliases glGetObjectPtrLabelKHR
  PFNGLGETDEBUGMESSAGELOGPROC glGetDebugMessageLog;    // aliases glGetDebugMessageLogARB, glGetDebugMessageLogKHR
  PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;    // aliases glGetFramebufferAttachmentParameterivEXT
  PFNGLGETFRAMEBUFFERPARAMETERIVPROC glGetFramebufferParameteriv;
  PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;    // aliases glGetRenderbufferParameterivEXT
  PFNGLGETMULTISAMPLEFVPROC glGetMultisamplefv;
  PFNGLGETQUERYINDEXEDIVPROC glGetQueryIndexediv;
  PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;    // aliases glGetQueryObjectui64vEXT
  PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;        // aliases glGetQueryObjectuivARB, glGetQueryObjectuivEXT
  PFNGLGETQUERYOBJECTI64VPROC glGetQueryObjecti64v;      // aliases glGetQueryObjecti64vEXT
  PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;          // aliases glGetQueryObjectivARB, glGetQueryObjectivEXT
  PFNGLGETQUERYIVPROC glGetQueryiv;                      // aliases glGetQueryivARB, glGetQueryivEXT
  PFNGLGETSYNCIVPROC glGetSynciv;
  PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;    // aliases glGetBufferSubDataARB
  PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
  PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;
  PFNGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;    // aliases glGetCompressedTexImageARB
  PFNGLGETNCOMPRESSEDTEXIMAGEPROC glGetnCompressedTexImage;                              // aliases glGetnCompressedTexImageARB
  PFNGLGETNTEXIMAGEPROC glGetnTexImage;                  // aliases glGetnTexImageARB
  PFNGLGETTEXPARAMETERIIVPROC glGetTexParameterIiv;      // aliases glGetTexParameterIivEXT, glGetTexParameterIivOES
  PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv;    // aliases glGetTexParameterIuivEXT, glGetTexParameterIuivOES
  PFNGLCLAMPCOLORPROC glClampColor;                      // aliases glClampColorARB
  PFNGLREADNPIXELSPROC glReadnPixels;                    // aliases glReadnPixelsARB, glReadnPixelsEXT
  PFNGLGETSAMPLERPARAMETERIIVPROC glGetSamplerParameterIiv;      // aliases glGetSamplerParameterIivEXT, glGetSamplerParameterIivOES
  PFNGLGETSAMPLERPARAMETERIUIVPROC glGetSamplerParameterIuiv;    // aliases glGetSamplerParameterIuivEXT, glGetSamplerParameterIuivOES
  PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv;
  PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv;
  PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glGetTransformFeedbackVarying;    // aliases glGetTransformFeedbackVaryingEXT
  PFNGLGETSUBROUTINEINDEXPROC glGetSubroutineIndex;
  PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC glGetSubroutineUniformLocation;
  PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC glGetActiveAtomicCounterBufferiv;
  PFNGLGETACTIVESUBROUTINENAMEPROC glGetActiveSubroutineName;
  PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC glGetActiveSubroutineUniformName;
  PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC glGetActiveSubroutineUniformiv;
  PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
  PFNGLGETUNIFORMINDICESPROC glGetUniformIndices;
  PFNGLGETUNIFORMSUBROUTINEUIVPROC glGetUniformSubroutineuiv;
  PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
  PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
  PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
  PFNGLGETACTIVEUNIFORMNAMEPROC glGetActiveUniformName;
  PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName;
  PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv;
  PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv;
  PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
  PFNGLGETUNIFORMFVPROC glGetUniformfv;
  PFNGLGETUNIFORMIVPROC glGetUniformiv;
  PFNGLGETUNIFORMUIVPROC glGetUniformuiv;    // aliases glGetUniformuivEXT
  PFNGLGETUNIFORMDVPROC glGetUniformdv;
  PFNGLGETNUNIFORMDVPROC glGetnUniformdv;                // aliases glGetnUniformdvARB
  PFNGLGETNUNIFORMFVPROC glGetnUniformfv;                // aliases glGetnUniformfvARB, glGetnUniformfvEXT
  PFNGLGETNUNIFORMIVPROC glGetnUniformiv;                // aliases glGetnUniformivARB, glGetnUniformivEXT
  PFNGLGETNUNIFORMUIVPROC glGetnUniformuiv;              // aliases glGetnUniformuivARB
  PFNGLGETVERTEXATTRIBIIVPROC glGetVertexAttribIiv;      // aliases glGetVertexAttribIivEXT
  PFNGLGETVERTEXATTRIBIUIVPROC glGetVertexAttribIuiv;    // aliases glGetVertexAttribIuivEXT
  PFNGLGETVERTEXATTRIBLDVPROC glGetVertexAttribLdv;      // aliases glGetVertexAttribLdvEXT
  PFNGLGETVERTEXATTRIBDVPROC glGetVertexAttribdv;
  PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
  PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;                            // aliases glCheckFramebufferStatusEXT
  PFNGLBLENDCOLORPROC glBlendColor;                    // aliases glBlendColorEXT
  PFNGLBLENDFUNCIPROC glBlendFunci;                    // aliases glBlendFunciARB, glBlendFunciEXT, glBlendFunciOES
  PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;      // aliases glBlendFuncSeparateARB
  PFNGLBLENDFUNCSEPARATEIPROC glBlendFuncSeparatei;    // aliases glBlendFuncSeparateiARB, glBlendFuncSeparateiEXT, glBlendFuncSeparateiOES
  PFNGLBLENDEQUATIONPROC glBlendEquation;              // aliases glBlendEquationEXT, glBlendEquationARB
  PFNGLBLENDEQUATIONIPROC glBlendEquationi;            // aliases glBlendEquationiARB, glBlendEquationiEXT, glBlendEquationiOES
  PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;    // aliases glBlendEquationSeparateARB, glBlendEquationSeparateEXT
  PFNGLBLENDEQUATIONSEPARATEIPROC glBlendEquationSeparatei;    // aliases glBlendEquationSeparateiARB, glBlendEquationSeparateiEXT, glBlendEquationSeparateiOES
  PFNGLBLENDBARRIERKHRPROC glBlendBarrierKHR;
  PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
  PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
  PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
  PFNGLCOLORMASKIPROC glColorMaski;    // aliases glColorMaskiEXT, glColorMaskIndexedEXT, glColorMaskiOES
  PFNGLSAMPLEMASKIPROC glSampleMaski;
  PFNGLSAMPLECOVERAGEPROC glSampleCoverage;        // aliases glSampleCoverageARB
  PFNGLMINSAMPLESHADINGPROC glMinSampleShading;    // aliases glMinSampleShadingARB, glMinSampleShadingOES
  PFNGLDEPTHRANGEFPROC glDepthRangef;
  PFNGLDEPTHRANGEINDEXEDPROC glDepthRangeIndexed;
  PFNGLDEPTHRANGEARRAYVPROC glDepthRangeArrayv;
  PFNGLCLIPCONTROLPROC glClipControl;
  PFNGLPROVOKINGVERTEXPROC glProvokingVertex;    // aliases glProvokingVertexEXT
  PFNGLPRIMITIVERESTARTINDEXPROC glPrimitiveRestartIndex;
  PFNGLCREATESHADERPROC glCreateShader;
  PFNGLDELETESHADERPROC glDeleteShader;
  PFNGLSHADERSOURCEPROC glShaderSource;
  PFNGLCOMPILESHADERPROC glCompileShader;
  PFNGLCREATESHADERPROGRAMVPROC glCreateShaderProgramv;    // aliases glCreateShaderProgramvEXT
  PFNGLCREATEPROGRAMPROC glCreateProgram;
  PFNGLDELETEPROGRAMPROC glDeleteProgram;
  PFNGLATTACHSHADERPROC glAttachShader;
  PFNGLDETACHSHADERPROC glDetachShader;
  PFNGLRELEASESHADERCOMPILERPROC glReleaseShaderCompiler;
  PFNGLLINKPROGRAMPROC glLinkProgram;
  PFNGLPROGRAMPARAMETERIPROC glProgramParameteri;    // aliases glProgramParameteriARB, glProgramParameteriEXT
  PFNGLUSEPROGRAMPROC glUseProgram;
  PFNGLSHADERBINARYPROC glShaderBinary;
  PFNGLPROGRAMBINARYPROC glProgramBinary;
  PFNGLUSEPROGRAMSTAGESPROC glUseProgramStages;                 // aliases glUseProgramStagesEXT
  PFNGLVALIDATEPROGRAMPROC glValidateProgram;
  PFNGLGENPROGRAMPIPELINESPROC glGenProgramPipelines;           // aliases glGenProgramPipelinesEXT
  PFNGLBINDPROGRAMPIPELINEPROC glBindProgramPipeline;           // aliases glBindProgramPipelineEXT
  PFNGLACTIVESHADERPROGRAMPROC glActiveShaderProgram;           // aliases glActiveShaderProgramEXT
  PFNGLDELETEPROGRAMPIPELINESPROC glDeleteProgramPipelines;     // aliases glDeleteProgramPipelinesEXT
  PFNGLVALIDATEPROGRAMPIPELINEPROC glValidateProgramPipeline;   // aliases glValidateProgramPipelineEXT
  PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;    // aliases glDebugMessageCallbackARB, glDebugMessageCallbackKHR
  PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;      // aliases glDebugMessageControlARB, glDebugMessageControlKHR
  PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert;        // aliases glDebugMessageInsertARB, glDebugMessageInsertKHR
  PFNGLPUSHDEBUGGROUPPROC glPushDebugGroup;                // aliases glPushDebugGroupKHR
  PFNGLPOPDEBUGGROUPPROC glPopDebugGroup;                  // aliases glPopDebugGroupKHR
  PFNGLOBJECTLABELPROC glObjectLabel;                      // aliases glObjectLabelKHR
  PFNGLLABELOBJECTEXTPROC glLabelObjectEXT;
  PFNGLOBJECTPTRLABELPROC glObjectPtrLabel;                // aliases glObjectPtrLabelKHR
  PFNGLENABLEIPROC glEnablei;                // aliases glEnableiEXT, glEnableIndexedEXT, glEnableiOES, glEnableiNV
  PFNGLDISABLEIPROC glDisablei;              // aliases glDisableiEXT, glDisableIndexedEXT, glDisableiOES, glDisableiNV
  PFNGLISENABLEDIPROC glIsEnabledi;          // aliases glIsEnablediEXT, glIsEnabledIndexedEXT, glIsEnablediOES, glIsEnablediNV
  PFNGLISBUFFERPROC glIsBuffer;              // aliases glIsBufferARB
  PFNGLISFRAMEBUFFERPROC glIsFramebuffer;    // aliases glIsFramebufferEXT
  PFNGLISPROGRAMPROC glIsProgram;
  PFNGLISPROGRAMPIPELINEPROC glIsProgramPipeline;    // aliases glIsProgramPipelineEXT
  PFNGLISQUERYPROC glIsQuery;                  // aliases glIsQueryARB, glIsQueryEXT
  PFNGLISRENDERBUFFERPROC glIsRenderbuffer;    // aliases glIsRenderbufferEXT
  PFNGLISSAMPLERPROC glIsSampler;
  PFNGLISSHADERPROC glIsShader;
  PFNGLISSYNCPROC glIsSync;
  PFNGLISTRANSFORMFEEDBACKPROC glIsTransformFeedback;
  PFNGLISVERTEXARRAYPROC glIsVertexArray;                  // aliases glIsVertexArrayOES
  PFNGLGENBUFFERSPROC glGenBuffers;                        // aliases glGenBuffersARB
  PFNGLBINDBUFFERPROC glBindBuffer;                        // aliases glBindBufferARB
  PFNGLDRAWBUFFERSPROC glDrawBuffers;                      // aliases glDrawBuffersARB, glDrawBuffersEXT
  PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;              // aliases glGenFramebuffersEXT
  PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;              // aliases glBindFramebufferEXT
  PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture;        // aliases glFramebufferTextureARB, glFramebufferTextureOES, glFramebufferTextureEXT
  PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D;    // aliases glFramebufferTexture1DEXT
  PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;    // aliases glFramebufferTexture2DEXT
  PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D;    // aliases glFramebufferTexture3DEXT, glFramebufferTexture3DOES
  PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;    // aliases glFramebufferRenderbufferEXT
  PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;    // aliases glFramebufferTextureLayerARB, glFramebufferTextureLayerEXT
  PFNGLFRAMEBUFFERPARAMETERIPROC glFramebufferParameteri;
  PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;      // aliases glDeleteFramebuffersEXT
  PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;          // aliases glGenRenderbuffersEXT
  PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;    // aliases glRenderbufferStorageEXT
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
  PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;    // aliases glDeleteRenderbuffersEXT
  PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;          // aliases glBindRenderbufferEXT
  PFNGLFENCESYNCPROC glFenceSync;
  PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
  PFNGLWAITSYNCPROC glWaitSync;
  PFNGLDELETESYNCPROC glDeleteSync;
  PFNGLGENQUERIESPROC glGenQueries;    // aliases glGenQueriesARB, glGenQueriesEXT
  PFNGLBEGINQUERYPROC glBeginQuery;    // aliases glBeginQueryARB, glBeginQueryEXT
  PFNGLBEGINQUERYINDEXEDPROC glBeginQueryIndexed;
  PFNGLENDQUERYPROC glEndQuery;    // aliases glEndQueryARB, glEndQueryEXT
  PFNGLENDQUERYINDEXEDPROC glEndQueryIndexed;
  PFNGLBEGINCONDITIONALRENDERPROC glBeginConditionalRender;
  PFNGLENDCONDITIONALRENDERPROC glEndConditionalRender;
  PFNGLQUERYCOUNTERPROC glQueryCounter;      // aliases glQueryCounterEXT
  PFNGLDELETEQUERIESPROC glDeleteQueries;    // aliases glDeleteQueriesARB, glDeleteQueriesEXT
  PFNGLBUFFERDATAPROC glBufferData;          // aliases glBufferDataARB
  PFNGLBUFFERSTORAGEPROC glBufferStorage;    // aliases glBufferStorageEXT
  PFNGLBUFFERSUBDATAPROC glBufferSubData;    // aliases glBufferSubDataARB
  PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData;
  PFNGLBINDBUFFERBASEPROC glBindBufferBase;      // aliases glBindBufferBaseEXT
  PFNGLBINDBUFFERRANGEPROC glBindBufferRange;    // aliases glBindBufferRangeEXT
  PFNGLBINDBUFFERSBASEPROC glBindBuffersBase;
  PFNGLBINDBUFFERSRANGEPROC glBindBuffersRange;
  PFNGLMAPBUFFERPROC glMapBuffer;    // aliases glMapBufferARB, glMapBufferOES
  PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
  PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange;
  PFNGLUNMAPBUFFERPROC glUnmapBuffer;    // aliases glUnmapBufferARB, glUnmapBufferOES
  PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings;    // aliases glTransformFeedbackVaryingsEXT
  PFNGLGENTRANSFORMFEEDBACKSPROC glGenTransformFeedbacks;
  PFNGLDELETETRANSFORMFEEDBACKSPROC glDeleteTransformFeedbacks;
  PFNGLBINDTRANSFORMFEEDBACKPROC glBindTransformFeedback;
  PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback;    // aliases glBeginTransformFeedbackEXT
  PFNGLPAUSETRANSFORMFEEDBACKPROC glPauseTransformFeedback;
  PFNGLRESUMETRANSFORMFEEDBACKPROC glResumeTransformFeedback;
  PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback;    // aliases glEndTransformFeedbackEXT
  PFNGLDRAWTRANSFORMFEEDBACKPROC glDrawTransformFeedback;
  PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC glDrawTransformFeedbackInstanced;
  PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC glDrawTransformFeedbackStream;
  PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC glDrawTransformFeedbackStreamInstanced;
  PFNGLDELETEBUFFERSPROC glDeleteBuffers;    // aliases glDeleteBuffersARB
  PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;           // aliases glGenVertexArraysOES
  PFNGLBINDVERTEXARRAYPROC glBindVertexArray;           // aliases glBindVertexArrayOES
  PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;     // aliases glDeleteVertexArraysOES
  PFNGLVERTEXATTRIB1DPROC glVertexAttrib1d;        // aliases glVertexAttrib1dARB
  PFNGLVERTEXATTRIB1DVPROC glVertexAttrib1dv;      // aliases glVertexAttrib1dvARB
  PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;        // aliases glVertexAttrib1fARB
  PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;      // aliases glVertexAttrib1fvARB
  PFNGLVERTEXATTRIB1SPROC glVertexAttrib1s;        // aliases glVertexAttrib1sARB
  PFNGLVERTEXATTRIB1SVPROC glVertexAttrib1sv;      // aliases glVertexAttrib1svARB
  PFNGLVERTEXATTRIB2DPROC glVertexAttrib2d;        // aliases glVertexAttrib2dARB
  PFNGLVERTEXATTRIB2DVPROC glVertexAttrib2dv;      // aliases glVertexAttrib2dvARB
  PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;        // aliases glVertexAttrib2fARB
  PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;      // aliases glVertexAttrib2fvARB
  PFNGLVERTEXATTRIB2SPROC glVertexAttrib2s;        // aliases glVertexAttrib2sARB
  PFNGLVERTEXATTRIB2SVPROC glVertexAttrib2sv;      // aliases glVertexAttrib2svARB
  PFNGLVERTEXATTRIB3DPROC glVertexAttrib3d;        // aliases glVertexAttrib3dARB
  PFNGLVERTEXATTRIB3DVPROC glVertexAttrib3dv;      // aliases glVertexAttrib3dvARB
  PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;        // aliases glVertexAttrib3fARB
  PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;      // aliases glVertexAttrib3fvARB
  PFNGLVERTEXATTRIB3SPROC glVertexAttrib3s;        // aliases glVertexAttrib3sARB
  PFNGLVERTEXATTRIB3SVPROC glVertexAttrib3sv;      // aliases glVertexAttrib3svARB
  PFNGLVERTEXATTRIB4NBVPROC glVertexAttrib4Nbv;    // aliases glVertexAttrib4NbvARB
  PFNGLVERTEXATTRIB4NIVPROC glVertexAttrib4Niv;    // aliases glVertexAttrib4NivARB
  PFNGLVERTEXATTRIB4NSVPROC glVertexAttrib4Nsv;    // aliases glVertexAttrib4NsvARB
  PFNGLVERTEXATTRIB4NUBPROC glVertexAttrib4Nub;
  PFNGLVERTEXATTRIB4NUBVPROC glVertexAttrib4Nubv;    // aliases glVertexAttrib4NubvARB
  PFNGLVERTEXATTRIB4NUIVPROC glVertexAttrib4Nuiv;    // aliases glVertexAttrib4NuivARB
  PFNGLVERTEXATTRIB4NUSVPROC glVertexAttrib4Nusv;    // aliases glVertexAttrib4NusvARB
  PFNGLVERTEXATTRIB4BVPROC glVertexAttrib4bv;        // aliases glVertexAttrib4bvARB
  PFNGLVERTEXATTRIB4DPROC glVertexAttrib4d;          // aliases glVertexAttrib4dARB
  PFNGLVERTEXATTRIB4DVPROC glVertexAttrib4dv;        // aliases glVertexAttrib4dvARB
  PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;          // aliases glVertexAttrib4fARB
  PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;        // aliases glVertexAttrib4fvARB
  PFNGLVERTEXATTRIB4IVPROC glVertexAttrib4iv;        // aliases glVertexAttrib4ivARB
  PFNGLVERTEXATTRIB4SPROC glVertexAttrib4s;          // aliases glVertexAttrib4sARB
  PFNGLVERTEXATTRIB4SVPROC glVertexAttrib4sv;        // aliases glVertexAttrib4svARB
  PFNGLVERTEXATTRIB4UBVPROC glVertexAttrib4ubv;      // aliases glVertexAttrib4ubvARB
  PFNGLVERTEXATTRIB4UIVPROC glVertexAttrib4uiv;      // aliases glVertexAttrib4uivARB
  PFNGLVERTEXATTRIB4USVPROC glVertexAttrib4usv;      // aliases glVertexAttrib4usvARB
  PFNGLVERTEXATTRIBI1IPROC glVertexAttribI1i;        // aliases glVertexAttribI1iEXT
  PFNGLVERTEXATTRIBI1IVPROC glVertexAttribI1iv;      // aliases glVertexAttribI1ivEXT
  PFNGLVERTEXATTRIBI1UIPROC glVertexAttribI1ui;      // aliases glVertexAttribI1uiEXT
  PFNGLVERTEXATTRIBI1UIVPROC glVertexAttribI1uiv;    // aliases glVertexAttribI1uivEXT
  PFNGLVERTEXATTRIBI2IPROC glVertexAttribI2i;        // aliases glVertexAttribI2iEXT
  PFNGLVERTEXATTRIBI2IVPROC glVertexAttribI2iv;      // aliases glVertexAttribI2ivEXT
  PFNGLVERTEXATTRIBI2UIPROC glVertexAttribI2ui;      // aliases glVertexAttribI2uiEXT
  PFNGLVERTEXATTRIBI2UIVPROC glVertexAttribI2uiv;    // aliases glVertexAttribI2uivEXT
  PFNGLVERTEXATTRIBI3IPROC glVertexAttribI3i;        // aliases glVertexAttribI3iEXT
  PFNGLVERTEXATTRIBI3IVPROC glVertexAttribI3iv;      // aliases glVertexAttribI3ivEXT
  PFNGLVERTEXATTRIBI3UIPROC glVertexAttribI3ui;      // aliases glVertexAttribI3uiEXT
  PFNGLVERTEXATTRIBI3UIVPROC glVertexAttribI3uiv;    // aliases glVertexAttribI3uivEXT
  PFNGLVERTEXATTRIBI4BVPROC glVertexAttribI4bv;      // aliases glVertexAttribI4bvEXT
  PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i;        // aliases glVertexAttribI4iEXT
  PFNGLVERTEXATTRIBI4IVPROC glVertexAttribI4iv;      // aliases glVertexAttribI4ivEXT
  PFNGLVERTEXATTRIBI4SVPROC glVertexAttribI4sv;      // aliases glVertexAttribI4svEXT
  PFNGLVERTEXATTRIBI4UBVPROC glVertexAttribI4ubv;    // aliases glVertexAttribI4ubvEXT
  PFNGLVERTEXATTRIBI4UIPROC glVertexAttribI4ui;      // aliases glVertexAttribI4uiEXT
  PFNGLVERTEXATTRIBI4UIVPROC glVertexAttribI4uiv;    // aliases glVertexAttribI4uivEXT
  PFNGLVERTEXATTRIBI4USVPROC glVertexAttribI4usv;    // aliases glVertexAttribI4usvEXT
  PFNGLVERTEXATTRIBL1DPROC glVertexAttribL1d;        // aliases glVertexAttribL1dEXT
  PFNGLVERTEXATTRIBL1DVPROC glVertexAttribL1dv;      // aliases glVertexAttribL1dvEXT
  PFNGLVERTEXATTRIBL2DPROC glVertexAttribL2d;        // aliases glVertexAttribL2dEXT
  PFNGLVERTEXATTRIBL2DVPROC glVertexAttribL2dv;      // aliases glVertexAttribL2dvEXT
  PFNGLVERTEXATTRIBL3DPROC glVertexAttribL3d;        // aliases glVertexAttribL3dEXT
  PFNGLVERTEXATTRIBL3DVPROC glVertexAttribL3dv;      // aliases glVertexAttribL3dvEXT
  PFNGLVERTEXATTRIBL4DPROC glVertexAttribL4d;        // aliases glVertexAttribL4dEXT
  PFNGLVERTEXATTRIBL4DVPROC glVertexAttribL4dv;      // aliases glVertexAttribL4dvEXT
  PFNGLVERTEXATTRIBP1UIPROC glVertexAttribP1ui;
  PFNGLVERTEXATTRIBP1UIVPROC glVertexAttribP1uiv;
  PFNGLVERTEXATTRIBP2UIPROC glVertexAttribP2ui;
  PFNGLVERTEXATTRIBP2UIVPROC glVertexAttribP2uiv;
  PFNGLVERTEXATTRIBP3UIPROC glVertexAttribP3ui;
  PFNGLVERTEXATTRIBP3UIVPROC glVertexAttribP3uiv;
  PFNGLVERTEXATTRIBP4UIPROC glVertexAttribP4ui;
  PFNGLVERTEXATTRIBP4UIVPROC glVertexAttribP4uiv;
  PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;      // aliases glVertexAttribPointerARB
  PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;    // aliases glVertexAttribIPointerEXT
  PFNGLVERTEXATTRIBLPOINTERPROC glVertexAttribLPointer;    // aliases glVertexAttribLPointerEXT
  PFNGLVERTEXATTRIBBINDINGPROC glVertexAttribBinding;
  PFNGLVERTEXATTRIBFORMATPROC glVertexAttribFormat;
  PFNGLVERTEXATTRIBIFORMATPROC glVertexAttribIFormat;
  PFNGLVERTEXATTRIBLFORMATPROC glVertexAttribLFormat;
  PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;    // aliases glVertexAttribDivisorARB
  PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
  PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation;    // aliases glBindFragDataLocationEXT
  PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed;
  PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;    // aliases glEnableVertexAttribArrayARB
  PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;    // aliases glDisableVertexAttribArrayARB
  PFNGLBINDVERTEXBUFFERPROC glBindVertexBuffer;
  PFNGLBINDVERTEXBUFFERSPROC glBindVertexBuffers;
  PFNGLVERTEXBINDINGDIVISORPROC glVertexBindingDivisor;
  PFNGLBINDIMAGETEXTUREPROC glBindImageTexture;    // aliases glBindImageTextureEXT
  PFNGLBINDIMAGETEXTURESPROC glBindImageTextures;
  PFNGLGENSAMPLERSPROC glGenSamplers;
  PFNGLBINDSAMPLERPROC glBindSampler;
  PFNGLBINDSAMPLERSPROC glBindSamplers;
  PFNGLBINDTEXTURESPROC glBindTextures;
  PFNGLDELETESAMPLERSPROC glDeleteSamplers;
  PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
  PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
  PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv;
  PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
  PFNGLSAMPLERPARAMETERIIVPROC glSamplerParameterIiv;      // aliases glSamplerParameterIivEXT, glSamplerParameterIivOES
  PFNGLSAMPLERPARAMETERIUIVPROC glSamplerParameterIuiv;    // aliases glSamplerParameterIuivEXT, glSamplerParameterIuivOES
  PFNGLPATCHPARAMETERIPROC glPatchParameteri;    // aliases glPatchParameteriEXT, glPatchParameteriOES
  PFNGLPATCHPARAMETERFVPROC glPatchParameterfv;
  PFNGLPOINTPARAMETERFPROC glPointParameterf;    // aliases glPointParameterfARB, glPointParameterfEXT
  PFNGLPOINTPARAMETERFVPROC glPointParameterfv;    // aliases glPointParameterfvARB, glPointParameterfvEXT
  PFNGLPOINTPARAMETERIPROC glPointParameteri;
  PFNGLPOINTPARAMETERIVPROC glPointParameteriv;
  PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
  PFNGLDISPATCHCOMPUTEINDIRECTPROC glDispatchComputeIndirect;
  PFNGLMEMORYBARRIERPROC glMemoryBarrier;    // aliases glMemoryBarrierEXT
  PFNGLMEMORYBARRIERBYREGIONPROC glMemoryBarrierByRegion;
  PFNGLTEXTUREBARRIERPROC glTextureBarrier;
  PFNGLCLEARDEPTHFPROC glClearDepthf;
  PFNGLCLEARBUFFERFVPROC glClearBufferfv;
  PFNGLCLEARBUFFERIVPROC glClearBufferiv;
  PFNGLCLEARBUFFERUIVPROC glClearBufferuiv;
  PFNGLCLEARBUFFERFIPROC glClearBufferfi;
  PFNGLCLEARBUFFERDATAPROC glClearBufferData;
  PFNGLCLEARBUFFERSUBDATAPROC glClearBufferSubData;
  PFNGLCLEARTEXIMAGEPROC glClearTexImage;
  PFNGLCLEARTEXSUBIMAGEPROC glClearTexSubImage;
  PFNGLINVALIDATEBUFFERDATAPROC glInvalidateBufferData;
  PFNGLINVALIDATEBUFFERSUBDATAPROC glInvalidateBufferSubData;
  PFNGLINVALIDATEFRAMEBUFFERPROC glInvalidateFramebuffer;
  PFNGLINVALIDATESUBFRAMEBUFFERPROC glInvalidateSubFramebuffer;
  PFNGLINVALIDATETEXIMAGEPROC glInvalidateTexImage;
  PFNGLINVALIDATETEXSUBIMAGEPROC glInvalidateTexSubImage;
  PFNGLSCISSORARRAYVPROC glScissorArrayv;             // aliases glScissorArrayvOES, glScissorArrayvNV
  PFNGLSCISSORINDEXEDPROC glScissorIndexed;           // aliases glScissorIndexedOES, glScissorIndexedNV
  PFNGLSCISSORINDEXEDVPROC glScissorIndexedv;         // aliases glScissorIndexedvOES, glScissorIndexedvNV
  PFNGLVIEWPORTINDEXEDFPROC glViewportIndexedf;       // aliases glViewportIndexedfOES, glViewportIndexedfNV
  PFNGLVIEWPORTINDEXEDFVPROC glViewportIndexedfv;     // aliases glViewportIndexedfvOES, glViewportIndexedfvNV
  PFNGLVIEWPORTARRAYVPROC glViewportArrayv;           // aliases glViewportArrayvOES, glViewportArrayvNV
  PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
  PFNGLSHADERSTORAGEBLOCKBINDINGPROC glShaderStorageBlockBinding;
  PFNGLUNIFORMSUBROUTINESUIVPROC glUniformSubroutinesuiv;
  PFNGLUNIFORM1FPROC glUniform1f;      // aliases glUniform1fARB
  PFNGLUNIFORM1IPROC glUniform1i;      // aliases glUniform1iARB
  PFNGLUNIFORM1UIPROC glUniform1ui;    // aliases glUniform1uiEXT
  PFNGLUNIFORM1DPROC glUniform1d;
  PFNGLUNIFORM2FPROC glUniform2f;      // aliases glUniform2fARB
  PFNGLUNIFORM2IPROC glUniform2i;      // aliases glUniform2iARB
  PFNGLUNIFORM2UIPROC glUniform2ui;    // aliases glUniform2uiEXT
  PFNGLUNIFORM2DPROC glUniform2d;
  PFNGLUNIFORM3FPROC glUniform3f;      // aliases glUniform3fARB
  PFNGLUNIFORM3IPROC glUniform3i;      // aliases glUniform3iARB
  PFNGLUNIFORM3UIPROC glUniform3ui;    // aliases glUniform3uiEXT
  PFNGLUNIFORM3DPROC glUniform3d;
  PFNGLUNIFORM4FPROC glUniform4f;      // aliases glUniform4fARB
  PFNGLUNIFORM4IPROC glUniform4i;      // aliases glUniform4iARB
  PFNGLUNIFORM4UIPROC glUniform4ui;    // aliases glUniform4uiEXT
  PFNGLUNIFORM4DPROC glUniform4d;
  PFNGLUNIFORM1FVPROC glUniform1fv;      // aliases glUniform1fvARB
  PFNGLUNIFORM1IVPROC glUniform1iv;      // aliases glUniform1ivARB
  PFNGLUNIFORM1UIVPROC glUniform1uiv;    // aliases glUniform1uivEXT
  PFNGLUNIFORM1DVPROC glUniform1dv;
  PFNGLUNIFORM2FVPROC glUniform2fv;      // aliases glUniform2fvARB
  PFNGLUNIFORM2IVPROC glUniform2iv;      // aliases glUniform2ivARB
  PFNGLUNIFORM2UIVPROC glUniform2uiv;    // aliases glUniform2uivEXT
  PFNGLUNIFORM2DVPROC glUniform2dv;
  PFNGLUNIFORM3FVPROC glUniform3fv;      // aliases glUniform3fvARB
  PFNGLUNIFORM3IVPROC glUniform3iv;      // aliases glUniform3ivARB
  PFNGLUNIFORM3UIVPROC glUniform3uiv;    // aliases glUniform3uivEXT
  PFNGLUNIFORM3DVPROC glUniform3dv;
  PFNGLUNIFORM4FVPROC glUniform4fv;      // aliases glUniform4fvARB
  PFNGLUNIFORM4IVPROC glUniform4iv;      // aliases glUniform4ivARB
  PFNGLUNIFORM4UIVPROC glUniform4uiv;    // aliases glUniform4uivEXT
  PFNGLUNIFORM4DVPROC glUniform4dv;
  PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;      // aliases glUniformMatrix2fvARB
  PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
  PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
  PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;      // aliases glUniformMatrix3fvARB
  PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
  PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
  PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;      // aliases glUniformMatrix4fvARB
  PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
  PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;
  PFNGLUNIFORMMATRIX2DVPROC glUniformMatrix2dv;
  PFNGLUNIFORMMATRIX2X3DVPROC glUniformMatrix2x3dv;
  PFNGLUNIFORMMATRIX2X4DVPROC glUniformMatrix2x4dv;
  PFNGLUNIFORMMATRIX3DVPROC glUniformMatrix3dv;
  PFNGLUNIFORMMATRIX3X2DVPROC glUniformMatrix3x2dv;
  PFNGLUNIFORMMATRIX3X4DVPROC glUniformMatrix3x4dv;
  PFNGLUNIFORMMATRIX4DVPROC glUniformMatrix4dv;
  PFNGLUNIFORMMATRIX4X2DVPROC glUniformMatrix4x2dv;
  PFNGLUNIFORMMATRIX4X3DVPROC glUniformMatrix4x3dv;
  PFNGLPROGRAMUNIFORM1FPROC glProgramUniform1f;        // aliases glProgramUniform1fEXT
  PFNGLPROGRAMUNIFORM1IPROC glProgramUniform1i;        // aliases glProgramUniform1iEXT
  PFNGLPROGRAMUNIFORM1UIPROC glProgramUniform1ui;      // aliases glProgramUniform1uiEXT
  PFNGLPROGRAMUNIFORM1DPROC glProgramUniform1d;        // aliases glProgramUniform1dEXT
  PFNGLPROGRAMUNIFORM2FPROC glProgramUniform2f;        // aliases glProgramUniform2fEXT
  PFNGLPROGRAMUNIFORM2IPROC glProgramUniform2i;        // aliases glProgramUniform2iEXT
  PFNGLPROGRAMUNIFORM2UIPROC glProgramUniform2ui;      // aliases glProgramUniform2uiEXT
  PFNGLPROGRAMUNIFORM2DPROC glProgramUniform2d;        // aliases glProgramUniform2dEXT
  PFNGLPROGRAMUNIFORM3FPROC glProgramUniform3f;        // aliases glProgramUniform3fEXT
  PFNGLPROGRAMUNIFORM3IPROC glProgramUniform3i;        // aliases glProgramUniform3iEXT
  PFNGLPROGRAMUNIFORM3UIPROC glProgramUniform3ui;      // aliases glProgramUniform3uiEXT
  PFNGLPROGRAMUNIFORM3DPROC glProgramUniform3d;        // aliases glProgramUniform3dEXT
  PFNGLPROGRAMUNIFORM4FPROC glProgramUniform4f;        // aliases glProgramUniform4fEXT
  PFNGLPROGRAMUNIFORM4IPROC glProgramUniform4i;        // aliases glProgramUniform4iEXT
  PFNGLPROGRAMUNIFORM4UIPROC glProgramUniform4ui;      // aliases glProgramUniform4uiEXT
  PFNGLPROGRAMUNIFORM4DPROC glProgramUniform4d;        // aliases glProgramUniform4dEXT
  PFNGLPROGRAMUNIFORM1FVPROC glProgramUniform1fv;      // aliases glProgramUniform1fvEXT
  PFNGLPROGRAMUNIFORM1IVPROC glProgramUniform1iv;      // aliases glProgramUniform1ivEXT
  PFNGLPROGRAMUNIFORM1UIVPROC glProgramUniform1uiv;    // aliases glProgramUniform1uivEXT
  PFNGLPROGRAMUNIFORM1DVPROC glProgramUniform1dv;      // aliases glProgramUniform1dvEXT
  PFNGLPROGRAMUNIFORM2FVPROC glProgramUniform2fv;      // aliases glProgramUniform2fvEXT
  PFNGLPROGRAMUNIFORM2IVPROC glProgramUniform2iv;      // aliases glProgramUniform2ivEXT
  PFNGLPROGRAMUNIFORM2UIVPROC glProgramUniform2uiv;    // aliases glProgramUniform2uivEXT
  PFNGLPROGRAMUNIFORM2DVPROC glProgramUniform2dv;      // aliases glProgramUniform2dvEXT
  PFNGLPROGRAMUNIFORM3FVPROC glProgramUniform3fv;      // aliases glProgramUniform3fvEXT
  PFNGLPROGRAMUNIFORM3IVPROC glProgramUniform3iv;      // aliases glProgramUniform3ivEXT
  PFNGLPROGRAMUNIFORM3UIVPROC glProgramUniform3uiv;    // aliases glProgramUniform3uivEXT
  PFNGLPROGRAMUNIFORM3DVPROC glProgramUniform3dv;      // aliases glProgramUniform3dvEXT
  PFNGLPROGRAMUNIFORM4FVPROC glProgramUniform4fv;      // aliases glProgramUniform4fvEXT
  PFNGLPROGRAMUNIFORM4IVPROC glProgramUniform4iv;      // aliases glProgramUniform4ivEXT
  PFNGLPROGRAMUNIFORM4UIVPROC glProgramUniform4uiv;    // aliases glProgramUniform4uivEXT
  PFNGLPROGRAMUNIFORM4DVPROC glProgramUniform4dv;      // aliases glProgramUniform4dvEXT
  PFNGLPROGRAMUNIFORMMATRIX2FVPROC glProgramUniformMatrix2fv;    // aliases glProgramUniformMatrix2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glProgramUniformMatrix2x3fv;    // aliases glProgramUniformMatrix2x3fvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glProgramUniformMatrix2x4fv;    // aliases glProgramUniformMatrix2x4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3FVPROC glProgramUniformMatrix3fv;    // aliases glProgramUniformMatrix3fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glProgramUniformMatrix3x2fv;    // aliases glProgramUniformMatrix3x2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glProgramUniformMatrix3x4fv;    // aliases glProgramUniformMatrix3x4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4FVPROC glProgramUniformMatrix4fv;    // aliases glProgramUniformMatrix4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glProgramUniformMatrix4x2fv;    // aliases glProgramUniformMatrix4x2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glProgramUniformMatrix4x3fv;    // aliases glProgramUniformMatrix4x3fvEXT
  PFNGLPROGRAMUNIFORMMATRIX2DVPROC glProgramUniformMatrix2dv;    // aliases glProgramUniformMatrix2dvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC glProgramUniformMatrix2x3dv;    // aliases glProgramUniformMatrix2x3dvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC glProgramUniformMatrix2x4dv;    // aliases glProgramUniformMatrix2x4dvEXT
  PFNGLPROGRAMUNIFORMMATRIX3DVPROC glProgramUniformMatrix3dv;    // aliases glProgramUniformMatrix3dvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC glProgramUniformMatrix3x2dv;    // aliases glProgramUniformMatrix3x2dvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC glProgramUniformMatrix3x4dv;    // aliases glProgramUniformMatrix3x4dvEXT
  PFNGLPROGRAMUNIFORMMATRIX4DVPROC glProgramUniformMatrix4dv;    // aliases glProgramUniformMatrix4dvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC glProgramUniformMatrix4x2dv;    // aliases glProgramUniformMatrix4x2dvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC glProgramUniformMatrix4x3dv;                       // aliases glProgramUniformMatrix4x3dvEXT
  PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;    // aliases glDrawRangeElementsEXT
  PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glDrawRangeElementsBaseVertex;   // aliases glDrawRangeElementsBaseVertexEXT, glDrawRangeElementsBaseVertexOES
  PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC glDrawArraysInstancedBaseInstance;    // aliases glDrawArraysInstancedBaseInstanceEXT
  PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;    // aliases glDrawArraysInstancedARB, glDrawArraysInstancedEXT
  PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;    // aliases glDrawElementsInstancedARB, glDrawElementsInstancedEXT
  PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC glDrawElementsInstancedBaseInstance;    // aliases glDrawElementsInstancedBaseInstanceEXT
  PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex;     // aliases glDrawElementsBaseVertexEXT, glDrawElementsBaseVertexOES
  PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex;   // aliases glDrawElementsInstancedBaseVertexEXT, glDrawElementsInstancedBaseVertexOES
  PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glDrawElementsInstancedBaseVertexBaseInstance;    // aliases glDrawElementsInstancedBaseVertexBaseInstanceEXT
  PFNGLMULTIDRAWARRAYSPROC glMultiDrawArrays;    // aliases glMultiDrawArraysEXT
  PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements;
  PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glMultiDrawElementsBaseVertex;   // aliases glMultiDrawElementsBaseVertexEXT, glMultiDrawElementsBaseVertexOES
  PFNGLMULTIDRAWARRAYSINDIRECTPROC glMultiDrawArraysIndirect;
  PFNGLMULTIDRAWELEMENTSINDIRECTPROC glMultiDrawElementsIndirect;
  PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect;
  PFNGLDRAWELEMENTSINDIRECTPROC glDrawElementsIndirect;
  PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;    // aliases glBlitFramebufferEXT

  // GLES core
  PFNGLPRIMITIVEBOUNDINGBOXPROC glPrimitiveBoundingBox;    // aliases glPrimitiveBoundingBoxARB, glPrimitiveBoundingBoxEXT, glPrimitiveBoundingBoxOES
  PFNGLBLENDBARRIERPROC glBlendBarrier;

  // GLES: EXT_multisampled_render_to_texture
  PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
  // this function should be an alias of glRenderBufferStorage2DMultisample, except there are driver
  // issues that prevent the functions from being treated interchangeably.
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;

  // GLES: EXT_discard_framebuffer
  PFNGLDISCARDFRAMEBUFFEREXTPROC glDiscardFramebufferEXT;

  // GLES: OES_viewport_array, NV_viewport_array
  // only 2 functions which have different parameter types, so they can't be aliases of the ARB functions
  PFNGLDEPTHRANGEARRAYFVOESPROC glDepthRangeArrayfvOES;      // aliases glDepthRangeArrayfvNV
  PFNGLDEPTHRANGEINDEXEDFOESPROC glDepthRangeIndexedfOES;    // aliases glDepthRangeIndexedfNV

  // ARB_shading_language_include
  PFNGLNAMEDSTRINGARBPROC glNamedStringARB;
  PFNGLDELETENAMEDSTRINGARBPROC glDeleteNamedStringARB;
  PFNGLCOMPILESHADERINCLUDEARBPROC glCompileShaderIncludeARB;
  PFNGLISNAMEDSTRINGARBPROC glIsNamedStringARB;
  PFNGLGETNAMEDSTRINGARBPROC glGetNamedStringARB;
  PFNGLGETNAMEDSTRINGIVARBPROC glGetNamedStringivARB;

  // ARB_compute_variable_group_size
  PFNGLDISPATCHCOMPUTEGROUPSIZEARBPROC glDispatchComputeGroupSizeARB;

  // ARB_indirect_parameters
  PFNGLMULTIDRAWARRAYSINDIRECTCOUNTPROC glMultiDrawArraysIndirectCount; // aliases glMultiDrawArraysIndirectCountARB
  PFNGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC glMultiDrawElementsIndirectCount; // aliases glMultiDrawElementsIndirectCountARB

  // EXT_raster_multisample
  PFNGLRASTERSAMPLESEXTPROC glRasterSamplesEXT;

  // EXT_depth_bounds_test
  PFNGLDEPTHBOUNDSEXTPROC glDepthBoundsEXT;

  // EXT/ARB_polygon_offset_clamp
  PFNGLPOLYGONOFFSETCLAMPPROC glPolygonOffsetClamp; // aliases glPolygonOffsetClampEXT

  // EXT_debug_marker
  PFNGLINSERTEVENTMARKEREXTPROC glInsertEventMarkerEXT;
  PFNGLPUSHGROUPMARKEREXTPROC glPushGroupMarkerEXT;
  PFNGLPOPGROUPMARKEREXTPROC glPopGroupMarkerEXT;

  // GREMEDY_frame_terminator
  PFNGLFRAMETERMINATORGREMEDYPROC glFrameTerminatorGREMEDY;

  // GREMEDY_string_marker
  PFNGLSTRINGMARKERGREMEDYPROC glStringMarkerGREMEDY;

  // OVR_multiview
  PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;

  // OVR_multiview_multisampled_render_to_texture
  PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;

  // QCOM_texture_foveated
  PFNGLTEXTUREFOVEATIONPARAMETERSQCOMPROC glTextureFoveationParametersQCOM;

  // ARB_parallel_shader_compile
  PFNGLMAXSHADERCOMPILERTHREADSKHRPROC glMaxShaderCompilerThreadsKHR; // aliases glMaxShaderCompilerThreadsARB

  // ARB_gl_spirv
  PFNGLSPECIALIZESHADERPROC glSpecializeShader; // aliases glSpecializeShaderARB

  // EXT_external_objects
  PFNGLGETUNSIGNEDBYTEVEXTPROC glGetUnsignedBytevEXT;
  PFNGLGETUNSIGNEDBYTEI_VEXTPROC glGetUnsignedBytei_vEXT;
  PFNGLDELETEMEMORYOBJECTSEXTPROC glDeleteMemoryObjectsEXT;
  PFNGLISMEMORYOBJECTEXTPROC glIsMemoryObjectEXT;
  PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT;
  PFNGLMEMORYOBJECTPARAMETERIVEXTPROC glMemoryObjectParameterivEXT;
  PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC glGetMemoryObjectParameterivEXT;
  PFNGLTEXSTORAGEMEM2DEXTPROC glTexStorageMem2DEXT;
  PFNGLTEXSTORAGEMEM2DMULTISAMPLEEXTPROC glTexStorageMem2DMultisampleEXT;
  PFNGLTEXSTORAGEMEM3DEXTPROC glTexStorageMem3DEXT;
  PFNGLTEXSTORAGEMEM3DMULTISAMPLEEXTPROC glTexStorageMem3DMultisampleEXT;
  PFNGLBUFFERSTORAGEMEMEXTPROC glBufferStorageMemEXT;
  PFNGLTEXTURESTORAGEMEM2DEXTPROC glTextureStorageMem2DEXT;
  PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC glTextureStorageMem2DMultisampleEXT;
  PFNGLTEXTURESTORAGEMEM3DEXTPROC glTextureStorageMem3DEXT;
  PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC glTextureStorageMem3DMultisampleEXT;
  PFNGLNAMEDBUFFERSTORAGEMEMEXTPROC glNamedBufferStorageMemEXT;
  PFNGLTEXSTORAGEMEM1DEXTPROC glTexStorageMem1DEXT;
  PFNGLTEXTURESTORAGEMEM1DEXTPROC glTextureStorageMem1DEXT;
  PFNGLGENSEMAPHORESEXTPROC glGenSemaphoresEXT;
  PFNGLDELETESEMAPHORESEXTPROC glDeleteSemaphoresEXT;
  PFNGLISSEMAPHOREEXTPROC glIsSemaphoreEXT;
  PFNGLSEMAPHOREPARAMETERUI64VEXTPROC glSemaphoreParameterui64vEXT;
  PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC glGetSemaphoreParameterui64vEXT;
  PFNGLWAITSEMAPHOREEXTPROC glWaitSemaphoreEXT;
  PFNGLSIGNALSEMAPHOREEXTPROC glSignalSemaphoreEXT;

  // EXT_external_objects_fd
  PFNGLIMPORTMEMORYFDEXTPROC glImportMemoryFdEXT;
  PFNGLIMPORTSEMAPHOREFDEXTPROC glImportSemaphoreFdEXT;

  // EXT_external_objects_win32
  PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT;
  PFNGLIMPORTMEMORYWIN32NAMEEXTPROC glImportMemoryWin32NameEXT;
  PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC glImportSemaphoreWin32HandleEXT;
  PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC glImportSemaphoreWin32NameEXT;

  // EXT_win32_keyed_mutex
  PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glAcquireKeyedMutexWin32EXT;
  PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glReleaseKeyedMutexWin32EXT;

  // EXT_direct_state_access below here. We only include the functions relevant for core 3.2+ GL,
  // not any functions for legacy functionality.
  //
  // NOTE: we set up ARB_dsa functions as aliases of EXT_dsa functions where they are identical.
  // This breaks our 'rule' of making core functions the canonical versions, but for good reason.
  //
  // As with other aliases, this assumes the functions defined to have identical semantics are safe
  // to substitute for each other (which in theory should be true). We do it this way around rather
  // than have EXT_dsa as aliases of ARB_dsa - which is usually what we do for EXT extension
  // variants. The reason being we want to support hw/sw configurations where ARB_dsa is not
  // present, so we need to fall back on EXT_dsa. If the EXT functions are the aliases, we will
  // never fetch them when getting function pointers, so if ARB_dsa functions aren't present then we
  // just get NULL. In theory EXT_dsa supported cases should be a strict superset of ARB_dsa
  // supported cases, so it's safe to always use the EXT variant when they're identical.
  //
  // Where a function is different, or unique to ARB_dsa, we include both separately. For ARB_dsa
  // unique functions these are at the end, noted by comments.

  PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC glCompressedTextureImage1DEXT;
  PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC glCompressedTextureImage2DEXT;
  PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC glCompressedTextureImage3DEXT;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC glCompressedTextureSubImage1DEXT;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC glCompressedTextureSubImage2DEXT;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC glCompressedTextureSubImage3DEXT;
  PFNGLGENERATETEXTUREMIPMAPEXTPROC glGenerateTextureMipmapEXT;
  PFNGLGETPOINTERI_VEXTPROC glGetPointeri_vEXT;
  PFNGLGETDOUBLEINDEXEDVEXTPROC glGetDoubleIndexedvEXT;
  PFNGLGETPOINTERINDEXEDVEXTPROC glGetPointerIndexedvEXT;
  PFNGLGETINTEGERINDEXEDVEXTPROC glGetIntegerIndexedvEXT;
  PFNGLGETBOOLEANINDEXEDVEXTPROC glGetBooleanIndexedvEXT;
  PFNGLGETFLOATINDEXEDVEXTPROC glGetFloatIndexedvEXT;
  PFNGLGETMULTITEXIMAGEEXTPROC glGetMultiTexImageEXT;
  PFNGLGETMULTITEXPARAMETERFVEXTPROC glGetMultiTexParameterfvEXT;
  PFNGLGETMULTITEXPARAMETERIVEXTPROC glGetMultiTexParameterivEXT;
  PFNGLGETMULTITEXPARAMETERIIVEXTPROC glGetMultiTexParameterIivEXT;
  PFNGLGETMULTITEXPARAMETERIUIVEXTPROC glGetMultiTexParameterIuivEXT;
  PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC glGetMultiTexLevelParameterfvEXT;
  PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC glGetMultiTexLevelParameterivEXT;
  PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC glGetCompressedMultiTexImageEXT;
  PFNGLGETNAMEDBUFFERPOINTERVEXTPROC glGetNamedBufferPointervEXT;    // aliases glGetNamedBufferPointerv
  PFNGLGETNAMEDPROGRAMIVEXTPROC glGetNamedProgramivEXT;
  PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glGetNamedFramebufferAttachmentParameterivEXT;    // aliases glGetNamedFramebufferAttachmentParameteriv
  PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC glGetNamedBufferParameterivEXT;    // aliases glGetNamedBufferParameteriv
  PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC glCheckNamedFramebufferStatusEXT;    // aliases glCheckNamedFramebufferStatus
  PFNGLGETNAMEDBUFFERSUBDATAEXTPROC glGetNamedBufferSubDataEXT;
  PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC glGetNamedFramebufferParameterivEXT;    // aliases glGetFramebufferParameterivEXT, glGetNamedFramebufferParameteriv
  PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC glGetNamedRenderbufferParameterivEXT;    // aliases glGetNamedRenderbufferParameteriv
  PFNGLGETVERTEXARRAYINTEGERVEXTPROC glGetVertexArrayIntegervEXT;
  PFNGLGETVERTEXARRAYPOINTERVEXTPROC glGetVertexArrayPointervEXT;
  PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC glGetVertexArrayIntegeri_vEXT;
  PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC glGetVertexArrayPointeri_vEXT;
  PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC glGetCompressedTextureImageEXT;
  PFNGLGETTEXTUREIMAGEEXTPROC glGetTextureImageEXT;
  PFNGLGETTEXTUREPARAMETERIVEXTPROC glGetTextureParameterivEXT;
  PFNGLGETTEXTUREPARAMETERFVEXTPROC glGetTextureParameterfvEXT;
  PFNGLGETTEXTUREPARAMETERIIVEXTPROC glGetTextureParameterIivEXT;
  PFNGLGETTEXTUREPARAMETERIUIVEXTPROC glGetTextureParameterIuivEXT;
  PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC glGetTextureLevelParameterivEXT;
  PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC glGetTextureLevelParameterfvEXT;
  PFNGLBINDMULTITEXTUREEXTPROC glBindMultiTextureEXT;
  PFNGLMAPNAMEDBUFFEREXTPROC glMapNamedBufferEXT;    // aliases glMapNamedBuffer
  PFNGLMAPNAMEDBUFFERRANGEEXTPROC glMapNamedBufferRangeEXT;
  PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC glFlushMappedNamedBufferRangeEXT;
  PFNGLUNMAPNAMEDBUFFEREXTPROC glUnmapNamedBufferEXT;            // aliases glUnmapNamedBuffer
  PFNGLCLEARNAMEDBUFFERDATAEXTPROC glClearNamedBufferDataEXT;    // aliases glClearNamedBufferData
  PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC glClearNamedBufferSubDataEXT;
  PFNGLNAMEDBUFFERDATAEXTPROC glNamedBufferDataEXT;
  PFNGLNAMEDBUFFERSTORAGEEXTPROC glNamedBufferStorageEXT;
  PFNGLNAMEDBUFFERSUBDATAEXTPROC glNamedBufferSubDataEXT;
  PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC glNamedCopyBufferSubDataEXT;
  PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC glNamedFramebufferTextureEXT;    // aliases glNamedFramebufferTexture
  PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC glNamedFramebufferTexture1DEXT;
  PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC glNamedFramebufferTexture2DEXT;
  PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC glNamedFramebufferTexture3DEXT;
  PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC glNamedFramebufferRenderbufferEXT;    // aliases glNamedFramebufferRenderbuffer
  PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC glNamedFramebufferTextureLayerEXT;    // aliases glNamedFramebufferTextureLayer
  PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC glNamedFramebufferParameteriEXT;    // aliases glNamedFramebufferParameteri
  PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC glNamedRenderbufferStorageEXT;    // aliases glNamedRenderbufferStorage
  PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glNamedRenderbufferStorageMultisampleEXT;    // aliases glNamedRenderbufferStorageMultisample
  PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC glFramebufferDrawBufferEXT;    // aliases glNamedFramebufferDrawBuffer
  PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC glFramebufferDrawBuffersEXT;    // aliases glNamedFramebufferDrawBuffers
  PFNGLFRAMEBUFFERREADBUFFEREXTPROC glFramebufferReadBufferEXT;    // aliases glNamedFramebufferReadBuffer
  PFNGLTEXTUREBUFFEREXTPROC glTextureBufferEXT;
  PFNGLTEXTUREBUFFERRANGEEXTPROC glTextureBufferRangeEXT;
  PFNGLTEXTUREIMAGE1DEXTPROC glTextureImage1DEXT;
  PFNGLTEXTUREIMAGE2DEXTPROC glTextureImage2DEXT;
  PFNGLTEXTUREIMAGE3DEXTPROC glTextureImage3DEXT;
  PFNGLTEXTUREPARAMETERFEXTPROC glTextureParameterfEXT;
  PFNGLTEXTUREPARAMETERFVEXTPROC glTextureParameterfvEXT;
  PFNGLTEXTUREPARAMETERIEXTPROC glTextureParameteriEXT;
  PFNGLTEXTUREPARAMETERIVEXTPROC glTextureParameterivEXT;
  PFNGLTEXTUREPARAMETERIIVEXTPROC glTextureParameterIivEXT;
  PFNGLTEXTUREPARAMETERIUIVEXTPROC glTextureParameterIuivEXT;
  PFNGLTEXTURESTORAGE1DEXTPROC glTextureStorage1DEXT;
  PFNGLTEXTURESTORAGE2DEXTPROC glTextureStorage2DEXT;
  PFNGLTEXTURESTORAGE3DEXTPROC glTextureStorage3DEXT;
  PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC glTextureStorage2DMultisampleEXT;
  PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC glTextureStorage3DMultisampleEXT;
  PFNGLTEXTURESUBIMAGE1DEXTPROC glTextureSubImage1DEXT;
  PFNGLTEXTURESUBIMAGE2DEXTPROC glTextureSubImage2DEXT;
  PFNGLTEXTURESUBIMAGE3DEXTPROC glTextureSubImage3DEXT;
  PFNGLCOPYTEXTUREIMAGE1DEXTPROC glCopyTextureImage1DEXT;
  PFNGLCOPYTEXTUREIMAGE2DEXTPROC glCopyTextureImage2DEXT;
  PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC glCopyTextureSubImage1DEXT;
  PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC glCopyTextureSubImage2DEXT;
  PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC glCopyTextureSubImage3DEXT;
  PFNGLMULTITEXPARAMETERIEXTPROC glMultiTexParameteriEXT;
  PFNGLMULTITEXPARAMETERIVEXTPROC glMultiTexParameterivEXT;
  PFNGLMULTITEXPARAMETERFEXTPROC glMultiTexParameterfEXT;
  PFNGLMULTITEXPARAMETERFVEXTPROC glMultiTexParameterfvEXT;
  PFNGLMULTITEXIMAGE1DEXTPROC glMultiTexImage1DEXT;
  PFNGLMULTITEXIMAGE2DEXTPROC glMultiTexImage2DEXT;
  PFNGLMULTITEXSUBIMAGE1DEXTPROC glMultiTexSubImage1DEXT;
  PFNGLMULTITEXSUBIMAGE2DEXTPROC glMultiTexSubImage2DEXT;
  PFNGLCOPYMULTITEXIMAGE1DEXTPROC glCopyMultiTexImage1DEXT;
  PFNGLCOPYMULTITEXIMAGE2DEXTPROC glCopyMultiTexImage2DEXT;
  PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC glCopyMultiTexSubImage1DEXT;
  PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC glCopyMultiTexSubImage2DEXT;
  PFNGLMULTITEXIMAGE3DEXTPROC glMultiTexImage3DEXT;
  PFNGLMULTITEXSUBIMAGE3DEXTPROC glMultiTexSubImage3DEXT;
  PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC glCopyMultiTexSubImage3DEXT;
  PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC glCompressedMultiTexImage3DEXT;
  PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC glCompressedMultiTexImage2DEXT;
  PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC glCompressedMultiTexImage1DEXT;
  PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC glCompressedMultiTexSubImage3DEXT;
  PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC glCompressedMultiTexSubImage2DEXT;
  PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC glCompressedMultiTexSubImage1DEXT;
  PFNGLMULTITEXBUFFEREXTPROC glMultiTexBufferEXT;
  PFNGLMULTITEXPARAMETERIIVEXTPROC glMultiTexParameterIivEXT;
  PFNGLMULTITEXPARAMETERIUIVEXTPROC glMultiTexParameterIuivEXT;
  PFNGLGENERATEMULTITEXMIPMAPEXTPROC glGenerateMultiTexMipmapEXT;
  PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC glVertexArrayVertexAttribOffsetEXT;
  PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC glVertexArrayVertexAttribIOffsetEXT;
  PFNGLENABLEVERTEXARRAYATTRIBEXTPROC glEnableVertexArrayAttribEXT;    // aliases glEnableVertexArrayAttrib
  PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC glDisableVertexArrayAttribEXT;    // aliases glDisableVertexArrayAttrib
  PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC glVertexArrayBindVertexBufferEXT;    // aliases glVertexArrayVertexBuffer
  PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC glVertexArrayVertexAttribFormatEXT;    // aliases glVertexArrayAttribFormat
  PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC glVertexArrayVertexAttribIFormatEXT;    // aliases glVertexArrayAttribIFormat
  PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC glVertexArrayVertexAttribLFormatEXT;    // aliases glVertexArrayAttribLFormat
  PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC glVertexArrayVertexAttribBindingEXT;    // aliases glVertexArrayAttribBinding
  PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC glVertexArrayVertexBindingDivisorEXT;    // aliases glVertexArrayBindingDivisor
  PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC glVertexArrayVertexAttribLOffsetEXT;
  PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC glVertexArrayVertexAttribDivisorEXT;

  // ARB_direct_state_access unique functions (others will be listed as aliases of EXT_dsa, see above).

  PFNGLCREATETRANSFORMFEEDBACKSPROC glCreateTransformFeedbacks;
  PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC glTransformFeedbackBufferBase;
  PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC glTransformFeedbackBufferRange;
  PFNGLGETTRANSFORMFEEDBACKI64_VPROC glGetTransformFeedbacki64_v;
  PFNGLGETTRANSFORMFEEDBACKI_VPROC glGetTransformFeedbacki_v;
  PFNGLGETTRANSFORMFEEDBACKIVPROC glGetTransformFeedbackiv;
  PFNGLCREATEBUFFERSPROC glCreateBuffers;

  // these functions aren't aliases only because the size parameter is a different type
  PFNGLGETNAMEDBUFFERSUBDATAPROC glGetNamedBufferSubData;
  PFNGLNAMEDBUFFERSTORAGEPROC glNamedBufferStorage;
  PFNGLNAMEDBUFFERDATAPROC glNamedBufferData;
  PFNGLNAMEDBUFFERSUBDATAPROC glNamedBufferSubData;
  PFNGLCOPYNAMEDBUFFERSUBDATAPROC glCopyNamedBufferSubData;
  PFNGLCLEARNAMEDBUFFERSUBDATAPROC glClearNamedBufferSubData;
  PFNGLMAPNAMEDBUFFERRANGEPROC glMapNamedBufferRange;
  PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC glFlushMappedNamedBufferRange;

  PFNGLGETNAMEDBUFFERPARAMETERI64VPROC glGetNamedBufferParameteri64v;
  PFNGLCREATEFRAMEBUFFERSPROC glCreateFramebuffers;
  PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC glInvalidateNamedFramebufferData;
  PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC glInvalidateNamedFramebufferSubData;
  PFNGLCLEARNAMEDFRAMEBUFFERIVPROC glClearNamedFramebufferiv;
  PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC glClearNamedFramebufferuiv;
  PFNGLCLEARNAMEDFRAMEBUFFERFVPROC glClearNamedFramebufferfv;
  PFNGLCLEARNAMEDFRAMEBUFFERFIPROC glClearNamedFramebufferfi;
  PFNGLBLITNAMEDFRAMEBUFFERPROC glBlitNamedFramebuffer;
  PFNGLCREATERENDERBUFFERSPROC glCreateRenderbuffers;
  PFNGLCREATETEXTURESPROC glCreateTextures;
  // many of these texture functions only vary by the lack of target parameter from the EXT_dsa
  // variants. The handling of this is generally to pipe through the EXT_dsa variant with a target
  // of GL_NONE, and that signifies the ARB_dsa function should be used. See gl_texture_funcs.cpp
  PFNGLTEXTUREBUFFERPROC glTextureBuffer;
  PFNGLTEXTUREBUFFERRANGEPROC glTextureBufferRange;
  PFNGLTEXTURESTORAGE1DPROC glTextureStorage1D;
  PFNGLTEXTURESTORAGE2DPROC glTextureStorage2D;
  PFNGLTEXTURESTORAGE3DPROC glTextureStorage3D;
  PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC glTextureStorage2DMultisample;
  PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC glTextureStorage3DMultisample;
  PFNGLTEXTURESUBIMAGE1DPROC glTextureSubImage1D;
  PFNGLTEXTURESUBIMAGE2DPROC glTextureSubImage2D;
  PFNGLTEXTURESUBIMAGE3DPROC glTextureSubImage3D;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC glCompressedTextureSubImage1D;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC glCompressedTextureSubImage2D;
  PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC glCompressedTextureSubImage3D;
  PFNGLCOPYTEXTURESUBIMAGE1DPROC glCopyTextureSubImage1D;
  PFNGLCOPYTEXTURESUBIMAGE2DPROC glCopyTextureSubImage2D;
  PFNGLCOPYTEXTURESUBIMAGE3DPROC glCopyTextureSubImage3D;
  PFNGLTEXTUREPARAMETERFPROC glTextureParameterf;
  PFNGLTEXTUREPARAMETERFVPROC glTextureParameterfv;
  PFNGLTEXTUREPARAMETERIPROC glTextureParameteri;
  PFNGLTEXTUREPARAMETERIIVPROC glTextureParameterIiv;
  PFNGLTEXTUREPARAMETERIUIVPROC glTextureParameterIuiv;
  PFNGLTEXTUREPARAMETERIVPROC glTextureParameteriv;
  PFNGLGENERATETEXTUREMIPMAPPROC glGenerateTextureMipmap;
  PFNGLBINDTEXTUREUNITPROC glBindTextureUnit;
  PFNGLGETTEXTUREIMAGEPROC glGetTextureImage;
  PFNGLGETTEXTURESUBIMAGEPROC glGetTextureSubImage;
  PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC glGetCompressedTextureImage;
  PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC glGetCompressedTextureSubImage;
  PFNGLGETTEXTURELEVELPARAMETERFVPROC glGetTextureLevelParameterfv;
  PFNGLGETTEXTURELEVELPARAMETERIVPROC glGetTextureLevelParameteriv;
  PFNGLGETTEXTUREPARAMETERIIVPROC glGetTextureParameterIiv;
  PFNGLGETTEXTUREPARAMETERIUIVPROC glGetTextureParameterIuiv;
  PFNGLGETTEXTUREPARAMETERFVPROC glGetTextureParameterfv;
  PFNGLGETTEXTUREPARAMETERIVPROC glGetTextureParameteriv;
  PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays;
  PFNGLCREATESAMPLERSPROC glCreateSamplers;
  PFNGLCREATEPROGRAMPIPELINESPROC glCreateProgramPipelines;
  PFNGLCREATEQUERIESPROC glCreateQueries;
  PFNGLVERTEXARRAYELEMENTBUFFERPROC glVertexArrayElementBuffer;
  PFNGLVERTEXARRAYVERTEXBUFFERSPROC glVertexArrayVertexBuffers;
  PFNGLGETVERTEXARRAYIVPROC glGetVertexArrayiv;
  PFNGLGETVERTEXARRAYINDEXED64IVPROC glGetVertexArrayIndexed64iv;
  PFNGLGETVERTEXARRAYINDEXEDIVPROC glGetVertexArrayIndexediv;
  PFNGLGETQUERYBUFFEROBJECTI64VPROC glGetQueryBufferObjecti64v;
  PFNGLGETQUERYBUFFEROBJECTIVPROC glGetQueryBufferObjectiv;
  PFNGLGETQUERYBUFFEROBJECTUI64VPROC glGetQueryBufferObjectui64v;
  PFNGLGETQUERYBUFFEROBJECTUIVPROC glGetQueryBufferObjectuiv;

  // INTEL_performance_query
  PFNGLBEGINPERFQUERYINTELPROC glBeginPerfQueryINTEL;
  PFNGLCREATEPERFQUERYINTELPROC glCreatePerfQueryINTEL;
  PFNGLDELETEPERFQUERYINTELPROC glDeletePerfQueryINTEL;
  PFNGLENDPERFQUERYINTELPROC glEndPerfQueryINTEL;
  PFNGLGETFIRSTPERFQUERYIDINTELPROC glGetFirstPerfQueryIdINTEL;
  PFNGLGETNEXTPERFQUERYIDINTELPROC glGetNextPerfQueryIdINTEL;
  PFNGLGETPERFCOUNTERINFOINTELPROC glGetPerfCounterInfoINTEL;
  PFNGLGETPERFQUERYDATAINTELPROC glGetPerfQueryDataINTEL;
  PFNGLGETPERFQUERYIDBYNAMEINTELPROC glGetPerfQueryIdByNameINTEL;
  PFNGLGETPERFQUERYINFOINTELPROC glGetPerfQueryInfoINTEL;

  // stubbed on all non-windows platforms
  PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV;
  PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
  PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
  PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
  PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
  PFNWGLDXOBJECTACCESSNVPROC wglDXObjectAccessNV;
  PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
  PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
};
// clang-format on

extern GLDispatchTable GL;

class WrappedOpenGL;

// the hooks need to call into our wrapped implementation from the entry point, but there can be
// multiple ways to initialise on a given platform, so whenever a context becomes active we call
// this function to register the 'active' GL implementation. This does mean e.g. we don't support
// wgl or glX and EGL together in the same application.
void SetDriverForHooks(WrappedOpenGL *driver);

// On windows we support injecting into the program at runtime, so we need to only enable hooks when
// we see context creation, to prevent crashes trying to handle function calls having seen no
// intialisation. This can have false positives if the program creates a context late, but it's the
// best we can do.
// On apple we suppress hooks while entering any CGL function so we don't record internal work that
// can mess up the replay
#if ENABLED(RDOC_WIN32) || ENABLED(RDOC_APPLE)
void EnableGLHooks();
void DisableGLHooks();
#else
#define EnableGLHooks() (void)0
#endif

#if ENABLED(RDOC_WIN32)
void DisableWGLHooksForEGL();
#else
#define DisableWGLHooksForEGL() (void)0
#endif

// this function looks up our list of hook entry points and returns our hook entry point instead of
// the real function, if it exists, or the real function if not. It's used in the platform-specific
// implementations of GetProcAddress to look up the shared list of hooks.
void *HookedGetProcAddress(const char *funcname, void *realFunc);

bool FullyImplementedFunction(const char *funcname);