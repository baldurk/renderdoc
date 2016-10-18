#include "gles_driver.h"

struct FramebufferAttachmentData
{
  bool renderbuffer;
  bool layered;
  int32_t layer;
  int32_t level;
  ResourceId obj;
};

struct FramebufferInitialData
{
  bool valid;
  FramebufferAttachmentData Attachments[10];
  GLenum DrawBuffers[8];
  GLenum ReadBuffer;

  static const GLenum attachmentNames[10];
};

template <>
void Serialiser::Serialise(const char *name, FramebufferInitialData &el);

void WrappedGLES::dumpCurrentState(const char * filename)
{
    static int counter = 0;

    Serialiser debugSerialiser((filename + std::to_string(counter++)).c_str(), Serialiser::WRITING, true);
    debugSerialiser.SetDebugText(true);
    GLRenderState debugRenderState(&m_Real, &debugSerialiser, WRITING);
    debugRenderState.FetchState(GetCtx(), this);
    GLResourceManager* rm = GetResourceManager();

    GLint readBinding, drawBinding;
    m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &readBinding);
    m_Real.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &drawBinding);


    FramebufferInitialData readFDBData;
    rm->Prepare_InitialState(FramebufferRes(GetCtx(), readBinding), (unsigned char*)&readFDBData);
    debugSerialiser.Serialise("FBInitialData(Read)", readFDBData);

    if (readBinding != drawBinding)
    {
      FramebufferInitialData drawFDBData;
      rm->Prepare_InitialState(FramebufferRes(GetCtx(), drawBinding), (unsigned char*)&drawFDBData);
      debugSerialiser.Serialise("FBInitialData(Draw)", drawFDBData);
    }

    debugRenderState.Serialise(WRITING, GetCtx(), this);

    debugSerialiser.FlushToDisk();

}
