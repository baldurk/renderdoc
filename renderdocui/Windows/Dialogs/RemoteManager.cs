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
using System.Threading;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    // this window lists the remote hosts the user has configured, and queries for any open connections
    // on any of them that indicate an application with renderdoc hooks that's running.
    public partial class RemoteManager : Form
    {
        MainWindow m_Main;
        Core m_Core;

        class RemoteConnect
        {
            public RemoteConnect(string h, UInt32 i) { host = h; ident = i; }
            public string host;
            public UInt32 ident;
        };

        private static string RemoteServerLiveText
        {
            get { return "Remote server running"; }
        }

        private static string RemoteServerDeadText
        {
            get { return "No remote server"; }
        }

        private static void SetRemoteServerLive(TreelistView.Node node, bool live, bool busy)
        {
            RemoteHost host = node.Tag as RemoteHost;

            host.ServerRunning = live;
            host.Busy = busy;

            if (host.Hostname == "localhost")
            {
                node.Image = null;
                node["running"] = "";
            }
            else
            {
                string text = live ? RemoteServerLiveText : RemoteServerDeadText;

                if (host.Connected)
                    text += " (Active Context)";
                else if (host.VersionMismatch)
                    text += " (Version Mismatch)";
                else if (host.Busy)
                    text += " (Busy)";

                node["running"] = text;

                node.Image = live
                    ? global::renderdocui.Properties.Resources.connect
                    : global::renderdocui.Properties.Resources.disconnect;
            }
        }

        private static bool IsRemoteServerLive(TreelistView.Node node)
        {
            return (node.Tag as RemoteHost).ServerRunning;
        }

        public RemoteManager(Core core, MainWindow main)
        {
            InitializeComponent();

            progressPicture.Image = global::renderdocui.Properties.Resources.hourglass;

            hosts_AfterSelect(hosts, new TreeViewEventArgs(null));

            Icon = global::renderdocui.Properties.Resources.icon;

            hostname.Font =
                hosts.Font =
                core.Config.PreferredFont;

            m_Core = core;
            m_Main = main;
        }

        private void RemoteManager_Load(object sender, EventArgs e)
        {
            hosts.BeginInit();

            m_Core.Config.AddAndroidHosts();
            foreach (var h in m_Core.Config.RemoteHosts)
                AddHost(h);

            hosts.EndInit();
        }

        // we kick off a thread per host to query for any open connections. The
        // interfaces on the C++ side should be thread-safe to cope with this
        private void AddHost(RemoteHost host)
        {
            TreelistView.Node node = new TreelistView.Node(new object[] { host.Hostname, "..." });

            node.Italic = true;
            node.Image = global::renderdocui.Properties.Resources.hourglass;
            node.Tag = host;

            hosts.Nodes.Add(node);

            refreshOne.Enabled = refreshAll.Enabled = false;

            {
                lookupMutex.WaitOne();
                lookupsInProgress++;
                lookupMutex.ReleaseMutex();
            }

            Thread th = Helpers.NewThread(new ParameterizedThreadStart(LookupHostConnections));
            th.Start(node);

            UpdateLookupsStatus();
        }

        private void UpdateLookupsStatus()
        {
            lookupsProgressFlow.Visible = !refreshAll.Enabled;
            remoteCountLabel.Text = String.Format("{0} lookups remaining", lookupsInProgress);
        }

        private static int lookupsInProgress = 0;
        private static Mutex lookupMutex = new Mutex();

        private static void RunRemoteServer(object o)
        {
            TreelistView.Node node = o as TreelistView.Node;
            RemoteHost host = node.Tag as RemoteHost;

            host.Launch();

            // now refresh this host
            Thread th = Helpers.NewThread(new ParameterizedThreadStart(LookupHostConnections));
            th.Start(node);
        }

        // this function looks up the remote connections and for each one open
        // queries it for the API, target (usually executable name) and if any user is already connected
        private static void LookupHostConnections(object o)
        {
            TreelistView.Node node = o as TreelistView.Node;

            RemoteHost host = node.Tag as RemoteHost;

            if(host == null)
                return;

            Control p = node.OwnerView;
            while (p.Parent != null)
                p = p.Parent;

            RemoteManager rhs = p as RemoteManager;

            string hostname = node["hostname"] as string;

            string username = System.Security.Principal.WindowsIdentity.GetCurrent().Name;

            host.CheckStatus();

            SetRemoteServerLive(node, host.ServerRunning, host.Busy);

            StaticExports.EnumerateRemoteTargets(hostname, (UInt32 i) => {
                try
                {
                    var conn = StaticExports.CreateTargetControl(hostname, i, username, false);

                    if (node.OwnerView.Visible)
                    {
                        string target = conn.Target;
                        string api = conn.API;
                        string busy = conn.BusyClient;

                        string running;

                        if (busy != "")
                            running = String.Format("Running {0}, {1} is connected", api, busy);
                        else
                            running = String.Format("Running {0}", api);

                        node.OwnerView.BeginInvoke((MethodInvoker)delegate
                        {
                            node.OwnerView.BeginUpdate();
                            node.Nodes.Add(new TreelistView.Node(new object[] { target, running })).Tag = new RemoteConnect(hostname, i);
                            node.OwnerView.EndUpdate();
                            node.Expand();
                        });
                    }

                    conn.Shutdown();
                }
                catch (ReplayCreateException)
                {
                }
            });

            if (node.OwnerView.Visible)
            {
                node.OwnerView.BeginInvoke((MethodInvoker)delegate
                {
                    node.OwnerView.BeginUpdate();
                    node.Italic = false;
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
                refreshOne.Enabled = refreshAll.Enabled = true;

            updateConnectButton();
            UpdateLookupsStatus();
        }

        private bool selectInProgress = false;

        private void hosts_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (selectInProgress)
                return;

            selectInProgress = true;

            runCommand.Text = "";
            hostname.Text = "";

            deleteHost.Enabled = refreshOne.Enabled = false;
            hostname.Enabled = addUpdateHost.Enabled = runCommand.Enabled = true;
            addUpdateHost.Text = "Add";

            if (hosts.SelectedNode != null &&
                hosts.SelectedNode.Tag != null)
            {
                RemoteHost host = hosts.SelectedNode.Tag as RemoteHost;

                if (host != null)
                {
                    if (refreshAll.Enabled)
                        refreshOne.Enabled = true;

                    if (host.Hostname == "localhost")
                    {
                        hostname.Text = "";
                        runCommand.Text = "";
                    }
                    else
                    {
                        deleteHost.Enabled = true;
                        runCommand.Text = host.RunCommand;
                        hostname.Text = host.Hostname;

                        addUpdateHost.Text = "Update";
                    }
                }
            }

            updateConnectButton();

            selectInProgress = false;
        }

        private void hostname_TextChanged(object sender, EventArgs e)
        {
            if(selectInProgress)
                return;

            selectInProgress = true;

            addUpdateHost.Text = "Add";
            addUpdateHost.Enabled = true;
            deleteHost.Enabled = refreshOne.Enabled = false;
            hostname.Enabled = addUpdateHost.Enabled = runCommand.Enabled = true;

            TreelistView.Node node = null;

            foreach(var n in hosts.Nodes)
            {
                if(n["hostname"].ToString() == hostname.Text)
                {
                    if (refreshAll.Enabled)
                        refreshOne.Enabled = true;

                    addUpdateHost.Text = "Update";

                    if (hostname.Text == "localhost")
                    {
                        hostname.Enabled = addUpdateHost.Enabled = runCommand.Enabled = false;
                    }
                    else
                    {
                        deleteHost.Enabled = true;
                        runCommand.Text = (n.Tag as RemoteHost).RunCommand;
                    }

                    node = n;
                    break;
                }
            }

            if (hosts.SelectedNode != node)
            {
                hosts.FocusedNode = node;
            }

            updateConnectButton();

            selectInProgress = false;
        }

        private void updateConnectButton()
        {
            if (hosts.SelectedNode != null &&
                hosts.SelectedNode.Tag != null)
            {
                connect.Enabled = true;
                connect.Text = "Connect to App";

                RemoteHost host = hosts.SelectedNode.Tag as RemoteHost;

                if (host != null)
                {
                    if (host.Hostname == "localhost")
                    {
                        connect.Enabled = false;
                    }
                    else if(host.ServerRunning)
                    {
                        connect.Text = "Shutdown";

                        if (host.Busy && !host.Connected)
                            connect.Enabled = false;
                    }
                    else
                    {
                        connect.Text = "Run Server";

                        if(host.RunCommand == "")
                            connect.Enabled = false;
                    }
                }
            }
            else
            {
                connect.Enabled = false;
            }
        }

        private void ConnectToApp(TreelistView.Node node)
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
            string host = hostname.Text.Trim();
            if (host.Length > 0)
            {
                bool found = false;

                StringComparer comp = StringComparer.Create(System.Globalization.CultureInfo.CurrentCulture, true);

                for (int i = 0; i < m_Core.Config.RemoteHosts.Count; i++)
                {
                    if (comp.Compare(m_Core.Config.RemoteHosts[i].Hostname, host.Trim()) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    RemoteHost h = new RemoteHost();
                    h.Hostname = host;
                    h.RunCommand = runCommand.Text;

                    m_Core.Config.RemoteHosts.Add(h);
                    m_Core.Config.Serialize(Core.ConfigFilename);

                    hosts.BeginUpdate();
                    AddHost(h);
                    hosts.EndUpdate();
                }
            }
            hostname.Text = "";
            hostname.Text = host;
        }

        private void SetRunCommand()
        {
            if (hosts.SelectedNode == null)
                return;

            RemoteHost h = hosts.SelectedNode.Tag as RemoteHost;

            if (h != null)
            {
                h.RunCommand = runCommand.Text.Trim();
                m_Core.Config.Serialize(Core.ConfigFilename);
            }
        }

        private void connect_Click(object sender, EventArgs e)
        {
            TreelistView.Node node = hosts.SelectedNode;
            if (node == null)
                return;

            if (node.Tag is RemoteConnect)
            {
                ConnectToApp(node);
            }
            else if (node.Tag is RemoteHost)
            {
                RemoteHost host = node.Tag as RemoteHost;

                if (host.ServerRunning)
                {
                    DialogResult res = MessageBox.Show(String.Format("Are you sure you wish to shut down running remote server on {0}?", host.Hostname),
                                                       "Remote server shutdown", MessageBoxButtons.YesNoCancel);

                    if (res == DialogResult.Cancel || res == DialogResult.No)
                        return;

                    // shut down
                    try
                    {
                        if (host.Connected)
                        {
                            m_Core.Renderer.ShutdownServer();
                            SetRemoteServerLive(node, false, false);
                        }
                        else
                        {
                            RemoteServer server = StaticExports.CreateRemoteServer(host.Hostname, 0);
                            server.ShutdownServerAndConnection();
                            hosts.BeginUpdate();
                            SetRemoteServerLive(node, false, false);
                            hosts.EndUpdate();
                        }
                    }
                    catch (Exception)
                    {
                        MessageBox.Show("Error shutting down remote server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }

                    updateConnectButton();
                }
                else
                {
                    // try to run
                    refreshOne.Enabled = refreshAll.Enabled = false;

                    {
                        lookupMutex.WaitOne();
                        lookupsInProgress++;
                        lookupMutex.ReleaseMutex();
                    }

                    Thread th = Helpers.NewThread(new ParameterizedThreadStart(RunRemoteServer));
                    th.Start(node);

                    UpdateLookupsStatus();
                }
            }
        }

        private void hosts_NodeDoubleClicked(TreelistView.Node node)
        {
            if(node.Tag is RemoteConnect)
                ConnectToApp(node);
        }

        private void refreshAll_Click(object sender, EventArgs e)
        {
            if (lookupsInProgress > 0)
                return;

            refreshOne.Enabled = refreshAll.Enabled = false;

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

            UpdateLookupsStatus();
        }

        private void refreshOne_Click(object sender, EventArgs e)
        {
            if (lookupsInProgress > 0 || hosts.SelectedNode == null)
                return;

            refreshOne.Enabled = refreshAll.Enabled = false;

            hosts.BeginUpdate();
            TreelistView.Node n = hosts.SelectedNode;
            {
                n.Nodes.Clear();
                n.Italic = true;
                n.Image = global::renderdocui.Properties.Resources.hourglass;
                n.Bold = false;

                Thread th = Helpers.NewThread(new ParameterizedThreadStart(LookupHostConnections));
                th.Start(n);
            }
            hosts.EndUpdate();

            UpdateLookupsStatus();
        }

        private void deleteHost_Click(object sender, EventArgs e)
        {
            if(hosts.SelectedNode == null || hosts.SelectedNode.Parent != null)
                return;

            string hostname = hosts.SelectedNode["hostname"] as string;

            if (hostname == "localhost")
                return;

            DialogResult res = MessageBox.Show(String.Format("Are you sure you wish to delete {0}?", hostname),
                                               "Deleting host", MessageBoxButtons.YesNoCancel);

            if (res == DialogResult.Cancel || res == DialogResult.No)
                return;

            if (res == DialogResult.Yes)
            {
                m_Core.Config.RemoteHosts.Remove(hosts.SelectedNode.Tag as RemoteHost);
                m_Core.Config.Serialize(Core.ConfigFilename);
                hosts.BeginUpdate();
                hosts.Nodes.Remove(hosts.SelectedNode);
                hosts.EndUpdate();

                hosts.FocusedNode = null;

                this.hostname.Text = "";
                this.hostname.Text = hostname;
            }
        }

        private void hosts_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyData == Keys.Return || e.KeyData == Keys.Enter)
            {
                if (connect.Enabled)
                    connect.PerformClick();
            }
            if (e.KeyData == Keys.Delete)
                deleteHost.PerformClick();
        }

        private void textbox_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                if(addUpdateHost.Enabled)
                    addUpdateHost.PerformClick();
            }
        }

        private void addUpdateHost_Click(object sender, EventArgs e)
        {
            if (hosts.SelectedNode != null && hosts.SelectedNode.Tag is RemoteHost)
                SetRunCommand();
            else
                AddNewHost();
        }
    }
}
