using System;
using System.Drawing;
using System.Windows.Forms;

namespace WeifenLuo.WinFormsUI.Docking
{
    internal abstract class DockOutlineBase
    {
        public DockOutlineBase()
        {
            Init();
        }

        private void Init()
        {
            SetValues(Rectangle.Empty, null, DockStyle.None, -1);
            SaveOldValues();
        }

        private Rectangle m_oldFloatWindowBounds;
        protected Rectangle OldFloatWindowBounds
        {
            get { return m_oldFloatWindowBounds; }
        }

        private Control m_oldDockTo;
        protected Control OldDockTo
        {
            get { return m_oldDockTo; }
        }

        private DockStyle m_oldDock;
        protected DockStyle OldDock
        {
            get { return m_oldDock; }
        }

        private int m_oldContentIndex;
        protected int OldContentIndex
        {
            get { return m_oldContentIndex; }
        }

        protected bool SameAsOldValue
        {
            get
            {
                return FloatWindowBounds == OldFloatWindowBounds &&
                    DockTo == OldDockTo &&
                    Dock == OldDock &&
                    ContentIndex == OldContentIndex;
            }
        }

        private Rectangle m_floatWindowBounds;
        public Rectangle FloatWindowBounds
        {
            get { return m_floatWindowBounds; }
        }

        private Control m_dockTo;
        public Control DockTo
        {
            get { return m_dockTo; }
        }

        private DockStyle m_dock;
        public DockStyle Dock
        {
            get { return m_dock; }
        }

        private int m_contentIndex;
        public int ContentIndex
        {
            get { return m_contentIndex; }
        }

        public bool FlagFullEdge
        {
            get { return m_contentIndex != 0; }
        }

        private bool m_flagTestDrop = false;
        public bool FlagTestDrop
        {
            get { return m_flagTestDrop; }
            set { m_flagTestDrop = value; }
        }

        private void SaveOldValues()
        {
            m_oldDockTo = m_dockTo;
            m_oldDock = m_dock;
            m_oldContentIndex = m_contentIndex;
            m_oldFloatWindowBounds = m_floatWindowBounds;
        }

        protected abstract void OnShow();

        protected abstract void OnClose();

        private void SetValues(Rectangle floatWindowBounds, Control dockTo, DockStyle dock, int contentIndex)
        {
            m_floatWindowBounds = floatWindowBounds;
            m_dockTo = dockTo;
            m_dock = dock;
            m_contentIndex = contentIndex;
            FlagTestDrop = true;
        }

        private void TestChange()
        {
            if (m_floatWindowBounds != m_oldFloatWindowBounds ||
                m_dockTo != m_oldDockTo ||
                m_dock != m_oldDock ||
                m_contentIndex != m_oldContentIndex)
                OnShow();
        }

        public void Show()
        {
            SaveOldValues();
            SetValues(Rectangle.Empty, null, DockStyle.None, -1);
            TestChange();
        }

        public void Show(DockPane pane, DockStyle dock)
        {
            SaveOldValues();
            SetValues(Rectangle.Empty, pane, dock, -1);
            TestChange();
        }

        public void Show(DockPane pane, int contentIndex)
        {
            SaveOldValues();
            SetValues(Rectangle.Empty, pane, DockStyle.Fill, contentIndex);
            TestChange();
        }

        public void Show(DockPanel dockPanel, DockStyle dock, bool fullPanelEdge)
        {
            SaveOldValues();
            SetValues(Rectangle.Empty, dockPanel, dock, fullPanelEdge ? -1 : 0);
            TestChange();
        }

        public void Show(Rectangle floatWindowBounds)
        {
            SaveOldValues();
            SetValues(floatWindowBounds, null, DockStyle.None, -1);
            TestChange();
        }

        public void Close()
        {
            OnClose();
        }
    }
}
