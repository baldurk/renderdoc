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
#include "common/common.h"
#include "common/formatting.h"
#include "core/plugins.h"
#include "official/RGA/Common/AmdDxGsaCompile.h"
#include "official/RGA/elf/elf32.h"
#include "official/RGA/elf/elf64.h"
#include "amd_isa_devices.h"

#if ENABLED(RDOC_X64)
#define DLL_NAME "atidxx64.dll"
#else
#define DLL_NAME "atidxx32.dll"
#endif

namespace GCNISA
{
extern rdcstr pluginPath;

static HMODULE GetAMDModule()
{
  // first try in the plugin locations
  HMODULE module = LoadLibraryA(LocatePluginFile(GCNISA::pluginPath, DLL_NAME).c_str());

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
    module = LoadLibraryA(DLL_NAME);

  return module;
}

HRESULT SafelyCompile(PfnAmdDxGsaCompileShader compileShader, AmdDxGsaCompileShaderInput &in,
                      AmdDxGsaCompileShaderOutput &out)
{
  HRESULT ret = E_FAIL;
  __try
  {
    ret = compileShader(&in, &out);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    RDCLOG("Exception occurred while compiling shader for ISA");
    out.pShaderBinary = NULL;
    out.shaderBinarySize = 0;
  }
  return ret;
}

template <typename Elf_Ehdr, typename Elf_Shdr>
static rdcstr ParseElf(const Elf_Ehdr *elfHeader, bool amdil, const rdcstr &target)
{
  rdcstr ret;

  const uint8_t *elf = (const uint8_t *)elfHeader;

  const Elf_Shdr *strtab =
      (const Elf_Shdr *)(elf + elfHeader->e_shoff + sizeof(Elf_Shdr) * elfHeader->e_shstrndx);

  const uint8_t *strtabData = elf + strtab->sh_offset;

  const AmdDxGsaCompileStats *stats = NULL;

  for(int section = 1; section < elfHeader->e_shnum; section++)
  {
    if(section == elfHeader->e_shstrndx)
      continue;

    const Elf_Shdr *sectHeader =
        (const Elf_Shdr *)(elf + elfHeader->e_shoff + sizeof(Elf_Shdr) * section);

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
    ret.insert(0, StringFormat::Fmt(
                      R"(; -------- Statistics ---------------------
; SGPRs: %u out of %u used
; VGPRs: %u out of %u used
; LDS: %u out of %u bytes used
; %u bytes scratch space used
; Instructions: %u ALU, %u Control Flow, %u TFETCH

)",
                      stats->numSgprsUsed, stats->availableSgprs, stats->numVgprsUsed,
                      stats->availableVgprs, stats->usedLdsBytes, stats->availableLdsBytes,
                      stats->usedScratchBytes, stats->numAluInst, stats->numControlFlowInst,
                      stats->numTfetchInst));
  }

  ret.insert(0, StringFormat::Fmt("; Disassembly for %s\n\n", target.c_str()));

  return ret;
}

static rdcstr ParseElf(const uint8_t *elf, bool amdil, const rdcstr &target)
{
  const Elf32_Ehdr *elf32Header = (const Elf32_Ehdr *)elf;

  // minimal code to extract data from ELF. We assume the ELF we got back is well-formed.
  if(IS_ELF(*elf32Header))
  {
    if(elf32Header->e_ident[EI_CLASS] == ELFCLASS32)
    {
      return ParseElf<Elf32_Ehdr, Elf32_Shdr>(elf32Header, amdil, target);
    }
    else if(elf32Header->e_ident[EI_CLASS] == ELFCLASS64)
    {
      return ParseElf<Elf64_Ehdr, Elf64_Shdr>((const Elf64_Ehdr *)elf, amdil, target);
    }
  }
  return "; Invalid ELF file generated";
}

rdcstr DisassembleDXBC(const bytebuf &shaderBytes, const rdcstr &target)
{
  HMODULE mod = GetAMDModule();

  if(mod == NULL)
    return "; Error loading " DLL_NAME R"(.

; Currently )" DLL_NAME R"( from AMD's driver package is required for GCN disassembly and it cannot be
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
    in.chipFamily = asicInfo[legacyAsicCount].chipFamily;
    in.chipRevision = asicInfo[legacyAsicCount].chipRevision;
    amdil = true;
  }

  if(in.chipFamily == 0)
    return "; Invalid ISA Target specified";

  // we do a little mini parse of the DXBC file, just enough to get the shader code out. This is
  // because we're getting called from outside the D3D backend where the shader bytes are opaque.

  const char *dxbcParseError =
      "; Failed to fetch D3D shader code from shader module, invalid DXBC container";

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

  rdcarray<uint32_t> chunkOffsets;
  for(uint32_t i = 0; i < numChunks; i++)
  {
    if(dxbc >= end)
      return dxbcParseError;

    chunkOffsets.push_back(*dxbc);
    dxbc++;
  }

  in.pShaderByteCode = NULL;
  in.byteCodeLength = 0;

  bool dxil = false;

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
    else if(*dxbc == MAKE_FOURCC('D', 'X', 'I', 'L') || *dxbc == MAKE_FOURCC('I', 'L', 'D', 'B'))
    {
      dxil = true;
    }
  }

  if(in.byteCodeLength == 0)
  {
    if(dxil)
      return "; Shader disassembly for DXIL shaders is not supported.";
    return dxbcParseError;
  }

  out.size = sizeof(out);

  HRESULT hr = SafelyCompile(compileShader, in, out);

  if(out.pShaderBinary == NULL || out.shaderBinarySize < 16)
  {
    RDCLOG("Failed to disassemble shader: %p/%zu (%s)", out.pShaderBinary, out.shaderBinarySize,
           ToStr(hr).c_str());
    return "; Failed to disassemble shader";
  }

  const rdcstr ret = ParseElf((const uint8_t *)out.pShaderBinary, amdil, target);

  freeShader(out.pShaderBinary);

  return ret;
}
};
