namespace renderdocui.Windows.Dialogs
{
    partial class UpdateDialog
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
            this.updateVer = new System.Windows.Forms.Label();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.doupdate = new System.Windows.Forms.Button();
            this.progressText = new System.Windows.Forms.Label();
            this.close = new System.Windows.Forms.Button();
            this.updateNotes = new System.Windows.Forms.RichTextBox();
            this.updateMetadata = new System.Windows.Forms.Label();
            this.metaDataLabel = new System.Windows.Forms.Label();
            this.rlsNotes = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // updateVer
            // 
            this.updateVer.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.updateVer.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.updateVer.Location = new System.Drawing.Point(10, 9);
            this.updateVer.Name = "updateVer";
            this.updateVer.Size = new System.Drawing.Size(378, 25);
            this.updateVer.TabIndex = 1;
            this.updateVer.Text = "Update Available - v0.00-betahash";
            this.updateVer.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // progressBar
            // 
            this.progressBar.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.progressBar.Location = new System.Drawing.Point(131, 289);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(249, 23);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Continuous;
            this.progressBar.TabIndex = 3;
            // 
            // doupdate
            // 
            this.doupdate.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.doupdate.Location = new System.Drawing.Point(212, 318);
            this.doupdate.Name = "doupdate";
            this.doupdate.Size = new System.Drawing.Size(168, 23);
            this.doupdate.TabIndex = 4;
            this.doupdate.Text = "Download, restart and Install";
            this.doupdate.UseVisualStyleBackColor = true;
            this.doupdate.Click += new System.EventHandler(this.doupdate_Click);
            // 
            // progressText
            // 
            this.progressText.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.progressText.AutoSize = true;
            this.progressText.Location = new System.Drawing.Point(15, 294);
            this.progressText.Name = "progressText";
            this.progressText.Size = new System.Drawing.Size(78, 13);
            this.progressText.TabIndex = 5;
            this.progressText.Text = "Downloading...";
            // 
            // close
            // 
            this.close.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.close.Location = new System.Drawing.Point(131, 318);
            this.close.Name = "close";
            this.close.Size = new System.Drawing.Size(75, 23);
            this.close.TabIndex = 6;
            this.close.Text = "Close";
            this.close.UseVisualStyleBackColor = true;
            this.close.Click += new System.EventHandler(this.close_Click);
            // 
            // updateNotes
            // 
            this.updateNotes.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.updateNotes.BackColor = System.Drawing.SystemColors.Window;
            this.updateNotes.Location = new System.Drawing.Point(18, 37);
            this.updateNotes.Name = "updateNotes";
            this.updateNotes.ReadOnly = true;
            this.updateNotes.ShortcutsEnabled = false;
            this.updateNotes.Size = new System.Drawing.Size(362, 201);
            this.updateNotes.TabIndex = 7;
            this.updateNotes.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler(this.updateNotes_LinkClicked);
            // 
            // updateMetadata
            // 
            this.updateMetadata.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.updateMetadata.AutoSize = true;
            this.updateMetadata.Location = new System.Drawing.Point(314, 247);
            this.updateMetadata.Name = "updateMetadata";
            this.updateMetadata.Size = new System.Drawing.Size(54, 65);
            this.updateMetadata.TabIndex = 8;
            this.updateMetadata.Text = "v0.xx\r\n\r\nv0.yy\r\n\r\nAA.BBMB";
            // 
            // metaDataLabel
            // 
            this.metaDataLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.metaDataLabel.AutoSize = true;
            this.metaDataLabel.Location = new System.Drawing.Point(226, 247);
            this.metaDataLabel.Name = "metaDataLabel";
            this.metaDataLabel.Size = new System.Drawing.Size(82, 65);
            this.metaDataLabel.TabIndex = 9;
            this.metaDataLabel.Text = "Current Version:\r\n\r\nNew Version:\r\n\r\nDownload Size:";
            this.metaDataLabel.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // rlsNotes
            // 
            this.rlsNotes.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.rlsNotes.Location = new System.Drawing.Point(15, 318);
            this.rlsNotes.Name = "rlsNotes";
            this.rlsNotes.Size = new System.Drawing.Size(110, 23);
            this.rlsNotes.TabIndex = 10;
            this.rlsNotes.Text = "Full Release Notes";
            this.rlsNotes.UseVisualStyleBackColor = true;
            this.rlsNotes.Click += new System.EventHandler(this.rlsNotes_Click);
            // 
            // UpdateDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(394, 354);
            this.Controls.Add(this.rlsNotes);
            this.Controls.Add(this.metaDataLabel);
            this.Controls.Add(this.updateMetadata);
            this.Controls.Add(this.updateNotes);
            this.Controls.Add(this.close);
            this.Controls.Add(this.progressText);
            this.Controls.Add(this.doupdate);
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.updateVer);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "UpdateDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Update Available";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label updateVer;
        private System.Windows.Forms.ProgressBar progressBar;
        private System.Windows.Forms.Button doupdate;
        private System.Windows.Forms.Label progressText;
        private System.Windows.Forms.Button close;
        private System.Windows.Forms.RichTextBox updateNotes;
        private System.Windows.Forms.Label updateMetadata;
        private System.Windows.Forms.Label metaDataLabel;
        private System.Windows.Forms.Button rlsNotes;
    }
}