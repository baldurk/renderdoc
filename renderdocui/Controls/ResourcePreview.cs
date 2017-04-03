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
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Controls
{
    [Designer(typeof(System.Windows.Forms.Design.ControlDesigner))]
    public partial class ResourcePreview : UserControl
    {
        private string m_Name;
        private UInt64 m_Width;
        private UInt32 m_Height, m_Depth, m_NumMips;
        private Core m_Core;
        private ReplayOutput m_Output;
        private IntPtr m_Handle;

        public ResourcePreview(Core core, ReplayOutput output)
        {
            InitializeComponent();

            descriptionLabel.Font = core.Config.PreferredFont;

            m_Name = "Unbound";
            m_Width = 1;
            m_Height = 1;
            m_Depth = 1;
            m_NumMips = 0;
            m_Unbound = true;
            thumbnail.Painting = false;

            m_Unbound = true;

            slotLabel.Text = "0";

            this.DoubleBuffered = true;

            SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);

            m_Handle = thumbnail.Handle;

            m_Core = core;
            m_Output = output;

            Selected = false;
        }

        public void Init()
        {
            descriptionLabel.Text = "Unbound";
            m_Unbound = true;
            thumbnail.Painting = true;
        }

        public void Init(string Name, UInt64 Width, UInt32 Height, UInt32 Depth, UInt32 NumMips)
        {
            m_Name = Name;
            m_Width = Width;
            m_Height = Height;
            m_Depth = Depth;
            m_NumMips = NumMips;
            m_Unbound = false;
            thumbnail.Painting = true;

            //descriptionLabel.Text = m_Width + "x" + m_Height + "x" + m_Depth + (m_NumMips > 0 ? "[" + m_NumMips + "]\n" : "\n") + m_Name;
            descriptionLabel.Text = m_Name;
        }

        public string SlotName
        {
            get { return slotLabel.Text; }
            set { slotLabel.Text = value; }
        }

        private bool m_Unbound = true;
        public bool Unbound
        {
            get
            {
                return m_Unbound;
            }
        }

        private bool m_Selected;
        public bool Selected
        {
            get { return m_Selected; }
            set
            {
                m_Selected = value;
                if (value)
                {
                    BackColor = Color.Red;
                }
                else
                {
                    BackColor = Color.Black;
                }
            }
        }

        public IntPtr ThumbnailHandle
        {
            get { return m_Handle; }
        }

        private void ResourcePreview_Load(object sender, EventArgs e)
        {
            Init(m_Name, m_Width, m_Height, m_Depth, m_NumMips);
        }

        public void Clear()
        {
            thumbnail.Invalidate();
        }

        private void thumbnail_Paint(object sender, PaintEventArgs e)
        {
            if (m_Output == null || m_Core.Renderer == null)
            {
                e.Graphics.Clear(Color.Black);
                return;
            }

            if (m_Output != null)
                m_Core.Renderer.InvokeForPaint("thumbpaint", (ReplayRenderer r) => { m_Output.Display(); });
        }

        public void SetSize(Size s)
        {
            MinimumSize = MaximumSize = s;
            Size = s;
        }

        private void child_MouseClick(object sender, MouseEventArgs e)
        {
            OnMouseClick(e);
        }

        private void child_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            OnMouseDoubleClick(e);
        }
    }
}
