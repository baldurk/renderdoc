/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
std::string CodeWriter::rootCMakeLists = R"(CMAKE_MINIMUM_REQUIRED (VERSION 3.9)

# Disable some of the default cmake build targets, keep debug and release
SET (CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

PROJECT(renderdoc_gen_frame)
SET (PROJECT_PREFIX               "")
SET (MACHINE_POSTFIX              "")
SET (MACHINE_IS_X64               TRUE)

SET (CMAKE_CXX_STANDARD           11)
IF ("${CMAKE_SIZEOF_VOID_P}"      EQUAL "8")
  SET (MACHINE_IS_X64  TRUE)
ELSEIF ("${CMAKE_SIZEOF_VOID_P}"  EQUAL "4")
  SET (MACHINE_IS_X64 FALSE)
ENDIF ()

IF (WIN32)
  IF (MACHINE_IS_X64)
    SET (MACHINE_POSTFIX            "_x64")
  ELSE ()
    SET (MACHINE_POSTFIX            "_x86")
  ENDIF ()
  IF (MSVC)
    SET (PROJECT_PREFIX              "vs${MSVC_TOOLSET_VERSION}_")
  ENDIF ()
ENDIF ()

GET_FILENAME_COMPONENT(Trace ${CMAKE_CURRENT_SOURCE_DIR} NAME)
STRING(REPLACE " " "_" Trace ${Trace})
PROJECT(${PROJECT_PREFIX}${Trace}${MACHINE_POSTFIX})

IF (MSVC)
  IF (OPTION_TREAT_WARNINGS_AS_ERRORS)
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
  ENDIF ()
ELSEIF (UNIX)
  SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")
ENDIF()

IF (MSVC)
  SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
ENDIF()

SET (EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/Build${MACHINE_POSTFIX}")
SET (LIBRARY_OUTPUT_PATH    "${CMAKE_SOURCE_DIR}/Build${MACHINE_POSTFIX}")

SET (DEBUG_POSTFIX "_DEBUG"
     CACHE STRING "Debug Postfitx for lib, samples and tools")
SET (CMAKE_RELEASE_POSTFIX ""
     CACHE STRING "Release Postfitx for lib, samples and tools")
SET (CMAKE_MINSIZEREL_POSTFIX "_MINSIZEREL"
     CACHE STRING "Minimum Size Release Postfitx for lib, samples and tools")
SET (CMAKE_RELWITHDEBINFO_POSTFIX "_RELWITHDEBINFO"
     CACHE STRING "Release With Debug Info Postfitx for lib, samples and tools")

FUNCTION (SETUP_PROJECT target)
  SET_TARGET_PROPERTIES (${target} PROPERTIES
                         OUTPUT_NAME_DEBUG   ${target}${MACHINE_POSTFIX}${DEBUG_POSTFIX}
                         OUTPUT_NAME_RELEASE ${target}${MACHINE_POSTFIX})
ENDFUNCTION ()

FIND_PACKAGE (Vulkan)
  MESSAGE (STATUS "Vulkan SDK                : $ENV{VULKAN_SDK}")
IF (Vulkan_FOUND)
  MESSAGE (STATUS "Vulkan Includes           : ${Vulkan_INCLUDE_DIRS}")
  MESSAGE (STATUS "Vulkan Libraries          : ${Vulkan_LIBRARIES}")
ELSE ()
  MESSAGE (FATAL_ERROR "Vulkan not found!")
ENDIF ()

ADD_LIBRARY (vulkan STATIC IMPORTED)

SET_TARGET_PROPERTIES (vulkan PROPERTIES
                       INTERFACE_INCLUDE_DIRECTORIES ${Vulkan_INCLUDE_DIRS}
                       IMPORTED_LOCATION             ${Vulkan_LIBRARIES})

IF (MSVC)
  SET_PROPERTY(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
               PROPERTY VS_STARTUP_PROJECT ${CMAKE_CURRENT_SOURCE_DIR}/sample_cpp_trace)
ENDIF ()

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}")

ADD_SUBDIRECTORY("${CMAKE_SOURCE_DIR}/sample_cpp_trace"
                 "${CMAKE_BINARY_DIR}/sample_cpp_trace")
ADD_SUBDIRECTORY("${CMAKE_SOURCE_DIR}/helper"
                 "${CMAKE_BINARY_DIR}/helper")
)";

/******************************************************************************/
std::string CodeWriter::projectCMakeLists = R"(IF(WIN32)
  SET (THIS_PROJECT_NAME sample_cpp_trace)
ELSE ()
  SET (THIS_PROJECT_NAME sample_cpp_trace_elf)
ENDIF ()

PROJECT(${THIS_PROJECT_NAME})

FILE(GLOB HEADERS "*.h")
FILE(GLOB SOURCES "*.cpp")
SET(SHADERS )
SET(TEMPLATES main_win.cpp main_xlib.cpp common.h)

SOURCE_GROUP ( "Source Files"   FILES ${SOURCES} )
SOURCE_GROUP ( "Template Files" FILES ${TEMPLATES} )
SOURCE_GROUP ( "Header Files"   FILES ${HEADERS} )
SOURCE_GROUP ( "Shader Files"   FILES ${SHADERS} )

ADD_EXECUTABLE(${THIS_PROJECT_NAME} ${TEMPLATES} ${SOURCES}
                                    ${HEADERS} ${SHADERS})

SETUP_PROJECT(${THIS_PROJECT_NAME})

TARGET_COMPILE_DEFINITIONS(${THIS_PROJECT_NAME} PRIVATE
                           UNICODE _UNICODE)

IF (WIN32)
  TARGET_COMPILE_DEFINITIONS(${THIS_PROJECT_NAME} PRIVATE
                             VK_USE_PLATFORM_WIN32_KHR
                             _CRT_SECURE_NO_DEPRECATE)
  TARGET_LINK_LIBRARIES(${THIS_PROJECT_NAME}
                        vulkan
                        helper
                        comctl32
                        rpcrt4
                        winmm
                        advapi32
                        wsock32
                        Dbghelp)
  SET_TARGET_PROPERTIES(${THIS_PROJECT_NAME} PROPERTIES
                        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /STACK:67108864"
                        LINK_FLAGS_DEBUG   "/SUBSYSTEM:WINDOWS /STACK:67108864")
ELSE ()
  TARGET_COMPILE_DEFINITIONS(${THIS_PROJECT_NAME} PRIVATE
                             VK_USE_PLATFORM_XLIB_KHR
                             __linux__)
  TARGET_LINK_LIBRARIES(${THIS_PROJECT_NAME}
                        libX11.so
                        libdl.so
                        helper
                        vulkan)
ENDIF ()
)";

/******************************************************************************/
std::string CodeWriter::helperCMakeLists = R"(SET (THIS_PROJECT_NAME helper)

PROJECT(${THIS_PROJECT_NAME})

ADD_LIBRARY(${THIS_PROJECT_NAME} STATIC "helper.h" "helper.cpp")

TARGET_COMPILE_DEFINITIONS(${THIS_PROJECT_NAME}
    PRIVATE UNICODE _UNICODE)
IF (NOT WIN32)
    TARGET_COMPILE_DEFINITIONS(${THIS_PROJECT_NAME}
        PRIVATE HELPER_COMPILE_STATIC_LIB)
ENDIF ()

TARGET_LINK_LIBRARIES(${THIS_PROJECT_NAME} vulkan)

SET_TARGET_PROPERTIES(${THIS_PROJECT_NAME} PROPERTIES
                      OUTPUT_NAME ${THIS_PROJECT_NAME}
                      ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_PATH}/${THIS_PROJECT_NAME}"
                      RUNTIME_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_PATH}/${THIS_PROJECT_NAME}"
                      LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_PATH}/${THIS_PROJECT_NAME}"
                      POSITION_INDEPENDENT_CODE ON)
)";

/******************************************************************************/
std::string CodeWriter::mainWinCpp = R"(//-----------------------------------------------------------------------------
// Generated with RenderDoc CPP Trace Gen
//
// File: main_win.cpp
//
//-----------------------------------------------------------------------------

// Defines the entry point that initializes and runs the serialized frame
// capture on Windows
#if _WIN32

#include <Windows.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "gen_main.h"

//-----------------------------------------------------------------------------
// Global Variable for Frame Replay
//-----------------------------------------------------------------------------
int frameLoops = -1;
double accumTimeWithReset = 0;
double accumTime = 0;
double avgTimeWithReset = 0;
double avgTime = 0;
double avgFPSWithReset = 0;
double avgFPS = 0;
uint64_t frames = 0;
double avgFrameMilliseconds = 0;
LARGE_INTEGER performanceCounterFrequency;
bool automated = false;
bool resourceReset = false;
HINSTANCE appInstance;
HWND appHwnd;

#define RDOC_WINDOW_CLASS_NAME L"RenderDoc Frame Loop"
#define RDOC_WINDOW_TITLE L"RenderDoc Frame Loop"

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL RegisterWndClass(HINSTANCE hInstance, UINT style);
HWND CreateWnd(HINSTANCE hInstance, HINSTANCE hPrevInstance, uint32_t PosX, uint32_t PosY,
               uint32_t Width, uint32_t Height, DWORD Style, DWORD ExtendedStyle);
void CreateResources();
void ReleaseResources();
void Render();

void PostStageProgress(const char *stage, uint32_t i, uint32_t N) {
  SetWindowTextA(appHwnd, StageProgressString(stage, i, N).c_str());
}

//-----------------------------------------------------------------------------
// MainWndProc
//-----------------------------------------------------------------------------
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case WM_KEYDOWN:
      if(wParam == VK_ESCAPE)
      {
        PostQuitMessage(0);
        return 0;
      }

      break;
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
      break;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// RegisterWndClass
//-----------------------------------------------------------------------------
BOOL RegisterWndClass(HINSTANCE hInstance, UINT style)
{
  // Populate the struct
  WNDCLASS wc;
  wc.style = style;
  wc.lpfnWndProc = (WNDPROC)MainWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon((HINSTANCE)NULL, IDI_APPLICATION);
  wc.hCursor = LoadCursor((HINSTANCE)NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName = L"";
  wc.lpszClassName = RDOC_WINDOW_CLASS_NAME;
  BOOL ret = RegisterClass(&wc);
  if(!ret && GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
  {
    return TRUE;
  }

  return ret;
}

//-----------------------------------------------------------------------------
// CreateWnd
//-----------------------------------------------------------------------------
HWND CreateWnd(HINSTANCE hInstance, HINSTANCE hPrevInstance, uint32_t PosX, uint32_t PosY,
               uint32_t Width, uint32_t Height, DWORD Style, DWORD ExtendedStyle)
{
  // Need to adjust first, to see if it fits the screen.
  RECT WindowRect = {(LONG)PosX, (LONG)PosY, (LONG)(PosX + Width), (LONG)(PosY + Height)};
  AdjustWindowRectEx(&WindowRect, Style, NULL, ExtendedStyle);
  Width = WindowRect.right - WindowRect.left;
  Height = WindowRect.bottom - WindowRect.top;

  return CreateWindowEx(ExtendedStyle, RDOC_WINDOW_CLASS_NAME, RDOC_WINDOW_TITLE, Style, PosX, PosY,
                        Width, Height, (HWND)NULL, (HMENU)NULL, hInstance, (LPVOID)NULL);
}

void CreateResources()
{
  RegisterWndClass(appInstance, CS_HREDRAW | CS_VREDRAW);
  // Resolution Width and Height are declared in gen_variables
  appHwnd = CreateWnd(appInstance, NULL, 0, 0, resolutionWidth, resolutionHeight,
                      WS_BORDER | WS_DLGFRAME | WS_GROUP | WS_OVERLAPPED | WS_POPUP | WS_SIZEBOX |
                          WS_SYSMENU | WS_TILED | WS_VISIBLE,
                      0);

  SetWindowTextA(appHwnd, "RenderDoc Frame Loop: Creating Resources");
  main_create();
  SetWindowTextA(appHwnd, "RenderDoc Frame Loop: Initializing Resources");
  main_init();
}

//-----------------------------------------------------------------------------
// ReleaseResources
//-----------------------------------------------------------------------------
void ReleaseResources()
{
  main_release();
}

//-----------------------------------------------------------------------------
// GetTimestampMillis
//-----------------------------------------------------------------------------
double GetTimestampMilliseconds()
{
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return 1e3 * ((double)counter.QuadPart) / performanceCounterFrequency.QuadPart;
}

//-----------------------------------------------------------------------------
// Render
//-----------------------------------------------------------------------------
void Render()
{
  double ts_pre_reset = GetTimestampMilliseconds();
  main_prereset();
  double ts_start = GetTimestampMilliseconds();
  main_render();
  main_postreset();
  double ts_end = GetTimestampMilliseconds();
  double frame_time = ts_end - ts_start;
  double frame_time_with_reset = ts_end - ts_pre_reset;

  frames++;

  accumTimeWithReset += frame_time_with_reset;
  accumTime += frame_time;
  avgTimeWithReset = accumTimeWithReset / frames;
  avgTime = accumTime / frames;
  avgFPSWithReset = 1000.0 / avgTimeWithReset;
  avgFPS = 1000.0 / avgTime;

  if(frames % 1 == 0)
  {
    char str[256];
    sprintf(str, "%s Avg Time [%f / %f] Avg FPS [%f /%f]", "RenderDoc Frame Loop", avgTimeWithReset,
            avgTime, avgFPSWithReset, avgFPS);
    SetWindowTextA(appHwnd, str);
  }
}

//-----------------------------------------------------------------------------
// ProcessMessages
//-----------------------------------------------------------------------------
static void ProcessMessages(bool &quit)
{
  MSG msg;
  if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    if(msg.message == WM_QUIT)
    {
      quit = true;
    }
    else
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

//-----------------------------------------------------------------------------
// ParseCommandLine
//-----------------------------------------------------------------------------
static bool ParseCommandLine()
{
  for(int i = 1; i < __argc; ++i)
  {
    if(0 == strcmp(__argv[i], "-repeat"))
    {
      ++i;
      if(i >= __argc)
      {
        return false;
      }
      frameLoops = atoi(__argv[i]);
    }
    else if(0 == strcmp(__argv[i], "-reset"))
    {
      resourceReset = true;
    }
    else
    {
      // Unknown command
      return false;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
// Usage
//-----------------------------------------------------------------------------
static void Usage()
{
  const wchar_t *usage =
      L"Options:\n"
      L"-repeat N    -- Number of frames to run\n"
      L"-reset       -- Perform a state reset in between frames\n";

  MessageBox(NULL, usage, L"Invalid command line", MB_ICONEXCLAMATION);
}

//-----------------------------------------------------------------------------
// WinMain
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  bool quit = false;

  appInstance = hInstance;

  // Parse the command line arguments
  if(!ParseCommandLine())
  {
    Usage();
    return EXIT_FAILURE;
  }

  try
  {
    CreateResources();

    QueryPerformanceFrequency(&performanceCounterFrequency);

    int repeatIteration = 0;
    while(frameLoops == -1 || repeatIteration < frameLoops)
    {
      ProcessMessages(quit);
      if(quit)
      {
        break;
      }

      Render();

      repeatIteration = (std::max)(0, repeatIteration + 1);
    }
  }
  catch(std::exception &e)
  {
    if(automated)
    {
      fprintf(stderr, "Error: %s", e.what());
    }
    else
    {
      std::string errorMessage(e.what());
      std::wstring errorMessageW(errorMessage.begin(), errorMessage.end());
      MessageBox(NULL, errorMessageW.c_str(), L"Error", MB_ICONEXCLAMATION);
    }
  }

  ReleaseResources();
  return EXIT_SUCCESS;
}

#endif    // if _WIN32
)";

/******************************************************************************/
std::string CodeWriter::mainXlibCpp = R"(//-----------------------------------------------------------------------------
// Generated with RenderDoc CPP Trace Gen
//
// File: main_xlib.cpp
//
//-----------------------------------------------------------------------------

// Defines the entry point that initializes and runs the serialized frame
// capture on Linux
#if defined(__linux__)

#include <stdio.h>
#include <string.h>
#include <stdexcept>

#include <X11/Xlib.h>

#include "gen_main.h"

int frameLoops = -1;
double accumTimeWithReset = 0;
double accumTime = 0;
double avgTimeWithReset = 0;
double avgTime = 0;
double avgFPSWithReset = 0;
double avgFPS = 0;
uint64_t frames = 0;
double avgFrameMilliseconds = 0;
uint64_t performanceCounterFrequency;
bool automated = false;
bool resourceReset = false;

Display *appDisplay;
Window appWindow;

#define RDOC_WINDOW_TITLE "RenderDoc Frame Loop"

void PostStageProgress(const char *stage, uint32_t i, uint32_t N) {
  XStoreName(appDisplay, appWindow, StageProgressString(stage, i, N).c_str());
}

Display *CreateDisplay()
{
  Display *res = XOpenDisplay(nullptr);
  if(!res)
  {
    throw std::runtime_error("Failed to open appDisplay");
  }
  return res;
}

Window CreateWindow()
{
  int screen = DefaultScreen(appDisplay);
  Window window = XCreateSimpleWindow(
      appDisplay, DefaultRootWindow(appDisplay), 0, 0, resolutionWidth, resolutionHeight, 0,
      BlackPixel(appDisplay, screen), WhitePixel(appDisplay, screen));
  XMapWindow(appDisplay, window);
  XStoreName(appDisplay, window, RDOC_WINDOW_TITLE);
  XFlush(appDisplay);

  return window;
}

void CreateResources()
{
  appDisplay = CreateDisplay();
  appWindow = CreateWindow();

  main_create();
  main_init();
}

//-----------------------------------------------------------------------------
// ReleaseResources
//-----------------------------------------------------------------------------
void ReleaseResources()
{
  main_release();
}

void Render()
{
  main_prereset();
  main_render();
  main_postreset();
}

//-----------------------------------------------------------------------------
// ParseCommandLine
//-----------------------------------------------------------------------------
static bool ParseCommandLine(int argc, char **argv)
{
  for(int i = 1; i < argc; ++i)
  {
    if(0 == strcmp(argv[i], "-repeat"))
    {
      ++i;
      if(i >= argc)
      {
        return false;
      }
      frameLoops = atoi(argv[i]);
    }
    else if(0 == strcmp(argv[i], "-reset"))
    {
      resourceReset = true;
    }
    else
    {
      // Unknown command
      return false;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
// Usage
//-----------------------------------------------------------------------------
static void Usage()
{
  const char *usage =
      "Options:\n"
      "-repeat N    -- Number of frames to run\n"
      "-reset       -- Perform a state reset in between frames\n";

  fprintf(stderr, usage);
}

int main(int argc, char **argv)
{
  bool quit = false;

  if(!ParseCommandLine(argc, argv))
  {
    Usage();
    return EXIT_FAILURE;
  }

  try
  {
    CreateResources();

    // TODO QueryPerformanceFrequency(&performanceCounterFrequency);

    int repeatIteration = 0;
    while(frameLoops == -1 || repeatIteration < frameLoops)
    {
      // TODO
      // ProcessMessages(quit);
      if(quit)
      {
        break;
      }

      Render();

      repeatIteration = (std::max)(0, repeatIteration + 1);
    }
  }
  catch(std::exception &e)
  {
    fprintf(stderr, "Error: %s", e.what());
  }

  ReleaseResources();
}

#endif
)";

/******************************************************************************/
std::string CodeWriter::commonH = R"(//-----------------------------------------------------------------------------
// Generated with RenderDoc CPP Trace Gen
//
// File: common.h
//
//-----------------------------------------------------------------------------

#pragma once
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "helper/helper.h"

void PostStageProgress(const char *stage, uint32_t i, uint32_t N);
)";

/******************************************************************************/
std::string CodeWriter::helperH = R"(//-----------------------------------------------------------------------------
// Generated with RenderDoc CPP Trace Gen
//
// File: helper.h
//
//-----------------------------------------------------------------------------

#pragma once
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#if defined(_WIN32)
#include <Windows.h>
#endif

#include <algorithm>
#include <vector>

#include "vulkan/vulkan.h"

#define var_to_string(s) #s

struct AuxVkTraceResources
{
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physDevice;
  VkPhysicalDeviceProperties physDeviceProperties;
  VkDebugReportCallbackEXT callback;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkQueue queue;
  VkFence fence;
  VkSemaphore semaphore;
};

struct Region
{
  uint64_t offset = 0;
  uint64_t size = 0;
  Region(){};
  Region(uint64_t o, uint64_t s) : offset(o), size(s) {}
};

struct MemoryRemap
{
  Region capture;
  Region replay;
};

typedef std::vector<MemoryRemap> MemoryRemapVec;

VkPresentModeKHR GetCompatiblePresentMode(VkPresentModeKHR captured,
                                          std::vector<VkPresentModeKHR> present);

int32_t MemoryTypeIndex(VkMemoryPropertyFlags mask, uint32_t bits,
                        VkPhysicalDeviceMemoryProperties memory_props);

uint32_t CompatibleMemoryTypeIndex(uint32_t type, const VkPhysicalDeviceMemoryProperties &captured,
                                   const VkPhysicalDeviceMemoryProperties &present, uint32_t bits);

VkResult CheckMemoryAllocationCompatibility(uint32_t type,
                                            const VkPhysicalDeviceMemoryProperties &captured,
                                            const VkPhysicalDeviceMemoryProperties &present,
                                            const VkMemoryRequirements &requirements);

void ReadBuffer(const char *name, std::vector<uint8_t> &buf);

void InitializeDestinationBuffer(VkDevice device, VkBuffer *dst_buffer, VkDeviceMemory dst_memory,
                                 uint64_t size);
void InitializeSourceBuffer(VkDevice device, VkBuffer *buffer, VkDeviceMemory *memory, size_t size,
                            uint8_t *initial_data, VkPhysicalDeviceMemoryProperties props,
                            MemoryRemapVec &remap);
void InitializeAuxResources(AuxVkTraceResources *aux, VkInstance instance, VkPhysicalDevice physDevice, VkDevice device);

void ImageLayoutTransition(VkCommandBuffer cmdBuffer, VkImage dstImage,
  VkImageSubresourceRange subresourceRange, VkImageLayout newLayout,
  uint32_t dstQueueFamily, VkImageLayout oldLayout, uint32_t srcQueueFamily);

void ImageLayoutTransition(AuxVkTraceResources aux, VkImage dst, VkImageCreateInfo dst_ci,
                           VkImageLayout final_layout,
                           VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED);
void ImageLayoutTransition(AuxVkTraceResources aux, VkImage dst,
                           VkImageSubresourceRange subresourceRange, VkImageLayout final_layout,
                           VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED);

VkImageAspectFlags GetFullAspectFromFormat(VkFormat fmt);
VkImageAspectFlags GetAspectFromFormat(VkFormat fmt);

void CopyResetImage(AuxVkTraceResources aux, VkImage dst, VkBuffer src, VkImageCreateInfo dst_ci);
void CopyResetBuffer(AuxVkTraceResources aux, VkBuffer dst, VkBuffer src, VkDeviceSize size);

void MakePhysicalDeviceFeaturesMatch(const VkPhysicalDeviceFeatures &available,
                                     VkPhysicalDeviceFeatures *captured_request);

void RegisterDebugCallback(AuxVkTraceResources aux, VkInstance instance,
                           VkDebugReportFlagBitsEXT flags);

void MapUpdateAliased(uint8_t *dst, uint8_t *src, const VkMappedMemoryRange &range,
                      VkMemoryAllocateInfo &ai, MemoryRemapVec &remap, VkDevice dev);
void MapUpdate(AuxVkTraceResources aux, uint8_t *dst, uint8_t *src, const VkMappedMemoryRange &range,
               VkMemoryAllocateInfo &ai, MemoryRemapVec &remap, VkDevice dev);

inline uint64_t AlignedSize(uint64_t size, uint64_t alignment)
{
  return ((size / alignment) + ((size % alignment) > 0 ? 1 : 0)) * alignment;
}

inline uint64_t AlignedDown(uint64_t size, uint64_t alignment) {
  return (uint64_t(size / alignment)) * alignment;
}

std::string StageProgressString(const char *stage, uint32_t i, uint32_t N);
)";

/******************************************************************************/
std::string CodeWriter::helperCppP1 = R"(//-----------------------------------------------------------------------------
// Generated with RenderDoc CPP Trace Gen
//
// File: helper.cpp
//
//-----------------------------------------------------------------------------

#include "helper.h"
#include <string>

VkBool32 VKAPI_PTR DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                 uint64_t object, size_t location, int32_t messageCode,
                                 const char *pLayerPrefix, const char *pMessage, void *pUserData)
{
  switch(flags)
  {
    case VK_DEBUG_REPORT_ERROR_BIT_EXT:
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT: fprintf(stderr, "%s\n", pMessage);
#if defined(_WIN32)
      OutputDebugStringA(pMessage);
      OutputDebugStringA("\n");
#endif
  }

  return VK_FALSE;
}

void RegisterDebugCallback(AuxVkTraceResources aux, VkInstance instance,
                           VkDebugReportFlagBitsEXT flags)
{
  PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
  CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugReportCallbackEXT");

  VkDebugReportCallbackCreateInfoEXT ci = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                                           0, flags, DebugCallback, NULL};
  if(CreateDebugReportCallback != NULL)
  {
    VkResult result = CreateDebugReportCallback(instance, &ci, NULL, &aux.callback);
    assert(result == VK_SUCCESS);
  }
}

VkPresentModeKHR GetCompatiblePresentMode(VkPresentModeKHR captured,
                                          std::vector<VkPresentModeKHR> present)
{
  for(uint32_t i = 0; i < present.size(); i++)
    if(present[i] == captured)
      return captured;

  assert(present.size() > 0);
  return present[0];
}

uint32_t FixCompressedSizes(VkFormat fmt, VkExtent3D &dim, uint32_t &offset)
{
  switch(fmt)
  {
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      dim.width = (uint32_t)AlignedSize(dim.width, 4);
      dim.height = (uint32_t)AlignedSize(dim.height, 4);
      dim.depth = (uint32_t)AlignedSize(dim.depth, 1);
      offset = (uint32_t)AlignedSize(offset, 4);
      return 1;
  }

  offset = (uint32_t)AlignedSize(offset, 4);
  return 0;
}

double SizeOfFormat(VkFormat fmt)
{
  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB: return 1.0;

    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:

    case VK_FORMAT_D16_UNORM: return 2.0;

    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB: return 3.0;

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return 4.0;

    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT: return 8.0;

    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT: return 16.0;

    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK: return 1.0;

    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK: return 0.5;

    default: assert(0);
  }
  return 0.0;
}

int MinDimensionSize(VkFormat format)
{
  switch(format)
  {
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK: return 4;

    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return 4;

    default: return 1;
  }
}

VkImageAspectFlags GetFullAspectFromFormat(VkFormat fmt)
{
  if(fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D32_SFLOAT)
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  else if(fmt == VK_FORMAT_D16_UNORM_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
          fmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  else
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageAspectFlags GetAspectFromFormat(VkFormat fmt)
{
  if(fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D16_UNORM_S8_UINT ||
     fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  else
    return VK_IMAGE_ASPECT_COLOR_BIT;
}


void ImageLayoutTransition(VkCommandBuffer cmdBuffer, VkImage dstImage,
  VkImageSubresourceRange subresourceRange, VkImageLayout newLayout,
  uint32_t dstQueueFamily, VkImageLayout oldLayout, uint32_t srcQueueFamily) {
  uint32_t all_access =
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT |
    VK_ACCESS_HOST_WRITE_BIT;

  VkImageMemoryBarrier imgBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    NULL, all_access, VK_ACCESS_TRANSFER_WRITE_BIT,
    oldLayout, newLayout,
    srcQueueFamily, dstQueueFamily,
    dstImage, subresourceRange};

  vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imgBarrier);
}

void ImageLayoutTransition(AuxVkTraceResources aux, VkImage dstImage,
                           VkImageSubresourceRange subresourceRange,
                           VkImageLayout newLayout, VkImageLayout oldLayout)
{
  ImageLayoutTransition(aux.command_buffer, dstImage, subresourceRange,
    newLayout, VK_QUEUE_FAMILY_IGNORED, oldLayout, VK_QUEUE_FAMILY_IGNORED);
}

void ImageLayoutTransition(AuxVkTraceResources aux, VkImage dstImage, VkImageCreateInfo dstCI,
                           VkImageLayout newLayout, VkImageLayout oldLayout)
{
  VkImageSubresourceRange subresourceRange = {GetFullAspectFromFormat(dstCI.format), 0,
                                              VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

  ImageLayoutTransition(aux, dstImage, subresourceRange,
    newLayout, oldLayout);
}

void CopyResetImage(AuxVkTraceResources aux, VkImage dst, VkBuffer src, VkImageCreateInfo dst_ci)
{
  ImageLayoutTransition(aux, dst, dst_ci, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  if(dst_ci.samples == VK_SAMPLE_COUNT_1_BIT)
  {
    std::vector<VkBufferImageCopy> regions;
    uint32_t offset = 0;
    for(uint32_t a = 0; a < dst_ci.arrayLayers; a++)
    {
      VkExtent3D dim = dst_ci.extent;
      uint32_t x = 0;
      FixCompressedSizes(dst_ci.format, dim, x);
      for(uint32_t i = 0; i < dst_ci.mipLevels; i++)
      {
        VkBufferImageCopy region = {offset,     dim.width,
                                    dim.height, {GetAspectFromFormat(dst_ci.format), i, a, 1},
                                    {0, 0, 0},  dim};
        offset += (uint32_t)(dim.depth * dim.width * dim.height * SizeOfFormat(dst_ci.format));
        dim.height = std::max<int>(dim.height / 2, 1);
        dim.width = std::max<int>(dim.width / 2, 1);
        dim.depth = std::max<int>(dim.depth / 2, 1);
        FixCompressedSizes(dst_ci.format, dim, offset);
        regions.push_back(region);
      }
    }
    const uint32_t kMaxUpdate = 100;
    for(uint32_t i = 0; i * kMaxUpdate < regions.size(); i++)
    {
      uint32_t count = std::min<uint32_t>(kMaxUpdate, (uint32_t)regions.size() - i * kMaxUpdate);
      uint32_t offset = i * kMaxUpdate;
      vkCmdCopyBufferToImage(aux.command_buffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             count, regions.data() + offset);
    }
  }
}
void CopyResetBuffer(AuxVkTraceResources aux, VkBuffer dst, VkBuffer src, VkDeviceSize size)
{
  if(size == 0)
    return;
  VkBufferCopy region = {0, 0, size};
  vkCmdCopyBuffer(aux.command_buffer, src, dst, 1, &region);
}

void InitializeDestinationBuffer(VkDevice device, VkBuffer *dst_buffer, VkDeviceMemory dst_memory,
                                 uint64_t size)
{
  assert(dst_buffer != NULL);
  if(size == 0)
    return;

  VkBufferCreateInfo buffer_dst_ci = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      NULL};

  VkResult result = vkCreateBuffer(device, &buffer_dst_ci, NULL, dst_buffer);
  assert(result == VK_SUCCESS);

  result = vkBindBufferMemory(device, *dst_buffer, dst_memory, 0);
  assert(result == VK_SUCCESS);
}
void InitializeSourceBuffer(VkDevice device, VkBuffer *src_buffer, VkDeviceMemory *src_memory,
                            size_t size, uint8_t *initial_data,
                            VkPhysicalDeviceMemoryProperties props, MemoryRemapVec &remap)
{
  assert(src_buffer != NULL && src_memory != NULL);
  if(size == 0)
    return;

  VkBufferCreateInfo buffer_src_ci = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      NULL};

  VkResult result = vkCreateBuffer(device, &buffer_src_ci, NULL, src_buffer);
  assert(result == VK_SUCCESS);

  VkMemoryRequirements buffer_requirements;
  vkGetBufferMemoryRequirements(device, *src_buffer, &buffer_requirements);

  VkFlags gpu_and_cpu_visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  uint32_t memory_type =
      MemoryTypeIndex(gpu_and_cpu_visible, buffer_requirements.memoryTypeBits, props);

  VkMemoryAllocateInfo memory_ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
                                    buffer_requirements.size, memory_type};

  result = vkAllocateMemory(device, &memory_ai, NULL, src_memory);
  assert(result == VK_SUCCESS);

  result = vkBindBufferMemory(device, *src_buffer, *src_memory, 0);
  assert(result == VK_SUCCESS);

  uint8_t *data = NULL;
  result = vkMapMemory(device, *src_memory, 0, size, 0, (void **)&data);
  assert(result == VK_SUCCESS);

  // For each resource bound in the memory allocation, copy the correct
  // memory segment into 'src' buffer.
  if(remap.size() > 0)
  {
    for(uint32_t i = 0; i < remap.size(); i++)
    {
      MemoryRemap mr = remap[i];
      if(mr.replay.offset + mr.replay.size <= size)
      {
        memcpy(data + mr.replay.offset, initial_data + mr.capture.offset,
               std::min<uint64_t>(mr.capture.size, mr.replay.size));
      }
    }
  }
  else
  {
    memcpy(data, initial_data, size);
  }

  VkMappedMemoryRange memory_range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, *src_memory, 0,
                                      size};

  result = vkFlushMappedMemoryRanges(device, 1, &memory_range);
  assert(result == VK_SUCCESS);

  vkUnmapMemory(device, *src_memory);
}

)";

/******************************************************************************/
std::string CodeWriter::helperCppP2 = R"(
void InitializeAuxResources(AuxVkTraceResources *aux, VkInstance instance, VkPhysicalDevice physDevice, VkDevice device)
{
  aux->instance = instance;
  aux->physDevice = physDevice;
  aux->device = device;
  vkGetPhysicalDeviceProperties(aux->physDevice, &aux->physDeviceProperties);

  vkGetDeviceQueue(device, 0, 0, &aux->queue);

  VkCommandPoolCreateInfo cmd_pool_ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
                                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0};

  VkResult result = vkCreateCommandPool(device, &cmd_pool_ci, NULL, &aux->command_pool);
  assert(result == VK_SUCCESS);

  VkCommandBufferAllocateInfo cmd_buffer_ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
                                               aux->command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

  result = vkAllocateCommandBuffers(device, &cmd_buffer_ai, &aux->command_buffer);
  assert(result == VK_SUCCESS);

  VkFenceCreateInfo fence_ci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0};

  result = vkCreateFence(device, &fence_ci, NULL, &aux->fence);
  assert(result == VK_SUCCESS);

  VkSemaphoreCreateInfo semaphore_ci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0};
  result = vkCreateSemaphore(device, &semaphore_ci, NULL, &aux->semaphore);
}

int32_t MemoryTypeIndex(VkMemoryPropertyFlags mask, uint32_t bits,
                        VkPhysicalDeviceMemoryProperties memory_props)
{
  for(uint32_t i = 0; i < memory_props.memoryTypeCount; ++i)
  {
    if((bits & 1) == 1)
    {
      // Type is available, does it match user properties?
      if((memory_props.memoryTypes[i].propertyFlags & mask) == mask)
      {
        return i;
      }
    }
    bits = bits >> 1;
  }
  return -1;
}

uint32_t CompatibleMemoryTypeIndex(uint32_t type, const VkPhysicalDeviceMemoryProperties &captured,
                                   const VkPhysicalDeviceMemoryProperties &present, uint32_t bits)
{
  // When the application was captured this is the property flag that was
  // picked as compatible. Try to find the closest match.
  // This if fairly conservative and here is an example where this might fail:
  // Let System A, where the trace is captured, has all the memory marked as
  // DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT (for example UMA devices).
  // The application requests a memory allocation with just a
  // HOST_VISIBLE_BIT but gets a memory type index that points to
  // HOST_VISIBLE_BIT | DEVICE_LOCAL_BIT. On System B, memory is split into:
  // 1. HOST_VISIBLE 2. DEVICE_LOCAL and 3. HOST_VISIBLE | DEVICE_LOCAL.
  // Since the captured memory type was HOST_VISIBLE | DEVICE_LOCAL on replay
  // the 3rd memory segment will get selected.
  VkMemoryType mem_type = captured.memoryTypes[type];
  VkMemoryPropertyFlags propertyFlag = mem_type.propertyFlags;

  // All memory types are approved with 0xFFFFFFFF bits
  return MemoryTypeIndex(propertyFlag, bits, present);
}

VkResult CheckMemoryAllocationCompatibility(uint32_t type,
                                            const VkPhysicalDeviceMemoryProperties &captured,
                                            const VkPhysicalDeviceMemoryProperties &present,
                                            const VkMemoryRequirements &requirements)
{
  VkMemoryType mem_type = captured.memoryTypes[type];
  VkMemoryPropertyFlags propertyFlag = mem_type.propertyFlags;

  uint32_t compat_type =
      CompatibleMemoryTypeIndex(type, captured, present, requirements.memoryTypeBits);

  uint32_t current = MemoryTypeIndex(propertyFlag, requirements.memoryTypeBits, present);

  return (compat_type == current ? VK_SUCCESS : VK_ERROR_VALIDATION_FAILED_EXT);
}

void ReadBuffer(const char *name, std::vector<uint8_t> &buf)
{
  FILE *f = fopen(name, "rb");
  if(f == NULL)
  {
    return;
  }

  fseek(f, 0, SEEK_END);
  uint64_t length = ftell(f);
  buf.resize(length);
  rewind(f);

  uint64_t result = fread(buf.data(), 1, length, f);
  fclose(f);
  assert(result <= length);
}

#define ReportMismatchedFeature(x, y)                                                             \
  if(x > y)                                                                                       \
  {                                                                                               \
    fprintf(stdout, "%s (%d) doesn't match %s (%d)\n", var_to_string(x), x, var_to_string(y), y); \
    x = y;                                                                                        \
  }

void MakePhysicalDeviceFeaturesMatch(const VkPhysicalDeviceFeatures &available,
                                     VkPhysicalDeviceFeatures *captured_request)
{
  ReportMismatchedFeature(captured_request->robustBufferAccess, available.robustBufferAccess);
  ReportMismatchedFeature(captured_request->fullDrawIndexUint32, available.fullDrawIndexUint32);
  ReportMismatchedFeature(captured_request->imageCubeArray, available.imageCubeArray);
  ReportMismatchedFeature(captured_request->independentBlend, available.independentBlend);
  ReportMismatchedFeature(captured_request->geometryShader, available.geometryShader);
  ReportMismatchedFeature(captured_request->tessellationShader, available.tessellationShader);
  ReportMismatchedFeature(captured_request->sampleRateShading, available.sampleRateShading);
  ReportMismatchedFeature(captured_request->dualSrcBlend, available.dualSrcBlend);
  ReportMismatchedFeature(captured_request->logicOp, available.logicOp);
  ReportMismatchedFeature(captured_request->multiDrawIndirect, available.multiDrawIndirect);
  ReportMismatchedFeature(captured_request->drawIndirectFirstInstance,
                          available.drawIndirectFirstInstance);
  ReportMismatchedFeature(captured_request->depthClamp, available.depthClamp);
  ReportMismatchedFeature(captured_request->depthBiasClamp, available.depthBiasClamp);
  ReportMismatchedFeature(captured_request->fillModeNonSolid, available.fillModeNonSolid);
  ReportMismatchedFeature(captured_request->depthBounds, available.depthBounds);
  ReportMismatchedFeature(captured_request->wideLines, available.wideLines);
  ReportMismatchedFeature(captured_request->largePoints, available.largePoints);
  ReportMismatchedFeature(captured_request->alphaToOne, available.alphaToOne);
  ReportMismatchedFeature(captured_request->multiViewport, available.multiViewport);
  ReportMismatchedFeature(captured_request->samplerAnisotropy, available.samplerAnisotropy);
  ReportMismatchedFeature(captured_request->textureCompressionETC2, available.textureCompressionETC2);
  ReportMismatchedFeature(captured_request->textureCompressionASTC_LDR,
                          available.textureCompressionASTC_LDR);
  ReportMismatchedFeature(captured_request->textureCompressionBC, available.textureCompressionBC);
  ReportMismatchedFeature(captured_request->occlusionQueryPrecise, available.occlusionQueryPrecise);
  ReportMismatchedFeature(captured_request->pipelineStatisticsQuery,
                          available.pipelineStatisticsQuery);
  ReportMismatchedFeature(captured_request->vertexPipelineStoresAndAtomics,
                          available.vertexPipelineStoresAndAtomics);
  ReportMismatchedFeature(captured_request->fragmentStoresAndAtomics,
                          available.fragmentStoresAndAtomics);
  ReportMismatchedFeature(captured_request->shaderTessellationAndGeometryPointSize,
                          available.shaderTessellationAndGeometryPointSize);
  ReportMismatchedFeature(captured_request->shaderImageGatherExtended,
                          available.shaderImageGatherExtended);
  ReportMismatchedFeature(captured_request->shaderStorageImageExtendedFormats,
                          available.shaderStorageImageExtendedFormats);
  ReportMismatchedFeature(captured_request->shaderStorageImageMultisample,
                          available.shaderStorageImageMultisample);
  ReportMismatchedFeature(captured_request->shaderStorageImageReadWithoutFormat,
                          available.shaderStorageImageReadWithoutFormat);
  ReportMismatchedFeature(captured_request->shaderStorageImageWriteWithoutFormat,
                          available.shaderStorageImageWriteWithoutFormat);
  ReportMismatchedFeature(captured_request->shaderUniformBufferArrayDynamicIndexing,
                          available.shaderUniformBufferArrayDynamicIndexing);
  ReportMismatchedFeature(captured_request->shaderSampledImageArrayDynamicIndexing,
                          available.shaderSampledImageArrayDynamicIndexing);
  ReportMismatchedFeature(captured_request->shaderStorageBufferArrayDynamicIndexing,
                          available.shaderStorageBufferArrayDynamicIndexing);
  ReportMismatchedFeature(captured_request->shaderStorageImageArrayDynamicIndexing,
                          available.shaderStorageImageArrayDynamicIndexing);
  ReportMismatchedFeature(captured_request->shaderClipDistance, available.shaderClipDistance);
  ReportMismatchedFeature(captured_request->shaderCullDistance, available.shaderCullDistance);
  ReportMismatchedFeature(captured_request->shaderFloat64, available.shaderFloat64);
  ReportMismatchedFeature(captured_request->shaderInt64, available.shaderInt64);
  ReportMismatchedFeature(captured_request->shaderInt16, available.shaderInt16);
  ReportMismatchedFeature(captured_request->shaderResourceResidency,
                          available.shaderResourceResidency);
  ReportMismatchedFeature(captured_request->shaderResourceMinLod, available.shaderResourceMinLod);
  ReportMismatchedFeature(captured_request->sparseBinding, available.sparseBinding);
  ReportMismatchedFeature(captured_request->sparseResidencyBuffer, available.sparseResidencyBuffer);
  ReportMismatchedFeature(captured_request->sparseResidencyImage2D, available.sparseResidencyImage2D);
  ReportMismatchedFeature(captured_request->sparseResidencyImage3D, available.sparseResidencyImage3D);
  ReportMismatchedFeature(captured_request->sparseResidency2Samples,
                          available.sparseResidency2Samples);
  ReportMismatchedFeature(captured_request->sparseResidency4Samples,
                          available.sparseResidency4Samples);
  ReportMismatchedFeature(captured_request->sparseResidency8Samples,
                          available.sparseResidency8Samples);
  ReportMismatchedFeature(captured_request->sparseResidency16Samples,
                          available.sparseResidency16Samples);
  ReportMismatchedFeature(captured_request->sparseResidencyAliased, available.sparseResidencyAliased);
  ReportMismatchedFeature(captured_request->variableMultisampleRate,
                          available.variableMultisampleRate);
  ReportMismatchedFeature(captured_request->inheritedQueries, available.inheritedQueries);
}

bool RegionsOverlap(const Region &r1, const Region &r2)
{
  // interval '1' and '2' start and end points:
  uint64_t i1_start = r1.offset;
  uint64_t i1_end = r1.offset + r1.size;
  uint64_t i2_start = r2.offset;
  uint64_t i2_end = r2.offset + r2.size;

  // two intervals i1 [s, e] and i2 [s, e] intersect
  // if X = max(i1.s, i2.s) < Y = min(i1.e, i2.e).
  return std::max<uint64_t>(i1_start, i2_start) < std::min<uint64_t>(i1_end, i2_end);
}

// RegionsIntersect(A, B) == RegionsIntersect(B, A)
Region RegionsIntersect(const Region &r1, const Region &r2)
{
  Region r;

  // two intervals i1 [s, e] and i2 [s, e] intersect
  // if X = max(i1.s, i2.s) < Y = min(i1.e, i2.e).
  r.offset = std::max<uint64_t>(r1.offset, r2.offset);
  r.size = std::min<uint64_t>(r1.offset + r1.size, r2.offset + r2.size) - r.offset;
  return r;
}

void MapUpdate(AuxVkTraceResources aux, uint8_t *dst, uint8_t *src, const VkMappedMemoryRange &range,
               VkMemoryAllocateInfo &ai, MemoryRemapVec &remap, VkDevice dev)
{
  if(dst != NULL)
  {
    std::vector<VkMappedMemoryRange> ranges;
    Region memory_region = {range.offset, range.size};
    assert(range.size != VK_WHOLE_SIZE);

    if(remap.size() > 0)
    {
      for(uint32_t i = 0; i < remap.size(); i++)
      {
        MemoryRemap mr = remap[i];
        Region captured_resource_region(mr.capture.offset, mr.capture.size);
        // If this memory range doesn't overlap with any captured resource continue
        if(!RegionsOverlap(memory_region, captured_resource_region))
          continue;

        // Find the inteval where these two regions overlap. It is guaranteed to be non-null.
        Region intersect = RegionsIntersect(memory_region, captured_resource_region);

        uint64_t skipped_resource_bytes = intersect.offset - mr.capture.offset;
        uint64_t skipped_memory_bytes = intersect.offset - memory_region.offset;
        intersect.size = std::min<uint64_t>(intersect.size, mr.replay.size);

        memcpy(dst + mr.replay.offset + skipped_resource_bytes, src + skipped_memory_bytes,
               intersect.size);

        VkMappedMemoryRange r = range;
        r.offset = mr.replay.offset + skipped_resource_bytes;
        r.size = AlignedSize(r.offset + intersect.size, aux.physDeviceProperties.limits.nonCoherentAtomSize);
        r.offset = AlignedDown(r.offset, aux.physDeviceProperties.limits.nonCoherentAtomSize);
        r.size = r.size - r.offset;
        if (r.offset + r.size > range.offset + range.size ||
            r.offset + r.size > ai.allocationSize) {
          r.size = VK_WHOLE_SIZE;
        }
        ranges.push_back(r);
      }
    }
    else
    {
      VkMappedMemoryRange r = range;
      r.size = std::min<uint64_t>(ai.allocationSize, range.size);
      memcpy(dst + r.offset, src, r.size);
      ranges.push_back(r);
    }

    VkResult result = vkFlushMappedMemoryRanges(dev, (uint32_t)ranges.size(), ranges.data());
    assert(result == VK_SUCCESS);
  }
}

std::string StageProgressString(const char *stage, uint32_t i, uint32_t N) {
  return std::string("RenderDoc Frame Loop: " + std::string(stage) + " part " + std::to_string(i) + " of " + std::to_string(N));
}
)";

/******************************************************************************/
std::string CodeWriter::genScriptWin = R"(
rd /s /q Win_VS2015x64
mkdir Win_VS2015x64
rd /s /q build_x64
mkdir build_x64\Debug
mkdir build_x64\Release
cmake.exe -Wno-dev -G "Visual Studio 14 2015 Win64" --build "" -H. -BWin_VS2015x64
pause
)";

/******************************************************************************/
std::string CodeWriter::genScriptLinux = R"(
which cmake && which ninja && export CC=clang && export CXX=clang++ && rm -rf linux_build && mkdir linux_build && cd linux_build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && ninja && echo "Build complete"
)";

/******************************************************************************/
std::string CodeWriter::genScriptWinNinja = R"(
rd / s / q Win_Ninja
mkdir Win_Ninja
rd / s / q build_x64
mkdir build_x64\Debug
mkdir build_x64\Release
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
cmake.exe -Wno-dev -G Ninja --build "" -H. -BWin_Ninja -DCMAKE_BUILD_TYPE=Release
cd Win_Ninja
ninja
cd .. /
pause
)";

} // namespace vk_cpp_codec