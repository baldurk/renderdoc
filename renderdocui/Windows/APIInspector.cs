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
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class APIInspector : DockContent, ILogViewerForm
    {
        private Core m_Core;

        public APIInspector(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            m_Core = core;

            callstack.Font =
                apiEvents.Font =
                core.Config.PreferredFont;

            panelSplitter.Collapsed = true;
        }

        public void OnLogfileLoaded()
        {
        }

        public void OnLogfileClosed()
        {
            apiEvents.BeginUpdate();
            apiEvents.Nodes.Clear();
            apiEvents.EndUpdate();

            if (!callstack.IsDisposed)
            {
                callstack.BeginUpdate();
                callstack.Items.Clear();
                callstack.EndUpdate();
            }
        }

        public void FillAPIView()
        {
            apiEvents.BeginUpdate();
            apiEvents.Nodes.Clear();

            Regex rgx = new Regex("^\\s*[{}]?");
            string replacement = "";

            FetchDrawcall draw = m_Core.CurDrawcall;

            if (draw != null && draw.events != null && draw.events.Length > 0)
            {
                foreach (var ev in draw.events)
                {
                    // hack until I have a proper interface. Skip events associated with this draw that
                    // come from another context (means they will just be completely omitted/invisible).
                    if (ev.context != draw.context)
                        continue;

                    string[] lines = ev.eventDesc.Split(new string[] { "\r\n", "\n" }, StringSplitOptions.None);

                    TreelistView.Node node = apiEvents.Nodes.Add(new TreelistView.Node(new object[] { ev.eventID, lines[0] }));

                    for (int i = 1; i < lines.Length; i++)
                    {
                        string l = rgx.Replace(lines[i], replacement);
                        if (l.Length > 0)
                            node.Nodes.Add(new TreelistView.Node(new object[] { "", l }));
                    }

                    if (ev.eventID == draw.eventID)
                        node.Bold = true;

                    node.Tag = (object)ev;
                }

                if (apiEvents.Nodes.Count > 0)
                    apiEvents.NodesSelection.Add(apiEvents.Nodes[0]);
            }

            apiEvents.EndUpdate();
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            FillAPIView();

            apiEvents.NodesSelection.Clear();
            apiEvents.FocusedNode = apiEvents.Nodes.LastNode;
        }

        private void AddCallstack(String[] calls)
        {
            if (callstack.InvokeRequired)
            {
                this.BeginInvoke(new Action(() =>
                {
                    AddCallstack(calls);
                }));
                return;
            }

            /*
            String commonRoot = calls[0];

            for (int i = 1; i < calls.Length - m_Core.Config.CallstackLevelSkip; i++)
            {
                int len = Math.Min(commonRoot.Length, calls[i].Length);

                int commonLen = 0;
                for (;commonLen < len; commonLen++)
                {
                    if (commonRoot[commonLen] != calls[i][commonLen])
                        break;
                }

                if (commonLen == 0)
                {
                    commonRoot = "";
                    break;
                }

                commonRoot = commonRoot.Substring(0, commonLen);
            }
            */

            callstack.Items.Clear();

            if (calls.Length == 1 && calls[0].Length == 0)
            {
                callstack.Items.Add("Symbols not loaded. Tools -> Resolve Symbols.");
            }
            else
            {
                for (int i = 0; i < calls.Length - m_Core.Config.CallstackLevelSkip; i++)
                {
                    //callstack.Items.Add(calls[i].Substring(commonRoot.Length));
                    callstack.Items.Add(calls[i]);
                }
            }
        }

        private void treeView1_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (IsDisposed || apiEvents.IsDisposed)
                return;

            FillCallstack();
        }

        public void FillCallstack()
        {
            if(apiEvents.SelectedNode == null)
                return;

            FetchAPIEvent ev = (FetchAPIEvent)apiEvents.SelectedNode.Tag;

            if(ev == null)
                ev = (FetchAPIEvent)apiEvents.SelectedNode.Parent.Tag;

            if (ev == null)
                return;

            if (ev.callstack != null && ev.callstack.Length > 0)
            {
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) => { AddCallstack(r.GetResolve(ev.callstack)); });
            }
            else
            {
                callstack.Items.Clear();
                callstack.Items.Add("No Callstack available.");
            }
        }

        private void APIEvents_Shown(object sender, EventArgs e)
        {
            if (m_Core.LogLoaded)
                OnLogfileLoaded();

            panelSplitter.Collapsed = true;
        }

        private void UpdateSettings()
        {
            bool changed = false;

            if (int.TryParse(callSkip.Text, out m_Core.Config.CallstackLevelSkip))
            {
                if (m_Core.Config.CallstackLevelSkip < 0)
                    m_Core.Config.CallstackLevelSkip = 0;
                callSkip.Text = m_Core.Config.CallstackLevelSkip.ToString();
                changed = true;
            }

            if (changed)
                FillCallstack();
        }

        private void callstack_MouseUp(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
                contextMenu.Show(callstack.PointToScreen(e.Location));

            if (e.Button == MouseButtons.Left && contextMenu.Visible)
                UpdateSettings();
        }

        private void callSkip_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\r' || e.KeyChar == '\n')
            {
                UpdateSettings();
                e.Handled = true;
            }
        }

        private void dirSkip_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\r' || e.KeyChar == '\n')
            {
                UpdateSettings();
                e.Handled = true;
            }
        }

        private void apiEvents_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

            if (e.KeyCode == Keys.C && e.Control)
            {
                string text = "";
                foreach (var n in apiEvents.NodesSelection)
                {
                    text += string.Format("{0,-5}  {1}" + Environment.NewLine, n[0].ToString(), n[1].ToString());
                }

                try
                {
                    if (text.Length > 0)
                        Clipboard.SetText(text);
                }
                catch (System.Exception)
                {
                    try
                    {
                        if (text.Length > 0)
                            Clipboard.SetDataObject(text);
                    }
                    catch (System.Exception)
                    {
                        // give up!
                    }
                }
            }
        }

        private void callstack_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

            if (e.KeyCode == Keys.C && e.Control)
            {
                string text = "";
                foreach (var n in callstack.SelectedItems)
                {
                    text += n.ToString() + Environment.NewLine;
                }

                try
                {
                    if (text.Length > 0)
                        Clipboard.SetText(text);
                }
                catch (System.Exception)
                {
                    try
                    {
                        if (text.Length > 0)
                            Clipboard.SetDataObject(text);
                    }
                    catch (System.Exception)
                    {
                        // give up!
                    }
                }
            }
        }

        private void callstacklabel_DoubleClick(object sender, EventArgs e)
        {
            panelSplitter.Collapsed = !panelSplitter.Collapsed;
        }

        private void APIInspector_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }
    }
}
