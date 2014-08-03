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
            TreelistView.TreeListColumn treeListColumn1 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("EID", "EID")));
            TreelistView.TreeListColumn treeListColumn2 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Event", "Event")));
            TreelistView.TreeListColumn treeListColumn3 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Before", "Before")));
            TreelistView.TreeListColumn treeListColumn4 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("BeforeCol", "")));
            TreelistView.TreeListColumn treeListColumn5 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("After", "After")));
            TreelistView.TreeListColumn treeListColumn6 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("AfterCol", "")));
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.historyContext = new System.Windows.Forms.Label();
            this.eventsHidden = new System.Windows.Forms.Label();
            this.events = new TreelistView.TreeListView();
            this.tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.events)).BeginInit();
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
            // historyContext
            // 
            this.historyContext.AutoSize = true;
            this.historyContext.Location = new System.Drawing.Point(3, 0);
            this.historyContext.Name = "historyContext";
            this.historyContext.Size = new System.Drawing.Size(378, 52);
            this.historyContext.TabIndex = 2;
            this.historyContext.Text = "***code overwritten preview*** Preview colours displayed in visible range {min} -" +
    " {max} with {red, blue, green} channels.\r\n\r\nRight click to debug an event, or hi" +
    "de failed events.";
            // 
            // eventsHidden
            // 
            this.eventsHidden.AutoSize = true;
            this.eventsHidden.Location = new System.Drawing.Point(3, 52);
            this.eventsHidden.Name = "eventsHidden";
            this.eventsHidden.Size = new System.Drawing.Size(0, 13);
            this.eventsHidden.TabIndex = 3;
            // 
            // events
            // 
            treeListColumn1.AutoSizeMinSize = 10;
            treeListColumn1.CellFormat.Padding = new System.Windows.Forms.Padding(4);
            treeListColumn1.CellFormat.TextAlignment = System.Drawing.ContentAlignment.TopLeft;
            treeListColumn1.Width = 50;
            treeListColumn2.AutoSize = true;
            treeListColumn2.AutoSizeMinSize = 20;
            treeListColumn2.CellFormat.Padding = new System.Windows.Forms.Padding(4);
            treeListColumn2.CellFormat.TextAlignment = System.Drawing.ContentAlignment.TopLeft;
            treeListColumn2.Width = 100;
            treeListColumn3.AutoSizeMinSize = 10;
            treeListColumn3.Width = 60;
            treeListColumn4.AutoSizeMinSize = 20;
            treeListColumn4.Width = 40;
            treeListColumn5.AutoSizeMinSize = 20;
            treeListColumn5.Width = 60;
            treeListColumn6.AutoSizeMinSize = 20;
            treeListColumn6.Width = 40;
            this.events.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3,
            treeListColumn4,
            treeListColumn5,
            treeListColumn6});
            this.events.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.events.Dock = System.Windows.Forms.DockStyle.Fill;
            this.events.Location = new System.Drawing.Point(3, 68);
            this.events.MultiSelect = false;
            this.events.Name = "events";
            this.events.RowOptions.ItemHeight = 96;
            this.events.RowOptions.ShowHeader = false;
            this.events.Size = new System.Drawing.Size(380, 407);
            this.events.TabIndex = 1;
            this.events.Text = "History Events";
            this.events.ViewOptions.ShowLine = false;
            this.events.ViewOptions.ShowPlusMinus = false;
            this.events.NodeDoubleClicked += new TreelistView.TreeListView.NodeDoubleClickedHandler(this.events_NodeDoubleClicked);
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
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.events)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView events;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.Label historyContext;
        private System.Windows.Forms.Label eventsHidden;
    }
}