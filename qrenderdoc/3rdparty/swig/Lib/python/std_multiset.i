/*
  Multisets
*/

%include <std_set.i>

%fragment("StdMultisetTraits","header",fragment="StdSequenceTraits")
%{
  namespace swig {
    template <class SwigPySeq, class T> 
    inline void
    assign(const SwigPySeq& swigpyseq, std::multiset<T>* seq) {
      // seq->insert(swigpyseq.begin(), swigpyseq.end()); // not used as not always implemented
      typedef typename SwigPySeq::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	seq->insert(seq->end(),(value_type)(*it));
      }
    }

    template <class T>
    struct traits_asptr<std::multiset<T> >  {
      static int asptr(PyObject *obj, std::multiset<T> **m) {
	return traits_asptr_stdseq<std::multiset<T> >::asptr(obj, m);
      }
    };

    template <class T>
    struct traits_from<std::multiset<T> > {
      static PyObject *from(const std::multiset<T>& vec) {
	return traits_from_stdseq<std::multiset<T> >::from(vec);
      }
    };
  }
%}

#define %swig_multiset_methods(Set...) %swig_set_methods(Set)



%include <std/std_multiset.i>
