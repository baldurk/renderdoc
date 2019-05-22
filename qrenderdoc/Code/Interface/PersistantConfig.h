/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

DOCUMENT(R"(Identifies a particular known tool used for shader processing.

.. data:: Unknown

  Corresponds to no known tool.

.. data:: SPIRV_Cross

  `SPIRV-Cross <https://github.com/KhronosGroup/SPIRV-Cross>`_.

.. data:: spirv_dis

  `spirv-dis from SPIRV-Tools <https://github.com/KhronosGroup/SPIRV-Tools>`_.

.. data:: glslangValidatorGLSL

  `glslang compiler (GLSL) <https://github.com/KhronosGroup/glslang>`_.

.. data:: glslangValidatorHLSL

  `glslang compiler (HLSL) <https://github.com/KhronosGroup/glslang>`_.

.. data:: spirv_as

  `spirv-as from SPIRV-Tools <https://github.com/KhronosGroup/SPIRV-Tools>`_.

.. data:: dxc

  `DirectX Shader Compiler <https://github.com/microsoft/DirectXShaderCompiler>`_.

)");
enum class KnownShaderTool : uint32_t
{
  Unknown,
  First = Unknown,
  SPIRV_Cross,
  spirv_dis,
  glslangValidatorGLSL,
  glslangValidatorHLSL,
  spirv_as,
  dxc,
  Count,
};

ITERABLE_OPERATORS(KnownShaderTool);

inline rdcstr ToolExecutable(KnownShaderTool tool)
{
  if(tool == KnownShaderTool::SPIRV_Cross)
    return "spirv-cross";
  else if(tool == KnownShaderTool::spirv_dis)
    return "spirv-dis";
  else if(tool == KnownShaderTool::glslangValidatorGLSL)
    return "glslangValidator";
  else if(tool == KnownShaderTool::glslangValidatorHLSL)
    return "glslangValidator";
  else if(tool == KnownShaderTool::spirv_as)
    return "spirv-as";
  else if(tool == KnownShaderTool::dxc)
    return "dxc";

  return "";
}

inline ShaderEncoding ToolInput(KnownShaderTool tool)
{
  if(tool == KnownShaderTool::SPIRV_Cross || tool == KnownShaderTool::spirv_dis)
    return ShaderEncoding::SPIRV;
  else if(tool == KnownShaderTool::glslangValidatorGLSL)
    return ShaderEncoding::GLSL;
  else if(tool == KnownShaderTool::glslangValidatorHLSL)
    return ShaderEncoding::HLSL;
  else if(tool == KnownShaderTool::spirv_as)
    return ShaderEncoding::SPIRVAsm;
  else if(tool == KnownShaderTool::dxc)
    return ShaderEncoding::HLSL;

  return ShaderEncoding::Unknown;
}

inline ShaderEncoding ToolOutput(KnownShaderTool tool)
{
  if(tool == KnownShaderTool::SPIRV_Cross)
    return ShaderEncoding::GLSL;
  else if(tool == KnownShaderTool::spirv_dis)
    return ShaderEncoding::SPIRVAsm;
  else if(tool == KnownShaderTool::glslangValidatorGLSL ||
          tool == KnownShaderTool::glslangValidatorHLSL || tool == KnownShaderTool::spirv_as ||
          tool == KnownShaderTool::dxc)
    return ShaderEncoding::SPIRV;

  return ShaderEncoding::Unknown;
}

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
:rtype: ``str``
)");
  rdcstr DefaultArguments() const;

  DOCUMENT(R"(Runs this program to disassemble a given shader reflection.

:param QWidget window: A handle to the window to use when showing a progress bar or error messages.
:param ~renderdoc.ShaderReflection shader: The shader to disassemble.
:param str args: arguments to pass to the tool. The default arguments can be obtained using
  :meth:`DefaultArguments` which can then be customised as desired. Passing an empty string uses the
  default arguments.
:return: The result of running the tool.
:rtype: ShaderToolOutput
)");
  ShaderToolOutput DisassembleShader(QWidget *window, const ShaderReflection *reflection,
                                     rdcstr args) const;

  DOCUMENT(R"(Runs this program to disassemble a given shader source.

:param QWidget window: A handle to the window to use when showing a progress bar or error messages.
:param str source: The source code, preprocessed into a single file.
:param str entryPoint: The name of the entry point in the shader to compile.
:param ~renderdoc.ShaderStage stage: The pipeline stage that this shader represents.
:param str args: arguments to pass to the tool. The default arguments can be obtained using
  :meth:`DefaultArguments` which can then be customised as desired. Passing an empty string uses the
  default arguments.
:return: The result of running the tool.
:rtype: ShaderToolOutput
)");
  ShaderToolOutput CompileShader(QWidget *window, rdcstr source, rdcstr entryPoint,
                                 ShaderStage stage, rdcstr args) const;
};

DECLARE_REFLECTION_STRUCT(ShaderProcessingTool);

#define BUGREPORT_URL "https://renderdoc.org/bugreporter"

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
:rtype: ``str``
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
#define CONFIG_SETTINGS()                                                                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, UIStyle, "")                                 \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCaptureFilePath, "")                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastFileBrowsePath, "")                      \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, RecentCaptureFiles)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCapturePath, "")                         \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, LastCaptureExe, "")                          \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, RecentCaptureSettings)            \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, TemporaryCaptureDirectory, "")               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, DefaultCaptureSaveDirectory, "")             \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_ResetRange, false)                  \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexSettings, true)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, TextureViewer_PerTexYFlip, false)                 \
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
  CONFIG_SETTING_VAL(public, bool, bool, Comments_ShowOnLoad, true)                        \
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
  CONFIG_SETTING_VAL(public, QString, rdcstr, Android_SDKPath, "")                         \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, Android_JDKPath, "")                         \
                                                                                           \
  CONFIG_SETTING_VAL(public, int, int, Android_MaxConnectTimeout, 30)                      \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, UnsupportedAndroid_LastUpdate,        \
                     rdcdatetime(2015, 01, 01))                                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_AllowChecks, true)                    \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CheckUpdate_UpdateAvailable, false)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CheckUpdate_CurrentVersion, "")              \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CheckUpdate_UpdateResponse, "")              \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, CheckUpdate_LastUpdate,               \
                     rdcdatetime(2012, 06, 27))                                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, QDateTime, rdcdatetime, DegradedCapture_LastUpdate,           \
                     rdcdatetime(2015, 01, 01))                                            \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, ExternalTool_RGPIntegration, false)               \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, ExternalTool_RadeonGPUProfiler, "")          \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Tips_HasSeenFirst, false)                         \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, AllowGlobalHook, false)                           \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, rdcarray<ShaderProcessingTool>, ShaderProcessors)   \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Analytics_TotalOptOut, false)                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, Analytics_ManualCheck, false)                     \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CrashReport_EmailNagged, false)                   \
                                                                                           \
  CONFIG_SETTING_VAL(public, bool, bool, CrashReport_ShouldRememberEmail, true)            \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CrashReport_EmailAddress, "")                \
                                                                                           \
  CONFIG_SETTING_VAL(public, QString, rdcstr, CrashReport_LastOpenedCapture, "")           \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, rdcarray<BugReport>, CrashReport_ReportedBugs)      \
                                                                                           \
  CONFIG_SETTING(public, QVariantList, rdcarray<rdcstr>, AlwaysLoad_Extensions)            \
                                                                                           \
  CONFIG_SETTING(private, QVariantMap, rdcstrpairs, ConfigSettings)                        \
                                                                                           \
  CONFIG_SETTING(private, QVariantList, rdcarray<RemoteHost>, RemoteHostList)

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

:return: The one or two character suffix.
:rtype: ``str``
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
not then it's added to the end and an item is dropped from the front of the list if necessary to
stay within a given maximum

As the name suggests, this is used for tracking a 'recent file' list.

:param list recentList: A ``list`` of ``str`` that is mutated by the function.
:param str file: The file to add to the list.
:param int maxItems: The maximum allowed length of the list.
)");
void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file, int maxItems);

DOCUMENT2(R"(A persistant config file that is automatically loaded and saved, which contains any
settings and information that needs to be preserved from one run to the next.

For more information about some of these settings that are user-facing see
:ref:`the documentation for the settings window <settings-window>`.

.. data:: UIStyle

  The style to load for the UI. Possible values include 'Native', 'RDLight', 'RDDark'. If empty,
  the closest of RDLight and RDDark will be chosen, based on the overall light-on-dark or
  dark-on-light theme of the application native style.

.. data:: LastCaptureFilePath

  The path to the last capture to be opened, which is useful as a default location for browsing.

.. data:: LastFileBrowsePath

  The path to the last file browsed to in any dialog. Used as a default location for all file
  browsers without another explicit default directory (such as opening capture files - see
  :data:`LastCaptureFilePath`).

.. data:: RecentCaptureFiles

  A ``list`` of ``str`` with the recently opened capture files.

.. data:: LastCapturePath

  The path containing the last executable that was captured, which is useful as a default location
  for browsing.

.. data:: LastCaptureExe

  The filename of the last executable that was captured, inside :data:`LastCapturePath`.

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

.. data:: TextureViewer_PerTexYFlip

  ``True`` if the :class:`TextureViewer` should treat y-flipping as a per-texture state rather than
  a global toggle.

  Does nothing if per-texture settings are disabled in general.

  Defaults to ``False``.

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

  ``True`` if the :class:`EventBrowser` should apply any colors specified with API marker regions.

  Defaults to ``True``.

.. data:: EventBrowser_ColorEventRow

  ``True`` if when coloring marker regions in the :class:`EventBrowser`, the whole row should be
  colored instead of just a side-bar.

  Defaults to ``True``.
)",
          R"(
.. data:: Comments_ShowOnLoad

  ``True`` if when loading a new capture that contains a comments section, the comment viewer will
  be opened and focussed.

  Defaults to ``False``.

.. data:: Formatter_MinFigures

  The minimum number of decimal places to show in formatted floating point values.

  .. note::

    The naming of 'MinFigures' is a historical artifact - this controls the number of decimal places
    only, not the number of significant figures.

  Defaults to ``2``.

.. data:: Formatter_MaxFigures

  The maximum number of decimal places to show in formatted floating point values.

  .. note::

    The naming of 'MaxFigures' is a historical artifact - this controls the number of decimal places
    only, not the number of significant figures.

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

.. data:: Android_SDKPath

  The path to the root of the android SDK, to locate android tools to use for android remote hosts.

  Defaults to using the tools distributed with RenderDoc.

.. data:: Android_JDKPath

  The path to the root of the Java JDK, to locate java for running android java tools.

  Defaults to using the JAVA_HOME environment variable, if set.

.. data:: Android_MaxConnectTimeout

  The maximum timeout in seconds to wait when launching an Android package.

  Defaults to ``30``.

.. data:: UnsupportedAndroid_LastUpdate

  A date containing the last time that the user was warned about an Android device being older than
  is generally supported. This prevents the user being spammed if they consistently use an old
  Android device. If it has been more than 3 weeks since the last time an old device was seen, we
  re-warn the user, but if it's less than 3 weeks we silently update this date so continuous use
  doesn't nag.

.. data:: CheckUpdate_AllowChecks

  ``True`` if when coloring marker regions in the :class:`EventBrowser`, the whole row should be
  colored instead of just a side-bar.

  Defaults to ``True``.

.. data:: CheckUpdate_UpdateAvailable

  ``True`` if an update to a newer version is currently available.

  Defaults to ``False``.

.. data:: CheckUpdate_CurrentVersion

  The current version at the time of update checks. Used to determine if a cached pending update is
  no longer valid because we got updated through some other method.

.. data:: CheckUpdate_UpdateResponse

  Contains the response from the update server from the last update check, with any release notes
  for the new version.

.. data:: CheckUpdate_LastUpdate

  A date containing the last time that update checks happened.

.. data:: DegradedCapture_LastUpdate

  A date containing the last time that the user was warned about captures being loaded in degraded
  support. This prevents the user being spammed if their hardware is low spec.

.. data:: ExternalTool_RGPIntegration

  Whether to enable integration with the external Radeon GPU Profiler tool.

.. data:: ExternalTool_RadeonGPUProfiler

  The path to the executable of the external Radeon GPU Profiler tool.

.. data:: Tips_HasSeenFirst

  ``True`` if the user has seen the first tip, which should always be shown first before
  randomising.

  Defaults to ``False``.

.. data:: AllowGlobalHook

  ``True`` if global hooking is enabled. Since it has potentially problematic side-effects and is
  dangerous, it requires explicit opt-in.

  Defaults to ``False``.

.. data:: ShaderProcessors

  A list of :class:`ShaderProcessingTool` detailing shader processing programs. The list comes in
  priority order, with earlier processors preferred over later ones.

.. data:: Analytics_TotalOptOut

  ``True`` if the user has selected to completely opt-out from and disable all analytics collection
  and reporting.

  Defaults to ``False``.

.. data:: Analytics_ManualCheck

  ``True`` if the user has remained with analytics turned on, but has chosen to manually check each
  report that is sent out.

  Defaults to ``False``.

.. data:: CrashReport_EmailNagged

  ``True`` if the user has been prompted to enter their email address on a crash report. This really
  helps find fixes for bugs, so we prompt the user once only if they didn't enter an email. Once the
  prompt has happened, regardless of the answer this is set to true and remains there forever.

  Defaults to ``False``.

.. data:: CrashReport_ShouldRememberEmail

  ``True`` if the email address entered in the crash reporter should be remembered for next time. If
  no email is entered then nothing happens (any previous saved email is kept).

  Defaults to ``True``.

.. data:: CrashReport_EmailAddress

  The saved email address for pre-filling out in crash reports.

.. data:: CrashReport_LastOpenedCapture

  The last opened capture, to send if any crash is encountered. This is different to the most recent
  opened file, because it's set before any processing happens (recent files are only added to the
  list when they successfully open), and it's cleared again when the capture is closed.

.. data:: CrashReport_ReportedBugs

  A list of :class:`BugReport` detailing previously submitted bugs that we're watching for updates.

.. data:: AlwaysLoad_Extensions

  A list of strings with extension packages to always load on startup, without needing manual
  enabling.


)");
class PersistantConfig
{
public:
// don't allow SWIG direct access to the RemoteHosts since they're an array of references and our
// bindings can't handle that properly
#if !defined(SWIG)
  // Runtime list of dynamically allocated hosts.
  // Saved to/from private RemoteHostList in CONFIG_SETTINGS()
  // This is documented above in the docstring, similar to the values in CONFIG_SETTINGS()
  // This must only be accessed on the UI thread to prevent races. For access on other threads (e.g.
  // a background/asynchronous update), take a copy on the UI thread, update it in the background,
  // then apply the updates.
  DOCUMENT("");
  rdcarray<RemoteHost *> RemoteHosts;
#endif

  DOCUMENT(R"(Returns the number of remote hosts currently registered.

:return: The number of remote hosts.
:rtype: ``int``
)");
  int RemoteHostCount();
  DOCUMENT(R"(Returns a given remote host at an index.

:param int index: The index of the remote host to retrieve
:return: The remote host specified, or ``None`` if an invalid index was passed
:rtype: ``RemoteHost``
)");
  RemoteHost *GetRemoteHost(int index);

  DOCUMENT(R"(Adds a new remote host.

:param RemoteHost host: The remote host to add.
R)");
  void AddRemoteHost(RemoteHost host);
  DOCUMENT("If configured, queries ``adb`` to add android hosts to :data:`RemoteHosts`.");
  void AddAndroidHosts();

  DOCUMENT("");
  CONFIG_SETTINGS()
public:
  PersistantConfig() {}
  ~PersistantConfig();

  DOCUMENT(R"(Loads the config from a given filename. This happens automatically on startup, so it's
not recommended that you call this function manually.

:param str filename: The filename to load from
:return: A boolean status if the load was successful.
:rtype: ``bool``
)");
  bool Load(const rdcstr &filename);

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
  void SetConfigSetting(const rdcstr &name, const rdcstr &value);
  DOCUMENT(R"(Retrieves an arbitrary dynamic setting. See :meth:`SetConfigSetting`.

:param str name: The name of the setting.
:return: The value of the setting, or the empty string if the setting did not exist.
:rtype: ``str``
)");
  rdcstr GetConfigSetting(const rdcstr &name);

  DOCUMENT(R"(Sets the UI style to the value in :data:`UIStyle`.

Changing the style after the application has started may not properly update everything, so to be
sure the new style is applied, the application should be restarted.

:param str name: The name of the setting.
:return: ``True`` if the style was set successfully, ``False`` if there was a problem e.g. the value
  of :data:`UIStyle` was unrecognised or empty.
:rtype: ``bool``
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
};
