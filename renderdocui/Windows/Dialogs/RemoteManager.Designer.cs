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
            System.Windows.Forms.GroupBox basicConfigBox;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.GroupBox opsBox;
            System.Windows.Forms.Label label2;
            TreelistView.TreeListColumn treeListColumn1 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("hostname", "Hostname")));
            TreelistView.TreeListColumn treeListColumn2 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("running", "Running")));
            this.hostname = new System.Windows.Forms.TextBox();
            this.runCommand = new System.Windows.Forms.TextBox();
            this.addUpdateHost = new System.Windows.Forms.Button();
            this.deleteHost = new System.Windows.Forms.Button();
            this.refreshOne = new System.Windows.Forms.Button();
            this.connect = new System.Windows.Forms.Button();
            this.refreshAll = new System.Windows.Forms.Button();
            this.lookupsProgressFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.progressPicture = new System.Windows.Forms.PictureBox();
            this.hosts = new TreelistView.TreeListView();
            this.remoteCountLabel = new System.Windows.Forms.Label();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            basicConfigBox = new System.Windows.Forms.GroupBox();
            tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            label3 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            opsBox = new System.Windows.Forms.GroupBox();
            label2 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            basicConfigBox.SuspendLayout();
            tableLayoutPanel2.SuspendLayout();
            opsBox.SuspendLayout();
            this.lookupsProgressFlow.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.progressPicture)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).BeginInit();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 2;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.Controls.Add(this.hosts, 0, 0);
            tableLayoutPanel1.Controls.Add(basicConfigBox, 0, 2);
            tableLayoutPanel1.Controls.Add(opsBox, 1, 2);
            tableLayoutPanel1.Controls.Add(this.lookupsProgressFlow, 0, 1);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 3;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(602, 460);
            tableLayoutPanel1.TabIndex = 2;
            // 
            // basicConfigBox
            // 
            basicConfigBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            basicConfigBox.Controls.Add(tableLayoutPanel2);
            basicConfigBox.Location = new System.Drawing.Point(3, 319);
            basicConfigBox.Name = "basicConfigBox";
            basicConfigBox.Size = new System.Drawing.Size(476, 138);
            basicConfigBox.TabIndex = 7;
            basicConfigBox.TabStop = false;
            basicConfigBox.Text = "Host configuration";
            // 
            // tableLayoutPanel2
            // 
            tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            tableLayoutPanel2.AutoSize = true;
            tableLayoutPanel2.ColumnCount = 2;
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel2.Controls.Add(this.hostname, 0, 1);
            tableLayoutPanel2.Controls.Add(this.runCommand, 0, 3);
            tableLayoutPanel2.Controls.Add(label3, 0, 2);
            tableLayoutPanel2.Controls.Add(this.addUpdateHost, 1, 1);
            tableLayoutPanel2.Controls.Add(label1, 0, 0);
            tableLayoutPanel2.Location = new System.Drawing.Point(6, 19);
            tableLayoutPanel2.Name = "tableLayoutPanel2";
            tableLayoutPanel2.RowCount = 4;
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 51.51515F));
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 48.48485F));
            tableLayoutPanel2.Size = new System.Drawing.Size(464, 112);
            tableLayoutPanel2.TabIndex = 0;
            // 
            // hostname
            // 
            this.hostname.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hostname.Location = new System.Drawing.Point(3, 23);
            this.hostname.Name = "hostname";
            this.hostname.Size = new System.Drawing.Size(395, 20);
            this.hostname.TabIndex = 1;
            this.hostname.TextChanged += new System.EventHandler(this.hostname_TextChanged);
            this.hostname.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textbox_KeyPress);
            // 
            // runCommand
            // 
            this.runCommand.Dock = System.Windows.Forms.DockStyle.Fill;
            this.runCommand.Location = new System.Drawing.Point(3, 89);
            this.runCommand.Name = "runCommand";
            this.runCommand.Size = new System.Drawing.Size(395, 20);
            this.runCommand.TabIndex = 4;
            this.runCommand.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textbox_KeyPress);
            // 
            // label3
            // 
            label3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(3, 57);
            label3.Margin = new System.Windows.Forms.Padding(3, 10, 3, 3);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(386, 26);
            label3.TabIndex = 3;
            label3.Text = "Run Command: Configure a command to run that launches the remote server on this h" +
    "ost.";
            // 
            // addUpdateHost
            // 
            this.addUpdateHost.Location = new System.Drawing.Point(404, 23);
            this.addUpdateHost.Name = "addUpdateHost";
            this.addUpdateHost.Size = new System.Drawing.Size(57, 21);
            this.addUpdateHost.TabIndex = 6;
            this.addUpdateHost.Text = "Add";
            this.addUpdateHost.UseVisualStyleBackColor = true;
            this.addUpdateHost.Click += new System.EventHandler(this.addUpdateHost_Click);
            // 
            // label1
            // 
            label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(3, 0);
            label1.MinimumSize = new System.Drawing.Size(0, 20);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(58, 20);
            label1.TabIndex = 0;
            label1.Text = "Hostname:";
            label1.TextAlign = System.Drawing.ContentAlignment.BottomCenter;
            // 
            // opsBox
            // 
            opsBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            opsBox.Controls.Add(this.deleteHost);
            opsBox.Controls.Add(this.refreshOne);
            opsBox.Controls.Add(this.connect);
            opsBox.Controls.Add(this.refreshAll);
            opsBox.Location = new System.Drawing.Point(485, 319);
            opsBox.Name = "opsBox";
            opsBox.Size = new System.Drawing.Size(114, 138);
            opsBox.TabIndex = 8;
            opsBox.TabStop = false;
            opsBox.Text = "Operations";
            // 
            // deleteHost
            // 
            this.deleteHost.Enabled = false;
            this.deleteHost.Location = new System.Drawing.Point(6, 106);
            this.deleteHost.Name = "deleteHost";
            this.deleteHost.Size = new System.Drawing.Size(101, 23);
            this.deleteHost.TabIndex = 6;
            this.deleteHost.Text = "Delete";
            this.deleteHost.UseVisualStyleBackColor = true;
            this.deleteHost.Click += new System.EventHandler(this.deleteHost_Click);
            // 
            // refreshOne
            // 
            this.refreshOne.Location = new System.Drawing.Point(6, 19);
            this.refreshOne.Name = "refreshOne";
            this.refreshOne.Size = new System.Drawing.Size(101, 23);
            this.refreshOne.TabIndex = 5;
            this.refreshOne.Text = "Refresh Selected";
            this.refreshOne.UseVisualStyleBackColor = true;
            this.refreshOne.Click += new System.EventHandler(this.refreshOne_Click);
            // 
            // connect
            // 
            this.connect.Enabled = false;
            this.connect.Location = new System.Drawing.Point(6, 77);
            this.connect.Name = "connect";
            this.connect.Size = new System.Drawing.Size(101, 23);
            this.connect.TabIndex = 5;
            this.connect.Text = "Connect to App";
            this.connect.UseVisualStyleBackColor = true;
            this.connect.Click += new System.EventHandler(this.connect_Click);
            // 
            // refreshAll
            // 
            this.refreshAll.Location = new System.Drawing.Point(6, 48);
            this.refreshAll.Name = "refreshAll";
            this.refreshAll.Size = new System.Drawing.Size(101, 23);
            this.refreshAll.TabIndex = 4;
            this.refreshAll.Text = "Refresh All";
            this.refreshAll.UseVisualStyleBackColor = true;
            this.refreshAll.Click += new System.EventHandler(this.refreshAll_Click);
            // 
            // lookupsProgressFlow
            // 
            this.lookupsProgressFlow.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            tableLayoutPanel1.SetColumnSpan(this.lookupsProgressFlow, 2);
            this.lookupsProgressFlow.Controls.Add(this.progressPicture);
            this.lookupsProgressFlow.Controls.Add(label2);
            this.lookupsProgressFlow.Controls.Add(this.remoteCountLabel);
            this.lookupsProgressFlow.Location = new System.Drawing.Point(3, 289);
            this.lookupsProgressFlow.MinimumSize = new System.Drawing.Size(0, 24);
            this.lookupsProgressFlow.Name = "lookupsProgressFlow";
            this.lookupsProgressFlow.Size = new System.Drawing.Size(596, 24);
            this.lookupsProgressFlow.TabIndex = 9;
            this.lookupsProgressFlow.Visible = false;
            // 
            // progressPicture
            // 
            this.progressPicture.Location = new System.Drawing.Point(3, 3);
            this.progressPicture.Name = "progressPicture";
            this.progressPicture.Size = new System.Drawing.Size(100, 16);
            this.progressPicture.SizeMode = System.Windows.Forms.PictureBoxSizeMode.AutoSize;
            this.progressPicture.TabIndex = 0;
            this.progressPicture.TabStop = false;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Dock = System.Windows.Forms.DockStyle.Left;
            label2.Location = new System.Drawing.Point(109, 0);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(228, 22);
            label2.TabIndex = 1;
            label2.Text = "Remote connections in progress. Please wait...";
            label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // hosts
            // 
            this.hosts.AlwaysDisplayVScroll = true;
            treeListColumn1.AutoSize = true;
            treeListColumn1.AutoSizeMinSize = 15;
            treeListColumn1.Width = 48;
            treeListColumn2.AutoSizeMinSize = 0;
            treeListColumn2.Width = 250;
            this.hosts.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2});
            tableLayoutPanel1.SetColumnSpan(this.hosts, 2);
            this.hosts.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.hosts.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hosts.Location = new System.Drawing.Point(3, 3);
            this.hosts.MultiSelect = false;
            this.hosts.Name = "hosts";
            this.hosts.RowOptions.ShowHeader = false;
            this.hosts.Size = new System.Drawing.Size(596, 280);
            this.hosts.TabIndex = 1;
            this.hosts.ViewOptions.HoverHandTreeColumn = false;
            this.hosts.ViewOptions.Indent = 12;
            this.hosts.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.hosts_AfterSelect);
            this.hosts.NodeDoubleClicked += new TreelistView.TreeListView.NodeDoubleClickedHandler(this.hosts_NodeDoubleClicked);
            this.hosts.KeyDown += new System.Windows.Forms.KeyEventHandler(this.hosts_KeyDown);
            // 
            // remoteCountLabel
            // 
            this.remoteCountLabel.AutoSize = true;
            this.remoteCountLabel.Dock = System.Windows.Forms.DockStyle.Left;
            this.remoteCountLabel.Location = new System.Drawing.Point(343, 0);
            this.remoteCountLabel.Name = "remoteCountLabel";
            this.remoteCountLabel.Size = new System.Drawing.Size(118, 22);
            this.remoteCountLabel.TabIndex = 2;
            this.remoteCountLabel.Text = "999 connections active";
            this.remoteCountLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // RemoteManager
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(602, 460);
            this.Controls.Add(tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Name = "RemoteManager";
            this.MinimizeBox = false;
            this.MaximizeBox = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Remote Host Manager";
            this.Load += new System.EventHandler(this.RemoteManager_Load);
            tableLayoutPanel1.ResumeLayout(false);
            basicConfigBox.ResumeLayout(false);
            basicConfigBox.PerformLayout();
            tableLayoutPanel2.ResumeLayout(false);
            tableLayoutPanel2.PerformLayout();
            opsBox.ResumeLayout(false);
            this.lookupsProgressFlow.ResumeLayout(false);
            this.lookupsProgressFlow.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.progressPicture)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView hosts;
        private System.Windows.Forms.Button connect;
        private System.Windows.Forms.Button refreshAll;
        private System.Windows.Forms.TextBox hostname;
        private System.Windows.Forms.TextBox runCommand;
        private System.Windows.Forms.Button addUpdateHost;
        private System.Windows.Forms.Button refreshOne;
        private System.Windows.Forms.Button deleteHost;
        private System.Windows.Forms.FlowLayoutPanel lookupsProgressFlow;
        private System.Windows.Forms.PictureBox progressPicture;
        private System.Windows.Forms.Label remoteCountLabel;
    }
}