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
            if (value == 0)
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

        private void AppendDrawStatistics(FetchFrameInfo[] frameList)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameDrawStats template = frameList[0].stats.draws;

            FetchFrameDrawStats totalUpdates = new FetchFrameDrawStats();
            totalUpdates.counts = new UInt32[template.counts.Length];

            foreach (var f in frameList)
            {
                FetchFrameDrawStats draws = f.stats.draws;

                totalUpdates.calls += draws.calls;
                totalUpdates.instanced += draws.instanced;
                totalUpdates.indirect += draws.indirect;

                System.Diagnostics.Debug.Assert(totalUpdates.counts.Length == draws.counts.Length);
                for (var t = 0; t < draws.counts.Length; t++)
                    totalUpdates.counts[t] += draws.counts[t];
            }

            statisticsLog.AppendText("\n*** Draw Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total calls: {0}, instanced: {1}, indirect: {2}\n", totalUpdates.calls, totalUpdates.instanced, totalUpdates.indirect));

            if ( totalUpdates.instanced > 0 )
            {
                statisticsLog.AppendText("\nHistogram of instance counts:\n");
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

        private void AppendDispatchStatistics(FetchFrameInfo[] frameList)
        {
            FetchFrameDispatchStats totalUpdates = new FetchFrameDispatchStats();

            foreach (var f in frameList)
            {
                FetchFrameDispatchStats dispatches = f.stats.dispatches;

                totalUpdates.calls += dispatches.calls;
                totalUpdates.indirect += dispatches.indirect;
            }

            statisticsLog.AppendText("\n*** Dispatch Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total calls: {0}, indirect: {1}\n", totalUpdates.calls, totalUpdates.indirect));
        }

        private void AppendInputAssemblerStatistics(FetchFrameInfo[] frameList)
        {
            FetchFrameIndexBindStats totalIndexStats = new FetchFrameIndexBindStats();

            foreach (var f in frameList)
            {
                FetchFrameIndexBindStats indices = f.stats.indices;

                totalIndexStats.calls += indices.calls;
                totalIndexStats.sets += indices.sets;
                totalIndexStats.nulls += indices.nulls;
            }

            FetchFrameLayoutBindStats totalLayoutStats = new FetchFrameLayoutBindStats();

            foreach (var f in frameList)
            {
                FetchFrameLayoutBindStats layouts = f.stats.layouts;

                totalLayoutStats.calls += layouts.calls;
                totalLayoutStats.sets += layouts.sets;
                totalLayoutStats.nulls += layouts.nulls;
            }

            // #mivance see AppendConstantBindStatistics
            FetchFrameVertexBindStats template = frameList[0].stats.vertices;
            FetchFrameVertexBindStats totalVertexStats = new FetchFrameVertexBindStats();
            totalVertexStats.slots = new UInt32[template.slots.Length];

            foreach (var f in frameList)
            {
                FetchFrameVertexBindStats vertices = f.stats.vertices;

                totalVertexStats.calls += vertices.calls;
                totalVertexStats.sets += vertices.sets;
                totalVertexStats.nulls += vertices.nulls;

                System.Diagnostics.Debug.Assert(totalVertexStats.slots.Length == vertices.slots.Length);
                for (var s = 0; s < vertices.slots.Length; s++)
                {
                    totalVertexStats.slots[s] += vertices.slots[s];
                }
            }

            statisticsLog.AppendText("\n*** Input Assembler Statistics ***\n\n");

            statisticsLog.AppendText(String.Format("Total index calls: {0}, non-null index sets: {1}, null index sets: {2}\n", totalIndexStats.calls, totalIndexStats.sets, totalIndexStats.nulls));
            statisticsLog.AppendText(String.Format("Total layout calls: {0}, non-null layout sets: {1}, null layout sets: {2}\n", totalLayoutStats.calls, totalLayoutStats.sets, totalLayoutStats.nulls));
            statisticsLog.AppendText(String.Format("Total vertex calls: {0}, non-null vertex sets: {1}, null vertex sets: {2}\n", totalVertexStats.calls, totalVertexStats.sets, totalVertexStats.nulls));

            statisticsLog.AppendText("\nHistogram of aggregate vertex slot counts per invocation:\n");
            UInt32 maxCount = 0;
            int maxWithValue = 0;
            for (var s = 1; s < totalVertexStats.slots.Length; s++)
            {
                UInt32 value = totalVertexStats.slots[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 1; s <= maxWithValue; s++)
            {
                UInt32 count = totalVertexStats.slots[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,2}: {1} {2}\n", s, Stars.Substring(0, slice), CountOrEmpty(count)));
            }
        }

        private void AppendConstantBindStatistics(FetchFrameInfo[] frameList)
        {
            System.Diagnostics.Debug.Assert(frameList.Length > 0);

            // #mivance C++-side we guarantee all stages will have the same slots
            // and sizes count, so pattern off of the first frame's first stage
            FetchFrameConstantBindStats template = frameList[0].stats.constants[0];

            // #mivance there is probably a way to iterate the fields via
            // GetType()/GetField() and build a sort of dynamic min/max/average
            // structure for a given type with known integral types (or arrays
            // thereof), but given we're heading for a Qt/C++ rewrite of the UI
            // perhaps best not to dwell too long on that
            FetchFrameConstantBindStats[] totalConstantsPerStage = new FetchFrameConstantBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalConstantsPerStage[s] = new FetchFrameConstantBindStats();
                totalConstantsPerStage[s].slots = new UInt32[template.slots.Length];
                totalConstantsPerStage[s].sizes = new UInt32[template.sizes.Length];
            }

            foreach (var f in frameList)
            {
                FetchFrameConstantBindStats[] constants = f.stats.constants;
                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    totalConstantsPerStage[s].calls += constants[s].calls;
                    totalConstantsPerStage[s].sets += constants[s].sets;
                    totalConstantsPerStage[s].nulls += constants[s].nulls;

                    System.Diagnostics.Debug.Assert(totalConstantsPerStage[s].slots.Length == constants[s].slots.Length);
                    for (var l = 0; l < constants[s].slots.Length; l++)
                    {
                        totalConstantsPerStage[s].slots[l] += constants[s].slots[l];
                    }

                    System.Diagnostics.Debug.Assert(totalConstantsPerStage[s].sizes.Length == constants[s].sizes.Length);
                    for (var z = 0; z < constants[s].sizes.Length; z++)
                    {
                        totalConstantsPerStage[s].sizes[z] += constants[s].sizes[z];
                    }
                }
            }

            FetchFrameConstantBindStats totalConstantsForAllStages = new FetchFrameConstantBindStats();
            totalConstantsForAllStages.slots = new UInt32[totalConstantsPerStage[0].slots.Length];
            totalConstantsForAllStages.sizes = new UInt32[totalConstantsPerStage[0].sizes.Length];

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                FetchFrameConstantBindStats perStage = totalConstantsPerStage[s];
                totalConstantsForAllStages.calls += perStage.calls;
                totalConstantsForAllStages.sets += perStage.sets;
                totalConstantsForAllStages.nulls += perStage.nulls;
                for (var l = 0; l < perStage.slots.Length; l++)
                {
                    totalConstantsForAllStages.slots[l] += perStage.slots[l];
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

            statisticsLog.AppendText("\nHistogram of aggregate slot counts per invocation across all stages:\n");
            UInt32 maxCount = 0;
            int maxWithValue = 0;
            for (var s = 1; s < totalConstantsForAllStages.slots.Length; s++)
            {
                UInt32 value = totalConstantsForAllStages.slots[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 1; s <= maxWithValue; s++)
            {
                UInt32 count = totalConstantsForAllStages.slots[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,2}: {1} {2}\n", s, Stars.Substring(0, slice), CountOrEmpty(count)));
            }

            statisticsLog.AppendText("\nHistogram of aggregate constant buffer sizes across all stages:\n");
            maxCount = 0;
            maxWithValue = 0;
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

        private void AppendSamplerBindStatistics(FetchFrameInfo[] frameList)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameSamplerBindStats template = frameList[0].stats.samplers[0];

            FetchFrameSamplerBindStats[] totalSamplersPerStage = new FetchFrameSamplerBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalSamplersPerStage[s] = new FetchFrameSamplerBindStats();
                totalSamplersPerStage[s].slots = new UInt32[template.slots.Length];
            }

            foreach (var f in frameList)
            {
                FetchFrameSamplerBindStats[] resources = f.stats.samplers;
                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    totalSamplersPerStage[s].calls += resources[s].calls;
                    totalSamplersPerStage[s].sets += resources[s].sets;
                    totalSamplersPerStage[s].nulls += resources[s].nulls;

                    System.Diagnostics.Debug.Assert(totalSamplersPerStage[s].slots.Length == resources[s].slots.Length);
                    for (var l = 0; l < resources[s].slots.Length; l++)
                    {
                        totalSamplersPerStage[s].slots[l] += resources[s].slots[l];
                    }
                }
            }

            FetchFrameSamplerBindStats totalSamplersForAllStages = new FetchFrameSamplerBindStats();
            totalSamplersForAllStages.slots = new UInt32[totalSamplersPerStage[0].slots.Length];

            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                FetchFrameSamplerBindStats perStage = totalSamplersPerStage[s];
                totalSamplersForAllStages.calls += perStage.calls;
                totalSamplersForAllStages.sets += perStage.sets;
                totalSamplersForAllStages.nulls += perStage.nulls;
                for (var l = 0; l < perStage.slots.Length; l++)
                {
                    totalSamplersForAllStages.slots[l] += perStage.slots[l];
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

            statisticsLog.AppendText("\nHistogram of aggregate slot counts per invocation across all stages:\n");
            UInt32 maxCount = 0;
            int maxWithValue = 0;
            for (var s = 1; s < totalSamplersForAllStages.slots.Length; s++)
            {
                UInt32 value = totalSamplersForAllStages.slots[s];
                if (value > 0)
                    maxWithValue = s;
                maxCount = Math.Max(maxCount, value);
            }

            for (var s = 1; s <= maxWithValue; s++)
            {
                UInt32 count = totalSamplersForAllStages.slots[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,2}: {1} {2}\n", s, Stars.Substring(0, slice), CountOrEmpty(count)));
            }
        }

        private void AppendResourceBindStatistics(FetchFrameInfo[] frameList)
        {
            System.Diagnostics.Debug.Assert(frameList.Length > 0);

            // #mivance see AppendConstantBindStatistics
            FetchFrameResourceBindStats template = frameList[0].stats.resources[0];

            FetchFrameResourceBindStats[] totalResourcesPerStage = new FetchFrameResourceBindStats[(int)ShaderStageType.Count];
            for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
            {
                totalResourcesPerStage[s] = new FetchFrameResourceBindStats();
                totalResourcesPerStage[s].types = new UInt32[template.types.Length];
                totalResourcesPerStage[s].slots = new UInt32[template.slots.Length];
            }

            foreach (var f in frameList)
            {
                FetchFrameResourceBindStats[] resources = f.stats.resources;
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

                    System.Diagnostics.Debug.Assert(totalResourcesPerStage[s].slots.Length == resources[s].slots.Length);
                    for (var l = 0; l < resources[s].slots.Length; l++)
                    {
                        totalResourcesPerStage[s].slots[l] += resources[s].slots[l];
                    }
                }
            }

            FetchFrameResourceBindStats totalResourcesForAllStages = new FetchFrameResourceBindStats();
            totalResourcesForAllStages.types = new UInt32[totalResourcesPerStage[0].types.Length];
            totalResourcesForAllStages.slots = new UInt32[totalResourcesPerStage[0].slots.Length];

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
                for (var l = 0; l < perStage.slots.Length; l++)
                {
                    totalResourcesForAllStages.slots[l] += perStage.slots[l];
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

            statisticsLog.AppendText("\nHistogram of resource types across all stages:\n");
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

            maxCount = 0;
            maxWithCount = 0;

            statisticsLog.AppendText("\nHistogram of aggregate slot counts per invocation across all stages:\n");
            for (var s = 1; s < totalResourcesForAllStages.slots.Length; s++)
            {
                UInt32 count = totalResourcesForAllStages.slots[s];
                if (count > 0)
                    maxWithCount = s;
                maxCount = Math.Max(maxCount, count);
            }

            for (var s = 1; s <= maxWithCount; s++)
            {
                UInt32 count = totalResourcesForAllStages.slots[s];
                int slice = SliceForString(Stars, count, maxCount);
                statisticsLog.AppendText(String.Format("{0,3}: {1} {2}\n", s, Stars.Substring(0, slice), CountOrEmpty(count)));
            }
        }

        private void AppendUpdateStatistics(FetchFrameInfo[] frameList)
        {
            // #mivance see AppendConstantBindStatistics
            FetchFrameUpdateStats template = frameList[0].stats.updates;

            FetchFrameUpdateStats totalUpdates = new FetchFrameUpdateStats();
            totalUpdates.types = new UInt32[template.types.Length];
            totalUpdates.sizes = new UInt32[template.sizes.Length];

            foreach (var f in frameList)
            {
                FetchFrameUpdateStats updates = f.stats.updates;

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

            statisticsLog.AppendText("\nHistogram of updated resource type:\n");
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

            statisticsLog.AppendText("\nHistogram of updated resource size:\n");
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

        private void AppendDetailedInformation(FetchFrameInfo[] frameList)
        {
            AppendDrawStatistics(frameList);
            AppendDispatchStatistics(frameList);
            AppendInputAssemblerStatistics(frameList);
            AppendConstantBindStatistics(frameList);
            AppendSamplerBindStatistics(frameList);
            AppendResourceBindStatistics(frameList);
            AppendUpdateStatistics(frameList);
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

        public void OnLogfileLoaded()
        {
            statisticsLog.Clear();

            long fileSize = (new FileInfo(m_Core.LogFileName)).Length;

            int firstIdx = 0;

            var firstDrawcall = m_Core.CurDrawcalls[firstIdx];
            while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                firstDrawcall = firstDrawcall.children[0];

            while (firstDrawcall.events.Length == 0)
            {
                if (firstDrawcall.next != null)
                {
                    firstDrawcall = firstDrawcall.next;
                    while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                        firstDrawcall = firstDrawcall.children[0];
                }
                else
                {
                    firstDrawcall = m_Core.CurDrawcalls[++firstIdx];
                    while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                        firstDrawcall = firstDrawcall.children[0];
                }
            }

            UInt64 persistentData = (UInt64)fileSize - firstDrawcall.events[0].fileOffset;

            var lastDraw = m_Core.CurDrawcalls[m_Core.CurDrawcalls.Length - 1];
            while (lastDraw.children != null && lastDraw.children.Length > 0)
                lastDraw = lastDraw.children[lastDraw.children.Length - 1];

            uint drawCount = 0;
            uint dispatchCount = 0;
            uint diagnosticCount = 0;
            foreach (var d in m_Core.CurDrawcalls)
                CountContributingEvents(d, ref drawCount, ref dispatchCount, ref diagnosticCount);

            uint numAPIcalls = lastDraw.eventID - diagnosticCount;

            // #mivance only recording this for comparison vis a vis draw call
            // iteration, we want to preserve the old stats data for the
            // backends which aren't recording statistics
            bool statsRecorded = false;
            uint numDraws = 0;
            uint numDispatches = 0;
            uint numIndexVertexSets = 0;
            uint numConstantSets = 0;
            uint numSamplerSets = 0;
            uint numResourceSets = 0;
            uint numResourceUpdates = 0;

            FetchFrameInfo[] frameList = m_Core.FrameInfo;

            foreach (var f in frameList)
            {
                if (f.stats.recorded == 0)
                    continue;

                statsRecorded = true;

                for (var s = (int)ShaderStageType.First; s < (int)ShaderStageType.Count; s++)
                {
                    numConstantSets += f.stats.constants[s].calls;
                    numSamplerSets += f.stats.samplers[s].calls;
                    numResourceSets += f.stats.resources[s].calls;
                }

                numResourceUpdates += f.stats.updates.calls;
                numIndexVertexSets += (f.stats.indices.calls + f.stats.vertices.calls + f.stats.layouts.calls);
                numDraws += f.stats.draws.calls;
                numDispatches += f.stats.dispatches.calls;
            }

            int numTextures = m_Core.CurTextures.Length;
            int numBuffers = m_Core.CurBuffers.Length;

            ulong IBBytes = 0;
            ulong VBBytes = 0;
            ulong BufBytes = 0;
            foreach (var b in m_Core.CurBuffers)
            {
                BufBytes += b.byteSize;

                if ((b.creationFlags & BufferCreationFlags.IB) != 0)
                    IBBytes += b.byteSize;
                if ((b.creationFlags & BufferCreationFlags.VB) != 0)
                    VBBytes += b.byteSize;
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

            string header = String.Format("Stats for {0}.\n\nFile size: {1:N2}MB\nPersistent Data (approx): {2:N2}MB\n",
                              Path.GetFileName(m_Core.LogFileName),
                              (float)fileSize / (1024.0f * 1024.0f), (float)persistentData / (1024.0f * 1024.0f));
            string draws = String.Format("Draw calls: {0}\nDispatch calls: {1}\n",
                              drawCount, dispatchCount);
            string calls = statsRecorded ? String.Format("API calls: {0}\n\tIndex/vertex bind calls: {1}\n\tConstant bind calls: {2}\n\tSampler bind calls: {3}\n\tResource bind calls: {4}\n\tResource update calls: {5}\n",
                              numAPIcalls, numIndexVertexSets, numConstantSets, numSamplerSets, numResourceSets, numResourceUpdates) : "";
            string ratio = String.Format("API:Draw call ratio: {0}\n\n", (float)numAPIcalls / (float)drawCount);
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

            if (statsRecorded)
                AppendDetailedInformation(frameList);

            statisticsLog.Select(0, 0);
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {

        }

        private void StatisticsViewer_Load(object sender, EventArgs e)
        {

        }
    }
}
