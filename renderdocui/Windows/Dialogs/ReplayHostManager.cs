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
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    // currently not used until the implementation is completed, but this allows the user
    // to choose which host they'll use by default to replay on when they try and load a log
    // that uses an API not implemented in their build of renderdoc.
    //
    // It can also be used force remote replay even for an API that is locally supported - 
    // this can be used if e.g. you want to run on a remote machine that has a different IHV's
    // GPU in it to check for bugs or access different hardware profiling counters
    public partial class ReplayHostManager : Form
    {
        Core m_Core;
        List<ComboBox> m_Hosts = new List<ComboBox>();

        public ReplayHostManager(Core core, MainWindow main)
        {
            InitializeComponent();

            localProxy.Font = core.Config.PreferredFont;

            Icon = global::renderdocui.Properties.Resources.icon;

            m_Core = core;

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

            localProxy.Items.AddRange(proxies);

            m_Core.Config.LocalProxy = Helpers.Clamp(m_Core.Config.LocalProxy, 0, proxies.Length - 1);

            localProxy.SelectedIndex = m_Core.Config.LocalProxy;

            var driversTable = new TableLayoutPanel();

            driversTable.SuspendLayout();

            driversBox.Controls.Clear();
            driversBox.Controls.Add(driversTable);

            driversTable.ColumnCount = 2;
            driversTable.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 30F));
            driversTable.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 70F));

            driversTable.Dock = System.Windows.Forms.DockStyle.Fill;
            driversTable.RowCount = m_Core.Config.ReplayHosts.Count + 1;

            int row = 0;

            List<string> hosts = new List<string>();

            foreach (var kv in m_Core.Config.ReplayHosts)
            {
                var driver = kv.Key;

                var lab = new Label();
                lab.Anchor = AnchorStyles.Right;
                lab.AutoSize = true;
                lab.Text = driver;

                var host = new ComboBox();
                host.Dock = DockStyle.Fill;
                host.FormattingEnabled = true;
                host.TabIndex = row + 1;

                host.Font = m_Core.Config.PreferredFont;

                hosts.Clear();

                if (kv.Value.Length > 0)
                    hosts.Add(kv.Value);

                var plugins = renderdocplugin.PluginHelpers.GetPlugins();

                // search plugins for to find other targets for this driver
                foreach (var plugin in plugins)
                {
                    var replayman = renderdocplugin.PluginHelpers.GetPluginInterface<renderdocplugin.ReplayManagerPlugin>(plugin);

                    if (replayman != null && replayman.GetTargetType() == driver)
                    {
                        var targets = replayman.GetOnlineTargets();

                        foreach (var t in targets)
                        {
                            if (!hosts.Contains(t))
                                hosts.Add(t);
                        }
                    }
                }

                // fill the combo box with previously used hosts
                foreach (var prev in m_Core.Config.PreviouslyUsedHosts)
                {
                    if (prev.Key == driver && !hosts.Contains(prev.Value))
                        hosts.Add(prev.Value);
                }

                driversTable.Controls.Add(lab, 0, row);
                driversTable.Controls.Add(host, 1, row);
                driversTable.RowStyles.Add(new RowStyle());

                hosts.Remove("");

                host.Items.AddRange(hosts.ToArray());
                if (kv.Value.Length > 0)
                    host.SelectedIndex = 0;

                host.Tag = driver;
                host.SelectedValueChanged += new EventHandler(host_SelectedValueChanged);
                host.KeyUp += new KeyEventHandler(host_KeyUp);

                m_Hosts.Add(host);

                row++;
            }

            driversTable.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));

            driversTable.ResumeLayout(false);
            driversTable.PerformLayout();
        }

        void host_KeyUp(object sender, KeyEventArgs e)
        {
            host_SelectedValueChanged(sender, null);
        }

        void host_SelectedValueChanged(object sender, EventArgs e)
        {
            var host = sender as ComboBox;
            string driver = host.Tag as string;

            if (driver.Length > 0 && m_Core.Config.ReplayHosts.ContainsKey(driver))
                m_Core.Config.ReplayHosts[driver] = host.Text;
        }

        private void localProxy_SelectedIndexChanged(object sender, EventArgs e)
        {
            m_Core.Config.LocalProxy = localProxy.SelectedIndex;
        }

        private void ReplayHostManager_FormClosed(object sender, FormClosedEventArgs e)
        {
            foreach (var host in m_Hosts)
            {
                string driver = host.Tag as string;

                if (host.Text.Length == 0)
                    continue;

                bool found = false;
                foreach (var prev in m_Core.Config.PreviouslyUsedHosts)
                    if (prev.Key == driver && prev.Value == host.Text)
                        found = true;

                if (!found)
                    m_Core.Config.PreviouslyUsedHosts.Add(new SerializableKeyValuePair<string, string>(driver, host.Text));
            }
        }
    }
}
