/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include <replay/version.h>
#include <string>

using std::string;
using std::wstring;

bool usingKillSignal = false;
volatile uint32_t killSignal = false;

rdctype::array<rdctype::str> convertArgs(const std::vector<std::string> &args)
{
  rdctype::array<rdctype::str> ret;
  ret.create((int)args.size());
  for(size_t i = 0; i < args.size(); i++)
    ret[i] = args[i];
  return ret;
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

void DisplayRendererPreview(IReplayController *renderer, uint32_t width, uint32_t height)
{
  if(renderer == NULL)
    return;

  rdctype::array<TextureDescription> texs = renderer->GetTextures();

  TextureDisplay d;
  d.mip = 0;
  d.sampleIdx = ~0U;
  d.overlay = DebugOverlay::NoOverlay;
  d.typeHint = CompType::Typeless;
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
  d.lightBackgroundColor = FloatVector(0.81f, 0.81f, 0.81f, 1.0f);
  d.darkBackgroundColor = FloatVector(0.57f, 0.57f, 0.57f, 1.0f);
  d.Red = d.Green = d.Blue = true;
  d.Alpha = false;

  for(int32_t i = 0; i < texs.count; i++)
  {
    if(texs[i].creationFlags & TextureCategory::SwapBuffer)
    {
      d.texid = texs[i].ID;
      break;
    }
  }

  rdctype::array<DrawcallDescription> draws = renderer->GetDrawcalls();

  if(draws.count > 0 && draws[draws.count - 1].flags & DrawFlags::Present)
  {
    ResourceId id = draws[draws.count - 1].copyDestination;
    if(id != ResourceId())
      d.texid = id;
  }

  DisplayRendererPreview(renderer, d, width, height);
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

  std::cerr << "Usage: renderdoccmd <command> [args ...]" << std::endl;
  std::cerr << "Command line tool for capture & replay with RenderDoc." << std::endl << std::endl;

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

static std::vector<std::string> version_lines;

struct VersionCommand : public Command
{
  VersionCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Print version information"; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::cout << "renderdoccmd " << (sizeof(uintptr_t) == sizeof(uint64_t) ? "x64" : "x86")
              << " v" MAJOR_MINOR_VERSION_STRING << " built from " << GIT_COMMIT_HASH << std::endl;

#if defined(DISTRIBUTION_VERSION)
    std::cout << "Packaged for " << DISTRIBUTION_NAME << " (" << DISTRIBUTION_VERSION << ") - "
              << DISTRIBUTION_CONTACT << std::endl;
#endif

    for(size_t i = 0; i < version_lines.size(); i++)
      std::cout << version_lines[i] << std::endl;

    std::cout << std::endl;

    return 0;
  }
};

void add_version_line(const std::string &str)
{
  version_lines.push_back(str);
}

struct HelpCommand : public Command
{
  HelpCommand(const GlobalEnvironment &env) : Command(env) {}
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
  ThumbCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<filename.rdc>");
    parser.add<string>("out", 'o', "The output filename to save the file to", true, "filename.jpg");
    parser.add<string>("format", 'f',
                       "The format of the output file. If empty, detected from filename", false, "",
                       cmdline::oneof<string>("jpg", "png", "bmp", "tga"));
    parser.add<uint32_t>(
        "max-size", 's',
        "The maximum dimension of the thumbnail. Default is 0, which is unlimited.", false, 0);
  }
  virtual const char *Description() { return "Saves a capture's embedded thumbnail to disk."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: thumb command requires a capture filename." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    string filename = rest[0];

    rest.erase(rest.begin());

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(rest));

    string outfile = parser.get<string>("out");

    string format = parser.get<string>("format");

    FileType type = FileType::JPG;

    if(format == "png")
    {
      type = FileType::PNG;
    }
    else if(format == "tga")
    {
      type = FileType::TGA;
    }
    else if(format == "bmp")
    {
      type = FileType::BMP;
    }
    else
    {
      const char *dot = strrchr(outfile.c_str(), '.');

      if(dot != NULL && strstr(dot, "png"))
        type = FileType::PNG;
      else if(dot != NULL && strstr(dot, "tga"))
        type = FileType::TGA;
      else if(dot != NULL && strstr(dot, "bmp"))
        type = FileType::BMP;
      else
        std::cerr << "Couldn't guess format from '" << outfile << "', defaulting to jpg."
                  << std::endl;
    }

    rdctype::array<byte> buf;

    ICaptureFile *file = RENDERDOC_OpenCaptureFile(filename.c_str());
    if(file->OpenStatus() == ReplayStatus::Succeeded)
    {
      buf = file->GetThumbnail(FileType::JPG, 0);
    }
    else
    {
      std::cerr << "Couldn't open '" << filename << "'" << std::endl;
    }
    file->Shutdown();

    if(buf.empty())
    {
      std::cerr << "Couldn't fetch the thumbnail in '" << filename << "'" << std::endl;
    }
    else
    {
      FILE *f = fopen(outfile.c_str(), "wb");

      if(!f)
      {
        std::cerr << "Couldn't open destination file '" << outfile << "'" << std::endl;
      }
      else
      {
        fwrite(buf.elems, 1, buf.count, f);
        fclose(f);

        std::cout << "Wrote thumbnail from '" << filename << "' to '" << outfile << "'." << std::endl;
      }
    }

    return 0;
  }
};

struct CaptureCommand : public Command
{
  CaptureCommand(const GlobalEnvironment &env) : Command(env) {}
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

    RENDERDOC_InitGlobalEnv(m_Env, rdctype::array<rdctype::str>());

    std::cout << "Launching '" << executable << "'";

    if(!cmdLine.empty())
      std::cout << " with params: " << cmdLine;

    std::cout << std::endl;

    rdctype::array<EnvironmentModification> env;

    uint32_t ident = RENDERDOC_ExecuteAndInject(
        executable.c_str(), workingDir.empty() ? "" : workingDir.c_str(),
        cmdLine.empty() ? "" : cmdLine.c_str(), env, logFile.empty() ? "" : logFile.c_str(), opts,
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
  InjectCommand(const GlobalEnvironment &env) : Command(env) {}
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

    rdctype::array<EnvironmentModification> env;

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(parser.rest()));

    uint32_t ident = RENDERDOC_InjectIntoProcess(PID, env, logFile.empty() ? "" : logFile.c_str(),
                                                 opts, parser.exist("wait-for-exit"));

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

struct RemoteServerCommand : public Command
{
  RemoteServerCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("daemon", 'd', "Go into the background.");
    parser.add<string>(
        "host", 'h', "The interface to listen on. By default listens on all interfaces", false, "");
    parser.add<uint32_t>("port", 'p', "The port to listen on.", false,
                         RENDERDOC_GetDefaultRemoteServerPort());
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

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(parser.rest()));

    std::cerr << "Spawning a replay host listening on " << (host.empty() ? "*" : host) << ":"
              << port << "..." << std::endl;

    if(parser.exist("daemon"))
    {
      std::cerr << "Detaching." << std::endl;
      Daemonise();
    }

    usingKillSignal = true;

    RENDERDOC_BecomeRemoteServer(host.empty() ? NULL : host.c_str(), port, &killSignal);

    std::cerr << std::endl << "Cleaning up from replay hosting." << std::endl;

    return 0;
  }
};

struct ReplayCommand : public Command
{
  ReplayCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<capture.rdc>");
    parser.add<uint32_t>("width", 'w', "The preview window width.", false, 1280);
    parser.add<uint32_t>("height", 'h', "The preview window height.", false, 720);
    parser.add<string>("remote-host", 0,
                       "Instead of replaying locally, replay on this host over the network.", false);
    parser.add<uint32_t>("remote-port", 0, "If --remote-host is set, use this port.", false,
                         RENDERDOC_GetDefaultRemoteServerPort());
  }
  virtual const char *Description()
  {
    return "Replay the log file and show the backbuffer on a preview window.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: capture command requires a filename to load." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    string filename = rest[0];

    rest.erase(rest.begin());

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(rest));

    if(parser.exist("remote-host"))
    {
      std::cout << "Replaying '" << filename << "' on " << parser.get<string>("remote-host") << ":"
                << parser.get<uint32_t>("remote-port") << "." << std::endl;

      IRemoteServer *remote = NULL;
      ReplayStatus status = RENDERDOC_CreateRemoteServerConnection(
          parser.get<string>("remote-host").c_str(), parser.get<uint32_t>("remote-port"), &remote);

      if(remote == NULL || status != ReplayStatus::Succeeded)
      {
        std::cerr << "Error: Couldn't connect to " << parser.get<string>("remote-host") << ":"
                  << parser.get<uint32_t>("remote-port") << "." << std::endl;
        std::cerr << "       Have you run renderdoccmd remoteserver on '"
                  << parser.get<string>("remote-host") << "'?" << std::endl;
        return 1;
      }

      std::cerr << "Copying capture file to remote server" << std::endl;

      rdctype::str remotePath = remote->CopyCaptureToRemote(filename.c_str(), NULL);

      IReplayController *renderer = NULL;
      std::tie(status, renderer) = remote->OpenCapture(~0U, remotePath.elems, NULL);

      if(status == ReplayStatus::Succeeded)
      {
        DisplayRendererPreview(renderer, parser.get<uint32_t>("width"),
                               parser.get<uint32_t>("height"));

        remote->CloseCapture(renderer);
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "'." << std::endl;
      }

      remote->ShutdownConnection();
    }
    else
    {
      std::cout << "Replaying '" << filename << "' locally.." << std::endl;

      ICaptureFile *file = RENDERDOC_OpenCaptureFile(filename.c_str());

      if(file->OpenStatus() != ReplayStatus::Succeeded)
      {
        std::cerr << "Couldn't load '" << filename << "'." << std::endl;
        return 1;
      }

      IReplayController *renderer = NULL;
      ReplayStatus status = ReplayStatus::InternalError;
      std::tie(status, renderer) = file->OpenCapture(NULL);

      file->Shutdown();

      if(status == ReplayStatus::Succeeded)
      {
        DisplayRendererPreview(renderer, parser.get<uint32_t>("width"),
                               parser.get<uint32_t>("height"));

        renderer->Shutdown();
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "'." << std::endl;
        return 1;
      }
    }
    return 0;
  }
};

struct CapAltBitCommand : public Command
{
  CapAltBitCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<uint32_t>("pid", 0, "");
    parser.add<string>("log", 0, "");
    parser.add<string>("debuglog", 0, "");
    parser.add<string>("capopts", 0, "");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Internal use only!"; }
  virtual bool IsInternalOnly() { return true; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    CaptureOptions cmdopts;
    readCapOpts(parser.get<string>("capopts").c_str(), &cmdopts);

    RENDERDOC_InitGlobalEnv(m_Env, rdctype::array<rdctype::str>());

    std::vector<std::string> rest = parser.rest();

    if(rest.size() % 3 != 0)
    {
      std::cerr << "Invalid generated capaltbit command rest.size() == " << rest.size() << std::endl;
      return 0;
    }

    int numEnvs = int(rest.size() / 3);

    rdctype::array<EnvironmentModification> env;
    env.create(numEnvs);

    for(int i = 0; i < numEnvs; i++)
    {
      string typeString = rest[i * 3 + 0];

      EnvMod type = EnvMod::Set;
      EnvSep sep = EnvSep::NoSep;

      if(typeString == "+env-replace")
      {
        type = EnvMod::Set;
        sep = EnvSep::NoSep;
      }
      else if(typeString == "+env-append-platform")
      {
        type = EnvMod::Append;
        sep = EnvSep::Platform;
      }
      else if(typeString == "+env-append-semicolon")
      {
        type = EnvMod::Append;
        sep = EnvSep::SemiColon;
      }
      else if(typeString == "+env-append-colon")
      {
        type = EnvMod::Append;
        sep = EnvSep::Colon;
      }
      else if(typeString == "+env-append")
      {
        type = EnvMod::Append;
        sep = EnvSep::NoSep;
      }
      else if(typeString == "+env-prepend-platform")
      {
        type = EnvMod::Prepend;
        sep = EnvSep::Platform;
      }
      else if(typeString == "+env-prepend-semicolon")
      {
        type = EnvMod::Prepend;
        sep = EnvSep::SemiColon;
      }
      else if(typeString == "+env-prepend-colon")
      {
        type = EnvMod::Prepend;
        sep = EnvSep::Colon;
      }
      else if(typeString == "+env-prepend")
      {
        type = EnvMod::Prepend;
        sep = EnvSep::NoSep;
      }
      else
      {
        std::cerr << "Invalid generated capaltbit env '" << rest[i * 3 + 0] << std::endl;
        return 0;
      }

      env[i] = EnvironmentModification(type, sep, rest[i * 3 + 1].c_str(), rest[i * 3 + 2].c_str());
    }

    string debuglog = parser.get<string>("debuglog");

    RENDERDOC_SetDebugLogFile(debuglog.c_str());

    int ret = RENDERDOC_InjectIntoProcess(parser.get<uint32_t>("pid"), env,
                                          parser.get<string>("log").c_str(), cmdopts, false);

    return ret;
  }
};

int renderdoccmd(const GlobalEnvironment &env, std::vector<std::string> &argv)
{
  try
  {
    // add basic commands, and common aliases
    add_command("version", new VersionCommand(env));

    add_alias("--version", "version");
    add_alias("-v", "version");
    // for windows
    add_alias("/version", "version");
    add_alias("/v", "version");

    add_command("help", new HelpCommand(env));

    add_alias("--help", "help");
    add_alias("-h", "help");
    add_alias("-?", "help");

    // for windows
    add_alias("/help", "help");
    add_alias("/h", "help");
    add_alias("/?", "help");

    // add platform agnostic commands
    add_command("thumb", new ThumbCommand(env));
    add_command("capture", new CaptureCommand(env));
    add_command("inject", new InjectCommand(env));
    add_command("remoteserver", new RemoteServerCommand(env));
    add_command("replay", new ReplayCommand(env));
    add_command("capaltbit", new CapAltBitCommand(env));

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
      cmd.add<string>("capture-file", 'c',
                      "Set the filename template for new captures. Frame number will be "
                      "automatically appended.",
                      false);
      cmd.add("wait-for-exit", 'w', "Wait for the target program to exit, before returning.");

      // CaptureOptions
      cmd.add("opt-disallow-vsync", 0,
              "Capturing Option: Disallow the application from enabling vsync.");
      cmd.add("opt-disallow-fullscreen", 0,
              "Capturing Option: Disallow the application from enabling fullscreen.");
      cmd.add("opt-api-validation", 0,
              "Capturing Option: Record API debugging events and messages.");
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
  catch(std::exception e)
  {
    fprintf(stderr, "Unexpected exception: %s\n", e.what());

    exit(1);
  }
}

int renderdoccmd(const GlobalEnvironment &env, int argc, char **c_argv)
{
  std::vector<std::string> argv;
  argv.resize(argc);
  for(int i = 0; i < argc; i++)
    argv[i] = c_argv[i];

  return renderdoccmd(env, argv);
}
