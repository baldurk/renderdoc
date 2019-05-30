/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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
#include "common/common.h"
#include "core/plugins.h"
#include "strings/string_utils.h"
#include "amd_isa_devices.h"

namespace GCNISA
{
#if ENABLED(RDOC_WIN32)
static const std::string amdspv_name = "amdspv.exe";
static const std::string virtualcontext_name = "VirtualContext.exe";
#else
static const std::string amdspv_name = "amdspv.sh";
static const std::string virtualcontext_name = "VirtualContext";
#endif

std::string pluginPath = "amd/isa";

// in amd_isa_<plat>.cpp
std::string DisassembleDXBC(const bytebuf &shaderBytes, const std::string &target);

static bool IsSupported(ShaderEncoding encoding)
{
  if(encoding == ShaderEncoding::GLSL)
  {
    std::string vc = LocatePluginFile(pluginPath, virtualcontext_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(vc.c_str(), get_dirname(vc).c_str(), "", true, &result);

    // running with no parameters produces an error, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  if(encoding == ShaderEncoding::SPIRV)
  {
    // TODO need to check if an AMD context is running
    std::string amdspv = LocatePluginFile(pluginPath, amdspv_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(amdspv.c_str(), get_dirname(amdspv).c_str(), "", true, &result);

    // running with no parameters produces help text, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  // we only need to check if we can get atidxx64.dll
  if(encoding == ShaderEncoding::DXBC)
  {
    std::string test = DisassembleDXBC(bytebuf(), "");

    return test.empty();
  }

  return false;
}

void GetTargets(GraphicsAPI api, std::vector<std::string> &targets)
{
  targets.reserve(asicCount + 1);

  ShaderEncoding primary = ShaderEncoding::SPIRV, secondary = ShaderEncoding::SPIRV;

  if(IsD3D(api))
  {
    primary = ShaderEncoding::DXBC;
    secondary = ShaderEncoding::DXBC;
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    primary = ShaderEncoding::GLSL;
    secondary = ShaderEncoding::SPIRV;
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    primary = ShaderEncoding::SPIRV;
    secondary = ShaderEncoding::SPIRV;
  }

  if(IsSupported(primary) || IsSupported(secondary))
  {
    // OpenGL doesn't support AMDIL
    if(api != GraphicsAPI::OpenGL)
      targets.push_back("AMDIL");

    int apiBitmask = 1 << (int)api;

    for(int i = 0; i < asicCount; i++)
    {
      if(asicInfo[i].apiBitmask & apiBitmask)
        targets.push_back(asicInfo[i].name);
    }
  }
  else
  {
    // if unsupported, push a 'dummy' target, so that when the user selects it they'll see the error
    // message
    targets.push_back("AMD GCN ISA");
  }
}

std::string DisassembleSPIRV(ShaderStage stage, const bytebuf &shaderBytes, const std::string &target)
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

  std::string cmdLine = "-Dall -l";

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
    case ShaderStage::Count: return "; Cannot identify shader type";
  }

  std::string tempPath = FileIO::GetTempFolderFilename() + "rdoc_isa__";
  std::string inPath = StringFormat::Fmt("%sin.spv", tempPath.c_str());

  cmdLine += StringFormat::Fmt(
      " -set in.spv=\"%sin.spv\" out.%s.palIlText=\"%sout.il\" out.%s.isa=\"%sout.bin\" "
      "out.%s.isaText=\"%sout.txt\" out.%s.isaInfo=\"%sstats.txt\" "
      "out.glslLog=\"%sout.log\" defaultOutput=0",
      tempPath.c_str(), stageName, tempPath.c_str(), stageName, tempPath.c_str(), stageName,
      tempPath.c_str(), stageName, tempPath.c_str(), tempPath.c_str());

  FileIO::dump(inPath.c_str(), shaderBytes.data(), shaderBytes.size());

  // try to locate the amdspv relative to our running program
  std::string amdspv = LocatePluginFile(pluginPath, amdspv_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(amdspv.c_str(), get_dirname(amdspv).c_str(), cmdLine.c_str(), true, &result);

  if(result.strStdout.find("SUCCESS") == std::string::npos)
  {
    return "; Failed to Disassemble - " + result.strStdout;
  }

  // remove artifacts we don't need
  FileIO::Delete(StringFormat::Fmt("%sin.spv", tempPath.c_str()).c_str());
  FileIO::Delete(StringFormat::Fmt("%sout.log", tempPath.c_str()).c_str());
  FileIO::Delete(StringFormat::Fmt("%sout.bin", tempPath.c_str()).c_str());

  std::string ret;

  if(amdil)
  {
    std::vector<byte> data;
    FileIO::slurp(StringFormat::Fmt("%sout.il", tempPath.c_str()).c_str(), data);

    ret = std::string(data.data(), data.data() + data.size());
  }
  else
  {
    std::vector<byte> data;
    FileIO::slurp(StringFormat::Fmt("%sout.txt", tempPath.c_str()).c_str(), data);

    ret = std::string(data.data(), data.data() + data.size());

    std::string statsfile = StringFormat::Fmt("%sstats.txt", tempPath.c_str());

    if(FileIO::exists(statsfile.c_str()))
    {
      FileIO::slurp(statsfile.c_str(), data);

      ret += std::string(data.data(), data.data() + data.size());
    }
  }

  FileIO::Delete(StringFormat::Fmt("%sout.il", tempPath.c_str()).c_str());
  FileIO::Delete(StringFormat::Fmt("%sout.txt", tempPath.c_str()).c_str());
  FileIO::Delete(StringFormat::Fmt("%sstats.txt", tempPath.c_str()).c_str());

  std::string header = StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str());

  ret.insert(ret.begin(), header.begin(), header.end());

  return ret;
}

std::string DisassembleGLSL(ShaderStage stage, const bytebuf &shaderBytes, const std::string &target)
{
  if(!IsSupported(ShaderEncoding::GLSL))
  {
    return R"(; GLSL disassembly not supported, couldn't locate VirtualContext.exe or it failed to run.
; It only works when the AMD driver is currently being used for graphics.
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
    case ShaderStage::Count: return "; Cannot identify shader type";
  }

  std::string tempPath = FileIO::GetTempFolderFilename() + "rdoc_isa__";
  std::string inPath = StringFormat::Fmt("%sin.%s", tempPath.c_str(), stageName);
  std::string outPath = StringFormat::Fmt("%sout.txt", tempPath.c_str());
  std::string binPath = StringFormat::Fmt("%sout.bin", tempPath.c_str());
  std::string statsPath = StringFormat::Fmt("%sstats.txt", tempPath.c_str());

  std::string cmdLine = "\"";

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

  // input files
  for(int i = 0; i < 6; i++)
  {
    if(i == stageIndex)
      cmdLine += inPath;

    cmdLine += ';';
  }

  if(!found)
    return "; Invalid ISA Target specified";

  cmdLine += ";\"";

  FileIO::dump(inPath.c_str(), shaderBytes.data(), shaderBytes.size());

  // try to locate the amdspv relative to our running program
  std::string vc = LocatePluginFile(pluginPath, virtualcontext_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(vc.c_str(), get_dirname(vc).c_str(), cmdLine.c_str(), true, &result);

  if(result.retCode != 0 || result.strStdout.find("Error") != std::string::npos ||
     result.strStdout.empty() || !FileIO::exists(outPath.c_str()))
  {
    return "; Failed to Disassemble - check AMD driver is currently running\n\n; " + result.strStdout;
  }

  // remove artifacts we don't need
  FileIO::Delete(inPath.c_str());
  FileIO::Delete(binPath.c_str());

  std::string ret;

  {
    std::vector<byte> data;
    FileIO::slurp(outPath.c_str(), data);
    ret = std::string(data.data(), data.data() + data.size());

    if(FileIO::exists(statsPath.c_str()))
    {
      FileIO::slurp(statsPath.c_str(), data);
      ret += "\n\n";
      ret += std::string(data.data(), data.data() + data.size());
    }
  }

  FileIO::Delete(outPath.c_str());
  FileIO::Delete(statsPath.c_str());

  std::string header = StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str());

  ret.insert(ret.begin(), header.begin(), header.end());

  return ret;
}

std::string Disassemble(ShaderEncoding encoding, ShaderStage stage, const bytebuf &shaderBytes,
                        const std::string &target)
{
  if(encoding == ShaderEncoding::DXBC)
    return DisassembleDXBC(shaderBytes, target);

  if(encoding == ShaderEncoding::SPIRV)
    return DisassembleSPIRV(stage, shaderBytes, target);

  if(encoding == ShaderEncoding::GLSL)
    return DisassembleGLSL(stage, shaderBytes, target);

  return StringFormat::Fmt("Unsupported encoding for shader '%s'", ToStr(encoding).c_str());
}

};    // namespace GCNISA
