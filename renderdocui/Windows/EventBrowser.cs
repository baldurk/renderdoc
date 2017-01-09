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
            public UInt32 eventID = 0;

            public bool marker = false;
        }

        private TreelistView.Node m_FrameNode = null;

        Dictionary<uint, List<CounterResult>> m_Times = new Dictionary<uint, List<CounterResult>>();

        private Core m_Core;

        public EventBrowser(Core core)
        {
            InitializeComponent();

            if (SystemInformation.HighContrast)
            {
                toolStrip1.Renderer = new ToolStripSystemRenderer();
                jumpStrip.Renderer = new ToolStripSystemRenderer();
                findStrip.Renderer = new ToolStripSystemRenderer();
                bookmarkStrip.Renderer = new ToolStripSystemRenderer();
            }

            Icon = global::renderdocui.Properties.Resources.icon;

            jumpToEID.Font =
                findEvent.Font =
                eventView.Font = 
                core.Config.PreferredFont;

            HideJumpAndFind();
            ClearBookmarks();

            m_Core = core;

            DockHandler.GetPersistStringCallback = PersistString;

            var col = eventView.Columns["Drawcall"]; eventView.Columns.SetVisibleIndex(col, -1);
            col = eventView.Columns["Duration"];     eventView.Columns.SetVisibleIndex(col, -1);

            UpdateDurationColumn();

            eventView.CellPainter.CellDataConverter = DataToString;

            findEventButton.Enabled = false;
            jumpEventButton.Enabled = false;
            timeDraws.Enabled = false;
            toggleBookmark.Enabled = false;
            export.Enabled = false;
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

        private TreelistView.Node MakeNode(UInt32 minEID, UInt32 maxEID, UInt32 minDraw, UInt32 maxDraw, string text, double duration)
        {
            string eidString = (maxEID == minEID) ? maxEID.ToString() : String.Format("{0}-{1}", minEID, maxEID);
            string drawString = (maxDraw == minDraw) ? maxDraw.ToString() : String.Format("{0}-{1}", minDraw, maxDraw);
            return new TreelistView.Node(new object[] {eidString, drawString, text.Replace("&", "&&"), duration });
        }

        private TreelistView.Node MakeNode(UInt32 EID, UInt32 draw, string text, double duration)
        {
            return new TreelistView.Node(new object[] { EID, draw, text.Replace("&", "&&"), duration });
        }

        private uint GetEndEventID(FetchDrawcall drawcall)
        {
            if (drawcall.children.Length == 0)
                return drawcall.eventID;

            return GetEndEventID(drawcall.children.Last());
        }

        private uint GetEndDrawID(FetchDrawcall drawcall)
        {
            if (drawcall.children.Length == 0)
                return drawcall.drawcallID;

            return GetEndDrawID(drawcall.children.Last());
        }

        public static bool ShouldHide(Core core, FetchDrawcall drawcall)
        {
            if (drawcall.flags.HasFlag(DrawcallFlags.PushMarker))
            {
                if (core.Config.EventBrowser_HideEmpty)
                {
                    if (drawcall.children == null || drawcall.children.Length == 0)
                        return true;

                    bool allhidden = true;

                    foreach (FetchDrawcall child in drawcall.children)
                    {
                        if (ShouldHide(core, child))
                            continue;

                        allhidden = false;
                        break;
                    }

                    if (allhidden)
                        return true;
                }

                if (core.Config.EventBrowser_HideAPICalls)
                {
                    if (drawcall.children == null || drawcall.children.Length == 0)
                        return false;

                    bool onlyapi = true;

                    foreach (FetchDrawcall child in drawcall.children)
                    {
                        if (ShouldHide(core, child))
                            continue;

                        if (!child.flags.HasFlag(DrawcallFlags.APICalls))
                        {
                            onlyapi = false;
                            break;
                        }
                    }

                    if (onlyapi)
                        return true;
                }
            }

            return false;
        }

        private TreelistView.Node AddDrawcall(FetchDrawcall drawcall, TreelistView.Node root)
        {
            if (EventBrowser.ShouldHide(m_Core, drawcall))
                return null;

            UInt32 eventNum = drawcall.eventID;
            TreelistView.Node drawNode = null;
            
            if(drawcall.children.Length > 0)
                drawNode = MakeNode(eventNum, GetEndEventID(drawcall), drawcall.drawcallID, GetEndDrawID(drawcall), drawcall.name, 0.0);
            else
                drawNode = MakeNode(eventNum, drawcall.drawcallID, drawcall.name, 0.0);

            if (m_Core.Config.EventBrowser_ApplyColours)
            {
                // if alpha isn't 0, assume the colour is valid
                if ((drawcall.flags & (DrawcallFlags.PushMarker | DrawcallFlags.SetMarker)) > 0 && drawcall.markerColour[3] > 0.0f)
                {
                    float red = drawcall.markerColour[0];
                    float green = drawcall.markerColour[1];
                    float blue = drawcall.markerColour[2];
                    float alpha = drawcall.markerColour[3];

                    drawNode.TreeLineColor = drawcall.GetColor();
                    drawNode.TreeLineWidth = 3.0f;

                    if (m_Core.Config.EventBrowser_ColourEventRow)
                    {
                        drawNode.BackColor = drawcall.GetColor();
                        drawNode.ForeColor = drawcall.GetTextColor(eventView.ForeColor);
                    }
                }
            }

            DeferredEvent def = new DeferredEvent();
            def.eventID = eventNum;
            def.marker = (drawcall.flags & DrawcallFlags.SetMarker) != 0;

            drawNode.Tag = def;

            if (drawcall.children != null && drawcall.children.Length > 0)
            {
                for (int i = 0; i < drawcall.children.Length; i++)
                {
                    AddDrawcall(drawcall.children[i], drawNode);

                    if (i > 0 && drawNode.Nodes.Count >= 2 &&
                        (drawcall.children[i - 1].flags & DrawcallFlags.SetMarker) > 0)
                    {
                        DeferredEvent markerTag = drawNode.Nodes[drawNode.Nodes.Count - 2].Tag as DeferredEvent;
                        DeferredEvent drawTag = drawNode.Nodes.LastNode.Tag as DeferredEvent;
                        markerTag.eventID = drawTag.eventID;
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

            if (drawNode.Nodes.IsEmpty() && (drawcall.flags & DrawcallFlags.PushMarker) != 0 && m_Core.Config.EventBrowser_HideEmpty)
                return null;

            root.Nodes.Add(drawNode);

            return drawNode;
        }

        private uint GetNodeEventID(TreelistView.Node n)
        {
            DeferredEvent def = n.Tag as DeferredEvent;

            if (def != null)
                return def.eventID;

            return 0;
        }

        private void SetDrawcallTimes(TreelistView.Node n, Dictionary<uint, List<CounterResult>> times)
        {
            if (n == null || times == null) return;

            // parent nodes take the value of the sum of their children
            double duration = 0.0;

            // look up leaf nodes in the dictionary
            if (n.Nodes.IsEmpty())
            {
                uint eid = GetNodeEventID(n);

                DeferredEvent def = n.Tag as DeferredEvent;

                if (def != null && def.marker)
                    duration = -1.0;
                else if (times.ContainsKey(eid))
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
            m_FrameNode = null;
            eventView.EndUpdate();

            prevDraw.Enabled = false;
            nextDraw.Enabled = false;

            ClearBookmarks();

            findEventButton.Enabled = false;
            jumpEventButton.Enabled = false;
            timeDraws.Enabled = false;
            toggleBookmark.Enabled = false;
            export.Enabled = false;
        }

        public void OnLogfileLoaded()
        {
            findEventButton.Enabled = true;
            jumpEventButton.Enabled = true;
            timeDraws.Enabled = true;
            toggleBookmark.Enabled = true;
            export.Enabled = true;

            prevDraw.Enabled = false;
            nextDraw.Enabled = false;

            ClearBookmarks();

            eventView.BeginUpdate();

            eventView.Nodes.Clear();

            {
                m_FrameNode = eventView.Nodes.Add(MakeMarker("Frame #" + m_Core.FrameInfo.frameNumber.ToString()));

                AddFrameDrawcalls(m_FrameNode, m_Core.GetDrawcalls());
            }

            eventView.EndUpdate();

            {
                // frame 1 -> event 1
                TreelistView.Node node = eventView.Nodes[0].Nodes[0];

                ExpandNode(node);

                DeferredEvent evt = eventView.Nodes[0].Nodes.LastNode.Tag as DeferredEvent;

                m_Core.SetEventID(null, evt.eventID + 1);

                m_FrameNode.Tag = evt;

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

        private bool FindEventNode(ref TreelistView.Node found, TreelistView.NodeCollection nodes, UInt32 eventID)
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
                    bool exact = FindEventNode(ref found, n.Nodes, eventID);
                    if (exact) return true;
                }
            }

            return false;
        }

        private bool FindEventNode(ref TreelistView.Node found, UInt32 eventID)
        {
            bool ret = FindEventNode(ref found, eventView.Nodes[0].Nodes, eventID);

            while (found != null && found.NextSibling != null && found.NextSibling.Tag is DeferredEvent)
            {
                DeferredEvent def = found.NextSibling.Tag as DeferredEvent;

                if (def.eventID == eventID)
                    found = found.NextSibling;
                else
                    break;
            }

            return ret;
        }

        private bool SelectEvent(UInt32 eventID)
        {
            if (eventView.Nodes.Count == 0) return false;

            TreelistView.Node found = null;
            FindEventNode(ref found, eventID);

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
                if (!IsBookmarked(GetNodeEventID(n)))
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
                        if (!IsBookmarked(GetNodeEventID(n)))
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
                    if (GetNodeEventID(n) > after && n["Name"].ToString().ToUpperInvariant().Contains(filter))
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

        public void OnEventSelected(UInt32 eventID)
        {
            SelectEvent(eventID);

            HighlightBookmarks();

            Invalidate();
        }

        private void eventView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            prevDraw.Enabled = false;
            nextDraw.Enabled = false;

            if (eventView.SelectedNode.Tag != null)
            {
                DeferredEvent def = eventView.SelectedNode.Tag as DeferredEvent;
                m_Core.SetEventID(this, def.eventID);

                FetchDrawcall draw = m_Core.CurDrawcall;

                if (draw != null && draw.previous != null)
                    prevDraw.Enabled = true;
                if (draw != null && draw.next != null)
                    nextDraw.Enabled = true;
            }

            HighlightBookmarks();
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
            if(!findStrip.Visible)
                HideJumpAndFind();

            jumpStrip.Visible = false;
            findStrip.Visible = true;

            findEvent.Focus();
            findEvent.BackColor = SystemColors.Window;
        }

        private void HideJumpAndFind()
        {
            jumpStrip.Visible = false;
            findStrip.Visible = false;

            ClearFindIcons();
        }

        private void eventView_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

            if (e.KeyCode == Keys.F3)
            {
                if(e.Shift)
                    Find(false);
                else
                    Find(true);
            }

            if(e.Control)
            {
                Keys[] digits = { Keys.D1, Keys.D2, Keys.D3, Keys.D4, Keys.D5,
                                  Keys.D6, Keys.D7, Keys.D8, Keys.D9, Keys.D0 };
                for (int i = 0; i < 10; i++)
                {
                    if (e.KeyCode == digits[i])
                    {
                        if (HasBookmark(i))
                        {
                            SelectEvent(GetBookmark(i));
                        }
                    }
                }
                if (e.KeyCode == Keys.B)
                {
                    ToggleBookmark(m_Core.CurEvent);
                }

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

                m_Times = r.FetchCounters(counters);

                BeginInvoke((MethodInvoker)delegate
                {
                    var col = eventView.Columns["Duration"];
                    if (col.VisibleIndex == -1)
                    {
                        eventView.Columns.SetVisibleIndex(col, eventView.Columns.VisibleColumns.Length);
                    }

                    eventView.BeginUpdate();

                    SetDrawcallTimes(m_FrameNode, m_Times);

                    eventView.EndUpdate();
                });
            });
        }
        private void jumpFind_Leave(object sender, EventArgs e)
        {
            if (findEvent.Text == "")
            {
                HideJumpAndFind();
            }
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

        private void findEvent_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.F3)
            {
                if (findHighlight.Enabled)
                    findHighlight.Enabled = false;

                findHighlight_Tick(sender, null);

                if (findEvent.Text.Length > 0)
                {
                    Find(e.Shift ? false : true);
                }

                e.Handled = true;
            }
        }

        private void findEvent_KeyPress(object sender, KeyPressEventArgs e)
        {
            // escape key
            if (e.KeyChar == '\0')
            {
                findHighlight.Enabled = false;

                HideJumpAndFind();

                eventView.Focus();

                e.Handled = true;
            }
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                if (findHighlight.Enabled)
                    findHighlight.Enabled = false;

                findHighlight_Tick(sender, null);

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
                    SelectEvent(eid);
                }

                //HideJumpAndFind();

                e.Handled = true;
            }
        }

        private void closeFind_Click(object sender, EventArgs e)
        {
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

        private void prevDraw_Click(object sender, EventArgs e)
        {
            FetchDrawcall draw = m_Core.CurDrawcall;

            if (draw != null && draw.previous != null)
                SelectEvent(draw.previous.eventID);
        }

        private void nextDraw_Click(object sender, EventArgs e)
        {
            FetchDrawcall draw = m_Core.CurDrawcall;

            if (draw != null && draw.next != null)
                SelectEvent(draw.next.eventID);
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
                SelectEvent((UInt32)eid);
                findEvent.BackColor = SystemColors.Window;
            }
            else // if(WrapSearch)
            {
                eid = FindEvent(findEvent.Text, forward ? 0 : UInt32.MaxValue, forward);
                if (eid >= 0)
                {
                    SelectEvent((UInt32)eid);
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

        private void bookmarkButton_Click(object sender, EventArgs e)
        {
            ToolStripButton but = sender as ToolStripButton;
            if (but != null)
            {
                SelectEvent((UInt32)but.Tag);
            }
        }

        private void toggleBookmark_Click(object sender, EventArgs e)
        {
            DeferredEvent def = eventView.SelectedNode.Tag as DeferredEvent;

            if (def != null)
            {
                ToggleBookmark(def.eventID);
            }
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

        private void eventViewRightClick_Opening(object sender, CancelEventArgs e)
        {
            collapseAll.Enabled = expandAll.Enabled = (eventView.SelectedNode != null && eventView.SelectedNode.HasChildren);
        }

        private void expandAll_Click(object sender, EventArgs e)
        {
            if(eventView.SelectedNode != null)
                eventView.SelectedNode.ExpandAll();
        }

        private void collapseAll_Click(object sender, EventArgs e)
        {
            if (eventView.SelectedNode != null)
                eventView.SelectedNode.CollapseAll();
        }

        private void EventBrowser_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }

        private List<UInt32> m_Bookmark = new List<UInt32>();
        private List<ToolStripButton> m_BookmarkButtons = new List<ToolStripButton>();

        private bool IsBookmarked(UInt32 EID)
        {
            return m_Bookmark.Contains(EID);
        }

        public bool HasBookmark(int index)
        {
            return index >= 0 && index < m_Bookmark.Count;
        }

        public UInt32 GetBookmark(int index)
        {
            if (!HasBookmark(index))
                return 0;
            return m_Bookmark[index];
        }

        private void HighlightBookmarks()
        {
            foreach (var b in m_BookmarkButtons)
            {
                UInt32 EID = (UInt32)b.Tag;
                if (m_Core.CurEvent == EID)
                    b.Checked = true;
                else
                    b.Checked = false;
            }
        }

        private void ClearBookmarks()
        {
            foreach(var b in m_BookmarkButtons)
                bookmarkStrip.Items.Remove(b);

            m_Bookmark.Clear();
            m_BookmarkButtons.Clear();

            bookmarkStrip.Visible = false;
        }

        private void ToggleBookmark(UInt32 EID)
        {
            int index = m_Bookmark.IndexOf(EID);

            TreelistView.Node found = null;
            FindEventNode(ref found, EID);

            if (index >= 0)
            {
                bookmarkStrip.Items.Remove(m_BookmarkButtons[index]);

                m_Bookmark.RemoveAt(index);
                m_BookmarkButtons.RemoveAt(index);

                found.Image = null;
            }
            else
            {
                ToolStripButton but = new ToolStripButton();

                but.DisplayStyle = ToolStripItemDisplayStyle.Text;
                but.Name = "bookmarkButton" + EID.ToString();
                but.Text = EID.ToString();
                but.Tag = EID;
                but.Size = new Size(23, 22);
                but.Click += new EventHandler(this.bookmarkButton_Click);

                but.Checked = true;

                bookmarkStrip.Items.Add(but);

                found.Image = global::renderdocui.Properties.Resources.asterisk_orange;

                m_Bookmark.Add(EID);
                m_BookmarkButtons.Add(but);
            }

            bookmarkStrip.Visible = m_BookmarkButtons.Count > 0;

            eventView.Invalidate();
        }

        private string GetExportDrawcallString(int indent, bool firstchild, FetchDrawcall drawcall)
        {
            string prefix = new string(' ', indent * 2 - (firstchild ? 1 : 0));
            if(firstchild)
                prefix += '\\';

            return String.Format("{0}- {1}", prefix, drawcall.name);
        }

        private void GetMaxNameLength(ref int maxNameLength, int indent, bool firstchild, FetchDrawcall drawcall)
        {
            string nameString = GetExportDrawcallString(indent, firstchild, drawcall);

            maxNameLength = Math.Max(maxNameLength, nameString.Length);

            for (int i = 0; i < drawcall.children.Length; i++)
                GetMaxNameLength(ref maxNameLength, indent + 1, i == 0, drawcall.children[i]);
        }

        private double GetDrawTime(FetchDrawcall drawcall)
        {
            if (drawcall.children.Length > 0)
            {
                double total = 0.0;

                foreach (FetchDrawcall c in drawcall.children)
                {
                    double f = GetDrawTime(c);
                    if(f >= 0)
                        total += f;
                }

                return total;
            }
            else if (m_Times.ContainsKey(drawcall.eventID))
            {
                return m_Times[drawcall.eventID][0].value.d;
            }

            return -1.0;
        }

        private void ExportDrawcall(StreamWriter sw, int maxNameLength, int indent, bool firstchild, FetchDrawcall drawcall)
        {
            string eidString = drawcall.children.Length > 0 ? "" : drawcall.eventID.ToString();

            string nameString = GetExportDrawcallString(indent, firstchild, drawcall);

            string line = String.Format("{0,-5} | {1,-" + maxNameLength + "} | {2,-6}", eidString, nameString, drawcall.drawcallID);

            if (m_Times.Count > 0)
            {
                if (m_Core.Config.EventBrowser_TimeUnit != m_TimeUnit)
                    UpdateDurationColumn();

                double f = GetDrawTime(drawcall);

                if (f >= 0)
                {
                    if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Milliseconds)
                        f *= 1000.0;
                    else if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Microseconds)
                        f *= 1000000.0;
                    else if (m_Core.Config.EventBrowser_TimeUnit == PersistantConfig.TimeUnit.Nanoseconds)
                        f *= 1000000000.0;

                    line += String.Format(" | {0}", Formatter.Format(f));
                }
                else
                {
                    line += " |";
                }
            }

            sw.WriteLine(line);

            for (int i = 0; i < drawcall.children.Length; i++)
                ExportDrawcall(sw, maxNameLength, indent + 1, i == 0, drawcall.children[i]);
        }

        private void export_Click(object sender, EventArgs e)
        {
            DialogResult res = exportDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                try
                {
                    using (Stream s = new FileStream(exportDialog.FileName, FileMode.Create))
                    {
                        StreamWriter sw = new StreamWriter(s);

                        sw.WriteLine(String.Format("{0} - Frame #{1}", m_Core.LogFileName, m_Core.FrameInfo.frameNumber));
                        sw.WriteLine("");

                        int maxNameLength = 0;

                        foreach (FetchDrawcall d in m_Core.GetDrawcalls())
                            GetMaxNameLength(ref maxNameLength, 0, false, d);

                        string line = String.Format(" EID  | {0,-" + maxNameLength + "} | Draw #", "Event");

                        if (m_Times.Count > 0)
                        {
                            if (m_Core.Config.EventBrowser_TimeUnit != m_TimeUnit)
                                UpdateDurationColumn();

                            line += String.Format(" | {0}", eventView.Columns["Duration"].Caption);
                        }

                        sw.WriteLine(line);

                        line = String.Format("--------{0}-----------", new string('-', maxNameLength));

                        if (m_Times.Count > 0)
                        {
                            int maxDurationLength = 0;
                            maxDurationLength = Math.Max(maxDurationLength, Formatter.Format(1.0).Length);
                            maxDurationLength = Math.Max(maxDurationLength, Formatter.Format(1.2345e-200).Length);
                            maxDurationLength = Math.Max(maxDurationLength, Formatter.Format(123456.7890123456789).Length);
                            line += new string('-', 3 + maxDurationLength); // 3 extra for " | "
                        }

                        sw.WriteLine(line);

                        foreach (FetchDrawcall d in m_Core.GetDrawcalls())
                            ExportDrawcall(sw, maxNameLength, 0, false, d);

                        sw.Dispose();
                    }
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save to " + exportDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot save",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }
    }
}
