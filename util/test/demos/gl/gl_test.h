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

#pragma once

#include "../test_common.h"

#include "3rdparty/glad/glad.h"

#include <vector>

struct OpenGLGraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::OpenGL;

  void Prepare(int argc, char **argv);
  bool Init();
  void Shutdown();
  GraphicsWindow *MakeWindow(int width, int height, const char *title);
  void *MakeContext(GraphicsWindow *win, void *share);
  void DestroyContext(void *ctx);
  void ActivateContext(GraphicsWindow *win, void *ctx);

  void PostInit();

  GLuint MakeProgram(std::string vertSrc, std::string fragSrc, std::string geomSrc = "");
  GLuint MakeProgram();
  GLuint MakePipeline();
  GLuint MakeBuffer();
  GLuint MakeTexture();
  GLuint MakeVAO();
  GLuint MakeFBO();

  void pushMarker(const std::string &name);
  void setMarker(const std::string &name);
  void popMarker();

  bool Running();
  void Present(GraphicsWindow *window);
  void Present() { Present(mainWindow); }
  int glMajor = 4;
  int glMinor = 3;
  bool coreProfile = true;
  bool gles = false;

  GraphicsWindow *mainWindow = NULL;
  void *mainContext = NULL;
  bool inited = false;

  struct
  {
    std::vector<GLuint> bufs, texs, progs, pipes, vaos, fbos;
  } managedResources;
};