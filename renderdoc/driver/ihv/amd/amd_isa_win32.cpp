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
#include "official/RGA/Common/AmdDxGsaCompile.h"
#include "official/RGA/elf/elf32.h"
#include "amd_isa_devices.h"

#if ENABLED(RDOC_X64)
#define DLL_NAME "atidxx64.dll"
#else
#define DLL_NAME "atidxx32.dll"
#endif

static const char *driverDllErrorMessage = R"(Error loading atidxx64.dll.

Currently atidxx64.dll from AMD's driver package is required for GCN disassembly and it cannot be
distributed with RenderDoc.

To see instructions on how to download and configure it on your system, go to:
https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";

namespace GCNISA
{
extern std::string pluginPath;

static HMODULE GetAMDModule()
{
  // first try in the plugin locations
  HMODULE module = LoadLibraryA(LocatePluginFile(GCNISA::pluginPath, DLL_NAME).c_str());

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
    module = LoadLibraryA(DLL_NAME);

  return module;
}

std::string DisassembleDXBC(const bytebuf &shaderBytes, const std::string &target)
{
  HMODULE mod = GetAMDModule();

  if(mod == NULL)
    return "; Error loading " DLL_NAME R"(.

; Currently atidxx64.dll from AMD's driver package is required for GCN disassembly and it cannot be
; distributed with RenderDoc.

; To see instructions on how to download and configure it on your system, go to:
; https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";

  // if shaderBytes is empty we're testing support, so return empty string - indicating no error
  // initialising
  if(shaderBytes.empty() || target == "")
    return "";

  PfnAmdDxGsaCompileShader compileShader =
      (PfnAmdDxGsaCompileShader)GetProcAddress(mod, "AmdDxGsaCompileShader");
  PfnAmdDxGsaFreeCompiledShader freeShader =
      (PfnAmdDxGsaFreeCompiledShader)GetProcAddress(mod, "AmdDxGsaFreeCompiledShader");

  AmdDxGsaCompileShaderInput in = {};
  AmdDxGsaCompileShaderOutput out = {};

  AmdDxGsaCompileOption opts[1] = {};

  in.inputType = GsaInputDxAsmBin;
  in.numCompileOptions = 0;
  in.pCompileOptions = opts;

  for(int i = 0; i < asicCount; i++)
  {
    const asic &a = asicInfo[i];
    if(target == a.name)
    {
      in.chipFamily = a.chipFamily;
      in.chipRevision = a.chipRevision;
      break;
    }
  }

  bool amdil = false;
  if(target == "AMDIL")
  {
    in.chipFamily = asicInfo[0].chipFamily;
    in.chipRevision = asicInfo[0].chipRevision;
    amdil = true;
  }

  if(in.chipFamily == 0)
    return "; Invalid ISA Target specified";

  // we do a little mini parse of the DXBC file, just enough to get the shader code out. This is
  // because we're getting called from outside the D3D backend where the shader bytes are opaque.

  const char *dxbcParseError = "; Failed to fetch D3D shader code from DXBC";

  const byte *base = shaderBytes.data();
  const uint32_t *end = (const uint32_t *)(base + shaderBytes.size());
  const uint32_t *dxbc = (const uint32_t *)base;

  if(*dxbc != MAKE_FOURCC('D', 'X', 'B', 'C'))
    return dxbcParseError;

  dxbc++;       // fourcc
  dxbc += 4;    // hash
  dxbc++;       // unknown
  dxbc++;       // fileLength

  if(dxbc >= end)
    return dxbcParseError;

  const uint32_t numChunks = *dxbc;
  dxbc++;

  std::vector<uint32_t> chunkOffsets;
  for(uint32_t i = 0; i < numChunks; i++)
  {
    if(dxbc >= end)
      return dxbcParseError;

    chunkOffsets.push_back(*dxbc);
    dxbc++;
  }

  in.pShaderByteCode = NULL;
  in.byteCodeLength = 0;

  for(uint32_t offs : chunkOffsets)
  {
    dxbc = (const uint32_t *)(base + offs);

    if(dxbc + 2 >= end)
      return dxbcParseError;

    if(*dxbc == MAKE_FOURCC('S', 'H', 'E', 'X') || *dxbc == MAKE_FOURCC('S', 'H', 'D', 'R'))
    {
      dxbc++;
      in.byteCodeLength = *dxbc;
      dxbc++;
      in.pShaderByteCode = dxbc;

      if(dxbc + (in.byteCodeLength / 4) > end)
        return dxbcParseError;

      break;
    }
  }

  if(in.byteCodeLength == 0)
    return dxbcParseError;

  out.size = sizeof(out);

  compileShader(&in, &out);

  if(out.pShaderBinary == NULL || out.shaderBinarySize < 16)
    return "; Failed to disassemble shader";

  const uint8_t *elf = (const uint8_t *)out.pShaderBinary;

  const Elf32_Ehdr *elfHeader = (const Elf32_Ehdr *)elf;

  std::string ret;

  // minimal code to extract data from ELF. We assume the ELF we got back is well-formed.
  if(IS_ELF(*elfHeader) && elfHeader->e_ident[EI_CLASS] == ELFCLASS32)
  {
    const Elf32_Shdr *strtab =
        (const Elf32_Shdr *)(elf + elfHeader->e_shoff + sizeof(Elf32_Shdr) * elfHeader->e_shstrndx);

    const uint8_t *strtabData = elf + strtab->sh_offset;

    const AmdDxGsaCompileStats *stats = NULL;

    for(int section = 1; section < elfHeader->e_shnum; section++)
    {
      if(section == elfHeader->e_shstrndx)
        continue;

      const Elf32_Shdr *sectHeader =
          (const Elf32_Shdr *)(elf + elfHeader->e_shoff + sizeof(Elf32_Shdr) * section);

      const char *name = (const char *)(strtabData + sectHeader->sh_name);

      const uint8_t *data = elf + sectHeader->sh_offset;

      if(!strcmp(name, ".stats"))
      {
        stats = (const AmdDxGsaCompileStats *)data;
      }
      else if(amdil && !strcmp(name, ".amdil_disassembly"))
      {
        ret.insert(0, (const char *)data, (size_t)sectHeader->sh_size);
        while(ret.back() == '\0')
          ret.pop_back();
      }
      else if(!amdil && !strcmp(name, ".disassembly"))
      {
        ret.insert(0, (const char *)data, (size_t)sectHeader->sh_size);
        while(ret.back() == '\0')
          ret.pop_back();
      }
    }

    if(stats && !amdil)
    {
      std::string statStr = StringFormat::Fmt(
          R"(; -------- Statistics ---------------------
; SGPRs: %u out of %u used
; VGPRs: %u out of %u used
; LDS: %u out of %u bytes used
; %u bytes scratch space used
; Instructions: %u ALU, %u Control Flow, %u TFETCH

)",
          stats->numSgprsUsed, stats->availableSgprs, stats->numVgprsUsed, stats->availableVgprs,
          stats->usedLdsBytes, stats->availableLdsBytes, stats->usedScratchBytes, stats->numAluInst,
          stats->numControlFlowInst, stats->numTfetchInst);

      ret.insert(ret.begin(), statStr.begin(), statStr.end());
    }

    std::string header = StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str());

    ret.insert(ret.begin(), header.begin(), header.end());
  }
  else
  {
    ret = "; Invalid ELF file generated";
  }

  freeShader(out.pShaderBinary);

  return ret;
}
};
