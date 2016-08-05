namespace renderdocui.Controls
{
    partial class ResourcePreview
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.slotLabel = new System.Windows.Forms.Label();
            this.descriptionLabel = new System.Windows.Forms.Label();
            this.thumbnail = new renderdocui.Controls.NoScrollPanel();
            this.tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.BackColor = System.Drawing.Color.Transparent;
            this.tableLayoutPanel1.ColumnCount = 2;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.slotLabel, 0, 1);
            this.tableLayoutPanel1.Controls.Add(this.descriptionLabel, 1, 1);
            this.tableLayoutPanel1.Controls.Add(this.thumbnail, 0, 0);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(3, 3);
            this.tableLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.Size = new System.Drawing.Size(110, 86);
            this.tableLayoutPanel1.TabIndex = 0;
            // 
            // slotLabel
            // 
            this.slotLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.slotLabel.AutoSize = true;
            this.slotLabel.BackColor = System.Drawing.SystemColors.ButtonShadow;
            this.slotLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.slotLabel.ForeColor = System.Drawing.SystemColors.ControlText;
            this.slotLabel.Location = new System.Drawing.Point(0, 60);
            this.slotLabel.Margin = new System.Windows.Forms.Padding(0);
            this.slotLabel.Name = "slotLabel";
            this.slotLabel.Size = new System.Drawing.Size(19, 26);
            this.slotLabel.TabIndex = 1;
            this.slotLabel.Text = "1";
            this.slotLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.slotLabel.MouseClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseClick);
            this.slotLabel.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseDoubleClick);
            // 
            // descriptionLabel
            // 
            this.descriptionLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.descriptionLabel.AutoEllipsis = true;
            this.descriptionLabel.BackColor = System.Drawing.SystemColors.ButtonShadow;
            this.descriptionLabel.ForeColor = System.Drawing.SystemColors.ControlText;
            this.descriptionLabel.Location = new System.Drawing.Point(19, 60);
            this.descriptionLabel.Margin = new System.Windows.Forms.Padding(0);
            this.descriptionLabel.Name = "descriptionLabel";
            this.descriptionLabel.Size = new System.Drawing.Size(91, 26);
            this.descriptionLabel.TabIndex = 2;
            this.descriptionLabel.Text = "Texture2D 117";
            this.descriptionLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.descriptionLabel.MouseClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseClick);
            this.descriptionLabel.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseDoubleClick);
            // 
            // thumbnail
            // 
            this.thumbnail.BackColor = System.Drawing.Color.Chartreuse;
            this.tableLayoutPanel1.SetColumnSpan(this.thumbnail, 2);
            this.thumbnail.Dock = System.Windows.Forms.DockStyle.Fill;
            this.thumbnail.Location = new System.Drawing.Point(0, 0);
            this.thumbnail.Margin = new System.Windows.Forms.Padding(0);
            this.thumbnail.Name = "thumbnail";
            this.thumbnail.Size = new System.Drawing.Size(110, 60);
            this.thumbnail.TabIndex = 0;
            this.thumbnail.Paint += new System.Windows.Forms.PaintEventHandler(this.thumbnail_Paint);
            this.thumbnail.MouseClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseClick);
            this.thumbnail.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.child_MouseDoubleClick);
            // 
            // ResourcePreview
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Black;
            this.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.Controls.Add(this.tableLayoutPanel1);
            this.Margin = new System.Windows.Forms.Padding(5, 0, 0, 0);
            this.Name = "ResourcePreview";
            this.Padding = new System.Windows.Forms.Padding(3);
            this.Size = new System.Drawing.Size(116, 92);
            this.Load += new System.EventHandler(this.ResourcePreview_Load);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private Controls.NoScrollPanel thumbnail;
        private System.Windows.Forms.Label slotLabel;
        private System.Windows.Forms.Label descriptionLabel;
    }
}
