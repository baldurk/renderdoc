/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include "Windows/Dialogs/AnalyticsConfirmDialog.h"
#include "Windows/Dialogs/AnalyticsPromptDialog.h"
#include "QRDInterface.h"

#if RENDERDOC_ANALYTICS_ENABLE

namespace
{
enum class AnalyticsState
{
  Nothing,
  PromptFirstTime,
  SubmitReport,
};

enum class AnalyticsSerialiseType
{
  Loading,
  Saving,
  Reporting,
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

// add a macro to either load or save, depending on our state.
#define ANALYTIC_SERIALISE(varname)                                   \
  if(type == AnalyticsSerialiseType::Loading)                         \
  {                                                                   \
    loadFrom(values, lit(#varname), Analytics::db->varname);          \
  }                                                                   \
  else                                                                \
  {                                                                   \
    saveTo(values, lit(#varname), Analytics::db->varname, reporting); \
  }

void AnalyticsSerialise(QVariantMap &values, AnalyticsSerialiseType type)
{
  bool reporting = type == AnalyticsSerialiseType::Reporting;

  if(!Analytics::db)
    return;

  static_assert(sizeof(Analytics) == 147, "Sizeof Analytics has changed - update serialisation.");

  // Date
  {
    ANALYTIC_SERIALISE(Date.Year);
    ANALYTIC_SERIALISE(Date.Month);
  }
  ANALYTIC_SERIALISE(Version);

  // Metadata
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

  // Performance
  {
    ANALYTIC_SERIALISE(Performance.LoadTime);
  }

  ANALYTIC_SERIALISE(APIs);
  ANALYTIC_SERIALISE(GPUVendors);

  // UIFeatures
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

  // Export
  {
    ANALYTIC_SERIALISE(Export.EventBrowser);
    ANALYTIC_SERIALISE(Export.PipelineState);
    ANALYTIC_SERIALISE(Export.MeshOutput);
    ANALYTIC_SERIALISE(Export.RawBuffer);
    ANALYTIC_SERIALISE(Export.Texture);
    ANALYTIC_SERIALISE(Export.Shader);
  }

  // ShaderDebug
  {
    ANALYTIC_SERIALISE(ShaderDebug.Vertex);
    ANALYTIC_SERIALISE(ShaderDebug.Pixel);
    ANALYTIC_SERIALISE(ShaderDebug.Compute);
  }

  // TextureOverlays
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

  // CaptureFeatures
  {
    ANALYTIC_SERIALISE(CaptureFeatures.ShaderLinkage);
    ANALYTIC_SERIALISE(CaptureFeatures.YUVTextures);
    ANALYTIC_SERIALISE(CaptureFeatures.SparseResources);
    ANALYTIC_SERIALISE(CaptureFeatures.MultiGPU);
    ANALYTIC_SERIALISE(CaptureFeatures.D3D12Bundle);
  }
}

};    // anonymous namespace

Analytics *Analytics::db = NULL;

void Analytics::Save()
{
  if(analyticsSaveLocation.isEmpty())
    return;

  QVariantMap values;

  // call to the serialise function to save into the 'values' map
  AnalyticsSerialise(values, AnalyticsSerialiseType::Saving);

  QFile f(analyticsSaveLocation);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    SaveToJSON(values, f, analyticsJSONMagic, analyticsJSONVersion);
}

void Analytics::Load()
{
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
        AnalyticsSerialise(values, AnalyticsSerialiseType::Loading);
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

void Analytics::Prompt(ICaptureContext &ctx, PersistantConfig &config)
{
  if(analyticsState == AnalyticsState::PromptFirstTime)
  {
    QWidget *mainWindow = ctx.GetMainWindow()->Widget();

    AnalyticsPromptDialog prompt(config, mainWindow);
    RDDialog::show(&prompt);
  }
  else if(analyticsState == AnalyticsState::SubmitReport)
  {
    QWidget *mainWindow = ctx.GetMainWindow()->Widget();

    QVariantMap values;

    AnalyticsSerialise(values, AnalyticsSerialiseType::Reporting);

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

void Prompt(ICaptureContext &ctx, PersistantConfig &config)
{
}
};

#endif
