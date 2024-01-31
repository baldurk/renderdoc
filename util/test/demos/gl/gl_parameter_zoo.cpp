/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#ifdef WIN64
extern "C" __declspec(dllexport) GLenum APIENTRY InternalFunction()
{
  return GL_QUERY_BUFFER;
}
#endif

RD_TEST(GL_Parameter_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "General tests of parameters known to cause problems - e.g. optional values that should be "
      "ignored, edge cases, special values, etc.";

  const char *vertex = R"EOSHADER(
#version 450 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

uniform int mode;

out vec4 v2fcol;

void main()
{
	gl_Position = vec4(Position.xyz * (mode == 1 ? 5.0f : 1.0f), 1);
	v2fcol = Color;
}

!!!!)EOSHADER";

  const char *pixel = R"EOSHADER(
#version 450 core

in vec4 v2fcol;

layout(location = 0, index = 0) out vec4 Color;
uniform int mode;

void main()
{
  if(mode == 1)
    Color = vec4(0, 0, 1, 0.5);
  else
	  Color = v2fcol;
}

)EOSHADER";

  int main()
  {
    // don't use core profile so that we can check glGetString(GL_EXTENSIONS)
    coreProfile = false;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    std::vector<std::string> exts;

    GLint num = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    exts.resize(num);
    for(GLint i = 0; i < num; i++)
      exts[i] = (const char *)glGetStringi(GL_EXTENSIONS, i);

    std::string ext_string = (const char *)glGetString(GL_EXTENSIONS);

    std::vector<std::string> exts_split;
    while(!ext_string.empty())
    {
      std::string e = ext_string.substr(0, ext_string.find(' '));
      ext_string.erase(0, e.length() + 1);
      exts_split.push_back(e);
    }

    std::sort(exts_split.begin(), exts_split.end());
    std::sort(exts.begin(), exts.end());

    if(exts != exts_split)
    {
      TEST_ERROR("Extension string from glGetString doesn't match glGetStringi extensions");
      return 4;
    }

    if(std::unique(exts.begin(), exts.end()) != exts.end())
    {
      TEST_ERROR("Extensions from glGetStringi contains duplicates");
      return 5;
    }

    if(std::unique(exts_split.begin(), exts_split.end()) != exts_split.end())
    {
      TEST_ERROR("Extension string from glGetString contains duplicates");
      return 5;
    }

    GLuint trashvao = MakeVAO();
    glBindVertexArray(trashvao);

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram();

    GLint len = (int)strlen(vertex) - 5;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex, &len);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &pixel, NULL);
    glCompileShader(fs);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLuint trash = MakeBuffer();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, trash);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, 1024, 0, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, trash);

    if(GLAD_GL_ARB_query_buffer_object)
      glBindBuffer(GL_QUERY_BUFFER, trash);

#if defined(WIN64)
    PFNGLGETERRORPROC internalFunc =
        (PFNGLGETERRORPROC)GetProcAddress(GetModuleHandleA(NULL), "InternalFunction");

    if(internalFunc == NULL || internalFunc() != GL_QUERY_BUFFER)
    {
      TEST_ERROR("Couldn't query own module for a function");
      program = 0;
    }
#endif

    ActivateContext(mainWindow, NULL);
    ActivateContext(mainWindow, mainContext, true);

    GLuint mapPersTest = MakeBuffer();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mapPersTest);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 1024, 0, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT);

    while(Running())
    {
      // trash the texture pack/unpack state
      glPixelStorei(GL_UNPACK_ROW_LENGTH, screenWidth + screenHeight + 99);
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 5);
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 8180);
      glPixelStorei(GL_UNPACK_SKIP_ROWS, 17);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

      glPixelStorei(GL_PACK_ROW_LENGTH, screenWidth + screenHeight * 1 + 37);
      glPixelStorei(GL_PACK_SKIP_PIXELS, 9734);
      glPixelStorei(GL_PACK_SKIP_ROWS, 33);
      glPixelStorei(GL_PACK_ALIGNMENT, 8);

      glBindBuffer(GL_SHADER_STORAGE_BUFFER, mapPersTest);
      glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 1024, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT);
      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

      for(int i = 0; i < 100; i++)
        glGetError();

      glBindProgramPipeline(0);

      if(glGetError() != GL_NONE)
      {
        TEST_ERROR("Got an error from glBindProgramPipeline");
      }

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 4, NULL);

      glBindVertexArray(trashvao);
      glBindVertexBuffers(0, 4, NULL, NULL, NULL);
      glBindTextures(0, 10, NULL);
      glBindSamplers(0, 10, NULL);
      glBindImageTextures(0, 4, NULL);

      glBindVertexArray(vao);

      float col[] = {1.0f, 0.0f, 1.0f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      setMarker("NoScissor");
      glDisable(GL_SCISSOR_TEST);
      glScissor(0, 0, 1, 1);
      glProgramUniform1i(program, glGetUniformLocation(program, "mode"), 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glEnable(GL_SCISSOR_TEST);
      glScissor(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glProgramUniform1i(program, glGetUniformLocation(program, "mode"), 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glScissor(320, 50, 1, 1);

      glProgramUniform1i(program, glGetUniformLocation(program, "mode"), 1);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      if(GLAD_GL_GREMEDY_frame_terminator)
        glFrameTerminatorGREMEDY();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
