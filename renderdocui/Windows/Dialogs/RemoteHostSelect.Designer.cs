namespace renderdocui.Windows.Dialogs
{
    partial class RemoteHostSelect
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
            TreelistView.TreeListColumn treeListColumn1 = new TreelistView.TreeListColumn("Hostname", "Hostname");
            TreelistView.TreeListColumn treeListColumn2 = new TreelistView.TreeListColumn("API", "API");
            TreelistView.TreeListColumn treeListColumn3 = new TreelistView.TreeListColumn("User", "Connected User");
            this.hosts = new TreelistView.TreeListView();
            this.connect = new System.Windows.Forms.Button();
            this.addhost = new System.Windows.Forms.Button();
            this.hostname = new System.Windows.Forms.TextBox();
            this.refresh = new System.Windows.Forms.Button();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).BeginInit();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 4;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.Controls.Add(this.hosts, 0, 0);
            tableLayoutPanel1.Controls.Add(this.connect, 3, 1);
            tableLayoutPanel1.Controls.Add(this.addhost, 1, 1);
            tableLayoutPanel1.Controls.Add(this.hostname, 0, 1);
            tableLayoutPanel1.Controls.Add(this.refresh, 2, 1);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(488, 297);
            tableLayoutPanel1.TabIndex = 2;
            // 
            // hosts
            // 
            this.hosts.AlwaysDisplayVScroll = true;
            treeListColumn1.AutoSize = true;
            treeListColumn1.AutoSizeMinSize = 15;
            treeListColumn1.Width = 48;
            treeListColumn2.AutoSizeMinSize = 15;
            treeListColumn2.Width = 80;
            treeListColumn3.AutoSizeMinSize = 15;
            treeListColumn3.Width = 125;
            this.hosts.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3});
            tableLayoutPanel1.SetColumnSpan(this.hosts, 4);
            this.hosts.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.hosts.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hosts.Location = new System.Drawing.Point(3, 3);
            this.hosts.MultiSelect = false;
            this.hosts.Name = "hosts";
            this.hosts.RowOptions.ShowHeader = false;
            this.hosts.Size = new System.Drawing.Size(482, 262);
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
            this.connect.Location = new System.Drawing.Point(410, 271);
            this.connect.Margin = new System.Windows.Forms.Padding(32, 3, 3, 3);
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
            this.addhost.Location = new System.Drawing.Point(219, 271);
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
            this.hostname.Location = new System.Drawing.Point(3, 271);
            this.hostname.Name = "hostname";
            this.hostname.Size = new System.Drawing.Size(210, 20);
            this.hostname.TabIndex = 2;
            this.hostname.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.hostname_KeyPress);
            // 
            // refresh
            // 
            this.refresh.Location = new System.Drawing.Point(300, 271);
            this.refresh.Name = "refresh";
            this.refresh.Size = new System.Drawing.Size(75, 23);
            this.refresh.TabIndex = 4;
            this.refresh.Text = "Refresh";
            this.refresh.UseVisualStyleBackColor = true;
            this.refresh.Click += new System.EventHandler(this.refresh_Click);
            // 
            // RemoteHostSelect
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(488, 297);
            this.Controls.Add(tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "RemoteHostSelect";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Attach to running instance";
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.hosts)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView hosts;
        private System.Windows.Forms.Button connect;
        private System.Windows.Forms.Button addhost;
        private System.Windows.Forms.TextBox hostname;
        private System.Windows.Forms.Button refresh;
    }
}