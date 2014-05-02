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
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdoc;
using renderdocui.Code;

namespace renderdocui.Windows.PipelineState
{
    // this is a tiny wrapper class. The only reason it's here is so that layouts get serialised
    // referring to this class, and we can then decide on a per-log basis which API's viewer to
    // instantiate and show, without needing people to change their layout.
    public partial class PipelineStateViewer : DockContent, ILogViewerForm
    {
        private Core m_Core;
        private D3D11PipelineStateViewer m_D3D11 = null;
        private GLPipelineStateViewer m_GL = null;
        private ILogViewerForm m_Current = null;

        public PipelineStateViewer(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

            DockHandler.GetPersistStringCallback = PersistString;

            m_D3D11 = new D3D11PipelineStateViewer(core, this);
            m_D3D11.Dock = DockStyle.Fill;
            Controls.Add(m_D3D11);
        }

        private string PersistString()
        {
            if (m_Current == m_D3D11)
                return GetType().ToString() + "D3D11";
            else if (m_Current == m_GL)
                return GetType().ToString() + "GL";

            return GetType().ToString();
        }

        public void InitFromPersistString(string str)
        {
            string type = str.Substring(GetType().ToString().Length);

            if (type == "GL")
                SetToGL();
            else if (type == "D3D11")
                SetToD3D11();
        }

        private void SetToD3D11()
        {
            m_GL = null;

            if (m_D3D11 == null)
            {
                Controls.Clear();
                m_D3D11 = new D3D11PipelineStateViewer(m_Core, this);
                m_D3D11.Dock = DockStyle.Fill;
                Controls.Add(m_D3D11);
            }

            m_Current = m_D3D11;
        }

        private void SetToGL()
        {
            m_D3D11 = null;

            if (m_GL == null)
            {
                Controls.Clear();
                m_GL = new GLPipelineStateViewer(m_Core, this);
                m_GL.Dock = DockStyle.Fill;
                Controls.Add(m_GL);
            }

            m_Current = m_GL;
        }

        public void OnLogfileLoaded()
        {
            if (m_Core.APIProps.pipelineType == APIPipelineStateType.D3D11)
                SetToD3D11();
            else if (m_Core.APIProps.pipelineType == APIPipelineStateType.OpenGL)
                SetToGL();

            m_Current.OnLogfileLoaded();
        }

        public void OnLogfileClosed()
        {
            if (m_Current != null)
                m_Current.OnLogfileClosed();
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
            if(m_Current != null)
                m_Current.OnEventSelected(frameID, eventID);
        }

        private void PipelineStateViewer_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }
    }
}
