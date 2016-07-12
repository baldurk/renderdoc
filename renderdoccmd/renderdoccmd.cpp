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

#include "renderdoccmd.h"
#include <app/renderdoc_app.h>
#include <replay/renderdoc_replay.h>
#include <string>

using std::string;
using std::wstring;

void readCapOpts(const std::string &str, CaptureOptions *opts)
{
  if(str.length() < sizeof(CaptureOptions))
    return;

  // serialise from string with two chars per byte
  byte *b = (byte *)opts;
  for(size_t i = 0; i < sizeof(CaptureOptions); i++)
    *(b++) = (byte(str[i * 2 + 0] - 'a') << 4) | byte(str[i * 2 + 1] - 'a');
}

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

std::map<std::string, Command *> commands;
std::map<std::string, std::string> aliases;

void add_command(const std::string &name, Command *cmd)
{
  commands[name] = cmd;
}

void add_alias(const std::string &alias, const std::string &command)
{
  aliases[alias] = command;
}

static void clean_up()
{
  for(auto it = commands.begin(); it != commands.end(); ++it)
    delete it->second;
}

static int command_usage(std::string command = "")
{
  if(!command.empty())
    std::cerr << command << " is not a valid command." << std::endl << std::endl;

  std::cerr << "renderdoccmd <command> [args ...]" << std::endl << std::endl;

  std::cerr << "Command can be one of:" << std::endl;

  size_t max_width = 0;
  for(auto it = commands.begin(); it != commands.end(); ++it)
  {
    if(it->second->IsInternalOnly())
      continue;

    max_width = std::max(max_width, it->first.length());
  }

  for(auto it = commands.begin(); it != commands.end(); ++it)
  {
    if(it->second->IsInternalOnly())
      continue;

    std::cerr << "  " << it->first;
    for(size_t n = it->first.length(); n < max_width + 4; n++)
      std::cerr << ' ';
    std::cerr << it->second->Description() << std::endl;
  }
  std::cerr << std::endl;

  std::cerr << "To see details of any command, see 'renderdoccmd <command> --help'" << std::endl
            << std::endl;

  std::cerr << "For more information, see <https://renderdoc.org/>." << std::endl;

  return 2;
}

struct VersionCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Print version information"; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::cout << "renderdoccmd " << RENDERDOC_GetVersionString()
              << (sizeof(uintptr_t) == sizeof(uint64_t) ? " x64 " : " x86 ")
              << RENDERDOC_GetCommitHash() << std::endl;
    return 0;
  }
};

struct HelpCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Print this help message"; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    command_usage();
    return 0;
  }
};

struct ThumbCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<filename.rdc>");
    parser.add<string>("out", 'o', "The output filename to save the jpg to", false, "filename.jpg");
  }
  virtual const char *Description() { return "Saves a capture's embedded thumbnail to disk."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    if(parser.rest().empty())
    {
      std::cerr << "Error: thumb command requires a capture filename." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    string filename = parser.rest()[0];

    string jpgname;

    if(parser.exist("out"))
    {
      jpgname = parser.get<string>("out");
    }
    else
    {
      jpgname = filename;

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
    }

    uint32_t len = 0;
    bool32 ret = RENDERDOC_GetThumbnail(filename.c_str(), NULL, len);

    if(!ret)
    {
      std::cerr << "Couldn't fetch the thumbnail in '" << filename << "'" << std::endl;
    }
    else
    {
      byte *jpgbuf = new byte[len];
      RENDERDOC_GetThumbnail(filename.c_str(), jpgbuf, len);

      FILE *f = fopen(jpgname.c_str(), "wb");

      if(!f)
      {
        std::cerr << "Couldn't open destination file '" << jpgname << "'" << std::endl;
      }
      else
      {
        fwrite(jpgbuf, 1, len, f);
        fclose(f);

        std::cout << "Wrote thumbnail from '" << filename << "' to '" << jpgname << "'." << std::endl;
      }

      delete[] jpgbuf;
    }

    return 0;
  }
};

struct CaptureCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<executable> [program arguments]");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Launches the given executable to capture."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return true; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &opts)
  {
    if(parser.rest().empty())
    {
      std::cerr << "Error: capture command requires an executable to launch." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    std::string executable = parser.rest()[0];
    std::string workingDir = parser.get<string>("working-dir");
    std::string cmdLine;
    std::string logFile = parser.get<string>("capture-file");

    for(size_t i = 1; i < parser.rest().size(); i++)
    {
      if(!cmdLine.empty())
        cmdLine += ' ';

      cmdLine += EscapeArgument(parser.rest()[i]);
    }

    std::cout << "Launching '" << executable << "'";

    if(!cmdLine.empty())
      std::cout << " with params: " << cmdLine;

    std::cout << std::endl;

    uint32_t ident = RENDERDOC_ExecuteAndInject(
        executable.c_str(), workingDir.empty() ? "" : workingDir.c_str(),
        cmdLine.empty() ? "" : cmdLine.c_str(), logFile.empty() ? "" : logFile.c_str(), &opts,
        parser.exist("wait-for-exit"));

    if(ident == 0)
    {
      std::cerr << "Failed to create & inject." << std::endl;
      return 2;
    }

    if(parser.exist("wait-for-exit"))
    {
      std::cerr << "'" << executable << "' finished executing." << std::endl;
      ident = 0;
    }
    else
    {
      std::cerr << "Launched as ID " << ident << std::endl;
    }

    return ident;
  }

  std::string EscapeArgument(const std::string &arg)
  {
    // nothing to escape or quote
    if(arg.find_first_of(" \t\r\n\"") == std::string::npos)
      return arg;

    // return arg in quotes, with any quotation marks escaped
    std::string ret = arg;

    size_t i = ret.find('\"');
    while(i != std::string::npos)
    {
      ret.insert(ret.begin() + i, '\\');

      i = ret.find('\"', i + 2);
    }

    return '"' + ret + '"';
  }
};

struct InjectCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<uint32_t>("PID", 0, "The process ID of the process to inject.", true);
  }
  virtual const char *Description() { return "Injects RenderDoc into a given running process."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return true; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &opts)
  {
    uint32_t PID = parser.get<uint32_t>("PID");
    std::string workingDir = parser.get<string>("working-dir");
    std::string logFile = parser.get<string>("capture-file");

    std::cout << "Injecting into PID " << PID << std::endl;

    uint32_t ident = RENDERDOC_InjectIntoProcess(PID, logFile.empty() ? "" : logFile.c_str(), &opts,
                                                 parser.exist("wait-for-exit"));

    if(ident == 0)
    {
      std::cerr << "Failed to inject." << std::endl;
      return 2;
    }

    if(parser.exist("wait-for-exit"))
    {
      std::cerr << PID << " finished executing." << std::endl;
      ident = 0;
    }
    else
    {
      std::cerr << "Launched as ID " << ident << std::endl;
    }

    return ident;
  }
};

struct ReplayHostCommand : public Command
{
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<string>(
        "host", 'h', "The interface to listen on. By default listens on all interfaces", false, "");
    parser.add<uint32_t>("port", 'p', "The port to listen on.", false,
                         RENDERDOC_DefaultReplayHostPort());
  }
  virtual const char *Description()
  {
    return "Start up a server listening as a host for remote replays.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    string host = parser.get<string>("host");
    uint32_t port = parser.get<uint32_t>("port");

    volatile bool32 kill = false;

    std::cerr << "Spawning a replay host listening on " << (host.empty() ? "*" : host) << ":"
              << port << "..." << std::endl;

    RENDERDOC_SpawnReplayHost(host.empty() ? NULL : host.c_str(), port, &kill);
    return 0;
  }
};

int renderdoccmd(std::vector<std::string> &argv)
{
  // add basic commands, and common aliases
  add_command("version", new VersionCommand());
  add_alias("--version", "version");
  add_alias("-v", "version");
  add_command("help", new HelpCommand());
  add_alias("-h", "help");
  add_alias("-?", "help");

  // add platform agnostic commands
  add_command("thumb", new ThumbCommand());
  add_command("capture", new CaptureCommand());
  add_command("inject", new InjectCommand());
  add_command("replayhost", new ReplayHostCommand());
  // add_command("replay", new ReplayCommand());
  // add_command("cap32for64", new Cap32For64Command());
  // add_command("remotereplay", new RemoteReplayCommand());

  if(argv.size() <= 1)
  {
    int ret = command_usage();
    clean_up();
    return ret;
  }

  // std::string programName = argv[0];

  argv.erase(argv.begin());

  std::string command = *argv.begin();

  argv.erase(argv.begin());

  auto it = commands.find(command);

  if(it == commands.end())
  {
    auto a = aliases.find(command);
    if(a != aliases.end())
      it = commands.find(a->second);
  }

  if(it == commands.end())
  {
    int ret = command_usage(command);
    clean_up();
    return ret;
  }

  cmdline::parser cmd;

  cmd.set_program_name("renderdoccmd");
  cmd.set_header(command);

  it->second->AddOptions(cmd);

  if(it->second->IsCaptureCommand())
  {
    cmd.add<string>("working-dir", 'd', "Set the working directory of the program, if launched.",
                    false);
    cmd.add<string>(
        "capture-file", 'c',
        "Set the filename template for new captures. Frame number will be automatically appended.",
        false);
    cmd.add("wait-for-exit", 'w', "Wait for the target program to exit, before returning.");

    // CaptureOptions
    cmd.add("opt-disallow-vsync", 0,
            "Capturing Option: Disallow the application from enabling vsync.");
    cmd.add("opt-disallow-fullscreen", 0,
            "Capturing Option: Disallow the application from enabling fullscreen.");
    cmd.add("opt-api-validation", 0, "Capturing Option: Record API debugging events and messages.");
    cmd.add("opt-api-validation-unmute", 0,
            "Capturing Option: Unmutes API debugging output from --opt-api-validation.");
    cmd.add("opt-capture-callstacks", 0,
            "Capturing Option: Capture CPU callstacks for API events.");
    cmd.add("opt-capture-callstacks-only-draws", 0,
            "Capturing Option: When capturing CPU callstacks, only capture them from drawcalls.");
    cmd.add<int>("opt-delay-for-debugger", 0,
                 "Capturing Option: Specify a delay in seconds to wait for a debugger to attach.",
                 false, 0, cmdline::range(0, 10000));
    cmd.add("opt-verify-map-writes", 0,
            "Capturing Option: Verify any writes to mapped buffers, by bounds checking.");
    cmd.add("opt-hook-children", 0,
            "Capturing Option: Hooks any system API calls that create child processes.");
    cmd.add("opt-ref-all-resources", 0,
            "Capturing Option: Include all live resources, not just those used by a frame.");
    cmd.add("opt-save-all-initials", 0,
            "Capturing Option: Save all initial resource contents at frame start.");
    cmd.add("opt-capture-all-cmd-lists", 0,
            "Capturing Option: In D3D11, record all command lists from application start.");
  }

  cmd.parse_check(argv, true);

  CaptureOptions opts;
  RENDERDOC_GetDefaultCaptureOptions(&opts);

  if(it->second->IsCaptureCommand())
  {
    if(cmd.exist("opt-disallow-vsync"))
      opts.AllowVSync = false;
    if(cmd.exist("opt-disallow-fullscreen"))
      opts.AllowFullscreen = false;
    if(cmd.exist("opt-api-validation"))
      opts.APIValidation = true;
    if(cmd.exist("opt-api-validation-unmute"))
      opts.DebugOutputMute = false;
    if(cmd.exist("opt-capture-callstacks"))
      opts.CaptureCallstacks = true;
    if(cmd.exist("opt-capture-callstacks-only-draws"))
      opts.CaptureCallstacksOnlyDraws = true;
    if(cmd.exist("opt-verify-map-writes"))
      opts.VerifyMapWrites = true;
    if(cmd.exist("opt-hook-children"))
      opts.HookIntoChildren = true;
    if(cmd.exist("opt-ref-all-resources"))
      opts.RefAllResources = true;
    if(cmd.exist("opt-save-all-initials"))
      opts.SaveAllInitials = true;
    if(cmd.exist("opt-capture-all-cmd-lists"))
      opts.CaptureAllCmdLists = true;

    opts.DelayForDebugger = (uint32_t)cmd.get<int>("opt-delay-for-debugger");
  }

  if(cmd.exist("help"))
  {
    std::cerr << cmd.usage() << std::endl;
    clean_up();
    return 0;
  }

  int ret = it->second->Execute(cmd, opts);
  clean_up();
  return ret;
}

#if 0
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
#endif

int renderdoccmd(int argc, char **c_argv)
{
  std::vector<std::string> argv;
  argv.resize(argc);
  for(int i = 0; i < argc; i++)
    argv[i] = c_argv[i];

  return renderdoccmd(argv);
}
