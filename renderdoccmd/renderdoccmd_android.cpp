/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "renderdoccmd.h"
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include <android_native_app_glue.h>
#define ANativeActivity_onCreate __attribute__((visibility("default"))) ANativeActivity_onCreate
extern "C" {
#include <android_native_app_glue.c>
}

#include <android/log.h>
#define LOGCAT_TAG "renderdoc"

using std::string;
using std::vector;
using std::istringstream;

struct android_app *android_state;

void Daemonise()
{
}

void DisplayRendererPreview(IReplayController *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height)
{
  ANativeWindow *connectionScreenWindow = android_state->window;

  IReplayOutput *out = renderer->CreateOutput(WindowingSystem::Android, connectionScreenWindow,
                                              ReplayOutputType::Texture);

  out->SetTextureDisplay(displayCfg);

  for(int i = 0; i < 100; i++)
  {
    renderer->SetFrameEvent(10000000, true);

    __android_log_print(ANDROID_LOG_INFO, LOGCAT_TAG, "Frame %i", i);
    out->Display();

    usleep(100000);
  }
}

// Returns the renderdoccmd arguments passed via am start
// Examples: am start ... -e renderdoccmd "remoteserver"
// -e renderdoccmd "replay /sdcard/capture.rdc"
vector<string> getRenderdoccmdArgs()
{
  JNIEnv *env;
  android_state->activity->vm->AttachCurrentThread(&env, 0);

  jobject me = android_state->activity->clazz;

  jclass acl = env->GetObjectClass(me);    // class pointer of NativeActivity
  jmethodID giid = env->GetMethodID(acl, "getIntent", "()Landroid/content/Intent;");
  jobject intent = env->CallObjectMethod(me, giid);    // Got our intent

  jclass icl = env->GetObjectClass(intent);    // class pointer of Intent
  jmethodID gseid =
      env->GetMethodID(icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

  jstring jsParam1 =
      (jstring)env->CallObjectMethod(intent, gseid, env->NewStringUTF("renderdoccmd"));

  vector<string> ret;
  if(!jsParam1)
    return ret;    // No arg value found

  ret.push_back("renderdoccmd");

  const char *param1 = env->GetStringUTFChars(jsParam1, 0);
  istringstream iss(param1);
  while(iss)
  {
    string sub;
    iss >> sub;
    ret.push_back(sub);
  }
  android_state->activity->vm->DetachCurrentThread();

  return ret;
}

void handle_cmd(android_app *app, int32_t cmd)
{
  switch(cmd)
  {
    case APP_CMD_INIT_WINDOW:
    {
      vector<string> args = getRenderdoccmdArgs();
      if(!args.size())
        break;    // Nothing for APK to do.
      renderdoccmd(GlobalEnvironment(), args);
      // activity is done and should be closed
      ANativeActivity_finish(android_state->activity);
      break;
    }
  }
}

void android_main(struct android_app *state)
{
  android_state = state;
  android_state->onAppCmd = handle_cmd;

  __android_log_print(ANDROID_LOG_INFO, LOGCAT_TAG, "android_main android_state->window: %p",
                      android_state->window);

  // Used to poll the events in the main loop
  int events;
  android_poll_source *source;
  do
  {
    if(ALooper_pollAll(1, nullptr, &events, (void **)&source) >= 0)
    {
      if(source != NULL)
        source->process(android_state, source);
    }
  } while(android_state->destroyRequested == 0);
}
