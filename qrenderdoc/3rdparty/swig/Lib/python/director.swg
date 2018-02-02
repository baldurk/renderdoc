/* -----------------------------------------------------------------------------
 * director.swg
 *
 * This file contains support for director classes so that Python proxy
 * methods can be called from C++.
 * ----------------------------------------------------------------------------- */

#ifndef SWIG_DIRECTOR_PYTHON_HEADER_
#define SWIG_DIRECTOR_PYTHON_HEADER_

#include <string>
#include <iostream>
#include <exception>
#include <vector>
#include <map>


/*
  Use -DSWIG_PYTHON_DIRECTOR_NO_VTABLE if you don't want to generate a 'virtual
  table', and avoid multiple GetAttr calls to retrieve the python
  methods.
*/

#ifndef SWIG_PYTHON_DIRECTOR_NO_VTABLE
#ifndef SWIG_PYTHON_DIRECTOR_VTABLE
#define SWIG_PYTHON_DIRECTOR_VTABLE
#endif
#endif



/*
  Use -DSWIG_DIRECTOR_NO_UEH if you prefer to avoid the use of the
  Undefined Exception Handler provided by swig.
*/
#ifndef SWIG_DIRECTOR_NO_UEH
#ifndef SWIG_DIRECTOR_UEH
#define SWIG_DIRECTOR_UEH
#endif
#endif


/*
  Use -DSWIG_DIRECTOR_NORTTI if you prefer to avoid the use of the
  native C++ RTTI and dynamic_cast<>. But be aware that directors
  could stop working when using this option.
*/
#ifdef SWIG_DIRECTOR_NORTTI
/*
   When we don't use the native C++ RTTI, we implement a minimal one
   only for Directors.
*/
# ifndef SWIG_DIRECTOR_RTDIR
# define SWIG_DIRECTOR_RTDIR

namespace Swig {
  class Director;
  SWIGINTERN std::map<void *, Director *>& get_rtdir_map() {
    static std::map<void *, Director *> rtdir_map;
    return rtdir_map;
  }

  SWIGINTERNINLINE void set_rtdir(void *vptr, Director *rtdir) {
    get_rtdir_map()[vptr] = rtdir;
  }

  SWIGINTERNINLINE Director *get_rtdir(void *vptr) {
    std::map<void *, Director *>::const_iterator pos = get_rtdir_map().find(vptr);
    Director *rtdir = (pos != get_rtdir_map().end()) ? pos->second : 0;
    return rtdir;
  }
}
# endif /* SWIG_DIRECTOR_RTDIR */

# define SWIG_DIRECTOR_CAST(ARG) Swig::get_rtdir(static_cast<void *>(ARG))
# define SWIG_DIRECTOR_RGTR(ARG1, ARG2) Swig::set_rtdir(static_cast<void *>(ARG1), ARG2)

#else

# define SWIG_DIRECTOR_CAST(ARG) dynamic_cast<Swig::Director *>(ARG)
# define SWIG_DIRECTOR_RGTR(ARG1, ARG2)

#endif /* SWIG_DIRECTOR_NORTTI */

extern "C" {
  struct swig_type_info;
}

namespace Swig {

  /* memory handler */
  struct GCItem {
    virtual ~GCItem() {}

    virtual int get_own() const {
      return 0;
    }
  };

  struct GCItem_var {
    GCItem_var(GCItem *item = 0) : _item(item) {
    }

    GCItem_var& operator=(GCItem *item) {
      GCItem *tmp = _item;
      _item = item;
      delete tmp;
      return *this;
    }

    ~GCItem_var() {
      delete _item;
    }

    GCItem * operator->() const {
      return _item;
    }

  private:
    GCItem *_item;
  };

  struct GCItem_Object : GCItem {
    GCItem_Object(int own) : _own(own) {
    }

    virtual ~GCItem_Object() {
    }

    int get_own() const {
      return _own;
    }

  private:
    int _own;
  };

  template <typename Type>
  struct GCItem_T : GCItem {
    GCItem_T(Type *ptr) : _ptr(ptr) {
    }

    virtual ~GCItem_T() {
      delete _ptr;
    }

  private:
    Type *_ptr;
  };

  template <typename Type>
  struct GCArray_T : GCItem {
    GCArray_T(Type *ptr) : _ptr(ptr) {
    }

    virtual ~GCArray_T() {
      delete[] _ptr;
    }

  private:
    Type *_ptr;
  };

  /* base class for director exceptions */
  class DirectorException : public std::exception {
  protected:
    std::string swig_msg;
  public:
    DirectorException(PyObject *error, const char *hdr ="", const char *msg ="") : swig_msg(hdr) {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      if (msg[0]) {
        swig_msg += " ";
        swig_msg += msg;
      }
      if (!PyErr_Occurred()) {
        PyErr_SetString(error, what());
      }
      SWIG_PYTHON_THREAD_END_BLOCK;
    }

    virtual ~DirectorException() throw() {
    }

    /* Deprecated, use what() instead */
    const char *getMessage() const {
      return what();
    }

    const char *what() const throw() {
      return swig_msg.c_str();
    }

    static void raise(PyObject *error, const char *msg) {
      throw DirectorException(error, msg);
    }

    static void raise(const char *msg) {
      raise(PyExc_RuntimeError, msg);
    }
  };

  /* unknown exception handler  */
  class UnknownExceptionHandler {
#ifdef SWIG_DIRECTOR_UEH
    static void handler() {
      try {
        throw;
      } catch (DirectorException& e) {
        std::cerr << "SWIG Director exception caught:" << std::endl
                  << e.what() << std::endl;
      } catch (std::exception& e) {
        std::cerr << "std::exception caught: "<< e.what() << std::endl;
      } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
      }

      std::cerr << std::endl
                << "Python interpreter traceback:" << std::endl;
      PyErr_Print();
      std::cerr << std::endl;

      std::cerr << "This exception was caught by the SWIG unexpected exception handler." << std::endl
                << "Try using %feature(\"director:except\") to avoid reaching this point." << std::endl
                << std::endl
                << "Exception is being re-thrown, program will likely abort/terminate." << std::endl;
      throw;
    }

  public:

    std::unexpected_handler old;
    UnknownExceptionHandler(std::unexpected_handler nh = handler) {
      old = std::set_unexpected(nh);
    }

    ~UnknownExceptionHandler() {
      std::set_unexpected(old);
    }
#endif
  };

  /* type mismatch in the return value from a python method call */
  class DirectorTypeMismatchException : public DirectorException {
  public:
    DirectorTypeMismatchException(PyObject *error, const char *msg="")
      : DirectorException(error, "SWIG director type mismatch", msg) {
    }

    DirectorTypeMismatchException(const char *msg="")
      : DirectorException(PyExc_TypeError, "SWIG director type mismatch", msg) {
    }

    static void raise(PyObject *error, const char *msg) {
      throw DirectorTypeMismatchException(error, msg);
    }

    static void raise(const char *msg) {
      throw DirectorTypeMismatchException(msg);
    }
  };

  /* any python exception that occurs during a director method call */
  class DirectorMethodException : public DirectorException {
  public:
    DirectorMethodException(const char *msg = "")
      : DirectorException(PyExc_RuntimeError, "SWIG director method error.", msg) {
    }

    static void raise(const char *msg) {
      throw DirectorMethodException(msg);
    }
  };

  /* attempt to call a pure virtual method via a director method */
  class DirectorPureVirtualException : public DirectorException {
  public:
    DirectorPureVirtualException(const char *msg = "")
      : DirectorException(PyExc_RuntimeError, "SWIG director pure virtual method called", msg) {
    }

    static void raise(const char *msg) {
      throw DirectorPureVirtualException(msg);
    }
  };


#if defined(SWIG_PYTHON_THREADS)
/*  __THREAD__ is the old macro to activate some thread support */
# if !defined(__THREAD__)
#   define __THREAD__ 1
# endif
#endif

#ifdef __THREAD__
# include "pythread.h"
  class Guard {
    PyThread_type_lock &mutex_;

  public:
    Guard(PyThread_type_lock & mutex) : mutex_(mutex) {
      PyThread_acquire_lock(mutex_, WAIT_LOCK);
    }

    ~Guard() {
      PyThread_release_lock(mutex_);
    }
  };
# define SWIG_GUARD(mutex) Guard _guard(mutex)
#else
# define SWIG_GUARD(mutex)
#endif

  /* director base class */
  class Director {
  private:
    /* pointer to the wrapped python object */
    PyObject *swig_self;
    /* flag indicating whether the object is owned by python or c++ */
    mutable bool swig_disown_flag;

    /* decrement the reference count of the wrapped python object */
    void swig_decref() const {
      if (swig_disown_flag) {
        SWIG_PYTHON_THREAD_BEGIN_BLOCK;
        Py_DECREF(swig_self);
        SWIG_PYTHON_THREAD_END_BLOCK;
      }
    }

  public:
    /* wrap a python object. */
    Director(PyObject *self) : swig_self(self), swig_disown_flag(false) {
    }

    /* discard our reference at destruction */
    virtual ~Director() {
      swig_decref();
    }

    /* return a pointer to the wrapped python object */
    PyObject *swig_get_self() const {
      return swig_self;
    }

    /* acquire ownership of the wrapped python object (the sense of "disown" is from python) */
    void swig_disown() const {
      if (!swig_disown_flag) {
        swig_disown_flag=true;
        swig_incref();
      }
    }

    /* increase the reference count of the wrapped python object */
    void swig_incref() const {
      if (swig_disown_flag) {
        Py_INCREF(swig_self);
      }
    }

    /* methods to implement pseudo protected director members */
    virtual bool swig_get_inner(const char * /* swig_protected_method_name */) const {
      return true;
    }

    virtual void swig_set_inner(const char * /* swig_protected_method_name */, bool /* swig_val */) const {
    }

  /* ownership management */
  private:
    typedef std::map<void *, GCItem_var> swig_ownership_map;
    mutable swig_ownership_map swig_owner;
#ifdef __THREAD__
    static PyThread_type_lock swig_mutex_own;
#endif

  public:
    template <typename Type>
    void swig_acquire_ownership_array(Type *vptr) const {
      if (vptr) {
        SWIG_GUARD(swig_mutex_own);
        swig_owner[vptr] = new GCArray_T<Type>(vptr);
      }
    }

    template <typename Type>
    void swig_acquire_ownership(Type *vptr) const {
      if (vptr) {
        SWIG_GUARD(swig_mutex_own);
        swig_owner[vptr] = new GCItem_T<Type>(vptr);
      }
    }

    void swig_acquire_ownership_obj(void *vptr, int own) const {
      if (vptr && own) {
        SWIG_GUARD(swig_mutex_own);
        swig_owner[vptr] = new GCItem_Object(own);
      }
    }

    int swig_release_ownership(void *vptr) const {
      int own = 0;
      if (vptr) {
        SWIG_GUARD(swig_mutex_own);
        swig_ownership_map::iterator iter = swig_owner.find(vptr);
        if (iter != swig_owner.end()) {
          own = iter->second->get_own();
          swig_owner.erase(iter);
        }
      }
      return own;
    }

    template <typename Type>
    static PyObject *swig_pyobj_disown(PyObject *pyobj, PyObject *SWIGUNUSEDPARM(args)) {
      SwigPyObject *sobj = (SwigPyObject *)pyobj;
      sobj->own = 0;
      Director *d = SWIG_DIRECTOR_CAST(reinterpret_cast<Type *>(sobj->ptr));
      if (d)
        d->swig_disown();
      return PyWeakref_NewProxy(pyobj, NULL);
    }
  };

#ifdef __THREAD__
  PyThread_type_lock Director::swig_mutex_own = PyThread_allocate_lock();
#endif
}

#endif
