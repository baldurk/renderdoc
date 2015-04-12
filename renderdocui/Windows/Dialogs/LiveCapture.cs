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
using System.Threading;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;
using System.IO;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;

using Process = System.Diagnostics.Process;

namespace renderdocui.Windows
{
    public partial class LiveCapture : DockContent
    {
        MainWindow m_Main = null;
        ImageList thumbs = null;

        Core m_Core = null;

        Thread m_ConnectThread = null;
        bool m_TriggerCapture = false;
        bool m_QueueCapture = false;
        int m_CaptureFrameNum = 0;
        int m_CaptureCounter = 0;
        bool m_Disconnect = false;
        RemoteAccess m_Connection = null;

        bool m_IgnoreThreadClosed = false;

        string m_Host;
        UInt32 m_RemoteIdent;

        class CaptureLog
        {
            public uint remoteID;
            public string exe;
            public string api;
            public DateTime timestamp;

            public bool copying;
            public bool saved;
            public bool opened;

            public string localpath;
        };

        class ChildProcess
        {
            public int PID;
            public uint ident;
            public string name;
            public bool added = false;
        };

        List<ChildProcess> m_Children = new List<ChildProcess>();

        public LiveCapture(Core core, string host, UInt32 remoteIdent, MainWindow main)
        {
            InitializeComponent();

            m_Core = core;
            m_Main = main;

            this.DoubleBuffered = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Connection = null;
            m_Host = host;
            m_RemoteIdent = remoteIdent;

            childProcessLabel.Visible = false;
            childProcesses.Visible = false;

            m_ConnectThread = null;

            Text = (m_Host.Length > 0 ? (m_Host + " - ") : "") + "Connecting...";
            connectionStatus.Text = "Connecting...";
            connectionIcon.Image = global::renderdocui.Properties.Resources.hourglass;

            thumbs = new ImageList();
            thumbs.ColorDepth = ColorDepth.Depth24Bit;

            thumbs.ImageSize = new Size(256, 144);
            captures.TileSize = new Size(400, 160);
            captures.LargeImageList = thumbs;

            captures.Columns.AddRange(new ColumnHeader[] { new ColumnHeader(), new ColumnHeader(), new ColumnHeader() });
        }

        public void QueueCapture(int frameNum)
        {
            m_CaptureFrameNum = frameNum;
            m_QueueCapture = true;
        }

        private void LiveCapture_Shown(object sender, EventArgs e)
        {
            m_ConnectThread = Helpers.NewThread(new ThreadStart(ConnectionThreadEntry));
            m_ConnectThread.Start();
        }

        private void ConnectionThreadEntry()
        {
            try
            {
                string username = System.Security.Principal.WindowsIdentity.GetCurrent().Name;

                m_Connection = StaticExports.CreateRemoteAccessConnection(m_Host, m_RemoteIdent, username, true);

                if (m_Connection.Connected)
                {
                    string api = "...";
                    if (m_Connection.API.Length > 0) api = m_Connection.API;
                    this.BeginInvoke((MethodInvoker)delegate
                    {
                        if (m_Connection.PID == 0)
                        {
                            connectionStatus.Text = String.Format("Connection established to {0} ({1})", m_Connection.Target, api);
                            Text = String.Format("{0} ({1})", m_Connection.Target, api);
                        }
                        else
                        {
                            connectionStatus.Text = String.Format("Connection established to {0} [PID {1}] ({2})",
                                     m_Connection.Target, m_Connection.PID, api);
                            Text = String.Format("{0} [PID {1}] ({2})", m_Connection.Target, m_Connection.PID, api);
                        }
                        connectionIcon.Image = global::renderdocui.Properties.Resources.connect;
                    });
                }
                else
                {
                    throw new ApplicationException();
                }

                while (m_Connection.Connected)
                {
                    m_Connection.ReceiveMessage();

                    if (m_TriggerCapture)
                    {
                        m_Connection.TriggerCapture();
                        m_TriggerCapture = false;
                    }

                    if (m_QueueCapture)
                    {
                        m_Connection.QueueCapture((uint)m_CaptureFrameNum);
                        m_QueueCapture = false;
                        m_CaptureFrameNum = 0;
                    }

                    if (m_Disconnect)
                    {
                        m_Connection.Shutdown();
                        m_Connection = null;
                        return;
                    }

                    if (m_Connection.InfoUpdated)
                    {
                        this.BeginInvoke((MethodInvoker)delegate
                        {
                            connectionStatus.Text = String.Format("Connection established to {0} ({1})", m_Connection.Target, m_Connection.API);
                            Text = String.Format("{0} ({1})", m_Connection.Target, m_Connection.API);
                            connectionIcon.Image = global::renderdocui.Properties.Resources.connect;
                        });

                        m_Connection.InfoUpdated = false;
                    }

                    if (m_Connection.CaptureExists)
                    {
                        uint capID = m_Connection.CaptureFile.ID;
                        DateTime timestamp = new DateTime(1970, 1, 1, 0, 0, 0);
                        timestamp = timestamp.AddSeconds(m_Connection.CaptureFile.timestamp).ToLocalTime();
                        byte[] thumb = m_Connection.CaptureFile.thumbnail;
                        string path = m_Connection.CaptureFile.localpath;

                        if (path.Length == 0 || File.Exists(path))
                        {
                            this.BeginInvoke((MethodInvoker)delegate
                            {
                                CaptureAdded(capID, m_Connection.Target, m_Connection.API, thumb, timestamp);
                                if (path.Length > 0)
                                    CaptureRetrieved(capID, path);
                            });
                            m_Connection.CaptureExists = false;

                            if (path.Length == 0)
                                m_Connection.CopyCapture(capID, m_Core.TempLogFilename("remotecopy_" + m_Connection.Target));
                        }
                    }

                    if (m_Connection.CaptureCopied)
                    {
                        uint capID = m_Connection.CaptureFile.ID;
                        string path = m_Connection.CaptureFile.localpath;

                        this.BeginInvoke((MethodInvoker)delegate
                        {
                            CaptureRetrieved(capID, path);
                        });
                        m_Connection.CaptureCopied = false;
                    }

                    if (m_Connection.ChildAdded)
                    {
                        if (m_Connection.NewChild.PID != 0)
                        {
                            try
                            {
                                ChildProcess c = new ChildProcess();
                                c.PID = (int)m_Connection.NewChild.PID;
                                c.ident = m_Connection.NewChild.ident;
                                c.name = Process.GetProcessById((int)m_Connection.NewChild.PID).ProcessName;

                                lock (m_Children)
                                {
                                    m_Children.Add(c);
                                }
                            }
                            catch (Exception)
                            {
                                // process expired/doesn't exist anymore
                            }
                        }

                        m_Connection.ChildAdded = false;
                    }
                }

                this.BeginInvoke((MethodInvoker)delegate
                {
                    connectionStatus.Text = "Connection closed";
                    connectionIcon.Image = global::renderdocui.Properties.Resources.disconnect;

                    ConnectionClosed();
                });
            }
            catch (ApplicationException)
            {
                this.BeginInvoke((MethodInvoker)delegate
                {
                    Text = (m_Host.Length > 0 ? (m_Host + " - ") : "") + "Connection failed";
                    connectionStatus.Text = "Connection failed";
                    connectionIcon.Image = global::renderdocui.Properties.Resources.delete;

                    ConnectionClosed();
                });
            }
        }

        Image MakeThumb(Size s, Stream st)
        {
            Bitmap thumb = new Bitmap(s.Width, s.Height, PixelFormat.Format32bppArgb);

            Graphics g = Graphics.FromImage(thumb);

            g.Clear(Color.Transparent);
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;

            if (st != null)
            {
                try
                {
                    using (var im = Image.FromStream(st))
                    {
                        float x = 0, y = 0;
                        float width = 0, height = 0;

                        float srcaspect = (float)im.Width / (float)im.Height;
                        float dstaspect = (float)s.Width / (float)s.Height;

                        if (srcaspect > dstaspect)
                        {
                            width = s.Width;
                            height = width / srcaspect;

                            y = (s.Height - height) / 2;
                        }
                        else
                        {
                            height = s.Height;
                            width = height * srcaspect;

                            x = (s.Width - width) / 2;
                        }

                        g.DrawImage(im, x, y, width, height);
                    }
                }
                catch (ArgumentException)
                {
                    // swallow - invalid thumbnail just allow it to be transparent
                }
            }

            g.Dispose();

            return thumb;
        }

        private void captures_ItemSelectionChanged(object sender, ListViewItemSelectionChangedEventArgs e)
        {
            deleteMenu.Enabled = (captures.SelectedItems.Count > 0);
            saveMenu.Enabled = saveThisCaptureToolStripMenuItem.Enabled =
                openMenu.Enabled = openThisCaptureToolStripMenuItem.Enabled =
                (captures.SelectedItems.Count == 1);

            if (captures.SelectedItems.Count > 0)
            {
                foreach(ListViewItem i in captures.SelectedItems)
                {
                    var log = i.Tag as CaptureLog;

                    if (log.copying)
                    {
                        deleteMenu.Enabled =
                            saveMenu.Enabled = saveThisCaptureToolStripMenuItem.Enabled =
                            openMenu.Enabled = openThisCaptureToolStripMenuItem.Enabled = false;
                        return;
                    }
                }
            }
        }

        private void captures_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (captures.SelectedItems.Count == 1)
                OpenCapture(captures.SelectedItems[0].Tag as CaptureLog);
        }

        private void captures_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right && captures.SelectedItems.Count > 0)
                rightclickContext.Show(captures.PointToScreen(e.Location));
        }

        private void OpenCapture(CaptureLog log)
        {
            if (!log.copying)
            {
                m_Main.LoadLogfile(log.localpath, !log.saved);
                log.opened = true;
            }
        }

        private bool SaveCapture(CaptureLog log)
        {
            if (log.copying) return false;

            string path = m_Main.GetSavePath();

            // we copy the temp log to the desired path, but the log item remains referring to the temp path.
            // This ensures that if the user deletes the saved path we can still open or re-save it.
            if (path.Length > 0)
            {
                File.Copy(log.localpath, path, true);
                return true;
            }

            return false;
        }

        public bool CheckAllowClose()
        {
            m_IgnoreThreadClosed = true;

            foreach (ListViewItem i in captures.Items)
            {
                var log = i.Tag as CaptureLog;

                if (log.saved) continue;

                captures.SelectedItems.Clear();
                Activate();
                captures.Focus();
                i.Selected = true;

                DialogResult res = MessageBox.Show(String.Format("Save this logfile from {0} at {1}?", log.exe, log.timestamp.ToString("T")),
                                                    "Unsaved log", MessageBoxButtons.YesNoCancel);

                if (res == DialogResult.Cancel)
                {
                    m_IgnoreThreadClosed = false;
                    return false;
                }

                if (res == DialogResult.Yes)
                {
                    bool success = SaveCapture(log);

                    if (!success)
                    {
                        m_IgnoreThreadClosed = false;
                        return false;
                    }
                }
            }

            m_IgnoreThreadClosed = false;
            return true;
        }

        public void CleanItems()
        {
            foreach (ListViewItem i in captures.Items)
            {
                var log = i.Tag as CaptureLog;

                if (!log.saved)
                {
                    if (log.localpath == m_Core.LogFileName)
                        m_Main.OwnTemporaryLog = true;
                    else
                        File.Delete(log.localpath);
                }
            }
            captures.Items.Clear();
        }

        private void LiveCapture_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (CheckAllowClose() == false)
            {
                e.Cancel = true;
                return;
            }

            KillThread();
        }

        private void KillThread()
        {
            if (m_ConnectThread.ThreadState != ThreadState.Aborted &&
                m_ConnectThread.ThreadState != ThreadState.Stopped)
            {
                m_Disconnect = true;
                m_ConnectThread.Join();
            }
        }

        private void LiveCapture_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Main.LiveCaptureClosed(this);

            captures.LargeImageList.Images.Clear();
            thumbs.Dispose();

            KillThread();
            CleanItems();
        }

        private bool CheckAllowDelete()
        {
            bool needcheck = false;

            bool multiple = captures.SelectedItems.Count > 1;

            foreach (ListViewItem i in captures.SelectedItems)
                needcheck |= !(i.Tag as CaptureLog).saved;

            if (!needcheck || captures.SelectedItems.Count == 0)
                return true;

            Focus();

            DialogResult res = MessageBox.Show(String.Format("Are you sure you wish to delete {0}?", multiple ? "these logs" : "this log") +
                                               "\nAny log currently opened will be closed.",
                                               multiple ? "Unsaved logs" : "Unsaved log", MessageBoxButtons.YesNoCancel);

            if (res == DialogResult.Cancel || res == DialogResult.No)
                return false;

            if (res == DialogResult.Yes)
                return true;

            return true;
        }

        private void DeleteCaptureUnprompted(ListViewItem item)
        {
            var log = item.Tag as CaptureLog;

            if (!log.saved)
            {
                if (log.localpath == m_Core.LogFileName)
                {
                    m_Main.OwnTemporaryLog = false;
                    m_Main.CloseLogfile();
                }

                File.Delete(log.localpath);
            }

            captures.Items.Remove(item);
        }

        private void openCapture_Click(object sender, EventArgs e)
        {
            if (captures.SelectedItems.Count == 1)
                OpenCapture(captures.SelectedItems[0].Tag as CaptureLog);
        }

        private void openNewWindow_Click(object sender, EventArgs e)
        {
            if (captures.SelectedItems.Count == 1)
            {
                var log = captures.SelectedItems[0].Tag as CaptureLog;

                var temppath = m_Core.TempLogFilename(log.exe);

                File.Copy(log.localpath, temppath);

                var process = new System.Diagnostics.Process();
                process.StartInfo = new System.Diagnostics.ProcessStartInfo(Application.ExecutablePath, String.Format("--tempfile \"{0}\"", temppath));
                process.Start();
            }
        }

        private void saveCapture_Click(object sender, EventArgs e)
        {
            if (captures.SelectedItems.Count == 1)
                SaveCapture(captures.SelectedItems[0].Tag as CaptureLog);
        }

        private void deleteCapture_Click(object sender, EventArgs e)
        {
            bool allow = CheckAllowDelete();

            if (!allow) return;

            foreach (ListViewItem i in captures.SelectedItems)
                DeleteCaptureUnprompted(i);
        }

        private void captures_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Delete)
            {
                deleteCapture_Click(sender, null);
            }
        }

        private void CaptureAdded(uint ID, string executable, string api, byte[] thumbnail, DateTime timestamp)
        {
            if (thumbnail == null || thumbnail.Length == 0)
            {
                using (Image t = MakeThumb(thumbs.ImageSize, null))
                {
                    thumbs.Images.Add(t);
                }
            }
            else
            {
                using (var ms = new MemoryStream(thumbnail))
                using (Image t = MakeThumb(thumbs.ImageSize, ms))
                {
                    thumbs.Images.Add(t);
                }
            }

            CaptureLog log = new CaptureLog();
            log.remoteID = ID;
            log.exe = executable;
            log.api = api;
            log.timestamp = timestamp;
            log.copying = true;
            log.saved = false;

            var item = new ListViewItem(new string[] { log.exe + " (Copying)", log.api, log.timestamp.ToString() }, thumbs.Images.Count - 1);
            item.Tag = log;
            item.SubItems[0].Font = new Font(item.SubItems[0].Font, FontStyle.Italic);

            captures.Items.Add(item);
        }

        private void CaptureRetrieved(uint ID, string localpath)
        {
            foreach (ListViewItem i in captures.Items)
            {
                var log = i.Tag as CaptureLog;

                if (log.remoteID == ID && log.copying)
                {
                    log.copying = false;
                    log.localpath = localpath;
                    i.SubItems[0].Text = log.exe;
                    i.SubItems[0].Font = new Font(i.SubItems[0].Font, FontStyle.Regular);
                }
            }
        }

        private void ConnectionClosed()
        {
            if (m_IgnoreThreadClosed) return;

            if (captures.Items.Count <= 1)
            {
                if (captures.Items.Count == 1)
                {
                    var log = captures.Items[0].Tag as CaptureLog;

                    if (log.opened)
                        return;

                    OpenCapture(log);
                    if (!log.saved)
                    {
                        log.saved = true;
                        m_Main.OwnTemporaryLog = true;
                    }
                }

                // auto-close and load log if we got a capture. If we
                // don't haveany captures but DO have child processes,
                // then don't close just yet.
                if(captures.Items.Count == 1 || m_Children.Count == 0)
                    Close();

                // if we have no captures and only one child, close and
                // open up a connection to it (similar to behaviour with
                // only one capture
                if (captures.Items.Count == 0 && m_Children.Count == 1)
                {
                    uint ident = m_Children[0].ident;
                    var live = new LiveCapture(m_Core, m_Host, ident, m_Main);
                    m_Main.ShowLiveCapture(live);
                    Close();
                }
            }
        }

        private void triggerCapture_Click(object sender, EventArgs e)
        {
            if (captureDelay.Value == 0)
            {
                m_TriggerCapture = true;
            }
            else
            {
                m_CaptureCounter = (int)captureDelay.Value;
                captureCountdown.Enabled = true;
                triggerCapture.Enabled = false;
                triggerCapture.Text = String.Format("Triggering in {0}s", m_CaptureCounter);
            }
        }

        private void queueCap_Click(object sender, EventArgs e)
        {
            m_CaptureFrameNum = (int)captureFrame.Value;
            m_QueueCapture = true;
        }

        private void captureCountdown_Tick(object sender, EventArgs e)
        {
            m_CaptureCounter--;

            if (m_CaptureCounter == 0)
            {
                m_TriggerCapture = true;
                captureCountdown.Enabled = false;
                triggerCapture.Enabled = true;
                triggerCapture.Text = "Trigger Capture";
            }
            else
            {
                triggerCapture.Text = String.Format("Triggering in {0}s", m_CaptureCounter);
            }
        }

        private void childUpdateTimer_Tick(object sender, EventArgs e)
        {
            // remove any stale processes
            for (int i = 0; i < m_Children.Count; i++)
            {
                try
                {
                    // if this throws an exception the process no longer exists so we'll remove it
                    Process.GetProcessById(m_Children[i].PID);
                }
                catch (Exception)
                {
                    if (m_Children[i].added)
                        childProcesses.Items.RemoveByKey(m_Children[i].PID.ToString());

                    // process expired/doesn't exist anymore
                    m_Children.RemoveAt(i);

                    // don't increment i, check the next element at i (if we weren't at the end
                    i--;
                }
            }

            for (int i = 0; i < m_Children.Count; i++)
            {
                if (!m_Children[i].added)
                {
                    string text = String.Format("{0} [PID {1}]", m_Children[i].name, m_Children[i].PID);

                    m_Children[i].added = true;
                    childProcesses.Items.Add(m_Children[i].PID.ToString(), text, 0).Tag = m_Children[i].ident;
                }
            }

            if (m_Children.Count > 0)
            {
                childProcessLabel.Visible = childProcesses.Visible = true;
            }
            else
            {
                childProcessLabel.Visible = childProcesses.Visible = false;
            }
        }

        private void childProcesses_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            if (childProcesses.SelectedItems.Count == 1 && childProcesses.SelectedItems[0].Tag is uint)
            {
                uint ident = (uint)childProcesses.SelectedItems[0].Tag;
                var live = new LiveCapture(m_Core, m_Host, ident, m_Main);
                m_Main.ShowLiveCapture(live);
            }                
        }
    }
}
