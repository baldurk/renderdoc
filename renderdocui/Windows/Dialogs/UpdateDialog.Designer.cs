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
            this.updateNotes = new System.Windows.Forms.TextBox();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.doupdate = new System.Windows.Forms.Button();
            this.progressText = new System.Windows.Forms.Label();
            this.close = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // updateVer
            // 
            this.updateVer.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.updateVer.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.updateVer.Location = new System.Drawing.Point(10, 9);
            this.updateVer.Name = "updateVer";
            this.updateVer.Size = new System.Drawing.Size(376, 25);
            this.updateVer.TabIndex = 1;
            this.updateVer.Text = "Update Available - v0.00-betahash";
            this.updateVer.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // updateNotes
            // 
            this.updateNotes.BackColor = System.Drawing.SystemColors.Window;
            this.updateNotes.Location = new System.Drawing.Point(15, 37);
            this.updateNotes.Multiline = true;
            this.updateNotes.Name = "updateNotes";
            this.updateNotes.ReadOnly = true;
            this.updateNotes.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.updateNotes.Size = new System.Drawing.Size(363, 194);
            this.updateNotes.TabIndex = 2;
            // 
            // progressBar
            // 
            this.progressBar.Location = new System.Drawing.Point(131, 244);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(249, 23);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Continuous;
            this.progressBar.TabIndex = 3;
            // 
            // doupdate
            // 
            this.doupdate.Location = new System.Drawing.Point(212, 273);
            this.doupdate.Name = "doupdate";
            this.doupdate.Size = new System.Drawing.Size(168, 23);
            this.doupdate.TabIndex = 4;
            this.doupdate.Text = "Download, restart and Install";
            this.doupdate.UseVisualStyleBackColor = true;
            this.doupdate.Click += new System.EventHandler(this.doupdate_Click);
            // 
            // progressText
            // 
            this.progressText.AutoSize = true;
            this.progressText.Location = new System.Drawing.Point(15, 249);
            this.progressText.Name = "progressText";
            this.progressText.Size = new System.Drawing.Size(78, 13);
            this.progressText.TabIndex = 5;
            this.progressText.Text = "Downloading...";
            // 
            // close
            // 
            this.close.Location = new System.Drawing.Point(131, 273);
            this.close.Name = "close";
            this.close.Size = new System.Drawing.Size(75, 23);
            this.close.TabIndex = 6;
            this.close.Text = "Close";
            this.close.UseVisualStyleBackColor = true;
            this.close.Click += new System.EventHandler(this.close_Click);
            // 
            // UpdateDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(392, 308);
            this.Controls.Add(this.close);
            this.Controls.Add(this.progressText);
            this.Controls.Add(this.doupdate);
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.updateNotes);
            this.Controls.Add(this.updateVer);
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
        private System.Windows.Forms.TextBox updateNotes;
        private System.Windows.Forms.ProgressBar progressBar;
        private System.Windows.Forms.Button doupdate;
        private System.Windows.Forms.Label progressText;
        private System.Windows.Forms.Button close;
    }
}