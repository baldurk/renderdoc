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

#include "data/glsl_shaders.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
#include "gl_driver.h"

const int firstChar = int(' ') + 1;
const int lastChar = 127;
const int numChars = lastChar - firstChar;
const float charPixelHeight = 20.0f;

stbtt_bakedchar chardata[numChars];

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

// TODO this could be a general class for use elsewhere (ie. code that wants to push and pop would
// set state through the class, which records dirty bits and then restores).
struct RenderTextState
{
  bool enableBits[8];
  GLenum ClipOrigin, ClipDepth;
  GLenum EquationRGB, EquationAlpha;
  GLenum SourceRGB, SourceAlpha;
  GLenum DestinationRGB, DestinationAlpha;
  GLenum PolygonMode;
  GLfloat Viewportf[4];
  GLint Viewport[4];
  GLenum ActiveTexture;
  GLuint tex0;
  GLuint ubo[3];
  GLuint prog;
  GLuint pipe;
  GLuint VAO;
  GLuint drawFBO;

  // if this context wasn't created with CreateContextAttribs we do an immediate mode render, so
  // fewer states are pushed/popped.
  // Note we don't assume a 1.0 context since that would be painful to handle. Instead we just skip
  // bits of state we're not going to mess with. In some cases this might cause problems e.g. we
  // don't use indexed enable states for blend and scissor test because we're assuming there's no
  // separate blending.
  //
  // In the end, this is just a best-effort to keep going without crashing. Old GL versions aren't
  // supported.
  void Push(const GLHookSet &gl, bool modern)
  {
    enableBits[0] = gl.glIsEnabled(eGL_DEPTH_TEST) != 0;
    enableBits[1] = gl.glIsEnabled(eGL_STENCIL_TEST) != 0;
    enableBits[2] = gl.glIsEnabled(eGL_CULL_FACE) != 0;
    if(modern)
    {
      if(!IsGLES)
        enableBits[3] = gl.glIsEnabled(eGL_DEPTH_CLAMP) != 0;

      if(HasExt[ARB_draw_buffers_blend])
        enableBits[4] = gl.glIsEnabledi(eGL_BLEND, 0) != 0;
      else
        enableBits[4] = gl.glIsEnabled(eGL_BLEND) != 0;

      if(HasExt[ARB_viewport_array])
        enableBits[5] = gl.glIsEnabledi(eGL_SCISSOR_TEST, 0) != 0;
      else
        enableBits[5] = gl.glIsEnabled(eGL_SCISSOR_TEST) != 0;
    }
    else
    {
      enableBits[3] = gl.glIsEnabled(eGL_BLEND) != 0;
      enableBits[4] = gl.glIsEnabled(eGL_SCISSOR_TEST) != 0;
      enableBits[5] = gl.glIsEnabled(eGL_TEXTURE_2D) != 0;
      enableBits[6] = gl.glIsEnabled(eGL_LIGHTING) != 0;
      enableBits[7] = gl.glIsEnabled(eGL_ALPHA_TEST) != 0;
    }

    if(modern && HasExt[ARB_clip_control])
    {
      gl.glGetIntegerv(eGL_CLIP_ORIGIN, (GLint *)&ClipOrigin);
      gl.glGetIntegerv(eGL_CLIP_DEPTH_MODE, (GLint *)&ClipDepth);
    }
    else
    {
      ClipOrigin = eGL_LOWER_LEFT;
      ClipDepth = eGL_NEGATIVE_ONE_TO_ONE;
    }

    if(modern && HasExt[ARB_draw_buffers_blend])
    {
      gl.glGetIntegeri_v(eGL_BLEND_EQUATION_RGB, 0, (GLint *)&EquationRGB);
      gl.glGetIntegeri_v(eGL_BLEND_EQUATION_ALPHA, 0, (GLint *)&EquationAlpha);

      gl.glGetIntegeri_v(eGL_BLEND_SRC_RGB, 0, (GLint *)&SourceRGB);
      gl.glGetIntegeri_v(eGL_BLEND_SRC_ALPHA, 0, (GLint *)&SourceAlpha);

      gl.glGetIntegeri_v(eGL_BLEND_DST_RGB, 0, (GLint *)&DestinationRGB);
      gl.glGetIntegeri_v(eGL_BLEND_DST_ALPHA, 0, (GLint *)&DestinationAlpha);
    }
    else
    {
      gl.glGetIntegerv(eGL_BLEND_EQUATION_RGB, (GLint *)&EquationRGB);
      gl.glGetIntegerv(eGL_BLEND_EQUATION_ALPHA, (GLint *)&EquationAlpha);

      gl.glGetIntegerv(eGL_BLEND_SRC_RGB, (GLint *)&SourceRGB);
      gl.glGetIntegerv(eGL_BLEND_SRC_ALPHA, (GLint *)&SourceAlpha);

      gl.glGetIntegerv(eGL_BLEND_DST_RGB, (GLint *)&DestinationRGB);
      gl.glGetIntegerv(eGL_BLEND_DST_ALPHA, (GLint *)&DestinationAlpha);
    }

    if(!VendorCheck[VendorCheck_AMD_polygon_mode_query] && !IsGLES)
    {
      GLenum dummy[2] = {eGL_FILL, eGL_FILL};
      // docs suggest this is enumeration[2] even though polygon mode can't be set independently for
      // front and back faces.
      gl.glGetIntegerv(eGL_POLYGON_MODE, (GLint *)&dummy);
      PolygonMode = dummy[0];
    }
    else
    {
      PolygonMode = eGL_FILL;
    }

    if(modern && HasExt[ARB_viewport_array])
      gl.glGetFloati_v(eGL_VIEWPORT, 0, &Viewportf[0]);
    else
      gl.glGetIntegerv(eGL_VIEWPORT, &Viewport[0]);

    gl.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&ActiveTexture);
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&tex0);

    // we get the current program but only try to restore it if it's non-0
    prog = 0;
    if(modern)
      gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);

    drawFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);

    // since we will use the fixed function pipeline, also need to check for program pipeline
    // bindings (if we weren't, our program would override)
    pipe = 0;
    if(modern && HasExt[ARB_separate_shader_objects])
      gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

    if(modern)
    {
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 0, (GLint *)&ubo[0]);
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 1, (GLint *)&ubo[1]);
      gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 2, (GLint *)&ubo[2]);

      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
    }
  }

  void Pop(const GLHookSet &gl, bool modern)
  {
    if(enableBits[0])
      gl.glEnable(eGL_DEPTH_TEST);
    else
      gl.glDisable(eGL_DEPTH_TEST);
    if(enableBits[1])
      gl.glEnable(eGL_STENCIL_TEST);
    else
      gl.glDisable(eGL_STENCIL_TEST);
    if(enableBits[2])
      gl.glEnable(eGL_CULL_FACE);
    else
      gl.glDisable(eGL_CULL_FACE);

    if(modern)
    {
      if(!IsGLES)
      {
        if(enableBits[3])
          gl.glEnable(eGL_DEPTH_CLAMP);
        else
          gl.glDisable(eGL_DEPTH_CLAMP);
      }

      if(HasExt[ARB_draw_buffers_blend])
      {
        if(enableBits[4])
          gl.glEnablei(eGL_BLEND, 0);
        else
          gl.glDisablei(eGL_BLEND, 0);
      }
      else
      {
        if(enableBits[4])
          gl.glEnable(eGL_BLEND);
        else
          gl.glDisable(eGL_BLEND);
      }

      if(HasExt[ARB_viewport_array])
      {
        if(enableBits[5])
          gl.glEnablei(eGL_SCISSOR_TEST, 0);
        else
          gl.glDisablei(eGL_SCISSOR_TEST, 0);
      }
      else
      {
        if(enableBits[5])
          gl.glEnable(eGL_SCISSOR_TEST);
        else
          gl.glDisable(eGL_SCISSOR_TEST);
      }
    }
    else
    {
      if(enableBits[3])
        gl.glEnable(eGL_BLEND);
      else
        gl.glDisable(eGL_BLEND);
      if(enableBits[4])
        gl.glEnable(eGL_SCISSOR_TEST);
      else
        gl.glDisable(eGL_SCISSOR_TEST);
      if(enableBits[5])
        gl.glEnable(eGL_TEXTURE_2D);
      else
        gl.glDisable(eGL_TEXTURE_2D);
      if(enableBits[6])
        gl.glEnable(eGL_LIGHTING);
      else
        gl.glDisable(eGL_LIGHTING);
      if(enableBits[7])
        gl.glEnable(eGL_ALPHA_TEST);
      else
        gl.glDisable(eGL_ALPHA_TEST);
    }

    if(modern && gl.glClipControl && HasExt[ARB_clip_control])
      gl.glClipControl(ClipOrigin, ClipDepth);

    if(modern && HasExt[ARB_draw_buffers_blend])
    {
      gl.glBlendFuncSeparatei(0, SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
      gl.glBlendEquationSeparatei(0, EquationRGB, EquationAlpha);
    }
    else
    {
      gl.glBlendFuncSeparate(SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
      gl.glBlendEquationSeparate(EquationRGB, EquationAlpha);
    }

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, PolygonMode);

    if(modern && HasExt[ARB_viewport_array])
      gl.glViewportIndexedf(0, Viewportf[0], Viewportf[1], Viewportf[2], Viewportf[3]);
    else
      gl.glViewport(Viewport[0], Viewport[1], (GLsizei)Viewport[2], (GLsizei)Viewport[3]);

    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, tex0);
    gl.glActiveTexture(ActiveTexture);

    if(drawFBO != 0 && gl.glBindFramebuffer)
      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);

    if(modern)
    {
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, ubo[0]);
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, ubo[1]);
      gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, ubo[2]);

      gl.glUseProgram(prog);

      gl.glBindVertexArray(VAO);
    }
    else
    {
      // only restore these if there was a setting and the function pointer exists
      if(gl.glUseProgram && prog != 0)
        gl.glUseProgram(prog);
      if(gl.glBindProgramPipeline && pipe != 0)
        gl.glBindProgramPipeline(pipe);
    }
  }
};

void WrappedOpenGL::ContextData::CreateDebugData(const GLHookSet &gl)
{
  // to let us display the overlay on old GL contexts, use as simple a subset of functionality as
  // possible to upload the texture. VAO and shaders are used optionally on modern contexts,
  // otherwise we fall back to immediate mode rendering by hand
  if(gl.glGetIntegerv && gl.glGenTextures && gl.glBindTexture && gl.glTexImage2D && gl.glTexParameteri)
  {
    string ttfstring = GetEmbeddedResource(sourcecodepro_ttf);
    byte *ttfdata = (byte *)ttfstring.c_str();

    byte *buf = new byte[FONT_TEX_WIDTH * FONT_TEX_HEIGHT];

    stbtt_BakeFontBitmap(ttfdata, 0, charPixelHeight, buf, FONT_TEX_WIDTH, FONT_TEX_HEIGHT,
                         firstChar, numChars, chardata);

    CharSize = charPixelHeight;
    CharAspect = chardata->xadvance / charPixelHeight;

    stbtt_fontinfo f = {0};
    stbtt_InitFont(&f, ttfdata, 0);

    int ascent = 0;
    stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

    float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, charPixelHeight);

    {
      PixelUnpackState unpack;

      unpack.Fetch(&gl, false);

      ResetPixelUnpackState(gl, false, 1);

      GLuint curtex = 0;
      gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&curtex);

      GLenum texFmt = eGL_R8;
      if(Legacy())
        texFmt = eGL_LUMINANCE;

      gl.glGenTextures(1, &GlyphTexture);
      gl.glBindTexture(eGL_TEXTURE_2D, GlyphTexture);
      gl.glTexImage2D(eGL_TEXTURE_2D, 0, texFmt, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, 0, eGL_RED,
                      eGL_UNSIGNED_BYTE, buf);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
      gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);

      gl.glBindTexture(eGL_TEXTURE_2D, curtex);

      unpack.Apply(&gl, false);
    }

    delete[] buf;

    Vec4f glyphData[2 * (numChars + 1)];

    for(int i = 0; i < numChars; i++)
    {
      stbtt_bakedchar *b = chardata + i;

      float x = b->xoff;
      float y = b->yoff + maxheight;

      glyphData[(i + 1) * 2 + 0] =
          Vec4f(x / b->xadvance, y / charPixelHeight, b->xadvance / float(b->x1 - b->x0),
                charPixelHeight / float(b->y1 - b->y0));
      glyphData[(i + 1) * 2 + 1] = Vec4f(b->x0, b->y0, b->x1, b->y1);
    }

    if(Modern() && gl.glGenVertexArrays && gl.glBindVertexArray)
    {
      GLuint curvao = 0;
      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&curvao);

      gl.glGenVertexArrays(1, &DummyVAO);
      gl.glBindVertexArray(DummyVAO);

      gl.glBindVertexArray(curvao);
    }

    if(Modern() && gl.glGenBuffers && gl.glBufferData && gl.glBindBuffer)
    {
      GLuint curubo = 0;
      gl.glGetIntegerv(eGL_UNIFORM_BUFFER_BINDING, (GLint *)&curubo);

      gl.glGenBuffers(1, &GlyphUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, GlyphUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(glyphData), glyphData, eGL_STATIC_DRAW);

      gl.glGenBuffers(1, &GeneralUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, GeneralUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(FontUBOData), NULL, eGL_DYNAMIC_DRAW);

      gl.glGenBuffers(1, &StringUBO);
      gl.glBindBuffer(eGL_UNIFORM_BUFFER, StringUBO);
      gl.glBufferData(eGL_UNIFORM_BUFFER, sizeof(uint32_t) * 4 * FONT_MAX_CHARS, NULL,
                      eGL_DYNAMIC_DRAW);

      gl.glBindBuffer(eGL_UNIFORM_BUFFER, curubo);
    }

    if(Modern() && gl.glCreateShader && gl.glShaderSource && gl.glCompileShader &&
       gl.glGetShaderiv && gl.glGetShaderInfoLog && gl.glDeleteShader && gl.glCreateProgram &&
       gl.glAttachShader && gl.glLinkProgram && gl.glGetProgramiv && gl.glGetProgramInfoLog)
    {
      vector<string> vs;
      vector<string> fs;

      ShaderType shaderType;
      int glslVersion;
      string fragDefines;

      if(IsGLES)
      {
        shaderType = eShaderGLSLES;
        glslVersion = 310;
        fragDefines = "";
      }
      else
      {
        shaderType = eShaderGLSL;
        glslVersion = 150;
        fragDefines =
            "#extension GL_ARB_shading_language_420pack : require\n"
            "#extension GL_ARB_separate_shader_objects : require\n"
            "#extension GL_ARB_explicit_attrib_location : require\n";
      }

      GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_text_vert), glslVersion);
      GenerateGLSLShader(fs, shaderType, fragDefines, GetEmbeddedResource(glsl_text_frag),
                         glslVersion);

      vector<const char *> vsc;
      vsc.reserve(vs.size());
      vector<const char *> fsc;
      fsc.reserve(fs.size());

      for(size_t i = 0; i < vs.size(); i++)
        vsc.push_back(vs[i].c_str());

      for(size_t i = 0; i < fs.size(); i++)
        fsc.push_back(fs[i].c_str());

      GLuint vert = gl.glCreateShader(eGL_VERTEX_SHADER);
      GLuint frag = gl.glCreateShader(eGL_FRAGMENT_SHADER);

      gl.glShaderSource(vert, (GLsizei)vs.size(), &vsc[0], NULL);
      gl.glShaderSource(frag, (GLsizei)fs.size(), &fsc[0], NULL);

      gl.glCompileShader(vert);
      gl.glCompileShader(frag);

      char buffer[1024] = {0};
      GLint status = 0;

      gl.glGetShaderiv(vert, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        gl.glGetShaderInfoLog(vert, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      gl.glGetShaderiv(frag, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        gl.glGetShaderInfoLog(frag, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      Program = gl.glCreateProgram();

      gl.glAttachShader(Program, vert);
      gl.glAttachShader(Program, frag);

      gl.glLinkProgram(Program);

      gl.glGetProgramiv(Program, eGL_LINK_STATUS, &status);
      if(status == 0)
      {
        gl.glGetProgramInfoLog(Program, 1024, NULL, buffer);
        RDCERR("Link error: %s", buffer);
      }

      gl.glDeleteShader(vert);
      gl.glDeleteShader(frag);
    }

    ready = true;
  }
}

void WrappedOpenGL::RenderOverlayText(float x, float y, const char *fmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, fmt);
  StringFormat::vsnprintf(tmpBuf, 4095, fmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  ContextData &ctxdata = GetCtxData();

  RenderTextState textState;

  textState.Push(m_Real, ctxdata.Modern());

  RenderOverlayStr(x, y, tmpBuf);

  textState.Pop(m_Real, ctxdata.Modern());
}

void WrappedOpenGL::RenderOverlayStr(float x, float y, const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderOverlayStr(x, y, text);
    RenderOverlayStr(x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  const GLHookSet &gl = m_Real;

  RDCASSERT(strlen(text) < (size_t)FONT_MAX_CHARS);

  ContextData &ctxdata = m_ContextData[GetCtx()];

  if(!ctxdata.built || !ctxdata.ready)
    return;

  // if it's reasonably modern context, assume we can use buffers and UBOs
  if(ctxdata.Modern())
  {
    gl.glBindBuffer(eGL_UNIFORM_BUFFER, ctxdata.GeneralUBO);

    FontUBOData *ubo = (FontUBOData *)gl.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(FontUBOData), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    ubo->TextPosition.x = x;
    ubo->TextPosition.y = y;

    ubo->FontScreenAspect.x = 1.0f / float(m_InitParams.width);
    ubo->FontScreenAspect.y = 1.0f / float(m_InitParams.height);

    ubo->TextSize = ctxdata.CharSize;
    ubo->FontScreenAspect.x *= ctxdata.CharAspect;

    ubo->CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
    ubo->CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    size_t len = strlen(text);

    if((int)len > FONT_MAX_CHARS)
    {
      static bool printedWarning = false;

      // this could be called once a frame, don't want to spam the log
      if(!printedWarning)
      {
        printedWarning = true;
        RDCWARN("log string '%s' is too long", text, (int)len);
      }

      len = FONT_MAX_CHARS;
    }

    gl.glBindBuffer(eGL_UNIFORM_BUFFER, ctxdata.StringUBO);
    uint32_t *texs =
        (uint32_t *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, len * 4 * sizeof(uint32_t),
                                        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if(texs)
    {
      for(size_t i = 0; i < len; i++)
      {
        texs[i * 4 + 0] = text[i] - ' ';
        texs[i * 4 + 1] = text[i] - ' ';
        texs[i * 4 + 2] = text[i] - ' ';
        texs[i * 4 + 3] = text[i] - ' ';
      }
    }
    else
    {
      static bool printedWarning = false;

      // this could be called once a frame, don't want to spam the log
      if(!printedWarning)
      {
        printedWarning = true;
        RDCWARN("failed to map %d characters for '%s' (%d)", (int)len, text, ctxdata.StringUBO);
      }
    }

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    //////////////////////////////////////////////////////////////////////////////////
    // Make sure if you change any other state in here, that you also update the push
    // and pop functions above (RenderTextState)

    // set blend state
    if(HasExt[ARB_draw_buffers_blend])
    {
      gl.glEnablei(eGL_BLEND, 0);
      gl.glBlendFuncSeparatei(0, eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA,
                              eGL_SRC_ALPHA);
      gl.glBlendEquationSeparatei(0, eGL_FUNC_ADD, eGL_FUNC_ADD);
    }
    else
    {
      gl.glEnable(eGL_BLEND);
      gl.glBlendFuncSeparate(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA, eGL_SRC_ALPHA);
      gl.glBlendEquationSeparate(eGL_FUNC_ADD, eGL_FUNC_ADD);
    }

    // set depth & stencil
    gl.glDisable(eGL_DEPTH_TEST);
    if(!IsGLES)
      gl.glDisable(eGL_DEPTH_CLAMP);
    gl.glDisable(eGL_STENCIL_TEST);
    gl.glDisable(eGL_CULL_FACE);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // set viewport & scissor
    if(HasExt[ARB_viewport_array])
    {
      gl.glViewportIndexedf(0, 0.0f, 0.0f, (float)m_InitParams.width, (float)m_InitParams.height);
      gl.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      gl.glViewport(0, 0, m_InitParams.width, m_InitParams.height);
      gl.glDisable(eGL_SCISSOR_TEST);
    }

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(gl.glClipControl && HasExt[ARB_clip_control])
      gl.glClipControl(eGL_LOWER_LEFT, eGL_NEGATIVE_ONE_TO_ONE);

    // bind UBOs
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, ctxdata.GeneralUBO);
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, ctxdata.GlyphUBO);
    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, ctxdata.StringUBO);

    // bind empty VAO just for valid rendering
    gl.glBindVertexArray(ctxdata.DummyVAO);

    // bind textures
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);

    // bind program
    gl.glUseProgram(ctxdata.Program);

    // draw string
    gl.glDrawArrays(eGL_TRIANGLES, 0, 6 * (GLsizei)len);
  }
  else
  {
    // if it wasn't created in modern fashion with createattribs, assume the worst and draw with
    // immediate mode (since it's impossible that the context is core profile, this will always
    // work)
    //
    // This isn't perfect since without a lot of fiddling we'd need to check if e.g. indexed
    // blending should be used or not. Since we're not too worried about working in this situation,
    // just doing something reasonable, we just assume roughly ~2.0 functionality

    //////////////////////////////////////////////////////////////////////////////////
    // Make sure if you change any other state in here, that you also update the push
    // and pop functions above (RenderTextState)

    // disable blending and some old-style fixed function features
    gl.glDisable(eGL_BLEND);
    gl.glDisable(eGL_LIGHTING);
    gl.glDisable(eGL_ALPHA_TEST);

    // set depth & stencil
    gl.glDisable(eGL_DEPTH_TEST);
    gl.glDisable(eGL_STENCIL_TEST);
    gl.glDisable(eGL_CULL_FACE);

    // set viewport & scissor
    gl.glViewport(0, 0, (GLsizei)m_InitParams.width, (GLsizei)m_InitParams.height);
    gl.glDisable(eGL_SCISSOR_TEST);
    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    // bind textures
    gl.glActiveTexture(eGL_TEXTURE0);
    gl.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);
    gl.glEnable(eGL_TEXTURE_2D);

    if(gl.glBindFramebuffer)
      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // just in case, try to disable the programmable pipeline
    if(gl.glUseProgram)
      gl.glUseProgram(0);
    if(gl.glBindProgramPipeline)
      gl.glBindProgramPipeline(0);

    // draw string (based on sample code from stb_truetype.h)
    vector<Vec4f> vertices;
    {
      y += 1.0f;
      y *= charPixelHeight;

      float startx = x;
      float starty = y;

      float maxx = x, minx = x;
      float maxy = y, miny = y - charPixelHeight;

      stbtt_aligned_quad q;

      const char *prepass = text;
      while(*prepass)
      {
        char c = *prepass;
        if(c >= firstChar && c <= lastChar)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - firstChar, &x, &y, &q, 1);

          maxx = RDCMAX(maxx, RDCMAX(q.x0, q.x1));
          maxy = RDCMAX(maxy, RDCMAX(q.y0, q.y1));

          minx = RDCMIN(minx, RDCMIN(q.x0, q.x1));
          miny = RDCMIN(miny, RDCMIN(q.y0, q.y1));
        }
        else
        {
          x += chardata[0].xadvance;
        }
        prepass++;
      }

      x = startx;
      y = starty;

      // draw black bar behind text

      vertices.push_back(Vec4f(minx, maxy, 0.0f, 0.0f));
      vertices.push_back(Vec4f(maxx, maxy, 0.0f, 0.0f));
      vertices.push_back(Vec4f(maxx, miny, 0.0f, 0.0f));
      vertices.push_back(Vec4f(minx, miny, 0.0f, 0.0f));

      while(*text)
      {
        char c = *text;
        if(c >= firstChar && c <= lastChar)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - firstChar, &x, &y, &q, 1);

          vertices.push_back(Vec4f(q.x0, q.y0, q.s0, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y0, q.s1, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y1, q.s1, q.t1));
          vertices.push_back(Vec4f(q.x0, q.y1, q.s0, q.t1));

          maxx = RDCMAX(maxx, RDCMAX(q.x0, q.x1));
          maxy = RDCMAX(maxy, RDCMAX(q.y0, q.y1));
        }
        else
        {
          x += chardata[0].xadvance;
        }
        ++text;
      }
    }
    m_Platform.DrawQuads((float)m_InitParams.width, (float)m_InitParams.height, vertices);
  }
}
