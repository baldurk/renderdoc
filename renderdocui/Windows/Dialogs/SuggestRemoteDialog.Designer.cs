namespace renderdocui.Windows.Dialogs
{
    partial class SuggestRemoteDialog
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
            this.components = new System.ComponentModel.Container();
            System.Windows.Forms.TableLayoutPanel tableLayout;
            System.Windows.Forms.Panel topPanel;
            System.Windows.Forms.Panel bottomPanel;
            this.warning = new System.Windows.Forms.Label();
            this.icon = new System.Windows.Forms.PictureBox();
            this.remote = new System.Windows.Forms.CheckBox();
            this.alwaysLocal = new System.Windows.Forms.CheckBox();
            this.local = new System.Windows.Forms.Button();
            this.cancel = new System.Windows.Forms.Button();
            this.remoteDropDown = new System.Windows.Forms.ContextMenuStrip(this.components);
            tableLayout = new System.Windows.Forms.TableLayoutPanel();
            topPanel = new System.Windows.Forms.Panel();
            bottomPanel = new System.Windows.Forms.Panel();
            tableLayout.SuspendLayout();
            topPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.icon)).BeginInit();
            bottomPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayout
            // 
            tableLayout.ColumnCount = 1;
            tableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayout.Controls.Add(topPanel, 0, 0);
            tableLayout.Controls.Add(bottomPanel, 0, 1);
            tableLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayout.Location = new System.Drawing.Point(0, 0);
            tableLayout.Margin = new System.Windows.Forms.Padding(0);
            tableLayout.Name = "tableLayout";
            tableLayout.RowCount = 2;
            tableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 40F));
            tableLayout.Size = new System.Drawing.Size(397, 163);
            tableLayout.TabIndex = 0;
            // 
            // topPanel
            // 
            topPanel.BackColor = System.Drawing.SystemColors.Window;
            topPanel.Controls.Add(this.warning);
            topPanel.Controls.Add(this.icon);
            topPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            topPanel.Location = new System.Drawing.Point(0, 0);
            topPanel.Margin = new System.Windows.Forms.Padding(0);
            topPanel.Name = "topPanel";
            topPanel.Size = new System.Drawing.Size(397, 123);
            topPanel.TabIndex = 0;
            // 
            // warning
            // 
            this.warning.AutoSize = true;
            this.warning.ForeColor = System.Drawing.SystemColors.WindowText;
            this.warning.Location = new System.Drawing.Point(61, 22);
            this.warning.Name = "warning";
            this.warning.Size = new System.Drawing.Size(47, 13);
            this.warning.TabIndex = 1;
            this.warning.Text = "Warning";
            // 
            // icon
            // 
            this.icon.Location = new System.Drawing.Point(22, 22);
            this.icon.Name = "icon";
            this.icon.Size = new System.Drawing.Size(32, 32);
            this.icon.TabIndex = 0;
            this.icon.TabStop = false;
            // 
            // bottomPanel
            // 
            bottomPanel.Controls.Add(this.remote);
            bottomPanel.Controls.Add(this.alwaysLocal);
            bottomPanel.Controls.Add(this.local);
            bottomPanel.Controls.Add(this.cancel);
            bottomPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            bottomPanel.Location = new System.Drawing.Point(0, 123);
            bottomPanel.Margin = new System.Windows.Forms.Padding(0);
            bottomPanel.Name = "bottomPanel";
            bottomPanel.Size = new System.Drawing.Size(397, 40);
            bottomPanel.TabIndex = 1;
            // 
            // remote
            // 
            this.remote.Appearance = System.Windows.Forms.Appearance.Button;
            this.remote.AutoCheck = false;
            this.remote.Image = global::renderdocui.Properties.Resources.down_arrow;
            this.remote.ImageAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.remote.Location = new System.Drawing.Point(152, 8);
            this.remote.Name = "remote";
            this.remote.Size = new System.Drawing.Size(75, 23);
            this.remote.TabIndex = 2;
            this.remote.Text = "Remote    ";
            this.remote.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.remote.UseVisualStyleBackColor = true;
            this.remote.Click += new System.EventHandler(this.remote_Click);
            // 
            // alwaysLocal
            // 
            this.alwaysLocal.AutoSize = true;
            this.alwaysLocal.Location = new System.Drawing.Point(12, 5);
            this.alwaysLocal.Name = "alwaysLocal";
            this.alwaysLocal.Size = new System.Drawing.Size(124, 30);
            this.alwaysLocal.TabIndex = 3;
            this.alwaysLocal.Text = "Don\'t prompt again,\r\nalways replay locally.\r\n";
            this.alwaysLocal.UseVisualStyleBackColor = true;
            this.alwaysLocal.CheckedChanged += new System.EventHandler(this.alwaysLocal_CheckedChanged);
            // 
            // local
            // 
            this.local.Location = new System.Drawing.Point(233, 8);
            this.local.Name = "local";
            this.local.Size = new System.Drawing.Size(75, 23);
            this.local.TabIndex = 1;
            this.local.Text = "Local";
            this.local.UseVisualStyleBackColor = true;
            this.local.Click += new System.EventHandler(this.local_Click);
            // 
            // cancel
            // 
            this.cancel.Location = new System.Drawing.Point(314, 8);
            this.cancel.Name = "cancel";
            this.cancel.Size = new System.Drawing.Size(75, 23);
            this.cancel.TabIndex = 0;
            this.cancel.Text = "Cancel";
            this.cancel.UseVisualStyleBackColor = true;
            this.cancel.Click += new System.EventHandler(this.cancel_Click);
            // 
            // remoteDropDown
            // 
            this.remoteDropDown.Name = "remoteDropDown";
            this.remoteDropDown.Size = new System.Drawing.Size(61, 4);
            this.remoteDropDown.Closed += new System.Windows.Forms.ToolStripDropDownClosedEventHandler(this.remoteDropDown_Closed);
            this.remoteDropDown.ItemAdded += new System.Windows.Forms.ToolStripItemEventHandler(this.remoteDropDown_ItemAdded);
            this.remoteDropDown.ItemClicked += new System.Windows.Forms.ToolStripItemClickedEventHandler(this.remoteDropDown_ItemClicked);
            // 
            // SuggestRemoteDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(397, 163);
            this.Controls.Add(tableLayout);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "SuggestRemoteDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Choose Remote Host?";
            tableLayout.ResumeLayout(false);
            topPanel.ResumeLayout(false);
            topPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.icon)).EndInit();
            bottomPanel.ResumeLayout(false);
            bottomPanel.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.PictureBox icon;
        private System.Windows.Forms.Label warning;
        private System.Windows.Forms.CheckBox remote;
        private System.Windows.Forms.Button local;
        private System.Windows.Forms.Button cancel;
        private System.Windows.Forms.CheckBox alwaysLocal;
        private System.Windows.Forms.ContextMenuStrip remoteDropDown;
    }
}