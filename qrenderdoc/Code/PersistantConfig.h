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

#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

typedef QMap<QString, QString> QStringMap;

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  access:                                                                 \
  type name = defaultValue;
#define CONFIG_SETTING(access, variantType, type, name) \
  access:                                               \
  type name;

#define CONFIG_SETTINGS()                                                                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastLogPath, "")                            \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, QList<QString>, RecentLogFiles)                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastCapturePath, "")                        \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastCaptureExe, "")                         \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, QList<QString>, RecentCaptureSettings)              \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, CallstackLevelSkip, 0)                              \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, TemporaryCaptureDirectory, "")              \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, DefaultCaptureSaveDirectory, "")            \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_ResetRange, false)                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexSettings, true)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, ShaderViewer_FriendlyNaming, true)                \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, AlwaysReplayLocally, false)                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, LocalProxy, 0)                                      \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, TimeUnit, EventBrowser_TimeUnit, TimeUnit::Microseconds) \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_HideEmpty, false)                    \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_HideAPICalls, false)                 \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ApplyColours, true)                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ColourEventRow, true)                \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Formatter_MinFigures, 2)                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Formatter_MaxFigures, 5)                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Formatter_NegExp, 5)                                \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Formatter_PosExp, 7)                                \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Font_PreferMonospaced, false)                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, Android_AdbExecutablePath, "")              \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Android_MaxConnectTimeout, 30)                      \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_AllowChecks, true)                    \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_UpdateAvailable, false)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, CheckUpdate_UpdateResponse, "")             \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, QDateTime, CheckUpdate_LastUpdate,                 \
                     QDateTime(QDate(2012, 06, 27), QTime(0, 0, 0)))                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, QDateTime, DegradedLog_LastUpdate,                 \
                     QDateTime(QDate(2015, 01, 01), QTime(0, 0, 0)))                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Tips_SeenFirst, false)                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, AllowGlobalHook, false)                           \
                                                                                           \
  CONFIG_SETTING(private, QVariantMap, QStringMap, ConfigSettings)

class PersistantConfig
{
public:
  enum class TimeUnit : int
  {
    Seconds = 0,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    Count,
  };

  CONFIG_SETTINGS()

public:
  PersistantConfig() {}
  bool Deserialize(QString filename);
  bool Serialize(QString filename = "");

  void SetupFormatting();

  static QString UnitPrefix(TimeUnit t)
  {
    if(t == TimeUnit::Seconds)
      return "s";
    else if(t == TimeUnit::Milliseconds)
      return "ms";
    else if(t == TimeUnit::Microseconds)
      return "µs";
    else if(t == TimeUnit::Nanoseconds)
      return "ns";

    return "s";
  }

  static void AddRecentFile(QList<QString> &recentList, const QString &file, int maxItems);

  void SetConfigSetting(QString name, QString value);
  QString GetConfigSetting(QString name);

private:
  QVariantMap storeValues() const;
  void applyValues(const QVariantMap &values);

  QString m_Filename;
};
