/*
  Unordered Sets
*/

%fragment("StdUnorderedSetTraits","header",fragment="StdSequenceTraits")
%{
  namespace swig {
    template <class SwigPySeq, class T> 
    inline void 
    assign(const SwigPySeq& swigpyseq, std::unordered_set<T>* seq) {
      // seq->insert(swigpyseq.begin(), swigpyseq.end()); // not used as not always implemented
      typedef typename SwigPySeq::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	seq->insert(seq->end(),(value_type)(*it));
      }
    }

    template <class T>
    struct traits_reserve<std::unordered_set<T> >  {
      static void reserve(std::unordered_set<T> &seq, typename std::unordered_set<T>::size_type n) {
        seq.reserve(n);
      }
    };

    template <class T>
    struct traits_asptr<std::unordered_set<T> >  {
      static int asptr(PyObject *obj, std::unordered_set<T> **s) {
	return traits_asptr_stdseq<std::unordered_set<T> >::asptr(obj, s);
      }
    };

    template <class T>
    struct traits_from<std::unordered_set<T> > {
      static PyObject *from(const std::unordered_set<T>& vec) {
	return traits_from_stdseq<std::unordered_set<T> >::from(vec);
      }
    };
  }
%}

%define %swig_unordered_set_methods(unordered_set...)
  %swig_sequence_iterator(unordered_set);
  %swig_container_methods(unordered_set);

  %extend  {
     void append(value_type x) {
       self->insert(x);
     }
  
     bool __contains__(value_type x) {
       return self->find(x) != self->end();
     }

     value_type __getitem__(difference_type i) const throw (std::out_of_range) {
       return *(swig::cgetpos(self, i));
     }

  };
%enddef

%include <std/std_unordered_set.i>
