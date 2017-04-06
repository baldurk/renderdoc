
#include "QRDInterface.h"
#include <QDir>
#include <QStandardPaths>
#include "Code/QRDUtils.h"

QString EnvironmentModification::GetTypeString() const
{
  QString ret;

  if(type == EnvMod::Append)
    ret = QString("Append, %1").arg(ToQStr(separator));
  else if(type == EnvMod::Prepend)
    ret = QString("Prepend, %1").arg(ToQStr(separator));
  else
    ret = "Set";

  return ret;
}

QString EnvironmentModification::GetDescription() const
{
  QString ret;

  if(type == EnvMod::Append)
    ret = QString("Append %1 with %2 using %3").arg(variable).arg(value).arg(ToQStr(separator));
  else if(type == EnvMod::Prepend)
    ret = QString("Prepend %1 with %2 using %3").arg(variable).arg(value).arg(ToQStr(separator));
  else
    ret = QString("Set %1 to %2").arg(variable).arg(value);

  return ret;
}

EnvironmentModification::operator QVariant() const
{
  QVariantMap ret;
  ret["variable"] = variable;
  ret["value"] = value;
  ret["type"] = ToQStr(type);
  ret["separator"] = ToQStr(separator);
  return ret;
}

EnvironmentModification::EnvironmentModification(const QVariant &v)
{
  QVariantMap data = v.toMap();
  variable = data["variable"].toString();
  value = data["value"].toString();

  QString t = data["type"].toString();

  if(t == ToQStr(EnvMod::Append))
    type = EnvMod::Append;
  else if(t == ToQStr(EnvMod::Prepend))
    type = EnvMod::Prepend;
  else
    type = EnvMod::Set;

  QString s = data["separator"].toString();

  if(s == ToQStr(EnvSep::SemiColon))
    separator = EnvSep::SemiColon;
  else if(s == ToQStr(EnvSep::Colon))
    separator = EnvSep::Colon;
  else if(s == ToQStr(EnvSep::Platform))
    separator = EnvSep::Platform;
  else
    separator = EnvSep::NoSep;
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
    env.push_back((QVariant)Environment[i]);
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
    EnvironmentModification e(env[i]);
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
