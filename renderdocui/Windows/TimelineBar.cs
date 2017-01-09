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
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    // this entire class needs burning down and rewriting from scratch.
    public partial class TimelineBar : DockContent, ILogViewerForm
    {
        private Core m_Core;
        private float m_Zoom = 1.0f;

        public TimelineBar(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

            this.DoubleBuffered = true;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            panel.Painting = true;

            panel.MouseWheelHandler = new MouseEventHandler(panel_MouseWheel);

            UpdateScrollbar(1.0f, 0.0f);
        }

        void panel_MouseWheel(object sender, MouseEventArgs e)
        {
            double z = Math.Log(m_Zoom);

            z += e.Delta / 2500.0;

            UpdateScrollbar(Math.Max(1.0f, (float)Math.Exp(z)), (float)e.X / (float)panel.ClientRectangle.Width);
        }

        public void OnLogfileClosed()
        {
            m_Root = null;
            m_HighlightResource = ResourceId.Null;
            m_HighlightUsage = null;

            panel.Invalidate();
            UpdateScrollbar(1.0f, 0.0f);
        }

        public void OnLogfileLoaded()
        {
            m_Root = GatherEvents(m_Core.CurDrawcalls);
            panel.Invalidate();
            UpdateScrollbar(1.0f, 0.0f);
        }

        public void OnEventSelected(UInt32 eventID)
        {
            panel.Invalidate();
        }

        private ResourceId m_HighlightResource = ResourceId.Null;
        private string m_HighlightName = "";
        private EventUsage[] m_HighlightUsage = null;

        public void HighlightResource(ResourceId id, string name, EventUsage[] usage)
        {
            m_HighlightResource = id;
            m_HighlightName = name;
            m_HighlightUsage = usage;
            panel.Invalidate();
        }

        private FetchTexture m_HistoryTex = null;
        private Point m_HistoryPoint = Point.Empty;
        private PixelModification[] m_History = null;

        public void HighlightHistory(FetchTexture tex, Point pt, PixelModification[] modif)
        {
            m_HistoryTex = tex;
            m_HistoryPoint = pt;
            m_History = modif;
            panel.Invalidate();
        }

        private RectangleF GetSubrect(RectangleF rect, float startSeg, float segWidth)
        {
            var subRect = rect;
            subRect.X = rect.X + rect.Width * startSeg;
            subRect.Width = rect.Width * segWidth;

            return subRect;
        }

        private float[] lastPipX = new float[] { -100.0f, -100.0f, -100.0f };

        private float DrawPip(Graphics g, Color col, RectangleF rect, int type,
                                int idx, int numChildren, float startSeg, float segWidth, string text)
        {
            var subRect = GetSubrect(rect, startSeg, segWidth);
            subRect.X += pipRadius;
            subRect.Y += pipPaddingY;
            subRect.Width -= pipRadius * 2;
            subRect.Height = pipRadius * 2;

            float delta = (float)(idx + 1) / (float)(numChildren + 1);

            float x = subRect.X - pipRadius + delta * Math.Max(0, subRect.Width);

            float y = subRect.Y;
            float width = pipRadius * 2;
            float height = pipRadius * 2;

            if (type == 0)
            {
                using(var brush = new SolidBrush(col))
                    g.FillPie(brush, x, y, width, height, 0.0f, 360.0f);
            }
            else if(type < 999)
            {
                height += 2;
                width += 4;
                x -= 2;

                PointF[] uptri = { new PointF(x, y+height-2),
                                      new PointF(x+width, y+height-2),
                                      new PointF(x+width/2.0f, y+2) };

                bool update = true;

                if (type > 2)
                {
                    if (type == 3)
                    {
                        uptri[1] = new PointF(x + width / 2.0f, y + height - 2);
                        update = false;
                        type = 1;
                    }
                    if (type == 4)
                    {
                        uptri[0] = new PointF(x + width / 2.0f, y + height - 2);
                        type = 1;
                    }
                    if (type == 5)
                    {
                        update = false;
                        type = 1;
                    }
                    if (type == 6)
                    {
                        update = false;
                        type = 2;
                    }
                }

                if (x - lastPipX[type] > pipRadius*2.0f)
                {
                    if (type == 2)
                    {
                        using (var pen = new Pen(col, 2))
                            g.DrawPolygon(pen, uptri);
                    }
                    else
                    {
                        using (var brush = new SolidBrush(col))
                            g.FillPolygon(brush, uptri);
                    }

                    if(update)
                        lastPipX[type] = x;
                }
            }

            return x + width / 2;
        }

        private const int pipPaddingY = 8;
        private const int pipRadius = 5;
        private const int barPadding = 4;
        private const int barHeight = 24;
        private const float barBorder = 1.0f;
        private Font barFont = new Font("Consolas", 10.0f, FontStyle.Regular);

        private Color lightBack = Color.FromArgb(235, 235, 235);
        private Color darkBack = Color.FromArgb(200, 200, 200);

        private float MinBarSize(Graphics g, string text)
        {
            var size = g.MeasureString(text, barFont);

            return size.Width + barBorder*2 + barPadding*2;
        }

        class Range
        {
            public Range(uint f, uint l) { first = f; last = l; }
            public uint first;
            public uint last;
        };

        private List<Range> m_ranges = new List<Range>();
        private uint maxeid = 100000000;

        private void MarkWrite(uint eid)
        {
            if (m_ranges.Count == 0 || m_ranges[m_ranges.Count-1].last < maxeid)
                m_ranges.Add(new Range(eid, maxeid));
        }

        private void MarkRead(uint eid)
        {
            if (m_ranges.Count == 0)
                m_ranges.Add(new Range(0, eid));
            else
                m_ranges[m_ranges.Count-1].last = eid;
        }

        private RectangleF DrawBar(Graphics g, Color back, Color fore, RectangleF rect, float startSeg, float segWidth, string text, bool visible)
        {
            var subRect = GetSubrect(rect, startSeg, segWidth);
            subRect.Height = barHeight;

            if (subRect.Contains(markerPos) && visible && showMarker && markerPos.Y < ClientRectangle.Height - pipRadius*6)
            {
                back = Color.LightYellow;
                fore = Color.Black;
                Cursor = Cursors.Hand;
            }

            if (visible)
            {
                using(var brush = new SolidBrush(back))
                    g.FillRectangle(brush, subRect.X, subRect.Y, subRect.Width, subRect.Height);

                using(var pen = new Pen(Brushes.Black, barBorder))
                    g.DrawRectangle(pen, subRect.X, subRect.Y, subRect.Width, subRect.Height);

                var p = new Pen(Color.FromArgb(230, 230, 230), 1);

                g.DrawLine(p, new PointF(subRect.Left, subRect.Bottom), new PointF(subRect.Left, rect.Bottom - pipRadius * 6));
                g.DrawLine(p, new PointF(subRect.Right, subRect.Bottom), new PointF(subRect.Right, rect.Bottom - pipRadius * 6));

                p.Dispose();
            }

            var stringSize = g.MeasureString(text, barFont);

            float left = subRect.X + barPadding;

            if(left < barPadding)
                left = Math.Min(barPadding, Math.Max(subRect.X + subRect.Width - barPadding * 2 - stringSize.Width, left));

            var textRect = new RectangleF(left, subRect.Y + barPadding,
                                           subRect.Width - barPadding*2, subRect.Height - barPadding);

            textRect.Height = Math.Min(textRect.Height, rect.Height - pipRadius * 6);

            if (visible)
            {
                g.Clip = new Region(textRect);
                using (var brush = new SolidBrush(fore))
                    g.DrawString(text, barFont, brush, textRect.X, textRect.Y);
                g.ResetClip();
            }

            var ret = rect;
            ret.X = subRect.X;
            ret.Y = subRect.Y + subRect.Height;
            ret.Width = subRect.Width;
            ret.Height = rect.Height - subRect.Height;

            return ret;
        }

        private class Section
        {
            public string Name = "";

            public Color color;
            public Color textcolor;

            public bool Expanded = false;
            public List<Section> subsections = null;
            public List<FetchDrawcall> draws = null;

            public RectangleF lastRect = new RectangleF();
            public List<float> lastPoss = new List<float>();
            public List<bool> lastUsage = new List<bool>();
            public List<bool> lastVisible = new List<bool>();
        }

        private Section m_Root = null;

        private Section GatherEvents(FetchDrawcall[] draws)
        {
            var sections = new List<List<FetchDrawcall>>();

            foreach (var d in draws)
            {
                if ((d.flags & (DrawcallFlags.SetMarker | DrawcallFlags.Present)) > 0)
                    continue;

                if(EventBrowser.ShouldHide(m_Core, d))
                    continue;

                bool newSection = ((d.flags & (DrawcallFlags.PushMarker|DrawcallFlags.MultiDraw)) > 0 || sections.Count == 0);
                if (!newSection)
                {
                    var lastSection = sections.Last();

                    if (lastSection.Count == 1 && (lastSection[0].flags & (DrawcallFlags.PushMarker | DrawcallFlags.MultiDraw)) > 0)
                        newSection = true;
                }

                if (newSection)
                    sections.Add(new List<FetchDrawcall>());

                sections.Last().Add(d);
            }

            Section ret = new Section();
            ret.subsections = new List<Section>();

            foreach (var s in sections)
            {
                Section sec = null;

                if (s.Count == 1 && (s[0].flags & (DrawcallFlags.PushMarker | DrawcallFlags.MultiDraw)) > 0)
                {
                    sec = GatherEvents(s[0].children);
                    if (m_Core.Config.EventBrowser_ApplyColours)
                    {
                        sec.color = s[0].GetColor();
                        sec.textcolor = s[0].GetTextColor(Color.Black);
                    }
                    else
                    {
                        sec.color = Color.Transparent;
                        sec.textcolor = Color.Black;
                    }
                    sec.Name = s[0].name;
                }
                else
                {
                    sec = new Section();
                    sec.draws = s;
                    for (int i = 0; i < sec.draws.Count; i++)
                    {
                        sec.lastPoss.Add(0.0f);
                        sec.lastUsage.Add(false);
                        sec.lastVisible.Add(false);
                    }
                }

                ret.subsections.Add(sec);
            }

            return ret;
        }

        private float MinSectionSize(Graphics g, Section s)
        {
            float myWidth = 20.0f;

            if (s.Name.Length > 0)
                myWidth = Math.Max(myWidth, MinBarSize(g, "+ " + s.Name));

            if (s.subsections == null || s.subsections.Count == 0)
                return myWidth;

            float childWidth = 0.0f;

            if(s.Expanded && s.subsections != null)
                foreach (var sub in s.subsections)
                    childWidth += MinSectionSize(g, sub);

            return Math.Max(myWidth, childWidth);
        }

        private void RenderSection(int depth, Graphics g, RectangleF rect, Section section, bool visible, float lastVisibleHeight)
        {
            float start = 0.0f;

            float[] minwidths = new float[section.subsections.Count];
            float[] widths = new float[section.subsections.Count];
            float maxwidth = 0.0f;
            float totalwidth = 0.0f;
            for (int i = 0; i < section.subsections.Count; i++)
            {
                // initial widths are minwidth, used to 'proportionally' size sections.
                // all that matters here is the relative proportions and that it's >= minwidth,
                // it gets sized up or down to fit later.
                minwidths[i] = MinSectionSize(g, section.subsections[i]);
                widths[i] = minwidths[i];

                maxwidth = Math.Max(maxwidth, minwidths[i]);
                totalwidth += widths[i];
            }

            float scale = rect.Width / totalwidth;

            // if we have space free, scale everything up the same
            if (totalwidth < rect.Width)
            {
                for (int i = 0; i < section.subsections.Count; i++)
                    widths[i] *= scale;
            }
            else
            {
                // scale everything down to fit (this will reduce some below minwidth)
                for (int i = 0; i < section.subsections.Count; i++)
                    widths[i] *= scale;

                // search for sections that are > their min width, and skim off the top.
                for (int i = 0; i < section.subsections.Count; i++)
                {
                    // found a section that's too small
                    if (widths[i] < minwidths[i])
                    {
                        // we try and skim an equal amount off every section, so the scaling
                        // is nice and uniform rather than left-focussed
                        float missing = minwidths[i] - widths[i];
                        float share = missing / (float)(section.subsections.Count - 1);

                        bool slack = false;

                        // keep going trying to find some slack and skimming it off
                        int iters = 0;
                        do
                        {
                            slack = false;
                            iters++;
                            if (iters == 10) break;
                            for (int j = 0; j < section.subsections.Count; )
                            {
                                // ignore current section
                                if (i == j)
                                {
                                    j++;
                                    continue;
                                }

                                // if this section has free space
                                if (widths[j] > minwidths[j])
                                {
                                    float avail = widths[j] - minwidths[j];

                                    // skim off up as much as is available, up to the share per section
                                    float delta = Math.Max(0.1f, Math.Min(avail, share));
                                    widths[i] += delta;
                                    widths[j] -= delta;
                                    missing -= delta;

                                    // if we didn't skim off our share, recalculate how much we'll need to
                                    // skim off each, and start again
                                    if (avail < share)
                                    {
                                        share = missing / (float)(section.subsections.Count - 1);
                                        j = 0;
                                        continue;
                                    }

                                    slack = true;
                                }

                                j++;
                            }
                            // keep going while there are sections with slack, and we need to make some up.
                            // ie. if we find all our missing space then we can finish, but also if we haven't
                            // found enough but there's nothing to give, we also give up.
                        } while (slack && missing > 0.0f);
                    }
                }
            }

            for (int i = 0; i < section.subsections.Count; i++)
                widths[i] /= rect.Width;

            var clipRect = rect;
            clipRect.Height -= pipRadius * 6;

            for (int i = 0; i < section.subsections.Count; i++)
            {
                var s = section.subsections[i];
                if (s.Name.Length > 0)
                {
                    var col = depth % 2 == 0 ? lightBack : darkBack;
                    var textcol = Color.Black;

                    if (s.color.A > 0)
                    {
                        col = s.color;
                        textcol = s.textcolor;
                    }

                    g.Clip = new Region(clipRect);
                    var childRect = DrawBar(g, col, textcol, rect, start, widths[i], (s.Expanded ? "- " : "+ ") + s.Name, visible);
                    g.ResetClip();

                    RenderSection(depth + 1, g, childRect, s, visible && s.Expanded, visible ? childRect.Top : lastVisibleHeight);

                    var backRect = GetSubrect(childRect, 0.0f, 1.0f);
                    backRect.Y += barBorder / 2;
                    backRect.Height = barHeight;

                    LinearGradientBrush brush = new LinearGradientBrush(new PointF(0, backRect.Y),
                                                                        new PointF(0, backRect.Y + backRect.Height + 1),
                                                                        Color.FromArgb(255, darkBack), Color.FromArgb(0, darkBack));

                    if (visible && s.Expanded && (s.subsections == null || s.subsections.Count == 0))
                    {
                        g.Clip = new Region(clipRect);
                        g.FillRectangle(brush, backRect);
                        g.ResetClip();
                    }

                    brush.Dispose();

                    s.lastRect = childRect;
                    s.lastRect.Y = rect.Y;
                    s.lastRect.Height = childRect.Y - rect.Y;

                    if (!visible) s.lastRect.Height = 0;
                }
                else
                {
                    var backRect = GetSubrect(rect, start, widths[i]);
                    backRect.Y += barBorder/2;
                    backRect.Height = barHeight;

                    LinearGradientBrush brush = new LinearGradientBrush(new PointF(0, backRect.Y),
                                                                        new PointF(0, backRect.Y + backRect.Height + 1),
                                                                        Color.FromArgb(255, darkBack), Color.FromArgb(0, darkBack));

                    if (visible)
                    {
                        g.Clip = new Region(clipRect);
                        g.FillRectangle(brush, backRect);
                        g.ResetClip();
                    }

                    brush.Dispose();

                    int highlight = -1;

                    var highlightBarRect = rect;
                    highlightBarRect.Y = highlightBarRect.Bottom - pipRadius * 4;
                    highlightBarRect.Height = pipRadius * 2;

                    if(s.draws != null)
                    {
                        for (int d = 0; d < s.draws.Count; d++)
                        {
                            if (m_History != null)
                            {
                                foreach (var u in m_History)
                                {
                                    if (u.eventID == s.draws[d].eventID)
                                    {
                                        var barcol = Color.Black;

                                        int type = 2;

                                        DrawPip(g, barcol, highlightBarRect, type, d, s.draws.Count, start, widths[i], "");
                                    }
                                }
                            }
                            else if (m_HighlightUsage != null)
                            {
                                foreach (var u in m_HighlightUsage)
                                {
                                    if ((u.eventID == s.draws[d].eventID) ||
                                        (u.eventID < s.draws[d].eventID && s.draws[d].events.Length > 0 && u.eventID >= s.draws[d].events[0].eventID))
                                    {
                                        var barcol = Color.Black;

                                        int type = 2;

                                        if (u.usage == ResourceUsage.Barrier)
                                            type = 6;

                                        DrawPip(g, barcol, highlightBarRect, type, d, s.draws.Count, start, widths[i], "");
                                    }
                                }
                            }

                            s.lastUsage[d] = false;
                            s.lastVisible[d] = visible;
                        }

                        for (int d = 0; d < s.draws.Count; d++)
                        {
                            if (m_History != null)
                            {
                                foreach (var u in m_History)
                                {
                                    if (u.eventID == s.draws[d].eventID)
                                    {
                                        if (u.EventPassed())
                                        {
                                            DrawPip(g, Color.Lime, highlightBarRect, 1, d, s.draws.Count, start, widths[i], "");
                                            MarkWrite(s.draws[d].eventID);
                                        }
                                        else
                                        {
                                            DrawPip(g, Color.Crimson, highlightBarRect, 1, d, s.draws.Count, start, widths[i], "");
                                            MarkRead(s.draws[d].eventID);
                                        }

                                        s.lastUsage[d] = true;
                                    }
                                }
                            }
                            else if (m_HighlightUsage != null)
                            {
                                foreach (var u in m_HighlightUsage)
                                {
                                    if ((u.eventID == s.draws[d].eventID) ||
                                        (u.eventID < s.draws[d].eventID && s.draws[d].events.Length > 0 && u.eventID >= s.draws[d].events[0].eventID))
                                    {
                                        // read/write
                                        if (
                                            ((int)u.usage >= (int)ResourceUsage.VS_RWResource &&
                                             (int)u.usage <= (int)ResourceUsage.All_RWResource) ||
                                            u.usage == ResourceUsage.GenMips ||
                                            u.usage == ResourceUsage.Copy ||
                                            u.usage == ResourceUsage.Resolve)
                                        {
                                            DrawPip(g, Color.Orchid, highlightBarRect, 3, d, s.draws.Count, start, widths[i], "");
                                            DrawPip(g, Color.Lime, highlightBarRect, 4, d, s.draws.Count, start, widths[i], "");
                                            MarkWrite(s.draws[d].eventID);
                                        }
                                        // write
                                        else if (u.usage == ResourceUsage.SO ||
                                                 u.usage == ResourceUsage.DepthStencilTarget ||
                                                 u.usage == ResourceUsage.ColourTarget ||
                                                 u.usage == ResourceUsage.CopyDst ||
                                                 u.usage == ResourceUsage.ResolveDst)
                                        {
                                            DrawPip(g, Color.Orchid, highlightBarRect, 1, d, s.draws.Count, start, widths[i], "");
                                            MarkWrite(s.draws[d].eventID);
                                        }
                                        // clear
                                        else if (u.usage == ResourceUsage.Clear)
                                        {
                                            DrawPip(g, Color.Silver, highlightBarRect, 1, d, s.draws.Count, start, widths[i], "");
                                            MarkWrite(s.draws[d].eventID);
                                        }
                                        // barrier
                                        else if (u.usage == ResourceUsage.Barrier)
                                        {
                                            DrawPip(g, Color.Tomato, highlightBarRect, 5, d, s.draws.Count, start, widths[i], "");
                                            MarkWrite(s.draws[d].eventID);
                                        }
                                        // read
                                        else
                                        {
                                            DrawPip(g, Color.Lime, highlightBarRect, 1, d, s.draws.Count, start, widths[i], "");
                                            MarkRead(s.draws[d].eventID);
                                        }

                                        s.lastUsage[d] = true;
                                    }
                                }
                            }
                        }

                        for (int d = 0; d < s.draws.Count; d++)
                        {
                            if (s.draws[d].eventID == m_Core.CurEvent)
                                highlight = d;

                            if (visible)
                            {
                                g.Clip = new Region(clipRect);

                                if (s.draws[d].eventID != m_Core.CurEvent)
                                    s.lastPoss[d] = DrawPip(g, Color.Blue, rect, 0, d, s.draws.Count, start, widths[i], s.draws[d].name);

                                g.ResetClip();
                            }
                            else
                            {
                                s.lastPoss[d] = DrawPip(g, Color.Blue, rect, 999, d, s.draws.Count, start, widths[i], s.draws[d].name);
                            }
                        }
                    }

                    if (highlight >= 0)
                    {
                        var subRect = GetSubrect(rect, start, widths[i]);
                        subRect.X += pipRadius;
                        subRect.Y += pipPaddingY;
                        subRect.Width -= pipRadius * 2;
                        subRect.Height = pipRadius * 2;

                        float delta = (float)(highlight + 1) / (float)(s.draws.Count + 1);

                        m_CurrentMarker = new PointF(subRect.X + delta * Math.Max(0, subRect.Width), lastVisibleHeight);
                    }

                    if (highlight >= 0 && visible && s.draws != null)
                    {
                        g.Clip = new Region(clipRect);

                        s.lastPoss[highlight] = DrawPip(g, Color.LightGreen, rect, 0, highlight, s.draws.Count, start, widths[i], s.draws[highlight].name);

                        g.ResetClip();
                    }

                    s.lastRect = backRect;

                    if (!visible) s.lastRect.Height = 0;
                }

                start += widths[i];
            }
        }

        private PointF m_CurrentMarker = new PointF(-1, -1);

        private bool m_FailedPaint = false;

        private void panel_Paint(object sender, PaintEventArgs e)
        {
            if(ClientRectangle.Width <= 0 || ClientRectangle.Height <= 0)
                return;

            Cursor = Cursors.Arrow;

            Bitmap bmp = null;

            try
            {
                bmp = new Bitmap(ClientRectangle.Width, ClientRectangle.Height);
            }
            catch (System.ArgumentException)
            {
                // out of memory or huge bitmap. Clear to black rather than crashing
                e.Graphics.Clear(Color.Black);

                if(!m_FailedPaint)
                {
                    renderdoc.StaticExports.LogText("Failed to paint TimelineBar - System.ArgumentException");
                    m_FailedPaint = true;
                }
                return;
            }

            var g = Graphics.FromImage(bmp);

            g.SmoothingMode = SmoothingMode.AntiAlias;

            var clientRect = panel.ClientRectangle;

            var rect = clientRect;
            rect.Inflate(-4, -4);
            rect.Width = (int)(rect.Width * m_Zoom);
            rect.X -= (int)(rect.Width * ScrollPos);

            if (m_Core.LogLoaded)
                Text = "Timeline - Frame #" + m_Core.FrameInfo.frameNumber;
            else
                Text = "Timeline";

            g.Clear(Color.White);

            var barRect = new Rectangle(clientRect.Left + 1, clientRect.Bottom - pipRadius * 6, clientRect.Width - 2, pipRadius * 6 - 2);

            using (var brush = new SolidBrush(Color.Azure))
                g.FillRectangle(brush, barRect);

            using (var pen = new Pen(Brushes.Black, 2))
            {
                g.DrawLine(pen,
                        new Point(clientRect.Left, clientRect.Bottom - pipRadius * 6),
                        new Point(clientRect.Right, clientRect.Bottom - pipRadius * 6));

                g.DrawRectangle(pen, new Rectangle(clientRect.Left, clientRect.Top, clientRect.Width - 1, clientRect.Height - 1));
            }

            lastPipX[0] = lastPipX[1] = lastPipX[2] = -100.0f;

            if (m_History != null)
            {
                g.DrawString("Pixel history for " + m_HistoryTex.name, barFont, Brushes.Black, barRect.X, barRect.Y + 2);
            }
            else if (m_HighlightResource != ResourceId.Null)
            {
                g.DrawString(m_HighlightName + " Reads", barFont, Brushes.Black, barRect.X, barRect.Y + 2);
                barRect.X += (int)Math.Ceiling(g.MeasureString(m_HighlightName + " Reads", barFont).Width);
                barRect.X += pipRadius;

                DrawPip(g, Color.Black, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 2, 0, 1, 0.0f, 1.0f, "");
                DrawPip(g, Color.Lime, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 1, 0, 1, 0.0f, 1.0f, "");

                barRect.X += pipRadius * 2;
                barRect.X += pipRadius;

                g.DrawString(", Clears ", barFont, Brushes.Black, barRect.X, barRect.Y + 2);
                barRect.X += (int)Math.Ceiling(g.MeasureString(", Clears ", barFont).Width);
                barRect.X += pipRadius;

                DrawPip(g, Color.Black, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 2, 0, 1, 0.0f, 1.0f, "");
                DrawPip(g, Color.Silver, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 1, 0, 1, 0.0f, 1.0f, "");

                if (m_Core.CurPipelineState.SupportsBarriers)
                {
                    barRect.X += pipRadius * 2;
                    barRect.X += pipRadius;

                    g.DrawString(", Barriers ", barFont, Brushes.Black, barRect.X, barRect.Y + 2);
                    barRect.X += (int)Math.Ceiling(g.MeasureString(", Barriers ", barFont).Width);
                    barRect.X += pipRadius;

                    DrawPip(g, Color.Black, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 2, 0, 1, 0.0f, 1.0f, "");
                    DrawPip(g, Color.Tomato, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 1, 0, 1, 0.0f, 1.0f, "");
                }

                barRect.X += pipRadius * 2;
                barRect.X += pipRadius;

                g.DrawString(" and Writes  ", barFont, Brushes.Black, barRect.X, barRect.Y + 2);
                barRect.X += (int)Math.Ceiling(g.MeasureString(" and Writes", barFont).Width);
                barRect.X += pipRadius;

                DrawPip(g, Color.Black, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 2, 0, 1, 0.0f, 1.0f, "");
                DrawPip(g, Color.Orchid, new RectangleF(barRect.X, barRect.Y - pipRadius, pipRadius * 2, pipRadius * 2), 1, 0, 1, 0.0f, 1.0f, "");
            }

            m_CurrentMarker = new PointF(-1, -1);

            m_ranges.Clear();

            if (m_Core.LogLoaded)
            {
                var frameRect = rect;

                using (var pen = new Pen(Brushes.Black, 2))
                    g.DrawLine(pen, new Point(frameRect.Left, frameRect.Top), new Point(frameRect.Right, frameRect.Top));

                var childRect = frameRect;
                childRect.Y += 1;

                lastPipX[0] = lastPipX[1] = lastPipX[2] = -100.0f;

                if (m_Root != null)
                    RenderSection(1, g, childRect, m_Root, true, childRect.Top);

                /*
                if (m_ranges.Count > 0)
                {
                 * var pen = new Pen(Brushes.Red, 8);
                    foreach (Range r in m_ranges)
                    {
                        float a = r.first == 0 ? 0.0f : GetEIDPoint(m_Root, r.first);
                        float b = r.last == maxeid ? clientRect.Width : GetEIDPoint(m_Root, r.last);

                        g.DrawLine(pen, new Point(clientRect.Left + (int)a, clientRect.Bottom - pipRadius * 3),
                                new Point(clientRect.Left + (int)b, clientRect.Bottom - pipRadius * 3));
                    }
                 * pen.Dispose();
                }
                 */
            }

            if (m_CurrentMarker.X >= 0)
            {
                using (var pen = new Pen(Color.FromArgb(200, 200, 200), 2.0f))
                    g.DrawLine(pen,
                                new PointF(m_CurrentMarker.X, m_CurrentMarker.Y),
                                new PointF(m_CurrentMarker.X, clientRect.Bottom - pipRadius * 6 - 2));
            }

            if (showMarker)
            {
                using (var pen = new Pen(Brushes.Red, 2.0f))
                    g.DrawLine(pen, new Point(markerPos.X, 0), new Point(markerPos.X, ClientRectangle.Height));
            }

            e.Graphics.DrawImage(bmp, 0, 0, ClientRectangle, GraphicsUnit.Pixel);
            g.Dispose();
            bmp.Dispose();
        }

        private float ScrollPos
        {
            get
            {
                if (m_Zoom <= 1.0f)
                    return 0.0f;

                return (float)scroll.Value / (float)scroll.Maximum;
            }
        }

        private void UpdateScrollbar(float newZoom, float zoomPos)
        {
            if (newZoom <= 1.0f)
                scroll.Visible = false;
            else
                scroll.Visible = true;

            // get previous zoom at zoomPos
            float prev = ScrollPos + zoomPos / m_Zoom;

            scroll.Maximum = 200;
            scroll.LargeChange = 100;
            scroll.SmallChange = 10;
            scroll.Maximum = (int)(100.0f * newZoom) - 1;

            // set value so it is the same, centred around zoomPos
            scroll.Value = (int)Math.Round(Math.Max(0, Math.Min(1, prev - (zoomPos / newZoom))) * scroll.Maximum);

            // Hackity-hack! Clamp from the right
            if(scroll.Maximum > scroll.LargeChange)
                scroll.Value = Math.Min(scroll.Value, scroll.Maximum - scroll.LargeChange + 1);

            m_Zoom = newZoom;

            panel.Invalidate();
        }

        private void scroll_Scroll(object sender, ScrollEventArgs e)
        {
            panel.Invalidate();
        }

        private bool showMarker = false;
        private Point markerPos = Point.Empty;

        private void panel_MouseLeave(object sender, EventArgs e)
        {
            showMarker = false;
            markerPos = Point.Empty;

            panel.Invalidate();

            Cursor = Cursors.Arrow;
        }

        private void panel_MouseEnter(object sender, EventArgs e)
        {
            showMarker = true;
        }

        private void panel_MouseMove(object sender, MouseEventArgs e)
        {
            markerPos = e.Location;

            panel.Invalidate();
        }

        private void TimelineBar_Resize(object sender, EventArgs e)
        {
            panel.Invalidate();
        }

        private float GetEIDPoint(Section s, uint eid)
        {
            if (s == null)
                return -1.0f;

            if (s.subsections != null)
            {
                foreach (var sub in s.subsections)
                {
                    float p = GetEIDPoint(sub, eid);

                    if (p > 0)
                        return p;
                }
            }

            for (int i = 0; i < s.lastPoss.Count; i++)
            {
                if (s.draws[i].eventID == eid)
                    return s.lastPoss[i];
            }

            return -1.0f;
        }

        private void FindDraw(Point p, Section s,
                              ref FetchDrawcall left, ref float dleft, ref bool uleft,
                              ref FetchDrawcall right, ref float dright, ref bool uright)
        {
            if (s == null)
                return;

            var rect = s.lastRect;
            rect.Y = 0;
            rect.Height = 10000;

            if (s.subsections != null)
            {
                foreach (var sub in s.subsections)
                {
                    FindDraw(p, sub, ref left, ref dleft, ref uleft, ref right, ref dright, ref uright);

                    if(left != null && right != null)
                        return;
                }
            }

            if (rect.Contains(p))
            {
                if (s.draws == null || s.draws.Count == 0)
                    return;

                for(int i=0; i < s.lastPoss.Count; i++)
                {
                    if (s.lastVisible[i])
                    {
                        if (s.lastPoss[i] <= p.X)
                        {
                            if (
                                // not found left
                                left == null ||
                                // this left is closer and as usage-y, or we don't have a usage-y one yet
                               (s.lastPoss[i] > dleft && s.lastUsage[i] == uleft) ||
                                // this left is WAY closer
                               (s.lastPoss[i] > dleft + 20.0f) ||
                                // this left is more usage-y
                               (s.lastUsage[i] && !uleft)
                              )
                            {
                                dleft = s.lastPoss[i];
                                uleft = s.lastUsage[i];
                                left = s.draws[i];
                            }
                        }

                        if (s.lastPoss[i] > p.X)
                        {
                            if (
                                // not found right
                                right == null ||
                                // this right is closer and as usage-y, or we don't have a usage-y one yet
                               (s.lastPoss[i] < dright && s.lastUsage[i] == uright) ||
                                // this right is WAY closer
                               (s.lastPoss[i] < dright - 20.0f) ||
                                // this right is more usage-y
                               (s.lastUsage[i] && !uright)
                              )
                            {
                                dright = s.lastPoss[i];
                                uright = s.lastUsage[i];
                                right = s.draws[i];
                            }
                        }
                    }
                }

                if (left != null && right != null)
                    return;
            }
        }

        private FetchDrawcall FindDraw(Point p)
        {
            FetchDrawcall left = null;
            FetchDrawcall right = null;
            float dleft = -1.0f;
            float dright = -1.0f;
            bool uleft = false;
            bool uright = false;

            FindDraw(p, m_Root, ref left, ref dleft, ref uleft, ref right, ref dright, ref uright);

            if(left == null)
                return right;
            if(right == null)
                return left;

            if(Math.Abs(p.X - dleft) < Math.Abs(p.X - dright))
                return left;
            else
                return right;
        }

        private bool ProcessClick(Point p, Section s)
        {
            if(s == null)
                return false;

            if (s.lastRect.Contains(p))
            {
                s.Expanded = !s.Expanded;
                return true;
            }
            else
            {
                if (s.subsections != null)
                {
                    foreach (var sub in s.subsections)
                    {
                        var ret = ProcessClick(p, sub);
                        if (ret)
                            return ret;
                    }
                }
            }

            return false;
        }

        private void panel_Click(object sender, EventArgs e)
        {
            var p = panel.PointToClient(Cursor.Position);

            var expanded = ProcessClick(p, m_Root);

            if (!expanded)
            {
                var draw = FindDraw(p);

                if (draw != null)
                    m_Core.SetEventID(null, draw.eventID);
            }
        }

        private void TimelineBar_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
            barFont.Dispose();
        }
    }
}
