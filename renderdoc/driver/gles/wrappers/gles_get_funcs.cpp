#include "../gles_driver.h"

GLenum WrappedGLES::glGetError()
{
    return m_Real.glGetError();
}
