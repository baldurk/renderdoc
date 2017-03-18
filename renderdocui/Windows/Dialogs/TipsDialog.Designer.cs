namespace renderdocui.Windows.Dialogs
{
    partial class TipsDialog
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
            System.Windows.Forms.Button close;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
            this.randomTip = new System.Windows.Forms.Button();
            this.nextTip = new System.Windows.Forms.Button();
            this.tipBox = new System.Windows.Forms.GroupBox();
            this.tipTitle = new System.Windows.Forms.Label();
            this.tipPicture = new System.Windows.Forms.PictureBox();
            this.tipLink = new System.Windows.Forms.LinkLabel();
            this.tipText = new System.Windows.Forms.Label();
            close = new System.Windows.Forms.Button();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            tableLayoutPanel1.SuspendLayout();
            this.tipBox.SuspendLayout();
            tableLayoutPanel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.tipPicture)).BeginInit();
            this.SuspendLayout();
            // 
            // close
            // 
            close.DialogResult = System.Windows.Forms.DialogResult.OK;
            close.Location = new System.Drawing.Point(472, 295);
            close.Name = "close";
            close.Size = new System.Drawing.Size(75, 23);
            close.TabIndex = 0;
            close.Text = "Close";
            close.UseVisualStyleBackColor = true;
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 3;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.Controls.Add(this.randomTip, 0, 1);
            tableLayoutPanel1.Controls.Add(close, 2, 1);
            tableLayoutPanel1.Controls.Add(this.nextTip, 1, 1);
            tableLayoutPanel1.Controls.Add(this.tipBox, 0, 0);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(550, 321);
            tableLayoutPanel1.TabIndex = 0;
            // 
            // randomTip
            // 
            this.randomTip.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.randomTip.Location = new System.Drawing.Point(311, 295);
            this.randomTip.Name = "randomTip";
            this.randomTip.Size = new System.Drawing.Size(74, 23);
            this.randomTip.TabIndex = 3;
            this.randomTip.Text = "Random Tip";
            this.randomTip.UseVisualStyleBackColor = true;
            this.randomTip.Click += new System.EventHandler(this.LoadRandomTip);
            // 
            // nextTip
            // 
            this.nextTip.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.nextTip.Location = new System.Drawing.Point(391, 295);
            this.nextTip.Name = "nextTip";
            this.nextTip.Size = new System.Drawing.Size(75, 23);
            this.nextTip.TabIndex = 1;
            this.nextTip.Text = "Next Tip";
            this.nextTip.UseVisualStyleBackColor = true;
            this.nextTip.Click += new System.EventHandler(this.LoadNextTip);
            // 
            // tipBox
            // 
            tableLayoutPanel1.SetColumnSpan(this.tipBox, 3);
            this.tipBox.Controls.Add(tableLayoutPanel2);
            this.tipBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tipBox.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.tipBox.Location = new System.Drawing.Point(3, 3);
            this.tipBox.Name = "tipBox";
            this.tipBox.Size = new System.Drawing.Size(544, 286);
            this.tipBox.TabIndex = 2;
            this.tipBox.TabStop = false;
            this.tipBox.Text = "Tip #X";
            // 
            // tableLayoutPanel2
            // 
            tableLayoutPanel2.ColumnCount = 1;
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.Controls.Add(this.tipTitle, 0, 0);
            tableLayoutPanel2.Controls.Add(this.tipPicture, 0, 2);
            tableLayoutPanel2.Controls.Add(this.tipLink, 0, 3);
            tableLayoutPanel2.Controls.Add(this.tipText, 0, 1);
            tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel2.Location = new System.Drawing.Point(3, 16);
            tableLayoutPanel2.Name = "tableLayoutPanel2";
            tableLayoutPanel2.RowCount = 4;
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            tableLayoutPanel2.Size = new System.Drawing.Size(538, 267);
            tableLayoutPanel2.TabIndex = 0;
            // 
            // tipTitle
            // 
            this.tipTitle.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tipTitle.AutoSize = true;
            this.tipTitle.Font = new System.Drawing.Font("Arial", 15.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.tipTitle.Location = new System.Drawing.Point(3, 0);
            this.tipTitle.Name = "tipTitle";
            this.tipTitle.Size = new System.Drawing.Size(532, 24);
            this.tipTitle.TabIndex = 0;
            this.tipTitle.Text = "Tip #X: Tip Title";
            this.tipTitle.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // tipPicture
            // 
            this.tipPicture.Anchor = System.Windows.Forms.AnchorStyles.Top;
            this.tipPicture.Location = new System.Drawing.Point(243, 180);
            this.tipPicture.Name = "tipPicture";
            this.tipPicture.Size = new System.Drawing.Size(52, 54);
            this.tipPicture.SizeMode = System.Windows.Forms.PictureBoxSizeMode.AutoSize;
            this.tipPicture.TabIndex = 1;
            this.tipPicture.TabStop = false;
            // 
            // tipLink
            // 
            this.tipLink.AutoSize = true;
            this.tipLink.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.tipLink.Location = new System.Drawing.Point(7, 244);
            this.tipLink.Margin = new System.Windows.Forms.Padding(7);
            this.tipLink.Name = "tipLink";
            this.tipLink.Size = new System.Drawing.Size(164, 16);
            this.tipLink.TabIndex = 3;
            this.tipLink.TabStop = true;
            this.tipLink.Text = "https://renderdoc.org/tips/1";
            this.tipLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.tipLink_LinkClicked);
            // 
            // tipText
            // 
            this.tipText.BackColor = System.Drawing.Color.Transparent;
            this.tipText.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tipText.Font = new System.Drawing.Font("Arial", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.tipText.Location = new System.Drawing.Point(3, 24);
            this.tipText.Name = "tipText";
            this.tipText.Padding = new System.Windows.Forms.Padding(10);
            this.tipText.Size = new System.Drawing.Size(532, 153);
            this.tipText.TabIndex = 4;
            this.tipText.Text = "Tip Text";
            // 
            // TipsDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(550, 321);
            this.Controls.Add(tableLayoutPanel1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.MinimumSize = new System.Drawing.Size(556, 346);
            this.Name = "TipsDialog";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "RenderDoc Tips";
            this.Load += new System.EventHandler(this.LoadRandomTip);
            tableLayoutPanel1.ResumeLayout(false);
            this.tipBox.ResumeLayout(false);
            tableLayoutPanel2.ResumeLayout(false);
            tableLayoutPanel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.tipPicture)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button nextTip;
        private System.Windows.Forms.GroupBox tipBox;
        private System.Windows.Forms.Label tipTitle;
        private System.Windows.Forms.PictureBox tipPicture;
        private System.Windows.Forms.LinkLabel tipLink;
        private System.Windows.Forms.Label tipText;
        private System.Windows.Forms.Button randomTip;
    }
}