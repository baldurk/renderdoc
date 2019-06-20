/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

// normally this is in the renderdoc core library, but it's needed for the 'unknown enum' path,
// so we implement it here using ostringstream. It's not great, but this is a very uncommon path -
// either for invalid values or for when a new enum is added and the code isn't updated
template <>
rdcstr DoStringise(const uint32_t &el)
{
  std::ostringstream oss;
  oss << el;
  return oss.str();
}

#include <replay/renderdoc_tostr.inl>

bool usingKillSignal = false;
volatile bool killSignal = false;

rdcarray<rdcstr> convertArgs(const std::vector<std::string> &args)
{
  rdcarray<rdcstr> ret;
  ret.reserve(args.size());
  for(size_t i = 0; i < args.size(); i++)
    ret.push_back(args[i]);
  return ret;
}

void DisplayRendererPreview(IReplayController *renderer, uint32_t width, uint32_t height,
                            uint32_t numLoops)
{
  if(renderer == NULL)
    return;

  rdcarray<TextureDescription> texs = renderer->GetTextures();

  TextureDisplay d;
  d.mip = 0;
  d.sampleIdx = ~0U;
  d.overlay = DebugOverlay::NoOverlay;
  d.typeHint = CompType::Typeless;
  d.customShaderId = ResourceId();
  d.hdrMultiplier = -1.0f;
  d.linearDisplayAsGamma = true;
  d.flipY = false;
  d.rangeMin = 0.0f;
  d.rangeMax = 1.0f;
  d.scale = 1.0f;
  d.xOffset = 0.0f;
  d.yOffset = 0.0f;
  d.sliceFace = 0;
  d.rawOutput = false;
  d.red = d.green = d.blue = true;
  d.alpha = false;

  for(const TextureDescription &desc : texs)
  {
    if(desc.creationFlags & TextureCategory::SwapBuffer)
    {
      d.resourceId = desc.resourceId;
      break;
    }
  }

  rdcarray<DrawcallDescription> draws = renderer->GetDrawcalls();

  if(!draws.empty() && draws.back().flags & DrawFlags::Present)
  {
    ResourceId id = draws.back().copyDestination;
    if(id != ResourceId())
      d.resourceId = id;
  }

  DisplayRendererPreview(renderer, d, width, height, numLoops);
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
              << " v" MAJOR_MINOR_VERSION_STRING << " built from " << RENDERDOC_GetCommitHash()
              << std::endl;

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
    parser.add<std::string>("out", 'o', "The output filename to save the file to", true,
                            "filename.jpg");
    parser.add<std::string>("format", 'f',
                            "The format of the output file. If empty, detected from filename",
                            false, "", cmdline::oneof<std::string>("jpg", "png", "bmp", "tga"));
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

    std::string filename = rest[0];

    rest.erase(rest.begin());

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(rest));

    std::string outfile = parser.get<std::string>("out");

    std::string format = parser.get<std::string>("format");

    uint32_t maxsize = parser.get<uint32_t>("max-size");

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
      else if(dot != NULL && strstr(dot, "jpg"))
        type = FileType::JPG;
      else
        std::cerr << "Couldn't guess format from '" << outfile << "', defaulting to jpg."
                  << std::endl;
    }

    bytebuf buf;

    ICaptureFile *file = RENDERDOC_OpenCaptureFile();
    ReplayStatus st = file->OpenFile(filename.c_str(), "rdc", NULL);
    if(st == ReplayStatus::Succeeded)
    {
      buf = file->GetThumbnail(type, maxsize).data;
    }
    else
    {
      std::cerr << "Couldn't open '" << filename << "': " << ToStr(st) << std::endl;
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
        fwrite(buf.data(), 1, buf.size(), f);
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
    std::string workingDir = parser.get<std::string>("working-dir");
    std::string cmdLine;
    std::string logFile = parser.get<std::string>("capture-file");

    for(size_t i = 1; i < parser.rest().size(); i++)
    {
      if(!cmdLine.empty())
        cmdLine += ' ';

      cmdLine += EscapeArgument(parser.rest()[i]);
    }

    RENDERDOC_InitGlobalEnv(m_Env, rdcarray<rdcstr>());

    std::cout << "Launching '" << executable << "'";

    if(!cmdLine.empty())
      std::cout << " with params: " << cmdLine;

    std::cout << std::endl;

    rdcarray<EnvironmentModification> env;

    ExecuteResult res = RENDERDOC_ExecuteAndInject(
        executable.c_str(), workingDir.empty() ? "" : workingDir.c_str(),
        cmdLine.empty() ? "" : cmdLine.c_str(), env, logFile.empty() ? "" : logFile.c_str(), opts,
        parser.exist("wait-for-exit"));

    if(res.status != ReplayStatus::Succeeded)
    {
      std::cerr << "Failed to create & inject: " << ToStr(res.status) << std::endl;
      return (int)res.status;
    }

    if(parser.exist("wait-for-exit"))
    {
      std::cerr << "'" << executable << "' finished executing." << std::endl;
      res.ident = 0;
    }
    else
    {
      std::cerr << "Launched as ID " << res.ident << std::endl;
    }

    return res.ident;
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
    std::string workingDir = parser.get<std::string>("working-dir");
    std::string logFile = parser.get<std::string>("capture-file");

    std::cout << "Injecting into PID " << PID << std::endl;

    rdcarray<EnvironmentModification> env;

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(parser.rest()));

    ExecuteResult res = RENDERDOC_InjectIntoProcess(
        PID, env, logFile.empty() ? "" : logFile.c_str(), opts, parser.exist("wait-for-exit"));

    if(res.status != ReplayStatus::Succeeded)
    {
      std::cerr << "Failed to inject: " << ToStr(res.status) << std::endl;
      return (int)res.status;
    }

    if(parser.exist("wait-for-exit"))
    {
      std::cerr << PID << " finished executing." << std::endl;
      res.ident = 0;
    }
    else
    {
      std::cerr << "Launched as ID " << res.ident << std::endl;
    }

    return res.ident;
  }
};

struct RemoteServerCommand : public Command
{
  RemoteServerCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("daemon", 'd', "Go into the background.");
    parser.add<std::string>(
        "host", 'h', "The interface to listen on. By default listens on all interfaces", false, "");
    parser.add<uint32_t>("port", 'p', "The port to listen on.", false,
                         RENDERDOC_GetDefaultRemoteServerPort());
    parser.add("preview", 'v', "Display a preview window when a replay is active.");
  }
  virtual const char *Description()
  {
    return "Start up a server listening as a host for remote replays.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::string host = parser.get<std::string>("host");
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

    // by default have a do-nothing callback that creates no windows
    RENDERDOC_PreviewWindowCallback previewWindow;

    // if the user asked for a preview, then call to the platform-specific preview function
    if(parser.exist("preview"))
      previewWindow = &DisplayRemoteServerPreview;

    // OR if the platform-specific preview function always has a window, then return it anyway.
    if(DisplayRemoteServerPreview(false, {}).system != WindowingSystem::Unknown)
      previewWindow = &DisplayRemoteServerPreview;

    RENDERDOC_BecomeRemoteServer(host.empty() ? NULL : host.c_str(), port,
                                 []() { return killSignal; }, previewWindow);

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
    parser.add<uint32_t>("loops", 'l', "How many times to loop the replay, or 0 for indefinite.",
                         false, 0);
    parser.add<std::string>("remote-host", 0,
                            "Instead of replaying locally, replay on this host over the network.",
                            false);
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

    std::string filename = rest[0];

    rest.erase(rest.begin());

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(rest));

    if(parser.exist("remote-host"))
    {
      std::cout << "Replaying '" << filename << "' on " << parser.get<std::string>("remote-host")
                << ":" << parser.get<uint32_t>("remote-port") << "." << std::endl;

      IRemoteServer *remote = NULL;
      ReplayStatus status =
          RENDERDOC_CreateRemoteServerConnection(parser.get<std::string>("remote-host").c_str(),
                                                 parser.get<uint32_t>("remote-port"), &remote);

      if(remote == NULL || status != ReplayStatus::Succeeded)
      {
        std::cerr << "Error: " << ToStr(status) << " - Couldn't connect to "
                  << parser.get<std::string>("remote-host") << ":"
                  << parser.get<uint32_t>("remote-port") << "." << std::endl;
        std::cerr << "       Have you run renderdoccmd remoteserver on '"
                  << parser.get<std::string>("remote-host") << "'?" << std::endl;
        return 1;
      }

      std::cerr << "Copying capture file to remote server" << std::endl;

      rdcstr remotePath = remote->CopyCaptureToRemote(filename.c_str(), NULL);

      IReplayController *renderer = NULL;
      rdctie(status, renderer) = remote->OpenCapture(~0U, remotePath.c_str(), NULL);

      if(status == ReplayStatus::Succeeded)
      {
        DisplayRendererPreview(renderer, parser.get<uint32_t>("width"),
                               parser.get<uint32_t>("height"), parser.get<uint32_t>("loops"));

        remote->CloseCapture(renderer);
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "': " << ToStr(status) << std::endl;
      }

      remote->ShutdownConnection();
    }
    else
    {
      std::cout << "Replaying '" << filename << "' locally.." << std::endl;

      ICaptureFile *file = RENDERDOC_OpenCaptureFile();

      if(file->OpenFile(filename.c_str(), "rdc", NULL) != ReplayStatus::Succeeded)
      {
        std::cerr << "Couldn't load '" << filename << "'." << std::endl;
        return 1;
      }

      IReplayController *renderer = NULL;
      ReplayStatus status = ReplayStatus::InternalError;
      rdctie(status, renderer) = file->OpenCapture(NULL);

      file->Shutdown();

      if(status == ReplayStatus::Succeeded)
      {
        DisplayRendererPreview(renderer, parser.get<uint32_t>("width"),
                               parser.get<uint32_t>("height"), parser.get<uint32_t>("loops"));

        renderer->Shutdown();
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "': " << ToStr(status) << std::endl;
        return 1;
      }
    }
    return 0;
  }
};

struct formats_reader
{
  formats_reader()
  {
    ICaptureFile *tmp = RENDERDOC_OpenCaptureFile();

    for(const CaptureFileFormat &f : tmp->GetCaptureFileFormats())
    {
      exts.push_back(f.extension);
      names.push_back(f.name);
    }

    tmp->Shutdown();
  }
  std::string operator()(const std::string &s)
  {
    if(std::find(exts.begin(), exts.end(), s) == exts.end())
      throw cmdline::cmdline_error("'" + s + "' is not one of the accepted values");
    return s;
  }
  std::string description() const
  {
    std::string ret = "Options are:";
    for(size_t i = 0; i < exts.size(); i++)
      ret += "\n  * " + exts[i] + " - " + names[i];
    return ret;
  }

private:
  std::vector<std::string> exts;
  std::vector<std::string> names;
};

struct ConvertCommand : public Command
{
  rdcarray<CaptureFileFormat> m_Formats;

  ConvertCommand(const GlobalEnvironment &env) : Command(env)
  {
    ICaptureFile *tmp = RENDERDOC_OpenCaptureFile();

    m_Formats = tmp->GetCaptureFileFormats();

    tmp->Shutdown();
  }

  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<std::string>("filename", 'f', "The file to convert from.", false);
    parser.add<std::string>("output", 'o', "The file to convert to.", false);
    parser.add<std::string>("input-format", 'i', "The format of the input file.", false, "",
                            formats_reader());
    parser.add<std::string>("convert-format", 'c', "The format of the output file.", false, "",
                            formats_reader());
    parser.add("list-formats", '\0', "Print a list of target formats.");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Convert between capture formats."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    if(parser.exist("list-formats"))
    {
      std::cout << "Available formats:" << std::endl;
      for(CaptureFileFormat f : m_Formats)
        std::cout << "'" << (std::string)f.extension << "': " << (std::string)f.name << std::endl
                  << " * " << (std::string)f.description << std::endl
                  << std::endl;
      return 0;
    }

    std::string infile = parser.get<std::string>("filename");
    std::string outfile = parser.get<std::string>("output");

    if(infile.empty())
    {
      std::cerr << "Need an input filename (-f)." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return 1;
    }

    if(outfile.empty())
    {
      std::cerr << "Need an output filename (-o)." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return 1;
    }

    std::string infmt = parser.get<std::string>("input-format");
    std::string outfmt = parser.get<std::string>("convert-format");

    // sort the formats by the length of the extension, so we check the longest ones first. This
    // means that .zip.xml will get chosen before just .xml
    std::sort(m_Formats.begin(), m_Formats.end(),
              [](const CaptureFileFormat &a, const CaptureFileFormat &b) {
                return a.extension.size() > b.extension.size();
              });

    if(infmt.empty())
    {
      // try to guess the format by looking for the extension in the filename
      for(CaptureFileFormat f : m_Formats)
      {
        std::string extension = ".";
        extension += f.extension;

        if(infile.find(extension.c_str()) != std::string::npos)
        {
          infmt = f.extension;
          break;
        }
      }
    }

    if(infmt.empty())
    {
      std::cerr << "Couldn't guess input format from filename." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return 1;
    }

    if(outfmt.empty())
    {
      // try to guess the format by looking for the extension in the filename
      for(CaptureFileFormat f : m_Formats)
      {
        std::string extension = ".";
        extension += f.extension;

        if(outfile.find(extension.c_str()) != std::string::npos)
        {
          outfmt = f.extension;
          break;
        }
      }
    }

    if(outfmt.empty())
    {
      std::cerr << "Couldn't guess output format from filename." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return 1;
    }

    ICaptureFile *file = RENDERDOC_OpenCaptureFile();

    ReplayStatus st = file->OpenFile(infile.c_str(), infmt.c_str(), NULL);

    if(st != ReplayStatus::Succeeded)
    {
      std::cerr << "Couldn't load '" << infile << "' as '" << infmt << "': " << ToStr(st)
                << std::endl;
      return 1;
    }

    st = file->Convert(outfile.c_str(), outfmt.c_str(), NULL, NULL);

    if(st != ReplayStatus::Succeeded)
    {
      std::cerr << "Couldn't convert '" << infile << "' to '" << outfile << "' as '" << outfmt
                << "': " << ToStr(st) << std::endl;
      return 1;
    }

    std::cout << "Converted '" << infile << "' to '" << outfile << "'" << std::endl;

    return 0;
  }
};

struct TestCommand : public Command
{
  TestCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer(
#if PYTHON_VERSION_MINOR > 0
        "<unit|functional>"
#else
        "<unit>"
#endif
        " [... parameters to test framework ...]");
    parser.add("help", '\0', "print this message");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Run internal tests such as unit tests."; }
  virtual bool HandlesUsageManually() { return true; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    std::vector<std::string> rest = parser.rest();

    if(rest.empty())
    {
      std::cerr << "First argument must specify a test framework" << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return 1;
    }

    std::string mode = rest[0];
    rest.erase(rest.begin());

    if(parser.exist("help"))
      rest.push_back("--help");

    if(mode == "unit")
      return RENDERDOC_RunUnitTests("renderdoccmd test unit", convertArgs(rest));
#if PYTHON_VERSION_MINOR > 0
    else if(mode == "functional")
      return RENDERDOC_RunFunctionalTests(PYTHON_VERSION_MINOR, convertArgs(rest));
#endif

    std::cerr << "Unsupported test frame work '" << mode << "'" << std::endl << std::endl;
    std::cerr << parser.usage() << std::endl;
    return 1;
  }
};

struct CapAltBitCommand : public Command
{
  CapAltBitCommand(const GlobalEnvironment &env) : Command(env) {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<uint32_t>("pid", 0, "");
    parser.add<std::string>("capfile", 0, "");
    parser.add<std::string>("debuglog", 0, "");
    parser.add<std::string>("capopts", 0, "");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Internal use only!"; }
  virtual bool IsInternalOnly() { return true; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    CaptureOptions cmdopts;
    cmdopts.DecodeFromString(parser.get<std::string>("capopts"));

    RENDERDOC_InitGlobalEnv(m_Env, rdcarray<rdcstr>());

    std::vector<std::string> rest = parser.rest();

    if(rest.size() % 3 != 0)
    {
      std::cerr << "Invalid generated capaltbit command rest.size() == " << rest.size() << std::endl;
      return 0;
    }

    int numEnvs = int(rest.size() / 3);

    rdcarray<EnvironmentModification> env;
    env.reserve(numEnvs);

    for(int i = 0; i < numEnvs; i++)
    {
      std::string typeString = rest[i * 3 + 0];

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

      env.push_back(
          EnvironmentModification(type, sep, rest[i * 3 + 1].c_str(), rest[i * 3 + 2].c_str()));
    }

    std::string debuglog = parser.get<std::string>("debuglog");

    RENDERDOC_SetDebugLogFile(debuglog.c_str());

    ExecuteResult result = RENDERDOC_InjectIntoProcess(
        parser.get<uint32_t>("pid"), env, parser.get<std::string>("capfile").c_str(), cmdopts, false);

    if(result.status == ReplayStatus::Succeeded)
      return result.ident;

    return (int)result.status;
  }
};

struct EmbeddedSectionCommand : public Command
{
  bool m_Extract = false;
  EmbeddedSectionCommand(const GlobalEnvironment &env, bool extract) : Command(env)
  {
    m_Extract = extract;
  }
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<capture.rdc>");
    parser.add<std::string>("section", 's', "The embedded section name.");
    parser.add<std::string>("file", 'f', m_Extract ? "The file to write the section contents to."
                                                   : "The file to read the section contents from.");
    parser.add("no-clobber", 'n', m_Extract ? "Don't overwrite the file if it already exists."
                                            : "Don't overwrite the section if it already exists.");

    if(!m_Extract)
    {
      parser.add("lz4", 0, "Use LZ4 to compress the data.");
      parser.add("zstd", 0, "Use Zstandard to compress the data.");
    }

    parser.add("list-sections", 0, "Print a list of known sections.");
  }
  virtual const char *Description()
  {
    if(m_Extract)
      return "Extract an arbitrary section of data from a capture.";
    else
      return "Inject an arbitrary section of data into a capture.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    if(parser.exist("list-sections"))
    {
      std::cout << "Known sections:" << std::endl;
      for(SectionType s : values<SectionType>())
        std::cout << ToStr(s) << std::endl;
      return 0;
    }

    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: this command requires a filename to load." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    std::string rdc = rest[0];

    rest.erase(rest.begin());

    RENDERDOC_InitGlobalEnv(m_Env, convertArgs(rest));

    std::string file = parser.get<std::string>("file");
    std::string section = parser.get<std::string>("section");
    bool noclobber = parser.exist("no-clobber");
    bool lz4 = !m_Extract && parser.exist("lz4");
    bool zstd = !m_Extract && parser.exist("zstd");

    if(zstd && lz4)
    {
      std::cerr << "Can't compress with Zstandard and lz4 - ignoring lz4." << std::endl;
      lz4 = false;
    }

    ICaptureFile *capfile = RENDERDOC_OpenCaptureFile();

    ReplayStatus status = capfile->OpenFile(rdc.c_str(), "", NULL);

    if(status != ReplayStatus::Succeeded)
    {
      capfile->Shutdown();
      std::cerr << "Couldn't load '" << rdc << "': " << ToStr(status) << std::endl;
      return 1;
    }

    if(m_Extract)
    {
      int idx = capfile->FindSectionByName(section.c_str());

      if(idx < 0)
      {
        std::cerr << "'" << rdc << "' has no section called '" << section << "'" << std::endl;
        std::cerr << "Available sections are:" << std::endl;

        int num = capfile->GetSectionCount();

        for(int i = 0; i < num; i++)
          std::cerr << "    " << capfile->GetSectionProperties(i).name.c_str() << std::endl;

        capfile->Shutdown();
        return 1;
      }

      FILE *f = NULL;

      if(noclobber)
      {
        bool exists = false;
        f = fopen(file.c_str(), "rb");
        if(f)
        {
          exists = true;
          fclose(f);
          f = NULL;
        }

        if(exists)
        {
          capfile->Shutdown();
          std::cerr << "Refusing to overwrite '" << file << "'" << std::endl;
          return 1;
        }
      }

      f = fopen(file.c_str(), "wb");

      if(!f)
      {
        capfile->Shutdown();
        std::cerr << "Couldn't open destination file '" << file << "'" << std::endl;
        return 1;
      }
      else
      {
        bytebuf blob = capfile->GetSectionContents(idx);

        capfile->Shutdown();

        fwrite(blob.data(), 1, blob.size(), f);
        fclose(f);

        std::cout << "Wrote '" << section << "' from '" << rdc << "' to '" << file << "'."
                  << std::endl;
      }
    }
    else    // insert/embed
    {
      int idx = capfile->FindSectionByName(section.c_str());

      if(idx >= 0)
      {
        if(noclobber)
        {
          capfile->Shutdown();
          std::cerr << "Refusing to overwrite section '" << section << "' in '" << rdc << "'"
                    << std::endl;
          return 1;
        }
        else
        {
          std::cout << "Overwriting section '" << section << "' in '" << rdc << "'" << std::endl;
        }
      }

      FILE *f = fopen(file.c_str(), "rb");

      if(!f)
      {
        capfile->Shutdown();
        std::cerr << "Couldn't open source file '" << file << "'" << std::endl;
        return 1;
      }

      bytebuf blob;

      fseek(f, 0, SEEK_END);
      int len = ftell(f);
      fseek(f, 0, SEEK_SET);

      if(len < 0)
      {
        len = 0;
        std::cerr << "I/O error reading from '" << file << "'" << std::endl;
      }

      blob.resize((size_t)len);
      size_t read = fread(blob.data(), 1, (size_t)len, f);

      if(read != (size_t)len)
        std::cerr << "I/O error reading from '" << file << "'" << std::endl;

      fclose(f);

      SectionProperties props;
      props.name = section;

      for(SectionType s : values<SectionType>())
      {
        if(ToStr(s) == section)
        {
          props.type = s;
          break;
        }
      }

      if(zstd)
        props.flags |= SectionFlags::ZstdCompressed;
      if(lz4)
        props.flags |= SectionFlags::LZ4Compressed;

      capfile->WriteSection(props, blob);

      capfile->Shutdown();

      std::cout << "Wrote '" << section << "' from '" << file << "' to '" << rdc << "'." << std::endl;
    }

    return 0;
  }
};

REPLAY_PROGRAM_MARKER()

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
    add_command("test", new TestCommand(env));
    add_command("convert", new ConvertCommand(env));
    add_command("embed", new EmbeddedSectionCommand(env, false));
    add_command("extract", new EmbeddedSectionCommand(env, true));

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
      cmd.add<std::string>("working-dir", 'd',
                           "Set the working directory of the program, if launched.", false);
      cmd.add<std::string>("capture-file", 'c',
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
      cmd.add("opt-verify-buffer-access", 0,
              "Capturing Option: Verify any writes to mapped buffers, by bounds checking, and "
              "initialise buffers to invalid value if uninitialised.");
      cmd.add("opt-hook-children", 0,
              "Capturing Option: Hooks any system API calls that create child processes.");
      cmd.add("opt-ref-all-resources", 0,
              "Capturing Option: Include all live resources, not just those used by a frame.");
      cmd.add("opt-capture-all-cmd-lists", 0,
              "Capturing Option: In D3D11, record all command lists from application start.");
    }

    cmd.parse_check(argv, true);

    CaptureOptions opts;
    RENDERDOC_GetDefaultCaptureOptions(&opts);

    if(it->second->IsCaptureCommand())
    {
      if(cmd.exist("opt-disallow-vsync"))
        opts.allowVSync = false;
      if(cmd.exist("opt-disallow-fullscreen"))
        opts.allowFullscreen = false;
      if(cmd.exist("opt-api-validation"))
        opts.apiValidation = true;
      if(cmd.exist("opt-api-validation-unmute"))
        opts.debugOutputMute = false;
      if(cmd.exist("opt-capture-callstacks"))
        opts.captureCallstacks = true;
      if(cmd.exist("opt-capture-callstacks-only-draws"))
        opts.captureCallstacksOnlyDraws = true;
      if(cmd.exist("opt-verify-buffer-access"))
        opts.verifyBufferAccess = true;
      if(cmd.exist("opt-hook-children"))
        opts.hookIntoChildren = true;
      if(cmd.exist("opt-ref-all-resources"))
        opts.refAllResources = true;
      if(cmd.exist("opt-capture-all-cmd-lists"))
        opts.captureAllCmdLists = true;

      opts.delayForDebugger = (uint32_t)cmd.get<int>("opt-delay-for-debugger");
    }

    if(!it->second->HandlesUsageManually() && cmd.exist("help"))
    {
      std::cerr << cmd.usage() << std::endl;
      clean_up();
      return 0;
    }

    int ret = it->second->Execute(cmd, opts);
    clean_up();
    return ret;
  }
  catch(std::exception &e)
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