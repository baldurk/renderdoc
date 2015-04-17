using System;
using System.Windows.Forms;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Security.Permissions;

namespace WeifenLuo.WinFormsUI.Docking
{
    public abstract class DockPaneCaptionBase : Control
    {
        protected internal DockPaneCaptionBase(DockPane pane)
        {
            m_dockPane = pane;

            SetStyle(ControlStyles.OptimizedDoubleBuffer |
                ControlStyles.ResizeRedraw |
                ControlStyles.UserPaint |
                ControlStyles.AllPaintingInWmPaint, true);
            SetStyle(ControlStyles.Selectable, false);
        }

        private DockPane m_dockPane;
        protected DockPane DockPane
        {
            get    {    return m_dockPane;    }
        }

        protected DockPane.AppearanceStyle Appearance
        {
            get    {    return DockPane.Appearance;    }
        }

        protected bool HasTabPageContextMenu
        {
            get { return DockPane.HasTabPageContextMenu; }
        }

        protected void ShowTabPageContextMenu(Point position)
        {
            DockPane.ShowTabPageContextMenu(this, position);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);

            if (e.Button == MouseButtons.Right)
                ShowTabPageContextMenu(new Point(e.X, e.Y));
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);

            if (e.Button == MouseButtons.Left &&
                DockPane.DockPanel.AllowEndUserDocking &&
                DockPane.AllowDockDragAndDrop &&
                !DockHelper.IsDockStateAutoHide(DockPane.DockState) &&
                DockPane.ActiveContent != null)
                DockPane.DockPanel.BeginDrag(DockPane);
        }

        [SecurityPermission(SecurityAction.LinkDemand, Flags = SecurityPermissionFlag.UnmanagedCode)]         
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == (int)Win32.Msgs.WM_LBUTTONDBLCLK)
            {
                if (DockHelper.IsDockStateAutoHide(DockPane.DockState))
                {
                    DockPane.DockPanel.ActiveAutoHideContent = null;
                    return;
                }

                if (DockPane.IsFloat)
                    DockPane.RestoreToPanel();
                else
                    DockPane.Float();
            }
            base.WndProc(ref m);
        }

        internal void RefreshChanges()
        {
            if (IsDisposed)
                return;

            OnRefreshChanges();
        }

        protected virtual void OnRightToLeftLayoutChanged()
        {
        }

        protected virtual void OnRefreshChanges()
        {
        }

        protected internal abstract int MeasureHeight();
    }
}
