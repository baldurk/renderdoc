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
    HookInit(glTexParameteri); \
    HookInit(glViewport); \
    HookInit(glLightfv); \
    HookInit(glMaterialfv); \
    HookInit(glGenLists); \
    HookInit(glNewList); \
    HookInit(glEndList); \
    HookInit(glCallList); \
    HookInit(glShadeModel); \
    HookInit(glBegin); \
    HookInit(glEnd); \
    HookInit(glVertex3f); \
    HookInit(glNormal3f); \
    HookInit(glPushMatrix); \
    HookInit(glPopMatrix); \
    HookInit(glMatrixMode); \
    HookInit(glLoadIdentity); \
    HookInit(glFrustum); \
    HookInit(glTranslatef); \
    HookInit(glRotatef); \



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
    HookExtension(PFNGLTEXSUBIMAGE1DPROC, glTexSubImage1D); \
    HookExtension(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D); \
    HookExtension(PFNGLTEXSUBIMAGE3DPROC, glTexSubImage3D); \
    HookExtension(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap); \
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
    HookExtension(PFNGLCOLORMASKIPROC, glColorMaski); \
    HookExtension(PFNGLDEPTHRANGEPROC, glDepthRange); \
    HookExtension(PFNGLDEPTHRANGEFPROC, glDepthRangef); \
    HookExtension(PFNGLDEPTHRANGEARRAYVPROC, glDepthRangeArrayv); \
    HookExtension(PFNGLDEPTHBOUNDSEXTPROC, glDepthBoundsEXT); \
    HookExtension(PFNGLCREATESHADERPROC, glCreateShader); \
    HookExtension(PFNGLDELETESHADERPROC, glDeleteShader); \
    HookExtension(PFNGLSHADERSOURCEPROC, glShaderSource); \
    HookExtension(PFNGLCOMPILESHADERPROC, glCompileShader); \
    HookExtension(PFNGLGETSHADERIVPROC, glGetShaderiv); \
    HookExtension(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog); \
    HookExtension(PFNGLCREATEPROGRAMPROC, glCreateProgram); \
    HookExtension(PFNGLDELETEPROGRAMPROC, glDeleteProgram); \
    HookExtension(PFNGLATTACHSHADERPROC, glAttachShader); \
    HookExtension(PFNGLLINKPROGRAMPROC, glLinkProgram); \
    HookExtension(PFNGLUSEPROGRAMPROC, glUseProgram); \
    HookExtension(PFNGLGETPROGRAMIVPROC, glGetProgramiv); \
    HookExtension(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog); \
    HookExtension(PFNGLGETPROGRAMINTERFACEIVPROC, glGetProgramInterfaceiv); \
    HookExtension(PFNGLGETPROGRAMRESOURCEIVPROC, glGetProgramResourceiv); \
    HookExtension(PFNGLGETPROGRAMRESOURCENAMEPROC, glGetProgramResourceName); \
    HookExtension(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback); \
    HookExtensionAlias(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback, glDebugMessageCallbackARB); \
    HookExtension(PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl); \
    HookExtension(PFNGLDEBUGMESSAGEINSERTPROC, glDebugMessageInsert); \
    HookExtension(PFNGLGETOBJECTLABELPROC, glGetObjectLabel); \
    HookExtension(PFNGLOBJECTLABELPROC, glObjectLabel); \
    HookExtension(PFNGLENABLEIPROC, glEnablei); \
    HookExtension(PFNGLDISABLEIPROC, glDisablei); \
    HookExtension(PFNGLGENBUFFERSPROC, glGenBuffers); \
    HookExtension(PFNGLBINDBUFFERPROC, glBindBuffer); \
    HookExtension(PFNGLDRAWBUFFERSPROC, glDrawBuffers); \
    HookExtension(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers); \
    HookExtension(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer); \
    HookExtension(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture); \
    HookExtension(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers); \
    HookExtension(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC, glGetFramebufferAttachmentParameteriv); \
    HookExtension(PFNGLBUFFERDATAPROC, glBufferData); \
    HookExtension(PFNGLBINDBUFFERBASEPROC, glBindBufferBase); \
    HookExtension(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange); \
    HookExtension(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange); \
    HookExtension(PFNGLUNMAPBUFFERPROC, glUnmapBuffer); \
    HookExtension(PFNGLDELETEBUFFERSPROC, glDeleteBuffers); \
    HookExtension(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData); \
    HookExtension(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays); \
    HookExtension(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray); \
    HookExtension(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays); \
    HookExtension(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer); \
    HookExtension(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray); \
    HookExtension(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray); \
    HookExtension(PFNGLGETVERTEXATTRIBIVPROC, glGetVertexAttribiv); \
    HookExtension(PFNGLGETVERTEXATTRIBPOINTERVPROC, glGetVertexAttribPointerv); \
    HookExtension(PFNGLGENSAMPLERSPROC, glGenSamplers); \
    HookExtension(PFNGLBINDSAMPLERPROC, glBindSampler); \
    HookExtension(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri); \
    HookExtension(PFNGLSAMPLERPARAMETERFPROC, glSamplerParameterf); \
    HookExtension(PFNGLSAMPLERPARAMETERIVPROC, glSamplerParameteriv); \
    HookExtension(PFNGLSAMPLERPARAMETERFVPROC, glSamplerParameterfv); \
    HookExtension(PFNGLSAMPLERPARAMETERIIVPROC, glSamplerParameterIiv); \
    HookExtension(PFNGLSAMPLERPARAMETERIUIVPROC, glSamplerParameterIuiv); \
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
    HookExtension(PFNGLGETACTIVEUNIFORMSIVPROC, glGetActiveUniformsiv); \
    HookExtension(PFNGLGETACTIVEATTRIBPROC, glGetActiveAttrib); \
    HookExtension(PFNGLGETUNIFORMFVPROC, glGetUniformfv); \
    HookExtension(PFNGLGETUNIFORMIVPROC, glGetUniformiv); \
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
    HookExtension(PFNGLBINDTEXTUREPROC, glBindTexture); \
    HookExtension(PFNGLBLENDFUNCPROC, glBlendFunc); \
    HookExtension(PFNGLCLEARPROC, glClear); \
    HookExtension(PFNGLCLEARCOLORPROC, glClearColor); \
    HookExtension(PFNGLCLEARDEPTHPROC, glClearDepth); \
    HookExtension(PFNGLCOLORMASKPROC, glColorMask); \
    HookExtension(PFNGLCULLFACEPROC, glCullFace); \
    HookExtension(PFNGLDEPTHFUNCPROC, glDepthFunc); \
    HookExtension(PFNGLDEPTHMASKPROC, glDepthMask); \
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
    HookExtension(PFNGLTEXPARAMETERIPROC, glTexParameteri); \
    HookExtension(PFNGLVIEWPORTPROC, glViewport); \
    HookExtension(PFNGLLIGHTFVPROC, glLightfv); \
    HookExtension(PFNGLMATERIALFVPROC, glMaterialfv); \
    HookExtension(PFNGLGENLISTSPROC, glGenLists); \
    HookExtension(PFNGLNEWLISTPROC, glNewList); \
    HookExtension(PFNGLENDLISTPROC, glEndList); \
    HookExtension(PFNGLCALLLISTPROC, glCallList); \
    HookExtension(PFNGLSHADEMODELPROC, glShadeModel); \
    HookExtension(PFNGLBEGINPROC, glBegin); \
    HookExtension(PFNGLENDPROC, glEnd); \
    HookExtension(PFNGLVERTEX3FPROC, glVertex3f); \
    HookExtension(PFNGLNORMAL3FPROC, glNormal3f); \
    HookExtension(PFNGLPUSHMATRIXPROC, glPushMatrix); \
    HookExtension(PFNGLPOPMATRIXPROC, glPopMatrix); \
    HookExtension(PFNGLMATRIXMODEPROC, glMatrixMode); \
    HookExtension(PFNGLLOADIDENTITYPROC, glLoadIdentity); \
    HookExtension(PFNGLFRUSTUMPROC, glFrustum); \
    HookExtension(PFNGLTRANSLATEFPROC, glTranslatef); \
    HookExtension(PFNGLROTATEFPROC, glRotatef); \



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
    HookWrapper3(void, glTexParameteri, GLenum, target, GLenum, pname, GLint, param); \
    HookWrapper4(void, glViewport, GLint, x, GLint, y, GLsizei, width, GLsizei, height); \
    HookWrapper3(void, glLightfv, GLenum, light, GLenum, pname, const GLfloat *, params); \
    HookWrapper3(void, glMaterialfv, GLenum, face, GLenum, pname, const GLfloat *, params); \
    HookWrapper1(GLuint, glGenLists, GLsizei, range); \
    HookWrapper2(void, glNewList, GLuint, list, GLenum, mode); \
    HookWrapper0(void, glEndList); \
    HookWrapper1(void, glCallList, GLuint, list); \
    HookWrapper1(void, glShadeModel, GLenum, mode); \
    HookWrapper1(void, glBegin, GLenum, mode); \
    HookWrapper0(void, glEnd); \
    HookWrapper3(void, glVertex3f, GLfloat, x, GLfloat, y, GLfloat, z); \
    HookWrapper3(void, glNormal3f, GLfloat, nx, GLfloat, ny, GLfloat, nz); \
    HookWrapper0(void, glPushMatrix); \
    HookWrapper0(void, glPopMatrix); \
    HookWrapper1(void, glMatrixMode, GLenum, mode); \
    HookWrapper0(void, glLoadIdentity); \
    HookWrapper6(void, glFrustum, GLdouble, left, GLdouble, right, GLdouble, bottom, GLdouble, top, GLdouble, zNear, GLdouble, zFar); \
    HookWrapper3(void, glTranslatef, GLfloat, x, GLfloat, y, GLfloat, z); \
    HookWrapper4(void, glRotatef, GLfloat, angle, GLfloat, x, GLfloat, y, GLfloat, z); \



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
    HookWrapper7(void, glTexSubImage1D, GLenum, target, GLint, level, GLint, xoffset, GLsizei, width, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper9(void, glTexSubImage2D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper11(void, glTexSubImage3D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper1(void, glGenerateMipmap, GLenum, target); \
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
    HookWrapper5(void, glColorMaski, GLuint, index, GLboolean, r, GLboolean, g, GLboolean, b, GLboolean, a); \
    HookWrapper2(void, glDepthRange, GLdouble, near, GLdouble, far); \
    HookWrapper2(void, glDepthRangef, GLfloat, n, GLfloat, f); \
    HookWrapper3(void, glDepthRangeArrayv, GLuint, first, GLsizei, count, const GLdouble *, v); \
    HookWrapper2(void, glDepthBoundsEXT, GLclampd, zmin, GLclampd, zmax); \
    HookWrapper1(GLuint, glCreateShader, GLenum, type); \
    HookWrapper1(void, glDeleteShader, GLuint, shader); \
    HookWrapper4(void, glShaderSource, GLuint, shader, GLsizei, count, const GLchar *const*, string, const GLint *, length); \
    HookWrapper1(void, glCompileShader, GLuint, shader); \
    HookWrapper3(void, glGetShaderiv, GLuint, shader, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetShaderInfoLog, GLuint, shader, GLsizei, bufSize, GLsizei *, length, GLchar *, infoLog); \
    HookWrapper0(GLuint, glCreateProgram); \
    HookWrapper1(void, glDeleteProgram, GLuint, program); \
    HookWrapper2(void, glAttachShader, GLuint, program, GLuint, shader); \
    HookWrapper1(void, glLinkProgram, GLuint, program); \
    HookWrapper1(void, glUseProgram, GLuint, program); \
    HookWrapper3(void, glGetProgramiv, GLuint, program, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetProgramInfoLog, GLuint, program, GLsizei, bufSize, GLsizei *, length, GLchar *, infoLog); \
    HookWrapper4(void, glGetProgramInterfaceiv, GLuint, program, GLenum, programInterface, GLenum, pname, GLint *, params); \
    HookWrapper8(void, glGetProgramResourceiv, GLuint, program, GLenum, programInterface, GLuint, index, GLsizei, propCount, const GLenum *, props, GLsizei, bufSize, GLsizei *, length, GLint *, params); \
    HookWrapper6(void, glGetProgramResourceName, GLuint, program, GLenum, programInterface, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLchar *, name); \
    HookWrapper2(void, glDebugMessageCallback, GLDEBUGPROC, callback, const void *, userParam); \
    HookWrapper6(void, glDebugMessageControl, GLenum, source, GLenum, type, GLenum, severity, GLsizei, count, const GLuint *, ids, GLboolean, enabled); \
    HookWrapper6(void, glDebugMessageInsert, GLenum, source, GLenum, type, GLuint, id, GLenum, severity, GLsizei, length, const GLchar *, buf); \
    HookWrapper5(void, glGetObjectLabel, GLenum, identifier, GLuint, name, GLsizei, bufSize, GLsizei *, length, GLchar *, label); \
    HookWrapper4(void, glObjectLabel, GLenum, identifier, GLuint, name, GLsizei, length, const GLchar *, label); \
    HookWrapper2(void, glEnablei, GLenum, target, GLuint, index); \
    HookWrapper2(void, glDisablei, GLenum, target, GLuint, index); \
    HookWrapper2(void, glGenBuffers, GLsizei, n, GLuint *, buffers); \
    HookWrapper2(void, glBindBuffer, GLenum, target, GLuint, buffer); \
    HookWrapper2(void, glDrawBuffers, GLsizei, n, const GLenum *, bufs); \
    HookWrapper2(void, glGenFramebuffers, GLsizei, n, GLuint *, framebuffers); \
    HookWrapper2(void, glBindFramebuffer, GLenum, target, GLuint, framebuffer); \
    HookWrapper4(void, glFramebufferTexture, GLenum, target, GLenum, attachment, GLuint, texture, GLint, level); \
    HookWrapper2(void, glDeleteFramebuffers, GLsizei, n, const GLuint *, framebuffers); \
    HookWrapper4(void, glGetFramebufferAttachmentParameteriv, GLenum, target, GLenum, attachment, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glBufferData, GLenum, target, GLsizeiptr, size, const void *, data, GLenum, usage); \
    HookWrapper3(void, glBindBufferBase, GLenum, target, GLuint, index, GLuint, buffer); \
    HookWrapper5(void, glBindBufferRange, GLenum, target, GLuint, index, GLuint, buffer, GLintptr, offset, GLsizeiptr, size); \
    HookWrapper4(void *, glMapBufferRange, GLenum, target, GLintptr, offset, GLsizeiptr, length, GLbitfield, access); \
    HookWrapper1(GLboolean, glUnmapBuffer, GLenum, target); \
    HookWrapper2(void, glDeleteBuffers, GLsizei, n, const GLuint *, buffers); \
    HookWrapper4(void, glGetBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, void *, data); \
    HookWrapper2(void, glGenVertexArrays, GLsizei, n, GLuint *, arrays); \
    HookWrapper1(void, glBindVertexArray, GLuint, array); \
    HookWrapper2(void, glDeleteVertexArrays, GLsizei, n, const GLuint *, arrays); \
    HookWrapper6(void, glVertexAttribPointer, GLuint, index, GLint, size, GLenum, type, GLboolean, normalized, GLsizei, stride, const void *, pointer); \
    HookWrapper1(void, glEnableVertexAttribArray, GLuint, index); \
    HookWrapper1(void, glDisableVertexAttribArray, GLuint, index); \
    HookWrapper3(void, glGetVertexAttribiv, GLuint, index, GLenum, pname, GLint *, params); \
    HookWrapper3(void, glGetVertexAttribPointerv, GLuint, index, GLenum, pname, void **, pointer); \
    HookWrapper2(void, glGenSamplers, GLsizei, count, GLuint *, samplers); \
    HookWrapper2(void, glBindSampler, GLuint, unit, GLuint, sampler); \
    HookWrapper3(void, glSamplerParameteri, GLuint, sampler, GLenum, pname, GLint, param); \
    HookWrapper3(void, glSamplerParameterf, GLuint, sampler, GLenum, pname, GLfloat, param); \
    HookWrapper3(void, glSamplerParameteriv, GLuint, sampler, GLenum, pname, const GLint *, param); \
    HookWrapper3(void, glSamplerParameterfv, GLuint, sampler, GLenum, pname, const GLfloat *, param); \
    HookWrapper3(void, glSamplerParameterIiv, GLuint, sampler, GLenum, pname, const GLint *, param); \
    HookWrapper3(void, glSamplerParameterIuiv, GLuint, sampler, GLenum, pname, const GLuint *, param); \
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
    HookWrapper5(void, glGetActiveUniformsiv, GLuint, program, GLsizei, uniformCount, const GLuint *, uniformIndices, GLenum, pname, GLint *, params); \
    HookWrapper7(void, glGetActiveAttrib, GLuint, program, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLint *, size, GLenum *, type, GLchar *, name); \
    HookWrapper3(void, glGetUniformfv, GLuint, program, GLint, location, GLfloat *, params); \
    HookWrapper3(void, glGetUniformiv, GLuint, program, GLint, location, GLint *, params); \
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




