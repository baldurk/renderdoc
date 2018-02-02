/* Compatibility macros for Python 3 */
#if PY_VERSION_HEX >= 0x03000000

#define PyClass_Check(obj) PyObject_IsInstance(obj, (PyObject *)&PyType_Type)
#define PyInt_Check(x) PyLong_Check(x)
#define PyInt_AsLong(x) PyLong_AsLong(x)
#define PyInt_FromLong(x) PyLong_FromLong(x)
#define PyInt_FromSize_t(x) PyLong_FromSize_t(x)
#define PyString_Check(name) PyBytes_Check(name)
#define PyString_FromString(x) PyUnicode_FromString(x)
#define PyString_Format(fmt, args)  PyUnicode_Format(fmt, args)
#define PyString_AsString(str) PyBytes_AsString(str)
#define PyString_Size(str) PyBytes_Size(str)	
#define PyString_InternFromString(key) PyUnicode_InternFromString(key)
#define Py_TPFLAGS_HAVE_CLASS Py_TPFLAGS_BASETYPE
#define PyString_AS_STRING(x) PyUnicode_AS_STRING(x)
#define _PyLong_FromSsize_t(x) PyLong_FromSsize_t(x)

#endif

#ifndef Py_TYPE
#  define Py_TYPE(op) ((op)->ob_type)
#endif

/* SWIG APIs for compatibility of both Python 2 & 3 */

#if PY_VERSION_HEX >= 0x03000000
#  define SWIG_Python_str_FromFormat PyUnicode_FromFormat
#else
#  define SWIG_Python_str_FromFormat PyString_FromFormat
#endif


/* Warning: This function will allocate a new string in Python 3,
 * so please call SWIG_Python_str_DelForPy3(x) to free the space.
 */
SWIGINTERN char*
SWIG_Python_str_AsChar(PyObject *str)
{
#if PY_VERSION_HEX >= 0x03000000
  char *cstr;
  char *newstr;
  Py_ssize_t len;
  str = PyUnicode_AsUTF8String(str);
  PyBytes_AsStringAndSize(str, &cstr, &len);
  newstr = (char *) malloc(len+1);
  memcpy(newstr, cstr, len+1);
  Py_XDECREF(str);
  return newstr;
#else
  return PyString_AsString(str);
#endif
}

#if PY_VERSION_HEX >= 0x03000000
#  define SWIG_Python_str_DelForPy3(x) free( (void*) (x) )
#else
#  define SWIG_Python_str_DelForPy3(x) 
#endif


SWIGINTERN PyObject*
SWIG_Python_str_FromChar(const char *c)
{
#if PY_VERSION_HEX >= 0x03000000
  return PyUnicode_FromString(c); 
#else
  return PyString_FromString(c);
#endif
}

/* Add PyOS_snprintf for old Pythons */
#if PY_VERSION_HEX < 0x02020000
# if defined(_MSC_VER) || defined(__BORLANDC__) || defined(_WATCOM)
#  define PyOS_snprintf _snprintf
# else
#  define PyOS_snprintf snprintf
# endif
#endif

/* A crude PyString_FromFormat implementation for old Pythons */
#if PY_VERSION_HEX < 0x02020000

#ifndef SWIG_PYBUFFER_SIZE
# define SWIG_PYBUFFER_SIZE 1024
#endif

static PyObject *
PyString_FromFormat(const char *fmt, ...) {
  va_list ap;
  char buf[SWIG_PYBUFFER_SIZE * 2];
  int res;
  va_start(ap, fmt);
  res = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return (res < 0 || res >= (int)sizeof(buf)) ? 0 : PyString_FromString(buf);
}
#endif

#ifndef PyObject_DEL
# define PyObject_DEL PyObject_Del
#endif

/* A crude PyExc_StopIteration exception for old Pythons */
#if PY_VERSION_HEX < 0x02020000
# ifndef PyExc_StopIteration
#  define PyExc_StopIteration PyExc_RuntimeError
# endif
# ifndef PyObject_GenericGetAttr
#  define PyObject_GenericGetAttr 0
# endif
#endif

/* Py_NotImplemented is defined in 2.1 and up. */
#if PY_VERSION_HEX < 0x02010000
# ifndef Py_NotImplemented
#  define Py_NotImplemented PyExc_RuntimeError
# endif
#endif

/* A crude PyString_AsStringAndSize implementation for old Pythons */
#if PY_VERSION_HEX < 0x02010000
# ifndef PyString_AsStringAndSize
#  define PyString_AsStringAndSize(obj, s, len) {*s = PyString_AsString(obj); *len = *s ? strlen(*s) : 0;}
# endif
#endif

/* PySequence_Size for old Pythons */
#if PY_VERSION_HEX < 0x02000000
# ifndef PySequence_Size
#  define PySequence_Size PySequence_Length
# endif
#endif

/* PyBool_FromLong for old Pythons */
#if PY_VERSION_HEX < 0x02030000
static
PyObject *PyBool_FromLong(long ok)
{
  PyObject *result = ok ? Py_True : Py_False;
  Py_INCREF(result);
  return result;
}
#endif

/* Py_ssize_t for old Pythons */
/* This code is as recommended by: */
/* http://www.python.org/dev/peps/pep-0353/#conversion-guidelines */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
# define PY_SSIZE_T_MAX INT_MAX
# define PY_SSIZE_T_MIN INT_MIN
typedef inquiry lenfunc;
typedef intargfunc ssizeargfunc;
typedef intintargfunc ssizessizeargfunc;
typedef intobjargproc ssizeobjargproc;
typedef intintobjargproc ssizessizeobjargproc;
typedef getreadbufferproc readbufferproc;
typedef getwritebufferproc writebufferproc;
typedef getsegcountproc segcountproc;
typedef getcharbufferproc charbufferproc;
static long PyNumber_AsSsize_t (PyObject *x, void *SWIGUNUSEDPARM(exc))
{
  long result = 0;
  PyObject *i = PyNumber_Int(x);
  if (i) {
    result = PyInt_AsLong(i);
    Py_DECREF(i);
  }
  return result;
}
#endif

#if PY_VERSION_HEX < 0x02050000
#define PyInt_FromSize_t(x) PyInt_FromLong((long)x)
#endif

#if PY_VERSION_HEX < 0x02040000
#define Py_VISIT(op)				\
  do { 						\
    if (op) {					\
      int vret = visit((op), arg);		\
      if (vret)					\
        return vret;				\
    }						\
  } while (0)
#endif

#if PY_VERSION_HEX < 0x02030000
typedef struct {
  PyTypeObject type;
  PyNumberMethods as_number;
  PyMappingMethods as_mapping;
  PySequenceMethods as_sequence;
  PyBufferProcs as_buffer;
  PyObject *name, *slots;
} PyHeapTypeObject;
#endif

#if PY_VERSION_HEX < 0x02030000
typedef destructor freefunc;
#endif

#if ((PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION > 6) || \
     (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION > 0) || \
     (PY_MAJOR_VERSION > 3))
# define SWIGPY_USE_CAPSULE
# define SWIGPY_CAPSULE_NAME ((char*)"swig_runtime_data" SWIG_RUNTIME_VERSION ".type_pointer_capsule" SWIG_TYPE_TABLE_NAME)
#endif

#if PY_VERSION_HEX < 0x03020000
#define PyDescr_TYPE(x) (((PyDescrObject *)(x))->d_type)
#define PyDescr_NAME(x) (((PyDescrObject *)(x))->d_name)
#define Py_hash_t long
#endif
