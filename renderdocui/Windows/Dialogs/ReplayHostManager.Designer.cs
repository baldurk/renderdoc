namespace renderdocui.Windows.Dialogs
{
    partial class ReplayHostManager
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
            System.Windows.Forms.Label label2;
            this.OK = new System.Windows.Forms.Button();
            this.driversBox = new System.Windows.Forms.GroupBox();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this.localProxy = new System.Windows.Forms.ComboBox();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            this.flowLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 1;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.Controls.Add(this.OK, 0, 3);
            tableLayoutPanel1.Controls.Add(this.driversBox, 0, 2);
            tableLayoutPanel1.Controls.Add(label1, 0, 0);
            tableLayoutPanel1.Controls.Add(this.flowLayoutPanel1, 0, 1);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 4;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(420, 400);
            tableLayoutPanel1.TabIndex = 0;
            // 
            // OK
            // 
            this.OK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.OK.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.OK.Location = new System.Drawing.Point(342, 374);
            this.OK.Name = "OK";
            this.OK.Size = new System.Drawing.Size(75, 23);
            this.OK.TabIndex = 0;
            this.OK.Text = "OK";
            this.OK.UseVisualStyleBackColor = true;
            // 
            // driversBox
            // 
            this.driversBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.driversBox.Location = new System.Drawing.Point(3, 75);
            this.driversBox.Name = "driversBox";
            this.driversBox.Size = new System.Drawing.Size(414, 293);
            this.driversBox.TabIndex = 2;
            this.driversBox.TabStop = false;
            this.driversBox.Text = "Remote Replay Hosts";
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(3, 0);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(301, 39);
            label1.TabIndex = 1;
            label1.Text = "Select Replay Host for each known driver.\r\n\r\nLeave blank to indicate local replay" +
    " should be used if possible.";
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.AutoSize = true;
            this.flowLayoutPanel1.Controls.Add(label2);
            this.flowLayoutPanel1.Controls.Add(this.localProxy);
            this.flowLayoutPanel1.Location = new System.Drawing.Point(3, 42);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(278, 27);
            this.flowLayoutPanel1.TabIndex = 3;
            // 
            // label2
            // 
            label2.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(3, 7);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(145, 13);
            label2.TabIndex = 0;
            label2.Text = "Choose local API for proxying";
            // 
            // localProxy
            // 
            this.localProxy.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.localProxy.FormattingEnabled = true;
            this.localProxy.Location = new System.Drawing.Point(154, 3);
            this.localProxy.Name = "localProxy";
            this.localProxy.Size = new System.Drawing.Size(121, 21);
            this.localProxy.TabIndex = 1;
            this.localProxy.SelectedIndexChanged += new System.EventHandler(this.localProxy_SelectedIndexChanged);
            // 
            // ReplayHostManager
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(420, 400);
            this.Controls.Add(tableLayoutPanel1);
            this.Name = "ReplayHostManager";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "ReplayHostManager";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.ReplayHostManager_FormClosed);
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            this.flowLayoutPanel1.ResumeLayout(false);
            this.flowLayoutPanel1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button OK;
        private System.Windows.Forms.GroupBox driversBox;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
        private System.Windows.Forms.ComboBox localProxy;
    }
}