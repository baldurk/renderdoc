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
using System.Threading;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    // this window lists the remote hosts the user has configured, and queries for any open connections
    // on any of them that indicate an application with renderdoc hooks that's running.
    public partial class RemoteHostSelect : Form
    {
        MainWindow m_Main;
        Core m_Core;

        class RemoteConnect
        {
            public RemoteConnect(string h, UInt32 i) { host = h; ident = i; }
            public string host;
            public UInt32 ident;
        };

        public RemoteHostSelect(Core core, MainWindow main)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            hostname.Font =
                hosts.Font =
                core.Config.PreferredFont;

            m_Core = core;
            m_Main = main;

            hosts.BeginInit();

            // localhost should always be available
            if (!m_Core.Config.RecentHosts.Contains("localhost"))
                m_Core.Config.RecentHosts.Add("localhost");

            foreach (var h in m_Core.Config.RecentHosts)
            {
                AddHost(h);
            }

            hosts.EndInit();
        }

        // we kick off a thread per host to query for any open connections. The
        // interfaces on the C++ side should be thread-safe to cope with this
        private void AddHost(String host)
        {
            TreelistView.Node node = new TreelistView.Node(new object[] { host, "" });

            node.Italic = true;
            node.Image = global::renderdocui.Properties.Resources.hourglass;

            hosts.Nodes.Add(node);

            refresh.Enabled = false;

            Thread th = Helpers.NewThread(new ParameterizedThreadStart(LookupHostConnections));
            th.Start(node);
        }

        private class AvailableRemote
        {
            public AvailableRemote(string t, string a, string b)
            {
                Target = t;
                API = a;
                Busy = b;
            }

            public string Target, API, Busy;
        }

        private static int lookupsInProgress = 0;
        private static Mutex lookupMutex = new Mutex();

        // this function looks up the remote connections and for each one open
        // queries it for the API, target (usually executable name) and if any user is already connected
        private static void LookupHostConnections(object o)
        {
            {
                lookupMutex.WaitOne();
                lookupsInProgress++;
                lookupMutex.ReleaseMutex();
            }

            TreelistView.Node node = o as TreelistView.Node;

            Control p = node.OwnerView;
            while (p.Parent != null)
                p = p.Parent;

            RemoteHostSelect rhs = p as RemoteHostSelect;

            string hostname = node["Hostname"] as string;

            var idents = StaticExports.EnumerateRemoteConnections(hostname);

            var remotes = new Dictionary<UInt32, AvailableRemote>();

            string username = System.Security.Principal.WindowsIdentity.GetCurrent().Name;

            foreach (var i in idents)
            {
                if (i != 0)
                {
                    try
                    {
                        var conn = StaticExports.CreateRemoteAccessConnection(hostname, i, username, false);

                        var data = new AvailableRemote(conn.Target, conn.API, conn.BusyClient);

                        conn.Shutdown();

                        remotes.Add(i, data);
                    }
                    catch (ApplicationException)
                    {
                    }
                }
            }

            if (node.OwnerView.Visible)
            {
                node.OwnerView.BeginInvoke((MethodInvoker)delegate
                {
                    node.OwnerView.BeginUpdate();
                    node.Italic = false;
                    node.Image = null;
                    foreach (var kv in remotes)
                    {
                        node.Nodes.Add(new TreelistView.Node(new object[] { kv.Value.Target, kv.Value.API, kv.Value.Busy })).Tag = new RemoteConnect(hostname, kv.Key);
                        node.Bold = true;
                    }
                    node.OwnerView.EndUpdate();
                });
            }

            {
                lookupMutex.WaitOne();
                lookupsInProgress--;
                lookupMutex.ReleaseMutex();
            }

            if(!rhs.IsDisposed && rhs.Visible)
                rhs.BeginInvoke((MethodInvoker)delegate { rhs.LookupComplete(); });
        }

        // don't allow the user to refresh until all pending connections have been checked
        // (to stop flooding)
        private void LookupComplete()
        {
            if (lookupsInProgress == 0)
            {
                refresh.Enabled = true;
            }
        }

        private void hosts_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (hosts.SelectedNode != null &&
                hosts.SelectedNode.Tag != null)
            {
                connect.Enabled = true;
            }
            else
            {
                connect.Enabled = false;
            }
        }

        private void ConnectToHost(TreelistView.Node node)
        {
            if (node != null &&
                node.Tag != null)
            {
                var connect = node.Tag as RemoteConnect;
                var live = new LiveCapture(m_Core, connect.host, connect.ident, m_Main);
                m_Main.ShowLiveCapture(live);
                Close();
            }
        }

        private void AddNewHost()
        {
            if (hostname.Text.Trim().Length > 0 && !m_Core.Config.RecentHosts.Contains(hostname.Text, StringComparer.OrdinalIgnoreCase))
            {
                m_Core.Config.RecentHosts.Add(hostname.Text);
                m_Core.Config.Serialize(Core.ConfigFilename);

                hosts.BeginUpdate();
                AddHost(hostname.Text);
                hosts.EndUpdate();
            }
            hostname.Text = "";
        }

        private void connect_Click(object sender, EventArgs e)
        {
            ConnectToHost(hosts.SelectedNode);
        }

        private void hosts_NodeDoubleClicked(TreelistView.Node node)
        {
            ConnectToHost(node);
        }

        private void addhost_Click(object sender, EventArgs e)
        {
            AddNewHost();
        }

        private void hostname_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
                AddNewHost();
        }

        private void refresh_Click(object sender, EventArgs e)
        {
            if (lookupsInProgress > 0)
                return;

            refresh.Enabled = false;

            hosts.BeginUpdate();
            foreach (TreelistView.Node n in hosts.Nodes)
            {
                n.Nodes.Clear();
                n.Italic = true;
                n.Image = global::renderdocui.Properties.Resources.hourglass;
                n.Bold = false;

                Thread th = Helpers.NewThread(new ParameterizedThreadStart(LookupHostConnections));
                th.Start(n);
            }
            hosts.EndUpdate();
        }

        private void hosts_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyData == Keys.Return || e.KeyData == Keys.Enter)
            {
                if (connect.Enabled)
                    ConnectToHost(hosts.SelectedNode);
            }
            if (e.KeyData == Keys.Delete && hosts.SelectedNode != null && hosts.SelectedNode.Parent == null)
            {
                string hostname = hosts.SelectedNode["Hostname"] as string;

                if(hostname == "localhost")
                    return;

                DialogResult res = MessageBox.Show(String.Format("Are you sure you wish to delete {0}?", hostname),
                                                   "Deleting host", MessageBoxButtons.YesNoCancel);

                if (res == DialogResult.Cancel || res == DialogResult.No)
                    return;

                if (res == DialogResult.Yes)
                {
                    m_Core.Config.RecentHosts.Remove(hosts.SelectedNode["Hostname"] as String);
                    m_Core.Config.Serialize(Core.ConfigFilename);
                    hosts.BeginUpdate();
                    hosts.Nodes.Remove(hosts.SelectedNode);
                    hosts.EndUpdate();
                }
            }
        }
    }
}
