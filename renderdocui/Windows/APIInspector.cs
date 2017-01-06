/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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

            Regex rgxopen = new Regex("^\\s*{");
            Regex rgxclose = new Regex("^\\s*}");

            FetchDrawcall draw = m_Core.CurDrawcall;

            if (draw != null && draw.events != null && draw.events.Length > 0)
            {
                foreach (var ev in draw.events)
                {
                    string[] lines = ev.eventDesc.Split(new string[] { "\r\n", "\n" }, StringSplitOptions.None);

                    TreelistView.Node root = new TreelistView.Node(new object[] { ev.eventID, lines[0] });

                    int i=1;

                    if (i < lines.Length && lines[i].Trim() == "{")
                        i++;

                    List<TreelistView.Node> nodestack = new List<TreelistView.Node>();
                    nodestack.Add(root);

                    for (; i < lines.Length; i++)
                    {
                        if (rgxopen.IsMatch(lines[i]))
                            nodestack.Add(nodestack.Last().Nodes.LastNode);
                        else if (rgxclose.IsMatch(lines[i]))
                            nodestack.RemoveAt(nodestack.Count - 1);
                        else if(lines[i].Trim().Length > 0 && nodestack.Count > 0)
                            nodestack.Last().Nodes.Add(new TreelistView.Node(new object[] { "", lines[i].Trim() }));
                    }

                    if (ev.eventID == draw.eventID)
                        root.Bold = true;

                    root.Tag = (object)ev;

                    apiEvents.Nodes.Add(root);
                }

                if (apiEvents.Nodes.Count > 0)
                    apiEvents.NodesSelection.Add(apiEvents.Nodes[0]);
            }

            apiEvents.EndUpdate();
        }

        public void OnEventSelected(UInt32 eventID)
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

            callstack.Items.Clear();

            if (calls.Length == 1 && calls[0].Length == 0)
            {
                callstack.Items.Add("Symbols not loaded. Tools -> Resolve Symbols.");
            }
            else
            {
                for (int i = 0; i < calls.Length; i++)
                    callstack.Items.Add(calls[i]);
            }
        }

        private void apiEvents_AfterSelect(object sender, TreeViewEventArgs e)
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
            panelSplitter.Collapsed = true;
        }

        private void apiEvents_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

            if (e.KeyCode == Keys.C && e.Control)
            {
                apiEvents.SortNodesSelection();

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
