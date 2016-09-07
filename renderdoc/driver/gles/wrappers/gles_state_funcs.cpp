
#include "../gles_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"
#include "serialise/serialiser.h"

#include "../gles_chunks.h"

bool WrappedGLES::Serialise_glClear(GLbitfield mask)
{
    printf("CALL: %s\n", __FUNCTION__);

    SERIALISE_ELEMENT(uint32_t, Mask, mask);

    if (m_State <= EXECUTING)
    {
        printf("Executing real %s(%d)\n", "glClear", Mask);
        m_Real.glClear(Mask);
    }

    return true;
}

void WrappedGLES::glClear(GLbitfield mask)
{
    printf("CALL: %s\n", __FUNCTION__);
    m_Real.glClear(mask);
    if (m_State == WRITING_CAPFRAME)
    {
        SCOPED_SERIALISE_CONTEXT(CLEAR);
        Serialise_glClear(mask);

        m_ContextRecord->AddChunk(scope.Get());
    }
}

bool WrappedGLES::Serialise_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  printf("CALL: %s\n", __FUNCTION__);
  SERIALISE_ELEMENT(float, r, red);
  SERIALISE_ELEMENT(float, g, green);
  SERIALISE_ELEMENT(float, b, blue);
  SERIALISE_ELEMENT(float, a, alpha);

  if (m_State <= EXECUTING)
  {
    printf("Executing real %s(%f, %f, %f, %f)\n", "glClearColor", r, g, b, a);
    m_Real.glClearColor(r, g, b, a);
  }

  return true;
}

void WrappedGLES::glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    printf("CALL: %s\n", __FUNCTION__);
    m_Real.glClearColor(red, green, blue, alpha);

    if(m_State == WRITING_CAPFRAME)
    {
        SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
        Serialise_glClearColor(red, green, blue, alpha);

        m_ContextRecord->AddChunk(scope.Get());
    }
}

bool WrappedGLES::Serialise_glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    SERIALISE_ELEMENT(int32_t, X, x);
    SERIALISE_ELEMENT(int32_t, Y, y);
    SERIALISE_ELEMENT(uint32_t, W, width);
    SERIALISE_ELEMENT(uint32_t, H, height);

    if(m_State <= EXECUTING)
    {
        m_Real.glViewport(X, Y, W, H);
    }

    return true;
}


void WrappedGLES::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    printf("CALL: %s\n", __FUNCTION__);
    m_Real.glViewport(x, y, width, height);

    if(m_State == WRITING_CAPFRAME)
    {
        SCOPED_SERIALISE_CONTEXT(VIEWPORT);
        Serialise_glViewport(x, y, width, height);

        m_ContextRecord->AddChunk(scope.Get());
    }
}
