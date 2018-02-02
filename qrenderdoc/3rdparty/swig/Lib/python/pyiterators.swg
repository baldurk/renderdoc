/* -----------------------------------------------------------------------------
 * pyiterators.swg
 *
 * Implement a python 'output' iterator for Python 2.2 or higher.
 *
 * Users can derive form the SwigPyIterator to implement their
 * own iterators. As an example (real one since we use it for STL/STD
 * containers), the template SwigPyIterator_T does the
 * implementation for generic C++ iterators.
 * ----------------------------------------------------------------------------- */

%include <std_common.i>

%fragment("SwigPyIterator","header",fragment="<stddef.h>") {
namespace swig {
  struct stop_iteration {
  };

  struct SwigPyIterator {
  private:
    SwigPtr_PyObject _seq;

  protected:
    SwigPyIterator(PyObject *seq) : _seq(seq)
    {
    }
      
  public:
    virtual ~SwigPyIterator() {}

    // Access iterator method, required by Python
    virtual PyObject *value() const = 0;

    // Forward iterator method, required by Python
    virtual SwigPyIterator *incr(size_t n = 1) = 0;
    
    // Backward iterator method, very common in C++, but not required in Python
    virtual SwigPyIterator *decr(size_t /*n*/ = 1)
    {
      throw stop_iteration();
    }

    // Random access iterator methods, but not required in Python
    virtual ptrdiff_t distance(const SwigPyIterator &/*x*/) const
    {
      throw std::invalid_argument("operation not supported");
    }

    virtual bool equal (const SwigPyIterator &/*x*/) const
    {
      throw std::invalid_argument("operation not supported");
    }
    
    // C++ common/needed methods
    virtual SwigPyIterator *copy() const = 0;

    PyObject *next()     
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK; // disable threads       
      PyObject *obj = value();
      incr();       
      SWIG_PYTHON_THREAD_END_BLOCK; // re-enable threads
      return obj;     
    }

    /* Make an alias for Python 3.x */
    PyObject *__next__()
    {
      return next();
    }

    PyObject *previous()
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK; // disable threads       
      decr();
      PyObject *obj = value();
      SWIG_PYTHON_THREAD_END_BLOCK; // re-enable threads       
      return obj;
    }

    SwigPyIterator *advance(ptrdiff_t n)
    {
      return  (n > 0) ?  incr(n) : decr(-n);
    }
      
    bool operator == (const SwigPyIterator& x)  const
    {
      return equal(x);
    }
      
    bool operator != (const SwigPyIterator& x) const
    {
      return ! operator==(x);
    }
      
    SwigPyIterator& operator += (ptrdiff_t n)
    {
      return *advance(n);
    }

    SwigPyIterator& operator -= (ptrdiff_t n)
    {
      return *advance(-n);
    }
      
    SwigPyIterator* operator + (ptrdiff_t n) const
    {
      return copy()->advance(n);
    }

    SwigPyIterator* operator - (ptrdiff_t n) const
    {
      return copy()->advance(-n);
    }
      
    ptrdiff_t operator - (const SwigPyIterator& x) const
    {
      return x.distance(*this);
    }
      
    static swig_type_info* descriptor() {
      static int init = 0;
      static swig_type_info* desc = 0;
      if (!init) {
	desc = SWIG_TypeQuery("swig::SwigPyIterator *");
	init = 1;
      }	
      return desc;
    }    
  };

%#if defined(SWIGPYTHON_BUILTIN)
  inline PyObject* make_output_iterator_builtin (PyObject *pyself)
  {
    Py_INCREF(pyself);
    return pyself;
  }
%#endif
}
}

%fragment("SwigPyIterator_T","header",fragment="<stddef.h>",fragment="SwigPyIterator",fragment="StdTraits",fragment="StdIteratorTraits") {
namespace swig {
  template<typename OutIterator>
  class SwigPyIterator_T :  public SwigPyIterator
  {
  public:
    typedef OutIterator out_iterator;
    typedef typename std::iterator_traits<out_iterator>::value_type value_type;    
    typedef SwigPyIterator_T<out_iterator> self_type;

    SwigPyIterator_T(out_iterator curr, PyObject *seq)
      : SwigPyIterator(seq), current(curr)
    {
    }

    const out_iterator& get_current() const
    {
      return current;
    }

    
    bool equal (const SwigPyIterator &iter) const
    {
      const self_type *iters = dynamic_cast<const self_type *>(&iter);
      if (iters) {
	return (current == iters->get_current());
      } else {
	throw std::invalid_argument("bad iterator type");
      }
    }
    
    ptrdiff_t distance(const SwigPyIterator &iter) const
    {
      const self_type *iters = dynamic_cast<const self_type *>(&iter);
      if (iters) {
	return std::distance(current, iters->get_current());
      } else {
	throw std::invalid_argument("bad iterator type");
      }
    }    
    
  protected:
    out_iterator current;
  };
  
  template <class ValueType>
  struct from_oper 
  {
    typedef const ValueType& argument_type;
    typedef PyObject *result_type;
    result_type operator()(argument_type v) const
    {
      return swig::from(v);
    }
  };

  template<typename OutIterator, 
	   typename ValueType = typename std::iterator_traits<OutIterator>::value_type,
	   typename FromOper = from_oper<ValueType> >
  class SwigPyIteratorOpen_T :  public SwigPyIterator_T<OutIterator>
  {
  public:
    FromOper from;
    typedef OutIterator out_iterator;
    typedef ValueType value_type;
    typedef SwigPyIterator_T<out_iterator>  base;
    typedef SwigPyIteratorOpen_T<OutIterator, ValueType, FromOper> self_type;
    
    SwigPyIteratorOpen_T(out_iterator curr, PyObject *seq)
      : SwigPyIterator_T<OutIterator>(curr, seq)
    {
    }
    
    PyObject *value() const {
      return from(static_cast<const value_type&>(*(base::current)));
    }
    
    SwigPyIterator *copy() const
    {
      return new self_type(*this);
    }

    SwigPyIterator *incr(size_t n = 1)
    {
      while (n--) {
	++base::current;
      }
      return this;
    }

    SwigPyIterator *decr(size_t n = 1)
    {
      while (n--) {
	--base::current;
      }
      return this;
    }
  };

  template<typename OutIterator, 
	   typename ValueType = typename std::iterator_traits<OutIterator>::value_type,
	   typename FromOper = from_oper<ValueType> >
  class SwigPyIteratorClosed_T :  public SwigPyIterator_T<OutIterator>
  {
  public:
    FromOper from;
    typedef OutIterator out_iterator;
    typedef ValueType value_type;
    typedef SwigPyIterator_T<out_iterator>  base;    
    typedef SwigPyIteratorClosed_T<OutIterator, ValueType, FromOper> self_type;
    
    SwigPyIteratorClosed_T(out_iterator curr, out_iterator first, out_iterator last, PyObject *seq)
      : SwigPyIterator_T<OutIterator>(curr, seq), begin(first), end(last)
    {
    }
    
    PyObject *value() const {
      if (base::current == end) {
	throw stop_iteration();
      } else {
	return from(static_cast<const value_type&>(*(base::current)));
      }
    }
    
    SwigPyIterator *copy() const
    {
      return new self_type(*this);
    }

    SwigPyIterator *incr(size_t n = 1)
    {
      while (n--) {
	if (base::current == end) {
	  throw stop_iteration();
	} else {
	  ++base::current;
	}
      }
      return this;
    }

    SwigPyIterator *decr(size_t n = 1)
    {
      while (n--) {
	if (base::current == begin) {
	  throw stop_iteration();
	} else {
	  --base::current;
	}
      }
      return this;
    }

  private:
    out_iterator begin;
    out_iterator end;
  };

  template<typename OutIter>
  inline SwigPyIterator*
  make_output_iterator(const OutIter& current, const OutIter& begin,const OutIter& end, PyObject *seq = 0)
  {
    return new SwigPyIteratorClosed_T<OutIter>(current, begin, end, seq);
  }

  template<typename OutIter>
  inline SwigPyIterator*
  make_output_iterator(const OutIter& current, PyObject *seq = 0)
  {
    return new SwigPyIteratorOpen_T<OutIter>(current, seq);
  }

}
}


%fragment("SwigPyIterator");
namespace swig 
{
  /*
    Throw a StopIteration exception
  */
  %ignore stop_iteration;
  struct stop_iteration {};
  
  %typemap(throws) stop_iteration {
    (void)$1;
    SWIG_SetErrorObj(PyExc_StopIteration, SWIG_Py_Void());
    SWIG_fail;
  }

  /* 
     Mark methods that return new objects
  */
  %newobject SwigPyIterator::copy;
  %newobject SwigPyIterator::operator + (ptrdiff_t n) const;
  %newobject SwigPyIterator::operator - (ptrdiff_t n) const;

  %nodirector SwigPyIterator;

#if defined(SWIGPYTHON_BUILTIN)
  %feature("python:tp_iter") SwigPyIterator "&swig::make_output_iterator_builtin";
  %feature("python:slot", "tp_iternext", functype="iternextfunc") SwigPyIterator::__next__;
#else
  %extend SwigPyIterator {
  %pythoncode %{def __iter__(self):
    return self%}
  }
#endif

  %catches(swig::stop_iteration) SwigPyIterator::value() const;
  %catches(swig::stop_iteration) SwigPyIterator::incr(size_t n = 1);
  %catches(swig::stop_iteration) SwigPyIterator::decr(size_t n = 1);
  %catches(std::invalid_argument) SwigPyIterator::distance(const SwigPyIterator &x) const;
  %catches(std::invalid_argument) SwigPyIterator::equal (const SwigPyIterator &x) const;
  %catches(swig::stop_iteration) SwigPyIterator::__next__();
  %catches(swig::stop_iteration) SwigPyIterator::next();
  %catches(swig::stop_iteration) SwigPyIterator::previous();
  %catches(swig::stop_iteration) SwigPyIterator::advance(ptrdiff_t n);
  %catches(swig::stop_iteration) SwigPyIterator::operator += (ptrdiff_t n);
  %catches(swig::stop_iteration) SwigPyIterator::operator -= (ptrdiff_t n);
  %catches(swig::stop_iteration) SwigPyIterator::operator + (ptrdiff_t n) const;
  %catches(swig::stop_iteration) SwigPyIterator::operator - (ptrdiff_t n) const;

  struct SwigPyIterator
  {
  protected:
    SwigPyIterator(PyObject *seq);

  public:
    virtual ~SwigPyIterator();

    // Access iterator method, required by Python
    virtual PyObject *value() const = 0;

    // Forward iterator method, required by Python
    virtual SwigPyIterator *incr(size_t n = 1) = 0;
    
    // Backward iterator method, very common in C++, but not required in Python
    virtual SwigPyIterator *decr(size_t n = 1);

    // Random access iterator methods, but not required in Python
    virtual ptrdiff_t distance(const SwigPyIterator &x) const;

    virtual bool equal (const SwigPyIterator &x) const;
    
    // C++ common/needed methods
    virtual SwigPyIterator *copy() const = 0;

    PyObject *next();
    PyObject *__next__();
    PyObject *previous();
    SwigPyIterator *advance(ptrdiff_t n);

    bool operator == (const SwigPyIterator& x)  const;
    bool operator != (const SwigPyIterator& x) const;
    SwigPyIterator& operator += (ptrdiff_t n);
    SwigPyIterator& operator -= (ptrdiff_t n);
    SwigPyIterator* operator + (ptrdiff_t n) const;
    SwigPyIterator* operator - (ptrdiff_t n) const;
    ptrdiff_t operator - (const SwigPyIterator& x) const;
  };
}

