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

#include "vk_test.h"

TEST(VK_CBuffer_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests every kind of constant that can be in a cbuffer to make sure it's decoded correctly.";

  std::string common = R"EOSHADER(

#version 430 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string glslpixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

struct vec3_1 { vec3 a; float b; };

struct nested { vec3_1 a; vec4 b[4]; vec3_1 c[4]; };

layout(set = 0, binding = 0, std140) uniform constsbuf
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

  nested structa[2];                    // [0] = {
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

  layout(column_major) mat2x3 ac;       // covers 2 vec4s with padding at end of each column (but not row)
                                        // row0: {324, 328}
                                        // row1: {325, 329}
                                        // row2: {326, 330}
                                        //       <327, 331>
  layout(row_major) mat2x3 ad;          // covers 3 vec4s with padding at end of each row (but not column)
                                        // row0: {332, 333}, <334, 335>
                                        // row1: {336, 337}, <338, 339>
                                        // row2: {340, 341}, <342, 343>

  layout(column_major) mat2x3 ae[2];    // covers 2 vec4s with padding at end of each column (but not row)
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
  layout(row_major) mat2x3 af[2];       // covers 3 vec4s with padding at end of each row (but not column)
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

  vec2 dummy10;                         // should have padding at the end = {384, 385}, <386, 387>

  layout(row_major) mat2x2 ag;          // each row is aligned to float4:
                                        // row0: {388, 389}, <390, 391>
                                        // row1: {392, 393}, <394, 395>

  vec2 dummy11;                         // should have padding at the end = {396, 397}, <398, 399>

  layout(column_major) mat2x2 ah;       // each column is aligned to float4:
                                        // row0: {400, 404}
                                        // row1: {401, 405}
                                        //       <402, 406>
                                        //       <403, 407>

  layout(row_major) mat2x2 ai[2];       // [0] = {
                                        //   row0: {408, 409}, <410, 411>
                                        //   row1: {412, 413}, <414, 415>
                                        // }
                                        // [1] = {
                                        //   row0: {416, 417}, <418, 419>
                                        //   row1: {420, 421}, <422, 423>
                                        // }
  layout(column_major) mat2x2 aj[2];    // [0] = {
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

  vec4 test;                            // {440, 441, 442, 443}
};

layout (constant_id = 0) const int A = 10;
layout (constant_id = 1) const float B = 0;
layout (constant_id = 3) const bool C = false;

void main()
{
  Color = test + vec4(0.1f, 0.0f, 0.0f, 0.0f);
}

)EOSHADER";

  std::string hlslpixel = R"EOSHADER(

struct float3_1 { float3 a; float b; };

struct nested { float3_1 a; float4 b[4]; float3_1 c[4]; };

layout(set = 0, binding = 0) cbuffer consts
{
  // dummy* entries are just to 'reset' packing to avoid pollution between tests

  float4 a;                               // basic float4 = {0, 1, 2, 3}
  float3 b;                               // should have a padding word at the end = {4, 5, 6}, <7>

  float2 c; float2 d;                     // should be packed together = {8, 9}, {10, 11}
  float e; float3 f;                      // should be packed together = 12, {13, 14, 15}
  float g; float2 h; float i;             // should be packed together = 16, {17, 18}, 19
  float j; float2 k;                      // should have a padding word at the end = 20, {21, 22}, <23>
  float2 l; float m;                      // should have a padding word at the end = {24, 25}, 26, <27>

  float n[4];                             // should cover 4 float4s = 28, <29..31>, 32, <33..35>, 36, <37..39>, 40
  float4 dummy1;

  float o[4];                             // should cover 4 float4s = 48, <..>, 52, <..>, 56, <..>, 60
  float p;                                // can't be packed in with above array = 64, <65, 66, 67>
  float4 dummy2;
  float4 gldummy;

  // HLSL majorness is flipped to match column-major SPIR-V with row-major HLSL.
  // This means column major declared matrices will show up as row major in any reflection and SPIR-V
  // it also means that dimensions are flipped, so a float3x4 is declared as a float4x3, and a 'row'
  // is really a column, and vice-versa a 'column' is really a row.

  column_major float4x4 q;                // should cover 4 float4s.
                                          // row1: {76, 77, 78, 79}
                                          // row2: {80, 81, 82, 83}
                                          // row3: {84, 85, 86, 87}
                                          // row3: {88, 89, 90, 91}
  row_major float4x4 r;                   // should cover 4 float4s
                                          // row0: {92, 96, 100, 104}
                                          // row1: {93, 97, 101, 105}
                                          // row2: {94, 98, 102, 106}
                                          // row3: {95, 99, 103, 107}

  column_major float3x4 s;                // covers 4 float4s with padding at end of each 'row'
                                          // row0: {108, 109, 110}, <111>
                                          // row1: {112, 113, 114}, <115>
                                          // row2: {116, 117, 118}, <119>
                                          // row3: {120, 121, 122}, <123>
  float4 dummy3;
  row_major float3x4 t;                   // covers 3 float4s with no padding
                                          // row0: {128, 132, 136}
                                          // row1: {129, 133, 137}
                                          // row2: {130, 134, 138}
                                          // row3: {131, 135, 139}
  float4 dummy4;

  column_major float2x3 u;                // covers 3 float4s with padding at end of each 'row' (but not 'column')
                                          // row0: {144, 145}, <146, 147>
                                          // row1: {148, 149}, <150, 151>
                                          // row2: {152, 153}, <154, 155>
  float4 dummy5;
  row_major float2x3 v;                   // covers 2 float4s with padding at end of each 'column' (but not 'row')
                                          // row0: {160, 164}
                                          // row1: {161, 165}
                                          // row2: {162, 166}
                                          //       <163, 167>
  float4 dummy6;

  column_major float2x2 w;                // covers 2 float4s with padding at end of each 'row' (but not 'column')
                                          // row0: {172, 173}, <174, 175>
                                          // row1: {176, 177}, <178, 179>
  float4 dummy7;
  row_major float2x2 x;                   // covers 2 float4s with padding at end of each 'column' (but not 'row')
                                          // row0: {184, 188}
                                          // row1: {185, 189}
                                          //       <186, 190>
                                          //       <187, 191>
  float4 dummy8;

  row_major float2x2 y;                   // covers the same as above, proving z doesn't overlap
                                          // row0: {196, 200}
                                          // row1: {197, 201}
                                          //       <198, 202>
                                          //       <199, 203>
  float z;                                // doesn't overlap in final row = 204, <205, 206, 207>

  // SPIR-V can't represent single-dimension matrices properly at the moment
/*
  row_major float4x1 aa;                  // covers 4 float4s with maximum padding
                                          // row0: {208, 212, 216, 220}
                                          //       <209, 213, 217, 221>
                                          //       <210, 214, 218, 222>
                                          //       <211, 215, 219, 223>

  column_major float4x1 ab;               // covers 1 float4 (equivalent to a plain float4 after row/column swap)
                                          // row0: {224, 225, 226, 227}
*/
  float4 dummy9[5];

  float4 multiarray[3][2];                // [0][0] = {228, 229, 230, 231}
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

  column_major float3x2 ac;               // covers 2 float4s with padding at end of each column (but not row)
                                          // row0: {324, 328}
                                          // row1: {325, 329}
                                          // row2: {326, 330}
                                          //       <327, 331>
  row_major float3x2 ad;                  // covers 3 float4s with padding at end of each row (but not column)
                                          // row0: {332, 333}, <334, 335>
                                          // row1: {336, 337}, <338, 339>
                                          // row2: {340, 341}, <342, 343>

  column_major float3x2 ae[2];            // covers 2 float4s with padding at end of each column (but not row)
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
  row_major float3x2 af[2];               // covers 3 float4s with padding at end of each row (but not column)
                                          // [0] = {
                                          //   row0: {360, 361}, <362, 363>
                                          //   row1: {364, 365}, <366, 367>
                                          //   row2: {368, 369}, <370, 371>
                                          // }
                                          // [1] = {
                                          //   row0: {372, 373}, <374, 375>
                                          //   row1: {376, 377}, <378, 379>
                                          //   row2: {380, 381},
                                          // }

  float2 dummy10;                          // consumes leftovers from above array = {382, 383}

  float2 dummy11;                         // should have padding at the end = {384, 385}, <386, 387>

  row_major float2x2 ag;                  // each row is aligned to float4:
                                          // row0: {388, 389}, <390, 391>
                                          // row1: {392, 393},

  float2 dummy12;                         // consumes leftovers from above matrix = {394, 395}
  float2 dummy13;                         // should have padding at the end = {396, 397}, <398, 399>

  column_major float2x2 ah;               // each column is aligned to float4:
                                          // row0: {400, 404}
                                          // row1: {401, 405}
                                          //       <402, 406>
                                          //       <403, 407>

  row_major float2x2 ai[2];               // [0] = {
                                          //   row0: {408, 409}, <410, 411>
                                          //   row1: {412, 413}, <414, 415>
                                          // }
                                          // [1] = {
                                          //   row0: {416, 417}, <418, 419>
                                          //   row1: {420, 421}, <422, 423>
                                          // }
  column_major float2x2 aj[2];            // [0] = {
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

  float4 test;                            // {440, 441, 442, 443}
};

float4 main() : SV_Target0
{
	return test + float4(0.1f, 0.0f, 0.0f, 0.0f);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_RELAXED_BLOCK_LAYOUT_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    AllocatedImage img(
        allocator,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {imgview}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + glslpixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    float data[2] = {20.0f, 0.0f};

    // data[1] is a bool
    VkBool32 btrue = true;
    memcpy(&data[1], &btrue, sizeof(btrue));

    VkSpecializationMapEntry specmap[2] = {
        {1, 0, sizeof(float)}, {3, 4, sizeof(VkBool32)},
    };

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = 2;
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(data);
    spec.pData = data;

    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;

    VkPipeline glslpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(hlslpixel, ShaderLang::hlsl, ShaderStage::frag, "main");

    VkPipeline hlslpipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    Vec4f cbufferdata[512];

    for(int i = 0; i < 512; i++)
      cbufferdata[i] = Vec4f(float(i * 4 + 0), float(i * 4 + 1), float(i * 4 + 2), float(i * 4 + 3));

    AllocatedBuffer cb(
        allocator, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(cbufferdata);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer)}),
                });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hlslpipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();