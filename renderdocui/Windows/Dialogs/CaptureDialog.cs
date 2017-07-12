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
using System.Text;
using System.Threading;
using System.IO;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using Microsoft.Win32;
using renderdocui.Code;
using renderdoc;

using Process = System.Diagnostics.Process;
using System.IO.Pipes;

namespace renderdocui.Windows.Dialogs
{
    public partial class CaptureDialog : DockContent
    {
        public class CaptureSettings
        {
            public CaptureOptions Options = StaticExports.GetDefaultCaptureOptions();
            public bool Inject = false;
            public bool AutoStart = false;
            public string Executable = "";
            public string WorkingDir = "";
            public string CmdLine = "";
            public EnvironmentModification[] Environment = new EnvironmentModification[0];
        }

        private class ProcessSorter : System.Collections.IComparer
        {
            public ProcessSorter(int col, SortOrder order)
            {
                Column = col;
                Sorting = order;
            }

            public int Compare(object x, object y)
            {
                ListViewItem a = x as ListViewItem;
                ListViewItem b = y as ListViewItem;

                if(a == null || b == null)
                    return -1;

                // PID
                if (Column == 1)
                {
                    int aPID = int.Parse(a.SubItems[Column].Text);
                    int bPID = int.Parse(b.SubItems[Column].Text);

                    if(aPID == bPID)
                        return 0;

                    if(Sorting == SortOrder.Ascending)
                        return (aPID < bPID) ? 1 : -1;
                    else
                        return (aPID < bPID) ? -1 : 1;
                }

                if (Sorting == SortOrder.Ascending)
                    return String.Compare(a.SubItems[Column].Text, b.SubItems[Column].Text);
                else
                    return -String.Compare(a.SubItems[Column].Text, b.SubItems[Column].Text);
            }

            public int Column;
            public SortOrder Sorting;
        }

        ProcessSorter m_ProcessSorter = new ProcessSorter(0, SortOrder.Ascending);

        private EnvironmentModification[] m_EnvModifications = new EnvironmentModification[0];

        private bool workDirHint = true;
        private bool processFilterHint = true;

        private Core m_Core;

        private void SetSettings(CaptureSettings settings)
        {
            InjectMode = settings.Inject;

            workDirPath_Enter(null, null);

            exePath.Text = settings.Executable;
            workDirPath.Text = settings.WorkingDir;
            cmdline.Text = settings.CmdLine;

            SetEnvironmentModifications(settings.Environment);

            workDirPath_Leave(null, null);

            AllowFullscreen.Checked = settings.Options.AllowFullscreen;
            AllowVSync.Checked = settings.Options.AllowVSync;
            HookIntoChildren.Checked = settings.Options.HookIntoChildren;
            CaptureCallstacks.Checked = settings.Options.CaptureCallstacks;
            CaptureCallstacksOnlyDraws.Checked = settings.Options.CaptureCallstacksOnlyDraws;
            APIValidation.Checked = settings.Options.APIValidation;
            RefAllResources.Checked = settings.Options.RefAllResources;
            SaveAllInitials.Checked = settings.Options.SaveAllInitials;
            DelayForDebugger.Value = settings.Options.DelayForDebugger;
            VerifyMapWrites.Checked = settings.Options.VerifyMapWrites;
            AutoStart.Checked = settings.AutoStart;

            if (settings.AutoStart)
            {
                TriggerCapture();
            }
        }

        private string RealWorkDir
        {
            get
            {
                return workDirHint ? "" : workDirPath.Text;
            }
        }

        private CaptureSettings GetSettings()
        {
            var ret = new CaptureSettings();

            ret.Inject = InjectMode;

            ret.AutoStart = AutoStart.Checked;

            ret.Executable = exePath.Text;
            ret.WorkingDir = RealWorkDir;
            ret.CmdLine = cmdline.Text;

            ret.Environment = m_EnvModifications;

            ret.Options.AllowFullscreen = AllowFullscreen.Checked;
            ret.Options.AllowVSync = AllowVSync.Checked;
            ret.Options.HookIntoChildren = HookIntoChildren.Checked;
            ret.Options.CaptureCallstacks = CaptureCallstacks.Checked;
            ret.Options.CaptureCallstacksOnlyDraws = CaptureCallstacksOnlyDraws.Checked;
            ret.Options.APIValidation = APIValidation.Checked;
            ret.Options.RefAllResources = RefAllResources.Checked;
            ret.Options.SaveAllInitials = SaveAllInitials.Checked;
            ret.Options.CaptureAllCmdLists = CaptureAllCmdLists.Checked;
            ret.Options.DelayForDebugger = (uint)DelayForDebugger.Value;
            ret.Options.VerifyMapWrites = VerifyMapWrites.Checked;

            return ret;
        }

        private bool m_InjectMode = false;
        public bool InjectMode
        {
            get
            {
                return m_InjectMode;
            }

            set
            {
                m_InjectMode = value;

                if (m_InjectMode)
                {
                    processGroup.Visible = true;
                    programGroup.Visible = false;

                    globalGroup.Visible = false;

                    mainTableLayout.RowStyles[1].SizeType = SizeType.Percent;
                    mainTableLayout.RowStyles[1].Height = 100.0f;

                    launch.Text = "Inject";

                    FillProcessList();
                    
                    Text = "Inject into Process";
                }
                else
                {
                    processGroup.Visible = false;
                    programGroup.Visible = true;

                    mainTableLayout.RowStyles[1].SizeType = SizeType.Absolute;
                    mainTableLayout.RowStyles[1].Height = 1.0f;

                    globalGroup.Visible = m_Core.Config.AllowGlobalHook;

                    launch.Text = "Launch";
                    
                    Text = "Capture Executable";
                }
            }
        }

        public CaptureDialog(Core core, OnCaptureMethod captureCallback, OnInjectMethod injectCallback)
        {
            InitializeComponent();

            exePath.Font =
                workDirPath.Font =
                cmdline.Font = 
                pidList.Font = 
                core.Config.PreferredFont;

            Icon = global::renderdocui.Properties.Resources.icon;

            vulkanLayerWarn.Visible = !Helpers.CheckVulkanLayerRegistration();

            var defaults = new CaptureSettings();
            defaults.Inject = false;
            
            m_CaptureCallback = captureCallback;
            m_InjectCallback = injectCallback;

            m_Core = core;

            workDirHint = true;
            workDirPath.ForeColor = SystemColors.GrayText;

            processFilterHint = true;
            processFilter.ForeColor = SystemColors.GrayText;
            processFilter_Leave(processFilter, new EventArgs());

            m_ProcessSorter.Sorting = SortOrder.Ascending;
            pidList.ListViewItemSorter = m_ProcessSorter;

            SetSettings(defaults);

            UpdateGlobalHook();
        }

        #region Callbacks

        public delegate void OnConnectionEstablishedMethod(LiveCapture live);

        public delegate void OnCaptureMethod(string exe, string workingDir, string cmdLine, EnvironmentModification[] env, CaptureOptions opts, OnConnectionEstablishedMethod cb);
        public delegate void OnInjectMethod(UInt32 PID, EnvironmentModification[] env, string name, CaptureOptions opts, OnConnectionEstablishedMethod cb);

        private OnCaptureMethod m_CaptureCallback = null;
        private OnInjectMethod m_InjectCallback = null;

        #endregion

        #region Dialog filling

        private void SaveSettings(string filename)
        {
            try
            {
                System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(CaptureSettings));
                StreamWriter writer = File.CreateText(filename);
                xs.Serialize(writer, GetSettings());
                writer.Flush();
                writer.Close();
            }
            catch (System.IO.IOException ex)
            {
                // Can't recover, but let user know that we couldn't save their settings.
                MessageBox.Show(String.Format("Error saving config file: {1}\n{0}", filename, ex.Message));
            }
        }

        public void LoadSettings(string filename)
        {
            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(CaptureSettings));
            try
            {
                StreamReader reader = File.OpenText(filename);
                SetSettings((CaptureSettings)xs.Deserialize(reader));
                reader.Close();
            }
            catch (System.Xml.XmlException)
            {
            }
            catch (System.Exception)
            {
                MessageBox.Show(String.Format("Failed to load capture settings from file {0}", filename), "Failed to load settings", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void AutoSizeLastColumn(ListView view)
        {
            // magic -2 value indicates it should fill to fit the width
            view.Columns[view.Columns.Count - 1].Width = -2;
        }

        private void FillProcessList()
        {
            Process[] processes = Process.GetProcesses();
            
            AutoSizeLastColumn(pidList);

            pidList.BeginUpdate();

            pidList.Items.Clear();
            foreach (var p in processes)
            {
                string[] values = new string[] { p.ProcessName, p.Id.ToString(), p.MainWindowTitle };

                if (!processFilterHint && processFilter.Text.Length != 0)
                {
                    bool match = false;

                    foreach(var v in values)
                        if (v.IndexOf(processFilter.Text, StringComparison.InvariantCultureIgnoreCase) >= 0)
                            match = true;

                    if(!match)
                        continue;
                }

                var item = new ListViewItem(values);
                item.Tag = (UInt32)p.Id;

                pidList.Items.Add(item);
            }

            pidList.EndUpdate();

            pidList.SelectedIndices.Clear();
            if(pidList.Items.Count > 0)
                pidList.SelectedIndices.Add(0);
        }

        private void pidList_ColumnClick(object sender, ColumnClickEventArgs e)
        {
            if(e.Column == m_ProcessSorter.Column)
            {
                if (m_ProcessSorter.Sorting == SortOrder.Ascending)
                    m_ProcessSorter.Sorting = SortOrder.Descending;
                else
                    m_ProcessSorter.Sorting = SortOrder.Ascending;
            }
            else
            {
                m_ProcessSorter = new ProcessSorter(e.Column, m_ProcessSorter.Sorting);
                pidList.ListViewItemSorter = m_ProcessSorter;
            }

            pidList.Sort();
        }

        private void pidList_Resize(object sender, EventArgs e)
        {
            AutoSizeLastColumn(pidList);
        }

        private void processFilter_Enter(object sender, EventArgs e)
        {
            if (processFilterHint)
            {
                processFilter.ForeColor = SystemColors.WindowText;
                processFilter.Text = "";
            }

            processFilterHint = false;
        }

        private void processFilter_Leave(object sender, EventArgs e)
        {
            if (processFilter.Text.Length == 0)
            {
                processFilterHint = true;
                processFilter.ForeColor = SystemColors.GrayText;
                processFilter.Text = "Filter process list by PID or name";
            }
        }

        private void processFilter_TextChanged(object sender, EventArgs e)
        {
            FillProcessList();
        }

        #endregion

        #region Capture execution

        private void OnCapture()
        {
            string exe = exePath.Text;

            // for non-remote captures, check the executable locally
            if (m_Core.Renderer.Remote == null)
            {
                if (!File.Exists(exe))
                {
                    MessageBox.Show("Invalid application executable: " + exe, "Invalid executable", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
            }

            string workingDir = "";

            // for non-remote captures, check the directory locally
            if (m_Core.Renderer.Remote == null)
            {
                if (Directory.Exists(RealWorkDir))
                    workingDir = RealWorkDir;
            }
            else
            {
                workingDir = RealWorkDir;
            }

            string cmdLine = cmdline.Text;

            m_CaptureCallback(exe, workingDir, cmdLine, GetSettings().Environment, GetSettings().Options, (LiveCapture live) =>
            {
                if (queueFrameCap.Checked)
                    live.QueueCapture((int)queuedCapFrame.Value);
            });
        }

        private void OnInject()
        {
            if (pidList.SelectedItems.Count == 1)
            {
                var item = pidList.SelectedItems[0];

                string name = item.SubItems[1].Text;
                UInt32 PID = (UInt32)item.Tag;

                m_InjectCallback(PID, GetSettings().Environment, name, GetSettings().Options, (LiveCapture live) =>
                {
                    if (queueFrameCap.Checked)
                        live.QueueCapture((int)queuedCapFrame.Value);
                });
            }
        }

        private void TriggerCapture()
        {
            if(InjectMode && m_InjectCallback != null)
                OnInject();

            if (!InjectMode && m_CaptureCallback != null)
                OnCapture();
        }

        #endregion

        #region Handlers

        private string ValidData(IDataObject d)
        {
            var fmts = new List<string>(d.GetFormats());

            if (fmts.Contains("FileName"))
            {
                var data = d.GetData("FileName") as Array;

                if (data != null && data.Length == 1 && data.GetValue(0) is string)
                {
                    var filename = (string)data.GetValue(0);

                    return Path.GetFullPath(filename);
                }
            }

            return "";
        }

        private void exeBrowse_Click(object sender, EventArgs e)
        {
            try
            {
                if (exePath.Text.Length > 0 && Directory.Exists(Path.GetDirectoryName(exePath.Text)))
                {
                    exeBrowser.InitialDirectory = Path.GetDirectoryName(exePath.Text);
                }
                else if (m_Core.Config.LastCapturePath.Length > 0)
                {
                    if (m_Core.Config.LastCaptureExe.Length > 0)
                    {
                        exeBrowser.FileName = m_Core.Config.LastCaptureExe;
                        exeBrowser.InitialDirectory = m_Core.Config.LastCapturePath;
                    }
                    else
                    {
                        exeBrowser.InitialDirectory = m_Core.Config.LastCapturePath;
                    }
                }
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }

            if (m_Core.Renderer.Remote == null)
            {
                exeBrowser.ShowDialog();
            }
            else
            {
                VirtualOpenFileDialog remoteBrowser = new VirtualOpenFileDialog(m_Core.Renderer);
                remoteBrowser.Opened += new EventHandler<FileOpenedEventArgs>(this.virtualExeBrowser_Opened);

                remoteBrowser.ShowDialog(this);
            }
        }

        private void exeBrowser_FileOk(object sender, CancelEventArgs e)
        {
            SetExecutableFilename(exeBrowser.FileName);
        }

        private void virtualExeBrowser_Opened(object sender, FileOpenedEventArgs e)
        {
            exePath.Text = e.FileName;

            UpdateWorkDirHint();
        }

        private void exePath_DragEnter(object sender, DragEventArgs e)
        {
            if (ValidData(e.Data).Length > 0)
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;
        }

        private void exePath_DragDrop(object sender, DragEventArgs e)
        {
            string fn = ValidData(e.Data);
            if (fn.Length > 0)
            {
                SetExecutableFilename(fn);
            }
        }

        private void workDirBrowse_Click(object sender, EventArgs e)
        {
            try
            {
                if (Directory.Exists(workDirPath.Text))
                    workDirBrowser.SelectedPath = workDirPath.Text;
                else if (Directory.Exists(Path.GetDirectoryName(exePath.Text)))
                    workDirBrowser.SelectedPath = Path.GetDirectoryName(exePath.Text);
                else if (m_Core.Config.LastCapturePath.Length > 0)
                    exeBrowser.InitialDirectory = m_Core.Config.LastCapturePath;
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }

            if (m_Core.Renderer.Remote == null)
            {
                var res = workDirBrowser.ShowDialog();

                if (res == DialogResult.Yes || res == DialogResult.OK)
                {
                    workDirPath.Text = workDirBrowser.SelectedPath;
                    workDirHint = false;
                    workDirPath.ForeColor = SystemColors.WindowText;
                }
            }
            else
            {
                VirtualOpenFileDialog remoteBrowser = new VirtualOpenFileDialog(m_Core.Renderer);
                remoteBrowser.Opened += new EventHandler<FileOpenedEventArgs>(this.virtualWorkDirBrowser_Opened);

                remoteBrowser.DirectoryBrowse = true;

                remoteBrowser.ShowDialog(this);
            }
        }

        private void virtualWorkDirBrowser_Opened(object sender, FileOpenedEventArgs e)
        {
            workDirPath.Text = e.FileName;
            workDirHint = false;
            workDirPath.ForeColor = SystemColors.WindowText;
        }

        private void setEnv_Click(object sender, EventArgs e)
        {
            EnvironmentEditor envEditor = new EnvironmentEditor();

            foreach (var mod in m_EnvModifications)
                envEditor.AddModification(mod, true);

            DialogResult res = envEditor.ShowDialog(this);

            if (res == DialogResult.OK)
                SetEnvironmentModifications(envEditor.Modifications);
        }

        private void SetEnvironmentModifications(EnvironmentModification[] modifications)
        {
            m_EnvModifications = modifications;

            string envModText = "";

            foreach (var mod in modifications)
            {
                if (envModText != "")
                    envModText += ", ";

                envModText += mod.GetDescription();
            }

            environmentDisplay.Text = envModText;
        }

        private void workDirPath_TextChanged(object sender, EventArgs e)
        {
            if(Directory.Exists(workDirPath.Text))
                workDirBrowser.SelectedPath = workDirPath.Text;
        }

        private void launch_Click(object sender, EventArgs e)
        {
            TriggerCapture();
        }

        private void close_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void pidRefresh_Click(object sender, EventArgs e)
        {
            FillProcessList();
        }

        private void save_Click(object sender, EventArgs e)
        {
            DialogResult res = saveDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                SaveSettings(saveDialog.FileName);
                m_Core.Config.AddRecentFile(m_Core.Config.RecentCaptureSettings, saveDialog.FileName, 10);
            }
        }

        private void load_Click(object sender, EventArgs e)
        {
            DialogResult res = loadDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                LoadSettings(loadDialog.FileName);
                m_Core.Config.AddRecentFile(m_Core.Config.RecentCaptureSettings, loadDialog.FileName, 10);
            }
        }

        private void CaptureCallstacks_CheckedChanged(object sender, EventArgs e)
        {
            if (CaptureCallstacks.Checked)
            {
                CaptureCallstacksOnlyDraws.Enabled = true;
            }
            else
            {
                CaptureCallstacksOnlyDraws.Checked = false;
                CaptureCallstacksOnlyDraws.Enabled = false;
            }
        }

        private void CaptureDialog_Shown(object sender, EventArgs e)
        {
            vulkanLayerWarn.Visible = !Helpers.CheckVulkanLayerRegistration();
        }

        #endregion

        private void capOptsGroup_Layout(object sender, LayoutEventArgs e)
        {
            capOptsFlow.MaximumSize = new Size(capOptsGroup.ClientRectangle.Width - 8, 0);
        }

        private void actionsGroup_Layout(object sender, LayoutEventArgs e)
        {
            actionsFlow.MaximumSize = new Size(actionsGroup.ClientRectangle.Width - 8, 0);
        }

        private void globalGroup_Layout(object sender, LayoutEventArgs e)
        {
            globalFlow.MaximumSize = new Size(globalGroup.ClientRectangle.Width - 8, 0);
        }

        private void workDirPath_Enter(object sender, EventArgs e)
        {
            if (workDirHint)
            {
                workDirPath.ForeColor = SystemColors.WindowText;
                workDirPath.Text = "";
            }

            workDirHint = false;
        }

        private void workDirPath_Leave(object sender, EventArgs e)
        {
            if (workDirPath.Text.Length == 0)
            {
                workDirHint = true;
                workDirPath.ForeColor = SystemColors.GrayText;

                UpdateWorkDirHint();
            }
        }

        private void exePath_TextChanged(object sender, EventArgs e)
        {
            // This is likely due to someone pasting a full path copied using
            // copy path. Removing the quotes is safe in any case
            if (exePath.Text.StartsWith ("\"") && exePath.Text.EndsWith ("\"") && exePath.Text.Length > 2)
            {
                exePath.Text = exePath.Text.Substring(1, exePath.Text.Length - 2);
            }

            UpdateWorkDirHint();
            UpdateGlobalHook();
        }

        private void UpdateWorkDirHint()
        {
            if (workDirHint == false) return;

            if (exePath.Text.Length == 0)
            {
                workDirPath.Text = "";
                return;
            }

            try
            {
                workDirPath.Text = Path.GetDirectoryName(exePath.Text);
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }

            // if it's a unix style path, maintain the slash type
            if (Helpers.CharCount(exePath.Text, '/') > Helpers.CharCount(exePath.Text, '\\'))
            {
                workDirPath.Text = workDirPath.Text.Replace('\\', '/');
            }
        }

        public void UpdateGlobalHook()
        {
            globalGroup.Visible = !InjectMode && m_Core.Config.AllowGlobalHook;

            if (exePath.Text.Length >= 4)
            {
                toggleGlobalHook.Enabled = true;
                globalLabel.Text = "Global hooking is risky!" + Environment.NewLine + Environment.NewLine +
                    "Be sure you know what you're doing.";

                if (toggleGlobalHook.Checked)
                    globalLabel.Text += Environment.NewLine + "Emergency restore @ %TEMP%\\RenderDoc_RestoreGlobalHook.reg";
            }
            else
            {
                toggleGlobalHook.Enabled = false;
                globalLabel.Text = "Global hooking requires an executable path, or filename";
            }
        }

        public void SetExecutableFilename(string filename)
        {
            filename = Path.GetFullPath(filename);

            exePath.Text = filename;

            UpdateWorkDirHint();

            m_Core.Config.LastCapturePath = Path.GetDirectoryName(filename);
            m_Core.Config.LastCaptureExe = Path.GetFileName(filename);
        }
        
        private void toggleGlobalHook_CheckedChanged(object sender, EventArgs e)
        {
            if (!toggleGlobalHook.Enabled)
                return;

            toggleGlobalHook.Enabled = false;

            if (toggleGlobalHook.Checked)
            {
                if(!Helpers.IsElevated)
                {
                    DialogResult res = MessageBox.Show("RenderDoc needs to restart with admin privileges. Restart?", "Restart as admin", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);

                    if (res == DialogResult.Yes)
                    {
                        string capfile = Path.GetTempFileName() + ".cap";

                        AutoStart.Checked = false;

                        SaveSettings(capfile);

                        var process = new Process();
                        process.StartInfo = new System.Diagnostics.ProcessStartInfo(Application.ExecutablePath, capfile);
                        process.StartInfo.Verb = "runas";
                        try
                        {
                            process.Start();
                        }
                        catch (Exception)
                        {
                            // don't restart if it failed for some reason (e.g. user clicked no to UAC)
                            toggleGlobalHook.Checked = false;
                            toggleGlobalHook.Enabled = true;
                            return;
                        }

                        m_Core.Config.Serialize(Core.ConfigFilename);
                        m_Core.Config.ReadOnly = true;
                        m_Core.AppWindow.Close();
                        return;
                    }
                    else
                    {
                        toggleGlobalHook.Checked = false;
                        toggleGlobalHook.Enabled = true;
                        return;
                    }
                }

                exePath.Enabled = exeBrowse.Enabled =
                    workDirPath.Enabled = workDirBrowse.Enabled =
                    cmdline.Enabled =
                    launch.Enabled = save.Enabled = load.Enabled = false;

                foreach (Control c in capOptsFlow.Controls)
                    c.Enabled = false;

                foreach (Control c in actionsFlow.Controls)
                    c.Enabled = false;

                toggleGlobalHook.Text = "Disable Global Hook";

                if (StaticExports.IsGlobalHookActive())
                    StaticExports.StopGlobalHook();

                string exe = exePath.Text;

                string logfile = exe;
                if (logfile.Contains("/")) logfile = logfile.Substring(logfile.LastIndexOf('/') + 1);
                if (logfile.Contains("\\")) logfile = logfile.Substring(logfile.LastIndexOf('\\') + 1);
                if (logfile.Contains(".")) logfile = logfile.Substring(0, logfile.IndexOf('.'));
                logfile = m_Core.TempLogFilename(logfile);

                bool success = StaticExports.StartGlobalHook(exe, logfile, GetSettings().Options);

                if(!success)
                {
                    // tidy up and exit
                    MessageBox.Show("Aborting. Couldn't start global hook. Check diagnostic log in help menu for more information",
                                    "Couldn't start global hook",
                                    MessageBoxButtons.OK, MessageBoxIcon.Error);

                    exePath.Enabled = exeBrowse.Enabled =
                        workDirPath.Enabled = workDirBrowse.Enabled =
                        cmdline.Enabled =
                        launch.Enabled = save.Enabled = load.Enabled = true;

                    foreach (Control c in capOptsFlow.Controls)
                        c.Enabled = true;

                    foreach (Control c in actionsFlow.Controls)
                        c.Enabled = true;

                    // won't recurse because it's not enabled yet
                    toggleGlobalHook.Checked = false;
                    toggleGlobalHook.Text = "Enable Global Hook";

                    toggleGlobalHook.Enabled = true;
                    return;
                }
            }
            else
            {
                if (StaticExports.IsGlobalHookActive())
                    StaticExports.StopGlobalHook();

                exePath.Enabled = exeBrowse.Enabled =
                    workDirPath.Enabled = workDirBrowse.Enabled =
                    cmdline.Enabled =
                    launch.Enabled = save.Enabled = load.Enabled = true;

                foreach (Control c in capOptsFlow.Controls)
                    c.Enabled = true;

                foreach (Control c in actionsFlow.Controls)
                    c.Enabled = true;

                toggleGlobalHook.Text = "Enable Global Hook";
            }

            toggleGlobalHook.Enabled = true;

            UpdateGlobalHook();
        }

        private void CaptureDialog_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (toggleGlobalHook.Checked)
                toggleGlobalHook.Checked = false;
        }

        private void vulkanLayerWarn_Click(object sender, EventArgs e)
        {
            string caption = "Configure Vulkan layer settings in registry?";

            bool hasOtherJSON = false;
            bool thisRegistered = false;
            string[] otherJSONs = new string[] {};

            Helpers.CheckVulkanLayerRegistration(out hasOtherJSON, out thisRegistered, out otherJSONs);

            string myJSON = Helpers.GetVulkanJSONPath(false);

            string msg = "Vulkan capture happens through the API's layer mechanism. RenderDoc has detected that ";

            if (hasOtherJSON)
            {
                msg += "there " + (otherJSONs.Length > 1 ? "are other RenderDoc builds" : "is another RenderDoc build") +
                    " registered already. " + (otherJSONs.Length > 1 ? "They" : "It") +
                    " must be disabled so that capture can happen without nasty clashes.";

                if (!thisRegistered)
                    msg += " Also ";
            }

            if (!thisRegistered)
            {
                msg += "the layer for this installation is not yet registered. This could be due to an " +
                    "upgrade from a version that didn't support Vulkan, or if this version is just a loose unzip/dev build.";
            }

            msg += "\n\nWould you like to proceed with the following changes?\n\n";

            if (hasOtherJSON)
            {
                foreach (var j in otherJSONs)
                    msg += "Unregister: " + j + "\n";

                msg += "\n";
            }

            if (!thisRegistered)
            {
                msg += "Register: " + myJSON + "\n";
                if (Environment.Is64BitProcess)
                    msg += "Register: " + Helpers.GetVulkanJSONPath(true) + "\n";
                msg += "\n";
            }

            msg += "This is a one-off change to the registry, it won't be needed again unless the installation moves.";
            
            DialogResult res = MessageBox.Show(msg, caption, MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);

            if (res == DialogResult.Yes)
            {
                Helpers.RegisterVulkanLayer();

                vulkanLayerWarn.Visible = !Helpers.CheckVulkanLayerRegistration();
            }
        }

        private void mainTableLayout_Layout(object sender, LayoutEventArgs e)
        {
            // bit of a hack to stop the table layout completely breaking.
            // in InjectMode make sure the main table layout doesn't get small enough to
            // reduce the processGroup to its minimum size
            if (InjectMode)
            {
                int margin = processGroup.ClientRectangle.Height - (processGroup.MinimumSize.Height + 20);

                if (processGroup.ClientRectangle.Height < processGroup.MinimumSize.Height + 20)
                    margin = 0;

                mainTableLayout.MinimumSize = new Size(0, mainTableLayout.ClientRectangle.Height - margin);
            }
        }

        private void pidList_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            TriggerCapture();
        }
    }
}
