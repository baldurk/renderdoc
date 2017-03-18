using System;
using System.Collections;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Security.Permissions;
using System.Diagnostics.CodeAnalysis;

namespace WeifenLuo.WinFormsUI.Docking
{
    public class FloatWindow : Form, INestedPanesContainer, IDockDragSource
    {
        private NestedPaneCollection m_nestedPanes;
        internal const int WM_CHECKDISPOSE = (int)(Win32.Msgs.WM_USER + 1);

        internal protected FloatWindow(DockPanel dockPanel, DockPane pane)
        {
            InternalConstruct(dockPanel, pane, false, Rectangle.Empty);
        }

        internal protected FloatWindow(DockPanel dockPanel, DockPane pane, Rectangle bounds)
        {
            InternalConstruct(dockPanel, pane, true, bounds);
        }

        private void InternalConstruct(DockPanel dockPanel, DockPane pane, bool boundsSpecified, Rectangle bounds)
        {
            if (dockPanel == null)
                throw(new ArgumentNullException(Strings.FloatWindow_Constructor_NullDockPanel));

            m_nestedPanes = new NestedPaneCollection(this);

            FormBorderStyle = FormBorderStyle.SizableToolWindow;
            ShowInTaskbar = false;
            if (dockPanel.RightToLeft != RightToLeft)
                RightToLeft = dockPanel.RightToLeft;
            if (RightToLeftLayout != dockPanel.RightToLeftLayout)
                RightToLeftLayout = dockPanel.RightToLeftLayout;
            
            SuspendLayout();
            if (boundsSpecified)
            {
                Bounds = bounds;
                StartPosition = FormStartPosition.Manual;
            }
            else
            {
                StartPosition = FormStartPosition.WindowsDefaultLocation;
                Size = dockPanel.DefaultFloatWindowSize;
            }

            m_dockPanel = dockPanel;
            Owner = DockPanel.FindForm();
            DockPanel.AddFloatWindow(this);
            if (pane != null)
                pane.FloatWindow = this;

            ResumeLayout();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (DockPanel != null)
                    DockPanel.RemoveFloatWindow(this);
                m_dockPanel = null;
            }
            base.Dispose(disposing);
        }

        private bool m_allowEndUserDocking = true;
        public bool AllowEndUserDocking
        {
            get    {    return m_allowEndUserDocking;    }
            set    {    m_allowEndUserDocking = value;    }
        }

        private bool m_doubleClickTitleBarToDock = false;
        public bool DoubleClickTitleBarToDock
        {
            get { return m_doubleClickTitleBarToDock; }
            set { m_doubleClickTitleBarToDock = value; }
        }

        public NestedPaneCollection NestedPanes
        {
            get    {    return m_nestedPanes;    }
        }

        public VisibleNestedPaneCollection VisibleNestedPanes
        {
            get    {    return NestedPanes.VisibleNestedPanes;    }
        }

        private DockPanel m_dockPanel;
        public DockPanel DockPanel
        {
            get    {    return m_dockPanel;    }
        }

        public DockState DockState
        {
            get    {    return DockState.Float;    }
        }
    
        public bool IsFloat
        {
            get    {    return DockState == DockState.Float;    }
        }

        internal bool IsDockStateValid(DockState dockState)
        {
            foreach (DockPane pane in NestedPanes)
                foreach (IDockContent content in pane.Contents)
                    if (!DockHelper.IsDockStateValid(dockState, content.DockHandler.DockAreas))
                        return false;

            return true;
        }

        protected override void OnActivated(EventArgs e)
        {
            DockPanel.FloatWindows.BringWindowToFront(this);
            base.OnActivated (e);
            // Propagate the Activated event to the visible panes content objects
            foreach (DockPane pane in VisibleNestedPanes)
                foreach (IDockContent content in pane.Contents)
                    content.OnActivated(e);
        }

        protected override void OnDeactivate(EventArgs e)
        {
            base.OnDeactivate(e);
            // Propagate the Deactivate event to the visible panes content objects
            foreach (DockPane pane in VisibleNestedPanes)
                foreach (IDockContent content in pane.Contents)
                    content.OnDeactivate(e);
        }

        protected override void OnLayout(LayoutEventArgs levent)
        {
            VisibleNestedPanes.Refresh();
            RefreshChanges();
            Visible = (VisibleNestedPanes.Count > 0);
            SetText();

            base.OnLayout(levent);
        }


        [SuppressMessage("Microsoft.Globalization", "CA1303:DoNotPassLiteralsAsLocalizedParameters", MessageId = "System.Windows.Forms.Control.set_Text(System.String)")]
        internal void SetText()
        {
            DockPane theOnlyPane = (VisibleNestedPanes.Count == 1) ? VisibleNestedPanes[0] : null;

            if (theOnlyPane == null || theOnlyPane.ActiveContent == null)
            {
                Text = " ";    // use " " instead of string.Empty because the whole title bar will disappear when ControlBox is set to false.
                Icon = null;
            }
            else
            {
                Text = theOnlyPane.ActiveContent.DockHandler.TabText;
                Icon = theOnlyPane.ActiveContent.DockHandler.Icon;
            }
        }

        protected override void SetBoundsCore(int x, int y, int width, int height, BoundsSpecified specified)
        {
            Rectangle rectWorkArea = SystemInformation.VirtualScreen;

            if (y + height > rectWorkArea.Bottom)
                y -= (y + height) - rectWorkArea.Bottom;

            if (y < rectWorkArea.Top)
                y += rectWorkArea.Top - y;

            base.SetBoundsCore (x, y, width, height, specified);
        }

        [SecurityPermission(SecurityAction.LinkDemand, Flags = SecurityPermissionFlag.UnmanagedCode)]
        protected override void WndProc(ref Message m)
        {
            switch (m.Msg)
            {
                case (int)Win32.Msgs.WM_NCLBUTTONDOWN:
                    {
                        if (IsDisposed)
                            return;

                        uint result = Win32Helper.IsRunningOnMono ? 0 : NativeMethods.SendMessage(this.Handle, (int)Win32.Msgs.WM_NCHITTEST, 0, (uint)m.LParam);
                        if (result == 2 && DockPanel.AllowEndUserDocking && this.AllowEndUserDocking)    // HITTEST_CAPTION
                        {
                            Activate();
                            m_dockPanel.BeginDrag(this);
                        }
                        else
                            base.WndProc(ref m);

                        return;
                    }
                case (int)Win32.Msgs.WM_NCRBUTTONDOWN:
                    {
                        uint result = Win32Helper.IsRunningOnMono ? 0 : NativeMethods.SendMessage(this.Handle, (int)Win32.Msgs.WM_NCHITTEST, 0, (uint)m.LParam);
                        if (result == 2)    // HITTEST_CAPTION
                        {
                            DockPane theOnlyPane = (VisibleNestedPanes.Count == 1) ? VisibleNestedPanes[0] : null;
                            if (theOnlyPane != null && theOnlyPane.ActiveContent != null)
                            {
                                theOnlyPane.ShowTabPageContextMenu(this, PointToClient(Control.MousePosition));
                                return;
                            }
                        }

                        base.WndProc(ref m);
                        return;
                    }
                case (int)Win32.Msgs.WM_CLOSE:
                    if (NestedPanes.Count == 0)
                    {
                        base.WndProc(ref m);
                        return;
                    }
                    for (int i = NestedPanes.Count - 1; i >= 0; i--)
                    {
                        DockContentCollection contents = NestedPanes[i].Contents;
                        for (int j = contents.Count - 1; j >= 0; j--)
                        {
                            IDockContent content = contents[j];
                            if (content.DockHandler.DockState != DockState.Float)
                                continue;

                            if (!content.DockHandler.CloseButton)
                                continue;

                            if (content.DockHandler.HideOnClose)
                                content.DockHandler.Hide();
                            else
                                content.DockHandler.Close();
                        }
                    }
                    return;
                case (int)Win32.Msgs.WM_NCLBUTTONDBLCLK:
                    {
                        uint result = !DoubleClickTitleBarToDock || Win32Helper.IsRunningOnMono 
                            ? 0
                            : NativeMethods.SendMessage(this.Handle, (int)Win32.Msgs.WM_NCHITTEST, 0, (uint)m.LParam);

                        if (result != 2)    // HITTEST_CAPTION
                        {
                            base.WndProc(ref m);
                            return;
                        }

                        DockPanel.SuspendLayout(true);

                        // Restore to panel
                        foreach (DockPane pane in NestedPanes)
                        {
                            if (pane.DockState != DockState.Float)
                                continue;
                            pane.RestoreToPanel();
                        }


                        DockPanel.ResumeLayout(true, true);
                        return;
                    }
                case WM_CHECKDISPOSE:
                    if (NestedPanes.Count == 0)
                        Dispose();
                    return;
            }

            base.WndProc(ref m);
        }

        internal void RefreshChanges()
        {
            if (IsDisposed)
                return;

            if (VisibleNestedPanes.Count == 0)
            {
                ControlBox = true;
                return;
            }

            for (int i=VisibleNestedPanes.Count - 1; i>=0; i--)
            {
                DockContentCollection contents = VisibleNestedPanes[i].Contents;
                for (int j=contents.Count - 1; j>=0; j--)
                {
                    IDockContent content = contents[j];
                    if (content.DockHandler.DockState != DockState.Float)
                        continue;

                    if (content.DockHandler.CloseButton && content.DockHandler.CloseButtonVisible)
                    {
                        ControlBox = true;
                        return;
                    }
                }
            }
            //Only if there is a ControlBox do we turn it off
            //old code caused a flash of the window.
            if (ControlBox)
                ControlBox = false;
        }

        public virtual Rectangle DisplayingRectangle
        {
            get    {    return ClientRectangle;    }
        }

        internal void TestDrop(IDockDragSource dragSource, DockOutlineBase dockOutline)
        {
            if (VisibleNestedPanes.Count == 1)
            {
                DockPane pane = VisibleNestedPanes[0];
                if (!dragSource.CanDockTo(pane))
                    return;

                Point ptMouse = Control.MousePosition;
                uint lParam = Win32Helper.MakeLong(ptMouse.X, ptMouse.Y);
                if (!Win32Helper.IsRunningOnMono)
                {
                    if (NativeMethods.SendMessage(Handle, (int)Win32.Msgs.WM_NCHITTEST, 0, lParam) == (uint)Win32.HitTest.HTCAPTION)
                    {
                        dockOutline.Show(VisibleNestedPanes[0], -1);
                    }
                }
            }
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

            if (pane.FloatWindow == this)
                return false;

            return true;
        }

        private int m_preDragExStyle;

        Rectangle IDockDragSource.BeginDrag(Point ptMouse)
        {
            m_preDragExStyle = NativeMethods.GetWindowLong(this.Handle, (int)Win32.GetWindowLongIndex.GWL_EXSTYLE);
            NativeMethods.SetWindowLong(this.Handle, 
                                        (int)Win32.GetWindowLongIndex.GWL_EXSTYLE,
                                        m_preDragExStyle | (int)(Win32.WindowExStyles.WS_EX_TRANSPARENT | Win32.WindowExStyles.WS_EX_LAYERED) );
            return Bounds;
        }

        void IDockDragSource.EndDrag()
        {
            NativeMethods.SetWindowLong(this.Handle, (int)Win32.GetWindowLongIndex.GWL_EXSTYLE, m_preDragExStyle);
            
            Invalidate(true);
            NativeMethods.SendMessage(this.Handle, (int)Win32.Msgs.WM_NCPAINT, 1, 0);
        }

        public  void FloatAt(Rectangle floatWindowBounds)
        {
            Bounds = floatWindowBounds;
        }

        public void DockTo(DockPane pane, DockStyle dockStyle, int contentIndex)
        {
            if (dockStyle == DockStyle.Fill)
            {
                for (int i = NestedPanes.Count - 1; i >= 0; i--)
                {
                    DockPane paneFrom = NestedPanes[i];
                    for (int j = paneFrom.Contents.Count - 1; j >= 0; j--)
                    {
                        IDockContent c = paneFrom.Contents[j];
                        c.DockHandler.Pane = pane;
                        if (contentIndex != -1)
                            pane.SetContentIndex(c, contentIndex);
                        c.DockHandler.Activate();
                    }
                }
            }
            else
            {
                DockAlignment alignment = DockAlignment.Left;
                if (dockStyle == DockStyle.Left)
                    alignment = DockAlignment.Left;
                else if (dockStyle == DockStyle.Right)
                    alignment = DockAlignment.Right;
                else if (dockStyle == DockStyle.Top)
                    alignment = DockAlignment.Top;
                else if (dockStyle == DockStyle.Bottom)
                    alignment = DockAlignment.Bottom;

                MergeNestedPanes(VisibleNestedPanes, pane.NestedPanesContainer.NestedPanes, pane, alignment, 0.5);
            }
        }

        public void DockTo(DockPanel panel, DockStyle dockStyle)
        {
            if (panel != DockPanel)
                throw new ArgumentException(Strings.IDockDragSource_DockTo_InvalidPanel, "panel");

            NestedPaneCollection nestedPanesTo = null;

            if (dockStyle == DockStyle.Top)
                nestedPanesTo = DockPanel.DockWindows[DockState.DockTop].NestedPanes;
            else if (dockStyle == DockStyle.Bottom)
                nestedPanesTo = DockPanel.DockWindows[DockState.DockBottom].NestedPanes;
            else if (dockStyle == DockStyle.Left)
                nestedPanesTo = DockPanel.DockWindows[DockState.DockLeft].NestedPanes;
            else if (dockStyle == DockStyle.Right)
                nestedPanesTo = DockPanel.DockWindows[DockState.DockRight].NestedPanes;
            else if (dockStyle == DockStyle.Fill)
                nestedPanesTo = DockPanel.DockWindows[DockState.Document].NestedPanes;

            DockPane prevPane = null;
            for (int i = nestedPanesTo.Count - 1; i >= 0; i--)
                if (nestedPanesTo[i] != VisibleNestedPanes[0])
                    prevPane = nestedPanesTo[i];
            MergeNestedPanes(VisibleNestedPanes, nestedPanesTo, prevPane, DockAlignment.Left, 0.5);
        }

        private static void MergeNestedPanes(VisibleNestedPaneCollection nestedPanesFrom, NestedPaneCollection nestedPanesTo, DockPane prevPane, DockAlignment alignment, double proportion)
        {
            if (nestedPanesFrom.Count == 0)
                return;

            int count = nestedPanesFrom.Count;
            DockPane[] panes = new DockPane[count];
            DockPane[] prevPanes = new DockPane[count];
            DockAlignment[] alignments = new DockAlignment[count];
            double[] proportions = new double[count];

            for (int i = 0; i < count; i++)
            {
                panes[i] = nestedPanesFrom[i];
                prevPanes[i] = nestedPanesFrom[i].NestedDockingStatus.PreviousPane;
                alignments[i] = nestedPanesFrom[i].NestedDockingStatus.Alignment;
                proportions[i] = nestedPanesFrom[i].NestedDockingStatus.Proportion;
            }

            DockPane pane = panes[0].DockTo(nestedPanesTo.Container, prevPane, alignment, proportion);
            panes[0].DockState = nestedPanesTo.DockState;

            for (int i = 1; i < count; i++)
            {
                for (int j = i; j < count; j++)
                {
                    if (prevPanes[j] == panes[i - 1])
                        prevPanes[j] = pane;
                }
                pane = panes[i].DockTo(nestedPanesTo.Container, prevPanes[i], alignments[i], proportions[i]);
                panes[i].DockState = nestedPanesTo.DockState;
            }
        }

        #endregion
    }
}
