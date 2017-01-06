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
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;

namespace renderdocui.Controls
{
    [Designer(typeof(System.Windows.Forms.Design.ControlDesigner))]
    public partial class RangeHistogram : UserControl
    {
        public RangeHistogram()
        {
            InitializeComponent();
        }

        #region Events

        private static readonly object RangeUpdatedEvent = new object();
        public event EventHandler<RangeHistogramEventArgs> RangeUpdated
        {
            add { Events.AddHandler(RangeUpdatedEvent, value); }
            remove { Events.RemoveHandler(RangeUpdatedEvent, value); }
        }
        protected virtual void OnRangeUpdated(RangeHistogramEventArgs e)
        {
            EventHandler<RangeHistogramEventArgs> handler = (EventHandler<RangeHistogramEventArgs>)Events[RangeUpdatedEvent];
            if (handler != null)
                handler(this, e);
        }

        #endregion

        #region Privates

        private float m_RangeMax = 1.0f;
        private float m_RangeMin = 0.0f;

        private float m_WhitePoint = 1.0f;
        private float m_BlackPoint = 0.0f;

        private float m_MinRangeSize = 0.001f;

        private int m_Margin = 4;
        private int m_Border = 1;
        private int m_MarkerSize = 6;

        #endregion

        #region Code Properties

        private uint[] m_HistogramData = null;
        private float m_HistogramMin = 0.0f;
        private float m_HistogramMax = 1.0f;

        // sets the range of data where the histogram data was calculated.
        public void SetHistogramRange(float min, float max)
        {
            m_HistogramMin = min;
            m_HistogramMax = max;
        }

        // sets the minimum and maximum as well as the black and white points
        public void SetRange(float min, float max)
        {
            m_RangeMin = min;
            if (min < 0.0f)
                m_RangeMax = Math.Max((min - float.Epsilon) * (1.0f - m_MinRangeSize), max);
            else
                m_RangeMax = Math.Max((min + float.Epsilon) * (1.0f + m_MinRangeSize), max);

            m_BlackPoint = m_RangeMin;
            m_WhitePoint = m_RangeMax;

            Invalidate();
            OnRangeUpdated(new RangeHistogramEventArgs(BlackPoint, WhitePoint));
        }

        public bool ValidRange
        {
            get
            {
                if (float.IsInfinity(m_WhitePoint) || float.IsNaN(m_WhitePoint) ||
                    float.IsInfinity(m_BlackPoint) || float.IsNaN(m_BlackPoint) ||
                    float.IsInfinity(m_RangeMax) || float.IsNaN(m_RangeMax) ||
                    float.IsInfinity(m_RangeMin) || float.IsNaN(m_RangeMin) ||
                    float.IsInfinity(m_RangeMax - m_RangeMin) || float.IsNaN(m_RangeMax - m_RangeMin) ||
                    float.IsInfinity(m_WhitePoint - m_BlackPoint) || float.IsNaN(m_WhitePoint - m_BlackPoint))
                {
                    return false;
                }

                return true;
            }
        }

        [Browsable(false)]
        public uint[] HistogramData
        {
            get
            {
                return m_HistogramData;
            }
            set
            {
                m_HistogramData = value;
                Invalidate();
            }
        }

        // black point and white point are the currently selected black/white points, within the
        // minimum and maximum points. (ie. not 0 or 1)
        [Browsable(false)]
        public float BlackPoint
        {
            get
            {
                return m_BlackPoint;
            }
            set
            {
                if (value <= m_RangeMin)
                    m_BlackPoint = m_RangeMin = value;
                else
                    m_BlackPoint = value;

                Invalidate();
                OnRangeUpdated(new RangeHistogramEventArgs(BlackPoint, WhitePoint));
            }
        }
        [Browsable(false)]
        public float WhitePoint
        {
            get
            {
                return m_WhitePoint;
            }
            set
            {
                if (value >= m_RangeMax)
                    m_WhitePoint = m_RangeMax = value;
                else
                    m_WhitePoint = value;

                Invalidate();
                OnRangeUpdated(new RangeHistogramEventArgs(BlackPoint, WhitePoint));
            }
        }

        // range min/max are the current minimum and maximum values that can be set
        // for the black and white points
        [Browsable(false)]
        public float RangeMin
        {
            get
            {
                return m_RangeMin;
            }
        }
        [Browsable(false)]
        public float RangeMax
        {
            get
            {
                return m_RangeMax;
            }
        }

        #endregion

        #region Designer Properties

        [Description("The smallest possible range that can be selected"), Category("Behavior")]
        [DefaultValue(typeof(float), "0.001")]
        public float MinRangeSize
        {
            get { return m_MinRangeSize; }
            set { m_MinRangeSize = Math.Max(0.000001f, value); }
        }

        [Description("The margin around the range display"), Category("Layout")]
        [DefaultValue(typeof(int), "4")]
        public int RangeMargin
        {
            get { return m_Margin; }
            set { m_Margin = value; }
        }

        [Description("The pixel border around the range bar itself"), Category("Appearance")]
        [DefaultValue(typeof(int), "1")]
        public int Border
        {
            get { return m_Border; }
            set { m_Border = value; }
        }

        [Description("The size in pixels of each marker"), Category("Appearance")]
        [DefaultValue(typeof(int), "6")]
        public int MarkerSize
        {
            get { return m_MarkerSize; }
            set { m_MarkerSize = value; }
        }

        #endregion

        #region Internal Properties

        private int m_TotalSpace { get { return m_Margin + m_Border; } }
        private int m_RegionWidth { get { return this.Width - m_TotalSpace * 2; } }

        // these are internal only, they give [0, 1] from minimum to maximum of where the black and white points are
        private float m_BlackDelta
        {
            get
            {
                if (!ValidRange) return 0.0f;
                return GetDelta(BlackPoint);
            }
            set
            {
                BlackPoint = Math.Min(WhitePoint - MinRangeSize, value * (RangeMax - RangeMin) + RangeMin);
            }
        }
        private float m_WhiteDelta
        {
            get
            {
                if (!ValidRange) return 1.0f;
                return GetDelta(WhitePoint);
            }
            set
            {
                WhitePoint = Math.Max(BlackPoint + MinRangeSize, value * (RangeMax - RangeMin) + RangeMin);
            }
        }

        private float GetDelta(float val)
        {
            return (val - RangeMin) / (RangeMax - RangeMin);
        }

        #endregion

        #region Tooltips

        private void ShowTooltips()
        {
            blackToolTip.Show(BlackPoint.ToString("F4"), this,
                this.ClientRectangle.Left + (int)(this.ClientRectangle.Width * m_BlackDelta), this.ClientRectangle.Bottom);

            whiteToolTip.Show(WhitePoint.ToString("F4"), this,
                this.ClientRectangle.Left + (int)(this.ClientRectangle.Width * m_WhiteDelta), this.ClientRectangle.Top - 15);
        }

        #endregion

        #region Mouse Handlers

        private Point m_mousePrev = new Point(-1, -1);

        private enum DraggingMode
        {
            NONE,
            WHITE,
            BLACK,
        }
        private DraggingMode m_DragMode;

        // This handler tries to figure out which handle (white or black) you were trying to
        // grab when you clicked.
        private void RangeHistogram_MouseDown(object sender, MouseEventArgs e)
        {
            if(e.Button != MouseButtons.Left || !ValidRange)
                return;

            Rectangle rect = this.ClientRectangle;

            rect.Inflate(-m_TotalSpace, -m_TotalSpace);

            int whiteX = (int)(m_WhiteDelta * rect.Width);
            int blackX = (int)(m_BlackDelta * rect.Width);

            var whiteVec = new PointF(whiteX - e.Location.X, ClientRectangle.Height - e.Location.Y);
            var blackVec = new PointF(blackX-e.Location.X, e.Location.Y);

            float whitedist = (float)Math.Sqrt(whiteVec.X * whiteVec.X + whiteVec.Y * whiteVec.Y);
            float blackdist = (float)Math.Sqrt(blackVec.X * blackVec.X + blackVec.Y * blackVec.Y);

            System.Diagnostics.Trace.WriteLine(string.Format("white {0} black {1}", whitedist, blackdist));

            if (whitedist < blackdist && whitedist < 18.0f)
                m_DragMode = DraggingMode.WHITE;
            else if (blackdist < whitedist && blackdist < 18.0f)
                m_DragMode = DraggingMode.BLACK;
            else if (e.Location.X > whiteX)
                m_DragMode = DraggingMode.WHITE;
            else if (e.Location.X < blackX)
                m_DragMode = DraggingMode.BLACK;

            if (m_DragMode == DraggingMode.WHITE)
            {
                float newWhite = (float)(e.Location.X - m_TotalSpace) / (float)m_RegionWidth;

                m_WhiteDelta = Math.Max(m_BlackDelta + m_MinRangeSize, Math.Min(1.0f, newWhite));
            }
            else if (m_DragMode == DraggingMode.BLACK)
            {
                float newBlack = (float)(e.Location.X - m_TotalSpace) / (float)m_RegionWidth;

                m_BlackDelta = Math.Min(m_WhiteDelta - m_MinRangeSize, Math.Max(0.0f, newBlack));
            }

            OnRangeUpdated(new RangeHistogramEventArgs(BlackPoint, WhitePoint));

            if (m_DragMode != DraggingMode.NONE)
            {
                this.Invalidate();
                this.Update();
            }

            m_mousePrev.X = e.X;
            m_mousePrev.Y = e.Y;
        }

        private void RangeHistogram_MouseUp(object sender, MouseEventArgs e)
        {
            whiteToolTip.Hide(this);
            blackToolTip.Hide(this);

            m_DragMode = DraggingMode.NONE;

            m_mousePrev.X = m_mousePrev.Y = -1;
        }

        private void RangeHistogram_MouseMove(object sender, MouseEventArgs e)
        {
            if (ValidRange && e.Button == MouseButtons.Left && (e.X != m_mousePrev.X || e.Y != m_mousePrev.Y))
            {
                if (m_DragMode == DraggingMode.WHITE)
                {
                    float newWhite = (float)(e.Location.X - m_TotalSpace) / (float)m_RegionWidth;

                    m_WhiteDelta = Math.Max(m_BlackDelta + m_MinRangeSize, Math.Min(1.0f, newWhite));
                }
                else if (m_DragMode == DraggingMode.BLACK)
                {
                    float newBlack = (float)(e.Location.X - m_TotalSpace) / (float)m_RegionWidth;

                    m_BlackDelta = Math.Min(m_WhiteDelta - m_MinRangeSize, Math.Max(0.0f, newBlack));
                }

                OnRangeUpdated(new RangeHistogramEventArgs(BlackPoint, WhitePoint));

                if (m_DragMode != DraggingMode.NONE)
                {
                    this.Invalidate();
                    this.Update();
                }

                m_mousePrev.X = e.X;
                m_mousePrev.Y = e.Y;

                ShowTooltips();
            }
        }

        private void RangeHistogram_MouseLeave(object sender, EventArgs e)
        {
            whiteToolTip.Hide(this);
            blackToolTip.Hide(this);
        }

        private void RangeHistogram_MouseEnter(object sender, EventArgs e)
        {
            ShowTooltips();
        }

        #endregion

        #region Other Handlers

        private void RangeHistogram_Paint(object sender, PaintEventArgs e)
        {
            Rectangle rect = this.ClientRectangle;

            e.Graphics.FillRectangle(SystemBrushes.Control, rect);

            rect.Inflate(-m_Margin, -m_Margin);

            e.Graphics.FillRectangle(SystemBrushes.ControlText, rect);

            rect.Inflate(-m_Border, -m_Border);

            e.Graphics.FillRectangle(ValidRange ? Brushes.DarkGray : Brushes.DarkRed, rect);

            int whiteX = (int)(m_WhiteDelta * rect.Width);
            int blackX = (int)(m_BlackDelta * rect.Width);

            Rectangle blackPoint = new Rectangle(rect.Left, rect.Top, blackX, rect.Height);
            Rectangle whitePoint = new Rectangle(rect.Left + whiteX, rect.Top, rect.Width - whiteX, rect.Height);

            e.Graphics.FillRectangle(Brushes.White, whitePoint);
            e.Graphics.FillRectangle(Brushes.Black, blackPoint);

            if (!ValidRange)
                return;

            if (HistogramData != null)
            {
                float minx = GetDelta(m_HistogramMin);
                float maxx = GetDelta(m_HistogramMax);

                UInt32 maxval = UInt32.MinValue;
                for (int i = 0; i < HistogramData.Length; i++)
                {
                    float x = (float)i / (float)HistogramData.Length;

                    float xdelta = minx + x * (maxx - minx);

                    if (xdelta >= 0.0f && xdelta <= 1.0f)
                    {
                        maxval = Math.Max(maxval, HistogramData[i]);
                    }
                }

                if (maxval == 0)
                    maxval = 1;

                for (int i = 0; i < HistogramData.Length; i++)
                {
                    float x = (float)i / (float)HistogramData.Length;
                    float y = (float)HistogramData[i] / (float)maxval;

                    float xdelta = minx + x * (maxx - minx);

                    if (xdelta >= 0.0f && xdelta <= 1.0f)
                    {
                        float segwidth = Math.Max(rect.Width * (maxx - minx) / (float)HistogramData.Length, 1);

                        RectangleF barRect = new RectangleF(new PointF(rect.Left + rect.Width * (minx + x * (maxx - minx)), rect.Bottom - rect.Height * y),
                                                            new SizeF(segwidth, rect.Height * y));

                        e.Graphics.FillRectangle(Brushes.Green, barRect);
                    }
                }
            }

            Point[] blackTriangle = { new Point(blackPoint.Right, m_MarkerSize*2),
                                      new Point(blackPoint.Right+m_MarkerSize, 0),
                                      new Point(blackPoint.Right-m_MarkerSize, 0) };

            e.Graphics.FillPolygon(Brushes.DarkGray, blackTriangle);

            Point[] whiteTriangle = { new Point(whitePoint.Left, whitePoint.Bottom-m_MarkerSize*2+m_Margin),
                                      new Point(whitePoint.Left+m_MarkerSize, whitePoint.Bottom+m_Margin),
                                      new Point(whitePoint.Left-m_MarkerSize, whitePoint.Bottom+m_Margin) };

            e.Graphics.FillPolygon(Brushes.DarkGray, whiteTriangle);

            blackTriangle[0].Y -= 2;
            blackTriangle[1].Y += 1;
            blackTriangle[2].Y += 1;

            blackTriangle[1].X -= 2;
            blackTriangle[2].X += 2;

            whiteTriangle[0].Y += 2;
            whiteTriangle[1].Y -= 1;
            whiteTriangle[2].Y -= 1;

            whiteTriangle[1].X -= 2;
            whiteTriangle[2].X += 2;

            e.Graphics.FillPolygon(Brushes.Black, blackTriangle);
            e.Graphics.FillPolygon(Brushes.White, whiteTriangle);
        }

        #endregion
    }

    public class RangeHistogramEventArgs : EventArgs
    {
        private float m_black, m_white;

        public RangeHistogramEventArgs(float black, float white)
        {
            m_black = black;
            m_white = white;
        }

        public float BlackPoint
        {
            get { return m_black; }
        }

        public float WhitePoint
        {
            get { return m_white; }
        }
    }

}
