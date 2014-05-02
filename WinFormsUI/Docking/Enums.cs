using System;
using System.ComponentModel;
using System.Windows.Forms;

namespace WeifenLuo.WinFormsUI.Docking
{
    [Flags]
    [Serializable]
    [Editor(typeof(DockAreasEditor), typeof(System.Drawing.Design.UITypeEditor))]
    public enum DockAreas
    {
        Float = 1,
        DockLeft = 2,
        DockRight = 4,
        DockTop = 8,
        DockBottom = 16,
        Document = 32
    }

    public enum DockState
    {
        Unknown = 0,
        Float = 1,
        DockTopAutoHide = 2,
        DockLeftAutoHide = 3,
        DockBottomAutoHide = 4,
        DockRightAutoHide = 5,
        Document = 6,
        DockTop = 7,
        DockLeft = 8,
        DockBottom = 9,
        DockRight = 10,
        Hidden = 11
    }

    public enum DockAlignment
    {
        Left,
        Right,
        Top,
        Bottom
    }

    public enum DocumentStyle
    {
        DockingMdi,
        DockingWindow,
        DockingSdi,
        SystemMdi,
    }

    /// <summary>
    /// The location to draw the DockPaneStrip for Document style windows.
    /// </summary>
    public enum DocumentTabStripLocation
    {
        Top,
        Bottom
    }
}
