/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include <algorithm>
#include "common/common.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

void GLReplay::ClearPostVSCache()
{
  WrappedOpenGL &drv = *m_pDriver;

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    drv.glDeleteBuffers(1, &it->second.vsout.buf);
    drv.glDeleteBuffers(1, &it->second.vsout.idxBuf);
    drv.glDeleteBuffers(1, &it->second.gsout.buf);
    drv.glDeleteBuffers(1, &it->second.gsout.idxBuf);
  }

  m_PostVSData.clear();
}

void GLReplay::InitPostVSBuffers(uint32_t eventId)
{
  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  GLMarkerRegion postvs(StringFormat::Fmt("PostVS for %u", eventId));

  MakeCurrentReplayContext(&m_ReplayCtx);

  WrappedOpenGL &drv = *m_pDriver;
  if(drv.m_ActiveFeedback)
  {
    drv.glEndTransformFeedback();
    drv.m_WasActiveFeedback = true;
  }

  GLResourceManager *rm = m_pDriver->GetResourceManager();

  GLRenderState rs;
  rs.FetchState(&drv);
  GLuint elArrayBuffer = 0;
  if(rs.VAO.name)
    drv.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&elArrayBuffer);

  // reflection structures
  ShaderReflection *vsRefl = NULL;
  ShaderReflection *tesRefl = NULL;
  ShaderReflection *gsRefl = NULL;

  // non-program used separable programs of each shader.
  // vsProg we can use on its own as there are no other stages to combine with, but for later stages
  // we need the shaders themselves to re-link into a single program.
  GLuint vsProg = 0;

  // one shader per stage (vs = 0, etc)
  GLuint stageShaders[4] = {};

  // temporary programs created as needed if the original program was created with
  // glCreateShaderProgramv and we don't have a shader to attach
  GLuint tmpShaders[4] = {};

  // these are the 'real' programs with uniform values that we need to copy over to our separable
  // programs. They may be duplicated if there's one program bound to multiple ages
  // one program per stage (vs = 0, etc)
  GLuint stageSrcPrograms[4] = {};

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall->numIndices == 0 || !(drawcall->flags & DrawFlags::Drawcall) ||
     ((drawcall->flags & DrawFlags::Instanced) && drawcall->numInstances == 0))
  {
    // draw is 0 length, nothing to do
    m_PostVSData[eventId] = GLPostVSData();
    return;
  }

  if(rs.Program.name == 0)
  {
    if(rs.Pipeline.name == 0)
    {
      RDCERR("No program or pipeline bound at draw");
      return;
    }
    else
    {
      ResourceId id = rm->GetID(rs.Pipeline);
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      for(int i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          if(i == 0)
          {
            vsRefl = GetShader(pipeDetails.stageShaders[i], ShaderEntryPoint());
            vsProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[i]].prog;
          }
          else if(i == 2)
          {
            tesRefl = GetShader(pipeDetails.stageShaders[2], ShaderEntryPoint());
          }
          else if(i == 3)
          {
            gsRefl = GetShader(pipeDetails.stageShaders[3], ShaderEntryPoint());
          }

          stageShaders[i] = rm->GetCurrentResource(pipeDetails.stageShaders[i]).name;
          stageSrcPrograms[i] = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;

          if(stageShaders[i] == stageSrcPrograms[i])
          {
            const WrappedOpenGL::ProgramData &progDetails =
                m_pDriver->m_Programs[pipeDetails.stagePrograms[i]];

            if(progDetails.shaderProgramUnlinkable)
            {
              const WrappedOpenGL::ShaderData &shadDetails =
                  m_pDriver->m_Shaders[pipeDetails.stageShaders[i]];

              std::vector<const char *> sources;
              sources.reserve(shadDetails.sources.size());

              for(const std::string &s : shadDetails.sources)
                sources.push_back(s.c_str());

              stageShaders[i] = tmpShaders[i] = drv.glCreateShader(ShaderEnum(i));
              drv.glShaderSource(tmpShaders[i], (GLsizei)sources.size(), sources.data(), NULL);
              drv.glCompileShader(tmpShaders[i]);

              GLint status = 0;
              drv.glGetShaderiv(tmpShaders[i], eGL_COMPILE_STATUS, &status);

              if(status == 0)
              {
                char buffer[1024] = {};
                drv.glGetShaderInfoLog(tmpShaders[i], 1024, NULL, buffer);
                RDCERR("Trying to recreate postvs program, couldn't compile shader:\n%s", buffer);
              }
            }
          }
        }
      }
    }
  }
  else
  {
    auto &progDetails = m_pDriver->m_Programs[rm->GetID(rs.Program)];

    for(int i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[0] != ResourceId())
      {
        if(i == 0)
        {
          vsRefl = GetShader(progDetails.stageShaders[0], ShaderEntryPoint());
          vsProg = m_pDriver->m_Shaders[progDetails.stageShaders[0]].prog;
        }
        else if(i == 2 && progDetails.stageShaders[2] != ResourceId())
        {
          tesRefl = GetShader(progDetails.stageShaders[2], ShaderEntryPoint());
        }
        else if(i == 3 && progDetails.stageShaders[3] != ResourceId())
        {
          gsRefl = GetShader(progDetails.stageShaders[3], ShaderEntryPoint());
        }

        stageShaders[i] = rm->GetCurrentResource(progDetails.stageShaders[i]).name;
      }

      stageSrcPrograms[i] = rs.Program.name;
    }
  }

  if(vsRefl == NULL || stageShaders[0] == 0)
  {
    // no vertex shader bound (no vertex processing - compute only program
    // or no program bound, for a clear etc)
    m_PostVSData[eventId] = GLPostVSData();

    // delete any temporaries
    for(size_t i = 0; i < 4; i++)
      if(tmpShaders[i])
        drv.glDeleteShader(tmpShaders[i]);

    return;
  }

  list<string> matrixVaryings;    // matrices need some fixup
  vector<const char *> varyings;

  CopyProgramAttribBindings(stageSrcPrograms[0], vsProg, vsRefl);

  varyings.clear();

  uint32_t stride = 0;
  int32_t posidx = -1;

  for(const SigParameter &sig : vsRefl->outputSignature)
  {
    const char *name = sig.varName.c_str();
    size_t len = sig.varName.size();

    bool include = true;

    // for matrices with names including :row1, :row2 etc we only include :row0
    // as a varying (but increment the stride for all rows to account for the space)
    // and modify the name to remove the :row0 part
    const char *colon = strchr(name, ':');
    if(colon)
    {
      if(name[len - 1] != '0')
      {
        include = false;
      }
      else
      {
        matrixVaryings.push_back(string(name, colon));
        name = matrixVaryings.back().c_str();
      }
    }

    if(include)
      varyings.push_back(name);

    if(sig.systemValue == ShaderBuiltin::Position)
      posidx = int32_t(varyings.size()) - 1;

    if(sig.compType == CompType::Double)
      stride += sizeof(double) * sig.compCount;
    else
      stride += sizeof(float) * sig.compCount;
  }

  // shift position attribute up to first, keeping order otherwise
  // the same
  if(posidx > 0)
  {
    const char *pos = varyings[posidx];
    varyings.erase(varyings.begin() + posidx);
    varyings.insert(varyings.begin(), pos);
  }

  // this is REALLY ugly, but I've seen problems with varying specification, so we try and
  // do some fixup by removing prefixes from the results we got from PROGRAM_OUTPUT.
  //
  // the problem I've seen is:
  //
  // struct vertex
  // {
  //   vec4 Color;
  // };
  //
  // layout(location = 0) out vertex Out;
  //
  // (from g_truc gl-410-primitive-tessellation-2). On AMD the varyings are what you might expect
  // (from
  // the PROGRAM_OUTPUT interface names reflected out): "Out.Color", "gl_Position"
  // however nvidia complains unless you use "Color", "gl_Position". This holds even if you add
  // other
  // variables to the vertex struct.
  //
  // strangely another sample that in-lines the output block like so:
  //
  // out block
  // {
  //   vec2 Texcoord;
  // } Out;
  //
  // uses "block.Texcoord" (reflected name from PROGRAM_OUTPUT and accepted by varyings string on
  // both
  // vendors). This is inconsistent as it's type.member not structname.member as move.
  //
  // The spec is very vague on exactly what these names should be, so I can't say which is correct
  // out of these three possibilities.
  //
  // So our 'fix' is to loop while we have problems linking with the varyings (since we know
  // otherwise
  // linking should succeed, as we only get here with a successfully linked separable program - if
  // it fails
  // to link, it's assigned 0 earlier) and remove any prefixes from variables seen in the link error
  // string.
  // The error string is something like:
  // "error: Varying (named Out.Color) specified but not present in the program object."
  //
  // Yeh. Ugly. Not guaranteed to work at all, but hopefully the common case will just be a single
  // block
  // without any nesting so this might work.
  // At least we don't have to reallocate strings all over, since the memory is
  // already owned elsewhere, we just need to modify pointers to trim prefixes. Bright side?

  GLint status = 0;
  bool finished = false;
  for(;;)
  {
    // specify current varyings & relink
    drv.glTransformFeedbackVaryings(vsProg, (GLsizei)varyings.size(), &varyings[0],
                                    eGL_INTERLEAVED_ATTRIBS);
    drv.glLinkProgram(vsProg);

    drv.glGetProgramiv(vsProg, eGL_LINK_STATUS, &status);

    // all good! Hopefully we'll mostly hit this
    if(status == 1)
      break;

    // if finished is true, this was our last attempt - there are no
    // more fixups possible
    if(finished)
      break;

    char buffer[1025] = {0};
    drv.glGetProgramInfoLog(vsProg, 1024, NULL, buffer);

    // assume we're finished and can't retry any more after this.
    // if we find a potential 'fixup' we'll set this back to false
    finished = true;

    // see if any of our current varyings are present in the buffer string
    for(size_t i = 0; i < varyings.size(); i++)
    {
      if(strstr(buffer, varyings[i]))
      {
        const char *prefix_removed = strchr(varyings[i], '.');

        // does it contain a prefix?
        if(prefix_removed)
        {
          prefix_removed++;    // now this is our string without the prefix

          // first check this won't cause a duplicate - if it does, we have to try something else
          bool duplicate = false;
          for(size_t j = 0; j < varyings.size(); j++)
          {
            if(!strcmp(varyings[j], prefix_removed))
            {
              duplicate = true;
              break;
            }
          }

          if(!duplicate)
          {
            // we'll attempt this fixup
            RDCWARN("Attempting XFB varying fixup, subst '%s' for '%s'", varyings[i], prefix_removed);
            varyings[i] = prefix_removed;
            finished = false;

            // don't try more than one at once (just in case)
            break;
          }
        }
      }
    }
  }

  if(status == 0)
  {
    char buffer[1025] = {0};
    drv.glGetProgramInfoLog(vsProg, 1024, NULL, buffer);
    RDCERR("Failed to fix-up. Link error making xfb vs program: %s", buffer);
    m_PostVSData[eventId] = GLPostVSData();

    // delete any temporaries
    for(size_t i = 0; i < 4; i++)
      if(tmpShaders[i])
        drv.glDeleteShader(tmpShaders[i]);

    return;
  }

  // copy across any uniform values, bindings etc from the real program containing
  // the vertex stage
  CopyProgramUniforms(stageSrcPrograms[0], vsProg);

  // we don't want to do any work, so just discard before rasterizing
  drv.glEnable(eGL_RASTERIZER_DISCARD);

  // bind our program and do the feedback draw
  drv.glUseProgram(vsProg);
  drv.glBindProgramPipeline(0);

  drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);

  GLuint idxBuf = 0;

  if(!(drawcall->flags & DrawFlags::Indexed))
  {
    uint64_t outputSize = uint64_t(drawcall->numIndices) * stride;

    if(drawcall->flags & DrawFlags::Instanced)
      outputSize *= drawcall->numInstances;

    // resize up the buffer if needed for the vertex output data
    if(DebugData.feedbackBufferSize < outputSize)
    {
      uint64_t oldSize = DebugData.feedbackBufferSize;
      DebugData.feedbackBufferSize = CalcMeshOutputSize(DebugData.feedbackBufferSize, outputSize);
      RDCWARN("Resizing xfb buffer from %llu to %llu for output", oldSize,
              DebugData.feedbackBufferSize);
      if(DebugData.feedbackBufferSize > INTPTR_MAX)
      {
        RDCERR("Too much data generated");
        DebugData.feedbackBufferSize = INTPTR_MAX;
      }
      drv.glNamedBufferDataEXT(DebugData.feedbackBuffer, (GLsizeiptr)DebugData.feedbackBufferSize,
                               NULL, eGL_DYNAMIC_READ);
    }

    // need to rebind this here because of an AMD bug that seems to ignore the buffer
    // bindings in the feedback object - or at least it errors if the default feedback
    // object has no buffers bound. Fortunately the state is still object-local so
    // we don't have to restore the buffer binding on the default feedback object.
    drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

    drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
    drv.glBeginTransformFeedback(eGL_POINTS);

    if(drawcall->flags & DrawFlags::Instanced)
    {
      if(HasExt[ARB_base_instance])
      {
        drv.glDrawArraysInstancedBaseInstance(eGL_POINTS, drawcall->vertexOffset,
                                              drawcall->numIndices, drawcall->numInstances,
                                              drawcall->instanceOffset);
      }
      else
      {
        drv.glDrawArraysInstanced(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices,
                                  drawcall->numInstances);
      }
    }
    else
    {
      drv.glDrawArrays(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices);
    }
  }
  else    // drawcall is indexed
  {
    ResourceId idxId = rm->GetID(BufferRes(drv.GetCtx(), elArrayBuffer));

    bytebuf idxdata;
    GetBufferData(idxId, drawcall->indexOffset * drawcall->indexByteWidth,
                  drawcall->numIndices * drawcall->indexByteWidth, idxdata);

    vector<uint32_t> indices;

    uint8_t *idx8 = (uint8_t *)&idxdata[0];
    uint16_t *idx16 = (uint16_t *)&idxdata[0];
    uint32_t *idx32 = (uint32_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    uint32_t numIndices =
        RDCMIN(uint32_t(idxdata.size() / drawcall->indexByteWidth), drawcall->numIndices);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = 0;
      if(drawcall->indexByteWidth == 1)
        i32 = uint32_t(idx8[i]);
      else if(drawcall->indexByteWidth == 2)
        i32 = uint32_t(idx16[i]);
      else if(drawcall->indexByteWidth == 4)
        i32 = idx32[i];

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it, i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(indices.begin(), 0);

    // An index buffer could be something like: 500, 501, 502, 501, 503, 502
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps, like: 500, 501, 502, 510, 511, 512
    // which would become 0, 1, 2, 3, 4, 5 and so the old index buffer would no longer be valid.
    // We just stream-out a tightly packed list of unique indices, and then remap the index buffer
    // so that what did point to 500 points to 0 (accounting for rebasing), and what did point
    // to 510 now points to 3 (accounting for the unique sort).

    // we use a map here since the indices may be sparse. Especially considering if an index
    // is 'invalid' like 0xcccccccc then we don't want an array of 3.4 billion entries.
    map<uint32_t, size_t> indexRemap;
    for(size_t i = 0; i < indices.size(); i++)
    {
      // by definition, this index will only appear once in indices[]
      indexRemap[indices[i]] = i;
    }

    // generate a temporary index buffer with our 'unique index set' indices,
    // so we can transform feedback each referenced vertex once
    GLuint indexSetBuffer = 0;
    drv.glGenBuffers(1, &indexSetBuffer);
    drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, indexSetBuffer);
    drv.glNamedBufferDataEXT(indexSetBuffer, sizeof(uint32_t) * indices.size(), &indices[0],
                             eGL_STATIC_DRAW);

    uint32_t outputSize = (uint32_t)indices.size() * stride;

    if(drawcall->flags & DrawFlags::Instanced)
      outputSize *= drawcall->numInstances;

    // resize up the buffer if needed for the vertex output data
    if(DebugData.feedbackBufferSize < outputSize)
    {
      uint64_t oldSize = DebugData.feedbackBufferSize;
      DebugData.feedbackBufferSize = CalcMeshOutputSize(DebugData.feedbackBufferSize, outputSize);
      RDCWARN("Resizing xfb buffer from %llu to %llu for output", oldSize,
              DebugData.feedbackBufferSize);
      if(DebugData.feedbackBufferSize > INTPTR_MAX)
      {
        RDCERR("Too much data generated");
        DebugData.feedbackBufferSize = INTPTR_MAX;
      }
      drv.glNamedBufferDataEXT(DebugData.feedbackBuffer, (GLsizeiptr)DebugData.feedbackBufferSize,
                               NULL, eGL_DYNAMIC_READ);
    }

    // need to rebind this here because of an AMD bug that seems to ignore the buffer
    // bindings in the feedback object - or at least it errors if the default feedback
    // object has no buffers bound. Fortunately the state is still object-local so
    // we don't have to restore the buffer binding on the default feedback object.
    drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

    drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
    drv.glBeginTransformFeedback(eGL_POINTS);

    if(drawcall->flags & DrawFlags::Instanced)
    {
      if(HasExt[ARB_base_instance])
      {
        drv.glDrawElementsInstancedBaseVertexBaseInstance(
            eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL, drawcall->numInstances,
            drawcall->baseVertex, drawcall->instanceOffset);
      }
      else
      {
        drv.glDrawElementsInstancedBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT,
                                              NULL, drawcall->numInstances, drawcall->baseVertex);
      }
    }
    else
    {
      drv.glDrawElementsBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL,
                                   drawcall->baseVertex);
    }

    // delete the buffer, we don't need it anymore
    drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
    drv.glDeleteBuffers(1, &indexSetBuffer);

    uint32_t stripRestartValue32 = 0;

    if(IsStrip(drawcall->topology) && rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart])
    {
      stripRestartValue32 = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                ? ~0U
                                : rs.PrimitiveRestartIndex;
    }

    // rebase existing index buffer to point from 0 onwards (which will index into our
    // stream-out'd vertex buffer)
    if(drawcall->indexByteWidth == 1)
    {
      uint8_t stripRestartValue = stripRestartValue32 & 0xff;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx8[i] == stripRestartValue)
          continue;

        idx8[i] = uint8_t(indexRemap[idx8[i]]);
      }
    }
    else if(drawcall->indexByteWidth == 2)
    {
      uint16_t stripRestartValue = stripRestartValue32 & 0xffff;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx16[i] == stripRestartValue)
          continue;

        idx16[i] = uint16_t(indexRemap[idx16[i]]);
      }
    }
    else
    {
      uint32_t stripRestartValue = stripRestartValue32;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx32[i] == stripRestartValue)
          continue;

        idx32[i] = uint32_t(indexRemap[idx32[i]]);
      }
    }

    // make the index buffer that can be used to render this postvs data - the original
    // indices, repointed (since we transform feedback to the start of our feedback
    // buffer and only tightly packed unique indices).
    if(!idxdata.empty())
    {
      drv.glGenBuffers(1, &idxBuf);
      drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, idxBuf);
      drv.glNamedBufferDataEXT(idxBuf, (GLsizeiptr)idxdata.size(), &idxdata[0], eGL_STATIC_DRAW);
    }

    // restore previous element array buffer binding
    drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
  }

  drv.glEndTransformFeedback();
  drv.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

  bool error = false;

  // this should be the same as the draw size
  GLuint primsWritten = 0;
  drv.glGetQueryObjectuiv(DebugData.feedbackQueries[0], eGL_QUERY_RESULT, &primsWritten);

  if(primsWritten == 0)
  {
    // we bailed out much earlier if this was a draw of 0 verts
    RDCERR("No primitives written - but we must have had some number of vertices in the draw");
    error = true;
  }

  // get buffer data from buffer attached to feedback object
  float *data = (float *)drv.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

  if(data == NULL)
  {
    drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
    RDCERR("Couldn't map feedback buffer!");
    error = true;
  }

  if(error)
  {
    // restore replay state we trashed
    drv.glUseProgram(rs.Program.name);
    drv.glBindProgramPipeline(rs.Pipeline.name);

    drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
    drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

    drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

    if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
      drv.glDisable(eGL_RASTERIZER_DISCARD);
    else
      drv.glEnable(eGL_RASTERIZER_DISCARD);

    m_PostVSData[eventId] = GLPostVSData();

    // delete any temporaries
    for(size_t i = 0; i < 4; i++)
      if(tmpShaders[i])
        drv.glDeleteShader(tmpShaders[i]);

    return;
  }

  // create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
  // can render from it to display previews).
  GLuint vsoutBuffer = 0;
  drv.glGenBuffers(1, &vsoutBuffer);
  drv.glBindBuffer(eGL_ARRAY_BUFFER, vsoutBuffer);
  drv.glNamedBufferDataEXT(vsoutBuffer, stride * primsWritten, data, eGL_STATIC_DRAW);

  byte *byteData = (byte *)data;

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  for(GLuint i = 1; posidx != -1 && i < primsWritten; i++)
  {
    //////////////////////////////////////////////////////////////////////////////////
    // derive near/far, assuming a standard perspective matrix
    //
    // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
    // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
    // and we know Wpost = Zpre from the perspective matrix.
    // we can then see from the perspective matrix that
    // m = F/(F-N)
    // c = -(F*N)/(F-N)
    //
    // with re-arranging and substitution, we then get:
    // N = -c/m
    // F = c/(1-m)
    //
    // so if we can derive m and c then we can determine N and F. We can do this with
    // two points, and we pick them reasonably distinct on z to reduce floating-point
    // error

    Vec4f *pos = (Vec4f *)(byteData + i * stride);

    if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
    {
      Vec2f A(pos0->w, pos0->z);
      Vec2f B(pos->w, pos->z);

      float m = (B.y - A.y) / (B.x - A.x);
      float c = B.y - B.x * m;

      if(m == 1.0f)
        continue;

      nearp = -c / m;
      farp = c / (1 - m);

      found = true;

      break;
    }
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
  {
    nearp = pos0->z;
    farp = FLT_MAX;
  }

  drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

  // store everything out to the PostVS data cache
  m_PostVSData[eventId].vsin.topo = drawcall->topology;
  m_PostVSData[eventId].vsout.buf = vsoutBuffer;
  m_PostVSData[eventId].vsout.vertStride = stride;
  m_PostVSData[eventId].vsout.nearPlane = nearp;
  m_PostVSData[eventId].vsout.farPlane = farp;

  m_PostVSData[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::Indexed);
  m_PostVSData[eventId].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventId].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventId].vsout.instStride =
        (stride * primsWritten) / RDCMAX(1U, drawcall->numInstances);

  m_PostVSData[eventId].vsout.idxBuf = 0;
  m_PostVSData[eventId].vsout.idxByteWidth = drawcall->indexByteWidth;
  if(m_PostVSData[eventId].vsout.useIndices && idxBuf)
  {
    m_PostVSData[eventId].vsout.idxBuf = idxBuf;
  }

  m_PostVSData[eventId].vsout.hasPosOut = posidx >= 0;

  m_PostVSData[eventId].vsout.topo = drawcall->topology;

  // set vsProg back to no varyings, for future use
  drv.glTransformFeedbackVaryings(vsProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
  drv.glLinkProgram(vsProg);

  GLuint lastFeedbackProg = 0;

  if(tesRefl || gsRefl)
  {
    ShaderReflection *lastRefl = gsRefl;

    if(!lastRefl)
      lastRefl = tesRefl;

    RDCASSERT(lastRefl);

    lastFeedbackProg = drv.glCreateProgram();

    // attach the shaders
    for(int i = 0; i < 4; i++)
      if(stageShaders[i])
        drv.glAttachShader(lastFeedbackProg, stageShaders[i]);

    varyings.clear();

    stride = 0;
    posidx = -1;

    for(const SigParameter &sig : lastRefl->outputSignature)
    {
      const char *name = sig.varName.c_str();
      size_t len = sig.varName.size();

      bool include = true;

      // for matrices with names including :row1, :row2 etc we only include :row0
      // as a varying (but increment the stride for all rows to account for the space)
      // and modify the name to remove the :row0 part
      const char *colon = strchr(name, ':');
      if(colon)
      {
        if(name[len - 1] != '0')
        {
          include = false;
        }
        else
        {
          matrixVaryings.push_back(std::string(name, colon));
          name = matrixVaryings.back().c_str();
        }
      }

      if(include)
        varyings.push_back(name);

      if(sig.systemValue == ShaderBuiltin::Position)
        posidx = int32_t(varyings.size()) - 1;

      stride += sizeof(float) * sig.compCount;
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      const char *pos = varyings[posidx];
      varyings.erase(varyings.begin() + posidx);
      varyings.insert(varyings.begin(), pos);
    }

    // see above for the justification/explanation of this monstrosity.

    status = 0;
    finished = false;
    for(;;)
    {
      // specify current varyings & relink
      drv.glTransformFeedbackVaryings(lastFeedbackProg, (GLsizei)varyings.size(), &varyings[0],
                                      eGL_INTERLEAVED_ATTRIBS);
      drv.glLinkProgram(lastFeedbackProg);

      drv.glGetProgramiv(lastFeedbackProg, eGL_LINK_STATUS, &status);

      // all good! Hopefully we'll mostly hit this
      if(status == 1)
        break;

      // if finished is true, this was our last attempt - there are no
      // more fixups possible
      if(finished)
        break;

      char buffer[1025] = {0};
      drv.glGetProgramInfoLog(lastFeedbackProg, 1024, NULL, buffer);

      // assume we're finished and can't retry any more after this.
      // if we find a potential 'fixup' we'll set this back to false
      finished = true;

      // see if any of our current varyings are present in the buffer string
      for(size_t i = 0; i < varyings.size(); i++)
      {
        if(strstr(buffer, varyings[i]))
        {
          const char *prefix_removed = strchr(varyings[i], '.');

          // does it contain a prefix?
          if(prefix_removed)
          {
            prefix_removed++;    // now this is our string without the prefix

            // first check this won't cause a duplicate - if it does, we have to try something else
            bool duplicate = false;
            for(size_t j = 0; j < varyings.size(); j++)
            {
              if(!strcmp(varyings[j], prefix_removed))
              {
                duplicate = true;
                break;
              }
            }

            if(!duplicate)
            {
              // we'll attempt this fixup
              RDCWARN("Attempting XFB varying fixup, subst '%s' for '%s'", varyings[i],
                      prefix_removed);
              varyings[i] = prefix_removed;
              finished = false;

              // don't try more than one at once (just in case)
              break;
            }
          }
        }
      }
    }

    // detach the shaders now that linking is complete
    for(int i = 0; i < 4; i++)
      if(stageShaders[i])
        drv.glDetachShader(lastFeedbackProg, stageShaders[i]);

    if(status == 0)
    {
      char buffer[1025] = {0};
      drv.glGetProgramInfoLog(lastFeedbackProg, 1024, NULL, buffer);
      RDCERR("Failed to fix-up. Link error making xfb last program: %s", buffer);
    }
    else
    {
      // copy across any uniform values, bindings etc from the real program containing
      // the vertex stage
      CopyProgramUniforms(stageSrcPrograms[0], lastFeedbackProg);

      // if tessellation is enabled, bind & copy uniforms. Note, control shader is optional
      // independent of eval shader (default values are used for the tessellation levels).
      if(stageSrcPrograms[1])
        CopyProgramUniforms(stageSrcPrograms[1], lastFeedbackProg);
      if(stageSrcPrograms[2])
        CopyProgramUniforms(stageSrcPrograms[2], lastFeedbackProg);

      // if we have a geometry shader, bind & copy uniforms
      if(stageSrcPrograms[3])
        CopyProgramUniforms(stageSrcPrograms[3], lastFeedbackProg);

      // bind our program and do the feedback draw
      drv.glUseProgram(lastFeedbackProg);
      drv.glBindProgramPipeline(0);

      drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);

      // need to rebind this here because of an AMD bug that seems to ignore the buffer
      // bindings in the feedback object - or at least it errors if the default feedback
      // object has no buffers bound. Fortunately the state is still object-local so
      // we don't have to restore the buffer binding on the default feedback object.
      drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

      idxBuf = 0;

      GLenum shaderOutMode = eGL_TRIANGLES;
      GLenum lastOutTopo = eGL_TRIANGLES;

      uint32_t maxOutputSize = stride;

      if(drawcall->flags & DrawFlags::Instanced)
        maxOutputSize *= drawcall->numInstances;

      uint32_t numInputPrimitives = drawcall->numIndices;
      GLenum drawtopo = MakeGLPrimitiveTopology(drawcall->topology);

      switch(drawcall->topology)
      {
        case Topology::Unknown:
        case Topology::PointList: break;
        case Topology::LineList: numInputPrimitives /= 2; break;
        case Topology::LineStrip: numInputPrimitives -= 1; break;
        case Topology::LineLoop: break;
        case Topology::TriangleList: numInputPrimitives /= 3; break;
        case Topology::TriangleStrip:
        case Topology::TriangleFan: numInputPrimitives -= 2; break;
        case Topology::LineList_Adj: numInputPrimitives /= 4; break;
        case Topology::LineStrip_Adj: numInputPrimitives -= 3; break;
        case Topology::TriangleList_Adj: numInputPrimitives /= 6; break;
        case Topology::TriangleStrip_Adj: numInputPrimitives -= 5; break;
        case Topology::PatchList_1CPs:
        case Topology::PatchList_2CPs:
        case Topology::PatchList_3CPs:
        case Topology::PatchList_4CPs:
        case Topology::PatchList_5CPs:
        case Topology::PatchList_6CPs:
        case Topology::PatchList_7CPs:
        case Topology::PatchList_8CPs:
        case Topology::PatchList_9CPs:
        case Topology::PatchList_10CPs:
        case Topology::PatchList_11CPs:
        case Topology::PatchList_12CPs:
        case Topology::PatchList_13CPs:
        case Topology::PatchList_14CPs:
        case Topology::PatchList_15CPs:
        case Topology::PatchList_16CPs:
        case Topology::PatchList_17CPs:
        case Topology::PatchList_18CPs:
        case Topology::PatchList_19CPs:
        case Topology::PatchList_20CPs:
        case Topology::PatchList_21CPs:
        case Topology::PatchList_22CPs:
        case Topology::PatchList_23CPs:
        case Topology::PatchList_24CPs:
        case Topology::PatchList_25CPs:
        case Topology::PatchList_26CPs:
        case Topology::PatchList_27CPs:
        case Topology::PatchList_28CPs:
        case Topology::PatchList_29CPs:
        case Topology::PatchList_30CPs:
        case Topology::PatchList_31CPs:
        case Topology::PatchList_32CPs:
          numInputPrimitives /= PatchList_Count(drawcall->topology);
          break;
      }

      if(lastRefl == gsRefl)
      {
        drv.glGetProgramiv(lastFeedbackProg, eGL_GEOMETRY_OUTPUT_TYPE, (GLint *)&shaderOutMode);

        GLint maxVerts = 1;

        drv.glGetProgramiv(lastFeedbackProg, eGL_GEOMETRY_VERTICES_OUT, (GLint *)&maxVerts);

        if(shaderOutMode == eGL_TRIANGLE_STRIP)
        {
          lastOutTopo = eGL_TRIANGLES;
          maxVerts = RDCMAX(3, maxVerts);
        }
        else if(shaderOutMode == eGL_LINE_STRIP)
        {
          lastOutTopo = eGL_LINES;
          maxVerts = RDCMAX(2, maxVerts);
        }
        else if(shaderOutMode == eGL_POINTS)
        {
          lastOutTopo = eGL_POINTS;
          maxVerts = RDCMAX(1, maxVerts);
        }

        maxOutputSize *= maxVerts * numInputPrimitives;
      }
      else if(lastRefl == tesRefl)
      {
        drv.glGetProgramiv(lastFeedbackProg, eGL_TESS_GEN_MODE, (GLint *)&shaderOutMode);

        uint32_t outputPrimitiveVerts = 1;

        if(shaderOutMode == eGL_QUADS)
        {
          lastOutTopo = eGL_TRIANGLES;
          outputPrimitiveVerts = 3;
        }
        else if(shaderOutMode == eGL_ISOLINES)
        {
          lastOutTopo = eGL_LINES;
          outputPrimitiveVerts = 2;
        }
        else if(shaderOutMode == eGL_TRIANGLES)
        {
          lastOutTopo = eGL_TRIANGLES;
          outputPrimitiveVerts = 3;
        }

        // assume an average maximum tessellation level of 32
        maxOutputSize *= 32 * outputPrimitiveVerts * numInputPrimitives;
      }

      // resize up the buffer if needed for the vertex output data
      if(DebugData.feedbackBufferSize < maxOutputSize)
      {
        uint64_t oldSize = DebugData.feedbackBufferSize;
        DebugData.feedbackBufferSize =
            CalcMeshOutputSize(DebugData.feedbackBufferSize, maxOutputSize);
        RDCWARN("Conservatively resizing xfb buffer from %llu to %llu for output", oldSize,
                DebugData.feedbackBufferSize);
        if(DebugData.feedbackBufferSize > INTPTR_MAX)
        {
          RDCERR("Too much data generated");
          DebugData.feedbackBufferSize = INTPTR_MAX;
        }
        drv.glNamedBufferDataEXT(DebugData.feedbackBuffer, (GLsizeiptr)DebugData.feedbackBufferSize,
                                 NULL, eGL_DYNAMIC_READ);
      }

      GLenum idxType = eGL_UNSIGNED_BYTE;
      if(drawcall->indexByteWidth == 2)
        idxType = eGL_UNSIGNED_SHORT;
      else if(drawcall->indexByteWidth == 4)
        idxType = eGL_UNSIGNED_INT;

      // instanced draws must be replayed one at a time so we can record the number of primitives
      // from
      // each drawcall, as due to expansion this can vary per-instance.
      if(drawcall->flags & DrawFlags::Instanced)
      {
        // if there is only one instance it's a trivial case and we don't need to bother with the
        // expensive path
        if(drawcall->numInstances > 1)
        {
          // ensure we have enough queries
          uint32_t curSize = (uint32_t)DebugData.feedbackQueries.size();
          if(curSize < drawcall->numInstances)
          {
            DebugData.feedbackQueries.resize(drawcall->numInstances);
            drv.glGenQueries(drawcall->numInstances - curSize,
                             DebugData.feedbackQueries.data() + curSize);
          }

          // do incremental draws to get the output size. We have to do this O(N^2) style because
          // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
          // instances and count the total number of verts each time, then we can see from the
          // difference how much each instance wrote.
          for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
          {
            drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
            drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
                             DebugData.feedbackQueries[inst - 1]);
            drv.glBeginTransformFeedback(lastOutTopo);

            if(!(drawcall->flags & DrawFlags::Indexed))
            {
              if(HasExt[ARB_base_instance])
              {
                drv.glDrawArraysInstancedBaseInstance(drawtopo, drawcall->vertexOffset,
                                                      drawcall->numIndices, inst,
                                                      drawcall->instanceOffset);
              }
              else
              {
                drv.glDrawArraysInstanced(drawtopo, drawcall->vertexOffset, drawcall->numIndices,
                                          inst);
              }
            }
            else
            {
              if(HasExt[ARB_base_instance])
              {
                drv.glDrawElementsInstancedBaseVertexBaseInstance(
                    drawtopo, drawcall->numIndices, idxType,
                    (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth), inst,
                    drawcall->baseVertex, drawcall->instanceOffset);
              }
              else
              {
                drv.glDrawElementsInstancedBaseVertex(
                    drawtopo, drawcall->numIndices, idxType,
                    (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth), inst,
                    drawcall->baseVertex);
              }
            }

            drv.glEndTransformFeedback();
            drv.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
          }
        }
        else
        {
          drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
          drv.glBeginTransformFeedback(lastOutTopo);

          if(!(drawcall->flags & DrawFlags::Indexed))
          {
            if(HasExt[ARB_base_instance])
            {
              drv.glDrawArraysInstancedBaseInstance(drawtopo, drawcall->vertexOffset,
                                                    drawcall->numIndices, drawcall->numInstances,
                                                    drawcall->instanceOffset);
            }
            else
            {
              drv.glDrawArraysInstanced(drawtopo, drawcall->vertexOffset, drawcall->numIndices,
                                        drawcall->numInstances);
            }
          }
          else
          {
            if(HasExt[ARB_base_instance])
            {
              drv.glDrawElementsInstancedBaseVertexBaseInstance(
                  drawtopo, drawcall->numIndices, idxType,
                  (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
                  drawcall->numInstances, drawcall->baseVertex, drawcall->instanceOffset);
            }
            else
            {
              drv.glDrawElementsInstancedBaseVertex(
                  drawtopo, drawcall->numIndices, idxType,
                  (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
                  drawcall->numInstances, drawcall->baseVertex);
            }
          }

          drv.glEndTransformFeedback();
          drv.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        }
      }
      else
      {
        drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
        drv.glBeginTransformFeedback(lastOutTopo);

        if(!(drawcall->flags & DrawFlags::Indexed))
        {
          drv.glDrawArrays(drawtopo, drawcall->vertexOffset, drawcall->numIndices);
        }
        else
        {
          drv.glDrawElementsBaseVertex(
              drawtopo, drawcall->numIndices, idxType,
              (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
              drawcall->baseVertex);
        }

        drv.glEndTransformFeedback();
        drv.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
      }

      std::vector<GLPostVSData::InstData> instData;

      if((drawcall->flags & DrawFlags::Instanced) && drawcall->numInstances > 1)
      {
        uint64_t prevVertCount = 0;

        for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
        {
          drv.glGetQueryObjectuiv(DebugData.feedbackQueries[inst], eGL_QUERY_RESULT, &primsWritten);

          uint32_t vertCount = 3 * primsWritten;

          GLPostVSData::InstData d;
          d.numVerts = uint32_t(vertCount - prevVertCount);
          d.bufOffset = uint32_t(stride * prevVertCount);
          prevVertCount = vertCount;

          instData.push_back(d);
        }
      }
      else
      {
        primsWritten = 0;
        drv.glGetQueryObjectuiv(DebugData.feedbackQueries[0], eGL_QUERY_RESULT, &primsWritten);
      }

      error = false;

      if(primsWritten == 0)
      {
        RDCWARN("No primitives written by last vertex processing stage");
        error = true;
      }

      // get buffer data from buffer attached to feedback object
      data = (float *)drv.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

      if(data == NULL)
      {
        drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
        RDCERR("Couldn't map feedback buffer!");
        error = true;
      }

      if(error)
      {
        // delete temporary program we made
        if(lastFeedbackProg)
          drv.glDeleteProgram(lastFeedbackProg);

        // restore replay state we trashed
        drv.glUseProgram(rs.Program.name);
        drv.glBindProgramPipeline(rs.Pipeline.name);

        drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

        drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

        if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
          drv.glDisable(eGL_RASTERIZER_DISCARD);
        else
          drv.glEnable(eGL_RASTERIZER_DISCARD);

        // delete any temporaries
        for(size_t i = 0; i < 4; i++)
          if(tmpShaders[i])
            drv.glDeleteShader(tmpShaders[i]);

        return;
      }

      if(lastRefl == tesRefl)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_TRIANGLES ||
           shaderOutMode == eGL_QUADS)    // query for quads returns # triangles
          m_PostVSData[eventId].gsout.numVerts = primsWritten * 3;
        else if(shaderOutMode == eGL_ISOLINES)
          m_PostVSData[eventId].gsout.numVerts = primsWritten * 2;
      }
      else if(lastRefl == gsRefl)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_POINTS)
          m_PostVSData[eventId].gsout.numVerts = primsWritten;
        else if(shaderOutMode == eGL_LINE_STRIP)
          m_PostVSData[eventId].gsout.numVerts = primsWritten * 2;
        else if(shaderOutMode == eGL_TRIANGLE_STRIP)
          m_PostVSData[eventId].gsout.numVerts = primsWritten * 3;
      }

      // create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
      // can render from it to display previews).
      GLuint lastoutBuffer = 0;
      drv.glGenBuffers(1, &lastoutBuffer);
      drv.glBindBuffer(eGL_ARRAY_BUFFER, lastoutBuffer);
      drv.glNamedBufferDataEXT(lastoutBuffer, stride * m_PostVSData[eventId].gsout.numVerts, data,
                               eGL_STATIC_DRAW);

      byteData = (byte *)data;

      nearp = 0.1f;
      farp = 100.0f;

      pos0 = (Vec4f *)byteData;

      found = false;

      for(uint32_t i = 1; posidx != -1 && i < m_PostVSData[eventId].gsout.numVerts; i++)
      {
        //////////////////////////////////////////////////////////////////////////////////
        // derive near/far, assuming a standard perspective matrix
        //
        // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
        // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
        // and we know Wpost = Zpre from the perspective matrix.
        // we can then see from the perspective matrix that
        // m = F/(F-N)
        // c = -(F*N)/(F-N)
        //
        // with re-arranging and substitution, we then get:
        // N = -c/m
        // F = c/(1-m)
        //
        // so if we can derive m and c then we can determine N and F. We can do this with
        // two points, and we pick them reasonably distinct on z to reduce floating-point
        // error

        Vec4f *pos = (Vec4f *)(byteData + i * stride);

        if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
        {
          Vec2f A(pos0->w, pos0->z);
          Vec2f B(pos->w, pos->z);

          float m = (B.y - A.y) / (B.x - A.x);
          float c = B.y - B.x * m;

          if(m == 1.0f)
            continue;

          nearp = -c / m;
          farp = c / (1 - m);

          found = true;

          break;
        }
      }

      // if we didn't find anything, all z's and w's were identical.
      // If the z is positive and w greater for the first element then
      // we detect this projection as reversed z with infinite far plane
      if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
      {
        nearp = pos0->z;
        farp = FLT_MAX;
      }

      drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

      // store everything out to the PostVS data cache
      m_PostVSData[eventId].gsout.buf = lastoutBuffer;
      m_PostVSData[eventId].gsout.instStride = 0;
      if(drawcall->flags & DrawFlags::Instanced)
      {
        m_PostVSData[eventId].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);
        m_PostVSData[eventId].gsout.instStride = stride * m_PostVSData[eventId].gsout.numVerts;
      }
      m_PostVSData[eventId].gsout.vertStride = stride;
      m_PostVSData[eventId].gsout.nearPlane = nearp;
      m_PostVSData[eventId].gsout.farPlane = farp;

      m_PostVSData[eventId].gsout.useIndices = false;

      m_PostVSData[eventId].gsout.hasPosOut = posidx >= 0;

      m_PostVSData[eventId].gsout.idxBuf = 0;
      m_PostVSData[eventId].gsout.idxByteWidth = 0;

      m_PostVSData[eventId].gsout.topo = MakePrimitiveTopology(lastOutTopo);

      m_PostVSData[eventId].gsout.instData = instData;
    }
  }

  // delete temporary pipelines we made
  if(lastFeedbackProg)
    drv.glDeleteProgram(lastFeedbackProg);

  // restore replay state we trashed
  drv.glUseProgram(rs.Program.name);
  drv.glBindProgramPipeline(rs.Pipeline.name);

  drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
  drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

  drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

  if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
    drv.glDisable(eGL_RASTERIZER_DISCARD);
  else
    drv.glEnable(eGL_RASTERIZER_DISCARD);

  // delete any temporaries
  for(size_t i = 0; i < 4; i++)
    if(tmpShaders[i])
      drv.glDeleteShader(tmpShaders[i]);
}

void GLReplay::InitPostVSBuffers(const vector<uint32_t> &passEvents)
{
  uint32_t prev = 0;

  // since we can always replay between drawcalls, just loop through all the events
  // doing partial replays and calling InitPostVSBuffers for each
  for(size_t i = 0; i < passEvents.size(); i++)
  {
    if(prev != passEvents[i])
    {
      m_pDriver->ReplayLog(prev, prev, eReplay_OnlyDraw);
      m_pDriver->ReplayLog(prev + 1, passEvents[i], eReplay_WithoutDraw);

      prev = passEvents[i];
    }

    const DrawcallDescription *d = m_pDriver->GetDrawcall(passEvents[i]);

    if(d)
      InitPostVSBuffers(passEvents[i]);
  }
}

MeshFormat GLReplay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                      MeshDataStage stage)
{
  GLPostVSData postvs;
  RDCEraseEl(postvs);

  // no multiview support
  (void)viewID;

  ContextPair ctx = {m_ReplayCtx.ctx, m_pDriver->ShareCtx(m_ReplayCtx.ctx)};

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  const GLPostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf)
    ret.indexResourceId = m_pDriver->GetResourceManager()->GetID(BufferRes(ctx, s.idxBuf));
  else
    ret.indexResourceId = ResourceId();
  ret.indexByteOffset = 0;
  ret.indexByteStride = s.idxByteWidth;
  ret.baseVertex = 0;

  if(s.buf)
    ret.vertexResourceId = m_pDriver->GetResourceManager()->GetID(BufferRes(ctx, s.buf));
  else
    ret.vertexResourceId = ResourceId();

  ret.vertexByteOffset = s.instStride * instID;
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;

  ret.showAlpha = false;

  ret.topology = s.topo;
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  if(instID < s.instData.size())
  {
    GLPostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  return ret;
}
