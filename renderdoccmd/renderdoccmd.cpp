/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <app/renderdoc_app.h>
#include <replay/renderdoc_replay.h>
#include <string>

using std::string;
using std::wstring;

uint32_t wtoi(wchar_t *str)
{
  uint32_t ret = 0;

  while(str && *str)
  {
    if(*str > L'9' || *str < L'0')
      break;

    uint32_t digit = uint32_t(*str - L'0');

    ret *= 10;
    ret += digit;

    str++;
  }

  return ret;
}

bool argequal(const std::string &a, const std::string &b)
{
  if(a.length() != b.length())
    return false;

  for(size_t i = 0; i < a.length(); i++)
  {
    char ca = a[i];
    char cb = b[i];

    // only compare ASCII characters in UTF-8 string
    if((ca & 0x80) == 0 && (cb & 0x80) == 0 && tolower(ca) != tolower(cb))
      return false;
  }

  return true;
}

void readCapOpts(const std::string &str, CaptureOptions *opts)
{
  if(str.length() < sizeof(CaptureOptions))
    return;

  // serialise from string with two chars per byte
  byte *b = (byte *)opts;
  for(size_t i = 0; i < sizeof(CaptureOptions); i++)
    *(b++) = (byte(str[i * 2 + 0] - 'a') << 4) | byte(str[i * 2 + 1] - 'a');
}

// defined in platform .cpps
void DisplayRendererPreview(ReplayRenderer *renderer, TextureDisplay displayCfg);
wstring GetUsername();

void DisplayRendererPreview(ReplayRenderer *renderer)
{
  if(renderer == NULL)
    return;

  rdctype::array<FetchTexture> texs;
  ReplayRenderer_GetTextures(renderer, &texs);

  TextureDisplay d;
  d.mip = 0;
  d.sampleIdx = ~0U;
  d.overlay = eTexOverlay_None;
  d.typeHint = eCompType_None;
  d.CustomShader = ResourceId();
  d.HDRMul = -1.0f;
  d.linearDisplayAsGamma = true;
  d.FlipY = false;
  d.rangemin = 0.0f;
  d.rangemax = 1.0f;
  d.scale = 1.0f;
  d.offx = 0.0f;
  d.offy = 0.0f;
  d.sliceFace = 0;
  d.rawoutput = false;
  d.lightBackgroundColour = d.darkBackgroundColour = FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
  d.Red = d.Green = d.Blue = true;
  d.Alpha = false;

  for(int32_t i = 0; i < texs.count; i++)
  {
    if(texs[i].creationFlags & eTextureCreate_SwapBuffer)
    {
      d.texid = texs[i].ID;
      break;
    }
  }

  rdctype::array<FetchDrawcall> draws;
  renderer->GetDrawcalls(&draws);

  if(draws.count > 0 && draws[draws.count - 1].flags & eDraw_Present)
  {
    ResourceId id = draws[draws.count - 1].copyDestination;
    if(id != ResourceId())
      d.texid = id;
  }

  DisplayRendererPreview(renderer, d);
}

int renderdoccmd(const std::vector<std::string> &argv)
{
  CaptureOptions opts;

  RENDERDOC_GetDefaultCaptureOptions(&opts);

  opts.AllowFullscreen = false;
  opts.AllowVSync = false;
  opts.DelayForDebugger = 5;
  opts.HookIntoChildren = true;

  if(argv.size() >= 2)
  {
    // fall through and print usage
    if(argequal(argv[1], "--help") || argequal(argv[1], "-h"))
    {
    }
    // if we were given a logfile, load it and continually replay it.
    else if(strstr(argv[1].c_str(), ".rdc") != NULL)
    {
      float progress = 0.0f;
      ReplayRenderer *renderer = NULL;
      auto status = RENDERDOC_CreateReplayRenderer(argv[1].c_str(), &progress, &renderer);

      if(renderer)
      {
        if(status == eReplayCreate_Success)
          DisplayRendererPreview(renderer);

        ReplayRenderer_Shutdown(renderer);
      }
      return 0;
    }
    // dump the image from a logfile
    if(argequal(argv[1], "--thumb") || argequal(argv[1], "-t"))
    {
      if(argv.size() >= 3)
      {
        string jpgname = argv[2];

        if(jpgname[jpgname.length() - 4] == '.' && jpgname[jpgname.length() - 3] == 'r' &&
           jpgname[jpgname.length() - 2] == 'd' && jpgname[jpgname.length() - 1] == 'c')
        {
          jpgname.pop_back();
          jpgname.pop_back();
          jpgname.pop_back();

          jpgname += "jpg";
        }
        else
        {
          jpgname += ".jpg";
        }

        uint32_t len = 0;
        bool32 ret = RENDERDOC_GetThumbnail(argv[2].c_str(), NULL, len);

        if(!ret)
        {
          fprintf(stderr, "No thumbnail in '%s' or error retrieving it", argv[2].c_str());
        }
        else
        {
          byte *jpgbuf = new byte[len];
          RENDERDOC_GetThumbnail(argv[2].c_str(), jpgbuf, len);

          FILE *f = fopen(jpgname.c_str(), "wb");

          if(!f)
          {
            fprintf(stderr, "Couldn't open destination file '%s'.", jpgname.c_str());
          }
          else
          {
            fwrite(jpgbuf, 1, len, f);
            fclose(f);
          }

          delete[] jpgbuf;
        }
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --thumb");
      }
      return 0;
    }
    // replay a logfile
    else if(argequal(argv[1], "--replay") || argequal(argv[1], "-r"))
    {
      if(argv.size() >= 3)
      {
        float progress = 0.0f;
        ReplayRenderer *renderer = NULL;
        auto status = RENDERDOC_CreateReplayRenderer(argv[2].c_str(), &progress, &renderer);

        if(renderer)
        {
          if(status == eReplayCreate_Success)
            DisplayRendererPreview(renderer);

          ReplayRenderer_Shutdown(renderer);
        }
        return 0;
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --replay");
      }
    }
#if defined(RENDERDOC_PLATFORM_WIN32)
    // if we were given an executable on windows, inject into it
    // can't do this on other platforms as there's no nice extension
    // and we don't want to just launch any single parameter in case it's
    // -h or -help or some other guess/typo
    else if(strstr(argv[1].c_str(), ".exe") != NULL)
    {
      uint32_t ident = RENDERDOC_ExecuteAndInject(argv[1].c_str(), NULL, NULL, NULL, &opts, false);

      if(ident == 0)
        fprintf(stderr, "Failed to create & inject\n");
      else
        fprintf(stderr, "Created & injected as %d\n", ident);

      return ident;
    }
#endif
    // capture a program with default capture options
    else if(argequal(argv[1], "--capture") || argequal(argv[1], "-c"))
    {
      if(argv.size() >= 4)
      {
        uint32_t ident =
            RENDERDOC_ExecuteAndInject(argv[2].c_str(), NULL, argv[3].c_str(), NULL, &opts, false);

        if(ident == 0)
          fprintf(stderr, "Failed to create & inject to '%s' with params \"%s\"\n", argv[2].c_str(),
                  argv[3].c_str());
        else
          fprintf(stderr, "Created & injected '%s' with params \"%s\" as %d\n", argv[2].c_str(),
                  argv[3].c_str(), ident);

        return ident;
      }
      else if(argv.size() >= 3)
      {
        uint32_t ident = RENDERDOC_ExecuteAndInject(argv[2].c_str(), NULL, NULL, NULL, &opts, false);

        if(ident == 0)
          fprintf(stderr, "Failed to create & inject to '%s'\n", argv[2].c_str());
        else
          fprintf(stderr, "Created & injected '%s' as %d\n", argv[2].c_str(), ident);

        return ident;
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --capture");
      }
    }
    // inject into a running process with default capture options
    else if(argequal(argv[1], "--inject") || argequal(argv[1], "-i"))
    {
      if(argv.size() >= 3)
      {
        const char *pid = argv[2].c_str();
        while(*pid == '"' || isspace(*pid))
          pid++;

        uint32_t pidNum = (uint32_t)atoi(pid);

        uint32_t ident = RENDERDOC_InjectIntoProcess(pidNum, NULL, &opts, false);

        if(ident == 0)
          printf("Failed to inject to %u\n", pidNum);
        else
          printf("Injected to %u as %u\n", pidNum, ident);

        return ident;
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --inject");
      }
    }
    // spawn remote replay host
    else if(argequal(argv[1], "--replayhost") || argequal(argv[1], "-rh"))
    {
      RENDERDOC_SpawnReplayHost(NULL);
      return 1;
    }
    // replay a logfile over the network on a remote host
    else if(argequal(argv[1], "--remotereplay") || argequal(argv[1], "-rr"))
    {
      if(argv.size() >= 4)
      {
        RemoteRenderer *remote = NULL;
        auto status = RENDERDOC_CreateRemoteReplayConnection(argv[2].c_str(), &remote);

        if(remote == NULL || status != eReplayCreate_Success)
          return 1;

        float progress = 0.0f;

        ReplayRenderer *renderer = NULL;
        status = RemoteRenderer_CreateProxyRenderer(remote, 0, argv[3].c_str(), &progress, &renderer);

        if(renderer)
        {
          if(status == eReplayCreate_Success)
            DisplayRendererPreview(renderer);

          ReplayRenderer_Shutdown(renderer);
        }
        return 0;
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --remotereplay");
      }
    }
    // not documented/useful for manual use on the cmd line, used internally
    else if(argequal(argv[1], "--cap32for64"))
    {
      if(argv.size() >= 5)
      {
        const char *pid = argv[2].c_str();
        while(*pid == '"' || isspace(*pid))
          pid++;

        uint32_t pidNum = (uint32_t)atoi(pid);

        const char *log = argv[3].c_str();
        if(log[0] == 0)
          log = NULL;

        CaptureOptions cmdopts;
        readCapOpts(argv[4].c_str(), &cmdopts);

        return RENDERDOC_InjectIntoProcess(pidNum, log, &cmdopts, false);
      }
      else
      {
        fprintf(stderr, "Not enough parameters to --cap32for64");
      }
    }
  }

  fprintf(stderr, "renderdoccmd usage:\n\n");
  fprintf(stderr,
          "  <file.rdc>                        Launch a preview window that replays this logfile "
          "and\n");
  fprintf(stderr, "                                    displays the backbuffer.\n");
#if defined(RENDERDOC_PLATFORM_WIN32)
  fprintf(stderr,
          "  <program.exe>                     Launch a capture of this program with default "
          "options.\n");
#endif
  fprintf(stderr, "\n");
  fprintf(stderr, "  -h,  --help                       Displays this help message.\n");
  fprintf(stderr,
          "  -t,  --thumb LOGFILE.rdc          Dumps the embedded thumbnail to LOGFILE.jpg if it "
          "exists.\n");
  fprintf(stderr,
          "  -c,  --capture PROGRAM            Launches capture of the program with default "
          "options.\n");
  fprintf(stderr,
          "  -i,  --inject PID                 Injects into the specified PID to capture.\n");
  fprintf(stderr,
          "  -r,  --replay LOGFILE             Launch a preview window that replays this logfile "
          "and\n");
  fprintf(stderr, "                                    displays the backbuffer.\n");
  fprintf(stderr,
          "  -rh, --replayhost                 Starts a replay host server that can be used to "
          "remotely\n");
  fprintf(stderr, "                                    replay logfiles from another machine.\n");
  fprintf(
      stderr,
      "  -rr, --remotereplay HOST LOGFILE  Launch a replay of the logfile and display a preview\n");
  fprintf(
      stderr,
      "                                    window. Use the remote host to replay all commands.\n");

  return 1;
}

int renderdoccmd(int argc, char **c_argv)
{
  std::vector<std::string> argv;
  argv.resize(argc);
  for(int i = 0; i < argc; i++)
    argv[i] = c_argv[i];

  return renderdoccmd(argv);
}
