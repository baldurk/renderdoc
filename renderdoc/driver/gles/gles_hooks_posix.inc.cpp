// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

//#define DUMP_GL_ERRORS

#ifdef DUMP_GL_ERRORS
class GLError
{
public:
    GLError(const char *function_arg)
        : function_arg(function_arg)
    {}

    ~GLError()
    {
      GLenum errorResult = OpenGLHook::glhooks.GetDriver()->glGetError();
      if (errorResult != GL_NO_ERROR) {
        RDCLOG("RES: %s : %p", function_arg, errorResult);
      }
    }
private:
    const char *function_arg;
};

#define CheckGLError(function) GLError errtest(function)

#else

#define CheckGLError(function)

#endif

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from glXGetProcAddress. If another library (or the app)
// creates a symbol called 'glEnable' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#define HookWrapper0(ret, function)                                \
  typedef ret (*CONCAT(function, _hooktype))();                    \
  extern "C" __attribute__((visibility("default"))) ret function() \
  {                                                                \
    SCOPED_LOCK(glLock);                                           \
    CheckGLError(#function);                                        \
    return OpenGLHook::glhooks.GetDriver()->function();            \
  }                                                                \
  ret CONCAT(function, _renderdoc_hooked)()                        \
  {                                                                \
    SCOPED_LOCK(glLock);                                           \
    return OpenGLHook::glhooks.GetDriver()->function();            \
  }
#define HookWrapper1(ret, function, t1, p1)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1) \
  {                                                                     \
    SCOPED_LOCK(glLock);                                                \
    CheckGLError(#function);                                             \
    return OpenGLHook::glhooks.GetDriver()->function(p1);               \
  }                                                                     \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                        \
  {                                                                     \
    SCOPED_LOCK(glLock);                                                \
    return OpenGLHook::glhooks.GetDriver()->function(p1);               \
  }
#define HookWrapper2(ret, function, t1, p1, t2, p2)                            \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    SCOPED_LOCK(glLock);                                                       \
    CheckGLError(#function);                                                    \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2);                  \
  }                                                                            \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                        \
  {                                                                            \
    SCOPED_LOCK(glLock);                                                       \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2);                  \
  }
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                           \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    SCOPED_LOCK(glLock);                                                              \
    CheckGLError(#function);                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3);                     \
  }                                                                                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                        \
  {                                                                                   \
    SCOPED_LOCK(glLock);                                                              \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3);                     \
  }
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    CheckGLError(#function);                                                                  \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4);                        \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4);                        \
  }
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    CheckGLError(#function);                                                                         \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5);                           \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                        \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5);                           \
  }
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                        \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6)               \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    CheckGLError(#function);                                                                  \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6);                \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)          \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6);                \
  }
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7)  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                    \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6, t7 p7)        \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    CheckGLError(#function);                                                                  \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7);            \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)   \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7);            \
  }
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,        \
                                                                 t5 p5, t6 p6, t7 p7, t8 p8)        \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    CheckGLError(#function);                                                                         \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8);               \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8);               \
  }
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9)                              \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    CheckGLError(#function);                                                                       \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);         \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);         \
  }
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10)                     \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    CheckGLError(#function);                                                                       \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);    \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);    \
  }
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,    \
                      p8, t9, p9, t10, p10, t11, p11)                                               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);         \
  extern "C" __attribute__((visibility("default"))) ret function(                                   \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)              \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    CheckGLError(#function);                                                                         \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,   \
                                          t9 p9, t10 p10, t11 p11)                                  \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); \
  }
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);   \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12)    \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    CheckGLError(#function);                                                                        \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12);                                         \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                        \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12);                                         \
  }
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13);                                                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13)                                                                                     \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    CheckGLError(#function);                                                                        \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13);                                    \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)               \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13);                                    \
  }
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)                \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14);                                            \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14)                                                                            \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    CheckGLError(#function);                                                                        \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14);                               \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)      \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14);                               \
  }
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)      \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15);                                       \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15)                                                                   \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    CheckGLError(#function);                                                                        \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14, p15);                          \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15)                                                 \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14, p15);                          \
  }

DefineDLLExportHooks();
DefineGLExtensionHooks();

/*
  in bash:

    function HookWrapper()
    {
        N=$1;
        echo "#undef HookWrapper$N";
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -en "\tCONCAT(function, _hooktype) CONCAT(unsupported_real_,function) = NULL;";

        echo -en "\tret CONCAT(function,_renderdoc_hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -e "\t{ \\";
        echo -e "\tstatic bool hit = false; if(hit == false) { RDCERR(\"Function \"
  STRINGIZE(function) \" not supported - capture may be broken\"); hit = true; } \\";
        echo -en "\treturn CONCAT(unsupported_real_,function)(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "\t}";
    }

  for I in `seq 0 15`; do HookWrapper $I; echo; done

  */

#undef HookWrapper0
#define HookWrapper0(ret, function)                                                     \
  typedef ret (*CONCAT(function, _hooktype))();                                         \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)()                                             \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)();                                       \
  }

#undef HookWrapper1
#define HookWrapper1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                                        \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1);                                     \
  }

#undef HookWrapper2
#define HookWrapper2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                                 \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2);                                 \
  }

#undef HookWrapper3
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                               \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                          \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3);                             \
  }

#undef HookWrapper4
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                   \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4);                         \
  }

#undef HookWrapper5
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)            \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5);                     \
  }

#undef HookWrapper6
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)     \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6);                 \
  }

#undef HookWrapper7
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)  \
  {                                                                                         \
    static bool hit = false;                                                                \
    if(hit == false)                                                                        \
    {                                                                                       \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");     \
      hit = true;                                                                           \
    }                                                                                       \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7);                 \
  }

#undef HookWrapper8
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                           \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    static bool hit = false;                                                                        \
    if(hit == false)                                                                                \
    {                                                                                               \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");             \
      hit = true;                                                                                   \
    }                                                                                               \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8);                     \
  }

#undef HookWrapper9
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9);               \
  }

#undef HookWrapper10
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);          \
  }

#undef HookWrapper11
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11)                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);     \
  }

#undef HookWrapper12
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                        \
  {                                                                                                \
    static bool hit = false;                                                                       \
    if(hit == false)                                                                               \
    {                                                                                              \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");            \
      hit = true;                                                                                  \
    }                                                                                              \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); \
  }

#undef HookWrapper13
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13);                                                \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13);                                              \
  }

#undef HookWrapper14
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14);                                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)     \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14);                                         \
  }

#undef HookWrapper15
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14, t15);                                      \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,     \
                                          t15 p15)                                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14, p15);                                    \
  }

DefineUnsupportedDummies();
