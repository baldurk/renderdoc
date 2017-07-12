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

// do not include any headers here, they must all be in QRDInterface.h
#include "QRDInterface.h"
#include "RemoteHost.h"

typedef QMap<QString, QString> QStringMap;

DOCUMENT("Describes an external program that can be used to disassemble SPIR-V.");
struct SPIRVDisassembler
{
  SPIRVDisassembler() {}
  VARIANT_CAST(SPIRVDisassembler);

  DOCUMENT("The human-readable name of the program.");
  QString name;
  DOCUMENT("The path to the executable to run for this program.");
  QString executable;
  DOCUMENT("The command line argmuents to pass to the program.");
  QString args;
};

DECLARE_REFLECTION_STRUCT(SPIRVDisassembler);

#define CONFIG_SETTING_VAL(access, variantType, type, name, defaultValue) \
  access:                                                                 \
  type name = defaultValue;
#define CONFIG_SETTING(access, variantType, type, name) \
  access:                                               \
  type name;

// Since this macro is already complex enough, the documentation for each of these members is
// in the docstring for PersistantConfig as :data: members.
// Please keep that docstring up to date when you add/remove/change these config settings.
// Note that only public properties should be documented.
#define CONFIG_SETTINGS()                                                                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastLogPath, QString())                     \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, QList<QString>, RecentLogFiles)                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastCapturePath, QString())                 \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, LastCaptureExe, QString())                  \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, QList<QString>, RecentCaptureSettings)              \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, TemporaryCaptureDirectory, QString())       \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, DefaultCaptureSaveDirectory, QString())     \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_ResetRange, false)                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexSettings, true)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, ShaderViewer_FriendlyNaming, true)                \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, AlwaysReplayLocally, false)                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, LocalProxyAPI, -1)                                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, TimeUnit, EventBrowser_TimeUnit, TimeUnit::Microseconds) \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_AddFake, true)                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_HideEmpty, false)                    \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_HideAPICalls, false)                 \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ApplyColors, true)                   \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ColorEventRow, true)                 \
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
  CONFIG_SETTING_VAL(public, QString, QString, Android_AdbExecutablePath, QString())       \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Android_MaxConnectTimeout, 30)                      \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_AllowChecks, true)                    \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_UpdateAvailable, false)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, QString, CheckUpdate_UpdateResponse, QString())      \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, QDateTime, CheckUpdate_LastUpdate,                 \
                     QDateTime(QDate(2012, 06, 27), QTime(0, 0, 0)))                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, QDateTime, DegradedLog_LastUpdate,                 \
                     QDateTime(QDate(2015, 01, 01), QTime(0, 0, 0)))                       \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Tips_HasSeenFirst, false)                         \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, AllowGlobalHook, false)                           \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, QList<SPIRVDisassembler>, SPIRVDisassemblers)       \
                                                                                           \
  CONFIG_SETTING(private, QVariantMap, QStringMap, ConfigSettings)                         \
                                                                                           \
  CONFIG_SETTING(private, QVariantList, QList<RemoteHost>, RemoteHostList)

DOCUMENT(R"(The unit that GPU durations are displayed in.

.. data:: Seconds

  The durations are displayed as seconds (s).

.. data:: Milliseconds

  The durations are displayed as milliseconds (ms).

.. data:: Microseconds

  The durations are displayed as microseconds (µs).

.. data:: Nanoseconds

  The durations are displayed as nanoseconds (ns).
)");
enum class TimeUnit : int
{
  Seconds = 0,
  Milliseconds,
  Microseconds,
  Nanoseconds,
  Count,
};

DOCUMENT(R"(Gets the suffix for a time unit.

:return: The one or two character suffix.
:rtype: ``str``
)");
inline QString UnitSuffix(TimeUnit unit)
{
  if(unit == TimeUnit::Seconds)
    return lit("s");
  else if(unit == TimeUnit::Milliseconds)
    return lit("ms");
  else if(unit == TimeUnit::Microseconds)
    // without a BOM in the file, this will break using lit() in MSVC
    return QString::fromUtf8("µs");
  else if(unit == TimeUnit::Nanoseconds)
    return lit("ns");

  return lit("s");
}

DOCUMENT(R"(Checks if a given file is in a list. If it is, then it's shuffled to the end. If it's
not then it's added to the end and an item is dropped from the front of the list if necessary to
stay within a given maximum

As the name suggests, this is used for tracking a 'recent file' list.

:param list recentList: A ``list`` of ``str`` that is mutated by the function.
:param str file: The file to add to the list.
:param int maxItems: The maximum allowed length of the list.
)");
void AddRecentFile(QList<QString> &recentList, const QString &file, int maxItems);

DOCUMENT(R"(A persistant config file that is automatically loaded and saved, which contains any
settings and information that needs to be preserved from one run to the next.

For more information about some of these settings that are user-facing see
:ref:`the documentation for the settings window <settings-window>`.

.. data:: LastLogPath

  The path to the last capture to be opened, which is useful as a default location for browsing.

.. data:: RecentLogFiles

  A ``list`` of ``str`` with the recently opened capture files.

.. data:: LastCapturePath

  The path containing the last executable that was captured, which is useful as a default location
  for browsing.

.. data:: LastCaptureExe

  The path to the last executable that was captured, inside :data:`LastCapturePath`.

.. data:: RecentCaptureSettings

  A ``list`` of ``str`` with the recently opened capture settings files.

.. data:: TemporaryCaptureDirectory

  The path to where temporary capture files should be stored until they're saved permanently.

.. data:: DefaultCaptureSaveDirectory

  The default path to save captures in, when browsing to save a temporary capture somewhere.

.. data:: TextureViewer_ResetRange

  ``True`` if the :class:`TextureViewer` should reset the visible range when a new texture is
  selected.

  Defaults to ``False``.

.. data:: TextureViewer_PerTexSettings

  ``True`` if the :class:`TextureViewer` should store most visualisation settings on a per-texture
  basis instead of keeping it persistent across different textures.

  Defaults to ``True``.

.. data:: ShaderViewer_FriendlyNaming

  ``True`` if the :class:`ShaderViewer` should replace register names with the high-level language
  variable names where possible.

  Defaults to ``True``.

.. data:: AlwaysReplayLocally

  ``True`` if when loading a capture that was originally captured on a remote device but uses an
  API that can be supported locally, should be loaded locally without prompting to switch to a
  remote context.

  Defaults to ``False``.

.. data:: LocalProxyAPI

  The index of the local proxy API to use when using remote context replay. ``-1`` if the default
  proxy should be used.

  Defaults to ``-1``.

.. data:: EventBrowser_TimeUnit

  The :class:`TimeUnit` to use to display the duration column in the :class:`EventBrowser`.

  Defaults to microseconds.

.. data:: EventBrowser_AddFake

  ``True`` if fake drawcall marker regions should be added to captures that don't have any markers,
  for easier browsing. The regions are identified by grouping drawcalls that write to the same
  targets together.

  Defaults to ``True``.

.. data:: EventBrowser_HideEmpty

  ``True`` if the :class:`EventBrowser` should hide marker regions that don't contain any actual
  non-marker events.

  Defaults to ``False``.

.. data:: EventBrowser_HideAPICalls

  ``True`` if the :class:`EventBrowser` should hide marker regions that don't contain any events
  that aren't just drawcalls (this will hide events under "API Events" faux-markers).

  Defaults to ``False``.

.. data:: EventBrowser_ApplyColors

  ``True`` if the :class:`EventBrowser` should apply any colours specified with API marker regions.

  Defaults to ``True``.

.. data:: EventBrowser_ColorEventRow

  ``True`` if when colouring marker regions in the :class:`EventBrowser`, the whole row should be
  coloured instead of just a side-bar.

  Defaults to ``True``.

.. data:: Formatter_MinFigures

  The minimum number of significant figures to show in formatted floating point values.

  Defaults to ``2``.

.. data:: Formatter_MaxFigures

  The maximum number of significant figures to show in formatted floating point values.

  Defaults to ``5``.

.. data:: Formatter_NegExp

  The cut-off on negative exponents of a normalised values to display using scientific notation.

  E.g. for a value of 5, anything below 1.0e-5 will be displayed using scientific notation.

  Defaults to ``5``.

.. data:: Formatter_PosExp

  The cut-off on the exponent of a normalised values to display using scientific notation.

  E.g. for a value of 7, anything above 1.0e+7 will be displayed using scientific notation.

  Defaults to ``7``.

.. data:: Font_PreferMonospaced

  ``True`` if a monospaced font should be used in all places where data is displayed, even if the
  data is not tabular such as names.

  Defaults to ``False``.

.. data:: Android_AdbExecutablePath

  The path to the ``adb`` executable to use for Android interaction.

.. data:: Android_MaxConnectTimeout

  The maximum timeout in seconds to wait when launching an Android package.

  Defaults to ``30``.

.. data:: CheckUpdate_AllowChecks

  ``True`` if when colouring marker regions in the :class:`EventBrowser`, the whole row should be
  coloured instead of just a side-bar.

  Defaults to ``True``.

.. data:: CheckUpdate_UpdateAvailable

  ``True`` if an update to a newer version is currently available.

  Defaults to ``False``.

.. data:: CheckUpdate_UpdateResponse

  Contains the response from the update server from the last update check, with any release notes
  for the new version.

.. data:: CheckUpdate_LastUpdate

  A date containing the last time that update checks happened.

.. data:: DegradedLog_LastUpdate

  A date containing the last time that the user was warned about captures being loaded in degraded
  support. This prevents the user being spammed if their hardware is low spec.

.. data:: Tips_HasSeenFirst

  ``True`` if the user has seen the first tip, which should always be shown first before
  randomising.

  Defaults to ``False``.

.. data:: AllowGlobalHook

  ``True`` if global hooking is enabled. Since it has potentially problematic side-effects and is
  dangerous, it requires explicit opt-in.

  Defaults to ``False``.

.. data:: SPIRVDisassemblers

  A list of :class:`SPIRVDisassembler` detailing the potential disassembler programs. The first one
  in the list is the default.

.. data:: RemoteHosts

  A ``list`` of :class:`RemoteHost` handles to the currently configured remote hosts.
)");
class PersistantConfig
{
public:
  // Runtime list of dynamically allocated hosts.
  // Saved to/from private RemoteHostList in CONFIG_SETTINGS()
  // This is documented above in the docstring, similar to the values in CONFIG_SETTINGS()
  DOCUMENT("");
  QList<RemoteHost *> RemoteHosts;

  DOCUMENT("If configured, queries ``adb`` to add android hosts to :data:`RemoteHosts`.");
  void AddAndroidHosts();

  DOCUMENT("");
  CONFIG_SETTINGS()

public:
  PersistantConfig() {}
  ~PersistantConfig();

  DOCUMENT(R"(Loads the config from a given filename. This happens automatically on startup, so it's
not recommended thattyou call this function manually.

:return: A boolean status if the load was successful.
:rtype: ``bool``
)");
  bool Load(const QString &filename);

  DOCUMENT(R"(Saves the config to disk. This can happen if you want to be sure a setting has been
propagated and will not be forgotten in the case of crash or otherwise unexpected exit.

:return: A boolean status if the save was successful.
:rtype: ``bool``
)");
  bool Save();

  DOCUMENT(R"(Closes the config file so that subsequent calls to Save() will not write to disk at
the file the config was loaded from.

This function is rarely directly used, except in the case where RenderDoc is relaunching itself and
wants to avoid file locking conflicts between the closing instance saving, and the loading instance
loading. It can explicitly save and close before relaunching.
)");
  void Close();

  DOCUMENT("Configures the :class:`Formatter` class with the settings from this config.");
  void SetupFormatting();

  DOCUMENT(R"(Sets an arbitrary dynamic setting similar to a key-value store. This can be used for
storing custom settings to be persisted without needing to modify code.

:param str name: The name of the setting. Any existing setting will be overwritten.
:param str value: The contents of the setting.
)");
  void SetConfigSetting(const QString &name, const QString &value);
  DOCUMENT(R"(Retrieves an arbitrary dynamic setting. See :meth:`SetConfigSetting`.

:param str name: The name of the setting.
:return: The value of the setting, or the empty string if the setting did not exist.
:rtype: ``str``
)");
  QString GetConfigSetting(const QString &name);

private:
  bool Deserialize(const QString &filename);
  bool Serialize(const QString &filename);
  QVariantMap storeValues() const;
  void applyValues(const QVariantMap &values);

  QString m_Filename;
};
