
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.h                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides declarations for the DirectX Compiler API entry point.           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_API__
#define __DXC_API__

#ifdef _WIN32
#ifndef DXC_API_IMPORT
#define DXC_API_IMPORT __declspec(dllimport)
#endif
#else
#ifndef DXC_API_IMPORT
#define DXC_API_IMPORT __attribute__ ((visibility ("default")))
#endif
#endif

#ifdef _WIN32
#define DECLARE_CROSS_PLATFORM_UUIDOF(T)
#define DEFINE_CROSS_PLATFORM_UUIDOF(T)
#else
#include <dlfcn.h>
#include "dxc/Support/WinAdapter.h"
#endif

struct IMalloc;

struct IDxcIncludeHandler;

typedef HRESULT (__stdcall *DxcCreateInstanceProc)(
    _In_ REFCLSID   rclsid,
    _In_ REFIID     riid,
    _Out_ LPVOID*   ppv
);

typedef HRESULT(__stdcall *DxcCreateInstance2Proc)(
  _In_ IMalloc    *pMalloc,
  _In_ REFCLSID   rclsid,
  _In_ REFIID     riid,
  _Out_ LPVOID*   ppv
  );

/// <summary>
/// Creates a single uninitialized object of the class associated with a specified CLSID.
/// </summary>
/// <param name="rclsid">
/// The CLSID associated with the data and code that will be used to create the object.
/// </param>
/// <param name="riid">
/// A reference to the identifier of the interface to be used to communicate 
/// with the object.
/// </param>
/// <param name="ppv">
/// Address of pointer variable that receives the interface pointer requested
/// in riid. Upon successful return, *ppv contains the requested interface
/// pointer. Upon failure, *ppv contains NULL.</param>
/// <remarks>
/// While this function is similar to CoCreateInstance, there is no COM involvement.
/// </remarks>

extern "C"
DXC_API_IMPORT HRESULT __stdcall DxcCreateInstance(
  _In_ REFCLSID   rclsid,
  _In_ REFIID     riid,
  _Out_ LPVOID*   ppv
  );

extern "C"
DXC_API_IMPORT HRESULT __stdcall DxcCreateInstance2(
  _In_ IMalloc    *pMalloc,
  _In_ REFCLSID   rclsid,
  _In_ REFIID     riid,
  _Out_ LPVOID*   ppv
);

// For convenience, equivalent definitions to CP_UTF8 and CP_UTF16.
#define DXC_CP_UTF8 65001
#define DXC_CP_UTF16 1200
// Use DXC_CP_ACP for: Binary;  ANSI Text;  Autodetect UTF with BOM
#define DXC_CP_ACP 0

// This flag indicates that the shader hash was computed taking into account source information (-Zss)
#define DXC_HASHFLAG_INCLUDES_SOURCE  1

// Hash digest type for ShaderHash
typedef struct DxcShaderHash {
  UINT32 Flags; // DXC_HASHFLAG_*
  BYTE HashDigest[16];
} DxcShaderHash;

#define DXC_FOURCC(ch0, ch1, ch2, ch3) (                     \
  (UINT32)(UINT8)(ch0)        | (UINT32)(UINT8)(ch1) << 8  | \
  (UINT32)(UINT8)(ch2) << 16  | (UINT32)(UINT8)(ch3) << 24   \
  )
#define DXC_PART_PDB                      DXC_FOURCC('I', 'L', 'D', 'B')
#define DXC_PART_PDB_NAME                 DXC_FOURCC('I', 'L', 'D', 'N')
#define DXC_PART_PRIVATE_DATA             DXC_FOURCC('P', 'R', 'I', 'V')
#define DXC_PART_ROOT_SIGNATURE           DXC_FOURCC('R', 'T', 'S', '0')
#define DXC_PART_DXIL                     DXC_FOURCC('D', 'X', 'I', 'L')
#define DXC_PART_REFLECTION_DATA          DXC_FOURCC('S', 'T', 'A', 'T')
#define DXC_PART_SHADER_HASH              DXC_FOURCC('H', 'A', 'S', 'H')
#define DXC_PART_INPUT_SIGNATURE          DXC_FOURCC('I', 'S', 'G', '1')
#define DXC_PART_OUTPUT_SIGNATURE         DXC_FOURCC('O', 'S', 'G', '1')
#define DXC_PART_PATCH_CONSTANT_SIGNATURE DXC_FOURCC('P', 'S', 'G', '1')

// Some option arguments are defined here for continuity with D3DCompile interface
#define DXC_ARG_DEBUG L"-Zi"
#define DXC_ARG_SKIP_VALIDATION L"-Vd"
#define DXC_ARG_SKIP_OPTIMIZATIONS L"-Od"
#define DXC_ARG_PACK_MATRIX_ROW_MAJOR L"-Zpr"
#define DXC_ARG_PACK_MATRIX_COLUMN_MAJOR L"-Zpc"
#define DXC_ARG_AVOID_FLOW_CONTROL L"-Gfa"
#define DXC_ARG_PREFER_FLOW_CONTROL L"-Gfp"
#define DXC_ARG_ENABLE_STRICTNESS L"-Ges"
#define DXC_ARG_ENABLE_BACKWARDS_COMPATIBILITY L"-Gec"
#define DXC_ARG_IEEE_STRICTNESS L"-Gis"
#define DXC_ARG_OPTIMIZATION_LEVEL0 L"-O0"
#define DXC_ARG_OPTIMIZATION_LEVEL1 L"-O1"
#define DXC_ARG_OPTIMIZATION_LEVEL2 L"-O2"
#define DXC_ARG_OPTIMIZATION_LEVEL3 L"-O3"
#define DXC_ARG_WARNINGS_ARE_ERRORS L"-WX"
#define DXC_ARG_RESOURCES_MAY_ALIAS L"-res_may_alias"
#define DXC_ARG_ALL_RESOURCES_BOUND L"-all_resources_bound"
#define DXC_ARG_DEBUG_NAME_FOR_SOURCE L"-Zss"
#define DXC_ARG_DEBUG_NAME_FOR_BINARY L"-Zsb"

// IDxcBlob is an alias of ID3D10Blob and ID3DBlob
struct __declspec(uuid("8BA5FB08-5195-40e2-AC58-0D989C3A0102"))
IDxcBlob : public IUnknown {
public:
  virtual LPVOID STDMETHODCALLTYPE GetBufferPointer(void) = 0;
  virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(void) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcBlob)
};

struct __declspec(uuid("7241d424-2646-4191-97c0-98e96e42fc68"))
IDxcBlobEncoding : public IDxcBlob {
public:
  virtual HRESULT STDMETHODCALLTYPE GetEncoding(_Out_ BOOL *pKnown,
                                                _Out_ UINT32 *pCodePage) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcBlobEncoding)
};

// Notes on IDxcBlobUtf16 and IDxcBlobUtf8
// These guarantee null-terminated text and the stated encoding.
// GetBufferSize() will return the size in bytes, including null-terminator
// GetStringLength() will return the length in characters, excluding the null-terminator
// Name strings will use IDxcBlobUtf16, while other string output blobs,
// such as errors/warnings, preprocessed HLSL, or other text will be based
// on the -encoding option.

// The API will use this interface for output name strings
struct __declspec(uuid("A3F84EAB-0FAA-497E-A39C-EE6ED60B2D84"))
IDxcBlobUtf16 : public IDxcBlobEncoding {
public:
  virtual LPCWSTR STDMETHODCALLTYPE GetStringPointer(void) = 0;
  virtual SIZE_T STDMETHODCALLTYPE GetStringLength(void) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcBlobUtf16)
};
struct __declspec(uuid("3DA636C9-BA71-4024-A301-30CBF125305B"))
IDxcBlobUtf8 : public IDxcBlobEncoding {
public:
  virtual LPCSTR STDMETHODCALLTYPE GetStringPointer(void) = 0;
  virtual SIZE_T STDMETHODCALLTYPE GetStringLength(void) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcBlobUtf8)
};

struct __declspec(uuid("7f61fc7d-950d-467f-b3e3-3c02fb49187c"))
IDxcIncludeHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE LoadSource(
    _In_z_ LPCWSTR pFilename,                                 // Candidate filename.
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource  // Resultant source object for included file, nullptr if not found.
    ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcIncludeHandler)
};

// Structure for supplying bytes or text input to Dxc APIs.
// Use Encoding = 0 for non-text bytes, ANSI text, or unknown with BOM.
typedef struct DxcBuffer {
  LPCVOID Ptr;
  SIZE_T Size;
  UINT Encoding;
} DxcText;

struct DxcDefine {
  LPCWSTR Name;
  _Maybenull_ LPCWSTR Value;
};

struct __declspec(uuid("73EFFE2A-70DC-45F8-9690-EFF64C02429D"))
IDxcCompilerArgs : public IUnknown {
  // Pass GetArguments() and GetCount() to Compile
  virtual LPCWSTR* STDMETHODCALLTYPE GetArguments() = 0;
  virtual UINT32 STDMETHODCALLTYPE GetCount() = 0;

  // Add additional arguments or defines here, if desired.
  virtual HRESULT STDMETHODCALLTYPE AddArguments(
    _In_opt_count_(argCount) LPCWSTR *pArguments,       // Array of pointers to arguments to add
    _In_ UINT32 argCount                                // Number of arguments to add
  ) = 0;
  virtual HRESULT STDMETHODCALLTYPE AddArgumentsUTF8(
    _In_opt_count_(argCount)LPCSTR *pArguments,         // Array of pointers to UTF-8 arguments to add
    _In_ UINT32 argCount                                // Number of arguments to add
  ) = 0;
  virtual HRESULT STDMETHODCALLTYPE AddDefines(
      _In_count_(defineCount) const DxcDefine *pDefines, // Array of defines
      _In_ UINT32 defineCount                            // Number of defines
  ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcCompilerArgs)
};

//////////////////////////
// Legacy Interfaces
/////////////////////////

// NOTE: IDxcUtils replaces IDxcLibrary
struct __declspec(uuid("e5204dc7-d18c-4c3c-bdfb-851673980fe7"))
IDxcLibrary : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetMalloc(_In_opt_ IMalloc *pMalloc) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateBlobFromFile(
    _In_z_ LPCWSTR pFileName, _In_opt_ UINT32* codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingFromPinned(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnHeapCopy(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnMalloc(
    _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateStreamFromBlobReadOnly(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcLibrary)
};

// NOTE: IDxcResult replaces IDxcOperationResult
struct __declspec(uuid("CEDB484A-D4E9-445A-B991-CA21CA157DC2"))
IDxcOperationResult : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetStatus(_Out_ HRESULT *pStatus) = 0;

  // GetResult returns the main result of the operation.
  // This corresponds to:
  // DXC_OUT_OBJECT - Compile() with shader or library target
  // DXC_OUT_DISASSEMBLY - Disassemble()
  // DXC_OUT_HLSL - Compile() with -P
  // DXC_OUT_ROOT_SIGNATURE - Compile() with rootsig_* target
  virtual HRESULT STDMETHODCALLTYPE GetResult(_COM_Outptr_result_maybenull_ IDxcBlob **ppResult) = 0;

  // GetErrorBuffer Corresponds to DXC_OUT_ERRORS.
  virtual HRESULT STDMETHODCALLTYPE GetErrorBuffer(_COM_Outptr_result_maybenull_ IDxcBlobEncoding **ppErrors) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcOperationResult)
};

// NOTE: IDxcCompiler3 replaces IDxcCompiler and IDxcCompiler2
struct __declspec(uuid("8c210bf3-011f-4422-8d70-6f9acb8db617"))
IDxcCompiler : public IUnknown {
  // Compile a single entry point to the target shader model
  virtual HRESULT STDMETHODCALLTYPE Compile(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_z_ LPCWSTR pEntryPoint,               // entry point name
    _In_z_ LPCWSTR pTargetProfile,                // shader profile to compile
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount)
      const DxcDefine *pDefines,                  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Compiler output status, buffer, and errors
  ) = 0;

  // Preprocess source text
  virtual HRESULT STDMETHODCALLTYPE Preprocess(
    _In_ IDxcBlob *pSource,                       // Source text to preprocess
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount)
      const DxcDefine *pDefines,                  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Preprocessor output status, buffer, and errors
  ) = 0;

  // Disassemble a program.
  virtual HRESULT STDMETHODCALLTYPE Disassemble(
    _In_ IDxcBlob *pSource,                         // Program to disassemble.
    _COM_Outptr_ IDxcBlobEncoding **ppDisassembly   // Disassembly text.
    ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcCompiler)
};

// NOTE: IDxcCompiler3 replaces IDxcCompiler and IDxcCompiler2
struct __declspec(uuid("A005A9D9-B8BB-4594-B5C9-0E633BEC4D37"))
IDxcCompiler2 : public IDxcCompiler {
  // Compile a single entry point to the target shader model with debug information.
  virtual HRESULT STDMETHODCALLTYPE CompileWithDebug(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_z_ LPCWSTR pEntryPoint,               // Entry point name
    _In_z_ LPCWSTR pTargetProfile,                // Shader profile to compile
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount)
      const DxcDefine *pDefines,                  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
    _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob. (Must be HeapFree()'d!)
    _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
  ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcCompiler2)
};

struct __declspec(uuid("F1B5BE2A-62DD-4327-A1C2-42AC1E1E78E6"))
IDxcLinker : public IUnknown {
public:
  // Register a library with name to ref it later.
  virtual HRESULT RegisterLibrary(
    _In_opt_ LPCWSTR pLibName,          // Name of the library.
    _In_ IDxcBlob *pLib                 // Library blob.
  ) = 0;

  // Links the shader and produces a shader blob that the Direct3D runtime can
  // use.
  virtual HRESULT STDMETHODCALLTYPE Link(
    _In_opt_ LPCWSTR pEntryName,        // Entry point name
    _In_ LPCWSTR pTargetProfile,        // shader profile to link
    _In_count_(libCount)
        const LPCWSTR *pLibNames,       // Array of library names to link
    _In_ UINT32 libCount,               // Number of libraries to link
    _In_opt_count_(argCount) const LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,               // Number of arguments
    _COM_Outptr_
        IDxcOperationResult **ppResult  // Linker output status, buffer, and errors
  ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcLinker)
};

/////////////////////////
// Latest interfaces. Please use these
////////////////////////

// NOTE: IDxcUtils replaces IDxcLibrary
struct __declspec(uuid("4605C4CB-2019-492A-ADA4-65F20BB7D67F"))
IDxcUtils : public IUnknown {
  // Create a sub-blob that holds a reference to the outer blob and points to its memory.
  virtual HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) = 0;

  // For codePage, use 0 (or DXC_CP_ACP) for raw binary or ANSI code page

  // Creates a blob referencing existing memory, with no copy.
  // User must manage the memory lifetime separately.
  // (was: CreateBlobWithEncodingFromPinned)
  virtual HRESULT STDMETHODCALLTYPE CreateBlobFromPinned(
    _In_bytecount_(size) LPCVOID pData, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;

  // Create blob, taking ownership of memory allocated with supplied allocator.
  // (was: CreateBlobWithEncodingOnMalloc)
  virtual HRESULT STDMETHODCALLTYPE MoveToBlob(
    _In_bytecount_(size) LPCVOID pData, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;

  ////
  // New blobs and copied contents are allocated with the current allocator

  // Copy blob contents to memory owned by the new blob.
  // (was: CreateBlobWithEncodingOnHeapCopy)
  virtual HRESULT STDMETHODCALLTYPE CreateBlob(
    _In_bytecount_(size) LPCVOID pData, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;

  // (was: CreateBlobFromFile)
  virtual HRESULT STDMETHODCALLTYPE LoadFile(
    _In_z_ LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateReadOnlyStreamFromBlob(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) = 0;

  // Create default file-based include handler
  virtual HRESULT STDMETHODCALLTYPE CreateDefaultIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) = 0;

  // Convert or return matching encoded text blobs
  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobUtf8 **pBlobEncoding) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobUtf16 **pBlobEncoding) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetDxilContainerPart(
    _In_ const DxcBuffer *pShader,
    _In_ UINT32 DxcPart,
    _Outptr_result_nullonfailure_ void **ppPartData,
    _Out_ UINT32 *pPartSizeInBytes) = 0;

  // Create reflection interface from serialized Dxil container, or DXC_PART_REFLECTION_DATA.
  // TBD: Require part header for RDAT?  (leaning towards yes)
  virtual HRESULT STDMETHODCALLTYPE CreateReflection(
    _In_ const DxcBuffer *pData, REFIID iid, void **ppvReflection) = 0;

  virtual HRESULT STDMETHODCALLTYPE BuildArguments(
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_z_ LPCWSTR pEntryPoint,               // Entry point name. (-E)
    _In_z_ LPCWSTR pTargetProfile,                // Shader profile to compile. (-T)
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount)
      const DxcDefine *pDefines,                  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _COM_Outptr_ IDxcCompilerArgs **ppArgs        // Arguments you can use with Compile() method
  ) = 0;

  // Takes the shader PDB and returns the hash and the container inside it
  virtual HRESULT STDMETHODCALLTYPE GetPDBContents(
    _In_ IDxcBlob *pPDBBlob, _COM_Outptr_ IDxcBlob **ppHash, _COM_Outptr_ IDxcBlob **ppContainer) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcUtils)
};

// For use with IDxcResult::[Has|Get]Output dxcOutKind argument
// Note: text outputs returned from version 2 APIs are UTF-8 or UTF-16 based on -encoding option
typedef enum DXC_OUT_KIND {
  DXC_OUT_NONE = 0,
  DXC_OUT_OBJECT = 1,         // IDxcBlob - Shader or library object
  DXC_OUT_ERRORS = 2,         // IDxcBlobUtf8 or IDxcBlobUtf16
  DXC_OUT_PDB = 3,            // IDxcBlob
  DXC_OUT_SHADER_HASH = 4,    // IDxcBlob - DxcShaderHash of shader or shader with source info (-Zsb/-Zss)
  DXC_OUT_DISASSEMBLY = 5,    // IDxcBlobUtf8 or IDxcBlobUtf16 - from Disassemble
  DXC_OUT_HLSL = 6,           // IDxcBlobUtf8 or IDxcBlobUtf16 - from Preprocessor or Rewriter
  DXC_OUT_TEXT = 7,           // IDxcBlobUtf8 or IDxcBlobUtf16 - other text, such as -ast-dump or -Odump
  DXC_OUT_REFLECTION = 8,     // IDxcBlob - RDAT part with reflection data
  DXC_OUT_ROOT_SIGNATURE = 9, // IDxcBlob - Serialized root signature output

  DXC_OUT_FORCE_DWORD = 0xFFFFFFFF
} DXC_OUT_KIND;

struct __declspec(uuid("58346CDA-DDE7-4497-9461-6F87AF5E0659"))
IDxcResult : public IDxcOperationResult {
  virtual BOOL STDMETHODCALLTYPE HasOutput(_In_ DXC_OUT_KIND dxcOutKind) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetOutput(_In_ DXC_OUT_KIND dxcOutKind,
    _In_ REFIID iid, _COM_Outptr_opt_result_maybenull_ void **ppvObject,
    _COM_Outptr_ IDxcBlobUtf16 **ppOutputName) = 0;

  virtual UINT32 GetNumOutputs() = 0;
  virtual DXC_OUT_KIND GetOutputByIndex(UINT32 Index) = 0;
  virtual DXC_OUT_KIND PrimaryOutput() = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcResult)
};

struct __declspec(uuid("228B4687-5A6A-4730-900C-9702B2203F54"))
IDxcCompiler3 : public IUnknown {
  // Compile a single entry point to the target shader model,
  // Compile a library to a library target (-T lib_*),
  // Compile a root signature (-T rootsig_*), or
  // Preprocess HLSL source (-P)
  virtual HRESULT STDMETHODCALLTYPE Compile(
    _In_ const DxcBuffer *pSource,                // Source text to compile
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _In_ REFIID riid, _Out_ LPVOID *ppResult      // IDxcResult: status, buffer, and errors
  ) = 0;

  // Disassemble a program.
  virtual HRESULT STDMETHODCALLTYPE Disassemble(
    _In_ const DxcBuffer *pObject,                // Program to disassemble: dxil container or bitcode.
    _In_ REFIID riid, _Out_ LPVOID *ppResult      // IDxcResult: status, disassembly text, and errors
    ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcCompiler3)
};

static const UINT32 DxcValidatorFlags_Default = 0;
static const UINT32 DxcValidatorFlags_InPlaceEdit = 1;  // Validator is allowed to update shader blob in-place.
static const UINT32 DxcValidatorFlags_RootSignatureOnly = 2;
static const UINT32 DxcValidatorFlags_ModuleOnly = 4;
static const UINT32 DxcValidatorFlags_ValidMask = 0x7;

struct __declspec(uuid("A6E82BD2-1FD7-4826-9811-2857E797F49A"))
IDxcValidator : public IUnknown {
  // Validate a shader.
  virtual HRESULT STDMETHODCALLTYPE Validate(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
    ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcValidator)
};

struct __declspec(uuid("334b1f50-2292-4b35-99a1-25588d8c17fe"))
IDxcContainerBuilder : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pDxilContainerHeader) = 0;                // Loads DxilContainer to the builder
  virtual HRESULT STDMETHODCALLTYPE AddPart(_In_ UINT32 fourCC, _In_ IDxcBlob *pSource) = 0;      // Part to add to the container
  virtual HRESULT STDMETHODCALLTYPE RemovePart(_In_ UINT32 fourCC) = 0;                           // Remove the part with fourCC
  virtual HRESULT STDMETHODCALLTYPE SerializeContainer(_Out_ IDxcOperationResult **ppResult) = 0; // Builds a container of the given container builder state

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcContainerBuilder)
};

struct __declspec(uuid("091f7a26-1c1f-4948-904b-e6e3a8a771d5"))
IDxcAssembler : public IUnknown {
  // Assemble dxil in ll or llvm bitcode to DXIL container.
  virtual HRESULT STDMETHODCALLTYPE AssembleToContainer(
    _In_ IDxcBlob *pShader,                       // Shader to assemble.
    _COM_Outptr_ IDxcOperationResult **ppResult   // Assembly output status, buffer, and errors
    ) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcAssembler)
};

struct __declspec(uuid("d2c21b26-8350-4bdc-976a-331ce6f4c54c"))
IDxcContainerReflection : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pContainer) = 0; // Container to load.
  virtual HRESULT STDMETHODCALLTYPE GetPartCount(_Out_ UINT32 *pResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPartKind(UINT32 idx, _Out_ UINT32 *pResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPartContent(UINT32 idx, _COM_Outptr_ IDxcBlob **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE FindFirstPartKind(UINT32 kind, _Out_ UINT32 *pResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPartReflection(UINT32 idx, REFIID iid, void **ppvObject) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcContainerReflection)
};

struct __declspec(uuid("AE2CD79F-CC22-453F-9B6B-B124E7A5204C"))
IDxcOptimizerPass : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetOptionName(_COM_Outptr_ LPWSTR *ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDescription(_COM_Outptr_ LPWSTR *ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetOptionArgCount(_Out_ UINT32 *pCount) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetOptionArgName(UINT32 argIndex, _COM_Outptr_ LPWSTR *ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetOptionArgDescription(UINT32 argIndex, _COM_Outptr_ LPWSTR *ppResult) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcOptimizerPass)
};

struct __declspec(uuid("25740E2E-9CBA-401B-9119-4FB42F39F270"))
IDxcOptimizer : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetAvailablePassCount(_Out_ UINT32 *pCount) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAvailablePass(UINT32 index, _COM_Outptr_ IDxcOptimizerPass** ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE RunOptimizer(IDxcBlob *pBlob,
    _In_count_(optionCount) LPCWSTR *ppOptions, UINT32 optionCount,
    _COM_Outptr_ IDxcBlob **pOutputModule,
    _COM_Outptr_opt_ IDxcBlobEncoding **ppOutputText) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcOptimizer)
};

static const UINT32 DxcVersionInfoFlags_None = 0;
static const UINT32 DxcVersionInfoFlags_Debug = 1; // Matches VS_FF_DEBUG
static const UINT32 DxcVersionInfoFlags_Internal = 2; // Internal Validator (non-signing)

struct __declspec(uuid("b04f5b50-2059-4f12-a8ff-a1e0cde1cc7e"))
IDxcVersionInfo : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcVersionInfo)
};

struct __declspec(uuid("fb6904c4-42f0-4b62-9c46-983af7da7c83"))
IDxcVersionInfo2 : public IDxcVersionInfo {
  virtual HRESULT STDMETHODCALLTYPE GetCommitInfo(_Out_ UINT32 *pCommitCount, _Out_ char **pCommitHash) = 0;

  DECLARE_CROSS_PLATFORM_UUIDOF(IDxcVersionInfo2)
};

// Note: __declspec(selectany) requires 'extern'
// On Linux __declspec(selectany) is removed and using 'extern' results in link error.
#ifdef _MSC_VER
#define CLSID_SCOPE __declspec(selectany) extern
#else
#define CLSID_SCOPE
#endif

CLSID_SCOPE const CLSID CLSID_DxcCompiler = {
    0x73e22d93,
    0xe6ce,
    0x47f3,
    {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};

// {EF6A8087-B0EA-4D56-9E45-D07E1A8B7806}
CLSID_SCOPE const GUID CLSID_DxcLinker = {
    0xef6a8087,
    0xb0ea,
    0x4d56,
    {0x9e, 0x45, 0xd0, 0x7e, 0x1a, 0x8b, 0x78, 0x6}};

// {CD1F6B73-2AB0-484D-8EDC-EBE7A43CA09F}
CLSID_SCOPE const CLSID CLSID_DxcDiaDataSource = {
    0xcd1f6b73,
    0x2ab0,
    0x484d,
    {0x8e, 0xdc, 0xeb, 0xe7, 0xa4, 0x3c, 0xa0, 0x9f}};

// {3E56AE82-224D-470F-A1A1-FE3016EE9F9D}
CLSID_SCOPE const CLSID CLSID_DxcCompilerArgs = {
    0x3e56ae82,
    0x224d,
    0x470f,
    {0xa1, 0xa1, 0xfe, 0x30, 0x16, 0xee, 0x9f, 0x9d}};

// {6245D6AF-66E0-48FD-80B4-4D271796748C}
CLSID_SCOPE const GUID CLSID_DxcLibrary = {
    0x6245d6af,
    0x66e0,
    0x48fd,
    {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}};

CLSID_SCOPE const GUID CLSID_DxcUtils = CLSID_DxcLibrary;

// {8CA3E215-F728-4CF3-8CDD-88AF917587A1}
CLSID_SCOPE const GUID CLSID_DxcValidator = {
    0x8ca3e215,
    0xf728,
    0x4cf3,
    {0x8c, 0xdd, 0x88, 0xaf, 0x91, 0x75, 0x87, 0xa1}};

// {D728DB68-F903-4F80-94CD-DCCF76EC7151}
CLSID_SCOPE const GUID CLSID_DxcAssembler = {
    0xd728db68,
    0xf903,
    0x4f80,
    {0x94, 0xcd, 0xdc, 0xcf, 0x76, 0xec, 0x71, 0x51}};

// {b9f54489-55b8-400c-ba3a-1675e4728b91}
CLSID_SCOPE const GUID CLSID_DxcContainerReflection = {
    0xb9f54489,
    0x55b8,
    0x400c,
    {0xba, 0x3a, 0x16, 0x75, 0xe4, 0x72, 0x8b, 0x91}};

// {AE2CD79F-CC22-453F-9B6B-B124E7A5204C}
CLSID_SCOPE const GUID CLSID_DxcOptimizer = {
    0xae2cd79f,
    0xcc22,
    0x453f,
    {0x9b, 0x6b, 0xb1, 0x24, 0xe7, 0xa5, 0x20, 0x4c}};

// {94134294-411f-4574-b4d0-8741e25240d2}
CLSID_SCOPE const GUID CLSID_DxcContainerBuilder = {
    0x94134294,
    0x411f,
    0x4574,
    {0xb4, 0xd0, 0x87, 0x41, 0xe2, 0x52, 0x40, 0xd2}};
#endif
