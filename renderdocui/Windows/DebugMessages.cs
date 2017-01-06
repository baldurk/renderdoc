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
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class DebugMessages : DockContent, ILogViewerForm
    {
        private Core m_Core;

        List<int> m_VisibleMessages = new List<int>();
        int m_NumMessages = 0;

        public DebugMessages(Core core)
        {
            InitializeComponent();

            if (SystemInformation.HighContrast)
                toolStrip1.Renderer = new ToolStripSystemRenderer();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

            messages.Font = core.Config.PreferredFont;

            RefreshMessageList();
        }

        public void OnLogfileClosed()
        {
            m_VisibleMessages.Clear();
            m_NumMessages = 0;
            messages.RowCount = 0;

            RefreshMessageList();
        }

        public void OnLogfileLoaded()
        {
            m_VisibleMessages.Clear();
            m_NumMessages = 0;
            messages.RowCount = 0;

            messages.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.AllCells;

            RefreshMessageList();
        }

        public void OnEventSelected(UInt32 eventID)
        {
        }

        private void messages_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex < m_VisibleMessages.Count)
                m_Core.SetEventID(null, m_Core.DebugMessages[m_VisibleMessages[e.RowIndex]].eventID);
        }

        private bool m_SuppressRefresh = false;

        public void RefreshMessageList()
        {
            if (m_SuppressRefresh) return;

            // add any new messages as default visible
            for (int i = m_NumMessages; i < m_Core.DebugMessages.Count; i++)
                m_VisibleMessages.Add(i);

            m_NumMessages = m_Core.DebugMessages.Count;

            if (displayHidden.Checked)
            {
                messages.RowCount = 0;
                messages.RowCount = m_Core.DebugMessages.Count;
            }
            else
            {
                messages.RowCount = 0;
                messages.RowCount = m_VisibleMessages.Count;
            }

            if (m_Core.UnreadMessageCount > 0)
                Text = String.Format("({0}) Errors and Warnings", m_Core.UnreadMessageCount);
            else
                Text = "Errors and Warnings";
        }

        bool IsRowVisible(int row)
        {
            return m_VisibleMessages.Contains(row);
        }

        void ShowRow(int row)
        {
            if (!IsRowVisible(row))
            {
                m_VisibleMessages.Add(row);
                m_VisibleMessages.Sort();
                RefreshMessageList();
            }
        }

        void HideRow(int row)
        {
            if (IsRowVisible(row))
            {
                m_VisibleMessages.Remove(row);
                RefreshMessageList();
            }
        }

        void ToggleRow(int row)
        {
            if (IsRowVisible(row))
            {
                HideRow(row);
            }
            else
            {
                ShowRow(row);
            }
        }

        private void displayHidden_CheckedChanged(object sender, EventArgs e)
        {
            RefreshMessageList();
        }

        int GetMessageIndex(int rowIndex)
        {
            if (displayHidden.Checked)
                return rowIndex;

            if (rowIndex < 0 || rowIndex >= m_VisibleMessages.Count)
                return -1;

            return m_VisibleMessages[rowIndex];
        }

        private void hideIndividual_Click(object sender, EventArgs e)
        {
            if (messages.SelectedRows.Count > 0)
            {
                m_SuppressRefresh = true;

                foreach (DataGridViewRow row in messages.SelectedRows)
                {
                    ToggleRow(GetMessageIndex(row.Index));
                }

                m_SuppressRefresh = false;

                RefreshMessageList();
            }

            messages.ClearSelection();
        }

        private void hideType_Click(object sender, EventArgs e)
        {
            if (messages.SelectedRows.Count == 1)
            {
                DataGridViewRow typerow = messages.SelectedRows[0];

                int msgIdx = GetMessageIndex(typerow.Index);

                DebugMessage msg = m_Core.DebugMessages[msgIdx];

                bool hiderows = IsRowVisible(msgIdx);

                m_SuppressRefresh = true;

                for(int i=0; i < m_Core.DebugMessages.Count; i++)
                {
                    var message = m_Core.DebugMessages[i];

                    if (message.category == msg.category &&
                        message.severity == msg.severity &&
                        message.messageID == msg.messageID)
                    {
                        if (hiderows)
                            HideRow(i);
                        else
                            ShowRow(i);
                    }
                }

                m_SuppressRefresh = false;

                RefreshMessageList();

                messages.ClearSelection();
            }
        }

        private void hideSource_Click(object sender, EventArgs e)
        {
            if (messages.SelectedRows.Count == 1)
            {
                DataGridViewRow typerow = messages.SelectedRows[0];

                int msgIdx = GetMessageIndex(typerow.Index);
                
                DebugMessage msg = m_Core.DebugMessages[msgIdx];

                bool hiderows = IsRowVisible(msgIdx);

                m_SuppressRefresh = true;

                for (int i = 0; i < m_Core.DebugMessages.Count; i++)
                {
                    var message = m_Core.DebugMessages[i];

                    if (message.source == msg.source)
                    {
                        if (hiderows)
                            HideRow(i);
                        else
                            ShowRow(i);
                    }
                }

                m_SuppressRefresh = false;

                RefreshMessageList();

                messages.ClearSelection();
            }
        }

        private void rightClickMenu_Opening(object sender, CancelEventArgs e)
        {
            hideType.Enabled = (messages.SelectedRows.Count == 1);
        }

        private void messages_MouseDown(object sender, MouseEventArgs e)
        {
            if (messages.SelectedRows.Count == 0 && e.Button == MouseButtons.Right)
            {
                var hti = messages.HitTest(e.X, e.Y);
                if(hti.RowIndex >= 0 && hti.RowIndex < messages.RowCount)
                    messages.Rows[hti.RowIndex].Selected = true;
            }

            if (e.Button == MouseButtons.Left)
            {
                var hti = messages.HitTest(e.X, e.Y);
                if (hti.RowIndex < 0 || hti.RowIndex >= messages.RowCount)
                    messages.ClearSelection();
            }
        }

        private void messages_CellValueNeeded(object sender, DataGridViewCellValueEventArgs e)
        {
            int msgIdx = GetMessageIndex(e.RowIndex);

            if (e.ColumnIndex < 0 || e.ColumnIndex >= messages.ColumnCount) return;
            if (msgIdx < 0 || msgIdx >= m_Core.DebugMessages.Count) return;

            if (e.ColumnIndex == 0) e.Value = m_Core.DebugMessages[msgIdx].eventID;
            if (e.ColumnIndex == 1) e.Value = m_Core.DebugMessages[msgIdx].source.Str();
            if (e.ColumnIndex == 2) e.Value = m_Core.DebugMessages[msgIdx].severity.ToString();
            if (e.ColumnIndex == 3) e.Value = m_Core.DebugMessages[msgIdx].category.ToString();
            if (e.ColumnIndex == 4) e.Value = m_Core.DebugMessages[msgIdx].messageID.ToString();
            if (e.ColumnIndex == 5) e.Value = m_Core.DebugMessages[msgIdx].description;
        }

        private void messages_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
        {
            int msgIdx = GetMessageIndex(e.RowIndex);

            if (e.ColumnIndex < 0 || e.ColumnIndex >= messages.ColumnCount) return;
            if (msgIdx < 0 || msgIdx >= m_Core.DebugMessages.Count) return;

            if (!IsRowVisible(msgIdx))
                e.CellStyle.BackColor = Color.Salmon;
            else if (m_Core.DebugMessages[msgIdx].source == DebugMessageSource.RuntimeWarning)
                e.CellStyle.BackColor = Color.Aquamarine;
        }

        private void DebugMessages_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }

        private void messages_CellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            // use BeginInvoke so we don't resize the messages mid-paint
            this.BeginInvoke(new Action(() =>
            {
                if (m_Core.UnreadMessageCount > 0)
                {
                    m_Core.UnreadMessageCount = 0;
                    RefreshMessageList();
                }
            }));
        }
    }
}
