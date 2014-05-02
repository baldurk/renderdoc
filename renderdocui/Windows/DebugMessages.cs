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
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class DebugMessages : DockContent, ILogViewerForm
    {
        private Core m_Core;

        struct DebugMessage
        {
            public DebugMessage(int i, UInt32 e, renderdoc.DebugMessage m) { Index = i; EventID = e; Message = m; }
            public int Index;
            public UInt32 EventID;
            public renderdoc.DebugMessage Message;
        };

        List<DebugMessage> m_Messages = new List<DebugMessage>();
        List<int> m_VisibleMessages = new List<int>();

        public DebugMessages(Core core)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;
        }

        public void OnLogfileClosed()
        {
            m_Messages.Clear();
            m_VisibleMessages.Clear();
            messages.RowCount = 0;
        }

        public void AddDebugMessages(FetchDrawcall[] drawcalls)
        {
            foreach (var draw in drawcalls)
            {
                if (draw.context != m_Core.FrameInfo[m_Core.CurFrame].immContextId)
                    continue;

                if(draw.children.Length > 0)
                    AddDebugMessages(draw.children);

                if (draw.debugMessages != null)
                {
                    foreach (var msg in draw.debugMessages)
                    {
                        int i = m_Messages.Count;
                        m_VisibleMessages.Add(i);
                        m_Messages.Add(new DebugMessage(i, draw.eventID, msg));
                    }
                }
            }
        }

        public void OnLogfileLoaded()
        {
            m_Messages.Clear();
            m_VisibleMessages.Clear();
            messages.RowCount = 0;

            messages.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.DisplayedCells;

            for (UInt32 f = 0; f < m_Core.FrameInfo.Length; f++)
            {
                FetchDrawcall[] drawcalls = m_Core.GetDrawcalls(f);

                AddDebugMessages(m_Core.GetDrawcalls(f));
            }

            messages.RowCount = m_Messages.Count;
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
        }

        private void DebugMessages_Shown(object sender, EventArgs e)
        {
            if (m_Core.LogLoaded)
                OnLogfileLoaded();
        }

        private void messages_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex < m_VisibleMessages.Count)
                m_Core.SetEventID(null, 0, m_Messages[m_VisibleMessages[e.RowIndex]].EventID);
        }

        private bool m_SuppressRefresh = false;

        void RefreshMessageList()
        {
            if (m_SuppressRefresh) return;

            if (displayHidden.Checked)
            {
                messages.RowCount = 0;
                messages.RowCount = m_Messages.Count;
            }
            else
            {
                messages.RowCount = 0;
                messages.RowCount = m_VisibleMessages.Count;
            }
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

                renderdoc.DebugMessage msg = m_Messages[msgIdx].Message;

                bool hiderows = IsRowVisible(msgIdx);

                m_SuppressRefresh = true;

                foreach (var message in m_Messages)
                {
                    if (message.Message.category == msg.category &&
                        message.Message.severity == msg.severity &&
                        message.Message.messageID == msg.messageID)
                    {
                        if (hiderows)
                            HideRow(message.Index);
                        else
                            ShowRow(message.Index);
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
            if (msgIdx < 0 || msgIdx >= m_Messages.Count) return;

            if (e.ColumnIndex == 0) e.Value = m_Messages[msgIdx].EventID;
            if (e.ColumnIndex == 1) e.Value = m_Messages[msgIdx].Message.severity.ToString();
            if (e.ColumnIndex == 2) e.Value = m_Messages[msgIdx].Message.category.ToString();
            if (e.ColumnIndex == 3) e.Value = m_Messages[msgIdx].Message.messageID.ToString();
            if (e.ColumnIndex == 4) e.Value = m_Messages[msgIdx].Message.description;
        }

        private void messages_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
        {
            int msgIdx = GetMessageIndex(e.RowIndex);

            if (e.ColumnIndex < 0 || e.ColumnIndex >= messages.ColumnCount) return;
            if (msgIdx < 0 || msgIdx >= m_Messages.Count) return;

            if (!IsRowVisible(msgIdx)) e.CellStyle.BackColor = Color.Salmon;
        }

        private void DebugMessages_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }
    }
}
