using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class StatisticsViewer : DockContent, ILogViewerForm
    {
        private Core m_Core;

        public StatisticsViewer(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

            statisticsLog.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
        }

        public void OnLogfileClosed()
        {
            statisticsLog.Clear();
        }

        private static readonly int HistogramWidth = 128;
        private static readonly string Stars = String.Concat(Enumerable.Repeat("*", HistogramWidth));

        private string Pow2IndexAsReadable(int index)
        {
            long value = 1L << index;

            if (value >= (1024 * 1024))
            {
                float slice = (float)value / (1024 * 1024);
                return String.Format("{0}MB", Formatter.Format(slice));
            }
            else if (value >= 1024)
            {
                float slice = (float)value / 1024;
                return String.Format("{0}KB", Formatter.Format(slice));
            }
            else
            {
                return String.Format("{0}B", Formatter.Format(value));
            }
        }

        private int SliceForString(string s, UInt32 value, UInt32 maximum)
        {
            if (value == 0 || maximum == 0)
                return 0;

            float ratio = (float)value / maximum;
            int slice = (int)(ratio * s.Length);
            return Math.Max(1, slice);
        }

        private string CountOrEmpty(UInt32 count)
        {
            if (count == 0)
                return "";
            else
                return String.Format("({0})", count);
        }

        private void AppendDrawStatistics(FetchFrameInfo frameInfo)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameDrawStats template = frameInfo.stats.draws;

            FetchFrameDrawStats totalUpdates = new FetchFrameDrawStats();
            totalUpdates.counts = new UInt32[template.counts.Length];

            {
                FetchFrameDrawStats draws = frameInfo.stats.draws;

                totalUpdates.calls += draws.calls;
                totalUpdates.instanced += draws.instanced;
                totalUpdates.indirect += draws.indirect;

                System.Diagnostics.Debug.Assert(totalUpdates.counts.Length == draws.counts.Length);
                for (var t = 0; t < draws.counts.Length; t++)
                    totalUpdates.counts[t] += draws.counts[t];
            }

            statisticsLog.AppendText("\n*** Draw Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total calls: {0}, instanced: {1}, indirect: {2}\n", totalUpdates.calls, totalUpdates.instanced, totalUpdates.indirect));

            if (totalUpdates.instanced > 0)
            {
                statisticsLog.AppendText("\nInstance counts:\n");
                UInt32 maxCount = 0;
                int maxWithValue = 0;
                int maximum = totalUpdates.counts.Length;
                for (var s = 1; s < maximum; s++)
                {
                    UInt32 value = totalUpdates.counts[s];
                    if (value > 0)
                        maxWithValue = s;
                    maxCount = Math.Max(maxCount, value);
                }

                for (var s = 1; s <= maxWithValue; s++)
                {
                    UInt32 count = totalUpdates.counts[s];
                    int slice = SliceForString(Stars, count, maxCount);
                    statisticsLog.AppendText(String.Format("{0,2}{1,2}: {2} {3}\n", (s == maximum - 1) ? ">=" : "", s, Stars.Substring(0, slice), CountOrEmpty(count)));
                }
            }
        }

        private void AppendDispatchStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameDispatchStats totalUpdates = new FetchFrameDispatchStats();

            {
                FetchFrameDispatchStats dispatches = frameInfo.stats.dispatches;

                totalUpdates.calls += dispatches.calls;
                totalUpdates.indirect += dispatches.indirect;
            }

            statisticsLog.AppendText("\n*** Dispatch Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total calls: {0}, indirect: {1}\n", totalUpdates.calls, totalUpdates.indirect));
        }

        private string CreateSimpleIntegerHistogram(string legend, UInt32[] array)
        {
            UInt32 maxCount = 0;
            int maxWithValue = 0;

            for (var o = 0; o < array.Length; o++)
            {
                UInt32 value = array[o];
                if (value > 0)
                    maxWithValue = o;
                maxCount = Math.Max(maxCount, value);
            }

            string text = String.Format("\n{0}:\n", legend);

            for (var o = 0; o <= maxWithValue; o++)
            {
                UInt32 count = array[o];
                int slice = SliceForString(Stars, count, maxCount);
                text += String.Format("{0,4}: {1} {2}\n", o, Stars.Substring(0, slice), CountOrEmpty(count));
            }

            return text;
        }

        private void AppendInputAssemblerStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameIndexBindStats totalIndexStats = new FetchFrameIndexBindStats();

            {
                FetchFrameIndexBindStats indices = frameInfo.stats.indices;

                totalIndexStats.calls += indices.calls;
                totalIndexStats.sets += indices.sets;
                totalIndexStats.nulls += indices.nulls;
            }

            FetchFrameLayoutBindStats totalLayoutStats = new FetchFrameLayoutBindStats();

            {
                FetchFrameLayoutBindStats layouts = frameInfo.stats.layouts;

                totalLayoutStats.calls += layouts.calls;
                totalLayoutStats.sets += layouts.sets;
                totalLayoutStats.nulls += layouts.nulls;
            }

            // #mivance see AppendConstantBindStatistics
            FetchFrameVertexBindStats template = frameInfo.stats.vertices;
            FetchFrameVertexBindStats totalVertexStats = new FetchFrameVertexBindStats();
            totalVertexStats.bindslots = new UInt32[template.bindslots.Length];

            {
                FetchFrameVertexBindStats vertices = frameInfo.stats.vertices;

                totalVertexStats.calls += vertices.calls;
                totalVertexStats.sets += vertices.sets;
                totalVertexStats.nulls += vertices.nulls;

                System.Diagnostics.Debug.Assert(totalVertexStats.bindslots.Length == vertices.bindslots.Length);
                for (var s = 0; s < vertices.bindslots.Length; s++)
                {
                    totalVertexStats.bindslots[s] += vertices.bindslots[s];
                }
            }

            statisticsLog.AppendText("\n*** Input Assembler Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total index calls: {0}, non-null index sets: {1}, null index sets: {2}\n", totalIndexStats.calls, totalIndexStats.sets, totalIndexStats.nulls));
            statisticsLog.AppendText(String.Format("Total layout calls: {0}, non-null layout sets: {1}, null layout sets: {2}\n", totalLayoutStats.calls, totalLayoutStats.sets, totalLayoutStats.nulls));
            statisticsLog.AppendText(String.Format("Total vertex calls: {0}, non-null vertex sets: {1}, null vertex sets: {2}\n", totalVertexStats.calls, totalVertexStats.sets, totalVertexStats.nulls));

            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Aggregate vertex slot counts per invocation", totalVertexStats.bindslots));
        }

        private void AppendShaderStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameShaderStats[] shaders = frameInfo.stats.shaders;
            FetchFrameShaderStats totalShadersPerStage = new FetchFrameShaderStats();
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalShadersPerStage.calls += shaders[s].calls;
                totalShadersPerStage.sets += shaders[s].sets;
                totalShadersPerStage.nulls += shaders[s].nulls;
                totalShadersPerStage.redundants += shaders[s].redundants;
            }

            statisticsLog.AppendText("\n*** Shader Set Statistics ***\n\n");

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                statisticsLog.AppendText(String.Format("{0} calls: {1}, non-null shader sets: {2}, null shader sets: {3}, redundant shader sets: {4}\n",
                                         m_Core.CurPipelineState.Abbrev((ShaderStageType)s), shaders[s].calls,
                                         shaders[s].sets, shaders[s].nulls, shaders[s].redundants));
            }

            statisticsLog.AppendText(String.Format("Total calls: {0}, non-null shader sets: {1}, null shader sets: {2}, reundant shader sets: {3}\n",
                                     totalShadersPerStage.calls, totalShadersPerStage.sets, totalShadersPerStage.nulls, totalShadersPerStage.redundants));
        }

        private void AppendConstantBindStatistics(FetchFrameInfo frameInfo)
        {
            // #mivance C++-side we guarantee all stages will have the same slots
            // and sizes count, so pattern off of the first frame's first stage
            FetchFrameConstantBindStats template = frameInfo.stats.constants[0];

            // #mivance there is probably a way to iterate the fields via
            // GetType()/GetField() and build a sort of dynamic min/max/average
            // structure for a given type with known integral types (or arrays
            // thereof), but given we're heading for a Qt/C++ rewrite of the UI
            // perhaps best not to dwell too long on that
            FetchFrameConstantBindStats[] totalConstantsPerStage = new FetchFrameConstantBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalConstantsPerStage[s] = new FetchFrameConstantBindStats();
                totalConstantsPerStage[s].bindslots = new UInt32[template.bindslots.Length];
                totalConstantsPerStage[s].sizes = new UInt32[template.sizes.Length];
            }

            {
                FetchFrameConstantBindStats[] constants = frameInfo.stats.constants;
                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    totalConstantsPerStage[s].calls += constants[s].calls;
                    totalConstantsPerStage[s].sets += constants[s].sets;
                    totalConstantsPerStage[s].nulls += constants[s].nulls;

                    System.Diagnostics.Debug.Assert(totalConstantsPerStage[s].bindslots.Length == constants[s].bindslots.Length);
                    for (var l = 0; l < constants[s].bindslots.Length; l++)
                    {
                        totalConstantsPerStage[s].bindslots[l] += constants[s].bindslots[l];
                    }

                    System.Diagnostics.Debug.Assert(totalConstantsPerStage[s].sizes.Length == constants[s].sizes.Length);
                    for (var z = 0; z < constants[s].sizes.Length; z++)
                    {
                        totalConstantsPerStage[s].sizes[z] += constants[s].sizes[z];
                    }
                }
            }

            FetchFrameConstantBindStats totalConstantsForAllStages = new FetchFrameConstantBindStats();
            totalConstantsForAllStages.bindslots = new UInt32[totalConstantsPerStage[0].bindslots.Length];
            totalConstantsForAllStages.sizes = new UInt32[totalConstantsPerStage[0].sizes.Length];

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                FetchFrameConstantBindStats perStage = totalConstantsPerStage[s];
                totalConstantsForAllStages.calls += perStage.calls;
                totalConstantsForAllStages.sets += perStage.sets;
                totalConstantsForAllStages.nulls += perStage.nulls;
                for (var l = 0; l < perStage.bindslots.Length; l++)
                {
                    totalConstantsForAllStages.bindslots[l] += perStage.bindslots[l];
                }
                for (var z = 0; z < perStage.sizes.Length; z++)
                {
                    totalConstantsForAllStages.sizes[z] += perStage.sizes[z];
                }
            }

            statisticsLog.AppendText("\n*** Constant Bind Statistics ***\n\n");

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                statisticsLog.AppendText(String.Format("{0} calls: {1}, non-null buffer sets: {2}, null buffer sets: {3}\n",
                                         m_Core.CurPipelineState.Abbrev((ShaderStageType)s), totalConstantsPerStage[s].calls,
                                         totalConstantsPerStage[s].sets, totalConstantsPerStage[s].nulls));
            }

            statisticsLog.AppendText(String.Format("Total calls: {0}, non-null buffer sets: {1}, null buffer sets: {2}\n",
                                     totalConstantsForAllStages.calls, totalConstantsForAllStages.sets, totalConstantsForAllStages.nulls));

            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Aggregate slot counts per invocation across all stages", totalConstantsForAllStages.bindslots));

            statisticsLog.AppendText("\nAggregate constant buffer sizes across all stages:\n");
            UInt32 maxCount = 0;
            int maxWithValue = 0;
            for (var s = 0; s < totalConstantsForAllStages.sizes.Length; s++)
            {
                UInt32 value = totalConstantsForAllStages.sizes[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 0; s <= maxWithValue; s++)
            {
                UInt32 count = totalConstantsForAllStages.sizes[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,8}: {1} {2}\n", Pow2IndexAsReadable(s), Stars.Substring(0, slice), CountOrEmpty(count)));
            }
        }

        private void AppendSamplerBindStatistics(FetchFrameInfo frameInfo)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameSamplerBindStats template = frameInfo.stats.samplers[0];

            FetchFrameSamplerBindStats[] totalSamplersPerStage = new FetchFrameSamplerBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalSamplersPerStage[s] = new FetchFrameSamplerBindStats();
                totalSamplersPerStage[s].bindslots = new UInt32[template.bindslots.Length];
            }

            {
                FetchFrameSamplerBindStats[] resources = frameInfo.stats.samplers;
                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    totalSamplersPerStage[s].calls += resources[s].calls;
                    totalSamplersPerStage[s].sets += resources[s].sets;
                    totalSamplersPerStage[s].nulls += resources[s].nulls;

                    System.Diagnostics.Debug.Assert(totalSamplersPerStage[s].bindslots.Length == resources[s].bindslots.Length);
                    for (var l = 0; l < resources[s].bindslots.Length; l++)
                    {
                        totalSamplersPerStage[s].bindslots[l] += resources[s].bindslots[l];
                    }
                }
            }

            FetchFrameSamplerBindStats totalSamplersForAllStages = new FetchFrameSamplerBindStats();
            totalSamplersForAllStages.bindslots = new UInt32[totalSamplersPerStage[0].bindslots.Length];

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                FetchFrameSamplerBindStats perStage = totalSamplersPerStage[s];
                totalSamplersForAllStages.calls += perStage.calls;
                totalSamplersForAllStages.sets += perStage.sets;
                totalSamplersForAllStages.nulls += perStage.nulls;
                for (var l = 0; l < perStage.bindslots.Length; l++)
                {
                    totalSamplersForAllStages.bindslots[l] += perStage.bindslots[l];
                }
            }

            statisticsLog.AppendText("\n*** Sampler Bind Statistics ***\n\n");

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                statisticsLog.AppendText(String.Format("{0} calls: {1}, non-null sampler sets: {2}, null sampler sets: {3}\n",
                                         m_Core.CurPipelineState.Abbrev((ShaderStageType)s), totalSamplersPerStage[s].calls,
                                         totalSamplersPerStage[s].sets, totalSamplersPerStage[s].nulls));
            }

            statisticsLog.AppendText(String.Format("Total calls: {0}, non-null sampler sets: {1}, null sampler sets: {2}\n",
                                     totalSamplersForAllStages.calls, totalSamplersForAllStages.sets, totalSamplersForAllStages.nulls));

            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Aggregate slot counts per invocation across all stages", totalSamplersForAllStages.bindslots));
        }

        private void AppendResourceBindStatistics(FetchFrameInfo frameInfo)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameResourceBindStats template = frameInfo.stats.resources[0];

            FetchFrameResourceBindStats[] totalResourcesPerStage = new FetchFrameResourceBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalResourcesPerStage[s] = new FetchFrameResourceBindStats();
                totalResourcesPerStage[s].types = new UInt32[template.types.Length];
                totalResourcesPerStage[s].bindslots = new UInt32[template.bindslots.Length];
            }

            {
                FetchFrameResourceBindStats[] resources = frameInfo.stats.resources;
                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    totalResourcesPerStage[s].calls += resources[s].calls;
                    totalResourcesPerStage[s].sets += resources[s].sets;
                    totalResourcesPerStage[s].nulls += resources[s].nulls;

                    System.Diagnostics.Debug.Assert(totalResourcesPerStage[s].types.Length == resources[s].types.Length);
                    for (var z = 0; z < resources[s].types.Length; z++)
                    {
                        totalResourcesPerStage[s].types[z] += resources[s].types[z];
                    }

                    System.Diagnostics.Debug.Assert(totalResourcesPerStage[s].bindslots.Length == resources[s].bindslots.Length);
                    for (var l = 0; l < resources[s].bindslots.Length; l++)
                    {
                        totalResourcesPerStage[s].bindslots[l] += resources[s].bindslots[l];
                    }
                }
            }

            FetchFrameResourceBindStats totalResourcesForAllStages = new FetchFrameResourceBindStats();
            totalResourcesForAllStages.types = new UInt32[totalResourcesPerStage[0].types.Length];
            totalResourcesForAllStages.bindslots = new UInt32[totalResourcesPerStage[0].bindslots.Length];

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                FetchFrameResourceBindStats perStage = totalResourcesPerStage[s];
                totalResourcesForAllStages.calls += perStage.calls;
                totalResourcesForAllStages.sets += perStage.sets;
                totalResourcesForAllStages.nulls += perStage.nulls;
                for (var t = 0; t < perStage.types.Length; t++)
                {
                    totalResourcesForAllStages.types[t] += perStage.types[t];
                }
                for (var l = 0; l < perStage.bindslots.Length; l++)
                {
                    totalResourcesForAllStages.bindslots[l] += perStage.bindslots[l];
                }
            }

            statisticsLog.AppendText("\n*** Resource Bind Statistics ***\n\n");

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                statisticsLog.AppendText(String.Format("{0} calls: {1} non-null resource sets: {2} null resource sets: {3}\n",
                                         m_Core.CurPipelineState.Abbrev((ShaderStageType)s), totalResourcesPerStage[s].calls,
                                         totalResourcesPerStage[s].sets, totalResourcesPerStage[s].nulls));
            }

            statisticsLog.AppendText(String.Format("Total calls: {0} non-null resource sets: {1} null resource sets: {2}\n",
                                     totalResourcesForAllStages.calls, totalResourcesForAllStages.sets,
                                     totalResourcesForAllStages.nulls));

            UInt32 maxCount = 0;
            int maxWithCount = 0;

            statisticsLog.AppendText("\nResource types across all stages:\n");
            for (var s = 0; s < totalResourcesForAllStages.types.Length; s++)
            {
                UInt32 count = totalResourcesForAllStages.types[s];
                if (count > 0)
                    maxWithCount = s;
                maxCount = Math.Max(maxCount, count);
            }

            for (var s = 0; s <= maxWithCount; s++)
            {
                UInt32 count = totalResourcesForAllStages.types[s];
                int slice = SliceForString(Stars, count, maxCount);
                ShaderResourceType type = (ShaderResourceType)s;
                statisticsLog.AppendText(String.Format("{0,16}: {1} {2}\n", type.ToString(), Stars.Substring(0, slice), CountOrEmpty(count)));
            }

            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Aggregate slot counts per invocation across all stages", totalResourcesForAllStages.bindslots));
        }

        private void AppendUpdateStatistics(FetchFrameInfo frameInfo)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameUpdateStats template = frameInfo.stats.updates;

            FetchFrameUpdateStats totalUpdates = new FetchFrameUpdateStats();
            totalUpdates.types = new UInt32[template.types.Length];
            totalUpdates.sizes = new UInt32[template.sizes.Length];

            {
                FetchFrameUpdateStats updates = frameInfo.stats.updates;

                totalUpdates.calls += updates.calls;
                totalUpdates.clients += updates.clients;
                totalUpdates.servers += updates.servers;

                System.Diagnostics.Debug.Assert(totalUpdates.types.Length == updates.types.Length);
                for (var t = 0; t < updates.types.Length; t++)
                    totalUpdates.types[t] += updates.types[t];

                System.Diagnostics.Debug.Assert(totalUpdates.sizes.Length == updates.sizes.Length);
                for (var t = 0; t < updates.sizes.Length; t++)
                    totalUpdates.sizes[t] += updates.sizes[t];
            }

            statisticsLog.AppendText("\n*** Resource Update Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total calls: {0}, client-updated memory: {1}, server-updated memory: {2}\n", totalUpdates.calls, totalUpdates.clients, totalUpdates.servers));

            statisticsLog.AppendText("\nUpdated resource types:\n");
            UInt32 maxCount = 0;
            int maxWithValue = 0;
            for (var s = 1; s < totalUpdates.types.Length; s++)
            {
                UInt32 value = totalUpdates.types[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 1; s <= maxWithValue; s++)
            {
                UInt32 count = totalUpdates.types[s];
                int slice = SliceForString(Stars, count, maxCount);
                ShaderResourceType type = (ShaderResourceType)s;
                statisticsLog.AppendText(String.Format("{0,16}: {1} {2}\n", type.ToString(), Stars.Substring(0, slice), CountOrEmpty(count)));
            }

            statisticsLog.AppendText("\nUpdated resource sizes:\n");
            maxCount = 0;
            maxWithValue = 0;
            for (var s = 0; s < totalUpdates.sizes.Length; s++)
            {
                UInt32 value = totalUpdates.sizes[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 0; s <= maxWithValue; s++)
            {
                UInt32 count = totalUpdates.sizes[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,8}: {1} {2}\n", Pow2IndexAsReadable(s), Stars.Substring(0, slice), CountOrEmpty(count)));
            }
        }

        private void AppendBlendStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameBlendStats blends = frameInfo.stats.blends;
            statisticsLog.AppendText("\n*** Blend Statistics ***\n");
            statisticsLog.AppendText(String.Format("Blend calls: {0} non-null sets: {1} null (default) sets: {2} redundant sets: {3}\n", blends.calls, blends.sets, blends.nulls, blends.redundants));
        }

        private void AppendDepthStencilStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameDepthStencilStats depths = frameInfo.stats.depths;
            statisticsLog.AppendText("\n*** Depth Stencil Statistics ***\n");
            statisticsLog.AppendText(String.Format("Depth/stencil calls: {0} non-null sets: {1}, null (default) sets: {2}, redundant sets: {3}\n", depths.calls, depths.sets, depths.nulls, depths.redundants));
        }

        private void AppendRasterizationStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameRasterizationStats rasters = frameInfo.stats.rasters;
            statisticsLog.AppendText("\n*** Rasterization Statistics ***\n");
            statisticsLog.AppendText(String.Format("Rasterization calls: {0} non-null sets: {1}, null (default) sets: {2}, redundant sets: {3}\n", rasters.calls, rasters.sets, rasters.nulls, rasters.redundants));
            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Viewports set", rasters.viewports));
            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Scissors set", rasters.rects));
        }

        private void AppendOutputStatistics(FetchFrameInfo frameInfo)
        {
            FetchFrameOutputStats outputs = frameInfo.stats.outputs;
            statisticsLog.AppendText("\n*** Output Statistics ***\n");
            statisticsLog.AppendText(String.Format("Output calls: {0} non-null sets: {1}, null sets: {2}\n", outputs.calls, outputs.sets, outputs.nulls));
            statisticsLog.AppendText(CreateSimpleIntegerHistogram("Outputs set", outputs.bindslots));
        }

        private void AppendDetailedInformation(FetchFrameInfo frameInfo)
        {
            if (frameInfo.stats.recorded == 0)
                return;

            AppendDrawStatistics(frameInfo);
            AppendDispatchStatistics(frameInfo);
            AppendInputAssemblerStatistics(frameInfo);
            AppendShaderStatistics(frameInfo);
            AppendConstantBindStatistics(frameInfo);
            AppendSamplerBindStatistics(frameInfo);
            AppendResourceBindStatistics(frameInfo);
            AppendBlendStatistics(frameInfo);
            AppendDepthStencilStatistics(frameInfo);
            AppendRasterizationStatistics(frameInfo);
            AppendUpdateStatistics(frameInfo);
            AppendOutputStatistics(frameInfo);
        }

        private void CountContributingEvents(FetchDrawcall draw, ref uint drawCount, ref uint dispatchCount, ref uint diagnosticCount)
        {
            const uint diagnosticMask = (uint)DrawcallFlags.SetMarker | (uint)DrawcallFlags.PushMarker | (uint)DrawcallFlags.PopMarker;
            uint diagnosticMasked = (uint)draw.flags & diagnosticMask;

            if (diagnosticMasked != 0)
                diagnosticCount += 1;

            if ((draw.flags & DrawcallFlags.Drawcall) != 0)
                drawCount += 1;

            if ((draw.flags & DrawcallFlags.Dispatch) != 0)
                dispatchCount += 1;

            if (draw.children != null)
            {
                foreach (var c in draw.children)
                    CountContributingEvents(c, ref drawCount, ref dispatchCount, ref diagnosticCount);
            }
        }

        public string AppendAPICallSummary(FetchFrameInfo frameInfo, uint numAPICalls)
        {
            if (frameInfo.stats.recorded == 0)
                return "";

            uint numConstantSets = 0;
            uint numSamplerSets = 0;
            uint numResourceSets = 0;
            uint numShaderSets = 0;

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                numConstantSets += frameInfo.stats.constants[s].calls;
                numSamplerSets += frameInfo.stats.samplers[s].calls;
                numResourceSets += frameInfo.stats.resources[s].calls;
                numShaderSets += frameInfo.stats.shaders[s].calls;
            }

            uint numResourceUpdates = frameInfo.stats.updates.calls;
            uint numIndexVertexSets = (frameInfo.stats.indices.calls + frameInfo.stats.vertices.calls + frameInfo.stats.layouts.calls);
            uint numDraws = frameInfo.stats.draws.calls;
            uint numDispatches = frameInfo.stats.dispatches.calls;
            uint numBlendSets = frameInfo.stats.blends.calls;
            uint numDepthStencilSets = frameInfo.stats.depths.calls;
            uint numRasterizationSets = frameInfo.stats.rasters.calls;
            uint numOutputSets = frameInfo.stats.outputs.calls;

            string calls = "";
            calls += String.Format("API calls: {0}\n", numAPICalls);
            calls += String.Format("\tIndex/vertex bind calls: {0}\n", numIndexVertexSets);
            calls += String.Format("\tConstant bind calls: {0}\n", numConstantSets);
            calls += String.Format("\tSampler bind calls: {0}\n", numSamplerSets);
            calls += String.Format("\tResource bind calls: {0}\n", numResourceSets);
            calls += String.Format("\tShader set calls: {0}\n", numShaderSets);
            calls += String.Format("\tBlend set calls: {0}\n", numBlendSets);
            calls += String.Format("\tDepth/stencil set calls: {0}\n", numDepthStencilSets);
            calls += String.Format("\tRasterization set calls: {0}\n", numRasterizationSets);
            calls += String.Format("\tResource update calls: {0}\n", numResourceUpdates);
            calls += String.Format("\tOutput set calls: {0}\n", numOutputSets);
            return calls;
        }

        public void OnLogfileLoaded()
        {
            statisticsLog.Clear();

            var lastDraw = m_Core.CurDrawcalls[m_Core.CurDrawcalls.Length - 1];
            while (lastDraw.children != null && lastDraw.children.Length > 0)
                lastDraw = lastDraw.children[lastDraw.children.Length - 1];

            uint drawCount = 0;
            uint dispatchCount = 0;
            uint diagnosticCount = 0;
            foreach (var d in m_Core.CurDrawcalls)
                CountContributingEvents(d, ref drawCount, ref dispatchCount, ref diagnosticCount);

            uint numAPIcalls = lastDraw.eventID - (drawCount + dispatchCount + diagnosticCount);

            int numTextures = m_Core.CurTextures.Length;
            int numBuffers = m_Core.CurBuffers.Length;

            ulong IBBytes = 0;
            ulong VBBytes = 0;
            ulong BufBytes = 0;
            foreach (var b in m_Core.CurBuffers)
            {
                BufBytes += b.length;

                if ((b.creationFlags & BufferCreationFlags.IB) != 0)
                    IBBytes += b.length;
                if ((b.creationFlags & BufferCreationFlags.VB) != 0)
                    VBBytes += b.length;
            }

            ulong RTBytes = 0;
            ulong TexBytes = 0;
            ulong LargeTexBytes = 0;

            int numRTs = 0;
            float texW = 0, texH = 0;
            float largeTexW = 0, largeTexH = 0;
            int texCount = 0, largeTexCount = 0;
            foreach (var t in m_Core.CurTextures)
            {
                if ((t.creationFlags & (TextureCreationFlags.RTV | TextureCreationFlags.DSV)) != 0)
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

                    if (t.width > 32 && t.height > 32)
                    {
                        largeTexW += (float)t.width;
                        largeTexH += (float)t.height;
                        largeTexCount++;

                        LargeTexBytes += t.byteSize;
                    }
                }
            }

            texW /= texCount;
            texH /= texCount;

            largeTexW /= largeTexCount;
            largeTexH /= largeTexCount;

            FetchFrameInfo frameInfo = m_Core.FrameInfo;

            UInt64 persistentData = frameInfo.persistentSize;

            float compressedMB = (float)frameInfo.compressedFileSize / (1024.0f * 1024.0f);
            float uncompressedMB = (float)frameInfo.uncompressedFileSize / (1024.0f * 1024.0f);
            float compressRatio = uncompressedMB / compressedMB;
            float persistentMB = (float)frameInfo.persistentSize / (1024.0f * 1024.0f);
            float initDataMB = (float)frameInfo.initDataSize / (1024.0f * 1024.0f);

            string header = String.Format("Stats for {0}.\n\nFile size: {1:N2}MB ({2:N2}MB uncompressed, compression ratio {3:N2}:1)\nPersistent Data (approx): {4:N2}MB, Frame-initial data (approx): {5:N2}MB\n",
                              Helpers.SafeGetFileName(m_Core.LogFileName), compressedMB, uncompressedMB, compressRatio, persistentMB, initDataMB);
            string draws = String.Format("Draw calls: {0}\nDispatch calls: {1}\n",
                              drawCount, dispatchCount);
            string calls = AppendAPICallSummary(frameInfo, numAPIcalls);
            string ratio = String.Format("API:Draw/Dispatch call ratio: {0}\n\n", (float)numAPIcalls / (float)(drawCount + dispatchCount));
            string textures = String.Format("{0} Textures - {1:N2} MB ({2:N2} MB over 32x32), {3} RTs - {4:N2} MB.\nAvg. tex dimension: {5}x{6} ({7}x{8} over 32x32)\n",
                              numTextures, (float)TexBytes / (1024.0f * 1024.0f), (float)LargeTexBytes / (1024.0f * 1024.0f),
                              numRTs, (float)RTBytes / (1024.0f * 1024.0f),
                              texW, texH, largeTexW, largeTexH);
            string buffers = String.Format("{0} Buffers - {1:N2} MB total {2:N2} MB IBs {3:N2} MB VBs.\n",
                             numBuffers, (float)BufBytes / (1024.0f * 1024.0f), (float)IBBytes / (1024.0f * 1024.0f), (float)VBBytes / (1024.0f * 1024.0f));
            string load = String.Format("{0} MB - Grand total GPU buffer + texture load.\n", (float)(TexBytes + BufBytes + RTBytes) / (1024.0f * 1024.0f));

            statisticsLog.AppendText(header);

            statisticsLog.AppendText("\n*** Summary ***\n\n");
            statisticsLog.AppendText(draws);
            statisticsLog.AppendText(calls);
            statisticsLog.AppendText(ratio);
            statisticsLog.AppendText(textures);
            statisticsLog.AppendText(buffers);
            statisticsLog.AppendText(load);

            AppendDetailedInformation(frameInfo);

            statisticsLog.Select(0, 0);
        }

        public void OnEventSelected(UInt32 eventID)
        {

        }

        private void StatisticsViewer_Load(object sender, EventArgs e)
        {

        }
    }
}
