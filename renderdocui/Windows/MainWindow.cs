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
using System.IO;
using System.Diagnostics;
using System.Net;
using System.Text;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows
{
    public partial class MainWindow : Form, ILogViewerForm, ILogLoadProgressListener, IMessageFilter
    {
        public bool PreFilterMessage(ref Message m)
        {
            if (m.Msg == (int)Win32PInvoke.Win32Message.WM_MOUSEWHEEL)
            {
                int pos = m.LParam.ToInt32();
                short x = (short)((pos >> 0) & 0xffff);
                short y = (short)((pos >> 16) & 0xffff);

                Win32PInvoke.POINT pt = new Win32PInvoke.POINT((int)x, (int)y);

                IntPtr wnd = Win32PInvoke.WindowFromPoint(pt);

                if (wnd != IntPtr.Zero && wnd != m.HWnd && Control.FromHandle(wnd) != null)
                {
                    Win32PInvoke.SendMessage(wnd, m.Msg, m.WParam, m.LParam);
                    return true;
                }

                return false;
            }
            return false;
        }

        private Core m_Core;
        private string m_InitFilename;
        private string m_InitRemoteHost;
        private uint m_InitRemoteIdent;

        private List<LiveCapture> m_LiveCaptures = new List<LiveCapture>();

        private RemoteHost m_SelectedHost = null;

        private string InformationalVersion
        {
            get
            {
                var assembly = Assembly.GetExecutingAssembly();
                var attrs = assembly.GetCustomAttributes(typeof(AssemblyInformationalVersionAttribute), false);

                if (attrs != null && attrs.Length > 0)
                {
                    AssemblyInformationalVersionAttribute attribute = (AssemblyInformationalVersionAttribute)attrs[0];

                    if (attribute != null)
                        return attribute.InformationalVersion;
                }

                return "";
            }
        }

        private string GitCommitHash
        {
            get
            {
                return InformationalVersion;
            }
        }

        private bool OfficialVersion { get { return /*RENDERDOC_OFFICIAL_BUILD*/false; } }

        private string BareVersionString
        {
            get
            {
                return Assembly.GetEntryAssembly().GetName().Version.ToString(2);
            }
        }

        private string VersionString
        {
            get
            {
                return "v" + Assembly.GetEntryAssembly().GetName().Version.ToString(2);
            }
        }

        public MainWindow(Core core, string initFilename, string remoteHost, uint remoteIdent, bool temp)
        {
            InitializeComponent();

            if (SystemInformation.HighContrast)
                dockPanel.Skin = Helpers.MakeHighContrastDockPanelSkin();

            Icon = global::renderdocui.Properties.Resources.icon;

            renderdocplugin.PluginHelpers.GetPlugins();

            statusIcon.Text = "";
            statusIcon.Image = null;
            statusText.Text = "";

            SetTitle();

            Application.AddMessageFilter(this);

            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);

            m_Core = core;
            m_InitFilename = initFilename;
            m_InitRemoteHost = remoteHost;
            m_InitRemoteIdent = remoteIdent;
            OwnTemporaryLog = temp;

            resolveSymbolsToolStripMenuItem.Enabled = false;
            resolveSymbolsToolStripMenuItem.Text = "Resolve Symbols";

            m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

            m_Core.AddLogViewer(this);
            m_Core.AddLogProgressListener(this);

            m_MessageTick = new System.Threading.Timer(MessageCheck, this as object, 500, 500);
            m_RemoteProbe = new System.Threading.Timer(RemoteProbe, this as object, 7500, 7500);
        }

        private void MainWindow_Load(object sender, EventArgs e)
        {
            bool loaded = LoadLayout(0);

            if (Win32PInvoke.GetModuleHandle("rdocself.dll") != IntPtr.Zero)
            {
                ToolStripMenuItem beginSelfCap = new ToolStripMenuItem();
                beginSelfCap.Text = "Start Self-hosted Capture";
                beginSelfCap.Click += new EventHandler((object o, EventArgs a) => { StaticExports.StartSelfHostCapture("rdocself.dll"); });

                ToolStripMenuItem endSelfCap = new ToolStripMenuItem();
                endSelfCap.Text = "End Self-hosted Capture";
                endSelfCap.Click += new EventHandler((object o, EventArgs a) => { StaticExports.EndSelfHostCapture("rdocself.dll"); });

                toolsToolStripMenuItem.DropDownItems.AddRange(new ToolStripItem[] {
                    new System.Windows.Forms.ToolStripSeparator(),
                    beginSelfCap,
                    endSelfCap,
                });
            }

            CheckUpdates();

            Thread remoteStatusThread = Helpers.NewThread(new ThreadStart(() =>
            {
                m_Core.Config.AddAndroidHosts();
                for (int i = 0; i < m_Core.Config.RemoteHosts.Count; i++)
                    m_Core.Config.RemoteHosts[i].CheckStatus();
            }));
            remoteStatusThread.Start();

            sendErrorReportToolStripMenuItem.Enabled = OfficialVersion;

            // create default layout if layout failed to load
            if (!loaded)
            {
                m_Core.GetAPIInspector().Show(dockPanel);
                m_Core.GetEventBrowser().Show(m_Core.GetAPIInspector().Pane, DockAlignment.Top, 0.5);

                m_Core.GetPipelineStateViewer().Show(dockPanel);

                var bv = m_Core.GetMeshViewer();
                bv.InitFromPersistString("");
                bv.Show(dockPanel);

                var tv = m_Core.GetTextureViewer();
                tv.InitFromPersistString("");
                tv.Show(dockPanel);

                m_Core.GetTimelineBar().Show(dockPanel);

                if (m_Core.CaptureDialog == null)
                    m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

                m_Core.CaptureDialog.InjectMode = false;
                m_Core.CaptureDialog.Show(dockPanel);
            }

            PopulateRecentFiles();
            PopulateRecentCaptures();

            if (m_InitRemoteIdent != 0)
            {
                var live = new LiveCapture(m_Core, m_InitRemoteHost, m_InitRemoteIdent, this);
                ShowLiveCapture(live);
            }

            if (m_InitFilename.Length > 0)
            {
                LoadFromFilename(m_InitFilename);

                m_InitFilename = "";
            }
        }

        #region ILogLoadProgressListener

        public void LogfileProgressBegin()
        {
            BeginInvoke(new Action(() =>
            {
                statusProgress.Visible = true;
            }));
        }

        public void LogfileProgress(float f)
        {
            BeginInvoke(new Action(() =>
            {
                if (statusProgress.Visible)
                {
                    if (f <= 0.0f || f >= 0.999f)
                    {
                        statusProgress.Visible = false;
                        statusText.Text = "";
                        statusIcon.Image = null;
                    }
                    else
                    {
                        statusProgress.Value = (int)(statusProgress.Maximum * f);
                    }
                }
            }));
        }

        #endregion

        #region ILogViewerForm

        public void OnLogfileClosed()
        {
            contextChooser.Enabled = true;

            statusText.Text = "";
            statusIcon.Image = null;
            statusProgress.Visible = false;

            resolveSymbolsToolStripMenuItem.Enabled = false;
            resolveSymbolsToolStripMenuItem.Text = "Resolve Symbols";

            SetTitle();

            // if the remote sever disconnected during log replay, resort back to a 'disconnected' state
            if (m_Core.Renderer.Remote != null && !m_Core.Renderer.Remote.ServerRunning)
            {
                statusText.Text = "Remote server disconnected. To attempt to reconnect please select it again.";
                contextChooser.Text = "Replay Context: Local";
                m_Core.Renderer.DisconnectFromRemoteServer();
            }
        }

        private static void RemoteProbe(object m)
        {
            if (!(m is MainWindow)) return;

            var me = (MainWindow)m;

            if (!me.Created || me.IsDisposed)
                return;

            // perform a probe of known remote hosts to see if they're running or not
            if (!me.m_Core.LogLoading && !me.m_Core.LogLoaded)
            {
                foreach (var host in me.m_Core.Config.RemoteHosts.ToArray())
                {
                    // don't mess with a host we're connected to - this is handled anyway
                    if (host.Connected)
                        continue;

                    host.CheckStatus();
                }
            }
        }

        private static void MessageCheck(object m)
        {
            if (!(m is MainWindow)) return;

            var me = (MainWindow)m;

            if (me.m_Core.LogLoaded)
            {
                me.m_Core.Renderer.BeginInvoke((ReplayRenderer r) =>
                {
                    DebugMessage[] msgs = r.GetDebugMessages();

                    bool disconnected = false;

                    if(me.m_Core.Renderer.Remote != null)
                    {
                        bool prev = me.m_Core.Renderer.Remote.ServerRunning;

                        me.m_Core.Renderer.PingRemote();

                        if(prev != me.m_Core.Renderer.Remote.ServerRunning)
                            disconnected = true;
                    }

                    me.BeginInvoke(new Action(() =>
                    {
                        // if we just got disconnected while replaying a log, alert the user.
                        if (disconnected)
                        {
                            MessageBox.Show("Remote server disconnected during replaying of this capture.\n" +
                                "The replay will now be non-functional. To restore you will have to close the capture, allow " +
                                "RenderDoc to reconnect and load the capture again",
                                "Remote server disconnected",
                                MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }

                        if (me.m_Core.Renderer.Remote != null && !me.m_Core.Renderer.Remote.ServerRunning)
                            me.contextChooser.Image = global::renderdocui.Properties.Resources.cross;

                        if (msgs.Length > 0)
                        {
                            me.m_Core.AddMessages(msgs);
                            me.m_Core.GetDebugMessages().RefreshMessageList();
                        }

                        if (me.m_Core.UnreadMessageCount > 0)
                        {
                            me.m_MessageAlternate = !me.m_MessageAlternate;
                        }
                        else
                        {
                            me.m_MessageAlternate = false;
                        }

                        me.LogHasErrors = (me.m_Core.DebugMessages.Count > 0);
                    }));
                });
            }
            else if(!me.m_Core.LogLoaded && !me.m_Core.LogLoading)
            {
                if (me.m_Core.Renderer.Remote != null)
                    me.m_Core.Renderer.PingRemote();

                if(!me.Created || me.IsDisposed)
                    return;

                me.BeginInvoke(new Action(() =>
                {
                    if (!me.Created || me.IsDisposed)
                        return;

                    if (me.m_Core.Renderer.Remote != null && !me.m_Core.Renderer.Remote.ServerRunning)
                    {
                        me.contextChooser.Image = global::renderdocui.Properties.Resources.cross;
                        me.contextChooser.Text = "Replay Context: Local";
                        me.statusText.Text = "Remote server disconnected. To attempt to reconnect please select it again.";

                        me.m_Core.Renderer.DisconnectFromRemoteServer();
                    }
                }));
            }
        }

        private System.Threading.Timer m_MessageTick = null;
        private System.Threading.Timer m_RemoteProbe = null;
        private bool m_MessageAlternate = false;

        private bool LogHasErrors
        {
            set
            {
                if (value == true)
                {
                    statusIcon.Image = m_MessageAlternate
                        ? null
                        : global::renderdocui.Properties.Resources.delete;
                    statusText.Text = String.Format("{0} loaded. Log has {1} errors, warnings or performance notes. " +
                        "See the 'Log Errors and Warnings' window.", Path.GetFileName(m_Core.LogFileName), m_Core.DebugMessages.Count);
                    if (m_Core.UnreadMessageCount > 0)
                    {
                        statusText.Text += String.Format(" {0} Unread.", m_Core.UnreadMessageCount);
                    }
                }
                else
                {
                    statusIcon.Image = global::renderdocui.Properties.Resources.tick;
                    statusText.Text = String.Format("{0} loaded. No problems detected.", Path.GetFileName(m_Core.LogFileName));
                }
            }
        }

        private void status_DoubleClick(object sender, EventArgs e)
        {
            m_Core.GetDebugMessages().Show(dockPanel);
        }

        public void OnLogfileLoaded()
        {
            // don't allow changing context while log is open
            contextChooser.Enabled = false;

            LogHasErrors = (m_Core.DebugMessages.Count > 0);

            statusProgress.Visible = false;

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) => {
                bool hasResolver = r.HasCallstacks();

                this.BeginInvoke(new Action(() =>
                {
                    resolveSymbolsToolStripMenuItem.Enabled = hasResolver;
                    resolveSymbolsToolStripMenuItem.Text = hasResolver ? "Resolve Symbols" : "Resolve Symbols - None in log";
                }));
            });

            saveLogToolStripMenuItem.Enabled = true;

            SetTitle();

            PopulateRecentFiles();

            m_Core.GetEventBrowser().Focus();
        }

        public void OnEventSelected(UInt32 eventID)
        {
        }

        #endregion

        #region Layout & Dock Container

        private void LoadCustomString(string persistString)
        {
            string[] parsedStrings = persistString.Split(new char[] { ',' });
            if (parsedStrings.Length == 6 && parsedStrings[0] == "WinSize")
            {
                bool maximised = Convert.ToBoolean(parsedStrings[5]);

                Point location = new Point(Convert.ToInt32(parsedStrings[1]), Convert.ToInt32(parsedStrings[2]));

                Rectangle bounds = Screen.FromPoint(location).Bounds;

                if (location.X <= bounds.Left)
                    location.X = bounds.Left + 100;
                if (location.X >= bounds.Right)
                    location.X = bounds.Right - 100;

                if (location.Y <= bounds.Top)
                    location.Y = bounds.Top + 100;
                if (location.Y >= bounds.Bottom)
                    location.Y = bounds.Bottom - 100;

                Size winsize = new Size(Convert.ToInt32(parsedStrings[3]), Convert.ToInt32(parsedStrings[4]));

                winsize.Width = Math.Max(200, winsize.Width);
                winsize.Height = Math.Max(200, winsize.Height);

                SetBounds(location.X, location.Y, winsize.Width, winsize.Height);

                var desired = FormWindowState.Normal;

                if (maximised)
                    desired = FormWindowState.Maximized;
                
                if(WindowState != desired)
                    WindowState = desired;
            }
        }

        private string SaveCustomString()
        {
            var r = this.WindowState == FormWindowState.Maximized ? RestoreBounds : Bounds;

            return "WinSize," + r.X + "," + r.Y + "," + r.Width + "," + r.Height + "," + (this.WindowState == FormWindowState.Maximized);
        }

        private bool IsPersist(string persiststring, string typestring)
        {
            if (persiststring.Length < typestring.Length) return false;
            return persiststring.Substring(0, typestring.Length) == typestring;
        }

        private IDockContent GetContentFromPersistString(string persistString)
        {
            if (IsPersist(persistString, typeof(EventBrowser).ToString()))
            {
                var ret = m_Core.GetEventBrowser();
                ret.InitFromPersistString(persistString);
                return ret;
            }
            else if (IsPersist(persistString, typeof(TextureViewer).ToString()))
            {
                var ret = m_Core.GetTextureViewer();
                ret.InitFromPersistString(persistString);
                return ret;
            }
            else if (IsPersist(persistString, typeof(BufferViewer).ToString()))
            {
                var ret = m_Core.GetMeshViewer();
                ret.InitFromPersistString(persistString);
                return ret;
            }
            else if (IsPersist(persistString, typeof(APIInspector).ToString()))
                return m_Core.GetAPIInspector();
            else if (IsPersist(persistString, typeof(PipelineState.PipelineStateViewer).ToString()))
            {
                var ret = m_Core.GetPipelineStateViewer();
                ret.InitFromPersistString(persistString);
                return ret;
            }
            else if (IsPersist(persistString, typeof(DebugMessages).ToString()))
                return m_Core.GetDebugMessages();
            else if (IsPersist(persistString, typeof(TimelineBar).ToString()))
                return m_Core.GetTimelineBar();
            else if (IsPersist(persistString, typeof(StatisticsViewer).ToString()))
                return m_Core.GetStatisticsViewer();
            else if (IsPersist(persistString, typeof(Dialogs.PythonShell).ToString()))
            {
                return new Dialogs.PythonShell(m_Core);
            }
            else if (IsPersist(persistString, typeof(Dialogs.CaptureDialog).ToString()))
            {
                if (m_Core.CaptureDialog == null)
                    m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

                return m_Core.CaptureDialog;
            }
            else if (persistString != null && persistString.Length > 0)
                LoadCustomString(persistString);

            return null;
        }

        private string GetConfigPath(int layout)
        {
            string dir = Core.ConfigDirectory;

            string filename = "DefaultLayout.config";

            if (layout > 0)
            {
                filename = "Layout" + layout.ToString() + ".config";
            }

            return Path.Combine(dir, filename);
        }

        private bool LoadLayout(int layout)
        {
            string configFile = GetConfigPath(layout);
            if (File.Exists(configFile))
            {
                int cnt = dockPanel.Contents.Count;
                for (int i = 0; i < cnt; i++)
                    if(dockPanel.Contents.Count > 0)
                        (dockPanel.Contents[0] as Form).Close();

                try
                {
                    dockPanel.LoadFromXml(configFile, new DeserializeDockContent(GetContentFromPersistString));
                }
                catch (System.Xml.XmlException)
                {
                    // file is invalid
                    return false;
                }
                catch (InvalidOperationException)
                {
                    // file is invalid
                    return false;
                }
                catch (Exception)
                {
                    MessageBox.Show("Something went seriously wrong trying to load the window layout.\n" +
                        "Trying to recover now, but you might have to delete the layout file in %APPDATA%/renderdoc.\n",
                        "Error loading window layout",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }
                return true;
            }
            
            return false;
        }

        private void SaveLayout(int layout)
        {
            string path = GetConfigPath(layout);

            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                dockPanel.SaveAsXml(path, SaveCustomString());
            }
            catch (System.Exception)
            {
                MessageBox.Show(String.Format("Error saving config file\n{0}\nNo config will be saved out.", path));
            }
        }

        private void LoadSaveLayout(ToolStripItem c, bool save)
        {
            if (c.Tag is string)
            {
                int i = 0;
                if (int.TryParse((string)c.Tag, out i))
                {
                    if (save)
                        SaveLayout(i);
                    else
                        LoadLayout(i);
                }
            }
        }

        private void saveLayout_Click(object sender, EventArgs e)
        {
            if (sender is ToolStripItem)
                LoadSaveLayout((ToolStripItem)sender, true);
        }

        private void loadLayout_Click(object sender, EventArgs e)
        {
            if (sender is ToolStripItem)
                LoadSaveLayout((ToolStripItem)sender, false);
        }

        private void SetTitle(string filename)
        {
            string prefix = "";

            if (m_Core != null && m_Core.LogLoaded)
            {
                prefix = Path.GetFileName(filename);
                if (m_Core.APIProps.degraded)
                    prefix += " !DEGRADED PERFORMANCE!";
                prefix += " - ";
            }

            if (m_Core != null && m_Core.Renderer.Remote != null)
                prefix += String.Format("Remote: {0} - ", m_Core.Renderer.Remote.Hostname);

            Text = prefix + "RenderDoc ";
            if(OfficialVersion)
                Text += VersionString;
            else
                Text += String.Format("Unofficial release ({0} - {1})", VersionString, GitCommitHash);

            if (IsVersionMismatched())
                Text += " - !! VERSION MISMATCH DETECTED !!";
        }

        private void SetTitle()
        {
            SetTitle(m_Core != null ? m_Core.LogFileName : "");
        }

        #endregion

        #region Capture & Log Loading

        public void LoadLogfile(string filename, bool temporary, bool local)
        {
            if (PromptCloseLog())
            {
                if (m_Core.LogLoading) return;

                string driver = "";
                string machineIdent = "";
                ReplaySupport support = ReplaySupport.Unsupported;

                bool remoteReplay = !local || (m_Core.Renderer.Remote != null && m_Core.Renderer.Remote.Connected);

                if (local)
                {
                    support = StaticExports.SupportLocalReplay(filename, out driver, out machineIdent);

                    // if the return value suggests remote replay, and it's not already selected, AND the user hasn't
                    // previously chosen to always replay locally without being prompted, ask if they'd prefer to
                    // switch to a remote context for replaying.
                    if (support == ReplaySupport.SuggestRemote && !remoteReplay && !m_Core.Config.AlwaysReplayLocally)
                    {
                        var dialog = new Dialogs.SuggestRemoteDialog(driver, machineIdent);

                        FillRemotesToolStrip(dialog.RemoteItems, false);

                        dialog.ShowDialog();

                        if (dialog.Result == Dialogs.SuggestRemoteDialog.SuggestRemoteResult.Cancel)
                        {
                            return;
                        }
                        else if (dialog.Result == Dialogs.SuggestRemoteDialog.SuggestRemoteResult.Remote)
                        {
                            // we only get back here from the dialog once the context switch has begun,
                            // so contextChooser will have been disabled.
                            // Check once to see if it's enabled before even popping up the dialog in case
                            // it has finished already. Otherwise pop up a waiting dialog until it completes
                            // one way or another, then process the result.

                            if (!contextChooser.Enabled)
                            {
                                ProgressPopup modal = new ProgressPopup((ModalCloseCallback)delegate
                                {
                                    return contextChooser.Enabled;
                                }, false);
                                modal.SetModalText("Please Wait - Checking remote connection...");

                                modal.ShowDialog();
                            }

                            remoteReplay = (m_Core.Renderer.Remote != null && m_Core.Renderer.Remote.Connected);

                            if (!remoteReplay)
                            {
                                string remoteMessage = "Failed to make a connection to the remote server.\n\n";

                                remoteMessage += "More information may be available in the status bar.";

                                MessageBox.Show(remoteMessage, "Couldn't connect to remote server", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                                return;
                            }
                        }
                        else
                        {
                            // nothing to do - we just continue replaying locally
                            // however we need to check if the user selected 'always replay locally' and
                            // set that bit as sticky in the config
                            if (dialog.AlwaysReplayLocally)
                            {
                                m_Core.Config.AlwaysReplayLocally = true;

                                m_Core.Config.Serialize(Core.ConfigFilename);
                            }
                        }
                    }

                    if (remoteReplay)
                    {
                        support = ReplaySupport.Unsupported;

                        string[] remoteDrivers = m_Core.Renderer.GetRemoteSupport();

                        for (int i = 0; i < remoteDrivers.Length; i++)
                        {
                            if (driver == remoteDrivers[i])
                                support = ReplaySupport.Supported;
                        }
                    }
                }

                Thread thread = null;

                string origFilename = filename;

                // if driver is empty something went wrong loading the log, let it be handled as usual
                // below. Otherwise indicate that support is missing.
                if (driver.Length > 0 && support == ReplaySupport.Unsupported)
                {
                    if (remoteReplay)
                    {
                        string remoteMessage = String.Format("This log was captured with {0} and cannot be replayed on {1}.\n\n", driver, m_Core.Renderer.Remote.Hostname);

                        remoteMessage += "Try selecting a different remote context in the status bar.";

                        MessageBox.Show(remoteMessage, "Unsupported logfile type", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                        return;
                    }
                    else
                    {
                        string remoteMessage = String.Format("This log was captured with {0} and cannot be replayed locally.\n\n", driver);

                        remoteMessage += "Try selecting a remote context in the status bar.";

                        MessageBox.Show(remoteMessage, "Unsupported logfile type", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                        return;
                    }
                }
                else
                {
                    if (remoteReplay && local)
                    {
                        try
                        {
                            filename = m_Core.Renderer.CopyCaptureToRemote(filename, this);

                            // deliberately leave local as true so that we keep referring to the locally saved log

                            // some error
                            if (filename == "")
                                throw new ApplicationException();
                        }
                        catch (Exception)
                        {
                            MessageBox.Show("Couldn't copy " + origFilename + " to remote host for replaying", "Error copying to remote",
                                                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return;
                        }
                    }

                    thread = Helpers.NewThread(new ThreadStart(() => m_Core.LoadLogfile(filename, origFilename, temporary, local)));
                }

                thread.Start();

                if(!remoteReplay)
                    m_Core.Config.LastLogPath = Path.GetDirectoryName(filename);

                statusText.Text = "Loading " + origFilename + "...";
            }
        }

        public void PopulateRecentFiles()
        {
            while (recentFilesToolStripMenuItem.DropDownItems.Count > 0)
            {
                if (recentFilesToolStripMenuItem.DropDownItems[0] is ToolStripSeparator)
                    break;

                recentFilesToolStripMenuItem.DropDownItems.RemoveAt(0);
            }

            recentFilesToolStripMenuItem.Enabled = false;

            int i = m_Core.Config.RecentLogFiles.Count;
            int idx = 0;
            foreach (var recentLog in m_Core.Config.RecentLogFiles)
            {
                var item = new ToolStripMenuItem("&" + i.ToString() + " " + recentLog, null, recentLogMenuItem_Click);
                item.Tag = idx;
                recentFilesToolStripMenuItem.DropDownItems.Insert(0, item);

                i--;
                idx++;

                recentFilesToolStripMenuItem.Enabled = true;
            }
        }

        private void PopulateRecentCaptures()
        {
            while (recentCapturesToolStripMenuItem.DropDownItems.Count > 0)
            {
                if (recentCapturesToolStripMenuItem.DropDownItems[0] is ToolStripSeparator)
                    break;

                recentCapturesToolStripMenuItem.DropDownItems.RemoveAt(0);
            }

            recentCapturesToolStripMenuItem.Enabled = false;

            int i = m_Core.Config.RecentCaptureSettings.Count;
            int idx = 0;
            foreach (var recentCapture in m_Core.Config.RecentCaptureSettings)
            {
                var item = new ToolStripMenuItem("&" + i.ToString() + " " + recentCapture, null, recentCaptureMenuItem_Click);
                item.Tag = idx;
                recentCapturesToolStripMenuItem.DropDownItems.Insert(0, item);

                i--;
                idx++;

                recentCapturesToolStripMenuItem.Enabled = true;
            }
        }

        public bool OwnTemporaryLog = false;
        private bool SavedTemporaryLog = false;

        public void ShowLiveCapture(LiveCapture live)
        {
            m_LiveCaptures.Add(live);
            live.Show(dockPanel);
        }

        public void LiveCaptureClosed(LiveCapture live)
        {
            m_LiveCaptures.Remove(live);
        }

        private void OpenCaptureConfigFile(String filename, bool exe)
        {
            if (m_Core.CaptureDialog == null)
                m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

            if(exe)
                m_Core.CaptureDialog.SetExecutableFilename(filename);
            else
                m_Core.CaptureDialog.LoadSettings(filename);
            m_Core.CaptureDialog.Show(dockPanel);

            // workaround for Show() not doing this
            if (m_Core.CaptureDialog.DockState == DockState.DockBottomAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockLeftAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockRightAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockTopAutoHide)
            {
                dockPanel.ActiveAutoHideContent = m_Core.CaptureDialog;
            }
        }

        private void LoadFromFilename(string filename)
        {
            if (Path.GetExtension(filename) == ".rdc")
            {
                LoadLogfile(filename, false, true);
            }
            else if (Path.GetExtension(filename) == ".cap")
            {
                OpenCaptureConfigFile(filename, false);
            }
            else if (Path.GetExtension(filename) == ".exe")
            {
                OpenCaptureConfigFile(filename, true);
            }
            else
            {
                // not a recognised filetype, see if we can load it anyway
                LoadLogfile(filename, false, true);
            }
        }

        public void CloseLogfile()
        {
            m_Core.CloseLogfile();

            saveLogToolStripMenuItem.Enabled = false;
        }

        private string lastSaveCapturePath = "";

        public string GetSavePath()
        {
            if(m_Core.Config.DefaultCaptureSaveDirectory != "")
            {
                try
                {
                    if (lastSaveCapturePath == "")
                        saveDialog.InitialDirectory = m_Core.Config.DefaultCaptureSaveDirectory;
                    else
                        saveDialog.InitialDirectory = lastSaveCapturePath;
                }
                catch (Exception)
                {
                }
            }

            saveDialog.FileName = "";

            DialogResult res = saveDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                try
                {
                    string dir = Path.GetDirectoryName(saveDialog.FileName);
                    if(Directory.Exists(dir))
                        lastSaveCapturePath = dir;
                }
                catch (Exception)
                {
                }

                return saveDialog.FileName;
            }

            return "";
        }

        private void OnCaptureTrigger(string exe, string workingDir, string cmdLine, EnvironmentModification[] env, CaptureOptions opts, Dialogs.CaptureDialog.OnConnectionEstablishedMethod callback)
        {
            if (!PromptCloseLog())
                return;

            string logfile = m_Core.TempLogFilename(Path.GetFileNameWithoutExtension(exe));

            StaticExports.SetConfigSetting("MaxConnectTimeout", m_Core.Config.MaxConnectTimeout.ToString());

            Thread th = Helpers.NewThread(new ThreadStart(() =>
            {
                UInt32 ret = m_Core.Renderer.ExecuteAndInject(exe, workingDir, cmdLine, env, logfile, opts);

                this.BeginInvoke(new Action(() =>
                {
                    if (ret == 0)
                    {
                        MessageBox.Show(string.Format("Error launching {0} for capture.\n\nCheck diagnostic log in Help menu for more details.", exe),
                                           "Error kicking capture", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }

                    var live = new LiveCapture(m_Core, m_Core.Renderer.Remote == null ? "" : m_Core.Renderer.Remote.Hostname, ret, this);
                    ShowLiveCapture(live);
                    callback(live);
                }));
            }));
            th.Start();

            // wait a few ms before popping up a progress bar
            th.Join(500);

            if (th.IsAlive)
            {
                ProgressPopup modal = new ProgressPopup((ModalCloseCallback)delegate
                {
                    return !th.IsAlive;
                }, false);
                modal.SetModalText(String.Format("Launching {0}, please wait...", exe));

                modal.ShowDialog();
            }
        }

        private void OnInjectTrigger(UInt32 PID, EnvironmentModification[] env, string name, CaptureOptions opts, Dialogs.CaptureDialog.OnConnectionEstablishedMethod callback)
        {
            if (!PromptCloseLog())
                return;

            string logfile = m_Core.TempLogFilename(name);

            Thread th = Helpers.NewThread(new ThreadStart(() =>
            {
                UInt32 ret = StaticExports.InjectIntoProcess(PID, env, logfile, opts);

                this.BeginInvoke(new Action(() =>
                {
                    if (ret == 0)
                    {
                        MessageBox.Show(string.Format("Error injecting into process {0} for capture.\n\nCheck diagnostic log in Help menu for more details.", PID),
                                           "Error kicking capture", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }

                    var live = new LiveCapture(m_Core, m_Core.Renderer.Remote == null ? "" : m_Core.Renderer.Remote.Hostname, ret, this);
                    ShowLiveCapture(live);
                    callback(live);
                }));
            }));
            th.Start();

            // wait a few ms before popping up a progress bar
            th.Join(500);

            if (th.IsAlive)
            {
                ProgressPopup modal = new ProgressPopup((ModalCloseCallback)delegate
                {
                    return !th.IsAlive;
                }, false);
                modal.SetModalText(String.Format("Injecting into {0}, please wait...", PID));

                modal.ShowDialog();
            }
        }

        private void captureLogToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (m_Core.CaptureDialog == null)
                m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

            m_Core.CaptureDialog.InjectMode = false;
            m_Core.CaptureDialog.Show(dockPanel);

            // workaround for Show() not doing this
            if (m_Core.CaptureDialog.DockState == DockState.DockBottomAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockLeftAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockRightAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockTopAutoHide)
            {
                dockPanel.ActiveAutoHideContent = m_Core.CaptureDialog;
            }
        }

        private void attachToInstanceToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.RemoteManager(m_Core, this)).ShowDialog();
        }

        private void injectIntoProcessToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (m_Core.CaptureDialog == null)
                m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

            m_Core.CaptureDialog.InjectMode = true;
            m_Core.CaptureDialog.Show(dockPanel);

            // workaround for Show() not doing this
            if (m_Core.CaptureDialog.DockState == DockState.DockBottomAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockLeftAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockRightAutoHide ||
                m_Core.CaptureDialog.DockState == DockState.DockTopAutoHide)
            {
                dockPanel.ActiveAutoHideContent = m_Core.CaptureDialog;
            }
        }

        private void openLogToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if(!PromptCloseLog())
                return;

            if (m_Core.Config.LastLogPath.Length > 0)
                openDialog.InitialDirectory = m_Core.Config.LastLogPath;

            DialogResult res = openDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                LoadFromFilename(openDialog.FileName);
            }
        }

        private void FillRemotesToolStrip(ToolStripItemCollection strip, bool includeLocalhost)
        {
            ToolStripItem[] items = new ToolStripItem[m_Core.Config.RemoteHosts.Count];

            int idx = 0;

            for (int i = 0; i < m_Core.Config.RemoteHosts.Count; i++)
            {
                RemoteHost host = m_Core.Config.RemoteHosts[i];

                // add localhost at the end
                if (host.Hostname == "localhost")
                    continue;

                ToolStripItem item = new ToolStripMenuItem();

                item.Image = host.ServerRunning && !host.VersionMismatch
                    ? global::renderdocui.Properties.Resources.tick
                    : global::renderdocui.Properties.Resources.cross;
                if (host.Connected)
                    item.Text = String.Format("{0} (Connected)", host.Hostname);
                else if (host.ServerRunning && host.VersionMismatch)
                    item.Text = String.Format("{0} (Bad Version)", host.Hostname);
                else if (host.ServerRunning && host.Busy)
                    item.Text = String.Format("{0} (Busy)", host.Hostname);
                else if (host.ServerRunning)
                    item.Text = String.Format("{0} (Online)", host.Hostname);
                else
                    item.Text = String.Format("{0} (Offline)", host.Hostname);
                item.Click += new EventHandler(switchContext);
                item.Tag = host;

                // don't allow switching to the connected host
                if (host.Connected)
                    item.Enabled = false;

                items[idx++] = item;
            }

            if(includeLocalhost && idx < items.Length)
                items[idx] = localContext;

            strip.Clear();
            foreach(ToolStripItem item in items)
                if(item != null)
                    strip.Add(item);
        }

        private void contextChooser_DropDownOpening(object sender, EventArgs e)
        {
            FillRemotesToolStrip(contextChooser.DropDownItems, true);
        }

        private void switchContext(object sender, EventArgs e)
        {
            ToolStripItem item = sender as ToolStripItem;

            m_SelectedHost = null;

            if (item == null)
                return;

            RemoteHost host = item.Tag as RemoteHost;

            foreach (var live in m_LiveCaptures)
            {
                // allow live captures to this host to stay open, that way
                // we can connect to a live capture, then switch into that
                // context
                if (host != null && live.Hostname == host.Hostname)
                    continue;

                if (live.CheckAllowClose() == false)
                    return;
            }

            if (!PromptCloseLog())
                return;

            foreach (var live in m_LiveCaptures.ToArray())
            {
                // allow live captures to this host to stay open, that way
                // we can connect to a live capture, then switch into that
                // context
                if (host != null && live.Hostname == host.Hostname)
                    continue;

                live.CleanItems();
                live.Close();
            }

            m_Core.Renderer.DisconnectFromRemoteServer();

            if (host == null)
            {
                contextChooser.Image = global::renderdocui.Properties.Resources.house;
                contextChooser.Text = "Replay Context: Local";

                injectIntoProcessToolStripMenuItem.Enabled = true;

                statusText.Text = "";

                SetTitle();
            }
            else
            {
                contextChooser.Text = "Replay Context: " + host.Hostname;
                contextChooser.Image = host.ServerRunning
                    ? global::renderdocui.Properties.Resources.connect
                    : global::renderdocui.Properties.Resources.disconnect;

                // disable until checking is done
                contextChooser.Enabled = false;

                injectIntoProcessToolStripMenuItem.Enabled = false;

                SetTitle();

                statusText.Text = "Checking remote server status...";

                m_SelectedHost = host;

                Thread th = Helpers.NewThread(new ThreadStart(() =>
                {
                    // see if the server is up
                    host.CheckStatus();

                    if (!host.ServerRunning && host.RunCommand != "")
                    {
                        this.BeginInvoke(new Action(() => { statusText.Text = "Running remote server command..."; }));

                        host.Launch();

                        // check if it's running now
                        host.CheckStatus();
                    }

                    if (host.ServerRunning && !host.Busy)
                    {
                        m_Core.Renderer.ConnectToRemoteServer(host);
                    }

                    this.BeginInvoke(new Action(() =>
                    {
                        contextChooser.Image = (host.ServerRunning && !host.Busy)
                            ? global::renderdocui.Properties.Resources.connect
                            : global::renderdocui.Properties.Resources.disconnect;

                        if (m_Core.Renderer.InitException != null)
                        {
                            contextChooser.Image = global::renderdocui.Properties.Resources.cross;
                            contextChooser.Text = "Replay Context: Local";
                            statusText.Text = "Connection failed: " + m_Core.Renderer.InitException.Status.Str();
                        }
                        else if (host.VersionMismatch)
                        {
                            statusText.Text = "Remote server is not running RenderDoc " + VersionString;
                        }
                        else if (host.Busy)
                        {
                            statusText.Text = "Remote server in use elsewhere";
                        }
                        else if (host.ServerRunning)
                        {
                            statusText.Text = "Remote server ready";
                        }
                        else
                        {
                            if(host.RunCommand != "")
                                statusText.Text = "Remote server not running or failed to start";
                            else
                                statusText.Text = "Remote server not running - no start command configured";
                        }

                        contextChooser.Enabled = true;
                    }));
                }));

                th.Start();
            }
        }

        #endregion

        #region Menu Handlers

        private void SetUpdateAvailable()
        {
            helpToolStripMenuItem.Image = global::renderdocui.Properties.Resources.hourglass;
            updateToolStripMenuItem.Enabled = true;
            updateToolStripMenuItem.Text = "An update is available";
        }

        private void SetNoUpdate()
        {
            helpToolStripMenuItem.Image = null;
            updateToolStripMenuItem.Enabled = false;
            updateToolStripMenuItem.Text = "No update available";
        }

        private void UpdatePopup()
        {
            (new Dialogs.UpdateDialog(m_Core)).ShowDialog();
        }

        private enum UpdateResult
        {
            Disabled,
            Unofficial,
            Toosoon,
            Latest,
            Upgrade,
        };

        private delegate void UpdateResultMethod(UpdateResult res);

        private bool IsVersionMismatched()
        {
            try
            {
                return "v" + StaticExports.GetVersionString() != VersionString;
            }
            catch (System.Exception)
            {
                // probably StaticExports.GetVersionString is missing, which means an old
                // version is running
                return true;
            }
        }

        private bool HandleMismatchedVersions()
        {
            if (IsVersionMismatched())
            {
                if (!OfficialVersion)
                {
                    MessageBox.Show("You are running an unofficial build with mismatched core and UI versions.\n" +
                        "Double check where you got your build from and do a sanity check!",
                        "Unofficial build - mismatched versions", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    DialogResult mb = MessageBox.Show("RenderDoc has detected mismatched versions between its internal module and UI.\n" +
                        "This is likely caused by a buggy update in the past which partially updated your install. Likely because a " +
                        "program was running with renderdoc while the update happened.\n" +
                        "You should reinstall RenderDoc immediately as this configuration is almost guaranteed to crash.\n\n" +
                        "Would you like to open the downloads page?",
                        "Mismatched versions", MessageBoxButtons.YesNo, MessageBoxIcon.Exclamation);

                    if (mb == DialogResult.Yes)
                        Process.Start("https://renderdoc.org/builds");

                    SetUpdateAvailable();
                }
                return true;
            }

            return false;
        }

        private void CheckUpdates(bool forceCheck = false, UpdateResultMethod callback = null)
        {
            bool mismatch = HandleMismatchedVersions();
            if (mismatch)
                return;

            if (!forceCheck && !m_Core.Config.CheckUpdate_AllowChecks)
            {
                updateToolStripMenuItem.Text = "Update checks disabled";
                if (callback != null) callback(UpdateResult.Disabled);
                return;
            }

            if (!OfficialVersion)
            {
                if (callback != null) callback(UpdateResult.Unofficial);
                return;
            }

            if (m_Core.Config.CheckUpdate_UpdateAvailable)
            {
                if (m_Core.Config.CheckUpdate_UpdateResponse.Length == 0)
                {
                    forceCheck = true;
                }
                else if(!forceCheck)
                {
                    SetUpdateAvailable();
                    return;
                }
            }

            DateTime today = DateTime.Now;
            DateTime compare = today.AddDays(-2);

            if (!forceCheck && compare.CompareTo(m_Core.Config.CheckUpdate_LastUpdate) < 0)
            {
                if (callback != null) callback(UpdateResult.Toosoon);
                return;
            }

            m_Core.Config.CheckUpdate_LastUpdate = today;

            string versionCheck = BareVersionString;

            statusText.Text = "Checking for updates...";
            statusProgress.Visible = true;
            statusProgress.Style = ProgressBarStyle.Marquee;
            statusProgress.MarqueeAnimationSpeed = 50;

            var updateThread = Helpers.NewThread(new ThreadStart(() =>
            {
                // spawn thread to check update
                WebRequest g = HttpWebRequest.Create(String.Format("https://renderdoc.org/getupdateurl/{0}/{1}?rtfnotes=1", IntPtr.Size == 4 ? "32" : "64", versionCheck));

                UpdateResult result = UpdateResult.Disabled;

                try
                {
                    var webresp = g.GetResponse();

                    using (var sr = new StreamReader(webresp.GetResponseStream()))
                    {
                        string response = sr.ReadToEnd();

                        if (response != "")
                        {
                            // window may have been closed while update check was on-going. If so, just return
                            if (Visible)
                            {
                                BeginInvoke((MethodInvoker)delegate
                                {
                                    m_Core.Config.CheckUpdate_UpdateAvailable = true;
                                    m_Core.Config.CheckUpdate_UpdateResponse = response;
                                    SetUpdateAvailable();
                                    UpdatePopup();
                                });
                            }
                            result = UpdateResult.Upgrade;
                        }
                        else if (callback != null)
                        {
                            if (Visible)
                            {
                                BeginInvoke((MethodInvoker)delegate
                                {
                                    m_Core.Config.CheckUpdate_UpdateAvailable = false;
                                    m_Core.Config.CheckUpdate_UpdateResponse = "";
                                    SetNoUpdate();
                                });
                            }
                            result = UpdateResult.Latest;
                        }
                    }

                    webresp.Close();
                }
                catch (WebException ex)
                {
                    StaticExports.LogText(String.Format("Problem checking for updates - {0}", ex.Message));
                }
                catch (Exception)
                {
                    // just want to swallow the exception, checking for updates doesn't need to be handled
                    // and it's not worth trying to retry.
                }

                // window may have been closed while update check was on-going. If so, just return
                if (Visible)
                {
                    BeginInvoke((MethodInvoker)delegate
                    {
                        statusText.Text = "";
                        statusProgress.Visible = false;
                        statusProgress.Style = ProgressBarStyle.Continuous;
                        statusProgress.MarqueeAnimationSpeed = 0;
                        if (callback != null && result != UpdateResult.Disabled)
                            callback(result);
                    });
                }
            }));

            updateThread.Start();
        }

        private void checkForUpdatesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            CheckUpdates(true, (UpdateResult res) =>
            {
                switch (res)
                {
                    case UpdateResult.Disabled:
                    case UpdateResult.Toosoon:
                    {
                        // won't happen, we forced the check
                        break;
                    }
                    case UpdateResult.Unofficial:
                    {
                        DialogResult mb = MessageBox.Show("You are running an unofficial build, not beta or stable release.\n" +
                            "Updates are only available for installed release builds\n\n" +
                            "Would you like to open the builds list in a browser?",
                            "Unofficial build", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                        if (mb == DialogResult.Yes)
                            Process.Start("https://renderdoc.org/builds");
                        break;
                    }
                    case UpdateResult.Latest:
                    {
                        MessageBox.Show("You are running the latest version.\n", "Latest version", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        break;
                    }
                    case UpdateResult.Upgrade:
                    {
                        // CheckUpdates() will have shown a dialog for this
                        break;
                    }
                }
            });
        }

        private void updateToolStripMenuItem_Click(object sender, EventArgs e)
        {
            bool mismatch = HandleMismatchedVersions();
            if (mismatch)
                return;

            SetUpdateAvailable();
            UpdatePopup();
        }

        private bool PromptCloseLog()
        {
            if (!m_Core.LogLoaded)
                return true;

            string deletepath = "";
            bool loglocal = false;

            if (OwnTemporaryLog)
            {
                string temppath = m_Core.LogFileName;
                loglocal = m_Core.IsLogLocal;

                DialogResult res = DialogResult.No;

                // unless we've saved the log, prompt to save
                if(!SavedTemporaryLog)
                    res = MessageBox.Show("Save this logfile?", "Unsaved log", MessageBoxButtons.YesNoCancel);

                if (res == DialogResult.Cancel)
                {
                    return false;
                }

                if (res == DialogResult.Yes)
                {
                    bool success = PromptSaveLog();

                    if (!success)
                    {
                        return false;
                    }
                }

                if (temppath != m_Core.LogFileName || res == DialogResult.No)
                    deletepath = temppath;
                OwnTemporaryLog = false;
                SavedTemporaryLog = false;
            }

            CloseLogfile();

            try
            {
                if (deletepath.Length > 0)
                    m_Core.Renderer.DeleteCapture(deletepath, loglocal);
            }
            catch (System.Exception)
            {
                // can't delete it! maybe already deleted?
            }

            return true;
        }

        private bool PromptSaveLog()
        {
            string saveFilename = GetSavePath();

            if (saveFilename != "")
            {
                if (m_Core.IsLogLocal && !File.Exists(m_Core.LogFileName))
                {
                    MessageBox.Show("Logfile " + m_Core.LogFileName + " couldn't be found, cannot save.", "File not found",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }

                try
                {
                    if (m_Core.IsLogLocal)
                    {
                        // we copy the (possibly) temp log to the desired path, but the log item remains referring to the original path.
                        // This ensures that if the user deletes the saved path we can still open or re-save it.
                        File.Copy(m_Core.LogFileName, saveDialog.FileName, true);
                    }
                    else
                    {
                        m_Core.Renderer.CopyCaptureFromRemote(m_Core.LogFileName, saveDialog.FileName, this);
                    }

                    m_Core.Config.AddRecentFile(m_Core.Config.RecentLogFiles, saveDialog.FileName, 10);
                    PopulateRecentFiles();
                    SetTitle(saveDialog.FileName);
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save to " + saveDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot save",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }

                // we don't prompt to save on closing - if the user deleted the log that we just saved, then
                // that is up to them.
                SavedTemporaryLog = true;

                return true;
            }

            return false;
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            // bookmark keys are handled globally
            if ((keyData & Keys.Control) == Keys.Control)
            {
                Keys[] digits = { Keys.D1, Keys.D2, Keys.D3, Keys.D4, Keys.D5,
                                  Keys.D6, Keys.D7, Keys.D8, Keys.D9, Keys.D0 };

                for (int i = 0; i < digits.Length; i++)
                {
                    if (keyData == (Keys.Control|digits[i]))
                    {
                        EventBrowser eb = m_Core.GetEventBrowser();

                        if (eb.Visible && eb.HasBookmark(i))
                        {
                            m_Core.SetEventID(null, eb.GetBookmark(i));

                            return true;
                        }
                    }
                }

                Control sender = Control.FromHandle(msg.HWnd);

                if (m_Core.LogLoaded && !(sender is TextBox) && !(sender is ScintillaNET.Scintilla))
                {
                    if (keyData == (Keys.Control | Keys.Left))
                    {
                        FetchDrawcall draw = m_Core.CurDrawcall;

                        if (draw != null && draw.previous != null)
                            m_Core.SetEventID(null, draw.previous.eventID);

                        return true;
                    }

                    if (keyData == (Keys.Control | Keys.Right))
                    {
                        FetchDrawcall draw = m_Core.CurDrawcall;

                        if (draw != null && draw.next != null)
                            m_Core.SetEventID(null, draw.next.eventID);

                        return true;
                    }
                }
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        private void MainWindow_FormClosing(object sender, FormClosingEventArgs e)
        {
            foreach (var live in m_LiveCaptures)
            {
                if (live.CheckAllowClose() == false)
                {
                    e.Cancel = true;
                    return;
                }
            }

            if (StaticExports.IsGlobalHookActive())
            {
                MessageBox.Show("Cannot close RenderDoc while global hook is active.", "Global hook active",
                                MessageBoxButtons.OK, MessageBoxIcon.Error);
                e.Cancel = true;
                return;
            }

            if (!PromptCloseLog())
            {
                e.Cancel = true;
                return;
            }

            foreach (var live in m_LiveCaptures.ToArray())
            {
                live.CleanItems();
                live.Close();
            }

            SaveLayout(0);

            m_Core.Shutdown();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.AboutDialog(VersionString)).ShowDialog();
        }

        private void optionsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.SettingsDialog(m_Core)).ShowDialog();
        }

        private void viewLogFileToolStripMenuItem_Click(object sender, EventArgs e)
        {
            string fn = StaticExports.GetLogFilename();

            if (File.Exists(fn))
            {
                Process.Start(fn);
            }
        }

        private void recentLogMenuItem_Click(object sender, EventArgs e)
        {
            ToolStripDropDownItem item = (ToolStripDropDownItem)sender;

            String filename = item.Text.Substring(3);

            if(File.Exists(filename))
            {
                LoadLogfile(filename, false, true);
            }
            else
            {
                DialogResult res = MessageBox.Show("File " + filename + " couldn't be found.\nRemove from recent list?", "File not found",
                                                    MessageBoxButtons.YesNoCancel, MessageBoxIcon.Information);

                if (res == DialogResult.Yes)
                {
                    int index = (int)item.Tag;
                    m_Core.Config.RecentLogFiles.RemoveAt(index);

                    PopulateRecentFiles();
                }
            }
        }

        private void recentCaptureMenuItem_Click(object sender, EventArgs e)
        {
            ToolStripDropDownItem item = (ToolStripDropDownItem)sender;

            String filename = item.Text.Substring(3);

            if (File.Exists(filename))
            {
                OpenCaptureConfigFile(filename, false);
            }
            else
            {
                DialogResult res = MessageBox.Show("File " + filename + " couldn't be found.\nRemove from recent list?", "File not found",
                                                    MessageBoxButtons.YesNoCancel, MessageBoxIcon.Information);

                if (res == DialogResult.Yes)
                {
                    int index = (int)item.Tag;
                    m_Core.Config.RecentCaptureSettings.RemoveAt(index);

                    PopulateRecentCaptures();
                }
            }
        }

        private void clearHistoryToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.Config.RecentLogFiles.Clear();

            PopulateRecentFiles();
        }

        private void saveLogToolStripMenuItem_Click(object sender, EventArgs e)
        {
            PromptSaveLog();
        }

        private void closeLogToolStripMenuItem_Click(object sender, EventArgs e)
        {
            PromptCloseLog();
        }

        private void clearHistoryToolStripMenuItem1_Click(object sender, EventArgs e)
        {
            m_Core.Config.RecentCaptureSettings.Clear();

            PopulateRecentCaptures();
        }

        private void sendErrorReportToolStripMenuItem_Click(object sender, EventArgs e)
        {
            StaticExports.TriggerExceptionHandler(IntPtr.Zero, false);
        }

        private void manageRemote_Click(object sender, EventArgs e)
        {
            (new Dialogs.RemoteManager(m_Core, this)).ShowDialog();
        }

        private void viewDocsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Help.ShowHelp(this, "renderdoc.chm", HelpNavigator.Topic, "html/b97b19f8-2b97-4dca-8a7a-ed7026eb43fe.htm");
            Help.ShowHelp(this, "renderdoc.chm");
        }

        private void showTipsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.TipsDialog(m_Core)).ShowDialog();
        }

        private void nightlybuildsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Process.Start("https://renderdoc.org/builds");
        }

        private void sourceOnGithubToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Process.Start("https://github.com/baldurk/renderdoc");
        }

        #endregion

        #region Dock Content showers

        private void eventViewerToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetEventBrowser().Show(dockPanel);
        }

        private void textureToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var t = m_Core.GetTextureViewer();
            
            if(!t.Visible)
                t.InitFromPersistString("");

            t.Show(dockPanel);
        }

        private void pythonShellToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.PythonShell(m_Core)).Show(dockPanel);
        }

        private void PipelineStateToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetPipelineStateViewer().Show(dockPanel);
        }

        private void APIInspectorToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetAPIInspector().Show(dockPanel);
        }

        private void debugMessagesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetDebugMessages().Show(dockPanel);
        }

        private void meshOutputToolStripMenuItem_Click(object sender, EventArgs e)
        {
            BufferViewer b = m_Core.GetMeshViewer();

            b.InitFromPersistString("");

            b.Show(dockPanel);
        }

        private void timelineToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetTimelineBar().Show(dockPanel);
        }

        private void statisticsViewerToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetStatisticsViewer().Show(dockPanel);
        }

        #endregion

        #region Symbol resolving

        private bool SymbolResolveCallback()
        {
            bool ret = false;
            // just bail if we managed to get here without a resolver.
            m_Core.Renderer.Invoke((ReplayRenderer r) => { ret = !r.HasCallstacks() || r.InitResolver(); });

            return ret;
        }

        private void resolveSymbolsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.Renderer.BeginInvoke((ReplayRenderer r) => { r.InitResolver(); });

            ProgressPopup modal = new ProgressPopup(SymbolResolveCallback, false);
            modal.SetModalText("Please Wait - Resolving Symbols.");

            modal.ShowDialog();

            m_Core.GetAPIInspector().FillCallstack();
        }

        #endregion

        #region Drag & Drop

        private string ValidData(IDataObject d)
        {
            var fmts = new List<string>(d.GetFormats());

            if (fmts.Contains("FileName"))
            {
                var data = d.GetData("FileName") as Array;

                if (data != null && data.Length == 1 && data.GetValue(0) is string)
                {
                    var filename = (string)data.GetValue(0);

                    if (File.Exists(filename))
                    {
                        return Path.GetFullPath(filename);
                    }
                }
            }

            return "";
        }

        private void MainWindow_DragDrop(object sender, DragEventArgs e)
        {
            string fn = ValidData(e.Data);
            if (fn.Length > 0)
            {
                LoadFromFilename(fn);
            }
        }

        private void MainWindow_DragEnter(object sender, DragEventArgs e)
        {
            if (ValidData(e.Data).Length > 0)
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;
        }

    #endregion

        private void startAndroidRemoteServerToolStripMenuItem_Click(object sender, EventArgs e)
        {
            string device = "";
            if (m_SelectedHost != null)
                device = m_SelectedHost.Hostname;
            StaticExports.StartAndroidRemoteServer(device);
        }
    }
}
