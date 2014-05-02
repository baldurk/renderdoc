using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.ComponentModel;
using System.Windows.Forms.VisualStyles;

namespace WeifenLuo.WinFormsUI.Docking
{
    internal class VS2005DockPaneCaption : DockPaneCaptionBase
    {
        private sealed class InertButton : InertButtonBase
        {
            private Bitmap m_image, m_imageAutoHide;

            public InertButton(VS2005DockPaneCaption dockPaneCaption, Bitmap image, Bitmap imageAutoHide)
                : base()
            {
                m_dockPaneCaption = dockPaneCaption;
                m_image = image;
                m_imageAutoHide = imageAutoHide;
                RefreshChanges();
            }

            private VS2005DockPaneCaption m_dockPaneCaption;
            private VS2005DockPaneCaption DockPaneCaption
            {
                get { return m_dockPaneCaption; }
            }

            public bool IsAutoHide
            {
                get { return DockPaneCaption.DockPane.IsAutoHide; }
            }

            public override Bitmap Image
            {
                get { return IsAutoHide ? m_imageAutoHide : m_image; }
            }

            protected override void OnRefreshChanges()
            {
                if (DockPaneCaption.DockPane.DockPanel != null)
                {
                    if (DockPaneCaption.TextColor != ForeColor)
                    {
                        ForeColor = DockPaneCaption.TextColor;
                        Invalidate();
                    }
                }
            }
        }

        #region consts
        private const int _TextGapTop = 2;
        private const int _TextGapBottom = 0;
        private const int _TextGapLeft = 3;
        private const int _TextGapRight = 3;
        private const int _ButtonGapTop = 2;
        private const int _ButtonGapBottom = 1;
        private const int _ButtonGapBetween = 1;
        private const int _ButtonGapLeft = 1;
        private const int _ButtonGapRight = 2;
        #endregion

        private static Bitmap _imageButtonClose;
        private static Bitmap ImageButtonClose
        {
            get
            {
                if (_imageButtonClose == null)
                    _imageButtonClose = Resources.DockPane_Close;

                return _imageButtonClose;
            }
        }

        private InertButton m_buttonClose;
        private InertButton ButtonClose
        {
            get
            {
                if (m_buttonClose == null)
                {
                    m_buttonClose = new InertButton(this, ImageButtonClose, ImageButtonClose);
                    m_toolTip.SetToolTip(m_buttonClose, ToolTipClose);
                    m_buttonClose.Click += new EventHandler(Close_Click);
                    Controls.Add(m_buttonClose);
                }

                return m_buttonClose;
            }
        }

        private static Bitmap _imageButtonAutoHide;
        private static Bitmap ImageButtonAutoHide
        {
            get
            {
                if (_imageButtonAutoHide == null)
                    _imageButtonAutoHide = Resources.DockPane_AutoHide;

                return _imageButtonAutoHide;
            }
        }

        private static Bitmap _imageButtonDock;
        private static Bitmap ImageButtonDock
        {
            get
            {
                if (_imageButtonDock == null)
                    _imageButtonDock = Resources.DockPane_Dock;

                return _imageButtonDock;
            }
        }

        private InertButton m_buttonAutoHide;
        private InertButton ButtonAutoHide
        {
            get
            {
                if (m_buttonAutoHide == null)
                {
                    m_buttonAutoHide = new InertButton(this, ImageButtonDock, ImageButtonAutoHide);
                    m_toolTip.SetToolTip(m_buttonAutoHide, ToolTipAutoHide);
                    m_buttonAutoHide.Click += new EventHandler(AutoHide_Click);
                    Controls.Add(m_buttonAutoHide);
                }

                return m_buttonAutoHide;
            }
        }

        private static Bitmap _imageButtonOptions;
        private static Bitmap ImageButtonOptions
        {
            get
            {
                if (_imageButtonOptions == null)
                    _imageButtonOptions = Resources.DockPane_Option;

                return _imageButtonOptions;
            }
        }

        private InertButton m_buttonOptions;
        private InertButton ButtonOptions
        {
            get
            {
                if (m_buttonOptions == null)
                {
                    m_buttonOptions = new InertButton(this, ImageButtonOptions, ImageButtonOptions);
                    m_toolTip.SetToolTip(m_buttonOptions, ToolTipOptions);
                    m_buttonOptions.Click += new EventHandler(Options_Click);
                    Controls.Add(m_buttonOptions);
                }
                return m_buttonOptions;
            }
        }

        private IContainer m_components;
        private IContainer Components
        {
            get { return m_components; }
        }

        private ToolTip m_toolTip;

        public VS2005DockPaneCaption(DockPane pane) : base(pane)
        {
            SuspendLayout();

            m_components = new Container();
            m_toolTip = new ToolTip(Components);

            ResumeLayout();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
                Components.Dispose();
            base.Dispose(disposing);
        }

        private static int TextGapTop
        {
            get    {    return _TextGapTop;    }
        }

        public Font TextFont
        {
            get { return DockPane.DockPanel.Skin.DockPaneStripSkin.TextFont; }
        }

        private static int TextGapBottom
        {
            get    {    return _TextGapBottom;    }
        }

        private static int TextGapLeft
        {
            get    {    return _TextGapLeft;    }
        }

        private static int TextGapRight
        {
            get    {    return _TextGapRight;    }
        }

        private static int ButtonGapTop
        {
            get    {    return _ButtonGapTop;    }
        }

        private static int ButtonGapBottom
        {
            get    {    return _ButtonGapBottom;    }
        }

        private static int ButtonGapLeft
        {
            get    {    return _ButtonGapLeft;    }
        }

        private static int ButtonGapRight
        {
            get    {    return _ButtonGapRight;    }
        }

        private static int ButtonGapBetween
        {
            get    {    return _ButtonGapBetween;    }
        }

        private static string _toolTipClose;
        private static string ToolTipClose
        {
            get
            {    
                if (_toolTipClose == null)
                    _toolTipClose = Strings.DockPaneCaption_ToolTipClose;
                return _toolTipClose;
            }
        }

        private static string _toolTipOptions;
        private static string ToolTipOptions
        {
            get
            {
                if (_toolTipOptions == null)
                    _toolTipOptions = Strings.DockPaneCaption_ToolTipOptions;

                return _toolTipOptions;
            }
        }

        private static string _toolTipAutoHide;
        private static string ToolTipAutoHide
        {
            get
            {    
                if (_toolTipAutoHide == null)
                    _toolTipAutoHide = Strings.DockPaneCaption_ToolTipAutoHide;
                return _toolTipAutoHide;
            }
        }

        private static Blend _activeBackColorGradientBlend;
        private static Blend ActiveBackColorGradientBlend
        {
            get
            {
                if (_activeBackColorGradientBlend == null)
                {
                    Blend blend = new Blend(2);

                    blend.Factors = new float[]{0.5F, 1.0F};
                    blend.Positions = new float[]{0.0F, 1.0F};
                    _activeBackColorGradientBlend = blend;
                }

                return _activeBackColorGradientBlend;
            }
        }

        private Color TextColor
        {
            get
            {
                if (DockPane.IsActivated)
                    return DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.ActiveCaptionGradient.TextColor;
                else
                    return DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.InactiveCaptionGradient.TextColor;
            }
        }

        private static TextFormatFlags _textFormat =
            TextFormatFlags.SingleLine |
            TextFormatFlags.EndEllipsis |
            TextFormatFlags.VerticalCenter;
        private TextFormatFlags TextFormat
        {
            get
            {
                if (RightToLeft == RightToLeft.No)
                    return _textFormat;
                else
                    return _textFormat | TextFormatFlags.RightToLeft | TextFormatFlags.Right;
            }
        }

        protected internal override int MeasureHeight()
        {
            int height = TextFont.Height + TextGapTop + TextGapBottom;

            if (height < ButtonClose.Image.Height + ButtonGapTop + ButtonGapBottom)
                height = ButtonClose.Image.Height + ButtonGapTop + ButtonGapBottom;

            return height;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint (e);
            DrawCaption(e.Graphics);
        }

        private void DrawCaption(Graphics g)
        {
            if (ClientRectangle.Width == 0 || ClientRectangle.Height == 0)
                return;

            if (DockPane.IsActivated)
            {
                Color startColor = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.ActiveCaptionGradient.StartColor;
                Color endColor = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.ActiveCaptionGradient.EndColor;
                LinearGradientMode gradientMode = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.ActiveCaptionGradient.LinearGradientMode;
                using (LinearGradientBrush brush = new LinearGradientBrush(ClientRectangle, startColor, endColor, gradientMode))
                {
                    brush.Blend = ActiveBackColorGradientBlend;
                    g.FillRectangle(brush, ClientRectangle);
                }
            }
            else
            {
                Color startColor = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.InactiveCaptionGradient.StartColor;
                Color endColor = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.InactiveCaptionGradient.EndColor;
                LinearGradientMode gradientMode = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.InactiveCaptionGradient.LinearGradientMode;
                using (LinearGradientBrush brush = new LinearGradientBrush(ClientRectangle, startColor, endColor, gradientMode))
                {
                    g.FillRectangle(brush, ClientRectangle);
                }
            }

            Rectangle rectCaption = ClientRectangle;

            Rectangle rectCaptionText = rectCaption;
            rectCaptionText.X += TextGapLeft;
            rectCaptionText.Width -= TextGapLeft + TextGapRight;
            rectCaptionText.Width -= ButtonGapLeft + ButtonClose.Width + ButtonGapRight;
            if (ShouldShowAutoHideButton)
                rectCaptionText.Width -= ButtonAutoHide.Width + ButtonGapBetween;
            if (HasTabPageContextMenu)
                rectCaptionText.Width -= ButtonOptions.Width + ButtonGapBetween;
            rectCaptionText.Y += TextGapTop;
            rectCaptionText.Height -= TextGapTop + TextGapBottom;

            Color colorText;
            if (DockPane.IsActivated)
                colorText = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.ActiveCaptionGradient.TextColor;
            else
                colorText = DockPane.DockPanel.Skin.DockPaneStripSkin.ToolWindowGradient.InactiveCaptionGradient.TextColor;

            TextRenderer.DrawText(g, DockPane.CaptionText, TextFont, DrawHelper.RtlTransform(this, rectCaptionText), colorText, TextFormat);
        }

        protected override void OnLayout(LayoutEventArgs levent)
        {
            SetButtonsPosition();
            base.OnLayout (levent);
        }

        protected override void OnRefreshChanges()
        {
            SetButtons();
            Invalidate();
        }

        private bool CloseButtonEnabled
        {
            get    {    return (DockPane.ActiveContent != null)? DockPane.ActiveContent.DockHandler.CloseButton : false;    }
        }

        /// <summary>
        /// Determines whether the close button is visible on the content
        /// </summary>
        private bool CloseButtonVisible
        {
            get { return (DockPane.ActiveContent != null) ? DockPane.ActiveContent.DockHandler.CloseButtonVisible : false; }
        }

        private bool ShouldShowAutoHideButton
        {
            get    {    return !DockPane.IsFloat;    }
        }

        private void SetButtons()
        {
            ButtonClose.Enabled = CloseButtonEnabled;
            ButtonClose.Visible = CloseButtonVisible;
            ButtonAutoHide.Visible = ShouldShowAutoHideButton;
            ButtonOptions.Visible = HasTabPageContextMenu;
            ButtonClose.RefreshChanges();
            ButtonAutoHide.RefreshChanges();
            ButtonOptions.RefreshChanges();
            
            SetButtonsPosition();
        }

        private void SetButtonsPosition()
        {
            // set the size and location for close and auto-hide buttons
            Rectangle rectCaption = ClientRectangle;
            int buttonWidth = ButtonClose.Image.Width;
            int buttonHeight = ButtonClose.Image.Height;
            int height = rectCaption.Height - ButtonGapTop - ButtonGapBottom;
            if (buttonHeight < height)
            {
                buttonWidth = buttonWidth * (height / buttonHeight);
                buttonHeight = height;
            }
            Size buttonSize = new Size(buttonWidth, buttonHeight);
            int x = rectCaption.X + rectCaption.Width - 1 - ButtonGapRight - m_buttonClose.Width;
            int y = rectCaption.Y + ButtonGapTop;
            Point point = new Point(x, y);
            ButtonClose.Bounds = DrawHelper.RtlTransform(this, new Rectangle(point, buttonSize));

            // If the close button is not visible draw the auto hide button overtop.
            // Otherwise it is drawn to the left of the close button.
            if (CloseButtonVisible)
                point.Offset(-(buttonWidth + ButtonGapBetween), 0);
            
            ButtonAutoHide.Bounds = DrawHelper.RtlTransform(this, new Rectangle(point, buttonSize));
            if (ShouldShowAutoHideButton)
                point.Offset(-(buttonWidth + ButtonGapBetween), 0);
            ButtonOptions.Bounds = DrawHelper.RtlTransform(this, new Rectangle(point, buttonSize));
        }

        private void Close_Click(object sender, EventArgs e)
        {
            DockPane.CloseActiveContent();
        }

        private void AutoHide_Click(object sender, EventArgs e)
        {
            DockPane.DockState = DockHelper.ToggleAutoHideState(DockPane.DockState);
            if (DockHelper.IsDockStateAutoHide(DockPane.DockState))
            {
                DockPane.DockPanel.ActiveAutoHideContent = null;
                DockPane.NestedDockingStatus.NestedPanes.SwitchPaneWithFirstChild(DockPane);
            }
        }

        private void Options_Click(object sender, EventArgs e)
        {
            ShowTabPageContextMenu(PointToClient(Control.MousePosition));
        }

        protected override void OnRightToLeftChanged(EventArgs e)
        {
            base.OnRightToLeftChanged(e);
            PerformLayout();
        }
    }
}
