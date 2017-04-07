
#include "QRDInterface.h"
#include <QDir>
#include <QStandardPaths>
#include "Code/QRDUtils.h"

QVariant EnvModToVariant(const EnvironmentModification &env)
{
  QVariantMap ret;
  ret["variable"] = ToQStr(env.name);
  ret["value"] = ToQStr(env.value);
  ret["type"] = ToQStr(env.mod);
  ret["separator"] = ToQStr(env.sep);
  return ret;
}

EnvironmentModification EnvModFromVariant(const QVariant &v)
{
  QVariantMap data = v.toMap();

  EnvironmentModification ret;

  ret.name = data["variable"].toString().toUtf8().data();
  ret.value = data["value"].toString().toUtf8().data();

  QString t = data["type"].toString();

  if(t == ToQStr(EnvMod::Append))
    ret.mod = EnvMod::Append;
  else if(t == ToQStr(EnvMod::Prepend))
    ret.mod = EnvMod::Prepend;
  else
    ret.mod = EnvMod::Set;

  QString s = data["separator"].toString();

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

  ret["AutoStart"] = AutoStart;

  ret["Executable"] = Executable;
  ret["WorkingDir"] = WorkingDir;
  ret["CmdLine"] = CmdLine;

  QVariantList env;
  for(int i = 0; i < Environment.size(); i++)
    env.push_back(EnvModToVariant(Environment[i]));
  ret["Environment"] = env;

  QVariantMap opts;
  opts["AllowVSync"] = Options.AllowVSync;
  opts["AllowFullscreen"] = Options.AllowFullscreen;
  opts["APIValidation"] = Options.APIValidation;
  opts["CaptureCallstacks"] = Options.CaptureCallstacks;
  opts["CaptureCallstacksOnlyDraws"] = Options.CaptureCallstacksOnlyDraws;
  opts["DelayForDebugger"] = Options.DelayForDebugger;
  opts["VerifyMapWrites"] = Options.VerifyMapWrites;
  opts["HookIntoChildren"] = Options.HookIntoChildren;
  opts["RefAllResources"] = Options.RefAllResources;
  opts["SaveAllInitials"] = Options.SaveAllInitials;
  opts["CaptureAllCmdLists"] = Options.CaptureAllCmdLists;
  opts["DebugOutputMute"] = Options.DebugOutputMute;
  ret["Options"] = opts;

  return ret;
}

CaptureSettings::CaptureSettings(const QVariant &v)
{
  QVariantMap data = v.toMap();

  AutoStart = data["AutoStart"].toBool();

  Executable = data["Executable"].toString();
  WorkingDir = data["WorkingDir"].toString();
  CmdLine = data["CmdLine"].toString();

  QVariantList env = data["Environment"].toList();
  for(int i = 0; i < env.size(); i++)
  {
    EnvironmentModification e = EnvModFromVariant(env[i]);
    Environment.push_back(e);
  }

  QVariantMap opts = data["Options"].toMap();

  Options.AllowVSync = opts["AllowVSync"].toBool();
  Options.AllowFullscreen = opts["AllowFullscreen"].toBool();
  Options.APIValidation = opts["APIValidation"].toBool();
  Options.CaptureCallstacks = opts["CaptureCallstacks"].toBool();
  Options.CaptureCallstacksOnlyDraws = opts["CaptureCallstacksOnlyDraws"].toBool();
  Options.DelayForDebugger = opts["DelayForDebugger"].toUInt();
  Options.VerifyMapWrites = opts["VerifyMapWrites"].toBool();
  Options.HookIntoChildren = opts["HookIntoChildren"].toBool();
  Options.RefAllResources = opts["RefAllResources"].toBool();
  Options.SaveAllInitials = opts["SaveAllInitials"].toBool();
  Options.CaptureAllCmdLists = opts["CaptureAllCmdLists"].toBool();
  Options.DebugOutputMute = opts["DebugOutputMute"].toBool();
}

QString ConfigFilePath(const QString &filename)
{
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  QDir dir(path);
  if(!dir.exists())
    dir.mkdir(".");

  return QDir::cleanPath(dir.absoluteFilePath(filename));
}
