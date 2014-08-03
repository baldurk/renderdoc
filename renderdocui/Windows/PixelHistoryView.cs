/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
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


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class PixelHistoryView : DockContent, ILogViewerForm
    {
        Core m_Core;
        FetchTexture texture;
        Point pixel;
        PixelModification[] modifications;
        bool[] visibleChannels;
        float rangeMin, rangeMax;
        int numChannels, channelIdx;

        public PixelHistoryView(Core core, FetchTexture tex, Point pt,
                                float rangemin, float rangemax, bool[] channels)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

            texture = tex;
            pixel = pt;
            rangeMin = rangemin;
            rangeMax = rangemax;
            visibleChannels = channels;

            Text = String.Format("Pixel History on {0} for ({1}, {2})", tex.name, pt.X, pt.Y);

            string channelStr = "";
            numChannels = 0;
            channelIdx = 0;

            if (channels[0])
            {
                channelStr += "R";
                numChannels++;
                channelIdx = 0;
            }
            if (channels[1])
            {
                channelStr += "G";
                numChannels++;
                channelIdx = 1;
            }
            if (channels[2])
            {
                channelStr += "B";
                numChannels++;
                channelIdx = 2;
            }

            channelStr += " channel";
            if (numChannels > 1)
                channelStr += "s";

            // if alpha channel is enabled it only does anything if the
            // other channels are all disabled. There is no RGBA preview
            if (numChannels == 0 && channels[3])
            {
                channelStr = "Alpha";
                numChannels = 1;
                channelIdx = 3;
            }

            historyContext.Text = String.Format("Preview colours displayed in visible range {0} - {1} with {2} visible.",
                                                Formatter.Format(rangemin), Formatter.Format(rangemax), channelStr) + Environment.NewLine;
            historyContext.Text += Environment.NewLine;
            historyContext.Text += "Double click to jump to an event." + Environment.NewLine;
            historyContext.Text += "Right click to debug an event, or hide failed events.";

            eventsHidden.Text = "";

            modifications = null;

            events.BeginUpdate();

            events.Nodes.Clear();

            events.Nodes.Add(new object[] { "", "Loading...", "", "", "", "" });

            events.EndUpdate();
        }

        public void SetHistory(PixelModification[] history)
        {
            modifications = history;

            UpdateEventList();

            PixelHistoryView_Enter(null, null);
        }

        void UpdateEventList()
        {
            if (modifications == null) return;

            events.BeginUpdate();

            events.Nodes.Clear();

            bool uintTex = (texture.format.compType == FormatComponentType.UInt);
            bool sintTex = (texture.format.compType == FormatComponentType.SInt);
            bool srgbTex = texture.format.srgbCorrected ||
                           (texture.creationFlags & TextureCreationFlags.SwapBuffer) > 0;
            bool floatTex = (!uintTex && !sintTex);

            int numComps = (int)texture.format.compCount;

            if (texture.format.compType == FormatComponentType.Depth ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D24S8) ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D32S8))
                numComps = 0;

            float rangesize = (rangeMax - rangeMin);

            foreach (PixelModification mod in modifications)
            {
                string name = "name";

                var drawcall = m_Core.GetDrawcall(m_Core.CurFrame, mod.eventID);

                if (drawcall == null) continue;

                name = drawcall.name;

                bool passed = mod.EventPassed();

                if (mod.backfaceCulled)
                    name += "\nBackface culled";
                if (mod.depthClipped)
                    name += "\nDepth Clipped";
                if (mod.scissorClipped)
                    name += "\nScissor Clipped";
                if (mod.shaderDiscarded)
                    name += "\nShader executed a discard";
                if (mod.depthTestFailed)
                    name += "\nDepth test failed";
                if (mod.stencilTestFailed)
                    name += "\nStencil test failed";

                if(!passed && hideFailedEventsToolStripMenuItem.Checked)
                    continue;

                string preModVal = "";
                string postModVal = "";

                string[] prefix = new string[] { "R: ", "G: ", "B: ", "A: " };

                if (uintTex)
                {
                    for (int i = 0; i < numComps; i++)
                    {
                        preModVal += prefix[i] + mod.preMod.col.value.u[i].ToString() + "\n";
                        postModVal += prefix[i] + mod.postMod.col.value.u[i].ToString() + "\n";
                    }
                }
                else if (sintTex)
                {
                    for (int i = 0; i < numComps; i++)
                    {
                        preModVal += prefix[i] + mod.preMod.col.value.i[i].ToString() + "\n";
                        postModVal += prefix[i] + mod.postMod.col.value.i[i].ToString() + "\n";
                    }
                }
                else
                {
                    for (int i = 0; i < numComps; i++)
                    {
                        preModVal += prefix[i] + Formatter.Format(mod.preMod.col.value.f[i]) + "\n";
                        postModVal += prefix[i] + Formatter.Format(mod.postMod.col.value.f[i]) + "\n";
                    }
                }

                if (mod.preMod.depth >= 0.0f)
                {
                    preModVal += "\nD: " + Formatter.Format(mod.preMod.depth);
                    postModVal += "\nD: " + Formatter.Format(mod.postMod.depth);
                }
                else
                {
                    preModVal += "\nD: -";
                    postModVal += "\nD: -";
                }

                if (mod.preMod.stencil >= 0)
                {
                    preModVal += String.Format("\nS: 0x{0:X2}", mod.preMod.stencil);
                    postModVal += String.Format("\nS: 0x{0:X2}", mod.postMod.stencil);
                }
                else
                {
                    preModVal += "\nS: -";
                    postModVal += "\nS: -";
                }

                var node = events.Nodes.Add(new object[] { mod.eventID, name, preModVal, "", postModVal, "" });

                node.DefaultBackColor = passed ? Color.FromArgb(235, 255, 235) : Color.FromArgb(255, 235, 235);

                if (floatTex || numComps == 0)
                {
                    float r = mod.preMod.col.value.f[0];
                    float g = mod.preMod.col.value.f[1];
                    float b = mod.preMod.col.value.f[2];

                    if (numChannels == 1)
                    {
                        r = g = b = mod.preMod.col.value.f[channelIdx];
                    }
                    else
                    {
                        if (!visibleChannels[0]) r = 0.0f;
                        if (!visibleChannels[1]) g = 0.0f;
                        if (!visibleChannels[2]) b = 0.0f;
                    }

                    r = Helpers.Clamp((r - rangeMin) / rangesize, 0.0f, 1.0f);
                    g = Helpers.Clamp((g - rangeMin) / rangesize, 0.0f, 1.0f);
                    b = Helpers.Clamp((b - rangeMin) / rangesize, 0.0f, 1.0f);

                    if(numComps == 0)
                        r = g = b = Helpers.Clamp((mod.preMod.depth - rangeMin) / rangesize, 0.0f, 1.0f);

                    if (srgbTex)
                    {
                        r = (float)Math.Pow(r, 1.0f / 2.2f);
                        g = (float)Math.Pow(g, 1.0f / 2.2f);
                        b = (float)Math.Pow(b, 1.0f / 2.2f);
                    }

                    node.IndexedBackColor[3] = Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));

                    r = mod.postMod.col.value.f[0];
                    g = mod.postMod.col.value.f[1];
                    b = mod.postMod.col.value.f[2];

                    if (numChannels == 1)
                    {
                        r = g = b = mod.postMod.col.value.f[channelIdx];
                    }
                    else
                    {
                        if (!visibleChannels[0]) r = 0.0f;
                        if (!visibleChannels[1]) g = 0.0f;
                        if (!visibleChannels[2]) b = 0.0f;
                    }

                    r = Helpers.Clamp((r - rangeMin) / rangesize, 0.0f, 1.0f);
                    g = Helpers.Clamp((g - rangeMin) / rangesize, 0.0f, 1.0f);
                    b = Helpers.Clamp((b - rangeMin) / rangesize, 0.0f, 1.0f);

                    if (numComps == 0)
                        r = g = b = Helpers.Clamp((mod.postMod.depth - rangeMin) / rangesize, 0.0f, 1.0f);

                    if (srgbTex)
                    {
                        r = (float)Math.Pow(r, 1.0f / 2.2f);
                        g = (float)Math.Pow(g, 1.0f / 2.2f);
                        b = (float)Math.Pow(b, 1.0f / 2.2f);
                    }

                    node.IndexedBackColor[5] = Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));
                }

                node.Tag = mod.eventID;
            }

            events.EndUpdate();
        }

        public void OnLogfileClosed()
        {
            Close();
        }

        public void OnLogfileLoaded()
        {
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
        }

        private void events_NodeDoubleClicked(TreelistView.Node node)
        {
            if (node.Tag is uint)
            {
                m_Core.SetEventID(this, m_Core.CurFrame, (uint)node.Tag);
            }
        }

        private void events_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                rightclickMenu.Show(events.PointToScreen(e.Location));
            }
        }

        private void debugToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (events.SelectedNode == null) return;

            var node = events.SelectedNode;

            if (node.Tag is uint)
            {
                m_Core.SetEventID(this, m_Core.CurFrame, (uint)node.Tag);

                ShaderDebugTrace trace = null;

                ShaderReflection shaderDetails = m_Core.CurPipelineState.GetShaderReflection(ShaderStageType.Pixel);

                m_Core.Renderer.Invoke((ReplayRenderer r) =>
                {
                    trace = r.PSGetDebugStates((UInt32)pixel.X, (UInt32)pixel.Y);
                });

                if (trace == null || trace.states.Length == 0)
                {
                    MessageBox.Show("Error debugging pixel.", "Debug Error",
                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                this.BeginInvoke(new Action(() =>
                {
                    ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, ShaderStageType.Pixel, trace);

                    s.Show(this.DockPanel);
                }));
            }
        }

        private void hideFailedEventsToolStripMenuItem_CheckedChanged(object sender, EventArgs e)
        {
            if (hideFailedEventsToolStripMenuItem.Checked)
                eventsHidden.Text = "Failed events are currently hidden";
            else
                eventsHidden.Text = "";

            UpdateEventList();
        }

        private void PixelHistoryView_Enter(object sender, EventArgs e)
        {
            var timeline = m_Core.GetTimelineBar();

            if (timeline != null)
                timeline.HighlightHistory(texture, pixel, modifications);
        }

        private void PixelHistoryView_Leave(object sender, EventArgs e)
        {
            var timeline = m_Core.GetTimelineBar();

            if (timeline != null)
                timeline.HighlightHistory(null, Point.Empty, null);
        }

        private void PixelHistoryView_FormClosed(object sender, FormClosedEventArgs e)
        {
            var timeline = m_Core.GetTimelineBar();

            if (timeline != null)
                timeline.HighlightHistory(null, Point.Empty, null);
        }
    }
}
