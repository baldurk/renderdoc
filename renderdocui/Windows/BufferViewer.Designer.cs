namespace renderdocui.Windows
{
    partial class BufferViewer
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
            System.Windows.Forms.Label label7;
            System.Windows.Forms.Label label6;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label5;
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BufferViewer));
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle7 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle8 = new System.Windows.Forms.DataGridViewCellStyle();
            WeifenLuo.WinFormsUI.Docking.DockPanelSkin dockPanelSkin3 = new WeifenLuo.WinFormsUI.Docking.DockPanelSkin();
            WeifenLuo.WinFormsUI.Docking.AutoHideStripSkin autoHideStripSkin3 = new WeifenLuo.WinFormsUI.Docking.AutoHideStripSkin();
            WeifenLuo.WinFormsUI.Docking.DockPanelGradient dockPanelGradient7 = new WeifenLuo.WinFormsUI.Docking.DockPanelGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient15 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.DockPaneStripSkin dockPaneStripSkin3 = new WeifenLuo.WinFormsUI.Docking.DockPaneStripSkin();
            WeifenLuo.WinFormsUI.Docking.DockPaneStripGradient dockPaneStripGradient3 = new WeifenLuo.WinFormsUI.Docking.DockPaneStripGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient16 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.DockPanelGradient dockPanelGradient8 = new WeifenLuo.WinFormsUI.Docking.DockPanelGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient17 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.DockPaneStripToolWindowGradient dockPaneStripToolWindowGradient3 = new WeifenLuo.WinFormsUI.Docking.DockPaneStripToolWindowGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient18 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient19 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.DockPanelGradient dockPanelGradient9 = new WeifenLuo.WinFormsUI.Docking.DockPanelGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient20 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            WeifenLuo.WinFormsUI.Docking.TabGradient tabGradient21 = new WeifenLuo.WinFormsUI.Docking.TabGradient();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle9 = new System.Windows.Forms.DataGridViewCellStyle();
            this.previewTable = new System.Windows.Forms.TableLayoutPanel();
            this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
            this.render = new renderdocui.Controls.NoScrollPanel();
            this.configCamControls = new System.Windows.Forms.GroupBox();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.camSpeed = new System.Windows.Forms.NumericUpDown();
            this.farGuess = new System.Windows.Forms.TextBox();
            this.nearGuess = new System.Windows.Forms.TextBox();
            this.aspectGuess = new System.Windows.Forms.TextBox();
            this.fovGuess = new System.Windows.Forms.TextBox();
            this.label4 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.matrixType = new System.Windows.Forms.ComboBox();
            this.toolStrip2 = new System.Windows.Forms.ToolStrip();
            this.configureCam = new System.Windows.Forms.ToolStripButton();
            this.resetCam = new System.Windows.Forms.ToolStripButton();
            this.fitScreen = new System.Windows.Forms.ToolStripButton();
            this.controlType = new System.Windows.Forms.ToolStripComboBox();
            this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
            this.drawRange = new System.Windows.Forms.ToolStripComboBox();
            this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
            this.toolStripLabel3 = new System.Windows.Forms.ToolStripLabel();
            this.solidShading = new System.Windows.Forms.ToolStripComboBox();
            this.wireframeDraw = new System.Windows.Forms.ToolStripButton();
            this.rightclickMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.debugVertex = new System.Windows.Forms.ToolStripMenuItem();
            this.openFormat = new System.Windows.Forms.ToolStripMenuItem();
            this.syncViews = new System.Windows.Forms.ToolStripMenuItem();
            this.exportToToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.cSVToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.rawToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.setInstanceToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.instanceIdx = new System.Windows.Forms.ToolStripTextBox();
            this.previewTab = new System.Windows.Forms.TabControl();
            this.tabPage1 = new System.Windows.Forms.TabPage();
            this.tabPage2 = new System.Windows.Forms.TabPage();
            this.tabPage3 = new System.Windows.Forms.TabPage();
            this.vsInBufferView = new System.Windows.Forms.DataGridView();
            this.csvSaveDialog = new System.Windows.Forms.SaveFileDialog();
            this.rawSaveDialog = new System.Windows.Forms.SaveFileDialog();
            this.vsOutBufferView = new System.Windows.Forms.DataGridView();
            this.dockPanel = new WeifenLuo.WinFormsUI.Docking.DockPanel();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.toolStripLabel1 = new System.Windows.Forms.ToolStripLabel();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.exportToolItem = new System.Windows.Forms.ToolStripDropDownButton();
            this.exportToCSVToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exportRawBytesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.syncViewsToolItem = new System.Windows.Forms.ToolStripButton();
            this.highlightVerts = new System.Windows.Forms.ToolStripButton();
            this.debugSep = new System.Windows.Forms.ToolStripSeparator();
            this.debugVertexToolItem = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.toolStripLabel2 = new System.Windows.Forms.ToolStripLabel();
            this.rowOffset = new System.Windows.Forms.ToolStripTextBox();
            this.byteOffsLab = new System.Windows.Forms.ToolStripLabel();
            this.byteOffset = new System.Windows.Forms.ToolStripTextBox();
            this.offsetCancel = new System.Windows.Forms.ToolStripButton();
            this.instSep = new System.Windows.Forms.ToolStripSeparator();
            this.instLabel = new System.Windows.Forms.ToolStripLabel();
            this.instanceIdxToolitem = new System.Windows.Forms.ToolStripTextBox();
            this.gsOutBufferView = new System.Windows.Forms.DataGridView();
            this.columnContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.resetSelectedColumnsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator5 = new System.Windows.Forms.ToolStripSeparator();
            this.selectColumnAsPositionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.selectColumnAsSecondaryToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.selectAlphaAsSecondaryToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            label7 = new System.Windows.Forms.Label();
            label6 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label5 = new System.Windows.Forms.Label();
            this.previewTable.SuspendLayout();
            this.tableLayoutPanel3.SuspendLayout();
            this.configCamControls.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.camSpeed)).BeginInit();
            this.toolStrip2.SuspendLayout();
            this.rightclickMenu.SuspendLayout();
            this.previewTab.SuspendLayout();
            this.tabPage1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.vsInBufferView)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.vsOutBufferView)).BeginInit();
            this.tableLayoutPanel1.SuspendLayout();
            this.flowLayoutPanel2.SuspendLayout();
            this.toolStrip1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.gsOutBufferView)).BeginInit();
            this.columnContextMenu.SuspendLayout();
            this.SuspendLayout();
            // 
            // label7
            // 
            label7.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            label7.AutoSize = true;
            label7.Location = new System.Drawing.Point(36, 167);
            label7.Margin = new System.Windows.Forms.Padding(3, 7, 3, 0);
            label7.Name = "label7";
            label7.Size = new System.Drawing.Size(55, 13);
            label7.TabIndex = 17;
            label7.Text = "Far Plane:";
            // 
            // label6
            // 
            label6.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label6.AutoSize = true;
            label6.Location = new System.Drawing.Point(28, 140);
            label6.Name = "label6";
            label6.Size = new System.Drawing.Size(63, 13);
            label6.TabIndex = 16;
            label6.Text = "Near Plane:";
            // 
            // label1
            // 
            label1.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(27, 88);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(64, 13);
            label1.TabIndex = 5;
            label1.Text = "Persp. FOV:";
            // 
            // label3
            // 
            label3.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(50, 6);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(41, 13);
            label3.TabIndex = 11;
            label3.Text = "Speed:";
            label3.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // label5
            // 
            label5.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label5.AutoSize = true;
            label5.Location = new System.Drawing.Point(20, 114);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(71, 13);
            label5.TabIndex = 15;
            label5.Text = "Aspect Ratio:";
            // 
            // previewTable
            // 
            this.previewTable.ColumnCount = 1;
            this.previewTable.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.previewTable.Controls.Add(this.tableLayoutPanel3, 0, 1);
            this.previewTable.Controls.Add(this.toolStrip2, 0, 0);
            this.previewTable.Dock = System.Windows.Forms.DockStyle.Fill;
            this.previewTable.Location = new System.Drawing.Point(3, 3);
            this.previewTable.Name = "previewTable";
            this.previewTable.RowCount = 2;
            this.previewTable.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.previewTable.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.previewTable.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.previewTable.Size = new System.Drawing.Size(545, 281);
            this.previewTable.TabIndex = 4;
            // 
            // tableLayoutPanel3
            // 
            this.tableLayoutPanel3.ColumnCount = 2;
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.Controls.Add(this.render, 1, 0);
            this.tableLayoutPanel3.Controls.Add(this.configCamControls, 0, 0);
            this.tableLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel3.Location = new System.Drawing.Point(3, 28);
            this.tableLayoutPanel3.Name = "tableLayoutPanel3";
            this.tableLayoutPanel3.RowCount = 1;
            this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel3.Size = new System.Drawing.Size(539, 250);
            this.tableLayoutPanel3.TabIndex = 5;
            // 
            // render
            // 
            this.render.BackColor = System.Drawing.Color.Black;
            this.render.Dock = System.Windows.Forms.DockStyle.Fill;
            this.render.Location = new System.Drawing.Point(203, 3);
            this.render.Name = "render";
            this.render.Size = new System.Drawing.Size(333, 244);
            this.render.TabIndex = 2;
            this.render.Paint += new System.Windows.Forms.PaintEventHandler(this.render_Paint);
            this.render.MouseClick += new System.Windows.Forms.MouseEventHandler(this.render_MouseClick);
            this.render.MouseDown += new System.Windows.Forms.MouseEventHandler(this.render_MouseDown);
            this.render.MouseMove += new System.Windows.Forms.MouseEventHandler(this.render_MouseMove);
            // 
            // configCamControls
            // 
            this.configCamControls.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.configCamControls.Controls.Add(this.tableLayoutPanel2);
            this.configCamControls.Location = new System.Drawing.Point(3, 3);
            this.configCamControls.Name = "configCamControls";
            this.configCamControls.Size = new System.Drawing.Size(194, 244);
            this.configCamControls.TabIndex = 4;
            this.configCamControls.TabStop = false;
            this.configCamControls.Text = "Camera Controls";
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.ColumnCount = 2;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this.tableLayoutPanel2.Controls.Add(this.camSpeed, 1, 1);
            this.tableLayoutPanel2.Controls.Add(label3, 0, 1);
            this.tableLayoutPanel2.Controls.Add(this.farGuess, 1, 7);
            this.tableLayoutPanel2.Controls.Add(this.nearGuess, 1, 6);
            this.tableLayoutPanel2.Controls.Add(label7, 0, 7);
            this.tableLayoutPanel2.Controls.Add(this.aspectGuess, 1, 5);
            this.tableLayoutPanel2.Controls.Add(label6, 0, 6);
            this.tableLayoutPanel2.Controls.Add(label5, 0, 5);
            this.tableLayoutPanel2.Controls.Add(this.fovGuess, 1, 4);
            this.tableLayoutPanel2.Controls.Add(label1, 0, 4);
            this.tableLayoutPanel2.Controls.Add(this.label4, 0, 3);
            this.tableLayoutPanel2.Controls.Add(this.label2, 0, 2);
            this.tableLayoutPanel2.Controls.Add(this.matrixType, 1, 3);
            this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel2.Location = new System.Drawing.Point(3, 16);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 8;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
            this.tableLayoutPanel2.Size = new System.Drawing.Size(188, 225);
            this.tableLayoutPanel2.TabIndex = 13;
            // 
            // camSpeed
            // 
            this.camSpeed.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.camSpeed.DecimalPlaces = 2;
            this.camSpeed.Increment = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this.camSpeed.Location = new System.Drawing.Point(97, 3);
            this.camSpeed.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this.camSpeed.Name = "camSpeed";
            this.camSpeed.Size = new System.Drawing.Size(88, 20);
            this.camSpeed.TabIndex = 12;
            this.camSpeed.Value = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this.camSpeed.ValueChanged += new System.EventHandler(this.camSpeed_ValueChanged);
            // 
            // farGuess
            // 
            this.farGuess.Location = new System.Drawing.Point(97, 163);
            this.farGuess.Name = "farGuess";
            this.farGuess.Size = new System.Drawing.Size(88, 20);
            this.farGuess.TabIndex = 20;
            this.farGuess.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.camGuess_KeyPress);
            // 
            // nearGuess
            // 
            this.nearGuess.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.nearGuess.Location = new System.Drawing.Point(97, 137);
            this.nearGuess.Name = "nearGuess";
            this.nearGuess.Size = new System.Drawing.Size(88, 20);
            this.nearGuess.TabIndex = 19;
            this.nearGuess.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.camGuess_KeyPress);
            // 
            // aspectGuess
            // 
            this.aspectGuess.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.aspectGuess.Location = new System.Drawing.Point(97, 111);
            this.aspectGuess.Name = "aspectGuess";
            this.aspectGuess.Size = new System.Drawing.Size(88, 20);
            this.aspectGuess.TabIndex = 18;
            this.aspectGuess.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.camGuess_KeyPress);
            // 
            // fovGuess
            // 
            this.fovGuess.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.fovGuess.Location = new System.Drawing.Point(97, 85);
            this.fovGuess.Name = "fovGuess";
            this.fovGuess.Size = new System.Drawing.Size(88, 20);
            this.fovGuess.TabIndex = 6;
            this.fovGuess.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.camGuess_KeyPress);
            // 
            // label4
            // 
            this.label4.Anchor = System.Windows.Forms.AnchorStyles.Right;
            this.label4.Location = new System.Drawing.Point(3, 56);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(88, 24);
            this.label4.TabIndex = 13;
            this.label4.Text = "Matrix Type";
            this.label4.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tableLayoutPanel2.SetColumnSpan(this.label2, 2);
            this.label2.Location = new System.Drawing.Point(3, 26);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(182, 29);
            this.label2.TabIndex = 21;
            this.label2.Text = "Manually set auto-guessed values\r\nBlank to use automatic value";
            this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // matrixType
            // 
            this.matrixType.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.matrixType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.matrixType.FormattingEnabled = true;
            this.matrixType.Items.AddRange(new object[] {
            "Perspective",
            "Orthographic"});
            this.matrixType.Location = new System.Drawing.Point(97, 58);
            this.matrixType.Name = "matrixType";
            this.matrixType.Size = new System.Drawing.Size(88, 21);
            this.matrixType.TabIndex = 14;
            this.matrixType.SelectedIndexChanged += new System.EventHandler(this.matrixType_SelectedIndexChanged);
            // 
            // toolStrip2
            // 
            this.toolStrip2.Dock = System.Windows.Forms.DockStyle.None;
            this.toolStrip2.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip2.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.configureCam,
            this.resetCam,
            this.fitScreen,
            this.controlType,
            this.toolStripSeparator3,
            this.drawRange,
            this.toolStripSeparator4,
            this.toolStripLabel3,
            this.solidShading,
            this.wireframeDraw});
            this.toolStrip2.Location = new System.Drawing.Point(0, 0);
            this.toolStrip2.Name = "toolStrip2";
            this.toolStrip2.Size = new System.Drawing.Size(544, 25);
            this.toolStrip2.TabIndex = 6;
            this.toolStrip2.Text = "toolStrip2";
            // 
            // configureCam
            // 
            this.configureCam.CheckOnClick = true;
            this.configureCam.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.configureCam.Image = global::renderdocui.Properties.Resources.cog;
            this.configureCam.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.configureCam.Name = "configureCam";
            this.configureCam.Size = new System.Drawing.Size(23, 22);
            this.configureCam.Text = "Configure Camera";
            this.configureCam.CheckedChanged += new System.EventHandler(this.configureCam_CheckedChanged);
            // 
            // resetCam
            // 
            this.resetCam.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.resetCam.Image = global::renderdocui.Properties.Resources.arrow_undo;
            this.resetCam.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.resetCam.Name = "resetCam";
            this.resetCam.Size = new System.Drawing.Size(23, 22);
            this.resetCam.Text = "Reset Camera Position";
            this.resetCam.Click += new System.EventHandler(this.resetCam_Click);
            // 
            // fitScreen
            // 
            this.fitScreen.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.fitScreen.Image = global::renderdocui.Properties.Resources.wand;
            this.fitScreen.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.fitScreen.Name = "fitScreen";
            this.fitScreen.Size = new System.Drawing.Size(23, 22);
            this.fitScreen.Text = "Fit Camera to Geometry";
            this.fitScreen.Click += new System.EventHandler(this.fitScreen_Click);
            // 
            // controlType
            // 
            this.controlType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.controlType.Items.AddRange(new object[] {
            "Arcball",
            "WASD"});
            this.controlType.Name = "controlType";
            this.controlType.Size = new System.Drawing.Size(81, 25);
            this.controlType.SelectedIndexChanged += new System.EventHandler(this.controlType_SelectedIndexChanged);
            // 
            // toolStripSeparator3
            // 
            this.toolStripSeparator3.Name = "toolStripSeparator3";
            this.toolStripSeparator3.Size = new System.Drawing.Size(6, 25);
            // 
            // drawRange
            // 
            this.drawRange.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.drawRange.Items.AddRange(new object[] {
            "Only this draw",
            "Since last clear"});
            this.drawRange.Name = "drawRange";
            this.drawRange.Size = new System.Drawing.Size(121, 25);
            this.drawRange.SelectedIndexChanged += new System.EventHandler(this.drawRange_SelectedIndexChanged);
            // 
            // toolStripSeparator4
            // 
            this.toolStripSeparator4.Name = "toolStripSeparator4";
            this.toolStripSeparator4.Size = new System.Drawing.Size(6, 25);
            // 
            // toolStripLabel3
            // 
            this.toolStripLabel3.Name = "toolStripLabel3";
            this.toolStripLabel3.Size = new System.Drawing.Size(70, 22);
            this.toolStripLabel3.Text = "Solid Shading";
            // 
            // solidShading
            // 
            this.solidShading.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.solidShading.Items.AddRange(new object[] {
            "None",
            "Solid Colour",
            "Flat Shaded",
            "Secondary"});
            this.solidShading.Name = "solidShading";
            this.solidShading.Size = new System.Drawing.Size(121, 25);
            this.solidShading.SelectedIndexChanged += new System.EventHandler(this.solidShading_SelectedIndexChanged);
            // 
            // wireframeDraw
            // 
            this.wireframeDraw.Checked = true;
            this.wireframeDraw.CheckOnClick = true;
            this.wireframeDraw.CheckState = System.Windows.Forms.CheckState.Checked;
            this.wireframeDraw.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.wireframeDraw.Image = ((System.Drawing.Image)(resources.GetObject("wireframeDraw.Image")));
            this.wireframeDraw.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.wireframeDraw.Name = "wireframeDraw";
            this.wireframeDraw.Size = new System.Drawing.Size(61, 22);
            this.wireframeDraw.Text = "Wireframe";
            this.wireframeDraw.CheckedChanged += new System.EventHandler(this.wireframeDraw_CheckedChanged);
            // 
            // rightclickMenu
            // 
            this.rightclickMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.debugVertex,
            this.openFormat,
            this.syncViews,
            this.exportToToolStripMenuItem,
            this.setInstanceToolStripMenuItem});
            this.rightclickMenu.Name = "rightclickMenu";
            this.rightclickMenu.Size = new System.Drawing.Size(185, 114);
            this.rightclickMenu.Closed += new System.Windows.Forms.ToolStripDropDownClosedEventHandler(this.rightclickMenu_Closed);
            // 
            // debugVertex
            // 
            this.debugVertex.Image = global::renderdocui.Properties.Resources.wrench;
            this.debugVertex.Name = "debugVertex";
            this.debugVertex.Size = new System.Drawing.Size(184, 22);
            this.debugVertex.Text = "Debug Selected Vertex";
            this.debugVertex.Click += new System.EventHandler(this.debugVertex_Click);
            // 
            // openFormat
            // 
            this.openFormat.Name = "openFormat";
            this.openFormat.Size = new System.Drawing.Size(184, 22);
            this.openFormat.Text = "Set Format";
            this.openFormat.Click += new System.EventHandler(this.openFormat_Click);
            // 
            // syncViews
            // 
            this.syncViews.CheckOnClick = true;
            this.syncViews.Image = global::renderdocui.Properties.Resources.arrow_join;
            this.syncViews.Name = "syncViews";
            this.syncViews.Size = new System.Drawing.Size(184, 22);
            this.syncViews.Text = "Sync Views";
            this.syncViews.Click += new System.EventHandler(this.syncViews_Click);
            // 
            // exportToToolStripMenuItem
            // 
            this.exportToToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.cSVToolStripMenuItem,
            this.rawToolStripMenuItem});
            this.exportToToolStripMenuItem.Image = global::renderdocui.Properties.Resources.save;
            this.exportToToolStripMenuItem.Name = "exportToToolStripMenuItem";
            this.exportToToolStripMenuItem.Size = new System.Drawing.Size(184, 22);
            this.exportToToolStripMenuItem.Text = "Export To...";
            // 
            // cSVToolStripMenuItem
            // 
            this.cSVToolStripMenuItem.Name = "cSVToolStripMenuItem";
            this.cSVToolStripMenuItem.Size = new System.Drawing.Size(125, 22);
            this.cSVToolStripMenuItem.Text = "CSV";
            this.cSVToolStripMenuItem.Click += new System.EventHandler(this.CSVToolStripMenuItem_Click);
            // 
            // rawToolStripMenuItem
            // 
            this.rawToolStripMenuItem.Name = "rawToolStripMenuItem";
            this.rawToolStripMenuItem.Size = new System.Drawing.Size(125, 22);
            this.rawToolStripMenuItem.Text = "Raw Bytes";
            this.rawToolStripMenuItem.Click += new System.EventHandler(this.rawToolStripMenuItem_Click);
            // 
            // setInstanceToolStripMenuItem
            // 
            this.setInstanceToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.instanceIdx});
            this.setInstanceToolStripMenuItem.Name = "setInstanceToolStripMenuItem";
            this.setInstanceToolStripMenuItem.Size = new System.Drawing.Size(184, 22);
            this.setInstanceToolStripMenuItem.Text = "Set Instance";
            // 
            // instanceIdx
            // 
            this.instanceIdx.Name = "instanceIdx";
            this.instanceIdx.Size = new System.Drawing.Size(100, 21);
            this.instanceIdx.Text = "0";
            this.instanceIdx.KeyDown += new System.Windows.Forms.KeyEventHandler(this.instanceIdx_KeyDown);
            // 
            // previewTab
            // 
            this.previewTab.Controls.Add(this.tabPage1);
            this.previewTab.Controls.Add(this.tabPage2);
            this.previewTab.Controls.Add(this.tabPage3);
            this.previewTab.Location = new System.Drawing.Point(12, 131);
            this.previewTab.Name = "previewTab";
            this.previewTab.SelectedIndex = 0;
            this.previewTab.Size = new System.Drawing.Size(559, 313);
            this.previewTab.TabIndex = 1;
            this.previewTab.SelectedIndexChanged += new System.EventHandler(this.meshStageDraw_SelectedIndexChanged);
            // 
            // tabPage1
            // 
            this.tabPage1.Controls.Add(this.previewTable);
            this.tabPage1.Location = new System.Drawing.Point(4, 22);
            this.tabPage1.Name = "tabPage1";
            this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage1.Size = new System.Drawing.Size(551, 287);
            this.tabPage1.TabIndex = 0;
            this.tabPage1.Text = "VS Input";
            this.tabPage1.UseVisualStyleBackColor = true;
            // 
            // tabPage2
            // 
            this.tabPage2.Location = new System.Drawing.Point(4, 22);
            this.tabPage2.Name = "tabPage2";
            this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage2.Size = new System.Drawing.Size(551, 287);
            this.tabPage2.TabIndex = 1;
            this.tabPage2.Text = "VS Output";
            this.tabPage2.UseVisualStyleBackColor = true;
            // 
            // tabPage3
            // 
            this.tabPage3.Location = new System.Drawing.Point(4, 22);
            this.tabPage3.Name = "tabPage3";
            this.tabPage3.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage3.Size = new System.Drawing.Size(551, 287);
            this.tabPage3.TabIndex = 2;
            this.tabPage3.Text = "GS/DS Output";
            this.tabPage3.UseVisualStyleBackColor = true;
            // 
            // vsInBufferView
            // 
            this.vsInBufferView.AllowUserToAddRows = false;
            this.vsInBufferView.AllowUserToDeleteRows = false;
            this.vsInBufferView.AllowUserToResizeRows = false;
            this.vsInBufferView.AutoSizeRowsMode = System.Windows.Forms.DataGridViewAutoSizeRowsMode.DisplayedCells;
            this.vsInBufferView.BackgroundColor = System.Drawing.Color.White;
            dataGridViewCellStyle7.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle7.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle7.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle7.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle7.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle7.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle7.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.vsInBufferView.DefaultCellStyle = dataGridViewCellStyle7;
            this.vsInBufferView.Location = new System.Drawing.Point(12, 41);
            this.vsInBufferView.Name = "vsInBufferView";
            this.vsInBufferView.ReadOnly = true;
            this.vsInBufferView.RowHeadersVisible = false;
            this.vsInBufferView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.vsInBufferView.ShowCellErrors = false;
            this.vsInBufferView.ShowCellToolTips = false;
            this.vsInBufferView.ShowEditingIcon = false;
            this.vsInBufferView.Size = new System.Drawing.Size(246, 85);
            this.vsInBufferView.TabIndex = 10;
            this.vsInBufferView.VirtualMode = true;
            this.vsInBufferView.CellMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_MouseClick);
            this.vsInBufferView.CellValueNeeded += new System.Windows.Forms.DataGridViewCellValueEventHandler(this.bufferView_CellValueNeeded);
            this.vsInBufferView.ColumnHeaderMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_ColumnHeaderMouseClick);
            this.vsInBufferView.Scroll += new System.Windows.Forms.ScrollEventHandler(this.bufferView_Scroll);
            this.vsInBufferView.SelectionChanged += new System.EventHandler(this.bufferView_SelectionChanged);
            this.vsInBufferView.Paint += new System.Windows.Forms.PaintEventHandler(this.bufferView_Paint);
            this.vsInBufferView.Enter += new System.EventHandler(this.bufferView_EnterLeave);
            this.vsInBufferView.Leave += new System.EventHandler(this.bufferView_EnterLeave);
            // 
            // csvSaveDialog
            // 
            this.csvSaveDialog.DefaultExt = "csv";
            this.csvSaveDialog.Filter = "CSV Files (*.csv)|*.csv";
            this.csvSaveDialog.Title = "Save Buffer as CSV";
            // 
            // rawSaveDialog
            // 
            this.rawSaveDialog.DefaultExt = "raw";
            this.rawSaveDialog.Filter = "Raw Buffer Data (*.raw)|*.raw";
            this.rawSaveDialog.Title = "Save Raw Buffer";
            // 
            // vsOutBufferView
            // 
            this.vsOutBufferView.AllowUserToAddRows = false;
            this.vsOutBufferView.AllowUserToDeleteRows = false;
            this.vsOutBufferView.AllowUserToResizeRows = false;
            this.vsOutBufferView.AutoSizeRowsMode = System.Windows.Forms.DataGridViewAutoSizeRowsMode.DisplayedCells;
            this.vsOutBufferView.BackgroundColor = System.Drawing.Color.White;
            dataGridViewCellStyle8.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle8.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle8.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle8.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle8.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle8.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle8.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.vsOutBufferView.DefaultCellStyle = dataGridViewCellStyle8;
            this.vsOutBufferView.Location = new System.Drawing.Point(264, 40);
            this.vsOutBufferView.Name = "vsOutBufferView";
            this.vsOutBufferView.ReadOnly = true;
            this.vsOutBufferView.RowHeadersVisible = false;
            this.vsOutBufferView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.vsOutBufferView.ShowCellErrors = false;
            this.vsOutBufferView.ShowCellToolTips = false;
            this.vsOutBufferView.ShowEditingIcon = false;
            this.vsOutBufferView.Size = new System.Drawing.Size(206, 85);
            this.vsOutBufferView.TabIndex = 11;
            this.vsOutBufferView.VirtualMode = true;
            this.vsOutBufferView.CellMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_MouseClick);
            this.vsOutBufferView.CellValueNeeded += new System.Windows.Forms.DataGridViewCellValueEventHandler(this.bufferView_CellValueNeeded);
            this.vsOutBufferView.ColumnHeaderMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_ColumnHeaderMouseClick);
            this.vsOutBufferView.Scroll += new System.Windows.Forms.ScrollEventHandler(this.bufferView_Scroll);
            this.vsOutBufferView.SelectionChanged += new System.EventHandler(this.bufferView_SelectionChanged);
            this.vsOutBufferView.Paint += new System.Windows.Forms.PaintEventHandler(this.bufferView_Paint);
            this.vsOutBufferView.Enter += new System.EventHandler(this.bufferView_EnterLeave);
            this.vsOutBufferView.Leave += new System.EventHandler(this.bufferView_EnterLeave);
            // 
            // dockPanel
            // 
            this.dockPanel.BackColor = System.Drawing.SystemColors.ControlDark;
            this.dockPanel.DefaultFloatWindowSize = new System.Drawing.Size(800, 400);
            this.dockPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.dockPanel.DockBottomPortion = 0.5D;
            this.dockPanel.DocumentStyle = WeifenLuo.WinFormsUI.Docking.DocumentStyle.DockingWindow;
            this.dockPanel.Location = new System.Drawing.Point(3, 34);
            this.dockPanel.Name = "dockPanel";
            this.dockPanel.Size = new System.Drawing.Size(834, 462);
            dockPanelGradient7.EndColor = System.Drawing.SystemColors.ControlLight;
            dockPanelGradient7.StartColor = System.Drawing.SystemColors.ControlLight;
            autoHideStripSkin3.DockStripGradient = dockPanelGradient7;
            tabGradient15.EndColor = System.Drawing.SystemColors.Control;
            tabGradient15.StartColor = System.Drawing.SystemColors.Control;
            tabGradient15.TextColor = System.Drawing.SystemColors.ControlDarkDark;
            autoHideStripSkin3.TabGradient = tabGradient15;
            autoHideStripSkin3.TextFont = new System.Drawing.Font("Tahoma", 8.25F);
            dockPanelSkin3.AutoHideStripSkin = autoHideStripSkin3;
            tabGradient16.EndColor = System.Drawing.SystemColors.ControlLightLight;
            tabGradient16.StartColor = System.Drawing.SystemColors.ControlLightLight;
            tabGradient16.TextColor = System.Drawing.SystemColors.ControlText;
            dockPaneStripGradient3.ActiveTabGradient = tabGradient16;
            dockPanelGradient8.EndColor = System.Drawing.SystemColors.Control;
            dockPanelGradient8.StartColor = System.Drawing.SystemColors.Control;
            dockPaneStripGradient3.DockStripGradient = dockPanelGradient8;
            tabGradient17.EndColor = System.Drawing.SystemColors.ControlLight;
            tabGradient17.StartColor = System.Drawing.SystemColors.ControlLight;
            tabGradient17.TextColor = System.Drawing.SystemColors.ControlText;
            dockPaneStripGradient3.InactiveTabGradient = tabGradient17;
            dockPaneStripSkin3.DocumentGradient = dockPaneStripGradient3;
            dockPaneStripSkin3.TextFont = new System.Drawing.Font("Tahoma", 8.25F);
            tabGradient18.EndColor = System.Drawing.SystemColors.ActiveCaption;
            tabGradient18.LinearGradientMode = System.Drawing.Drawing2D.LinearGradientMode.Vertical;
            tabGradient18.StartColor = System.Drawing.SystemColors.GradientActiveCaption;
            tabGradient18.TextColor = System.Drawing.SystemColors.ActiveCaptionText;
            dockPaneStripToolWindowGradient3.ActiveCaptionGradient = tabGradient18;
            tabGradient19.EndColor = System.Drawing.SystemColors.Control;
            tabGradient19.StartColor = System.Drawing.SystemColors.Control;
            tabGradient19.TextColor = System.Drawing.SystemColors.ControlText;
            dockPaneStripToolWindowGradient3.ActiveTabGradient = tabGradient19;
            dockPanelGradient9.EndColor = System.Drawing.SystemColors.ControlLight;
            dockPanelGradient9.StartColor = System.Drawing.SystemColors.ControlLight;
            dockPaneStripToolWindowGradient3.DockStripGradient = dockPanelGradient9;
            tabGradient20.EndColor = System.Drawing.SystemColors.InactiveCaption;
            tabGradient20.LinearGradientMode = System.Drawing.Drawing2D.LinearGradientMode.Vertical;
            tabGradient20.StartColor = System.Drawing.SystemColors.GradientInactiveCaption;
            tabGradient20.TextColor = System.Drawing.SystemColors.InactiveCaptionText;
            dockPaneStripToolWindowGradient3.InactiveCaptionGradient = tabGradient20;
            tabGradient21.EndColor = System.Drawing.Color.Transparent;
            tabGradient21.StartColor = System.Drawing.Color.Transparent;
            tabGradient21.TextColor = System.Drawing.SystemColors.ControlDarkDark;
            dockPaneStripToolWindowGradient3.InactiveTabGradient = tabGradient21;
            dockPaneStripSkin3.ToolWindowGradient = dockPaneStripToolWindowGradient3;
            dockPanelSkin3.DockPaneStripSkin = dockPaneStripSkin3;
            this.dockPanel.Skin = dockPanelSkin3;
            this.dockPanel.TabIndex = 12;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.flowLayoutPanel2, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.dockPanel, 0, 1);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 2;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(840, 499);
            this.tableLayoutPanel1.TabIndex = 13;
            // 
            // flowLayoutPanel2
            // 
            this.flowLayoutPanel2.AutoSize = true;
            this.flowLayoutPanel2.Controls.Add(this.toolStrip1);
            this.flowLayoutPanel2.Location = new System.Drawing.Point(3, 3);
            this.flowLayoutPanel2.Name = "flowLayoutPanel2";
            this.flowLayoutPanel2.Size = new System.Drawing.Size(728, 25);
            this.flowLayoutPanel2.TabIndex = 0;
            // 
            // toolStrip1
            // 
            this.toolStrip1.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
            this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripLabel1,
            this.toolStripSeparator1,
            this.exportToolItem,
            this.syncViewsToolItem,
            this.highlightVerts,
            this.debugSep,
            this.debugVertexToolItem,
            this.toolStripSeparator2,
            this.toolStripLabel2,
            this.rowOffset,
            this.byteOffsLab,
            this.byteOffset,
            this.offsetCancel,
            this.instSep,
            this.instLabel,
            this.instanceIdxToolitem});
            this.toolStrip1.Location = new System.Drawing.Point(0, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.Size = new System.Drawing.Size(728, 25);
            this.toolStrip1.TabIndex = 0;
            this.toolStrip1.Text = "toolStrip1";
            // 
            // toolStripLabel1
            // 
            this.toolStripLabel1.Name = "toolStripLabel1";
            this.toolStripLabel1.Size = new System.Drawing.Size(47, 22);
            this.toolStripLabel1.Text = "Controls";
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(6, 25);
            // 
            // exportToolItem
            // 
            this.exportToolItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.exportToolItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.exportToCSVToolStripMenuItem,
            this.exportRawBytesToolStripMenuItem});
            this.exportToolItem.Image = global::renderdocui.Properties.Resources.save;
            this.exportToolItem.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.exportToolItem.Name = "exportToolItem";
            this.exportToolItem.Size = new System.Drawing.Size(29, 22);
            this.exportToolItem.Text = "toolStripDropDownButton1";
            // 
            // exportToCSVToolStripMenuItem
            // 
            this.exportToCSVToolStripMenuItem.Name = "exportToCSVToolStripMenuItem";
            this.exportToCSVToolStripMenuItem.Size = new System.Drawing.Size(157, 22);
            this.exportToCSVToolStripMenuItem.Text = "Export to CSV";
            this.exportToCSVToolStripMenuItem.Click += new System.EventHandler(this.CSVToolStripMenuItem_Click);
            // 
            // exportRawBytesToolStripMenuItem
            // 
            this.exportRawBytesToolStripMenuItem.Name = "exportRawBytesToolStripMenuItem";
            this.exportRawBytesToolStripMenuItem.Size = new System.Drawing.Size(157, 22);
            this.exportRawBytesToolStripMenuItem.Text = "Export raw bytes";
            this.exportRawBytesToolStripMenuItem.Click += new System.EventHandler(this.rawToolStripMenuItem_Click);
            // 
            // syncViewsToolItem
            // 
            this.syncViewsToolItem.CheckOnClick = true;
            this.syncViewsToolItem.Image = global::renderdocui.Properties.Resources.arrow_join;
            this.syncViewsToolItem.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.syncViewsToolItem.Name = "syncViewsToolItem";
            this.syncViewsToolItem.Size = new System.Drawing.Size(80, 22);
            this.syncViewsToolItem.Text = "Sync Views";
            this.syncViewsToolItem.Click += new System.EventHandler(this.syncViewsToolItem_Click);
            // 
            // highlightVerts
            // 
            this.highlightVerts.Checked = true;
            this.highlightVerts.CheckOnClick = true;
            this.highlightVerts.CheckState = System.Windows.Forms.CheckState.Checked;
            this.highlightVerts.Image = global::renderdocui.Properties.Resources.asterisk_orange;
            this.highlightVerts.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.highlightVerts.Name = "highlightVerts";
            this.highlightVerts.Size = new System.Drawing.Size(109, 22);
            this.highlightVerts.Text = "Highlight Vertices";
            this.highlightVerts.CheckedChanged += new System.EventHandler(this.highlightVerts_CheckedChanged);
            // 
            // debugSep
            // 
            this.debugSep.Name = "debugSep";
            this.debugSep.Size = new System.Drawing.Size(6, 25);
            // 
            // debugVertexToolItem
            // 
            this.debugVertexToolItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.debugVertexToolItem.Enabled = false;
            this.debugVertexToolItem.Image = global::renderdocui.Properties.Resources.wrench;
            this.debugVertexToolItem.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.debugVertexToolItem.Name = "debugVertexToolItem";
            this.debugVertexToolItem.Size = new System.Drawing.Size(23, 22);
            this.debugVertexToolItem.Text = "Debug this Vertex";
            this.debugVertexToolItem.Click += new System.EventHandler(this.debugVertex_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(6, 25);
            // 
            // toolStripLabel2
            // 
            this.toolStripLabel2.Name = "toolStripLabel2";
            this.toolStripLabel2.Size = new System.Drawing.Size(62, 22);
            this.toolStripLabel2.Text = "Row Offset";
            // 
            // rowOffset
            // 
            this.rowOffset.Name = "rowOffset";
            this.rowOffset.Size = new System.Drawing.Size(50, 25);
            this.rowOffset.Text = "0";
            this.rowOffset.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.rowOffset_KeyPress);
            // 
            // byteOffsLab
            // 
            this.byteOffsLab.Name = "byteOffsLab";
            this.byteOffsLab.Size = new System.Drawing.Size(63, 22);
            this.byteOffsLab.Text = "Byte Offset";
            // 
            // byteOffset
            // 
            this.byteOffset.Name = "byteOffset";
            this.byteOffset.Size = new System.Drawing.Size(60, 25);
            this.byteOffset.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.byteOffset_KeyPress);
            // 
            // offsetCancel
            // 
            this.offsetCancel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.offsetCancel.Image = global::renderdocui.Properties.Resources.red_x_16;
            this.offsetCancel.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.offsetCancel.Name = "offsetCancel";
            this.offsetCancel.Size = new System.Drawing.Size(23, 22);
            this.offsetCancel.Text = "Reset";
            this.offsetCancel.Click += new System.EventHandler(this.offsetCancel_Click);
            // 
            // instSep
            // 
            this.instSep.Name = "instSep";
            this.instSep.Size = new System.Drawing.Size(6, 25);
            // 
            // instLabel
            // 
            this.instLabel.Name = "instLabel";
            this.instLabel.Size = new System.Drawing.Size(49, 22);
            this.instLabel.Text = "Instance";
            // 
            // instanceIdxToolitem
            // 
            this.instanceIdxToolitem.Name = "instanceIdxToolitem";
            this.instanceIdxToolitem.Size = new System.Drawing.Size(100, 25);
            this.instanceIdxToolitem.Text = "0";
            this.instanceIdxToolitem.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.instanceIdxToolitem_KeyPress);
            // 
            // gsOutBufferView
            // 
            this.gsOutBufferView.AllowUserToAddRows = false;
            this.gsOutBufferView.AllowUserToDeleteRows = false;
            this.gsOutBufferView.AllowUserToResizeRows = false;
            this.gsOutBufferView.AutoSizeRowsMode = System.Windows.Forms.DataGridViewAutoSizeRowsMode.DisplayedCells;
            this.gsOutBufferView.BackgroundColor = System.Drawing.Color.White;
            dataGridViewCellStyle9.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle9.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle9.Font = new System.Drawing.Font("Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle9.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle9.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle9.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle9.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.gsOutBufferView.DefaultCellStyle = dataGridViewCellStyle9;
            this.gsOutBufferView.Location = new System.Drawing.Point(476, 40);
            this.gsOutBufferView.Name = "gsOutBufferView";
            this.gsOutBufferView.ReadOnly = true;
            this.gsOutBufferView.RowHeadersVisible = false;
            this.gsOutBufferView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.gsOutBufferView.ShowCellErrors = false;
            this.gsOutBufferView.ShowCellToolTips = false;
            this.gsOutBufferView.ShowEditingIcon = false;
            this.gsOutBufferView.Size = new System.Drawing.Size(206, 85);
            this.gsOutBufferView.TabIndex = 14;
            this.gsOutBufferView.VirtualMode = true;
            this.gsOutBufferView.CellMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_MouseClick);
            this.gsOutBufferView.CellValueNeeded += new System.Windows.Forms.DataGridViewCellValueEventHandler(this.bufferView_CellValueNeeded);
            this.gsOutBufferView.ColumnHeaderMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.bufferView_ColumnHeaderMouseClick);
            this.gsOutBufferView.Scroll += new System.Windows.Forms.ScrollEventHandler(this.bufferView_Scroll);
            this.gsOutBufferView.SelectionChanged += new System.EventHandler(this.bufferView_SelectionChanged);
            this.gsOutBufferView.Paint += new System.Windows.Forms.PaintEventHandler(this.bufferView_Paint);
            this.gsOutBufferView.Enter += new System.EventHandler(this.bufferView_EnterLeave);
            this.gsOutBufferView.Leave += new System.EventHandler(this.bufferView_EnterLeave);
            // 
            // columnContextMenu
            // 
            this.columnContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.resetSelectedColumnsToolStripMenuItem,
            this.toolStripSeparator5,
            this.selectColumnAsPositionToolStripMenuItem,
            this.selectColumnAsSecondaryToolStripMenuItem,
            this.selectAlphaAsSecondaryToolStripMenuItem});
            this.columnContextMenu.Name = "columnContextMenu";
            this.columnContextMenu.Size = new System.Drawing.Size(202, 98);
            // 
            // resetSelectedColumnsToolStripMenuItem
            // 
            this.resetSelectedColumnsToolStripMenuItem.Name = "resetSelectedColumnsToolStripMenuItem";
            this.resetSelectedColumnsToolStripMenuItem.Size = new System.Drawing.Size(201, 22);
            this.resetSelectedColumnsToolStripMenuItem.Text = "Reset Selected Columns";
            this.resetSelectedColumnsToolStripMenuItem.Click += new System.EventHandler(this.resetSelectedColumnsToolStripMenuItem_Click);
            // 
            // toolStripSeparator5
            // 
            this.toolStripSeparator5.Name = "toolStripSeparator5";
            this.toolStripSeparator5.Size = new System.Drawing.Size(198, 6);
            // 
            // selectColumnAsPositionToolStripMenuItem
            // 
            this.selectColumnAsPositionToolStripMenuItem.Name = "selectColumnAsPositionToolStripMenuItem";
            this.selectColumnAsPositionToolStripMenuItem.Size = new System.Drawing.Size(201, 22);
            this.selectColumnAsPositionToolStripMenuItem.Text = "Select as Position";
            this.selectColumnAsPositionToolStripMenuItem.Click += new System.EventHandler(this.selectColumnAsPositionToolStripMenuItem_Click);
            // 
            // selectColumnAsSecondaryToolStripMenuItem
            // 
            this.selectColumnAsSecondaryToolStripMenuItem.Name = "selectColumnAsSecondaryToolStripMenuItem";
            this.selectColumnAsSecondaryToolStripMenuItem.Size = new System.Drawing.Size(201, 22);
            this.selectColumnAsSecondaryToolStripMenuItem.Text = "Select as Secondary";
            this.selectColumnAsSecondaryToolStripMenuItem.Click += new System.EventHandler(this.selectColumnAsSecondaryToolStripMenuItem_Click);
            // 
            // selectAlphaAsSecondaryToolStripMenuItem
            // 
            this.selectAlphaAsSecondaryToolStripMenuItem.Name = "selectAlphaAsSecondaryToolStripMenuItem";
            this.selectAlphaAsSecondaryToolStripMenuItem.Size = new System.Drawing.Size(201, 22);
            this.selectAlphaAsSecondaryToolStripMenuItem.Text = "Select Alpha as Secondary";
            this.selectAlphaAsSecondaryToolStripMenuItem.Click += new System.EventHandler(this.selectAlphaAsSecondaryToolStripMenuItem_Click);
            // 
            // BufferViewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(840, 499);
            this.Controls.Add(this.gsOutBufferView);
            this.Controls.Add(this.vsOutBufferView);
            this.Controls.Add(this.vsInBufferView);
            this.Controls.Add(this.previewTab);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "BufferViewer";
            this.Text = "BufferViewer";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.BufferViewer_FormClosed);
            this.Load += new System.EventHandler(this.BufferViewer_Load);
            this.previewTable.ResumeLayout(false);
            this.previewTable.PerformLayout();
            this.tableLayoutPanel3.ResumeLayout(false);
            this.configCamControls.ResumeLayout(false);
            this.tableLayoutPanel2.ResumeLayout(false);
            this.tableLayoutPanel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.camSpeed)).EndInit();
            this.toolStrip2.ResumeLayout(false);
            this.toolStrip2.PerformLayout();
            this.rightclickMenu.ResumeLayout(false);
            this.previewTab.ResumeLayout(false);
            this.tabPage1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.vsInBufferView)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.vsOutBufferView)).EndInit();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.flowLayoutPanel2.ResumeLayout(false);
            this.flowLayoutPanel2.PerformLayout();
            this.toolStrip1.ResumeLayout(false);
            this.toolStrip1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.gsOutBufferView)).EndInit();
            this.columnContextMenu.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ContextMenuStrip rightclickMenu;
        private System.Windows.Forms.ToolStripMenuItem openFormat;
        private System.Windows.Forms.ToolStripMenuItem exportToToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem cSVToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem rawToolStripMenuItem;
        private System.Windows.Forms.TabControl previewTab;
        private System.Windows.Forms.TabPage tabPage1;
        private System.Windows.Forms.DataGridView vsInBufferView;
        private System.Windows.Forms.TabPage tabPage2;
        private System.Windows.Forms.SaveFileDialog csvSaveDialog;
        private System.Windows.Forms.SaveFileDialog rawSaveDialog;
        private Controls.NoScrollPanel render;
        private System.Windows.Forms.ToolStripMenuItem setInstanceToolStripMenuItem;
        private System.Windows.Forms.ToolStripTextBox instanceIdx;
        private System.Windows.Forms.ToolStripMenuItem debugVertex;
        private System.Windows.Forms.TableLayoutPanel previewTable;
        private System.Windows.Forms.DataGridView vsOutBufferView;
        private WeifenLuo.WinFormsUI.Docking.DockPanel dockPanel;
        private System.Windows.Forms.ToolStripMenuItem syncViews;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
        private System.Windows.Forms.ToolStrip toolStrip1;
        private System.Windows.Forms.ToolStripLabel toolStripLabel1;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripDropDownButton exportToolItem;
        private System.Windows.Forms.ToolStripMenuItem exportToCSVToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exportRawBytesToolStripMenuItem;
        private System.Windows.Forms.ToolStripButton syncViewsToolItem;
        private System.Windows.Forms.ToolStripSeparator debugSep;
        private System.Windows.Forms.ToolStripButton debugVertexToolItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripLabel toolStripLabel2;
        private System.Windows.Forms.ToolStripTextBox rowOffset;
        private System.Windows.Forms.ToolStripButton offsetCancel;
        private System.Windows.Forms.ToolStripSeparator instSep;
        private System.Windows.Forms.ToolStripLabel instLabel;
        private System.Windows.Forms.ToolStripTextBox instanceIdxToolitem;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
        private System.Windows.Forms.GroupBox configCamControls;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
        private System.Windows.Forms.TextBox nearGuess;
        private System.Windows.Forms.TextBox aspectGuess;
        private System.Windows.Forms.NumericUpDown camSpeed;
        private System.Windows.Forms.TextBox fovGuess;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.ComboBox matrixType;
        private System.Windows.Forms.TextBox farGuess;
        private System.Windows.Forms.ToolStrip toolStrip2;
        private System.Windows.Forms.ToolStripButton configureCam;
        private System.Windows.Forms.ToolStripComboBox controlType;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
        private System.Windows.Forms.ToolStripComboBox drawRange;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
        private System.Windows.Forms.ToolStripLabel toolStripLabel3;
        private System.Windows.Forms.ToolStripComboBox solidShading;
        private System.Windows.Forms.ToolStripButton wireframeDraw;
        private System.Windows.Forms.ToolStripButton resetCam;
        private System.Windows.Forms.ToolStripButton fitScreen;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.ToolStripLabel byteOffsLab;
        private System.Windows.Forms.ToolStripTextBox byteOffset;
        private System.Windows.Forms.TabPage tabPage3;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.DataGridView gsOutBufferView;
        private System.Windows.Forms.ToolStripButton highlightVerts;
        private System.Windows.Forms.ContextMenuStrip columnContextMenu;
        private System.Windows.Forms.ToolStripMenuItem resetSelectedColumnsToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
        private System.Windows.Forms.ToolStripMenuItem selectColumnAsPositionToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem selectColumnAsSecondaryToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem selectAlphaAsSecondaryToolStripMenuItem;

    }
}