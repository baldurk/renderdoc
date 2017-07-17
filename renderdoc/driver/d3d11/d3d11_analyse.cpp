/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "data/resource.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"

#include "data/hlsl/debugcbuffers.h"

// over this number of cycles and things get problematic
#define SHADER_DEBUG_WARN_THRESHOLD 100000

bool PromptDebugTimeout(DXBC::ProgramType prog, uint32_t cycleCounter)
{
  string msg = StringFormat::Fmt(
      "RenderDoc's shader debugging has been running for over %u cycles, which indicates either a "
      "very long-running loop, or possibly an infinite loop. Continuing could lead to extreme "
      "memory allocations, slow UI or even crashes. Would you like to abort debugging to see what "
      "has run so far?\n\n"
      "Hit yes to abort debugging. Note that loading the resulting trace could take several "
      "minutes.",
      cycleCounter);

  int ret = MessageBoxA(NULL, msg.c_str(), "Shader debugging timeout", MB_YESNO | MB_ICONWARNING);

  if(ret == IDYES)
    return true;

  return false;
}

void D3D11DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                                             const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars,
                                             const vector<byte> &data)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  size_t o = offset;

  for(size_t v = 0; v < invars.size(); v++)
  {
    size_t vec = o + invars[v].descriptor.offset / 16;
    size_t comp = (invars[v].descriptor.offset - (invars[v].descriptor.offset & ~0xf)) / 4;
    size_t sz = RDCMAX(1U, invars[v].type.descriptor.bytesize / 16);

    offset = vec + sz;

    string basename = prefix + invars[v].name;

    uint32_t rows = invars[v].type.descriptor.rows;
    uint32_t cols = invars[v].type.descriptor.cols;
    uint32_t elems = RDCMAX(1U, invars[v].type.descriptor.elements);

    if(!invars[v].type.members.empty())
    {
      char buf[64] = {0};
      StringFormat::snprintf(buf, 63, "[%d]", elems);

      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = VarType::Float;

      std::vector<ShaderVariable> varmembers;

      if(elems > 1)
      {
        for(uint32_t i = 0; i < elems; i++)
        {
          StringFormat::snprintf(buf, 63, "[%d]", i);

          if(flatten)
          {
            FillCBufferVariables(basename + buf + ".", vec, flatten, invars[v].type.members,
                                 outvars, data);
          }
          else
          {
            ShaderVariable vr;
            vr.name = basename + buf;
            vr.rows = vr.columns = 0;
            vr.type = VarType::Float;

            std::vector<ShaderVariable> mems;

            FillCBufferVariables("", vec, flatten, invars[v].type.members, mems, data);

            vr.isStruct = true;

            vr.members = mems;

            varmembers.push_back(vr);
          }
        }

        var.isStruct = false;
      }
      else
      {
        var.isStruct = true;

        if(flatten)
          FillCBufferVariables(basename + ".", vec, flatten, invars[v].type.members, outvars, data);
        else
          FillCBufferVariables("", vec, flatten, invars[v].type.members, varmembers, data);
      }

      if(!flatten)
      {
        var.members = varmembers;
        outvars.push_back(var);
      }

      continue;
    }

    if(invars[v].type.descriptor.varClass == CLASS_OBJECT ||
       invars[v].type.descriptor.varClass == CLASS_STRUCT ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_CLASS ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_POINTER)
    {
      RDCWARN("Unexpected variable '%s' of class '%u' in cbuffer, skipping.",
              invars[v].name.c_str(), invars[v].type.descriptor.type);
      continue;
    }

    size_t elemByteSize = 4;
    VarType type = VarType::Float;
    switch(invars[v].type.descriptor.type)
    {
      case VARTYPE_INT: type = VarType::Int; break;
      case VARTYPE_FLOAT: type = VarType::Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8: type = VarType::UInt; break;
      case VARTYPE_DOUBLE:
        elemByteSize = 8;
        type = VarType::Double;
        break;
      default:
        RDCERR("Unexpected type %d for variable '%s' in cbuffer", invars[v].type.descriptor.type,
               invars[v].name.c_str());
    }

    bool columnMajor = invars[v].type.descriptor.varClass == CLASS_MATRIX_COLUMNS;

    size_t outIdx = vec;
    if(!flatten)
    {
      outIdx = outvars.size();
      outvars.resize(RDCMAX(outIdx + 1, outvars.size()));
    }
    else
    {
      if(columnMajor)
        outvars.resize(RDCMAX(outIdx + cols * elems, outvars.size()));
      else
        outvars.resize(RDCMAX(outIdx + rows * elems, outvars.size()));
    }

    size_t dataOffset = vec * sizeof(Vec4f) + comp * sizeof(float);

    if(outvars[outIdx].name.count > 0)
    {
      RDCASSERT(flatten);

      RDCASSERT(outvars[vec].rows == 1);
      RDCASSERT(outvars[vec].columns == comp);
      RDCASSERT(rows == 1);

      string combinedName = outvars[outIdx].name.elems;
      combinedName += ", " + basename;
      outvars[outIdx].name = combinedName;
      outvars[outIdx].rows = 1;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns += cols;

      if(dataOffset < data.size())
      {
        const byte *d = &data[dataOffset];

        memcpy(&outvars[outIdx].value.uv[comp], d,
               RDCMIN(data.size() - dataOffset, elemByteSize * cols));
      }
    }
    else
    {
      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = type;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns = cols;

      ShaderVariable &var = outvars[outIdx];

      bool isArray = invars[v].type.descriptor.elements > 1;

      if(rows * elems == 1)
      {
        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          memcpy(&outvars[outIdx].value.uv[flatten ? comp : 0], d,
                 RDCMIN(data.size() - dataOffset, elemByteSize * cols));
        }
      }
      else if(!isArray && !flatten)
      {
        outvars[outIdx].rows = rows;

        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          RDCASSERT(rows <= 4 && rows * cols <= 16);

          if(columnMajor)
          {
            uint32_t tmp[16] = {0};

            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t c = 0; c < cols; c++)
            {
              size_t srcoffs = 4 * elemByteSize * c;
              size_t dstoffs = rows * elemByteSize * c;
              memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * rows));
            }

            // transpose
            for(size_t r = 0; r < rows; r++)
              for(size_t c = 0; c < cols; c++)
                outvars[outIdx].value.uv[r * cols + c] = tmp[c * rows + r];
          }
          else    // CLASS_MATRIX_ROWS or other data not to transpose.
          {
            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t r = 0; r < rows; r++)
            {
              size_t srcoffs = 4 * elemByteSize * r;
              size_t dstoffs = cols * elemByteSize * r;
              memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * cols));
            }
          }
        }
      }
      else if(rows * elems > 1)
      {
        char buf[64] = {0};

        var.name = outvars[outIdx].name;

        vector<ShaderVariable> varmembers;
        vector<ShaderVariable> *out = &outvars;
        size_t rowCopy = 1;

        uint32_t registers = rows;
        uint32_t regLen = cols;
        const char *regName = "row";

        string base = outvars[outIdx].name.elems;

        if(!flatten)
        {
          var.rows = 0;
          var.columns = 0;
          outIdx = 0;
          out = &varmembers;
          varmembers.resize(elems);
          rowCopy = rows;
          rows = 1;
          registers = 1;
        }
        else
        {
          if(columnMajor)
          {
            registers = cols;
            regLen = rows;
            regName = "col";
          }
        }

        size_t rowDataOffset = vec * sizeof(Vec4f);

        for(size_t r = 0; r < registers * elems; r++)
        {
          if(isArray && registers > 1)
            StringFormat::snprintf(buf, 63, "[%d].%s%d", r / registers, regName, r % registers);
          else if(registers > 1)
            StringFormat::snprintf(buf, 63, ".%s%d", regName, r);
          else
            StringFormat::snprintf(buf, 63, "[%d]", r);

          (*out)[outIdx + r].name = base + buf;
          (*out)[outIdx + r].rows = (uint32_t)rowCopy;
          (*out)[outIdx + r].type = type;
          (*out)[outIdx + r].isStruct = false;
          (*out)[outIdx + r].columns = regLen;

          size_t totalSize = 0;

          if(flatten)
          {
            totalSize = elemByteSize * regLen;
          }
          else
          {
            // in a matrix, each major element before the last takes up a full
            // vec4 at least
            size_t vecSize = elemByteSize * 4;

            if(columnMajor)
              totalSize = vecSize * (cols - 1) + elemByteSize * rowCopy;
            else
              totalSize = vecSize * (rowCopy - 1) + elemByteSize * cols;
          }

          if((rowDataOffset % sizeof(Vec4f) != 0) &&
             (rowDataOffset / sizeof(Vec4f) != (rowDataOffset + totalSize) / sizeof(Vec4f)))
          {
            rowDataOffset = AlignUp(rowDataOffset, sizeof(Vec4f));
          }

          // arrays are also aligned to the nearest Vec4f for each element
          if(!flatten && isArray)
          {
            rowDataOffset = AlignUp(rowDataOffset, sizeof(Vec4f));
          }

          if(rowDataOffset < data.size())
          {
            const byte *d = &data[rowDataOffset];

            memcpy(&((*out)[outIdx + r].value.uv[0]), d,
                   RDCMIN(data.size() - rowDataOffset, totalSize));

            if(!flatten && columnMajor)
            {
              ShaderVariable tmp = (*out)[outIdx + r];

              size_t transposeRows = rowCopy > 1 ? 4 : 1;

              // transpose
              for(size_t ri = 0; ri < transposeRows; ri++)
                for(size_t ci = 0; ci < cols; ci++)
                  (*out)[outIdx + r].value.uv[ri * cols + ci] = tmp.value.uv[ci * transposeRows + ri];
            }
          }

          if(flatten)
          {
            rowDataOffset += sizeof(Vec4f);
          }
          else
          {
            if(columnMajor)
              rowDataOffset += sizeof(Vec4f) * (cols - 1) + sizeof(float) * rowCopy;
            else
              rowDataOffset += sizeof(Vec4f) * (rowCopy - 1) + sizeof(float) * cols;
          }
        }

        if(!flatten)
        {
          var.isStruct = false;
          var.members = varmembers;
        }
      }
    }
  }
}

void D3D11DebugManager::FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, bool flattenVec4s,
                                             const vector<byte> &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

ShaderDebug::State D3D11DebugManager::CreateShaderDebugState(ShaderDebugTrace &trace, int quadIdx,
                                                             DXBC::DXBCFile *dxbc,
                                                             vector<byte> *cbufData)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  State initialState = State(quadIdx, &trace, dxbc, m_WrappedDevice);

  // use pixel shader here to get inputs

  int32_t maxReg = -1;
  for(size_t i = 0; i < dxbc->m_InputSig.size(); i++)
    maxReg = RDCMAX(maxReg, (int32_t)dxbc->m_InputSig[i].regIndex);

  bool inputCoverage = false;

  for(size_t i = 0; i < dxbc->GetNumDeclarations(); i++)
  {
    const DXBC::ASMDecl &decl = dxbc->GetDeclaration(i);

    if(decl.declaration == OPCODE_DCL_INPUT && decl.operand.type == TYPE_INPUT_COVERAGE_MASK)
    {
      inputCoverage = true;
      break;
    }
  }

  if(maxReg >= 0 || inputCoverage)
  {
    create_array(trace.inputs, maxReg + 1 + (inputCoverage ? 1 : 0));
    for(size_t i = 0; i < dxbc->m_InputSig.size(); i++)
    {
      char buf[64] = {0};

      SigParameter &sig = dxbc->m_InputSig[i];

      StringFormat::snprintf(buf, 63, "v%d", sig.regIndex);

      ShaderVariable v;

      v.name = StringFormat::Fmt("%s (%s)", buf, sig.semanticIdxName.elems);
      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      if(sig.compType == CompType::UInt)
        v.type = VarType::UInt;
      else if(sig.compType == CompType::SInt)
        v.type = VarType::Int;

      if(trace.inputs[sig.regIndex].columns == 0)
        trace.inputs[sig.regIndex] = v;
      else
        trace.inputs[sig.regIndex].columns = RDCMAX(trace.inputs[sig.regIndex].columns, v.columns);
    }

    if(inputCoverage)
    {
      trace.inputs[maxReg + 1] = ShaderVariable("vCoverage", 0U, 0U, 0U, 0U);
      trace.inputs[maxReg + 1].columns = 1;
    }
  }

  uint32_t specialOutputs = 0;
  maxReg = -1;
  for(size_t i = 0; i < dxbc->m_OutputSig.size(); i++)
  {
    if(dxbc->m_OutputSig[i].regIndex == ~0U)
      specialOutputs++;
    else
      maxReg = RDCMAX(maxReg, (int32_t)dxbc->m_OutputSig[i].regIndex);
  }

  if(maxReg >= 0 || specialOutputs > 0)
  {
    create_array(initialState.outputs, maxReg + 1 + specialOutputs);
    for(size_t i = 0; i < dxbc->m_OutputSig.size(); i++)
    {
      SigParameter &sig = dxbc->m_OutputSig[i];

      if(sig.regIndex == ~0U)
        continue;

      char buf[64] = {0};

      StringFormat::snprintf(buf, 63, "o%d", sig.regIndex);

      ShaderVariable v;

      v.name = StringFormat::Fmt("%s (%s)", buf, sig.semanticIdxName.elems);
      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      if(initialState.outputs[sig.regIndex].columns == 0)
        initialState.outputs[sig.regIndex] = v;
      else
        initialState.outputs[sig.regIndex].columns =
            RDCMAX(initialState.outputs[sig.regIndex].columns, v.columns);
    }

    int32_t outIdx = maxReg + 1;

    for(size_t i = 0; i < dxbc->m_OutputSig.size(); i++)
    {
      SigParameter &sig = dxbc->m_OutputSig[i];

      if(sig.regIndex != ~0U)
        continue;

      ShaderVariable v;

      if(sig.systemValue == ShaderBuiltin::OutputControlPointIndex)
        v.name = "vOutputControlPointID";
      else if(sig.systemValue == ShaderBuiltin::DepthOutput)
        v.name = "oDepth";
      else if(sig.systemValue == ShaderBuiltin::DepthOutputLessEqual)
        v.name = "oDepthLessEqual";
      else if(sig.systemValue == ShaderBuiltin::DepthOutputGreaterEqual)
        v.name = "oDepthGreaterEqual";
      else if(sig.systemValue == ShaderBuiltin::MSAACoverage)
        v.name = "oMask";
      // if(sig.systemValue == TYPE_OUTPUT_CONTROL_POINT)							str = "oOutputControlPoint";
      else
      {
        RDCERR("Unhandled output: %s (%d)", sig.semanticName.c_str(), sig.systemValue);
        continue;
      }

      v.rows = 1;
      v.columns = sig.regChannelMask & 0x8 ? 4 : sig.regChannelMask & 0x4
                                                     ? 3
                                                     : sig.regChannelMask & 0x2
                                                           ? 2
                                                           : sig.regChannelMask & 0x1 ? 1 : 0;

      initialState.outputs[outIdx++] = v;
    }
  }

  create_array(trace.cbuffers, dxbc->m_CBuffers.size());
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    if(dxbc->m_CBuffers[i].descriptor.type != CBuffer::Descriptor::TYPE_CBUFFER)
      continue;

    vector<ShaderVariable> vars;

    FillCBufferVariables(dxbc->m_CBuffers[i].variables, vars, true,
                         cbufData[dxbc->m_CBuffers[i].reg]);

    trace.cbuffers[i] = vars;

    for(int32_t c = 0; c < trace.cbuffers[i].count; c++)
      trace.cbuffers[i][c].name = StringFormat::Fmt("cb%u[%u] (%s)", dxbc->m_CBuffers[i].reg,
                                                    (uint32_t)c, trace.cbuffers[i][c].name.elems);
  }

  initialState.Init();

  return initialState;
}

void D3D11DebugManager::CreateShaderGlobalState(ShaderDebug::GlobalState &global,
                                                DXBC::DXBCFile *dxbc, uint32_t UAVStartSlot,
                                                ID3D11UnorderedAccessView **UAVs,
                                                ID3D11ShaderResourceView **SRVs)
{
  for(int i = 0; UAVs != NULL && i + UAVStartSlot < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    int dsti = i + UAVStartSlot;
    if(UAVs[i])
    {
      ID3D11Resource *res = NULL;
      UAVs[i]->GetResource(&res);

      global.uavs[dsti].hiddenCounter = GetStructCount(UAVs[i]);

      D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
      UAVs[i]->GetDesc(&udesc);

      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

      if(udesc.Format != DXGI_FORMAT_UNKNOWN)
        format = udesc.Format;

      if(format == DXGI_FORMAT_UNKNOWN)
      {
        if(WrappedID3D11Texture1D::IsAlloc(res))
        {
          D3D11_TEXTURE1D_DESC desc;
          ((WrappedID3D11Texture1D *)res)->GetDesc(&desc);
          format = desc.Format;
        }
        else if(WrappedID3D11Texture2D1::IsAlloc(res))
        {
          D3D11_TEXTURE2D_DESC desc;
          ((WrappedID3D11Texture2D1 *)res)->GetDesc(&desc);
          format = desc.Format;
        }
        else if(WrappedID3D11Texture3D1::IsAlloc(res))
        {
          D3D11_TEXTURE3D_DESC desc;
          ((WrappedID3D11Texture3D1 *)res)->GetDesc(&desc);
          format = desc.Format;
        }
      }

      if(format != DXGI_FORMAT_UNKNOWN)
      {
        ResourceFormat fmt = MakeResourceFormat(GetTypedFormat(udesc.Format));

        global.uavs[dsti].format.byteWidth = fmt.compByteWidth;
        global.uavs[dsti].format.numComps = fmt.compCount;
        global.uavs[dsti].format.fmt = fmt.compType;

        if(udesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
          global.uavs[dsti].format.byteWidth = 11;
        if(udesc.Format == DXGI_FORMAT_R10G10B10A2_UINT ||
           udesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
          global.uavs[dsti].format.byteWidth = 10;
      }

      if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
      {
        global.uavs[dsti].firstElement = udesc.Buffer.FirstElement;
        global.uavs[dsti].numElements = udesc.Buffer.NumElements;
      }

      if(res)
      {
        if(WrappedID3D11Buffer::IsAlloc(res))
        {
          GetBufferData((ID3D11Buffer *)res, 0, 0, global.uavs[dsti].data);
        }
        else
        {
          global.uavs[dsti].tex = true;

          uint32_t &rowPitch = global.uavs[dsti].rowPitch;
          uint32_t &depthPitch = global.uavs[dsti].depthPitch;

          vector<byte> &data = global.uavs[dsti].data;

          if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D ||
             udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
          {
            D3D11_TEXTURE1D_DESC desc;
            ((WrappedID3D11Texture1D *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture1D *stagingTex = NULL;
            m_pDevice->CreateTexture1D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture1D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = 0;
            depthPitch = 0;
            size_t datasize = GetByteSize(desc.Width, 1, 1, desc.Format, udesc.Texture1D.MipSlice);

            uint32_t numSlices = 1;

            byte *srcdata = (byte *)mapped.pData;
            if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
            {
              rowPitch = mapped.RowPitch;
              srcdata += udesc.Texture1DArray.FirstArraySlice * rowPitch;
              numSlices = udesc.Texture1DArray.ArraySize;
              datasize = numSlices * rowPitch;
            }

            data.resize(datasize);

            // copy with all padding etc intact
            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture1D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
          else if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D ||
                  udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
          {
            D3D11_TEXTURE2D_DESC desc;
            ((WrappedID3D11Texture2D1 *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture2D *stagingTex = NULL;
            m_pDevice->CreateTexture2D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
            // use either
            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture2D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = mapped.RowPitch;
            depthPitch = 0;
            size_t datasize = rowPitch * desc.Height;

            uint32_t numSlices = 1;

            byte *srcdata = (byte *)mapped.pData;
            if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
            {
              depthPitch = mapped.DepthPitch;
              srcdata += udesc.Texture2DArray.FirstArraySlice * depthPitch;
              numSlices = udesc.Texture2DArray.ArraySize;
              datasize = numSlices * depthPitch;
            }

            // copy with all padding etc intact
            data.resize(datasize);

            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture2D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
          else if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
          {
            D3D11_TEXTURE3D_DESC desc;
            ((WrappedID3D11Texture3D1 *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture3D *stagingTex = NULL;
            m_pDevice->CreateTexture3D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
            // use either
            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture3D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = mapped.RowPitch;
            depthPitch = mapped.DepthPitch;

            byte *srcdata = (byte *)mapped.pData;
            srcdata += udesc.Texture3D.FirstWSlice * mapped.DepthPitch;
            uint32_t numSlices = udesc.Texture3D.WSize;
            size_t datasize = depthPitch * numSlices;

            data.resize(datasize);

            // copy with all padding etc intact
            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture3D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
        }
      }

      SAFE_RELEASE(res);
    }
  }

  for(int i = 0; SRVs != NULL && i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
  {
    if(SRVs[i])
    {
      ID3D11Resource *res = NULL;
      SRVs[i]->GetResource(&res);

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      SRVs[i]->GetDesc(&sdesc);

      if(sdesc.Format != DXGI_FORMAT_UNKNOWN)
      {
        ResourceFormat fmt = MakeResourceFormat(sdesc.Format);

        global.srvs[i].format.byteWidth = fmt.compByteWidth;
        global.srvs[i].format.numComps = fmt.compCount;
        global.srvs[i].format.fmt = fmt.compType;

        if(sdesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
          global.srvs[i].format.byteWidth = 11;
        if(sdesc.Format == DXGI_FORMAT_R10G10B10A2_UINT ||
           sdesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
          global.srvs[i].format.byteWidth = 10;
      }

      if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
      {
        // I know this isn't what the docs say, but as best as I can tell
        // this is how it's used.
        global.srvs[i].firstElement = sdesc.Buffer.FirstElement;
        global.srvs[i].numElements = sdesc.Buffer.NumElements;
      }
      else if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
      {
        global.srvs[i].firstElement = sdesc.BufferEx.FirstElement;
        global.srvs[i].numElements = sdesc.BufferEx.NumElements;
      }

      if(res)
      {
        if(WrappedID3D11Buffer::IsAlloc(res))
        {
          GetBufferData((ID3D11Buffer *)res, 0, 0, global.srvs[i].data);
        }
      }

      SAFE_RELEASE(res);
    }
  }

  for(size_t i = 0; i < dxbc->GetNumDeclarations(); i++)
  {
    const DXBC::ASMDecl &decl = dxbc->GetDeclaration(i);

    if(decl.declaration == DXBC::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW ||
       decl.declaration == DXBC::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
    {
      uint32_t slot = (uint32_t)decl.operand.indices[0].index;

      if(global.groupshared.size() <= slot)
      {
        global.groupshared.resize(slot + 1);

        ShaderDebug::GlobalState::groupsharedMem &mem = global.groupshared[slot];

        mem.structured = (decl.declaration == DXBC::OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED);

        mem.count = decl.count;
        if(mem.structured)
          mem.bytestride = decl.stride;
        else
          mem.bytestride = 4;    // raw groupshared is implicitly uint32s

        mem.data.resize(mem.bytestride * mem.count);
      }
    }
  }
}

// struct that saves pointers as we iterate through to where we ultimately
// want to copy the data to
struct DataOutput
{
  DataOutput(int regster, int element, int numWords, ShaderBuiltin attr, bool inc)
  {
    reg = regster;
    elem = element;
    numwords = numWords;
    sysattribute = attr;
    included = inc;
  }

  int reg;
  int elem;
  ShaderBuiltin sysattribute;

  int numwords;

  bool included;
};

struct DebugHit
{
  uint32_t numHits;
  float posx;
  float posy;
  float depth;
  uint32_t primitive;
  uint32_t isFrontFace;
  uint32_t sample;
  uint32_t coverage;
  uint32_t rawdata;    // arbitrary, depending on shader
};

ShaderDebugTrace D3D11DebugManager::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                                uint32_t idx, uint32_t instOffset,
                                                uint32_t vertOffset)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  D3D11MarkerRegion debugpixRegion(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventID, vertid, instid, idx));

  ShaderDebugTrace empty;

  const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(eventID);

  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11VertexShader *stateVS = NULL;
  m_pImmediateContext->VSGetShader(&stateVS, NULL, NULL);

  WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

  SAFE_RELEASE(stateVS);

  if(!vs)
    return empty;

  DXBCFile *dxbc = vs->GetDXBC();

  if(!dxbc)
    return empty;

  D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();

  vector<D3D11_INPUT_ELEMENT_DESC> inputlayout = m_WrappedDevice->GetLayoutDesc(rs->IA.Layout);

  set<UINT> vertexbuffers;
  uint32_t trackingOffs[32] = {0};

  UINT MaxStepRate = 1U;

  // need special handling for other step rates
  for(size_t i = 0; i < inputlayout.size(); i++)
  {
    if(inputlayout[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA &&
       inputlayout[i].InstanceDataStepRate < draw->numInstances)
      MaxStepRate = RDCMAX(inputlayout[i].InstanceDataStepRate, MaxStepRate);

    UINT slot =
        RDCCLAMP(inputlayout[i].InputSlot, 0U, UINT(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1));

    vertexbuffers.insert(slot);

    if(inputlayout[i].AlignedByteOffset == ~0U)
    {
      inputlayout[i].AlignedByteOffset = trackingOffs[slot];
    }
    else
    {
      trackingOffs[slot] = inputlayout[i].AlignedByteOffset;
    }

    ResourceFormat fmt = MakeResourceFormat(inputlayout[i].Format);

    trackingOffs[slot] += fmt.compByteWidth * fmt.compCount;
  }

  vector<byte> vertData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  vector<byte> *instData = new vector<byte>[MaxStepRate * D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  vector<byte> staticData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

  for(auto it = vertexbuffers.begin(); it != vertexbuffers.end(); ++it)
  {
    UINT i = *it;
    if(rs->IA.VBs[i])
    {
      GetBufferData(rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i] * (vertOffset + idx),
                    rs->IA.Strides[i], vertData[i]);

      for(UINT isr = 1; isr <= MaxStepRate; isr++)
      {
        GetBufferData(rs->IA.VBs[i],
                      rs->IA.Offsets[i] + rs->IA.Strides[i] * (instOffset + (instid / isr)),
                      rs->IA.Strides[i], instData[i * MaxStepRate + isr - 1]);
      }

      GetBufferData(rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i] * instOffset,
                    rs->IA.Strides[i], staticData[i]);
    }
  }

  vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->VS.ConstantBuffers[i])
      GetBufferData(rs->VS.ConstantBuffers[i], rs->VS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  ShaderDebugTrace ret;

  GlobalState global;
  CreateShaderGlobalState(global, dxbc, 0, NULL, rs->VS.SRVs);
  State initialState = CreateShaderDebugState(ret, -1, dxbc, cbufData);

  for(int32_t i = 0; i < ret.inputs.count; i++)
  {
    if(dxbc->m_InputSig[i].systemValue == ShaderBuiltin::Undefined ||
       dxbc->m_InputSig[i].systemValue ==
           ShaderBuiltin::Position)    // SV_Position seems to get promoted
                                       // automatically, but it's invalid for
                                       // vertex input
    {
      const D3D11_INPUT_ELEMENT_DESC *el = NULL;

      string signame = strlower(string(dxbc->m_InputSig[i].semanticName.elems));

      for(size_t l = 0; l < inputlayout.size(); l++)
      {
        string layoutname = strlower(string(inputlayout[l].SemanticName));

        if(signame == layoutname && dxbc->m_InputSig[i].semanticIndex == inputlayout[l].SemanticIndex)
        {
          el = &inputlayout[l];
          break;
        }
        if(signame == layoutname + ToStr::Get(inputlayout[l].SemanticIndex))
        {
          el = &inputlayout[l];
          break;
        }
      }

      RDCASSERT(el);

      if(!el)
        continue;

      byte *srcData = NULL;
      size_t dataSize = 0;

      if(el->InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA)
      {
        if(vertData[el->InputSlot].size() >= el->AlignedByteOffset)
        {
          srcData = &vertData[el->InputSlot][el->AlignedByteOffset];
          dataSize = vertData[el->InputSlot].size() - el->AlignedByteOffset;
        }
      }
      else
      {
        if(el->InstanceDataStepRate == 0 || el->InstanceDataStepRate >= draw->numInstances)
        {
          srcData = &staticData[el->InputSlot][el->AlignedByteOffset];
          dataSize = staticData[el->InputSlot].size() - el->AlignedByteOffset;
        }
        else
        {
          UINT isrIdx = el->InputSlot * MaxStepRate + (el->InstanceDataStepRate - 1);
          if(instData[isrIdx].size() >= el->AlignedByteOffset)
          {
            srcData = &instData[isrIdx][el->AlignedByteOffset];
            dataSize = instData[isrIdx].size() - el->AlignedByteOffset;
          }
        }
      }

      ResourceFormat fmt = MakeResourceFormat(el->Format);

      // more data needed than is provided
      if(dxbc->m_InputSig[i].compCount > fmt.compCount)
      {
        ret.inputs[i].value.u.w = 1;

        if(fmt.compType == CompType::Float)
          ret.inputs[i].value.f.w = 1.0f;
      }

      // interpret special formats
      if(fmt.special)
      {
        Vec3f *v3 = (Vec3f *)ret.inputs[i].value.fv;
        Vec4f *v4 = (Vec4f *)ret.inputs[i].value.fv;

        // only pull in all or nothing from these,
        // if there's only e.g. 3 bytes remaining don't read and unpack some of
        // a 4-byte special format
        size_t packedsize = 4;
        if(fmt.specialFormat == SpecialFormat::R5G5B5A1 ||
           fmt.specialFormat == SpecialFormat::R5G6B5 || fmt.specialFormat == SpecialFormat::R4G4B4A4)
          packedsize = 2;

        if(srcData == NULL || packedsize > dataSize)
        {
          ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
              ret.inputs[i].value.u.w = 0;
        }
        else if(fmt.specialFormat == SpecialFormat::R5G5B5A1)
        {
          RDCASSERT(fmt.bgraOrder);
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB5G5R5A1(packed);
        }
        else if(fmt.specialFormat == SpecialFormat::R5G6B5)
        {
          RDCASSERT(fmt.bgraOrder);
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v3 = ConvertFromB5G6R5(packed);
        }
        else if(fmt.specialFormat == SpecialFormat::R4G4B4A4)
        {
          RDCASSERT(fmt.bgraOrder);
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB4G4R4A4(packed);
        }
        else if(fmt.specialFormat == SpecialFormat::R10G10B10A2)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];

          if(fmt.compType == CompType::UInt)
          {
            ret.inputs[i].value.u.z = (packed >> 0) & 0x3ff;
            ret.inputs[i].value.u.y = (packed >> 10) & 0x3ff;
            ret.inputs[i].value.u.x = (packed >> 20) & 0x3ff;
            ret.inputs[i].value.u.w = (packed >> 30) & 0x003;
          }
          else
          {
            *v4 = ConvertFromR10G10B10A2(packed);
          }
        }
        else if(fmt.special && fmt.specialFormat == SpecialFormat::R11G11B10)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];
          *v3 = ConvertFromR11G11B10(packed);
        }
      }
      else
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          if(srcData == NULL || fmt.compByteWidth > dataSize)
          {
            ret.inputs[i].value.uv[c] = 0;
            continue;
          }

          dataSize -= fmt.compByteWidth;

          if(fmt.compByteWidth == 1)
          {
            byte *src = srcData + c * fmt.compByteWidth;

            if(fmt.compType == CompType::UInt)
              ret.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              ret.inputs[i].value.iv[c] = *((int8_t *)src);
            else if(fmt.compType == CompType::UNorm)
              ret.inputs[i].value.fv[c] = float(*src) / 255.0f;
            else if(fmt.compType == CompType::SNorm)
            {
              signed char *schar = (signed char *)src;

              // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
              if(*schar == -128)
                ret.inputs[i].value.fv[c] = -1.0f;
              else
                ret.inputs[i].value.fv[c] = float(*schar) / 127.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 2)
          {
            uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float)
              ret.inputs[i].value.fv[c] = ConvertFromHalf(*src);
            else if(fmt.compType == CompType::UInt)
              ret.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              ret.inputs[i].value.iv[c] = *((int16_t *)src);
            else if(fmt.compType == CompType::UNorm)
              ret.inputs[i].value.fv[c] = float(*src) / float(UINT16_MAX);
            else if(fmt.compType == CompType::SNorm)
            {
              int16_t *sint = (int16_t *)src;

              // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
              if(*sint == -32768)
                ret.inputs[i].value.fv[c] = -1.0f;
              else
                ret.inputs[i].value.fv[c] = float(*sint) / 32767.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 4)
          {
            uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
               fmt.compType == CompType::SInt)
              memcpy(&ret.inputs[i].value.uv[c], src, 4);
            else
              RDCERR("Unexpected component type");
          }
        }

        if(fmt.bgraOrder)
        {
          RDCASSERT(fmt.compCount == 4);
          std::swap(ret.inputs[i].value.fv[2], ret.inputs[i].value.fv[0]);
        }
      }
    }
    else if(dxbc->m_InputSig[i].systemValue == ShaderBuiltin::VertexIndex)
    {
      uint32_t sv_vertid = vertid;

      if(draw->flags & DrawFlags::UseIBuffer)
        sv_vertid = idx;

      if(dxbc->m_InputSig[i].compType == CompType::Float)
        ret.inputs[i].value.f.x = ret.inputs[i].value.f.y = ret.inputs[i].value.f.z =
            ret.inputs[i].value.f.w = (float)sv_vertid;
      else
        ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
            ret.inputs[i].value.u.w = sv_vertid;
    }
    else if(dxbc->m_InputSig[i].systemValue == ShaderBuiltin::InstanceIndex)
    {
      if(dxbc->m_InputSig[i].compType == CompType::Float)
        ret.inputs[i].value.f.x = ret.inputs[i].value.f.y = ret.inputs[i].value.f.z =
            ret.inputs[i].value.f.w = (float)instid;
      else
        ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
            ret.inputs[i].value.u.w = instid;
    }
    else
    {
      RDCERR("Unhandled system value semantic on VS input");
    }
  }

  delete[] instData;

  State last;

  vector<ShaderDebugState> states;

  states.push_back((State)initialState);

  D3D11MarkerRegion simloop("Simulation Loop");

  for(int cycleCounter = 0;; cycleCounter++)
  {
    if(initialState.Finished())
      break;

    initialState = initialState.GetNext(global, NULL);

    states.push_back((State)initialState);

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(DXBC::TYPE_VERTEX, cycleCounter))
        break;
    }
  }

  ret.states = states;

  return ret;
}

ShaderDebugTrace D3D11DebugManager::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y,
                                               uint32_t sample, uint32_t primitive)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  D3D11MarkerRegion debugpixRegion(
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventID, x, y, sample, primitive));

  ShaderDebugTrace empty;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11PixelShader *statePS = NULL;
  m_pImmediateContext->PSGetShader(&statePS, NULL, NULL);

  WrappedID3D11Shader<ID3D11PixelShader> *ps = (WrappedID3D11Shader<ID3D11PixelShader> *)statePS;

  SAFE_RELEASE(statePS);

  ID3D11GeometryShader *stateGS = NULL;
  m_pImmediateContext->GSGetShader(&stateGS, NULL, NULL);

  WrappedID3D11Shader<ID3D11GeometryShader> *gs =
      (WrappedID3D11Shader<ID3D11GeometryShader> *)stateGS;

  SAFE_RELEASE(stateGS);

  ID3D11DomainShader *stateDS = NULL;
  m_pImmediateContext->DSGetShader(&stateDS, NULL, NULL);

  WrappedID3D11Shader<ID3D11DomainShader> *ds = (WrappedID3D11Shader<ID3D11DomainShader> *)stateDS;

  SAFE_RELEASE(stateDS);

  ID3D11VertexShader *stateVS = NULL;
  m_pImmediateContext->VSGetShader(&stateVS, NULL, NULL);

  WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

  SAFE_RELEASE(stateVS);

  if(!ps)
    return empty;

  D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();

  DXBCFile *dxbc = ps->GetDXBC();

  if(!dxbc)
    return empty;

  DXBCFile *prevdxbc = NULL;

  if(prevdxbc == NULL && gs != NULL)
    prevdxbc = gs->GetDXBC();
  if(prevdxbc == NULL && ds != NULL)
    prevdxbc = ds->GetDXBC();
  if(prevdxbc == NULL && vs != NULL)
    prevdxbc = vs->GetDXBC();

  vector<DataOutput> initialValues;

  string extractHlsl = "struct PSInput\n{\n";

  int structureStride = 0;

  if(dxbc->m_InputSig.empty())
  {
    extractHlsl += "float4 input_dummy : SV_Position;\n";

    initialValues.push_back(DataOutput(-1, 0, 4, ShaderBuiltin::Undefined, true));

    structureStride += 4;
  }

  vector<string> floatInputs;
  vector<pair<string, pair<uint32_t, uint32_t> > >
      arrays;    // name, pair<start semantic index, end semantic index>

  uint32_t nextreg = 0;

  for(size_t i = 0; i < dxbc->m_InputSig.size(); i++)
  {
    extractHlsl += "  ";

    bool included = true;

    // handled specially to account for SV_ ordering
    if(dxbc->m_InputSig[i].systemValue == ShaderBuiltin::PrimitiveIndex ||
       dxbc->m_InputSig[i].systemValue == ShaderBuiltin::MSAACoverage ||
       dxbc->m_InputSig[i].systemValue == ShaderBuiltin::IsFrontFace ||
       dxbc->m_InputSig[i].systemValue == ShaderBuiltin::MSAASampleIndex)
    {
      extractHlsl += "//";
      included = false;
    }

    bool arrayElem = false;

    for(size_t a = 0; a < arrays.size(); a++)
    {
      if(arrays[a].first == dxbc->m_InputSig[i].semanticName.elems &&
         arrays[a].second.first <= dxbc->m_InputSig[i].semanticIndex &&
         arrays[a].second.second >= dxbc->m_InputSig[i].semanticIndex)
      {
        extractHlsl += "//";
        included = false;
        arrayElem = true;
      }
    }

    int missingreg = int(dxbc->m_InputSig[i].regIndex) - int(nextreg);

    // fill in holes from output sig of previous shader if possible, to try and
    // ensure the same register order
    for(int dummy = 0; dummy < missingreg; dummy++)
    {
      bool filled = false;

      if(prevdxbc)
      {
        for(size_t os = 0; os < prevdxbc->m_OutputSig.size(); os++)
        {
          if(prevdxbc->m_OutputSig[os].regIndex == nextreg + dummy)
          {
            filled = true;

            if(prevdxbc->m_OutputSig[os].compType == CompType::Float)
              extractHlsl += "float";
            else if(prevdxbc->m_OutputSig[os].compType == CompType::SInt)
              extractHlsl += "int";
            else if(prevdxbc->m_OutputSig[os].compType == CompType::UInt)
              extractHlsl += "uint";
            else
              RDCERR("Unexpected input signature type: %d", prevdxbc->m_OutputSig[os].compType);

            int numCols = (prevdxbc->m_OutputSig[os].regChannelMask & 0x1 ? 1 : 0) +
                          (prevdxbc->m_OutputSig[os].regChannelMask & 0x2 ? 1 : 0) +
                          (prevdxbc->m_OutputSig[os].regChannelMask & 0x4 ? 1 : 0) +
                          (prevdxbc->m_OutputSig[os].regChannelMask & 0x8 ? 1 : 0);

            structureStride += 4 * numCols;

            initialValues.push_back(DataOutput(-1, 0, numCols, ShaderBuiltin::Undefined, true));

            string name = prevdxbc->m_OutputSig[os].semanticIdxName.elems;

            extractHlsl += ToStr::Get((uint32_t)numCols) + " input_" + name + " : " + name + ";\n";
          }
        }
      }

      if(!filled)
      {
        string dummy_reg = "dummy_register";
        dummy_reg += ToStr::Get((uint32_t)nextreg + dummy);
        extractHlsl += "float4 var_" + dummy_reg + " : semantic_" + dummy_reg + ";\n";

        initialValues.push_back(DataOutput(-1, 0, 4, ShaderBuiltin::Undefined, true));

        structureStride += 4 * sizeof(float);
      }
    }

    nextreg = dxbc->m_InputSig[i].regIndex + 1;

    if(dxbc->m_InputSig[i].compType == CompType::Float)
      extractHlsl += "float";
    else if(dxbc->m_InputSig[i].compType == CompType::SInt)
      extractHlsl += "int";
    else if(dxbc->m_InputSig[i].compType == CompType::UInt)
      extractHlsl += "uint";
    else
      RDCERR("Unexpected input signature type: %d", dxbc->m_InputSig[i].compType);

    int numCols = (dxbc->m_InputSig[i].regChannelMask & 0x1 ? 1 : 0) +
                  (dxbc->m_InputSig[i].regChannelMask & 0x2 ? 1 : 0) +
                  (dxbc->m_InputSig[i].regChannelMask & 0x4 ? 1 : 0) +
                  (dxbc->m_InputSig[i].regChannelMask & 0x8 ? 1 : 0);

    if(included)
      structureStride += 4 * numCols;

    string name = dxbc->m_InputSig[i].semanticIdxName.elems;

    // arrays of interpolators are handled really weirdly. They use cbuffer
    // packing rules where each new value is in a new register (rather than
    // e.g. 2 x float2 in a single register), but that's pointless because
    // you can't dynamically index into input registers.
    // If we declare those elements as a non-array, the float2s or floats
    // will be packed into registers and won't match up to the previous
    // shader.
    // HOWEVER to add an extra bit of fun, fxc will happily pack other
    // parameters not in the array into spare parts of the registers.
    //
    // So I think the upshot is that we can detect arrays reliably by
    // whenever we encounter a float or float2 at the start of a register,
    // search forward to see if the next register has an element that is the
    // same semantic name and one higher semantic index. If so, there's an
    // array, so keep searching to enumerate its length.
    // I think this should be safe if the packing just happens to place those
    // registers together.

    int arrayLength = 0;

    if(included && numCols <= 2 && dxbc->m_InputSig[i].regChannelMask <= 0x3)
    {
      uint32_t nextIdx = dxbc->m_InputSig[i].semanticIndex + 1;

      for(size_t j = i + 1; j < dxbc->m_InputSig.size(); j++)
      {
        // if we've found the 'next' semantic
        if(!strcmp(dxbc->m_InputSig[i].semanticName.elems, dxbc->m_InputSig[j].semanticName.elems) &&
           nextIdx == dxbc->m_InputSig[j].semanticIndex)
        {
          int jNumCols = (dxbc->m_InputSig[i].regChannelMask & 0x1 ? 1 : 0) +
                         (dxbc->m_InputSig[i].regChannelMask & 0x2 ? 1 : 0) +
                         (dxbc->m_InputSig[i].regChannelMask & 0x4 ? 1 : 0) +
                         (dxbc->m_InputSig[i].regChannelMask & 0x8 ? 1 : 0);

          // if it's the same size, and it's at the start of the next register
          if(jNumCols == numCols && dxbc->m_InputSig[j].regChannelMask <= 0x3)
          {
            if(arrayLength == 0)
              arrayLength = 2;
            else
              arrayLength++;

            // continue searching now
            nextIdx++;
            j = i + 1;
            continue;
          }
        }
      }

      if(arrayLength > 0)
        arrays.push_back(
            std::make_pair(dxbc->m_InputSig[i].semanticName.elems,
                           std::make_pair(dxbc->m_InputSig[i].semanticIndex, nextIdx - 1)));
    }

    // as another side effect of the above, an element declared as a 1-length array won't be
    // detected but it WILL be put in its own register (not packed together), so detect this
    // case too.
    // Note we have to search *backwards* because we need to know if this register should have
    // been packed into the previous register, but wasn't. float/float2 can be packed after an
    // array just fine.
    if(included && i > 0 && arrayLength == 0 && numCols <= 2 &&
       dxbc->m_InputSig[i].regChannelMask <= 0x3)
    {
      const SigParameter &prev = dxbc->m_InputSig[i - 1];

      if(prev.compCount <= 2 && prev.regChannelMask <= 0x3)
        arrayLength = 1;
    }

    // The compiler is also really annoying and will go to great lengths to rearrange elements
    // and screw up our declaration, to pack things together. E.g.:
    // float2 a : TEXCOORD1;
    // float4 b : TEXCOORD2;
    // float4 c : TEXCOORD3;
    // float2 d : TEXCOORD4;
    // the compiler will move d up and pack it into the last two components of a.
    // To prevent this, we look forward and backward to check that we aren't expecting to pack
    // with anything, and if not then we just make it a 1-length array to ensure no packing.
    // Note the regChannelMask & 0x1 means it is using .x, so it's not the tail-end of a pack
    if(included && arrayLength == 0 && numCols <= 2 && (dxbc->m_InputSig[i].regChannelMask & 0x1))
    {
      if(i == dxbc->m_InputSig.size() - 1)
      {
        // the last element is never packed
        arrayLength = 1;
      }
      else
      {
        // if the next reg is using .x, it wasn't packed with us
        if(dxbc->m_InputSig[i + 1].regChannelMask & 0x1)
          arrayLength = 1;
      }
    }

    extractHlsl += ToStr::Get((uint32_t)numCols) + " input_" + name;
    if(arrayLength > 0)
      extractHlsl += "[" + ToStr::Get(arrayLength) + "]";
    extractHlsl += " : " + name;

    if(included && dxbc->m_InputSig[i].compType == CompType::Float)
    {
      if(arrayLength == 0)
      {
        floatInputs.push_back("input_" + name);
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
          floatInputs.push_back("input_" + name + "[" + ToStr::Get(a) + "]");
      }
    }

    extractHlsl += ";\n";

    int firstElem = dxbc->m_InputSig[i].regChannelMask & 0x1
                        ? 0
                        : dxbc->m_InputSig[i].regChannelMask & 0x2
                              ? 1
                              : dxbc->m_InputSig[i].regChannelMask & 0x4
                                    ? 2
                                    : dxbc->m_InputSig[i].regChannelMask & 0x8 ? 3 : -1;

    // arrays get added all at once (because in the struct data, they are contiguous even if
    // in the input signature they're not).
    if(!arrayElem)
    {
      if(arrayLength == 0)
      {
        initialValues.push_back(DataOutput(dxbc->m_InputSig[i].regIndex, firstElem, numCols,
                                           dxbc->m_InputSig[i].systemValue, included));
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
        {
          initialValues.push_back(DataOutput(dxbc->m_InputSig[i].regIndex + a, firstElem, numCols,
                                             dxbc->m_InputSig[i].systemValue, included));
        }
      }
    }
  }

  extractHlsl += "};\n\n";

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  uint32_t uavslot = 0;

  ID3D11DepthStencilView *depthView = NULL;
  ID3D11RenderTargetView *rtView = NULL;
  // preserve at least one render target and/or the depth view, so that
  // we have the right multisample level on output either way
  m_pImmediateContext->OMGetRenderTargets(1, &rtView, &depthView);
  if(rtView != NULL)
    uavslot = 1;

  extractHlsl +=
      "struct PSInitialData { uint hit; float3 pos; uint prim; uint fface; uint sample; uint "
      "covge; float derivValid; PSInput IN; PSInput INddx; PSInput INddy; PSInput INddxfine; "
      "PSInput INddyfine; };\n\n";
  extractHlsl += "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u" +
                 ToStr::Get(uavslot) + ");\n\n";
  extractHlsl +=
      "void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : "
      "SV_PrimitiveID, uint sample : SV_SampleIndex, uint covge : SV_Coverage, bool fface : "
      "SV_IsFrontFace)\n{\n";
  extractHlsl += "  uint idx = " + ToStr::Get(overdrawLevels) + ";\n";
  extractHlsl += "  if(abs(debug_pixelPos.x - " + ToStr::Get(x) +
                 ".5) < 0.5f && abs(debug_pixelPos.y - " + ToStr::Get(y) + ".5) < 0.5f)\n";
  extractHlsl += "    InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n\n";
  extractHlsl += "  idx = min(idx, " + ToStr::Get(overdrawLevels) + ");\n\n";
  extractHlsl += "  PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";
  extractHlsl += "  PSInitialBuffer[idx].prim = prim;\n";
  extractHlsl += "  PSInitialBuffer[idx].fface = fface;\n";
  extractHlsl += "  PSInitialBuffer[idx].covge = covge;\n";
  extractHlsl += "  PSInitialBuffer[idx].sample = sample;\n";
  extractHlsl += "  PSInitialBuffer[idx].IN = IN;\n";
  extractHlsl += "  PSInitialBuffer[idx].derivValid = ddx(debug_pixelPos.x);\n";
  extractHlsl += "  PSInitialBuffer[idx].INddx = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddy = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddxfine = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddyfine = (PSInput)0;\n";

  for(size_t i = 0; i < floatInputs.size(); i++)
  {
    const string &name = floatInputs[i];
    extractHlsl += "  PSInitialBuffer[idx].INddx." + name + " = ddx(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddy." + name + " = ddy(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddxfine." + name + " = ddx_fine(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddyfine." + name + " = ddy_fine(IN." + name + ");\n";
  }
  extractHlsl += "\n}";

  ID3D11PixelShader *extract = MakePShader(extractHlsl.c_str(), "ExtractInputsPS", "ps_5_0");

  uint32_t structStride = sizeof(uint32_t)       // uint hit;
                          + sizeof(float) * 3    // float3 pos;
                          + sizeof(uint32_t)     // uint prim;
                          + sizeof(uint32_t)     // uint fface;
                          + sizeof(uint32_t)     // uint sample;
                          + sizeof(uint32_t)     // uint covge;
                          + sizeof(float)        // float derivValid;
                          +
                          structureStride * 5;    // PSInput IN, INddx, INddy, INddxfine, INddyfine;

  HRESULT hr = S_OK;

  D3D11_BUFFER_DESC bdesc;
  bdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bdesc.CPUAccessFlags = 0;
  bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bdesc.Usage = D3D11_USAGE_DEFAULT;
  bdesc.StructureByteStride = structStride;
  bdesc.ByteWidth = bdesc.StructureByteStride * (overdrawLevels + 1);

  ID3D11Buffer *initialBuf = NULL;
  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &initialBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer %08x", hr);
    return empty;
  }

  bdesc.BindFlags = 0;
  bdesc.MiscFlags = 0;
  bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bdesc.Usage = D3D11_USAGE_STAGING;
  bdesc.StructureByteStride = 0;

  ID3D11Buffer *stageBuf = NULL;
  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &stageBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer %08x", hr);
    return empty;
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
  uavdesc.Format = DXGI_FORMAT_UNKNOWN;
  uavdesc.Buffer.FirstElement = 0;
  uavdesc.Buffer.Flags = 0;
  uavdesc.Buffer.NumElements = overdrawLevels + 1;
  uavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

  ID3D11UnorderedAccessView *initialUAV = NULL;
  hr = m_pDevice->CreateUnorderedAccessView(initialBuf, &uavdesc, &initialUAV);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer %08x", hr);
    return empty;
  }

  UINT zero = 0;
  m_pImmediateContext->ClearUnorderedAccessViewUint(initialUAV, &zero);

  UINT count = (UINT)-1;
  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(uavslot, &rtView, depthView,
                                                                 uavslot, 1, &initialUAV, &count);
  m_pImmediateContext->PSSetShader(extract, NULL, 0);

  SAFE_RELEASE(rtView);
  SAFE_RELEASE(depthView);

  {
    D3D11MarkerRegion initState("Replaying event for initial states");

    m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

    m_pImmediateContext->CopyResource(stageBuf, initialBuf);
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = m_pImmediateContext->Map(stageBuf, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map stage buff %08x", hr);
    return empty;
  }

  byte *initialData = new byte[bdesc.ByteWidth];
  memcpy(initialData, mapped.pData, bdesc.ByteWidth);

  m_pImmediateContext->Unmap(stageBuf, 0);

  SAFE_RELEASE(initialUAV);
  SAFE_RELEASE(initialBuf);
  SAFE_RELEASE(stageBuf);

  SAFE_RELEASE(extract);

  DebugHit *buf = (DebugHit *)initialData;

  D3D11MarkerRegion::Set(StringFormat::Fmt("Got %u hits", buf[0].numHits));

  if(buf[0].numHits == 0)
  {
    RDCLOG("No hit for this event");
    SAFE_DELETE_ARRAY(initialData);
    return empty;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // our debugging quad. Order is TL, TR, BL, BR
  State quad[4];

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

  vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->PS.ConstantBuffers[i])
      GetBufferData(rs->PS.ConstantBuffers[i], rs->PS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS;

  if(rs->OM.DepthStencilState)
  {
    D3D11_DEPTH_STENCIL_DESC desc;
    rs->OM.DepthStencilState->GetDesc(&desc);
    depthFunc = desc.DepthFunc;
  }

  DebugHit *winner = NULL;

  if(sample == ~0U)
    sample = 0;

  if(primitive != ~0U)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *hit = (DebugHit *)(initialData + i * structStride);

      if(hit->primitive == primitive && hit->sample == sample)
        winner = hit;
    }
  }

  if(winner == NULL)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *hit = (DebugHit *)(initialData + i * structStride);

      if(winner == NULL || (winner->sample != sample && hit->sample == sample) ||
         depthFunc == D3D11_COMPARISON_ALWAYS || depthFunc == D3D11_COMPARISON_NEVER ||
         depthFunc == D3D11_COMPARISON_NOT_EQUAL || depthFunc == D3D11_COMPARISON_EQUAL)
      {
        winner = hit;
        continue;
      }

      if((depthFunc == D3D11_COMPARISON_LESS && hit->depth < winner->depth) ||
         (depthFunc == D3D11_COMPARISON_LESS_EQUAL && hit->depth <= winner->depth) ||
         (depthFunc == D3D11_COMPARISON_GREATER && hit->depth > winner->depth) ||
         (depthFunc == D3D11_COMPARISON_GREATER_EQUAL && hit->depth >= winner->depth))
      {
        if(hit->sample == sample)
          winner = hit;
      }
    }
  }

  if(winner == NULL)
  {
    RDCLOG("Couldn't find any pixels that passed depth test at target co-ordinates");
    SAFE_DELETE_ARRAY(initialData);
    return empty;
  }

  ShaderDebugTrace traces[4];

  GlobalState global;
  CreateShaderGlobalState(global, dxbc, rs->OM.UAVStartSlot, rs->OM.UAVs, rs->PS.SRVs);

  {
    DebugHit *hit = winner;

    State initialState = CreateShaderDebugState(traces[destIdx], destIdx, dxbc, cbufData);

    rdctype::array<ShaderVariable> &ins = traces[destIdx].inputs;
    if(ins.count > 0 && !strcmp(ins[ins.count - 1].name.elems, "vCoverage"))
      ins[ins.count - 1].value.u.x = hit->coverage;

    initialState.semantics.coverage = hit->coverage;
    initialState.semantics.primID = hit->primitive;
    initialState.semantics.isFrontFace = hit->isFrontFace;

    uint32_t *data = &hit->rawdata;

    float *ddx = (float *)data;

    // ddx(SV_Position.x) MUST be 1.0
    if(*ddx != 1.0f)
    {
      RDCERR("Derivatives invalid");
      SAFE_DELETE_ARRAY(initialData);
      return empty;
    }

    data++;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      int32_t *rawout = NULL;

      if(initialValues[i].reg >= 0)
      {
        ShaderVariable &invar = traces[destIdx].inputs[initialValues[i].reg];

        if(initialValues[i].sysattribute == ShaderBuiltin::PrimitiveIndex)
        {
          invar.value.u.x = hit->primitive;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAASampleIndex)
        {
          invar.value.u.x = hit->sample;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAACoverage)
        {
          invar.value.u.x = hit->coverage;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::IsFrontFace)
        {
          invar.value.u.x = hit->isFrontFace ? ~0U : 0;
        }
        else
        {
          rawout = &invar.value.iv[initialValues[i].elem];

          memcpy(rawout, data, initialValues[i].numwords * 4);
        }
      }

      if(initialValues[i].included)
        data += initialValues[i].numwords;
    }

    for(int i = 0; i < 4; i++)
    {
      if(i != destIdx)
        traces[i] = traces[destIdx];
      quad[i] = initialState;
      quad[i].SetTrace(i, &traces[i]);
      if(i != destIdx)
        quad[i].SetHelper();
    }

    // We make the assumption that the coarse derivatives are
    // generated from (0,0) in the quad, and fine derivatives
    // are generated from the destination index and its
    // neighbours in X and Y.
    // This isn't spec'd but we must assume something and this
    // will hopefully get us closest to reproducing actual
    // results.
    //
    // For debugging, we need members of the quad to be able
    // to generate coarse and fine derivatives.
    //
    // For (0,0) we only need the coarse derivatives to get
    // our neighbours (1,0) and (0,1) which will give us
    // coarse and fine derivatives being identical.
    //
    // For the others we will need to use a combination of
    // coarse and fine derivatives to get the diagonal element
    // in the quad. E.g. for (1,1):
    //
    // (1,0) = (1,1) - ddx_fine
    // (0,1) = (1,1) - ddy_fine
    // (0,0) = (1,1) - ddy_fine - ddx_coarse
    //
    // This only works if coarse and fine are calculated as we
    // are assuming.

    ddx = (float *)data;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      if(!initialValues[i].included)
        continue;

      if(initialValues[i].reg >= 0)
      {
        if(destIdx == 0)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddx[w];
            traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddx[w];
          }
        }
        else if(destIdx == 1)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddx[w];
            traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddx[w];
          }
        }
        else if(destIdx == 2)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddx[w];
          }
        }
        else if(destIdx == 3)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddx[w];
          }
        }
      }

      ddx += initialValues[i].numwords;
    }

    float *ddy = ddx;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      if(!initialValues[i].included)
        continue;

      if(initialValues[i].reg >= 0)
      {
        if(destIdx == 0)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddy[w];
            traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddy[w];
          }
        }
        else if(destIdx == 1)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddy[w];
          }
        }
        else if(destIdx == 2)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddy[w];
            traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddy[w];
          }
        }
      }

      ddy += initialValues[i].numwords;
    }

    float *ddxfine = ddy;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      if(!initialValues[i].included)
        continue;

      if(initialValues[i].reg >= 0)
      {
        if(destIdx == 2)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddxfine[w];
          }
        }
        else if(destIdx == 3)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[2].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddxfine[w];
          }
        }
      }

      ddxfine += initialValues[i].numwords;
    }

    float *ddyfine = ddxfine;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      if(!initialValues[i].included)
        continue;

      if(initialValues[i].reg >= 0)
      {
        if(destIdx == 1)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[3].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] += ddyfine[w];
          }
        }
        else if(destIdx == 3)
        {
          for(int w = 0; w < initialValues[i].numwords; w++)
          {
            traces[1].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddyfine[w];
            traces[0].inputs[initialValues[i].reg].value.fv[initialValues[i].elem + w] -= ddyfine[w];
          }
        }
      }

      ddyfine += initialValues[i].numwords;
    }
  }

  SAFE_DELETE_ARRAY(initialData);

  vector<ShaderDebugState> states;

  states.push_back((State)quad[destIdx]);

  // ping pong between so that we can have 'current' quad to update into new one
  State quad2[4];

  State *curquad = quad;
  State *newquad = quad2;

  // marks any threads stalled waiting for others to catch up
  bool activeMask[4] = {true, true, true, true};

  int cycleCounter = 0;

  D3D11MarkerRegion simloop("Simulation Loop");

  // simulate lockstep until all threads are finished
  bool finished = true;
  do
  {
    for(size_t i = 0; i < 4; i++)
    {
      if(activeMask[i])
        newquad[i] = curquad[i].GetNext(global, curquad);
      else
        newquad[i] = curquad[i];
    }

    State *a = curquad;
    curquad = newquad;
    newquad = a;

    // if our destination quad is paused don't record multiple identical states.
    if(activeMask[destIdx])
      states.push_back((State)curquad[destIdx]);

    // we need to make sure that control flow which converges stays in lockstep so that
    // derivatives are still valid. While diverged, we don't have to keep threads in lockstep
    // since using derivatives is invalid.

    // Threads diverge either in ifs, loops, or switches. Due to the nature of the bytecode,
    // all threads *must* pass through the same exit instruction for each, there's no jumping
    // around with gotos. Note also for the same reason, the only time threads are on earlier
    // instructions is if they are still catching up to a thread that has exited the control
    // flow.

    // So the scheme is as follows:
    // * If all threads have the same nextInstruction, just continue we are still in lockstep.
    // * If threads are out of lockstep, find any thread which has nextInstruction pointing
    //   immediately *after* an ENDIF, ENDLOOP or ENDSWITCH. Pointing directly at one is not
    //   an indication the thread is done, as the next step for an ENDLOOP will jump back to
    //   the matching LOOP and continue iterating.
    // * Pause any thread matching the above until all threads are pointing to the same
    //   instruction. By the assumption above, all threads will eventually pass through this
    //   terminating instruction so we just pause any other threads and don't do anything
    //   until the control flow has converged and we can continue stepping in lockstep.

    // mark all threads as active again.
    // if we've converged, or we were never diverged, this keeps everything ticking
    activeMask[0] = activeMask[1] = activeMask[2] = activeMask[3] = true;

    if(curquad[0].nextInstruction != curquad[1].nextInstruction ||
       curquad[0].nextInstruction != curquad[2].nextInstruction ||
       curquad[0].nextInstruction != curquad[3].nextInstruction)
    {
      // this isn't *perfect* but it will still eventually continue. We look for the most
      // advanced thread, and check to see if it's just finished a control flow. If it has
      // then we assume it's at the convergence point and wait for every other thread to
      // catch up, pausing any threads that reach the convergence point before others.

      // Note this might mean we don't have any threads paused even within divergent flow.
      // This is fine and all we care about is pausing to make sure threads don't run ahead
      // into code that should be lockstep. We don't care at all about what they do within
      // the code that is divergent.

      // The reason this isn't perfect is that the most advanced thread could be on an
      // inner loop or inner if, not the convergence point, and we could be pausing it
      // fruitlessly. Worse still - it could be on a branch none of the other threads will
      // take so they will never reach that exact instruction.
      // But we know that all threads will eventually go through the convergence point, so
      // even in that worst case if we didn't pick the right waiting point, another thread
      // will overtake and become the new most advanced thread and the previous waiting
      // thread will resume. So in this case we caused a thread to wait more than it should
      // have but that's not a big deal as it's within divergent flow so they don't have to
      // stay in lockstep. Also if all threads will eventually pass that point we picked,
      // we just waited to converge even in technically divergent code which is also
      // harmless.

      // Phew!

      uint32_t convergencePoint = 0;

      // find which thread is most advanced
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction > convergencePoint)
          convergencePoint = curquad[i].nextInstruction;

      if(convergencePoint > 0)
      {
        OpcodeType op = dxbc->GetInstruction(convergencePoint - 1).operation;

        // if the most advnaced thread hasn't just finished control flow, then all
        // threads are still running, so don't converge
        if(op != OPCODE_ENDIF && op != OPCODE_ENDLOOP && op != OPCODE_ENDSWITCH)
          convergencePoint = 0;
      }

      // pause any threads at that instruction (could be none)
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction == convergencePoint)
          activeMask[i] = false;
    }

    finished = curquad[destIdx].Finished();

    cycleCounter++;

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(DXBC::TYPE_VERTEX, cycleCounter))
        break;
    }
  } while(!finished);

  traces[destIdx].states = states;

  return traces[destIdx];
}

ShaderDebugTrace D3D11DebugManager::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                                const uint32_t threadid[3])
{
  using namespace DXBC;
  using namespace ShaderDebug;

  D3D11MarkerRegion simloop(StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)",
                                              eventID, groupid[0], groupid[1], groupid[2],
                                              threadid[0], threadid[1], threadid[2]));

  ShaderDebugTrace empty;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11ComputeShader *stateCS = NULL;
  m_pImmediateContext->CSGetShader(&stateCS, NULL, NULL);

  WrappedID3D11Shader<ID3D11ComputeShader> *cs = (WrappedID3D11Shader<ID3D11ComputeShader> *)stateCS;

  SAFE_RELEASE(stateCS);

  if(!cs)
    return empty;

  DXBCFile *dxbc = cs->GetDXBC();

  if(!dxbc)
    return empty;

  D3D11RenderState *rs = m_WrappedContext->GetCurrentPipelineState();

  vector<byte> cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->CS.ConstantBuffers[i])
      GetBufferData(rs->CS.ConstantBuffers[i], rs->CS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  ShaderDebugTrace ret;

  GlobalState global;
  CreateShaderGlobalState(global, dxbc, 0, rs->CSUAVs, rs->CS.SRVs);
  State initialState = CreateShaderDebugState(ret, -1, dxbc, cbufData);

  for(int i = 0; i < 3; i++)
  {
    initialState.semantics.GroupID[i] = groupid[i];
    initialState.semantics.ThreadID[i] = threadid[i];
  }

  vector<ShaderDebugState> states;

  states.push_back((State)initialState);

  for(int cycleCounter = 0;; cycleCounter++)
  {
    if(initialState.Finished())
      break;

    initialState = initialState.GetNext(global, NULL);

    states.push_back((State)initialState);

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(DXBC::TYPE_VERTEX, cycleCounter))
        break;
    }
  }

  ret.states = states;

  return ret;
}

uint32_t D3D11DebugManager::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x,
                                       uint32_t y)
{
  if(cfg.position.numVerts == 0)
    return ~0U;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  struct MeshPickData
  {
    Vec3f RayPos;
    uint32_t PickIdx;

    Vec3f RayDir;
    uint32_t PickNumVerts;

    Vec2f PickCoords;
    Vec2f PickViewport;

    uint32_t MeshMode;
    uint32_t PickUnproject;
    Vec2f Padding;

    Matrix4f PickMVP;

  } cbuf;

  cbuf.PickCoords = Vec2f((float)x, (float)y);
  cbuf.PickViewport = Vec2f((float)GetWidth(), (float)GetHeight());
  cbuf.PickIdx = cfg.position.idxByteWidth ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numVerts;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth()) / float(GetHeight()));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f pickMVP = projMat.Mul(camMat);

  ResourceFormat resFmt;
  resFmt.compByteWidth = cfg.position.compByteWidth;
  resFmt.compCount = cfg.position.compCount;
  resFmt.compType = cfg.position.compType;
  resFmt.special = false;
  if(cfg.position.specialFormat != SpecialFormat::Unknown)
  {
    resFmt.special = true;
    resFmt.specialFormat = cfg.position.specialFormat;
  }

  Matrix4f pickMVPProj;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    pickMVPProj = projMat.Mul(camMat.Mul(guessProj.Inverse()));
  }

  Vec3f rayPos;
  Vec3f rayDir;
  // convert mouse pos to world space ray
  {
    Matrix4f inversePickMVP = pickMVP.Inverse();

    float pickX = ((float)x) / ((float)GetWidth());
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)GetHeight());
    // flip the Y axis
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    Vec3f cameraToWorldNearPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

    Vec3f cameraToWorldFarPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

    Vec3f testDir = (cameraToWorldFarPosition - cameraToWorldNearPosition);
    testDir.Normalise();

    // Calculate the ray direction first in the regular way (above), so we can use the
    // the output for testing if the ray we are picking is negative or not. This is similar
    // to checking against the forward direction of the camera, but more robust
    if(cfg.position.unproject)
    {
      Matrix4f inversePickMVPGuess = pickMVPProj.Inverse();

      Vec3f nearPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

      Vec3f farPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else
    {
      rayDir = testDir;
      rayPos = cameraToWorldNearPosition;
    }
  }

  cbuf.RayPos = rayPos;
  cbuf.RayDir = rayDir;

  cbuf.PickMVP = cfg.position.unproject ? pickMVPProj : pickMVP;

  bool isTriangleMesh = true;
  switch(cfg.position.topo)
  {
    case Topology::TriangleList:
    {
      cbuf.MeshMode = MESH_TRIANGLE_LIST;
      break;
    }
    case Topology::TriangleStrip:
    {
      cbuf.MeshMode = MESH_TRIANGLE_STRIP;
      break;
    }
    case Topology::TriangleList_Adj:
    {
      cbuf.MeshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    }
    case Topology::TriangleStrip_Adj:
    {
      cbuf.MeshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    }
    default:    // points, lines, patchlists, unknown
    {
      cbuf.MeshMode = MESH_OTHER;
      isTriangleMesh = false;
    }
  }

  ID3D11Buffer *vb = NULL, *ib = NULL;
  DXGI_FORMAT ifmt = cfg.position.idxByteWidth == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  {
    auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.buf);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      vb = it->second.m_Buffer;

    it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.idxbuf);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      ib = it->second.m_Buffer;
  }

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers.
  // In the case of VB we also tightly pack and unpack the data. IB can just be
  // read as R16 or R32 via the SRV so it is just a straight copy

  if(cfg.position.idxByteWidth)
  {
    // resize up on demand
    if(m_DebugRender.PickIBBuf == NULL ||
       m_DebugRender.PickIBSize < cfg.position.numVerts * cfg.position.idxByteWidth)
    {
      SAFE_RELEASE(m_DebugRender.PickIBBuf);
      SAFE_RELEASE(m_DebugRender.PickIBSRV);

      D3D11_BUFFER_DESC desc = {cfg.position.numVerts * cfg.position.idxByteWidth,
                                D3D11_USAGE_DEFAULT,
                                D3D11_BIND_SHADER_RESOURCE,
                                0,
                                0,
                                0};

      m_DebugRender.PickIBSize = cfg.position.numVerts * cfg.position.idxByteWidth;

      hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickIBBuf);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PickIBBuf %08x", hr);
        return ~0U;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      sdesc.Format = ifmt;
      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = cfg.position.numVerts;

      hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickIBBuf, &sdesc,
                                               &m_DebugRender.PickIBSRV);

      if(FAILED(hr))
      {
        SAFE_RELEASE(m_DebugRender.PickIBBuf);
        RDCERR("Failed to create PickIBSRV %08x", hr);
        return ~0U;
      }
    }

    // copy index data as-is, the view format will take care of the rest

    RDCASSERT(cfg.position.idxoffs < 0xffffffff);

    D3D11_BUFFER_DESC ibdesc;
    ib->GetDesc(&ibdesc);

    D3D11_BOX box;
    box.front = 0;
    box.back = 1;
    box.left = (uint32_t)cfg.position.idxoffs;
    box.right = (uint32_t)cfg.position.idxoffs + cfg.position.numVerts * cfg.position.idxByteWidth;
    box.top = 0;
    box.bottom = 1;

    box.right = RDCMIN(box.right, ibdesc.ByteWidth - (uint32_t)cfg.position.idxoffs);

    m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickIBBuf, 0, 0, 0, 0, ib, 0, &box);
  }

  if(m_DebugRender.PickVBBuf == NULL ||
     m_DebugRender.PickVBSize < cfg.position.numVerts * sizeof(Vec4f))
  {
    SAFE_RELEASE(m_DebugRender.PickVBBuf);
    SAFE_RELEASE(m_DebugRender.PickVBSRV);

    D3D11_BUFFER_DESC desc = {cfg.position.numVerts * sizeof(Vec4f),
                              D3D11_USAGE_DEFAULT,
                              D3D11_BIND_SHADER_RESOURCE,
                              0,
                              0,
                              0};

    m_DebugRender.PickVBSize = cfg.position.numVerts * sizeof(Vec4f);

    hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickVBBuf);

    if(FAILED(hr))
    {
      RDCERR("Failed to create PickVBBuf %08x", hr);
      return ~0U;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
    sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    sdesc.Buffer.FirstElement = 0;
    sdesc.Buffer.NumElements = cfg.position.numVerts;

    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickVBBuf, &sdesc,
                                             &m_DebugRender.PickVBSRV);

    if(FAILED(hr))
    {
      SAFE_RELEASE(m_DebugRender.PickVBBuf);
      RDCERR("Failed to create PickVBSRV %08x", hr);
      return ~0U;
    }
  }

  // unpack and linearise the data
  {
    FloatVector *vbData = new FloatVector[cfg.position.numVerts];

    vector<byte> oldData;
    GetBufferData(vb, cfg.position.offset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numVerts; i++)
    {
      uint32_t idx = i;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(idx < idxclamp)
        idx = 0;
      else if(cfg.position.baseVertex < 0)
        idx -= idxclamp;
      else if(cfg.position.baseVertex > 0)
        idx += cfg.position.baseVertex;

      vbData[i] = HighlightCache::InterpretVertex(data, idx, cfg, dataEnd, valid);
    }

    D3D11_BOX box;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;
    box.left = 0;
    box.right = cfg.position.numVerts * sizeof(Vec4f);

    m_pImmediateContext->UpdateSubresource(m_DebugRender.PickVBBuf, 0, &box, vbData, sizeof(Vec4f),
                                           sizeof(Vec4f));

    delete[] vbData;
  }

  ID3D11ShaderResourceView *srvs[2] = {m_DebugRender.PickIBSRV, m_DebugRender.PickVBSRV};

  ID3D11Buffer *buf = MakeCBuffer(&cbuf, sizeof(cbuf));

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &buf);

  m_pImmediateContext->CSSetShaderResources(0, 2, srvs);

  UINT reset = 0;
  m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_DebugRender.PickResultUAV, &reset);

  m_pImmediateContext->CSSetShader(m_DebugRender.MeshPickCS, NULL, 0);

  m_pImmediateContext->Dispatch(cfg.position.numVerts / 1024 + 1, 1, 1);

  m_pImmediateContext->CopyStructureCount(m_DebugRender.histogramBuff, 0,
                                          m_DebugRender.PickResultUAV);

  vector<byte> results;
  GetBufferData(m_DebugRender.histogramBuff, 0, 0, results);

  uint32_t numResults = *(uint32_t *)&results[0];

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        Vec3f intersectionPoint;
      };

      GetBufferData(m_DebugRender.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
      {
        float pickDistance = (pickResults[i].intersectionPoint - rayPos).Length();
        if(pickDistance < closestPickDistance)
        {
          closest = pickResults + i;
        }
      }

      return closest->vertid;
    }
    else
    {
      struct PickResult
      {
        uint32_t vertid;
        uint32_t idx;
        float len;
        float depth;
      };

      GetBufferData(m_DebugRender.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
      {
        // We need to keep the picking order consistent in the face
        // of random buffer appends, when multiple vertices have the
        // identical position (e.g. if UVs or normals are different).
        //
        // We could do something to try and disambiguate, but it's
        // never going to be intuitive, it's just going to flicker
        // confusingly.
        if(pickResults[i].len < closest->len ||
           (pickResults[i].len == closest->len && pickResults[i].depth < closest->depth) ||
           (pickResults[i].len == closest->len && pickResults[i].depth == closest->depth &&
            pickResults[i].vertid < closest->vertid))
          closest = pickResults + i;
      }

      return closest->vertid;
    }
  }

  return ~0U;
}

void D3D11DebugManager::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                                  uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

  m_pImmediateContext->OMSetRenderTargets(1, &m_DebugRender.PickPixelRT, NULL);

  float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  m_pImmediateContext->ClearRenderTargetView(m_DebugRender.PickPixelRT, color);

  D3D11_VIEWPORT viewport;
  RDCEraseEl(viewport);

  int oldW = GetWidth(), oldH = GetHeight();

  SetOutputDimensions(100, 100);

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = 100;
  viewport.Height = 100;

  m_pImmediateContext->RSSetViewports(1, &viewport);

  {
    TextureDisplay texDisplay;

    texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
    texDisplay.HDRMul = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.FlipY = false;
    texDisplay.mip = mip;
    texDisplay.sampleIdx = sample;
    texDisplay.CustomShader = ResourceId();
    texDisplay.sliceFace = sliceFace;
    texDisplay.rangemin = 0.0f;
    texDisplay.rangemax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.texid = texture;
    texDisplay.typeHint = typeHint;
    texDisplay.rawoutput = true;
    texDisplay.offx = -float(x);
    texDisplay.offy = -float(y);

    RenderTexture(texDisplay, false);
  }

  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = 0;
  box.right = 1;
  box.top = 0;
  box.bottom = 1;

  ID3D11Resource *res = NULL;
  m_DebugRender.PickPixelRT->GetResource(&res);

  m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickPixelStageTex, 0, 0, 0, 0, res, 0,
                                             &box);

  SAFE_RELEASE(res);

  D3D11_MAPPED_SUBRESOURCE mapped;
  mapped.pData = NULL;
  HRESULT hr =
      m_pImmediateContext->Map(m_DebugRender.PickPixelStageTex, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map stage buff %08x", hr);
  }

  float *pix = (float *)mapped.pData;

  if(pix == NULL)
  {
    RDCERR("Failed to map pick-pixel staging texture.");
  }
  else
  {
    pixel[0] = pix[0];
    pixel[1] = pix[1];
    pixel[2] = pix[2];
    pixel[3] = pix[3];
  }

  SetOutputDimensions(oldW, oldH);

  m_pImmediateContext->Unmap(m_DebugRender.PickPixelStageTex, 0);
}

byte *D3D11DebugManager::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                        const GetTextureDataParams &params, size_t &dataSize)
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11Resource *dummyTex = NULL;

  uint32_t subresource = 0;
  uint32_t mips = 0;

  dataSize = 0;
  size_t bytesize = 0;

  if(WrappedID3D11Texture1D::m_TextureList.find(tex) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *wrapTex =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE1D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture1D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);

    if(mip >= mips || arrayIdx >= desc.ArraySize)
      return NULL;

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.ArraySize = 1;
    }

    subresource = arrayIdx * mips + mip;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
      return NULL;
    }

    bytesize = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture1D *rtTex = NULL;

      hr = m_pDevice->CreateTexture1D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. %08x", hr);
        SAFE_RELEASE(d);
        return NULL;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture1D.MipSlice = mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. %08x", hr);
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return NULL;
      }

      ID3D11RenderTargetView *rtv = wrappedrtv;

      m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
      float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, color);

      D3D11_VIEWPORT viewport = {0, 0, (float)(desc.Width >> mip), 1.0f, 0.0f, 1.0f};

      int oldW = GetWidth(), oldH = GetHeight();
      SetOutputDimensions(desc.Width, 1);
      m_pImmediateContext->RSSetViewports(1, &viewport);

      {
        TextureDisplay texDisplay;

        texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
        texDisplay.HDRMul = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.FlipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.CustomShader = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        texDisplay.rangemin = params.blackPoint;
        texDisplay.rangemax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.texid = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawoutput = false;
        texDisplay.offx = 0;
        texDisplay.offy = 0;

        RenderTexture(texDisplay, false);
      }

      SetOutputDimensions(oldW, oldH);

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);

      SAFE_RELEASE(wrappedrtv);
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(tex) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *wrapTex =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE2D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    bool wasms = false;

    if(desc.SampleDesc.Count > 1)
    {
      desc.ArraySize *= desc.SampleDesc.Count;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;

      wasms = true;
    }

    ID3D11Texture2D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

    if(mip >= mips || arrayIdx >= desc.ArraySize)
      return NULL;

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      desc.Format = (IsSRGBFormat(desc.Format) || wrapTex->m_RealDescriptor)
                        ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                        : DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.ArraySize = 1;
    }

    subresource = arrayIdx * mips + mip;

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
      return NULL;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture2D *rtTex = NULL;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. %08x", hr);
        SAFE_RELEASE(d);
        return NULL;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture2D.MipSlice = mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. %08x", hr);
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return NULL;
      }

      ID3D11RenderTargetView *rtv = wrappedrtv;

      m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
      float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, color);

      D3D11_VIEWPORT viewport = {0,    0,   (float)(desc.Width >> mip), (float)(desc.Height >> mip),
                                 0.0f, 1.0f};

      int oldW = GetWidth(), oldH = GetHeight();
      SetOutputDimensions(desc.Width, desc.Height);
      m_pImmediateContext->RSSetViewports(1, &viewport);

      {
        TextureDisplay texDisplay;

        texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
        texDisplay.HDRMul = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.FlipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
        texDisplay.CustomShader = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        texDisplay.rangemin = params.blackPoint;
        texDisplay.rangemax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.texid = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawoutput = false;
        texDisplay.offx = 0;
        texDisplay.offy = 0;

        RenderTexture(texDisplay, false);
      }

      SetOutputDimensions(oldW, oldH);

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);

      SAFE_RELEASE(wrappedrtv);
    }
    else if(wasms && params.resolve)
    {
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.CPUAccessFlags = 0;

      ID3D11Texture2D *resolveTex = NULL;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &resolveTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to resolve texture. %08x", hr);
        SAFE_RELEASE(d);
        return NULL;
      }

      m_pImmediateContext->ResolveSubresource(resolveTex, arrayIdx, wrapTex, arrayIdx, desc.Format);
      m_pImmediateContext->CopyResource(d, resolveTex);

      SAFE_RELEASE(resolveTex);
    }
    else if(wasms)
    {
      CopyTex2DMSToArray(UNWRAP(WrappedID3D11Texture2D1, d), wrapTex->GetReal());
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(tex) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *wrapTex =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE3D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture3D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);

    if(mip >= mips)
      return NULL;

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    subresource = mip;

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. %08x", hr);
      return NULL;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip);

    if(params.remap)
    {
      RDCASSERT(params.remap == eRemap_RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture3D *rtTex = NULL;

      hr = m_pDevice->CreateTexture3D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. %08x", hr);
        SAFE_RELEASE(d);
        return NULL;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture3D.MipSlice = mip;
      rtvDesc.Texture3D.FirstWSlice = 0;
      rtvDesc.Texture3D.WSize = 1;
      ID3D11RenderTargetView *wrappedrtv = NULL;
      ID3D11RenderTargetView *rtv = NULL;

      D3D11_VIEWPORT viewport = {0,    0,   (float)(desc.Width >> mip), (float)(desc.Height >> mip),
                                 0.0f, 1.0f};

      int oldW = GetWidth(), oldH = GetHeight();

      for(UINT i = 0; i < (desc.Depth >> mip); i++)
      {
        rtvDesc.Texture3D.FirstWSlice = i;
        hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
        if(FAILED(hr))
        {
          RDCERR("Couldn't create target rtv to downcast texture. %08x", hr);
          SAFE_RELEASE(d);
          SAFE_RELEASE(rtTex);
          return NULL;
        }

        rtv = wrappedrtv;

        m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
        float color[4] = {0.0f, 0.5f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTargetView(rtv, color);

        SetOutputDimensions(desc.Width, desc.Height);
        m_pImmediateContext->RSSetViewports(1, &viewport);

        TextureDisplay texDisplay;

        texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
        texDisplay.HDRMul = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.FlipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.CustomShader = ResourceId();
        texDisplay.sliceFace = i << mip;
        texDisplay.rangemin = params.blackPoint;
        texDisplay.rangemax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.texid = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawoutput = false;
        texDisplay.offx = 0;
        texDisplay.offy = 0;

        RenderTexture(texDisplay, false);

        SAFE_RELEASE(wrappedrtv);
      }

      SetOutputDimensions(oldW, oldH);

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else
  {
    RDCERR("Trying to get texture data for unknown ID %llu!", tex);
    dataSize = 0;
    return new byte[0];
  }

  MapIntercept intercept;

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  HRESULT hr = m_pImmediateContext->Map(dummyTex, subresource, D3D11_MAP_READ, 0, &mapped);

  byte *ret = NULL;

  if(SUCCEEDED(hr))
  {
    ret = new byte[bytesize];
    dataSize = bytesize;
    intercept.InitWrappedResource(dummyTex, subresource, ret);
    intercept.SetD3D(mapped);
    intercept.CopyFromD3D();

    // for 3D textures if we wanted a particular slice (arrayIdx > 0)
    // copy it into the beginning.
    if(intercept.numSlices > 1 && arrayIdx > 0 && (int)arrayIdx < intercept.numSlices)
    {
      byte *dst = ret;
      byte *src = ret + intercept.app.DepthPitch * arrayIdx;

      for(int row = 0; row < intercept.numRows; row++)
      {
        memcpy(dst, src, intercept.app.RowPitch);

        src += intercept.app.RowPitch;
        dst += intercept.app.RowPitch;
      }
    }
  }
  else
  {
    RDCERR("Couldn't map staging texture to retrieve data. %08x", hr);
  }

  SAFE_RELEASE(dummyTex);

  return ret;
}

ResourceId D3D11DebugManager::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                                uint32_t arrayIdx, uint32_t sampleIdx,
                                                CompType typeHint)
{
  TextureShaderDetails details = GetShaderDetails(texid, typeHint, false);

  CreateCustomShaderTex(details.texWidth, details.texHeight);

  D3D11RenderStateTracker tracker(m_WrappedContext);

  {
    D3D11_RENDER_TARGET_VIEW_DESC desc;

    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = mip;

    WrappedID3D11Texture2D1 *wrapped = (WrappedID3D11Texture2D1 *)m_CustomShaderTex;
    HRESULT hr = m_pDevice->CreateRenderTargetView(wrapped, &desc, &m_CustomShaderRTV);

    if(FAILED(hr))
    {
      RDCERR("Failed to create custom shader rtv %08x", hr);
      return m_CustomShaderResourceId;
    }
  }

  m_pImmediateContext->OMSetRenderTargets(1, &m_CustomShaderRTV, NULL);

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pImmediateContext->ClearRenderTargetView(m_CustomShaderRTV, clr);

  D3D11_VIEWPORT viewport;
  RDCEraseEl(viewport);

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = (float)RDCMAX(1U, details.texWidth >> mip);
  viewport.Height = (float)RDCMAX(1U, details.texHeight >> mip);

  m_pImmediateContext->RSSetViewports(1, &viewport);

  TextureDisplay disp;
  disp.Red = disp.Green = disp.Blue = disp.Alpha = true;
  disp.FlipY = false;
  disp.offx = 0.0f;
  disp.offy = 0.0f;
  disp.CustomShader = shader;
  disp.texid = texid;
  disp.typeHint = typeHint;
  disp.lightBackgroundColor = disp.darkBackgroundColor = FloatVector(0, 0, 0, 0);
  disp.HDRMul = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangemin = 0.0f;
  disp.rangemax = 1.0f;
  disp.rawoutput = false;
  disp.scale = 1.0f;
  disp.sliceFace = arrayIdx;

  SetOutputDimensions(RDCMAX(1U, details.texWidth >> mip), RDCMAX(1U, details.texHeight >> mip));

  RenderTexture(disp, true);

  return m_CustomShaderResourceId;
}

void D3D11DebugManager::CreateCustomShaderTex(uint32_t w, uint32_t h)
{
  D3D11_TEXTURE2D_DESC texdesc;

  texdesc.ArraySize = 1;
  texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  texdesc.CPUAccessFlags = 0;
  texdesc.MipLevels = CalcNumMips((int)w, (int)h, 1);
  texdesc.MiscFlags = 0;
  texdesc.SampleDesc.Count = 1;
  texdesc.SampleDesc.Quality = 0;
  texdesc.Usage = D3D11_USAGE_DEFAULT;
  texdesc.Width = w;
  texdesc.Height = h;
  texdesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  if(m_CustomShaderTex)
  {
    D3D11_TEXTURE2D_DESC customTexDesc;
    m_CustomShaderTex->GetDesc(&customTexDesc);

    if(customTexDesc.Width == w && customTexDesc.Height == h)
      return;

    SAFE_RELEASE(m_CustomShaderRTV);
    SAFE_RELEASE(m_CustomShaderTex);
  }

  HRESULT hr = m_pDevice->CreateTexture2D(&texdesc, NULL, &m_CustomShaderTex);

  if(FAILED(hr))
  {
    RDCERR("Failed to create custom shader tex %08x", hr);
  }
  else
  {
    m_CustomShaderResourceId = GetIDForResource(m_CustomShaderTex);
  }
}

ResourceId D3D11DebugManager::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                            uint32_t eventID, const vector<uint32_t> &passEvents)
{
  TextureShaderDetails details = GetShaderDetails(texid, typeHint, false);

  ResourceId id = texid;

  D3D11_TEXTURE2D_DESC realTexDesc;
  realTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  realTexDesc.Usage = D3D11_USAGE_DEFAULT;
  realTexDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
  realTexDesc.ArraySize = 1;
  realTexDesc.MipLevels = 1;
  realTexDesc.CPUAccessFlags = 0;
  realTexDesc.MiscFlags = 0;
  realTexDesc.SampleDesc.Count = 1;
  realTexDesc.SampleDesc.Quality = 0;
  realTexDesc.Width = details.texWidth;
  realTexDesc.Height = details.texHeight;

  if(details.texType == eTexType_2DMS)
  {
    realTexDesc.SampleDesc.Count = details.sampleCount;
    realTexDesc.SampleDesc.Quality = details.sampleQuality;
  }

  D3D11RenderStateTracker tracker(m_WrappedContext);

  D3D11_TEXTURE2D_DESC customTexDesc;
  RDCEraseEl(customTexDesc);
  if(m_OverlayRenderTex)
    m_OverlayRenderTex->GetDesc(&customTexDesc);

  WrappedID3D11Texture2D1 *wrappedCustomRenderTex = (WrappedID3D11Texture2D1 *)m_OverlayRenderTex;

  // need to recreate backing custom render tex
  if(realTexDesc.Width != customTexDesc.Width || realTexDesc.Height != customTexDesc.Height ||
     realTexDesc.Format != customTexDesc.Format ||
     realTexDesc.SampleDesc.Count != customTexDesc.SampleDesc.Count ||
     realTexDesc.SampleDesc.Quality != customTexDesc.SampleDesc.Quality)
  {
    SAFE_RELEASE(m_OverlayRenderTex);
    m_OverlayResourceId = ResourceId();

    ID3D11Texture2D *customRenderTex = NULL;
    HRESULT hr = m_pDevice->CreateTexture2D(&realTexDesc, NULL, &customRenderTex);
    if(FAILED(hr))
    {
      RDCERR("Failed to create custom render tex %08x", hr);
      return ResourceId();
    }
    wrappedCustomRenderTex = (WrappedID3D11Texture2D1 *)customRenderTex;

    m_OverlayRenderTex = wrappedCustomRenderTex;
    m_OverlayResourceId = wrappedCustomRenderTex->GetResourceID();
  }

  ID3D11Texture2D *preDrawDepth = NULL;
  ID3D11Texture2D *renderDepth = NULL;

  ID3D11DepthStencilView *dsView = NULL;

  m_pImmediateContext->OMGetRenderTargets(0, NULL, &dsView);

  D3D11_DEPTH_STENCIL_VIEW_DESC dsViewDesc;
  RDCEraseEl(dsViewDesc);
  if(dsView)
  {
    ID3D11Texture2D *realDepth = NULL;

    dsView->GetResource((ID3D11Resource **)&realDepth);

    dsView->GetDesc(&dsViewDesc);

    SAFE_RELEASE(dsView);

    D3D11_TEXTURE2D_DESC desc;

    realDepth->GetDesc(&desc);

    HRESULT hr = S_OK;

    hr = m_pDevice->CreateTexture2D(&desc, NULL, &preDrawDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create preDrawDepth %08x", hr);
      SAFE_RELEASE(realDepth);
      return m_OverlayResourceId;
    }
    hr = m_pDevice->CreateTexture2D(&desc, NULL, &renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth %08x", hr);
      SAFE_RELEASE(realDepth);
      return m_OverlayResourceId;
    }

    m_pImmediateContext->CopyResource(preDrawDepth, realDepth);

    SAFE_RELEASE(realDepth);
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
  rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
  rtDesc.Texture2D.MipSlice = 0;

  if(realTexDesc.SampleDesc.Count > 1 || realTexDesc.SampleDesc.Quality > 0)
  {
    rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
  }

  ID3D11RenderTargetView *rtv = NULL;
  HRESULT hr = m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, &rtv);
  if(FAILED(hr))
  {
    RDCERR("Failed to create custom render tex RTV %08x", hr);
    return m_OverlayResourceId;
  }

  FLOAT black[] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pImmediateContext->ClearRenderTargetView(rtv, black);

  if(renderDepth)
  {
    m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);

    hr = m_pDevice->CreateDepthStencilView(renderDepth, &dsViewDesc, &dsView);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth DSV %08x", hr);
      return m_OverlayResourceId;
    }
  }

  m_pImmediateContext->OMSetRenderTargets(1, &rtv, dsView);

  SAFE_RELEASE(dsView);

  D3D11_DEPTH_STENCIL_DESC dsDesc;

  dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
      dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
      dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  dsDesc.DepthEnable = TRUE;
  dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  dsDesc.StencilEnable = FALSE;
  dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state %08x", hr);
      return m_OverlayResourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = FALSE;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state %08x", hr);
        return m_OverlayResourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.5f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    float overlayConsts[] = {0.8f, 0.1f, 0.8f, 1.0f};
    ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state %08x", hr);
      return m_OverlayResourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    ID3D11RasterizerState *rsCull = NULL;
    D3D11_RASTERIZER_DESC origdesc;

    {
      m_pImmediateContext->RSGetState(&rs);

      if(rs)
        rs->GetDesc(&origdesc);
      else
        origdesc.CullMode = D3D11_CULL_BACK;

      SAFE_RELEASE(rs);
    }

    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = FALSE;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state %08x", hr);
        return m_OverlayResourceId;
      }

      rdesc.CullMode = origdesc.CullMode;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rsCull);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state %08x", hr);
        return m_OverlayResourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    float overlayConsts[] = {1.0f, 0.0f, 0.0f, 1.0f};
    ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

    overlayConsts[0] = 0.0f;
    overlayConsts[1] = 1.0f;

    buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rsCull);

    m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
    SAFE_RELEASE(rsCull);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->PSSetShader(m_DebugRender.OutlinePS, NULL, 0);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImmediateContext->IASetInputLayout(NULL);

    D3D11_RASTERIZER_DESC origdesc;

    {
      ID3D11RasterizerState *rs = NULL;

      m_pImmediateContext->RSGetState(&rs);

      if(rs)
        rs->GetDesc(&origdesc);
      else
        origdesc.ScissorEnable = FALSE;

      SAFE_RELEASE(rs);
    }

    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create drawcall depth stencil state %08x", hr);
      return m_OverlayResourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    D3D11_BLEND_DESC blendDesc;
    RDCEraseEl(blendDesc);

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState *bs = NULL;
    hr = m_pDevice->CreateBlendState(&blendDesc, &bs);

    float blendwhite[] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(bs, blendwhite, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      rdesc.FillMode = D3D11_FILL_SOLID;
      rdesc.CullMode = D3D11_CULL_NONE;
      rdesc.FrontCounterClockwise = FALSE;
      rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
      rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
      rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      rdesc.DepthClipEnable = FALSE;
      rdesc.ScissorEnable = FALSE;
      rdesc.MultisampleEnable = FALSE;
      rdesc.AntialiasedLineEnable = FALSE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall rast state %08x", hr);
        return m_OverlayResourceId;
      }
    }

    float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

    m_pImmediateContext->RSSetState(rs);

    DebugPixelCBufferData pixelData = {0};

    UINT dummy = 1;
    D3D11_VIEWPORT views[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {0};
    m_pImmediateContext->RSGetViewports(&dummy, views);

    // border colour (dark, 2px, opaque)
    pixelData.WireframeColour = Vec3f(0.1f, 0.1f, 0.1f);
    // inner colour (light, transparent)
    pixelData.Channels = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
    pixelData.OutputDisplayFormat = 0;
    pixelData.RangeMinimum = views[0].TopLeftX;
    pixelData.InverseRangeSize = views[0].TopLeftY;
    pixelData.TextureResolutionPS = Vec3f(views[0].Width, views[0].Height, 0.0f);

    ID3D11Buffer *buf = MakeCBuffer(&pixelData, sizeof(DebugPixelCBufferData));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->Draw(3, 0);

    if(origdesc.ScissorEnable)
    {
      D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {0};
      m_pImmediateContext->RSGetScissorRects(&dummy, rects);

      D3D11_VIEWPORT scissorview;

      scissorview.TopLeftX = (float)rects[0].left;
      scissorview.TopLeftY = (float)rects[0].top;
      scissorview.MinDepth = 0.0f;
      scissorview.MaxDepth = 1.0f;
      scissorview.Width = (float)(rects[0].right - rects[0].left);
      scissorview.Height = (float)(rects[0].bottom - rects[0].top);

      m_pImmediateContext->RSSetViewports(1, &scissorview);

      pixelData.OutputDisplayFormat = 1;
      pixelData.RangeMinimum = scissorview.TopLeftX;
      pixelData.InverseRangeSize = scissorview.TopLeftY;
      pixelData.TextureResolutionPS = Vec3f(scissorview.Width, scissorview.Height, 0.0f);

      buf = MakeCBuffer(&pixelData, sizeof(DebugPixelCBufferData));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->Draw(3, 0);
    }

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
    SAFE_RELEASE(bs);
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

    dsDesc.DepthEnable = FALSE;

    ID3D11DepthStencilState *os = NULL;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
    if(FAILED(hr))
    {
      RDCERR("Failed to create wireframe depth state %08x", hr);
      return m_OverlayResourceId;
    }

    m_pImmediateContext->OMSetDepthStencilState(os, 0);

    m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    ID3D11RasterizerState *rs = NULL;
    {
      D3D11_RASTERIZER_DESC rdesc;

      m_pImmediateContext->RSGetState(&rs);

      if(rs)
      {
        rs->GetDesc(&rdesc);
      }
      else
      {
        rdesc.FillMode = D3D11_FILL_SOLID;
        rdesc.CullMode = D3D11_CULL_BACK;
        rdesc.FrontCounterClockwise = FALSE;
        rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rdesc.DepthClipEnable = TRUE;
        rdesc.ScissorEnable = FALSE;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = FALSE;
      }

      SAFE_RELEASE(rs);

      rdesc.FillMode = D3D11_FILL_WIREFRAME;
      rdesc.CullMode = D3D11_CULL_NONE;

      hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
      if(FAILED(hr))
      {
        RDCERR("Failed to create wireframe rast state %08x", hr);
        return m_OverlayResourceId;
      }
    }

    float overlayConsts[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, overlayConsts);

    overlayConsts[3] = 1.0f;
    ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

    m_pImmediateContext->RSSetState(rs);

    m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

    SAFE_RELEASE(os);
    SAFE_RELEASE(rs);
  }
  else if(overlay == DebugOverlay::ClearBeforePass || overlay == DebugOverlay::ClearBeforeDraw)
  {
    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::ClearBeforePass)
        m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      D3D11RenderState *state = m_WrappedContext->GetCurrentPipelineState();

      for(size_t i = 0; i < ARRAY_COUNT(state->OM.RenderTargets); i++)
        if(state->OM.RenderTargets[i])
          m_pImmediateContext->ClearRenderTargetView(state->OM.RenderTargets[i], black);

      for(size_t i = 0; i < events.size(); i++)
      {
        m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass)
        {
          m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_WrappedDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    SCOPED_TIMER("Triangle size");

    // ensure it will be recreated on next use
    SAFE_RELEASE(m_MeshDisplayLayout);
    m_PrevMeshFmt = ResourceFormat();

    D3D11_INPUT_ELEMENT_DESC layoutdesc[2] = {};

    layoutdesc[0].SemanticName = "pos";
    layoutdesc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    // dummy for vertex shader
    layoutdesc[1].SemanticName = "sec";
    layoutdesc[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;

    hr = m_pDevice->CreateInputLayout(layoutdesc, 2, m_DebugRender.MeshVSBytecode,
                                      m_DebugRender.MeshVSBytelen, &m_MeshDisplayLayout);

    if(FAILED(hr))
    {
      RDCERR("Failed to create m_MeshDisplayLayout %08x", hr);
      m_MeshDisplayLayout = NULL;
    }

    DebugVertexCBuffer vertexData = {};
    vertexData.LineStrip = 0;
    vertexData.ModelViewProj = Matrix4f::Identity();
    vertexData.SpriteSize = Vec2f();
    FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

    ID3D11Buffer *psbuf = MakeCBuffer(&overdrawRamp[0].x, sizeof(overdrawRamp));

    Vec4f viewport = Vec4f((float)details.texWidth, (float)details.texHeight);
    ID3D11Buffer *gsbuf = MakeCBuffer(&viewport.x, sizeof(viewport));

    float overlayConsts[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pImmediateContext->ClearRenderTargetView(rtv, overlayConsts);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::TriangleSizeDraw)
      events.clear();

    events.push_back(eventID);

    if(overlay == DebugOverlay::TriangleSizePass)
      m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

    events.push_back(eventID);

    for(size_t i = 0; i < events.size(); i++)
    {
      D3D11RenderState oldstate = *m_WrappedContext->GetCurrentPipelineState();

      D3D11_DEPTH_STENCIL_DESC dsdesc = {
          /*DepthEnable =*/TRUE,
          /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
          /*DepthFunc =*/D3D11_COMPARISON_LESS,
          /*StencilEnable =*/FALSE,
          /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
          /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
          /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                          D3D11_COMPARISON_ALWAYS},
          /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                         D3D11_COMPARISON_ALWAYS},
      };
      ID3D11DepthStencilState *ds = NULL;

      if(oldstate.OM.DepthStencilState)
        oldstate.OM.DepthStencilState->GetDesc(&dsdesc);

      dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
      dsdesc.StencilWriteMask = 0;

      m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

      m_pImmediateContext->OMSetDepthStencilState(ds, oldstate.OM.StencRef);

      SAFE_RELEASE(ds);

      const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(events[i]);

      for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
      {
        MeshFormat fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
        if(fmt.buf == ResourceId())
          fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

        if(fmt.buf != ResourceId())
        {
          D3D11_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(fmt.topo);

          ID3D11Buffer *ibuf = NULL;
          DXGI_FORMAT ifmt = DXGI_FORMAT_R16_UINT;
          UINT ioffs = (UINT)fmt.idxoffs;

          ID3D11Buffer *vbs[2] = {NULL, NULL};
          UINT str[2] = {fmt.stride, 4};
          UINT offs[2] = {(UINT)fmt.offset, 0};

          {
            auto it = WrappedID3D11Buffer::m_BufferList.find(fmt.buf);

            if(it != WrappedID3D11Buffer::m_BufferList.end())
              vbs[0] = it->second.m_Buffer;

            it = WrappedID3D11Buffer::m_BufferList.find(fmt.idxbuf);

            if(it != WrappedID3D11Buffer::m_BufferList.end())
              ibuf = it->second.m_Buffer;

            if(fmt.idxByteWidth == 4)
              ifmt = DXGI_FORMAT_R32_UINT;
          }

          m_pImmediateContext->IASetVertexBuffers(0, 1, vbs, str, offs);
          if(ibuf)
            m_pImmediateContext->IASetIndexBuffer(ibuf, ifmt, ioffs);
          else
            m_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, NULL);

          m_pImmediateContext->IASetPrimitiveTopology(topo);

          m_pImmediateContext->IASetInputLayout(m_MeshDisplayLayout);
          m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
          m_pImmediateContext->PSSetConstantBuffers(0, 1, &psbuf);
          m_pImmediateContext->GSSetConstantBuffers(0, 1, &gsbuf);
          m_pImmediateContext->VSSetShader(m_DebugRender.MeshVS, NULL, 0);
          m_pImmediateContext->GSSetShader(m_DebugRender.TriangleSizeGS, NULL, 0);
          m_pImmediateContext->PSSetShader(m_DebugRender.TriangleSizePS, NULL, 0);
          m_pImmediateContext->HSSetShader(NULL, NULL, 0);
          m_pImmediateContext->DSSetShader(NULL, NULL, 0);
          m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);
          m_pImmediateContext->OMSetRenderTargets(1, &rtv, oldstate.OM.DepthView);

          if(ibuf)
            m_pImmediateContext->DrawIndexed(fmt.numVerts, 0, fmt.baseVertex);
          else
            m_pImmediateContext->Draw(fmt.numVerts, 0);
        }
      }

      oldstate.ApplyState(m_WrappedContext);

      if(overlay == DebugOverlay::TriangleSizePass)
      {
        m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(i + 1 < events.size())
          m_WrappedDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
      }
    }

    if(overlay == DebugOverlay::TriangleSizePass)
      m_WrappedDevice->ReplayLog(0, eventID, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    SCOPED_TIMER("Quad Overdraw");

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::QuadOverdrawDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::QuadOverdrawPass)
        m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      D3D11RenderState *state = m_WrappedContext->GetCurrentPipelineState();

      uint32_t width = 1920 >> 1;
      uint32_t height = 1080 >> 1;

      uint32_t depthWidth = 1920;
      uint32_t depthHeight = 1080;
      bool forceDepth = false;

      {
        ID3D11Resource *res = NULL;
        if(state->OM.RenderTargets[0])
        {
          state->OM.RenderTargets[0]->GetResource(&res);
        }
        else if(state->OM.DepthView)
        {
          state->OM.DepthView->GetResource(&res);
        }
        else
        {
          RDCERR("Couldn't get size of existing targets");
          return m_OverlayResourceId;
        }

        D3D11_RESOURCE_DIMENSION dim;
        res->GetType(&dim);

        if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        {
          D3D11_TEXTURE1D_DESC texdesc;
          ((ID3D11Texture1D *)res)->GetDesc(&texdesc);

          width = RDCMAX(1U, texdesc.Width >> 1);
          height = 1;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
          D3D11_TEXTURE2D_DESC texdesc;
          ((ID3D11Texture2D *)res)->GetDesc(&texdesc);

          width = RDCMAX(1U, texdesc.Width >> 1);
          height = RDCMAX(1U, texdesc.Height >> 1);

          if(texdesc.SampleDesc.Count > 1)
          {
            forceDepth = true;
            depthWidth = texdesc.Width;
            depthHeight = texdesc.Height;
          }
        }
        else
        {
          RDCERR("Trying to show quad overdraw on invalid view");
          return m_OverlayResourceId;
        }

        SAFE_RELEASE(res);
      }

      ID3D11DepthStencilView *depthOverride = NULL;

      if(forceDepth)
      {
        D3D11_TEXTURE2D_DESC texDesc = {
            depthWidth,
            depthHeight,
            1U,
            1U,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            {1, 0},
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_DEPTH_STENCIL,
            0,
            0,
        };

        ID3D11Texture2D *tex = NULL;
        m_pDevice->CreateTexture2D(&texDesc, NULL, &tex);
        m_pDevice->CreateDepthStencilView(tex, NULL, &depthOverride);
        SAFE_RELEASE(tex);
      }

      D3D11_TEXTURE2D_DESC uavTexDesc = {
          width,
          height,
          1U,
          4U,
          DXGI_FORMAT_R32_UINT,
          {1, 0},
          D3D11_USAGE_DEFAULT,
          D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
          0,
          0,
      };

      ID3D11Texture2D *overdrawTex = NULL;
      ID3D11ShaderResourceView *overdrawSRV = NULL;
      ID3D11UnorderedAccessView *overdrawUAV = NULL;

      m_pDevice->CreateTexture2D(&uavTexDesc, NULL, &overdrawTex);
      m_pDevice->CreateShaderResourceView(overdrawTex, NULL, &overdrawSRV);
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, &overdrawUAV);

      UINT val = 0;
      m_pImmediateContext->ClearUnorderedAccessViewUint(overdrawUAV, &val);

      for(size_t i = 0; i < events.size(); i++)
      {
        D3D11RenderState oldstate = *m_WrappedContext->GetCurrentPipelineState();

        D3D11_DEPTH_STENCIL_DESC dsdesc = {
            /*DepthEnable =*/TRUE,
            /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
            /*DepthFunc =*/D3D11_COMPARISON_LESS,
            /*StencilEnable =*/FALSE,
            /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
            /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
            /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                            D3D11_COMPARISON_ALWAYS},
            /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                           D3D11_COMPARISON_ALWAYS},
        };
        ID3D11DepthStencilState *ds = NULL;

        if(state->OM.DepthStencilState)
          state->OM.DepthStencilState->GetDesc(&dsdesc);

        dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsdesc.StencilWriteMask = 0;

        m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

        m_pImmediateContext->OMSetDepthStencilState(ds, oldstate.OM.StencRef);

        SAFE_RELEASE(ds);

        UINT UAVcount = 0;
        m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
            0, NULL, depthOverride ? depthOverride : oldstate.OM.DepthView, 0, 1, &overdrawUAV,
            &UAVcount);

        m_pImmediateContext->PSSetShader(m_DebugRender.QuadOverdrawPS, NULL, 0);

        m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        oldstate.ApplyState(m_WrappedContext);

        if(overlay == DebugOverlay::QuadOverdrawPass)
        {
          m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_WrappedDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }

      SAFE_RELEASE(depthOverride);

      // resolve pass
      {
        m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
        m_pImmediateContext->HSSetShader(NULL, NULL, 0);
        m_pImmediateContext->DSSetShader(NULL, NULL, 0);
        m_pImmediateContext->GSSetShader(NULL, NULL, 0);
        m_pImmediateContext->PSSetShader(m_DebugRender.QOResolvePS, NULL, 0);
        m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImmediateContext->IASetInputLayout(NULL);

        ID3D11Buffer *buf = MakeCBuffer(&overdrawRamp[0].x, sizeof(overdrawRamp));

        m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

        m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);

        m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.NoDepthState, 0);
        m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);
        m_pImmediateContext->RSSetState(m_DebugRender.RastState);

        float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

        m_pImmediateContext->PSSetShaderResources(0, 1, &overdrawSRV);

        m_pImmediateContext->Draw(3, 0);
      }

      SAFE_RELEASE(overdrawTex);
      SAFE_RELEASE(overdrawSRV);
      SAFE_RELEASE(overdrawUAV);

      if(overlay == DebugOverlay::QuadOverdrawPass)
        m_WrappedDevice->ReplayLog(0, eventID, eReplay_WithoutDraw);
    }
  }
  else if(preDrawDepth)
  {
    D3D11_DEPTH_STENCIL_DESC cur = {0};

    UINT stencilRef = 0;

    {
      ID3D11DepthStencilState *os = NULL;
      m_pImmediateContext->OMGetDepthStencilState(&os, &stencilRef);

      if(os)
      {
        os->GetDesc(&cur);
        SAFE_RELEASE(os);
      }
      else
      {
        cur.DepthEnable = TRUE;
        cur.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        cur.DepthFunc = D3D11_COMPARISON_LESS;    // default depth func
        cur.StencilEnable = FALSE;
        cur.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
        cur.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
        cur.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        cur.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        cur.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        cur.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        cur.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        cur.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
      }
    }

    // make sure that if a test is disabled, it shows all
    // pixels passing
    if(!cur.DepthEnable)
      cur.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if(!cur.StencilEnable)
      cur.StencilEnable = D3D11_COMPARISON_ALWAYS;

    if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
    {
      ID3D11DepthStencilState *os = NULL;

      D3D11_DEPTH_STENCIL_DESC d = dsDesc;

      if(overlay == DebugOverlay::Depth)
      {
        dsDesc.DepthEnable = d.DepthEnable = TRUE;
        dsDesc.StencilEnable = d.StencilEnable = FALSE;

        switch(cur.DepthFunc)
        {
          case D3D11_COMPARISON_ALWAYS: d.DepthFunc = D3D11_COMPARISON_NEVER; break;
          case D3D11_COMPARISON_NEVER: d.DepthFunc = D3D11_COMPARISON_ALWAYS; break;

          case D3D11_COMPARISON_EQUAL: d.DepthFunc = D3D11_COMPARISON_NOT_EQUAL; break;
          case D3D11_COMPARISON_NOT_EQUAL: d.DepthFunc = D3D11_COMPARISON_EQUAL; break;

          case D3D11_COMPARISON_LESS: d.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; break;
          case D3D11_COMPARISON_GREATER_EQUAL: d.DepthFunc = D3D11_COMPARISON_LESS; break;

          case D3D11_COMPARISON_GREATER: d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; break;
          case D3D11_COMPARISON_LESS_EQUAL: d.DepthFunc = D3D11_COMPARISON_GREATER; break;
        }
      }
      else if(overlay == DebugOverlay::Stencil)
      {
        dsDesc.DepthEnable = d.DepthEnable = FALSE;
        dsDesc.StencilEnable = d.StencilEnable = TRUE;

        d.FrontFace = cur.FrontFace;
        d.BackFace = cur.BackFace;
        dsDesc.StencilReadMask = d.StencilReadMask = cur.StencilReadMask;
        dsDesc.StencilWriteMask = d.StencilWriteMask = cur.StencilWriteMask;

        switch(cur.FrontFace.StencilFunc)
        {
          case D3D11_COMPARISON_ALWAYS: d.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER; break;
          case D3D11_COMPARISON_NEVER: d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; break;

          case D3D11_COMPARISON_EQUAL: d.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL; break;
          case D3D11_COMPARISON_NOT_EQUAL: d.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL; break;

          case D3D11_COMPARISON_LESS:
            d.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
            break;
          case D3D11_COMPARISON_GREATER_EQUAL:
            d.FrontFace.StencilFunc = D3D11_COMPARISON_LESS;
            break;

          case D3D11_COMPARISON_GREATER:
            d.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
            break;
          case D3D11_COMPARISON_LESS_EQUAL:
            d.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
            break;
        }

        switch(cur.BackFace.StencilFunc)
        {
          case D3D11_COMPARISON_ALWAYS: d.BackFace.StencilFunc = D3D11_COMPARISON_NEVER; break;
          case D3D11_COMPARISON_NEVER: d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS; break;

          case D3D11_COMPARISON_EQUAL: d.BackFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL; break;
          case D3D11_COMPARISON_NOT_EQUAL: d.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL; break;

          case D3D11_COMPARISON_LESS:
            d.BackFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
            break;
          case D3D11_COMPARISON_GREATER_EQUAL:
            d.BackFace.StencilFunc = D3D11_COMPARISON_LESS;
            break;

          case D3D11_COMPARISON_GREATER:
            d.BackFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
            break;
          case D3D11_COMPARISON_LESS_EQUAL:
            d.BackFace.StencilFunc = D3D11_COMPARISON_GREATER;
            break;
        }
      }

      SAFE_RELEASE(os);
      hr = m_pDevice->CreateDepthStencilState(&d, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create depth/stencil overlay depth state %08x", hr);
        return m_OverlayResourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

      m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

      float redConsts[] = {255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f};

      ID3D11Buffer *buf = MakeCBuffer(redConsts, sizeof(redConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      SAFE_RELEASE(os);

      m_pImmediateContext->CopyResource(renderDepth, preDrawDepth);

      d = dsDesc;

      if(overlay == DebugOverlay::Depth)
      {
        d.DepthFunc = cur.DepthFunc;
      }
      else if(overlay == DebugOverlay::Stencil)
      {
        d.FrontFace = cur.FrontFace;
        d.BackFace = cur.BackFace;
      }

      hr = m_pDevice->CreateDepthStencilState(&d, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create depth/stencil overlay depth state 2 %08x", hr);
        return m_OverlayResourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, stencilRef);

      float greenConsts[] = {0.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f};

      buf = MakeCBuffer(greenConsts, sizeof(greenConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      SAFE_RELEASE(os);
    }
  }
  else
  {
    // no depth? trivial pass for depth or stencil tests
    if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
    {
      m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

      dsDesc.DepthEnable = FALSE;
      dsDesc.StencilEnable = FALSE;

      ID3D11DepthStencilState *os = NULL;
      hr = m_pDevice->CreateDepthStencilState(&dsDesc, &os);
      if(FAILED(hr))
      {
        RDCERR("Failed to create drawcall depth stencil state %08x", hr);
        return m_OverlayResourceId;
      }

      m_pImmediateContext->OMSetDepthStencilState(os, 0);

      m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

      ID3D11RasterizerState *rs = NULL;
      {
        D3D11_RASTERIZER_DESC rdesc;

        rdesc.FillMode = D3D11_FILL_SOLID;
        rdesc.CullMode = D3D11_CULL_NONE;
        rdesc.FrontCounterClockwise = FALSE;
        rdesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        rdesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rdesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rdesc.DepthClipEnable = FALSE;
        rdesc.ScissorEnable = FALSE;
        rdesc.MultisampleEnable = FALSE;
        rdesc.AntialiasedLineEnable = FALSE;

        hr = m_pDevice->CreateRasterizerState(&rdesc, &rs);
        if(FAILED(hr))
        {
          RDCERR("Failed to create drawcall rast state %08x", hr);
          return m_OverlayResourceId;
        }
      }

      float clearColour[] = {0.0f, 1.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, clearColour);

      float overlayConsts[] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3D11Buffer *buf = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &buf);

      m_pImmediateContext->RSSetState(rs);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      SAFE_RELEASE(os);
      SAFE_RELEASE(rs);
    }
    else
    {
      RDCERR("Unhandled overlay case!");
    }
  }

  SAFE_RELEASE(rtv);

  SAFE_RELEASE(renderDepth);
  SAFE_RELEASE(preDrawDepth);

  return m_OverlayResourceId;
}

struct CopyPixelParams
{
  bool multisampled;
  bool floatTex;
  bool uintTex;
  bool intTex;

  UINT subres;

  bool depthcopy;     // are we copying depth or colour
  bool depthbound;    // if copying depth, was any depth bound (or should we write <-1,-1> marker)

  ID3D11Texture2D *sourceTex;    // texture with the actual data in it
  ID3D11Texture2D *srvTex;       // could be same as sourceTex if sourceTex had BIND_SRV flag on,
                                 // otherwise a texture of same format with BIND_SRV to copy to

  ID3D11ShaderResourceView *srv[2];    // srv[0] = colour or depth, srv[1] = stencil or NULL

  ID3D11UnorderedAccessView *uav;    // uav to copy pixel to

  ID3D11Buffer *srcxyCBuf;
  ID3D11Buffer *storexyCBuf;
};

void D3D11DebugManager::PixelHistoryCopyPixel(CopyPixelParams &p, uint32_t x, uint32_t y)
{
  // perform a subresource copy if the real source tex couldn't be directly bound as SRV
  if(p.sourceTex != p.srvTex && p.sourceTex && p.srvTex)
    m_pImmediateContext->CopySubresourceRegion(p.srvTex, p.subres, 0, 0, 0, p.sourceTex, p.subres,
                                               NULL);

  ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

  uint32_t UAVStartSlot = 0;
  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(tmpViews[i] != NULL)
    {
      UAVStartSlot = i + 1;
      SAFE_RELEASE(tmpViews[i]);
    }
  }

  ID3D11RenderTargetView *prevRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11UnorderedAccessView *prevUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
  ID3D11DepthStencilView *prevDSV = NULL;
  const UINT numUAVs =
      m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
      UAVStartSlot, prevRTVs, &prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  ID3D11ComputeShader *curCS = NULL;
  ID3D11ClassInstance *curCSInst[D3D11_SHADER_MAX_INTERFACES] = {NULL};
  UINT curCSNumInst = D3D11_SHADER_MAX_INTERFACES;
  ID3D11Buffer *curCSCBuf[2] = {0};
  ID3D11ShaderResourceView *curCSSRVs[10] = {0};
  ID3D11UnorderedAccessView *curCSUAV[4] = {0};
  UINT initCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&initCounts[0], 0xff, sizeof(initCounts));

  m_pImmediateContext->CSGetShader(&curCS, curCSInst, &curCSNumInst);
  m_pImmediateContext->CSGetConstantBuffers(0, ARRAY_COUNT(curCSCBuf), curCSCBuf);
  m_pImmediateContext->CSGetShaderResources(0, ARRAY_COUNT(curCSSRVs), curCSSRVs);
  m_pImmediateContext->CSGetUnorderedAccessViews(0, ARRAY_COUNT(curCSUAV), curCSUAV);

  uint32_t storexyData[4] = {x, y, uint32_t(p.depthcopy), uint32_t(p.srv[1] != NULL)};

  D3D11_MAPPED_SUBRESOURCE mapped;
  m_pImmediateContext->Map(p.storexyCBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  memcpy(mapped.pData, storexyData, sizeof(storexyData));

  m_pImmediateContext->Unmap(p.storexyCBuf, 0);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &p.srcxyCBuf);
  m_pImmediateContext->CSSetConstantBuffers(1, 1, &p.storexyCBuf);

  UINT offs = 0;

  if(p.depthcopy)
  {
    offs = 0;
  }
  else
  {
    if(p.floatTex)
      offs = 1;
    else if(p.uintTex)
      offs = 2;
    else if(p.intTex)
      offs = 3;
  }

  m_pImmediateContext->CSSetUnorderedAccessViews(offs, 1, &p.uav, initCounts);

  if(p.depthcopy)
  {
    offs = p.multisampled ? 2 : 0;
  }
  else
  {
    if(p.floatTex)
      offs = 4;
    else if(p.uintTex)
      offs = 6;
    else if(p.intTex)
      offs = 8;

    if(p.multisampled)
      offs++;
  }

  m_pImmediateContext->CSSetShaderResources(offs, 2, p.srv);

  m_pImmediateContext->CSSetShader(!p.depthcopy || p.depthbound ? m_DebugRender.PixelHistoryCopyCS
                                                                : m_DebugRender.PixelHistoryUnusedCS,
                                   NULL, 0);
  m_pImmediateContext->Dispatch(1, 1, 1);

  m_pImmediateContext->CSSetShader(curCS, curCSInst, curCSNumInst);
  m_pImmediateContext->CSSetConstantBuffers(0, ARRAY_COUNT(curCSCBuf), curCSCBuf);
  m_pImmediateContext->CSSetShaderResources(0, ARRAY_COUNT(curCSSRVs), curCSSRVs);
  m_pImmediateContext->CSSetUnorderedAccessViews(0, ARRAY_COUNT(curCSUAV), curCSUAV, initCounts);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
      UAVStartSlot, prevRTVs, prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs, initCounts);

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    SAFE_RELEASE(prevRTVs[i]);
  for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    SAFE_RELEASE(prevUAVs[i]);
  SAFE_RELEASE(prevDSV);

  SAFE_RELEASE(curCS);
  for(UINT i = 0; i < curCSNumInst; i++)
    SAFE_RELEASE(curCSInst[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSCBuf); i++)
    SAFE_RELEASE(curCSCBuf[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSSRVs); i++)
    SAFE_RELEASE(curCSSRVs[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSUAV); i++)
    SAFE_RELEASE(curCSUAV[i]);
}

vector<PixelModification> D3D11DebugManager::PixelHistory(vector<EventUsage> events,
                                                          ResourceId target, uint32_t x, uint32_t y,
                                                          uint32_t slice, uint32_t mip,
                                                          uint32_t sampleIdx, CompType typeHint)
{
  vector<PixelModification> history;

  // this function needs a *huge* amount of tidying, refactoring and documenting.

  if(events.empty())
    return history;

  TextureShaderDetails details = GetShaderDetails(target, typeHint, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return history;

  D3D11MarkerRegion historyMarker(
      StringFormat::Fmt("Doing PixelHistory on %llu, (%u,%u) %u, %u, %u over %u events", target, x,
                        y, slice, mip, sampleIdx, (uint32_t)events.size()));

  details.texFmt = GetNonSRGBFormat(details.texFmt);
  details.texFmt = GetTypedFormat(details.texFmt, typeHint);

  SCOPED_TIMER("D3D11DebugManager::PixelHistory");

  if(sampleIdx > details.sampleCount)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (details.sampleCount > 1);

  // sampleIdx used later for deciding subresource to read from, so
  // set it to 0 for the no-sample case (resolved, or never MSAA in the
  // first place).
  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  // needed for comparison with viewports
  float xf = (float)x;
  float yf = (float)y;

  RDCDEBUG("Checking Pixel History on %llu (%u, %u) with %u possible events", target, x, y,
           (uint32_t)events.size());

  // these occlusion queries are run with every test possible disabled
  vector<ID3D11Query *> occl;
  occl.reserve(events.size());

  ID3D11Query *testQueries[6] = {0};    // one query for each test we do per-drawcall

  uint32_t pixstoreStride = 4;

  // reserve 3 pixels per draw (worst case all events). This is used for Pre value, Post value and
  // # frag overdraw (with & without original shader). It's reused later to retrieve per-fragment
  // post values.
  uint32_t pixstoreSlots = (uint32_t)(events.size() * pixstoreStride);

  // need UAV compatible format, so switch B8G8R8A8 for R8G8B8A8, everything will
  // render as normal and it will just be swizzled (which we were doing manually anyway).
  if(details.texFmt == DXGI_FORMAT_B8G8R8A8_UNORM)
    details.texFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

  // other transformations, B8G8R8X8 also as R8G8B8A8 (alpha will be ignored)
  if(details.texFmt == DXGI_FORMAT_B8G8R8X8_UNORM)
    details.texFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

  // R32G32B32 as R32G32B32A32 (alpha will be ignored)
  if(details.texFmt == DXGI_FORMAT_R32G32B32_FLOAT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
  if(details.texFmt == DXGI_FORMAT_R32G32B32_UINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_UINT;
  if(details.texFmt == DXGI_FORMAT_R32G32B32_SINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_SINT;

  // these formats are only valid for depth textures at which point pixstore doesn't matter, so it
  // can be anything.
  if(details.texFmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS ||
     details.texFmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
     details.texFmt == DXGI_FORMAT_R24G8_TYPELESS || details.texFmt == DXGI_FORMAT_D24_UNORM_S8_UINT ||

     details.texFmt == DXGI_FORMAT_D16_UNORM ||

     details.texFmt == DXGI_FORMAT_D32_FLOAT ||

     details.texFmt == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
     details.texFmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
     details.texFmt == DXGI_FORMAT_R32G8X24_TYPELESS ||
     details.texFmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_UINT;

  // define a texture that we can copy before/after results into.
  // We always allocate at least 2048 slots, to allow for pixel history that only touches a couple
  // of events still being able to overdraw many times. The idea being that if we're taking the
  // history over many events, then the events which don't take up any slots or only one will mostly
  // dominate over those that take more than the average. If we only have one or two candidate
  // events then at least 2048 slots gives a huge amount of potential overdraw.
  D3D11_TEXTURE2D_DESC pixstoreDesc = {
      2048U,
      RDCMAX(1U, (pixstoreSlots / 2048) + 1),
      1U,
      1U,
      details.texFmt,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_UNORDERED_ACCESS,
      0,
      0,
  };

  ID3D11Texture2D *pixstore = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &pixstore);

  // This is used for shader output values.
  pixstoreDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

  ID3D11Texture2D *shadoutStore = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &shadoutStore);

  // we use R32G32 so that we can bind this buffer as UAV and write to both depth and stencil
  // components.
  // the shader does the upcasting for us when we read from depth or stencil
  pixstoreDesc.Format = DXGI_FORMAT_R32G32_FLOAT;

  ID3D11Texture2D *pixstoreDepth = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &pixstoreDepth);

  pixstoreDesc.Usage = D3D11_USAGE_STAGING;
  pixstoreDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  pixstoreDesc.BindFlags = 0;

  pixstoreDesc.Format = details.texFmt;

  ID3D11Texture2D *pixstoreReadback = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &pixstoreReadback);

  pixstoreDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

  ID3D11Texture2D *shadoutStoreReadback = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &shadoutStoreReadback);

  pixstoreDesc.Format = DXGI_FORMAT_R32G32_FLOAT;

  ID3D11Texture2D *pixstoreDepthReadback = NULL;
  m_pDevice->CreateTexture2D(&pixstoreDesc, NULL, &pixstoreDepthReadback);

  ID3D11UnorderedAccessView *pixstoreUAV = NULL;
  m_pDevice->CreateUnorderedAccessView(pixstore, NULL, &pixstoreUAV);

  ID3D11UnorderedAccessView *shadoutStoreUAV = NULL;
  m_pDevice->CreateUnorderedAccessView(shadoutStore, NULL, &shadoutStoreUAV);

  ID3D11UnorderedAccessView *pixstoreDepthUAV = NULL;
  m_pDevice->CreateUnorderedAccessView(pixstoreDepth, NULL, &pixstoreDepthUAV);

  // very wasteful, but we must leave the viewport as is to get correct rasterisation which means
  // same dimensions of render target.
  D3D11_TEXTURE2D_DESC shadoutDesc = {
      details.texWidth,
      details.texHeight,
      1U,
      1U,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      {details.sampleCount, details.sampleQuality},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      0,
      0,
  };
  ID3D11Texture2D *shadOutput = NULL;
  m_pDevice->CreateTexture2D(&shadoutDesc, NULL, &shadOutput);

  ID3D11ShaderResourceView *shadOutputSRV = NULL;
  m_pDevice->CreateShaderResourceView(shadOutput, NULL, &shadOutputSRV);

  ID3D11RenderTargetView *shadOutputRTV = NULL;
  m_pDevice->CreateRenderTargetView(shadOutput, NULL, &shadOutputRTV);

  shadoutDesc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
  shadoutDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
  ID3D11Texture2D *shaddepthOutput = NULL;
  m_pDevice->CreateTexture2D(&shadoutDesc, NULL, &shaddepthOutput);

  ID3D11DepthStencilView *shaddepthOutputDSV = NULL;
  {
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    desc.Flags = 0;
    desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;

    if(multisampled)
      desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    m_pDevice->CreateDepthStencilView(shaddepthOutput, &desc, &shaddepthOutputDSV);
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC copyDepthSRVDesc, copyStencilSRVDesc;
  copyDepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  copyDepthSRVDesc.Texture2D.MipLevels = 1;
  copyDepthSRVDesc.Texture2D.MostDetailedMip = 0;
  copyStencilSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  copyStencilSRVDesc.Texture2D.MipLevels = 1;
  copyStencilSRVDesc.Texture2D.MostDetailedMip = 0;

  if(multisampled)
    copyDepthSRVDesc.ViewDimension = copyStencilSRVDesc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2DMS;

  ID3D11ShaderResourceView *shaddepthOutputDepthSRV = NULL, *shaddepthOutputStencilSRV = NULL;

  {
    copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    m_pDevice->CreateShaderResourceView(shaddepthOutput, &copyDepthSRVDesc, &shaddepthOutputDepthSRV);
    copyDepthSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    m_pDevice->CreateShaderResourceView(shaddepthOutput, &copyDepthSRVDesc,
                                        &shaddepthOutputStencilSRV);
  }

  // depth texture to copy to, as CopySubresourceRegion can't copy single pixels out of a depth
  // buffer,
  // and we can't guarantee that the original depth texture is SRV-compatible to allow single-pixel
  // copies
  // via compute shader.
  //
  // Due to copies having to match formats between source and destination we don't create these
  // textures up
  // front but on demand, and resize up as necessary. We do a whole copy from this, then a CS copy
  // via SRV to UAV
  // to copy into the pixstore (which we do a final copy to for readback). The extra step is
  // necessary as
  // you can Copy to a staging texture but you can't use a CS, which we need for single-pixel depth
  // (and stencil) copy.

  D3D11_TEXTURE2D_DESC depthCopyD24S8Desc = {
      details.texWidth,
      details.texHeight,
      details.texMips,
      details.texArraySize,
      DXGI_FORMAT_R24G8_TYPELESS,
      {details.sampleCount, details.sampleQuality},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE,
      0,
      0,
  };
  ID3D11Texture2D *depthCopyD24S8 = NULL;
  ID3D11ShaderResourceView *depthCopyD24S8_DepthSRV = NULL, *depthCopyD24S8_StencilSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD32S8Desc = depthCopyD24S8Desc;
  depthCopyD32S8Desc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
  ID3D11Texture2D *depthCopyD32S8 = NULL;
  ID3D11ShaderResourceView *depthCopyD32S8_DepthSRV = NULL, *depthCopyD32S8_StencilSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD32Desc = depthCopyD32S8Desc;
  depthCopyD32Desc.Format = DXGI_FORMAT_R32_TYPELESS;
  ID3D11Texture2D *depthCopyD32 = NULL;
  ID3D11ShaderResourceView *depthCopyD32_DepthSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD16Desc = depthCopyD24S8Desc;
  depthCopyD16Desc.Format = DXGI_FORMAT_R16_TYPELESS;
  ID3D11Texture2D *depthCopyD16 = NULL;
  ID3D11ShaderResourceView *depthCopyD16_DepthSRV = NULL;

  bool floatTex = false, uintTex = false, intTex = false;

  if(IsUIntFormat(details.texFmt) || IsTypelessFormat(details.texFmt))
  {
    uintTex = true;
  }
  else if(IsIntFormat(details.texFmt))
  {
    intTex = true;
  }
  else
  {
    floatTex = true;
  }

  uint32_t srcxyData[8] = {
      x,
      y,
      multisampled ? sampleIdx : mip,
      slice,

      uint32_t(multisampled),
      uint32_t(floatTex),
      uint32_t(uintTex),
      uint32_t(intTex),
  };

  uint32_t shadoutsrcxyData[8];
  memcpy(shadoutsrcxyData, srcxyData, sizeof(srcxyData));

  // shadout texture doesn't have slices/mips, just one of the right dimension
  shadoutsrcxyData[2] = multisampled ? sampleIdx : 0;
  shadoutsrcxyData[3] = 0;

  ID3D11Buffer *srcxyCBuf = MakeCBuffer(sizeof(srcxyData));
  ID3D11Buffer *shadoutsrcxyCBuf = MakeCBuffer(sizeof(shadoutsrcxyData));
  ID3D11Buffer *storexyCBuf = MakeCBuffer(sizeof(srcxyData));

  FillCBuffer(srcxyCBuf, srcxyData, sizeof(srcxyData));
  FillCBuffer(shadoutsrcxyCBuf, shadoutsrcxyData, sizeof(shadoutsrcxyData));

  // so we do:
  // per sample: orig depth --copy--> depthCopyXXX (created/upsized on demand) --CS pixel copy-->
  // pixstoreDepth
  // at end: pixstoreDepth --copy--> pixstoreDepthReadback
  //
  // First copy is only needed if orig depth is not SRV-able
  // CS pixel copy is needed since it's the only way to copy only one pixel from depth texture,
  // CopySubresourceRegion
  // can't copy a sub-box of a depth copy. It also is required in the MSAA case to read a specific
  // pixel/sample out.
  //
  // final copy is needed to get data into a readback texture since we can't have CS writing to
  // staging texture
  //
  //
  // for colour it's simple, it's just
  // per sample: orig color --copy--> pixstore
  // at end: pixstore --copy--> pixstoreReadback
  //
  // this is slightly redundant but it only adds one extra copy at the end and an extra target, and
  // allows to handle
  // MSAA source textures (which can't copy direct to a staging texture)

  ID3D11Resource *targetres = NULL;

  if(WrappedID3D11Texture1D::m_TextureList.find(target) != WrappedID3D11Texture1D::m_TextureList.end())
    targetres = WrappedID3D11Texture1D::m_TextureList[target].m_Texture;
  else if(WrappedID3D11Texture2D1::m_TextureList.find(target) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
    targetres = WrappedID3D11Texture2D1::m_TextureList[target].m_Texture;
  else if(WrappedID3D11Texture3D1::m_TextureList.find(target) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
    targetres = WrappedID3D11Texture3D1::m_TextureList[target].m_Texture;

  CopyPixelParams colourCopyParams = {};

  // common parameters
  colourCopyParams.multisampled = multisampled;
  colourCopyParams.floatTex = floatTex;
  colourCopyParams.uintTex = uintTex;
  colourCopyParams.intTex = intTex;
  colourCopyParams.srcxyCBuf = srcxyCBuf;
  colourCopyParams.storexyCBuf = storexyCBuf;
  colourCopyParams.subres = details.texArraySize * slice + mip;

  CopyPixelParams depthCopyParams = colourCopyParams;

  colourCopyParams.depthcopy = false;
  colourCopyParams.sourceTex = (ID3D11Texture2D *)targetres;
  colourCopyParams.srvTex = (ID3D11Texture2D *)details.srvResource;
  colourCopyParams.srv[0] = details.srv[details.texType];
  colourCopyParams.srv[1] = NULL;
  colourCopyParams.uav = pixstoreUAV;

  depthCopyParams.depthcopy = true;
  depthCopyParams.uav = pixstoreDepthUAV;

  // while issuing the above queries we can check to see which tests are enabled so we don't
  // bother checking if depth testing failed if the depth test was disabled
  vector<uint32_t> flags(events.size());
  enum
  {
    TestEnabled_BackfaceCulling = 1 << 0,
    TestEnabled_DepthClip = 1 << 1,
    TestEnabled_Scissor = 1 << 2,
    TestEnabled_DepthTesting = 1 << 3,
    TestEnabled_StencilTesting = 1 << 4,

    // important to know if blending is enabled or not as we currently skip a bunch of stuff
    // and only pay attention to the final passing fragment if blending is off
    Blending_Enabled = 1 << 5,

    // additional flags we can trivially detect on the CPU for edge cases
    TestMustFail_Scissor =
        1 << 6,    // if the scissor is enabled, pixel lies outside all regions (could be only one)
    TestMustPass_Scissor =
        1 << 7,    // if the scissor is enabled, pixel lies inside all regions (could be only one)
    TestMustFail_DepthTesting = 1 << 8,      // if the comparison func is NEVER
    TestMustFail_StencilTesting = 1 << 9,    // if the comparison func is NEVER for both faces, or
                                             // one face is backface culled and the other is NEVER

    // if the sample mask set at this event doesn't have the right bit set
    TestMustFail_SampleMask = 1 << 10,
  };

#if 1
  BOOL occlData = 0;
  const D3D11_QUERY_DESC occlDesc = {D3D11_QUERY_OCCLUSION_PREDICATE, 0};
#else
  UINT64 occlData = 0;
  const D3D11_QUERY_DESC occlDesc = {D3D11_QUERY_OCCLUSION, 0};
#endif

  HRESULT hr = S_OK;

  for(size_t i = 0; i < events.size(); i++)
  {
    ID3D11Query *q = NULL;
    m_pDevice->CreateQuery(&occlDesc, &q);
    occl.push_back(q);
  }

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
    m_pDevice->CreateQuery(&occlDesc, &testQueries[i]);

  //////////////////////////////////////////////////////////////////
  // Check that everything we need has successfully created.
  // We free everything together at the end

  bool allCreated = true;

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
  {
    if(!testQueries[i])
    {
      RDCERR("Failed to create test query %d", i);
      allCreated = false;
    }
  }

  if(!pixstore || !pixstoreUAV || !pixstoreReadback)
  {
    RDCERR("Failed to create pixstore (%p %p %p) (%u slots @ fmt %u)", pixstore, pixstoreUAV,
           pixstoreReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!pixstoreDepth || !pixstoreDepthUAV || !pixstoreDepthReadback)
  {
    RDCERR("Failed to create pixstoreDepth (%p %p %p) (%u slots @ fmt %u)", pixstoreDepth,
           pixstoreDepthUAV, pixstoreDepthReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!shadoutStore || !shadoutStoreUAV || !shadoutStoreReadback)
  {
    RDCERR("Failed to create shadoutStore (%p %p %p) (%u slots @ fmt %u)", shadoutStore,
           shadoutStoreUAV, shadoutStoreReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!shadOutput || !shadOutputSRV || !shadOutputRTV)
  {
    RDCERR("Failed to create shadoutStore (%p %p %p) (%ux%u [%u,%u] @ fmt %u)", shadOutput,
           shadOutputSRV, shadOutputRTV, details.texWidth, details.texHeight, details.sampleCount,
           details.sampleQuality, details.texFmt);
    allCreated = false;
  }

  if(!shaddepthOutput || !shaddepthOutputDSV || !shaddepthOutputDepthSRV || !shaddepthOutputStencilSRV)
  {
    RDCERR("Failed to create shadoutStore (%p %p %p %p) (%ux%u [%u,%u] @ fmt %u)", shaddepthOutput,
           shaddepthOutputDSV, shaddepthOutputDepthSRV, shaddepthOutputStencilSRV, details.texWidth,
           details.texHeight, details.sampleCount, details.sampleQuality, details.texFmt);
    allCreated = false;
  }

  if(!srcxyCBuf || !storexyCBuf)
  {
    RDCERR("Failed to create cbuffers (%p %p)", srcxyCBuf, storexyCBuf);
    allCreated = false;
  }

  if(!allCreated)
  {
    for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
      SAFE_RELEASE(testQueries[i]);

    SAFE_RELEASE(pixstore);
    SAFE_RELEASE(shadoutStore);
    SAFE_RELEASE(pixstoreDepth);

    SAFE_RELEASE(pixstoreReadback);
    SAFE_RELEASE(shadoutStoreReadback);
    SAFE_RELEASE(pixstoreDepthReadback);

    SAFE_RELEASE(pixstoreUAV);
    SAFE_RELEASE(shadoutStoreUAV);
    SAFE_RELEASE(pixstoreDepthUAV);

    SAFE_RELEASE(shadOutput);
    SAFE_RELEASE(shadOutputSRV);
    SAFE_RELEASE(shadOutputRTV);
    SAFE_RELEASE(shaddepthOutput);
    SAFE_RELEASE(shaddepthOutputDSV);
    SAFE_RELEASE(shaddepthOutputDepthSRV);
    SAFE_RELEASE(shaddepthOutputStencilSRV);

    SAFE_RELEASE(depthCopyD24S8);
    SAFE_RELEASE(depthCopyD24S8_DepthSRV);
    SAFE_RELEASE(depthCopyD24S8_StencilSRV);

    SAFE_RELEASE(depthCopyD32S8);
    SAFE_RELEASE(depthCopyD32S8_DepthSRV);
    SAFE_RELEASE(depthCopyD32S8_StencilSRV);

    SAFE_RELEASE(depthCopyD32);
    SAFE_RELEASE(depthCopyD32_DepthSRV);

    SAFE_RELEASE(depthCopyD16);
    SAFE_RELEASE(depthCopyD16_DepthSRV);

    SAFE_RELEASE(srcxyCBuf);
    SAFE_RELEASE(shadoutsrcxyCBuf);
    SAFE_RELEASE(storexyCBuf);

    return history;
  }

  m_WrappedDevice->ReplayLog(0, events[0].eventID, eReplay_WithoutDraw);

  ID3D11RasterizerState *curRS = NULL;
  ID3D11RasterizerState *newRS = NULL;
  ID3D11DepthStencilState *newDS = NULL;
  ID3D11PixelShader *curPS = NULL;
  ID3D11ClassInstance *curInst[D3D11_SHADER_MAX_INTERFACES] = {NULL};
  UINT curNumInst = 0;
  UINT curNumViews = 0;
  UINT curNumScissors = 0;
  D3D11_VIEWPORT curViewports[16] = {0};
  D3D11_RECT curScissors[16] = {0};
  D3D11_RECT newScissors[16] = {0};
  ID3D11BlendState *curBS = NULL;
  float blendFactor[4] = {0};
  UINT curSample = 0;
  ID3D11DepthStencilState *curDS = NULL;
  UINT stencilRef = 0;

  ////////////////////////////////////////////////////////////////////////
  // Main loop over each event to determine if it rasterized to this pixel

  for(size_t ev = 0; ev < events.size(); ev++)
  {
    curNumInst = D3D11_SHADER_MAX_INTERFACES;
    curNumScissors = curNumViews = 16;

    bool uavOutput =
        ((events[ev].usage >= ResourceUsage::VS_RWResource &&
          events[ev].usage <= ResourceUsage::CS_RWResource) ||
         events[ev].usage == ResourceUsage::CopyDst || events[ev].usage == ResourceUsage::Copy ||
         events[ev].usage == ResourceUsage::Resolve ||
         events[ev].usage == ResourceUsage::ResolveDst || events[ev].usage == ResourceUsage::GenMips);

    m_pImmediateContext->RSGetState(&curRS);
    m_pImmediateContext->OMGetBlendState(&curBS, blendFactor, &curSample);
    m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
    m_pImmediateContext->PSGetShader(&curPS, curInst, &curNumInst);
    m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
    m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);

    // defaults (mostly)
    // disable tests/clips and enable scissor as we need it to clip visibility to just our pixel
    D3D11_RASTERIZER_DESC rd = {
        /*FillMode =*/D3D11_FILL_SOLID,
        /*CullMode =*/D3D11_CULL_NONE,
        /*FrontCounterClockwise =*/FALSE,
        /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
        /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
        /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /*DepthClipEnable =*/FALSE,
        /*ScissorEnable =*/TRUE,
        /*MultisampleEnable =*/FALSE,
        /*AntialiasedLineEnable =*/FALSE,
    };

    D3D11_RASTERIZER_DESC rsDesc = {
        /*FillMode =*/D3D11_FILL_SOLID,
        /*CullMode =*/D3D11_CULL_BACK,
        /*FrontCounterClockwise =*/FALSE,
        /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
        /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
        /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /*DepthClipEnable =*/TRUE,
        /*ScissorEnable =*/FALSE,
        /*MultisampleEnable =*/FALSE,
        /*AntialiasedLineEnable =*/FALSE,
    };

    if(curRS)
    {
      curRS->GetDesc(&rsDesc);

      rd = rsDesc;

      if(rd.CullMode != D3D11_CULL_NONE)
        flags[ev] |= TestEnabled_BackfaceCulling;
      if(rd.DepthClipEnable)
        flags[ev] |= TestEnabled_DepthClip;
      if(rd.ScissorEnable)
        flags[ev] |= TestEnabled_Scissor;

      rd.CullMode = D3D11_CULL_NONE;
      rd.DepthClipEnable = FALSE;

      rd.ScissorEnable = TRUE;
    }
    else
    {
      rsDesc.CullMode = D3D11_CULL_BACK;
      rsDesc.ScissorEnable = FALSE;

      // defaults
      flags[ev] |= (TestEnabled_BackfaceCulling | TestEnabled_DepthClip);
    }

    if(curDS)
    {
      D3D11_DEPTH_STENCIL_DESC dsDesc;
      curDS->GetDesc(&dsDesc);

      if(dsDesc.DepthEnable)
      {
        if(dsDesc.DepthFunc != D3D11_COMPARISON_ALWAYS)
          flags[ev] |= TestEnabled_DepthTesting;

        if(dsDesc.DepthFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_DepthTesting;
      }

      if(dsDesc.StencilEnable)
      {
        if(dsDesc.FrontFace.StencilFunc != D3D11_COMPARISON_ALWAYS ||
           dsDesc.BackFace.StencilFunc != D3D11_COMPARISON_ALWAYS)
          flags[ev] |= TestEnabled_StencilTesting;

        if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER &&
           dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_StencilTesting;

        if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER &&
           rsDesc.CullMode == D3D11_CULL_BACK)
          flags[ev] |= TestMustFail_StencilTesting;

        if(rsDesc.CullMode == D3D11_CULL_FRONT &&
           dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_StencilTesting;
      }
    }
    else
    {
      // defaults
      flags[ev] |= TestEnabled_DepthTesting;
    }

    if(rsDesc.ScissorEnable)
    {
      // see if we can find at least one scissor region this pixel could fall into
      bool inRegion = false;
      bool inAllRegions = true;

      for(UINT i = 0; i < curNumScissors && i < curNumViews; i++)
      {
        if(xf >= float(curScissors[i].left) && yf >= float(curScissors[i].top) &&
           xf < float(curScissors[i].right) && yf < float(curScissors[i].bottom))
        {
          inRegion = true;
        }
        else
        {
          inAllRegions = false;
        }
      }

      if(!inRegion)
        flags[ev] |= TestMustFail_Scissor;
      if(inAllRegions)
        flags[ev] |= TestMustPass_Scissor;
    }

    if(curBS)
    {
      D3D11_BLEND_DESC desc;
      curBS->GetDesc(&desc);

      if(desc.IndependentBlendEnable)
      {
        for(int i = 0; i < 8; i++)
        {
          if(desc.RenderTarget[i].BlendEnable)
          {
            flags[ev] |= Blending_Enabled;
            break;
          }
        }
      }
      else
      {
        if(desc.RenderTarget[0].BlendEnable)
          flags[ev] |= Blending_Enabled;
      }
    }
    else
    {
      // no blending enabled by default
    }

    // sampleMask is a mask containing only the bit for the sample we want
    // (or 0xFFFFFFFF if no sample was chosen and we are looking at them all).
    if((curSample & sampleMask) == 0)
    {
      flags[ev] |= TestMustFail_SampleMask;
    }

    m_pDevice->CreateRasterizerState(&rd, &newRS);
    m_pImmediateContext->RSSetState(newRS);
    SAFE_RELEASE(newRS);

    m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);

    m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.NopDepthState, stencilRef);

    for(UINT i = 0; i < curNumViews; i++)
    {
      // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

      // if (x,y) pixel isn't in viewport, make empty rect)
      if(xf < curViewports[i].TopLeftX || yf < curViewports[i].TopLeftY ||
         xf >= curViewports[i].TopLeftX + curViewports[i].Width ||
         yf >= curViewports[i].TopLeftY + curViewports[i].Height)
      {
        newScissors[i].left = newScissors[i].top = newScissors[i].bottom = newScissors[i].right = 0;
      }
      else
      {
        newScissors[i].left = LONG(x);
        newScissors[i].top = LONG(y);
        newScissors[i].right = newScissors[i].left + 1;
        newScissors[i].bottom = newScissors[i].top + 1;
      }
    }

    // scissor every viewport
    m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

    // figure out where this event lies in the pixstore texture
    UINT storex = UINT(ev % (2048 / pixstoreStride));
    UINT storey = UINT(ev / (2048 / pixstoreStride));

    bool depthBound = false;
    ID3D11Texture2D **copyTex = NULL;
    ID3D11ShaderResourceView **copyDepthSRV = NULL;
    ID3D11ShaderResourceView **copyStencilSRV = NULL;
    ID3D11Resource *depthRes = NULL;

    // if the depth resource was already BIND_SRV we just create these SRVs pointing to it,
    // then release them after, instead of using srvs to texture copies
    ID3D11ShaderResourceView *releaseDepthSRV = NULL;
    ID3D11ShaderResourceView *releaseStencilSRV = NULL;

    {
      ID3D11DepthStencilView *dsv = NULL;
      m_pImmediateContext->OMGetRenderTargets(0, NULL, &dsv);

      if(dsv)
      {
        depthBound = true;

        dsv->GetResource(&depthRes);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsv->GetDesc(&dsvDesc);

        SAFE_RELEASE(dsv);

        D3D11_RESOURCE_DIMENSION dim;
        depthRes->GetType(&dim);

        D3D11_TEXTURE2D_DESC desc2d;
        RDCEraseEl(desc2d);

        if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        {
          ID3D11Texture1D *tex = (ID3D11Texture1D *)depthRes;
          D3D11_TEXTURE1D_DESC desc1d;
          tex->GetDesc(&desc1d);

          desc2d.Format = desc1d.Format;
          desc2d.Width = desc1d.Width;
          desc2d.Height = 1;
          desc2d.BindFlags = desc1d.BindFlags;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
          ID3D11Texture2D *tex = (ID3D11Texture2D *)depthRes;
          tex->GetDesc(&desc2d);
        }
        else
        {
          RDCERR("Unexpected size of depth buffer");
        }

        bool srvable = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) &&
                       (desc2d.BindFlags & D3D11_BIND_SHADER_RESOURCE) > 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        if(dsvDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
          srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = dsvDesc.Texture2D.MipSlice;

        D3D11_TEXTURE2D_DESC *copyDesc = NULL;
        if(desc2d.Format == DXGI_FORMAT_R16_FLOAT || desc2d.Format == DXGI_FORMAT_R16_SINT ||
           desc2d.Format == DXGI_FORMAT_R16_UINT || desc2d.Format == DXGI_FORMAT_R16_SNORM ||
           desc2d.Format == DXGI_FORMAT_R16_UNORM || desc2d.Format == DXGI_FORMAT_R16_TYPELESS ||
           desc2d.Format == DXGI_FORMAT_D16_UNORM)
        {
          copyDesc = &depthCopyD16Desc;
          copyTex = &depthCopyD16;
          copyDepthSRV = &depthCopyD16_DepthSRV;
          copyStencilSRV = NULL;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R16_UNORM;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R16_UNORM;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_R24G8_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
        {
          copyDesc = &depthCopyD24S8Desc;
          copyTex = &depthCopyD24S8;
          copyDepthSRV = &depthCopyD24S8_DepthSRV;
          copyStencilSRV = &depthCopyD24S8_StencilSRV;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
          copyStencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
            srvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseStencilSRV);
            copyStencilSRV = &releaseStencilSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R32_FLOAT || desc2d.Format == DXGI_FORMAT_R32_SINT ||
                desc2d.Format == DXGI_FORMAT_R32_UINT ||
                desc2d.Format == DXGI_FORMAT_R32_TYPELESS || desc2d.Format == DXGI_FORMAT_D32_FLOAT)
        {
          copyDesc = &depthCopyD32Desc;
          copyTex = &depthCopyD32;
          copyDepthSRV = &depthCopyD32_DepthSRV;
          copyStencilSRV = NULL;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_R32G8X24_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
        {
          copyDesc = &depthCopyD32S8Desc;
          copyTex = &depthCopyD32S8;
          copyDepthSRV = &depthCopyD32S8_DepthSRV;
          copyStencilSRV = &depthCopyD32S8_StencilSRV;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
          copyStencilSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
            srvDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseStencilSRV);
            copyStencilSRV = &releaseStencilSRV;
          }
        }

        if(!srvable &&
           (*copyTex == NULL || desc2d.Width > copyDesc->Width || desc2d.Height > copyDesc->Height))
        {
          // recreate texture
          SAFE_RELEASE(*copyTex);
          SAFE_RELEASE(*copyDepthSRV);
          if(copyStencilSRV)
            SAFE_RELEASE(*copyStencilSRV);

          m_pDevice->CreateTexture2D(copyDesc, NULL, copyTex);
          m_pDevice->CreateShaderResourceView(*copyTex, &copyDepthSRVDesc, copyDepthSRV);
          if(copyStencilSRV)
            m_pDevice->CreateShaderResourceView(*copyTex, &copyStencilSRVDesc, copyStencilSRV);
        }
      }
    }

    PixelHistoryCopyPixel(colourCopyParams, storex * pixstoreStride + 0, storey);

    depthCopyParams.depthbound = depthBound;
    depthCopyParams.sourceTex = (ID3D11Texture2D *)depthRes;
    depthCopyParams.srvTex = copyTex ? *copyTex : NULL;
    depthCopyParams.srv[0] = copyDepthSRV ? *copyDepthSRV : NULL;
    depthCopyParams.srv[1] = copyStencilSRV ? *copyStencilSRV : NULL;

    PixelHistoryCopyPixel(depthCopyParams, storex * pixstoreStride + 0, storey);

    m_pImmediateContext->Begin(occl[ev]);

    // For UAV output we only want to replay once in pristine conditions (only fetching before/after
    // values)
    if(!uavOutput)
      m_WrappedDevice->ReplayLog(0, events[ev].eventID, eReplay_OnlyDraw);

    m_pImmediateContext->End(occl[ev]);

    // determine how many fragments returned from the shader
    if(!uavOutput)
    {
      D3D11_RASTERIZER_DESC rdsc = rsDesc;

      rdsc.ScissorEnable = TRUE;
      // leave depth clip mode as normal
      // leave backface culling mode as normal

      m_pDevice->CreateRasterizerState(&rdsc, &newRS);

      m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
      m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassIncrDepthState, stencilRef);
      m_pImmediateContext->RSSetState(newRS);

      SAFE_RELEASE(newRS);

      ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
      m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

      uint32_t UAVStartSlot = 0;
      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
        if(tmpViews[i] != NULL)
        {
          UAVStartSlot = i + 1;
          SAFE_RELEASE(tmpViews[i]);
        }
      }

      ID3D11RenderTargetView *prevRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
      ID3D11UnorderedAccessView *prevUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
      ID3D11DepthStencilView *prevDSV = NULL;
      const UINT numUAVs =
          m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
      m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
          UAVStartSlot, prevRTVs, &prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs);

      CopyPixelParams params = depthCopyParams;
      params.depthbound = true;
      params.srvTex = params.sourceTex = shaddepthOutput;
      params.srv[0] = shaddepthOutputDepthSRV;
      params.srv[1] = shaddepthOutputStencilSRV;

      m_pImmediateContext->ClearDepthStencilView(shaddepthOutputDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);

      m_pImmediateContext->OMSetRenderTargets(0, NULL, shaddepthOutputDSV);

      // replay first with overlay shader. This is guaranteed to count all fragments
      m_WrappedDevice->ReplayLog(0, events[ev].eventID, eReplay_OnlyDraw);
      PixelHistoryCopyPixel(params, storex * pixstoreStride + 2, storey);

      m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);

      m_pImmediateContext->ClearDepthStencilView(shaddepthOutputDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);

      // now replay with original shader. Some fragments may discard and not be counted
      m_WrappedDevice->ReplayLog(0, events[ev].eventID, eReplay_OnlyDraw);
      PixelHistoryCopyPixel(params, storex * pixstoreStride + 3, storey);

      UINT initCounts[D3D11_1_UAV_SLOT_COUNT];
      memset(&initCounts[0], 0xff, sizeof(initCounts));

      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
          UAVStartSlot, prevRTVs, prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs,
          initCounts);

      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        SAFE_RELEASE(prevRTVs[i]);
      for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
        SAFE_RELEASE(prevUAVs[i]);
      SAFE_RELEASE(prevDSV);
    }
    else
    {
      m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);
    }

    m_pImmediateContext->RSSetState(curRS);
    m_pImmediateContext->RSSetScissorRects(curNumScissors, curScissors);
    m_pImmediateContext->OMSetBlendState(curBS, blendFactor, curSample);
    m_pImmediateContext->OMSetDepthStencilState(curDS, stencilRef);

    for(UINT i = 0; i < curNumInst; i++)
      SAFE_RELEASE(curInst[i]);

    SAFE_RELEASE(curPS);
    SAFE_RELEASE(curRS);
    SAFE_RELEASE(curBS);
    SAFE_RELEASE(curDS);

    // replay only draw to get immediately post-modification values
    m_WrappedDevice->ReplayLog(events[ev].eventID, events[ev].eventID, eReplay_OnlyDraw);

    PixelHistoryCopyPixel(colourCopyParams, storex * pixstoreStride + 1, storey);
    PixelHistoryCopyPixel(depthCopyParams, storex * pixstoreStride + 1, storey);

    SAFE_RELEASE(releaseDepthSRV);
    SAFE_RELEASE(releaseStencilSRV);

    if(ev < events.size() - 1)
      m_WrappedDevice->ReplayLog(events[ev].eventID + 1, events[ev + 1].eventID, eReplay_WithoutDraw);

    SAFE_RELEASE(depthRes);
  }

  ////////////////////////////////////////////////////////////////////////
  // Second loop over each event to determine if it the above query returned
  // true and narrow down which tests (if any) it failed

  for(size_t i = 0; i < occl.size(); i++)
  {
    do
    {
      hr = m_pImmediateContext->GetData(occl[i], &occlData, sizeof(occlData), 0);
    } while(hr == S_FALSE);
    RDCASSERTEQUAL(hr, S_OK);

    D3D11RenderState::ResourceRange resourceRange(targetres, mip, slice);

    const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(events[i].eventID);

    bool clear = bool(draw->flags & DrawFlags::Clear);

    bool uavWrite =
        ((events[i].usage >= ResourceUsage::VS_RWResource &&
          events[i].usage <= ResourceUsage::CS_RWResource) ||
         events[i].usage == ResourceUsage::CopyDst || events[i].usage == ResourceUsage::Copy ||
         events[i].usage == ResourceUsage::Resolve ||
         events[i].usage == ResourceUsage::ResolveDst || events[i].usage == ResourceUsage::GenMips);

    if(events[i].view != ResourceId())
    {
      // if the access is through a view, check the mip/slice matches
      bool used = false;

      ID3D11DeviceChild *view = m_ResourceManager->GetCurrentResource(events[i].view);

      if(WrappedID3D11RenderTargetView1::IsAlloc(view))
      {
        WrappedID3D11RenderTargetView1 *rtv = (WrappedID3D11RenderTargetView1 *)view;

        D3D11RenderState::ResourceRange viewRange(rtv);

        if(viewRange.Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11DepthStencilView::IsAlloc(view))
      {
        WrappedID3D11DepthStencilView *dsv = (WrappedID3D11DepthStencilView *)view;

        D3D11RenderState::ResourceRange viewRange(dsv);

        if(viewRange.Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(view))
      {
        WrappedID3D11ShaderResourceView1 *srv = (WrappedID3D11ShaderResourceView1 *)view;

        D3D11RenderState::ResourceRange viewRange(srv);

        if(viewRange.Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(view))
      {
        WrappedID3D11UnorderedAccessView1 *uav = (WrappedID3D11UnorderedAccessView1 *)view;

        D3D11RenderState::ResourceRange viewRange(uav);

        if(viewRange.Intersects(resourceRange))
          used = true;
      }
      else
      {
        RDCWARN("Unexpected view type, ID %llu. Assuming used...", events[i].view);
        used = true;
      }

      if(!used)
      {
        RDCDEBUG("Usage %d at %u didn't refer to the matching mip/slice (%u/%u)", events[i].usage,
                 events[i].eventID, mip, slice);
        occlData = 0;
        clear = uavWrite = false;
      }
    }

    if(occlData > 0 || clear || uavWrite)
    {
      PixelModification mod;
      RDCEraseEl(mod);

      mod.eventID = events[i].eventID;

      mod.directShaderWrite = uavWrite;
      mod.unboundPS = false;

      mod.preMod.col.value_u[0] = (uint32_t)i;

      if(!(draw->flags & DrawFlags::Clear) && !uavWrite)
      {
        if(flags[i] & TestMustFail_DepthTesting)
          mod.depthTestFailed = true;
        if(flags[i] & TestMustFail_StencilTesting)
          mod.stencilTestFailed = true;
        if(flags[i] & TestMustFail_Scissor)
          mod.scissorClipped = true;
        if(flags[i] & TestMustFail_SampleMask)
          mod.sampleMasked = true;

        m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

        {
          ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
          m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews,
                                                  NULL);

          uint32_t UAVStartSlot = 0;
          for(int v = 0; v < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; v++)
          {
            if(tmpViews[v] != NULL)
            {
              UAVStartSlot = v + 1;
              SAFE_RELEASE(tmpViews[v]);
            }
          }

          ID3D11RenderTargetView *curRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
          ID3D11UnorderedAccessView *curUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
          ID3D11DepthStencilView *curDSV = NULL;
          const UINT numUAVs = m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT
                                                            : D3D11_PS_CS_UAV_REGISTER_COUNT;
          m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
              UAVStartSlot, curRTVs, &curDSV, UAVStartSlot, numUAVs - UAVStartSlot, curUAVs);

          // release these now in case we skip this modification, but don't NULL them
          // so we can still compare
          {
            for(int rtv = 0; rtv < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; rtv++)
              if(curRTVs[rtv])
                curRTVs[rtv]->Release();

            for(int uav = 0; uav < D3D11_1_UAV_SLOT_COUNT; uav++)
              if(curUAVs[uav])
                curUAVs[uav]->Release();

            if(curDSV)
              curDSV->Release();
          }
        }

        curNumScissors = curNumViews = 16;
        m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
        m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);
        m_pImmediateContext->RSGetState(&curRS);
        m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
        blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 1.0f;
        curSample = ~0U;

        D3D11_RASTERIZER_DESC rdesc = {
            /*FillMode =*/D3D11_FILL_SOLID,
            /*CullMode =*/D3D11_CULL_BACK,
            /*FrontCounterClockwise =*/FALSE,
            /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
            /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
            /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            /*DepthClipEnable =*/TRUE,
            /*ScissorEnable =*/FALSE,
            /*MultisampleEnable =*/FALSE,
            /*AntialiasedLineEnable =*/FALSE,
        };
        if(curRS)
          curRS->GetDesc(&rdesc);

        SAFE_RELEASE(curRS);

        D3D11_DEPTH_STENCIL_DESC dsdesc = {
            /*DepthEnable =*/TRUE,
            /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
            /*DepthFunc =*/D3D11_COMPARISON_LESS,
            /*StencilEnable =*/FALSE,
            /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
            /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
            /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                            D3D11_COMPARISON_ALWAYS},
            /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                           D3D11_COMPARISON_ALWAYS},
        };

        if(curDS)
          curDS->GetDesc(&dsdesc);

        SAFE_RELEASE(curDS);

        for(UINT v = 0; v < curNumViews; v++)
        {
          // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

          // if (x,y) pixel isn't in viewport, make empty rect)
          if(xf < curViewports[v].TopLeftX || yf < curViewports[v].TopLeftY ||
             xf >= curViewports[v].TopLeftX + curViewports[v].Width ||
             yf >= curViewports[v].TopLeftY + curViewports[v].Height)
          {
            newScissors[v].left = newScissors[v].top = newScissors[v].bottom =
                newScissors[v].right = 0;
          }
          else
          {
            newScissors[v].left = LONG(x);
            newScissors[v].top = LONG(y);
            newScissors[v].right = newScissors[v].left + 1;
            newScissors[v].bottom = newScissors[v].top + 1;
          }
        }

        // for each test we only disable pipeline rejection tests that fall *after* it.
        // e.g. to get an idea if a pixel failed backface culling or not, we enable only backface
        // culling and disable everything else (since it happens first).
        // For depth testing, we leave all tests enabled up to then - as we only want to know which
        // pixels were rejected by the depth test, not pixels that might have passed the depth test
        // had they not been discarded earlier by backface culling or depth clipping.

        // test shader discard
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[3]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[3]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_BackfaceCulling)
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          rd.DepthClipEnable = FALSE;
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[0]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[0]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_DepthClip)
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[1]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[1]);

          SAFE_RELEASE(newRS);
        }

        // only check scissor if test is enabled and we don't know if it's pass or fail yet
        if((flags[i] & (TestEnabled_Scissor | TestMustPass_Scissor | TestMustFail_Scissor)) ==
           TestEnabled_Scissor)
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          // newScissors has scissor regions calculated to hit our target pixel on every viewport,
          // but we must
          // intersect that with the original scissors regions for correct testing behaviour.
          // This amounts to making any scissor region that doesn't overlap with the target pixel
          // empty.
          //
          // Note that in the case of only one scissor region we can trivially detect pass/fail of
          // the test against
          // our pixel on the CPU so we won't come in here (see check above against
          // MustFail/MustPass). So we will
          // only do this in the case where we have multiple scissor regions/viewports, some
          // intersecting the pixel
          // and some not. So we make the not intersecting scissor regions empty so our occlusion
          // query tests to see
          // if any pixels were written to the "passing" viewports
          D3D11_RECT intersectScissors[16] = {0};
          memcpy(intersectScissors, newScissors, sizeof(intersectScissors));

          for(UINT s = 0; s < curNumScissors; s++)
          {
            if(curScissors[s].left > newScissors[s].left ||
               curScissors[s].right < newScissors[s].right ||
               curScissors[s].top > newScissors[s].top ||
               curScissors[s].bottom < newScissors[s].bottom)
            {
              // scissor region from the log doesn't touch our target pixel, make empty.
              intersectScissors[s].left = intersectScissors[s].right = intersectScissors[s].top =
                  intersectScissors[s].bottom = 0;
            }
          }

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumScissors, intersectScissors);

          m_pImmediateContext->Begin(testQueries[2]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[2]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_DepthTesting)
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          D3D11_DEPTH_STENCIL_DESC dsd = dsdesc;

          // make stencil trivially pass
          dsd.StencilEnable = TRUE;
          dsd.StencilReadMask = 0xff;
          dsd.StencilWriteMask = 0xff;
          dsd.FrontFace.StencilDepthFailOp = dsd.FrontFace.StencilFailOp =
              dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
          dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
          dsd.BackFace.StencilDepthFailOp = dsd.BackFace.StencilFailOp =
              dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
          dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

          m_pDevice->CreateDepthStencilState(&dsd, &newDS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[4]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[4]);

          SAFE_RELEASE(newRS);
          SAFE_RELEASE(newDS);
        }

        if(flags[i] & TestEnabled_StencilTesting)
        {
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          rd.DepthClipEnable = FALSE;
          rd.CullMode = D3D11_CULL_NONE;

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          // leave depthstencil testing exactly as is, because a depth-fail means
          // stencil isn't run
          m_pDevice->CreateDepthStencilState(&dsdesc, &newDS);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[5]);

          m_WrappedDevice->ReplayLog(0, events[i].eventID, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[5]);

          SAFE_RELEASE(newRS);
          SAFE_RELEASE(newDS);
        }

        // we check these in the order defined, as a positive from the backface cull test
        // will invalidate tests later (as they will also be backface culled)

        do
        {
          if(flags[i] & TestEnabled_BackfaceCulling)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[0], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.backfaceCulled = (occlData == 0);

            if(mod.backfaceCulled)
              break;
          }

          if(flags[i] & TestEnabled_DepthClip)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[1], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.depthClipped = (occlData == 0);

            if(mod.depthClipped)
              break;
          }

          if(!mod.backfaceCulled &&
             (flags[i] & (TestEnabled_Scissor | TestMustPass_Scissor | TestMustFail_Scissor)) ==
                 TestEnabled_Scissor)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[2], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.scissorClipped = (occlData == 0);

            if(mod.scissorClipped)
              break;
          }

          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[3], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.shaderDiscarded = (occlData == 0);

            if(mod.shaderDiscarded)
              break;
          }

          if(flags[i] & TestEnabled_DepthTesting)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[4], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.depthTestFailed = (occlData == 0);

            if(mod.depthTestFailed)
              break;
          }

          if(flags[i] & TestEnabled_StencilTesting)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[5], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.stencilTestFailed = (occlData == 0);

            if(mod.stencilTestFailed)
              break;
          }
        } while((void)0, 0);
      }

      history.push_back(mod);

      RDCDEBUG("Event %u is visible, %llu samples visible", events[i].eventID, (UINT64)occlData);
    }

    SAFE_RELEASE(occl[i]);
  }

  m_pImmediateContext->CopyResource(pixstoreReadback, pixstore);
  m_pImmediateContext->CopyResource(pixstoreDepthReadback, pixstoreDepth);

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  m_pImmediateContext->Map(pixstoreReadback, 0, D3D11_MAP_READ, 0, &mapped);

  D3D11_MAPPED_SUBRESOURCE mappedDepth = {0};
  m_pImmediateContext->Map(pixstoreDepthReadback, 0, D3D11_MAP_READ, 0, &mappedDepth);

  byte *pixstoreDepthData = (byte *)mappedDepth.pData;
  byte *pixstoreData = (byte *)mapped.pData;

  ////////////////////////////////////////////////////////////////////////////////////////
  // Third loop over each modification event to read back the pre-draw colour + depth data
  // as well as the # fragments to use in the next step

  ResourceFormat fmt = MakeResourceFormat(GetTypedFormat(details.texFmt));

  for(size_t h = 0; h < history.size(); h++)
  {
    PixelModification &mod = history[h];

    uint32_t pre = mod.preMod.col.value_u[0];

    mod.preMod.col.value_u[0] = 0;

    // figure out where this event lies in the pixstore texture
    uint32_t storex = uint32_t(pre % (2048 / pixstoreStride));
    uint32_t storey = uint32_t(pre / (2048 / pixstoreStride));

    if(!fmt.special && fmt.compCount > 0 && fmt.compByteWidth > 0)
    {
      byte *rowdata = pixstoreData + mapped.RowPitch * storey;

      for(int p = 0; p < 2; p++)
      {
        byte *data = rowdata + fmt.compCount * fmt.compByteWidth * (storex * pixstoreStride + p);

        ModificationValue *val = (p == 0 ? &mod.preMod : &mod.postMod);

        if(fmt.compType == CompType::SInt)
        {
          // need to get correct sign, but otherwise just copy

          if(fmt.compByteWidth == 1)
          {
            int8_t *d = (int8_t *)data;
            for(uint32_t c = 0; c < fmt.compCount; c++)
              val->col.value_i[c] = d[c];
          }
          else if(fmt.compByteWidth == 2)
          {
            int16_t *d = (int16_t *)data;
            for(uint32_t c = 0; c < fmt.compCount; c++)
              val->col.value_i[c] = d[c];
          }
          else if(fmt.compByteWidth == 4)
          {
            int32_t *d = (int32_t *)data;
            for(uint32_t c = 0; c < fmt.compCount; c++)
              val->col.value_i[c] = d[c];
          }
        }
        else
        {
          for(uint32_t c = 0; c < fmt.compCount; c++)
            memcpy(&val->col.value_u[c], data + fmt.compByteWidth * c, fmt.compByteWidth);
        }
      }
    }
    else
    {
      if(fmt.special && (fmt.specialFormat == SpecialFormat::R10G10B10A2 ||
                         fmt.specialFormat == SpecialFormat::R11G11B10))
      {
        byte *rowdata = pixstoreData + mapped.RowPitch * storey;

        for(int p = 0; p < 2; p++)
        {
          byte *data = rowdata + sizeof(uint32_t) * (storex * pixstoreStride + p);

          uint32_t *u = (uint32_t *)data;

          ModificationValue *val = (p == 0 ? &mod.preMod : &mod.postMod);

          Vec4f v;
          if(fmt.specialFormat == SpecialFormat::R10G10B10A2)
            v = ConvertFromR10G10B10A2(*u);
          if(fmt.specialFormat == SpecialFormat::R11G11B10)
          {
            Vec3f v3 = ConvertFromR11G11B10(*u);
            v = Vec4f(v3.x, v3.y, v3.z);
          }

          memcpy(&val->col.value_f[0], &v, sizeof(float) * 4);
        }
      }
      else
      {
        RDCWARN("need to fetch pixel values from special formats");
      }
    }

    {
      byte *rowdata = pixstoreDepthData + mappedDepth.RowPitch * storey;
      float *data = (float *)(rowdata + 2 * sizeof(float) * (storex * pixstoreStride + 0));

      mod.preMod.depth = data[0];
      mod.preMod.stencil = int32_t(data[1]);

      mod.postMod.depth = data[2];
      mod.postMod.stencil = int32_t(data[3]);

      // data[4] unused
      mod.shaderOut.col.value_i[0] =
          int32_t(data[5]);    // fragments writing to the pixel in this event with overlay shader

      // data[6] unused
      mod.shaderOut.col.value_i[1] =
          int32_t(data[7]);    // fragments writing to the pixel in this event with original shader
    }
  }

  m_pImmediateContext->Unmap(pixstoreDepthReadback, 0);
  m_pImmediateContext->Unmap(pixstoreReadback, 0);

  /////////////////////////////////////////////////////////////////////////
  // simple loop to expand out the history events by number of fragments,
  // duplicatinug and setting fragIndex in each

  for(size_t h = 0; h < history.size();)
  {
    int32_t frags = RDCMAX(1, history[h].shaderOut.col.value_i[0]);
    int32_t fragsClipped = RDCCLAMP(history[h].shaderOut.col.value_i[1], 1, frags);

    // if we have fewer fragments with the original shader, some discarded
    // so we need to do a thorough check to see which fragments discarded
    bool someFragsClipped = (fragsClipped < frags);

    PixelModification mod = history[h];

    for(int32_t f = 1; f < frags; f++)
      history.insert(history.begin() + h + 1, mod);

    for(int32_t f = 0; f < frags; f++)
    {
      history[h + f].fragIndex = f;
      history[h + f].primitiveID = someFragsClipped;
    }

    h += frags;
  }

  uint32_t prev = 0;

  /////////////////////////////////////////////////////////////////////////
  // loop for each fragment, for non-final fragments fetch the post-output
  // buffer value, and for each fetch the shader output value

  uint32_t postColSlot = 0;
  uint32_t shadColSlot = 0;
  uint32_t depthSlot = 0;

  uint32_t rtIndex = 100000;
  ID3D11RenderTargetView *RTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};

  ID3D11DepthStencilState *ds = NULL;

  CopyPixelParams shadoutCopyParams = colourCopyParams;
  shadoutCopyParams.sourceTex = shadoutCopyParams.srvTex = shadOutput;
  shadoutCopyParams.srv[0] = shadOutputSRV;
  shadoutCopyParams.uav = shadoutStoreUAV;
  shadoutCopyParams.srcxyCBuf = shadoutsrcxyCBuf;

  depthCopyParams.sourceTex = depthCopyParams.srvTex = shaddepthOutput;
  depthCopyParams.srv[0] = shaddepthOutputDepthSRV;
  depthCopyParams.srv[1] = shaddepthOutputStencilSRV;

  for(size_t h = 0; h < history.size(); h++)
  {
    const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(history[h].eventID);

    if(draw->flags & DrawFlags::Clear)
      continue;

    D3D11MarkerRegion historyData(
        StringFormat::Fmt("Fetching history data for %u: %s", draw->eventID, draw->name.c_str()));

    if(prev != history[h].eventID)
    {
      D3D11MarkerRegion predraw("fetching pre-draw");

      m_WrappedDevice->ReplayLog(0, history[h].eventID, eReplay_WithoutDraw);
      prev = history[h].eventID;

      curNumScissors = curNumViews = 16;
      m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);

      for(UINT v = 0; v < curNumViews; v++)
      {
        // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

        // if (x,y) pixel isn't in viewport, make empty rect)
        if(xf < curViewports[v].TopLeftX || yf < curViewports[v].TopLeftY ||
           xf >= curViewports[v].TopLeftX + curViewports[v].Width ||
           yf >= curViewports[v].TopLeftY + curViewports[v].Height)
        {
          newScissors[v].left = newScissors[v].top = newScissors[v].bottom = newScissors[v].right = 0;
        }
        else
        {
          newScissors[v].left = LONG(x);
          newScissors[v].top = LONG(y);
          newScissors[v].right = newScissors[v].left + 1;
          newScissors[v].bottom = newScissors[v].top + 1;
        }
      }

      m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

      m_pImmediateContext->RSGetState(&curRS);

      D3D11_RASTERIZER_DESC rdesc = {
          /*FillMode =*/D3D11_FILL_SOLID,
          /*CullMode =*/D3D11_CULL_BACK,
          /*FrontCounterClockwise =*/FALSE,
          /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
          /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
          /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
          /*DepthClipEnable =*/TRUE,
          /*ScissorEnable =*/FALSE,
          /*MultisampleEnable =*/FALSE,
          /*AntialiasedLineEnable =*/FALSE,
      };
      if(curRS)
        curRS->GetDesc(&rdesc);

      SAFE_RELEASE(curRS);

      m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);

      // make a depth-stencil state object that writes to depth, uses same comparison
      // as currently set, and tests stencil INCR_SAT / GREATER_EQUAL for fragment selection
      D3D11_DEPTH_STENCIL_DESC dsdesc = {
          /*DepthEnable =*/TRUE,
          /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
          /*DepthFunc =*/D3D11_COMPARISON_LESS,
          /*StencilEnable =*/TRUE,
          /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
          /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
          /*FrontFace =*/{D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT,
                          D3D11_STENCIL_OP_INCR_SAT, D3D11_COMPARISON_GREATER_EQUAL},
          /*BackFace =*/{D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT,
                         D3D11_STENCIL_OP_INCR_SAT, D3D11_COMPARISON_GREATER_EQUAL},
      };
      if(curDS)
      {
        D3D11_DEPTH_STENCIL_DESC stateDesc;
        curDS->GetDesc(&stateDesc);
        dsdesc.DepthFunc = stateDesc.DepthFunc;
      }

      if(history[h].preMod.depth < 0.0f)
        dsdesc.DepthEnable = FALSE;

      SAFE_RELEASE(curDS);

      m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

      D3D11_RASTERIZER_DESC rd = rdesc;

      rd.ScissorEnable = TRUE;
      // leave depth clip mode as normal
      // leave backface culling mode as normal

      m_pDevice->CreateRasterizerState(&rd, &newRS);
      m_pImmediateContext->RSSetState(newRS);
      SAFE_RELEASE(newRS);

      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        SAFE_RELEASE(RTVs[i]);

      m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, RTVs, NULL);

      rtIndex = 100000;

      for(uint32_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
        if(RTVs[i])
        {
          if(rtIndex == 100000)
          {
            ID3D11Resource *res = NULL;
            RTVs[i]->GetResource(&res);

            if(res == targetres)
              rtIndex = i;

            SAFE_RELEASE(res);
          }

          // leave the target RTV in the array
          if(rtIndex != i)
            SAFE_RELEASE(RTVs[i]);
        }
      }

      if(rtIndex == 100000)
      {
        rtIndex = 0;
        RDCWARN("Couldn't find target RT bound at this event");
      }
    }

    float cleardepth = RDCCLAMP(history[h].preMod.depth, 0.0f, 1.0f);

    m_pImmediateContext->ClearDepthStencilView(
        shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

    m_pImmediateContext->OMSetDepthStencilState(ds, history[h].fragIndex);

    // if we're not the last modification in our event, need to fetch post fragment value
    if(h + 1 < history.size() && history[h].eventID == history[h + 1].eventID)
    {
      D3D11MarkerRegion middraw("fetching mid-draw");

      m_pImmediateContext->OMSetRenderTargets(rtIndex + 1, RTVs, shaddepthOutputDSV);

      m_WrappedDevice->ReplayLog(0, history[h].eventID, eReplay_OnlyDraw);

      PixelHistoryCopyPixel(colourCopyParams, postColSlot % 2048, postColSlot / 2048);
      postColSlot++;
    }

    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.StencIncrEqDepthState,
                                                history[h].fragIndex);

    m_pImmediateContext->ClearDepthStencilView(
        shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

    // fetch shader output value & primitive ID
    {
      m_pImmediateContext->OMGetBlendState(&curBS, blendFactor, &curSample);

      m_pImmediateContext->OMSetBlendState(NULL, blendFactor, curSample);

      // fetch shader output value
      {
        D3D11MarkerRegion shadout("fetching shader-out");

        ID3D11RenderTargetView *sparseRTVs[8] = {0};
        sparseRTVs[rtIndex] = shadOutputRTV;
        m_pImmediateContext->OMSetRenderTargets(rtIndex + 1, sparseRTVs, shaddepthOutputDSV);

        m_WrappedDevice->ReplayLog(0, history[h].eventID, eReplay_OnlyDraw);

        PixelHistoryCopyPixel(shadoutCopyParams, shadColSlot % 2048, shadColSlot / 2048);
        shadColSlot++;

        m_pImmediateContext->OMSetRenderTargets(0, NULL, NULL);

        PixelHistoryCopyPixel(depthCopyParams, depthSlot % 2048, depthSlot / 2048);
        depthSlot++;
      }

      m_pImmediateContext->ClearDepthStencilView(
          shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

      // fetch primitive ID
      {
        D3D11MarkerRegion primid("fetching prim ID");

        m_pImmediateContext->OMSetRenderTargets(1, &shadOutputRTV, shaddepthOutputDSV);

        m_pImmediateContext->PSGetShader(&curPS, curInst, &curNumInst);
        m_pImmediateContext->PSSetShader(m_DebugRender.PrimitiveIDPS, NULL, 0);

        if(curPS == NULL)
          history[h].unboundPS = true;

        m_WrappedDevice->ReplayLog(0, history[h].eventID, eReplay_OnlyDraw);

        m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);

        for(UINT i = 0; i < curNumInst; i++)
          SAFE_RELEASE(curInst[i]);

        SAFE_RELEASE(curPS);

        PixelHistoryCopyPixel(shadoutCopyParams, shadColSlot % 2048, shadColSlot / 2048);
        shadColSlot++;
      }

      m_pImmediateContext->OMSetBlendState(curBS, blendFactor, curSample);
      SAFE_RELEASE(curBS);
    }
  }

  SAFE_RELEASE(ds);

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    SAFE_RELEASE(RTVs[i]);

  m_pImmediateContext->CopyResource(shadoutStoreReadback, shadoutStore);
  m_pImmediateContext->CopyResource(pixstoreReadback, pixstore);
  m_pImmediateContext->CopyResource(pixstoreDepthReadback, pixstoreDepth);

  D3D11_MAPPED_SUBRESOURCE mappedShadout = {0};
  m_pImmediateContext->Map(pixstoreReadback, 0, D3D11_MAP_READ, 0, &mapped);
  m_pImmediateContext->Map(pixstoreDepthReadback, 0, D3D11_MAP_READ, 0, &mappedDepth);
  m_pImmediateContext->Map(shadoutStoreReadback, 0, D3D11_MAP_READ, 0, &mappedShadout);

  byte *shadoutStoreData = (byte *)mappedShadout.pData;
  pixstoreData = (byte *)mapped.pData;
  pixstoreDepthData = (byte *)mappedDepth.pData;

  /////////////////////////////////////////////////////////////////////////
  // final loop to fetch the values from above into the modification events

  postColSlot = 0;
  shadColSlot = 0;
  depthSlot = 0;

  prev = 0;

  // this is used to track if any previous fragments in the current draw
  // discarded. If so, the shader output values will be off-by-one in the
  // shader output storage due to stencil counting errors, and we need to
  // offset.
  uint32_t discardedOffset = 0;

  for(size_t h = 0; h < history.size(); h++)
  {
    const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(history[h].eventID);

    if(draw->flags & DrawFlags::Clear)
      continue;

    // if we're not the last modification in our event, need to fetch post fragment value
    if(h + 1 < history.size() && history[h].eventID == history[h + 1].eventID)
    {
      // colour
      {
        if(!fmt.special && fmt.compCount > 0 && fmt.compByteWidth > 0)
        {
          byte *rowdata = pixstoreData + mapped.RowPitch * (postColSlot / 2048);
          byte *data = rowdata + fmt.compCount * fmt.compByteWidth * (postColSlot % 2048);

          if(fmt.compType == CompType::SInt)
          {
            // need to get correct sign, but otherwise just copy

            if(fmt.compByteWidth == 1)
            {
              int8_t *d = (int8_t *)data;
              for(uint32_t c = 0; c < fmt.compCount; c++)
                history[h].postMod.col.value_i[c] = d[c];
            }
            else if(fmt.compByteWidth == 2)
            {
              int16_t *d = (int16_t *)data;
              for(uint32_t c = 0; c < fmt.compCount; c++)
                history[h].postMod.col.value_i[c] = d[c];
            }
            else if(fmt.compByteWidth == 4)
            {
              int32_t *d = (int32_t *)data;
              for(uint32_t c = 0; c < fmt.compCount; c++)
                history[h].postMod.col.value_i[c] = d[c];
            }
          }
          else
          {
            for(uint32_t c = 0; c < fmt.compCount; c++)
              memcpy(&history[h].postMod.col.value_u[c], data + fmt.compByteWidth * c,
                     fmt.compByteWidth);
          }
        }
        else
        {
          if(fmt.special && (fmt.specialFormat == SpecialFormat::R10G10B10A2 ||
                             fmt.specialFormat == SpecialFormat::R11G11B10))
          {
            byte *rowdata = pixstoreData + mapped.RowPitch * (postColSlot / 2048);
            byte *data = rowdata + sizeof(uint32_t) * (postColSlot % 2048);

            uint32_t *u = (uint32_t *)data;

            Vec4f v;
            if(fmt.specialFormat == SpecialFormat::R10G10B10A2)
              v = ConvertFromR10G10B10A2(*u);
            if(fmt.specialFormat == SpecialFormat::R11G11B10)
            {
              Vec3f v3 = ConvertFromR11G11B10(*u);
              v = Vec4f(v3.x, v3.y, v3.z);
            }

            memcpy(&history[h].postMod.col.value_f[0], &v, sizeof(float) * 4);
          }
          else
          {
            RDCWARN("need to fetch pixel values from special formats");
          }
        }
      }

      // we don't retrieve the correct-precision depth value post-fragment. This is only possible
      // for
      // D24 and D32 - D16 doesn't have attached stencil, so we wouldn't be able to get correct
      // depth
      // AND identify each fragment. Instead we just mark this as no data, and the shader output
      // depth
      // should be sufficient.
      if(history[h].preMod.depth >= 0.0f)
        history[h].postMod.depth = -2.0f;
      else
        history[h].postMod.depth = -1.0f;

      // we can't retrieve stencil value after each fragment, as we use stencil to identify the
      // fragment
      if(history[h].preMod.stencil >= 0)
        history[h].postMod.stencil = -2;
      else
        history[h].postMod.stencil = -1;

      // in each case we only mark as "unknown" when the depth/stencil isn't already known to be
      // unbound

      postColSlot++;
    }

    // if we're not the first modification in our event, set our preMod to the previous postMod
    if(h > 0 && history[h].eventID == history[h - 1].eventID)
    {
      history[h].preMod = history[h - 1].postMod;
    }

    // reset discarded offset every event
    if(h > 0 && history[h].eventID != history[h - 1].eventID)
    {
      discardedOffset = 0;
    }

    // fetch shader output value
    {
      // colour
      {
        // shader output is always 4 32bit components, so we can copy straight
        // Note that because shader output values are interleaved with
        // primitive IDs, the discardedOffset is doubled when looking at
        // shader output values
        uint32_t offsettedSlot = (shadColSlot - discardedOffset * 2);
        RDCASSERT(discardedOffset * 2 <= shadColSlot);

        byte *rowdata = shadoutStoreData + mappedShadout.RowPitch * (offsettedSlot / 2048);
        byte *data = rowdata + 4 * sizeof(float) * (offsettedSlot % 2048);

        memcpy(&history[h].shaderOut.col.value_u[0], data, 4 * sizeof(float));
      }

      // depth
      {
        uint32_t offsettedSlot = (depthSlot - discardedOffset);
        RDCASSERT(discardedOffset <= depthSlot);

        byte *rowdata = pixstoreDepthData + mappedDepth.RowPitch * (offsettedSlot / 2048);
        float *data = (float *)(rowdata + 2 * sizeof(float) * (offsettedSlot % 2048));

        history[h].shaderOut.depth = data[0];
        if(history[h].postMod.stencil == -1)
          history[h].shaderOut.stencil = -1;
        else
          history[h].shaderOut.stencil =
              -2;    // can't retrieve this as we use stencil to identify each fragment
      }

      shadColSlot++;
      depthSlot++;
    }

    // fetch primitive ID
    {
      // shader output is always 4 32bit components, so we can copy straight
      byte *rowdata = shadoutStoreData + mappedShadout.RowPitch * (shadColSlot / 2048);
      byte *data = rowdata + 4 * sizeof(float) * (shadColSlot % 2048);

      bool someFragsClipped = history[h].primitiveID != 0;

      memcpy(&history[h].primitiveID, data, sizeof(uint32_t));

      shadColSlot++;

      // if some fragments clipped in this draw, we need to check to see if this
      // primitive ID was one of the ones that clipped.
      // Currently the way we do that is by drawing only that primitive
      // and doing a
      if(someFragsClipped)
      {
        // don't need to worry about trashing state, since at this point we don't need to restore it
        // anymore
        if(prev != history[h].eventID)
        {
          m_WrappedDevice->ReplayLog(0, history[h].eventID, eReplay_WithoutDraw);

          //////////////////////////////////////////////////////////////
          // Set up an identical raster state, but with scissor enabled.
          // This matches the setup when we were originally fetching the
          // number of fragments.
          m_pImmediateContext->RSGetState(&curRS);

          D3D11_RASTERIZER_DESC rsDesc = {
              /*FillMode =*/D3D11_FILL_SOLID,
              /*CullMode =*/D3D11_CULL_BACK,
              /*FrontCounterClockwise =*/FALSE,
              /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
              /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
              /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
              /*DepthClipEnable =*/TRUE,
              /*ScissorEnable =*/FALSE,
              /*MultisampleEnable =*/FALSE,
              /*AntialiasedLineEnable =*/FALSE,
          };

          if(curRS)
            curRS->GetDesc(&rsDesc);

          SAFE_RELEASE(curRS);

          rsDesc.ScissorEnable = TRUE;

          // scissor to our pixel
          newScissors[0].left = LONG(x);
          newScissors[0].top = LONG(y);
          newScissors[0].right = newScissors[0].left + 1;
          newScissors[0].bottom = newScissors[0].top + 1;

          m_pImmediateContext->RSSetScissorRects(1, newScissors);

          m_pDevice->CreateRasterizerState(&rsDesc, &newRS);

          m_pImmediateContext->RSSetState(newRS);

          // other states can just be set to always pass, we already know this primitive ID renders
          m_pImmediateContext->OMSetBlendState(m_DebugRender.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetRenderTargets(0, NULL, shaddepthOutputDSV);
          m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, 0);

          SAFE_RELEASE(newRS);
        }
        prev = history[h].eventID;

        m_pImmediateContext->ClearDepthStencilView(
            shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

        m_pImmediateContext->Begin(testQueries[0]);

        // do draw
        if(draw->flags & DrawFlags::UseIBuffer)
        {
          // TODO once pixel history distinguishes between instances, draw only the instance for
          // this fragment
          m_pImmediateContext->DrawIndexedInstanced(
              RENDERDOC_NumVerticesPerPrimitive(draw->topology), RDCMAX(1U, draw->numInstances),
              draw->indexOffset + RENDERDOC_VertexOffset(draw->topology, history[h].primitiveID),
              draw->baseVertex, draw->instanceOffset);
        }
        else
        {
          m_pImmediateContext->DrawInstanced(
              RENDERDOC_NumVerticesPerPrimitive(draw->topology), RDCMAX(1U, draw->numInstances),
              draw->vertexOffset + RENDERDOC_VertexOffset(draw->topology, history[h].primitiveID),
              draw->instanceOffset);
        }

        m_pImmediateContext->End(testQueries[0]);

        do
        {
          hr = m_pImmediateContext->GetData(testQueries[0], &occlData, sizeof(occlData), 0);
        } while(hr == S_FALSE);
        RDCASSERTEQUAL(hr, S_OK);

        if(occlData == 0)
        {
          history[h].shaderDiscarded = true;
          discardedOffset++;
          RDCEraseEl(history[h].shaderOut);
          history[h].shaderOut.depth = -1.0f;
          history[h].shaderOut.stencil = -1;
        }
      }
    }
  }

  m_pImmediateContext->Unmap(shadoutStoreReadback, 0);
  m_pImmediateContext->Unmap(pixstoreReadback, 0);
  m_pImmediateContext->Unmap(pixstoreDepthReadback, 0);

  // interpret float/unorm values
  if(!fmt.special && fmt.compType != CompType::UInt && fmt.compType != CompType::SInt)
  {
    for(size_t h = 0; h < history.size(); h++)
    {
      PixelModification &mod = history[h];
      if(fmt.compType == CompType::Float && fmt.compByteWidth == 2)
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          mod.preMod.col.value_f[c] = ConvertFromHalf(uint16_t(mod.preMod.col.value_u[c]));
          mod.postMod.col.value_f[c] = ConvertFromHalf(uint16_t(mod.postMod.col.value_u[c]));
        }
      }
      else if(fmt.compType == CompType::UNorm && fmt.compByteWidth == 1 && fmt.srgbCorrected)
      {
        RDCASSERT(fmt.compByteWidth == 1);

        for(uint32_t c = 0; c < RDCMIN(fmt.compCount, 3U); c++)
        {
          mod.preMod.col.value_f[c] = ConvertFromSRGB8(mod.preMod.col.value_u[c] & 0xff);
          mod.postMod.col.value_f[c] = ConvertFromSRGB8(mod.postMod.col.value_u[c] & 0xff);
        }

        // alpha is not SRGB'd
        if(fmt.compCount == 4)
        {
          mod.preMod.col.value_f[3] = float(mod.preMod.col.value_u[3] & 0xff) / 255.0f;
          mod.postMod.col.value_f[3] = float(mod.postMod.col.value_u[3] & 0xff) / 255.0f;
        }
      }
      else if(fmt.compType == CompType::UNorm)
      {
        // only 32bit unorm format is depth, handled separately
        float maxVal = fmt.compByteWidth == 2 ? 65535.0f : 255.0f;

        RDCASSERT(fmt.compByteWidth < 4);

        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          mod.preMod.col.value_f[c] = float(mod.preMod.col.value_u[c]) / maxVal;
          mod.postMod.col.value_f[c] = float(mod.postMod.col.value_u[c]) / maxVal;
        }
      }
      else if(fmt.compType == CompType::SNorm && fmt.compByteWidth == 2)
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          mod.preMod.col.value_f[c] = float(mod.preMod.col.value_u[c]);
          mod.postMod.col.value_f[c] = float(mod.postMod.col.value_u[c]);
        }
      }
      else if(fmt.compType == CompType::SNorm && fmt.compByteWidth == 1)
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          int8_t *d = (int8_t *)&mod.preMod.col.value_u[c];

          if(*d == -128)
            mod.preMod.col.value_f[c] = -1.0f;
          else
            mod.preMod.col.value_f[c] = float(*d) / 127.0f;

          d = (int8_t *)&mod.postMod.col.value_u[c];

          if(*d == -128)
            mod.postMod.col.value_f[c] = -1.0f;
          else
            mod.postMod.col.value_f[c] = float(*d) / 127.0f;
        }
      }
      else if(fmt.compType == CompType::SNorm && fmt.compByteWidth == 2)
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          int16_t *d = (int16_t *)&mod.preMod.col.value_u[c];

          if(*d == -32768)
            mod.preMod.col.value_f[c] = -1.0f;
          else
            mod.preMod.col.value_f[c] = float(*d) / 32767.0f;

          d = (int16_t *)&mod.postMod.col.value_u[c];

          if(*d == -32768)
            mod.postMod.col.value_f[c] = -1.0f;
          else
            mod.postMod.col.value_f[c] = float(*d) / 32767.0f;
        }
      }
    }
  }

#if ENABLED(RDOC_DEVEL)
  for(size_t h = 0; h < history.size(); h++)
  {
    PixelModification &hs = history[h];
    RDCDEBUG(
        "\nHistory %u @ frag %u from prim %u in %u (depth culled %u):\n"
        "pre {%f,%f,%f,%f} {%f,%d}\n"
        "+ shad {%f,%f,%f,%f} {%f,%d}\n"
        "-> post {%f,%f,%f,%f} {%f,%d}",
        uint32_t(h), hs.fragIndex, hs.primitiveID, hs.eventID, hs.depthTestFailed,

        hs.preMod.col.value_f[0], hs.preMod.col.value_f[1], hs.preMod.col.value_f[2],
        hs.preMod.col.value_f[3], hs.preMod.depth, hs.preMod.stencil,

        hs.shaderOut.col.value_f[0], hs.shaderOut.col.value_f[1], hs.shaderOut.col.value_f[2],
        hs.shaderOut.col.value_f[3], hs.shaderOut.depth, hs.shaderOut.stencil,

        hs.postMod.col.value_f[0], hs.postMod.col.value_f[1], hs.postMod.col.value_f[2],
        hs.postMod.col.value_f[3], hs.postMod.depth, hs.postMod.stencil);
  }
#endif

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
    SAFE_RELEASE(testQueries[i]);

  SAFE_RELEASE(pixstore);
  SAFE_RELEASE(shadoutStore);
  SAFE_RELEASE(pixstoreDepth);

  SAFE_RELEASE(pixstoreReadback);
  SAFE_RELEASE(shadoutStoreReadback);
  SAFE_RELEASE(pixstoreDepthReadback);

  SAFE_RELEASE(pixstoreUAV);
  SAFE_RELEASE(shadoutStoreUAV);
  SAFE_RELEASE(pixstoreDepthUAV);

  SAFE_RELEASE(shadOutput);
  SAFE_RELEASE(shadOutputSRV);
  SAFE_RELEASE(shadOutputRTV);
  SAFE_RELEASE(shaddepthOutput);
  SAFE_RELEASE(shaddepthOutputDSV);
  SAFE_RELEASE(shaddepthOutputDepthSRV);
  SAFE_RELEASE(shaddepthOutputStencilSRV);

  SAFE_RELEASE(depthCopyD24S8);
  SAFE_RELEASE(depthCopyD24S8_DepthSRV);
  SAFE_RELEASE(depthCopyD24S8_StencilSRV);

  SAFE_RELEASE(depthCopyD32S8);
  SAFE_RELEASE(depthCopyD32S8_DepthSRV);
  SAFE_RELEASE(depthCopyD32S8_StencilSRV);

  SAFE_RELEASE(depthCopyD32);
  SAFE_RELEASE(depthCopyD32_DepthSRV);

  SAFE_RELEASE(depthCopyD16);
  SAFE_RELEASE(depthCopyD16_DepthSRV);

  SAFE_RELEASE(srcxyCBuf);
  SAFE_RELEASE(shadoutsrcxyCBuf);
  SAFE_RELEASE(storexyCBuf);

  return history;
}
