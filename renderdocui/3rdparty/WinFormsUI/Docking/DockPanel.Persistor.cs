using System;
using System.ComponentModel;
using System.Windows.Forms;
using System.Drawing;
using WeifenLuo.WinFormsUI.Docking;
using System.IO;
using System.Text;
using System.Xml;
using System.Globalization;

namespace WeifenLuo.WinFormsUI.Docking
{
    partial class DockPanel
    {
        private static class Persistor
        {
            private const string ConfigFileVersion = "1.0";
            private static string[] CompatibleConfigFileVersions = new string[] { };

            private class DummyContent : DockContent
            {
            }

            private struct DockPanelStruct
            {
                private double m_dockLeftPortion;
                public double DockLeftPortion
                {
                    get { return m_dockLeftPortion; }
                    set { m_dockLeftPortion = value; }
                }

                private double m_dockRightPortion;
                public double DockRightPortion
                {
                    get { return m_dockRightPortion; }
                    set { m_dockRightPortion = value; }
                }

                private double m_dockTopPortion;
                public double DockTopPortion
                {
                    get { return m_dockTopPortion; }
                    set { m_dockTopPortion = value; }
                }

                private double m_dockBottomPortion;
                public double DockBottomPortion
                {
                    get { return m_dockBottomPortion; }
                    set { m_dockBottomPortion = value; }
                }

                private int m_indexActiveDocumentPane;
                public int IndexActiveDocumentPane
                {
                    get { return m_indexActiveDocumentPane; }
                    set { m_indexActiveDocumentPane = value; }
                }

                private int m_indexActivePane;
                public int IndexActivePane
                {
                    get { return m_indexActivePane; }
                    set { m_indexActivePane = value; }
                }
            }

            private struct ContentStruct
            {
                private string m_persistString;
                public string PersistString
                {
                    get { return m_persistString; }
                    set { m_persistString = value; }
                }

                private double m_autoHidePortion;
                public double AutoHidePortion
                {
                    get { return m_autoHidePortion; }
                    set { m_autoHidePortion = value; }
                }

                private bool m_isHidden;
                public bool IsHidden
                {
                    get { return m_isHidden; }
                    set { m_isHidden = value; }
                }

                private bool m_isFloat;
                public bool IsFloat
                {
                    get { return m_isFloat; }
                    set { m_isFloat = value; }
                }
            }

            private struct PaneStruct
            {
                private DockState m_dockState;
                public DockState DockState
                {
                    get { return m_dockState; }
                    set { m_dockState = value; }
                }

                private int m_indexActiveContent;
                public int IndexActiveContent
                {
                    get { return m_indexActiveContent; }
                    set { m_indexActiveContent = value; }
                }

                private int[] m_indexContents;
                public int[] IndexContents
                {
                    get { return m_indexContents; }
                    set { m_indexContents = value; }
                }

                private int m_zOrderIndex;
                public int ZOrderIndex
                {
                    get { return m_zOrderIndex; }
                    set { m_zOrderIndex = value; }
                }
            }

            private struct NestedPane
            {
                private int m_indexPane;
                public int IndexPane
                {
                    get { return m_indexPane; }
                    set { m_indexPane = value; }
                }

                private int m_indexPrevPane;
                public int IndexPrevPane
                {
                    get { return m_indexPrevPane; }
                    set { m_indexPrevPane = value; }
                }

                private DockAlignment m_alignment;
                public DockAlignment Alignment
                {
                    get { return m_alignment; }
                    set { m_alignment = value; }
                }

                private double m_proportion;
                public double Proportion
                {
                    get { return m_proportion; }
                    set { m_proportion = value; }
                }
            }

            private struct DockWindowStruct
            {
                private DockState m_dockState;
                public DockState DockState
                {
                    get { return m_dockState; }
                    set { m_dockState = value; }
                }

                private int m_zOrderIndex;
                public int ZOrderIndex
                {
                    get { return m_zOrderIndex; }
                    set { m_zOrderIndex = value; }
                }

                private NestedPane[] m_nestedPanes;
                public NestedPane[] NestedPanes
                {
                    get { return m_nestedPanes; }
                    set { m_nestedPanes = value; }
                }
            }

            private struct FloatWindowStruct
            {
                private Rectangle m_bounds;
                public Rectangle Bounds
                {
                    get { return m_bounds; }
                    set { m_bounds = value; }
                }

                private int m_zOrderIndex;
                public int ZOrderIndex
                {
                    get { return m_zOrderIndex; }
                    set { m_zOrderIndex = value; }
                }

                private NestedPane[] m_nestedPanes;
                public NestedPane[] NestedPanes
                {
                    get { return m_nestedPanes; }
                    set { m_nestedPanes = value; }
                }

                private bool m_maximized;
                public bool Maximized
                {
                    get { return m_maximized; }
                    set { m_maximized = value; }
                }
            }

            public static void SaveAsXml(DockPanel dockPanel, string fileName, string userString)
            {
                SaveAsXml(dockPanel, fileName, userString, Encoding.Unicode);
            }

            public static void SaveAsXml(DockPanel dockPanel, string fileName, string userString, Encoding encoding)
            {
                FileStream fs = new FileStream(fileName, FileMode.Create);
                try
                {
                    SaveAsXml(dockPanel, fs, userString, encoding);
                }
                finally
                {
                    fs.Close();
                }
            }

            public static void SaveAsXml(DockPanel dockPanel, Stream stream, string userString, Encoding encoding)
            {
                SaveAsXml(dockPanel, stream, userString, encoding, false);
            }

            public static void SaveAsXml(DockPanel dockPanel, Stream stream, string userString, Encoding encoding, bool upstream)
            {
                XmlTextWriter xmlOut = new XmlTextWriter(stream, encoding);

                // Use indenting for readability
                xmlOut.Formatting = Formatting.Indented;

                if (!upstream)
                    xmlOut.WriteStartDocument();

                // Always begin file with identification and warning
                xmlOut.WriteComment(Strings.DockPanel_Persistor_XmlFileComment1);
                xmlOut.WriteComment(Strings.DockPanel_Persistor_XmlFileComment2);

                // Associate a version number with the root element so that future version of the code
                // will be able to be backwards compatible or at least recognise out of date versions
                xmlOut.WriteStartElement("DockPanel");
                xmlOut.WriteAttributeString("FormatVersion", ConfigFileVersion);
                xmlOut.WriteAttributeString("DockLeftPortion", dockPanel.DockLeftPortion.ToString(CultureInfo.InvariantCulture));
                xmlOut.WriteAttributeString("DockRightPortion", dockPanel.DockRightPortion.ToString(CultureInfo.InvariantCulture));
                xmlOut.WriteAttributeString("DockTopPortion", dockPanel.DockTopPortion.ToString(CultureInfo.InvariantCulture));
                xmlOut.WriteAttributeString("DockBottomPortion", dockPanel.DockBottomPortion.ToString(CultureInfo.InvariantCulture));
                xmlOut.WriteAttributeString("UserString", userString);

                if (!Win32Helper.IsRunningOnMono)
                {
                    xmlOut.WriteAttributeString("ActiveDocumentPane", dockPanel.Panes.IndexOf(dockPanel.ActiveDocumentPane).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("ActivePane", dockPanel.Panes.IndexOf(dockPanel.ActivePane).ToString(CultureInfo.InvariantCulture));
                }                

                // Contents
                xmlOut.WriteStartElement("Contents");
                xmlOut.WriteAttributeString("Count", dockPanel.Contents.Count.ToString(CultureInfo.InvariantCulture));
                foreach (IDockContent content in dockPanel.Contents)
                {
                    xmlOut.WriteStartElement("Content");
                    xmlOut.WriteAttributeString("ID", dockPanel.Contents.IndexOf(content).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("PersistString", content.DockHandler.PersistString);
                    xmlOut.WriteAttributeString("AutoHidePortion", content.DockHandler.AutoHidePortion.ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("IsHidden", content.DockHandler.IsHidden.ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("IsFloat", content.DockHandler.IsFloat.ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteEndElement();
                }
                xmlOut.WriteEndElement();

                // Panes
                xmlOut.WriteStartElement("Panes");
                xmlOut.WriteAttributeString("Count", dockPanel.Panes.Count.ToString(CultureInfo.InvariantCulture));
                foreach (DockPane pane in dockPanel.Panes)
                {
                    xmlOut.WriteStartElement("Pane");
                    xmlOut.WriteAttributeString("ID", dockPanel.Panes.IndexOf(pane).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("DockState", pane.DockState.ToString());
                    xmlOut.WriteAttributeString("ActiveContent", dockPanel.Contents.IndexOf(pane.ActiveContent).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteStartElement("Contents");
                    xmlOut.WriteAttributeString("Count", pane.Contents.Count.ToString(CultureInfo.InvariantCulture));
                    foreach (IDockContent content in pane.Contents)
                    {
                        xmlOut.WriteStartElement("Content");
                        xmlOut.WriteAttributeString("ID", pane.Contents.IndexOf(content).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteAttributeString("RefID", dockPanel.Contents.IndexOf(content).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteEndElement();
                    }
                    xmlOut.WriteEndElement();
                    xmlOut.WriteEndElement();
                }
                xmlOut.WriteEndElement();

                // DockWindows
                xmlOut.WriteStartElement("DockWindows");
                int dockWindowId = 0;
                foreach (DockWindow dw in dockPanel.DockWindows)
                {
                    xmlOut.WriteStartElement("DockWindow");
                    xmlOut.WriteAttributeString("ID", dockWindowId.ToString(CultureInfo.InvariantCulture));
                    dockWindowId++;
                    xmlOut.WriteAttributeString("DockState", dw.DockState.ToString());
                    xmlOut.WriteAttributeString("ZOrderIndex", dockPanel.Controls.IndexOf(dw).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteStartElement("NestedPanes");
                    xmlOut.WriteAttributeString("Count", dw.NestedPanes.Count.ToString(CultureInfo.InvariantCulture));
                    foreach (DockPane pane in dw.NestedPanes)
                    {
                        xmlOut.WriteStartElement("Pane");
                        xmlOut.WriteAttributeString("ID", dw.NestedPanes.IndexOf(pane).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteAttributeString("RefID", dockPanel.Panes.IndexOf(pane).ToString(CultureInfo.InvariantCulture));
                        NestedDockingStatus status = pane.NestedDockingStatus;
                        xmlOut.WriteAttributeString("PrevPane", dockPanel.Panes.IndexOf(status.PreviousPane).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteAttributeString("Alignment", status.Alignment.ToString());
                        xmlOut.WriteAttributeString("Proportion", status.Proportion.ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteEndElement();
                    }
                    xmlOut.WriteEndElement();
                    xmlOut.WriteEndElement();
                }
                xmlOut.WriteEndElement();

                // FloatWindows
                RectangleConverter rectConverter = new RectangleConverter();
                xmlOut.WriteStartElement("FloatWindows");
                xmlOut.WriteAttributeString("Count", dockPanel.FloatWindows.Count.ToString(CultureInfo.InvariantCulture));
                foreach (FloatWindow fw in dockPanel.FloatWindows)
                {
                    xmlOut.WriteStartElement("FloatWindow");
                    xmlOut.WriteAttributeString("ID", dockPanel.FloatWindows.IndexOf(fw).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("Bounds", rectConverter.ConvertToInvariantString(fw.Bounds));
                    xmlOut.WriteAttributeString("ZOrderIndex", fw.DockPanel.FloatWindows.IndexOf(fw).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteAttributeString("Maximized", (fw.WindowState == FormWindowState.Maximized).ToString(CultureInfo.InvariantCulture));
                    xmlOut.WriteStartElement("NestedPanes");
                    xmlOut.WriteAttributeString("Count", fw.NestedPanes.Count.ToString(CultureInfo.InvariantCulture));
                    foreach (DockPane pane in fw.NestedPanes)
                    {
                        xmlOut.WriteStartElement("Pane");
                        xmlOut.WriteAttributeString("ID", fw.NestedPanes.IndexOf(pane).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteAttributeString("RefID", dockPanel.Panes.IndexOf(pane).ToString(CultureInfo.InvariantCulture));
                        NestedDockingStatus status = pane.NestedDockingStatus;
                        xmlOut.WriteAttributeString("PrevPane", dockPanel.Panes.IndexOf(status.PreviousPane).ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteAttributeString("Alignment", status.Alignment.ToString());
                        xmlOut.WriteAttributeString("Proportion", status.Proportion.ToString(CultureInfo.InvariantCulture));
                        xmlOut.WriteEndElement();
                    }
                    xmlOut.WriteEndElement();
                    xmlOut.WriteEndElement();
                }
                xmlOut.WriteEndElement();    //    </FloatWindows>

                xmlOut.WriteEndElement();

                if (!upstream)
                {
                    xmlOut.WriteEndDocument();
                    xmlOut.Close();
                }
                else
                    xmlOut.Flush();
            }

            public static void LoadFromXml(DockPanel dockPanel, string fileName, DeserializeDockContent deserializeContent)
            {
                FileStream fs = new FileStream(fileName, FileMode.Open, FileAccess.Read);
                try
                {
                    LoadFromXml(dockPanel, fs, deserializeContent);
                }
                finally
                {
                    fs.Close();
                }
            }

            public static void LoadFromXml(DockPanel dockPanel, Stream stream, DeserializeDockContent deserializeContent)
            {
                LoadFromXml(dockPanel, stream, deserializeContent, true);
            }

            private static ContentStruct[] LoadContents(XmlTextReader xmlIn)
            {
                int countOfContents = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                ContentStruct[] contents = new ContentStruct[countOfContents];
                MoveToNextElement(xmlIn);
                for (int i = 0; i < countOfContents; i++)
                {
                    int id = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                    if (xmlIn.Name != "Content" || id != i)
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);

                    contents[i].PersistString = xmlIn.GetAttribute("PersistString");
                    contents[i].AutoHidePortion = Convert.ToDouble(xmlIn.GetAttribute("AutoHidePortion"), CultureInfo.InvariantCulture);
                    contents[i].IsHidden = Convert.ToBoolean(xmlIn.GetAttribute("IsHidden"), CultureInfo.InvariantCulture);
                    contents[i].IsFloat = Convert.ToBoolean(xmlIn.GetAttribute("IsFloat"), CultureInfo.InvariantCulture);
                    MoveToNextElement(xmlIn);
                }

                return contents;
            }

            private static PaneStruct[] LoadPanes(XmlTextReader xmlIn)
            {
                EnumConverter dockStateConverter = new EnumConverter(typeof(DockState));
                int countOfPanes = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                PaneStruct[] panes = new PaneStruct[countOfPanes];
                MoveToNextElement(xmlIn);
                for (int i = 0; i < countOfPanes; i++)
                {
                    int id = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                    if (xmlIn.Name != "Pane" || id != i)
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);

                    panes[i].DockState = (DockState)dockStateConverter.ConvertFrom(xmlIn.GetAttribute("DockState"));
                    panes[i].IndexActiveContent = Convert.ToInt32(xmlIn.GetAttribute("ActiveContent"), CultureInfo.InvariantCulture);
                    panes[i].ZOrderIndex = -1;

                    MoveToNextElement(xmlIn);
                    if (xmlIn.Name != "Contents")
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                    int countOfPaneContents = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                    panes[i].IndexContents = new int[countOfPaneContents];
                    MoveToNextElement(xmlIn);
                    for (int j = 0; j < countOfPaneContents; j++)
                    {
                        int id2 = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                        if (xmlIn.Name != "Content" || id2 != j)
                            throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);

                        panes[i].IndexContents[j] = Convert.ToInt32(xmlIn.GetAttribute("RefID"), CultureInfo.InvariantCulture);
                        MoveToNextElement(xmlIn);
                    }
                }

                return panes;
            }

            private static DockWindowStruct[] LoadDockWindows(XmlTextReader xmlIn, DockPanel dockPanel)
            {
                EnumConverter dockStateConverter = new EnumConverter(typeof(DockState));
                EnumConverter dockAlignmentConverter = new EnumConverter(typeof(DockAlignment));
                int countOfDockWindows = dockPanel.DockWindows.Count;
                DockWindowStruct[] dockWindows = new DockWindowStruct[countOfDockWindows];
                MoveToNextElement(xmlIn);
                for (int i = 0; i < countOfDockWindows; i++)
                {
                    int id = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                    if (xmlIn.Name != "DockWindow" || id != i)
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);

                    dockWindows[i].DockState = (DockState)dockStateConverter.ConvertFrom(xmlIn.GetAttribute("DockState"));
                    dockWindows[i].ZOrderIndex = Convert.ToInt32(xmlIn.GetAttribute("ZOrderIndex"), CultureInfo.InvariantCulture);
                    MoveToNextElement(xmlIn);
                    if (xmlIn.Name != "DockList" && xmlIn.Name != "NestedPanes")
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                    int countOfNestedPanes = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                    dockWindows[i].NestedPanes = new NestedPane[countOfNestedPanes];
                    MoveToNextElement(xmlIn);
                    for (int j = 0; j < countOfNestedPanes; j++)
                    {
                        int id2 = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                        if (xmlIn.Name != "Pane" || id2 != j)
                            throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                        dockWindows[i].NestedPanes[j].IndexPane = Convert.ToInt32(xmlIn.GetAttribute("RefID"), CultureInfo.InvariantCulture);
                        dockWindows[i].NestedPanes[j].IndexPrevPane = Convert.ToInt32(xmlIn.GetAttribute("PrevPane"), CultureInfo.InvariantCulture);
                        dockWindows[i].NestedPanes[j].Alignment = (DockAlignment)dockAlignmentConverter.ConvertFrom(xmlIn.GetAttribute("Alignment"));
                        dockWindows[i].NestedPanes[j].Proportion = Convert.ToDouble(xmlIn.GetAttribute("Proportion"), CultureInfo.InvariantCulture);
                        MoveToNextElement(xmlIn);
                    }
                }

                return dockWindows;
            }

            private static FloatWindowStruct[] LoadFloatWindows(XmlTextReader xmlIn)
            {
                EnumConverter dockAlignmentConverter = new EnumConverter(typeof(DockAlignment));
                RectangleConverter rectConverter = new RectangleConverter();
                int countOfFloatWindows = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                FloatWindowStruct[] floatWindows = new FloatWindowStruct[countOfFloatWindows];
                MoveToNextElement(xmlIn);
                for (int i = 0; i < countOfFloatWindows; i++)
                {
                    int id = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                    if (xmlIn.Name != "FloatWindow" || id != i)
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);

                    floatWindows[i].Bounds = (Rectangle)rectConverter.ConvertFromInvariantString(xmlIn.GetAttribute("Bounds"));
                    floatWindows[i].Maximized = Convert.ToBoolean(xmlIn.GetAttribute("Maximized"), CultureInfo.InvariantCulture);
                    floatWindows[i].ZOrderIndex = Convert.ToInt32(xmlIn.GetAttribute("ZOrderIndex"), CultureInfo.InvariantCulture);
                    MoveToNextElement(xmlIn);
                    if (xmlIn.Name != "DockList" && xmlIn.Name != "NestedPanes")
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                    int countOfNestedPanes = Convert.ToInt32(xmlIn.GetAttribute("Count"), CultureInfo.InvariantCulture);
                    floatWindows[i].NestedPanes = new NestedPane[countOfNestedPanes];
                    MoveToNextElement(xmlIn);
                    for (int j = 0; j < countOfNestedPanes; j++)
                    {
                        int id2 = Convert.ToInt32(xmlIn.GetAttribute("ID"), CultureInfo.InvariantCulture);
                        if (xmlIn.Name != "Pane" || id2 != j)
                            throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                        floatWindows[i].NestedPanes[j].IndexPane = Convert.ToInt32(xmlIn.GetAttribute("RefID"), CultureInfo.InvariantCulture);
                        floatWindows[i].NestedPanes[j].IndexPrevPane = Convert.ToInt32(xmlIn.GetAttribute("PrevPane"), CultureInfo.InvariantCulture);
                        floatWindows[i].NestedPanes[j].Alignment = (DockAlignment)dockAlignmentConverter.ConvertFrom(xmlIn.GetAttribute("Alignment"));
                        floatWindows[i].NestedPanes[j].Proportion = Convert.ToDouble(xmlIn.GetAttribute("Proportion"), CultureInfo.InvariantCulture);
                        MoveToNextElement(xmlIn);
                    }
                }

                return floatWindows;
            }

            public static void LoadFromXml(DockPanel dockPanel, Stream stream, DeserializeDockContent deserializeContent, bool closeStream)
            {

                if (dockPanel.Contents.Count != 0)
                    throw new InvalidOperationException(Strings.DockPanel_LoadFromXml_AlreadyInitialized);

                XmlTextReader xmlIn = new XmlTextReader(stream);
                xmlIn.WhitespaceHandling = WhitespaceHandling.None;
                xmlIn.MoveToContent();

                while (!xmlIn.Name.Equals("DockPanel"))
                {
                    if (!MoveToNextElement(xmlIn))
                        throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                }

                string formatVersion = xmlIn.GetAttribute("FormatVersion");
                if (!IsFormatVersionValid(formatVersion))
                    throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidFormatVersion);

                string userString = xmlIn.GetAttribute("UserString");
                deserializeContent(userString);

                DockPanelStruct dockPanelStruct = new DockPanelStruct();
                dockPanelStruct.DockLeftPortion = Convert.ToDouble(xmlIn.GetAttribute("DockLeftPortion"), CultureInfo.InvariantCulture);
                dockPanelStruct.DockRightPortion = Convert.ToDouble(xmlIn.GetAttribute("DockRightPortion"), CultureInfo.InvariantCulture);
                dockPanelStruct.DockTopPortion = Convert.ToDouble(xmlIn.GetAttribute("DockTopPortion"), CultureInfo.InvariantCulture);
                dockPanelStruct.DockBottomPortion = Convert.ToDouble(xmlIn.GetAttribute("DockBottomPortion"), CultureInfo.InvariantCulture);
                dockPanelStruct.IndexActiveDocumentPane = Convert.ToInt32(xmlIn.GetAttribute("ActiveDocumentPane"), CultureInfo.InvariantCulture);
                dockPanelStruct.IndexActivePane = Convert.ToInt32(xmlIn.GetAttribute("ActivePane"), CultureInfo.InvariantCulture);

                // Load Contents
                MoveToNextElement(xmlIn);
                if (xmlIn.Name != "Contents")
                    throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                ContentStruct[] contents = LoadContents(xmlIn);

                // Load Panes
                if (xmlIn.Name != "Panes")
                    throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                PaneStruct[] panes = LoadPanes(xmlIn);

                // Load DockWindows
                if (xmlIn.Name != "DockWindows")
                    throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                DockWindowStruct[] dockWindows = LoadDockWindows(xmlIn, dockPanel);

                // Load FloatWindows
                if (xmlIn.Name != "FloatWindows")
                    throw new ArgumentException(Strings.DockPanel_LoadFromXml_InvalidXmlFormat);
                FloatWindowStruct[] floatWindows = LoadFloatWindows(xmlIn);

                if (closeStream)
                    xmlIn.Close();

                dockPanel.SuspendLayout(true);

                dockPanel.DockLeftPortion = dockPanelStruct.DockLeftPortion;
                dockPanel.DockRightPortion = dockPanelStruct.DockRightPortion;
                dockPanel.DockTopPortion = dockPanelStruct.DockTopPortion;
                dockPanel.DockBottomPortion = dockPanelStruct.DockBottomPortion;

                // Set DockWindow ZOrders
                int prevMaxDockWindowZOrder = int.MaxValue;
                for (int i = 0; i < dockWindows.Length; i++)
                {
                    int maxDockWindowZOrder = -1;
                    int index = -1;
                    for (int j = 0; j < dockWindows.Length; j++)
                    {
                        if (dockWindows[j].ZOrderIndex > maxDockWindowZOrder && dockWindows[j].ZOrderIndex < prevMaxDockWindowZOrder)
                        {
                            maxDockWindowZOrder = dockWindows[j].ZOrderIndex;
                            index = j;
                        }
                    }

                    dockPanel.DockWindows[dockWindows[index].DockState].BringToFront();
                    prevMaxDockWindowZOrder = maxDockWindowZOrder;
                }

                // Create Contents
                for (int i = 0; i < contents.Length; i++)
                {
                    IDockContent content = deserializeContent(contents[i].PersistString);
                    if (content == null || dockPanel.Contents.IndexOf(content) >= 0)
                        content = new DummyContent();
                    content.DockHandler.DockPanel = dockPanel;
                    content.DockHandler.AutoHidePortion = contents[i].AutoHidePortion;
                    content.DockHandler.IsHidden = true;
                    content.DockHandler.IsFloat = contents[i].IsFloat;
                }

                // Create panes
                for (int i = 0; i < panes.Length; i++)
                {
                    DockPane pane = null;
                    for (int j = 0; j < panes[i].IndexContents.Length; j++)
                    {
                        IDockContent content = dockPanel.Contents[panes[i].IndexContents[j]];
                        if (j == 0)
                            pane = dockPanel.DockPaneFactory.CreateDockPane(content, panes[i].DockState, false);
                        else if (panes[i].DockState == DockState.Float)
                            content.DockHandler.FloatPane = pane;
                        else
                            content.DockHandler.PanelPane = pane;
                    }
                }

                // Assign Panes to DockWindows
                for (int i = 0; i < dockWindows.Length; i++)
                {
                    for (int j = 0; j < dockWindows[i].NestedPanes.Length; j++)
                    {
                        DockWindow dw = dockPanel.DockWindows[dockWindows[i].DockState];
                        int indexPane = dockWindows[i].NestedPanes[j].IndexPane;
                        DockPane pane = dockPanel.Panes[indexPane];
                        int indexPrevPane = dockWindows[i].NestedPanes[j].IndexPrevPane;
                        DockPane prevPane = (indexPrevPane == -1) ? dw.NestedPanes.GetDefaultPreviousPane(pane) : dockPanel.Panes[indexPrevPane];
                        DockAlignment alignment = dockWindows[i].NestedPanes[j].Alignment;
                        double proportion = dockWindows[i].NestedPanes[j].Proportion;
                        pane.DockTo(dw, prevPane, alignment, proportion);
                        if (panes[indexPane].DockState == dw.DockState)
                            panes[indexPane].ZOrderIndex = dockWindows[i].ZOrderIndex;
                    }
                }

                // Create float windows
                for (int i = 0; i < floatWindows.Length; i++)
                {
                    FloatWindow fw = null;
                    for (int j = 0; j < floatWindows[i].NestedPanes.Length; j++)
                    {
                        int indexPane = floatWindows[i].NestedPanes[j].IndexPane;
                        DockPane pane = dockPanel.Panes[indexPane];
                        if (j == 0)
                            fw = dockPanel.FloatWindowFactory.CreateFloatWindow(dockPanel, pane, floatWindows[i].Bounds);
                        else
                        {
                            int indexPrevPane = floatWindows[i].NestedPanes[j].IndexPrevPane;
                            DockPane prevPane = indexPrevPane == -1 ? null : dockPanel.Panes[indexPrevPane];
                            DockAlignment alignment = floatWindows[i].NestedPanes[j].Alignment;
                            double proportion = floatWindows[i].NestedPanes[j].Proportion;
                            pane.DockTo(fw, prevPane, alignment, proportion);
                        }

                        if (floatWindows[i].Maximized)
                            fw.WindowState = FormWindowState.Maximized;

                        if (panes[indexPane].DockState == fw.DockState)
                            panes[indexPane].ZOrderIndex = floatWindows[i].ZOrderIndex;
                    }
                }

                // sort IDockContent by its Pane's ZOrder
                int[] sortedContents = null;
                if (contents.Length > 0)
                {
                    sortedContents = new int[contents.Length];
                    for (int i = 0; i < contents.Length; i++)
                        sortedContents[i] = i;

                    int lastDocument = contents.Length;
                    for (int i = 0; i < contents.Length - 1; i++)
                    {
                        for (int j = i + 1; j < contents.Length; j++)
                        {
                            DockPane pane1 = dockPanel.Contents[sortedContents[i]].DockHandler.Pane;
                            int ZOrderIndex1 = pane1 == null ? 0 : panes[dockPanel.Panes.IndexOf(pane1)].ZOrderIndex;
                            DockPane pane2 = dockPanel.Contents[sortedContents[j]].DockHandler.Pane;
                            int ZOrderIndex2 = pane2 == null ? 0 : panes[dockPanel.Panes.IndexOf(pane2)].ZOrderIndex;
                            if (ZOrderIndex1 > ZOrderIndex2)
                            {
                                int temp = sortedContents[i];
                                sortedContents[i] = sortedContents[j];
                                sortedContents[j] = temp;
                            }
                        }
                    }
                }

                // show non-document IDockContent first to avoid screen flickers
                for (int i = 0; i < contents.Length; i++)
                {
                    IDockContent content = dockPanel.Contents[sortedContents[i]];
                    if (content.DockHandler.Pane != null && content.DockHandler.Pane.DockState != DockState.Document)
                        content.DockHandler.IsHidden = contents[sortedContents[i]].IsHidden;
                }

                // after all non-document IDockContent, show document IDockContent
                for (int i = 0; i < contents.Length; i++)
                {
                    IDockContent content = dockPanel.Contents[sortedContents[i]];
                    if (content.DockHandler.Pane != null && content.DockHandler.Pane.DockState == DockState.Document)
                        content.DockHandler.IsHidden = contents[sortedContents[i]].IsHidden;
                }

                for (int i = 0; i < panes.Length; i++)
                    dockPanel.Panes[i].ActiveContent = panes[i].IndexActiveContent == -1 ? null : dockPanel.Contents[panes[i].IndexActiveContent];

                if (dockPanelStruct.IndexActiveDocumentPane != -1)
                    dockPanel.Panes[dockPanelStruct.IndexActiveDocumentPane].Activate();

                if (dockPanelStruct.IndexActivePane != -1)
                    dockPanel.Panes[dockPanelStruct.IndexActivePane].Activate();

                for (int i = dockPanel.Contents.Count - 1; i >= 0; i--)
                    if (dockPanel.Contents[i] is DummyContent)
                        dockPanel.Contents[i].DockHandler.Form.Close();

                dockPanel.ResumeLayout(true, true);
            }

            private static bool MoveToNextElement(XmlTextReader xmlIn)
            {
                if (!xmlIn.Read())
                    return false;

                while (xmlIn.NodeType == XmlNodeType.EndElement)
                {
                    if (!xmlIn.Read())
                        return false;
                }

                return true;
            }

            private static bool IsFormatVersionValid(string formatVersion)
            {
                if (formatVersion == ConfigFileVersion)
                    return true;

                foreach (string s in CompatibleConfigFileVersions)
                    if (s == formatVersion)
                        return true;

                return false;
            }
        }

        public void SaveAsXml(string fileName, string userString)
        {
            Persistor.SaveAsXml(this, fileName, userString);
        }

        public void SaveAsXml(string fileName, string userString, Encoding encoding)
        {
            Persistor.SaveAsXml(this, fileName, userString, encoding);
        }

        public void SaveAsXml(Stream stream, string userString, Encoding encoding)
        {
            Persistor.SaveAsXml(this, stream, userString, encoding);
        }

        public void SaveAsXml(Stream stream, string userString, Encoding encoding, bool upstream)
        {
            Persistor.SaveAsXml(this, stream, userString, encoding, upstream);
        }

        public void LoadFromXml(string fileName, DeserializeDockContent deserializeContent)
        {
            Persistor.LoadFromXml(this, fileName, deserializeContent);
        }

        public void LoadFromXml(Stream stream, DeserializeDockContent deserializeContent)
        {
            Persistor.LoadFromXml(this, stream, deserializeContent);
        }

        public void LoadFromXml(Stream stream, DeserializeDockContent deserializeContent, bool closeStream)
        {
            Persistor.LoadFromXml(this, stream, deserializeContent, closeStream);
        }
    }
}
