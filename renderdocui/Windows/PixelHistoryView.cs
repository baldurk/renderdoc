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

        public PixelHistoryView(Core core, FetchTexture tex, Point pt, float rangemin, float rangemax, PixelModification[] history)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            events.BeginInit();
            events.BeginUpdate();

            m_Core = core;

            Text += String.Format(" on {0} for ({1}, {2})", tex.name, pt.X, pt.Y);

            bool uintTex = (tex.format.compType == FormatComponentType.UInt);
            bool sintTex = (tex.format.compType == FormatComponentType.SInt);
            bool srgbTex = tex.format.srgbCorrected ||
                           (tex.creationFlags & TextureCreationFlags.SwapBuffer) > 0;
            bool floatTex = (!uintTex && !sintTex);

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

                if (uintTex)
                {
                    preModVal = mod.preMod.value.u[0].ToString() + "\n" +
                                mod.preMod.value.u[1].ToString() + "\n" +
                                mod.preMod.value.u[2].ToString() + "\n" +
                                mod.preMod.value.u[3].ToString();
                    postModVal = mod.postMod.value.u[0].ToString() + "\n" +
                                 mod.postMod.value.u[1].ToString() + "\n" +
                                 mod.postMod.value.u[2].ToString() + "\n" +
                                 mod.postMod.value.u[3].ToString();
                }
                else if (sintTex)
                {
                    preModVal = mod.preMod.value.i[0].ToString() + "\n" +
                                mod.preMod.value.i[1].ToString() + "\n" +
                                mod.preMod.value.i[2].ToString() + "\n" +
                                mod.preMod.value.i[3].ToString();
                    postModVal = mod.postMod.value.i[0].ToString() + "\n" +
                                 mod.postMod.value.i[1].ToString() + "\n" +
                                 mod.postMod.value.i[2].ToString() + "\n" +
                                 mod.postMod.value.i[3].ToString();
                }
                else
                {
                    preModVal = Formatter.Format(mod.preMod.value.f[0]) + "\n" +
                                Formatter.Format(mod.preMod.value.f[1]) + "\n" +
                                Formatter.Format(mod.preMod.value.f[2]) + "\n" +
                                Formatter.Format(mod.preMod.value.f[3]);
                    postModVal = Formatter.Format(mod.postMod.value.f[0]) + "\n" +
                                 Formatter.Format(mod.postMod.value.f[1]) + "\n" +
                                 Formatter.Format(mod.postMod.value.f[2]) + "\n" +
                                 Formatter.Format(mod.postMod.value.f[3]);
                }

                var node = events.Nodes.Add(new object[] { mod.eventID, name, preModVal, "", postModVal, "" });

                node.DefaultBackColor = passed ? Color.FromArgb(235, 255, 235) : Color.FromArgb(255, 235, 235);

                if (floatTex)
                {
                    float r = Helpers.Clamp((mod.preMod.value.f[0] - rangemin) / rangesize, 0.0f, 1.0f);
                    float g = Helpers.Clamp((mod.preMod.value.f[1] - rangemin) / rangesize, 0.0f, 1.0f);
                    float b = Helpers.Clamp((mod.preMod.value.f[2] - rangemin) / rangesize, 0.0f, 1.0f);

                    if (srgbTex)
                    {
                        r = (float)Math.Pow(r, 1.0f / 2.2f);
                        g = (float)Math.Pow(g, 1.0f / 2.2f);
                        b = (float)Math.Pow(b, 1.0f / 2.2f);
                    }

                    node.IndexedBackColor[3] = Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));

                    r = Helpers.Clamp((mod.postMod.value.f[0] - rangemin) / rangesize, 0.0f, 1.0f);
                    g = Helpers.Clamp((mod.postMod.value.f[1] - rangemin) / rangesize, 0.0f, 1.0f);
                    b = Helpers.Clamp((mod.postMod.value.f[2] - rangemin) / rangesize, 0.0f, 1.0f);

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
