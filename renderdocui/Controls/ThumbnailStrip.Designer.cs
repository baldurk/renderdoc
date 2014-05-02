namespace renderdocui.Controls
{
    partial class ThumbnailStrip
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
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
            this.panel = new System.Windows.Forms.Panel();
            this.hscroll = new System.Windows.Forms.HScrollBar();
            this.vscroll = new System.Windows.Forms.VScrollBar();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 2;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel1.Controls.Add(this.panel, 0, 0);
            tableLayoutPanel1.Controls.Add(this.hscroll, 0, 1);
            tableLayoutPanel1.Controls.Add(this.vscroll, 1, 0);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(791, 246);
            tableLayoutPanel1.TabIndex = 1;
            // 
            // panel
            // 
            this.panel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel.Location = new System.Drawing.Point(0, 0);
            this.panel.Margin = new System.Windows.Forms.Padding(0);
            this.panel.Name = "panel";
            this.panel.Size = new System.Drawing.Size(775, 230);
            this.panel.TabIndex = 0;
            this.panel.ControlAdded += new System.Windows.Forms.ControlEventHandler(this.panel_ControlAddRemove);
            this.panel.ControlRemoved += new System.Windows.Forms.ControlEventHandler(this.panel_ControlAddRemove);
            this.panel.MouseClick += new System.Windows.Forms.MouseEventHandler(this.panel_MouseClick);
            // 
            // hscroll
            // 
            this.hscroll.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.hscroll.Location = new System.Drawing.Point(0, 230);
            this.hscroll.Name = "hscroll";
            this.hscroll.Size = new System.Drawing.Size(775, 16);
            this.hscroll.TabIndex = 1;
            this.hscroll.Scroll += new System.Windows.Forms.ScrollEventHandler(this.hscroll_Scroll);
            // 
            // vscroll
            // 
            this.vscroll.Dock = System.Windows.Forms.DockStyle.Right;
            this.vscroll.Location = new System.Drawing.Point(775, 0);
            this.vscroll.Name = "vscroll";
            this.vscroll.Size = new System.Drawing.Size(16, 230);
            this.vscroll.TabIndex = 2;
            this.vscroll.Scroll += new System.Windows.Forms.ScrollEventHandler(this.vscroll_Scroll);
            // 
            // ThumbnailStrip
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(tableLayoutPanel1);
            this.Margin = new System.Windows.Forms.Padding(0);
            this.Name = "ThumbnailStrip";
            this.Size = new System.Drawing.Size(791, 246);
            this.Layout += new System.Windows.Forms.LayoutEventHandler(this.ThumbnailStrip_Layout);
            tableLayoutPanel1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panel;
        private System.Windows.Forms.HScrollBar hscroll;
        private System.Windows.Forms.VScrollBar vscroll;
    }
}
