/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "gl_shader_refl.h"
#include <algorithm>
#include <functional>
#include "driver/shaders/spirv/glslang_compile.h"
#include "glslang/glslang/Public/ResourceLimits.h"
#include "glslang/glslang/Public/ShaderLang.h"
#include "gl_driver.h"

template <>
rdcstr DoStringise(const FFVertexOutput &el)
{
  BEGIN_ENUM_STRINGISE(FFVertexOutput);
  {
    STRINGISE_ENUM_CLASS_NAMED(PointSize, "gl_PointSize");
    STRINGISE_ENUM_CLASS_NAMED(ClipDistance, "gl_ClipDistance");
    STRINGISE_ENUM_CLASS_NAMED(CullDistance, "gl_CullDistance");
    STRINGISE_ENUM_CLASS_NAMED(ClipVertex, "gl_ClipVertex");
    STRINGISE_ENUM_CLASS_NAMED(FrontColor, "gl_FrontColor");
    STRINGISE_ENUM_CLASS_NAMED(BackColor, "gl_BackColor");
    STRINGISE_ENUM_CLASS_NAMED(FrontSecondaryColor, "gl_FrontSecondaryColor");
    STRINGISE_ENUM_CLASS_NAMED(BackSecondaryColor, "gl_BackSecondaryColor");
    STRINGISE_ENUM_CLASS_NAMED(TexCoord, "gl_TexCoord");
    STRINGISE_ENUM_CLASS_NAMED(FogFragCoord, "gl_FogFragCoord");
    STRINGISE_ENUM_CLASS_NAMED(Count, "gl_Count");
  }
  END_ENUM_STRINGISE();
}

void namesort(rdcarray<ShaderConstant> &vars)
{
  if(vars.empty())
    return;

  struct name_sort
  {
    bool operator()(const ShaderConstant &a, const ShaderConstant &b) { return a.name < b.name; }
  };

  std::sort(vars.begin(), vars.end(), name_sort());

  for(size_t i = 0; i < vars.size(); i++)
    namesort(vars[i].type.members);
}

void sort(rdcarray<ShaderConstant> &vars)
{
  if(vars.empty())
    return;

  std::sort(vars.begin(), vars.end(), [](const ShaderConstant &a, const ShaderConstant &b) {
    return a.byteOffset < b.byteOffset;
  });

  for(size_t i = 0; i < vars.size(); i++)
    sort(vars[i].type.members);
}

void CheckVertexOutputUses(const rdcarray<rdcstr> &sources, FixedFunctionVertexOutputs &outputUsage)
{
  outputUsage = FixedFunctionVertexOutputs();

  for(FFVertexOutput output : values<FFVertexOutput>())
  {
    // we consider an output used if we encounter a '=' before either a ';' or the end of the string
    rdcstr name = ToStr(output);

    for(size_t i = 0; i < sources.size(); i++)
    {
      const rdcstr &s = sources[i];

      int32_t offs = 0;

      for(;;)
      {
        offs = s.find(name, offs);

        if(offs < 0)
          break;

        while(offs < s.count())
        {
          if(s[offs] == '=')
          {
            outputUsage.used[(int)output] = true;
            break;
          }

          if(s[offs] == ';')
            break;

          offs++;
        }
      }
    }
  }
}

// little utility function that if necessary emulates glCreateShaderProgramv functionality but using
// glCompileShaderIncludeARB
static GLuint CreateSepProgram(WrappedOpenGL &driver, GLenum type, GLsizei numSources,
                               const char **sources, GLsizei numPaths, const char **paths)
{
  // by the nature of this function, it might fail - we don't want to spew
  // false positive looking messages into the log.
  driver.SuppressDebugMessages(true);

  GLuint program = 0;

  // definition of glCreateShaderProgramv from the spec
  GLuint shader = driver.glCreateShader(type);
  if(shader)
  {
    driver.glShaderSource(shader, numSources, sources, NULL);

    if(paths == NULL)
      driver.glCompileShader(shader);
    else
      driver.glCompileShaderIncludeARB(shader, numPaths, paths, NULL);

    program = driver.glCreateProgram();
    if(program)
    {
      GLint compiled = 0;

      driver.glGetShaderiv(shader, eGL_COMPILE_STATUS, &compiled);
      driver.glProgramParameteri(program, eGL_PROGRAM_SEPARABLE, GL_TRUE);

      if(compiled)
      {
        driver.glAttachShader(program, shader);
        driver.glLinkProgram(program);

        // we deliberately leave the shaders attached so this program can be re-linked.
        // they will be cleaned up when the program is deleted
        // driver.glDetachShader(program, shader);
      }
    }
    driver.glDeleteShader(shader);
  }

  driver.SuppressDebugMessages(false);
  return program;
}

static bool isspacetab(char c)
{
  return c == '\t' || c == ' ';
}

static bool isnewline(char c)
{
  return c == '\r' || c == '\n';
}

static bool iswhitespace(char c)
{
  return isspacetab(c) || isnewline(c);
}

GLuint MakeSeparableShaderProgram(WrappedOpenGL &drv, GLenum type, const rdcarray<rdcstr> &sources,
                                  const rdcarray<rdcstr> &includepaths)
{
  // in and out blocks are added separately, in case one is there already
  const char *blockIdentifiers[2] = {"in gl_PerVertex", "out gl_PerVertex"};
  rdcstr blocks[2] = {"", ""};

  if(type == eGL_VERTEX_SHADER)
  {
    blocks[1] =
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[]; };\n";
  }
  else if(type == eGL_TESS_CONTROL_SHADER)
  {
    blocks[0] =
        "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[]; } "
        "gl_in[];\n";
    blocks[1] =
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[]; } "
        "gl_out[];\n";
  }
  else
  {
    blocks[0] =
        "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[]; } "
        "gl_in[];\n";
    blocks[1] =
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[]; };\n";
  }

  const char **strings = new const char *[sources.size()];
  for(size_t i = 0; i < sources.size(); i++)
    strings[i] = sources[i].c_str();

  const char **paths = NULL;
  GLsizei numPaths = 0;
  if(!includepaths.empty())
  {
    numPaths = (GLsizei)includepaths.size();

    paths = new const char *[includepaths.size()];
    for(size_t i = 0; i < includepaths.size(); i++)
      paths[i] = includepaths[i].c_str();
  }

  GLuint sepProg = CreateSepProgram(drv, type, (GLsizei)sources.size(), strings, numPaths, paths);

  GLint status;
  drv.glGetProgramiv(sepProg, eGL_LINK_STATUS, &status);

  // allow any vertex processing shader to redeclare gl_PerVertex
  // on GLES it is not required
  if(!IsGLES && status == 0 && type != eGL_FRAGMENT_SHADER && type != eGL_COMPUTE_SHADER)
  {
    drv.glDeleteProgram(sepProg);
    sepProg = 0;

    // try and patch up shader
    // naively insert gl_PerVertex block as soon as it's valid (after #version)
    // this will fail if e.g. a member of gl_PerVertex is declared at global scope
    // (this is probably most likely for clipdistance if it's redeclared with a size)

    // we start by concatenating the source strings to make parsing easier.
    rdcstr combined;

    for(size_t i = 0; i < sources.size(); i++)
      combined += sources[i];

    for(int attempt = 0; attempt < 2; attempt++)
    {
      rdcstr src = combined;

      if(attempt == 1)
      {
        drv.glDeleteProgram(sepProg);
        sepProg = 0;

        RDCLOG("Attempting to pre-process shader with glslang to allow patching");

        glslang::TShader sh(EShLanguage(ShaderIdx(type)));

        const char *c_src = combined.c_str();
        sh.setStrings(&c_src, 1);
        sh.setEnvInput(glslang::EShSourceGlsl, EShLanguage(ShaderIdx(type)),
                       glslang::EShClientOpenGL, 100);
        sh.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        sh.setEnvTarget(glslang::EShTargetNone, glslang::EShTargetSpv_1_0);

        glslang::TShader::ForbidIncluder incl;

        bool success;

        {
          std::string outstr;
          success = sh.preprocess(GetDefaultResources(), 100, ENoProfile, false, false,
                                  EShMsgOnlyPreprocessor, &outstr, incl);
          src.assign(outstr.c_str(), outstr.size());
        }

        if(!success)
        {
          RDCLOG("glslang failed:\n\n%s\n\n%s", sh.getInfoLog(), sh.getInfoDebugLog());
          continue;
        }
      }

      for(int blocktype = 0; blocktype < 2; blocktype++)
      {
        // vertex shaders don't have an in block
        if(type == eGL_VERTEX_SHADER && blocktype == 0)
          continue;

        rdcstr block = blocks[blocktype];
        const char *identifier = blockIdentifiers[blocktype];

        // if we find the 'identifier' (ie. the block name),
        // assume this block is already present and stop.
        // only try and insert this block if the shader doesn't already have it
        if(src.contains(identifier))
        {
          continue;
        }

        {
          int32_t len = src.count();

          // find if this source contains a #version, accounting for whitespace
          int32_t it = 0;

          while(it >= 0)
          {
            it = src.find("#", it);

            if(it < 0)
              break;

            // advance past the #
            ++it;

            // skip whitespace
            while(it < len && isspacetab(src[it]))
              ++it;

            if(it + 7 < len && !strncmp(&src[it], "version", 7))
            {
              it += sizeof("version") - 1;
              break;
            }
          }

          // no #version found
          if(it < 0)
          {
            // ensure we're using at least #version 130
            src.insert(0, "#version 130\n");
            // insert after that
            it = 13;
          }
          else
          {
            // it now points after the #version

            // skip whitespace
            while(it < len && isspacetab(src[it]))
              ++it;

            // if the version is less than 130 we need to upgrade it to even be able to use out
            // blocks
            if(src[it] == '1' && src[it + 1] < '3')
              src[it + 1] = '3';

            // skip number
            while(it < len && src[it] >= '0' && src[it] <= '9')
              ++it;

            // skip whitespace
            while(it < len && isspacetab(src[it]))
              ++it;

            if(!strncmp(&src[it], "core", 4))
              it += sizeof("core") - 1;
            if(!strncmp(&src[it], "compatibility", 13))
              it += sizeof("compatibility") - 1;
            if(!strncmp(&src[it], "es", 2))
              it += sizeof("es") - 1;

            it++;

            // next line after #version, insert the extension declaration
            if(it < src.count())
              src.insert(it, "#extension GL_ARB_separate_shader_objects : enable\n");

            // how deep are we in an #if. We want to place our definition
            // outside of any #ifs.
            int if_depth = 0;

            // now skip past comments, and any #directives
            while(it < len)
            {
              // skip whitespace
              while(it < len && iswhitespace(src[it]))
                ++it;

              // skip C++ style comments
              if(it + 1 < len && src[it] == '/' && src[it + 1] == '/')
              {
                // keep going until the next newline
                while(it < len && !isnewline(src[it]))
                  ++it;

                // skip more things
                continue;
              }

              // skip preprocessor directives
              if(src[it] == '#')
              {
                // skip the '#'
                it++;

                // skip whitespace
                while(it < len && iswhitespace(src[it]))
                  ++it;

                // if it's an if, then increase our depth
                // This covers:
                // #if
                // #ifdef
                // #ifndef
                if(!strncmp(&src[it], "if", 2))
                {
                  if_depth++;
                }
                else if(!strncmp(&src[it], "endif", 5))
                {
                  if_depth--;
                }
                // everything else is #extension or #else or #undef or anything

                // keep going until the next newline
                while(it < len && !isnewline(src[it]))
                {
                  // if we encounter a C-style comment in the middle of a #define
                  // we can't consume it because then we'd miss the start of it.
                  // Instead we break out (although we're not technically at the
                  // end of the pre-processor line) and let it be consumed next.
                  // Note that we can discount C++-style comments because they
                  // want to consume to the end of the line too.
                  if(it + 1 < len && src[it] == '/' && src[it + 1] == '*')
                    break;

                  ++it;
                }

                // skip more things
                continue;
              }

              // skip C style comments
              if(it + 1 < len && src[it] == '/' && src[it + 1] == '*')
              {
                // keep going until the we reach a */
                while(it + 1 < len && (src[it] != '*' || src[it + 1] != '/'))
                  ++it;

                // skip the closing */ too
                it += 2;

                // skip more things
                continue;
              }

              // see if we have a precision statement, if so skip that
              const char precision[] = "precision";
              if(int32_t(it + sizeof(precision)) < len &&
                 !strncmp(&src[it], precision, sizeof(precision) - 1))
              {
                // since we're speculating here (although what else could it be?) we don't modify
                // it until we're sure.
                int32_t pit = it + sizeof(precision);

                // skip whitespace
                while(pit < len && isspacetab(src[pit]))
                  ++pit;

                // if we now match any of the precisions, then continue consuming until the next ;
                const char lowp[] = "lowp";
                const char mediump[] = "mediump";
                const char highp[] = "highp";

                bool precisionMatch = (int32_t(pit + sizeof(lowp)) < len &&
                                       !strncmp(&src[pit], lowp, sizeof(lowp) - 1) &&
                                       isspacetab(src[pit + sizeof(lowp) - 1]));
                precisionMatch |= (int32_t(pit + sizeof(mediump)) < len &&
                                   !strncmp(&src[pit], mediump, sizeof(mediump) - 1) &&
                                   isspacetab(src[pit + sizeof(mediump) - 1]));
                precisionMatch |= (int32_t(pit + sizeof(highp)) < len &&
                                   !strncmp(&src[pit], highp, sizeof(highp) - 1) &&
                                   isspacetab(src[pit + sizeof(highp) - 1]));

                if(precisionMatch)
                {
                  it = pit;
                  while(it < len && src[it] != ';')
                    ++it;

                  ++it;    // skip the ; itself

                  // skip more things
                  continue;
                }

                // otherwise just stop here, it's not a precision statement
              }

              // nothing more to skip, check if we're outside an if
              if(if_depth == 0)
                break;

              // if not, this might not be a comment, etc etc. Just skip to the next line
              // so we can keep going to find the #endif
              while(it < len && !isnewline(src[it]))
                ++it;
            }
          }

          if(it < src.count())
            src.insert(it, block);
        }
      }

      const char *c_src = src.c_str();

      sepProg = CreateSepProgram(drv, type, 1, &c_src, numPaths, paths);

      // when we get it to link, bail!
      drv.glGetProgramiv(sepProg, eGL_LINK_STATUS, &status);
      if(status == 1)
        break;

      RDCWARN("Couldn't patch separability into shader, attempt #%d", attempt + 1);
    }
  }

  if(sepProg)
    drv.glGetProgramiv(sepProg, eGL_LINK_STATUS, &status);
  else
    status = 0;

  if(status == 0)
  {
    char buffer[1025] = {0};
    drv.glGetProgramInfoLog(sepProg, 1024, NULL, buffer);

    RDCERR("Couldn't make separable shader program for shader. Errors:\n%s", buffer);

    drv.glDeleteProgram(sepProg);
    sepProg = 0;
  }

  delete[] strings;
  if(paths)
    delete[] paths;

  return sepProg;
}

void ReconstructVarTree(GLenum query, GLuint sepProg, GLuint varIdx, GLint numParentBlocks,
                        rdcarray<ShaderConstant> *parentBlocks,
                        rdcarray<ShaderConstant> *defaultBlock)
{
  const size_t numProps = 9;

  GLenum resProps[numProps] = {eGL_TYPE,         eGL_NAME_LENGTH,  eGL_LOCATION,
                               eGL_BLOCK_INDEX,  eGL_ARRAY_SIZE,   eGL_OFFSET,
                               eGL_IS_ROW_MAJOR, eGL_ARRAY_STRIDE, eGL_MATRIX_STRIDE};

  // GL_LOCATION not valid for buffer variables (it's only used if offset comes back -1, which will
  // never happen for buffer variables)
  if(query == eGL_BUFFER_VARIABLE)
    resProps[2] = eGL_OFFSET;

  GLint values[numProps] = {-1, -1, -1, -1, -1, -1, -1, -1};
  GL.glGetProgramResourceiv(sepProg, query, varIdx, numProps, resProps, numProps, NULL, values);

  ShaderConstant var;

  var.type.elements = RDCMAX(1, values[4]);

  // set type (or bail if it's not a variable - sampler or such)
  switch(values[0])
  {
    case eGL_FLOAT_VEC4:
    case eGL_FLOAT_VEC3:
    case eGL_FLOAT_VEC2:
    case eGL_FLOAT:
    case eGL_FLOAT_MAT4:
    case eGL_FLOAT_MAT3:
    case eGL_FLOAT_MAT2:
    case eGL_FLOAT_MAT4x2:
    case eGL_FLOAT_MAT4x3:
    case eGL_FLOAT_MAT3x4:
    case eGL_FLOAT_MAT3x2:
    case eGL_FLOAT_MAT2x4:
    case eGL_FLOAT_MAT2x3: var.type.baseType = VarType::Float; break;
    case eGL_DOUBLE_VEC4:
    case eGL_DOUBLE_VEC3:
    case eGL_DOUBLE_VEC2:
    case eGL_DOUBLE:
    case eGL_DOUBLE_MAT4:
    case eGL_DOUBLE_MAT3:
    case eGL_DOUBLE_MAT2:
    case eGL_DOUBLE_MAT4x2:
    case eGL_DOUBLE_MAT4x3:
    case eGL_DOUBLE_MAT3x4:
    case eGL_DOUBLE_MAT3x2:
    case eGL_DOUBLE_MAT2x4:
    case eGL_DOUBLE_MAT2x3: var.type.baseType = VarType::Double; break;
    case eGL_UNSIGNED_INT_VEC4:
    case eGL_UNSIGNED_INT_VEC3:
    case eGL_UNSIGNED_INT_VEC2:
    case eGL_UNSIGNED_INT:
    case eGL_BOOL_VEC4:
    case eGL_BOOL_VEC3:
    case eGL_BOOL_VEC2:
    case eGL_BOOL: var.type.baseType = VarType::UInt; break;
    case eGL_INT_VEC4:
    case eGL_INT_VEC3:
    case eGL_INT_VEC2:
    case eGL_INT: var.type.baseType = VarType::SInt; break;
    default:
      // not a variable (sampler etc)
      return;
  }

  // set # rows if it's a matrix
  var.type.rows = 1;

  switch(values[0])
  {
    case eGL_FLOAT_MAT4:
    case eGL_DOUBLE_MAT4:
    case eGL_FLOAT_MAT2x4:
    case eGL_DOUBLE_MAT2x4:
    case eGL_FLOAT_MAT3x4:
    case eGL_DOUBLE_MAT3x4: var.type.rows = 4; break;
    case eGL_FLOAT_MAT3:
    case eGL_DOUBLE_MAT3:
    case eGL_FLOAT_MAT4x3:
    case eGL_DOUBLE_MAT4x3:
    case eGL_FLOAT_MAT2x3:
    case eGL_DOUBLE_MAT2x3: var.type.rows = 3; break;
    case eGL_FLOAT_MAT2:
    case eGL_DOUBLE_MAT2:
    case eGL_FLOAT_MAT4x2:
    case eGL_DOUBLE_MAT4x2:
    case eGL_FLOAT_MAT3x2:
    case eGL_DOUBLE_MAT3x2: var.type.rows = 2; break;
    default: break;
  }

  // set # columns
  switch(values[0])
  {
    case eGL_FLOAT_VEC4:
    case eGL_FLOAT_MAT4:
    case eGL_FLOAT_MAT4x2:
    case eGL_FLOAT_MAT4x3:
    case eGL_DOUBLE_VEC4:
    case eGL_DOUBLE_MAT4:
    case eGL_DOUBLE_MAT4x2:
    case eGL_DOUBLE_MAT4x3:
    case eGL_UNSIGNED_INT_VEC4:
    case eGL_BOOL_VEC4:
    case eGL_INT_VEC4: var.type.columns = 4; break;
    case eGL_FLOAT_VEC3:
    case eGL_FLOAT_MAT3:
    case eGL_FLOAT_MAT3x4:
    case eGL_FLOAT_MAT3x2:
    case eGL_DOUBLE_VEC3:
    case eGL_DOUBLE_MAT3:
    case eGL_DOUBLE_MAT3x4:
    case eGL_DOUBLE_MAT3x2:
    case eGL_UNSIGNED_INT_VEC3:
    case eGL_BOOL_VEC3:
    case eGL_INT_VEC3: var.type.columns = 3; break;
    case eGL_FLOAT_VEC2:
    case eGL_FLOAT_MAT2:
    case eGL_FLOAT_MAT2x4:
    case eGL_FLOAT_MAT2x3:
    case eGL_DOUBLE_VEC2:
    case eGL_DOUBLE_MAT2:
    case eGL_DOUBLE_MAT2x4:
    case eGL_DOUBLE_MAT2x3:
    case eGL_UNSIGNED_INT_VEC2:
    case eGL_BOOL_VEC2:
    case eGL_INT_VEC2: var.type.columns = 2; break;
    case eGL_FLOAT:
    case eGL_DOUBLE:
    case eGL_UNSIGNED_INT:
    case eGL_INT:
    case eGL_BOOL: var.type.columns = 1; break;
    default: break;
  }

  // set name
  switch(values[0])
  {
    case eGL_FLOAT_VEC4: var.type.name = "vec4"; break;
    case eGL_FLOAT_VEC3: var.type.name = "vec3"; break;
    case eGL_FLOAT_VEC2: var.type.name = "vec2"; break;
    case eGL_FLOAT: var.type.name = "float"; break;
    case eGL_FLOAT_MAT4: var.type.name = "mat4"; break;
    case eGL_FLOAT_MAT3: var.type.name = "mat3"; break;
    case eGL_FLOAT_MAT2: var.type.name = "mat2"; break;
    case eGL_FLOAT_MAT4x2: var.type.name = "mat4x2"; break;
    case eGL_FLOAT_MAT4x3: var.type.name = "mat4x3"; break;
    case eGL_FLOAT_MAT3x4: var.type.name = "mat3x4"; break;
    case eGL_FLOAT_MAT3x2: var.type.name = "mat3x2"; break;
    case eGL_FLOAT_MAT2x4: var.type.name = "mat2x4"; break;
    case eGL_FLOAT_MAT2x3: var.type.name = "mat2x3"; break;
    case eGL_DOUBLE_VEC4: var.type.name = "dvec4"; break;
    case eGL_DOUBLE_VEC3: var.type.name = "dvec3"; break;
    case eGL_DOUBLE_VEC2: var.type.name = "dvec2"; break;
    case eGL_DOUBLE: var.type.name = "double"; break;
    case eGL_DOUBLE_MAT4: var.type.name = "dmat4"; break;
    case eGL_DOUBLE_MAT3: var.type.name = "dmat3"; break;
    case eGL_DOUBLE_MAT2: var.type.name = "dmat2"; break;
    case eGL_DOUBLE_MAT4x2: var.type.name = "dmat4x2"; break;
    case eGL_DOUBLE_MAT4x3: var.type.name = "dmat4x3"; break;
    case eGL_DOUBLE_MAT3x4: var.type.name = "dmat3x4"; break;
    case eGL_DOUBLE_MAT3x2: var.type.name = "dmat3x2"; break;
    case eGL_DOUBLE_MAT2x4: var.type.name = "dmat2x4"; break;
    case eGL_DOUBLE_MAT2x3: var.type.name = "dmat2x3"; break;
    case eGL_UNSIGNED_INT_VEC4: var.type.name = "uvec4"; break;
    case eGL_UNSIGNED_INT_VEC3: var.type.name = "uvec3"; break;
    case eGL_UNSIGNED_INT_VEC2: var.type.name = "uvec2"; break;
    case eGL_UNSIGNED_INT: var.type.name = "uint"; break;
    case eGL_BOOL_VEC4: var.type.name = "bvec4"; break;
    case eGL_BOOL_VEC3: var.type.name = "bvec3"; break;
    case eGL_BOOL_VEC2: var.type.name = "bvec2"; break;
    case eGL_BOOL: var.type.name = "bool"; break;
    case eGL_INT_VEC4: var.type.name = "ivec4"; break;
    case eGL_INT_VEC3: var.type.name = "ivec3"; break;
    case eGL_INT_VEC2: var.type.name = "ivec2"; break;
    case eGL_INT: var.type.name = "int"; break;
    default: break;
  }

  if(values[5] == -1 && values[2] >= 0)
  {
    var.byteOffset = values[2];
  }
  else if(values[5] >= 0)
  {
    var.byteOffset = values[5];
  }
  else
  {
    var.byteOffset = ~0U;
  }

  if(values[6] > 0)
    var.type.flags |= ShaderVariableFlags::RowMajorMatrix;
  var.type.matrixByteStride = (uint8_t)values[8];
  var.type.arrayByteStride = (uint32_t)values[7];

  bool bareUniform = false;

  // for plain uniforms we won't get an array/matrix byte stride. Calculate tightly packed strides
  if(values[3] == -1)
  {
    bareUniform = true;

    // plain matrices are always column major, so this is the size of a column
    var.type.flags &= ~ShaderVariableFlags::RowMajorMatrix;

    const uint32_t elemByteStride = (var.type.baseType == VarType::Double) ? 8 : 4;
    var.type.matrixByteStride = uint8_t(var.type.rows * elemByteStride);

    // arrays are fetched as individual glGetUniform calls
    var.type.arrayByteStride = 0;
  }

  // set vectors/scalars as row major for convenience, since that's how they're stored in the fv
  // array.
  switch(values[0])
  {
    case eGL_FLOAT_VEC4:
    case eGL_FLOAT_VEC3:
    case eGL_FLOAT_VEC2:
    case eGL_FLOAT:
    case eGL_DOUBLE_VEC4:
    case eGL_DOUBLE_VEC3:
    case eGL_DOUBLE_VEC2:
    case eGL_DOUBLE:
    case eGL_UNSIGNED_INT_VEC4:
    case eGL_UNSIGNED_INT_VEC3:
    case eGL_UNSIGNED_INT_VEC2:
    case eGL_UNSIGNED_INT:
    case eGL_BOOL_VEC4:
    case eGL_BOOL_VEC3:
    case eGL_BOOL_VEC2:
    case eGL_BOOL:
    case eGL_INT_VEC4:
    case eGL_INT_VEC3:
    case eGL_INT_VEC2:
    case eGL_INT: var.type.flags |= ShaderVariableFlags::RowMajorMatrix; break;
    default: break;
  }

  var.name.resize(values[1] - 1);
  GL.glGetProgramResourceName(sepProg, query, varIdx, values[1], NULL, &var.name[0]);

  rdcstr fullname = var.name;

  int32_t c = values[1] - 1;

  // trim off trailing [0] if it's an array
  if(var.name[c - 3] == '[' && var.name[c - 2] == '0' && var.name[c - 1] == ']')
    var.name.resize(c - 3);
  else
    var.type.elements = 1;

  GLint topLevelStride = 0;
  if(query == eGL_BUFFER_VARIABLE)
  {
    GLenum propName = eGL_TOP_LEVEL_ARRAY_STRIDE;
    GL.glGetProgramResourceiv(sepProg, query, varIdx, 1, &propName, 1, NULL, &topLevelStride);

    // if ARRAY_SIZE is 0 this is an unbounded array
    if(values[4] == 0)
      var.type.elements = ~0U;
  }

  rdcarray<ShaderConstant> *parentmembers = defaultBlock;

  if(!bareUniform && values[3] < numParentBlocks)
  {
    parentmembers = &parentBlocks[values[3]];
  }

  if(parentmembers == NULL)
  {
    RDCWARN("Found variable '%s' without parent block index '%d'", var.name.c_str(), values[3]);
    return;
  }

  char *nm = &var.name[0];

  bool multiDimArray = false;
  int arrayIdx = 0;

  bool blockLevel = true;
  int level = 0;

  // reverse figure out structures and structure arrays
  while(strchr(nm, '.') || strchr(nm, '['))
  {
    char *base = nm;
    while(*nm != '.' && *nm != '[')
      nm++;

    // determine if we have an array index, and NULL out
    // what's after the base variable name
    bool isarray = (*nm == '[');
    *nm = 0;
    nm++;

    arrayIdx = 0;

    // if it's an array, get the index used
    if(isarray)
    {
      // get array index, it's always a decimal number
      while(*nm >= '0' && *nm <= '9')
      {
        arrayIdx *= 10;
        arrayIdx += int(*nm) - int('0');
        nm++;
      }

      RDCASSERT(*nm == ']');
      *nm = 0;
      nm++;

      // skip forward to the child name
      if(*nm == '.')
      {
        *nm = 0;
        nm++;
      }
      else
      {
        // if there's no . after the array index, this is a multi-dimensional array.
        multiDimArray = true;
      }
    }

    // construct a parent variable
    ShaderConstant parentVar;
    parentVar.name = base;
    parentVar.byteOffset = var.byteOffset;
    parentVar.type.name = "struct";
    parentVar.type.rows = 0;
    parentVar.type.columns = 0;
    parentVar.type.baseType = VarType::Struct;
    parentVar.type.elements = isarray && !multiDimArray ? RDCMAX(1U, uint32_t(arrayIdx + 1)) : 1;
    parentVar.type.matrixByteStride = 0;
    parentVar.type.arrayByteStride = (uint32_t)topLevelStride;

    // consider all block-level SSBO structs to have infinite elements if they are an array at all
    // for structs that aren't the last struct in a block which can't be infinite, this will be
    // fixup'd later by looking at the offset of subsequent elements
    if(blockLevel && topLevelStride && isarray)
      parentVar.type.elements = ~0U;

    if(!blockLevel)
      topLevelStride = 0;

    // this is no longer block level after the first array, or the first struct member.
    //
    // this logic is because whether or not a block has a name affects what comes back. E.g.
    // buffer ssbo { float ssbo_foo } ; will just be "ssbo_foo", whereas
    // buffer ssbo { float ssbo_foo } root; will be "root.ssbo_foo"
    //
    // we only use blocklevel for the check above, to see if we are on a block-level array to mark
    // it as unknown size (to be fixed later), so this check can be a little fuzzy as long as it
    // doesn't have false positives.
    if(isarray || level >= 1)
      blockLevel = false;

    bool found = false;

    // if we can find the base variable already, we recurse into its members
    for(size_t i = 0; i < parentmembers->size(); i++)
    {
      if((*parentmembers)[i].name == base)
      {
        // if we find the variable, update the # elements to account for this new array index
        // and pick the minimum offset of all of our children as the parent offset. This is mostly
        // just for sorting
        (*parentmembers)[i].type.elements =
            RDCMAX((*parentmembers)[i].type.elements, parentVar.type.elements);
        (*parentmembers)[i].byteOffset = RDCMIN((*parentmembers)[i].byteOffset, parentVar.byteOffset);

        parentmembers = &((*parentmembers)[i].type.members);
        found = true;

        break;
      }
    }

    // if we didn't find the base variable, add it and recuse inside
    if(!found)
    {
      parentmembers->push_back(parentVar);
      parentmembers = &(parentmembers->back().type.members);
    }

    if(multiDimArray)
    {
      // if this is a multi-dimensional array, we've now selected the root array.
      // We now iterate all the way down to the last element, then break out of the list so it can
      // be added as an array.
      //
      // Note: this means that for float foo[4][3] we won't iterate here - we've already selected
      // foo, and outside of the loop we'll push back a float[3] for each of foo[0], foo[1], foo[2],
      // foo[3] that we encounter in this iteration process.
      //
      // For bar[4][3][2] we've selected bar, we'll then push back a [4] member and then outside of
      // the loop we'll push back each of bar[.][0], bar[.][1], etc as a float[2]

      while(*nm)
      {
        parentVar.name = StringFormat::Fmt("[%d]", arrayIdx);

        found = false;
        for(size_t i = 0; i < parentmembers->size(); i++)
        {
          if((*parentmembers)[i].name == parentVar.name)
          {
            parentmembers = &((*parentmembers)[i].type.members);
            found = true;

            break;
          }
        }

        if(!found)
        {
          parentmembers->push_back(parentVar);
          parentmembers = &(parentmembers->back().type.members);
        }

        arrayIdx = 0;

        RDCASSERT(*nm == '[');
        nm++;

        while(*nm >= '0' && *nm <= '9')
        {
          arrayIdx *= 10;
          arrayIdx += int(*nm) - int('0');
          nm++;
        }

        RDCASSERT(*nm == ']');
        *nm = 0;
        nm++;
      }

      break;
    }

    // the 0th element of each array fills out the actual members, when we
    // encounter an index above that we only use it to increase the type.elements
    // member (which we've done by this point) and can stop recursing
    //
    // The exception is when we're looking at bare uniforms - there the struct members all have
    // individual locations and are generally aggressively stripped by the driver.
    // So then it's possible to get foo[0].bar.a and foo[1].bar.b - so higher indices can reveal
    // more of the structure.
    if(arrayIdx > 0 && !bareUniform)
    {
      parentmembers = NULL;
      break;
    }

    level++;
  }

  if(parentmembers)
  {
    // if this is a bare uniform we need to be careful - just above we continued iterating for
    // higher indices, because you can have cases that return e.g. foo[0].bar.a and foo[1].bar.b and
    // get 'more information' from later indices, the full structure is not revealed under foo[0].
    // However as a result of that it means we could see foo[0].bar.a and foo[1].bar.a - we need to
    // be careful not to add the final 'a' twice. Check for duplicates and be sure it's really a
    // duplicate.
    bool duplicate = false;

    // nm points into var.name's storage, so copy out to a temporary
    rdcstr n = nm;
    var.name = n;

    if(bareUniform && !multiDimArray)
    {
      for(size_t i = 0; i < parentmembers->size(); i++)
      {
        if((*parentmembers)[i].name == var.name)
        {
          ShaderConstantType &oldtype = (*parentmembers)[i].type;
          ShaderConstantType &newtype = var.type;

          if(oldtype.rows != newtype.rows || oldtype.columns != newtype.columns ||
             oldtype.baseType != newtype.baseType || oldtype.elements != newtype.elements)
          {
            RDCERR("When reconstructing %s, found duplicate but different final member %s",
                   fullname.c_str(), (*parentmembers)[i].name.c_str());
          }

          duplicate = true;
          break;
        }
      }
    }

    if(duplicate)
      return;

    // for multidimensional arrays there will be no proper name, so name the variable by the index
    if(multiDimArray)
      var.name = StringFormat::Fmt("[%d]", arrayIdx);

    parentmembers->push_back(var);
  }
}

static uint32_t GetVarAlignment(bool std140, const ShaderConstant &c)
{
  if(!c.type.members.empty())
  {
    uint32_t ret = 4;
    for(const ShaderConstant &m : c.type.members)
      ret = RDCMAX(ret, GetVarAlignment(std140, m));

    if(std140)
      ret = AlignUp16(ret);
    return ret;
  }

  uint8_t vecSize = c.type.columns;

  if(c.type.rows > 1 && c.type.ColMajor())
    vecSize = c.type.rows;

  if(vecSize <= 1)
    return 4;
  if(vecSize == 2)
    return 8;
  return 16;
}

static uint32_t GetVarArrayStride(bool std140, const ShaderConstant &c)
{
  uint32_t stride;
  if(!c.type.members.empty())
  {
    const ShaderConstant &lastChild = c.type.members.back();
    stride = GetVarArrayStride(std140, lastChild);
    if(lastChild.type.elements > 1 && lastChild.type.elements != ~0U)
      stride *= lastChild.type.elements;
    stride = AlignUp(lastChild.byteOffset + stride, GetVarAlignment(std140, c));
  }
  else
  {
    if(c.type.elements > 1)
    {
      stride = c.type.arrayByteStride;
    }
    else
    {
      stride = VarTypeByteSize(c.type.baseType);

      if(c.type.rows > 1)
      {
        if(std140)
        {
          stride *= 4;

          if(c.type.ColMajor())
            stride *= RDCMAX((uint8_t)1, c.type.columns);
          else
            stride *= RDCMAX((uint8_t)1, c.type.rows);
        }
        else
        {
          if(c.type.ColMajor())
          {
            stride *= RDCMAX((uint8_t)1, c.type.columns);
            if(c.type.rows == 3)
              stride *= 4;
            else
              stride *= RDCMAX((uint8_t)1, c.type.rows);
          }
          else
          {
            stride *= RDCMAX((uint8_t)1, c.type.rows);
            if(c.type.columns == 3)
              stride *= 4;
            else
              stride *= RDCMAX((uint8_t)1, c.type.columns);
          }
        }
      }
      else
      {
        if(c.type.columns == 3 && std140)
          stride *= 4;
        else
          stride *= RDCMAX((uint8_t)1, c.type.columns);
      }
    }
  }

  return stride;
}

void FixupStructOffsetsAndSize(bool std140, ShaderConstant &member)
{
  for(ShaderConstant &child : member.type.members)
  {
    FixupStructOffsetsAndSize(std140, child);
    child.byteOffset -= member.byteOffset;
  }

  if(!member.type.members.empty())
  {
    member.type.arrayByteStride = GetVarArrayStride(std140, member);
  }
}

int ParseVersionStatement(const char *version)
{
  if(strncmp(version, "#version", 8) != 0)
    return 0;

  version += 8;
  while(isspace(*version))
    version++;

  int ret = 0;
  while(*version >= '0' && *version <= '9')
  {
    ret *= 10;
    ret += int(*version) - int('0');
    version++;
  }

  return ret;
}

static void AddSigParameter(rdcarray<SigParameter> &sigs, uint32_t &regIndex,
                            const SigParameter &sig, const char *nm, int rows, int arrayIdx)
{
  if(rows == 1)
  {
    SigParameter s = sig;

    if(s.regIndex == ~0U)
      s.regIndex = regIndex++;
    else if(arrayIdx >= 0)
      s.regIndex += arrayIdx;

    if(arrayIdx >= 0)
      s.varName = StringFormat::Fmt("%s[%d]", nm, arrayIdx);

    sigs.push_back(s);
  }
  else
  {
    for(int r = 0; r < rows; r++)
    {
      SigParameter s = sig;

      if(s.regIndex == ~0U)
        s.regIndex = regIndex++;
      else if(arrayIdx >= 0)
        s.regIndex += rows * arrayIdx + r;
      else
        s.regIndex += r;

      if(arrayIdx >= 0)
        s.varName = StringFormat::Fmt("%s[%d]:col%d", nm, arrayIdx, r);
      else
        s.varName = StringFormat::Fmt("%s:col%d", nm, r);

      sigs.push_back(s);
    }
  }
}

void MakeShaderReflection(GLenum shadType, GLuint sepProg, ShaderReflection &refl,
                          const FixedFunctionVertexOutputs &outputUsage)
{
  refl.stage = MakeShaderStage(shadType);
  refl.debugInfo.entrySourceName = refl.entryPoint = "main";
  refl.encoding = ShaderEncoding::GLSL;
  refl.debugInfo.compiler = KnownShaderTool::Unknown;
  refl.debugInfo.encoding = ShaderEncoding::GLSL;

  if(shadType == eGL_COMPUTE_SHADER)
  {
    GL.glGetProgramiv(sepProg, eGL_COMPUTE_WORK_GROUP_SIZE,
                      (GLint *)refl.dispatchThreadsDimension.data());
  }
  else
  {
    RDCEraseEl(refl.dispatchThreadsDimension);
  }

  rdcarray<ShaderResource> &roresources = refl.readOnlyResources;
  rdcarray<ShaderResource> &rwresources = refl.readWriteResources;

  GLint numUniforms = 0;
  GL.glGetProgramInterfaceiv(sepProg, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

  const size_t numProps = 7;

  GLenum resProps[numProps] = {
      eGL_TYPE,       eGL_NAME_LENGTH, eGL_LOCATION,     eGL_BLOCK_INDEX,
      eGL_ARRAY_SIZE, eGL_OFFSET,      eGL_IS_ROW_MAJOR,
  };

  for(GLint u = 0; u < numUniforms; u++)
  {
    GLint values[numProps];
    GL.glGetProgramResourceiv(sepProg, eGL_UNIFORM, u, numProps, resProps, numProps, NULL, values);

    ShaderResource res;
    res.isReadOnly = true;
    res.isTexture = true;
    res.variableType.rows = 1;
    res.variableType.columns = 4;
    res.variableType.elements = 1;
    res.variableType.arrayByteStride = 0;
    res.variableType.matrixByteStride = 0;

    res.descriptorType = DescriptorType::ImageSampler;

    // float samplers
    if(values[0] == eGL_SAMPLER_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "samplerBuffer";
      res.variableType.baseType = VarType::Float;
      res.descriptorType = DescriptorType::TypedBuffer;
    }
    else if(values[0] == eGL_SAMPLER_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "sampler1D";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "sampler1DArray";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_SHADOW)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "sampler1DShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_ARRAY_SHADOW)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "sampler1DArrayShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "sampler2D";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "sampler2DArray";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_SHADOW)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "sampler2DShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_ARRAY_SHADOW)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "sampler2DArrayShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "sampler2DRect";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_RECT_SHADOW)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "sampler2DRectShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "sampler3D";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "samplerCube";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE_SHADOW)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "samplerCubeShadow";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "samplerCubeArray";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "sampler2DMS";
      res.variableType.baseType = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "sampler2DMSArray";
      res.variableType.baseType = VarType::Float;
    }
    // int samplers
    else if(values[0] == eGL_INT_SAMPLER_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "isamplerBuffer";
      res.variableType.baseType = VarType::SInt;
      res.descriptorType = DescriptorType::TypedBuffer;
    }
    else if(values[0] == eGL_INT_SAMPLER_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "isampler1D";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "isampler1DArray";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "isampler2D";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "isampler2DArray";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "isampler2DRect";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "isampler3D";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "isamplerCube";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "isamplerCubeArray";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "isampler2DMS";
      res.variableType.baseType = VarType::SInt;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "isampler2DMSArray";
      res.variableType.baseType = VarType::SInt;
    }
    // unsigned int samplers
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "usamplerBuffer";
      res.variableType.baseType = VarType::UInt;
      res.descriptorType = DescriptorType::TypedBuffer;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "usampler1D";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "usampler1DArray";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "usampler2D";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "usampler2DArray";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "usampler2DRect";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "usampler3D";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "usamplerCube";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "usamplerCubeArray";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "usampler2DMS";
      res.variableType.baseType = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "usampler2DMSArray";
      res.variableType.baseType = VarType::UInt;
    }
    // float images
    else if(values[0] == eGL_IMAGE_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "imageBuffer";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
      res.descriptorType = DescriptorType::ReadWriteTypedBuffer;
    }
    else if(values[0] == eGL_IMAGE_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "image1D";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "image1DArray";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "image2D";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "image2DArray";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "image2DRect";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "image3D";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "imageCube";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "imageCubeArray";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "image2DMS";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "image2DMSArray";
      res.variableType.baseType = VarType::Float;
      res.isReadOnly = false;
    }
    // int images
    else if(values[0] == eGL_INT_IMAGE_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "iimageBuffer";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
      res.descriptorType = DescriptorType::ReadWriteTypedBuffer;
    }
    else if(values[0] == eGL_INT_IMAGE_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "iimage1D";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "iimage1DArray";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "iimage2D";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "iimage2DArray";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "iimage2DRect";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "iimage3D";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "iimageCube";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "iimageCubeArray";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "iimage2DMS";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "iimage2DMSArray";
      res.variableType.baseType = VarType::SInt;
      res.isReadOnly = false;
    }
    // unsigned int images
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_BUFFER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "uimageBuffer";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
      res.descriptorType = DescriptorType::ReadWriteTypedBuffer;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_1D)
    {
      res.textureType = TextureType::Texture1D;
      res.variableType.name = "uimage1D";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_1D_ARRAY)
    {
      res.textureType = TextureType::Texture1DArray;
      res.variableType.name = "uimage1DArray";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D)
    {
      res.textureType = TextureType::Texture2D;
      res.variableType.name = "uimage2D";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_ARRAY)
    {
      res.textureType = TextureType::Texture2DArray;
      res.variableType.name = "uimage2DArray";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_RECT)
    {
      res.textureType = TextureType::TextureRect;
      res.variableType.name = "uimage2DRect";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_3D)
    {
      res.textureType = TextureType::Texture3D;
      res.variableType.name = "uimage3D";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_CUBE)
    {
      res.textureType = TextureType::TextureCube;
      res.variableType.name = "uimageCube";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY)
    {
      res.textureType = TextureType::TextureCubeArray;
      res.variableType.name = "uimageCubeArray";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE)
    {
      res.textureType = TextureType::Texture2DMS;
      res.variableType.name = "uimage2DMS";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.textureType = TextureType::Texture2DMSArray;
      res.variableType.name = "uimage2DMSArray";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
    }
    // atomic counter
    else if(values[0] == eGL_UNSIGNED_INT_ATOMIC_COUNTER)
    {
      res.textureType = TextureType::Buffer;
      res.variableType.name = "atomic_uint";
      res.variableType.baseType = VarType::UInt;
      res.isReadOnly = false;
      res.isTexture = false;
      res.variableType.columns = 1;
      res.descriptorType = DescriptorType::ReadWriteBuffer;
    }
    else
    {
      // not a sampler
      continue;
    }

    if(!res.isReadOnly && res.textureType == TextureType::Buffer)
      res.descriptorType = DescriptorType::ReadWriteImage;

    res.hasSampler = res.isReadOnly;

    char *namebuf = new char[values[1] + 1];
    GL.glGetProgramResourceName(sepProg, eGL_UNIFORM, u, values[1], NULL, namebuf);
    namebuf[values[1]] = 0;

    rdcstr name = namebuf;

    delete[] namebuf;

    res.name = name;

    rdcarray<ShaderResource> &reslist = (res.isReadOnly ? roresources : rwresources);

    reslist.push_back(res);

    // array of samplers
    if(values[4] > 1)
    {
      name = name.substr(0, name.length() - 3);    // trim off [0] on the end
      for(int i = 1; i < values[4]; i++)
      {
        rdcstr arrname = StringFormat::Fmt("%s[%d]", name.c_str(), i);

        res.name = arrname;

        reslist.push_back(res);
      }
    }
  }

  rdcarray<size_t> ssbos;
  uint32_t ssboMembers = 0;

  GLint numSSBOs = 0;
  if(HasExt[ARB_shader_storage_buffer_object])
  {
    GL.glGetProgramInterfaceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

    for(GLint u = 0; u < numSSBOs; u++)
    {
      GLenum propName = eGL_NAME_LENGTH;
      GLint len;
      GL.glGetProgramResourceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, u, 1, &propName, 1, NULL, &len);

      char *nm = new char[len + 1];
      GL.glGetProgramResourceName(sepProg, eGL_SHADER_STORAGE_BLOCK, u, len + 1, NULL, nm);

      ShaderResource res;
      res.isReadOnly = false;
      res.isTexture = false;
      res.textureType = TextureType::Buffer;
      res.variableType.rows = 0;
      res.variableType.columns = 0;
      res.variableType.elements = 1;
      res.variableType.arrayByteStride = 0;
      res.variableType.matrixByteStride = 0;
      res.variableType.name = "buffer";
      res.variableType.baseType = VarType::UInt;
      res.name = nm;
      res.descriptorType = DescriptorType::ReadWriteBuffer;

      GLint numMembers = 0;

      propName = eGL_NUM_ACTIVE_VARIABLES;
      GL.glGetProgramResourceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, u, 1, &propName, 1, NULL,
                                (GLint *)&numMembers);

      char *arr = strchr(nm, '[');
      const bool isArray = (arr != NULL);

      uint32_t arrayIdx = 0;
      if(isArray)
      {
        arr++;
        while(*arr >= '0' && *arr <= '9')
        {
          arrayIdx *= 10;
          arrayIdx += int(*arr) - int('0');
          arr++;
        }
      }

      ssbos.push_back(rwresources.size());
      rwresources.push_back(res);

      // only count members from the first array index
      if(!isArray || arrayIdx == 0)
        ssboMembers += numMembers;

      delete[] nm;
    }
  }

  {
    rdcarray<ShaderConstant> *members = new rdcarray<ShaderConstant>[ssbos.size()];

    for(uint32_t i = 0; i < ssboMembers; i++)
    {
      ReconstructVarTree(eGL_BUFFER_VARIABLE, sepProg, i, (GLint)ssbos.size(), members, NULL);
    }

    // if we have members in an array ssbo, broadcast this to any ssbo with the same basename
    // without members, since the variables will only be reported as belonging to one block
    for(size_t ssbo = 0; ssbo < ssbos.size(); ssbo++)
    {
      rdcstr basename = rwresources[ssbos[ssbo]].name;
      int arrOffs = basename.indexOf('[');
      if(arrOffs > 0)
      {
        basename.erase(arrOffs, basename.size());

        if(!members[ssbo].empty())
        {
          for(size_t ssbo2 = 0; ssbo2 < ssbos.size(); ssbo2++)
          {
            if(!members[ssbo2].empty())
              continue;

            rdcstr basename2 = rwresources[ssbos[ssbo2]].name;
            arrOffs = basename2.indexOf('[');
            if(arrOffs > 0)
            {
              basename2.erase(arrOffs, basename2.size());

              if(basename == basename2)
              {
                members[ssbo2] = members[ssbo];
              }
            }
          }
        }
      }
    }

    for(size_t ssbo = 0; ssbo < ssbos.size(); ssbo++)
    {
      sort(members[ssbo]);

      rdcstr basename = rwresources[ssbos[ssbo]].name;
      int arrOffs = basename.indexOf('[');
      if(arrOffs > 0)
        basename.erase(arrOffs, basename.size());

      if(!members[ssbo].empty() && basename == members[ssbo][0].name)
        std::swap(rwresources[ssbos[ssbo]].variableType.members, members[ssbo][0].type.members);
      else
        std::swap(rwresources[ssbos[ssbo]].variableType.members, members[ssbo]);
    }

    // patch-up reflection data. For top-level arrays use the stride & rough size to calculate the
    // number of elements, and make all child byteOffset values relative to their parent
    for(size_t ssbo = 0; ssbo < ssbos.size(); ssbo++)
    {
      rdcarray<ShaderConstant> &ssboVars = rwresources[ssbos[ssbo]].variableType.members;

      // can't make perfect guesses of struct alignment but assume std430 for ssbos
      for(ShaderConstant &member : ssboVars)
        FixupStructOffsetsAndSize(false, member);

      for(size_t rootMember = 0; rootMember + 1 < ssboVars.size(); rootMember++)
      {
        ShaderConstant &member = ssboVars[rootMember];

        const uint32_t memberSizeBound = ssboVars[rootMember + 1].byteOffset - member.byteOffset;
        const uint32_t stride = member.type.arrayByteStride;

        if(stride != 0 && member.type.elements == ~0U)
        {
          if(memberSizeBound >= 2 * stride)
            member.type.elements = memberSizeBound / stride;
          else
            member.type.elements = 1;
        }
      }
    }

    delete[] members;
  }

  rdcarray<ShaderConstant> globalUniforms;

  GLint numUBOs = 0;
  rdcarray<rdcstr> uboNames;
  rdcarray<ShaderConstant> *ubos = NULL;

  {
    GL.glGetProgramInterfaceiv(sepProg, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

    ubos = new rdcarray<ShaderConstant>[numUBOs];
    uboNames.resize(numUBOs);

    for(GLint u = 0; u < numUBOs; u++)
    {
      GLenum nameLen = eGL_NAME_LENGTH;
      GLint len;
      GL.glGetProgramResourceiv(sepProg, eGL_UNIFORM_BLOCK, u, 1, &nameLen, 1, NULL, &len);

      char *nm = new char[len + 1];
      GL.glGetProgramResourceName(sepProg, eGL_UNIFORM_BLOCK, u, len + 1, NULL, nm);
      uboNames[u] = nm;
      delete[] nm;
    }
  }

  for(GLint u = 0; u < numUniforms; u++)
  {
    ReconstructVarTree(eGL_UNIFORM, sepProg, u, numUBOs, ubos, &globalUniforms);
  }

  refl.constantBlocks.reserve(numUBOs + (globalUniforms.empty() ? 0 : 1));

  if(ubos)
  {
    for(int i = 0; i < numUBOs; i++)
    {
      if(!ubos[i].empty())
      {
        ConstantBlock cblock;
        cblock.name = uboNames[i];
        cblock.bufferBacked = true;

        GLenum bufSize = eGL_BUFFER_DATA_SIZE;
        GL.glGetProgramResourceiv(sepProg, eGL_UNIFORM_BLOCK, i, 1, &bufSize, 1, NULL,
                                  (GLint *)&cblock.byteSize);

        sort(ubos[i]);

        // can't make perfect guesses of struct alignment but assume std140 for ubos
        for(ShaderConstant &member : ubos[i])
          FixupStructOffsetsAndSize(true, member);

        std::swap(cblock.variables, ubos[i]);

        refl.constantBlocks.push_back(cblock);
      }
    }
  }

  if(!globalUniforms.empty())
  {
    ConstantBlock globals;
    globals.name = "$Globals";
    globals.bufferBacked = false;

    // global uniforms have no defined order, location will be per implementation, so sort instead
    // alphabetically
    namesort(globalUniforms);
    std::swap(globals.variables, globalUniforms);

    refl.constantBlocks.push_back(globals);
  }

  delete[] ubos;

  for(int sigType = 0; sigType < 2; sigType++)
  {
    GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);
    rdcarray<SigParameter> *sigArray = (sigType == 0 ? &refl.inputSignature : &refl.outputSignature);

    GLint numInputs;
    GL.glGetProgramInterfaceiv(sepProg, sigEnum, eGL_ACTIVE_RESOURCES, &numInputs);

    if(numInputs > 0)
    {
      rdcarray<SigParameter> sigs;
      sigs.reserve(numInputs);

      uint32_t regIndex = 0;

      for(GLint i = 0; i < numInputs; i++)
      {
        GLenum props[] = {eGL_NAME_LENGTH, eGL_TYPE, eGL_LOCATION, eGL_ARRAY_SIZE,
                          eGL_LOCATION_COMPONENT};
        GLint values[] = {0, 0, 0, 0, 0};

        GLsizei numSigProps = (GLsizei)ARRAY_COUNT(props);

        // GL_LOCATION_COMPONENT not supported on core <4.4 (or without GL_ARB_enhanced_layouts)
        // on GLES, or when we don't have native program interface query
        if(!HasExt[ARB_enhanced_layouts] || !HasExt[ARB_program_interface_query])
          numSigProps--;
        GL.glGetProgramResourceiv(sepProg, sigEnum, i, numSigProps, props, numSigProps, NULL, values);

        char *nm = new char[values[0] + 1];
        GL.glGetProgramResourceName(sepProg, sigEnum, i, values[0] + 1, NULL, nm);

        SigParameter sig;

        sig.varName = nm;
        sig.semanticIndex = 0;
        sig.needSemanticIndex = false;
        sig.stream = 0;

        int rows = 1;

        switch(values[1])
        {
          case eGL_DOUBLE:
          case eGL_DOUBLE_VEC2:
          case eGL_DOUBLE_VEC3:
          case eGL_DOUBLE_VEC4:
          case eGL_DOUBLE_MAT4:
          case eGL_DOUBLE_MAT4x3:
          case eGL_DOUBLE_MAT4x2:
          case eGL_DOUBLE_MAT3:
          case eGL_DOUBLE_MAT3x4:
          case eGL_DOUBLE_MAT3x2:
          case eGL_DOUBLE_MAT2:
          case eGL_DOUBLE_MAT2x3:
          case eGL_DOUBLE_MAT2x4: sig.varType = VarType::Double; break;
          case eGL_FLOAT:
          case eGL_FLOAT_VEC2:
          case eGL_FLOAT_VEC3:
          case eGL_FLOAT_VEC4:
          case eGL_FLOAT_MAT4:
          case eGL_FLOAT_MAT4x3:
          case eGL_FLOAT_MAT4x2:
          case eGL_FLOAT_MAT3:
          case eGL_FLOAT_MAT3x4:
          case eGL_FLOAT_MAT3x2:
          case eGL_FLOAT_MAT2:
          case eGL_FLOAT_MAT2x3:
          case eGL_FLOAT_MAT2x4: sig.varType = VarType::Float; break;
          case eGL_INT:
          case eGL_INT_VEC2:
          case eGL_INT_VEC3:
          case eGL_INT_VEC4: sig.varType = VarType::SInt; break;
          case eGL_UNSIGNED_INT:
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_UNSIGNED_INT_VEC4: sig.varType = VarType::UInt; break;
          case eGL_BOOL:
          case eGL_BOOL_VEC2:
          case eGL_BOOL_VEC3:
          case eGL_BOOL_VEC4: sig.varType = VarType::Bool; break;
          default:
            sig.varType = VarType::Float;
            RDCWARN("Unhandled signature element type %s", ToStr((GLenum)values[1]).c_str());
        }

        switch(values[1])
        {
          case eGL_FLOAT:
          case eGL_DOUBLE:
          case eGL_INT:
          case eGL_UNSIGNED_INT:
          case eGL_BOOL:
            sig.compCount = 1;
            sig.regChannelMask = 0x1;
            break;
          case eGL_FLOAT_VEC2:
          case eGL_DOUBLE_VEC2:
          case eGL_INT_VEC2:
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2:
            sig.compCount = 2;
            sig.regChannelMask = 0x3;
            break;
          case eGL_FLOAT_VEC3:
          case eGL_DOUBLE_VEC3:
          case eGL_INT_VEC3:
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3:
            sig.compCount = 3;
            sig.regChannelMask = 0x7;
            break;
          case eGL_FLOAT_VEC4:
          case eGL_DOUBLE_VEC4:
          case eGL_INT_VEC4:
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4:
            sig.compCount = 4;
            sig.regChannelMask = 0xf;
            break;
          case eGL_FLOAT_MAT4:
          case eGL_DOUBLE_MAT4:
            sig.compCount = 4;
            rows = 4;
            sig.regChannelMask = 0xf;
            break;
          case eGL_FLOAT_MAT4x3:
          case eGL_DOUBLE_MAT4x3:
            sig.compCount = 4;
            rows = 3;
            sig.regChannelMask = 0xf;
            break;
          case eGL_FLOAT_MAT4x2:
          case eGL_DOUBLE_MAT4x2:
            sig.compCount = 4;
            rows = 2;
            sig.regChannelMask = 0xf;
            break;
          case eGL_FLOAT_MAT3:
          case eGL_DOUBLE_MAT3:
            sig.compCount = 3;
            rows = 3;
            sig.regChannelMask = 0x7;
            break;
          case eGL_FLOAT_MAT3x4:
          case eGL_DOUBLE_MAT3x4:
            sig.compCount = 3;
            rows = 4;
            sig.regChannelMask = 0x7;
            break;
          case eGL_FLOAT_MAT3x2:
          case eGL_DOUBLE_MAT3x2:
            sig.compCount = 3;
            rows = 2;
            sig.regChannelMask = 0x7;
            break;
          case eGL_FLOAT_MAT2:
          case eGL_DOUBLE_MAT2:
            sig.compCount = 2;
            rows = 2;
            sig.regChannelMask = 0x3;
            break;
          case eGL_FLOAT_MAT2x3:
          case eGL_DOUBLE_MAT2x3:
            sig.compCount = 2;
            rows = 3;
            sig.regChannelMask = 0x3;
            break;
          case eGL_FLOAT_MAT2x4:
          case eGL_DOUBLE_MAT2x4:
            sig.compCount = 2;
            rows = 4;
            sig.regChannelMask = 0x3;
            break;
          default:
            RDCWARN("Unhandled signature element type %s", ToStr((GLenum)values[1]).c_str());
            sig.compCount = 4;
            sig.regChannelMask = 0xf;
            break;
        }

        sig.systemValue = ShaderBuiltin::Undefined;

        const char *varname = nm;

        if(!strncmp(varname, "gl_PerVertex.", 13))
          varname += 13;

#define IS_BUILTIN(builtin) !strncmp(varname, builtin, sizeof(builtin) - 1)

        // some vertex outputs can be reflected (especially by glslang) if they're just declared and
        // not used, which is quite common with redeclaring outputs for separable programs - either
        // by the program or by us. So instead use our manual quick-and-dirty usage check to skip
        // potential false-positives.
        bool unused = false;
        for(FFVertexOutput ffoutput : ::values<FFVertexOutput>())
        {
          // we consider an output used if we encounter a '=' before either a ';' or the end of the
          // string
          rdcstr outName = ToStr(ffoutput);

          // we do a substring search so that gl_ClipDistance matches gl_ClipDistance[0]
          if(strstr(varname, outName.c_str()))
          {
            unused = !outputUsage.used[(int)ffoutput];
            break;
          }
        }

        if(unused)
        {
          delete[] nm;
          continue;
        }

        // VS built-in inputs
        if(IS_BUILTIN("gl_VertexID"))
          sig.systemValue = ShaderBuiltin::VertexIndex;
        if(IS_BUILTIN("gl_InstanceID"))
          sig.systemValue = ShaderBuiltin::InstanceIndex;
        if(IS_BUILTIN("gl_BaseVertex"))
          sig.systemValue = ShaderBuiltin::BaseVertex;
        if(IS_BUILTIN("gl_BaseInstance"))
          sig.systemValue = ShaderBuiltin::BaseInstance;
        if(IS_BUILTIN("gl_DrawID"))
          sig.systemValue = ShaderBuiltin::DrawIndex;

        // VS built-in outputs
        if(IS_BUILTIN("gl_Position"))
          sig.systemValue = ShaderBuiltin::Position;
        if(IS_BUILTIN("gl_PointSize"))
          sig.systemValue = ShaderBuiltin::PointSize;
        if(IS_BUILTIN("gl_ClipDistance"))
          sig.systemValue = ShaderBuiltin::ClipDistance;

        // TCS built-in inputs
        if(IS_BUILTIN("gl_PatchVerticesIn"))
          sig.systemValue = ShaderBuiltin::PatchNumVertices;
        if(IS_BUILTIN("gl_PrimitiveID"))
          sig.systemValue = ShaderBuiltin::PrimitiveIndex;
        if(IS_BUILTIN("gl_InvocationID"))
          sig.systemValue = ShaderBuiltin::OutputControlPointIndex;

        // TCS built-in outputs
        if(IS_BUILTIN("gl_TessLevelOuter"))
          sig.systemValue = ShaderBuiltin::OuterTessFactor;
        if(IS_BUILTIN("gl_TessLevelInner"))
          sig.systemValue = ShaderBuiltin::InsideTessFactor;

        // TES built-in inputs
        if(IS_BUILTIN("gl_TessCoord"))
          sig.systemValue = ShaderBuiltin::DomainLocation;
        if(IS_BUILTIN("gl_PatchVerticesIn"))
          sig.systemValue = ShaderBuiltin::PatchNumVertices;
        if(IS_BUILTIN("gl_PrimitiveID"))
          sig.systemValue = ShaderBuiltin::PrimitiveIndex;

        // GS built-in inputs
        if(IS_BUILTIN("gl_PrimitiveIDIn"))
          sig.systemValue = ShaderBuiltin::PrimitiveIndex;
        if(IS_BUILTIN("gl_InvocationID") && shadType == eGL_GEOMETRY_SHADER)
          sig.systemValue = ShaderBuiltin::GSInstanceIndex;
        if(IS_BUILTIN("gl_Layer"))
          sig.systemValue = ShaderBuiltin::RTIndex;
        if(IS_BUILTIN("gl_ViewID_OVR"))
          sig.systemValue = ShaderBuiltin::MultiViewIndex;
        if(IS_BUILTIN("gl_ViewportIndex"))
          sig.systemValue = ShaderBuiltin::ViewportIndex;

        // GS built-in outputs
        if(IS_BUILTIN("gl_Layer"))
          sig.systemValue = ShaderBuiltin::RTIndex;
        if(IS_BUILTIN("gl_ViewportIndex"))
          sig.systemValue = ShaderBuiltin::ViewportIndex;

        // PS built-in inputs
        if(IS_BUILTIN("gl_FragCoord"))
          sig.systemValue = ShaderBuiltin::Position;
        if(IS_BUILTIN("gl_FrontFacing"))
          sig.systemValue = ShaderBuiltin::IsFrontFace;
        if(IS_BUILTIN("gl_PointCoord"))
          sig.systemValue = ShaderBuiltin::RTIndex;
        if(IS_BUILTIN("gl_SampleID"))
          sig.systemValue = ShaderBuiltin::MSAASampleIndex;
        if(IS_BUILTIN("gl_SamplePosition"))
          sig.systemValue = ShaderBuiltin::MSAASamplePosition;
        if(IS_BUILTIN("gl_SampleMaskIn"))
          sig.systemValue = ShaderBuiltin::MSAACoverage;

        // PS built-in outputs
        if(IS_BUILTIN("gl_FragDepth"))
          sig.systemValue = ShaderBuiltin::DepthOutput;
        if(IS_BUILTIN("gl_SampleMask"))
          sig.systemValue = ShaderBuiltin::MSAACoverage;
        if(IS_BUILTIN("gl_FragStencilRefARB"))
          sig.systemValue = ShaderBuiltin::StencilReference;

        // CS built-in inputs
        if(IS_BUILTIN("gl_NumWorkGroups"))
          sig.systemValue = ShaderBuiltin::DispatchSize;
        if(IS_BUILTIN("gl_WorkGroupID"))
          sig.systemValue = ShaderBuiltin::GroupIndex;
        if(IS_BUILTIN("gl_LocalInvocationID"))
          sig.systemValue = ShaderBuiltin::GroupThreadIndex;
        if(IS_BUILTIN("gl_GlobalInvocationID"))
          sig.systemValue = ShaderBuiltin::DispatchThreadIndex;
        if(IS_BUILTIN("gl_LocalInvocationIndex"))
          sig.systemValue = ShaderBuiltin::GroupFlatIndex;

#undef IS_BUILTIN
        if(sig.systemValue == ShaderBuiltin::Undefined)
          sig.regIndex = values[2] >= 0 ? values[2] : ~0U;
        else
          sig.regIndex = 0;

        if(shadType == eGL_FRAGMENT_SHADER && sigEnum == eGL_PROGRAM_OUTPUT &&
           sig.systemValue == ShaderBuiltin::Undefined)
          sig.systemValue = ShaderBuiltin::ColorOutput;

        // don't apply location component for built-ins
        if(sig.systemValue == ShaderBuiltin::Undefined)
          sig.regChannelMask <<= values[4];

        sig.channelUsedMask = sig.regChannelMask;

        if(values[3] <= 1)
        {
          AddSigParameter(sigs, regIndex, sig, nm, rows, -1);
        }
        else
        {
          rdcstr basename = nm;
          if(basename[basename.size() - 3] == '[' && basename[basename.size() - 2] == '0' &&
             basename[basename.size() - 1] == ']')
          {
            basename.resize(basename.size() - 3);
            for(int a = 0; a < values[3]; a++)
              AddSigParameter(sigs, regIndex, sig, basename.c_str(), rows, a);
          }
          else
          {
            RDCWARN("Got signature parameter %s with array size %d but no [0] suffix", nm, values[3]);
            AddSigParameter(sigs, regIndex, sig, nm, rows, -1);
          }
        }

        delete[] nm;
      }
      struct sig_param_sort
      {
        bool operator()(const SigParameter &a, const SigParameter &b)
        {
          if(a.systemValue == b.systemValue)
          {
            if(a.regIndex != b.regIndex)
              return a.regIndex < b.regIndex;

            return a.varName < b.varName;
          }

          if(a.systemValue == ShaderBuiltin::Undefined)
            return false;
          if(b.systemValue == ShaderBuiltin::Undefined)
            return true;

          return a.systemValue < b.systemValue;
        }
      };

      std::sort(sigs.begin(), sigs.end(), sig_param_sort());

      *sigArray = sigs;
    }
  }

  // TODO: fill in Interfaces with shader subroutines?
}

void EvaluateVertexAttributeBinds(GLuint curProg, const ShaderReflection *refl, bool spirv,
                                  rdcarray<int32_t> &vertexAttrBindings)
{
  GLint numVAttribBindings = 16;
  GL.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

  vertexAttrBindings.resize(numVAttribBindings);
  for(int32_t i = 0; i < numVAttribBindings; i++)
    vertexAttrBindings[i] = -1;

  if(!refl)
    return;

  if(spirv)
  {
    for(size_t i = 0; i < refl->inputSignature.size(); i++)
      if(refl->inputSignature[i].systemValue == ShaderBuiltin::Undefined)

        return;
  }

  for(int32_t i = 0; i < refl->inputSignature.count(); i++)
  {
    // skip system inputs, as some drivers will return a location for them
    if(refl->inputSignature[i].systemValue != ShaderBuiltin::Undefined)
      continue;

    // SPIR-V has fixed bindings
    if(spirv)
    {
      vertexAttrBindings[refl->inputSignature[i].regIndex] = (int32_t)i;
      continue;
    }

    int32_t matrixRow = 0;
    rdcstr varName = refl->inputSignature[i].varName;

    int32_t offs = varName.find(":col");
    if(offs >= 0)
    {
      matrixRow = varName[offs + 4] - '0';
      varName.resize(offs);
    }

    GLint loc = GL.glGetAttribLocation(curProg, varName.c_str());

    if(loc >= 0 && loc < numVAttribBindings)
    {
      vertexAttrBindings[loc + matrixRow] = i;
    }
  }
}

void GetCurrentBinding(GLuint curProg, ShaderReflection *refl, const ShaderResource &resource,
                       uint32_t &slot, bool &used)
{
  // in case of bugs, we readback into this array instead of a single int
  GLint dummyReadback[32];

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    dummyReadback[i] = 0x6c7b8a9d;
#endif

  const GLenum refEnums[] = {
      eGL_REFERENCED_BY_VERTEX_SHADER,          eGL_REFERENCED_BY_TESS_CONTROL_SHADER,
      eGL_REFERENCED_BY_TESS_EVALUATION_SHADER, eGL_REFERENCED_BY_GEOMETRY_SHADER,
      eGL_REFERENCED_BY_FRAGMENT_SHADER,        eGL_REFERENCED_BY_COMPUTE_SHADER,
  };

  const GLenum refEnum = refEnums[(uint32_t)refl->stage];

  const GLenum atomicRefEnums[] = {
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER,
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER,
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER,
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER,
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER,
      eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER,
  };

  const GLenum atomicRefEnum = atomicRefEnums[(uint32_t)refl->stage];

  if(refl->encoding == ShaderEncoding::OpenGLSPIRV)
  {
    if(resource.isTexture && resource.fixedBindNumber != ~0U)
    {
      GL.glGetUniformiv(curProg, resource.fixedBindNumber, dummyReadback);
      slot = dummyReadback[0];
      used = true;
    }
  }
  else if(resource.isReadOnly)
  {
    if(resource.isTexture)
    {
      // normal sampler or image load/store

      GLint loc = GL.glGetUniformLocation(curProg, resource.name.c_str());
      if(loc >= 0)
      {
        GL.glGetUniformiv(curProg, loc, dummyReadback);
        slot = dummyReadback[0];
      }
      else
      {
        slot = 0;
      }

      // handle sampler arrays, use the base name
      rdcstr name = resource.name;
      if(name.back() == ']')
      {
        do
        {
          name.pop_back();
        } while(name.back() != '[');
        name.pop_back();
      }

      GLuint idx = 0;
      idx = GL.glGetProgramResourceIndex(curProg, eGL_UNIFORM, name.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        used = false;
      }
      else
      {
        GLint glUsed = 0;
        GL.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &refEnum, 1, NULL, &glUsed);
        used = (glUsed != 0);
      }
    }
    else
    {
      slot = 0;
      used = false;
    }
  }
  else
  {
    if(resource.isTexture)
    {
      // image load/store

      GLint loc = GL.glGetUniformLocation(curProg, resource.name.c_str());
      if(loc >= 0)
      {
        GL.glGetUniformiv(curProg, loc, dummyReadback);
        slot = dummyReadback[0];
      }
      else
      {
        slot = 0;
      }

      // handle sampler arrays, use the base name
      rdcstr name = resource.name;
      if(name.back() == ']')
      {
        do
        {
          name.pop_back();
        } while(name.back() != '[');
        name.pop_back();
      }

      GLuint idx = 0;
      idx = GL.glGetProgramResourceIndex(curProg, eGL_UNIFORM, name.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        used = false;
      }
      else
      {
        GLint glUsed = 0;
        GL.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &refEnum, 1, NULL, &glUsed);
        used = (glUsed != 0);
      }
    }
    else
    {
      if(resource.variableType.columns == 1 && resource.variableType.rows == 1 &&
         resource.variableType.baseType == VarType::UInt)
      {
        // atomic uint
        GLuint idx = GL.glGetProgramResourceIndex(curProg, eGL_UNIFORM, resource.name.c_str());

        if(idx == GL_INVALID_INDEX)
        {
          slot = 0;
          used = false;
        }
        else
        {
          GLenum prop = eGL_ATOMIC_COUNTER_BUFFER_INDEX;
          GLuint atomicIndex;
          GL.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &prop, 1, NULL,
                                    (GLint *)&atomicIndex);

          if(atomicIndex == GL_INVALID_INDEX)
          {
            slot = 0;
            used = false;
          }
          else
          {
            if(IsGLES)
            {
              prop = eGL_BUFFER_BINDING;
              GL.glGetProgramResourceiv(curProg, eGL_ATOMIC_COUNTER_BUFFER, atomicIndex, 1, &prop,
                                        1, NULL, (GLint *)&slot);
              GLint glUsed = 0;
              GL.glGetProgramResourceiv(curProg, eGL_ATOMIC_COUNTER_BUFFER, atomicIndex, 1,
                                        &refEnum, 1, NULL, &glUsed);
              used = (glUsed != 0);
            }
            else
            {
              GL.glGetActiveAtomicCounterBufferiv(
                  curProg, atomicIndex, eGL_ATOMIC_COUNTER_BUFFER_BINDING, (GLint *)&slot);
              GLint glUsed = 0;
              GL.glGetActiveAtomicCounterBufferiv(curProg, atomicIndex, atomicRefEnum, &glUsed);
              used = (glUsed != 0);
            }
          }
        }
      }
      else
      {
        // shader storage buffer object
        GLuint idx =
            GL.glGetProgramResourceIndex(curProg, eGL_SHADER_STORAGE_BLOCK, resource.name.c_str());

        if(idx == GL_INVALID_INDEX)
        {
          slot = 0;
          used = false;
        }
        else
        {
          GLenum prop = eGL_BUFFER_BINDING;
          GL.glGetProgramResourceiv(curProg, eGL_SHADER_STORAGE_BLOCK, idx, 1, &prop, 1, NULL,
                                    (GLint *)&slot);
          GLint glUsed = 0;
          GL.glGetProgramResourceiv(curProg, eGL_SHADER_STORAGE_BLOCK, idx, 1, &refEnum, 1, NULL,
                                    &glUsed);
          used = (glUsed != 0);
        }
      }
    }
  }

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    if(dummyReadback[i] != 0x6c7b8a9d)
      RDCERR("Invalid uniform readback - data beyond first element modified!");
#endif
}

void GetCurrentBinding(GLuint curProg, ShaderReflection *refl, const ConstantBlock &cblock,
                       uint32_t &slot, bool &used)
{
  if(refl->encoding == ShaderEncoding::OpenGLSPIRV)
  {
    // It's fuzzy on whether UBOs can be remapped with glUniformBlockBinding so for now we hope that
    // anyone using UBOs and SPIR-V will at least specify immutable bindings in the SPIR-V.
    return;
  }

  // in case of bugs, we readback into this array instead of a single int
  GLint dummyReadback[32];

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    dummyReadback[i] = 0x6c7b8a9d;
#endif

  const GLenum refEnums[] = {
      eGL_REFERENCED_BY_VERTEX_SHADER,          eGL_REFERENCED_BY_TESS_CONTROL_SHADER,
      eGL_REFERENCED_BY_TESS_EVALUATION_SHADER, eGL_REFERENCED_BY_GEOMETRY_SHADER,
      eGL_REFERENCED_BY_FRAGMENT_SHADER,        eGL_REFERENCED_BY_COMPUTE_SHADER,
  };

  const GLenum refEnum = refEnums[(uint32_t)refl->stage];

  if(cblock.bufferBacked)
  {
    GLint loc = GL.glGetUniformBlockIndex(curProg, cblock.name.c_str());
    if(loc >= 0)
    {
      GL.glGetActiveUniformBlockiv(curProg, loc, eGL_UNIFORM_BLOCK_BINDING, dummyReadback);
      slot = dummyReadback[0];
    }
  }
  else
  {
    slot = 0;
  }

  if(!cblock.bufferBacked)
  {
    used = true;
  }
  else
  {
    GLuint idx = GL.glGetProgramResourceIndex(curProg, eGL_UNIFORM_BLOCK, cblock.name.c_str());
    if(idx == GL_INVALID_INDEX)
    {
      used = false;
    }
    else
    {
      GLint glUsed = 0;
      GL.glGetProgramResourceiv(curProg, eGL_UNIFORM_BLOCK, idx, 1, &refEnum, 1, NULL, &glUsed);
      used = (glUsed != 0);
    }
  }

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    if(dummyReadback[i] != 0x6c7b8a9d)
      RDCERR("Invalid uniform readback - data beyond first element modified!");
#endif
}
