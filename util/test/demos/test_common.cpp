/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019-2021 Baldur Karlsson
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

void DebugPrint(const char *fmt, ...)
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

#ifndef HAVE_SHADERC
#define HAVE_SHADERC 0
#endif

// this define toggles on/off using the linked shaderc. This can be useful for quick testing without
// having to remove the built shaderc files
#define USE_LINKED_SHADERC (1 && HAVE_SHADERC)

#if USE_LINKED_SHADERC
#include <shaderc/shaderc.h>
#else
typedef void *shaderc_compiler_t;
#endif

static shaderc_compiler_t shaderc = NULL;
static std::string externalCompiler;

bool InternalSpvCompiler()
{
  return bool(USE_LINKED_SHADERC);
}

bool SpvCompilationSupported()
{
  if(shaderc)
    return true;

#if USE_LINKED_SHADERC
  shaderc = shaderc_compiler_initialize();
#endif

  if(shaderc)
    return true;

  for(std::string compiler : {"glslc", "glslangValidator"})
  {
    FILE *pipe = popen((compiler + EXECUTABLE_SUFFIX " --version 2>&1").c_str(), "r");

    if(!pipe)
      continue;

    msleep(500);

    int code = pclose(pipe);

    if(WEXITSTATUS(code) == 0)
    {
      externalCompiler = compiler;
      return true;
    }
  }

  return false;
}

std::vector<uint32_t> CompileShaderToSpv(const std::string &source_text, SPIRVTarget target,
                                         ShaderLang lang, ShaderStage stage, const char *entry_point,
                                         const std::map<std::string, std::string> &macros)
{
  std::vector<uint32_t> ret;

  if(shaderc)
  {
#if USE_LINKED_SHADERC
    static bool logged = false;

    if(!logged)
    {
      logged = true;
      TEST_LOG("Compiling using built-in shaderc");
    }
    shaderc_compile_options_t opts = shaderc_compile_options_initialize();

    if(lang == ShaderLang::glsl)
      shaderc_compile_options_set_source_language(opts, shaderc_source_language_glsl);
    else if(lang == ShaderLang::hlsl)
      shaderc_compile_options_set_source_language(opts, shaderc_source_language_hlsl);

    shaderc_compile_options_set_generate_debug_info(opts);
    shaderc_compile_options_set_optimization_level(opts, shaderc_optimization_level_zero);

    shaderc_shader_kind shader_kind = shaderc_glsl_vertex_shader;

    switch(stage)
    {
      case ShaderStage::vert: shader_kind = shaderc_vertex_shader; break;
      case ShaderStage::frag: shader_kind = shaderc_fragment_shader; break;
      case ShaderStage::tesscontrol: shader_kind = shaderc_tess_control_shader; break;
      case ShaderStage::tesseval: shader_kind = shaderc_tess_evaluation_shader; break;
      case ShaderStage::geom: shader_kind = shaderc_geometry_shader; break;
      case ShaderStage::comp: shader_kind = shaderc_compute_shader; break;
    }

    if(target == SPIRVTarget::opengl)
      shaderc_compile_options_set_target_env(opts, shaderc_target_env_opengl, 0);
    else if(target == SPIRVTarget::vulkan11)
      shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan,
                                             shaderc_env_version_vulkan_1_1);
    else if(target == SPIRVTarget::vulkan12)
      shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan,
                                             shaderc_env_version_vulkan_1_2);

    for(auto it : macros)
      shaderc_compile_options_add_macro_definition(opts, it.first.c_str(), it.first.length(),
                                                   it.second.c_str(), it.second.length());

    shaderc_compilation_result_t res;

    if(lang == ShaderLang::spvasm)
      res = shaderc_assemble_into_spv(shaderc, source_text.c_str(), source_text.size(), opts);
    else
      res = shaderc_compile_into_spv(shaderc, source_text.c_str(), source_text.size(), shader_kind,
                                     "inshader", entry_point, opts);

    shaderc_compilation_status status = shaderc_result_get_compilation_status(res);

    if(status != shaderc_compilation_status_success)
    {
      TEST_ERROR("Couldn't compile shader with built-in shaderc: %s",
                 shaderc_result_get_error_message(res));

      if(res)
        shaderc_result_release(res);

      return ret;
    }

    size_t sz = shaderc_result_get_length(res);

    TEST_ASSERT((sz % 4) == 0, "shaderc result isn't 4-byte aligned");

    ret.resize(sz / 4);

    memcpy(&ret[0], shaderc_result_get_bytes(res), sz);

    shaderc_result_release(res);

    shaderc_compile_options_release(opts);

    return ret;
#endif
  }

  std::string command_line;

  std::string path = GetExecutableName();
  path.erase(path.find_last_of("/\\"));
  path += "/tmp";

  MakeDir(path.c_str());

  std::string infile = path + "/input";
  std::string outfile = path + "/output";

  if(externalCompiler == "glslc")
  {
    command_line = "glslc" EXECUTABLE_SUFFIX " -g -O0";
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
    command_line = "glslangValidator" EXECUTABLE_SUFFIX " -g ";
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

  msleep(100);

  int code = pclose(pipe);

  if(code != 0)
  {
    TEST_ERROR("Invoking %s failed: %s.", externalCompiler.c_str(), command_line.c_str());
    return ret;
  }

  f = fopen(outfile.c_str(), "rb");
  if(f)
  {
    fseek(f, 0, SEEK_END);
    ret.resize(ftell(f) / sizeof(uint32_t));
    fseek(f, 0, SEEK_SET);
    fread(&ret[0], sizeof(uint32_t), ret.size(), f);
    fclose(f);
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

  prepared = true;

  dataRoot = GetEnvVar("RENDERDOC_DEMOS_DATA");

  if(dataRoot.empty())
    dataRoot = GetCWD() + "/data/demos/";

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
}

bool GraphicsTest::Init()
{
  srand(0U);

  pRENDERDOC_GetAPI RENDERDOC_GetAPI = NULL;

#if defined(WIN32)
  HMODULE mod = GetModuleHandleA("renderdoc.dll");
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
#else
  void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
  if(mod)
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
#endif

  if(RENDERDOC_GetAPI)
  {
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_0_0, (void **)&rdoc);

    if(ret != 1)
      rdoc = NULL;
  }

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
