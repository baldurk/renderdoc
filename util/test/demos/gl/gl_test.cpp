/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2015-2019 Baldur Karlsson
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

#include "gl_test.h"
#include <stdio.h>

static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                   GLsizei length, const GLchar *message, const void *userParam)
{
  // too much spam on these types
  if(type != GL_DEBUG_TYPE_PERFORMANCE && type != GL_DEBUG_TYPE_OTHER &&
     source != GL_DEBUG_SOURCE_APPLICATION)
  {
    TEST_ERROR("Debug message: %s", message);
  }
}

void OpenGLGraphicsTest::PostInit()
{
  glEnable(GL_FRAMEBUFFER_SRGB);

  if(GLAD_GL_KHR_debug)
  {
    glDebugMessageCallback(&debugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  }

  TEST_LOG("Running GL test on %s / %s / %s", glGetString(GL_VENDOR), glGetString(GL_RENDERER),
           glGetString(GL_VERSION));
}

void OpenGLGraphicsTest::Shutdown()
{
  if(!managedResources.bufs.empty())
    glDeleteBuffers((GLsizei)managedResources.bufs.size(), &managedResources.bufs[0]);
  if(!managedResources.texs.empty())
    glDeleteTextures((GLsizei)managedResources.texs.size(), &managedResources.texs[0]);
  if(!managedResources.vaos.empty())
    glDeleteVertexArrays((GLsizei)managedResources.vaos.size(), &managedResources.vaos[0]);
  if(!managedResources.fbos.empty())
    glDeleteFramebuffers((GLsizei)managedResources.fbos.size(), &managedResources.fbos[0]);
  if(!managedResources.pipes.empty())
    glDeleteProgramPipelines((GLsizei)managedResources.pipes.size(), &managedResources.pipes[0]);

  for(GLuint p : managedResources.progs)
    glDeleteProgram(p);

  delete mainWindow;
  DestroyContext(mainContext);
}

GLuint OpenGLGraphicsTest::MakeProgram(std::string vertSrc, std::string fragSrc, std::string geomSrc)
{
  GLuint vs = vertSrc.empty() ? 0 : glCreateShader(GL_VERTEX_SHADER);
  GLuint fs = fragSrc.empty() ? 0 : glCreateShader(GL_FRAGMENT_SHADER);
  GLuint gs = geomSrc.empty() ? 0 : glCreateShader(GL_GEOMETRY_SHADER);

  const char *cstr = NULL;

  if(vs)
  {
    cstr = vertSrc.c_str();
    glShaderSource(vs, 1, &cstr, NULL);
    glCompileShader(vs);
  }

  if(fs)
  {
    cstr = fragSrc.c_str();
    glShaderSource(fs, 1, &cstr, NULL);
    glCompileShader(fs);
  }

  if(gs)
  {
    cstr = geomSrc.c_str();
    glShaderSource(gs, 1, &cstr, NULL);
    glCompileShader(gs);
  }

  char buffer[1024];
  GLint status = 0;

  if(vs)
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
  else
    status = 1;

  if(status == 0)
  {
    glGetShaderInfoLog(vs, 1024, NULL, buffer);
    TEST_ERROR("Shader error: %s", buffer);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(gs);
    return 0;
  }

  if(fs)
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
  else
    status = 1;

  if(status == 0)
  {
    glGetShaderInfoLog(fs, 1024, NULL, buffer);
    TEST_ERROR("Shader error: %s", buffer);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(gs);
    return 0;
  }

  if(gs)
    glGetShaderiv(gs, GL_COMPILE_STATUS, &status);
  else
    status = 1;

  if(status == 0)
  {
    glGetShaderInfoLog(gs, 1024, NULL, buffer);
    TEST_ERROR("Shader error: %s", buffer);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(gs);
    return 0;
  }

  GLuint program = glCreateProgram();

  if(vs)
    glAttachShader(program, vs);
  if(fs)
    glAttachShader(program, fs);
  if(gs)
    glAttachShader(program, gs);

  if(!vs || !fs)
    glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);

  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if(status == 0)
  {
    glGetProgramInfoLog(program, 1024, NULL, buffer);
    TEST_ERROR("Link error: %s", buffer);

    glDeleteProgram(program);
    program = 0;
  }

  if(vs)
  {
    glDetachShader(program, vs);
    glDeleteShader(vs);
  }
  if(fs)
  {
    glDetachShader(program, fs);
    glDeleteShader(fs);
  }
  if(gs)
  {
    glDetachShader(program, gs);
    glDeleteShader(gs);
  }

  if(program)
    managedResources.progs.push_back(program);

  return program;
}

GLuint OpenGLGraphicsTest::MakeProgram()
{
  GLuint program = glCreateProgram();
  managedResources.progs.push_back(program);
  return program;
}

GLuint OpenGLGraphicsTest::MakeBuffer()
{
  std::vector<uint32_t> &bufs = managedResources.bufs;

  bufs.push_back(0);
  glGenBuffers(1, &bufs[bufs.size() - 1]);
  return bufs[bufs.size() - 1];
}

GLuint OpenGLGraphicsTest::MakePipeline()
{
  std::vector<uint32_t> &pipes = managedResources.pipes;

  pipes.push_back(0);
  glCreateProgramPipelines(1, &pipes[pipes.size() - 1]);
  return pipes[pipes.size() - 1];
}

GLuint OpenGLGraphicsTest::MakeTexture()
{
  std::vector<uint32_t> &texs = managedResources.texs;

  texs.push_back(0);
  glGenTextures(1, &texs[texs.size() - 1]);
  return texs[texs.size() - 1];
}

GLuint OpenGLGraphicsTest::MakeVAO()
{
  std::vector<uint32_t> &vaos = managedResources.vaos;

  vaos.push_back(0);
  glGenVertexArrays(1, &vaos[vaos.size() - 1]);
  return vaos[vaos.size() - 1];
}

GLuint OpenGLGraphicsTest::MakeFBO()
{
  std::vector<uint32_t> &fbos = managedResources.fbos;

  fbos.push_back(0);
  glGenFramebuffers(1, &fbos[fbos.size() - 1]);
  return fbos[fbos.size() - 1];
}

void OpenGLGraphicsTest::pushMarker(const std::string &name)
{
  if(glPushDebugGroup)
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
}

void OpenGLGraphicsTest::setMarker(const std::string &name)
{
  if(glDebugMessageInsert)
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                         GL_DEBUG_SEVERITY_LOW, -1, name.c_str());
}

void OpenGLGraphicsTest::popMarker()
{
  if(glPopDebugGroup)
    glPopDebugGroup();
}

bool OpenGLGraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}