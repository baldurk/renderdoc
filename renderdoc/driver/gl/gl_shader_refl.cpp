/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2017 Baldur Karlsson
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
#include "gl_driver.h"

// declare versions of ShaderConstant/ShaderVariableType with vectors
// to more easily build up the members of nested structures
struct DynShaderConstant;

struct DynShaderVariableType
{
  struct
  {
    VarType type;
    uint32_t rows;
    uint32_t cols;
    uint32_t elements;
    bool32 rowMajorStorage;
    uint32_t arrayStride;
    string name;
  } descriptor;

  vector<DynShaderConstant> members;
};

struct DynShaderConstant
{
  string name;
  struct
  {
    uint32_t vec;
    uint32_t comp;
  } reg;
  DynShaderVariableType type;
};

void sort(vector<DynShaderConstant> &vars)
{
  if(vars.empty())
    return;

  struct offset_sort
  {
    bool operator()(const DynShaderConstant &a, const DynShaderConstant &b)
    {
      if(a.reg.vec == b.reg.vec)
        return a.reg.comp < b.reg.comp;
      else
        return a.reg.vec < b.reg.vec;
    }
  };

  std::sort(vars.begin(), vars.end(), offset_sort());

  for(size_t i = 0; i < vars.size(); i++)
    sort(vars[i].type.members);
}

void copy(rdctype::array<ShaderConstant> &outvars, const vector<DynShaderConstant> &invars)
{
  if(invars.empty())
  {
    RDCEraseEl(outvars);
    return;
  }

  create_array_uninit(outvars, invars.size());
  for(size_t i = 0; i < invars.size(); i++)
  {
    outvars[i].name = invars[i].name;
    outvars[i].reg.vec = invars[i].reg.vec;
    outvars[i].reg.comp = invars[i].reg.comp;
    outvars[i].defaultValue = 0;
    outvars[i].type.descriptor.type = invars[i].type.descriptor.type;
    outvars[i].type.descriptor.rows = invars[i].type.descriptor.rows;
    outvars[i].type.descriptor.cols = invars[i].type.descriptor.cols;
    outvars[i].type.descriptor.elements = invars[i].type.descriptor.elements;
    outvars[i].type.descriptor.rowMajorStorage = invars[i].type.descriptor.rowMajorStorage;
    outvars[i].type.descriptor.arrayStride = invars[i].type.descriptor.arrayStride;
    outvars[i].type.descriptor.name = invars[i].type.descriptor.name;
    copy(outvars[i].type.members, invars[i].type.members);
  }
}

void CheckVertexOutputUses(const vector<string> &sources, bool &pointSizeUsed, bool &clipDistanceUsed)
{
  pointSizeUsed = false;
  clipDistanceUsed = false;

  for(size_t i = 0; i < sources.size(); i++)
  {
    const string &s = sources[i];

    size_t offs = 0;

    for(;;)
    {
      offs = s.find("gl_PointSize", offs);

      if(offs == string::npos)
        break;

      // consider gl_PointSize used if we encounter a '=' before a ';' or the end of the string

      while(offs < s.length())
      {
        if(s[offs] == '=')
        {
          pointSizeUsed = true;
          break;
        }

        if(s[offs] == ';')
          break;

        offs++;
      }
    }

    offs = 0;

    for(;;)
    {
      offs = s.find("gl_ClipDistance", offs);

      if(offs == string::npos)
        break;

      // consider gl_ClipDistance used if we encounter a '=' before a ';' or the end of the string

      while(offs < s.length())
      {
        if(s[offs] == '=')
        {
          clipDistanceUsed = true;
          break;
        }

        if(s[offs] == ';')
          break;

        offs++;
      }
    }
  }
}

// little utility function that if necessary emulates glCreateShaderProgramv functionality but using
// glCompileShaderIncludeARB
static GLuint CreateSepProgram(WrappedOpenGL &gl, GLenum type, GLsizei numSources,
                               const char **sources, GLsizei numPaths, const char **paths)
{
  // by the nature of this function, it might fail - we don't want to spew
  // false positive looking messages into the log.
  gl.SuppressDebugMessages(true);

  const GLHookSet &real = gl.GetHookset();

  GLuint program = 0;

  // definition of glCreateShaderProgramv from the spec
  GLuint shader = real.glCreateShader(type);
  if(shader)
  {
    real.glShaderSource(shader, numSources, sources, NULL);

    if(paths == NULL)
      real.glCompileShader(shader);
    else
      real.glCompileShaderIncludeARB(shader, numPaths, paths, NULL);

    program = real.glCreateProgram();
    if(program)
    {
      GLint compiled = 0;

      real.glGetShaderiv(shader, eGL_COMPILE_STATUS, &compiled);
      real.glProgramParameteri(program, eGL_PROGRAM_SEPARABLE, GL_TRUE);

      if(compiled)
      {
        real.glAttachShader(program, shader);
        real.glLinkProgram(program);

        // we deliberately leave the shaders attached so this program can be re-linked.
        // they will be cleaned up when the program is deleted
        // gl.glDetachShader(program, shader);
      }
    }
    real.glDeleteShader(shader);
  }

  gl.SuppressDebugMessages(false);
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

GLuint MakeSeparableShaderProgram(WrappedOpenGL &gl, GLenum type, vector<string> sources,
                                  vector<string> *includepaths)
{
  // in and out blocks are added separately, in case one is there already
  const char *blockIdentifiers[2] = {"in gl_PerVertex", "out gl_PerVertex"};
  string blocks[2] = {"", ""};

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
  if(includepaths)
  {
    numPaths = (GLsizei)includepaths->size();

    paths = new const char *[includepaths->size()];
    for(size_t i = 0; i < includepaths->size(); i++)
      paths[i] = (*includepaths)[i].c_str();
  }

  GLuint sepProg = CreateSepProgram(gl, type, (GLsizei)sources.size(), strings, numPaths, paths);

  GLint status;
  gl.glGetProgramiv(sepProg, eGL_LINK_STATUS, &status);

  // allow any vertex processing shader to redeclare gl_PerVertex
  // on GLES it is not required
  if(!IsGLES && status == 0 && type != eGL_FRAGMENT_SHADER && type != eGL_COMPUTE_SHADER)
  {
    gl.glDeleteProgram(sepProg);
    sepProg = 0;

    // try and patch up shader
    // naively insert gl_PerVertex block as soon as it's valid (after #version)
    // this will fail if e.g. a member of gl_PerVertex is declared at global scope
    // (this is probably most likely for clipdistance if it's redeclared with a size)

    // these strings contain whichever source string we replaced, here to scope until
    // the program has been created
    string subStrings[2];

    for(int blocktype = 0; blocktype < 2; blocktype++)
    {
      // vertex shaders don't have an in block
      if(type == eGL_VERTEX_SHADER && blocktype == 0)
        continue;

      string &substituted = subStrings[blocktype];

      string block = blocks[blocktype];
      const char *identifier = blockIdentifiers[blocktype];

      bool already = false;

      for(size_t i = 0; i < sources.size(); i++)
      {
        // if we find the 'identifier' (ie. the block name),
        // assume this block is already present and stop
        if(sources[i].find(identifier) != string::npos)
        {
          already = true;
          break;
        }
      }

      // only try and insert this block if the shader doesn't already have it
      if(already)
        continue;

      for(size_t i = 0; i < sources.size(); i++)
      {
        string src = strings[i];

        size_t len = src.length();

        // find if this source contains a #version, accounting for whitespace
        size_t it = 0;

        while(it != string::npos)
        {
          it = src.find("#", it);

          if(it == string::npos)
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
        if(it == string::npos)
          continue;

        // it now points after the #version

        // skip whitespace
        while(it < len && isspacetab(src[it]))
          ++it;

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
          if(it + sizeof(precision) < len && !strncmp(&src[it], precision, sizeof(precision) - 1))
          {
            // since we're speculating here (although what else could it be?) we don't modify
            // it until we're sure.
            size_t pit = it + sizeof(precision);

            // skip whitespace
            while(pit < len && isspacetab(src[pit]))
              ++pit;

            // if we now match any of the precisions, then continue consuming until the next ;
            const char lowp[] = "lowp";
            const char mediump[] = "mediump";
            const char highp[] = "highp";

            bool precisionMatch =
                (pit + sizeof(lowp) < len && !strncmp(&src[pit], lowp, sizeof(lowp) - 1) &&
                 isspacetab(src[pit + sizeof(lowp) - 1]));
            precisionMatch |=
                (pit + sizeof(mediump) < len && !strncmp(&src[pit], mediump, sizeof(mediump) - 1) &&
                 isspacetab(src[pit + sizeof(mediump) - 1]));
            precisionMatch |=
                (pit + sizeof(highp) < len && !strncmp(&src[pit], highp, sizeof(highp) - 1) &&
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

        substituted = src;

        substituted.insert(it, block);

        strings[i] = substituted.c_str();

        break;
      }
    }

    sepProg = CreateSepProgram(gl, type, (GLsizei)sources.size(), strings, numPaths, paths);
  }

  gl.glGetProgramiv(sepProg, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    char buffer[1025] = {0};
    gl.glGetProgramInfoLog(sepProg, 1024, NULL, buffer);

    RDCERR("Couldn't make separable shader program for shader. Errors:\n%s", buffer);

    gl.glDeleteProgram(sepProg);
    sepProg = 0;
  }

  delete[] strings;
  if(paths)
    delete[] paths;

  return sepProg;
}

void ReconstructVarTree(const GLHookSet &gl, GLenum query, GLuint sepProg, GLuint varIdx,
                        GLint numParentBlocks, vector<DynShaderConstant> *parentBlocks,
                        vector<DynShaderConstant> *defaultBlock)
{
  const size_t numProps = 8;

  GLenum resProps[numProps] = {eGL_TYPE,       eGL_NAME_LENGTH, eGL_LOCATION,     eGL_BLOCK_INDEX,
                               eGL_ARRAY_SIZE, eGL_OFFSET,      eGL_IS_ROW_MAJOR, eGL_ARRAY_STRIDE};

  // GL_LOCATION not valid for buffer variables (it's only used if offset comes back -1, which will
  // never
  // happen for buffer variables)
  if(query == eGL_BUFFER_VARIABLE)
    resProps[2] = eGL_OFFSET;

  GLint values[numProps] = {-1, -1, -1, -1, -1, -1, -1, -1};
  gl.glGetProgramResourceiv(sepProg, query, varIdx, numProps, resProps, numProps, NULL, values);

  DynShaderConstant var;

  var.type.descriptor.elements = RDCMAX(1, values[4]);

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
    case eGL_FLOAT_MAT2x3: var.type.descriptor.type = VarType::Float; break;
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
    case eGL_DOUBLE_MAT2x3: var.type.descriptor.type = VarType::Double; break;
    case eGL_UNSIGNED_INT_VEC4:
    case eGL_UNSIGNED_INT_VEC3:
    case eGL_UNSIGNED_INT_VEC2:
    case eGL_UNSIGNED_INT:
    case eGL_BOOL_VEC4:
    case eGL_BOOL_VEC3:
    case eGL_BOOL_VEC2:
    case eGL_BOOL: var.type.descriptor.type = VarType::UInt; break;
    case eGL_INT_VEC4:
    case eGL_INT_VEC3:
    case eGL_INT_VEC2:
    case eGL_INT: var.type.descriptor.type = VarType::Int; break;
    default:
      // not a variable (sampler etc)
      return;
  }

  // set # rows if it's a matrix
  var.type.descriptor.rows = 1;

  switch(values[0])
  {
    case eGL_FLOAT_MAT4:
    case eGL_DOUBLE_MAT4:
    case eGL_FLOAT_MAT2x4:
    case eGL_DOUBLE_MAT2x4:
    case eGL_FLOAT_MAT3x4:
    case eGL_DOUBLE_MAT3x4: var.type.descriptor.rows = 4; break;
    case eGL_FLOAT_MAT3:
    case eGL_DOUBLE_MAT3:
    case eGL_FLOAT_MAT4x3:
    case eGL_DOUBLE_MAT4x3:
    case eGL_FLOAT_MAT2x3:
    case eGL_DOUBLE_MAT2x3: var.type.descriptor.rows = 3; break;
    case eGL_FLOAT_MAT2:
    case eGL_DOUBLE_MAT2:
    case eGL_FLOAT_MAT4x2:
    case eGL_DOUBLE_MAT4x2:
    case eGL_FLOAT_MAT3x2:
    case eGL_DOUBLE_MAT3x2: var.type.descriptor.rows = 2; break;
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
    case eGL_INT_VEC4: var.type.descriptor.cols = 4; break;
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
    case eGL_INT_VEC3: var.type.descriptor.cols = 3; break;
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
    case eGL_INT_VEC2: var.type.descriptor.cols = 2; break;
    case eGL_FLOAT:
    case eGL_DOUBLE:
    case eGL_UNSIGNED_INT:
    case eGL_INT:
    case eGL_BOOL: var.type.descriptor.cols = 1; break;
    default: break;
  }

  // set name
  switch(values[0])
  {
    case eGL_FLOAT_VEC4: var.type.descriptor.name = "vec4"; break;
    case eGL_FLOAT_VEC3: var.type.descriptor.name = "vec3"; break;
    case eGL_FLOAT_VEC2: var.type.descriptor.name = "vec2"; break;
    case eGL_FLOAT: var.type.descriptor.name = "float"; break;
    case eGL_FLOAT_MAT4: var.type.descriptor.name = "mat4"; break;
    case eGL_FLOAT_MAT3: var.type.descriptor.name = "mat3"; break;
    case eGL_FLOAT_MAT2: var.type.descriptor.name = "mat2"; break;
    case eGL_FLOAT_MAT4x2: var.type.descriptor.name = "mat4x2"; break;
    case eGL_FLOAT_MAT4x3: var.type.descriptor.name = "mat4x3"; break;
    case eGL_FLOAT_MAT3x4: var.type.descriptor.name = "mat3x4"; break;
    case eGL_FLOAT_MAT3x2: var.type.descriptor.name = "mat3x2"; break;
    case eGL_FLOAT_MAT2x4: var.type.descriptor.name = "mat2x4"; break;
    case eGL_FLOAT_MAT2x3: var.type.descriptor.name = "mat2x3"; break;
    case eGL_DOUBLE_VEC4: var.type.descriptor.name = "dvec4"; break;
    case eGL_DOUBLE_VEC3: var.type.descriptor.name = "dvec3"; break;
    case eGL_DOUBLE_VEC2: var.type.descriptor.name = "dvec2"; break;
    case eGL_DOUBLE: var.type.descriptor.name = "double"; break;
    case eGL_DOUBLE_MAT4: var.type.descriptor.name = "dmat4"; break;
    case eGL_DOUBLE_MAT3: var.type.descriptor.name = "dmat3"; break;
    case eGL_DOUBLE_MAT2: var.type.descriptor.name = "dmat2"; break;
    case eGL_DOUBLE_MAT4x2: var.type.descriptor.name = "dmat4x2"; break;
    case eGL_DOUBLE_MAT4x3: var.type.descriptor.name = "dmat4x3"; break;
    case eGL_DOUBLE_MAT3x4: var.type.descriptor.name = "dmat3x4"; break;
    case eGL_DOUBLE_MAT3x2: var.type.descriptor.name = "dmat3x2"; break;
    case eGL_DOUBLE_MAT2x4: var.type.descriptor.name = "dmat2x4"; break;
    case eGL_DOUBLE_MAT2x3: var.type.descriptor.name = "dmat2x3"; break;
    case eGL_UNSIGNED_INT_VEC4: var.type.descriptor.name = "uvec4"; break;
    case eGL_UNSIGNED_INT_VEC3: var.type.descriptor.name = "uvec3"; break;
    case eGL_UNSIGNED_INT_VEC2: var.type.descriptor.name = "uvec2"; break;
    case eGL_UNSIGNED_INT: var.type.descriptor.name = "uint"; break;
    case eGL_BOOL_VEC4: var.type.descriptor.name = "bvec4"; break;
    case eGL_BOOL_VEC3: var.type.descriptor.name = "bvec3"; break;
    case eGL_BOOL_VEC2: var.type.descriptor.name = "bvec2"; break;
    case eGL_BOOL: var.type.descriptor.name = "bool"; break;
    case eGL_INT_VEC4: var.type.descriptor.name = "ivec4"; break;
    case eGL_INT_VEC3: var.type.descriptor.name = "ivec3"; break;
    case eGL_INT_VEC2: var.type.descriptor.name = "ivec2"; break;
    case eGL_INT: var.type.descriptor.name = "int"; break;
    default: break;
  }

  if(values[5] == -1 && values[2] >= 0)
  {
    var.reg.vec = values[2];
    var.reg.comp = 0;
  }
  else if(values[5] >= 0)
  {
    var.reg.vec = values[5] / 16;
    var.reg.comp = (values[5] / 4) % 4;

    RDCASSERT((values[5] % 4) == 0);
  }
  else
  {
    var.reg.vec = var.reg.comp = ~0U;
  }

  var.type.descriptor.rowMajorStorage = (values[6] > 0);
  var.type.descriptor.arrayStride = values[7];

  var.name.resize(values[1] - 1);
  gl.glGetProgramResourceName(sepProg, query, varIdx, values[1], NULL, &var.name[0]);

  int32_t c = values[1] - 1;

  // trim off trailing [0] if it's an array
  if(var.name[c - 3] == '[' && var.name[c - 2] == '0' && var.name[c - 1] == ']')
    var.name.resize(c - 3);
  else
    var.type.descriptor.elements = 0;

  GLint topLevelStride = 0;
  if(query == eGL_BUFFER_VARIABLE)
  {
    GLenum propName = eGL_TOP_LEVEL_ARRAY_STRIDE;
    gl.glGetProgramResourceiv(sepProg, query, varIdx, 1, &propName, 1, NULL, &topLevelStride);
  }

  vector<DynShaderConstant> *parentmembers = defaultBlock;

  if(values[3] != -1 && values[3] < numParentBlocks)
  {
    parentmembers = &parentBlocks[values[3]];
  }

  if(parentmembers == NULL)
  {
    RDCWARN("Found variable '%s' without parent block index '%d'", var.name.c_str(), values[3]);
    return;
  }

  char *nm = &var.name[0];

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

    int arrayIdx = 0;

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
        // we strip any trailing [0] above (which is useful for non-structure variables),
        // so we should not hit this path unless two variables exist like:
        // structure.member[0]
        // structure.member[1]
        // The program introspection should only return the first for a basic type,
        // and we should not hit this case
        parentmembers = NULL;
        RDCWARN("Unexpected naked array as member (expected only one [0], which should be trimmed");
        break;
      }
    }

    // construct a parent variable
    DynShaderConstant parentVar;
    parentVar.name = base;
    parentVar.reg.vec = var.reg.vec;
    parentVar.reg.comp = 0;
    parentVar.type.descriptor.name = "struct";
    parentVar.type.descriptor.rows = 0;
    parentVar.type.descriptor.cols = 0;
    parentVar.type.descriptor.rowMajorStorage = false;
    parentVar.type.descriptor.type = var.type.descriptor.type;
    parentVar.type.descriptor.elements = isarray ? RDCMAX(1U, uint32_t(arrayIdx + 1)) : 0;
    parentVar.type.descriptor.arrayStride = topLevelStride;

    bool found = false;

    // if we can find the base variable already, we recurse into its members
    for(size_t i = 0; i < parentmembers->size(); i++)
    {
      if((*parentmembers)[i].name == base)
      {
        // if we find the variable, update the # elements to account for this new array index
        // and pick the minimum offset of all of our children as the parent offset. This is mostly
        // just for sorting
        (*parentmembers)[i].type.descriptor.elements =
            RDCMAX((*parentmembers)[i].type.descriptor.elements, parentVar.type.descriptor.elements);
        (*parentmembers)[i].reg.vec = RDCMIN((*parentmembers)[i].reg.vec, parentVar.reg.vec);

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

    // the 0th element of each array fills out the actual members, when we
    // encounter an index above that we only use it to increase the type.descriptor.elements
    // member (which we've done by this point) and can stop recursing
    if(arrayIdx > 0)
    {
      parentmembers = NULL;
      break;
    }
  }

  if(parentmembers)
  {
    // nm points into var.name's storage, so copy out to a temporary
    string n = nm;
    var.name = n;

    parentmembers->push_back(var);
  }
}

void MakeShaderReflection(const GLHookSet &gl, GLenum shadType, GLuint sepProg,
                          ShaderReflection &refl, bool pointSizeUsed, bool clipDistanceUsed)
{
  refl.DebugInfo.compileFlags = 0;

  if(shadType == eGL_COMPUTE_SHADER)
  {
    gl.glGetProgramiv(sepProg, eGL_COMPUTE_WORK_GROUP_SIZE, (GLint *)refl.DispatchThreadsDimension);
  }
  else
  {
    RDCEraseEl(refl.DispatchThreadsDimension);
  }

  vector<ShaderResource> roresources, rwresources;

  GLint numUniforms = 0;
  gl.glGetProgramInterfaceiv(sepProg, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

  const size_t numProps = 7;

  GLenum resProps[numProps] = {
      eGL_TYPE,       eGL_NAME_LENGTH, eGL_LOCATION,     eGL_BLOCK_INDEX,
      eGL_ARRAY_SIZE, eGL_OFFSET,      eGL_IS_ROW_MAJOR,
  };

  for(GLint u = 0; u < numUniforms; u++)
  {
    GLint values[numProps];
    gl.glGetProgramResourceiv(sepProg, eGL_UNIFORM, u, numProps, resProps, numProps, NULL, values);

    ShaderResource res;
    res.IsSampler = false;    // no separate sampler objects in GL
    res.IsReadOnly = true;
    res.IsTexture = true;
    res.variableType.descriptor.rows = 1;
    res.variableType.descriptor.cols = 4;
    res.variableType.descriptor.elements = 0;
    res.variableType.descriptor.rowMajorStorage = false;
    res.variableType.descriptor.arrayStride = 0;

    // float samplers
    if(values[0] == eGL_SAMPLER_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "samplerBuffer";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "sampler1D";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "sampler1DArray";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_SHADOW)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "sampler1DShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_1D_ARRAY_SHADOW)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "sampler1DArrayShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "sampler2D";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "sampler2DArray";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_SHADOW)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "sampler2DShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_ARRAY_SHADOW)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "sampler2DArrayShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "sampler2DRect";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_RECT_SHADOW)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "sampler2DRectShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "sampler3D";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "samplerCube";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE_SHADOW)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "samplerCubeShadow";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "samplerCubeArray";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "sampler2DMS";
      res.variableType.descriptor.type = VarType::Float;
    }
    else if(values[0] == eGL_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "sampler2DMSArray";
      res.variableType.descriptor.type = VarType::Float;
    }
    // int samplers
    else if(values[0] == eGL_INT_SAMPLER_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "isamplerBuffer";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "isampler1D";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "isampler1DArray";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "isampler2D";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "isampler2DArray";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "isampler2DRect";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "isampler3D";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "isamplerCube";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "isamplerCubeArray";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "isampler2DMS";
      res.variableType.descriptor.type = VarType::Int;
    }
    else if(values[0] == eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "isampler2DMSArray";
      res.variableType.descriptor.type = VarType::Int;
    }
    // unsigned int samplers
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "usamplerBuffer";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "usampler1D";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "usampler1DArray";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "usampler2D";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "usampler2DArray";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "usampler2DRect";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "usampler3D";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "usamplerCube";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "usamplerCubeArray";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "usampler2DMS";
      res.variableType.descriptor.type = VarType::UInt;
    }
    else if(values[0] == eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "usampler2DMSArray";
      res.variableType.descriptor.type = VarType::UInt;
    }
    // float images
    else if(values[0] == eGL_IMAGE_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "imageBuffer";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "image1D";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "image1DArray";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "image2D";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "image2DArray";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "image2DRect";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "image3D";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "imageCube";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "imageCubeArray";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "image2DMS";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "image2DMSArray";
      res.variableType.descriptor.type = VarType::Float;
      res.IsReadOnly = false;
    }
    // int images
    else if(values[0] == eGL_INT_IMAGE_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "iimageBuffer";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "iimage1D";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "iimage1DArray";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "iimage2D";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "iimage2DArray";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "iimage2DRect";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "iimage3D";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "iimageCube";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "iimageCubeArray";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "iimage2DMS";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "iimage2DMSArray";
      res.variableType.descriptor.type = VarType::Int;
      res.IsReadOnly = false;
    }
    // unsigned int images
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_BUFFER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "uimageBuffer";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_1D)
    {
      res.resType = TextureDim::Texture1D;
      res.variableType.descriptor.name = "uimage1D";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_1D_ARRAY)
    {
      res.resType = TextureDim::Texture1DArray;
      res.variableType.descriptor.name = "uimage1DArray";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D)
    {
      res.resType = TextureDim::Texture2D;
      res.variableType.descriptor.name = "uimage2D";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_ARRAY)
    {
      res.resType = TextureDim::Texture2DArray;
      res.variableType.descriptor.name = "uimage2DArray";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_RECT)
    {
      res.resType = TextureDim::TextureRect;
      res.variableType.descriptor.name = "uimage2DRect";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_3D)
    {
      res.resType = TextureDim::Texture3D;
      res.variableType.descriptor.name = "uimage3D";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_CUBE)
    {
      res.resType = TextureDim::TextureCube;
      res.variableType.descriptor.name = "uimageCube";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY)
    {
      res.resType = TextureDim::TextureCubeArray;
      res.variableType.descriptor.name = "uimageCubeArray";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE)
    {
      res.resType = TextureDim::Texture2DMS;
      res.variableType.descriptor.name = "uimage2DMS";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    else if(values[0] == eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
    {
      res.resType = TextureDim::Texture2DMSArray;
      res.variableType.descriptor.name = "uimage2DMSArray";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
    }
    // atomic counter
    else if(values[0] == eGL_UNSIGNED_INT_ATOMIC_COUNTER)
    {
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.name = "atomic_uint";
      res.variableType.descriptor.type = VarType::UInt;
      res.IsReadOnly = false;
      res.IsTexture = false;
      res.variableType.descriptor.cols = 1;
    }
    else
    {
      // not a sampler
      continue;
    }

    char *namebuf = new char[values[1] + 1];
    gl.glGetProgramResourceName(sepProg, eGL_UNIFORM, u, values[1], NULL, namebuf);
    namebuf[values[1]] = 0;

    string name = namebuf;

    res.name = name;

    vector<ShaderResource> &reslist = (res.IsReadOnly ? roresources : rwresources);

    res.bindPoint = (int32_t)reslist.size();
    reslist.push_back(res);

    // array of samplers
    if(values[4] > 1)
    {
      name = name.substr(0, name.length() - 3);    // trim off [0] on the end
      for(int i = 1; i < values[4]; i++)
      {
        string arrname = StringFormat::Fmt("%s[%d]", name.c_str(), i);

        res.bindPoint = (int32_t)reslist.size();
        res.name = arrname;

        reslist.push_back(res);
      }
    }
  }

  vector<int32_t> ssbos;
  uint32_t ssboMembers = 0;

  GLint numSSBOs = 0;
  if(HasExt[ARB_shader_storage_buffer_object])
  {
    gl.glGetProgramInterfaceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

    for(GLint u = 0; u < numSSBOs; u++)
    {
      GLenum propName = eGL_NAME_LENGTH;
      GLint len;
      gl.glGetProgramResourceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, u, 1, &propName, 1, NULL, &len);

      char *nm = new char[len + 1];
      gl.glGetProgramResourceName(sepProg, eGL_SHADER_STORAGE_BLOCK, u, len + 1, NULL, nm);

      ShaderResource res;
      res.IsSampler = false;
      res.IsReadOnly = false;
      res.IsTexture = false;
      res.resType = TextureDim::Buffer;
      res.variableType.descriptor.rows = 0;
      res.variableType.descriptor.cols = 0;
      res.variableType.descriptor.elements = len;
      res.variableType.descriptor.rowMajorStorage = false;
      res.variableType.descriptor.arrayStride = 0;
      res.variableType.descriptor.name = "buffer";
      res.variableType.descriptor.type = VarType::UInt;
      res.bindPoint = (int32_t)rwresources.size();
      res.name = nm;

      propName = eGL_NUM_ACTIVE_VARIABLES;
      gl.glGetProgramResourceiv(sepProg, eGL_SHADER_STORAGE_BLOCK, u, 1, &propName, 1, NULL,
                                (GLint *)&res.variableType.descriptor.elements);

      rwresources.push_back(res);
      ssbos.push_back(res.bindPoint);
      ssboMembers += res.variableType.descriptor.elements;

      delete[] nm;
    }
  }

  {
    vector<DynShaderConstant> *members = new vector<DynShaderConstant>[ssbos.size()];

    for(uint32_t i = 0; i < ssboMembers; i++)
    {
      ReconstructVarTree(gl, eGL_BUFFER_VARIABLE, sepProg, i, (GLint)ssbos.size(), members, NULL);
    }

    for(size_t ssbo = 0; ssbo < ssbos.size(); ssbo++)
    {
      sort(members[ssbo]);

      // account for padding for std430 layout, if we have a root array of
      // structs, we need to pad the struct up to have the correct alignment
      if(members[ssbo].size() == 1 && !members[ssbo][0].type.members.empty() &&
         members[ssbo][0].type.descriptor.arrayStride != 0)
      {
        // now that we're sorted, see what the tightly packed stride would be by looking at the last
        // member
        uint32_t desiredStride = members[ssbo][0].type.descriptor.arrayStride;

        DynShaderConstant *last = &members[ssbo][0].type.members.back();
        while(!last->type.members.empty())
          last = &last->type.members.back();

        // start from the offset
        uint32_t stride = last->reg.vec * 16 + last->reg.comp * 4;

        // add its size
        uint32_t size = last->type.descriptor.rows * last->type.descriptor.cols * 4;
        if(last->type.descriptor.type == VarType::Double)
          size *= 2;

        stride += size;

        if(stride < desiredStride)
        {
          uint32_t padding = desiredStride - stride;

          RDCASSERT((padding % 4) == 0 && padding <= 16, padding);

          padding /= 4;

          DynShaderConstant paddingVar;
          paddingVar.name = "__padding";
          paddingVar.reg.vec = last->reg.vec + (size / 16);
          paddingVar.reg.comp = (last->reg.comp + size / 4) % 16;
          paddingVar.type.descriptor.type = VarType::UInt;
          paddingVar.type.descriptor.rows = 1;
          paddingVar.type.descriptor.cols = padding;
          paddingVar.type.descriptor.elements = 1;
          paddingVar.type.descriptor.rowMajorStorage = false;
          paddingVar.type.descriptor.arrayStride = 0;
          paddingVar.type.descriptor.name = StringFormat::Fmt("uint%u", padding);

          members[ssbo][0].type.members.push_back(paddingVar);
        }
      }

      copy(rwresources[ssbos[ssbo]].variableType.members, members[ssbo]);
    }

    delete[] members;
  }

  vector<DynShaderConstant> globalUniforms;

  GLint numUBOs = 0;
  vector<string> uboNames;
  vector<DynShaderConstant> *ubos = NULL;

  {
    gl.glGetProgramInterfaceiv(sepProg, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

    ubos = new vector<DynShaderConstant>[numUBOs];
    uboNames.resize(numUBOs);

    for(GLint u = 0; u < numUBOs; u++)
    {
      GLenum nameLen = eGL_NAME_LENGTH;
      GLint len;
      gl.glGetProgramResourceiv(sepProg, eGL_UNIFORM_BLOCK, u, 1, &nameLen, 1, NULL, &len);

      char *nm = new char[len + 1];
      gl.glGetProgramResourceName(sepProg, eGL_UNIFORM_BLOCK, u, len + 1, NULL, nm);
      uboNames[u] = nm;
      delete[] nm;
    }
  }

  for(GLint u = 0; u < numUniforms; u++)
  {
    ReconstructVarTree(gl, eGL_UNIFORM, sepProg, u, numUBOs, ubos, &globalUniforms);
  }

  vector<ConstantBlock> cbuffers;

  if(ubos)
  {
    cbuffers.reserve(numUBOs + (globalUniforms.empty() ? 0 : 1));

    for(int i = 0; i < numUBOs; i++)
    {
      if(!ubos[i].empty())
      {
        ConstantBlock cblock;
        cblock.name = uboNames[i];
        cblock.bufferBacked = true;
        cblock.bindPoint = (int32_t)cbuffers.size();

        GLenum bufSize = eGL_BUFFER_DATA_SIZE;
        gl.glGetProgramResourceiv(sepProg, eGL_UNIFORM_BLOCK, i, 1, &bufSize, 1, NULL,
                                  (GLint *)&cblock.byteSize);

        sort(ubos[i]);
        copy(cblock.variables, ubos[i]);

        cbuffers.push_back(cblock);
      }
    }
  }

  if(!globalUniforms.empty())
  {
    ConstantBlock globals;
    globals.name = "$Globals";
    globals.bufferBacked = false;
    globals.bindPoint = (int32_t)cbuffers.size();

    sort(globalUniforms);
    copy(globals.variables, globalUniforms);

    cbuffers.push_back(globals);
  }

  delete[] ubos;
  for(int sigType = 0; sigType < 2; sigType++)
  {
    GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);
    rdctype::array<SigParameter> *sigArray = (sigType == 0 ? &refl.InputSig : &refl.OutputSig);

    GLint numInputs;
    gl.glGetProgramInterfaceiv(sepProg, sigEnum, eGL_ACTIVE_RESOURCES, &numInputs);

    if(numInputs > 0)
    {
      vector<SigParameter> sigs;
      sigs.reserve(numInputs);
      for(GLint i = 0; i < numInputs; i++)
      {
        GLenum props[] = {eGL_NAME_LENGTH, eGL_TYPE, eGL_LOCATION, eGL_LOCATION_COMPONENT};
        GLint values[] = {0, 0, 0, 0};

        GLsizei numSigProps = (GLsizei)ARRAY_COUNT(props);

        // GL_LOCATION_COMPONENT not supported on core <4.4 (or without GL_ARB_enhanced_layouts)
        // and on GLES, either
        if(!HasExt[ARB_enhanced_layouts])
          numSigProps--;
        gl.glGetProgramResourceiv(sepProg, sigEnum, i, numSigProps, props, numSigProps, NULL, values);

        char *nm = new char[values[0] + 1];
        gl.glGetProgramResourceName(sepProg, sigEnum, i, values[0] + 1, NULL, nm);

        SigParameter sig;

        sig.varName = nm;
        sig.semanticIndex = 0;
        sig.needSemanticIndex = false;
        sig.stream = 0;

        int rows = 1;

        switch(values[1])
        {
          case eGL_FLOAT:
          case eGL_DOUBLE:
          case eGL_FLOAT_VEC2:
          case eGL_DOUBLE_VEC2:
          case eGL_FLOAT_VEC3:
          case eGL_DOUBLE_VEC3:
          case eGL_FLOAT_VEC4:
          case eGL_DOUBLE_VEC4:
          case eGL_FLOAT_MAT4:
          case eGL_DOUBLE_MAT4:
          case eGL_FLOAT_MAT4x3:
          case eGL_DOUBLE_MAT4x3:
          case eGL_FLOAT_MAT4x2:
          case eGL_DOUBLE_MAT4x2:
          case eGL_FLOAT_MAT3:
          case eGL_DOUBLE_MAT3:
          case eGL_FLOAT_MAT3x4:
          case eGL_DOUBLE_MAT3x4:
          case eGL_FLOAT_MAT3x2:
          case eGL_DOUBLE_MAT3x2:
          case eGL_FLOAT_MAT2:
          case eGL_DOUBLE_MAT2:
          case eGL_FLOAT_MAT2x3:
          case eGL_DOUBLE_MAT2x3:
          case eGL_FLOAT_MAT2x4:
          case eGL_DOUBLE_MAT2x4: sig.compType = CompType::Float; break;
          case eGL_INT:
          case eGL_INT_VEC2:
          case eGL_INT_VEC3:
          case eGL_INT_VEC4: sig.compType = CompType::SInt; break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL:
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2:
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3:
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: sig.compType = CompType::UInt; break;
          default:
            sig.compType = CompType::Float;
            RDCWARN("Unhandled signature element type %s", ToStr::Get((GLenum)values[1]).c_str());
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
            rows = 2;
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
            RDCWARN("Unhandled signature element type %s", ToStr::Get((GLenum)values[1]).c_str());
            sig.compCount = 4;
            sig.regChannelMask = 0xf;
            break;
        }

        sig.regChannelMask <<= values[3];

        sig.channelUsedMask = sig.regChannelMask;

        sig.systemValue = ShaderBuiltin::Undefined;

#define IS_BUILTIN(builtin) !strncmp(nm, builtin, sizeof(builtin) - 1)

        // if these weren't used, they were probably added just to make a separable program
        // (either by us or the program originally). Skip them from the output signature
        if(IS_BUILTIN("gl_PointSize") && !pointSizeUsed)
          continue;
        if(IS_BUILTIN("gl_ClipDistance") && !clipDistanceUsed)
          continue;

        // VS built-in inputs
        if(IS_BUILTIN("gl_VertexID"))
          sig.systemValue = ShaderBuiltin::VertexIndex;
        if(IS_BUILTIN("gl_InstanceID"))
          sig.systemValue = ShaderBuiltin::InstanceIndex;

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
        if(shadType == eGL_FRAGMENT_SHADER && sigEnum == eGL_PROGRAM_OUTPUT &&
           sig.systemValue == ShaderBuiltin::Undefined)
          sig.systemValue = ShaderBuiltin::ColorOutput;

        if(sig.systemValue == ShaderBuiltin::Undefined)
          sig.regIndex = values[2] >= 0 ? values[2] : i;
        else
          sig.regIndex = values[2] >= 0 ? values[2] : 0;

        if(rows == 1)
        {
          sigs.push_back(sig);
        }
        else
        {
          for(int r = 0; r < rows; r++)
          {
            SigParameter s = sig;
            s.varName = StringFormat::Fmt("%s:row%d", nm, r);
            s.regIndex += r;
            sigs.push_back(s);
          }
        }

        delete[] nm;
      }
      struct sig_param_sort
      {
        bool operator()(const SigParameter &a, const SigParameter &b)
        {
          if(a.systemValue == b.systemValue)
            return a.regIndex < b.regIndex;
          return a.systemValue < b.systemValue;
        }
      };

      std::sort(sigs.begin(), sigs.end(), sig_param_sort());

      *sigArray = sigs;
    }
  }

  // TODO: fill in Interfaces with shader subroutines?

  refl.ReadOnlyResources = roresources;
  refl.ReadWriteResources = rwresources;
  refl.ConstantBlocks = cbuffers;
}

void GetBindpointMapping(const GLHookSet &gl, GLuint curProg, int shadIdx, ShaderReflection *refl,
                         ShaderBindpointMapping &mapping)
{
  // in case of bugs, we readback into this array instead of
  GLint dummyReadback[32];

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    dummyReadback[i] = 0x6c7b8a9d;
#endif

  const GLenum refEnum[] = {
      eGL_REFERENCED_BY_VERTEX_SHADER,          eGL_REFERENCED_BY_TESS_CONTROL_SHADER,
      eGL_REFERENCED_BY_TESS_EVALUATION_SHADER, eGL_REFERENCED_BY_GEOMETRY_SHADER,
      eGL_REFERENCED_BY_FRAGMENT_SHADER,        eGL_REFERENCED_BY_COMPUTE_SHADER,
  };

  int32_t numReadOnlyResources = refl ? refl->ReadOnlyResources.count : 0;

  create_array_uninit(mapping.ReadOnlyResources, numReadOnlyResources);
  for(int32_t i = 0; i < numReadOnlyResources; i++)
  {
    if(refl->ReadOnlyResources.elems[i].IsTexture)
    {
      // normal sampler or image load/store

      GLint loc = gl.glGetUniformLocation(curProg, refl->ReadOnlyResources.elems[i].name.elems);
      if(loc >= 0)
      {
        gl.glGetUniformiv(curProg, loc, dummyReadback);
        mapping.ReadOnlyResources[i].bindset = 0;
        mapping.ReadOnlyResources[i].bind = dummyReadback[0];
        mapping.ReadOnlyResources[i].arraySize = 1;
      }

      // handle sampler arrays, use the base name
      string name = refl->ReadOnlyResources.elems[i].name.elems;
      if(name.back() == ']')
      {
        do
        {
          name.pop_back();
        } while(name.back() != '[');
        name.pop_back();
      }

      GLuint idx = 0;
      idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM, name.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        mapping.ReadOnlyResources[i].used = false;
      }
      else
      {
        GLint used = 0;
        gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &refEnum[shadIdx], 1, NULL, &used);
        mapping.ReadOnlyResources[i].used = (used != 0);
      }
    }
    else
    {
      mapping.ReadOnlyResources[i].bindset = -1;
      mapping.ReadOnlyResources[i].bind = -1;
      mapping.ReadOnlyResources[i].used = false;
      mapping.ReadOnlyResources[i].arraySize = 1;
    }
  }

  int32_t numReadWriteResources = refl ? refl->ReadWriteResources.count : 0;

  create_array_uninit(mapping.ReadWriteResources, numReadWriteResources);
  for(int32_t i = 0; i < numReadWriteResources; i++)
  {
    if(refl->ReadWriteResources.elems[i].IsTexture)
    {
      // image load/store

      GLint loc = gl.glGetUniformLocation(curProg, refl->ReadWriteResources.elems[i].name.elems);
      if(loc >= 0)
      {
        gl.glGetUniformiv(curProg, loc, dummyReadback);
        mapping.ReadWriteResources[i].bindset = 0;
        mapping.ReadWriteResources[i].bind = dummyReadback[0];
        mapping.ReadWriteResources[i].arraySize = 1;
      }

      // handle sampler arrays, use the base name
      string name = refl->ReadWriteResources.elems[i].name.elems;
      if(name.back() == ']')
      {
        do
        {
          name.pop_back();
        } while(name.back() != '[');
        name.pop_back();
      }

      GLuint idx = 0;
      idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM, name.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        mapping.ReadWriteResources[i].used = false;
      }
      else
      {
        GLint used = 0;
        gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &refEnum[shadIdx], 1, NULL, &used);
        mapping.ReadWriteResources[i].used = (used != 0);
      }
    }
    else if(!refl->ReadWriteResources.elems[i].IsTexture)
    {
      if(refl->ReadWriteResources.elems[i].variableType.descriptor.cols == 1 &&
         refl->ReadWriteResources.elems[i].variableType.descriptor.rows == 1 &&
         refl->ReadWriteResources.elems[i].variableType.descriptor.type == VarType::UInt)
      {
        // atomic uint
        GLuint idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM,
                                                  refl->ReadWriteResources.elems[i].name.elems);

        if(idx == GL_INVALID_INDEX)
        {
          mapping.ReadWriteResources[i].bindset = -1;
          mapping.ReadWriteResources[i].bind = -1;
          mapping.ReadWriteResources[i].used = false;
          mapping.ReadWriteResources[i].arraySize = 1;
        }
        else
        {
          GLenum prop = eGL_ATOMIC_COUNTER_BUFFER_INDEX;
          GLuint atomicIndex;
          gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &prop, 1, NULL,
                                    (GLint *)&atomicIndex);

          if(atomicIndex == GL_INVALID_INDEX)
          {
            mapping.ReadWriteResources[i].bindset = -1;
            mapping.ReadWriteResources[i].bind = -1;
            mapping.ReadWriteResources[i].used = false;
            mapping.ReadWriteResources[i].arraySize = 1;
          }
          else
          {
            if(IsGLES)
            {
              prop = eGL_BUFFER_BINDING;
              mapping.ReadWriteResources[i].bindset = 0;
              gl.glGetProgramResourceiv(curProg, eGL_ATOMIC_COUNTER_BUFFER, atomicIndex, 1, &prop,
                                        1, NULL, &mapping.ReadWriteResources[i].bind);
              GLint used = 0;
              gl.glGetProgramResourceiv(curProg, eGL_ATOMIC_COUNTER_BUFFER, atomicIndex, 1,
                                        &refEnum[shadIdx], 1, NULL, &used);
              mapping.ReadWriteResources[i].used = (used != 0);
              mapping.ReadWriteResources[i].arraySize = 1;
            }
            else
            {
              const GLenum atomicRefEnum[] = {
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER,
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER,
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER,
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER,
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER,
                  eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER,
              };
              mapping.ReadWriteResources[i].bindset = 0;
              gl.glGetActiveAtomicCounterBufferiv(curProg, atomicIndex,
                                                  eGL_ATOMIC_COUNTER_BUFFER_BINDING,
                                                  &mapping.ReadWriteResources[i].bind);
              GLint used = 0;
              gl.glGetActiveAtomicCounterBufferiv(curProg, atomicIndex, atomicRefEnum[shadIdx],
                                                  &used);
              mapping.ReadWriteResources[i].used = (used != 0);
              mapping.ReadWriteResources[i].arraySize = 1;
            }
          }
        }
      }
      else
      {
        // shader storage buffer object
        GLuint idx = gl.glGetProgramResourceIndex(curProg, eGL_SHADER_STORAGE_BLOCK,
                                                  refl->ReadWriteResources.elems[i].name.elems);

        if(idx == GL_INVALID_INDEX)
        {
          mapping.ReadWriteResources[i].bindset = -1;
          mapping.ReadWriteResources[i].bind = -1;
          mapping.ReadWriteResources[i].used = false;
          mapping.ReadWriteResources[i].arraySize = 1;
        }
        else
        {
          GLenum prop = eGL_BUFFER_BINDING;
          mapping.ReadWriteResources[i].bindset = 0;
          gl.glGetProgramResourceiv(curProg, eGL_SHADER_STORAGE_BLOCK, idx, 1, &prop, 1, NULL,
                                    &mapping.ReadWriteResources[i].bind);
          GLint used = 0;
          gl.glGetProgramResourceiv(curProg, eGL_SHADER_STORAGE_BLOCK, idx, 1, &refEnum[shadIdx], 1,
                                    NULL, &used);
          mapping.ReadWriteResources[i].used = (used != 0);
          mapping.ReadWriteResources[i].arraySize = 1;
        }
      }
    }
    else
    {
      mapping.ReadWriteResources[i].bindset = -1;
      mapping.ReadWriteResources[i].bind = -1;
      mapping.ReadWriteResources[i].used = false;
      mapping.ReadWriteResources[i].arraySize = 1;
    }
  }

  int32_t numCBlocks = refl ? refl->ConstantBlocks.count : 0;

  create_array_uninit(mapping.ConstantBlocks, numCBlocks);
  for(int32_t i = 0; i < numCBlocks; i++)
  {
    if(refl->ConstantBlocks.elems[i].bufferBacked)
    {
      GLint loc = gl.glGetUniformBlockIndex(curProg, refl->ConstantBlocks.elems[i].name.elems);
      if(loc >= 0)
      {
        gl.glGetActiveUniformBlockiv(curProg, loc, eGL_UNIFORM_BLOCK_BINDING, dummyReadback);
        mapping.ConstantBlocks[i].bindset = 0;
        mapping.ConstantBlocks[i].bind = dummyReadback[0];
        mapping.ConstantBlocks[i].arraySize = 1;
      }
    }
    else
    {
      mapping.ConstantBlocks[i].bindset = -1;
      mapping.ConstantBlocks[i].bind = -1;
      mapping.ConstantBlocks[i].arraySize = 1;
    }

    if(!refl->ConstantBlocks.elems[i].bufferBacked)
    {
      mapping.ConstantBlocks[i].used = true;
    }
    else
    {
      GLuint idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM_BLOCK,
                                                refl->ConstantBlocks.elems[i].name.elems);
      if(idx == GL_INVALID_INDEX)
      {
        mapping.ConstantBlocks[i].used = false;
      }
      else
      {
        GLint used = 0;
        gl.glGetProgramResourceiv(curProg, eGL_UNIFORM_BLOCK, idx, 1, &refEnum[shadIdx], 1, NULL,
                                  &used);
        mapping.ConstantBlocks[i].used = (used != 0);
      }
    }
  }

  GLint numVAttribBindings = 16;
  gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

  create_array_uninit(mapping.InputAttributes, numVAttribBindings);
  for(int32_t i = 0; i < numVAttribBindings; i++)
    mapping.InputAttributes[i] = -1;

  // override identity map with bindings
  if(shadIdx == 0 && refl)
  {
    for(int32_t i = 0; i < refl->InputSig.count; i++)
    {
      GLint loc = gl.glGetAttribLocation(curProg, refl->InputSig.elems[i].varName.elems);

      if(loc >= 0 && loc < numVAttribBindings)
      {
        mapping.InputAttributes[loc] = i;
      }
    }
  }

#if ENABLED(RDOC_DEVEL)
  for(size_t i = 1; i < ARRAY_COUNT(dummyReadback); i++)
    if(dummyReadback[i] != 0x6c7b8a9d)
      RDCERR("Invalid uniform readback - data beyond first element modified!");
#endif
}
