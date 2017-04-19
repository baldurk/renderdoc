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
using System.IO;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;

namespace renderdocui.Windows.Dialogs
{
    public partial class SettingsDialog : Form
    {
        Core m_Core = null;
        bool initialising = false;

        public SettingsDialog(Core c)
        {
            m_Core = c;

            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            pagesTree.BeginUpdate();
            foreach (TabPage t in settingsTabs.TabPages)
            {
                pagesTree.Nodes.Add(new object[] { t.Text }).Tag = t;
            }
            pagesTree.EndUpdate();
            pagesTree.FocusedNode = pagesTree.Nodes[0];

            saveDirectory.Text = m_Core.Config.DefaultCaptureSaveDirectory;
            tempDirectory.Text = m_Core.Config.TemporaryCaptureDirectory;

            externalDisassemblerEnabledCheckbox.Checked = m_Core.Config.ExternalDisassemblerEnabled;
            externalDisassemblerArgs.Text = m_Core.Config.GetDefaultExternalDisassembler().args;
            externalDisassemblePath.Text = m_Core.Config.GetDefaultExternalDisassembler().executable;
            adbPath.Text = m_Core.Config.AdbExecutablePath;
            maxConnectTimeout.Value = m_Core.Config.MaxConnectTimeout;

            TextureViewer_ResetRange.Checked = m_Core.Config.TextureViewer_ResetRange;
            TextureViewer_PerTexSettings.Checked = m_Core.Config.TextureViewer_PerTexSettings;
            ShaderViewer_FriendlyNaming.Checked = m_Core.Config.ShaderViewer_FriendlyNaming;
            CheckUpdate_AllowChecks.Checked = m_Core.Config.CheckUpdate_AllowChecks;
            Font_PreferMonospaced.Checked = m_Core.Config.Font_PreferMonospaced;

            AlwaysReplayLocally.Checked = m_Core.Config.AlwaysReplayLocally;

            AllowGlobalHook.Checked = m_Core.Config.AllowGlobalHook;
            
            {
                Type type = m_Core.Config.EventBrowser_TimeUnit.GetType();

                EventBrowser_TimeUnit.Items.Clear();

                foreach (int v in type.GetEnumValues())
                {
                    EventBrowser_TimeUnit.Items.Add(PersistantConfig.UnitPrefix((PersistantConfig.TimeUnit)v));
                }
            }

            EventBrowser_TimeUnit.SelectedIndex = (int)m_Core.Config.EventBrowser_TimeUnit;
            EventBrowser_HideEmpty.Checked = m_Core.Config.EventBrowser_HideEmpty;
            EventBrowser_HideAPICalls.Checked = m_Core.Config.EventBrowser_HideAPICalls;
            EventBrowser_ApplyColours.Checked = m_Core.Config.EventBrowser_ApplyColours;
            EventBrowser_ColourEventRow.Checked = m_Core.Config.EventBrowser_ColourEventRow;
            EventBrowser_AddFake.Checked = m_Core.Config.EventBrowser_AddFake;

            // disable sub-checkbox
            EventBrowser_ColourEventRow.Enabled = EventBrowser_ApplyColours.Checked;

            initialising = true;

            Formatter_MinFigures.Value = m_Core.Config.Formatter_MinFigures;
            Formatter_MaxFigures.Value = m_Core.Config.Formatter_MaxFigures;
            Formatter_NegExp.Value = m_Core.Config.Formatter_NegExp;
            Formatter_PosExp.Value = m_Core.Config.Formatter_PosExp;

            initialising = false;
        }

        private void ok_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void rdcAssoc_Click(object sender, EventArgs e)
        {
            Helpers.InstallRDCAssociation();
        }

        private void capAssoc_Click(object sender, EventArgs e)
        {
            Helpers.InstallCAPAssociation();
        }

        private void TextureViewer_ResetRange_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.TextureViewer_ResetRange = TextureViewer_ResetRange.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void TextureViewer_PerTexSettings_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.TextureViewer_PerTexSettings = TextureViewer_PerTexSettings.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void friendlyRegName_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.ShaderViewer_FriendlyNaming = ShaderViewer_FriendlyNaming.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }
        
        private void pagesTree_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if(pagesTree.FocusedNode != null)
                settingsTabs.SelectedTab = (pagesTree.FocusedNode.Tag as TabPage);
        }

        private void formatter_ValueChanged(object sender, EventArgs e)
        {
            if(initialising)
                return;

            m_Core.Config.Formatter_MinFigures = (int)Formatter_MinFigures.Value;
            m_Core.Config.Formatter_MaxFigures = (int)Formatter_MaxFigures.Value;
            m_Core.Config.Formatter_NegExp = (int)Formatter_NegExp.Value;
            m_Core.Config.Formatter_PosExp = (int)Formatter_PosExp.Value;

            m_Core.Config.SetupFormatting();

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void CheckUpdate_AllowChecks_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.CheckUpdate_AllowChecks = CheckUpdate_AllowChecks.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void Font_PreferMonospaced_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.Font_PreferMonospaced = Font_PreferMonospaced.Checked;

            m_Core.Config.SetupFormatting();

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void AllowGlobalHook_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.AllowGlobalHook = AllowGlobalHook.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);

            if (m_Core.CaptureDialog != null)
                m_Core.CaptureDialog.UpdateGlobalHook();
        }

        private void AlwaysReplayLocally_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.AlwaysReplayLocally = AlwaysReplayLocally.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_TimeUnit_SelectionChangeCommitted(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_TimeUnit = (PersistantConfig.TimeUnit)EventBrowser_TimeUnit.SelectedIndex;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_HideEmpty_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_HideEmpty = EventBrowser_HideEmpty.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_HideAPICalls_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_HideAPICalls = EventBrowser_HideAPICalls.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_ApplyColours_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_ApplyColours = EventBrowser_ApplyColours.Checked;

            // disable sub-checkbox
            EventBrowser_ColourEventRow.Enabled = EventBrowser_ApplyColours.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_ColourEventRow_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_ColourEventRow = EventBrowser_ColourEventRow.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void EventBrowser_AddFake_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.EventBrowser_AddFake = EventBrowser_AddFake.Checked;

            m_Core.Config.Serialize(Core.ConfigFilename);
        }

        private void tempDirectory_TextChanged(object sender, EventArgs e)
        {
            try
            {
                if (Directory.Exists(tempDirectory.Text))
                    m_Core.Config.TemporaryCaptureDirectory = tempDirectory.Text;
                else
                    m_Core.Config.TemporaryCaptureDirectory = "";

                m_Core.Config.Serialize(Core.ConfigFilename);
            }
            catch (Exception)
            {
            }
        }

        private void browseTempCaptureDirectory_Click(object sender, EventArgs e)
        {
            try
            {
                try
                {
                    if (Directory.Exists(m_Core.Config.TemporaryCaptureDirectory))
                        browserCaptureDialog.SelectedPath = m_Core.Config.TemporaryCaptureDirectory;
                    else
                        browserCaptureDialog.SelectedPath = Path.GetTempPath();
                }
                catch (Exception)
                {
                }
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }

            var res = browserCaptureDialog.ShowDialog();

            if (res == DialogResult.Yes || res == DialogResult.OK)
            {
                try
                {
                    m_Core.Config.TemporaryCaptureDirectory = browserCaptureDialog.SelectedPath;

                    m_Core.Config.Serialize(Core.ConfigFilename);
                }
                catch (Exception)
                {
                }
            }
        }

        private void saveDirectory_TextChanged(object sender, EventArgs e)
        {
            try
            {
                if (Directory.Exists(saveDirectory.Text) || saveDirectory.Text == "")
                    m_Core.Config.DefaultCaptureSaveDirectory = saveDirectory.Text;

                m_Core.Config.Serialize(Core.ConfigFilename);
            }
            catch (Exception)
            {
            }
        }

        private void browseSaveCaptureDirectory_Click(object sender, EventArgs e)
        {
            try
            {
                try
                {
                    if (Directory.Exists(m_Core.Config.DefaultCaptureSaveDirectory))
                        browserCaptureDialog.SelectedPath = m_Core.Config.DefaultCaptureSaveDirectory;
                    else
                        browserCaptureDialog.SelectedPath = "";
                }
                catch (Exception)
                {
                }
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }

            var res = browserCaptureDialog.ShowDialog();

            if (res == DialogResult.Yes || res == DialogResult.OK)
            {
                try
                {
                    m_Core.Config.DefaultCaptureSaveDirectory = browserCaptureDialog.SelectedPath;
                    saveDirectory.Text = browserCaptureDialog.SelectedPath;

                    m_Core.Config.Serialize(Core.ConfigFilename);
                }
                catch (Exception)
                {
                }
            }
        }

        private void chooseSearchPaths_Click(object sender, EventArgs e)
        {
            var editor = new OrderedListEditor(m_Core, "Shader debug info search paths", "Search Path", OrderedListEditor.Browsing.Folder);

            foreach (string path in m_Core.Config.GetConfigSetting("shader.debug.searchPaths").Split(';'))
                if(path.Trim() != "")
                    editor.AddItem(path);

            DialogResult res = editor.ShowDialog();

            if (res == DialogResult.OK)
                m_Core.Config.SetConfigSetting("shader.debug.searchPaths", String.Join(";", editor.GetItems()));
        }

        private void browseExtDisasemble_Click(object sender, EventArgs e)
        {
            var res = browseExtDisassembleDialog.ShowDialog();

            if (res == DialogResult.Yes || res == DialogResult.OK)
            {
                try
                {
                    m_Core.Config.GetDefaultExternalDisassembler().executable = browseExtDisassembleDialog.FileName;
                    externalDisassemblePath.Text = browseExtDisassembleDialog.FileName;
                }
                catch (Exception)
                {
                }
            }
        }

        private void externalDisassemblePath_TextChanged(object sender, EventArgs e)
        {
            try
            {
                m_Core.Config.GetDefaultExternalDisassembler().executable = externalDisassemblePath.Text;
            }
            catch (Exception)
            {
            }
        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {
            try
            {
                m_Core.Config.GetDefaultExternalDisassembler().args = externalDisassemblerArgs.Text;
            }
            catch (Exception)
            {
            }
        }

        private void externalDisassemblerEnabledCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            m_Core.Config.ExternalDisassemblerEnabled = externalDisassemblerEnabledCheckbox.Checked;
        }

        private void browseAdbPath_Click(object sender, EventArgs e)
        {
            var res = browseExtDisassembleDialog.ShowDialog();

            if (res == DialogResult.Yes || res == DialogResult.OK)
            {
                try
                {
                    adbPath.Text = browseExtDisassembleDialog.FileName;
                    m_Core.Config.AdbExecutablePath = adbPath.Text;
                }
                catch (Exception)
                {
                }
            }
        }

        private void adbPath_TextChanged(object sender, EventArgs e)
        {
            try
            {
                m_Core.Config.AdbExecutablePath = adbPath.Text;
            }
            catch (Exception)
            {
            }
        }

        private void maxConnectTimeout_ValueChanged(object sender, EventArgs e)
        {
            try
            {
                m_Core.Config.MaxConnectTimeout = (uint)maxConnectTimeout.Value;
            }
            catch (Exception)
            {
            }
        }
    }
}
