/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "amd_rgp.h"
#include "common/common.h"
#include "core/core.h"
#include "core/plugins.h"
#include "official/RGP/DevDriverAPI.h"

uint64_t MakeTagFromMarker(const char *marker)
{
  if(!marker)
    return 0;

  uint64_t ret = 0;

  for(int i = 0; i < 7 && marker && marker[i]; i++)
    ret |= uint64_t(marker[i]) << (i * 8);

  return ret;
}

const char *AMDRGPControl::GetBeginMarker()
{
  return "BeginRenderDocRGPCapture======";
}

const char *AMDRGPControl::GetEndMarker()
{
  return "EndRenderDocRGPCapture======";
}

uint64_t AMDRGPControl::GetBeginTag()
{
  return MakeTagFromMarker(GetBeginMarker());
}

uint64_t AMDRGPControl::GetEndTag()
{
  return MakeTagFromMarker(GetEndMarker());
}

AMDRGPControl::AMDRGPControl()
{
  m_RGPDispatchTable = new DevDriverAPI;
  m_RGPDispatchTable->majorVersion = DEV_DRIVER_API_MAJOR_VERSION;
  m_RGPDispatchTable->minorVersion = DEV_DRIVER_API_MINOR_VERSION;
  m_RGPContext = NULL;

  const bool enabled = RenderDoc::Inst().GetConfigSetting("ExternalTool_RGPIntegration") == "1";

  if(!enabled)
  {
    RDCLOG("AMD RGP Interop is not enabled");
    return;
  }

#if ENABLED(RDOC_WIN32) || ENABLED(RDOC_LINUX)

  RDCLOG("Attempting to enable AMD RGP Interop");

  // manually load in the DevDriverAPI dll and set up the function table
  std::string dllName("DevDriverAPI");

#if ENABLED(RDOC_WIN32)
#if ENABLED(RDOC_X64)
  dllName += "-x64";
#endif
  dllName += ".dll";
#else
  dllName = "lib" + dllName;
  dllName += ".so";
#endif

  // first try in the plugin location it will be in distributed builds
  std::string dllPath = LocatePluginFile("amd/rgp", dllName.c_str());

  void *module = Process::LoadModule(dllPath.c_str());
  if(module == NULL)
  {
    module = Process::LoadModule(dllName.c_str());
  }

  if(module == NULL)
  {
    RDCWARN(
        "AMD DevDriverAPI could not be initialized successfully. "
        "Are you missing the DLL?");
    return;
  }

  typedef DevDriverStatus (*DevDriverGetFuncTableType)(void *);

  DevDriverGetFuncTableType DevDriverGetFuncTable =
      (DevDriverGetFuncTableType)Process::GetFunctionAddress(module, "DevDriverGetFuncTable");

  DevDriverStatus rgpStatus = DevDriverGetFuncTable(m_RGPDispatchTable);
  if(rgpStatus == DEV_DRIVER_STATUS_SUCCESS)
  {
    DevDriverFeatures initOptions[] = {
        {DEV_DRIVER_FEATURE_ENABLE_RGP, sizeof(DevDriverFeatureRGP)},
    };

    int numFeatures = sizeof(initOptions) / sizeof(initOptions[0]);
    rgpStatus = m_RGPDispatchTable->DevDriverInit(initOptions, numFeatures, &m_RGPContext);

    // check the driver version if initialization succeeded.
    bool supportsInterop = false;
    if(rgpStatus == DEV_DRIVER_STATUS_SUCCESS)
    {
      supportsInterop = DriverSupportsInterop();

      if(supportsInterop)
        RDCLOG("AMD RGP Interop was successfully enabled");
      else
        RDCLOG("AMD RGP Interop could not be enabled");
    }

    // if initialization failed or driver doesn't support interop
    if(supportsInterop == false)
    {
      if(m_RGPContext != NULL)
        m_RGPDispatchTable->DevDriverFinish(m_RGPContext);
      m_RGPContext = NULL;
    }
  }
  else
#endif
  {
    m_RGPContext = NULL;
  }
}

AMDRGPControl::~AMDRGPControl()
{
  if(m_RGPContext != NULL)
    m_RGPDispatchTable->DevDriverFinish(m_RGPContext);

  delete m_RGPDispatchTable;
  m_RGPContext = NULL;
  m_RGPDispatchTable = NULL;
}

bool AMDRGPControl::Initialised()
{
  return m_RGPContext != NULL;
}

bool AMDRGPControl::TriggerCapture(const std::string &path)
{
  if(m_RGPContext == NULL)
    return false;

  // set up for capturing
  RGPProfileOptions profileOptions = {};

  profileOptions.m_pProfileFilePath = path.c_str();

  profileOptions.m_beginFrameTerminatorTag = GetBeginTag();
  profileOptions.m_endFrameTerminatorTag = GetEndTag();
  profileOptions.m_pBeginFrameTerminatorString = GetBeginMarker();
  profileOptions.m_pEndFrameTerminatorString = GetEndMarker();

  DevDriverStatus result = m_RGPDispatchTable->TriggerRgpProfile(m_RGPContext, &profileOptions);
  if(result == DEV_DRIVER_STATUS_SUCCESS)
    return true;
  else
    return false;
}

bool AMDRGPControl::HasCapture()
{
  if(m_RGPContext == NULL)
    return false;

  return m_RGPDispatchTable->IsRgpProfileCaptured(m_RGPContext) == DEV_DRIVER_STATUS_SUCCESS;
}

bool AMDRGPControl::DriverSupportsInterop()
{
  // interop is supported on AMD driver version 18.10.1 or newer
  if(m_RGPContext == NULL)
    return false;

  unsigned int majorVersion = 0;
  unsigned int minorVersion = 0;
  unsigned int subminorVersion = 0;

  if(m_RGPDispatchTable->GetFullDriverVersion(m_RGPContext, &majorVersion, &minorVersion,
                                              &subminorVersion) == DEV_DRIVER_STATUS_SUCCESS)
  {
    if(
        // 19.x.x+
        majorVersion > 18 ||
        // 18.11.x+
        (majorVersion == 18 && minorVersion >= 11) ||
        // 18.10.2+
        (majorVersion == 18 && minorVersion == 10 && subminorVersion > 1))
    {
      return true;
    }
  }
  return false;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Check that markers are distinct for begin and end", "[amd]")
{
  std::string beginMark = AMDRGPControl::GetBeginMarker();
  std::string endMark = AMDRGPControl::GetEndMarker();

  CHECK(beginMark != endMark);
  CHECK(beginMark != "");
  CHECK(endMark != "");

  uint64_t beginTag = AMDRGPControl::GetBeginTag();
  uint64_t endTag = AMDRGPControl::GetEndTag();

  CHECK(beginTag != endTag);
  CHECK(beginTag != 0);
  CHECK(endTag != 0);
}

#endif