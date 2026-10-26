/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2026 Baldur Karlsson
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

#include "test_common.h"
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>

const DefaultA2V DefaultTri[3] = {
    {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
    {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
};

/* XPM */
const char *SmileyTexture[63] = {
    /* columns rows colors chars-per-pixel */
    "48 48 14 1 ", "  c #000017", ". c #FF0017", "X c #FF735C", "o c #00FF17", "O c #FF8B5C",
    "+ c #D0B95C", "@ c #E7A25C", "# c #B9D05C", "$ c #8BFF45", "% c #A2E745", "& c #A2FF45",
    "* c #A2E75C", "= c #1700FF", "- c #B9FFFF",
    /* pixels */
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "---------------------      ---------------------",
    "-----------------              -----------------",
    "---------------    XOOOOOOO@@    ---------------",
    "-------------   XXXOOOOOO@@@@@@@   -------------",
    "------------   XXXOOOOOO@@@@@@@@@   ------------",
    "-----------  XXXXOOOOOO@@@@@@@@@@++  -----------",
    "----------  XXXXOOOOOO@@@@@@@@@@++++  ----------",
    "---------  XXXXOOOOOO@@@@@@@@@@++++++  ---------",
    "--------  XXXXOOOOOO@@@@@@@@@++++++++#  --------",
    "--------  XXXOOOOOO@@@@@@@@@++++++++##  --------",
    "-------  XXXOOOOOO@@@@@@@@@++++++++####  -------",
    "------- XXOOOOO...@@@@@@@@+++++ooo###### -------",
    "------  XOOOOO.....@@@@@@+++++ooooo#####  ------",
    "------  OOOOOO.....@@@@@++++++ooooo#####  ------",
    "------ OOOOOOO.....@@@@+++++++ooooo####%% ------",
    "------ OOOOOO@.....@@@++++++++ooooo###%%* ------",
    "-----  OOOOO@@.....@@++++++++#ooooo##%%*%  -----",
    "-----  OOOO@@@@...@@++++++++###ooo##%%*%%  -----",
    "-----  OOO@@@@@@@@@++++++++########%%*%%%  -----",
    "-----  O@@@@@@@@@@++++++++########%%*%%%%  -----",
    "-----  @@@@@@@@@@++++++++########%%*%%%%%  -----",
    "-----  @@@@@@@@@++++++++########%%*%%%%%&  -----",
    "------ @@@@@@@@++++++++########%%*%%%%%&& ------",
    "------ @@@@==+++++++++########%%*%%==%&&& ------",
    "------  @@@===+++++++########%%*%%===&&&  ------",
    "------  @@@+===+++++########%%*%%%==&&&$  ------",
    "------- @@+++===+++#######%%%*%%%==&&&$$ -------",
    "-------  +++++===+#######%%%*%%%==&&&$$  -------",
    "--------  +++++===######%%%%%%%===&&$$  --------",
    "--------  +++++#====###%%*%%%====&&$$$  --------",
    "---------  +++####===#%%*%%%====&&$$$  ---------",
    "----------  +######===========&&&$$$  ----------",
    "-----------  #######%=======&&&&$$$  -----------",
    "------------   ####%%*%%%%%&&&&$$   ------------",
    "-------------   ##%%*%%%%%&&&&$$   -------------",
    "---------------    *%%%%%&&&&    ---------------",
    "-----------------              -----------------",
    "---------------------      ---------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------",
    "------------------------------------------------"};

// since tolower is int -> int, this warns below. make a char -> char alternative
char toclower(char c)
{
  return (char)tolower(c);
}

char tocupper(char c)
{
  return (char)toupper(c);
}

uint16_t MakeHalf(float f)
{
  bool sign = f < 0.0f;
  f = sign ? -f : f;

  if(f < 1e-15f)
    return 0;

  int exp;
  f = frexpf(f, &exp);

  uint32_t mantissa;
  memcpy(&mantissa, &f, sizeof(mantissa));
  mantissa = (mantissa & 0x007fffff) >> 13;

  uint16_t ret = mantissa & 0x3ff;
  ret |= ((exp + 14) << 10);
  if(sign)
    ret |= 0x8000;

  return ret;
}

std::string strlower(const std::string &str)
{
  std::string newstr(str);
  std::transform(newstr.begin(), newstr.end(), newstr.begin(), toclower);
  return newstr;
}

std::string strupper(const std::string &str)
{
  std::string newstr(str);
  std::transform(newstr.begin(), newstr.end(), newstr.begin(), tocupper);
  return newstr;
}

std::string trim(const std::string &str)
{
  const char *whitespace = "\t \n\r";
  size_t start = str.find_first_not_of(whitespace);
  size_t end = str.find_last_not_of(whitespace);

  // no non-whitespace characters, return the empty string
  if(start == std::string::npos)
    return "";

  // searching from the start found something, so searching from the end must have too.
  return str.substr(start, end - start + 1);
}

static char printBuf[4096] = {};
static FILE *logFile = NULL;

#if defined(ANDROID)
#include <android/log.h>
#endif

static bool debugLogEnabled = true;

void SetDebugLogEnabled(bool enabled)
{
  debugLogEnabled = enabled;
}

void DebugPrint(const char *fmt, ...)
{
  if(!debugLogEnabled)
    return;

  va_list args;
  va_start(args, fmt);

  vsnprintf(printBuf, 4095, fmt, args);

  va_end(args);

  fputs(printBuf, stdout);
  fflush(stdout);

  if(logFile)
  {
    fputs(printBuf, logFile);
    fflush(logFile);
  }

#if defined(WIN32)
  OutputDebugStringA(printBuf);
#endif

#if defined(ANDROID)
  __android_log_print(ANDROID_LOG_DEBUG, "rd_demos", "%s", printBuf);
#endif
}

void OutputPrint(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  vsnprintf(printBuf, 4095, fmt, args);

  va_end(args);

  fputs(printBuf, stdout);
  fflush(stdout);

  if(logFile)
  {
    fputs(printBuf, logFile);
    fflush(logFile);
  }

#if defined(WIN32)
  OutputDebugStringA(printBuf);
#endif

#if defined(ANDROID)
  __android_log_print(ANDROID_LOG_INFO, "rd_demos", "%s", printBuf);
#endif
}

void LoadXPM(const char **XPM, Texture &tex)
{
  uint32_t numColors = 0;
  sscanf(XPM[0], "%u %u %u %*u", &tex.width, &tex.height, &numColors);

  uint32_t colors[256] = {};

  for(uint32_t c = 0; c < numColors; c++)
  {
    char ch = XPM[c + 1][0];

    uint32_t col = 0;

    sscanf(XPM[c + 1] + 1, " c #%x", &col);

    // need to bgr swap, set full alpha
    colors[ch] = 0xff000000 | ((col & 0xff) << 16) | (col & 0xff00) | ((col & 0xff0000) >> 16);
  }

  tex.data.resize(tex.width * tex.height);

  for(uint32_t y = 0; y < tex.height; y++)
  {
    const char *row = XPM[1 + numColors + y];

    for(uint32_t x = 0; x < tex.width; x++)
      tex.data[y * tex.width + x] = colors[row[x]];
  }
}

static std::string externalCompiler;

bool SpvCompilationSupported()
{
  for(std::string compiler : {"glslc", "glslangValidator"})
  {
    FILE *pipe = popen((compiler + EXECUTABLE_SUFFIX " --version 2>&1").c_str(), "r");

    if(!pipe)
      continue;

    char dummy[512];
    while(!feof(pipe))
    {
      fread(dummy, 1, 512, pipe);
    }

    int code = pclose(pipe);

    if(WEXITSTATUS(code) == 0)
    {
      externalCompiler = compiler;
      return true;
    }
  }

  return false;
}

std::vector<uint32_t> CompileShaderToSpv(const char *demo_name, const std::string &source_text,
                                         SPIRVTarget target, ShaderLang lang, ShaderStage stage,
                                         const char *entry_point,
                                         const std::map<std::string, std::string> &macros,
                                         bool debug_info)
{
  std::vector<uint32_t> ret;

  std::string command_line;

  std::string path = GetExecutableName();
  path.erase(path.find_last_of("/\\"));
  path += "/tmp/";
  MakeDir(path.c_str());
  path += demo_name;

  MakeDir(path.c_str());

  std::string infile = path + "/input";
  std::string outfile = path + "/output";

  if(externalCompiler == "glslc")
  {
    command_line = "glslc" EXECUTABLE_SUFFIX;
    if(debug_info)
      command_line += " -g -O0";
    command_line += " -fentry-point=";
    command_line += entry_point;

    if(lang == ShaderLang::glsl)
      command_line += " -x glsl";
    else if(lang == ShaderLang::hlsl)
      command_line += " -x hlsl";

    for(auto it : macros)
      command_line += " -D" + it.first + "=" + it.second;

    // can't compile SPIR-V assembly if we specify a shader stage
    if(lang != ShaderLang::spvasm)
    {
      switch(stage)
      {
        case ShaderStage::vert: command_line += " -fshader-stage=vert"; break;
        case ShaderStage::frag: command_line += " -fshader-stage=frag"; break;
        case ShaderStage::tesscontrol: command_line += " -fshader-stage=tesscontrol"; break;
        case ShaderStage::tesseval: command_line += " -fshader-stage=tesseval"; break;
        case ShaderStage::geom: command_line += " -fshader-stage=geom"; break;
        case ShaderStage::comp: command_line += " -fshader-stage=comp"; break;
        case ShaderStage::mesh: command_line += " -fshader-stage=mesh"; break;
        case ShaderStage::task: command_line += " -fshader-stage=task"; break;
      }
    }
    else
    {
      infile += ".spvasm";
    }

    if(target == SPIRVTarget::opengl)
      command_line += " --target-env=opengl";
    else if(target == SPIRVTarget::vulkan11)
      command_line += " --target-env=vulkan1.1";
    else if(target == SPIRVTarget::vulkan12)
      command_line += " --target-env=vulkan1.2";

    command_line += " -o ";
    command_line += outfile;
    command_line += " ";
    command_line += infile;
  }
  else if(externalCompiler == "glslangValidator")
  {
    command_line = "glslangValidator" EXECUTABLE_SUFFIX;
    if(debug_info)
      command_line += " -g ";
    command_line += " --entry-point ";
    command_line += entry_point;

    if(lang == ShaderLang::hlsl)
      command_line += " -D";

    if(lang == ShaderLang::spvasm)
    {
      TEST_ERROR("Can't compile SPIR-V assembly with glslangValidator");
      return ret;
    }

    for(auto it : macros)
      command_line += " -D" + it.first + "=" + it.second;

    switch(stage)
    {
      case ShaderStage::vert: command_line += " -S vert"; break;
      case ShaderStage::frag: command_line += " -S frag"; break;
      case ShaderStage::tesscontrol: command_line += " -S tesscontrol"; break;
      case ShaderStage::tesseval: command_line += " -S tesseval"; break;
      case ShaderStage::geom: command_line += " -S geom"; break;
      case ShaderStage::comp: command_line += " -S comp"; break;
      case ShaderStage::mesh: command_line += " -S mesh"; break;
      case ShaderStage::task: command_line += " -S task"; break;
    }

    if(target == SPIRVTarget::opengl)
      command_line += " -G --target-env opengl";
    else if(target == SPIRVTarget::vulkan11)
      command_line += " -V --target-env vulkan1.1";
    else if(target == SPIRVTarget::vulkan12)
      command_line += " -V --target-env vulkan1.2";
    else if(target == SPIRVTarget::vulkan)
      command_line += " -V --target-env vulkan1.0";

    command_line += " -o ";
    command_line += outfile;
    command_line += " ";
    command_line += infile;
  }

  FILE *f = fopen(infile.c_str(), "wb");
  if(f)
  {
    fwrite(source_text.c_str(), 1, source_text.size(), f);
    fclose(f);
  }

  FILE *pipe = popen(command_line.c_str(), "r");

  if(!pipe)
  {
    TEST_ERROR("Couldn't run %s to compile shaders.", externalCompiler.c_str());
    return ret;
  }

  char dummy[512];
  while(!feof(pipe))
  {
    fread(dummy, 1, 512, pipe);
  }

  pclose(pipe);

  f = fopen(outfile.c_str(), "rb");
  if(f)
  {
    fseek(f, 0, SEEK_END);
    ret.resize(ftell(f) / sizeof(uint32_t));
    fseek(f, 0, SEEK_SET);
    fread(&ret[0], sizeof(uint32_t), ret.size(), f);
    fclose(f);
  }
  else
  {
    TEST_ERROR("Failed to run compiler:\n%s", command_line.c_str());
  }

  unlink(infile.c_str());
  unlink(outfile.c_str());

  return ret;
}

int GraphicsTest::maxFrameCount = -1;
std::string GraphicsTest::dataRoot;
int GraphicsTest::screenWidth = 400;
int GraphicsTest::screenHeight = 300;
bool GraphicsTest::debugDevice = false;

void GraphicsTest::Prepare(int argc, char **argv)
{
  static bool prepared = false;

  // nothing to do per-test if we've already prepared
  if(prepared)
    return;

  TEST_LOG("Requires glslc/shaderc for Vulkan tests");

  prepared = true;

  dataRoot = GetEnvVar("RENDERDOC_DEMOS_DATA");

  if(dataRoot.empty())
    dataRoot = GetCWD() + "/data/demos/";

  tmpPath = GetExecutableName();
  tmpPath.erase(tmpPath.find_last_of("/\\"));
  tmpPath += "/tmp";
  MakeDir(tmpPath.c_str());

  // parse parameters
  for(int i = 0; i < argc; i++)
  {
    if(!strcmp(argv[i], "--debug") || !strcmp(argv[i], "--validate"))
    {
      debugDevice = true;
    }

    if(i + 1 < argc && (!strcmp(argv[i], "--frames") || !strcmp(argv[i], "--framecount") ||
                        !strcmp(argv[i], "--max-frames")))
    {
      maxFrameCount = atoi(argv[i + 1]);
    }

    if(i + 1 < argc && !strcmp(argv[i], "--log"))
    {
      logFile = fopen(argv[i + 1], "w");
    }

    if(i + 1 < argc && !strcmp(argv[i], "--tmp"))
    {
      tmpPath = argv[i + 1];
    }

    if(i + 1 < argc && (!strcmp(argv[i], "--width") || !strcmp(argv[i], "-w")))
    {
      screenWidth = atoi(argv[i + 1]);

      if(screenWidth < 1)
        screenWidth = 1;
      if(screenWidth > 7680)
        screenWidth = 7680;
    }

    if(i + 1 < argc && (!strcmp(argv[i], "--height") || !strcmp(argv[i], "-h")))
    {
      screenHeight = atoi(argv[i + 1]);

      if(screenHeight < 1)
        screenHeight = 1;
      if(screenHeight > 4320)
        screenHeight = 4320;
    }

    if(i + 1 < argc && !strcmp(argv[i], "--data"))
    {
      dataRoot = argv[i + 1];
      while(dataRoot.back() == '/' || dataRoot.back() == '\\')
        dataRoot.pop_back();
      dataRoot += "/";
    }
  }

  tmpPath += "/";
  tmpPath += demoName;
  MakeDir(tmpPath.c_str());

  pRENDERDOC_GetAPI RENDERDOC_GetAPI = NULL;

#if defined(WIN32)
  HMODULE mod = GetModuleHandleA("renderdoc.dll");
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
#elif defined(ANDROID)
  void *mod = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD);
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
#elif defined(__linux__)
  void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
#elif defined(__APPLE__)
  void *mod = dlopen("librenderdoc.dylib", RTLD_NOW | RTLD_NOLOAD);
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
#else
#error UNKNOWN PLATFORM
#endif

  if(RENDERDOC_GetAPI)
  {
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_7_0, (void **)&rdoc);

    if(ret != 1)
      rdoc = NULL;
  }
}

bool GraphicsTest::Init()
{
  srand(0U);

  return true;
}

void GraphicsTest::Shutdown()
{
}

std::string GraphicsTest::GetDataPath(const std::string &filename)
{
  return dataRoot + filename;
}

bool GraphicsTest::FrameLimit()
{
  curFrame++;
  if(maxFrameCount > 0 && curFrame >= maxFrameCount)
    return false;

  return true;
}

namespace PixelHistory
{
draw makeDraw(const std::vector<DefaultA2V> &verts)
{
  uint32_t first = (uint32_t)vb.size();

  vb.insert(vb.end(), verts.begin(), verts.end());

  return {first, (uint32_t)verts.size()};
}

std::vector<DefaultA2V> vb;

draw DepthWrite;
draw UnboundPS;
draw StencilWrite;
draw Background;
draw CullFront;
draw DepthBoundsPrep;
draw DepthBoundsClip;
draw Draws300;
draw Instances300;
draw MainTest;
draw ScissorFail;
draw ScissorPass;
draw StencilRef;
draw StencilMask;
draw DepthTest;
draw SampleColour;
draw DepthEqualSetup;
draw DepthEqualFail;
draw DepthEqualPass16;
draw DepthEqualPass24;
draw DepthEqualPass32;
draw ColourMask;
draw OverflowingDraw;
draw PerFragDiscard;

void init()
{
  // this triangle occludes in depth
  CullFront = DepthWrite = makeDraw({
      {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // this triangle occludes in stencil
  UnboundPS = StencilWrite = makeDraw({
      {Vec3f(-0.5f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.5f, 0.5f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // this triangle is just in the background to contribute to overdraw
  Background = makeDraw({
      {Vec3f(-0.9f, -0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, 0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.9f, -0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  MainTest = makeDraw({
      // large gradient, failing depth and stencil on left side behind DepthWrite and StencilWrite
      {Vec3f(-0.3f, -0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.3f, 0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.5f, 0.0f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // smaller black triangle in front of background but behind gradient above
      {Vec3f(-0.2f, -0.2f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.2f, 0.0f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.2f, -0.4f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // backface culled
      {Vec3f(0.1f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.5f, -0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.5f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // depth clipped (i.e. not clamped)
      {Vec3f(0.6f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.7f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.8f, 0.0f, 1.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // scissor clips part of this triangle
  ScissorFail = makeDraw({
      {Vec3f(-0.7f, -0.8f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.5f, -0.5f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.3f, -0.8f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // scissor does clip some but passes where above fails
  ScissorPass = makeDraw({
      {Vec3f(-0.7f, -0.8f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.5f, -0.5f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.3f, -0.8f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // fails due to stencil ref
  StencilRef = makeDraw({
      {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // fails due to stencil mask
  StencilMask = makeDraw({
      {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // Six triangles, five fragments reported.
  DepthTest = makeDraw({
      // 0: Fails depth test
      {Vec3f(0.0f, -0.8f, 0.97f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.4f, -0.2f, 0.97f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.8f, -0.8f, 0.97f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // 1: Passes
      {Vec3f(0.2f, -0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.4f, -0.4f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.6f, -0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // 2: Fails depth test compared to 1st fragment
      {Vec3f(0.2f, -0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.4f, -0.6f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.6f, -0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // 3: Passes
      {Vec3f(0.2f, -0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.4f, -0.7f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.6f, -0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // 4: Fails depth bounds test
      {Vec3f(0.2f, -0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.4f, -0.7f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.6f, -0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

      // 5: Fails backface culling, not reported.
      {Vec3f(0.6f, -0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(0.4f, -0.7f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.2f, -0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
  });

  // depth bounds prep
  DepthBoundsPrep = makeDraw({
      {Vec3f(0.6f, 0.3f, 0.3f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.7f, 0.5f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.8f, 0.3f, 0.7f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });
  // depth bounds clip
  DepthBoundsClip = makeDraw({
      {Vec3f(0.6f, 0.3f, 0.3f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.7f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.8f, 0.3f, 0.7f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // 300 draws of 1 triangle
  Draws300 = makeDraw({
      {Vec3f(-0.7f, 0.0f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.8f, 0.2f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.6f, 0.2f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
  });
  // 300 instances of 1 triangle
  Instances300 = makeDraw({
      {Vec3f(-0.7f, 0.6f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.8f, 0.8f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.6f, 0.8f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
  });
  // sample colouring triangle
  SampleColour = makeDraw({
      {Vec3f(0.6f, -0.4f, 0.3f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.7f, -0.2f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.8f, -0.4f, 0.7f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // depth equal setup
  DepthEqualSetup = makeDraw({
      {Vec3f(-0.1f, -0.8f, 0.5f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, -0.6f, 0.5f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.1f, -0.8f, 0.5f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // depth equal that fails
  DepthEqualFail = makeDraw({
      {Vec3f(-0.1f, -0.8f, 0.5f + 5.0e-4f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, -0.6f, 0.5f + 5.0e-4f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.1f, -0.8f, 0.5f + 5.0e-4f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  // D16 depth equal (enough) triangle
  DepthEqualPass16 = makeDraw({
      {Vec3f(-0.1f, -0.8f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, -0.6f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.1f, -0.8f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });
  // D24 depth equal (enough) triangle
  DepthEqualPass24 = makeDraw({
      {Vec3f(-0.1f, -0.8f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, -0.6f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.1f, -0.8f, 0.5f + 1.0e-8f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });
  // D32 depth equal triangle
  DepthEqualPass32 = makeDraw({
      {Vec3f(-0.1f, -0.8f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, -0.6f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.1f, -0.8f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  });

  ColourMask = makeDraw({
      {Vec3f(-0.7f, 0.4f, 0.33f), Vec4f(3.0f, 3.0f, 3.0f, 3.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.8f, 0.6f, 0.33f), Vec4f(3.0f, 3.0f, 3.0f, 3.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.6f, 0.6f, 0.33f), Vec4f(3.0f, 3.0f, 3.0f, 3.0f), Vec2f(0.0f, 0.0f)},
  });

  // three triangles that overlap and clamp each component individually
  OverflowingDraw = makeDraw({
      {Vec3f(-0.5f, 0.6f, 0.33f), Vec4f(-1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.6f, 0.8f, 0.33f), Vec4f(-1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.4f, 0.8f, 0.33f), Vec4f(-1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

      {Vec3f(-0.5f, 0.6f, 0.33f), Vec4f(0.0f, -1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.6f, 0.8f, 0.33f), Vec4f(0.0f, -1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.4f, 0.8f, 0.33f), Vec4f(0.0f, -1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

      {Vec3f(-0.5f, 0.6f, 0.33f), Vec4f(0.0f, 0.0f, -1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.6f, 0.8f, 0.33f), Vec4f(0.0f, 0.0f, -1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.4f, 0.8f, 0.33f), Vec4f(0.0f, 0.0f, -1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
  });

  // multiple fragments overlapping the same pixel where only one discards
  PerFragDiscard = makeDraw({
      // for now we discard the first one, because Vulkan/D3D12 can't determine ordering
      {Vec3f(-0.7f, -0.2f, 0.33f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.8f, 0.0f, 0.33f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.6f, 0.0f, 0.33f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},

      {Vec3f(-0.7f, -0.2f, 0.33f), Vec4f(-1.0f, -1.0f, -1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.8f, 0.0f, 0.33f), Vec4f(-1.0f, -1.0f, -1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.6f, 0.0f, 0.33f), Vec4f(-1.0f, -1.0f, -1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
  });
};

};
