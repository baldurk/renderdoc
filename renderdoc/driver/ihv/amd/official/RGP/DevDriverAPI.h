//=============================================================================
// Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  An API for the developer mode driver to initialize driver protocols.
///
/// Can be used by applications to write RGP profiles/RMV traces of themselves.
//=============================================================================

#ifndef RDP_SOURCE_API_DEV_DRIVER_API_H_
#define RDP_SOURCE_API_DEV_DRIVER_API_H_

#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
// The API calling convention on Windows.
#define DEV_DRIVER_API_CALL __cdecl
#ifdef BUILD_DLL
#define DEV_DRIVER_API_EXPORT __declspec(dllexport)
#elif defined(IMPORT_DLL)
#define DEV_DRIVER_API_EXPORT __declspec(dllimport)
#else
#define DEV_DRIVER_API_EXPORT
#endif  // BUILD_DLL
#else
// The API calling convention on Linux.
#define DEV_DRIVER_API_CALL
#define DEV_DRIVER_API_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif  // #ifdef __cplusplus

/// @brief Handle to a dev driver context.
typedef void* DevDriverAPIContext;

/// @brief An enum of status codes returned from the API.
typedef enum
{
    kDevDriverStatusSuccess             = 0,
    kDevDriverStatusError               = -1,
    kDevDriverStatusFailed              = -2,
    kDevDriverStatusNullPointer         = -3,
    kDevDriverStatusBadAlloc            = -4,
    kDevDriverStatusCaptureFailed       = -5,
    kDevDriverStatusNotCaptured         = -6,
    kDevDriverStatusInvalidMajorVersion = -7,
    kDevDriverStatusInvalidParameters   = -8,
    kDevDriverStatusAlreadyCaptured     = -9,
    kDevDriverStatusCaptureInProgress   = -10,
    kDevDriverStatusNotAvailable        = -11,
    kDevDriverStatusParsingFailure      = -12,
} DevDriverStatus;

/// @brief An enum of options to pass into the DevDriverAPI.
typedef enum
{
    kDevDriverFeatureEnableRgp = 1,
    kDevDriverFeatureEnableRmv = 2,
    kDevDriverFeatureEnableRra = 3
} DevDriverFeature;

/// @brief Structure containing any features relating to RGP.
typedef struct DevDriverFeatureRGP
{
    union
    {
        struct
        {
            uint32_t disable_etw : 1;               ///< Disable Etw protocol when creating listener.
            uint32_t enable_instruction_trace : 1;  ///< Enable instruction tracing.
            uint32_t reserved : 30;                 ///< Reserved for future use.
        } flags;                                    ///< The configuration flags.

        uint32_t u32_all;  ///< Representation of all of the flags.
    };
    union
    {
        struct
        {
            uint32_t start;  ///< OpenCL/HIP dispatch range start index.
            uint32_t end;    ///< OpenCL/HIP dispatch range end index.
        } dispatch_indices;  ///< OpenCL/HIP dispatch range indices.

        uint32_t frame_number;  ///< DX/VK frame number to capture.
    };

    uint32_t shader_engine_mask;  ///< The SE mask used when collecting instruction trace data.

} DevDriverFeatureRGP;

/// @brief The API PSO version for RGP features.
#define s_FEATURE_RGP_API_PSO_VERSION 24

/// @brief Structure containing any features relating to RMV.
typedef struct DevDriverFeatureRMV
{
    // Presently nothing needed for RMV initialization.
    uint32_t reserved;  ///< Ensure a specific size struct for C/C++.
} DevDriverFeatureRMV;

/// @brief Structure containing any features relating to RRA capture.
typedef struct DevDriverFeatureRRA
{
    uint32_t reserved;
} DevDriverFeatureRRA;

/// @brief Structure containing enabled developer driver feature options.
///
/// Specific feature structures can be added to the union as needed.
typedef struct DevDriverFeatures
{
    DevDriverFeature option;  ///< Which feature this describes.

    /// @brief The size of the actual data contained in this struct.
    ///
    /// If option is kDevDriverFeatureEnableRgp then this should be sizeof(DevDriverFeatureRGP),
    /// otherwise it should be sizeof(DevDriverFeatureRMV) since this will be a description of
    /// an RMV feature.
    uint32_t size;

    union
    {
        DevDriverFeatureRGP feature_rgp;  ///< The data describing the RGP features.
        DevDriverFeatureRMV feature_rmv;  ///< The data describing the RMV features.
        DevDriverFeatureRRA feature_rra;  ///< The data describing the RRA features.
    };
} DevDriverFeatures;

/// @brief Structure containing any options required for taking an RGP profile.
typedef struct RGPProfileOptions
{
    /// @brief The file (and path) used to save the captured profile to.
    ///
    /// If the path is omitted, the file will be saved to the default folder.
    /// If nullptr is specified, then a filename will be generated based on the process
    /// name and a date/time timestamp.
    const char* profile_file_path;

    // Frame terminator values.

    /// @brief Frame terminator begin tag (Vulkan).
    ///
    /// Should be non-zero if being used.
    uint64_t begin_frame_terminator_tag;

    /// @brief Frame terminator end tag (Vulkan).
    ///
    /// Should be non-zero if being used.
    uint64_t end_frame_terminator_tag;

    /// @brief Frame terminator begin string (D3D12).
    ///
    /// Should be non-null/non-empty if being used.
    const char* begin_frame_terminator_string;

    /// @brief Frame terminator end string (D3D12).
    ///
    /// Should be non-null/non-empty if being used.
    const char* end_frame_terminator_string;
} RGPProfileOptions;

/// @brief Structure containing any options required for taking an RMV trace.
typedef struct RMVTraceOptions
{
    /// @brief The file (and path) used to save the captured trace to.
    ///
    /// If the path is omitted, the file will be saved to the default folder.
    /// If nullptr is specified, then a filename will be generated based on the process
    /// name and a date/time timestamp.
    const char* trace_file_path;
} RMVTraceOptions;

typedef struct RRACaptureOptions
{
    /// @brief The file (and path) used to save the captured scene to.
    ///
    /// If this path component is omitted, then file will be saved to default working directory.
    /// If nullptr is specified, then a filename will be generated based on the process name
    /// and a date timestamp.
    const char* capture_file_path;
} RRACaptureOptions;

// function typedefs

/// @brief Initialization function.
///
/// To be called before initializing the device.
///
/// @param feature_list An array of DevDriverFeatures structures containing
///                     a list of features to be enabled.
/// @param feature_count The number of features in the list.
/// @param out_handle A returned handle to the DevDriverAPI context.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error
///         code if not. If this function fails, out_handle will be unchanged.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnInit)(const DevDriverFeatures feature_list[], uint32_t feature_count, DevDriverAPIContext* out_handle);

/// @brief Cleanup function to be called at application shutdown.
/// @param context The DevDriverAPI context.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnFinish)(DevDriverAPIContext context);

/// @brief Start triggering a profile.
///
/// The actual profiling is done in a separate thread.
/// The calling function will need to call IsRGPProfileCaptured() to determine
/// if the profile has finished.
///
/// @param context The DevDriverAPI context.
/// @param profile_options A structure of type RGPProfileOptions containing the
///                        profile options.
/// @return kDevDriverStatusSuccess if a capture was successfully started, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnTriggerRGPProfile)(DevDriverAPIContext context, const RGPProfileOptions* const profile_options);

/// @brief Has an RGP profile been taken?
/// @param context The DevDriverAPI context.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnIsRGPProfileCaptured)(DevDriverAPIContext context);

/// @brief Get the name of the last captured RGP profile.
///
/// Call this after taking a profile.
///
/// @param context [in] The DevDriverAPI context.
/// @param out_profile_name [out] The name of the last captured RGP profile.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnGetRGPProfileName)(DevDriverAPIContext context, const char** out_profile_name);

/// @brief Get the video driver version number, including the subminor version.
///
/// Indirectly returns the major, minor and subminor version numbers in the parameters.
///
/// @param major_version The major version number returned.
/// @param minor_version The minor version number returned.
/// @param subminor_version The minor version number returned.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error
///         code if not. If an error is returned, the version numbers passed in are
///         unmodified.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnGetFullDriverVersion)(DevDriverAPIContext context,
                                                                              unsigned int*       major_version,
                                                                              unsigned int*       minor_version,
                                                                              unsigned int*       subminor_version);

/// @brief Insert a snapshot string into the RMV event stream.
/// @param context The DevDriverAPI context.
/// @param snapshot_name A null-terminated string to be inserted into the RMV event stream.
/// @return kDevDriverStatusSuccess if the snapshot string was inserted successfully,
///         or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnInsertRMVSnapshot)(DevDriverAPIContext context, const char* snapshot_name);

/// @brief Trigger collection of an RMV trace.
///
/// The file writing is done in a separate thread.
/// The calling function will need to call IsRMVTraceCaptured() to determine
/// if the trace has finished.
///
/// @param context The DevDriverAPI context.
/// @param trace_options A structure of type RMVTraceOptions containing the trace options.
/// @return kDevDriverStatusSuccess if a capture was successfully started, or
///         a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnTriggerRMVTrace)(DevDriverAPIContext context, const RMVTraceOptions* const trace_options);

/// @brief Has an RMV trace been taken?
/// @param context The DevDriverAPI context.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnIsRMVTraceCaptured)(DevDriverAPIContext context);

/// @brief Get the name of the last captured RMV trace.
///
/// Call this after taking a trace.
///
/// @param [in] context The DevDriverAPI context.
/// @param [out] out_trace_name The name of the last captured RMV trace.
/// @return kDevDriverStatusSuccess if successful, or a DevDriverStatus error code if not.
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnGetRMVTraceName)(DevDriverAPIContext context, const char** out_trace_name);

/// @brief Get RRA capture file name
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnGetRRACaptureName)(DevDriverAPIContext context, const char** out_capture_name);

/// @brief Request an RRA capture
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnRequestRRACapture)(DevDriverAPIContext context);

/// @brief Collect an RRA capture
typedef DevDriverStatus(DEV_DRIVER_API_CALL* DevDriverFnCollectRRACapture)(DevDriverAPIContext context, const RRACaptureOptions* const capture_options);

/// @brief Structure containing the list of functions supported by this version of the API.
///
/// Also contains major and minor version numbers.
typedef struct DevDriverAPI
{
    uint32_t major_version;  ///< The major version of the API.
    uint32_t minor_version;  ///< The minor version of the API.

    DevDriverFnInit   devdriver_init;    ///< Called before initializing the device.
    DevDriverFnFinish devdriver_finish;  ///< Cleanup function to be called at application shutdown.

    DevDriverFnTriggerRGPProfile    trigger_rgp_profile;      ///< Trigger a new RGP profile.
    DevDriverFnIsRGPProfileCaptured is_rgp_profile_captured;  ///< Returns whether or not an RGP profile has been captured.
    DevDriverFnGetRGPProfileName    get_rgp_profile_name;     ///< Provides the name of the last RGP profile captured.
    void* reserved_entry_point;  ///< Removed entrypoint -- this is a placeholder to maintain backwards compatibility in the api table.
    DevDriverFnGetFullDriverVersion get_full_driver_version;  ///< Provides the video driver version.

    DevDriverFnInsertRMVSnapshot  insert_rmv_snapshot;    ///< Insert a snapshot string into the RMV event stream.
    DevDriverFnTriggerRMVTrace    trigger_rmv_trace;      ///< Triggers an RMV trace.
    DevDriverFnIsRMVTraceCaptured is_rmv_trace_captured;  ///< Returns whether or not an RMV trace has been captured.
    DevDriverFnGetRMVTraceName    get_rmv_trace_name;     ///< Provides the name of the last RMV trace captured.

    DevDriverFnGetRRACaptureName get_rra_capture_name;  ///< Provides the name of the last RRA capture.
    DevDriverFnRequestRRACapture request_rra_capture;   ///< Requests RRA capture should be started.
    DevDriverFnCollectRRACapture collect_rra_capture;   ///< Collects the RRA capture data and writes to disk.
} DevDriverAPI;

// Version constants
///@brief The major version of the API.
static const uint32_t kDevDriverApiMajorVersion = 2;

/// @brief The minor version of the API.
static const uint32_t kDevDriverApiMinorVersion = (sizeof(DevDriverAPI));

/// @brief Get the function table.
/// @param [out] api_table_out Where to write the API table to.
DEV_DRIVER_API_EXPORT DevDriverStatus DEV_DRIVER_API_CALL DevDriverGetFuncTable(void* api_table_out);

#ifdef __cplusplus
}
#endif  // #ifdef __cplusplus

#endif
