﻿/******************************************************************************
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
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;
using System.IO;

namespace renderdocui.Windows
{
    public partial class EventBrowser : DockContent, ILogViewerForm
    {
        class DeferredEvent
        {
            public UInt32 frameID = 0;
            public UInt32 eventID = 0;

            public bool marker = false;

            public ResourceId defCtx = ResourceId.Null;
            public UInt32 firstDefEv = 0;
            public UInt32 lastDefEv = 0;
        }

        private List<TreelistView.Node> m_FrameNodes = new List<TreelistView.Node>();

        private Core m_Core;

        public EventBrowser(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            jumpToEID.Font =
                findEvent.Font =
                eventView.Font = 
                core.Config.PreferredFont;

            HideJumpAndFind();

            m_Core = core;

            DockHandler.GetPersistStringCallback = PersistString;

            var col = eventView.Columns["Drawcall"]; eventView.Columns.SetVisibleIndex(col, -1);
            col = eventView.Columns["Duration"];     eventView.Columns.SetVisibleIndex(col, -1);

            UpdateDurationColumn();

            eventView.CellPainter.CellDataConverter = DataToString;

            findEventButton.Enabled = false;
            jumpEventButton.Enabled = false;
            timeDraws.Enabled = false;
        }

        public class PersistData
        {
            public static int currentPersistVersion = 1;
            public int persistVersion = currentPersistVersion;

            public struct ColumnArrangement
            {
                public string fieldname;
                public int visibleindex;
                public int width;
            };

            public List<ColumnArrangement> visibleColumns = new List<ColumnArrangement>();

            public static PersistData GetDefaults(TreelistView.TreeListView view)
            {
                PersistData data = new PersistData();

                foreach (var c in view.Columns)
                {
                    ColumnArrangement a = new ColumnArrangement();
                    a.fieldname = c.Fieldname;
                    a.visibleindex = c.VisibleIndex;
                    a.width = c.Width;
                    data.visibleColumns.Add(a);
                }

                return data;
            }
        }

        public void InitFromPersistString(string str)
        {
            PersistData data = null;

            try
            {
                if (str.Length > GetType().ToString().Length)
                {
                    var reader = new StringReader(str.Substring(GetType().ToString().Length));

                    System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(PersistData));
                    data = (PersistData)xs.Deserialize(reader);

                    reader.Close();
                }
            }
            catch (System.Xml.XmlException)
            {
            }
            catch (InvalidOperationException)
            {
                // don't need to handle it. Leave data null and pick up defaults below
            }

            if (data == null || data.persistVersion != PersistData.currentPersistVersion)
            {
                data = PersistData.GetDefaults(eventView);
            }

            ApplyPersistData(data);
        }

        private void ApplyPersistData(PersistData data)
        {
            // loop twice because first time will ensure the right columns are visible but
            // e.g. if the first column we grabbed should be in visibleindex 2, it would 
            // get shown and be forced to 0 (arbitrary example). Second pass ensures the
            // order is correct
            for (int i = 0; i < 2; i++)
            {
                foreach (var c in data.visibleColumns)
                {
                    var col = eventView.Columns[c.fieldname];
                    if (col == null) continue;
                    eventView.Columns.SetVisibleIndex(col, c.visibleindex);

                    if (i == 1)
                        col.Width = c.width;
                }
            }
        }

        private string PersistString()
        {
            var writer = new StringWriter();

            writer.Write(GetType().ToString());

            PersistData data = PersistData.GetDefaults(eventView);

            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(PersistData));
            xs.Serialize(writer, data);

            return writer.ToString();
        }


        private PersistantConfig.TimeUnit m_TimeUnit = PersistantConfig.TimeUnit.Microseconds;

        private void UpdateDurationColumn()
        {
            m_TimeUnit = m_Core.Config.EventBrowser_TimeUnit;

            string durationString = PersistantConfig.UnitPrefix(m_TimeUnit);

            eventView.Columns["Duration"].Caption = String.Format("Duration ({0})", durationString);
        }

        private string DataToString(TreelistView.TreeListColumn column, object data)
        {
            if (column.Fieldname == "Duration")
            {
                double f = (double)data;
                if (f < 0.0)
                    return "";

                if (m_Core.Config.EventBrowser_TimeUnit != m_TimeUnit)
                    UpdateDurationColumn();

                if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Milliseconds)
                    f *= 1000.0;
                else if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Microseconds)
                    f *= 1000000.0;
                else if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Nanoseconds)
                    f *= 1000000000.0;

                return Formatter.Format(f);
            }

            return data.ToString();
        }

        private TreelistView.Node MakeMarker(string text)
        {
            return new TreelistView.Node(new object[] { "", "", text, -1.0 });
        }

        private TreelistView.Node MakeNode(UInt32 EID, UInt32 draw, string text, double duration)
        {
            return new TreelistView.Node(new object[] { EID, draw, text, duration });
        }

        private TreelistView.Node AddDrawcall(FetchDrawcall drawcall, TreelistView.Node root)
        {
            if (m_Core.Config.EventBrowser_HideEmpty)
            {
                if ((drawcall.children == null || drawcall.children.Length == 0) && (drawcall.flags & DrawcallFlags.PushMarker) != 0)
                    return null;
            }

            UInt32 eventNum = drawcall.eventID;
            TreelistView.Node drawNode = MakeNode(eventNum, drawcall.drawcallID, drawcall.name, 0.0);

            DeferredEvent def = new DeferredEvent();
            def.frameID = m_Core.CurFrame;
            def.eventID = eventNum;
            def.marker = (drawcall.flags & DrawcallFlags.SetMarker) != 0;

            if (drawcall.context != m_Core.FrameInfo[m_Core.CurFrame].immContextId)
            {
                def.defCtx = drawcall.context;
                def.lastDefEv = drawcall.eventID;

                FetchDrawcall parent = drawcall.parent;
                while(!parent.name.Contains("ExecuteCommand"))
                    parent = parent.parent;

                def.eventID = parent.eventID-1;

                def.firstDefEv = parent.children[0].eventID;
                if(parent.children[0].events.Length > 0)
                    def.firstDefEv = parent.children[0].events[0].eventID;
            }

            drawNode.Tag = def;

            if (drawcall.children != null && drawcall.children.Length > 0)
            {
                for (int i = 0; i < drawcall.children.Length; i++)
                {
                    AddDrawcall(drawcall.children[i], drawNode);

                    if (i > 0 && (drawcall.children[i-1].flags & DrawcallFlags.SetMarker) > 0)
                    {
                        drawNode.Nodes[drawNode.Nodes.Count - 2].Tag = drawNode.Nodes.LastNode.Tag;
                    }
                }

                bool found = false;

                for (int i = drawNode.Nodes.Count - 1; i >= 0; i--)
                {
                    DeferredEvent t = drawNode.Nodes[i].Tag as DeferredEvent;
                    if (t != null && !t.marker)
                    {
                        drawNode.Tag = drawNode.Nodes[i].Tag;
                        found = true;
                        break;
                    }
                }

                if (!found && !drawNode.Nodes.IsEmpty())
                    drawNode.Tag = drawNode.Nodes.LastNode.Tag;
            }

            if (drawNode.Nodes.IsEmpty() && (drawcall.flags & DrawcallFlags.PushMarker) != 0)
                return null;

            root.Nodes.Add(drawNode);

            return drawNode;
        }

        private void SetDrawcallTimes(TreelistView.Node n, Dictionary<uint, List<CounterResult>> times)
        {
            if (n == null || times == null) return;

            // parent nodes take the value of the sum of their children
            double duration = 0.0;

            // look up leaf nodes in the dictionary
            if (n.Nodes.IsEmpty())
            {
                uint eid = (uint)n["EID"];

                if (times.ContainsKey(eid))
                    duration = times[eid][0].value.d;
                else
                    duration = -1.0;

                n["Duration"] = duration;

                return;
            }

            for (int i = 0; i < n.Nodes.Count; i++)
            {
                SetDrawcallTimes(n.Nodes[i], times);

                double nd = (double)n.Nodes[i]["Duration"];

                if(nd > 0.0)
                    duration += nd;
            }

            n["Duration"] = duration;
        }

        private void AddFrameDrawcalls(TreelistView.Node frame, FetchDrawcall[] drawcalls)
        {
            eventView.BeginUpdate();

            frame["Duration"] = -1.0;

            DeferredEvent startEv = new DeferredEvent();
            startEv.frameID = m_Core.CurFrame;
            startEv.eventID = 0;

            frame.Nodes.Clear();
            frame.Nodes.Add(MakeNode(0, 0, "Frame Start", -1.0)).Tag = startEv;

            for (int i = 0; i < drawcalls.Length; i++)
                AddDrawcall(drawcalls[i], frame);

            frame.Tag = frame.Nodes.LastNode.Tag;

            eventView.EndUpdate();
        }

        public void OnLogfileClosed()
        {
            eventView.BeginUpdate();
            eventView.Nodes.Clear();
            m_FrameNodes.Clear();
            eventView.EndUpdate();

            findEventButton.Enabled = false;
            jumpEventButton.Enabled = false;
            timeDraws.Enabled = false;
        }

        public void OnLogfileLoaded()
        {
            FetchFrameInfo[] frameList = m_Core.FrameInfo;

            findEventButton.Enabled = true;
            jumpEventButton.Enabled = true;
            timeDraws.Enabled = true;

            eventView.BeginUpdate();

            eventView.Nodes.Clear();

            m_FrameNodes.Clear();

            for (int curFrame = 0; curFrame < frameList.Length; curFrame++)
            {
                TreelistView.Node frame = eventView.Nodes.Add(MakeMarker("Frame #" + frameList[curFrame].frameNumber.ToString()));

                m_FrameNodes.Add(frame);
            }

            eventView.EndUpdate();
                
            for (int curFrame = 0; curFrame < frameList.Length; curFrame++)
                AddFrameDrawcalls(m_FrameNodes[curFrame], m_Core.GetDrawcalls((UInt32)curFrame));

            if (frameList.Length > 0)
            {
                // frame 1 -> event 1
                TreelistView.Node node = eventView.Nodes[0].Nodes[0];

                ExpandNode(node);

                DeferredEvent evt = eventView.Nodes[0].Nodes.LastNode.Tag as DeferredEvent;

                m_Core.SetEventID(null, evt.frameID, evt.eventID + 1);

                eventView.NodesSelection.Clear();
                eventView.NodesSelection.Add(eventView.Nodes[0]);
                eventView.FocusedNode = eventView.Nodes[0];
            }
        }

        public void ExpandNode(TreelistView.Node node)
        {
            var n = node;
            while (node != null)
            {
                node.Expand();
                node = node.Parent;
            }
            eventView.EnsureVisible(n);
        }

        private bool SelectEvent(ref TreelistView.Node found, TreelistView.NodeCollection nodes, UInt32 frameID, UInt32 eventID)
        {
            foreach (var n in nodes)
            {
                DeferredEvent ndef = n.Tag is DeferredEvent ? n.Tag as DeferredEvent : null;
                DeferredEvent fdef = found != null && found.Tag is DeferredEvent ? found.Tag as DeferredEvent : null;

                if (ndef != null)
                {
                    if (ndef.eventID >= eventID && (found == null || ndef.eventID <= fdef.eventID))
                        found = n;

                    if (ndef.eventID == eventID && n.Nodes.Count == 0)
                        return true;
                }

                if (n.Nodes.Count > 0)
                {
                    bool exact = SelectEvent(ref found, n.Nodes, frameID, eventID);
                    if (exact) return true;
                }
            }

            return false;
        }

        private bool SelectEvent(UInt32 frameID, UInt32 eventID)
        {
            if (eventView.Nodes.Count == 0) return false;

            TreelistView.Node found = null;
            SelectEvent(ref found, eventView.Nodes[0].Nodes, frameID, eventID);
            if (found != null)
            {
                eventView.FocusedNode = found;

                ExpandNode(found);
                return true;
            }

            return false;
        }

        private void ClearFindIcons(TreelistView.NodeCollection nodes)
        {
            foreach (var n in nodes)
            {
                n.Image = null;

                if (n.Nodes.Count > 0)
                {
                    ClearFindIcons(n.Nodes);
                }
            }
        }

        private void ClearFindIcons()
        {
            if (eventView.Nodes.Count > 0)
            {
                ClearFindIcons(eventView.Nodes[0].Nodes);

                eventView.Invalidate();
            }
        }

        private int SetFindIcons(TreelistView.NodeCollection nodes, string filter)
        {
            int results = 0;

            foreach (var n in nodes)
            {
                if (n.Tag is DeferredEvent)
                {
                    if (n["Name"].ToString().ToUpperInvariant().Contains(filter))
                    {
                        n.Image = global::renderdocui.Properties.Resources.find;
                        results++;
                    }
                }

                if (n.Nodes.Count > 0)
                {
                    results += SetFindIcons(n.Nodes, filter);
                }
            }

            return results;
        }

        private int SetFindIcons(string filter)
        {
            if (filter.Length == 0)
                return 0;

            return SetFindIcons(eventView.Nodes[0].Nodes, filter.ToUpperInvariant());
        }

        private TreelistView.Node FindNode(TreelistView.NodeCollection nodes, string filter, UInt32 after)
        {
            foreach (var n in nodes)
            {
                if (n.Tag is DeferredEvent)
                {
                    if ((UInt32)n["EID"] > after && n["Name"].ToString().ToUpperInvariant().Contains(filter))
                        return n;
                }

                if (n.Nodes.Count > 0)
                {
                    TreelistView.Node found = FindNode(n.Nodes, filter, after);

                    if (found != null)
                        return found;
                }
            }

            return null;
        }

        private int FindEvent(TreelistView.NodeCollection nodes, string filter, UInt32 after, bool forward)
        {
			if(nodes == null) return -1;
		
            for (int i = forward ? 0 : nodes.Count - 1;
                 i >= 0 && i < nodes.Count;
                 i += forward ? 1 : -1)
            {
                var n = nodes[i];

                if (n.Tag is DeferredEvent)
                {
                    DeferredEvent def = n.Tag as DeferredEvent;

                    bool matchesAfter = (forward && def.eventID > after) || (!forward && def.eventID < after);

                    if (matchesAfter && n["Name"].ToString().ToUpperInvariant().Contains(filter))
                        return (int)def.eventID;
                }

                if (n.Nodes.Count > 0)
                {
                    int found = FindEvent(n.Nodes, filter, after, forward);

                    if (found > 0)
                        return found;
                }
            }

            return -1;
        }

        private int FindEvent(string filter, UInt32 after, bool forward)
        {
            if (eventView.Nodes.Count == 0)
                return 0;

            return FindEvent(eventView.Nodes[0].Nodes, filter.ToUpperInvariant(), after, forward);
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            SelectEvent(frameID, eventID);

            Invalidate();
        }

        private void eventView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (eventView.SelectedNode.Tag != null)
            {
                DeferredEvent def = eventView.SelectedNode.Tag as DeferredEvent;

                if (def.defCtx != ResourceId.Null)
                    m_Core.SetContextFilter(this, 0, def.eventID, def.defCtx, def.firstDefEv, def.lastDefEv);
                else
                    m_Core.SetEventID(this, 0, def.eventID);
            }
        }

        private void EventBrowser_Shown(object sender, EventArgs e)
        {
            if (m_Core.LogLoaded)
                OnLogfileLoaded();
        }

        private void ShowJump()
        {
            HideJumpAndFind();

            jumpStrip.Visible = true;
            findStrip.Visible = false;

            jumpToEID.Text = "";
            jumpToEID.Focus();
        }

        private void ShowFind()
        {
            HideJumpAndFind();

            jumpStrip.Visible = false;
            findStrip.Visible = true;

            findEvent.Text = "";
            findEvent.Focus();
            findEvent.BackColor = SystemColors.Window;
        }

        private void HideJumpAndFind()
        {
            jumpStrip.Visible = false;

            if (findEvent.Text.Length == 0)
            {
                findStrip.Visible = false;

                ClearFindIcons();
            }
        }

        private void eventView_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

            if(e.Control)
            {
                if (e.KeyCode == Keys.G)
                {
                    ShowJump();
                }
                if (e.KeyCode == Keys.F)
                {
                    ShowFind();
                }
                if (e.KeyCode == Keys.T)
                {
                    TimeDrawcalls();
                }
                if (e.KeyCode == Keys.C)
                {
                    string text = "";
                    for (int i = 0; i < eventView.FocusedNode.Count; i++)
                    {
                        text += DataToString(eventView.Columns[i], eventView.FocusedNode[i]) + " ";
                    }
                    text += Environment.NewLine;

                    Clipboard.SetText(text);
                }
            }
        }

        private void SelectColumns()
        {
            var columns = new Dictionary<string, bool>();
            foreach (var c in eventView.Columns)
                columns.Add(c.Fieldname, c.VisibleIndex >= 0);

            var cs = new Dialogs.ColumnSelector(columns, "Name");
            var result = cs.ShowDialog();

            if (result == DialogResult.OK)
            {
                columns = cs.GetColumnValues();

                foreach (var c in columns)
                {
                    var col = eventView.Columns[c.Key];

                    if (col == null) continue;

                    if (!c.Value && col.VisibleIndex >= 0)
                        eventView.Columns.SetVisibleIndex(col, -1);
                    else if (c.Value && col.VisibleIndex < 0)
                        eventView.Columns.SetVisibleIndex(col, eventView.Columns.VisibleColumns.Length);
                }
            }
        }

        private void TimeDrawcalls()
        {
            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                uint[] counters = { (uint)GPUCounters.EventGPUDuration };

                var avail = r.EnumerateCounters();

                var desc = r.DescribeCounter(counters[0]);

                Dictionary<uint, List<CounterResult>>[] times = new Dictionary<uint, List<CounterResult>>[m_Core.FrameInfo.Length];
                for (int curFrame = 0; curFrame < m_Core.FrameInfo.Length; curFrame++)
                    times[curFrame] = r.FetchCounters((UInt32)curFrame, 0, ~0U, counters);

                BeginInvoke((MethodInvoker)delegate
                {
                    var col = eventView.Columns["Duration"];
                    if (col.VisibleIndex == -1)
                    {
                        eventView.Columns.SetVisibleIndex(col, eventView.Columns.VisibleColumns.Length);
                    }

                    eventView.BeginUpdate();

                    for (int curFrame = 0; curFrame < m_FrameNodes.Count; curFrame++)
                        SetDrawcallTimes(m_FrameNodes[curFrame], times[curFrame]);

                    eventView.EndUpdate();
                });
            });
        }
        private void jumpFind_Leave(object sender, EventArgs e)
        {
            HideJumpAndFind();
        }

        private void jumpToEID_TextChanged(object sender, EventArgs e)
        {
            /*
            UInt32 eid = 0;
            if (UInt32.TryParse(jumpToEID.Text, out eid))
            {
                SelectEvent(0, eid);
            }
             */
        }

        private void findEvent_TextChanged(object sender, EventArgs e)
        {
            if (findEvent.Text.Length > 0)
            {
                findHighlight.Enabled = false;
                findHighlight.Enabled = true;
            }
            else
            {
                findHighlight.Enabled = false;

                findEvent.BackColor = SystemColors.Window;
                ClearFindIcons();
            }
        }

        private void findHighlight_Tick(object sender, EventArgs e)
        {
            if (findEvent.Text.Length == 0)
            {
                findEvent.BackColor = SystemColors.Window;
                ClearFindIcons();
            }

            ClearFindIcons();

            int results = SetFindIcons(findEvent.Text);

            if (results > 0)
                findEvent.BackColor = SystemColors.Window;
            else
                findEvent.BackColor = Color.Red;

            findHighlight.Enabled = false;
        }

        private void findEvent_KeyPress(object sender, KeyPressEventArgs e)
        {
            // escape key
            if (e.KeyChar == '\0')
            {
                findHighlight.Enabled = false;
                findEvent.Text = "";

                HideJumpAndFind();

                eventView.Focus();

                e.Handled = true;
            }
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                if (findHighlight.Enabled)
                {
                    findHighlight.Enabled = false;
                    findHighlight_Tick(sender, null);
                }

                if (findEvent.Text.Length > 0)
                {
                    Find(true);
                }

                e.Handled = true;
            }
        }

        private void jumpToEID_KeyPress(object sender, KeyPressEventArgs e)
        {
            // escape key
            if (e.KeyChar == '\0')
            {
                HideJumpAndFind();

                eventView.Focus();

                e.Handled = true;
            }
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                UInt32 eid = 0;
                if (UInt32.TryParse(jumpToEID.Text, out eid))
                {
                    SelectEvent(0, eid);
                }

                //HideJumpAndFind();

                e.Handled = true;
            }
        }

        private void closeFind_Click(object sender, EventArgs e)
        {
            findEvent.Text = "";

            HideJumpAndFind();

            eventView.Focus();
        }

        private void EventBrowser_Leave(object sender, EventArgs e)
        {
            HideJumpAndFind();
        }

        private void findNext_Click(object sender, EventArgs e)
        {
            Find(true);
        }

        private void findPrev_Click(object sender, EventArgs e)
        {
            Find(false);
        }

        private void Find(bool forward)
        {
            if (findEvent.Text.Length == 0)
                return;

            UInt32 curEID = m_Core.CurEvent;
            if (eventView.SelectedNode != null && eventView.Tag is DeferredEvent)
                curEID = (eventView.Tag as DeferredEvent).eventID;

            int eid = FindEvent(findEvent.Text, curEID, forward);
            if (eid >= 0)
            {
                SelectEvent(0, (UInt32)eid);
                findEvent.BackColor = SystemColors.Window;
            }
            else // if(WrapSearch)
            {
                eid = FindEvent(findEvent.Text, forward ? 0 : UInt32.MaxValue, forward);
                if (eid >= 0)
                {
                    SelectEvent(0, (UInt32)eid);
                    findEvent.BackColor = SystemColors.Window;
                }
                else
                {
                    findEvent.BackColor = Color.Red;
                }
            }
        }

        private void findEventButton_Click(object sender, EventArgs e)
        {
            bool show = !findStrip.Visible;

            HideJumpAndFind();

            eventView.Focus();

            if (show)
                ShowFind();
        }

        private void jumpEventButton_Click(object sender, EventArgs e)
        {
            bool show = !jumpStrip.Visible;

            HideJumpAndFind();

            eventView.Focus();

            if (show)
                ShowJump();
        }

        private void timeDraws_Click(object sender, EventArgs e)
        {
            TimeDrawcalls();
        }

        private void selectColumnsButton_Click(object sender, EventArgs e)
        {
            SelectColumns();
        }

        private void selectVisibleColumnsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SelectColumns();
        }

        private void EventBrowser_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }
    }
}
