namespace renderdocui.Windows.Dialogs
{
    partial class TextureSaveDialog
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
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel3;
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label4;
            System.Windows.Forms.Label label5;
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel6;
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel4;
            System.Windows.Forms.FlowLayoutPanel flowLayoutPanel5;
            System.Windows.Forms.GroupBox groupBox2;
            System.Windows.Forms.Button browse;
            this.fileFormat = new System.Windows.Forms.ComboBox();
            this.jpegCompression = new System.Windows.Forms.NumericUpDown();
            this.ok = new System.Windows.Forms.Button();
            this.cancel = new System.Windows.Forms.Button();
            this.alphaLDRGroup = new System.Windows.Forms.GroupBox();
            this.alphaMap = new System.Windows.Forms.ComboBox();
            this.alphaCol = new System.Windows.Forms.Button();
            this.blackPoint = new System.Windows.Forms.TextBox();
            this.whitePoint = new System.Windows.Forms.TextBox();
            this.sliceGroup = new System.Windows.Forms.GroupBox();
            this.exportAllSlices = new System.Windows.Forms.CheckBox();
            this.oneSlice = new System.Windows.Forms.CheckBox();
            this.sliceSelect = new System.Windows.Forms.ComboBox();
            this.mapSlicesToGrid = new System.Windows.Forms.CheckBox();
            this.gridWidth = new System.Windows.Forms.NumericUpDown();
            this.cubeCruciform = new System.Windows.Forms.CheckBox();
            this.mipGroup = new System.Windows.Forms.GroupBox();
            this.exportAllMips = new System.Windows.Forms.CheckBox();
            this.oneMip = new System.Windows.Forms.CheckBox();
            this.mipSelect = new System.Windows.Forms.ComboBox();
            this.sampleGroup = new System.Windows.Forms.GroupBox();
            this.mapSampleArray = new System.Windows.Forms.CheckBox();
            this.resolveSamples = new System.Windows.Forms.CheckBox();
            this.oneSample = new System.Windows.Forms.CheckBox();
            this.sampleSelect = new System.Windows.Forms.ComboBox();
            this.filename = new System.Windows.Forms.TextBox();
            this.colorDialog = new System.Windows.Forms.ColorDialog();
            this.saveTexDialog = new System.Windows.Forms.SaveFileDialog();
            this.typingTimer = new System.Windows.Forms.Timer(this.components);
            tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            groupBox1 = new System.Windows.Forms.GroupBox();
            flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
            flowLayoutPanel3 = new System.Windows.Forms.FlowLayoutPanel();
            label3 = new System.Windows.Forms.Label();
            label4 = new System.Windows.Forms.Label();
            label5 = new System.Windows.Forms.Label();
            flowLayoutPanel6 = new System.Windows.Forms.FlowLayoutPanel();
            flowLayoutPanel4 = new System.Windows.Forms.FlowLayoutPanel();
            flowLayoutPanel5 = new System.Windows.Forms.FlowLayoutPanel();
            groupBox2 = new System.Windows.Forms.GroupBox();
            browse = new System.Windows.Forms.Button();
            tableLayoutPanel1.SuspendLayout();
            groupBox1.SuspendLayout();
            flowLayoutPanel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.jpegCompression)).BeginInit();
            flowLayoutPanel2.SuspendLayout();
            this.alphaLDRGroup.SuspendLayout();
            flowLayoutPanel3.SuspendLayout();
            this.sliceGroup.SuspendLayout();
            flowLayoutPanel6.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.gridWidth)).BeginInit();
            this.mipGroup.SuspendLayout();
            flowLayoutPanel4.SuspendLayout();
            this.sampleGroup.SuspendLayout();
            flowLayoutPanel5.SuspendLayout();
            groupBox2.SuspendLayout();
            this.SuspendLayout();
            // 
            // tableLayoutPanel1
            // 
            tableLayoutPanel1.AutoSize = true;
            tableLayoutPanel1.ColumnCount = 1;
            tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            tableLayoutPanel1.Controls.Add(groupBox1, 0, 1);
            tableLayoutPanel1.Controls.Add(flowLayoutPanel2, 0, 6);
            tableLayoutPanel1.Controls.Add(this.alphaLDRGroup, 0, 5);
            tableLayoutPanel1.Controls.Add(this.sliceGroup, 0, 4);
            tableLayoutPanel1.Controls.Add(this.mipGroup, 0, 2);
            tableLayoutPanel1.Controls.Add(this.sampleGroup, 0, 3);
            tableLayoutPanel1.Controls.Add(groupBox2, 0, 0);
            tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            tableLayoutPanel1.Name = "tableLayoutPanel1";
            tableLayoutPanel1.RowCount = 7;
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 50F));
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            tableLayoutPanel1.Size = new System.Drawing.Size(378, 555);
            tableLayoutPanel1.TabIndex = 0;
            // 
            // groupBox1
            // 
            groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox1.AutoSize = true;
            groupBox1.Controls.Add(flowLayoutPanel1);
            groupBox1.Location = new System.Drawing.Point(3, 53);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(372, 72);
            groupBox1.TabIndex = 0;
            groupBox1.TabStop = false;
            groupBox1.Text = "File Format";
            // 
            // flowLayoutPanel1
            // 
            flowLayoutPanel1.AutoSize = true;
            flowLayoutPanel1.Controls.Add(label1);
            flowLayoutPanel1.Controls.Add(this.fileFormat);
            flowLayoutPanel1.Controls.Add(label2);
            flowLayoutPanel1.Controls.Add(this.jpegCompression);
            flowLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            flowLayoutPanel1.Location = new System.Drawing.Point(3, 16);
            flowLayoutPanel1.Name = "flowLayoutPanel1";
            flowLayoutPanel1.Size = new System.Drawing.Size(366, 53);
            flowLayoutPanel1.TabIndex = 0;
            // 
            // label1
            // 
            label1.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(3, 7);
            label1.MinimumSize = new System.Drawing.Size(150, 0);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(150, 13);
            label1.TabIndex = 1;
            label1.Text = "File Format:";
            label1.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // fileFormat
            // 
            this.fileFormat.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            flowLayoutPanel1.SetFlowBreak(this.fileFormat, true);
            this.fileFormat.FormattingEnabled = true;
            this.fileFormat.Location = new System.Drawing.Point(159, 3);
            this.fileFormat.Name = "fileFormat";
            this.fileFormat.Size = new System.Drawing.Size(121, 21);
            this.fileFormat.TabIndex = 3;
            this.fileFormat.SelectedIndexChanged += new System.EventHandler(this.fileFormat_SelectedIndexChanged);
            // 
            // label2
            // 
            label2.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(3, 33);
            label2.MinimumSize = new System.Drawing.Size(150, 0);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(150, 13);
            label2.TabIndex = 2;
            label2.Text = "JPEG Compression:";
            label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // jpegCompression
            // 
            this.jpegCompression.Location = new System.Drawing.Point(159, 30);
            this.jpegCompression.Name = "jpegCompression";
            this.jpegCompression.Size = new System.Drawing.Size(120, 20);
            this.jpegCompression.TabIndex = 4;
            this.jpegCompression.ValueChanged += new System.EventHandler(this.jpegCompression_ValueChanged);
            // 
            // flowLayoutPanel2
            // 
            flowLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            flowLayoutPanel2.AutoSize = true;
            flowLayoutPanel2.Controls.Add(this.ok);
            flowLayoutPanel2.Controls.Add(this.cancel);
            flowLayoutPanel2.Location = new System.Drawing.Point(213, 523);
            flowLayoutPanel2.Name = "flowLayoutPanel2";
            flowLayoutPanel2.Size = new System.Drawing.Size(162, 29);
            flowLayoutPanel2.TabIndex = 1;
            // 
            // ok
            // 
            this.ok.Location = new System.Drawing.Point(3, 3);
            this.ok.Name = "ok";
            this.ok.Size = new System.Drawing.Size(75, 23);
            this.ok.TabIndex = 22;
            this.ok.Text = "Save";
            this.ok.UseVisualStyleBackColor = true;
            this.ok.Click += new System.EventHandler(this.ok_Click);
            // 
            // cancel
            // 
            this.cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancel.Location = new System.Drawing.Point(84, 3);
            this.cancel.Name = "cancel";
            this.cancel.Size = new System.Drawing.Size(75, 23);
            this.cancel.TabIndex = 23;
            this.cancel.Text = "Cancel";
            this.cancel.UseVisualStyleBackColor = true;
            // 
            // alphaLDRGroup
            // 
            this.alphaLDRGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.alphaLDRGroup.AutoSize = true;
            this.alphaLDRGroup.Controls.Add(flowLayoutPanel3);
            this.alphaLDRGroup.Location = new System.Drawing.Point(3, 428);
            this.alphaLDRGroup.Name = "alphaLDRGroup";
            this.alphaLDRGroup.Size = new System.Drawing.Size(372, 79);
            this.alphaLDRGroup.TabIndex = 2;
            this.alphaLDRGroup.TabStop = false;
            this.alphaLDRGroup.Text = "Alpha Handling";
            // 
            // flowLayoutPanel3
            // 
            flowLayoutPanel3.AutoSize = true;
            flowLayoutPanel3.Controls.Add(label3);
            flowLayoutPanel3.Controls.Add(this.alphaMap);
            flowLayoutPanel3.Controls.Add(this.alphaCol);
            flowLayoutPanel3.Controls.Add(label4);
            flowLayoutPanel3.Controls.Add(this.blackPoint);
            flowLayoutPanel3.Controls.Add(label5);
            flowLayoutPanel3.Controls.Add(this.whitePoint);
            flowLayoutPanel3.Dock = System.Windows.Forms.DockStyle.Fill;
            flowLayoutPanel3.Location = new System.Drawing.Point(3, 16);
            flowLayoutPanel3.Name = "flowLayoutPanel3";
            flowLayoutPanel3.Size = new System.Drawing.Size(366, 60);
            flowLayoutPanel3.TabIndex = 0;
            // 
            // label3
            // 
            label3.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(3, 7);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(80, 13);
            label3.TabIndex = 1;
            label3.Text = "Alpha mapping:";
            label3.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // alphaMap
            // 
            this.alphaMap.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.alphaMap.FormattingEnabled = true;
            this.alphaMap.Items.AddRange(new object[] {
            "Discard",
            "Blend to Colour",
            "Blend to Checkerboard"});
            this.alphaMap.Location = new System.Drawing.Point(89, 3);
            this.alphaMap.Name = "alphaMap";
            this.alphaMap.Size = new System.Drawing.Size(148, 21);
            this.alphaMap.TabIndex = 18;
            this.alphaMap.SelectedIndexChanged += new System.EventHandler(this.alphaMap_SelectedIndexChanged);
            // 
            // alphaCol
            // 
            flowLayoutPanel3.SetFlowBreak(this.alphaCol, true);
            this.alphaCol.Location = new System.Drawing.Point(243, 3);
            this.alphaCol.Name = "alphaCol";
            this.alphaCol.Size = new System.Drawing.Size(110, 21);
            this.alphaCol.TabIndex = 19;
            this.alphaCol.Text = "Background Colour";
            this.alphaCol.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.alphaCol.UseVisualStyleBackColor = true;
            this.alphaCol.Click += new System.EventHandler(this.alphaCol_Click);
            // 
            // label4
            // 
            label4.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label4.AutoSize = true;
            label4.Location = new System.Drawing.Point(3, 40);
            label4.Margin = new System.Windows.Forms.Padding(3, 7, 3, 0);
            label4.Name = "label4";
            label4.Size = new System.Drawing.Size(64, 13);
            label4.TabIndex = 3;
            label4.Text = "Black Point:";
            label4.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // blackPoint
            // 
            this.blackPoint.Location = new System.Drawing.Point(73, 37);
            this.blackPoint.Margin = new System.Windows.Forms.Padding(3, 10, 3, 3);
            this.blackPoint.Name = "blackPoint";
            this.blackPoint.Size = new System.Drawing.Size(100, 20);
            this.blackPoint.TabIndex = 20;
            this.blackPoint.TextChanged += new System.EventHandler(this.blackPoint_TextChanged);
            // 
            // label5
            // 
            label5.Anchor = System.Windows.Forms.AnchorStyles.Right;
            label5.AutoSize = true;
            label5.Location = new System.Drawing.Point(179, 40);
            label5.Margin = new System.Windows.Forms.Padding(3, 7, 3, 0);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(65, 13);
            label5.TabIndex = 4;
            label5.Text = "White Point:";
            label5.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // whitePoint
            // 
            this.whitePoint.Location = new System.Drawing.Point(250, 37);
            this.whitePoint.Margin = new System.Windows.Forms.Padding(3, 10, 3, 3);
            this.whitePoint.Name = "whitePoint";
            this.whitePoint.Size = new System.Drawing.Size(100, 20);
            this.whitePoint.TabIndex = 21;
            this.whitePoint.TextChanged += new System.EventHandler(this.whitePoint_TextChanged);
            // 
            // sliceGroup
            // 
            this.sliceGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.sliceGroup.AutoSize = true;
            this.sliceGroup.Controls.Add(flowLayoutPanel6);
            this.sliceGroup.Location = new System.Drawing.Point(3, 304);
            this.sliceGroup.Name = "sliceGroup";
            this.sliceGroup.Size = new System.Drawing.Size(372, 118);
            this.sliceGroup.TabIndex = 5;
            this.sliceGroup.TabStop = false;
            this.sliceGroup.Text = "Array/Depth Slices";
            // 
            // flowLayoutPanel6
            // 
            flowLayoutPanel6.AutoSize = true;
            flowLayoutPanel6.Controls.Add(this.exportAllSlices);
            flowLayoutPanel6.Controls.Add(this.oneSlice);
            flowLayoutPanel6.Controls.Add(this.sliceSelect);
            flowLayoutPanel6.Controls.Add(this.mapSlicesToGrid);
            flowLayoutPanel6.Controls.Add(this.gridWidth);
            flowLayoutPanel6.Controls.Add(this.cubeCruciform);
            flowLayoutPanel6.Dock = System.Windows.Forms.DockStyle.Fill;
            flowLayoutPanel6.Location = new System.Drawing.Point(3, 16);
            flowLayoutPanel6.Name = "flowLayoutPanel6";
            flowLayoutPanel6.Size = new System.Drawing.Size(366, 99);
            flowLayoutPanel6.TabIndex = 0;
            // 
            // exportAllSlices
            // 
            this.exportAllSlices.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            flowLayoutPanel6.SetFlowBreak(this.exportAllSlices, true);
            this.exportAllSlices.Location = new System.Drawing.Point(3, 3);
            this.exportAllSlices.MinimumSize = new System.Drawing.Size(120, 0);
            this.exportAllSlices.Name = "exportAllSlices";
            this.exportAllSlices.Size = new System.Drawing.Size(120, 17);
            this.exportAllSlices.TabIndex = 12;
            this.exportAllSlices.Text = "Export All Slices";
            this.exportAllSlices.UseVisualStyleBackColor = true;
            this.exportAllSlices.CheckedChanged += new System.EventHandler(this.exportAllSlices_CheckedChanged);
            // 
            // oneSlice
            // 
            this.oneSlice.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.oneSlice.AutoSize = true;
            this.oneSlice.Location = new System.Drawing.Point(3, 28);
            this.oneSlice.MinimumSize = new System.Drawing.Size(120, 0);
            this.oneSlice.Name = "oneSlice";
            this.oneSlice.Size = new System.Drawing.Size(120, 17);
            this.oneSlice.TabIndex = 13;
            this.oneSlice.Text = "Select Slice:";
            this.oneSlice.UseVisualStyleBackColor = true;
            this.oneSlice.CheckedChanged += new System.EventHandler(this.oneSlice_CheckedChanged);
            // 
            // sliceSelect
            // 
            this.sliceSelect.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            flowLayoutPanel6.SetFlowBreak(this.sliceSelect, true);
            this.sliceSelect.FormattingEnabled = true;
            this.sliceSelect.Location = new System.Drawing.Point(129, 26);
            this.sliceSelect.Name = "sliceSelect";
            this.sliceSelect.Size = new System.Drawing.Size(121, 21);
            this.sliceSelect.TabIndex = 14;
            this.sliceSelect.SelectedIndexChanged += new System.EventHandler(this.sliceSelect_SelectedIndexChanged);
            // 
            // mapSlicesToGrid
            // 
            this.mapSlicesToGrid.AutoSize = true;
            this.mapSlicesToGrid.Location = new System.Drawing.Point(3, 53);
            this.mapSlicesToGrid.Name = "mapSlicesToGrid";
            this.mapSlicesToGrid.Size = new System.Drawing.Size(179, 17);
            this.mapSlicesToGrid.TabIndex = 15;
            this.mapSlicesToGrid.Text = "Show Slices as Grid. Grid Width:";
            this.mapSlicesToGrid.UseVisualStyleBackColor = true;
            this.mapSlicesToGrid.CheckedChanged += new System.EventHandler(this.mapSlicesToGrid_CheckedChanged);
            // 
            // gridWidth
            // 
            this.gridWidth.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            flowLayoutPanel6.SetFlowBreak(this.gridWidth, true);
            this.gridWidth.Location = new System.Drawing.Point(188, 53);
            this.gridWidth.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.gridWidth.Name = "gridWidth";
            this.gridWidth.Size = new System.Drawing.Size(49, 20);
            this.gridWidth.TabIndex = 16;
            this.gridWidth.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.gridWidth.ValueChanged += new System.EventHandler(this.gridWidth_ValueChanged);
            // 
            // cubeCruciform
            // 
            this.cubeCruciform.AutoSize = true;
            this.cubeCruciform.Location = new System.Drawing.Point(3, 79);
            this.cubeCruciform.Name = "cubeCruciform";
            this.cubeCruciform.Size = new System.Drawing.Size(144, 17);
            this.cubeCruciform.TabIndex = 17;
            this.cubeCruciform.Text = "Show Cubemap as Cross";
            this.cubeCruciform.UseVisualStyleBackColor = true;
            this.cubeCruciform.CheckedChanged += new System.EventHandler(this.cubeCruciform_CheckedChanged);
            // 
            // mipGroup
            // 
            this.mipGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.mipGroup.AutoSize = true;
            this.mipGroup.Controls.Add(flowLayoutPanel4);
            this.mipGroup.Location = new System.Drawing.Point(3, 131);
            this.mipGroup.Name = "mipGroup";
            this.mipGroup.Size = new System.Drawing.Size(372, 69);
            this.mipGroup.TabIndex = 3;
            this.mipGroup.TabStop = false;
            this.mipGroup.Text = "Mips";
            // 
            // flowLayoutPanel4
            // 
            flowLayoutPanel4.AutoSize = true;
            flowLayoutPanel4.Controls.Add(this.exportAllMips);
            flowLayoutPanel4.Controls.Add(this.oneMip);
            flowLayoutPanel4.Controls.Add(this.mipSelect);
            flowLayoutPanel4.Dock = System.Windows.Forms.DockStyle.Fill;
            flowLayoutPanel4.Location = new System.Drawing.Point(3, 16);
            flowLayoutPanel4.Name = "flowLayoutPanel4";
            flowLayoutPanel4.Size = new System.Drawing.Size(366, 50);
            flowLayoutPanel4.TabIndex = 0;
            // 
            // exportAllMips
            // 
            this.exportAllMips.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.exportAllMips.AutoSize = true;
            flowLayoutPanel4.SetFlowBreak(this.exportAllMips, true);
            this.exportAllMips.Location = new System.Drawing.Point(3, 3);
            this.exportAllMips.MinimumSize = new System.Drawing.Size(120, 0);
            this.exportAllMips.Name = "exportAllMips";
            this.exportAllMips.Size = new System.Drawing.Size(120, 17);
            this.exportAllMips.TabIndex = 5;
            this.exportAllMips.Text = "Export All Mips";
            this.exportAllMips.UseVisualStyleBackColor = true;
            this.exportAllMips.CheckedChanged += new System.EventHandler(this.exportAllMips_CheckedChanged);
            // 
            // oneMip
            // 
            this.oneMip.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.oneMip.AutoSize = true;
            this.oneMip.Location = new System.Drawing.Point(3, 28);
            this.oneMip.MinimumSize = new System.Drawing.Size(120, 0);
            this.oneMip.Name = "oneMip";
            this.oneMip.Size = new System.Drawing.Size(120, 17);
            this.oneMip.TabIndex = 6;
            this.oneMip.Text = "Select Mip:";
            this.oneMip.UseVisualStyleBackColor = true;
            this.oneMip.CheckedChanged += new System.EventHandler(this.oneMip_CheckedChanged);
            // 
            // mipSelect
            // 
            this.mipSelect.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            flowLayoutPanel4.SetFlowBreak(this.mipSelect, true);
            this.mipSelect.FormattingEnabled = true;
            this.mipSelect.Location = new System.Drawing.Point(129, 26);
            this.mipSelect.Name = "mipSelect";
            this.mipSelect.Size = new System.Drawing.Size(121, 21);
            this.mipSelect.TabIndex = 7;
            this.mipSelect.SelectedIndexChanged += new System.EventHandler(this.mipSelect_SelectedIndexChanged);
            // 
            // sampleGroup
            // 
            this.sampleGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.sampleGroup.AutoSize = true;
            this.sampleGroup.Controls.Add(flowLayoutPanel5);
            this.sampleGroup.Location = new System.Drawing.Point(3, 206);
            this.sampleGroup.Name = "sampleGroup";
            this.sampleGroup.Size = new System.Drawing.Size(372, 92);
            this.sampleGroup.TabIndex = 6;
            this.sampleGroup.TabStop = false;
            this.sampleGroup.Text = "MSAA Samples";
            // 
            // flowLayoutPanel5
            // 
            flowLayoutPanel5.AutoSize = true;
            flowLayoutPanel5.Controls.Add(this.mapSampleArray);
            flowLayoutPanel5.Controls.Add(this.resolveSamples);
            flowLayoutPanel5.Controls.Add(this.oneSample);
            flowLayoutPanel5.Controls.Add(this.sampleSelect);
            flowLayoutPanel5.Dock = System.Windows.Forms.DockStyle.Fill;
            flowLayoutPanel5.Location = new System.Drawing.Point(3, 16);
            flowLayoutPanel5.Name = "flowLayoutPanel5";
            flowLayoutPanel5.Size = new System.Drawing.Size(366, 73);
            flowLayoutPanel5.TabIndex = 0;
            // 
            // mapSampleArray
            // 
            this.mapSampleArray.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.mapSampleArray.AutoSize = true;
            flowLayoutPanel5.SetFlowBreak(this.mapSampleArray, true);
            this.mapSampleArray.Location = new System.Drawing.Point(3, 3);
            this.mapSampleArray.Name = "mapSampleArray";
            this.mapSampleArray.Size = new System.Drawing.Size(162, 17);
            this.mapSampleArray.TabIndex = 8;
            this.mapSampleArray.Text = "Map Samples as Array Slices";
            this.mapSampleArray.UseVisualStyleBackColor = true;
            this.mapSampleArray.CheckedChanged += new System.EventHandler(this.mapSampleArray_CheckedChanged);
            // 
            // resolveSamples
            // 
            this.resolveSamples.AutoSize = true;
            flowLayoutPanel5.SetFlowBreak(this.resolveSamples, true);
            this.resolveSamples.Location = new System.Drawing.Point(3, 26);
            this.resolveSamples.Name = "resolveSamples";
            this.resolveSamples.Size = new System.Drawing.Size(108, 17);
            this.resolveSamples.TabIndex = 9;
            this.resolveSamples.Text = "Resolve Samples";
            this.resolveSamples.UseVisualStyleBackColor = true;
            this.resolveSamples.CheckedChanged += new System.EventHandler(this.resolveSamples_CheckedChanged);
            // 
            // oneSample
            // 
            this.oneSample.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.oneSample.AutoSize = true;
            this.oneSample.Location = new System.Drawing.Point(3, 51);
            this.oneSample.MinimumSize = new System.Drawing.Size(120, 0);
            this.oneSample.Name = "oneSample";
            this.oneSample.Size = new System.Drawing.Size(120, 17);
            this.oneSample.TabIndex = 10;
            this.oneSample.Text = "Select Sample:";
            this.oneSample.UseVisualStyleBackColor = true;
            this.oneSample.CheckedChanged += new System.EventHandler(this.oneSample_CheckedChanged);
            // 
            // sampleSelect
            // 
            this.sampleSelect.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            flowLayoutPanel5.SetFlowBreak(this.sampleSelect, true);
            this.sampleSelect.FormattingEnabled = true;
            this.sampleSelect.Location = new System.Drawing.Point(129, 49);
            this.sampleSelect.Name = "sampleSelect";
            this.sampleSelect.Size = new System.Drawing.Size(121, 21);
            this.sampleSelect.TabIndex = 11;
            this.sampleSelect.SelectedIndexChanged += new System.EventHandler(this.sampleSelect_SelectedIndexChanged);
            // 
            // groupBox2
            // 
            groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox2.Controls.Add(this.filename);
            groupBox2.Controls.Add(browse);
            groupBox2.Location = new System.Drawing.Point(3, 3);
            groupBox2.Name = "groupBox2";
            groupBox2.Size = new System.Drawing.Size(372, 44);
            groupBox2.TabIndex = 7;
            groupBox2.TabStop = false;
            groupBox2.Text = "Path";
            // 
            // filename
            // 
            this.filename.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.filename.Location = new System.Drawing.Point(9, 15);
            this.filename.Name = "filename";
            this.filename.Size = new System.Drawing.Size(322, 20);
            this.filename.TabIndex = 1;
            this.filename.KeyUp += new System.Windows.Forms.KeyEventHandler(this.filename_KeyUp);
            // 
            // browse
            // 
            browse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            browse.Location = new System.Drawing.Point(337, 15);
            browse.Name = "browse";
            browse.Size = new System.Drawing.Size(26, 23);
            browse.TabIndex = 2;
            browse.Text = "...";
            browse.UseVisualStyleBackColor = true;
            browse.Click += new System.EventHandler(this.browse_Click);
            // 
            // colorDialog
            // 
            this.colorDialog.AnyColor = true;
            // 
            // saveTexDialog
            // 
            this.saveTexDialog.DefaultExt = "dds";
            this.saveTexDialog.OverwritePrompt = false;
            this.saveTexDialog.Title = "Save Texture As";
            // 
            // typingTimer
            // 
            this.typingTimer.Interval = 200;
            this.typingTimer.Tick += new System.EventHandler(this.typingTimer_Tick);
            // 
            // TextureSaveDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.ClientSize = new System.Drawing.Size(378, 555);
            this.Controls.Add(tableLayoutPanel1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "TextureSaveDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Save Texture";
            this.Shown += new System.EventHandler(this.TextureSaveDialog_Shown);
            tableLayoutPanel1.ResumeLayout(false);
            tableLayoutPanel1.PerformLayout();
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            flowLayoutPanel1.ResumeLayout(false);
            flowLayoutPanel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.jpegCompression)).EndInit();
            flowLayoutPanel2.ResumeLayout(false);
            this.alphaLDRGroup.ResumeLayout(false);
            this.alphaLDRGroup.PerformLayout();
            flowLayoutPanel3.ResumeLayout(false);
            flowLayoutPanel3.PerformLayout();
            this.sliceGroup.ResumeLayout(false);
            this.sliceGroup.PerformLayout();
            flowLayoutPanel6.ResumeLayout(false);
            flowLayoutPanel6.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.gridWidth)).EndInit();
            this.mipGroup.ResumeLayout(false);
            this.mipGroup.PerformLayout();
            flowLayoutPanel4.ResumeLayout(false);
            flowLayoutPanel4.PerformLayout();
            this.sampleGroup.ResumeLayout(false);
            this.sampleGroup.PerformLayout();
            flowLayoutPanel5.ResumeLayout(false);
            flowLayoutPanel5.PerformLayout();
            groupBox2.ResumeLayout(false);
            groupBox2.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox fileFormat;
        private System.Windows.Forms.NumericUpDown jpegCompression;
        private System.Windows.Forms.Button ok;
        private System.Windows.Forms.Button cancel;
        private System.Windows.Forms.ComboBox alphaMap;
        private System.Windows.Forms.Button alphaCol;
        private System.Windows.Forms.ColorDialog colorDialog;
        private System.Windows.Forms.GroupBox alphaLDRGroup;
        private System.Windows.Forms.TextBox blackPoint;
        private System.Windows.Forms.TextBox whitePoint;
        private System.Windows.Forms.GroupBox sampleGroup;
        private System.Windows.Forms.GroupBox sliceGroup;
        private System.Windows.Forms.CheckBox exportAllSlices;
        private System.Windows.Forms.GroupBox mipGroup;
        private System.Windows.Forms.CheckBox exportAllMips;
        private System.Windows.Forms.ComboBox mipSelect;
        private System.Windows.Forms.CheckBox mapSampleArray;
        private System.Windows.Forms.ComboBox sampleSelect;
        private System.Windows.Forms.CheckBox mapSlicesToGrid;
        private System.Windows.Forms.CheckBox cubeCruciform;
        private System.Windows.Forms.NumericUpDown gridWidth;
        private System.Windows.Forms.CheckBox oneSlice;
        private System.Windows.Forms.ComboBox sliceSelect;
        private System.Windows.Forms.CheckBox oneMip;
        private System.Windows.Forms.CheckBox oneSample;
        private System.Windows.Forms.CheckBox resolveSamples;
        private System.Windows.Forms.TextBox filename;
        private System.Windows.Forms.SaveFileDialog saveTexDialog;
        private System.Windows.Forms.Timer typingTimer;
    }
}