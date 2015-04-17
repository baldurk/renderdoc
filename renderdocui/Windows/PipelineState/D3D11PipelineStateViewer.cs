﻿/******************************************************************************
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
using System.Xml;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdocui.Controls;
using renderdoc;

namespace renderdocui.Windows.PipelineState
{
    public partial class D3D11PipelineStateViewer : UserControl, ILogViewerForm
    {
        private Core m_Core;
        private DockContent m_DockContent;

        // keep track of the VB nodes (we want to be able to highlight them easily on hover)
        private List<TreelistView.Node> m_VBNodes = new List<TreelistView.Node>();

        public D3D11PipelineStateViewer(Core core, DockContent c)
        {
            InitializeComponent();

            m_DockContent = c;

            inputLayouts.Font = core.Config.PreferredFont;
            iabuffers.Font = core.Config.PreferredFont;

            csUAVs.Font = core.Config.PreferredFont;
            gsStreams.Font = core.Config.PreferredFont;

            groupX.Font = groupY.Font = groupZ.Font = core.Config.PreferredFont;
            threadX.Font = threadY.Font = threadZ.Font = core.Config.PreferredFont;

            vsShader.Font = vsResources.Font = vsSamplers.Font = vsCBuffers.Font = vsClasses.Font = core.Config.PreferredFont;
            gsShader.Font = gsResources.Font = gsSamplers.Font = gsCBuffers.Font = gsClasses.Font = core.Config.PreferredFont;
            hsShader.Font = hsResources.Font = hsSamplers.Font = hsCBuffers.Font = hsClasses.Font = core.Config.PreferredFont;
            dsShader.Font = dsResources.Font = dsSamplers.Font = dsCBuffers.Font = dsClasses.Font = core.Config.PreferredFont;
            psShader.Font = psResources.Font = psSamplers.Font = psCBuffers.Font = psClasses.Font = core.Config.PreferredFont;
            csShader.Font = csResources.Font = csSamplers.Font = csCBuffers.Font = csClasses.Font = core.Config.PreferredFont;

            viewports.Font = core.Config.PreferredFont;
            scissors.Font = core.Config.PreferredFont;

            targetOutputs.Font = core.Config.PreferredFont;
            blendOperations.Font = core.Config.PreferredFont;
            
            pipeFlow.Font = new System.Drawing.Font(core.Config.PreferredFont.FontFamily, 11.25F,
                System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

            pipeFlow.SetStages(new KeyValuePair<string, string>[] {
                new KeyValuePair<string,string>("IA", "Input Assembler"),
                new KeyValuePair<string,string>("VS", "Vertex Shader"),
                new KeyValuePair<string,string>("HS", "Hull Shader"),
                new KeyValuePair<string,string>("DS", "Domain Shader"),
                new KeyValuePair<string,string>("GS", "Geometry Shader"),
                new KeyValuePair<string,string>("RS", "Rasterizer"),
                new KeyValuePair<string,string>("PS", "Pixel Shader"),
                new KeyValuePair<string,string>("OM", "Output Merger"),
                new KeyValuePair<string,string>("CS", "Compute Shader"),
            });

            pipeFlow.IsolateStage(8); // compute shader isolated

            pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });

            //Icon = global::renderdocui.Properties.Resources.icon;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            iaBytecodeMismatch.Text = "";
            iaBytecodeMismatch.Visible = false;

            toolTip.SetToolTip(vsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(dsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(hsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(gsShaderCog, "Open Shader Source");
            toolTip.SetToolTip(psShaderCog, "Open Shader Source");
            toolTip.SetToolTip(csShaderCog, "Open Shader Source");

            toolTip.SetToolTip(vsShader, "Open Shader Source");
            toolTip.SetToolTip(dsShader, "Open Shader Source");
            toolTip.SetToolTip(hsShader, "Open Shader Source");
            toolTip.SetToolTip(gsShader, "Open Shader Source");
            toolTip.SetToolTip(psShader, "Open Shader Source");
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

            iaBytecodeMismatch.Text = "";
            iaBytecodeMismatch.Visible = false;
            iaBytecode.Text = "";

            ClearShaderState(vsShader, vsResources, vsSamplers, vsCBuffers, vsClasses);
            ClearShaderState(gsShader, gsResources, gsSamplers, gsCBuffers, gsClasses);
            ClearShaderState(hsShader, hsResources, hsSamplers, hsCBuffers, hsClasses);
            ClearShaderState(dsShader, dsResources, dsSamplers, dsCBuffers, dsClasses);
            ClearShaderState(psShader, psResources, psSamplers, psCBuffers, psClasses);
            ClearShaderState(csShader, csResources, csSamplers, csCBuffers, csClasses);

            csUAVs.Nodes.Clear();
            gsStreams.Nodes.Clear();

            var tick = global::renderdocui.Properties.Resources.tick;

            fillMode.Text = "Solid";
            cullMode.Text = "Front";
            frontCCW.Image = tick;

            scissorEnable.Image = tick;
            lineAAEnable.Image = tick;
            multisampleEnable.Image = tick;

            depthClip.Image = tick;
            depthBias.Text = "0";
            depthBiasClamp.Text = "0.0";
            slopeScaledBias.Text = "0.0";

            viewports.Nodes.Clear();
            scissors.Nodes.Clear();

            targetOutputs.Nodes.Clear();
            blendOperations.Nodes.Clear();

            alphaToCoverage.Image = tick;
            independentBlend.Image = tick;

            blendFactor.Text = "0.00, 0.00, 0.00, 0.00";

            sampleMask.Text = "FFFFFFFF";

            depthEnable.Image = tick;
            depthFunc.Text = "GREATER_EQUAL";
            depthWrite.Image = tick;

            stencilEnable.Image = tick;
            stencilReadMask.Text = "FF";
            stencilWriteMask.Text = "FF";
            stencilRef.Text = "FF";

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
                                    D3D11PipelineState.ShaderStage stage,
                                    Label shader, TreelistView.TreeListView resources, TreelistView.TreeListView samplers,
                                    TreelistView.TreeListView cbuffers, TreelistView.TreeListView classes)
        {
            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null)
                shader.Text = "Unbound";
            else
                shader.Text = stage.ShaderName;

            if (shaderDetails != null && shaderDetails.DebugInfo.entryFunc.Length > 0 && shaderDetails.DebugInfo.files.Length > 0)
                shader.Text = shaderDetails.DebugInfo.entryFunc + "()" + " - " + 
                                Path.GetFileName(shaderDetails.DebugInfo.files[0].filename);

            int vs = 0;
            
            vs = resources.VScrollValue();
            resources.BeginUpdate();
            resources.Nodes.Clear();
            if (stage.SRVs != null)
            {
                int i = 0;
                foreach (var r in stage.SRVs)
                {
                    ShaderResource shaderInput = null;

                    if (shaderDetails != null)
                    {
                        foreach (var bind in shaderDetails.Resources)
                        {
                            if (bind.IsSRV && bind.bindPoint == i)
                                shaderInput = bind;
                        }
                    }

                    bool filledSlot = (r.Resource != ResourceId.Null);
                    bool usedSlot = (shaderInput != null);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string slotname = i.ToString();

                        if (shaderInput != null && shaderInput.name.Length > 0)
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
                                typename = texs[t].resType.Str();

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

                                // get the buffer type, whether it's just a basic type or a complex struct
                                if (shaderInput != null && !shaderInput.IsTexture)
                                {
                                    if (r.Format.compType == FormatComponentType.None)
                                    {
                                        if (shaderInput.variableType.members.Length > 0)
                                            format = "struct " + shaderInput.variableType.Name;
                                        else
                                            format = shaderInput.variableType.Name;
                                    }
                                    else
                                    {
                                        format = r.Format.ToString();
                                    }
                                }

                                tag = bufs[t];
                            }
                        }

                        var node = resources.Nodes.Add(new object[] { slotname, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }
                    i++;
                }
            }
            resources.EndUpdate();
            resources.NodesSelection.Clear();
            resources.SetVScrollValue(vs);

            vs = samplers.VScrollValue();
            samplers.BeginUpdate();
            samplers.Nodes.Clear();
            if (stage.Samplers != null)
            {
                int i = 0;
                foreach (var s in stage.Samplers)
                {
                    ShaderResource shaderInput = null;

                    if (shaderDetails != null)
                    {
                        foreach (var bind in shaderDetails.Resources)
                        {
                            if (bind.IsSampler && bind.bindPoint == i)
                                shaderInput = bind;
                        }
                    }

                    bool filledSlot = (s.AddressU.Length > 0);
                    bool usedSlot = (shaderInput != null);
                    
                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
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

                        string[] addr = { s.AddressU, s.AddressV, s.AddressW };

                        // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
                        for (int a = 0; a < 3; a++)
                        {
                            string prefix = "" + "UVW"[a];

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

                        if(s.UseBorder)
                            addressing += String.Format("<{0}>", borderColor);

                        string filter = s.Filter;

                        if (s.MaxAniso > 0)
                            filter += String.Format(" {0}x", s.MaxAniso);

                        if (s.UseComparison)
                            filter += String.Format(" ({0})", s.Comparison);

                        var node = samplers.Nodes.Add(new object[] { slotname, addressing,
                                                            filter,
                                                            (s.MinLOD == -float.MaxValue ? "0" : s.MinLOD.ToString()) + " - " +
                                                            (s.MaxLOD == float.MaxValue ? "FLT_MAX" : s.MaxLOD.ToString()),
                                                            s.MipLODBias.ToString() });

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }

                    i++;
                }
            }
            samplers.EndUpdate();
            samplers.NodesSelection.Clear();
            samplers.SetVScrollValue(vs);

            vs = cbuffers.VScrollValue();
            cbuffers.BeginUpdate();
            cbuffers.Nodes.Clear();
            if (stage.ConstantBuffers != null)
            {
                UInt32 i = 0;
                foreach (var b in stage.ConstantBuffers)
                {
                    ConstantBlock shaderCBuf = null;

                    if (shaderDetails != null && i < shaderDetails.ConstantBlocks.Length && shaderDetails.ConstantBlocks[i].name.Length > 0)
                        shaderCBuf = shaderDetails.ConstantBlocks[i];

                    bool filledSlot = (b.Buffer != ResourceId.Null);
                    bool usedSlot = (shaderCBuf != null);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string name = "Constant Buffer " + b.Buffer.ToString();
                        UInt32 length = 1;
                        int numvars = shaderCBuf != null ? shaderCBuf.variables.Length : 0;

                        if (!filledSlot)
                        {
                            name = "Empty";
                            length = 0;
                        }

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == b.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;
                            }
                        }

                        string slotname = i.ToString();

                        if (shaderCBuf != null && shaderCBuf.name.Length > 0)
                            slotname += ": " + shaderCBuf.name;

                        string sizestr = String.Format("{0} Variables, {1} bytes", numvars, length);
                        string vecrange = String.Format("{0} - {1}", b.VecOffset, b.VecOffset + b.VecCount);

                        var node = cbuffers.Nodes.Add(new object[] { slotname, name, vecrange, sizestr });

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

            vs = classes.VScrollValue();
            classes.BeginUpdate();
            classes.Nodes.Clear();
            {
                UInt32 i = 0;
                foreach (var inst in stage.ClassInstances)
                {
                    string interfaceName = String.Format("Interface {0}", i);

                    if (shaderDetails != null && i < shaderDetails.Interfaces.Length)
                        interfaceName = shaderDetails.Interfaces[i].Name;

                    classes.Nodes.Add(new object[] { i.ToString(), interfaceName, inst });

                    i++;
                }
            }
            classes.EndUpdate();
            classes.NodesSelection.Clear();
            classes.SetVScrollValue(vs);

            classes.Visible = classes.Parent.Visible = (stage.ClassInstances.Length > 0);
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
            D3D11PipelineState state = m_Core.CurD3D11PipelineState;
            FetchDrawcall draw = m_Core.CurDrawcall;

            var tick = global::renderdocui.Properties.Resources.tick;
            var cross = global::renderdocui.Properties.Resources.cross;

            bool[] usedVBuffers = new bool[128];
            UInt32[] layoutOffs = new UInt32[128];

            for (int i = 0; i < 128; i++)
            {
                usedVBuffers[i] = false;
                layoutOffs[i] = 0;
            }

            ////////////////////////////////////////////////
            // Input Assembler

            if(state.m_IA.Bytecode == null)
                iaBytecode.Text = "None";
            else if (state.m_IA.Bytecode.DebugInfo == null || state.m_IA.Bytecode.DebugInfo.entryFunc.Length == 0)
                iaBytecode.Text = "Layout " + state.m_IA.layout.ToString();
            else
                iaBytecode.Text = state.m_IA.Bytecode.DebugInfo.entryFunc;

            iaBytecodeMismatch.Text = "";
            iaBytecodeMismatch.Visible = false;
            
            // check for IA-VS mismatches here.
            // This should be moved to a "Render Doctor" window reporting problems
            if (state.m_IA.Bytecode != null && state.m_VS.ShaderDetails != null)
            {
                string mismatchDetails = "";

                // VS wants more elements
                if (state.m_IA.Bytecode.InputSig.Length < state.m_VS.ShaderDetails.InputSig.Length)
                    mismatchDetails += "IA bytecode provides fewer elements than VS wants.\n";

                {
                    var IA = state.m_IA.Bytecode.InputSig;
                    var VS = state.m_VS.ShaderDetails.InputSig;

                    int count = Math.Min(IA.Length, VS.Length);

                    for (int i = 0; i < count; i++)
                    {
                        // misorder or misnamed semantics
                        if (IA[i].semanticIdxName.ToUpperInvariant() != VS[i].semanticIdxName.ToUpperInvariant())
                            mismatchDetails += String.Format("IA bytecode semantic {0}: {1} != VS bytecode semantic {0}: {2}\n", i,
                                                                IA[i].semanticIdxName, VS[i].semanticIdxName);

                        // VS wants more components
                        if (IA[i].compCount < VS[i].compCount)
                            mismatchDetails += String.Format("IA bytecode semantic {0} ({1}) is {3}-wide, VS bytecode semantic {0} ({1}) {2} is {4}-wide\n", i,
                                                                IA[i].semanticIdxName, VS[i].semanticIdxName,
                                                                IA[i].compCount, VS[i].compCount);
                        
                        // VS wants different types
                        if (IA[i].compType != VS[i].compType)
                            mismatchDetails += String.Format("IA bytecode semantic {0} ({1}) is {3}, VS bytecode semantic {0} ({2}) is {4}\n", i,
                                                                IA[i].semanticIdxName, VS[i].semanticIdxName,
                                                                IA[i].compType, VS[i].compType);
                    }
                }

                if (mismatchDetails.Length != 0)
                {
                    iaBytecodeMismatch.Text = "Warning: Mismatch detected between bytecode used to create IA and currently bound VS inputs";
                    toolTip.SetToolTip(iaBytecodeMismatch, mismatchDetails.Trim());
                    iaBytecodeMismatch.Visible = true;
                }
            }

            int vs = 0;
            
            vs = inputLayouts.VScrollValue();
            inputLayouts.Nodes.Clear();
            inputLayouts.BeginUpdate();
            if (state.m_IA.layouts != null)
            {
                int i = 0;
                foreach (var l in state.m_IA.layouts)
                {
                    string byteOffs = l.ByteOffset.ToString();

                    // D3D11 specific value
                    if (l.ByteOffset == uint.MaxValue)
                    {
                        byteOffs = String.Format("APPEND_ALIGNED ({0})", layoutOffs[l.InputSlot]);
                    }
                    else
                    {
                        layoutOffs[l.InputSlot] = l.ByteOffset;
                    }

                    layoutOffs[l.InputSlot] += l.Format.compByteWidth * l.Format.compCount;

                    var node = inputLayouts.Nodes.Add(new object[] {
                                              i, l.SemanticName, l.SemanticIndex.ToString(), l.Format, l.InputSlot.ToString(), byteOffs,
                                              l.PerInstance ? "PER_INSTANCE" : "PER_VERTEX", l.InstanceDataStepRate.ToString() });

                    usedVBuffers[l.InputSlot] = true;

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;

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

            if (state.m_IA.ibuffer != null && state.m_IA.ibuffer.Buffer != ResourceId.Null)
            {
                if (ibufferUsed || showDisabled.Checked)
                {
                    string ptr = "Buffer " + state.m_IA.ibuffer.Buffer.ToString();
                    string name = ptr;
                    UInt32 length = 1;

                    if (!ibufferUsed)
                    {
                        length = 0;
                    }

                    for (int t = 0; t < bufs.Length; t++)
                    {
                        if (bufs[t].ID == state.m_IA.ibuffer.Buffer)
                        {
                            name = bufs[t].name;
                            length = bufs[t].length;
                        }
                    }

                    var node = iabuffers.Nodes.Add(new object[] { "Index", name, draw != null ? draw.indexByteWidth : 0, state.m_IA.ibuffer.Offset, length });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = state.m_IA.ibuffer.Buffer;

                    if (!ibufferUsed)
                        InactiveRow(node);

                    if (state.m_IA.ibuffer.Buffer == ResourceId.Null)
                        EmptyRow(node);
                }
            }
            else
            {
                if (ibufferUsed || showEmpty.Checked)
                {
                    var node = iabuffers.Nodes.Add(new object[] { "Index", "No Buffer Set", "-", "-", "-" });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = state.m_IA.ibuffer.Buffer;

                    EmptyRow(node);

                    if (!ibufferUsed)
                        InactiveRow(node);
                }
            }

            m_VBNodes.Clear();

            if (state.m_IA.vbuffers != null)
            {
                int i = 0;
                foreach (var v in state.m_IA.vbuffers)
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

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == v.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;
                            }
                        }

                        TreelistView.Node node = null;
                        
                        if(filledSlot)
                            node = iabuffers.Nodes.Add(new object[] { i, name, v.Stride, v.Offset, length });
                        else
                            node = iabuffers.Nodes.Add(new object[] { i, "No Buffer Set", "-", "-", "-" });

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

            SetShaderState(texs, bufs, state.m_VS, vsShader, vsResources, vsSamplers, vsCBuffers, vsClasses);
            SetShaderState(texs, bufs, state.m_GS, gsShader, gsResources, gsSamplers, gsCBuffers, gsClasses);
            SetShaderState(texs, bufs, state.m_HS, hsShader, hsResources, hsSamplers, hsCBuffers, hsClasses);
            SetShaderState(texs, bufs, state.m_DS, dsShader, dsResources, dsSamplers, dsCBuffers, dsClasses);
            SetShaderState(texs, bufs, state.m_PS, psShader, psResources, psSamplers, psCBuffers, psClasses);
            SetShaderState(texs, bufs, state.m_CS, csShader, csResources, csSamplers, csCBuffers, csClasses);

            vs = csUAVs.VScrollValue();
            csUAVs.Nodes.Clear();
            csUAVs.BeginUpdate();

            if (state.m_CS.UAVs != null)
            {
                int i = 0;
                foreach (var r in state.m_CS.UAVs)
                {
                    ShaderResource shaderInput = null;

                    if (state.m_CS.ShaderDetails != null)
                    {
                        foreach (var bind in state.m_CS.ShaderDetails.Resources)
                        {
                            if (bind.IsReadWrite && bind.bindPoint == i)
                                shaderInput = bind;
                        }
                    }

                    bool filledSlot = (r.Resource != ResourceId.Null);
                    bool usedSlot = (shaderInput != null);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = "Unknown";
                        string name = "UAV " + r.Resource.ToString();
                        string typename = "Unknown";
                        object tag = null;

                        if (!filledSlot)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

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
                                typename = texs[t].resType.Str();

                                tag = texs[t];
                            }
                        }

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

                                if (bufs[t].structureSize > 0)
                                    typename = "StructuredBuffer[" + (bufs[t].length / bufs[t].structureSize) + "]";

                                if (r.Structured)
                                {
                                    typename += " (Count: " + r.BufferStructCount + ")";
                                }

                                if (shaderInput != null && !shaderInput.IsTexture)
                                {
                                    if (r.Format.compType == FormatComponentType.None)
                                    {
                                        if (shaderInput.variableType.members.Length > 0)
                                            format = "struct " + shaderInput.variableType.Name;
                                        else
                                            format = shaderInput.variableType.Name;
                                    }
                                    else
                                    {
                                        format = r.Format.ToString();
                                    }
                                }

                                tag = bufs[t];
                            }
                        }

                        var node = csUAVs.Nodes.Add(new object[] { i, name, typename, w, h, d, a, format });

                        node.Tag = tag;

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }

                    i++;
                }
            }
            csUAVs.NodesSelection.Clear();
            csUAVs.EndUpdate();
            csUAVs.SetVScrollValue(vs);

            bool streamoutSet = false;
            vs = gsStreams.VScrollValue();
            gsStreams.BeginUpdate();
            gsStreams.Nodes.Clear();
            if (state.m_SO.Outputs != null)
            {
                int i = 0;
                foreach (var s in state.m_SO.Outputs)
                {
                    bool filledSlot = (s.Buffer != ResourceId.Null);
                    bool usedSlot = (filledSlot);

                    streamoutSet |= filledSlot;

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string name = "Buffer " + s.Buffer.ToString();
                        uint length = 0;

                        if (!filledSlot)
                        {
                            name = "Empty";
                        }

                        FetchBuffer fetch = null;

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == s.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;

                                fetch = bufs[t];
                            }
                        }

                        var node = gsStreams.Nodes.Add(new object[] { i, name, length, s.Offset });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = fetch;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }
                    i++;
                }
            }
            gsStreams.EndUpdate();
            gsStreams.NodesSelection.Clear();
            gsStreams.SetVScrollValue(vs);

            gsStreams.Visible = gsStreams.Parent.Visible = streamoutSet;
            if (streamoutSet)
                geomTableLayout.ColumnStyles[1].Width = 50.0f;
            else
                geomTableLayout.ColumnStyles[1].Width = 0;

            ////////////////////////////////////////////////
            // Rasterizer

            vs = viewports.VScrollValue();
            viewports.BeginUpdate();
            viewports.Nodes.Clear();
            if (state.m_RS.Viewports != null)
            {
                int i = 0;
                foreach (var v in state.m_RS.Viewports)
                {
                    if (v.Width != v.Height || v.Width != 0 || v.Height != 0 || showEmpty.Checked)
                    {
                        var node = viewports.Nodes.Add(new object[] { i, v.TopLeft[0], v.TopLeft[1], v.Width, v.Height, v.MinDepth, v.MaxDepth });

                        if (v.Width == v.Height && v.Width == 0 && v.Height == 0)
                            EmptyRow(node);
                    }

                    i++;
                }
            }
            viewports.NodesSelection.Clear();
            viewports.EndUpdate();
            viewports.SetVScrollValue(vs);

            vs = scissors.VScrollValue();
            scissors.BeginUpdate();
            scissors.Nodes.Clear();
            if (state.m_RS.Scissors != null)
            {
                int i = 0;
                foreach (var s in state.m_RS.Scissors)
                {
                    if (s.right != 0 || s.bottom != 0 || showEmpty.Checked)
                    {
                        var node = scissors.Nodes.Add(new object[] { i, s.left, s.top, s.right - s.left, s.bottom - s.top });

                        if (s.right == 0 && s.bottom == 0)
                            EmptyRow(node);
                    }

                    i++;
                }
            }
            scissors.NodesSelection.Clear();
            scissors.EndUpdate();
            scissors.SetVScrollValue(vs);

            fillMode.Text = state.m_RS.m_State.FillMode.ToString();
            cullMode.Text = state.m_RS.m_State.CullMode.ToString();
            frontCCW.Image = state.m_RS.m_State.FrontCCW ? tick : cross;

            scissorEnable.Image = state.m_RS.m_State.ScissorEnable ? tick : cross;
            lineAAEnable.Image = state.m_RS.m_State.AntialiasedLineEnable ? tick : cross;
            multisampleEnable.Image = state.m_RS.m_State.MultisampleEnable ? tick : cross;

            depthClip.Image = state.m_RS.m_State.DepthClip ? tick : cross;
            depthBias.Text = state.m_RS.m_State.DepthBias.ToString();
            depthBiasClamp.Text = Formatter.Format(state.m_RS.m_State.DepthBiasClamp);
            slopeScaledBias.Text = Formatter.Format(state.m_RS.m_State.SlopeScaledDepthBias);
            forcedSampleCount.Text = state.m_RS.m_State.ForcedSampleCount.ToString();

            ////////////////////////////////////////////////
            // Output Merger

            bool[] targets = new bool[8];

            for (int i = 0; i < 8; i++)
                targets[i] = false;

            vs = targetOutputs.VScrollValue();
            targetOutputs.BeginUpdate();
            targetOutputs.Nodes.Clear();
            if (state.m_OM.RenderTargets != null)
            {
                int i = 0;
                foreach (var p in state.m_OM.RenderTargets)
                {
                    if (p.Resource != ResourceId.Null || showEmpty.Checked)
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = "Unknown";
                        string name = "Texture " + p.ToString();
                        string typename = "Unknown";
                        object tag = null;

                        if (p.Resource == ResourceId.Null)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == p.Resource)
                            {
                                w = texs[t].width;
                                h = texs[t].height;
                                d = texs[t].depth;
                                a = texs[t].arraysize;
                                format = texs[t].format.ToString();
                                name = texs[t].name;
                                typename = texs[t].resType.Str();

                                tag = texs[t];
                            }
                        }

                        var node = targetOutputs.Nodes.Add(new object[] { i, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (p.Resource == ResourceId.Null)
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

            if (state.m_OM.UAVs != null)
            {
                int i = 0;
                foreach (var r in state.m_OM.UAVs)
                {
                    ShaderResource shaderInput = null;

                    if (state.m_PS.ShaderDetails != null)
                    {
                        foreach (var bind in state.m_PS.ShaderDetails.Resources)
                        {
                            if (bind.IsReadWrite && bind.bindPoint == i + state.m_OM.UAVStartSlot)
                                shaderInput = bind;
                        }
                    }

                    bool filledSlot = (r.Resource != ResourceId.Null);
                    bool usedSlot = (shaderInput != null);

                    // note: we don't show empty UAVs as these "slots" are already showed as empty RTs.

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) // it's bound, but not referenced, and we have "show disabled"
                        )
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = "Unknown";
                        string name = "UAV " + r.Resource.ToString();
                        string typename = "Unknown";
                        object tag = null;

                        if (!filledSlot)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

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
                                typename = texs[t].resType.Str();

                                tag = texs[t];
                            }
                        }

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

                                if (bufs[t].structureSize > 0)
                                    typename = "StructuredBuffer[" + (bufs[t].length / bufs[t].structureSize) + "]";

                                if (r.Structured)
                                {
                                    typename += " (Count: " + r.BufferStructCount + ")";
                                }

                                if (shaderInput != null && !shaderInput.IsTexture)
                                {
                                    if (r.Format.compType == FormatComponentType.None)
                                    {
                                        if (shaderInput.variableType.members.Length > 0)
                                            format = "struct " + shaderInput.variableType.Name;
                                        else
                                            format = shaderInput.variableType.Name;
                                    }
                                    else
                                    {
                                        format = r.Format.ToString();
                                    }
                                }

                                tag = bufs[t];
                            }
                        }

                        var node = targetOutputs.Nodes.Add(new object[] { i + state.m_OM.UAVStartSlot, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (r.Resource == ResourceId.Null)
                            EmptyRow(node);
                    }

                    i++;
                }
            }

            if (state.m_OM.DepthTarget.Resource != ResourceId.Null || showEmpty.Checked)
            {
                UInt32 w = 1, h = 1, d = 1;
                UInt32 a = 1;
                string format = "Unknown";
                string name = "Depth Target " + state.m_OM.DepthTarget.Resource.ToString();
                string typename = "Unknown";
                object tag = null;

                if (state.m_OM.DepthTarget.Resource == ResourceId.Null)
                {
                    name = "Empty";
                    format = "-";
                    typename = "-";
                    w = h = d = a = 0;
                }

                for (int t = 0; t < texs.Length; t++)
                {
                    if (texs[t].ID == state.m_OM.DepthTarget.Resource)
                    {
                        w = texs[t].width;
                        h = texs[t].height;
                        d = texs[t].depth;
                        a = texs[t].arraysize;
                        format = texs[t].format.ToString();
                        name = texs[t].name;
                        typename = texs[t].resType.Str();

                        tag = texs[t];
                    }
                }

                var node = targetOutputs.Nodes.Add(new object[] { "Depth", name, typename, w, h, d, a, format });

                node.Image = global::renderdocui.Properties.Resources.action;
                node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                node.Tag = tag;

                if (state.m_OM.DepthTarget.Resource == ResourceId.Null)
                    EmptyRow(node);
            }
            targetOutputs.EndUpdate();
            targetOutputs.NodesSelection.Clear();
            targetOutputs.SetVScrollValue(vs);

            vs = blendOperations.VScrollValue();
            blendOperations.BeginUpdate();
            blendOperations.Nodes.Clear();
            {
                int i = 0;
                foreach(var blend in state.m_OM.m_BlendState.Blends)
                {
                    bool filledSlot = (blend.Enabled == true || targets[i]);
                    bool usedSlot = (targets[i]);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        var node = blendOperations.Nodes.Add(new object[] { i,
                                                        blend.Enabled,
                                                        blend.LogicEnabled,

                                                        blend.m_Blend.Source,
                                                        blend.m_Blend.Destination,
                                                        blend.m_Blend.Operation,

                                                        blend.m_AlphaBlend.Source,
                                                        blend.m_AlphaBlend.Destination,
                                                        blend.m_AlphaBlend.Operation,

                                                        blend.LogicOp,

                                                        ((blend.WriteMask & 0x1) == 0 ? "_" : "R") +
                                                        ((blend.WriteMask & 0x2) == 0 ? "_" : "G") +
                                                        ((blend.WriteMask & 0x4) == 0 ? "_" : "B") +
                                                        ((blend.WriteMask & 0x8) == 0 ? "_" : "A")
                    });


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


            alphaToCoverage.Image = state.m_OM.m_BlendState.AlphaToCoverage ? tick : cross;
            independentBlend.Image = state.m_OM.m_BlendState.IndependentBlend ? tick : cross;

            blendFactor.Text = state.m_OM.m_BlendState.BlendFactor[0].ToString("F2") + ", " +
                                state.m_OM.m_BlendState.BlendFactor[1].ToString("F2") + ", " +
                                state.m_OM.m_BlendState.BlendFactor[2].ToString("F2") + ", " +
                                state.m_OM.m_BlendState.BlendFactor[3].ToString("F2");

            sampleMask.Text = state.m_OM.m_BlendState.SampleMask.ToString("X8");

            depthEnable.Image = state.m_OM.m_State.DepthEnable ? tick : cross;
            depthFunc.Text = state.m_OM.m_State.DepthFunc;
            depthWrite.Image = state.m_OM.m_State.DepthWrites ? tick : cross;

            stencilEnable.Image = state.m_OM.m_State.StencilEnable ? tick : cross;
            stencilReadMask.Text = state.m_OM.m_State.StencilReadMask.ToString("X2");
            stencilWriteMask.Text = state.m_OM.m_State.StencilWriteMask.ToString("X2");
            stencilRef.Text = state.m_OM.m_State.StencilRef.ToString("X2");

            stencilFuncs.BeginUpdate();
            stencilFuncs.Nodes.Clear();
            stencilFuncs.Nodes.Add(new object[] { "Front", state.m_OM.m_State.m_FrontFace.Func, state.m_OM.m_State.m_FrontFace.FailOp, 
                                                 state.m_OM.m_State.m_FrontFace.DepthFailOp, state.m_OM.m_State.m_FrontFace.PassOp });
            stencilFuncs.Nodes.Add(new object[] { "Back", state.m_OM.m_State.m_BackFace.Func, state.m_OM.m_State.m_BackFace.FailOp, 
                                                 state.m_OM.m_State.m_BackFace.DepthFailOp, state.m_OM.m_State.m_BackFace.PassOp });
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
                    state.m_HS.Shader != ResourceId.Null,
                    state.m_DS.Shader != ResourceId.Null,
                    state.m_GS.Shader != ResourceId.Null,
                    true,
                    state.m_PS.Shader != ResourceId.Null,
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

            D3D11PipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage == null) return;

            if (tag is FetchTexture)
            {
                FetchTexture tex = (FetchTexture)tag;

                if (tex.resType == ShaderResourceType.Buffer)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(false, tex.ID);
                    viewer.Show(m_DockContent.DockPanel);
                }
                else
                {
                    var viewer = m_Core.GetTextureViewer();
                    viewer.Show(m_DockContent.DockPanel);
                    if (!viewer.IsDisposed)
                        viewer.ViewTexture(tex.ID, true);
                }
            }
            else if(tag is FetchBuffer)
            {
                FetchBuffer buf = (FetchBuffer)tag;
                D3D11PipelineState.ShaderStage.ResourceView view = null;

                string format = "";

                var deets = stage.ShaderDetails;

                int bind = -1;
                bool uav = false;

                for (int i = 0; i < stage.SRVs.Length; i++)
                {
                    if (stage.SRVs[i].Resource == buf.ID)
                    {
                        bind = i;
                        view = stage.SRVs[i];
                        break;
                    }
                }

                for (int i = 0; i < stage.UAVs.Length; i++)
                {
                    if (stage.UAVs[i].Resource == buf.ID)
                    {
                        bind = i;
                        uav = true;
                        view = stage.UAVs[i];
                        break;
                    }
                    if (stage == m_Core.CurD3D11PipelineState.m_PS &&
                        m_Core.CurD3D11PipelineState.m_OM.UAVs[i].Resource == buf.ID)
                    {
                        bind = i + (int)m_Core.CurD3D11PipelineState.m_OM.UAVStartSlot;
                        uav = true;
                        view = m_Core.CurD3D11PipelineState.m_OM.UAVs[i];
                        break;
                    }
                }

                if (deets != null)
                {
                    foreach (var r in deets.Resources)
                    {
                        if(r.IsTexture)
                            continue;

                        if ((r.IsSRV && !uav) || (r.IsReadWrite && uav))
                        {
                            if (r.bindPoint == bind)
                            {
                                if (r.variableType.members.Length == 0)
                                {
                                    if (view != null)
                                    {
                                        if (view.Format.special && view.Format.specialFormat == SpecialFormat.R10G10B10A2)
                                        {
                                            if (view.Format.compType == FormatComponentType.UInt) format = "uintten";
                                            if (view.Format.compType == FormatComponentType.UNorm) format = "unormten";
                                        }
                                        else if (!view.Format.special)
                                        {
                                            switch (view.Format.compByteWidth)
                                            {
                                                case 1:
                                                {
                                                    if (view.Format.compType == FormatComponentType.UNorm) format = "unormb";
                                                    if (view.Format.compType == FormatComponentType.SNorm) format = "snormb";
                                                    if (view.Format.compType == FormatComponentType.UInt) format = "ubyte";
                                                    if (view.Format.compType == FormatComponentType.SInt) format = "byte";
                                                    break;
                                                }
                                                case 2:
                                                {
                                                    if (view.Format.compType == FormatComponentType.UNorm) format = "unormh";
                                                    if (view.Format.compType == FormatComponentType.SNorm) format = "snormh";
                                                    if (view.Format.compType == FormatComponentType.UInt) format = "ushort";
                                                    if (view.Format.compType == FormatComponentType.SInt) format = "short";
                                                    if (view.Format.compType == FormatComponentType.Float) format = "half";
                                                    break;
                                                }
                                                case 4:
                                                {
                                                    if (view.Format.compType == FormatComponentType.UNorm) format = "unormf";
                                                    if (view.Format.compType == FormatComponentType.SNorm) format = "snormf";
                                                    if (view.Format.compType == FormatComponentType.UInt) format = "uint";
                                                    if (view.Format.compType == FormatComponentType.SInt) format = "int";
                                                    if (view.Format.compType == FormatComponentType.Float) format = "float";
                                                    break;
                                                }
                                            }

                                            format += view.Format.compCount;
                                        }
                                    }

                                    if (format == "" && r.variableType.Name.Length > 0)
                                        format = r.variableType.Name;

                                    format += " " + r.name + ";";
                                }
                                else
                                {
                                    format = "// struct " + r.variableType.Name + Environment.NewLine +
                                                "{" + Environment.NewLine + FormatMembers(1, "", r.variableType.members) + "}";
                                }
                                break;
                            }
                        }
                    }
                }

                if (buf.ID != ResourceId.Null)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    if (format.Length == 0)
                        viewer.ViewRawBuffer(true, buf.ID);
                    else
                        viewer.ViewRawBuffer(true, buf.ID, format);
                    viewer.Show(m_DockContent.DockPanel);
                }
            }
        }

        private void defaultCopyPaste_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.C && e.Control)
            {
                string text = "";

                if (sender is DataGridView)
                {
                    foreach (DataGridViewRow row in ((DataGridView)sender).SelectedRows)
                    {
                        foreach (var cell in row.Cells)
                            text += cell.ToString() + " ";
                        text += Environment.NewLine;
                    }
                }
                else if (sender is TreelistView.TreeListView)
                {
                    TreelistView.NodesSelection sel = ((TreelistView.TreeListView)sender).NodesSelection;

                    if (sel.Count > 0)
                    {
                        for (int i = 0; i < sel.Count; i++)
                        {
                            for (int v = 0; v < sel[i].Count; v++)
                                text += sel[i][v].ToString() + " ";
                            text += Environment.NewLine;
                        }
                    }
                    else
                    {
                        TreelistView.Node n = ((TreelistView.TreeListView)sender).SelectedNode;
                        for (int v = 0; v < n.Count; v++)
                            text += n[v].ToString() + " ";
                        text += Environment.NewLine;
                    }
                }

                if(text.Length > 0)
                    Clipboard.SetText(text);
            }
        }

        private void disableSelection_Leave(object sender, EventArgs e)
        {
            if (sender is DataGridView)
                ((DataGridView)sender).ClearSelection();
            else if (sender is TreelistView.TreeListView)
            {
                TreelistView.TreeListView tv = (TreelistView.TreeListView)sender;

                int vs = tv.VScrollValue();
                tv.NodesSelection.Clear();
                tv.FocusedNode = null;
                tv.SetVScrollValue(vs);
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
                    viewer.ViewRawBuffer(true, id);
                    viewer.Show(m_DockContent.DockPanel);
                }
            }
        }

        private void inputLayouts_NodeDoubleClick(TreelistView.Node node)
        {
            (new BufferViewer(m_Core, true)).Show(m_DockContent.DockPanel);
        }

        private D3D11PipelineState.ShaderStage GetStageForSender(object sender)
        {
            D3D11PipelineState.ShaderStage stage = null;

            if (!m_Core.LogLoaded)
                return null;

            object cur = sender;

            while (cur is Control)
            {
                if (cur == tabVS)
                    stage = m_Core.CurD3D11PipelineState.m_VS;
                else if (cur == tabGS)
                    stage = m_Core.CurD3D11PipelineState.m_GS;
                else if (cur == tabHS)
                    stage = m_Core.CurD3D11PipelineState.m_HS;
                else if (cur == tabDS)
                    stage = m_Core.CurD3D11PipelineState.m_DS;
                else if (cur == tabPS)
                    stage = m_Core.CurD3D11PipelineState.m_PS;
                else if (cur == tabCS)
                    stage = m_Core.CurD3D11PipelineState.m_CS;
                else if (cur == tabOM)
                    stage = m_Core.CurD3D11PipelineState.m_PS;

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
            if (sender == iaBytecode || sender == iaBytecodeCog)
            {
                if(m_Core.CurD3D11PipelineState != null &&
                    m_Core.CurD3D11PipelineState.m_IA.Bytecode != null)
                {
                    (new ShaderViewer(m_Core, m_Core.CurD3D11PipelineState.m_IA.Bytecode, ShaderStageType.Vertex, null, ""))
                        .Show(m_DockContent.DockPanel);
                }

                return;
            }

            D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

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

        // start a shaderviewer to edit this shader, optionally generating stub HLSL if there isn't
        // HLSL source available for this shader.
        private void shaderedit_Click(object sender, EventArgs e)
        {
            D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null || shaderDetails == null) return;

            var entryFunc = String.Format("EditedShader{0}S", stage.stage.ToString()[0]);

            string mainfile = "";

            var files = new Dictionary<string, string>();
            if (shaderDetails.DebugInfo.entryFunc.Length > 0 && shaderDetails.DebugInfo.files.Length > 0)
            {
                entryFunc = shaderDetails.DebugInfo.entryFunc;

                foreach (var s in shaderDetails.DebugInfo.files)
                    files.Add(Path.GetFileName(s.filename), s.filetext);

                mainfile = Path.GetFileName(shaderDetails.DebugInfo.files[0].filename);
            }
            else
            {
                var nl = Environment.NewLine;
                var nl2 = Environment.NewLine + Environment.NewLine;

                string hlsl = "// No HLSL available - function stub generated" + nl2;

                var shType = String.Format("{0}S", stage.stage.ToString()[0]);

                foreach (var res in shaderDetails.Resources)
                {
                    if (res.IsSampler)
                    {
                        hlsl += String.Format("//SamplerComparisonState {0} : register(s{1}); // can't disambiguate", res.name, res.bindPoint) + nl;
                        hlsl += String.Format("SamplerState {0} : register(s{1}); // can't disambiguate", res.name, res.bindPoint) + nl;
                    }
                    else
                    {
                        char regChar = 't';

                        if (res.IsReadWrite)
                        {
                            hlsl += "RW";
                            regChar = 'u';
                        }

                        if (res.IsTexture)
                        {
                            hlsl += String.Format("{0}<{1}> {2} : register({3}{4});", res.resType.ToString(), res.variableType.descriptor.name, res.name, regChar, res.bindPoint) + nl;
                        }
                        else
                        {
                            if (res.variableType.descriptor.rows == 1)
                                hlsl += String.Format("Buffer<{0}> {1} : register({2}{3});", res.variableType.descriptor.name, res.name, regChar, res.bindPoint) + nl;
                            else
                                hlsl += String.Format("StructuredBuffer<{0}> {1} : register({2}{3});", res.variableType.descriptor.name, res.name, regChar, res.bindPoint) + nl;
                        }
                    }
                }

                hlsl += nl2;

                string cbuffers = "";

                int cbufIdx = 0;
                foreach (var cbuf in shaderDetails.ConstantBlocks)
                {
                    if (cbuf.name.Length > 0 && cbuf.variables.Length > 0)
                    {
                        cbuffers += String.Format("cbuffer {0} : register(b{1}) {{", cbuf.name, cbufIdx) + nl;
                        MakeShaderVariablesHLSL(true, cbuf.variables, ref cbuffers, ref hlsl);
                        cbuffers += "};" + nl2;
                    }
                    cbufIdx++;
                }

                hlsl += cbuffers + nl2;

                hlsl += String.Format("struct {0}Input{1}{{{1}", shType, nl);
                foreach(var sig in shaderDetails.InputSig)
                    hlsl += String.Format("\t{0} {1} : {2};" + nl, sig.TypeString, sig.varName.Length > 0 ? sig.varName : ("param" + sig.regIndex), sig.D3D11SemanticString);
                hlsl += "};" + nl2;

                hlsl += String.Format("struct {0}Output{1}{{{1}", shType, nl);
                foreach (var sig in shaderDetails.OutputSig)
                    hlsl += String.Format("\t{0} {1} : {2};" + nl, sig.TypeString, sig.varName.Length > 0 ? sig.varName : ("param" + sig.regIndex), sig.D3D11SemanticString);
                hlsl += "};" + nl2;

                hlsl += String.Format("{0}Output {1}(in {0}Input IN){2}{{{2}\t{0}Output OUT = ({0}Output)0;{2}{2}\t// ...{2}{2}\treturn OUT;{2}}}{2}", shType, entryFunc, nl);

                mainfile = "generated.hlsl";

                files.Add(mainfile, hlsl);
            }

            if (files.Count == 0)
                return;

            ShaderViewer sv = new ShaderViewer(m_Core, false, entryFunc, files,

            // Save Callback
            (ShaderViewer viewer, Dictionary<string, string> updatedfiles) =>
            {
                string compileSource = updatedfiles.First().Value;

                // try and match up #includes against the files that we have. This isn't always
                // possible as fxc only seems to include the source for files if something in
                // that file was included in the compiled output. So you might end up with
                // dangling #includes - we just have to ignore them
                int offs = compileSource.IndexOf("#include");

                while(offs >= 0)
                {
                    // search back to ensure this is a valid #include (ie. not in a comment).
                    // Must only see whitespace before, then a newline.
                    int ws = Math.Max(0, offs-1);
                    while (ws >= 0 && (compileSource[ws] == ' ' || compileSource[ws] == '\t'))
                        ws--;

                    // not valid? jump to next.
                    if (ws > 0 && compileSource[ws] != '\n')
                    {
                        offs = compileSource.IndexOf("#include", offs + 1);
                        continue;
                    }

                    int start = ws+1;
                    
                    bool tail = true;

                    int lineEnd = compileSource.IndexOf("\n", start+1);
                    if(lineEnd == -1)
                    {
                        lineEnd = compileSource.Length;
                        tail = false;
                    }

                    ws = offs + "#include".Length;
                    while (compileSource[ws] == ' ' || compileSource[ws] == '\t')
                        ws++;

                    string line = compileSource.Substring(offs, lineEnd-offs+1);

                    if (compileSource[ws] != '<' && compileSource[ws] != '"')
                    {
                        viewer.ShowErrors("Invalid #include directive found:\r\n" + line);
                        return;
                    }

                    // find matching char, either <> or "";
                    int end = compileSource.IndexOf(compileSource[ws] == '"' ? '"' : '>', ws + 1);

                    if (end == -1)
                    {
                        viewer.ShowErrors("Invalid #include directive found:\r\n" + line);
                        return;
                    }

                    string fname = compileSource.Substring(ws + 1, end - ws - 1);

                    string fileText = "";

                    if (updatedfiles.ContainsKey(fname))
                        fileText = updatedfiles[fname];
                    else
                        fileText = "// Can't find file " + fname + "\n";

                    compileSource = compileSource.Substring(0, offs) + "\n\n" + fileText + "\n\n" + (tail ? compileSource.Substring(lineEnd + 1) : "");

                    // need to start searching from the beginning - wasteful but allows nested includes to work
                    offs = compileSource.IndexOf("#include");
                }

                if (updatedfiles.ContainsKey("@cmdline"))
                    compileSource = updatedfiles["@cmdline"] + "\n\n" + compileSource;

                // invoke off to the ReplayRenderer to replace the log's shader
                // with our edited one
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    string errs = "";

                    uint flags = shaderDetails.DebugInfo.compileFlags;

                    ResourceId from = stage.Shader;
                    ResourceId to = r.BuildTargetShader(entryFunc, compileSource, flags, stage.stage, out errs);

                    viewer.BeginInvoke((MethodInvoker)delegate { viewer.ShowErrors(errs); });
                    if (to == ResourceId.Null)
                    {
                        r.RemoveReplacement(from);
                    }
                    else
                    {
                        r.ReplaceResource(from, to);
                    }
                });
            },

            // Close Callback
            () =>
            {
                // remove the replacement on close (we could make this more sophisticated if there
                // was a place to control replaced resources/shaders).
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    r.RemoveReplacement(stage.Shader);
                });
            });

            sv.Show(m_DockContent.DockPanel);
        }

        private void ShowCBuffer(D3D11PipelineState.ShaderStage stage, UInt32 slot)
        {
            if (stage.ShaderDetails != null &&
                (stage.ShaderDetails.ConstantBlocks.Length <= slot ||
                 stage.ShaderDetails.ConstantBlocks[slot].name.Length == 0)
               )
            {
                // unused cbuffer, open regular buffer viewer
                var viewer = new BufferViewer(m_Core, false);

                if (stage.ConstantBuffers.Length < slot)
                    return;

                var buf = stage.ConstantBuffers[slot].Buffer;
                viewer.ViewRawBuffer(true, buf);
                viewer.Show(m_DockContent.DockPanel);

                return;
            }

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
            D3D11PipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage != null && node.Tag is UInt32)
            {
                ShowCBuffer(stage, (UInt32)node.Tag);
            }
        }

        private void CBuffers_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

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
                    string arr = "";
                    if (v.type.descriptor.elements > 1)
                        arr = String.Format("[{0}]", v.type.descriptor.elements);
                    ret += indentstr + v.type.Name + " " + nameprefix + v.name + arr + ";" + Environment.NewLine;
                }

                i++;
            }

            return ret;
        }

        private void shaderCog_MouseEnter(object sender, EventArgs e)
        {
            if (sender == iaBytecode || sender == iaBytecodeCog)
            {
                if (m_Core.CurD3D11PipelineState != null &&
                    m_Core.CurD3D11PipelineState.m_IA.Bytecode != null)
                    iaBytecodeCog.Image = global::renderdocui.Properties.Resources.action_hover;

                return;
            }

            if (sender is PictureBox)
            {
                D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage != null && stage.Shader != ResourceId.Null)
                    (sender as PictureBox).Image = global::renderdocui.Properties.Resources.action_hover;
            }

            if (sender is Label)
            {
                D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseEnter(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Domain) shaderCog_MouseEnter(dsShaderCog, e);
                if (stage.stage == ShaderStageType.Hull) shaderCog_MouseEnter(hsShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseEnter(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Pixel) shaderCog_MouseEnter(psShaderCog, e);
                if (stage.stage == ShaderStageType.Compute) shaderCog_MouseEnter(csShaderCog, e);
            }
        }

        private void shaderCog_MouseLeave(object sender, EventArgs e)
        {
            if (sender == iaBytecode || sender == iaBytecodeCog)
            {
                iaBytecodeCog.Image = global::renderdocui.Properties.Resources.action;

                return;
            }

            if (sender is PictureBox)
            {
                (sender as PictureBox).Image = global::renderdocui.Properties.Resources.action;
            }

            if (sender is Label)
            {
                D3D11PipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseLeave(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Domain) shaderCog_MouseLeave(dsShaderCog, e);
                if (stage.stage == ShaderStageType.Hull) shaderCog_MouseLeave(hsShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseLeave(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Pixel) shaderCog_MouseLeave(psShaderCog, e);
                if (stage.stage == ShaderStageType.Compute) shaderCog_MouseLeave(csShaderCog, e);
            }
        }

        private void pipeFlow_SelectedStageChanged(object sender, EventArgs e)
        {
            stageTabControl.SelectedIndex = pipeFlow.SelectedStage;
        }

        private void csDebug_Click(object sender, EventArgs e)
        {
            uint gx = 0, gy = 0, gz = 0;
            uint tx = 0, ty = 0, tz = 0;

            if (m_Core.CurD3D11PipelineState == null ||
                m_Core.CurD3D11PipelineState.m_CS.Shader == ResourceId.Null ||
                m_Core.CurD3D11PipelineState.m_CS.ShaderDetails == null)
                return;

            if (uint.TryParse(groupX.Text, out gx) &&
                uint.TryParse(groupY.Text, out gy) &&
                uint.TryParse(groupZ.Text, out gz) &&
                uint.TryParse(threadX.Text, out tx) &&
                uint.TryParse(threadY.Text, out ty) &&
                uint.TryParse(threadZ.Text, out tz))
            {
                ShaderDebugTrace trace = null;

                ShaderReflection shaderDetails = m_Core.CurD3D11PipelineState.m_CS.ShaderDetails;

                m_Core.Renderer.Invoke((ReplayRenderer r) =>
                {
                    trace = r.DebugThread(new uint[] { gx, gy, gz }, new uint[] { tx, ty, tz });
                });

                if (trace == null || trace.states.Length == 0)
                {
                    MessageBox.Show("Couldn't debug compute shader.", "Uh Oh!",
                                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }

                this.BeginInvoke(new Action(() =>
                {
                    string debugContext = String.Format("Group [{0},{1},{2}] Thread [{3},{4},{5}]", gx, gy, gz, tx, ty, tz);

                    ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, ShaderStageType.Compute, trace, debugContext);

                    s.Show(m_DockContent.DockPanel);
                }));
            }
            else
            {
                MessageBox.Show("Enter numbers for group and thread ID.", "Invalid thread", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
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
            if (m_Core.CurD3D11PipelineState == null) return;

            Point mousePoint = inputLayouts.PointToClient(Cursor.Position);
            var hoverNode = inputLayouts.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            var IA = m_Core.CurD3D11PipelineState.m_IA;

            if (hoverNode != null)
            {
                int index = inputLayouts.Nodes.GetNodeIndex(hoverNode);

                if (index >= 0 && index < IA.layouts.Length)
                {
                    uint slot = IA.layouts[index].InputSlot;

                    HighlightIASlot(slot);
                }
            }
        }

        private void HighlightIASlot(uint slot)
        {
            var IA = m_Core.CurD3D11PipelineState.m_IA;
        
            Color c = HSLColor(GetHueForVB((int)slot), 1.0f, 0.95f);

            if (slot < m_VBNodes.Count)
                m_VBNodes[(int)slot].DefaultBackColor = c;

            for (int i = 0; i < inputLayouts.Nodes.Count; i++)
            {
                var n = inputLayouts.Nodes[i];
                if (IA.layouts[i].InputSlot == slot)
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
            if (m_Core.CurD3D11PipelineState == null) return;

            Point mousePoint = iabuffers.PointToClient(Cursor.Position);
            var hoverNode = iabuffers.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            if (hoverNode != null)
            {
                int idx = m_VBNodes.IndexOf(hoverNode);
                if (idx >= 0)
                    HighlightIASlot((uint)idx);
                else
                    hoverNode.DefaultBackColor = SystemColors.ControlLight;
            }
        }

        private void ExportHTMLTable(XmlTextWriter writer, string[] cols, object[][] rows)
        {
            writer.WriteStartElement("table");

            {
                writer.WriteStartElement("thead");
                writer.WriteStartElement("tr");

                foreach (var col in cols)
                {
                    writer.WriteStartElement("th");
                    writer.WriteString(col);
                    writer.WriteEndElement();
                }

                writer.WriteEndElement();
                writer.WriteEndElement();
            }

            {
                writer.WriteStartElement("tbody");

                if (rows.Length == 0)
                {
                    writer.WriteStartElement("tr");

                    for (int i = 0; i < cols.Length; i++)
                    {
                        writer.WriteStartElement("td");
                        writer.WriteString("-");
                        writer.WriteEndElement();
                    }

                    writer.WriteEndElement();
                }
                else
                {
                    foreach (var row in rows)
                    {
                        writer.WriteStartElement("tr");

                        foreach (var el in row)
                        {
                            writer.WriteStartElement("td");
                            writer.WriteString(el.ToString());
                            writer.WriteEndElement();
                        }

                        writer.WriteEndElement();
                    }
                }

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        private void ExportHTMLTable(XmlTextWriter writer, string[] cols, object[] rows)
        {
            ExportHTMLTable(writer, cols, new object[][] { rows });
        }

        private object[] ExportViewHTML(D3D11PipelineState.ShaderStage.ResourceView view, int i, ShaderReflection refl, string extraParams)
        {
            FetchTexture[] texs = m_Core.CurTextures;
            FetchBuffer[] bufs = m_Core.CurBuffers;

            ShaderResource shaderInput = null;

            if (refl != null)
            {
                foreach (var bind in refl.Resources)
                {
                    if (bind.IsReadWrite && bind.bindPoint == i)
                        shaderInput = bind;
                }
            }

            string name = "Empty";
            string typename = "Unknown";
            string format = "Unknown";
            uint w = 0, h = 0, d = 0, a = 0;

            FetchTexture tex = null;
            FetchBuffer buf = null;

            // check to see if it's a texture
            for (int t = 0; t < texs.Length; t++)
            {
                if (texs[t].ID == view.Resource)
                {
                    w = texs[t].width;
                    h = texs[t].height;
                    d = texs[t].depth;
                    a = texs[t].arraysize;
                    format = texs[t].format.ToString();
                    name = texs[t].name;
                    typename = texs[t].resType.ToString();

                    tex = texs[t];
                }
            }

            // if not a texture, it must be a buffer
            for (int t = 0; t < bufs.Length; t++)
            {
                if (bufs[t].ID == view.Resource)
                {
                    w = bufs[t].length;
                    h = 0;
                    d = 0;
                    a = 0;
                    format = view.Format.ToString();
                    name = bufs[t].name;
                    typename = "Buffer";

                    // for structured buffers, display how many 'elements' there are in the buffer
                    if (bufs[t].structureSize > 0)
                        typename = "StructuredBuffer[" + (bufs[t].length / bufs[t].structureSize) + "]";

                    if (view.Structured)
                    {
                        typename += " (Count: " + view.BufferStructCount + ")";
                    }

                    if (shaderInput != null && !shaderInput.IsTexture)
                    {
                        if (view.Format.compType == FormatComponentType.None)
                        {
                            if (shaderInput.variableType.members.Length > 0)
                                format = "struct " + shaderInput.variableType.Name;
                            else
                                format = shaderInput.variableType.Name;
                        }
                        else
                        {
                            format = view.Format.ToString();
                        }
                    }

                    buf = bufs[t];
                }
            }

            string viewParams = "";

            if(buf != null)
            {
                viewParams = String.Format("First Element: {0}, Num Elements {1}, Flags {2}", view.FirstElement, view.NumElements, view.Flags);
            }

            if (tex != null)
            {
                if(tex.mips > 1)
                    viewParams = String.Format("Highest Mip: {0}, Num Mips: {1}", view.HighestMip, view.NumMipLevels);

                if (tex.arraysize > 1)
                {
                    if (viewParams.Length > 0)
                        viewParams += ", ";
                    viewParams += String.Format("First Slice: {0}, Array Size: {1}", view.FirstArraySlice, view.ArraySize);
                }
            }

            if (viewParams == "")
                viewParams = extraParams;
            else
                viewParams += ", " + extraParams;

            return new object[] {
                        i, name,
                        view.Type, typename,
                        w, h, d, a,
                        view.Format, format,
                        viewParams };
        }

        private void ExportHTML(XmlTextWriter writer, D3D11PipelineState.InputAssembler ia)
        {
            FetchBuffer[] bufs = m_Core.CurBuffers;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Input Layouts");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var l in ia.layouts)
                {
                    rows.Add(new object[] { i, l.SemanticName, l.SemanticIndex, l.Format, l.InputSlot, l.ByteOffset, l.PerInstance, l.InstanceDataStepRate });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "Semantic Name", "Semantic Index", "Format", "Input Slot", "Byte Offset", "Per Instance", "Instance Data Step Rate" }, rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Vertex Buffers");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var vb in ia.vbuffers)
                {
                    string name = "Buffer " + vb.Buffer.ToString();
                    UInt32 length = 0;

                    if (vb.Buffer == ResourceId.Null)
                    {
                        continue;
                    }
                    else
                    {
                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == vb.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;
                            }
                        }
                    }

                    rows.Add(new object[] { i, name, vb.Stride, vb.Offset, length });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "Buffer", "Stride", "Offset", "Byte Length" }, rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Index Buffer");
                writer.WriteEndElement();

                string name = "Buffer " + ia.ibuffer.Buffer.ToString();
                UInt32 length = 0;

                if (ia.ibuffer.Buffer == ResourceId.Null)
                {
                    name = "Empty";
                }
                else
                {
                    for (int t = 0; t < bufs.Length; t++)
                    {
                        if (bufs[t].ID == ia.ibuffer.Buffer)
                        {
                            name = bufs[t].name;
                            length = bufs[t].length;
                        }
                    }
                }

                string ifmt = "UNKNOWN";
                if (m_Core.CurDrawcall.indexByteWidth == 2)
                    ifmt = "R16_UINT";
                if (m_Core.CurDrawcall.indexByteWidth == 4)
                    ifmt = "R32_UINT";

                ExportHTMLTable(writer, new string[] { "Buffer", "Format", "Offset", "Byte Length" },
                    new object[] { name, ifmt, ia.ibuffer.Offset.ToString(), length.ToString() });
            }

            writer.WriteStartElement("p");
            writer.WriteEndElement();

            ExportHTMLTable(writer, new string[] { "Primitive Topology" }, new object[] { m_Core.CurDrawcall.topology.Str() });
        }

        private void ExportHTML(XmlTextWriter writer, D3D11PipelineState.ShaderStage sh)
        {
            FetchBuffer[] bufs = m_Core.CurBuffers;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Shader");
                writer.WriteEndElement();

                ShaderReflection shaderDetails = sh.ShaderDetails;

                string shadername = "Unknown";

                if (sh.Shader == ResourceId.Null)
                    shadername = "Unbound";
                else
                    shadername = sh.ShaderName;

                if (shaderDetails != null && shaderDetails.DebugInfo.entryFunc.Length > 0 && shaderDetails.DebugInfo.files.Length > 0)
                    shadername = shaderDetails.DebugInfo.entryFunc + "()" + " - " +
                                    Path.GetFileName(shaderDetails.DebugInfo.files[0].filename);

                writer.WriteStartElement("p");
                writer.WriteString(shadername);
                writer.WriteEndElement();

                if (sh.Shader == ResourceId.Null)
                    return;
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Resources");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < sh.SRVs.Length; i++)
                {
                    if (sh.SRVs[i].View == ResourceId.Null) continue;

                    rows.Add(ExportViewHTML(sh.SRVs[i], i, null, ""));
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Name",
                        "View Type", "Resource Type",
                        "Width", "Height", "Depth", "Array Size",
                        "View Format", "Resource Format",
                        "View Parameters",
                    },
                    rows.ToArray());
            }

            if (sh.stage == ShaderStageType.Compute)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Unordered Access Views");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < sh.UAVs.Length; i++)
                {
                    if (sh.UAVs[i].View == ResourceId.Null) continue;

                    rows.Add(ExportViewHTML(sh.UAVs[i], i, null, ""));
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Name",
                        "View Type", "Resource Type",
                        "Width", "Height", "Depth", "Array Size",
                        "View Format", "Resource Format",
                        "View Parameters",
                    },
                    rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Samplers");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < sh.Samplers.Length; i++)
                {
                    D3D11PipelineState.ShaderStage.Sampler s = sh.Samplers[i];

                    if (s.Samp == ResourceId.Null) continue;

                    string borderColor = s.BorderColor[0].ToString() + ", " +
                                            s.BorderColor[1].ToString() + ", " +
                                            s.BorderColor[2].ToString() + ", " +
                                            s.BorderColor[3].ToString();

                    string addressing = "";

                    string addPrefix = "";
                    string addVal = "";

                    string[] addr = { s.AddressU, s.AddressV, s.AddressW };

                    // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
                    for (int a = 0; a < 3; a++)
                    {
                        string prefix = "" + "UVW"[a];

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

                    rows.Add(new object[] { i, addressing, borderColor,
                                            s.Comparison, s.Filter, s.MaxAniso.ToString(),
                                            s.MinLOD == -float.MaxValue ? "-FLT_MAX" : s.MinLOD.ToString(),
                                            s.MaxLOD == float.MaxValue ? "FLT_MAX" : s.MaxLOD.ToString(),
                                            s.MipLODBias.ToString() });
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Addressing", "Border Colour",
                        "Comparison", "Filter", "Max Anisotropy",
                        "Min LOD", "Max LOD", "Mip Bias",
                    },
                    rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Constant Buffers");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < sh.ConstantBuffers.Length; i++)
                {
                    ConstantBlock shaderCBuf = null;

                    if (sh.ConstantBuffers[i].Buffer == ResourceId.Null) continue;

                    if (sh.ShaderDetails != null && i < sh.ShaderDetails.ConstantBlocks.Length &&
                        sh.ShaderDetails.ConstantBlocks[i].name.Length > 0)
                        shaderCBuf = sh.ShaderDetails.ConstantBlocks[i];

                    string name = "Constant Buffer " + sh.ConstantBuffers[i].Buffer.ToString();
                    UInt32 length = 1;
                    int numvars = shaderCBuf != null ? shaderCBuf.variables.Length : 0;

                    if (sh.ConstantBuffers[i].Buffer == ResourceId.Null)
                    {
                        name = "Empty";
                        length = 0;
                    }

                    for (int t = 0; t < bufs.Length; t++)
                    {
                        if (bufs[t].ID == sh.ConstantBuffers[i].Buffer)
                        {
                            name = bufs[t].name;
                            length = bufs[t].length;
                        }
                    }

                    rows.Add(new object[] { i, name, sh.ConstantBuffers[i].VecOffset, sh.ConstantBuffers[i].VecCount, numvars, length });
                }

                ExportHTMLTable(writer, new string[] { "Slot", "Buffer", "Vector Offset", "Vector Count", "Number of Variables", "Byte Length", }, rows.ToArray());
            }

            if (sh.ClassInstances.Length > 0)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Class Instances");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < sh.ClassInstances.Length; i++)
                {
                    string interfaceName = String.Format("Interface {0}", i);

                    if (sh.ShaderDetails != null && i < sh.ShaderDetails.Interfaces.Length)
                        interfaceName = sh.ShaderDetails.Interfaces[i].Name;

                    rows.Add(new object[] { i, interfaceName, sh.ClassInstances[i] });
                }

                ExportHTMLTable(writer, new string[] { "Slot", "Interface Name", "Instance Name", }, rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, D3D11PipelineState.Streamout so)
        {
            FetchBuffer[] bufs = m_Core.CurBuffers;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Stream Out Targets");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var o in so.Outputs)
                {
                    string name = "Buffer " + o.Buffer.ToString();
                    UInt32 length = 0;

                    if (o.Buffer == ResourceId.Null)
                    {
                        name = "Empty";
                    }
                    else
                    {
                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == o.Buffer)
                            {
                                name = bufs[t].name;
                                length = bufs[t].length;
                            }
                        }
                    }

                    rows.Add(new object[] { i, name, o.Offset, length });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "Buffer", "Offset", "Byte Length" }, rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, D3D11PipelineState.Rasterizer rs)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("States");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Fill Mode", "Cull Mode", "Front CCW" },
                    new object[] { rs.m_State.FillMode, rs.m_State.CullMode, rs.m_State.FrontCCW ? "Yes" : "No" });

                writer.WriteStartElement("p");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Scissor Enable", "Line AA Enable", "Multisample Enable", "Forced Sample Count" },
                    new object[] { rs.m_State.ScissorEnable ? "Yes" : "No", rs.m_State.AntialiasedLineEnable ? "Yes" : "No",
                                   rs.m_State.MultisampleEnable ? "Yes" : "No", rs.m_State.ForcedSampleCount });

                writer.WriteStartElement("p");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Depth Clip", "Depth Bias", "Depth Bias Clamp", "Slope Scaled Bias" },
                    new object[] { rs.m_State.DepthClip ? "Yes" : "No", rs.m_State.DepthBias,
                                   Formatter.Format(rs.m_State.DepthBiasClamp), Formatter.Format(rs.m_State.SlopeScaledDepthBias)});
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Viewports");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var v in rs.Viewports)
                {
                    if (v.Width == v.Height && v.Width == 0 && v.Height == 0) continue;

                    rows.Add(new object[] { i, v.TopLeft[0], v.TopLeft[1], v.Width, v.Height, v.MinDepth, v.MaxDepth });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "X", "Y", "Width", "Height", "Min Depth", "Max Depth" }, rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Scissors");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var s in rs.Scissors)
                {
                    if (s.right == 0 && s.bottom == 0) continue;

                    rows.Add(new object[] { i, s.left, s.top, s.right - s.left, s.bottom - s.top });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "X", "Y", "Width", "Height" }, rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, D3D11PipelineState.OutputMerger om)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Blend State");
                writer.WriteEndElement();
                
                var blendFactor = om.m_BlendState.BlendFactor[0].ToString("F2") + ", " +
                                    om.m_BlendState.BlendFactor[1].ToString("F2") + ", " +
                                    om.m_BlendState.BlendFactor[2].ToString("F2") + ", " +
                                    om.m_BlendState.BlendFactor[3].ToString("F2");

                ExportHTMLTable(writer,
                    new string[] { "Independent Blend Enable", "Alpha to Coverage", "Sample Mask", "Blend Factor" },
                    new object[] { om.m_BlendState.IndependentBlend ? "Yes" : "No", om.m_BlendState.AlphaToCoverage ? "Yes" : "No",
                                   om.m_BlendState.SampleMask.ToString("X8"), blendFactor, });

                writer.WriteStartElement("h3");
                writer.WriteString("Target Blends");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var b in om.m_BlendState.Blends)
                {
                    if (!b.Enabled) continue;

                    rows.Add(new object[] {
                        i,
                        b.Enabled ? "Yes" : "No", b.LogicEnabled ? "Yes" : "No",
                        b.m_Blend.Source, b.m_Blend.Destination, b.m_Blend.Operation,
                        b.m_AlphaBlend.Source, b.m_AlphaBlend.Destination, b.m_AlphaBlend.Operation,
                        b.LogicOp, b.WriteMask });

                    i++;
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot",
                        "Blend Enable", "Logic Enable",
                        "Blend Source", "Blend Destination", "Blend Operation",
                        "Alpha Blend Source", "Alpha Blend Destination", "Alpha Blend Operation",
                        "Logic Operation", "Write Mask",
                    },
                    rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Depth State");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Depth Test Enable", "Depth Writes Enable", "Depth Function" },
                    new object[] { om.m_State.DepthEnable ? "Yes" : "No", om.m_State.DepthWrites ? "Yes" : "No", om.m_State.DepthFunc });
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Render targets");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < om.RenderTargets.Length; i++)
                {
                    if (om.RenderTargets[i].View == ResourceId.Null) continue;

                    rows.Add(ExportViewHTML(om.RenderTargets[i], i, null, ""));
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Name",
                        "View Type", "Resource Type",
                        "Width", "Height", "Depth", "Array Size",
                        "View Format", "Resource Format",
                        "View Parameters",
                    },
                    rows.ToArray());
            }

            if(om.UAVs.Length > 0 && om.UAVs[0].View != ResourceId.Null)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Unordered Access Views");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;

                for (; i < om.UAVStartSlot; i++)
                    rows.Add(new object[] { i, "Empty", "", "", "", "", 0, 0, 0, 0, "", "", "" });

                for (; i < om.RenderTargets.Length; i++)
                {
                    if (om.UAVs[i - om.UAVStartSlot].View == ResourceId.Null) continue;

                    rows.Add(ExportViewHTML(om.UAVs[i - om.UAVStartSlot], i, m_Core.CurD3D11PipelineState.m_PS.ShaderDetails, ""));
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Name",
                        "View Type", "Resource Type",
                        "Width", "Height", "Depth", "Array Size",
                        "View Format", "Resource Format",
                        "View Parameters",
                    },
                    rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Depth target");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                string extra = "";

                if(om.DepthReadOnly && om.StencilReadOnly)
                    extra = "Depth & Stencil Read-Only";
                else if (om.DepthReadOnly)
                    extra = "Depth Read-Only";
                else if (om.StencilReadOnly)
                    extra = "Stencil Read-Only";

                rows.Add(ExportViewHTML(om.DepthTarget, 0, null, extra));

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot", "Name",
                        "View Type", "Resource Type",
                        "Width", "Height", "Depth", "Array Size",
                        "View Format", "Resource Format",
                        "View Parameters",
                    },
                    rows.ToArray());
            }
        }

        private void ExportHTML(string filename)
        {
            using (Stream s = new FileStream(filename, FileMode.Create))
            {
                StreamWriter sw = new StreamWriter(s);

                sw.WriteLine("<!DOCTYPE html>");

                using (XmlTextWriter writer = new XmlTextWriter(sw))
                {
                    writer.Formatting = Formatting.Indented;
                    writer.Indentation = 4;

                    writer.WriteStartElement("html");
                    writer.WriteAttributeString("lang", "en");

                    var title = String.Format("{0} EID {1} - D3D11 Pipeline export", Path.GetFileName(m_Core.LogFileName), m_Core.CurEvent);

                    var css = @"
/* If you think this css is ugly/bad, open a pull request! */
body { margin: 20px; }
div.stage { border: 1px solid #BBBBBB; border-radius: 5px; padding: 16px; margin-bottom: 32px; }
div.stage h1 { text-decoration: underline; margin-top: 0px; }
div.stage table { border: 1px solid #AAAAAA; border-collapse: collapse; }
div.stage table thead tr { border-bottom: 1px solid #AAAAAA; background-color: #EEEEFF; }
div.stage table tr th { border-right: 1px solid #AAAAAA; padding: 6px; }
div.stage table tr td { border-right: 1px solid #AAAAAA; background-color: #EEEEEE; padding: 3px; }
";

                    {
                        writer.WriteStartElement("head");

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("charset", "utf-8");
                        writer.WriteEndElement();

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("http-equiv", "X-UA-Compatible");
                        writer.WriteAttributeString("content", "IE=edge");
                        writer.WriteEndElement();

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("name", "viewport");
                        writer.WriteAttributeString("content", "width=device-width, initial-scale=1");
                        writer.WriteEndElement();

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("name", "description");
                        writer.WriteAttributeString("content", "");
                        writer.WriteEndElement();

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("name", "author");
                        writer.WriteAttributeString("content", "");
                        writer.WriteEndElement();

                        writer.WriteStartElement("meta");
                        writer.WriteAttributeString("http-equiv", "Content-Type");
                        writer.WriteAttributeString("content", "text/html;charset=utf-8");
                        writer.WriteEndElement();

                        writer.WriteStartElement("title");
                        writer.WriteString(title);
                        writer.WriteEndElement();

                        writer.WriteStartElement("style");
                        writer.WriteComment(css);
                        writer.WriteEndElement();

                        writer.WriteEndElement(); // head
                    }

                    {
                        writer.WriteStartElement("body");

                        writer.WriteStartElement("h1");
                        writer.WriteString(title);
                        writer.WriteEndElement();

                        writer.WriteStartElement("h3");
                        {
                            var context = String.Format("Frame {0}", m_Core.FrameInfo[m_Core.CurFrame].frameNumber);

                            FetchDrawcall draw = m_Core.CurDrawcall;

                            var drawstack = new List<FetchDrawcall>();
                            var parent = draw.parent;
                            while (parent != null)
                            {
                                drawstack.Add(parent);
                                parent = parent.parent;
                            }

                            drawstack.Reverse();

                            foreach (var d in drawstack)
                            {
                                context += String.Format(" > {0}", d.name);
                            }

                            FetchDrawcall prev = null;

                            for (uint i = draw.events[0].eventID; i <= draw.events.Last().eventID; i++)
                            {
                                prev = m_Core.GetDrawcall(m_Core.CurFrame, i);
                                if (prev != null)
                                    break;
                            }

                            if (prev != null)
                                context += String.Format(" => {0}, {1}", prev.name, draw.name);
                            else
                                context += String.Format(" => {0}", draw.name);

                            writer.WriteString(context);
                        }
                        writer.WriteEndElement(); // h2 context

                        string[] stageNames = new string[]{
                            "Input Assembler",
                            "Vertex Shader",
                            "Hull Shader",
                            "Domain Shader",
                            "Geometry Shader",
                            "Stream Out",
                            "Rasterizer",
                            "Pixel Shader",
                            "Output Merger",
                            "Compute Shader",
                        };

                        string[] stageAbbrevs = new string[]{
                            "IA", "VS", "HS", "DS", "GS", "SO", "RS", "PS", "OM", "CS"
                        };

                        int stage=0;
                        foreach(var sn in stageNames)
                        {
                            writer.WriteStartElement("div");
                            writer.WriteStartElement("a");
                            writer.WriteAttributeString("name", stageAbbrevs[stage]);
                            writer.WriteEndElement();
                            writer.WriteEndElement();

                            writer.WriteStartElement("div");
                            writer.WriteAttributeString("class", "stage");

                            writer.WriteStartElement("h1");
                            writer.WriteString(sn);
                            writer.WriteEndElement();

                            switch(stage)
                            {
                                case 0: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_IA); break;
                                case 1: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_VS); break;
                                case 2: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_HS); break;
                                case 3: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_DS); break;
                                case 4: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_GS); break;
                                case 5: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_SO); break;
                                case 6: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_RS); break;
                                case 7: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_PS); break;
                                case 8: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_OM); break;
                                case 9: ExportHTML(writer, m_Core.CurD3D11PipelineState.m_CS); break;
                            }

                            writer.WriteEndElement();

                            stage++;
                        }

                        writer.WriteEndElement(); // body
                    }

                    writer.WriteEndElement(); // html

                    writer.Close();
                }

                sw.Dispose();
            }
        }

        private void export_Click(object sender, EventArgs e)
        {
            if (!m_Core.LogLoaded) return;
            
            DialogResult res = exportDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                ExportHTML(exportDialog.FileName);
            }
        }
   }
}