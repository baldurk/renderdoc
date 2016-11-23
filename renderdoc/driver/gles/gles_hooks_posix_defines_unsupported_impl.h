
#undef HookWrapper0
#define HookWrapper0(return_type, function )                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function()     \
  {  \
    return CONCAT(function, _renderdoc_hooked)();  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)() \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)();  \
  }

#undef HookWrapper1
#define HookWrapper1(return_type, function, T0, A0)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0);  \
  }

#undef HookWrapper2
#define HookWrapper2(return_type, function, T0, A0, T1, A1)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1);  \
  }

#undef HookWrapper3
#define HookWrapper3(return_type, function, T0, A0, T1, A1, T2, A2)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2);  \
  }

#undef HookWrapper4
#define HookWrapper4(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3);  \
  }

#undef HookWrapper5
#define HookWrapper5(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4);  \
  }

#undef HookWrapper6
#define HookWrapper6(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5);  \
  }

#undef HookWrapper7
#define HookWrapper7(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6);  \
  }

#undef HookWrapper8
#define HookWrapper8(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7);  \
  }

#undef HookWrapper9
#define HookWrapper9(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8);  \
  }

#undef HookWrapper10
#define HookWrapper10(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);  \
  }

#undef HookWrapper11
#define HookWrapper11(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10);  \
  }

#undef HookWrapper12
#define HookWrapper12(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11);  \
  }

#undef HookWrapper13
#define HookWrapper13(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12);  \
  }

#undef HookWrapper14
#define HookWrapper14(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12, T13, A13)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13);  \
  }

#undef HookWrapper15
#define HookWrapper15(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12, T13, A13, T14, A14)                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \
  extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13, T14 A14)     \
  {  \
    return CONCAT(function, _renderdoc_hooked)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14);  \
  }  \
  return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13, T14 A14) \
  {  \
    static bool hit = false;    \
    if (hit == false)           \
    {                          \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;               \
    }                          \
    return CONCAT(unsupported_real_, function)(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14);  \
  }
