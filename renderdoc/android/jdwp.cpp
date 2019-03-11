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

#include "jdwp.h"
#include <functional>
#include "core/core.h"
#include "strings/string_utils.h"
#include "android.h"
#include "android_utils.h"

namespace JDWP
{
void InjectVulkanLayerSearchPath(Connection &conn, threadID thread, int32_t slotIdx,
                                 const std::string &libPath)
{
  referenceTypeID stringClass = conn.GetType("Ljava/lang/String;");
  methodID stringConcat = conn.GetMethod(stringClass, "concat");

  if(conn.IsErrored())
    return;

  if(!stringClass || !stringConcat)
  {
    RDCERR("Couldn't find java.lang.String (%llu) or java.lang.String.concat() (%llu)", stringClass,
           stringConcat);
    return;
  }

  // get the callstack frames
  std::vector<StackFrame> stack = conn.GetCallStack(thread);

  if(stack.empty())
  {
    RDCERR("Couldn't get callstack!");
    return;
  }

  // get the local in the top (current) frame
  value librarySearchPath = conn.GetLocalValue(thread, stack[0].id, slotIdx, Tag::Object);

  if(librarySearchPath.tag != Tag::String || librarySearchPath.String == 0)
  {
    RDCERR("Couldn't get 'String librarySearchPath' local parameter!");
    return;
  }

  RDCDEBUG("librarySearchPath is %s", conn.GetString(librarySearchPath.String).c_str());

  value appendSearch = conn.NewString(thread, ":" + libPath);

  // temp = librarySearchPath.concat(appendSearch);
  value temp = conn.InvokeInstance(thread, stringClass, stringConcat, librarySearchPath.String,
                                   {appendSearch});

  if(temp.tag != Tag::String || temp.String == 0)
  {
    RDCERR("Failed to concat search path!");
    return;
  }

  RDCDEBUG("librarySearchPath is now %s", conn.GetString(temp.String).c_str());

  // we will have resume the thread above to call concat, invalidating our frames.
  // Re-fetch the callstack
  stack = conn.GetCallStack(thread);

  if(stack.empty())
  {
    RDCERR("Couldn't get callstack!");
    return;
  }

  // replace the search path with our modified one
  // librarySearchPath = temp;
  conn.SetLocalValue(thread, stack[0].id, slotIdx, temp);
}

bool InjectLibraries(const std::string &deviceID, Network::Socket *sock)
{
  Connection conn(sock);

  // check that the handshake completed successfully
  if(conn.IsErrored())
    return false;

  // immediately re-suspend, as connecting will have woken it up
  conn.Suspend();

  conn.SetupIDSizes();

  if(conn.IsErrored())
    return false;

  // default to arm as a safe bet
  Android::ABI abi = Android::ABI::armeabi_v7a;

  // determine the CPU ABI from android.os.Build.CPU_ABI
  referenceTypeID buildClass = conn.GetType("Landroid/os/Build;");
  if(buildClass)
  {
    fieldID CPU_ABI = conn.GetField(buildClass, "CPU_ABI");

    if(CPU_ABI)
    {
      value val = conn.GetFieldValue(buildClass, CPU_ABI);

      if(val.tag == Tag::String)
        abi = Android::GetABI(conn.GetString(val.String));
      else
        RDCERR("CPU_ABI value was type %u, not string!", (uint32_t)val.tag);
    }
    else
    {
      RDCERR("Couldn't find CPU_ABI field in android.os.Build");
    }
  }
  else
  {
    RDCERR("Couldn't find android.os.Build");
  }

  if(abi == Android::ABI::unknown)
  {
    RDCERR("Unrecognised running ABI, falling back to armeabi-v7a");
    abi = Android::ABI::armeabi_v7a;
  }

  std::string libPath = Android::GetPathForPackage(deviceID, Android::GetRenderDocPackageForABI(abi));

  switch(abi)
  {
    case Android::ABI::unknown:
    case Android::ABI::armeabi_v7a: libPath += "lib/arm"; break;
    case Android::ABI::arm64_v8a: libPath += "lib/arm64"; break;
    case Android::ABI::x86_64: libPath += "lib/x86_64"; break;
    case Android::ABI::x86: libPath += "lib/x86"; break;
  }

  RDCLOG("Injecting RenderDoc from library in %s", libPath.c_str());

  if(conn.IsErrored())
    return false;

  // try to find the vulkan loader class and patch the search path when getClassLoader is called.
  // This is an optional step as some devices may not support vulkan and may not have this class, so
  // in that case we just skip it.
  referenceTypeID vulkanLoaderClass = conn.GetType("Landroid/app/ApplicationLoaders;");

  if(vulkanLoaderClass)
  {
    // See:
    // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/app/ApplicationLoaders.java
    // for the public getClassLoader.

    // look for both signatures in this order, as it goes from most recent to least recent. In some
    // cases (e.g. with List<ClassLoader> sharedLibraries) the older function is still around as an
    // overload that forwards on - so may not be called. This would cause us to wait for a function
    // to be hit that was never hit.

    const char *getClassLoaderSignatures[] = {
        // ClassLoader getClassLoader(String zip, int targetSdkVersion, boolean isBundled,
        //                            String librarySearchPath, String libraryPermittedPath,
        //                            ClassLoader parent, String cacheKey,
        //                            String classLoaderName, List<ClassLoader> sharedLibraries);
        "(Ljava/lang/String;IZLjava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;"
        "Ljava/lang/String;Ljava/lang/String;Ljava/util/List;)Ljava/lang/ClassLoader;",

        // ClassLoader getClassLoader(String zip, int targetSdkVersion, boolean isBundled,
        //                            String librarySearchPath, String libraryPermittedPath,
        //                            ClassLoader parent, String classLoaderName);
        "(Ljava/lang/String;IZLjava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;"
        "Ljava/lang/String;)Ljava/lang/ClassLoader;",

        // ClassLoader getClassLoader(String zip, int targetSdkVersion, boolean isBundled,
        //                            String librarySearchPath, String libraryPermittedPath,
        //                            ClassLoader parent);
        "(Ljava/lang/String;IZLjava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)"
        "Ljava/lang/ClassLoader;",
    };

    methodID vulkanLoaderMethod = 0;
    for(const char *sig : getClassLoaderSignatures)
    {
      vulkanLoaderMethod = conn.GetMethod(vulkanLoaderClass, "getClassLoader", sig);

      if(vulkanLoaderMethod)
      {
        RDCLOG("Got android.app.ApplicationLoaders.getClassLoader signature %s", sig);
        break;
      }
    }

    if(vulkanLoaderMethod)
    {
      int32_t slotIdx =
          conn.GetLocalVariable(vulkanLoaderClass, vulkanLoaderMethod, "librarySearchPath");

      // as a default, use the 4th slot as it's the 4th argument argument (0 is this), if symbols
      // weren't available we can't identify the variable by name
      if(slotIdx == -1)
        slotIdx = 4;

      // wait for the method to get hit - WaitForEvent will resume, watch events, and return
      // (re-suspended) when the first event occurs that matches the filter function
      Event evData =
          conn.WaitForEvent(EventKind::MethodEntry, {{ModifierKind::ClassOnly, vulkanLoaderClass}},
                            [vulkanLoaderMethod](const Event &evData) {
                              return evData.MethodEntry.location.meth == vulkanLoaderMethod;
                            });

      // if we successfully hit the event, try to inject
      if(evData.eventKind == EventKind::MethodEntry)
        InjectVulkanLayerSearchPath(conn, evData.MethodEntry.thread, slotIdx, libPath);
    }
    else
    {
      // we expect if we can get the class, we should find the method.
      RDCERR("Couldn't find getClassLoader method in android.app.ApplicationLoaders");
    }
  }
  else
  {
    // warning only - it's not a problem if we're capturing GLES
    RDCWARN("Couldn't find class android.app.ApplicationLoaders. Vulkan won't be hooked.");
  }

  // we get here whether we processed vulkan or not. Now we need to wait for the application to hit
  // onCreate() and load our library

  referenceTypeID androidApp = conn.GetType("Landroid/app/Application;");

  if(androidApp == 0)
  {
    RDCERR("Couldn't find android.app.Application");
    return false;
  }

  methodID appConstruct = conn.GetMethod(androidApp, "<init>", "()V");

  if(appConstruct == 0)
  {
    RDCERR("Couldn't find android.app.Application constructor");
    return false;
  }

  threadID thread;

  // wait until we hit the constructor of android.app.Application
  {
    Event evData = conn.WaitForEvent(EventKind::MethodEntry, {{ModifierKind::ClassOnly, androidApp}},
                                     [appConstruct](const Event &evData) {
                                       return evData.MethodEntry.location.meth == appConstruct;
                                     });

    if(evData.eventKind == EventKind::MethodEntry)
      thread = evData.MethodEntry.thread;
  }

  if(thread == 0)
  {
    RDCERR("Didn't hit android.app.Application constructor");
    return false;
  }

  // get the callstack frames
  std::vector<StackFrame> stack = conn.GetCallStack(thread);

  if(stack.empty())
  {
    RDCERR("Couldn't get callstack!");
    return false;
  }

  // get this on the top frame
  objectID thisPtr = conn.GetThis(thread, stack[0].id);

  if(thisPtr == 0)
  {
    RDCERR("Couldn't find this");
    return false;
  }

  // get the type for the this object
  referenceTypeID thisType = conn.GetType(thisPtr);

  if(thisType == 0)
  {
    RDCERR("Couldn't find this's class");
    return false;
  }

  // call getClass, this will give us the information for the most derived class
  methodID getClass = conn.GetMethod(thisType, "getClass", "()Ljava/lang/Class;");

  if(getClass == 0)
  {
    RDCERR("Couldn't find this.getClass()");
    return false;
  }

  value thisClass = conn.InvokeInstance(thread, thisType, getClass, thisPtr, {});

  if(thisClass.tag != Tag::ClassObject || thisClass.Object == 0)
  {
    RDCERR("Failed to call this.getClass()!");
    return false;
  }

  // look up onCreate in the most derived class - since we can't guarantee that the base
  // application.app.onCreate() will get called.
  //
  // Note that because we're filtering on both classID and methodID, we need to return back the
  // exact class in the inheritance hierarchy matching the methodID, otherwise we could filter on
  // the derived class but a parent method, and have no hits.
  //
  // This can happen if the most derived class doesn't have an onCreate, and we have to search to a
  // superclass
  referenceTypeID onCreateClass = thisClass.RefType;
  methodID onCreateMethod = conn.GetMethod(thisClass.RefType, "onCreate", "()V", &onCreateClass);

  if(onCreateMethod == 0)
  {
    RDCERR("Couldn't find this.getClass().onCreate()");
    return false;
  }

  // wait until we hit the derived onCreate
  {
    thread = 0;

    Event evData =
        conn.WaitForEvent(EventKind::MethodEntry, {{ModifierKind::ClassOnly, onCreateClass}},
                          [onCreateMethod](const Event &evData) {
                            return evData.MethodEntry.location.meth == onCreateMethod;
                          });

    if(evData.eventKind == EventKind::MethodEntry)
      thread = evData.MethodEntry.thread;
  }

  if(thread == 0)
  {
    RDCERR("Didn't hit android.app.Application.onCreate()");
    return false;
  }

  // find java.lang.Runtime
  referenceTypeID runtime = conn.GetType("Ljava/lang/Runtime;");

  if(runtime == 0)
  {
    RDCERR("Couldn't find java.lang.Runtime");
    return false;
  }

  // find both the static Runtime.getRuntime() as well as the instance Runtime.load()
  methodID getRuntime = conn.GetMethod(runtime, "getRuntime", "()Ljava/lang/Runtime;");
  methodID load = conn.GetMethod(runtime, "load", "(Ljava/lang/String;)V");

  if(getRuntime == 0 || load == 0)
  {
    RDCERR("Couldn't find java.lang.Runtime.getRuntime() %llu or java.lang.Runtime.load() %llu",
           getRuntime, load);
    return false;
  }

  // get the Runtime object via java.lang.Runtime.getRuntime()
  value runtimeObject = conn.InvokeStatic(thread, runtime, getRuntime, {});

  if(runtimeObject.tag != Tag::Object || runtimeObject.Object == 0)
  {
    RDCERR("Failed to call getClass!");
    return false;
  }

  // call Runtime.load() on our library. This will load the library and from then on it's
  // responsible for injecting its hooks into GLES on its own. See android_hook.cpp for more
  // information on the implementation
  value ret = conn.InvokeInstance(thread, runtime, load, runtimeObject.Object,
                                  {conn.NewString(thread, libPath + "/" RENDERDOC_ANDROID_LIBRARY)});

  if(ret.tag != Tag::Void)
  {
    RDCERR("Failed to call load(%s/%s)!", libPath.c_str(), RENDERDOC_ANDROID_LIBRARY);
    return false;
  }

  return true;
}
};    // namespace JDWP

namespace Android
{
bool InjectWithJDWP(const std::string &deviceID, uint16_t jdwpport)
{
  Network::Socket *sock = Network::CreateClientSocket("localhost", jdwpport, 500);

  if(sock)
  {
    bool ret = JDWP::InjectLibraries(deviceID, sock);
    delete sock;

    return ret;
  }
  else
  {
    RDCERR("Couldn't make JDWP connection");
  }

  return false;
}
};    // namespace Android
