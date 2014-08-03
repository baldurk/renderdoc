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

        public PixelHistoryView(Core core, FetchTexture tex, Point pt,
                                float rangemin, float rangemax, bool[] channels,
                                PixelModification[] history)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            events.BeginInit();
            events.BeginUpdate();

            m_Core = core;

            Text = String.Format("Pixel History on {0} for ({1}, {2})", tex.name, pt.X, pt.Y);

            string channelStr = "";
            int numChannels = 0;
            int channelIdx = 0;

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
            historyContext.Text += "Right click to debug an event, or hide failed events.";

            bool uintTex = (tex.format.compType == FormatComponentType.UInt);
            bool sintTex = (tex.format.compType == FormatComponentType.SInt);
            bool srgbTex = tex.format.srgbCorrected ||
                           (tex.creationFlags & TextureCreationFlags.SwapBuffer) > 0;
            bool floatTex = (!uintTex && !sintTex);

            int numComps = (int)tex.format.compCount;

            if (tex.format.compType == FormatComponentType.Depth ||
                (tex.format.special && tex.format.specialFormat == SpecialFormat.D24S8) ||
                (tex.format.special && tex.format.specialFormat == SpecialFormat.D32S8))
                numComps = 0;

            float rangesize = (rangemax - rangemin);

            foreach (PixelModification mod in history)
            {
                string name = "name";

                var drawcall = core.GetDrawcall(core.CurFrame, mod.eventID);

                if (drawcall == null) continue;

                name = drawcall.name;

                bool passed = true;

                if (mod.backfaceCulled)
                {
                    name += "\nBackface culled";
                    passed = false;
                }
                if (mod.depthClipped)
                {
                    name += "\nDepth Clipped";
                    passed = false;
                }
                if (mod.scissorClipped)
                {
                    name += "\nScissor Clipped";
                    passed = false;
                }
                if (mod.shaderDiscarded)
                {
                    name += "\nShader executed a discard";
                    passed = false;
                }
                if (mod.depthTestFailed)
                {
                    name += "\nDepth test failed";
                    passed = false;
                }
                if (mod.stencilTestFailed)
                {
                    name += "\nStencil test failed";
                    passed = false;
                }

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
                        if (!channels[0]) r = 0.0f;
                        if (!channels[1]) g = 0.0f;
                        if (!channels[2]) b = 0.0f;
                    }

                    r = Helpers.Clamp((r - rangemin) / rangesize, 0.0f, 1.0f);
                    g = Helpers.Clamp((g - rangemin) / rangesize, 0.0f, 1.0f);
                    b = Helpers.Clamp((b - rangemin) / rangesize, 0.0f, 1.0f);

                    if(numComps == 0)
                        r = g = b = Helpers.Clamp((mod.preMod.depth - rangemin) / rangesize, 0.0f, 1.0f);

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
                        if (!channels[0]) r = 0.0f;
                        if (!channels[1]) g = 0.0f;
                        if (!channels[2]) b = 0.0f;
                    }

                    r = Helpers.Clamp((r - rangemin) / rangesize, 0.0f, 1.0f);
                    g = Helpers.Clamp((g - rangemin) / rangesize, 0.0f, 1.0f);
                    b = Helpers.Clamp((b - rangemin) / rangesize, 0.0f, 1.0f);

                    if (numComps == 0)
                        r = g = b = Helpers.Clamp((mod.postMod.depth - rangemin) / rangesize, 0.0f, 1.0f);

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
            events.EndInit();
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
    }
}
