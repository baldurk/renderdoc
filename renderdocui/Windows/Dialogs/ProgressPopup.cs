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
using renderdocui.Code;

namespace renderdocui.Windows
{
    public delegate bool ModalCloseCallback();

    public partial class ProgressPopup : Form, ILogLoadProgressListener
    {
        ModalCloseCallback m_Callback;

        public ProgressPopup(ModalCloseCallback callback, bool realProgress)
        {
            InitializeComponent();
            m_Callback = callback;

            if (realProgress)
                progressBar.Style = ProgressBarStyle.Continuous;
        }

        #region ILogLoadProgressListener

        public void LogfileProgressBegin()
        {
        }

        public void LogfileProgress(float f)
        {
            if (!Visible) 
                return;

            BeginInvoke(new Action(() =>
            {
                if (m_Callback())
                {
                    Close();
                    return;
                }

                f = Helpers.Clamp(f, 0.0f, 1.0f);
                progressBar.Value = (int)(progressBar.Maximum * f);
            }));
        }

        #endregion

        public void SetModalText(String text)
        {
            ModalMessage.Text = text;
        }

        private void checkTimer_Tick(object sender, EventArgs e)
        {
            if (m_Callback())
            {
                Close();

                checkTimer.Stop();
                return;
            }

            checkTimer.Stop();
            checkTimer.Start();
        }
    }
}
