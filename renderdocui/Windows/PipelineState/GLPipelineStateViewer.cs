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
using renderdoc;

namespace renderdocui.Windows.PipelineState
{
    public partial class GLPipelineStateViewer : UserControl, ILogViewerForm
    {
        private Core m_Core;
        private DockContent m_DockContent;

        // keep track of the VB nodes (we want to be able to highlight them easily on hover)
        private List<TreelistView.Node> m_VBNodes = new List<TreelistView.Node>();

        public GLPipelineStateViewer(Core core, DockContent c)
        {
            InitializeComponent();

            m_DockContent = c;

            pipeFlow.SetStages(new KeyValuePair<string, string>[] {
                new KeyValuePair<string,string>("VTX", "Vertex Input"),
                new KeyValuePair<string,string>("VS", "Vertex Shader"),
                new KeyValuePair<string,string>("TCS", "Tess. Control Shader"),
                new KeyValuePair<string,string>("TES", "Tess. Eval. Shader"),
                new KeyValuePair<string,string>("GS", "Geometry Shader"),
                new KeyValuePair<string,string>("RS", "Rasterizer"),
                new KeyValuePair<string,string>("FS", "Fragment Shader"),
                new KeyValuePair<string,string>("FB", "Framebuffer Output"),
                new KeyValuePair<string,string>("CS", "Compute Shader"),
            });

            pipeFlow.IsolateStage(8); // compute shader isolated

            //Icon = global::renderdocui.Properties.Resources.icon;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            toolTip.SetToolTip(vsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(tcsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(tesShaderCog, "Open Shader Source");
            toolTip.SetToolTip(gsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(fsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(csShaderCog, "Open Shader Source");

            toolTip.SetToolTip(vsShader, "Open Shader Source");
            toolTip.SetToolTip(tcsShader, "Open Shader Source");
            toolTip.SetToolTip(tesShader, "Open Shader Source");
            toolTip.SetToolTip(gsShader, "Open Shader Source");
            toolTip.SetToolTip(fsShader, "Open Shader Source");
            toolTip.SetToolTip(csShader, "Open Shader Source");

            OnLogfileClosed();

            m_Core = core;
        }

        public void OnLogfileClosed()
        {
            inputLayouts.Nodes.Clear();
            iabuffers.Nodes.Clear();
            topology.Text = "";
            topologyDiagram.Image = null;

            restartIndex.Text = "";

            ClearShaderState(vsShader, vsTextures, vsSamplers, vsCBuffers, vsSubroutines);
            ClearShaderState(gsShader, gsTextures, gsSamplers, gsCBuffers, gsSubroutines);
            ClearShaderState(tesShader, tesTextures, tesSamplers, tesCBuffers, tesSubroutines);
            ClearShaderState(tcsShader, tcsTextures, tcsSamplers, tcsCBuffers, tcsSubroutines);
            ClearShaderState(fsShader, fsTextures, fsSamplers, fsCBuffers, fsSubroutines);
            ClearShaderState(csShader, csTextures, csSamplers, csCBuffers, csSubroutines);

            csUAVs.Nodes.Clear();
            gsStreams.Nodes.Clear();

            var tick = global::renderdocui.Properties.Resources.tick;
            var cross = global::renderdocui.Properties.Resources.cross;

            fillMode.Text = "Solid";
            cullMode.Text = "Front";
            frontCCW.Image = tick;

            scissorEnable.Image = tick;
            provokingVertex.Text = "Last";
            rasterizerDiscard.Image = cross;

            pointSize.Text = "1.0";
            lineWidth.Text = "1.0";

            clipSetup.Text = "0,0 Lower Left, Z= -1 to 1";
            clipDistances.Text = "-";

            depthClamp.Image = tick;
            depthBias.Text = "0.0";
            slopeScaledBias.Text = "0.0";
            offsetClamp.Text = "";
            offsetClamp.Image = cross;

            viewports.Nodes.Clear();
            scissors.Nodes.Clear();

            targetOutputs.Nodes.Clear();
            blendOperations.Nodes.Clear();

            blendFactor.Text = "0.00, 0.00, 0.00, 0.00";

            depthEnable.Image = tick;
            depthFunc.Text = "GREATER_EQUAL";
            depthWrite.Image = tick;

            depthBounds.Image = tick;
            depthBounds.Text = "0.000 - 1.000";

            stencilEnable.Image = tick;

            stencilFuncs.Nodes.Clear();

            pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });
        }
        
        public void OnLogfileLoaded()
        {
            OnEventSelected(m_Core.CurFrame, m_Core.CurEvent);
        }

        private void EmptyRow(TreelistView.Node node)
        {
            node.BackColor = Color.Firebrick;
        }

        private void InactiveRow(TreelistView.Node node)
        {
            node.Italic = true;
        }

        private void ClearShaderState(Label shader, TreelistView.TreeListView resources, TreelistView.TreeListView samplers,
                                      TreelistView.TreeListView cbuffers, TreelistView.TreeListView classes)
        {
            shader.Text = "Unbound";
            resources.Nodes.Clear();
            samplers.Nodes.Clear();
            cbuffers.Nodes.Clear();
            classes.Nodes.Clear();
        }

        // Set a shader stage's resources and values
        private void SetShaderState(FetchTexture[] texs, FetchBuffer[] bufs,
                                    GLPipelineState state, GLPipelineState.ShaderStage stage,
                                    Label shader, TreelistView.TreeListView textures, TreelistView.TreeListView samplers,
                                    TreelistView.TreeListView cbuffers, TreelistView.TreeListView subs)
        {
            ShaderReflection shaderDetails = stage.ShaderDetails;
            var mapping = stage.BindpointMapping;

            if (stage.Shader == ResourceId.Null)
                shader.Text = "Unbound";
            else
                shader.Text = "Shader " + stage.Shader.ToString();

            if (shaderDetails != null && shaderDetails.DebugInfo.entryFunc != "" && shaderDetails.DebugInfo.files.Length > 0)
                shader.Text = shaderDetails.DebugInfo.entryFunc + "()" + " - " + 
                                Path.GetFileName(shaderDetails.DebugInfo.files[0].filename);

            int vs = 0;
            int vs2 = 0;

            // simultaneous update of resources and samplers
            vs = textures.VScrollValue();
            textures.BeginUpdate();
            textures.Nodes.Clear();
            vs2 = samplers.VScrollValue();
            samplers.BeginUpdate();
            samplers.Nodes.Clear();
            if (state.Textures != null)
            {
                for (int i = 0; i < state.Textures.Length; i++)
                {
                    var r = state.Textures[i];
                    var s = state.Samplers[i];

                    ShaderResource shaderInput = null;
                    BindpointMap map = null;

                    if (shaderDetails != null)
                    {
                        foreach (var bind in shaderDetails.Resources)
                        {
                            if (bind.IsSRV && mapping.Resources[bind.bindPoint].bind == i)
                            {
                                shaderInput = bind;
                                map = mapping.Resources[bind.bindPoint];
                            }
                        }
                    }

                    bool filledSlot = (r.Resource != ResourceId.Null);
                    bool usedSlot = (shaderInput != null && map.used);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        // do texture
                        {
                            string slotname = i.ToString();

                            if (shaderInput != null && shaderInput.name != "")
                                slotname += ": " + shaderInput.name;

                            UInt32 w = 1, h = 1, d = 1;
                            UInt32 a = 1;
                            string format = "Unknown";
                            string name = "Shader Resource " + r.Resource.ToString();
                            string typename = "Unknown";
                            object tag = null;

                            if (!filledSlot)
                            {
                                name = "Empty";
                                format = "-";
                                typename = "-";
                                w = h = d = a = 0;
                            }

                            // check to see if it's a texture
                            for (int t = 0; t < texs.Length; t++)
                            {
                                if (texs[t].ID == r.Resource)
                                {
                                    w = texs[t].width;
                                    h = texs[t].height;
                                    d = texs[t].depth;
                                    a = texs[t].arraysize;
                                    format = texs[t].format.ToString();
                                    name = texs[t].name;
                                    typename = texs[t].resType.ToString();

                                    if (texs[t].format.special &&
                                        (texs[t].format.specialFormat == SpecialFormat.D24S8 ||
                                         texs[t].format.specialFormat == SpecialFormat.D32S8)
                                        )
                                    {
                                        if (r.DepthReadChannel == 0)
                                            format += " Depth-Read";
                                        else if (r.DepthReadChannel == 1)
                                            format += " Stencil-Read";
                                    }
                                    else if (
                                        r.Swizzle[0] != TextureSwizzle.Red ||
                                        r.Swizzle[1] != TextureSwizzle.Green ||
                                        r.Swizzle[2] != TextureSwizzle.Blue ||
                                        r.Swizzle[3] != TextureSwizzle.Alpha)
                                    {
                                        format += String.Format(" swizzle[{0}{1}{2}{3}]",
                                            r.Swizzle[0].Str(),
                                            r.Swizzle[1].Str(),
                                            r.Swizzle[2].Str(),
                                            r.Swizzle[3].Str());
                                    }

                                    tag = texs[t];
                                }
                            }

                            // if not a texture, it must be a buffer
                            for (int t = 0; t < bufs.Length; t++)
                            {
                                if (bufs[t].ID == r.Resource)
                                {
                                    w = bufs[t].length;
                                    h = 0;
                                    d = 0;
                                    a = 0;
                                    format = "";
                                    name = bufs[t].name;
                                    typename = "Buffer";

                                    // for structured buffers, display how many 'elements' there are in the buffer
                                    if (bufs[t].structureSize > 0)
                                        typename = "StructuredBuffer[" + (bufs[t].length / bufs[t].structureSize) + "]";

                                    tag = bufs[t];
                                }
                            }

                            var node = textures.Nodes.Add(new object[] { slotname, name, typename, w, h, d, a, format });

                            node.Image = global::renderdocui.Properties.Resources.action;
                            node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                            node.Tag = tag;

                            if (!filledSlot)
                                EmptyRow(node);

                            if (!usedSlot)
                                InactiveRow(node);
                        }

                        // do sampler
                        {
                            string slotname = i.ToString();

                            if (shaderInput != null && shaderInput.name.Length > 0)
                                slotname += ": " + shaderInput.name;

                            string borderColor = s.BorderColor[0].ToString() + ", " +
                                                    s.BorderColor[1].ToString() + ", " +
                                                    s.BorderColor[2].ToString() + ", " +
                                                    s.BorderColor[3].ToString();

                            string addressing = "";

                            string addPrefix = "";
                            string addVal = "";

                            string[] addr = { s.AddressS, s.AddressT, s.AddressR };

                            // arrange like either STR: WRAP or ST: WRAP, R: CLAMP
                            for (int a = 0; a < 3; a++)
                            {
                                string prefix = "" + "STR"[a];

                                if (a == 0 || addr[a] == addr[a - 1])
                                {
                                    addPrefix += prefix;
                                }
                                else
                                {
                                    addressing += addPrefix + ": " + addVal + ", ";

                                    addPrefix = prefix;
                                }
                                addVal = addr[a];
                            }

                            addressing += addPrefix + ": " + addVal;

                            if (s.UseBorder)
                                addressing += String.Format("<{0}>", borderColor);

                            if (r.ResType == ShaderResourceType.TextureCube ||
                                r.ResType == ShaderResourceType.TextureCubeArray)
                            {
                                addressing += s.SeamlessCube ? " Seamless" : " Non-Seamless";
                            }

                            string minfilter = s.MinFilter;

                            if (s.MaxAniso > 1)
                                minfilter += String.Format(" Aniso{0}x", s.MaxAniso);

                            if (s.UseComparison)
                                minfilter = String.Format("{0}", s.Comparison);

                            var node = samplers.Nodes.Add(new object[] { slotname, addressing,
                                                            minfilter, s.MagFilter,
                                                            (s.MinLOD == -float.MaxValue ? "0" : s.MinLOD.ToString()) + " - " +
                                                            (s.MaxLOD == float.MaxValue ? "FLT_MAX" : s.MaxLOD.ToString()),
                                                            s.MipLODBias.ToString() });

                            if (!filledSlot)
                                EmptyRow(node);

                            if (!usedSlot)
                                InactiveRow(node);
                        }
                    }
                }
            }
            textures.EndUpdate();
            textures.NodesSelection.Clear();
            textures.SetVScrollValue(vs);
            samplers.EndUpdate();
            samplers.NodesSelection.Clear();
            samplers.SetVScrollValue(vs2);

            vs = cbuffers.VScrollValue();
            cbuffers.BeginUpdate();
            cbuffers.Nodes.Clear();
            if(shaderDetails != null)
            {
                UInt32 i = 0;
                foreach (var shaderCBuf in shaderDetails.ConstantBlocks)
                {
                    int bindPoint = stage.BindpointMapping.ConstantBlocks[i].bind;

                    GLPipelineState.Buffer b = null;

                    if (bindPoint >= 0 && bindPoint < state.UniformBuffers.Length)
                        b = state.UniformBuffers[bindPoint];

                    bool filledSlot = !shaderCBuf.bufferBacked ||
                        (b != null && b.Resource != ResourceId.Null);
                    bool usedSlot = stage.BindpointMapping.ConstantBlocks[i].used;

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        ulong offset = 0;
                        ulong length = 0;
                        int numvars = shaderCBuf.variables.Length;

                        string slotname = "Uniforms";
                        string name = "";
                        string sizestr = String.Format("{0} Variables", numvars);
                        string byterange = "";

                        if (!filledSlot)
                        {
                            name = "Empty";
                            length = 0;
                        }

                        if (b != null)
                        {
                            slotname = String.Format("{0}: {1}", bindPoint, shaderCBuf.name);
                            name = "UBO " + b.Resource.ToString();
                            offset = b.Offset;
                            length = b.Size;

                            for (int t = 0; t < bufs.Length; t++)
                            {
                                if (bufs[t].ID == b.Resource)
                                {
                                    name = bufs[t].name;
                                    if (length == 0)
                                        length = bufs[t].length;
                                }
                            }

                            sizestr = String.Format("{0} Variables, {1} bytes", numvars, length);
                            byterange = String.Format("{0} - {1}", offset, offset + length);
                        }

                        var node = cbuffers.Nodes.Add(new object[] { slotname, name, byterange, sizestr });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = i;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }
                    i++;
                }
            }
            cbuffers.EndUpdate();
            cbuffers.NodesSelection.Clear();
            cbuffers.SetVScrollValue(vs);

            vs = subs.VScrollValue();
            subs.BeginUpdate();
            subs.Nodes.Clear();
            {
                // TODO fetch subroutines
            }
            subs.EndUpdate();
            subs.NodesSelection.Clear();
            subs.SetVScrollValue(vs);

            subs.Visible = subs.Parent.Visible = true; //(stage.Subroutines.Length > 0);
        }

        // from https://gist.github.com/mjijackson/5311256
        private float CalcHue(float p, float q, float t)
        {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;

            if (t < 1.0f / 6.0f)
                return p + (q - p) * 6.0f * t;

            if (t < 0.5f)
                return q;

            if (t < 2.0f / 3.0f)
                return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;

            return p;
        }

        private Color HSLColor(float h, float s, float l)
        {
            float r, g, b;

            if (s == 0)
            {
                r = g = b = l; // achromatic
            }
            else
            {
                var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
                var p = 2 * l - q;
                r = CalcHue(p, q, h + 1.0f / 3.0f);
                g = CalcHue(p, q, h);
                b = CalcHue(p, q, h - 1.0f / 3.0f);
            }

            return Color.FromArgb(255, (int)(r * 255), (int)(g * 255), (int)(b * 255));
        }

        private void UpdateState()
        {
            if (!m_Core.LogLoaded)
                return;

            FetchTexture[] texs = m_Core.CurTextures;
            FetchBuffer[] bufs = m_Core.CurBuffers;
            GLPipelineState state = m_Core.CurGLPipelineState;
            FetchDrawcall draw = m_Core.CurDrawcall;

            var tick = global::renderdocui.Properties.Resources.tick;
            var cross = global::renderdocui.Properties.Resources.cross;

            bool[] usedVBuffers = new bool[128];

            for (int i = 0; i < 128; i++)
            {
                usedVBuffers[i] = false;
            }

            ////////////////////////////////////////////////
            // Input Assembler

            int vs = 0;

            vs = inputLayouts.VScrollValue();
            inputLayouts.Nodes.Clear();
            inputLayouts.BeginUpdate();
            if (state.m_VtxIn.attributes != null)
            {
                int i = 0;
                foreach (var l in state.m_VtxIn.attributes)
                {
                    bool filledSlot = true; // there's always an attribute, either from a buffer (if it's enabled)
                                            // or generic (if disabled)
                    bool usedSlot = false;

                    string name = String.Format("Attribute {0}", i);

                    if (state.m_VS.Shader != ResourceId.Null)
                    {
                        int attrib = state.m_VS.BindpointMapping.InputAttributes[i];

                        if (attrib >= 0 && attrib < state.m_VS.ShaderDetails.InputSig.Length)
                        {
                            name = state.m_VS.ShaderDetails.InputSig[attrib].varName;
                            usedSlot = true;
                        }
                    }

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string byteOffs = l.RelativeOffset.ToString();

                        string genericVal = String.Format("Generic=<{0}, {1}, {2}, {3}>",
                            l.GenericValue.x, l.GenericValue.y, l.GenericValue.z, l.GenericValue.w);

                        var node = inputLayouts.Nodes.Add(new object[] {
                            i,
                            l.Enabled ? "Enabled" : "Disabled", name,
                            l.Enabled ? l.Format.ToString() : genericVal,
                            l.BufferSlot.ToString(), byteOffs, });

                        if(l.Enabled)
                            usedVBuffers[l.BufferSlot] = true;

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;

                        if (!usedSlot)
                            InactiveRow(node);
                    }

                    i++;
                }
            }
            inputLayouts.NodesSelection.Clear();
            inputLayouts.EndUpdate();
            inputLayouts.SetVScrollValue(vs);

            PrimitiveTopology topo = draw != null ? draw.topology : PrimitiveTopology.Unknown;

            topology.Text = topo.ToString();
            if (topo > PrimitiveTopology.PatchList)
            {
                int numCPs = (int)topo - (int)PrimitiveTopology.PatchList + 1;

                topology.Text = string.Format("PatchList ({0} Control Points)", numCPs);
            }

            switch (topo)
            {
                case PrimitiveTopology.PointList:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_pointlist;
                    break;
                case PrimitiveTopology.LineList:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_linelist;
                    break;
                case PrimitiveTopology.LineStrip:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_linestrip;
                    break;
                case PrimitiveTopology.TriangleList:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_trilist;
                    break;
                case PrimitiveTopology.TriangleStrip:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_tristrip;
                    break;
                case PrimitiveTopology.LineList_Adj:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_linelist_adj;
                    break;
                case PrimitiveTopology.LineStrip_Adj:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_linestrip_adj;
                    break;
                case PrimitiveTopology.TriangleList_Adj:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_trilist_adj;
                    break;
                case PrimitiveTopology.TriangleStrip_Adj:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_tristrip_adj;
                    break;
                default:
                    topologyDiagram.Image = global::renderdocui.Properties.Resources.topo_patch;
                    break;
            }

            vs = iabuffers.VScrollValue();
            iabuffers.Nodes.Clear();
            iabuffers.BeginUpdate();

            bool ibufferUsed = draw != null && (draw.flags & DrawcallFlags.UseIBuffer) != 0;

            if (ibufferUsed)
            {
                if(state.m_VtxIn.primitiveRestart)
                    restartIndex.Text = String.Format("Restart Idx: 0x{0:X8}", state.m_VtxIn.restartIndex);
                else
                    restartIndex.Text = "Restart Idx: Disabled";
            }
            else
            {
                restartIndex.Text = "";
            }

            if (state.m_VtxIn.ibuffer != null)
            {
                if (ibufferUsed || showDisabled.Checked)
                {
                    string ptr = "Buffer " + state.m_VtxIn.ibuffer.ToString();
                    string name = ptr;
                    UInt32 length = 1;

                    if (!ibufferUsed)
                    {
                        length = 0;
                    }

                    for (int t = 0; t < bufs.Length; t++)
                    {
                        if (bufs[t].ID == state.m_VtxIn.ibuffer)
                        {
                            name = bufs[t].name;
                            length = bufs[t].length;
                        }
                    }

                    var node = iabuffers.Nodes.Add(new object[] { "Element", name, draw != null ? draw.indexByteWidth : 0, 0, 0, length });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = state.m_VtxIn.ibuffer;

                    if (!ibufferUsed)
                        InactiveRow(node);

                    if (state.m_VtxIn.ibuffer == ResourceId.Null)
                        EmptyRow(node);
                }
            }
            else
            {
                if (ibufferUsed || showEmpty.Checked)
                {
                    var node = iabuffers.Nodes.Add(new object[] { "Element", "No Buffer Set", "-", "-", "-", "-" });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = state.m_VtxIn.ibuffer;

                    EmptyRow(node);

                    if (!ibufferUsed)
                        InactiveRow(node);
                }
            }

            m_VBNodes.Clear();

            if (state.m_VtxIn.vbuffers != null)
            {
                int i = 0;
                foreach (var v in state.m_VtxIn.vbuffers)
                {
                    bool filledSlot = (v.Buffer != ResourceId.Null);
                    bool usedSlot = (usedVBuffers[i]);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string name = "Buffer " + v.Buffer.ToString();
                        UInt32 length = 1;

                        if (!filledSlot)
                        {
                            name = "Empty";
                            length = 0;
                        }

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == v.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;
                            }
                        }

                        var node = iabuffers.Nodes.Add(new object[] { i, name, v.Stride, v.Offset, v.Divisor, length });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = v.Buffer;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);

                        m_VBNodes.Add(node);
                    }

                    i++;
                }
            }
            iabuffers.NodesSelection.Clear();
            iabuffers.EndUpdate();
            iabuffers.SetVScrollValue(vs);

            SetShaderState(texs, bufs, state, state.m_VS, vsShader, vsTextures, vsSamplers, vsCBuffers, vsSubroutines);
            SetShaderState(texs, bufs, state, state.m_GS, gsShader, gsTextures, gsSamplers, gsCBuffers, gsSubroutines);
            SetShaderState(texs, bufs, state, state.m_TES, tesShader, tesTextures, tesSamplers, tesCBuffers, tesSubroutines);
            SetShaderState(texs, bufs, state, state.m_TCS, tcsShader, tcsTextures, tcsSamplers, tcsCBuffers, tcsSubroutines);
            SetShaderState(texs, bufs, state, state.m_FS, fsShader, fsTextures, fsSamplers, fsCBuffers, fsSubroutines);
            SetShaderState(texs, bufs, state, state.m_CS, csShader, csTextures, csSamplers, csCBuffers, csSubroutines);


            csUAVs.Nodes.Clear();
            csUAVs.BeginUpdate();

            csUAVs.NodesSelection.Clear();
            csUAVs.EndUpdate();

            gsStreams.BeginUpdate();
            gsStreams.Nodes.Clear();
            gsStreams.EndUpdate();
            gsStreams.NodesSelection.Clear();

            ////////////////////////////////////////////////
            // Rasterizer

            vs = viewports.VScrollValue();
            viewports.BeginUpdate();
            viewports.Nodes.Clear();
            if (state.m_RS.Viewports != null)
            {
                // accumulate identical viewports to save on visual repetition
                int prev = 0;
                for (int i = 0; i < state.m_RS.Viewports.Length; i++)
                {
                    var v1 = state.m_RS.Viewports[prev];
                    var v2 = state.m_RS.Viewports[i];

                    if (v1.Width != v2.Width ||
                        v1.Height != v2.Height ||
                        v1.Left != v2.Left ||
                        v1.Bottom != v2.Bottom ||
                        v1.MinDepth != v2.MinDepth ||
                        v1.MaxDepth != v2.MaxDepth)
                    {
                        if (v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || showEmpty.Checked)
                        {
                            string indexstring = prev.ToString();
                            if (prev < i - 1)
                                indexstring = String.Format("{0}-{1}", prev, i - 1);
                            var node = viewports.Nodes.Add(new object[] { indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth });

                            if (v1.Width == v1.Height && v1.Width == 0 && v1.Height == 0)
                                EmptyRow(node);
                        }

                        prev = i;
                    }
                }

                // handle the last batch (the loop above leaves the last batch un-added)
                if(prev < state.m_RS.Viewports.Length)
                {
                    var v1 = state.m_RS.Viewports[prev];

                    if (v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || showEmpty.Checked)
                    {
                        string indexstring = prev.ToString();
                        if (prev < state.m_RS.Viewports.Length-1)
                            indexstring = String.Format("{0}-{1}", prev, state.m_RS.Viewports.Length - 1);
                        var node = viewports.Nodes.Add(new object[] { indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth });

                        if (v1.Width == v1.Height && v1.Width == 0 && v1.Height == 0)
                            EmptyRow(node);
                    }
                }
            }
            viewports.NodesSelection.Clear();
            viewports.EndUpdate();
            viewports.SetVScrollValue(vs);

            bool anyScissorEnable = false;

            vs = scissors.VScrollValue();
            scissors.BeginUpdate();
            scissors.Nodes.Clear();
            if (state.m_RS.Scissors != null)
            {
                // accumulate identical scissors to save on visual repetition
                int prev = 0;
                for (int i = 0; i < state.m_RS.Scissors.Length; i++)
                {
                    var s1 = state.m_RS.Scissors[prev];
                    var s2 = state.m_RS.Scissors[i];

                    if (s1.Width != s2.Width ||
                        s1.Height != s2.Height ||
                        s1.Left != s2.Left ||
                        s1.Bottom != s2.Bottom)
                    {
                        if (s1.Width != s1.Height || s1.Width != 0 || s1.Height != 0 || showEmpty.Checked)
                        {
                            string indexstring = prev.ToString();
                            if (prev < i - 1)
                                indexstring = String.Format("{0}-{1}", prev, i - 1);
                            var node = scissors.Nodes.Add(new object[] { indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height, s1.Enabled });

                            if (s1.Width == s1.Height && s1.Width == 0 && s1.Height == 0)
                                EmptyRow(node);
                        }

                        prev = i;
                    }
                }

                // handle the last batch (the loop above leaves the last batch un-added)
                if (prev < state.m_RS.Scissors.Length)
                {
                    var s1 = state.m_RS.Scissors[prev];

                    if (s1.Width != s1.Height || s1.Width != 0 || s1.Height != 0 || showEmpty.Checked)
                    {
                        string indexstring = prev.ToString();
                        if (prev < state.m_RS.Scissors.Length - 1)
                            indexstring = String.Format("{0}-{1}", prev, state.m_RS.Scissors.Length - 1);
                        var node = scissors.Nodes.Add(new object[] { indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height, s1.Enabled });

                        if (s1.Width == s1.Height && s1.Width == 0 && s1.Height == 0)
                            EmptyRow(node);
                    }
                }
            }
            scissors.NodesSelection.Clear();
            scissors.EndUpdate();
            scissors.SetVScrollValue(vs);

            fillMode.Text = state.m_RS.m_State.FillMode.ToString();
            cullMode.Text = state.m_RS.m_State.CullMode.ToString();
            frontCCW.Image = state.m_RS.m_State.FrontCCW ? tick : cross;

            scissorEnable.Image = anyScissorEnable  ? tick : cross;
            provokingVertex.Text = state.m_VtxIn.provokingVertexLast ? "Last" : "First";

            rasterizerDiscard.Image = state.m_VtxProcess.discard ? tick : cross;

            pointSize.Text = state.m_RS.m_State.ProgrammablePointSize ? "Program" : Formatter.Format(state.m_RS.m_State.PointSize);
            lineWidth.Text = Formatter.Format(state.m_RS.m_State.LineWidth);

            clipSetup.Text = "";
            if(state.m_VtxProcess.clipOriginLowerLeft)
                clipSetup.Text += "0,0 Lower Left";
            else
                clipSetup.Text += "0,0 Upper Left";
            clipSetup.Text += ", ";
            if (state.m_VtxProcess.clipNegativeOneToOne)
                clipSetup.Text += "Z= -1 to 1";
            else
                clipSetup.Text += "Z= 0 to 1";
            
            clipDistances.Text = "";

            int numDist = 0;
            for (int i = 0; i < state.m_VtxProcess.clipPlanes.Length; i++)
            {
                if (state.m_VtxProcess.clipPlanes[i])
                {
                    if (numDist > 0)
                        clipDistances.Text += ", ";
                    clipDistances.Text += i.ToString();

                    numDist++;
                }
            }

            if (numDist == 0)
                clipDistances.Text = "-";
            else
                clipDistances.Text += " enabled";

            depthClamp.Image = state.m_RS.m_State.DepthClamp ? cross : tick;
            depthBias.Text = Formatter.Format(state.m_RS.m_State.DepthBias);
            slopeScaledBias.Text = Formatter.Format(state.m_RS.m_State.SlopeScaledDepthBias);
            if (state.m_RS.m_State.OffsetClamp == 0.0f || float.IsNaN(state.m_RS.m_State.OffsetClamp))
            {
                offsetClamp.Text = "";
                offsetClamp.Image = cross;
            }
            else
            {
                offsetClamp.Text = Formatter.Format(state.m_RS.m_State.OffsetClamp);
                offsetClamp.Image = null;
            }

            ////////////////////////////////////////////////
            // Output Merger

            bool[] targets = new bool[8];

            for (int i = 0; i < 8; i++)
                targets[i] = false;

            vs = targetOutputs.VScrollValue();
            targetOutputs.BeginUpdate();
            targetOutputs.Nodes.Clear();
            {
                int i = 0;
                foreach (var db in state.m_FB.m_DrawFBO.DrawBuffers)
                {
                    ResourceId p = ResourceId.Null;

                    if (db >= 0) p = state.m_FB.m_DrawFBO.Color[db];

                    if (p != ResourceId.Null || showEmpty.Checked)
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = "Unknown";
                        string name = "Texture " + p.ToString();
                        string typename = "Unknown";
                        object tag = null;

                        if (p == ResourceId.Null)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == p)
                            {
                                w = texs[t].width;
                                h = texs[t].height;
                                d = texs[t].depth;
                                a = texs[t].arraysize;
                                format = texs[t].format.ToString();
                                name = texs[t].name;
                                typename = texs[t].resType.ToString();

                                tag = texs[t];

                                if (texs[t].format.srgbCorrected && !state.m_FB.FramebufferSRGB)
                                    name += " (GL_FRAMEBUFFER_SRGB = 0)";
                            }
                        }

                        var node = targetOutputs.Nodes.Add(new object[] { i, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (p == ResourceId.Null)
                        {
                            EmptyRow(node);
                        }
                        else
                        {
                            targets[i] = true;
                        }
                    }

                    i++;
                }
            }

            {
                int i = 0;
                foreach (ResourceId depthstencil in new ResourceId[] { state.m_FB.m_DrawFBO.Depth, state.m_FB.m_DrawFBO.Stencil })
                {
                    if (depthstencil != ResourceId.Null || showEmpty.Checked)
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = "Unknown";
                        string name = "Depth Target " + depthstencil.ToString();
                        string typename = "Unknown";
                        object tag = null;

                        if (depthstencil == ResourceId.Null)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == depthstencil)
                            {
                                w = texs[t].width;
                                h = texs[t].height;
                                d = texs[t].depth;
                                a = texs[t].arraysize;
                                format = texs[t].format.ToString();
                                name = texs[t].name;
                                typename = texs[t].resType.ToString();

                                tag = texs[t];
                            }
                        }

                        string slot = "Depth";
                        if (i == 1) slot = "Stencil";

                        var node = targetOutputs.Nodes.Add(new object[] { slot, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (depthstencil == ResourceId.Null)
                            EmptyRow(node);
                    }

                    i++;
                }
            }
            targetOutputs.EndUpdate();
            targetOutputs.NodesSelection.Clear();
            targetOutputs.SetVScrollValue(vs);

            vs = blendOperations.VScrollValue();
            blendOperations.BeginUpdate();
            blendOperations.Nodes.Clear();
            if(state.m_FB.m_BlendState.Blends.Length > 0)
            {
                bool logic = (state.m_FB.m_BlendState.Blends[0].LogicOp != "");

                int i = 0;
                foreach (var blend in state.m_FB.m_BlendState.Blends)
                {
                    bool filledSlot = (blend.Enabled == true || targets[i]);
                    bool usedSlot = (targets[i]);

                    // if logic operation is enabled, blending is disabled
                    if (logic)
                        filledSlot = (i == 0);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        TreelistView.Node node = null;
                        if (i == 0 && logic)
                        {
                            node = blendOperations.Nodes.Add(new object[] { i,
                                                        true,

                                                        "-",
                                                        "-",
                                                        blend.LogicOp,

                                                        "-",
                                                        "-",
                                                        "-",

                                                        ((blend.WriteMask & 0x1) == 0 ? "_" : "R") +
                                                        ((blend.WriteMask & 0x2) == 0 ? "_" : "G") +
                                                        ((blend.WriteMask & 0x4) == 0 ? "_" : "B") +
                                                        ((blend.WriteMask & 0x8) == 0 ? "_" : "A")
                            });
                        }
                        else
                        {
                            node = blendOperations.Nodes.Add(new object[] { i,
                                                        blend.Enabled,

                                                        blend.m_Blend.Source,
                                                        blend.m_Blend.Destination,
                                                        blend.m_Blend.Operation,

                                                        blend.m_AlphaBlend.Source,
                                                        blend.m_AlphaBlend.Destination,
                                                        blend.m_AlphaBlend.Operation,

                                                        ((blend.WriteMask & 0x1) == 0 ? "_" : "R") +
                                                        ((blend.WriteMask & 0x2) == 0 ? "_" : "G") +
                                                        ((blend.WriteMask & 0x4) == 0 ? "_" : "B") +
                                                        ((blend.WriteMask & 0x8) == 0 ? "_" : "A")
                            });
                        }


                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }

                    i++;
                }
            }
            blendOperations.NodesSelection.Clear();
            blendOperations.EndUpdate();
            blendOperations.SetVScrollValue(vs);

            blendFactor.Text = state.m_FB.m_BlendState.BlendFactor[0].ToString("F2") + ", " +
                                state.m_FB.m_BlendState.BlendFactor[1].ToString("F2") + ", " +
                                state.m_FB.m_BlendState.BlendFactor[2].ToString("F2") + ", " +
                                state.m_FB.m_BlendState.BlendFactor[3].ToString("F2");

            depthEnable.Image = state.m_DepthState.DepthEnable ? tick : cross;
            depthFunc.Text = state.m_DepthState.DepthFunc;
            depthWrite.Image = state.m_DepthState.DepthWrites ? tick : cross;

            depthBounds.Image = state.m_DepthState.DepthBounds ? tick : cross;
            depthBounds.Text = String.Format("{0:F3} - {1:F3}", state.m_DepthState.NearBound, state.m_DepthState.FarBound);

            stencilEnable.Image = state.m_StencilState.StencilEnable ? tick : cross;

            stencilFuncs.BeginUpdate();
            stencilFuncs.Nodes.Clear();
            var sop = state.m_StencilState.m_FrontFace;
            stencilFuncs.Nodes.Add(new object[] { "Front", sop.Func, sop.FailOp, sop.DepthFailOp, sop.PassOp, sop.Ref, sop.WriteMask, sop.ValueMask });
            sop = state.m_StencilState.m_BackFace;
            stencilFuncs.Nodes.Add(new object[] { "Back", sop.Func, sop.FailOp, sop.DepthFailOp, sop.PassOp, sop.Ref, sop.WriteMask, sop.ValueMask });
            stencilFuncs.EndUpdate();
            stencilFuncs.NodesSelection.Clear();

            // highlight the appropriate stages in the flowchart
            if (draw == null)
            {
                pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });
            }
            else if ((draw.flags & DrawcallFlags.Dispatch) != 0)
            {
                pipeFlow.SetStagesEnabled(new bool[] { false, false, false, false, false, false, false, false, true });
            }
            else
            {
                pipeFlow.SetStagesEnabled(new bool[] {
                    true,
                    true,
                    state.m_TES.Shader != ResourceId.Null,
                    state.m_TCS.Shader != ResourceId.Null,
                    state.m_GS.Shader != ResourceId.Null,
                    true,
                    state.m_FS.Shader != ResourceId.Null,
                    true,
                    false
                });

                // if(streamout only)
                //{
                //    pipeFlow.Rasterizer = false;
                //    pipeFlow.OutputMerger = false;
                //}
            }
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            UpdateState();
        }

        private void hideDisabledEmpty_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                rightclickMenu.Show((Control)sender, new Point(e.X, e.Y));
            }
        }

        private void hideDisabled_Click(object sender, EventArgs e)
        {
            showDisabled.Checked = !showDisabled.Checked;
            showDisabledToolitem.Checked = showDisabled.Checked;
            UpdateState();
        }

        private void hideEmpty_Click(object sender, EventArgs e)
        {
            showEmpty.Checked = !showEmpty.Checked;
            showEmptyToolitem.Checked = showEmpty.Checked;
            UpdateState();
        }

        // launch the appropriate kind of viewer, depending on the type of resource that's in this node
        private void textureCell_CellDoubleClick(TreelistView.Node node)
        {
            object tag = node.Tag;

            GLPipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage == null) return;

            if (tag is FetchTexture)
            {
                FetchTexture tex = (FetchTexture)tag;

                var viewer = m_Core.GetTextureViewer();
                viewer.Show(m_DockContent.DockPanel);
                if (!viewer.IsDisposed)
                    viewer.ViewTexture(tex.ID, true);
            }
            else if(tag is FetchBuffer)
            {
            }
        }

        private void disableSelection_Leave(object sender, EventArgs e)
        {
            if (sender is DataGridView)
                ((DataGridView)sender).ClearSelection();
            else if (sender is TreelistView.TreeListView)
            {
                ((TreelistView.TreeListView)sender).NodesSelection.Clear();
                ((TreelistView.TreeListView)sender).FocusedNode = null;
            }
        }

        private void disableSelection_VisibleChanged(object sender, EventArgs e)
        {
            ((DataGridView)sender).ClearSelection();
            ((DataGridView)sender).AutoResizeColumns(DataGridViewAutoSizeColumnsMode.AllCells);
        }

        private void iabuffers_NodeDoubleClicked(TreelistView.Node node)
        {
            if (node.Tag is ResourceId)
            {
                ResourceId id = (ResourceId)node.Tag;

                if (id != ResourceId.Null)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(id);
                    viewer.Show(m_DockContent.DockPanel);
                }
            }
        }

        private void inputLayouts_NodeDoubleClick(TreelistView.Node node)
        {
            (new BufferViewer(m_Core, true)).Show(m_DockContent.DockPanel);
        }

        private GLPipelineState.ShaderStage GetStageForSender(object sender)
        {
            GLPipelineState.ShaderStage stage = null;

            if (!m_Core.LogLoaded)
                return null;

            object cur = sender;

            while (cur is Control)
            {
                if (cur == tabVS)
                    stage = m_Core.CurGLPipelineState.m_VS;
                else if (cur == tabGS)
                    stage = m_Core.CurGLPipelineState.m_GS;
                else if (cur == tabTCS)
                    stage = m_Core.CurGLPipelineState.m_TCS;
                else if (cur == tabTES)
                    stage = m_Core.CurGLPipelineState.m_TES;
                else if (cur == tabFS)
                    stage = m_Core.CurGLPipelineState.m_FS;
                else if (cur == tabCS)
                    stage = m_Core.CurGLPipelineState.m_CS;
                else if (cur == tabFB)
                    stage = m_Core.CurGLPipelineState.m_FS;

                if (stage != null)
                    return stage;

                Control c = (Control)cur;

                if(c.Parent == null)
                    break;

                cur = ((Control)cur).Parent;
            }

            System.Diagnostics.Debug.Fail("Unrecognised control calling event handler");

            return null;
        }

        private void shader_Click(object sender, EventArgs e)
        {
            GLPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null) return;

            ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, stage.stage, null, "");

            s.Show(m_DockContent.DockPanel);
        }

        private void MakeShaderVariablesHLSL(bool cbufferContents, ShaderConstant[] vars, ref string struct_contents, ref string struct_defs)
        {
            var nl = Environment.NewLine;
            var nl2 = Environment.NewLine + Environment.NewLine;

            foreach (var v in vars)
            {
                if (v.type.members.Length > 0)
                {
                    string def = "struct " + v.type.descriptor.name + " {" + nl;

                    if(!struct_defs.Contains(def))
                    {
                        string contents = "";
                        MakeShaderVariablesHLSL(false, v.type.members, ref contents, ref struct_defs);

                        struct_defs += def + contents + "};" + nl2;
                    }
                }

                struct_contents += "\t" + v.type.descriptor.name + " " + v.name;

                char comp = 'x';
                if (v.reg.comp == 1) comp = 'y';
                if (v.reg.comp == 2) comp = 'z';
                if (v.reg.comp == 3) comp = 'w';

                if (cbufferContents) struct_contents += String.Format(" : packoffset(c{0}.{1});", v.reg.vec, comp);
                else struct_contents += ";";

                struct_contents += nl;
            }
        }

        private void shaderedit_Click(object sender, EventArgs e)
        {
        }

        private void ShowCBuffer(GLPipelineState.ShaderStage stage, UInt32 slot)
        {
            var existing = ConstantBufferPreviewer.Has(stage.stage, slot);
            if (existing != null)
            {
                existing.Show();
                return;
            }

            var prev = new ConstantBufferPreviewer(m_Core, stage.stage, slot);

            prev.ShowDock(m_DockContent.Pane, DockAlignment.Right, 0.3);
        }

        private void cbuffers_NodeDoubleClicked(TreelistView.Node node)
        {
            GLPipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage != null && node.Tag is UInt32)
            {
                ShowCBuffer(stage, (UInt32)node.Tag);
            }
        }

        private void CBuffers_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            GLPipelineState.ShaderStage stage = GetStageForSender(sender);

            object tag = ((DataGridView)sender).Rows[e.RowIndex].Tag;

            if (stage != null && tag is UInt32)
            {
                ShowCBuffer(stage, (UInt32)tag);
            }
        }

        private string FormatMembers(int indent, string nameprefix, ShaderConstant[] vars)
        {
            string indentstr = new string(' ', indent*4);

            string ret = "";

            int i = 0;

            foreach (var v in vars)
            {
                if (v.type.members.Length > 0)
                {
                    if (i > 0)
                        ret += "\n";
                    ret += indentstr + "// struct " + v.type.Name + Environment.NewLine;
                    ret += indentstr + "{" + Environment.NewLine +
                                FormatMembers(indent + 1, v.name + "_", v.type.members) +
                           indentstr + "}" + Environment.NewLine;
                    if (i < vars.Length-1)
                        ret += Environment.NewLine;
                }
                else
                {
                    ret += indentstr + v.type.Name + " " + nameprefix + v.name + ";" + Environment.NewLine;
                }

                i++;
            }

            return ret;
        }

        private void shaderCog_MouseEnter(object sender, EventArgs e)
        {
            if (sender is PictureBox)
            {
                GLPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage != null && stage.Shader != ResourceId.Null)
                    (sender as PictureBox).Image = global::renderdocui.Properties.Resources.action_hover;
            }

            if (sender is Label)
            {
                GLPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseEnter(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Control) shaderCog_MouseEnter(tcsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Eval) shaderCog_MouseEnter(tesShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseEnter(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Fragment) shaderCog_MouseEnter(fsShaderCog, e);
                if (stage.stage == ShaderStageType.Compute) shaderCog_MouseEnter(csShaderCog, e);
            }
        }

        private void shaderCog_MouseLeave(object sender, EventArgs e)
        {
            if (sender is PictureBox)
            {
                (sender as PictureBox).Image = global::renderdocui.Properties.Resources.action;
            }

            if (sender is Label)
            {
                GLPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseLeave(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Control) shaderCog_MouseLeave(tcsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Eval) shaderCog_MouseLeave(tesShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseLeave(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Fragment) shaderCog_MouseLeave(fsShaderCog, e);
                if (stage.stage == ShaderStageType.Compute) shaderCog_MouseLeave(csShaderCog, e);
            }
        }

        private void pipeFlow_SelectedStageChanged(object sender, EventArgs e)
        {
            stageTabControl.SelectedIndex = pipeFlow.SelectedStage;
        }

        private void csDebug_Click(object sender, EventArgs e)
        {
        }

        private void meshView_MouseEnter(object sender, EventArgs e)
        {
            meshView.BackColor = Color.LightGray;
        }

        private void meshView_MouseLeave(object sender, EventArgs e)
        {
            meshView.BackColor = SystemColors.Control;
        }

        private void meshView_Click(object sender, EventArgs e)
        {
            (new BufferViewer(m_Core, true)).Show(m_DockContent.DockPanel);
        }

        private float GetHueForVB(int i)
        {
            int idx = ((i+1) * 21) % 32; // space neighbouring colours reasonably distinctly
            return (float)(idx) / 32.0f;
        }

        private void inputLayouts_MouseMove(object sender, MouseEventArgs e)
        {
            if (m_Core.CurGLPipelineState == null) return;

            Point mousePoint = inputLayouts.PointToClient(Cursor.Position);
            var hoverNode = inputLayouts.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            var VtxIn = m_Core.CurGLPipelineState.m_VtxIn;

            if (hoverNode != null)
            {
                int index = inputLayouts.Nodes.GetNodeIndex(hoverNode);

                if (index >= 0 && index < VtxIn.attributes.Length)
                {
                    uint slot = VtxIn.attributes[index].BufferSlot;

                    HighlightVtxAttribSlot(slot);
                }
            }
        }

        private void HighlightVtxAttribSlot(uint slot)
        {
            var VtxIn = m_Core.CurGLPipelineState.m_VtxIn;
        
            Color c = HSLColor(GetHueForVB((int)slot), 1.0f, 0.95f);

            if (slot < m_VBNodes.Count)
                m_VBNodes[(int)slot].DefaultBackColor = c;

            for (int i = 0; i < inputLayouts.Nodes.Count; i++)
            {
                var n = inputLayouts.Nodes[i];
                if (VtxIn.attributes[i].BufferSlot == slot)
                    n.DefaultBackColor = c;
                else
                    n.DefaultBackColor = Color.Transparent;
            }

            inputLayouts.Invalidate();
            iabuffers.Invalidate();
        }

        private void ia_MouseLeave(object sender, EventArgs e)
        {
            foreach (var n in iabuffers.Nodes)
                n.DefaultBackColor = Color.Transparent;

            foreach (var n in inputLayouts.Nodes)
                n.DefaultBackColor = Color.Transparent;

            inputLayouts.Invalidate();
            iabuffers.Invalidate();
        }

        private void iabuffers_MouseMove(object sender, MouseEventArgs e)
        {
            if (m_Core.CurGLPipelineState == null) return;

            Point mousePoint = iabuffers.PointToClient(Cursor.Position);
            var hoverNode = iabuffers.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            if (hoverNode != null)
            {
                int idx = m_VBNodes.IndexOf(hoverNode);
                if (idx >= 0)
                    HighlightVtxAttribSlot((uint)idx);
                else
                    hoverNode.DefaultBackColor = SystemColors.ControlLight;
            }
        }
   }
}