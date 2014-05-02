namespace renderdocui.Windows.Dialogs
{
    partial class BufferFormatSpecifier
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
            System.Windows.Forms.GroupBox groupBox1;
            System.Windows.Forms.Label label1;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BufferFormatSpecifier));
            this.formatText = new System.Windows.Forms.TextBox();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.apply = new System.Windows.Forms.Button();
            this.errors = new System.Windows.Forms.Label();
            groupBox1 = new System.Windows.Forms.GroupBox();
            label1 = new System.Windows.Forms.Label();
            groupBox1.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBox1
            // 
            groupBox1.Controls.Add(this.formatText);
            groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox1.Location = new System.Drawing.Point(3, 117);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(650, 141);
            groupBox1.TabIndex = 0;
            groupBox1.TabStop = false;
            groupBox1.Text = "Format";
            // 
            // formatText
            // 
            this.formatText.Dock = System.Windows.Forms.DockStyle.Fill;
            this.formatText.Font = new System.Drawing.Font("Consolas", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.formatText.Location = new System.Drawing.Point(3, 16);
            this.formatText.Multiline = true;
            this.formatText.Name = "formatText";
            this.formatText.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.formatText.Size = new System.Drawing.Size(644, 122);
            this.formatText.TabIndex = 0;
            this.formatText.Text = "float4 asd; // blah blah\r\nfloat3 bar;";
            this.formatText.KeyDown += new System.Windows.Forms.KeyEventHandler(this.formatText_KeyDown);
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(8, 8);
            label1.Margin = new System.Windows.Forms.Padding(8);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(549, 52);
            label1.TabIndex = 1;
            label1.Text = resources.GetString("label1.Text");
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(groupBox1, 0, 2);
            this.tableLayoutPanel1.Controls.Add(label1, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.apply, 0, 3);
            this.tableLayoutPanel1.Controls.Add(this.errors, 0, 1);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 4;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.Size = new System.Drawing.Size(656, 300);
            this.tableLayoutPanel1.TabIndex = 0;
            // 
            // apply
            // 
            this.apply.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.apply.Location = new System.Drawing.Point(585, 269);
            this.apply.Margin = new System.Windows.Forms.Padding(8);
            this.apply.Name = "apply";
            this.apply.Size = new System.Drawing.Size(63, 23);
            this.apply.TabIndex = 2;
            this.apply.Text = "Apply";
            this.apply.UseVisualStyleBackColor = true;
            this.apply.Click += new System.EventHandler(this.apply_Click);
            // 
            // errors
            // 
            this.errors.Dock = System.Windows.Forms.DockStyle.Fill;
            this.errors.Font = new System.Drawing.Font("Microsoft Sans Serif", 14.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.errors.ForeColor = System.Drawing.Color.DarkRed;
            this.errors.Location = new System.Drawing.Point(3, 68);
            this.errors.Name = "errors";
            this.errors.Size = new System.Drawing.Size(650, 46);
            this.errors.TabIndex = 3;
            // 
            // BufferFormatSpecifier
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(656, 300);
            this.Controls.Add(this.tableLayoutPanel1);
            this.DockAreas = ((WeifenLuo.WinFormsUI.Docking.DockAreas)(((((WeifenLuo.WinFormsUI.Docking.DockAreas.DockLeft | WeifenLuo.WinFormsUI.Docking.DockAreas.DockRight) 
            | WeifenLuo.WinFormsUI.Docking.DockAreas.DockTop) 
            | WeifenLuo.WinFormsUI.Docking.DockAreas.DockBottom) 
            | WeifenLuo.WinFormsUI.Docking.DockAreas.Document)));
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "BufferFormatSpecifier";
            this.ShowInTaskbar = false;
            this.Text = "Buffer Format";
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.TextBox formatText;
        private System.Windows.Forms.Button apply;
        private System.Windows.Forms.Label errors;
    }
}