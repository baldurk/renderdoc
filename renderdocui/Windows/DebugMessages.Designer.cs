namespace renderdocui.Windows
{
    partial class DebugMessages
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DebugMessages));
            this.messages = new System.Windows.Forms.DataGridView();
            this.dataGridViewTextBoxColumn8 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.Source = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.dataGridViewTextBoxColumn9 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.dataGridViewTextBoxColumn10 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ID = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.dataGridViewTextBoxColumn12 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.rightClickMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.hideIndividual = new System.Windows.Forms.ToolStripMenuItem();
            this.hideType = new System.Windows.Forms.ToolStripMenuItem();
            this.hideSource = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripContainer1 = new System.Windows.Forms.ToolStripContainer();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.displayHidden = new System.Windows.Forms.ToolStripButton();
            ((System.ComponentModel.ISupportInitialize)(this.messages)).BeginInit();
            this.rightClickMenu.SuspendLayout();
            this.toolStripContainer1.ContentPanel.SuspendLayout();
            this.toolStripContainer1.TopToolStripPanel.SuspendLayout();
            this.toolStripContainer1.SuspendLayout();
            this.toolStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // messages
            // 
            this.messages.AllowUserToAddRows = false;
            this.messages.AllowUserToDeleteRows = false;
            this.messages.AllowUserToResizeRows = false;
            this.messages.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.AllCells;
            this.messages.BackgroundColor = System.Drawing.Color.White;
            this.messages.ClipboardCopyMode = System.Windows.Forms.DataGridViewClipboardCopyMode.EnableWithoutHeaderText;
            this.messages.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.messages.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.dataGridViewTextBoxColumn8,
            this.Source,
            this.dataGridViewTextBoxColumn9,
            this.dataGridViewTextBoxColumn10,
            this.ID,
            this.dataGridViewTextBoxColumn12});
            this.messages.ContextMenuStrip = this.rightClickMenu;
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.messages.DefaultCellStyle = dataGridViewCellStyle2;
            this.messages.Dock = System.Windows.Forms.DockStyle.Fill;
            this.messages.Location = new System.Drawing.Point(0, 0);
            this.messages.Name = "messages";
            this.messages.ReadOnly = true;
            this.messages.RowHeadersVisible = false;
            this.messages.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.messages.Size = new System.Drawing.Size(729, 438);
            this.messages.TabIndex = 10;
            this.messages.VirtualMode = true;
            this.messages.CellDoubleClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.messages_CellDoubleClick);
            this.messages.CellFormatting += new System.Windows.Forms.DataGridViewCellFormattingEventHandler(this.messages_CellFormatting);
            this.messages.CellPainting += new System.Windows.Forms.DataGridViewCellPaintingEventHandler(this.messages_CellPainting);
            this.messages.CellValueNeeded += new System.Windows.Forms.DataGridViewCellValueEventHandler(this.messages_CellValueNeeded);
            this.messages.MouseDown += new System.Windows.Forms.MouseEventHandler(this.messages_MouseDown);
            // 
            // dataGridViewTextBoxColumn8
            // 
            this.dataGridViewTextBoxColumn8.HeaderText = "EID";
            this.dataGridViewTextBoxColumn8.MinimumWidth = 40;
            this.dataGridViewTextBoxColumn8.Name = "dataGridViewTextBoxColumn8";
            this.dataGridViewTextBoxColumn8.ReadOnly = true;
            this.dataGridViewTextBoxColumn8.Width = 50;
            // 
            // Source
            // 
            this.Source.HeaderText = "Source";
            this.Source.Name = "Source";
            this.Source.ReadOnly = true;
            this.Source.Width = 66;
            // 
            // dataGridViewTextBoxColumn9
            // 
            this.dataGridViewTextBoxColumn9.HeaderText = "Severity";
            this.dataGridViewTextBoxColumn9.MinimumWidth = 40;
            this.dataGridViewTextBoxColumn9.Name = "dataGridViewTextBoxColumn9";
            this.dataGridViewTextBoxColumn9.ReadOnly = true;
            this.dataGridViewTextBoxColumn9.Width = 70;
            // 
            // dataGridViewTextBoxColumn10
            // 
            this.dataGridViewTextBoxColumn10.HeaderText = "Category";
            this.dataGridViewTextBoxColumn10.MinimumWidth = 40;
            this.dataGridViewTextBoxColumn10.Name = "dataGridViewTextBoxColumn10";
            this.dataGridViewTextBoxColumn10.ReadOnly = true;
            this.dataGridViewTextBoxColumn10.Width = 74;
            // 
            // ID
            // 
            this.ID.HeaderText = "ID";
            this.ID.Name = "ID";
            this.ID.ReadOnly = true;
            this.ID.Width = 43;
            // 
            // dataGridViewTextBoxColumn12
            // 
            this.dataGridViewTextBoxColumn12.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this.dataGridViewTextBoxColumn12.HeaderText = "Description";
            this.dataGridViewTextBoxColumn12.MinimumWidth = 40;
            this.dataGridViewTextBoxColumn12.Name = "dataGridViewTextBoxColumn12";
            this.dataGridViewTextBoxColumn12.ReadOnly = true;
            this.dataGridViewTextBoxColumn12.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
            // 
            // rightClickMenu
            // 
            this.rightClickMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.hideIndividual,
            this.hideType,
            this.hideSource});
            this.rightClickMenu.Name = "rightClickMenu";
            this.rightClickMenu.Size = new System.Drawing.Size(238, 70);
            this.rightClickMenu.Opening += new System.ComponentModel.CancelEventHandler(this.rightClickMenu_Opening);
            // 
            // hideIndividual
            // 
            this.hideIndividual.Name = "hideIndividual";
            this.hideIndividual.Size = new System.Drawing.Size(237, 22);
            this.hideIndividual.Text = "Show/Hide this individual message";
            this.hideIndividual.Click += new System.EventHandler(this.hideIndividual_Click);
            // 
            // hideType
            // 
            this.hideType.Name = "hideType";
            this.hideType.Size = new System.Drawing.Size(237, 22);
            this.hideType.Text = "Show/Hide this message type";
            this.hideType.Click += new System.EventHandler(this.hideType_Click);
            // 
            // hideSource
            // 
            this.hideSource.Name = "hideSource";
            this.hideSource.Size = new System.Drawing.Size(237, 22);
            this.hideSource.Text = "Show/Hide this message source";
            this.hideSource.Click += new System.EventHandler(this.hideSource_Click);
            // 
            // toolStripContainer1
            // 
            // 
            // toolStripContainer1.ContentPanel
            // 
            this.toolStripContainer1.ContentPanel.Controls.Add(this.messages);
            this.toolStripContainer1.ContentPanel.Size = new System.Drawing.Size(729, 438);
            this.toolStripContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.toolStripContainer1.Location = new System.Drawing.Point(0, 0);
            this.toolStripContainer1.Name = "toolStripContainer1";
            this.toolStripContainer1.Size = new System.Drawing.Size(729, 463);
            this.toolStripContainer1.TabIndex = 11;
            this.toolStripContainer1.Text = "toolStripContainer1";
            // 
            // toolStripContainer1.TopToolStripPanel
            // 
            this.toolStripContainer1.TopToolStripPanel.Controls.Add(this.toolStrip1);
            // 
            // toolStrip1
            // 
            this.toolStrip1.Dock = System.Windows.Forms.DockStyle.None;
            this.toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.displayHidden});
            this.toolStrip1.Location = new System.Drawing.Point(3, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.Size = new System.Drawing.Size(164, 25);
            this.toolStrip1.TabIndex = 0;
            // 
            // displayHidden
            // 
            this.displayHidden.CheckOnClick = true;
            this.displayHidden.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.displayHidden.Image = ((System.Drawing.Image)(resources.GetObject("displayHidden.Image")));
            this.displayHidden.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.displayHidden.Name = "displayHidden";
            this.displayHidden.Size = new System.Drawing.Size(130, 22);
            this.displayHidden.Text = "Display hidden messages";
            this.displayHidden.CheckedChanged += new System.EventHandler(this.displayHidden_CheckedChanged);
            // 
            // DebugMessages
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(729, 463);
            this.Controls.Add(this.toolStripContainer1);
            this.Name = "DebugMessages";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.DebugMessages_FormClosed);
            ((System.ComponentModel.ISupportInitialize)(this.messages)).EndInit();
            this.rightClickMenu.ResumeLayout(false);
            this.toolStripContainer1.ContentPanel.ResumeLayout(false);
            this.toolStripContainer1.TopToolStripPanel.ResumeLayout(false);
            this.toolStripContainer1.TopToolStripPanel.PerformLayout();
            this.toolStripContainer1.ResumeLayout(false);
            this.toolStripContainer1.PerformLayout();
            this.toolStrip1.ResumeLayout(false);
            this.toolStrip1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.DataGridView messages;
        private System.Windows.Forms.ToolStripContainer toolStripContainer1;
        private System.Windows.Forms.ToolStrip toolStrip1;
        private System.Windows.Forms.ToolStripButton displayHidden;
        private System.Windows.Forms.ContextMenuStrip rightClickMenu;
        private System.Windows.Forms.ToolStripMenuItem hideIndividual;
        private System.Windows.Forms.ToolStripMenuItem hideType;
        private System.Windows.Forms.DataGridViewTextBoxColumn dataGridViewTextBoxColumn8;
        private System.Windows.Forms.DataGridViewTextBoxColumn Source;
        private System.Windows.Forms.DataGridViewTextBoxColumn dataGridViewTextBoxColumn9;
        private System.Windows.Forms.DataGridViewTextBoxColumn dataGridViewTextBoxColumn10;
        private System.Windows.Forms.DataGridViewTextBoxColumn ID;
        private System.Windows.Forms.DataGridViewTextBoxColumn dataGridViewTextBoxColumn12;
        private System.Windows.Forms.ToolStripMenuItem hideSource;

    }
}