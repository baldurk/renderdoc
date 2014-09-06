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

            events.Nodes.Add(new object[] { "Loading...", "", "", "", "" });

            events.EndUpdate();
        }

        public void SetHistory(PixelModification[] history)
        {
            modifications = history;

            UpdateEventList();

            PixelHistoryView_Enter(null, null);
        }

        private string[] colourLetterPrefix = new string[] { "R: ", "G: ", "B: ", "A: " };

        private string ModificationValueString(ModificationValue val, ResourceFormat fmt, bool depth)
        {
            string s = "";

            bool uintTex = (fmt.compType == FormatComponentType.UInt);
            bool sintTex = (fmt.compType == FormatComponentType.SInt);
            int numComps = (int)(fmt.compCount);

            if (!depth)
            {
                if (uintTex)
                {
                    for (int i = 0; i < numComps; i++)
                        s += colourLetterPrefix[i] + val.col.value.u[i].ToString() + "\n";
                }
                else if (sintTex)
                {
                    for (int i = 0; i < numComps; i++)
                        s += colourLetterPrefix[i] + val.col.value.i[i].ToString() + "\n";
                }
                else
                {
                    for (int i = 0; i < numComps; i++)
                        s += colourLetterPrefix[i] + Formatter.Format(val.col.value.f[i]) + "\n";
                }
            }

            if (val.depth >= 0.0f)
                s += "\nD: " + Formatter.Format(val.depth);
            else if (val.depth < -1.5f)
                s += "\nD: ?";
            else
                s += "\nD: -";

            if (val.stencil >= 0)
                s += String.Format("\nS: 0x{0:X2}", val.stencil);
            else if (val.stencil == -2)
                s += "\nS: ?";
            else
                s += "\nS: -";

            return s;
        }

        private Color ModificationValueColor(ModificationValue val, bool depth)
        {
            float rangesize = (rangeMax - rangeMin);

            float r = val.col.value.f[0];
            float g = val.col.value.f[1];
            float b = val.col.value.f[2];

            if (numChannels == 1)
            {
                r = g = b = val.col.value.f[channelIdx];
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

            if (depth)
                r = g = b = Helpers.Clamp((val.depth - rangeMin) / rangesize, 0.0f, 1.0f);

            {
                r = (float)Math.Pow(r, 1.0f / 2.2f);
                g = (float)Math.Pow(g, 1.0f / 2.2f);
                b = (float)Math.Pow(b, 1.0f / 2.2f);
            }

            return Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));
        }

        private string FailureString(PixelModification mod)
        {
            string s = "";

            if (mod.backfaceCulled)
                s += "\nBackface culled";
            if (mod.depthClipped)
                s += "\nDepth Clipped";
            if (mod.scissorClipped)
                s += "\nScissor Clipped";
            if (mod.shaderDiscarded)
                s += "\nShader executed a discard";
            if (mod.depthTestFailed)
                s += "\nDepth test failed";
            if (mod.stencilTestFailed)
                s += "\nStencil test failed";

            return s;
        }

        private TreelistView.Node MakeFragmentNode(PixelModification mod)
        {
            bool uintTex = (texture.format.compType == FormatComponentType.UInt);
            bool sintTex = (texture.format.compType == FormatComponentType.SInt);
            bool floatTex = (!uintTex && !sintTex);

            bool depth = false;

            if (texture.format.compType == FormatComponentType.Depth ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D24S8) ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D32S8))
                depth = true;

            string name = String.Format("Primitive {0}\n", mod.primitiveID);

            ResourceFormat fmt = new ResourceFormat(floatTex ? FormatComponentType.Float : texture.format.compType, 4, 4);

            string shadOutVal = "Shader Out\n\n" + ModificationValueString(mod.shaderOut, fmt, depth);
            string postModVal = "Tex After\n\n" + ModificationValueString(mod.postMod, texture.format, depth);

            if (!mod.EventPassed() && hideFailedEventsToolStripMenuItem.Checked)
                return null;

            var node = new TreelistView.Node(new object[] { name, shadOutVal, "", postModVal, "" });

            node.Tag = mod.eventID;

            if (floatTex || depth)
            {
                node.IndexedBackColor[2] = ModificationValueColor(mod.shaderOut, depth);
                node.IndexedBackColor[4] = ModificationValueColor(mod.postMod, depth);
            }

            return node;
        }

        private TreelistView.Node MakeEventNode(List<TreelistView.Node> nodes, List<PixelModification> mods)
        {
            bool uintTex = (texture.format.compType == FormatComponentType.UInt);
            bool sintTex = (texture.format.compType == FormatComponentType.SInt);
            bool floatTex = (!uintTex && !sintTex);

            bool depth = false;

            if (texture.format.compType == FormatComponentType.Depth ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D24S8) ||
                (texture.format.special && texture.format.specialFormat == SpecialFormat.D32S8))
                depth = true;

            var drawcall = m_Core.GetDrawcall(m_Core.CurFrame, mods[0].eventID);
            if (drawcall == null) return null;

            string name = "";
            var drawstack = new List<FetchDrawcall>();
            var parent = drawcall.parent;
            while (parent != null)
            {
                drawstack.Add(parent);
                parent = parent.parent;
            }

            drawstack.Reverse();

            if (drawstack.Count > 0)
            {
                name += "> " + drawstack[0].name;

                if (drawstack.Count > 3)
                    name += " ...";

                name += "\n";

                if (drawstack.Count > 2)
                    name += "> " + drawstack[drawstack.Count - 2].name + "\n";
                if (drawstack.Count > 1)
                    name += "> " + drawstack[drawstack.Count - 1].name + "\n";

                name += "\n";
            }

            name += String.Format("EID {0}\n{1}{2}\n{3} Fragments touching pixel\n", mods[0].eventID, drawcall.name, FailureString(mods[0]), nodes.Count);

            bool passed = mods[0].EventPassed();

            string preModVal = "Tex Before\n\n" + ModificationValueString(mods.First().preMod, texture.format, depth);
            string postModVal = "Tex After\n\n" + ModificationValueString(mods.Last().postMod, texture.format, depth);

            var node = new TreelistView.Node(new object[] { name, preModVal, "", postModVal, "" });

            node.DefaultBackColor = passed ? Color.FromArgb(235, 255, 235) : Color.FromArgb(255, 235, 235);
            node.Tag = mods[0].eventID;

            if (floatTex || depth)
            {
                node.IndexedBackColor[2] = ModificationValueColor(mods.First().preMod, depth);
                node.IndexedBackColor[4] = ModificationValueColor(mods.Last().postMod, depth);
            }

            if ((drawcall.flags & DrawcallFlags.Clear) == 0)
            {
                foreach (var child in nodes)
                    node.Nodes.Add(child);
            }

            return node;
        }

        private void UpdateEventList()
        {
            if (modifications == null) return;

            events.BeginUpdate();

            events.Nodes.Clear();

            var frags = new List<TreelistView.Node>();
            var mods = new List<PixelModification>();

            for(int i=0; i < modifications.Length; i++)
            {
                var node = MakeFragmentNode(modifications[i]);
                if (node == null) continue;

                frags.Add(node);
                mods.Add(modifications[i]);

                if (i + 1 >= modifications.Length || modifications[i + 1].eventID != modifications[i].eventID)
                {
                    if(frags.Count > 0)
                        events.Nodes.Add(MakeEventNode(frags, mods));

                    frags.Clear();
                    mods.Clear();
                }
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
                debugToolStripMenuItem.Enabled = false;

                debugToolStripMenuItem.Text = "Debug Pixel";

                if (events.SelectedNode != null && events.SelectedNode.Tag != null && events.SelectedNode.Tag is uint)
                {
                    debugToolStripMenuItem.Enabled = true;
                    debugToolStripMenuItem.Text = String.Format("Debug Pixel ({0}, {1}) at Event {2}",
                                                                    pixel.X, pixel.Y, (uint)events.SelectedNode.Tag);
                }

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
                    string debugContext = String.Format("Pixel {0},{1}", pixel.X, pixel.Y);

                    ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, ShaderStageType.Pixel, trace, debugContext);

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
