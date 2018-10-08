//=============================================================================
/// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  An API for the developer mode driver to initialize driver protocols.
///  Can be used by applications to take RGP profiles of themselves
//=============================================================================
#ifndef DEV_DRIVER_API_H_
#define DEV_DRIVER_API_H_

#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  /// The API calling convention on windows.
  #define DEV_DRIVER_API_CALL __cdecl
  #ifdef BUILD_DLL
    #define DEV_DRIVER_API_EXPORT __declspec(dllexport)
  #elif defined(IMPORT_DLL)
    #define DEV_DRIVER_API_EXPORT __declspec(dllimport)
  #else
    #define DEV_DRIVER_API_EXPORT
  #endif // BUILD_DLL
#else
  /// The API calling convention on linux.
  #define DEV_DRIVER_API_CALL
  #define DEV_DRIVER_API_EXPORT
#endif

#define DEV_DRIVER_API_MAJOR_VERSION 1
#define DEV_DRIVER_API_MINOR_VERSION (sizeof(DevDriverAPI))

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

typedef void* DevDriverAPIContext;

// An enum of status codes returned from the API
typedef enum
{
    DEV_DRIVER_STATUS_SUCCESS               = 0,
    DEV_DRIVER_STATUS_ERROR                 = -1,
    DEV_DRIVER_STATUS_FAILED                = -2,
    DEV_DRIVER_STATUS_NULL_POINTER          = -3,
    DEV_DRIVER_STATUS_BAD_ALLOC             = -4,
    DEV_DRIVER_STATUS_CAPTURE_FAILED        = -5,
    DEV_DRIVER_STATUS_NOT_CAPTURED          = -6,
    DEV_DRIVER_STATUS_INVALID_MAJOR_VERSION = -7,
    DEV_DRIVER_STATUS_INVALID_PARAMETERS    = -8,
} DevDriverStatus;

// An enum of options to pass into the DevDriverAPI
typedef enum
{
    DEV_DRIVER_FEATURE_ENABLE_RGP           = 1,
} DevDriverFeature;

// structure containing any features relating to RGP
typedef struct DevDriverFeatureRGP
{
    // presently nothing needed for RGP initialization
    uint32_t                    reserved;       ///< ensure a specific size struct for C/C++
} DevDriverFeatureRGP;

// structure containing a developer driver feature. Specific
// feature structures can be added to the union as needed
typedef struct DevDriverFeatures
{
    DevDriverFeature            m_option;
    uint32_t                    m_size;
    union
    {
        DevDriverFeatureRGP     m_featureRGP;
    };
} DevDriverFeatures;

// structure containing any options required for taking an RGP profile
typedef struct RGPProfileOptions
{
    const char* m_pProfileFilePath;             ///< The file (and path) used to save the captured profile to. If the
                                                ///< path is omitted, the file will be saved to the default folder.
                                                ///< If nullptr is specified, then a filename will be generated based
                                                ///< on the process name and a date/time timestamp

    // frame terminator values
    uint64_t    m_beginFrameTerminatorTag;      ///< frame terminator begin tag (vulkan). Should be non-zero if being used
    uint64_t    m_endFrameTerminatorTag;        ///< frame terminator end tag (vulkan). Should be non-zero if being used
    const char* m_pBeginFrameTerminatorString;  ///< frame terminator begin string (D3D12). Should be non-null/non-empty if being used
    const char* m_pEndFrameTerminatorString;    ///< frame terminator end string (D3D12). Should be non-null/non-empty if being used
} RGPProfileOptions;

// function typedefs

/// Initialization function. To be called before initializing the device.
/// \param featureList An array of DevDriverFeatures structures containing
///  a list of features to be enabled
/// \param featureCount The number of features in the list
/// \param pOutHandle A returned handle to the DevDriverAPI context.
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If this function fails, pOutHandle will be unchanged.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnInit)(const DevDriverFeatures featureList[], uint32_t featureCount, DevDriverAPIContext* pOutHandle);

/// Cleanup function. To be called at application shutdown.
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnFinish)(DevDriverAPIContext context);

/// Start triggering a profile. The actual profiling is done in a separate thread.
/// The calling function will need to call IsRGPProfileCaptured() to determine
/// if the profile has finished.
/// \param context The DevDriverAPI context
/// \param profileOptions A structure of type RGPProfileOptions containing the
///  profile options
/// \return DEV_DRIVER_STATUS_SUCCESS if a capture was successfully started, or
///  a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnTriggerRGPProfile)(DevDriverAPIContext context, const RGPProfileOptions* const profileOptions);

/// Has an RGP profile been taken?
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnIsRGPProfileCaptured)(DevDriverAPIContext context);

/// Get the name of the last captured RGP profile. Call this after taking a
/// profile.
/// \param context The DevDriverAPI context
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnGetRGPProfileName)(DevDriverAPIContext context, const char** ppOutProfileName);

/// Get the video driver version number.
/// indirectly returns the major and minor version numbers in the parameters
/// \param outMajorVersion The major version number returned
/// \param outMinorVersion The minor version number returned
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If an error is returned, the version nunbers passed in are
///  unmodified.
/// \warning This function is deprecated
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnGetDriverVersion)(DevDriverAPIContext context, unsigned int& outMajorVersion, unsigned int& outMinorVersion);

/// Get the video driver version number, including the subminor version.
/// indirectly returns the major, minor and subminor version numbers in the parameters
/// \param pMajorVersion The major version number returned
/// \param pMinorVersion The minor version number returned
/// \param pSubminorVersion The minor version number returned
/// \return DEV_DRIVER_STATUS_SUCCESS if successful, or a DevDriverStatus error
///  code if not. If an error is returned, the version nunbers passed in are
///  unmodified.
typedef DevDriverStatus(DEV_DRIVER_API_CALL*
    DevDriverFnGetFullDriverVersion)(DevDriverAPIContext context, unsigned int* pMajorVersion, unsigned int* pMinorVersion, unsigned int* ptSubminorVersion);

// structure containing the list of functions supported by this version of the API.
// Also contains major and minor version numbers
typedef struct DevDriverAPI
{
    uint32_t                        majorVersion;
    uint32_t                        minorVersion;

    DevDriverFnInit                 DevDriverInit;
    DevDriverFnFinish               DevDriverFinish;

    DevDriverFnTriggerRGPProfile    TriggerRgpProfile;
    DevDriverFnIsRGPProfileCaptured IsRgpProfileCaptured;
    DevDriverFnGetRGPProfileName    GetRgpProfileName;
    DevDriverFnGetDriverVersion     GetDriverVersion;
    DevDriverFnGetFullDriverVersion GetFullDriverVersion;
} DevDriverAPI;

/// Get the function table
DEV_DRIVER_API_EXPORT DevDriverStatus DEV_DRIVER_API_CALL DevDriverGetFuncTable(void* pApiTableOut);

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#endif // DEV_DRIVER_API_H_
