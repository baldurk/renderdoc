/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "Code/QRDUtils.h"
#include "Styles/StyleData.h"
#include "QRDInterface.h"

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
    ret.push_back(make_rdcpair<rdcstr, rdcstr>(k, val[k].toString()));
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
}

void PersistantConfig::AddAndroidHosts()
{
  QMap<rdcstr, RemoteHost *> oldHosts;
  for(int i = RemoteHosts.count() - 1; i >= 0; i--)
  {
    if(RemoteHosts[i]->IsADB())
    {
      RemoteHost *host = RemoteHosts.takeAt(i);
      oldHosts[host->hostname] = host;
    }
  }

  QString androidSDKPath = QFile::exists(Android_SDKPath) ? QString(Android_SDKPath) : QString();

  SetConfigSetting("androidSDKPath", androidSDKPath);

  QString androidJDKPath = QFile::exists(Android_JDKPath) ? QString(Android_JDKPath) : QString();

  SetConfigSetting("androidJDKPath", androidJDKPath);

  SetConfigSetting("MaxConnectTimeout", QString::number(Android_MaxConnectTimeout));

  SetConfigSetting(lit("Android_AutoPushLayerToApp"),
                   Android_AutoPushLayerToApp ? lit("1") : lit("0"));

  rdcstr androidHosts;
  RENDERDOC_EnumerateAndroidDevices(&androidHosts);
  for(const QString &hostName :
      QString(androidHosts).split(QLatin1Char(','), QString::SkipEmptyParts))
  {
    RemoteHost *host = NULL;

    if(oldHosts.contains(hostName))
      host = oldHosts.take(hostName);
    else
      host = new RemoteHost();

    host->hostname = hostName;
    rdcstr friendly;
    RENDERDOC_GetAndroidFriendlyName(hostName.toUtf8().data(), friendly);
    host->friendlyName = friendly;
    // Just a command to display in the GUI and allow Launch() to be called.
    host->runCommand = lit("org.renderdoc.renderdoccmd");
    RemoteHosts.push_back(host);
  }

  // delete any leftovers
  QMapIterator<rdcstr, RemoteHost *> i(oldHosts);
  while(i.hasNext())
  {
    i.next();
    delete i.value();
  }
}

bool PersistantConfig::SetStyle()
{
  for(int i = 0; i < StyleData::numAvailable; i++)
  {
    if(UIStyle == rdcstr(StyleData::availStyles[i].styleID))
    {
      QApplication::setStyle(StyleData::availStyles[i].creator());
      return true;
    }
  }

  if(UIStyle != "")
    qCritical() << "Unrecognised UI style" << QString(UIStyle);

  return false;
}

PersistantConfig::~PersistantConfig()
{
  for(RemoteHost *h : RemoteHosts)
    delete h;
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

  // localhost should always be available as a remote host
  bool foundLocalhost = false;

  for(RemoteHost host : RemoteHostList)
  {
    RemoteHosts.push_back(new RemoteHost(host));

    if(host.IsLocalhost())
      foundLocalhost = true;
  }

  if(!foundLocalhost)
  {
    RemoteHost *host = new RemoteHost();
    host->hostname = "localhost";
    RemoteHosts.insert(0, host);
  }

  return ret;
}

bool PersistantConfig::Save()
{
  if(m_Filename.isEmpty())
    return true;

  // update serialize list
  RemoteHostList.clear();
  for(RemoteHost *host : RemoteHosts)
    RemoteHostList.push_back(*host);

  RENDERDOC_SetConfigSetting("Disassembly_FriendlyNaming", ShaderViewer_FriendlyNaming ? "1" : "0");

  return Serialize(m_Filename);
}

void PersistantConfig::Close()
{
  m_Filename = QString();
}

void PersistantConfig::SetupFormatting()
{
  Formatter::setParams(*this);
}

void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file, int maxItems)
{
  QDir dir(file);
  QString path = dir.canonicalPath();

  if(!recentList.contains(path))
  {
    recentList.push_back(path);
    if(recentList.count() >= maxItems)
      recentList.erase(0);
  }
  else
  {
    recentList.removeOne(path);
    recentList.push_back(path);
  }
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

SPIRVDisassembler::SPIRVDisassembler(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains(lit("name")))
    name = map[lit("name")].toString();
  if(map.contains(lit("executable")))
    executable = map[lit("executable")].toString();
  if(map.contains(lit("args")))
    args = map[lit("args")].toString();
}

SPIRVDisassembler::operator QVariant() const
{
  QVariantMap map;

  map[lit("name")] = name;
  map[lit("executable")] = executable;
  map[lit("args")] = args;

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

BugReport::operator QVariant() const
{
  QVariantMap map;

  map[lit("reportId")] = reportId;
  map[lit("submitDate")] = submitDate;
  map[lit("checkDate")] = checkDate;
  map[lit("unreadUpdates")] = unreadUpdates;

  return map;
}
