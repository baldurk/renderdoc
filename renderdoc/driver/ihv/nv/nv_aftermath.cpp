/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "nv_aftermath.h"

#include <unordered_map>
#include "common/formatting.h"
#include "common/threading.h"
#include "core/plugins.h"
#include "core/settings.h"

RDOC_CONFIG(bool, Replay_Debug_EnableAftermath, false,
            "Enable nvidia Aftermath for diagnosing GPU crashes or failures on D3D12 and Vulkan.");

RDOC_CONFIG(bool, Replay_Debug_EnableNVRTValidation, false,
            "Enable nvidia Raytracing validation on D3D12 and Vulkan.");

#include "driver/ihv/nv/official/aftermath/GFSDK_Aftermath.h"
#include "driver/ihv/nv/official/aftermath/GFSDK_Aftermath_GpuCrashDump.h"
#include "driver/ihv/nv/official/aftermath/GFSDK_Aftermath_GpuCrashDumpDecoding.h"

// assume this macro is only available if we got the real headers
#if defined(GFSDK_AFTERMATH_CALL) && ENABLED(RDOC_WIN32)

#include "official/nvapi/nvapi.h"

#include <windows.h>

#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi.h"
#include "driver/vulkan/official/vulkan_core.h"

namespace
{
#include "official/nvapi/nvapi_interface.h"

uint32_t getId(const char *name)
{
  // slow lookup, we only check a couple of functions
  for(NVAPI_INTERFACE_TABLE &table : nvapi_interface_table)
    if(!strcmp(table.func, name))
      return table.id;

  RDCERR("Couldn't get function ID for %s", name);

  return 0;
}
};

typedef void *(*PFN_nvapi_QueryInterface)(NvU32 id);

namespace
{
#define AFTERMATH_FUNCS(x) \
  x(DX12_Initialize);      \
  x(DisableGpuCrashDumps); \
  x(EnableGpuCrashDumps);  \
  x(GetCrashDumpStatus);   \
  x(GetShaderDebugInfoIdentifier);

struct aftermath_table
{
#define DECL_FUNC(a) CONCAT(PFN_GFSDK_Aftermath_, a) a;
  AFTERMATH_FUNCS(DECL_FUNC)

#define FETCH_FUNC(a) \
  if(!a)              \
    a = (decltype(a))GetProcAddress(module, "GFSDK_Aftermath_" STRINGIZE(a));

  void init(HMODULE module)
  {
    if(!module)
      return;
    AFTERMATH_FUNCS(FETCH_FUNC);
  }
};

aftermath_table table = {};
uint32_t dumpNumber = 0;

void GpuCrashDumpCallback(const void *pGpuCrashDump, const uint32_t gpuCrashDumpSize, void *)
{
  FileIO::WriteAll(StringFormat::sntimef(Timing::GetUTCTime(), "aftermath_dump-%y%m%d%H%M%S-") +
                       StringFormat::Fmt("%u-%u.nv-gpudmp", Process::GetCurrentPID(), dumpNumber++),
                   pGpuCrashDump, gpuCrashDumpSize);
}

void GpuShaderDebugInfoCallback(const void *pShaderDebugInfo, const uint32_t shaderDebugInfoSize,
                                void *)
{
  RDCLOG("shader info %p %zu", pShaderDebugInfo, shaderDebugInfoSize);

  GFSDK_Aftermath_ShaderDebugInfoIdentifier ident;
  table.GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, pShaderDebugInfo,
                                     shaderDebugInfoSize, &ident);

  FileIO::WriteAll(StringFormat::Fmt("shader-%08x-%08x.nvdbg", ident.id[0], ident.id[1]),
                   pShaderDebugInfo, shaderDebugInfoSize);
}

void DescriptionCB(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void *pUserData)
{
  addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
                 "RenderDoc Aftermath Integration");
  addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion,
                 "v" MAJOR_MINOR_VERSION_STRING);
}

};

void NVAftermath_Init()
{
  if(Replay_Debug_EnableNVRTValidation())
  {
    RDCLOG("Preparing for NV RT validation");
    SetEnvironmentVariableA("NV_ALLOW_RAYTRACING_VALIDATION", "1");
  }

  if(Replay_Debug_EnableAftermath())
  {
#if ENABLED(RDOC_X64)
    rdcstr dllPath = LocatePluginFile("nv/aftermath/", "GFSDK_Aftermath_Lib.x64.dll");
#else
    rdcstr dllPath = LocatePluginFile("nv/aftermath/", "GFSDK_Aftermath_Lib.x86.dll");
#endif

    HMODULE aftermath = (HMODULE)Process::LoadModule(dllPath);

    if(aftermath == NULL)
    {
      RDCWARN("Couldn't load aftermath dll, check it is in nv/aftermath/ plugins folder");
      return;
    }

    table.init(aftermath);

    if(table.EnableGpuCrashDumps)
    {
      GFSDK_Aftermath_Result res = table.EnableGpuCrashDumps(
          GFSDK_Aftermath_Version_API,
          GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX |
              GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
          GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks, GpuCrashDumpCallback,
          &GpuShaderDebugInfoCallback, DescriptionCB, NULL, NULL);

      if(GFSDK_Aftermath_SUCCEED(res))
      {
        RDCLOG("NV Aftermath initialised successfully");
      }
      else
      {
        RDCERR("NV Aftermath failed to initialise: %x", res);
      }
    }
    else
    {
      RDCWARN("NV aftermath unavailable - check out of date drivers");
    }
  }
}

// on shutdown we could call GFSDK_Aftermath_DisableGpuCrashDumps

void NVAftermath_EnableVK(const std::set<rdcstr> &supportedExtensions, rdcarray<rdcstr> &Extensions,
                          const void **deviceCreateNext)
{
  if(Replay_Debug_EnableNVRTValidation())
  {
    if(supportedExtensions.find(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      Extensions.push_back(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME);

      static VkPhysicalDeviceRayTracingValidationFeaturesNV features = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV,
          NULL,
          true,
      };
      features.pNext = (void *)*deviceCreateNext;
      *deviceCreateNext = &features;

      RDCLOG("NV raytracing validation enabled for Vulkan");
    }
    else
    {
      RDCWARN("NV Raytracing validation extension unavailable");
    }
  }

  if(Replay_Debug_EnableAftermath())
  {
    // if we failed to init, silently stop here
    if(!table.GetCrashDumpStatus)
      return;

    if(supportedExtensions.find(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME) !=
           supportedExtensions.end() &&
       supportedExtensions.find(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME) !=
           supportedExtensions.end())
    {
      static VkDeviceDiagnosticsConfigCreateInfoNV features = {
          VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV,
          NULL,
          VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
              VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |
              VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
              VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV,
      };

      features.pNext = *deviceCreateNext;
      *deviceCreateNext = &features;

      Extensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
      Extensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);

      RDCLOG("NV Aftermath enabled for Vulkan");
    }
    else
    {
      RDCWARN("NV Aftermath extensions unavailable");
    }
  }
}

using PFN_NvAPI_D3D12_FlushRaytracingValidationMessages =
    decltype(&::NvAPI_D3D12_FlushRaytracingValidationMessages);

PFN_NvAPI_D3D12_FlushRaytracingValidationMessages NvAPI_FlushRTValidation;

void __stdcall rtValidationCB(void *, NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY severity,
                              const char *messageCode, const char *message,
                              const char *messageDetails)
{
  const char *severityString = "unknown";
  switch(severity)
  {
    case NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_ERROR: severityString = "error"; break;
    case NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_WARNING:
      severityString = "warning";
      break;
  }
  RDCWARN("Ray Tracing Validation message: %s: [%s] %s\n%s", severityString, messageCode, message,
          messageDetails);
}

void NVAftermath_EnableD3D12(ID3D12Device *dev)
{
  if(Replay_Debug_EnableAftermath())
  {
    // if we failed to init, silently stop here
    if(!table.GetCrashDumpStatus)
      return;

    GFSDK_Aftermath_Result res =
        table.DX12_Initialize(GFSDK_Aftermath_Version_API,
                              GFSDK_Aftermath_FeatureFlags_EnableMarkers |
                                  GFSDK_Aftermath_FeatureFlags_EnableResourceTracking |
                                  GFSDK_Aftermath_FeatureFlags_CallStackCapturing |
                                  GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo |
                                  GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting,
                              dev);

    if(GFSDK_Aftermath_SUCCEED(res))
    {
      RDCLOG("NV Aftermath enabled successfully for D3D12");
    }
    else
    {
      RDCERR("NV Aftermath failed to enabled for D3D12: %x", res);
    }
  }

  if(Replay_Debug_EnableNVRTValidation())
  {
#if ENABLED(RDOC_X64)
    HMODULE nvapi = LoadLibraryA("nvapi64.dll");
#else
    HMODULE nvapi = LoadLibraryA("nvapi.dll");
#endif

    if(!nvapi)
    {
      RDCWARN("Couldn't load nvapi.dll to enable NV Raytracing validation");
      return;
    }

    PFN_nvapi_QueryInterface nvapi_QueryInterface =
        (PFN_nvapi_QueryInterface)GetProcAddress(nvapi, "nvapi_QueryInterface");

    if(!nvapi_QueryInterface)
    {
      RDCWARN("Couldn't get nvapi_QueryInterface to enable NV Raytracing validation");
      return;
    }

    using PFN_NvAPI_Initialize = decltype(&::NvAPI_Initialize);

    PFN_NvAPI_Initialize NvAPI_Initialize =
        (PFN_NvAPI_Initialize)nvapi_QueryInterface(getId("NvAPI_Initialize"));

    if(!NvAPI_Initialize)
    {
      RDCWARN("Couldn't get NvAPI_Initialize to enable NV Raytracing validation");
      return;
    }

    NvAPI_Status nvResult = NvAPI_Initialize();

    if(nvResult == NVAPI_OK)
    {
      using PFN_NvAPI_D3D12_EnableRaytracingValidation =
          decltype(&::NvAPI_D3D12_EnableRaytracingValidation);
      using PFN_NvAPI_D3D12_RegisterRaytracingValidationMessageCallback =
          decltype(&::NvAPI_D3D12_RegisterRaytracingValidationMessageCallback);

      PFN_NvAPI_D3D12_EnableRaytracingValidation NvAPI_D3D12_EnableRaytracingValidation =
          (PFN_NvAPI_D3D12_EnableRaytracingValidation)nvapi_QueryInterface(
              getId("NvAPI_D3D12_EnableRaytracingValidation"));
      PFN_NvAPI_D3D12_RegisterRaytracingValidationMessageCallback
          NvAPI_D3D12_RegisterRaytracingValidationMessageCallback =
              (PFN_NvAPI_D3D12_RegisterRaytracingValidationMessageCallback)nvapi_QueryInterface(
                  getId("NvAPI_D3D12_RegisterRaytracingValidationMessageCallback"));
      NvAPI_FlushRTValidation =
          (PFN_NvAPI_D3D12_FlushRaytracingValidationMessages)nvapi_QueryInterface(
              getId("NvAPI_D3D12_FlushRaytracingValidationMessages"));

      if(NvAPI_D3D12_EnableRaytracingValidation &&
         NvAPI_D3D12_RegisterRaytracingValidationMessageCallback && NvAPI_FlushRTValidation)
      {
        ID3D12Device5 *dev5 = NULL;
        dev->QueryInterface(__uuidof(ID3D12Device5), (void **)&dev5);

        nvResult = NvAPI_D3D12_EnableRaytracingValidation(
            dev5, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE);

        if(nvResult == NVAPI_OK)
        {
          RDCLOG("Enabled NV RT validation for D3D12");

          void *dummy = NULL;
          nvResult = NvAPI_D3D12_RegisterRaytracingValidationMessageCallback(dev5, &rtValidationCB,
                                                                             NULL, &dummy);
        }
        else
        {
          RDCWARN("Couldn't enable NV RT validation: %d", nvResult);
        }

        SAFE_RELEASE(dev5);
      }
      else
      {
        RDCWARN("Couldn't get NV raytracing validation functions");
      }
    }
    else
    {
      RDCWARN("NvAPI_Initialize failed when trying to enable NV Raytracing validation: %d", nvResult);
    }
  }
}

void NVAftermath_DumpRTValidation(ID3D12Device5 *dev5)
{
  if(NvAPI_FlushRTValidation)
    NvAPI_FlushRTValidation(dev5);
}

void NVAftermath_DumpCrash()
{
  if(!table.GetCrashDumpStatus)
    return;

  GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
  GFSDK_Aftermath_Result res = table.GetCrashDumpStatus(&status);

  if(!GFSDK_Aftermath_SUCCEED(res))
  {
    RDCERR("Aftermath failed to get crash dump status: %x", res);
    return;
  }

  if(status == GFSDK_Aftermath_CrashDump_Status_NotStarted)
  {
    RDCLOG("No aftermath crash detected");
    return;
  }

  if(status == GFSDK_Aftermath_CrashDump_Status_Unknown)
  {
    RDCERR("Unknown error fetching aftermath crash");
    return;
  }

  int i = 0;
  while(status == GFSDK_Aftermath_CrashDump_Status_CollectingData ||
        status == GFSDK_Aftermath_CrashDump_Status_InvokingCallback)
  {
    Threading::Sleep(50);
    res = table.GetCrashDumpStatus(&status);

    if(!GFSDK_Aftermath_SUCCEED(res))
    {
      RDCERR("Aftermath failed to get crash dump status: %x", res);
      return;
    }

    i++;
    if(i > 100)
    {
      RDCERR("Time-out waiting for aftermath results");
      break;
    }
  }

  if(status == GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed)
    RDCERR("Collecting data failed");
  else if(status == GFSDK_Aftermath_CrashDump_Status_Unknown)
    RDCERR("Unknown error fetching aftermath crash");
  else if(status == GFSDK_Aftermath_CrashDump_Status_Finished)
    RDCLOG("Aftermath dump created");
}

#else    // real aftermath headers available

void NVAftermath_Init()
{
  if(Replay_Debug_EnableNVRTValidation())
  {
    RDCLOG("NV RT validation support unavailable in this build");
  }

  if(Replay_Debug_EnableAftermath())
  {
    RDCLOG("NV Aftermath support unavailable in this build");
  }
}

void NVAftermath_EnableVK(const std::set<rdcstr> &supportedExtensions, rdcarray<rdcstr> &Extensions,
                          const void **deviceCreateNext)
{
}

void NVAftermath_EnableD3D12(ID3D12Device *dev)
{
}

void NVAftermath_DumpCrash()
{
}

void NVAftermath_DumpRTValidation(ID3D12Device5 *dev5)
{
}

#endif
