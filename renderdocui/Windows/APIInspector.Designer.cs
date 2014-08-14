﻿namespace renderdocui.Windows
{
    partial class APIInspector
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
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
            TreelistView.TreeListColumn treeListColumn1 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("EID", "EID")));
            TreelistView.TreeListColumn treeListColumn2 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Call", "API Call")));
            this.apiEvents = new TreelistView.TreeListView();
            this.callstacklabel = new System.Windows.Forms.Label();
            this.callstackSkipLevelsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.callSkip = new System.Windows.Forms.ToolStripTextBox();
            this.contextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.panelSplitter = new renderdocui.Controls.DoubleClickSplitter();
            this.callstack = new System.Windows.Forms.ListBox();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.apiEvents)).BeginInit();
            this.contextMenu.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.panelSplitter)).BeginInit();
            this.panelSplitter.Panel1.SuspendLayout();
            this.panelSplitter.Panel2.SuspendLayout();
            this.panelSplitter.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 1;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.Controls.Add(this.apiEvents, 0, 0);
            tableLayoutPanel1.Controls.Add(this.callstacklabel, 0, 1);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(292, 171);
            tableLayoutPanel1.TabIndex = 3;
            // 
            // apiEvents
            // 
            this.apiEvents.AlwaysDisplayVScroll = true;
            treeListColumn1.AutoSizeMinSize = 50;
            treeListColumn1.Width = 60;
            treeListColumn2.AutoSize = true;
            treeListColumn2.AutoSizeMinSize = 0;
            treeListColumn2.Width = 50;
            this.apiEvents.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2});
            this.apiEvents.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.apiEvents.Dock = System.Windows.Forms.DockStyle.Fill;
            this.apiEvents.Location = new System.Drawing.Point(3, 3);
            this.apiEvents.Name = "apiEvents";
            this.apiEvents.RowOptions.ShowHeader = false;
            this.apiEvents.Size = new System.Drawing.Size(286, 152);
            this.apiEvents.TabIndex = 1;
            this.apiEvents.Text = "API Calls";
            this.apiEvents.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.treeView1_AfterSelect);
            this.apiEvents.KeyDown += new System.Windows.Forms.KeyEventHandler(this.apiEvents_KeyDown);
            // 
            // callstacklabel
            // 
            this.callstacklabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.callstacklabel.AutoSize = true;
            this.callstacklabel.Cursor = System.Windows.Forms.Cursors.HSplit;
            this.callstacklabel.Location = new System.Drawing.Point(3, 158);
            this.callstacklabel.Name = "callstacklabel";
            this.callstacklabel.Size = new System.Drawing.Size(286, 13);
            this.callstacklabel.TabIndex = 3;
            this.callstacklabel.Text = "Callstack";
            this.callstacklabel.TextAlign = System.Drawing.ContentAlignment.BottomCenter;
            this.callstacklabel.DoubleClick += new System.EventHandler(this.callstacklabel_DoubleClick);
            // 
            // callstackSkipLevelsToolStripMenuItem
            // 
            this.callstackSkipLevelsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.callSkip});
            this.callstackSkipLevelsToolStripMenuItem.Name = "callstackSkipLevelsToolStripMenuItem";
            this.callstackSkipLevelsToolStripMenuItem.Size = new System.Drawing.Size(171, 22);
            this.callstackSkipLevelsToolStripMenuItem.Text = "Callstack Skip Levels";
            // 
            // callSkip
            // 
            this.callSkip.Name = "callSkip";
            this.callSkip.Size = new System.Drawing.Size(100, 21);
            this.callSkip.Text = "0";
            this.callSkip.ToolTipText = "Number of levels in the callstack to skip, from the bottom";
            this.callSkip.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.callSkip_KeyPress);
            // 
            // contextMenu
            // 
            this.contextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.callstackSkipLevelsToolStripMenuItem});
            this.contextMenu.Name = "contextMenuStrip1";
            this.contextMenu.Size = new System.Drawing.Size(172, 26);
            // 
            // panelSplitter
            // 
            this.panelSplitter.Collapsed = false;
            this.panelSplitter.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panelSplitter.Location = new System.Drawing.Point(0, 0);
            this.panelSplitter.Name = "panelSplitter";
            this.panelSplitter.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // panelSplitter.Panel1
            // 
            this.panelSplitter.Panel1.Controls.Add(tableLayoutPanel1);
            this.panelSplitter.Panel1Collapse = false;
            // 
            // panelSplitter.Panel2
            // 
            this.panelSplitter.Panel2.Controls.Add(this.callstack);
            this.panelSplitter.Size = new System.Drawing.Size(292, 273);
            this.panelSplitter.SplitterDistance = 171;
            this.panelSplitter.SplitterWidth = 7;
            this.panelSplitter.TabIndex = 4;
            // 
            // callstack
            // 
            this.callstack.Dock = System.Windows.Forms.DockStyle.Fill;
            this.callstack.FormattingEnabled = true;
            this.callstack.HorizontalScrollbar = true;
            this.callstack.Location = new System.Drawing.Point(0, 0);
            this.callstack.Name = "callstack";
            this.callstack.SelectionMode = System.Windows.Forms.SelectionMode.MultiExtended;
            this.callstack.Size = new System.Drawing.Size(292, 95);
            this.callstack.TabIndex = 2;
            this.callstack.KeyDown += new System.Windows.Forms.KeyEventHandler(this.callstack_KeyDown);
            // 
            // APIInspector
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(292, 273);
            this.Controls.Add(this.panelSplitter);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "APIInspector";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockLeft;
            this.Text = "API Calls";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.APIInspector_FormClosed);
            this.Shown += new System.EventHandler(this.APIEvents_Shown);
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.apiEvents)).EndInit();
            this.contextMenu.ResumeLayout(false);
            this.panelSplitter.Panel1.ResumeLayout(false);
            this.panelSplitter.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.panelSplitter)).EndInit();
            this.panelSplitter.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView apiEvents;
        private System.Windows.Forms.ListBox callstack;
        private System.Windows.Forms.ToolStripMenuItem callstackSkipLevelsToolStripMenuItem;
        private System.Windows.Forms.ToolStripTextBox callSkip;
        private System.Windows.Forms.ContextMenuStrip contextMenu;
        private Controls.DoubleClickSplitter panelSplitter;
        private System.Windows.Forms.Label callstacklabel;

    }
}