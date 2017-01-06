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

namespace renderdocui.Windows.Dialogs
{
    public partial class ColumnSelector : Form
    {
        ListViewItem m_Required = null;

        public ColumnSelector(Dictionary<string, bool> columns, string required)
        {
            InitializeComponent();

            foreach (var c in columns)
            {
                var item = columnList.Items.Add(c.Key);
                item.Checked = c.Value;
                if (c.Key == required)
                    m_Required = item;
            }

            if(m_Required != null)
                m_Required.Font = new Font(m_Required.Font, FontStyle.Bold);
        }

        public Dictionary<string, bool> GetColumnValues()
        {
            var ret = new Dictionary<string, bool>();

            foreach (ListViewItem c in columnList.Items)
            {
                ret.Add(c.Text, c.Checked);
            }

            return ret;
        }

        private void columnList_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            if (m_Required != null && e.Index == m_Required.Index)
                e.NewValue = CheckState.Checked;
        }
    }
}
