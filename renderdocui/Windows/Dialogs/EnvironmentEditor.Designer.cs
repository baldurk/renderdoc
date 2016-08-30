namespace renderdocui.Windows.Dialogs
{
    partial class EnvironmentEditor
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
            TreelistView.TreeListColumn treeListColumn1 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Name", "Name")));
            TreelistView.TreeListColumn treeListColumn2 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Modification", "Modification")));
            TreelistView.TreeListColumn treeListColumn3 = ((TreelistView.TreeListColumn)(new TreelistView.TreeListColumn("Value", "Value")));
            System.Windows.Forms.GroupBox groupBox2;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.GroupBox groupBox1;
            System.Windows.Forms.Button ok;
            System.Windows.Forms.Button cancel;
            this.variables = new TreelistView.TreeListView();
            this.varValue = new System.Windows.Forms.TextBox();
            this.varName = new System.Windows.Forms.TextBox();
            this.pendSeparator = new System.Windows.Forms.ComboBox();
            this.prependType = new System.Windows.Forms.RadioButton();
            this.appendType = new System.Windows.Forms.RadioButton();
            this.setType = new System.Windows.Forms.RadioButton();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this.addUpdate = new System.Windows.Forms.Button();
            this.delete = new System.Windows.Forms.Button();
            this.flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            groupBox2 = new System.Windows.Forms.GroupBox();
            label2 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            groupBox1 = new System.Windows.Forms.GroupBox();
            ok = new System.Windows.Forms.Button();
            cancel = new System.Windows.Forms.Button();
            tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.variables)).BeginInit();
            groupBox2.SuspendLayout();
            groupBox1.SuspendLayout();
            this.flowLayoutPanel1.SuspendLayout();
            this.flowLayoutPanel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 2;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel1.Controls.Add(this.variables, 0, 0);
            tableLayoutPanel1.Controls.Add(groupBox2, 0, 1);
            tableLayoutPanel1.Controls.Add(groupBox1, 1, 1);
            tableLayoutPanel1.Controls.Add(this.flowLayoutPanel1, 1, 2);
            tableLayoutPanel1.Controls.Add(this.flowLayoutPanel2, 1, 3);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 4;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(673, 474);
            tableLayoutPanel1.TabIndex = 2;
            // 
            // variables
            // 
            treeListColumn1.AutoSizeMinSize = 0;
            treeListColumn1.Width = 160;
            treeListColumn2.AutoSizeMinSize = 0;
            treeListColumn2.Width = 170;
            treeListColumn3.AutoSize = true;
            treeListColumn3.AutoSizeMinSize = 100;
            treeListColumn3.Width = 50;
            this.variables.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1,
            treeListColumn2,
            treeListColumn3});
            this.variables.ColumnsOptions.LeftMargin = 0;
            tableLayoutPanel1.SetColumnSpan(this.variables, 2);
            this.variables.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.variables.Dock = System.Windows.Forms.DockStyle.Fill;
            this.variables.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.variables.Location = new System.Drawing.Point(3, 3);
            this.variables.Name = "variables";
            this.variables.RowOptions.ShowHeader = false;
            this.variables.Size = new System.Drawing.Size(667, 257);
            this.variables.TabIndex = 11;
            this.variables.Text = "treeListView1";
            this.variables.ViewOptions.HoverHandTreeColumn = false;
            this.variables.ViewOptions.PadForPlusMinus = false;
            this.variables.ViewOptions.ShowLine = false;
            this.variables.ViewOptions.ShowPlusMinus = false;
            this.variables.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.variables_AfterSelect);
            this.variables.KeyDown += new System.Windows.Forms.KeyEventHandler(this.variables_KeyDown);
            // 
            // groupBox2
            // 
            groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox2.Controls.Add(label2);
            groupBox2.Controls.Add(this.varValue);
            groupBox2.Controls.Add(label1);
            groupBox2.Controls.Add(this.varName);
            groupBox2.Location = new System.Drawing.Point(3, 266);
            groupBox2.Name = "groupBox2";
            groupBox2.Size = new System.Drawing.Size(489, 122);
            groupBox2.TabIndex = 4;
            groupBox2.TabStop = false;
            groupBox2.Text = "Environment Variable";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(9, 68);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(34, 13);
            label2.TabIndex = 3;
            label2.Text = "Value";
            // 
            // varValue
            // 
            this.varValue.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.varValue.Location = new System.Drawing.Point(9, 84);
            this.varValue.Name = "varValue";
            this.varValue.Size = new System.Drawing.Size(473, 20);
            this.varValue.TabIndex = 2;
            this.varValue.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.varField_KeyPress);
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(9, 16);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(35, 13);
            label1.TabIndex = 1;
            label1.Text = "Name";
            // 
            // varName
            // 
            this.varName.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.varName.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.Suggest;
            this.varName.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.CustomSource;
            this.varName.Location = new System.Drawing.Point(9, 32);
            this.varName.Name = "varName";
            this.varName.Size = new System.Drawing.Size(473, 20);
            this.varName.TabIndex = 1;
            this.varName.TextChanged += new System.EventHandler(this.varName_TextChanged);
            this.varName.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.varField_KeyPress);
            // 
            // groupBox1
            // 
            groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox1.Controls.Add(this.pendSeparator);
            groupBox1.Controls.Add(this.prependType);
            groupBox1.Controls.Add(this.appendType);
            groupBox1.Controls.Add(this.setType);
            groupBox1.Location = new System.Drawing.Point(498, 266);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(172, 122);
            groupBox1.TabIndex = 2;
            groupBox1.TabStop = false;
            groupBox1.Text = "Modification type";
            // 
            // pendSeparator
            // 
            this.pendSeparator.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.pendSeparator.FormattingEnabled = true;
            this.pendSeparator.Location = new System.Drawing.Point(7, 92);
            this.pendSeparator.Name = "pendSeparator";
            this.pendSeparator.Size = new System.Drawing.Size(121, 21);
            this.pendSeparator.TabIndex = 6;
            // 
            // prependType
            // 
            this.prependType.AutoSize = true;
            this.prependType.Location = new System.Drawing.Point(7, 68);
            this.prependType.Name = "prependType";
            this.prependType.Size = new System.Drawing.Size(95, 17);
            this.prependType.TabIndex = 5;
            this.prependType.TabStop = true;
            this.prependType.Text = "Prepend Value";
            this.prependType.UseVisualStyleBackColor = true;
            this.prependType.CheckedChanged += new System.EventHandler(this.modification_CheckedChanged);
            // 
            // appendType
            // 
            this.appendType.AutoSize = true;
            this.appendType.Location = new System.Drawing.Point(7, 44);
            this.appendType.Name = "appendType";
            this.appendType.Size = new System.Drawing.Size(92, 17);
            this.appendType.TabIndex = 4;
            this.appendType.TabStop = true;
            this.appendType.Text = "Append Value";
            this.appendType.UseVisualStyleBackColor = true;
            this.appendType.CheckedChanged += new System.EventHandler(this.modification_CheckedChanged);
            // 
            // setType
            // 
            this.setType.AutoSize = true;
            this.setType.Location = new System.Drawing.Point(7, 20);
            this.setType.Name = "setType";
            this.setType.Size = new System.Drawing.Size(71, 17);
            this.setType.TabIndex = 3;
            this.setType.TabStop = true;
            this.setType.Text = "Set Value";
            this.setType.UseVisualStyleBackColor = true;
            this.setType.CheckedChanged += new System.EventHandler(this.modification_CheckedChanged);
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.flowLayoutPanel1.Controls.Add(this.addUpdate);
            this.flowLayoutPanel1.Controls.Add(this.delete);
            this.flowLayoutPanel1.Location = new System.Drawing.Point(498, 391);
            this.flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0, 0, 3, 0);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(172, 28);
            this.flowLayoutPanel1.TabIndex = 9;
            // 
            // addUpdate
            // 
            this.addUpdate.Location = new System.Drawing.Point(3, 3);
            this.addUpdate.Name = "addUpdate";
            this.addUpdate.Size = new System.Drawing.Size(80, 23);
            this.addUpdate.TabIndex = 7;
            this.addUpdate.Text = "Add / Update";
            this.addUpdate.UseVisualStyleBackColor = true;
            this.addUpdate.Click += new System.EventHandler(this.addUpdate_Click);
            // 
            // delete
            // 
            this.delete.Location = new System.Drawing.Point(89, 3);
            this.delete.Name = "delete";
            this.delete.Size = new System.Drawing.Size(80, 23);
            this.delete.TabIndex = 8;
            this.delete.Text = "Delete";
            this.delete.UseVisualStyleBackColor = true;
            this.delete.Click += new System.EventHandler(this.delete_Click);
            // 
            // flowLayoutPanel2
            // 
            this.flowLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.flowLayoutPanel2.Controls.Add(ok);
            this.flowLayoutPanel2.Controls.Add(cancel);
            this.flowLayoutPanel2.Location = new System.Drawing.Point(498, 445);
            this.flowLayoutPanel2.Margin = new System.Windows.Forms.Padding(3, 26, 3, 3);
            this.flowLayoutPanel2.Name = "flowLayoutPanel2";
            this.flowLayoutPanel2.Size = new System.Drawing.Size(172, 26);
            this.flowLayoutPanel2.TabIndex = 10;
            // 
            // ok
            // 
            ok.DialogResult = System.Windows.Forms.DialogResult.OK;
            ok.Location = new System.Drawing.Point(3, 3);
            ok.Name = "ok";
            ok.Size = new System.Drawing.Size(80, 23);
            ok.TabIndex = 9;
            ok.Text = "OK";
            ok.UseVisualStyleBackColor = true;
            // 
            // cancel
            // 
            cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            cancel.Location = new System.Drawing.Point(89, 3);
            cancel.Name = "cancel";
            cancel.Size = new System.Drawing.Size(80, 23);
            cancel.TabIndex = 10;
            cancel.Text = "Cancel";
            cancel.UseVisualStyleBackColor = true;
            // 
            // EnvironmentEditor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = cancel;
            this.ClientSize = new System.Drawing.Size(673, 474);
            this.Controls.Add(tableLayoutPanel1);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "EnvironmentEditor";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Capture Environment Editor";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.EnvironmentEditor_FormClosing);
            this.Load += new System.EventHandler(this.EnvironmentEditor_Load);
            tableLayoutPanel1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.variables)).EndInit();
            groupBox2.ResumeLayout(false);
            groupBox2.PerformLayout();
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            this.flowLayoutPanel1.ResumeLayout(false);
            this.flowLayoutPanel2.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TextBox varValue;
        private System.Windows.Forms.TextBox varName;
        private System.Windows.Forms.Button addUpdate;
        private System.Windows.Forms.ComboBox pendSeparator;
        private System.Windows.Forms.RadioButton prependType;
        private System.Windows.Forms.RadioButton appendType;
        private System.Windows.Forms.RadioButton setType;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
        private System.Windows.Forms.Button delete;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
        private TreelistView.TreeListView variables;
    }
}