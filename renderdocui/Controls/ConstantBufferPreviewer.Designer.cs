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
            TreelistView.TreeListColumn treeListColumn1 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("VarName", "Name")));
            TreelistView.TreeListColumn treeListColumn2 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("VarValue", "Value")));
            TreelistView.TreeListColumn treeListColumn3 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("VarType", "Type")));
            this.slotLabel = new System.Windows.Forms.Label();
            this.nameLabel = new System.Windows.Forms.Label();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.variables = new TreelistView.TreeListView();
            this.tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.variables)).BeginInit();
            this.SuspendLayout();
            // 
            // slotLabel
            // 
            this.slotLabel.AutoSize = true;
            this.slotLabel.Location = new System.Drawing.Point(3, 0);
            this.slotLabel.Name = "slotLabel";
            this.slotLabel.Padding = new System.Windows.Forms.Padding(5);
            this.slotLabel.Size = new System.Drawing.Size(10, 23);
            this.slotLabel.TabIndex = 0;
            // 
            // nameLabel
            // 
            this.nameLabel.AutoSize = true;
            this.nameLabel.Location = new System.Drawing.Point(19, 0);
            this.nameLabel.Name = "nameLabel";
            this.nameLabel.Padding = new System.Windows.Forms.Padding(5);
            this.nameLabel.Size = new System.Drawing.Size(10, 23);
            this.nameLabel.TabIndex = 1;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 2;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.slotLabel, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.nameLabel, 1, 0);
            this.tableLayoutPanel1.Controls.Add(this.variables, 0, 1);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(499, 357);
            this.tableLayoutPanel1.TabIndex = 0;
            // 
            // variables
            // 
            treeListColumn1.AutoSizeMinSize = 0;
            treeListColumn1.Width = 175;
            treeListColumn2.AutoSize = true;
            treeListColumn2.AutoSizeMinSize = 0;
            treeListColumn2.Width = 50;
            treeListColumn3.AutoSizeMinSize = 0;
            treeListColumn3.Width = 50;
            this.variables.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3});
            this.variables.ColumnsOptions.LeftMargin = 0;
            this.tableLayoutPanel1.SetColumnSpan(this.variables, 2);
            this.variables.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.variables.Dock = System.Windows.Forms.DockStyle.Fill;
            this.variables.Location = new System.Drawing.Point(3, 26);
            this.variables.Name = "variables";
            this.variables.RowOptions.ShowHeader = false;
            this.variables.Size = new System.Drawing.Size(493, 328);
            this.variables.TabIndex = 2;
            this.variables.Text = "treeListView1";
            this.variables.KeyDown += new System.Windows.Forms.KeyEventHandler(this.variables_KeyDown);
            // 
            // ConstantBufferPreviewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.tableLayoutPanel1);
            this.Name = "ConstantBufferPreviewer";
            this.Size = new System.Drawing.Size(499, 357);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.variables)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private TreelistView.TreeListView variables;
        private System.Windows.Forms.Label slotLabel;
        private System.Windows.Forms.Label nameLabel;

    }
}
