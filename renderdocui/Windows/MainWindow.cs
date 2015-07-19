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

        private renderdocplugin.ReplayManagerPlugin m_ReplayHost = null;
        private string m_RemoteReplay = "";

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
                return InformationalVersion.Replace("-official", "").Replace("-beta", "");
            }
        }

        private bool BetaVersion
        {
            get
            {
                return InformationalVersion.Contains("-beta");
            }
        }

        private bool OfficialVersion
        {
            get
            {
                return InformationalVersion.Contains("-official");
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

            logStatisticsToolStripMenuItem.Enabled = false;

            resolveSymbolsToolStripMenuItem.Enabled = false;
            resolveSymbolsToolStripMenuItem.Text = "Resolve Symbols";

            m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

            m_Core.AddLogViewer(this);
            m_Core.AddLogProgressListener(this);
        }

        private void MainWindow_Load(object sender, EventArgs e)
        {
            bool loaded = LoadLayout(0);

            CheckUpdates();

            sendErrorReportToolStripMenuItem.Enabled = OfficialVersion || BetaVersion;

            // create default layout if layout failed to load
            if (!loaded)
            {
                m_Core.GetAPIInspector().Show(dockPanel);
                m_Core.GetEventBrowser().Show(m_Core.GetAPIInspector().Pane, DockAlignment.Top, 0.5);

                m_Core.GetPipelineStateViewer().Show(dockPanel);

                var bv = new BufferViewer(m_Core, true);
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
                if (Path.GetExtension(m_InitFilename) == ".rdc")
                {
                    LoadLogAsync(m_InitFilename, false);
                }
                else if (Path.GetExtension(m_InitFilename) == ".cap")
                {
                    if (m_Core.CaptureDialog == null)
                        m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

                    m_Core.CaptureDialog.LoadSettings(m_InitFilename);
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
                else
                {
                    // not a recognised filetype, see if we can load it anyway
                    LoadLogAsync(m_InitFilename, false);
                }

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
            statusText.Text = "";
            statusIcon.Image = null;
            statusProgress.Visible = false;

            m_MessageTick.Dispose();
            m_MessageTick = null;

            logStatisticsToolStripMenuItem.Enabled = false;

            resolveSymbolsToolStripMenuItem.Enabled = false;
            resolveSymbolsToolStripMenuItem.Text = "Resolve Symbols";

            if (m_ReplayHost != null)
                m_ReplayHost.CloseReplay();

            m_ReplayHost = null;
            m_RemoteReplay = "";

            SetTitle();
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

                    me.BeginInvoke(new Action(() =>
                    {
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

            if (me == null) return;
            if (me.m_MessageTick != null) me.m_MessageTick.Change(500, System.Threading.Timeout.Infinite);
        }

        private System.Threading.Timer m_MessageTick = null;
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
            LogHasErrors = (m_Core.DebugMessages.Count > 0);

            m_MessageTick = new System.Threading.Timer(MessageCheck, this as object, 500, System.Threading.Timeout.Infinite);

            statusProgress.Visible = false;

            m_Core.Renderer.BeginInvoke((ReplayRenderer r) => {
                bool hasResolver = r.HasCallstacks();

                this.BeginInvoke(new Action(() =>
                {
                    resolveSymbolsToolStripMenuItem.Enabled = hasResolver;
                    resolveSymbolsToolStripMenuItem.Text = hasResolver ? "Resolve Symbols" : "Resolve Symbols - None in log";
                }));
            });

            logStatisticsToolStripMenuItem.Enabled = true;

            saveLogToolStripMenuItem.Enabled = true;

            SetTitle();

            PopulateRecentFiles();

            m_Core.GetEventBrowser().Focus();
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
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
                var ret = new BufferViewer(m_Core, true);
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

        private void SetTitle()
        {
            string prefix = "";

            if (m_Core != null && m_Core.LogLoaded)
            {
                prefix = Path.GetFileName(m_Core.LogFileName);
                if (m_Core.APIProps.degraded)
                    prefix += " !DEGRADED PERFORMANCE!";
                if (m_RemoteReplay.Length > 0)
                    prefix += String.Format(" (Remote replay on {0})", m_RemoteReplay);
                prefix += " - ";
            }

            Text = prefix + "RenderDoc ";
            if(OfficialVersion)
                Text += VersionString;
            else if(BetaVersion)
                Text += String.Format("{0}-beta - {1}", VersionString, GitCommitHash);
            else
                Text += String.Format("Unofficial release ({0} - {1})", VersionString, GitCommitHash);
        }

        #endregion

        #region Capture & Log Loading

        private void LoadLogAsync(string filename, bool temporary)
        {
            if (m_Core.LogLoading) return;

            string driver = "";
            bool support = StaticExports.SupportLocalReplay(filename, out driver);

            Thread thread = null;

            if (!m_Core.Config.ReplayHosts.ContainsKey(driver) && driver.Trim().Length > 0)
                m_Core.Config.ReplayHosts.Add(driver, "");

            // if driver is empty something went wrong loading the log, let it be handled as usual
            // below. Otherwise prompt to replay remotely.
            if (driver.Length > 0 && (!support || m_Core.Config.ReplayHosts[driver].Length > 0))
            {
                string remoteMessage = String.Format("This log was captured with {0}", driver);

                if(!support)
                    remoteMessage += " and cannot be replayed locally.\n";
                else
                    remoteMessage += " and your settings say to replay this remotely.\n";

                if (m_Core.Config.ReplayHosts[driver].Length == 0)
                    remoteMessage += "Do you wish to select a remote host to replay on?\n\n" +
                                  "You can set up a default host for this driver on the next screen.";
                else
                    remoteMessage += String.Format("Would you like to launch replay on remote host {0}?\n\n" +
                                                    "You can change this default via Tools -> Manage Replay Devices.", m_Core.Config.ReplayHosts[driver]);

                DialogResult res = MessageBox.Show(remoteMessage, "Launch remote replay?", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Exclamation);

                if (res == DialogResult.Yes)
                {
                    if (m_Core.Config.ReplayHosts[driver].Length == 0)
                    {
                        (new Dialogs.ReplayHostManager(m_Core, this)).ShowDialog();

                        if (m_Core.Config.ReplayHosts[driver].Length == 0)
                            return;
                    }

                    m_RemoteReplay = m_Core.Config.ReplayHosts[driver];

                    var plugins = renderdocplugin.PluginHelpers.GetPlugins();

                    // search plugins for to find manager for this driver and launch replay
                    foreach (var plugin in plugins)
                    {
                        var replayman = renderdocplugin.PluginHelpers.GetPluginInterface<renderdocplugin.ReplayManagerPlugin>(plugin);

                        if (replayman != null && replayman.GetTargetType() == driver)
                        {
                            var targets = replayman.GetOnlineTargets();

                            if (targets.Contains(m_RemoteReplay))
                            {
                                replayman.RunReplay(m_RemoteReplay);

                                // save replay connection so we can close replay
                                m_ReplayHost = replayman;
                                break;
                            }
                        }
                    }

                    thread = Helpers.NewThread(new ThreadStart(() =>
                    {
                        string[] drivers = new string[0];
                        try
                        {
                            var dummy = StaticExports.CreateRemoteReplayConnection(m_RemoteReplay);
                            drivers = dummy.RemoteSupportedReplays();
                            dummy.Shutdown();
                        }
                        catch (ApplicationException ex)
                        {
                            string errmsg = "Unknown error message";
                            if (ex.Data.Contains("status"))
                                errmsg = ((ReplayCreateStatus)ex.Data["status"]).Str();

                            MessageBox.Show(String.Format("Failed to fetch supported drivers on host {0}: {1}.\n\nCheck diagnostic log in Help menu for more details.", m_RemoteReplay, errmsg),
                                            "Error getting driver list", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }

                        // no drivers means we didn't connect
                        if (drivers.Length == 0)
                        {
                        }
                        else
                        {
                            bool found = false;
                            foreach (var d in drivers)
                                if (d == driver)
                                    found = true;

                            if (!found)
                            {
                                MessageBox.Show(String.Format("Remote host {0} doesn't support {1}.", m_RemoteReplay, driver),
                                                "Unsupported remote replay", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            }
                            else
                            {
                                string[] proxies = new string[0];
                                try
                                {
                                    var dummy = StaticExports.CreateRemoteReplayConnection("-");
                                    proxies = dummy.LocalProxies();
                                    dummy.Shutdown();
                                }
                                catch (ApplicationException ex)
                                {
                                    string errmsg = "Unknown error message";
                                    if (ex.Data.Contains("status"))
                                        errmsg = ((ReplayCreateStatus)ex.Data["status"]).Str();

                                    MessageBox.Show(String.Format("Failed to fetch local proxy drivers: {0}.\n\nCheck diagnostic log in Help menu for more details.", errmsg),
                                                    "Error getting driver list", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                }
                                if (proxies.Length > 0)
                                {
                                    m_Core.Config.LocalProxy = Helpers.Clamp(m_Core.Config.LocalProxy, 0, proxies.Length - 1);

                                    m_Core.LoadLogfile(m_Core.Config.LocalProxy, m_RemoteReplay, filename, temporary);
                                    if (m_Core.LogLoaded)
                                        return;
                                }
                            }
                        }

                        // clean up.
                        if (m_ReplayHost != null)
                            m_ReplayHost.CloseReplay();
                        m_RemoteReplay = "";

                        BeginInvoke(new Action(() =>
                        {
                            statusText.Text = "";
                            statusIcon.Image = null;
                            statusProgress.Visible = false;
                        }));
                    }));
                }
                else
                {
                    return;
                }
            }
            else
            {
                thread = Helpers.NewThread(new ThreadStart(() => m_Core.LoadLogfile(filename, temporary)));
            }
            
            thread.Start();

            m_Core.Config.LastLogPath = Path.GetDirectoryName(filename);

            statusText.Text = "Loading " + filename + "...";
        }

        private void PopulateRecentFiles()
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

        public void LoadLogfile(string fn, bool temporary)
        {
            if (PromptCloseLog())
                LoadLogAsync(fn, temporary);
        }

        public void CloseLogfile()
        {
            m_Core.CloseLogfile();

            saveLogToolStripMenuItem.Enabled = false;
        }

        public string GetSavePath()
        {
            DialogResult res = saveDialog.ShowDialog();

            if (res == DialogResult.OK)
                return saveDialog.FileName;

            return "";
        }

        private LiveCapture OnCaptureTrigger(string exe, string workingDir, string cmdLine, CaptureOptions opts)
        {
            if (!PromptCloseLog())
                return null;

            string logfile = m_Core.TempLogFilename(Path.GetFileNameWithoutExtension(exe));

            UInt32 ret = StaticExports.ExecuteAndInject(exe, workingDir, cmdLine, logfile, opts);

            if (ret == 0)
            {
                MessageBox.Show(string.Format("Error launching {0} for capture.\n\nCheck diagnostic log in Help menu for more details.", exe),
                                   "Error kicking capture", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return null;
            }

            var live = new LiveCapture(m_Core, "", ret, this);
            ShowLiveCapture(live);
            return live;
        }

        private LiveCapture OnInjectTrigger(UInt32 PID, string name, CaptureOptions opts)
        {
            if (!PromptCloseLog())
                return null;

            string logfile = m_Core.TempLogFilename(name);

            UInt32 ret = StaticExports.InjectIntoProcess(PID, logfile, opts);

            if (ret == 0)
            {
                MessageBox.Show(string.Format("Error injecting into process {0} for capture.\n\nCheck diagnostic log in Help menu for more details.", PID),
                                   "Error kicking capture", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return null;
            }

            var live = new LiveCapture(m_Core, "", ret, this);
            ShowLiveCapture(live);
            return live;
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
            (new Dialogs.RemoteHostSelect(m_Core, this)).ShowDialog();
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
                LoadLogAsync(openDialog.FileName, false);
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

        private void CheckUpdates()
        {
            if (!m_Core.Config.CheckUpdate_AllowChecks)
            {
                updateToolStripMenuItem.Text = "Update checks disabled";
                return;
            }
            
            if(!OfficialVersion && !BetaVersion)
                return;

            DateTime today = DateTime.Now;
            DateTime compare = today.AddDays(-2);

            if(compare.CompareTo(m_Core.Config.CheckUpdate_LastUpdate) < 0)
                return;

            m_Core.Config.CheckUpdate_LastUpdate = today;

            string versionCheck = VersionString;

            if (BetaVersion)
                versionCheck += String.Format("-{0}-beta", GitCommitHash.Substring(0, 8));

            var updateThread = Helpers.NewThread(new ThreadStart(() =>
            {
                // spawn thread to check update
                WebRequest g = HttpWebRequest.Create(String.Format("http://renderdoc.org/checkupdate/{0}", versionCheck));

                try
                {
                    var webresp = g.GetResponse();

                    using (var sr = new StreamReader(webresp.GetResponseStream()))
                    {
                        string response = sr.ReadToEnd();

                        if (response == "update")
                            BeginInvoke((MethodInvoker)delegate { m_Core.Config.CheckUpdate_UpdateAvailable = true; SetUpdateAvailable(); });
                    }

                    webresp.Close();
                }
                catch (WebException ex)
                {
                    StaticExports.LogText(String.Format("Problem checking for updates - {0}", ex.Message));
                    return;
                }
                catch (Exception)
                {
                    // just want to swallow the exception, checking for updates doesn't need to be handled
                    // and it's not worth trying to retry.
                    return;
                }
            }));

            updateThread.Start();
        }

        private bool PromptCloseLog()
        {
            string deletepath = "";

            if (OwnTemporaryLog)
            {
                string temppath = m_Core.LogFileName;

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
                    File.Delete(deletepath);
            }
            catch (System.Exception)
            {
                // can't delete it! maybe already deleted?
            }

            return true;
        }

        private bool PromptSaveLog()
        {
            DialogResult res = saveDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                if (!File.Exists(m_Core.LogFileName))
                {
                    MessageBox.Show("Logfile " + m_Core.LogFileName + " couldn't be found, cannot save.", "File not found",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }

                try
                {
                    // we copy the (possibly) temp log to the desired path, but the log item remains referring to the original path.
                    // This ensures that if the user deletes the saved path we can still open or re-save it.
                    File.Copy(m_Core.LogFileName, saveDialog.FileName, true);
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
                            m_Core.SetEventID(null, m_Core.CurFrame, eb.GetBookmark(i));

                            return true;
                        }
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

        private void CountDrawsDispatches(FetchDrawcall draw, ref int numDraws, ref int numDispatches)
        {
            if ((draw.flags & DrawcallFlags.Drawcall) != 0)
            {
                numDraws++;
            }
            if ((draw.flags & DrawcallFlags.Dispatch) != 0)
            {
                numDraws++;
                numDispatches++;
            }

            if(draw.children != null)
            {
                foreach (var d in draw.children)
                    CountDrawsDispatches(d, ref numDraws, ref numDispatches);
            }
        }

        private void logStatisticsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            long fileSize = (new FileInfo(m_Core.LogFileName)).Length;

            int firstIdx = 0;

            var firstDrawcall = m_Core.CurDrawcalls[firstIdx];
            while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                firstDrawcall = firstDrawcall.children[0];

            while (firstDrawcall.events.Length == 0)
            {
                if (firstDrawcall.next != null)
                {
                    firstDrawcall = firstDrawcall.next;
                    while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                        firstDrawcall = firstDrawcall.children[0];
                }
                else
                {
                    firstDrawcall = m_Core.CurDrawcalls[++firstIdx];
                    while (firstDrawcall.children != null && firstDrawcall.children.Length > 0)
                        firstDrawcall = firstDrawcall.children[0];
                }
            }

            UInt64 persistantData = (UInt64)fileSize - firstDrawcall.events[0].fileOffset;

            var lastDraw = m_Core.CurDrawcalls[m_Core.CurDrawcalls.Length - 1];
            while (lastDraw.children != null && lastDraw.children.Length > 0)
                lastDraw = lastDraw.children[lastDraw.children.Length - 1];

            uint numAPIcalls = lastDraw.eventID;

            int numDrawcalls = 0;
            int numDispatches = 0;

            foreach(var d in m_Core.CurDrawcalls)
                CountDrawsDispatches(d, ref numDrawcalls, ref numDispatches);

            int numTextures = m_Core.CurTextures.Length;
            int numBuffers = m_Core.CurBuffers.Length;

            ulong IBBytes = 0;
            ulong VBBytes = 0;
            ulong BufBytes = 0;
            foreach(var b in m_Core.CurBuffers)
            {
                BufBytes += b.byteSize;

                if((b.creationFlags & BufferCreationFlags.IB) != 0)
                    IBBytes += b.byteSize;
                if((b.creationFlags & BufferCreationFlags.VB) != 0)
                    VBBytes += b.byteSize;
            }

            ulong RTBytes = 0;
            ulong TexBytes = 0;
            ulong LargeTexBytes = 0;

            int numRTs = 0;
            float texW = 0, texH = 0;
            float largeTexW = 0, largeTexH = 0;
            int texCount = 0, largeTexCount = 0;
            foreach (var t in m_Core.CurTextures)
            {
                if ((t.creationFlags & (TextureCreationFlags.RTV|TextureCreationFlags.DSV)) != 0)
                {
                    numRTs++;

                    RTBytes += t.byteSize;
                }
                else
                {
                    texW += (float)t.width;
                    texH += (float)t.height;
                    texCount++;

                    TexBytes += t.byteSize;

                    if (t.width > 32 && t.height > 32)
                    {
                        largeTexW += (float)t.width;
                        largeTexH += (float)t.height;
                        largeTexCount++;

                        LargeTexBytes += t.byteSize;
                    }
                }
            }

            texW /= texCount;
            texH /= texCount;

            largeTexW /= largeTexCount;
            largeTexH /= largeTexCount;

            string msg =
                String.Format("Stats for {0}.\n\nFile size: {1:N2}MB\nPersistant Data (approx): {2:N2}MB\n\n",
                              Path.GetFileName(m_Core.LogFileName),
                              (float)fileSize / (1024.0f * 1024.0f), (float)persistantData / (1024.0f * 1024.0f)) +
                String.Format("Draw calls: {0} ({1} of them are dispatches)\nAPI calls: {2}\nAPI:Draw call ratio: {3}\n\n",
                              numDrawcalls, numDispatches, numAPIcalls, (float)numAPIcalls / (float)numDrawcalls) +
                String.Format("{0} Textures - {1:N2} MB ({2:N2} MB over 32x32), {3} RTs - {4:N2} MB.\nAvg. tex dimension: {5}x{6} ({7}x{8} over 32x32)\n",
                              numTextures, (float)TexBytes / (1024.0f * 1024.0f), (float)LargeTexBytes / (1024.0f * 1024.0f),
                              numRTs, (float)RTBytes / (1024.0f * 1024.0f),
                              texW, texH, largeTexW, largeTexH) +
                String.Format("{0} Buffers - {1:N2} MB total {2:N2} MB IBs {3:N2} MB VBs.\n",
                             numBuffers, (float)BufBytes / (1024.0f * 1024.0f), (float)IBBytes / (1024.0f * 1024.0f), (float)VBBytes / (1024.0f * 1024.0f)) +
                String.Format("{0} MB - Grand total GPU buffer + texture load", (float)(TexBytes + BufBytes + RTBytes) / (1024.0f * 1024.0f));

            MessageBox.Show(msg);
        }

        private void recentLogMenuItem_Click(object sender, EventArgs e)
        {
            ToolStripDropDownItem item = (ToolStripDropDownItem)sender;

            String filename = item.Text.Substring(3);

            if(File.Exists(filename))
            {
                LoadLogfile(filename, false);
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
                if (m_Core.CaptureDialog == null)
                    m_Core.CaptureDialog = new Dialogs.CaptureDialog(m_Core, OnCaptureTrigger, OnInjectTrigger);

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

        private void manageReplayDevicesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            (new Dialogs.ReplayHostManager(m_Core, this)).ShowDialog();
        }

        private void launchReplayHostToolStripMenuItem_Click(object sender, EventArgs e)
        {
            bool killReplay = false;

            Thread thread = Helpers.NewThread(new ThreadStart(() =>
            {
                StaticExports.SpawnReplayHost(ref killReplay);
            }));

            thread.Start();

            MessageBox.Show("Remote Replay is now running. Click OK to close.", "Remote replay",
                                               MessageBoxButtons.OK, MessageBoxIcon.Information);

            killReplay = true;
        }

        private void viewDocsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Help.ShowHelp(this, "renderdoc.chm", HelpNavigator.Topic, "html/b97b19f8-2b97-4dca-8a7a-ed7026eb43fe.htm");
            Help.ShowHelp(this, "renderdoc.chm");
        }

        private void updateToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Process.Start(String.Format("http://renderdoc.org/getupdate/{0}", VersionString));

            DateTime today = DateTime.Now;

            m_Core.Config.CheckUpdate_LastUpdate = today;
            m_Core.Config.CheckUpdate_UpdateAvailable = false;
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
            BufferViewer b = new BufferViewer(m_Core, true);

            b.InitFromPersistString("");

            b.Show(dockPanel);
        }

        private void timelineToolStripMenuItem_Click(object sender, EventArgs e)
        {
            m_Core.GetTimelineBar().Show(dockPanel);
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

            ModalPopup modal = new ModalPopup(SymbolResolveCallback, false);
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
                LoadLogfile(fn, false);
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
    }
}
