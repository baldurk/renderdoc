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
using System.Diagnostics;
using System.Text;
using System.IO;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;

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

            exePath.Text = settings.Executable;
            workDirPath.Text = settings.WorkingDir;
            cmdline.Text = settings.CmdLine;

            AllowFullscreen.Checked = settings.Options.AllowFullscreen;
            AllowVSync.Checked = settings.Options.AllowVSync;
            CacheStateObjects.Checked = settings.Options.CacheStateObjects;
            HookIntoChildren.Checked = settings.Options.HookIntoChildren;
            CaptureCallstacks.Checked = settings.Options.CaptureCallstacks;
            CaptureCallstacksOnlyDraws.Checked = settings.Options.CaptureCallstacksOnlyDraws;
            DebugDeviceMode.Checked = settings.Options.DebugDeviceMode;
            RefAllResources.Checked = settings.Options.RefAllResources;
            SaveAllInitials.Checked = settings.Options.SaveAllInitials;
            DelayForDebugger.Value = settings.Options.DelayForDebugger;
            AutoStart.Checked = settings.AutoStart;

            UpdateWorkDirHint();

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
            ret.Options.CacheStateObjects = CacheStateObjects.Checked;
            ret.Options.CaptureCallstacks = CaptureCallstacks.Checked;
            ret.Options.CaptureCallstacksOnlyDraws = CaptureCallstacksOnlyDraws.Checked;
            ret.Options.DebugDeviceMode = DebugDeviceMode.Checked;
            ret.Options.RefAllResources = RefAllResources.Checked;
            ret.Options.SaveAllInitials = SaveAllInitials.Checked;
            ret.Options.CaptureAllCmdLists = CaptureAllCmdLists.Checked;
            ret.Options.DelayForDebugger = (uint)DelayForDebugger.Value;

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

                    capture.Text = "Inject";

                    FillProcessList();
                    
                    Text = "Inject into Process";
                }
                else
                {
                    processGroup.Visible = false;
                    programGroup.Visible = true;

                    capture.Text = "Capture";
                    
                    Text = "Capture Executable";
                }
            }
        }

        public CaptureDialog(Core core, OnCaptureMethod captureCallback, OnInjectMethod injectCallback)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            var defaults = new CaptureSettings();
            defaults.Inject = false;
            
            m_CaptureCallback = captureCallback;
            m_InjectCallback = injectCallback;

            m_Core = core;

            workDirHint = true;
            workDirPath.ForeColor = SystemColors.GrayText;

            SetSettings(defaults);
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

            if (queueFrameCap.Checked)
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
                if (exePath.Text != "" && Directory.Exists(Path.GetDirectoryName(exePath.Text)))
                    exeBrowser.InitialDirectory = Path.GetDirectoryName(exePath.Text);
                else if (m_Core.Config.LastCapturePath != "")
                    exeBrowser.InitialDirectory = m_Core.Config.LastCapturePath;
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
        }

        private void exePath_DragEnter(object sender, DragEventArgs e)
        {
            if (ValidData(e.Data) != "")
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;
        }

        private void exePath_DragDrop(object sender, DragEventArgs e)
        {
            string fn = ValidData(e.Data);
            if (fn != "")
            {
                exePath.Text = fn;

                UpdateWorkDirHint();

                m_Core.Config.LastCapturePath = Path.GetDirectoryName(exeBrowser.FileName);
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
                else if (m_Core.Config.LastCapturePath != "")
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
            if (workDirPath.Text == "")
            {
                workDirHint = true;
                workDirPath.ForeColor = SystemColors.GrayText;

                UpdateWorkDirHint();
            }
        }

        private void exePath_TextChanged(object sender, EventArgs e)
        {
            UpdateWorkDirHint();
        }

        private void UpdateWorkDirHint()
        {
            if (workDirHint == false) return;

            if(exePath.Text == "")
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
    }
}
