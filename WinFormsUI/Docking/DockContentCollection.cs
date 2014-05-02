using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace WeifenLuo.WinFormsUI.Docking
{
    public class DockContentCollection : ReadOnlyCollection<IDockContent>
    {
        private static List<IDockContent> _emptyList = new List<IDockContent>(0);

        internal DockContentCollection()
            : base(new List<IDockContent>())
        {
        }

        internal DockContentCollection(DockPane pane)
            : base(_emptyList)
        {
            m_dockPane = pane;
        }

        private DockPane m_dockPane = null;
        private DockPane DockPane
        {
            get { return m_dockPane; }
        }

        public new IDockContent this[int index]
        {
            get
            {
                if (DockPane == null)
                    return Items[index] as IDockContent;
                else
                    return GetVisibleContent(index);
            }
        }

        internal int Add(IDockContent content)
        {
#if DEBUG
            if (DockPane != null)
                throw new InvalidOperationException();
#endif

            if (Contains(content))
                return IndexOf(content);

            Items.Add(content);
            return Count - 1;
        }

        internal void AddAt(IDockContent content, int index)
        {
#if DEBUG
            if (DockPane != null)
                throw new InvalidOperationException();
#endif

            if (index < 0 || index > Items.Count - 1)
                return;

            if (Contains(content))
                return;

            Items.Insert(index, content);
        }

        public new bool Contains(IDockContent content)
        {
            if (DockPane == null)
                return Items.Contains(content);
            else
                return (GetIndexOfVisibleContents(content) != -1);
        }

        public new int Count
        {
            get
            {
                if (DockPane == null)
                    return base.Count;
                else
                    return CountOfVisibleContents;
            }
        }

        public new int IndexOf(IDockContent content)
        {
            if (DockPane == null)
            {
                if (!Contains(content))
                    return -1;
                else
                    return Items.IndexOf(content);
            }
            else
                return GetIndexOfVisibleContents(content);
        }

        internal void Remove(IDockContent content)
        {
            if (DockPane != null)
                throw new InvalidOperationException();

            if (!Contains(content))
                return;

            Items.Remove(content);
        }

        private int CountOfVisibleContents
        {
            get
            {
#if DEBUG
                if (DockPane == null)
                    throw new InvalidOperationException();
#endif

                int count = 0;
                foreach (IDockContent content in DockPane.Contents)
                {
                    if (content.DockHandler.DockState == DockPane.DockState)
                        count++;
                }
                return count;
            }
        }

        private IDockContent GetVisibleContent(int index)
        {
#if DEBUG
            if (DockPane == null)
                throw new InvalidOperationException();
#endif

            int currentIndex = -1;
            foreach (IDockContent content in DockPane.Contents)
            {
                if (content.DockHandler.DockState == DockPane.DockState)
                    currentIndex++;

                if (currentIndex == index)
                    return content;
            }
            throw (new ArgumentOutOfRangeException());
        }

        private int GetIndexOfVisibleContents(IDockContent content)
        {
#if DEBUG
            if (DockPane == null)
                throw new InvalidOperationException();
#endif

            if (content == null)
                return -1;

            int index = -1;
            foreach (IDockContent c in DockPane.Contents)
            {
                if (c.DockHandler.DockState == DockPane.DockState)
                {
                    index++;

                    if (c == content)
                        return index;
                }
            }
            return -1;
        }
    }
}
