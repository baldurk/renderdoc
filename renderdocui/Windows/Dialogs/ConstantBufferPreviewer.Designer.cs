namespace renderdocui.Controls
{
    partial class ConstantBufferPreviewer
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConstantBufferPreviewer));
            TreelistView.TreeListColumn treeListColumn7 = new TreelistView.TreeListColumn("VarName", "Name");
            TreelistView.TreeListColumn treeListColumn8 = new TreelistView.TreeListColumn("VarValue", "Value");
            TreelistView.TreeListColumn treeListColumn9 = new TreelistView.TreeListColumn("VarType", "Type");
            this.tableLayout = new System.Windows.Forms.TableLayoutPanel();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.slotLabel = new System.Windows.Forms.ToolStripLabel();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.nameLabel = new System.Windows.Forms.ToolStripLabel();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.setFormat = new System.Windows.Forms.ToolStripButton();
            this.split = new System.Windows.Forms.SplitContainer();
            this.variables = new TreelistView.TreeListView();
            this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
            this.saveCSV = new System.Windows.Forms.ToolStripButton();
            this.exportDialog = new System.Windows.Forms.SaveFileDialog();
            this.tableLayout.SuspendLayout();
            this.toolStrip1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.split)).BeginInit();
            this.split.Panel1.SuspendLayout();
            this.split.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.variables)).BeginInit();
            this.SuspendLayout();
            // 
            // tableLayout
            // 
            this.tableLayout.ColumnCount = 1;
            this.tableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayout.Controls.Add(this.toolStrip1, 0, 0);
            this.tableLayout.Controls.Add(this.split, 0, 1);
            this.tableLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayout.Location = new System.Drawing.Point(0, 0);
            this.tableLayout.Name = "tableLayout";
            this.tableLayout.RowCount = 2;
            this.tableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayout.Size = new System.Drawing.Size(491, 330);
            this.tableLayout.TabIndex = 0;
            // 
            // toolStrip1
            // 
            this.toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.slotLabel,
            this.toolStripSeparator1,
            this.nameLabel,
            this.toolStripSeparator2,
            this.setFormat,
            this.toolStripSeparator3,
            this.saveCSV});
            this.toolStrip1.Location = new System.Drawing.Point(0, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.Size = new System.Drawing.Size(491, 25);
            this.toolStrip1.TabIndex = 4;
            this.toolStrip1.Text = "toolStrip1";
            // 
            // slotLabel
            // 
            this.slotLabel.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.slotLabel.Name = "slotLabel";
            this.slotLabel.Size = new System.Drawing.Size(35, 22);
            this.slotLabel.Text = "    ";
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(6, 25);
            // 
            // nameLabel
            // 
            this.nameLabel.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.nameLabel.Name = "nameLabel";
            this.nameLabel.Size = new System.Drawing.Size(42, 22);
            this.nameLabel.Text = "     ";
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(6, 25);
            // 
            // setFormat
            // 
            this.setFormat.CheckOnClick = true;
            this.setFormat.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.setFormat.Image = ((System.Drawing.Image)(resources.GetObject("setFormat.Image")));
            this.setFormat.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.setFormat.Name = "setFormat";
            this.setFormat.Size = new System.Drawing.Size(23, 22);
            this.setFormat.Text = "{}";
            this.setFormat.ToolTipText = "Set constant buffer layout";
            this.setFormat.CheckedChanged += new System.EventHandler(this.setFormat_CheckedChanged);
            // 
            // split
            // 
            this.split.Dock = System.Windows.Forms.DockStyle.Fill;
            this.split.Location = new System.Drawing.Point(3, 28);
            this.split.Name = "split";
            this.split.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // split.Panel1
            // 
            this.split.Panel1.Controls.Add(this.variables);
            this.split.Panel1MinSize = 100;
            this.split.Panel2Collapsed = true;
            this.split.Panel2MinSize = 150;
            this.split.Size = new System.Drawing.Size(485, 299);
            this.split.SplitterDistance = 100;
            this.split.TabIndex = 5;
            // 
            // variables
            // 
            treeListColumn7.AutoSizeMinSize = 0;
            treeListColumn7.Width = 175;
            treeListColumn8.AutoSize = true;
            treeListColumn8.AutoSizeMinSize = 0;
            treeListColumn8.Width = 50;
            treeListColumn9.AutoSizeMinSize = 0;
            treeListColumn9.Width = 70;
            this.variables.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn7,
            treeListColumn8,
            treeListColumn9});
            this.variables.ColumnsOptions.LeftMargin = 0;
            this.variables.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.variables.Dock = System.Windows.Forms.DockStyle.Fill;
            this.variables.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.variables.Location = new System.Drawing.Point(0, 0);
            this.variables.Name = "variables";
            this.variables.RowOptions.ShowHeader = false;
            this.variables.Size = new System.Drawing.Size(485, 299);
            this.variables.TabIndex = 4;
            this.variables.Text = "treeListView1";
            this.variables.KeyDown += new System.Windows.Forms.KeyEventHandler(this.variables_KeyDown);
            // 
            // toolStripSeparator3
            // 
            this.toolStripSeparator3.Name = "toolStripSeparator3";
            this.toolStripSeparator3.Size = new System.Drawing.Size(6, 25);
            // 
            // saveCSV
            // 
            this.saveCSV.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.saveCSV.Image = global::renderdocui.Properties.Resources.save;
            this.saveCSV.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.saveCSV.Name = "saveCSV";
            this.saveCSV.Size = new System.Drawing.Size(23, 22);
            this.saveCSV.Text = "Save as CSV";
            this.saveCSV.Click += new System.EventHandler(this.saveCSV_Click);
            // 
            // exportDialog
            // 
            this.exportDialog.DefaultExt = "csv";
            this.exportDialog.Filter = "CSV Files (*.csv)|*.csv";
            this.exportDialog.Title = "Export buffer data as CSV";
            // 
            // ConstantBufferPreviewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(491, 330);
            this.Controls.Add(this.tableLayout);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "ConstantBufferPreviewer";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockRight;
            this.tableLayout.ResumeLayout(false);
            this.tableLayout.PerformLayout();
            this.toolStrip1.ResumeLayout(false);
            this.toolStrip1.PerformLayout();
            this.split.Panel1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.split)).EndInit();
            this.split.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.variables)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

		private System.Windows.Forms.TableLayoutPanel tableLayout;
		private System.Windows.Forms.ToolStrip toolStrip1;
		private System.Windows.Forms.ToolStripLabel slotLabel;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripLabel nameLabel;
		private System.Windows.Forms.ToolStripButton setFormat;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.SplitContainer split;
		private TreelistView.TreeListView variables;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
        private System.Windows.Forms.ToolStripButton saveCSV;
        private System.Windows.Forms.SaveFileDialog exportDialog;

    }
}
