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

typedef void (APIENTRYP PFNGLLIGHTFVPROC) (GLenum light, GLenum pname, const GLfloat *params);
typedef void (APIENTRYP PFNGLMATERIALFVPROC) (GLenum face, GLenum pname, const GLfloat *params);
typedef GLuint (APIENTRYP PFNGLGENLISTSPROC) (GLsizei range);
typedef void (APIENTRYP PFNGLNEWLISTPROC) (GLuint list, GLenum mode);
typedef void (APIENTRYP PFNGLENDLISTPROC) (void);
typedef void (APIENTRYP PFNGLCALLLISTPROC) (GLuint list);
typedef void (APIENTRYP PFNGLSHADEMODELPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLBEGINPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLENDPROC) (void);
typedef void (APIENTRYP PFNGLVERTEX3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLNORMAL3FPROC) (GLfloat nx, GLfloat ny, GLfloat nz);
typedef void (APIENTRYP PFNGLPUSHMATRIXPROC) (void);
typedef void (APIENTRYP PFNGLPOPMATRIXPROC) (void);
typedef void (APIENTRYP PFNGLMATRIXMODEPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLLOADIDENTITYPROC) (void);
typedef void (APIENTRYP PFNGLFRUSTUMPROC) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef void (APIENTRYP PFNGLTRANSLATEFPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRYP PFNGLROTATEFPROC) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

