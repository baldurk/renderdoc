﻿/******************************************************************************
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
            public CaptureOptions Options = CaptureOptions.Defaults;
            public bool Inject = false;
            public bool AutoStart = false;
            public string Executable = "";
            public string WorkingDir = "";
            public string CmdLine = "";
        }

        private bool workDirHint = true;

        private Core m_Core;

        private void SetSettings(CaptureSettings settings)
        {
            InjectMode = settings.Inject;

            workDirPath_Enter(null, null);

            exePath.Text = settings.Executable;
            workDirPath.Text = settings.WorkingDir;
            cmdline.Text = settings.CmdLine;

            workDirPath_Leave(null, null);

            AllowFullscreen.Checked = settings.Options.AllowFullscreen;
            AllowVSync.Checked = settings.Options.AllowVSync;
            HookIntoChildren.Checked = settings.Options.HookIntoChildren;
            CaptureCallstacks.Checked = settings.Options.CaptureCallstacks;
            CaptureCallstacksOnlyDraws.Checked = settings.Options.CaptureCallstacksOnlyDraws;
            DebugDeviceMode.Checked = settings.Options.DebugDeviceMode;
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

            ret.Options.AllowFullscreen = AllowFullscreen.Checked;
            ret.Options.AllowVSync = AllowVSync.Checked;
            ret.Options.HookIntoChildren = HookIntoChildren.Checked;
            ret.Options.CaptureCallstacks = CaptureCallstacks.Checked;
            ret.Options.CaptureCallstacksOnlyDraws = CaptureCallstacksOnlyDraws.Checked;
            ret.Options.DebugDeviceMode = DebugDeviceMode.Checked;
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

                    capture.Text = "Inject";

                    FillProcessList();
                    
                    Text = "Inject into Process";
                }
                else
                {
                    processGroup.Visible = false;
                    programGroup.Visible = true;

                    globalGroup.Visible = m_Core.Config.AllowGlobalHook;

                    capture.Text = "Capture";
                    
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

            var defaults = new CaptureSettings();
            defaults.Inject = false;
            
            m_CaptureCallback = captureCallback;
            m_InjectCallback = injectCallback;

            m_Core = core;

            workDirHint = true;
            workDirPath.ForeColor = SystemColors.GrayText;

            SetSettings(defaults);

            UpdateGlobalHook();
        }

        #region Callbacks

        public delegate LiveCapture OnCaptureMethod(string exe, string workingDir, string cmdLine, CaptureOptions opts);
        public delegate LiveCapture OnInjectMethod(UInt32 PID, string name, CaptureOptions opts);

        private OnCaptureMethod m_CaptureCallback = null;
        private OnInjectMethod m_InjectCallback = null;

        #endregion

        #region Dialog filling

        private void SaveSettings(string filename)
        {
            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(CaptureSettings));
            StreamWriter writer = File.CreateText(filename);
            xs.Serialize(writer, GetSettings());
            writer.Flush();
            writer.Close();
        }

        public void LoadSettings(string filename)
        {
            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(CaptureSettings));
            StreamReader reader = File.OpenText(filename);
            try
            {
                SetSettings((CaptureSettings)xs.Deserialize(reader));
            }
            catch (System.Xml.XmlException)
            {
            }
            catch (System.InvalidOperationException)
            {
                MessageBox.Show(String.Format("Failed to load capture settings from file {0}", filename), "Failed to load settings", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            
            reader.Close();
        }

        private void FillProcessList()
        {
            Process[] processes = Process.GetProcesses();

            Array.Sort(processes, (a, b) => String.Compare(a.ProcessName, b.ProcessName));
            
            // magic -2 value indicates it should fill to fit the width
            pidList.Columns[pidList.Columns.Count - 1].Width = -2;

            pidList.Items.Clear();
            foreach (var p in processes)
            {
                var item = new ListViewItem(new string[] { p.Id.ToString(), p.ProcessName });
                item.Tag = (UInt32)p.Id;
                pidList.Items.Add(item);
            }

            pidList.SelectedIndices.Clear();
            pidList.SelectedIndices.Add(0);
        }

        #endregion

        #region Capture execution

        private void OnCapture()
        {
            string exe = exePath.Text;
            if (!File.Exists(exe))
            {
                MessageBox.Show("Invalid application executable: " + exe, "Invalid executable", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            string workingDir = "";
            if (Directory.Exists(RealWorkDir))
                workingDir = RealWorkDir;

            string cmdLine = cmdline.Text;

            var live = m_CaptureCallback(exe, workingDir, cmdLine, GetSettings().Options);

            if (queueFrameCap.Checked && live != null)
                live.QueueCapture((int)queuedCapFrame.Value);
        }

        private void OnInject()
        {
            if (pidList.SelectedItems.Count == 1)
            {
                var item = pidList.SelectedItems[0];

                string name = item.SubItems[1].Text;
                UInt32 PID = (UInt32)item.Tag;

                var live = m_InjectCallback(PID, name, GetSettings().Options);

                if (queueFrameCap.Checked && live != null)
                    live.QueueCapture((int)queuedCapFrame.Value);
            }
        }

        private void TriggerCapture()
        {
            //(new LiveCapture()).Show(DockPanel);
            //return;

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

            exeBrowser.ShowDialog();
        }

        private void exeBrowser_FileOk(object sender, CancelEventArgs e)
        {
            exePath.Text = exeBrowser.FileName;

            UpdateWorkDirHint();

            m_Core.Config.LastCapturePath = Path.GetDirectoryName(exeBrowser.FileName);
            m_Core.Config.LastCaptureExe = Path.GetFileName(exeBrowser.FileName);
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
                exePath.Text = fn;

                UpdateWorkDirHint();

                m_Core.Config.LastCapturePath = Path.GetDirectoryName(exeBrowser.FileName);
                m_Core.Config.LastCaptureExe = Path.GetFileName(exeBrowser.FileName);
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

            var res = workDirBrowser.ShowDialog();

            if (res == DialogResult.Yes || res == DialogResult.OK)
            {
                workDirPath.Text = workDirBrowser.SelectedPath;
                workDirHint = false;
                workDirPath.ForeColor = SystemColors.WindowText;
            }
        }

        private void workDirPath_TextChanged(object sender, EventArgs e)
        {
            if(Directory.Exists(workDirPath.Text))
                workDirBrowser.SelectedPath = workDirPath.Text;
        }

        private void capture_Click(object sender, EventArgs e)
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

        private string prevAppInit = "";
        private string prevAppInitWoW64 = "";
        private int prevAppInitEnabled = 0;
        private int prevAppInitWoW64Enabled = 0;

        private AutoResetEvent wakeupEvent = new AutoResetEvent(false);
        private bool pipeExit = false;
        private Thread pipeThread = null;
        private NamedPipeServerStream pipe32 = null;
        private NamedPipeServerStream pipe64 = null;

        private void EnableAppInit(RegistryKey parent, string path, string dllname, out int prevEnabled, out string prevStr)
        {
            RegistryKey key = parent.OpenSubKey("Microsoft", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("Windows NT", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("CurrentVersion", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("Windows", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            object o = key.GetValue("LoadAppInit_DLLs");
            if (o == null || !(o is int)) { prevEnabled = 0; prevStr = ""; return; }
            prevEnabled = (int)o;

            o = key.GetValue("AppInit_DLLs");
            if (o == null || !(o is string)) { prevEnabled = 0; prevStr = ""; return; }
            prevStr = (string)o;

            key.SetValue("AppInit_DLLs", Win32PInvoke.ShortPath(Path.Combine(path, dllname)));
            key.SetValue("LoadAppInit_DLLs", (int)1);
        }

        private void RestoreAppInit(RegistryKey parent, int prevEnabled, string prevStr)
        {
            RegistryKey key = parent.OpenSubKey("Microsoft", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("Windows NT", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("CurrentVersion", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key = key.OpenSubKey("Windows", true);
            if (key == null) { prevEnabled = 0; prevStr = ""; return; }

            key.SetValue("AppInit_DLLs", prevStr);
            key.SetValue("LoadAppInit_DLLs", prevEnabled);
        }

        private void PipeTick()
        {
            while (!pipeExit)
            {
                wakeupEvent.WaitOne(250);
            }

            if (pipe32 != null)
            {
                if (pipe32.IsConnected)
                {
                    using (StreamWriter writer = new StreamWriter(pipe32))
                    {
                        writer.Write("exit");
                        writer.Flush();
                    }
                }

                pipe32.Dispose();
                pipe32 = null;
            }

            if (pipe64 != null)
            {
                if (pipe64.IsConnected)
                {
                    using (StreamWriter writer = new StreamWriter(pipe64))
                    {
                        writer.Write("exit");
                        writer.Flush();
                    }
                }

                pipe64.Dispose();
                pipe64 = null;
            }
        }

        private void ExitPipeThread()
        {
            pipeExit = true;
            wakeupEvent.Set();

            if (pipeThread != null)
            {
                if (pipeThread.ThreadState != ThreadState.Aborted &&
                    pipeThread.ThreadState != ThreadState.Stopped)
                {
                    // try to shut down gracefully
                    pipeThread.Join(1000);

                    if (pipeThread.ThreadState != ThreadState.Aborted &&
                        pipeThread.ThreadState != ThreadState.Stopped)
                    {
                        pipeThread.Abort();
                        pipeThread.Join();
                    }
                }
            }

            pipeThread = null;

            if (pipe32 != null)
            {
                pipe32.Dispose();
                pipe32 = null;
            }

            if (pipe64 != null)
            {
                pipe64.Dispose();
                pipe64 = null;
            }
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
                    capture.Enabled = save.Enabled = load.Enabled = false;

                foreach (Control c in capOptsFlow.Controls)
                    c.Enabled = false;

                foreach (Control c in actionsFlow.Controls)
                    c.Enabled = false;

                toggleGlobalHook.Text = "Disable Global Hook";

                var path = Path.GetDirectoryName(Path.GetFullPath(Application.ExecutablePath));

                var regfile = Path.Combine(Path.GetTempPath(), "RenderDoc_RestoreGlobalHook.reg");

                if (Environment.Is64BitProcess)
                {
                    EnableAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE").CreateSubKey("Wow6432Node"),
                                  path, "x86\\renderdocshim32.dll",
                                  out prevAppInitWoW64Enabled, out prevAppInitWoW64);

                    EnableAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE"),
                                  path, "renderdocshim64.dll",
                                  out prevAppInitEnabled, out prevAppInit);

                    using (FileStream s = File.OpenWrite(regfile))
                    {
                        using(StreamWriter sw = new StreamWriter(s))
                        {
                            sw.WriteLine("Windows Registry Editor Version 5.00");
                            sw.WriteLine("");
                            sw.WriteLine("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows]");
                            sw.WriteLine(String.Format("\"LoadAppInit_DLLs\"=dword:{0:X8}", prevAppInitWoW64Enabled));
                            sw.WriteLine(String.Format("\"AppInit_DLLs\"=\"{0}\"", prevAppInitWoW64));
                            sw.WriteLine("");
                            sw.WriteLine("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows]");
                            sw.WriteLine(String.Format("\"LoadAppInit_DLLs\"=dword:{0:X8}", prevAppInitEnabled));
                            sw.WriteLine(String.Format("\"AppInit_DLLs\"=\"{0}\"", prevAppInit));
                            sw.Flush();
                        }
                    }
                }
                else
                {
                    // if this is a 64-bit OS, it will re-direct our request to Wow6432Node anyway, so we
                    // don't need to handle that manually
                    EnableAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE"), path, "renderdocshim32.dll",
                                    out prevAppInitEnabled, out prevAppInit);

                    using (FileStream s = File.OpenWrite(regfile))
                    {
                        using (StreamWriter sw = new StreamWriter(s))
                        {
                            sw.WriteLine("Windows Registry Editor Version 5.00");
                            sw.WriteLine("");
                            sw.WriteLine("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows]");
                            sw.WriteLine(String.Format("\"LoadAppInit_DLLs\"=dword:{0:X8}", prevAppInitEnabled));
                            sw.WriteLine(String.Format("\"AppInit_DLLs\"=\"{0}\"", prevAppInit));
                            sw.Flush();
                        }
                    }
                }

                ExitPipeThread();

                pipeExit = false;

                pipe32 = new NamedPipeServerStream("RenderDoc.GlobalHookControl32");
                pipe64 = new NamedPipeServerStream("RenderDoc.GlobalHookControl64");

                pipeThread = Helpers.NewThread(new ThreadStart(PipeTick));

                pipeThread.Start();

                string exe = exePath.Text;

                string logfile = exe;
                if (logfile.Contains("/")) logfile = logfile.Substring(logfile.LastIndexOf('/') + 1);
                if (logfile.Contains("\\")) logfile = logfile.Substring(logfile.LastIndexOf('\\') + 1);
                if (logfile.Contains(".")) logfile = logfile.Substring(0, logfile.IndexOf('.'));
                logfile = m_Core.TempLogFilename(logfile);

                StaticExports.StartGlobalHook(exe, logfile, GetSettings().Options);
            }
            else
            {
                ExitPipeThread();

                exePath.Enabled = exeBrowse.Enabled =
                    workDirPath.Enabled = workDirBrowse.Enabled =
                    cmdline.Enabled =
                    capture.Enabled = save.Enabled = load.Enabled = true;

                foreach (Control c in capOptsFlow.Controls)
                    c.Enabled = true;

                foreach (Control c in actionsFlow.Controls)
                    c.Enabled = true;

                toggleGlobalHook.Text = "Enable Global Hook";

                if (Environment.Is64BitProcess)
                {
                    RestoreAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE").CreateSubKey("Wow6432Node"), prevAppInitWoW64Enabled, prevAppInitWoW64);
                    RestoreAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE"), prevAppInitEnabled, prevAppInit);
                }
                else
                {
                    // if this is a 64-bit OS, it will re-direct our request to Wow6432Node anyway, so we
                    // don't need to handle that manually
                    RestoreAppInit(Registry.LocalMachine.CreateSubKey("SOFTWARE"), prevAppInitEnabled, prevAppInit);
                }

                var regfile = Path.Combine(Path.GetTempPath(), "RenderDoc_RestoreGlobalHook.reg");

                if (File.Exists(regfile)) File.Delete(regfile);
            }

            toggleGlobalHook.Enabled = true;

            UpdateGlobalHook();
        }

        private void CaptureDialog_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (toggleGlobalHook.Checked)
                toggleGlobalHook.Checked = false;
        }
    }
}
