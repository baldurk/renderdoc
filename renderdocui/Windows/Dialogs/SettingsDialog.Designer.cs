namespace renderdocui.Windows.Dialogs
{
    partial class SettingsDialog
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
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
            System.Windows.Forms.GroupBox groupBox1;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SettingsDialog));
            System.Windows.Forms.Label label13;
            System.Windows.Forms.Label label6;
            System.Windows.Forms.Label label4;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label5;
            System.Windows.Forms.Label label7;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label11;
            System.Windows.Forms.Label label15;
            System.Windows.Forms.GroupBox groupBox2;
            System.Windows.Forms.Label label10;
            System.Windows.Forms.GroupBox groupBox3;
            System.Windows.Forms.Label label12;
            System.Windows.Forms.GroupBox groupBox4;
            System.Windows.Forms.Label label8;
            System.Windows.Forms.Label label9;
            TreelistView.TreeListColumn treeListColumn1 = new TreelistView.TreeListColumn("Section", "Section");
            this.settingsTabs = new renderdocui.Controls.TablessControl();
            this.generalTab = new System.Windows.Forms.TabPage();
            this.AllowGlobalHook = new System.Windows.Forms.CheckBox();
            this.Formatter_PosExp = new System.Windows.Forms.NumericUpDown();
            this.Formatter_NegExp = new System.Windows.Forms.NumericUpDown();
            this.rdcAssoc = new System.Windows.Forms.Button();
            this.capAssoc = new System.Windows.Forms.Button();
            this.Formatter_MaxFigures = new System.Windows.Forms.NumericUpDown();
            this.Formatter_MinFigures = new System.Windows.Forms.NumericUpDown();
            this.CheckUpdate_AllowChecks = new System.Windows.Forms.CheckBox();
            this.browseCaptureDirectory = new System.Windows.Forms.Button();
            this.Font_PreferMonospaced = new System.Windows.Forms.CheckBox();
            this.texViewTab = new System.Windows.Forms.TabPage();
            this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
            this.TextureViewer_ResetRange = new System.Windows.Forms.CheckBox();
            this.TextureViewer_PerTexSettings = new System.Windows.Forms.CheckBox();
            this.label14 = new System.Windows.Forms.Label();
            this.shadViewTab = new System.Windows.Forms.TabPage();
            this.tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
            this.ShaderViewer_FriendlyNaming = new System.Windows.Forms.CheckBox();
            this.eventTab = new System.Windows.Forms.TabPage();
            this.tableLayoutPanel5 = new System.Windows.Forms.TableLayoutPanel();
            this.EventBrowser_TimeUnit = new System.Windows.Forms.ComboBox();
            this.EventBrowser_HideEmpty = new System.Windows.Forms.CheckBox();
            this.pagesTree = new TreelistView.TreeListView();
            this.ok = new System.Windows.Forms.Button();
            this.toolTip = new System.Windows.Forms.ToolTip(this.components);
            this.browserCaptureDialog = new System.Windows.Forms.FolderBrowserDialog();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            groupBox1 = new System.Windows.Forms.GroupBox();
            tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            label13 = new System.Windows.Forms.Label();
            label6 = new System.Windows.Forms.Label();
            label4 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            label5 = new System.Windows.Forms.Label();
            label7 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label11 = new System.Windows.Forms.Label();
            label15 = new System.Windows.Forms.Label();
            groupBox2 = new System.Windows.Forms.GroupBox();
            label10 = new System.Windows.Forms.Label();
            groupBox3 = new System.Windows.Forms.GroupBox();
            label12 = new System.Windows.Forms.Label();
            groupBox4 = new System.Windows.Forms.GroupBox();
            label8 = new System.Windows.Forms.Label();
            label9 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            this.settingsTabs.SuspendLayout();
            this.generalTab.SuspendLayout();
            groupBox1.SuspendLayout();
            tableLayoutPanel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_PosExp)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_NegExp)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MaxFigures)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MinFigures)).BeginInit();
            this.texViewTab.SuspendLayout();
            groupBox2.SuspendLayout();
            this.tableLayoutPanel3.SuspendLayout();
            this.shadViewTab.SuspendLayout();
            groupBox3.SuspendLayout();
            this.tableLayoutPanel4.SuspendLayout();
            this.eventTab.SuspendLayout();
            groupBox4.SuspendLayout();
            this.tableLayoutPanel5.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pagesTree)).BeginInit();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.ColumnCount = 2;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 30F));
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 70F));
            tableLayoutPanel1.Controls.Add(this.settingsTabs, 1, 0);
            tableLayoutPanel1.Controls.Add(this.pagesTree, 0, 0);
            tableLayoutPanel1.Controls.Add(this.ok, 1, 1);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 2;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(580, 353);
            tableLayoutPanel1.TabIndex = 1;
            // 
            // settingsTabs
            // 
            this.settingsTabs.Alignment = System.Windows.Forms.TabAlignment.Left;
            this.settingsTabs.Controls.Add(this.generalTab);
            this.settingsTabs.Controls.Add(this.texViewTab);
            this.settingsTabs.Controls.Add(this.shadViewTab);
            this.settingsTabs.Controls.Add(this.eventTab);
            this.settingsTabs.Dock = System.Windows.Forms.DockStyle.Fill;
            this.settingsTabs.Location = new System.Drawing.Point(177, 3);
            this.settingsTabs.Multiline = true;
            this.settingsTabs.Name = "settingsTabs";
            this.settingsTabs.SelectedIndex = 0;
            this.settingsTabs.Size = new System.Drawing.Size(400, 318);
            this.settingsTabs.TabIndex = 0;
            // 
            // generalTab
            // 
            this.generalTab.Controls.Add(groupBox1);
            this.generalTab.Location = new System.Drawing.Point(23, 4);
            this.generalTab.Name = "generalTab";
            this.generalTab.Padding = new System.Windows.Forms.Padding(3);
            this.generalTab.Size = new System.Drawing.Size(373, 310);
            this.generalTab.TabIndex = 0;
            this.generalTab.Text = "General";
            this.generalTab.UseVisualStyleBackColor = true;
            // 
            // groupBox1
            // 
            groupBox1.Controls.Add(tableLayoutPanel2);
            groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox1.Location = new System.Drawing.Point(3, 3);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(367, 304);
            groupBox1.TabIndex = 0;
            groupBox1.TabStop = false;
            groupBox1.Text = "General";
            // 
            // tableLayoutPanel2
            // 
            tableLayoutPanel2.ColumnCount = 2;
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel2.Controls.Add(this.AllowGlobalHook, 1, 7);
            tableLayoutPanel2.Controls.Add(label13, 0, 7);
            tableLayoutPanel2.Controls.Add(this.Formatter_PosExp, 1, 5);
            tableLayoutPanel2.Controls.Add(this.Formatter_NegExp, 1, 4);
            tableLayoutPanel2.Controls.Add(label6, 0, 3);
            tableLayoutPanel2.Controls.Add(label4, 0, 2);
            tableLayoutPanel2.Controls.Add(this.rdcAssoc, 1, 0);
            tableLayoutPanel2.Controls.Add(label1, 0, 0);
            tableLayoutPanel2.Controls.Add(label2, 0, 1);
            tableLayoutPanel2.Controls.Add(this.capAssoc, 1, 1);
            tableLayoutPanel2.Controls.Add(label5, 0, 4);
            tableLayoutPanel2.Controls.Add(label7, 0, 5);
            tableLayoutPanel2.Controls.Add(this.Formatter_MaxFigures, 1, 3);
            tableLayoutPanel2.Controls.Add(this.Formatter_MinFigures, 1, 2);
            tableLayoutPanel2.Controls.Add(label3, 0, 8);
            tableLayoutPanel2.Controls.Add(this.CheckUpdate_AllowChecks, 1, 8);
            tableLayoutPanel2.Controls.Add(label11, 0, 6);
            tableLayoutPanel2.Controls.Add(this.browseCaptureDirectory, 1, 6);
            tableLayoutPanel2.Controls.Add(label15, 0, 9);
            tableLayoutPanel2.Controls.Add(this.Font_PreferMonospaced, 1, 9);
            tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel2.Location = new System.Drawing.Point(3, 16);
            tableLayoutPanel2.Name = "tableLayoutPanel2";
            tableLayoutPanel2.RowCount = 11;
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.Size = new System.Drawing.Size(361, 285);
            tableLayoutPanel2.TabIndex = 0;
            // 
            // AllowGlobalHook
            // 
            this.AllowGlobalHook.AutoSize = true;
            this.AllowGlobalHook.Checked = true;
            this.AllowGlobalHook.CheckState = System.Windows.Forms.CheckState.Checked;
            this.AllowGlobalHook.Location = new System.Drawing.Point(268, 194);
            this.AllowGlobalHook.Name = "AllowGlobalHook";
            this.AllowGlobalHook.Size = new System.Drawing.Size(15, 14);
            this.AllowGlobalHook.TabIndex = 16;
            this.toolTip.SetToolTip(this.AllowGlobalHook, resources.GetString("AllowGlobalHook.ToolTip"));
            this.AllowGlobalHook.UseVisualStyleBackColor = true;
            this.AllowGlobalHook.CheckedChanged += new System.EventHandler(this.AllowGlobalHook_CheckedChanged);
            // 
            // label13
            // 
            label13.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label13.AutoSize = true;
            label13.Location = new System.Drawing.Point(3, 191);
            label13.Name = "label13";
            label13.Size = new System.Drawing.Size(259, 20);
            label13.TabIndex = 15;
            label13.Text = "Allow global process hooking - be careful!";
            label13.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label13, resources.GetString("label13.ToolTip"));
            // 
            // Formatter_PosExp
            // 
            this.Formatter_PosExp.Location = new System.Drawing.Point(268, 139);
            this.Formatter_PosExp.Maximum = new decimal(new int[] {
            20,
            0,
            0,
            0});
            this.Formatter_PosExp.Name = "Formatter_PosExp";
            this.Formatter_PosExp.Size = new System.Drawing.Size(90, 20);
            this.Formatter_PosExp.TabIndex = 6;
            this.toolTip.SetToolTip(this.Formatter_PosExp, "Any numbers larger than this exponent will be displayed in scientific notation.\r\n" +
        "e.g. 1000 * 10 = 1e4");
            this.Formatter_PosExp.Value = new decimal(new int[] {
            7,
            0,
            0,
            0});
            this.Formatter_PosExp.ValueChanged += new System.EventHandler(this.formatter_ValueChanged);
            // 
            // Formatter_NegExp
            // 
            this.Formatter_NegExp.Location = new System.Drawing.Point(268, 113);
            this.Formatter_NegExp.Maximum = new decimal(new int[] {
            20,
            0,
            0,
            0});
            this.Formatter_NegExp.Name = "Formatter_NegExp";
            this.Formatter_NegExp.Size = new System.Drawing.Size(90, 20);
            this.Formatter_NegExp.TabIndex = 5;
            this.toolTip.SetToolTip(this.Formatter_NegExp, "Any numbers smaller than this exponent will be displayed in scientific notation.\r" +
        "\nE.g. a value of 3 means 0.005 / 10 = 5E-4");
            this.Formatter_NegExp.Value = new decimal(new int[] {
            5,
            0,
            0,
            0});
            this.Formatter_NegExp.ValueChanged += new System.EventHandler(this.formatter_ValueChanged);
            // 
            // label6
            // 
            label6.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label6.AutoSize = true;
            label6.Location = new System.Drawing.Point(3, 87);
            label6.Margin = new System.Windows.Forms.Padding(3);
            label6.Name = "label6";
            label6.Size = new System.Drawing.Size(259, 20);
            label6.TabIndex = 6;
            label6.Text = "Maximum significant figures on decimals";
            label6.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label6, "No more significant figures than this will be displayed on floats.\r\ne.g. a value " +
        "of 5 means 0.123456789 will display as 0.12345");
            // 
            // label4
            // 
            label4.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label4.AutoSize = true;
            label4.Location = new System.Drawing.Point(3, 61);
            label4.Margin = new System.Windows.Forms.Padding(3);
            label4.Name = "label4";
            label4.Size = new System.Drawing.Size(259, 20);
            label4.TabIndex = 4;
            label4.Text = "Minimum decimal places on float values";
            label4.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label4, "Decimals will display at least this many digits.\r\ne.g. a value of 2 means 0 will " +
        "display as 0.00, 0.5 as 0.50");
            // 
            // rdcAssoc
            // 
            this.rdcAssoc.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.rdcAssoc.Location = new System.Drawing.Point(268, 3);
            this.rdcAssoc.Name = "rdcAssoc";
            this.rdcAssoc.Size = new System.Drawing.Size(90, 23);
            this.rdcAssoc.TabIndex = 1;
            this.rdcAssoc.Text = "Associate .rdc";
            this.rdcAssoc.UseVisualStyleBackColor = true;
            this.rdcAssoc.Click += new System.EventHandler(this.rdcAssoc_Click);
            // 
            // label1
            // 
            label1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(3, 3);
            label1.Margin = new System.Windows.Forms.Padding(3);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(259, 23);
            label1.TabIndex = 0;
            label1.Text = "Associate .rdc with RenderDoc";
            label1.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // label2
            // 
            label2.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(3, 32);
            label2.Margin = new System.Windows.Forms.Padding(3);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(259, 23);
            label2.TabIndex = 2;
            label2.Text = "Associate .cap with RenderDoc";
            label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // capAssoc
            // 
            this.capAssoc.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.capAssoc.Location = new System.Drawing.Point(268, 32);
            this.capAssoc.Name = "capAssoc";
            this.capAssoc.Size = new System.Drawing.Size(90, 23);
            this.capAssoc.TabIndex = 2;
            this.capAssoc.Text = "Associate .cap";
            this.capAssoc.UseVisualStyleBackColor = true;
            this.capAssoc.Click += new System.EventHandler(this.capAssoc_Click);
            // 
            // label5
            // 
            label5.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label5.AutoSize = true;
            label5.Location = new System.Drawing.Point(3, 113);
            label5.Margin = new System.Windows.Forms.Padding(3);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(259, 20);
            label5.TabIndex = 5;
            label5.Text = "Negative exponential cutoff value";
            label5.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label5, "Any numbers smaller than this exponent will be displayed in scientific notation.\r" +
        "\nE.g. a value of 3 means 0.005 / 10 = 5E-4\r\n");
            // 
            // label7
            // 
            label7.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label7.AutoSize = true;
            label7.Location = new System.Drawing.Point(3, 139);
            label7.Margin = new System.Windows.Forms.Padding(3);
            label7.Name = "label7";
            label7.Size = new System.Drawing.Size(259, 20);
            label7.TabIndex = 7;
            label7.Text = "Positive exponential cutoff value";
            label7.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label7, "Any numbers larger than this exponent will be displayed in scientific notation.\r\n" +
        "e.g. 1000 * 10 = 1e4");
            // 
            // Formatter_MaxFigures
            // 
            this.Formatter_MaxFigures.Location = new System.Drawing.Point(268, 87);
            this.Formatter_MaxFigures.Maximum = new decimal(new int[] {
            29,
            0,
            0,
            0});
            this.Formatter_MaxFigures.Minimum = new decimal(new int[] {
            2,
            0,
            0,
            0});
            this.Formatter_MaxFigures.Name = "Formatter_MaxFigures";
            this.Formatter_MaxFigures.Size = new System.Drawing.Size(90, 20);
            this.Formatter_MaxFigures.TabIndex = 4;
            this.toolTip.SetToolTip(this.Formatter_MaxFigures, "No more significant figures than this will be displayed on floats.\r\ne.g. a value " +
        "of 5 means 0.123456789 will display as 0.12345\r\n");
            this.Formatter_MaxFigures.Value = new decimal(new int[] {
            5,
            0,
            0,
            0});
            this.Formatter_MaxFigures.ValueChanged += new System.EventHandler(this.formatter_ValueChanged);
            // 
            // Formatter_MinFigures
            // 
            this.Formatter_MinFigures.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.Formatter_MinFigures.Location = new System.Drawing.Point(268, 61);
            this.Formatter_MinFigures.Maximum = new decimal(new int[] {
            29,
            0,
            0,
            0});
            this.Formatter_MinFigures.Name = "Formatter_MinFigures";
            this.Formatter_MinFigures.Size = new System.Drawing.Size(90, 20);
            this.Formatter_MinFigures.TabIndex = 3;
            this.toolTip.SetToolTip(this.Formatter_MinFigures, "Decimals will display at least this many digits.\r\ne.g. a value of 2 means 0 will " +
        "display as 0.00, 0.5 as 0.50\r\n");
            this.Formatter_MinFigures.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
            this.Formatter_MinFigures.ValueChanged += new System.EventHandler(this.formatter_ValueChanged);
            // 
            // label3
            // 
            label3.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(3, 211);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(259, 20);
            label3.TabIndex = 12;
            label3.Text = "Allow periodic anonymous update checks";
            label3.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label3, "Allows RenderDoc to phone home to http://renderdoc.org to anonymously check for n" +
        "ew versions.");
            // 
            // CheckUpdate_AllowChecks
            // 
            this.CheckUpdate_AllowChecks.AutoSize = true;
            this.CheckUpdate_AllowChecks.Checked = true;
            this.CheckUpdate_AllowChecks.CheckState = System.Windows.Forms.CheckState.Checked;
            this.CheckUpdate_AllowChecks.Location = new System.Drawing.Point(268, 214);
            this.CheckUpdate_AllowChecks.Name = "CheckUpdate_AllowChecks";
            this.CheckUpdate_AllowChecks.Size = new System.Drawing.Size(15, 14);
            this.CheckUpdate_AllowChecks.TabIndex = 8;
            this.toolTip.SetToolTip(this.CheckUpdate_AllowChecks, "Allows RenderDoc to phone home to http://renderdoc.org to anonymously check for n" +
        "ew versions.");
            this.CheckUpdate_AllowChecks.UseVisualStyleBackColor = true;
            this.CheckUpdate_AllowChecks.CheckedChanged += new System.EventHandler(this.CheckUpdate_AllowChecks_CheckedChanged);
            // 
            // label11
            // 
            label11.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label11.AutoSize = true;
            label11.Location = new System.Drawing.Point(3, 162);
            label11.Name = "label11";
            label11.Size = new System.Drawing.Size(259, 29);
            label11.TabIndex = 14;
            label11.Text = "Directory for temporary capture files";
            label11.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label11, "Changes the directory where capture files are saved after being created, until sa" +
        "ved manually or deleted.\r\n\r\nDefaults to %TEMP%.");
            // 
            // browseCaptureDirectory
            // 
            this.browseCaptureDirectory.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.browseCaptureDirectory.Location = new System.Drawing.Point(268, 165);
            this.browseCaptureDirectory.Name = "browseCaptureDirectory";
            this.browseCaptureDirectory.Size = new System.Drawing.Size(90, 23);
            this.browseCaptureDirectory.TabIndex = 7;
            this.browseCaptureDirectory.Text = "Browse";
            this.toolTip.SetToolTip(this.browseCaptureDirectory, "Changes the directory where capture files are saved after being created, until sa" +
        "ved manually or deleted.\r\n\r\nDefaults to %TEMP%.");
            this.browseCaptureDirectory.UseVisualStyleBackColor = true;
            this.browseCaptureDirectory.Click += new System.EventHandler(this.browseCaptureDirectory_Click);
            // 
            // label15
            // 
            label15.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label15.AutoSize = true;
            label15.Location = new System.Drawing.Point(3, 231);
            label15.Name = "label15";
            label15.Size = new System.Drawing.Size(259, 20);
            label15.TabIndex = 17;
            label15.Text = "Prefer monospaced fonts in UI (restart required)";
            label15.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label15, "Wherever possible a monospaced font will be used instead of the default font");
            // 
            // Font_PreferMonospaced
            // 
            this.Font_PreferMonospaced.AutoSize = true;
            this.Font_PreferMonospaced.Checked = true;
            this.Font_PreferMonospaced.CheckState = System.Windows.Forms.CheckState.Checked;
            this.Font_PreferMonospaced.Location = new System.Drawing.Point(268, 234);
            this.Font_PreferMonospaced.Name = "Font_PreferMonospaced";
            this.Font_PreferMonospaced.Size = new System.Drawing.Size(15, 14);
            this.Font_PreferMonospaced.TabIndex = 18;
            this.toolTip.SetToolTip(this.Font_PreferMonospaced, "Wherever possible a monospaced font will be used instead of the default font");
            this.Font_PreferMonospaced.UseVisualStyleBackColor = true;
            this.Font_PreferMonospaced.CheckedChanged += new System.EventHandler(this.Font_PreferMonospaced_CheckedChanged);
            // 
            // texViewTab
            // 
            this.texViewTab.Controls.Add(groupBox2);
            this.texViewTab.Location = new System.Drawing.Point(23, 4);
            this.texViewTab.Name = "texViewTab";
            this.texViewTab.Padding = new System.Windows.Forms.Padding(3);
            this.texViewTab.Size = new System.Drawing.Size(373, 310);
            this.texViewTab.TabIndex = 1;
            this.texViewTab.Text = "Texture Viewer";
            this.texViewTab.UseVisualStyleBackColor = true;
            // 
            // groupBox2
            // 
            groupBox2.Controls.Add(this.tableLayoutPanel3);
            groupBox2.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox2.Location = new System.Drawing.Point(3, 3);
            groupBox2.Name = "groupBox2";
            groupBox2.Size = new System.Drawing.Size(367, 304);
            groupBox2.TabIndex = 0;
            groupBox2.TabStop = false;
            groupBox2.Text = "Texture Viewer";
            // 
            // tableLayoutPanel3
            // 
            this.tableLayoutPanel3.ColumnCount = 2;
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            this.tableLayoutPanel3.Controls.Add(this.TextureViewer_ResetRange, 1, 0);
            this.tableLayoutPanel3.Controls.Add(this.TextureViewer_PerTexSettings, 1, 1);
            this.tableLayoutPanel3.Controls.Add(label10, 0, 0);
            this.tableLayoutPanel3.Controls.Add(this.label14, 0, 1);
            this.tableLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel3.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel3.Name = "tableLayoutPanel3";
            this.tableLayoutPanel3.RowCount = 3;
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.Size = new System.Drawing.Size(361, 285);
            this.tableLayoutPanel3.TabIndex = 0;
            // 
            // TextureViewer_ResetRange
            // 
            this.TextureViewer_ResetRange.AutoSize = true;
            this.TextureViewer_ResetRange.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TextureViewer_ResetRange.Location = new System.Drawing.Point(291, 3);
            this.TextureViewer_ResetRange.Name = "TextureViewer_ResetRange";
            this.TextureViewer_ResetRange.Size = new System.Drawing.Size(67, 14);
            this.TextureViewer_ResetRange.TabIndex = 20;
            this.toolTip.SetToolTip(this.TextureViewer_ResetRange, "Reset visible range when changing event or texture");
            this.TextureViewer_ResetRange.UseVisualStyleBackColor = true;
            this.TextureViewer_ResetRange.CheckedChanged += new System.EventHandler(this.TextureViewer_ResetRange_CheckedChanged);
            // 
            // TextureViewer_PerTexSettings
            // 
            this.TextureViewer_PerTexSettings.AutoSize = true;
            this.TextureViewer_PerTexSettings.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TextureViewer_PerTexSettings.Location = new System.Drawing.Point(291, 23);
            this.TextureViewer_PerTexSettings.Name = "TextureViewer_PerTexSettings";
            this.TextureViewer_PerTexSettings.Size = new System.Drawing.Size(67, 14);
            this.TextureViewer_PerTexSettings.TabIndex = 20;
            this.toolTip.SetToolTip(this.TextureViewer_PerTexSettings, "The visible channels (RGBA) and selected mip/slice are remembered and restored pe" +
        "r-texture.");
            this.TextureViewer_PerTexSettings.UseVisualStyleBackColor = true;
            this.TextureViewer_PerTexSettings.CheckedChanged += new System.EventHandler(this.TextureViewer_PerTexSettings_CheckedChanged);
            // 
            // label10
            // 
            label10.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label10.AutoSize = true;
            label10.Location = new System.Drawing.Point(3, 3);
            label10.Margin = new System.Windows.Forms.Padding(3);
            label10.Name = "label10";
            label10.Size = new System.Drawing.Size(282, 14);
            label10.TabIndex = 4;
            label10.Text = "Reset Range on changing selection";
            label10.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // label14
            // 
            this.label14.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label14.AutoSize = true;
            this.label14.Location = new System.Drawing.Point(3, 23);
            this.label14.Margin = new System.Windows.Forms.Padding(3);
            this.label14.Name = "label14";
            this.label14.Size = new System.Drawing.Size(282, 14);
            this.label14.TabIndex = 4;
            this.label14.Text = "Visible channels && mip/slice saved per-texture";
            this.label14.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // shadViewTab
            // 
            this.shadViewTab.Controls.Add(groupBox3);
            this.shadViewTab.Location = new System.Drawing.Point(23, 4);
            this.shadViewTab.Name = "shadViewTab";
            this.shadViewTab.Padding = new System.Windows.Forms.Padding(3);
            this.shadViewTab.Size = new System.Drawing.Size(373, 310);
            this.shadViewTab.TabIndex = 2;
            this.shadViewTab.Text = "Shader Viewer";
            this.shadViewTab.UseVisualStyleBackColor = true;
            // 
            // groupBox3
            // 
            groupBox3.Controls.Add(this.tableLayoutPanel4);
            groupBox3.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox3.Location = new System.Drawing.Point(3, 3);
            groupBox3.Name = "groupBox3";
            groupBox3.Size = new System.Drawing.Size(367, 304);
            groupBox3.TabIndex = 0;
            groupBox3.TabStop = false;
            groupBox3.Text = "Shader Viewer";
            // 
            // tableLayoutPanel4
            // 
            this.tableLayoutPanel4.ColumnCount = 2;
            this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            this.tableLayoutPanel4.Controls.Add(this.ShaderViewer_FriendlyNaming, 1, 0);
            this.tableLayoutPanel4.Controls.Add(label12, 0, 0);
            this.tableLayoutPanel4.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel4.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel4.Name = "tableLayoutPanel4";
            this.tableLayoutPanel4.RowCount = 2;
            this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel4.Size = new System.Drawing.Size(361, 285);
            this.tableLayoutPanel4.TabIndex = 1;
            // 
            // ShaderViewer_FriendlyNaming
            // 
            this.ShaderViewer_FriendlyNaming.AutoSize = true;
            this.ShaderViewer_FriendlyNaming.Location = new System.Drawing.Point(291, 3);
            this.ShaderViewer_FriendlyNaming.Name = "ShaderViewer_FriendlyNaming";
            this.ShaderViewer_FriendlyNaming.Size = new System.Drawing.Size(15, 14);
            this.ShaderViewer_FriendlyNaming.TabIndex = 40;
            this.toolTip.SetToolTip(this.ShaderViewer_FriendlyNaming, "In disassembly view, rename constant registers to their names from shader reflect" +
        "ion data");
            this.ShaderViewer_FriendlyNaming.UseVisualStyleBackColor = true;
            this.ShaderViewer_FriendlyNaming.CheckedChanged += new System.EventHandler(this.friendlyRegName_CheckedChanged);
            // 
            // label12
            // 
            label12.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label12.AutoSize = true;
            label12.Location = new System.Drawing.Point(3, 3);
            label12.Margin = new System.Windows.Forms.Padding(3);
            label12.Name = "label12";
            label12.Size = new System.Drawing.Size(282, 14);
            label12.TabIndex = 6;
            label12.Text = "Rename disassembly registers";
            label12.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // eventTab
            // 
            this.eventTab.Controls.Add(groupBox4);
            this.eventTab.Location = new System.Drawing.Point(23, 4);
            this.eventTab.Name = "eventTab";
            this.eventTab.Padding = new System.Windows.Forms.Padding(3);
            this.eventTab.Size = new System.Drawing.Size(373, 310);
            this.eventTab.TabIndex = 3;
            this.eventTab.Text = "Event Browser";
            this.eventTab.UseVisualStyleBackColor = true;
            // 
            // groupBox4
            // 
            groupBox4.Controls.Add(this.tableLayoutPanel5);
            groupBox4.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox4.Location = new System.Drawing.Point(3, 3);
            groupBox4.Name = "groupBox4";
            groupBox4.Size = new System.Drawing.Size(367, 304);
            groupBox4.TabIndex = 1;
            groupBox4.TabStop = false;
            groupBox4.Text = "Event Browser";
            // 
            // tableLayoutPanel5
            // 
            this.tableLayoutPanel5.ColumnCount = 2;
            this.tableLayoutPanel5.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            this.tableLayoutPanel5.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            this.tableLayoutPanel5.Controls.Add(label8, 0, 0);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_TimeUnit, 1, 0);
            this.tableLayoutPanel5.Controls.Add(label9, 0, 1);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_HideEmpty, 1, 1);
            this.tableLayoutPanel5.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel5.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel5.Name = "tableLayoutPanel5";
            this.tableLayoutPanel5.RowCount = 3;
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel5.Size = new System.Drawing.Size(361, 285);
            this.tableLayoutPanel5.TabIndex = 0;
            // 
            // label8
            // 
            label8.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label8.AutoSize = true;
            label8.Location = new System.Drawing.Point(3, 3);
            label8.Margin = new System.Windows.Forms.Padding(3);
            label8.Name = "label8";
            label8.Size = new System.Drawing.Size(282, 21);
            label8.TabIndex = 3;
            label8.Text = "Time unit used for event browser timings";
            label8.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_TimeUnit
            // 
            this.EventBrowser_TimeUnit.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.EventBrowser_TimeUnit.FormattingEnabled = true;
            this.EventBrowser_TimeUnit.Location = new System.Drawing.Point(291, 3);
            this.EventBrowser_TimeUnit.Name = "EventBrowser_TimeUnit";
            this.EventBrowser_TimeUnit.Size = new System.Drawing.Size(67, 21);
            this.EventBrowser_TimeUnit.TabIndex = 50;
            this.toolTip.SetToolTip(this.EventBrowser_TimeUnit, "The time unit to use when displaying the duration column in the event browser");
            this.EventBrowser_TimeUnit.SelectionChangeCommitted += new System.EventHandler(this.EventBrowser_TimeUnit_SelectionChangeCommitted);
            // 
            // label9
            // 
            label9.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label9.AutoSize = true;
            label9.Location = new System.Drawing.Point(3, 30);
            label9.Margin = new System.Windows.Forms.Padding(3);
            label9.Name = "label9";
            label9.Size = new System.Drawing.Size(282, 14);
            label9.TabIndex = 7;
            label9.Text = "Hide empty marker sections (requires log reload)";
            label9.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_HideEmpty
            // 
            this.EventBrowser_HideEmpty.AutoSize = true;
            this.EventBrowser_HideEmpty.Location = new System.Drawing.Point(291, 30);
            this.EventBrowser_HideEmpty.Name = "EventBrowser_HideEmpty";
            this.EventBrowser_HideEmpty.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_HideEmpty.TabIndex = 51;
            this.toolTip.SetToolTip(this.EventBrowser_HideEmpty, "In the Event Browser and Timeline Bar, marker sections that contain no API calls " +
        "or drawcalls will be completely removed");
            this.EventBrowser_HideEmpty.UseVisualStyleBackColor = true;
            this.EventBrowser_HideEmpty.CheckedChanged += new System.EventHandler(this.EventBrowser_HideEmpty_CheckedChanged);
            // 
            // pagesTree
            // 
            this.pagesTree.AlwaysDisplayVScroll = true;
            treeListColumn1.AutoSize = true;
            treeListColumn1.AutoSizeMinSize = 0;
            treeListColumn1.Width = 50;
            this.pagesTree.Columns.AddRange(new TreelistView.TreeListColumn[] {
            treeListColumn1});
            this.pagesTree.ColumnsOptions.HeaderHeight = 1;
            this.pagesTree.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.pagesTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this.pagesTree.Location = new System.Drawing.Point(3, 3);
            this.pagesTree.MultiSelect = false;
            this.pagesTree.Name = "pagesTree";
            this.pagesTree.RowOptions.ShowHeader = false;
            this.pagesTree.Size = new System.Drawing.Size(168, 318);
            this.pagesTree.TabIndex = 0;
            this.pagesTree.ViewOptions.ShowGridLines = false;
            this.pagesTree.ViewOptions.ShowLine = false;
            this.pagesTree.ViewOptions.ShowPlusMinus = false;
            this.pagesTree.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.pagesTree_AfterSelect);
            // 
            // ok
            // 
            this.ok.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ok.Location = new System.Drawing.Point(502, 327);
            this.ok.Name = "ok";
            this.ok.Size = new System.Drawing.Size(75, 23);
            this.ok.TabIndex = 100;
            this.ok.Text = "OK";
            this.ok.UseVisualStyleBackColor = true;
            this.ok.Click += new System.EventHandler(this.ok_Click);
            // 
            // browserCaptureDialog
            // 
            this.browserCaptureDialog.RootFolder = System.Environment.SpecialFolder.MyComputer;
            // 
            // SettingsDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(580, 353);
            this.Controls.Add(tableLayoutPanel1);
            this.MinimumSize = new System.Drawing.Size(500, 300);
            this.Name = "SettingsDialog";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Settings";
            tableLayoutPanel1.ResumeLayout(false);
            this.settingsTabs.ResumeLayout(false);
            this.generalTab.ResumeLayout(false);
            groupBox1.ResumeLayout(false);
            tableLayoutPanel2.ResumeLayout(false);
            tableLayoutPanel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_PosExp)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_NegExp)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MaxFigures)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MinFigures)).EndInit();
            this.texViewTab.ResumeLayout(false);
            groupBox2.ResumeLayout(false);
            this.tableLayoutPanel3.ResumeLayout(false);
            this.tableLayoutPanel3.PerformLayout();
            this.shadViewTab.ResumeLayout(false);
            groupBox3.ResumeLayout(false);
            this.tableLayoutPanel4.ResumeLayout(false);
            this.tableLayoutPanel4.PerformLayout();
            this.eventTab.ResumeLayout(false);
            groupBox4.ResumeLayout(false);
            this.tableLayoutPanel5.ResumeLayout(false);
            this.tableLayoutPanel5.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pagesTree)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private renderdocui.Controls.TablessControl settingsTabs;
        private System.Windows.Forms.TabPage generalTab;
        private System.Windows.Forms.TabPage texViewTab;
        private TreelistView.TreeListView pagesTree;
        private System.Windows.Forms.Button ok;
        private System.Windows.Forms.Button rdcAssoc;
        private System.Windows.Forms.Button capAssoc;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
        private System.Windows.Forms.CheckBox TextureViewer_ResetRange;
        private System.Windows.Forms.CheckBox TextureViewer_PerTexSettings;
        private System.Windows.Forms.TabPage shadViewTab;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
        private System.Windows.Forms.CheckBox ShaderViewer_FriendlyNaming;
        private System.Windows.Forms.ToolTip toolTip;
        private System.Windows.Forms.NumericUpDown Formatter_PosExp;
        private System.Windows.Forms.NumericUpDown Formatter_NegExp;
        private System.Windows.Forms.NumericUpDown Formatter_MaxFigures;
        private System.Windows.Forms.NumericUpDown Formatter_MinFigures;
        private System.Windows.Forms.CheckBox CheckUpdate_AllowChecks;
        private System.Windows.Forms.TabPage eventTab;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel5;
        private System.Windows.Forms.ComboBox EventBrowser_TimeUnit;
        private System.Windows.Forms.CheckBox EventBrowser_HideEmpty;
        private System.Windows.Forms.Button browseCaptureDirectory;
        private System.Windows.Forms.FolderBrowserDialog browserCaptureDialog;
        private System.Windows.Forms.CheckBox AllowGlobalHook;
        private System.Windows.Forms.CheckBox Font_PreferMonospaced;
        private System.Windows.Forms.Label label14;
    }
}