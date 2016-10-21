#include "gles_driver.h"
#include <fstream>

struct VertexAttribInitialData
{
  uint32_t enabled;
  uint32_t vbslot;
  uint32_t offset;
  GLenum type;
  int32_t normalized;
  uint32_t integer;
  uint32_t size;
};

struct VertexBufferInitialData
{
  ResourceId Buffer;
  uint64_t Stride;
  uint64_t Offset;
  uint32_t Divisor;
};

struct VAOInitialData
{
  bool valid;
  VertexAttribInitialData VertexAttribs[16];
  VertexBufferInitialData VertexBuffers[16];
  ResourceId ElementArrayBuffer;
};

struct FeedbackInitialData
{
  bool valid;
  ResourceId Buffer[4];
  uint64_t Offset[4];
  uint64_t Size[4];
};

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
void Serialiser::Serialise(const char *name, FeedbackInitialData &el);

template <>
void Serialiser::Serialise(const char *name, FramebufferInitialData &el);

template <>
void Serialiser::Serialise(const char *name, VertexAttribInitialData &el);

template <>
void Serialiser::Serialise(const char *name, VertexBufferInitialData &el);


void WrappedGLES::dumpCurrentState(const char * filename)
{
    static int counter = 0;
    char outputFilename[256];
    snprintf(outputFilename, 254, "%s_%d", filename, counter++);

    //Serialiser debugSerialiser((filename + std::to_string(counter++)).c_str(), Serialiser::WRITING, true);
    Serialiser debugSerialiser("", Serialiser::WRITING, true);
    debugSerialiser.SetDebugText(true);
    GLResourceManager* rm = GetResourceManager();
    // --------------- Framebuffer ----------------------------------------------------
    {
      GLint readFBO, drawFBO;
      m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &readFBO);
      m_Real.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);

      {
        ScopedContext VAOScope(&debugSerialiser, "Initial contents", "FramebufferInitialData (Read)", 0, true);
        debugSerialiser.Serialise("FBO Read ID", readFBO);
        FramebufferInitialData readFBOData;
        rm->Prepare_InitialState(FramebufferRes(GetCtx(), readFBO), (unsigned char*)&readFBOData);
        debugSerialiser.Serialise("FBOInitialData(Read)", readFBOData);
      }

      if (readFBO != drawFBO)
      {
        ScopedContext VAOScope(&debugSerialiser, "Initial contents", "FramebufferInitialData (Draw)", 0, true);
        debugSerialiser.Serialise("FBO Draw ID", drawFBO);
        FramebufferInitialData drawFBOData;
        rm->Prepare_InitialState(FramebufferRes(GetCtx(), drawFBO), (unsigned char*)&drawFBOData);
        debugSerialiser.Serialise("FBOInitialData(Draw)", drawFBOData);
      }
    }
    // -------------------- VAO ---------------------------------------------------------
    {
      GLint VAO;
      m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
      ScopedContext VAOScope(&debugSerialiser, "Initial contents", "VAOInitialData", 0, true);
      debugSerialiser.Serialise("VAO ID", VAO);

      VAOInitialData VAOData;
      rm->Prepare_InitialState(VertexArrayRes(GetCtx(), VAO), (unsigned char*)&VAOData);
      debugSerialiser.Serialise("valid", VAOData.valid);
      for(GLuint i = 0; i < 16; i++)
      {
        debugSerialiser.Serialise("VertexAttrib[]", VAOData.VertexAttribs[i]);
        debugSerialiser.Serialise("VertexBuffer[]", VAOData.VertexBuffers[i]);
      }
      debugSerialiser.Serialise("ElementArrayBuffer", VAOData.ElementArrayBuffer);
    }

    // -------------------- TransformFeedback -------------------------------------------
    {
      GLint TFO;
      m_Real.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&TFO);
      ScopedContext VAOScope(&debugSerialiser, "Initial contents", "FeedbackInitialData", 0, true);
      debugSerialiser.Serialise("TFO ID", TFO);

      FeedbackInitialData TFOData;
      rm->Prepare_InitialState(FeedbackRes(GetCtx(), TFO), (unsigned char*)&TFOData);
      debugSerialiser.Serialise("TFOInitialData", TFOData);
    }
    // ----------------------------------------------------------------------------------
    {
      ScopedContext VAOScope(&debugSerialiser, "Render state", "GLRenderState", 0, true);
      GLRenderState debugRenderState(&m_Real, &debugSerialiser, WRITING);
      debugRenderState.FetchState(GetCtx(), this);
      debugRenderState.Serialise(WRITING, GetCtx(), this);
    }

    std::ofstream ofs (outputFilename, std::ofstream::out);
    if (ofs)
    {
      ofs << debugSerialiser.GetDebugStr();
      ofs.close();
    }
}
