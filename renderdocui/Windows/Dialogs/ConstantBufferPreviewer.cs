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
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;
using renderdocui.Windows.Dialogs;
using renderdoc;
using WeifenLuo.WinFormsUI.Docking;
using System.IO;

namespace renderdocui.Controls
{
	public partial class ConstantBufferPreviewer : DockContent, ILogViewerForm, IBufferFormatProcessor
    {
        private Core m_Core;

        public ConstantBufferPreviewer(Core c, ShaderStageType stage, UInt32 slot)
        {
            InitializeComponent();

            m_Core = c;
            Stage = stage;
            Slot = slot;
            shader = m_Core.CurPipelineState.GetShader(stage);
            UpdateLabels();

            uint offs = 0;
            uint size = 0;
            m_Core.CurPipelineState.GetConstantBuffer(Stage, Slot, out cbuffer, out offs, out size);

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
            {
                SetVariables(r.GetCBufferVariableContents(shader, Slot, cbuffer, offs));
            });

            m_Core.AddLogViewer(this);
        }

        private static List<ConstantBufferPreviewer> m_Docks = new List<ConstantBufferPreviewer>();

        public static DockContent Has(ShaderStageType stage, UInt32 slot)
        {
            foreach (var cb in m_Docks)
            {
                if(cb.Stage == stage && cb.Slot == slot)
                    return cb as DockContent;
            }

            return null;
        }

        public void ShowDock(DockPane pane, DockAlignment align, double proportion)
        {
            FormClosed += new FormClosedEventHandler(dock_FormClosed);
            
            if (m_Docks.Count > 0)
                Show(m_Docks[0].Pane, m_Docks[0]);
            else
                Show(pane, align, proportion);

            m_Docks.Add(this);
        }

        static void dock_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Docks.Remove(sender as ConstantBufferPreviewer);
        }

        /// <summary> 
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();

                m_Core.RemoveLogViewer(this);
            }
            base.Dispose(disposing);
        }

        public void OnLogfileClosed()
        {
            variables.BeginUpdate();
            variables.Nodes.Clear();

            variables.EndUpdate();
            variables.Invalidate();
        }

        public void OnLogfileLoaded()
        {
            variables.BeginUpdate();
            variables.Nodes.Clear();

            variables.EndUpdate();
            variables.Invalidate();
        }

        private void AddVariables(TreelistView.NodeCollection root, ShaderVariable[] vars)
        {
            foreach (var v in vars)
            {
                TreelistView.Node n = root.Add(new TreelistView.Node(new object[] { v.name, v, v.TypeString() }));

                if (v.rows > 1)
                {
                    for (int i = 0; i < v.rows; i++)
                    {
                        n.Nodes.Add(new TreelistView.Node(new object[] { String.Format("{0}.row{1}", v.name, i), v.Row(i), v.RowTypeString() }));
                    }
                }

                if (v.members.Length > 0)
                {
                    AddVariables(n.Nodes, v.members);
                }
            }
        }

        private void SetVariables(ShaderVariable[] vars)
        {
            if (variables.InvokeRequired)
            {
                this.BeginInvoke(new Action(() => { SetVariables(vars); }));
                return;
            }

            variables.BeginUpdate();
            variables.Nodes.Clear();

            if(vars != null && vars.Length > 0)
                AddVariables(variables.Nodes, vars);

            variables.EndUpdate();
            variables.Invalidate();
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            uint offs = 0;
            uint size = 0;
            m_Core.CurPipelineState.GetConstantBuffer(Stage, Slot, out cbuffer, out offs, out size);

            shader = m_Core.CurPipelineState.GetShader(Stage);
            var reflection = m_Core.CurPipelineState.GetShaderReflection(Stage);

            UpdateLabels();

            if (reflection == null || reflection.ConstantBlocks.Length <= Slot)
            {
                SetVariables(null);
                return;
            }

			if (m_FormatOverride != null)
			{
				m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
				{
					SetVariables(ApplyFormatOverride(r.GetBufferData(cbuffer, offs, size)));
				});
			}
			else
			{
				m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
				{
					SetVariables(r.GetCBufferVariableContents(shader, Slot, cbuffer, offs));
				});
			}
        }

        private string BufferName = "";

        private ResourceId cbuffer;
        private ResourceId shader;
        private ShaderStageType Stage;
        private UInt32 Slot = 0;

        public override string Text
        {
            get
            {
                return String.Format("{0} CB {1}", Stage.ToString(), Slot);
            }
        }

        private void UpdateLabels()
        {
            BufferName = "";

            bool needName = true;

            foreach (var b in m_Core.CurBuffers)
            {
                if (b.ID == cbuffer)
                {
                    BufferName = b.name;
                    if(b.customName)
                        needName = false;
                }
            }

            var reflection = m_Core.CurPipelineState.GetShaderReflection(Stage);

            if (reflection != null)
            {
                if (needName &&
                    Slot < reflection.ConstantBlocks.Length &&
                    reflection.ConstantBlocks[Slot].name.Length > 0)
                    BufferName = "<" + reflection.ConstantBlocks[Slot].name + ">";
            }

            nameLabel.Text = BufferName;

            slotLabel.Text = Stage.ToString();
            slotLabel.Text += " Shader Slot " + Slot;
        }

        private void variables_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.C && e.Control)
            {
                int[] width = new int[] { 0, 0, 0 };

                foreach (var n in variables.NodesSelection)
                {
                    width[0] = Math.Max(width[0], n[0].ToString().Length);
                    width[1] = Math.Max(width[1], n[1].ToString().Length);
                    width[2] = Math.Max(width[2], n[2].ToString().Length);
                }

                width[0] = Math.Min(50, width[0]);
                width[1] = Math.Min(50, width[1]);
                width[2] = Math.Min(50, width[2]);

                string fmt = "{0,-" + width[0] + "}   {1,-" + width[1] + "}   {2,-" + width[2] + "}" + Environment.NewLine;

                string text = "";
                foreach (var n in variables.NodesSelection)
                {
                    text += string.Format(fmt, n[0], n[1], n[2]);
                }

                Clipboard.SetText(text);
            }
        }

		private BufferFormatSpecifier m_FormatSpecifier = null;
		private FormatElement[] m_FormatOverride = null;

		ShaderVariable[] ApplyFormatOverride(byte[] data)
		{
			if(m_FormatOverride == null || m_FormatOverride.Length == 0) return null;

			var stream = new MemoryStream(data);
			var reader = new BinaryReader(stream);

			ShaderVariable[] ret = new ShaderVariable[m_FormatOverride.Length];

			for (int i = 0; i < m_FormatOverride.Length; i++)
			{
				stream.Seek(m_FormatOverride[i].offset, SeekOrigin.Begin);
				ret[i] = m_FormatOverride[i].GetShaderVar(reader);
			}

			reader.Dispose();
			stream.Dispose();

			return ret;
		}

		public void ProcessBufferFormat(string formatText)
		{
			if (formatText.Length == 0)
			{
				m_FormatOverride = null;
				if (m_FormatSpecifier != null)
					m_FormatSpecifier.SetErrors("");
			}
			else
			{
				string errors = "";

				m_FormatOverride = FormatElement.ParseFormatString(formatText, false, out errors);

				if (m_FormatSpecifier != null)
					m_FormatSpecifier.SetErrors(errors);
			}

			OnEventSelected(m_Core.CurFrame, m_Core.CurEvent);
		}

		private void setFormat_CheckedChanged(object sender, EventArgs e)
		{
			if (!setFormat.Checked)
			{
				split.Panel2.Controls.Remove(m_FormatSpecifier);
				split.Panel2Collapsed = true;
				
				ProcessBufferFormat("");

				return;
			}

			if (m_FormatSpecifier == null)
				m_FormatSpecifier = new BufferFormatSpecifier(this, "");

			split.Panel2.Controls.Add(m_FormatSpecifier);
			m_FormatSpecifier.Dock = DockStyle.Fill;
			split.Panel2Collapsed = false;
			split.SplitterDistance = split.ClientRectangle.Height / 2;
		}
    }
}
