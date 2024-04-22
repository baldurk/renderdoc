/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "amd_isa.h"
#include <ctype.h>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "common/formatting.h"
#include "core/plugins.h"
#include "core/settings.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"
#include "amd_isa_devices.h"

RDOC_CONFIG(bool, AMD_ISA_ShowLegacyASICs, false,
            "Show legacy ASICs for AMD shader disassembly targets. Note that depending on the "
            "environment if driver support is required, these may not be available.");

namespace GCNISA
{
#if ENABLED(RDOC_WIN32)
static const rdcstr amdspv_name = "amdspv.exe";
static const rdcstr virtualcontext_name = "VirtualContext.exe";
#else
static const rdcstr amdspv_name = "amdspv";
static const rdcstr virtualcontext_name = "VirtualContext";
#endif

rdcstr pluginPath = "amd/isa";

// in amd_isa_<plat>.cpp
rdcstr DisassembleDXBC(const bytebuf &shaderBytes, const rdcstr &target);

static bool CheckForSupport(ShaderEncoding encoding)
{
  if(encoding == ShaderEncoding::GLSL)
  {
    rdcstr vc = LocatePluginFile(pluginPath, virtualcontext_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(vc, get_dirname(vc), "", true, &result);

    // running with no parameters produces an error, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  if(encoding == ShaderEncoding::SPIRV || encoding == ShaderEncoding::OpenGLSPIRV)
  {
    // TODO need to check if an AMD context is running
    rdcstr amdspv = LocatePluginFile(pluginPath, amdspv_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(amdspv, get_dirname(amdspv), "", true, &result);

    // running with no parameters produces help text, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  // we only need to check if we can get atidxx64.dll
  if(encoding == ShaderEncoding::DXBC)
  {
    rdcstr test = DisassembleDXBC(bytebuf(), "");

    return test.empty();
  }

  return false;
}

static void GetEncodings(GraphicsAPI api, ShaderEncoding &primary, ShaderEncoding &secondary)
{
  if(IsD3D(api))
  {
    primary = ShaderEncoding::DXBC;
    secondary = ShaderEncoding::DXIL;
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    primary = ShaderEncoding::GLSL;
    secondary = ShaderEncoding::OpenGLSPIRV;
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    primary = ShaderEncoding::SPIRV;
    secondary = ShaderEncoding::SPIRV;
  }
}

bool encodingCached[arraydim<ShaderEncoding>()] = {};
bool encodingSupported[arraydim<ShaderEncoding>()] = {};

static const rdcstr unsupportedTargetName = "AMD GCN ISA";

Threading::ThreadHandle supportCheckThread = 0;

static void CacheSupport(ShaderEncoding primary, ShaderEncoding secondary = ShaderEncoding::Unknown)
{
  // if there's a thread running, sync it now.
  if(supportCheckThread)
  {
    Threading::JoinThread(supportCheckThread);
    Threading::CloseThread(supportCheckThread);
    supportCheckThread = 0;
  }

  // if we have these encodings cached now, return
  if(encodingCached[(size_t)primary] &&
     (secondary == ShaderEncoding::Unknown || encodingCached[(size_t)secondary]))
    return;

  // kick off a thread to cache these encodings' support
  supportCheckThread = Threading::CreateThread([primary, secondary]() {
    encodingSupported[(size_t)primary] = CheckForSupport(primary);
    encodingSupported[(size_t)secondary] = CheckForSupport(secondary);

    encodingCached[(size_t)primary] = true;
    encodingCached[(size_t)secondary] = true;
  });
}

static bool IsSupported(ShaderEncoding encoding)
{
  CacheSupport(encoding);

  return encodingSupported[(size_t)encoding];
}

void CacheSupport(GraphicsAPI api)
{
  ShaderEncoding primary = ShaderEncoding::SPIRV, secondary = ShaderEncoding::SPIRV;
  GetEncodings(api, primary, secondary);

  CacheSupport(primary, secondary);
}

void GetTargets(GraphicsAPI api, const DriverInformation &driver, rdcarray<rdcstr> &targets)
{
  targets.reserve(asicCount + 1);

  ShaderEncoding primary = ShaderEncoding::SPIRV, secondary = ShaderEncoding::SPIRV;
  GetEncodings(api, primary, secondary);

  bool validAMDGLDriver = true;

  if(api == GraphicsAPI::OpenGL)
  {
    if(driver.vendor == GPUVendor::AMD)
    {
      // try to see if we're on 22.7.1 or newer. This is a bit of guesswork but we assume the
      // version string won't change *too* much over time. If it does, it's impossible to predict
      // how so this code will just need to be changed.
      // We match any /[0-9.]+/ substrings by hand, discard if one looks like an OpenGL version
      // (4.x.y) and assume the next is the driver version
      const char *num = driver.version;
      const char *end = num + sizeof(driver.version);
      while(*num && num < end)
      {
        // skip any non-matching digits
        if(!isdigit(*num) && *num != '.')
        {
          num++;
          continue;
        }

        // consume all matching digits
        rdcstr versionStr;
        while(isdigit(*num) || *num == '.')
        {
          versionStr.push_back(*num);
          num++;
        }

        // ignore OpenGL-looking versions
        if(versionStr.beginsWith("4."))
          continue;

        int versionNum[3] = {};
        int idx = 0;
        for(char c : versionStr)
        {
          if(isdigit(c))
          {
            versionNum[idx] *= 10;
            versionNum[idx] += int(c - '0');
          }
          else if(c == '.')
          {
            idx++;

            if(idx > 2)
              break;
          }
        }

        RDCLOG("Running on AMD driver version %d.%d.%d", versionNum[0], versionNum[1], versionNum[2]);

        if(versionNum[0] > 22 || versionNum[1] >= 7)
          validAMDGLDriver = false;

        // we found a match, stop regardless
        break;
      }
    }
  }

  if(validAMDGLDriver && (IsSupported(primary) || IsSupported(secondary)))
  {
    targets.push_back("AMDIL");

    for(int i = AMD_ISA_ShowLegacyASICs() ? 0 : legacyAsicCount; i < asicCount; i++)
      targets.push_back(asicInfo[i].name);
  }
  else
  {
    // if unsupported, push a 'dummy' target, so that when the user selects it they'll see the error
    // message
    targets.push_back(unsupportedTargetName);
  }
}

rdcstr DisassembleSPIRV(ShaderStage stage, const bytebuf &shaderBytes, const rdcstr &target)
{
  if(!IsSupported(ShaderEncoding::SPIRV))
  {
    return "; SPIR-V disassembly not supported, couldn't locate " + amdspv_name + R"(
; Normally it's in plugins/amd/isa/ in your build - if you are building locally you'll need to
; download the plugins package.
;
; To see instructions on how to download and configure the plugins on your system, go to:
; https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";
  }

  rdcstr cmdLine = "-Dall -l";

  bool found = false;

  for(int i = 0; i < asicCount; i++)
  {
    const asic &a = asicInfo[i];
    if(target == a.name)
    {
      cmdLine += " -gfxip ";
      cmdLine += a.gfxIpString;

      found = true;

      break;
    }
  }

  bool amdil = false;
  if(!found && target == "AMDIL")
  {
    cmdLine += " -gfxip 8";
    found = true;
    amdil = true;
  }

  if(!found)
    return "; Invalid ISA Target specified";

  const char *stageName = "unk";

  switch(stage)
  {
    case ShaderStage::Vertex: stageName = "vert"; break;
    case ShaderStage::Tess_Control: stageName = "tesc"; break;
    case ShaderStage::Tess_Eval: stageName = "tese"; break;
    case ShaderStage::Geometry: stageName = "geom"; break;
    case ShaderStage::Fragment: stageName = "frag"; break;
    case ShaderStage::Compute: stageName = "comp"; break;
    case ShaderStage::Task: stageName = "task"; break;
    case ShaderStage::Mesh: stageName = "mesh"; break;
    case ShaderStage::RayGen: stageName = "rgen"; break;
    case ShaderStage::Intersection: stageName = "rint"; break;
    case ShaderStage::AnyHit: stageName = "rahit"; break;
    case ShaderStage::ClosestHit: stageName = "rchit"; break;
    case ShaderStage::Miss: stageName = "rmiss"; break;
    case ShaderStage::Callable: stageName = "rcall"; break;
    case ShaderStage::Count: return "; Cannot identify shader type";
  }

  rdcstr tempPath = FileIO::GetTempFolderFilename() + "rdoc_isa__";
  rdcstr inPath = StringFormat::Fmt("%sin.spv", tempPath.c_str());

  cmdLine += StringFormat::Fmt(
      " -set in.spv=\"%sin.spv\" out.%s.palIlText=\"%sout.il\" out.%s.isa=\"%sout.bin\" "
      "out.%s.isaText=\"%sout.txt\" out.%s.isaInfo=\"%sstats.txt\" "
      "out.glslLog=\"%sout.log\" defaultOutput=0",
      tempPath.c_str(), stageName, tempPath.c_str(), stageName, tempPath.c_str(), stageName,
      tempPath.c_str(), stageName, tempPath.c_str(), tempPath.c_str());

  FileIO::WriteAll(inPath, shaderBytes);

  // try to locate the amdspv relative to our running program
  rdcstr amdspv = LocatePluginFile(pluginPath, amdspv_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(amdspv, get_dirname(amdspv), cmdLine, true, &result);

  if(result.strStdout.find("SUCCESS") < 0)
  {
    return "; Failed to Disassemble - " + result.strStdout;
  }

  // remove artifacts we don't need
  FileIO::Delete(StringFormat::Fmt("%sin.spv", tempPath.c_str()));
  FileIO::Delete(StringFormat::Fmt("%sout.log", tempPath.c_str()));
  FileIO::Delete(StringFormat::Fmt("%sout.bin", tempPath.c_str()));

  rdcstr ret;

  if(amdil)
  {
    FileIO::ReadAll(StringFormat::Fmt("%sout.il", tempPath.c_str()), ret);
  }
  else
  {
    FileIO::ReadAll(StringFormat::Fmt("%sout.txt", tempPath.c_str()), ret);

    rdcstr statsfile = StringFormat::Fmt("%sstats.txt", tempPath.c_str());

    if(FileIO::exists(statsfile))
    {
      rdcstr stats;
      FileIO::ReadAll(statsfile, stats);

      ret += "\n\n" + stats;
    }
  }

  FileIO::Delete(StringFormat::Fmt("%sout.il", tempPath.c_str()));
  FileIO::Delete(StringFormat::Fmt("%sout.txt", tempPath.c_str()));
  FileIO::Delete(StringFormat::Fmt("%sstats.txt", tempPath.c_str()));

  rdcstr header = StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str());

  ret.insert(0, header.begin(), header.size());

  return ret;
}

rdcstr DisassembleGLSL(ShaderStage stage, const bytebuf &shaderBytes, const rdcstr &target)
{
  if(!IsSupported(ShaderEncoding::GLSL) || target == unsupportedTargetName)
  {
    return R"(; GLSL disassembly not supported, couldn't locate VirtualContext.exe or it failed to run.
; It only works when the AMD driver is currently being used for graphics, and only on drivers older
; *older* than 22.7.1, where support for this method of disassembly stopped.
;
; To see instructions on how to download and configure the plugins on your system, go to:
; https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";
  }

  const char *stageName = "unk";
  int stageIndex = 0;

  switch(stage)
  {
    case ShaderStage::Vertex:
      stageIndex = 0;
      stageName = "vert";
      break;
    case ShaderStage::Tess_Control:
      stageIndex = 1;
      stageName = "tesc";
      break;
    case ShaderStage::Tess_Eval:
      stageIndex = 2;
      stageName = "tese";
      break;
    case ShaderStage::Geometry:
      stageIndex = 3;
      stageName = "geom";
      break;
    case ShaderStage::Fragment:
      stageIndex = 4;
      stageName = "frag";
      break;
    case ShaderStage::Compute:
      stageIndex = 5;
      stageName = "comp";
      break;
    case ShaderStage::Task:
      stageIndex = 6;
      stageName = "task";
      break;
    case ShaderStage::Mesh:
      stageIndex = 7;
      stageName = "mesh";
      break;
    case ShaderStage::RayGen:
      stageIndex = 8;
      stageName = "rgen";
      break;
    case ShaderStage::Intersection:
      stageIndex = 9;
      stageName = "rint";
      break;
    case ShaderStage::AnyHit:
      stageIndex = 10;
      stageName = "rahit";
      break;
    case ShaderStage::ClosestHit:
      stageIndex = 11;
      stageName = "rchit";
      break;
    case ShaderStage::Miss:
      stageIndex = 12;
      stageName = "rmiss";
      break;
    case ShaderStage::Callable:
      stageIndex = 13;
      stageName = "rcall";
      break;
    case ShaderStage::Count: return "; Cannot identify shader type";
  }

  rdcstr tempPath = FileIO::GetTempFolderFilename() + "rdoc_isa__";
  rdcstr inPath = StringFormat::Fmt("%sin.%s", tempPath.c_str(), stageName);
  rdcstr outPath = StringFormat::Fmt("%sout.txt", tempPath.c_str());
  rdcstr binPath = StringFormat::Fmt("%sout.bin", tempPath.c_str());
  rdcstr statsPath = StringFormat::Fmt("%sstats.txt", tempPath.c_str());
  rdcstr ilPath = StringFormat::Fmt("%sil.txt", tempPath.c_str());

  rdcstr cmdLine = "\"";

  // ISA disassembly
  for(int i = 0; i < 6; i++)
  {
    if(i == stageIndex)
      cmdLine += outPath;

    cmdLine += ';';
  }

  // ISA binary, we don't care about this
  cmdLine += binPath + ";";

  // statistics
  for(int i = 0; i < 6; i++)
  {
    if(i == stageIndex)
      cmdLine += statsPath;

    cmdLine += ';';
  }

  bool found = false;
  for(int i = 0; i < asicCount; i++)
  {
    const asic &a = asicInfo[i];
    if(target == a.name)
    {
      cmdLine += StringFormat::Fmt("%d;%d;", a.chipFamily, a.chipRevision);
      found = true;
      break;
    }
  }

  // dummy values
  if(!found)
  {
    const asic &a = asicInfo[legacyAsicCount];
    cmdLine += StringFormat::Fmt("%d;%d;", a.chipFamily, a.chipRevision);
  }

  // input files
  for(int i = 0; i < 6; i++)
  {
    if(i == stageIndex)
      cmdLine += inPath;

    cmdLine += ';';
  }

  cmdLine += ";\"";

  // amdil files
  for(int i = 0; i < 6; i++)
  {
    if(i == stageIndex)
      cmdLine += ilPath;

    cmdLine += ';';
  }

  if(!found && target == "AMDIL")
  {
    outPath = ilPath;
    found = true;
  }

  if(!found)
    return "; Invalid ISA Target specified";

  FileIO::WriteAll(inPath, shaderBytes);

  // try to locate the amdspv relative to our running program
  rdcstr vc = LocatePluginFile(pluginPath, virtualcontext_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(vc, get_dirname(vc), cmdLine, true, &result);

  if(result.retCode != 0 || result.strStdout.find("Error") >= 0 || result.strStdout.empty() ||
     !FileIO::exists(outPath))
  {
    return "; Failed to Disassemble - check AMD driver is currently running\n\n; " + result.strStdout;
  }

  // remove artifacts we don't need
  FileIO::Delete(inPath);
  FileIO::Delete(binPath);

  rdcstr ret;

  {
    FileIO::ReadAll(outPath, ret);

    while(ret.back() == '\0')
      ret.pop_back();

    if(FileIO::exists(statsPath))
    {
      rdcstr stats;
      FileIO::ReadAll(statsPath, stats);
      ret += "\n\n" + stats;
    }
  }

  FileIO::Delete(outPath);
  FileIO::Delete(statsPath);

  rdcstr header = StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str());

  ret.insert(0, header.begin(), header.size());

  return ret;
}

rdcstr Disassemble(ShaderEncoding encoding, ShaderStage stage, const bytebuf &shaderBytes,
                   const rdcstr &target)
{
  if(encoding == ShaderEncoding::DXBC)
    return DisassembleDXBC(shaderBytes, target);

  if(encoding == ShaderEncoding::SPIRV || encoding == ShaderEncoding::OpenGLSPIRV)
    return DisassembleSPIRV(stage, shaderBytes, target);

  if(encoding == ShaderEncoding::GLSL)
    return DisassembleGLSL(stage, shaderBytes, target);

  return StringFormat::Fmt("Unsupported encoding for shader '%s'", ToStr(encoding).c_str());
}

};    // namespace GCNISA
