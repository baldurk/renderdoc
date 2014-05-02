using System;
using System.Drawing;
using System.Windows.Forms;
using System.Drawing.Drawing2D;
using System.ComponentModel;

namespace WeifenLuo.WinFormsUI.Docking
{
    internal class VS2005AutoHideStrip : AutoHideStripBase
    {
        private class TabVS2005 : Tab
        {
            internal TabVS2005(IDockContent content)
                : base(content)
            {
            }

            private int m_tabX = 0;
            public int TabX
            {
                get { return m_tabX; }
                set { m_tabX = value; }
            }

            private int m_tabWidth = 0;
            public int TabWidth
            {
                get { return m_tabWidth; }
                set { m_tabWidth = value; }
            }

        }

        private const int _ImageHeight = 16;
        private const int _ImageWidth = 16;
        private const int _ImageGapTop = 2;
        private const int _ImageGapLeft = 4;
        private const int _ImageGapRight = 2;
        private const int _ImageGapBottom = 2;
        private const int _TextGapLeft = 0;
        private const int _TextGapRight = 0;
        private const int _TabGapTop = 3;
        private const int _TabGapLeft = 4;
        private const int _TabGapBetween = 10;

        #region Customizable Properties
        public Font TextFont
        {
            get { return DockPanel.Skin.AutoHideStripSkin.TextFont; }
        }

        private static StringFormat _stringFormatTabHorizontal;
        private StringFormat StringFormatTabHorizontal
        {
            get
            {
                if (_stringFormatTabHorizontal == null)
                {
                    _stringFormatTabHorizontal = new StringFormat();
                    _stringFormatTabHorizontal.Alignment = StringAlignment.Near;
                    _stringFormatTabHorizontal.LineAlignment = StringAlignment.Center;
                    _stringFormatTabHorizontal.FormatFlags = StringFormatFlags.NoWrap;
                    _stringFormatTabHorizontal.Trimming = StringTrimming.None;
                }

                if (RightToLeft == RightToLeft.Yes)
                    _stringFormatTabHorizontal.FormatFlags |= StringFormatFlags.DirectionRightToLeft;
                else
                    _stringFormatTabHorizontal.FormatFlags &= ~StringFormatFlags.DirectionRightToLeft;

                return _stringFormatTabHorizontal;
            }
        }

        private static StringFormat _stringFormatTabVertical;
        private StringFormat StringFormatTabVertical
        {
            get
            {
                if (_stringFormatTabVertical == null)
                {
                    _stringFormatTabVertical = new StringFormat();
                    _stringFormatTabVertical.Alignment = StringAlignment.Near;
                    _stringFormatTabVertical.LineAlignment = StringAlignment.Center;
                    _stringFormatTabVertical.FormatFlags = StringFormatFlags.NoWrap | StringFormatFlags.DirectionVertical;
                    _stringFormatTabVertical.Trimming = StringTrimming.None;
                }
                if (RightToLeft == RightToLeft.Yes)
                    _stringFormatTabVertical.FormatFlags |= StringFormatFlags.DirectionRightToLeft;
                else
                    _stringFormatTabVertical.FormatFlags &= ~StringFormatFlags.DirectionRightToLeft;

                return _stringFormatTabVertical;
            }
        }

        private static int ImageHeight
        {
            get { return _ImageHeight; }
        }

        private static int ImageWidth
        {
            get { return _ImageWidth; }
        }

        private static int ImageGapTop
        {
            get { return _ImageGapTop; }
        }

        private static int ImageGapLeft
        {
            get { return _ImageGapLeft; }
        }

        private static int ImageGapRight
        {
            get { return _ImageGapRight; }
        }

        private static int ImageGapBottom
        {
            get { return _ImageGapBottom; }
        }

        private static int TextGapLeft
        {
            get { return _TextGapLeft; }
        }

        private static int TextGapRight
        {
            get { return _TextGapRight; }
        }

        private static int TabGapTop
        {
            get { return _TabGapTop; }
        }

        private static int TabGapLeft
        {
            get { return _TabGapLeft; }
        }

        private static int TabGapBetween
        {
            get { return _TabGapBetween; }
        }

        private static Pen PenTabBorder
        {
            get { return SystemPens.GrayText; }
        }
        #endregion

        private static Matrix _matrixIdentity = new Matrix();
        private static Matrix MatrixIdentity
        {
            get { return _matrixIdentity; }
        }

        private static DockState[] _dockStates;
        private static DockState[] DockStates
        {
            get
            {
                if (_dockStates == null)
                {
                    _dockStates = new DockState[4];
                    _dockStates[0] = DockState.DockLeftAutoHide;
                    _dockStates[1] = DockState.DockRightAutoHide;
                    _dockStates[2] = DockState.DockTopAutoHide;
                    _dockStates[3] = DockState.DockBottomAutoHide;
                }
                return _dockStates;
            }
        }

        private static GraphicsPath _graphicsPath;
        internal static GraphicsPath GraphicsPath
        {
            get
            {
                if (_graphicsPath == null)
                    _graphicsPath = new GraphicsPath();

                return _graphicsPath;
            }
        }

        public VS2005AutoHideStrip(DockPanel panel)
            : base(panel)
        {
            SetStyle(ControlStyles.ResizeRedraw |
                ControlStyles.UserPaint |
                ControlStyles.AllPaintingInWmPaint |
                ControlStyles.OptimizedDoubleBuffer, true);
            BackColor = SystemColors.ControlLight;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;

            Color startColor = DockPanel.Skin.AutoHideStripSkin.DockStripGradient.StartColor;
            Color endColor = DockPanel.Skin.AutoHideStripSkin.DockStripGradient.EndColor;
            LinearGradientMode gradientMode = DockPanel.Skin.AutoHideStripSkin.DockStripGradient.LinearGradientMode;
            using (LinearGradientBrush brush = new LinearGradientBrush(ClientRectangle, startColor, endColor, gradientMode))
            {
                g.FillRectangle(brush, ClientRectangle);
            }

            DrawTabStrip(g);
        }

        protected override void OnLayout(LayoutEventArgs levent)
        {
            CalculateTabs();
            base.OnLayout(levent);
        }

        private void DrawTabStrip(Graphics g)
        {
            DrawTabStrip(g, DockState.DockTopAutoHide);
            DrawTabStrip(g, DockState.DockBottomAutoHide);
            DrawTabStrip(g, DockState.DockLeftAutoHide);
            DrawTabStrip(g, DockState.DockRightAutoHide);
        }

        private void DrawTabStrip(Graphics g, DockState dockState)
        {
            Rectangle rectTabStrip = GetLogicalTabStripRectangle(dockState);

            if (rectTabStrip.IsEmpty)
                return;

            Matrix matrixIdentity = g.Transform;
            if (dockState == DockState.DockLeftAutoHide || dockState == DockState.DockRightAutoHide)
            {
                Matrix matrixRotated = new Matrix();
                matrixRotated.RotateAt(90, new PointF((float)rectTabStrip.X + (float)rectTabStrip.Height / 2,
                    (float)rectTabStrip.Y + (float)rectTabStrip.Height / 2));
                g.Transform = matrixRotated;
            }

            foreach (Pane pane in GetPanes(dockState))
            {
                foreach (TabVS2005 tab in pane.AutoHideTabs)
                    DrawTab(g, tab);
            }
            g.Transform = matrixIdentity;
        }

        private void CalculateTabs()
        {
            CalculateTabs(DockState.DockTopAutoHide);
            CalculateTabs(DockState.DockBottomAutoHide);
            CalculateTabs(DockState.DockLeftAutoHide);
            CalculateTabs(DockState.DockRightAutoHide);
        }

        private void CalculateTabs(DockState dockState)
        {
            Rectangle rectTabStrip = GetLogicalTabStripRectangle(dockState);

            int imageHeight = rectTabStrip.Height - ImageGapTop - ImageGapBottom;
            int imageWidth = ImageWidth;
            if (imageHeight > ImageHeight)
                imageWidth = ImageWidth * (imageHeight / ImageHeight);

            int x = TabGapLeft + rectTabStrip.X;
            foreach (Pane pane in GetPanes(dockState))
            {
                foreach (TabVS2005 tab in pane.AutoHideTabs)
                {
                    int width = imageWidth + ImageGapLeft + ImageGapRight +
                        TextRenderer.MeasureText(tab.Content.DockHandler.TabText, TextFont).Width +
                        TextGapLeft + TextGapRight;
                    tab.TabX = x;
                    tab.TabWidth = width;
                    x += width;
                }

                x += TabGapBetween;
            }
        }

        private Rectangle RtlTransform(Rectangle rect, DockState dockState)
        {
            Rectangle rectTransformed;
            if (dockState == DockState.DockLeftAutoHide || dockState == DockState.DockRightAutoHide)
                rectTransformed = rect;
            else
                rectTransformed = DrawHelper.RtlTransform(this, rect);

            return rectTransformed;
        }

        private GraphicsPath GetTabOutline(TabVS2005 tab, bool transformed, bool rtlTransform)
        {
            DockState dockState = tab.Content.DockHandler.DockState;
            Rectangle rectTab = GetTabRectangle(tab, transformed);
            if (rtlTransform)
                rectTab = RtlTransform(rectTab, dockState);
            bool upTab = (dockState == DockState.DockLeftAutoHide || dockState == DockState.DockBottomAutoHide);
            DrawHelper.GetRoundedCornerTab(GraphicsPath, rectTab, upTab);

            return GraphicsPath;
        }

        private void DrawTab(Graphics g, TabVS2005 tab)
        {
            Rectangle rectTabOrigin = GetTabRectangle(tab);
            if (rectTabOrigin.IsEmpty)
                return;

            DockState dockState = tab.Content.DockHandler.DockState;
            IDockContent content = tab.Content;

            GraphicsPath path = GetTabOutline(tab, false, true);

            Color startColor = DockPanel.Skin.AutoHideStripSkin.TabGradient.StartColor;
            Color endColor = DockPanel.Skin.AutoHideStripSkin.TabGradient.EndColor;
            LinearGradientMode gradientMode = DockPanel.Skin.AutoHideStripSkin.TabGradient.LinearGradientMode;
            g.FillPath(new LinearGradientBrush(rectTabOrigin, startColor, endColor, gradientMode), path);
            g.DrawPath(PenTabBorder, path);

            // Set no rotate for drawing icon and text
            Matrix matrixRotate = g.Transform;
            g.Transform = MatrixIdentity;

            // Draw the icon
            Rectangle rectImage = rectTabOrigin;
            rectImage.X += ImageGapLeft;
            rectImage.Y += ImageGapTop;
            int imageHeight = rectTabOrigin.Height - ImageGapTop - ImageGapBottom;
            int imageWidth = ImageWidth;
            if (imageHeight > ImageHeight)
                imageWidth = ImageWidth * (imageHeight / ImageHeight);
            rectImage.Height = imageHeight;
            rectImage.Width = imageWidth;
            rectImage = GetTransformedRectangle(dockState, rectImage);

            if (dockState == DockState.DockLeftAutoHide || dockState == DockState.DockRightAutoHide)
            {
                // The DockState is DockLeftAutoHide or DockRightAutoHide, so rotate the image 90 degrees to the right. 
                Rectangle rectTransform = RtlTransform(rectImage, dockState);
                Point[] rotationPoints =
                { 
                    new Point(rectTransform.X + rectTransform.Width, rectTransform.Y), 
                    new Point(rectTransform.X + rectTransform.Width, rectTransform.Y + rectTransform.Height), 
                    new Point(rectTransform.X, rectTransform.Y)
                };

                using (Icon rotatedIcon = new Icon(((Form)content).Icon, 16, 16))
                {
                    g.DrawImage(rotatedIcon.ToBitmap(), rotationPoints);
                }
            }
            else
            {
                // Draw the icon normally without any rotation.
                g.DrawIcon(((Form)content).Icon, RtlTransform(rectImage, dockState));
            }

            // Draw the text
            Rectangle rectText = rectTabOrigin;
            rectText.X += ImageGapLeft + imageWidth + ImageGapRight + TextGapLeft;
            rectText.Width -= ImageGapLeft + imageWidth + ImageGapRight + TextGapLeft;
            rectText = RtlTransform(GetTransformedRectangle(dockState, rectText), dockState);

            Color textColor = DockPanel.Skin.AutoHideStripSkin.TabGradient.TextColor;

            if (dockState == DockState.DockLeftAutoHide || dockState == DockState.DockRightAutoHide)
                g.DrawString(content.DockHandler.TabText, TextFont, new SolidBrush(textColor), rectText, StringFormatTabVertical);
            else
                g.DrawString(content.DockHandler.TabText, TextFont, new SolidBrush(textColor), rectText, StringFormatTabHorizontal);

            // Set rotate back
            g.Transform = matrixRotate;
        }

        private Rectangle GetLogicalTabStripRectangle(DockState dockState)
        {
            return GetLogicalTabStripRectangle(dockState, false);
        }

        private Rectangle GetLogicalTabStripRectangle(DockState dockState, bool transformed)
        {
            if (!DockHelper.IsDockStateAutoHide(dockState))
                return Rectangle.Empty;

            int leftPanes = GetPanes(DockState.DockLeftAutoHide).Count;
            int rightPanes = GetPanes(DockState.DockRightAutoHide).Count;
            int topPanes = GetPanes(DockState.DockTopAutoHide).Count;
            int bottomPanes = GetPanes(DockState.DockBottomAutoHide).Count;

            int x, y, width, height;

            height = MeasureHeight();
            if (dockState == DockState.DockLeftAutoHide && leftPanes > 0)
            {
                x = 0;
                y = (topPanes == 0) ? 0 : height;
                width = Height - (topPanes == 0 ? 0 : height) - (bottomPanes == 0 ? 0 : height);
            }
            else if (dockState == DockState.DockRightAutoHide && rightPanes > 0)
            {
                x = Width - height;
                if (leftPanes != 0 && x < height)
                    x = height;
                y = (topPanes == 0) ? 0 : height;
                width = Height - (topPanes == 0 ? 0 : height) - (bottomPanes == 0 ? 0 : height);
            }
            else if (dockState == DockState.DockTopAutoHide && topPanes > 0)
            {
                x = leftPanes == 0 ? 0 : height;
                y = 0;
                width = Width - (leftPanes == 0 ? 0 : height) - (rightPanes == 0 ? 0 : height);
            }
            else if (dockState == DockState.DockBottomAutoHide && bottomPanes > 0)
            {
                x = leftPanes == 0 ? 0 : height;
                y = Height - height;
                if (topPanes != 0 && y < height)
                    y = height;
                width = Width - (leftPanes == 0 ? 0 : height) - (rightPanes == 0 ? 0 : height);
            }
            else
                return Rectangle.Empty;

            if (width == 0 || height == 0)
            {
                return Rectangle.Empty;
            }

            var rect = new Rectangle(x, y, width, height);
            return transformed ? GetTransformedRectangle(dockState, rect) : rect;
        }

        private Rectangle GetTabRectangle(TabVS2005 tab)
        {
            return GetTabRectangle(tab, false);
        }

        private Rectangle GetTabRectangle(TabVS2005 tab, bool transformed)
        {
            DockState dockState = tab.Content.DockHandler.DockState;
            Rectangle rectTabStrip = GetLogicalTabStripRectangle(dockState);

            if (rectTabStrip.IsEmpty)
                return Rectangle.Empty;

            int x = tab.TabX;
            int y = rectTabStrip.Y +
                (dockState == DockState.DockTopAutoHide || dockState == DockState.DockRightAutoHide ?
                0 : TabGapTop);
            int width = tab.TabWidth;
            int height = rectTabStrip.Height - TabGapTop;

            if (!transformed)
                return new Rectangle(x, y, width, height);
            else
                return GetTransformedRectangle(dockState, new Rectangle(x, y, width, height));
        }

        private Rectangle GetTransformedRectangle(DockState dockState, Rectangle rect)
        {
            if (dockState != DockState.DockLeftAutoHide && dockState != DockState.DockRightAutoHide)
                return rect;

            PointF[] pts = new PointF[1];
            // the center of the rectangle
            pts[0].X = (float)rect.X + (float)rect.Width / 2;
            pts[0].Y = (float)rect.Y + (float)rect.Height / 2;
            Rectangle rectTabStrip = GetLogicalTabStripRectangle(dockState);
            Matrix matrix = new Matrix();
            matrix.RotateAt(90, new PointF((float)rectTabStrip.X + (float)rectTabStrip.Height / 2,
                (float)rectTabStrip.Y + (float)rectTabStrip.Height / 2));
            matrix.TransformPoints(pts);

            return new Rectangle((int)(pts[0].X - (float)rect.Height / 2 + .5F),
                (int)(pts[0].Y - (float)rect.Width / 2 + .5F),
                rect.Height, rect.Width);
        }

        protected override IDockContent HitTest(Point ptMouse)
        {
            foreach (DockState state in DockStates)
            {
                Rectangle rectTabStrip = GetLogicalTabStripRectangle(state, true);
                if (!rectTabStrip.Contains(ptMouse))
                    continue;

                foreach (Pane pane in GetPanes(state))
                {
                    foreach (TabVS2005 tab in pane.AutoHideTabs)
                    {
                        GraphicsPath path = GetTabOutline(tab, true, true);
                        if (path.IsVisible(ptMouse))
                            return tab.Content;
                    }
                }
            }

            return null;
        }

        protected internal override int MeasureHeight()
        {
            return Math.Max(ImageGapBottom +
                ImageGapTop + ImageHeight,
                TextFont.Height) + TabGapTop;
        }

        protected override void OnRefreshChanges()
        {
            CalculateTabs();
            Invalidate();
        }

        protected override AutoHideStripBase.Tab CreateTab(IDockContent content)
        {
            return new TabVS2005(content);
        }
    }
}
