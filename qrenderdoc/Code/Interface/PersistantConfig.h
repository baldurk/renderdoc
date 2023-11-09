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

#pragma once

// do not include any headers here, they must all be in QRDInterface.h
#include "QRDInterface.h"
#include "RemoteHost.h"

class QMutex;

DOCUMENT(R"(Contains the output from invoking a :class:`ShaderProcessingTool`, including both the
actual output data desired as well as any stdout/stderr messages.
)");
struct ShaderToolOutput
{
  DOCUMENT("The output log - containing the information about the tool run and any errors.");
  rdcstr log;

  DOCUMENT("The actual output data from the tool");
  bytebuf result;
};

DOCUMENT(R"(Describes an external program that can be used to process shaders, typically either
compiling from a high-level language to a binary format, or decompiling from the binary format to
a high-level language or textual representation.

Commonly used with SPIR-V.
)");
struct ShaderProcessingTool
{
  DOCUMENT("");
  ShaderProcessingTool() = default;
  VARIANT_CAST(ShaderProcessingTool);
  bool operator==(const ShaderProcessingTool &o) const
  {
    return tool == o.tool && name == o.name && executable == o.executable && args == o.args &&
           input == o.input && output == o.output;
  }
  bool operator<(const ShaderProcessingTool &o) const
  {
    if(tool != o.tool)
      return tool < o.tool;
    if(name != o.name)
      return name < o.name;
    if(executable != o.executable)
      return executable < o.executable;
    if(args != o.args)
      return args < o.args;
    if(input != o.input)
      return input < o.input;
    if(output != o.output)
      return output < o.output;
    return false;
  }
  DOCUMENT("The :class:`KnownShaderTool` identifying which known tool this program is.");
  KnownShaderTool tool = KnownShaderTool::Unknown;
  DOCUMENT("The human-readable name of the program.");
  rdcstr name;
  DOCUMENT("The path to the executable to run for this program.");
  rdcstr executable;
  DOCUMENT("The command line argmuents to pass to the program.");
  rdcstr args;
  DOCUMENT("The input that this program expects.");
  ShaderEncoding input = ShaderEncoding::Unknown;
  DOCUMENT("The output that this program provides.");
  ShaderEncoding output = ShaderEncoding::Unknown;

  DOCUMENT(R"(Return the default arguments used when invoking this tool

:return: The arguments specified for this tool.
:rtype: str
)");
  rdcstr DefaultArguments() const;

  DOCUMENT(R"(Runs this program to disassemble a given shader reflection.

:param QWidget window: A handle to the window to use when showing a progress bar or error messages.
:param renderdoc.ShaderReflection shader: The shader to disassemble.
:param str args: arguments to pass to the tool. The default arguments can be obtained using
  :meth:`DefaultArguments` which can then be customised as desired. Passing an empty string uses the
  default arguments.
:return: The result of running the tool.
:rtype: ShaderToolOutput
)");
  ShaderToolOutput DisassembleShader(QWidget *window, const ShaderReflection *shader,
                                     rdcstr args) const;

  DOCUMENT(R"(Runs this program to disassemble a given shader source.

:param QWidget window: A handle to the window to use when showing a progress bar or error messages.
:param str source: The source code, preprocessed into a single file.
:param str entryPoint: The name of the entry point in the shader to compile.
:param renderdoc.ShaderStage stage: The pipeline stage that this shader represents.
:param str spirvVer: The version of SPIR-V in use for this shader, or an empty string for defaults.
  The current version can be obtained from reflection data via the ``@spirver`` compile flag.
:param str args: arguments to pass to the tool. The default arguments can be obtained using
  :meth:`DefaultArguments` which can then be customised as desired. Passing an empty string uses the
  default arguments.
:return: The result of running the tool.
:rtype: ShaderToolOutput
)");
  ShaderToolOutput CompileShader(QWidget *window, rdcstr source, rdcstr entryPoint,
                                 ShaderStage stage, rdcstr spirvVer, rdcstr args) const;

private:
  DOCUMENT("Internal function");
  rdcstr IOArguments() const;
};

DECLARE_REFLECTION_STRUCT(ShaderProcessingTool);

#if !defined(SWIG)
#define BUGREPORT_URL "https://renderdoc.org/bugreporter"
#endif

DOCUMENT("Describes a submitted bug report.");
struct BugReport
{
  DOCUMENT("");
  BugReport() { unreadUpdates = false; }
  VARIANT_CAST(BugReport);
  bool operator==(const BugReport &o) const
  {
    return reportId == o.reportId && submitDate == o.submitDate && checkDate == o.checkDate &&
           unreadUpdates == o.unreadUpdates;
  }
  bool operator<(const BugReport &o) const
  {
    if(reportId != o.reportId)
      return reportId < o.reportId;
    if(submitDate != o.submitDate)
      return submitDate < o.submitDate;
    if(checkDate != o.checkDate)
      return checkDate < o.checkDate;
    if(unreadUpdates != o.unreadUpdates)
      return unreadUpdates < o.unreadUpdates;
    return false;
  }
  DOCUMENT("The private ID of the bug report.");
  rdcstr reportId;
  DOCUMENT("The original date when this bug was submitted.");
  rdcdatetime submitDate;
  DOCUMENT("The last date that we checked for updates.");
  rdcdatetime checkDate;
  DOCUMENT("Unread updates to the bug exist");
  bool unreadUpdates = false;

  DOCUMENT(R"(Gets the URL for this report.

:return: The URL to the report.
:rtype: str
)");
  rdcstr URL() const;
};

DECLARE_REFLECTION_STRUCT(BugReport);

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
#define CONFIG_SETTINGS()                                                                          \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The style to load for the UI. Possible values include 'Native', 'RDLight', 'RDDark'. "      \
      "If empty, the closest of RDLight and RDDark will be chosen, based on the overall "          \
      "light-on-dark or dark-on-light theme of the application native style.");                    \
  CONFIG_SETTING_VAL(public, QString, rdcstr, UIStyle, "")                                         \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The path to the last capture to be opened, which is useful as a default location for "      \
      "browsing.");                                                                                \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCaptureFilePath, "")                             \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The path to the last file browsed to in any dialog. Used as a default location for all "    \
      "file browsers without another explicit default directory (such as opening capture files - " \
      "see :data:`LastCaptureFilePath`).");                                                        \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastFileBrowsePath, "")                              \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The recently opened capture files.\n"                                                       \
      "\n:"                                                                                        \
      "type: List[str]");                                                                          \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, RecentCaptureFiles)                       \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The path containing the last executable that was captured, which is useful as a default "   \
      "location for browsing.");                                                                   \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCapturePath, "")                                 \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The filename of the last executable that was captured, inside :data:`LastCapturePath`.");   \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCaptureExe, "")                                  \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The recently opened capture settings files.\n"                                              \
      "\n:"                                                                                        \
      "type: List[str]");                                                                          \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, RecentCaptureSettings)                    \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The path to where temporary capture files should be stored until they're saved "            \
      "permanently.");                                                                             \
  CONFIG_SETTING_VAL(public, QString, rdcstr, TemporaryCaptureDirectory, "")                       \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The default path to save captures in, when browsing to save a temporary capture "           \
      "somewhere.");                                                                               \
  CONFIG_SETTING_VAL(public, QString, rdcstr, DefaultCaptureSaveDirectory, "")                     \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A :class:`ReplayOptions` containing the configured default replay options to use in most "  \
      "scenarios when no specific options are given.\n"                                            \
      "\n:"                                                                                        \
      "type: renderdoc.ReplayOptions");                                                            \
  CONFIG_SETTING(public, QVariant, ReplayOptions, DefaultReplayOptions)                            \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the :class:`TextureViewer` should reset the visible range when a new texture "  \
      "is selected.\n"                                                                             \
      "\n:"                                                                                        \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_ResetRange, false)                          \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the :class:`TextureViewer` should store most visualisation settings on a "      \
      "per-texture basis instead of keeping it persistent across different textures.\n"            \
      "\n:"                                                                                        \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexSettings, true)                       \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the :class:`TextureViewer` should treat y-flipping as a per-texture state "     \
      "rather than a global toggle.\n"                                                             \
      "\n"                                                                                         \
      "Does nothing if per-texture settings are disabled in general.\n"                            \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexYFlip, false)                         \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "List of the directories containing custom shader files for the Texture Viewer.\n"           \
      "\n:"                                                                                        \
      "type: List[str]");                                                                          \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, TextureViewer_ShaderDirs)                 \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if when loading a capture that was originally captured on a remote device but "    \
      "uses an API that can be supported locally, should be loaded locally without prompting to "  \
      "switch to a remote context.\n"                                                              \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, AlwaysReplayLocally, false)                               \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The index of the local proxy API to use when using remote context replay. ``-1`` if the "   \
      "default proxy should be used.\n"                                                            \
      "\n"                                                                                         \
      "Defaults to ``-1``.");                                                                      \
  CONFIG_SETTING_VAL(public, int, int, LocalProxyAPI, -1)                                          \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A list of strings with saved formats for the buffer formatter. The first line is the "      \
      "name and the rest of the contents are the formats.\n"                                       \
      "\n"                                                                                         \
      ":type: List[str]");                                                                         \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, BufferFormatter_SavedFormats)             \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The :class:`TimeUnit` to use to display the duration column in the "                        \
      ":class:`EventBrowser`.\n"                                                                   \
      "\n"                                                                                         \
      "Defaults to microseconds.");                                                                \
  CONFIG_SETTING_VAL(public, int, TimeUnit, EventBrowser_TimeUnit, TimeUnit::Microseconds)         \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if fake action marker regions should be added to captures that don't have any "    \
      "markers, for easier browsing. The regions are identified by grouping actions that write "   \
      "to the same targets together.\n"                                                            \
      "\n"                                                                                         \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_AddFake, true)                               \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the :class:`EventBrowser` should apply any colors specified with API marker "   \
      "regions.\n"                                                                                 \
      "\n"                                                                                         \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ApplyColors, true)                           \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if when coloring marker regions in the :class:`EventBrowser`, the whole row "      \
      "should be colored instead of just a side-bar.\n"                                            \
      "\n"                                                                                         \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, EventBrowser_ColorEventRow, true)                         \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if when loading a new capture that contains a comments section, the comment "      \
      "viewer will be opened and focussed.\n"                                                      \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, Comments_ShowOnLoad, true)                                \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The minimum number of decimal places to show in formatted floating point values.\n"         \
      "\n"                                                                                         \
      ".. note::\n"                                                                                \
      "  The naming of 'MinFigures' is a historical artifact - this controls the number of "       \
      "  decimal places only, not the number of significant figures.\n"                            \
      "\n"                                                                                         \
      "Defaults to ``2``.");                                                                       \
  CONFIG_SETTING_VAL(public, int, int, Formatter_MinFigures, 2)                                    \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The maximum number of decimal places to show in formatted floating point values.\n"         \
      "\n"                                                                                         \
      ".. note::\n"                                                                                \
      "  The naming of 'MaxFigures' is a historical artifact - this controls the number of "       \
      "  decimal places only, not the number of significant figures.\n"                            \
      "\n"                                                                                         \
      "Defaults to ``5``.");                                                                       \
  CONFIG_SETTING_VAL(public, int, int, Formatter_MaxFigures, 5)                                    \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The cut-off on negative exponents of a normalised values to display using scientific "      \
      "notation.\n"                                                                                \
      "\n"                                                                                         \
      "E.g. for a value of 5, anything below 1.0e-5 will be displayed using scientific "           \
      "notation.\n"                                                                                \
      "\n"                                                                                         \
      "Defaults to ``5``.");                                                                       \
  CONFIG_SETTING_VAL(public, int, int, Formatter_NegExp, 5)                                        \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The cut-off on positive exponents of a normalised values to display using scientific "      \
      "notation.\n"                                                                                \
      "\n"                                                                                         \
      "E.g. for a value of 7, anything below 1.0e+7 will be displayed using scientific "           \
      "notation.\n"                                                                                \
      "\n"                                                                                         \
      "Defaults to ``7``.");                                                                       \
  CONFIG_SETTING_VAL(public, int, int, Formatter_PosExp, 7)                                        \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The formatting mode to use for values marked as Offsets or Sizes.\n"                        \
      "\n"                                                                                         \
      "E.g. Auto: decimal by default and hexadecimal if above a certain threshold, "               \
      "Decimal: always use decimal, Hexadecimal: always use hexadecimal."                          \
      "\n"                                                                                         \
      "Defaults to ``Auto``.");                                                                    \
  CONFIG_SETTING_VAL(public, int, OffsetSizeDisplayMode, Formatter_OffsetSizeDisplayMode,          \
                     OffsetSizeDisplayMode::Auto)                                                  \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The global scale to apply to fonts in the application, expressed as a float.\n"             \
      "\n"                                                                                         \
      "Defaults to ``1.0`` which means 100%.");                                                    \
  CONFIG_SETTING_VAL(public, float, float, Font_GlobalScale, 1.0f)                                 \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The font family to use in the UI.\n"                                                        \
      "\n"                                                                                         \
      "Defaults to an empty string which means to use the system default.");                       \
  CONFIG_SETTING_VAL(public, QString, rdcstr, Font_Family, "")                                     \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The monospaced font family to use in the UI.\n"                                             \
      "\n"                                                                                         \
      "Defaults to an empty string which means to use the system default.");                       \
  CONFIG_SETTING_VAL(public, QString, rdcstr, Font_MonoFamily, "")                                 \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if a monospaced font should be used in all places where data is displayed, even "  \
      "if the data is not tabular such as names.\n"                                                \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, Font_PreferMonospaced, false)                             \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A date containing the last time that the user was warned about an Android device being "    \
      "older than is generally supported. This prevents the user being spammed if they "           \
      "consistently use an old Android device. If it has been more than 3 weeks since the last "   \
      "time an old device was seen, we re-warn the user, but if it's less than 3 weeks we "        \
      "silently update this date so continuous use doesn't nag.");                                 \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, UnsupportedAndroid_LastUpdate,                \
                     rdcdatetime(2015, 01, 01))                                                    \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the UI should be allowed to make update checks remotely to see if a new "       \
      "version is available.\n"                                                                    \
      "\n"                                                                                         \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_AllowChecks, true)                            \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if an update to a newer version is currently available.\n"                         \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_UpdateAvailable, false)                       \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The current version at the time of update checks. Used to determine if a cached pending "   \
      "update is no longer valid because we got updated through some other method.");              \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CheckUpdate_CurrentVersion, "")                      \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "Contains the response from the update server from the last update check, with any release " \
      "notes for the new version.");                                                               \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CheckUpdate_UpdateResponse, "")                      \
                                                                                                   \
  DOCUMENT("A date containing the last time that update checks happened.");                        \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, CheckUpdate_LastUpdate,                       \
                     rdcdatetime(2012, 06, 27))                                                    \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A date containing the last time that the user was warned about captures being loaded in "   \
      "degraded support. This prevents the user being spammed if their hardware is low spec.");    \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, DegradedCapture_LastUpdate,                   \
                     rdcdatetime(2015, 01, 01))                                                    \
                                                                                                   \
  DOCUMENT("The path to the executable of the external Radeon GPU Profiler tool.");                \
  CONFIG_SETTING_VAL(public, QString, rdcstr, ExternalTool_RadeonGPUProfiler, "")                  \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the user has seen the first tip, which should always be shown first before "    \
      "randomising.\n"                                                                             \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, Tips_HasSeenFirst, false)                                 \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if global hooking is enabled. Since it has potentially problematic side-effects "  \
      "and is dangerous, it requires explicit opt-in.\n"                                           \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, AllowGlobalHook, false)                                   \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if process injection is enabled. Since it can often break and is almost always "   \
      "not want users want to do. New users can get confused by it being there and go to it "      \
      "first.\n"                                                                                   \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, AllowProcessInject, false)                                \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A list of :class:`ShaderProcessingTool` detailing shader processing programs. The list "    \
      "comes in priority order, with earlier processors preferred over later ones.\n"              \
      "\n"                                                                                         \
      ":type: List[ShaderProcessingTool]");                                                        \
  CONFIG_SETTING(public, QVariantList, rdcarray<ShaderProcessingTool>, ShaderProcessors)           \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the user has selected to completely opt-out from and disable all analytics "    \
      "collection and reporting.\n"                                                                \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, Analytics_TotalOptOut, false)                             \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the user has remained with analytics turned on, but has chosen to manually "    \
      "check each report that is sent out.\n"                                                      \
      "collection and reporting.\n"                                                                \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, Analytics_ManualCheck, false)                             \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the user has been prompted to enter their email address on a crash report. "    \
      "This really helps find fixes for bugs, so we prompt the user once only if they didn't "     \
      "enter an email. Once the prompt has happened, regardless of the answer this is set to "     \
      "true and remains there forever.\n"                                                          \
      "\n"                                                                                         \
      "Defaults to ``False``.");                                                                   \
  CONFIG_SETTING_VAL(public, bool, bool, CrashReport_EmailNagged, false)                           \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "``True`` if the email address entered in the crash reporter should be remembered for next " \
      "time. If no email is entered then nothing happens (any previous saved email is kept).\n"    \
      "\n"                                                                                         \
      "Defaults to ``True``.");                                                                    \
  CONFIG_SETTING_VAL(public, bool, bool, CrashReport_ShouldRememberEmail, true)                    \
                                                                                                   \
  DOCUMENT("The saved email address for pre-filling out in crash reports.");                       \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CrashReport_EmailAddress, "")                        \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "The last opened capture, to send if any crash is encountered. This is different to the "    \
      "most recent opened file, because it's set before any processing happens (recent files are " \
      "only added to the list when they successfully open), and it's cleared again when the "      \
      "capture is closed.");                                                                       \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CrashReport_LastOpenedCapture, "")                   \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A list of :class:`BugReport` detailing previously submitted bugs that we're watching for "  \
      "updates.\n"                                                                                 \
      "\n"                                                                                         \
      ":type: List[BugReport]");                                                                   \
  CONFIG_SETTING(public, QVariantList, rdcarray<BugReport>, CrashReport_ReportedBugs)              \
                                                                                                   \
  DOCUMENT(                                                                                        \
      "A list of strings with extension packages to always load on startup, without needing "      \
      "manual enabling.\n"                                                                         \
      "\n"                                                                                         \
      ":type: List[str]");                                                                         \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, AlwaysLoad_Extensions)                    \
                                                                                                   \
  DOCUMENT("");                                                                                    \
  CONFIG_SETTING(private, QVariantList, rdcarray<RemoteHost>, RemoteHostList)

DOCUMENT(R"(The formatting mode used when displaying fields marked as Offsets or Sizes.

.. data:: Auto

  The data is displayed as decimal values by default and hexadecimal if above a certain threshold.

.. data:: Decimal

  The data is displayed as decimal values.

.. data:: Hexadecimal

  The data is displayed as hexadecimal values.
)");
enum class OffsetSizeDisplayMode : int
{
  Auto = 0,
  Decimal,
  Hexadecimal,
  Count,
};

DECLARE_REFLECTION_ENUM(OffsetSizeDisplayMode);

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

DECLARE_REFLECTION_ENUM(TimeUnit);

DOCUMENT(R"(Gets the suffix for a time unit.

:param TimeUnit unit: The unit to get a suffix for.
:return: The one or two character suffix.
:rtype: str
)");
inline rdcstr UnitSuffix(TimeUnit unit)
{
  if(unit == TimeUnit::Seconds)
    return "s";
  else if(unit == TimeUnit::Milliseconds)
    return "ms";
  else if(unit == TimeUnit::Microseconds)
    // without a BOM in the file, this will break using lit() in MSVC
    return "µs";
  else if(unit == TimeUnit::Nanoseconds)
    return "ns";

  return "s";
}

DOCUMENT(R"(Checks if a given file is in a list. If it is, then it's shuffled to the end. If it's
not then it's added to the end

As the name suggests, this is used for tracking a 'recent file' list.

:param List[str] recentList: The list that is mutated by the function.
:param str file: The file to add to the list.
)");
void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file);

DOCUMENT(R"(Removes a given file from the list, after normalising the path. If the path isn't
present then the list is not modified.

As the name suggests, this is used for tracking a 'recent file' list.

:param List[str] recentList: The list that is mutated by the function.
:param str file: The file to remove from the list.
)");
void RemoveRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file);

struct LegacyData;

#if !defined(SWIG)
class QVariant;

// not exposed to swig - allow windows to have completely custom persistent storage that aren't
// "settings".
struct CustomPersistentStorage
{
  CustomPersistentStorage(rdcstr name);
  virtual void save(QVariant &v) const = 0;
  virtual void load(const QVariant &v) = 0;
};
#endif

DOCUMENT(R"(A persistant config file that is automatically loaded and saved, which contains any
settings and information that needs to be preserved from one run to the next.

For more information about some of these settings that are user-facing see
:ref:`the documentation for the settings window <settings-window>`.
)");
class PersistantConfig
{
public:
  DOCUMENT(R"(Returns a list of all remote hosts.

:return: The remote host list
:rtype: List[RemoteHost]
)");
  rdcarray<RemoteHost> GetRemoteHosts();
  DOCUMENT(R"(Look up a remote host by hostname.

:param str hostname: The hostname to look up
:return: The remote host for the given hostname, or an invalid ``RemoteHost`` if no such exists.
:rtype: RemoteHost
)");
  RemoteHost GetRemoteHost(const rdcstr &hostname);

  DOCUMENT(R"(Adds a new remote host.

:param RemoteHost host: The remote host to add.
)");
  void AddRemoteHost(RemoteHost host);
  DOCUMENT(R"(Removes an existing remote host.

:param RemoteHost host: The remote host to remove.
)");
  void RemoveRemoteHost(RemoteHost host);
  DOCUMENT("If configured, queries available device protocols to update auto-configured hosts.");
  void UpdateEnumeratedProtocolDevices();

  DOCUMENT("");
  CONFIG_SETTINGS()
public:
  PersistantConfig();
  ~PersistantConfig();

  DOCUMENT(R"(Loads the config from a given filename. This happens automatically on startup, so it's
not recommended that you call this function manually.

:param str filename: The filename to load from
:return: A boolean status if the load was successful.
:rtype: bool
)");
  bool Load(const rdcstr &filename);

  DOCUMENT(R"(Saves the config to disk. This can happen if you want to be sure a setting has been
propagated and will not be forgotten in the case of crash or otherwise unexpected exit.

:return: A boolean status if the save was successful.
:rtype: bool
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

  DOCUMENT(R"(Sets the UI style to the value in :data:`UIStyle`.

Changing the style after the application has started may not properly update everything, so to be
sure the new style is applied, the application should be restarted.

:return: ``True`` if the style was set successfully, ``False`` if there was a problem e.g. the value
  of :data:`UIStyle` was unrecognised or empty.
:rtype: bool
)");
  bool SetStyle();

private:
  bool Deserialize(const rdcstr &filename);
  bool Serialize(const rdcstr &filename);

#if !defined(SWIG_GENERATED)
  QVariantMap storeValues() const;
  void applyValues(const QVariantMap &values);
#endif

  rdcstr m_Filename;

  // legacy storage for config settings that were ported into the core config. We keep them here
  // so we can store them out again, to keep the config the same for a while even if it's unused
  LegacyData *m_Legacy;
};
