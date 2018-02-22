//==============================================================================
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief GPA Interface Loader Utility header file
//==============================================================================

#ifndef _GPA_INTERFACE_LOADER_H_
#define _GPA_INTERFACE_LOADER_H_

#ifdef _WIN32
    #include <windows.h>
    #define GPA_MAX_PATH MAX_PATH
    #ifdef _WIN64
        #define GPA_IS_64_BIT
    #else
        #define GPA_IS_32_BIT
    #endif
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #define GPA_MAX_PATH 4096
    #ifdef __x86_64__
        #define GPA_IS_64_BIT
    #else
        #define GPA_IS_32_BIT
    #endif
#endif

#include "GPUPerfAPI.h"
#include <map>
#include <stdint.h>
#include <algorithm>

#ifdef UNICODE
    typedef wchar_t LocaleChar;
    typedef std::wstring LocaleString;
    #define GPA_OPENCL_LIB L"GPUPerfAPICL"
    #define GPA_OPENGL_LIB L"GPUPerfAPIGL"
    #define GPA_OPENGL_ES_LIB L"GPUPerfAPIGLES"
    #define GPA_DIRECTX11_LIB L"GPUPerfAPIDX11"
    #define GPA_DIRECTX12_LIB L"GPUPerfAPIDX12"
    #define GPA_HSA_LIB L"GPUPerfAPIHSA"

    #ifdef _WIN32
        #define GPA_LIB_PREFIX L""
        #define GPA_LIB_SUFFIX L".dll"
        #define GPA_X64_ARCH_SUFFIX L"-x64"
        #define GPA_X86_ARCH_SUFFIX L""
    #else
        #define GPA_LIB_PREFIX L"lib"
        #define GPA_LIB_SUFFIX L".so"
        #define GPA_X64_ARCH_SUFFIX L""
        #define GPA_X86_ARCH_SUFFIX L"32"
    #endif

#else
    typedef char LocaleChar;
    typedef std::string LocaleString;
    #define GPA_OPENCL_LIB "GPUPerfAPICL"
    #define GPA_OPENGL_LIB "GPUPerfAPIGL"
    #define GPA_OPENGL_ES_LIB "GPUPerfAPIGLES"
    #define GPA_DIRECTX11_LIB "GPUPerfAPIDX11"
    #define GPA_DIRECTX12_LIB "GPUPerfAPIDX12"
    #define GPA_HSA_LIB "GPUPerfAPIHSA"
    #define GPA_X64_ARCH_SUFFIX "-x64"
    #define GPA_X86_ARCH_SUFFIX "32"

    #ifdef _WIN32
        #define GPA_LIB_PREFIX ""
        #define GPA_LIB_SUFFIX ".dll"
        #define GPA_X64_ARCH_SUFFIX "-x64"
        #define GPA_X86_ARCH_SUFFIX ""
    #else
        #define LIB_PREFIX "lib"
        #define LIB_SUFFIX ".so"
        #define GPA_X64_ARCH_SUFFIX ""
        #define GPA_X86_ARCH_SUFFIX "32"
    #endif
#endif

#define GPA_GET_FUNCTION_TABLE_FUNCTION_NAME "GPA_GetFuncTable"

inline LocaleString GetWorkingDirectoryPath()
{
    LocaleChar selfModuleName[GPA_MAX_PATH];
    LocaleString path;

#ifdef _WIN32
    GetModuleFileName(nullptr, selfModuleName, GPA_MAX_PATH);
    path = LocaleString(selfModuleName);
    std::replace(path.begin(), path.end(), '\\', '/');
#else
    int len;
    len = readlink("/proc/self/exe", selfModuleName, GPA_MAX_PATH - 1);

    if (len != -1)
    {
        selfModuleName[len] = '\0';
    }

    path = LocaleString(selfModuleName);
#endif

    size_t lastSlashPos = path.find_last_of('/');

    if (std::string::npos != lastSlashPos)
    {
        path = path.substr(0, lastSlashPos + 1);
    }

    return path;
}


/// Singleton Class to handle the loading and unloading the possible APIs
class GPAApiManager
{
public:

    /// Returns the instance of the GPAApiManger
    /// \return returns  the instance of the GPAApiManager
    static GPAApiManager* Instance()
    {
        if (nullptr == m_pGpaApiManger)
        {
            m_pGpaApiManger = new(std::nothrow) GPAApiManager();
        }

        return m_pGpaApiManger;
    }

    /// Loads the dll and initialize the function table for the passed api type
    /// \param[in] apiType type of the api to be loaded
    /// \param[opt,in] libPath path to the folder containing dll
    /// \return returns appropriate status of the operation
    GPA_Status LoadApi(const GPA_API_Type& apiType, const LocaleString libPath = LocaleString())
    {
        GPA_Status status = GPA_STATUS_OK;

        if (m_gpaApiFunctionTables.find(apiType) == m_gpaApiFunctionTables.end())
        {
            if (apiType > GPA_API__START && apiType < GPA_API_NO_SUPPORT)
            {
                LocaleString libFullPath;
                libFullPath.append(GPA_LIB_PREFIX);

                switch (apiType)
                {
#ifdef _WIN32

                    case GPA_API_DIRECTX_11:
                        libFullPath.append(GPA_DIRECTX11_LIB);
                        break;

                    case GPA_API_DIRECTX_12:
                        libFullPath.append(GPA_DIRECTX12_LIB);
                        break;
#endif

                    case GPA_API_OPENGL:
                        libFullPath.append(GPA_OPENGL_LIB);
                        break;

                    case GPA_API_OPENGLES:
                        libFullPath.append(GPA_OPENGL_ES_LIB);
                        break;

                    case GPA_API_OPENCL:
                        libFullPath.append(GPA_OPENCL_LIB);
                        break;
#ifdef _LINUX

                    case GPA_API_HSA:
                        libFullPath.append(HSA_LIB);
                        break;
#endif

                    case GPA_API_VULKAN:
                    default:
                        status = GPA_STATUS_ERROR_API_NOT_SUPPORTED;
                }

#ifdef GPA_IS_64_BIT
                libFullPath.append(GPA_X64_ARCH_SUFFIX);
#else
                libFullPath.append(GPA_X86_ARCH_SUFFIX);
#endif

                libFullPath.append(GPA_LIB_SUFFIX);

                if (libPath.empty())
                {
                    libFullPath = GetWorkingDirectoryPath() + libFullPath;
                }
                else
                {
                    libFullPath = libPath + libFullPath;
                }

                LibHandle libHandle;

#ifdef _WIN32
                libHandle = LoadLibrary(libFullPath.c_str());
#else
                libHandle = dlopen(libFullPath.c_str(), RTLD_LAZY);
#endif
                GPA_GetFuncTablePtrType funcTableFn;

                if (nullptr != libHandle)
                {
#ifdef _WIN32
                    funcTableFn = reinterpret_cast<GPA_GetFuncTablePtrType>(GetProcAddress(libHandle, GPA_GET_FUNCTION_TABLE_FUNCTION_NAME));
#else
                    funcTableFn = reinterpret_cast<GPA_GetFuncTablePtrType>(dlsym(libHandle, GPA_GET_FUNCTION_TABLE_FUNCTION_NAME));
#endif

                    if (nullptr != funcTableFn)
                    {
                        GPAApi* pGPAApi = new(std::nothrow) GPAApi();
                        funcTableFn(reinterpret_cast<void**>(&pGPAApi));

                        if (pGPAApi->m_apiId == GPA_API_CURRENT_UUID)
                        {
                            m_gpaApiFunctionTables.insert(std::pair<GPA_API_Type, std::pair<LibHandle, GPAApi*>>(apiType, std::pair<LibHandle, GPAApi*>(libHandle, pGPAApi)));
                        }
                        else
                        {
                            delete pGPAApi;
                            status = GPA_STATUS_ERROR_LIB_LOAD_VERION_MISMATCH;
                        }
                    }
                    else
                    {
                        status = GPA_STATUS_ERROR_LIB_LOAD_FAILED;
                    }
                }
            }
            else
            {
                status = GPA_STATUS_ERROR_API_NOT_SUPPORTED;
            }
        }

        return status;
    }

    /// Unloads the function table for the passed api
    /// \param[in] apiType api type
    void UnloadApi(const GPA_API_Type& apiType)
    {
        GPAApi* pApi = GetApi(apiType);

        if (nullptr != pApi)
        {
            delete pApi;
            m_gpaApiFunctionTables.erase(apiType);
        }
    }

    /// Get the function table for the passed API
    /// \param[in] apiType api type
    /// \return returns pointer to the api function table if loaded otherwise nullpointer
    GPAApi* GetApi(const GPA_API_Type& apiType)
    {
        if (m_gpaApiFunctionTables.find(apiType) != m_gpaApiFunctionTables.end())
        {
            return m_gpaApiFunctionTables[apiType].second;
        }

        return nullptr;
    }

    /// Destructor
    ~GPAApiManager()
    {
        for (std::map<GPA_API_Type, std::pair<LibHandle, GPAApi*>>::iterator it = m_gpaApiFunctionTables.begin(); it != m_gpaApiFunctionTables.end(); ++it)
        {
            delete it->second.second;
        }

        m_gpaApiFunctionTables.clear();
    }

private:
    /// Constructor
    GPAApiManager()
    {}

    static GPAApiManager*                                  m_pGpaApiManger;                 ///< GPA Api Manager pointer
    std::map<GPA_API_Type, std::pair<LibHandle, GPAApi*>>  m_gpaApiFunctionTables;          ///< container to hold the function pointer table for all loaded api
};

// Note: For usage - Add Initialization of the static instance in compiling unit
// GPAApiManager* GPAApiManager::m_pGpaApiManger = nullptr;

#endif
