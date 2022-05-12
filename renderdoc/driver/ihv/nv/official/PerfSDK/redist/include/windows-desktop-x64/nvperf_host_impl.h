/*
 * Copyright 2014-2022  NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.
 *
 * This software and the information contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and conditions
 * of a form of NVIDIA software license agreement.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

#include <stdlib.h>
#include <string.h>

#if _WIN32
#include <wchar.h>
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include <nvperf_common.h>
#include <nvperf_host.h>
#include <nvperf_target.h>
#include <nvperf_d3d12_host.h>
#include <nvperf_d3d12_target.h>
#include <nvperf_d3d11_host.h>
#include <nvperf_d3d11_target.h>
#include <nvperf_device_host.h>
#include <nvperf_device_target.h>
#include <nvperf_vulkan_host.h>
#include <nvperf_vulkan_target.h>
#include <nvperf_opengl_host.h>
#include <nvperf_opengl_target.h>

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 8)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef NVPA_GenericFn (*NVPA_GetProcAddress_Fn)(const char* pFunctionName);
typedef NVPA_Status (*NVPW_SetLibraryLoadPaths_Fn)(NVPW_SetLibraryLoadPaths_Params* pParams);
typedef NVPA_Status (*NVPW_SetLibraryLoadPathsW_Fn)(NVPW_SetLibraryLoadPathsW_Params* pParams);
typedef NVPA_Status (*NVPW_InitializeHost_Fn)(NVPW_InitializeHost_Params* pParams);
typedef NVPA_Status (*NVPW_CounterData_CalculateCounterDataImageCopySize_Fn)(NVPW_CounterData_CalculateCounterDataImageCopySize_Params* pParams);
typedef NVPA_Status (*NVPW_CounterData_InitializeCounterDataImageCopy_Fn)(NVPW_CounterData_InitializeCounterDataImageCopy_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_Create_Fn)(NVPW_CounterDataCombiner_Create_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_Destroy_Fn)(NVPW_CounterDataCombiner_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_CreateRange_Fn)(NVPW_CounterDataCombiner_CreateRange_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_CopyIntoRange_Fn)(NVPW_CounterDataCombiner_CopyIntoRange_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_AccumulateIntoRange_Fn)(NVPW_CounterDataCombiner_AccumulateIntoRange_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_SumIntoRange_Fn)(NVPW_CounterDataCombiner_SumIntoRange_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataCombiner_WeightedSumIntoRange_Fn)(NVPW_CounterDataCombiner_WeightedSumIntoRange_Params* pParams);
typedef NVPA_Status (*NVPW_GetSupportedChipNames_Fn)(NVPW_GetSupportedChipNames_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_Destroy_Fn)(NVPW_RawMetricsConfig_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_SetCounterAvailability_Fn)(NVPW_RawMetricsConfig_SetCounterAvailability_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_BeginPassGroup_Fn)(NVPW_RawMetricsConfig_BeginPassGroup_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_EndPassGroup_Fn)(NVPW_RawMetricsConfig_EndPassGroup_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_GetNumMetrics_Fn)(NVPW_RawMetricsConfig_GetNumMetrics_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_GetMetricProperties_V2_Fn)(NVPW_RawMetricsConfig_GetMetricProperties_V2_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_AddMetrics_Fn)(NVPW_RawMetricsConfig_AddMetrics_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_IsAddMetricsPossible_Fn)(NVPW_RawMetricsConfig_IsAddMetricsPossible_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_GenerateConfigImage_Fn)(NVPW_RawMetricsConfig_GenerateConfigImage_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_GetConfigImage_Fn)(NVPW_RawMetricsConfig_GetConfigImage_Params* pParams);
typedef NVPA_Status (*NVPW_RawMetricsConfig_GetNumPasses_V2_Fn)(NVPW_RawMetricsConfig_GetNumPasses_V2_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Fn)(NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Fn)(NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataBuilder_Create_Fn)(NVPW_CounterDataBuilder_Create_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataBuilder_Destroy_Fn)(NVPW_CounterDataBuilder_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataBuilder_AddMetrics_Fn)(NVPW_CounterDataBuilder_AddMetrics_Params* pParams);
typedef NVPA_Status (*NVPW_CounterDataBuilder_GetCounterDataPrefix_Fn)(NVPW_CounterDataBuilder_GetCounterDataPrefix_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_Destroy_Fn)(NVPW_MetricsEvaluator_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetMetricNames_Fn)(NVPW_MetricsEvaluator_GetMetricNames_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Fn)(NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Fn)(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_HwUnitToString_Fn)(NVPW_MetricsEvaluator_HwUnitToString_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetCounterProperties_Fn)(NVPW_MetricsEvaluator_GetCounterProperties_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetRatioMetricProperties_Fn)(NVPW_MetricsEvaluator_GetRatioMetricProperties_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetThroughputMetricProperties_Fn)(NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetSupportedSubmetrics_Fn)(NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetMetricRawDependencies_Fn)(NVPW_MetricsEvaluator_GetMetricRawDependencies_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_DimUnitToString_Fn)(NVPW_MetricsEvaluator_DimUnitToString_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_GetMetricDimUnits_Fn)(NVPW_MetricsEvaluator_GetMetricDimUnits_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_SetUserData_Fn)(NVPW_MetricsEvaluator_SetUserData_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_EvaluateToGpuValues_Fn)(NVPW_MetricsEvaluator_EvaluateToGpuValues_Params* pParams);
typedef NVPA_Status (*NVPW_MetricsEvaluator_SetDeviceAttributes_Fn)(NVPW_MetricsEvaluator_SetDeviceAttributes_Params* pParams);
typedef NVPA_Status (*NVPW_InitializeTarget_Fn)(NVPW_InitializeTarget_Params* pParams);
typedef NVPA_Status (*NVPW_GetDeviceCount_Fn)(NVPW_GetDeviceCount_Params* pParams);
typedef NVPA_Status (*NVPW_Device_GetNames_Fn)(NVPW_Device_GetNames_Params* pParams);
typedef NVPA_Status (*NVPW_Device_GetPciBusIds_Fn)(NVPW_Device_GetPciBusIds_Params* pParams);
typedef NVPA_Status (*NVPW_Device_GetMigAttributes_Fn)(NVPW_Device_GetMigAttributes_Params* pParams);
typedef NVPA_Status (*NVPW_Adapter_GetDeviceIndex_Fn)(NVPW_Adapter_GetDeviceIndex_Params* pParams);
typedef NVPA_Status (*NVPW_CounterData_GetNumRanges_Fn)(NVPW_CounterData_GetNumRanges_Params* pParams);
typedef NVPA_Status (*NVPW_CounterData_GetChipName_Fn)(NVPW_CounterData_GetChipName_Params* pParams);
typedef NVPA_Status (*NVPW_Config_GetNumPasses_V2_Fn)(NVPW_Config_GetNumPasses_V2_Params* pParams);
typedef NVPA_Status (*NVPW_QueryVersionNumber_Fn)(NVPW_QueryVersionNumber_Params* pParams);
typedef NVPA_Status (*NVPW_Device_GetClockStatus_Fn)(NVPW_Device_GetClockStatus_Params* pParams);
typedef NVPA_Status (*NVPW_Device_SetClockSetting_Fn)(NVPW_Device_SetClockSetting_Params* pParams);
typedef NVPA_Status (*NVPW_CounterData_GetRangeDescriptions_Fn)(NVPW_CounterData_GetRangeDescriptions_Params* pParams);
typedef NVPA_Status (*NVPW_Profiler_CounterData_GetRangeDescriptions_Fn)(NVPW_Profiler_CounterData_GetRangeDescriptions_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_CounterData_GetSampleTime_Fn)(NVPW_PeriodicSampler_CounterData_GetSampleTime_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_CounterData_TrimInPlace_Fn)(NVPW_PeriodicSampler_CounterData_TrimInPlace_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_CounterData_GetInfo_Fn)(NVPW_PeriodicSampler_CounterData_GetInfo_Params* pParams);
typedef NVPA_Status (*NVPW_PeriodicSampler_CounterData_GetTriggerCount_Fn)(NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_RawMetricsConfig_Create_Fn)(NVPW_D3D12_RawMetricsConfig_Create_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Fn)(NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MetricsEvaluator_Initialize_Fn)(NVPW_D3D12_MetricsEvaluator_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_LoadDriver_Fn)(NVPW_D3D12_LoadDriver_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Device_GetDeviceIndex_Fn)(NVPW_D3D12_Device_GetDeviceIndex_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_GetLUID_Fn)(NVPW_D3D12_GetLUID_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Fn)(NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CounterDataImage_Initialize_Fn)(NVPW_D3D12_Profiler_CounterDataImage_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)(NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)(NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CalcTraceBufferSize_Fn)(NVPW_D3D12_Profiler_CalcTraceBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_BeginSession_Fn)(NVPW_D3D12_Profiler_Queue_BeginSession_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_EndSession_Fn)(NVPW_D3D12_Profiler_Queue_EndSession_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Queue_ServicePendingGpuOperations_Fn)(NVPW_D3D12_Queue_ServicePendingGpuOperations_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_SetConfig_Fn)(NVPW_D3D12_Profiler_Queue_SetConfig_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_ClearConfig_Fn)(NVPW_D3D12_Profiler_Queue_ClearConfig_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_BeginPass_Fn)(NVPW_D3D12_Profiler_Queue_BeginPass_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_EndPass_Fn)(NVPW_D3D12_Profiler_Queue_EndPass_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_PushRange_Fn)(NVPW_D3D12_Profiler_Queue_PushRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_PopRange_Fn)(NVPW_D3D12_Profiler_Queue_PopRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CommandList_PushRange_Fn)(NVPW_D3D12_Profiler_CommandList_PushRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_CommandList_PopRange_Fn)(NVPW_D3D12_Profiler_CommandList_PopRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_DecodeCounters_Fn)(NVPW_D3D12_Profiler_Queue_DecodeCounters_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Fn)(NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_Profiler_IsGpuSupported_Fn)(NVPW_D3D12_Profiler_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_IsGpuSupported_Fn)(NVPW_D3D12_MiniTrace_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_DeviceState_Create_Fn)(NVPW_D3D12_MiniTrace_DeviceState_Create_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_DeviceState_Destroy_Fn)(NVPW_D3D12_MiniTrace_DeviceState_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_Queue_Register_Fn)(NVPW_D3D12_MiniTrace_Queue_Register_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_Queue_Unregister_Fn)(NVPW_D3D12_MiniTrace_Queue_Unregister_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Fn)(NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Fn)(NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Params* pParams);
typedef NVPA_Status (*NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Fn)(NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_RawMetricsConfig_Create_Fn)(NVPW_D3D11_RawMetricsConfig_Create_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Fn)(NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_MetricsEvaluator_Initialize_Fn)(NVPW_D3D11_MetricsEvaluator_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Fn)(NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_CounterDataImage_Initialize_Fn)(NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)(NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)(NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_LoadDriver_Fn)(NVPW_D3D11_LoadDriver_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_GetLUID_Fn)(NVPW_D3D11_GetLUID_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Device_GetDeviceIndex_Fn)(NVPW_D3D11_Device_GetDeviceIndex_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_CalcTraceBufferSize_Fn)(NVPW_D3D11_Profiler_CalcTraceBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_BeginSession_Fn)(NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_EndSession_Fn)(NVPW_D3D11_Profiler_DeviceContext_EndSession_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_SetConfig_Fn)(NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Fn)(NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_BeginPass_Fn)(NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_EndPass_Fn)(NVPW_D3D11_Profiler_DeviceContext_EndPass_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_PushRange_Fn)(NVPW_D3D11_Profiler_DeviceContext_PushRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_PopRange_Fn)(NVPW_D3D11_Profiler_DeviceContext_PopRange_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Fn)(NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_IsGpuSupported_Fn)(NVPW_D3D11_Profiler_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Fn)(NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params* pParams);
typedef NVPA_Status (*NVPW_Device_RawMetricsConfig_Create_Fn)(NVPW_Device_RawMetricsConfig_Create_Params* pParams);
typedef NVPA_Status (*NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Fn)(NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_Device_MetricsEvaluator_Initialize_Fn)(NVPW_Device_MetricsEvaluator_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_IsGpuSupported_Fn)(NVPW_GPU_PeriodicSampler_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Fn)(NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Fn)(NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_BeginSession_Fn)(NVPW_GPU_PeriodicSampler_BeginSession_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_BeginSession_V2_Fn)(NVPW_GPU_PeriodicSampler_BeginSession_V2_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_EndSession_Fn)(NVPW_GPU_PeriodicSampler_EndSession_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_GetCounterAvailability_Fn)(NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_SetConfig_Fn)(NVPW_GPU_PeriodicSampler_SetConfig_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_StartSampling_Fn)(NVPW_GPU_PeriodicSampler_StartSampling_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_StopSampling_Fn)(NVPW_GPU_PeriodicSampler_StopSampling_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_CpuTrigger_Fn)(NVPW_GPU_PeriodicSampler_CpuTrigger_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Fn)(NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Fn)(NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Fn)(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_DecodeCounters_Fn)(NVPW_GPU_PeriodicSampler_DecodeCounters_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Fn)(NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params* pParams);
typedef NVPA_Status (*NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Fn)(NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params* pParams);
typedef NVPA_Status (*NVPW_VK_RawMetricsConfig_Create_Fn)(NVPW_VK_RawMetricsConfig_Create_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Fn)(NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MetricsEvaluator_Initialize_Fn)(NVPW_VK_MetricsEvaluator_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CounterDataImage_CalculateSize_Fn)(NVPW_VK_Profiler_CounterDataImage_CalculateSize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CounterDataImage_Initialize_Fn)(NVPW_VK_Profiler_CounterDataImage_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)(NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)(NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);
typedef NVPA_Status (*NVPW_VK_LoadDriver_Fn)(NVPW_VK_LoadDriver_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Device_GetDeviceIndex_Fn)(NVPW_VK_Device_GetDeviceIndex_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_GetRequiredInstanceExtensions_Fn)(NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_GetRequiredDeviceExtensions_Fn)(NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CalcTraceBufferSize_Fn)(NVPW_VK_Profiler_CalcTraceBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_BeginSession_Fn)(NVPW_VK_Profiler_Queue_BeginSession_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_EndSession_Fn)(NVPW_VK_Profiler_Queue_EndSession_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Queue_ServicePendingGpuOperations_Fn)(NVPW_VK_Queue_ServicePendingGpuOperations_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_SetConfig_Fn)(NVPW_VK_Profiler_Queue_SetConfig_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_ClearConfig_Fn)(NVPW_VK_Profiler_Queue_ClearConfig_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_BeginPass_Fn)(NVPW_VK_Profiler_Queue_BeginPass_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_EndPass_Fn)(NVPW_VK_Profiler_Queue_EndPass_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CommandBuffer_PushRange_Fn)(NVPW_VK_Profiler_CommandBuffer_PushRange_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_CommandBuffer_PopRange_Fn)(NVPW_VK_Profiler_CommandBuffer_PopRange_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_DecodeCounters_Fn)(NVPW_VK_Profiler_Queue_DecodeCounters_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_IsGpuSupported_Fn)(NVPW_VK_Profiler_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_VK_Profiler_Queue_GetCounterAvailability_Fn)(NVPW_VK_Profiler_Queue_GetCounterAvailability_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_IsGpuSupported_Fn)(NVPW_VK_MiniTrace_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_DeviceState_Create_Fn)(NVPW_VK_MiniTrace_DeviceState_Create_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_DeviceState_Destroy_Fn)(NVPW_VK_MiniTrace_DeviceState_Destroy_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_Queue_Register_Fn)(NVPW_VK_MiniTrace_Queue_Register_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_Queue_Unregister_Fn)(NVPW_VK_MiniTrace_Queue_Unregister_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Fn)(NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Fn)(NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Params* pParams);
typedef NVPA_Status (*NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Fn)(NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_RawMetricsConfig_Create_Fn)(NVPW_OpenGL_RawMetricsConfig_Create_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Fn)(NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_MetricsEvaluator_Initialize_Fn)(NVPW_OpenGL_MetricsEvaluator_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_LoadDriver_Fn)(NVPW_OpenGL_LoadDriver_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_GetCurrentGraphicsContext_Fn)(NVPW_OpenGL_GetCurrentGraphicsContext_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Fn)(NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_IsGpuSupported_Fn)(NVPW_OpenGL_Profiler_IsGpuSupported_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Fn)(NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Fn)(NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)(NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)(NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_CalcTraceBufferSize_Fn)(NVPW_OpenGL_Profiler_CalcTraceBufferSize_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Params* pParams);
typedef NVPA_Status (*NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Fn)(NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Params* pParams);

// Default implementations
static NVPA_Status g_defaultStatus = NVPA_STATUS_NOT_LOADED;

static NVPA_GenericFn NVPA_GetProcAddress_Default(const char* pFunctionName)
{
    (void)pFunctionName;
    return NULL;
}
static NVPA_GenericFn NVPA_GetProcAddress_Default(const char* pFunctionName);
static NVPA_Status NVPW_SetLibraryLoadPaths_Default(NVPW_SetLibraryLoadPaths_Params* pParams);
static NVPA_Status NVPW_SetLibraryLoadPathsW_Default(NVPW_SetLibraryLoadPathsW_Params* pParams);
static NVPA_Status NVPW_InitializeHost_Default(NVPW_InitializeHost_Params* pParams);
static NVPA_Status NVPW_CounterData_CalculateCounterDataImageCopySize_Default(NVPW_CounterData_CalculateCounterDataImageCopySize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterData_InitializeCounterDataImageCopy_Default(NVPW_CounterData_InitializeCounterDataImageCopy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_Create_Default(NVPW_CounterDataCombiner_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_Destroy_Default(NVPW_CounterDataCombiner_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_CreateRange_Default(NVPW_CounterDataCombiner_CreateRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_CopyIntoRange_Default(NVPW_CounterDataCombiner_CopyIntoRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_AccumulateIntoRange_Default(NVPW_CounterDataCombiner_AccumulateIntoRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_SumIntoRange_Default(NVPW_CounterDataCombiner_SumIntoRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataCombiner_WeightedSumIntoRange_Default(NVPW_CounterDataCombiner_WeightedSumIntoRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GetSupportedChipNames_Default(NVPW_GetSupportedChipNames_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_Destroy_Default(NVPW_RawMetricsConfig_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_SetCounterAvailability_Default(NVPW_RawMetricsConfig_SetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_BeginPassGroup_Default(NVPW_RawMetricsConfig_BeginPassGroup_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_EndPassGroup_Default(NVPW_RawMetricsConfig_EndPassGroup_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_GetNumMetrics_Default(NVPW_RawMetricsConfig_GetNumMetrics_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_GetMetricProperties_V2_Default(NVPW_RawMetricsConfig_GetMetricProperties_V2_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_AddMetrics_Default(NVPW_RawMetricsConfig_AddMetrics_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_IsAddMetricsPossible_Default(NVPW_RawMetricsConfig_IsAddMetricsPossible_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_GenerateConfigImage_Default(NVPW_RawMetricsConfig_GenerateConfigImage_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_GetConfigImage_Default(NVPW_RawMetricsConfig_GetConfigImage_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_RawMetricsConfig_GetNumPasses_V2_Default(NVPW_RawMetricsConfig_GetNumPasses_V2_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Default(NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Default(NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataBuilder_Create_Default(NVPW_CounterDataBuilder_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataBuilder_Destroy_Default(NVPW_CounterDataBuilder_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataBuilder_AddMetrics_Default(NVPW_CounterDataBuilder_AddMetrics_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterDataBuilder_GetCounterDataPrefix_Default(NVPW_CounterDataBuilder_GetCounterDataPrefix_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_Destroy_Default(NVPW_MetricsEvaluator_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetMetricNames_Default(NVPW_MetricsEvaluator_GetMetricNames_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Default(NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Default(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_HwUnitToString_Default(NVPW_MetricsEvaluator_HwUnitToString_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetCounterProperties_Default(NVPW_MetricsEvaluator_GetCounterProperties_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetRatioMetricProperties_Default(NVPW_MetricsEvaluator_GetRatioMetricProperties_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetThroughputMetricProperties_Default(NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetSupportedSubmetrics_Default(NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetMetricRawDependencies_Default(NVPW_MetricsEvaluator_GetMetricRawDependencies_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_DimUnitToString_Default(NVPW_MetricsEvaluator_DimUnitToString_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_GetMetricDimUnits_Default(NVPW_MetricsEvaluator_GetMetricDimUnits_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_SetUserData_Default(NVPW_MetricsEvaluator_SetUserData_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_EvaluateToGpuValues_Default(NVPW_MetricsEvaluator_EvaluateToGpuValues_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_MetricsEvaluator_SetDeviceAttributes_Default(NVPW_MetricsEvaluator_SetDeviceAttributes_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_InitializeTarget_Default(NVPW_InitializeTarget_Params* pParams);
static NVPA_Status NVPW_GetDeviceCount_Default(NVPW_GetDeviceCount_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_GetNames_Default(NVPW_Device_GetNames_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_GetPciBusIds_Default(NVPW_Device_GetPciBusIds_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_GetMigAttributes_Default(NVPW_Device_GetMigAttributes_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Adapter_GetDeviceIndex_Default(NVPW_Adapter_GetDeviceIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterData_GetNumRanges_Default(NVPW_CounterData_GetNumRanges_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterData_GetChipName_Default(NVPW_CounterData_GetChipName_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Config_GetNumPasses_V2_Default(NVPW_Config_GetNumPasses_V2_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_QueryVersionNumber_Default(NVPW_QueryVersionNumber_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_GetClockStatus_Default(NVPW_Device_GetClockStatus_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_SetClockSetting_Default(NVPW_Device_SetClockSetting_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_CounterData_GetRangeDescriptions_Default(NVPW_CounterData_GetRangeDescriptions_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Profiler_CounterData_GetRangeDescriptions_Default(NVPW_Profiler_CounterData_GetRangeDescriptions_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_CounterData_GetSampleTime_Default(NVPW_PeriodicSampler_CounterData_GetSampleTime_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_CounterData_TrimInPlace_Default(NVPW_PeriodicSampler_CounterData_TrimInPlace_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_CounterData_GetInfo_Default(NVPW_PeriodicSampler_CounterData_GetInfo_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_PeriodicSampler_CounterData_GetTriggerCount_Default(NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_RawMetricsConfig_Create_Default(NVPW_D3D12_RawMetricsConfig_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Default(NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MetricsEvaluator_Initialize_Default(NVPW_D3D12_MetricsEvaluator_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_LoadDriver_Default(NVPW_D3D12_LoadDriver_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Device_GetDeviceIndex_Default(NVPW_D3D12_Device_GetDeviceIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_GetLUID_Default(NVPW_D3D12_GetLUID_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Default(NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_Initialize_Default(NVPW_D3D12_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Default(NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Default(NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CalcTraceBufferSize_Default(NVPW_D3D12_Profiler_CalcTraceBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_BeginSession_Default(NVPW_D3D12_Profiler_Queue_BeginSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_EndSession_Default(NVPW_D3D12_Profiler_Queue_EndSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Queue_ServicePendingGpuOperations_Default(NVPW_D3D12_Queue_ServicePendingGpuOperations_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_SetConfig_Default(NVPW_D3D12_Profiler_Queue_SetConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_ClearConfig_Default(NVPW_D3D12_Profiler_Queue_ClearConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_BeginPass_Default(NVPW_D3D12_Profiler_Queue_BeginPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_EndPass_Default(NVPW_D3D12_Profiler_Queue_EndPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_PushRange_Default(NVPW_D3D12_Profiler_Queue_PushRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_PopRange_Default(NVPW_D3D12_Profiler_Queue_PopRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CommandList_PushRange_Default(NVPW_D3D12_Profiler_CommandList_PushRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_CommandList_PopRange_Default(NVPW_D3D12_Profiler_CommandList_PopRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_DecodeCounters_Default(NVPW_D3D12_Profiler_Queue_DecodeCounters_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Default(NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_Profiler_IsGpuSupported_Default(NVPW_D3D12_Profiler_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_IsGpuSupported_Default(NVPW_D3D12_MiniTrace_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_DeviceState_Create_Default(NVPW_D3D12_MiniTrace_DeviceState_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_DeviceState_Destroy_Default(NVPW_D3D12_MiniTrace_DeviceState_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_Queue_Register_Default(NVPW_D3D12_MiniTrace_Queue_Register_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_Queue_Unregister_Default(NVPW_D3D12_MiniTrace_Queue_Unregister_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Default(NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Default(NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Default(NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_RawMetricsConfig_Create_Default(NVPW_D3D11_RawMetricsConfig_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Default(NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_MetricsEvaluator_Initialize_Default(NVPW_D3D11_MetricsEvaluator_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Default(NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_Initialize_Default(NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Default(NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Default(NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_LoadDriver_Default(NVPW_D3D11_LoadDriver_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_GetLUID_Default(NVPW_D3D11_GetLUID_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Device_GetDeviceIndex_Default(NVPW_D3D11_Device_GetDeviceIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_CalcTraceBufferSize_Default(NVPW_D3D11_Profiler_CalcTraceBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginSession_Default(NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndSession_Default(NVPW_D3D11_Profiler_DeviceContext_EndSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_SetConfig_Default(NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Default(NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginPass_Default(NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndPass_Default(NVPW_D3D11_Profiler_DeviceContext_EndPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PushRange_Default(NVPW_D3D11_Profiler_DeviceContext_PushRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PopRange_Default(NVPW_D3D11_Profiler_DeviceContext_PopRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Default(NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_IsGpuSupported_Default(NVPW_D3D11_Profiler_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Default(NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_RawMetricsConfig_Create_Default(NVPW_Device_RawMetricsConfig_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Default(NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_Device_MetricsEvaluator_Initialize_Default(NVPW_Device_MetricsEvaluator_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_IsGpuSupported_Default(NVPW_GPU_PeriodicSampler_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Default(NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Default(NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession_Default(NVPW_GPU_PeriodicSampler_BeginSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession_V2_Default(NVPW_GPU_PeriodicSampler_BeginSession_V2_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_EndSession_Default(NVPW_GPU_PeriodicSampler_EndSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_GetCounterAvailability_Default(NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_SetConfig_Default(NVPW_GPU_PeriodicSampler_SetConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_StartSampling_Default(NVPW_GPU_PeriodicSampler_StartSampling_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_StopSampling_Default(NVPW_GPU_PeriodicSampler_StopSampling_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_CpuTrigger_Default(NVPW_GPU_PeriodicSampler_CpuTrigger_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Default(NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Default(NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Default(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters_Default(NVPW_GPU_PeriodicSampler_DecodeCounters_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Default(NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Default(NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_RawMetricsConfig_Create_Default(NVPW_VK_RawMetricsConfig_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Default(NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MetricsEvaluator_Initialize_Default(NVPW_VK_MetricsEvaluator_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CounterDataImage_CalculateSize_Default(NVPW_VK_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CounterDataImage_Initialize_Default(NVPW_VK_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Default(NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Default(NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_LoadDriver_Default(NVPW_VK_LoadDriver_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Device_GetDeviceIndex_Default(NVPW_VK_Device_GetDeviceIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_GetRequiredInstanceExtensions_Default(NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_GetRequiredDeviceExtensions_Default(NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CalcTraceBufferSize_Default(NVPW_VK_Profiler_CalcTraceBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_BeginSession_Default(NVPW_VK_Profiler_Queue_BeginSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_EndSession_Default(NVPW_VK_Profiler_Queue_EndSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Queue_ServicePendingGpuOperations_Default(NVPW_VK_Queue_ServicePendingGpuOperations_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_SetConfig_Default(NVPW_VK_Profiler_Queue_SetConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_ClearConfig_Default(NVPW_VK_Profiler_Queue_ClearConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_BeginPass_Default(NVPW_VK_Profiler_Queue_BeginPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_EndPass_Default(NVPW_VK_Profiler_Queue_EndPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CommandBuffer_PushRange_Default(NVPW_VK_Profiler_CommandBuffer_PushRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_CommandBuffer_PopRange_Default(NVPW_VK_Profiler_CommandBuffer_PopRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_DecodeCounters_Default(NVPW_VK_Profiler_Queue_DecodeCounters_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_IsGpuSupported_Default(NVPW_VK_Profiler_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_Profiler_Queue_GetCounterAvailability_Default(NVPW_VK_Profiler_Queue_GetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_IsGpuSupported_Default(NVPW_VK_MiniTrace_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_DeviceState_Create_Default(NVPW_VK_MiniTrace_DeviceState_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_DeviceState_Destroy_Default(NVPW_VK_MiniTrace_DeviceState_Destroy_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_Queue_Register_Default(NVPW_VK_MiniTrace_Queue_Register_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_Queue_Unregister_Default(NVPW_VK_MiniTrace_Queue_Unregister_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Default(NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Default(NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Default(NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_RawMetricsConfig_Create_Default(NVPW_OpenGL_RawMetricsConfig_Create_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Default(NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_MetricsEvaluator_Initialize_Default(NVPW_OpenGL_MetricsEvaluator_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_LoadDriver_Default(NVPW_OpenGL_LoadDriver_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_GetCurrentGraphicsContext_Default(NVPW_OpenGL_GetCurrentGraphicsContext_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Default(NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_IsGpuSupported_Default(NVPW_OpenGL_Profiler_IsGpuSupported_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Default(NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Default(NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Default(NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Default(NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_CalcTraceBufferSize_Default(NVPW_OpenGL_Profiler_CalcTraceBufferSize_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Default(NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Default(NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Default(NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Default(NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Default(NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Default(NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Default(NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Default(NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Default(NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
static NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Default(NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Params* pParams)
{
    (void)pParams;
    return g_defaultStatus;
}
typedef struct NvPerfApi {
    NVPA_GetProcAddress_Fn                                       NVPA_GetProcAddress;
    NVPW_SetLibraryLoadPaths_Fn                                  NVPW_SetLibraryLoadPaths;
    NVPW_SetLibraryLoadPathsW_Fn                                 NVPW_SetLibraryLoadPathsW;
    NVPW_InitializeHost_Fn                                       NVPW_InitializeHost;
    NVPW_CounterData_CalculateCounterDataImageCopySize_Fn        NVPW_CounterData_CalculateCounterDataImageCopySize;
    NVPW_CounterData_InitializeCounterDataImageCopy_Fn           NVPW_CounterData_InitializeCounterDataImageCopy;
    NVPW_CounterDataCombiner_Create_Fn                           NVPW_CounterDataCombiner_Create;
    NVPW_CounterDataCombiner_Destroy_Fn                          NVPW_CounterDataCombiner_Destroy;
    NVPW_CounterDataCombiner_CreateRange_Fn                      NVPW_CounterDataCombiner_CreateRange;
    NVPW_CounterDataCombiner_CopyIntoRange_Fn                    NVPW_CounterDataCombiner_CopyIntoRange;
    NVPW_CounterDataCombiner_AccumulateIntoRange_Fn              NVPW_CounterDataCombiner_AccumulateIntoRange;
    NVPW_CounterDataCombiner_SumIntoRange_Fn                     NVPW_CounterDataCombiner_SumIntoRange;
    NVPW_CounterDataCombiner_WeightedSumIntoRange_Fn             NVPW_CounterDataCombiner_WeightedSumIntoRange;
    NVPW_GetSupportedChipNames_Fn                                NVPW_GetSupportedChipNames;
    NVPW_RawMetricsConfig_Destroy_Fn                             NVPW_RawMetricsConfig_Destroy;
    NVPW_RawMetricsConfig_SetCounterAvailability_Fn              NVPW_RawMetricsConfig_SetCounterAvailability;
    NVPW_RawMetricsConfig_BeginPassGroup_Fn                      NVPW_RawMetricsConfig_BeginPassGroup;
    NVPW_RawMetricsConfig_EndPassGroup_Fn                        NVPW_RawMetricsConfig_EndPassGroup;
    NVPW_RawMetricsConfig_GetNumMetrics_Fn                       NVPW_RawMetricsConfig_GetNumMetrics;
    NVPW_RawMetricsConfig_GetMetricProperties_V2_Fn              NVPW_RawMetricsConfig_GetMetricProperties_V2;
    NVPW_RawMetricsConfig_AddMetrics_Fn                          NVPW_RawMetricsConfig_AddMetrics;
    NVPW_RawMetricsConfig_IsAddMetricsPossible_Fn                NVPW_RawMetricsConfig_IsAddMetricsPossible;
    NVPW_RawMetricsConfig_GenerateConfigImage_Fn                 NVPW_RawMetricsConfig_GenerateConfigImage;
    NVPW_RawMetricsConfig_GetConfigImage_Fn                      NVPW_RawMetricsConfig_GetConfigImage;
    NVPW_RawMetricsConfig_GetNumPasses_V2_Fn                     NVPW_RawMetricsConfig_GetNumPasses_V2;
    NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Fn     NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize;
    NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Fn     NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize;
    NVPW_CounterDataBuilder_Create_Fn                            NVPW_CounterDataBuilder_Create;
    NVPW_CounterDataBuilder_Destroy_Fn                           NVPW_CounterDataBuilder_Destroy;
    NVPW_CounterDataBuilder_AddMetrics_Fn                        NVPW_CounterDataBuilder_AddMetrics;
    NVPW_CounterDataBuilder_GetCounterDataPrefix_Fn              NVPW_CounterDataBuilder_GetCounterDataPrefix;
    NVPW_MetricsEvaluator_Destroy_Fn                             NVPW_MetricsEvaluator_Destroy;
    NVPW_MetricsEvaluator_GetMetricNames_Fn                      NVPW_MetricsEvaluator_GetMetricNames;
    NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Fn               NVPW_MetricsEvaluator_GetMetricTypeAndIndex;
    NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Fn NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest;
    NVPW_MetricsEvaluator_HwUnitToString_Fn                      NVPW_MetricsEvaluator_HwUnitToString;
    NVPW_MetricsEvaluator_GetCounterProperties_Fn                NVPW_MetricsEvaluator_GetCounterProperties;
    NVPW_MetricsEvaluator_GetRatioMetricProperties_Fn            NVPW_MetricsEvaluator_GetRatioMetricProperties;
    NVPW_MetricsEvaluator_GetThroughputMetricProperties_Fn       NVPW_MetricsEvaluator_GetThroughputMetricProperties;
    NVPW_MetricsEvaluator_GetSupportedSubmetrics_Fn              NVPW_MetricsEvaluator_GetSupportedSubmetrics;
    NVPW_MetricsEvaluator_GetMetricRawDependencies_Fn            NVPW_MetricsEvaluator_GetMetricRawDependencies;
    NVPW_MetricsEvaluator_DimUnitToString_Fn                     NVPW_MetricsEvaluator_DimUnitToString;
    NVPW_MetricsEvaluator_GetMetricDimUnits_Fn                   NVPW_MetricsEvaluator_GetMetricDimUnits;
    NVPW_MetricsEvaluator_SetUserData_Fn                         NVPW_MetricsEvaluator_SetUserData;
    NVPW_MetricsEvaluator_EvaluateToGpuValues_Fn                 NVPW_MetricsEvaluator_EvaluateToGpuValues;
    NVPW_MetricsEvaluator_SetDeviceAttributes_Fn                 NVPW_MetricsEvaluator_SetDeviceAttributes;
    NVPW_InitializeTarget_Fn                                     NVPW_InitializeTarget;
    NVPW_GetDeviceCount_Fn                                       NVPW_GetDeviceCount;
    NVPW_Device_GetNames_Fn                                      NVPW_Device_GetNames;
    NVPW_Device_GetPciBusIds_Fn                                  NVPW_Device_GetPciBusIds;
    NVPW_Device_GetMigAttributes_Fn                              NVPW_Device_GetMigAttributes;
    NVPW_Adapter_GetDeviceIndex_Fn                               NVPW_Adapter_GetDeviceIndex;
    NVPW_CounterData_GetNumRanges_Fn                             NVPW_CounterData_GetNumRanges;
    NVPW_CounterData_GetChipName_Fn                              NVPW_CounterData_GetChipName;
    NVPW_Config_GetNumPasses_V2_Fn                               NVPW_Config_GetNumPasses_V2;
    NVPW_QueryVersionNumber_Fn                                   NVPW_QueryVersionNumber;
    NVPW_Device_GetClockStatus_Fn                                NVPW_Device_GetClockStatus;
    NVPW_Device_SetClockSetting_Fn                               NVPW_Device_SetClockSetting;
    NVPW_CounterData_GetRangeDescriptions_Fn                     NVPW_CounterData_GetRangeDescriptions;
    NVPW_Profiler_CounterData_GetRangeDescriptions_Fn            NVPW_Profiler_CounterData_GetRangeDescriptions;
    NVPW_PeriodicSampler_CounterData_GetSampleTime_Fn            NVPW_PeriodicSampler_CounterData_GetSampleTime;
    NVPW_PeriodicSampler_CounterData_TrimInPlace_Fn              NVPW_PeriodicSampler_CounterData_TrimInPlace;
    NVPW_PeriodicSampler_CounterData_GetInfo_Fn                  NVPW_PeriodicSampler_CounterData_GetInfo;
    NVPW_PeriodicSampler_CounterData_GetTriggerCount_Fn          NVPW_PeriodicSampler_CounterData_GetTriggerCount;
    NVPW_D3D12_RawMetricsConfig_Create_Fn                        NVPW_D3D12_RawMetricsConfig_Create;
    NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Fn    NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize;
    NVPW_D3D12_MetricsEvaluator_Initialize_Fn                    NVPW_D3D12_MetricsEvaluator_Initialize;
    NVPW_D3D12_LoadDriver_Fn                                     NVPW_D3D12_LoadDriver;
    NVPW_D3D12_Device_GetDeviceIndex_Fn                          NVPW_D3D12_Device_GetDeviceIndex;
    NVPW_D3D12_GetLUID_Fn                                        NVPW_D3D12_GetLUID;
    NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Fn        NVPW_D3D12_Profiler_CounterDataImage_CalculateSize;
    NVPW_D3D12_Profiler_CounterDataImage_Initialize_Fn           NVPW_D3D12_Profiler_CounterDataImage_Initialize;
    NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize;
    NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Fn NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer;
    NVPW_D3D12_Profiler_CalcTraceBufferSize_Fn                   NVPW_D3D12_Profiler_CalcTraceBufferSize;
    NVPW_D3D12_Profiler_Queue_BeginSession_Fn                    NVPW_D3D12_Profiler_Queue_BeginSession;
    NVPW_D3D12_Profiler_Queue_EndSession_Fn                      NVPW_D3D12_Profiler_Queue_EndSession;
    NVPW_D3D12_Queue_ServicePendingGpuOperations_Fn              NVPW_D3D12_Queue_ServicePendingGpuOperations;
    NVPW_D3D12_Profiler_Queue_SetConfig_Fn                       NVPW_D3D12_Profiler_Queue_SetConfig;
    NVPW_D3D12_Profiler_Queue_ClearConfig_Fn                     NVPW_D3D12_Profiler_Queue_ClearConfig;
    NVPW_D3D12_Profiler_Queue_BeginPass_Fn                       NVPW_D3D12_Profiler_Queue_BeginPass;
    NVPW_D3D12_Profiler_Queue_EndPass_Fn                         NVPW_D3D12_Profiler_Queue_EndPass;
    NVPW_D3D12_Profiler_Queue_PushRange_Fn                       NVPW_D3D12_Profiler_Queue_PushRange;
    NVPW_D3D12_Profiler_Queue_PopRange_Fn                        NVPW_D3D12_Profiler_Queue_PopRange;
    NVPW_D3D12_Profiler_CommandList_PushRange_Fn                 NVPW_D3D12_Profiler_CommandList_PushRange;
    NVPW_D3D12_Profiler_CommandList_PopRange_Fn                  NVPW_D3D12_Profiler_CommandList_PopRange;
    NVPW_D3D12_Profiler_Queue_DecodeCounters_Fn                  NVPW_D3D12_Profiler_Queue_DecodeCounters;
    NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Fn          NVPW_D3D12_Profiler_Queue_GetCounterAvailability;
    NVPW_D3D12_Profiler_IsGpuSupported_Fn                        NVPW_D3D12_Profiler_IsGpuSupported;
    NVPW_D3D12_MiniTrace_IsGpuSupported_Fn                       NVPW_D3D12_MiniTrace_IsGpuSupported;
    NVPW_D3D12_MiniTrace_DeviceState_Create_Fn                   NVPW_D3D12_MiniTrace_DeviceState_Create;
    NVPW_D3D12_MiniTrace_DeviceState_Destroy_Fn                  NVPW_D3D12_MiniTrace_DeviceState_Destroy;
    NVPW_D3D12_MiniTrace_Queue_Register_Fn                       NVPW_D3D12_MiniTrace_Queue_Register;
    NVPW_D3D12_MiniTrace_Queue_Unregister_Fn                     NVPW_D3D12_MiniTrace_Queue_Unregister;
    NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Fn          NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger;
    NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Fn                NVPW_D3D12_MiniTrace_CommandList_MarkerCpu;
    NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Fn            NVPW_D3D12_MiniTrace_CommandList_HostTimestamp;
    NVPW_D3D11_RawMetricsConfig_Create_Fn                        NVPW_D3D11_RawMetricsConfig_Create;
    NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Fn    NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize;
    NVPW_D3D11_MetricsEvaluator_Initialize_Fn                    NVPW_D3D11_MetricsEvaluator_Initialize;
    NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Fn        NVPW_D3D11_Profiler_CounterDataImage_CalculateSize;
    NVPW_D3D11_Profiler_CounterDataImage_Initialize_Fn           NVPW_D3D11_Profiler_CounterDataImage_Initialize;
    NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize;
    NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Fn NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer;
    NVPW_D3D11_LoadDriver_Fn                                     NVPW_D3D11_LoadDriver;
    NVPW_D3D11_GetLUID_Fn                                        NVPW_D3D11_GetLUID;
    NVPW_D3D11_Device_GetDeviceIndex_Fn                          NVPW_D3D11_Device_GetDeviceIndex;
    NVPW_D3D11_Profiler_CalcTraceBufferSize_Fn                   NVPW_D3D11_Profiler_CalcTraceBufferSize;
    NVPW_D3D11_Profiler_DeviceContext_BeginSession_Fn            NVPW_D3D11_Profiler_DeviceContext_BeginSession;
    NVPW_D3D11_Profiler_DeviceContext_EndSession_Fn              NVPW_D3D11_Profiler_DeviceContext_EndSession;
    NVPW_D3D11_Profiler_DeviceContext_SetConfig_Fn               NVPW_D3D11_Profiler_DeviceContext_SetConfig;
    NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Fn             NVPW_D3D11_Profiler_DeviceContext_ClearConfig;
    NVPW_D3D11_Profiler_DeviceContext_BeginPass_Fn               NVPW_D3D11_Profiler_DeviceContext_BeginPass;
    NVPW_D3D11_Profiler_DeviceContext_EndPass_Fn                 NVPW_D3D11_Profiler_DeviceContext_EndPass;
    NVPW_D3D11_Profiler_DeviceContext_PushRange_Fn               NVPW_D3D11_Profiler_DeviceContext_PushRange;
    NVPW_D3D11_Profiler_DeviceContext_PopRange_Fn                NVPW_D3D11_Profiler_DeviceContext_PopRange;
    NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Fn          NVPW_D3D11_Profiler_DeviceContext_DecodeCounters;
    NVPW_D3D11_Profiler_IsGpuSupported_Fn                        NVPW_D3D11_Profiler_IsGpuSupported;
    NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Fn  NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability;
    NVPW_Device_RawMetricsConfig_Create_Fn                       NVPW_Device_RawMetricsConfig_Create;
    NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Fn   NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize;
    NVPW_Device_MetricsEvaluator_Initialize_Fn                   NVPW_Device_MetricsEvaluator_Initialize;
    NVPW_GPU_PeriodicSampler_IsGpuSupported_Fn                   NVPW_GPU_PeriodicSampler_IsGpuSupported;
    NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Fn       NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources;
    NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Fn        NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize;
    NVPW_GPU_PeriodicSampler_BeginSession_Fn                     NVPW_GPU_PeriodicSampler_BeginSession;
    NVPW_GPU_PeriodicSampler_BeginSession_V2_Fn                  NVPW_GPU_PeriodicSampler_BeginSession_V2;
    NVPW_GPU_PeriodicSampler_EndSession_Fn                       NVPW_GPU_PeriodicSampler_EndSession;
    NVPW_GPU_PeriodicSampler_GetCounterAvailability_Fn           NVPW_GPU_PeriodicSampler_GetCounterAvailability;
    NVPW_GPU_PeriodicSampler_SetConfig_Fn                        NVPW_GPU_PeriodicSampler_SetConfig;
    NVPW_GPU_PeriodicSampler_StartSampling_Fn                    NVPW_GPU_PeriodicSampler_StartSampling;
    NVPW_GPU_PeriodicSampler_StopSampling_Fn                     NVPW_GPU_PeriodicSampler_StopSampling;
    NVPW_GPU_PeriodicSampler_CpuTrigger_Fn                       NVPW_GPU_PeriodicSampler_CpuTrigger;
    NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Fn   NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize;
    NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Fn      NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize;
    NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Fn            NVPW_GPU_PeriodicSampler_GetRecordBufferStatus;
    NVPW_GPU_PeriodicSampler_DecodeCounters_Fn                   NVPW_GPU_PeriodicSampler_DecodeCounters;
    NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Fn                NVPW_GPU_PeriodicSampler_DecodeCounters_V2;
    NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Fn NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported;
    NVPW_VK_RawMetricsConfig_Create_Fn                           NVPW_VK_RawMetricsConfig_Create;
    NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Fn       NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize;
    NVPW_VK_MetricsEvaluator_Initialize_Fn                       NVPW_VK_MetricsEvaluator_Initialize;
    NVPW_VK_Profiler_CounterDataImage_CalculateSize_Fn           NVPW_VK_Profiler_CounterDataImage_CalculateSize;
    NVPW_VK_Profiler_CounterDataImage_Initialize_Fn              NVPW_VK_Profiler_CounterDataImage_Initialize;
    NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize;
    NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Fn NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer;
    NVPW_VK_LoadDriver_Fn                                        NVPW_VK_LoadDriver;
    NVPW_VK_Device_GetDeviceIndex_Fn                             NVPW_VK_Device_GetDeviceIndex;
    NVPW_VK_Profiler_GetRequiredInstanceExtensions_Fn            NVPW_VK_Profiler_GetRequiredInstanceExtensions;
    NVPW_VK_Profiler_GetRequiredDeviceExtensions_Fn              NVPW_VK_Profiler_GetRequiredDeviceExtensions;
    NVPW_VK_Profiler_CalcTraceBufferSize_Fn                      NVPW_VK_Profiler_CalcTraceBufferSize;
    NVPW_VK_Profiler_Queue_BeginSession_Fn                       NVPW_VK_Profiler_Queue_BeginSession;
    NVPW_VK_Profiler_Queue_EndSession_Fn                         NVPW_VK_Profiler_Queue_EndSession;
    NVPW_VK_Queue_ServicePendingGpuOperations_Fn                 NVPW_VK_Queue_ServicePendingGpuOperations;
    NVPW_VK_Profiler_Queue_SetConfig_Fn                          NVPW_VK_Profiler_Queue_SetConfig;
    NVPW_VK_Profiler_Queue_ClearConfig_Fn                        NVPW_VK_Profiler_Queue_ClearConfig;
    NVPW_VK_Profiler_Queue_BeginPass_Fn                          NVPW_VK_Profiler_Queue_BeginPass;
    NVPW_VK_Profiler_Queue_EndPass_Fn                            NVPW_VK_Profiler_Queue_EndPass;
    NVPW_VK_Profiler_CommandBuffer_PushRange_Fn                  NVPW_VK_Profiler_CommandBuffer_PushRange;
    NVPW_VK_Profiler_CommandBuffer_PopRange_Fn                   NVPW_VK_Profiler_CommandBuffer_PopRange;
    NVPW_VK_Profiler_Queue_DecodeCounters_Fn                     NVPW_VK_Profiler_Queue_DecodeCounters;
    NVPW_VK_Profiler_IsGpuSupported_Fn                           NVPW_VK_Profiler_IsGpuSupported;
    NVPW_VK_Profiler_Queue_GetCounterAvailability_Fn             NVPW_VK_Profiler_Queue_GetCounterAvailability;
    NVPW_VK_MiniTrace_IsGpuSupported_Fn                          NVPW_VK_MiniTrace_IsGpuSupported;
    NVPW_VK_MiniTrace_DeviceState_Create_Fn                      NVPW_VK_MiniTrace_DeviceState_Create;
    NVPW_VK_MiniTrace_DeviceState_Destroy_Fn                     NVPW_VK_MiniTrace_DeviceState_Destroy;
    NVPW_VK_MiniTrace_Queue_Register_Fn                          NVPW_VK_MiniTrace_Queue_Register;
    NVPW_VK_MiniTrace_Queue_Unregister_Fn                        NVPW_VK_MiniTrace_Queue_Unregister;
    NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Fn           NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger;
    NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Fn                 NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu;
    NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Fn             NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp;
    NVPW_OpenGL_RawMetricsConfig_Create_Fn                       NVPW_OpenGL_RawMetricsConfig_Create;
    NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Fn   NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize;
    NVPW_OpenGL_MetricsEvaluator_Initialize_Fn                   NVPW_OpenGL_MetricsEvaluator_Initialize;
    NVPW_OpenGL_LoadDriver_Fn                                    NVPW_OpenGL_LoadDriver;
    NVPW_OpenGL_GetCurrentGraphicsContext_Fn                     NVPW_OpenGL_GetCurrentGraphicsContext;
    NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Fn                NVPW_OpenGL_GraphicsContext_GetDeviceIndex;
    NVPW_OpenGL_Profiler_IsGpuSupported_Fn                       NVPW_OpenGL_Profiler_IsGpuSupported;
    NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Fn       NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize;
    NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Fn          NVPW_OpenGL_Profiler_CounterDataImage_Initialize;
    NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize;
    NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Fn NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer;
    NVPW_OpenGL_Profiler_CalcTraceBufferSize_Fn                  NVPW_OpenGL_Profiler_CalcTraceBufferSize;
    NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Fn         NVPW_OpenGL_Profiler_GraphicsContext_BeginSession;
    NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Fn           NVPW_OpenGL_Profiler_GraphicsContext_EndSession;
    NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Fn            NVPW_OpenGL_Profiler_GraphicsContext_SetConfig;
    NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Fn          NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig;
    NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Fn            NVPW_OpenGL_Profiler_GraphicsContext_BeginPass;
    NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Fn              NVPW_OpenGL_Profiler_GraphicsContext_EndPass;
    NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Fn            NVPW_OpenGL_Profiler_GraphicsContext_PushRange;
    NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Fn             NVPW_OpenGL_Profiler_GraphicsContext_PopRange;
    NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Fn       NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters;
    NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Fn NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability;
} NvPerfApi;

#if _WIN32
typedef wchar_t NVPW_User_PathCharType;
#else
typedef char NVPW_User_PathCharType;
#endif

typedef struct NVPW_User_Api
{
    void* hModNvPerf;
    NVPA_GetProcAddress_Fn nvPerfGetProcAddress;
    NvPerfApi fn;
    size_t numSearchPaths;
    NVPW_User_PathCharType** ppSearchPaths;
} NVPW_User_Api;

static NVPW_User_Api g_api = {
      0 /* hModNvPerf */
    , 0 /* nvPerfGetProcAddress */
    , {
          &NVPA_GetProcAddress_Default
        , &NVPW_SetLibraryLoadPaths_Default
        , &NVPW_SetLibraryLoadPathsW_Default
        , &NVPW_InitializeHost_Default
        , &NVPW_CounterData_CalculateCounterDataImageCopySize_Default
        , &NVPW_CounterData_InitializeCounterDataImageCopy_Default
        , &NVPW_CounterDataCombiner_Create_Default
        , &NVPW_CounterDataCombiner_Destroy_Default
        , &NVPW_CounterDataCombiner_CreateRange_Default
        , &NVPW_CounterDataCombiner_CopyIntoRange_Default
        , &NVPW_CounterDataCombiner_AccumulateIntoRange_Default
        , &NVPW_CounterDataCombiner_SumIntoRange_Default
        , &NVPW_CounterDataCombiner_WeightedSumIntoRange_Default
        , &NVPW_GetSupportedChipNames_Default
        , &NVPW_RawMetricsConfig_Destroy_Default
        , &NVPW_RawMetricsConfig_SetCounterAvailability_Default
        , &NVPW_RawMetricsConfig_BeginPassGroup_Default
        , &NVPW_RawMetricsConfig_EndPassGroup_Default
        , &NVPW_RawMetricsConfig_GetNumMetrics_Default
        , &NVPW_RawMetricsConfig_GetMetricProperties_V2_Default
        , &NVPW_RawMetricsConfig_AddMetrics_Default
        , &NVPW_RawMetricsConfig_IsAddMetricsPossible_Default
        , &NVPW_RawMetricsConfig_GenerateConfigImage_Default
        , &NVPW_RawMetricsConfig_GetConfigImage_Default
        , &NVPW_RawMetricsConfig_GetNumPasses_V2_Default
        , &NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Default
        , &NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Default
        , &NVPW_CounterDataBuilder_Create_Default
        , &NVPW_CounterDataBuilder_Destroy_Default
        , &NVPW_CounterDataBuilder_AddMetrics_Default
        , &NVPW_CounterDataBuilder_GetCounterDataPrefix_Default
        , &NVPW_MetricsEvaluator_Destroy_Default
        , &NVPW_MetricsEvaluator_GetMetricNames_Default
        , &NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Default
        , &NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Default
        , &NVPW_MetricsEvaluator_HwUnitToString_Default
        , &NVPW_MetricsEvaluator_GetCounterProperties_Default
        , &NVPW_MetricsEvaluator_GetRatioMetricProperties_Default
        , &NVPW_MetricsEvaluator_GetThroughputMetricProperties_Default
        , &NVPW_MetricsEvaluator_GetSupportedSubmetrics_Default
        , &NVPW_MetricsEvaluator_GetMetricRawDependencies_Default
        , &NVPW_MetricsEvaluator_DimUnitToString_Default
        , &NVPW_MetricsEvaluator_GetMetricDimUnits_Default
        , &NVPW_MetricsEvaluator_SetUserData_Default
        , &NVPW_MetricsEvaluator_EvaluateToGpuValues_Default
        , &NVPW_MetricsEvaluator_SetDeviceAttributes_Default
        , &NVPW_InitializeTarget_Default
        , &NVPW_GetDeviceCount_Default
        , &NVPW_Device_GetNames_Default
        , &NVPW_Device_GetPciBusIds_Default
        , &NVPW_Device_GetMigAttributes_Default
        , &NVPW_Adapter_GetDeviceIndex_Default
        , &NVPW_CounterData_GetNumRanges_Default
        , &NVPW_CounterData_GetChipName_Default
        , &NVPW_Config_GetNumPasses_V2_Default
        , &NVPW_QueryVersionNumber_Default
        , &NVPW_Device_GetClockStatus_Default
        , &NVPW_Device_SetClockSetting_Default
        , &NVPW_CounterData_GetRangeDescriptions_Default
        , &NVPW_Profiler_CounterData_GetRangeDescriptions_Default
        , &NVPW_PeriodicSampler_CounterData_GetSampleTime_Default
        , &NVPW_PeriodicSampler_CounterData_TrimInPlace_Default
        , &NVPW_PeriodicSampler_CounterData_GetInfo_Default
        , &NVPW_PeriodicSampler_CounterData_GetTriggerCount_Default
        , &NVPW_D3D12_RawMetricsConfig_Create_Default
        , &NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Default
        , &NVPW_D3D12_MetricsEvaluator_Initialize_Default
        , &NVPW_D3D12_LoadDriver_Default
        , &NVPW_D3D12_Device_GetDeviceIndex_Default
        , &NVPW_D3D12_GetLUID_Default
        , &NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Default
        , &NVPW_D3D12_Profiler_CounterDataImage_Initialize_Default
        , &NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Default
        , &NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Default
        , &NVPW_D3D12_Profiler_CalcTraceBufferSize_Default
        , &NVPW_D3D12_Profiler_Queue_BeginSession_Default
        , &NVPW_D3D12_Profiler_Queue_EndSession_Default
        , &NVPW_D3D12_Queue_ServicePendingGpuOperations_Default
        , &NVPW_D3D12_Profiler_Queue_SetConfig_Default
        , &NVPW_D3D12_Profiler_Queue_ClearConfig_Default
        , &NVPW_D3D12_Profiler_Queue_BeginPass_Default
        , &NVPW_D3D12_Profiler_Queue_EndPass_Default
        , &NVPW_D3D12_Profiler_Queue_PushRange_Default
        , &NVPW_D3D12_Profiler_Queue_PopRange_Default
        , &NVPW_D3D12_Profiler_CommandList_PushRange_Default
        , &NVPW_D3D12_Profiler_CommandList_PopRange_Default
        , &NVPW_D3D12_Profiler_Queue_DecodeCounters_Default
        , &NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Default
        , &NVPW_D3D12_Profiler_IsGpuSupported_Default
        , &NVPW_D3D12_MiniTrace_IsGpuSupported_Default
        , &NVPW_D3D12_MiniTrace_DeviceState_Create_Default
        , &NVPW_D3D12_MiniTrace_DeviceState_Destroy_Default
        , &NVPW_D3D12_MiniTrace_Queue_Register_Default
        , &NVPW_D3D12_MiniTrace_Queue_Unregister_Default
        , &NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Default
        , &NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Default
        , &NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Default
        , &NVPW_D3D11_RawMetricsConfig_Create_Default
        , &NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Default
        , &NVPW_D3D11_MetricsEvaluator_Initialize_Default
        , &NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Default
        , &NVPW_D3D11_Profiler_CounterDataImage_Initialize_Default
        , &NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Default
        , &NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Default
        , &NVPW_D3D11_LoadDriver_Default
        , &NVPW_D3D11_GetLUID_Default
        , &NVPW_D3D11_Device_GetDeviceIndex_Default
        , &NVPW_D3D11_Profiler_CalcTraceBufferSize_Default
        , &NVPW_D3D11_Profiler_DeviceContext_BeginSession_Default
        , &NVPW_D3D11_Profiler_DeviceContext_EndSession_Default
        , &NVPW_D3D11_Profiler_DeviceContext_SetConfig_Default
        , &NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Default
        , &NVPW_D3D11_Profiler_DeviceContext_BeginPass_Default
        , &NVPW_D3D11_Profiler_DeviceContext_EndPass_Default
        , &NVPW_D3D11_Profiler_DeviceContext_PushRange_Default
        , &NVPW_D3D11_Profiler_DeviceContext_PopRange_Default
        , &NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Default
        , &NVPW_D3D11_Profiler_IsGpuSupported_Default
        , &NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Default
        , &NVPW_Device_RawMetricsConfig_Create_Default
        , &NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Default
        , &NVPW_Device_MetricsEvaluator_Initialize_Default
        , &NVPW_GPU_PeriodicSampler_IsGpuSupported_Default
        , &NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Default
        , &NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Default
        , &NVPW_GPU_PeriodicSampler_BeginSession_Default
        , &NVPW_GPU_PeriodicSampler_BeginSession_V2_Default
        , &NVPW_GPU_PeriodicSampler_EndSession_Default
        , &NVPW_GPU_PeriodicSampler_GetCounterAvailability_Default
        , &NVPW_GPU_PeriodicSampler_SetConfig_Default
        , &NVPW_GPU_PeriodicSampler_StartSampling_Default
        , &NVPW_GPU_PeriodicSampler_StopSampling_Default
        , &NVPW_GPU_PeriodicSampler_CpuTrigger_Default
        , &NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Default
        , &NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Default
        , &NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Default
        , &NVPW_GPU_PeriodicSampler_DecodeCounters_Default
        , &NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Default
        , &NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Default
        , &NVPW_VK_RawMetricsConfig_Create_Default
        , &NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Default
        , &NVPW_VK_MetricsEvaluator_Initialize_Default
        , &NVPW_VK_Profiler_CounterDataImage_CalculateSize_Default
        , &NVPW_VK_Profiler_CounterDataImage_Initialize_Default
        , &NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Default
        , &NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Default
        , &NVPW_VK_LoadDriver_Default
        , &NVPW_VK_Device_GetDeviceIndex_Default
        , &NVPW_VK_Profiler_GetRequiredInstanceExtensions_Default
        , &NVPW_VK_Profiler_GetRequiredDeviceExtensions_Default
        , &NVPW_VK_Profiler_CalcTraceBufferSize_Default
        , &NVPW_VK_Profiler_Queue_BeginSession_Default
        , &NVPW_VK_Profiler_Queue_EndSession_Default
        , &NVPW_VK_Queue_ServicePendingGpuOperations_Default
        , &NVPW_VK_Profiler_Queue_SetConfig_Default
        , &NVPW_VK_Profiler_Queue_ClearConfig_Default
        , &NVPW_VK_Profiler_Queue_BeginPass_Default
        , &NVPW_VK_Profiler_Queue_EndPass_Default
        , &NVPW_VK_Profiler_CommandBuffer_PushRange_Default
        , &NVPW_VK_Profiler_CommandBuffer_PopRange_Default
        , &NVPW_VK_Profiler_Queue_DecodeCounters_Default
        , &NVPW_VK_Profiler_IsGpuSupported_Default
        , &NVPW_VK_Profiler_Queue_GetCounterAvailability_Default
        , &NVPW_VK_MiniTrace_IsGpuSupported_Default
        , &NVPW_VK_MiniTrace_DeviceState_Create_Default
        , &NVPW_VK_MiniTrace_DeviceState_Destroy_Default
        , &NVPW_VK_MiniTrace_Queue_Register_Default
        , &NVPW_VK_MiniTrace_Queue_Unregister_Default
        , &NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Default
        , &NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Default
        , &NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Default
        , &NVPW_OpenGL_RawMetricsConfig_Create_Default
        , &NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Default
        , &NVPW_OpenGL_MetricsEvaluator_Initialize_Default
        , &NVPW_OpenGL_LoadDriver_Default
        , &NVPW_OpenGL_GetCurrentGraphicsContext_Default
        , &NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Default
        , &NVPW_OpenGL_Profiler_IsGpuSupported_Default
        , &NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Default
        , &NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Default
        , &NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Default
        , &NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Default
        , &NVPW_OpenGL_Profiler_CalcTraceBufferSize_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Default
        , &NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Default
      }
    , 0 /* numSearchPaths */
    , 0 /* ppSearchPaths */
};

static NVPA_GenericFn GetNvPerfProc(const char* pName, NVPA_GenericFn pDefault);
static int InitNvPerf(void);

static void InitNvPerfProcs(void)
{
    g_api.fn.NVPA_GetProcAddress = (NVPA_GetProcAddress_Fn)GetNvPerfProc("NVPA_GetProcAddress", (NVPA_GenericFn)g_api.fn.NVPA_GetProcAddress);
    g_api.fn.NVPW_SetLibraryLoadPaths = (NVPW_SetLibraryLoadPaths_Fn)GetNvPerfProc("NVPW_SetLibraryLoadPaths", (NVPA_GenericFn)g_api.fn.NVPW_SetLibraryLoadPaths);
    g_api.fn.NVPW_SetLibraryLoadPathsW = (NVPW_SetLibraryLoadPathsW_Fn)GetNvPerfProc("NVPW_SetLibraryLoadPathsW", (NVPA_GenericFn)g_api.fn.NVPW_SetLibraryLoadPathsW);
    g_api.fn.NVPW_InitializeHost = (NVPW_InitializeHost_Fn)GetNvPerfProc("NVPW_InitializeHost", (NVPA_GenericFn)g_api.fn.NVPW_InitializeHost);
    g_api.fn.NVPW_CounterData_CalculateCounterDataImageCopySize = (NVPW_CounterData_CalculateCounterDataImageCopySize_Fn)GetNvPerfProc("NVPW_CounterData_CalculateCounterDataImageCopySize", (NVPA_GenericFn)g_api.fn.NVPW_CounterData_CalculateCounterDataImageCopySize);
    g_api.fn.NVPW_CounterData_InitializeCounterDataImageCopy = (NVPW_CounterData_InitializeCounterDataImageCopy_Fn)GetNvPerfProc("NVPW_CounterData_InitializeCounterDataImageCopy", (NVPA_GenericFn)g_api.fn.NVPW_CounterData_InitializeCounterDataImageCopy);
    g_api.fn.NVPW_CounterDataCombiner_Create = (NVPW_CounterDataCombiner_Create_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_Create", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_Create);
    g_api.fn.NVPW_CounterDataCombiner_Destroy = (NVPW_CounterDataCombiner_Destroy_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_Destroy);
    g_api.fn.NVPW_CounterDataCombiner_CreateRange = (NVPW_CounterDataCombiner_CreateRange_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_CreateRange", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_CreateRange);
    g_api.fn.NVPW_CounterDataCombiner_CopyIntoRange = (NVPW_CounterDataCombiner_CopyIntoRange_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_CopyIntoRange", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_CopyIntoRange);
    g_api.fn.NVPW_CounterDataCombiner_AccumulateIntoRange = (NVPW_CounterDataCombiner_AccumulateIntoRange_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_AccumulateIntoRange", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_AccumulateIntoRange);
    g_api.fn.NVPW_CounterDataCombiner_SumIntoRange = (NVPW_CounterDataCombiner_SumIntoRange_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_SumIntoRange", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_SumIntoRange);
    g_api.fn.NVPW_CounterDataCombiner_WeightedSumIntoRange = (NVPW_CounterDataCombiner_WeightedSumIntoRange_Fn)GetNvPerfProc("NVPW_CounterDataCombiner_WeightedSumIntoRange", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataCombiner_WeightedSumIntoRange);
    g_api.fn.NVPW_GetSupportedChipNames = (NVPW_GetSupportedChipNames_Fn)GetNvPerfProc("NVPW_GetSupportedChipNames", (NVPA_GenericFn)g_api.fn.NVPW_GetSupportedChipNames);
    g_api.fn.NVPW_RawMetricsConfig_Destroy = (NVPW_RawMetricsConfig_Destroy_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_Destroy);
    g_api.fn.NVPW_RawMetricsConfig_SetCounterAvailability = (NVPW_RawMetricsConfig_SetCounterAvailability_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_SetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_SetCounterAvailability);
    g_api.fn.NVPW_RawMetricsConfig_BeginPassGroup = (NVPW_RawMetricsConfig_BeginPassGroup_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_BeginPassGroup", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_BeginPassGroup);
    g_api.fn.NVPW_RawMetricsConfig_EndPassGroup = (NVPW_RawMetricsConfig_EndPassGroup_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_EndPassGroup", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_EndPassGroup);
    g_api.fn.NVPW_RawMetricsConfig_GetNumMetrics = (NVPW_RawMetricsConfig_GetNumMetrics_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_GetNumMetrics", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_GetNumMetrics);
    g_api.fn.NVPW_RawMetricsConfig_GetMetricProperties_V2 = (NVPW_RawMetricsConfig_GetMetricProperties_V2_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_GetMetricProperties_V2", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_GetMetricProperties_V2);
    g_api.fn.NVPW_RawMetricsConfig_AddMetrics = (NVPW_RawMetricsConfig_AddMetrics_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_AddMetrics", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_AddMetrics);
    g_api.fn.NVPW_RawMetricsConfig_IsAddMetricsPossible = (NVPW_RawMetricsConfig_IsAddMetricsPossible_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_IsAddMetricsPossible", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_IsAddMetricsPossible);
    g_api.fn.NVPW_RawMetricsConfig_GenerateConfigImage = (NVPW_RawMetricsConfig_GenerateConfigImage_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_GenerateConfigImage", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_GenerateConfigImage);
    g_api.fn.NVPW_RawMetricsConfig_GetConfigImage = (NVPW_RawMetricsConfig_GetConfigImage_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_GetConfigImage", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_GetConfigImage);
    g_api.fn.NVPW_RawMetricsConfig_GetNumPasses_V2 = (NVPW_RawMetricsConfig_GetNumPasses_V2_Fn)GetNvPerfProc("NVPW_RawMetricsConfig_GetNumPasses_V2", (NVPA_GenericFn)g_api.fn.NVPW_RawMetricsConfig_GetNumPasses_V2);
    g_api.fn.NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize = (NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Fn)GetNvPerfProc("NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize);
    g_api.fn.NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize = (NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Fn)GetNvPerfProc("NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize);
    g_api.fn.NVPW_CounterDataBuilder_Create = (NVPW_CounterDataBuilder_Create_Fn)GetNvPerfProc("NVPW_CounterDataBuilder_Create", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataBuilder_Create);
    g_api.fn.NVPW_CounterDataBuilder_Destroy = (NVPW_CounterDataBuilder_Destroy_Fn)GetNvPerfProc("NVPW_CounterDataBuilder_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataBuilder_Destroy);
    g_api.fn.NVPW_CounterDataBuilder_AddMetrics = (NVPW_CounterDataBuilder_AddMetrics_Fn)GetNvPerfProc("NVPW_CounterDataBuilder_AddMetrics", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataBuilder_AddMetrics);
    g_api.fn.NVPW_CounterDataBuilder_GetCounterDataPrefix = (NVPW_CounterDataBuilder_GetCounterDataPrefix_Fn)GetNvPerfProc("NVPW_CounterDataBuilder_GetCounterDataPrefix", (NVPA_GenericFn)g_api.fn.NVPW_CounterDataBuilder_GetCounterDataPrefix);
    g_api.fn.NVPW_MetricsEvaluator_Destroy = (NVPW_MetricsEvaluator_Destroy_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_Destroy);
    g_api.fn.NVPW_MetricsEvaluator_GetMetricNames = (NVPW_MetricsEvaluator_GetMetricNames_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetMetricNames", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetMetricNames);
    g_api.fn.NVPW_MetricsEvaluator_GetMetricTypeAndIndex = (NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetMetricTypeAndIndex", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetMetricTypeAndIndex);
    g_api.fn.NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest = (NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest);
    g_api.fn.NVPW_MetricsEvaluator_HwUnitToString = (NVPW_MetricsEvaluator_HwUnitToString_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_HwUnitToString", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_HwUnitToString);
    g_api.fn.NVPW_MetricsEvaluator_GetCounterProperties = (NVPW_MetricsEvaluator_GetCounterProperties_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetCounterProperties", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetCounterProperties);
    g_api.fn.NVPW_MetricsEvaluator_GetRatioMetricProperties = (NVPW_MetricsEvaluator_GetRatioMetricProperties_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetRatioMetricProperties", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetRatioMetricProperties);
    g_api.fn.NVPW_MetricsEvaluator_GetThroughputMetricProperties = (NVPW_MetricsEvaluator_GetThroughputMetricProperties_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetThroughputMetricProperties", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetThroughputMetricProperties);
    g_api.fn.NVPW_MetricsEvaluator_GetSupportedSubmetrics = (NVPW_MetricsEvaluator_GetSupportedSubmetrics_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetSupportedSubmetrics", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetSupportedSubmetrics);
    g_api.fn.NVPW_MetricsEvaluator_GetMetricRawDependencies = (NVPW_MetricsEvaluator_GetMetricRawDependencies_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetMetricRawDependencies", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetMetricRawDependencies);
    g_api.fn.NVPW_MetricsEvaluator_DimUnitToString = (NVPW_MetricsEvaluator_DimUnitToString_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_DimUnitToString", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_DimUnitToString);
    g_api.fn.NVPW_MetricsEvaluator_GetMetricDimUnits = (NVPW_MetricsEvaluator_GetMetricDimUnits_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_GetMetricDimUnits", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_GetMetricDimUnits);
    g_api.fn.NVPW_MetricsEvaluator_SetUserData = (NVPW_MetricsEvaluator_SetUserData_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_SetUserData", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_SetUserData);
    g_api.fn.NVPW_MetricsEvaluator_EvaluateToGpuValues = (NVPW_MetricsEvaluator_EvaluateToGpuValues_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_EvaluateToGpuValues", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_EvaluateToGpuValues);
    g_api.fn.NVPW_MetricsEvaluator_SetDeviceAttributes = (NVPW_MetricsEvaluator_SetDeviceAttributes_Fn)GetNvPerfProc("NVPW_MetricsEvaluator_SetDeviceAttributes", (NVPA_GenericFn)g_api.fn.NVPW_MetricsEvaluator_SetDeviceAttributes);
    g_api.fn.NVPW_InitializeTarget = (NVPW_InitializeTarget_Fn)GetNvPerfProc("NVPW_InitializeTarget", (NVPA_GenericFn)g_api.fn.NVPW_InitializeTarget);
    g_api.fn.NVPW_GetDeviceCount = (NVPW_GetDeviceCount_Fn)GetNvPerfProc("NVPW_GetDeviceCount", (NVPA_GenericFn)g_api.fn.NVPW_GetDeviceCount);
    g_api.fn.NVPW_Device_GetNames = (NVPW_Device_GetNames_Fn)GetNvPerfProc("NVPW_Device_GetNames", (NVPA_GenericFn)g_api.fn.NVPW_Device_GetNames);
    g_api.fn.NVPW_Device_GetPciBusIds = (NVPW_Device_GetPciBusIds_Fn)GetNvPerfProc("NVPW_Device_GetPciBusIds", (NVPA_GenericFn)g_api.fn.NVPW_Device_GetPciBusIds);
    g_api.fn.NVPW_Device_GetMigAttributes = (NVPW_Device_GetMigAttributes_Fn)GetNvPerfProc("NVPW_Device_GetMigAttributes", (NVPA_GenericFn)g_api.fn.NVPW_Device_GetMigAttributes);
    g_api.fn.NVPW_Adapter_GetDeviceIndex = (NVPW_Adapter_GetDeviceIndex_Fn)GetNvPerfProc("NVPW_Adapter_GetDeviceIndex", (NVPA_GenericFn)g_api.fn.NVPW_Adapter_GetDeviceIndex);
    g_api.fn.NVPW_CounterData_GetNumRanges = (NVPW_CounterData_GetNumRanges_Fn)GetNvPerfProc("NVPW_CounterData_GetNumRanges", (NVPA_GenericFn)g_api.fn.NVPW_CounterData_GetNumRanges);
    g_api.fn.NVPW_CounterData_GetChipName = (NVPW_CounterData_GetChipName_Fn)GetNvPerfProc("NVPW_CounterData_GetChipName", (NVPA_GenericFn)g_api.fn.NVPW_CounterData_GetChipName);
    g_api.fn.NVPW_Config_GetNumPasses_V2 = (NVPW_Config_GetNumPasses_V2_Fn)GetNvPerfProc("NVPW_Config_GetNumPasses_V2", (NVPA_GenericFn)g_api.fn.NVPW_Config_GetNumPasses_V2);
    g_api.fn.NVPW_QueryVersionNumber = (NVPW_QueryVersionNumber_Fn)GetNvPerfProc("NVPW_QueryVersionNumber", (NVPA_GenericFn)g_api.fn.NVPW_QueryVersionNumber);
    g_api.fn.NVPW_Device_GetClockStatus = (NVPW_Device_GetClockStatus_Fn)GetNvPerfProc("NVPW_Device_GetClockStatus", (NVPA_GenericFn)g_api.fn.NVPW_Device_GetClockStatus);
    g_api.fn.NVPW_Device_SetClockSetting = (NVPW_Device_SetClockSetting_Fn)GetNvPerfProc("NVPW_Device_SetClockSetting", (NVPA_GenericFn)g_api.fn.NVPW_Device_SetClockSetting);
    g_api.fn.NVPW_CounterData_GetRangeDescriptions = (NVPW_CounterData_GetRangeDescriptions_Fn)GetNvPerfProc("NVPW_CounterData_GetRangeDescriptions", (NVPA_GenericFn)g_api.fn.NVPW_CounterData_GetRangeDescriptions);
    g_api.fn.NVPW_Profiler_CounterData_GetRangeDescriptions = (NVPW_Profiler_CounterData_GetRangeDescriptions_Fn)GetNvPerfProc("NVPW_Profiler_CounterData_GetRangeDescriptions", (NVPA_GenericFn)g_api.fn.NVPW_Profiler_CounterData_GetRangeDescriptions);
    g_api.fn.NVPW_PeriodicSampler_CounterData_GetSampleTime = (NVPW_PeriodicSampler_CounterData_GetSampleTime_Fn)GetNvPerfProc("NVPW_PeriodicSampler_CounterData_GetSampleTime", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_CounterData_GetSampleTime);
    g_api.fn.NVPW_PeriodicSampler_CounterData_TrimInPlace = (NVPW_PeriodicSampler_CounterData_TrimInPlace_Fn)GetNvPerfProc("NVPW_PeriodicSampler_CounterData_TrimInPlace", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_CounterData_TrimInPlace);
    g_api.fn.NVPW_PeriodicSampler_CounterData_GetInfo = (NVPW_PeriodicSampler_CounterData_GetInfo_Fn)GetNvPerfProc("NVPW_PeriodicSampler_CounterData_GetInfo", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_CounterData_GetInfo);
    g_api.fn.NVPW_PeriodicSampler_CounterData_GetTriggerCount = (NVPW_PeriodicSampler_CounterData_GetTriggerCount_Fn)GetNvPerfProc("NVPW_PeriodicSampler_CounterData_GetTriggerCount", (NVPA_GenericFn)g_api.fn.NVPW_PeriodicSampler_CounterData_GetTriggerCount);
    g_api.fn.NVPW_D3D12_RawMetricsConfig_Create = (NVPW_D3D12_RawMetricsConfig_Create_Fn)GetNvPerfProc("NVPW_D3D12_RawMetricsConfig_Create", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_RawMetricsConfig_Create);
    g_api.fn.NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize = (NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize);
    g_api.fn.NVPW_D3D12_MetricsEvaluator_Initialize = (NVPW_D3D12_MetricsEvaluator_Initialize_Fn)GetNvPerfProc("NVPW_D3D12_MetricsEvaluator_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MetricsEvaluator_Initialize);
    g_api.fn.NVPW_D3D12_LoadDriver = (NVPW_D3D12_LoadDriver_Fn)GetNvPerfProc("NVPW_D3D12_LoadDriver", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_LoadDriver);
    g_api.fn.NVPW_D3D12_Device_GetDeviceIndex = (NVPW_D3D12_Device_GetDeviceIndex_Fn)GetNvPerfProc("NVPW_D3D12_Device_GetDeviceIndex", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Device_GetDeviceIndex);
    g_api.fn.NVPW_D3D12_GetLUID = (NVPW_D3D12_GetLUID_Fn)GetNvPerfProc("NVPW_D3D12_GetLUID", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_GetLUID);
    g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateSize = (NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CounterDataImage_CalculateSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateSize);
    g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_Initialize = (NVPW_D3D12_Profiler_CounterDataImage_Initialize_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CounterDataImage_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_Initialize);
    g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize = (NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize);
    g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer = (NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer);
    g_api.fn.NVPW_D3D12_Profiler_CalcTraceBufferSize = (NVPW_D3D12_Profiler_CalcTraceBufferSize_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CalcTraceBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CalcTraceBufferSize);
    g_api.fn.NVPW_D3D12_Profiler_Queue_BeginSession = (NVPW_D3D12_Profiler_Queue_BeginSession_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_BeginSession", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_BeginSession);
    g_api.fn.NVPW_D3D12_Profiler_Queue_EndSession = (NVPW_D3D12_Profiler_Queue_EndSession_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_EndSession", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_EndSession);
    g_api.fn.NVPW_D3D12_Queue_ServicePendingGpuOperations = (NVPW_D3D12_Queue_ServicePendingGpuOperations_Fn)GetNvPerfProc("NVPW_D3D12_Queue_ServicePendingGpuOperations", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Queue_ServicePendingGpuOperations);
    g_api.fn.NVPW_D3D12_Profiler_Queue_SetConfig = (NVPW_D3D12_Profiler_Queue_SetConfig_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_SetConfig", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_SetConfig);
    g_api.fn.NVPW_D3D12_Profiler_Queue_ClearConfig = (NVPW_D3D12_Profiler_Queue_ClearConfig_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_ClearConfig", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_ClearConfig);
    g_api.fn.NVPW_D3D12_Profiler_Queue_BeginPass = (NVPW_D3D12_Profiler_Queue_BeginPass_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_BeginPass", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_BeginPass);
    g_api.fn.NVPW_D3D12_Profiler_Queue_EndPass = (NVPW_D3D12_Profiler_Queue_EndPass_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_EndPass", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_EndPass);
    g_api.fn.NVPW_D3D12_Profiler_Queue_PushRange = (NVPW_D3D12_Profiler_Queue_PushRange_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_PushRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_PushRange);
    g_api.fn.NVPW_D3D12_Profiler_Queue_PopRange = (NVPW_D3D12_Profiler_Queue_PopRange_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_PopRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_PopRange);
    g_api.fn.NVPW_D3D12_Profiler_CommandList_PushRange = (NVPW_D3D12_Profiler_CommandList_PushRange_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CommandList_PushRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CommandList_PushRange);
    g_api.fn.NVPW_D3D12_Profiler_CommandList_PopRange = (NVPW_D3D12_Profiler_CommandList_PopRange_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_CommandList_PopRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_CommandList_PopRange);
    g_api.fn.NVPW_D3D12_Profiler_Queue_DecodeCounters = (NVPW_D3D12_Profiler_Queue_DecodeCounters_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_DecodeCounters", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_DecodeCounters);
    g_api.fn.NVPW_D3D12_Profiler_Queue_GetCounterAvailability = (NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_Queue_GetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_Queue_GetCounterAvailability);
    g_api.fn.NVPW_D3D12_Profiler_IsGpuSupported = (NVPW_D3D12_Profiler_IsGpuSupported_Fn)GetNvPerfProc("NVPW_D3D12_Profiler_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_Profiler_IsGpuSupported);
    g_api.fn.NVPW_D3D12_MiniTrace_IsGpuSupported = (NVPW_D3D12_MiniTrace_IsGpuSupported_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_IsGpuSupported);
    g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Create = (NVPW_D3D12_MiniTrace_DeviceState_Create_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_DeviceState_Create", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Create);
    g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Destroy = (NVPW_D3D12_MiniTrace_DeviceState_Destroy_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_DeviceState_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Destroy);
    g_api.fn.NVPW_D3D12_MiniTrace_Queue_Register = (NVPW_D3D12_MiniTrace_Queue_Register_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_Queue_Register", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_Queue_Register);
    g_api.fn.NVPW_D3D12_MiniTrace_Queue_Unregister = (NVPW_D3D12_MiniTrace_Queue_Unregister_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_Queue_Unregister", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_Queue_Unregister);
    g_api.fn.NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger = (NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger);
    g_api.fn.NVPW_D3D12_MiniTrace_CommandList_MarkerCpu = (NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_CommandList_MarkerCpu", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_CommandList_MarkerCpu);
    g_api.fn.NVPW_D3D12_MiniTrace_CommandList_HostTimestamp = (NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Fn)GetNvPerfProc("NVPW_D3D12_MiniTrace_CommandList_HostTimestamp", (NVPA_GenericFn)g_api.fn.NVPW_D3D12_MiniTrace_CommandList_HostTimestamp);
    g_api.fn.NVPW_D3D11_RawMetricsConfig_Create = (NVPW_D3D11_RawMetricsConfig_Create_Fn)GetNvPerfProc("NVPW_D3D11_RawMetricsConfig_Create", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_RawMetricsConfig_Create);
    g_api.fn.NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize = (NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize);
    g_api.fn.NVPW_D3D11_MetricsEvaluator_Initialize = (NVPW_D3D11_MetricsEvaluator_Initialize_Fn)GetNvPerfProc("NVPW_D3D11_MetricsEvaluator_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_MetricsEvaluator_Initialize);
    g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateSize = (NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_CounterDataImage_CalculateSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateSize);
    g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_Initialize = (NVPW_D3D11_Profiler_CounterDataImage_Initialize_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_CounterDataImage_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_Initialize);
    g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize = (NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize);
    g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer = (NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer);
    g_api.fn.NVPW_D3D11_LoadDriver = (NVPW_D3D11_LoadDriver_Fn)GetNvPerfProc("NVPW_D3D11_LoadDriver", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_LoadDriver);
    g_api.fn.NVPW_D3D11_GetLUID = (NVPW_D3D11_GetLUID_Fn)GetNvPerfProc("NVPW_D3D11_GetLUID", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_GetLUID);
    g_api.fn.NVPW_D3D11_Device_GetDeviceIndex = (NVPW_D3D11_Device_GetDeviceIndex_Fn)GetNvPerfProc("NVPW_D3D11_Device_GetDeviceIndex", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Device_GetDeviceIndex);
    g_api.fn.NVPW_D3D11_Profiler_CalcTraceBufferSize = (NVPW_D3D11_Profiler_CalcTraceBufferSize_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_CalcTraceBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_CalcTraceBufferSize);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginSession = (NVPW_D3D11_Profiler_DeviceContext_BeginSession_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_BeginSession", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginSession);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndSession = (NVPW_D3D11_Profiler_DeviceContext_EndSession_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_EndSession", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndSession);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_SetConfig = (NVPW_D3D11_Profiler_DeviceContext_SetConfig_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_SetConfig", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_SetConfig);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_ClearConfig = (NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_ClearConfig", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_ClearConfig);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginPass = (NVPW_D3D11_Profiler_DeviceContext_BeginPass_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_BeginPass", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginPass);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndPass = (NVPW_D3D11_Profiler_DeviceContext_EndPass_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_EndPass", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndPass);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PushRange = (NVPW_D3D11_Profiler_DeviceContext_PushRange_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_PushRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PushRange);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PopRange = (NVPW_D3D11_Profiler_DeviceContext_PopRange_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_PopRange", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PopRange);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_DecodeCounters = (NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_DecodeCounters", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_DecodeCounters);
    g_api.fn.NVPW_D3D11_Profiler_IsGpuSupported = (NVPW_D3D11_Profiler_IsGpuSupported_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_IsGpuSupported);
    g_api.fn.NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability = (NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Fn)GetNvPerfProc("NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability);
    g_api.fn.NVPW_Device_RawMetricsConfig_Create = (NVPW_Device_RawMetricsConfig_Create_Fn)GetNvPerfProc("NVPW_Device_RawMetricsConfig_Create", (NVPA_GenericFn)g_api.fn.NVPW_Device_RawMetricsConfig_Create);
    g_api.fn.NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize = (NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize);
    g_api.fn.NVPW_Device_MetricsEvaluator_Initialize = (NVPW_Device_MetricsEvaluator_Initialize_Fn)GetNvPerfProc("NVPW_Device_MetricsEvaluator_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_Device_MetricsEvaluator_Initialize);
    g_api.fn.NVPW_GPU_PeriodicSampler_IsGpuSupported = (NVPW_GPU_PeriodicSampler_IsGpuSupported_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_IsGpuSupported);
    g_api.fn.NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources = (NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources);
    g_api.fn.NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize = (NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize);
    g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession = (NVPW_GPU_PeriodicSampler_BeginSession_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_BeginSession", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession);
    g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession_V2 = (NVPW_GPU_PeriodicSampler_BeginSession_V2_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_BeginSession_V2", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession_V2);
    g_api.fn.NVPW_GPU_PeriodicSampler_EndSession = (NVPW_GPU_PeriodicSampler_EndSession_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_EndSession", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_EndSession);
    g_api.fn.NVPW_GPU_PeriodicSampler_GetCounterAvailability = (NVPW_GPU_PeriodicSampler_GetCounterAvailability_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_GetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_GetCounterAvailability);
    g_api.fn.NVPW_GPU_PeriodicSampler_SetConfig = (NVPW_GPU_PeriodicSampler_SetConfig_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_SetConfig", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_SetConfig);
    g_api.fn.NVPW_GPU_PeriodicSampler_StartSampling = (NVPW_GPU_PeriodicSampler_StartSampling_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_StartSampling", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_StartSampling);
    g_api.fn.NVPW_GPU_PeriodicSampler_StopSampling = (NVPW_GPU_PeriodicSampler_StopSampling_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_StopSampling", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_StopSampling);
    g_api.fn.NVPW_GPU_PeriodicSampler_CpuTrigger = (NVPW_GPU_PeriodicSampler_CpuTrigger_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_CpuTrigger", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_CpuTrigger);
    g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize = (NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize);
    g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize = (NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize);
    g_api.fn.NVPW_GPU_PeriodicSampler_GetRecordBufferStatus = (NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_GetRecordBufferStatus", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_GetRecordBufferStatus);
    g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters = (NVPW_GPU_PeriodicSampler_DecodeCounters_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_DecodeCounters", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters);
    g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters_V2 = (NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_DecodeCounters_V2", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters_V2);
    g_api.fn.NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported = (NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Fn)GetNvPerfProc("NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported", (NVPA_GenericFn)g_api.fn.NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported);
    g_api.fn.NVPW_VK_RawMetricsConfig_Create = (NVPW_VK_RawMetricsConfig_Create_Fn)GetNvPerfProc("NVPW_VK_RawMetricsConfig_Create", (NVPA_GenericFn)g_api.fn.NVPW_VK_RawMetricsConfig_Create);
    g_api.fn.NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize = (NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize);
    g_api.fn.NVPW_VK_MetricsEvaluator_Initialize = (NVPW_VK_MetricsEvaluator_Initialize_Fn)GetNvPerfProc("NVPW_VK_MetricsEvaluator_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_VK_MetricsEvaluator_Initialize);
    g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateSize = (NVPW_VK_Profiler_CounterDataImage_CalculateSize_Fn)GetNvPerfProc("NVPW_VK_Profiler_CounterDataImage_CalculateSize", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateSize);
    g_api.fn.NVPW_VK_Profiler_CounterDataImage_Initialize = (NVPW_VK_Profiler_CounterDataImage_Initialize_Fn)GetNvPerfProc("NVPW_VK_Profiler_CounterDataImage_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CounterDataImage_Initialize);
    g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize = (NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize);
    g_api.fn.NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer = (NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)GetNvPerfProc("NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer);
    g_api.fn.NVPW_VK_LoadDriver = (NVPW_VK_LoadDriver_Fn)GetNvPerfProc("NVPW_VK_LoadDriver", (NVPA_GenericFn)g_api.fn.NVPW_VK_LoadDriver);
    g_api.fn.NVPW_VK_Device_GetDeviceIndex = (NVPW_VK_Device_GetDeviceIndex_Fn)GetNvPerfProc("NVPW_VK_Device_GetDeviceIndex", (NVPA_GenericFn)g_api.fn.NVPW_VK_Device_GetDeviceIndex);
    g_api.fn.NVPW_VK_Profiler_GetRequiredInstanceExtensions = (NVPW_VK_Profiler_GetRequiredInstanceExtensions_Fn)GetNvPerfProc("NVPW_VK_Profiler_GetRequiredInstanceExtensions", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_GetRequiredInstanceExtensions);
    g_api.fn.NVPW_VK_Profiler_GetRequiredDeviceExtensions = (NVPW_VK_Profiler_GetRequiredDeviceExtensions_Fn)GetNvPerfProc("NVPW_VK_Profiler_GetRequiredDeviceExtensions", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_GetRequiredDeviceExtensions);
    g_api.fn.NVPW_VK_Profiler_CalcTraceBufferSize = (NVPW_VK_Profiler_CalcTraceBufferSize_Fn)GetNvPerfProc("NVPW_VK_Profiler_CalcTraceBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CalcTraceBufferSize);
    g_api.fn.NVPW_VK_Profiler_Queue_BeginSession = (NVPW_VK_Profiler_Queue_BeginSession_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_BeginSession", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_BeginSession);
    g_api.fn.NVPW_VK_Profiler_Queue_EndSession = (NVPW_VK_Profiler_Queue_EndSession_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_EndSession", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_EndSession);
    g_api.fn.NVPW_VK_Queue_ServicePendingGpuOperations = (NVPW_VK_Queue_ServicePendingGpuOperations_Fn)GetNvPerfProc("NVPW_VK_Queue_ServicePendingGpuOperations", (NVPA_GenericFn)g_api.fn.NVPW_VK_Queue_ServicePendingGpuOperations);
    g_api.fn.NVPW_VK_Profiler_Queue_SetConfig = (NVPW_VK_Profiler_Queue_SetConfig_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_SetConfig", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_SetConfig);
    g_api.fn.NVPW_VK_Profiler_Queue_ClearConfig = (NVPW_VK_Profiler_Queue_ClearConfig_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_ClearConfig", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_ClearConfig);
    g_api.fn.NVPW_VK_Profiler_Queue_BeginPass = (NVPW_VK_Profiler_Queue_BeginPass_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_BeginPass", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_BeginPass);
    g_api.fn.NVPW_VK_Profiler_Queue_EndPass = (NVPW_VK_Profiler_Queue_EndPass_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_EndPass", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_EndPass);
    g_api.fn.NVPW_VK_Profiler_CommandBuffer_PushRange = (NVPW_VK_Profiler_CommandBuffer_PushRange_Fn)GetNvPerfProc("NVPW_VK_Profiler_CommandBuffer_PushRange", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CommandBuffer_PushRange);
    g_api.fn.NVPW_VK_Profiler_CommandBuffer_PopRange = (NVPW_VK_Profiler_CommandBuffer_PopRange_Fn)GetNvPerfProc("NVPW_VK_Profiler_CommandBuffer_PopRange", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_CommandBuffer_PopRange);
    g_api.fn.NVPW_VK_Profiler_Queue_DecodeCounters = (NVPW_VK_Profiler_Queue_DecodeCounters_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_DecodeCounters", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_DecodeCounters);
    g_api.fn.NVPW_VK_Profiler_IsGpuSupported = (NVPW_VK_Profiler_IsGpuSupported_Fn)GetNvPerfProc("NVPW_VK_Profiler_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_IsGpuSupported);
    g_api.fn.NVPW_VK_Profiler_Queue_GetCounterAvailability = (NVPW_VK_Profiler_Queue_GetCounterAvailability_Fn)GetNvPerfProc("NVPW_VK_Profiler_Queue_GetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_VK_Profiler_Queue_GetCounterAvailability);
    g_api.fn.NVPW_VK_MiniTrace_IsGpuSupported = (NVPW_VK_MiniTrace_IsGpuSupported_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_IsGpuSupported);
    g_api.fn.NVPW_VK_MiniTrace_DeviceState_Create = (NVPW_VK_MiniTrace_DeviceState_Create_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_DeviceState_Create", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_DeviceState_Create);
    g_api.fn.NVPW_VK_MiniTrace_DeviceState_Destroy = (NVPW_VK_MiniTrace_DeviceState_Destroy_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_DeviceState_Destroy", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_DeviceState_Destroy);
    g_api.fn.NVPW_VK_MiniTrace_Queue_Register = (NVPW_VK_MiniTrace_Queue_Register_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_Queue_Register", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_Queue_Register);
    g_api.fn.NVPW_VK_MiniTrace_Queue_Unregister = (NVPW_VK_MiniTrace_Queue_Unregister_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_Queue_Unregister", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_Queue_Unregister);
    g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger = (NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger);
    g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu = (NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu);
    g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp = (NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Fn)GetNvPerfProc("NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp", (NVPA_GenericFn)g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp);
    g_api.fn.NVPW_OpenGL_RawMetricsConfig_Create = (NVPW_OpenGL_RawMetricsConfig_Create_Fn)GetNvPerfProc("NVPW_OpenGL_RawMetricsConfig_Create", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_RawMetricsConfig_Create);
    g_api.fn.NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize = (NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize);
    g_api.fn.NVPW_OpenGL_MetricsEvaluator_Initialize = (NVPW_OpenGL_MetricsEvaluator_Initialize_Fn)GetNvPerfProc("NVPW_OpenGL_MetricsEvaluator_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_MetricsEvaluator_Initialize);
    g_api.fn.NVPW_OpenGL_LoadDriver = (NVPW_OpenGL_LoadDriver_Fn)GetNvPerfProc("NVPW_OpenGL_LoadDriver", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_LoadDriver);
    g_api.fn.NVPW_OpenGL_GetCurrentGraphicsContext = (NVPW_OpenGL_GetCurrentGraphicsContext_Fn)GetNvPerfProc("NVPW_OpenGL_GetCurrentGraphicsContext", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_GetCurrentGraphicsContext);
    g_api.fn.NVPW_OpenGL_GraphicsContext_GetDeviceIndex = (NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Fn)GetNvPerfProc("NVPW_OpenGL_GraphicsContext_GetDeviceIndex", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_GraphicsContext_GetDeviceIndex);
    g_api.fn.NVPW_OpenGL_Profiler_IsGpuSupported = (NVPW_OpenGL_Profiler_IsGpuSupported_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_IsGpuSupported", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_IsGpuSupported);
    g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize = (NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize);
    g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_Initialize = (NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_CounterDataImage_Initialize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_Initialize);
    g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize = (NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize);
    g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer = (NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer);
    g_api.fn.NVPW_OpenGL_Profiler_CalcTraceBufferSize = (NVPW_OpenGL_Profiler_CalcTraceBufferSize_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_CalcTraceBufferSize", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_CalcTraceBufferSize);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginSession = (NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_BeginSession", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginSession);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndSession = (NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_EndSession", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndSession);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_SetConfig = (NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_SetConfig", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_SetConfig);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig = (NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginPass = (NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_BeginPass", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginPass);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndPass = (NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_EndPass", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndPass);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PushRange = (NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_PushRange", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PushRange);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PopRange = (NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_PopRange", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PopRange);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters = (NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters);
    g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability = (NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Fn)GetNvPerfProc("NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability", (NVPA_GenericFn)g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability);
}
static NVPA_Status NVPW_InitializeHost_Default(NVPW_InitializeHost_Params* pParams)
{
    InitNvPerf();
    if (g_api.fn.NVPW_InitializeHost != &NVPW_InitializeHost_Default && g_api.fn.NVPW_InitializeHost != &NVPW_InitializeHost)
    {
        return g_api.fn.NVPW_InitializeHost(pParams);
    }
    return g_defaultStatus;
}
static NVPA_Status NVPW_InitializeTarget_Default(NVPW_InitializeTarget_Params* pParams)
{
    InitNvPerf();
    if (g_api.fn.NVPW_InitializeTarget != &NVPW_InitializeTarget_Default && g_api.fn.NVPW_InitializeTarget != &NVPW_InitializeTarget)
    {
        return g_api.fn.NVPW_InitializeTarget(pParams);
    }
    return g_defaultStatus;
}
NVPA_GenericFn NVPA_GetProcAddress(const char* pFunctionName)
{
    return g_api.fn.NVPA_GetProcAddress(pFunctionName);
}
NVPA_Status NVPW_SetLibraryLoadPaths(NVPW_SetLibraryLoadPaths_Params* pParams)
{
    return g_api.fn.NVPW_SetLibraryLoadPaths(pParams);
}
NVPA_Status NVPW_SetLibraryLoadPathsW(NVPW_SetLibraryLoadPathsW_Params* pParams)
{
    return g_api.fn.NVPW_SetLibraryLoadPathsW(pParams);
}
NVPA_Status NVPW_InitializeHost(NVPW_InitializeHost_Params* pParams)
{
    return g_api.fn.NVPW_InitializeHost(pParams);
}
NVPA_Status NVPW_CounterData_CalculateCounterDataImageCopySize(NVPW_CounterData_CalculateCounterDataImageCopySize_Params* pParams)
{
    return g_api.fn.NVPW_CounterData_CalculateCounterDataImageCopySize(pParams);
}
NVPA_Status NVPW_CounterData_InitializeCounterDataImageCopy(NVPW_CounterData_InitializeCounterDataImageCopy_Params* pParams)
{
    return g_api.fn.NVPW_CounterData_InitializeCounterDataImageCopy(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_Create(NVPW_CounterDataCombiner_Create_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_Create(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_Destroy(NVPW_CounterDataCombiner_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_Destroy(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_CreateRange(NVPW_CounterDataCombiner_CreateRange_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_CreateRange(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_CopyIntoRange(NVPW_CounterDataCombiner_CopyIntoRange_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_CopyIntoRange(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_AccumulateIntoRange(NVPW_CounterDataCombiner_AccumulateIntoRange_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_AccumulateIntoRange(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_SumIntoRange(NVPW_CounterDataCombiner_SumIntoRange_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_SumIntoRange(pParams);
}
NVPA_Status NVPW_CounterDataCombiner_WeightedSumIntoRange(NVPW_CounterDataCombiner_WeightedSumIntoRange_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataCombiner_WeightedSumIntoRange(pParams);
}
NVPA_Status NVPW_GetSupportedChipNames(NVPW_GetSupportedChipNames_Params* pParams)
{
    return g_api.fn.NVPW_GetSupportedChipNames(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_Destroy(NVPW_RawMetricsConfig_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_Destroy(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_SetCounterAvailability(NVPW_RawMetricsConfig_SetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_SetCounterAvailability(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_BeginPassGroup(NVPW_RawMetricsConfig_BeginPassGroup_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_BeginPassGroup(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_EndPassGroup(NVPW_RawMetricsConfig_EndPassGroup_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_EndPassGroup(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_GetNumMetrics(NVPW_RawMetricsConfig_GetNumMetrics_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_GetNumMetrics(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_GetMetricProperties_V2(NVPW_RawMetricsConfig_GetMetricProperties_V2_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_GetMetricProperties_V2(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_AddMetrics(NVPW_RawMetricsConfig_AddMetrics_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_AddMetrics(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_IsAddMetricsPossible(NVPW_RawMetricsConfig_IsAddMetricsPossible_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_IsAddMetricsPossible(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_GenerateConfigImage(NVPW_RawMetricsConfig_GenerateConfigImage_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_GenerateConfigImage(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_GetConfigImage(NVPW_RawMetricsConfig_GetConfigImage_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_GetConfigImage(pParams);
}
NVPA_Status NVPW_RawMetricsConfig_GetNumPasses_V2(NVPW_RawMetricsConfig_GetNumPasses_V2_Params* pParams)
{
    return g_api.fn.NVPW_RawMetricsConfig_GetNumPasses_V2(pParams);
}
NVPA_Status NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize(NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize(pParams);
}
NVPA_Status NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize(NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize(pParams);
}
NVPA_Status NVPW_CounterDataBuilder_Create(NVPW_CounterDataBuilder_Create_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataBuilder_Create(pParams);
}
NVPA_Status NVPW_CounterDataBuilder_Destroy(NVPW_CounterDataBuilder_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataBuilder_Destroy(pParams);
}
NVPA_Status NVPW_CounterDataBuilder_AddMetrics(NVPW_CounterDataBuilder_AddMetrics_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataBuilder_AddMetrics(pParams);
}
NVPA_Status NVPW_CounterDataBuilder_GetCounterDataPrefix(NVPW_CounterDataBuilder_GetCounterDataPrefix_Params* pParams)
{
    return g_api.fn.NVPW_CounterDataBuilder_GetCounterDataPrefix(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_Destroy(NVPW_MetricsEvaluator_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_Destroy(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetMetricNames(NVPW_MetricsEvaluator_GetMetricNames_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetMetricNames(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetMetricTypeAndIndex(NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetMetricTypeAndIndex(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_HwUnitToString(NVPW_MetricsEvaluator_HwUnitToString_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_HwUnitToString(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetCounterProperties(NVPW_MetricsEvaluator_GetCounterProperties_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetCounterProperties(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetRatioMetricProperties(NVPW_MetricsEvaluator_GetRatioMetricProperties_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetRatioMetricProperties(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetThroughputMetricProperties(NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetThroughputMetricProperties(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetSupportedSubmetrics(NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetSupportedSubmetrics(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetMetricRawDependencies(NVPW_MetricsEvaluator_GetMetricRawDependencies_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetMetricRawDependencies(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_DimUnitToString(NVPW_MetricsEvaluator_DimUnitToString_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_DimUnitToString(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_GetMetricDimUnits(NVPW_MetricsEvaluator_GetMetricDimUnits_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_GetMetricDimUnits(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_SetUserData(NVPW_MetricsEvaluator_SetUserData_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_SetUserData(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_EvaluateToGpuValues(NVPW_MetricsEvaluator_EvaluateToGpuValues_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_EvaluateToGpuValues(pParams);
}
NVPA_Status NVPW_MetricsEvaluator_SetDeviceAttributes(NVPW_MetricsEvaluator_SetDeviceAttributes_Params* pParams)
{
    return g_api.fn.NVPW_MetricsEvaluator_SetDeviceAttributes(pParams);
}
NVPA_Status NVPW_InitializeTarget(NVPW_InitializeTarget_Params* pParams)
{
    return g_api.fn.NVPW_InitializeTarget(pParams);
}
NVPA_Status NVPW_GetDeviceCount(NVPW_GetDeviceCount_Params* pParams)
{
    return g_api.fn.NVPW_GetDeviceCount(pParams);
}
NVPA_Status NVPW_Device_GetNames(NVPW_Device_GetNames_Params* pParams)
{
    return g_api.fn.NVPW_Device_GetNames(pParams);
}
NVPA_Status NVPW_Device_GetPciBusIds(NVPW_Device_GetPciBusIds_Params* pParams)
{
    return g_api.fn.NVPW_Device_GetPciBusIds(pParams);
}
NVPA_Status NVPW_Device_GetMigAttributes(NVPW_Device_GetMigAttributes_Params* pParams)
{
    return g_api.fn.NVPW_Device_GetMigAttributes(pParams);
}
NVPA_Status NVPW_Adapter_GetDeviceIndex(NVPW_Adapter_GetDeviceIndex_Params* pParams)
{
    return g_api.fn.NVPW_Adapter_GetDeviceIndex(pParams);
}
NVPA_Status NVPW_CounterData_GetNumRanges(NVPW_CounterData_GetNumRanges_Params* pParams)
{
    return g_api.fn.NVPW_CounterData_GetNumRanges(pParams);
}
NVPA_Status NVPW_CounterData_GetChipName(NVPW_CounterData_GetChipName_Params* pParams)
{
    return g_api.fn.NVPW_CounterData_GetChipName(pParams);
}
NVPA_Status NVPW_Config_GetNumPasses_V2(NVPW_Config_GetNumPasses_V2_Params* pParams)
{
    return g_api.fn.NVPW_Config_GetNumPasses_V2(pParams);
}
NVPA_Status NVPW_QueryVersionNumber(NVPW_QueryVersionNumber_Params* pParams)
{
    return g_api.fn.NVPW_QueryVersionNumber(pParams);
}
NVPA_Status NVPW_Device_GetClockStatus(NVPW_Device_GetClockStatus_Params* pParams)
{
    return g_api.fn.NVPW_Device_GetClockStatus(pParams);
}
NVPA_Status NVPW_Device_SetClockSetting(NVPW_Device_SetClockSetting_Params* pParams)
{
    return g_api.fn.NVPW_Device_SetClockSetting(pParams);
}
NVPA_Status NVPW_CounterData_GetRangeDescriptions(NVPW_CounterData_GetRangeDescriptions_Params* pParams)
{
    return g_api.fn.NVPW_CounterData_GetRangeDescriptions(pParams);
}
NVPA_Status NVPW_Profiler_CounterData_GetRangeDescriptions(NVPW_Profiler_CounterData_GetRangeDescriptions_Params* pParams)
{
    return g_api.fn.NVPW_Profiler_CounterData_GetRangeDescriptions(pParams);
}
NVPA_Status NVPW_PeriodicSampler_CounterData_GetSampleTime(NVPW_PeriodicSampler_CounterData_GetSampleTime_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_CounterData_GetSampleTime(pParams);
}
NVPA_Status NVPW_PeriodicSampler_CounterData_TrimInPlace(NVPW_PeriodicSampler_CounterData_TrimInPlace_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_CounterData_TrimInPlace(pParams);
}
NVPA_Status NVPW_PeriodicSampler_CounterData_GetInfo(NVPW_PeriodicSampler_CounterData_GetInfo_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_CounterData_GetInfo(pParams);
}
NVPA_Status NVPW_PeriodicSampler_CounterData_GetTriggerCount(NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params* pParams)
{
    return g_api.fn.NVPW_PeriodicSampler_CounterData_GetTriggerCount(pParams);
}
NVPA_Status NVPW_D3D12_RawMetricsConfig_Create(NVPW_D3D12_RawMetricsConfig_Create_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_RawMetricsConfig_Create(pParams);
}
NVPA_Status NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize(NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_D3D12_MetricsEvaluator_Initialize(NVPW_D3D12_MetricsEvaluator_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MetricsEvaluator_Initialize(pParams);
}
NVPA_Status NVPW_D3D12_LoadDriver(NVPW_D3D12_LoadDriver_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_LoadDriver(pParams);
}
NVPA_Status NVPW_D3D12_Device_GetDeviceIndex(NVPW_D3D12_Device_GetDeviceIndex_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Device_GetDeviceIndex(pParams);
}
NVPA_Status NVPW_D3D12_GetLUID(NVPW_D3D12_GetLUID_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_GetLUID(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_CalculateSize(NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateSize(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_Initialize(NVPW_D3D12_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_Initialize(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CalcTraceBufferSize(NVPW_D3D12_Profiler_CalcTraceBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CalcTraceBufferSize(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_BeginSession(NVPW_D3D12_Profiler_Queue_BeginSession_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_BeginSession(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_EndSession(NVPW_D3D12_Profiler_Queue_EndSession_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_EndSession(pParams);
}
NVPA_Status NVPW_D3D12_Queue_ServicePendingGpuOperations(NVPW_D3D12_Queue_ServicePendingGpuOperations_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Queue_ServicePendingGpuOperations(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_SetConfig(NVPW_D3D12_Profiler_Queue_SetConfig_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_SetConfig(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_ClearConfig(NVPW_D3D12_Profiler_Queue_ClearConfig_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_ClearConfig(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_BeginPass(NVPW_D3D12_Profiler_Queue_BeginPass_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_BeginPass(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_EndPass(NVPW_D3D12_Profiler_Queue_EndPass_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_EndPass(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_PushRange(NVPW_D3D12_Profiler_Queue_PushRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_PushRange(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_PopRange(NVPW_D3D12_Profiler_Queue_PopRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_PopRange(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CommandList_PushRange(NVPW_D3D12_Profiler_CommandList_PushRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CommandList_PushRange(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_CommandList_PopRange(NVPW_D3D12_Profiler_CommandList_PopRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_CommandList_PopRange(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_DecodeCounters(NVPW_D3D12_Profiler_Queue_DecodeCounters_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_DecodeCounters(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_Queue_GetCounterAvailability(NVPW_D3D12_Profiler_Queue_GetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_Queue_GetCounterAvailability(pParams);
}
NVPA_Status NVPW_D3D12_Profiler_IsGpuSupported(NVPW_D3D12_Profiler_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_Profiler_IsGpuSupported(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_IsGpuSupported(NVPW_D3D12_MiniTrace_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_IsGpuSupported(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_DeviceState_Create(NVPW_D3D12_MiniTrace_DeviceState_Create_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Create(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_DeviceState_Destroy(NVPW_D3D12_MiniTrace_DeviceState_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_DeviceState_Destroy(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_Queue_Register(NVPW_D3D12_MiniTrace_Queue_Register_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_Queue_Register(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_Queue_Unregister(NVPW_D3D12_MiniTrace_Queue_Unregister_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_Queue_Unregister(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger(NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_CommandList_MarkerCpu(NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_CommandList_MarkerCpu(pParams);
}
NVPA_Status NVPW_D3D12_MiniTrace_CommandList_HostTimestamp(NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Params* pParams)
{
    return g_api.fn.NVPW_D3D12_MiniTrace_CommandList_HostTimestamp(pParams);
}
NVPA_Status NVPW_D3D11_RawMetricsConfig_Create(NVPW_D3D11_RawMetricsConfig_Create_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_RawMetricsConfig_Create(pParams);
}
NVPA_Status NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize(NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_D3D11_MetricsEvaluator_Initialize(NVPW_D3D11_MetricsEvaluator_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_MetricsEvaluator_Initialize(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateSize(NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateSize(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_Initialize(NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_Initialize(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer(pParams);
}
NVPA_Status NVPW_D3D11_LoadDriver(NVPW_D3D11_LoadDriver_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_LoadDriver(pParams);
}
NVPA_Status NVPW_D3D11_GetLUID(NVPW_D3D11_GetLUID_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_GetLUID(pParams);
}
NVPA_Status NVPW_D3D11_Device_GetDeviceIndex(NVPW_D3D11_Device_GetDeviceIndex_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Device_GetDeviceIndex(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_CalcTraceBufferSize(NVPW_D3D11_Profiler_CalcTraceBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_CalcTraceBufferSize(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginSession(NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginSession(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndSession(NVPW_D3D11_Profiler_DeviceContext_EndSession_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndSession(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_SetConfig(NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_SetConfig(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_ClearConfig(NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_ClearConfig(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginPass(NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_BeginPass(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndPass(NVPW_D3D11_Profiler_DeviceContext_EndPass_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_EndPass(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PushRange(NVPW_D3D11_Profiler_DeviceContext_PushRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PushRange(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PopRange(NVPW_D3D11_Profiler_DeviceContext_PopRange_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_PopRange(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_DecodeCounters(NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_DecodeCounters(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_IsGpuSupported(NVPW_D3D11_Profiler_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_IsGpuSupported(pParams);
}
NVPA_Status NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability(NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability(pParams);
}
NVPA_Status NVPW_Device_RawMetricsConfig_Create(NVPW_Device_RawMetricsConfig_Create_Params* pParams)
{
    return g_api.fn.NVPW_Device_RawMetricsConfig_Create(pParams);
}
NVPA_Status NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize(NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_Device_MetricsEvaluator_Initialize(NVPW_Device_MetricsEvaluator_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_Device_MetricsEvaluator_Initialize(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_IsGpuSupported(NVPW_GPU_PeriodicSampler_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_IsGpuSupported(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources(NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize(NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession(NVPW_GPU_PeriodicSampler_BeginSession_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession_V2(NVPW_GPU_PeriodicSampler_BeginSession_V2_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_BeginSession_V2(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_EndSession(NVPW_GPU_PeriodicSampler_EndSession_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_EndSession(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_GetCounterAvailability(NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_GetCounterAvailability(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_SetConfig(NVPW_GPU_PeriodicSampler_SetConfig_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_SetConfig(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_StartSampling(NVPW_GPU_PeriodicSampler_StartSampling_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_StartSampling(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_StopSampling(NVPW_GPU_PeriodicSampler_StopSampling_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_StopSampling(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_CpuTrigger(NVPW_GPU_PeriodicSampler_CpuTrigger_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_CpuTrigger(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize(NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize(NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_GetRecordBufferStatus(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_GetRecordBufferStatus(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters(NVPW_GPU_PeriodicSampler_DecodeCounters_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters_V2(NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_DecodeCounters_V2(pParams);
}
NVPA_Status NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported(NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params* pParams)
{
    return g_api.fn.NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported(pParams);
}
NVPA_Status NVPW_VK_RawMetricsConfig_Create(NVPW_VK_RawMetricsConfig_Create_Params* pParams)
{
    return g_api.fn.NVPW_VK_RawMetricsConfig_Create(pParams);
}
NVPA_Status NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize(NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_VK_MetricsEvaluator_Initialize(NVPW_VK_MetricsEvaluator_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_VK_MetricsEvaluator_Initialize(pParams);
}
NVPA_Status NVPW_VK_Profiler_CounterDataImage_CalculateSize(NVPW_VK_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateSize(pParams);
}
NVPA_Status NVPW_VK_Profiler_CounterDataImage_Initialize(NVPW_VK_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CounterDataImage_Initialize(pParams);
}
NVPA_Status NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer(pParams);
}
NVPA_Status NVPW_VK_LoadDriver(NVPW_VK_LoadDriver_Params* pParams)
{
    return g_api.fn.NVPW_VK_LoadDriver(pParams);
}
NVPA_Status NVPW_VK_Device_GetDeviceIndex(NVPW_VK_Device_GetDeviceIndex_Params* pParams)
{
    return g_api.fn.NVPW_VK_Device_GetDeviceIndex(pParams);
}
NVPA_Status NVPW_VK_Profiler_GetRequiredInstanceExtensions(NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_GetRequiredInstanceExtensions(pParams);
}
NVPA_Status NVPW_VK_Profiler_GetRequiredDeviceExtensions(NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_GetRequiredDeviceExtensions(pParams);
}
NVPA_Status NVPW_VK_Profiler_CalcTraceBufferSize(NVPW_VK_Profiler_CalcTraceBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CalcTraceBufferSize(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_BeginSession(NVPW_VK_Profiler_Queue_BeginSession_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_BeginSession(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_EndSession(NVPW_VK_Profiler_Queue_EndSession_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_EndSession(pParams);
}
NVPA_Status NVPW_VK_Queue_ServicePendingGpuOperations(NVPW_VK_Queue_ServicePendingGpuOperations_Params* pParams)
{
    return g_api.fn.NVPW_VK_Queue_ServicePendingGpuOperations(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_SetConfig(NVPW_VK_Profiler_Queue_SetConfig_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_SetConfig(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_ClearConfig(NVPW_VK_Profiler_Queue_ClearConfig_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_ClearConfig(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_BeginPass(NVPW_VK_Profiler_Queue_BeginPass_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_BeginPass(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_EndPass(NVPW_VK_Profiler_Queue_EndPass_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_EndPass(pParams);
}
NVPA_Status NVPW_VK_Profiler_CommandBuffer_PushRange(NVPW_VK_Profiler_CommandBuffer_PushRange_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CommandBuffer_PushRange(pParams);
}
NVPA_Status NVPW_VK_Profiler_CommandBuffer_PopRange(NVPW_VK_Profiler_CommandBuffer_PopRange_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_CommandBuffer_PopRange(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_DecodeCounters(NVPW_VK_Profiler_Queue_DecodeCounters_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_DecodeCounters(pParams);
}
NVPA_Status NVPW_VK_Profiler_IsGpuSupported(NVPW_VK_Profiler_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_IsGpuSupported(pParams);
}
NVPA_Status NVPW_VK_Profiler_Queue_GetCounterAvailability(NVPW_VK_Profiler_Queue_GetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_VK_Profiler_Queue_GetCounterAvailability(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_IsGpuSupported(NVPW_VK_MiniTrace_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_IsGpuSupported(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_DeviceState_Create(NVPW_VK_MiniTrace_DeviceState_Create_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_DeviceState_Create(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_DeviceState_Destroy(NVPW_VK_MiniTrace_DeviceState_Destroy_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_DeviceState_Destroy(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_Queue_Register(NVPW_VK_MiniTrace_Queue_Register_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_Queue_Register(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_Queue_Unregister(NVPW_VK_MiniTrace_Queue_Unregister_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_Queue_Unregister(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger(NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_FrontEndTrigger(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu(NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_MarkerCpu(pParams);
}
NVPA_Status NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp(NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp_Params* pParams)
{
    return g_api.fn.NVPW_VK_MiniTrace_CommandBuffer_HostTimestamp(pParams);
}
NVPA_Status NVPW_OpenGL_RawMetricsConfig_Create(NVPW_OpenGL_RawMetricsConfig_Create_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_RawMetricsConfig_Create(pParams);
}
NVPA_Status NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize(NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_OpenGL_MetricsEvaluator_Initialize(NVPW_OpenGL_MetricsEvaluator_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_MetricsEvaluator_Initialize(pParams);
}
NVPA_Status NVPW_OpenGL_LoadDriver(NVPW_OpenGL_LoadDriver_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_LoadDriver(pParams);
}
NVPA_Status NVPW_OpenGL_GetCurrentGraphicsContext(NVPW_OpenGL_GetCurrentGraphicsContext_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_GetCurrentGraphicsContext(pParams);
}
NVPA_Status NVPW_OpenGL_GraphicsContext_GetDeviceIndex(NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_GraphicsContext_GetDeviceIndex(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_IsGpuSupported(NVPW_OpenGL_Profiler_IsGpuSupported_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_IsGpuSupported(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize(NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_Initialize(NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_Initialize(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_CalcTraceBufferSize(NVPW_OpenGL_Profiler_CalcTraceBufferSize_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_CalcTraceBufferSize(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_BeginSession(NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginSession(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_EndSession(NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndSession(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_SetConfig(NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_SetConfig(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig(NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_ClearConfig(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_BeginPass(NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_BeginPass(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_EndPass(NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_EndPass(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_PushRange(NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PushRange(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_PopRange(NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_PopRange(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters(NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters(pParams);
}
NVPA_Status NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability(NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability_Params* pParams)
{
    return g_api.fn.NVPW_OpenGL_Profiler_GraphicsContext_GetCounterAvailability(pParams);
}
static void* LibOpen(wchar_t const* name)
{
    return (void*)LoadLibraryW(name);
}

static NVPA_GenericFn LibSym(void* module, char const* name)
{
    return (NVPA_GenericFn)GetProcAddress((HMODULE)module, name);
}

static size_t GetModuleDirectory(HMODULE hModule, wchar_t* dir, size_t dirLen)
{
    wchar_t dllPath[MAX_PATH] = {0};
    DWORD result = GetModuleFileNameW(hModule, dllPath, MAX_PATH);
    if (result == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        return 0;
    }

    const size_t dllPathLength = wcslen(dllPath);
    if (!dllPathLength)
    {
        return 0;
    }

    for (ptrdiff_t u = dllPathLength - 1; u >= 0; --u)
    {
        if (dllPath[u] == L'\\' || dllPath[u] == L'/')
        {
            dllPath[u] = 0;
            wcsncpy_s(dir, dirLen, dllPath, u + 1);
            return u;
        }
    }

    /* GetModuleFileNameW returns a fully qualified path.  If we reached here, something went wrong. */
    return 0;
}

#pragma warning(push)
#pragma warning(disable:4054) /* function-pointer/data-pointer conversion */
static size_t GetCurrentModuleDirectory(wchar_t* dir, size_t dirLen)
{
    dir[0] = 0;

    HMODULE hModule = 0;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&GetCurrentModuleDirectory,
        &hModule);
    if (!hModule)
    {
        return 0;
    }

    return GetModuleDirectory(hModule, dir, dirLen);
}
#pragma warning(pop)
static void* LoadNvPerfLibrary(void)
{
#define LIB_NAME_LEN  (2 * MAX_PATH)
    wchar_t const* const pLibName = L"nvperf_grfx_host.dll";

    if (g_api.numSearchPaths == 0)
    {
        /* Load from default paths */
        void* hNvPerf = LibOpen(pLibName);
        if (hNvPerf)
        {
            return hNvPerf;
        }

        /* if we are on Windows, also try to load from the path of the current module */
        wchar_t libNameW[LIB_NAME_LEN];
        size_t strIndex = GetCurrentModuleDirectory(libNameW, LIB_NAME_LEN);
        /* strIndex = 0 when not find hModule */
        if (strIndex == 0)
        {
            return 0;
        }
        libNameW[strIndex++] = L'\\';
        wcsncpy_s(libNameW + strIndex, LIB_NAME_LEN - strIndex, pLibName, LIB_NAME_LEN - strIndex);

        hNvPerf = LoadLibraryW(libNameW);
        if (hNvPerf)
        {
            return hNvPerf;
        }
    }
    else
    {
        size_t pathIndex;
        for (pathIndex = 0; pathIndex < g_api.numSearchPaths; ++pathIndex)
        {
            if (!g_api.ppSearchPaths[pathIndex])
            {
                continue;
            }

            wchar_t pLibFullName[LIB_NAME_LEN + 1];
            size_t strIndex = wcslen(g_api.ppSearchPaths[pathIndex]);
            wcsncpy_s(pLibFullName, LIB_NAME_LEN, g_api.ppSearchPaths[pathIndex], LIB_NAME_LEN);
            pLibFullName[strIndex++] = L'\\';
            wcsncpy_s(pLibFullName + strIndex, LIB_NAME_LEN - strIndex, pLibName, LIB_NAME_LEN - strIndex);

            void* hNvPerf = LibOpen(pLibFullName);
            if (hNvPerf)
            {
                return hNvPerf;
            }
        }
    }
#undef LIB_NAME_LEN
    return 0;
}

/* Returns 0 on failure, 1 on success. */
static int InitNvPerf(void)
{
    if (!g_api.hModNvPerf)
    {
        g_api.hModNvPerf = LoadNvPerfLibrary();
        if (!g_api.hModNvPerf)
        {
            return 0;
        }
    }

    g_defaultStatus = NVPA_STATUS_FUNCTION_NOT_FOUND;
    g_api.nvPerfGetProcAddress = (NVPA_GetProcAddress_Fn)LibSym(g_api.hModNvPerf, "NVPA_GetProcAddress");
    if (!g_api.nvPerfGetProcAddress)
    {
        return 0;
    }
    

    InitNvPerfProcs();
    return 1;
}
static NVPA_GenericFn GetNvPerfProc(char const* pName, NVPA_GenericFn pDefault)
{
    NVPA_GenericFn pProc = g_api.nvPerfGetProcAddress(pName);
    if (pProc)
    {
        return pProc;
    }
    return pDefault;
}

static void FreeSearchPaths(void)
{
    if (g_api.ppSearchPaths)
    {
        size_t index;
        for (index = 0; index < g_api.numSearchPaths; ++index)
        {
            free(g_api.ppSearchPaths[index]);
        }
        free(g_api.ppSearchPaths);
        g_api.ppSearchPaths = NULL;
        g_api.numSearchPaths = 0;
    }
}

static NVPA_Status NVPW_SetLibraryLoadPaths_Default(NVPW_SetLibraryLoadPaths_Params* pParams)
{
    const size_t MaxLibPathLength = 4096;
    size_t index;

    /* free the old paths */
    FreeSearchPaths();

    if (pParams->numPaths == 0 || pParams->ppPaths == NULL)
    {
        return NVPA_STATUS_SUCCESS;
    }

    #ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable : 6385 )
    #endif

    g_api.numSearchPaths = pParams->numPaths;
    g_api.ppSearchPaths = (NVPW_User_PathCharType**)malloc(pParams->numPaths * sizeof(NVPW_User_PathCharType*));
    if (!g_api.ppSearchPaths)
    {
        return NVPA_STATUS_OUT_OF_MEMORY;
    }
    memset(g_api.ppSearchPaths, 0, pParams->numPaths * sizeof(NVPW_User_PathCharType*));

    for (index = 0; index < pParams->numPaths; ++index)
    {
        const void* pEnd = memchr(pParams->ppPaths[index], '\0', MaxLibPathLength);
        if (!pEnd)
        {
            return NVPA_STATUS_INVALID_ARGUMENT;
        }
        const size_t len = (const char*)pEnd - pParams->ppPaths[index] + 1;

        g_api.ppSearchPaths[index] = (NVPW_User_PathCharType*)malloc((len) * sizeof(NVPW_User_PathCharType));
        if (!g_api.ppSearchPaths[index])
        {
            return NVPA_STATUS_OUT_OF_MEMORY;
        }
#if defined(_WIN32)
        size_t numConverted;
        mbstowcs_s(&numConverted, g_api.ppSearchPaths[index], len, pParams->ppPaths[index], len);
#else
        strncpy(g_api.ppSearchPaths[index], pParams->ppPaths[index], len);
#endif
    }

    #ifdef _MSC_VER
    #pragma warning( pop )
    #endif

    return NVPA_STATUS_SUCCESS;
}

static NVPA_Status NVPW_SetLibraryLoadPathsW_Default(NVPW_SetLibraryLoadPathsW_Params* pParams)
{
    size_t index;

    /* free the old paths */
    FreeSearchPaths();

    if (pParams->numPaths == 0 || pParams->ppwPaths == NULL)
    {
        return NVPA_STATUS_SUCCESS;
    }

    #ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable : 6385 )
    #endif

    g_api.numSearchPaths = pParams->numPaths;
    g_api.ppSearchPaths = (NVPW_User_PathCharType**)malloc(pParams->numPaths * sizeof(NVPW_User_PathCharType*));
    if (!g_api.ppSearchPaths)
    {
        return NVPA_STATUS_OUT_OF_MEMORY;
    }
    memset(g_api.ppSearchPaths, 0, pParams->numPaths * sizeof(NVPW_User_PathCharType*));

    for (index = 0; index < pParams->numPaths; ++index)
    {
        /* calculate the length of the dest */
#if defined(_WIN32)
        size_t len = wcslen(pParams->ppwPaths[index]) + 1;
#else
        size_t len = wcstombs(NULL, pParams->ppwPaths[index], 0) + 1;
#endif
        /* allocate the dest buffer */
        g_api.ppSearchPaths[index] = (NVPW_User_PathCharType*)malloc((len) * sizeof(NVPW_User_PathCharType));
        if (!g_api.ppSearchPaths[index])
        {
            return NVPA_STATUS_OUT_OF_MEMORY;
        }
        /* copy/convert the source to dest */
#if defined(_WIN32)
        wcsncpy_s(g_api.ppSearchPaths[index], len, pParams->ppwPaths[index], len);
#else
        wcstombs(g_api.ppSearchPaths[index], pParams->ppwPaths[index], len);
#endif
    }

    #ifdef _MSC_VER
    #pragma warning( pop )
    #endif

    return NVPA_STATUS_SUCCESS;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
