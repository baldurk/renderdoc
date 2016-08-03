namespace renderdocui.Windows.Dialogs
{
    partial class RemoteManager
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
            TreelistView.TreeListColumn treeListColumn3 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("hostname", "Hostname")));
            TreelistView.TreeListColumn treeListColumn4 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("running", "Running")));
            System.Windows.Forms.GroupBox basicConfigBox;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label4;
            this.hosts = new TreelistView.TreeListView();
            this.connect = new System.Windows.Forms.Button();
            this.addhost = new System.Windows.Forms.Button();
            this.hostname = new System.Windows.Forms.TextBox();
            this.refresh = new System.Windows.Forms.Button();
            this.configure = new System.Windows.Forms.Button();
            this.configHostname = new System.Windows.Forms.TextBox();
            this.configRunCommand = new System.Windows.Forms.TextBox();
            this.setHostname = new System.Windows.Forms.Button();
            this.setRunCommand = new System.Windows.Forms.Button();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            basicConfigBox = new System.Windows.Forms.GroupBox();
            tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label4 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).BeginInit();
            basicConfigBox.SuspendLayout();
            tableLayoutPanel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 6;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.Controls.Add(this.hosts, 0, 0);
            tableLayoutPanel1.Controls.Add(this.connect, 5, 1);
            tableLayoutPanel1.Controls.Add(this.addhost, 1, 1);
            tableLayoutPanel1.Controls.Add(this.hostname, 0, 1);
            tableLayoutPanel1.Controls.Add(this.refresh, 2, 1);
            tableLayoutPanel1.Controls.Add(this.configure, 4, 1);
            tableLayoutPanel1.Controls.Add(basicConfigBox, 0, 2);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 3;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel1.Size = new System.Drawing.Size(632, 438);
            tableLayoutPanel1.TabIndex = 2;
            // 
            // hosts
            // 
            this.hosts.AlwaysDisplayVScroll = true;
            treeListColumn3.AutoSize = true;
            treeListColumn3.AutoSizeMinSize = 15;
            treeListColumn3.Width = 48;
            treeListColumn4.AutoSizeMinSize = 0;
            treeListColumn4.Width = 250;
            this.hosts.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn3,
            treeListColumn4});
            tableLayoutPanel1.SetColumnSpan(this.hosts, 6);
            this.hosts.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.hosts.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hosts.Location = new System.Drawing.Point(3, 3);
            this.hosts.MultiSelect = false;
            this.hosts.Name = "hosts";
            this.hosts.RowOptions.ShowHeader = false;
            this.hosts.Size = new System.Drawing.Size(626, 259);
            this.hosts.TabIndex = 1;
            this.hosts.ViewOptions.HoverHandTreeColumn = false;
            this.hosts.ViewOptions.Indent = 12;
            this.hosts.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.hosts_AfterSelect);
            this.hosts.NodeDoubleClicked += new TreelistView.TreeListView.NodeDoubleClickedHandler(this.hosts_NodeDoubleClicked);
            this.hosts.KeyDown += new System.Windows.Forms.KeyEventHandler(this.hosts_KeyDown);
            // 
            // connect
            // 
            this.connect.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.connect.Enabled = false;
            this.connect.Location = new System.Drawing.Point(554, 268);
            this.connect.Name = "connect";
            this.connect.Size = new System.Drawing.Size(75, 23);
            this.connect.TabIndex = 5;
            this.connect.Text = "Connect";
            this.connect.UseVisualStyleBackColor = true;
            this.connect.Click += new System.EventHandler(this.connect_Click);
            // 
            // addhost
            // 
            this.addhost.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.addhost.Location = new System.Drawing.Point(291, 268);
            this.addhost.Name = "addhost";
            this.addhost.Size = new System.Drawing.Size(75, 23);
            this.addhost.TabIndex = 3;
            this.addhost.Text = "Add Host";
            this.addhost.UseVisualStyleBackColor = true;
            this.addhost.Click += new System.EventHandler(this.addhost_Click);
            // 
            // hostname
            // 
            this.hostname.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.hostname.Location = new System.Drawing.Point(3, 268);
            this.hostname.Name = "hostname";
            this.hostname.Size = new System.Drawing.Size(282, 20);
            this.hostname.TabIndex = 2;
            this.hostname.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textbox_KeyPress);
            // 
            // refresh
            // 
            this.refresh.Location = new System.Drawing.Point(372, 268);
            this.refresh.Name = "refresh";
            this.refresh.Size = new System.Drawing.Size(75, 23);
            this.refresh.TabIndex = 4;
            this.refresh.Text = "Refresh";
            this.refresh.UseVisualStyleBackColor = true;
            this.refresh.Click += new System.EventHandler(this.refresh_Click);
            // 
            // configure
            // 
            this.configure.Location = new System.Drawing.Point(473, 268);
            this.configure.Name = "configure";
            this.configure.Size = new System.Drawing.Size(75, 23);
            this.configure.TabIndex = 6;
            this.configure.Text = "Configure";
            this.configure.UseVisualStyleBackColor = true;
            // 
            // basicConfigBox
            // 
            basicConfigBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            tableLayoutPanel1.SetColumnSpan(basicConfigBox, 6);
            basicConfigBox.Controls.Add(tableLayoutPanel2);
            basicConfigBox.Location = new System.Drawing.Point(3, 297);
            basicConfigBox.Name = "basicConfigBox";
            basicConfigBox.Size = new System.Drawing.Size(626, 138);
            basicConfigBox.TabIndex = 7;
            basicConfigBox.TabStop = false;
            basicConfigBox.Text = "Basic configuration";
            // 
            // tableLayoutPanel2
            // 
            tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            tableLayoutPanel2.AutoSize = true;
            tableLayoutPanel2.ColumnCount = 3;
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel2.Controls.Add(label1, 0, 1);
            tableLayoutPanel2.Controls.Add(this.configHostname, 1, 1);
            tableLayoutPanel2.Controls.Add(this.configRunCommand, 1, 3);
            tableLayoutPanel2.Controls.Add(label2, 0, 3);
            tableLayoutPanel2.Controls.Add(label3, 1, 2);
            tableLayoutPanel2.Controls.Add(label4, 1, 0);
            tableLayoutPanel2.Controls.Add(this.setHostname, 2, 1);
            tableLayoutPanel2.Controls.Add(this.setRunCommand, 2, 3);
            tableLayoutPanel2.Location = new System.Drawing.Point(6, 19);
            tableLayoutPanel2.Name = "tableLayoutPanel2";
            tableLayoutPanel2.RowCount = 4;
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            tableLayoutPanel2.Size = new System.Drawing.Size(614, 112);
            tableLayoutPanel2.TabIndex = 0;
            // 
            // label1
            // 
            label1.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(53, 34);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(58, 13);
            label1.TabIndex = 0;
            label1.Text = "Hostname:";
            // 
            // configHostname
            // 
            this.configHostname.Dock = System.Windows.Forms.DockStyle.Fill;
            this.configHostname.Location = new System.Drawing.Point(117, 29);
            this.configHostname.Name = "configHostname";
            this.configHostname.Size = new System.Drawing.Size(453, 20);
            this.configHostname.TabIndex = 1;
            this.configHostname.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textbox_KeyPress);
            // 
            // configRunCommand
            // 
            this.configRunCommand.Dock = System.Windows.Forms.DockStyle.Fill;
            this.configRunCommand.Location = new System.Drawing.Point(117, 85);
            this.configRunCommand.Name = "configRunCommand";
            this.configRunCommand.Size = new System.Drawing.Size(453, 20);
            this.configRunCommand.TabIndex = 4;
            this.configRunCommand.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textbox_KeyPress);
            // 
            // label2
            // 
            label2.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(3, 90);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(108, 13);
            label2.TabIndex = 2;
            label2.Text = "Server run command:";
            // 
            // label3
            // 
            label3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(117, 66);
            label3.Margin = new System.Windows.Forms.Padding(3, 10, 3, 3);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(413, 13);
            label3.TabIndex = 3;
            label3.Text = "This lets you configure a command to run that launches the remote server on this " +
    "host.";
            // 
            // label4
            // 
            label4.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            label4.AutoSize = true;
            label4.Location = new System.Drawing.Point(117, 10);
            label4.Margin = new System.Windows.Forms.Padding(3, 10, 3, 3);
            label4.Name = "label4";
            label4.Size = new System.Drawing.Size(261, 13);
            label4.TabIndex = 5;
            label4.Text = "Configure the network hostname for this remote server";
            // 
            // setHostname
            // 
            this.setHostname.Location = new System.Drawing.Point(576, 29);
            this.setHostname.Name = "setHostname";
            this.setHostname.Size = new System.Drawing.Size(34, 23);
            this.setHostname.TabIndex = 6;
            this.setHostname.Text = "Set";
            this.setHostname.UseVisualStyleBackColor = true;
            this.setHostname.Click += new System.EventHandler(this.setConfig_Click);
            // 
            // setRunCommand
            // 
            this.setRunCommand.Location = new System.Drawing.Point(576, 85);
            this.setRunCommand.Name = "setRunCommand";
            this.setRunCommand.Size = new System.Drawing.Size(34, 23);
            this.setRunCommand.TabIndex = 7;
            this.setRunCommand.Text = "Set";
            this.setRunCommand.UseVisualStyleBackColor = true;
            this.setRunCommand.Click += new System.EventHandler(this.setConfig_Click);
            // 
            // RemoteManager
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(632, 438);
            this.Controls.Add(tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "RemoteManager";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Remote manager";
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).EndInit();
            basicConfigBox.ResumeLayout(false);
            basicConfigBox.PerformLayout();
            tableLayoutPanel2.ResumeLayout(false);
            tableLayoutPanel2.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView hosts;
        private System.Windows.Forms.Button connect;
        private System.Windows.Forms.Button addhost;
        private System.Windows.Forms.TextBox hostname;
        private System.Windows.Forms.Button refresh;
        private System.Windows.Forms.Button configure;
        private System.Windows.Forms.TextBox configHostname;
        private System.Windows.Forms.TextBox configRunCommand;
        private System.Windows.Forms.Button setHostname;
        private System.Windows.Forms.Button setRunCommand;
    }
}