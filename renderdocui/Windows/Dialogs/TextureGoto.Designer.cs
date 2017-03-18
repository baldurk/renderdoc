namespace renderdocui.Windows.Dialogs
{
    partial class TextureGoto
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
            System.Windows.Forms.Label label1;
            this.chooseX = new System.Windows.Forms.NumericUpDown();
            this.chooseY = new System.Windows.Forms.NumericUpDown();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            label1 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.chooseX)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.chooseY)).BeginInit();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.AutoSize = true;
            tableLayoutPanel1.ColumnCount = 2;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            tableLayoutPanel1.Controls.Add(this.chooseX, 0, 1);
            tableLayoutPanel1.Controls.Add(this.chooseY, 1, 1);
            tableLayoutPanel1.Controls.Add(label1, 0, 0);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(4, 4);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(137, 39);
            tableLayoutPanel1.TabIndex = 0;
            // 
            // label1
            // 
            label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label1.AutoSize = true;
            tableLayoutPanel1.SetColumnSpan(label1, 2);
            label1.Location = new System.Drawing.Point(3, 0);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(131, 13);
            label1.TabIndex = 0;
            label1.Text = "Goto Location";
            label1.TextAlign = System.Drawing.ContentAlignment.BottomCenter;
            // 
            // chooseX
            // 
            this.chooseX.Location = new System.Drawing.Point(3, 16);
            this.chooseX.Maximum = new decimal(new int[] {
            16384,
            0,
            0,
            0});
            this.chooseX.Name = "chooseX";
            this.chooseX.Size = new System.Drawing.Size(62, 20);
            this.chooseX.TabIndex = 1;
            this.chooseX.Value = new decimal(new int[] {
            16384,
            0,
            0,
            0});
            this.chooseX.Enter += new System.EventHandler(this.location_Enter);
            this.chooseX.KeyDown += new System.Windows.Forms.KeyEventHandler(this.location_KeyDown);
            // 
            // chooseY
            // 
            this.chooseY.Location = new System.Drawing.Point(71, 16);
            this.chooseY.Maximum = new decimal(new int[] {
            16384,
            0,
            0,
            0});
            this.chooseY.Name = "chooseY";
            this.chooseY.Size = new System.Drawing.Size(63, 20);
            this.chooseY.TabIndex = 2;
            this.chooseY.Value = new decimal(new int[] {
            16384,
            0,
            0,
            0});
            this.chooseY.Enter += new System.EventHandler(this.location_Enter);
            this.chooseY.KeyDown += new System.Windows.Forms.KeyEventHandler(this.location_KeyDown);
            // 
            // TextureGoto
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.ClientSize = new System.Drawing.Size(145, 47);
            this.ControlBox = false;
            this.Controls.Add(tableLayoutPanel1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "TextureGoto";
            this.Padding = new System.Windows.Forms.Padding(4);
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
            this.TopMost = true;
            this.Deactivate += new System.EventHandler(this.TextureGoto_Deactivate);
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.chooseX)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.chooseY)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.NumericUpDown chooseX;
        private System.Windows.Forms.NumericUpDown chooseY;
    }
}