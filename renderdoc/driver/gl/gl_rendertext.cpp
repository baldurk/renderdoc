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

#include "data/glsl_shaders.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
#include "gl_driver.h"

const float charPixelHeight = 20.0f;

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126

const int numChars = FONT_LAST_CHAR - FONT_FIRST_CHAR + 1;

stbtt_bakedchar chardata[numChars];

void WrappedOpenGL::ContextData::CreateDebugData()
{
  // if these constants change, update glsl_ubos.h and gltext.vert
  RDCCOMPILE_ASSERT(FONT_FIRST_CHAR == int(' '), "FONT_FIRST_CHAR is incorrect");
  RDCCOMPILE_ASSERT(FONT_LAST_CHAR == 126, "FONT_LAST_CHAR is incorrect");

  // to let us display the overlay on old GL contexts, use as simple a subset of functionality as
  // possible to upload the texture. VAO and shaders are used optionally on modern contexts,
  // otherwise we fall back to immediate mode rendering by hand
  if(GL.glGetIntegerv && GL.glGenTextures && GL.glBindTexture && GL.glTexImage2D && GL.glTexParameteri)
  {
    std::string ttfstring = GetEmbeddedResource(sourcecodepro_ttf);
    byte *ttfdata = (byte *)ttfstring.c_str();

    byte *buf = new byte[FONT_TEX_WIDTH * FONT_TEX_HEIGHT];

    stbtt_BakeFontBitmap(ttfdata, 0, charPixelHeight, buf, FONT_TEX_WIDTH, FONT_TEX_HEIGHT,
                         FONT_FIRST_CHAR + 1, numChars, chardata);

    CharSize = charPixelHeight;
#if ENABLED(RDOC_ANDROID)
    CharSize *= 2.0f;
#endif
    CharAspect = chardata->xadvance / charPixelHeight;

    stbtt_fontinfo f = {0};
    stbtt_InitFont(&f, ttfdata, 0);

    int ascent = 0;
    stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

    float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, charPixelHeight);

    {
      PixelUnpackState unpack;

      unpack.Fetch(false);

      ResetPixelUnpackState(false, 1);

      GLuint curtex = 0;
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&curtex);

      GLenum texFmt = eGL_R8;
      if(Legacy())
        texFmt = eGL_LUMINANCE;

      GL.glGenTextures(1, &GlyphTexture);
      GL.glBindTexture(eGL_TEXTURE_2D, GlyphTexture);
      GL.glTexImage2D(eGL_TEXTURE_2D, 0, texFmt, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, 0, eGL_RED,
                      eGL_UNSIGNED_BYTE, buf);
      GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
      GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
      GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);

      GL.glBindTexture(eGL_TEXTURE_2D, curtex);

      unpack.Apply(false);
    }

    delete[] buf;

    Vec4f posdata[numChars + 1] = {};
    Vec4f uvdata[numChars + 1] = {};

    // premultiply CharacterSize
    const Vec4f uvscale =
        Vec4f(1.0f / float(FONT_TEX_WIDTH), 1.0f / float(FONT_TEX_HEIGHT), 0.0f, 1.0f);

    for(int i = 0; i < numChars; i++)
    {
      stbtt_bakedchar *b = chardata + i;

      float x = b->xoff;
      float y = b->yoff + maxheight;

      posdata[i + 1] = Vec4f(x / b->xadvance, y / charPixelHeight, b->xadvance / float(b->x1 - b->x0),
                             charPixelHeight / float(b->y1 - b->y0));
      uvdata[i + 1] =
          Vec4f(b->x0 * uvscale.x, b->y0 * uvscale.y, b->x1 * uvscale.x, b->y1 * uvscale.y);
    }

    if(Modern())
    {
      if(GLCoreVersion > 20 && GL.glGenVertexArrays && GL.glBindVertexArray)
      {
        GLuint curvao = 0;
        GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&curvao);

        GL.glGenVertexArrays(1, &DummyVAO);
        GL.glBindVertexArray(DummyVAO);

        GL.glBindVertexArray(curvao);
      }

      if(GL.glGenBuffers && GL.glBufferData && GL.glBindBuffer)
      {
        GLuint curbuf = 0;
        GL.glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&curbuf);

        GL.glGenBuffers(1, &ArrayBuffer);
        GL.glBindBuffer(eGL_ARRAY_BUFFER, ArrayBuffer);
        GL.glBufferData(eGL_ARRAY_BUFFER, sizeof(float) * 5 * FONT_MAX_CHARS * 6, NULL,
                        eGL_DYNAMIC_DRAW);

        GL.glBindBuffer(eGL_ARRAY_BUFFER, curbuf);
      }

      if(GL.glCreateShader && GL.glShaderSource && GL.glCompileShader && GL.glGetShaderiv &&
         GL.glGetShaderInfoLog && GL.glDeleteShader && GL.glCreateProgram && GL.glAttachShader &&
         GL.glLinkProgram && GL.glGetProgramiv && GL.glGetProgramInfoLog)
      {
        std::string vs;
        std::string fs;

        ShaderType shaderType;
        int glslVersion;
        std::string vertDefines, fragDefines;

        if(IsGLES)
        {
          shaderType = eShaderGLSLES;
          glslVersion = 100;
          fragDefines = "precision highp float;";
        }
        else
        {
          shaderType = eShaderGLSL;
          glslVersion = 110;

#if ENABLED(RDOC_APPLE)
          // on mac we need to define a more modern version and use modern texture sampling
          glslVersion = GLCoreVersion * 10;
          fragDefines =
              "#define varying in\n"
              "#define texture2D texture\n"
              "#define gl_FragColor outcol\n"
              "out vec4 outcol;";
          vertDefines =
              "#define varying out\n"
              "#define attribute in";
#endif
        }

        vs = GenerateGLSLShader(GetEmbeddedResource(glsl_gltext_vert), shaderType, glslVersion,
                                vertDefines);
        fs = GenerateGLSLShader(GetEmbeddedResource(glsl_gltext_frag), shaderType, glslVersion,
                                fragDefines);

        GLuint vert = GL.glCreateShader(eGL_VERTEX_SHADER);
        GLuint frag = GL.glCreateShader(eGL_FRAGMENT_SHADER);

        const char *csrc = vs.c_str();
        GL.glShaderSource(vert, 1, &csrc, NULL);
        csrc = fs.c_str();
        GL.glShaderSource(frag, 1, &csrc, NULL);

        GL.glCompileShader(vert);
        GL.glCompileShader(frag);

        char buffer[1024] = {0};
        GLint status = 0;

        GL.glGetShaderiv(vert, eGL_COMPILE_STATUS, &status);
        if(status == 0)
        {
          GL.glGetShaderInfoLog(vert, 1024, NULL, buffer);
          RDCERR("Shader error: %s", buffer);
        }

        GL.glGetShaderiv(frag, eGL_COMPILE_STATUS, &status);
        if(status == 0)
        {
          GL.glGetShaderInfoLog(frag, 1024, NULL, buffer);
          RDCERR("Shader error: %s", buffer);
        }

        Program = GL.glCreateProgram();

        GL.glAttachShader(Program, vert);
        GL.glAttachShader(Program, frag);

        GL.glBindAttribLocation(Program, 0, "pos");
        GL.glBindAttribLocation(Program, 1, "uv");
        GL.glBindAttribLocation(Program, 2, "charidx");

        GL.glLinkProgram(Program);

        GL.glGetProgramiv(Program, eGL_LINK_STATUS, &status);
        if(status == 1)
        {
          GLuint prevProg = 0;
          GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prevProg);

          GL.glUseProgram(Program);

          // texture is from texture0
          GL.glUniform1i(GL.glGetUniformLocation(Program, "font_tex"), 0);

          // upload posdata and uvdata
          GL.glUniform4fv(GL.glGetUniformLocation(Program, "posdata"), numChars, &posdata[0].x);
          GL.glUniform4fv(GL.glGetUniformLocation(Program, "uvdata"), numChars, &uvdata[0].x);

          GL.glUseProgram(prevProg);
        }
        else
        {
          GL.glGetProgramInfoLog(Program, 1024, NULL, buffer);
          RDCERR("Link error: %s", buffer);

          GL.glDeleteProgram(Program);
          Program = 0;
        }

        GL.glDeleteShader(vert);
        GL.glDeleteShader(frag);
      }
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

  GLPushPopState textState;

  textState.Push(ctxdata.Modern());

  RenderOverlayStr(x, y, tmpBuf);

  textState.Pop(ctxdata.Modern());
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

  RDCASSERT(strlen(text) < (size_t)FONT_MAX_CHARS);

  ContextData &ctxdata = GetCtxData();

  if(!ctxdata.built || !ctxdata.ready || (ctxdata.Modern() && ctxdata.Program == 0))
    return;

  // if it's reasonably modern context, assume we can use buffers and UBOs
  if(ctxdata.Modern())
  {
    GL.glBindBuffer(eGL_ARRAY_BUFFER, ctxdata.ArrayBuffer);

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

    static const Vec2f basePos[] = {
        Vec2f(0.0, 0.0), Vec2f(1.0, 0.0), Vec2f(0.0, 1.0),
        Vec2f(1.0, 0.0), Vec2f(0.0, 1.0), Vec2f(1.0, 1.0),
    };

    const Vec2f FontScreenAspect(ctxdata.CharAspect / RDCMAX(1.0f, float(ctxdata.initParams.width)),
                                 1.0f / RDCMAX(1.0f, float(ctxdata.initParams.height)));

    float *vertexData = new float[5 * len * 6];

    for(size_t i = 0; i < len; i++)
    {
      for(int ch = 0; ch < 6; ch++)
      {
        Vec2f uv = basePos[ch];

        Vec2f pos = uv;
        pos.x += float(i) + x;
        pos.y += y;

        pos.x *= 2.0f * ctxdata.CharSize * FontScreenAspect.x;
        pos.y *= 2.0f * ctxdata.CharSize * FontScreenAspect.y;

        pos.x -= 1.0f;
        pos.y -= 1.0f;

        vertexData[(i * 6 + ch) * 5 + 0] = pos.x;
        vertexData[(i * 6 + ch) * 5 + 1] = ctxdata.initParams.isYFlipped ? pos.y : -pos.y;
        vertexData[(i * 6 + ch) * 5 + 2] = uv.x;
        vertexData[(i * 6 + ch) * 5 + 3] = uv.y;
        vertexData[(i * 6 + ch) * 5 + 4] = float(text[i] - FONT_FIRST_CHAR);
      }
    }

    // we read 6 * len vec2 positions and 6 * len float characters
    GL.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(float) * 5 * len * 6, vertexData);

    delete[] vertexData;

    //////////////////////////////////////////////////////////////////////////////////
    // Make sure if you change any other state in here, that you also update the push
    // and pop functions in GLPushPopState

    // set blend state
    if(HasExt[ARB_draw_buffers_blend])
    {
      GL.glEnablei(eGL_BLEND, 0);
      GL.glBlendFuncSeparatei(0, eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA,
                              eGL_SRC_ALPHA);
      GL.glBlendEquationSeparatei(0, eGL_FUNC_ADD, eGL_FUNC_ADD);
    }
    else
    {
      GL.glEnable(eGL_BLEND);
      GL.glBlendFuncSeparate(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA, eGL_SRC_ALPHA);
      GL.glBlendEquationSeparate(eGL_FUNC_ADD, eGL_FUNC_ADD);
    }

    if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
    {
      GL.glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    else
    {
      GL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    // set depth & stencil
    GL.glDisable(eGL_DEPTH_TEST);
    if(!IsGLES)
      GL.glDisable(eGL_DEPTH_CLAMP);
    GL.glDisable(eGL_STENCIL_TEST);
    GL.glDisable(eGL_CULL_FACE);
    GL.glDisable(eGL_RASTERIZER_DISCARD);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // set viewport & scissor
    if(HasExt[ARB_viewport_array])
    {
      GL.glViewportIndexedf(0, 0.0f, 0.0f, (float)ctxdata.initParams.width,
                            (float)ctxdata.initParams.height);
      GL.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      GL.glViewport(0, 0, ctxdata.initParams.width, ctxdata.initParams.height);
      GL.glDisable(eGL_SCISSOR_TEST);
    }

    if(!IsGLES)
      GL.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(GL.glClipControl && HasExt[ARB_clip_control])
      GL.glClipControl(eGL_LOWER_LEFT, eGL_NEGATIVE_ONE_TO_ONE);

    // bind VAO so we can fiddle with vertex attrib state
    GL.glBindVertexArray(ctxdata.DummyVAO);

    GLsizei stride = sizeof(float) * 5;

    GL.glVertexAttribPointer(0, 2, eGL_FLOAT, false, stride, (void *)uintptr_t(sizeof(float) * 0));
    GL.glVertexAttribPointer(1, 2, eGL_FLOAT, false, stride, (void *)uintptr_t(sizeof(float) * 2));
    GL.glVertexAttribPointer(2, 1, eGL_FLOAT, false, stride, (void *)uintptr_t(sizeof(float) * 4));
    GL.glEnableVertexAttribArray(0);
    GL.glEnableVertexAttribArray(1);
    GL.glEnableVertexAttribArray(2);

    // bind textures
    GL.glActiveTexture(eGL_TEXTURE0);
    GL.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);

    // bind program
    GL.glUseProgram(ctxdata.Program);

    // draw string
    GL.glDrawArrays(eGL_TRIANGLES, 0, 6 * (GLsizei)len);
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
    GL.glDisable(eGL_BLEND);
    GL.glDisable(eGL_LIGHTING);
    GL.glDisable(eGL_ALPHA_TEST);

    // set depth & stencil
    GL.glDisable(eGL_DEPTH_TEST);
    GL.glDisable(eGL_STENCIL_TEST);
    GL.glDisable(eGL_CULL_FACE);

    // set viewport & scissor
    GL.glViewport(0, 0, (GLsizei)ctxdata.initParams.width, (GLsizei)ctxdata.initParams.height);
    GL.glDisable(eGL_SCISSOR_TEST);
    if(!IsGLES)
      GL.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    // bind textures
    GL.glActiveTexture(eGL_TEXTURE0);
    GL.glBindTexture(eGL_TEXTURE_2D, ctxdata.GlyphTexture);
    GL.glEnable(eGL_TEXTURE_2D);

    if(GL.glBindFramebuffer)
      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    // just in case, try to disable the programmable pipeline
    if(GL.glUseProgram)
      GL.glUseProgram(0);
    if(GL.glBindProgramPipeline)
      GL.glBindProgramPipeline(0);

    // draw string (based on sample code from stb_truetype.h)
    std::vector<Vec4f> vertices;
    {
      y += 1.0f;
      y *= charPixelHeight;

#if ENABLED(RDOC_ANDROID)
      y *= 2.0f;
#endif

      float startx = x;
      float starty = y;

      float maxx = x, minx = x;
      float maxy = y, miny = y - charPixelHeight;

      stbtt_aligned_quad q;

      const char *prepass = text;
      while(*prepass)
      {
        char c = *prepass;
        if(c > FONT_FIRST_CHAR && c < FONT_LAST_CHAR)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - FONT_FIRST_CHAR - 1, &x,
                             &y, &q, 1);

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

      float mul = ctxdata.initParams.isYFlipped ? -1.0f : 1.0f;

      while(*text)
      {
        char c = *text;
        if(c > FONT_FIRST_CHAR && c < FONT_LAST_CHAR)
        {
          stbtt_GetBakedQuad(chardata, FONT_TEX_WIDTH, FONT_TEX_HEIGHT, c - FONT_FIRST_CHAR - 1, &x,
                             &y, &q, 1);

          vertices.push_back(Vec4f(q.x0, q.y0 * mul, q.s0, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y0 * mul, q.s1, q.t0));
          vertices.push_back(Vec4f(q.x1, q.y1 * mul, q.s1, q.t1));
          vertices.push_back(Vec4f(q.x0, q.y1 * mul, q.s0, q.t1));

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
    m_Platform.DrawQuads((float)ctxdata.initParams.width, (float)ctxdata.initParams.height, vertices);
  }
}
