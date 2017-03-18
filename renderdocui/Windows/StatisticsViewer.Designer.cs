namespace renderdocui.Windows
{
    partial class StatisticsViewer
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
            this.statisticsLog = new System.Windows.Forms.RichTextBox();
            this.SuspendLayout();
            // 
            // statisticsLog
            // 
            this.statisticsLog.BackColor = System.Drawing.SystemColors.ControlLightLight;
            this.statisticsLog.Dock = System.Windows.Forms.DockStyle.Fill;
            this.statisticsLog.Location = new System.Drawing.Point(0, 0);
            this.statisticsLog.Name = "statisticsLog";
            this.statisticsLog.ReadOnly = true;
            this.statisticsLog.Size = new System.Drawing.Size(812, 482);
            this.statisticsLog.TabIndex = 0;
            this.statisticsLog.Text = "";
            // 
            // StatisticsViewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(812, 482);
            this.Controls.Add(this.statisticsLog);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "StatisticsViewer";
            this.Text = "Statistics";
            this.Load += new System.EventHandler(this.StatisticsViewer_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.RichTextBox statisticsLog;
    }
}