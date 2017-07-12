
#include "QRDInterface.h"
#include <QDir>
#include <QStandardPaths>
#include "Code/QRDUtils.h"

QVariant EnvModToVariant(const EnvironmentModification &env)
{
  QVariantMap ret;
  ret[lit("variable")] = ToQStr(env.name);
  ret[lit("value")] = ToQStr(env.value);
  ret[lit("type")] = ToQStr(env.mod);
  ret[lit("separator")] = ToQStr(env.sep);
  return ret;
}

EnvironmentModification EnvModFromVariant(const QVariant &v)
{
  QVariantMap data = v.toMap();

  EnvironmentModification ret;

  ret.name = data[lit("variable")].toString().toUtf8().data();
  ret.value = data[lit("value")].toString().toUtf8().data();

  QString t = data[lit("type")].toString();

  if(t == ToQStr(EnvMod::Append))
    ret.mod = EnvMod::Append;
  else if(t == ToQStr(EnvMod::Prepend))
    ret.mod = EnvMod::Prepend;
  else
    ret.mod = EnvMod::Set;

  QString s = data[lit("separator")].toString();

  if(s == ToQStr(EnvSep::SemiColon))
    ret.sep = EnvSep::SemiColon;
  else if(s == ToQStr(EnvSep::Colon))
    ret.sep = EnvSep::Colon;
  else if(s == ToQStr(EnvSep::Platform))
    ret.sep = EnvSep::Platform;
  else
    ret.sep = EnvSep::NoSep;

  return ret;
}

CaptureSettings::CaptureSettings()
{
  Inject = false;
  AutoStart = false;
  RENDERDOC_GetDefaultCaptureOptions(&Options);
}

CaptureSettings::operator QVariant() const
{
  QVariantMap ret;

  ret[lit("Inject")] = Inject;
  ret[lit("AutoStart")] = AutoStart;

  ret[lit("Executable")] = Executable;
  ret[lit("WorkingDir")] = WorkingDir;
  ret[lit("CmdLine")] = CmdLine;

  QVariantList env;
  for(int i = 0; i < Environment.size(); i++)
    env.push_back(EnvModToVariant(Environment[i]));
  ret[lit("Environment")] = env;

  QVariantMap opts;
  opts[lit("AllowVSync")] = Options.AllowVSync;
  opts[lit("AllowFullscreen")] = Options.AllowFullscreen;
  opts[lit("APIValidation")] = Options.APIValidation;
  opts[lit("CaptureCallstacks")] = Options.CaptureCallstacks;
  opts[lit("CaptureCallstacksOnlyDraws")] = Options.CaptureCallstacksOnlyDraws;
  opts[lit("DelayForDebugger")] = Options.DelayForDebugger;
  opts[lit("VerifyMapWrites")] = Options.VerifyMapWrites;
  opts[lit("HookIntoChildren")] = Options.HookIntoChildren;
  opts[lit("RefAllResources")] = Options.RefAllResources;
  opts[lit("SaveAllInitials")] = Options.SaveAllInitials;
  opts[lit("CaptureAllCmdLists")] = Options.CaptureAllCmdLists;
  opts[lit("DebugOutputMute")] = Options.DebugOutputMute;
  ret[lit("Options")] = opts;

  return ret;
}

CaptureSettings::CaptureSettings(const QVariant &v)
{
  QVariantMap data = v.toMap();

  Inject = data[lit("Inject")].toBool();
  AutoStart = data[lit("AutoStart")].toBool();

  Executable = data[lit("Executable")].toString();
  WorkingDir = data[lit("WorkingDir")].toString();
  CmdLine = data[lit("CmdLine")].toString();

  QVariantList env = data[lit("Environment")].toList();
  for(int i = 0; i < env.size(); i++)
  {
    EnvironmentModification e = EnvModFromVariant(env[i]);
    Environment.push_back(e);
  }

  QVariantMap opts = data[lit("Options")].toMap();

  Options.AllowVSync = opts[lit("AllowVSync")].toBool();
  Options.AllowFullscreen = opts[lit("AllowFullscreen")].toBool();
  Options.APIValidation = opts[lit("APIValidation")].toBool();
  Options.CaptureCallstacks = opts[lit("CaptureCallstacks")].toBool();
  Options.CaptureCallstacksOnlyDraws = opts[lit("CaptureCallstacksOnlyDraws")].toBool();
  Options.DelayForDebugger = opts[lit("DelayForDebugger")].toUInt();
  Options.VerifyMapWrites = opts[lit("VerifyMapWrites")].toBool();
  Options.HookIntoChildren = opts[lit("HookIntoChildren")].toBool();
  Options.RefAllResources = opts[lit("RefAllResources")].toBool();
  Options.SaveAllInitials = opts[lit("SaveAllInitials")].toBool();
  Options.CaptureAllCmdLists = opts[lit("CaptureAllCmdLists")].toBool();
  Options.DebugOutputMute = opts[lit("DebugOutputMute")].toBool();
}

QString ConfigFilePath(const QString &filename)
{
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  QDir dir(path);
  if(!dir.exists())
    dir.mkdir(lit("."));

  return QDir::cleanPath(dir.absoluteFilePath(filename));
}
