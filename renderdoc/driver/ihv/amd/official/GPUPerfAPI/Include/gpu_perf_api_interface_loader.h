//==============================================================================
// Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief GPA Interface Loader Utility header file
//==============================================================================

// Note: For usage, copy and paste (and then uncomment) the
// following four lines into a compilation unit that uses
// this header file. These are needed to initialize
// static/extern data declared in this header file:

// #ifdef __cplusplus
// GpaApiManager* GpaApiManager::gpa_api_manager_ = nullptr;
// #endif
// GpaFuncTableInfo* gpa_function_table_info = NULL;

// In order to use this header file with a debug build of GPA
// the "USE_DEBUG_GPA" preprocessor macro should be defined before
// including this header file

// In order to use this header file with an internal build of GPA
// the "USE_INTERNAL_GPA" preprocessor macro should be defined before
// including this header file

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_INTERFACE_LOADER_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_INTERFACE_LOADER_H_

#ifdef _WIN32
#include <Windows.h>
#define GPA_MAX_PATH MAX_PATH  ///< Macro for max path length.
#ifdef _WIN64
#define GPA_IS_64_BIT  ///< Macro specifying 64-bit build.
#else
#define GPA_IS_32_BIT  ///< Macro specifying 32-bit build.
#endif
#else
#include <dlfcn.h>
#include <unistd.h>
#define GPA_MAX_PATH 4096  ///< Macro for max path length.
#ifdef __x86_64__
#define GPA_IS_64_BIT  ///< Macro specifying 64-bit build.
#else
#define GPA_IS_32_BIT  ///< Macro specifying 32-bit build.
#endif
#endif

#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
#include <string>
#endif

#include "gpu_perf_api.h"

#ifdef UNICODE
typedef wchar_t LocaleChar;  ///< Typedef for ANSI vs. Unicode character.
#ifdef __cplusplus
typedef std::wstring LocaleString;  ///< Typedef for ANSI vs. Unicode string.
#endif

#define TFORMAT2(x) L##x        ///< Macro for string expansion.
#define TFORMAT(x) TFORMAT2(x)  ///< Macro for string expansion.

#define STR_CAT(dest, dest_size, src) wcscat_s(dest, dest_size, src)                    ///< Macro for safe strcat.
#define STR_COPY(dest, dest_size, src) wcscpy_s(dest, dest_size, src)                   ///< Macro for safe strcpy.
#define STR_NCOPY(dest, dest_size, src, count) wcsncpy_s(dest, dest_size, src, count);  ///< Macro for safe strncpy.
#define STR_LEN(str, str_length) wcsnlen_s(str, str_length)                             ///< Macro for safe strnlen.
#define MEM_CPY(dest, src, count) wmemcpy(dest, src, count)
#define MEM_SET(ptr, wc, num) wmemset(ptr, wc, num)

#else
typedef char        LocaleChar;    ///< Typedef for ANSI vs. Unicode character.
#ifdef __cplusplus
typedef std::string LocaleString;  ///< Typedef for ANSI vs. Unicode string.
#endif

#define TFORMAT(x) (x)  ///< Macro for string expansion.

#define STR_CAT(dest, dest_size, src) strcat_s(dest, dest_size, src)                    ///< Macro for safe strcat.
#define STR_COPY(dest, dest_size, src) strcpy_s(dest, dest_size, src)                   ///< Macro for safe strcpy.
#define STR_NCOPY(dest, dest_size, src, count) strncpy_s(dest, dest_size, src, count);  ///< Macro for safe strncpy.
#define STR_LEN(str, str_length) strnlen_s(str, str_length)                             ///< Macro for safe strnlen.
#define MEM_CPY(dest, src, count) memcpy(dest, src, count)
#define MEM_SET(ptr, wc, num) memset(ptr, wc, num)
#endif

#define GPA_OPENCL_LIB TFORMAT("GPUPerfAPICL")       ///< Macro for base name of GPA OpenCL library.
#define GPA_OPENGL_LIB TFORMAT("GPUPerfAPIGL")       ///< Macro for base name of GPA OpenGL library.
#define GPA_DIRECTX11_LIB TFORMAT("GPUPerfAPIDX11")  ///< Macro for base name of GPA DirectX 11 library.
#define GPA_DIRECTX12_LIB TFORMAT("GPUPerfAPIDX12")  ///< Macro for base name of GPA DirectX 12 library.
#define GPA_VULKAN_LIB TFORMAT("GPUPerfAPIVK")       ///< Macro for base name of GPA Vulkan library.

#ifdef _WIN32
#define GPA_LIB_PREFIX TFORMAT("")           ///< Macro for platform-specific lib file prefix.
#define GPA_LIB_SUFFIX TFORMAT(".dll")       ///< Macro for platform-specific lib file suffix.
#define GPA_X64_ARCH_SUFFIX TFORMAT("-x64")  ///< Macro for 64-bit lib file architecture suffix.
#define GPA_X86_ARCH_SUFFIX TFORMAT("")      ///< Macro for 32-bit lib file architecture suffix.
#else
#define GPA_LIB_PREFIX TFORMAT("lib")      ///< Macro for platform-specific lib file prefix.
#define GPA_LIB_SUFFIX TFORMAT(".so")      ///< Macro for platform-specific lib file suffix.
#define GPA_X64_ARCH_SUFFIX TFORMAT("")    ///< Macro for 64-bit lib file architecture suffix.
#define GPA_X86_ARCH_SUFFIX TFORMAT("32")  ///< Macro for 32-bit lib file architecture suffix.
#endif

#define GPA_DEBUG_SUFFIX TFORMAT("-d")            ///< Macro for debug suffix.
#define GPA_INTERNAL_SUFFIX TFORMAT("-Internal")  ///< Macro for internal build lib file suffix.

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))  ///< Macro to calculate array length.

#define GPA_GET_FUNCTION_TABLE_FUNCTION_NAME "GpaGetFuncTable"  ///< Macro for GpaGetFuncTable entrypoint.

/// @brief GPAFunctionTableInfo structure.
typedef struct _GpaFuncTableInfo
{
    GpaApiType        gpa_api_type;          ///< API type.
    GpaFunctionTable* gpa_func_table;        ///< GPA function table.
    LibHandle         lib_handle;            ///< Library handle.
    void*             next_func_table_info;  ///< Pointer to next function table info.
} GpaFuncTableInfo;

extern GpaFuncTableInfo* gpa_function_table_info;  ///< Global instance of GPA function table info.

/// @brief Replaces the Windows style path separator to Unix style.
///
/// @param [in] file_path File path.
/// @param [out] last_separator_position If not null, last separator position in the path string will be returned.
static inline void Win2UnixPathSeparator(LocaleChar* file_path, unsigned int* last_separator_position)
{
    unsigned int counter        = 0;
    unsigned int last_slash_pos = 0;

    while ('\0' != file_path[counter])
    {
        if ('\\' == file_path[counter])
        {
            file_path[counter] = '/';
        }

        if ('/' == file_path[counter])
        {
            last_slash_pos = counter;
        }

        ++counter;
    }

    if (NULL != last_separator_position)
    {
        *last_separator_position = last_slash_pos;
    }
}

/// @brief Get the current working directory.
///
/// @return The current working directory.
static const LocaleChar* GpaInterfaceLoaderGetWorkingDirectoryPath()
{
    static LocaleChar working_directory_static_string[GPA_MAX_PATH];

    working_directory_static_string[0]                   = 0;
    LocaleChar temp_working_directory_path[GPA_MAX_PATH] = {0};

#ifdef _WIN32
    GetModuleFileName(NULL, temp_working_directory_path, ARRAY_LENGTH(temp_working_directory_path));
#else
    int  len;
    char temp_working_directory_path_in_char[GPA_MAX_PATH] = {0};
    len = readlink("/proc/self/exe", temp_working_directory_path_in_char, ARRAY_LENGTH(temp_working_directory_path_in_char) - 1);

    if (len != -1)
    {
        temp_working_directory_path[len] = '\0';
    }

#ifdef UNICODE
    mbstowcs(temp_working_directory_path, temp_working_directory_path_in_char, ARRAY_LENGTH(temp_working_directory_path_in_char));
#else
    MEM_CPY(temp_working_directory_path, temp_working_directory_path_in_char, ARRAY_LENGTH(temp_working_directory_path_in_char));
#endif
#endif

    unsigned int last_slash_position = 0;

    Win2UnixPathSeparator(temp_working_directory_path, &last_slash_position);

    MEM_SET(working_directory_static_string, 0, ARRAY_LENGTH(working_directory_static_string));
    STR_NCOPY(working_directory_static_string, ARRAY_LENGTH(working_directory_static_string), temp_working_directory_path, last_slash_position);
    return working_directory_static_string;
}

/// @brief Gets the library file name for the given API.
///
/// @param [in] gpa_api_type Type of the API.
///
/// @return Library file name.
static inline const LocaleChar* GpaInterfaceLoaderGetLibraryFileName(GpaApiType gpa_api_type)
{
    static LocaleChar filename_static_string[GPA_MAX_PATH];

    filename_static_string[0] = 0;

    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_LIB_PREFIX);

    switch (gpa_api_type)
    {
#ifdef _WIN32
    case kGpaApiDirectx11:
        STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_DIRECTX11_LIB);
        break;

    case kGpaApiDirectx12:
        STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_DIRECTX12_LIB);
        break;

    case kGpaApiOpencl:
        STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_OPENCL_LIB);
        break;
#endif

    case kGpaApiOpengl:
        STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_OPENGL_LIB);
        break;

    case kGpaApiVulkan:
        STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_VULKAN_LIB);
        break;

    default:
        MEM_SET(filename_static_string, 0, ARRAY_LENGTH(filename_static_string));
        return filename_static_string;
    }

#ifdef GPA_IS_64_BIT
    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_X64_ARCH_SUFFIX);
#else
    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_X86_ARCH_SUFFIX);
#endif

#ifdef USE_DEBUG_GPA
    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_DEBUG_SUFFIX);
#endif

#ifdef USE_INTERNAL_GPA
    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_INTERNAL_SUFFIX);
#endif

    STR_CAT(filename_static_string, ARRAY_LENGTH(filename_static_string), GPA_LIB_SUFFIX);

    return filename_static_string;
}

/// @brief Get the library full path.

/// @param [in] gpa_api_type Type of the API.
/// @param [in] lib_path Local path to the library files.
///
/// @return Library with full path.
static inline const LocaleChar* GpaInterfaceLoaderGetLibraryFullPath(GpaApiType gpa_api_type, const LocaleChar* lib_path)
{
    static LocaleChar lib_path_static_string[GPA_MAX_PATH];

    lib_path_static_string[0] = 0;

    const LocaleChar* lib_name = GpaInterfaceLoaderGetLibraryFileName(gpa_api_type);

    if (STR_LEN(lib_name, GPA_MAX_PATH) > 1)
    {
        LocaleChar temp_lib_file_name[GPA_MAX_PATH]     = {0};
        LocaleChar temp_working_directory[GPA_MAX_PATH] = {0};

        STR_COPY(temp_lib_file_name, ARRAY_LENGTH(temp_lib_file_name), lib_name);

        if (NULL == lib_path)
        {
            const LocaleChar* working_directory_path = GpaInterfaceLoaderGetWorkingDirectoryPath();
            STR_COPY(temp_working_directory, ARRAY_LENGTH(temp_working_directory), working_directory_path);
        }
        else
        {
            STR_COPY(temp_working_directory, ARRAY_LENGTH(temp_working_directory), lib_path);
            Win2UnixPathSeparator(temp_working_directory, NULL);
        }

        size_t stringLength = STR_LEN(temp_working_directory, ARRAY_LENGTH(temp_working_directory));

        if (temp_working_directory[stringLength - 1] != '/')
        {
            temp_working_directory[stringLength]     = '/';
            temp_working_directory[stringLength + 1] = '\0';
        }

        MEM_SET(lib_path_static_string, 0, ARRAY_LENGTH(lib_path_static_string));
        STR_COPY(lib_path_static_string, ARRAY_LENGTH(lib_path_static_string), temp_working_directory);
        STR_CAT(lib_path_static_string, ARRAY_LENGTH(lib_path_static_string), temp_lib_file_name);
    }

    return lib_path_static_string;
}

/// @brief Loads the dll and initialize the function table for the given API.
///
/// @param [in] api_type Type of the API to be loaded.
/// @param [in] lib_path Path to the folder containing dll.
///
/// @return The status of the operation.
/// @retval kGpaStatusOk On success.
/// @retval kGpaStatusErrorFailed If an internal error occurred.
/// @retval kGpaStatusErrorApiNotSupported The desired API is not supported on the current system.
/// @retval kGpaStatusErrorLibAlreadyLoaded The necessary library has already been loaded.
/// @retval kGpaStatusErrorLibLoadFailed The library failed to load.
static inline GpaStatus GpaInterfaceLoaderLoadApi(GpaApiType api_type, const LocaleChar* lib_path)
{
#if DISABLE_GPA
    UNREFERENCED_PARAMETER(api_type);
    UNREFERENCED_PARAMETER(lib_path);
    if (NULL == gpa_function_table_info)
    {
        gpa_function_table_info                 = (GpaFuncTableInfo*)malloc(sizeof(GpaFuncTableInfo));
        gpa_function_table_info->gpa_func_table = (GpaFunctionTable*)malloc(sizeof(GpaFunctionTable));
    }

    gpa_function_table_info->gpa_api_type                  = kGpaApiLast;
    gpa_function_table_info->lib_handle                    = NULL;
    gpa_function_table_info->gpa_func_table->major_version = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
    gpa_function_table_info->gpa_func_table->minor_version = GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER;
    GpaStatus status                                       = GpaGetFuncTable(gpa_function_table_info->gpa_func_table);
    gpa_function_table_info->next_func_table_info          = NULL;
#else
    if (NULL == gpa_function_table_info)
    {
        gpa_function_table_info = (GpaFuncTableInfo*)malloc(sizeof(GpaFuncTableInfo));

        if (NULL != gpa_function_table_info)
        {
            gpa_function_table_info->gpa_api_type         = kGpaApiNoSupport;
            gpa_function_table_info->lib_handle           = NULL;
            gpa_function_table_info->gpa_func_table       = NULL;
            gpa_function_table_info->next_func_table_info = NULL;
        }
    }

    GpaStatus status = kGpaStatusErrorFailed;

    if (NULL != gpa_function_table_info)
    {
        status = kGpaStatusOk;

        if (api_type >= kGpaApiStart && api_type < kGpaApiNoSupport)
        {
            const LocaleChar* lib_name = GpaInterfaceLoaderGetLibraryFileName(api_type);

            if (NULL == lib_name)
            {
                return kGpaStatusErrorApiNotSupported;
            }

            {
                GpaFuncTableInfo* function_table_info = gpa_function_table_info;

                while (NULL != function_table_info)
                {
                    if (function_table_info->gpa_api_type == api_type)
                    {
                        return kGpaStatusErrorLibAlreadyLoaded;
                    }

                    function_table_info = (GpaFuncTableInfo*)function_table_info->next_func_table_info;
                }
            }

            const LocaleChar* lib_full_path = GpaInterfaceLoaderGetLibraryFullPath(api_type, lib_path);
            LibHandle         lib_handle    = NULL;

#ifdef _WIN32
            lib_handle                      = LoadLibrary(lib_full_path);
#else

#ifdef UNICODE
            char lib_full_path_char[GPA_MAX_PATH];
            int  ret                = wcstombs(lib_full_path_char, lib_full_path, GPA_MAX_PATH);
            lib_full_path_char[ret] = '\0';
            lib_handle              = dlopen(lib_full_path_char, RTLD_LAZY);
#else
            lib_handle = dlopen(lib_full_path, RTLD_LAZY);
#endif

#endif
            GpaGetFuncTablePtrType gpa_get_func_table;

            if (NULL != lib_handle)
            {
#ifdef _WIN32
                gpa_get_func_table = (GpaGetFuncTablePtrType)(GetProcAddress(lib_handle, GPA_GET_FUNCTION_TABLE_FUNCTION_NAME));
#else
                gpa_get_func_table = (GpaGetFuncTablePtrType)(dlsym(lib_handle, GPA_GET_FUNCTION_TABLE_FUNCTION_NAME));
#endif

                if (NULL != gpa_get_func_table)
                {
                    GpaFunctionTable* gpa_func_table = (GpaFunctionTable*)malloc(sizeof(GpaFunctionTable));

                    if (NULL == gpa_func_table)
                    {
                        return kGpaStatusErrorFailed;
                    }

                    gpa_func_table->major_version = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
                    gpa_func_table->minor_version = GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER;
                    status                        = gpa_get_func_table((void*)(gpa_func_table));

                    if (kGpaStatusOk == status)
                    {
                        if (NULL == gpa_function_table_info->gpa_func_table)
                        {
                            gpa_function_table_info->gpa_api_type         = api_type;
                            gpa_function_table_info->gpa_func_table       = gpa_func_table;
                            gpa_function_table_info->lib_handle           = lib_handle;
                            gpa_function_table_info->next_func_table_info = NULL;
                        }
                        else
                        {
                            GpaFuncTableInfo* new_table_info = (GpaFuncTableInfo*)malloc(sizeof(GpaFuncTableInfo));

                            if (NULL == new_table_info)
                            {
                                return kGpaStatusErrorFailed;
                            }

                            new_table_info->gpa_api_type         = api_type;
                            new_table_info->gpa_func_table       = gpa_func_table;
                            new_table_info->lib_handle           = lib_handle;
                            new_table_info->next_func_table_info = NULL;

                            GpaFuncTableInfo* function_table_info = gpa_function_table_info;

                            while (NULL != function_table_info->next_func_table_info)
                            {
                                function_table_info = (GpaFuncTableInfo*)function_table_info->next_func_table_info;
                            }

                            function_table_info->next_func_table_info = new_table_info;
                        }
                    }
                    else
                    {
                        free(gpa_func_table);
                    }
                }
                else
                {
                    status = kGpaStatusErrorLibLoadFailed;
                }
            }
            else
            {
                status = kGpaStatusErrorLibLoadFailed;
            }
        }
        else
        {
            status = kGpaStatusErrorApiNotSupported;
        }
    }
#endif

    return status;
}

/// @brief Get the function table for the given API.
///
/// @param [in] gpa_api_type API type.
///
/// @return Pointer to the API function table if loaded, otherwise null pointer.
static inline const GpaFunctionTable* GpaInterfaceLoaderGetFunctionTable(GpaApiType gpa_api_type)
{
#if DISABLE_GPA
    UNREFERENCED_PARAMETER(gpa_api_type);
    return gpa_function_table_info->gpa_func_table;
#else
    GpaFuncTableInfo* function_table_info = gpa_function_table_info;
    GpaFunctionTable* function_table      = NULL;

    while (NULL != function_table_info)
    {
        if (function_table_info->gpa_api_type == gpa_api_type)
        {
            function_table = function_table_info->gpa_func_table;
            break;
        }

        function_table_info = (GpaFuncTableInfo*)function_table_info->next_func_table_info;
    }

    return function_table;
#endif
}

/// @brief Unloads the function table for the given API.
///
/// @param [in] gpa_api_type API type.
///
/// @return kGpaStatusOk upon successful operation.
static inline GpaStatus GpaInterfaceLoaderUnLoadApi(GpaApiType gpa_api_type)
{
#if DISABLE_GPA
    UNREFERENCED_PARAMETER(gpa_api_type);
    return kGpaStatusOk;
#else
    GpaStatus status = kGpaStatusErrorFailed;

    GpaFuncTableInfo* function_table_info = gpa_function_table_info;

    while (NULL != function_table_info)
    {
        if (function_table_info->gpa_api_type == gpa_api_type)
        {
            free(function_table_info->gpa_func_table);
            LibHandle lib_handle = function_table_info->lib_handle;

            if (NULL != lib_handle)
            {
#ifdef _WIN32
                FreeLibrary(lib_handle);
#else
                dlclose(lib_handle);
#endif
                function_table_info->lib_handle     = NULL;
                function_table_info->gpa_func_table = NULL;
                status                              = kGpaStatusOk;
                break;
            }
        }

        function_table_info = (GpaFuncTableInfo*)function_table_info->next_func_table_info;
    }

    return status;
#endif
}

/// @brief Clears the loader.
static inline void GpaInterfaceLoaderClearLoader()
{
#if DISABLE_GPA
    gpa_function_table_info->gpa_api_type         = kGpaApiNoSupport;
    gpa_function_table_info->lib_handle           = NULL;
    gpa_function_table_info->next_func_table_info = NULL;
    gpa_function_table_info->gpa_func_table       = NULL;
#else
    if (NULL != gpa_function_table_info)
    {
        while (NULL != gpa_function_table_info->next_func_table_info)
        {
            GpaFuncTableInfo* function_table_info = (GpaFuncTableInfo*)(gpa_function_table_info->next_func_table_info);

            if (NULL != function_table_info->lib_handle)
            {
#ifdef _WIN32
                FreeLibrary(function_table_info->lib_handle);
#else
                dlclose(function_table_info->lib_handle);
#endif
                function_table_info->lib_handle = NULL;
            }

            if (NULL != function_table_info->gpa_func_table)
            {
                free(function_table_info->gpa_func_table);
                function_table_info->gpa_func_table = NULL;
            }

            if (NULL != function_table_info->next_func_table_info)
            {
                gpa_function_table_info->next_func_table_info = function_table_info->next_func_table_info;
                free(function_table_info);
            }
            else
            {
                gpa_function_table_info->next_func_table_info = NULL;
            }
        }

        if (NULL != gpa_function_table_info->lib_handle)
        {
#ifdef _WIN32
            FreeLibrary(gpa_function_table_info->lib_handle);
#else
            dlclose(gpa_function_table_info->lib_handle);
#endif
            gpa_function_table_info->lib_handle = NULL;
        }

        if (NULL != gpa_function_table_info->gpa_func_table)
        {
            free(gpa_function_table_info->gpa_func_table);
            gpa_function_table_info->gpa_func_table = NULL;
        }

        gpa_function_table_info->gpa_api_type = kGpaApiNoSupport;
        free(gpa_function_table_info);
        gpa_function_table_info = NULL;
    }
#endif
}

#ifdef __cplusplus
/// @brief Singleton Class to handle the loading and unloading the possible APIs.
class GpaApiManager
{
public:
    /// @brief Returns the instance of the GPAApiManger.
    ///
    /// @return The instance of the GpiApiManager.
    static GpaApiManager* Instance()
    {
        if (nullptr == gpa_api_manager_)
        {
            gpa_api_manager_ = new (std::nothrow) GpaApiManager();
        }

        return gpa_api_manager_;
    }

    /// @brief Deletes the static instance instance.
    static void DeleteInstance()
    {
        if (nullptr != gpa_api_manager_)
        {
            delete gpa_api_manager_;
            gpa_api_manager_ = nullptr;
        }
    }

    /// @brief Loads the dll and initialize the function table for the passed API type.
    ///
    /// @param [in] api_type Type of the API to be loaded.
    /// @param[in] lib_path Path to the folder containing dll.
    ///
    /// @return The status of the operation.
    /// @retval kGpaStatusOk On success.
    /// @retval kGpaStatusErrorFailed If an internal error occurred.
    /// @retval kGpaStatusErrorApiNotSupported The desired API is not supported on the current system.
    /// @retval kGpaStatusErrorLibAlreadyLoaded The necessary library has already been loaded.
    /// @retval kGpaStatusErrorLibLoadFailed The library failed to load.
    GpaStatus LoadApi(const GpaApiType& api_type, const LocaleString lib_path = LocaleString()) const
    {
        LocaleChar lib_path_as_char[GPA_MAX_PATH] = {0};
        bool       local_path_given               = false;

        if (!lib_path.empty())
        {
            STR_COPY(lib_path_as_char, ARRAY_LENGTH(lib_path_as_char), lib_path.c_str());
            local_path_given = true;
        }

        return GpaInterfaceLoaderLoadApi(api_type, local_path_given ? lib_path_as_char : NULL);
    }

    /// @brief Unloads the function table for the passed API.
    ///
    /// @param [in] api_type API type.
    void UnloadApi(const GpaApiType& apiType) const
    {
        GpaInterfaceLoaderUnLoadApi(apiType);
    }

    /// @brief Get the function table for the passed API.
    ///
    /// @param [in] api_type API type.
    ///
    /// @return Pointer to the API function table if loaded, otherwise null pointer.
    GpaFunctionTable* GetFunctionTable(const GpaApiType& api_type) const
    {
        return const_cast<GpaFunctionTable*>(GpaInterfaceLoaderGetFunctionTable(api_type));
    }

    /// @brief Gets the library file name for the given API.
    ///
    /// @param [in] api_type Type of the API.
    ///
    /// @return Library file name string.
    LocaleString GetLibraryFileName(const GpaApiType& api_type) const
    {
        return LocaleString(GpaInterfaceLoaderGetLibraryFileName(api_type));
    }

    /// @brief Get the library full path.
    ///
    /// @param [in] api_type Type of the API.
    /// @param [in,opt] lib_path Local path to the library files.
    ///
    /// @return Library with full path string.
    LocaleString GetLibraryFullPath(const GpaApiType& api_type, const LocaleString lib_path = LocaleString()) const
    {
        LocaleChar lib_path_as_char[GPA_MAX_PATH] = {0};
        bool       local_path_given              = false;

        if (!lib_path.empty())
        {
            STR_COPY(lib_path_as_char, ARRAY_LENGTH(lib_path_as_char), lib_path.c_str());
            local_path_given = true;
        }

        return LocaleString(GpaInterfaceLoaderGetLibraryFullPath(api_type, local_path_given ? lib_path_as_char : NULL));
    }

private:
    /// @brief Constructor.
    GpaApiManager() = default;

    /// @brief Destructor.
    ~GpaApiManager()
    {
        GpaInterfaceLoaderClearLoader();
    }

    static GpaApiManager* gpa_api_manager_;  ///< GPA Api Manager pointer.
};

#endif  //__cplusplus

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_INTERFACE_LOADER_H_
