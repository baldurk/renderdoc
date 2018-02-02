/*
  std::array
*/

%fragment("StdArrayTraits","header",fragment="StdSequenceTraits")
%{
  namespace swig {
    template <class T, size_t N>
    struct traits_asptr<std::array<T, N> >  {
      static int asptr(PyObject *obj, std::array<T, N> **vec) {
	return traits_asptr_stdseq<std::array<T, N> >::asptr(obj, vec);
      }
    };

    template <class T, size_t N>
    struct traits_from<std::array<T, N> > {
      static PyObject *from(const std::array<T, N>& vec) {
	return traits_from_stdseq<std::array<T, N> >::from(vec);
      }
    };

    template <class SwigPySeq, class T, size_t N>
    inline void
    assign(const SwigPySeq& swigpyseq, std::array<T, N>* seq) {
      if (swigpyseq.size() < seq->size())
        throw std::invalid_argument("std::array cannot be expanded in size");
      else if (swigpyseq.size() > seq->size())
        throw std::invalid_argument("std::array cannot be reduced in size");
      std::copy(swigpyseq.begin(), swigpyseq.end(), seq->begin());
    }

    template <class T, size_t N>
    inline void
    erase(std::array<T, N>* SWIGUNUSEDPARM(seq), const typename std::array<T, N>::iterator& SWIGUNUSEDPARM(position)) {
      throw std::invalid_argument("std::array object does not support item deletion");
    }

    // Only limited slicing is supported as std::array is fixed in size
    template <class T, size_t N, class Difference>
    inline std::array<T, N>*
    getslice(const std::array<T, N>* self, Difference i, Difference j, Py_ssize_t step) {
      typedef std::array<T, N> Sequence;
      typename Sequence::size_type size = self->size();
      Difference ii = 0;
      Difference jj = 0;
      swig::slice_adjust(i, j, step, size, ii, jj);

      if (step == 1 && ii == 0 && static_cast<typename Sequence::size_type>(jj) == size) {
        Sequence *sequence = new Sequence();
        std::copy(self->begin(), self->end(), sequence->begin());
        return sequence;
      } else if (step == -1 && static_cast<typename Sequence::size_type>(ii) == (size - 1) && jj == -1) {
        Sequence *sequence = new Sequence();
        std::copy(self->rbegin(), self->rend(), sequence->begin());
        return sequence;
      } else {
        throw std::invalid_argument("std::array object only supports getting a slice that is the size of the array");
      }
    }

    template <class T, size_t N, class Difference, class InputSeq>
    inline void
    setslice(std::array<T, N>* self, Difference i, Difference j, Py_ssize_t step, const InputSeq& is = InputSeq()) {
      typedef std::array<T, N> Sequence;
      typename Sequence::size_type size = self->size();
      Difference ii = 0;
      Difference jj = 0;
      swig::slice_adjust(i, j, step, size, ii, jj, true);

      if (step == 1 && ii == 0 && static_cast<typename Sequence::size_type>(jj) == size) {
        std::copy(is.begin(), is.end(), self->begin());
      } else if (step == -1 && static_cast<typename Sequence::size_type>(ii) == (size - 1) && jj == -1) {
        std::copy(is.rbegin(), is.rend(), self->begin());
      } else {
        throw std::invalid_argument("std::array object only supports setting a slice that is the size of the array");
      }
    }

    template <class T, size_t N, class Difference>
    inline void
    delslice(std::array<T, N>* SWIGUNUSEDPARM(self), Difference SWIGUNUSEDPARM(i), Difference SWIGUNUSEDPARM(j), Py_ssize_t SWIGUNUSEDPARM(step)) {
      throw std::invalid_argument("std::array object does not support item deletion");
    }
  }
%}

#define %swig_array_methods(Type...) %swig_sequence_methods_non_resizable(Type)
#define %swig_array_methods_val(Type...) %swig_sequence_methods_non_resizable_val(Type);

%include <std/std_array.i>

