using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Security.Permissions;
using System.Diagnostics.CodeAnalysis;

namespace WeifenLuo.WinFormsUI.Docking
{
    [ToolboxItem(false)]
    public partial class DockPane : UserControl, IDockDragSource
    {
        public enum AppearanceStyle
        {
            ToolWindow,
            Document
        }

        private enum HitTestArea
        {
            Caption,
            TabStrip,
            Content,
            None
        }

        private struct HitTestResult
        {
            public HitTestArea HitArea;
            public int Index;

            public HitTestResult(HitTestArea hitTestArea, int index)
            {
                HitArea = hitTestArea;
                Index = index;
            }
        }

        private DockPaneCaptionBase m_captionControl;
        private DockPaneCaptionBase CaptionControl
        {
            get { return m_captionControl; }
        }

        private DockPaneStripBase m_tabStripControl;
        public DockPaneStripBase TabStripControl
        {
            get { return m_tabStripControl; }
        }

        internal protected DockPane(IDockContent content, DockState visibleState, bool show)
        {
            InternalConstruct(content, visibleState, false, Rectangle.Empty, null, DockAlignment.Right, 0.5, show);
        }

        [SuppressMessage("Microsoft.Naming", "CA1720:AvoidTypeNamesInParameters", MessageId = "1#")]
        internal protected DockPane(IDockContent content, FloatWindow floatWindow, bool show)
        {
            if (floatWindow == null)
                throw new ArgumentNullException("floatWindow");

            InternalConstruct(content, DockState.Float, false, Rectangle.Empty, floatWindow.NestedPanes.GetDefaultPreviousPane(this), DockAlignment.Right, 0.5, show);
        }

        internal protected DockPane(IDockContent content, DockPane previousPane, DockAlignment alignment, double proportion, bool show)
        {
            if (previousPane == null)
                throw (new ArgumentNullException("previousPane"));
            InternalConstruct(content, previousPane.DockState, false, Rectangle.Empty, previousPane, alignment, proportion, show);
        }

        [SuppressMessage("Microsoft.Naming", "CA1720:AvoidTypeNamesInParameters", MessageId = "1#")]
        internal protected DockPane(IDockContent content, Rectangle floatWindowBounds, bool show)
        {
            InternalConstruct(content, DockState.Float, true, floatWindowBounds, null, DockAlignment.Right, 0.5, show);
        }

        private void InternalConstruct(IDockContent content, DockState dockState, bool flagBounds, Rectangle floatWindowBounds, DockPane prevPane, DockAlignment alignment, double proportion, bool show)
        {
            if (dockState == DockState.Hidden || dockState == DockState.Unknown)
                throw new ArgumentException(Strings.DockPane_SetDockState_InvalidState);

            if (content == null)
                throw new ArgumentNullException(Strings.DockPane_Constructor_NullContent);

            if (content.DockHandler.DockPanel == null)
                throw new ArgumentException(Strings.DockPane_Constructor_NullDockPanel);


            SuspendLayout();
            SetStyle(ControlStyles.Selectable, false);

            m_isFloat = (dockState == DockState.Float);

            m_contents = new DockContentCollection();
            m_displayingContents = new DockContentCollection(this);
            m_dockPanel = content.DockHandler.DockPanel;
            m_dockPanel.AddPane(this);

            m_splitter = new SplitterControl(this);

            m_nestedDockingStatus = new NestedDockingStatus(this);

            m_captionControl = DockPanel.DockPaneCaptionFactory.CreateDockPaneCaption(this);
            m_tabStripControl = DockPanel.DockPaneStripFactory.CreateDockPaneStrip(this);
            Controls.AddRange(new Control[] { m_captionControl, m_tabStripControl });

            DockPanel.SuspendLayout(true);
            if (flagBounds)
                FloatWindow = DockPanel.FloatWindowFactory.CreateFloatWindow(DockPanel, this, floatWindowBounds);
            else if (prevPane != null)
                DockTo(prevPane.NestedPanesContainer, prevPane, alignment, proportion);

            SetDockState(dockState);
            if (show)
                content.DockHandler.Pane = this;
            else if (this.IsFloat)
                content.DockHandler.FloatPane = this;
            else
                content.DockHandler.PanelPane = this;

            ResumeLayout();
            DockPanel.ResumeLayout(true, true);
        }

        private bool m_isDisposing;

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                // IMPORTANT: avoid nested call into this method on Mono. 
                // https://github.com/dockpanelsuite/dockpanelsuite/issues/16
                if (Win32Helper.IsRunningOnMono)
                {
                    if (m_isDisposing)
                        return;

                    m_isDisposing = true;
                }

                m_dockState = DockState.Unknown;

                if (NestedPanesContainer != null)
                    NestedPanesContainer.NestedPanes.Remove(this);

                if (DockPanel != null)
                {
                    DockPanel.RemovePane(this);
                    m_dockPanel = null;
                }

                Splitter.Dispose();
                if (m_autoHidePane != null)
                    m_autoHidePane.Dispose();
            }
            base.Dispose(disposing);
        }

        private IDockContent m_activeContent = null;
        public virtual IDockContent ActiveContent
        {
            get { return m_activeContent; }
            set
            {
                if (ActiveContent == value)
                    return;

                if (value != null)
                {
                    if (!DisplayingContents.Contains(value))
                        throw (new InvalidOperationException(Strings.DockPane_ActiveContent_InvalidValue));
                }
                else
                {
                    if (DisplayingContents.Count != 0)
                        throw (new InvalidOperationException(Strings.DockPane_ActiveContent_InvalidValue));
                }

                IDockContent oldValue = m_activeContent;

                if (DockPanel.ActiveAutoHideContent == oldValue)
                    DockPanel.ActiveAutoHideContent = null;

                m_activeContent = value;

                if (DockPanel.DocumentStyle == DocumentStyle.DockingMdi && DockState == DockState.Document)
                {
                    if (m_activeContent != null)
                        m_activeContent.DockHandler.Form.BringToFront();
                }
                else
                {
                    if (m_activeContent != null)
                        m_activeContent.DockHandler.SetVisible();
                    if (oldValue != null && DisplayingContents.Contains(oldValue))
                        oldValue.DockHandler.SetVisible();
                    if (IsActivated && m_activeContent != null)
                        m_activeContent.DockHandler.Activate();
                }

                if (FloatWindow != null)
                    FloatWindow.SetText();

                if (DockPanel.DocumentStyle == DocumentStyle.DockingMdi &&
                    DockState == DockState.Document)
                    RefreshChanges(false);  // delayed layout to reduce screen flicker
                else
                    RefreshChanges();

                if (m_activeContent != null)
                    TabStripControl.EnsureTabVisible(m_activeContent);
            }
        }

        private bool m_allowDockDragAndDrop = true;
        public virtual bool AllowDockDragAndDrop
        {
            get { return m_allowDockDragAndDrop; }
            set { m_allowDockDragAndDrop = value; }
        }

        private IDisposable m_autoHidePane = null;
        internal IDisposable AutoHidePane
        {
            get { return m_autoHidePane; }
            set { m_autoHidePane = value; }
        }

        private object m_autoHideTabs = null;
        internal object AutoHideTabs
        {
            get { return m_autoHideTabs; }
            set { m_autoHideTabs = value; }
        }

        private object TabPageContextMenu
        {
            get
            {
                IDockContent content = ActiveContent;

                if (content == null)
                    return null;

                if (content.DockHandler.TabPageContextMenuStrip != null)
                    return content.DockHandler.TabPageContextMenuStrip;
                else if (content.DockHandler.TabPageContextMenu != null)
                    return content.DockHandler.TabPageContextMenu;
                else
                    return null;
            }
        }

        internal bool HasTabPageContextMenu
        {
            get { return TabPageContextMenu != null; }
        }

        internal void ShowTabPageContextMenu(Control control, Point position)
        {
            object menu = TabPageContextMenu;

            if (menu == null)
                return;

            ContextMenuStrip contextMenuStrip = menu as ContextMenuStrip;
            if (contextMenuStrip != null)
            {
                contextMenuStrip.Show(control, position);
                return;
            }

            ContextMenu contextMenu = menu as ContextMenu;
            if (contextMenu != null)
                contextMenu.Show(this, position);
        }

        private Rectangle CaptionRectangle
        {
            get
            {
                if (!HasCaption)
                    return Rectangle.Empty;

                Rectangle rectWindow = DisplayingRectangle;
                int x, y, width;
                x = rectWindow.X;
                y = rectWindow.Y;
                width = rectWindow.Width;
                int height = CaptionControl.MeasureHeight();

                return new Rectangle(x, y, width, height);
            }
        }

        internal Rectangle ContentRectangle
        {
            get
            {
                Rectangle rectWindow = DisplayingRectangle;
                Rectangle rectCaption = CaptionRectangle;
                Rectangle rectTabStrip = TabStripRectangle;

                int x = rectWindow.X;

                int y = rectWindow.Y + (rectCaption.IsEmpty ? 0 : rectCaption.Height);
                if (DockState == DockState.Document && DockPanel.DocumentTabStripLocation == DocumentTabStripLocation.Top)
                    y += rectTabStrip.Height;

                int width = rectWindow.Width;
                int height = rectWindow.Height - rectCaption.Height - rectTabStrip.Height;

                return new Rectangle(x, y, width, height);
            }
        }

        internal Rectangle TabStripRectangle
        {
            get
            {
                if (Appearance == AppearanceStyle.ToolWindow)
                    return TabStripRectangle_ToolWindow;
                else
                    return TabStripRectangle_Document;
            }
        }

        private Rectangle TabStripRectangle_ToolWindow
        {
            get
            {
                if (DisplayingContents.Count <= 1 || IsAutoHide)
                    return Rectangle.Empty;

                Rectangle rectWindow = DisplayingRectangle;

                int width = rectWindow.Width;
                int height = TabStripControl.MeasureHeight();
                int x = rectWindow.X;
                int y = rectWindow.Bottom - height;
                Rectangle rectCaption = CaptionRectangle;
                if (rectCaption.Contains(x, y))
                    y = rectCaption.Y + rectCaption.Height;

                return new Rectangle(x, y, width, height);
            }
        }

        private Rectangle TabStripRectangle_Document
        {
            get
            {
                if (DisplayingContents.Count == 0)
                    return Rectangle.Empty;

                if (DisplayingContents.Count == 1 && DockPanel.DocumentStyle == DocumentStyle.DockingSdi)
                    return Rectangle.Empty;

                Rectangle rectWindow = DisplayingRectangle;
                int x = rectWindow.X;
                int width = rectWindow.Width;
                int height = TabStripControl.MeasureHeight();

                int y = 0;
                if (DockPanel.DocumentTabStripLocation == DocumentTabStripLocation.Bottom)
                    y = rectWindow.Height - height;
                else
                    y = rectWindow.Y;

                return new Rectangle(x, y, width, height);
            }
        }

        public virtual string CaptionText
        {
            get { return ActiveContent == null ? string.Empty : ActiveContent.DockHandler.TabText; }
        }

        private DockContentCollection m_contents;
        public DockContentCollection Contents
        {
            get { return m_contents; }
        }

        private DockContentCollection m_displayingContents;
        public DockContentCollection DisplayingContents
        {
            get { return m_displayingContents; }
        }

        private DockPanel m_dockPanel;
        public DockPanel DockPanel
        {
            get { return m_dockPanel; }
        }

        private bool HasCaption
        {
            get
            {
                if (DockState == DockState.Document ||
                    DockState == DockState.Hidden ||
                    DockState == DockState.Unknown ||
                    (DockState == DockState.Float && FloatWindow.VisibleNestedPanes.Count <= 1))
                    return false;
                else
                    return true;
            }
        }

        private bool m_isActivated = false;
        public bool IsActivated
        {
            get { return m_isActivated; }
        }
        internal void SetIsActivated(bool value)
        {
            if (m_isActivated == value)
                return;

            m_isActivated = value;
            if (DockState != DockState.Document)
                RefreshChanges(false);
            OnIsActivatedChanged(EventArgs.Empty);
        }

        private bool m_isActiveDocumentPane = false;
        public bool IsActiveDocumentPane
        {
            get { return m_isActiveDocumentPane; }
        }
        internal void SetIsActiveDocumentPane(bool value)
        {
            if (m_isActiveDocumentPane == value)
                return;

            m_isActiveDocumentPane = value;
            if (DockState == DockState.Document)
                RefreshChanges();
            OnIsActiveDocumentPaneChanged(EventArgs.Empty);
        }

        public bool IsDockStateValid(DockState dockState)
        {
            foreach (IDockContent content in Contents)
                if (!content.DockHandler.IsDockStateValid(dockState))
                    return false;

            return true;
        }

        public bool IsAutoHide
        {
            get { return DockHelper.IsDockStateAutoHide(DockState); }
        }

        public AppearanceStyle Appearance
        {
            get { return (DockState == DockState.Document) ? AppearanceStyle.Document : AppearanceStyle.ToolWindow; }
        }

        internal Rectangle DisplayingRectangle
        {
            get { return ClientRectangle; }
        }

        public void Activate()
        {
            if (DockHelper.IsDockStateAutoHide(DockState) && DockPanel.ActiveAutoHideContent != ActiveContent)
                DockPanel.ActiveAutoHideContent = ActiveContent;
            else if (!IsActivated && ActiveContent != null)
                ActiveContent.DockHandler.Activate();
        }

        internal void AddContent(IDockContent content)
        {
            if (Contents.Contains(content))
                return;

            Contents.Add(content);
        }

        internal void Close()
        {
            Dispose();
        }

        public void CloseActiveContent()
        {
            CloseContent(ActiveContent);
        }

        internal void CloseContent(IDockContent content)
        {
            if (content == null)
                return;

            if (!content.DockHandler.CloseButton)
                return;

            DockPanel dockPanel = DockPanel;

            dockPanel.SuspendLayout(true);

            try
            {
                if (content.DockHandler.HideOnClose)
                {
                    content.DockHandler.Hide();
                    NestedDockingStatus.NestedPanes.SwitchPaneWithFirstChild(this);
                }
                else
                    content.DockHandler.Close();
            }
            finally
            {
                dockPanel.ResumeLayout(true, true);
            }
        }

        private HitTestResult GetHitTest(Point ptMouse)
        {
            Point ptMouseClient = PointToClient(ptMouse);

            Rectangle rectCaption = CaptionRectangle;
            if (rectCaption.Contains(ptMouseClient))
                return new HitTestResult(HitTestArea.Caption, -1);

            Rectangle rectContent = ContentRectangle;
            if (rectContent.Contains(ptMouseClient))
                return new HitTestResult(HitTestArea.Content, -1);

            Rectangle rectTabStrip = TabStripRectangle;
            if (rectTabStrip.Contains(ptMouseClient))
                return new HitTestResult(HitTestArea.TabStrip, TabStripControl.HitTest(TabStripControl.PointToClient(ptMouse)));

            return new HitTestResult(HitTestArea.None, -1);
        }

        private bool m_isHidden = true;
        public bool IsHidden
        {
            get { return m_isHidden; }
        }
        private void SetIsHidden(bool value)
        {
            if (m_isHidden == value)
                return;

            m_isHidden = value;
            if (DockHelper.IsDockStateAutoHide(DockState))
            {
                DockPanel.RefreshAutoHideStrip();
                DockPanel.PerformLayout();
            }
            else if (NestedPanesContainer != null)
                ((Control)NestedPanesContainer).PerformLayout();
        }

        protected override void OnLayout(LayoutEventArgs levent)
        {
            SetIsHidden(DisplayingContents.Count == 0);
            if (!IsHidden)
            {
                CaptionControl.Bounds = CaptionRectangle;
                TabStripControl.Bounds = TabStripRectangle;

                SetContentBounds();

                foreach (IDockContent content in Contents)
                {
                    if (DisplayingContents.Contains(content))
                        if (content.DockHandler.FlagClipWindow && content.DockHandler.Form.Visible)
                            content.DockHandler.FlagClipWindow = false;
                }
            }

            base.OnLayout(levent);
        }

        internal void SetContentBounds()
        {
            Rectangle rectContent = ContentRectangle;
            if (DockState == DockState.Document && DockPanel.DocumentStyle == DocumentStyle.DockingMdi)
                rectContent = DockPanel.RectangleToMdiClient(RectangleToScreen(rectContent));

            Rectangle rectInactive = new Rectangle(-rectContent.Width, rectContent.Y, rectContent.Width, rectContent.Height);
            foreach (IDockContent content in Contents)
                if (content.DockHandler.Pane == this)
                {
                    if (content == ActiveContent)
                        content.DockHandler.Form.Bounds = rectContent;
                    else
                        content.DockHandler.Form.Bounds = rectInactive;
                }
        }

        internal void RefreshChanges()
        {
            RefreshChanges(true);
        }

        private void RefreshChanges(bool performLayout)
        {
            if (IsDisposed)
                return;

            CaptionControl.RefreshChanges();
            TabStripControl.RefreshChanges();
            if (DockState == DockState.Float && FloatWindow != null)
                FloatWindow.RefreshChanges();
            if (DockHelper.IsDockStateAutoHide(DockState) && DockPanel != null)
            {
                DockPanel.RefreshAutoHideStrip();
                DockPanel.PerformLayout();
            }

            if (performLayout)
                PerformLayout();
        }

        internal void RemoveContent(IDockContent content)
        {
            if (!Contents.Contains(content))
                return;

            Contents.Remove(content);
        }

        public void SetContentIndex(IDockContent content, int index)
        {
            int oldIndex = Contents.IndexOf(content);
            if (oldIndex == -1)
                throw (new ArgumentException(Strings.DockPane_SetContentIndex_InvalidContent));

            if (index < 0 || index > Contents.Count - 1)
                if (index != -1)
                    throw (new ArgumentOutOfRangeException(Strings.DockPane_SetContentIndex_InvalidIndex));

            if (oldIndex == index)
                return;
            if (oldIndex == Contents.Count - 1 && index == -1)
                return;

            Contents.Remove(content);
            if (index == -1)
                Contents.Add(content);
            else if (oldIndex < index)
                Contents.AddAt(content, index - 1);
            else
                Contents.AddAt(content, index);

            RefreshChanges();
        }

        private void SetParent()
        {
            if (DockState == DockState.Unknown || DockState == DockState.Hidden)
            {
                SetParent(null);
                Splitter.Parent = null;
            }
            else if (DockState == DockState.Float)
            {
                SetParent(FloatWindow);
                Splitter.Parent = FloatWindow;
            }
            else if (DockHelper.IsDockStateAutoHide(DockState))
            {
                SetParent(DockPanel.AutoHideControl);
                Splitter.Parent = null;
            }
            else
            {
                SetParent(DockPanel.DockWindows[DockState]);
                Splitter.Parent = Parent;
            }
        }

        private void SetParent(Control value)
        {
            if (Parent == value)
                return;

            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // Workaround of .Net Framework bug:
            // Change the parent of a control with focus may result in the first
            // MDI child form get activated. 
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            IDockContent contentFocused = GetFocusedContent();
            if (contentFocused != null)
                DockPanel.SaveFocus();

            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

            Parent = value;

            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // Workaround of .Net Framework bug:
            // Change the parent of a control with focus may result in the first
            // MDI child form get activated. 
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            if (contentFocused != null)
                contentFocused.DockHandler.Activate();
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        }

        public new void Show()
        {
            Activate();
        }

        internal void TestDrop(IDockDragSource dragSource, DockOutlineBase dockOutline)
        {
            if (!dragSource.CanDockTo(this))
                return;

            Point ptMouse = Control.MousePosition;

            HitTestResult hitTestResult = GetHitTest(ptMouse);
            if (hitTestResult.HitArea == HitTestArea.Caption)
                dockOutline.Show(this, -1);
            else if (hitTestResult.HitArea == HitTestArea.TabStrip && hitTestResult.Index != -1)
                dockOutline.Show(this, hitTestResult.Index);
        }

        internal void ValidateActiveContent()
        {
            if (ActiveContent == null)
            {
                if (DisplayingContents.Count != 0)
                    ActiveContent = DisplayingContents[0];
                return;
            }

            if (DisplayingContents.IndexOf(ActiveContent) >= 0)
                return;

            IDockContent prevVisible = null;
            for (int i = Contents.IndexOf(ActiveContent) - 1; i >= 0; i--)
                if (Contents[i].DockHandler.DockState == DockState)
                {
                    prevVisible = Contents[i];
                    break;
                }

            IDockContent nextVisible = null;
            for (int i = Contents.IndexOf(ActiveContent) + 1; i < Contents.Count; i++)
                if (Contents[i].DockHandler.DockState == DockState)
                {
                    nextVisible = Contents[i];
                    break;
                }

            if (prevVisible != null)
                ActiveContent = prevVisible;
            else if (nextVisible != null)
                ActiveContent = nextVisible;
            else
                ActiveContent = null;
        }

        private static readonly object DockStateChangedEvent = new object();
        public event EventHandler DockStateChanged
        {
            add { Events.AddHandler(DockStateChangedEvent, value); }
            remove { Events.RemoveHandler(DockStateChangedEvent, value); }
        }
        protected virtual void OnDockStateChanged(EventArgs e)
        {
            EventHandler handler = (EventHandler)Events[DockStateChangedEvent];
            if (handler != null)
                handler(this, e);
        }

        private static readonly object IsActivatedChangedEvent = new object();
        public event EventHandler IsActivatedChanged
        {
            add { Events.AddHandler(IsActivatedChangedEvent, value); }
            remove { Events.RemoveHandler(IsActivatedChangedEvent, value); }
        }
        protected virtual void OnIsActivatedChanged(EventArgs e)
        {
            EventHandler handler = (EventHandler)Events[IsActivatedChangedEvent];
            if (handler != null)
                handler(this, e);
        }

        private static readonly object IsActiveDocumentPaneChangedEvent = new object();
        public event EventHandler IsActiveDocumentPaneChanged
        {
            add { Events.AddHandler(IsActiveDocumentPaneChangedEvent, value); }
            remove { Events.RemoveHandler(IsActiveDocumentPaneChangedEvent, value); }
        }
        protected virtual void OnIsActiveDocumentPaneChanged(EventArgs e)
        {
            EventHandler handler = (EventHandler)Events[IsActiveDocumentPaneChangedEvent];
            if (handler != null)
                handler(this, e);
        }

        public DockWindow DockWindow
        {
            get { return (m_nestedDockingStatus.NestedPanes == null) ? null : m_nestedDockingStatus.NestedPanes.Container as DockWindow; }
            set
            {
                DockWindow oldValue = DockWindow;
                if (oldValue == value)
                    return;

                DockTo(value);
            }
        }

        public FloatWindow FloatWindow
        {
            get { return (m_nestedDockingStatus.NestedPanes == null) ? null : m_nestedDockingStatus.NestedPanes.Container as FloatWindow; }
            set
            {
                FloatWindow oldValue = FloatWindow;
                if (oldValue == value)
                    return;

                DockTo(value);
            }
        }

        private NestedDockingStatus m_nestedDockingStatus;
        public NestedDockingStatus NestedDockingStatus
        {
            get { return m_nestedDockingStatus; }
        }

        private bool m_isFloat;
        public bool IsFloat
        {
            get { return m_isFloat; }
        }

        public INestedPanesContainer NestedPanesContainer
        {
            get
            {
                if (NestedDockingStatus.NestedPanes == null)
                    return null;
                else
                    return NestedDockingStatus.NestedPanes.Container;
            }
        }

        private DockState m_dockState = DockState.Unknown;
        public DockState DockState
        {
            get { return m_dockState; }
            set
            {
                SetDockState(value);
            }
        }

        public DockPane SetDockState(DockState value)
        {
            if (value == DockState.Unknown || value == DockState.Hidden)
                throw new InvalidOperationException(Strings.DockPane_SetDockState_InvalidState);

            if ((value == DockState.Float) == this.IsFloat)
            {
                InternalSetDockState(value);
                return this;
            }

            if (DisplayingContents.Count == 0)
                return null;

            IDockContent firstContent = null;
            for (int i = 0; i < DisplayingContents.Count; i++)
            {
                IDockContent content = DisplayingContents[i];
                if (content.DockHandler.IsDockStateValid(value))
                {
                    firstContent = content;
                    break;
                }
            }
            if (firstContent == null)
                return null;

            firstContent.DockHandler.DockState = value;
            DockPane pane = firstContent.DockHandler.Pane;
            DockPanel.SuspendLayout(true);
            for (int i = 0; i < DisplayingContents.Count; i++)
            {
                IDockContent content = DisplayingContents[i];
                if (content.DockHandler.IsDockStateValid(value))
                    content.DockHandler.Pane = pane;
            }
            DockPanel.ResumeLayout(true, true);
            return pane;
        }

        private void InternalSetDockState(DockState value)
        {
            if (m_dockState == value)
                return;

            DockState oldDockState = m_dockState;
            INestedPanesContainer oldContainer = NestedPanesContainer;

            m_dockState = value;

            SuspendRefreshStateChange();

            IDockContent contentFocused = GetFocusedContent();
            if (contentFocused != null)
                DockPanel.SaveFocus();

            if (!IsFloat)
                DockWindow = DockPanel.DockWindows[DockState];
            else if (FloatWindow == null)
                FloatWindow = DockPanel.FloatWindowFactory.CreateFloatWindow(DockPanel, this);

            if (contentFocused != null)
            {
                if (!Win32Helper.IsRunningOnMono)
                {
                    DockPanel.ContentFocusManager.Activate(contentFocused);
                }
            }

            ResumeRefreshStateChange(oldContainer, oldDockState);
        }

        private int m_countRefreshStateChange = 0;
        private void SuspendRefreshStateChange()
        {
            m_countRefreshStateChange++;
            DockPanel.SuspendLayout(true);
        }

        private void ResumeRefreshStateChange()
        {
            m_countRefreshStateChange--;
            System.Diagnostics.Debug.Assert(m_countRefreshStateChange >= 0);
            DockPanel.ResumeLayout(true, true);
        }

        private bool IsRefreshStateChangeSuspended
        {
            get { return m_countRefreshStateChange != 0; }
        }

        private void ResumeRefreshStateChange(INestedPanesContainer oldContainer, DockState oldDockState)
        {
            ResumeRefreshStateChange();
            RefreshStateChange(oldContainer, oldDockState);
        }

        private void RefreshStateChange(INestedPanesContainer oldContainer, DockState oldDockState)
        {
            if (IsRefreshStateChangeSuspended)
                return;

            SuspendRefreshStateChange();

            DockPanel.SuspendLayout(true);

            IDockContent contentFocused = GetFocusedContent();
            if (contentFocused != null)
                DockPanel.SaveFocus();
            SetParent();

            if (ActiveContent != null)
                ActiveContent.DockHandler.SetDockState(ActiveContent.DockHandler.IsHidden, DockState, ActiveContent.DockHandler.Pane);
            foreach (IDockContent content in Contents)
            {
                if (content.DockHandler.Pane == this)
                    content.DockHandler.SetDockState(content.DockHandler.IsHidden, DockState, content.DockHandler.Pane);
            }

            if (oldContainer != null)
            {
                Control oldContainerControl = (Control)oldContainer;
                if (oldContainer.DockState == oldDockState && !oldContainerControl.IsDisposed)
                    oldContainerControl.PerformLayout();
            }
            if (DockHelper.IsDockStateAutoHide(oldDockState))
                DockPanel.RefreshActiveAutoHideContent();

            if (NestedPanesContainer.DockState == DockState)
                ((Control)NestedPanesContainer).PerformLayout();
            if (DockHelper.IsDockStateAutoHide(DockState))
                DockPanel.RefreshActiveAutoHideContent();

            if (DockHelper.IsDockStateAutoHide(oldDockState) ||
                DockHelper.IsDockStateAutoHide(DockState))
            {
                DockPanel.RefreshAutoHideStrip();
                DockPanel.PerformLayout();
            }

            ResumeRefreshStateChange();

            if (contentFocused != null)
                contentFocused.DockHandler.Activate();

            DockPanel.ResumeLayout(true, true);

            if (oldDockState != DockState)
                OnDockStateChanged(EventArgs.Empty);
        }

        private IDockContent GetFocusedContent()
        {
            IDockContent contentFocused = null;
            foreach (IDockContent content in Contents)
            {
                if (content.DockHandler.Form.ContainsFocus)
                {
                    contentFocused = content;
                    break;
                }
            }

            return contentFocused;
        }

        public DockPane DockTo(INestedPanesContainer container)
        {
            if (container == null)
                throw new InvalidOperationException(Strings.DockPane_DockTo_NullContainer);

            DockAlignment alignment;
            if (container.DockState == DockState.DockLeft || container.DockState == DockState.DockRight)
                alignment = DockAlignment.Bottom;
            else
                alignment = DockAlignment.Right;

            return DockTo(container, container.NestedPanes.GetDefaultPreviousPane(this), alignment, 0.5);
        }

        public DockPane DockTo(INestedPanesContainer container, DockPane previousPane, DockAlignment alignment, double proportion)
        {
            if (container == null)
                throw new InvalidOperationException(Strings.DockPane_DockTo_NullContainer);

            if (container.IsFloat == this.IsFloat)
            {
                InternalAddToDockList(container, previousPane, alignment, proportion);
                return this;
            }

            IDockContent firstContent = GetFirstContent(container.DockState);
            if (firstContent == null)
                return null;

            DockPane pane;
            DockPanel.DummyContent.DockPanel = DockPanel;
            if (container.IsFloat)
                pane = DockPanel.DockPaneFactory.CreateDockPane(DockPanel.DummyContent, (FloatWindow)container, true);
            else
                pane = DockPanel.DockPaneFactory.CreateDockPane(DockPanel.DummyContent, container.DockState, true);

            pane.DockTo(container, previousPane, alignment, proportion);
            SetVisibleContentsToPane(pane);
            DockPanel.DummyContent.DockPanel = null;

            return pane;
        }

        private void SetVisibleContentsToPane(DockPane pane)
        {
            SetVisibleContentsToPane(pane, ActiveContent);
        }

        private void SetVisibleContentsToPane(DockPane pane, IDockContent activeContent)
        {
            for (int i = 0; i < DisplayingContents.Count; i++)
            {
                IDockContent content = DisplayingContents[i];
                if (content.DockHandler.IsDockStateValid(pane.DockState))
                {
                    content.DockHandler.Pane = pane;
                    i--;
                }
            }

            if (activeContent.DockHandler.Pane == pane)
                pane.ActiveContent = activeContent;
        }

        private void InternalAddToDockList(INestedPanesContainer container, DockPane prevPane, DockAlignment alignment, double proportion)
        {
            if ((container.DockState == DockState.Float) != IsFloat)
                throw new InvalidOperationException(Strings.DockPane_DockTo_InvalidContainer);

            int count = container.NestedPanes.Count;
            if (container.NestedPanes.Contains(this))
                count--;
            if (prevPane == null && count > 0)
                throw new InvalidOperationException(Strings.DockPane_DockTo_NullPrevPane);

            if (prevPane != null && !container.NestedPanes.Contains(prevPane))
                throw new InvalidOperationException(Strings.DockPane_DockTo_NoPrevPane);

            if (prevPane == this)
                throw new InvalidOperationException(Strings.DockPane_DockTo_SelfPrevPane);

            INestedPanesContainer oldContainer = NestedPanesContainer;
            DockState oldDockState = DockState;
            container.NestedPanes.Add(this);
            NestedDockingStatus.SetStatus(container.NestedPanes, prevPane, alignment, proportion);

            if (DockHelper.IsDockWindowState(DockState))
                m_dockState = container.DockState;

            RefreshStateChange(oldContainer, oldDockState);
        }

        public void SetNestedDockingProportion(double proportion)
        {
            NestedDockingStatus.SetStatus(NestedDockingStatus.NestedPanes, NestedDockingStatus.PreviousPane, NestedDockingStatus.Alignment, proportion);
            if (NestedPanesContainer != null)
                ((Control)NestedPanesContainer).PerformLayout();
        }

        public DockPane Float()
        {
            DockPanel.SuspendLayout(true);

            IDockContent activeContent = ActiveContent;

            DockPane floatPane = GetFloatPaneFromContents();
            if (floatPane == null)
            {
                IDockContent firstContent = GetFirstContent(DockState.Float);
                if (firstContent == null)
                {
                    DockPanel.ResumeLayout(true, true);
                    return null;
                }
                floatPane = DockPanel.DockPaneFactory.CreateDockPane(firstContent, DockState.Float, true);
            }
            SetVisibleContentsToPane(floatPane, activeContent);

            DockPanel.ResumeLayout(true, true);
            return floatPane;
        }

        private DockPane GetFloatPaneFromContents()
        {
            DockPane floatPane = null;
            for (int i = 0; i < DisplayingContents.Count; i++)
            {
                IDockContent content = DisplayingContents[i];
                if (!content.DockHandler.IsDockStateValid(DockState.Float))
                    continue;

                if (floatPane != null && content.DockHandler.FloatPane != floatPane)
                    return null;
                else
                    floatPane = content.DockHandler.FloatPane;
            }

            return floatPane;
        }

        private IDockContent GetFirstContent(DockState dockState)
        {
            for (int i = 0; i < DisplayingContents.Count; i++)
            {
                IDockContent content = DisplayingContents[i];
                if (content.DockHandler.IsDockStateValid(dockState))
                    return content;
            }
            return null;
        }

        public void RestoreToPanel()
        {
            DockPanel.SuspendLayout(true);

            IDockContent activeContent = DockPanel.ActiveContent;

            for (int i = DisplayingContents.Count - 1; i >= 0; i--)
            {
                IDockContent content = DisplayingContents[i];
                if (content.DockHandler.CheckDockState(false) != DockState.Unknown)
                    content.DockHandler.IsFloat = false;
            }

            DockPanel.ResumeLayout(true, true);
        }

        [SecurityPermission(SecurityAction.LinkDemand, Flags = SecurityPermissionFlag.UnmanagedCode)]
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == (int)Win32.Msgs.WM_MOUSEACTIVATE)
                Activate();

            base.WndProc(ref m);
        }

        #region IDockDragSource Members

        #region IDragSource Members

        Control IDragSource.DragControl
        {
            get { return this; }
        }

        #endregion

        bool IDockDragSource.IsDockStateValid(DockState dockState)
        {
            return IsDockStateValid(dockState);
        }

        bool IDockDragSource.CanDockTo(DockPane pane)
        {
            if (!IsDockStateValid(pane.DockState))
                return false;

            if (pane == this)
                return false;

            return true;
        }

        Rectangle IDockDragSource.BeginDrag(Point ptMouse)
        {
            Point location = PointToScreen(new Point(0, 0));
            Size size;

            DockPane floatPane = ActiveContent.DockHandler.FloatPane;
            if (DockState == DockState.Float || floatPane == null || floatPane.FloatWindow.NestedPanes.Count != 1)
                size = DockPanel.DefaultFloatWindowSize;
            else
                size = floatPane.FloatWindow.Size;

            if (ptMouse.X > location.X + size.Width)
                location.X += ptMouse.X - (location.X + size.Width) + Measures.SplitterSize;

            return new Rectangle(location, size);
        }

        void IDockDragSource.EndDrag()
        {
        }

        public void FloatAt(Rectangle floatWindowBounds)
        {
            if (FloatWindow == null || FloatWindow.NestedPanes.Count != 1)
                FloatWindow = DockPanel.FloatWindowFactory.CreateFloatWindow(DockPanel, this, floatWindowBounds);
            else
                FloatWindow.Bounds = floatWindowBounds;

            DockState = DockState.Float;

            NestedDockingStatus.NestedPanes.SwitchPaneWithFirstChild(this);
        }

        public void DockTo(DockPane pane, DockStyle dockStyle, int contentIndex)
        {
            if (dockStyle == DockStyle.Fill)
            {
                IDockContent activeContent = ActiveContent;
                for (int i = Contents.Count - 1; i >= 0; i--)
                {
                    IDockContent c = Contents[i];
                    if (c.DockHandler.DockState == DockState)
                    {
                        c.DockHandler.Pane = pane;
                        if (contentIndex != -1)
                            pane.SetContentIndex(c, contentIndex);
                    }
                }
                pane.ActiveContent = activeContent;
            }
            else
            {
                if (dockStyle == DockStyle.Left)
                    DockTo(pane.NestedPanesContainer, pane, DockAlignment.Left, 0.5);
                else if (dockStyle == DockStyle.Right)
                    DockTo(pane.NestedPanesContainer, pane, DockAlignment.Right, 0.5);
                else if (dockStyle == DockStyle.Top)
                    DockTo(pane.NestedPanesContainer, pane, DockAlignment.Top, 0.5);
                else if (dockStyle == DockStyle.Bottom)
                    DockTo(pane.NestedPanesContainer, pane, DockAlignment.Bottom, 0.5);

                DockState = pane.DockState;
            }
        }

        public void DockTo(DockPanel panel, DockStyle dockStyle)
        {
            if (panel != DockPanel)
                throw new ArgumentException(Strings.IDockDragSource_DockTo_InvalidPanel, "panel");

            if (dockStyle == DockStyle.Top)
                DockState = DockState.DockTop;
            else if (dockStyle == DockStyle.Bottom)
                DockState = DockState.DockBottom;
            else if (dockStyle == DockStyle.Left)
                DockState = DockState.DockLeft;
            else if (dockStyle == DockStyle.Right)
                DockState = DockState.DockRight;
            else if (dockStyle == DockStyle.Fill)
                DockState = DockState.Document;
        }

        #endregion
    }
}
