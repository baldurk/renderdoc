/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "QRDInterface.h"
#include <QDir>
#include <QStandardPaths>
#include "Code/QRDUtils.h"

QVariant EnvModToVariant(const EnvironmentModification &env)
{
  QVariantMap ret;
  ret[lit("variable")] = env.name;
  ret[lit("value")] = env.value;
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
  inject = false;
  autoStart = false;
  queuedFrameCap = 0;
  numQueuedFrames = 0;
  RENDERDOC_GetDefaultCaptureOptions(&options);
}

CaptureSettings::operator QVariant() const
{
  QVariantMap ret;

  ret[lit("inject")] = inject;
  ret[lit("autoStart")] = autoStart;

  ret[lit("executable")] = executable;
  ret[lit("workingDir")] = workingDir;
  ret[lit("commandLine")] = commandLine;

  QVariantList env;
  for(int i = 0; i < environment.count(); i++)
    env.push_back(EnvModToVariant(environment[i]));
  ret[lit("environment")] = env;

  QVariantMap opts;
  opts[lit("allowVSync")] = options.allowVSync;
  opts[lit("allowFullscreen")] = options.allowFullscreen;
  opts[lit("apiValidation")] = options.apiValidation;
  opts[lit("captureCallstacks")] = options.captureCallstacks;
  opts[lit("captureCallstacksOnlyDraws")] = options.captureCallstacksOnlyActions;
  opts[lit("delayForDebugger")] = options.delayForDebugger;
  opts[lit("verifyBufferAccess")] = options.verifyBufferAccess;
  opts[lit("hookIntoChildren")] = options.hookIntoChildren;
  opts[lit("refAllResources")] = options.refAllResources;
  opts[lit("captureAllCmdLists")] = options.captureAllCmdLists;
  opts[lit("debugOutputMute")] = options.debugOutputMute;
  opts[lit("softMemoryLimit")] = options.softMemoryLimit;
  ret[lit("options")] = opts;

  ret[lit("queuedFrameCap")] = queuedFrameCap;
  ret[lit("numQueuedFrames")] = numQueuedFrames;

  return ret;
}

CaptureSettings::CaptureSettings(const QVariant &v)
{
  QVariantMap data = v.toMap();

  inject = data[lit("inject")].toBool();
  autoStart = data[lit("autoStart")].toBool();

  executable = data[lit("executable")].toString();
  workingDir = data[lit("workingDir")].toString();
  commandLine = data[lit("commandLine")].toString();

  QVariantList env = data[lit("environment")].toList();
  for(int i = 0; i < env.size(); i++)
  {
    EnvironmentModification e = EnvModFromVariant(env[i]);
    environment.push_back(e);
  }

  QVariantMap opts = data[lit("options")].toMap();

  options.allowVSync = opts[lit("allowVSync")].toBool();
  options.allowFullscreen = opts[lit("allowFullscreen")].toBool();
  options.apiValidation = opts[lit("apiValidation")].toBool();
  options.captureCallstacks = opts[lit("captureCallstacks")].toBool();
  options.captureCallstacksOnlyActions = opts[lit("captureCallstacksOnlyDraws")].toBool();
  options.delayForDebugger = opts[lit("delayForDebugger")].toUInt();
  // old name for verifyBufferAccess was verifyMapWrites, so use that as a fallback
  if(opts.contains(lit("verifyBufferAccess")))
    options.verifyBufferAccess = opts[lit("verifyBufferAccess")].toBool();
  else
    options.verifyBufferAccess = opts[lit("verifyMapWrites")].toBool();
  options.hookIntoChildren = opts[lit("hookIntoChildren")].toBool();
  options.refAllResources = opts[lit("refAllResources")].toBool();
  options.captureAllCmdLists = opts[lit("captureAllCmdLists")].toBool();
  options.debugOutputMute = opts[lit("debugOutputMute")].toBool();
  options.softMemoryLimit = opts[lit("softMemoryLimit")].toUInt();

  if(data.contains(lit("queuedFrameCap")))
    queuedFrameCap = data[lit("queuedFrameCap")].toUInt();
  else
    queuedFrameCap = 0;
  if(data.contains(lit("numQueuedFrames")))
    numQueuedFrames = data[lit("numQueuedFrames")].toUInt();
  else
    numQueuedFrames = 0;
}

rdcstr ConfigFilePath(const rdcstr &filename)
{
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  QDir dir(path);
  if(!dir.exists())
    dir.mkdir(lit("."));

  return QDir::cleanPath(dir.absoluteFilePath(filename));
}
