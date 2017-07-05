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
#include "driver/shaders/dxbc/dxbc_inspect.h"
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
std::string LocatePlugin(const std::string &fileName);
};

static HMODULE GetAMDModule()
{
  // first try in the plugin locations
  HMODULE module = LoadLibraryA(GCNISA::LocatePlugin(DLL_NAME).c_str());

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
    module = LoadLibraryA(DLL_NAME);

  return module;
}

std::string GCNISA::Disassemble(const DXBC::DXBCFile *dxbc, const std::string &target)
{
  HMODULE mod = GetAMDModule();

  if(mod == NULL)
    return "; Error loading " DLL_NAME R"(.

; Currently atidxx64.dll from AMD's driver package is required for GCN disassembly and it cannot be
; distributed with RenderDoc.

; To see instructions on how to download and configure it on your system, go to:
; https://github.com/baldurk/renderdoc/wiki/GCN-ISA)";

  // if DXBC is NULL we're testing support, so return empty string - indicating no error
  // initialising
  if(dxbc == NULL || target == "")
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

  for(const asic &a : asicInfo)
  {
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

  in.pShaderByteCode = dxbc->m_HexDump.data();
  in.byteCodeLength = dxbc->m_HexDump.size();

  out.size = sizeof(out);

  compileShader(&in, &out);

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
      }
      else if(!amdil && !strcmp(name, ".disassembly"))
      {
        ret.insert(0, (const char *)data, (size_t)sectHeader->sh_size);
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
