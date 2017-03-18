namespace renderdocui.Windows.Dialogs
{
    partial class FindAllDialog
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
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            this.options = new System.Windows.Forms.GroupBox();
            this.wordstart = new System.Windows.Forms.CheckBox();
            this.wholeword = new System.Windows.Forms.CheckBox();
            this.matchcase = new System.Windows.Forms.CheckBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.regexsearch = new System.Windows.Forms.RadioButton();
            this.normalsearch = new System.Windows.Forms.RadioButton();
            this.dofind = new System.Windows.Forms.Button();
            this.regexoptions = new System.Windows.Forms.GroupBox();
            this.singleline = new System.Windows.Forms.CheckBox();
            this.rtl = new System.Windows.Forms.CheckBox();
            this.multi = new System.Windows.Forms.CheckBox();
            this.ignorewspace = new System.Windows.Forms.CheckBox();
            this.ignorecase = new System.Windows.Forms.CheckBox();
            this.explicitcap = new System.Windows.Forms.CheckBox();
            this.ecma = new System.Windows.Forms.CheckBox();
            this.invariant = new System.Windows.Forms.CheckBox();
            this.compiled = new System.Windows.Forms.CheckBox();
            this.findtext = new System.Windows.Forms.ComboBox();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            this.options.SuspendLayout();
            this.panel1.SuspendLayout();
            this.regexoptions.SuspendLayout();
            this.SuspendLayout();
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(12, 16);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(27, 13);
            label1.TabIndex = 0;
            label1.Text = "Find";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(13, 48);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(68, 13);
            label2.TabIndex = 3;
            label2.Text = "Search Type";
            // 
            // options
            // 
            this.options.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.options.Controls.Add(this.wordstart);
            this.options.Controls.Add(this.wholeword);
            this.options.Controls.Add(this.matchcase);
            this.options.Location = new System.Drawing.Point(16, 98);
            this.options.Name = "options";
            this.options.Size = new System.Drawing.Size(384, 94);
            this.options.TabIndex = 4;
            this.options.TabStop = false;
            this.options.Text = "Options";
            // 
            // wordstart
            // 
            this.wordstart.AutoSize = true;
            this.wordstart.Location = new System.Drawing.Point(7, 68);
            this.wordstart.Name = "wordstart";
            this.wordstart.Size = new System.Drawing.Size(77, 17);
            this.wordstart.TabIndex = 2;
            this.wordstart.Text = "Word Start";
            this.wordstart.UseVisualStyleBackColor = true;
            // 
            // wholeword
            // 
            this.wholeword.AutoSize = true;
            this.wholeword.Location = new System.Drawing.Point(7, 44);
            this.wholeword.Name = "wholeword";
            this.wholeword.Size = new System.Drawing.Size(86, 17);
            this.wholeword.TabIndex = 1;
            this.wholeword.Text = "Whole Word";
            this.wholeword.UseVisualStyleBackColor = true;
            // 
            // matchcase
            // 
            this.matchcase.AutoSize = true;
            this.matchcase.Location = new System.Drawing.Point(7, 20);
            this.matchcase.Name = "matchcase";
            this.matchcase.Size = new System.Drawing.Size(83, 17);
            this.matchcase.TabIndex = 0;
            this.matchcase.Text = "Match Case";
            this.matchcase.UseVisualStyleBackColor = true;
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.regexsearch);
            this.panel1.Controls.Add(this.normalsearch);
            this.panel1.Location = new System.Drawing.Point(16, 67);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(197, 25);
            this.panel1.TabIndex = 2;
            // 
            // regexsearch
            // 
            this.regexsearch.AutoSize = true;
            this.regexsearch.Location = new System.Drawing.Point(77, 3);
            this.regexsearch.Name = "regexsearch";
            this.regexsearch.Size = new System.Drawing.Size(116, 17);
            this.regexsearch.TabIndex = 1;
            this.regexsearch.Text = "Regular Expression";
            this.regexsearch.UseVisualStyleBackColor = true;
            this.regexsearch.CheckedChanged += new System.EventHandler(this.regexsearch_CheckedChanged);
            // 
            // normalsearch
            // 
            this.normalsearch.AutoSize = true;
            this.normalsearch.Checked = true;
            this.normalsearch.Location = new System.Drawing.Point(3, 3);
            this.normalsearch.Name = "normalsearch";
            this.normalsearch.Size = new System.Drawing.Size(68, 17);
            this.normalsearch.TabIndex = 0;
            this.normalsearch.TabStop = true;
            this.normalsearch.Text = "Standard";
            this.normalsearch.UseVisualStyleBackColor = true;
            this.normalsearch.CheckedChanged += new System.EventHandler(this.normalsearch_CheckedChanged);
            // 
            // dofind
            // 
            this.dofind.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.dofind.Location = new System.Drawing.Point(308, 199);
            this.dofind.Name = "dofind";
            this.dofind.Size = new System.Drawing.Size(91, 23);
            this.dofind.TabIndex = 5;
            this.dofind.Text = "Find In All Files";
            this.dofind.UseVisualStyleBackColor = true;
            this.dofind.Click += new System.EventHandler(this.dofind_Click);
            // 
            // regexoptions
            // 
            this.regexoptions.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.regexoptions.Controls.Add(this.singleline);
            this.regexoptions.Controls.Add(this.rtl);
            this.regexoptions.Controls.Add(this.multi);
            this.regexoptions.Controls.Add(this.ignorewspace);
            this.regexoptions.Controls.Add(this.ignorecase);
            this.regexoptions.Controls.Add(this.explicitcap);
            this.regexoptions.Controls.Add(this.ecma);
            this.regexoptions.Controls.Add(this.invariant);
            this.regexoptions.Controls.Add(this.compiled);
            this.regexoptions.Location = new System.Drawing.Point(15, 241);
            this.regexoptions.Name = "regexoptions";
            this.regexoptions.Size = new System.Drawing.Size(385, 93);
            this.regexoptions.TabIndex = 6;
            this.regexoptions.TabStop = false;
            this.regexoptions.Text = "Options";
            // 
            // singleline
            // 
            this.singleline.AutoSize = true;
            this.singleline.Location = new System.Drawing.Point(274, 68);
            this.singleline.Name = "singleline";
            this.singleline.Size = new System.Drawing.Size(71, 17);
            this.singleline.TabIndex = 8;
            this.singleline.Text = "Singleline";
            this.singleline.UseVisualStyleBackColor = true;
            // 
            // rtl
            // 
            this.rtl.AutoSize = true;
            this.rtl.Location = new System.Drawing.Point(274, 44);
            this.rtl.Name = "rtl";
            this.rtl.Size = new System.Drawing.Size(84, 17);
            this.rtl.TabIndex = 7;
            this.rtl.Text = "Right to Left";
            this.rtl.UseVisualStyleBackColor = true;
            // 
            // multi
            // 
            this.multi.AutoSize = true;
            this.multi.Location = new System.Drawing.Point(274, 20);
            this.multi.Name = "multi";
            this.multi.Size = new System.Drawing.Size(64, 17);
            this.multi.TabIndex = 6;
            this.multi.Text = "Multiline";
            this.multi.UseVisualStyleBackColor = true;
            // 
            // ignorewspace
            // 
            this.ignorewspace.AutoSize = true;
            this.ignorewspace.Location = new System.Drawing.Point(119, 68);
            this.ignorewspace.Name = "ignorewspace";
            this.ignorewspace.Size = new System.Drawing.Size(153, 17);
            this.ignorewspace.TabIndex = 5;
            this.ignorewspace.Text = "Ignore Pattern Whitespace";
            this.ignorewspace.UseVisualStyleBackColor = true;
            // 
            // ignorecase
            // 
            this.ignorecase.AutoSize = true;
            this.ignorecase.Location = new System.Drawing.Point(119, 44);
            this.ignorecase.Name = "ignorecase";
            this.ignorecase.Size = new System.Drawing.Size(83, 17);
            this.ignorecase.TabIndex = 4;
            this.ignorecase.Text = "Ignore Case";
            this.ignorecase.UseVisualStyleBackColor = true;
            // 
            // explicitcap
            // 
            this.explicitcap.AutoSize = true;
            this.explicitcap.Location = new System.Drawing.Point(119, 20);
            this.explicitcap.Name = "explicitcap";
            this.explicitcap.Size = new System.Drawing.Size(99, 17);
            this.explicitcap.TabIndex = 3;
            this.explicitcap.Text = "Explicit Capture";
            this.explicitcap.UseVisualStyleBackColor = true;
            // 
            // ecma
            // 
            this.ecma.AutoSize = true;
            this.ecma.Location = new System.Drawing.Point(8, 68);
            this.ecma.Name = "ecma";
            this.ecma.Size = new System.Drawing.Size(86, 17);
            this.ecma.TabIndex = 2;
            this.ecma.Text = "ECMA Script";
            this.ecma.UseVisualStyleBackColor = true;
            // 
            // invariant
            // 
            this.invariant.AutoSize = true;
            this.invariant.Location = new System.Drawing.Point(8, 44);
            this.invariant.Name = "invariant";
            this.invariant.Size = new System.Drawing.Size(103, 17);
            this.invariant.TabIndex = 1;
            this.invariant.Text = "Culture Invariant";
            this.invariant.UseVisualStyleBackColor = true;
            // 
            // compiled
            // 
            this.compiled.AutoSize = true;
            this.compiled.Location = new System.Drawing.Point(8, 20);
            this.compiled.Name = "compiled";
            this.compiled.Size = new System.Drawing.Size(69, 17);
            this.compiled.TabIndex = 0;
            this.compiled.Text = "Compiled";
            this.compiled.UseVisualStyleBackColor = true;
            // 
            // findtext
            // 
            this.findtext.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.findtext.FormattingEnabled = true;
            this.findtext.Location = new System.Drawing.Point(45, 13);
            this.findtext.Name = "findtext";
            this.findtext.Size = new System.Drawing.Size(355, 21);
            this.findtext.TabIndex = 7;
            this.findtext.KeyDown += new System.Windows.Forms.KeyEventHandler(this.findtext_KeyDown);
            // 
            // FindAllDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(412, 360);
            this.Controls.Add(this.findtext);
            this.Controls.Add(this.regexoptions);
            this.Controls.Add(this.dofind);
            this.Controls.Add(this.options);
            this.Controls.Add(label2);
            this.Controls.Add(this.panel1);
            this.Controls.Add(label1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.MinimumSize = new System.Drawing.Size(420, 256);
            this.Name = "FindAllDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.Text = "Find In All Files";
            this.Shown += new System.EventHandler(this.FindAllDialog_Shown);
            this.options.ResumeLayout(false);
            this.options.PerformLayout();
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.regexoptions.ResumeLayout(false);
            this.regexoptions.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.RadioButton regexsearch;
        private System.Windows.Forms.RadioButton normalsearch;
        private System.Windows.Forms.CheckBox wordstart;
        private System.Windows.Forms.CheckBox wholeword;
        private System.Windows.Forms.CheckBox matchcase;
        private System.Windows.Forms.Button dofind;
        private System.Windows.Forms.GroupBox regexoptions;
        private System.Windows.Forms.CheckBox ecma;
        private System.Windows.Forms.CheckBox invariant;
        private System.Windows.Forms.CheckBox compiled;
        private System.Windows.Forms.GroupBox options;
        private System.Windows.Forms.CheckBox singleline;
        private System.Windows.Forms.CheckBox rtl;
        private System.Windows.Forms.CheckBox multi;
        private System.Windows.Forms.CheckBox ignorewspace;
        private System.Windows.Forms.CheckBox ignorecase;
        private System.Windows.Forms.CheckBox explicitcap;
        private System.Windows.Forms.ComboBox findtext;

    }
}