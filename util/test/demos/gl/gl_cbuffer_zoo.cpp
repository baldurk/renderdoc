/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "gl_test.h"

TEST(GL_CBuffer_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests every kind of constant that can be in a cbuffer to make sure it's decoded "
      "correctly";

  std::string common = R"EOSHADER(

#version 430 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

struct vec3_1 { vec3 a; float b; };

struct nested { vec3_1 a; vec4 b[4]; vec3_1 c[4]; };

layout(binding = 0, std140) uniform constsbuf
{
  // dummy* entries are just to 'reset' packing to avoid pollution between tests

  vec4 a;                               // basic vec4 = {0, 1, 2, 3}
  vec3 b;                               // should have a padding word at the end = {4, 5, 6}, <7>

  vec2 c; vec2 d;                       // should be packed together = {8, 9}, {10, 11}
  float e; vec3 f;                      // can't be packed together = 12, <13, 14, 15>, {16, 17, 18}, <19>
  vec4 dummy0;
  float j; vec2 k;                      // should have a padding word before the vec2 = 24, <25>, {26, 27}
  vec2 l; float m;                      // should have a padding word at the end = {28, 29}, 30, <31>

  float n[4];                           // should cover 4 vec4s = 32, <33..35>, 36, <37..39>, 40, <41..43>, 44
  vec4 dummy1;

  float o[4];                           // should cover 4 vec4s = 52, <53..55>, 56, <57..59>, 60, <61..63>, 64
  float p;                              // can't be packed in with above array = 68, <69, 70, 71>
  vec4 dummy2;

  layout(column_major) mat4x4 q;        // should cover 4 vec4s.
                                        // row0: {76, 80, 84, 88}
                                        // row1: {77, 81, 85, 89}
                                        // row2: {78, 82, 86, 90}
                                        // row3: {79, 83, 87, 91}
  layout(row_major) mat4x4 r;           // should cover 4 vec4s
                                        // row0: {92, 93, 94, 95}
                                        // row1: {96, 97, 98, 99}
                                        // row2: {100, 101, 102, 103}
                                        // row3: {104, 105, 106, 107}

  layout(column_major) mat4x3 s;        // covers 4 vec4s with padding at end of each column
                                        // row0: {108, 112, 116, 120}
                                        // row1: {109, 113, 117, 121}
                                        // row2: {110, 114, 118, 122}
                                        //       <111, 115, 119, 123>
  vec4 dummy3;
  layout(row_major) mat4x3 t;           // covers 3 vec4s with no padding
                                        // row0: {128, 129, 130, 131}
                                        // row1: {132, 133, 134, 135}
                                        // row2: {136, 137, 138, 139}
  vec4 dummy4;

  layout(column_major) mat3x2 u;        // covers 3 vec4s with padding at end of each column (but not row)
                                        // row0: {144, 148, 152}
                                        // row1: {145, 149, 153}
                                        //       <146, 150, 154>
                                        //       <147, 151, 155>
  vec4 dummy5;
  layout(row_major) mat3x2 v;           // covers 2 vec4s with padding at end of each row (but not column)
                                        // row0: {160, 161, 162}, <163>
                                        // row1: {164, 165, 166}, <167>
  vec4 dummy6;

  layout(column_major) mat2x2 w;        // covers 2 vec4s with padding at end of each column (but not row)
                                        // row0: {172, 176}
                                        // row1: {173, 177}
                                        //       <174, 178>
                                        //       <175, 179>
  vec4 dummy7;
  layout(row_major) mat2x2 x;           // covers 2 vec4s with padding at end of each row (but not column)
                                        // row0: {184, 185}, <186, 187>
                                        // row1: {188, 189}, <190, 191>
  vec4 dummy8;

  layout(row_major) mat2x2 y;           // covers the same as above, and checks z doesn't overlap
                                        // row0: {196, 197}, <198, 199>
                                        // row1: {200, 201}, <202, 203>
  float z;                              // can't overlap = 204, <205, 206, 207>

  // GL Doesn't have single-column matrices
/*
  layout(row_major) mat1x4 aa;          // covers 4 vec4s with maximum padding
                                        // row0: {208}, <209, 210, 211>
                                        // row1: {212}, <213, 214, 215>
                                        // row2: {216}, <217, 218, 219>
                                        // row3: {220}, <221, 222, 223>

  layout(column_major) mat1x4 ab;       // covers 1 vec4 (equivalent to a plain vec4)
                                        // row0: {224}
                                        // row1: {225}
                                        // row2: {226}
                                        // row3: {227}
*/
  vec4 dummy9[5];

  vec4 multiarray[3][2];                // [0][0] = {228, 229, 230, 231}
                                        // [0][1] = {232, 233, 234, 235}
                                        // [1][0] = {236, 237, 238, 239}
                                        // [1][1] = {240, 241, 242, 243}
                                        // [2][0] = {244, 245, 246, 247}
                                        // [2][1] = {248, 249, 250, 251}

  nested structa[2];                      // [0] = {
                                          //   .a = { { 252, 253, 254 }, 255 }
                                          //   .b[0] = { 256, 257, 258, 259 }
                                          //   .b[1] = { 260, 261, 262, 263 }
                                          //   .b[2] = { 264, 265, 266, 267 }
                                          //   .b[3] = { 268, 269, 270, 271 }
                                          //   .c[0] = { { 272, 273, 274 }, 275 }
                                          //   .c[1] = { { 276, 277, 278 }, 279 }
                                          //   .c[2] = { { 280, 281, 282 }, 283 }
                                          //   .c[3] = { { 284, 285, 286 }, 287 }
                                          // }
                                          // [1] = {
                                          //   .a = { { 288, 289, 290 }, 291 }
                                          //   .b[0] = { 292, 293, 294, 295 }
                                          //   .b[1] = { 296, 297, 298, 299 }
                                          //   .b[2] = { 300, 301, 302, 303 }
                                          //   .b[3] = { 304, 305, 306, 307 }
                                          //   .c[0] = { { 308, 309, 310 }, 311 }
                                          //   .c[1] = { { 312, 313, 314 }, 315 }
                                          //   .c[2] = { { 316, 317, 318 }, 319 }
                                          //   .c[3] = { { 320, 321, 322 }, 323 }
                                          // }

  layout(column_major) mat2x3 ac;         // covers 2 vec4s with padding at end of each column (but not row)
                                          // row0: {324, 328}
                                          // row1: {325, 329}
                                          // row2: {326, 330}
                                          //       <327, 331>
  layout(row_major) mat2x3 ad;            // covers 3 vec4s with padding at end of each row (but not column)
                                          // row0: {332, 333}, <334, 335>
                                          // row1: {336, 337}, <338, 339>
                                          // row2: {340, 341}, <342, 343>

  layout(column_major) mat2x3 ae[2];      // covers 2 vec4s with padding at end of each column (but not row)
                                          // [0] = {
                                          //   row0: {344, 348}
                                          //   row1: {345, 349}
                                          //   row2: {346, 350}
                                          //         <347, 351>
                                          // }
                                          // [1] = {
                                          //   row0: {352, 356}
                                          //   row1: {353, 357}
                                          //   row2: {354, 358}
                                          //         <355, 359>
                                          // }
  layout(row_major) mat2x3 af[2];         // covers 3 vec4s with padding at end of each row (but not column)
                                          // [0] = {
                                          //   row0: {360, 361}, <362, 363>
                                          //   row1: {364, 365}, <366, 367>
                                          //   row2: {368, 369}, <370, 371>
                                          // }
                                          // [1] = {
                                          //   row0: {372, 373}, <374, 375>
                                          //   row1: {376, 377}, <378, 379>
                                          //   row2: {380, 381}, <382, 383>
                                          // }

  vec2 dummy10;                           // should have padding at the end = {384, 385}, <386, 387>

  layout(row_major) mat2x2 ag;            // each row is aligned to float4:
                                          // row0: {388, 389}, <390, 391>
                                          // row1: {392, 393}, <394, 395>

  vec2 dummy11;                           // should have padding at the end = {396, 397}, <398, 399>

  layout(column_major) mat2x2 ah;         // each column is aligned to float4:
                                          // row0: {400, 404}
                                          // row1: {401, 405}
                                          //       <402, 406>
                                          //       <403, 407>

  layout(row_major) mat2x2 ai[2];         // [0] = {
                                          //   row0: {408, 409}, <410, 411>
                                          //   row1: {412, 413}, <414, 415>
                                          // }
                                          // [1] = {
                                          //   row0: {416, 417}, <418, 419>
                                          //   row1: {420, 421}, <422, 423>
                                          // }
  layout(column_major) mat2x2 aj[2];      // [0] = {
                                          //   row0: {424, 428}
                                          //   row1: {425, 429}
                                          //         <426, 430>
                                          //         <427, 431>
                                          // }
                                          // [1] = {
                                          //   row0: {432, 436}
                                          //   row1: {433, 437}
                                          //         <434, 438>
                                          //         <435, 439>
                                          // }

  vec4 test;                              // {440, 441, 442, 443}

  // because GL has worse handling of multidimensional arrays than other APIs, we add an extra test
  // here with more than 2 dimensions

  vec4 multiarray2[4][3][2];              // [0][0][0] = {444, 445, 446, 447}
                                          // [0][0][1] = {448, 449, 450, 451}
                                          // [0][1][0] = {452, ..., ..., ...}
                                          // [0][1][1] = {..., ..., ..., ...}
                                          // [0][2][0] = {..., ..., ..., ...}
                                          // [0][2][1] = {..., ..., ..., ...}
                                          // [1][0][0] = {..., ..., ..., ...}
                                          // [1][0][1] = {..., ..., ..., ...}
                                          // [1][1][0] = {..., ..., ..., ...}
                                          // [1][1][1] = {..., ..., ..., ...}
                                          // [1][2][0] = {..., ..., ..., ...}
                                          // [1][2][1] = {..., ..., ..., ...}
                                          // [2][0][0] = {..., ..., ..., ...}
                                          // [2][0][1] = {..., ..., ..., ...}
                                          // [2][1][0] = {..., ..., ..., ...}
                                          // [2][1][1] = {..., ..., ..., ...}
                                          // [2][2][0] = {..., ..., ..., ...}
                                          // [2][2][1] = {..., ..., ..., ...}
                                          // [3][0][0] = {..., ..., ..., ...}
                                          // [3][0][1] = {..., ..., ..., ...}
                                          // [3][1][0] = {..., ..., ..., ...}
                                          // [3][1][1] = {..., ..., ..., ...}
                                          // [3][2][0] = {..., ..., ..., ...}
                                          // [3][2][1] = {..., ..., ..., ...}
};

uniform vec4 A;
uniform vec2 B;
uniform vec3 C;
uniform mat2x3 D;
uniform float E[3];
uniform vec4 F[3][2][2];
uniform nested G[2];

void main()
{
  // we need to ref all of the variables we want to include to force GL to include them :(.
  float blah = a.x + b.x + c.x + d.x + e.x + f.x + j.x + k.x + l.x + m.x;
  blah += n[0] + o[0] + p.x;
  blah += q[0].x + r[0].x + s[0].x + t[0].x + u[0].x + v[0].x + w[0].x + x[0].x + y[0].x + z;
  blah += multiarray[0][0].x + ac[0][0] + ad[0][0] + ae[0][0][0] + af[0][0][0];
  blah += A.z + B.x + C.y + D[0][1] + E[2] + F[1][0][0].y + F[1][0][1].y;
  blah += G[0].a.b + G[1].a.b + G[1].b[3].w + G[1].c[3].a.y;
  blah *= vertIn.uv.z;
  Color = blah + test + vec4(0.1f, 0.0f, 0.0f, 0.0f);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(common + vertex, common + pixel);

    Vec4f cbufferdata[684];

    for(int i = 0; i < 684; i++)
      cbufferdata[i] = Vec4f(float(i * 4 + 0), float(i * 4 + 1), float(i * 4 + 2), float(i * 4 + 3));

    GLuint cb = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, cb);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(cbufferdata), cbufferdata, GL_MAP_WRITE_BIT);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glBindVertexArray(vao);

      glBindBufferBase(GL_UNIFORM_BUFFER, 0, cb);

      glUseProgram(program);

      GLint location = -1;

      location = glGetUniformLocation(program, "A");
      if(location != -1)
        glUniform4f(location, 10.0f, 20.0f, 30.0f, 40.0f);

      location = glGetUniformLocation(program, "B");
      if(location != -1)
        glUniform2f(location, 50.0f, 60.0f);

      location = glGetUniformLocation(program, "C");
      if(location != -1)
        glUniform3f(location, 70.0f, 80.0f, 90.0f);

      // implicitly column major, so:
      // {100.0, 130.0}
      // {110.0, 140.0}
      // {120.0, 150.0}
      float D[2 * 3] = {
          100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f,
      };
      location = glGetUniformLocation(program, "D");
      if(location != -1)
        glUniformMatrix2x3fv(location, 1, false, D);

      float E[3] = {160.0f, 170.0f, 180.0f};
      location = glGetUniformLocation(program, "E");
      if(location != -1)
        glUniform1fv(location, 3, E);

      float val = 190.0f;

      float F[2 * 4] = {};

#define NEXT_F()                 \
  for(int x = 0; x < 2 * 4; x++) \
  {                              \
    F[x] = val;                  \
    val += 10.0f;                \
  }

      NEXT_F();
      location = glGetUniformLocation(program, "F[0][0]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      NEXT_F();
      location = glGetUniformLocation(program, "F[0][1]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      NEXT_F();
      location = glGetUniformLocation(program, "F[1][0]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      NEXT_F();
      location = glGetUniformLocation(program, "F[1][1]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      NEXT_F();
      location = glGetUniformLocation(program, "F[2][0]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      NEXT_F();
      location = glGetUniformLocation(program, "F[2][1]");
      if(location != -1)
        glUniform4fv(location, 2, F);

      location = glGetUniformLocation(program, "G[0].a.a");
      if(location != -1)
        glUniform3f(location, 680.0f, 690.0f, 700.0f);

      location = glGetUniformLocation(program, "G[0].a.b");
      if(location != -1)
        glUniform1f(location, 710.0f);

      float vals[] = {
          // b[0]
          720.0f, 730.0f, 740.0f, 750.0f,
          // b[1]
          760.0f, 770.0f, 780.0f, 790.0f,
          // b[2]
          800.0f, 810.0f, 820.0f, 830.0f,
          // b[3]
          840.0f, 850.0f, 860.0f, 870.0f,
      };

      location = glGetUniformLocation(program, "G[0].b");
      if(location != -1)
        glUniform4fv(location, 4, vals);

      location = glGetUniformLocation(program, "G[0].c[0].a");
      if(location != -1)
        glUniform3f(location, 880.0f, 890.0f, 900.0f);

      location = glGetUniformLocation(program, "G[0].c[0].b");
      if(location != -1)
        glUniform1f(location, 910.0f);

      location = glGetUniformLocation(program, "G[0].c[1].a");
      if(location != -1)
        glUniform3f(location, 920.0f, 930.0f, 940.0f);

      location = glGetUniformLocation(program, "G[0].c[1].b");
      if(location != -1)
        glUniform1f(location, 950.0f);

      location = glGetUniformLocation(program, "G[0].c[2].a");
      if(location != -1)
        glUniform3f(location, 960.0f, 970.0f, 980.0f);

      location = glGetUniformLocation(program, "G[0].c[2].b");
      if(location != -1)
        glUniform1f(location, 990.0f);

      location = glGetUniformLocation(program, "G[0].c[3].a");
      if(location != -1)
        glUniform3f(location, 1000.0f, 1010.0f, 1020.0f);

      location = glGetUniformLocation(program, "G[0].c[3].b");
      if(location != -1)
        glUniform1f(location, 1030.0f);

      location = glGetUniformLocation(program, "G[1].a.a");
      if(location != -1)
        glUniform3f(location, 1040.0f, 1050.0f, 1060.0f);

      location = glGetUniformLocation(program, "G[1].a.b");
      if(location != -1)
        glUniform1f(location, 1070.0f);

      float vals2[] = {
          // b[0]
          1080.0f, 1090.0f, 1100.0f, 1110.0f,
          // b[1]
          1120.0f, 1130.0f, 1140.0f, 1150.0f,
          // b[2]
          1160.0f, 1170.0f, 1180.0f, 1190.0f,
          // b[3]
          1200.0f, 1210.0f, 1220.0f, 1230.0f,
      };

      location = glGetUniformLocation(program, "G[1].b");
      if(location != -1)
        glUniform4fv(location, 4, vals2);

      location = glGetUniformLocation(program, "G[1].c[0].a");
      if(location != -1)
        glUniform3f(location, 1240.0f, 1250.0f, 1260.0f);

      location = glGetUniformLocation(program, "G[1].c[0].b");
      if(location != -1)
        glUniform1f(location, 1270.0f);

      location = glGetUniformLocation(program, "G[1].c[1].a");
      if(location != -1)
        glUniform3f(location, 1280.0f, 1290.0f, 1300.0f);

      location = glGetUniformLocation(program, "G[1].c[1].b");
      if(location != -1)
        glUniform1f(location, 1310.0f);

      location = glGetUniformLocation(program, "G[1].c[2].a");
      if(location != -1)
        glUniform3f(location, 1320.0f, 1330.0f, 1340.0f);

      location = glGetUniformLocation(program, "G[1].c[2].b");
      if(location != -1)
        glUniform1f(location, 1350.0f);

      location = glGetUniformLocation(program, "G[1].c[3].a");
      if(location != -1)
        glUniform3f(location, 1360.0f, 1370.0f, 1380.0f);

      location = glGetUniformLocation(program, "G[1].c[3].b");
      if(location != -1)
        glUniform1f(location, 1390.0f);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
