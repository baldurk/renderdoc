namespace renderdocui.Windows.Dialogs
{
    partial class VirtualOpenFileDialog
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
            this.components = new System.ComponentModel.Container();
            System.Windows.Forms.TableLayoutPanel mainTable;
            System.Windows.Forms.ToolStrip toolStrip;
            System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
            System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
            System.Windows.Forms.ToolStripLabel locLabel;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(VirtualOpenFileDialog));
            System.Windows.Forms.Label fnLabel;
            System.Windows.Forms.Button cancel;
            this.back = new System.Windows.Forms.ToolStripButton();
            this.forward = new System.Windows.Forms.ToolStripButton();
            this.up = new System.Windows.Forms.ToolStripButton();
            this.location = new renderdocui.Controls.ToolStripSpringTextBox();
            this.directoryTree = new System.Windows.Forms.TreeView();
            this.iconList = new System.Windows.Forms.ImageList(this.components);
            this.fileList = new System.Windows.Forms.ListView();
            this.filename = new System.Windows.Forms.TextBox();
            this.fileType = new System.Windows.Forms.ComboBox();
            this.open = new System.Windows.Forms.Button();
            this.showHidden = new System.Windows.Forms.CheckBox();
            mainTable = new System.Windows.Forms.TableLayoutPanel();
            toolStrip = new System.Windows.Forms.ToolStrip();
            toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            locLabel = new System.Windows.Forms.ToolStripLabel();
            fnLabel = new System.Windows.Forms.Label();
            cancel = new System.Windows.Forms.Button();
            mainTable.SuspendLayout();
            toolStrip.SuspendLayout();
            this.SuspendLayout();
            // 
            // mainTable
            // 
            mainTable.ColumnCount = 4;
            mainTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            mainTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            mainTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            mainTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            mainTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            mainTable.Controls.Add(toolStrip, 0, 0);
            mainTable.Controls.Add(this.directoryTree, 0, 1);
            mainTable.Controls.Add(this.fileList, 1, 1);
            mainTable.Controls.Add(fnLabel, 0, 3);
            mainTable.Controls.Add(this.filename, 1, 3);
            mainTable.Controls.Add(this.fileType, 2, 3);
            mainTable.Controls.Add(this.open, 2, 4);
            mainTable.Controls.Add(cancel, 3, 4);
            mainTable.Controls.Add(this.showHidden, 1, 4);
            mainTable.Dock = System.Windows.Forms.DockStyle.Fill;
            mainTable.Location = new System.Drawing.Point(0, 0);
            mainTable.Margin = new System.Windows.Forms.Padding(3, 3, 3, 0);
            mainTable.Name = "mainTable";
            mainTable.RowCount = 5;
            mainTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            mainTable.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            mainTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            mainTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            mainTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            mainTable.Size = new System.Drawing.Size(764, 486);
            mainTable.TabIndex = 0;
            // 
            // toolStrip
            // 
            toolStrip.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            mainTable.SetColumnSpan(toolStrip, 4);
            toolStrip.Dock = System.Windows.Forms.DockStyle.None;
            toolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            toolStrip.ImageScalingSize = new System.Drawing.Size(24, 24);
            toolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.back,
            this.forward,
            toolStripSeparator1,
            this.up,
            toolStripSeparator2,
            locLabel,
            this.location});
            toolStrip.Location = new System.Drawing.Point(0, 0);
            toolStrip.Name = "toolStrip";
            toolStrip.Size = new System.Drawing.Size(764, 31);
            toolStrip.TabIndex = 3;
            toolStrip.Text = "toolStrip1";
            // 
            // back
            // 
            this.back.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.back.Image = global::renderdocui.Properties.Resources.back;
            this.back.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.back.Margin = new System.Windows.Forms.Padding(0, 1, 5, 2);
            this.back.Name = "back";
            this.back.Size = new System.Drawing.Size(28, 28);
            this.back.Click += new System.EventHandler(this.back_Click);
            // 
            // forward
            // 
            this.forward.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.forward.Image = global::renderdocui.Properties.Resources.forward;
            this.forward.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.forward.Name = "forward";
            this.forward.Size = new System.Drawing.Size(28, 28);
            this.forward.Click += new System.EventHandler(this.forward_Click);
            // 
            // toolStripSeparator1
            // 
            toolStripSeparator1.Name = "toolStripSeparator1";
            toolStripSeparator1.Size = new System.Drawing.Size(6, 31);
            // 
            // up
            // 
            this.up.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.up.Image = global::renderdocui.Properties.Resources.upfolder;
            this.up.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.up.Name = "up";
            this.up.Size = new System.Drawing.Size(28, 28);
            this.up.Click += new System.EventHandler(this.up_Click);
            // 
            // toolStripSeparator2
            // 
            toolStripSeparator2.Name = "toolStripSeparator2";
            toolStripSeparator2.Size = new System.Drawing.Size(6, 31);
            // 
            // locLabel
            // 
            locLabel.Name = "locLabel";
            locLabel.Size = new System.Drawing.Size(51, 28);
            locLabel.Text = "Location:";
            // 
            // location
            // 
            this.location.Name = "location";
            this.location.Size = new System.Drawing.Size(578, 31);
            this.location.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.location_KeyPress);
            // 
            // directoryTree
            // 
            this.directoryTree.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.directoryTree.FullRowSelect = true;
            this.directoryTree.HideSelection = false;
            this.directoryTree.HotTracking = true;
            this.directoryTree.ImageIndex = 2;
            this.directoryTree.ImageList = this.iconList;
            this.directoryTree.Location = new System.Drawing.Point(3, 34);
            this.directoryTree.Name = "directoryTree";
            this.directoryTree.PathSeparator = "/";
            this.directoryTree.SelectedImageIndex = 0;
            this.directoryTree.Size = new System.Drawing.Size(251, 381);
            this.directoryTree.TabIndex = 5;
            this.directoryTree.BeforeExpand += new System.Windows.Forms.TreeViewCancelEventHandler(this.directoryTree_BeforeExpand);
            this.directoryTree.BeforeSelect += new System.Windows.Forms.TreeViewCancelEventHandler(this.directoryTree_BeforeSelect);
            // 
            // iconList
            // 
            this.iconList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("iconList.ImageStream")));
            this.iconList.TransparentColor = System.Drawing.Color.Transparent;
            this.iconList.Images.SetKeyName(0, "drive");
            this.iconList.Images.SetKeyName(1, "folder");
            this.iconList.Images.SetKeyName(2, "folder_faded");
            this.iconList.Images.SetKeyName(3, "file");
            this.iconList.Images.SetKeyName(4, "file_faded");
            this.iconList.Images.SetKeyName(5, "executable");
            this.iconList.Images.SetKeyName(6, "executable_faded");
            // 
            // fileList
            // 
            mainTable.SetColumnSpan(this.fileList, 3);
            this.fileList.Dock = System.Windows.Forms.DockStyle.Fill;
            this.fileList.Location = new System.Drawing.Point(260, 34);
            this.fileList.Name = "fileList";
            this.fileList.Size = new System.Drawing.Size(501, 381);
            this.fileList.SmallImageList = this.iconList;
            this.fileList.TabIndex = 6;
            this.fileList.UseCompatibleStateImageBehavior = false;
            this.fileList.View = System.Windows.Forms.View.List;
            this.fileList.SelectedIndexChanged += new System.EventHandler(this.fileList_SelectedIndexChanged);
            this.fileList.DoubleClick += new System.EventHandler(this.fileList_DoubleClick);
            // 
            // fnLabel
            // 
            fnLabel.Anchor = System.Windows.Forms.AnchorStyles.Right;
            fnLabel.AutoSize = true;
            fnLabel.Location = new System.Drawing.Point(205, 425);
            fnLabel.Margin = new System.Windows.Forms.Padding(3, 0, 0, 0);
            fnLabel.Name = "fnLabel";
            fnLabel.Size = new System.Drawing.Size(52, 13);
            fnLabel.TabIndex = 7;
            fnLabel.Text = "Filename:";
            fnLabel.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // filename
            // 
            this.filename.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.filename.Location = new System.Drawing.Point(260, 421);
            this.filename.Margin = new System.Windows.Forms.Padding(3, 3, 15, 3);
            this.filename.Name = "filename";
            this.filename.Size = new System.Drawing.Size(315, 20);
            this.filename.TabIndex = 8;
            this.filename.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.filename_KeyPress);
            // 
            // fileType
            // 
            this.fileType.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            mainTable.SetColumnSpan(this.fileType, 2);
            this.fileType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.fileType.FormattingEnabled = true;
            this.fileType.Items.AddRange(new object[] {
            "Executables",
            "All Files"});
            this.fileType.Location = new System.Drawing.Point(593, 421);
            this.fileType.Margin = new System.Windows.Forms.Padding(3, 3, 15, 3);
            this.fileType.Name = "fileType";
            this.fileType.Size = new System.Drawing.Size(156, 21);
            this.fileType.TabIndex = 9;
            this.fileType.SelectedIndexChanged += new System.EventHandler(this.fileType_SelectedIndexChanged);
            // 
            // open
            // 
            this.open.Location = new System.Drawing.Point(593, 448);
            this.open.Name = "open";
            this.open.Size = new System.Drawing.Size(75, 23);
            this.open.TabIndex = 10;
            this.open.Text = "Open";
            this.open.UseVisualStyleBackColor = true;
            this.open.Click += new System.EventHandler(this.open_Click);
            // 
            // cancel
            // 
            cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            cancel.Location = new System.Drawing.Point(674, 448);
            cancel.Margin = new System.Windows.Forms.Padding(3, 3, 15, 15);
            cancel.Name = "cancel";
            cancel.Size = new System.Drawing.Size(75, 23);
            cancel.TabIndex = 11;
            cancel.Text = "Cancel";
            cancel.UseVisualStyleBackColor = true;
            cancel.Click += new System.EventHandler(this.cancel_Click);
            // 
            // showHidden
            // 
            this.showHidden.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.showHidden.AutoSize = true;
            this.showHidden.Location = new System.Drawing.Point(466, 451);
            this.showHidden.Margin = new System.Windows.Forms.Padding(3, 6, 15, 3);
            this.showHidden.Name = "showHidden";
            this.showHidden.Size = new System.Drawing.Size(109, 17);
            this.showHidden.TabIndex = 12;
            this.showHidden.Text = "Show hidden files";
            this.showHidden.UseVisualStyleBackColor = true;
            this.showHidden.CheckedChanged += new System.EventHandler(this.showHidden_CheckedChanged);
            // 
            // VirtualOpenFileDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = cancel;
            this.ClientSize = new System.Drawing.Size(764, 486);
            this.Controls.Add(mainTable);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "VirtualOpenFileDialog";
            this.ShowInTaskbar = false;
            this.Text = "Open";
            this.Load += new System.EventHandler(this.VirtualOpenFileDialog_Load);
            mainTable.ResumeLayout(false);
            mainTable.PerformLayout();
            toolStrip.ResumeLayout(false);
            toolStrip.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TreeView directoryTree;
        private System.Windows.Forms.ListView fileList;
        private System.Windows.Forms.ImageList iconList;
        private System.Windows.Forms.TextBox filename;
        private System.Windows.Forms.ComboBox fileType;
        private System.Windows.Forms.CheckBox showHidden;
        private renderdocui.Controls.ToolStripSpringTextBox location;
        private System.Windows.Forms.ToolStripButton back;
        private System.Windows.Forms.ToolStripButton forward;
        private System.Windows.Forms.ToolStripButton up;
        private System.Windows.Forms.Button open;

    }
}