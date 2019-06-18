/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include <float.h>
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

void GLReplay::RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg)
{
  WrappedOpenGL &drv = *m_pDriver;

  if(cfg.position.vertexResourceId == ResourceId())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  GLMarkerRegion renderMesh(
      StringFormat::Fmt("RenderMesh with %zu secondary draws", secondaryDraws.size()));

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth / DebugData.outHeight);

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f ModelViewProj = projMat.Mul(camMat);
  Matrix4f guessProjInv;

  drv.glBindVertexArray(DebugData.meshVAO);

  const MeshFormat *meshData[2] = {&cfg.position, &cfg.second};

  GLenum topo = MakeGLPrimitiveTopology(cfg.position.topology);

  MeshUBOData uboParams = {};
  MeshUBOData *uboptr = NULL;

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  if(HasExt[EXT_framebuffer_sRGB])
    drv.glEnable(eGL_FRAMEBUFFER_SRGB);

  drv.glDisable(eGL_CULL_FACE);

  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
    {
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
    }

    guessProjInv = guessProj.Inverse();

    ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
  }

  uboParams.mvp = ModelViewProj;
  uboParams.homogenousInput = cfg.position.unproject;
  uboParams.pointSpriteSize = Vec2f(0.0f, 0.0f);

  if(!secondaryDraws.empty())
  {
    uboParams.displayFormat = MESHDISPLAY_SOLID;

    if(!IsGLES)
      drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    // secondary draws have to come from gl_Position which is float4
    drv.glVertexAttribFormat(0, 4, eGL_FLOAT, GL_FALSE, 0);
    drv.glEnableVertexAttribArray(0);
    drv.glDisableVertexAttribArray(1);

    drv.glUseProgram(DebugData.meshProg[0]);

    for(size_t i = 0; i < secondaryDraws.size(); i++)
    {
      const MeshFormat &fmt = secondaryDraws[i];

      if(fmt.vertexResourceId != ResourceId() &&
         m_pDriver->GetResourceManager()->HasCurrentResource(fmt.vertexResourceId))
      {
        uboParams.color = Vec4f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z, fmt.meshColor.w);

        uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.vertexResourceId).name;
        drv.glBindVertexBuffer(0, vb, (GLintptr)fmt.vertexByteOffset, fmt.vertexByteStride);

        {
          GLint bytesize = 0;
          drv.glGetNamedBufferParameterivEXT(vb, eGL_BUFFER_SIZE, &bytesize);

          // skip empty source buffers
          if(bytesize == 0)
            continue;
        }

        GLenum secondarytopo = MakeGLPrimitiveTopology(fmt.topology);

        if(fmt.indexByteStride)
        {
          GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.indexResourceId).name;
          drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

          GLenum idxtype = eGL_UNSIGNED_BYTE;
          if(fmt.indexByteStride == 2)
            idxtype = eGL_UNSIGNED_SHORT;
          else if(fmt.indexByteStride == 4)
            idxtype = eGL_UNSIGNED_INT;

          drv.glDrawElementsBaseVertex(secondarytopo, fmt.numIndices, idxtype,
                                       (const void *)uintptr_t(fmt.indexByteOffset), fmt.baseVertex);
        }
        else
        {
          drv.glDrawArrays(secondarytopo, 0, fmt.numIndices);
        }
      }
    }
  }

  int progidx = 0;
  bool validData[2] = {};

  for(uint32_t i = 0; i < 2; i++)
  {
    if(meshData[i]->vertexResourceId == ResourceId() ||
       !m_pDriver->GetResourceManager()->HasCurrentResource(meshData[i]->vertexResourceId))
      continue;

    if(meshData[i]->format.Special())
    {
      if(meshData[i]->format.type == ResourceFormatType::R10G10B10A2)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          drv.glVertexAttribIFormat(i, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
        if(meshData[i]->format.compType == CompType::SInt)
          drv.glVertexAttribIFormat(i, 4, eGL_INT_2_10_10_10_REV, 0);
      }
      else if(meshData[i]->format.type == ResourceFormatType::R11G11B10)
      {
        drv.glVertexAttribFormat(i, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
      }
      else
      {
        RDCWARN("Unsupported vertex attribute format: %x", meshData[i]->format.type);
      }
    }
    else if(meshData[i]->format.compType == CompType::Float ||
            meshData[i]->format.compType == CompType::UNorm ||
            meshData[i]->format.compType == CompType::SNorm)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(meshData[i]->format.compByteWidth == 4)
      {
        if(meshData[i]->format.compType == CompType::Float)
          fmttype = eGL_FLOAT;
        else if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_INT;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_INT;
      }
      else if(meshData[i]->format.compByteWidth == 2)
      {
        if(meshData[i]->format.compType == CompType::Float)
          fmttype = eGL_HALF_FLOAT;
        else if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_SHORT;
      }
      else if(meshData[i]->format.compByteWidth == 1)
      {
        if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_BYTE;
      }

      drv.glVertexAttribFormat(i, meshData[i]->format.compCount, fmttype,
                               meshData[i]->format.compType != CompType::Float, 0);
    }
    else if(meshData[i]->format.compType == CompType::UInt ||
            meshData[i]->format.compType == CompType::SInt)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(meshData[i]->format.compByteWidth == 4)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_INT;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_INT;
      }
      else if(meshData[i]->format.compByteWidth == 2)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_SHORT;
      }
      else if(meshData[i]->format.compByteWidth == 1)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_BYTE;
      }

      drv.glVertexAttribIFormat(i, meshData[i]->format.compCount, fmttype, 0);
    }
    else if(meshData[i]->format.compType == CompType::Double)
    {
      drv.glVertexAttribLFormat(i, meshData[i]->format.compCount, eGL_DOUBLE, 0);

      progidx |= (1 << i);
    }

    GLintptr offs = (GLintptr)meshData[i]->vertexByteOffset;

    if(meshData[i]->instanced)
      offs += meshData[i]->vertexByteStride * (cfg.curInstance / meshData[i]->instStepRate);

    GLuint vb =
        m_pDriver->GetResourceManager()->GetCurrentResource(meshData[i]->vertexResourceId).name;

    {
      GLint bytesize = 0;
      drv.glGetNamedBufferParameterivEXT(vb, eGL_BUFFER_SIZE, &bytesize);

      // skip empty source buffers
      if(bytesize == 0)
        continue;
    }

    drv.glBindVertexBuffer(i, vb, offs, meshData[i]->vertexByteStride);

    if(meshData[i]->instanced)
      drv.glVertexAttribDivisor(i, 1);
    else
      drv.glVertexAttribDivisor(i, 0);

    validData[i] = true;
  }

  GLuint prog = DebugData.meshProg[progidx];

  if(prog == 0)
  {
    RDCWARN("Couldn't compile right double-compatible mesh display shader");
    prog = DebugData.meshProg[0];
  }

  drv.glUseProgram(prog);

  // enable position attribute
  if(validData[0])
    drv.glEnableVertexAttribArray(0);
  else
    drv.glDisableVertexAttribArray(0);
  drv.glDisableVertexAttribArray(1);

  drv.glEnable(eGL_DEPTH_TEST);

  // solid render
  if(cfg.solidShadeMode != SolidShade::NoSolid && topo != eGL_PATCHES)
  {
    drv.glDepthFunc(eGL_LESS);

    GLuint solidProg = prog;

    if(cfg.solidShadeMode == SolidShade::Lit && DebugData.meshgsProg[0])
    {
      // pick program with GS for per-face lighting
      solidProg = DebugData.meshgsProg[progidx];

      if(solidProg == 0)
      {
        RDCWARN("Couldn't compile right double-compatible mesh display shader");
        solidProg = DebugData.meshgsProg[0];
      }

      ClearGLErrors();
      drv.glUseProgram(solidProg);
      GLenum err = drv.glGetError();

      err = eGL_NONE;
    }

    MeshUBOData *soliddata = (MeshUBOData *)drv.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    soliddata->mvp = ModelViewProj;
    soliddata->pointSpriteSize = Vec2f(0.0f, 0.0f);
    soliddata->homogenousInput = cfg.position.unproject;

    soliddata->color = Vec4f(0.8f, 0.8f, 0.0f, 1.0f);

    uint32_t OutputDisplayFormat = (uint32_t)cfg.solidShadeMode;
    if(cfg.solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
      OutputDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;
    soliddata->displayFormat = OutputDisplayFormat;

    if(cfg.solidShadeMode == SolidShade::Lit)
      soliddata->invProj = projMat.Inverse();

    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    if(validData[1])
      drv.glEnableVertexAttribArray(1);
    else
      drv.glDisableVertexAttribArray(1);

    if(!IsGLES)
      drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(cfg.position.indexByteStride)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.indexByteStride == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.indexByteStride == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.indexResourceId != ResourceId())
      {
        GLuint ib =
            m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
      }
      drv.glDrawElementsBaseVertex(topo, cfg.position.numIndices, idxtype,
                                   (const void *)uintptr_t(cfg.position.indexByteOffset),
                                   cfg.position.baseVertex);
    }
    else
    {
      drv.glDrawArrays(topo, 0, cfg.position.numIndices);
    }

    drv.glDisableVertexAttribArray(1);

    drv.glUseProgram(prog);
  }

  drv.glDepthFunc(eGL_ALWAYS);

  // wireframe render
  if(cfg.solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw || topo == eGL_PATCHES)
  {
    uboParams.color = Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y,
                            cfg.position.meshColor.z, cfg.position.meshColor.w);

    uboParams.displayFormat = MESHDISPLAY_SOLID;

    if(!IsGLES)
      drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    if(cfg.position.indexByteStride)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.indexByteStride == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.indexByteStride == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.indexResourceId != ResourceId() &&
         m_pDriver->GetResourceManager()->HasCurrentResource(cfg.position.indexResourceId))
      {
        GLuint ib =
            m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

        drv.glDrawElementsBaseVertex(
            topo != eGL_PATCHES ? topo : eGL_POINTS, cfg.position.numIndices, idxtype,
            (const void *)uintptr_t(cfg.position.indexByteOffset), cfg.position.baseVertex);
      }
    }
    else
    {
      drv.glDrawArrays(topo != eGL_PATCHES ? topo : eGL_POINTS, 0, cfg.position.numIndices);
    }
  }

  // helpers always use basic float-input program
  drv.glUseProgram(DebugData.meshProg[0]);

  if(cfg.showBBox)
  {
    Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
    Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

    Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
    Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
    Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

    Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
    Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
    Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
    Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
    drv.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(bbox), &bbox[0]);

    drv.glBindVertexArray(DebugData.triHighlightVAO);

    uboParams.color = Vec4f(0.2f, 0.2f, 1.0f, 1.0f);

    Matrix4f mvpMat = projMat.Mul(camMat);

    uboParams.mvp = mvpMat;

    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    // we want this to clip
    drv.glDepthFunc(eGL_LESS);

    drv.glDrawArrays(eGL_LINES, 0, 24);

    drv.glDepthFunc(eGL_ALWAYS);
  }

  // draw axis helpers
  if(!cfg.position.unproject)
  {
    drv.glBindVertexArray(DebugData.axisVAO);

    uboParams.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    drv.glDrawArrays(eGL_LINES, 0, 2);

    uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);
    drv.glDrawArrays(eGL_LINES, 2, 2);

    uboParams.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);
    drv.glDrawArrays(eGL_LINES, 4, 2);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    drv.glBindVertexArray(DebugData.frustumVAO);

    uboParams.color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    uboParams.mvp = ModelViewProj;

    uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    drv.glDrawArrays(eGL_LINES, 0, 24);
  }

  if(!IsGLES)
    drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

  // show highlighted vertex
  if(cfg.highlightVert != ~0U)
  {
    m_HighlightCache.CacheHighlightingData(eventId, cfg);

    GLenum meshtopo = topo;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    std::vector<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    std::vector<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    std::vector<FloatVector> adjacentPrimVertices;

    GLenum primTopo = eGL_TRIANGLES;
    uint32_t primSize = 3;    // number of verts per primitive

    if(meshtopo == eGL_LINES || meshtopo == eGL_LINES_ADJACENCY || meshtopo == eGL_LINE_STRIP ||
       meshtopo == eGL_LINE_STRIP_ADJACENCY)
    {
      primSize = 2;
      primTopo = eGL_LINES;
    }

    bool valid = m_HighlightCache.FetchHighlightPositions(cfg, activeVertex, activePrim,
                                                          adjacentPrimVertices, inactiveVertices);

    if(valid)
    {
      ////////////////////////////////////////////////////////////////
      // prepare rendering (for both vertices & primitives)

      // if data is from post transform, it will be in clipspace
      if(cfg.position.unproject)
        ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
      else
        ModelViewProj = projMat.Mul(camMat);

      uboParams.homogenousInput = cfg.position.unproject;

      uboParams.mvp = ModelViewProj;

      drv.glBindVertexArray(DebugData.triHighlightVAO);

      ////////////////////////////////////////////////////////////////
      // render primitives

      // Draw active primitive (red)
      uboParams.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);

      if(activePrim.size() >= primSize)
      {
        uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
        drv.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f) * primSize, &activePrim[0]);

        drv.glDrawArrays(primTopo, 0, primSize);
      }

      // Draw adjacent primitives (green)
      uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
        drv.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f) * adjacentPrimVertices.size(),
                            &adjacentPrimVertices[0]);

        drv.glDrawArrays(primTopo, 0, (GLsizei)adjacentPrimVertices.size());
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots
      float scale = 800.0f / float(DebugData.outHeight);
      float asp = float(DebugData.outWidth) / float(DebugData.outHeight);

      uboParams.pointSpriteSize = Vec2f(scale / asp, scale);

      // Draw active vertex (blue)
      uboParams.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);

      uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      *uboptr = uboParams;
      drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
      drv.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);

      drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

      // Draw inactive vertices (green)
      uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

      uboptr = (MeshUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      *uboptr = uboParams;
      drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      for(size_t i = 0; i < inactiveVertices.size(); i++)
      {
        vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];

        drv.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);

        drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }
    }
  }
}
