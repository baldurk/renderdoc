/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2023 Baldur Karlsson
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

#include "dxbc_bytecode_editor.h"
#include "dxbc_container.h"

namespace DXBCBytecode
{
DXBCBytecode::Operation Edit::oper(OpcodeType o, const rdcarray<Operand> &operands)
{
  Operation ret;
  ret.operation = o;
  ret.operands = operands;

  // fxc doesn't like outputs that are selected, promote them to masked..
  if(!ret.operands.empty() && (ret.operands[0].flags & Operand::FLAG_SELECTED))
  {
    ret.operands[0].flags = Operand::FLAG_MASKED;
    ret.operands[0].numComponents = NUMCOMPS_4;
  }

  return ret;
}

ProgramEditor::ProgramEditor(const DXBC::DXBCContainer *container, bytebuf &outBlob)
    : Program(container->GetDXBCByteCode()->GetTokens()), m_OutBlob(outBlob)
{
  m_SM51 = (m_Major == 0x5 && m_Minor == 0x1);

  m_OutBlob = container->GetShaderBlob();

  DecodeProgram();
}

ProgramEditor::~ProgramEditor()
{
  rdcarray<uint32_t> encoded = EncodeProgram();
  // only one of these fourcc's will be present
  size_t dummy = 0;
  if(DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_SHEX, dummy))
    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, MAKE_FOURCC('S', 'H', 'E', 'X'), encoded);
  if(DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_SHDR, dummy))
    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, MAKE_FOURCC('S', 'H', 'D', 'R'), encoded);
}

/*

this is the rough order of declarations. Sometimes the order is different so it's *unlikely* that
the global order matters strongly but we keep to it where possible just in case.

E.g. geometry shaders declare outputs after temps, pixel shaders the other way around

*/

static const rdcarray<OpcodeType> opcodeOrder = {
    OPCODE_DCL_INPUT_CONTROL_POINT_COUNT,

    OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT,

    OPCODE_DCL_TESS_DOMAIN,

    OPCODE_DCL_TESS_PARTITIONING,

    OPCODE_DCL_TESS_OUTPUT_PRIMITIVE,

    OPCODE_DCL_HS_MAX_TESSFACTOR,

    OPCODE_DCL_GLOBAL_FLAGS,

    OPCODE_DCL_CONSTANT_BUFFER,

    OPCODE_DCL_SAMPLER,

    // sorted by space, then register. Types can be intermixed as a result
    OPCODE_DCL_RESOURCE,
    OPCODE_DCL_RESOURCE_RAW,
    OPCODE_DCL_RESOURCE_STRUCTURED,

    // sorted by space, then register. Types can be intermixed as a result
    OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED,
    OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW,
    OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED,

    OPCODE_DCL_FUNCTION_BODY,

    OPCODE_DCL_FUNCTION_TABLE,

    OPCODE_DCL_INTERFACE,

    // these can be mixed in signature order
    OPCODE_DCL_INPUT_PS,
    OPCODE_DCL_INPUT_PS_SIV,
    // last of the input group
    OPCODE_DCL_INPUT_PS_SGV,

    // these can in any order
    OPCODE_DCL_INPUT,
    OPCODE_DCL_INPUT_SIV,
    OPCODE_DCL_INPUT_SGV,

    // similarly in any order
    OPCODE_DCL_OUTPUT,
    OPCODE_DCL_OUTPUT_SIV,
    OPCODE_DCL_OUTPUT_SGV,

    OPCODE_DCL_TEMPS,

    OPCODE_DCL_INDEX_RANGE,

    OPCODE_DCL_INDEXABLE_TEMP,

    OPCODE_DCL_THREAD_GROUP,

    OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,

    OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW,

    OPCODE_DCL_GS_INPUT_PRIMITIVE,

    OPCODE_DCL_STREAM,

    // unknown
    OPCODE_DCL_GS_INSTANCE_COUNT,

    OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY,

    // geometry outputs here

    OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT,

    // these happen in the separate phase declarations
    OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT,
    OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT,
};

size_t ProgramEditor::GetDeclarationPosition(OpcodeType op)
{
  const int32_t opBoundary = opcodeOrder.indexOf(op);

  // find the first declaration after resources
  size_t i;
  for(i = 0; i < m_Declarations.size(); i++)
  {
    // resources go before these declarations
    int32_t idx = opcodeOrder.indexOf(m_Declarations[i].declaration);

    if(idx > opBoundary)
      break;
  }

  return i;
}

uint32_t ProgramEditor::AddTemp()
{
  for(Declaration &decl : m_Declarations)
  {
    if(decl.declaration == OPCODE_DCL_TEMPS)
    {
      uint32_t ret = decl.numTemps;
      decl.numTemps++;
      return ret;
    }
  }

  Declaration decl;
  decl.declaration = OPCODE_DCL_TEMPS;
  decl.numTemps = 1;

  m_Declarations.insert(GetDeclarationPosition(OPCODE_DCL_TEMPS), decl);

  return 0;
}

Edit::ResourceIdentifier ProgramEditor::DeclareResource(const ResourceDecl &desc, uint32_t space,
                                                        uint32_t regLow, uint32_t regHigh)
{
  Edit::ResourceIdentifier identifier = {0, ~0U};

  Declaration decl;

  decl.operand.type = TYPE_RESOURCE;

  if(m_SM51)
  {
    decl.space = space;
    decl.operand.indices.resize(3);

    // the next identifier is the one after all the UAVs, in SM51 they're assigned from 0...
    // because they don't correspond to binding registers and are effectively bytecode-local
    for(const Declaration &d : m_Declarations)
    {
      if(d.declaration == OPCODE_DCL_RESOURCE || d.declaration == OPCODE_DCL_RESOURCE_RAW ||
         d.declaration == OPCODE_DCL_RESOURCE_STRUCTURED)
        identifier.first++;
    }

    decl.operand.indices[0].absolute = true;
    decl.operand.indices[0].index = identifier.first;
    decl.operand.indices[1].absolute = true;
    decl.operand.indices[1].index = regLow;
    decl.operand.indices[2].absolute = true;
    decl.operand.indices[2].index = regHigh;

    identifier.second = regLow;
  }
  else
  {
    decl.operand.indices.resize(1);
    decl.operand.indices[0].absolute = true;
    decl.operand.indices[0].index = regLow;
    identifier.first = regLow;
  }

  if(desc.raw)
  {
    decl.declaration = OPCODE_DCL_RESOURCE_RAW;
  }
  else if(desc.structured)
  {
    decl.declaration = OPCODE_DCL_RESOURCE_STRUCTURED;

    decl.structured.stride = desc.stride;
  }
  else
  {
    decl.declaration = OPCODE_DCL_RESOURCE;

    decl.resource.sampleCount = 0;
    switch(desc.type)
    {
      case TextureType::Buffer: decl.resource.dim = RESOURCE_DIMENSION_BUFFER; break;
      case TextureType::Texture1D: decl.resource.dim = RESOURCE_DIMENSION_TEXTURE1D; break;
      case TextureType::Texture1DArray:
        decl.resource.dim = RESOURCE_DIMENSION_TEXTURE1DARRAY;
        break;
      case TextureType::Texture2D: decl.resource.dim = RESOURCE_DIMENSION_TEXTURE2D; break;
      case TextureType::Texture2DArray:
        decl.resource.dim = RESOURCE_DIMENSION_TEXTURE2DARRAY;
        break;
      case TextureType::Texture2DMS:
      case TextureType::Texture2DMSArray:
      {
        decl.resource.dim = desc.type == TextureType::Texture2DMSArray
                                ? RESOURCE_DIMENSION_TEXTURE2DMSARRAY
                                : RESOURCE_DIMENSION_TEXTURE2DMS;
        decl.resource.sampleCount = desc.sampleCount;
        break;
      }
      case TextureType::Texture3D: decl.resource.dim = RESOURCE_DIMENSION_TEXTURE3D; break;
      case TextureType::TextureCube: decl.resource.dim = RESOURCE_DIMENSION_TEXTURECUBE; break;
      case TextureType::TextureCubeArray:
        decl.resource.dim = RESOURCE_DIMENSION_TEXTURECUBEARRAY;
        break;
      default:
        RDCERR("Invalid texture type in declaration: %s", ToStr(desc.type).c_str());
        return {~0U, ~0U};
    }

    switch(desc.compType)
    {
      case CompType::Float: decl.resource.resType[0] = DXBC::RETURN_TYPE_FLOAT; break;
      case CompType::UNorm: decl.resource.resType[0] = DXBC::RETURN_TYPE_UNORM; break;
      case CompType::SNorm: decl.resource.resType[0] = DXBC::RETURN_TYPE_SNORM; break;
      case CompType::UInt: decl.resource.resType[0] = DXBC::RETURN_TYPE_UINT; break;
      case CompType::SInt: decl.resource.resType[0] = DXBC::RETURN_TYPE_SINT; break;
      default:
        RDCERR("Invalid component type in declaration: %s", ToStr(desc.compType).c_str());
        return {~0U, ~0U};
    }

    decl.resource.resType[1] = decl.resource.resType[0];
    decl.resource.resType[2] = decl.resource.resType[0];
    decl.resource.resType[3] = decl.resource.resType[0];
  }

  // add at the end of the resources. This may not preserve space/reg sorting depending on the
  // declared space or registers (but most likely we will always declare with a high space to not
  // stomp on the application's existing bindings)
  m_Declarations.insert(GetDeclarationPosition(OPCODE_DCL_RESOURCE_STRUCTURED), decl);

  return identifier;
}

Edit::ResourceIdentifier ProgramEditor::DeclareUAV(const ResourceDecl &desc, uint32_t space,
                                                   uint32_t regLow, uint32_t regHigh)
{
  Edit::ResourceIdentifier identifier = {0, ~0U};

  Declaration decl;

  decl.operand.type = TYPE_UNORDERED_ACCESS_VIEW;

  if(m_SM51)
  {
    decl.space = space;
    decl.operand.indices.resize(3);

    // the next identifier is the one after all the UAVs, in SM51 they're assigned from 0...
    // because they don't correspond to binding registers and are effectively bytecode-local
    for(const Declaration &d : m_Declarations)
    {
      if(d.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED ||
         d.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW ||
         d.declaration == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
        identifier.first++;
    }

    decl.operand.indices[0].absolute = true;
    decl.operand.indices[0].index = identifier.first;
    decl.operand.indices[1].absolute = true;
    decl.operand.indices[1].index = regLow;
    decl.operand.indices[2].absolute = true;
    decl.operand.indices[2].index = regHigh;

    identifier.second = regLow;
  }
  else
  {
    decl.operand.indices.resize(1);
    decl.operand.indices[0].absolute = true;
    decl.operand.indices[0].index = regLow;
    identifier.first = regLow;
  }

  if(desc.raw)
  {
    decl.declaration = OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW;

    decl.raw.rov = desc.rov;
    decl.raw.globallyCoherant = desc.globallyCoherant;
  }
  else if(desc.structured)
  {
    decl.declaration = OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED;

    decl.structured.stride = desc.stride;

    decl.structured.hasCounter = desc.hasCounter;
    decl.structured.rov = desc.rov;
    decl.structured.globallyCoherant = desc.globallyCoherant;
  }
  else
  {
    decl.declaration = OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED;

    decl.uav_typed.rov = desc.rov;
    decl.uav_typed.globallyCoherant = desc.globallyCoherant;

    switch(desc.type)
    {
      case TextureType::Buffer: decl.uav_typed.dim = RESOURCE_DIMENSION_BUFFER; break;
      case TextureType::Texture1D: decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURE1D; break;
      case TextureType::Texture1DArray:
        decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURE1DARRAY;
        break;
      case TextureType::Texture2D: decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURE2D; break;
      case TextureType::Texture2DArray:
        decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURE2DARRAY;
        break;
      case TextureType::Texture3D: decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURE3D; break;
      case TextureType::TextureCube: decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURECUBE; break;
      case TextureType::TextureCubeArray:
        decl.uav_typed.dim = RESOURCE_DIMENSION_TEXTURECUBEARRAY;
        break;
      default:
        RDCERR("Invalid texture type in declaration: %s", ToStr(desc.type).c_str());
        return {~0U, ~0U};
    }

    switch(desc.compType)
    {
      case CompType::Float: decl.uav_typed.resType[0] = DXBC::RETURN_TYPE_FLOAT; break;
      case CompType::UNorm: decl.uav_typed.resType[0] = DXBC::RETURN_TYPE_UNORM; break;
      case CompType::SNorm: decl.uav_typed.resType[0] = DXBC::RETURN_TYPE_SNORM; break;
      case CompType::UInt: decl.uav_typed.resType[0] = DXBC::RETURN_TYPE_UINT; break;
      case CompType::SInt: decl.uav_typed.resType[0] = DXBC::RETURN_TYPE_SINT; break;
      default:
        RDCERR("Invalid component type in declaration: %s", ToStr(desc.compType).c_str());
        return {~0U, ~0U};
    }

    decl.uav_typed.resType[1] = decl.uav_typed.resType[0];
    decl.uav_typed.resType[2] = decl.uav_typed.resType[0];
    decl.uav_typed.resType[3] = decl.uav_typed.resType[0];
  }

  // add at the end of the UAVs. This may not preserve space/reg sorting depending on the declared
  // space or registers (but most likely we will always declare with a high space to not stomp on
  // the application's existing bindings)
  m_Declarations.insert(GetDeclarationPosition(OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED), decl);

  return identifier;
}

};    // namespace DXBCBytecode

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

#include "common/formatting.h"
#include "driver/dx/official/d3dcompiler.h"
#include "dxbc_compile.h"

static bytebuf dxbccompile(const rdcstr &source, const rdcstr &profile = "ps_5_0",
                           uint32_t flags = D3DCOMPILE_OPTIMIZATION_LEVEL0 |
                                            D3DCOMPILE_SKIP_OPTIMIZATION)
{
  HMODULE d3dcompiler = GetD3DCompiler();

  if(!d3dcompiler)
    return {};

  pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

  if(compileFunc == NULL)
  {
    RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
  }

  HRESULT hr = S_OK;

  ID3DBlob *byteBlob = NULL;

  ID3DBlob *errBlob;
  hr = compileFunc(source.c_str(), source.size(), "main", NULL, NULL, "main", profile.c_str(),
                   flags, 0, &byteBlob, &errBlob);

  if(errBlob)
    RDCLOG("%s", (char *)errBlob->GetBufferPointer());

  REQUIRE(SUCCEEDED(hr));

  bytebuf bytecode;
  bytecode.assign((const byte *)byteBlob->GetBufferPointer(), byteBlob->GetBufferSize());

  SAFE_RELEASE(byteBlob);

  return bytecode;
}

static rdcstr make_source(const rdcstr &snippet)
{
  rdcstr source;
  if(snippet.contains("main("))
    source = snippet;
  else
    source = R"(
cbuffer cbuf : register(b0)
{
  float4 cbuf_float4;
  float cbuf_float;
  uint cbuf_uint;
};

SamplerState samp : register(s0);
Texture2D<float4> tex : register(t0);
Texture2D<float4> tex2 : register(t1);
Texture2D<float4> tex3 : register(t2);
Texture2D<float4> tex4 : register(t3);

float4 main(float input : INPUT) : SV_Target0
{
  float4 ret = input.xxxx;
)" + snippet +
             R"(
  return ret;
}
)";

  return source;
}

static int substrCount(const rdcstr &haystack, const rdcstr &needle)
{
  int ret = 0;
  int idx = 0;

  while(true)
  {
    idx = haystack.find(needle, idx);

    if(idx < 0)
      break;

    ret++;
    idx++;
  }

  return ret;
}

TEST_CASE("Test shader editing", "[dxbc]")
{
  SECTION("Test that making no changes means no binary changes")
  {
    // create snippets that affect the compilation since we don't have embedded source
    rdcarray<rdcstr> snippets = {
        R"(
)",
        R"(
ret.x = sin(ret.x);
)",
        R"(
ret.xy = cos(ret.zw * ret.xy);
)",
        R"(
ret.xy += sqrt(ret.z).xx;
)",
        R"(
ret.zw += tex.Load(ret.xyz).yz;
)",
    };

    for(rdcstr snippet : snippets)
    {
      for(rdcstr profile : {"ps_5_0", "ps_5_1"})
      {
        bytebuf bytecode = dxbccompile(make_source(snippet), profile);

        DXBC::DXBCContainer container(bytecode, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

        bytebuf edited;
        {
          DXBCBytecode::ProgramEditor editor(&container, edited);
        }

        CHECK(bytecode == edited);
      }
    }
  }

  SECTION("Test adding more temporary registers")
  {
    rdcarray<rdcstr> snippets = {
        // no temps at all
        R"(
	float4 main(float input : INPUT) : SV_Target0
	{
	  return input.xxxx;
	}
)",
        // one temp for ret
        R"(
)",
        // 2 temps, for ret and temp
        R"(
  float4 temp = sqrt(input);
  ret *= temp;
)",
    };

    for(size_t i = 0; i < snippets.size(); i++)
    {
      for(rdcstr profile : {"ps_5_0", "ps_5_1"})
      {
        bytebuf bytecode = dxbccompile(make_source(snippets[i]), profile);

        DXBC::DXBCContainer container(bytecode, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

        rdcstr disasm_before = container.GetDXBCByteCode()->GetDisassembly();
        uint32_t temp_index = 123456;

        bytebuf edited;
        {
          DXBCBytecode::ProgramEditor editor(&container, edited);

          temp_index = editor.AddTemp();
        }

        DXBC::DXBCContainer container2(edited, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

        rdcstr disasm_after = container2.GetDXBCByteCode()->GetDisassembly();

        CHECK(temp_index == (uint32_t)i);
        if(i == 0)
        {
          CHECK(!disasm_before.contains("dcl_temp"));
        }
        else
        {
          CHECK(disasm_before.contains(StringFormat::Fmt("dcl_temps %u", i)));
        }

        CHECK(disasm_after.contains(StringFormat::Fmt("dcl_temps %u", i + 1)));
      }
    }
  };

  SECTION("Test adding simple instructions")
  {
    for(rdcstr profile : {"ps_5_0", "ps_5_1"})
    {
      bytebuf bytecode = dxbccompile(make_source(""), profile);

      DXBC::DXBCContainer container(bytecode, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

      rdcstr disasm_before = container.GetDXBCByteCode()->GetDisassembly();
      uint32_t t = 123456;

      bytebuf edited;
      {
        DXBCBytecode::ProgramEditor editor(&container, edited);

        t = editor.AddTemp();

        CHECK(t == 1);

        using namespace DXBCBytecode;
        using namespace DXBCBytecode::Edit;

        // mov r1.x, v0.x
        editor.InsertOperation(0, oper(OPCODE_MOV, {temp(t).swizzle(0), input(0).swizzle(0)}));
        // sqrt r1.y, r1.x
        editor.InsertOperation(1, oper(OPCODE_SQRT, {temp(t).swizzle(1), temp(t).swizzle(0)}));
        // mul r1.z, r1.x, r1.x
        editor.InsertOperation(
            2, oper(OPCODE_MUL, {temp(t).swizzle(2), temp(t).swizzle(0), temp(t).swizzle(1)}));

        DXBCBytecode::Operation &op = editor.GetInstruction(3);

        REQUIRE(op.operands.size() == 2);
        CHECK(op.operands[1].type == TYPE_INPUT);

        // using reswizzle will mean that the mask will get applied - i.e. if the original operand
        // was
        // .xyz_ then we'll reswizzle .zzzz into .zzz_
        // in practice we know this shader reads from v0.xxxx but let's test that this works as
        // expected
        op.operands[1] = temp(t)
                             .swizzle(2, 2, 2, 2)
                             .reswizzle(op.operands[1].comps[0], op.operands[1].comps[1],
                                        op.operands[1].comps[2], op.operands[1].comps[3]);
      }

      DXBC::DXBCContainer container2(edited, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

      rdcstr disasm_after = container2.GetDXBCByteCode()->GetDisassembly();

      CHECK(!disasm_before.contains("sqrt "));
      CHECK(disasm_after.contains("mov r1.x, v0.x"));
      CHECK(disasm_after.contains("sqrt r1.y, r1.x"));
      CHECK(disasm_after.contains("mul r1.z, r1.x, r1.y"));

      CHECK(disasm_before.contains("mov r0.xyzw, v0.xxxx"));
      CHECK(disasm_after.contains("mov r0.xyzw, r1.zzzz"));
    }
  };

  SECTION("Test adding UAV and access instructions")
  {
    for(rdcstr profile : {"ps_5_0", "ps_5_1"})
    {
      bytebuf bytecode = dxbccompile(make_source(R"(
uint3 uvm = uint3(ret.xyz);
ret += tex.Load(uvm);
uvm += uint3(1,2,3);
ret += tex2.Load(uvm);
ret += tex3.Load(uvm);
ret += tex2.Load(uvm);
ret += tex4.Load(uvm);
ret += tex2.Load(uvm);
)"),
                                     profile);

      DXBC::DXBCContainer container(bytecode, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

      rdcstr disasm_before = container.GetDXBCByteCode()->GetDisassembly();

      bytebuf edited;
      {
        DXBCBytecode::ProgramEditor editor(&container, edited);

        uint32_t t = editor.AddTemp();

        DXBCBytecode::ResourceDecl desc;
        desc.compType = CompType::UInt;
        desc.type = TextureType::Buffer;
        desc.raw = true;

        DXBCBytecode::Edit::ResourceIdentifier u = editor.DeclareUAV(desc, 12, 7, 7);

        uint32_t texOffset[] = {1, 19, 235, 7534, 8294, 67952};

        for(size_t i = 0; i < editor.GetNumInstructions(); i++)
        {
          const DXBCBytecode::Operation &op = editor.GetInstruction(i);

          using namespace DXBCBytecode;
          using namespace DXBCBytecode::Edit;

          if(op.operation == OPCODE_LD)
          {
            Operand coords = op.operands[1];
            uint32_t texIndex = (uint32_t)op.operands[2].indices[0].index;

            // add the x coord to the y coord of the load
            editor.InsertOperation(
                i++, oper(OPCODE_IADD, {temp(t).swizzle(0), coords.swizzle(0), coords.swizzle(1)}));
            // add some value depending on which texture is being loaded from
            editor.InsertOperation(i++, oper(OPCODE_IADD, {temp(t).swizzle(0), temp(t).swizzle(0),
                                                           imm(texOffset[texIndex])}));
            editor.InsertOperation(i++,
                                   oper(OPCODE_ATOMIC_OR, {uav(u), temp(t).swizzle(0), imm(~0U)}));
          }
        }
      }

      DXBC::DXBCContainer container2(edited, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

      rdcstr disasm_after = container2.GetDXBCByteCode()->GetDisassembly();

      CHECK(substrCount(disasm_before, "ld_indexable") == 6);
      CHECK(substrCount(disasm_before, "atomic_or") == 0);
      CHECK(substrCount(disasm_before, "dcl_uav") == 0);
      CHECK(substrCount(disasm_before, "iadd") == 1);

      CHECK(substrCount(disasm_after, "ld_indexable") == 6);
      CHECK(substrCount(disasm_after, "atomic_or") == 6);
      CHECK(substrCount(disasm_after, "dcl_uav") == 1);
      CHECK(substrCount(disasm_after, "iadd") == 1 + 12);    // two per lookup

      CHECK(disasm_after.contains("iadd r4.x, r4.x, l(7534, 0, 0, 0)"));
    }
  };
}

#endif
