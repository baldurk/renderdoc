/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2023 Baldur Karlsson
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

#include "d3d12_test.h"

RD_TEST(D3D12_Shader_Linkage_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests various shader linkage scenarios to ensure proper handling of data between shader "
      "stages.";

  enum class VarType : uint32_t
  {
    Float = 0,
    UInt = 1,
    Count = 2,
  };
  const std::string varTypeName[(uint32_t)VarType::Count] = {"float", "uint"};

  struct ShaderLinkageEntry
  {
    bool nointerp;
    VarType type;
    uint32_t components;
    uint32_t arraySize;
    std::string semantic;
    bool consumedByPS;
  };

  std::string BuildStruct(const std::vector<ShaderLinkageEntry> &outputs)
  {
    std::string structDef = R"EOSHADER(
struct v2f
{
  float4 pos : SV_POSITION;
)EOSHADER";

    for(size_t i = 0; i < outputs.size(); ++i)
    {
      structDef += "  ";
      if(outputs[i].nointerp)
        structDef += "nointerpolation ";
      structDef += varTypeName[(uint32_t)outputs[i].type];
      structDef += std::to_string(outputs[i].components);
      structDef += " ";
      structDef += "element" + std::to_string(i);
      if(outputs[i].arraySize != 0)
        structDef += "[" + std::to_string(outputs[i].arraySize) + "]";
      structDef += " : " + outputs[i].semantic + ";\n";
    }

    structDef += "};";
    return structDef;
  }

  std::string BuildVS(const std::vector<ShaderLinkageEntry> &outputs)
  {
    std::string vs = R"EOSHADER(
struct vertin
{
  float3 pos : POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};
)EOSHADER";

    vs += BuildStruct(outputs);

    vs += R"EOSHADER(

v2f main(vertin IN, uint vid : SV_VertexID)
{
  v2f OUT = (v2f)0;
  OUT.pos = float4(IN.pos, 1.0f);
)EOSHADER";

    float counterFloat = 0.0f;
    uint32_t counterUInt = 0;
    for(size_t i = 0; i < outputs.size(); ++i)
    {
      uint32_t count = std::max(1U, outputs[i].arraySize);
      for(uint32_t j = 0; j < count; ++j)
      {
        vs += "  OUT.element" + std::to_string(i);
        if(outputs[i].arraySize != 0)
          vs += "[" + std::to_string(j) + "]";
        vs += " = ";
        vs += varTypeName[(uint32_t)outputs[i].type];
        vs += std::to_string(outputs[i].components);
        vs += "(";
        for(uint32_t k = 0; k < outputs[i].components; ++k)
        {
          if(k != 0)
            vs += ", ";
          vs += std::to_string(outputs[i].type == VarType::Float ? counterFloat++ : counterUInt++);
        }
        vs += ");\n";
      }
    }

    vs += "\n  return OUT;\n}\n";

    return vs;
  }

  std::string BuildPS(const std::vector<ShaderLinkageEntry> &inputs)
  {
    std::string ps = BuildStruct(inputs);

    ps += R"EOSHADER(

float4 main(v2f IN) : SV_Target0
{
  float4 outF = float4(0.0f, 0.0f, 0.0f, 0.0f);
  uint4 outU = uint4(0, 0, 0, 0);

)EOSHADER";

    const std::string varAccess[] = {"  outF", "  outU"};
    const std::string componentAccess[] = {".x", ".xy", ".xyz", ".xyzw"};

    for(size_t i = 0; i < inputs.size(); ++i)
    {
      if(inputs[i].consumedByPS)
      {
        if(inputs[i].arraySize == 0)
        {
          ps += varAccess[(uint32_t)inputs[i].type];
          ps += componentAccess[inputs[i].components - 1];
          ps += " += IN.element" + std::to_string(i) + ";\n";
        }
        else
        {
          // Access each element
          for(uint32_t j = 0; j < inputs[i].arraySize; ++j)
          {
            ps += varAccess[(uint32_t)inputs[i].type];
            ps += componentAccess[inputs[i].components - 1];
            ps += " += IN.element" + std::to_string(i);
            ps += "[" + std::to_string(j) + "];\n";
          }
        }
      }
    }

    ps += "\n  return outF + (float4)outU;\n}\n";
    return ps;
  }

  ID3D12PipelineStatePtr BuildPSO(ID3D12RootSignaturePtr rootSig,
                                  const std::vector<ShaderLinkageEntry> &elements)
  {
    ID3DBlobPtr vsblob = Compile(BuildVS(elements), "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(BuildPS(elements), "main", "ps_5_0");
    ID3D12PipelineStatePtr pso = MakePSO().RootSig(rootSig).InputLayout().VS(vsblob).PS(psblob).RTVs(
        {DXGI_FORMAT_R32G32B32A32_FLOAT});
    return pso;
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);
    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12RootSignaturePtr sig = MakeSig({});

    std::vector<ID3D12PipelineStatePtr> psos;

    // No additional semantics
    psos.push_back(BuildPSO(sig, {}));

    // A single semantic of various types, interpolation modes, and components
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{true, VarType::Float, 1, 0, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 4, 0, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 4, 0, "TEXCOORD0", false}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 0, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 4, 0, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 4, 0, "TEXCOORD0", false}}));
    psos.push_back(BuildPSO(sig, {{true, VarType::UInt, 4, 0, "TEXCOORD0", true}}));

    // test semantics with indices that don't start from 0
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{true, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 4, 0, "TEXCOORD1", true}}));

    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    psos.push_back(BuildPSO(sig, {{true, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 0, "TEXCOORD2", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 4, 0, "TEXCOORD2", true}}));

    // A single semantic with various array sizes
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 1, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 2, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 5, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 1, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 2, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 5, "TEXCOORD0", true}}));

    // Multiple semantics that pack together
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 2, 0, "TEXCOORD0", true},
                                  {false, VarType::UInt, 2, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{true, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {true, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 3, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 3, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD1", true},
                                  {false, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    // These pack into v1.x, v2.xy, and v1.y
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::UInt, 2, 0, "TEXCOORD1", true},
                                  {false, VarType::Float, 1, 0, "TEXCOORD2", true}}));

    // Multiple semantics that don't pack together
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 3, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 3, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 4, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 4, 0, "TEXCOORD1", true}}));

    // Multiple semantics that will pack together "out of order" thanks to FXC's rules
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 3, 0, "TEXCOORD1", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD2", true}}));

    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 1, "TEXCOORD1", true},
                                  {false, VarType::Float, 3, 2, "TEXCOORD2", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD4", true}}));

    // Semantics that don't pack together due to being arrays
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 2, "TEXCOORD0", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 1, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {false, VarType::Float, 2, 1, "TEXCOORD1", true}}));

    // Tests focusing on different interpolation modes
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                  {true, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v1.y since they share interpolation modes and types
    // (all int semantics are nointerpolation). Test that they don't get placed in v1.x and v2.x
    psos.push_back(BuildPSO(sig, {{false, VarType::UInt, 1, 0, "TEXCOORD0", true},
                                  {true, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v2.x since their interpolation modes differ. Test that
    // they don't turn into an array[2] which would result in an erroneous interpolation mode for
    // one semantic or the other
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v1.y despite having different types since the
    // interpolation mode is the same. Test that they don't turn into an array[2] which would place
    // them in the wrong registers
    psos.push_back(BuildPSO(sig, {{true, VarType::Float, 1, 0, "TEXCOORD0", true},
                                  {false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));

    // Bespoke tests for broken scenarios discovered through bug reports:

    // These semantics live in v1.xy, v2.x, and v3.xyz due to each being an array. If any of them
    // are not treated as an array[1], they will incorrectly pack together with a previous semantic
    psos.push_back(BuildPSO(sig, {{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                  {false, VarType::Float, 1, 1, "TEXCOORD1", false},
                                  {false, VarType::Float, 3, 1, "TEXCOORD2", true}}));

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(1);

      OMSetRenderTargets(cmd, {offrtv}, {});
      ClearRenderTargetView(cmd, bbrtv, {0.4f, 0.5f, 0.6f, 1.0f});
      ClearRenderTargetView(cmd, offrtv, {0.4f, 0.5f, 0.6f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      cmd->SetGraphicsRootSignature(sig);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      for(size_t i = 0; i < psos.size(); ++i)
      {
        setMarker(cmd, "draw" + std::to_string(i));
        cmd->SetPipelineState(psos[i]);
        cmd->DrawInstanced(3, 1, 0, 0);
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
