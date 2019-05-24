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

#pragma once

#if defined(WIN32)
#include "win32/win32_platform.h"
#else
#include "linux/linux_platform.h"
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include "renderdoc_app.h"

typedef uint8_t byte;

enum class SPIRVTarget
{
  opengl,
  vulkan
};
enum class ShaderLang
{
  glsl,
  hlsl
};
enum class ShaderStage
{
  vert,
  tesscontrol,
  tesseval,
  geom,
  frag,
  comp
};

bool InternalSpvCompiler();
bool SpvCompilationSupported();
std::vector<uint32_t> CompileShaderToSpv(const std::string &source_text, SPIRVTarget target,
                                         ShaderLang lang, ShaderStage stage, const char *entry_point);

struct Vec2f
{
  Vec2f(float X = 0.0f, float Y = 0.0f)
  {
    x = X;
    y = Y;
  }
  float x;
  float y;
};

class Vec3f
{
public:
  Vec3f(const float X = 0.0f, const float Y = 0.0f, const float Z = 0.0f) : x(X), y(Y), z(Z) {}
  inline float Dot(const Vec3f &o) const { return x * o.x + y * o.y + z * o.z; }
  inline Vec3f Cross(const Vec3f &o) const
  {
    return Vec3f(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
  }

  inline float Length() const { return sqrt(Dot(*this)); }
  inline void Normalise()
  {
    float l = Length();
    x /= l;
    y /= l;
    z /= l;
  }

  float x, y, z;
};

struct Vec4f
{
  Vec4f(float X = 0.0f, float Y = 0.0f, float Z = 0.0f, float W = 0.0f)
  {
    x = X;
    y = Y;
    z = Z;
    w = W;
  }
  float x, y, z, w;
};

struct Vec4i
{
  Vec4i(int32_t X = 0, int32_t Y = 0, int32_t Z = 0, int32_t W = 0)
  {
    x = X;
    y = Y;
    z = Z;
    w = W;
  }
  int32_t x, y, z, w;
};

struct DefaultA2V
{
  Vec3f pos;
  Vec4f col;
  Vec2f uv;
};

extern const DefaultA2V DefaultTri[3];
extern const char *SmileyTexture[63];

struct Texture
{
  uint32_t width;
  uint32_t height;
  std::vector<uint32_t> data;
};

void LoadXPM(const char **XPM, Texture &tex);

struct GraphicsWindow
{
  virtual ~GraphicsWindow() {}
  virtual void Resize(int width, int height) = 0;
  virtual bool Update() = 0;
};

struct GraphicsTest
{
  virtual ~GraphicsTest() {}
  virtual GraphicsWindow *MakeWindow(int width, int height, const char *title) { return NULL; }
  virtual int main() { return 9; }
  virtual void Prepare(int argc, char **argv);
  virtual bool Init();
  virtual void Shutdown();

  std::string Avail;

  std::string GetDataPath(const std::string &filename);

  bool FrameLimit();

  int curFrame = 0;

  const char *screenTitle = "RenderDoc test program";

  bool headless = false;

  RENDERDOC_API_1_0_0 *rdoc = NULL;

  // shared parameters
  static int maxFrameCount;
  static std::string dataRoot;
  static int screenWidth;
  static int screenHeight;
  static bool debugDevice;
};

enum class TestAPI
{
  D3D11,
  Vulkan,
  OpenGL,
  D3D12,
  Count,
};

inline const char *APIName(TestAPI API)
{
  switch(API)
  {
    case TestAPI::D3D11: return "D3D11";
    case TestAPI::Vulkan: return "Vulkan";
    case TestAPI::OpenGL: return "OpenGL";
    case TestAPI::D3D12: return "D3D12";
    case TestAPI::Count: break;
  }

  return "???";
}

struct TestMetadata
{
  TestAPI API;
  const char *Name;
  const char *Description;
  GraphicsTest *test;

  bool IsAvailable() const { return test->Avail.empty(); }
  const char *AvailMessage() const { return test->Avail.c_str(); }
  bool operator<(const TestMetadata &o)
  {
    if(API != o.API)
      return API < o.API;

    int ret = strcmp(Name, o.Name);
    if(ret != 0)
      return ret < 0;

    return test < o.test;
  }
};

void RegisterTest(TestMetadata test);

#define TEST(Test, Parent)                \
  struct Test;                            \
  typedef Test CurrentTest;               \
  namespace                               \
  {                                       \
  constexpr const char *TestName = #Test; \
  };                                      \
  struct Test : Parent

#define REGISTER_TEST()                            \
  namespace                                        \
  {                                                \
  struct TestRegistration                          \
  {                                                \
    CurrentTest m_impl;                            \
    TestRegistration()                             \
    {                                              \
      TestMetadata test;                           \
      test.API = CurrentTest::API;                 \
      test.Name = TestName;                        \
      test.Description = CurrentTest::Description; \
      test.test = &m_impl;                         \
      RegisterTest(test);                          \
    }                                              \
  } Anon##__LINE__;                                \
  };

std::string GetCWD();
std::string GetEnvVar(const char *var);

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define RANDF(mn, mx) ((float(rand()) / float(RAND_MAX)) * ((mx) - (mn)) + (mn))

template <typename T>
inline T AlignUp(T x, T a)
{
  return (x + (a - 1)) & (~(a - 1));
}

template <typename T, typename A>
inline T AlignUpPtr(T x, A a)
{
  return (T)AlignUp<uintptr_t>((uintptr_t)x, (uintptr_t)a);
}

std::string strlower(const std::string &str);
std::string strupper(const std::string &str);
std::string trim(const std::string &str);

void DebugPrint(const char *fmt, ...);

#define TEST_ASSERT(cond, fmt, ...)                                                               \
  if(!(cond))                                                                                     \
  {                                                                                               \
    DebugPrint("%s:%d Assert Failure '%s': " fmt "\n", __FILE__, __LINE__, #cond, ##__VA_ARGS__); \
    DEBUG_BREAK();                                                                                \
  }

#define TEST_LOG(fmt, ...)                                                 \
  do                                                                       \
  {                                                                        \
    DebugPrint("%s:%d Log: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
  } while(0)
#define TEST_WARN(fmt, ...)                                                    \
  do                                                                           \
  {                                                                            \
    DebugPrint("%s:%d Warning: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
  } while(0)
#define TEST_ERROR(fmt, ...)                                                 \
  do                                                                         \
  {                                                                          \
    DebugPrint("%s:%d Error: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    DEBUG_BREAK();                                                           \
  } while(0)
#define TEST_FATAL(fmt, ...)                                                       \
  do                                                                               \
  {                                                                                \
    DebugPrint("%s:%d Fatal Error: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    DEBUG_BREAK();                                                                 \
    exit(0);                                                                       \
  } while(0)
#define TEST_UNIMPLEMENTED(fmt, ...)                                                 \
  do                                                                                 \
  {                                                                                  \
    DebugPrint("%s:%d Unimplemented: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    DEBUG_BREAK();                                                                   \
    exit(0);                                                                         \
  } while(0)
