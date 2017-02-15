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

#include "PersistantConfig.h"
#include <QDebug>
#include <QDir>
#include "QRDUtils.h"
#include "RemoteHost.h"
#include "renderdoc_replay.h"

#define JSON_ID "rdocConfigData"
#define JSON_VER 1

// helper templates to convert some more complex types to/from appropriate variants
template <typename variantType, typename origType>
variantType convertToVariant(const origType &val)
{
  return variantType(val);
}

template <typename variantType, typename innerType>
variantType convertToVariant(const QList<innerType> &val)
{
  variantType ret;
  ret.reserve(val.count());
  for(const innerType &s : val)
    ret.push_back(s);
  return ret;
}

template <>
QVariantMap convertToVariant(const QStringMap &val)
{
  QVariantMap ret;
  for(const QString &k : val.keys())
  {
    ret[k] = val[k];
  }
  return ret;
}

template <typename origType, typename variantType>
origType convertFromVariant(const variantType &val)
{
  return origType(val);
}

template <>
QString convertFromVariant(const QVariant &val)
{
  return val.toString();
}

template <typename listType>
listType convertFromVariant(const QList<QVariant> &val)
{
  listType ret;
  ret.reserve(val.count());
  for(const QVariant &s : val)
    ret.push_back(convertFromVariant<decltype(ret.value(0))>(s));
  return ret;
}

template <>
QStringMap convertFromVariant(const QVariantMap &val)
{
  QStringMap ret;
  for(const QString &k : val.keys())
  {
    ret[k] = val[k].toString();
  }
  return ret;
}

bool PersistantConfig::Deserialize(const QString &filename)
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

  qInfo() << "Couldn't load layout from " << filename << " " << f.errorString();

  return false;
}

bool PersistantConfig::Serialize(const QString &filename)
{
  if(filename != "")
    m_Filename = filename;

  QVariantMap values = storeValues();

  QFile f(m_Filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return SaveToJSON(values, f, JSON_ID, JSON_VER);

  qWarning() << "Couldn't write to " << m_Filename << " " << f.errorString();

  return false;
}

QVariantMap PersistantConfig::storeValues() const
{
  QVariantMap ret;

#undef CONFIG_SETTING_VAL
#undef CONFIG_SETTING

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  ret[#name] = convertToVariant<variantType>(name);
#define CONFIG_SETTING(access, variantType, type, name) \
  ret[#name] = convertToVariant<variantType>(name);

  CONFIG_SETTINGS()

  return ret;
}

void PersistantConfig::applyValues(const QVariantMap &values)
{
#undef CONFIG_SETTING_VAL
#undef CONFIG_SETTING

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  if(values.contains(#name))                                              \
    name = convertFromVariant<type>(values[#name].value<variantType>());
#define CONFIG_SETTING(access, variantType, type, name) \
  if(values.contains(#name))                            \
    name = convertFromVariant<type>(values[#name].value<variantType>());

  CONFIG_SETTINGS()
}

void PersistantConfig::AddAndroidHosts()
{
  for(int i = RemoteHosts.count() - 1; i >= 0; i--)
  {
    if(RemoteHosts[i]->Hostname.startsWith("adb:"))
      delete RemoteHosts.takeAt(i);
  }

  QString adbExePath = QFile::exists(Android_AdbExecutablePath) ? Android_AdbExecutablePath : "";

  // Set the config setting as it will be reused when we start the remoteserver etc.
  SetConfigSetting("adbExePath", adbExePath);

  if(adbExePath.isEmpty())
    return;    // adb path must be non-empty in the Options dialog.

  rdctype::str androidHosts;
  RENDERDOC_EnumerateAndroidDevices(&androidHosts);
  for(const QString &hostName : ToQStr(androidHosts).split(',', QString::SkipEmptyParts))
  {
    RemoteHost *host = new RemoteHost();
    host->Hostname = "adb:" + hostName;
    RemoteHosts.push_back(host);
  }
}

PersistantConfig::~PersistantConfig()
{
  for(RemoteHost *h : RemoteHosts)
    delete h;
}

bool PersistantConfig::Load(const QString &filename)
{
  bool ret = Deserialize(filename);

  // perform some sanitisation to make sure config is always in sensible state
  for(const QString &key : ConfigSettings.keys())
  {
    // redundantly set each setting so it is flushed to the core dll
    SetConfigSetting(key, ConfigSettings[key]);
  }

  // localhost should always be available as a remote host
  bool foundLocalhost = false;

  for(RemoteHost host : RemoteHostList)
  {
    RemoteHosts.push_back(new RemoteHost(host));

    if(host.Hostname == "localhost")
      foundLocalhost = true;
  }

  if(!foundLocalhost)
  {
    RemoteHost *host = new RemoteHost();
    host->Hostname = "localhost";
    RemoteHosts.insert(RemoteHosts.begin(), host);
  }

  return ret;
}

bool PersistantConfig::Save()
{
  // update serialize list
  RemoteHostList.clear();
  for(RemoteHost *host : RemoteHosts)
    RemoteHostList.push_back(*host);

  return Serialize(m_Filename);
}

void PersistantConfig::SetupFormatting()
{
  Formatter::setParams(Formatter_MinFigures, Formatter_MaxFigures, Formatter_NegExp,
                       Formatter_PosExp);

  // TODO Preferred Font
  /*
            PreferredFont = Font_PreferMonospaced
                ? new System.Drawing.Font("Consolas", 9.25F, System.Drawing.FontStyle.Regular,
     System.Drawing.GraphicsUnit.Point, ((byte)(0)))
                : new System.Drawing.Font("Tahoma", 8.25F);
                */
}

void PersistantConfig::AddRecentFile(QList<QString> &recentList, const QString &file, int maxItems)
{
  QDir dir(file);
  QString path = dir.canonicalPath();

  if(!recentList.contains(path))
  {
    recentList.push_back(path);
    if(recentList.count() >= maxItems)
      recentList.removeAt(0);
  }
  else
  {
    recentList.removeOne(path);
    recentList.push_back(path);
  }
}

void PersistantConfig::SetConfigSetting(const QString &name, const QString &value)
{
  if(name.isEmpty())
    return;

  ConfigSettings[name] = value;
  RENDERDOC_SetConfigSetting(name.toUtf8().data(), value.toUtf8().data());
}

QString PersistantConfig::GetConfigSetting(const QString &name)
{
  if(ConfigSettings.contains(name))
    return ConfigSettings[name];

  return "";
}
