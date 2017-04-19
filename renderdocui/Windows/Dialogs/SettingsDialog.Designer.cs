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
            System.Windows.Forms.Label label20;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SettingsDialog));
            System.Windows.Forms.Label label13;
            System.Windows.Forms.Label label6;
            System.Windows.Forms.Label label4;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label5;
            System.Windows.Forms.Label label7;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label15;
            System.Windows.Forms.Label label18;
            System.Windows.Forms.Label label11;
            System.Windows.Forms.TableLayoutPanel tableLayoutPanel6;
            System.Windows.Forms.Label label19;
            System.Windows.Forms.GroupBox groupBox2;
            System.Windows.Forms.Label label10;
            System.Windows.Forms.GroupBox groupBox3;
            System.Windows.Forms.Label label12;
            System.Windows.Forms.GroupBox groupBox4;
            System.Windows.Forms.Label label8;
            System.Windows.Forms.Label label9;
            System.Windows.Forms.Label label16;
            System.Windows.Forms.Label label17;
            System.Windows.Forms.Label label26;
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
            this.browseTempCaptureDirectory = new System.Windows.Forms.Button();
            this.Font_PreferMonospaced = new System.Windows.Forms.CheckBox();
            this.AlwaysReplayLocally = new System.Windows.Forms.CheckBox();
            this.browseSaveCaptureDirectory = new System.Windows.Forms.Button();
            this.saveDirectory = new System.Windows.Forms.TextBox();
            this.tempDirectory = new System.Windows.Forms.TextBox();
            this.corePage = new System.Windows.Forms.TabPage();
            this.groupBox5 = new System.Windows.Forms.GroupBox();
            this.chooseSearchPaths = new System.Windows.Forms.Button();
            this.texViewTab = new System.Windows.Forms.TabPage();
            this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
            this.TextureViewer_ResetRange = new System.Windows.Forms.CheckBox();
            this.TextureViewer_PerTexSettings = new System.Windows.Forms.CheckBox();
            this.label14 = new System.Windows.Forms.Label();
            this.shadViewTab = new System.Windows.Forms.TabPage();
            this.groupBox6 = new System.Windows.Forms.GroupBox();
            this.tableLayoutPanel7 = new System.Windows.Forms.TableLayoutPanel();
            this.label23 = new System.Windows.Forms.Label();
            this.externalDisassemblerEnabledCheckbox = new System.Windows.Forms.CheckBox();
            this.label21 = new System.Windows.Forms.Label();
            this.label22 = new System.Windows.Forms.Label();
            this.externalDisassemblePath = new System.Windows.Forms.TextBox();
            this.browseExtDisasemble = new System.Windows.Forms.Button();
            this.externalDisassemblerArgs = new System.Windows.Forms.TextBox();
            this.label24 = new System.Windows.Forms.Label();
            this.tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
            this.ShaderViewer_FriendlyNaming = new System.Windows.Forms.CheckBox();
            this.eventTab = new System.Windows.Forms.TabPage();
            this.tableLayoutPanel5 = new System.Windows.Forms.TableLayoutPanel();
            this.EventBrowser_TimeUnit = new System.Windows.Forms.ComboBox();
            this.EventBrowser_HideEmpty = new System.Windows.Forms.CheckBox();
            this.label35 = new System.Windows.Forms.Label();
            this.EventBrowser_HideAPICalls = new System.Windows.Forms.CheckBox();
            this.EventBrowser_ApplyColours = new System.Windows.Forms.CheckBox();
            this.EventBrowser_ColourEventRow = new System.Windows.Forms.CheckBox();
            this.EventBrowser_AddFake = new System.Windows.Forms.CheckBox();
            this.androidTab = new System.Windows.Forms.TabPage();
            this.groupBox7 = new System.Windows.Forms.GroupBox();
            this.tableLayoutPanel8 = new System.Windows.Forms.TableLayoutPanel();
            this.label25 = new System.Windows.Forms.Label();
            this.maxConnectTimeout = new System.Windows.Forms.NumericUpDown();
            this.label36 = new System.Windows.Forms.Label();
            this.adbPath = new System.Windows.Forms.TextBox();
            this.browseAdbPath = new System.Windows.Forms.Button();
            this.pagesTree = new TreelistView.TreeListView();
            this.ok = new System.Windows.Forms.Button();
            this.toolTip = new System.Windows.Forms.ToolTip(this.components);
            this.browserCaptureDialog = new System.Windows.Forms.FolderBrowserDialog();
            this.browseExtDisassembleDialog = new System.Windows.Forms.OpenFileDialog();
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            groupBox1 = new System.Windows.Forms.GroupBox();
            tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            label20 = new System.Windows.Forms.Label();
            label13 = new System.Windows.Forms.Label();
            label6 = new System.Windows.Forms.Label();
            label4 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            label5 = new System.Windows.Forms.Label();
            label7 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label15 = new System.Windows.Forms.Label();
            label18 = new System.Windows.Forms.Label();
            label11 = new System.Windows.Forms.Label();
            tableLayoutPanel6 = new System.Windows.Forms.TableLayoutPanel();
            label19 = new System.Windows.Forms.Label();
            groupBox2 = new System.Windows.Forms.GroupBox();
            label10 = new System.Windows.Forms.Label();
            groupBox3 = new System.Windows.Forms.GroupBox();
            label12 = new System.Windows.Forms.Label();
            groupBox4 = new System.Windows.Forms.GroupBox();
            label8 = new System.Windows.Forms.Label();
            label9 = new System.Windows.Forms.Label();
            label16 = new System.Windows.Forms.Label();
            label17 = new System.Windows.Forms.Label();
            label26 = new System.Windows.Forms.Label();
            tableLayoutPanel1.SuspendLayout();
            this.settingsTabs.SuspendLayout();
            this.generalTab.SuspendLayout();
            groupBox1.SuspendLayout();
            tableLayoutPanel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_PosExp)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_NegExp)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MaxFigures)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.Formatter_MinFigures)).BeginInit();
            this.corePage.SuspendLayout();
            this.groupBox5.SuspendLayout();
            tableLayoutPanel6.SuspendLayout();
            this.texViewTab.SuspendLayout();
            groupBox2.SuspendLayout();
            this.tableLayoutPanel3.SuspendLayout();
            this.shadViewTab.SuspendLayout();
            groupBox3.SuspendLayout();
            this.groupBox6.SuspendLayout();
            this.tableLayoutPanel7.SuspendLayout();
            this.tableLayoutPanel4.SuspendLayout();
            this.eventTab.SuspendLayout();
            groupBox4.SuspendLayout();
            this.tableLayoutPanel5.SuspendLayout();
            this.androidTab.SuspendLayout();
            this.groupBox7.SuspendLayout();
            this.tableLayoutPanel8.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.maxConnectTimeout)).BeginInit();
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
            tableLayoutPanel1.Size = new System.Drawing.Size(537, 451);
            tableLayoutPanel1.TabIndex = 1;
            // 
            // settingsTabs
            // 
            this.settingsTabs.Alignment = System.Windows.Forms.TabAlignment.Left;
            this.settingsTabs.Controls.Add(this.generalTab);
            this.settingsTabs.Controls.Add(this.corePage);
            this.settingsTabs.Controls.Add(this.texViewTab);
            this.settingsTabs.Controls.Add(this.shadViewTab);
            this.settingsTabs.Controls.Add(this.eventTab);
            this.settingsTabs.Controls.Add(this.androidTab);
            this.settingsTabs.Dock = System.Windows.Forms.DockStyle.Fill;
            this.settingsTabs.Location = new System.Drawing.Point(164, 3);
            this.settingsTabs.Multiline = true;
            this.settingsTabs.Name = "settingsTabs";
            this.settingsTabs.SelectedIndex = 0;
            this.settingsTabs.Size = new System.Drawing.Size(370, 416);
            this.settingsTabs.TabIndex = 0;
            // 
            // generalTab
            // 
            this.generalTab.Controls.Add(groupBox1);
            this.generalTab.Location = new System.Drawing.Point(23, 4);
            this.generalTab.Name = "generalTab";
            this.generalTab.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.generalTab.Size = new System.Drawing.Size(343, 408);
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
            groupBox1.Size = new System.Drawing.Size(337, 402);
            groupBox1.TabIndex = 0;
            groupBox1.TabStop = false;
            groupBox1.Text = "General";
            // 
            // tableLayoutPanel2
            // 
            tableLayoutPanel2.ColumnCount = 2;
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel2.Controls.Add(label20, 0, 8);
            tableLayoutPanel2.Controls.Add(this.AllowGlobalHook, 1, 10);
            tableLayoutPanel2.Controls.Add(label13, 0, 10);
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
            tableLayoutPanel2.Controls.Add(label3, 0, 11);
            tableLayoutPanel2.Controls.Add(this.CheckUpdate_AllowChecks, 1, 11);
            tableLayoutPanel2.Controls.Add(this.browseTempCaptureDirectory, 1, 7);
            tableLayoutPanel2.Controls.Add(label15, 0, 12);
            tableLayoutPanel2.Controls.Add(this.Font_PreferMonospaced, 1, 12);
            tableLayoutPanel2.Controls.Add(label18, 0, 13);
            tableLayoutPanel2.Controls.Add(this.AlwaysReplayLocally, 1, 13);
            tableLayoutPanel2.Controls.Add(this.browseSaveCaptureDirectory, 1, 9);
            tableLayoutPanel2.Controls.Add(this.saveDirectory, 0, 9);
            tableLayoutPanel2.Controls.Add(label11, 0, 6);
            tableLayoutPanel2.Controls.Add(this.tempDirectory, 0, 7);
            tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel2.Location = new System.Drawing.Point(3, 16);
            tableLayoutPanel2.Name = "tableLayoutPanel2";
            tableLayoutPanel2.RowCount = 15;
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel2.Size = new System.Drawing.Size(331, 383);
            tableLayoutPanel2.TabIndex = 0;
            // 
            // label20
            // 
            label20.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label20.AutoSize = true;
            label20.Location = new System.Drawing.Point(3, 211);
            label20.MinimumSize = new System.Drawing.Size(0, 20);
            label20.Name = "label20";
            label20.Size = new System.Drawing.Size(228, 20);
            label20.TabIndex = 21;
            label20.Text = "Default save directory for captures";
            label20.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label20, "Changes the default directory for the save dialog when saving capture files.\r\n\r\nD" +
        "efaults to blank, which follows system default behaviour.");
            // 
            // AllowGlobalHook
            // 
            this.AllowGlobalHook.AutoSize = true;
            this.AllowGlobalHook.Checked = true;
            this.AllowGlobalHook.CheckState = System.Windows.Forms.CheckState.Checked;
            this.AllowGlobalHook.Location = new System.Drawing.Point(237, 263);
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
            label13.Location = new System.Drawing.Point(3, 260);
            label13.Name = "label13";
            label13.Size = new System.Drawing.Size(228, 20);
            label13.TabIndex = 15;
            label13.Text = "Allow global process hooking - be careful!";
            label13.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label13, resources.GetString("label13.ToolTip"));
            // 
            // Formatter_PosExp
            // 
            this.Formatter_PosExp.Location = new System.Drawing.Point(237, 139);
            this.Formatter_PosExp.Maximum = new decimal(new int[] {
            20,
            0,
            0,
            0});
            this.Formatter_PosExp.Name = "Formatter_PosExp";
            this.Formatter_PosExp.Size = new System.Drawing.Size(91, 20);
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
            this.Formatter_NegExp.Location = new System.Drawing.Point(237, 113);
            this.Formatter_NegExp.Maximum = new decimal(new int[] {
            20,
            0,
            0,
            0});
            this.Formatter_NegExp.Name = "Formatter_NegExp";
            this.Formatter_NegExp.Size = new System.Drawing.Size(91, 20);
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
            label6.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label6.Name = "label6";
            label6.Size = new System.Drawing.Size(228, 20);
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
            label4.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label4.Name = "label4";
            label4.Size = new System.Drawing.Size(228, 20);
            label4.TabIndex = 4;
            label4.Text = "Minimum decimal places on float values";
            label4.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label4, "Decimals will display at least this many digits.\r\ne.g. a value of 2 means 0 will " +
        "display as 0.00, 0.5 as 0.50");
            // 
            // rdcAssoc
            // 
            this.rdcAssoc.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.rdcAssoc.Location = new System.Drawing.Point(238, 3);
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
            label1.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(228, 23);
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
            label2.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(228, 23);
            label2.TabIndex = 2;
            label2.Text = "Associate .cap with RenderDoc";
            label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // capAssoc
            // 
            this.capAssoc.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.capAssoc.Location = new System.Drawing.Point(238, 32);
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
            label5.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(228, 20);
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
            label7.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label7.Name = "label7";
            label7.Size = new System.Drawing.Size(228, 20);
            label7.TabIndex = 7;
            label7.Text = "Positive exponential cutoff value";
            label7.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label7, "Any numbers larger than this exponent will be displayed in scientific notation.\r\n" +
        "e.g. 1000 * 10 = 1e4");
            // 
            // Formatter_MaxFigures
            // 
            this.Formatter_MaxFigures.Location = new System.Drawing.Point(237, 87);
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
            this.Formatter_MaxFigures.Size = new System.Drawing.Size(91, 20);
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
            this.Formatter_MinFigures.Location = new System.Drawing.Point(237, 61);
            this.Formatter_MinFigures.Maximum = new decimal(new int[] {
            29,
            0,
            0,
            0});
            this.Formatter_MinFigures.Name = "Formatter_MinFigures";
            this.Formatter_MinFigures.Size = new System.Drawing.Size(91, 20);
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
            label3.Location = new System.Drawing.Point(3, 280);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(228, 20);
            label3.TabIndex = 12;
            label3.Text = "Allow periodic anonymous update checks";
            label3.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label3, "Allows RenderDoc to phone home to https://renderdoc.org to anonymously check for " +
        "new versions.");
            // 
            // CheckUpdate_AllowChecks
            // 
            this.CheckUpdate_AllowChecks.AutoSize = true;
            this.CheckUpdate_AllowChecks.Checked = true;
            this.CheckUpdate_AllowChecks.CheckState = System.Windows.Forms.CheckState.Checked;
            this.CheckUpdate_AllowChecks.Location = new System.Drawing.Point(237, 283);
            this.CheckUpdate_AllowChecks.Name = "CheckUpdate_AllowChecks";
            this.CheckUpdate_AllowChecks.Size = new System.Drawing.Size(15, 14);
            this.CheckUpdate_AllowChecks.TabIndex = 8;
            this.toolTip.SetToolTip(this.CheckUpdate_AllowChecks, "Allows RenderDoc to phone home to https://renderdoc.org to anonymously check for " +
        "new versions.");
            this.CheckUpdate_AllowChecks.UseVisualStyleBackColor = true;
            this.CheckUpdate_AllowChecks.CheckedChanged += new System.EventHandler(this.CheckUpdate_AllowChecks_CheckedChanged);
            // 
            // browseTempCaptureDirectory
            // 
            this.browseTempCaptureDirectory.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.browseTempCaptureDirectory.Location = new System.Drawing.Point(238, 185);
            this.browseTempCaptureDirectory.Name = "browseTempCaptureDirectory";
            this.browseTempCaptureDirectory.Size = new System.Drawing.Size(90, 23);
            this.browseTempCaptureDirectory.TabIndex = 7;
            this.browseTempCaptureDirectory.Text = "Browse";
            this.toolTip.SetToolTip(this.browseTempCaptureDirectory, "Changes the directory where capture files are saved after being created, until sa" +
        "ved manually or deleted.\r\n\r\nDefaults to %TEMP%.");
            this.browseTempCaptureDirectory.UseVisualStyleBackColor = true;
            this.browseTempCaptureDirectory.Click += new System.EventHandler(this.browseTempCaptureDirectory_Click);
            // 
            // label15
            // 
            label15.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label15.AutoSize = true;
            label15.Location = new System.Drawing.Point(3, 300);
            label15.Name = "label15";
            label15.Size = new System.Drawing.Size(228, 26);
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
            this.Font_PreferMonospaced.Location = new System.Drawing.Point(237, 303);
            this.Font_PreferMonospaced.Name = "Font_PreferMonospaced";
            this.Font_PreferMonospaced.Size = new System.Drawing.Size(15, 14);
            this.Font_PreferMonospaced.TabIndex = 18;
            this.toolTip.SetToolTip(this.Font_PreferMonospaced, "Wherever possible a monospaced font will be used instead of the default font");
            this.Font_PreferMonospaced.UseVisualStyleBackColor = true;
            this.Font_PreferMonospaced.CheckedChanged += new System.EventHandler(this.Font_PreferMonospaced_CheckedChanged);
            // 
            // label18
            // 
            label18.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label18.AutoSize = true;
            label18.Location = new System.Drawing.Point(3, 326);
            label18.Name = "label18";
            label18.Size = new System.Drawing.Size(228, 26);
            label18.TabIndex = 19;
            label18.Text = "Always replay logs locally, never prompt about it";
            label18.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label18, resources.GetString("label18.ToolTip"));
            // 
            // AlwaysReplayLocally
            // 
            this.AlwaysReplayLocally.AutoSize = true;
            this.AlwaysReplayLocally.Checked = true;
            this.AlwaysReplayLocally.CheckState = System.Windows.Forms.CheckState.Checked;
            this.AlwaysReplayLocally.Location = new System.Drawing.Point(237, 329);
            this.AlwaysReplayLocally.Name = "AlwaysReplayLocally";
            this.AlwaysReplayLocally.Size = new System.Drawing.Size(15, 14);
            this.AlwaysReplayLocally.TabIndex = 20;
            this.toolTip.SetToolTip(this.AlwaysReplayLocally, resources.GetString("AlwaysReplayLocally.ToolTip"));
            this.AlwaysReplayLocally.UseVisualStyleBackColor = true;
            this.AlwaysReplayLocally.CheckedChanged += new System.EventHandler(this.AlwaysReplayLocally_CheckedChanged);
            // 
            // browseSaveCaptureDirectory
            // 
            this.browseSaveCaptureDirectory.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.browseSaveCaptureDirectory.Location = new System.Drawing.Point(238, 234);
            this.browseSaveCaptureDirectory.Name = "browseSaveCaptureDirectory";
            this.browseSaveCaptureDirectory.Size = new System.Drawing.Size(90, 23);
            this.browseSaveCaptureDirectory.TabIndex = 22;
            this.browseSaveCaptureDirectory.Text = "Browse";
            this.toolTip.SetToolTip(this.browseSaveCaptureDirectory, "Changes the default directory for the save dialog when saving capture files.\r\n\r\nD" +
        "efaults to blank, which follows system default behaviour.\r\n");
            this.browseSaveCaptureDirectory.UseVisualStyleBackColor = true;
            this.browseSaveCaptureDirectory.Click += new System.EventHandler(this.browseSaveCaptureDirectory_Click);
            // 
            // saveDirectory
            // 
            this.saveDirectory.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.saveDirectory.Location = new System.Drawing.Point(3, 234);
            this.saveDirectory.Name = "saveDirectory";
            this.saveDirectory.Size = new System.Drawing.Size(228, 20);
            this.saveDirectory.TabIndex = 23;
            this.toolTip.SetToolTip(this.saveDirectory, "Changes the default directory for the save dialog when saving capture files.\r\n\r\nD" +
        "efaults to blank, which follows system default behaviour.");
            this.saveDirectory.TextChanged += new System.EventHandler(this.saveDirectory_TextChanged);
            // 
            // label11
            // 
            label11.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label11.AutoSize = true;
            label11.Location = new System.Drawing.Point(3, 162);
            label11.MinimumSize = new System.Drawing.Size(0, 20);
            label11.Name = "label11";
            label11.Size = new System.Drawing.Size(228, 20);
            label11.TabIndex = 14;
            label11.Text = "Directory for temporary capture files";
            label11.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(label11, "Changes the directory where capture files are saved after being created, until sa" +
        "ved manually or deleted.\r\n\r\nDefaults to %TEMP%.");
            // 
            // tempDirectory
            // 
            this.tempDirectory.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tempDirectory.Location = new System.Drawing.Point(3, 185);
            this.tempDirectory.Name = "tempDirectory";
            this.tempDirectory.Size = new System.Drawing.Size(228, 20);
            this.tempDirectory.TabIndex = 24;
            this.toolTip.SetToolTip(this.tempDirectory, "Changes the directory where capture files are saved after being created, until sa" +
        "ved manually or deleted.\r\n\r\nDefaults to %TEMP%.");
            this.tempDirectory.TextChanged += new System.EventHandler(this.tempDirectory_TextChanged);
            // 
            // corePage
            // 
            this.corePage.Controls.Add(this.groupBox5);
            this.corePage.Location = new System.Drawing.Point(23, 4);
            this.corePage.Name = "corePage";
            this.corePage.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.corePage.Size = new System.Drawing.Size(344, 407);
            this.corePage.TabIndex = 4;
            this.corePage.Text = "Core";
            this.corePage.UseVisualStyleBackColor = true;
            // 
            // groupBox5
            // 
            this.groupBox5.Controls.Add(tableLayoutPanel6);
            this.groupBox5.Dock = System.Windows.Forms.DockStyle.Fill;
            this.groupBox5.Location = new System.Drawing.Point(3, 3);
            this.groupBox5.Name = "groupBox5";
            this.groupBox5.Size = new System.Drawing.Size(338, 401);
            this.groupBox5.TabIndex = 0;
            this.groupBox5.TabStop = false;
            this.groupBox5.Text = "Core";
            // 
            // tableLayoutPanel6
            // 
            tableLayoutPanel6.ColumnCount = 2;
            tableLayoutPanel6.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel6.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            tableLayoutPanel6.Controls.Add(this.chooseSearchPaths, 1, 0);
            tableLayoutPanel6.Controls.Add(label19, 0, 0);
            tableLayoutPanel6.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel6.Location = new System.Drawing.Point(3, 16);
            tableLayoutPanel6.Name = "tableLayoutPanel6";
            tableLayoutPanel6.RowCount = 2;
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            tableLayoutPanel6.Size = new System.Drawing.Size(332, 382);
            tableLayoutPanel6.TabIndex = 1;
            // 
            // chooseSearchPaths
            // 
            this.chooseSearchPaths.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.chooseSearchPaths.Location = new System.Drawing.Point(239, 3);
            this.chooseSearchPaths.Name = "chooseSearchPaths";
            this.chooseSearchPaths.Size = new System.Drawing.Size(90, 23);
            this.chooseSearchPaths.TabIndex = 1;
            this.chooseSearchPaths.Text = "Choose paths";
            this.chooseSearchPaths.UseVisualStyleBackColor = true;
            this.chooseSearchPaths.Click += new System.EventHandler(this.chooseSearchPaths_Click);
            // 
            // label19
            // 
            label19.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label19.AutoSize = true;
            label19.Location = new System.Drawing.Point(3, 3);
            label19.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label19.Name = "label19";
            label19.Size = new System.Drawing.Size(230, 23);
            label19.TabIndex = 0;
            label19.Text = "Shader debug search paths";
            label19.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // texViewTab
            // 
            this.texViewTab.Controls.Add(groupBox2);
            this.texViewTab.Location = new System.Drawing.Point(23, 4);
            this.texViewTab.Name = "texViewTab";
            this.texViewTab.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.texViewTab.Size = new System.Drawing.Size(344, 407);
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
            groupBox2.Size = new System.Drawing.Size(338, 401);
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
            this.tableLayoutPanel3.Size = new System.Drawing.Size(332, 382);
            this.tableLayoutPanel3.TabIndex = 0;
            // 
            // TextureViewer_ResetRange
            // 
            this.TextureViewer_ResetRange.AutoSize = true;
            this.TextureViewer_ResetRange.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TextureViewer_ResetRange.Location = new System.Drawing.Point(268, 3);
            this.TextureViewer_ResetRange.Name = "TextureViewer_ResetRange";
            this.TextureViewer_ResetRange.Size = new System.Drawing.Size(61, 14);
            this.TextureViewer_ResetRange.TabIndex = 20;
            this.toolTip.SetToolTip(this.TextureViewer_ResetRange, "Reset visible range when changing event or texture");
            this.TextureViewer_ResetRange.UseVisualStyleBackColor = true;
            this.TextureViewer_ResetRange.CheckedChanged += new System.EventHandler(this.TextureViewer_ResetRange_CheckedChanged);
            // 
            // TextureViewer_PerTexSettings
            // 
            this.TextureViewer_PerTexSettings.AutoSize = true;
            this.TextureViewer_PerTexSettings.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TextureViewer_PerTexSettings.Location = new System.Drawing.Point(268, 23);
            this.TextureViewer_PerTexSettings.Name = "TextureViewer_PerTexSettings";
            this.TextureViewer_PerTexSettings.Size = new System.Drawing.Size(61, 14);
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
            label10.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label10.Name = "label10";
            label10.Size = new System.Drawing.Size(259, 14);
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
            this.label14.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.label14.Name = "label14";
            this.label14.Size = new System.Drawing.Size(259, 14);
            this.label14.TabIndex = 4;
            this.label14.Text = "Visible channels && mip/slice saved per-texture";
            this.label14.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // shadViewTab
            // 
            this.shadViewTab.Controls.Add(groupBox3);
            this.shadViewTab.Location = new System.Drawing.Point(23, 4);
            this.shadViewTab.Name = "shadViewTab";
            this.shadViewTab.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.shadViewTab.Size = new System.Drawing.Size(344, 407);
            this.shadViewTab.TabIndex = 2;
            this.shadViewTab.Text = "Shader Viewer";
            this.shadViewTab.UseVisualStyleBackColor = true;
            // 
            // groupBox3
            // 
            groupBox3.Controls.Add(this.groupBox6);
            groupBox3.Controls.Add(this.tableLayoutPanel4);
            groupBox3.Dock = System.Windows.Forms.DockStyle.Fill;
            groupBox3.Location = new System.Drawing.Point(3, 3);
            groupBox3.Name = "groupBox3";
            groupBox3.Size = new System.Drawing.Size(338, 401);
            groupBox3.TabIndex = 0;
            groupBox3.TabStop = false;
            groupBox3.Text = "Shader Viewer";
            // 
            // groupBox6
            // 
            this.groupBox6.Controls.Add(this.tableLayoutPanel7);
            this.groupBox6.Dock = System.Windows.Forms.DockStyle.Top;
            this.groupBox6.Location = new System.Drawing.Point(3, 60);
            this.groupBox6.Name = "groupBox6";
            this.groupBox6.Size = new System.Drawing.Size(332, 166);
            this.groupBox6.TabIndex = 46;
            this.groupBox6.TabStop = false;
            this.groupBox6.Text = "Vulkan";
            // 
            // tableLayoutPanel7
            // 
            this.tableLayoutPanel7.ColumnCount = 2;
            this.tableLayoutPanel7.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            this.tableLayoutPanel7.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            this.tableLayoutPanel7.Controls.Add(this.label23, 0, 3);
            this.tableLayoutPanel7.Controls.Add(this.externalDisassemblerEnabledCheckbox, 1, 0);
            this.tableLayoutPanel7.Controls.Add(this.label21, 0, 0);
            this.tableLayoutPanel7.Controls.Add(this.label22, 0, 1);
            this.tableLayoutPanel7.Controls.Add(this.externalDisassemblePath, 0, 2);
            this.tableLayoutPanel7.Controls.Add(this.browseExtDisasemble, 1, 2);
            this.tableLayoutPanel7.Controls.Add(this.externalDisassemblerArgs, 0, 4);
            this.tableLayoutPanel7.Controls.Add(this.label24, 0, 5);
            this.tableLayoutPanel7.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel7.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel7.Name = "tableLayoutPanel7";
            this.tableLayoutPanel7.RowCount = 6;
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel7.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayoutPanel7.Size = new System.Drawing.Size(326, 147);
            this.tableLayoutPanel7.TabIndex = 0;
            // 
            // label23
            // 
            this.label23.AutoSize = true;
            this.label23.Dock = System.Windows.Forms.DockStyle.Fill;
            this.label23.Location = new System.Drawing.Point(3, 65);
            this.label23.Name = "label23";
            this.label23.Size = new System.Drawing.Size(254, 13);
            this.label23.TabIndex = 50;
            this.label23.Text = "Specify the command line arguments";
            // 
            // externalDisassemblerEnabledCheckbox
            // 
            this.externalDisassemblerEnabledCheckbox.AutoSize = true;
            this.externalDisassemblerEnabledCheckbox.Location = new System.Drawing.Point(263, 3);
            this.externalDisassemblerEnabledCheckbox.Name = "externalDisassemblerEnabledCheckbox";
            this.externalDisassemblerEnabledCheckbox.Size = new System.Drawing.Size(15, 14);
            this.externalDisassemblerEnabledCheckbox.TabIndex = 44;
            this.externalDisassemblerEnabledCheckbox.UseVisualStyleBackColor = true;
            this.externalDisassemblerEnabledCheckbox.CheckedChanged += new System.EventHandler(this.externalDisassemblerEnabledCheckbox_CheckedChanged);
            // 
            // label21
            // 
            this.label21.AutoSize = true;
            this.label21.Location = new System.Drawing.Point(3, 0);
            this.label21.Name = "label21";
            this.label21.Size = new System.Drawing.Size(146, 13);
            this.label21.TabIndex = 45;
            this.label21.Text = "Vulkan External Disassembler";
            // 
            // label22
            // 
            this.label22.AutoSize = true;
            this.label22.Dock = System.Windows.Forms.DockStyle.Fill;
            this.label22.Location = new System.Drawing.Point(3, 20);
            this.label22.Name = "label22";
            this.label22.Size = new System.Drawing.Size(254, 13);
            this.label22.TabIndex = 49;
            this.label22.Text = "Select external SPIR-V disassembler executable";
            // 
            // externalDisassemblePath
            // 
            this.externalDisassemblePath.Dock = System.Windows.Forms.DockStyle.Fill;
            this.externalDisassemblePath.Location = new System.Drawing.Point(3, 36);
            this.externalDisassemblePath.Name = "externalDisassemblePath";
            this.externalDisassemblePath.Size = new System.Drawing.Size(254, 20);
            this.externalDisassemblePath.TabIndex = 47;
            this.externalDisassemblePath.TextChanged += new System.EventHandler(this.externalDisassemblePath_TextChanged);
            // 
            // browseExtDisasemble
            // 
            this.browseExtDisasemble.Dock = System.Windows.Forms.DockStyle.Left;
            this.browseExtDisasemble.Location = new System.Drawing.Point(263, 36);
            this.browseExtDisasemble.Name = "browseExtDisasemble";
            this.browseExtDisasemble.Size = new System.Drawing.Size(57, 26);
            this.browseExtDisasemble.TabIndex = 48;
            this.browseExtDisasemble.Text = "Browse";
            this.browseExtDisasemble.UseVisualStyleBackColor = true;
            this.browseExtDisasemble.Click += new System.EventHandler(this.browseExtDisasemble_Click);
            // 
            // externalDisassemblerArgs
            // 
            this.externalDisassemblerArgs.Dock = System.Windows.Forms.DockStyle.Fill;
            this.externalDisassemblerArgs.Location = new System.Drawing.Point(3, 81);
            this.externalDisassemblerArgs.Name = "externalDisassemblerArgs";
            this.externalDisassemblerArgs.Size = new System.Drawing.Size(254, 20);
            this.externalDisassemblerArgs.TabIndex = 46;
            this.externalDisassemblerArgs.TextChanged += new System.EventHandler(this.textBox1_TextChanged);
            // 
            // label24
            // 
            this.label24.AutoSize = true;
            this.label24.Dock = System.Windows.Forms.DockStyle.Fill;
            this.label24.Location = new System.Drawing.Point(3, 104);
            this.label24.Name = "label24";
            this.label24.Size = new System.Drawing.Size(254, 43);
            this.label24.TabIndex = 51;
            this.label24.Text = "NOTE: Use the {spv_bin} and {spv_disas} tags to indicate to the external disassem" +
    "bler the input SPIR-V binary and the output SPIR-V disassembly respectively.";
            // 
            // tableLayoutPanel4
            // 
            this.tableLayoutPanel4.ColumnCount = 2;
            this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 80F));
            this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20F));
            this.tableLayoutPanel4.Controls.Add(this.ShaderViewer_FriendlyNaming, 1, 0);
            this.tableLayoutPanel4.Controls.Add(label12, 0, 0);
            this.tableLayoutPanel4.Dock = System.Windows.Forms.DockStyle.Top;
            this.tableLayoutPanel4.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel4.Name = "tableLayoutPanel4";
            this.tableLayoutPanel4.RowCount = 2;
            this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayoutPanel4.Size = new System.Drawing.Size(332, 44);
            this.tableLayoutPanel4.TabIndex = 1;
            // 
            // ShaderViewer_FriendlyNaming
            // 
            this.ShaderViewer_FriendlyNaming.AutoSize = true;
            this.ShaderViewer_FriendlyNaming.Location = new System.Drawing.Point(268, 3);
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
            label12.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label12.Name = "label12";
            label12.Size = new System.Drawing.Size(259, 14);
            label12.TabIndex = 6;
            label12.Text = "Rename disassembly registers";
            label12.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // eventTab
            // 
            this.eventTab.Controls.Add(groupBox4);
            this.eventTab.Location = new System.Drawing.Point(23, 4);
            this.eventTab.Name = "eventTab";
            this.eventTab.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.eventTab.Size = new System.Drawing.Size(343, 408);
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
            groupBox4.Size = new System.Drawing.Size(337, 402);
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
            this.tableLayoutPanel5.Controls.Add(label9, 0, 2);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_HideEmpty, 1, 2);
            this.tableLayoutPanel5.Controls.Add(this.label35, 0, 3);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_HideAPICalls, 1, 3);
            this.tableLayoutPanel5.Controls.Add(label16, 0, 4);
            this.tableLayoutPanel5.Controls.Add(label17, 0, 5);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_ApplyColours, 1, 4);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_ColourEventRow, 1, 5);
            this.tableLayoutPanel5.Controls.Add(label26, 0, 1);
            this.tableLayoutPanel5.Controls.Add(this.EventBrowser_AddFake, 1, 1);
            this.tableLayoutPanel5.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel5.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel5.Name = "tableLayoutPanel5";
            this.tableLayoutPanel5.RowCount = 7;
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel5.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayoutPanel5.Size = new System.Drawing.Size(331, 383);
            this.tableLayoutPanel5.TabIndex = 0;
            // 
            // label8
            // 
            label8.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label8.AutoSize = true;
            label8.Location = new System.Drawing.Point(3, 3);
            label8.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label8.Name = "label8";
            label8.Size = new System.Drawing.Size(258, 21);
            label8.TabIndex = 3;
            label8.Text = "Time unit used for event browser timings";
            label8.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_TimeUnit
            // 
            this.EventBrowser_TimeUnit.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.EventBrowser_TimeUnit.FormattingEnabled = true;
            this.EventBrowser_TimeUnit.Location = new System.Drawing.Point(267, 3);
            this.EventBrowser_TimeUnit.Name = "EventBrowser_TimeUnit";
            this.EventBrowser_TimeUnit.Size = new System.Drawing.Size(61, 21);
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
            label9.Location = new System.Drawing.Point(3, 49);
            label9.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label9.Name = "label9";
            label9.Size = new System.Drawing.Size(258, 14);
            label9.TabIndex = 7;
            label9.Text = "Hide empty marker sections (requires log reload)";
            label9.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_HideEmpty
            // 
            this.EventBrowser_HideEmpty.AutoSize = true;
            this.EventBrowser_HideEmpty.Location = new System.Drawing.Point(267, 49);
            this.EventBrowser_HideEmpty.Name = "EventBrowser_HideEmpty";
            this.EventBrowser_HideEmpty.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_HideEmpty.TabIndex = 51;
            this.toolTip.SetToolTip(this.EventBrowser_HideEmpty, "In the Event Browser and Timeline Bar, marker sections that contain no API calls " +
        "or drawcalls will be completely removed");
            this.EventBrowser_HideEmpty.UseVisualStyleBackColor = true;
            this.EventBrowser_HideEmpty.CheckedChanged += new System.EventHandler(this.EventBrowser_HideEmpty_CheckedChanged);
            // 
            // label35
            // 
            this.label35.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label35.AutoSize = true;
            this.label35.Location = new System.Drawing.Point(3, 69);
            this.label35.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.label35.Name = "label35";
            this.label35.Size = new System.Drawing.Size(258, 26);
            this.label35.TabIndex = 7;
            this.label35.Text = "Hide marker sections with only non-draw API calls (requires log reload)";
            this.label35.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_HideAPICalls
            // 
            this.EventBrowser_HideAPICalls.AutoSize = true;
            this.EventBrowser_HideAPICalls.Location = new System.Drawing.Point(267, 69);
            this.EventBrowser_HideAPICalls.Name = "EventBrowser_HideAPICalls";
            this.EventBrowser_HideAPICalls.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_HideAPICalls.TabIndex = 51;
            this.toolTip.SetToolTip(this.EventBrowser_HideAPICalls, "In the Event Browser and Timeline Bar, marker sections that contain only non-draw" +
        " API calls - e.g. only queries, or only state setting - will be completely remov" +
        "ed");
            this.EventBrowser_HideAPICalls.UseVisualStyleBackColor = true;
            this.EventBrowser_HideAPICalls.CheckedChanged += new System.EventHandler(this.EventBrowser_HideAPICalls_CheckedChanged);
            // 
            // label16
            // 
            label16.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label16.AutoSize = true;
            label16.Location = new System.Drawing.Point(3, 101);
            label16.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label16.Name = "label16";
            label16.Size = new System.Drawing.Size(258, 14);
            label16.TabIndex = 52;
            label16.Text = "Apply marker colours (requires log reload)";
            label16.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // label17
            // 
            label17.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label17.AutoSize = true;
            label17.Location = new System.Drawing.Point(3, 121);
            label17.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label17.Name = "label17";
            label17.Padding = new System.Windows.Forms.Padding(15, 0, 0, 0);
            label17.Size = new System.Drawing.Size(258, 14);
            label17.TabIndex = 53;
            label17.Text = "- Colourise whole row for marker regions";
            label17.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // EventBrowser_ApplyColours
            // 
            this.EventBrowser_ApplyColours.AutoSize = true;
            this.EventBrowser_ApplyColours.Location = new System.Drawing.Point(267, 101);
            this.EventBrowser_ApplyColours.Name = "EventBrowser_ApplyColours";
            this.EventBrowser_ApplyColours.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_ApplyColours.TabIndex = 54;
            this.toolTip.SetToolTip(this.EventBrowser_ApplyColours, "In the Event Browser and Timeline Bar, marker sections and marker labels will be " +
        "coloured with an API-specified colour.\r\n");
            this.EventBrowser_ApplyColours.UseVisualStyleBackColor = true;
            this.EventBrowser_ApplyColours.CheckedChanged += new System.EventHandler(this.EventBrowser_ApplyColours_CheckedChanged);
            // 
            // EventBrowser_ColourEventRow
            // 
            this.EventBrowser_ColourEventRow.AutoSize = true;
            this.EventBrowser_ColourEventRow.Location = new System.Drawing.Point(267, 121);
            this.EventBrowser_ColourEventRow.Name = "EventBrowser_ColourEventRow";
            this.EventBrowser_ColourEventRow.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_ColourEventRow.TabIndex = 55;
            this.toolTip.SetToolTip(this.EventBrowser_ColourEventRow, "When colouring marker sections in the Event Browser, the whole row of a marker re" +
        "gion will be coloured, not just a bar to the left of its children.");
            this.EventBrowser_ColourEventRow.UseVisualStyleBackColor = true;
            this.EventBrowser_ColourEventRow.CheckedChanged += new System.EventHandler(this.EventBrowser_ColourEventRow_CheckedChanged);
            // 
            // EventBrowser_AddFake
            // 
            this.EventBrowser_AddFake.AutoSize = true;
            this.EventBrowser_AddFake.Location = new System.Drawing.Point(266, 29);
            this.EventBrowser_AddFake.Margin = new System.Windows.Forms.Padding(2, 2, 2, 2);
            this.EventBrowser_AddFake.Name = "EventBrowser_AddFake";
            this.EventBrowser_AddFake.Size = new System.Drawing.Size(15, 14);
            this.EventBrowser_AddFake.TabIndex = 57;
            this.toolTip.SetToolTip(this.EventBrowser_AddFake, "In the Event Browser, add fake markers if none found in capture");
            this.EventBrowser_AddFake.UseVisualStyleBackColor = true;
            this.EventBrowser_AddFake.CheckedChanged += new System.EventHandler(this.EventBrowser_AddFake_CheckedChanged);
            // 
            // label26
            // 
            label26.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label26.AutoSize = true;
            label26.Location = new System.Drawing.Point(3, 30);
            label26.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            label26.Name = "label26";
            label26.Size = new System.Drawing.Size(258, 13);
            label26.TabIndex = 56;
            label26.Text = "Add fake markers if none present (requires log reload)";
            label26.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // androidTab
            // 
            this.androidTab.Controls.Add(this.groupBox7);
            this.androidTab.Location = new System.Drawing.Point(23, 4);
            this.androidTab.Name = "androidTab";
            this.androidTab.Padding = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.androidTab.Size = new System.Drawing.Size(344, 407);
            this.androidTab.TabIndex = 5;
            this.androidTab.Text = "Android";
            this.androidTab.UseVisualStyleBackColor = true;
            // 
            // groupBox7
            // 
            this.groupBox7.Controls.Add(this.tableLayoutPanel8);
            this.groupBox7.Dock = System.Windows.Forms.DockStyle.Fill;
            this.groupBox7.Location = new System.Drawing.Point(3, 3);
            this.groupBox7.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBox7.Name = "groupBox7";
            this.groupBox7.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBox7.Size = new System.Drawing.Size(338, 401);
            this.groupBox7.TabIndex = 1;
            this.groupBox7.TabStop = false;
            this.groupBox7.Text = "Android";
            // 
            // tableLayoutPanel8
            // 
            this.tableLayoutPanel8.ColumnCount = 2;
            this.tableLayoutPanel8.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel8.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel8.Controls.Add(this.label25, 0, 2);
            this.tableLayoutPanel8.Controls.Add(this.maxConnectTimeout, 1, 2);
            this.tableLayoutPanel8.Controls.Add(this.label36, 0, 0);
            this.tableLayoutPanel8.Controls.Add(this.adbPath, 0, 1);
            this.tableLayoutPanel8.Controls.Add(this.browseAdbPath, 1, 1);
            this.tableLayoutPanel8.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel8.Location = new System.Drawing.Point(4, 17);
            this.tableLayoutPanel8.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tableLayoutPanel8.Name = "tableLayoutPanel8";
            this.tableLayoutPanel8.RowCount = 4;
            this.tableLayoutPanel8.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel8.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel8.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel8.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel8.Size = new System.Drawing.Size(330, 380);
            this.tableLayoutPanel8.TabIndex = 0;
            // 
            // label25
            // 
            this.label25.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label25.AutoSize = true;
            this.label25.Location = new System.Drawing.Point(4, 70);
            this.label25.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.label25.Name = "label25";
            this.label25.Size = new System.Drawing.Size(226, 20);
            this.label25.TabIndex = 26;
            this.label25.Text = "Max Connection Timeout (s)";
            this.label25.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(this.label25, "Maximum time to try connecting to the target app.");
            // 
            // maxConnectTimeout
            // 
            this.maxConnectTimeout.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.maxConnectTimeout.AutoSize = true;
            this.maxConnectTimeout.Location = new System.Drawing.Point(238, 70);
            this.maxConnectTimeout.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.maxConnectTimeout.Maximum = new decimal(new int[] {
            1000,
            0,
            0,
            0});
            this.maxConnectTimeout.Name = "maxConnectTimeout";
            this.maxConnectTimeout.Size = new System.Drawing.Size(88, 20);
            this.maxConnectTimeout.TabIndex = 25;
            this.toolTip.SetToolTip(this.maxConnectTimeout, "Maximum time to try connecting to the target app.");
            this.maxConnectTimeout.ValueChanged += new System.EventHandler(this.maxConnectTimeout_ValueChanged);
            // 
            // label36
            // 
            this.label36.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label36.AutoSize = true;
            this.label36.Location = new System.Drawing.Point(3, 3);
            this.label36.Margin = new System.Windows.Forms.Padding(3, 3, 3, 3);
            this.label36.MinimumSize = new System.Drawing.Size(0, 31);
            this.label36.Name = "label36";
            this.label36.Size = new System.Drawing.Size(228, 31);
            this.label36.TabIndex = 14;
            this.label36.Text = "Android ADB executable path";
            this.label36.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(this.label36, "The location of adb.exe, used to control Android devices.");
            // 
            // adbPath
            // 
            this.adbPath.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.adbPath.Location = new System.Drawing.Point(3, 41);
            this.adbPath.Name = "adbPath";
            this.adbPath.Size = new System.Drawing.Size(228, 20);
            this.adbPath.TabIndex = 24;
            this.toolTip.SetToolTip(this.adbPath, "The location of adb.exe, used to control Android devices.");
            this.adbPath.TextChanged += new System.EventHandler(this.adbPath_TextChanged);
            // 
            // browseAdbPath
            // 
            this.browseAdbPath.Anchor = System.Windows.Forms.AnchorStyles.None;
            this.browseAdbPath.Location = new System.Drawing.Point(237, 40);
            this.browseAdbPath.Name = "browseAdbPath";
            this.browseAdbPath.Size = new System.Drawing.Size(90, 23);
            this.browseAdbPath.TabIndex = 7;
            this.browseAdbPath.Text = "Browse";
            this.toolTip.SetToolTip(this.browseAdbPath, "The location of adb.exe, used to control Android devices.");
            this.browseAdbPath.UseVisualStyleBackColor = true;
            this.browseAdbPath.Click += new System.EventHandler(this.browseAdbPath_Click);
            // 
            // pagesTree
            // 
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
            this.pagesTree.Size = new System.Drawing.Size(155, 416);
            this.pagesTree.TabIndex = 0;
            this.pagesTree.ViewOptions.ShowGridLines = false;
            this.pagesTree.ViewOptions.ShowLine = false;
            this.pagesTree.ViewOptions.ShowPlusMinus = false;
            this.pagesTree.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.pagesTree_AfterSelect);
            // 
            // ok
            // 
            this.ok.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ok.Location = new System.Drawing.Point(459, 425);
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
            this.ClientSize = new System.Drawing.Size(537, 451);
            this.Controls.Add(tableLayoutPanel1);
            this.MinimumSize = new System.Drawing.Size(495, 290);
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
            this.corePage.ResumeLayout(false);
            this.groupBox5.ResumeLayout(false);
            tableLayoutPanel6.ResumeLayout(false);
            tableLayoutPanel6.PerformLayout();
            this.texViewTab.ResumeLayout(false);
            groupBox2.ResumeLayout(false);
            this.tableLayoutPanel3.ResumeLayout(false);
            this.tableLayoutPanel3.PerformLayout();
            this.shadViewTab.ResumeLayout(false);
            groupBox3.ResumeLayout(false);
            this.groupBox6.ResumeLayout(false);
            this.tableLayoutPanel7.ResumeLayout(false);
            this.tableLayoutPanel7.PerformLayout();
            this.tableLayoutPanel4.ResumeLayout(false);
            this.tableLayoutPanel4.PerformLayout();
            this.eventTab.ResumeLayout(false);
            groupBox4.ResumeLayout(false);
            this.tableLayoutPanel5.ResumeLayout(false);
            this.tableLayoutPanel5.PerformLayout();
            this.androidTab.ResumeLayout(false);
            this.groupBox7.ResumeLayout(false);
            this.tableLayoutPanel8.ResumeLayout(false);
            this.tableLayoutPanel8.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.maxConnectTimeout)).EndInit();
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
        private System.Windows.Forms.CheckBox EventBrowser_HideAPICalls;
        private System.Windows.Forms.Button browseTempCaptureDirectory;
        private System.Windows.Forms.FolderBrowserDialog browserCaptureDialog;
        private System.Windows.Forms.CheckBox AllowGlobalHook;
        private System.Windows.Forms.CheckBox Font_PreferMonospaced;
        private System.Windows.Forms.Label label14;
        private System.Windows.Forms.TabPage corePage;
        private System.Windows.Forms.GroupBox groupBox5;
        private System.Windows.Forms.Button chooseSearchPaths;
        private System.Windows.Forms.CheckBox EventBrowser_ApplyColours;
        private System.Windows.Forms.CheckBox EventBrowser_ColourEventRow;
        private System.Windows.Forms.CheckBox AlwaysReplayLocally;
        private System.Windows.Forms.Button browseSaveCaptureDirectory;
        private System.Windows.Forms.TextBox saveDirectory;
        private System.Windows.Forms.TextBox tempDirectory;
        private System.Windows.Forms.CheckBox externalDisassemblerEnabledCheckbox;
        private System.Windows.Forms.Label label21;
        private System.Windows.Forms.GroupBox groupBox6;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel7;
        private System.Windows.Forms.TextBox externalDisassemblerArgs;
        private System.Windows.Forms.TextBox externalDisassemblePath;
        private System.Windows.Forms.Button browseExtDisasemble;
        private System.Windows.Forms.OpenFileDialog browseExtDisassembleDialog;
        private System.Windows.Forms.Label label23;
        private System.Windows.Forms.Label label22;
        private System.Windows.Forms.Label label24;
        private System.Windows.Forms.TabPage androidTab;
        private System.Windows.Forms.Button browseAdbPath;
        private System.Windows.Forms.TextBox adbPath;
        private System.Windows.Forms.NumericUpDown maxConnectTimeout;
        private System.Windows.Forms.Label label35;
        private System.Windows.Forms.GroupBox groupBox7;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel8;
        private System.Windows.Forms.Label label25;
        private System.Windows.Forms.Label label36;
        private System.Windows.Forms.CheckBox EventBrowser_AddFake;
    }
}