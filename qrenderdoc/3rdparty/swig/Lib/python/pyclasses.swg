#ifdef __cplusplus

/*
  SwigPtr_PyObject is used as a replacement of PyObject *, where
  the INCREF/DECREF are applied as needed.

  You can use SwigPtr_PyObject in a container, such as
  
     std::vector<SwigPtr_PyObject>;

  or as a member variable:
  
     struct A {
       SwigPtr_PyObject obj;
       A(PyObject *o) : _obj(o) {
       }
     };

   or as a input/output value 

     SwigPtr_PyObject func(SwigPtr_PyObject obj) {     
       SwigPtr_PyObject out = PyString_FromFormat("hello %s", PyObject_AsString(obj));
       Py_DECREF(out);
       return out;
     }

   just remember to pair the object creation with the proper DECREF,
   the same as with plain PyObject *ptr, since SwigPtr_PyObject always add
   one reference at construction.

   SwigPtr_PyObject is 'visible' at the wrapped side, so you can do:


      %template(pyvector) std::vector<swig::SwigPtr_PyObject>;

   and all the proper typemaps will be used.
   
*/

namespace swig {
  %ignore SwigPtr_PyObject;
  struct SwigPtr_PyObject {};
  %apply PyObject * {SwigPtr_PyObject};
  %apply PyObject * const& {SwigPtr_PyObject const&};

  %typemap(typecheck,precedence=SWIG_TYPECHECK_SWIGOBJECT,noblock=1) SwigPtr_PyObject const& "$1 = ($input != 0);";


  /* For output */
  %typemap(out,noblock=1)  SwigPtr_PyObject {
    $result = (PyObject *)$1;
    Py_INCREF($result);
  }
  
  %typemap(out,noblock=1)  SwigPtr_PyObject const & {
    $result = (PyObject *)*$1;
    Py_INCREF($result);
  }
  
}

%{
namespace swig {
  class SwigPtr_PyObject {
  protected:
    PyObject *_obj;

  public:
    SwigPtr_PyObject() :_obj(0)
    {
    }

    SwigPtr_PyObject(const SwigPtr_PyObject& item) : _obj(item._obj)
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      Py_XINCREF(_obj);      
      SWIG_PYTHON_THREAD_END_BLOCK;
    }
    
    SwigPtr_PyObject(PyObject *obj, bool initial_ref = true) :_obj(obj)
    {
      if (initial_ref) {
        SWIG_PYTHON_THREAD_BEGIN_BLOCK;
        Py_XINCREF(_obj);
        SWIG_PYTHON_THREAD_END_BLOCK;
      }
    }
    
    SwigPtr_PyObject & operator=(const SwigPtr_PyObject& item) 
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      Py_XINCREF(item._obj);
      Py_XDECREF(_obj);
      _obj = item._obj;
      SWIG_PYTHON_THREAD_END_BLOCK;
      return *this;      
    }
    
    ~SwigPtr_PyObject() 
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      Py_XDECREF(_obj);
      SWIG_PYTHON_THREAD_END_BLOCK;
    }
    
    operator PyObject *() const
    {
      return _obj;
    }

    PyObject *operator->() const
    {
      return _obj;
    }
  };
}
%}

/*
  SwigVar_PyObject is used to manage 'in the scope' PyObject * variables,
  as in

  int func () {
    SwigVar_PyObject obj = PyString_FromString("hello");
  }

  ie, 'obj' is created and destructed in the same scope from
  a python object that carries at least one reference value.
  
  SwigVar_PyObject just take care of applying the proper Py_DECREF.

  Hence, this class is purely internal and not visible at the wrapped side.
 */
namespace swig {
  %ignore SwigVar_PyObject;
  struct SwigVar_PyObject {};
  %apply PyObject * {SwigVar_PyObject};
  %apply PyObject * const& {SwigVar_PyObject const&};
}

%{
namespace swig {
  struct SwigVar_PyObject : SwigPtr_PyObject {
    SwigVar_PyObject(PyObject* obj = 0) : SwigPtr_PyObject(obj, false) { }
    
    SwigVar_PyObject & operator = (PyObject* obj)
    {
      Py_XDECREF(_obj);
      _obj = obj;
      return *this;      
    }
  };
}
%}


#endif
