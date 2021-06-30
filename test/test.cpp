// Test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define RENDERDOC_PLATFORM_WIN32
#define RENDERDOC_EXPORTS
#include <iostream>
#include <string>
#include "../renderdoc/android/android_utils.h"

extern "C"  bool ExChangePackageName(Android::ABI abi, const std::string &dir,
                                             const std::string &org_package,
                         const std::string &new_package);

int main()
{
  std::cout << "Hello World!\n";
  ExChangePackageName(Android::ABI::arm64_v8a, "D:\\RenderDoc\\android\\",
                      "org.renderdoc.renderdoccmd.arm64", "com.android.vading");
  return 0;
}

