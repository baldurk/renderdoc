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

#pragma once

// This file controls the telemetry/analytics functionality in RenderDoc.
//
// If you don't care about any details and just want to make sure this is turned off for your builds
// then go below to #define RENDERDOC_ANALYTICS_ENABLE and change it to 0. That will cease all
// recording and reporting of analytics data. It won't delete any previously recorded analytics and
// won't stop any other builds from working, since the code that knows about what the analytics is
// will be compiled out.
//
// -------------------------------------------------------------------------------------------------
//
// RenderDoc's analytics works in two phases: Data is recorded first to an internal database that
// can contain more information than will be sent, to allow for accurate tracking before
// aggregation. This isn't as scary as it sounds - e.g. consider the 'UsageLevel' stat that's
// reported, which is a number in the range 1 to 31 indicating how many days in the month the
// program was launched. In order to track that, we need to have a sticky flag for each day to count
// it, since given a number "20" it's impossible to know if we've counted today or not.
//
// Similar cases are found for averages that need to store the number of cases and a total, which is
// then flattened down into just an average for reporting.
//
// This database is stored internally and then converted to the report in the 'AnalyticsSerialise'
// function.
//
// Once the report is sent, the database is reset and begins the next period.

// this is the root switch that can turn off *all* analytics code globally
#define RENDERDOC_ANALYTICS_ENABLE 1

// we don't want any of this to be accessible to script, only code.
#if !defined(SWIG) && !defined(SWIG_GENERATED)

// We also compile out all of the code if analytics are disabled so there's not even a code
// reference to where the data is collected.
#if RENDERDOC_ANALYTICS_ENABLE

struct AnalyticsAverage
{
  double Total = 0.0;
  int Count = 0;

  void Add(double Val)
  {
    Total += Val;
    Count++;
  }

  AnalyticsAverage() = default;
  AnalyticsAverage(QVariant v)
  {
    QVariantMap vmap = v.toMap();
    Total = vmap[lit("Total")].toDouble();
    Count = vmap[lit("Count")].toInt();
  }

  QVariant toVariant(bool reporting) const
  {
    if(reporting)
    {
      if(Count == 0)
        return 0.0;
      return Total / double(Count);
    }

    QVariantMap ret;
    ret[lit("Total")] = Total;
    ret[lit("Count")] = Count;
    return ret;
  }
};

class PersistantConfig;
struct ICaptureContext;

// we set this struct to byte-packing, so that any change will affect sizeof() and fail a
// static_assert in AnalyticsSerialise. This only has to be on one platform since it will block
// building when a commit is pushed. This struct isn't memcpy'd around and isn't performance
// sensitive, so there's no need to have perfectly aligned data.
#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

// we declare the analystics data struct here, this contains the information we're storing
// If you add anything to this struct, make sure to update AnalyticsSerialise, and
// AnalyticsDocumentation.
struct Analytics
{
  // utility function - loads the analytics from disk and initialise the Analytics::db member.
  static void Load();
  // utility function - explicitly disables the analytics and sets it into a black-hole mode that
  // does nothing.
  static void Disable();
  // utility function - performs any UI-level prompting, such as asking the user if they want to
  // opt-out, or manually vetting a report for uploading.
  static void Prompt(ICaptureContext &ctx, PersistantConfig &config);
  // the singleton instance of analytics. May be NULL if analytics aren't initialised or have been
  // opted-out from.
  static Analytics *db;

  // utility function - displays an annotated report documenting what each member means
  static void DocumentReport();

  // Function to save the analytics to disk, if it's been initialised. Every set macro below will
  // call this after the data is set to flush it to disk.
  void Save();

  // book-keeping: contains the year/month where this database was started. Thus if current date !=
  // saved date, then we should try to submit the analytics report.
  struct
  {
    int Year = 0;
    int Month = 0;
  } Date;

  // version number. Most data is opportunistically gathered, so if some data is missing then it
  // just doesn't get included in the report so we can add new data, but we bump the version
  // here if something more significant changes.
  int Version = 1;

  struct
  {
    // The version string (MAJOR_MINOR_VERSION_STRING) of this build.
    QString RenderDocVersion;

    // The distribution information (DISTRIBUTION_NAME) for this build.
    QString DistributionVersion;

    // A human readable name of the operating system.
    QString OSVersion;

    // Either 32 or 64 indicating which bit-depth the UI is running as
    int Bitness = 0;

    // whether a development build has been run - either a nightly build or a local build.
    bool DevelBuildRun = false;

    // whether an official build has been run - whether distributed from the RenderDoc website or
    // through a linux distribution
    bool OfficialBuildRun = false;

    // Counts which unique days in the last month the program was run
    bool DaysUsed[32] = {0};
  } Metadata;

  struct
  {
    // how long do captures take to load, on average
    AnalyticsAverage LoadTime;
  } Performance;

  // which APIs have been used
  QStringList APIs;

  // A list of which GPU vendors have been used for replay
  QStringList GPUVendors;

  // which UI features have been used, as a simple yes/no
  struct
  {
    bool Bookmarks = false;
    bool ResourceInspect = false;
    bool ShaderEditing = false;
    bool CallstackResolve = false;
    bool PixelHistory = false;
    bool DrawcallTimes = false;
    bool PerformanceCounters = false;
    bool PythonInterop = false;
    bool CustomTextureVisualise = false;
    bool ImageViewer = false;
    bool CaptureComments = false;
    bool AndroidRemoteReplay = false;
    bool NonAndroidRemoteReplay = false;
  } UIFeatures;

  struct
  {
    bool EventBrowser = false;
    bool PipelineState = false;
    bool MeshOutput = false;
    bool RawBuffer = false;
    bool Texture = false;
    bool Shader = false;
  } Export;

  struct
  {
    bool Vertex = false;
    bool Pixel = false;
    bool Compute = false;
  } ShaderDebug;

  struct
  {
    bool Drawcall = false;
    bool Wireframe = false;
    bool Depth = false;
    bool Stencil = false;
    bool BackfaceCull = false;
    bool ViewportScissor = false;
    bool NaN = false;
    bool Clipping = false;
    bool ClearBeforePass = false;
    bool ClearBeforeDraw = false;
    bool QuadOverdrawPass = false;
    bool QuadOverdrawDraw = false;
    bool TriangleSizePass = false;
    bool TriangleSizeDraw = false;
  } TextureOverlays;

  // If some particular API specific features are seen in a capture, as a simple yes/no. See
  // APIProperties
  struct
  {
    bool ShaderLinkage = false;
    bool YUVTextures = false;
    bool SparseResources = false;
    bool MultiGPU = false;
    bool D3D12Bundle = false;
  } CaptureFeatures;
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

// straightforward set of a value
#define ANALYTIC_SET(name, val)        \
  do                                   \
  {                                    \
    if(Analytics::db)                  \
    {                                  \
      auto value = val;                \
      if(Analytics::db->name != value) \
      {                                \
        Analytics::db->name = value;   \
        Analytics::db->Save();         \
      }                                \
    }                                  \
  } while(0);

// add a data point to an average
#define ANALYTIC_ADDAVG(name, val)  \
  do                                \
  {                                 \
    if(Analytics::db)               \
    {                               \
      Analytics::db->name.Add(val); \
      Analytics::db->Save();        \
    }                               \
  } while(0);

// add an element to an array, if it's not already present
#define ANALYTIC_ADDUNIQ(name, val)         \
  do                                        \
  {                                         \
    if(Analytics::db)                       \
    {                                       \
      bool found = false;                   \
      for(auto &v : Analytics::db->name)    \
      {                                     \
        if(v == val)                        \
        {                                   \
          found = true;                     \
          break;                            \
        }                                   \
      }                                     \
                                            \
      if(!found)                            \
      {                                     \
        Analytics::db->name.push_back(val); \
        Analytics::db->Save();              \
      }                                     \
    }                                       \
  } while(0);

#else

// here we declare macro stubs that satisfy the use in the codebase.
#define ANALYTIC_SET(name, val) (void)val
#define ANALYTIC_ADDAVG(name, val) (void)val
#define ANALYTIC_ADDUNIQ(name, val) (void)val

class PersistantConfig;
struct ICaptureContext;

namespace Analytics
{
void Disable();
void Load();
void Prompt(ICaptureContext &ctx, PersistantConfig &config);
void DocumentReport();
};

#endif

#endif