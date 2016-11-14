/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "gles_common.h"

struct GLHookSet
{
  // ++ dllexport
  PFNGLACTIVESHADERPROGRAMPROC                                glActiveShaderProgram;        // aliases glActiveShaderProgramEXT
  PFNGLACTIVETEXTUREPROC                                      glActiveTexture;              
  PFNGLATTACHSHADERPROC                                       glAttachShader;               
  PFNGLBEGINQUERYPROC                                         glBeginQuery;                 // aliases glBeginQueryEXT
  PFNGLBEGINTRANSFORMFEEDBACKPROC                             glBeginTransformFeedback;     
  PFNGLBINDATTRIBLOCATIONPROC                                 glBindAttribLocation;         
  PFNGLBINDBUFFERPROC                                         glBindBuffer;                 
  PFNGLBINDBUFFERBASEPROC                                     glBindBufferBase;             
  PFNGLBINDBUFFERRANGEPROC                                    glBindBufferRange;            
  PFNGLBINDFRAMEBUFFERPROC                                    glBindFramebuffer;            
  PFNGLBINDIMAGETEXTUREPROC                                   glBindImageTexture;           
  PFNGLBINDPROGRAMPIPELINEPROC                                glBindProgramPipeline;        // aliases glBindProgramPipelineEXT
  PFNGLBINDRENDERBUFFERPROC                                   glBindRenderbuffer;           
  PFNGLBINDSAMPLERPROC                                        glBindSampler;                
  PFNGLBINDTEXTUREPROC                                        glBindTexture;                
  PFNGLBINDTRANSFORMFEEDBACKPROC                              glBindTransformFeedback;      
  PFNGLBINDVERTEXARRAYPROC                                    glBindVertexArray;            // aliases glBindVertexArrayOES
  PFNGLBINDVERTEXBUFFERPROC                                   glBindVertexBuffer;           
  PFNGLBLENDBARRIERPROC                                       glBlendBarrier;               // aliases glBlendBarrierKHR, glBlendBarrierNV
  PFNGLBLENDCOLORPROC                                         glBlendColor;                 
  PFNGLBLENDEQUATIONPROC                                      glBlendEquation;              
  PFNGLBLENDEQUATIONIPROC                                     glBlendEquationi;             // aliases glBlendEquationiEXT, glBlendEquationiOES
  PFNGLBLENDEQUATIONSEPARATEPROC                              glBlendEquationSeparate;      
  PFNGLBLENDEQUATIONSEPARATEIPROC                             glBlendEquationSeparatei;     // aliases glBlendEquationSeparateiEXT, glBlendEquationSeparateiOES
  PFNGLBLENDFUNCPROC                                          glBlendFunc;                  
  PFNGLBLENDFUNCIPROC                                         glBlendFunci;                 // aliases glBlendFunciEXT, glBlendFunciOES
  PFNGLBLENDFUNCSEPARATEPROC                                  glBlendFuncSeparate;          
  PFNGLBLENDFUNCSEPARATEIPROC                                 glBlendFuncSeparatei;         // aliases glBlendFuncSeparateiEXT, glBlendFuncSeparateiOES
  PFNGLBLITFRAMEBUFFERPROC                                    glBlitFramebuffer;            // aliases glBlitFramebufferANGLE, glBlitFramebufferNV
  PFNGLBUFFERDATAPROC                                         glBufferData;                 
  PFNGLBUFFERSUBDATAPROC                                      glBufferSubData;              
  PFNGLCHECKFRAMEBUFFERSTATUSPROC                             glCheckFramebufferStatus;     
  PFNGLCLEARPROC                                              glClear;                      
  PFNGLCLEARBUFFERFIPROC                                      glClearBufferfi;              
  PFNGLCLEARBUFFERFVPROC                                      glClearBufferfv;              
  PFNGLCLEARBUFFERIVPROC                                      glClearBufferiv;              
  PFNGLCLEARBUFFERUIVPROC                                     glClearBufferuiv;             
  PFNGLCLEARCOLORPROC                                         glClearColor;                 
  PFNGLCLEARDEPTHFPROC                                        glClearDepthf;                
  PFNGLCLEARSTENCILPROC                                       glClearStencil;               
  PFNGLCLIENTWAITSYNCPROC                                     glClientWaitSync;             // aliases glClientWaitSyncAPPLE
  PFNGLCOLORMASKPROC                                          glColorMask;                  
  PFNGLCOLORMASKIPROC                                         glColorMaski;                 // aliases glColorMaskiEXT, glColorMaskiOES
  PFNGLCOMPILESHADERPROC                                      glCompileShader;              
  PFNGLCOMPRESSEDTEXIMAGE2DPROC                               glCompressedTexImage2D;       
  PFNGLCOMPRESSEDTEXIMAGE3DPROC                               glCompressedTexImage3D;       // aliases glCompressedTexImage3DOES
  PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC                            glCompressedTexSubImage2D;    
  PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC                            glCompressedTexSubImage3D;    // aliases glCompressedTexSubImage3DOES
  PFNGLCOPYBUFFERSUBDATAPROC                                  glCopyBufferSubData;          // aliases glCopyBufferSubDataNV
  PFNGLCOPYIMAGESUBDATAPROC                                   glCopyImageSubData;           // aliases glCopyImageSubDataEXT, glCopyImageSubDataOES
  PFNGLCOPYTEXIMAGE2DPROC                                     glCopyTexImage2D;             
  PFNGLCOPYTEXSUBIMAGE2DPROC                                  glCopyTexSubImage2D;          
  PFNGLCOPYTEXSUBIMAGE3DPROC                                  glCopyTexSubImage3D;          // aliases glCopyTexSubImage3DOES
  PFNGLCREATEPROGRAMPROC                                      glCreateProgram;              
  PFNGLCREATESHADERPROC                                       glCreateShader;               
  PFNGLCREATESHADERPROGRAMVPROC                               glCreateShaderProgramv;       // aliases glCreateShaderProgramvEXT
  PFNGLCULLFACEPROC                                           glCullFace;                   
  PFNGLDEBUGMESSAGECALLBACKPROC                               glDebugMessageCallback;       // aliases glDebugMessageCallbackKHR
  PFNGLDEBUGMESSAGECONTROLPROC                                glDebugMessageControl;        // aliases glDebugMessageControlKHR
  PFNGLDEBUGMESSAGEINSERTPROC                                 glDebugMessageInsert;         // aliases glDebugMessageInsertKHR
  PFNGLDELETEBUFFERSPROC                                      glDeleteBuffers;              
  PFNGLDELETEFRAMEBUFFERSPROC                                 glDeleteFramebuffers;         
  PFNGLDELETEPROGRAMPROC                                      glDeleteProgram;              
  PFNGLDELETEPROGRAMPIPELINESPROC                             glDeleteProgramPipelines;     // aliases glDeleteProgramPipelinesEXT
  PFNGLDELETEQUERIESPROC                                      glDeleteQueries;              // aliases glDeleteQueriesEXT
  PFNGLDELETERENDERBUFFERSPROC                                glDeleteRenderbuffers;        
  PFNGLDELETESAMPLERSPROC                                     glDeleteSamplers;             
  PFNGLDELETESHADERPROC                                       glDeleteShader;               
  PFNGLDELETESYNCPROC                                         glDeleteSync;                 // aliases glDeleteSyncAPPLE
  PFNGLDELETETEXTURESPROC                                     glDeleteTextures;             
  PFNGLDELETETRANSFORMFEEDBACKSPROC                           glDeleteTransformFeedbacks;   
  PFNGLDELETEVERTEXARRAYSPROC                                 glDeleteVertexArrays;         // aliases glDeleteVertexArraysOES
  PFNGLDEPTHFUNCPROC                                          glDepthFunc;                  
  PFNGLDEPTHMASKPROC                                          glDepthMask;                  
  PFNGLDEPTHRANGEFPROC                                        glDepthRangef;                
  PFNGLDETACHSHADERPROC                                       glDetachShader;               
  PFNGLDISABLEPROC                                            glDisable;                    
  PFNGLDISABLEIPROC                                           glDisablei;                   // aliases glDisableiEXT, glDisableiNV, glDisableiOES
  PFNGLDISABLEVERTEXATTRIBARRAYPROC                           glDisableVertexAttribArray;   
  PFNGLDISPATCHCOMPUTEPROC                                    glDispatchCompute;            
  PFNGLDISPATCHCOMPUTEINDIRECTPROC                            glDispatchComputeIndirect;    
  PFNGLDRAWARRAYSPROC                                         glDrawArrays;                 
  PFNGLDRAWARRAYSINDIRECTPROC                                 glDrawArraysIndirect;         
  PFNGLDRAWARRAYSINSTANCEDPROC                                glDrawArraysInstanced;        // aliases glDrawArraysInstancedANGLE, glDrawArraysInstancedEXT, glDrawArraysInstancedNV
  PFNGLDRAWBUFFERSPROC                                        glDrawBuffers;                // aliases glDrawBuffersEXT, glDrawBuffersNV
  PFNGLDRAWELEMENTSPROC                                       glDrawElements;               
  PFNGLDRAWELEMENTSBASEVERTEXPROC                             glDrawElementsBaseVertex;     // aliases glDrawElementsBaseVertexEXT, glDrawElementsBaseVertexOES
  PFNGLDRAWELEMENTSINDIRECTPROC                               glDrawElementsIndirect;       
  PFNGLDRAWELEMENTSINSTANCEDPROC                              glDrawElementsInstanced;      // aliases glDrawElementsInstancedANGLE, glDrawElementsInstancedEXT, glDrawElementsInstancedNV
  PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC                    glDrawElementsInstancedBaseVertex;// aliases glDrawElementsInstancedBaseVertexEXT, glDrawElementsInstancedBaseVertexOES
  PFNGLDRAWRANGEELEMENTSPROC                                  glDrawRangeElements;          
  PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC                        glDrawRangeElementsBaseVertex;// aliases glDrawRangeElementsBaseVertexEXT, glDrawRangeElementsBaseVertexOES
  PFNGLENABLEPROC                                             glEnable;                     
  PFNGLENABLEIPROC                                            glEnablei;                    // aliases glEnableiEXT, glEnableiNV, glEnableiOES
  PFNGLENABLEVERTEXATTRIBARRAYPROC                            glEnableVertexAttribArray;    
  PFNGLENDQUERYPROC                                           glEndQuery;                   // aliases glEndQueryEXT
  PFNGLENDTRANSFORMFEEDBACKPROC                               glEndTransformFeedback;       
  PFNGLFENCESYNCPROC                                          glFenceSync;                  // aliases glFenceSyncAPPLE
  PFNGLFINISHPROC                                             glFinish;                     
  PFNGLFLUSHPROC                                              glFlush;                      
  PFNGLFLUSHMAPPEDBUFFERRANGEPROC                             glFlushMappedBufferRange;     // aliases glFlushMappedBufferRangeEXT
  PFNGLFRAMEBUFFERPARAMETERIPROC                              glFramebufferParameteri;      
  PFNGLFRAMEBUFFERRENDERBUFFERPROC                            glFramebufferRenderbuffer;    
  PFNGLFRAMEBUFFERTEXTUREPROC                                 glFramebufferTexture;         // aliases glFramebufferTextureEXT, glFramebufferTextureOES
  PFNGLFRAMEBUFFERTEXTURE2DPROC                               glFramebufferTexture2D;       
  PFNGLFRAMEBUFFERTEXTURELAYERPROC                            glFramebufferTextureLayer;    
  PFNGLFRONTFACEPROC                                          glFrontFace;                  
  PFNGLGENBUFFERSPROC                                         glGenBuffers;                 
  PFNGLGENERATEMIPMAPPROC                                     glGenerateMipmap;             
  PFNGLGENFRAMEBUFFERSPROC                                    glGenFramebuffers;            
  PFNGLGENPROGRAMPIPELINESPROC                                glGenProgramPipelines;        // aliases glGenProgramPipelinesEXT
  PFNGLGENQUERIESPROC                                         glGenQueries;                 // aliases glGenQueriesEXT
  PFNGLGENRENDERBUFFERSPROC                                   glGenRenderbuffers;           
  PFNGLGENSAMPLERSPROC                                        glGenSamplers;                
  PFNGLGENTEXTURESPROC                                        glGenTextures;                
  PFNGLGENTRANSFORMFEEDBACKSPROC                              glGenTransformFeedbacks;      
  PFNGLGENVERTEXARRAYSPROC                                    glGenVertexArrays;            // aliases glGenVertexArraysOES
  PFNGLGETACTIVEATTRIBPROC                                    glGetActiveAttrib;            
  PFNGLGETACTIVEUNIFORMPROC                                   glGetActiveUniform;           
  PFNGLGETACTIVEUNIFORMBLOCKIVPROC                            glGetActiveUniformBlockiv;    
  PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC                          glGetActiveUniformBlockName;  
  PFNGLGETACTIVEUNIFORMSIVPROC                                glGetActiveUniformsiv;        
  PFNGLGETATTACHEDSHADERSPROC                                 glGetAttachedShaders;         
  PFNGLGETATTRIBLOCATIONPROC                                  glGetAttribLocation;          
  PFNGLGETBOOLEANI_VPROC                                      glGetBooleani_v;              
  PFNGLGETBOOLEANVPROC                                        glGetBooleanv;                
  PFNGLGETBUFFERPARAMETERI64VPROC                             glGetBufferParameteri64v;     
  PFNGLGETBUFFERPARAMETERIVPROC                               glGetBufferParameteriv;       
  PFNGLGETBUFFERPOINTERVPROC                                  glGetBufferPointerv;          // aliases glGetBufferPointervOES
  PFNGLGETDEBUGMESSAGELOGPROC                                 glGetDebugMessageLog;         // aliases glGetDebugMessageLogKHR
  PFNGLGETERRORPROC                                           glGetError;                   
  PFNGLGETFLOATVPROC                                          glGetFloatv;                  
  PFNGLGETFRAGDATALOCATIONPROC                                glGetFragDataLocation;        
  PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC                glGetFramebufferAttachmentParameteriv;
  PFNGLGETFRAMEBUFFERPARAMETERIVPROC                          glGetFramebufferParameteriv;  
  PFNGLGETGRAPHICSRESETSTATUSPROC                             glGetGraphicsResetStatus;     // aliases glGetGraphicsResetStatusEXT, glGetGraphicsResetStatusKHR
  PFNGLGETINTEGER64I_VPROC                                    glGetInteger64i_v;            
  PFNGLGETINTEGER64VPROC                                      glGetInteger64v;              // aliases glGetInteger64vAPPLE
  PFNGLGETINTEGERI_VPROC                                      glGetIntegeri_v;              // aliases glGetIntegeri_vEXT
  PFNGLGETINTEGERVPROC                                        glGetIntegerv;                
  PFNGLGETINTERNALFORMATIVPROC                                glGetInternalformativ;        
  PFNGLGETMULTISAMPLEFVPROC                                   glGetMultisamplefv;           
  PFNGLGETNUNIFORMFVPROC                                      glGetnUniformfv;              // aliases glGetnUniformfvEXT, glGetnUniformfvKHR
  PFNGLGETNUNIFORMIVPROC                                      glGetnUniformiv;              // aliases glGetnUniformivEXT, glGetnUniformivKHR
  PFNGLGETNUNIFORMUIVPROC                                     glGetnUniformuiv;             // aliases glGetnUniformuivKHR
  PFNGLGETOBJECTLABELPROC                                     glGetObjectLabel;             // aliases glGetObjectLabelEXT, glGetObjectLabelKHR
  PFNGLGETOBJECTPTRLABELPROC                                  glGetObjectPtrLabel;          // aliases glGetObjectPtrLabelKHR
  PFNGLGETPOINTERVPROC                                        glGetPointerv;                // aliases glGetPointervKHR
  PFNGLGETPROGRAMBINARYPROC                                   glGetProgramBinary;           // aliases glGetProgramBinaryOES
  PFNGLGETPROGRAMINFOLOGPROC                                  glGetProgramInfoLog;          
  PFNGLGETPROGRAMINTERFACEIVPROC                              glGetProgramInterfaceiv;      
  PFNGLGETPROGRAMIVPROC                                       glGetProgramiv;               
  PFNGLGETPROGRAMPIPELINEINFOLOGPROC                          glGetProgramPipelineInfoLog;  // aliases glGetProgramPipelineInfoLogEXT
  PFNGLGETPROGRAMPIPELINEIVPROC                               glGetProgramPipelineiv;       // aliases glGetProgramPipelineivEXT
  PFNGLGETPROGRAMRESOURCEINDEXPROC                            glGetProgramResourceIndex;    
  PFNGLGETPROGRAMRESOURCEIVPROC                               glGetProgramResourceiv;       
  PFNGLGETPROGRAMRESOURCELOCATIONPROC                         glGetProgramResourceLocation; 
  PFNGLGETPROGRAMRESOURCENAMEPROC                             glGetProgramResourceName;     
  PFNGLGETQUERYIVPROC                                         glGetQueryiv;                 // aliases glGetQueryivEXT
  PFNGLGETQUERYOBJECTUIVPROC                                  glGetQueryObjectuiv;          // aliases glGetQueryObjectuivEXT
  PFNGLGETRENDERBUFFERPARAMETERIVPROC                         glGetRenderbufferParameteriv; 
  PFNGLGETSAMPLERPARAMETERFVPROC                              glGetSamplerParameterfv;      
  PFNGLGETSAMPLERPARAMETERIIVPROC                             glGetSamplerParameterIiv;     // aliases glGetSamplerParameterIivEXT, glGetSamplerParameterIivOES
  PFNGLGETSAMPLERPARAMETERIUIVPROC                            glGetSamplerParameterIuiv;    // aliases glGetSamplerParameterIuivEXT, glGetSamplerParameterIuivOES
  PFNGLGETSAMPLERPARAMETERIVPROC                              glGetSamplerParameteriv;      
  PFNGLGETSHADERINFOLOGPROC                                   glGetShaderInfoLog;           
  PFNGLGETSHADERIVPROC                                        glGetShaderiv;                
  PFNGLGETSHADERPRECISIONFORMATPROC                           glGetShaderPrecisionFormat;   
  PFNGLGETSHADERSOURCEPROC                                    glGetShaderSource;            
  PFNGLGETSTRINGPROC                                          glGetString;                  
  PFNGLGETSTRINGIPROC                                         glGetStringi;                 
  PFNGLGETSYNCIVPROC                                          glGetSynciv;                  // aliases glGetSyncivAPPLE
  PFNGLGETTEXLEVELPARAMETERFVPROC                             glGetTexLevelParameterfv;     
  PFNGLGETTEXLEVELPARAMETERIVPROC                             glGetTexLevelParameteriv;     
  PFNGLGETTEXPARAMETERFVPROC                                  glGetTexParameterfv;          
  PFNGLGETTEXPARAMETERIIVPROC                                 glGetTexParameterIiv;         // aliases glGetTexParameterIivEXT, glGetTexParameterIivOES
  PFNGLGETTEXPARAMETERIUIVPROC                                glGetTexParameterIuiv;        // aliases glGetTexParameterIuivEXT, glGetTexParameterIuivOES
  PFNGLGETTEXPARAMETERIVPROC                                  glGetTexParameteriv;          
  PFNGLGETTRANSFORMFEEDBACKVARYINGPROC                        glGetTransformFeedbackVarying;
  PFNGLGETUNIFORMBLOCKINDEXPROC                               glGetUniformBlockIndex;       
  PFNGLGETUNIFORMFVPROC                                       glGetUniformfv;               
  PFNGLGETUNIFORMINDICESPROC                                  glGetUniformIndices;          
  PFNGLGETUNIFORMIVPROC                                       glGetUniformiv;               
  PFNGLGETUNIFORMLOCATIONPROC                                 glGetUniformLocation;         
  PFNGLGETUNIFORMUIVPROC                                      glGetUniformuiv;              
  PFNGLGETVERTEXATTRIBFVPROC                                  glGetVertexAttribfv;          
  PFNGLGETVERTEXATTRIBIIVPROC                                 glGetVertexAttribIiv;         
  PFNGLGETVERTEXATTRIBIUIVPROC                                glGetVertexAttribIuiv;        
  PFNGLGETVERTEXATTRIBIVPROC                                  glGetVertexAttribiv;          
  PFNGLGETVERTEXATTRIBPOINTERVPROC                            glGetVertexAttribPointerv;    
  PFNGLHINTPROC                                               glHint;                       
  PFNGLINVALIDATEFRAMEBUFFERPROC                              glInvalidateFramebuffer;      
  PFNGLINVALIDATESUBFRAMEBUFFERPROC                           glInvalidateSubFramebuffer;   
  PFNGLISBUFFERPROC                                           glIsBuffer;                   
  PFNGLISENABLEDPROC                                          glIsEnabled;                  
  PFNGLISENABLEDIPROC                                         glIsEnabledi;                 // aliases glIsEnablediEXT, glIsEnablediNV, glIsEnablediOES
  PFNGLISFRAMEBUFFERPROC                                      glIsFramebuffer;              
  PFNGLISPROGRAMPROC                                          glIsProgram;                  
  PFNGLISPROGRAMPIPELINEPROC                                  glIsProgramPipeline;          // aliases glIsProgramPipelineEXT
  PFNGLISQUERYPROC                                            glIsQuery;                    // aliases glIsQueryEXT
  PFNGLISRENDERBUFFERPROC                                     glIsRenderbuffer;             
  PFNGLISSAMPLERPROC                                          glIsSampler;                  
  PFNGLISSHADERPROC                                           glIsShader;                   
  PFNGLISSYNCPROC                                             glIsSync;                     // aliases glIsSyncAPPLE
  PFNGLISTEXTUREPROC                                          glIsTexture;                  
  PFNGLISTRANSFORMFEEDBACKPROC                                glIsTransformFeedback;        
  PFNGLISVERTEXARRAYPROC                                      glIsVertexArray;              // aliases glIsVertexArrayOES
  PFNGLLINEWIDTHPROC                                          glLineWidth;                  
  PFNGLLINKPROGRAMPROC                                        glLinkProgram;                
  PFNGLMAPBUFFERRANGEPROC                                     glMapBufferRange;             // aliases glMapBufferRangeEXT
  PFNGLMEMORYBARRIERPROC                                      glMemoryBarrier;              
  PFNGLMEMORYBARRIERBYREGIONPROC                              glMemoryBarrierByRegion;      
  PFNGLMINSAMPLESHADINGPROC                                   glMinSampleShading;           // aliases glMinSampleShadingOES
  PFNGLOBJECTLABELPROC                                        glObjectLabel;                // aliases glObjectLabelKHR
  PFNGLOBJECTPTRLABELPROC                                     glObjectPtrLabel;             // aliases glObjectPtrLabelKHR
  PFNGLPATCHPARAMETERIPROC                                    glPatchParameteri;            // aliases glPatchParameteriEXT, glPatchParameteriOES
  PFNGLPAUSETRANSFORMFEEDBACKPROC                             glPauseTransformFeedback;     
  PFNGLPIXELSTOREIPROC                                        glPixelStorei;                
  PFNGLPOLYGONOFFSETPROC                                      glPolygonOffset;              
  PFNGLPOPDEBUGGROUPPROC                                      glPopDebugGroup;              // aliases glPopDebugGroupKHR
  PFNGLPRIMITIVEBOUNDINGBOXPROC                               glPrimitiveBoundingBox;       // aliases glPrimitiveBoundingBoxEXT, glPrimitiveBoundingBoxOES
  PFNGLPROGRAMBINARYPROC                                      glProgramBinary;              // aliases glProgramBinaryOES
  PFNGLPROGRAMPARAMETERIPROC                                  glProgramParameteri;          // aliases glProgramParameteriEXT
  PFNGLPROGRAMUNIFORM1FPROC                                   glProgramUniform1f;           // aliases glProgramUniform1fEXT
  PFNGLPROGRAMUNIFORM1FVPROC                                  glProgramUniform1fv;          // aliases glProgramUniform1fvEXT
  PFNGLPROGRAMUNIFORM1IPROC                                   glProgramUniform1i;           // aliases glProgramUniform1iEXT
  PFNGLPROGRAMUNIFORM1IVPROC                                  glProgramUniform1iv;          // aliases glProgramUniform1ivEXT
  PFNGLPROGRAMUNIFORM1UIPROC                                  glProgramUniform1ui;          // aliases glProgramUniform1uiEXT
  PFNGLPROGRAMUNIFORM1UIVPROC                                 glProgramUniform1uiv;         // aliases glProgramUniform1uivEXT
  PFNGLPROGRAMUNIFORM2FPROC                                   glProgramUniform2f;           // aliases glProgramUniform2fEXT
  PFNGLPROGRAMUNIFORM2FVPROC                                  glProgramUniform2fv;          // aliases glProgramUniform2fvEXT
  PFNGLPROGRAMUNIFORM2IPROC                                   glProgramUniform2i;           // aliases glProgramUniform2iEXT
  PFNGLPROGRAMUNIFORM2IVPROC                                  glProgramUniform2iv;          // aliases glProgramUniform2ivEXT
  PFNGLPROGRAMUNIFORM2UIPROC                                  glProgramUniform2ui;          // aliases glProgramUniform2uiEXT
  PFNGLPROGRAMUNIFORM2UIVPROC                                 glProgramUniform2uiv;         // aliases glProgramUniform2uivEXT
  PFNGLPROGRAMUNIFORM3FPROC                                   glProgramUniform3f;           // aliases glProgramUniform3fEXT
  PFNGLPROGRAMUNIFORM3FVPROC                                  glProgramUniform3fv;          // aliases glProgramUniform3fvEXT
  PFNGLPROGRAMUNIFORM3IPROC                                   glProgramUniform3i;           // aliases glProgramUniform3iEXT
  PFNGLPROGRAMUNIFORM3IVPROC                                  glProgramUniform3iv;          // aliases glProgramUniform3ivEXT
  PFNGLPROGRAMUNIFORM3UIPROC                                  glProgramUniform3ui;          // aliases glProgramUniform3uiEXT
  PFNGLPROGRAMUNIFORM3UIVPROC                                 glProgramUniform3uiv;         // aliases glProgramUniform3uivEXT
  PFNGLPROGRAMUNIFORM4FPROC                                   glProgramUniform4f;           // aliases glProgramUniform4fEXT
  PFNGLPROGRAMUNIFORM4FVPROC                                  glProgramUniform4fv;          // aliases glProgramUniform4fvEXT
  PFNGLPROGRAMUNIFORM4IPROC                                   glProgramUniform4i;           // aliases glProgramUniform4iEXT
  PFNGLPROGRAMUNIFORM4IVPROC                                  glProgramUniform4iv;          // aliases glProgramUniform4ivEXT
  PFNGLPROGRAMUNIFORM4UIPROC                                  glProgramUniform4ui;          // aliases glProgramUniform4uiEXT
  PFNGLPROGRAMUNIFORM4UIVPROC                                 glProgramUniform4uiv;         // aliases glProgramUniform4uivEXT
  PFNGLPROGRAMUNIFORMMATRIX2FVPROC                            glProgramUniformMatrix2fv;    // aliases glProgramUniformMatrix2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC                          glProgramUniformMatrix2x3fv;  // aliases glProgramUniformMatrix2x3fvEXT
  PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC                          glProgramUniformMatrix2x4fv;  // aliases glProgramUniformMatrix2x4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3FVPROC                            glProgramUniformMatrix3fv;    // aliases glProgramUniformMatrix3fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC                          glProgramUniformMatrix3x2fv;  // aliases glProgramUniformMatrix3x2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC                          glProgramUniformMatrix3x4fv;  // aliases glProgramUniformMatrix3x4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4FVPROC                            glProgramUniformMatrix4fv;    // aliases glProgramUniformMatrix4fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC                          glProgramUniformMatrix4x2fv;  // aliases glProgramUniformMatrix4x2fvEXT
  PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC                          glProgramUniformMatrix4x3fv;  // aliases glProgramUniformMatrix4x3fvEXT
  PFNGLPUSHDEBUGGROUPPROC                                     glPushDebugGroup;             // aliases glPushDebugGroupKHR
  PFNGLREADBUFFERPROC                                         glReadBuffer;                 // aliases glReadBufferNV
  PFNGLREADNPIXELSPROC                                        glReadnPixels;                // aliases glReadnPixelsEXT, glReadnPixelsKHR
  PFNGLREADPIXELSPROC                                         glReadPixels;                 
  PFNGLRELEASESHADERCOMPILERPROC                              glReleaseShaderCompiler;      
  PFNGLRENDERBUFFERSTORAGEPROC                                glRenderbufferStorage;        
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC                     glRenderbufferStorageMultisample;// aliases glRenderbufferStorageMultisampleANGLE, glRenderbufferStorageMultisampleAPPLE, glRenderbufferStorageMultisampleEXT, glRenderbufferStorageMultisampleIMG, glRenderbufferStorageMultisampleNV
  PFNGLRESUMETRANSFORMFEEDBACKPROC                            glResumeTransformFeedback;    
  PFNGLSAMPLECOVERAGEPROC                                     glSampleCoverage;             
  PFNGLSAMPLEMASKIPROC                                        glSampleMaski;                
  PFNGLSAMPLERPARAMETERFPROC                                  glSamplerParameterf;          
  PFNGLSAMPLERPARAMETERFVPROC                                 glSamplerParameterfv;         
  PFNGLSAMPLERPARAMETERIPROC                                  glSamplerParameteri;          
  PFNGLSAMPLERPARAMETERIIVPROC                                glSamplerParameterIiv;        // aliases glSamplerParameterIivEXT, glSamplerParameterIivOES
  PFNGLSAMPLERPARAMETERIUIVPROC                               glSamplerParameterIuiv;       // aliases glSamplerParameterIuivEXT, glSamplerParameterIuivOES
  PFNGLSAMPLERPARAMETERIVPROC                                 glSamplerParameteriv;         
  PFNGLSCISSORPROC                                            glScissor;                    
  PFNGLSHADERBINARYPROC                                       glShaderBinary;               
  PFNGLSHADERSOURCEPROC                                       glShaderSource;               
  PFNGLSTENCILFUNCPROC                                        glStencilFunc;                
  PFNGLSTENCILFUNCSEPARATEPROC                                glStencilFuncSeparate;        
  PFNGLSTENCILMASKPROC                                        glStencilMask;                
  PFNGLSTENCILMASKSEPARATEPROC                                glStencilMaskSeparate;        
  PFNGLSTENCILOPPROC                                          glStencilOp;                  
  PFNGLSTENCILOPSEPARATEPROC                                  glStencilOpSeparate;          
  PFNGLTEXBUFFERPROC                                          glTexBuffer;                  // aliases glTexBufferEXT, glTexBufferOES
  PFNGLTEXBUFFERRANGEPROC                                     glTexBufferRange;             // aliases glTexBufferRangeEXT, glTexBufferRangeOES
  PFNGLTEXIMAGE2DPROC                                         glTexImage2D;                 
  PFNGLTEXIMAGE3DPROC                                         glTexImage3D;                 // aliases glTexImage3DOES
  PFNGLTEXPARAMETERFPROC                                      glTexParameterf;              
  PFNGLTEXPARAMETERFVPROC                                     glTexParameterfv;             
  PFNGLTEXPARAMETERIPROC                                      glTexParameteri;              
  PFNGLTEXPARAMETERIIVPROC                                    glTexParameterIiv;            // aliases glTexParameterIivEXT, glTexParameterIivOES
  PFNGLTEXPARAMETERIUIVPROC                                   glTexParameterIuiv;           // aliases glTexParameterIuivEXT, glTexParameterIuivOES
  PFNGLTEXPARAMETERIVPROC                                     glTexParameteriv;             
  PFNGLTEXSTORAGE2DPROC                                       glTexStorage2D;               // aliases glTexStorage2DEXT
  PFNGLTEXSTORAGE2DMULTISAMPLEPROC                            glTexStorage2DMultisample;    
  PFNGLTEXSTORAGE3DPROC                                       glTexStorage3D;               // aliases glTexStorage3DEXT
  PFNGLTEXSTORAGE3DMULTISAMPLEPROC                            glTexStorage3DMultisample;    // aliases glTexStorage3DMultisampleOES
  PFNGLTEXSUBIMAGE2DPROC                                      glTexSubImage2D;              
  PFNGLTEXSUBIMAGE3DPROC                                      glTexSubImage3D;              // aliases glTexSubImage3DOES
  PFNGLTRANSFORMFEEDBACKVARYINGSPROC                          glTransformFeedbackVaryings;  
  PFNGLUNIFORM1FPROC                                          glUniform1f;                  
  PFNGLUNIFORM1FVPROC                                         glUniform1fv;                 
  PFNGLUNIFORM1IPROC                                          glUniform1i;                  
  PFNGLUNIFORM1IVPROC                                         glUniform1iv;                 
  PFNGLUNIFORM1UIPROC                                         glUniform1ui;                 
  PFNGLUNIFORM1UIVPROC                                        glUniform1uiv;                
  PFNGLUNIFORM2FPROC                                          glUniform2f;                  
  PFNGLUNIFORM2FVPROC                                         glUniform2fv;                 
  PFNGLUNIFORM2IPROC                                          glUniform2i;                  
  PFNGLUNIFORM2IVPROC                                         glUniform2iv;                 
  PFNGLUNIFORM2UIPROC                                         glUniform2ui;                 
  PFNGLUNIFORM2UIVPROC                                        glUniform2uiv;                
  PFNGLUNIFORM3FPROC                                          glUniform3f;                  
  PFNGLUNIFORM3FVPROC                                         glUniform3fv;                 
  PFNGLUNIFORM3IPROC                                          glUniform3i;                  
  PFNGLUNIFORM3IVPROC                                         glUniform3iv;                 
  PFNGLUNIFORM3UIPROC                                         glUniform3ui;                 
  PFNGLUNIFORM3UIVPROC                                        glUniform3uiv;                
  PFNGLUNIFORM4FPROC                                          glUniform4f;                  
  PFNGLUNIFORM4FVPROC                                         glUniform4fv;                 
  PFNGLUNIFORM4IPROC                                          glUniform4i;                  
  PFNGLUNIFORM4IVPROC                                         glUniform4iv;                 
  PFNGLUNIFORM4UIPROC                                         glUniform4ui;                 
  PFNGLUNIFORM4UIVPROC                                        glUniform4uiv;                
  PFNGLUNIFORMBLOCKBINDINGPROC                                glUniformBlockBinding;        
  PFNGLUNIFORMMATRIX2FVPROC                                   glUniformMatrix2fv;           
  PFNGLUNIFORMMATRIX2X3FVPROC                                 glUniformMatrix2x3fv;         // aliases glUniformMatrix2x3fvNV
  PFNGLUNIFORMMATRIX2X4FVPROC                                 glUniformMatrix2x4fv;         // aliases glUniformMatrix2x4fvNV
  PFNGLUNIFORMMATRIX3FVPROC                                   glUniformMatrix3fv;           
  PFNGLUNIFORMMATRIX3X2FVPROC                                 glUniformMatrix3x2fv;         // aliases glUniformMatrix3x2fvNV
  PFNGLUNIFORMMATRIX3X4FVPROC                                 glUniformMatrix3x4fv;         // aliases glUniformMatrix3x4fvNV
  PFNGLUNIFORMMATRIX4FVPROC                                   glUniformMatrix4fv;           
  PFNGLUNIFORMMATRIX4X2FVPROC                                 glUniformMatrix4x2fv;         // aliases glUniformMatrix4x2fvNV
  PFNGLUNIFORMMATRIX4X3FVPROC                                 glUniformMatrix4x3fv;         // aliases glUniformMatrix4x3fvNV
  PFNGLUNMAPBUFFERPROC                                        glUnmapBuffer;                // aliases glUnmapBufferOES
  PFNGLUSEPROGRAMPROC                                         glUseProgram;                 
  PFNGLUSEPROGRAMSTAGESPROC                                   glUseProgramStages;           // aliases glUseProgramStagesEXT
  PFNGLVALIDATEPROGRAMPROC                                    glValidateProgram;            
  PFNGLVALIDATEPROGRAMPIPELINEPROC                            glValidateProgramPipeline;    // aliases glValidateProgramPipelineEXT
  PFNGLVERTEXATTRIB1FPROC                                     glVertexAttrib1f;             
  PFNGLVERTEXATTRIB1FVPROC                                    glVertexAttrib1fv;            
  PFNGLVERTEXATTRIB2FPROC                                     glVertexAttrib2f;             
  PFNGLVERTEXATTRIB2FVPROC                                    glVertexAttrib2fv;            
  PFNGLVERTEXATTRIB3FPROC                                     glVertexAttrib3f;             
  PFNGLVERTEXATTRIB3FVPROC                                    glVertexAttrib3fv;            
  PFNGLVERTEXATTRIB4FPROC                                     glVertexAttrib4f;             
  PFNGLVERTEXATTRIB4FVPROC                                    glVertexAttrib4fv;            
  PFNGLVERTEXATTRIBBINDINGPROC                                glVertexAttribBinding;        
  PFNGLVERTEXATTRIBDIVISORPROC                                glVertexAttribDivisor;        // aliases glVertexAttribDivisorANGLE, glVertexAttribDivisorEXT, glVertexAttribDivisorNV
  PFNGLVERTEXATTRIBFORMATPROC                                 glVertexAttribFormat;         
  PFNGLVERTEXATTRIBI4IPROC                                    glVertexAttribI4i;            
  PFNGLVERTEXATTRIBI4IVPROC                                   glVertexAttribI4iv;           
  PFNGLVERTEXATTRIBI4UIPROC                                   glVertexAttribI4ui;           
  PFNGLVERTEXATTRIBI4UIVPROC                                  glVertexAttribI4uiv;          
  PFNGLVERTEXATTRIBIFORMATPROC                                glVertexAttribIFormat;        
  PFNGLVERTEXATTRIBIPOINTERPROC                               glVertexAttribIPointer;       
  PFNGLVERTEXATTRIBPOINTERPROC                                glVertexAttribPointer;        
  PFNGLVERTEXBINDINGDIVISORPROC                               glVertexBindingDivisor;       
  PFNGLVIEWPORTPROC                                           glViewport;                   
  PFNGLWAITSYNCPROC                                           glWaitSync;                   // aliases glWaitSyncAPPLE
  // --

  // ++ glext
//PFNGLACTIVESHADERPROGRAMEXTPROC                             glActiveShaderProgramEXT;     
  PFNGLALPHAFUNCQCOMPROC                                      glAlphaFuncQCOM;              
  PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC                glApplyFramebufferAttachmentCMAAINTEL;
  PFNGLBEGINCONDITIONALRENDERNVPROC                           glBeginConditionalRenderNV;   
  PFNGLBEGINPERFMONITORAMDPROC                                glBeginPerfMonitorAMD;        
  PFNGLBEGINPERFQUERYINTELPROC                                glBeginPerfQueryINTEL;        
//PFNGLBEGINQUERYEXTPROC                                      glBeginQueryEXT;              
  PFNGLBINDFRAGDATALOCATIONEXTPROC                            glBindFragDataLocationEXT;    
  PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC                     glBindFragDataLocationIndexedEXT;
//PFNGLBINDPROGRAMPIPELINEEXTPROC                             glBindProgramPipelineEXT;     
//PFNGLBINDVERTEXARRAYOESPROC                                 glBindVertexArrayOES;         
//PFNGLBLENDBARRIERKHRPROC                                    glBlendBarrierKHR;            
//PFNGLBLENDBARRIERNVPROC                                     glBlendBarrierNV;             
//PFNGLBLENDEQUATIONIEXTPROC                                  glBlendEquationiEXT;          
//PFNGLBLENDEQUATIONIOESPROC                                  glBlendEquationiOES;          
//PFNGLBLENDEQUATIONSEPARATEIEXTPROC                          glBlendEquationSeparateiEXT;  
//PFNGLBLENDEQUATIONSEPARATEIOESPROC                          glBlendEquationSeparateiOES;  
//PFNGLBLENDFUNCIEXTPROC                                      glBlendFunciEXT;              
//PFNGLBLENDFUNCIOESPROC                                      glBlendFunciOES;              
//PFNGLBLENDFUNCSEPARATEIEXTPROC                              glBlendFuncSeparateiEXT;      
//PFNGLBLENDFUNCSEPARATEIOESPROC                              glBlendFuncSeparateiOES;      
  PFNGLBLENDPARAMETERINVPROC                                  glBlendParameteriNV;          
//PFNGLBLITFRAMEBUFFERANGLEPROC                               glBlitFramebufferANGLE;       
//PFNGLBLITFRAMEBUFFERNVPROC                                  glBlitFramebufferNV;          
  PFNGLBUFFERSTORAGEEXTPROC                                   glBufferStorageEXT;           
  PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC                        glClearPixelLocalStorageuiEXT;
  PFNGLCLEARTEXIMAGEEXTPROC                                   glClearTexImageEXT;           
  PFNGLCLEARTEXSUBIMAGEEXTPROC                                glClearTexSubImageEXT;        
//PFNGLCLIENTWAITSYNCAPPLEPROC                                glClientWaitSyncAPPLE;        
//PFNGLCOLORMASKIEXTPROC                                      glColorMaskiEXT;              
//PFNGLCOLORMASKIOESPROC                                      glColorMaskiOES;              
//PFNGLCOMPRESSEDTEXIMAGE3DOESPROC                            glCompressedTexImage3DOES;    
//PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC                         glCompressedTexSubImage3DOES; 
  PFNGLCONSERVATIVERASTERPARAMETERINVPROC                     glConservativeRasterParameteriNV;
//PFNGLCOPYBUFFERSUBDATANVPROC                                glCopyBufferSubDataNV;        
//PFNGLCOPYIMAGESUBDATAEXTPROC                                glCopyImageSubDataEXT;        
//PFNGLCOPYIMAGESUBDATAOESPROC                                glCopyImageSubDataOES;        
  PFNGLCOPYPATHNVPROC                                         glCopyPathNV;                 
//PFNGLCOPYTEXSUBIMAGE3DOESPROC                               glCopyTexSubImage3DOES;       
  PFNGLCOPYTEXTURELEVELSAPPLEPROC                             glCopyTextureLevelsAPPLE;     
  PFNGLCOVERAGEMASKNVPROC                                     glCoverageMaskNV;             
  PFNGLCOVERAGEMODULATIONNVPROC                               glCoverageModulationNV;       
  PFNGLCOVERAGEMODULATIONTABLENVPROC                          glCoverageModulationTableNV;  
  PFNGLCOVERAGEOPERATIONNVPROC                                glCoverageOperationNV;        
  PFNGLCOVERFILLPATHINSTANCEDNVPROC                           glCoverFillPathInstancedNV;   
  PFNGLCOVERFILLPATHNVPROC                                    glCoverFillPathNV;            
  PFNGLCOVERSTROKEPATHINSTANCEDNVPROC                         glCoverStrokePathInstancedNV; 
  PFNGLCOVERSTROKEPATHNVPROC                                  glCoverStrokePathNV;          
  PFNGLCREATEPERFQUERYINTELPROC                               glCreatePerfQueryINTEL;       
//PFNGLCREATESHADERPROGRAMVEXTPROC                            glCreateShaderProgramvEXT;    
//PFNGLDEBUGMESSAGECALLBACKKHRPROC                            glDebugMessageCallbackKHR;    
//PFNGLDEBUGMESSAGECONTROLKHRPROC                             glDebugMessageControlKHR;     
//PFNGLDEBUGMESSAGEINSERTKHRPROC                              glDebugMessageInsertKHR;      
  PFNGLDELETEFENCESNVPROC                                     glDeleteFencesNV;             
  PFNGLDELETEPATHSNVPROC                                      glDeletePathsNV;              
  PFNGLDELETEPERFMONITORSAMDPROC                              glDeletePerfMonitorsAMD;      
  PFNGLDELETEPERFQUERYINTELPROC                               glDeletePerfQueryINTEL;       
//PFNGLDELETEPROGRAMPIPELINESEXTPROC                          glDeleteProgramPipelinesEXT;  
//PFNGLDELETEQUERIESEXTPROC                                   glDeleteQueriesEXT;           
//PFNGLDELETESYNCAPPLEPROC                                    glDeleteSyncAPPLE;            
//PFNGLDELETEVERTEXARRAYSOESPROC                              glDeleteVertexArraysOES;      
  PFNGLDEPTHRANGEARRAYFVNVPROC                                glDepthRangeArrayfvNV;        
  PFNGLDEPTHRANGEARRAYFVOESPROC                               glDepthRangeArrayfvOES;       
  PFNGLDEPTHRANGEINDEXEDFNVPROC                               glDepthRangeIndexedfNV;       
  PFNGLDEPTHRANGEINDEXEDFOESPROC                              glDepthRangeIndexedfOES;      
  PFNGLDISABLEDRIVERCONTROLQCOMPROC                           glDisableDriverControlQCOM;   
//PFNGLDISABLEIEXTPROC                                        glDisableiEXT;                
//PFNGLDISABLEINVPROC                                         glDisableiNV;                 
//PFNGLDISABLEIOESPROC                                        glDisableiOES;                
  PFNGLDISCARDFRAMEBUFFEREXTPROC                              glDiscardFramebufferEXT;      
//PFNGLDRAWARRAYSINSTANCEDANGLEPROC                           glDrawArraysInstancedANGLE;   
  PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC                 glDrawArraysInstancedBaseInstanceEXT;
//PFNGLDRAWARRAYSINSTANCEDEXTPROC                             glDrawArraysInstancedEXT;     
//PFNGLDRAWARRAYSINSTANCEDNVPROC                              glDrawArraysInstancedNV;      
//PFNGLDRAWBUFFERSEXTPROC                                     glDrawBuffersEXT;             
  PFNGLDRAWBUFFERSINDEXEDEXTPROC                              glDrawBuffersIndexedEXT;      
//PFNGLDRAWBUFFERSNVPROC                                      glDrawBuffersNV;              
//PFNGLDRAWELEMENTSBASEVERTEXEXTPROC                          glDrawElementsBaseVertexEXT;  
//PFNGLDRAWELEMENTSBASEVERTEXOESPROC                          glDrawElementsBaseVertexOES;  
//PFNGLDRAWELEMENTSINSTANCEDANGLEPROC                         glDrawElementsInstancedANGLE; 
  PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC               glDrawElementsInstancedBaseInstanceEXT;
  PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC     glDrawElementsInstancedBaseVertexBaseInstanceEXT;
//PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC                 glDrawElementsInstancedBaseVertexEXT;
//PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC                 glDrawElementsInstancedBaseVertexOES;
//PFNGLDRAWELEMENTSINSTANCEDEXTPROC                           glDrawElementsInstancedEXT;   
//PFNGLDRAWELEMENTSINSTANCEDNVPROC                            glDrawElementsInstancedNV;    
//PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC                     glDrawRangeElementsBaseVertexEXT;
//PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC                     glDrawRangeElementsBaseVertexOES;
  PFNGLDRAWTRANSFORMFEEDBACKEXTPROC                           glDrawTransformFeedbackEXT;   
  PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDEXTPROC                  glDrawTransformFeedbackInstancedEXT;
  PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC               glEGLImageTargetRenderbufferStorageOES;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC                         glEGLImageTargetTexture2DOES; 
  PFNGLENABLEDRIVERCONTROLQCOMPROC                            glEnableDriverControlQCOM;    
//PFNGLENABLEIEXTPROC                                         glEnableiEXT;                 
//PFNGLENABLEINVPROC                                          glEnableiNV;                  
//PFNGLENABLEIOESPROC                                         glEnableiOES;                 
  PFNGLENDCONDITIONALRENDERNVPROC                             glEndConditionalRenderNV;     
  PFNGLENDPERFMONITORAMDPROC                                  glEndPerfMonitorAMD;          
  PFNGLENDPERFQUERYINTELPROC                                  glEndPerfQueryINTEL;          
//PFNGLENDQUERYEXTPROC                                        glEndQueryEXT;                
  PFNGLENDTILINGQCOMPROC                                      glEndTilingQCOM;              
  PFNGLEXTGETBUFFERPOINTERVQCOMPROC                           glExtGetBufferPointervQCOM;   
  PFNGLEXTGETBUFFERSQCOMPROC                                  glExtGetBuffersQCOM;          
  PFNGLEXTGETFRAMEBUFFERSQCOMPROC                             glExtGetFramebuffersQCOM;     
  PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC                      glExtGetProgramBinarySourceQCOM;
  PFNGLEXTGETPROGRAMSQCOMPROC                                 glExtGetProgramsQCOM;         
  PFNGLEXTGETRENDERBUFFERSQCOMPROC                            glExtGetRenderbuffersQCOM;    
  PFNGLEXTGETSHADERSQCOMPROC                                  glExtGetShadersQCOM;          
  PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC                      glExtGetTexLevelParameterivQCOM;
  PFNGLEXTGETTEXSUBIMAGEQCOMPROC                              glExtGetTexSubImageQCOM;      
  PFNGLEXTGETTEXTURESQCOMPROC                                 glExtGetTexturesQCOM;         
  PFNGLEXTISPROGRAMBINARYQCOMPROC                             glExtIsProgramBinaryQCOM;     
  PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC                     glExtTexObjectStateOverrideiQCOM;
//PFNGLFENCESYNCAPPLEPROC                                     glFenceSyncAPPLE;             
  PFNGLFINISHFENCENVPROC                                      glFinishFenceNV;              
//PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC                          glFlushMappedBufferRangeEXT;  
  PFNGLFRAGMENTCOVERAGECOLORNVPROC                            glFragmentCoverageColorNV;    
  PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC                glFramebufferPixelLocalStorageSizeEXT;
  PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC                     glFramebufferSampleLocationsfvNV;
  PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC                  glFramebufferTexture2DDownsampleIMG;
  PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC                 glFramebufferTexture2DMultisampleEXT;
  PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC                 glFramebufferTexture2DMultisampleIMG;
  PFNGLFRAMEBUFFERTEXTURE3DOESPROC                            glFramebufferTexture3DOES;    
//PFNGLFRAMEBUFFERTEXTUREEXTPROC                              glFramebufferTextureEXT;      
  PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC               glFramebufferTextureLayerDownsampleIMG;
  PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC          glFramebufferTextureMultisampleMultiviewOVR;
  PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC                     glFramebufferTextureMultiviewOVR;
//PFNGLFRAMEBUFFERTEXTUREOESPROC                              glFramebufferTextureOES;      
  PFNGLGENFENCESNVPROC                                        glGenFencesNV;                
  PFNGLGENPATHSNVPROC                                         glGenPathsNV;                 
  PFNGLGENPERFMONITORSAMDPROC                                 glGenPerfMonitorsAMD;         
//PFNGLGENPROGRAMPIPELINESEXTPROC                             glGenProgramPipelinesEXT;     
//PFNGLGENQUERIESEXTPROC                                      glGenQueriesEXT;              
//PFNGLGENVERTEXARRAYSOESPROC                                 glGenVertexArraysOES;         
//PFNGLGETBUFFERPOINTERVOESPROC                               glGetBufferPointervOES;       
  PFNGLGETCOVERAGEMODULATIONTABLENVPROC                       glGetCoverageModulationTableNV;
//PFNGLGETDEBUGMESSAGELOGKHRPROC                              glGetDebugMessageLogKHR;      
  PFNGLGETDRIVERCONTROLSQCOMPROC                              glGetDriverControlsQCOM;      
  PFNGLGETDRIVERCONTROLSTRINGQCOMPROC                         glGetDriverControlStringQCOM; 
  PFNGLGETFENCEIVNVPROC                                       glGetFenceivNV;               
  PFNGLGETFIRSTPERFQUERYIDINTELPROC                           glGetFirstPerfQueryIdINTEL;   
  PFNGLGETFLOATI_VNVPROC                                      glGetFloati_vNV;              
  PFNGLGETFLOATI_VOESPROC                                     glGetFloati_vOES;             
  PFNGLGETFRAGDATAINDEXEXTPROC                                glGetFragDataIndexEXT;        
  PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC             glGetFramebufferPixelLocalStorageSizeEXT;
//PFNGLGETGRAPHICSRESETSTATUSEXTPROC                          glGetGraphicsResetStatusEXT;  
//PFNGLGETGRAPHICSRESETSTATUSKHRPROC                          glGetGraphicsResetStatusKHR;  
  PFNGLGETIMAGEHANDLENVPROC                                   glGetImageHandleNV;           
//PFNGLGETINTEGER64VAPPLEPROC                                 glGetInteger64vAPPLE;         
//PFNGLGETINTEGERI_VEXTPROC                                   glGetIntegeri_vEXT;           
  PFNGLGETINTERNALFORMATSAMPLEIVNVPROC                        glGetInternalformatSampleivNV;
  PFNGLGETNEXTPERFQUERYIDINTELPROC                            glGetNextPerfQueryIdINTEL;    
//PFNGLGETNUNIFORMFVEXTPROC                                   glGetnUniformfvEXT;           
//PFNGLGETNUNIFORMFVKHRPROC                                   glGetnUniformfvKHR;           
//PFNGLGETNUNIFORMIVEXTPROC                                   glGetnUniformivEXT;           
//PFNGLGETNUNIFORMIVKHRPROC                                   glGetnUniformivKHR;           
//PFNGLGETNUNIFORMUIVKHRPROC                                  glGetnUniformuivKHR;          
//PFNGLGETOBJECTLABELEXTPROC                                  glGetObjectLabelEXT;          
//PFNGLGETOBJECTLABELKHRPROC                                  glGetObjectLabelKHR;          
//PFNGLGETOBJECTPTRLABELKHRPROC                               glGetObjectPtrLabelKHR;       
  PFNGLGETPATHCOMMANDSNVPROC                                  glGetPathCommandsNV;          
  PFNGLGETPATHCOORDSNVPROC                                    glGetPathCoordsNV;            
  PFNGLGETPATHDASHARRAYNVPROC                                 glGetPathDashArrayNV;         
  PFNGLGETPATHLENGTHNVPROC                                    glGetPathLengthNV;            
  PFNGLGETPATHMETRICRANGENVPROC                               glGetPathMetricRangeNV;       
  PFNGLGETPATHMETRICSNVPROC                                   glGetPathMetricsNV;           
  PFNGLGETPATHPARAMETERFVNVPROC                               glGetPathParameterfvNV;       
  PFNGLGETPATHPARAMETERIVNVPROC                               glGetPathParameterivNV;       
  PFNGLGETPATHSPACINGNVPROC                                   glGetPathSpacingNV;           
  PFNGLGETPERFCOUNTERINFOINTELPROC                            glGetPerfCounterInfoINTEL;    
  PFNGLGETPERFMONITORCOUNTERDATAAMDPROC                       glGetPerfMonitorCounterDataAMD;
  PFNGLGETPERFMONITORCOUNTERINFOAMDPROC                       glGetPerfMonitorCounterInfoAMD;
  PFNGLGETPERFMONITORCOUNTERSAMDPROC                          glGetPerfMonitorCountersAMD;  
  PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC                     glGetPerfMonitorCounterStringAMD;
  PFNGLGETPERFMONITORGROUPSAMDPROC                            glGetPerfMonitorGroupsAMD;    
  PFNGLGETPERFMONITORGROUPSTRINGAMDPROC                       glGetPerfMonitorGroupStringAMD;
  PFNGLGETPERFQUERYDATAINTELPROC                              glGetPerfQueryDataINTEL;      
  PFNGLGETPERFQUERYIDBYNAMEINTELPROC                          glGetPerfQueryIdByNameINTEL;  
  PFNGLGETPERFQUERYINFOINTELPROC                              glGetPerfQueryInfoINTEL;      
//PFNGLGETPOINTERVKHRPROC                                     glGetPointervKHR;             
//PFNGLGETPROGRAMBINARYOESPROC                                glGetProgramBinaryOES;        
//PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC                       glGetProgramPipelineInfoLogEXT;
//PFNGLGETPROGRAMPIPELINEIVEXTPROC                            glGetProgramPipelineivEXT;    
  PFNGLGETPROGRAMRESOURCEFVNVPROC                             glGetProgramResourcefvNV;     
  PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC                 glGetProgramResourceLocationIndexEXT;
//PFNGLGETQUERYIVEXTPROC                                      glGetQueryivEXT;              
  PFNGLGETQUERYOBJECTI64VEXTPROC                              glGetQueryObjecti64vEXT;      
  PFNGLGETQUERYOBJECTIVEXTPROC                                glGetQueryObjectivEXT;        
  PFNGLGETQUERYOBJECTUI64VEXTPROC                             glGetQueryObjectui64vEXT;     
//PFNGLGETQUERYOBJECTUIVEXTPROC                               glGetQueryObjectuivEXT;       
//PFNGLGETSAMPLERPARAMETERIIVEXTPROC                          glGetSamplerParameterIivEXT;  
//PFNGLGETSAMPLERPARAMETERIIVOESPROC                          glGetSamplerParameterIivOES;  
//PFNGLGETSAMPLERPARAMETERIUIVEXTPROC                         glGetSamplerParameterIuivEXT; 
//PFNGLGETSAMPLERPARAMETERIUIVOESPROC                         glGetSamplerParameterIuivOES; 
//PFNGLGETSYNCIVAPPLEPROC                                     glGetSyncivAPPLE;             
//PFNGLGETTEXPARAMETERIIVEXTPROC                              glGetTexParameterIivEXT;      
//PFNGLGETTEXPARAMETERIIVOESPROC                              glGetTexParameterIivOES;      
//PFNGLGETTEXPARAMETERIUIVEXTPROC                             glGetTexParameterIuivEXT;     
//PFNGLGETTEXPARAMETERIUIVOESPROC                             glGetTexParameterIuivOES;     
  PFNGLGETTEXTUREHANDLEIMGPROC                                glGetTextureHandleIMG;        
  PFNGLGETTEXTUREHANDLENVPROC                                 glGetTextureHandleNV;         
  PFNGLGETTEXTURESAMPLERHANDLEIMGPROC                         glGetTextureSamplerHandleIMG; 
  PFNGLGETTEXTURESAMPLERHANDLENVPROC                          glGetTextureSamplerHandleNV;  
  PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC                     glGetTranslatedShaderSourceANGLE;
  PFNGLGETUNIFORMI64VNVPROC                                   glGetUniformi64vNV;           
  PFNGLINSERTEVENTMARKEREXTPROC                               glInsertEventMarkerEXT;       
  PFNGLINTERPOLATEPATHSNVPROC                                 glInterpolatePathsNV;         
//PFNGLISENABLEDIEXTPROC                                      glIsEnablediEXT;              
//PFNGLISENABLEDINVPROC                                       glIsEnablediNV;               
//PFNGLISENABLEDIOESPROC                                      glIsEnablediOES;              
  PFNGLISFENCENVPROC                                          glIsFenceNV;                  
  PFNGLISIMAGEHANDLERESIDENTNVPROC                            glIsImageHandleResidentNV;    
  PFNGLISPATHNVPROC                                           glIsPathNV;                   
  PFNGLISPOINTINFILLPATHNVPROC                                glIsPointInFillPathNV;        
  PFNGLISPOINTINSTROKEPATHNVPROC                              glIsPointInStrokePathNV;      
//PFNGLISPROGRAMPIPELINEEXTPROC                               glIsProgramPipelineEXT;       
//PFNGLISQUERYEXTPROC                                         glIsQueryEXT;                 
//PFNGLISSYNCAPPLEPROC                                        glIsSyncAPPLE;                
  PFNGLISTEXTUREHANDLERESIDENTNVPROC                          glIsTextureHandleResidentNV;  
//PFNGLISVERTEXARRAYOESPROC                                   glIsVertexArrayOES;           
  PFNGLLABELOBJECTEXTPROC                                     glLabelObjectEXT;             
  PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC                       glMakeImageHandleNonResidentNV;
  PFNGLMAKEIMAGEHANDLERESIDENTNVPROC                          glMakeImageHandleResidentNV;  
  PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC                     glMakeTextureHandleNonResidentNV;
  PFNGLMAKETEXTUREHANDLERESIDENTNVPROC                        glMakeTextureHandleResidentNV;
  PFNGLMAPBUFFEROESPROC                                       glMapBufferOES;               
//PFNGLMAPBUFFERRANGEEXTPROC                                  glMapBufferRangeEXT;          
  PFNGLMATRIXLOAD3X2FNVPROC                                   glMatrixLoad3x2fNV;           
  PFNGLMATRIXLOAD3X3FNVPROC                                   glMatrixLoad3x3fNV;           
  PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC                          glMatrixLoadTranspose3x3fNV;  
  PFNGLMATRIXMULT3X2FNVPROC                                   glMatrixMult3x2fNV;           
  PFNGLMATRIXMULT3X3FNVPROC                                   glMatrixMult3x3fNV;           
  PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC                          glMatrixMultTranspose3x3fNV;  
//PFNGLMINSAMPLESHADINGOESPROC                                glMinSampleShadingOES;        
  PFNGLMULTIDRAWARRAYSEXTPROC                                 glMultiDrawArraysEXT;         
  PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC                         glMultiDrawArraysIndirectEXT; 
  PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC                     glMultiDrawElementsBaseVertexEXT;
  PFNGLMULTIDRAWELEMENTSBASEVERTEXOESPROC                     glMultiDrawElementsBaseVertexOES;
  PFNGLMULTIDRAWELEMENTSEXTPROC                               glMultiDrawElementsEXT;       
  PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC                       glMultiDrawElementsIndirectEXT;
  PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC                glNamedFramebufferSampleLocationsfvNV;
//PFNGLOBJECTLABELKHRPROC                                     glObjectLabelKHR;             
//PFNGLOBJECTPTRLABELKHRPROC                                  glObjectPtrLabelKHR;          
//PFNGLPATCHPARAMETERIEXTPROC                                 glPatchParameteriEXT;         
//PFNGLPATCHPARAMETERIOESPROC                                 glPatchParameteriOES;         
  PFNGLPATHCOMMANDSNVPROC                                     glPathCommandsNV;             
  PFNGLPATHCOORDSNVPROC                                       glPathCoordsNV;               
  PFNGLPATHCOVERDEPTHFUNCNVPROC                               glPathCoverDepthFuncNV;       
  PFNGLPATHDASHARRAYNVPROC                                    glPathDashArrayNV;            
  PFNGLPATHGLYPHINDEXARRAYNVPROC                              glPathGlyphIndexArrayNV;      
  PFNGLPATHGLYPHINDEXRANGENVPROC                              glPathGlyphIndexRangeNV;      
  PFNGLPATHGLYPHRANGENVPROC                                   glPathGlyphRangeNV;           
  PFNGLPATHGLYPHSNVPROC                                       glPathGlyphsNV;               
  PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC                        glPathMemoryGlyphIndexArrayNV;
  PFNGLPATHPARAMETERFNVPROC                                   glPathParameterfNV;           
  PFNGLPATHPARAMETERFVNVPROC                                  glPathParameterfvNV;          
  PFNGLPATHPARAMETERINVPROC                                   glPathParameteriNV;           
  PFNGLPATHPARAMETERIVNVPROC                                  glPathParameterivNV;          
  PFNGLPATHSTENCILDEPTHOFFSETNVPROC                           glPathStencilDepthOffsetNV;   
  PFNGLPATHSTENCILFUNCNVPROC                                  glPathStencilFuncNV;          
  PFNGLPATHSTRINGNVPROC                                       glPathStringNV;               
  PFNGLPATHSUBCOMMANDSNVPROC                                  glPathSubCommandsNV;          
  PFNGLPATHSUBCOORDSNVPROC                                    glPathSubCoordsNV;            
  PFNGLPOINTALONGPATHNVPROC                                   glPointAlongPathNV;           
  PFNGLPOLYGONMODENVPROC                                      glPolygonModeNV;              
  PFNGLPOLYGONOFFSETCLAMPEXTPROC                              glPolygonOffsetClampEXT;      
//PFNGLPOPDEBUGGROUPKHRPROC                                   glPopDebugGroupKHR;           
  PFNGLPOPGROUPMARKEREXTPROC                                  glPopGroupMarkerEXT;          
//PFNGLPRIMITIVEBOUNDINGBOXEXTPROC                            glPrimitiveBoundingBoxEXT;    
//PFNGLPRIMITIVEBOUNDINGBOXOESPROC                            glPrimitiveBoundingBoxOES;    
//PFNGLPROGRAMBINARYOESPROC                                   glProgramBinaryOES;           
//PFNGLPROGRAMPARAMETERIEXTPROC                               glProgramParameteriEXT;       
  PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC                      glProgramPathFragmentInputGenNV;
//PFNGLPROGRAMUNIFORM1FEXTPROC                                glProgramUniform1fEXT;        
//PFNGLPROGRAMUNIFORM1FVEXTPROC                               glProgramUniform1fvEXT;       
  PFNGLPROGRAMUNIFORM1I64NVPROC                               glProgramUniform1i64NV;       
  PFNGLPROGRAMUNIFORM1I64VNVPROC                              glProgramUniform1i64vNV;      
//PFNGLPROGRAMUNIFORM1IEXTPROC                                glProgramUniform1iEXT;        
//PFNGLPROGRAMUNIFORM1IVEXTPROC                               glProgramUniform1ivEXT;       
  PFNGLPROGRAMUNIFORM1UI64NVPROC                              glProgramUniform1ui64NV;      
  PFNGLPROGRAMUNIFORM1UI64VNVPROC                             glProgramUniform1ui64vNV;     
//PFNGLPROGRAMUNIFORM1UIEXTPROC                               glProgramUniform1uiEXT;       
//PFNGLPROGRAMUNIFORM1UIVEXTPROC                              glProgramUniform1uivEXT;      
//PFNGLPROGRAMUNIFORM2FEXTPROC                                glProgramUniform2fEXT;        
//PFNGLPROGRAMUNIFORM2FVEXTPROC                               glProgramUniform2fvEXT;       
  PFNGLPROGRAMUNIFORM2I64NVPROC                               glProgramUniform2i64NV;       
  PFNGLPROGRAMUNIFORM2I64VNVPROC                              glProgramUniform2i64vNV;      
//PFNGLPROGRAMUNIFORM2IEXTPROC                                glProgramUniform2iEXT;        
//PFNGLPROGRAMUNIFORM2IVEXTPROC                               glProgramUniform2ivEXT;       
  PFNGLPROGRAMUNIFORM2UI64NVPROC                              glProgramUniform2ui64NV;      
  PFNGLPROGRAMUNIFORM2UI64VNVPROC                             glProgramUniform2ui64vNV;     
//PFNGLPROGRAMUNIFORM2UIEXTPROC                               glProgramUniform2uiEXT;       
//PFNGLPROGRAMUNIFORM2UIVEXTPROC                              glProgramUniform2uivEXT;      
//PFNGLPROGRAMUNIFORM3FEXTPROC                                glProgramUniform3fEXT;        
//PFNGLPROGRAMUNIFORM3FVEXTPROC                               glProgramUniform3fvEXT;       
  PFNGLPROGRAMUNIFORM3I64NVPROC                               glProgramUniform3i64NV;       
  PFNGLPROGRAMUNIFORM3I64VNVPROC                              glProgramUniform3i64vNV;      
//PFNGLPROGRAMUNIFORM3IEXTPROC                                glProgramUniform3iEXT;        
//PFNGLPROGRAMUNIFORM3IVEXTPROC                               glProgramUniform3ivEXT;       
  PFNGLPROGRAMUNIFORM3UI64NVPROC                              glProgramUniform3ui64NV;      
  PFNGLPROGRAMUNIFORM3UI64VNVPROC                             glProgramUniform3ui64vNV;     
//PFNGLPROGRAMUNIFORM3UIEXTPROC                               glProgramUniform3uiEXT;       
//PFNGLPROGRAMUNIFORM3UIVEXTPROC                              glProgramUniform3uivEXT;      
//PFNGLPROGRAMUNIFORM4FEXTPROC                                glProgramUniform4fEXT;        
//PFNGLPROGRAMUNIFORM4FVEXTPROC                               glProgramUniform4fvEXT;       
  PFNGLPROGRAMUNIFORM4I64NVPROC                               glProgramUniform4i64NV;       
  PFNGLPROGRAMUNIFORM4I64VNVPROC                              glProgramUniform4i64vNV;      
//PFNGLPROGRAMUNIFORM4IEXTPROC                                glProgramUniform4iEXT;        
//PFNGLPROGRAMUNIFORM4IVEXTPROC                               glProgramUniform4ivEXT;       
  PFNGLPROGRAMUNIFORM4UI64NVPROC                              glProgramUniform4ui64NV;      
  PFNGLPROGRAMUNIFORM4UI64VNVPROC                             glProgramUniform4ui64vNV;     
//PFNGLPROGRAMUNIFORM4UIEXTPROC                               glProgramUniform4uiEXT;       
//PFNGLPROGRAMUNIFORM4UIVEXTPROC                              glProgramUniform4uivEXT;      
  PFNGLPROGRAMUNIFORMHANDLEUI64IMGPROC                        glProgramUniformHandleui64IMG;
  PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC                         glProgramUniformHandleui64NV; 
  PFNGLPROGRAMUNIFORMHANDLEUI64VIMGPROC                       glProgramUniformHandleui64vIMG;
  PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC                        glProgramUniformHandleui64vNV;
//PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC                         glProgramUniformMatrix2fvEXT; 
//PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC                       glProgramUniformMatrix2x3fvEXT;
//PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC                       glProgramUniformMatrix2x4fvEXT;
//PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC                         glProgramUniformMatrix3fvEXT; 
//PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC                       glProgramUniformMatrix3x2fvEXT;
//PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC                       glProgramUniformMatrix3x4fvEXT;
//PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC                         glProgramUniformMatrix4fvEXT; 
//PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC                       glProgramUniformMatrix4x2fvEXT;
//PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC                       glProgramUniformMatrix4x3fvEXT;
//PFNGLPUSHDEBUGGROUPKHRPROC                                  glPushDebugGroupKHR;          
  PFNGLPUSHGROUPMARKEREXTPROC                                 glPushGroupMarkerEXT;         
  PFNGLQUERYCOUNTEREXTPROC                                    glQueryCounterEXT;            
  PFNGLRASTERSAMPLESEXTPROC                                   glRasterSamplesEXT;           
  PFNGLREADBUFFERINDEXEDEXTPROC                               glReadBufferIndexedEXT;       
//PFNGLREADBUFFERNVPROC                                       glReadBufferNV;               
//PFNGLREADNPIXELSEXTPROC                                     glReadnPixelsEXT;             
//PFNGLREADNPIXELSKHRPROC                                     glReadnPixelsKHR;             
//PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC                glRenderbufferStorageMultisampleANGLE;
//PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC                glRenderbufferStorageMultisampleAPPLE;
//PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC                  glRenderbufferStorageMultisampleEXT;
//PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC                  glRenderbufferStorageMultisampleIMG;
//PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC                   glRenderbufferStorageMultisampleNV;
  PFNGLRESOLVEDEPTHVALUESNVPROC                               glResolveDepthValuesNV;       
  PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC                 glResolveMultisampleFramebufferAPPLE;
//PFNGLSAMPLERPARAMETERIIVEXTPROC                             glSamplerParameterIivEXT;     
//PFNGLSAMPLERPARAMETERIIVOESPROC                             glSamplerParameterIivOES;     
//PFNGLSAMPLERPARAMETERIUIVEXTPROC                            glSamplerParameterIuivEXT;    
//PFNGLSAMPLERPARAMETERIUIVOESPROC                            glSamplerParameterIuivOES;    
  PFNGLSCISSORARRAYVNVPROC                                    glScissorArrayvNV;            
  PFNGLSCISSORARRAYVOESPROC                                   glScissorArrayvOES;           
  PFNGLSCISSORINDEXEDNVPROC                                   glScissorIndexedNV;           
  PFNGLSCISSORINDEXEDOESPROC                                  glScissorIndexedOES;          
  PFNGLSCISSORINDEXEDVNVPROC                                  glScissorIndexedvNV;          
  PFNGLSCISSORINDEXEDVOESPROC                                 glScissorIndexedvOES;         
  PFNGLSELECTPERFMONITORCOUNTERSAMDPROC                       glSelectPerfMonitorCountersAMD;
  PFNGLSETFENCENVPROC                                         glSetFenceNV;                 
  PFNGLSTARTTILINGQCOMPROC                                    glStartTilingQCOM;            
  PFNGLSTENCILFILLPATHINSTANCEDNVPROC                         glStencilFillPathInstancedNV; 
  PFNGLSTENCILFILLPATHNVPROC                                  glStencilFillPathNV;          
  PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC                       glStencilStrokePathInstancedNV;
  PFNGLSTENCILSTROKEPATHNVPROC                                glStencilStrokePathNV;        
  PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC                glStencilThenCoverFillPathInstancedNV;
  PFNGLSTENCILTHENCOVERFILLPATHNVPROC                         glStencilThenCoverFillPathNV; 
  PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC              glStencilThenCoverStrokePathInstancedNV;
  PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC                       glStencilThenCoverStrokePathNV;
  PFNGLSUBPIXELPRECISIONBIASNVPROC                            glSubpixelPrecisionBiasNV;    
  PFNGLTESTFENCENVPROC                                        glTestFenceNV;                
//PFNGLTEXBUFFEREXTPROC                                       glTexBufferEXT;               
//PFNGLTEXBUFFEROESPROC                                       glTexBufferOES;               
//PFNGLTEXBUFFERRANGEEXTPROC                                  glTexBufferRangeEXT;          
//PFNGLTEXBUFFERRANGEOESPROC                                  glTexBufferRangeOES;          
//PFNGLTEXIMAGE3DOESPROC                                      glTexImage3DOES;              
  PFNGLTEXPAGECOMMITMENTEXTPROC                               glTexPageCommitmentEXT;       
//PFNGLTEXPARAMETERIIVEXTPROC                                 glTexParameterIivEXT;         
//PFNGLTEXPARAMETERIIVOESPROC                                 glTexParameterIivOES;         
//PFNGLTEXPARAMETERIUIVEXTPROC                                glTexParameterIuivEXT;        
//PFNGLTEXPARAMETERIUIVOESPROC                                glTexParameterIuivOES;        
  PFNGLTEXSTORAGE1DEXTPROC                                    glTexStorage1DEXT;            
//PFNGLTEXSTORAGE2DEXTPROC                                    glTexStorage2DEXT;            
//PFNGLTEXSTORAGE3DEXTPROC                                    glTexStorage3DEXT;            
//PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC                         glTexStorage3DMultisampleOES; 
//PFNGLTEXSUBIMAGE3DOESPROC                                   glTexSubImage3DOES;           
  PFNGLTEXTURESTORAGE1DEXTPROC                                glTextureStorage1DEXT;        
  PFNGLTEXTURESTORAGE2DEXTPROC                                glTextureStorage2DEXT;        
  PFNGLTEXTURESTORAGE3DEXTPROC                                glTextureStorage3DEXT;        
  PFNGLTEXTUREVIEWEXTPROC                                     glTextureViewEXT;             
  PFNGLTEXTUREVIEWOESPROC                                     glTextureViewOES;             
  PFNGLTRANSFORMPATHNVPROC                                    glTransformPathNV;            
  PFNGLUNIFORM1I64NVPROC                                      glUniform1i64NV;              
  PFNGLUNIFORM1I64VNVPROC                                     glUniform1i64vNV;             
  PFNGLUNIFORM1UI64NVPROC                                     glUniform1ui64NV;             
  PFNGLUNIFORM1UI64VNVPROC                                    glUniform1ui64vNV;            
  PFNGLUNIFORM2I64NVPROC                                      glUniform2i64NV;              
  PFNGLUNIFORM2I64VNVPROC                                     glUniform2i64vNV;             
  PFNGLUNIFORM2UI64NVPROC                                     glUniform2ui64NV;             
  PFNGLUNIFORM2UI64VNVPROC                                    glUniform2ui64vNV;            
  PFNGLUNIFORM3I64NVPROC                                      glUniform3i64NV;              
  PFNGLUNIFORM3I64VNVPROC                                     glUniform3i64vNV;             
  PFNGLUNIFORM3UI64NVPROC                                     glUniform3ui64NV;             
  PFNGLUNIFORM3UI64VNVPROC                                    glUniform3ui64vNV;            
  PFNGLUNIFORM4I64NVPROC                                      glUniform4i64NV;              
  PFNGLUNIFORM4I64VNVPROC                                     glUniform4i64vNV;             
  PFNGLUNIFORM4UI64NVPROC                                     glUniform4ui64NV;             
  PFNGLUNIFORM4UI64VNVPROC                                    glUniform4ui64vNV;            
  PFNGLUNIFORMHANDLEUI64IMGPROC                               glUniformHandleui64IMG;       
  PFNGLUNIFORMHANDLEUI64NVPROC                                glUniformHandleui64NV;        
  PFNGLUNIFORMHANDLEUI64VIMGPROC                              glUniformHandleui64vIMG;      
  PFNGLUNIFORMHANDLEUI64VNVPROC                               glUniformHandleui64vNV;       
//PFNGLUNIFORMMATRIX2X3FVNVPROC                               glUniformMatrix2x3fvNV;       
//PFNGLUNIFORMMATRIX2X4FVNVPROC                               glUniformMatrix2x4fvNV;       
//PFNGLUNIFORMMATRIX3X2FVNVPROC                               glUniformMatrix3x2fvNV;       
//PFNGLUNIFORMMATRIX3X4FVNVPROC                               glUniformMatrix3x4fvNV;       
//PFNGLUNIFORMMATRIX4X2FVNVPROC                               glUniformMatrix4x2fvNV;       
//PFNGLUNIFORMMATRIX4X3FVNVPROC                               glUniformMatrix4x3fvNV;       
//PFNGLUNMAPBUFFEROESPROC                                     glUnmapBufferOES;             
//PFNGLUSEPROGRAMSTAGESEXTPROC                                glUseProgramStagesEXT;        
//PFNGLVALIDATEPROGRAMPIPELINEEXTPROC                         glValidateProgramPipelineEXT; 
//PFNGLVERTEXATTRIBDIVISORANGLEPROC                           glVertexAttribDivisorANGLE;   
//PFNGLVERTEXATTRIBDIVISOREXTPROC                             glVertexAttribDivisorEXT;     
//PFNGLVERTEXATTRIBDIVISORNVPROC                              glVertexAttribDivisorNV;      
  PFNGLVIEWPORTARRAYVNVPROC                                   glViewportArrayvNV;           
  PFNGLVIEWPORTARRAYVOESPROC                                  glViewportArrayvOES;          
  PFNGLVIEWPORTINDEXEDFNVPROC                                 glViewportIndexedfNV;         
  PFNGLVIEWPORTINDEXEDFOESPROC                                glViewportIndexedfOES;        
  PFNGLVIEWPORTINDEXEDFVNVPROC                                glViewportIndexedfvNV;        
  PFNGLVIEWPORTINDEXEDFVOESPROC                               glViewportIndexedfvOES;       
  PFNGLVIEWPORTSWIZZLENVPROC                                  glViewportSwizzleNV;          
//PFNGLWAITSYNCAPPLEPROC                                      glWaitSyncAPPLE;              
  PFNGLWEIGHTPATHSNVPROC                                      glWeightPathsNV;              
  PFNGLWINDOWRECTANGLESEXTPROC                                glWindowRectanglesEXT;        
  // --
};

