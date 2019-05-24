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

TEST(GL_Buffer_Updates, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test of buffer updates, both buffers that are updated regularly and get marked as "
      "dirty, as well as buffers updated mid-frame";

  std::string vertex = R"EOSHADER(
#version 420 core

void main()
{
  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(1.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 1.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  gl_Position = verts[gl_VertexID];
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0, std140) uniform constsbuf
{
  vec4 col;
};

void main()
{
	Color = col;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    Vec4f red(1.0f, 0.0f, 0.0f, 1.0f);
    Vec4f cyan(0.0f, 1.0f, 1.0f, 1.0f);
    Vec4f green(0.0f, 1.0f, 0.0f, 1.0f);

// clang-format explodes on these for some reason
#define TEST_CASES()                             \
  BUFFER_TEST(BufferDataImmutable)               \
  BUFFER_TEST(BufferStorageImmutable)            \
  BUFFER_TEST(BufferDataOrphanedOnce)            \
  BUFFER_TEST(BufferDataOrphanedMany)            \
  BUFFER_TEST(BufferDataOrphanedPerFrame)        \
  BUFFER_TEST(BufferDataUpdatedOnce)             \
  BUFFER_TEST(BufferDataUpdatedMany)             \
  BUFFER_TEST(BufferDataUpdatedPerFrame)         \
  BUFFER_TEST(BufferStorageUpdatedOnce)          \
  BUFFER_TEST(BufferStorageUpdatedMany)          \
  BUFFER_TEST(BufferStorageUpdatedPerFrame)      \
  BUFFER_TEST(SingleMapBufferReadback)           \
  BUFFER_TEST(SingleMapBufferRangeReadback)      \
  BUFFER_TEST(CoherentMapBufferRangeReadback)    \
  BUFFER_TEST(CleanBufferMapWriteInvalidate)     \
  BUFFER_TEST(CleanBufferMapWriteNonInvalidate)  \
  BUFFER_TEST(DirtyBufferMapWriteInvalidate)     \
  BUFFER_TEST(DirtyBufferMapWriteNonInvalidate)  \
  BUFFER_TEST(CleanBufferMapFlushExplicit)       \
  BUFFER_TEST(DirtyBufferMapFlushExplicit)       \
  BUFFER_TEST(CoherentMapWrite)                  \
  BUFFER_TEST(CoherentMapWriteInvalidateRange)   \
  BUFFER_TEST(CoherentMapWriteInvalidateBuffer)  \
  BUFFER_TEST(CoherentMapWriteUnsynchronised)    \
  BUFFER_TEST(NonCoherentMapFlush)               \
  BUFFER_TEST(NonCoherentMapFlushUnsynchronised) \
  BUFFER_TEST(OffsetMapWrite)                    \
  BUFFER_TEST(OffsetMapFlush)

#undef BUFFER_TEST
#define BUFFER_TEST(name) name,
    enum
    {
      TEST_CASES() TestCount,
    };

#undef BUFFER_TEST
#define BUFFER_TEST(name) #name,
    const char *TestNames[TestCount] = {TEST_CASES()};

    Vec4f *ptrs[TestCount] = {};
    GLuint buffers[TestCount] = {};
    for(int i = 0; i < TestCount; i++)
    {
      buffers[i] = MakeBuffer();
      glBindBuffer(GL_UNIFORM_BUFFER, buffers[i]);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataImmutable]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferStorageImmutable]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataOrphanedOnce]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataOrphanedMany]);
    for(int i = 0; i < 100; i++)
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataOrphanedPerFrame]);
    for(int i = 0; i < 100; i++)
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataUpdatedOnce]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataUpdatedMany]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataUpdatedPerFrame]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferStorageUpdatedOnce]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_STORAGE_BIT);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferStorageUpdatedMany]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferStorageUpdatedPerFrame]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[SingleMapBufferReadback]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_MAP_READ_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[SingleMapBufferRangeReadback]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_MAP_READ_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[CoherentMapBufferRangeReadback]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    ptrs[CoherentMapBufferRangeReadback] =
        (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                  GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapWriteInvalidate]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapWriteNonInvalidate]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapFlushExplicit]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[CoherentMapWrite]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    ptrs[CoherentMapWrite] =
        (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                  GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[CoherentMapWriteInvalidateRange]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    ptrs[CoherentMapWriteInvalidateRange] = (Vec4f *)glMapBufferRange(
        GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT |
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[CoherentMapWriteInvalidateBuffer]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    ptrs[CoherentMapWriteInvalidateBuffer] = (Vec4f *)glMapBufferRange(
        GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT |
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[CoherentMapWriteUnsynchronised]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red,
                    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    ptrs[CoherentMapWriteUnsynchronised] = (Vec4f *)glMapBufferRange(
        GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlush]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

    ptrs[NonCoherentMapFlush] = (Vec4f *)glMapBufferRange(
        GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
        GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlushUnsynchronised]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

    ptrs[NonCoherentMapFlushUnsynchronised] =
        (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                  GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT |
                                      GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[OffsetMapWrite]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &cyan,
                    GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &cyan);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[OffsetMapFlush]);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &cyan,
                    GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    for(int i = 0; i < 100; i++)
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &cyan);

    // these buffers are used for indicating a CPU readback passed or failed
    GLuint pass = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, pass);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, 0);
    GLuint fail = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, fail);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, 0);

    GLuint program = MakeProgram(vertex, pixel);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glGenBuffers(1, &buffers[CleanBufferMapWriteInvalidate]);
      glGenBuffers(1, &buffers[CleanBufferMapWriteNonInvalidate]);
      glGenBuffers(1, &buffers[CleanBufferMapFlushExplicit]);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataOrphanedPerFrame]);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &green, GL_DYNAMIC_DRAW);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferDataUpdatedPerFrame]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[BufferStorageUpdatedPerFrame]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &green);

      glBindBuffer(GL_COPY_READ_BUFFER, pass);
      glBindBuffer(GL_COPY_WRITE_BUFFER, buffers[SingleMapBufferReadback]);
      glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(Vec4f));

      glBindBuffer(GL_COPY_READ_BUFFER, pass);
      glBindBuffer(GL_COPY_WRITE_BUFFER, buffers[SingleMapBufferRangeReadback]);
      glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(Vec4f));

      glBindBuffer(GL_COPY_READ_BUFFER, pass);
      glBindBuffer(GL_COPY_WRITE_BUFFER, buffers[CoherentMapBufferRangeReadback]);
      glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(Vec4f));

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[CleanBufferMapWriteInvalidate]);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);

      Vec4f *ptr = NULL;

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      if(ptr)
        memcpy(ptr, &green, sizeof(Vec4f));
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[CleanBufferMapWriteNonInvalidate]);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), GL_MAP_WRITE_BIT);
      if(ptr)
      {
        ptr->x = 0.0f;
        ptr->y = 1.0f;
      }
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapWriteInvalidate]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      if(ptr)
        memcpy(ptr, &green, sizeof(Vec4f));
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapWriteNonInvalidate]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &red);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), GL_MAP_WRITE_BIT);
      if(ptr)
      {
        ptr->x = 0.0f;
        ptr->y = 1.0f;
      }
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[CleanBufferMapFlushExplicit]);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec4f), &red, GL_DYNAMIC_DRAW);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                      GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
      if(ptr)
      {
        ptr->x = 0.0f;
        ptr->y = 1.0f;
      }
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 2);
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[DirtyBufferMapFlushExplicit]);
      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f),
                                      GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
      if(ptr)
      {
        ptr->x = 0.0f;
        ptr->y = 1.0f;
      }
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 2);
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      if(ptrs[CoherentMapWrite])
        memcpy(ptrs[CoherentMapWrite], &red, sizeof(Vec4f));
      if(ptrs[CoherentMapWriteInvalidateRange])
        memcpy(ptrs[CoherentMapWriteInvalidateRange], &red, sizeof(Vec4f));
      if(ptrs[CoherentMapWriteInvalidateBuffer])
        memcpy(ptrs[CoherentMapWriteInvalidateBuffer], &red, sizeof(Vec4f));
      if(ptrs[CoherentMapWriteUnsynchronised])
        memcpy(ptrs[CoherentMapWriteUnsynchronised], &red, sizeof(Vec4f));

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlush]);
      if(ptrs[NonCoherentMapFlush])
        memcpy(ptrs[NonCoherentMapFlush], &red, sizeof(Vec4f));
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlushUnsynchronised]);
      if(ptrs[NonCoherentMapFlushUnsynchronised])
        memcpy(ptrs[NonCoherentMapFlushUnsynchronised], &red, sizeof(Vec4f));
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4);

      glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[OffsetMapWrite]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &cyan);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, sizeof(float) * 2, sizeof(float),
                                      GL_MAP_WRITE_BIT);
      if(ptr)
        ptr->x = 0.0f;
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      glBindBuffer(GL_UNIFORM_BUFFER, buffers[OffsetMapFlush]);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &cyan);

      ptr = (Vec4f *)glMapBufferRange(GL_UNIFORM_BUFFER, sizeof(float) * 2, sizeof(float),
                                      GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
      if(ptr)
        ptr->x = 0.0f;
      glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float));
      glUnmapBuffer(GL_UNIFORM_BUFFER);

      const int squareSize = 50;

      int buf = 0;

      for(int y = 0; y < screenHeight && buf < TestCount; y += squareSize)
      {
        for(int x = 0; x < screenWidth && buf < TestCount; x += squareSize)
        {
          glViewport(x + 1, y + 1, squareSize - 2, squareSize - 2);

          glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                               GL_DEBUG_SEVERITY_HIGH, -1, TestNames[buf]);

          if(buf == CoherentMapWrite || buf == CoherentMapWriteInvalidateRange ||
             buf == CoherentMapWriteInvalidateBuffer || buf == CoherentMapWriteUnsynchronised)
          {
            if(ptrs[buf])
              memcpy(ptrs[buf], &green, sizeof(Vec4f));

            if(buf == CoherentMapWriteUnsynchronised)
              glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
          }

          if(buf == NonCoherentMapFlush)
          {
            glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlush]);
            if(ptrs[NonCoherentMapFlush])
              memcpy(ptrs[NonCoherentMapFlush], &green, sizeof(Vec4f));
            glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4);
          }

          if(buf == NonCoherentMapFlushUnsynchronised)
          {
            glBindBuffer(GL_UNIFORM_BUFFER, buffers[NonCoherentMapFlushUnsynchronised]);
            if(ptrs[NonCoherentMapFlushUnsynchronised])
              memcpy(ptrs[NonCoherentMapFlushUnsynchronised], &green, sizeof(Vec4f));
            glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(float) * 4);
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
          }

          if(buf == SingleMapBufferReadback)
          {
            glBindBuffer(GL_COPY_READ_BUFFER, buffers[buf]);
            void *mapped = glMapBuffer(GL_COPY_READ_BUFFER, GL_READ_ONLY);

            if(mapped && !memcmp(mapped, &green, sizeof(Vec4f)))
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, pass);
            else
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, fail);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glUnmapBuffer(GL_COPY_READ_BUFFER);
          }
          else if(buf == SingleMapBufferRangeReadback)
          {
            glBindBuffer(GL_COPY_READ_BUFFER, buffers[buf]);
            void *mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, sizeof(Vec4f), GL_MAP_READ_BIT);

            if(mapped && !memcmp(mapped, &green, sizeof(Vec4f)))
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, pass);
            else
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, fail);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glUnmapBuffer(GL_COPY_READ_BUFFER);
          }
          else if(buf == CoherentMapBufferRangeReadback)
          {
            if(ptrs[buf] && !memcmp(ptrs[buf], &green, sizeof(Vec4f)))
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, pass);
            else
              glBindBufferBase(GL_UNIFORM_BUFFER, 0, fail);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
          }
          else
          {
            // default, just make sure it has green data by rendering
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffers[buf]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
          }

          buf++;
        }
      }

      Present();

      glDeleteBuffers(1, &buffers[CleanBufferMapWriteInvalidate]);
      glDeleteBuffers(1, &buffers[CleanBufferMapWriteNonInvalidate]);
      glDeleteBuffers(1, &buffers[CleanBufferMapFlushExplicit]);
    }

    // unmap any persistent buffers
    for(int i = 0; i < TestCount; i++)
    {
      if(ptrs[i])
      {
        glBindBuffer(GL_UNIFORM_BUFFER, buffers[i]);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
      }
    }

    return 0;
  }
};

REGISTER_TEST();
