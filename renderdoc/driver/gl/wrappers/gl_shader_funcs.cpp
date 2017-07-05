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

#include "../gl_driver.h"
#include "../gl_shader_refl.h"
#include "common/common.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "serialise/string_utils.h"

void WrappedOpenGL::ShaderData::Compile(WrappedOpenGL &gl, ResourceId id)
{
  bool pointSizeUsed = false, clipDistanceUsed = false;
  if(type == eGL_VERTEX_SHADER)
    CheckVertexOutputUses(sources, pointSizeUsed, clipDistanceUsed);

  {
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

    create_array_init(reflection.RawBytes, concatenated.size(), (byte *)concatenated.c_str());
  }

  GLuint sepProg = prog;

  if(sepProg == 0)
    sepProg = MakeSeparableShaderProgram(gl, type, sources, NULL);

  if(sepProg == 0)
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

    reflection.ID = id;
    reflection.EntryPoint = "main";

    // TODO sort these so that the first file contains the entry point
    create_array_uninit(reflection.DebugInfo.files, sources.size());
    for(size_t i = 0; i < sources.size(); i++)
    {
      reflection.DebugInfo.files[i].first = StringFormat::Fmt("source%u.glsl", (uint32_t)i);
      reflection.DebugInfo.files[i].second = sources[i];
    }
  }
}

#pragma region Shaders

bool WrappedOpenGL::Serialise_glCreateShader(GLuint shader, GLenum type)
{
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));

  if(m_State == READING)
  {
    GLuint real = m_Real.glCreateShader(Type);

    GLResource res = ShaderRes(GetCtx(), real);

    ResourceId liveId = GetResourceManager()->RegisterResource(res);

    m_Shaders[liveId].type = Type;

    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

GLuint WrappedOpenGL::glCreateShader(GLenum type)
{
  GLuint real = m_Real.glCreateShader(type);

  GLResource res = ShaderRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(m_State >= WRITING)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(CREATE_SHADER);
      Serialise_glCreateShader(real, type);

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

bool WrappedOpenGL::Serialise_glShaderSource(GLuint shader, GLsizei count,
                                             const GLchar *const *source, const GLint *length)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));
  SERIALISE_ELEMENT(uint32_t, Count, count);

  vector<string> srcs;

  for(uint32_t i = 0; i < Count; i++)
  {
    string s;
    if(source && source[i])
      s = (length && length[i] > 0) ? string(source[i], source[i] + length[i]) : string(source[i]);

    m_pSerialiser->SerialiseString("source", s);

    if(m_State == READING)
      srcs.push_back(s);
  }

  if(m_State == READING)
  {
    size_t numStrings = srcs.size();

    const char **strings = new const char *[numStrings];
    for(size_t i = 0; i < numStrings; i++)
      strings[i] = srcs[i].c_str();

    ResourceId liveId = GetResourceManager()->GetLiveID(id);

    m_Shaders[liveId].sources.clear();
    m_Shaders[liveId].sources.reserve(Count);

    for(uint32_t i = 0; i < Count; i++)
      m_Shaders[liveId].sources.push_back(strings[i]);

    m_Real.glShaderSource(GetResourceManager()->GetLiveResource(id).name, Count, strings, NULL);

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

    delete[] strings;
  }

  return true;
}

void WrappedOpenGL::glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                                   const GLint *length)
{
  m_Real.glShaderSource(shader, count, string, length);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(SHADERSOURCE);
      Serialise_glShaderSource(shader, count, string, length);

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

bool WrappedOpenGL::Serialise_glCompileShader(GLuint shader)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));

  if(m_State == READING)
  {
    ResourceId liveId = GetResourceManager()->GetLiveID(id);

    m_Shaders[liveId].Compile(*this, id);

    m_Real.glCompileShader(GetResourceManager()->GetLiveResource(id).name);
  }

  return true;
}

void WrappedOpenGL::glCompileShader(GLuint shader)
{
  m_Real.glCompileShader(shader);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(COMPILESHADER);
      Serialise_glCompileShader(shader);

      record->AddChunk(scope.Get());
    }
  }
  else
  {
    ResourceId id = GetResourceManager()->GetID(ShaderRes(GetCtx(), shader));
    m_Shaders[id].Compile(*this, id);
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

bool WrappedOpenGL::Serialise_glAttachShader(GLuint program, GLuint shader)
{
  SERIALISE_ELEMENT(ResourceId, progid, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(ResourceId, shadid, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));

  if(m_State == READING)
  {
    ResourceId liveProgId = GetResourceManager()->GetLiveID(progid);
    ResourceId liveShadId = GetResourceManager()->GetLiveID(shadid);

    m_Programs[liveProgId].shaders.push_back(liveShadId);

    m_Real.glAttachShader(GetResourceManager()->GetLiveResource(progid).name,
                          GetResourceManager()->GetLiveResource(shadid).name);
  }

  return true;
}

void WrappedOpenGL::glAttachShader(GLuint program, GLuint shader)
{
  m_Real.glAttachShader(program, shader);

  if(m_State >= WRITING && program != 0 && shader != 0)
  {
    GLResourceRecord *progRecord =
        GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    GLResourceRecord *shadRecord =
        GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERT(progRecord && shadRecord);
    if(progRecord && shadRecord)
    {
      SCOPED_SERIALISE_CONTEXT(ATTACHSHADER);
      Serialise_glAttachShader(program, shader);

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

bool WrappedOpenGL::Serialise_glDetachShader(GLuint program, GLuint shader)
{
  SERIALISE_ELEMENT(ResourceId, progid, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(ResourceId, shadid, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));

  if(m_State == READING)
  {
    ResourceId liveProgId = GetResourceManager()->GetLiveID(progid);
    ResourceId liveShadId = GetResourceManager()->GetLiveID(shadid);

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
  m_Real.glDetachShader(program, shader);

  // check that shader still exists, it might have been deleted. If it has, it's not too important
  // that we detach the shader (only important if the program will attach it elsewhere).
  if(m_State >= WRITING && program != 0 && shader != 0 &&
     GetResourceManager()->HasCurrentResource(ShaderRes(GetCtx(), shader)))
  {
    GLResourceRecord *progRecord =
        GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERT(progRecord);
    {
      SCOPED_SERIALISE_CONTEXT(DETACHSHADER);
      Serialise_glDetachShader(program, shader);

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

#pragma endregion

#pragma region Programs

bool WrappedOpenGL::Serialise_glCreateShaderProgramv(GLuint program, GLenum type, GLsizei count,
                                                     const GLchar *const *strings)
{
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(int32_t, Count, count);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));

  vector<string> src;

  for(int32_t i = 0; i < Count; i++)
  {
    string s;
    if(m_State >= WRITING)
      s = strings[i];
    m_pSerialiser->SerialiseString("Source", s);
    if(m_State < WRITING)
      src.push_back(s);
  }

  if(m_State == READING)
  {
    char **sources = new char *[Count];

    for(int32_t i = 0; i < Count; i++)
      sources[i] = &src[i][0];

    GLuint real = m_Real.glCreateShaderProgramv(Type, Count, sources);
    // we want a separate program that we can mess about with for making overlays
    // and relink without having to worry about restoring the 'real' program state.
    GLuint sepprog = MakeSeparableShaderProgram(*this, Type, src, NULL);

    delete[] sources;

    GLResource res = ProgramRes(GetCtx(), real);

    ResourceId liveId = m_ResourceManager->RegisterResource(res);

    auto &progDetails = m_Programs[liveId];

    progDetails.linked = true;
    progDetails.shaders.push_back(liveId);
    progDetails.stageShaders[ShaderIdx(Type)] = liveId;
    progDetails.shaderProgramUnlinkable = true;

    auto &shadDetails = m_Shaders[liveId];

    shadDetails.type = Type;
    shadDetails.sources.swap(src);
    shadDetails.prog = sepprog;

    shadDetails.Compile(*this, id);

    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

GLuint WrappedOpenGL::glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
  GLuint real = m_Real.glCreateShaderProgramv(type, count, strings);

  if(real == 0)
    return real;

  GLResource res = ProgramRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(m_State >= WRITING)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(CREATE_SHADERPROGRAM);
      Serialise_glCreateShaderProgramv(real, type, count, strings);

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

    shadDetails.Compile(*this, id);
  }

  return real;
}

bool WrappedOpenGL::Serialise_glCreateProgram(GLuint program)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));

  if(m_State == READING)
  {
    GLuint real = m_Real.glCreateProgram();

    GLResource res = ProgramRes(GetCtx(), real);

    ResourceId liveId = m_ResourceManager->RegisterResource(res);

    m_Programs[liveId].linked = false;

    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

GLuint WrappedOpenGL::glCreateProgram()
{
  GLuint real = m_Real.glCreateProgram();

  GLResource res = ProgramRes(GetCtx(), real);
  ResourceId id = GetResourceManager()->RegisterResource(res);

  if(m_State >= WRITING)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(CREATE_PROGRAM);
      Serialise_glCreateProgram(real);

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

bool WrappedOpenGL::Serialise_glLinkProgram(GLuint program)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));

  if(m_State == READING)
  {
    ResourceId progid = GetResourceManager()->GetLiveID(id);

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

    m_Real.glLinkProgram(GetResourceManager()->GetLiveResource(id).name);
  }

  return true;
}

void WrappedOpenGL::glLinkProgram(GLuint program)
{
  m_Real.glLinkProgram(program);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(LINKPROGRAM);
      Serialise_glLinkProgram(program);

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

bool WrappedOpenGL::Serialise_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex,
                                                    GLuint uniformBlockBinding)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, index, uniformBlockIndex);
  SERIALISE_ELEMENT(uint32_t, binding, uniformBlockBinding);

  if(m_State == READING)
  {
    m_Real.glUniformBlockBinding(GetResourceManager()->GetLiveResource(id).name, index, binding);
  }

  return true;
}

void WrappedOpenGL::glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex,
                                          GLuint uniformBlockBinding)
{
  m_Real.glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(UNIFORM_BLOCKBIND);
      Serialise_glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex,
                                                          GLuint storageBlockBinding)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, index, storageBlockIndex);
  SERIALISE_ELEMENT(uint32_t, binding, storageBlockBinding);

  if(m_State == READING)
  {
    m_Real.glShaderStorageBlockBinding(GetResourceManager()->GetLiveResource(id).name, index,
                                       binding);
  }

  return true;
}

void WrappedOpenGL::glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex,
                                                GLuint storageBlockBinding)
{
  m_Real.glShaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(STORAGE_BLOCKBIND);
      Serialise_glShaderStorageBlockBinding(program, storageBlockIndex, storageBlockBinding);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glBindAttribLocation(GLuint program, GLuint index, const GLchar *name_)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, idx, index);

  string name = name_ ? name_ : "";
  m_pSerialiser->Serialise("Name", name);

  if(m_State == READING)
  {
    m_Real.glBindAttribLocation(GetResourceManager()->GetLiveResource(id).name, idx, name.c_str());
  }

  return true;
}

void WrappedOpenGL::glBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
  m_Real.glBindAttribLocation(program, index, name);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(BINDATTRIB_LOCATION);
      Serialise_glBindAttribLocation(program, index, name);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name_)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, col, color);

  string name = name_ ? name_ : "";
  m_pSerialiser->Serialise("Name", name);

  if(m_State == READING)
  {
    m_Real.glBindFragDataLocation(GetResourceManager()->GetLiveResource(id).name, col, name.c_str());
  }

  return true;
}

void WrappedOpenGL::glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name)
{
  m_Real.glBindFragDataLocation(program, color, name);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(BINDFRAGDATA_LOCATION);
      Serialise_glBindFragDataLocation(program, color, name);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glUniformSubroutinesuiv(GLenum shadertype, GLsizei count,
                                                      const GLuint *indices)
{
  SERIALISE_ELEMENT(GLenum, sh, shadertype);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT_ARR(uint32_t, Idxs, indices, Count);

  if(m_State <= EXECUTING)
    m_Real.glUniformSubroutinesuiv(sh, Count, Idxs);

  SAFE_DELETE_ARRAY(Idxs);

  return true;
}

void WrappedOpenGL::glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices)
{
  m_Real.glUniformSubroutinesuiv(shadertype, count, indices);

  if(m_State >= WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(UNIFORM_SUBROUTINE);
    Serialise_glUniformSubroutinesuiv(shadertype, count, indices);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber,
                                                            GLuint index, const GLchar *name_)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, colNum, colorNumber);
  SERIALISE_ELEMENT(uint32_t, idx, index);

  string name = name_ ? name_ : "";
  m_pSerialiser->Serialise("Name", name);

  if(m_State == READING)
  {
    m_Real.glBindFragDataLocationIndexed(GetResourceManager()->GetLiveResource(id).name, colNum,
                                         idx, name.c_str());
  }

  return true;
}

void WrappedOpenGL::glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index,
                                                  const GLchar *name)
{
  m_Real.glBindFragDataLocationIndexed(program, colorNumber, index, name);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(BINDFRAGDATA_LOCATION_INDEXED);
      Serialise_glBindFragDataLocationIndexed(program, colorNumber, index, name);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glTransformFeedbackVaryings(GLuint program, GLsizei count,
                                                          const GLchar *const *varyings,
                                                          GLenum bufferMode)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(GLenum, Mode, bufferMode);

  string *vars = m_State >= WRITING ? NULL : new string[Count];
  char **varstrs = m_State >= WRITING ? NULL : new char *[Count];

  for(uint32_t c = 0; c < Count; c++)
  {
    string v = varyings && varyings[c] ? varyings[c] : "";
    m_pSerialiser->Serialise("Varying", v);
    if(vars)
    {
      vars[c] = v;
      varstrs[c] = (char *)vars[c].c_str();
    }
  }

  if(m_State == READING)
  {
    m_Real.glTransformFeedbackVaryings(GetResourceManager()->GetLiveResource(id).name, Count,
                                       varstrs, Mode);
  }

  SAFE_DELETE_ARRAY(vars);
  SAFE_DELETE_ARRAY(varstrs);

  return true;
}

void WrappedOpenGL::glTransformFeedbackVaryings(GLuint program, GLsizei count,
                                                const GLchar *const *varyings, GLenum bufferMode)
{
  m_Real.glTransformFeedbackVaryings(program, count, varyings, bufferMode);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(FEEDBACK_VARYINGS);
      Serialise_glTransformFeedbackVaryings(program, count, varyings, bufferMode);

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedOpenGL::Serialise_glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(int32_t, Value, value);

  if(m_State == READING)
  {
    m_Real.glProgramParameteri(GetResourceManager()->GetLiveResource(id).name, PName, Value);
  }

  return true;
}

void WrappedOpenGL::glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
  m_Real.glProgramParameteri(program, pname, value);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 program);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(PROGRAMPARAMETER);
      Serialise_glProgramParameteri(program, pname, value);

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

bool WrappedOpenGL::Serialise_glUseProgram(GLuint program)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));

  if(m_State <= EXECUTING)
  {
    if(id == ResourceId())
      m_Real.glUseProgram(0);
    else
      m_Real.glUseProgram(GetResourceManager()->GetLiveResource(id).name);
  }

  return true;
}

void WrappedOpenGL::glUseProgram(GLuint program)
{
  m_Real.glUseProgram(program);

  GetCtxData().m_Program = program;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(USEPROGRAM);
    Serialise_glUseProgram(program);

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
  if(m_State < WRITING)
  {
    m_Real.glShaderBinary(count, shaders, binaryformat, binary, length);
  }
}

void WrappedOpenGL::glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary,
                                    GLsizei length)
{
  // deliberately don't forward on this call when writing, since we want to coax the app into
  // providing non-binary shaders.
  if(m_State < WRITING)
  {
    m_Real.glProgramBinary(program, binaryFormat, binary, length);
  }
}

#pragma endregion

#pragma region Program Pipelines

static const uint64_t marker_glUseProgramStages_hack = 0xffbbcc0014151617ULL;

bool WrappedOpenGL::Serialise_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
  if(GetLogVersion() >= 0x000011)
  {
    // this marker value is used below to identify where the serialised data sits.
    SERIALISE_ELEMENT(uint64_t, marker, marker_glUseProgramStages_hack);
  }
  SERIALISE_ELEMENT(ResourceId, pipe,
                    GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), pipeline)));
  SERIALISE_ELEMENT(uint32_t, Stages, stages);
  SERIALISE_ELEMENT(
      ResourceId, prog,
      (program ? GetResourceManager()->GetID(ProgramRes(GetCtx(), program)) : ResourceId()));

  if(m_State < WRITING)
  {
    if(prog != ResourceId())
    {
      ResourceId livePipeId = GetResourceManager()->GetLiveID(pipe);
      ResourceId liveProgId = GetResourceManager()->GetLiveID(prog);

      PipelineData &pipeDetails = m_Pipelines[livePipeId];
      ProgramData &progDetails = m_Programs[liveProgId];

      for(size_t s = 0; s < 6; s++)
      {
        if(Stages & ShaderBit(s))
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

      m_Real.glUseProgramStages(GetResourceManager()->GetLiveResource(pipe).name, Stages,
                                GetResourceManager()->GetLiveResource(prog).name);
    }
    else
    {
      ResourceId livePipeId = GetResourceManager()->GetLiveID(pipe);
      PipelineData &pipeDetails = m_Pipelines[livePipeId];

      for(size_t s = 0; s < 6; s++)
      {
        if(Stages & ShaderBit(s))
        {
          pipeDetails.stagePrograms[s] = ResourceId();
          pipeDetails.stageShaders[s] = ResourceId();
        }
      }

      m_Real.glUseProgramStages(GetResourceManager()->GetLiveResource(pipe).name, Stages, 0);
    }
  }

  return true;
}

void WrappedOpenGL::glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
  m_Real.glUseProgramStages(pipeline, stages, program);

  if(m_State > WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(USE_PROGRAMSTAGES);
    Serialise_glUseProgramStages(pipeline, stages, program);

    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ProgramPipeRes(GetCtx(), pipeline));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 pipeline);

    if(record == NULL)
      return;

    Chunk *chunk = scope.Get();

    if(m_State == WRITING_CAPFRAME)
    {
      m_ContextRecord->AddChunk(chunk);
    }
    else
    {
      // USE_PROGRAMSTAGES is one of the few kinds of chunk that are
      // recorded to pipeline records, so we can probably find previous
      // uses (if it's been constantly rebound instead of once at init
      // time) that can be popped as redundant.
      // We do have to be careful though to make sure we only remove
      // redundant calls, not other different USE_PROGRAMSTAGES calls!
      struct FilterChunkClass
      {
        FilterChunkClass(uint32_t s) : stages(s) {}
        uint32_t stages;

        // this is kind of a hack, but it would be really awkward
        // to make a general solution just for this one case, and
        // we also can't really afford to drop it entirely.
        // we search for the marker serialised above, skip over the
        // pipeline id (as it will be the same in all chunks in this
        // record), and check if the Stages bitfield afterwards is
        // the same - if so we remove that chunk as replaced by
        // this one
        bool operator()(Chunk *c) const
        {
          if(c->GetChunkType() != USE_PROGRAMSTAGES)
            return false;

          byte *b = c->GetData();
          byte *end = b + c->GetLength();

          // 'fast' path, rather than searching byte-by-byte from
          // the start to be safe, check the exact difference it should
          // always be first.
          if(*(uint64_t *)(b + 6) == marker_glUseProgramStages_hack)
            b += 6;

          while(b + sizeof(uint64_t) < end)
          {
            uint64_t *marker = (uint64_t *)b;
            if(*marker == marker_glUseProgramStages_hack)
            {
              // increment to point to pipeline id
              marker++;
              // increment to point to stages field
              marker++;

              // now compare
              uint32_t *chunkStages = (uint32_t *)marker;

              if(*chunkStages == stages)
                return true;
              return false;
            }

            b++;
          }
          RDCERR(
              "Didn't find marker value! This should not happen, check "
              "Serialise_glUseProgramStages serialisation");
          return false;
        }
      };
      record->FilterChunks(FilterChunkClass(stages));

      record->AddChunk(chunk);
    }

    if(program)
    {
      GLResourceRecord *progrecord =
          GetResourceManager()->GetResourceRecord(ProgramRes(GetCtx(), program));
      RDCASSERT(progrecord);
      record->AddParent(progrecord);
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

bool WrappedOpenGL::Serialise_glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), *pipelines)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenProgramPipelines(1, &real);
    m_Real.glBindProgramPipeline(real);
    m_Real.glBindProgramPipeline(0);

    GLResource res = ProgramPipeRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
  m_Real.glGenProgramPipelines(n, pipelines);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ProgramPipeRes(GetCtx(), pipelines[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_PROGRAMPIPE);
        Serialise_glGenProgramPipelines(1, pipelines + i);

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

bool WrappedOpenGL::Serialise_glCreateProgramPipelines(GLsizei n, GLuint *pipelines)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), *pipelines)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glCreateProgramPipelines(1, &real);

    GLResource res = ProgramPipeRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glCreateProgramPipelines(GLsizei n, GLuint *pipelines)
{
  m_Real.glCreateProgramPipelines(n, pipelines);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ProgramPipeRes(GetCtx(), pipelines[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_PROGRAMPIPE);
        Serialise_glCreateProgramPipelines(1, pipelines + i);

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

bool WrappedOpenGL::Serialise_glBindProgramPipeline(GLuint pipeline)
{
  SERIALISE_ELEMENT(
      ResourceId, id,
      (pipeline ? GetResourceManager()->GetID(ProgramPipeRes(GetCtx(), pipeline)) : ResourceId()));

  if(m_State <= EXECUTING)
  {
    if(id == ResourceId())
    {
      m_Real.glBindProgramPipeline(0);
    }
    else
    {
      GLuint live = GetResourceManager()->GetLiveResource(id).name;
      m_Real.glBindProgramPipeline(live);
    }
  }

  return true;
}

void WrappedOpenGL::glBindProgramPipeline(GLuint pipeline)
{
  m_Real.glBindProgramPipeline(pipeline);

  GetCtxData().m_ProgramPipeline = pipeline;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_PROGRAMPIPE);
    Serialise_glBindProgramPipeline(pipeline);

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
    // we'll ever
    // hit this path (most people using separable programs will use the glProgramUniform*
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

bool WrappedOpenGL::Serialise_glCompileShaderIncludeARB(GLuint shader, GLsizei count,
                                                        const GLchar *const *path,
                                                        const GLint *length)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(GetCtx(), shader)));
  SERIALISE_ELEMENT(int32_t, Count, count);

  vector<string> paths;

  for(int32_t i = 0; i < Count; i++)
  {
    string s;
    if(path && path[i])
      s = (length && length[i] > 0) ? string(path[i], path[i] + length[i]) : string(path[i]);

    m_pSerialiser->SerialiseString("path", s);

    if(m_State == READING)
      paths.push_back(s);
  }

  if(m_State == READING)
  {
    size_t numStrings = paths.size();

    const char **pathstrings = new const char *[numStrings];
    for(size_t i = 0; i < numStrings; i++)
      pathstrings[i] = paths[i].c_str();

    ResourceId liveId = GetResourceManager()->GetLiveID(id);

    auto &shadDetails = m_Shaders[liveId];

    shadDetails.includepaths.clear();
    shadDetails.includepaths.reserve(Count);

    for(int32_t i = 0; i < Count; i++)
      shadDetails.includepaths.push_back(pathstrings[i]);

    shadDetails.Compile(*this, id);

    m_Real.glCompileShaderIncludeARB(GetResourceManager()->GetLiveResource(id).name, Count,
                                     pathstrings, NULL);

    delete[] pathstrings;
  }

  return true;
}

void WrappedOpenGL::glCompileShaderIncludeARB(GLuint shader, GLsizei count,
                                              const GLchar *const *path, const GLint *length)
{
  m_Real.glCompileShaderIncludeARB(shader, count, path, length);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(GetCtx(), shader));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 shader);
    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(COMPILESHADERINCLUDE);
      Serialise_glCompileShaderIncludeARB(shader, count, path, length);

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

    shadDetails.Compile(*this, id);
  }
}

bool WrappedOpenGL::Serialise_glNamedStringARB(GLenum type, GLint namelen, const GLchar *name,
                                               GLint stringlen, const GLchar *str)
{
  SERIALISE_ELEMENT(GLenum, Type, type);

  string namestr = name ? string(name, name + (namelen > 0 ? namelen : strlen(name))) : "";
  string valstr = str ? string(str, str + (stringlen > 0 ? stringlen : strlen(str))) : "";

  m_pSerialiser->Serialise("Name", namestr);
  m_pSerialiser->Serialise("String", valstr);

  if(m_State == READING)
  {
    m_Real.glNamedStringARB(Type, (GLint)namestr.length(), namestr.c_str(), (GLint)valstr.length(),
                            valstr.c_str());
  }

  return true;
}

void WrappedOpenGL::glNamedStringARB(GLenum type, GLint namelen, const GLchar *name,
                                     GLint stringlen, const GLchar *str)
{
  m_Real.glNamedStringARB(type, namelen, name, stringlen, str);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(NAMEDSTRING);
    Serialise_glNamedStringARB(type, namelen, name, stringlen, str);

    // if a program repeatedly created/destroyed named strings this will fill up with useless
    // strings,
    // but chances are that won't be the case - a few will be created at init time and that's it
    m_DeviceRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glDeleteNamedStringARB(GLint namelen, const GLchar *name)
{
  string namestr = name ? string(name, name + (namelen > 0 ? namelen : strlen(name))) : "";

  m_pSerialiser->Serialise("Name", namestr);

  if(m_State == READING)
  {
    m_Real.glDeleteNamedStringARB((GLint)namestr.length(), namestr.c_str());
  }

  return true;
}

void WrappedOpenGL::glDeleteNamedStringARB(GLint namelen, const GLchar *name)
{
  m_Real.glDeleteNamedStringARB(namelen, name);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DELETENAMEDSTRING);
    Serialise_glDeleteNamedStringARB(namelen, name);

    // if a program repeatedly created/destroyed named strings this will fill up with useless
    // strings,
    // but chances are that won't be the case - a few will be created at init time and that's it
    m_DeviceRecord->AddChunk(scope.Get());
  }
}

#pragma endregion
