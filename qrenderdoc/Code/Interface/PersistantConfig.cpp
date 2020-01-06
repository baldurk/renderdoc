/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStandardPaths>
#include "Code/QRDUtils.h"
#include "Styles/StyleData.h"
#include "QRDInterface.h"

template <>
rdcstr DoStringise(const TimeUnit &el)
{
  BEGIN_ENUM_STRINGISE(TimeUnit)
  {
    STRINGISE_ENUM_CLASS(Seconds);
    STRINGISE_ENUM_CLASS(Milliseconds);
    STRINGISE_ENUM_CLASS(Microseconds);
    STRINGISE_ENUM_CLASS(Nanoseconds);
  }
  END_ENUM_STRINGISE();
}

#define JSON_ID "rdocConfigData"
#define JSON_VER 1

// helper templates to convert some more complex types to/from appropriate variants
template <typename variantType, typename origType>
variantType convertToVariant(const origType &val)
{
  return variantType(val);
}

template <typename variantType, typename innerType>
variantType convertToVariant(const rdcarray<innerType> &val)
{
  variantType ret;
  ret.reserve(val.count());
  for(const innerType &s : val)
    ret.push_back(s);
  return ret;
}

template <>
QVariantMap convertToVariant(const rdcstrpairs &val)
{
  QVariantMap ret;
  for(const rdcstrpair &k : val)
  {
    ret[k.first] = k.second;
  }
  return ret;
}

template <typename origType, typename variantType>
origType convertFromVariant(const variantType &val)
{
  return origType(val);
}

template <>
rdcstr convertFromVariant(const QVariant &val)
{
  return val.toString();
}

template <typename listType>
listType convertFromVariant(const QVariantList &val)
{
  listType ret;
  ret.reserve(val.count());
  for(const QVariant &s : val)
    ret.push_back(convertFromVariant<typename listType::value_type>(s));
  return ret;
}

template <>
rdcstrpairs convertFromVariant(const QVariantMap &val)
{
  rdcstrpairs ret;
  for(const QString &k : val.keys())
  {
    ret.push_back({k, val[k].toString()});
  }
  return ret;
}

bool PersistantConfig::Deserialize(const rdcstr &filename)
{
  QFile f(filename);

  m_Filename = filename;

  // silently allow missing configs
  if(!f.exists())
    return true;

  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QVariantMap values;

    bool success = LoadFromJSON(values, f, JSON_ID, JSON_VER);

    if(!success)
      return false;

    applyValues(values);

    return true;
  }

  qInfo() << "Couldn't load layout from " << QString(filename) << " " << f.errorString();

  return false;
}

bool PersistantConfig::Serialize(const rdcstr &filename)
{
  if(!filename.isEmpty())
    m_Filename = filename;

  QVariantMap values = storeValues();

  QFile f(m_Filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return SaveToJSON(values, f, JSON_ID, JSON_VER);

  qWarning() << "Couldn't write to " << QString(m_Filename) << " " << f.errorString();

  return false;
}

QVariantMap PersistantConfig::storeValues() const
{
  QVariantMap ret;

#undef CONFIG_SETTING_VAL
#undef CONFIG_SETTING

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  ret[lit(#name)] = convertToVariant<variantType>(name);
#define CONFIG_SETTING(access, variantType, type, name) \
  ret[lit(#name)] = convertToVariant<variantType>(name);

  CONFIG_SETTINGS()

  return ret;
}

void PersistantConfig::applyValues(const QVariantMap &values)
{
#undef CONFIG_SETTING_VAL
#undef CONFIG_SETTING

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  if(values.contains(lit(#name)))                                         \
    name = convertFromVariant<type>(values[lit(#name)].value<variantType>());
#define CONFIG_SETTING(access, variantType, type, name) \
  if(values.contains(lit(#name)))                       \
    name = convertFromVariant<type>(values[lit(#name)].value<variantType>());

  CONFIG_SETTINGS()

// backwards compatibility code, to apply old values.
#define RENAMED_SETTING(variantType, oldName, newName) \
  if(values.contains(lit(#oldName)))                   \
    newName = convertFromVariant<decltype(newName)>(values[lit(#oldName)].value<variantType>());

  RENAMED_SETTING(QString, LastLogPath, LastCaptureFilePath);
  RENAMED_SETTING(QVariantList, RecentLogFiles, RecentCaptureFiles);
  RENAMED_SETTING(QDateTime, DegradedLog_LastUpdate, DegradedCapture_LastUpdate);
  RENAMED_SETTING(QVariantList, SPIRVDisassemblers, ShaderProcessors);
}

static QMutex RemoteHostLock;

rdcarray<RemoteHost> PersistantConfig::GetRemoteHosts()
{
  QMutexLocker autolock(&RemoteHostLock);
  return RemoteHostList;
}

RemoteHost PersistantConfig::GetRemoteHost(const rdcstr &hostname)
{
  RemoteHost ret;

  {
    QMutexLocker autolock(&RemoteHostLock);
    for(size_t i = 0; i < RemoteHostList.size(); i++)
    {
      if(RemoteHostList[i].Hostname() == hostname)
      {
        ret = RemoteHostList[i];
        break;
      }
    }
  }

  return ret;
}

void PersistantConfig::AddRemoteHost(RemoteHost host)
{
  if(!host.IsValid())
    return;

  QMutexLocker autolock(&RemoteHostLock);

  // don't add duplicates
  for(size_t i = 0; i < RemoteHostList.size(); i++)
  {
    if(RemoteHostList[i] == host)
    {
      RemoteHostList[i] = host;
      return;
    }
  }

  RemoteHostList.push_back(host);
}

void PersistantConfig::RemoveRemoteHost(RemoteHost host)
{
  if(!host.IsValid())
    return;

  QMutexLocker autolock(&RemoteHostLock);

  for(size_t i = 0; i < RemoteHostList.size(); i++)
  {
    if(RemoteHostList[i] == host)
    {
      RemoteHostList.takeAt(i);
      break;
    }
  }
}

void PersistantConfig::UpdateEnumeratedProtocolDevices()
{
  QString androidSDKPath = (!Android_SDKPath.isEmpty() && QFile::exists(Android_SDKPath))
                               ? QString(Android_SDKPath)
                               : QString();

  SetConfigSetting("androidSDKPath", androidSDKPath);

  QString androidJDKPath = (!Android_JDKPath.isEmpty() && QFile::exists(Android_JDKPath))
                               ? QString(Android_JDKPath)
                               : QString();

  SetConfigSetting("androidJDKPath", androidJDKPath);

  SetConfigSetting("MaxConnectTimeout", QString::number(Android_MaxConnectTimeout));

  rdcarray<RemoteHost> enumeratedDevices;

  rdcarray<rdcstr> protocols;
  RENDERDOC_GetSupportedDeviceProtocols(&protocols);

  for(const rdcstr &p : protocols)
  {
    IDeviceProtocolController *protocol = RENDERDOC_GetDeviceProtocolController(p);

    rdcarray<rdcstr> devices = protocol->GetDevices();

    for(const rdcstr &d : devices)
    {
      RemoteHost newhost(protocol->GetProtocolName() + "://" + d);
      enumeratedDevices.push_back(newhost);
    }
  }

  QMutexLocker autolock(&RemoteHostLock);

  QMap<rdcstr, RemoteHost> oldHosts;

  for(int i = RemoteHostList.count() - 1; i >= 0; i--)
  {
    if(RemoteHostList[i].Protocol())
    {
      RemoteHost host = RemoteHostList.takeAt(i);
      oldHosts[host.Hostname()] = host;
    }
  }

  for(RemoteHost host : enumeratedDevices)
  {
    // if we already had this host, use that one.
    if(oldHosts.contains(host.Hostname()))
      host = oldHosts.take(host.Hostname());

    host.SetFriendlyName(host.Protocol()->GetFriendlyName(host.Hostname()));
    // Just a command to display in the GUI and allow Launch() to be called.
    host.SetRunCommand("Automatically handled");
    RemoteHostList.push_back(host);
  }

  // delete any leftovers
  QMutableMapIterator<rdcstr, RemoteHost> i(oldHosts);
  while(i.hasNext())
  {
    i.next();
    i.value().SetShutdown();
  }
}

bool PersistantConfig::SetStyle()
{
  for(int i = 0; i < StyleData::numAvailable; i++)
  {
    if(UIStyle == rdcstr(StyleData::availStyles[i].styleID))
    {
      QStyle *style = StyleData::availStyles[i].creator();
      Formatter::setPalette(style->standardPalette());
      QApplication::setStyle(style);
      return true;
    }
  }

  if(UIStyle != "")
    qCritical() << "Unrecognised UI style" << QString(UIStyle);

  return false;
}

PersistantConfig::~PersistantConfig()
{
}

bool PersistantConfig::Load(const rdcstr &filename)
{
  bool ret = Deserialize(filename);

  // perform some sanitisation to make sure config is always in sensible state
  for(const rdcstrpair &key : ConfigSettings)
  {
    // redundantly set each setting so it is flushed to the core dll
    SetConfigSetting(key.first, key.second);
  }

  RENDERDOC_SetConfigSetting("Disassembly_FriendlyNaming", ShaderViewer_FriendlyNaming ? "1" : "0");
  RENDERDOC_SetConfigSetting("ExternalTool_RGPIntegration", ExternalTool_RGPIntegration ? "1" : "0");

  RDDialog::DefaultBrowsePath = LastFileBrowsePath;

  // localhost should always be available as a remote host
  {
    QMutexLocker autolock(&RemoteHostLock);

    bool foundLocalhost = false;

    rdcarray<RemoteHost> hosts;
    hosts.swap(RemoteHostList);

    for(RemoteHost host : hosts)
    {
      // skip invalid hosts
      if(!host.IsValid())
        continue;

      // backwards compatibility - skip old adb hosts that were adb:
      if(host.Hostname().find("adb:") >= 0 && host.Protocol() == NULL)
        continue;

      RemoteHostList.push_back(host);

      if(host.IsLocalhost())
        foundLocalhost = true;
    }

    if(!foundLocalhost)
    {
      RemoteHost host;
      host.m_hostname = "localhost";
      RemoteHostList.insert(0, host);
    }
  }

  bool tools[arraydim<KnownShaderTool>()] = {};

  // see which known tools are registered
  for(const ShaderProcessingTool &dis : ShaderProcessors)
  {
    // if it's declared
    if(dis.tool != KnownShaderTool::Unknown)
      tools[(size_t)dis.tool] = true;

    for(KnownShaderTool tool : values<KnownShaderTool>())
    {
      if(QString(dis.executable).contains(ToolExecutable(tool)))
        tools[(size_t)tool] = true;
    }
  }

  for(KnownShaderTool tool : values<KnownShaderTool>())
  {
    if(tool == KnownShaderTool::Unknown || tools[(size_t)tool])
      continue;

    QString exe = ToolExecutable(tool);

    if(exe.isEmpty())
      continue;

    // try to find the tool in PATH
    QString path = QStandardPaths::findExecutable(exe);

    if(!path.isEmpty())
    {
      ShaderProcessingTool s;
      s.name = ToQStr(tool);
      // we store just the base name, so when we launch the process it will always find it in PATH,
      // rather than baking in the current PATH result.
      s.executable = exe;
      s.tool = tool;

      ShaderProcessors.push_back(s);

      continue;
    }

    // try to find it in our plugins folder
    QDir appDir(QApplication::applicationDirPath());

    QStringList searchPaths = {appDir.absoluteFilePath(lit("plugins/spirv/"))};

#if defined(Q_OS_WIN64)
    // windows local
    searchPaths << appDir.absoluteFilePath(lit("../../plugins-win64/spirv/"));
#elif defined(Q_OS_WIN32)
    // windows local
    searchPaths << appDir.absoluteFilePath(lit("../../plugins-win32/spirv/"));
#elif defined(Q_OS_LINUX)
    // linux installation
    searchPaths << appDir.absoluteFilePath(lit("../share/renderdoc/plugins/spirv/"));
    // linux local
    searchPaths << appDir.absoluteFilePath(lit("../../plugins-linux64/spirv/"));
#endif

    searchPaths << appDir.absoluteFilePath(lit("../../plugins/"));

    path = QStandardPaths::findExecutable(exe, searchPaths);

    if(!path.isEmpty())
    {
      ShaderProcessingTool s;
      s.name = ToQStr(tool);
      s.executable = path;
      s.tool = tool;

      ShaderProcessors.push_back(s);

      continue;
    }
  }

  // sanitisation pass, if a tool is declared as a known type ensure its inputs/outputs are correct.
  // This is mostly for backwards compatibility with configs from before the inputs/outputs were
  // added.
  for(ShaderProcessingTool &dis : ShaderProcessors)
  {
    if(dis.tool != KnownShaderTool::Unknown)
    {
      dis.input = ToolInput(dis.tool);
      dis.output = ToolOutput(dis.tool);
    }
  }

  return ret;
}

bool PersistantConfig::Save()
{
  if(m_Filename.isEmpty())
    return true;

  RENDERDOC_SetConfigSetting("Disassembly_FriendlyNaming", ShaderViewer_FriendlyNaming ? "1" : "0");
  RENDERDOC_SetConfigSetting("ExternalTool_RGPIntegration", ExternalTool_RGPIntegration ? "1" : "0");

  LastFileBrowsePath = RDDialog::DefaultBrowsePath;

  // truncate the lists to a maximum of 9 items, allow more to exist in memory
  rdcarray<rdcstr> capFiles = RecentCaptureFiles;
  rdcarray<rdcstr> capSettings = RecentCaptureSettings;

  // the oldest items are first, so remove from there
  while(RecentCaptureFiles.count() >= 10)
    RecentCaptureFiles.erase(0);
  while(RecentCaptureSettings.count() >= 10)
    RecentCaptureSettings.erase(0);

  bool ret = Serialize(m_Filename);

  // restore lists
  RecentCaptureFiles = capFiles;
  RecentCaptureSettings = capSettings;

  return ret;
}

void PersistantConfig::Close()
{
  m_Filename = QString();
}

void PersistantConfig::SetupFormatting()
{
  Formatter::setParams(*this);
}

void RemoveRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file)
{
  recentList.removeOne(QDir::cleanPath(file));
}

void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file)
{
  QDir dir(file);
  QString path = dir.canonicalPath();

  if(path.isEmpty())
  {
    qWarning() << "Got empty path from " << QString(file);
    return;
  }

  if(recentList.contains(path))
    recentList.removeOne(path);

  recentList.push_back(path);
}

void PersistantConfig::SetConfigSetting(const rdcstr &name, const rdcstr &value)
{
  if(name.isEmpty())
    return;

  bool found = false;
  for(rdcstrpair &kv : ConfigSettings)
  {
    if(kv.first == name)
    {
      kv.second = value;
      found = true;
      break;
    }
  }

  if(!found)
    ConfigSettings.push_back(make_rdcpair(name, value));
  RENDERDOC_SetConfigSetting(name.data(), value.data());
}

rdcstr PersistantConfig::GetConfigSetting(const rdcstr &name)
{
  for(const rdcstrpair &kv : ConfigSettings)
    if(kv.first == name)
      return kv.second;

  return "";
}

ShaderProcessingTool::ShaderProcessingTool(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains(lit("tool")))
    tool = (KnownShaderTool)map[lit("tool")].toUInt();
  if(map.contains(lit("name")))
    name = map[lit("name")].toString();
  if(map.contains(lit("executable")))
    executable = map[lit("executable")].toString();
  if(map.contains(lit("args")))
  {
    QString a = map[lit("args")].toString();

    // backwards compatibility
    a.replace(lit("{spv_disasm}"), lit("{output_file}"));
    a.replace(lit("{spv_bin}"), lit("{input_file}"));

    args = a;
  }

  if(map.contains(lit("input")))
  {
    input = (ShaderEncoding)map[lit("input")].toUInt();
  }
  else
  {
    // backwards compatibility, it's a SPIR-V disassembler
    input = ShaderEncoding::SPIRV;
  }

  if(map.contains(lit("output")))
  {
    output = (ShaderEncoding)map[lit("output")].toUInt();
  }
  else
  {
    // backwards compatibility, we have to guess, assume GLSL as a sensible default
    output = ShaderEncoding::GLSL;
  }
}

rdcstr ShaderProcessingTool::DefaultArguments() const
{
  if(tool == KnownShaderTool::SPIRV_Cross)
    return "--output {output_file} {input_file} --vulkan-semantics";
  else if(tool == KnownShaderTool::spirv_dis)
    return "--no-color -o {output_file} {input_file}";
  else if(tool == KnownShaderTool::glslangValidatorGLSL)
    return "-g -V -o {output_file} {input_file} -S {glsl_stage4}";
  else if(tool == KnownShaderTool::glslangValidatorHLSL)
    return "-D -g -V -o {output_file} {input_file} -S {glsl_stage4} -e {entry_point}";
  else if(tool == KnownShaderTool::spirv_as)
    return "-o {output_file} {input_file}";
  else if(tool == KnownShaderTool::dxc)
    return "-T {hlsl_stage2}_6_0 -E {entry_point} -Fo {output_file} {input_file} -spirv";

  return args;
}

ShaderProcessingTool::operator QVariant() const
{
  QVariantMap map;

  map[lit("tool")] = (uint32_t)tool;
  map[lit("name")] = name;
  map[lit("executable")] = executable;
  map[lit("args")] = args;
  map[lit("input")] = (uint32_t)input;
  map[lit("output")] = (uint32_t)output;

  return map;
}

BugReport::BugReport(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains(lit("reportId")))
    reportId = map[lit("reportId")].toString();
  if(map.contains(lit("submitDate")))
    submitDate = map[lit("submitDate")].toDateTime();
  if(map.contains(lit("checkDate")))
    checkDate = map[lit("checkDate")].toDateTime();
  if(map.contains(lit("unreadUpdates")))
    unreadUpdates = map[lit("unreadUpdates")].toBool();
}

rdcstr BugReport::URL() const
{
  return lit(BUGREPORT_URL "/report/%1").arg(QString(reportId));
}

BugReport::operator QVariant() const
{
  QVariantMap map;

  map[lit("reportId")] = reportId;
  map[lit("submitDate")] = submitDate;
  map[lit("checkDate")] = checkDate;
  map[lit("unreadUpdates")] = unreadUpdates;

  return map;
}

ReplayOptions::ReplayOptions(const QVariant &var)
{
  QVariantMap map = var.toMap();

  if(map.contains(lit("apiValidation")))
    apiValidation = map[lit("apiValidation")].toBool();
  if(map.contains(lit("forceGPUVendor")))
    forceGPUVendor = (GPUVendor)map[lit("forceGPUVendor")].toUInt();
  if(map.contains(lit("forceGPUDeviceID")))
    forceGPUDeviceID = map[lit("forceGPUDeviceID")].toUInt();
  if(map.contains(lit("forceGPUDriverName")))
    forceGPUDriverName = map[lit("forceGPUDriverName")].toString();
  if(map.contains(lit("optimisation")))
    optimisation = (ReplayOptimisationLevel)map[lit("optimisation")].toUInt();
}

ReplayOptions::operator QVariant() const
{
  QVariantMap map;

  map[lit("apiValidation")] = apiValidation;
  map[lit("forceGPUVendor")] = (uint32_t)forceGPUVendor;
  map[lit("forceGPUDeviceID")] = forceGPUDeviceID;
  map[lit("forceGPUDriverName")] = forceGPUDriverName;
  map[lit("optimisation")] = (uint32_t)optimisation;

  return map;
}
