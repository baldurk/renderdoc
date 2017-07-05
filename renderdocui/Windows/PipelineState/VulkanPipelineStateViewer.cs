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
using System.Xml;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdocui.Controls;
using renderdoc;

namespace renderdocui.Windows.PipelineState
{
    public partial class VulkanPipelineStateViewer : UserControl, ILogViewerForm
    {
        private Core m_Core;
        private DockContent m_DockContent;

        // keep track of the VB nodes (we want to be able to highlight them easily on hover)
        private List<TreelistView.Node> m_VBNodes = new List<TreelistView.Node>();
        private List<TreelistView.Node> m_BindNodes = new List<TreelistView.Node>();

        // keep track of resource nodes that need view details
        private List<TreelistView.Node> m_ViewDetailNodes = new List<TreelistView.Node>();

        TreelistView.Node m_CurViewDetailNode = null;

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

        class SamplerData
        {
            public SamplerData(TreelistView.Node n) { node = n; }
            public List<TreelistView.Node> images = new List<TreelistView.Node>();
            public TreelistView.Node node = null;
        };

        class CBufferTag
        {
            public CBufferTag(UInt32 s, UInt32 i) { slotIdx = s; arrayIdx = i; }
            public UInt32 slotIdx;
            public UInt32 arrayIdx;
        };

        class BufferResTag
        {
            public BufferResTag(bool rw, UInt32 b, ResourceId id, ulong offs, ulong sz)
            { rwRes = rw; bindPoint = b; ID = id; offset = offs; size = sz; }
            public bool rwRes;
            public UInt32 bindPoint;
            public ResourceId ID;
            public ulong offset;
            public ulong size;
        };

        private class ViewTexTag
        {
            public ViewTexTag(ResourceFormat f, UInt32 bm, UInt32 bl, UInt32 nm, UInt32 nl, FetchTexture t)
            {
                fmt = f;
                baseMip = bm;
                baseLayer = bl;
                numMip = nm;
                numLayer = nl;
                tex = t;
            }

            public ResourceFormat fmt;
            public UInt32 baseMip;
            public UInt32 baseLayer;
            public UInt32 numMip;
            public UInt32 numLayer;
            public FetchTexture tex;
        };

        private Dictionary<TreelistView.Node, TreelistView.Node> m_CombinedImageSamplers = new Dictionary<TreelistView.Node, TreelistView.Node>();

        public VulkanPipelineStateViewer(Core core, DockContent c)
        {
            InitializeComponent();

            if (SystemInformation.HighContrast)
                toolStrip1.Renderer = new ToolStripSystemRenderer();

            m_DockContent = c;

            viAttrs.Font = core.Config.PreferredFont;
            viBuffers.Font = core.Config.PreferredFont;

            vsShader.Font = vsResources.Font = vsCBuffers.Font = core.Config.PreferredFont;
            gsShader.Font = gsResources.Font = gsCBuffers.Font = core.Config.PreferredFont;
            hsShader.Font = hsResources.Font = hsCBuffers.Font = core.Config.PreferredFont;
            dsShader.Font = dsResources.Font = dsCBuffers.Font = core.Config.PreferredFont;
            psShader.Font = psResources.Font = psCBuffers.Font = core.Config.PreferredFont;
            csShader.Font = csResources.Font = csCBuffers.Font = core.Config.PreferredFont;

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

            pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });

            //Icon = global::renderdocui.Properties.Resources.icon;

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

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
            HideViewDetailsTooltip();
            m_ViewDetailNodes.Clear();

            viAttrs.Nodes.Clear();
            viBuffers.Nodes.Clear();
            topology.Text = "";
            topologyDiagram.Image = null;

            ClearShaderState(vsShader, vsResources, vsCBuffers);
            ClearShaderState(gsShader, gsResources, gsCBuffers);
            ClearShaderState(hsShader, hsResources, hsCBuffers);
            ClearShaderState(dsShader, dsResources, dsCBuffers);
            ClearShaderState(psShader, psResources, psCBuffers);
            ClearShaderState(csShader, csResources, csCBuffers);

            var tick = global::renderdocui.Properties.Resources.tick;

            fillMode.Text = "Solid";
            cullMode.Text = "Front";
            frontCCW.Image = tick;

            depthBias.Text = "0.0";
            depthBiasClamp.Text = "0.0";
            slopeScaledBias.Text = "0.0";

            depthClamp.Image = tick;
            rasterizerDiscard.Image = tick;
            lineWidth.Text = "1.0";

            sampleCount.Text = "1";
            sampleShading.Image = tick;
            minSampleShading.Text = "0.0";
            sampleMask.Text = "FFFFFFFF";
            
            viewports.Nodes.Clear();
            scissors.Nodes.Clear();

            targetOutputs.Nodes.Clear();
            blendOperations.Nodes.Clear();

            blendFactor.Text = "0.00, 0.00, 0.00, 0.00";
            logicOp.Text = "-";
            alphaToOne.Image = tick;

            depthEnable.Image = tick;
            depthFunc.Text = "GREATER_EQUAL";
            depthWrite.Image = tick;

            depthBounds.Text = "0.0-1.0";
            depthBounds.Image = null;

            stencilFuncs.Nodes.Clear();

            pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });
        }
        
        public void OnLogfileLoaded()
        {
            OnEventSelected(m_Core.CurEvent);
        }

        private void EmptyRow(TreelistView.Node node)
        {
            node.BackColor = Color.FromArgb(255, 70, 70);
        }

        private void InactiveRow(TreelistView.Node node)
        {
            node.Italic = true;
        }

        private void ViewDetailsRow(TreelistView.Node node, bool highlight)
        {
            if (highlight)
            {
                node.BackColor = Color.Aquamarine;
                node.ForeColor = Color.Black;
            }
            m_ViewDetailNodes.Add(node);
        }

        private bool HasImportantViewParams(VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement view, FetchTexture tex)
        {
            // Since mutable formats are more unclear in vulkan (it allows casting between any
            // similar format, and the underlying texture still has a valid format), we consider
            // a format difference to be important even though we display the view's format in
            // the row data itself
            if (view.viewfmt != tex.format ||
                view.baseMip > 0 || view.baseLayer > 0 ||
                (view.numMip < tex.mips && tex.mips > 1) ||
                (view.numLayer < tex.arraysize && tex.arraysize > 1))
                return true;

            return false;
        }

        private bool HasImportantViewParams(VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement view, FetchBuffer buf)
        {
            if (view.offset > 0 || view.size < buf.length)
                return true;

            return false;
        }

        private bool HasImportantViewParams(VulkanPipelineState.CurrentPass.Framebuffer.Attachment att, FetchTexture tex)
        {
            // see above in BindingElement overload for justification for comparing formats
            if (att.viewfmt != tex.format ||
                att.baseMip > 0 || att.baseLayer > 0 ||
                (att.numMip < tex.mips && tex.mips > 1) ||
                (att.numLayer < tex.arraysize && tex.arraysize > 1))
                return true;

            return false;
        }

        private void ClearShaderState(Label shader, TreelistView.TreeListView resources,
                                      TreelistView.TreeListView cbuffers)
        {
            shader.Text = "Unbound";
            resources.Nodes.Clear();
            cbuffers.Nodes.Clear();
        }

        private object[] MakeSampler(string bindset, string slotname, VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement descriptor)
        {
            string addressing = "";
            string addPrefix = "";
            string addVal = "";

            string filter = "";

            string[] addr = { descriptor.addrU.ToString(), descriptor.addrV.ToString(), descriptor.addrW.ToString() };

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

            if (descriptor.UseBorder())
                addressing += " <" + descriptor.BorderColor[0].ToString() + ", " +
                                            descriptor.BorderColor[1].ToString() + ", " +
                                            descriptor.BorderColor[2].ToString() + ", " +
                                            descriptor.BorderColor[3].ToString() + ">";

            if (descriptor.unnormalized)
                addressing += " (Un-norm)";

            filter += descriptor.Filter.ToString();

            if (descriptor.maxAniso > 1.0f)
                filter += String.Format(" Aniso {0}x", descriptor.maxAniso);

            if (descriptor.Filter.func == FilterFunc.Comparison)
                filter += String.Format(" ({0})", descriptor.comparison);
            else if (descriptor.Filter.func != FilterFunc.Normal)
                filter += String.Format(" ({0})", descriptor.Filter.func);

            string lod = "LODs: " +
                         (descriptor.minlod == -float.MaxValue ? "0" : descriptor.minlod.ToString()) + " - " +
                         (descriptor.maxlod == float.MaxValue ? "FLT_MAX" : descriptor.maxlod.ToString());

            if (descriptor.mipBias != 0.0f)
                lod += String.Format(" Bias {0}", descriptor.mipBias);

            return new object[] {
                                        "", bindset, slotname,
                                        descriptor.immutableSampler ? "Immutable Sampler" : "Sampler",
                                        descriptor.SamplerName, 
                                        addressing,
                                        filter + ", " + lod
                                    };
        }

        private void AddResourceRow(ShaderReflection shaderDetails, VulkanPipelineState.ShaderStage stage, int bindset, int bind, VulkanPipelineState.Pipeline pipe, TreelistView.TreeListView resources, FetchTexture[] texs, FetchBuffer[] bufs, ref Dictionary<ResourceId, SamplerData> samplers)
        {
            ShaderResource shaderRes = null;
            BindpointMap bindMap = null;

            bool isrw = false;
            uint bindPoint = 0;

            if (shaderDetails != null)
            {
                for (int i = 0; i < shaderDetails.ReadOnlyResources.Length; i++)
                {
                    var ro = shaderDetails.ReadOnlyResources[i];

                    if (stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset == bindset &&
                        stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind == bind)
                    {
                        bindPoint = (uint)i;
                        shaderRes = ro;
                        bindMap = stage.BindpointMapping.ReadOnlyResources[ro.bindPoint];
                    }
                }

                for (int i = 0; i < shaderDetails.ReadWriteResources.Length; i++)
                {
                    var rw = shaderDetails.ReadWriteResources[i];

                    if (stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset == bindset &&
                        stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind == bind)
                    {
                        bindPoint = (uint)i;
                        isrw = true;
                        shaderRes = rw;
                        bindMap = stage.BindpointMapping.ReadWriteResources[rw.bindPoint];
                    }
                }
            }

            VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement[] slotBinds = null;
            ShaderBindType bindType = ShaderBindType.Unknown;
            ShaderStageBits stageBits = (ShaderStageBits)0;

            if (bindset < pipe.DescSets.Length && bind < pipe.DescSets[bindset].bindings.Length)
            {
                slotBinds = pipe.DescSets[bindset].bindings[bind].binds;
                bindType = pipe.DescSets[bindset].bindings[bind].type;
                stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
            }
            else
            {
                if (shaderRes.IsSampler)
                    bindType = ShaderBindType.Sampler;
                else if (shaderRes.IsSampler && shaderRes.IsTexture)
                    bindType = ShaderBindType.ImageSampler;
                else if (shaderRes.resType == ShaderResourceType.Buffer)
                    bindType = ShaderBindType.ReadOnlyTBuffer;
                else
                    bindType = ShaderBindType.ReadOnlyImage;
            }

            bool usedSlot = bindMap != null && bindMap.used;
            bool stageBitsIncluded = stageBits.HasFlag((ShaderStageBits)(1 << (int)stage.stage));

            // skip descriptors that aren't for this shader stage
            if (!usedSlot && !stageBitsIncluded)
                return;

            if (bindType == ShaderBindType.ConstantBuffer)
                return;

            // TODO - check compatibility between bindType and shaderRes.resType ?

            // consider it filled if any array element is filled
            bool filledSlot = false;
            for (int idx = 0; slotBinds != null && idx < slotBinds.Length; idx++)
            {
                filledSlot |= slotBinds[idx].res != ResourceId.Null;
                if (bindType == ShaderBindType.Sampler || bindType == ShaderBindType.ImageSampler)
                    filledSlot |= slotBinds[idx].sampler != ResourceId.Null;
            }

            // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
            if (!stageBitsIncluded)
                filledSlot = false;

            // show if
            if (usedSlot || // it's referenced by the shader - regardless of empty or not
                (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                )
            {
                TreelistView.NodeCollection parentNodes = resources.Nodes;

                string setname = bindset.ToString();

                string slotname = bind.ToString();
                if (shaderRes != null && shaderRes.name.Length > 0)
                    slotname += ": " + shaderRes.name;

                int arrayLength = 0;
                if (slotBinds != null) arrayLength = slotBinds.Length;
                else arrayLength = (int)bindMap.arraySize;

                // for arrays, add a parent element that we add the real cbuffers below
                if (arrayLength > 1)
                {
                    var node = parentNodes.Add(new object[] { "", setname, slotname, String.Format("Array[{0}]", arrayLength), "", "", "", "" });

                    node.TreeColumn = 0;

                    if (!filledSlot)
                        EmptyRow(node);

                    if (!usedSlot)
                        InactiveRow(node);

                    parentNodes = node.Nodes;
                }

                for (int idx = 0; idx < arrayLength; idx++)
                {
                    VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement descriptorBind = null;
                    if (slotBinds != null) descriptorBind = slotBinds[idx];

                    if (arrayLength > 1)
                    {
                        slotname = String.Format("{0}[{1}]", bind, idx);

                        if (shaderRes != null && shaderRes.name.Length > 0)
                            slotname += ": " + shaderRes.name;
                    }

                    bool isbuf = false;
                    UInt32 w = 1, h = 1, d = 1;
                    UInt32 a = 1;
                    UInt32 samples = 1;
                    UInt64 len = 0;
                    string format = "Unknown";
                    string name = "Empty";
                    ShaderResourceType restype = ShaderResourceType.None;
                    object tag = null;
                    bool viewDetails = false;

                    ulong descriptorLen = 0;

                    if(descriptorBind != null)
                        descriptorLen = descriptorBind.size;

                    if (filledSlot && descriptorBind != null)
                    {
                        name = "Object " + descriptorBind.res.ToString();

                        format = descriptorBind.viewfmt.ToString();

                        // check to see if it's a texture
                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == descriptorBind.res)
                            {
                                w = texs[t].width;
                                h = texs[t].height;
                                d = texs[t].depth;
                                a = texs[t].arraysize;
                                name = texs[t].name;
                                restype = texs[t].resType;
                                samples = texs[t].msSamp;

                                if (HasImportantViewParams(descriptorBind, texs[t]))
                                    viewDetails = true;

                                tag = new ViewTexTag(
                                        descriptorBind.viewfmt,
                                        descriptorBind.baseMip, descriptorBind.baseLayer,
                                        descriptorBind.numMip, descriptorBind.numLayer,
                                        texs[t]
                                    );
                            }
                        }

                        // if not a texture, it must be a buffer
                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == descriptorBind.res)
                            {
                                len = bufs[t].length;
                                w = 0;
                                h = 0;
                                d = 0;
                                a = 0;
                                name = bufs[t].name;
                                restype = ShaderResourceType.Buffer;

                                if(descriptorLen == ulong.MaxValue)
                                    descriptorLen = len - descriptorBind.offset;

                                tag = new BufferResTag(isrw, bindPoint, bufs[t].ID, descriptorBind.offset, descriptorLen);

                                if (HasImportantViewParams(descriptorBind, bufs[t]))
                                    viewDetails = true;

                                isbuf = true;
                            }
                        }
                    }
                    else
                    {
                        name = "Empty";
                        format = "-";
                        w = h = d = a = 0;
                    }

                    TreelistView.Node node = null;

                    if (bindType == ShaderBindType.ReadWriteBuffer ||
                        bindType == ShaderBindType.ReadOnlyTBuffer ||
                        bindType == ShaderBindType.ReadWriteTBuffer
                        )
                    {
                        if (!isbuf)
                        {
                            node = parentNodes.Add(new object[] {
                                "", bindset, slotname, bindType,
                                "-", 
                                "-",
                                "",
                            });

                            EmptyRow(node);
                        }
                        else
                        {
                            string range = "-";
                            if (descriptorBind != null)
                                range = String.Format("{0} - {1}", descriptorBind.offset, descriptorLen);

                            node = parentNodes.Add(new object[] {
                                "", bindset, slotname, bindType,
                                name, 
                                String.Format("{0} bytes", len),
                                range,
                            });

                            node.Image = global::renderdocui.Properties.Resources.action;
                            node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                            node.Tag = tag;

                            if (!filledSlot)
                                EmptyRow(node);

                            if (!usedSlot)
                                InactiveRow(node);
                        }
                    }
                    else if (bindType == ShaderBindType.Sampler)
                    {
                        if (descriptorBind == null || descriptorBind.sampler == ResourceId.Null)
                        {
                            node = parentNodes.Add(new object[] {
                                "", bindset, slotname, bindType,
                                "-", 
                                "-",
                                "",
                            });

                            EmptyRow(node);
                        }
                        else
                        {
                            node = parentNodes.Add(MakeSampler(bindset.ToString(), slotname, descriptorBind));

                            if (!filledSlot)
                                EmptyRow(node);

                            if (!usedSlot)
                                InactiveRow(node);

                            var data = new SamplerData(node);
                            node.Tag = data;

                            if (!samplers.ContainsKey(descriptorBind.sampler))
                                samplers.Add(descriptorBind.sampler, data);
                        }
                    }
                    else
                    {
                        if (descriptorBind == null || descriptorBind.res == ResourceId.Null)
                        {
                            node = parentNodes.Add(new object[] {
                                "", bindset, slotname, bindType,
                                "-", 
                                "-",
                                "",
                            });

                            EmptyRow(node);
                        }
                        else
                        {
                            string typename = restype.Str() + " " + bindType.Str().Replace("&", "&&");

                            string dim;

                            if (restype == ShaderResourceType.Texture3D)
                                dim = String.Format("{0}x{1}x{2}", w, h, d);
                            else if (restype == ShaderResourceType.Texture1D || restype == ShaderResourceType.Texture1DArray)
                                dim = w.ToString();
                            else
                                dim = String.Format("{0}x{1}", w, h);

                            if (descriptorBind.swizzle[0] != TextureSwizzle.Red ||
                                descriptorBind.swizzle[1] != TextureSwizzle.Green ||
                                descriptorBind.swizzle[2] != TextureSwizzle.Blue ||
                                descriptorBind.swizzle[3] != TextureSwizzle.Alpha)
                            {
                                format += String.Format(" swizzle[{0}{1}{2}{3}]",
                                    descriptorBind.swizzle[0].Str(),
                                    descriptorBind.swizzle[1].Str(),
                                    descriptorBind.swizzle[2].Str(),
                                    descriptorBind.swizzle[3].Str());
                            }

                            if (restype == ShaderResourceType.Texture1DArray ||
                               restype == ShaderResourceType.Texture2DArray ||
                               restype == ShaderResourceType.Texture2DMSArray ||
                               restype == ShaderResourceType.TextureCubeArray)
                            {
                                dim += String.Format(" {0}[{1}]", restype.Str(), a);
                            }

                            if (restype == ShaderResourceType.Texture2DMS || restype == ShaderResourceType.Texture2DMSArray)
                                dim += String.Format(", {0}x MSAA", samples);

                            node = parentNodes.Add(new object[] {
                                "", bindset, slotname, typename,
                                name, 
                                dim,
                                format,
                            });

                            node.Image = global::renderdocui.Properties.Resources.action;
                            node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                            node.Tag = tag;

                            if (!filledSlot)
                                EmptyRow(node);

                            if (!usedSlot)
                                InactiveRow(node);

                            ViewDetailsRow(node, viewDetails);
                        }

                        if (bindType == ShaderBindType.ImageSampler)
                        {
                            if (descriptorBind == null || descriptorBind.sampler == ResourceId.Null)
                            {
                                node = parentNodes.Add(new object[] {
                                    "", bindset, slotname, bindType,
                                    "-", 
                                    "-",
                                    "",
                                });

                                EmptyRow(node);
                            }
                            else
                            {
                                var texnode = node;

                                if (!samplers.ContainsKey(descriptorBind.sampler))
                                {
                                    node = parentNodes.Add(MakeSampler("", "", descriptorBind));

                                    if (!filledSlot)
                                        EmptyRow(node);

                                    if (!usedSlot)
                                        InactiveRow(node);

                                    var data = new SamplerData(node);
                                    node.Tag = data;

                                    samplers.Add(descriptorBind.sampler, data);
                                }

                                if (texnode != null)
                                {
                                    m_CombinedImageSamplers[texnode] = samplers[descriptorBind.sampler].node;
                                    samplers[descriptorBind.sampler].images.Add(texnode);
                                }
                            }
                        }
                    }
                }
            }
        }

        private void AddConstantBlockRow(ShaderReflection shaderDetails, VulkanPipelineState.ShaderStage stage,
                                         int bindset, int bind,
                                         VulkanPipelineState.Pipeline pipe, TreelistView.TreeListView cbuffers,
                                         FetchBuffer[] bufs)
        {
            ConstantBlock cblock = null;
            BindpointMap bindMap = null;

            uint slot = uint.MaxValue;
            if (shaderDetails != null)
            {
                for (slot = 0; slot < (uint)shaderDetails.ConstantBlocks.Length; slot++)
                {
                    ConstantBlock cb = shaderDetails.ConstantBlocks[slot];
                    if (stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset == bindset &&
                        stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind == bind)
                    {
                        cblock = cb;
                        bindMap = stage.BindpointMapping.ConstantBlocks[cb.bindPoint];
                        break;
                    }
                }

                if (slot >= (uint)shaderDetails.ConstantBlocks.Length)
                    slot = uint.MaxValue;
            }

            VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement[] slotBinds = null;
            ShaderBindType bindType = ShaderBindType.ConstantBuffer;
            ShaderStageBits stageBits = (ShaderStageBits)0;

            if (bindset < pipe.DescSets.Length && bind < pipe.DescSets[bindset].bindings.Length)
            {
                slotBinds = pipe.DescSets[bindset].bindings[bind].binds;
                bindType = pipe.DescSets[bindset].bindings[bind].type;
                stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
            }

            bool usedSlot = bindMap != null && bindMap.used;
            bool stageBitsIncluded = stageBits.HasFlag((ShaderStageBits)(1 << (int)stage.stage));

            // skip descriptors that aren't for this shader stage
            if (!usedSlot && !stageBitsIncluded)
                return;

            if (bindType != ShaderBindType.ConstantBuffer)
                return;

            // consider it filled if any array element is filled (or it's push constants)
            bool filledSlot = cblock != null && !cblock.bufferBacked;
            for (int idx = 0; slotBinds != null && idx < slotBinds.Length; idx++)
                filledSlot |= slotBinds[idx].res != ResourceId.Null;

            // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
            if (!stageBitsIncluded)
                filledSlot = false;

            // show if
            if (usedSlot || // it's referenced by the shader - regardless of empty or not
                (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                )
            {
                TreelistView.NodeCollection parentNodes = cbuffers.Nodes;

                string setname = bindset.ToString();

                string slotname = bind.ToString();
                if (cblock != null && cblock.name.Length > 0)
                    slotname += ": " + cblock.name;

                int arrayLength = 0;
                if (slotBinds != null) arrayLength = slotBinds.Length;
                else arrayLength = (int)bindMap.arraySize;

                // for arrays, add a parent element that we add the real cbuffers below
                if (arrayLength > 1)
                {
                    var node = parentNodes.Add(new object[] { "", setname, slotname, String.Format("Array[{0}]", arrayLength), "", "" });

                    node.TreeColumn = 0;

                    if (!filledSlot)
                        EmptyRow(node);

                    if (!usedSlot)
                        InactiveRow(node);

                    parentNodes = node.Nodes;
                }

                for (int idx = 0; idx < arrayLength; idx++)
                {
                    VulkanPipelineState.Pipeline.DescriptorSet.DescriptorBinding.BindingElement descriptorBind = null;
                    if(slotBinds != null) descriptorBind = slotBinds[idx];

                    if (arrayLength > 1)
                    {
                        slotname = String.Format("{0}[{1}]", bind, idx);

                        if (cblock != null && cblock.name.Length > 0)
                            slotname += ": " + cblock.name;
                    }

                    string name = "Empty";
                    UInt64 length = 0;
                    int numvars = cblock != null ? cblock.variables.Length : 0;
                    UInt64 byteSize = cblock != null ? cblock.byteSize : 0;
                    
                    string vecrange = "-";

                    if (filledSlot && descriptorBind != null)
                    {
                        name = "";
                        length = descriptorBind.size;

                        for (int t = 0; t < bufs.Length; t++)
                        {
                            if (bufs[t].ID == descriptorBind.res)
                            {
                                name = bufs[t].name;
                                if(length == ulong.MaxValue)
                                    length = bufs[t].length - descriptorBind.offset;
                            }
                        }

                        if (name == "")
                            name = "UBO " + descriptorBind.res.ToString();

                        vecrange = String.Format("{0} - {1}", descriptorBind.offset, descriptorBind.offset + length);
                    }

                    string sizestr;
                    
                    // push constants or specialization constants
                    if (cblock != null && !cblock.bufferBacked)
                    {
                        setname = "";
                        slotname = cblock.name;
                        name = "Push constants";
                        vecrange = "";
                        sizestr = String.Format("{0} Variables", numvars);

                        // could maybe get range from ShaderVariable.reg if it's filled out
                        // from SPIR-V side.
                    }
                    else
                    {
                        if (length == byteSize)
                            sizestr = String.Format("{0} Variables, {1} bytes", numvars, length);
                        else
                            sizestr = String.Format("{0} Variables, {1} bytes needed, {2} provided", numvars, byteSize, length);

                        if (length < byteSize)
                            filledSlot = false;
                    }

                    var node = parentNodes.Add(new object[] { "", setname, slotname, name, vecrange, sizestr });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = new CBufferTag(slot, (uint)idx);

                    if (!filledSlot)
                        EmptyRow(node);

                    if (!usedSlot)
                        InactiveRow(node);
                }
            }
        }

        // Set a shader stage's resources and values
        private void SetShaderState(FetchTexture[] texs, FetchBuffer[] bufs,
                                    VulkanPipelineState.ShaderStage stage, VulkanPipelineState.Pipeline pipe,
                                    Label shader, TreelistView.TreeListView resources,
                                    TreelistView.TreeListView cbuffers)
        {
            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null)
                shader.Text = "Unbound";
            else
                shader.Text = stage.ShaderName;

            if (shaderDetails != null)
            {
                if (shaderDetails.DebugInfo.files.Length > 0 || shaderDetails.EntryPoint != "main")
                    shader.Text = shaderDetails.EntryPoint + "()";

                if (shaderDetails.DebugInfo.files.Length > 0)
                    shader.Text += " - " + shaderDetails.DebugInfo.files[0].BaseFilename;
            }

            int vs = 0;
            
            vs = resources.VScrollValue();
            resources.BeginUpdate();
            resources.Nodes.Clear();

            var samplers = new Dictionary<ResourceId, SamplerData>();

            for(int bindset = 0; bindset < pipe.DescSets.Length; bindset++)
            {
                for(int bind = 0; bind < pipe.DescSets[bindset].bindings.Length; bind++)
                {
                    AddResourceRow(shaderDetails, stage, bindset, bind, pipe, resources, texs, bufs, ref samplers);
                }

                // if we have a shader bound, go through and add rows for any resources it wants for binds that aren't
                // in this descriptor set (e.g. if layout mismatches)
                if (shaderDetails != null)
                {
                    for (int i = 0; i < shaderDetails.ReadOnlyResources.Length; i++)
                    {
                        var ro = shaderDetails.ReadOnlyResources[i];

                        if (stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset == bindset &&
                            stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind >= pipe.DescSets[bindset].bindings.Length)
                        {
                            AddResourceRow(shaderDetails, stage, bindset,
                                stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind,
                                pipe, resources, texs, bufs, ref samplers);
                        }
                    }

                    for (int i = 0; i < shaderDetails.ReadWriteResources.Length; i++)
                    {
                        var rw = shaderDetails.ReadWriteResources[i];

                        if (stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset == bindset &&
                            stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind >= pipe.DescSets[bindset].bindings.Length)
                        {
                            AddResourceRow(shaderDetails, stage, bindset,
                                stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind,
                                pipe, resources, texs, bufs, ref samplers);
                        }
                    }
                }
            }

            // if we have a shader bound, go through and add rows for any resources it wants for descriptor sets that aren't
            // bound at all
            if (shaderDetails != null)
            {
                for (int i = 0; i < shaderDetails.ReadOnlyResources.Length; i++)
                {
                    var ro = shaderDetails.ReadOnlyResources[i];

                    if (stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset >= pipe.DescSets.Length)
                    {
                        AddResourceRow(shaderDetails, stage,
                            stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset,
                            stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind,
                            pipe, resources, texs, bufs, ref samplers);
                    }
                }

                for (int i = 0; i < shaderDetails.ReadWriteResources.Length; i++)
                {
                    var rw = shaderDetails.ReadWriteResources[i];

                    if (stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset >= pipe.DescSets.Length)
                    {
                        AddResourceRow(shaderDetails, stage,
                            stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset,
                            stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind,
                            pipe, resources, texs, bufs, ref samplers);
                    }
                }
            }

            resources.EndUpdate();
            resources.NodesSelection.Clear();
            resources.SetVScrollValue(vs);

            vs = cbuffers.VScrollValue();
            cbuffers.BeginUpdate();
            cbuffers.Nodes.Clear();
            for(int bindset = 0; bindset < pipe.DescSets.Length; bindset++)
            {
                for(int bind = 0; bind < pipe.DescSets[bindset].bindings.Length; bind++)
                {
                    AddConstantBlockRow(shaderDetails, stage, bindset, bind, pipe, cbuffers, bufs);
                }

                // if we have a shader bound, go through and add rows for any cblocks it wants for binds that aren't
                // in this descriptor set (e.g. if layout mismatches)
                if (shaderDetails != null)
                {
                    for (int i = 0; i < shaderDetails.ConstantBlocks.Length; i++)
                    {
                        var cb = shaderDetails.ConstantBlocks[i];

                        if (stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset == bindset &&
                            stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind >= pipe.DescSets[bindset].bindings.Length)
                        {
                            AddConstantBlockRow(shaderDetails, stage, bindset,
                                stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind,
                                pipe, cbuffers, bufs);
                        }
                    }
                }
            }

            // if we have a shader bound, go through and add rows for any resources it wants for descriptor sets that aren't
            // bound at all
            if (shaderDetails != null)
            {
                for (int i = 0; i < shaderDetails.ConstantBlocks.Length; i++)
                {
                    var cb = shaderDetails.ConstantBlocks[i];

                    if (stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset >= pipe.DescSets.Length && cb.bufferBacked)
                    {
                        AddConstantBlockRow(shaderDetails, stage,
                            stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset,
                            stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind,
                            pipe, cbuffers, bufs);
                    }
                }
            }

            // search for push constants and add them last
            if (shaderDetails != null)
            {
                for (int cb = 0; cb < shaderDetails.ConstantBlocks.Length; cb++)
                {
                    var cblock = shaderDetails.ConstantBlocks[cb];
                    if (cblock.bufferBacked == false)
                    {
                        // could maybe get range from ShaderVariable.reg if it's filled out
                        // from SPIR-V side.

                        var node = cbuffers.Nodes.Add(new object[] { "", "",
                        cblock.name, "Push constants",
                        "", String.Format("{0} Variables", cblock.variables.Length) });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = new CBufferTag((uint)cb, 0);
                    }
                }
            }
            cbuffers.EndUpdate();
            cbuffers.NodesSelection.Clear();
            cbuffers.SetVScrollValue(vs);
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

            m_CombinedImageSamplers.Clear();

            FetchTexture[] texs = m_Core.CurTextures;
            FetchBuffer[] bufs = m_Core.CurBuffers;
            VulkanPipelineState state = m_Core.CurVulkanPipelineState;
            FetchDrawcall draw = m_Core.CurDrawcall;

            var tick = global::renderdocui.Properties.Resources.tick;
            var cross = global::renderdocui.Properties.Resources.cross;

            bool[] usedBindings = new bool[128];

            for (int i = 0; i < 128; i++)
                usedBindings[i] = false;

            ////////////////////////////////////////////////
            // Vertex Input

            int vs = 0;
            
            vs = viAttrs.VScrollValue();
            viAttrs.Nodes.Clear();
            viAttrs.BeginUpdate();
            if (state.VI.attrs != null)
            {
                int i = 0;
                foreach (var a in state.VI.attrs)
                {
                    bool filledSlot = true;
                    bool usedSlot = false;

                    string name = String.Format("Attribute {0}", i);

                    if (state.m_VS.Shader != ResourceId.Null)
                    {
                        int attrib = -1;
                        if(a.location < state.m_VS.BindpointMapping.InputAttributes.Length)
                            attrib = state.m_VS.BindpointMapping.InputAttributes[a.location];

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
                        var node = viAttrs.Nodes.Add(new object[] {
                                              i, name, a.location, a.binding, a.format, a.byteoffset });

                        usedBindings[a.binding] = true;

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;

                        if (!usedSlot)
                            InactiveRow(node);
                    }

                    i++;
                }
            }
            viAttrs.NodesSelection.Clear();
            viAttrs.EndUpdate();
            viAttrs.SetVScrollValue(vs);

            m_BindNodes.Clear();

            PrimitiveTopology topo = draw != null ? draw.topology : PrimitiveTopology.Unknown;

            topology.Text = topo.ToString();
            if (topo > PrimitiveTopology.PatchList)
            {
                int numCPs = (int)topo - (int)PrimitiveTopology.PatchList + 1;

                topology.Text = string.Format("PatchList ({0} Control Points)", numCPs);
            }

            primRestart.Visible = state.IA.primitiveRestartEnable;

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

            vs = viBuffers.VScrollValue();
            viBuffers.Nodes.Clear();
            viBuffers.BeginUpdate();

            bool ibufferUsed = draw != null && (draw.flags & DrawcallFlags.UseIBuffer) != 0;

            if (state.IA.ibuffer != null && state.IA.ibuffer.buf != ResourceId.Null)
            {
                if (ibufferUsed || showDisabled.Checked)
                {
                    string ptr = "Buffer " + state.IA.ibuffer.buf.ToString();
                    string name = ptr;
                    UInt64 length = 1;

                    if (!ibufferUsed)
                    {
                        length = 0;
                    }

                    for (int t = 0; t < bufs.Length; t++)
                    {
                        if (bufs[t].ID == state.IA.ibuffer.buf)
                        {
                            name = bufs[t].name;
                            length = bufs[t].length;
                        }
                    }

                    var node = viBuffers.Nodes.Add(new object[] { "Index", name, "Index", state.IA.ibuffer.offs, draw.indexByteWidth, length });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = new IABufferTag(state.IA.ibuffer.buf, draw != null ? draw.indexOffset : 0);

                    if (!ibufferUsed)
                        InactiveRow(node);

                    if (state.IA.ibuffer.buf == ResourceId.Null)
                        EmptyRow(node);
                }
            }
            else
            {
                if (ibufferUsed || showEmpty.Checked)
                {
                    var node = viBuffers.Nodes.Add(new object[] { "Index", "No Buffer Set", "-", "-", "-", "-" });

                    node.Image = global::renderdocui.Properties.Resources.action;
                    node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                    node.Tag = new IABufferTag(state.IA.ibuffer.buf, draw != null ? draw.indexOffset : 0);

                    EmptyRow(node);

                    if (!ibufferUsed)
                        InactiveRow(node);
                }
            }

            m_VBNodes.Clear();

            if (state.VI.vbuffers != null)
            {
                int i = 0;
                for(; i < Math.Max(state.VI.vbuffers.Length, state.VI.binds.Length); i++)
                {
                    var vbuff = (i < state.VI.vbuffers.Length ? state.VI.vbuffers[i] : null);
                    VulkanPipelineState.VertexInput.Binding bind = null;

                    for (int b = 0; b < state.VI.binds.Length; b++)
                    {
                        if (state.VI.binds[b].vbufferBinding == i)
                            bind = state.VI.binds[b];
                    }

                    bool filledSlot = ((vbuff != null && vbuff.buffer != ResourceId.Null) || bind != null);
                    bool usedSlot = (usedBindings[i]);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        string name = "No Buffer";
                        string rate = "-";
                        UInt64 length = 1;
                        UInt64 offset = 0;
                        UInt32 stride = 0;

                        if (vbuff != null)
                        {
                            name = "Buffer " + vbuff.buffer.ToString();
                            offset = vbuff.offset;

                            for (int t = 0; t < bufs.Length; t++)
                            {
                                if (bufs[t].ID == vbuff.buffer)
                                {
                                    name = bufs[t].name;
                                    length = bufs[t].length;
                                }
                            }
                        }

                        if (bind != null)
                        {
                            stride = bind.bytestride;
                            rate = bind.perInstance ? "Instance" : "Vertex";
                        }
                        else
                        {
                            name += ", No Binding";
                        }

                        TreelistView.Node node = null;
                        
                        if(filledSlot)
                            node = viBuffers.Nodes.Add(new object[] { i, name, rate, offset, stride, length });
                        else
                            node = viBuffers.Nodes.Add(new object[] { i, "No Binding", "-", "-", "-", "-" });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = new IABufferTag(vbuff != null ? vbuff.buffer : ResourceId.Null, vbuff != null ? vbuff.offset : 0);

                        if (!filledSlot || bind == null || vbuff == null)
                            EmptyRow(node);

                        if (!usedSlot)
                            InactiveRow(node);

                        m_VBNodes.Add(node);
                    }
                }

                for (; i < usedBindings.Length; i++)
                {
                    if (usedBindings[i])
                    {
                        TreelistView.Node node = viBuffers.Nodes.Add(new object[] { i, "No Binding", "-", "-", "-", "-" });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = new IABufferTag(ResourceId.Null, 0);

                        EmptyRow(node);

                        InactiveRow(node);

                        m_VBNodes.Add(node);
                    }
                }
            }
            viBuffers.NodesSelection.Clear();
            viBuffers.EndUpdate();
            viBuffers.SetVScrollValue(vs);

            SetShaderState(texs, bufs, state.m_VS, state.graphics, vsShader, vsResources, vsCBuffers);
            SetShaderState(texs, bufs, state.m_GS, state.graphics, gsShader, gsResources, gsCBuffers);
            SetShaderState(texs, bufs, state.m_TCS, state.graphics, hsShader, hsResources, hsCBuffers);
            SetShaderState(texs, bufs, state.m_TES, state.graphics, dsShader, dsResources, dsCBuffers);
            SetShaderState(texs, bufs, state.m_FS, state.graphics, psShader, psResources, psCBuffers);
            SetShaderState(texs, bufs, state.m_CS, state.compute, csShader, csResources, csCBuffers);

            ////////////////////////////////////////////////
            // Rasterizer

            vs = viewports.VScrollValue();
            viewports.BeginUpdate();
            viewports.Nodes.Clear();

            int vs2 = scissors.VScrollValue();
            scissors.BeginUpdate();
            scissors.Nodes.Clear();

            if (state.Pass.renderpass.obj != ResourceId.Null)
            {
                scissors.Nodes.Add(new object[] { "Render Area",
                    state.Pass.renderArea.x, state.Pass.renderArea.y, 
                    state.Pass.renderArea.width, state.Pass.renderArea.height });
            }

            {
                int i = 0;
                foreach (var v in state.VP.viewportScissors)
                {
                    string misc = "";

                    if (v.vp.Height < 0.0f)
                        misc = "Inverted (negative height)";

                    var node = viewports.Nodes.Add(new object[] { i, v.vp.x, v.vp.y, v.vp.Width, Math.Abs(v.vp.Height), v.vp.MinDepth, v.vp.MaxDepth, misc });

                    if (v.vp.Width == 0 || v.vp.Height == 0 || v.vp.MinDepth == v.vp.MaxDepth)
                        EmptyRow(node);

                    node = scissors.Nodes.Add(new object[] { i, v.scissor.x, v.scissor.y, v.scissor.width, v.scissor.height });

                    if (v.scissor.width == 0 || v.scissor.height == 0)
                        EmptyRow(node);

                    i++;
                }
            }

            viewports.NodesSelection.Clear();
            viewports.EndUpdate();
            viewports.SetVScrollValue(vs);

            scissors.NodesSelection.Clear();
            scissors.EndUpdate();
            scissors.SetVScrollValue(vs2);

            fillMode.Text = state.RS.FillMode.ToString();
            cullMode.Text = state.RS.CullMode.ToString();
            frontCCW.Image = state.RS.FrontCCW ? tick : cross;

            depthBias.Text = Formatter.Format(state.RS.depthBias);
            depthBiasClamp.Text = Formatter.Format(state.RS.depthBiasClamp);
            slopeScaledBias.Text = Formatter.Format(state.RS.slopeScaledDepthBias);

            depthClamp.Image = state.RS.depthClampEnable ? tick : cross;
            rasterizerDiscard.Image = state.RS.rasterizerDiscardEnable ? tick : cross;
            lineWidth.Text = Formatter.Format(state.RS.lineWidth);

            sampleCount.Text = state.MSAA.rasterSamples.ToString();
            sampleShading.Image = state.MSAA.sampleShadingEnable ? tick : cross;
            minSampleShading.Text = Formatter.Format(state.MSAA.minSampleShading);
            sampleMask.Text = state.MSAA.sampleMask.ToString("X8");
            
            ////////////////////////////////////////////////
            // Output Merger

            bool[] targets = new bool[8];

            for (int i = 0; i < 8; i++)
                targets[i] = false;

            vs = targetOutputs.VScrollValue();
            targetOutputs.BeginUpdate();
            targetOutputs.Nodes.Clear();
            if (state.Pass.framebuffer.attachments != null)
            {
                int i = 0;
                foreach (var p in state.Pass.framebuffer.attachments)
                {
                    int colIdx = Array.IndexOf(state.Pass.renderpass.colorAttachments, (uint)i);
                    int resIdx = Array.IndexOf(state.Pass.renderpass.resolveAttachments, (uint)i);

                    bool filledSlot = (p.img != ResourceId.Null);
                    bool usedSlot = (colIdx >= 0 || resIdx >= 0 || state.Pass.renderpass.depthstencilAttachment == i);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        UInt32 w = 1, h = 1, d = 1;
                        UInt32 a = 1;
                        string format = p.viewfmt.ToString();
                        string name = "Texture " + p.ToString();
                        string typename = "Unknown";
                        object tag = null;
                        bool viewDetails = false;

                        if (p.img == ResourceId.Null)
                        {
                            name = "Empty";
                            format = "-";
                            typename = "-";
                            w = h = d = a = 0;
                        }

                        for (int t = 0; t < texs.Length; t++)
                        {
                            if (texs[t].ID == p.img)
                            {
                                w = texs[t].width;
                                h = texs[t].height;
                                d = texs[t].depth;
                                a = texs[t].arraysize;
                                name = texs[t].name;
                                typename = texs[t].resType.Str();

                                if (!texs[t].customName && state.m_FS.ShaderDetails != null)
                                {
                                    for(int s=0; s < state.m_FS.ShaderDetails.OutputSig.Length; s++)
                                    {
                                        if(state.m_FS.ShaderDetails.OutputSig[s].regIndex == colIdx &&
                                            (state.m_FS.ShaderDetails.OutputSig[s].systemValue == SystemAttribute.None ||
                                            state.m_FS.ShaderDetails.OutputSig[s].systemValue == SystemAttribute.ColourOutput))
                                        {
                                            name = String.Format("<{0}>", state.m_FS.ShaderDetails.OutputSig[s].varName);
                                        }
                                    }
                                }

                                if (HasImportantViewParams(p, texs[t]))
                                    viewDetails = true;

                                tag = new ViewTexTag(p.viewfmt, p.baseMip, p.baseLayer, p.numMip, p.numLayer, texs[t]);
                            }
                        }

                        if (p.swizzle[0] != TextureSwizzle.Red ||
                            p.swizzle[1] != TextureSwizzle.Green ||
                            p.swizzle[2] != TextureSwizzle.Blue ||
                            p.swizzle[3] != TextureSwizzle.Alpha)
                        {
                            format += String.Format(" swizzle[{0}{1}{2}{3}]",
                                p.swizzle[0].Str(),
                                p.swizzle[1].Str(),
                                p.swizzle[2].Str(),
                                p.swizzle[3].Str());
                        }

                        string slotname = String.Format("Color {0}", i);
                        if (resIdx >= 0)
                            slotname = String.Format("Resolve {0}", i);
                        else if (colIdx < 0)
                            slotname = "Depth";

                        var node = targetOutputs.Nodes.Add(new object[] { slotname, name, typename, w, h, d, a, format });

                        node.Image = global::renderdocui.Properties.Resources.action;
                        node.HoverImage = global::renderdocui.Properties.Resources.action_hover;
                        node.Tag = tag;

                        if (p.img == ResourceId.Null)
                        {
                            EmptyRow(node);
                        }
                        else if (!usedSlot)
                        {
                            InactiveRow(node);
                        }
                        else
                        {
                            targets[i] = true;

                            ViewDetailsRow(node, viewDetails);
                        }
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
            {
                int i = 0;
                foreach(var blend in state.CB.attachments)
                {
                    bool filledSlot = true;
                    bool usedSlot = (targets[i]);

                    // show if
                    if (usedSlot || // it's referenced by the shader - regardless of empty or not
                        (showDisabled.Checked && !usedSlot && filledSlot) || // it's bound, but not referenced, and we have "show disabled"
                        (showEmpty.Checked && !filledSlot) // it's empty, and we have "show empty"
                        )
                    {
                        var node = blendOperations.Nodes.Add(new object[] { i,
                                                        blend.blendEnable,

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

            blendFactor.Text = state.CB.blendConst[0].ToString("F2") + ", " +
                                state.CB.blendConst[1].ToString("F2") + ", " +
                                state.CB.blendConst[2].ToString("F2") + ", " +
                                state.CB.blendConst[3].ToString("F2");
            logicOp.Text = state.CB.logicOpEnable ? state.CB.Logic.ToString() : "-";
            alphaToOne.Image = state.CB.alphaToOneEnable ? tick : cross;

            depthEnable.Image = state.DS.depthTestEnable ? tick : cross;
            depthFunc.Text = state.DS.depthCompareOp.ToString();
            depthWrite.Image = state.DS.depthWriteEnable ? tick : cross;

            if (state.DS.depthBoundsEnable)
            {
                depthBounds.Text = Formatter.Format(state.DS.minDepthBounds) + "-" + Formatter.Format(state.DS.maxDepthBounds);
                depthBounds.Image = null;
            }
            else
            {
                depthBounds.Text = "";
                depthBounds.Image = cross;
            }

            stencilFuncs.BeginUpdate();
            stencilFuncs.Nodes.Clear();
            if (state.DS.stencilTestEnable)
            {
                stencilFuncs.Nodes.Add(new object[] { "Front", state.DS.front.Func, state.DS.front.FailOp, 
                                                 state.DS.front.DepthFailOp, state.DS.front.PassOp,
                                                 state.DS.front.writeMask.ToString("X2"),
                                                 state.DS.front.compareMask.ToString("X2"),
                                                 state.DS.front.stencilref.ToString("X2")});
                stencilFuncs.Nodes.Add(new object[] { "Back", state.DS.back.Func, state.DS.back.FailOp, 
                                                 state.DS.back.DepthFailOp, state.DS.back.PassOp,
                                                 state.DS.back.writeMask.ToString("X2"),
                                                 state.DS.back.compareMask.ToString("X2"),
                                                 state.DS.back.stencilref.ToString("X2")});
            }
            else
            {
                stencilFuncs.Nodes.Add(new object[] { "Front", "-", "-", "-", "-", "-", "-", "-" });
                stencilFuncs.Nodes.Add(new object[] { "Back", "-", "-", "-", "-", "-", "-", "-" });
            }
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
                    state.m_TCS.Shader != ResourceId.Null,
                    state.m_TES.Shader != ResourceId.Null,
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

        private void HideViewDetailsTooltip()
        {
            if (m_CurViewDetailNode != null)
                toolTip.Hide(m_CurViewDetailNode.OwnerView);
            m_CurViewDetailNode = null;
        }

        private void MakeNodesTransparent(TreelistView.TreeListView treeview)
        {
            foreach (var n in treeview.Nodes)
            {
                foreach (var n2 in n.Nodes)
                    n2.DefaultBackColor = Color.Transparent;
                n.DefaultBackColor = Color.Transparent;
            }

            treeview.Invalidate();
        }

        private void resource_MouseLeave(object sender, EventArgs e)
        {
            MakeNodesTransparent((TreelistView.TreeListView)sender);

            HideViewDetailsTooltip();
        }

        private void resource_MouseMove(object sender, MouseEventArgs e)
        {
            TreelistView.TreeListView treeview = (TreelistView.TreeListView)sender;

            if (m_Core.CurVulkanPipelineState == null) return;

            Point mousePoint = treeview.PointToClient(Cursor.Position);
            var hoverNode = treeview.CalcHitNode(mousePoint);

            MakeNodesTransparent(treeview);

            if (hoverNode != null)
            {
                if (hoverNode.Tag is SamplerData)
                {
                    SamplerData data = (SamplerData)hoverNode.Tag;
                    foreach (var imgnode in data.images)
                        imgnode.DefaultBackColor = Color.Wheat;
                }
                else if (m_CombinedImageSamplers.ContainsKey(hoverNode))
                {
                    m_CombinedImageSamplers[hoverNode].DefaultBackColor = Color.LightCyan;
                }

                if (m_ViewDetailNodes.Contains(hoverNode))
                {
                    if (hoverNode != m_CurViewDetailNode)
                    {
                        // round y up to the next row
                        int y = (e.Location.Y - treeview.Columns.Options.HeaderHeight) / treeview.RowOptions.ItemHeight;
                        y = treeview.Columns.Options.HeaderHeight + (y + 1) * treeview.RowOptions.ItemHeight;

                        string text = "";

                        ViewTexTag tex = (hoverNode.Tag as ViewTexTag);
                        BufferResTag buf = (hoverNode.Tag as BufferResTag);

                        if (tex != null)
                        {
                            if(m_Core.CurVulkanPipelineState.Images.ContainsKey(tex.tex.ID))
                                text += String.Format("Texture is in the '{0}' layout\n\n", m_Core.CurVulkanPipelineState.Images[tex.tex.ID].layouts[0].name);

                            if (tex.tex.format != tex.fmt)
                                text += String.Format("The texture is format {0}, the view treats it as {1}.\n",
                                    tex.tex.format, tex.fmt);

                            if (tex.tex.mips > 1 && (tex.tex.mips != tex.numMip || tex.baseMip > 0))
                            {
                                if (tex.numMip == 1)
                                    text += String.Format("The texture has {0} mips, the view covers mip {1}.\n",
                                        tex.tex.mips, tex.baseMip);
                                else
                                    text += String.Format("The texture has {0} mips, the view covers mips {1}-{2}.\n",
                                        tex.tex.mips, tex.baseMip, tex.baseMip + tex.numMip - 1);
                            }

                            if (tex.tex.arraysize > 1 && (tex.tex.arraysize != tex.numLayer || tex.baseLayer > 0))
                            {
                                if (tex.numLayer == 1)
                                    text += String.Format("The texture has {0} array slices, the view covers slice {1}.\n",
                                        tex.tex.arraysize, tex.baseLayer);
                                else
                                    text += String.Format("The texture has {0} array slices, the view covers slices {1}-{2}.\n",
                                        tex.tex.arraysize, tex.baseLayer, tex.baseLayer + tex.numLayer);
                            }
                        }
                        else if (buf != null)
                        {
                            text += String.Format("The view covers bytes {0}-{1}.\nThe buffer is {3} bytes in length.",
                                buf.offset, buf.size,
                                m_Core.GetBuffer(buf.ID).length);
                        }

                        toolTip.Show(text.TrimEnd(), treeview, e.Location.X + Cursor.Size.Width, y);

                        m_CurViewDetailNode = hoverNode;
                    }
                }
                else // node is not in view details list
                {
                    HideViewDetailsTooltip();
                }
            }
            else
            {
                HideViewDetailsTooltip();
            }
        }
        // launch the appropriate kind of viewer, depending on the type of resource that's in this node
        private void textureCell_CellDoubleClick(TreelistView.Node node)
        {
            object tag = node.Tag;

            VulkanPipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage == null) return;

            if (tag is ViewTexTag)
                tag = (tag as ViewTexTag).tex;

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
            else if (tag is BufferResTag)
            {
                BufferResTag buf = (BufferResTag)tag;

                ShaderResource shaderRes = buf.rwRes
                    ? stage.ShaderDetails.ReadWriteResources[buf.bindPoint]
                    : stage.ShaderDetails.ReadOnlyResources[buf.bindPoint];

                string format = "// struct " + shaderRes.variableType.Name + Environment.NewLine;

                if (shaderRes.variableType.members.Length > 1)
                {
                    format += "// members skipped as they are fixed size:" + Environment.NewLine;
                    for (int i = 0; i < shaderRes.variableType.members.Length - 1; i++)
                    {
                        format += shaderRes.variableType.members[i].type.descriptor.name + " " + shaderRes.variableType.members[i].name + ";" + Environment.NewLine;
                    }
                }

                if (shaderRes.variableType.members.Length > 0)
                {
                    format += "{" + Environment.NewLine + FormatMembers(1, "", shaderRes.variableType.members.Last().type.members) + "}";
                }
                else
                {
                    var desc = shaderRes.variableType.descriptor;

                    format = "";
                    if (desc.rowMajorStorage)
                        format += "row_major ";

                    format += desc.type.Str();
                    if (desc.rows > 1 && desc.cols > 1)
                        format += String.Format("{0}x{1}", desc.rows, desc.cols);
                    else if (desc.cols > 1)
                        format += desc.cols;

                    if (desc.name.Length > 0)
                        format += " " + desc.name;

                    if (desc.elements > 1)
                        format += String.Format("[{0}]", desc.elements);
                }

                if (buf.ID != ResourceId.Null)
                {
                    var viewer = new BufferViewer(m_Core, false);
                    viewer.ViewRawBuffer(true, buf.offset, buf.size, buf.ID, format);
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
                    TreelistView.TreeListView view = (TreelistView.TreeListView)sender;
                    view.SortNodesSelection();
                    TreelistView.NodesSelection sel = view.NodesSelection;

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
            var viewer = m_Core.GetMeshViewer();
            viewer.Show(m_DockContent.DockPanel);
        }

        private VulkanPipelineState.ShaderStage GetStageForSender(object sender)
        {
            VulkanPipelineState.ShaderStage stage = null;

            if (!m_Core.LogLoaded)
                return null;

            object cur = sender;

            while (cur is Control)
            {
                if (cur == tabVS)
                    stage = m_Core.CurVulkanPipelineState.m_VS;
                else if (cur == tabGS)
                    stage = m_Core.CurVulkanPipelineState.m_GS;
                else if (cur == tabHS)
                    stage = m_Core.CurVulkanPipelineState.m_TCS;
                else if (cur == tabDS)
                    stage = m_Core.CurVulkanPipelineState.m_TES;
                else if (cur == tabPS)
                    stage = m_Core.CurVulkanPipelineState.m_FS;
                else if (cur == tabCS)
                    stage = m_Core.CurVulkanPipelineState.m_CS;
                else if (cur == tabOM)
                    stage = m_Core.CurVulkanPipelineState.m_FS;

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
            VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null) return;

            ShaderViewer s = new ShaderViewer(m_Core, shaderDetails, stage.stage, null, "");

            s.Show(m_DockContent.DockPanel);
        }

        private void shaderSave_Click(object sender, EventArgs e)
        {
            VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null) return;

            shaderSaveDialog.FileName = "";

            DialogResult res = shaderSaveDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                try
                {
                    FileStream writer = File.Create(shaderSaveDialog.FileName);

                    writer.Write(shaderDetails.RawBytes, 0, shaderDetails.RawBytes.Length);

                    writer.Flush();
                    writer.Close();
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save to " + shaderSaveDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot save",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
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

        // start a shaderviewer to edit this shader, optionally generating stub GLSL if there isn't
        // GLSL source available for this shader.
        private void shaderedit_Click(object sender, EventArgs e)
        {
            VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage == null) return;

            ShaderReflection shaderDetails = stage.ShaderDetails;

            if (stage.Shader == ResourceId.Null || shaderDetails == null) return;

            var files = new Dictionary<string, string>();

            if (m_Core.Config.ExternalDisassemblerEnabled)
            {
                BackgroundWorker bgWorker = new BackgroundWorker();

                ProgressPopup modal = new ProgressPopup(delegate { return !bgWorker.IsBusy; }, false);
                modal.SetModalText("Please wait - running external disassembler.");

                bgWorker.RunWorkerCompleted += (obj, eventArgs) =>
                {
                    if((bool)eventArgs.Result == true)
                    {
                        ShowShaderViewer(stage, files);
                    }
                };
                bgWorker.DoWork += (obj, eventArgs) =>
                {
                    eventArgs.Result = true;

                    //try to use the external disassembler to get back the shader source
                    string spv_bin_file = "spv_bin.spv";
                    string spv_disas_file = "spv_disas.txt";

                    spv_bin_file = Path.Combine(Path.GetTempPath(), spv_bin_file);
                    spv_disas_file = Path.Combine(Path.GetTempPath(), spv_disas_file);

                    //replace the {spv_bin} tag with the correct SPIR-V binary source
                    string args = m_Core.Config.GetDefaultExternalDisassembler().args;
                    if (args.Contains(ExternalDisassembler.SPV_BIN_TAG))
                    {
                        args = args.Replace(ExternalDisassembler.SPV_BIN_TAG, spv_bin_file);

                        //write to temp file
                        try
                        {
                            // Open file for reading
                            using (FileStream fileStream =
                                new FileStream(spv_bin_file, FileMode.Create,
                                                        FileAccess.Write))
                            {
                                // Writes a block of bytes to this stream using data from
                                // a byte array.
                                fileStream.Write(shaderDetails.RawBytes, 0, shaderDetails.RawBytes.Length);

                                // close file stream
                                fileStream.Close();
                            }
                        }
                        catch (Exception ex)
                        {
                            // Error
                            MessageBox.Show("Couldn't save SPIR-V binary to file: " + spv_bin_file +
                                Environment.NewLine + ex.ToString(), "Cannot save",
                                MessageBoxButtons.OK, MessageBoxIcon.Error);
                            eventArgs.Result = false;
                        }
                    }
                    else
                    {
                        MessageBox.Show("Please indicate the " + ExternalDisassembler.SPV_BIN_TAG +
                            " in the arguments list!", "Error disassembling",
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                        eventArgs.Result = false;
                    }
                    bool useStdout = true;
                    if (args.Contains(ExternalDisassembler.SPV_DISAS_TAG))
                    {
                        args = args.Replace(ExternalDisassembler.SPV_DISAS_TAG, spv_disas_file);
                        useStdout = false;
                    }
                    // else we assume the disassembler will return the result to stdout

                    //check if any errors
                    if ((bool)eventArgs.Result == false)
                        return;

                    //run the disassembler
                    try
                    {
                        using (System.Diagnostics.Process disassemblerProcess = new System.Diagnostics.Process())
                        {
                            disassemblerProcess.StartInfo.WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden;
                            //settings up parameters for the install process
                            disassemblerProcess.StartInfo.FileName = m_Core.Config.GetDefaultExternalDisassembler().executable;

                            disassemblerProcess.StartInfo.Arguments = args;
                            disassemblerProcess.StartInfo.RedirectStandardOutput = true;
                            disassemblerProcess.StartInfo.UseShellExecute = false;
                            disassemblerProcess.StartInfo.CreateNoWindow = true;

                            disassemblerProcess.Start();

                            string shaderDisassembly = "";
                            if(useStdout)
                            {
                                StringBuilder q = new StringBuilder();
                                while (!disassemblerProcess.HasExited)
                                {
                                    q.Append(disassemblerProcess.StandardOutput.ReadToEnd());
                                }
                                shaderDisassembly = q.ToString();
                            } else
                            {
                                //read from the output file after the process has finished
                                bool ok = disassemblerProcess.WaitForExit(5*1000); //wait for 5 sec max
                                if(ok)
                                {
                                    shaderDisassembly = File.ReadAllText(spv_disas_file);
                                }
                                else
                                {
                                    eventArgs.Result = false;
                                }
                            }

                            files.Add("Disassembly", shaderDisassembly);

                            // Check for sucessful completion
                            if (disassemblerProcess.ExitCode != 0)
                            {
                                MessageBox.Show("Error wile running external disassembler: " + m_Core.Config.GetDefaultExternalDisassembler().name +
                                                        Environment.NewLine + "Return code: " + disassemblerProcess.ExitCode, "Error disassembling",
                                                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                                eventArgs.Result = false;
                            }
                        }

                        //delete the temp files
                        if(File.Exists(spv_bin_file))
                            File.Delete(spv_bin_file);
                        if(File.Exists(spv_disas_file))
                            File.Delete(spv_disas_file);
                    }
                    catch (Exception ex)
                    { 
                        // Error
                        MessageBox.Show("Error using external disassembler: " + m_Core.Config.GetDefaultExternalDisassembler().name + 
                                                    Environment.NewLine + ex.ToString(), "Error disassembling",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                        eventArgs.Result = false;
                    }
                };
                bgWorker.RunWorkerAsync();

                modal.ShowDialog();
            }
            else
            {
                // use disassembly for now. It's not compilable GLSL but it's better than
                // starting with a blank canvas
                m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    var disasm = r.DisassembleShader(shaderDetails, "");
                    files.Add("Disassembly", disasm);
                    this.BeginInvoke((MethodInvoker)delegate
                    {
                        ShowShaderViewer(stage, files);
                    });
                });
            }
        }

        private void ShowShaderViewer(VulkanPipelineState.ShaderStage stage, Dictionary<String, String> files)
        {
            VulkanPipelineStateViewer pipeviewer = this;

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
                    ResourceId to = r.BuildTargetShader("main", compileSource, stage.ShaderDetails.DebugInfo.compileFlags, stage.stage, out errs);

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

        private void ShowCBuffer(VulkanPipelineState.ShaderStage stage, CBufferTag tag)
        {
            if (tag == null || tag.slotIdx == uint.MaxValue) return;

            VulkanPipelineState.Pipeline pipe = m_Core.CurVulkanPipelineState.graphics;
            if(stage.stage == ShaderStageType.Compute)
                pipe = m_Core.CurVulkanPipelineState.compute;

            var existing = ConstantBufferPreviewer.Has(stage.stage, tag.slotIdx, tag.arrayIdx);
            if (existing != null)
            {
                existing.Show();
                return;
            }

            var prev = new ConstantBufferPreviewer(m_Core, stage.stage, tag.slotIdx, tag.arrayIdx);

            prev.ShowDock(m_DockContent.Pane, DockAlignment.Right, 0.3);
        }

        private void cbuffers_NodeDoubleClicked(TreelistView.Node node)
        {
            VulkanPipelineState.ShaderStage stage = GetStageForSender(node.OwnerView);

            if (stage != null)
                ShowCBuffer(stage, node.Tag as CBufferTag);
        }

        private void CBuffers_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

            if (stage != null)
                ShowCBuffer(stage, ((DataGridView)sender).Rows[e.RowIndex].Tag as CBufferTag);
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
            if (sender is PictureBox)
            {
                VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage != null && stage.Shader != ResourceId.Null)
                    (sender as PictureBox).Image = global::renderdocui.Properties.Resources.action_hover;
            }

            if (sender is Label)
            {
                VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseEnter(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Control) shaderCog_MouseEnter(dsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Eval) shaderCog_MouseEnter(hsShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseEnter(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Fragment) shaderCog_MouseEnter(psShaderCog, e);
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
                VulkanPipelineState.ShaderStage stage = GetStageForSender(sender);

                if (stage == null) return;

                if (stage.stage == ShaderStageType.Vertex) shaderCog_MouseEnter(vsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Control) shaderCog_MouseEnter(dsShaderCog, e);
                if (stage.stage == ShaderStageType.Tess_Eval) shaderCog_MouseEnter(hsShaderCog, e);
                if (stage.stage == ShaderStageType.Geometry) shaderCog_MouseEnter(gsShaderCog, e);
                if (stage.stage == ShaderStageType.Fragment) shaderCog_MouseEnter(psShaderCog, e);
                if (stage.stage == ShaderStageType.Compute) shaderCog_MouseEnter(csShaderCog, e);
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
            var viewer = m_Core.GetMeshViewer();
            viewer.Show(m_DockContent.DockPanel);
        }

        private float GetHueForVB(int i)
        {
            int idx = ((i+1) * 21) % 32; // space neighbouring colours reasonably distinctly
            return (float)(idx) / 32.0f;
        }

        private void HighlightIABind(uint slot)
        {
            var VI = m_Core.CurVulkanPipelineState.VI;
        
            Color c = HSLColor(GetHueForVB((int)slot), 1.0f, 0.95f);
            
            if (slot < m_VBNodes.Count)
                m_VBNodes[(int)slot].DefaultBackColor = c;

            if (slot < m_BindNodes.Count)
                m_BindNodes[(int)slot].DefaultBackColor = c;

            for (int i = 0; i < viAttrs.Nodes.Count; i++)
            {
                var n = viAttrs.Nodes[i];
                if (VI.attrs[i].binding == slot)
                    n.DefaultBackColor = c;
                else
                    n.DefaultBackColor = Color.Transparent;
            }

            viAttrs.Invalidate();
            viBuffers.Invalidate();
        }

        private void iaattrs_MouseMove(object sender, MouseEventArgs e)
        {
            if (m_Core.CurVulkanPipelineState == null) return;

            Point mousePoint = viAttrs.PointToClient(Cursor.Position);
            var hoverNode = viAttrs.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            var VI = m_Core.CurVulkanPipelineState.VI;

            if (hoverNode != null)
            {
                int index = viAttrs.Nodes.GetNodeIndex(hoverNode);

                if (index >= 0 && index < VI.attrs.Length)
                {
                    uint binding = VI.attrs[index].binding;

                    HighlightIABind(binding);
                }
            }
        }

        private void iabuffers_MouseMove(object sender, MouseEventArgs e)
        {
            if (m_Core.CurVulkanPipelineState == null) return;

            Point mousePoint = viBuffers.PointToClient(Cursor.Position);
            var hoverNode = viBuffers.CalcHitNode(mousePoint);

            ia_MouseLeave(sender, e);

            if (hoverNode != null)
            {
                int idx = m_VBNodes.IndexOf(hoverNode);
                if (idx >= 0)
                    HighlightIABind((uint)idx);
                else
                    hoverNode.DefaultBackColor = SystemColors.ControlLight;
            }
        }

        private void ia_MouseLeave(object sender, EventArgs e)
        {
            foreach (var n in viBuffers.Nodes)
                n.DefaultBackColor = Color.Transparent;

            foreach (var n in viAttrs.Nodes)
                n.DefaultBackColor = Color.Transparent;

            viAttrs.Invalidate();
            viBuffers.Invalidate();
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

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.InputAssembly ia)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Index Buffer");
                writer.WriteEndElement();

                FetchBuffer ib = m_Core.GetBuffer(ia.ibuffer.buf);

                string name = "Empty";
                ulong length = 0;

                if (ib != null)
                {
                    name = ib.name;
                    length = ib.length;
                }

                string ifmt = "UNKNOWN";
                if (m_Core.CurDrawcall.indexByteWidth == 2)
                    ifmt = "UINT16";
                if (m_Core.CurDrawcall.indexByteWidth == 4)
                    ifmt = "UINT32";

                ExportHTMLTable(writer, new string[] { "Buffer", "Format", "Offset", "Byte Length", "Primitive Restart" },
                    new object[] { name, ifmt, ia.ibuffer.offs.ToString(), length.ToString(), ia.primitiveRestartEnable ? "Yes" : "No" });
            }

            writer.WriteStartElement("p");
            writer.WriteEndElement();

            ExportHTMLTable(writer,
                new string[] { "Primitive Topology", "Tessellation Control Points" },
                new object[] { m_Core.CurDrawcall.topology.Str(), m_Core.CurVulkanPipelineState.Tess.numControlPoints });
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.VertexInput vi)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Attributes");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                foreach (var attr in vi.attrs)
                    rows.Add(new object[] { attr.location, attr.binding, attr.format.ToString(), attr.byteoffset });

                ExportHTMLTable(writer, new string[] { "Location", "Binding", "Format", "Offset" }, rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Bindings");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                foreach (var attr in vi.binds)
                    rows.Add(new object[] { attr.vbufferBinding, attr.bytestride, attr.perInstance ? "PER_INSTANCE" : "PER_VERTEX" });

                ExportHTMLTable(writer, new string[] { "Binding", "Byte Stride", "Step Rate" }, rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Vertex Buffers");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var vb in vi.vbuffers)
                {
                    string name = "Buffer " + vb.buffer.ToString();
                    ulong length = 0;

                    if (vb.buffer == ResourceId.Null)
                    {
                        continue;
                    }
                    else
                    {
                        FetchBuffer buf = m_Core.GetBuffer(vb.buffer);
                        if(buf != null)
                        {
                            name = buf.name;
                            length = buf.length;
                        }
                    }

                    rows.Add(new object[] { i, name, vb.offset, length });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Binding", "Buffer", "Offset", "Byte Length" }, rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.ShaderStage sh)
        {
            ShaderReflection shaderDetails = sh.ShaderDetails;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Shader");
                writer.WriteEndElement();

                string shadername = "Unknown";

                if (sh.Shader == ResourceId.Null)
                    shadername = "Unbound";
                else
                    shadername = sh.ShaderName;

                if (shaderDetails != null && shaderDetails.DebugInfo.files.Length > 0)
                    shadername = shaderDetails.EntryPoint + "()" + " - " +
                                    shaderDetails.DebugInfo.files[0].BaseFilename;

                writer.WriteStartElement("p");
                writer.WriteString(shadername);
                writer.WriteEndElement();

                if (sh.Shader == ResourceId.Null)
                    return;
            }

            var pipeline = (sh.stage == ShaderStageType.Compute ? m_Core.CurVulkanPipelineState.compute : m_Core.CurVulkanPipelineState.graphics);
            
            if(shaderDetails.ConstantBlocks.Length > 0)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("UBOs");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for(int i=0; i < shaderDetails.ConstantBlocks.Length; i++)
                {
                    var b = shaderDetails.ConstantBlocks[i];
                    var bindMap = sh.BindpointMapping.ConstantBlocks[i];

                    if (!bindMap.used) continue;

                    var set = pipeline.DescSets[sh.BindpointMapping.ConstantBlocks[i].bindset];
                    var bind = set.bindings[sh.BindpointMapping.ConstantBlocks[i].bind];

                    string setname = bindMap.bindset.ToString();

                    string slotname = bindMap.bind.ToString();
                    slotname += ": " + b.name;

                    for (int a = 0; a < bind.descriptorCount; a++)
                    {
                        var descriptorBind = bind.binds[a];

                        ResourceId id = bind.binds[a].res;

                        if (bindMap.arraySize > 1)
                            slotname = String.Format("{0}: {1}[{2}]", bindMap.bind, b.name, a);

                        string name = "";
                        ulong byteOffset = descriptorBind.offset;
                        ulong length = descriptorBind.size;
                        int numvars = b.variables.Length;

                        if (descriptorBind.res == ResourceId.Null)
                        {
                            name = "Empty";
                            length = 0;
                        }

                        FetchBuffer buf = m_Core.GetBuffer(id);
                        if (buf != null)
                        {
                            name = buf.name;

                            if (length == ulong.MaxValue)
                                length = buf.length - byteOffset;
                        }

                        if (name == "")
                            name = "UBO " + descriptorBind.res.ToString();

                        // push constants
                        if (!b.bufferBacked)
                        {
                            setname = "";
                            slotname = b.name;
                            name = "Push constants";
                            byteOffset = 0;
                            length = 0;

                            // could maybe get range/size from ShaderVariable.reg if it's filled out
                            // from SPIR-V side.
                        }

                        rows.Add(new object[] { setname, slotname, name, byteOffset, length, numvars, b.byteSize });
                    }
                }

                ExportHTMLTable(writer, new string[] { "Set", "Bind", "Buffer", "Byte Offset", "Byte Size", "Number of Variables", "Bytes Needed" }, rows.ToArray());
            }
            
            if(shaderDetails.ReadOnlyResources.Length > 0)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Read-only Resources");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < shaderDetails.ReadOnlyResources.Length; i++)
                {
                    var b = shaderDetails.ReadOnlyResources[i];
                    var bindMap = sh.BindpointMapping.ReadOnlyResources[i];

                    if (!bindMap.used) continue;

                    var set = pipeline.DescSets[sh.BindpointMapping.ReadOnlyResources[i].bindset];
                    var bind = set.bindings[sh.BindpointMapping.ReadOnlyResources[i].bind];

                    string setname = bindMap.bindset.ToString();

                    string slotname = bindMap.bind.ToString();
                    slotname += ": " + b.name;

                    for (int a = 0; a < bind.descriptorCount; a++)
                    {
                        var descriptorBind = bind.binds[a];

                        ResourceId id = bind.binds[a].res;

                        if (bindMap.arraySize > 1)
                            slotname = String.Format("{0}: {1}[{2}]", bindMap.bind, b.name, a);

                        string name = "";

                        if (descriptorBind.res == ResourceId.Null)
                            name = "Empty";

                        FetchBuffer buf = m_Core.GetBuffer(id);
                        if (buf != null)
                            name = buf.name;

                        FetchTexture tex = m_Core.GetTexture(id);
                        if (tex != null)
                            name = tex.name;

                        if (name == "")
                            name = "Resource " + descriptorBind.res.ToString();

                        UInt64 w = 1;
                        UInt32 h = 1, d = 1;
                        UInt32 arr = 0;
                        string format = "Unknown";
                        string viewParams = "";

                        if(tex != null)
                        {
                            w = tex.width;
                            h = tex.height;
                            d = tex.depth;
                            arr = tex.arraysize;
                            format = tex.format.ToString();
                            name = tex.name;

                            if (tex.mips > 1)
                                viewParams = String.Format("Mips: {0}-{1}", descriptorBind.baseMip, descriptorBind.baseMip+descriptorBind.numMip - 1);

                            if (tex.arraysize > 1)
                            {
                                if (viewParams.Length > 0)
                                    viewParams += ", ";
                                viewParams += String.Format("Layers: {0}-{1}", descriptorBind.baseLayer, descriptorBind.baseLayer + descriptorBind.numLayer - 1);
                            }
                        }

                        if(buf != null)
                        {
                            w = buf.length;
                            h = 0;
                            d = 0;
                            a = 0;
                            format = "-";
                            name = buf.name;

                            ulong length = descriptorBind.size;

                            if (length == ulong.MaxValue)
                                length = buf.length - descriptorBind.offset;

                            viewParams = String.Format("Byte Range: {0} - {1}", descriptorBind.offset, descriptorBind.offset + length);
                        }

                        if (bind.type != ShaderBindType.Sampler)
                            rows.Add(new object[] { setname, slotname, name, bind.type, w, h, d, arr, format, viewParams });

                        if (bind.type == ShaderBindType.ImageSampler || bind.type == ShaderBindType.Sampler)
                        {
                            name = "Sampler " + descriptorBind.sampler.ToString();

                            if (bind.type == ShaderBindType.ImageSampler)
                                setname = slotname = "";

                            object[] sampDetails = MakeSampler("", "", descriptorBind);
                            rows.Add(new object[] {
                                setname, slotname, name, bind.type,
                                "", "", "", "",
                                sampDetails[5],
                                sampDetails[6]
                            });
                        }
                    }
                }

                ExportHTMLTable(writer, new string[] {
                    "Set", "Bind", "Buffer",
                    "Resource Type",
                    "Width", "Height", "Depth", "Array Size",
                    "Resource Format",
                    "View Parameters"
                    },
                    rows.ToArray());
            }
            
            if(shaderDetails.ReadWriteResources.Length > 0)
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Read-write Resources");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                for (int i = 0; i < shaderDetails.ReadWriteResources.Length; i++)
                {
                    var b = shaderDetails.ReadWriteResources[i];
                    var bindMap = sh.BindpointMapping.ReadWriteResources[i];

                    if (!bindMap.used) continue;

                    var set = pipeline.DescSets[sh.BindpointMapping.ReadWriteResources[i].bindset];
                    var bind = set.bindings[sh.BindpointMapping.ReadWriteResources[i].bind];

                    string setname = bindMap.bindset.ToString();

                    string slotname = bindMap.bind.ToString();
                    slotname += ": " + b.name;

                    for (int a = 0; a < bind.descriptorCount; a++)
                    {
                        var descriptorBind = bind.binds[a];

                        ResourceId id = bind.binds[a].res;

                        if (bindMap.arraySize > 1)
                            slotname = String.Format("{0}: {1}[{2}]", bindMap.bind, b.name, a);

                        string name = "";

                        if (descriptorBind.res == ResourceId.Null)
                            name = "Empty";

                        FetchBuffer buf = m_Core.GetBuffer(id);
                        if (buf != null)
                            name = buf.name;

                        FetchTexture tex = m_Core.GetTexture(id);
                        if (tex != null)
                            name = tex.name;

                        if (name == "")
                            name = "Resource " + descriptorBind.res.ToString();

                        UInt64 w = 1;
                        UInt32 h = 1, d = 1;
                        UInt32 arr = 0;
                        string format = "Unknown";
                        string viewParams = "";

                        if (tex != null)
                        {
                            w = tex.width;
                            h = tex.height;
                            d = tex.depth;
                            arr = tex.arraysize;
                            format = tex.format.ToString();
                            name = tex.name;

                            if (tex.mips > 1)
                                viewParams = String.Format("Highest Mip: {0}", descriptorBind.baseMip);

                            if (tex.arraysize > 1)
                            {
                                if (viewParams.Length > 0)
                                    viewParams += ", ";
                                viewParams += String.Format("First array slice: {0}", descriptorBind.baseLayer);
                            }
                        }

                        if (buf != null)
                        {
                            w = buf.length;
                            h = 0;
                            d = 0;
                            a = 0;
                            format = "-";
                            name = buf.name;

                            ulong length = descriptorBind.size;

                            if (length == ulong.MaxValue)
                                length = buf.length - descriptorBind.offset;

                            viewParams = String.Format("Byte Range: {0} - {1}", descriptorBind.offset, descriptorBind.offset + length);
                        }

                        rows.Add(new object[] { setname, slotname, name, bind.type, w, h, d, arr, format, viewParams });
                    }
                }

                ExportHTMLTable(writer, new string[] {
                    "Set", "Bind", "Buffer",
                    "Resource Type",
                    "Width", "Height", "Depth", "Array Size",
                    "Resource Format",
                    "View Parameters"
                    },
                    rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.Raster rs)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Raster State");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Fill Mode", "Cull Mode", "Front CCW" },
                    new object[] { rs.FillMode, rs.CullMode, rs.FrontCCW ? "Yes" : "No" });

                writer.WriteStartElement("p");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Depth Clip Enable", "Rasterizer Discard Enable" },
                    new object[] { rs.depthClampEnable ? "Yes" : "No", rs.rasterizerDiscardEnable ? "Yes" : "No" });

                writer.WriteStartElement("p");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Depth Bias", "Depth Bias Clamp", "Slope Scaled Bias", "Line Width" },
                    new object[] { Formatter.Format(rs.depthBias), Formatter.Format(rs.depthBiasClamp),
                                    Formatter.Format(rs.slopeScaledDepthBias), Formatter.Format(rs.lineWidth) });
            }

            VulkanPipelineState.MultiSample msaa = m_Core.CurVulkanPipelineState.MSAA;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Multisampling State");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Raster Samples", "Sample-rate shading", "Min Sample Shading Rate", "Sample Mask" },
                    new object[] { msaa.rasterSamples, msaa.sampleShadingEnable ? "Yes" : "No",
                                    Formatter.Format(msaa.minSampleShading), msaa.sampleMask.ToString("X8") });
            }

            VulkanPipelineState.ViewState vp = m_Core.CurVulkanPipelineState.VP;

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Viewports");
                writer.WriteEndElement();

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var vs in vp.viewportScissors)
                {
                    var v = vs.vp;

                    rows.Add(new object[] { i, v.x, v.y, v.Width, v.Height, v.MinDepth, v.MaxDepth });

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
                foreach (var vs in vp.viewportScissors)
                {
                    var s = vs.scissor;

                    rows.Add(new object[] { i, s.x, s.y, s.width, s.height });

                    i++;
                }

                ExportHTMLTable(writer, new string[] { "Slot", "X", "Y", "Width", "Height" }, rows.ToArray());
            }
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.ColorBlend cb)
        {
            writer.WriteStartElement("h3");
            writer.WriteString("Color Blend State");
            writer.WriteEndElement();

            var blendConst = Formatter.Format(cb.blendConst[0]) + ", " +
                                Formatter.Format(cb.blendConst[1]) + ", " +
                                Formatter.Format(cb.blendConst[2]) + ", " +
                                Formatter.Format(cb.blendConst[3]);

            ExportHTMLTable(writer,
                new string[] { "Alpha to Coverage", "Alpha to One", "Logic Op", "Blend Constant" },
                new object[] { cb.alphaToCoverageEnable ? "Yes" : "No", cb.alphaToOneEnable ? "Yes" : "No",
                                cb.logicOpEnable ? cb.Logic.ToString() : "Disabled", blendConst, });


            writer.WriteStartElement("h3");
            writer.WriteString("Attachment Blends");
            writer.WriteEndElement();

            List<object[]> rows = new List<object[]>();

            int i = 0;
            foreach (var b in cb.attachments)
            {
                rows.Add(new object[] {
                        i,
                        b.blendEnable ? "Yes" : "No",
                        b.m_Blend.Source, b.m_Blend.Destination, b.m_Blend.Operation,
                        b.m_AlphaBlend.Source, b.m_AlphaBlend.Destination, b.m_AlphaBlend.Operation,
                        ((b.WriteMask & 0x1) == 0 ? "_" : "R") +
                        ((b.WriteMask & 0x2) == 0 ? "_" : "G") +
                        ((b.WriteMask & 0x4) == 0 ? "_" : "B") +
                        ((b.WriteMask & 0x8) == 0 ? "_" : "A") });

                i++;
            }

            ExportHTMLTable(writer,
                new string[] {
                        "Slot",
                        "Blend Enable",
                        "Blend Source", "Blend Destination", "Blend Operation",
                        "Alpha Blend Source", "Alpha Blend Destination", "Alpha Blend Operation",
                        "Write Mask",
                    },
                rows.ToArray());
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.DepthStencil ds)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Depth State");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Depth Test Enable", "Depth Writes Enable", "Depth Function", "Depth Bounds" },
                    new object[] {
                        ds.depthTestEnable ? "Yes" : "No", ds.depthWriteEnable ? "Yes" : "No",
                        ds.depthCompareOp,
                        ds.depthBoundsEnable ? String.Format("{0} - {1}", Formatter.Format(ds.minDepthBounds), Formatter.Format(ds.maxDepthBounds)) : "Disabled",
                    });
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Stencil State");
                writer.WriteEndElement();

                if (ds.stencilTestEnable)
                {
                    List<object[]> rows = new List<object[]>();

                    rows.Add(new object[] {
                        "Front",
                        ds.front.stencilref.ToString("X2"), ds.front.compareMask.ToString("X2"), ds.front.writeMask.ToString("X2"),
                        ds.front.Func, ds.front.PassOp, ds.front.FailOp, ds.front.DepthFailOp
                    });

                    rows.Add(new object[] {
                        "Back",
                        ds.back.stencilref.ToString("X2"), ds.back.compareMask.ToString("X2"), ds.back.writeMask.ToString("X2"),
                        ds.back.Func, ds.back.PassOp, ds.back.FailOp, ds.back.DepthFailOp
                    });

                    ExportHTMLTable(writer,
                        new string[] { "Face", "Ref", "Compare Mask", "Write Mask", "Function", "Pass Op", "Fail Op", "Depth Fail Op" },
                        rows.ToArray());
                }
                else
                {
                    writer.WriteStartElement("p");
                    writer.WriteString("Disabled");
                    writer.WriteEndElement();
                }
            }
        }

        private void ExportHTML(XmlTextWriter writer, VulkanPipelineState.CurrentPass pass)
        {
            {
                writer.WriteStartElement("h3");
                writer.WriteString("Framebuffer");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "Width", "Height", "Layers" },
                    new object[] { pass.framebuffer.width, pass.framebuffer.height, pass.framebuffer.layers });

                List<object[]> rows = new List<object[]>();

                int i = 0;
                foreach (var a in pass.framebuffer.attachments)
                {
                    FetchTexture tex = m_Core.GetTexture(a.img);

                    string name = "Image " + a.img;

                    if (tex != null) name = tex.name;

                    rows.Add(new object[] {
                        i,
                        name, a.baseMip, a.numMip, a.baseLayer, a.numLayer });

                    i++;
                }

                ExportHTMLTable(writer,
                    new string[] {
                        "Slot",
                        "Image", "First mip", "Number of mips", "First array layer", "Number of layers",
                    },
                    rows.ToArray());
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Render Pass");
                writer.WriteEndElement();

                if (pass.renderpass.inputAttachments.Length > 0)
                {
                    object[][] inputs = new object[pass.renderpass.inputAttachments.Length][];

                    for (int i = 0; i < pass.renderpass.inputAttachments.Length; i++)
                        inputs[i] = new object[] { pass.renderpass.inputAttachments[i] };

                    ExportHTMLTable(writer, new string[] { "Input Attachment", }, inputs);

                    writer.WriteStartElement("p");
                    writer.WriteEndElement();

                }

                if (pass.renderpass.colorAttachments.Length > 0)
                {
                    object[][] colors = new object[pass.renderpass.colorAttachments.Length][];

                    for (int i = 0; i < pass.renderpass.colorAttachments.Length; i++)
                        colors[i] = new object[] { pass.renderpass.colorAttachments[i] };

                    ExportHTMLTable(writer, new string[] { "Color Attachment", }, colors);

                    writer.WriteStartElement("p");
                    writer.WriteEndElement();
                }

                if (pass.renderpass.depthstencilAttachment >= 0)
                {
                    writer.WriteStartElement("p");
                    writer.WriteString("Depth-stencil Attachment: " + pass.renderpass.depthstencilAttachment);
                    writer.WriteEndElement();
                }
            }

            {
                writer.WriteStartElement("h3");
                writer.WriteString("Render Area");
                writer.WriteEndElement();

                ExportHTMLTable(writer,
                    new string[] { "X", "Y", "Width", "Height" },
                    new object[] { pass.renderArea.x, pass.renderArea.y, pass.renderArea.width, pass.renderArea.height });
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

                    var title = String.Format("{0} EID {1} - Vulkan Pipeline export", Path.GetFileName(m_Core.LogFileName), m_Core.CurEvent);

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
                            var context = String.Format("Frame {0}", m_Core.FrameInfo.frameNumber);

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

                            context += String.Format(" => {0}", draw.name);

                            writer.WriteString(context);
                        }
                        writer.WriteEndElement(); // h2 context

                        string[] stageNames = new string[]{
                            "Input Assembly",
                            "Vertex Input",
                            "Vertex Shader",
                            "Tess. Control Shader",
                            "Tess. Eval. Shader",
                            "Geometry Shader",
                            "Rasterizer",
                            "Fragment Shader",
                            "Color Blend",
                            "Depth Stencil",
                            "Current Pass",
                            "Compute Shader",
                        };

                        string[] stageAbbrevs = new string[]{
                            "IA", "VI", "VS", "TCS", "TES", "GS", "RS", "FS", "CB", "DS", "Pass", "CS"
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
                                case 0:  ExportHTML(writer, m_Core.CurVulkanPipelineState.IA); break;
                                case 1:  ExportHTML(writer, m_Core.CurVulkanPipelineState.VI); break;
                                case 2:  ExportHTML(writer, m_Core.CurVulkanPipelineState.m_VS); break;
                                case 3:  ExportHTML(writer, m_Core.CurVulkanPipelineState.m_TCS); break;
                                case 4:  ExportHTML(writer, m_Core.CurVulkanPipelineState.m_TES); break;
                                case 5:  ExportHTML(writer, m_Core.CurVulkanPipelineState.m_GS); break;
                                case 6:  ExportHTML(writer, m_Core.CurVulkanPipelineState.RS); break;
                                case 7:  ExportHTML(writer, m_Core.CurVulkanPipelineState.m_FS); break;
                                case 8:  ExportHTML(writer, m_Core.CurVulkanPipelineState.CB); break;
                                case 9:  ExportHTML(writer, m_Core.CurVulkanPipelineState.DS); break;
                                case 10: ExportHTML(writer, m_Core.CurVulkanPipelineState.Pass); break;
                                case 11: ExportHTML(writer, m_Core.CurVulkanPipelineState.m_CS); break;
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