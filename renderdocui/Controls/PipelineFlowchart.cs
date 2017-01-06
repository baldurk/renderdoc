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
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace renderdocui.Controls
{
    public partial class PipelineFlowchart : UserControl
    {
        public PipelineFlowchart()
        {
            InitializeComponent();

            this.DoubleBuffered = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        }

        public void SetStages(KeyValuePair<string, string>[] stages)
        {
            m_StageNames = stages;
            m_StagesEnabled = new bool[stages.Length];
            m_StageFlows = new bool[stages.Length];
            for(int i=0; i < stages.Length; i++)
                m_StageFlows[i] = true;

            Invalidate();
        }

        #region Events

        private static readonly object SelectedStageChangedEvent = new object();

        [Description("Event raised when the selected pipeline stage is changed."), Category("Behavior")]
        public event EventHandler<EventArgs> SelectedStageChanged
        {
            add { Events.AddHandler(SelectedStageChangedEvent, value); }
            remove { Events.RemoveHandler(SelectedStageChangedEvent, value); }
        }
        protected virtual void OnSelectedStageChanged(EventArgs e)
        {
            EventHandler<EventArgs> handler = (EventHandler<EventArgs>)Events[SelectedStageChangedEvent];
            if (handler != null)
                handler(this, e);
        }

        #endregion

        #region Data Properties

        private bool[] m_StagesEnabled = null;
        private bool[] m_StageFlows = null;
        private KeyValuePair<string, string>[] m_StageNames = null;

        public void SetStagesEnabled(bool[] enabled)
        {
            if (m_StagesEnabled != null && enabled.Length == m_StagesEnabled.Length)
                m_StagesEnabled = enabled;

            Invalidate();
        }

        public void SetStageName(int index, KeyValuePair<string, string> name)
        {
            if (index >= 0 && index < m_StageNames.Length)
                m_StageNames[index] = name;

            Invalidate();
        }

        public void IsolateStage(int index)
        {
            if (m_StageNames != null && index >= 0 && index < m_StageNames.Length)
                m_StageFlows[index] = false;
        }

        private bool IsStageEnabled(int index)
        {
            return m_StagesEnabled != null && m_StagesEnabled[index];
        }

        private int m_HoverStage = -1;
        private int m_SelectedStage = 0;

        [Browsable(false)]
        public int SelectedStage
        {
            get
            {
                return m_SelectedStage;
            }
            set
            {
                if (value >= 0 && m_StageNames != null && value < m_StageNames.Length)
                {
                    m_SelectedStage = value;
                    Invalidate();

                    OnSelectedStageChanged(new EventArgs());
                }
            }
        }

        #endregion

        #region Constants and positions/dimensions

        const int BoxBorderWidth = 3;
        const int MinBoxDimension = 25;
        const int MaxBoxCornerRadius = 20;
        const float BoxCornerRadiusFraction = 1.0f / 6.0f;
        const float ArrowHeadSize = 6.0f;
        const int MinBoxMargin = 4;
        const int BoxLabelMargin = 8;
        const float BoxMarginFraction = 0.02f;

        private Rectangle TotalAreaRect
        {
            get
            {
                Rectangle rect = ClientRectangle;

                rect.Inflate(-1, -1);

                rect.X += Padding.Left;
                rect.Width -= Padding.Left + Padding.Right;

                rect.Y += Padding.Top;
                rect.Height -= Padding.Top + Padding.Bottom;

                rect.Inflate(-BoxBorderWidth, -BoxBorderWidth);

                return rect;
            }
        }

        private float BoxMargin
        {
            get
            {
                float margin = Math.Max(TotalAreaRect.Width, TotalAreaRect.Height) * BoxMarginFraction;

                margin = Math.Max(MinBoxMargin, margin);

                return margin;
            }
        }

        private int NumGaps { get { return m_StageNames == null ? 1 : m_StageNames.Length - 1; } }
        private int NumItems { get { return m_StageNames == null ? 2 : m_StageNames.Length; } }

        private SizeF BoxSize
        {
            get
            {
                float boxeswidth = TotalAreaRect.Width - NumGaps * BoxMargin;

                float boxdim = Math.Min(TotalAreaRect.Height, boxeswidth / NumItems);

                boxdim = Math.Max(MinBoxDimension, boxdim);

                float oblongwidth = Math.Max(0, (boxeswidth - boxdim * NumItems) / NumItems);

                return new SizeF(boxdim + oblongwidth, boxdim);
            }
        }

        private RectangleF GetBoxRect(int i)
        {
            return new RectangleF(TotalAreaRect.X + i * (BoxSize.Width + BoxMargin),
                                  TotalAreaRect.Y + TotalAreaRect.Height / 2 - BoxSize.Height / 2,
                                  BoxSize.Width, BoxSize.Height);
        }

        #endregion

        #region Painting

        GraphicsPath RoundedRect(RectangleF rect, int radius)
        {
            GraphicsPath ret = new GraphicsPath();
            
            ret.StartFigure();

            ret.AddArc(rect.X, rect.Y, 2 * radius, 2 * radius, 180, 90);
            ret.AddLine(rect.X + radius, rect.Y, rect.X + rect.Width - radius, rect.Y);
            ret.AddArc(rect.X + rect.Width - radius * 2, rect.Y, 2 * radius, 2 * radius, 270, 90);
            ret.AddLine(rect.X + rect.Width, rect.Y + radius, rect.X + rect.Width, rect.Y + rect.Height - radius);
            ret.AddArc(rect.X + rect.Width - 2*radius, rect.Y + rect.Height - 2 * radius, 2 * radius, 2 * radius, 0, 90);
            ret.AddLine(rect.X + rect.Width - radius, rect.Y + rect.Height, rect.X + radius, rect.Y + rect.Height);
            ret.AddArc(rect.X, rect.Y + rect.Height - 2*radius, 2 * radius, 2 * radius, 90, 90);

            ret.CloseFigure();

            return ret;
        }

        void DrawArrow(Graphics dc, Brush b, Pen p, float headsize, float y, float left, float right)
        {
            dc.DrawLine(p, new PointF(left, y), new PointF(right, y));

            using (GraphicsPath head = new GraphicsPath())
            {
                head.StartFigure();

                head.AddLine(new PointF(right, y), new PointF(right - headsize, y - headsize));
                head.AddLine(new PointF(right - headsize, y - headsize), new PointF(right - headsize, y + headsize));

                head.CloseFigure();

                dc.FillPath(b, head);
            }
        }

        private void PipelineFlowchart_Paint(object sender, PaintEventArgs e)
        {
            if (m_StageNames == null)
                return;

            var dc = e.Graphics;

            dc.CompositingQuality = CompositingQuality.HighQuality;
            dc.SmoothingMode = SmoothingMode.AntiAlias;

            int radius = (int)Math.Min(MaxBoxCornerRadius, BoxSize.Height * BoxCornerRadiusFraction);

            float arrowY = TotalAreaRect.Y + TotalAreaRect.Height / 2;

            using (var pen = new Pen(SystemBrushes.WindowFrame, BoxBorderWidth))
            using (var selectedpen = new Pen(Brushes.Red, BoxBorderWidth))
            {
                for (int i = 0; i < NumGaps; i++)
                {
                    if (!m_StageFlows[i] || !m_StageFlows[i + 1])
                        continue;

                    float right = TotalAreaRect.X + (i + 1) * (BoxSize.Width + BoxMargin);
                    float left = right - BoxMargin;

                    DrawArrow(dc, SystemBrushes.WindowFrame, pen, ArrowHeadSize, arrowY, left, right);
                }

                for (int i = 0; i < NumItems; i++)
                {
                    RectangleF boxrect = GetBoxRect(i);

                    var backBrush = SystemBrushes.Window;
                    var textBrush = SystemBrushes.WindowText;
                    var outlinePen = pen;

                    if (SystemInformation.HighContrast)
                    {
                        backBrush = SystemBrushes.ActiveCaption;
                        textBrush = SystemBrushes.ActiveCaptionText;
                    }

                    if (!IsStageEnabled(i))
                    {
                        backBrush = SystemBrushes.InactiveCaption;
                        textBrush = SystemBrushes.InactiveCaptionText;
                    }

                    if (i == m_HoverStage)
                    {
                        backBrush = SystemBrushes.Info;
                        textBrush = SystemBrushes.InfoText;
                    }

                    if (i == SelectedStage)
                    {
                        //backBrush = Brushes.Coral;
                        outlinePen = selectedpen;
                    }

                    using (var boxpath = RoundedRect(boxrect, radius))
                    {
                        dc.FillPath(backBrush, boxpath);
                        dc.DrawPath(outlinePen, boxpath);
                    }

                    StringFormat format = new StringFormat();
                    format.LineAlignment = StringAlignment.Center;
                    format.Alignment = StringAlignment.Center;

                    var s = m_StageNames[i].Value;

                    var size = dc.MeasureString(s, Font);

                    // Decide whether we can draw the whole stage name or just the abbreviation.
                    // This can look a little awkward sometimes if it's sometimes abbreviated sometimes
                    // not. Maybe it should always abbreviate or not at all?
                    if (size.Width + BoxLabelMargin > (float)boxrect.Width)
                    {
                        s = s.Replace(" ", "\n");
                        size = dc.MeasureString(s, Font);

                        if (size.Width + BoxLabelMargin > (float)boxrect.Width ||
                            size.Height + BoxLabelMargin > (float)boxrect.Height)
                        {
                            s = m_StageNames[i].Key;
                            size = dc.MeasureString(s, Font);
                        }
                    }

                    dc.DrawString(s, Font, textBrush, new PointF(boxrect.X + boxrect.Width / 2, arrowY), format);
                }
            }
        }

        #endregion

        #region Mouse Handling

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);

            int old = m_HoverStage;
            m_HoverStage = -1;

            for(int i=0; i < NumItems; i++)
            {
                if (GetBoxRect(i).Contains(e.Location))
                {
                    m_HoverStage = i;
                    break;
                }
            }

            if(m_HoverStage != old)
                Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);

            m_HoverStage = -1;
            Invalidate();
        }

        protected override void OnMouseClick(MouseEventArgs e)
        {
            base.OnMouseClick(e);

            for (int i = 0; i < NumItems; i++)
            {
                if (GetBoxRect(i).Contains(e.Location))
                {
                    SelectedStage = i;
                    break;
                }
            }
        }

        #endregion
    }
}
