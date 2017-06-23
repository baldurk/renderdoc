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
using System.Threading;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;
using System.IO;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;

using Process = System.Diagnostics.Process;
using System.Runtime.InteropServices;

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
        int m_CaptureNumFrames = 1;
        int m_CaptureFrameNum = 0;
        int m_CaptureCounter = 0;
        bool m_Disconnect = false;
        TargetControl m_Connection = null;

        uint m_CopyLogID = uint.MaxValue;
        string m_CopyLogLocalPath = "";
        List<uint> m_DeleteLogs = new List<uint>();

        bool m_IgnoreThreadClosed = false;

        string m_Host;
        UInt32 m_RemoteIdent;

        class CaptureLog
        {
            public uint remoteID;
            public string exe;
            public string api;
            public DateTime timestamp;

            public Image thumb;

            public bool saved;
            public bool opened;

            public string path;
            public bool local;
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

            if (SystemInformation.HighContrast)
                toolStrip1.Renderer = new ToolStripSystemRenderer();

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

            SetText("Connecting...");
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

        public string Hostname
        {
            get
            {
                return m_Host;
            }
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

                m_Connection = StaticExports.CreateTargetControl(m_Host, m_RemoteIdent, username, true);

                if (m_Connection.Connected)
                {
                    string api = "No API detected";
                    if (m_Connection.API.Length > 0) api = m_Connection.API;
                    this.BeginInvoke((MethodInvoker)delegate
                    {
                        if (m_Connection.PID == 0)
                        {
                            connectionStatus.Text = String.Format("Connection established to {0} ({1})", m_Connection.Target, api);
                            SetText(String.Format("{0}", m_Connection.Target));
                        }
                        else
                        {
                            connectionStatus.Text = String.Format("Connection established to {0} [PID {1}] ({2})",
                                     m_Connection.Target, m_Connection.PID, api);
                            SetText(String.Format("{0} [PID {1}]", m_Connection.Target, m_Connection.PID));
                        }
                        connectionIcon.Image = global::renderdocui.Properties.Resources.connect;
                    });
                }
                else
                {
                    throw new ReplayCreateException(ReplayCreateStatus.NetworkIOFailed);
                }

                while (m_Connection.Connected)
                {
                    m_Connection.ReceiveMessage();

                    if (m_TriggerCapture)
                    {
                        m_Connection.TriggerCapture((uint)m_CaptureNumFrames);
                        m_TriggerCapture = false;
                    }

                    if (m_QueueCapture)
                    {
                        m_Connection.QueueCapture((uint)m_CaptureFrameNum);
                        m_QueueCapture = false;
                        m_CaptureFrameNum = 0;
                    }

                    if (m_CopyLogLocalPath != "")
                    {
                        m_Connection.CopyCapture(m_CopyLogID, m_CopyLogLocalPath);
                        m_CopyLogLocalPath = "";
                        m_CopyLogID = uint.MaxValue;
                    }

                    List<uint> dels = new List<uint>();
                    lock (m_DeleteLogs)
                    {
                        dels.AddRange(m_DeleteLogs);
                        m_DeleteLogs.Clear();
                    }

                    foreach(var del in dels)
                        m_Connection.DeleteCapture(del);

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
                            if (m_Connection.PID == 0)
                            {
                                connectionStatus.Text = String.Format("Connection established to {0} ({1})", m_Connection.Target, m_Connection.API);
                                SetText(String.Format("{0}", m_Connection.Target));
                            }
                            else
                            {
                                connectionStatus.Text = String.Format("Connection established to {0} [PID {1}] ({2})",
                                         m_Connection.Target, m_Connection.PID, m_Connection.API);
                                SetText(String.Format("{0} [PID {1}]", m_Connection.Target, m_Connection.PID));
                            }
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
                        int thumbWidth = m_Connection.CaptureFile.thumbWidth;
                        int thumbHeight = m_Connection.CaptureFile.thumbHeight;
                        string path = m_Connection.CaptureFile.path;
                        bool local = m_Connection.CaptureFile.local;

                        this.BeginInvoke((MethodInvoker)delegate
                        {
                            CaptureAdded(capID, m_Connection.Target, m_Connection.API, thumb, thumbWidth, thumbHeight, timestamp, path, local);
                        });
                        m_Connection.CaptureExists = false;
                    }

                    if (m_Connection.CaptureCopied)
                    {
                        uint capID = m_Connection.CaptureFile.ID;
                        string path = m_Connection.CaptureFile.path;

                        this.BeginInvoke((MethodInvoker)delegate
                        {
                            CaptureCopied(capID, path);
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

                    numFrames.Enabled = captureDelay.Enabled = captureFrame.Enabled =
                        triggerCapture.Enabled = queueCap.Enabled = false;

                    ConnectionClosed();
                });
            }
            catch (ReplayCreateException)
            {
                this.BeginInvoke((MethodInvoker)delegate
                {
                    SetText("Connection failed");
                    connectionStatus.Text = "Connection failed";
                    connectionIcon.Image = global::renderdocui.Properties.Resources.delete;

                    ConnectionClosed();
                });
            }
        }

        Image MakeThumb(Size s, byte[] data, int thumbWidth, int thumbHeight)
        {
            Bitmap thumb = new Bitmap(s.Width, s.Height, PixelFormat.Format32bppArgb);

            Graphics g = Graphics.FromImage(thumb);

            g.Clear(Color.Transparent);
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;

            if (data != null && thumbWidth > 0 && thumbHeight > 0)
            {
                try
                {
                    using (var im = new Bitmap(thumbWidth, thumbHeight, PixelFormat.Format24bppRgb))
                    {
                        BitmapData bits = im.LockBits(new Rectangle(0, 0, thumbWidth, thumbHeight), ImageLockMode.WriteOnly, im.PixelFormat);

                        // need to endian-swap for .NET
                        for (int dy = 0; dy < bits.Height; dy++)
                        {
                            for (int dx = 0; dx < bits.Width; dx++)
                            {
                                int offs = dy * bits.Width * 3 + dx * 3;
                                byte r = data[offs + 0];
                                data[offs + 0] = data[offs + 2];
                                data[offs + 2] = r;
                            }
                        }

                        Marshal.Copy(data, 0, bits.Scan0, data.Length);

                        im.UnlockBits(bits);

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

            if(captures.SelectedItems.Count == 1)
            {
                CaptureLog cap = captures.SelectedItems[0].Tag as CaptureLog;

                newInstanceToolStripMenuItem.Enabled = cap.local;

                if (cap.thumb != null)
                {
                    preview.Image = cap.thumb;
                }
                else
                {
                    preview.Image = null;
                    preview.Size = new Size(16, 16);
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
            log.opened = true;

            if (!log.local &&
               (m_Core.Renderer.Remote == null ||
                m_Core.Renderer.Remote.Hostname != m_Host ||
                !m_Core.Renderer.Remote.Connected)
              )
            {
                MessageBox.Show(
                    String.Format("This capture is on remote host {0} and there is no active replay context on that host.\n" +
                        "You can either save the log locally, or switch to a replay context on {0}.\n\n", m_Host),
                    "No active replay context", MessageBoxButtons.OK);
                return;
            }

            m_Main.LoadLogfile(log.path, !log.saved, log.local);
        }

        private bool SaveCapture(CaptureLog log)
        {
            string path = m_Main.GetSavePath();

            // we copy the temp log to the desired path, but the log item remains referring to the temp path.
            // This ensures that if the user deletes the saved path we can still open or re-save it.
            if (path.Length > 0)
            {
                try
                {
                    if (log.local)
                    {
                        File.Copy(log.path, path, true);
                    }
                    else if (m_Connection.Connected)
                    {
                        // if we have a current live connection, prefer using it
                        m_CopyLogLocalPath = path;
                        m_CopyLogID = log.remoteID;
                    }
                    else
                    {
                        if (m_Core.Renderer.Remote == null ||
                            m_Core.Renderer.Remote.Hostname != m_Host ||
                            !m_Core.Renderer.Remote.Connected)
                        {
                            MessageBox.Show(
                                String.Format("This capture is on remote host {0} and there is no active replay context on that host.\n" +
                                "Without an active replay context the capture cannot be saved, try switching to a replay context on {0}.\n\n", m_Host),
                                "No active replay context", MessageBoxButtons.OK);
                            return false;
                        }

                        m_Core.Renderer.CopyCaptureFromRemote(log.path, path, this);
                        m_Core.Renderer.DeleteCapture(log.path, false);
                    }

                    log.saved = true;
                    log.path = path;
                    m_Core.Config.AddRecentFile(m_Core.Config.RecentLogFiles, path, 10);
                    m_Main.PopulateRecentFiles();
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save to " + path + Environment.NewLine + ex.ToString(), "Cannot save",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return true;
            }

            return false;
        }

        public bool CheckAllowClose()
        {
            m_IgnoreThreadClosed = true;

            bool suppressRemoteWarning = false;

            foreach (ListViewItem i in captures.Items)
            {
                var log = i.Tag as CaptureLog;

                if (log.saved) continue;

                captures.SelectedItems.Clear();
                Activate();
                captures.Focus();
                i.Selected = true;

                DialogResult res = DialogResult.No;

                if (!suppressRemoteWarning)
                {
                    res = MessageBox.Show(String.Format("Save this logfile from {0} at {1}?", log.exe, log.timestamp.ToString("T")),
                                                        "Unsaved log", MessageBoxButtons.YesNoCancel);
                }

                if (res == DialogResult.Cancel)
                {
                    m_IgnoreThreadClosed = false;
                    return false;
                }

                // we either have to save or delete the log. Make sure that if it's remote that we are able
                // to by having an active connection or replay context on that host.
                if (suppressRemoteWarning == false && !m_Connection.Connected && !log.local &&
                    (m_Core.Renderer.Remote == null ||
                     m_Core.Renderer.Remote.Hostname != m_Host ||
                     !m_Core.Renderer.Remote.Connected)
                    )
                {
                    DialogResult res2 = MessageBox.Show(
                        String.Format("This capture is on remote host {0} and there is no active replay context on that host.\n", m_Host) +
                        "Without an active replay context the capture cannot be " + (res == DialogResult.Yes ? "saved.\n\n" : "deleted.\n\n") +
                        "Would you like to continue and discard this capture and any others, to be left in the temporary folder on the remote machine?",
                        "No active replay context", MessageBoxButtons.YesNoCancel);

                    if (res2 == DialogResult.Yes)
                    {
                        suppressRemoteWarning = true;
                        res = DialogResult.No;
                    }
                    else
                    {
                        m_IgnoreThreadClosed = false;
                        return false;
                    }
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
                    try
                    {
                        if (log.path == m_Core.LogFileName)
                        {
                            m_Main.OwnTemporaryLog = true;
                        }
                        else
                        {
                            // if connected, prefer using the live connection
                            if (m_Connection.Connected && !log.local)
                            {
                                lock (m_DeleteLogs)
                                {
                                    m_DeleteLogs.Add(log.remoteID);
                                }
                            }
                            else
                            {
                                m_Core.Renderer.DeleteCapture(log.path, log.local);
                            }
                        }
                    }
                    catch (System.Exception)
                    {
                        // couldn't delete log - deleted from under us?
                    }
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

            CleanItems();
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

            CleanItems();
            KillThread();
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
                if (log.path == m_Core.LogFileName)
                {
                    m_Main.OwnTemporaryLog = true;
                    m_Main.CloseLogfile();
                }

                try
                {
                    // if connected, prefer using the live connection
                    if (m_Connection.Connected && !log.local)
                    {
                        lock (m_DeleteLogs)
                        {
                            m_DeleteLogs.Add(log.remoteID);
                        }
                    }
                    else
                    {
                        m_Core.Renderer.DeleteCapture(log.path, log.local);
                    }
                }
                catch (System.Exception)
                {
                    // couldn't delete log - deleted from under us?
                }
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

                if (!log.local)
                {
                    MessageBox.Show("Can't open log in new instance with remote server in use", "Cannot open new instance",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                try
                {
                    File.Copy(log.path, temppath);
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save log to temporary location" + Environment.NewLine + ex.ToString(), "Cannot save temporary log",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

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

        DateTime lastEdit = DateTime.MinValue;

        private void captures_AfterLabelEdit(object sender, LabelEditEventArgs e)
        {
            lastEdit = DateTime.Now;
        }

        private void captures_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Delete)
            {
                deleteCapture_Click(sender, null);
            }
            if (e.KeyCode == Keys.F2 && captures.SelectedItems.Count == 1)
            {
                captures.SelectedItems[0].BeginEdit();
            }
            if (e.KeyCode == Keys.Return || e.KeyCode == Keys.Enter)
            {
                // don't interpret the enter from ending an edit as an enter to open
                if(DateTime.Now.Subtract(lastEdit).TotalMilliseconds >= 500)
                    openCapture_Click(sender, null);
            }
        }

        private void CaptureCopied(uint ID, string localPath)
        {
            foreach (ListViewItem item in captures.Items)
            {
                var log = item.Tag as CaptureLog;

                if (log != null && log.remoteID == ID)
                {
                    log.local = true;
                    log.path = localPath;
                    item.SubItems[0].Text = log.exe;
                    item.SubItems[0].Font = new Font(item.SubItems[0].Font, FontStyle.Regular);
                }
            }
        }

        private void CaptureAdded(uint ID, string executable, string api, byte[] thumbnail, int thumbWidth, int thumbHeight, DateTime timestamp, string path, bool local)
        {
            if (thumbnail == null || thumbnail.Length == 0)
            {
                using (Image t = MakeThumb(thumbs.ImageSize, null, 0, 0))
                {
                    thumbs.Images.Add(t);
                }
            }
            else
            {
                using (Image t = MakeThumb(thumbs.ImageSize, thumbnail, thumbWidth, thumbHeight))
                {
                    thumbs.Images.Add(t);
                }
            }

            CaptureLog log = new CaptureLog();
            log.remoteID = ID;
            log.exe = executable;
            log.api = api;
            log.timestamp = timestamp;
            log.thumb = null;
            try
            {
                if (thumbnail != null && thumbnail.Length != 0)
                {
                    using (var ms = new MemoryStream(thumbnail))
                        log.thumb = Image.FromStream(ms);
                }
            }
            catch (ArgumentException)
            {
            }
            log.saved = false;
            log.path = path;
            log.local = local;

            string title = log.exe;
            if (!local)
                title += " (Remote)";

            var item = new ListViewItem(new string[] { title, log.api, log.timestamp.ToString() }, thumbs.Images.Count - 1);
            item.Tag = log;
            if(!local)
                item.SubItems[0].Font = new Font(item.SubItems[0].Font, FontStyle.Italic);

            captures.Items.Add(item);
        }

        private void ConnectionClosed()
        {
            if (m_IgnoreThreadClosed) return;

            if (captures.Items.Count <= 1)
            {
                if (captures.Items.Count == 1)
                {
                    var log = captures.Items[0].Tag as CaptureLog;

                    // only auto-open a non-local log if we are successfully connected
                    // to this machine as a remote context
                    if (!log.local)
                    {
                        if (m_Core.Renderer.Remote == null ||
                           m_Host != m_Core.Renderer.Remote.Hostname ||
                           !m_Core.Renderer.Remote.Connected)
                            return;
                    }

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
            m_CaptureNumFrames = (int)numFrames.Value;
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
            if (m_Children.Count > 0)
            {
                Process[] processes = Process.GetProcesses();

                // remove any stale processes
                for (int i = 0; i < m_Children.Count; i++)
                {
                    bool found = false;

                    foreach (var p in processes)
                    {
                        if (p.Id == m_Children[i].PID)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
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

        private void SetText(String title)
        {
            Text = (m_Host.Length > 0 ? (m_Host + " - ") : "") + title;
        }

        private void previewToggle_CheckedChanged(object sender, EventArgs e)
        {
            previewSplit.Panel2Collapsed = !previewToggle.Checked;
        }

        private Point previewDragStart = Point.Empty;

        private void preview_MouseDown(object sender, MouseEventArgs e)
        {
            Point mouse = preview.PointToScreen(e.Location);
            if (e.Button == MouseButtons.Left)
            {
                previewDragStart = mouse;
                preview.Cursor = Cursors.NoMove2D;
            }
        }

        private void preview_MouseMove(object sender, MouseEventArgs e)
        {
            Point mouse = preview.PointToScreen(e.Location);
            if (e.Button == MouseButtons.Left)
            {
                SuspendLayout();
                Point p = previewSplit.Panel2.AutoScrollPosition;
                previewSplit.Panel2.AutoScrollPosition = new Point(-(p.X + mouse.X - previewDragStart.X),
                                                                   -(p.Y + mouse.Y - previewDragStart.Y));
                previewDragStart = mouse;
                ResumeLayout();
            }
            else
            {
                preview.Cursor = Cursors.Default;
            }
        }
    }
}
