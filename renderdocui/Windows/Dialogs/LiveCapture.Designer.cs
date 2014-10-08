﻿namespace renderdocui.Windows
{
    partial class LiveCapture
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
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.GroupBox groupBox1;
            System.Windows.Forms.ToolStrip toolStrip1;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(LiveCapture));
            this.queueCap = new System.Windows.Forms.Button();
            this.captureFrame = new System.Windows.Forms.NumericUpDown();
            this.captureDelay = new System.Windows.Forms.NumericUpDown();
            this.triggerCapture = new System.Windows.Forms.Button();
            this.deleteMenu = new System.Windows.Forms.ToolStripButton();
            this.saveMenu = new System.Windows.Forms.ToolStripButton();
            this.openMenu = new System.Windows.Forms.ToolStripSplitButton();
            this.openToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.newInstanceToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.captures = new System.Windows.Forms.ListView();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this.connectionIcon = new System.Windows.Forms.Label();
            this.connectionStatus = new System.Windows.Forms.Label();
            this.childProcessLabel = new System.Windows.Forms.Label();
            this.childProcesses = new System.Windows.Forms.ListView();
            this.rightclickContext = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.openThisCaptureToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.newInstanceToolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
            this.saveThisCaptureToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.deleteThisCaptureToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.captureCountdown = new System.Windows.Forms.Timer(this.components);
            this.childUpdateTimer = new System.Windows.Forms.Timer(this.components);
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            groupBox1 = new System.Windows.Forms.GroupBox();
            toolStrip1 = new System.Windows.Forms.ToolStrip();
            groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.captureFrame)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.captureDelay)).BeginInit();
            toolStrip1.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.flowLayoutPanel1.SuspendLayout();
            this.rightclickContext.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(3, 205);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(98, 13);
            label1.TabIndex = 6;
            label1.Text = "Captures collected:";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(6, 21);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(74, 13);
            label2.TabIndex = 2;
            label2.Text = "Capture Delay";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(6, 50);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(86, 13);
            label3.TabIndex = 3;
            label3.Text = "Capture Frame #";
            // 
            // groupBox1
            // 
            groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox1.Controls.Add(this.queueCap);
            groupBox1.Controls.Add(this.captureFrame);
            groupBox1.Controls.Add(label3);
            groupBox1.Controls.Add(label2);
            groupBox1.Controls.Add(this.captureDelay);
            groupBox1.Controls.Add(this.triggerCapture);
            groupBox1.Location = new System.Drawing.Point(3, 34);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(351, 75);
            groupBox1.TabIndex = 7;
            groupBox1.TabStop = false;
            groupBox1.Text = "Tools";
            // 
            // queueCap
            // 
            this.queueCap.Location = new System.Drawing.Point(222, 44);
            this.queueCap.Name = "queueCap";
            this.queueCap.Size = new System.Drawing.Size(92, 23);
            this.queueCap.TabIndex = 4;
            this.queueCap.Text = "Queue Capture";
            this.queueCap.UseVisualStyleBackColor = true;
            this.queueCap.Click += new System.EventHandler(this.queueCap_Click);
            // 
            // captureFrame
            // 
            this.captureFrame.Location = new System.Drawing.Point(96, 46);
            this.captureFrame.Maximum = new decimal(new int[] {
            10000000,
            0,
            0,
            0});
            this.captureFrame.Minimum = new decimal(new int[] {
            2,
            0,
            0,
            0});
            this.captureFrame.Name = "captureFrame";
            this.captureFrame.Size = new System.Drawing.Size(120, 20);
            this.captureFrame.TabIndex = 3;
            this.captureFrame.ThousandsSeparator = true;
            this.captureFrame.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
            // 
            // captureDelay
            // 
            this.captureDelay.Location = new System.Drawing.Point(96, 19);
            this.captureDelay.Name = "captureDelay";
            this.captureDelay.Size = new System.Drawing.Size(120, 20);
            this.captureDelay.TabIndex = 1;
            // 
            // triggerCapture
            // 
            this.triggerCapture.Location = new System.Drawing.Point(222, 17);
            this.triggerCapture.Name = "triggerCapture";
            this.triggerCapture.Size = new System.Drawing.Size(92, 23);
            this.triggerCapture.TabIndex = 2;
            this.triggerCapture.Text = "Trigger Capture";
            this.triggerCapture.UseVisualStyleBackColor = true;
            this.triggerCapture.Click += new System.EventHandler(this.triggerCapture_Click);
            // 
            // toolStrip1
            // 
            toolStrip1.CanOverflow = false;
            toolStrip1.Dock = System.Windows.Forms.DockStyle.Fill;
            toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.deleteMenu,
            this.saveMenu,
            this.openMenu});
            toolStrip1.Location = new System.Drawing.Point(0, 375);
            toolStrip1.Name = "toolStrip1";
            toolStrip1.RightToLeft = System.Windows.Forms.RightToLeft.Yes;
            toolStrip1.Size = new System.Drawing.Size(357, 25);
            toolStrip1.Stretch = true;
            toolStrip1.TabIndex = 8;
            toolStrip1.Text = "toolStrip1";
            // 
            // deleteMenu
            // 
            this.deleteMenu.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.deleteMenu.Image = ((System.Drawing.Image)(resources.GetObject("deleteMenu.Image")));
            this.deleteMenu.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.deleteMenu.Name = "deleteMenu";
            this.deleteMenu.Size = new System.Drawing.Size(42, 22);
            this.deleteMenu.Text = "Delete";
            this.deleteMenu.Click += new System.EventHandler(this.deleteCapture_Click);
            // 
            // saveMenu
            // 
            this.saveMenu.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.saveMenu.Image = ((System.Drawing.Image)(resources.GetObject("saveMenu.Image")));
            this.saveMenu.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.saveMenu.Name = "saveMenu";
            this.saveMenu.Size = new System.Drawing.Size(35, 22);
            this.saveMenu.Text = "Save";
            this.saveMenu.Click += new System.EventHandler(this.saveCapture_Click);
            // 
            // openMenu
            // 
            this.openMenu.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.openMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openToolStripMenuItem,
            this.newInstanceToolStripMenuItem});
            this.openMenu.Image = ((System.Drawing.Image)(resources.GetObject("openMenu.Image")));
            this.openMenu.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.openMenu.Name = "openMenu";
            this.openMenu.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.openMenu.Size = new System.Drawing.Size(49, 22);
            this.openMenu.Text = "Open";
            this.openMenu.ButtonClick += new System.EventHandler(this.openCapture_Click);
            // 
            // openToolStripMenuItem
            // 
            this.openToolStripMenuItem.Name = "openToolStripMenuItem";
            this.openToolStripMenuItem.Size = new System.Drawing.Size(150, 22);
            this.openToolStripMenuItem.Text = "In &this instance";
            this.openToolStripMenuItem.Click += new System.EventHandler(this.openCapture_Click);
            // 
            // newInstanceToolStripMenuItem
            // 
            this.newInstanceToolStripMenuItem.Name = "newInstanceToolStripMenuItem";
            this.newInstanceToolStripMenuItem.Size = new System.Drawing.Size(150, 22);
            this.newInstanceToolStripMenuItem.Text = "In &new instance";
            this.newInstanceToolStripMenuItem.Click += new System.EventHandler(this.openNewWindow_Click);
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.Controls.Add(this.captures, 0, 5);
            this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel1, 0, 0);
            this.tableLayoutPanel1.Controls.Add(label1, 0, 4);
            this.tableLayoutPanel1.Controls.Add(groupBox1, 0, 1);
            this.tableLayoutPanel1.Controls.Add(toolStrip1, 0, 6);
            this.tableLayoutPanel1.Controls.Add(this.childProcessLabel, 0, 2);
            this.tableLayoutPanel1.Controls.Add(this.childProcesses, 0, 3);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 7;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 32F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.Size = new System.Drawing.Size(357, 400);
            this.tableLayoutPanel1.TabIndex = 0;
            // 
            // captures
            // 
            this.captures.Dock = System.Windows.Forms.DockStyle.Fill;
            this.captures.FullRowSelect = true;
            this.captures.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
            this.captures.Location = new System.Drawing.Point(3, 221);
            this.captures.Name = "captures";
            this.captures.Size = new System.Drawing.Size(351, 151);
            this.captures.TabIndex = 6;
            this.captures.TileSize = new System.Drawing.Size(300, 100);
            this.captures.UseCompatibleStateImageBehavior = false;
            this.captures.View = System.Windows.Forms.View.Tile;
            this.captures.ItemSelectionChanged += new System.Windows.Forms.ListViewItemSelectionChangedEventHandler(this.captures_ItemSelectionChanged);
            this.captures.KeyUp += new System.Windows.Forms.KeyEventHandler(this.captures_KeyUp);
            this.captures.MouseClick += new System.Windows.Forms.MouseEventHandler(this.captures_MouseClick);
            this.captures.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.captures_MouseDoubleClick);
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.Controls.Add(this.connectionIcon);
            this.flowLayoutPanel1.Controls.Add(this.connectionStatus);
            this.flowLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.flowLayoutPanel1.Location = new System.Drawing.Point(3, 3);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(351, 25);
            this.flowLayoutPanel1.TabIndex = 4;
            this.flowLayoutPanel1.WrapContents = false;
            // 
            // connectionIcon
            // 
            this.connectionIcon.Image = global::renderdocui.Properties.Resources.hourglass;
            this.connectionIcon.Location = new System.Drawing.Point(3, 0);
            this.connectionIcon.Name = "connectionIcon";
            this.connectionIcon.Size = new System.Drawing.Size(21, 23);
            this.connectionIcon.TabIndex = 1;
            this.connectionIcon.Text = "     ";
            // 
            // connectionStatus
            // 
            this.connectionStatus.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.connectionStatus.AutoSize = true;
            this.connectionStatus.Location = new System.Drawing.Point(30, 5);
            this.connectionStatus.Name = "connectionStatus";
            this.connectionStatus.Size = new System.Drawing.Size(70, 13);
            this.connectionStatus.TabIndex = 2;
            this.connectionStatus.Text = "Connecting...";
            this.connectionStatus.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // childProcessLabel
            // 
            this.childProcessLabel.Location = new System.Drawing.Point(3, 112);
            this.childProcessLabel.Name = "childProcessLabel";
            this.childProcessLabel.Size = new System.Drawing.Size(85, 28);
            this.childProcessLabel.TabIndex = 9;
            this.childProcessLabel.Text = "Child Processes:";
            this.childProcessLabel.TextAlign = System.Drawing.ContentAlignment.BottomLeft;
            // 
            // childProcesses
            // 
            this.childProcesses.Dock = System.Windows.Forms.DockStyle.Fill;
            this.childProcesses.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
            this.childProcesses.Location = new System.Drawing.Point(3, 143);
            this.childProcesses.MinimumSize = new System.Drawing.Size(0, 40);
            this.childProcesses.Name = "childProcesses";
            this.childProcesses.Size = new System.Drawing.Size(351, 40);
            this.childProcesses.TabIndex = 5;
            this.childProcesses.UseCompatibleStateImageBehavior = false;
            this.childProcesses.View = System.Windows.Forms.View.Tile;
            this.childProcesses.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.childProcesses_MouseDoubleClick);
            // 
            // rightclickContext
            // 
            this.rightclickContext.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openThisCaptureToolStripMenuItem,
            this.saveThisCaptureToolStripMenuItem,
            this.deleteThisCaptureToolStripMenuItem});
            this.rightclickContext.Name = "rightclickContext";
            this.rightclickContext.Size = new System.Drawing.Size(124, 70);
            // 
            // openThisCaptureToolStripMenuItem
            // 
            this.openThisCaptureToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openToolStripMenuItem1,
            this.newInstanceToolStripMenuItem1});
            this.openThisCaptureToolStripMenuItem.Name = "openThisCaptureToolStripMenuItem";
            this.openThisCaptureToolStripMenuItem.Size = new System.Drawing.Size(123, 22);
            this.openThisCaptureToolStripMenuItem.Text = "&Open in...";
            // 
            // openToolStripMenuItem1
            // 
            this.openToolStripMenuItem1.Name = "openToolStripMenuItem1";
            this.openToolStripMenuItem1.Size = new System.Drawing.Size(138, 22);
            this.openToolStripMenuItem1.Text = "This instance";
            this.openToolStripMenuItem1.Click += new System.EventHandler(this.openCapture_Click);
            // 
            // newInstanceToolStripMenuItem1
            // 
            this.newInstanceToolStripMenuItem1.Name = "newInstanceToolStripMenuItem1";
            this.newInstanceToolStripMenuItem1.Size = new System.Drawing.Size(138, 22);
            this.newInstanceToolStripMenuItem1.Text = "New instance";
            this.newInstanceToolStripMenuItem1.Click += new System.EventHandler(this.openNewWindow_Click);
            // 
            // saveThisCaptureToolStripMenuItem
            // 
            this.saveThisCaptureToolStripMenuItem.Name = "saveThisCaptureToolStripMenuItem";
            this.saveThisCaptureToolStripMenuItem.Size = new System.Drawing.Size(123, 22);
            this.saveThisCaptureToolStripMenuItem.Text = "&Save";
            this.saveThisCaptureToolStripMenuItem.Click += new System.EventHandler(this.saveCapture_Click);
            // 
            // deleteThisCaptureToolStripMenuItem
            // 
            this.deleteThisCaptureToolStripMenuItem.Name = "deleteThisCaptureToolStripMenuItem";
            this.deleteThisCaptureToolStripMenuItem.Size = new System.Drawing.Size(123, 22);
            this.deleteThisCaptureToolStripMenuItem.Text = "&Delete";
            this.deleteThisCaptureToolStripMenuItem.Click += new System.EventHandler(this.deleteCapture_Click);
            // 
            // captureCountdown
            // 
            this.captureCountdown.Interval = 1000;
            this.captureCountdown.Tick += new System.EventHandler(this.captureCountdown_Tick);
            // 
            // childUpdateTimer
            // 
            this.childUpdateTimer.Enabled = true;
            this.childUpdateTimer.Tick += new System.EventHandler(this.childUpdateTimer_Tick);
            // 
            // LiveCapture
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(357, 400);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "LiveCapture";
            this.Text = "Connecting...";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.LiveCapture_FormClosing);
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.LiveCapture_FormClosed);
            this.Shown += new System.EventHandler(this.LiveCapture_Shown);
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.captureFrame)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.captureDelay)).EndInit();
            toolStrip1.ResumeLayout(false);
            toolStrip1.PerformLayout();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.flowLayoutPanel1.ResumeLayout(false);
            this.flowLayoutPanel1.PerformLayout();
            this.rightclickContext.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
        private System.Windows.Forms.Label connectionIcon;
        private System.Windows.Forms.Label connectionStatus;
        private System.Windows.Forms.ContextMenuStrip rightclickContext;
        private System.Windows.Forms.ToolStripMenuItem openThisCaptureToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem deleteThisCaptureToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem saveThisCaptureToolStripMenuItem;
        private System.Windows.Forms.Timer captureCountdown;
        private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem newInstanceToolStripMenuItem1;
        private System.Windows.Forms.Label childProcessLabel;
        private System.Windows.Forms.Timer childUpdateTimer;
        private System.Windows.Forms.ListView childProcesses;
        private System.Windows.Forms.Button queueCap;
        private System.Windows.Forms.NumericUpDown captureFrame;
        private System.Windows.Forms.NumericUpDown captureDelay;
        private System.Windows.Forms.Button triggerCapture;
        private System.Windows.Forms.ListView captures;
        private System.Windows.Forms.ToolStripButton deleteMenu;
        private System.Windows.Forms.ToolStripButton saveMenu;
        private System.Windows.Forms.ToolStripSplitButton openMenu;
        private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem newInstanceToolStripMenuItem;
    }
}