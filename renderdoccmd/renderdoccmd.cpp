/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

rdcstr conv(const std::string &s)
{
  return rdcstr(s.c_str(), s.size());
}

std::string conv(const rdcstr &s)
{
  return std::string(s.begin(), s.end());
}

static int command_usage(std::string command = "");

// normally this is in the renderdoc core library, but it's needed for the 'unknown enum' path,
// so we implement it here using ostringstream. It's not great, but this is a very uncommon path -
// either for invalid values or for when a new enum is added and the code isn't updated
template <>
rdcstr DoStringise(const uint32_t &el)
{
  std::ostringstream oss;
  oss << el;
  return conv(oss.str());
}

inline std::ostream &operator<<(std::ostream &os, rdcstr const &str)
{
  return os << str.c_str();
}

#include <replay/renderdoc_tostr.inl>

bool usingKillSignal = false;
volatile bool killSignal = false;

rdcarray<rdcstr> convertArgs(const std::vector<std::string> &args)
{
  rdcarray<rdcstr> ret;
  ret.reserve(args.size());
  for(size_t i = 0; i < args.size(); i++)
    ret.push_back(conv(args[i]));
  return ret;
}

void DisplayRendererPreview(IReplayController *renderer, uint32_t width, uint32_t height,
                            uint32_t numLoops)
{
  if(renderer == NULL)
    return;

  rdcarray<TextureDescription> texs = renderer->GetTextures();

  TextureDisplay d;
  d.subresource = {0, 0, ~0U};
  d.overlay = DebugOverlay::NoOverlay;
  d.typeCast = CompType::Typeless;
  d.customShaderId = ResourceId();
  d.hdrMultiplier = -1.0f;
  d.linearDisplayAsGamma = true;
  d.flipY = false;
  d.rangeMin = 0.0f;
  d.rangeMax = 1.0f;
  d.scale = 1.0f;
  d.xOffset = 0.0f;
  d.yOffset = 0.0f;
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

  rdcarray<ActionDescription> actions = renderer->GetRootActions();

  ActionDescription *last = NULL;

  if(!actions.empty())
    last = &actions.back();

  while(last && !last->children.empty())
    last = &last->children.back();

  if(last && last->flags & ActionFlags::Present)
  {
    ResourceId id = last->copyDestination;
    if(id != ResourceId())
      d.resourceId = id;
  }

  DisplayRendererPreview(renderer, d, width, height, numLoops);
}

static std::vector<std::string> version_lines;

struct VersionCommand : public Command
{
  VersionCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Print version information"; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual bool Parse(cmdline::parser &, GlobalEnvironment &) { return true; }
  virtual int Execute(const CaptureOptions &)
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
  HelpCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser) {}
  virtual const char *Description() { return "Print this help message"; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual bool Parse(cmdline::parser &, GlobalEnvironment &) { return true; }
  virtual int Execute(const CaptureOptions &)
  {
    command_usage();
    return 0;
  }
};

struct ThumbCommand : public Command
{
private:
  std::string infile;
  std::string outfile;
  std::string format;
  uint32_t maxsize = 0;

public:
  ThumbCommand() : Command() {}
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
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: thumb command requires a capture filename." << std::endl
                << std::endl
                << parser.usage();
      return false;
    }

    infile = rest[0];

    rest.erase(rest.begin());

    parser.set_rest(rest);

    outfile = parser.get<std::string>("out");
    format = parser.get<std::string>("format");
    maxsize = parser.get<uint32_t>("max-size");

    return true;
  }

  virtual int Execute(const CaptureOptions &)
  {
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
    ResultDetails st = file->OpenFile(conv(infile), "rdc", NULL);
    if(st.OK())
    {
      buf = file->GetThumbnail(type, maxsize).data;
    }
    else
    {
      std::cerr << "Couldn't open '" << infile << "': " << st.Message() << std::endl;
    }
    file->Shutdown();

    if(buf.empty())
    {
      std::cerr << "Couldn't fetch the thumbnail in '" << infile << "'" << std::endl;
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

        std::cout << "Wrote thumbnail from '" << infile << "' to '" << outfile << "'." << std::endl;
      }
    }

    return 0;
  }
};

struct CaptureCommand : public Command
{
private:
  std::string executable;
  std::string workingDir;
  std::string cmdLine;
  std::string logFile;
  bool wait_for_exit = false;

public:
  CaptureCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.set_footer("<executable> [program arguments]");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Launches the given executable to capture."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return true; }
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    std::vector<std::string> rest = parser.rest();

    parser.set_rest({});

    if(rest.empty())
    {
      std::cerr << "Error: capture command requires an executable to launch." << std::endl
                << std::endl
                << parser.usage();
      return 0;
    }

    executable = rest[0];
    workingDir = parser.get<std::string>("working-dir");
    logFile = parser.get<std::string>("capture-file");

    for(size_t i = 1; i < rest.size(); i++)
    {
      if(!cmdLine.empty())
        cmdLine += ' ';

      cmdLine += EscapeArgument(rest[i]);
    }

    wait_for_exit = parser.exist("wait-for-exit");

    return true;
  }

  virtual int Execute(const CaptureOptions &opts)
  {
    std::cout << "Launching '" << executable << "'";

    if(!cmdLine.empty())
      std::cout << " with params: " << cmdLine;

    std::cout << std::endl;

    rdcarray<EnvironmentModification> env;

    ExecuteResult res = RENDERDOC_ExecuteAndInject(
        conv(executable), conv(workingDir), conv(cmdLine), env, conv(logFile), opts, wait_for_exit);

    if(res.result.code != ResultCode::Succeeded)
    {
      std::cerr << "Failed to create & inject: " << res.result.Message() << std::endl;
      return (int)res.result.code;
    }

    if(wait_for_exit)
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
private:
  uint32_t PID = 0;
  std::string captureFile;
  bool wait_for_exit = false;

public:
  InjectCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<uint32_t>("PID", 0, "The process ID of the process to inject.", true);
  }
  virtual const char *Description() { return "Injects RenderDoc into a given running process."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return true; }
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    PID = parser.get<uint32_t>("PID");
    captureFile = parser.get<std::string>("capture-file");
    wait_for_exit = parser.exist("wait-for-exit");
    return true;
  }
  virtual int Execute(const CaptureOptions &opts)
  {
    std::cout << "Injecting into PID " << PID << std::endl;

    rdcarray<EnvironmentModification> env;

    ExecuteResult res = RENDERDOC_InjectIntoProcess(PID, env, conv(captureFile), opts, wait_for_exit);

    if(res.result.code != ResultCode::Succeeded)
    {
      std::cerr << "Failed to inject: " << res.result.Message() << std::endl;
      return (int)res.result.code;
    }

    if(wait_for_exit)
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
private:
  std::string host;
  bool daemon = false;
  bool preview = false;
  uint16_t port = 0;

public:
  RemoteServerCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("daemon", 'd', "Go into the background.");
    parser.add<std::string>(
        "host", 'h', "The interface to listen on. By default listens on all interfaces", false, "");
    parser.add("preview", 'v', "Display a preview window when a replay is active.");
    parser.add<uint32_t>(
        "port", 'p',
        "The port to listen on. Default is 0, which listens on RenderDoc's default port.", false, 0);
  }
  virtual const char *Description()
  {
    return "Start up a server listening as a host for remote replays.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &env)
  {
    env.enumerateGPUs = true;
    host = parser.get<std::string>("host");
    daemon = parser.exist("daemon");
    preview = parser.exist("preview");
    port = parser.get<uint32_t>("port") & 0xffff;
    return true;
  }
  virtual int Execute(const CaptureOptions &)
  {
    std::cerr << "Spawning a replay host listening on " << (host.empty() ? "*" : host);
    if(port != 0)
      std::cerr << ":" << port;
    std::cerr << "..." << std::endl;

    if(daemon)
    {
      std::cerr << "Detaching." << std::endl;
      Daemonise();
    }

    usingKillSignal = true;

    // by default have a do-nothing callback that creates no windows
    RENDERDOC_PreviewWindowCallback previewWindow;

    // if the user asked for a preview, then call to the platform-specific preview function
    if(preview)
      previewWindow = &DisplayRemoteServerPreview;

    // OR if the platform-specific preview function always has a window, then return it anyway.
    if(DisplayRemoteServerPreview(false, {}).system != WindowingSystem::Unknown)
      previewWindow = &DisplayRemoteServerPreview;

    RENDERDOC_BecomeRemoteServer(conv(host), port, []() { return killSignal; }, previewWindow);

    std::cerr << std::endl << "Cleaning up from replay hosting." << std::endl;

    return 0;
  }
};

struct ReplayCommand : public Command
{
private:
  std::string filename;
  std::string remote_host;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t loops = 0;

public:
  ReplayCommand() : Command() {}
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
  }
  virtual const char *Description()
  {
    return "Replay a capture and show the backbuffer on a preview window.";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: replay command requires a filename to load." << std::endl
                << std::endl
                << parser.usage();
      return false;
    }

    filename = rest[0];

    rest.erase(rest.begin());

    parser.set_rest(rest);

    if(parser.exist("remote-host"))
      remote_host = parser.get<std::string>("remote-host");

    width = parser.get<uint32_t>("width");
    height = parser.get<uint32_t>("height");
    loops = parser.get<uint32_t>("loops");

    return true;
  }
  virtual int Execute(const CaptureOptions &)
  {
    if(!remote_host.empty())
    {
      std::cout << "Replaying '" << filename << "' on " << remote_host << "." << std::endl;

      IRemoteServer *remote = NULL;
      ResultDetails result = RENDERDOC_CreateRemoteServerConnection(conv(remote_host), &remote);

      if(remote == NULL || result.code != ResultCode::Succeeded)
      {
        std::cerr << "Error: " << result.Message() << " - Couldn't connect to " << remote_host
                  << "." << std::endl;
        std::cerr << "       Have you run renderdoccmd remoteserver on '" << remote_host << "'?"
                  << std::endl;
        return 1;
      }

      std::cerr << "Copying capture file to remote server" << std::endl;

      rdcstr remotePath = remote->CopyCaptureToRemote(conv(filename), NULL);

      IReplayController *renderer = NULL;
      rdctie(result, renderer) = remote->OpenCapture(~0U, remotePath, ReplayOptions(), NULL);

      if(result.OK())
      {
        DisplayRendererPreview(renderer, width, height, loops);

        remote->CloseCapture(renderer);
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "': " << result.Message()
                  << std::endl;
      }

      remote->ShutdownConnection();
    }
    else
    {
      std::cout << "Replaying '" << filename << "' locally.." << std::endl;

      ICaptureFile *file = RENDERDOC_OpenCaptureFile();

      ResultDetails res = file->OpenFile(conv(filename), "rdc", NULL);

      if(res.code != ResultCode::Succeeded)
      {
        std::cerr << "Couldn't load '" << filename << "': " << res.Message() << std::endl;
        return 1;
      }

      IReplayController *renderer = NULL;
      ResultDetails result = {};
      rdctie(result, renderer) = file->OpenCapture(ReplayOptions(), NULL);

      file->Shutdown();

      if(result.OK())
      {
        DisplayRendererPreview(renderer, width, height, loops);

        renderer->Shutdown();
      }
      else
      {
        std::cerr << "Couldn't load and replay '" << filename << "': " << result.Message()
                  << std::endl;
        return 1;
      }
    }
    return 0;
  }
};

struct formats_reader
{
  formats_reader(bool input)
  {
    ICaptureFile *tmp = RENDERDOC_OpenCaptureFile();

    for(const CaptureFileFormat &f : tmp->GetCaptureFileFormats())
    {
      if(!f.openSupported && input)
        continue;

      exts.push_back(conv(f.extension));
      names.push_back(conv(f.name));
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
private:
  rdcarray<CaptureFileFormat> m_Formats;
  bool list_formats = false;
  std::string infile;
  std::string outfile;
  std::string infmt;
  std::string outfmt;

public:
  ConvertCommand() : Command() {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add<std::string>("filename", 'f', "The file to convert from.", false);
    parser.add<std::string>("output", 'o', "The file to convert to.", false);
    parser.add<std::string>("input-format", 'i', "The format of the input file.", false, "",
                            formats_reader(true));
    parser.add<std::string>("convert-format", 'c', "The format of the output file.", false, "",
                            formats_reader(false));
    parser.add("list-formats", '\0', "Print a list of target formats.");
    parser.stop_at_rest(true);
  }
  virtual const char *Description() { return "Convert between capture formats."; }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    list_formats = parser.exist("list-formats");

    if(list_formats)
      return true;

    infile = parser.get<std::string>("filename");
    outfile = parser.get<std::string>("output");

    if(infile.empty())
    {
      std::cerr << "Need an input filename (-f)." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return false;
    }

    if(outfile.empty())
    {
      std::cerr << "Need an output filename (-o)." << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return false;
    }

    infmt = parser.get<std::string>("input-format");
    outfmt = parser.get<std::string>("convert-format");

    return true;
  }

  virtual int Execute(const CaptureOptions &)
  {
    ICaptureFile *tmp = RENDERDOC_OpenCaptureFile();

    m_Formats = tmp->GetCaptureFileFormats();

    tmp->Shutdown();

    if(list_formats)
    {
      std::cout << "Available formats:" << std::endl;
      for(CaptureFileFormat f : m_Formats)
        std::cout << "'" << f.extension << "': " << f.name << std::endl
                  << " * " << f.description << std::endl
                  << std::endl;
      return 0;
    }

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
        if(infile.find(conv("." + f.extension)) != std::string::npos)
        {
          infmt = conv(f.extension);
          break;
        }
      }
    }

    if(infmt.empty())
    {
      std::cerr << "Couldn't guess input format from filename '" << infile << "'." << std::endl
                << std::endl;
      return 1;
    }

    if(outfmt.empty())
    {
      // try to guess the format by looking for the extension in the filename
      for(CaptureFileFormat f : m_Formats)
      {
        if(outfile.find(conv("." + f.extension)) != std::string::npos)
        {
          outfmt = conv(f.extension);
          break;
        }
      }
    }

    if(outfmt.empty())
    {
      std::cerr << "Couldn't guess output format from filename '" << outfile << "'." << std::endl
                << std::endl;
      return 1;
    }

    ICaptureFile *file = RENDERDOC_OpenCaptureFile();

    ResultDetails st = file->OpenFile(conv(infile), conv(infmt), NULL);

    if(st.code != ResultCode::Succeeded)
    {
      std::cerr << "Couldn't load '" << infile << "' as '" << infmt << "': " << st.Message()
                << std::endl;
      return 1;
    }

    st = file->Convert(conv(outfile), conv(outfmt), NULL, NULL);

    if(st.code != ResultCode::Succeeded)
    {
      std::cerr << "Couldn't convert '" << infile << "' to '" << outfile << "' as '" << outfmt
                << "': " << st.Message() << std::endl;
      return 1;
    }

    std::cout << "Converted '" << infile << "' to '" << outfile << "'" << std::endl;

    return 0;
  }
};

struct TestCommand : public Command
{
private:
  std::string mode;
  rdcarray<rdcstr> args;

public:
  TestCommand() : Command() {}
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
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    std::vector<std::string> rest = parser.rest();

    parser.set_rest({});

    if(rest.empty())
    {
      std::cerr << "First argument must specify a test framework" << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return false;
    }

    mode = rest[0];
    rest.erase(rest.begin());

    if(mode != "unit"
#if PYTHON_VERSION_MINOR > 0
       && mode != "functional"
#endif
       )
    {
      std::cerr << "Unsupported test frame work '" << mode << "'" << std::endl << std::endl;
      std::cerr << parser.usage() << std::endl;
      return false;
    }

    if(parser.exist("help"))
      rest.push_back("--help");

    args = convertArgs(rest);

    return true;
  }

  virtual int Execute(const CaptureOptions &)
  {
    if(mode == "unit")
      return RENDERDOC_RunUnitTests("renderdoccmd test unit", args);
#if PYTHON_VERSION_MINOR > 0
    else if(mode == "functional")
      return RENDERDOC_RunFunctionalTests(PYTHON_VERSION_MINOR, args);
#endif

    std::cerr << "Unsupported test frame work '" << mode << "'" << std::endl << std::endl;
    return 1;
  }
};

struct CapAltBitCommand : public Command
{
private:
  CaptureOptions cmdopts;
  rdcarray<EnvironmentModification> env;
  std::string debuglog;
  uint32_t pid;
  std::string capfile;

public:
  CapAltBitCommand() : Command() {}
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
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    cmdopts.DecodeFromString(conv(parser.get<std::string>("capopts")));

    std::vector<std::string> rest = parser.rest();

    parser.set_rest({});

    if(rest.size() % 3 != 0)
    {
      std::cerr << "Invalid generated capaltbit command rest.size() == " << rest.size() << std::endl;
      return false;
    }

    int numEnvs = int(rest.size() / 3);

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

      env.push_back(EnvironmentModification(type, sep, conv(rest[i * 3 + 1]), conv(rest[i * 3 + 2])));
    }

    debuglog = parser.get<std::string>("debuglog");
    pid = parser.get<uint32_t>("pid");
    capfile = parser.get<std::string>("capfile");

    return true;
  }
  virtual int Execute(const CaptureOptions &)
  {
    RENDERDOC_SetDebugLogFile(conv(debuglog));

    ExecuteResult result = RENDERDOC_InjectIntoProcess(pid, env, conv(capfile), cmdopts, false);

    if(result.result.OK())
      return result.ident;

    return (int)result.result.code;
  }
};

struct EmbeddedSectionCommand : public Command
{
private:
  bool m_Extract = false;
  bool list_sections = false;
  std::string rdc;
  std::string file;
  std::string section;
  bool noclobber = false;
  bool lz4 = false;
  bool zstd = false;

public:
  EmbeddedSectionCommand(bool extract) : Command() { m_Extract = extract; }
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
  virtual bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    list_sections = parser.exist("list-sections");

    if(list_sections)
      return true;

    std::vector<std::string> rest = parser.rest();
    if(rest.empty())
    {
      std::cerr << "Error: this command requires a filename to load." << std::endl
                << std::endl
                << parser.usage();
      return false;
    }

    rdc = rest[0];

    rest.erase(rest.begin());

    parser.set_rest(rest);

    file = parser.get<std::string>("file");
    section = parser.get<std::string>("section");
    noclobber = parser.exist("no-clobber");
    lz4 = !m_Extract && parser.exist("lz4");
    zstd = !m_Extract && parser.exist("zstd");

    return true;
  }
  virtual int Execute(const CaptureOptions &)
  {
    if(list_sections)
    {
      std::cout << "Known sections:" << std::endl;
      for(SectionType s : values<SectionType>())
        std::cout << ToStr(s) << std::endl;
      return 0;
    }

    if(zstd && lz4)
    {
      std::cerr << "Can't compress with Zstandard and lz4 - ignoring lz4." << std::endl;
      lz4 = false;
    }

    ICaptureFile *capfile = RENDERDOC_OpenCaptureFile();

    ResultDetails result = capfile->OpenFile(conv(rdc), "", NULL);

    if(result.code != ResultCode::Succeeded)
    {
      capfile->Shutdown();
      std::cerr << "Couldn't load '" << rdc << "': " << result.Message() << std::endl;
      return 1;
    }

    if(m_Extract)
    {
      int idx = capfile->FindSectionByName(conv(section));

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
      int idx = capfile->FindSectionByName(conv(section));

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
      long len = ftell(f);
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
      props.name = conv(section);

      for(SectionType s : values<SectionType>())
      {
        if(ToStr(s) == props.name)
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

struct VulkanRegisterCommand : public Command
{
private:
  bool m_LayerNeedUpdate = false;
  VulkanLayerRegistrationInfo m_Info;
  bool explain = false;
  bool register_layer = false;
  bool user = false;
  bool system = false;

public:
  VulkanRegisterCommand() : Command()
  {
    m_LayerNeedUpdate = RENDERDOC_NeedVulkanLayerRegistration(&m_Info);
  }
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("explain", '\0',
               "Explain what the status of the layer registration is, and how it can be resolved");
    parser.add("register", '\0', "Register RenderDoc's vulkan layer");
    parser.add("user", '\0',
               "Install layer registration at user-local level instead of system-wide");
    parser.add("system", '\0',
               "Install layer registration system-wide (requires admin privileges)");
  }
  virtual const char *Description() { return "Vulkan layer registration needs attention"; }
  virtual bool IsInternalOnly()
  {
    // if the layer is registered and doesn't need an update, don't report this command in help
    return !m_LayerNeedUpdate;
  }
  virtual bool IsCaptureCommand() { return false; }
  bool Parse(cmdline::parser &parser, GlobalEnvironment &)
  {
    explain = parser.exist("explain");
    register_layer = parser.exist("register");
    user = parser.exist("user");
    system = parser.exist("system");

    return true;
  }

  virtual int Execute(const CaptureOptions &)
  {
    if(explain || !register_layer)
    {
      if(m_LayerNeedUpdate)
      {
        if(m_Info.flags & VulkanLayerFlags::Unfixable)
        {
          std::cerr << "** There is an unfixable problem with your vulkan layer configuration.\n\n"
                       "This is most commonly caused by having a distribution-provided package of "
                       "RenderDoc "
                       "installed, which cannot be modified by another build of RenderDoc.\n\n"
                       "Please consult the RenderDoc documentation, or package/distribution "
                       "documentation on "
                       "linux."
                    << std::endl;

          if(m_Info.otherJSONs.size() > 1)
            std::cerr << "Conflicting manifests:\n\n";
          else
            std::cerr << "Conflicting manifest:\n\n";

          for(const rdcstr &j : m_Info.otherJSONs)
            std::cerr << conv(j) << std::endl;

          return 0;
        }

        std::cerr << "*************************************************************************"
                  << std::endl;
        std::cerr << "**          Warning: Vulkan layer not correctly registered.            **"
                  << std::endl;
        std::cerr << std::endl;

        if(m_Info.flags & VulkanLayerFlags::OtherInstallsRegistered)
          std::cerr << " - Non-matching RenderDoc layer(s) are registered." << std::endl;

        if(!(m_Info.flags & VulkanLayerFlags::ThisInstallRegistered))
          std::cerr << " - This build's RenderDoc layer is not registered." << std::endl;

        std::cerr << std::endl;

        std::cerr << " To fix this, the following actions must take place: " << std::endl
                  << std::endl;

        const bool registerAll = bool(m_Info.flags & VulkanLayerFlags::RegisterAll);
        const bool updateAllowed = bool(m_Info.flags & VulkanLayerFlags::UpdateAllowed);

        for(const rdcstr &j : m_Info.otherJSONs)
          std::cerr << (updateAllowed ? " Unregister/update: " : " Unregister: ") << j.c_str()
                    << std::endl;

        if(!(m_Info.flags & VulkanLayerFlags::ThisInstallRegistered))
        {
          if(registerAll)
          {
            for(const rdcstr &j : m_Info.myJSONs)
              std::cerr << (updateAllowed ? " Register/update: " : " Register: ") << j.c_str()
                        << std::endl;
          }
          else
          {
            std::cerr << (updateAllowed ? " Register one of:" : " Register/update one of:")
                      << std::endl;
            for(const rdcstr &j : m_Info.myJSONs)
              std::cerr << "  -- " << j.c_str() << "\n";
          }
        }

        std::cerr << std::endl;

        if(m_Info.flags & VulkanLayerFlags::UserRegisterable)
        {
          std::cerr << " You must choose whether to register at user or system level." << std::endl
                    << std::endl;
          std::cerr
              << " 'vulkanlayer --register --user' will register the layer local to your user."
              << std::endl;
          if(m_Info.flags & VulkanLayerFlags::NeedElevation)
            std::cerr << "  (This requires admin permissions to unregister other installs)"
                      << std::endl;
          else
            std::cerr << " (This does not require admin permission)" << std::endl;
          std::cerr << std::endl;
          std::cerr << " If you want to install system-wide, run 'vulkanlayer --register --system'."
                    << std::endl;
          std::cerr << "  (This requires admin permission)" << std::endl;

          std::cerr << "*************************************************************************"
                    << std::endl;
          std::cerr << std::endl;
        }
        else
        {
          std::cerr << " The layer must be registered at system level, this operation requires\n"
                    << " admin permissions." << std::endl;
          std::cerr << std::endl;
          std::cerr << " Run 'vulkanlayer --register --system' as administrator to register."
                    << std::endl;

          std::cerr << std::endl;
          std::cerr << "*************************************************************************"
                    << std::endl;
          std::cerr << std::endl;
        }
      }
      else
      {
        std::cerr << "The RenderDoc vulkan layer appears to be correctly registered." << std::endl;
      }

      // don't do anything if we're just explaining the situation
      return 0;
    }

    if(!(m_Info.flags & VulkanLayerFlags::UserRegisterable))
    {
      if(user)
      {
        std::cerr << "Vulkan layer cannot be registered at user level." << std::endl;
        return 1;
      }
    }

    if(user && system)
    {
      std::cerr << "Vulkan layer cannot be registered at user and system levels." << std::endl;
      return 1;
    }
    else if(user || system)
    {
      RENDERDOC_UpdateVulkanLayerRegistration(system);

      if(RENDERDOC_NeedVulkanLayerRegistration(NULL))
      {
        std::cerr << "Vulkan layer registration not successful. ";
        if(system)
          std::cerr << "Check that you are running as administrator";
        std::cerr << std::endl;
      }
    }
    else
    {
      std::cerr << "You must select either '--user' or '--system' to choose where to register the "
                   "vulkan layer."
                << std::endl;
      return 1;
    }

    return 0;
  }
};

REPLAY_PROGRAM_MARKER()

VulkanRegisterCommand *vulkan = NULL;

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

static int command_usage(std::string command)
{
  if(!command.empty())
    std::cerr << command << " is not a valid command." << std::endl << std::endl;

  if(vulkan && !vulkan->IsInternalOnly())
    std::cerr << "** NOTE: Vulkan layer registration problem detected.\n"
                 "** Run 'vulkanlayer --explain' for more details"
              << std::endl
              << std::endl;

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

int renderdoccmd(GlobalEnvironment &env, std::vector<std::string> &argv)
{
  // we don't need this in renderdoccmd.
  env.enumerateGPUs = false;

  vulkan = new VulkanRegisterCommand();

  // if vulkan isn't supported, or the layer is fully registered, this command will not be listed
  // in help so it will be invisible
  add_command("vulkanlayer", vulkan);

  try
  {
    // add basic commands, and common aliases
    add_command("version", new VersionCommand());

    add_alias("--version", "version");
    add_alias("-v", "version");
    // for windows
    add_alias("/version", "version");
    add_alias("/v", "version");

    add_command("help", new HelpCommand());

    add_alias("--help", "help");
    add_alias("-h", "help");
    add_alias("-?", "help");

    // for windows
    add_alias("/help", "help");
    add_alias("/h", "help");
    add_alias("/?", "help");

    // add platform agnostic commands
    add_command("thumb", new ThumbCommand());
    add_command("capture", new CaptureCommand());
    add_command("inject", new InjectCommand());
    add_command("remoteserver", new RemoteServerCommand());
    add_command("replay", new ReplayCommand());
    add_command("capaltbit", new CapAltBitCommand());
    add_command("test", new TestCommand());
    add_command("convert", new ConvertCommand());
    add_command("embed", new EmbeddedSectionCommand(false));
    add_command("extract", new EmbeddedSectionCommand(true));

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
      cmd.add("opt-capture-callstacks-only-actions", 0,
              "Capturing Option: When capturing CPU callstacks, only capture them from actions.");
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
      cmd.add<int>("opt-soft-memory-limit", 0,
                   "Capturing Option: Specify a soft memory limit to try to respect.", false, 0,
                   cmdline::range(0, 10000));
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
      if(cmd.exist("opt-capture-callstacks-only-actions"))
        opts.captureCallstacksOnlyActions = true;
      if(cmd.exist("opt-verify-buffer-access"))
        opts.verifyBufferAccess = true;
      if(cmd.exist("opt-hook-children"))
        opts.hookIntoChildren = true;
      if(cmd.exist("opt-ref-all-resources"))
        opts.refAllResources = true;
      if(cmd.exist("opt-capture-all-cmd-lists"))
        opts.captureAllCmdLists = true;

      opts.delayForDebugger = (uint32_t)cmd.get<int>("opt-delay-for-debugger");
      opts.softMemoryLimit = (uint32_t)cmd.get<int>("opt-soft-memory-limit");
    }

    if(!it->second->HandlesUsageManually() && cmd.exist("help"))
    {
      std::cerr << cmd.usage() << std::endl;
      clean_up();
      return 0;
    }

    if(!it->second->Parse(cmd, env))
    {
      clean_up();
      return 1;
    }

    rdcarray<rdcstr> args = convertArgs(cmd.rest());

    args.append(it->second->ReplayArgs());

    RENDERDOC_InitialiseReplay(env, args);

    int ret = it->second->Execute(opts);

    RENDERDOC_ShutdownReplay();

    clean_up();
    return ret;
  }
  catch(std::exception &e)
  {
    fprintf(stderr, "Unexpected exception: %s\n", e.what());

    clean_up();

    return 1;
  }
}

int renderdoccmd(GlobalEnvironment &env, int argc, char **c_argv)
{
  std::vector<std::string> argv;
  argv.resize(argc);
  for(int i = 0; i < argc; i++)
    argv[i] = c_argv[i];

  return renderdoccmd(env, argv);
}
