namespace renderdocui.Windows
{
    partial class TimelineBar
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
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.scroll = new System.Windows.Forms.HScrollBar();
            this.panel = new renderdocui.Controls.NoScrollPanel();
            this.tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.scroll, 0, 1);
            this.tableLayoutPanel1.Controls.Add(this.panel, 0, 0);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.Size = new System.Drawing.Size(726, 177);
            this.tableLayoutPanel1.TabIndex = 0;
            // 
            // scroll
            // 
            this.scroll.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.scroll.LargeChange = 100;
            this.scroll.Location = new System.Drawing.Point(0, 161);
            this.scroll.Maximum = 200;
            this.scroll.Name = "scroll";
            this.scroll.Size = new System.Drawing.Size(726, 16);
            this.scroll.SmallChange = 10;
            this.scroll.TabIndex = 0;
            this.scroll.Scroll += new System.Windows.Forms.ScrollEventHandler(this.scroll_Scroll);
            // 
            // panel
            // 
            this.panel.BackColor = System.Drawing.Color.White;
            this.panel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel.Location = new System.Drawing.Point(3, 3);
            this.panel.Name = "panel";
            this.panel.Size = new System.Drawing.Size(720, 155);
            this.panel.TabIndex = 1;
            this.panel.Click += new System.EventHandler(this.panel_Click);
            this.panel.Paint += new System.Windows.Forms.PaintEventHandler(this.panel_Paint);
            this.panel.MouseEnter += new System.EventHandler(this.panel_MouseEnter);
            this.panel.MouseLeave += new System.EventHandler(this.panel_MouseLeave);
            this.panel.MouseMove += new System.Windows.Forms.MouseEventHandler(this.panel_MouseMove);
            // 
            // TimelineBar
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(726, 177);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "TimelineBar";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.DockTop;
            this.Text = "Timeline";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.TimelineBar_FormClosed);
            this.Resize += new System.EventHandler(this.TimelineBar_Resize);
            this.tableLayoutPanel1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.HScrollBar scroll;
        private Controls.NoScrollPanel panel;
    }
}