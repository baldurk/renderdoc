using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace WeifenLuo.WinFormsUI.Docking
{
    public class DockWindowCollection : ReadOnlyCollection<DockWindow>
    {
        internal DockWindowCollection(DockPanel dockPanel)
            : base(new List<DockWindow>())
        {
            Items.Add(new DockWindow(dockPanel, DockState.Document));
            Items.Add(new DockWindow(dockPanel, DockState.DockLeft));
            Items.Add(new DockWindow(dockPanel, DockState.DockRight));
            Items.Add(new DockWindow(dockPanel, DockState.DockTop));
            Items.Add(new DockWindow(dockPanel, DockState.DockBottom));
        }

        public DockWindow this [DockState dockState]
        {
            get
            {
                if (dockState == DockState.Document)
                    return Items[0];
                else if (dockState == DockState.DockLeft || dockState == DockState.DockLeftAutoHide)
                    return Items[1];
                else if (dockState == DockState.DockRight || dockState == DockState.DockRightAutoHide)
                    return Items[2];
                else if (dockState == DockState.DockTop || dockState == DockState.DockTopAutoHide)
                    return Items[3];
                else if (dockState == DockState.DockBottom || dockState == DockState.DockBottomAutoHide)
                    return Items[4];

                throw (new ArgumentOutOfRangeException());
            }
        }
    }
}
