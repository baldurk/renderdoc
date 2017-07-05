/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "driver/shaders/spirv/spirv_common.h"
#include "serialise/string_utils.h"
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

std::string LocatePlugin(const std::string &fileName)
{
  std::string ret;

  std::string exepath;
  FileIO::GetExecutableFilename(exepath);
  exepath = dirname(exepath);

  std::vector<std::string> paths;

#if defined(RENDERDOC_PLUGINS_PATH)
  string customPath(RENDERDOC_PLUGINS_PATH);

  if(FileIO::IsRelativePath(customPath))
    customPath = exepath + "/" + customPath;

  paths.push_back(customPath);
#endif

  // windows installation
  paths.push_back(exepath + "/plugins");
  // linux installation
  paths.push_back(exepath + "/../share/renderdoc/plugins");
// also search the appropriate OS-specific location in the root
#if ENABLED(RDOC_WIN32) && ENABLED(RDOC_X64)
  paths.push_back(exepath + "/../../plugins-win64");
#endif

#if ENABLED(RDOC_WIN32) && DISABLED(RDOC_X64)
  paths.push_back(exepath + "/../../plugins-win32");
#endif

#if ENABLED(RDOC_LINUX)
  paths.push_back(exepath + "/../../plugins-linux64");
#endif

  // there is no standard path for local builds as we don't provide these plugins in the repository
  // directly. As a courtesy we search the root of the build, from the executable. The user can
  // always put the plugins folder relative to the exe where it would be in an installation too.
  paths.push_back(exepath + "/../../plugins");

  // in future maybe we want to search a user-specific plugins folder? Like ~/.renderdoc/ on linux
  // or %APPDATA%/renderdoc on windows?

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    std::string check = paths[i] + "/amd/isa/" + fileName;
    if(FileIO::exists(check.c_str()))
    {
      ret = check;
      break;
    }
  }

  // if we didn't find it anywhere, just try running it directly in case it's in the PATH
  if(ret.empty())
    ret = fileName;

  return ret;
}

static bool IsSupported(GraphicsAPI api)
{
  if(api == GraphicsAPI::Vulkan)
  {
    std::string vc = LocatePlugin(virtualcontext_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(vc.c_str(), dirname(vc).c_str(), "", &result);

    // running with no parameters produces an error, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  if(api == GraphicsAPI::OpenGL)
  {
    // TODO need to check if an AMD context is running
    std::string amdspv = LocatePlugin(amdspv_name);

    Process::ProcessResult result = {};
    Process::LaunchProcess(amdspv.c_str(), dirname(amdspv).c_str(), "", &result);

    // running with no parameters produces help text, so if there's no output something went wrong.
    if(result.strStdout.empty())
      return false;

    return true;
  }

  // we only need to check if we can get atidxx64.dll
  if(api == GraphicsAPI::D3D11 || api == GraphicsAPI::D3D12)
  {
    DXBC::DXBCFile *dummy = NULL;
    std::string test = Disassemble(dummy, "");

    return test.empty();
  }

  return false;
}

void GetTargets(GraphicsAPI api, std::vector<std::string> &targets)
{
  targets.reserve(ARRAY_COUNT(asicInfo) + 1);

  if(IsSupported(api))
  {
    // OpenGL doesn't support AMDIL
    if(api != GraphicsAPI::OpenGL)
      targets.push_back("AMDIL");

    for(const asic &a : asicInfo)
      targets.push_back(a.name);
  }
  else
  {
    // if unsupported, push a 'dummy' target, so that when the user selects it they'll see the error
    // message
    targets.push_back("AMD GCN ISA");
  }
}

std::string Disassemble(const SPVModule *spv, const std::string &entry, const std::string &target)
{
  if(!IsSupported(GraphicsAPI::Vulkan))
  {
    return R"(; SPIR-V disassembly not supported, couldn't locate amdspv.exe.
; Normally it's in plugins/amd/isa/ in your build - if you are building locally you'll need to
; download the plugins package.
;
; To see instructions on how to download and configure the plugins on your system, go to:
; https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";
  }

  std::string cmdLine = "-set spirvDasmLegacyFormat=1 -Dall -l";

  bool found = false;

  for(const asic &a : asicInfo)
  {
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

  ShaderStage stage = spv->StageForEntry(entry);

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

  FileIO::dump(inPath.c_str(), spv->spirv.data(), spv->spirv.size() * sizeof(uint32_t));

  // try to locate the amdspv relative to our running program
  std::string amdspv = LocatePlugin(amdspv_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(amdspv.c_str(), dirname(amdspv).c_str(), cmdLine.c_str(), &result);

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
    vector<byte> data;
    FileIO::slurp(StringFormat::Fmt("%sout.il", tempPath.c_str()).c_str(), data);

    ret = std::string(data.data(), data.data() + data.size());
  }
  else
  {
    vector<byte> data;
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

std::string Disassemble(ShaderStage stage, const std::vector<std::string> &glsl,
                        const std::string &target)
{
  if(!IsSupported(GraphicsAPI::OpenGL))
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
  for(const asic &a : asicInfo)
  {
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

  std::string source;

  // concatenate the source files together
  for(const std::string &s : glsl)
  {
    source += s;
    source += "\n";
  }

  FileIO::dump(inPath.c_str(), source.data(), source.size());

  // try to locate the amdspv relative to our running program
  std::string vc = LocatePlugin(virtualcontext_name);

  Process::ProcessResult result = {};
  Process::LaunchProcess(vc.c_str(), dirname(vc).c_str(), cmdLine.c_str(), &result);

  if(result.retCode != 0)
  {
    return "; Failed to Disassemble - " + result.strStdout;
  }

  // remove artifacts we don't need
  FileIO::Delete(inPath.c_str());
  FileIO::Delete(binPath.c_str());

  std::string ret;

  {
    vector<byte> data;
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

};    // namespace GCNISA
