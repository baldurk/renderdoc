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
using System.Threading;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    // this window lists the remote hosts the user has configured, and queries for any open connections
    // on any of them that indicate an application with renderdoc hooks that's running.
    public partial class OrderedListEditor : Form
    {
        public enum Browsing
        {
            None,
            Folder,
            File,
        };

        private Browsing browseMode = Browsing.Folder;

        public OrderedListEditor(Core core, string windowName, string itemName, Browsing browse)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            items.Font = core.Config.PreferredFont;

            Text = windowName;
            items.Columns[1].HeaderText = itemName;

            if (browse == Browsing.None)
                items.Columns.RemoveAt(BrowserColumnIndex);

            browseMode = browse;
        }

        private void SetItem(int row, String text)
        {
            items.Rows[row].SetValues(new object[] { null, text, null, null, null });
        }

        public void AddItem(String text)
        {
            items.Rows.Add();
            SetItem(items.RowCount - 2, text);
        }

        public string[] GetItems()
        {
            string[] ret = new string[items.RowCount - 1];

            for (int i = 0; i < items.RowCount - 1; i++)
                ret[i] = items.Rows[i].Cells[1].Value.ToString();

            return ret;
        }

        private int ItemNumberColumnIndex
        {
            get
            {
                return 0;
            }
        }

        private int ItemNameColumnIndex
        {
            get
            {
                return 1;
            }
        }

        private int FirstButtonColumnIndex
        {
            get
            {
                return 2;
            }
        }

        private int BrowserColumnIndex
        {
            get
            {
                return browseMode == Browsing.None ? -1 : 2;
            }
        }

        private int MoveUpColumnIndex
        {
            get
            {
                return browseMode == Browsing.None ? 2 : 3;
            }
        }

        private int MoveDownColumnIndex
        {
            get
            {
                return browseMode == Browsing.None ? 3 : 4;
            }
        }

        private bool IsClickableCell(int row, int column)
        {
            if (column < FirstButtonColumnIndex)
                return false;

            if (column == MoveUpColumnIndex && row == 0)
                return false;

            if (column == MoveDownColumnIndex && row >= items.RowCount - 2)
                return false;

            if (column != BrowserColumnIndex && row == items.RowCount - 1)
                return false;

            return true;
        }

        private void items_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
        {
            if (e.ColumnIndex == ItemNumberColumnIndex)
            {
                e.Value = e.RowIndex + 1;
                return;
            }
            else if (e.ColumnIndex == BrowserColumnIndex)
            {
                e.Value = global::renderdocui.Properties.Resources.folder_page;
                return;
            }
            else if (e.ColumnIndex == MoveUpColumnIndex)
            {
                if (IsClickableCell(e.RowIndex, e.ColumnIndex))
                {
                    e.Value = global::renderdocui.Properties.Resources.up_arrow;
                    return;
                }
            }
            else if (e.ColumnIndex == MoveDownColumnIndex)
            {
                if(IsClickableCell(e.RowIndex, e.ColumnIndex))
                {
                    e.Value = global::renderdocui.Properties.Resources.down_arrow;
                    return;
                }
            }

            if (e.ColumnIndex >= FirstButtonColumnIndex)
                e.Value = new Bitmap(1, 1);
        }

        private void items_CellMouseMove(object sender, DataGridViewCellMouseEventArgs e)
        {
            if (IsClickableCell(e.RowIndex, e.ColumnIndex))
                items.Cursor = Cursors.Hand;
            else
                items.Cursor = Cursors.Default;
        }

        private void items_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.ColumnIndex == BrowserColumnIndex)
            {
                if(browseMode == Browsing.Folder)
                {
                    var res = itemFolderBrowser.ShowDialog();

                    if (res == DialogResult.Yes || res == DialogResult.OK)
                    {
                        if (items.Rows[e.RowIndex].IsNewRow)
                            AddItem(itemFolderBrowser.SelectedPath);
                        else
                            SetItem(e.RowIndex, itemFolderBrowser.SelectedPath);
                    }
                }
                else if (browseMode == Browsing.File)
                {
                    var res = itemFileBrowser.ShowDialog();

                    if (res == DialogResult.Yes || res == DialogResult.OK)
                    {
                        if (items.Rows[e.RowIndex].IsNewRow)
                            AddItem(itemFileBrowser.FileName);
                        else
                            SetItem(e.RowIndex, itemFileBrowser.FileName);
                    }
                }
            }
            else if (e.ColumnIndex == MoveUpColumnIndex && e.RowIndex > 0 && e.RowIndex < items.RowCount - 1)
            {
                var row = items.Rows[e.RowIndex];
                items.Rows.RemoveAt(e.RowIndex);
                items.Rows.Insert(e.RowIndex - 1, row);
                items.ClearSelection();
                row.Selected = true;
            }
            else if (e.ColumnIndex == MoveDownColumnIndex && e.RowIndex < items.RowCount - 2)
            {
                var row = items.Rows[e.RowIndex];
                items.Rows.RemoveAt(e.RowIndex);
                items.Rows.Insert(e.RowIndex + 1, row);
                items.ClearSelection();
                row.Selected = true;
            }
        }
    }
}
