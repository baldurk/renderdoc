/*
  Unordered Multisets
*/

%include <std_unordered_set.i>

%fragment("StdUnorderedMultisetTraits","header",fragment="StdSequenceTraits")
%{
  namespace swig {
    template <class SwigPySeq, class T> 
    inline void
    assign(const SwigPySeq& swigpyseq, std::unordered_multiset<T>* seq) {
      // seq->insert(swigpyseq.begin(), swigpyseq.end()); // not used as not always implemented
      typedef typename SwigPySeq::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	seq->insert(seq->end(),(value_type)(*it));
      }
    }

    template <class T>
    struct traits_reserve<std::unordered_multiset<T> >  {
      static void reserve(std::unordered_multiset<T> &seq, typename std::unordered_multiset<T>::size_type n) {
        seq.reserve(n);
      }
    };

    template <class T>
    struct traits_asptr<std::unordered_multiset<T> >  {
      static int asptr(PyObject *obj, std::unordered_multiset<T> **m) {
	return traits_asptr_stdseq<std::unordered_multiset<T> >::asptr(obj, m);
      }
    };

    template <class T>
    struct traits_from<std::unordered_multiset<T> > {
      static PyObject *from(const std::unordered_multiset<T>& vec) {
	return traits_from_stdseq<std::unordered_multiset<T> >::from(vec);
      }
    };
  }
%}

#define %swig_unordered_multiset_methods(Set...) %swig_set_methods(Set)



%include <std/std_unordered_multiset.i>
