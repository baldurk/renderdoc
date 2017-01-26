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
            System.Windows.Forms.Label label3;
            System.Windows.Forms.Label label6;
            this.globalLabel = new System.Windows.Forms.Label();
            this.actionsGroup = new System.Windows.Forms.GroupBox();
            this.actionsFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.queueFrameCap = new System.Windows.Forms.CheckBox();
            this.queuedCapFrame = new System.Windows.Forms.NumericUpDown();
            this.globalGroup = new System.Windows.Forms.GroupBox();
            this.globalFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.toggleGlobalHook = new System.Windows.Forms.CheckBox();
            this.capOptsGroup = new System.Windows.Forms.GroupBox();
            this.capOptsFlow = new System.Windows.Forms.FlowLayoutPanel();
            this.AllowFullscreen = new System.Windows.Forms.CheckBox();
            this.AllowVSync = new System.Windows.Forms.CheckBox();
            this.panel3 = new System.Windows.Forms.Panel();
            this.DelayForDebugger = new System.Windows.Forms.NumericUpDown();
            this.label4 = new System.Windows.Forms.Label();
            this.CaptureCallstacks = new System.Windows.Forms.CheckBox();
            this.CaptureCallstacksOnlyDraws = new System.Windows.Forms.CheckBox();
            this.APIValidation = new System.Windows.Forms.CheckBox();
            this.HookIntoChildren = new System.Windows.Forms.CheckBox();
            this.SaveAllInitials = new System.Windows.Forms.CheckBox();
            this.RefAllResources = new System.Windows.Forms.CheckBox();
            this.CaptureAllCmdLists = new System.Windows.Forms.CheckBox();
            this.VerifyMapWrites = new System.Windows.Forms.CheckBox();
            this.AutoStart = new System.Windows.Forms.CheckBox();
            this.panel2 = new System.Windows.Forms.Panel();
            this.load = new System.Windows.Forms.Button();
            this.save = new System.Windows.Forms.Button();
            this.close = new System.Windows.Forms.Button();
            this.launch = new System.Windows.Forms.Button();
            this.exeBrowser = new System.Windows.Forms.OpenFileDialog();
            this.workDirBrowser = new System.Windows.Forms.FolderBrowserDialog();
            this.saveDialog = new System.Windows.Forms.SaveFileDialog();
            this.loadDialog = new System.Windows.Forms.OpenFileDialog();
            this.toolTip = new System.Windows.Forms.ToolTip(this.components);
            this.cmdline = new System.Windows.Forms.TextBox();
            this.workDirBrowse = new System.Windows.Forms.Button();
            this.workDirPath = new System.Windows.Forms.TextBox();
            this.exeBrowse = new System.Windows.Forms.Button();
            this.exePath = new System.Windows.Forms.TextBox();
            this.pidRefresh = new System.Windows.Forms.Button();
            this.pidList = new System.Windows.Forms.ListView();
            this.winTitle = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.environmentDisplay = new System.Windows.Forms.TextBox();
            this.setEnv = new System.Windows.Forms.Button();
            this.processFilter = new System.Windows.Forms.TextBox();
            this.mainTableLayout = new System.Windows.Forms.TableLayoutPanel();
            this.programGroup = new System.Windows.Forms.GroupBox();
            this.processGroup = new System.Windows.Forms.GroupBox();
            this.vulkanLayerWarn = new System.Windows.Forms.Button();
            label2 = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            pid = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            name = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            label5 = new System.Windows.Forms.Label();
            label3 = new System.Windows.Forms.Label();
            label6 = new System.Windows.Forms.Label();
            this.actionsGroup.SuspendLayout();
            this.actionsFlow.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.queuedCapFrame)).BeginInit();
            this.globalGroup.SuspendLayout();
            this.globalFlow.SuspendLayout();
            this.capOptsGroup.SuspendLayout();
            this.capOptsFlow.SuspendLayout();
            this.panel3.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.DelayForDebugger)).BeginInit();
            this.panel2.SuspendLayout();
            this.mainTableLayout.SuspendLayout();
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
            name.Width = 120;
            // 
            // label5
            // 
            label5.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            label5.AutoEllipsis = true;
            label5.Font = new System.Drawing.Font("Microsoft Sans Serif", 14.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            label5.ForeColor = System.Drawing.Color.Red;
            label5.Location = new System.Drawing.Point(9, 16);
            label5.Name = "label5";
            label5.Size = new System.Drawing.Size(179, 35);
            label5.TabIndex = 3;
            label5.Text = "NOTE: Injecting only works when the process has not used the target API";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new System.Drawing.Point(6, 74);
            label3.Name = "label3";
            label3.Size = new System.Drawing.Size(126, 13);
            label3.TabIndex = 7;
            label3.Text = "Command-line Arguments";
            this.toolTip.SetToolTip(label3, "The command-line that will be passed to the executable on launch");
            // 
            // label6
            // 
            label6.AutoSize = true;
            label6.Location = new System.Drawing.Point(6, 103);
            label6.Name = "label6";
            label6.Size = new System.Drawing.Size(112, 13);
            label6.TabIndex = 8;
            label6.Text = "Environment Variables";
            this.toolTip.SetToolTip(label6, "The command-line that will be passed to the executable on launch");
            // 
            // globalLabel
            // 
            this.globalLabel.AutoSize = true;
            this.globalLabel.ForeColor = System.Drawing.Color.Red;
            this.globalLabel.Location = new System.Drawing.Point(3, 3);
            this.globalLabel.Margin = new System.Windows.Forms.Padding(3);
            this.globalLabel.Name = "globalLabel";
            this.globalLabel.Size = new System.Drawing.Size(188, 39);
            this.globalLabel.TabIndex = 2;
            this.globalLabel.Text = "Text is set by code Text is set by code\r\nText is set by codeText is set by code\r\n" +
    "Text is set by codeText is set by code";
            // 
            // actionsGroup
            // 
            this.actionsGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.actionsGroup.AutoSize = true;
            this.actionsGroup.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.actionsGroup.Controls.Add(this.actionsFlow);
            this.actionsGroup.Location = new System.Drawing.Point(10, 531);
            this.actionsGroup.Margin = new System.Windows.Forms.Padding(10);
            this.actionsGroup.Name = "actionsGroup";
            this.actionsGroup.Size = new System.Drawing.Size(195, 65);
            this.actionsGroup.TabIndex = 11;
            this.actionsGroup.TabStop = false;
            this.actionsGroup.Text = "Actions";
            this.actionsGroup.Layout += new System.Windows.Forms.LayoutEventHandler(this.actionsGroup_Layout);
            // 
            // actionsFlow
            // 
            this.actionsFlow.AutoSize = true;
            this.actionsFlow.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.actionsFlow.Controls.Add(this.queueFrameCap);
            this.actionsFlow.Controls.Add(this.queuedCapFrame);
            this.actionsFlow.Location = new System.Drawing.Point(3, 16);
            this.actionsFlow.Name = "actionsFlow";
            this.actionsFlow.Size = new System.Drawing.Size(283, 30);
            this.actionsFlow.TabIndex = 1;
            // 
            // queueFrameCap
            // 
            this.queueFrameCap.Location = new System.Drawing.Point(3, 3);
            this.queueFrameCap.Name = "queueFrameCap";
            this.queueFrameCap.Size = new System.Drawing.Size(151, 24);
            this.queueFrameCap.TabIndex = 20;
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
            1,
            0,
            0,
            0});
            this.queuedCapFrame.Name = "queuedCapFrame";
            this.queuedCapFrame.Size = new System.Drawing.Size(120, 20);
            this.queuedCapFrame.TabIndex = 21;
            this.queuedCapFrame.ThousandsSeparator = true;
            this.queuedCapFrame.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
            // 
            // globalGroup
            // 
            this.globalGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.globalGroup.AutoSize = true;
            this.globalGroup.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.globalGroup.Controls.Add(this.globalFlow);
            this.globalGroup.Location = new System.Drawing.Point(10, 616);
            this.globalGroup.Margin = new System.Windows.Forms.Padding(10);
            this.globalGroup.Name = "globalGroup";
            this.globalGroup.Size = new System.Drawing.Size(195, 80);
            this.globalGroup.TabIndex = 12;
            this.globalGroup.TabStop = false;
            this.globalGroup.Text = "Global Process Hook";
            this.globalGroup.Layout += new System.Windows.Forms.LayoutEventHandler(this.globalGroup_Layout);
            // 
            // globalFlow
            // 
            this.globalFlow.AutoSize = true;
            this.globalFlow.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.globalFlow.Controls.Add(this.globalLabel);
            this.globalFlow.Controls.Add(this.toggleGlobalHook);
            this.globalFlow.Location = new System.Drawing.Point(3, 16);
            this.globalFlow.Name = "globalFlow";
            this.globalFlow.Size = new System.Drawing.Size(312, 45);
            this.globalFlow.TabIndex = 0;
            // 
            // toggleGlobalHook
            // 
            this.toggleGlobalHook.Appearance = System.Windows.Forms.Appearance.Button;
            this.toggleGlobalHook.AutoSize = true;
            this.toggleGlobalHook.Location = new System.Drawing.Point(197, 3);
            this.toggleGlobalHook.Name = "toggleGlobalHook";
            this.toggleGlobalHook.Size = new System.Drawing.Size(112, 23);
            this.toggleGlobalHook.TabIndex = 3;
            this.toggleGlobalHook.Text = "Enable Global Hook";
            this.toggleGlobalHook.UseVisualStyleBackColor = true;
            this.toggleGlobalHook.CheckedChanged += new System.EventHandler(this.toggleGlobalHook_CheckedChanged);
            // 
            // capOptsGroup
            // 
            this.capOptsGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.capOptsGroup.AutoSize = true;
            this.capOptsGroup.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.capOptsGroup.Controls.Add(this.capOptsFlow);
            this.capOptsGroup.Location = new System.Drawing.Point(10, 398);
            this.capOptsGroup.Margin = new System.Windows.Forms.Padding(10);
            this.capOptsGroup.Name = "capOptsGroup";
            this.capOptsGroup.Size = new System.Drawing.Size(195, 113);
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
            this.capOptsFlow.Controls.Add(this.APIValidation);
            this.capOptsFlow.Controls.Add(this.HookIntoChildren);
            this.capOptsFlow.Controls.Add(this.SaveAllInitials);
            this.capOptsFlow.Controls.Add(this.RefAllResources);
            this.capOptsFlow.Controls.Add(this.CaptureAllCmdLists);
            this.capOptsFlow.Controls.Add(this.VerifyMapWrites);
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
            this.AllowFullscreen.TabIndex = 7;
            this.AllowFullscreen.Text = "Allow Fullscreen";
            this.toolTip.SetToolTip(this.AllowFullscreen, "Allows the application to switch to full-screen mode");
            this.AllowFullscreen.UseVisualStyleBackColor = true;
            // 
            // AllowVSync
            // 
            this.AllowVSync.Location = new System.Drawing.Point(139, 3);
            this.AllowVSync.Name = "AllowVSync";
            this.AllowVSync.Size = new System.Drawing.Size(130, 20);
            this.AllowVSync.TabIndex = 8;
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
            this.DelayForDebugger.TabIndex = 9;
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
            this.CaptureCallstacksOnlyDraws.TabIndex = 11;
            this.CaptureCallstacksOnlyDraws.Text = "Only Drawcall stacks";
            this.toolTip.SetToolTip(this.CaptureCallstacksOnlyDraws, "Only collect callstacks on \'drawcall\' level api calls");
            this.CaptureCallstacksOnlyDraws.UseVisualStyleBackColor = true;
            // 
            // APIValidation
            // 
            this.APIValidation.Location = new System.Drawing.Point(139, 29);
            this.APIValidation.Name = "APIValidation";
            this.APIValidation.Size = new System.Drawing.Size(130, 20);
            this.APIValidation.TabIndex = 12;
            this.APIValidation.Text = "Enable API validation";
            this.toolTip.SetToolTip(this.APIValidation, "Initialise the graphics API with built-in validation enabled - allows capturing a" +
        "nd reading of errors and warnings generated by the API\'s own debugging system");
            this.APIValidation.UseVisualStyleBackColor = true;
            // 
            // HookIntoChildren
            // 
            this.HookIntoChildren.Location = new System.Drawing.Point(275, 29);
            this.HookIntoChildren.Name = "HookIntoChildren";
            this.HookIntoChildren.Size = new System.Drawing.Size(130, 20);
            this.HookIntoChildren.TabIndex = 14;
            this.HookIntoChildren.Text = "Hook Into Children";
            this.toolTip.SetToolTip(this.HookIntoChildren, "Hook into child processes - useful with launchers or similar intermediate process" +
        "es");
            this.HookIntoChildren.UseVisualStyleBackColor = true;
            // 
            // SaveAllInitials
            // 
            this.SaveAllInitials.Location = new System.Drawing.Point(411, 29);
            this.SaveAllInitials.Name = "SaveAllInitials";
            this.SaveAllInitials.Size = new System.Drawing.Size(130, 20);
            this.SaveAllInitials.TabIndex = 15;
            this.SaveAllInitials.Text = "Save All Initials";
            this.toolTip.SetToolTip(this.SaveAllInitials, "Save the initial state of all API resources at the start of each captured frame");
            this.SaveAllInitials.UseVisualStyleBackColor = true;
            // 
            // RefAllResources
            // 
            this.RefAllResources.Location = new System.Drawing.Point(3, 55);
            this.RefAllResources.Name = "RefAllResources";
            this.RefAllResources.Size = new System.Drawing.Size(130, 20);
            this.RefAllResources.TabIndex = 16;
            this.RefAllResources.Text = "Ref All Resources";
            this.toolTip.SetToolTip(this.RefAllResources, "Consider all resources to be included, even if unused in the capture frame");
            this.RefAllResources.UseVisualStyleBackColor = true;
            // 
            // CaptureAllCmdLists
            // 
            this.CaptureAllCmdLists.Location = new System.Drawing.Point(139, 55);
            this.CaptureAllCmdLists.Name = "CaptureAllCmdLists";
            this.CaptureAllCmdLists.Size = new System.Drawing.Size(130, 20);
            this.CaptureAllCmdLists.TabIndex = 17;
            this.CaptureAllCmdLists.Text = "Capture All Cmd Lists";
            this.toolTip.SetToolTip(this.CaptureAllCmdLists, "When enabled, all deferred command lists will be saved even while idling.\r\nThis h" +
        "as an overhead but ensures if you hold onto a list it will be captured.");
            this.CaptureAllCmdLists.UseVisualStyleBackColor = true;
            // 
            // VerifyMapWrites
            // 
            this.VerifyMapWrites.Location = new System.Drawing.Point(275, 55);
            this.VerifyMapWrites.Name = "VerifyMapWrites";
            this.VerifyMapWrites.Size = new System.Drawing.Size(130, 20);
            this.VerifyMapWrites.TabIndex = 18;
            this.VerifyMapWrites.Text = "Verify Map() Writes";
            this.VerifyMapWrites.UseVisualStyleBackColor = true;
            // 
            // AutoStart
            // 
            this.AutoStart.Location = new System.Drawing.Point(411, 55);
            this.AutoStart.Name = "AutoStart";
            this.AutoStart.Size = new System.Drawing.Size(130, 20);
            this.AutoStart.TabIndex = 19;
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
            this.panel2.Controls.Add(this.launch);
            this.panel2.Location = new System.Drawing.Point(3, 709);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(209, 26);
            this.panel2.TabIndex = 8;
            // 
            // load
            // 
            this.load.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.load.Location = new System.Drawing.Point(50, 3);
            this.load.Name = "load";
            this.load.Size = new System.Drawing.Size(39, 23);
            this.load.TabIndex = 24;
            this.load.Text = "Load";
            this.toolTip.SetToolTip(this.load, "Load a saved set of capture settings");
            this.load.UseVisualStyleBackColor = true;
            this.load.Click += new System.EventHandler(this.load_Click);
            // 
            // save
            // 
            this.save.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.save.Location = new System.Drawing.Point(4, 3);
            this.save.Name = "save";
            this.save.Size = new System.Drawing.Size(40, 23);
            this.save.TabIndex = 23;
            this.save.Text = "Save";
            this.toolTip.SetToolTip(this.save, "Save these capture settings to file to load later");
            this.save.UseVisualStyleBackColor = true;
            this.save.Click += new System.EventHandler(this.save_Click);
            // 
            // close
            // 
            this.close.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.close.Location = new System.Drawing.Point(161, 3);
            this.close.Margin = new System.Windows.Forms.Padding(0);
            this.close.Name = "close";
            this.close.Size = new System.Drawing.Size(41, 23);
            this.close.TabIndex = 22;
            this.close.Text = "Close";
            this.close.UseVisualStyleBackColor = true;
            this.close.Click += new System.EventHandler(this.close_Click);
            // 
            // capture
            // 
            this.launch.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.launch.Location = new System.Drawing.Point(104, 3);
            this.launch.Margin = new System.Windows.Forms.Padding(0);
            this.launch.Name = "capture";
            this.launch.Size = new System.Drawing.Size(52, 23);
            this.launch.TabIndex = 21;
            this.launch.Text = "Launch";
            this.toolTip.SetToolTip(this.launch, "Trigger a capture of the selected program");
            this.launch.UseVisualStyleBackColor = true;
            this.launch.Click += new System.EventHandler(this.launch_Click);
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
            // cmdline
            // 
            this.cmdline.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.cmdline.Location = new System.Drawing.Point(137, 71);
            this.cmdline.Name = "cmdline";
            this.cmdline.Size = new System.Drawing.Size(51, 20);
            this.cmdline.TabIndex = 4;
            this.toolTip.SetToolTip(this.cmdline, "The command-line that will be passed to the executable on launch");
            // 
            // workDirBrowse
            // 
            this.workDirBrowse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.workDirBrowse.Location = new System.Drawing.Point(164, 45);
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
            this.workDirPath.Size = new System.Drawing.Size(22, 20);
            this.workDirPath.TabIndex = 2;
            this.toolTip.SetToolTip(this.workDirPath, "The working directory the executable will be launched in");
            this.workDirPath.TextChanged += new System.EventHandler(this.workDirPath_TextChanged);
            this.workDirPath.Enter += new System.EventHandler(this.workDirPath_Enter);
            this.workDirPath.Leave += new System.EventHandler(this.workDirPath_Leave);
            // 
            // exeBrowse
            // 
            this.exeBrowse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.exeBrowse.Location = new System.Drawing.Point(164, 18);
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
            this.exePath.Size = new System.Drawing.Size(22, 20);
            this.exePath.TabIndex = 0;
            this.toolTip.SetToolTip(this.exePath, "The executable file to launch");
            this.exePath.TextChanged += new System.EventHandler(this.exePath_TextChanged);
            this.exePath.DragDrop += new System.Windows.Forms.DragEventHandler(this.exePath_DragDrop);
            this.exePath.DragEnter += new System.Windows.Forms.DragEventHandler(this.exePath_DragEnter);
            // 
            // pidRefresh
            // 
            this.pidRefresh.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.pidRefresh.Location = new System.Drawing.Point(113, 128);
            this.pidRefresh.Name = "pidRefresh";
            this.pidRefresh.Size = new System.Drawing.Size(75, 23);
            this.pidRefresh.TabIndex = 6;
            this.pidRefresh.Text = "Refresh";
            this.toolTip.SetToolTip(this.pidRefresh, "Refresh the list of processes");
            this.pidRefresh.UseVisualStyleBackColor = true;
            this.pidRefresh.Click += new System.EventHandler(this.pidRefresh_Click);
            // 
            // pidList
            // 
            this.pidList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.pidList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            name,
            pid,
            this.winTitle});
            this.pidList.FullRowSelect = true;
            this.pidList.GridLines = true;
            this.pidList.HideSelection = false;
            this.pidList.LabelWrap = false;
            this.pidList.Location = new System.Drawing.Point(6, 54);
            this.pidList.MultiSelect = false;
            this.pidList.Name = "pidList";
            this.pidList.Size = new System.Drawing.Size(182, 68);
            this.pidList.TabIndex = 5;
            this.toolTip.SetToolTip(this.pidList, "Select the process to inject into - must not yet have utilised the target API");
            this.pidList.UseCompatibleStateImageBehavior = false;
            this.pidList.View = System.Windows.Forms.View.Details;
            this.pidList.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.pidList_ColumnClick);
            this.pidList.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.pidList_MouseDoubleClick);
            this.pidList.Resize += new System.EventHandler(this.pidList_Resize);
            // 
            // winTitle
            // 
            this.winTitle.Text = "Window Title";
            this.winTitle.Width = 599;
            // 
            // environmentDisplay
            // 
            this.environmentDisplay.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.environmentDisplay.BackColor = System.Drawing.SystemColors.Control;
            this.environmentDisplay.Cursor = System.Windows.Forms.Cursors.IBeam;
            this.environmentDisplay.Location = new System.Drawing.Point(137, 97);
            this.environmentDisplay.Name = "environmentDisplay";
            this.environmentDisplay.ReadOnly = true;
            this.environmentDisplay.Size = new System.Drawing.Size(22, 20);
            this.environmentDisplay.TabIndex = 9;
            this.toolTip.SetToolTip(this.environmentDisplay, "The working directory the executable will be launched in");
            // 
            // setEnv
            // 
            this.setEnv.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.setEnv.Location = new System.Drawing.Point(164, 96);
            this.setEnv.Name = "setEnv";
            this.setEnv.Size = new System.Drawing.Size(24, 20);
            this.setEnv.TabIndex = 10;
            this.setEnv.Text = "...";
            this.toolTip.SetToolTip(this.setEnv, "Browse for a working directory");
            this.setEnv.UseVisualStyleBackColor = true;
            this.setEnv.Click += new System.EventHandler(this.setEnv_Click);
            // 
            // processFilter
            // 
            this.processFilter.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.processFilter.Location = new System.Drawing.Point(6, 130);
            this.processFilter.Name = "processFilter";
            this.processFilter.Size = new System.Drawing.Size(103, 20);
            this.processFilter.TabIndex = 7;
            this.toolTip.SetToolTip(this.processFilter, "The working directory the executable will be launched in");
            this.processFilter.TextChanged += new System.EventHandler(this.processFilter_TextChanged);
            this.processFilter.Enter += new System.EventHandler(this.processFilter_Enter);
            this.processFilter.Leave += new System.EventHandler(this.processFilter_Leave);
            // 
            // mainTableLayout
            // 
            this.mainTableLayout.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.mainTableLayout.ColumnCount = 1;
            this.mainTableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.mainTableLayout.Controls.Add(this.programGroup, 0, 0);
            this.mainTableLayout.Controls.Add(this.panel2, 0, 6);
            this.mainTableLayout.Controls.Add(this.capOptsGroup, 0, 3);
            this.mainTableLayout.Controls.Add(this.processGroup, 0, 1);
            this.mainTableLayout.Controls.Add(this.actionsGroup, 0, 4);
            this.mainTableLayout.Controls.Add(this.globalGroup, 0, 5);
            this.mainTableLayout.Controls.Add(this.vulkanLayerWarn, 0, 2);
            this.mainTableLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this.mainTableLayout.Location = new System.Drawing.Point(0, 0);
            this.mainTableLayout.Name = "mainTableLayout";
            this.mainTableLayout.RowCount = 7;
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.mainTableLayout.Size = new System.Drawing.Size(215, 738);
            this.mainTableLayout.TabIndex = 8;
            this.mainTableLayout.Layout += new System.Windows.Forms.LayoutEventHandler(this.mainTableLayout_Layout);
            // 
            // programGroup
            // 
            this.programGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.programGroup.Controls.Add(this.setEnv);
            this.programGroup.Controls.Add(this.environmentDisplay);
            this.programGroup.Controls.Add(label6);
            this.programGroup.Controls.Add(label3);
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
            this.programGroup.Size = new System.Drawing.Size(195, 134);
            this.programGroup.TabIndex = 10;
            this.programGroup.TabStop = false;
            this.programGroup.Text = "Program";
            // 
            // processGroup
            // 
            this.processGroup.Controls.Add(this.processFilter);
            this.processGroup.Controls.Add(label5);
            this.processGroup.Controls.Add(this.pidList);
            this.processGroup.Controls.Add(this.pidRefresh);
            this.processGroup.Dock = System.Windows.Forms.DockStyle.Fill;
            this.processGroup.Location = new System.Drawing.Point(10, 164);
            this.processGroup.Margin = new System.Windows.Forms.Padding(10);
            this.processGroup.MinimumSize = new System.Drawing.Size(0, 160);
            this.processGroup.Name = "processGroup";
            this.processGroup.Size = new System.Drawing.Size(195, 160);
            this.processGroup.TabIndex = 9;
            this.processGroup.TabStop = false;
            this.processGroup.Text = "Process";
            // 
            // vulkanLayerWarn
            // 
            this.vulkanLayerWarn.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.vulkanLayerWarn.BackColor = System.Drawing.SystemColors.Info;
            this.vulkanLayerWarn.Cursor = System.Windows.Forms.Cursors.Hand;
            this.vulkanLayerWarn.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.vulkanLayerWarn.Image = global::renderdocui.Properties.Resources.information;
            this.vulkanLayerWarn.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.vulkanLayerWarn.Location = new System.Drawing.Point(3, 340);
            this.vulkanLayerWarn.Margin = new System.Windows.Forms.Padding(3, 10, 3, 10);
            this.vulkanLayerWarn.Name = "vulkanLayerWarn";
            this.vulkanLayerWarn.Size = new System.Drawing.Size(209, 38);
            this.vulkanLayerWarn.TabIndex = 13;
            this.vulkanLayerWarn.Text = "Warning: Vulkan capture is not configured. Click here to set up Vulkan capture.";
            this.vulkanLayerWarn.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.vulkanLayerWarn.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.vulkanLayerWarn.UseVisualStyleBackColor = false;
            this.vulkanLayerWarn.Click += new System.EventHandler(this.vulkanLayerWarn_Click);
            // 
            // CaptureDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoScroll = true;
            this.AutoScrollMinSize = new System.Drawing.Size(215, 0);
            this.ClientSize = new System.Drawing.Size(196, 754);
            this.Controls.Add(this.mainTableLayout);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "CaptureDialog";
            this.ShowHint = WeifenLuo.WinFormsUI.Docking.DockState.Document;
            this.Text = "CaptureDialog";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.CaptureDialog_FormClosing);
            this.Shown += new System.EventHandler(this.CaptureDialog_Shown);
            this.actionsGroup.ResumeLayout(false);
            this.actionsGroup.PerformLayout();
            this.actionsFlow.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.queuedCapFrame)).EndInit();
            this.globalGroup.ResumeLayout(false);
            this.globalGroup.PerformLayout();
            this.globalFlow.ResumeLayout(false);
            this.globalFlow.PerformLayout();
            this.capOptsGroup.ResumeLayout(false);
            this.capOptsGroup.PerformLayout();
            this.capOptsFlow.ResumeLayout(false);
            this.panel3.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.DelayForDebugger)).EndInit();
            this.panel2.ResumeLayout(false);
            this.mainTableLayout.ResumeLayout(false);
            this.mainTableLayout.PerformLayout();
            this.programGroup.ResumeLayout(false);
            this.programGroup.PerformLayout();
            this.processGroup.ResumeLayout(false);
            this.processGroup.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button launch;
        private System.Windows.Forms.Button close;
        private System.Windows.Forms.CheckBox AllowVSync;
        private System.Windows.Forms.CheckBox APIValidation;
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
        private System.Windows.Forms.CheckBox VerifyMapWrites;
        private System.Windows.Forms.CheckBox AutoStart;
        private System.Windows.Forms.ToolTip toolTip;
        private System.Windows.Forms.FlowLayoutPanel capOptsFlow;
        private System.Windows.Forms.TableLayoutPanel mainTableLayout;
        private System.Windows.Forms.GroupBox programGroup;
        private System.Windows.Forms.TextBox cmdline;
        private System.Windows.Forms.Button workDirBrowse;
        private System.Windows.Forms.TextBox workDirPath;
        private System.Windows.Forms.Button exeBrowse;
        private System.Windows.Forms.TextBox exePath;
        private System.Windows.Forms.GroupBox processGroup;
        private System.Windows.Forms.ListView pidList;
        private System.Windows.Forms.Button pidRefresh;
        private System.Windows.Forms.CheckBox CaptureAllCmdLists;
        private System.Windows.Forms.FlowLayoutPanel actionsFlow;
        private System.Windows.Forms.NumericUpDown queuedCapFrame;
        private System.Windows.Forms.CheckBox queueFrameCap;
        private System.Windows.Forms.GroupBox capOptsGroup;
        private System.Windows.Forms.FlowLayoutPanel globalFlow;
        private System.Windows.Forms.CheckBox toggleGlobalHook;
        private System.Windows.Forms.GroupBox actionsGroup;
        private System.Windows.Forms.GroupBox globalGroup;
        private System.Windows.Forms.Label globalLabel;
        private System.Windows.Forms.Button vulkanLayerWarn;
        private System.Windows.Forms.Button setEnv;
        private System.Windows.Forms.TextBox environmentDisplay;
        private System.Windows.Forms.ColumnHeader winTitle;
        private System.Windows.Forms.TextBox processFilter;

    }
}