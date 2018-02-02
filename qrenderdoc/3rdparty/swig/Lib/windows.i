/* -----------------------------------------------------------------------------
 * windows.i
 *
 * SWIG library file to support types found in windows.h as well as Microsoft
 * integral type extensions. The types are set for 32 bit Windows.
 * ----------------------------------------------------------------------------- */

// Support for non ISO (Windows) integral types
%apply unsigned char { unsigned __int8 };
%apply const unsigned char& { const unsigned __int8& };

%apply signed char { __int8 };
%apply const signed char& { const __int8& };

%apply unsigned short { unsigned __int16 };
%apply const unsigned short& { const unsigned __int16& };

%apply short { __int16 };
%apply const short& { const __int16& };

%apply unsigned int { unsigned __int32 };
%apply const unsigned int& { const unsigned __int32& };

%apply int { __int32 };
%apply const int& { const __int32& };

%apply unsigned long long { unsigned __int64 };
%apply const unsigned long long& { const unsigned __int64& };

%apply long long { __int64 };
%apply const long long& { const __int64& };


// Workaround Microsoft calling conventions
#define __cdecl
#define __fastcall
#define __far
#define __forceinline
#define __fortran
#define __inline
#define __pascal
#define __stdcall
#define __syscall
#define _cdecl
#define _fastcall
#define _inline
#define _pascal
#define _stdcall
#define WINAPI
#define __declspec(WINDOWS_EXTENDED_ATTRIBUTE)

#define __w64

// Types from windef.h
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef char *PSZ;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float FLOAT;
typedef FLOAT *PFLOAT;
typedef BOOL *PBOOL;
typedef BOOL *LPBOOL;
typedef BYTE *PBYTE;
typedef BYTE *LPBYTE;
typedef int *PINT;
typedef int *LPINT;
typedef WORD *PWORD;
typedef WORD *LPWORD;
typedef long *LPLONG;
typedef DWORD *PDWORD;
typedef DWORD *LPDWORD;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned int *PUINT;

// Types from basetsd.h
typedef signed char INT8, *PINT8;
typedef signed short INT16, *PINT16;
typedef signed int INT32, *PINT32;
typedef signed __int64 INT64, *PINT64;
typedef unsigned char UINT8, *PUINT8;
typedef unsigned short UINT16, *PUINT16;
typedef unsigned int UINT32, *PUINT32;
typedef unsigned __int64 UINT64, *PUINT64;
typedef signed int LONG32, *PLONG32;
typedef unsigned int ULONG32, *PULONG32;
typedef unsigned int DWORD32, *PDWORD32;
typedef __w64 int INT_PTR, *PINT_PTR;
typedef __w64 unsigned int UINT_PTR, *PUINT_PTR;
typedef __w64 long LONG_PTR, *PLONG_PTR;
typedef __w64 unsigned long ULONG_PTR, *PULONG_PTR;
typedef unsigned short UHALF_PTR, *PUHALF_PTR;
typedef short HALF_PTR, *PHALF_PTR;
typedef __w64 long SHANDLE_PTR;
typedef __w64 unsigned long HANDLE_PTR;
typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
typedef __int64 LONG64, *PLONG64;
typedef unsigned __int64 ULONG64, *PULONG64;
typedef unsigned __int64 DWORD64, *PDWORD64;

// Types from winnt.h
typedef void *PVOID;
typedef void *PVOID64;
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
typedef CHAR *PCHAR;
typedef CHAR *LPCH, *PCH;
typedef const CHAR *LPCCH, *PCCH;
typedef CHAR *NPSTR;
typedef CHAR *LPSTR, *PSTR;
typedef const CHAR *LPCSTR, *PCSTR;
typedef char TCHAR, *PTCHAR;
typedef unsigned char TBYTE , *PTBYTE ;
typedef LPSTR LPTCH, PTCH;
typedef LPSTR PTSTR, LPTSTR, PUTSTR, LPUTSTR;
typedef LPCSTR PCTSTR, LPCTSTR, PCUTSTR, LPCUTSTR;
typedef SHORT *PSHORT;
typedef LONG *PLONG;
typedef void *HANDLE;
typedef HANDLE *PHANDLE;
typedef BYTE FCHAR;
typedef WORD FSHORT;
typedef DWORD FLONG;
typedef LONG HRESULT;
typedef char CCHAR;
typedef DWORD LCID;
typedef PDWORD PLCID;
typedef WORD LANGID;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;
typedef LONGLONG *PLONGLONG;
typedef ULONGLONG *PULONGLONG;
typedef ULONGLONG DWORDLONG;
typedef DWORDLONG *PDWORDLONG;
typedef BYTE BOOLEAN;
typedef BOOLEAN *PBOOLEAN;

