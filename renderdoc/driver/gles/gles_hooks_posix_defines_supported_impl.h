
#undef HookWrapper0
#define HookWrapper0(return_type, function )                       \
    extern "C" __attribute__((visibility("default"))) return_type function()     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function();  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)() \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function();  \
    }

#undef HookWrapper1
#define HookWrapper1(return_type, function, T0, A0)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0);  \
    }

#undef HookWrapper2
#define HookWrapper2(return_type, function, T0, A0, T1, A1)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1);  \
    }

#undef HookWrapper3
#define HookWrapper3(return_type, function, T0, A0, T1, A1, T2, A2)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2);  \
    }

#undef HookWrapper4
#define HookWrapper4(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3);  \
    }

#undef HookWrapper5
#define HookWrapper5(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4);  \
    }

#undef HookWrapper6
#define HookWrapper6(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5);  \
    }

#undef HookWrapper7
#define HookWrapper7(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6);  \
    }

#undef HookWrapper8
#define HookWrapper8(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7);  \
    }

#undef HookWrapper9
#define HookWrapper9(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8);  \
    }

#undef HookWrapper10
#define HookWrapper10(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9);  \
    }

#undef HookWrapper11
#define HookWrapper11(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10);  \
    }

#undef HookWrapper12
#define HookWrapper12(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11);  \
    }

#undef HookWrapper13
#define HookWrapper13(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12);  \
    }

#undef HookWrapper14
#define HookWrapper14(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12, T13, A13)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13);  \
    }

#undef HookWrapper15
#define HookWrapper15(return_type, function, T0, A0, T1, A1, T2, A2, T3, A3, T4, A4, T5, A5, T6, A6, T7, A7, T8, A8, T9, A9, T10, A10, T11, A11, T12, A12, T13, A13, T14, A14)                       \
    extern "C" __attribute__((visibility("default"))) return_type function(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13, T14 A14)     \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_WRAPPER(function);            \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14);  \
    }  \
    return_type CONCAT(function, _renderdoc_hooked)(T0 A0, T1 A1, T2 A2, T3 A3, T4 A4, T5 A5, T6 A6, T7 A7, T8 A8, T9 A9, T10 A10, T11 A11, T12 A12, T13 A13, T14 A14) \
    {  \
        SCOPED_LOCK_GUARD();                \
        DEBUG_HOOKED(function);             \
        return DRIVER()->function(A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14);  \
    }
