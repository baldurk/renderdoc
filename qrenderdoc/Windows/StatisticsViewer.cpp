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

#include "StatisticsViewer.h"
#include <QFontDatabase>
#include "Code/QRDUtils.h"
#include "ui_StatisticsViewer.h"

static const int HistogramWidth = 128;
static const QString Stars = QString(HistogramWidth, QLatin1Char('*'));

QString Pow2IndexAsReadable(int index)
{
  uint64_t value = 1ULL << index;

  if(value >= (1024 * 1024))
  {
    float slice = (float)value / (1024 * 1024);
    return QFormatStr("%1MB").arg(Formatter::Format(slice));
  }
  else if(value >= 1024)
  {
    float slice = (float)value / 1024;
    return QFormatStr("%1KB").arg(Formatter::Format(slice));
  }
  else
  {
    return QFormatStr("%1B").arg(Formatter::Format((float)value));
  }
}

int SliceForString(const QString &s, uint32_t value, uint32_t maximum)
{
  if(value == 0 || maximum == 0)
    return 0;

  float ratio = (float)value / maximum;
  int slice = (int)(ratio * s.length());
  return qMax(1, slice);
}

QString CountOrEmpty(uint32_t count)
{
  if(count == 0)
    return QString();
  else
    return QFormatStr("(%1)").arg(count);
}

QString CreateSimpleIntegerHistogram(const QString &legend, const rdcarray<uint32_t> &array)
{
  uint32_t maxCount = 0;
  int maxWithValue = 0;

  for(int o = 0; o < array.count(); o++)
  {
    uint32_t value = array[o];
    if(value > 0)
      maxWithValue = o;
    maxCount = qMax(maxCount, value);
  }

  QString text = QFormatStr("\n%1:\n").arg(legend);

  for(int o = 0; o <= maxWithValue; o++)
  {
    uint32_t count = array[o];
    int slice = SliceForString(Stars, count, maxCount);
    text += QFormatStr("%1: %2 %3\n").arg(o, 4).arg(Stars.left(slice)).arg(CountOrEmpty(count));
  }

  return text;
}

void StatisticsViewer::AppendDrawStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  // #mivance see AppendConstantBindStatistics
  const DrawcallStats &draws = frameInfo.stats.draws;

  m_Report.append(tr("\n*** Draw Statistics ***\n\n"));

  m_Report.append(tr("Total calls: %1, instanced: %2, indirect: %3\n")
                      .arg(draws.calls)
                      .arg(draws.instanced)
                      .arg(draws.indirect));

  if(draws.instanced > 0)
  {
    m_Report.append(tr("\nInstance counts:\n"));
    uint32_t maxCount = 0;
    int maxWithValue = 0;
    int maximum = draws.counts.count();
    for(int s = 1; s < maximum; s++)
    {
      uint32_t value = draws.counts[s];
      if(value > 0)
        maxWithValue = s;
      maxCount = qMax(maxCount, value);
    }

    for(int s = 1; s <= maxWithValue; s++)
    {
      uint32_t count = draws.counts[s];
      int slice = SliceForString(Stars, count, maxCount);
      m_Report.append(QFormatStr("%1%2: %3 %4\n")
                          .arg((s == maximum - 1) ? lit(">=") : lit("  "))
                          .arg(s, 2)
                          .arg(Stars.left(slice))
                          .arg(CountOrEmpty(count)));
    }
  }
}

void StatisticsViewer::AppendDispatchStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  m_Report.append(tr("\n*** Dispatch Statistics ***\n\n"));
  m_Report.append(tr("Total calls: %1, indirect: %2\n")
                      .arg(frameInfo.stats.dispatches.calls)
                      .arg(frameInfo.stats.dispatches.indirect));
}

void StatisticsViewer::AppendInputAssemblerStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  const IndexBindStats &indices = frameInfo.stats.indices;
  const LayoutBindStats &layouts = frameInfo.stats.layouts;

  const VertexBindStats &vertices = frameInfo.stats.vertices;

  m_Report.append(tr("\n*** Input Assembler Statistics ***\n\n"));

  m_Report.append(tr("Total index calls: %1, non-null index sets: %2, null index sets: %3\n")
                      .arg(indices.calls)
                      .arg(indices.sets)
                      .arg(indices.nulls));
  m_Report.append(tr("Total layout calls: %1, non-null layout sets: %2, null layout sets: %3\n")
                      .arg(layouts.calls)
                      .arg(layouts.sets)
                      .arg(layouts.nulls));
  m_Report.append(tr("Total vertex calls: %1, non-null vertex sets: %2, null vertex sets: %3\n")
                      .arg(vertices.calls)
                      .arg(vertices.sets)
                      .arg(vertices.nulls));

  m_Report.append(CreateSimpleIntegerHistogram(tr("Aggregate vertex slot counts per invocation"),
                                               vertices.bindslots));
}

void StatisticsViewer::AppendShaderStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  const ShaderChangeStats *shaders = frameInfo.stats.shaders;
  ShaderChangeStats totalShadersPerStage;
  memset(&totalShadersPerStage, 0, sizeof(totalShadersPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalShadersPerStage.calls += shaders[s].calls;
    totalShadersPerStage.sets += shaders[s].sets;
    totalShadersPerStage.nulls += shaders[s].nulls;
    totalShadersPerStage.redundants += shaders[s].redundants;
  }

  m_Report.append(tr("\n*** Shader Set Statistics ***\n\n"));

  for(auto s : indices<ShaderStage>())
  {
    m_Report.append(tr("%1 calls: %2, non-null shader sets: %3, null shader sets: %4, "
                       "redundant shader sets: %5\n")
                        .arg(m_Ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                        .arg(shaders[s].calls)
                        .arg(shaders[s].sets)
                        .arg(shaders[s].nulls)
                        .arg(shaders[s].redundants));
  }

  m_Report.append(tr("Total calls: %1, non-null shader sets: %2, null shader sets: %3, "
                     "redundant shader sets: %4\n")
                      .arg(totalShadersPerStage.calls)
                      .arg(totalShadersPerStage.sets)
                      .arg(totalShadersPerStage.nulls)
                      .arg(totalShadersPerStage.redundants));
}

void StatisticsViewer::AppendConstantBindStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  // #mivance C++-side we guarantee all stages will have the same slots
  // and sizes count, so pattern off of the first frame's first stage
  const ConstantBindStats &reference = frameInfo.stats.constants[0];

  // #mivance there is probably a way to iterate the fields via
  // GetType()/GetField() and build a sort of dynamic min/max/average
  // structure for a given type with known integral types (or arrays
  // thereof), but given we're heading for a Qt/C++ rewrite of the UI
  // perhaps best not to dwell too long on that
  ConstantBindStats totalConstantsPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalConstantsPerStage, 0, sizeof(totalConstantsPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalConstantsPerStage[s].bindslots.resize(reference.bindslots.size());
    totalConstantsPerStage[s].sizes.resize(reference.sizes.size());
  }

  {
    const ConstantBindStats *constants = frameInfo.stats.constants;
    for(auto s : indices<ShaderStage>())
    {
      totalConstantsPerStage[s].calls += constants[s].calls;
      totalConstantsPerStage[s].sets += constants[s].sets;
      totalConstantsPerStage[s].nulls += constants[s].nulls;

      for(int l = 0; l < constants[s].bindslots.count(); l++)
        totalConstantsPerStage[s].bindslots[l] += constants[s].bindslots[l];

      for(int z = 0; z < constants[s].sizes.count(); z++)
        totalConstantsPerStage[s].sizes[z] += constants[s].sizes[z];
    }
  }

  ConstantBindStats totalConstantsForAllStages;
  memset(&totalConstantsForAllStages, 0, sizeof(totalConstantsForAllStages));
  totalConstantsForAllStages.bindslots.resize(totalConstantsPerStage[0].bindslots.size());
  totalConstantsForAllStages.sizes.resize(totalConstantsPerStage[0].sizes.size());

  for(auto s : indices<ShaderStage>())
  {
    const ConstantBindStats &perStage = totalConstantsPerStage[s];
    totalConstantsForAllStages.calls += perStage.calls;
    totalConstantsForAllStages.sets += perStage.sets;
    totalConstantsForAllStages.nulls += perStage.nulls;

    for(int l = 0; l < perStage.bindslots.count(); l++)
      totalConstantsForAllStages.bindslots[l] += perStage.bindslots[l];

    for(int z = 0; z < perStage.sizes.count(); z++)
      totalConstantsForAllStages.sizes[z] += perStage.sizes[z];
  }

  m_Report.append(tr("\n*** Constant Bind Statistics ***\n\n"));

  for(auto s : indices<ShaderStage>())
  {
    m_Report.append(tr("%1 calls: %2, non-null buffer sets: %3, null buffer sets: %4\n")
                        .arg(m_Ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                        .arg(totalConstantsPerStage[s].calls)
                        .arg(totalConstantsPerStage[s].sets)
                        .arg(totalConstantsPerStage[s].nulls));
  }

  m_Report.append(tr("Total calls: %1, non-null buffer sets: %2, null buffer sets: %3\n")
                      .arg(totalConstantsForAllStages.calls)
                      .arg(totalConstantsForAllStages.sets)
                      .arg(totalConstantsForAllStages.nulls));

  m_Report.append(
      CreateSimpleIntegerHistogram(tr("Aggregate slot counts per invocation across all stages"),
                                   totalConstantsForAllStages.bindslots));

  m_Report.append(tr("\nAggregate constant buffer sizes across all stages:\n"));
  uint32_t maxCount = 0;
  int maxWithValue = 0;
  for(int s = 0; s < totalConstantsForAllStages.sizes.count(); s++)
  {
    uint32_t value = totalConstantsForAllStages.sizes[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 0; s <= maxWithValue; s++)
  {
    uint32_t count = totalConstantsForAllStages.sizes[s];
    int slice = SliceForString(Stars, count, maxCount);
    m_Report.append(QFormatStr("%1: %2 %3\n")
                        .arg(Pow2IndexAsReadable(s), 8)
                        .arg(Stars.left(slice))
                        .arg(CountOrEmpty(count)));
  }
}

void StatisticsViewer::AppendSamplerBindStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  // #mivance see AppendConstantBindStatistics
  const SamplerBindStats &reference = frameInfo.stats.samplers[0];

  SamplerBindStats totalSamplersPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalSamplersPerStage, 0, sizeof(totalSamplersPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalSamplersPerStage[s].bindslots.resize(reference.bindslots.size());
  }

  {
    const SamplerBindStats *samplers = frameInfo.stats.samplers;
    for(auto s : indices<ShaderStage>())
    {
      totalSamplersPerStage[s].calls += samplers[s].calls;
      totalSamplersPerStage[s].sets += samplers[s].sets;
      totalSamplersPerStage[s].nulls += samplers[s].nulls;

      for(int l = 0; l < samplers[s].bindslots.count(); l++)
      {
        totalSamplersPerStage[s].bindslots[l] += samplers[s].bindslots[l];
      }
    }
  }

  SamplerBindStats totalSamplersForAllStages;
  memset(&totalSamplersForAllStages, 0, sizeof(totalSamplersForAllStages));
  totalSamplersForAllStages.bindslots.resize(totalSamplersPerStage[0].bindslots.size());

  for(auto s : indices<ShaderStage>())
  {
    SamplerBindStats perStage = totalSamplersPerStage[s];
    totalSamplersForAllStages.calls += perStage.calls;
    totalSamplersForAllStages.sets += perStage.sets;
    totalSamplersForAllStages.nulls += perStage.nulls;
    for(int l = 0; l < perStage.bindslots.count(); l++)
    {
      totalSamplersForAllStages.bindslots[l] += perStage.bindslots[l];
    }
  }

  m_Report.append(tr("\n*** Sampler Bind Statistics ***\n\n"));

  for(auto s : indices<ShaderStage>())
  {
    m_Report.append(tr("%1 calls: %2, non-null sampler sets: %3, null sampler sets: %4\n")
                        .arg(m_Ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                        .arg(totalSamplersPerStage[s].calls)
                        .arg(totalSamplersPerStage[s].sets)
                        .arg(totalSamplersPerStage[s].nulls));
  }

  m_Report.append(tr("Total calls: %1, non-null sampler sets: %2, null sampler sets: %3\n")
                      .arg(totalSamplersForAllStages.calls)
                      .arg(totalSamplersForAllStages.sets)
                      .arg(totalSamplersForAllStages.nulls));

  m_Report.append(
      CreateSimpleIntegerHistogram(tr("Aggregate slot counts per invocation across all stages"),
                                   totalSamplersForAllStages.bindslots));
}

void StatisticsViewer::AppendResourceBindStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  // #mivance see AppendConstantBindStatistics
  const ResourceBindStats &reference = frameInfo.stats.resources[0];

  ResourceBindStats totalResourcesPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalResourcesPerStage, 0, sizeof(totalResourcesPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalResourcesPerStage[s].types.resize(reference.types.size());
    totalResourcesPerStage[s].bindslots.resize(reference.bindslots.size());
  }

  {
    const ResourceBindStats *resources = frameInfo.stats.resources;
    for(auto s : indices<ShaderStage>())
    {
      totalResourcesPerStage[s].calls += resources[s].calls;
      totalResourcesPerStage[s].sets += resources[s].sets;
      totalResourcesPerStage[s].nulls += resources[s].nulls;

      for(int z = 0; z < resources[s].types.count(); z++)
      {
        totalResourcesPerStage[s].types[z] += resources[s].types[z];
      }

      for(int l = 0; l < resources[s].bindslots.count(); l++)
      {
        totalResourcesPerStage[s].bindslots[l] += resources[s].bindslots[l];
      }
    }
  }

  ResourceBindStats totalResourcesForAllStages;
  memset(&totalResourcesForAllStages, 0, sizeof(totalResourcesForAllStages));
  totalResourcesForAllStages.types.resize(totalResourcesPerStage[0].types.size());
  totalResourcesForAllStages.bindslots.resize(totalResourcesPerStage[0].bindslots.size());

  for(auto s : indices<ShaderStage>())
  {
    ResourceBindStats perStage = totalResourcesPerStage[s];
    totalResourcesForAllStages.calls += perStage.calls;
    totalResourcesForAllStages.sets += perStage.sets;
    totalResourcesForAllStages.nulls += perStage.nulls;
    for(int t = 0; t < perStage.types.count(); t++)
    {
      totalResourcesForAllStages.types[t] += perStage.types[t];
    }
    for(int l = 0; l < perStage.bindslots.count(); l++)
    {
      totalResourcesForAllStages.bindslots[l] += perStage.bindslots[l];
    }
  }

  m_Report.append(tr("\n*** Resource Bind Statistics ***\n\n"));

  for(auto s : indices<ShaderStage>())
  {
    m_Report.append(tr("%1 calls: %2 non-null resource sets: %3 null resource sets: %4\n")
                        .arg(m_Ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                        .arg(totalResourcesPerStage[s].calls)
                        .arg(totalResourcesPerStage[s].sets)
                        .arg(totalResourcesPerStage[s].nulls));
  }

  m_Report.append(tr("Total calls: %1 non-null resource sets: %2 null resource sets: %3\n")
                      .arg(totalResourcesForAllStages.calls)
                      .arg(totalResourcesForAllStages.sets)
                      .arg(totalResourcesForAllStages.nulls));

  uint32_t maxCount = 0;
  int maxWithCount = 0;

  m_Report.append(tr("\nResource types across all stages:\n"));
  for(int s = 0; s < totalResourcesForAllStages.types.count(); s++)
  {
    uint32_t count = totalResourcesForAllStages.types[s];
    if(count > 0)
      maxWithCount = s;
    maxCount = qMax(maxCount, count);
  }

  for(int s = 0; s <= maxWithCount; s++)
  {
    uint32_t count = totalResourcesForAllStages.types[s];
    int slice = SliceForString(Stars, count, maxCount);
    TextureType type = (TextureType)s;
    m_Report.append(
        QFormatStr("%1: %2 %3\n").arg(ToQStr(type), 20).arg(Stars.left(slice)).arg(CountOrEmpty(count)));
  }

  m_Report.append(
      CreateSimpleIntegerHistogram(tr("Aggregate slot counts per invocation across all stages"),
                                   totalResourcesForAllStages.bindslots));
}

void StatisticsViewer::AppendUpdateStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  // #mivance see AppendConstantBindStatistics
  const ResourceUpdateStats &reference = frameInfo.stats.updates;

  ResourceUpdateStats totalUpdates;
  memset(&totalUpdates, 0, sizeof(totalUpdates));
  totalUpdates.types.resize(reference.types.size());
  totalUpdates.sizes.resize(reference.sizes.size());

  {
    ResourceUpdateStats updates = frameInfo.stats.updates;

    totalUpdates.calls += updates.calls;
    totalUpdates.clients += updates.clients;
    totalUpdates.servers += updates.servers;

    for(int t = 0; t < updates.types.count(); t++)
      totalUpdates.types[t] += updates.types[t];

    for(int t = 0; t < updates.sizes.count(); t++)
      totalUpdates.sizes[t] += updates.sizes[t];
  }

  m_Report.append(tr("\n*** Resource Update Statistics ***\n\n"));

  m_Report.append(tr("Total calls: %1, client-updated memory: %2, server-updated memory: %3\n")
                      .arg(totalUpdates.calls)
                      .arg(totalUpdates.clients)
                      .arg(totalUpdates.servers));

  m_Report.append(tr("\nUpdated resource types:\n"));
  uint32_t maxCount = 0;
  int maxWithValue = 0;
  for(int s = 1; s < totalUpdates.types.count(); s++)
  {
    uint32_t value = totalUpdates.types[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 1; s <= maxWithValue; s++)
  {
    uint32_t count = totalUpdates.types[s];
    int slice = SliceForString(Stars, count, maxCount);
    TextureType type = (TextureType)s;
    m_Report.append(
        QFormatStr("%1: %2 %3\n").arg(ToQStr(type), 20).arg(Stars.left(slice)).arg(CountOrEmpty(count)));
  }

  m_Report.append(tr("\nUpdated resource sizes:\n"));
  maxCount = 0;
  maxWithValue = 0;
  for(int s = 0; s < totalUpdates.sizes.count(); s++)
  {
    uint32_t value = totalUpdates.sizes[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 0; s <= maxWithValue; s++)
  {
    uint32_t count = totalUpdates.sizes[s];
    int slice = SliceForString(Stars, count, maxCount);
    m_Report.append(QFormatStr("%1: %2 %3\n")
                        .arg(Pow2IndexAsReadable(s), 8)
                        .arg(Stars.left(slice))
                        .arg(CountOrEmpty(count)));
  }
}

void StatisticsViewer::AppendBlendStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  BlendStats blends = frameInfo.stats.blends;
  m_Report.append(tr("\n*** Blend Statistics ***\n"));
  m_Report.append(
      tr("Blend calls: %1 non-null sets: %2, null (default) sets: %3, redundant sets: %4\n")
          .arg(blends.calls)
          .arg(blends.sets)
          .arg(blends.nulls)
          .arg(blends.redundants));
}

void StatisticsViewer::AppendDepthStencilStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  DepthStencilStats depths = frameInfo.stats.depths;
  m_Report.append(tr("\n*** Depth Stencil Statistics ***\n"));
  m_Report.append(tr("Depth/stencil calls: %1 non-null sets: %2, null (default) sets: "
                     "%3, redundant sets: %4\n")
                      .arg(depths.calls)
                      .arg(depths.sets)
                      .arg(depths.nulls)
                      .arg(depths.redundants));
}

void StatisticsViewer::AppendRasterizationStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  RasterizationStats rasters = frameInfo.stats.rasters;
  m_Report.append(tr("\n*** Rasterization Statistics ***\n"));
  m_Report.append(tr("Rasterization calls: %1 non-null sets: %2, null (default) sets: "
                     "%3, redundant sets: %4\n")
                      .arg(rasters.calls)
                      .arg(rasters.sets)
                      .arg(rasters.nulls)
                      .arg(rasters.redundants));
  m_Report.append(CreateSimpleIntegerHistogram(tr("Viewports set"), rasters.viewports));
  m_Report.append(CreateSimpleIntegerHistogram(tr("Scissors set"), rasters.rects));
}

void StatisticsViewer::AppendOutputStatistics()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  OutputTargetStats outputs = frameInfo.stats.outputs;
  m_Report.append(tr("\n*** Output Statistics ***\n"));
  m_Report.append(tr("Output calls: %1 non-null sets: %2, null sets: %3\n")
                      .arg(outputs.calls)
                      .arg(outputs.sets)
                      .arg(outputs.nulls));
  m_Report.append(CreateSimpleIntegerHistogram(tr("Outputs set"), outputs.bindslots));
}

void StatisticsViewer::AppendDetailedInformation()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  if(!frameInfo.stats.recorded)
    return;

  AppendDrawStatistics();
  AppendDispatchStatistics();
  AppendInputAssemblerStatistics();
  AppendShaderStatistics();
  AppendConstantBindStatistics();
  AppendSamplerBindStatistics();
  AppendResourceBindStatistics();
  AppendBlendStatistics();
  AppendDepthStencilStatistics();
  AppendRasterizationStatistics();
  AppendUpdateStatistics();
  AppendOutputStatistics();
}

void StatisticsViewer::CountContributingEvents(const DrawcallDescription &draw, uint32_t &drawCount,
                                               uint32_t &dispatchCount, uint32_t &diagnosticCount)
{
  const DrawFlags diagnosticMask =
      DrawFlags::SetMarker | DrawFlags::PushMarker | DrawFlags::PopMarker;
  DrawFlags diagnosticMasked = draw.flags & diagnosticMask;

  if(diagnosticMasked != DrawFlags::NoFlags)
    diagnosticCount += 1;

  if(draw.flags & DrawFlags::Drawcall)
    drawCount += 1;

  if(draw.flags & DrawFlags::Dispatch)
    dispatchCount += 1;

  for(const DrawcallDescription &c : draw.children)
    CountContributingEvents(c, drawCount, dispatchCount, diagnosticCount);
}

void StatisticsViewer::AppendAPICallSummary()
{
  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  if(!frameInfo.stats.recorded)
    return;

  uint32_t numConstantSets = 0;
  uint32_t numSamplerSets = 0;
  uint32_t numResourceSets = 0;
  uint32_t numShaderSets = 0;

  for(auto s : indices<ShaderStage>())
  {
    numConstantSets += frameInfo.stats.constants[s].calls;
    numSamplerSets += frameInfo.stats.samplers[s].calls;
    numResourceSets += frameInfo.stats.resources[s].calls;
    numShaderSets += frameInfo.stats.shaders[s].calls;
  }

  uint32_t numResourceUpdates = frameInfo.stats.updates.calls;
  uint32_t numIndexVertexSets = (frameInfo.stats.indices.calls + frameInfo.stats.vertices.calls +
                                 frameInfo.stats.layouts.calls);
  uint32_t numBlendSets = frameInfo.stats.blends.calls;
  uint32_t numDepthStencilSets = frameInfo.stats.depths.calls;
  uint32_t numRasterizationSets = frameInfo.stats.rasters.calls;
  uint32_t numOutputSets = frameInfo.stats.outputs.calls;

  m_Report += tr("\tIndex/vertex bind calls: %1\n").arg(numIndexVertexSets);
  m_Report += tr("\tConstant bind calls: %1\n").arg(numConstantSets);
  m_Report += tr("\tSampler bind calls: %1\n").arg(numSamplerSets);
  m_Report += tr("\tResource bind calls: %1\n").arg(numResourceSets);
  m_Report += tr("\tShader set calls: %1\n").arg(numShaderSets);
  m_Report += tr("\tBlend set calls: %1\n").arg(numBlendSets);
  m_Report += tr("\tDepth/stencil set calls: %1\n").arg(numDepthStencilSets);
  m_Report += tr("\tRasterization set calls: %1\n").arg(numRasterizationSets);
  m_Report += tr("\tResource update calls: %1\n").arg(numResourceUpdates);
  m_Report += tr("\tOutput set calls: %1\n").arg(numOutputSets);
}

void StatisticsViewer::GenerateReport()
{
  const rdcarray<DrawcallDescription> &curDraws = m_Ctx.CurDrawcalls();

  uint32_t drawCount = 0;
  uint32_t dispatchCount = 0;
  uint32_t diagnosticCount = 0;
  for(const DrawcallDescription &d : curDraws)
    CountContributingEvents(d, drawCount, dispatchCount, diagnosticCount);

  uint32_t numAPIcalls =
      m_Ctx.GetLastDrawcall()->eventId - (drawCount + dispatchCount + diagnosticCount);

  int numTextures = m_Ctx.GetTextures().count();
  int numBuffers = m_Ctx.GetBuffers().count();

  uint64_t IBBytes = 0;
  uint64_t VBBytes = 0;
  uint64_t BufBytes = 0;
  for(const BufferDescription &b : m_Ctx.GetBuffers())
  {
    BufBytes += b.length;

    if(b.creationFlags & BufferCategory::Index)
      IBBytes += b.length;
    if(b.creationFlags & BufferCategory::Vertex)
      VBBytes += b.length;
  }

  uint64_t RTBytes = 0;
  uint64_t TexBytes = 0;
  uint64_t LargeTexBytes = 0;

  int numRTs = 0;
  float texW = 0, texH = 0;
  float largeTexW = 0, largeTexH = 0;
  int texCount = 0, largeTexCount = 0;
  for(const TextureDescription &t : m_Ctx.GetTextures())
  {
    if(t.creationFlags & (TextureCategory::ColorTarget | TextureCategory::DepthTarget))
    {
      numRTs++;

      RTBytes += t.byteSize;
    }
    else
    {
      texW += (float)t.width;
      texH += (float)t.height;
      texCount++;

      TexBytes += t.byteSize;

      if(t.width > 32 && t.height > 32)
      {
        largeTexW += (float)t.width;
        largeTexH += (float)t.height;
        largeTexCount++;

        LargeTexBytes += t.byteSize;
      }
    }
  }

  if(texCount > 0)
  {
    texW /= texCount;
    texH /= texCount;
  }

  if(largeTexCount > 0)
  {
    largeTexW /= largeTexCount;
    largeTexH /= largeTexCount;
  }

  float drawRatio = 0.0f;
  if(drawCount + dispatchCount > 0)
    drawRatio = (float)numAPIcalls / (float)(drawCount + dispatchCount);

  const FrameDescription &frameInfo = m_Ctx.FrameInfo();

  float compressedMB = (float)frameInfo.compressedFileSize / (1024.0f * 1024.0f);
  float uncompressedMB = (float)frameInfo.uncompressedFileSize / (1024.0f * 1024.0f);
  float compressRatio = uncompressedMB / compressedMB;
  float persistentMB = (float)frameInfo.persistentSize / (1024.0f * 1024.0f);
  float initDataMB = (float)frameInfo.initDataSize / (1024.0f * 1024.0f);

  QString header =
      tr("Stats for %1.\n\nFile size: %2MB (%3MB uncompressed, compression ratio %4:1)\n"
         "Persistent Data (approx): %5MB, Frame-initial data (approx): %6MB\n")
          .arg(QFileInfo(m_Ctx.GetCaptureFilename()).fileName())
          .arg(compressedMB, 2, 'f', 2)
          .arg(uncompressedMB, 2, 'f', 2)
          .arg(compressRatio, 2, 'f', 2)
          .arg(persistentMB, 2, 'f', 2)
          .arg(initDataMB, 2, 'f', 2);
  QString drawList = tr("Draw calls: %1\nDispatch calls: %2\n").arg(drawCount).arg(dispatchCount);
  QString ratio = tr("API:Draw/Dispatch call ratio: %1\n\n").arg(drawRatio);
  QString textures = tr("%1 Textures - %2 MB (%3 MB over 32x32), %4 RTs - %5 MB.\n"
                        "Avg. tex dimension: %6x%7 (%8x%9 over 32x32)\n")
                         .arg(numTextures)
                         .arg((float)TexBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg((float)LargeTexBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg(numRTs)
                         .arg((float)RTBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg(texW)
                         .arg(texH)
                         .arg(largeTexW)
                         .arg(largeTexH);
  QString buffers = tr("%1 Buffers - %2 MB total %3 MB IBs %4 MB VBs.\n")
                        .arg(numBuffers)
                        .arg((float)BufBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                        .arg((float)IBBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                        .arg((float)VBBytes / (1024.0f * 1024.0f), 2, 'f', 2);
  QString load = tr("%1 MB - Grand total GPU buffer + texture load.\n")
                     .arg((float)(TexBytes + BufBytes + RTBytes) / (1024.0f * 1024.0f), 2, 'f', 2);

  m_Report = header;

  m_Report.append(tr("\n*** Summary ***\n\n"));
  m_Report.append(drawList);
  m_Report += tr("API calls: %1\n").arg(numAPIcalls);
  AppendAPICallSummary();
  m_Report.append(ratio);
  m_Report.append(textures);
  m_Report.append(buffers);
  m_Report.append(load);

  AppendDetailedInformation();
}

StatisticsViewer::StatisticsViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::StatisticsViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->statistics->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_Ctx.AddCaptureViewer(this);
}

StatisticsViewer::~StatisticsViewer()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void StatisticsViewer::OnCaptureClosed()
{
  ui->statistics->clear();
}

void StatisticsViewer::OnCaptureLoaded()
{
  GenerateReport();
  ui->statistics->setText(m_Report);
}