/******************************************************************************
 * The MIT License (MIT)
 * 
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

        private TextureDisplay m_TexDisplay = new TextureDisplay();

        private ToolStripControlHost depthStencilToolstrip = null;

        private DockContent m_PreviewPanel = null;
        private DockContent m_TexlistDockPanel = null;

        private FileSystemWatcher m_FSWatcher = null;

        private int m_HighWaterStatusLength = 0;

        public enum FollowType { OutputColour, OutputDepth, ReadWriteRes, InputResource }
        struct Following
        {
            public FollowType Type;
            public int index;

            public Following(FollowType t, int i) { Type = t; index = i; }

            public override int GetHashCode()
            {
                return Type.GetHashCode() + index.GetHashCode();
            }
            public override bool Equals(object obj)
            {
                return obj is Following && this == (Following)obj;
            }
            public static bool operator ==(Following s1, Following s2)
            {
                return s1.Type == s2.Type && s1.index == s2.index;
            }
            public static bool operator !=(Following s1, Following s2)
            {
                return !(s1 == s2);
            }

            // todo, implement these better for GL :(
            private static void GetDrawContext(Core core, out bool copy, out bool compute)
            {
                var curDraw = core.CurDrawcall;
                copy = curDraw != null && (curDraw.flags & (DrawcallFlags.Copy | DrawcallFlags.Resolve)) != 0;
                compute = curDraw != null && (curDraw.flags & DrawcallFlags.Dispatch) != 0 &&
                          core.CurPipelineState.GetShader(ShaderStageType.Compute) != ResourceId.Null;
            }

            public int GetHighestMip(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                // TODO find a way (without copy-pasting tons of code)
                // to duplicate the logic of GetResourceId() but return
                // different information
                if (copy || compute)
                    return -1;

                if (core.APIProps.pipelineType == APIPipelineStateType.D3D11)
                {
                    D3D11PipelineState.ShaderStage.ResourceView view = null;

                    if (Type == FollowType.OutputColour)
                    {
                        view = core.CurD3D11PipelineState.m_OM.RenderTargets[index];
                    }
                    else if (Type == FollowType.OutputDepth)
                    {
                        view = core.CurD3D11PipelineState.m_OM.DepthTarget;
                    }
                    else if (Type == FollowType.ReadWriteRes)
                    {
                        if (index >= core.CurD3D11PipelineState.m_OM.UAVStartSlot)
                            view = core.CurD3D11PipelineState.m_OM.UAVs[index];
                    }
                    else if (Type == FollowType.InputResource)
                    {
                        view = core.CurD3D11PipelineState.m_PS.SRVs[index];
                    }

                    return view != null ? (int)view.HighestMip : -1;
                }
                else
                {
                    if (Type == FollowType.OutputColour)
                    {
                        return (int)core.CurGLPipelineState.m_FB.m_DrawFBO.Color[index].Mip;
                    }
                    else if (Type == FollowType.OutputDepth)
                    {
                        return (int)core.CurGLPipelineState.m_FB.m_DrawFBO.Depth.Mip;
                    }
                    else if (Type == FollowType.ReadWriteRes)
                    {
                        return (int)core.CurGLPipelineState.Images[index].Level;
                    }
                }

                return -1;
            }

            public int GetFirstArraySlice(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                // TODO find a way (without copy-pasting tons of code)
                // to duplicate the logic of GetResourceId() but return
                // different information
                if (copy || compute)
                    return -1;

                if (core.APIProps.pipelineType == APIPipelineStateType.D3D11)
                {
                    D3D11PipelineState.ShaderStage.ResourceView view = null;

                    if (Type == FollowType.OutputColour)
                    {
                        view = core.CurD3D11PipelineState.m_OM.RenderTargets[index];
                    }
                    else if (Type == FollowType.OutputDepth)
                    {
                        view = core.CurD3D11PipelineState.m_OM.DepthTarget;
                    }
                    else if (Type == FollowType.ReadWriteRes)
                    {
                        if (index >= core.CurD3D11PipelineState.m_OM.UAVStartSlot)
                            view = core.CurD3D11PipelineState.m_OM.UAVs[index];
                    }
                    else if (Type == FollowType.InputResource)
                    {
                        view = core.CurD3D11PipelineState.m_PS.SRVs[index];
                    }

                    return view != null ? (int)view.FirstArraySlice : -1;
                }
                else
                {
                    if (Type == FollowType.OutputColour)
                    {
                        return (int)core.CurGLPipelineState.m_FB.m_DrawFBO.Color[index].Layer;
                    }
                    else if (Type == FollowType.OutputDepth)
                    {
                        return (int)core.CurGLPipelineState.m_FB.m_DrawFBO.Depth.Layer;
                    }
                    else if (Type == FollowType.ReadWriteRes)
                    {
                        if(!core.CurGLPipelineState.Images[index].Layered)
                            return (int)core.CurGLPipelineState.Images[index].Layer;
                    }
                    else if (Type == FollowType.InputResource)
                    {
                        return (int)core.CurGLPipelineState.Textures[index].FirstSlice;
                    }
                }

                return -1;
            }

            public ResourceId GetResourceId(Core core)
            {
                ResourceId id = ResourceId.Null;

                if (Type == FollowType.OutputColour)
                {
                    var outputs = GetOutputTargets(core);

                    if(index < outputs.Length)
                        id = outputs[index];
                }
                else if (Type == FollowType.OutputDepth)
                {
                    id = GetDepthTarget(core);
                }
                else if (Type == FollowType.ReadWriteRes)
                {
                    var rw = GetReadWriteResources(core);

                    if (index < rw.Length)
                        id = rw[index];
                }
                else if (Type == FollowType.InputResource)
                {
                    var res = GetResources(core);

                    if(index < res.Length)
                        id = res[index];
                }

                return id;
            }

            public static ResourceId[] GetReadWriteResources(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy)
                    return new ResourceId[0];
                else if (compute)
                    return core.CurPipelineState.GetReadWriteResources(ShaderStageType.Compute);
                else
                    return core.CurPipelineState.GetReadWriteResources(ShaderStageType.Pixel);
            }

            public static ResourceId[] GetOutputTargets(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy)
                    return new ResourceId[] { curDraw.copyDestination };
                else if(compute)
                    return new ResourceId[0];
                else
                    return core.CurPipelineState.GetOutputTargets();
            }

            public static ResourceId GetDepthTarget(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy || compute)
                    return ResourceId.Null;
                else
                    return core.CurPipelineState.GetDepthTarget();
            }

            public static ResourceId[] GetResources(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy)
                    return new ResourceId[] { curDraw.copySource };
                else if (compute)
                    return core.CurPipelineState.GetResources(ShaderStageType.Compute);
                else
                    return core.CurPipelineState.GetResources(ShaderStageType.Pixel);
            }

            public static ShaderReflection GetReflection(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy)
                    return null;
                else if (compute)
                    return core.CurPipelineState.GetShaderReflection(ShaderStageType.Compute);
                else
                    return core.CurPipelineState.GetShaderReflection(ShaderStageType.Pixel);
            }

            public static ShaderBindpointMapping GetMapping(Core core)
            {
                var curDraw = core.CurDrawcall;
                bool copy, compute;
                GetDrawContext(core, out copy, out compute);

                if (copy)
                    return null;
                else if (compute)
                    return core.CurPipelineState.GetBindpointMapping(ShaderStageType.Compute);
                else
                    return core.CurPipelineState.GetBindpointMapping(ShaderStageType.Pixel);
            }
        }
        private Following m_Following = new Following(FollowType.OutputColour, 0);

        public class TexSettings
        {
            public TexSettings() { r = g = b = true; a = false; mip = 0; slice = 0; minrange = 0.0f; maxrange = 1.0f; }

            public bool r, g, b, a;
            public bool depth, stencil;
            public int mip, slice;
            public float minrange, maxrange;
        }

        private Dictionary<ResourceId, TexSettings> m_TextureSettings = new Dictionary<ResourceId, TexSettings>();

        #endregion

        public TextureViewer(Core core)
        {
            m_Core = core;

            InitializeComponent();

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

            render.Painting = true;
            pixelContext.Painting = true;

            saveTex.Enabled = false;

            DockHandler.GetPersistStringCallback = PersistString;

            renderContainer.MouseWheelHandler = render_MouseWheel;
            render.MouseWheel += render_MouseWheel;
            renderContainer.MouseDown += render_MouseClick;
            renderContainer.MouseMove += render_MouseMove;

            render.KeyHandler = render_KeyDown;
            pixelContext.KeyHandler = render_KeyDown;

            rangeHistogram.RangeUpdated += new EventHandler<RangeHistogramEventArgs>(rangeHistogram_RangeUpdated);

            this.DoubleBuffered = true;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            channels.SelectedIndex = 0;

            FitToWindow = true;
            overlay.SelectedIndex = 0;
            m_Following = new Following(FollowType.OutputColour, 0);

            texturefilter.SelectedIndex = 0;

            if (m_Core.LogLoaded)
                OnLogfileLoaded();
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

            var w3 = Helpers.WrapDockContent(dockPanel, texPanel, "Inputs");
            w3.DockAreas &= ~DockAreas.Document;
            w3.DockState = DockState.DockRight;
            w3.Show();

            w3.CloseButton = false;
            w3.CloseButtonVisible = false;

            var w5 = Helpers.WrapDockContent(dockPanel, rtPanel, "Outputs");
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

            ApplyPersistData(data);
        }

        private IDockContent GetContentFromPersistString(string persistString)
        {
            Control[] persistors = {
                                       renderToolstripContainer,
                                       texPanel,
                                       rtPanel,
                                       texlistContainer,
                                       pixelContextPanel
                                   };

            foreach(var p in persistors)
                if (persistString == p.Name && p.Parent is IDockContent && (p.Parent as DockContent).DockPanel == null)
                    return p.Parent as IDockContent;

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
                                       texPanel,
                                       rtPanel,
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

                    dockPanel.LoadFromXml(strm, new DeserializeDockContent(GetContentFromPersistString));
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
                    viewer.ViewRawBuffer(true, ID);
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

                if (!m_CustomShaders.ContainsKey(key))
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
                return;
            }

            var path = Path.Combine(Core.ConfigDirectory, customShader.Text + m_Core.APIProps.ShaderExtension);

            string src = "";

            if (m_Core.APIProps.pipelineType == APIPipelineStateType.D3D11)
            {
                src = String.Format(
                    "float4 main(float4 pos : SV_Position, float4 uv : TEXCOORD0) : SV_Target0{0}" +
                    "{{{0}" +
                    "    return float4(0,0,0,1);{0}" +
                    "}}{0}"
                    , Environment.NewLine);
            }
            else if (m_Core.APIProps.pipelineType == APIPipelineStateType.OpenGL)
            {
                src = String.Format(
                    "#version 420 core{0}" +
                    "layout (location = 0) out vec4 color_out;{0}" + 
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

        public void OnLogfileLoaded()
        {
            var outConfig = new OutputConfig();
            outConfig.m_Type = OutputType.TexDisplay;

            saveTex.Enabled = true;

            m_Following = new Following(FollowType.OutputColour, 0);

            rtPanel.ClearThumbnails();
            texPanel.ClearThumbnails();

            m_HighWaterStatusLength = 0;

            IntPtr contextHandle = pixelContext.Handle;
            IntPtr renderHandle = render.Handle;
            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                m_Output = r.CreateOutput(renderHandle);
                m_Output.SetPixelContext(contextHandle);
                m_Output.SetOutputConfig(outConfig);

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

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
        }

        void CustomShaderModified(object sender, FileSystemEventArgs e)
        {
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

            m_PrevArea = 0.0f;
            m_HighWaterStatusLength = 0;

            saveTex.Enabled = false;

            rtPanel.ClearThumbnails();
            texPanel.ClearThumbnails();

            texturefilter.SelectedIndex = 0;

            m_TexDisplay = new TextureDisplay();
            m_TexDisplay.darkBackgroundColour = darkBack;
            m_TexDisplay.lightBackgroundColour = lightBack;

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

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            if (IsDisposed) return;

            UI_OnTextureSelectionChanged();

            ResourceId[] RTs = Following.GetOutputTargets(m_Core);
            ResourceId[] RWs = Following.GetReadWriteResources(m_Core);
            ResourceId Depth = Following.GetDepthTarget(m_Core);
            ResourceId[] Texs = Following.GetResources(m_Core);

            ShaderReflection details = Following.GetReflection(m_Core);
            ShaderBindpointMapping mapping = Following.GetMapping(m_Core);

            var curDraw = m_Core.GetDrawcall(frameID, eventID);
            bool copy = curDraw != null && (curDraw.flags & (DrawcallFlags.Copy|DrawcallFlags.Resolve)) != 0;

            if (m_Output == null) return;

            UI_CreateThumbnails();

            int i = 0;
            for(int rt=0; rt < RTs.Length; rt++)
            {
                ResourcePreview prev;

                if (i < rtPanel.Thumbnails.Length)
                    prev = rtPanel.Thumbnails[i];
                else
                    prev = UI_CreateThumbnail(rtPanel);

                if (RTs[rt] != ResourceId.Null)
                {
                    FetchTexture tex = null;
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == RTs[rt])
                            tex = t;

                    FetchBuffer buf = null;
                    foreach (var b in m_Core.CurBuffers)
                        if (b.ID == RTs[rt])
                            buf = b;

                    string bindName = "";

                    if (copy)
                        bindName = "Destination";

                    if (tex != null)
                    {
                        prev.Init(!tex.customName && bindName.Length > 0 ? bindName : tex.name, tex.width, tex.height, tex.depth, tex.mips);
                        IntPtr handle = prev.ThumbnailHandle;
                        ResourceId id = RTs[rt];
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, id);
                        });
                    }
                    else if (buf != null)
                    {
                        prev.Init(!buf.customName && bindName.Length > 0 ? bindName : buf.name, buf.length, 0, 0, Math.Max(1, buf.structureSize));
                        IntPtr handle = prev.ThumbnailHandle;
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, ResourceId.Null);
                        });
                    }
                    else
                    {
                        prev.Init();
                    }

                    prev.Tag = new Following(FollowType.OutputColour, rt);
                    prev.SlotName = copy ? "DST" : rt.ToString();
                    prev.Visible = true;
                }
                else if (prev.Selected)
                {
                    prev.Init();
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null);
                    });
                }
                else
                {
                    prev.Init();
                    prev.Visible = false;
                }

                i++;
            }

            for (int rw = 0; rw < RWs.Length; rw++)
            {
                ResourcePreview prev;

                if (i < rtPanel.Thumbnails.Length)
                    prev = rtPanel.Thumbnails[i];
                else
                    prev = UI_CreateThumbnail(rtPanel);
                
                FetchTexture tex = null;
                foreach (var t in m_Core.CurTextures)
                    if (t.ID == RWs[rw])
                        tex = t;

                FetchBuffer buf = null;
                foreach (var b in m_Core.CurBuffers)
                    if (b.ID == RWs[rw])
                        buf = b;

                bool used = false;

                string bindName = "";

                if (details != null)
                {
                    foreach (var bind in details.Resources)
                    {
                        if (mapping.Resources[bind.bindPoint].bind == rw && bind.IsReadWrite)
                        {
                            used = true;
                            bindName = "<" + bind.name + ">";
                        }
                    }
                }
                    
                // show if
                if (used || // it's referenced by the shader - regardless of empty or not
                    (showDisabled.Checked && !used && RWs[rw] != ResourceId.Null) || // it's bound, but not referenced, and we have "show disabled"
                    (showEmpty.Checked && RWs[rw] == ResourceId.Null) // it's empty, and we have "show empty"
                    )
                {
                    if (tex != null)
                    {
                        prev.Init(!tex.customName && bindName.Length > 0 ? bindName : tex.name, tex.width, tex.height, tex.depth, tex.mips);
                        IntPtr handle = prev.ThumbnailHandle;
                        ResourceId id = RWs[rw];
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, id);
                        });
                    }
                    else if (buf != null)
                    {
                        prev.Init(!buf.customName && bindName.Length > 0 ? bindName : buf.name, buf.length, 0, 0, Math.Max(1, buf.structureSize));
                        IntPtr handle = prev.ThumbnailHandle;
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, ResourceId.Null);
                        });
                    }
                    else
                    {
                        prev.Init();
                    }

                    prev.Tag = new Following(FollowType.ReadWriteRes, rw);
                    prev.SlotName = "RW" + rw;
                    prev.Visible = true;
                }
                else if (prev.Selected)
                {
                    prev.Init();
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null);
                    });
                }
                else
                {
                    prev.Init();
                    prev.Visible = false;
                }

                i++;
            }


            {
                ResourcePreview prev;

                // depth thumbnail is always the last one
                if (i < rtPanel.Thumbnails.Length)
                {
                    // hide others
                    for (; i < rtPanel.Thumbnails.Length - 1; i++)
                    {
                        rtPanel.Thumbnails[i].Init();
                        rtPanel.Thumbnails[i].Visible = false;
                    }
                    prev = rtPanel.Thumbnails[rtPanel.Thumbnails.Length - 1];
                }
                else
                {
                    prev = UI_CreateThumbnail(rtPanel);
                }

                if (Depth != ResourceId.Null)
                {
                    FetchTexture tex = null;
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == Depth)
                            tex = t;

                    FetchBuffer buf = null;
                    foreach (var b in m_Core.CurBuffers)
                        if (b.ID == Depth)
                            buf = b;

                    if (tex != null)
                    {
                        prev.Init(tex.name, tex.width, tex.height, tex.depth, tex.mips);
                        IntPtr handle = prev.ThumbnailHandle;
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, Depth);
                        });
                    }
                    else if (buf != null)
                    {
                        prev.Init(buf.name, buf.length, 0, 0, Math.Max(1, buf.structureSize));
                        IntPtr handle = prev.ThumbnailHandle;
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, ResourceId.Null);
                        });
                    }
                    else
                    {
                        prev.Init();
                    }

                    prev.Tag = new Following(FollowType.OutputDepth, 0);
                    prev.SlotName = "D";
                    prev.Visible = true;
                }
                else if (prev.Selected)
                {
                    prev.Init();
                    IntPtr handle = prev.ThumbnailHandle;
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null);
                    });
                }
                else
                {
                    prev.Init();
                    prev.Visible = false;
                }
            }

            rtPanel.RefreshLayout();
            
            i = 0;
            for(; i < Texs.Length; i++)
            {
                ResourcePreview prev;

                if (i < texPanel.Thumbnails.Length)
                    prev = texPanel.Thumbnails[i];
                else
                    prev = UI_CreateThumbnail(texPanel);

                bool used = false;

                string bindName = "";

                if (details != null)
                {
                    foreach (var bind in details.Resources)
                    {
                        if (mapping.Resources[bind.bindPoint].bind == i && bind.IsSRV)
                        {
                            used = true;
                            bindName = "<" + bind.name + ">";
                        }
                    }
                }

                if (copy)
                {
                    used = true;
                    bindName = "Source";
                }

                // show if
                if (used || // it's referenced by the shader - regardless of empty or not
                    (showDisabled.Checked && !used && Texs[i] != ResourceId.Null) || // it's bound, but not referenced, and we have "show disabled"
                    (showEmpty.Checked && Texs[i] == ResourceId.Null) // it's empty, and we have "show empty"
                    )
                {
                    FetchTexture tex = null;
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == Texs[i])
                            tex = t;

                    FetchBuffer buf = null;
                    foreach (var b in m_Core.CurBuffers)
                        if (b.ID == Texs[i])
                            buf = b;

                    if (tex != null)
                    {
                        prev.Init(!tex.customName && bindName.Length > 0 ? bindName : tex.name, tex.width, tex.height, tex.depth, tex.mips);
                        IntPtr handle = prev.ThumbnailHandle;
                        ResourceId id = Texs[i];
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, id);
                        });
                    }
                    else if (buf != null)
                    {
                        prev.Init(!buf.customName && bindName.Length > 0 ? bindName : buf.name, buf.length, 0, 0, Math.Max(1, buf.structureSize));
                        IntPtr handle = prev.ThumbnailHandle;
                        m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                        {
                            m_Output.AddThumbnail(handle, ResourceId.Null);
                        });
                    }
                    else
                    {
                        prev.Init();
                    }

                    prev.Tag = new Following(FollowType.InputResource, i);
                    prev.SlotName = copy ? "SRC" : i.ToString();
                    prev.Visible = true;
                }
                else if (prev.Selected)
                {
                    FetchTexture tex = null;
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == Texs[i])
                            tex = t;

                    IntPtr handle = prev.ThumbnailHandle;
                    if (Texs[i] == ResourceId.Null || tex == null)
                        prev.Init();
                    else
                        prev.Init("Unused", tex.width, tex.height, tex.depth, tex.mips);
                    m_Core.Renderer.BeginInvoke((ReplayRenderer rep) =>
                    {
                        m_Output.AddThumbnail(handle, ResourceId.Null);
                    });
                }
                else
                {
                    prev.Init();
                    prev.Visible = false;
                }
            }

            texPanel.RefreshLayout();

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
            if (rtPanel.Thumbnails.Length > 0 || texPanel.Thumbnails.Length > 0) return;

            rtPanel.SuspendLayout();
            texPanel.SuspendLayout();

            // these will expand, but we make sure that there is a good set reserved
            for (int i = 0; i < 9; i++)
            {
                var prev = UI_CreateThumbnail(rtPanel);

                if(i == 0)
                    prev.Selected = true;
            }

            for (int i = 0; i < 128; i++)
                UI_CreateThumbnail(texPanel);

            rtPanel.ResumeLayout();
            texPanel.ResumeLayout();
        }

        private int prevFirstArraySlice = -1;
        private int prevHighestMip = -1;

        private float m_PrevArea = 0.0f;

        private void UI_OnTextureSelectionChanged()
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

                m_TextureSettings[m_TexDisplay.texid].depth = depthDisplay.Checked;
                m_TextureSettings[m_TexDisplay.texid].stencil = stencilDisplay.Checked;

                m_TextureSettings[m_TexDisplay.texid].mip = mipLevel.SelectedIndex;
                m_TextureSettings[m_TexDisplay.texid].slice = sliceFace.SelectedIndex;

                m_TextureSettings[m_TexDisplay.texid].minrange = rangeHistogram.BlackPoint;
                m_TextureSettings[m_TexDisplay.texid].maxrange = rangeHistogram.WhitePoint;
            }

            m_TexDisplay.texid = tex.ID;

            m_CurPixelValue = null;
            m_CurRealValue = null;

            // try to maintain the pan in the new texture. If the new texture
            // is an integer multiple of the old texture, this will keep the
            // top left pixel the same. Due to the difference in scale, the rest
            // of the image will be out though. This is useful for downsample chains and
            // things where you're flipping back and forth between overlapping
            // textures, but really needs a mode where the zoom level is changed
            // to compensate as well.
            float curArea = (float)CurrentTexture.width * (float)CurrentTexture.height;

            if(m_PrevArea > 0.0f)
            {
                float prevX = m_TexDisplay.offx;
                float prevY = m_TexDisplay.offy;

                // this scale factor is arbitrary really, only intention is to have
                // integer scales come out precisely, other 'similar' sizes will be
                // similar ish
                float scaleFactor = (float)(Math.Sqrt(curArea) / Math.Sqrt(m_PrevArea));

                m_TexDisplay.offx = prevX * scaleFactor;
                m_TexDisplay.offy = prevY * scaleFactor;
            }

            m_PrevArea = curArea;

            // refresh scroll position
            ScrollPosition = ScrollPosition;

            UI_UpdateStatusText();

            mipLevel.Items.Clear();
            sliceFace.Items.Clear();

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

                int highestMip = m_Following.GetHighestMip(m_Core);
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

            if (tex.numSubresources == tex.mips && tex.depth <= 1)
            {
                sliceFace.Enabled = false;
            }
            else
            {
                sliceFace.Enabled = true;

                sliceFace.Visible = sliceFaceLabel.Visible = true;

                String[] cubeFaces = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

                UInt32 numSlices = (Math.Max(1, tex.depth) * tex.numSubresources) / tex.mips;

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

                int firstArraySlice = m_Following.GetFirstArraySlice(m_Core);
                // see above with highestMip and prevHighestMip for the logic behind this
                if (firstArraySlice >= 0 && (newtex || firstArraySlice != prevFirstArraySlice))
                {
                    useslicesettings = false;
                    sliceFace.SelectedIndex = Helpers.Clamp(firstArraySlice, 0, (int)numSlices - 1);
                }

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
                    customRed.Checked = true;
                    customGreen.Checked = true;
                    customBlue.Checked = true;
                    customAlpha.Checked = false;

                    stencilDisplay.Checked = false;
                    depthDisplay.Checked = true;

                    norangePaint = true;
                    rangeHistogram.SetRange(0.0f, 1.0f);
                    norangePaint = false;
                }

                // reset the range if desired
                if (m_Core.Config.TextureViewer_ResetRange)
                {
                    rangeHistogram.SetRange(0.0f, 1.0f);
                }
            }

            UI_UpdateFittedScale();

            //render.Width = (int)(CurrentTexDisplayWidth * m_TexDisplay.scale);
            //render.Height = (int)(CurrentTexDisplayHeight * m_TexDisplay.scale);

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
            });
        }

        void dockPanel_ActiveDocumentChanged(object sender, EventArgs e)
        {
            var d = dockPanel.ActiveDocument as DockContent;

            if (d == null) return;

            if (d.Visible)
                d.Controls.Add(renderToolstripContainer);

            UI_OnTextureSelectionChanged();
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
                        case FollowType.ReadWriteRes:
                            m_PreviewPanel.Text = string.Format("Cur RW Output - {0}", name);
                            break;
                        case FollowType.InputResource:
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
                        case FollowType.ReadWriteRes:
                            m_PreviewPanel.Text = string.Format("Cur RW Output");
                            break;
                        case FollowType.InputResource:
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

            bool dsv = ((tex.creationFlags & TextureCreationFlags.DSV) != 0);
            bool uintTex = (tex.format.compType == FormatComponentType.UInt);
            bool sintTex = (tex.format.compType == FormatComponentType.SInt);

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
            if (m_Core.APIProps.pipelineType == APIPipelineStateType.OpenGL)
                y = (int)(tex.height - 1) - y;
            if (m_TexDisplay.FlipY)
                y = (int)(tex.height - 1) - y;

            y = Math.Max(0, y);

            int x = m_CurHoverPixel.X >> (int)m_TexDisplay.mip;
            float invWidth = tex.width > 0 ? 1.0f / tex.width : 0.0f;
            float invHeight = tex.height > 0 ? 1.0f /tex.height : 0.0f;

            string hoverCoords = String.Format("{0,4}, {1,4} ({2:0.0000}, {3:0.0000})", 
                x, y, (x * invWidth), (y * invHeight));

            string statusText = "Hover - " + hoverCoords;

            if (m_CurHoverPixel.X > tex.width || m_CurHoverPixel.Y > tex.height || m_CurHoverPixel.X < 0 || m_CurHoverPixel.Y < 0)
                statusText = "Hover - [" + hoverCoords + "]";

            if (m_CurPixelValue != null)
            {
                x = m_PickedPoint.X >> (int)m_TexDisplay.mip;
                y = m_PickedPoint.Y >> (int)m_TexDisplay.mip;
                if (m_Core.APIProps.pipelineType == APIPipelineStateType.OpenGL)
                    y = (int)(tex.height - 1) - y;
                if (m_TexDisplay.FlipY)
                    y = (int)(tex.height - 1) - y;

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

                m_TexDisplay.linearDisplayAsGamma = gammaDisplay.Checked;
            }

            if (tex != null && (tex.creationFlags & TextureCreationFlags.DSV) > 0 &&
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
                    customCreate.Enabled = false;
                }
                else
                {
                    customDelete.Enabled = customEdit.Enabled = false;
                    customCreate.Enabled = true;
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

            Brush dark = new SolidBrush(Color.FromArgb((int)(255 * Math.Sqrt(Math.Sqrt(darkBack.x))),
                                                       (int)(255 * Math.Sqrt(Math.Sqrt(darkBack.y))),
                                                       (int)(255 * Math.Sqrt(Math.Sqrt(darkBack.z)))));

            Brush light = new SolidBrush(Color.FromArgb((int)(255 * Math.Sqrt(Math.Sqrt(lightBack.x))),
                                                        (int)(255 * Math.Sqrt(Math.Sqrt(lightBack.y))),
                                                        (int)(255 * Math.Sqrt(Math.Sqrt(lightBack.z)))));

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
            if (m_Output == null || m_Core.Renderer == null || PixelPicked == false)
            {
                if (backcolorPick.Checked)
                    e.Graphics.Clear(colorDialog.Color);
                else
                    DrawCheckerboard(e.Graphics, pixelContext.DisplayRectangle);
                return;
            }

            m_Core.Renderer.Invoke((ReplayRenderer r) => { if (m_Output != null) m_Output.Display(); });
        }

        private void render_Paint(object sender, PaintEventArgs e)
        {
            renderContainer.Invalidate();
            if (m_Output == null || m_Core.Renderer == null)
            {
                if (backcolorPick.Checked)
                    e.Graphics.Clear(colorDialog.Color);
                else
                    DrawCheckerboard(e.Graphics, render.DisplayRectangle);
                return;
            }

            foreach (var prev in rtPanel.Thumbnails)
                if (prev.Unbound) prev.Clear();

            foreach (var prev in texPanel.Thumbnails)
                if (prev.Unbound) prev.Clear();

            m_Core.Renderer.Invoke((ReplayRenderer r) => { if (m_Output != null) m_Output.Display(); });
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

            FetchTexture tex = CurrentTexture;

            if (tex == null)
            {
                if(m_Core.LogLoaded)
                    foreach (var t in m_Core.CurTextures)
                        if (t.ID == m_TexDisplay.texid)
                            tex = t;

                if(tex == null)
                    return;
            }

            //render.Width = Math.Min(500, (int)(tex.width * m_TexDisplay.scale));
            //render.Height = Math.Min(500, (int)(tex.height * m_TexDisplay.scale));

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
                y = (int)tex.height - y;

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

            if (e.KeyCode == Keys.Up && m_PickedPoint.Y > 0)
            {
                m_PickedPoint = new Point(m_PickedPoint.X, m_PickedPoint.Y - 1);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Down && m_PickedPoint.Y < tex.height-1)
            {
                m_PickedPoint = new Point(m_PickedPoint.X, m_PickedPoint.Y + 1);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Left && m_PickedPoint.X > 0)
            {
                m_PickedPoint = new Point(m_PickedPoint.X - 1, m_PickedPoint.Y);
                nudged = true;
            }
            else if (e.KeyCode == Keys.Right && m_PickedPoint.X < tex.width - 1)
            {
                m_PickedPoint = new Point(m_PickedPoint.X + 1, m_PickedPoint.Y);
                nudged = true;
            }

            if(nudged)
            {
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

                    m_PickedPoint.X = Helpers.Clamp(m_PickedPoint.X, 0, (int)tex.width-1);
                    m_PickedPoint.Y = Helpers.Clamp(m_PickedPoint.Y, 0, (int)tex.height - 1);

                    m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
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
                    m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                    {
                        if (m_Output != null)
                        {
                            UInt32 y = (UInt32)m_CurHoverPixel.Y;
                            if (m_TexDisplay.FlipY)
                                y = tex.height - y;
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

            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);

            if (m_Output != null && m_PickedPoint.X >= 0 && m_PickedPoint.Y >= 0)
            {
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                });
            }

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
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

            m_Core.Renderer.BeginInvoke(RT_UpdateVisualRange);

            if (m_Output != null && m_PickedPoint.X >= 0 && m_PickedPoint.Y >= 0)
            {
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    if (m_Output != null)
                        RT_PickPixelsAndUpdate(m_PickedPoint.X, m_PickedPoint.Y, true);
                });
            }

            m_Core.Renderer.BeginInvoke(RT_UpdateAndDisplay);
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
                m_Core.Renderer.Invoke((ReplayRenderer r) => { RT_UpdateAndDisplay(r); if (m_Output != null) m_Output.Display(); });
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
            rangeHistogram.SetRange(0.0f, 1.0f);

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
            if (!m_Core.LogLoaded || CurrentTexture == null)
                return;

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                PixelValue min, max;
                bool success = r.GetMinMax(m_TexDisplay.texid, m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx, out min, out max);

                if (success)
                {
                    float minval = float.MaxValue;
                    float maxval = -float.MaxValue;

                    bool changeRange = false;

                    ResourceFormat fmt = CurrentTexture.format;

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
            if (!m_Visualise || CurrentTexture == null) return;

            ResourceFormat fmt = CurrentTexture.format;

            bool success = true;

            uint[] histogram;
            success = r.GetHistogram(m_TexDisplay.texid, m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx,
                                     rangeHistogram.RangeMin, rangeHistogram.RangeMax,
                                     m_TexDisplay.Red,
                                     m_TexDisplay.Green && fmt.compCount > 1,
                                     m_TexDisplay.Blue && fmt.compCount > 2,
                                     m_TexDisplay.Alpha && fmt.compCount > 3,
                                     out histogram);

            if (success)
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
                    history = r.PixelHistory(CurrentTexture.ID, (UInt32)x, (UInt32)y, m_TexDisplay.sliceFace, m_TexDisplay.mip, m_TexDisplay.sampleIdx);

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

        private TextureSaveDialog m_SaveDialog = null;

        private void saveTex_Click(object sender, EventArgs e)
        {
            if (m_SaveDialog == null)
                m_SaveDialog = new TextureSaveDialog(m_Core);

            m_SaveDialog.saveData.id = m_TexDisplay.texid;
            m_SaveDialog.saveData.slice.sliceIndex = (int)m_TexDisplay.sliceFace;
            m_SaveDialog.saveData.mip = (int)m_TexDisplay.mip;

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
            m_SaveDialog.saveData.alpha = m_TexDisplay.Alpha ? AlphaMapping.BlendToCheckerboard : AlphaMapping.Discard;
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
                    MessageBox.Show(string.Format("Error saving texture {0}.\n\nCheck diagnostic log in Help menu for more details.", saveTextureDialog.FileName),
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

        #endregion

        #region Thumbnail strip

        private void AddResourceUsageEntry(List<ToolStripItem> items, uint start, uint end, ResourceUsage usage)
        {
            ToolStripItem item = null;

            if (start == end)
                item = new ToolStripLabel("EID " + start + ": " + usage.Str(m_Core.APIProps.pipelineType));
            else
                item = new ToolStripLabel("EID " + start + "-" + end + ": " + usage.Str(m_Core.APIProps.pipelineType));

            item.Click += new EventHandler(resourceContextItem_Click);
            item.Tag = end;

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

                            var curDraw = m_Core.GetDrawcall(m_Core.CurFrame, u.eventID);

                            if (u.usage != us || curDraw.previous == null || curDraw.previous.eventID != end)
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

                foreach (var p in rtPanel.Thumbnails)
                    p.Selected = false;

                foreach (var p in texPanel.Thumbnails)
                    p.Selected = false;

                m_Following = follow;
                prev.Selected = true;

                var id = m_Following.GetResourceId(m_Core);

                if (id != ResourceId.Null)
                {
                    UI_OnTextureSelectionChanged();
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
                    m_Core.SetEventID(null, m_Core.CurFrame, (uint)c.Tag);
                else if (c.Tag is ResourceId)
                    ViewTexture((ResourceId)c.Tag, false);
            }
        }

        private void showDisabled_Click(object sender, EventArgs e)
        {
            showDisabled.Checked = !showDisabled.Checked;

            if (m_Core.LogLoaded)
                OnEventSelected(m_Core.CurFrame, m_Core.CurEvent);
        }

        private void showEmpty_Click(object sender, EventArgs e)
        {
            showEmpty.Checked = !showEmpty.Checked;

            if (m_Core.LogLoaded)
                OnEventSelected(m_Core.CurFrame, m_Core.CurEvent);
        }

        private void TextureViewer_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }

        #endregion
   }
}