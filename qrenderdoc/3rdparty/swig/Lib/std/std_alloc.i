namespace std
{
  /**
   *  @brief  The "standard" allocator, as per [20.4].
   *
   *  The private _Alloc is "SGI" style.  (See comments at the top
   *  of stl_alloc.h.)
   *
   *  The underlying allocator behaves as follows.
   *    - __default_alloc_template is used via two typedefs
   *    - "__single_client_alloc" typedef does no locking for threads
   *    - "__alloc" typedef is threadsafe via the locks
   *    - __new_alloc is used for memory requests
   *
   *  (See @link Allocators allocators info @endlink for more.)
   */
  template<typename _Tp>
    class allocator
    {
    public:
      typedef size_t     size_type;
      typedef ptrdiff_t  difference_type;
      typedef _Tp*       pointer;
      typedef const _Tp* const_pointer;
      typedef _Tp&       reference;
      typedef const _Tp& const_reference;
      typedef _Tp        value_type;

      template<typename _Tp1>
        struct rebind;

      allocator() throw();
      
      allocator(const allocator&) throw();
      template<typename _Tp1>
        allocator(const allocator<_Tp1>&) throw();
      ~allocator() throw();
      

      pointer
      address(reference __x) const;
      

      const_pointer
      address(const_reference __x) const;
      

      // NB: __n is permitted to be 0.  The C++ standard says nothing
      // about what the return value is when __n == 0.
      _Tp*
      allocate(size_type __n, const void* = 0);

      // __p is not permitted to be a null pointer.
      void
      deallocate(pointer __p, size_type __n);

      size_type
      max_size() const throw();

      void construct(pointer __p, const _Tp& __val);
      void destroy(pointer __p);
    };

  template<>
    class allocator<void>
    {
    public:
      typedef size_t      size_type;
      typedef ptrdiff_t   difference_type;
      typedef void*       pointer;
      typedef const void* const_pointer;
      typedef void        value_type;

      template<typename _Tp1>
        struct rebind;
    };
} // namespace std
