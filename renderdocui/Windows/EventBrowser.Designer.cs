namespace renderdocui.Windows
{
    partial class EventBrowser
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
            System.Windows.Forms.ToolStripLabel toolStripLabel3;
            System.Windows.Forms.ToolStripLabel toolStripLabel4;
            TreelistView.TreeListColumn treeListColumn1 = new TreelistView.TreeListColumn("EID", "EID");
            TreelistView.TreeListColumn treeListColumn2 = new TreelistView.TreeListColumn("Drawcall", "Draw #");
            TreelistView.TreeListColumn treeListColumn3 = new TreelistView.TreeListColumn("Name", "Name");
            TreelistView.TreeListColumn treeListColumn4 = new TreelistView.TreeListColumn("Duration", "Duration (µs)");
            this.toolStripLabel1 = new System.Windows.Forms.ToolStripLabel();
            this.toolStripLabel2 = new System.Windows.Forms.ToolStripLabel();
            this.toolStripContainer1 = new System.Windows.Forms.ToolStripContainer();
            this.eventView = new TreelistView.TreeListView();
            this.eventViewRightClick = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.expandAll = new System.Windows.Forms.ToolStripMenuItem();
            this.collapseAll = new System.Windows.Forms.ToolStripMenuItem();
            this.selectVisibleColumnsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.findEventButton = new System.Windows.Forms.ToolStripButton();
            this.jumpEventButton = new System.Windows.Forms.ToolStripButton();
            this.timeDraws = new System.Windows.Forms.ToolStripButton();
            this.selectColumnsButton = new System.Windows.Forms.ToolStripButton();
            this.toggleBookmark = new System.Windows.Forms.ToolStripButton();
            this.export = new System.Windows.Forms.ToolStripButton();
            this.jumpStrip = new System.Windows.Forms.ToolStrip();
            this.jumpToEID = new renderdocui.Controls.ToolStripSpringTextBox();
            this.findStrip = new System.Windows.Forms.ToolStrip();
            this.findEvent = new renderdocui.Controls.ToolStripSpringTextBox();
            this.closeFind = new System.Windows.Forms.ToolStripButton();
            this.findNext = new System.Windows.Forms.ToolStripButton();
            this.findPrev = new System.Windows.Forms.ToolStripButton();
            this.bookmarkStrip = new System.Windows.Forms.ToolStrip();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.findHighlight = new System.Windows.Forms.Timer(this.components);
            this.exportDialog = new System.Windows.Forms.SaveFileDialog();
            this.prevDraw = new System.Windows.Forms.ToolStripButton();
            this.nextDraw = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
            toolStripLabel3 = new System.Windows.Forms.ToolStripLabel();
            toolStripLabel4 = new System.Windows.Forms.ToolStripLabel();
            this.toolStripContainer1.ContentPanel.SuspendLayout();
            this.toolStripContainer1.TopToolStripPanel.SuspendLayout();
            this.toolStripContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.eventView)).BeginInit();
            this.eventViewRightClick.SuspendLayout();
            this.toolStrip1.SuspendLayout();
            this.jumpStrip.SuspendLayout();
            this.findStrip.SuspendLayout();
            this.bookmarkStrip.SuspendLayout();
            this.SuspendLayout();
            // 
            // toolStripLabel3
            // 
            toolStripLabel3.Name = "toolStripLabel3";
            toolStripLabel3.Size = new System.Drawing.Size(47, 22);
            toolStripLabel3.Text = "Controls";
            // 
            // toolStripLabel4
            // 
            toolStripLabel4.Image = global::renderdocui.Properties.Resources.asterisk_orange;
            toolStripLabel4.Name = "toolStripLabel4";
            toolStripLabel4.Size = new System.Drawing.Size(74, 22);
            toolStripLabel4.Text = "Bookmarks";
            // 
            // toolStripLabel1
            // 
            this.toolStripLabel1.Image = global::renderdocui.Properties.Resources.flag_green;
            this.toolStripLabel1.Name = "toolStripLabel1";
            this.toolStripLabel1.Size = new System.Drawing.Size(92, 22);
            this.toolStripLabel1.Text = "Jump to Event";
            // 
            // toolStripLabel2
            // 
            this.toolStripLabel2.Image = global::renderdocui.Properties.Resources.find;
            this.toolStripLabel2.Name = "toolStripLabel2";
            this.toolStripLabel2.Size = new System.Drawing.Size(74, 22);
            this.toolStripLabel2.Text = "Find Event";
            // 
            // toolStripContainer1
            // 
            this.toolStripContainer1.BottomToolStripPanelVisible = false;
            // 
            // toolStripContainer1.ContentPanel
            // 
            this.toolStripContainer1.ContentPanel.Controls.Add(this.eventView);
            this.toolStripContainer1.ContentPanel.Size = new System.Drawing.Size(285, 238);
            this.toolStripContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.toolStripContainer1.LeftToolStripPanelVisible = false;
            this.toolStripContainer1.Location = new System.Drawing.Point(0, 0);
            this.toolStripContainer1.Name = "toolStripContainer1";
            this.toolStripContainer1.RightToolStripPanelVisible = false;
            this.toolStripContainer1.Size = new System.Drawing.Size(285, 338);
            this.toolStripContainer1.TabIndex = 1;
            this.toolStripContainer1.Text = "toolStripContainer1";
            // 
            // toolStripContainer1.TopToolStripPanel
            // 
            this.toolStripContainer1.TopToolStripPanel.Controls.Add(this.toolStrip1);
            this.toolStripContainer1.TopToolStripPanel.Controls.Add(this.jumpStrip);
            this.toolStripContainer1.TopToolStripPanel.Controls.Add(this.findStrip);
            this.toolStripContainer1.TopToolStripPanel.Controls.Add(this.bookmarkStrip);
            // 
            // eventView
            // 
            this.eventView.AlwaysDisplayVScroll = true;
            treeListColumn1.AutoSizeMinSize = 15;
            treeListColumn1.Width = 80;
            treeListColumn2.AutoSizeMinSize = 15;
            treeListColumn2.Width = 45;
            treeListColumn3.AutoSize = true;
            treeListColumn3.AutoSizeMinSize = 100;
            treeListColumn3.Width = 200;
            treeListColumn4.AutoSizeMinSize = 20;
            treeListColumn4.Width = 75;
            this.eventView.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3,
            treeListColumn4});
            this.eventView.ContextMenuStrip = this.eventViewRightClick;
            this.eventView.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.eventView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.eventView.Location = new System.Drawing.Point(0, 0);
            this.eventView.MultiSelect = false;
            this.eventView.Name = "eventView";
            this.eventView.RowOptions.ShowHeader = false;
            this.eventView.SelectedImage = global::renderdocui.Properties.Resources.flag_green;
            this.eventView.Size = new System.Drawing.Size(285, 238);
            this.eventView.TabIndex = 0;
            this.eventView.TreeColumn = 2;
            this.eventView.ViewOptions.HoverHandTreeColumn = false;
            this.eventView.ViewOptions.Indent = 12;
            this.eventView.ViewOptions.UserRearrangeableColumns = true;
            this.eventView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.eventView_AfterSelect);
            this.eventView.KeyDown += new System.Windows.Forms.KeyEventHandler(this.eventView_KeyDown);
            // 
            // eventViewRightClick
            // 
            this.eventViewRightClick.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.expandAll,
            this.collapseAll,
            this.selectVisibleColumnsToolStripMenuItem});
            this.eventViewRightClick.Name = "contextMenuStrip1";
            this.eventViewRightClick.Size = new System.Drawing.Size(179, 70);
            this.eventViewRightClick.Opening += new System.ComponentModel.CancelEventHandler(this.eventViewRightClick_Opening);
            // 
            // expandAll
            // 
            this.expandAll.Image = global::renderdocui.Properties.Resources.fit_window;
            this.expandAll.Name = "expandAll";
            this.expandAll.Size = new System.Drawing.Size(178, 22);
            this.expandAll.Text = "&Expand All";
            this.expandAll.Click += new System.EventHandler(this.expandAll_Click);
            // 
            // collapseAll
            // 
            this.collapseAll.Image = global::renderdocui.Properties.Resources.arrow_in;
            this.collapseAll.Name = "collapseAll";
            this.collapseAll.Size = new System.Drawing.Size(178, 22);
            this.collapseAll.Text = "&Collapse All";
            this.collapseAll.Click += new System.EventHandler(this.collapseAll_Click);
            // 
            // selectVisibleColumnsToolStripMenuItem
            // 
            this.selectVisibleColumnsToolStripMenuItem.Image = global::renderdocui.Properties.Resources.timeline_marker;
            this.selectVisibleColumnsToolStripMenuItem.Name = "selectVisibleColumnsToolStripMenuItem";
            this.selectVisibleColumnsToolStripMenuItem.Size = new System.Drawing.Size(178, 22);
            this.selectVisibleColumnsToolStripMenuItem.Text = "Select &Visible Columns";
            this.selectVisibleColumnsToolStripMenuItem.Click += new System.EventHandler(this.selectVisibleColumnsToolStripMenuItem_Click);
            // 
            // toolStrip1
            // 
            this.toolStrip1.Dock = System.Windows.Forms.DockStyle.None;
            this.toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            toolStripLabel3,
            this.toolStripSeparator1,
            this.prevDraw,
            this.nextDraw,
            this.toolStripSeparator3,
            this.findEventButton,
            this.jumpEventButton,
            this.timeDraws,
            this.selectColumnsButton,
            this.toggleBookmark,
            this.export});
            this.toolStrip1.LayoutStyle = System.Windows.Forms.ToolStripLayoutStyle.HorizontalStackWithOverflow;
            this.toolStrip1.Location = new System.Drawing.Point(0, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.Size = new System.Drawing.Size(285, 25);
            this.toolStrip1.Stretch = true;
            this.toolStrip1.TabIndex = 2;
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(6, 25);
            // 
            // findEventButton
            // 
            this.findEventButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.findEventButton.Image = global::renderdocui.Properties.Resources.find;
            this.findEventButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.findEventButton.Name = "findEventButton";
            this.findEventButton.Size = new System.Drawing.Size(23, 22);
            this.findEventButton.Text = "Find event by string";
            this.findEventButton.ToolTipText = "Find event by string";
            this.findEventButton.Click += new System.EventHandler(this.findEventButton_Click);
            // 
            // jumpEventButton
            // 
            this.jumpEventButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.jumpEventButton.Image = global::renderdocui.Properties.Resources.flag_green;
            this.jumpEventButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.jumpEventButton.Name = "jumpEventButton";
            this.jumpEventButton.Size = new System.Drawing.Size(23, 22);
            this.jumpEventButton.Text = "Jump to an Event by EID";
            this.jumpEventButton.ToolTipText = "Jump to an Event by EID";
            this.jumpEventButton.Click += new System.EventHandler(this.jumpEventButton_Click);
            // 
            // timeDraws
            // 
            this.timeDraws.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.timeDraws.Image = global::renderdocui.Properties.Resources.time;
            this.timeDraws.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.timeDraws.Name = "timeDraws";
            this.timeDraws.Size = new System.Drawing.Size(23, 22);
            this.timeDraws.Text = "Time durations for the drawcalls";
            this.timeDraws.ToolTipText = "Time durations for the drawcalls";
            this.timeDraws.Click += new System.EventHandler(this.timeDraws_Click);
            // 
            // selectColumnsButton
            // 
            this.selectColumnsButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.selectColumnsButton.Image = global::renderdocui.Properties.Resources.timeline_marker;
            this.selectColumnsButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.selectColumnsButton.Name = "selectColumnsButton";
            this.selectColumnsButton.Size = new System.Drawing.Size(23, 22);
            this.selectColumnsButton.Text = "Select visible columns";
            this.selectColumnsButton.Click += new System.EventHandler(this.selectColumnsButton_Click);
            // 
            // toggleBookmark
            // 
            this.toggleBookmark.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.toggleBookmark.Image = global::renderdocui.Properties.Resources.asterisk_orange;
            this.toggleBookmark.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.toggleBookmark.Name = "toggleBookmark";
            this.toggleBookmark.Size = new System.Drawing.Size(23, 22);
            this.toggleBookmark.Text = "Toggle Bookmark";
            this.toggleBookmark.Click += new System.EventHandler(this.toggleBookmark_Click);
            // 
            // export
            // 
            this.export.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.export.Image = global::renderdocui.Properties.Resources.save;
            this.export.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.export.Name = "export";
            this.export.Size = new System.Drawing.Size(23, 22);
            this.export.Text = "Export";
            this.export.Click += new System.EventHandler(this.export_Click);
            // 
            // jumpStrip
            // 
            this.jumpStrip.Dock = System.Windows.Forms.DockStyle.None;
            this.jumpStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.jumpStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripLabel1,
            this.jumpToEID});
            this.jumpStrip.Location = new System.Drawing.Point(0, 25);
            this.jumpStrip.Name = "jumpStrip";
            this.jumpStrip.Size = new System.Drawing.Size(285, 25);
            this.jumpStrip.Stretch = true;
            this.jumpStrip.TabIndex = 0;
            // 
            // jumpToEID
            // 
            this.jumpToEID.Name = "jumpToEID";
            this.jumpToEID.Size = new System.Drawing.Size(159, 25);
            this.jumpToEID.ToolTipText = "Jump to event by event ID";
            this.jumpToEID.Leave += new System.EventHandler(this.jumpFind_Leave);
            this.jumpToEID.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.jumpToEID_KeyPress);
            this.jumpToEID.TextChanged += new System.EventHandler(this.jumpToEID_TextChanged);
            // 
            // findStrip
            // 
            this.findStrip.Dock = System.Windows.Forms.DockStyle.None;
            this.findStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.findStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripLabel2,
            this.findEvent,
            this.closeFind,
            this.findNext,
            this.findPrev});
            this.findStrip.Location = new System.Drawing.Point(0, 50);
            this.findStrip.Name = "findStrip";
            this.findStrip.Size = new System.Drawing.Size(285, 25);
            this.findStrip.Stretch = true;
            this.findStrip.TabIndex = 1;
            // 
            // findEvent
            // 
            this.findEvent.Name = "findEvent";
            this.findEvent.Size = new System.Drawing.Size(108, 25);
            this.findEvent.ToolTipText = "Find an event by type or name";
            this.findEvent.Leave += new System.EventHandler(this.jumpFind_Leave);
            this.findEvent.KeyDown += new System.Windows.Forms.KeyEventHandler(this.findEvent_KeyDown);
            this.findEvent.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.findEvent_KeyPress);
            this.findEvent.TextChanged += new System.EventHandler(this.findEvent_TextChanged);
            // 
            // closeFind
            // 
            this.closeFind.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.closeFind.Image = global::renderdocui.Properties.Resources.cross;
            this.closeFind.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.closeFind.Name = "closeFind";
            this.closeFind.Size = new System.Drawing.Size(23, 22);
            this.closeFind.Text = "Close";
            this.closeFind.ToolTipText = "Close search";
            this.closeFind.Click += new System.EventHandler(this.closeFind_Click);
            // 
            // findNext
            // 
            this.findNext.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.findNext.Image = global::renderdocui.Properties.Resources.stepnext;
            this.findNext.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.findNext.Name = "findNext";
            this.findNext.Size = new System.Drawing.Size(23, 22);
            this.findNext.Text = "Find Next Match";
            this.findNext.Click += new System.EventHandler(this.findNext_Click);
            // 
            // findPrev
            // 
            this.findPrev.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.findPrev.Image = global::renderdocui.Properties.Resources.stepprev;
            this.findPrev.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.findPrev.Name = "findPrev";
            this.findPrev.Size = new System.Drawing.Size(23, 22);
            this.findPrev.Text = "Find Previous Match";
            this.findPrev.Click += new System.EventHandler(this.findPrev_Click);
            // 
            // bookmarkStrip
            // 
            this.bookmarkStrip.Dock = System.Windows.Forms.DockStyle.None;
            this.bookmarkStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.bookmarkStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            toolStripLabel4,
            this.toolStripSeparator2});
            this.bookmarkStrip.Location = new System.Drawing.Point(0, 75);
            this.bookmarkStrip.Name = "bookmarkStrip";
            this.bookmarkStrip.Size = new System.Drawing.Size(285, 25);
            this.bookmarkStrip.Stretch = true;
            this.bookmarkStrip.TabIndex = 3;
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(6, 25);
            // 
            // findHighlight
            // 
            this.findHighlight.Interval = 400;
            this.findHighlight.Tick += new System.EventHandler(this.findHighlight_Tick);
            // 
            // exportDialog
            // 
            this.exportDialog.DefaultExt = "txt";
            this.exportDialog.Filter = "Text Files (*.txt)|*.txt";
            this.exportDialog.Title = "Save Event List";
            // 
            // prevDraw
            // 
            this.prevDraw.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.prevDraw.Image = global::renderdocui.Properties.Resources.back;
            this.prevDraw.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.prevDraw.Name = "prevDraw";
            this.prevDraw.Size = new System.Drawing.Size(23, 22);
            this.prevDraw.Text = "Go to Previous Drawcall";
            this.prevDraw.Click += new System.EventHandler(this.prevDraw_Click);
            // 
            // nextDraw
            // 
            this.nextDraw.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.nextDraw.Image = global::renderdocui.Properties.Resources.forward;
            this.nextDraw.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.nextDraw.Name = "nextDraw";
            this.nextDraw.Size = new System.Drawing.Size(23, 22);
            this.nextDraw.Text = "Go To Next Drawcall";
            this.nextDraw.Click += new System.EventHandler(this.nextDraw_Click);
            // 
            // toolStripSeparator3
            // 
            this.toolStripSeparator3.Name = "toolStripSeparator3";
            this.toolStripSeparator3.Size = new System.Drawing.Size(6, 25);
            // 
            // EventBrowser
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(285, 338);
            this.Controls.Add(this.toolStripContainer1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "EventBrowser";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockLeft;
            this.Text = "Event Browser";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.EventBrowser_FormClosed);
            this.Leave += new System.EventHandler(this.EventBrowser_Leave);
            this.toolStripContainer1.ContentPanel.ResumeLayout(false);
            this.toolStripContainer1.TopToolStripPanel.ResumeLayout(false);
            this.toolStripContainer1.TopToolStripPanel.PerformLayout();
            this.toolStripContainer1.ResumeLayout(false);
            this.toolStripContainer1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.eventView)).EndInit();
            this.eventViewRightClick.ResumeLayout(false);
            this.toolStrip1.ResumeLayout(false);
            this.toolStrip1.PerformLayout();
            this.jumpStrip.ResumeLayout(false);
            this.jumpStrip.PerformLayout();
            this.findStrip.ResumeLayout(false);
            this.findStrip.PerformLayout();
            this.bookmarkStrip.ResumeLayout(false);
            this.bookmarkStrip.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView eventView;
        private System.Windows.Forms.ToolStripContainer toolStripContainer1;
        private System.Windows.Forms.ToolStrip jumpStrip;
        private renderdocui.Controls.ToolStripSpringTextBox jumpToEID;
        private System.Windows.Forms.ToolStrip findStrip;
        private renderdocui.Controls.ToolStripSpringTextBox findEvent;
        private System.Windows.Forms.ToolStripButton closeFind;
        private System.Windows.Forms.ToolStripLabel toolStripLabel2;
        private System.Windows.Forms.ToolStripLabel toolStripLabel1;
        private System.Windows.Forms.ToolStripButton findNext;
        private System.Windows.Forms.ToolStripButton findPrev;
        private System.Windows.Forms.ToolStrip toolStrip1;
        private System.Windows.Forms.ToolStripButton findEventButton;
        private System.Windows.Forms.ToolStripButton jumpEventButton;
        private System.Windows.Forms.ToolStripButton timeDraws;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripButton selectColumnsButton;
        private System.Windows.Forms.ContextMenuStrip eventViewRightClick;
        private System.Windows.Forms.ToolStripMenuItem selectVisibleColumnsToolStripMenuItem;
        private System.Windows.Forms.Timer findHighlight;
        private System.Windows.Forms.ToolStrip bookmarkStrip;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripButton toggleBookmark;
        private System.Windows.Forms.ToolStripMenuItem expandAll;
        private System.Windows.Forms.ToolStripMenuItem collapseAll;
        private System.Windows.Forms.ToolStripButton export;
        private System.Windows.Forms.SaveFileDialog exportDialog;
        private System.Windows.Forms.ToolStripButton prevDraw;
        private System.Windows.Forms.ToolStripButton nextDraw;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;

    }
}