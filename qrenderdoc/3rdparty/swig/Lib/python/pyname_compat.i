/* 
* From SWIG 1.3.37 we deprecated all SWIG symbols that start with Py,
* since they are inappropriate and discouraged in Python documentation
* (from http://www.python.org/doc/2.5.2/api/includes.html):
*
* "All user visible names defined by Python.h (except those defined by the included
* standard headers) have one of the prefixes "Py" or "_Py". Names beginning with
* "_Py" are for internal use by the Python implementation and should not be used
* by extension writers. Structure member names do not have a reserved prefix.
*
* Important: user code should never define names that begin with "Py" or "_Py".
* This confuses the reader, and jeopardizes the portability of the user code to
* future Python versions, which may define additional names beginning with one
* of these prefixes."
*
* This file defined macros to provide backward compatibility for these deprecated
* symbols. In the case you have these symbols in your interface file, you can simply
* include this file at beginning of it.
*
* However, this file may be removed in future release of SWIG, so using this file to
* keep these inappropriate names in your SWIG interface file is also not recommended.
* Instead, we provide a simple tool for converting your interface files to
* the new naming convention. You can get the tool from the SWIG distribution:
* Tools/pyname_patch.py
*/

%fragment("PySequence_Base", "header", fragment="SwigPySequence_Base") {}
%fragment("PySequence_Cont", "header", fragment="SwigPySequence_Cont") {}
%fragment("PySwigIterator_T", "header", fragment="SwigPyIterator_T") {}
%fragment("PyPairBoolOutputIterator", "header", fragment="SwigPyPairBoolOutputIterator") {}
%fragment("PySwigIterator", "header", fragment="SwigPyIterator") {}
%fragment("PySwigIterator_T", "header", fragment="SwigPyIterator_T") {}

%inline %{
#define PyMapIterator_T SwigPyMapIterator_T
#define PyMapKeyIterator_T SwigPyMapKeyIterator_T
#define PyMapValueIterator_T SwigPyMapValueITerator_T
#define PyObject_ptr SwigPtr_PyObject
#define PyObject_var SwigVar_PyObject
#define PyOper SwigPyOper
#define PySeq SwigPySeq
#define PySequence_ArrowProxy SwigPySequence_ArrowProxy
#define PySequence_Cont SwigPySequence_Cont
#define PySequence_InputIterator SwigPySequence_InputIterator
#define PySequence_Ref SwigPySequence_Ref
#define PySwigClientData SwigPyClientData
#define PySwigClientData_Del SwigPyClientData_Del
#define PySwigClientData_New SwigPyClientData_New
#define PySwigIterator SwigPyIterator
#define PySwigIteratorClosed_T SwigPyIteratorClosed_T
#define PySwigIteratorOpen_T SwigPyIteratorOpen_T
#define PySwigIterator_T SwigPyIterator_T
#define PySwigObject SwigPyObject
#define PySwigObject_Check SwigPyObject_Check
#define PySwigObject_GetDesc SwigPyObject_GetDesc
#define PySwigObject_New SwigPyObject_New
#define PySwigObject_acquire SwigPyObject_acquire
#define PySwigObject_append SwigPyObject_append
#define PySwigObject_as_number SwigPyObject_as_number
#define PySwigObject_compare SwigPyObject_compare
#define PySwigObject_dealloc SwigPyObject_dealloc
#define PySwigObject_disown SwigPyObject_disown
#define PySwigObject_format SwigPyObject_format
#define PySwigObject_getattr SwigPyObject_getattr
#define PySwigObject_hex SwigPyObject_hex
#define PySwigObject_long SwigPyObject_long
#define PySwigObject_next SwigPyObject_next
#define PySwigObject_oct SwigPyObject_oct
#define PySwigObject_own SwigPyObject_own
#define PySwigObject_repr SwigPyObject_repr
#define PySwigObject_richcompare SwigPyObject_richcompare
#define PySwigObject_type SwigPyObject_type
#define PySwigPacked SwigPyPacked
#define PySwigPacked_Check SwigPyPacked_Check
#define PySwigPacked_New SwigPyPacked_New
#define PySwigPacked_UnpackData SwigPyPacked_UnpackData
#define PySwigPacked_compare SwigPyPacked_compare
#define PySwigPacked_dealloc SwigPyPacked_dealloc
#define PySwigPacked_print SwigPyPacked_print
#define PySwigPacked_repr SwigPyPacked_repr
#define PySwigPacked_str SwigPyPacked_str
#define PySwigPacked_type SwigPyPacked_type
#define pyseq swigpyseq
#define pyswigobject_type swigpyobject_type
#define pyswigpacked_type swigpypacked_type
%}
