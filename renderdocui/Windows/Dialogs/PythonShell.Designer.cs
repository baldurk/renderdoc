namespace renderdocui.Windows.Dialogs
{
    partial class PythonShell
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
            System.Windows.Forms.ToolStripContainer toolStripContainer1;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PythonShell));
            this.scriptTable = new System.Windows.Forms.TableLayoutPanel();
            this.scriptSplit = new System.Windows.Forms.SplitContainer();
            this.scriptOutput = new System.Windows.Forms.TextBox();
            this.toolStrip2 = new System.Windows.Forms.ToolStrip();
            this.newScript = new System.Windows.Forms.ToolStripButton();
            this.openButton = new System.Windows.Forms.ToolStripButton();
            this.saveAs = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.runButton = new System.Windows.Forms.ToolStripButton();
            this.abortButton = new System.Windows.Forms.ToolStripButton();
            this.shellTable = new System.Windows.Forms.TableLayoutPanel();
            this.interactiveInput = new System.Windows.Forms.TextBox();
            this.interactiveOutput = new System.Windows.Forms.TextBox();
            this.executeCmd = new System.Windows.Forms.Button();
            this.clearCmd = new System.Windows.Forms.Button();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.shellMode = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.scriptMode = new System.Windows.Forms.ToolStripButton();
            this.saveDialog = new System.Windows.Forms.SaveFileDialog();
            this.openDialog = new System.Windows.Forms.OpenFileDialog();
            this.linenumTimer = new System.Windows.Forms.Timer(this.components);
            toolStripContainer1 = new System.Windows.Forms.ToolStripContainer();
            toolStripContainer1.ContentPanel.SuspendLayout();
            toolStripContainer1.TopToolStripPanel.SuspendLayout();
            toolStripContainer1.SuspendLayout();
            this.scriptTable.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.scriptSplit)).BeginInit();
            this.scriptSplit.Panel2.SuspendLayout();
            this.scriptSplit.SuspendLayout();
            this.toolStrip2.SuspendLayout();
            this.shellTable.SuspendLayout();
            this.toolStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // toolStripContainer1
            // 
            // 
            // toolStripContainer1.ContentPanel
            // 
            toolStripContainer1.ContentPanel.Controls.Add(this.scriptTable);
            toolStripContainer1.ContentPanel.Controls.Add(this.shellTable);
            toolStripContainer1.ContentPanel.Size = new System.Drawing.Size(658, 425);
            toolStripContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            toolStripContainer1.Location = new System.Drawing.Point(0, 0);
            toolStripContainer1.Name = "toolStripContainer1";
            toolStripContainer1.Size = new System.Drawing.Size(658, 450);
            toolStripContainer1.TabIndex = 0;
            toolStripContainer1.Text = "toolStripContainer1";
            // 
            // toolStripContainer1.TopToolStripPanel
            // 
            toolStripContainer1.TopToolStripPanel.Controls.Add(this.toolStrip1);
            // 
            // scriptTable
            // 
            this.scriptTable.ColumnCount = 1;
            this.scriptTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.scriptTable.Controls.Add(this.scriptSplit, 0, 1);
            this.scriptTable.Controls.Add(this.toolStrip2, 0, 0);
            this.scriptTable.Location = new System.Drawing.Point(340, 40);
            this.scriptTable.Name = "scriptTable";
            this.scriptTable.RowCount = 2;
            this.scriptTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.scriptTable.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.scriptTable.Size = new System.Drawing.Size(306, 373);
            this.scriptTable.TabIndex = 2;
            // 
            // scriptSplit
            // 
            this.scriptSplit.Dock = System.Windows.Forms.DockStyle.Fill;
            this.scriptSplit.Location = new System.Drawing.Point(3, 28);
            this.scriptSplit.Name = "scriptSplit";
            // 
            // scriptSplit.Panel2
            // 
            this.scriptSplit.Panel2.Controls.Add(this.scriptOutput);
            this.scriptSplit.Size = new System.Drawing.Size(300, 342);
            this.scriptSplit.SplitterDistance = 193;
            this.scriptSplit.TabIndex = 1;
            // 
            // scriptOutput
            // 
            this.scriptOutput.BackColor = System.Drawing.SystemColors.Window;
            this.scriptOutput.Dock = System.Windows.Forms.DockStyle.Fill;
            this.scriptOutput.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.scriptOutput.Location = new System.Drawing.Point(0, 0);
            this.scriptOutput.Multiline = true;
            this.scriptOutput.Name = "scriptOutput";
            this.scriptOutput.ReadOnly = true;
            this.scriptOutput.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.scriptOutput.Size = new System.Drawing.Size(103, 342);
            this.scriptOutput.TabIndex = 2;
            // 
            // toolStrip2
            // 
            this.toolStrip2.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip2.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.newScript,
            this.openButton,
            this.saveAs,
            this.toolStripSeparator2,
            this.runButton,
            this.abortButton});
            this.toolStrip2.Location = new System.Drawing.Point(0, 0);
            this.toolStrip2.Name = "toolStrip2";
            this.toolStrip2.Size = new System.Drawing.Size(306, 25);
            this.toolStrip2.TabIndex = 2;
            this.toolStrip2.Text = "toolStrip2";
            // 
            // newScript
            // 
            this.newScript.Image = global::renderdocui.Properties.Resources.page_white_edit;
            this.newScript.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.newScript.Name = "newScript";
            this.newScript.Size = new System.Drawing.Size(48, 22);
            this.newScript.Text = "New";
            this.newScript.Click += new System.EventHandler(this.newScript_Click);
            // 
            // openButton
            // 
            this.openButton.Image = global::renderdocui.Properties.Resources.folder_page;
            this.openButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.openButton.Name = "openButton";
            this.openButton.Size = new System.Drawing.Size(53, 22);
            this.openButton.Text = "Open";
            this.openButton.Click += new System.EventHandler(this.openButton_Click);
            // 
            // saveAs
            // 
            this.saveAs.Image = global::renderdocui.Properties.Resources.save;
            this.saveAs.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.saveAs.Name = "saveAs";
            this.saveAs.Size = new System.Drawing.Size(66, 22);
            this.saveAs.Text = "Save As";
            this.saveAs.Click += new System.EventHandler(this.saveAs_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(6, 25);
            // 
            // runButton
            // 
            this.runButton.Image = global::renderdocui.Properties.Resources.runfwd;
            this.runButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.runButton.Name = "runButton";
            this.runButton.Size = new System.Drawing.Size(46, 22);
            this.runButton.Text = "Run";
            this.runButton.Click += new System.EventHandler(this.runButton_Click);
            // 
            // abortButton
            // 
            this.abortButton.Image = global::renderdocui.Properties.Resources.delete;
            this.abortButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.abortButton.Name = "abortButton";
            this.abortButton.Size = new System.Drawing.Size(54, 22);
            this.abortButton.Text = "Abort";
            this.abortButton.Click += new System.EventHandler(this.abortButton_Click);
            // 
            // shellTable
            // 
            this.shellTable.ColumnCount = 3;
            this.shellTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.shellTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.shellTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.shellTable.Controls.Add(this.interactiveInput, 0, 0);
            this.shellTable.Controls.Add(this.interactiveOutput, 0, 2);
            this.shellTable.Controls.Add(this.executeCmd, 1, 0);
            this.shellTable.Controls.Add(this.clearCmd, 2, 0);
            this.shellTable.Location = new System.Drawing.Point(25, 40);
            this.shellTable.Name = "shellTable";
            this.shellTable.RowCount = 3;
            this.shellTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.shellTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.shellTable.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.shellTable.Size = new System.Drawing.Size(293, 373);
            this.shellTable.TabIndex = 0;
            // 
            // interactiveInput
            // 
            this.interactiveInput.AcceptsTab = true;
            this.interactiveInput.Dock = System.Windows.Forms.DockStyle.Top;
            this.interactiveInput.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.interactiveInput.Location = new System.Drawing.Point(3, 3);
            this.interactiveInput.MinimumSize = new System.Drawing.Size(4, 20);
            this.interactiveInput.Multiline = true;
            this.interactiveInput.Name = "interactiveInput";
            this.interactiveInput.Size = new System.Drawing.Size(175, 20);
            this.interactiveInput.TabIndex = 0;
            this.interactiveInput.WordWrap = false;
            this.interactiveInput.TextChanged += new System.EventHandler(this.interactiveInput_TextChanged);
            this.interactiveInput.KeyDown += new System.Windows.Forms.KeyEventHandler(this.interactiveInput_KeyDown);
            this.interactiveInput.Layout += new System.Windows.Forms.LayoutEventHandler(this.interactiveInput_Layout);
            // 
            // interactiveOutput
            // 
            this.interactiveOutput.BackColor = System.Drawing.SystemColors.Window;
            this.shellTable.SetColumnSpan(this.interactiveOutput, 3);
            this.interactiveOutput.Dock = System.Windows.Forms.DockStyle.Fill;
            this.interactiveOutput.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.interactiveOutput.Location = new System.Drawing.Point(3, 32);
            this.interactiveOutput.Multiline = true;
            this.interactiveOutput.Name = "interactiveOutput";
            this.interactiveOutput.ReadOnly = true;
            this.interactiveOutput.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.interactiveOutput.Size = new System.Drawing.Size(287, 338);
            this.interactiveOutput.TabIndex = 1;
            // 
            // executeCmd
            // 
            this.executeCmd.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.executeCmd.Location = new System.Drawing.Point(184, 3);
            this.executeCmd.Name = "executeCmd";
            this.executeCmd.Size = new System.Drawing.Size(60, 23);
            this.executeCmd.TabIndex = 2;
            this.executeCmd.Text = "Execute";
            this.executeCmd.UseVisualStyleBackColor = true;
            this.executeCmd.Click += new System.EventHandler(this.executeCmd_Click);
            // 
            // clearCmd
            // 
            this.clearCmd.Location = new System.Drawing.Point(250, 3);
            this.clearCmd.Name = "clearCmd";
            this.clearCmd.Size = new System.Drawing.Size(40, 23);
            this.clearCmd.TabIndex = 3;
            this.clearCmd.Text = "Clear";
            this.clearCmd.UseVisualStyleBackColor = true;
            this.clearCmd.Click += new System.EventHandler(this.clearCmd_Click);
            // 
            // toolStrip1
            // 
            this.toolStrip1.Dock = System.Windows.Forms.DockStyle.None;
            this.toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.shellMode,
            this.toolStripSeparator1,
            this.scriptMode});
            this.toolStrip1.Location = new System.Drawing.Point(3, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.Size = new System.Drawing.Size(192, 25);
            this.toolStrip1.TabIndex = 0;
            // 
            // shellMode
            // 
            this.shellMode.Checked = true;
            this.shellMode.CheckState = System.Windows.Forms.CheckState.Checked;
            this.shellMode.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.shellMode.Image = ((System.Drawing.Image)(resources.GetObject("shellMode.Image")));
            this.shellMode.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.shellMode.Name = "shellMode";
            this.shellMode.Size = new System.Drawing.Size(88, 22);
            this.shellMode.Text = "Interactive shell";
            this.shellMode.Click += new System.EventHandler(this.mode_Changed);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(6, 25);
            // 
            // scriptMode
            // 
            this.scriptMode.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.scriptMode.Image = ((System.Drawing.Image)(resources.GetObject("scriptMode.Image")));
            this.scriptMode.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.scriptMode.Name = "scriptMode";
            this.scriptMode.Size = new System.Drawing.Size(64, 22);
            this.scriptMode.Text = "Run scripts";
            this.scriptMode.Click += new System.EventHandler(this.mode_Changed);
            // 
            // saveDialog
            // 
            this.saveDialog.DefaultExt = "py";
            this.saveDialog.Filter = "Python Scripts (*.py)|*.py";
            this.saveDialog.Title = "Save script as .py";
            // 
            // openDialog
            // 
            this.openDialog.DefaultExt = "py";
            this.openDialog.Filter = "Python Scripts (*.py)|*.py";
            this.openDialog.Title = "Open .py script";
            // 
            // linenumTimer
            // 
            this.linenumTimer.Interval = 500;
            this.linenumTimer.Tick += new System.EventHandler(this.linenumTimer_Tick);
            // 
            // PythonShell
            // 
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(658, 450);
            this.Controls.Add(toolStripContainer1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "PythonShell";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.Document;
            this.Text = "PythonShell";
            this.DragDrop += new System.Windows.Forms.DragEventHandler(this.shell_DragDrop);
            this.DragEnter += new System.Windows.Forms.DragEventHandler(this.shell_DragEnter);
            toolStripContainer1.ContentPanel.ResumeLayout(false);
            toolStripContainer1.TopToolStripPanel.ResumeLayout(false);
            toolStripContainer1.TopToolStripPanel.PerformLayout();
            toolStripContainer1.ResumeLayout(false);
            toolStripContainer1.PerformLayout();
            this.scriptTable.ResumeLayout(false);
            this.scriptTable.PerformLayout();
            this.scriptSplit.Panel2.ResumeLayout(false);
            this.scriptSplit.Panel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.scriptSplit)).EndInit();
            this.scriptSplit.ResumeLayout(false);
            this.toolStrip2.ResumeLayout(false);
            this.toolStrip2.PerformLayout();
            this.shellTable.ResumeLayout(false);
            this.shellTable.PerformLayout();
            this.toolStrip1.ResumeLayout(false);
            this.toolStrip1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TableLayoutPanel shellTable;
        private System.Windows.Forms.SplitContainer scriptSplit;
        private System.Windows.Forms.TextBox interactiveInput;
        private System.Windows.Forms.TextBox interactiveOutput;
        private System.Windows.Forms.ToolStripButton shellMode;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripButton scriptMode;
        private System.Windows.Forms.Button executeCmd;
        private System.Windows.Forms.TextBox scriptOutput;
        private System.Windows.Forms.Button clearCmd;
        private System.Windows.Forms.TableLayoutPanel scriptTable;
        private System.Windows.Forms.ToolStrip toolStrip2;
        private System.Windows.Forms.ToolStripButton openButton;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripButton runButton;
        private System.Windows.Forms.ToolStripButton saveAs;
        private System.Windows.Forms.SaveFileDialog saveDialog;
        private System.Windows.Forms.OpenFileDialog openDialog;
        private System.Windows.Forms.ToolStripButton newScript;
        private System.Windows.Forms.Timer linenumTimer;
        private System.Windows.Forms.ToolStripButton abortButton;
        private System.Windows.Forms.ToolStrip toolStrip1;


    }
}