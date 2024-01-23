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

#include "d3d11_test.h"

RD_TEST(D3D11_Shader_Linkage_Zoo, D3D11GraphicsTest)
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

  struct TestCase
  {
    ID3D11VertexShaderPtr vs;
    ID3D11PixelShaderPtr ps;
    ID3D11InputLayoutPtr inputLayout;
  };

  TestCase BuildTestCase(const std::vector<ShaderLinkageEntry> &elements)
  {
    ID3DBlobPtr vsblob = Compile(BuildVS(elements), "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(BuildPS(elements), "main", "ps_5_0");
    TestCase ret;
    ret.vs = CreateVS(vsblob);
    ret.ps = CreatePS(psblob);

    D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &ret.inputLayout));

    return ret;
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    ID3D11Texture2DPtr fltTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight).RTV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    std::vector<TestCase> tests;

    // No additional semantics
    tests.push_back(BuildTestCase({}));

    // A single semantic of various types, interpolation modes, and components
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{true, VarType::Float, 1, 0, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 4, 0, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 4, 0, "TEXCOORD0", false}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 0, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 4, 0, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 4, 0, "TEXCOORD0", false}}));
    tests.push_back(BuildTestCase({{true, VarType::UInt, 4, 0, "TEXCOORD0", true}}));

    // test semantics with indices that don't start from 0
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{true, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 4, 0, "TEXCOORD1", true}}));

    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    tests.push_back(BuildTestCase({{true, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 0, "TEXCOORD2", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 4, 0, "TEXCOORD2", true}}));

    // A single semantic with various array sizes
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 1, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 2, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 5, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 1, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 2, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 5, "TEXCOORD0", true}}));

    // Multiple semantics that pack together
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::UInt, 2, 0, "TEXCOORD0", true},
                                   {false, VarType::UInt, 2, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{true, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {true, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 3, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 3, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD1", true},
                                   {false, VarType::Float, 1, 0, "TEXCOORD2", true}}));
    // These pack into v1.x, v2.xy, and v1.y
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::UInt, 2, 0, "TEXCOORD1", true},
                                   {false, VarType::Float, 1, 0, "TEXCOORD2", true}}));

    // Multiple semantics that don't pack together
    tests.push_back(BuildTestCase({{false, VarType::Float, 3, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 3, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 4, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 1, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 4, 0, "TEXCOORD1", true}}));

    // Multiple semantics that will pack together "out of order" thanks to FXC's rules
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 3, 0, "TEXCOORD1", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD2", true}}));

    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 1, "TEXCOORD1", true},
                                   {false, VarType::Float, 3, 2, "TEXCOORD2", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD4", true}}));

    // Semantics that don't pack together due to being arrays
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 2, "TEXCOORD0", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 1, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {false, VarType::Float, 2, 1, "TEXCOORD1", true}}));

    // Tests focusing on different interpolation modes
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 0, "TEXCOORD0", true},
                                   {true, VarType::Float, 2, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v1.y since they share interpolation modes and types
    // (all int semantics are nointerpolation). Test that they don't get placed in v1.x and v2.x
    tests.push_back(BuildTestCase({{false, VarType::UInt, 1, 0, "TEXCOORD0", true},
                                   {true, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v2.x since their interpolation modes differ. Test that
    // they don't turn into an array[2] which would result in an erroneous interpolation mode for
    // one semantic or the other
    tests.push_back(BuildTestCase({{false, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));
    // These semantics are placed in v1.x and v1.y despite having different types since the
    // interpolation mode is the same. Test that they don't turn into an array[2] which would place
    // them in the wrong registers
    tests.push_back(BuildTestCase({{true, VarType::Float, 1, 0, "TEXCOORD0", true},
                                   {false, VarType::UInt, 1, 0, "TEXCOORD1", true}}));

    // Bespoke tests for broken scenarios discovered through bug reports:

    // These semantics live in v1.xy, v2.x, and v3.xyz due to each being an array. If any of them
    // are not treated as an array[1], they will incorrectly pack together with a previous semantic
    tests.push_back(BuildTestCase({{false, VarType::Float, 2, 1, "TEXCOORD0", true},
                                   {false, VarType::Float, 1, 1, "TEXCOORD1", false},
                                   {false, VarType::Float, 3, 1, "TEXCOORD2", true}}));

    while(Running())
    {
      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      for(size_t i = 0; i < tests.size(); ++i)
      {
        setMarker("draw" + std::to_string(i));

        ctx->IASetInputLayout(tests[i].inputLayout);

        ctx->VSSetShader(tests[i].vs, NULL, 0);
        ctx->PSSetShader(tests[i].ps, NULL, 0);

        ctx->Draw(3, 0);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
