/* This file defines an internal function for processing default arguments
   with proxy classes.

   There seems to be no straightforward way to write proxy functions
   involving default arguments. For example :

             def foo(arg1,arg2,*args):
                     proxyc.foo(arg1,arg2,args)

   This fails because args is now a tuple and SWIG doesn't know what to
   do with it.

   This file allows a different approach :

            def foo(arg1,arg2,*args):
                    proxyc.__call_defarg(proxyc.foo,(arg1,arg2,)+args)

   Basically, we form a new tuple from the object, call this special
   __call_defarg method and it passes control to the real wrapper function.
   An ugly hack, but it works.
*/

SWIGINTERN PyObject *swig_call_defargs(PyObject *self, PyObject *args) {
  PyObject *func;
  PyObject *parms;
  
  if (!PyArg_ParseTuple(args,"OO",&func,&parms))
    return NULL;
  
  if (!PyCallable_Check(func)) {
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    PyErr_SetString(PyExc_TypeError, "__call_defarg : Need a callable object!");
    SWIG_PYTHON_THREAD_END_BLOCK;
    return NULL;
  }
  return PyEval_CallObject(func,parms);
}
