/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"

#include "data/hlsl/debugcbuffers.h"

void D3D11DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                                             const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, const bytebuf &data)
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
      case VARTYPE_MIN12INT:
      case VARTYPE_MIN16INT:
      case VARTYPE_INT: type = VarType::Int; break;
      case VARTYPE_MIN8FLOAT:
      case VARTYPE_MIN10FLOAT:
      case VARTYPE_MIN16FLOAT:
      case VARTYPE_FLOAT: type = VarType::Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8:
      case VARTYPE_MIN16UINT: type = VarType::UInt; break;
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

    if(!outvars[outIdx].name.empty())
    {
      RDCASSERT(flatten);

      RDCASSERT(outvars[vec].rows == 1);
      RDCASSERT(outvars[vec].columns == comp);
      RDCASSERT(rows == 1);

      std::string combinedName = outvars[outIdx].name;
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

        std::vector<ShaderVariable> varmembers;
        std::vector<ShaderVariable> *out = &outvars;
        size_t rowCopy = 1;

        uint32_t registers = rows;
        uint32_t regLen = cols;
        const char *regName = "row";

        std::string base = outvars[outIdx].name;

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
                                             const bytebuf &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

uint32_t D3D11DebugManager::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x,
                                       uint32_t y)
{
  if(cfg.position.numIndices == 0)
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
  cbuf.PickIdx = cfg.position.indexByteStride ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numIndices;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth()) / float(GetHeight()));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f pickMVP = projMat.Mul(camMat);

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
  switch(cfg.position.topology)
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
  DXGI_FORMAT ifmt = cfg.position.indexByteStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  {
    auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.vertexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      vb = it->second.m_Buffer;

    it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.indexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      ib = it->second.m_Buffer;
  }

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers.
  // In the case of VB we also tightly pack and unpack the data. IB can just be
  // read as R16 or R32 via the SRV so it is just a straight copy

  if(cfg.position.indexByteStride)
  {
    // resize up on demand
    if(m_DebugRender.PickIBBuf == NULL ||
       m_DebugRender.PickIBSize < cfg.position.numIndices * cfg.position.indexByteStride)
    {
      SAFE_RELEASE(m_DebugRender.PickIBBuf);
      SAFE_RELEASE(m_DebugRender.PickIBSRV);

      D3D11_BUFFER_DESC desc = {cfg.position.numIndices * cfg.position.indexByteStride,
                                D3D11_USAGE_DEFAULT,
                                D3D11_BIND_SHADER_RESOURCE,
                                0,
                                0,
                                0};

      m_DebugRender.PickIBSize = cfg.position.numIndices * cfg.position.indexByteStride;

      hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickIBBuf);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PickIBBuf HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      sdesc.Format = ifmt;
      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = cfg.position.numIndices;

      hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickIBBuf, &sdesc,
                                               &m_DebugRender.PickIBSRV);

      if(FAILED(hr))
      {
        SAFE_RELEASE(m_DebugRender.PickIBBuf);
        RDCERR("Failed to create PickIBSRV HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }
    }

    // copy index data as-is, the view format will take care of the rest

    RDCASSERT(cfg.position.indexByteOffset < 0xffffffff);

    if(ib)
    {
      D3D11_BUFFER_DESC ibdesc;
      ib->GetDesc(&ibdesc);

      D3D11_BOX box;
      box.front = 0;
      box.back = 1;
      box.left = (uint32_t)cfg.position.indexByteOffset;
      box.right = (uint32_t)cfg.position.indexByteOffset +
                  cfg.position.numIndices * cfg.position.indexByteStride;
      box.top = 0;
      box.bottom = 1;

      box.right = RDCMIN(box.right, ibdesc.ByteWidth - (uint32_t)cfg.position.indexByteOffset);

      m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickIBBuf, 0, 0, 0, 0, ib, 0, &box);
    }
  }

  if(m_DebugRender.PickVBBuf == NULL ||
     m_DebugRender.PickVBSize < cfg.position.numIndices * sizeof(Vec4f))
  {
    SAFE_RELEASE(m_DebugRender.PickVBBuf);
    SAFE_RELEASE(m_DebugRender.PickVBSRV);

    D3D11_BUFFER_DESC desc = {cfg.position.numIndices * sizeof(Vec4f),
                              D3D11_USAGE_DEFAULT,
                              D3D11_BIND_SHADER_RESOURCE,
                              0,
                              0,
                              0};

    m_DebugRender.PickVBSize = cfg.position.numIndices * sizeof(Vec4f);

    hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickVBBuf);

    if(FAILED(hr))
    {
      RDCERR("Failed to create PickVBBuf HRESULT: %s", ToStr(hr).c_str());
      return ~0U;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
    sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    sdesc.Buffer.FirstElement = 0;
    sdesc.Buffer.NumElements = cfg.position.numIndices;

    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickVBBuf, &sdesc,
                                             &m_DebugRender.PickVBSRV);

    if(FAILED(hr))
    {
      SAFE_RELEASE(m_DebugRender.PickVBBuf);
      RDCERR("Failed to create PickVBSRV HRESULT: %s", ToStr(hr).c_str());
      return ~0U;
    }
  }

  // unpack and linearise the data
  if(vb)
  {
    FloatVector *vbData = new FloatVector[cfg.position.numIndices];

    bytebuf oldData;
    GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numIndices; i++)
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
    box.right = cfg.position.numIndices * sizeof(Vec4f);

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

  m_pImmediateContext->Dispatch(cfg.position.numIndices / 1024 + 1, 1, 1);

  m_pImmediateContext->CopyStructureCount(m_DebugRender.histogramBuff, 0,
                                          m_DebugRender.PickResultUAV);

  bytebuf results;
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

  D3D11MarkerRegion marker("PickPixel");

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

    texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
    texDisplay.hdrMultiplier = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.flipY = false;
    texDisplay.mip = mip;
    texDisplay.sampleIdx = sample;
    texDisplay.customShaderId = ResourceId();
    texDisplay.sliceFace = sliceFace;
    texDisplay.rangeMin = 0.0f;
    texDisplay.rangeMax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.resourceId = texture;
    texDisplay.typeHint = typeHint;
    texDisplay.rawOutput = true;
    texDisplay.xOffset = -float(x);
    texDisplay.yOffset = -float(y);

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
    RDCERR("Failed to map stage buff HRESULT: %s", ToStr(hr).c_str());
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

void D3D11DebugManager::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                       const GetTextureDataParams &params, bytebuf &data)
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11Resource *dummyTex = NULL;

  uint32_t subresource = 0;
  uint32_t mips = 0;

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
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.ArraySize = 1;
    }

    subresource = arrayIdx * mips + mip;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture1D *rtTex = NULL;

      hr = m_pDevice->CreateTexture1D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture1D.MipSlice = mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return;
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

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

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
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

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
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture2D *rtTex = NULL;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture2D.MipSlice = mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return;
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

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

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
        RDCERR("Couldn't create target texture to resolve texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
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
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    subresource = mip;

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      subresource = mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture3D *rtTex = NULL;

      hr = m_pDevice->CreateTexture3D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
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
          RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
          SAFE_RELEASE(d);
          SAFE_RELEASE(rtTex);
          return;
        }

        rtv = wrappedrtv;

        m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
        float color[4] = {0.0f, 0.5f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTargetView(rtv, color);

        SetOutputDimensions(desc.Width, desc.Height);
        m_pImmediateContext->RSSetViewports(1, &viewport);

        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = i << mip;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

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
    return;
  }

  MapIntercept intercept;

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  HRESULT hr = m_pImmediateContext->Map(dummyTex, subresource, D3D11_MAP_READ, 0, &mapped);

  if(SUCCEEDED(hr))
  {
    data.resize(bytesize);
    intercept.InitWrappedResource(dummyTex, subresource, data.data());
    intercept.SetD3D(mapped);
    intercept.CopyFromD3D();

    // for 3D textures if we wanted a particular slice (arrayIdx > 0)
    // copy it into the beginning.
    if(intercept.numSlices > 1 && arrayIdx > 0 && (int)arrayIdx < intercept.numSlices)
    {
      byte *dst = data.data();
      byte *src = data.data() + intercept.app.DepthPitch * arrayIdx;

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
    RDCERR("Couldn't map staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
  }

  SAFE_RELEASE(dummyTex);
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
      RDCERR("Failed to create custom shader rtv HRESULT: %s", ToStr(hr).c_str());
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
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = shader;
  disp.resourceId = texid;
  disp.typeHint = typeHint;
  disp.backgroundColor = FloatVector(0, 0, 0, 1.0);
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
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
    RDCERR("Failed to create custom shader tex HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    m_CustomShaderResourceId = GetIDForResource(m_CustomShaderTex);
  }
}
