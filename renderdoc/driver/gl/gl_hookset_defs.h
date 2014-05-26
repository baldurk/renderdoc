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
    HookInit(glBlendColor); \
    HookInit(glClear); \
    HookInit(glClearColor); \
    HookInit(glClearDepth); \
    HookInit(glDepthFunc); \
    HookInit(glDisable); \
    HookInit(glDrawArrays); \
    HookInit(glEnable); \
    HookInit(glGenTextures); \
    HookInit(glDeleteTextures); \
    HookInit(glGetError); \
    HookInit(glGetFloatv); \
    HookInit(glGetTexLevelParameteriv); \
    HookInit(glGetTexLevelParameterfv); \
    HookInit(glGetTexParameterfv); \
    HookInit(glGetTexParameteriv); \
    HookInit(glGetIntegerv); \
    HookInit(glGetString); \
    HookInit(glHint); \
    HookInit(glPixelStorei); \
    HookInit(glPixelStoref); \
    HookInit(glPolygonMode); \
    HookInit(glReadPixels); \
    HookInit(glReadBuffer); \
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
    HookExtension(PFNGLTEXSTORAGE2DPROC, glTexStorage2D); \
    HookExtension(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D); \
    HookExtension(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap); \
    HookExtension(PFNGLGETINTERNALFORMATIVPROC, glGetInternalformativ); \
    HookExtension(PFNGLGETINTERNALFORMATI64VPROC, glGetInternalformati64v); \
    HookExtension(PFNGLGETBUFFERPARAMETERIVPROC, glGetBufferParameteriv); \
    HookExtension(PFNGLGETSTRINGIPROC, glGetStringi); \
    HookExtension(PFNGLGETINTEGERI_VPROC, glGetIntegeri_v); \
    HookExtension(PFNGLGETINTEGER64I_VPROC, glGetInteger64i_v); \
    HookExtension(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus); \
    HookExtension(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate); \
    HookExtension(PFNGLBLENDFUNCSEPARATEIPROC, glBlendFuncSeparatei); \
    HookExtension(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate); \
    HookExtension(PFNGLBLENDEQUATIONSEPARATEIPROC, glBlendEquationSeparatei); \
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
    HookExtension(PFNGLGETOBJECTLABELPROC, glGetObjectLabel); \
    HookExtension(PFNGLOBJECTLABELPROC, glObjectLabel); \
    HookExtension(PFNGLGENBUFFERSPROC, glGenBuffers); \
    HookExtension(PFNGLBINDBUFFERPROC, glBindBuffer); \
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
    HookExtension(PFNGLGETVERTEXATTRIBIVPROC, glGetVertexAttribiv); \
    HookExtension(PFNGLGETVERTEXATTRIBPOINTERVPROC, glGetVertexAttribPointerv); \
    HookExtension(PFNGLGENSAMPLERSPROC, glGenSamplers); \
    HookExtension(PFNGLBINDSAMPLERPROC, glBindSampler); \
    HookExtension(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri); \
    HookExtension(PFNGLCLEARBUFFERFVPROC, glClearBufferfv); \
    HookExtension(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation); \
    HookExtension(PFNGLGETACTIVEUNIFORMPROC, glGetActiveUniform); \
    HookExtension(PFNGLGETUNIFORMFVPROC, glGetUniformfv); \
    HookExtension(PFNGLGETUNIFORMIVPROC, glGetUniformiv); \
    HookExtension(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv); \
    HookExtension(PFNGLUNIFORM3FVPROC, glUniform3fv); \
    HookExtension(PFNGLUNIFORM4FVPROC, glUniform4fv); \
    HookExtension(PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC, glDrawArraysInstancedBaseInstance); \
    HookExtension(PFNGLBINDTEXTUREPROC, glBindTexture); \
    HookExtension(PFNGLBLENDFUNCPROC, glBlendFunc); \
    HookExtension(PFNGLBLENDCOLORPROC, glBlendColor); \
    HookExtension(PFNGLCLEARPROC, glClear); \
    HookExtension(PFNGLCLEARCOLORPROC, glClearColor); \
    HookExtension(PFNGLCLEARDEPTHPROC, glClearDepth); \
    HookExtension(PFNGLDEPTHFUNCPROC, glDepthFunc); \
    HookExtension(PFNGLDISABLEPROC, glDisable); \
    HookExtension(PFNGLDRAWARRAYSPROC, glDrawArrays); \
    HookExtension(PFNGLENABLEPROC, glEnable); \
    HookExtension(PFNGLGENTEXTURESPROC, glGenTextures); \
    HookExtension(PFNGLDELETETEXTURESPROC, glDeleteTextures); \
    HookExtension(PFNGLGETERRORPROC, glGetError); \
    HookExtension(PFNGLGETFLOATVPROC, glGetFloatv); \
    HookExtension(PFNGLGETTEXLEVELPARAMETERIVPROC, glGetTexLevelParameteriv); \
    HookExtension(PFNGLGETTEXLEVELPARAMETERFVPROC, glGetTexLevelParameterfv); \
    HookExtension(PFNGLGETTEXPARAMETERFVPROC, glGetTexParameterfv); \
    HookExtension(PFNGLGETTEXPARAMETERIVPROC, glGetTexParameteriv); \
    HookExtension(PFNGLGETINTEGERVPROC, glGetIntegerv); \
    HookExtension(PFNGLGETSTRINGPROC, glGetString); \
    HookExtension(PFNGLHINTPROC, glHint); \
    HookExtension(PFNGLPIXELSTOREIPROC, glPixelStorei); \
    HookExtension(PFNGLPIXELSTOREFPROC, glPixelStoref); \
    HookExtension(PFNGLPOLYGONMODEPROC, glPolygonMode); \
    HookExtension(PFNGLREADPIXELSPROC, glReadPixels); \
    HookExtension(PFNGLREADBUFFERPROC, glReadBuffer); \
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
    HookWrapper4(void, glBlendColor, GLfloat, red, GLfloat, green, GLfloat, blue, GLfloat, alpha); \
    HookWrapper1(void, glClear, GLbitfield, mask); \
    HookWrapper4(void, glClearColor, GLfloat, red, GLfloat, green, GLfloat, blue, GLfloat, alpha); \
    HookWrapper1(void, glClearDepth, GLdouble, depth); \
    HookWrapper1(void, glDepthFunc, GLenum, func); \
    HookWrapper1(void, glDisable, GLenum, cap); \
    HookWrapper3(void, glDrawArrays, GLenum, mode, GLint, first, GLsizei, count); \
    HookWrapper1(void, glEnable, GLenum, cap); \
    HookWrapper2(void, glGenTextures, GLsizei, n, GLuint *, textures); \
    HookWrapper2(void, glDeleteTextures, GLsizei, n, const GLuint *, textures); \
    HookWrapper0(GLenum, glGetError); \
    HookWrapper2(void, glGetFloatv, GLenum, pname, GLfloat *, data); \
    HookWrapper4(void, glGetTexLevelParameteriv, GLenum, target, GLint, level, GLenum, pname, GLint *, params); \
    HookWrapper4(void, glGetTexLevelParameterfv, GLenum, target, GLint, level, GLenum, pname, GLfloat *, params); \
    HookWrapper3(void, glGetTexParameterfv, GLenum, target, GLenum, pname, GLfloat *, params); \
    HookWrapper3(void, glGetTexParameteriv, GLenum, target, GLenum, pname, GLint *, params); \
    HookWrapper2(void, glGetIntegerv, GLenum, pname, GLint *, data); \
    HookWrapper1(const GLubyte *, glGetString, GLenum, name); \
    HookWrapper2(void, glHint, GLenum, target, GLenum, mode); \
    HookWrapper2(void, glPixelStorei, GLenum, pname, GLint, param); \
    HookWrapper2(void, glPixelStoref, GLenum, pname, GLfloat, param); \
    HookWrapper2(void, glPolygonMode, GLenum, face, GLenum, mode); \
    HookWrapper7(void, glReadPixels, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, void *, pixels); \
    HookWrapper1(void, glReadBuffer, GLenum, mode); \
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
    HookWrapper5(void, glTexStorage2D, GLenum, target, GLsizei, levels, GLenum, internalformat, GLsizei, width, GLsizei, height); \
    HookWrapper9(void, glTexSubImage2D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, const void *, pixels); \
    HookWrapper1(void, glGenerateMipmap, GLenum, target); \
    HookWrapper5(void, glGetInternalformativ, GLenum, target, GLenum, internalformat, GLenum, pname, GLsizei, bufSize, GLint *, params); \
    HookWrapper5(void, glGetInternalformati64v, GLenum, target, GLenum, internalformat, GLenum, pname, GLsizei, bufSize, GLint64 *, params); \
    HookWrapper3(void, glGetBufferParameteriv, GLenum, target, GLenum, pname, GLint *, params); \
    HookWrapper2(const GLubyte *, glGetStringi, GLenum, name, GLuint, index); \
    HookWrapper3(void, glGetIntegeri_v, GLenum, target, GLuint, index, GLint *, data); \
    HookWrapper3(void, glGetInteger64i_v, GLenum, target, GLuint, index, GLint64 *, data); \
    HookWrapper1(GLenum, glCheckFramebufferStatus, GLenum, target); \
    HookWrapper4(void, glBlendFuncSeparate, GLenum, sfactorRGB, GLenum, dfactorRGB, GLenum, sfactorAlpha, GLenum, dfactorAlpha); \
    HookWrapper5(void, glBlendFuncSeparatei, GLuint, buf, GLenum, sfactorRGB, GLenum, dfactorRGB, GLenum, sfactorAlpha, GLenum, dfactorAlpha); \
    HookWrapper2(void, glBlendEquationSeparate, GLenum, modeRGB, GLenum, modeAlpha); \
    HookWrapper3(void, glBlendEquationSeparatei, GLuint, buf, GLenum, modeRGB, GLenum, modeAlpha); \
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
    HookWrapper5(void, glGetObjectLabel, GLenum, identifier, GLuint, name, GLsizei, bufSize, GLsizei *, length, GLchar *, label); \
    HookWrapper4(void, glObjectLabel, GLenum, identifier, GLuint, name, GLsizei, length, const GLchar *, label); \
    HookWrapper2(void, glGenBuffers, GLsizei, n, GLuint *, buffers); \
    HookWrapper2(void, glBindBuffer, GLenum, target, GLuint, buffer); \
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
    HookWrapper3(void, glGetVertexAttribiv, GLuint, index, GLenum, pname, GLint *, params); \
    HookWrapper3(void, glGetVertexAttribPointerv, GLuint, index, GLenum, pname, void **, pointer); \
    HookWrapper2(void, glGenSamplers, GLsizei, count, GLuint *, samplers); \
    HookWrapper2(void, glBindSampler, GLuint, unit, GLuint, sampler); \
    HookWrapper3(void, glSamplerParameteri, GLuint, sampler, GLenum, pname, GLint, param); \
    HookWrapper3(void, glClearBufferfv, GLenum, buffer, GLint, drawbuffer, const GLfloat *, value); \
    HookWrapper2(GLint, glGetUniformLocation, GLuint, program, const GLchar *, name); \
    HookWrapper7(void, glGetActiveUniform, GLuint, program, GLuint, index, GLsizei, bufSize, GLsizei *, length, GLint *, size, GLenum *, type, GLchar *, name); \
    HookWrapper3(void, glGetUniformfv, GLuint, program, GLint, location, GLfloat *, params); \
    HookWrapper3(void, glGetUniformiv, GLuint, program, GLint, location, GLint *, params); \
    HookWrapper4(void, glUniformMatrix4fv, GLint, location, GLsizei, count, GLboolean, transpose, const GLfloat *, value); \
    HookWrapper3(void, glUniform3fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper3(void, glUniform4fv, GLint, location, GLsizei, count, const GLfloat *, value); \
    HookWrapper5(void, glDrawArraysInstancedBaseInstance, GLenum, mode, GLint, first, GLsizei, count, GLsizei, instancecount, GLuint, baseinstance); \




