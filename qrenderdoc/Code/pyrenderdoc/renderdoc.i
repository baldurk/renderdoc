%module(docstring="This is the API to RenderDoc's internals.") renderdoc

%feature("autodoc", "0");

// just define linux platform to make sure things compile with no extra __declspec attributes
#define RENDERDOC_PLATFORM_LINUX

// we don't need these for the interface, they just confuse things
#define NO_ENUM_CLASS_OPERATORS

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text

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

// strip off the RENDERDOC_ namespace prefix, it's unnecessary
%rename("%(strip:[RENDERDOC_])s") "";

%fragment("pyconvert", "header") {
  static char convert_error[1024] = {};

  %#include "Code/pyrenderdoc/pyconversion.h"
}

%fragment("tempalloc", "header") {
  template<typename T, bool is_pointer = std::is_pointer<T>::value>
  struct pointer_unwrap;

  template<typename T>
  struct pointer_unwrap<T, false>
  {
    static void tempset(T &ptr, T *tempobj)
    {
    }

    static void tempalloc(T &ptr, unsigned char *tempmem)
    {
    }

    static void tempdealloc(T &ptr)
    {
    }

    static T &indirect(T &ptr)
    {
      return ptr;
    }
  };

  template<typename T>
  struct pointer_unwrap<T, true>
  {
    typedef typename std::remove_pointer<T>::type U;

    static void tempset(U *&ptr, U *tempobj)
    {
      ptr = tempobj;
    }

    static void tempalloc(U *&ptr, unsigned char *tempmem)
    {
      ptr = new (tempmem) U;
    }

    static void tempdealloc(U *ptr)
    {
      ptr->~U();
    }

    static U &indirect(U *ptr)
    {
      return *ptr;
    }
  };

  template<typename T>
  void tempalloc(T &ptr, unsigned char *tempmem)
  {
    pointer_unwrap<T>::tempalloc(ptr, tempmem);
  }

  template<typename T, typename U>
  void tempset(T &ptr, U *tempobj)
  {
    pointer_unwrap<T>::tempset(ptr, tempobj);
  }

  template<typename T>
  void tempdealloc(T ptr)
  {
    pointer_unwrap<T>::tempdealloc(ptr);
  }

  template<typename T>
  typename std::remove_pointer<T>::type &indirect(T &ptr)
  {
    return pointer_unwrap<T>::indirect(ptr);
  }
}

%define SIMPLE_TYPEMAPS_VARIANT(BaseType, SimpleType)
%typemap(in, fragment="tempalloc,pyconvert") SimpleType (BaseType temp) {
  tempset($1, &temp);

  int res = Convert($input, indirect($1));
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}

%typemap(out, fragment="tempalloc,pyconvert") SimpleType {
  $result = Convert(indirect($1));
}
%enddef

%define SIMPLE_TYPEMAPS(SimpleType)

SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType *)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType &)

%enddef

%define CONTAINER_TYPEMAPS(ContainerType)

%typemap(in, fragment="tempalloc,pyconvert") ContainerType (unsigned char tempmem[32]) {
  static_assert(sizeof(tempmem) >= sizeof(std::remove_pointer<decltype($1)>::type), "not enough temp space for $1_basetype");
  
  if(!PyList_Check($input))
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' list expected for argument $argnum of type '$1_basetype'"); 
  }

  tempalloc($1, tempmem);

  int failIdx = 0;
  int res = TypeConversion<std::remove_pointer<decltype($1)>::type>::Convert($input, indirect($1), &failIdx);

  if(!SWIG_IsOK(res))
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ArgError(res), convert_error); 
  }
}

%typemap(freearg, fragment="tempalloc") ContainerType {
  tempdealloc($1);
}

%typemap(argout, fragment="tempalloc,pyconvert") ContainerType {
  // empty the previous contents
  if(PyDict_Check($input))
  {
    PyDict_Clear($input);
  }
  else
  {
    Py_ssize_t sz = PySequence_Size($input);
    if(sz > 0)
      PySequence_DelSlice($input, 0, sz);
  }

  // overwrite with array contents
  int failIdx = 0;
  PyObject *res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertInPlace($input, indirect($1), &failIdx);

  if(!res)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error); 
  }
}

%typemap(out, fragment="tempalloc,pyconvert") ContainerType {
  int failIdx = 0;
  $result = TypeConversion<std::remove_pointer<$1_basetype>::type>::Convert(indirect($1), &failIdx);
  if(!$result)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' returning type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error);
  }
}

%enddef

SIMPLE_TYPEMAPS(rdctype::str)

CONTAINER_TYPEMAPS(rdctype::arr)

%typemap(in, fragment="pyconvert") std::function {
  PyObject *func = $input;
  failed$argnum = false;
  $1 = ConvertFunc<$1_ltype>("$symname", func, failed$argnum);
}

%typemap(argout) std::function (bool failed) {
  if(failed) SWIG_fail;
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

%header %{
  #include <set>
%}

%init %{
  // verify that docstrings aren't duplicated, which is a symptom of missing DOCUMENT()
  // macros around newly added classes/members.
  #if !defined(RELEASE)
  static bool doc_checked = false;

  if(!doc_checked)
  {
    doc_checked = true;

    std::set<std::string> docstrings;
    for(size_t i=0; i < sizeof(swig_type_initial)/sizeof(swig_type_initial[0]); i++)
    {
      SwigPyClientData *typeinfo = (SwigPyClientData *)swig_type_initial[i]->clientdata;

      // opaque types have no typeinfo, skip these
      if(!typeinfo) continue;

      PyTypeObject *typeobj = typeinfo->pytype;

      std::string typedoc = typeobj->tp_doc;

      auto result = docstrings.insert(typedoc);

      if(!result.second)
      {
        snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on struct '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), typeobj->tp_name);
        RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
      }

      PyMethodDef *method = typeobj->tp_methods;

      while(method->ml_doc)
      {
        std::string typedoc = method->ml_doc;

        size_t i = 0;
        while(typedoc[i] == '\n')
          i++;

        // skip the first line as it's autodoc generated
        i = typedoc.find('\n', i);
        if(i != std::string::npos)
        {
          while(typedoc[i] == '\n')
            i++;

          typedoc.erase(0, i);

          result = docstrings.insert(typedoc);

          if(!result.second)
          {
            snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on method '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), method->ml_name);
            RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
          }
        }

        method++;
      }
    }
  }
  #endif
%}
