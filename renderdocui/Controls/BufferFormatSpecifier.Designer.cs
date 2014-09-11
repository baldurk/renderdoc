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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BufferFormatSpecifier));
			this.formatText = new System.Windows.Forms.TextBox();
			this.helpText = new System.Windows.Forms.Label();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.errors = new System.Windows.Forms.Label();
			this.apply = new System.Windows.Forms.Button();
			this.hideHelp = new System.Windows.Forms.Button();
			groupBox1 = new System.Windows.Forms.GroupBox();
			groupBox1.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// groupBox1
			// 
			groupBox1.Controls.Add(this.formatText);
			groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
			groupBox1.Location = new System.Drawing.Point(3, 195);
			groupBox1.Name = "groupBox1";
			groupBox1.Size = new System.Drawing.Size(556, 227);
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
			this.formatText.Size = new System.Drawing.Size(550, 208);
			this.formatText.TabIndex = 0;
			this.formatText.Text = "float4 asd; // blah blah\r\nfloat3 bar;";
			this.formatText.KeyDown += new System.Windows.Forms.KeyEventHandler(this.formatText_KeyDown);
			// 
			// helpText
			// 
			this.helpText.AutoSize = true;
			this.helpText.Location = new System.Drawing.Point(8, 8);
			this.helpText.Margin = new System.Windows.Forms.Padding(8);
			this.helpText.Name = "helpText";
			this.helpText.Size = new System.Drawing.Size(517, 130);
			this.helpText.TabIndex = 1;
			this.helpText.Text = resources.GetString("helpText.Text");
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.ColumnCount = 2;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.Controls.Add(groupBox1, 0, 2);
			this.tableLayoutPanel1.Controls.Add(this.helpText, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.errors, 0, 1);
			this.tableLayoutPanel1.Controls.Add(this.apply, 1, 2);
			this.tableLayoutPanel1.Controls.Add(this.hideHelp, 1, 0);
			this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 3;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(643, 425);
			this.tableLayoutPanel1.TabIndex = 0;
			// 
			// errors
			// 
			this.tableLayoutPanel1.SetColumnSpan(this.errors, 2);
			this.errors.Dock = System.Windows.Forms.DockStyle.Fill;
			this.errors.Font = new System.Drawing.Font("Microsoft Sans Serif", 14.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.errors.ForeColor = System.Drawing.Color.DarkRed;
			this.errors.Location = new System.Drawing.Point(3, 146);
			this.errors.Name = "errors";
			this.errors.Size = new System.Drawing.Size(637, 46);
			this.errors.TabIndex = 3;
			// 
			// apply
			// 
			this.apply.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.apply.Location = new System.Drawing.Point(572, 394);
			this.apply.Margin = new System.Windows.Forms.Padding(8);
			this.apply.Name = "apply";
			this.apply.Size = new System.Drawing.Size(63, 23);
			this.apply.TabIndex = 1;
			this.apply.Text = "Apply";
			this.apply.UseVisualStyleBackColor = true;
			this.apply.Click += new System.EventHandler(this.apply_Click);
			// 
			// hideHelp
			// 
			this.hideHelp.Location = new System.Drawing.Point(565, 3);
			this.hideHelp.Name = "hideHelp";
			this.hideHelp.Size = new System.Drawing.Size(75, 23);
			this.hideHelp.TabIndex = 4;
			this.hideHelp.Text = "Hide Help";
			this.hideHelp.UseVisualStyleBackColor = true;
			this.hideHelp.Click += new System.EventHandler(this.hideHelp_Click);
			// 
			// BufferFormatSpecifier
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.tableLayoutPanel1);
			this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Name = "BufferFormatSpecifier";
			this.Size = new System.Drawing.Size(643, 425);
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
		private System.Windows.Forms.Button hideHelp;
		private System.Windows.Forms.Label helpText;
    }
}