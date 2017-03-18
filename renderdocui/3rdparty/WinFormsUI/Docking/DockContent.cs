using System;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Diagnostics.CodeAnalysis;

namespace WeifenLuo.WinFormsUI.Docking
{
    public class DockContent : Form, IDockContent
    {
        public DockContent()
        {
            m_dockHandler = new DockContentHandler(this, new GetPersistStringCallback(GetPersistString));
            m_dockHandler.DockStateChanged += new EventHandler(DockHandler_DockStateChanged);
            //Suggested as a fix by bensty regarding form resize
            this.ParentChanged += new EventHandler(DockContent_ParentChanged);
        }

        //Suggested as a fix by bensty regarding form resize
        private void DockContent_ParentChanged(object Sender, EventArgs e)
        {
            if (this.Parent != null)
                this.Font = this.Parent.Font;
        }

        private DockContentHandler m_dockHandler = null;
        [Browsable(false)]
        public DockContentHandler DockHandler
        {
            get    {    return m_dockHandler;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_AllowEndUserDocking_Description")]
        [DefaultValue(true)]
        public bool AllowEndUserDocking
        {
            get    {    return DockHandler.AllowEndUserDocking;    }
            set    {    DockHandler.AllowEndUserDocking = value;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_DockAreas_Description")]
        [DefaultValue(DockAreas.DockLeft|DockAreas.DockRight|DockAreas.DockTop|DockAreas.DockBottom|DockAreas.Document|DockAreas.Float)]
        public DockAreas DockAreas
        {
            get    {    return DockHandler.DockAreas;    }
            set    {    DockHandler.DockAreas = value;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_AutoHidePortion_Description")]
        [DefaultValue(0.25)]
        public double AutoHidePortion
        {
            get    {    return DockHandler.AutoHidePortion;    }
            set    {    DockHandler.AutoHidePortion = value;    }
        }

        private string m_tabText = null;
        [Localizable(true)]
        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_TabText_Description")]
        [DefaultValue(null)]
        public string TabText
        {
            get { return m_tabText; }
            set { DockHandler.TabText = m_tabText = value; }
        }

        private bool ShouldSerializeTabText()
        {
            return (m_tabText != null);
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_CloseButton_Description")]
        [DefaultValue(true)]
        public bool CloseButton
        {
            get    {    return DockHandler.CloseButton;    }
            set    {    DockHandler.CloseButton = value;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_CloseButtonVisible_Description")]
        [DefaultValue(true)]
        public bool CloseButtonVisible
        {
            get { return DockHandler.CloseButtonVisible; }
            set { DockHandler.CloseButtonVisible = value; }
        }
        
        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockPanel DockPanel
        {
            get {    return DockHandler.DockPanel; }
            set    {    DockHandler.DockPanel = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockState DockState
        {
            get    {    return DockHandler.DockState;    }
            set    {    DockHandler.DockState = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockPane Pane
        {
            get {    return DockHandler.Pane; }
            set    {    DockHandler.Pane = value;        }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public bool IsHidden
        {
            get    {    return DockHandler.IsHidden;    }
            set    {    DockHandler.IsHidden = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockState VisibleState
        {
            get    {    return DockHandler.VisibleState;    }
            set    {    DockHandler.VisibleState = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public bool IsFloat
        {
            get    {    return DockHandler.IsFloat;    }
            set    {    DockHandler.IsFloat = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockPane PanelPane
        {
            get    {    return DockHandler.PanelPane;    }
            set    {    DockHandler.PanelPane = value;    }
        }

        [Browsable(false)]
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public DockPane FloatPane
        {
            get    {    return DockHandler.FloatPane;    }
            set    {    DockHandler.FloatPane = value;    }
        }

        [SuppressMessage("Microsoft.Design", "CA1024:UsePropertiesWhereAppropriate")]
        protected virtual string GetPersistString()
        {
            return GetType().ToString();
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_HideOnClose_Description")]
        [DefaultValue(false)]
        public bool HideOnClose
        {
            get    {    return DockHandler.HideOnClose;    }
            set    {    DockHandler.HideOnClose = value;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_ShowHint_Description")]
        [DefaultValue(DockState.Unknown)]
        public DockState ShowHint
        {
            get    {    return DockHandler.ShowHint;    }
            set    {    DockHandler.ShowHint = value;    }
        }

        [Browsable(false)]
        public bool IsActivated
        {
            get    {    return DockHandler.IsActivated;    }
        }

        public bool IsDockStateValid(DockState dockState)
        {
            return DockHandler.IsDockStateValid(dockState);
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_TabPageContextMenu_Description")]
        [DefaultValue(null)]
        public ContextMenu TabPageContextMenu
        {
            get    {    return DockHandler.TabPageContextMenu;    }
            set    {    DockHandler.TabPageContextMenu = value;    }
        }

        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockContent_TabPageContextMenuStrip_Description")]
        [DefaultValue(null)]
        public ContextMenuStrip TabPageContextMenuStrip
        {
            get { return DockHandler.TabPageContextMenuStrip; }
            set { DockHandler.TabPageContextMenuStrip = value; }
        }

        [Localizable(true)]
        [Category("Appearance")]
        [LocalizedDescription("DockContent_ToolTipText_Description")]
        [DefaultValue(null)]
        public string ToolTipText
        {
            get    {    return DockHandler.ToolTipText;    }
            set {    DockHandler.ToolTipText = value;    }
        }

        public new void Activate()
        {
            DockHandler.Activate();
        }

        public new void Hide()
        {
            DockHandler.Hide();
        }

        public new void Show()
        {
            DockHandler.Show();
        }

        public void Show(DockPanel dockPanel)
        {
            DockHandler.Show(dockPanel);
        }

        public void Show(DockPanel dockPanel, DockState dockState)
        {
            DockHandler.Show(dockPanel, dockState);
        }

        [SuppressMessage("Microsoft.Naming", "CA1720:AvoidTypeNamesInParameters")]
        public void Show(DockPanel dockPanel, Rectangle floatWindowBounds)
        {
            DockHandler.Show(dockPanel, floatWindowBounds);
        }

        public void Show(DockPane pane, IDockContent beforeContent)
        {
            DockHandler.Show(pane, beforeContent);
        }

        public void Show(DockPane previousPane, DockAlignment alignment, double proportion)
        {
            DockHandler.Show(previousPane, alignment, proportion);
        }

        [SuppressMessage("Microsoft.Naming", "CA1720:AvoidTypeNamesInParameters")]
        public void FloatAt(Rectangle floatWindowBounds)
        {
            DockHandler.FloatAt(floatWindowBounds);
        }

        public void DockTo(DockPane paneTo, DockStyle dockStyle, int contentIndex)
        {
            DockHandler.DockTo(paneTo, dockStyle, contentIndex);
        }

        public void DockTo(DockPanel panel, DockStyle dockStyle)
        {
            DockHandler.DockTo(panel, dockStyle);
        }

        #region IDockContent Members
        void IDockContent.OnActivated(EventArgs e)
        {
            this.OnActivated(e);
        }

        void IDockContent.OnDeactivate(EventArgs e)
        {
            this.OnDeactivate(e);
        }
        #endregion

        #region Events
        private void DockHandler_DockStateChanged(object sender, EventArgs e)
        {
            OnDockStateChanged(e);
        }

        private static readonly object DockStateChangedEvent = new object();
        [LocalizedCategory("Category_PropertyChanged")]
        [LocalizedDescription("Pane_DockStateChanged_Description")]
        public event EventHandler DockStateChanged
        {
            add    {    Events.AddHandler(DockStateChangedEvent, value);    }
            remove    {    Events.RemoveHandler(DockStateChangedEvent, value);    }
        }
        protected virtual void OnDockStateChanged(EventArgs e)
        {
            EventHandler handler = (EventHandler)Events[DockStateChangedEvent];
            if (handler != null)
                handler(this, e);
        }
        #endregion

        /// <summary>
        /// Overridden to avoid resize issues with nested controls
        /// </summary>
        /// <remarks>
        /// http://blogs.msdn.com/b/alejacma/archive/2008/11/20/controls-won-t-get-resized-once-the-nesting-hierarchy-of-windows-exceeds-a-certain-depth-x64.aspx
        /// http://support.microsoft.com/kb/953934
        /// </remarks>
        protected override void OnSizeChanged(EventArgs e)
        {
            if (DockPanel != null && DockPanel.SupportDeeplyNestedContent && IsHandleCreated)
            {
                BeginInvoke((MethodInvoker)delegate
                {
                    base.OnSizeChanged(e);
                });
            }
            else
            {
                base.OnSizeChanged(e);
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (Pane != null && Pane.DockPanel != null && Pane.DockPanel.CloseTabsToLeft &&
                Pane.ActiveContent == this && Pane.TabStripControl != null)
            {
                var tabs = Pane.TabStripControl.Tabs;

                if (tabs != null && tabs.Count > 1)
                {
                    for (int i = 0; i < tabs.Count; i++)
                    {
                        if (tabs[i].Content == this)
                        {
                            DockContent dc = null;

                            if (i > 0)
                            {
                                for (int j = i-1; j >= 0; j--)
                                {
                                    if (tabs[j].Content is DockContent)
                                    {
                                        dc = tabs[j].Content as DockContent;
                                        break;
                                    }
                                }
                            }

                            if(dc != null)
                                dc.Show();

                            break;
                        }
                    }
                }
            }

            base.OnFormClosing(e);
        }
    }
}
