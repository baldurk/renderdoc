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

#include "../gl_driver.h"
#include "../gl_shader_refl.h"
#include "common/common.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "strings/string_utils.h"

enum GLshaderbitfield
{
};

DECLARE_REFLECTION_ENUM(GLshaderbitfield);

template <>
std::string DoStringise(const GLshaderbitfield &el)
{
  RDCCOMPILE_ASSERT(sizeof(GLshaderbitfield) == sizeof(GLbitfield) &&
                        sizeof(GLshaderbitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  BEGIN_BITFIELD_STRINGISE(GLshaderbitfield);
  {
    STRINGISE_BITFIELD_BIT(GL_VERTEX_SHADER_BIT);
    STRINGISE_BITFIELD_BIT(GL_TESS_CONTROL_SHADER_BIT);
    STRINGISE_BITFIELD_BIT(GL_TESS_EVALUATION_SHADER_BIT);
    STRINGISE_BITFIELD_BIT(GL_GEOMETRY_SHADER_BIT);
    STRINGISE_BITFIELD_BIT(GL_FRAGMENT_SHADER_BIT);
    STRINGISE_BITFIELD_BIT(GL_COMPUTE_SHADER_BIT);
  }
  END_BITFIELD_STRINGISE();
}

void WrappedOpenGL::ShaderData::Compile(WrappedOpenGL &gl, ResourceId id, GLuint realShader)
{
  bool pointSizeUsed = false, clipDistanceUsed = false;
  if(type == eGL_VERTEX_SHADER)
    CheckVertexOutputUses(sources, pointSizeUsed, clipDistanceUsed);

  string concatenated;

  for(size_t i = 0; i < sources.size(); i++)
  {
    if(sources.size() > 1)
    {
      if(i > 0)
        concatenated += "\n";
      concatenated += "/////////////////////////////";
      concatenated += StringFormat::Fmt("// Source file %u", (uint32_t)i);
      concatenated += "/////////////////////////////";
      concatenated += "\n";
    }

    concatenated += sources[i];
  }

  size_t offs = concatenated.find("#version");

  if(offs == std::string::npos)
  {
    // if there's no #version it's assumed to be 100 which we set below
    version = 0;
  }
  else
  {
    // see if we find a second result after the first
    size_t offs2 = concatenated.find("#version", offs + 1);

    if(offs2 == std::string::npos)
    {
      version = ParseVersionStatement(concatenated.c_str() + offs);
    }
    else
    {
      // slow path, multiple #version matches so the first one might be in a comment. We need to
      // search from the start, past comments and whitespace, to find the first real #version.
      const char *search = concatenated.c_str();
      const char *end = search + concatenated.size();

      while(search < end)
      {
        // skip whitespace
        if(isspace(*search))
        {
          search++;
          continue;
        }

        // skip single-line C++ style comments
        if(search + 1 < end && search[0] == '/' && search[1] == '/')
        {
          // continue until the next newline
          while(search < end && search[0] != '\r' && search[0] != '\n')
            search++;

          // continue, the whitespace skip above will skip the newline
          continue;
        }

        // skip multi-line C style comments
        if(search + 1 < end && search[0] == '/' && search[1] == '*')
        {
          // continue until the ending marker
          while(search + 1 < end && (search[0] != '*' || search[1] != '/'))
            search++;

          // skip the end marker
          search += 2;

          // continue, the whitespace skip above will skip the newline
          continue;
        }

        // missing #version is valid, so just exit
        if(search + sizeof("#version") > end)
        {
          RDCERR("Bad shader - reached end of text after skipping all comments and whitespace");
          break;
        }

        std::string versionText(search, search + sizeof("#version") - 1);

        // if we found the version, parse it
        if(versionText == "#version")
          version = ParseVersionStatement(search);

        // otherwise break - a missing #version is valid, and a legal #version cannot occur anywhere
        // after this point.
        break;
      }
    }
  }

  // default to version 100
  if(version == 0)
    version = 100;

  reflection.encoding = ShaderEncoding::GLSL;
  reflection.rawBytes.assign((byte *)concatenated.c_str(), concatenated.size());

  GLuint sepProg = prog;

  GLint status = 0;
  if(realShader == 0)
    status = 1;
  else
    gl.glGetShaderiv(realShader, eGL_COMPILE_STATUS, &status);

  if(sepProg == 0 && status == 1)
    sepProg = MakeSeparableShaderProgram(gl, type, sources, NULL);

  if(status == 0)
  {
    RDCDEBUG("Real shader failed to compile, so skipping separable program and reflection.");
  }
  else if(sepProg == 0)
  {
    RDCERR(
        "Couldn't make separable program for shader via patching - functionality will be broken.");
  }
  else
  {
    prog = sepProg;
    MakeShaderReflection(gl.GetHookset(), type, sepProg, reflection, pointSizeUsed, clipDistanceUsed);

    vector<uint32_t> spirvwords;

    SPIRVCompilationSettings settings(SPIRVSourceLanguage::OpenGLGLSL,
                                      SPIRVShaderStage(ShaderIdx(type)));

    string s = CompileSPIRV(settings, sources, spirvwords);
    if(!spirvwords.empty())
      ParseSPIRV(&spirvwords.front(), spirvwords.size(), spirv);
    else
      disassembly = s;

    reflection.resourceId = id;
    reflection.entryPoint = "main";

    reflection.stage = MakeShaderStage(type);

    reflection.debugInfo.files.resize(1);
    reflection.debugInfo.files[0].filename = "main.glsl";
    reflection.debugInfo.files[0].contents = concatenated;
  }
}

#pragma region Shaders

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateShader(SerialiserType &ser, GLenum type, GLuint shader)
{
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_LOCAL(Shader, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = m_Real.glCreateShader(type);

    GLResource res = ShaderRes(GetCtx(), real);

    ResourceId liveId = GetResourceManager()->RegisterResource(res);

    m_Shaders[liveId].type = type;

    GetResourceManager()->AddLiveResource(Shader, res);

    AddResource(Shader, ResourceType::Shader, "Shader");
  }

  return true;
}

GLuint WrappedOpenGL::glCreateShader(GLenum type)
{
  GLuint real;
  SERIALISE_TIME_CALL(real = m_Real.glCreateShader(type));

  GLResource res = ShaderRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCreateShader(ser, type, real);

      chunk = scope.Get();
    }

    GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    RDCASSERT(record);

    record->AddChunk(chunk);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);

    m_Shaders[id].type = type;
  }

  return real;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glShaderSource(SerialiserType &ser, GLuint shaderHandle, GLsizei count,
                                             const GLchar *const *source, const GLint *length)
{
  SERIALISE_ELEMENT_LOCAL(shader, ShaderRes(GetCtx(), shaderHandle));

  // serialisation can't handle the length parameter neatly, so we compromise by serialising via a
  // vector
  std::vector<std::string> sources;

  if(ser.IsWriting())
  {
    sources.reserve(count);
    for(GLsizei c = 0; c < count; c++)
    {
      sources.push_back((length && length[c] > 0) ? std::string(source[c], source[c] + length[c])
                                                  : std::string(source[c]));
    }
  }

  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(sources);
  SERIALISE_ELEMENT_ARRAY(length, count);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<const char *> strs;
    for(size_t i = 0; i < sources.size(); i++)
      strs.push_back(sources[i].c_str());

    ResourceId liveId = GetResourceManager()->GetID(shader);

    m_Shaders[liveId].sources = sources;

    m_Real.glShaderSource(shader.name, (GLsizei)sources.size(), strs.data(), NULL);

    // if we've already disassembled this shader, undo all that.
    // Note this means we don't support compiling the same shader multiple times
    // attached to different programs, but that is *utterly crazy* and anyone
    // who tries to actually do that should be ashamed.
    // Doing this means we support the case of recompiling a shader different ways
    // and relinking a program before use, which is still moderately crazy and
    // so people who do that should be moderately ashamed.
    if(m_Shaders[liveId].prog)
    {
      m_Real.glDeleteProgram(m_Shaders[liveId].prog);
      m_Shaders[liveId].prog = 0;
      m_Shaders[liveId].spirv = SPVModule();
      m_Shaders[liveId].reflection = ShaderReflection();
    }

    AddResourceInitChunk(shader);
  }

  return true;
}

void WrappedOpenGL::glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                                   const GLint *length)
{
  SERIALISE_TIME_CALL(m_Real.glShaderSource(shader, count, string, length));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glShaderSource(ser, shader, count, string, length);

      record->AddChunk(scope.Get());
    }
  }
  else
  {
    ResourceId id = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));
    m_Shaders[id].sources.clear();
    m_Shaders[id].sources.reserve(count);

    for(GLsizei i = 0; i < count; i++)
      m_Shaders[id].sources.push_back(string[i]);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompileShader(SerialiserType &ser, GLuint shaderHandle)
{
  SERIALISE_ELEMENT_LOCAL(shader, ShaderRes(GetCtx(), shaderHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetID(shader);

    m_Real.glCompileShader(shader.name);

    m_Shaders[liveId].Compile(*this, GetResourceManager()->GetOriginalID(liveId), shader.name);

    AddResourceInitChunk(shader);
  }

  return true;
}

void WrappedOpenGL::glCompileShader(GLuint shader)
{
  m_Real.glCompileShader(shader);

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCompileShader(ser, shader);

      record->AddChunk(scope.Get());
    }
  }
  else
  {
    ResourceId id = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));
    m_Shaders[id].Compile(*this, id, shader);
  }
}

void WrappedOpenGL::glReleaseShaderCompiler()
{
  m_Real.glReleaseShaderCompiler();
}

void WrappedOpenGL::glDeleteShader(GLuint shader)
{
  m_Real.glDeleteShader(shader);

  GLResource res = ShaderRes(GetCtx(), shader);
  if(GetResourceManager()->HasCurrentResource(res))
  {
    if(GetResourceManager()->HasResourceRecord(res))
      GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
    GetResourceManager()->UnregisterResource(res);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glAttachShader(SerialiserType &ser, GLuint programHandle,
                                             GLuint shaderHandle)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT_LOCAL(shader, ShaderRes(GetCtx(), shaderHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveProgId = GetResourceManager()->GetID(program);
    ResourceId liveShadId = GetResourceManager()->GetID(shader);

    m_Programs[liveProgId].shaders.push_back(liveShadId);

    m_Real.glAttachShader(program.name, shader.name);

    AddResourceInitChunk(program);
    DerivedResource(program, GetResourceManager()->GetOriginalID(liveShadId));
  }

  return true;
}

void WrappedOpenGL::glAttachShader(GLuint program, GLuint shader)
{
  SERIALISE_TIME_CALL(m_Real.glAttachShader(program, shader));

  if(program && shader)
  {
    if(IsCaptureMode(m_State))
    {
      GLResourceRecord *progRecord =
          GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
      GLResourceRecord *shadRecord =
          GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
      RDCASSERT(progRecord && shadRecord);
      if(progRecord && shadRecord)
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glAttachShader(ser, program, shader);

        progRecord->AddParent(shadRecord);
        progRecord->AddChunk(scope.Get());
      }
    }
    else
    {
      ResourceId progid = GetResourceManager()->GetID(ProgramRes(GetCtx(), program));
      ResourceId shadid = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));
      m_Programs[progid].shaders.push_back(shadid);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDetachShader(SerialiserType &ser, GLuint programHandle,
                                             GLuint shaderHandle)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT_LOCAL(shader, ShaderRes(GetCtx(), shaderHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveProgId = GetResourceManager()->GetID(program);
    ResourceId liveShadId = GetResourceManager()->GetID(shader);

    // in order to be able to relink programs, we don't replay detaches. This should be valid as
    // it's legal to have a shader attached to multiple programs, so even if it's attached again
    // that doesn't affect the attach here.
    /*
    if(!m_Programs[liveProgId].linked)
    {
      for(auto it = m_Programs[liveProgId].shaders.begin();
          it != m_Programs[liveProgId].shaders.end(); ++it)
      {
        if(*it == liveShadId)
        {
          m_Programs[liveProgId].shaders.erase(it);
          break;
        }
      }
    }

    m_Real.glDetachShader(GetResourceManager()->GetLiveResource(progid).name,
                          GetResourceManager()->GetLiveResource(shadid).name);
    */
  }

  return true;
}

void WrappedOpenGL::glDetachShader(GLuint program, GLuint shader)
{
  SERIALISE_TIME_CALL(m_Real.glDetachShader(program, shader));

  if(program && shader)
  {
    // check that shader still exists, it might have been deleted. If it has, it's not too important
    // that we detach the shader (only important if the program will attach it elsewhere).
    if(IsCaptureMode(m_State) && GetResourceManager()->HasCurrentResource(ShaderRes(GetCtx(), shader)))
    {
      GLResourceRecord *progRecord =
          GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
      RDCASSERT(progRecord);
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glDetachShader(ser, program, shader);

        progRecord->AddChunk(scope.Get());
      }
    }
    else
    {
      ResourceId progid = GetResourceManager()->GetID(ProgramRes(GetCtx(), program));
      ResourceId shadid = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));

      if(!m_Programs[progid].linked)
      {
        for(auto it = m_Programs[progid].shaders.begin(); it != m_Programs[progid].shaders.end(); ++it)
        {
          if(*it == shadid)
          {
            m_Programs[progid].shaders.erase(it);
            break;
          }
        }
      }
    }
  }
}

#pragma endregion

#pragma region Programs

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateShaderProgramv(SerialiserType &ser, GLenum type, GLsizei count,
                                                     const GLchar *const *strings, GLuint program)
{
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(strings, count);
  SERIALISE_ELEMENT_LOCAL(Program, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<std::string> src;
    for(GLsizei i = 0; i < count; i++)
      src.push_back(strings[i]);

    GLuint real = m_Real.glCreateShaderProgramv(type, count, strings);
    // we want a separate program that we can mess about with for making overlays
    // and relink without having to worry about restoring the 'real' program state.
    GLuint sepprog = MakeSeparableShaderProgram(*this, type, src, NULL);

    GLResource res = ProgramRes(GetCtx(), real);

    ResourceId liveId = m_ResourceManager->RegisterResource(res);

    auto &progDetails = m_Programs[liveId];

    progDetails.linked = true;
    progDetails.shaders.push_back(liveId);
    progDetails.stageShaders[ShaderIdx(type)] = liveId;
    progDetails.shaderProgramUnlinkable = true;

    auto &shadDetails = m_Shaders[liveId];

    shadDetails.type = type;
    shadDetails.sources.swap(src);
    shadDetails.prog = sepprog;

    shadDetails.Compile(*this, Program, 0);

    GetResourceManager()->AddLiveResource(Program, res);

    AddResource(Program, ResourceType::StateObject, "Program");
  }

  return true;
}

GLuint WrappedOpenGL::glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
  GLuint real;
  SERIALISE_TIME_CALL(real = m_Real.glCreateShaderProgramv(type, count, strings));

  if(real == 0)
    return real;

  GLResource res = ProgramRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCreateShaderProgramv(ser, type, count, strings, real);

      chunk = scope.Get();
    }

    GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    RDCASSERT(record);

    // we always want to mark programs as dirty so we can serialise their
    // locations as initial state (and form a remapping table)
    GetResourceManager()->MarkDirtyResource(id);

    record->AddChunk(chunk);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);

    vector<string> src;
    for(GLsizei i = 0; i < count; i++)
      src.push_back(strings[i]);

    GLuint sepprog = MakeSeparableShaderProgram(*this, type, src, NULL);

    auto &progDetails = m_Programs[id];

    progDetails.linked = true;
    progDetails.shaders.push_back(id);
    progDetails.stageShaders[ShaderIdx(type)] = id;

    auto &shadDetails = m_Shaders[id];

    shadDetails.type = type;
    shadDetails.sources.swap(src);
    shadDetails.prog = sepprog;

    shadDetails.Compile(*this, id, 0);
  }

  return real;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateProgram(SerialiserType &ser, GLuint program)
{
  SERIALISE_ELEMENT_LOCAL(Program, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = m_Real.glCreateProgram();

    GLResource res = ProgramRes(GetCtx(), real);

    ResourceId liveId = m_ResourceManager->RegisterResource(res);

    m_Programs[liveId].linked = false;

    GetResourceManager()->AddLiveResource(Program, res);

    AddResource(Program, ResourceType::StateObject, "Program");
  }

  return true;
}

GLuint WrappedOpenGL::glCreateProgram()
{
  GLuint real;
  SERIALISE_TIME_CALL(real = m_Real.glCreateProgram());

  GLResource res = ProgramRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCreateProgram(ser, real);

      chunk = scope.Get();
    }

    GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    RDCASSERT(record);

    // we always want to mark programs as dirty so we can serialise their
    // locations as initial state (and form a remapping table)
    GetResourceManager()->MarkDirtyResource(id);

    record->AddChunk(chunk);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);

    m_Programs[id].linked = false;
  }

  return real;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glLinkProgram(SerialiserType &ser, GLuint programHandle)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId progid = GetResourceManager()->GetID(program);

    ProgramData &progDetails = m_Programs[progid];

    progDetails.linked = true;

    for(size_t s = 0; s < 6; s++)
    {
      for(size_t sh = 0; sh < progDetails.shaders.size(); sh++)
      {
        if(m_Shaders[progDetails.shaders[sh]].type == ShaderEnum(s))
          progDetails.stageShaders[s] = progDetails.shaders[sh];
      }
    }

    m_Real.glLinkProgram(program.name);

    AddResourceInitChunk(program);
  }

  return true;
}

void WrappedOpenGL::glLinkProgram(GLuint program)
{
  SERIALISE_TIME_CALL(m_Real.glLinkProgram(program));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glLinkProgram(ser, program);

      record->AddChunk(scope.Get());
    }
  }
  else
  {
    ResourceId progid = GetResourceManager()->GetID(ProgramRes(GetCtx(), program));

    ProgramData &progDetails = m_Programs[progid];

    progDetails.linked = true;

    for(size_t s = 0; s < 6; s++)
    {
      for(size_t sh = 0; sh < progDetails.shaders.size(); sh++)
      {
        if(m_Shaders[progDetails.shaders[sh]].type == ShaderEnum(s))
          progDetails.stageShaders[s] = progDetails.shaders[sh];
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUniformBlockBinding(SerialiserType &ser, GLuint programHandle,
                                                    GLuint uniformBlockIndex,
                                                    GLuint uniformBlockBinding)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(uniformBlockIndex);
  SERIALISE_ELEMENT(uniformBlockBinding);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glUniformBlockBinding(program.name, uniformBlockIndex, uniformBlockBinding);
  }

  return true;
}

void WrappedOpenGL::glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex,
                                          GLuint uniformBlockBinding)
{
  SERIALISE_TIME_CALL(m_Real.glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glUniformBlockBinding(ser, program, uniformBlockIndex, uniformBlockBinding);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glShaderStorageBlockBinding(SerialiserType &ser, GLuint programHandle,
                                                          GLuint storageBlockIndex,
                                                          GLuint storageBlockBinding)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(storageBlockIndex);
  SERIALISE_ELEMENT(storageBlockBinding);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glShaderStorageBlockBinding(program.name, storageBlockIndex, storageBlockBinding);
  }

  return true;
}

void WrappedOpenGL::glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex,
                                                GLuint storageBlockBinding)
{
  SERIALISE_TIME_CALL(
      m_Real.glShaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glShaderStorageBlockBinding(ser, program, storageBlockIndex, storageBlockBinding);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindAttribLocation(SerialiserType &ser, GLuint programHandle,
                                                   GLuint index, const GLchar *name)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindAttribLocation(program.name, index, name);
  }

  return true;
}

void WrappedOpenGL::glBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
  SERIALISE_TIME_CALL(m_Real.glBindAttribLocation(program, index, name));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindAttribLocation(ser, program, index, name);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindFragDataLocation(SerialiserType &ser, GLuint programHandle,
                                                     GLuint color, const GLchar *name)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(color);
  SERIALISE_ELEMENT(name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindFragDataLocation(program.name, color, name);
  }

  return true;
}

void WrappedOpenGL::glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name)
{
  SERIALISE_TIME_CALL(m_Real.glBindFragDataLocation(program, color, name));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindFragDataLocation(ser, program, color, name);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUniformSubroutinesuiv(SerialiserType &ser, GLenum shadertype,
                                                      GLsizei count, const GLuint *indices)
{
  SERIALISE_ELEMENT(shadertype);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(indices, count);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glUniformSubroutinesuiv(shadertype, count, indices);

    APIProps.ShaderLinkage = true;
  }

  return true;
}

void WrappedOpenGL::glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices)
{
  SERIALISE_TIME_CALL(m_Real.glUniformSubroutinesuiv(shadertype, count, indices));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glUniformSubroutinesuiv(ser, shadertype, count, indices);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindFragDataLocationIndexed(SerialiserType &ser,
                                                            GLuint programHandle, GLuint colorNumber,
                                                            GLuint index, const GLchar *name)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(colorNumber);
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindFragDataLocationIndexed(program.name, colorNumber, index, name);
  }

  return true;
}

void WrappedOpenGL::glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index,
                                                  const GLchar *name)
{
  SERIALISE_TIME_CALL(m_Real.glBindFragDataLocationIndexed(program, colorNumber, index, name));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindFragDataLocationIndexed(ser, program, colorNumber, index, name);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTransformFeedbackVaryings(SerialiserType &ser, GLuint programHandle,
                                                          GLsizei count,
                                                          const GLchar *const *varyings,
                                                          GLenum bufferMode)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(varyings, count);
  SERIALISE_ELEMENT(bufferMode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glTransformFeedbackVaryings(program.name, count, varyings, bufferMode);
  }

  return true;
}

void WrappedOpenGL::glTransformFeedbackVaryings(GLuint program, GLsizei count,
                                                const GLchar *const *varyings, GLenum bufferMode)
{
  SERIALISE_TIME_CALL(m_Real.glTransformFeedbackVaryings(program, count, varyings, bufferMode));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glTransformFeedbackVaryings(ser, program, count, varyings, bufferMode);

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glProgramParameteri(SerialiserType &ser, GLuint programHandle,
                                                  GLenum pname, GLint value)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(value);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glProgramParameteri(program.name, pname, value);
  }

  return true;
}

void WrappedOpenGL::glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
  SERIALISE_TIME_CALL(m_Real.glProgramParameteri(program, pname, value));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glProgramParameteri(ser, program, pname, value);

      record->AddChunk(scope.Get());
    }
  }
}

void WrappedOpenGL::glDeleteProgram(GLuint program)
{
  m_Real.glDeleteProgram(program);

  GLResource res = ProgramRes(GetCtx(), program);
  if(GetResourceManager()->HasCurrentResource(res))
  {
    GetResourceManager()->MarkCleanResource(res);
    if(GetResourceManager()->HasResourceRecord(res))
      GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
    GetResourceManager()->UnregisterResource(res);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUseProgram(SerialiserType &ser, GLuint programHandle)
{
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glUseProgram(program.name);
  }

  return true;
}

void WrappedOpenGL::glUseProgram(GLuint program)
{
  SERIALISE_TIME_CALL(m_Real.glUseProgram(program));

  GetCtxData().m_Program = program;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glUseProgram(ser, program);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ProgramRes(GetCtx(), program), eFrameRef_Read);
  }
}

void WrappedOpenGL::glValidateProgram(GLuint program)
{
  m_Real.glValidateProgram(program);
}

void WrappedOpenGL::glValidateProgramPipeline(GLuint pipeline)
{
  m_Real.glValidateProgramPipeline(pipeline);
}

void WrappedOpenGL::glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat,
                                   const void *binary, GLsizei length)
{
  // deliberately don't forward on this call when writing, since we want to coax the app into
  // providing non-binary shaders.
  if(IsReplayMode(m_State))
  {
    m_Real.glShaderBinary(count, shaders, binaryformat, binary, length);
  }
}

void WrappedOpenGL::glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary,
                                    GLsizei length)
{
  // deliberately don't forward on this call when writing, since we want to coax the app into
  // providing non-binary shaders.
  if(IsReplayMode(m_State))
  {
    m_Real.glProgramBinary(program, binaryFormat, binary, length);
  }
}

#pragma endregion

#pragma region Program Pipelines

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUseProgramStages(SerialiserType &ser, GLuint pipelineHandle,
                                                 GLbitfield stages, GLuint programHandle)
{
  SERIALISE_ELEMENT_LOCAL(pipeline, ProgramPipeRes(GetCtx(), pipelineHandle));
  SERIALISE_ELEMENT_TYPED(GLshaderbitfield, stages);
  SERIALISE_ELEMENT_LOCAL(program, ProgramRes(GetCtx(), programHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(program.name)
    {
      ResourceId livePipeId = GetResourceManager()->GetID(pipeline);
      ResourceId liveProgId = GetResourceManager()->GetID(program);

      PipelineData &pipeDetails = m_Pipelines[livePipeId];
      ProgramData &progDetails = m_Programs[liveProgId];

      for(size_t s = 0; s < 6; s++)
      {
        if(stages & ShaderBit(s))
        {
          for(size_t sh = 0; sh < progDetails.shaders.size(); sh++)
          {
            if(m_Shaders[progDetails.shaders[sh]].type == ShaderEnum(s))
            {
              pipeDetails.stagePrograms[s] = liveProgId;
              pipeDetails.stageShaders[s] = progDetails.shaders[sh];
              break;
            }
          }
        }
      }

      m_Real.glUseProgramStages(pipeline.name, stages, program.name);
    }
    else
    {
      ResourceId livePipeId = GetResourceManager()->GetID(pipeline);
      PipelineData &pipeDetails = m_Pipelines[livePipeId];

      for(size_t s = 0; s < 6; s++)
      {
        if(stages & ShaderBit(s))
        {
          pipeDetails.stagePrograms[s] = ResourceId();
          pipeDetails.stageShaders[s] = ResourceId();
        }
      }

      m_Real.glUseProgramStages(pipeline.name, stages, 0);
    }
  }

  return true;
}

void WrappedOpenGL::glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
  SERIALISE_TIME_CALL(m_Real.glUseProgramStages(pipeline, stages, program));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ProgramPipeRes(GetCtx(), pipeline));

    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 pipeline);

    if(record == NULL)
      return;

    if(program)
    {
      GLResourceRecord *progrecord =
          GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
      RDCASSERT(progrecord);

      if(progrecord)
        record->AddParent(progrecord);
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glUseProgramStages(ser, pipeline, stages, program);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
    {
      m_ContextRecord->AddChunk(chunk);
    }
    else
    {
      record->AddChunk(chunk);
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
  else
  {
    if(program)
    {
      ResourceId pipeID = GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), pipeline));
      ResourceId progID = GetResourceManager()->GetID(ProgramRes(GetCtx(), program));

      PipelineData &pipeDetails = m_Pipelines[pipeID];
      ProgramData &progDetails = m_Programs[progID];

      for(size_t s = 0; s < 6; s++)
      {
        if(stages & ShaderBit(s))
        {
          for(size_t sh = 0; sh < progDetails.shaders.size(); sh++)
          {
            if(m_Shaders[progDetails.shaders[sh]].type == ShaderEnum(s))
            {
              pipeDetails.stagePrograms[s] = progID;
              pipeDetails.stageShaders[s] = progDetails.shaders[sh];
              break;
            }
          }
        }
      }
    }
    else
    {
      ResourceId pipeID = GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), pipeline));
      PipelineData &pipeDetails = m_Pipelines[pipeID];

      for(size_t s = 0; s < 6; s++)
      {
        if(stages & ShaderBit(s))
        {
          pipeDetails.stagePrograms[s] = ResourceId();
          pipeDetails.stageShaders[s] = ResourceId();
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenProgramPipelines(SerialiserType &ser, GLsizei n, GLuint *pipelines)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(pipeline, GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), *pipelines)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenProgramPipelines(1, &real);
    m_Real.glBindProgramPipeline(real);
    m_Real.glBindProgramPipeline(0);

    GLResource res = ProgramPipeRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(pipeline, res);

    AddResource(pipeline, ResourceType::StateObject, "Pipeline");
  }

  return true;
}

void WrappedOpenGL::glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
  SERIALISE_TIME_CALL(m_Real.glGenProgramPipelines(n, pipelines));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ProgramPipeRes(GetCtx(), pipelines[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenProgramPipelines(ser, 1, pipelines + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateProgramPipelines(SerialiserType &ser, GLsizei n,
                                                       GLuint *pipelines)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(pipeline, GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), *pipelines)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateProgramPipelines(1, &real);

    GLResource res = ProgramPipeRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(pipeline, res);

    AddResource(pipeline, ResourceType::StateObject, "Pipeline");
  }

  return true;
}

void WrappedOpenGL::glCreateProgramPipelines(GLsizei n, GLuint *pipelines)
{
  SERIALISE_TIME_CALL(m_Real.glCreateProgramPipelines(n, pipelines));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ProgramPipeRes(GetCtx(), pipelines[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateProgramPipelines(ser, 1, pipelines + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindProgramPipeline(SerialiserType &ser, GLuint pipelineHandle)
{
  SERIALISE_ELEMENT_LOCAL(pipeline, ProgramPipeRes(GetCtx(), pipelineHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindProgramPipeline(pipeline.name);
  }

  return true;
}

void WrappedOpenGL::glBindProgramPipeline(GLuint pipeline)
{
  SERIALISE_TIME_CALL(m_Real.glBindProgramPipeline(pipeline));

  GetCtxData().m_ProgramPipeline = pipeline;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindProgramPipeline(ser, pipeline);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ProgramPipeRes(GetCtx(), pipeline),
                                                      eFrameRef_Read);
  }
}

void WrappedOpenGL::glActiveShaderProgram(GLuint pipeline, GLuint program)
{
  m_Real.glActiveShaderProgram(pipeline, program);
}

GLuint WrappedOpenGL::GetUniformProgram()
{
  ContextData &cd = GetCtxData();

  // program gets first dibs, if one is bound then that's where glUniform* calls go.
  if(cd.m_Program != 0)
  {
    return cd.m_Program;
  }
  else if(cd.m_ProgramPipeline != 0)
  {
    GLuint ret = 0;

    // otherwise, query the active program for the pipeline (could cache this above in
    // glActiveShaderProgram)
    // we do this query every time instead of caching the result, since I think it's unlikely that
    // we'll ever hit this path (most people using separable programs will use the glProgramUniform*
    // interface).
    // That way we don't pay the cost of a potentially expensive query unless we really need it.
    m_Real.glGetProgramPipelineiv(cd.m_ProgramPipeline, eGL_ACTIVE_PROGRAM, (GLint *)&ret);

    return ret;
  }

  return 0;
}

void WrappedOpenGL::glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ProgramPipeRes(GetCtx(), pipelines[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteProgramPipelines(n, pipelines);
}

#pragma endregion

#pragma region ARB_shading_language_include

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompileShaderIncludeARB(SerialiserType &ser, GLuint shaderHandle,
                                                        GLsizei count, const GLchar *const *path,
                                                        const GLint *length)
{
  SERIALISE_ELEMENT_LOCAL(shader, ShaderRes(GetCtx(), shaderHandle));

  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(path, count);
  SERIALISE_ELEMENT_ARRAY(length, count);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetID(shader);

    auto &shadDetails = m_Shaders[liveId];

    shadDetails.includepaths.clear();
    shadDetails.includepaths.reserve(count);

    for(int32_t i = 0; i < count; i++)
      shadDetails.includepaths.push_back(path[i]);

    m_Real.glCompileShaderIncludeARB(shader.name, count, path, NULL);

    shadDetails.Compile(*this, GetResourceManager()->GetOriginalID(liveId), shader.name);

    AddResourceInitChunk(shader);
  }

  return true;
}

void WrappedOpenGL::glCompileShaderIncludeARB(GLuint shader, GLsizei count,
                                              const GLchar *const *path, const GLint *length)
{
  SERIALISE_TIME_CALL(m_Real.glCompileShaderIncludeARB(shader, count, path, length));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCompileShaderIncludeARB(ser, shader, count, path, length);

      record->AddChunk(scope.Get());
    }
  }
  else
  {
    ResourceId id = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));

    auto &shadDetails = m_Shaders[id];

    shadDetails.includepaths.clear();
    shadDetails.includepaths.reserve(count);

    for(int32_t i = 0; i < count; i++)
      shadDetails.includepaths.push_back(path[i]);

    shadDetails.Compile(*this, id, shader);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedStringARB(SerialiserType &ser, GLenum type, GLint namelen,
                                               const GLchar *nameStr, GLint stringlen,
                                               const GLchar *valStr)
{
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(namelen);
  SERIALISE_ELEMENT_LOCAL(
      name, nameStr ? std::string(nameStr, nameStr + (namelen > 0 ? namelen : strlen(nameStr))) : "");
  SERIALISE_ELEMENT(stringlen);
  SERIALISE_ELEMENT_LOCAL(
      value,
      valStr ? std::string(valStr, valStr + (stringlen > 0 ? stringlen : strlen(valStr))) : "");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glNamedStringARB(type, (GLint)name.length(), name.c_str(), (GLint)value.length(),
                            value.c_str());
  }

  return true;
}

void WrappedOpenGL::glNamedStringARB(GLenum type, GLint namelen, const GLchar *name,
                                     GLint stringlen, const GLchar *str)
{
  SERIALISE_TIME_CALL(m_Real.glNamedStringARB(type, namelen, name, stringlen, str));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedStringARB(ser, type, namelen, name, stringlen, str);

    // if a program repeatedly created/destroyed named strings this will fill up with useless
    // strings,
    // but chances are that won't be the case - a few will be created at init time and that's it
    m_DeviceRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDeleteNamedStringARB(SerialiserType &ser, GLint namelen,
                                                     const GLchar *nameStr)
{
  SERIALISE_ELEMENT(namelen);
  SERIALISE_ELEMENT_LOCAL(
      name, nameStr ? std::string(nameStr, nameStr + (namelen > 0 ? namelen : strlen(nameStr))) : "");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glDeleteNamedStringARB((GLint)name.length(), name.c_str());
  }

  return true;
}

void WrappedOpenGL::glDeleteNamedStringARB(GLint namelen, const GLchar *name)
{
  SERIALISE_TIME_CALL(m_Real.glDeleteNamedStringARB(namelen, name));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDeleteNamedStringARB(ser, namelen, name);

    // if a program repeatedly created/destroyed named strings this will fill up with useless
    // strings,
    // but chances are that won't be the case - a few will be created at init time and that's it
    m_DeviceRecord->AddChunk(scope.Get());
  }
}

#pragma endregion

INSTANTIATE_FUNCTION_SERIALISED(void, glCreateShader, GLenum type, GLuint shader);
INSTANTIATE_FUNCTION_SERIALISED(void, glShaderSource, GLuint shaderHandle, GLsizei count,
                                const GLchar *const *source, const GLint *length);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompileShader, GLuint shaderHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glAttachShader, GLuint programHandle, GLuint shaderHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glDetachShader, GLuint programHandle, GLuint shaderHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateShaderProgramv, GLenum type, GLsizei count,
                                const GLchar *const *strings, GLuint program);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateProgram, GLuint program);
INSTANTIATE_FUNCTION_SERIALISED(void, glLinkProgram, GLuint programHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glUniformBlockBinding, GLuint programHandle,
                                GLuint uniformBlockIndex, GLuint uniformBlockBinding);
INSTANTIATE_FUNCTION_SERIALISED(void, glShaderStorageBlockBinding, GLuint programHandle,
                                GLuint storageBlockIndex, GLuint storageBlockBinding);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindAttribLocation, GLuint programHandle, GLuint index,
                                const GLchar *name);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindFragDataLocation, GLuint programHandle, GLuint color,
                                const GLchar *name);
INSTANTIATE_FUNCTION_SERIALISED(void, glUniformSubroutinesuiv, GLenum shadertype, GLsizei count,
                                const GLuint *indices);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindFragDataLocationIndexed, GLuint programHandle,
                                GLuint colorNumber, GLuint index, const GLchar *name);
INSTANTIATE_FUNCTION_SERIALISED(void, glTransformFeedbackVaryings, GLuint programHandle,
                                GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
INSTANTIATE_FUNCTION_SERIALISED(void, glProgramParameteri, GLuint programHandle, GLenum pname,
                                GLint value);
INSTANTIATE_FUNCTION_SERIALISED(void, glUseProgram, GLuint programHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glUseProgramStages, GLuint pipelineHandle, GLbitfield stages,
                                GLuint programHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenProgramPipelines, GLsizei n, GLuint *pipelines);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateProgramPipelines, GLsizei n, GLuint *pipelines);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindProgramPipeline, GLuint pipelineHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompileShaderIncludeARB, GLuint shaderHandle, GLsizei count,
                                const GLchar *const *path, const GLint *length);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedStringARB, GLenum type, GLint namelen,
                                const GLchar *nameStr, GLint stringlen, const GLchar *valStr);
INSTANTIATE_FUNCTION_SERIALISED(void, glDeleteNamedStringARB, GLint namelen, const GLchar *nameStr);
