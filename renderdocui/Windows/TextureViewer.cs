/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;
using System.IO;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdocui.Controls;
using renderdocui.Windows.Dialogs;
using renderdoc;
using System.Threading;

namespace renderdocui.Windows
{
    public partial class TextureViewer : DockContent, ILogViewerForm
    {
        #region Privates

        private Core m_Core;

        private ReplayOutput m_Output = null;

        private Dialogs.TextureGoto m_Goto = null;

        private TextureDisplay m_TexDisplay = new TextureDisplay();

        private ToolStripControlHost depthStencilToolstrip = null;

        private DockContent m_PreviewPanel = null;
        private DockContent m_TexlistDockPanel = null;

        private FileSystemWatcher m_FSWatcher = null;

        private int m_HighWaterStatusLength = 0;

        public enum FollowType { OutputColour, OutputDepth, ReadWrite, ReadOnly }
        struct Following
        {
            public FollowType Type;
            public ShaderStageType Stage;
            public int index;
            public int arrayEl;

            public static Following Default = new Following(FollowType.OutputColour, ShaderStageType.Pixel, 0, 0);

            public Following(FollowType t, ShaderStageType s, int i, int a) { Type = t; Stage = s; index = i; arrayEl = a; }

            public override int GetHashCode()
            {
                return Type.GetHashCode() +
                    Stage.GetHashCode() +
                    index.GetHashCode();
            }
            public override bool Equals(object obj)
            {
                return obj is Following && this == (Following)obj;
            }
            public static bool operator ==(Following s1, Following s2)
            {
                return s1.Type == s2.Type &&
                    s1.Stage == s2.Stage &&
                    s1.index == s2.index;
            }
            public static bool operator !=(Following s1, Following s2)
            {
                return !(s1 == s2);
            }

            public static void GetDrawContext(Core core, out bool copy, out bool clear, out bool compute)
            {
                var curDraw = core.CurDrawcall;
                copy = curDraw != null && (curDraw.flags & (DrawcallFlags.Copy | DrawcallFlags.Resolve | DrawcallFlags.Present)) != 0;
                clear = curDraw != null && (curDraw.flags & DrawcallFlags.Clear) != 0;
                compute = curDraw != null && (curDraw.flags & DrawcallFlags.Dispatch) != 0 &&
                          core.CurPipelineState.GetShader(ShaderStageType.Compute) != ResourceId.Null;
            }

            public int GetHighestMip(Core core)
            {
                return GetBoundResource(core, arrayEl).HighestMip;
            }

            public int GetFirstArraySlice(Core core)
            {
                return GetBoundResource(core, arrayEl).FirstSlice;
            }

            public FormatComponentType GetTypeHint(Core core)
            {
                return GetBoundResource(core, arrayEl).typeHint;
            }

            public ResourceId GetResourceId(Core core)
            {
                return GetBoundResource(core, arrayEl).Id;
            }

            public BoundResource GetBoundResource(Core core, int arrayIdx)
            {
                BoundResource ret = new BoundResource();

                if (Type == FollowType.OutputColour)
                {
                    var outputs = GetOutputTargets(core);

                    if (index < outputs.Length)
                        ret = outputs[index];
                }
                else if (Type == FollowType.OutputDepth)
                {
                    ret = GetDepthTarget(core);
                }
                else if (Type == FollowType.ReadWrite)
                {
                    var rw = GetReadWriteResources(core);

                    var mapping = GetMapping(core);

                    if (index < mapping.ReadWriteResources.Length)
                    {
                        var key = mapping.ReadWriteResources[index];

                        if (rw.ContainsKey(key))
                            ret = rw[key][arrayIdx];
                    }
                }
                else if (Type == FollowType.ReadOnly)
                {
                    var res = GetReadOnlyResources(core);

                    var mapping = GetMapping(core);

                    if (index < mapping.ReadOnlyResources.Length)
                    {
                        var key = mapping.ReadOnlyResources[index];

                        if (res.ContainsKey(key))
                            ret = res[key][arrayIdx];
                    }
                }

                return ret;
            }

            public static BoundResource[] GetOutputTargets(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (copy || clear)
                {
                    return new BoundResource[] { new BoundResource(curDraw.copyDestination) };
                }
                else if (compute)
                {
                    return new BoundResource[0];
                }
                else
                {
                    var ret = core.CurPipelineState.GetOutputTargets();

                    if (ret.Length == 0 && curDraw != null && (curDraw.flags & DrawcallFlags.Present) != 0)
                    {
                        if (curDraw.copyDestination != ResourceId.Null)
                            return new BoundResource[] { new BoundResource(curDraw.copyDestination) };

                        foreach (var t in core.CurTextures)
                            if ((t.creationFlags & TextureCreationFlags.SwapBuffer) != 0)
                                return new BoundResource[] { new BoundResource(t.ID) };
                    }

                    return ret;
                }
            }

            public static BoundResource GetDepthTarget(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (copy || clear || compute)
                    return new BoundResource(ResourceId.Null);
                else
                    return core.CurPipelineState.GetDepthTarget();
            }

            public Dictionary<BindpointMap, BoundResource[]> GetReadWriteResources(Core core)
            {
                return GetReadWriteResources(core, Stage);
            }

            public static Dictionary<BindpointMap, BoundResource[]> GetReadWriteResources(Core core, ShaderStageType stage)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (copy || clear)
                {
                    return new Dictionary<BindpointMap, BoundResource[]>();
                }
                else if (compute)
                {
                    // only return compute resources for one stage
                    if (stage == ShaderStageType.Pixel || stage == ShaderStageType.Compute)
                        return core.CurPipelineState.GetReadWriteResources(ShaderStageType.Compute);
                    else
                        return new Dictionary<BindpointMap, BoundResource[]>();
                }
                else
                {
                    return core.CurPipelineState.GetReadWriteResources(stage);
                }
            }

            public Dictionary<BindpointMap, BoundResource[]> GetReadOnlyResources(Core core)
            {
                return GetReadOnlyResources(core, Stage);
            }

            public static Dictionary<BindpointMap, BoundResource[]> GetReadOnlyResources(Core core, ShaderStageType stage)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (clear)
                {
                    // no inputs for a clear
                    return new Dictionary<BindpointMap, BoundResource[]>();
                }
                else if (copy)
                {
                    var ret = new Dictionary<BindpointMap, BoundResource[]>();

                    // only return copy source for one stage
                    if(stage == ShaderStageType.Pixel)
                        ret.Add(new BindpointMap(0, 0), new BoundResource[] { new BoundResource(curDraw.copySource) });

                    return ret;
                }
                else if (compute)
                {
                    // only return compute resources for one stage
                    if (stage == ShaderStageType.Pixel || stage == ShaderStageType.Compute)
                        return core.CurPipelineState.GetReadOnlyResources(ShaderStageType.Compute);
                    else
                        return new Dictionary<BindpointMap, BoundResource[]>();
                }
                else
                {
                    return core.CurPipelineState.GetReadOnlyResources(stage);
                }
            }

            public ShaderReflection GetReflection(Core core)
            {
                return GetReflection(core, Stage);
            }

            public static ShaderReflection GetReflection(Core core, ShaderStageType stage)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (copy || clear)
                    return null;
                else if (compute)
                    return core.CurPipelineState.GetShaderReflection(ShaderStageType.Compute);
                else
                    return core.CurPipelineState.GetShaderReflection(stage);
            }

            public ShaderBindpointMapping GetMapping(Core core)
            {
                return GetMapping(core, Stage);
            }

            public static ShaderBindpointMapping GetMapping(Core core, ShaderStageType stage)
            {
                var curDraw = core.CurDrawcall;
                bool copy, clear, compute;
                GetDrawContext(core, out copy, out clear, out compute);

                if (copy || clear)
                {
                    ShaderBindpointMapping mapping = new ShaderBindpointMapping();
                    mapping.ConstantBlocks = new BindpointMap[0];
                    mapping.ReadWriteResources = new BindpointMap[0];
                    mapping.InputAttributes = new int[0];

                    // for copy, in PS only, add a single mapping to get the copy source
                    if (copy && stage == ShaderStageType.Pixel)
                        mapping.ReadOnlyResources = new BindpointMap[] { new BindpointMap(0, 0) };
                    else
                        mapping.ReadOnlyResources = new BindpointMap[0];

                    return mapping;
                }
                else if (compute)
                {
                    return core.CurPipelineState.GetBindpointMapping(ShaderStageType.Compute);
                }
                else
                {
                    return core.CurPipelineState.GetBindpointMapping(stage);
                }
            }
        }
        private Following m_Following = Following.Default;

        public class TexSettings
        {
            public TexSettings()
            {
                r = g = b = true; a = false;
                mip = 0; slice = 0;
                minrange = 0.0f; maxrange = 1.0f;
                typeHint = FormatComponentType.None;
            }

            public int displayType; // RGBA, RGBM, Custom
            public string customShader;
            public bool r, g, b, a;
            public bool depth, stencil;
            public int mip, slice;
            public float minrange, maxrange;
            public FormatComponentType typeHint;
        }

        private Dictionary<ResourceId, TexSettings> m_TextureSettings = new Dictionary<ResourceId, TexSettings>();

        #endregion

        public TextureViewer(Core core)
        {
            m_Core = core;

            InitializeComponent();

            if (SystemInformation.HighContrast)
            {
                dockPanel.Skin = Helpers.MakeHighContrastDockPanelSkin();
                zoomStrip.Renderer = new ToolStripSystemRenderer();
                overlayStrip.Renderer = new ToolStripSystemRenderer();
                subStrip.Renderer = new ToolStripSystemRenderer();
                rangeStrip.Renderer = new ToolStripSystemRenderer();
                channelStrip.Renderer = new ToolStripSystemRenderer();
                actionsStrip.Renderer = new ToolStripSystemRenderer();
            }

            m_Goto = new Dialogs.TextureGoto(GotoLocation);

            textureList.Font =
                texturefilter.Font =
                rangeBlack.Font =
                rangeWhite.Font =
                customShader.Font =
                hdrMul.Font =
                channels.Font =
                mipLevel.Font =
                sliceFace.Font =
                zoomOption.Font = 
                core.Config.PreferredFont;

            Icon = global::renderdocui.Properties.Resources.icon;

            textureList.m_Core = core;
            textureList.GoIconClick += new EventHandler<GoIconClickEventArgs>(textureList_GoIconClick);

            UI_SetupToolstrips();
            UI_SetupDocks();
            UI_UpdateTextureDetails();
            statusLabel.Text = "";
            zoomOption.SelectedText = "";
            mipLevel.Enabled = false;
            sliceFace.Enabled = false;

            rangeBlack.ResizeToFit = false;
            rangeWhite.ResizeToFit = false;

            PixelPicked = false;

            mainLayout.Dock = DockStyle.Fill;

            saveTex.Enabled = gotoLocationButton.Enabled = viewTexBuffer.Enabled = false;

            DockHandler.GetPersistStringCallback = PersistString;

            renderContainer.MouseWheelHandler = render_MouseWheel;
            renderContainer.MouseDown += render_MouseClick;
            renderContainer.MouseMove += render_MouseMove;

            RecreateRenderPanel();
            RecreateContextPanel();

            rangeHistogram.RangeUpdated += new EventHandler<RangeHistogramEventArgs>(rangeHistogram_RangeUpdated);

            this.DoubleBuffered = true;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            channels.SelectedIndex = 0;

            FitToWindow = true;
            overlay.SelectedIndex = 0;
            m_Following = Following.Default;

            texturefilter.SelectedIndex = 0;
        }

        private void UI_SetupDocks()
        {
            m_PreviewPanel = Helpers.WrapDockContent(dockPanel, renderToolstripContainer, "Current");
            m_PreviewPanel.DockState = DockState.Document;
            m_PreviewPanel.AllowEndUserDocking = false;
            m_PreviewPanel.Show();

            m_PreviewPanel.CloseButton = false;
            m_PreviewPanel.CloseButtonVisible = false;

            m_PreviewPanel.DockHandler.TabPageContextMenuStrip = tabContextMenu;

            dockPanel.ActiveDocumentChanged += new EventHandler(dockPanel_ActiveDocumentChanged);

            var w3 = Helpers.WrapDockContent(dockPanel, roPanel, "Inputs");
            w3.DockAreas &= ~DockAreas.Document;
            w3.DockState = DockState.DockRight;
            w3.Show();

            w3.CloseButton = false;
            w3.CloseButtonVisible = false;

            var w5 = Helpers.WrapDockContent(dockPanel, rwPanel, "Outputs");
            w5.DockAreas &= ~DockAreas.Document;
            w5.DockState = DockState.DockRight;
            w5.Show(w3.Pane, w3);

            w5.CloseButton = false;
            w5.CloseButtonVisible = false;

            m_TexlistDockPanel = Helpers.WrapDockContent(dockPanel, texlistContainer, "Texture List");
            m_TexlistDockPanel.DockAreas &= ~DockAreas.Document;
            m_TexlistDockPanel.DockState = DockState.DockLeft;
            m_TexlistDockPanel.Hide();

            m_TexlistDockPanel.HideOnClose = true;

            var w4 = Helpers.WrapDockContent(dockPanel, pixelContextPanel, "Pixel Context");
            w4.DockAreas &= ~DockAreas.Document;
            w4.Show(w3.Pane, DockAlignment.Bottom, 0.3);

            w4.CloseButton = false;
            w4.CloseButtonVisible = false;
        }

        private void UI_SetupToolstrips()
        {
            int idx = rangeStrip.Items.IndexOf(rangeWhite);
            rangeStrip.Items.Insert(idx, new ToolStripControlHost(rangeHistogram));

            for (int i = 0; i < channelStrip.Items.Count; i++)
            {
                if (channelStrip.Items[i] == mulSep)
                {
                    depthStencilToolstrip = new ToolStripControlHost(depthstencilPanel);
                    channelStrip.Items.Insert(i, depthStencilToolstrip);
                    break;
                }
            }
        }

        public class PersistData
        {
            public static int currentPersistVersion = 4;
            public int persistVersion = currentPersistVersion;

            public string panelLayout;
            public FloatVector darkBack = new FloatVector(0, 0, 0, 0);
            public FloatVector lightBack = new FloatVector(0, 0, 0, 0);

            public static PersistData GetDefaults()
            {
                PersistData data = new PersistData();

                data.panelLayout = "";
                data.darkBack = new FloatVector(0, 0, 0, 0);
                data.lightBack = new FloatVector(0, 0, 0, 0);

                return data;
            }
        }

        public void InitFromPersistString(string str)
        {
            PersistData data = null;

            try
            {
                if (str.Length > GetType().ToString().Length)
                {
                    var reader = new StringReader(str.Substring(GetType().ToString().Length));

                    System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(PersistData));
                    data = (PersistData)xs.Deserialize(reader);

                    reader.Close();
                }
            }
            catch (System.Xml.XmlException)
            {
            }
            catch(InvalidOperationException)
            {
                // don't need to handle it. Leave data null and pick up defaults below
            }

            if (data == null || data.persistVersion != PersistData.currentPersistVersion)
            {
                data = PersistData.GetDefaults();
            }

            // fixup old incorrect checkerboard colours
            if (data.lightBack.x != data.darkBack.x)
            {
                TextureDisplay defaults = new TextureDisplay();
                data.lightBack = defaults.lightBackgroundColour;
                data.darkBack = defaults.darkBackgroundColour;
            }

            ApplyPersistData(data);
        }

        private IDockContent GetContentFromPersistString(string persistString)
        {
            Control[] persistors = {
                                       renderToolstripContainer,
                                       roPanel,
                                       rwPanel,
                                       texlistContainer,
                                       pixelContextPanel
                                   };

            foreach(var p in persistors)
                if (persistString == p.Name && p.Parent is IDockContent && (p.Parent as DockContent).DockPanel == null)
                    return p.Parent as IDockContent;

            // backwards compatibilty for rename
            if(persistString == "texPanel")
                return roPanel.Parent as IDockContent;
            if(persistString == "rtPanel")
                return rwPanel.Parent as IDockContent;

            return null;
        }

        private string onloadLayout = "";
        private FloatVector darkBack = new FloatVector(0, 0, 0, 0);
        private FloatVector lightBack = new FloatVector(0, 0, 0, 0);

        private void ApplyPersistData(PersistData data)
        {
            onloadLayout = data.panelLayout;
            darkBack = data.darkBack;
            lightBack = data.lightBack;
        }

        private void TextureViewer_Load(object sender, EventArgs e)
        {
            if (onloadLayout.Length > 0)
            {
                Control[] persistors = {
                                       renderToolstripContainer,
                                       roPanel,
                                       rwPanel,
                                       texlistContainer,
                                       pixelContextPanel
                                   };

                foreach (var p in persistors)
                    (p.Parent as DockContent).DockPanel = null;

                var enc = new UnicodeEncoding();
                using (var strm = new MemoryStream(enc.GetBytes(onloadLayout)))
                {
                    strm.Flush();
                    strm.Position = 0;

                    try
                    {
                        dockPanel.LoadFromXml(strm, new DeserializeDockContent(GetContentFromPersistString));
                    }
                    catch (System.Exception)
                    {
                        // on error, go back to default layout
                        UI_SetupDocks();
                    }
                }

                onloadLayout = "";
            }

            if (darkBack.x != lightBack.x)
            {
                backcolorPick.Checked = false;
                checkerBack.Checked = true;
            }
            else
            {
                backcolorPick.Checked = true;
                checkerBack.Checked = false;

                colorDialog.Color = Color.FromArgb((int)(255 * darkBack.x),
                    (int)(255 * darkBack.y),
                    (int)(255 * darkBack.z));
            }
        }

        private string PersistString()
        {
            var writer = new StringWriter();

            writer.Write(GetType().ToString());

            PersistData data = new PersistData();

            // passing in a MemoryStream gets disposed - can't see a way to retrieve this
            // in-memory.
            var enc = new UnicodeEncoding();
            var path = Path.GetTempFileName();
            dockPanel.SaveAsXml(path, "", enc);
            try
            {
                data.panelLayout = File.ReadAllText(path, enc);
                File.Delete(path);
            }
            catch (System.Exception)
            {
                // can't recover
                return writer.ToString();
            }

            data.darkBack = darkBack;
            data.lightBack = lightBack;

            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(PersistData));
            xs.Serialize(writer, data);

            return writer.ToString();
        }

        #region Public Functions

        private Dictionary<ResourceId, DockContent> lockedTabs = new Dictionary<ResourceId, DockContent>();

        public void GotoLocation(int x, int y)
        {
            if(!m_Core.LogLoaded || CurrentTexture == null)
                return;

            m_PickedPoint = new Point(x, y);

            uint mipHeight = Math.Max(1, CurrentTexture.height >> (int)m_TexDisplay.mip);
            if (m_Core.APIProps.pipelineType == GraphicsAPI.OpenGL)
                m_PickedPoint.Y = (int)(mipHeight - 1) - m_PickedPoint.Y;
            if (m_TexDisplay.FlipY)
                m_PickedPoint.Y = (int)(mipHeight - 1) - m_PickedPoint.Y;

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                if (m_Output != null)
                    RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);

                RT_UpdateAndDisplay(r);
            });

            UI_UpdateStatusText();
        }

        public void ViewTexture(ResourceId ID, bool focus)
        {
            if (this.InvokeRequired)
            {
                this.BeginInvoke(new Action(() => { this.ViewTexture(ID, focus); }));
                return;
            }

            TextureViewer_Load(null, null);

            if (lockedTabs.ContainsKey(ID))
            {
                if (!lockedTabs[ID].IsDisposed && !lockedTabs[ID].IsHidden)
                {
                    if (focus)
                        Show();

                    lockedTabs[ID].Show();
                    m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
                    return;
                }

                lockedTabs.Remove(ID);
            }

            for (int i = 0; i < m_Core.CurTextures.Length; i++)
            {
                if (m_Core.CurTextures[i].ID == ID)
                {
                    FetchTexture current = m_Core.CurTextures[i];

                    var newPanel = Helpers.WrapDockContent(dockPanel, renderToolstripContainer, current.name);

                    newPanel.DockState = DockState.Document;
                    newPanel.AllowEndUserDocking = false;

                    newPanel.Icon = Icon.FromHandle(global::renderdocui.Properties.Resources.page_white_link.GetHicon());

                    newPanel.Tag = current;

                    newPanel.DockHandler.TabPageContextMenuStrip = tabContextMenu;
                    newPanel.FormClosing += new FormClosingEventHandler(PreviewPanel_FormClosing);

                    newPanel.Show(m_PreviewPanel.Pane, null);

                    newPanel.Show();

                    if (focus)
                        Show();

                    lockedTabs.Add(ID, newPanel);

                    m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
                    return;
                }
            }

            for (int i = 0; i < m_Core.CurBuffers.Length; i++)
            {
                if (m_Core.CurBuffers[i].ID == ID)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(true, 0, ulong.MaxValue, ID);
                    viewer.Show(DockPanel);
                    return;
                }
            }
        }

        #endregion

        #region Custom Shader handling

        private List<string> m_CustomShadersBusy = new List<string>();
        private Dictionary<string, ResourceId> m_CustomShaders = new Dictionary<string, ResourceId>();
        private Dictionary<string, ShaderViewer> m_CustomShaderEditor = new Dictionary<string, ShaderViewer>();

        private void ReloadCustomShaders(string filter)
        {
            if (!m_Core.LogLoaded) return;

            if (filter.Length > 0)
            {
                var shaders = m_CustomShaders.Values.ToArray();

                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    foreach (var s in shaders)
                        r.FreeCustomShader(s);
                });

                customShader.Items.Clear();
                m_CustomShaders.Clear();
            }
            else
            {
                var fn = Path.GetFileNameWithoutExtension(filter);
                var key = fn.ToUpperInvariant();

                if (m_CustomShaders.ContainsKey(key))
                {
                    if (m_CustomShadersBusy.Contains(key))
                        return;

                    ResourceId freed = m_CustomShaders[key];
                    m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                    {
                        r.FreeCustomShader(freed);
                    });

                    m_CustomShaders.Remove(key);

                    var text = customShader.Text;

                    for (int i = 0; i < customShader.Items.Count; i++)
                    {
                        if (customShader.Items[i].ToString() == fn)
                        {
                            customShader.Items.RemoveAt(i);
                            break;
                        }
                    }

                    customShader.Text = text;
                }
            }

            foreach (var f in Directory.EnumerateFiles(Core.ConfigDirectory, "*" + m_Core.APIProps.ShaderExtension))
            {
                var fn = Path.GetFileNameWithoutExtension(f);
                var key = fn.ToUpperInvariant();

                if (!m_CustomShaders.ContainsKey(key) && !m_CustomShadersBusy.Contains(key))
                {
                    try
                    {
                        string source = File.ReadAllText(f);

                        m_CustomShaders.Add(key, ResourceId.Null);
                        m_CustomShadersBusy.Add(key);
                        m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                        {
                            string errors = "";

                            ResourceId id = r.BuildCustomShader("main", source, 0, ShaderStageType.Pixel, out errors);

                            if (m_CustomShaderEditor.ContainsKey(key))
                            {
                                BeginInvoke((MethodInvoker)delegate
                                {
                                    m_CustomShaderEditor[key].ShowErrors(errors);
                                });
                            }

                            BeginInvoke((MethodInvoker)delegate
                            {
                                customShader.Items.Add(fn);
                                m_CustomShaders[key] = id;
                                m_CustomShadersBusy.Remove(key);

                                customShader.AutoCompleteSource = AutoCompleteSource.None;
                                customShader.AutoCompleteSource = AutoCompleteSource.ListItems;

                                UI_UpdateChannels();
                            });
                        });
                    }
                    catch (System.Exception)
                    {
                        // just continue, skip this file
                    }
                }
            }
        }

        private void customCreate_Click(object sender, EventArgs e)
        {
            if (customShader.Text == null || customShader.Text.Length == 0)
            {
                MessageBox.Show("No name entered.\nEnter a name in the textbox.", "Error Creating Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            if (m_CustomShaders.ContainsKey(customShader.Text.ToUpperInvariant()))
            {
                MessageBox.Show("Selected shader already exists.\nEnter a new name in the textbox.", "Error Creating Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                customShader.Text = "";
                UI_UpdateChannels();
                return;
            }

            var path = Path.Combine(Core.ConfigDirectory, customShader.Text + m_Core.APIProps.ShaderExtension);

            string src = "";

            if (m_Core.APIProps.pipelineType.IsD3D())
            {
                src = String.Format(
                    "float4 main(float4 pos : SV_Position, float4 uv : TEXCOORD0) : SV_Target0{0}" +
                    "{{{0}" +
                    "    return float4(0,0,0,1);{0}" +
                    "}}{0}"
                    , Environment.NewLine);
            }
            else if (m_Core.APIProps.pipelineType == GraphicsAPI.OpenGL ||
                m_Core.APIProps.pipelineType == GraphicsAPI.Vulkan)
            {
                src = String.Format(
                    "#version 420 core{0}{0}" +
                    "layout (location = 0) in vec2 uv;{0}{0}" + 
                    "layout (location = 0) out vec4 color_out;{0}{0}" + 
                    "void main(){0}" +
                    "{{{0}" +
                    "    color_out = vec4(0,0,0,1);{0}" +
                    "}}{0}"
                    , Environment.NewLine);
            }

            try
            {
                File.WriteAllText(path, src);
            }
            catch (System.Exception)
            {
                // ignore this file
            }

            // auto-open edit window
            customEdit_Click(sender, e);
        }

        private void customEdit_Click(object sender, EventArgs e)
        {
            var filename = customShader.Text;
            var key = filename.ToUpperInvariant();

            string src = "";

            try
            {
                src = File.ReadAllText(Path.Combine(Core.ConfigDirectory, filename + m_Core.APIProps.ShaderExtension));
            }
            catch (System.Exception ex)
            {
                MessageBox.Show("Couldn't open file for shader " + filename + Environment.NewLine + ex.ToString(), "Cannot open shader",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var files = new Dictionary<string, string>();
            files.Add(filename, src);
            ShaderViewer s = new ShaderViewer(m_Core, true, "Custom Shader", files,

            // Save Callback
            (ShaderViewer viewer, Dictionary<string, string> updatedfiles) =>
            {
                foreach (var f in updatedfiles)
                {
                    var path = Path.Combine(Core.ConfigDirectory, f.Key + m_Core.APIProps.ShaderExtension);
                    try
                    {
                        File.WriteAllText(path, f.Value);
                    }
                    catch (System.Exception ex)
                    {
                        MessageBox.Show("Couldn't save file for shader " + filename + Environment.NewLine + ex.ToString(), "Cannot save shader",
                                                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
            },

            // Close Callback
            () =>
            {
                m_CustomShaderEditor.Remove(key);
            });

            m_CustomShaderEditor[key] = s;

            s.Show(this.DockPanel);
        }

        private void customDelete_Click(object sender, EventArgs e)
        {
            if (customShader.Text == null || customShader.Text.Length == 0)
            {
                MessageBox.Show("No shader selected.\nSelect a custom shader from the drop-down", "Error Deleting Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            if (!m_CustomShaders.ContainsKey(customShader.Text.ToUpperInvariant()))
            {
                MessageBox.Show("Selected shader doesn't exist.\nSelect a custom shader from the drop-down", "Error Deleting Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            DialogResult res = MessageBox.Show(String.Format("Really delete {0}?", customShader.Text), "Deleting Custom Shader", MessageBoxButtons.YesNoCancel);

            if (res == DialogResult.Yes)
            {
                var path = Path.Combine(Core.ConfigDirectory, customShader.Text + m_Core.APIProps.ShaderExtension);
                if(!File.Exists(path))
                {
                    MessageBox.Show(String.Format("Shader file {0} can't be found.\nSelect a custom shader from the drop-down", customShader.Text),
                                    "Error Deleting Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                try
                {
                    File.Delete(path);
                }
                catch (Exception)
                {
                    MessageBox.Show(String.Format("Error deleting shader {0}.\nSelect a custom shader from the drop-down", customShader.Text),
                                    "Error Deleting Shader", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                customShader.Text = "";
                UI_UpdateChannels();
            }
        }

        #endregion

        #region ILogViewerForm

        void RecreateRenderPanel()
        {
            renderContainer.Controls.Clear();

            render.Dispose();

            render = new NoScrollPanel();

            render.Painting = true;

            render.BackColor = Color.Black;
            render.Dock = DockStyle.Fill;
            render.Paint += new PaintEventHandler(this.render_Paint);
            render.Layout += new LayoutEventHandler(this.render_Layout);
            render.MouseClick += new MouseEventHandler(this.render_MouseClick);
            render.MouseDown += new MouseEventHandler(this.render_MouseClick);
            render.MouseLeave += new EventHandler(this.render_MouseLeave);
            render.MouseMove += new MouseEventHandler(this.render_MouseMove);
            render.MouseUp += new MouseEventHandler(this.render_MouseUp);
            render.MouseWheel += render_MouseWheel;
            render.KeyHandler = render_KeyDown;

            renderContainer.Controls.Add(render);
        }

        void RecreateContextPanel()
        {
            pixelContextPanel.Controls.Clear();

            pixelContext.Dispose();

            pixelContext = new NoScrollPanel();

            pixelContext.Painting = true;

            pixelContext.BackColor = Color.Transparent;
            pixelContext.Dock = DockStyle.Fill;
            pixelContext.Paint += new PaintEventHandler(pixelContext_Paint);
            pixelContext.MouseClick += new MouseEventHandler(pixelContext_MouseClick);
            pixelContext.KeyHandler = render_KeyDown;

            pixelContextPanel.Controls.Add(pixelContext, 0, 0);
            pixelContextPanel.Controls.Add(debugPixelContext, 1, 1);
            pixelContextPanel.Controls.Add(pixelHistory, 0, 1);

            pixelContextPanel.SetColumnSpan(pixelContext, 2);
        }

        public void OnLogfileLoaded()
        {
            saveTex.Enabled = gotoLocationButton.Enabled = viewTexBuffer.Enabled = true;

            m_Following = Following.Default;

            rwPanel.ClearThumbnails();
            roPanel.ClearThumbnails();

            RecreateRenderPanel();
            RecreateContextPanel();

            m_HighWaterStatusLength = 0;

            IntPtr contextHandle = pixelContext.Handle;
            IntPtr renderHandle = render.Handle;
            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                m_Output = r.CreateOutput(renderHandle, OutputType.TexDisplay);
                m_Output.SetPixelContext(contextHandle);

                this.BeginInvoke(new Action(UI_CreateThumbnails));
            });

            m_FSWatcher = new FileSystemWatcher(Core.ConfigDirectory, "*" + m_Core.APIProps.ShaderExtension);
            m_FSWatcher.EnableRaisingEvents = true;
            m_FSWatcher.Changed += new FileSystemEventHandler(CustomShaderModified);
            m_FSWatcher.Renamed += new RenamedEventHandler(CustomShaderModified);
            m_FSWatcher.Created += new FileSystemEventHandler(CustomShaderModified);
            m_FSWatcher.Deleted += new FileSystemEventHandler(CustomShaderModified);
            ReloadCustomShaders("");

            texturefilter.SelectedIndex = 0;
            texturefilter.Text = "";
            textureList.FillTextureList("", true, true);

            m_TexDisplay.darkBackgroundColour = darkBack;
            m_TexDisplay.lightBackgroundColour = lightBack;

            m_TexDisplay.typeHint = FormatComponentType.None;

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
        }

        void CustomShaderModified(object sender, FileSystemEventArgs e)
        {
            if (!Visible || IsDisposed) return;

            Thread.Sleep(5);
            BeginInvoke((MethodInvoker)delegate
            {
                ReloadCustomShaders(e.Name);
            });
        }

        public void OnLogfileClosed()
        {
            if (IsDisposed) return;

            if(m_FSWatcher != null)
                m_FSWatcher.EnableRaisingEvents = false;
            m_FSWatcher = null;

            m_Output = null;

            m_TextureSettings.Clear();

            m_PrevSize = PointF.Empty;
            m_HighWaterStatusLength = 0;

            saveTex.Enabled = gotoLocationButton.Enabled = viewTexBuffer.Enabled = false;

            rwPanel.ClearThumbnails();
            roPanel.ClearThumbnails();

            RecreateRenderPanel();
            RecreateContextPanel();

            texturefilter.SelectedIndex = 0;

            m_TexDisplay = new TextureDisplay();
            m_TexDisplay.darkBackgroundColour = darkBack;
            m_TexDisplay.lightBackgroundColour = lightBack;

            m_TexDisplay.typeHint = FormatComponentType.None;

            PixelPicked = false;

            statusLabel.Text = "";
            m_PreviewPanel.Text = "Current";
            zoomOption.Text = "";
            mipLevel.Items.Clear();
            sliceFace.Items.Clear();
            rangeHistogram.SetRange(0.0f, 1.0f);

            channels.SelectedIndex = 0;
            overlay.SelectedIndex = 0;

            customShader.Items.Clear();
            m_CustomShaders.Clear();

            textureList.Items.Clear();

            render.Invalidate();

            renderHScroll.Enabled = false;
            renderVScroll.Enabled = false;

            hoverSwatch.BackColor = Color.Black;

            var tabs = m_PreviewPanel.Pane.TabStripControl.Tabs;

            for (int i = 0; i < tabs.Count; i++)
            {
                if (tabs[i].Content != m_PreviewPanel)
                {
                    (tabs[i].Content as DockContent).Close();
                    i--;
                }
            }

            (m_PreviewPanel as DockContent).Show();

            UI_UpdateTextureDetails();
            UI_UpdateChannels();
        }

        private void InitResourcePreview(ResourcePreview prev, ResourceId id, FormatComponentType typeHint, bool force, Following follow, string bindName, string slotName)
        {
            if (id != ResourceId.Null || force)
            {
                FetchTexture tex = null;
                foreach (var t in m_Core.CurTextures)
                    if (t.ID == id)
                        tex = t;

                FetchBuffer buf = null;
                foreach (var b in m_Core.CurBuffers)
                    if (b.ID == id)
                        buf = b;

                if (tex != null)
                {
                    string fullname = bindName;
                    if (tex.customName)
                    {
                        if (fullname.Length > 0)
                            fullname += " = ";
                        fullname += tex.name;
                    }
                    if (fullname.Length == 0)
                        fullname = tex.name;

                    prev.Init(fullname, tex.width, tex.height, tex.depth, tex.mips);
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, id, typeHint);
                    });
                }
                else if (buf != null)
                {
                    string fullname = bindName;
                    if (buf.customName)
                    {
                        if (fullname.Length > 0)
                            fullname += " = ";
                        fullname += buf.name;
                    }
                    if (fullname.Length == 0)
                        fullname = buf.name;

                    prev.Init(fullname, buf.length, 0, 0, 1);
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null, FormatComponentType.None);
                    });
                }
                else
                {
                    prev.Init();
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null, FormatComponentType.None);
                    });
                }

                prev.Tag = follow;
                prev.SlotName = slotName;
                prev.Visible = true;
                prev.Selected = (m_Following == follow);
            }
            else if (m_Following == follow)
            {
                FetchTexture tex = null;

                if(id != ResourceId.Null)
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == id)
                            tex = t;

                IntPtr handle = prev.ThumbnailHandle;
                if (id == ResourceId.Null || tex == null)
                    prev.Init();
                else
                    prev.Init("Unused", tex.width, tex.height, tex.depth, tex.mips);
                prev.Selected = true;
                m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                {
                    m_Output.AddThumbnail(handle, ResourceId.Null, FormatComponentType.None);
                });
            }
            else
            {
                prev.Init();
                prev.Visible = false;
            }
        }

        private void InitStageResourcePreviews(ShaderStageType stage, ShaderResource[] resourceDetails, BindpointMap[] mapping,
                                               Dictionary<BindpointMap, BoundResource[]> ResList,
                                               ThumbnailStrip prevs, ref int prevIndex,
                                               bool copy, bool rw)
        {
            for (int idx = 0; idx < mapping.Length; idx++)
            {
                var key = mapping[idx];

                BoundResource[] resArray = null;

                if (ResList.ContainsKey(key))
                    resArray = ResList[key];

                int arrayLen = resArray != null ? resArray.Length : 1;

                for (int arrayIdx = 0; arrayIdx < arrayLen; arrayIdx++)
                {
                    ResourceId id = resArray != null ? resArray[arrayIdx].Id : ResourceId.Null;
                    FormatComponentType typeHint = resArray != null ? resArray[arrayIdx].typeHint : FormatComponentType.None;

                    bool used = key.used;
                    bool samplerBind = false;
                    bool otherBind = false;

                    string bindName = "";

                    foreach (var bind in resourceDetails)
                    {
                        if (bind.bindPoint == idx && bind.IsSRV && !bind.IsSampler)
                        {
                            bindName = bind.name;
                            otherBind = true;
                            break;
                        }

                        if (bind.bindPoint == idx)
                        {
                            if(bind.IsSampler)
                                samplerBind = true;
                            else
                                otherBind = true;
                        }
                    }

                    if (samplerBind && !otherBind)
                        continue;

                    if (copy)
                    {
                        used = true;
                        bindName = "Source";
                    }

                    Following follow = new Following(rw ? FollowType.ReadWrite : FollowType.ReadOnly, stage, idx, arrayIdx);
                    string slotName = String.Format("{0} {1}{2}", m_Core.CurPipelineState.Abbrev(stage), rw ? "RW " : "", idx);

                    if (arrayLen > 1)
                        slotName += String.Format("[{0}]", arrayIdx);

                    if (copy)
                        slotName = "SRC";

                    // show if
                    bool show = (used || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !used && id != ResourceId.Null) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && id == ResourceId.Null) // it's empty, and we have "show empty"
                        );

                    ResourcePreview prev;

                    if (prevIndex < prevs.Thumbnails.Length)
                    {
                        prev = prevs.Thumbnails[prevIndex];
                    }
                    else
                    {
                        // don't create it if we're not actually going to show it
                        if (!show)
                            continue;

                        prev = UI_CreateThumbnail(prevs);
                    }

                    prevIndex++;

                    InitResourcePreview(prev, show ? id : ResourceId.Null, typeHint, show, follow, bindName, slotName);
                }
            }
        }

        public void OnEventSelected(UInt32 eventID)
        {
            if (IsDisposed) return;

            if (!CurrentTextureIsLocked || (CurrentTexture != null && m_TexDisplay.texid != CurrentTexture.ID))
                UI_OnTextureSelectionChanged(true);

            if (m_Output == null) return;

            UI_CreateThumbnails();

            BoundResource[] RTs = Following.GetOutputTargets(m_Core);
            BoundResource Depth = Following.GetDepthTarget(m_Core);

            int rwIndex = 0;
            int roIndex = 0;

            var curDraw = m_Core.GetDrawcall(eventID);
            bool copy = curDraw != null && (curDraw.flags & (DrawcallFlags.Copy|DrawcallFlags.Resolve|DrawcallFlags.Present)) != 0;
            bool compute = curDraw != null && (curDraw.flags & (DrawcallFlags.Dispatch)) != 0;

            for(int rt=0; rt < RTs.Length; rt++)
            {
                ResourcePreview prev;

                if (rwIndex < rwPanel.Thumbnails.Length)
                    prev = rwPanel.Thumbnails[rwIndex];
                else
                    prev = UI_CreateThumbnail(rwPanel);

                rwIndex++;

                Following follow = new Following(FollowType.OutputColour, ShaderStageType.Pixel, rt, 0);
                string bindName = copy ? "Destination" : "";
                string slotName = copy ? "DST" : String.Format("{0}{1}", m_Core.CurPipelineState.OutputAbbrev(), rt);

                InitResourcePreview(prev, RTs[rt].Id, RTs[rt].typeHint, false, follow, bindName, slotName);
            }

            // depth
            {
                ResourcePreview prev;
                
                if (rwIndex < rwPanel.Thumbnails.Length)
                    prev = rwPanel.Thumbnails[rwIndex];
                else
                    prev = UI_CreateThumbnail(rwPanel);

                rwIndex++;
                
                Following follow = new Following(FollowType.OutputDepth, ShaderStageType.Pixel, 0, 0);

                InitResourcePreview(prev, Depth.Id, Depth.typeHint, false, follow, "", "DS");
            }

            ShaderStageType[] stages = new ShaderStageType[] { 
                ShaderStageType.Vertex,
                ShaderStageType.Hull,
                ShaderStageType.Domain,
                ShaderStageType.Geometry,
                ShaderStageType.Pixel
            };

            if (compute) stages = new ShaderStageType[] { ShaderStageType.Compute };

            // display resources used for all stages
            foreach (ShaderStageType stage in stages)
            {
                Dictionary<BindpointMap, BoundResource[]> RWs = Following.GetReadWriteResources(m_Core, stage);
                Dictionary<BindpointMap, BoundResource[]> ROs = Following.GetReadOnlyResources(m_Core, stage);

                ShaderReflection details = Following.GetReflection(m_Core, stage);
                ShaderBindpointMapping mapping = Following.GetMapping(m_Core, stage);

                if (mapping == null)
                    continue;

                InitStageResourcePreviews(stage,
                    details != null ? details.ReadWriteResources : new ShaderResource[0],
                    mapping.ReadWriteResources,
                    RWs, rwPanel, ref rwIndex, copy, true);

                InitStageResourcePreviews(stage,
                    details != null ? details.ReadOnlyResources : new ShaderResource[0],
                    mapping.ReadOnlyResources,
                    ROs, roPanel, ref roIndex, copy, false);
            }

            // hide others
            for (; rwIndex < rwPanel.Thumbnails.Length; rwIndex++)
            {
                rwPanel.Thumbnails[rwIndex].Init();
                rwPanel.Thumbnails[rwIndex].Visible = false;
            }

            rwPanel.RefreshLayout();
            
            for (; roIndex < roPanel.Thumbnails.Length; roIndex++)
            {
                roPanel.Thumbnails[roIndex].Init();
                roPanel.Thumbnails[roIndex].Visible = false;
            }

            roPanel.RefreshLayout();

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);

            if(autoFit.Checked)
                AutoFitRange();
        }

        #endregion

        #region Update UI state

        private ResourcePreview UI_CreateThumbnail(ThumbnailStrip strip)
        {
            var prev = new ResourcePreview(m_Core, m_Output);
            prev.Anchor = AnchorStyles.Top | AnchorStyles.Bottom;
            prev.MouseClick += thumbsLayout_MouseClick;
            prev.MouseDoubleClick += thumbsLayout_MouseDoubleClick;
            prev.Visible = false;
            strip.AddThumbnail(prev);
            return prev;
        }

        private void UI_CreateThumbnails()
        {
            if (rwPanel.Thumbnails.Length > 0 || roPanel.Thumbnails.Length > 0) return;

            rwPanel.SuspendLayout();
            roPanel.SuspendLayout();

            // these will expand, but we make sure that there is a good set reserved
            for (int i = 0; i < 9; i++)
            {
                var prev = UI_CreateThumbnail(rwPanel);

                if(i == 0)
                    prev.Selected = true;
            }

            for (int i = 0; i < 128; i++)
                UI_CreateThumbnail(roPanel);

            rwPanel.ResumeLayout();
            roPanel.ResumeLayout();
        }

        private int prevFirstArraySlice = -1;
        private int prevHighestMip = -1;

        private PointF m_PrevSize = PointF.Empty;

        private void UI_SetHistogramRange(FetchTexture tex, FormatComponentType typeHint)
        {
            if (tex != null && (tex.format.compType == FormatComponentType.SNorm || typeHint == FormatComponentType.SNorm))
                rangeHistogram.SetRange(-1.0f, 1.0f);
            else
                rangeHistogram.SetRange(0.0f, 1.0f);
        }

        private void UI_OnTextureSelectionChanged(bool newdraw)
        {
            FetchTexture tex = CurrentTexture;

            // reset high-water mark
            m_HighWaterStatusLength = 0;

            if (tex == null) return;

            bool newtex = (m_TexDisplay.texid != tex.ID);

            // save settings for this current texture
            if (m_Core.Config.TextureViewer_PerTexSettings)
            {
                if (!m_TextureSettings.ContainsKey(m_TexDisplay.texid))
                    m_TextureSettings.Add(m_TexDisplay.texid, new TexSettings());

                m_TextureSettings[m_TexDisplay.texid].r = customRed.Checked;
                m_TextureSettings[m_TexDisplay.texid].g = customGreen.Checked;
                m_TextureSettings[m_TexDisplay.texid].b = customBlue.Checked;
                m_TextureSettings[m_TexDisplay.texid].a = customAlpha.Checked;

                m_TextureSettings[m_TexDisplay.texid].displayType = channels.SelectedIndex;
                m_TextureSettings[m_TexDisplay.texid].customShader = customShader.Text;

                m_TextureSettings[m_TexDisplay.texid].depth = depthDisplay.Checked;
                m_TextureSettings[m_TexDisplay.texid].stencil = stencilDisplay.Checked;

                m_TextureSettings[m_TexDisplay.texid].mip = mipLevel.SelectedIndex;
                m_TextureSettings[m_TexDisplay.texid].slice = sliceFace.SelectedIndex;

                m_TextureSettings[m_TexDisplay.texid].minrange = rangeHistogram.BlackPoint;
                m_TextureSettings[m_TexDisplay.texid].maxrange = rangeHistogram.WhitePoint;

                m_TextureSettings[m_TexDisplay.texid].typeHint = m_Following.GetTypeHint(m_Core);
            }

            m_TexDisplay.texid = tex.ID;

            // interpret the texture according to the currently following type.
            if(!CurrentTextureIsLocked)
                m_TexDisplay.typeHint = m_Following.GetTypeHint(m_Core);
            else
                m_TexDisplay.typeHint = FormatComponentType.None;

            // if there is no such type or it isn't being followed, use the last seen interpretation
            if (m_TexDisplay.typeHint == FormatComponentType.None && m_TextureSettings.ContainsKey(m_TexDisplay.texid))
                m_TexDisplay.typeHint = m_TextureSettings[m_TexDisplay.texid].typeHint;

            // try to maintain the pan in the new texture. If the new texture
            // is approx an integer multiple of the old texture, just changing
            // the scale will keep everything the same. This is useful for
            // downsample chains and things where you're flipping back and forth
            // between overlapping textures, but even in the non-integer case
            // pan will be kept approximately the same.
            PointF curSize = new PointF((float)CurrentTexture.width, (float)CurrentTexture.height);
            float curArea = curSize.Area();
            float prevArea = m_PrevSize.Area();

            if (prevArea > 0.0f)
            {
                float prevX = m_TexDisplay.offx;
                float prevY = m_TexDisplay.offy;
                float prevScale = m_TexDisplay.scale;

                // allow slight difference in aspect ratio for rounding errors
                // in downscales (e.g. 1680x1050 -> 840x525 -> 420x262 in the
                // last downscale the ratios are 1.6 and 1.603053435).
                if (Math.Abs(curSize.Aspect() - m_PrevSize.Aspect()) < 0.01f)
                {
                    m_TexDisplay.scale *= m_PrevSize.X / curSize.X;
                    CurrentZoomValue = m_TexDisplay.scale;
                }
                else
                {
                    // this scale factor is arbitrary really, only intention is to have
                    // integer scales come out precisely, other 'similar' sizes will be
                    // similar ish
                    float scaleFactor = (float)(Math.Sqrt(curArea) / Math.Sqrt(prevArea));

                    m_TexDisplay.offx = prevX * scaleFactor;
                    m_TexDisplay.offy = prevY * scaleFactor;
                }
            }

            m_PrevSize = curSize;

            // refresh scroll position
            ScrollPosition = ScrollPosition;

            UI_UpdateStatusText();

            mipLevel.Items.Clear();

            m_TexDisplay.mip = 0;
            m_TexDisplay.sliceFace = 0;

            bool usemipsettings = true;
            bool useslicesettings = true;

            if (tex.msSamp > 1)
            {
                for (int i = 0; i < tex.msSamp; i++)
                    mipLevel.Items.Add(String.Format("Sample {0}", i));

                // add an option to display unweighted average resolved value,
                // to get an idea of how the samples average
                if(tex.format.compType != FormatComponentType.UInt &&
                    tex.format.compType != FormatComponentType.SInt &&
                    tex.format.compType != FormatComponentType.Depth &&
                    (tex.creationFlags & TextureCreationFlags.DSV) == 0)
                    mipLevel.Items.Add("Average val");

                mipLevelLabel.Text = "Sample";

                mipLevel.SelectedIndex = 0;
            }
            else
            {
                for (int i = 0; i < tex.mips; i++)
                    mipLevel.Items.Add(i + " - " + Math.Max(1, tex.width >> i) + "x" + Math.Max(1, tex.height >> i));

                mipLevelLabel.Text = "Mip";

                int highestMip = -1;

                // only switch to the selected mip for outputs, and when changing drawcall
                if (!CurrentTextureIsLocked && m_Following.Type != FollowType.ReadOnly && newdraw)
                    highestMip = m_Following.GetHighestMip(m_Core);

                // assuming we get a valid mip for the highest mip, only switch to it
                // if we've selected a new texture, or if it's different than the last mip.
                // This prevents the case where the user has clicked on another mip and
                // we don't want to snap their view back when stepping between events with the
                // same mip used. But it does mean that if they are stepping between
                // events with different mips used, then we will update in that case.
                if (highestMip >= 0 && (newtex || highestMip != prevHighestMip))
                {
                    usemipsettings = false;
                    mipLevel.SelectedIndex = Helpers.Clamp(highestMip, 0, (int)tex.mips - 1);
                }

                if (mipLevel.SelectedIndex == -1)
                    mipLevel.SelectedIndex = Helpers.Clamp(prevHighestMip, 0, (int)tex.mips - 1);

                prevHighestMip = highestMip;
            }

            if (tex.mips == 1 && tex.msSamp <= 1)
            {
                mipLevel.Enabled = false;
            }
            else
            {
                mipLevel.Enabled = true;
            }

            sliceFace.Items.Clear();

            if (tex.arraysize == 1 && tex.depth <= 1)
            {
                sliceFace.Enabled = false;
            }
            else
            {
                sliceFace.Enabled = true;

                sliceFace.Visible = sliceFaceLabel.Visible = true;

                String[] cubeFaces = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

                UInt32 numSlices = tex.arraysize;

                // for 3D textures, display the number of slices at this mip
                if(tex.depth > 1)
                    numSlices = Math.Max(1, tex.depth >> (int)mipLevel.SelectedIndex);

                for (UInt32 i = 0; i < numSlices; i++)
                {
                    if (tex.cubemap)
                    {
                        String name = cubeFaces[i%6];
                        if (numSlices > 6)
                            name = string.Format("[{0}] {1}", (i / 6), cubeFaces[i%6]); // Front 1, Back 2, 3, 4 etc for cube arrays
                        sliceFace.Items.Add(name);
                    }
                    else
                    {
                        sliceFace.Items.Add("Slice " + i);
                    }
                }

                int firstArraySlice = -1;
                // only switch to the selected mip for outputs, and when changing drawcall
                if (!CurrentTextureIsLocked && m_Following.Type != FollowType.ReadOnly && newdraw)
                    firstArraySlice = m_Following.GetFirstArraySlice(m_Core);

                // see above with highestMip and prevHighestMip for the logic behind this
                if (firstArraySlice >= 0 && (newtex || firstArraySlice != prevFirstArraySlice))
                {
                    useslicesettings = false;
                    sliceFace.SelectedIndex = Helpers.Clamp(firstArraySlice, 0, (int)numSlices - 1);
                }

                if (sliceFace.SelectedIndex == -1)
                    sliceFace.SelectedIndex = Helpers.Clamp(prevFirstArraySlice, 0, (int)numSlices - 1);

                prevFirstArraySlice = firstArraySlice;
            }

            // because slice and mip are specially set above, we restore any per-tex settings to apply
            // even if we don't switch to a new texture.
            // Note that if the slice or mip was changed because that slice or mip is the selected one
            // at the API level, we leave this alone.
            if (m_Core.Config.TextureViewer_PerTexSettings && m_TextureSettings.ContainsKey(tex.ID))
            {
                if (usemipsettings)
                    mipLevel.SelectedIndex = m_TextureSettings[tex.ID].mip;

                if (useslicesettings)
                    sliceFace.SelectedIndex = m_TextureSettings[tex.ID].slice;
            }

            // handling for if we've switched to a new texture
            if (newtex)
            {
                // if we save certain settings per-texture, restore them (if we have any)
                if (m_Core.Config.TextureViewer_PerTexSettings && m_TextureSettings.ContainsKey(tex.ID))
                {
                    channels.SelectedIndex = m_TextureSettings[tex.ID].displayType;

                    customShader.Text = m_TextureSettings[tex.ID].customShader;

                    customRed.Checked = m_TextureSettings[tex.ID].r;
                    customGreen.Checked = m_TextureSettings[tex.ID].g;
                    customBlue.Checked = m_TextureSettings[tex.ID].b;
                    customAlpha.Checked = m_TextureSettings[tex.ID].a;

                    depthDisplay.Checked = m_TextureSettings[m_TexDisplay.texid].depth;
                    stencilDisplay.Checked = m_TextureSettings[m_TexDisplay.texid].stencil;

                    norangePaint = true;
                    rangeHistogram.SetRange(m_TextureSettings[m_TexDisplay.texid].minrange, m_TextureSettings[m_TexDisplay.texid].maxrange);
                    norangePaint = false;
                }
                else if (m_Core.Config.TextureViewer_PerTexSettings)
                {
                    // if we are using per-tex settings, reset back to RGB
                    channels.SelectedIndex = 0;

                    customShader.Text = "";

                    customRed.Checked = true;
                    customGreen.Checked = true;
                    customBlue.Checked = true;
                    customAlpha.Checked = false;

                    stencilDisplay.Checked = false;
                    depthDisplay.Checked = true;

                    norangePaint = true;
                    UI_SetHistogramRange(tex, m_TexDisplay.typeHint);
                    norangePaint = false;
                }

                // reset the range if desired
                if (m_Core.Config.TextureViewer_ResetRange)
                {
                    UI_SetHistogramRange(tex, m_TexDisplay.typeHint);
                }
            }

            UI_UpdateFittedScale();

            UI_UpdateTextureDetails();

            UI_UpdateChannels();

            if (autoFit.Checked)
                AutoFitRange();

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                RT_UpdateVisualRange(r);

                RT_UpdateAndDisplay(r);

                if (tex.ID != ResourceId.Null)
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);

                    var us = r.GetUsage(tex.ID);

                    var tb = m_Core.TimelineBar;

                    if (tb != null && tb.Visible && !tb.IsDisposed)
                    {
                        this.BeginInvoke(new Action(() =>
                        {
                            tb.HighlightResource(tex.ID, tex.name, us);
                        }));
                    }
                }
                else
                {
                    m_CurPixelValue = null;
                    m_CurRealValue = null;
                }
            });
        }

        void dockPanel_ActiveDocumentChanged(object sender, EventArgs e)
        {
            var d = dockPanel.ActiveDocument as DockContent;

            if (d == null) return;

            if (d.Visible)
                d.Controls.Add(renderToolstripContainer);

            UI_OnTextureSelectionChanged(false);
        }

        void PreviewPanel_FormClosing(object sender, FormClosingEventArgs e)
        {
            if ((sender as Control).Visible == false && renderToolstripContainer.Parent != sender)
                return;

            var tabs = m_PreviewPanel.Pane.TabStripControl.Tabs;

            for (int i = 0; i < tabs.Count; i++)
            {
                if (tabs[i].Content == sender)
                {
                    var dc = m_PreviewPanel;

                    if (i > 0)
                    {
                        dc = (tabs[i - 1].Content as DockContent);
                    }
                    else if (i < tabs.Count - 1)
                    {
                        dc = (tabs[i + 1].Content as DockContent);
                    }

                    dc.Controls.Add(renderToolstripContainer);
                    dc.Show();

                    return;
                }
            }

            m_PreviewPanel.Controls.Add(renderToolstripContainer);
            m_PreviewPanel.Show();
        }

        private void UI_UpdateTextureDetails()
        {
            texStatusDim.Text = "";

            if (m_Core.CurTextures == null || CurrentTexture == null)
            {
                m_PreviewPanel.Text = "Unbound";

                texStatusDim.Text = "";
                return;
            }

            FetchTexture current = CurrentTexture;

            ResourceId followID = m_Following.GetResourceId(m_Core);

            {
                bool found = false;

                string name = "";

                foreach (var t in m_Core.CurTextures)
                {
                    if (t.ID == followID)
                    {
                        name = t.name;
                        found = true;
                    }
                }

                foreach (var b in m_Core.CurBuffers)
                {
                    if (b.ID == followID)
                    {
                        name = b.name;
                        found = true;
                    }
                }

                if (followID == ResourceId.Null)
                {
                    m_PreviewPanel.Text = "Unbound";
                }
                else if(found)
                {
                    switch (m_Following.Type)
                    {
                        case FollowType.OutputColour:
                            m_PreviewPanel.Text = string.Format("Cur Output {0} - {1}", m_Following.index, name);
                            break;
                        case FollowType.OutputDepth:
                            m_PreviewPanel.Text = string.Format("Cur Depth Output - {0}", name);
                            break;
                        case FollowType.ReadWrite:
                            m_PreviewPanel.Text = string.Format("Cur RW Output - {0}", name);
                            break;
                        case FollowType.ReadOnly:
                            m_PreviewPanel.Text = string.Format("Cur Input {0} - {1}", m_Following.index, name);
                            break;
                    }
                }
                else
                {
                    switch (m_Following.Type)
                    {
                        case FollowType.OutputColour:
                            m_PreviewPanel.Text = string.Format("Cur Output {0}", m_Following.index);
                            break;
                        case FollowType.OutputDepth:
                            m_PreviewPanel.Text = string.Format("Cur Depth Output");
                            break;
                        case FollowType.ReadWrite:
                            m_PreviewPanel.Text = string.Format("Cur RW Output");
                            break;
                        case FollowType.ReadOnly:
                            m_PreviewPanel.Text = string.Format("Cur Input {0}", m_Following.index);
                            break;
                    }
                }
            }

            texStatusDim.Text = current.name + " - ";
            
            if (current.dimension >= 1)
                texStatusDim.Text += current.width;
            if (current.dimension >= 2)
                texStatusDim.Text += "x" + current.height;
            if (current.dimension >= 3)
                texStatusDim.Text += "x" + current.depth;

            if (current.arraysize > 1)
                texStatusDim.Text += "[" + current.arraysize + "]";

            if(current.msQual > 0 || current.msSamp > 1)
                texStatusDim.Text += string.Format(" MS{{{0}x {1}Q}}", current.msSamp, current.msQual);

            texStatusDim.Text += " " + current.mips + " mips";

            texStatusDim.Text += " - " + current.format.ToString();

            if (current.format.compType != m_TexDisplay.typeHint &&
                 m_TexDisplay.typeHint != FormatComponentType.None)
            {
                texStatusDim.Text += " Viewed as " + m_TexDisplay.typeHint.Str();
            }
        }

        private bool PixelPicked
        {
            get
            {
                return (m_CurPixelValue != null);
            }

            set
            {
                if (value == true)
                {
                    debugPixelContext.Enabled = true;
                    toolTip.RemoveAll();
                    toolTip.SetToolTip(debugPixelContext, "Debug this pixel");
                    toolTip.SetToolTip(pixelHistory, "Show history for this pixel");

                    pixelHistory.Enabled = true;
                }
                else
                {
                    m_CurPixelValue = null;
                    m_CurRealValue = null;

                    debugPixelContext.Enabled = false;
                    toolTip.RemoveAll();
                    toolTip.SetToolTip(debugPixelContext, "Right Click to choose a pixel");
                    toolTip.SetToolTip(pixelHistory, "Right Click to choose a pixel");

                    pixelHistory.Enabled = false;
                }

                pixelContext.Invalidate();
            }
        }

        private void UI_UpdateStatusText()
        {
            if (textureList.InvokeRequired)
            {
                this.BeginInvoke(new Action(UI_UpdateStatusText));
                return;
            }

            FetchTexture tex = CurrentTexture;

            if (tex == null) return;

            bool dsv = ((tex.creationFlags & TextureCreationFlags.DSV) != 0) || (tex.format.compType == FormatComponentType.Depth);
            bool uintTex = (tex.format.compType == FormatComponentType.UInt);
            bool sintTex = (tex.format.compType == FormatComponentType.SInt);

            if (tex.format.compType == FormatComponentType.None && m_TexDisplay.typeHint == FormatComponentType.UInt)
                uintTex = true;

            if (tex.format.compType == FormatComponentType.None && m_TexDisplay.typeHint == FormatComponentType.SInt)
                sintTex = true;

            if (m_TexDisplay.overlay == TextureDisplayOverlay.QuadOverdrawPass ||
                m_TexDisplay.overlay == TextureDisplayOverlay.QuadOverdrawDraw)
            {
                dsv = false;
                uintTex = false;
                sintTex = true;
            }

            if (m_CurHoverValue != null)
            {
                if (dsv || uintTex || sintTex)
                {
                    hoverSwatch.BackColor = Color.Black;
                }
                else
                {
                    float r = Helpers.Clamp(m_CurHoverValue.value.f[0], 0.0f, 1.0f);
                    float g = Helpers.Clamp(m_CurHoverValue.value.f[1], 0.0f, 1.0f);
                    float b = Helpers.Clamp(m_CurHoverValue.value.f[2], 0.0f, 1.0f);

                    if (tex.format.srgbCorrected || (tex.creationFlags & TextureCreationFlags.SwapBuffer) > 0)
                    {
                        r = (float)Math.Pow(r, 1.0f / 2.2f);
                        g = (float)Math.Pow(g, 1.0f / 2.2f);
                        b = (float)Math.Pow(b, 1.0f / 2.2f);
                    }

                    hoverSwatch.BackColor = Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));
                }
            }

            int y = m_CurHoverPixel.Y >> (int)m_TexDisplay.mip;

            uint mipWidth = Math.Max(1, tex.width >> (int)m_TexDisplay.mip);
            uint mipHeight = Math.Max(1, tex.height >> (int)m_TexDisplay.mip);

            if (m_Core.APIProps.pipelineType == GraphicsAPI.OpenGL)
                y = (int)(mipHeight - 1) - y;
            if (m_TexDisplay.FlipY)
                y = (int)(mipHeight - 1) - y;

            y = Math.Max(0, y);

            int x = m_CurHoverPixel.X >> (int)m_TexDisplay.mip;
            float invWidth = mipWidth > 0 ? 1.0f / mipWidth : 0.0f;
            float invHeight = mipHeight > 0 ? 1.0f / mipHeight : 0.0f;

            string hoverCoords = String.Format("{0,4}, {1,4} ({2:0.0000}, {3:0.0000})", 
                x, y, (x * invWidth), (y * invHeight));

            string statusText = "Hover - " + hoverCoords;

            if (m_CurHoverPixel.X > tex.width || m_CurHoverPixel.Y > tex.height || m_CurHoverPixel.X < 0 || m_CurHoverPixel.Y < 0)
                statusText = "Hover - [" + hoverCoords + "]";

            if (m_CurPixelValue != null)
            {
                x = m_PickedPoint.X >> (int)m_TexDisplay.mip;
                y = m_PickedPoint.Y >> (int)m_TexDisplay.mip;
                if (m_Core.APIProps.pipelineType == GraphicsAPI.OpenGL)
                    y = (int)(mipHeight - 1) - y;
                if (m_TexDisplay.FlipY)
                    y = (int)(mipHeight - 1) - y;

                y = Math.Max(0, y);

                statusText += " - Right click - " + String.Format("{0,4}, {1,4}: ", x, y);

                PixelValue val = m_CurPixelValue;

                if (m_TexDisplay.CustomShader != ResourceId.Null && m_CurRealValue != null)
                {
                    statusText += Formatter.Format(val.value.f[0]) + ", " +
                                  Formatter.Format(val.value.f[1]) + ", " +
                                  Formatter.Format(val.value.f[2]) + ", " +
                                  Formatter.Format(val.value.f[3]);

                    val = m_CurRealValue;
                    
                    statusText += " (Real: ";
                }

                if (dsv)
                {
                    statusText += "Depth ";
                    if (uintTex)
                    {
                        if(tex.format.compByteWidth == 2)
                            statusText += Formatter.Format(val.value.u16[0]);
                        else              
                            statusText += Formatter.Format(val.value.u[0]);
                    }
                    else
                    {
                        statusText += Formatter.Format(val.value.f[0]);
                    }
                    statusText += String.Format(", Stencil {0} / 0x{0:X2}", (int)(255.0f * val.value.f[1]));
                }
                else
                {
                    if (uintTex)
                    {
                        statusText += val.value.u[0].ToString() + ", " +
                                      val.value.u[1].ToString() + ", " +
                                      val.value.u[2].ToString() + ", " +
                                      val.value.u[3].ToString();
                    }
                    else if (sintTex)
                    {
                        statusText += val.value.i[0].ToString() + ", " +
                                      val.value.i[1].ToString() + ", " +
                                      val.value.i[2].ToString() + ", " +
                                      val.value.i[3].ToString();
                    }
                    else
                    {
                        statusText += Formatter.Format(val.value.f[0]) + ", " +
                                      Formatter.Format(val.value.f[1]) + ", " +
                                      Formatter.Format(val.value.f[2]) + ", " +
                                      Formatter.Format(val.value.f[3]);
                    }
                }

                if (m_TexDisplay.CustomShader != ResourceId.Null)
                    statusText += ")";

                PixelPicked = true;
            }
            else
            {
                statusText += " - Right click to pick a pixel";

                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        m_Output.DisablePixelContext();
                });

                PixelPicked = false;
            }

            // try and keep status text consistent by sticking to the high water mark
            // of length (prevents nasty oscillation when the length of the string is
            // just popping over/under enough to overflow onto the next line).

            if (statusText.Length > m_HighWaterStatusLength)
                m_HighWaterStatusLength = statusText.Length;

            if (statusText.Length < m_HighWaterStatusLength)
                statusText += new String(' ', m_HighWaterStatusLength - statusText.Length);

            statusLabel.Text = statusText;
        }

        private void UI_UpdateChannels()
        {
            FetchTexture tex = CurrentTexture;

            channelStrip.SuspendLayout();

            if (tex != null && (tex.creationFlags & TextureCreationFlags.SwapBuffer) != 0)
            {
                // swapbuffer is always srgb for 8-bit types, linear for 16-bit types
                gammaDisplay.Enabled = false;

                if (tex.format.compByteWidth == 2 && !tex.format.special)
                    m_TexDisplay.linearDisplayAsGamma = false;
                else
                    m_TexDisplay.linearDisplayAsGamma = true;
            }
            else
            {
                if (tex != null && !tex.format.srgbCorrected)
                    gammaDisplay.Enabled = true;
                else
                    gammaDisplay.Enabled = false;

                m_TexDisplay.linearDisplayAsGamma = !gammaDisplay.Enabled || gammaDisplay.Checked;
            }

            if (tex != null && tex.format.srgbCorrected)
                m_TexDisplay.linearDisplayAsGamma = false;

            bool dsv = false;
            if(tex != null)
                dsv = ((tex.creationFlags & TextureCreationFlags.DSV) != 0) || (tex.format.compType == FormatComponentType.Depth);

            if (dsv &&
                (string)channels.SelectedItem != "Custom")
            {
                customRed.Visible = false;
                customGreen.Visible = false;
                customBlue.Visible = false;
                customAlpha.Visible = false;
                mulLabel.Visible = false;
                hdrMul.Visible = false;
                customShader.Visible = false;
                customCreate.Visible = false;
                customEdit.Visible = false;
                customDelete.Visible = false;
                depthStencilToolstrip.Visible = true;

                backcolorPick.Visible = false;
                checkerBack.Visible = false;

                mulSep.Visible = false;

                m_TexDisplay.Red = depthDisplay.Checked;
                m_TexDisplay.Green = stencilDisplay.Checked;
                m_TexDisplay.Blue = false;
                m_TexDisplay.Alpha = false;

                m_TexDisplay.HDRMul = -1.0f;
                if (m_TexDisplay.CustomShader != ResourceId.Null) { m_CurPixelValue = null; m_CurRealValue = null; UI_UpdateStatusText(); }
                m_TexDisplay.CustomShader = ResourceId.Null;
            }
            else if ((string)channels.SelectedItem == "RGBA" || !m_Core.LogLoaded)
            {
                customRed.Visible = true;
                customGreen.Visible = true;
                customBlue.Visible = true;
                customAlpha.Visible = true;
                mulLabel.Visible = false;
                hdrMul.Visible = false;
                customShader.Visible = false;
                customCreate.Visible = false;
                customEdit.Visible = false;
                customDelete.Visible = false;
                depthStencilToolstrip.Visible = false;

                backcolorPick.Visible = true;
                checkerBack.Visible = true;

                mulSep.Visible = false;

                m_TexDisplay.Red = customRed.Checked;
                m_TexDisplay.Green = customGreen.Checked;
                m_TexDisplay.Blue = customBlue.Checked;
                m_TexDisplay.Alpha = customAlpha.Checked;

                m_TexDisplay.HDRMul = -1.0f;
                if (m_TexDisplay.CustomShader != ResourceId.Null) { m_CurPixelValue = null; m_CurRealValue = null; UI_UpdateStatusText(); }
                m_TexDisplay.CustomShader = ResourceId.Null;
            }
            else if ((string)channels.SelectedItem == "RGBM")
            {
                customRed.Visible = true;
                customGreen.Visible = true;
                customBlue.Visible = true;
                customAlpha.Visible = false;
                mulLabel.Visible = true;
                hdrMul.Visible = true;
                customShader.Visible = false;
                customCreate.Visible = false;
                customEdit.Visible = false;
                customDelete.Visible = false;
                depthStencilToolstrip.Visible = false;

                backcolorPick.Visible = false;
                checkerBack.Visible = false;

                mulSep.Visible = true;

                m_TexDisplay.Red = customRed.Checked;
                m_TexDisplay.Green = customGreen.Checked;
                m_TexDisplay.Blue = customBlue.Checked;
                m_TexDisplay.Alpha = false;

                float mul = 32.0f;

                if (!float.TryParse(hdrMul.Text, out mul))
                    hdrMul.Text = mul.ToString();

                m_TexDisplay.HDRMul = mul;
                if (m_TexDisplay.CustomShader != ResourceId.Null) { m_CurPixelValue = null; m_CurRealValue = null; UI_UpdateStatusText(); }
                m_TexDisplay.CustomShader = ResourceId.Null;
            }
            else if ((string)channels.SelectedItem == "Custom")
            {
                customRed.Visible = true;
                customGreen.Visible = true;
                customBlue.Visible = true;
                customAlpha.Visible = true;
                mulLabel.Visible = false;
                hdrMul.Visible = false;
                customShader.Visible = true;
                customCreate.Visible = true;
                customEdit.Visible = true;
                customDelete.Visible = true;
                depthStencilToolstrip.Visible = false;

                backcolorPick.Visible = false;
                checkerBack.Visible = false;

                mulSep.Visible = false;

                m_TexDisplay.Red = customRed.Checked;
                m_TexDisplay.Green = customGreen.Checked;
                m_TexDisplay.Blue = customBlue.Checked;
                m_TexDisplay.Alpha = customAlpha.Checked;

                m_TexDisplay.HDRMul = -1.0f;

                m_TexDisplay.CustomShader = ResourceId.Null;
                if (m_CustomShaders.ContainsKey(customShader.Text.ToUpperInvariant()))
                {
                    if (m_TexDisplay.CustomShader == ResourceId.Null) { m_CurPixelValue = null; m_CurRealValue = null; UI_UpdateStatusText(); }
                    m_TexDisplay.CustomShader = m_CustomShaders[customShader.Text.ToUpperInvariant()];
                    customDelete.Enabled = customEdit.Enabled = true;
                }
                else
                {
                    customDelete.Enabled = customEdit.Enabled = false;
                }
            }

            m_TexDisplay.FlipY = flip_y.Checked;

            channelStrip.ResumeLayout();

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
        }

        private void RT_UpdateAndDisplay(ReplayRenderer r)
        {
            if (m_Output == null) return;

            m_Output.SetTextureDisplay(m_TexDisplay);

            render.Invalidate();
        }

        private void DrawCheckerboard(Graphics g, Rectangle rect)
        {
            int numX = (int)Math.Ceiling((float)rect.Width / 64.0f);
            int numY = (int)Math.Ceiling((float)rect.Height / 64.0f);

            Brush dark = new SolidBrush(Color.FromArgb((int)(255 * darkBack.x),
                                                       (int)(255 * darkBack.y),
                                                       (int)(255 * darkBack.z)));

            Brush light = new SolidBrush(Color.FromArgb((int)(255 * lightBack.x),
                                                        (int)(255 * lightBack.y),
                                                        (int)(255 * lightBack.z)));

            for (int x = 0; x < numX; x++)
            {
                for (int y = 0; y < numY; y++)
                {
                    var brush = ((x%2) == (y%2)) ? dark : light;
                    g.FillRectangle(brush, x * 64, y * 64, 64, 64);
                }
            }

            dark.Dispose();
            light.Dispose();
        }

        private void pixelContext_Paint(object sender, PaintEventArgs e)
        {
            if (m_Output == null || m_Core.Renderer == null)
            {
                DrawCheckerboard(e.Graphics, pixelContext.DisplayRectangle);
                return;
            }

            m_Core.Renderer.InvokeForPaint("contextpaint", (ReplayRenderer r) => { if (m_Output != null) m_Output.Display(); });
        }

        private void render_Paint(object sender, PaintEventArgs e)
        {
            renderContainer.Invalidate();
            if (m_Output == null || m_Core.Renderer == null)
            {
                DrawCheckerboard(e.Graphics, render.DisplayRectangle);
                return;
            }

            foreach (var prev in rwPanel.Thumbnails)
                if (prev.Unbound) prev.Clear();

            foreach (var prev in roPanel.Thumbnails)
                if (prev.Unbound) prev.Clear();

            m_Core.Renderer.InvokeForPaint("texpaint", (ReplayRenderer r) => { if (m_Output != null) m_Output.Display(); });
        }

        #endregion

        #region Scale Handling

        private FetchTexture FollowingTexture
        {
            get
            {
                if (!m_Core.LogLoaded || m_Core.CurTextures == null) return null;

                ResourceId ID = m_Following.GetResourceId(m_Core);

                if (ID == ResourceId.Null)
                    ID = m_TexDisplay.texid;

                for (int i = 0; i < m_Core.CurTextures.Length; i++)
                {
                    if (m_Core.CurTextures[i].ID == ID)
                    {
                        return m_Core.CurTextures[i];
                    }
                }

                return null;
            }
        }

        private bool CurrentTextureIsLocked
        {
            get
            {
                var dc = renderToolstripContainer.Parent as DockContent;

                return (dc != null && dc.Tag != null);
            }
        }

        private FetchTexture CurrentTexture
        {
            get
            {
                var dc = renderToolstripContainer.Parent as DockContent;

                if (dc != null && dc.Tag != null)
                    return dc.Tag as FetchTexture;

                return FollowingTexture;
            }
        }

        private UInt32 CurrentTexDisplayWidth
        {
            get
            {
                if (CurrentTexture == null)
                    return 1;

                return CurrentTexture.width;
            }
        }
        private UInt32 CurrentTexDisplayHeight
        {
            get
            {
                if (CurrentTexture == null)
                    return 1;

                if (CurrentTexture.dimension == 1)
                    return 100;

                return CurrentTexture.height;
            }
        }

        private bool FitToWindow
        {
            get
            {
                return fitToWindow.Checked;
            }

            set
            {
                if (!FitToWindow && value)
                {
                    fitToWindow.Checked = true;
                }
                else if (FitToWindow && !value)
                {
                    fitToWindow.Checked = false;
                    float curScale = m_TexDisplay.scale;
                    zoomOption.SelectedText = "";
                    CurrentZoomValue = curScale;
                }
            }
        }

        private float GetFitScale()
        {
            if (CurrentTexture == null)
                return 1.0f;
            float xscale = (float)render.Width / (float)CurrentTexDisplayWidth;
            float yscale = (float)render.Height / (float)CurrentTexDisplayHeight;
            return Math.Min(xscale, yscale);
        }

        private void UI_UpdateFittedScale()
        {
            if (FitToWindow)
                UI_SetScale(1.0f);
        }

        private void UI_SetScale(float s)
        {
            UI_SetScale(s, render.ClientRectangle.Width / 2, render.ClientRectangle.Height / 2);
        }

        bool ScrollUpdateScrollbars = true;

        float CurMaxScrollX
        {
            get
            {
                return render.Width - CurrentTexDisplayWidth * m_TexDisplay.scale;
            }
        }

        float CurMaxScrollY
        {
            get
            {
                return render.Height - CurrentTexDisplayHeight * m_TexDisplay.scale;
            }
        }

        Point ScrollPosition
        {
            get
            {
                return new Point((int)m_TexDisplay.offx, (int)m_TexDisplay.offy);
            }

            set
            {
                m_TexDisplay.offx = Math.Max(CurMaxScrollX, value.X);
                m_TexDisplay.offy = Math.Max(CurMaxScrollY, value.Y);

                m_TexDisplay.offx = Math.Min(0.0f, (float)m_TexDisplay.offx);
                m_TexDisplay.offy = Math.Min(0.0f, (float)m_TexDisplay.offy);

                if (ScrollUpdateScrollbars)
                {
                    if (renderHScroll.Enabled)
                    {
                        // this is so stupid.
                        float actualMaximum = (float)(renderHScroll.Maximum - renderHScroll.LargeChange);
                        float delta = m_TexDisplay.offx / (float)CurMaxScrollX;
                        renderHScroll.Value = (int)Helpers.Clamp( (int)(delta * actualMaximum), 0, renderHScroll.Maximum);
                    }

                    if (renderVScroll.Enabled)
                    {
                        // really. So stupid.
                        float actualMaximum = (float)(renderVScroll.Maximum - renderVScroll.LargeChange);
                        float delta = m_TexDisplay.offy / (float)CurMaxScrollY;
                        renderVScroll.Value = (int)Helpers.Clamp((int)(delta * actualMaximum), 0, renderVScroll.Maximum);
                    }
                }

                m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
            }
        }

        private void UI_SetScale(float s, int x, int y)
        {
            if (FitToWindow)
                s = GetFitScale();

            float prevScale = m_TexDisplay.scale;

            m_TexDisplay.scale = Math.Max(0.1f, Math.Min(256.0f, s));

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);

            float scaleDelta = (m_TexDisplay.scale / prevScale);

            Point newPos = ScrollPosition;

            newPos -= new Size(x, y);
            newPos = new Point((int)(newPos.X * scaleDelta), (int)(newPos.Y * scaleDelta));
            newPos += new Size(x, y);

            ScrollPosition = newPos;

            CurrentZoomValue = m_TexDisplay.scale;

            CalcScrollbars();
        }

        private void CalcScrollbars()
        {
            if (Math.Floor(CurrentTexDisplayWidth * m_TexDisplay.scale) <= render.Width)
            {
                renderHScroll.Enabled = false;
            }
            else
            {
                renderHScroll.Enabled = true;

                renderHScroll.Maximum = (int)Math.Ceiling(CurrentTexDisplayWidth * m_TexDisplay.scale - (float)render.Width);
                renderHScroll.LargeChange = Math.Max(1, renderHScroll.Maximum/6);
            }

            if (Math.Floor(CurrentTexDisplayHeight * m_TexDisplay.scale) <= render.Height)
            {
                renderVScroll.Enabled = false;
            }
            else
            {
                renderVScroll.Enabled = true;

                renderVScroll.Maximum = (int)Math.Ceiling(CurrentTexDisplayHeight * m_TexDisplay.scale - (float)render.Height);
                renderVScroll.LargeChange = Math.Max(1, renderVScroll.Maximum / 6);
            }
        }

        private void render_Layout(object sender, LayoutEventArgs e)
        {
            UI_UpdateFittedScale();
            CalcScrollbars();

            renderContainer.Invalidate();
        }

        #endregion

        #region Mouse movement and scrolling

        private Point m_DragStartScroll = Point.Empty;
        private Point m_DragStartPos = Point.Empty;

        private Point m_CurHoverPixel = Point.Empty;
        private Point m_PickedPoint = Point.Empty;

        private PixelValue m_CurRealValue = null;
        private PixelValue m_CurPixelValue = null;
        private PixelValue m_CurHoverValue = null;

        private void RT_UpdateHoverColour(PixelValue v)
        {
            m_CurHoverValue = v;

            this.BeginInvoke(new Action(UI_UpdateStatusText));
        }

        private void RT_PickPixelsAndUpdate(int x, int y, bool ctx)
        {
            FetchTexture tex = CurrentTexture;

            if (tex == null) return;

            if(ctx)
                m_Output.SetPixelContextLocation((UInt32)x, (UInt32)y);

            if (m_TexDisplay.FlipY)
                y = (int)(tex.height - 1) - y;

            var pickValue = m_Output.PickPixel(m_TexDisplay.texid, true, (UInt32)x, (UInt32)y,
                                                    m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx);
            PixelValue realValue = null;
            if (m_TexDisplay.CustomShader != ResourceId.Null)
                realValue = m_Output.PickPixel(m_TexDisplay.texid, false, (UInt32)x, (UInt32)y,
                                                    m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx);

            RT_UpdatePixelColour(pickValue, realValue, false);
        }

        private void RT_UpdatePixelColour(PixelValue withCustom, PixelValue realValue, bool UpdateHover)
        {
            m_CurPixelValue = withCustom;
            if (UpdateHover)
                m_CurHoverValue = withCustom;
            m_CurRealValue = realValue;

            this.BeginInvoke(new Action(UI_UpdateStatusText));
        }

        private void ShowGotoPopup()
        {
            if (CurrentTexture != null)
            {
                Point p = m_PickedPoint;

                uint mipHeight = Math.Max(1, CurrentTexture.height >> (int)m_TexDisplay.mip);

                if (m_Core.APIProps.pipelineType == GraphicsAPI.OpenGL)
                    p.Y = (int)(mipHeight - 1) - p.Y;
                if (m_TexDisplay.FlipY)
                    p.Y = (int)(mipHeight - 1) - p.Y;

                m_Goto.Show(render, p);
            }
        }

        private void render_KeyDown(object sender, KeyEventArgs e)
        {
            bool nudged = false;

            FetchTexture tex = CurrentTexture;

            if (tex == null) return;
            
            if (e.KeyCode == Keys.C && e.Control)
            {
                try
                {
                    Clipboard.SetText(texStatusDim.Text + " | " + statusLabel.Text);
                }
                catch (System.Exception)
                {
                    try
                    {
                        Clipboard.SetDataObject(texStatusDim.Text + " | " + statusLabel.Text);
                    }
                    catch (System.Exception)
                    {
                        // give up!
                    }
                }
            }

            if (!m_Core.LogLoaded) return;

            if (e.KeyCode == Keys.G && e.Control)
            {
                ShowGotoPopup();

            }

            int increment = 1 << (int)m_TexDisplay.mip;

            if (e.KeyCode == Keys.Up && m_PickedPoint.Y > 0)
            {
                m_PickedPoint = new Point(m_PickedPoint.X, m_PickedPoint.Y - increment);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Down && m_PickedPoint.Y < tex.height-1)
            {
                m_PickedPoint = new Point(m_PickedPoint.X, m_PickedPoint.Y + increment);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Left && m_PickedPoint.X > 0)
            {
                m_PickedPoint = new Point(m_PickedPoint.X - increment, m_PickedPoint.Y);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Right && m_PickedPoint.X < tex.width - 1)
            {
                m_PickedPoint = new Point(m_PickedPoint.X + increment, m_PickedPoint.Y);
                nudged = true;
            }

            if(nudged)
            {
                m_PickedPoint = new Point(
                    Helpers.Clamp(m_PickedPoint.X, 0, (int)tex.width-1),
                    Helpers.Clamp(m_PickedPoint.Y, 0, (int)tex.height - 1));
                e.Handled = true;

                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);

                    RT_UpdateAndDisplay(r);
                });

                UI_UpdateStatusText();
            }
        }

        private void renderHScroll_Scroll(object sender, ScrollEventArgs e)
        {
            ScrollUpdateScrollbars = false;

            if (e.Type != ScrollEventType.EndScroll)
            {
                float actualMaximum = (float)(renderHScroll.Maximum - renderHScroll.LargeChange);
                float delta = (float)e.NewValue / actualMaximum;
                ScrollPosition = new Point((int)(CurMaxScrollX * delta), ScrollPosition.Y);
            }

            ScrollUpdateScrollbars = true;
        }

        private void renderVScroll_Scroll(object sender, ScrollEventArgs e)
        {
            ScrollUpdateScrollbars = false;

            if (e.Type != ScrollEventType.EndScroll)
            {
                float actualMaximum = (float)(renderVScroll.Maximum - renderVScroll.LargeChange);
                float delta = (float)e.NewValue / actualMaximum;
                ScrollPosition = new Point(ScrollPosition.X, (int)(CurMaxScrollY * delta) );
            }

            ScrollUpdateScrollbars = true;
        }

        private void render_MouseLeave(object sender, EventArgs e)
        {
            Cursor = Cursors.Default;
        }

        private void render_MouseUp(object sender, MouseEventArgs e)
        {
            Cursor = Cursors.Default;
        }

        private void render_MouseMove(object sender, MouseEventArgs e)
        {
            m_CurHoverPixel = render.PointToClient(Cursor.Position);

            m_CurHoverPixel.X = (int)(((float)m_CurHoverPixel.X - m_TexDisplay.offx) / m_TexDisplay.scale);
            m_CurHoverPixel.Y = (int)(((float)m_CurHoverPixel.Y - m_TexDisplay.offy) / m_TexDisplay.scale);

            if (e.Button == MouseButtons.Right && m_TexDisplay.texid != ResourceId.Null)
            {
                FetchTexture tex = CurrentTexture;

                if (tex != null)
                {
                    m_PickedPoint = m_CurHoverPixel;

                    m_PickedPoint.X = Helpers.Clamp(m_PickedPoint.X, 0, (int)tex.width - 1);
                    m_PickedPoint.Y = Helpers.Clamp(m_PickedPoint.Y, 0, (int)tex.height - 1);

                    m_Core.Renderer.BeginInvoke("PickPixelClick", (ReplayRenderer r) =>
                    {
                        if (m_Output != null)
                            RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                    });
                }

                Cursor = Cursors.Cross;
            }

            if (e.Button == MouseButtons.None && m_TexDisplay.texid != ResourceId.Null)
            {
                FetchTexture tex = CurrentTexture;

                if (tex != null)
                {
                    m_Core.Renderer.BeginInvoke("PickPixelHover", (ReplayRenderer r) =>
                    {
                        if (m_Output != null)
                        {
                            UInt32 y = (UInt32)m_CurHoverPixel.Y;
                            if (m_TexDisplay.FlipY)
                                y = (uint)(tex.height - 1) - y;
                            RT_UpdateHoverColour(m_Output.PickPixel(m_TexDisplay.texid, true, (UInt32)m_CurHoverPixel.X, y,
                                                                      m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx));
                        }
                    });
                }
            }

            Panel p = renderContainer;

            Point curpos = Cursor.Position;

            if (e.Button == MouseButtons.Left)
            {
                if (Math.Abs(m_DragStartPos.X - curpos.X) > p.HorizontalScroll.SmallChange ||
                    Math.Abs(m_DragStartPos.Y - curpos.Y) > p.VerticalScroll.SmallChange)
                {
                    ScrollPosition = new Point(m_DragStartScroll.X + (curpos.X - m_DragStartPos.X),
                                               m_DragStartScroll.Y + (curpos.Y - m_DragStartPos.Y));
                }

                Cursor = Cursors.NoMove2D;
            }

            if (e.Button != MouseButtons.Left && e.Button != MouseButtons.Right)
            {
                Cursor = Cursors.Default;
            }

            UI_UpdateStatusText();
        }

        private void pixelContext_MouseClick(object sender, MouseEventArgs e)
        {
            pixelContext.Focus();

            if (!m_Core.LogLoaded)
                return;

            if (e.Button == MouseButtons.Right)
            {
                pixelContextMenu.Show(pixelContext, e.Location);
            }
        }

        private void render_MouseClick(object sender, MouseEventArgs e)
        {
            render.Focus();

            if (e.Button == MouseButtons.Right)
            {
                FetchTexture tex = CurrentTexture;

                if (tex != null)
                {
                    m_PickedPoint = m_CurHoverPixel;

                    m_PickedPoint.X = Helpers.Clamp(m_PickedPoint.X, 0, (int)tex.width - 1);
                    m_PickedPoint.Y = Helpers.Clamp(m_PickedPoint.Y, 0, (int)tex.height - 1);

                    m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                    {
                        if (m_Output != null)
                            RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                    });
                }

                Cursor = Cursors.Cross;
            }

            if (e.Button == MouseButtons.Left)
            {
                m_DragStartPos = Cursor.Position;
                m_DragStartScroll = ScrollPosition;

                Cursor = Cursors.NoMove2D;
            }
        }

        private void render_MouseWheel(object sender, MouseEventArgs e)
        {
            Point cursorPos = renderContainer.PointToClient(Cursor.Position);

            FitToWindow = false;

            // scroll in logarithmic scale
            double logScale = Math.Log(m_TexDisplay.scale);
            logScale += e.Delta / 2500.0;
            UI_SetScale((float)Math.Exp(logScale), cursorPos.X, cursorPos.Y);

            ((HandledMouseEventArgs)e).Handled = true;
        }

        #endregion

        #region Texture Display Options

        private float CurrentZoomValue
        {
            get
            {
                if (FitToWindow)
                    return m_TexDisplay.scale;

                int zoom = 100;
                Int32.TryParse(zoomOption.Text.ToString().Replace('%', ' '), out zoom);
                return (float)(zoom) / 100.0f;
            }

            set
            {
                if(!zoomOption.IsDisposed)
                    zoomOption.Text = (Math.Ceiling(value * 100)).ToString() + "%";
            }
        }

        private void zoomOption_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                string txt = zoomOption.Text;
                FitToWindow = false;
                zoomOption.Text = txt;

                UI_SetScale(CurrentZoomValue);
            }
        }

        private void zoomOption_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                string txt = zoomOption.Text;
                FitToWindow = false;
                zoomOption.Text = txt;

                UI_SetScale(CurrentZoomValue);
            }
        }

        private void zoomOption_SelectedIndexChanged(object sender, EventArgs e)
        {
            if ((zoomOption.Focused || zoomOption.ContentRectangle.Contains(Cursor.Position))
                && zoomOption.SelectedItem != null)
            {
                var item = zoomOption.SelectedItem.ToString();

                FitToWindow = false;

                zoomOption.Text = item;

                UI_SetScale(CurrentZoomValue);
            }
        }

        private void zoomOption_DropDownClosed(object sender, EventArgs e)
        {
            if (zoomOption.SelectedItem != null)
            {
                var item = zoomOption.SelectedItem.ToString();

                FitToWindow = false;

                zoomOption.Text = item;

                UI_SetScale(CurrentZoomValue);
            }
        }


        private void fitToWindow_CheckedChanged(object sender, EventArgs e)
        {
            UI_UpdateFittedScale();
        }

        private void zoomExactSize_Click(object sender, EventArgs e)
        {
            fitToWindow.Checked = false;
            UI_SetScale(1.0f);
        }

        private void backcolorPick_Click(object sender, EventArgs e)
        {
            var result = colorDialog.ShowDialog();

            if (result == DialogResult.OK || result == DialogResult.Yes)
            {
                darkBack = lightBack = m_TexDisplay.darkBackgroundColour =
                    m_TexDisplay.lightBackgroundColour = new FloatVector(
                        ((float)colorDialog.Color.R) / 255.0f,
                        ((float)colorDialog.Color.G) / 255.0f,
                        ((float)colorDialog.Color.B) / 255.0f);

                backcolorPick.Checked = true;
                checkerBack.Checked = false;
            }

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);

            if (m_Output == null)
            {
                render.Invalidate();
                pixelContext.Invalidate();
            }
        }

        private void checkerBack_Click(object sender, EventArgs e)
        {
            var defaults = new TextureDisplay();

            darkBack = m_TexDisplay.darkBackgroundColour = defaults.darkBackgroundColour;
            lightBack = m_TexDisplay.lightBackgroundColour = defaults.lightBackgroundColour;

            backcolorPick.Checked = false;
            checkerBack.Checked = true;

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);

            if (m_Output == null)
            {
                render.Invalidate();
                pixelContext.Invalidate();
            }
        }
        private void mipLevel_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (CurrentTexture == null) return;

            uint prevSlice = m_TexDisplay.sliceFace;

            if (CurrentTexture.mips > 1)
            {
                m_TexDisplay.mip = (UInt32)mipLevel.SelectedIndex;
                m_TexDisplay.sampleIdx = 0;
            }
            else
            {
                m_TexDisplay.mip = 0;
                m_TexDisplay.sampleIdx = (UInt32)mipLevel.SelectedIndex;
                if (mipLevel.SelectedIndex == CurrentTexture.msSamp)
                    m_TexDisplay.sampleIdx = ~0U;
            }

            // For 3D textures, update the slice list for this mip
            if(CurrentTexture.depth > 1)
            {
                uint newSlice = prevSlice >> (int)m_TexDisplay.mip;

                UInt32 numSlices = Math.Max(1, CurrentTexture.depth >> (int)m_TexDisplay.mip);

                sliceFace.Items.Clear();

                for (UInt32 i = 0; i < numSlices; i++)
                    sliceFace.Items.Add("Slice " + i);

                // changing sliceFace index will handle updating range & re-picking
                sliceFace.SelectedIndex = Helpers.Clamp((int)newSlice, 0, sliceFace.Items.Count-1);

                return;
            }

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);

            if (m_Output != null && m_PickedPoint.X >= 0 && m_PickedPoint.Y >= 0)
            {
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                });
            }
        }

        private void overlay_SelectedIndexChanged(object sender, EventArgs e)
        {
            m_TexDisplay.overlay = TextureDisplayOverlay.None;

            if (overlay.SelectedIndex > 0)
                m_TexDisplay.overlay = (TextureDisplayOverlay)overlay.SelectedIndex;

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
        }

        private void sliceFace_SelectedIndexChanged(object sender, EventArgs e)
        {
            m_TexDisplay.sliceFace = (UInt32)sliceFace.SelectedIndex;

            if (CurrentTexture.depth > 1)
                m_TexDisplay.sliceFace = (UInt32)(sliceFace.SelectedIndex << (int)m_TexDisplay.mip);

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);

            if (m_Output != null && m_PickedPoint.X >= 0 && m_PickedPoint.Y >= 0)
            {
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                });
            }

        }

        private void updateChannelsHandler(object sender, EventArgs e)
        {
            UI_UpdateChannels();
        }

        private void channelButton_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right && sender is ToolStripButton)
            {
                bool checkd = false;

                var butts = new ToolStripButton[] { customRed, customGreen, customBlue, customAlpha };

                foreach (var b in butts)
                {
                    if(b.Checked && b != sender)
                        checkd = true;
                    if(!b.Checked && b == sender)
                        checkd = true;
                }

                customRed.Checked = !checkd;
                customGreen.Checked = !checkd;
                customBlue.Checked = !checkd;
                customAlpha.Checked = !checkd;
                (sender as ToolStripButton).Checked = checkd;
            }
        }

        bool norangePaint = false;
        Thread rangePaintThread = null;

        void rangeHistogram_RangeUpdated(object sender, Controls.RangeHistogramEventArgs e)
        {
            m_TexDisplay.rangemin = e.BlackPoint;
            m_TexDisplay.rangemax = e.WhitePoint;

            rangeBlack.Text = Formatter.Format(e.BlackPoint);
            rangeWhite.Text = Formatter.Format(e.WhitePoint);

            if (rangePaintThread != null &&
                rangePaintThread.ThreadState != ThreadState.Aborted &&
                rangePaintThread.ThreadState != ThreadState.Stopped)
            {
                return;
            }

            if (norangePaint)
                return;

            rangePaintThread = Helpers.NewThread(new ThreadStart(() =>
            {
                m_Core.Renderer.InvokeForPaint("", (ReplayRenderer r) => { RT_UpdateAndDisplay(r); if (m_Output != null) m_Output.Display(); });
                Thread.Sleep(8);
            }));
            rangePaintThread.Start();
        }

        #endregion

        #region Handlers

        private void zoomRange_Click(object sender, EventArgs e)
        {
            float black = rangeHistogram.BlackPoint;
            float white = rangeHistogram.WhitePoint;

            autoFit.Checked = false;

            rangeHistogram.SetRange(black, white);

            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
        }

        private void reset01_Click(object sender, EventArgs e)
        {
            UI_SetHistogramRange(CurrentTexture, m_TexDisplay.typeHint);

            autoFit.Checked = false;

            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
        }

        private void autoFit_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                autoFit.Checked = !autoFit.Checked;

                if (autoFit.Checked)
                    AutoFitRange();
            }
        }

        private void autoFit_Click(object sender, EventArgs e)
        {
            AutoFitRange();
        }

        private void AutoFitRange()
        {
            // no log loaded or buffer/empty texture currently being viewed - don't autofit
            if (!m_Core.LogLoaded || CurrentTexture == null || m_Output == null)
                return;

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                PixelValue min, max;
                m_Output.GetMinMax(out min, out max);

                {
                    float minval = float.MaxValue;
                    float maxval = -float.MaxValue;

                    bool changeRange = false;

                    ResourceFormat fmt = CurrentTexture.format;

                    if (m_TexDisplay.CustomShader != ResourceId.Null)
                    {
                        fmt.compType = FormatComponentType.Float;
                    }

                    for (int i = 0; i < 4; i++)
                    {
                        if (fmt.compType == FormatComponentType.UInt)
                        {
                            min.value.f[i] = min.value.u[i];
                            max.value.f[i] = max.value.u[i];
                        }
                        else if (fmt.compType == FormatComponentType.SInt)
                        {
                            min.value.f[i] = min.value.i[i];
                            max.value.f[i] = max.value.i[i];
                        }
                    }

                    if (m_TexDisplay.Red)
                    {
                        minval = Math.Min(minval, min.value.f[0]);
                        maxval = Math.Max(maxval, max.value.f[0]);
                        changeRange = true;
                    }
                    if (m_TexDisplay.Green && fmt.compCount > 1)
                    {
                        minval = Math.Min(minval, min.value.f[1]);
                        maxval = Math.Max(maxval, max.value.f[1]);
                        changeRange = true;
                    }
                    if (m_TexDisplay.Blue && fmt.compCount > 2)
                    {
                        minval = Math.Min(minval, min.value.f[2]);
                        maxval = Math.Max(maxval, max.value.f[2]);
                        changeRange = true;
                    }
                    if (m_TexDisplay.Alpha && fmt.compCount > 3)
                    {
                        minval = Math.Min(minval, min.value.f[3]);
                        maxval = Math.Max(maxval, max.value.f[3]);
                        changeRange = true;
                    }

                    if (changeRange)
                    {
                        this.BeginInvoke(new Action(() =>
                        {
                            rangeHistogram.SetRange(minval, maxval);
                            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
                        }));
                    }
                }
            });
        }

        private bool m_Visualise = false;

        private void visualiseRange_CheckedChanged(object sender, EventArgs e)
        {
            if (visualiseRange.Checked)
            {
                rangeHistogram.MinimumSize = new Size(300, 90);

                m_Visualise = true;
                m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
            }
            else
            {
                m_Visualise = false;
                rangeHistogram.MinimumSize = new Size(200, 20);

                rangeHistogram.HistogramData = null;
            }
        }

        private void RT_UpdateVisualRange(ReplayRenderer r)
        {
            if (!m_Visualise || CurrentTexture == null || m_Output == null) return;

            ResourceFormat fmt = CurrentTexture.format;

            if (m_TexDisplay.CustomShader != ResourceId.Null)
                fmt.compCount = 4;

            uint[] histogram;
            m_Output.GetHistogram(rangeHistogram.RangeMin, rangeHistogram.RangeMax,
                                     m_TexDisplay.Red,
                                     m_TexDisplay.Green && fmt.compCount > 1,
                                     m_TexDisplay.Blue && fmt.compCount > 2,
                                     m_TexDisplay.Alpha && fmt.compCount > 3,
                                     out histogram);

            {
                this.BeginInvoke(new Action(() =>
                {
                    rangeHistogram.SetHistogramRange(rangeHistogram.RangeMin, rangeHistogram.RangeMax);
                    rangeHistogram.HistogramData = histogram;
                }));
            }
        }

        bool rangePoint_Dirty = false;

        private void rangePoint_Changed(object sender, EventArgs e)
        {
            rangePoint_Dirty = true;
        }

        private void rangePoint_Update()
        {
            float black = rangeHistogram.BlackPoint;
            float white = rangeHistogram.WhitePoint;

            float.TryParse(rangeBlack.Text, out black);
            float.TryParse(rangeWhite.Text, out white);

            rangeHistogram.SetRange(black, white);

            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);
        }

        private void rangePoint_Leave(object sender, EventArgs e)
        {
            if (!rangePoint_Dirty) return;

            rangePoint_Update();

            rangePoint_Dirty = false;
        }

        private void rangePoint_KeyPress(object sender, KeyPressEventArgs e)
        {
            // escape key
            if (e.KeyChar == '\0')
            {
                rangePoint_Dirty = false;
                rangeHistogram.SetRange(rangeHistogram.BlackPoint, rangeHistogram.WhitePoint);
            }
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                rangePoint_Update();
            }
        }

        private void tabContextMenu_Opening(object sender, CancelEventArgs e)
        {
            if (tabContextMenu.SourceControl == m_PreviewPanel.Pane.TabStripControl)
            {
                int idx = m_PreviewPanel.Pane.TabStripControl.Tabs.IndexOf(m_PreviewPanel.Pane.ActiveContent);

                if (idx == -1)
                    e.Cancel = true;

                if (m_PreviewPanel.Pane.ActiveContent == m_PreviewPanel)
                    closeTab.Enabled = false;
                else
                    closeTab.Enabled = true;
            }
        }

        private void closeTab_Click(object sender, EventArgs e)
        {
            if (tabContextMenu.SourceControl == m_PreviewPanel.Pane.TabStripControl)
            {
                if (m_PreviewPanel.Pane.ActiveContent != m_PreviewPanel)
                {
                    (m_PreviewPanel.Pane.ActiveContent as DockContent).Close();
                }
            }
        }

        private void closeOtherTabs_Click(object sender, EventArgs e)
        {
            if (tabContextMenu.SourceControl == m_PreviewPanel.Pane.TabStripControl)
            {
                IDockContent active = m_PreviewPanel.Pane.ActiveContent;

                var tabs = m_PreviewPanel.Pane.TabStripControl.Tabs;

                for(int i=0; i < tabs.Count; i++)
                {
                    if (tabs[i].Content != active && tabs[i].Content != m_PreviewPanel)
                    {
                        (tabs[i].Content as DockContent).Close();
                        i--;
                    }
                }

                (active as DockContent).Show();
            }
        }

        private void closeTabsToRight_Click(object sender, EventArgs e)
        {
            if (tabContextMenu.SourceControl == m_PreviewPanel.Pane.TabStripControl)
            {
                int idx = m_PreviewPanel.Pane.TabStripControl.Tabs.IndexOf(m_PreviewPanel.Pane.ActiveContent);

                var tabs = m_PreviewPanel.Pane.TabStripControl.Tabs;

                while (tabs.Count > idx+1)
                {
                    (m_PreviewPanel.Pane.TabStripControl.Tabs[idx + 1].Content as DockContent).Close();
                }
            }
        }

        private void TextureViewer_Resize(object sender, EventArgs e)
        {
            render.Invalidate();
        }

        private void pixelHistory_Click(object sender, EventArgs e)
        {
            PixelModification[] history = null;

            int x = m_PickedPoint.X >> (int)m_TexDisplay.mip;
            int y = m_PickedPoint.Y >> (int)m_TexDisplay.mip;

            PixelHistoryView hist = new PixelHistoryView(m_Core, CurrentTexture, new Point(x, y), m_TexDisplay.sampleIdx,
                                                         m_TexDisplay.rangemin, m_TexDisplay.rangemax,
                                                         new bool[] { m_TexDisplay.Red, m_TexDisplay.Green, m_TexDisplay.Blue, m_TexDisplay.Alpha });

            hist.Show(DockPanel);

            // add a short delay so that controls repainting after a new panel appears can get at the
            // render thread before we insert the long blocking pixel history task
            var delayedHistory = new BackgroundWorker();
            delayedHistory.DoWork += delegate
            {
                Thread.Sleep(100);
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    history = r.PixelHistory(CurrentTexture.ID,
                        (UInt32)x, (UInt32)y,
                        m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx,
                        m_TexDisplay.typeHint);

                    this.BeginInvoke(new Action(() =>
                    {
                        hist.SetHistory(history);
                    }));
                });
            };
            delayedHistory.RunWorkerAsync();
        }

        private void debugPixel_Click(object sender, EventArgs e)
        {
            ShaderDebugTrace trace = null;

            ShaderReflection shaderDetails = m_Core.CurPipelineState.GetShaderReflection(ShaderStageType.Pixel);

            if(m_PickedPoint.X < 0 || m_PickedPoint.Y < 0)
                return;

            int x = m_PickedPoint.X >> (int)m_TexDisplay.mip;
            int y = m_PickedPoint.Y >> (int)m_TexDisplay.mip;

            m_Core.Renderer.Invoke((ReplayRenderer r) =>
            {
                trace = r.DebugPixel((UInt32)x, (UInt32)y, m_TexDisplay.sampleIdx, uint.MaxValue);
            });

            if (trace == null || trace.states.Length == 0)
            {
                // if we couldn't debug the pixel on this event, open up a pixel history
                pixelHistory_Click(sender, e);
                return;
            }

            this.BeginInvoke(new Action(() =>
            {
                string debugContext = String.Format("Pixel {0},{1}", x, y);

                ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, ShaderStageType.Pixel, trace, debugContext);

                s.Show(this.DockPanel);
            }));
        }

        private void viewTexBuffer_Click(object sender, EventArgs e)
        {
            if (CurrentTexture != null)
            {
                var viewer = new BufferViewer(m_Core, false);
                viewer.ViewRawBuffer(false, 0, ulong.MaxValue, CurrentTexture.ID);
                viewer.Show(this.DockPanel);
            }
        }

        private TextureSaveDialog m_SaveDialog = null;

        private void saveTex_Click(object sender, EventArgs e)
        {
            if(CurrentTexture == null)
                return;

            if (m_SaveDialog == null)
                m_SaveDialog = new TextureSaveDialog(m_Core);

            m_SaveDialog.saveData.id = m_TexDisplay.texid;
            m_SaveDialog.saveData.typeHint = m_TexDisplay.typeHint;
            m_SaveDialog.saveData.slice.sliceIndex = (int)m_TexDisplay.sliceFace;
            m_SaveDialog.saveData.mip = (int)m_TexDisplay.mip;

            if(CurrentTexture != null && CurrentTexture.depth > 1)
                m_SaveDialog.saveData.slice.sliceIndex = (int)m_TexDisplay.sliceFace >> (int)m_TexDisplay.mip;

            m_SaveDialog.saveData.channelExtract = -1;
            if (m_TexDisplay.Red && !m_TexDisplay.Green && !m_TexDisplay.Blue && !m_TexDisplay.Alpha)
                m_SaveDialog.saveData.channelExtract = 0;
            if (!m_TexDisplay.Red && m_TexDisplay.Green && !m_TexDisplay.Blue && !m_TexDisplay.Alpha)
                m_SaveDialog.saveData.channelExtract = 1;
            if (!m_TexDisplay.Red && !m_TexDisplay.Green && m_TexDisplay.Blue && !m_TexDisplay.Alpha)
                m_SaveDialog.saveData.channelExtract = 2;
            if (!m_TexDisplay.Red && !m_TexDisplay.Green && !m_TexDisplay.Blue && m_TexDisplay.Alpha)
                m_SaveDialog.saveData.channelExtract = 3;

            m_SaveDialog.saveData.comp.blackPoint = m_TexDisplay.rangemin;
            m_SaveDialog.saveData.comp.whitePoint = m_TexDisplay.rangemax;
            m_SaveDialog.saveData.alphaCol = m_TexDisplay.lightBackgroundColour;
            m_SaveDialog.saveData.alpha = m_TexDisplay.Alpha ? AlphaMapping.Preserve : AlphaMapping.Discard;
            if (m_TexDisplay.Alpha && !checkerBack.Checked) m_SaveDialog.saveData.alpha = AlphaMapping.BlendToColour;
            m_SaveDialog.tex = CurrentTexture;

            if (m_TexDisplay.CustomShader != ResourceId.Null)
            {
                m_Core.Renderer.Invoke((ReplayRenderer r) =>
                {
                    ResourceId id = m_Output.GetCustomShaderTexID();
                    if(id != ResourceId.Null)
                        m_SaveDialog.saveData.id = id;
                });
            }

            if(m_SaveDialog.ShowDialog() == DialogResult.OK)
            { 
                bool ret = false;

                m_Core.Renderer.Invoke((ReplayRenderer r) =>
                {
                    ret = r.SaveTexture(m_SaveDialog.saveData, m_SaveDialog.Filename);
                });

                if(!ret)
                    MessageBox.Show(string.Format("Error saving texture {0}.\n\nCheck diagnostic log in Help menu for more details.", m_SaveDialog.Filename),
                                       "Error saving texture", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void texturefilter_TextChanged(object sender, EventArgs e)
        {
            textureList.FillTextureList(texturefilter.SelectedIndex <= 0 ? texturefilter.Text : "",
                                        texturefilter.SelectedIndex == 1,
                                        texturefilter.SelectedIndex == 2);
        }

        private void texturefilter_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                texturefilter.SelectedIndex = 0;
                texturefilter.Text = "";
            }
        }

        private void clearTexFilter_Click(object sender, EventArgs e)
        {
            texturefilter.SelectedIndex = 0;
            texturefilter.Text = "";
        }

        private void toolstripEnabledChanged(object sender, EventArgs e)
        {
            overlayStrip.Visible = overlayStripEnabled.Checked;
            overlayStrip.Visible = overlayStripEnabled.Checked;
            channelStrip.Visible = channelsStripEnabled.Checked;
            zoomStrip.Visible = zoomStripEnabled.Checked;
            rangeStrip.Visible = rangeStripEnabled.Checked;
        }

        private void mainToolstrips_TopToolStripPanel_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
                toolstripMenu.Show((Control)sender, e.Location);
        }

        private void texListShow_Click(object sender, EventArgs e)
        {
            if (!m_TexlistDockPanel.Visible)
            {
                texturefilter.SelectedIndex = 0;
                texturefilter.Text = "";

                m_TexlistDockPanel.Show();
            }
            else
            {
                m_TexlistDockPanel.Hide();
            }
        }

        void textureList_GoIconClick(object sender, GoIconClickEventArgs e)
        {
            ViewTexture(e.ID, false);
        }

        private void gotoLocationButton_Click(object sender, EventArgs e)
        {
            ShowGotoPopup();
        }

        #endregion

        #region Thumbnail strip

        private void AddResourceUsageEntry(List<ToolStripItem> items, uint start, uint end, ResourceUsage usage)
        {
            ToolStripItem item = null;

            if (start == end)
                item = new ToolStripMenuItem("EID " + start + ": " + usage.Str(m_Core.APIProps.pipelineType));
            else
                item = new ToolStripMenuItem("EID " + start + "-" + end + ": " + usage.Str(m_Core.APIProps.pipelineType));

            item.Click += new EventHandler(resourceContextItem_Click);
            item.Tag = end;
            item.Size = new System.Drawing.Size(180, 22);

            items.Add(item);
        }

        private void OpenResourceContextMenu(ResourceId id, bool thumbStripMenu, Control c, Point p)
        {
            var menuItems = new List<ToolStripItem>();

            int i = 0;
            for (i = 0; i < rightclickMenu.Items.Count; i++)
            {
                menuItems.Add(rightclickMenu.Items[i]);
                if (rightclickMenu.Items[i] == usedStartLabel)
                    break;

                menuItems[i].Visible = thumbStripMenu;
            }

            if (m_Core.CurPipelineState.SupportsBarriers)
            {
                imageInLayoutMenuItem.Visible = true;
                imageInLayoutMenuItem.Text = "Image is in layout " + m_Core.CurPipelineState.GetImageLayout(id);
            }
            else
            {
                imageInLayoutMenuItem.Visible = false;
            }

            if (id != ResourceId.Null)
            {
                usedSep.Visible = true;
                usedStartLabel.Visible = true;
                openNewTab.Visible = true;

                openNewTab.Tag = id;

                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    EventUsage[] usage = r.GetUsage(id);

                    this.BeginInvoke(new Action(() =>
                    {
                        uint start = 0;
                        uint end = 0;
                        ResourceUsage us = ResourceUsage.IndexBuffer;

                        foreach (var u in usage)
                        {
                            if (start == 0)
                            {
                                start = end = u.eventID;
                                us = u.usage;
                                continue;
                            }

                            var curDraw = m_Core.GetDrawcall(u.eventID);

                            bool distinct = false;

                            // if the usage is different from the last, add a new entry,
                            // or if the previous draw link is broken.
                            if (u.usage != us || curDraw == null || curDraw.previous == null)
                            {
                                distinct = true;
                            }
                            else
                            {
                                // otherwise search back through real draws, to see if the
                                // last event was where we were - otherwise it's a new
                                // distinct set of drawcalls and should have a separate
                                // entry in the context menu
                                FetchDrawcall prev = curDraw.previous;

                                while(prev != null && prev.eventID > end)
                                {
                                    if((prev.flags & (DrawcallFlags.Dispatch|DrawcallFlags.Drawcall|DrawcallFlags.CmdList)) == 0)
                                    {
                                        prev = prev.previous;
                                    }
                                    else
                                    {
                                        distinct = true;
                                        break;
                                    }

                                    if(prev == null)
                                        distinct = true;
                                }
                            }

                            if(distinct)
                            {
                                AddResourceUsageEntry(menuItems, start, end, us);
                                start = end = u.eventID;
                                us = u.usage;
                            }

                            end = u.eventID;
                        }

                        if (start != 0)
                            AddResourceUsageEntry(menuItems, start, end, us);

                        rightclickMenu.Items.Clear();
                        rightclickMenu.Items.AddRange(menuItems.ToArray());

                        rightclickMenu.Show(c, p);
                    }));
                });
            }
            else
            {
                usedSep.Visible = false;
                usedStartLabel.Visible = false;
                openNewTab.Visible = false;

                rightclickMenu.Show(c, p);
            }
        }

        private void thumbsLayout_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left && sender is ResourcePreview)
            {
                var id = m_Following.GetResourceId(m_Core);

                if (id != ResourceId.Null)
                    ViewTexture(id, false);
            }
        }

        private void thumbsLayout_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left && sender is ResourcePreview)
            {
                var prev = (ResourcePreview)sender;

                var follow = (Following)prev.Tag;

                foreach (var p in rwPanel.Thumbnails)
                    p.Selected = false;

                foreach (var p in roPanel.Thumbnails)
                    p.Selected = false;

                m_Following = follow;
                prev.Selected = true;

                var id = m_Following.GetResourceId(m_Core);

                if (id != ResourceId.Null)
                {
                    UI_OnTextureSelectionChanged(false);
                    m_PreviewPanel.Show();
                }
            }

            if (e.Button == MouseButtons.Right)
            {
                ResourceId id = ResourceId.Null;

                if (sender is ResourcePreview)
                {
                    var prev = (ResourcePreview)sender;

                    var tagdata = (Following)prev.Tag;

                    id = tagdata.GetResourceId(m_Core);

                    if (id == ResourceId.Null && tagdata == m_Following)
                        id = m_TexDisplay.texid;
                }

                OpenResourceContextMenu(id, true, (Control)sender, e.Location);
            }
        }

        private void textureList_MouseUp(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
                OpenResourceContextMenu(FollowingTexture == null ? ResourceId.Null : FollowingTexture.ID, false,
                                            (Control)sender, e.Location);
        }

        void resourceContextItem_Click(object sender, EventArgs e)
        {
            if (sender is ToolStripItem)
            {
                var c = (ToolStripItem)sender;

                if (c.Tag is uint)
                    m_Core.SetEventID(null, (uint)c.Tag);
                else if (c.Tag is ResourceId)
                    ViewTexture((ResourceId)c.Tag, false);
            }
        }

        private void showDisabled_Click(object sender, EventArgs e)
        {
            showDisabled.Checked = !showDisabled.Checked;

            if (m_Core.LogLoaded)
                OnEventSelected(m_Core.CurEvent);
        }

        private void showEmpty_Click(object sender, EventArgs e)
        {
            showEmpty.Checked = !showEmpty.Checked;

            if (m_Core.LogLoaded)
                OnEventSelected(m_Core.CurEvent);
        }

        private void TextureViewer_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }

        #endregion
   }
}