/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <algorithm>
#include "common/common.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"
#include "gl_shader_refl.h"

static void MakeVaryingsFromShaderReflection(ShaderReflection &refl, rdcarray<const char *> &varyings,
                                             uint32_t &stride, ShaderReflection *trimRefl = NULL)
{
  static rdcarray<rdcstr> tmpStrings;
  tmpStrings.reserve(refl.outputSignature.size());
  tmpStrings.clear();

  varyings.clear();

  int32_t posidx = -1;
  stride = 0;

  for(const SigParameter &sig : refl.outputSignature)
  {
    const char *name = sig.varName.c_str();
    size_t len = sig.varName.size();

    bool excludeMatrixRow = false;
    bool excludeTrimmedRefl = false;

    if(trimRefl)
    {
      // search through the trimmed output signature to see if it's there too. If not, we exclude it
      excludeTrimmedRefl = true;

      for(const SigParameter &sig2 : trimRefl->outputSignature)
      {
        if(sig.varName == sig2.varName)
        {
          excludeTrimmedRefl = false;
          break;
        }
      }

      if(excludeTrimmedRefl)
        RDCLOG("Trimming %s from output signature", name);
    }

    // for matrices with names including :row1, :row2 etc we only include :row0
    // as a varying (but increment the stride for all rows to account for the space)
    // and modify the name to remove the :row0 part
    const char *colon = strchr(name, ':');
    if(colon)
    {
      if(name[len - 1] != '0')
      {
        excludeMatrixRow = true;
      }
      else
      {
        tmpStrings.push_back(rdcstr(name, colon - name));
        name = tmpStrings.back().c_str();
      }
    }

    if(!excludeMatrixRow && !excludeTrimmedRefl)
      varyings.push_back(name);

    if(sig.systemValue == ShaderBuiltin::Position)
      posidx = int32_t(varyings.size()) - 1;

    uint32_t outputComponents = (sig.varType == VarType::Double ? 2 : 1) * sig.compCount;

    stride += sizeof(float) * outputComponents;

    // if it was trimmed, we need to leave space so that everything else lines up
    if(excludeTrimmedRefl)
    {
      const char *skipComponents[] = {
          "gl_SkipComponents1", "gl_SkipComponents2", "gl_SkipComponents3", "gl_SkipComponents4",
      };

      while(outputComponents > 0)
      {
        // skip 1, 2, 3 or 4 components at a time
        varyings.push_back(skipComponents[RDCMIN(4U, outputComponents) - 1]);
        outputComponents -= RDCMIN(4U, outputComponents);
      }
    }
  }

  // shift position attribute up to first, keeping order otherwise
  // the same
  if(posidx > 0)
  {
    const char *pos = varyings[posidx];
    varyings.erase(posidx);
    varyings.insert(0, pos);
  }
}

static GLuint RecompileShader(WrappedOpenGL &drv, const WrappedOpenGL::ShaderData &shadDetails,
                              uint32_t drawIndex)
{
  GLuint ret = drv.glCreateShader(shadDetails.type);

  if(!shadDetails.sources.empty())
  {
    rdcstr source_string;

    for(const rdcstr &s : shadDetails.sources)
      source_string += s;

    // substitute out gl_DrawID use
    int offs = 0;
    do
    {
      offs = source_string.find("gl_DrawID", offs + 1);
      if(offs < 0)
        break;

      // check word boundary at the start
      if(isalnum(source_string[offs - 1]) || source_string[offs - 1] == '_')
        continue;

      // go to the end of the word
      int end = offs + 9;

      // if it's gl_DrawIDARB, allow that.
      if(end + 3 < source_string.count() && source_string[end + 0] == 'A' &&
         source_string[end + 1] == 'R' && source_string[end + 2] == 'B')
        end += 3;

      // check word boundary at the end
      if(isalnum(source_string[end]) || source_string[end] == '_')
        continue;

      // otherwise we've found a match. Add brackets and add our draw index
      source_string.insert(end, StringFormat::Fmt("+%u)", drawIndex));
      source_string.insert(offs, '(');

      // ensure we start searching from after the modified entry
      offs++;
    } while(offs > 0);

    const char *cstr = source_string.c_str();

    drv.glShaderSource(ret, 1, &cstr, NULL);
    drv.glCompileShader(ret);
  }
  else if(!shadDetails.spirvWords.empty())
  {
    drv.glShaderBinary(1, &ret, eGL_SHADER_BINARY_FORMAT_SPIR_V, shadDetails.spirvWords.data(),
                       GLsizei(shadDetails.spirvWords.size() * sizeof(uint32_t)));

    drv.glSpecializeShader(ret, shadDetails.entryPoint.c_str(), (GLuint)shadDetails.specIDs.size(),
                           shadDetails.specIDs.data(), shadDetails.specValues.data());
  }

  GLint status = 0;
  drv.glGetShaderiv(ret, eGL_COMPILE_STATUS, &status);

  if(status == 0)
  {
    char buffer[1024] = {};
    drv.glGetShaderInfoLog(ret, 1024, NULL, buffer);
    RDCERR("Trying to recreate postvs program, couldn't compile shader:\n%s", buffer);
  }

  return ret;
}

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

  GLPostVSData &ret = m_PostVSData[eventId];

  if(m_pDriver->IsUnsafeDraw(eventId))
  {
    ret.gsout.status = ret.vsout.status = "Errors detected with drawcall";
    return;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLMarkerRegion postvs(StringFormat::Fmt("PostVS for %u", eventId));

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

  if(HasExt[ARB_query_buffer_object])
    drv.glBindBuffer(eGL_QUERY_BUFFER, 0);

  // reflection structures
  ShaderReflection *vsRefl = NULL;
  ShaderReflection *tesRefl = NULL;
  ShaderReflection *gsRefl = NULL;
  SPIRVPatchData vsPatch, tesPatch, gsPatch;

  // the program we'll be binding, that we attach shaders to
  GLuint feedbackProg = drv.glCreateProgram();

  // one shader per stage (vs = 0, etc)
  GLuint stageShaders[4] = {};

  // temporary programs created as needed if the original program was created with
  // glCreateShaderProgramv and we don't have a shader to attach
  GLuint tmpShaders[4] = {};

  // the ID if we need to recompile into tmpShaders. This can also be required if gl_DrawID is used
  // since we can't get that faithfully.
  ResourceId recompile[4] = {};

  // these are the 'real' programs with uniform values that we need to copy over to our separable
  // programs. They may be duplicated if there's one program bound to multiple ages
  // one program per stage (vs = 0, etc)
  GLuint stageSrcPrograms[4] = {};

  const ActionDescription *action = m_pDriver->GetAction(eventId);
  const GLDrawParams &drawParams = m_pDriver->GetDrawParameters(eventId);

  if(action->numIndices == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 indices/vertices)";
    return;
  }

  if((action->flags & ActionFlags::Instanced) && action->numInstances == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 instances)";
    return;
  }

  uint32_t glslVer = 0;
  FixedFunctionVertexOutputs outputUsage = {};

  if(rs.Program.name == 0)
  {
    if(rs.Pipeline.name == 0)
    {
      ret.gsout.status = ret.vsout.status = "No program or pipeline bound at draw";
      RDCERR("%s", ret.vsout.status.c_str());
      return;
    }
    else
    {
      ResourceId id = rm->GetResID(rs.Pipeline);
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      for(int i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          ShaderReflection *refl = NULL;
          if(i == 0)
          {
            refl = vsRefl = GetShader(ResourceId(), pipeDetails.stageShaders[i], ShaderEntryPoint());
            glslVer = m_pDriver->m_Shaders[pipeDetails.stageShaders[0]].version;
            vsPatch = m_pDriver->m_Shaders[pipeDetails.stageShaders[0]].patchData;

            CheckVertexOutputUses(m_pDriver->m_Shaders[pipeDetails.stageShaders[0]].sources,
                                  outputUsage);
          }
          else if(i == 2)
          {
            refl = tesRefl = GetShader(ResourceId(), pipeDetails.stageShaders[2], ShaderEntryPoint());
            tesPatch = m_pDriver->m_Shaders[pipeDetails.stageShaders[2]].patchData;
          }
          else if(i == 3)
          {
            refl = gsRefl = GetShader(ResourceId(), pipeDetails.stageShaders[3], ShaderEntryPoint());
            gsPatch = m_pDriver->m_Shaders[pipeDetails.stageShaders[3]].patchData;
          }

          stageShaders[i] = rm->GetCurrentResource(pipeDetails.stageShaders[i]).name;
          stageSrcPrograms[i] = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;

          if(stageShaders[i] == stageSrcPrograms[i])
          {
            const WrappedOpenGL::ProgramData &progDetails =
                m_pDriver->m_Programs[pipeDetails.stagePrograms[i]];

            if(progDetails.shaderProgramUnlinkable)
            {
              recompile[i] = pipeDetails.stageShaders[i];
            }
          }

          if(refl)
          {
            for(const SigParameter &sig : refl->inputSignature)
            {
              if(sig.systemValue == ShaderBuiltin::DrawIndex)
              {
                recompile[i] = pipeDetails.stageShaders[i];
                break;
              }
            }
          }
        }
      }
    }
  }
  else
  {
    auto &progDetails = m_pDriver->m_Programs[rm->GetResID(rs.Program)];

    for(int i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[0] != ResourceId())
      {
        ShaderReflection *refl = NULL;
        if(i == 0)
        {
          refl = vsRefl = GetShader(ResourceId(), progDetails.stageShaders[0], ShaderEntryPoint());
          glslVer = m_pDriver->m_Shaders[progDetails.stageShaders[0]].version;
          vsPatch = m_pDriver->m_Shaders[progDetails.stageShaders[0]].patchData;

          CheckVertexOutputUses(m_pDriver->m_Shaders[progDetails.stageShaders[0]].sources,
                                outputUsage);
        }
        else if(i == 2 && progDetails.stageShaders[2] != ResourceId())
        {
          refl = tesRefl = GetShader(ResourceId(), progDetails.stageShaders[2], ShaderEntryPoint());
          tesPatch = m_pDriver->m_Shaders[progDetails.stageShaders[2]].patchData;
        }
        else if(i == 3 && progDetails.stageShaders[3] != ResourceId())
        {
          refl = gsRefl = GetShader(ResourceId(), progDetails.stageShaders[3], ShaderEntryPoint());
          gsPatch = m_pDriver->m_Shaders[progDetails.stageShaders[3]].patchData;
        }

        stageShaders[i] = rm->GetCurrentResource(progDetails.stageShaders[i]).name;

        if(refl)
        {
          for(const SigParameter &sig : refl->inputSignature)
          {
            if(sig.systemValue == ShaderBuiltin::DrawIndex)
            {
              recompile[i] = progDetails.stageShaders[i];
              break;
            }
          }
        }
      }

      stageSrcPrograms[i] = rs.Program.name;
    }
  }

  for(int i = 0; i < 4; i++)
  {
    if(recompile[i] != ResourceId())
    {
      const WrappedOpenGL::ShaderData &shadDetails = m_pDriver->m_Shaders[recompile[i]];

      stageShaders[i] = tmpShaders[i] = RecompileShader(drv, shadDetails, action->drawIndex);
    }
  }

  if(vsRefl == NULL || stageShaders[0] == 0)
  {
    ret.gsout.status = ret.vsout.status = "No vertex shader bound";

    // delete any temporaries
    for(size_t i = 0; i < 4; i++)
      if(tmpShaders[i])
        drv.glDeleteShader(tmpShaders[i]);

    return;
  }

  if(tesRefl || gsRefl)
  {
    // put a general error in here in case anything goes wrong fetching VS outputs
    ret.gsout.status =
        "No geometry/tessellation output fetched due to error processing vertex stage.";
  }
  else
  {
    ret.gsout.status = "No geometry and no tessellation shader bound.";
  }

  // GLES requires a fragment shader even with rasterizer discard, so we'll attach this
  GLuint dummyFrag = 0;

  if(IsGLES)
  {
    dummyFrag = drv.glCreateShader(eGL_FRAGMENT_SHADER);

    if(glslVer == 0)
      glslVer = 100;

    rdcstr src =
        StringFormat::Fmt("#version %d %s\nvoid main() {}\n", glslVer, glslVer == 100 ? "" : "es");

    const char *csrc = src.c_str();

    drv.glShaderSource(dummyFrag, 1, &csrc, NULL);
    drv.glCompileShader(dummyFrag);

    GLint status = 0;
    drv.glGetShaderiv(dummyFrag, eGL_COMPILE_STATUS, &status);

    if(status == 0)
    {
      drv.glDeleteShader(dummyFrag);
      dummyFrag = 0;

      if(HasExt[ARB_separate_shader_objects])
      {
        RDCERR(
            "Couldn't create dummy fragment shader for GLES, trying to set program to be "
            "separable");
        drv.glProgramParameteri(feedbackProg, eGL_PROGRAM_SEPARABLE, GL_TRUE);
      }
      else
      {
        RDCERR(
            "Couldn't create dummy fragment shader for GLES, separable programs not available. "
            "Vertex output data will likely be broken");
      }
    }
  }

  uint32_t stride = 0;
  GLuint vsOrigShader = 0;

  bool hasPosition = false;

  for(const SigParameter &sig : vsRefl->outputSignature)
  {
    if(sig.systemValue == ShaderBuiltin::Position)
    {
      hasPosition = true;
      break;
    }
  }

  if(vsRefl->encoding == ShaderEncoding::OpenGLSPIRV)
  {
    // SPIR-V path
    vsOrigShader = stageShaders[0];

    stageShaders[0] = tmpShaders[0] = drv.glCreateShader(eGL_VERTEX_SHADER);

    rdcarray<uint32_t> spirv;
    spirv.resize(vsRefl->rawBytes.size() / sizeof(uint32_t));
    memcpy(spirv.data(), vsRefl->rawBytes.data(), vsRefl->rawBytes.size());

    AddXFBAnnotations(*vsRefl, vsPatch, 0, vsRefl->entryPoint.c_str(), spirv, stride);

    drv.glShaderBinary(1, &stageShaders[0], eGL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
                       (GLsizei)spirv.size() * 4);

    drv.glSpecializeShader(stageShaders[0], vsRefl->entryPoint.c_str(), 0, NULL, NULL);

    char buffer[1024] = {};
    GLint status = 0;
    GL.glGetShaderiv(stageShaders[0], eGL_COMPILE_STATUS, &status);
    if(status == 0)
    {
      GL.glGetShaderInfoLog(stageShaders[0], 1024, NULL, buffer);
      RDCERR("SPIR-V post-vs patched shader compile error: %s", buffer);
      ret.vsout.status = "Failed to patch SPIR-V vertex shader to use transform feedback.";
      return;
    }
    // attach the vertex shader
    drv.glAttachShader(feedbackProg, stageShaders[0]);

    // attach the dummy fragment shader, if it exists
    if(dummyFrag)
      drv.glAttachShader(feedbackProg, dummyFrag);

    drv.glLinkProgram(feedbackProg);

    drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

    if(status == 0)
    {
      drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);
      RDCERR("SPIR-V post-vs patched program link error: %s", buffer);
      ret.vsout.status = "Failed to patch SPIR-V vertex shader to use transform feedback.";
      return;
    }
  }
  else
  {
    // non-SPIRV path

    // attach the vertex shader
    drv.glAttachShader(feedbackProg, stageShaders[0]);

    // attach the dummy fragment shader, if it exists
    if(dummyFrag)
      drv.glAttachShader(feedbackProg, dummyFrag);

    CopyProgramAttribBindings(stageSrcPrograms[0], feedbackProg, vsRefl);

    rdcarray<const char *> varyings;
    MakeVaryingsFromShaderReflection(*vsRefl, varyings, stride);

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
    // (from the PROGRAM_OUTPUT interface names reflected out): "Out.Color", "gl_Position"
    // however nvidia complains unless you use "Color", "gl_Position". This holds even if you add
    // other variables to the vertex struct.
    //
    // strangely another sample that in-lines the output block like so:
    //
    // out block
    // {
    //   vec2 Texcoord;
    // } Out;
    //
    // uses "block.Texcoord" (reflected name from PROGRAM_OUTPUT and accepted by varyings string on
    // both vendors). This is inconsistent as it's type.member not structname.member as move.
    //
    // The spec is very vague on exactly what these names should be, so I can't say which is correct
    // out of these three possibilities.
    //
    // So our 'fix' is to loop while we have problems linking with the varyings (since we know
    // otherwise linking should succeed, as we only get here with a successfully linked separable
    // program - if it fails to link, it's assigned 0 earlier) and remove any prefixes from
    // variables seen in the link error string.
    // The error string is something like:
    // "error: Varying (named Out.Color) specified but not present in the program object."
    //
    // Yeh. Ugly. Not guaranteed to work at all, but hopefully the common case will just be a single
    // block without any nesting so this might work.
    // At least we don't have to reallocate strings all over, since the memory is
    // already owned elsewhere, we just need to modify pointers to trim prefixes. Bright side?

    GLint status = 0;
    bool finished = false;
    for(;;)
    {
      // don't print debug messages from these links - we know some might fail but as long as we
      // eventually get one to work that's fine.
      drv.SuppressDebugMessages(true);

      // specify current varyings & relink
      drv.glTransformFeedbackVaryings(feedbackProg, (GLsizei)varyings.size(), &varyings[0],
                                      eGL_INTERLEAVED_ATTRIBS);
      drv.glLinkProgram(feedbackProg);

      drv.SuppressDebugMessages(false);

      drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

      // all good! Hopefully we'll mostly hit this
      if(status == 1)
        break;

      RDCWARN("Failed to link postvs program with varyings");

      // if finished is true, this was our last attempt - there are no
      // more fixups possible
      if(finished)
      {
        RDCWARN("No fixups possible");
        break;
      }

      RDCLOG("Attempting fixup...");

      char buffer[1025] = {0};
      drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);

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

    if(status == 0)
    {
      // if we STILL can't link then something is really messy. Some drivers like AMD reflect out
      // unused variables when reflecting a separable program, then complain when they are passed in
      // as varyings. We remove all the varyings, link the program, then reflect it as-is and try to
      // use the output signature from that as the varyings.
      RDCWARN("Failed to generate XFB varyings from normal reflection - making one final attempt.");
      RDCWARN(
          "This is often caused by sensitive drivers and output variables declared but never "
          "written to.");

      drv.SuppressDebugMessages(true);

      drv.glTransformFeedbackVaryings(feedbackProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
      drv.glLinkProgram(feedbackProg);

      drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

      if(status == 1)
      {
        ShaderReflection tempRefl;
        MakeShaderReflection(eGL_VERTEX_SHADER, feedbackProg, tempRefl, outputUsage);

        // remake the varyings with tempRefl to 'trim' the output signature
        MakeVaryingsFromShaderReflection(*vsRefl, varyings, stride, &tempRefl);

        drv.glTransformFeedbackVaryings(feedbackProg, (GLsizei)varyings.size(), &varyings[0],
                                        eGL_INTERLEAVED_ATTRIBS);
        drv.glLinkProgram(feedbackProg);

        drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);
      }
      else
      {
        RDCWARN("Can't link program with no varyings!");
      }

      drv.SuppressDebugMessages(false);
    }

    if(status == 0)
    {
      char buffer[1025] = {0};
      drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);
      RDCERR("Failed to fix-up. Link error making xfb vs program: %s", buffer);

      // delete any temporaries
      for(size_t i = 0; i < 4; i++)
        if(tmpShaders[i])
          drv.glDeleteShader(tmpShaders[i]);

      drv.glDeleteShader(dummyFrag);

      drv.glDeleteProgram(feedbackProg);

      ret.vsout.status = "Failed to relink program to use transform feedback.";
      return;
    }
  }

  // here the SPIR-V and GLSL paths recombine.

  // copy across any uniform values, bindings etc from the real program containing
  // the vertex stage
  {
    PerStageReflections stages;
    m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), stageSrcPrograms[0]), stages);

    PerStageReflections dstStages;
    m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), feedbackProg), dstStages);

    CopyProgramUniforms(stages, stageSrcPrograms[0], dstStages, feedbackProg);
  }

  // we don't want to do any work, so just discard before rasterizing
  drv.glEnable(eGL_RASTERIZER_DISCARD);

  // bind our program and do the feedback draw
  drv.glUseProgram(feedbackProg);
  drv.glBindProgramPipeline(0);

  if(HasExt[ARB_transform_feedback2])
    drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);

  bool flipY = false;

  if(HasExt[ARB_clip_control])
  {
    GLenum clipOrigin = eGL_LOWER_LEFT;
    GL.glGetIntegerv(eGL_CLIP_ORIGIN, (GLint *)&clipOrigin);

    if(clipOrigin == eGL_UPPER_LEFT)
      flipY = true;
  }

  GLuint idxBuf = 0;

  if(vsRefl->outputSignature.empty())
  {
    // nothing to do, store an empty cache
  }
  else
  {
    if(!(action->flags & ActionFlags::Indexed))
    {
      uint64_t outputSize = uint64_t(action->numIndices) * stride;

      if(action->flags & ActionFlags::Instanced)
        outputSize *= action->numInstances;

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

      if(action->flags & ActionFlags::Instanced)
      {
        if(HasExt[ARB_base_instance])
        {
          drv.glDrawArraysInstancedBaseInstance(eGL_POINTS, action->vertexOffset, action->numIndices,
                                                action->numInstances, action->instanceOffset);
        }
        else
        {
          drv.glDrawArraysInstanced(eGL_POINTS, action->vertexOffset, action->numIndices,
                                    action->numInstances);
        }
      }
      else
      {
        drv.glDrawArrays(eGL_POINTS, action->vertexOffset, action->numIndices);
      }
    }
    else    // action is indexed
    {
      ResourceId idxId = rm->GetResID(BufferRes(drv.GetCtx(), elArrayBuffer));

      bytebuf idxdata;
      GetBufferData(idxId, action->indexOffset * drawParams.indexWidth,
                    action->numIndices * drawParams.indexWidth, idxdata);

      rdcarray<uint32_t> indices;

      uint8_t *idx8 = (uint8_t *)&idxdata[0];
      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(idxdata.size() / drawParams.indexWidth), action->numIndices);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = 0;
        if(drawParams.indexWidth == 1)
          i32 = uint32_t(idx8[i]);
        else if(drawParams.indexWidth == 2)
          i32 = uint32_t(idx16[i]);
        else if(drawParams.indexWidth == 4)
          i32 = idx32[i];

        auto it = std::lower_bound(indices.begin(), indices.end(), i32);

        if(it != indices.end() && *it == i32)
          continue;

        indices.insert(it - indices.begin(), i32);
      }

      // if we read out of bounds, we'll also have a 0 index being referenced
      // (as 0 is read). Don't insert 0 if we already have 0 though
      if(numIndices < action->numIndices && (indices.empty() || indices[0] != 0))
        indices.insert(0, 0);

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
      std::map<uint32_t, size_t> indexRemap;
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

      if(action->flags & ActionFlags::Instanced)
        outputSize *= action->numInstances;

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

      if(action->flags & ActionFlags::Instanced)
      {
        if(HasExt[ARB_base_instance])
        {
          drv.glDrawElementsInstancedBaseVertexBaseInstance(
              eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL, action->numInstances,
              action->baseVertex, action->instanceOffset);
        }
        else
        {
          drv.glDrawElementsInstancedBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT,
                                                NULL, action->numInstances, action->baseVertex);
        }
      }
      else
      {
        drv.glDrawElementsBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL,
                                     action->baseVertex);
      }

      // delete the buffer, we don't need it anymore
      drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
      drv.glDeleteBuffers(1, &indexSetBuffer);

      uint32_t stripRestartValue32 = 0;

      if(rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart] ||
         rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex])
      {
        stripRestartValue32 = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                  ? ~0U
                                  : rs.PrimitiveRestartIndex;
      }

      // rebase existing index buffer to point from 0 onwards (which will index into our
      // stream-out'd vertex buffer)
      if(drawParams.indexWidth == 1)
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
      else if(drawParams.indexWidth == 2)
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
      ret.vsout.status = "Error obtaining vertex data via transform feedback";
    }

    // get buffer data from buffer attached to feedback object
    float *data = (float *)drv.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

    if(data == NULL)
    {
      drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
      RDCERR("Couldn't map feedback buffer!");
      error = true;
      ret.vsout.status = "Error reading back vertex data from GPU";
    }

    if(error)
    {
      // restore replay state we trashed
      drv.glUseProgram(rs.Program.name);
      drv.glBindProgramPipeline(rs.Pipeline.name);

      drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
      drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

      if(HasExt[ARB_transform_feedback2])
        drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

      if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
        drv.glDisable(eGL_RASTERIZER_DISCARD);
      else
        drv.glEnable(eGL_RASTERIZER_DISCARD);

      // delete any temporaries
      for(size_t i = 0; i < 4; i++)
        if(tmpShaders[i])
          drv.glDeleteShader(tmpShaders[i]);

      drv.glDeleteShader(dummyFrag);

      drv.glDeleteProgram(feedbackProg);

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

    for(GLuint i = 1; hasPosition && i < primsWritten; i++)
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

        if(m == 1.0f || c == 0.0f)
          continue;

        if(-c / m <= 0.000001f)
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
    ret.vsin.topo = drawParams.topo;
    ret.vsout.buf = vsoutBuffer;
    ret.vsout.vertStride = stride;
    ret.vsout.nearPlane = nearp;
    ret.vsout.farPlane = farp;

    ret.vsout.useIndices = bool(action->flags & ActionFlags::Indexed);
    ret.vsout.numVerts = action->numIndices;

    ret.vsout.instStride = 0;
    if(action->flags & ActionFlags::Instanced)
      ret.vsout.instStride = (stride * primsWritten) / RDCMAX(1U, action->numInstances);

    ret.vsout.idxBuf = 0;
    ret.vsout.idxByteWidth = drawParams.indexWidth;
    if(ret.vsout.useIndices && idxBuf)
    {
      ret.vsout.idxBuf = idxBuf;
    }

    ret.vsout.hasPosOut = hasPosition;

    ret.vsout.topo = drawParams.topo;
  }

  if(tesRefl || gsRefl)
  {
    ret.gsout.status.clear();

    ShaderReflection *lastRefl = gsRefl;
    SPIRVPatchData lastPatch = gsPatch;
    int lastIndex = 3;

    if(!lastRefl)
    {
      lastRefl = tesRefl;
      lastPatch = tesPatch;
      lastIndex = 2;
    }

    bool lastSPIRV = (lastRefl->encoding == ShaderEncoding::OpenGLSPIRV);

    RDCASSERT(lastRefl);

    // if the vertex shader was SPIR-V we didn't attach it and instead attached a tmp one with
    // patched SPIR-V. Detach it and attach the original one without any XFB annotations
    if(vsOrigShader)
    {
      drv.glDetachShader(feedbackProg, stageShaders[0]);
      stageShaders[0] = vsOrigShader;
      drv.glAttachShader(feedbackProg, stageShaders[0]);
    }

    // attach the other non-vertex shaders
    for(int i = 1; i < 4; i++)
    {
      if(stageShaders[i])
      {
        // if the last shader is non-SPIR-V, don't attach it - we'll build our own
        if(lastSPIRV && i == lastIndex)
          continue;

        drv.glAttachShader(feedbackProg, stageShaders[i]);
      }
    }

    GLint status = 0;

    hasPosition = false;

    for(const SigParameter &sig : lastRefl->outputSignature)
    {
      if(sig.systemValue == ShaderBuiltin::Position)
      {
        hasPosition = true;
        break;
      }
    }

    if(lastSPIRV)
    {
      // SPIR-V path
      stageShaders[lastIndex] = tmpShaders[lastIndex] = drv.glCreateShader(ShaderEnum(lastIndex));

      rdcarray<uint32_t> spirv;
      spirv.resize(lastRefl->rawBytes.size() / sizeof(uint32_t));
      memcpy(spirv.data(), lastRefl->rawBytes.data(), lastRefl->rawBytes.size());

      AddXFBAnnotations(*lastRefl, lastPatch, 0, lastRefl->entryPoint.c_str(), spirv, stride);

      drv.glShaderBinary(1, &stageShaders[lastIndex], eGL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
                         (GLsizei)spirv.size() * 4);

      drv.glSpecializeShader(stageShaders[lastIndex], lastRefl->entryPoint.c_str(), 0, NULL, NULL);

      char buffer[1024] = {};
      GL.glGetShaderiv(stageShaders[lastIndex], eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        GL.glGetShaderInfoLog(stageShaders[lastIndex], 1024, NULL, buffer);
        RDCERR("SPIR-V post-gs patched shader compile error: %s", buffer);
        ret.gsout.status =
            "Failed to patch SPIR-V geometry/tessellation shader to use transform feedback.";
        return;
      }

      // attach the last shader
      drv.glAttachShader(feedbackProg, stageShaders[lastIndex]);

      drv.glLinkProgram(feedbackProg);

      drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

      if(status == 0)
      {
        drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);
        RDCERR("SPIR-V post-gs patched program link error: %s", buffer);
        ret.gsout.status =
            "Failed to patch SPIR-V geometry/tessellation shader to use transform feedback.";
        return;
      }
    }
    else
    {
      rdcarray<const char *> varyings;

      MakeVaryingsFromShaderReflection(*lastRefl, varyings, stride);

      // see above for the justification/explanation of this monstrosity.

      bool finished = false;
      for(;;)
      {
        drv.SuppressDebugMessages(true);

        // specify current varyings & relink
        drv.glTransformFeedbackVaryings(feedbackProg, (GLsizei)varyings.size(), &varyings[0],
                                        eGL_INTERLEAVED_ATTRIBS);
        drv.glLinkProgram(feedbackProg);

        drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

        drv.SuppressDebugMessages(false);

        // all good! Hopefully we'll mostly hit this
        if(status == 1)
          break;

        // if finished is true, this was our last attempt - there are no
        // more fixups possible
        if(finished)
          break;

        char buffer[1025] = {0};
        drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);

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

              // first check this won't cause a duplicate - if it does, we have to try something
              // else
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

      if(status == 0)
      {
        // if we STILL can't link then something is really messy. Some drivers like AMD reflect out
        // unused variables when reflecting a separable program, then complain when they are passed
        // in as varyings. We remove all the varyings, link the program, then reflect it as-is and
        // try to use the output signature from that as the varyings.
        RDCWARN(
            "Failed to generate XFB varyings from normal reflection - making one final attempt.");
        RDCWARN(
            "This is often caused by sensitive drivers and output variables declared but never "
            "written to.");

        drv.SuppressDebugMessages(true);

        drv.glTransformFeedbackVaryings(feedbackProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
        drv.glLinkProgram(feedbackProg);

        drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);

        if(status == 1)
        {
          ShaderReflection tempRefl;
          MakeShaderReflection(ShaderEnum((size_t)lastRefl->stage), feedbackProg, tempRefl,
                               outputUsage);

          // remake the varyings with tempRefl to 'trim' the output signature
          MakeVaryingsFromShaderReflection(*lastRefl, varyings, stride, &tempRefl);

          drv.glTransformFeedbackVaryings(feedbackProg, (GLsizei)varyings.size(), &varyings[0],
                                          eGL_INTERLEAVED_ATTRIBS);
          drv.glLinkProgram(feedbackProg);

          drv.glGetProgramiv(feedbackProg, eGL_LINK_STATUS, &status);
        }
        else
        {
          RDCWARN("Can't link program with no varyings!");
        }

        drv.SuppressDebugMessages(false);
      }
    }

    // detach the shaders now that linking is complete
    for(int i = 0; i < 4; i++)
      if(stageShaders[i])
        drv.glDetachShader(feedbackProg, stageShaders[i]);

    if(status == 0)
    {
      char buffer[1025] = {0};
      drv.glGetProgramInfoLog(feedbackProg, 1024, NULL, buffer);
      RDCERR("Failed to fix-up. Link error making xfb last program: %s", buffer);
    }
    else
    {
      PerStageReflections dstStages;
      m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), feedbackProg), dstStages);

      // copy across any uniform values, bindings etc from the real program containing
      // the vertex stage
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), stageSrcPrograms[0]), stages);

        CopyProgramUniforms(stages, stageSrcPrograms[0], dstStages, feedbackProg);
      }

      // if tessellation is enabled, bind & copy uniforms. Note, control shader is optional
      // independent of eval shader (default values are used for the tessellation levels).
      if(stageSrcPrograms[1])
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), stageSrcPrograms[1]), stages);

        CopyProgramUniforms(stages, stageSrcPrograms[1], dstStages, feedbackProg);
      }

      if(stageSrcPrograms[2])
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), stageSrcPrograms[2]), stages);

        CopyProgramUniforms(stages, stageSrcPrograms[2], dstStages, feedbackProg);
      }

      // if we have a geometry shader, bind & copy uniforms
      if(stageSrcPrograms[3])
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(drv.GetCtx(), stageSrcPrograms[3]), stages);

        CopyProgramUniforms(stages, stageSrcPrograms[3], dstStages, feedbackProg);
      }

      // bind our program and do the feedback draw
      drv.glUseProgram(feedbackProg);
      drv.glBindProgramPipeline(0);

      if(HasExt[ARB_transform_feedback2])
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

      if(action->flags & ActionFlags::Instanced)
        maxOutputSize *= action->numInstances;

      uint32_t numInputPrimitives = action->numIndices;
      GLenum drawtopo = MakeGLPrimitiveTopology(drawParams.topo);

      switch(drawParams.topo)
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
          numInputPrimitives /= PatchList_Count(drawParams.topo);
          break;
      }

      if(lastRefl == gsRefl)
      {
        drv.glGetProgramiv(feedbackProg, eGL_GEOMETRY_OUTPUT_TYPE, (GLint *)&shaderOutMode);

        GLint maxVerts = 1;

        drv.glGetProgramiv(feedbackProg, eGL_GEOMETRY_VERTICES_OUT, (GLint *)&maxVerts);

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
        drv.glGetProgramiv(feedbackProg, eGL_TESS_GEN_MODE, (GLint *)&shaderOutMode);

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
      if(drawParams.indexWidth == 2)
        idxType = eGL_UNSIGNED_SHORT;
      else if(drawParams.indexWidth == 4)
        idxType = eGL_UNSIGNED_INT;

      // instanced draws must be replayed one at a time so we can record the number of primitives
      // from
      // each drawcall, as due to expansion this can vary per-instance.
      if(action->flags & ActionFlags::Instanced)
      {
        // if there is only one instance it's a trivial case and we don't need to bother with the
        // expensive path
        if(action->numInstances > 1)
        {
          // ensure we have enough queries
          uint32_t curSize = (uint32_t)DebugData.feedbackQueries.size();
          if(curSize < action->numInstances)
          {
            DebugData.feedbackQueries.resize(action->numInstances);
            drv.glGenQueries(action->numInstances - curSize,
                             DebugData.feedbackQueries.data() + curSize);
          }

          // do incremental draws to get the output size. We have to do this O(N^2) style because
          // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
          // instances and count the total number of verts each time, then we can see from the
          // difference how much each instance wrote.
          for(uint32_t inst = 1; inst <= action->numInstances; inst++)
          {
            drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
            drv.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
                             DebugData.feedbackQueries[inst - 1]);
            drv.glBeginTransformFeedback(lastOutTopo);

            if(!(action->flags & ActionFlags::Indexed))
            {
              if(HasExt[ARB_base_instance])
              {
                drv.glDrawArraysInstancedBaseInstance(
                    drawtopo, action->vertexOffset, action->numIndices, inst, action->instanceOffset);
              }
              else
              {
                drv.glDrawArraysInstanced(drawtopo, action->vertexOffset, action->numIndices, inst);
              }
            }
            else
            {
              if(HasExt[ARB_base_instance])
              {
                drv.glDrawElementsInstancedBaseVertexBaseInstance(
                    drawtopo, action->numIndices, idxType,
                    (const void *)(uintptr_t(action->indexOffset) * uintptr_t(drawParams.indexWidth)),
                    inst, action->baseVertex, action->instanceOffset);
              }
              else
              {
                drv.glDrawElementsInstancedBaseVertex(
                    drawtopo, action->numIndices, idxType,
                    (const void *)(uintptr_t(action->indexOffset) * uintptr_t(drawParams.indexWidth)),
                    inst, action->baseVertex);
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

          if(!(action->flags & ActionFlags::Indexed))
          {
            if(HasExt[ARB_base_instance])
            {
              drv.glDrawArraysInstancedBaseInstance(drawtopo, action->vertexOffset,
                                                    action->numIndices, action->numInstances,
                                                    action->instanceOffset);
            }
            else
            {
              drv.glDrawArraysInstanced(drawtopo, action->vertexOffset, action->numIndices,
                                        action->numInstances);
            }
          }
          else
          {
            if(HasExt[ARB_base_instance])
            {
              drv.glDrawElementsInstancedBaseVertexBaseInstance(
                  drawtopo, action->numIndices, idxType,
                  (const void *)(uintptr_t(action->indexOffset) * uintptr_t(drawParams.indexWidth)),
                  action->numInstances, action->baseVertex, action->instanceOffset);
            }
            else
            {
              drv.glDrawElementsInstancedBaseVertex(
                  drawtopo, action->numIndices, idxType,
                  (const void *)(uintptr_t(action->indexOffset) * uintptr_t(drawParams.indexWidth)),
                  action->numInstances, action->baseVertex);
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

        if(!(action->flags & ActionFlags::Indexed))
        {
          drv.glDrawArrays(drawtopo, action->vertexOffset, action->numIndices);
        }
        else
        {
          drv.glDrawElementsBaseVertex(
              drawtopo, action->numIndices, idxType,
              (const void *)(uintptr_t(action->indexOffset) * uintptr_t(drawParams.indexWidth)),
              action->baseVertex);
        }

        drv.glEndTransformFeedback();
        drv.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
      }

      rdcarray<GLPostVSData::InstData> instData;

      GLuint primsWritten = 0;

      if((action->flags & ActionFlags::Instanced) && action->numInstances > 1)
      {
        uint64_t prevVertCount = 0;

        for(uint32_t inst = 0; inst < action->numInstances; inst++)
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

      bool error = false;

      if(primsWritten == 0)
      {
        RDCWARN("No primitives written by last vertex processing stage");
        error = true;
        ret.gsout.status = "No detectable output generated by geometry/tessellation shaders";
      }

      // get buffer data from buffer attached to feedback object
      float *data = (float *)drv.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

      if(data == NULL)
      {
        drv.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
        RDCERR("Couldn't map feedback buffer!");
        ret.gsout.status = "Couldn't read back geometry/tessellation output data from GPU";
        error = true;
      }

      if(error)
      {
        // delete temporary program we made
        drv.glDeleteProgram(feedbackProg);

        // restore replay state we trashed
        drv.glUseProgram(rs.Program.name);
        drv.glBindProgramPipeline(rs.Pipeline.name);

        drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

        if(HasExt[ARB_transform_feedback2])
          drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

        if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
          drv.glDisable(eGL_RASTERIZER_DISCARD);
        else
          drv.glEnable(eGL_RASTERIZER_DISCARD);

        // delete any temporaries
        for(size_t i = 0; i < 4; i++)
          if(tmpShaders[i])
            drv.glDeleteShader(tmpShaders[i]);

        drv.glDeleteShader(dummyFrag);

        return;
      }

      if(lastRefl == tesRefl)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_TRIANGLES ||
           shaderOutMode == eGL_QUADS)    // query for quads returns # triangles
          ret.gsout.numVerts = primsWritten * 3;
        else if(shaderOutMode == eGL_ISOLINES)
          ret.gsout.numVerts = primsWritten * 2;
      }
      else if(lastRefl == gsRefl)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_POINTS)
          ret.gsout.numVerts = primsWritten;
        else if(shaderOutMode == eGL_LINE_STRIP)
          ret.gsout.numVerts = primsWritten * 2;
        else if(shaderOutMode == eGL_TRIANGLE_STRIP)
          ret.gsout.numVerts = primsWritten * 3;
      }

      // create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
      // can render from it to display previews).
      GLuint lastoutBuffer = 0;
      drv.glGenBuffers(1, &lastoutBuffer);
      drv.glBindBuffer(eGL_ARRAY_BUFFER, lastoutBuffer);
      drv.glNamedBufferDataEXT(lastoutBuffer, stride * ret.gsout.numVerts, data, eGL_STATIC_DRAW);

      byte *byteData = (byte *)data;

      float nearp = 0.1f;
      float farp = 100.0f;

      Vec4f *pos0 = (Vec4f *)byteData;

      bool found = false;

      for(uint32_t i = 1; hasPosition && i < ret.gsout.numVerts; i++)
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

          if(m == 1.0f || c == 0.0f)
            continue;

          if(-c / m <= 0.000001f)
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
      ret.gsout.buf = lastoutBuffer;
      ret.gsout.instStride = 0;
      if(action->flags & ActionFlags::Instanced)
      {
        ret.gsout.numVerts /= RDCMAX(1U, action->numInstances);
        ret.gsout.instStride = stride * ret.gsout.numVerts;
      }
      ret.gsout.vertStride = stride;
      ret.gsout.nearPlane = nearp;
      ret.gsout.farPlane = farp;

      ret.gsout.useIndices = false;

      ret.gsout.flipY = flipY;

      ret.gsout.hasPosOut = hasPosition;

      ret.gsout.idxBuf = 0;
      ret.gsout.idxByteWidth = 0;

      ret.gsout.topo = MakePrimitiveTopology(lastOutTopo);

      ret.gsout.instData = instData;
    }
  }
  else
  {
    ret.vsout.flipY = flipY;
  }

  // delete temporary program we made
  drv.glDeleteProgram(feedbackProg);

  // restore replay state we trashed
  drv.glUseProgram(rs.Program.name);
  drv.glBindProgramPipeline(rs.Pipeline.name);

  drv.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array].name);
  drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

  if(HasExt[ARB_query_buffer_object])
    drv.glBindBuffer(eGL_QUERY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Query].name);

  if(HasExt[ARB_transform_feedback2])
    drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj.name);

  if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
    drv.glDisable(eGL_RASTERIZER_DISCARD);
  else
    drv.glEnable(eGL_RASTERIZER_DISCARD);

  // delete any temporaries
  for(size_t i = 0; i < 4; i++)
    if(tmpShaders[i])
      drv.glDeleteShader(tmpShaders[i]);

  drv.glDeleteShader(dummyFrag);
}

void GLReplay::InitPostVSBuffers(const rdcarray<uint32_t> &passEvents)
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

    const ActionDescription *d = m_pDriver->GetAction(passEvents[i]);

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

  ContextPair ctx = {m_ReplayCtx.ctx, m_pDriver->GetShareGroup(m_ReplayCtx.ctx)};

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  const GLPostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf)
  {
    ret.indexResourceId = m_pDriver->GetResourceManager()->GetResID(BufferRes(ctx, s.idxBuf));
    ret.indexByteStride = s.idxByteWidth;
    ret.indexByteSize = ~0ULL;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = 0;
  ret.baseVertex = 0;

  if(s.buf)
  {
    ret.vertexResourceId = m_pDriver->GetResourceManager()->GetResID(BufferRes(ctx, s.buf));
    ret.vertexByteSize = ~0ULL;
  }
  else
  {
    ret.vertexResourceId = ResourceId();
  }

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

  ret.flipY = s.flipY;

  if(instID < s.instData.size())
  {
    GLPostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  ret.status = s.status;

  return ret;
}
