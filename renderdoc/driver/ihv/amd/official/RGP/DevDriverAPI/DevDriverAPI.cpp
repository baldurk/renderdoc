//=============================================================================
/// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  An API for the developer mode driver to initialize driver protocols.
///  Can be used by applications to take RGP profiles of themselves
//=============================================================================

#ifdef _WIN32
#include "ADLGetDriverVersion.h"
#else
#include <iostream>
#include <fstream>
#include <string>
#endif

#include <algorithm>

#include "DevDriverAPI.h"
#include "RGPClientInProcessModel.h"

// C wrapper functions

//-----------------------------------------------------------------------------
/// Initialization function. To be called before initializing the device.
/// \param featureList An array of DevDriverFeatures structures containing
///  a list of features to be enabled
/// \param featureCount The number of features in the list
/// \param pOutHandle A returned handle to the DevDriverAPI context.
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If this function fails, pOutHandle will be unchanged.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
Init(const DevDriverFeatures featureList[], uint32_t featureCount, DevDriverAPIContext* pOutHandle)
{
    bool rgpEnabled = false;

    for (uint32_t loop = 0; loop < featureCount; loop++)
    {
        DevDriverFeature feature = featureList[loop].m_option;
        switch (feature)
        {
        case DEV_DRIVER_FEATURE_ENABLE_RGP:
            rgpEnabled = true;
            break;

        default:
            break;
        }
    }

    RGPClientInProcessModel* pHandle = new(std::nothrow) RGPClientInProcessModel();

    if (pHandle != nullptr)
    {
        if (pHandle->Init(rgpEnabled) == true)
        {
            *pOutHandle = reinterpret_cast<DevDriverAPIContext>(pHandle);
            return DEV_DRIVER_STATUS_SUCCESS;
        }
        else
        {
            delete pHandle;
            return DEV_DRIVER_STATUS_FAILED;
        }
    }
    else
    {
        return DEV_DRIVER_STATUS_BAD_ALLOC;
    }
}

//-----------------------------------------------------------------------------
/// Cleanup function. To be called at application shutdown.
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
Finish(DevDriverAPIContext handle)
{
    if (handle != nullptr)
    {
        RGPClientInProcessModel* pObj = reinterpret_cast<RGPClientInProcessModel*>(handle);
        pObj->Finish();
        delete pObj;
        return DEV_DRIVER_STATUS_SUCCESS;
    }
    else
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }
}

//-----------------------------------------------------------------------------
/// Start triggering a profile. The actual profiling is done in a separate thread.
/// The calling function will need to call IsRGPProfileCaptured() to determine
/// if the profile has finished.
/// \param context The DevDriverAPI context
/// \param profileOptions A structure of type RGPProfileOptions containing the
///  profile options
/// \return DEV_DRIVER_STATUS_SUCCESS if a capture was successfully started, or
///  a DevDriverStatus error code if not.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
TriggerCapture(DevDriverAPIContext handle, const RGPProfileOptions* const profileOptions)
{
    if (handle != nullptr)
    {
        RGPClientInProcessModel* pObj = reinterpret_cast<RGPClientInProcessModel*>(handle);
        bool requestingFrameTerminators = pObj->SetTriggerMarkerParams(profileOptions->m_beginFrameTerminatorTag, profileOptions->m_endFrameTerminatorTag,
                                     profileOptions->m_pBeginFrameTerminatorString, profileOptions->m_pEndFrameTerminatorString);

        bool captureAllowed = pObj->IsCaptureAllowed(requestingFrameTerminators);
        if (captureAllowed == false)
        {
            return DEV_DRIVER_STATUS_INVALID_PARAMETERS;
        }

        bool triggered = pObj->TriggerCapture(profileOptions->m_pProfileFilePath);
        if (triggered == true)
        {
            return DEV_DRIVER_STATUS_SUCCESS;
        }
        else
        {
            return DEV_DRIVER_STATUS_CAPTURE_FAILED;
        }
    }
    else
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }
}

//-----------------------------------------------------------------------------
/// Has an RGP profile been taken?
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
IsProfileCaptured(DevDriverAPIContext handle)
{
    if (handle != nullptr)
    {
        bool captured = reinterpret_cast<RGPClientInProcessModel*>(handle)->IsProfileCaptured();
        if (captured == true)
        {
            return DEV_DRIVER_STATUS_SUCCESS;
        }
        else
        {
            return DEV_DRIVER_STATUS_NOT_CAPTURED;
        }
    }
    else
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }
}

//-----------------------------------------------------------------------------
/// Get the name of the last captured RGP profile. Call this after taking a
/// profile.
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
GetProfileName(DevDriverAPIContext handle, const char** ppOutProfileName)
{
    if (handle != nullptr)
    {
        const char* pProfileName = reinterpret_cast<RGPClientInProcessModel*>(handle)->GetProfileName();
        *ppOutProfileName = pProfileName;
        return DEV_DRIVER_STATUS_SUCCESS;
    }
    else
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }
}

#ifndef _WIN32
//-----------------------------------------------------------------------------
/// Convert a string to an int. Continue parsing the string until an non-digit
/// is found.
/// \param pString Reference to a pointer to a string to parse. The new value
///  of the pointer is modified in the calling function
/// \return The integer value found in the string. If no integer value is found
///  0 is returned.
//-----------------------------------------------------------------------------
static unsigned int ToInt(const char* &pString)
{
    unsigned int value = 0;
    while (*pString >= '0' && *pString <= '9')
    {
        value *= 10;
        value += *pString++ - '0';
    }
    return value;
}
#endif

//-----------------------------------------------------------------------------
/// Get the video driver version number, including the subminor version.
/// indirectly returns the major, minor and subminor version numbers in the parameters
/// \param pMajorVersion The major version number returned
/// \param pMinorVersion The minor version number returned
/// \param pSubminorVersion The minor version number returned
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If an error is returned, the version nunbers passed in are
///  unmodified.
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
GetFullDriverVersion(DevDriverAPIContext handle, unsigned int* pMajorVersion, unsigned int* pMinorVersion, unsigned int* pSubminorVersion)
{
    if (handle == nullptr)
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }

    bool result = false;
    unsigned int majorVersion = 0;
    unsigned int minorVersion = 0;
    unsigned int subminorVersion = 0;
#ifdef _WIN32
    result = ADLGetDriverVersion(majorVersion, minorVersion, subminorVersion);
#else
    const int sysret = system("modinfo amdgpu | grep version > version.txt");
    if (sysret != 0)
    {
        return DEV_DRIVER_STATUS_ERROR;
    }

    std::string lineString;
    std::ifstream inFile;
    const char* versionFile = "version.txt";
    inFile.open(versionFile);
    if (inFile.is_open())
    {
        do
        {
            getline(inFile, lineString);
            size_t pos = 0;
            const char* searchString = "version:";
            const size_t searchStringLength = strlen(searchString);

            // look for all instances of the search string and only consider ones which are longer than
            // the search string. These strings should have a version number on the end. The complete version
            // string should look something like:
            // version:        18.10.0.1
            pos = lineString.find(searchString, pos);
            if (pos == 0)
            {
                const char* pVersion = lineString.c_str();
                pVersion += searchStringLength;

                // skip spaces
                while (*pVersion == ' ')
                {
                    pVersion++;
                }

                // get the major version number
                majorVersion = ToInt(pVersion);

                DD_ASSERT(pVersion[0] == '.');

                // skip the delimiter
                pVersion++;

                // get the minor version
                minorVersion = ToInt(pVersion);

                if (pVersion[0] == '.')
                {
                    // skip the delimiter
                    pVersion++;

                    // get the minor version
                    subminorVersion = ToInt(pVersion);
                }

                result = true;
            }
        }
        while (lineString.length() > 0 && result == false);
        inFile.close();
    }

    // remove the file
    remove(versionFile);

#endif

    if (result == true)
    {
        *pMajorVersion = majorVersion;
        *pMinorVersion = minorVersion;
        *pSubminorVersion = subminorVersion;
        return DEV_DRIVER_STATUS_SUCCESS;
    }
    else
    {
        return DEV_DRIVER_STATUS_ERROR;
    }
}

//-----------------------------------------------------------------------------
/// Get the video driver version number.
/// indirectly returns the major and minor version numbers in the parameters
/// \param outMajorVersion The major version number returned
/// \param outMinorVersion The minor version number returned
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If an error is returned, the version nunbers passed in are
///  unmodified.
/// \warning This function is deprecated
//-----------------------------------------------------------------------------
static DevDriverStatus DEV_DRIVER_API_CALL
GetDriverVersion(DevDriverAPIContext handle, unsigned int& outMajorVersion, unsigned int& outMinorVersion)
{
    unsigned int subminorVersion = 0;
    return GetFullDriverVersion(handle, &outMajorVersion, &outMinorVersion, &subminorVersion);
}

//-----------------------------------------------------------------------------
/// Get the function table
/// \param pAprTableOut Pointer to an array to receive the list of function
///  pointers. Should be initialized to be at least the size of a DevDriverAPI
///  structure.
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
//-----------------------------------------------------------------------------
DEV_DRIVER_API_EXPORT DevDriverStatus DEV_DRIVER_API_CALL
DevDriverGetFuncTable(void* pApiTableOut)
{
    if (pApiTableOut == nullptr)
    {
        return DEV_DRIVER_STATUS_NULL_POINTER;
    }

    DevDriverAPI* pApiTable = reinterpret_cast<DevDriverAPI*>(pApiTableOut);

    if (pApiTable->majorVersion != DEV_DRIVER_API_MAJOR_VERSION)
    {
        // only support the exact major version match for now
        return DEV_DRIVER_STATUS_INVALID_MAJOR_VERSION;
    }

    uint32_t suppliedMinorVersion = pApiTable->minorVersion;

    // build the dispatch table containing all supported functions in this library
    DevDriverAPI   apiTable;

    apiTable.majorVersion = DEV_DRIVER_API_MAJOR_VERSION;
    apiTable.minorVersion = std::min<uint32_t>(suppliedMinorVersion, DEV_DRIVER_API_MINOR_VERSION);

    apiTable.DevDriverInit         = Init;
    apiTable.DevDriverFinish       = Finish;

    apiTable.TriggerRgpProfile     = TriggerCapture;
    apiTable.IsRgpProfileCaptured  = IsProfileCaptured;
    apiTable.GetRgpProfileName     = GetProfileName;

    apiTable.GetDriverVersion      = GetDriverVersion;
    apiTable.GetFullDriverVersion  = GetFullDriverVersion;

    // only copy the functions supported by the incoming requested library
    memcpy(pApiTable, &apiTable, apiTable.minorVersion);
    return DEV_DRIVER_STATUS_SUCCESS;
}
