/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include <QJsonDocument>
#include "renderdoc_replay.h"

// helper templates to convert some more complex types to/from appropriate variants
template <typename variantType, typename origType>
variantType convertToVariant(const origType &val)
{
  return variantType(val);
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

template <>
QVariantList convertToVariant(const QList<QString> &val)
{
  QVariantList ret;
  ret.reserve(val.count());
  for(const QString &s : val)
    ret.push_back(s);
  return ret;
}

template <typename origType, typename variantType>
origType convertFromVariant(const variantType &val)
{
  return origType(val);
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

template <>
QList<QString> convertFromVariant(const QVariantList &val)
{
  QList<QString> ret;
  ret.reserve(val.count());
  for(const QVariant &s : val)
    ret.push_back(s.toString());
  return ret;
}

bool PersistantConfig::Deserialize(QString filename)
{
  QFile f(filename);

  // silently allow missing configs
  if(!f.exists())
    return true;

  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray json = f.readAll();

    if(json.isEmpty())
    {
      qCritical() << "Read invalid empty JSON data from file " << f.errorString();
      return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(json);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert file to JSON document";
      return false;
    }

    QVariantMap values = doc.toVariant().toMap();

    if(values.isEmpty() || !values.contains("renderdocConfigData"))
    {
      qCritical() << "Converted config data is invalid or unrecognised";
      return false;
    }

    applyValues(values);

    return true;
  }

  qInfo() << "Couldn't load layout from " << filename << " " << f.errorString();

  return false;
}

bool PersistantConfig::Serialize(QString filename)
{
  QVariantMap values = storeValues();

  // marker that this is indeed a valid config to load from
  values["renderdocConfigData"] = 1;

  QFile f(filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    QJsonDocument doc = QJsonDocument::fromVariant(values);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert config data to JSON document";
      return false;
    }

    QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

    qint64 ret = f.write(jsontext);

    if(ret != jsontext.size())
    {
      qCritical() << "Failed to write JSON data to file: " << ret << " " << f.errorString();
      return false;
    }

    return true;
  }

  qWarning() << "Couldn't write to " << filename << " " << f.errorString();

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

void PersistantConfig::SetupFormatting()
{
  /*
            Formatter.MinFigures = Formatter_MinFigures;
            Formatter.MaxFigures = Formatter_MaxFigures;
            Formatter.ExponentialNegCutoff = Formatter_NegExp;
            Formatter.ExponentialPosCutoff = Formatter_PosExp;

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

void PersistantConfig::SetConfigSetting(QString name, QString value)
{
  ConfigSettings[name] = value;
  RENDERDOC_SetConfigSetting(name.toUtf8().data(), value.toUtf8().data());
}

QString PersistantConfig::GetConfigSetting(QString name)
{
  if(ConfigSettings.contains(name))
    return ConfigSettings[name];

  return "";
}
