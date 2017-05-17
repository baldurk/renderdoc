using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.IO;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;
using IronPython.Hosting;
using Microsoft.Scripting.Hosting;
using IronPython.Runtime.Exceptions;
using System.Threading;

namespace renderdocui.Windows.Dialogs
{
    public partial class PythonShell : DockContent
    {
        Core m_Core = null;
        
        private ScriptEngine pythonengine = null;
        private ScriptScope shellscope = null;

        ScintillaNET.Scintilla scriptEditor = null;

        private bool m_LibsLoaded = false;

        public PythonShell(Core core)
        {
            InitializeComponent();

            if (SystemInformation.HighContrast)
            {
                toolStrip1.Renderer = new ToolStripSystemRenderer();
                toolStrip2.Renderer = new ToolStripSystemRenderer();
            }

            shellTable.Dock = DockStyle.Fill;
            scriptTable.Dock = DockStyle.Fill;

            scriptEditor = new ScintillaNET.Scintilla();
            ((System.ComponentModel.ISupportInitialize)(scriptEditor)).BeginInit();

            scriptEditor.Dock = System.Windows.Forms.DockStyle.Fill;
            scriptEditor.Location = new System.Drawing.Point(3, 3);
            scriptEditor.Name = "scripteditor";
            scriptEditor.Font = new Font("Consolas", 8.25F, FontStyle.Regular, GraphicsUnit.Point, 0);

            scriptEditor.Margins.Left = 4;
            scriptEditor.Margins.Margin0.Width = 30;
            scriptEditor.Margins.Margin1.Width = 0;
            scriptEditor.Margins.Margin2.Width = 16;

            scriptEditor.Markers[0].BackColor = System.Drawing.Color.LightCoral;

            scriptEditor.ConfigurationManager.Language = "python";

            ((System.ComponentModel.ISupportInitialize)(scriptEditor)).EndInit();

            scriptEditor.KeyDown += new KeyEventHandler(scriptEditor_KeyDown);
            scriptEditor.TextChanged += new EventHandler(scriptEditor_TextChanged);

            scriptEditor.Scrolling.HorizontalWidth = 1;

            const uint SCI_SETSCROLLWIDTHTRACKING = 2516;
            scriptEditor.NativeInterface.SendMessageDirect(SCI_SETSCROLLWIDTHTRACKING, true);

            scriptSplit.Panel1.Controls.Add(scriptEditor);

            m_Core = core;

            pythonengine = NewEngine();

            mode_Changed(shellMode, null);

            newScript.PerformClick();

            clearCmd_Click(null, null);

            EnableButtons(true);
        }

        void scriptEditor_TextChanged(object sender, EventArgs e)
        {
            SetLineNumber(-1);
        }

        void scriptEditor_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Control)
            {
                if (e.KeyCode == Keys.O)
                {
                    openButton.PerformClick();
                    e.Handled = true;
                    e.SuppressKeyPress = true;
                }
                if (e.KeyCode == Keys.S)
                {
                    saveAs.PerformClick();
                    e.Handled = true;
                    e.SuppressKeyPress = true;
                }
            }
        }

        private ScriptEngine NewEngine()
        {
            var engine = Python.CreateEngine();

            List<string> searches = new List<string>(engine.GetSearchPaths());

            searches.Add(Directory.GetCurrentDirectory());

            string libspath = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "pythonlibs.zip");

            if (File.Exists(libspath))
            {
                m_LibsLoaded = true;
                searches.Add(libspath);
            }

            engine.SetSearchPaths(searches);

            return engine;
        }

        private ScriptScope NewScope(ScriptEngine engine)
        {
            engine.Runtime.LoadAssembly(typeof(PythonShell).Assembly);

            var scope = engine.CreateScope();

            scope.SetVariable("pyrenderdoc", m_Core);

            // try to import the RenderDoc namespace.
            // This isn't equivalent to scope.ImportModule
            try
            {
                engine.CreateScriptSourceFromString("import renderdoc").Execute(scope);
            }
            catch (Exception)
            {
            }

            return scope;
        }

        private Thread pythonThread = null;

        private MemoryStream stdout = null;
        private StreamWriter stdoutwriter = null;
        private StreamReader stdoutreader = null;
        private int linenum = -1;

        private string Execute(ScriptEngine engine, ScriptScope scope, string script)
        {
            stdout = new MemoryStream();
            stdoutwriter = new StreamWriter(stdout);
            stdoutreader = new StreamReader(stdout);

            engine.Runtime.IO.SetOutput(stdout, stdoutwriter);

            try
            {
                m_Core.Renderer.SetExceptionCatching(true);
                dynamic ret = engine.CreateScriptSourceFromString(script).Execute(scope);
                m_Core.Renderer.SetExceptionCatching(false);
                if (ret != null)
                {
                    stdoutwriter.Write(ret.ToString() + Environment.NewLine);
                    stdoutwriter.Flush();
                }
            }
            catch (Exception ex)
            {
                m_Core.Renderer.SetExceptionCatching(false);

                // IronPython throws so many exceptions, we don't want to kill the application
                // so we just swallow Exception to cover all the bases
                string exstr = engine.GetService<ExceptionOperations>().FormatException(ex);
                stdoutwriter.Write(exstr);
                stdoutwriter.Write(Environment.NewLine);
                stdoutwriter.Flush();
            }

            stdout.Seek(0, SeekOrigin.Begin);

            string output = stdoutreader.ReadToEnd();

            stdoutreader.Dispose();

            stdout = null;
            stdoutreader = null;
            stdoutwriter = null;

            return output;
        }

        private void mode_Changed(object sender, EventArgs e)
        {
            if (sender == shellMode)
            {
                shellMode.Checked = true;
                scriptMode.Checked = false;
                scriptTable.Visible = false;
                shellTable.Visible = true;

                Text = "Interactive Python Shell";

                interactiveInput_TextChanged(null, null);
            }
            else if (sender == scriptMode)
            {
                scriptMode.Checked = true;
                shellMode.Checked = false;
                shellTable.Visible = false;
                scriptTable.Visible = true;

                scriptOutput.Text = "";

                Text = "Python Script Execute";
            }
        }

        private List<string> history = new List<string>();
        int historyidx = -1;
        string workingtext = "";

        private void interactiveInput_KeyDown(object sender, KeyEventArgs e)
        {
            if (!e.Shift && e.KeyCode == Keys.Return)
            {
                executeCmd_Click(null, null);
                e.Handled = true;
                e.SuppressKeyPress = true;
            }

            bool moved = false;

            if (e.KeyCode == Keys.Down && historyidx > -1)
            {
                historyidx--;

                moved = true;
            }
            if (e.KeyCode == Keys.Up && historyidx + 1 < history.Count)
            {
                if (historyidx == -1)
                    workingtext = interactiveInput.Text;

                historyidx++;

                moved = true;
            }

            if (moved)
            {
                if (historyidx == -1)
                    interactiveInput.Text = workingtext;
                else
                    interactiveInput.Text = history[historyidx];

                interactiveInput.Select(interactiveInput.Text.Length, 0);
            }
        }

        private void interactiveInput_TextChanged(object sender, EventArgs e)
        {
            using (var g = interactiveInput.CreateGraphics())
            {
                SizeF MessageSize = g.MeasureString(interactiveInput.Text + "a", interactiveInput.Font,
                                                    interactiveInput.Width, new StringFormat(0));

                const int maxHeight = 100;

                if (MessageSize.Height <= maxHeight)
                {
                    interactiveInput.Height = 20 + (int)MessageSize.Height;
                    interactiveInput.ScrollBars = ScrollBars.None;
                }
                else
                {
                    interactiveInput.Height = 20 + maxHeight;
                    interactiveInput.ScrollBars = ScrollBars.Vertical;
                }
            }
        }

        private void interactiveInput_Layout(object sender, LayoutEventArgs e)
        {
            interactiveInput_TextChanged(null, null);
        }

        private void executeCmd_Click(object sender, EventArgs e)
        {
            var nl = Environment.NewLine;
            interactiveOutput.AppendText(">> " + interactiveInput.Text.Trim().Replace(nl, nl + ">> ") + nl);
            interactiveOutput.AppendText(Execute(pythonengine, shellscope, interactiveInput.Text));

            history.Insert(0, interactiveInput.Text);
            historyidx = -1;
            workingtext = "";

            interactiveInput.Text = "";
        }

        private void clearCmd_Click(object sender, EventArgs e)
        {
            interactiveOutput.Text = String.Format("RenderDoc Python console, powered by IronPython {0}{1}" +
                                "The 'pyrenderdoc' object is the Core class instance.{1}" +
                                "The 'renderdoc' module is available, as the matching namespace in C#.{1}",
                                IronPython.CurrentVersion.AssemblyFileVersion, Environment.NewLine);

            if (!m_LibsLoaded)
            {
                interactiveOutput.Text = String.Format("!!! pythonlibs.zip not found! Check installation !!!{0}" +
                    "!!! If building locally, ensure you have compiled python libraries: !!!{0}" +
                    "!!! Download IronPython-2.7.4 package, run this command and rebuild renderdocui !!!{0}" +
                    "cd renderdocui/3rdparty/ironpython/ && ./compilelibs.sh /path/to/IronPython-2.7.4{0}{0}{1}",
                    Environment.NewLine, interactiveOutput.Text);
            }

            shellscope = NewScope(pythonengine);
        }

        private static PythonShell me = null;

        private static TracebackDelegate PythonTrace(TraceBackFrame frame, string result, object payload)
        {
            if(me != null)
                me.TraceCallback(frame, result, payload);
            return PythonTrace;
        }
        
        private void TraceCallback(TraceBackFrame frame, string result, object payload)
        {
            if (result == "exception")
            {
                System.Diagnostics.Trace.WriteLine("On line " + frame.f_lineno.ToString());
            }
            linenum = (int)frame.f_lineno - 1;

            stdoutwriter.Flush();
            stdout.Seek(0, SeekOrigin.Begin);
            string output = stdoutreader.ReadToEnd();
            stdout.Seek(0, SeekOrigin.Begin);
            stdout.SetLength(0);

            if (output.Length > 0)
            {
            this.BeginInvoke(new Action(() =>
            {
                scriptOutput.Text += output;
                scriptOutput.SelectionStart = scriptOutput.TextLength;
                scriptOutput.ScrollToCaret();
            }));
        }
        }

        bool recurse = false;
        
        private void SetLineNumber(int lineNum)
        {
            if (recurse || me == null || me.IsDisposed || me.scriptEditor == null)
                return;

            recurse = true;

            for (int i = 0; i < me.scriptEditor.Lines.Count; i++)
            {
                me.scriptEditor.Lines[i].DeleteMarker(0);
            }

            if (lineNum >= 0 && lineNum < me.scriptEditor.Lines.Count)
            {
                me.scriptEditor.Lines[lineNum].AddMarker(0);
            }

            recurse = false;
        }

        private void EnableButtons(bool enable)
        {
            shellMode.Enabled = scriptMode.Enabled =
                newScript.Enabled = openButton.Enabled = saveAs.Enabled =
                runButton.Enabled = enable;
            abortButton.Enabled = !enable;
        }

        private void abortButton_Click(object sender, EventArgs e)
        {
            if (pythonThread != null)
                pythonThread.Abort();
        }

        private void runButton_Click(object sender, EventArgs e)
        {
            var scriptscope = NewScope(pythonengine);

            me = this;

            var script = scriptEditor.Text;

            scriptOutput.Text = "";
            linenum = -1;

            EnableButtons(false);

            linenumTimer.Enabled = true;
            linenumTimer.Start();

            pythonThread = Helpers.NewThread(new ThreadStart(() =>
            {
                pythonengine.SetTrace(PythonTrace);

                string output = "";
                // ignore output, the trace handler above will print output
                try
                {
                    output = Execute(pythonengine, scriptscope, script);
                }
                catch (ThreadAbortException)
                {
                    // python was interrupted
                    Thread.ResetAbort();
                }

                linenumTimer.Stop();
                pythonengine.SetTrace(null);

                this.BeginInvoke(new Action(() =>
                {
                    pythonThread = null;

                    scriptOutput.Text += output;
                    scriptOutput.SelectionStart = scriptOutput.TextLength;
                    scriptOutput.ScrollToCaret();

                    SetLineNumber(linenum);

                    EnableButtons(true);
                }));
            }));

            pythonThread.Start();
        }

        private string ValidData(IDataObject d)
        {
            var fmts = new List<string>(d.GetFormats());

            if (fmts.Contains("FileName"))
            {
                var data = d.GetData("FileName") as Array;

                if (data != null && data.Length == 1 && data.GetValue(0) is string)
                {
                    var filename = (string)data.GetValue(0);

                    try
                    {
                        if (File.Exists(filename) && Path.GetExtension(filename).ToUpperInvariant() == ".PY")
                            return Path.GetFullPath(filename);
                    }
                    catch (ArgumentException)
                    {
                        // invalid path etc
                    }
                }
            }

            return "";
        }

        private void shell_DragEnter(object sender, DragEventArgs e)
        {
            if (ValidData(e.Data).Length > 0)
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;
        }

        private void shell_DragDrop(object sender, DragEventArgs e)
        {
            string fn = ValidData(e.Data);
            if (fn.Length > 0)
            {
                try
                {
                    scriptEditor.Text = File.ReadAllText(fn);
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't open file " + saveDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot open script",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }

                mode_Changed(scriptMode, null);
            }
        }

        private void openButton_Click(object sender, EventArgs e)
        {
            DialogResult res = openDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                try
                {
                    scriptEditor.Text = File.ReadAllText(openDialog.FileName);
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't load from " + openDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot open script",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void saveAs_Click(object sender, EventArgs e)
        {
            DialogResult res = saveDialog.ShowDialog();

            if (res == DialogResult.OK)
            {
                try
                {
                    File.WriteAllText(saveDialog.FileName, scriptEditor.Text);
                }
                catch (System.Exception ex)
                {
                    MessageBox.Show("Couldn't save to " + saveDialog.FileName + Environment.NewLine + ex.ToString(), "Cannot save script",
                                                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void newScript_Click(object sender, EventArgs e)
        {
            scriptEditor.Text = String.Format("# RenderDoc Python scripts, powered by IronPython {0}\n" +
                                "# The 'pyrenderdoc' object is the Core class instance.\n" +
                                "# The 'renderdoc' module is available, as the matching namespace in C#\n\n",
                                IronPython.CurrentVersion.AssemblyFileVersion);

            if (!m_LibsLoaded)
            {
                scriptEditor.Text += "# !!! pythonlibs.zip not found! Check installation !!!\n" +
                    "# !!! If building locally, ensure you have compiled python libraries: !!!\n" +
                    "# !!! Download IronPython-2.7.4 package, run this command and rebuild renderdocui !!!\n" +
                    "# cd renderdocui/3rdparty/ironpython/ && ./compilelibs.sh /path/to/IronPython-2.7.4\n\n";
            }

            scriptEditor.Text = scriptEditor.Text.Replace("\n", Environment.NewLine);
        }

        private void linenumTimer_Tick(object sender, EventArgs e)
        {
            SetLineNumber(linenum);
        }
    }
}
