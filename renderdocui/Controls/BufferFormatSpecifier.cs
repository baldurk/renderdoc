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
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    public partial class BufferFormatSpecifier : UserControl
    {
        IBufferFormatProcessor m_Viewer = null;

        public BufferFormatSpecifier(IBufferFormatProcessor viewer, string format)
        {
            InitializeComponent();

            // WHY THE HELL do you require \r\n in text boxes?
            formatText.Text = format.Replace("\r\n", "\n").Replace("\n", Environment.NewLine);

			errors.Visible = false;

            m_Viewer = viewer;
        }

        private void apply_Click(object sender, EventArgs e)
        {
            SetErrors("");
            m_Viewer.ProcessBufferFormat(formatText.Text);
        }

        public void SetErrors(string err)
        {
            errors.Text = err;
            if (errors.Text.Length == 0)
                errors.Visible = false;
            else
                errors.Visible = true;
        }

        private void formatText_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.A && e.Control)
            {
                e.SuppressKeyPress = true;
                formatText.SelectAll();
            }
        }

        public void ToggleHelp()
        {
            helpText.Visible = !helpText.Visible;

            tableLayoutPanel1.SuspendLayout();

            if (helpText.Visible)
            {
                tableLayoutPanel1.Controls.Remove(formatGroupBox);
                tableLayoutPanel1.Controls.Add(formatGroupBox, 0, 1);
                tableLayoutPanel1.SetRowSpan(formatGroupBox, 1);
            }
            else
            {
                tableLayoutPanel1.Controls.Remove(formatGroupBox);
                tableLayoutPanel1.Controls.Add(formatGroupBox, 0, 0);
                tableLayoutPanel1.SetRowSpan(formatGroupBox, 2);
            }

            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
        }

        private void toggleHelp_Click(object sender, EventArgs e)
        {
            ToggleHelp();
        }
    }

    public interface IBufferFormatProcessor
    {
        void ProcessBufferFormat(string formatText);
    }
}
