/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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
        private struct ReadWriteTag
        {
            public ReadWriteTag(UInt32 i, FetchBuffer b)
            {
                idx = i;
                buf = b;
            }

            public UInt32 idx;
            public FetchBuffer buf;
        };

        private struct IABufferTag
        {
            public IABufferTag(ResourceId i, ulong offs)
            {
                id = i;
                offset = offs;
            }

            public ResourceId id;
            public ulong offset;
        };

        private Core m_Core;
        private DockContent m_DockContent;

        // keep track of the VB nodes (we want to be able to highlight them easily on hover)
        private List<TreelistView.Node> m_VBNodes = new List<TreelistView.Node>();

        private enum GLReadWriteType
        {
            Atomic,
            SSBO,
            Image,
        };

        public GLPipelineStateViewer(Core core, DockContent c)
        {
            InitializeComponent();

            m_DockContent = c;

            inputLayouts.Font = core.Config.PreferredFont;
            iabuffers.Font = core.Config.PreferredFont;

            gsFeedback.Font = core.Config.PreferredFont;

            groupX.Font = groupY.Font = groupZ.Font = core.Config.PreferredFont;
            threadX.Font = threadY.Font = threadZ.Font = core.Config.PreferredFont;

            vsShader.Font = vsTextures.Font = vsSamplers.Font = vsCBuffers.Font = vsSubroutines.Font = vsReadWrite.Font = core.Config.PreferredFont;
            gsShader.Font = gsTextures.Font = gsSamplers.Font = gsCBuffers.Font = gsSubroutines.Font = gsReadWrite.Font = core.Config.PreferredFont;
            tcsShader.Font = tcsTextures.Font = tcsSamplers.Font = tcsCBuffers.Font = tcsSubroutines.Font = tcsReadWrite.Font = core.Config.PreferredFont;
            tesShader.Font = tesTextures.Font = tesSamplers.Font = tesCBuffers.Font = tesSubroutines.Font = tesReadWrite.Font = core.Config.PreferredFont;
            fsShader.Font = fsTextures.Font = fsSamplers.Font = fsCBuffers.Font = fsSubroutines.Font = fsReadWrite.Font = core.Config.PreferredFont;
            csShader.Font = csTextures.Font = csSamplers.Font = csCBuffers.Font = csSubroutines.Font = csReadWrite.Font = core.Config.PreferredFont;

            viewports.Font = core.Config.PreferredFont;
            scissors.Font = core.Config.PreferredFont;

            targetOutputs.Font = core.Config.PreferredFont;
            blendOperations.Font = core.Config.PreferredFont;

            pipeFlow.Font = new System.Drawing.Font(core.Config.PreferredFont.FontFamily, 11.25F,
                System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

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

            ClearShaderState(vsShader, vsTextures, vsSamplers, vsCBuffers, vsSubroutines, vsReadWrite);
            ClearShaderState(gsShader, gsTextures, gsSamplers, gsCBuffers, gsSubroutines, gsReadWrite);
            ClearShaderState(tesShader, tesTextures, tesSamplers, tesCBuffers, tesSubroutines, tesReadWrite);
            ClearShaderState(tcsShader, tcsTextures, tcsSamplers, tcsCBuffers, tcsSubroutines, tcsReadWrite);
            ClearShaderState(fsShader, fsTextures, fsSamplers, fsCBuffers, fsSubroutines, fsReadWrite);
            ClearShaderState(csShader, csTextures, csSamplers, csCBuffers, csSubroutines, csReadWrite);

            gsFeedback.Nodes.Clear();

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

            multisampleEnable.Image = tick;
            sampleShading.Image = tick;
            minSampleShadingRate.Text = "0.0";
            alphaToCoverage.Image = tick;
            alphaToOne.Image = tick;
            sampleCoverage.Text = "";
            sampleCoverage.Image = cross;
            sampleMask.Text = "";
            sampleMask.Image = cross;

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
            OnEventSelected(m_Core.CurEvent);
        }

        private void EmptyRow(TreelistView.Node node)
        {
            node.BackColor = Color.Firebrick;
        }

        private void InactiveRow(TreelistView.Node node)
        {
            node.Italic = true;
        }

        private void ClearShaderState(Label shader, TreelistView.TreeListView textures, TreelistView.TreeListView samplers,
                                      TreelistView.TreeListView cbuffers, TreelistView.TreeListView subroutines,
                                      TreelistView.TreeListView readwrites)
        {
            shader.Text = "Unbound";
            textures.Nodes.Clear();
            samplers.Nodes.Clear();
            cbuffers.Nodes.Clear();
            subroutines.Nodes.Clear();
            readwrites.Nodes.Clear();
        }

        private GLReadWriteType GetGLReadWriteType(ShaderResource res)
        {
            GLReadWriteType ret = GLReadWriteType.Image;

            if (res.IsTexture)
            {
                ret = GLReadWriteType.Image;
            }
            else
            {
                if (res.variableType.descriptor.rows == 1 &&
                    res.variableType.descriptor.cols == 1 &&
                    res.variableType.descriptor.type == VarType.UInt)
                {
                    ret = GLReadWriteType.Atomic;
                }
                else
                {
                    ret = GLReadWriteType.SSBO;
                }
            }

            return ret;
        }

        // Set a shader stage's resources and values
        private void SetShaderState(FetchTexture[] texs, FetchBuffer[] bufs,
                                    GLPipelineState state, GLPipelineState.ShaderStage stage,
                                    TableLayoutPanel table, Label shader,
                                    TreelistView.TreeListView textures, TreelistView.TreeListView samplers,
                                    TreelistView.TreeListView cbuffers, TreelistView.TreeListView subs,
                                    TreelistView.TreeListView readwrites)
        {
            ShaderReflection shaderDetails = stage.ShaderDetails;
            var mapping = stage.BindpointMapping;

            if (stage.Shader == ResourceId.Null)
            {
                shader.Text = "Unbound";
            }
            else
            {
                string shaderName = stage.stage.Str(APIPipelineStateType.OpenGL) + " Shader";

                if (!stage.customShaderName && !stage.customProgramName && !stage.customPipelineName)
                {
                    shader.Text = shaderName + stage.Shader.ToString();
                }
                else
                {
                    if (stage.customShaderName)
                        shaderName = stage.ShaderName;

                    if (stage.customProgramName)
                        shaderName = stage.ProgramName + " - " + shaderName;

                    if (stage.customPipelineName && stage.PipelineActive)
                        shaderName = stage.PipelineName + " - " + shaderName;

                    shader.Text = shaderName;
                }
            }

            // disabled since entry function is always main, and filenames have no names, so this is useless.
            /*
            if (shaderDetails != null && shaderDetails.DebugInfo.entryFunc != "" && shaderDetails.DebugInfo.files.Length > 0)
                shader.Text = shaderDetails.DebugInfo.entryFunc + "()" + " - " +
                                Path.GetFileName(shaderDetails.DebugInfo.files[0].filename);
             */

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
                        foreach (var bind in shaderDetails.ReadOnlyResources)
                        {
                            if (bind.IsSRV && mapping.ReadOnlyResources[bind.bindPoint].bind == i)
                            {
                                shaderInput = bind;
                                map = mapping.ReadOnlyResources[bind.bindPoint];
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
                                    typename = texs[t].resType.Str();

                                    if (texs[t].format.special &&
                                        (texs[t].format.specialFormat == SpecialFormat.D16S8 ||
                                         texs[t].format.specialFormat == SpecialFormat.D24S8 ||
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
            if (shaderDetails != null)
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
                UInt32 i = 0;
                foreach (var subval in stage.Subroutines)
                {
                    subs.Nodes.Add(new object[] { i.ToString(), subval.ToString() });

                    i++;
                }
            }
            subs.EndUpdate();
            subs.NodesSelection.Clear();
            subs.SetVScrollValue(vs);

            {
                subs.Visible = subs.Parent.Visible = (stage.Subroutines.Length > 0);
                int row = table.GetRow(subs.Parent);
                if (row >= 0 && row < table.RowStyles.Count)
                {
                    if (stage.Subroutines.Length > 0)
                        table.RowStyles[row].Height = table.RowStyles[1].Height;
                    else
                        table.RowStyles[row].Height = 0;
                }
            }

            vs = readwrites.VScrollValue();
            readwrites.BeginUpdate();
            readwrites.Nodes.Clear();
            if (shaderDetails != null)
            {
                UInt32 i = 0;
                foreach (var res in shaderDetails.ReadWriteResources)
                {
                    int bindPoint = stage.BindpointMapping.ReadWriteResources[i].bind;

                    GLReadWriteType readWriteType = GetGLReadWriteType(res);

                    GLPipelineState.Buffer bf = null;
                    GLPipelineState.ImageLoadStore im = null;
                    ResourceId id = ResourceId.Null;

                    if (readWriteType == GLReadWriteType.Image && bindPoint >= 0 && bindPoint < state.Images.Length)
                    {
                        im = state.Images[bindPoint];
                        id = state.Images[bindPoint].Resource;
                    }

                    if (readWriteType == GLReadWriteType.Atomic && bindPoint >= 0 && bindPoint < state.AtomicBuffers.Length)
                    {
                        bf = state.AtomicBuffers[bindPoint];
                        id = state.AtomicBuffers[bindPoint].Resource;
                    }

                    if (readWriteType == GLReadWriteType.SSBO && bindPoint >= 0 && bindPoint < state.ShaderStorageBuffers.Length)
                    {
                        bf = state.ShaderStorageBuffers[bindPoint];
                        id = state.ShaderStorageBuffers[bindPoint].Resource;
                    }

                    bool filledSlot = id != ResourceId.Null;
                    bool usedSlot = stage.BindpointMapping.ReadWriteResources[i].used;

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string binding = readWriteType == GLReadWriteType.Image ? "Image" :
                            readWriteType == GLReadWriteType.Atomic ? "Atomic" :
                            readWriteType == GLReadWriteType.SSBO ? "SSBO" :
                            "Unknown";

                        string slotname = String.Format("{0}: {1}", bindPoint, res.name);
                        string name = "";
                        string dimensions = "";
                        string format = "-";
                        string access = "Read/Write";
                        if (im != null)
                        {
                            if (im.readAllowed && !im.writeAllowed) access = "Read-Only";
                            if (!im.readAllowed && im.writeAllowed) access = "Write-Only";
                            format = im.Format.ToString();
                        }

                        object tag = null;

                        // check to see if it's a texture
                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == id)
                            {
                                if (texs[t].dimension == 1)
                                {
                                    if(texs[t].arraysize > 1)
                                        dimensions = String.Format("{0}[{1}]", texs[t].width, texs[t].arraysize);
                                    else
                                        dimensions = String.Format("{0}", texs[t].width);
                                }
                                else if (texs[t].dimension == 2)
                                {
                                    if (texs[t].arraysize > 1)
                                        dimensions = String.Format("{0}x{1}[{2}]", texs[t].width, texs[t].height, texs[t].arraysize);
                                    else
                                        dimensions = String.Format("{0}x{1}", texs[t].width, texs[t].height);
                                }
                                else if (texs[t].dimension == 3)
                                {
                                    dimensions = String.Format("{0}x{1}x{2}", texs[t].width, texs[t].height, texs[t].depth);
                                }

                                name = texs[t].name;

                                tag = texs[t];
                            }
                        }

                        // if not a texture, it must be a buffer
                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == id)
                            {
                                ulong offset = 0;
                                ulong length = bufs[t].length;
                                if (bf != null && bf.Size > 0)
                                {
                                    offset = bf.Offset;
                                    length = bf.Size;
                                }

                                if(offset > 0)
                                    dimensions = String.Format("{0} bytes at offset {1} bytes", length, offset);
                                else
                                    dimensions = String.Format("{0} bytes", length);

                                name = bufs[t].name;

                                tag = new ReadWriteTag(i, bufs[t]);
                            }
                        }

                        if (!filledSlot)
                        {
                            name = "Empty";
                            dimensions = "-";
                            access = "-";
                        }

                        var node = readwrites.Nodes.Add(new object[] { binding, slotname, name, dimensions, format, access });

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
            readwrites.EndUpdate();
            readwrites.NodesSelection.Clear();
            readwrites.SetVScrollValue(vs);

            {
                readwrites.Visible = readwrites.Parent.Visible = (readwrites.Nodes.Count > 0);
                int row = table.GetRow(readwrites.Parent);
                if (row >= 0 && row < table.RowStyles.Count)
                {
                    if (readwrites.Nodes.Count > 0)
                        table.RowStyles[row].Height = table.RowStyles[1].Height;
                    else
                        table.RowStyles[row].Height = 0;
                }
            }
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

                    uint compCount = 4;
                    FormatComponentType compType = FormatComponentType.Float;

                    if (state.m_VS.Shader != ResourceId.Null)
                    {
                        int attrib = state.m_VS.BindpointMapping.InputAttributes[i];

                        if (attrib >= 0 && attrib < state.m_VS.ShaderDetails.InputSig.Length)
                        {
                            name = state.m_VS.ShaderDetails.InputSig[attrib].varName;
                            compCount = state.m_VS.ShaderDetails.InputSig[attrib].compCount;
                            compType = state.m_VS.ShaderDetails.InputSig[attrib].compType;
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

                        string genericVal = "Generic";

                        string fmtstr = "";
                             if (compCount == 1) fmtstr = "=<{0}>";
                        else if (compCount == 2) fmtstr = "=<{0}, {1}>";
                        else if (compCount == 3) fmtstr = "=<{0}, {1}, {2}>";
                        else if (compCount == 4) fmtstr = "=<{0}, {1}, {2}, {3}>";

                        if (compType == FormatComponentType.Float)
                            genericVal += String.Format(fmtstr, l.GenericValue.f[0], l.GenericValue.f[1], l.GenericValue.f[2], l.GenericValue.f[3]);
                        else if (compType == FormatComponentType.UInt)
                            genericVal += String.Format(fmtstr, l.GenericValue.u[0], l.GenericValue.u[1], l.GenericValue.u[2], l.GenericValue.u[3]);
                        else if (compType == FormatComponentType.SInt)
                            genericVal += String.Format(fmtstr, l.GenericValue.i[0], l.GenericValue.i[1], l.GenericValue.i[2], l.GenericValue.i[3]);

                        var node = inputLayouts.Nodes.Add(new object[] {
                            i,
                            l.Enabled ? "Enabled" : "Disabled", name,
                            l.Enabled ? l.Format.ToString() : genericVal,
                            l.BufferSlot.ToString(), byteOffs, });

                        node.Tag = l.BufferSlot;

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

            if (state.m_VtxIn.ibuffer != ResourceId.Null)
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
                    node.Tag = new IABufferTag(state.m_VtxIn.ibuffer, draw.indexOffset);

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
                    node.Tag = new IABufferTag(state.m_VtxIn.ibuffer, draw.indexOffset);

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
                        node.Tag = new IABufferTag(v.Buffer, v.Offset);

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

            SetShaderState(texs, bufs, state, state.m_VS, vsTable, vsShader, vsTextures, vsSamplers, vsCBuffers, vsSubroutines, vsReadWrite);
            SetShaderState(texs, bufs, state, state.m_GS, gsTable, gsShader, gsTextures, gsSamplers, gsCBuffers, gsSubroutines, gsReadWrite);
            SetShaderState(texs, bufs, state, state.m_TES, tesTable, tesShader, tesTextures, tesSamplers, tesCBuffers, tesSubroutines, tesReadWrite);
            SetShaderState(texs, bufs, state, state.m_TCS, tcsTable, tcsShader, tcsTextures, tcsSamplers, tcsCBuffers, tcsSubroutines, tcsReadWrite);
            SetShaderState(texs, bufs, state, state.m_FS, fsTable, fsShader, fsTextures, fsSamplers, fsCBuffers, fsSubroutines, fsReadWrite);
            SetShaderState(texs, bufs, state, state.m_CS, csTable, csShader, csTextures, csSamplers, csCBuffers, csSubroutines, csReadWrite);

            vs = gsFeedback.VScrollValue();
            gsFeedback.BeginUpdate();
            gsFeedback.Nodes.Clear();
            if (state.m_Feedback.Active)
            {
                feedbackPaused.Image = state.m_Feedback.Paused ? tick : cross;
                for(int i=0; i < state.m_Feedback.BufferBinding.Length; i++)
                {
                    bool filledSlot = (state.m_Feedback.BufferBinding[i] != ResourceId.Null);
                    bool usedSlot = (filledSlot);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string name = "Buffer " + state.m_Feedback.BufferBinding[i].ToString();
                        ulong length = state.m_Feedback.Size[i];

                        if (!filledSlot)
                        {
                            name = "Empty";
                        }

                        FetchBuffer fetch = null;

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == state.m_Feedback.BufferBinding[i])
                            {
                                name = bufs[t].name;
                                if(length == 0)
                                    length = bufs[t].length;

                                fetch = bufs[t];
                            }
                        }

                        var node = gsFeedback.Nodes.Add(new object[] { i, name, length, state.m_Feedback.Offset[i] });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = fetch;

                        if (!filledSlot)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);
                    }
                }
            }
            gsFeedback.EndUpdate();
            gsFeedback.NodesSelection.Clear();
            gsFeedback.SetVScrollValue(vs);

            gsFeedback.Visible = gsFeedback.Parent.Visible = state.m_Feedback.Active;
            if (state.m_Feedback.Active)
                gsTable.ColumnStyles[1].Width = 50.0f;
            else
                gsTable.ColumnStyles[1].Width = 0;

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
                        if (v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || v1.MinDepth != v2.MaxDepth || showEmpty.Checked)
                        {
                            string indexstring = prev.ToString();
                            if (prev < i - 1)
                                indexstring = String.Format("{0}-{1}", prev, i - 1);
                            var node = viewports.Nodes.Add(new object[] { indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth });

                            if (v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
                                EmptyRow(node);
                        }

                        prev = i;
                    }
                }

                // handle the last batch (the loop above leaves the last batch un-added)
                if(prev < state.m_RS.Viewports.Length)
                {
                    var v1 = state.m_RS.Viewports[prev];

                    // must display at least one viewport - otherwise if they are
                    // all empty we get an empty list - we want a nice obvious
                    // 'invalid viewport' entry. So check if prev is 0

                    if (v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || showEmpty.Checked || prev == 0)
                    {
                        string indexstring = prev.ToString();
                        if (prev < state.m_RS.Viewports.Length-1)
                            indexstring = String.Format("{0}-{1}", prev, state.m_RS.Viewports.Length - 1);
                        var node = viewports.Nodes.Add(new object[] { indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth });

                        if (v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
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
                        s1.Bottom != s2.Bottom ||
                        s1.Enabled != s2.Enabled)
                    {
                        if (s1.Enabled || showEmpty.Checked)
                        {
                            string indexstring = prev.ToString();
                            if (prev < i - 1)
                                indexstring = String.Format("{0}-{1}", prev, i - 1);
                            var node = scissors.Nodes.Add(new object[] { indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height, s1.Enabled });

                            if (s1.Width == 0 || s1.Height == 0)
                                EmptyRow(node);

                            if (!s1.Enabled)
                                InactiveRow(node);
                        }

                        prev = i;
                    }
                }

                // handle the last batch (the loop above leaves the last batch un-added)
                if (prev < state.m_RS.Scissors.Length)
                {
                    var s1 = state.m_RS.Scissors[prev];

                    if (s1.Enabled || showEmpty.Checked)
                    {
                        string indexstring = prev.ToString();
                        if (prev < state.m_RS.Scissors.Length - 1)
                            indexstring = String.Format("{0}-{1}", prev, state.m_RS.Scissors.Length - 1);
                        var node = scissors.Nodes.Add(new object[] { indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height, s1.Enabled });

                        if (s1.Width == 0 || s1.Height == 0)
                            EmptyRow(node);

                        if (!s1.Enabled)
                            InactiveRow(node);
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

            multisampleEnable.Image = state.m_RS.m_State.MultisampleEnable ? cross : tick;
            sampleShading.Image = state.m_RS.m_State.SampleShading ? cross : tick;
            minSampleShadingRate.Text = Formatter.Format(state.m_RS.m_State.MinSampleShadingRate);
            alphaToCoverage.Image = state.m_RS.m_State.SampleAlphaToCoverage ? cross : tick;
            alphaToOne.Image = state.m_RS.m_State.SampleAlphaToOne ? cross : tick;
            if (state.m_RS.m_State.SampleCoverage)
            {
                sampleCoverage.Text = Formatter.Format(state.m_RS.m_State.SampleCoverageValue);
                if (state.m_RS.m_State.SampleCoverageInvert)
                    sampleCoverage.Text += " inverted";
                sampleCoverage.Image = null;
            }
            else
            {
                sampleCoverage.Text = "";
                sampleCoverage.Image = cross;
            }
            if (state.m_RS.m_State.SampleMask)
            {
                sampleMask.Text = state.m_RS.m_State.SampleMaskValue.ToString("X8");
                sampleMask.Image = null;
            }
            else
            {
                sampleMask.Text = "";
                sampleMask.Image = cross;
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

                    if (db >= 0) p = state.m_FB.m_DrawFBO.Color[db].Obj;

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
                                typename = texs[t].resType.Str();

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
                foreach (ResourceId depthstencil in new ResourceId[] { state.m_FB.m_DrawFBO.Depth.Obj, state.m_FB.m_DrawFBO.Stencil.Obj })
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
                                typename = texs[t].resType.Str();

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
                bool raster = true;
                bool fbo = true;

                if (state.m_VtxProcess.discard)
                {
                    raster = fbo = false;
                }

                if (state.m_GS.Shader == ResourceId.Null &&
                    state.m_Feedback.Active)
                {
                    pipeFlow.SetStageName(4, new KeyValuePair<string, string>("XFB", "Transform Feedback"));
                }
                else
                {
                    pipeFlow.SetStageName(4, new KeyValuePair<string, string>("GS", "Geometry Shader"));
                }

                pipeFlow.SetStagesEnabled(new bool[] {
                    true,
                    true,
                    state.m_TES.Shader != ResourceId.Null,
                    state.m_TCS.Shader != ResourceId.Null,
                    state.m_GS.Shader != ResourceId.Null || state.m_Feedback.Active,
                    raster,
                    !state.m_VtxProcess.discard && state.m_FS.Shader != ResourceId.Null,
                    fbo,
                    false
                });
            }
        }

        public void OnEventSelected(UInt32 eventID)
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

                if (tex.resType == ShaderResourceType.Buffer)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(false, 0, ulong.MaxValue, tex.ID);
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
            else if(tag is ReadWriteTag)
            {
                ReadWriteTag rwtag = (ReadWriteTag)tag;
                FetchBuffer buf = rwtag.buf;

                string format = "";

                var deets = stage.ShaderDetails;
                
                ShaderResource r = deets.ReadWriteResources[rwtag.idx];
                var bindpoint = stage.BindpointMapping.ReadWriteResources[rwtag.idx];

                GLReadWriteType readWriteType = GetGLReadWriteType(r);

                ulong offset = 0;
                ulong size = ulong.MaxValue;
                if (readWriteType == GLReadWriteType.SSBO)
                {
                    offset = m_Core.CurGLPipelineState.ShaderStorageBuffers[bindpoint.bind].Offset;
                    size = m_Core.CurGLPipelineState.ShaderStorageBuffers[bindpoint.bind].Size;
                }
                if (readWriteType == GLReadWriteType.Atomic)
                {
                    offset = m_Core.CurGLPipelineState.AtomicBuffers[bindpoint.bind].Offset;
                    size = m_Core.CurGLPipelineState.AtomicBuffers[bindpoint.bind].Size;
                }

                if (deets != null)
                {
                    if (r.variableType.members.Length == 0)
                    {
                        if (format == "" && r.variableType.Name.Length > 0)
                            format = r.variableType.Name + " " + r.name + ";";
                    }
                    else
                    {
                        format = FormatMembers(0, "", r.variableType.members);
                    }
                }

                if (buf.ID != ResourceId.Null)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    if (format.Length == 0)
                        viewer.ViewRawBuffer(true, offset, size, buf.ID);
                    else
                        viewer.ViewRawBuffer(true, offset, size, buf.ID, format);
                    viewer.Show(m_DockContent.DockPanel);
                }
            }
        }

        private void defaultCopyPaste_KeyDown(object sender, KeyEventArgs e)
        {
            if (!m_Core.LogLoaded) return;

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

                try
                {
                    if (text.Length > 0)
                        Clipboard.SetText(text);
                }
                catch (System.Exception)
                {
                    try
                    {
                        if (text.Length > 0)
                            Clipboard.SetDataObject(text);
                    }
                    catch (System.Exception)
                    {
                        // give up!
                    }
                }
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
            if (node.Tag is IABufferTag)
            {
                IABufferTag tag = (IABufferTag)node.Tag;

                if (tag.id != ResourceId.Null)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(true, tag.offset, ulong.MaxValue, tag.id);
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

        private void MakeShaderVariablesGLSL(bool cbufferContents, ShaderConstant[] vars, ref string struct_contents, ref string struct_defs)
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
                        MakeShaderVariablesGLSL(false, v.type.members, ref contents, ref struct_defs);

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
            GLPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null || shaderDetails == null) return;

            var files = new Dictionary<string, string>();
            foreach (var s in shaderDetails.DebugInfo.files)
                files.Add(s.BaseFilename, s.filetext);

            if (files.Count == 0)
                return;

            GLPipelineStateViewer pipeviewer = this;

            ShaderViewer sv = new ShaderViewer(m_Core, false, "main", files,

            // Save Callback
            (ShaderViewer viewer, Dictionary<string, string> updatedfiles) =>
            {
                string compileSource = "";
                foreach (var kv in updatedfiles)
                    compileSource += kv.Value;

                // invoke off to the ReplayRenderer to replace the log's shader
                // with our edited one
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    string errs = "";

                    ResourceId from = stage.Shader;
                    ResourceId to = r.BuildTargetShader("main", compileSource, shaderDetails.DebugInfo.compileFlags, stage.stage, out errs);

                    viewer.BeginInvoke((MethodInvoker)delegate { viewer.ShowErrors(errs); });
                    if (to == ResourceId.Null)
                    {
                        r.RemoveReplacement(from);
                        pipeviewer.BeginInvoke((MethodInvoker)delegate { m_Core.RefreshStatus(); });
                    }
                    else
                    {
                        r.ReplaceResource(from, to);
                        pipeviewer.BeginInvoke((MethodInvoker)delegate { m_Core.RefreshStatus(); });
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
                    pipeviewer.BeginInvoke((MethodInvoker)delegate { m_Core.RefreshStatus(); });
                });
            });

            sv.Show(m_DockContent.DockPanel);
        }

        private void ShowCBuffer(GLPipelineState.ShaderStage stage, UInt32 slot)
        {
            var existing = ConstantBufferPreviewer.Has(stage.stage, slot, 0);
            if (existing != null)
            {
                existing.Show();
                return;
            }

            var prev = new ConstantBufferPreviewer(m_Core, stage.stage, slot, 0);

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
                    ret += indentstr + "// struct " + v.name + Environment.NewLine;
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
                if(hoverNode.Tag != null && hoverNode.Tag is uint) 
                    HighlightVtxAttribSlot((uint)hoverNode.Tag);
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
                uint buf = (uint)n.Tag;
                if (buf == slot)
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