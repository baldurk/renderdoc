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
            this.events = new TreelistView.TreeListView();
            ((System.ComponentModel.ISupportInitialize)(this.events)).BeginInit();
            this.SuspendLayout();
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
            treeListColumn3.Width = 50;
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
            this.events.Location = new System.Drawing.Point(0, 0);
            this.events.MultiSelect = false;
            this.events.Name = "events";
            this.events.RowOptions.ItemHeight = 64;
            this.events.RowOptions.ShowHeader = false;
            this.events.Size = new System.Drawing.Size(386, 478);
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
            this.Controls.Add(this.events);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "PixelHistoryView";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockRight;
            this.Text = "Pixel History";
            ((System.ComponentModel.ISupportInitialize)(this.events)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private TreelistView.TreeListView events;
    }
}