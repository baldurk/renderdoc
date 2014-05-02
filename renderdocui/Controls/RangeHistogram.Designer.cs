namespace renderdocui.Controls
{
    partial class RangeHistogram
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
            this.components = new System.ComponentModel.Container();
            this.whiteToolTip = new System.Windows.Forms.ToolTip(this.components);
            this.blackToolTip = new System.Windows.Forms.ToolTip(this.components);
            this.SuspendLayout();
            // 
            // whiteToolTip
            // 
            this.whiteToolTip.AutomaticDelay = 0;
            this.whiteToolTip.UseAnimation = false;
            this.whiteToolTip.UseFading = false;
            // 
            // blackToolTip
            // 
            this.blackToolTip.AutomaticDelay = 0;
            this.blackToolTip.UseAnimation = false;
            this.blackToolTip.UseFading = false;
            // 
            // RangeHistogram
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.GradientActiveCaption;
            this.DoubleBuffered = true;
            this.Name = "RangeHistogram";
            this.Size = new System.Drawing.Size(150, 40);
            this.Paint += new System.Windows.Forms.PaintEventHandler(this.RangeHistogram_Paint);
            this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.RangeHistogram_MouseDown);
            this.MouseEnter += new System.EventHandler(this.RangeHistogram_MouseEnter);
            this.MouseLeave += new System.EventHandler(this.RangeHistogram_MouseLeave);
            this.MouseMove += new System.Windows.Forms.MouseEventHandler(this.RangeHistogram_MouseMove);
            this.MouseUp += new System.Windows.Forms.MouseEventHandler(this.RangeHistogram_MouseUp);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ToolTip whiteToolTip;
        private System.Windows.Forms.ToolTip blackToolTip;

    }
}
