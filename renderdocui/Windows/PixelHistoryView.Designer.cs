namespace renderdocui.Windows
{
    partial class PixelHistoryView
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
            TreelistView.TreeListColumn treeListColumn1 = new TreelistView.TreeListColumn("Event", "Event");
            TreelistView.TreeListColumn treeListColumn2 = new TreelistView.TreeListColumn("Before", "");
            TreelistView.TreeListColumn treeListColumn3 = new TreelistView.TreeListColumn("BeforeCol", "");
            TreelistView.TreeListColumn treeListColumn4 = new TreelistView.TreeListColumn("After", "");
            TreelistView.TreeListColumn treeListColumn5 = new TreelistView.TreeListColumn("AfterCol", "");
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.events = new TreelistView.TreeListView();
            this.historyContext = new System.Windows.Forms.Label();
            this.eventsHidden = new System.Windows.Forms.Label();
            this.rightclickMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.hideFailedEventsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.debugToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.jumpToPrimitiveMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.events)).BeginInit();
            this.rightclickMenu.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.events, 0, 2);
            this.tableLayoutPanel1.Controls.Add(this.historyContext, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.eventsHidden, 0, 1);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 3;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(386, 478);
            this.tableLayoutPanel1.TabIndex = 2;
            // 
            // events
            // 
            treeListColumn1.AutoSize = true;
            treeListColumn1.AutoSizeMinSize = 20;
            treeListColumn1.CellFormat.Padding = new System.Windows.Forms.Padding(4);
            treeListColumn1.CellFormat.TextAlignment = System.Drawing.ContentAlignment.TopLeft;
            treeListColumn1.Width = 100;
            treeListColumn2.AutoSizeMinSize = 20;
            treeListColumn2.Width = 70;
            treeListColumn3.AutoSizeMinSize = 17;
            treeListColumn3.Width = 17;
            treeListColumn4.AutoSizeMinSize = 20;
            treeListColumn4.Width = 70;
            treeListColumn5.AutoSizeMinSize = 17;
            treeListColumn5.Width = 17;
            this.events.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3,
            treeListColumn4,
            treeListColumn5});
            this.events.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.events.Dock = System.Windows.Forms.DockStyle.Fill;
            this.events.Location = new System.Drawing.Point(3, 68);
            this.events.MultiSelect = false;
            this.events.Name = "events";
            this.events.RowOptions.ItemHeight = 120;
            this.events.RowOptions.ShowHeader = false;
            this.events.Size = new System.Drawing.Size(380, 407);
            this.events.TabIndex = 1;
            this.events.Text = "History Events";
            this.events.ViewOptions.ShowLine = false;
            this.events.NodeDoubleClicked += new TreelistView.TreeListView.NodeDoubleClickedHandler(this.events_NodeDoubleClicked);
            this.events.MouseClick += new System.Windows.Forms.MouseEventHandler(this.events_MouseClick);
            // 
            // historyContext
            // 
            this.historyContext.AutoSize = true;
            this.historyContext.Location = new System.Drawing.Point(3, 0);
            this.historyContext.Name = "historyContext";
            this.historyContext.Size = new System.Drawing.Size(378, 52);
            this.historyContext.TabIndex = 2;
            this.historyContext.Text = "***code overwritten preview*** Preview colours displayed in visible range {min} -" +
    " {max} with {red, blue, green} channels.\r\n\r\nRight click to debug an event, hi" +
    "de failed events, or jump to the modification's primitive in the mesh view.";
            // 
            // eventsHidden
            // 
            this.eventsHidden.AutoSize = true;
            this.eventsHidden.ForeColor = System.Drawing.Color.Red;
            this.eventsHidden.Location = new System.Drawing.Point(3, 52);
            this.eventsHidden.Name = "eventsHidden";
            this.eventsHidden.Size = new System.Drawing.Size(92, 13);
            this.eventsHidden.TabIndex = 3;
            this.eventsHidden.Text = "events are hidden";
            // 
            // rightclickMenu
            // 
            this.rightclickMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.hideFailedEventsToolStripMenuItem,
            this.jumpToPrimitiveMenuItem,
            this.debugToolStripMenuItem});
            this.rightclickMenu.Name = "rightclickMenu";
            this.rightclickMenu.Size = new System.Drawing.Size(161, 92);
            // 
            // hideFailedEventsToolStripMenuItem
            // 
            this.hideFailedEventsToolStripMenuItem.CheckOnClick = true;
            this.hideFailedEventsToolStripMenuItem.Name = "hideFailedEventsToolStripMenuItem";
            this.hideFailedEventsToolStripMenuItem.Size = new System.Drawing.Size(160, 22);
            this.hideFailedEventsToolStripMenuItem.Text = "&Hide failed events";
            this.hideFailedEventsToolStripMenuItem.CheckedChanged += new System.EventHandler(this.hideFailedEventsToolStripMenuItem_CheckedChanged);
            // 
            // debugToolStripMenuItem
            // 
            this.debugToolStripMenuItem.Name = "debugToolStripMenuItem";
            this.debugToolStripMenuItem.Size = new System.Drawing.Size(160, 22);
            this.debugToolStripMenuItem.Text = "&Debug pixel";
            this.debugToolStripMenuItem.Click += new System.EventHandler(this.debugToolStripMenuItem_Click);
            // 
            // jumpToPrimitiveMenuItem
            // 
            this.jumpToPrimitiveMenuItem.Name = "jumpToPrimitiveMenuItem";
            this.jumpToPrimitiveMenuItem.Size = new System.Drawing.Size(160, 22);
            this.jumpToPrimitiveMenuItem.Text = "Jump to Primitive";
            this.jumpToPrimitiveMenuItem.Click += new System.EventHandler(this.jumpToPrimitiveMenuItem_Click);
            // 
            // PixelHistoryView
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(386, 478);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "PixelHistoryView";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockRight;
            this.Text = "Pixel History";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.PixelHistoryView_FormClosed);
            this.Enter += new System.EventHandler(this.PixelHistoryView_Enter);
            this.Leave += new System.EventHandler(this.PixelHistoryView_Leave);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.events)).EndInit();
            this.rightclickMenu.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView events;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.Label historyContext;
        private System.Windows.Forms.Label eventsHidden;
        private System.Windows.Forms.ContextMenuStrip rightclickMenu;
        private System.Windows.Forms.ToolStripMenuItem hideFailedEventsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem debugToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem jumpToPrimitiveMenuItem;
    }
}