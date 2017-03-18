namespace renderdocui.Windows.Dialogs
{
    partial class OrderedListEditor
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle3 = new System.Windows.Forms.DataGridViewCellStyle();
            this.items = new System.Windows.Forms.DataGridView();
            this.number = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.name = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.browse = new System.Windows.Forms.DataGridViewImageColumn();
            this.moveup = new System.Windows.Forms.DataGridViewImageColumn();
            this.movedown = new System.Windows.Forms.DataGridViewImageColumn();
            this.itemFolderBrowser = new System.Windows.Forms.FolderBrowserDialog();
            this.itemFileBrowser = new System.Windows.Forms.OpenFileDialog();
            this.cancel = new System.Windows.Forms.Button();
            this.ok = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.items)).BeginInit();
            this.SuspendLayout();
            // 
            // items
            // 
            this.items.AllowUserToResizeColumns = false;
            this.items.AllowUserToResizeRows = false;
            this.items.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.items.BackgroundColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.items.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle1;
            this.items.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.items.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.number,
            this.name,
            this.browse,
            this.moveup,
            this.movedown});
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.items.DefaultCellStyle = dataGridViewCellStyle2;
            this.items.Location = new System.Drawing.Point(12, 12);
            this.items.MultiSelect = false;
            this.items.Name = "items";
            dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.items.RowHeadersDefaultCellStyle = dataGridViewCellStyle3;
            this.items.RowHeadersVisible = false;
            this.items.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.items.Size = new System.Drawing.Size(464, 326);
            this.items.TabIndex = 5;
            this.items.CellContentClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.items_CellContentClick);
            this.items.CellFormatting += new System.Windows.Forms.DataGridViewCellFormattingEventHandler(this.items_CellFormatting);
            this.items.CellMouseMove += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.items_CellMouseMove);
            // 
            // number
            // 
            this.number.HeaderText = "#";
            this.number.MinimumWidth = 10;
            this.number.Name = "number";
            this.number.ReadOnly = true;
            this.number.Width = 32;
            // 
            // name
            // 
            this.name.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this.name.HeaderText = "Item name (changed at runtime)";
            this.name.Name = "name";
            // 
            // browse
            // 
            this.browse.HeaderText = "";
            this.browse.Image = global::renderdocui.Properties.Resources.folder_page;
            this.browse.MinimumWidth = 15;
            this.browse.Name = "browse";
            this.browse.ReadOnly = true;
            this.browse.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.browse.Width = 32;
            // 
            // moveup
            // 
            this.moveup.HeaderText = "";
            this.moveup.Image = global::renderdocui.Properties.Resources.up_arrow;
            this.moveup.MinimumWidth = 15;
            this.moveup.Name = "moveup";
            this.moveup.ReadOnly = true;
            this.moveup.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.moveup.Width = 32;
            // 
            // movedown
            // 
            this.movedown.HeaderText = "";
            this.movedown.Image = global::renderdocui.Properties.Resources.down_arrow;
            this.movedown.MinimumWidth = 15;
            this.movedown.Name = "movedown";
            this.movedown.ReadOnly = true;
            this.movedown.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.movedown.Width = 32;
            // 
            // itemFolderBrowser
            // 
            this.itemFolderBrowser.RootFolder = System.Environment.SpecialFolder.MyComputer;
            // 
            // cancel
            // 
            this.cancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancel.Location = new System.Drawing.Point(401, 344);
            this.cancel.Name = "cancel";
            this.cancel.Size = new System.Drawing.Size(75, 23);
            this.cancel.TabIndex = 6;
            this.cancel.Text = "Cancel";
            this.cancel.UseVisualStyleBackColor = true;
            // 
            // ok
            // 
            this.ok.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.ok.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.ok.Location = new System.Drawing.Point(320, 344);
            this.ok.Name = "ok";
            this.ok.Size = new System.Drawing.Size(75, 23);
            this.ok.TabIndex = 7;
            this.ok.Text = "OK";
            this.ok.UseVisualStyleBackColor = true;
            // 
            // OrderedListEditor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(488, 379);
            this.Controls.Add(this.ok);
            this.Controls.Add(this.cancel);
            this.Controls.Add(this.items);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
            this.Name = "OrderedListEditor";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Window Title (changed at runtime)";
            ((System.ComponentModel.ISupportInitialize)(this.items)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.DataGridView items;
        private System.Windows.Forms.DataGridViewTextBoxColumn number;
        private System.Windows.Forms.DataGridViewTextBoxColumn name;
        private System.Windows.Forms.DataGridViewImageColumn browse;
        private System.Windows.Forms.DataGridViewImageColumn moveup;
        private System.Windows.Forms.DataGridViewImageColumn movedown;
        private System.Windows.Forms.FolderBrowserDialog itemFolderBrowser;
        private System.Windows.Forms.OpenFileDialog itemFileBrowser;
        private System.Windows.Forms.Button cancel;
        private System.Windows.Forms.Button ok;

    }
}