/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <QBuffer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextEdit>
#include <QUrlQuery>
#include <QVBoxLayout>
#include "Code/QRDUtils.h"
#include "Windows/Dialogs/AnalyticsConfirmDialog.h"
#include "Windows/Dialogs/AnalyticsPromptDialog.h"
#include "QRDInterface.h"

#if RENDERDOC_ANALYTICS_ENABLE

template <>
inline rdcliteral TypeName<int32_t>()
{
  return "int"_lit;
}

template <>
inline rdcliteral TypeName<QString>()
{
  return "string"_lit;
}

template <>
inline rdcliteral TypeName<QStringList>()
{
  return "string array"_lit;
}

template <>
inline rdcliteral TypeName<bool>()
{
  return "bool"_lit;
}

template <>
inline rdcliteral TypeName<AnalyticsAverage>()
{
  return "Average"_lit;
}

template <>
inline rdcliteral TypeName<bool[32]>()
{
  // DaysUsed
  return "int"_lit;
}

namespace
{
enum class AnalyticsState
{
  Nothing,
  Disabled,
  PromptFirstTime,
  SubmitReport,
};

enum class AnalyticsSerialiseType
{
  Loading,
  Saving,
  Reporting,
  Documenting,
};

static AnalyticsState analyticsState = AnalyticsState::Nothing;
static Analytics actualDB;
static QString analyticsSaveLocation;

static const char *analyticsJSONMagic = "Analytics";
static const int analyticsJSONVersion = 1;

template <typename T>
QVariant analyticsToVar(const T &el, bool reporting)
{
  return QVariant(el);
}

// annoying specialisation, but necessary
template <typename T>
QVariant analyticsToVar(const bool (&el)[32], bool reporting)
{
  QVariantList l;
  for(int i = 0; i < 32; i++)
    l.push_back(QVariant(el[i]));
  return l;
}

template <>
QVariant analyticsToVar(const AnalyticsAverage &el, bool reporting)
{
  return el.toVariant(reporting);
}

template <typename T>
void analyticsFromVar(T &el, QVariant v)
{
  el = v.value<T>();
}

template <>
void analyticsFromVar(bool (&el)[32], QVariant v)
{
  QVariantList l = v.toList();
  for(int i = 0; i < 32 && i < l.count(); i++)
    el[i] = l[i].toBool();
}

template <>
void analyticsFromVar(AnalyticsAverage &el, QVariant v)
{
  el = AnalyticsAverage(v);
}

// these functions will process a name like Foo.Bar into Foo being a QVariantMap and Bar being a
// member of it.
template <typename T>
void loadFrom(const QVariantMap &parent, const QString &name, T &el)
{
  int idx = name.indexOf(QLatin1Char('.'));
  if(idx > 0)
  {
    QString parentName = name.left(idx);
    QString subname = name.mid(idx + 1);

    if(parent.contains(parentName))
      loadFrom(parent[parentName].toMap(), subname, el);

    return;
  }

  if(parent.contains(name))
    analyticsFromVar<T>(el, parent[name]);
}

template <typename T>
void saveTo(QVariantMap &parent, const QString &name, const T &el, bool reporting)
{
  int idx = name.indexOf(QLatin1Char('.'));
  if(idx > 0)
  {
    QString parentName = name.left(idx);
    QString subname = name.mid(idx + 1);

    // need to load-hit-store to change the contents :(.
    {
      QVariantMap map = parent[parentName].toMap();
      saveTo(map, subname, el, reporting);
      parent[parentName] = map;
    }
    return;
  }

  parent[name] = analyticsToVar<T>(el, reporting);
}

// add a macro to either document, load, or save, depending on our state.
#define ANALYTIC_SERIALISE(varname)                                       \
  if(type == AnalyticsSerialiseType::Documenting)                         \
  {                                                                       \
    QString var = lit(#varname);                                          \
    int idx = var.indexOf(QLatin1Char('.'));                              \
    if(idx >= 0)                                                          \
      var = var.mid(idx + 1);                                             \
    doc += lit("<b>%1 (%2)</b>: %3<br>")                                  \
               .arg(var)                                                  \
               .arg(QString(rdcstr(TypeName<decltype(serdb.varname)>()))) \
               .arg(docs.varname);                                        \
  }                                                                       \
  else if(type == AnalyticsSerialiseType::Loading)                        \
  {                                                                       \
    loadFrom(values, lit(#varname), serdb.varname);                       \
  }                                                                       \
  else                                                                    \
  {                                                                       \
    saveTo(values, lit(#varname), serdb.varname, reporting);              \
  }

// only used during documenting
#define ANALYTIC_SECTION(section)                          \
  if(type == AnalyticsSerialiseType::Documenting)          \
  {                                                        \
    doc += lit("<h2>%1</h2>").arg(docs.section_##section); \
  }

// macros for documenting the analytic values
#define DOCUMENT_ANALYTIC(name, docs) QString name = lit(docs);

#define DOCUMENT_ANALYTIC_SECTION(name, docs) \
  name;                                       \
  QString section_##name = lit(docs);

// must match the properties in Analytics
static struct AnalyticsDocumentation
{
  struct
  {
    DOCUMENT_ANALYTIC(Year, "The year this data was recorded in.");
    DOCUMENT_ANALYTIC(Month, "The month this data was recorded in.");
  } DOCUMENT_ANALYTIC_SECTION(Date, "Date range");

  DOCUMENT_ANALYTIC(Version, "The version number of the analytics data.");

  struct
  {
    DOCUMENT_ANALYTIC(RenderDocVersion, "The RenderDoc build version used to submit the report.");
    DOCUMENT_ANALYTIC(DistributionVersion, "The distribution version, if this is a linux build.");
    DOCUMENT_ANALYTIC(OSVersion, "OS version as reported by Qt.");
    DOCUMENT_ANALYTIC(Bitness, "Whether the build is 64-bit or 32-bit.");
    DOCUMENT_ANALYTIC(DevelBuildRun,
                      "Has a local or nightly or otherwise unofficial build been run?");
    DOCUMENT_ANALYTIC(OfficialBuildRun, "Has an officially produced binary build been run?");
    DOCUMENT_ANALYTIC(DaysUsed, "How many unique days in this month was the program run?");
  } DOCUMENT_ANALYTIC_SECTION(Metadata, "Metadata");

  struct
  {
    DOCUMENT_ANALYTIC(LoadTime, "How long (on average) did captures take to load?");
  } DOCUMENT_ANALYTIC_SECTION(Performance, "Performance");

  DOCUMENT_ANALYTIC(APIs, "A list of the distinct APIs that were replayed.");

  DOCUMENT_ANALYTIC(GPUVendors, "A list of the distinct GPU vendors used for replay.");

  struct
  {
    DOCUMENT_ANALYTIC(Bookmarks, "Did the user set any event bookmarks?");
    DOCUMENT_ANALYTIC(ResourceInspect, "Did the user use the resource inspector?");
    DOCUMENT_ANALYTIC(ShaderEditing, "Did the user edit a shader (any API)?");
    DOCUMENT_ANALYTIC(CallstackResolve, "Did the user capture and resolve CPU callstacks?");
    DOCUMENT_ANALYTIC(PixelHistory, "Did the user run a pixel history?");
    DOCUMENT_ANALYTIC(DrawcallTimes, "Did the user fetch drawcall timings/durations?");
    DOCUMENT_ANALYTIC(PerformanceCounters, "Did the user fetch advanced performance counters?");
    DOCUMENT_ANALYTIC(PythonInterop, "Did the user run any python scripts or commands?");
    DOCUMENT_ANALYTIC(CustomTextureVisualise,
                      "Did the user use a custom texture visualisation shader?");
    DOCUMENT_ANALYTIC(ImageViewer,
                      "Did the user employ RenderDoc as an image (DDS/PNG/HDR) viewer?");
    DOCUMENT_ANALYTIC(CaptureComments,
                      "Did the user make and save any comments in a capture file?");
    DOCUMENT_ANALYTIC(AndroidRemoteReplay, "Did the user use Android remote replay functionality?");
    DOCUMENT_ANALYTIC(NonAndroidRemoteReplay, "Did the user use remote replay on non-Android?");
  } DOCUMENT_ANALYTIC_SECTION(UIFeatures, "UI Features");

  struct
  {
    DOCUMENT_ANALYTIC(EventBrowser, "Did the user ever export drawcalls from the event browser?");
    DOCUMENT_ANALYTIC(PipelineState, "Did the user ever export the pipeline state (any API)?");
    DOCUMENT_ANALYTIC(MeshOutput, "Did the user ever export mesh data (inputs or outputs)?");
    DOCUMENT_ANALYTIC(RawBuffer, "Did the user ever export raw buffer data?");
    DOCUMENT_ANALYTIC(Texture, "Did the user ever export a texture?");
    DOCUMENT_ANALYTIC(Shader, "Did the user ever export a shader?");
  } DOCUMENT_ANALYTIC_SECTION(Export, "Data Export");

  struct
  {
    DOCUMENT_ANALYTIC(Vertex, "Did the user ever debug a vertex shader?");
    DOCUMENT_ANALYTIC(Pixel, "Did the user ever debug a pixel shader?");
    DOCUMENT_ANALYTIC(Compute, "Did the user ever debug a compute shader?");
  } DOCUMENT_ANALYTIC_SECTION(ShaderDebug, "Shader Debugging");

  struct
  {
    DOCUMENT_ANALYTIC(Drawcall, "Did the user use the Drawcall overlay?");
    DOCUMENT_ANALYTIC(Wireframe, "Did the user use the Wireframe overlay?");
    DOCUMENT_ANALYTIC(Depth, "Did the user use the Depth Test overlay?");
    DOCUMENT_ANALYTIC(Stencil, "Did the user use the Stencil Test overlay?");
    DOCUMENT_ANALYTIC(BackfaceCull, "Did the user use the Backface Culling overlay?");
    DOCUMENT_ANALYTIC(ViewportScissor, "Did the user use the Viewport/Scissor overlay?");
    DOCUMENT_ANALYTIC(NaN, "Did the user use the NaN/Inf/-ve overlay?");
    DOCUMENT_ANALYTIC(Clipping, "Did the user use the Histogram Clipping overlay?");
    DOCUMENT_ANALYTIC(ClearBeforePass, "Did the user use the Clear Before Pass overlay?");
    DOCUMENT_ANALYTIC(ClearBeforeDraw, "Did the user use the Clear Before Draw overlay?");
    DOCUMENT_ANALYTIC(QuadOverdrawPass, "Did the user use the Quad Overdraw (Pass) overlay?");
    DOCUMENT_ANALYTIC(QuadOverdrawDraw, "Did the user use the Quad Overdraw (Draw) overlay?");
    DOCUMENT_ANALYTIC(TriangleSizePass, "Did the user use the Triangle Size (Pass) overlay?");
    DOCUMENT_ANALYTIC(TriangleSizeDraw, "Did the user use the Triangle Size (Draw) overlay?");
  } DOCUMENT_ANALYTIC_SECTION(TextureOverlays, "Texture Overlays");

  struct
  {
    DOCUMENT_ANALYTIC(ShaderLinkage, "Did any capture use 'shader linkage' functionality?");
    DOCUMENT_ANALYTIC(YUVTextures, "Did any capture use YUV/composite textures?");
    DOCUMENT_ANALYTIC(SparseResources, "Did any capture use sparse aka tiled resources?");
    DOCUMENT_ANALYTIC(MultiGPU, "Did any capture make use of multiple GPUs?");
    DOCUMENT_ANALYTIC(D3D12Bundle, "Did any D3D12 capture use bundles?");
  } DOCUMENT_ANALYTIC_SECTION(CaptureFeatures, "Capture API Usage");
} docs;

void AnalyticsSerialise(Analytics &serdb, QVariantMap &values, AnalyticsSerialiseType type)
{
  bool reporting = type == AnalyticsSerialiseType::Reporting;

// only check this on 64-bit as it is different on 32-bit
#if QT_POINTER_SIZE == 8 && defined(Q_OS_WIN32)
  static_assert(sizeof(Analytics) == 147, "Sizeof Analytics has changed - update serialisation.");
#endif

  QString doc;

  doc += lit("<h1>Report Explained</h1>");

  ANALYTIC_SERIALISE(Version);

  ANALYTIC_SECTION(Date);
  {
    ANALYTIC_SERIALISE(Date.Year);
    ANALYTIC_SERIALISE(Date.Month);
  }

  ANALYTIC_SECTION(Metadata);
  {
    ANALYTIC_SERIALISE(Metadata.RenderDocVersion);
    ANALYTIC_SERIALISE(Metadata.DistributionVersion);
    ANALYTIC_SERIALISE(Metadata.OSVersion);
    ANALYTIC_SERIALISE(Metadata.Bitness);
    ANALYTIC_SERIALISE(Metadata.DevelBuildRun);
    ANALYTIC_SERIALISE(Metadata.OfficialBuildRun);

    // special handling for reporting DaysUsed, to flatten into a number
    if(reporting)
    {
      int sum = 0;
      for(bool day : Analytics::db->Metadata.DaysUsed)
        sum += day ? 1 : 0;

      saveTo(values, lit("Metadata.DaysUsed"), sum, reporting);
    }
    else
    {
      ANALYTIC_SERIALISE(Metadata.DaysUsed);
    }
  }

  ANALYTIC_SECTION(Performance);
  {
    ANALYTIC_SERIALISE(Performance.LoadTime);
  }

  doc += lit("<h2>API/GPU Usage</h2>");

  ANALYTIC_SERIALISE(APIs);
  ANALYTIC_SERIALISE(GPUVendors);

  ANALYTIC_SECTION(UIFeatures);
  {
    ANALYTIC_SERIALISE(UIFeatures.Bookmarks);
    ANALYTIC_SERIALISE(UIFeatures.ResourceInspect);
    ANALYTIC_SERIALISE(UIFeatures.ShaderEditing);
    ANALYTIC_SERIALISE(UIFeatures.CallstackResolve);
    ANALYTIC_SERIALISE(UIFeatures.PixelHistory);
    ANALYTIC_SERIALISE(UIFeatures.DrawcallTimes);
    ANALYTIC_SERIALISE(UIFeatures.PerformanceCounters);
    ANALYTIC_SERIALISE(UIFeatures.PythonInterop);
    ANALYTIC_SERIALISE(UIFeatures.CustomTextureVisualise);
    ANALYTIC_SERIALISE(UIFeatures.ImageViewer);
    ANALYTIC_SERIALISE(UIFeatures.CaptureComments);
    ANALYTIC_SERIALISE(UIFeatures.AndroidRemoteReplay);
    ANALYTIC_SERIALISE(UIFeatures.NonAndroidRemoteReplay);
  }

  ANALYTIC_SECTION(Export);
  {
    ANALYTIC_SERIALISE(Export.EventBrowser);
    ANALYTIC_SERIALISE(Export.PipelineState);
    ANALYTIC_SERIALISE(Export.MeshOutput);
    ANALYTIC_SERIALISE(Export.RawBuffer);
    ANALYTIC_SERIALISE(Export.Texture);
    ANALYTIC_SERIALISE(Export.Shader);
  }

  ANALYTIC_SECTION(ShaderDebug);
  {
    ANALYTIC_SERIALISE(ShaderDebug.Vertex);
    ANALYTIC_SERIALISE(ShaderDebug.Pixel);
    ANALYTIC_SERIALISE(ShaderDebug.Compute);
  }

  ANALYTIC_SECTION(TextureOverlays);
  {
    ANALYTIC_SERIALISE(TextureOverlays.Drawcall);
    ANALYTIC_SERIALISE(TextureOverlays.Wireframe);
    ANALYTIC_SERIALISE(TextureOverlays.Depth);
    ANALYTIC_SERIALISE(TextureOverlays.Stencil);
    ANALYTIC_SERIALISE(TextureOverlays.BackfaceCull);
    ANALYTIC_SERIALISE(TextureOverlays.ViewportScissor);
    ANALYTIC_SERIALISE(TextureOverlays.NaN);
    ANALYTIC_SERIALISE(TextureOverlays.Clipping);
    ANALYTIC_SERIALISE(TextureOverlays.ClearBeforePass);
    ANALYTIC_SERIALISE(TextureOverlays.ClearBeforeDraw);
    ANALYTIC_SERIALISE(TextureOverlays.QuadOverdrawPass);
    ANALYTIC_SERIALISE(TextureOverlays.QuadOverdrawDraw);
    ANALYTIC_SERIALISE(TextureOverlays.TriangleSizePass);
    ANALYTIC_SERIALISE(TextureOverlays.TriangleSizeDraw);
  }

  ANALYTIC_SECTION(CaptureFeatures);
  {
    ANALYTIC_SERIALISE(CaptureFeatures.ShaderLinkage);
    ANALYTIC_SERIALISE(CaptureFeatures.YUVTextures);
    ANALYTIC_SERIALISE(CaptureFeatures.SparseResources);
    ANALYTIC_SERIALISE(CaptureFeatures.MultiGPU);
    ANALYTIC_SERIALISE(CaptureFeatures.D3D12Bundle);
  }

  if(type == AnalyticsSerialiseType::Documenting)
    values[lit("doc")] = doc;
}

};    // anonymous namespace

Analytics *Analytics::db = NULL;

void Analytics::Save()
{
  if(analyticsSaveLocation.isEmpty() || analyticsState == AnalyticsState::Disabled ||
     Analytics::db == NULL)
    return;

  QVariantMap values;

  // call to the serialise function to save into the 'values' map
  AnalyticsSerialise(*Analytics::db, values, AnalyticsSerialiseType::Saving);

  QFile f(analyticsSaveLocation);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    SaveToJSON(values, f, analyticsJSONMagic, analyticsJSONVersion);
}

void Analytics::Disable()
{
  // do not save any values
  Analytics::db = NULL;
  analyticsSaveLocation = QString();
  analyticsState = AnalyticsState::Disabled;
}

void Analytics::Load()
{
  // refuse to load if we were previously disabled, just in case this function is called somehow. We
  // require a full restart with the analytics enabled for it to start collecting.
  if(analyticsState == AnalyticsState::Disabled)
    return;

  // allocate space for the Analytics singleton
  Analytics::db = &actualDB;

  // find the filename where the analytics will be saved
  analyticsSaveLocation = configFilePath(lit("analytics.json"));

  QFile f(analyticsSaveLocation);

  // silently allow missing database
  if(f.exists())
  {
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      QVariantMap values;

      bool success = LoadFromJSON(values, f, analyticsJSONMagic, analyticsJSONVersion);

      if(success)
        AnalyticsSerialise(*Analytics::db, values, AnalyticsSerialiseType::Loading);
    }
  }

  // if year is 0, this was uninitialised meaning there was no previous analytics database. We set
  // the current month, and mark a flag that we need to prompt the user about opting out of
  // analytics.
  int currentYear = QDateTime::currentDateTime().date().year();
  int currentMonth = QDateTime::currentDateTime().date().month();
  if(Analytics::db->Date.Year == 0)
  {
    Analytics::db->Date.Year = currentYear;
    Analytics::db->Date.Month = currentMonth;
    analyticsState = AnalyticsState::PromptFirstTime;
  }
  else if(Analytics::db->Date.Year != currentYear || Analytics::db->Date.Month != currentMonth)
  {
    // if the analytics is not from this month, submit the report.
    analyticsState = AnalyticsState::SubmitReport;
  }
}

void Analytics::DocumentReport()
{
  Analytics dummyDB;
  QVariantMap dummyMap;
  AnalyticsSerialise(dummyDB, dummyMap, AnalyticsSerialiseType::Documenting);
  QString reportText = dummyMap[lit("doc")].toString();

  {
    QDialog dialog;
    dialog.setWindowTitle(lit("Sample Analytics Report"));
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog.setFixedSize(600, 500);

    QDialogButtonBox buttons;
    buttons.addButton(QDialogButtonBox::Ok);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    QTextEdit report;
    report.setReadOnly(true);
    report.setAcceptRichText(true);
    report.setText(reportText);

    QVBoxLayout layout;

    layout.addWidget(&report);
    layout.addWidget(&buttons);

    dialog.setLayout(&layout);

    RDDialog::show(&dialog);
  }
}

void Analytics::Prompt(ICaptureContext &ctx, PersistantConfig &config)
{
  if(analyticsState == AnalyticsState::Disabled)
  {
    // do nothing, we're disabled
    return;
  }
  else if(analyticsState == AnalyticsState::PromptFirstTime)
  {
    QWidget *mainWindow = ctx.GetMainWindow()->Widget();

    AnalyticsPromptDialog prompt(config, mainWindow);
    RDDialog::show(&prompt);
  }
  else if(analyticsState == AnalyticsState::SubmitReport && Analytics::db != NULL)
  {
    QWidget *mainWindow = ctx.GetMainWindow()->Widget();

    QVariantMap values;

    AnalyticsSerialise(*Analytics::db, values, AnalyticsSerialiseType::Reporting);

    QBuffer buf;
    buf.open(QBuffer::WriteOnly);

    SaveToJSON(values, buf, NULL, 0);

    QString jsonReport = QString::fromUtf8(buf.buffer());

    if(config.Analytics_ManualCheck)
    {
      AnalyticsConfirmDialog confirm(jsonReport, mainWindow);
      int res = RDDialog::show(&confirm);

      if(res == 0)
        analyticsState = AnalyticsState::Nothing;
    }

    if(analyticsState == AnalyticsState::SubmitReport)
    {
      QNetworkAccessManager *mgr = new QNetworkAccessManager(mainWindow);

      QUrlQuery postData;

      postData.addQueryItem(lit("report"), jsonReport);

      QNetworkRequest request(QUrl(lit("https://renderdoc.org/analytics")));

      request.setHeader(QNetworkRequest::ContentTypeHeader,
                        lit("application/x-www-form-urlencoded"));

      QByteArray dataText = postData.toString(QUrl::FullyEncoded).toUtf8();
      mgr->post(request, dataText);
    }

    // reset the database now
    *Analytics::db = Analytics();
    Analytics::db->Date.Year = QDateTime::currentDateTime().date().year();
    Analytics::db->Date.Month = QDateTime::currentDateTime().date().month();
  }
}

#else    // RENDERDOC_ANALYTICS_ENABLE

namespace Analytics
{
void Load()
{
}

void Disable()
{
}

void Prompt(ICaptureContext &ctx, PersistantConfig &config)
{
}

void DocumentReport()
{
}
};

#endif
