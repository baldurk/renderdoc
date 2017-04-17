%module(docstring="This is the API to RenderDoc's internals.") renderdoc

%feature("autodoc");

// just define linux platform to make sure things compile with no extra __declspec attributes
#define RENDERDOC_PLATFORM_LINUX

// we don't need these for the interface, they just confuse things
#define NO_ENUM_CLASS_OPERATORS

// ignore warning about base class rdctype::array<char> methods in rdctype::str
#pragma SWIG nowarn=401

// ignore warning about redundant declaration of typedef (byte/bool32)
#pragma SWIG nowarn=322

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

// Since SWIG will inline all namespaces, and doesn't support nested structs, the namespaces
// for each pipeline state causes conflicts. We just fall back to a rename with _ as that's
// still acceptable/intuitive.

%rename(D3D11_Layout) D3D11Pipe::Layout;
%rename(D3D11_VB) D3D11Pipe::VB;
%rename(D3D11_IB) D3D11Pipe::IB;
%rename(D3D11_IA) D3D11Pipe::IA;
%rename(D3D11_View) D3D11Pipe::View;
%rename(D3D11_Sampler) D3D11Pipe::Sampler;
%rename(D3D11_CBuffer) D3D11Pipe::CBuffer;
%rename(D3D11_Shader) D3D11Pipe::Shader;
%rename(D3D11_SOBind) D3D11Pipe::SOBind;
%rename(D3D11_SO) D3D11Pipe::SO;
%rename(D3D11_Viewport) D3D11Pipe::Viewport;
%rename(D3D11_Scissor) D3D11Pipe::Scissor;
%rename(D3D11_RasterizerState) D3D11Pipe::RasterizerState;
%rename(D3D11_Rasterizer) D3D11Pipe::Rasterizer;
%rename(D3D11_DepthStencilState) D3D11Pipe::DepthStencilState;
%rename(D3D11_StencilOp) D3D11Pipe::StencilOp;
%rename(D3D11_Blend) D3D11Pipe::Blend;
%rename(D3D11_BlendOp) D3D11Pipe::BlendOp;
%rename(D3D11_BlendState) D3D11Pipe::BlendState;
%rename(D3D11_OM) D3D11Pipe::OM;
%rename(D3D11_State) D3D11Pipe::State;

%rename(GL_VertexAttribute) GLPipe::VertexAttribute;
%rename(GL_VB) GLPipe::VB;
%rename(GL_VertexInput) GLPipe::VertexInput;
%rename(GL_Shader) GLPipe::Shader;
%rename(GL_FixedVertexProcessing) GLPipe::FixedVertexProcessing;
%rename(GL_Texture) GLPipe::Texture;
%rename(GL_Sampler) GLPipe::Sampler;
%rename(GL_Buffer) GLPipe::Buffer;
%rename(GL_ImageLoadStore) GLPipe::ImageLoadStore;
%rename(GL_Feedback) GLPipe::Feedback;
%rename(GL_Viewport) GLPipe::Viewport;
%rename(GL_Scissor) GLPipe::Scissor;
%rename(GL_RasterizerState) GLPipe::RasterizerState;
%rename(GL_Rasterizer) GLPipe::Rasterizer;
%rename(GL_DepthState) GLPipe::DepthState;
%rename(GL_StencilOp) GLPipe::StencilOp;
%rename(GL_StencilState) GLPipe::StencilState;
%rename(GL_Attachment) GLPipe::Attachment;
%rename(GL_FBO) GLPipe::FBO;
%rename(GL_BlendOp) GLPipe::BlendOp;
%rename(GL_Blend) GLPipe::Blend;
%rename(GL_BlendState) GLPipe::BlendState;
%rename(GL_FrameBuffer) GLPipe::FrameBuffer;
%rename(GL_Hints) GLPipe::Hints;
%rename(GL_State) GLPipe::State;

%rename(VK_BindingElement) VKPipe::BindingElement;
%rename(VK_DescriptorBinding) VKPipe::DescriptorBinding;
%rename(VK_DescriptorSet) VKPipe::DescriptorSet;
%rename(VK_Pipeline) VKPipe::Pipeline;
%rename(VK_IB) VKPipe::IB;
%rename(VK_InputAssembly) VKPipe::InputAssembly;
%rename(VK_VertexAttribute) VKPipe::VertexAttribute;
%rename(VK_VertexBinding) VKPipe::VertexBinding;
%rename(VK_VB) VKPipe::VB;
%rename(VK_VertexInput) VKPipe::VertexInput;
%rename(VK_SpecInfo) VKPipe::SpecInfo;
%rename(VK_Shader) VKPipe::Shader;
%rename(VK_Tessellation) VKPipe::Tessellation;
%rename(VK_Viewport) VKPipe::Viewport;
%rename(VK_Scissor) VKPipe::Scissor;
%rename(VK_ViewportScissor) VKPipe::ViewportScissor;
%rename(VK_ViewState) VKPipe::ViewState;
%rename(VK_Raster) VKPipe::Raster;
%rename(VK_MultiSample) VKPipe::MultiSample;
%rename(VK_BlendOp) VKPipe::BlendOp;
%rename(VK_Blend) VKPipe::Blend;
%rename(VK_ColorBlend) VKPipe::ColorBlend;
%rename(VK_StencilOp) VKPipe::StencilOp;
%rename(VK_DepthStencil) VKPipe::DepthStencil;
%rename(VK_RenderPass) VKPipe::RenderPass;
%rename(VK_Attachment) VKPipe::Attachment;
%rename(VK_Framebuffer) VKPipe::Framebuffer;
%rename(VK_RenderArea) VKPipe::RenderArea;
%rename(VK_CurrentPass) VKPipe::CurrentPass;
%rename(VK_ImageLayout) VKPipe::ImageLayout;
%rename(VK_ImageData) VKPipe::ImageData;
%rename(VK_State) VKPipe::State;

%rename(D3D12_Layout) D3D12Pipe::Layout;
%rename(D3D12_VB) D3D12Pipe::VB;
%rename(D3D12_IB) D3D12Pipe::IB;
%rename(D3D12_IA) D3D12Pipe::IA;
%rename(D3D12_View) D3D12Pipe::View;
%rename(D3D12_Sampler) D3D12Pipe::Sampler;
%rename(D3D12_CBuffer) D3D12Pipe::CBuffer;
%rename(D3D12_RegisterSpace) D3D12Pipe::RegisterSpace;
%rename(D3D12_Shader) D3D12Pipe::Shader;
%rename(D3D12_SOBind) D3D12Pipe::SOBind;
%rename(D3D12_Streamout) D3D12Pipe::Streamout;
%rename(D3D12_Viewport) D3D12Pipe::Viewport;
%rename(D3D12_Scissor) D3D12Pipe::Scissor;
%rename(D3D12_RasterizerState) D3D12Pipe::RasterizerState;
%rename(D3D12_Rasterizer) D3D12Pipe::Rasterizer;
%rename(D3D12_StencilOp) D3D12Pipe::StencilOp;
%rename(D3D12_DepthStencilState) D3D12Pipe::DepthStencilState;
%rename(D3D12_BlendOp) D3D12Pipe::BlendOp;
%rename(D3D12_Blend) D3D12Pipe::Blend;
%rename(D3D12_BlendState) D3D12Pipe::BlendState;
%rename(D3D12_OM) D3D12Pipe::OM;
%rename(D3D12_ResourceState) D3D12Pipe::ResourceState;
%rename(D3D12_ResourceData) D3D12Pipe::ResourceData;
%rename(D3D12_State) D3D12Pipe::State;

%fragment("pyconvert", "header") {
  static char convert_error[1024] = {};

  %#include "Code/pyrenderdoc/pyconversion.h"
}

%fragment("tempalloc", "header") {
  template<typename T>
  void tempalloc(T *&ptr, unsigned char *tempmem)
  {
    ptr = new (tempmem) T;
  }
}

%typemap(in, fragment="tempalloc,pyconvert") rdctype::array<T> * (unsigned char tempmem[sizeof(void*)*2]) {
  static_assert(sizeof(tempmem) >= sizeof(*$1), "sizeof rdctype::array isn't equal");
  
  if(!PyList_Check($input))
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' list expected for argument $argnum of type '$1_basetype'"); 
  }

  tempalloc($1, tempmem);

  int failIdx = 0;
  int res = TypeConversion<std::remove_pointer<decltype($1)>::type>::Convert($input, *$1, &failIdx);

  if(!SWIG_IsOK(res))
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ArgError(res), convert_error); 
  }
}

%typemap(freearg) rdctype::array<T> * {
  $1->Delete();
}

%typemap(argout, fragment="pyconvert") rdctype::array<T> * {
  // empty the previous contents
  Py_ssize_t sz = PyList_Size($input);
  if(sz > 0)
    PySequence_DelSlice($input, 0, sz);

  // overwrite with array contents
  int failIdx = 0;
  PyObject *res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertList($input, *$1, &failIdx);

  if(!res)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error); 
  }
}

%typemap(out, fragment="pyconvert") rdctype::array<T> * {
  int failIdx = 0;
  $result = TypeConversion<std::remove_pointer<decltype($1)>::type>::Convert(*$1, &failIdx);
  if(!result)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' returning type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error);
  }
}

%typemap(in, fragment="pyconvert") rdctype::str * {
  $1 = new rdctype::str;
  int res = Convert($input, *$1);
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}
%typemap(freearg) rdctype::str * {
  delete $1;
}

%typemap(in, fragment="pyconvert") rdctype::str {
  int res = Convert($input, $1);
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}

%typemap(out, fragment="pyconvert") rdctype::str * {
  $result = Convert(*$1);
}

%typemap(out, fragment="pyconvert") rdctype::str {
  $result = Convert($1);
}

// ignore some operators SWIG doesn't have to worry about
%ignore rdctype::array::operator=;
%ignore rdctype::array::operator[];
%ignore rdctype::str::operator=;
%ignore rdctype::str::operator const char *;

// SWIG generates destructor wrappers for these interfaces that we don't want
%ignore IReplayOutput::~IReplayOutput();
%ignore IReplayRenderer::~IReplayRenderer();
%ignore ITargetControl::~ITargetControl();
%ignore IRemoteServer::~IRemoteServer();

%{
  #include "renderdoc_replay.h"
%}

%include <stdint.i>

%include "renderdoc_replay.h"
%include "basic_types.h"
%include "capture_options.h"
%include "control_types.h"
%include "d3d11_pipestate.h"
%include "d3d12_pipestate.h"
%include "data_types.h"
%include "gl_pipestate.h"
%include "replay_enums.h"
%include "shader_types.h"
%include "vk_pipestate.h"

// add a built-in __str__ function that will generate string representations in python
%extend rdctype::str {
  const char *__str__() const { return $self->c_str(); }
};

// declare a function for passing external objects into python
%wrapper %{

PyObject *PassObjectToPython(const char *type, void *obj)
{
  swig_type_info *t = SWIG_TypeQuery(type);
  if(t == NULL)
    return NULL;

  return SWIG_InternalNewPointerObj(obj, t, 0);
}

%}

