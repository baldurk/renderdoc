namespace renderdocui.Windows.Dialogs
{
    partial class CaptureDialog
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
            System.Windows.Forms.Label label2;
            System.Windows.Forms.Label label1;
            System.Windows.Forms.ColumnHeader pid;
            System.Windows.Forms.ColumnHeader name;
            System.Windows.Forms.Label label5;
            System.Windows.Forms.GroupBox groupBox1;
            this.flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
            this.queueFrameCap = new System.Windows.Forms.CheckBox();
            this.queuedCapFrame = new System.Windows.Forms.NumericUpDown();
            this.capOptsGroup = new System.Windows.Forms.GroupBox();
            this.capOptsFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.AllowFullscreen = new System.Windows.Forms.CheckBox();
            this.AllowVSync = new System.Windows.Forms.CheckBox();
            this.panel3 = new System.Windows.Forms.Panel();
            this.DelayForDebugger = new System.Windows.Forms.NumericUpDown();
            this.label4 = new System.Windows.Forms.Label();
            this.CaptureCallstacks = new System.Windows.Forms.CheckBox();
            this.CaptureCallstacksOnlyDraws = new System.Windows.Forms.CheckBox();
            this.DebugDeviceMode = new System.Windows.Forms.CheckBox();
            this.CacheStateObjects = new System.Windows.Forms.CheckBox();
            this.HookIntoChildren = new System.Windows.Forms.CheckBox();
            this.SaveAllInitials = new System.Windows.Forms.CheckBox();
            this.RefAllResources = new System.Windows.Forms.CheckBox();
            this.CaptureAllCmdLists = new System.Windows.Forms.CheckBox();
            this.AutoStart = new System.Windows.Forms.CheckBox();
            this.panel2 = new System.Windows.Forms.Panel();
            this.load = new System.Windows.Forms.Button();
            this.save = new System.Windows.Forms.Button();
            this.close = new System.Windows.Forms.Button();
            this.capture = new System.Windows.Forms.Button();
            this.exeBrowser = new System.Windows.Forms.OpenFileDialog();
            this.workDirBrowser = new System.Windows.Forms.FolderBrowserDialog();
            this.saveDialog = new System.Windows.Forms.SaveFileDialog();
            this.loadDialog = new System.Windows.Forms.OpenFileDialog();
            this.toolTip = new System.Windows.Forms.ToolTip(this.components);
            this.label3 = new System.Windows.Forms.Label();
            this.cmdline = new System.Windows.Forms.TextBox();
            this.workDirBrowse = new System.Windows.Forms.Button();
            this.workDirPath = new System.Windows.Forms.TextBox();
            this.exeBrowse = new System.Windows.Forms.Button();
            this.exePath = new System.Windows.Forms.TextBox();
            this.pidRefresh = new System.Windows.Forms.Button();
            this.pidList = new System.Windows.Forms.ListView();
            this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
            this.programGroup = new System.Windows.Forms.GroupBox();
            this.processGroup = new System.Windows.Forms.GroupBox();
            label2 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            pid = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            name = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            label5 = new System.Windows.Forms.Label();
            groupBox1 = new System.Windows.Forms.GroupBox();
            groupBox1.SuspendLayout();
            this.flowLayoutPanel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.queuedCapFrame)).BeginInit();
            this.capOptsGroup.SuspendLayout();
            this.capOptsFlow.SuspendLayout();
            this.panel3.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.DelayForDebugger)).BeginInit();
            this.panel2.SuspendLayout();
            this.tableLayoutPanel2.SuspendLayout();
            this.programGroup.SuspendLayout();
            this.processGroup.SuspendLayout();
            this.SuspendLayout();
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(6, 48);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(92, 13);
            label2.TabIndex = 6;
            label2.Text = "Working Directory";
            this.toolTip.SetToolTip(label2, "The working directory the executable will be launched in");
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(6, 22);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(85, 13);
            label1.TabIndex = 5;
            label1.Text = "Executable Path";
            this.toolTip.SetToolTip(label1, "The executable file to launch");
            // 
            // pid
            // 
            pid.Text = "PID";
            // 
            // name
            // 
            name.Text = "Name";
            name.Width = 430;
            // 
            // label5
            // 
            label5.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label5.Location = new System.Drawing.Point(9, 16);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(581, 23);
            label5.TabIndex = 3;
            label5.Text = "NOTE: Injecting only works when the process has not used the target API";
            // 
            // groupBox1
            // 
            groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            groupBox1.Controls.Add(this.flowLayoutPanel2);
            groupBox1.Location = new System.Drawing.Point(10, 500);
            groupBox1.Margin = new System.Windows.Forms.Padding(10);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new System.Drawing.Size(603, 49);
            groupBox1.TabIndex = 11;
            groupBox1.TabStop = false;
            groupBox1.Text = "Actions";
            // 
            // flowLayoutPanel2
            // 
            this.flowLayoutPanel2.Controls.Add(this.queueFrameCap);
            this.flowLayoutPanel2.Controls.Add(this.queuedCapFrame);
            this.flowLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.flowLayoutPanel2.Location = new System.Drawing.Point(3, 16);
            this.flowLayoutPanel2.Name = "flowLayoutPanel2";
            this.flowLayoutPanel2.Size = new System.Drawing.Size(597, 30);
            this.flowLayoutPanel2.TabIndex = 1;
            // 
            // queueFrameCap
            // 
            this.queueFrameCap.Location = new System.Drawing.Point(3, 3);
            this.queueFrameCap.Name = "queueFrameCap";
            this.queueFrameCap.Size = new System.Drawing.Size(151, 24);
            this.queueFrameCap.TabIndex = 1;
            this.queueFrameCap.Text = "Queue Capture of Frame";
            this.queueFrameCap.UseVisualStyleBackColor = true;
            // 
            // queuedCapFrame
            // 
            this.queuedCapFrame.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.queuedCapFrame.Location = new System.Drawing.Point(160, 5);
            this.queuedCapFrame.Maximum = new decimal(new int[] {
            10000000,
            0,
            0,
            0});
            this.queuedCapFrame.Minimum = new decimal(new int[] {
            2,
            0,
            0,
            0});
            this.queuedCapFrame.Name = "queuedCapFrame";
            this.queuedCapFrame.Size = new System.Drawing.Size(120, 20);
            this.queuedCapFrame.TabIndex = 0;
            this.queuedCapFrame.ThousandsSeparator = true;
            this.queuedCapFrame.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
            // 
            // capOptsGroup
            // 
            this.capOptsGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.capOptsGroup.AutoSize = true;
            this.capOptsGroup.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.capOptsGroup.Controls.Add(this.capOptsFlow);
            this.capOptsGroup.Location = new System.Drawing.Point(10, 367);
            this.capOptsGroup.Margin = new System.Windows.Forms.Padding(10);
            this.capOptsGroup.Name = "capOptsGroup";
            this.capOptsGroup.Size = new System.Drawing.Size(603, 113);
            this.capOptsGroup.TabIndex = 4;
            this.capOptsGroup.TabStop = false;
            this.capOptsGroup.Text = "Capture Options";
            this.capOptsGroup.Layout += new System.Windows.Forms.LayoutEventHandler(this.capOptsGroup_Layout);
            // 
            // capOptsFlow
            // 
            this.capOptsFlow.AutoSize = true;
            this.capOptsFlow.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.capOptsFlow.Controls.Add(this.AllowFullscreen);
            this.capOptsFlow.Controls.Add(this.AllowVSync);
            this.capOptsFlow.Controls.Add(this.panel3);
            this.capOptsFlow.Controls.Add(this.CaptureCallstacks);
            this.capOptsFlow.Controls.Add(this.CaptureCallstacksOnlyDraws);
            this.capOptsFlow.Controls.Add(this.DebugDeviceMode);
            this.capOptsFlow.Controls.Add(this.CacheStateObjects);
            this.capOptsFlow.Controls.Add(this.HookIntoChildren);
            this.capOptsFlow.Controls.Add(this.SaveAllInitials);
            this.capOptsFlow.Controls.Add(this.RefAllResources);
            this.capOptsFlow.Controls.Add(this.CaptureAllCmdLists);
            this.capOptsFlow.Controls.Add(this.AutoStart);
            this.capOptsFlow.Location = new System.Drawing.Point(3, 16);
            this.capOptsFlow.MaximumSize = new System.Drawing.Size(640, 0);
            this.capOptsFlow.Name = "capOptsFlow";
            this.capOptsFlow.Size = new System.Drawing.Size(544, 78);
            this.capOptsFlow.TabIndex = 13;
            // 
            // AllowFullscreen
            // 
            this.AllowFullscreen.Location = new System.Drawing.Point(3, 3);
            this.AllowFullscreen.Name = "AllowFullscreen";
            this.AllowFullscreen.Size = new System.Drawing.Size(130, 20);
            this.AllowFullscreen.TabIndex = 8;
            this.AllowFullscreen.Text = "Allow Fullscreen";
            this.toolTip.SetToolTip(this.AllowFullscreen, "Allows the application to switch to full-screen mode");
            this.AllowFullscreen.UseVisualStyleBackColor = true;
            // 
            // AllowVSync
            // 
            this.AllowVSync.Location = new System.Drawing.Point(139, 3);
            this.AllowVSync.Name = "AllowVSync";
            this.AllowVSync.Size = new System.Drawing.Size(130, 20);
            this.AllowVSync.TabIndex = 9;
            this.AllowVSync.Text = "Allow VSync";
            this.toolTip.SetToolTip(this.AllowVSync, "Allows the application to enable v-sync");
            this.AllowVSync.UseVisualStyleBackColor = true;
            // 
            // panel3
            // 
            this.panel3.Controls.Add(this.DelayForDebugger);
            this.panel3.Controls.Add(this.label4);
            this.panel3.Location = new System.Drawing.Point(272, 3);
            this.panel3.Margin = new System.Windows.Forms.Padding(0, 3, 6, 3);
            this.panel3.Name = "panel3";
            this.panel3.Size = new System.Drawing.Size(130, 20);
            this.panel3.TabIndex = 14;
            // 
            // DelayForDebugger
            // 
            this.DelayForDebugger.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.DelayForDebugger.Location = new System.Drawing.Point(3, 0);
            this.DelayForDebugger.Margin = new System.Windows.Forms.Padding(0, 0, 3, 3);
            this.DelayForDebugger.Name = "DelayForDebugger";
            this.DelayForDebugger.Size = new System.Drawing.Size(48, 20);
            this.DelayForDebugger.TabIndex = 12;
            this.toolTip.SetToolTip(this.DelayForDebugger, "Pauses for N seconds after launching the process,\r\nto allow a debugger to attach");
            // 
            // label4
            // 
            this.label4.Dock = System.Windows.Forms.DockStyle.Right;
            this.label4.Location = new System.Drawing.Point(51, 0);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(79, 20);
            this.label4.TabIndex = 13;
            this.label4.Text = "Seconds Delay";
            this.label4.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.toolTip.SetToolTip(this.label4, "Pauses for N seconds after launching the process,\r\nto allow a debugger to attach");
            // 
            // CaptureCallstacks
            // 
            this.CaptureCallstacks.Location = new System.Drawing.Point(411, 3);
            this.CaptureCallstacks.Name = "CaptureCallstacks";
            this.CaptureCallstacks.Size = new System.Drawing.Size(130, 20);
            this.CaptureCallstacks.TabIndex = 10;
            this.CaptureCallstacks.Text = "Collect Callstacks";
            this.toolTip.SetToolTip(this.CaptureCallstacks, "Collect a callstack on every API call");
            this.CaptureCallstacks.UseVisualStyleBackColor = true;
            this.CaptureCallstacks.CheckedChanged += new System.EventHandler(this.CaptureCallstacks_CheckedChanged);
            // 
            // CaptureCallstacksOnlyDraws
            // 
            this.CaptureCallstacksOnlyDraws.Enabled = false;
            this.CaptureCallstacksOnlyDraws.Location = new System.Drawing.Point(3, 29);
            this.CaptureCallstacksOnlyDraws.Name = "CaptureCallstacksOnlyDraws";
            this.CaptureCallstacksOnlyDraws.Size = new System.Drawing.Size(130, 20);
            this.CaptureCallstacksOnlyDraws.TabIndex = 19;
            this.CaptureCallstacksOnlyDraws.Text = "Only Drawcall stacks";
            this.toolTip.SetToolTip(this.CaptureCallstacksOnlyDraws, "Only collect callstacks on \'drawcall\' level api calls");
            this.CaptureCallstacksOnlyDraws.UseVisualStyleBackColor = true;
            // 
            // DebugDeviceMode
            // 
            this.DebugDeviceMode.Location = new System.Drawing.Point(139, 29);
            this.DebugDeviceMode.Name = "DebugDeviceMode";
            this.DebugDeviceMode.Size = new System.Drawing.Size(130, 20);
            this.DebugDeviceMode.TabIndex = 9;
            this.DebugDeviceMode.Text = "Create Debug Device";
            this.toolTip.SetToolTip(this.DebugDeviceMode, "D3D11: Create a debug device - allows capturing and reading of D3D errors and war" +
        "nings");
            this.DebugDeviceMode.UseVisualStyleBackColor = true;
            // 
            // CacheStateObjects
            // 
            this.CacheStateObjects.Location = new System.Drawing.Point(275, 29);
            this.CacheStateObjects.Name = "CacheStateObjects";
            this.CacheStateObjects.Size = new System.Drawing.Size(130, 20);
            this.CacheStateObjects.TabIndex = 11;
            this.CacheStateObjects.Text = "Cache State Objects";
            this.toolTip.SetToolTip(this.CacheStateObjects, "D3D11: Caches state objects so that rapid creation & destruction\r\ndoesn\'t inflate" +
        " memory used and log file size");
            this.CacheStateObjects.UseVisualStyleBackColor = true;
            // 
            // HookIntoChildren
            // 
            this.HookIntoChildren.Location = new System.Drawing.Point(411, 29);
            this.HookIntoChildren.Name = "HookIntoChildren";
            this.HookIntoChildren.Size = new System.Drawing.Size(130, 20);
            this.HookIntoChildren.TabIndex = 16;
            this.HookIntoChildren.Text = "Hook Into Children";
            this.toolTip.SetToolTip(this.HookIntoChildren, "Hook into child processes - useful with launchers or similar intermediate process" +
        "es");
            this.HookIntoChildren.UseVisualStyleBackColor = true;
            // 
            // SaveAllInitials
            // 
            this.SaveAllInitials.Location = new System.Drawing.Point(3, 55);
            this.SaveAllInitials.Name = "SaveAllInitials";
            this.SaveAllInitials.Size = new System.Drawing.Size(130, 20);
            this.SaveAllInitials.TabIndex = 17;
            this.SaveAllInitials.Text = "Save All Initials";
            this.toolTip.SetToolTip(this.SaveAllInitials, "Save the initial state of all API resources at the start of each captured frame");
            this.SaveAllInitials.UseVisualStyleBackColor = true;
            // 
            // RefAllResources
            // 
            this.RefAllResources.Location = new System.Drawing.Point(139, 55);
            this.RefAllResources.Name = "RefAllResources";
            this.RefAllResources.Size = new System.Drawing.Size(130, 20);
            this.RefAllResources.TabIndex = 18;
            this.RefAllResources.Text = "Ref All Resources";
            this.toolTip.SetToolTip(this.RefAllResources, "Consider all resources to be included, even if unused in the capture frame");
            this.RefAllResources.UseVisualStyleBackColor = true;
            // 
            // CaptureAllCmdLists
            // 
            this.CaptureAllCmdLists.Location = new System.Drawing.Point(275, 55);
            this.CaptureAllCmdLists.Name = "CaptureAllCmdLists";
            this.CaptureAllCmdLists.Size = new System.Drawing.Size(130, 20);
            this.CaptureAllCmdLists.TabIndex = 21;
            this.CaptureAllCmdLists.Text = "Capture All Cmd Lists";
            this.toolTip.SetToolTip(this.CaptureAllCmdLists, "When enabled, all deferred command lists will be saved even while idling.\r\nThis h" +
        "as an overhead but ensures if you hold onto a list it will be captured.");
            this.CaptureAllCmdLists.UseVisualStyleBackColor = true;
            // 
            // AutoStart
            // 
            this.AutoStart.Location = new System.Drawing.Point(411, 55);
            this.AutoStart.Name = "AutoStart";
            this.AutoStart.Size = new System.Drawing.Size(130, 20);
            this.AutoStart.TabIndex = 20;
            this.AutoStart.Text = "Auto start";
            this.toolTip.SetToolTip(this.AutoStart, "If these capture settings are saved & run, auto start the capture instantly on lo" +
        "ad");
            this.AutoStart.UseVisualStyleBackColor = true;
            // 
            // panel2
            // 
            this.panel2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.panel2.Controls.Add(this.load);
            this.panel2.Controls.Add(this.save);
            this.panel2.Controls.Add(this.close);
            this.panel2.Controls.Add(this.capture);
            this.panel2.Location = new System.Drawing.Point(3, 562);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(617, 26);
            this.panel2.TabIndex = 8;
            // 
            // load
            // 
            this.load.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.load.Location = new System.Drawing.Point(104, 3);
            this.load.Name = "load";
            this.load.Size = new System.Drawing.Size(86, 23);
            this.load.TabIndex = 8;
            this.load.Text = "Load Settings";
            this.toolTip.SetToolTip(this.load, "Load a saved set of capture settings");
            this.load.UseVisualStyleBackColor = true;
            this.load.Click += new System.EventHandler(this.load_Click);
            // 
            // save
            // 
            this.save.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.save.Location = new System.Drawing.Point(4, 3);
            this.save.Name = "save";
            this.save.Size = new System.Drawing.Size(94, 23);
            this.save.TabIndex = 7;
            this.save.Text = "Save Settings";
            this.toolTip.SetToolTip(this.save, "Save these capture settings to file to load later");
            this.save.UseVisualStyleBackColor = true;
            this.save.Click += new System.EventHandler(this.save_Click);
            // 
            // close
            // 
            this.close.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.close.Location = new System.Drawing.Point(540, 3);
            this.close.Margin = new System.Windows.Forms.Padding(0);
            this.close.Name = "close";
            this.close.Size = new System.Drawing.Size(70, 23);
            this.close.TabIndex = 6;
            this.close.Text = "Close";
            this.close.UseVisualStyleBackColor = true;
            this.close.Click += new System.EventHandler(this.close_Click);
            // 
            // capture
            // 
            this.capture.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.capture.Location = new System.Drawing.Point(464, 3);
            this.capture.Margin = new System.Windows.Forms.Padding(0);
            this.capture.Name = "capture";
            this.capture.Size = new System.Drawing.Size(70, 23);
            this.capture.TabIndex = 5;
            this.capture.Text = "Capture";
            this.toolTip.SetToolTip(this.capture, "Trigger a capture of the selected program");
            this.capture.UseVisualStyleBackColor = true;
            this.capture.Click += new System.EventHandler(this.capture_Click);
            // 
            // exeBrowser
            // 
            this.exeBrowser.DefaultExt = "exe";
            this.exeBrowser.FileName = "Application.exe";
            this.exeBrowser.Filter = "Executable files|*.exe|All files|*.*";
            this.exeBrowser.RestoreDirectory = true;
            this.exeBrowser.FileOk += new System.ComponentModel.CancelEventHandler(this.exeBrowser_FileOk);
            // 
            // workDirBrowser
            // 
            this.workDirBrowser.Description = "Choose Working Directory";
            this.workDirBrowser.RootFolder = System.Environment.SpecialFolder.MyComputer;
            // 
            // saveDialog
            // 
            this.saveDialog.DefaultExt = "cap";
            this.saveDialog.FileName = "Settings.cap";
            this.saveDialog.Filter = "Capture settings|*.cap";
            // 
            // loadDialog
            // 
            this.loadDialog.DefaultExt = "cap";
            this.loadDialog.FileName = "Settings.cap";
            this.loadDialog.Filter = "Capture settings|*.cap";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(6, 74);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(126, 13);
            this.label3.TabIndex = 7;
            this.label3.Text = "Command-line Arguments";
            this.toolTip.SetToolTip(this.label3, "The command-line that will be passed to the executable on launch");
            // 
            // cmdline
            // 
            this.cmdline.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.cmdline.Location = new System.Drawing.Point(137, 71);
            this.cmdline.Name = "cmdline";
            this.cmdline.Size = new System.Drawing.Size(460, 20);
            this.cmdline.TabIndex = 4;
            this.toolTip.SetToolTip(this.cmdline, "The command-line that will be passed to the executable on launch");
            // 
            // workDirBrowse
            // 
            this.workDirBrowse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.workDirBrowse.Location = new System.Drawing.Point(573, 45);
            this.workDirBrowse.Name = "workDirBrowse";
            this.workDirBrowse.Size = new System.Drawing.Size(24, 20);
            this.workDirBrowse.TabIndex = 3;
            this.workDirBrowse.Text = "...";
            this.toolTip.SetToolTip(this.workDirBrowse, "Browse for a working directory");
            this.workDirBrowse.UseVisualStyleBackColor = true;
            this.workDirBrowse.Click += new System.EventHandler(this.workDirBrowse_Click);
            // 
            // workDirPath
            // 
            this.workDirPath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.workDirPath.Location = new System.Drawing.Point(137, 45);
            this.workDirPath.Name = "workDirPath";
            this.workDirPath.Size = new System.Drawing.Size(430, 20);
            this.workDirPath.TabIndex = 2;
            this.toolTip.SetToolTip(this.workDirPath, "The working directory the executable will be launched in");
            this.workDirPath.TextChanged += new System.EventHandler(this.workDirPath_TextChanged);
            this.workDirPath.Enter += new System.EventHandler(this.workDirPath_Enter);
            this.workDirPath.Leave += new System.EventHandler(this.workDirPath_Leave);
            // 
            // exeBrowse
            // 
            this.exeBrowse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.exeBrowse.Location = new System.Drawing.Point(573, 19);
            this.exeBrowse.Name = "exeBrowse";
            this.exeBrowse.Size = new System.Drawing.Size(24, 20);
            this.exeBrowse.TabIndex = 1;
            this.exeBrowse.Text = "...";
            this.toolTip.SetToolTip(this.exeBrowse, "Browse for an executable file");
            this.exeBrowse.UseVisualStyleBackColor = true;
            this.exeBrowse.Click += new System.EventHandler(this.exeBrowse_Click);
            // 
            // exePath
            // 
            this.exePath.AllowDrop = true;
            this.exePath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.exePath.Location = new System.Drawing.Point(137, 19);
            this.exePath.Name = "exePath";
            this.exePath.Size = new System.Drawing.Size(430, 20);
            this.exePath.TabIndex = 0;
            this.toolTip.SetToolTip(this.exePath, "The executable file to launch");
            this.exePath.TextChanged += new System.EventHandler(this.exePath_TextChanged);
            this.exePath.DragDrop += new System.Windows.Forms.DragEventHandler(this.exePath_DragDrop);
            this.exePath.DragEnter += new System.Windows.Forms.DragEventHandler(this.exePath_DragEnter);
            // 
            // pidRefresh
            // 
            this.pidRefresh.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.pidRefresh.Location = new System.Drawing.Point(518, 180);
            this.pidRefresh.Name = "pidRefresh";
            this.pidRefresh.Size = new System.Drawing.Size(75, 23);
            this.pidRefresh.TabIndex = 1;
            this.pidRefresh.Text = "Refresh";
            this.toolTip.SetToolTip(this.pidRefresh, "Refresh the list of processes");
            this.pidRefresh.UseVisualStyleBackColor = true;
            this.pidRefresh.Click += new System.EventHandler(this.pidRefresh_Click);
            // 
            // pidList
            // 
            this.pidList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.pidList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            pid,
            name});
            this.pidList.FullRowSelect = true;
            this.pidList.GridLines = true;
            this.pidList.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
            this.pidList.HideSelection = false;
            this.pidList.Location = new System.Drawing.Point(6, 42);
            this.pidList.MultiSelect = false;
            this.pidList.Name = "pidList";
            this.pidList.Size = new System.Drawing.Size(584, 129);
            this.pidList.TabIndex = 2;
            this.toolTip.SetToolTip(this.pidList, "Select the process to inject into - must not yet have utilised the target API");
            this.pidList.UseCompatibleStateImageBehavior = false;
            this.pidList.View = System.Windows.Forms.View.Details;
            // 
            // tableLayoutPanel2
            // 
            this.tableLayoutPanel2.ColumnCount = 1;
            this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel2.Controls.Add(this.programGroup, 0, 0);
            this.tableLayoutPanel2.Controls.Add(this.panel2, 0, 4);
            this.tableLayoutPanel2.Controls.Add(this.capOptsGroup, 0, 2);
            this.tableLayoutPanel2.Controls.Add(this.processGroup, 0, 1);
            this.tableLayoutPanel2.Controls.Add(groupBox1, 0, 3);
            this.tableLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel2.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel2.Name = "tableLayoutPanel2";
            this.tableLayoutPanel2.RowCount = 5;
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel2.Size = new System.Drawing.Size(623, 761);
            this.tableLayoutPanel2.TabIndex = 8;
            // 
            // programGroup
            // 
            this.programGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.programGroup.Controls.Add(this.label3);
            this.programGroup.Controls.Add(label2);
            this.programGroup.Controls.Add(label1);
            this.programGroup.Controls.Add(this.cmdline);
            this.programGroup.Controls.Add(this.workDirBrowse);
            this.programGroup.Controls.Add(this.workDirPath);
            this.programGroup.Controls.Add(this.exeBrowse);
            this.programGroup.Controls.Add(this.exePath);
            this.programGroup.Location = new System.Drawing.Point(10, 10);
            this.programGroup.Margin = new System.Windows.Forms.Padding(10);
            this.programGroup.Name = "programGroup";
            this.programGroup.Size = new System.Drawing.Size(603, 108);
            this.programGroup.TabIndex = 10;
            this.programGroup.TabStop = false;
            this.programGroup.Text = "Program";
            // 
            // processGroup
            // 
            this.processGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.processGroup.Controls.Add(label5);
            this.processGroup.Controls.Add(this.pidList);
            this.processGroup.Controls.Add(this.pidRefresh);
            this.processGroup.Location = new System.Drawing.Point(10, 138);
            this.processGroup.Margin = new System.Windows.Forms.Padding(10);
            this.processGroup.Name = "processGroup";
            this.processGroup.Size = new System.Drawing.Size(603, 209);
            this.processGroup.TabIndex = 9;
            this.processGroup.TabStop = false;
            this.processGroup.Text = "Process";
            // 
            // CaptureDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoScroll = true;
            this.AutoScrollMinSize = new System.Drawing.Size(360, 0);
            this.ClientSize = new System.Drawing.Size(623, 761);
            this.Controls.Add(this.tableLayoutPanel2);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "CaptureDialog";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.Document;
            this.Text = "CaptureDialog";
            groupBox1.ResumeLayout(false);
            this.flowLayoutPanel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.queuedCapFrame)).EndInit();
            this.capOptsGroup.ResumeLayout(false);
            this.capOptsGroup.PerformLayout();
            this.capOptsFlow.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.DelayForDebugger)).EndInit();
            this.panel2.ResumeLayout(false);
            this.tableLayoutPanel2.ResumeLayout(false);
            this.tableLayoutPanel2.PerformLayout();
            this.programGroup.ResumeLayout(false);
            this.programGroup.PerformLayout();
            this.processGroup.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button capture;
        private System.Windows.Forms.Button close;
        private System.Windows.Forms.CheckBox CacheStateObjects;
        private System.Windows.Forms.CheckBox AllowVSync;
        private System.Windows.Forms.CheckBox DebugDeviceMode;
        private System.Windows.Forms.CheckBox AllowFullscreen;
        private System.Windows.Forms.CheckBox CaptureCallstacks;
        private System.Windows.Forms.Panel panel3;
        private System.Windows.Forms.NumericUpDown DelayForDebugger;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.OpenFileDialog exeBrowser;
        private System.Windows.Forms.FolderBrowserDialog workDirBrowser;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.Button load;
        private System.Windows.Forms.Button save;
        private System.Windows.Forms.SaveFileDialog saveDialog;
        private System.Windows.Forms.OpenFileDialog loadDialog;
        private System.Windows.Forms.CheckBox HookIntoChildren;
        private System.Windows.Forms.CheckBox SaveAllInitials;
        private System.Windows.Forms.CheckBox RefAllResources;
        private System.Windows.Forms.CheckBox CaptureCallstacksOnlyDraws;
        private System.Windows.Forms.CheckBox AutoStart;
        private System.Windows.Forms.ToolTip toolTip;
        private System.Windows.Forms.FlowLayoutPanel capOptsFlow;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
        private System.Windows.Forms.GroupBox programGroup;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.TextBox cmdline;
        private System.Windows.Forms.Button workDirBrowse;
        private System.Windows.Forms.TextBox workDirPath;
        private System.Windows.Forms.Button exeBrowse;
        private System.Windows.Forms.TextBox exePath;
        private System.Windows.Forms.GroupBox processGroup;
        private System.Windows.Forms.ListView pidList;
        private System.Windows.Forms.Button pidRefresh;
        private System.Windows.Forms.CheckBox CaptureAllCmdLists;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
        private System.Windows.Forms.NumericUpDown queuedCapFrame;
        private System.Windows.Forms.CheckBox queueFrameCap;
        private System.Windows.Forms.GroupBox capOptsGroup;

    }
}