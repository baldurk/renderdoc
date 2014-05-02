/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Reflection;
using System.IO;
using System.Text;
using System.Windows.Forms;
using WeifenLuo.WinFormsUI.Docking;
using renderdocui.Code;
using renderdoc;
using System.Text.RegularExpressions;

namespace renderdocui.Windows
{
    public partial class ShaderViewer : DockContent, ILogViewerForm
    {
        public delegate void SaveMethod(ShaderViewer viewer, Dictionary<string, string> fileText);
        public delegate void CloseMethod();

        public const int CURRENT_MARKER = 0;
        public const int BREAKPOINT_MARKER = 2;
        public const int FINISHED_MARKER = 4;

        private List<int> CurrentLineMarkers = new List<int>();
        private List<int> BreakpointMarkers = new List<int>();
        private List<int> FinishedMarkers = new List<int>();

        private Core m_Core = null;
        private ShaderReflection m_ShaderDetails = null;
        private D3D11PipelineState.ShaderStage m_Stage = null;
        private ShaderDebugTrace m_Trace = null;
        private ScintillaNET.Scintilla m_DisassemblyView = null;

        private DockContent m_ErrorsDock = null;
        private DockContent m_ConstantsDock = null;
        private DockContent m_VariablesDock = null;
        private DockContent m_WatchDock = null;

        private int CurrentStep_;
        public int CurrentStep
        {
            get
            {
                return CurrentStep_;
            }
            set
            {
                if (m_Trace != null && m_Trace.states != null)
                {
                    CurrentStep_ = Math.Min(m_Trace.states.Length - 1, Math.Max(0, value));
                }
                else
                {
                    CurrentStep_ = 0;
                }

                UpdateDebugging();
            }
        }

        ScintillaNET.Scintilla CurrentScintilla
        {
            get
            {
                foreach (var s in m_Scintillas)
                {
                    if (s.Focused)
                        return s;
                }

                return null;
            }
        }

        private string FriendlyName(string disasm, string stem, ShaderConstant[] vars)
        {
            foreach (var v in vars)
            {
                if (v.type.descriptor.rows == 0 && v.type.descriptor.cols == 0 && v.type.members.Length > 0)
                {
                    disasm = FriendlyName(disasm, stem, v.type.members);
                }
                else if (v.type.descriptor.rows > 0 && v.type.descriptor.cols > 0)
                {
                    uint numRegs = v.type.descriptor.rows;

                    for (uint r = 0; r < numRegs; r++)
                    {
                        var reg = string.Format("{0}[{1}]", stem, v.reg.vec + r);

                        int compStart = r == 0 ? (int)v.reg.comp : 0;
                        int compEnd = compStart + (int)v.type.descriptor.cols;

                        var comps = "xyzw".Substring(compStart, compEnd - compStart);

                        var regexp = string.Format(", (-|abs\\()?{0}\\.([{1}]*)([^xyzw])", Regex.Escape(reg), comps);

                        var match = Regex.Match(disasm, regexp);

                        while (match.Success)
                        {
                            var swizzle = match.Groups[2].Value.ToCharArray();

                            for (int c = 0; c < swizzle.Length; c++)
                            {
                                int val = "xyzw".IndexOf(swizzle[c]);
                                swizzle[c] = "xyzw"[val - compStart];
                            }

                            var name = numRegs == 1 ? v.name : string.Format("{0}[{1}]", v.name, r);

                            var replacement = string.Format(", {0}{1}.{2}{3}",
                                                    match.Groups[1].Value, name, new string(swizzle), match.Groups[3].Value);

                            disasm = disasm.Remove(match.Index, match.Length);

                            disasm = disasm.Insert(match.Index, replacement);

                            match = Regex.Match(disasm, regexp);
                        }
                    }
                }
            }

            return disasm;
        }

        private List<ScintillaNET.Scintilla> m_Scintillas = new List<ScintillaNET.Scintilla>();
        private SaveMethod m_SaveCallback = null;
        private CloseMethod m_CloseCallback = null;

        public ShaderViewer(Core core, bool custom, string entry, Dictionary<string, string> files, SaveMethod saveCallback, CloseMethod closeCallback)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            this.SuspendLayout();

            mainLayout.Dock = DockStyle.Fill;

            snippetDropDown.Visible = custom;

            debuggingStrip.Visible = false;

            inSigBox.Visible = false;
            outSigBox.Visible = false;

            m_Core = core;
            m_SaveCallback = saveCallback;
            m_CloseCallback = closeCallback;

            DockContent sel = null;

            foreach (var f in files)
            {
                var name = f.Key;

                ScintillaNET.Scintilla scintilla1 = MakeEditor("scintilla" + name, true);
                scintilla1.Text = f.Value;
                scintilla1.IsReadOnly = false;
                scintilla1.Tag = name;

                scintilla1.PreviewKeyDown += new PreviewKeyDownEventHandler(scintilla1_PreviewKeyDown);
                scintilla1.KeyDown += new KeyEventHandler(scintilla1_KeyDown);

                m_Scintillas.Add(scintilla1);

                var w = Helpers.WrapDockContent(dockPanel, scintilla1, name);
                w.CloseButton = false;
                w.CloseButtonVisible = false;
                w.Show(dockPanel);

                if (f.Value.Contains(entry))
                    sel = w;

                Text = string.Format("{0} - Edit ({1})", entry, f.Key);
            }

            if(sel != null)
                sel.Show();

            ShowConstants();
            ShowVariables();
            ShowWatch();
            ShowErrors();

            {
                m_ConstantsDock.Hide();
                m_VariablesDock.Hide();
                m_WatchDock.Hide();
            }

            this.ResumeLayout(false);
        }

        void scintilla1_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.S && e.Control)
            {
                e.SuppressKeyPress = true;
            }
        }

        private HashSet<int> m_Breakpoints = new HashSet<int>();
            
        void scintilla1_DebuggingKeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.F9)
            {
                var sc = sender as ScintillaNET.Scintilla;

                var line = sc.Lines.FromPosition(sc.CurrentPos);

                while (line != null)
                {
                    var trimmed = line.Text.Trim();

                    int colon = trimmed.IndexOf(":");

                    if (colon >= 0)
                    {
                        string start = trimmed.Substring(0, colon);

                        int lineNum = -1;

                        if (int.TryParse(start, out lineNum))
                        {
                            if (line.GetMarkers().Contains(sc.Markers[BREAKPOINT_MARKER]))
                            {
                                line.DeleteMarkerSet(BreakpointMarkers);
                                m_Breakpoints.Remove(lineNum);
                            }
                            else
                            {
                                line.AddMarkerSet(BreakpointMarkers);
                                m_Breakpoints.Add(lineNum);
                            }

                            sc.Invalidate();
                            return;
                        }
                    }

                    line = line.Next;
                }
            }
        }

        void scintilla1_PreviewKeyDown(object sender, PreviewKeyDownEventArgs e)
        {
            if (e.KeyCode == Keys.S && e.Control)
            {
                saveButton_Click(null, null);
                e.IsInputKey = true;
            }
        }

        public ShaderViewer(Core core, ShaderReflection shader, ShaderStageType stage, ShaderDebugTrace trace)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            this.SuspendLayout();

            mainLayout.Dock = DockStyle.Fill;

            m_Core = core;
            m_ShaderDetails = shader;
            m_Trace = trace;
            m_Stage = null;
            switch (stage)
            {
                case ShaderStageType.Vertex: m_Stage = m_Core.CurD3D11PipelineState.m_VS; break;
                case ShaderStageType.Domain: m_Stage = m_Core.CurD3D11PipelineState.m_DS; break;
                case ShaderStageType.Hull: m_Stage = m_Core.CurD3D11PipelineState.m_HS; break;
                case ShaderStageType.Geometry: m_Stage = m_Core.CurD3D11PipelineState.m_GS; break;
                case ShaderStageType.Pixel: m_Stage = m_Core.CurD3D11PipelineState.m_PS; break;
                case ShaderStageType.Compute: m_Stage = m_Core.CurD3D11PipelineState.m_CS; break;
            }

            var disasm = shader.Disassembly;

            if (m_Core.Config.ShaderViewer_FriendlyNaming)
            {
                for (int i = 0; i < m_ShaderDetails.ConstantBlocks.Length; i++)
                {
                    var stem = string.Format("cb{0}", i);

                    var cbuf = m_ShaderDetails.ConstantBlocks[i];

                    if (cbuf.variables.Length == 0)
                        continue;

                    disasm = FriendlyName(disasm, stem, cbuf.variables);
                }

                foreach (var r in m_ShaderDetails.Resources)
                {
                    if (r.IsSRV)
                    {
                        var needle = string.Format(", t{0}([^0-9])", r.bindPoint);
                        var replacement = string.Format(", {0}$1", r.name);

                        Regex rgx = new Regex(needle);
                        disasm = rgx.Replace(disasm, replacement);
                    }
                    if (r.IsSampler)
                    {
                        var needle = string.Format(", s{0}([^0-9])", r.bindPoint);
                        var replacement = string.Format(", {0}$1", r.name);

                        Regex rgx = new Regex(needle);
                        disasm = rgx.Replace(disasm, replacement);
                    }
                    if (r.IsUAV)
                    {
                        var needle = string.Format(", u{0}([^0-9])", r.bindPoint);
                        var replacement = string.Format(", {0}$1", r.name);

                        Regex rgx = new Regex(needle);
                        disasm = rgx.Replace(disasm, replacement);
                    }
                }
            }

            {
                m_DisassemblyView = MakeEditor("scintillaDisassem", false);
                m_DisassemblyView.Text = disasm;
                m_DisassemblyView.IsReadOnly = true;
                m_DisassemblyView.TabIndex = 0;

                m_DisassemblyView.KeyDown += new KeyEventHandler(m_DisassemblyView_KeyDown);

                m_DisassemblyView.Markers[CURRENT_MARKER].BackColor = System.Drawing.Color.LightCoral;
                m_DisassemblyView.Markers[CURRENT_MARKER].Symbol = ScintillaNET.MarkerSymbol.Background;
                m_DisassemblyView.Markers[CURRENT_MARKER+1].BackColor = System.Drawing.Color.LightCoral;
                m_DisassemblyView.Markers[CURRENT_MARKER+1].Symbol = ScintillaNET.MarkerSymbol.ShortArrow;

                CurrentLineMarkers.Add(CURRENT_MARKER);
                CurrentLineMarkers.Add(CURRENT_MARKER+1);

                m_DisassemblyView.Markers[FINISHED_MARKER].BackColor = System.Drawing.Color.LightSlateGray;
                m_DisassemblyView.Markers[FINISHED_MARKER].Symbol = ScintillaNET.MarkerSymbol.Background;
                m_DisassemblyView.Markers[FINISHED_MARKER + 1].BackColor = System.Drawing.Color.LightSlateGray;
                m_DisassemblyView.Markers[FINISHED_MARKER + 1].Symbol = ScintillaNET.MarkerSymbol.RoundRectangle;

                FinishedMarkers.Add(FINISHED_MARKER);
                FinishedMarkers.Add(FINISHED_MARKER + 1);

                m_DisassemblyView.Markers[BREAKPOINT_MARKER].BackColor = System.Drawing.Color.Red;
                m_DisassemblyView.Markers[BREAKPOINT_MARKER].Symbol = ScintillaNET.MarkerSymbol.Background;
                m_DisassemblyView.Markers[BREAKPOINT_MARKER+1].BackColor = System.Drawing.Color.Red;
                m_DisassemblyView.Markers[BREAKPOINT_MARKER+1].Symbol = ScintillaNET.MarkerSymbol.Circle;

                BreakpointMarkers.Add(BREAKPOINT_MARKER);
                BreakpointMarkers.Add(BREAKPOINT_MARKER + 1);

                m_Scintillas.Add(m_DisassemblyView);

                var w = Helpers.WrapDockContent(dockPanel, m_DisassemblyView, "Disassembly");
                w.DockState = DockState.Document;
                w.Show();

                w.CloseButton = false;
                w.CloseButtonVisible = false;
            }

            if (shader.DebugInfo.entryFunc != "" && shader.DebugInfo.files.Length > 0)
            {
                Text = shader.DebugInfo.entryFunc+ "()";

                DockContent sel = null;
                foreach (var f in shader.DebugInfo.files)
                {
                    var name = Path.GetFileName(f.filename);

                    ScintillaNET.Scintilla scintilla1 = MakeEditor("scintilla" + name, true);
                    scintilla1.Text = f.filetext;
                    scintilla1.IsReadOnly = true;

                    scintilla1.Tag = name;

                    var w = Helpers.WrapDockContent(dockPanel, scintilla1, name);
                    w.CloseButton = false;
                    w.CloseButtonVisible = false;
                    w.Show(dockPanel);

                    m_Scintillas.Add(scintilla1);

                    if (f.filetext.Contains(shader.DebugInfo.entryFunc))
                        sel = w;
                }

                if (trace != null || sel == null)
                    sel = (DockContent)m_DisassemblyView.Parent;

                sel.Show();
            }

            ShowConstants();
            ShowVariables();
            ShowWatch();
            ShowErrors();

            editStrip.Visible = false;

            m_ErrorsDock.Hide();

            if (trace == null)
            {
                debuggingStrip.Visible = false;
                m_ConstantsDock.Hide();
                m_VariablesDock.Hide();
                m_WatchDock.Hide();

                var insig = Helpers.WrapDockContent(dockPanel, inSigBox);
                insig.CloseButton = insig.CloseButtonVisible = false;

                var outsig = Helpers.WrapDockContent(dockPanel, outSigBox);
                outsig.CloseButton = outsig.CloseButtonVisible = false;

                insig.Show(dockPanel, DockState.DockBottom);
                outsig.Show(insig.Pane, DockAlignment.Right, 0.5);

                foreach (var s in m_ShaderDetails.InputSig)
                {
                    string name = s.varName == "" ? s.semanticName : String.Format("{0} ({1})", s.varName, s.semanticName);
                    var node = inSig.Nodes.Add(new object[] { name, s.semanticIndex, s.regIndex, s.TypeString, s.systemValue.ToString(),
                                                                SigParameter.GetComponentString(s.regChannelMask), SigParameter.GetComponentString(s.channelUsedMask) });
                }

                bool multipleStreams = false;
                for (int i = 0; i < m_ShaderDetails.OutputSig.Length; i++)
                {
                    if (m_ShaderDetails.OutputSig[i].stream > 0)
                    {
                        multipleStreams = true;
                        break;
                    }
                }

                foreach (var s in m_ShaderDetails.OutputSig)
                {
                    string name = s.varName == "" ? s.semanticName : String.Format("{0} ({1})", s.varName, s.semanticName);

                    if(multipleStreams)
                        name = String.Format("Stream {0} : {1}", s.stream, name);

                    var node = outSig.Nodes.Add(new object[] { name, s.semanticIndex, s.regIndex, s.TypeString, s.systemValue.ToString(),
                                                                SigParameter.GetComponentString(s.regChannelMask), SigParameter.GetComponentString(s.channelUsedMask) });
                }
            }
            else
            {
                inSigBox.Visible = false;
                outSigBox.Visible = false;

                m_DisassemblyView.Margins.Margin1.Width = 20;

                m_DisassemblyView.Margins.Margin2.Width = 0;

                m_DisassemblyView.Margins.Margin3.Width = 20;
                m_DisassemblyView.Margins.Margin3.IsMarkerMargin = true;
                m_DisassemblyView.Margins.Margin3.IsFoldMargin = false;
                m_DisassemblyView.Margins.Margin3.Type = ScintillaNET.MarginType.Symbol;

                m_DisassemblyView.Margins.Margin1.Mask = (int)m_DisassemblyView.Markers[BREAKPOINT_MARKER + 1].Mask;
                m_DisassemblyView.Margins.Margin3.Mask &= ~((int)m_DisassemblyView.Markers[BREAKPOINT_MARKER + 1].Mask);

                m_DisassemblyView.KeyDown += new KeyEventHandler(scintilla1_DebuggingKeyDown);

                watchRegs.Items.Add(new ListViewItem(new string[] { "", "", "" }));
            }

            CurrentStep = 0;

            this.ResumeLayout(false);
        }

        private ScintillaNET.Scintilla MakeEditor(string name, bool hlsl)
        {
            ScintillaNET.Scintilla scintilla1 = new ScintillaNET.Scintilla();
            ((System.ComponentModel.ISupportInitialize)(scintilla1)).BeginInit();

            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ShaderViewer));

            // 
            // scintilla1
            // 
            scintilla1.Dock = System.Windows.Forms.DockStyle.Fill;
            scintilla1.Location = new System.Drawing.Point(3, 3);
            scintilla1.Margins.Left = 4;
            scintilla1.Margins.Margin0.Width = 30;
            scintilla1.Margins.Margin1.Width = 0;
            scintilla1.Margins.Margin2.Width = 16;
            scintilla1.Name = name;
            scintilla1.Size = new System.Drawing.Size(581, 494);

            scintilla1.Click += new EventHandler(scintilla1_Click);

            scintilla1.Indicators[4].Style = ScintillaNET.IndicatorStyle.RoundBox;
            scintilla1.Indicators[4].Color = Color.DarkGreen;

            ((System.ComponentModel.ISupportInitialize)(scintilla1)).EndInit();

            var hlslpath = Path.Combine(Core.ConfigDirectory, "hlsl.xml");

            if (!File.Exists(hlslpath) ||
                File.GetLastWriteTimeUtc(hlslpath).CompareTo(File.GetLastWriteTimeUtc(Assembly.GetExecutingAssembly().Location)) < 0)
            {
                using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("renderdocui.Resources.hlsl.xml"))
                {
                    using (StreamReader reader = new StreamReader(stream))
                    {
                        File.WriteAllText(hlslpath, reader.ReadToEnd());
                    }
                }
            }

            if (hlsl)
            {
                scintilla1.Lexing.LexerLanguageMap["hlsl"] = "cpp";
                scintilla1.ConfigurationManager.CustomLocation = Core.ConfigDirectory;
                scintilla1.ConfigurationManager.Language = "hlsl";
                scintilla1.Lexing.SetProperty("lexer.cpp.track.preprocessor", "0");
            }
            else
            {
                scintilla1.ConfigurationManager.Language = "asm";
            }

            scintilla1.Scrolling.HorizontalWidth = 1;

            const uint SCI_SETSCROLLWIDTHTRACKING = 2516;
            scintilla1.NativeInterface.SendMessageDirect(SCI_SETSCROLLWIDTHTRACKING, true);

            return scintilla1;
        }

        List<ScintillaNET.Range> m_PrevRanges = new List<ScintillaNET.Range>();

        void scintilla1_Click(object sender, EventArgs e)
        {
            ScintillaNET.Scintilla scintilla1 = sender as ScintillaNET.Scintilla;

            string word = scintilla1.GetWordFromPosition(scintilla1.CurrentPos);

            var match = Regex.Match(word, "^[rvo][0-9]+$");

            foreach (ScintillaNET.Range r in m_PrevRanges)
            {
                r.ClearIndicator(4);
            }

            m_PrevRanges.Clear();

            if (match.Success)
            {
                var matches = Regex.Matches(scintilla1.Text, word + "\\.[xyzwrgba]+");

                foreach(Match m in matches)
                {
                    var r = scintilla1.GetRange(m.Index, m.Index + m.Length);
                    m_PrevRanges.Add(r);
                    r.SetIndicator(4);
                }
            }
        }

        public void ShowErrors(string errs)
        {
            errors.Text = errs.Replace("\n", Environment.NewLine);
        }

        public string StringRep(ShaderVariable var, bool useType)
        {
            if (displayInts.Checked || (useType && var.type == VarType.Int))
                return var.Row(0, VarType.Int);

            if (useType && var.type == VarType.UInt)
                return var.Row(0, VarType.UInt);

            return var.Row(0, VarType.Float).ToString();
        }

        public void UpdateDebugging()
        {
            if (m_Trace == null || m_Trace.states.Length == 0)
            {
                //curInstruction.Text = "0";

                for (int i = 0; i < m_DisassemblyView.Lines.Count; i++)
                {
                    m_DisassemblyView.Lines[i].DeleteMarkerSet(CurrentLineMarkers);
                    m_DisassemblyView.Lines[i].DeleteMarkerSet(FinishedMarkers);
                }

                return;
            }

            var state = m_Trace.states[CurrentStep];

            //curInstruction.Text = CurrentStep.ToString();

            UInt32 nextInst = state.nextInstruction;
            bool done = false;

            if (CurrentStep == m_Trace.states.Length - 1)
            {
                nextInst--;
                done = true;
            }

            // add current instruction marker
            for (int i = 0; i < m_DisassemblyView.Lines.Count; i++)
            {
                m_DisassemblyView.Lines[i].DeleteMarkerSet(CurrentLineMarkers);
                m_DisassemblyView.Lines[i].DeleteMarkerSet(FinishedMarkers);

                if (m_DisassemblyView.Lines[i].Text.Trim().StartsWith(nextInst.ToString() + ":"))
                {
                    m_DisassemblyView.Lines[i].AddMarkerSet(done ? FinishedMarkers : CurrentLineMarkers);
                    m_DisassemblyView.Caret.LineNumber = i;

                    if (!m_DisassemblyView.Lines[i].IsVisible)
                        m_DisassemblyView.Scrolling.ScrollToCaret();
                }
            }

            m_DisassemblyView.Invalidate();

            if (constantRegs.Nodes.IsEmpty())
            {
                constantRegs.BeginUpdate();

                for (int i = 0; i < m_Trace.cbuffers.Length; i++)
                {
                    for (int j = 0; j < m_Trace.cbuffers[i].variables.Length; j++)
                    {
                        if (m_Trace.cbuffers[i].variables[j].rows > 0 || m_Trace.cbuffers[i].variables[j].columns > 0)
                            constantRegs.Nodes.Add(new TreelistView.Node(new object[] {
                                m_Trace.cbuffers[i].variables[j].name,
                                "cbuffer",
                                StringRep(m_Trace.cbuffers[i].variables[j], false)
                            }));
                    }
                }

                foreach (var input in m_Trace.inputs)
                {
                  constantRegs.Nodes.Add(new TreelistView.Node(new object[] { input.name, input.type.ToString() + " input", StringRep(input, true) }));
                }

                var pipestate = m_Core.CurD3D11PipelineState;

                foreach (var slot in m_ShaderDetails.Resources)
                {
                    if (slot.IsSampler)
                        continue;

                    var res = m_Stage.SRVs[slot.bindPoint];

                    if (slot.IsUAV)
                    {
                        if(m_Stage.stage == ShaderStageType.Pixel)
                            res = pipestate.m_OM.UAVs[slot.bindPoint - pipestate.m_OM.UAVStartSlot];
                        else
                            res = m_Stage.UAVs[slot.bindPoint];
                    }

                    bool found = false;

                    var name = slot.bindPoint + " (" + slot.name + ")";

                    foreach (var tex in m_Core.CurTextures)
                    {
                        if (tex.ID == res.Resource)
                        {
                            constantRegs.Nodes.Add(new TreelistView.Node(new object[] {
                                "t" + name, "Texture",
                                tex.width + "x" + tex.height + "x" + (tex.depth > 1 ? tex.depth : tex.arraysize) +
                                "[" + tex.mips + "] @ " + tex.format + " - " + tex.name
                            }));

                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        foreach (var buf in m_Core.CurBuffers)
                        {
                            if (buf.ID == res.Resource)
                            {
                                string prefix = "u";

                                if (slot.IsSRV)
                                    prefix = "t";

                                constantRegs.Nodes.Add(new TreelistView.Node(new object[] {
                                    prefix + name, "Buffer",
                                    buf.length + " - " + buf.name
                                }));

                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found)
                    {
                        string prefix = "u";

                        if (slot.IsSRV)
                            prefix = "t";

                        constantRegs.Nodes.Add(new TreelistView.Node(new object[] {
                                    prefix + name, "Resource",
                                    "unknown"
                                }));
                    }
                }

                constantRegs.EndUpdate();
            }
            else
            {
                constantRegs.BeginUpdate();

                int c = 0;

                for (int i = 0; i < m_Trace.cbuffers.Length; i++)
                {
                    for (int j = 0; j < m_Trace.cbuffers[i].variables.Length; j++)
                    {
                        if (m_Trace.cbuffers[i].variables[j].rows > 0 || m_Trace.cbuffers[i].variables[j].columns > 0)
                            constantRegs.Nodes[c++].SetData(new object[] {
                                    m_Trace.cbuffers[i].variables[j].name,
                                    "cbuffer",
                                    StringRep(m_Trace.cbuffers[i].variables[j], false)
                            });
                    }
                }

                constantRegs.EndUpdate();
            }

            if (variableRegs.Nodes.IsEmpty())
            {
                for (int i = 0; i < state.registers.Length; i++)
                    variableRegs.Nodes.Add("a");

                for (int i = 0; i < state.outputs.Length; i++)
                    variableRegs.Nodes.Add("a");
            }

            variableRegs.BeginUpdate();

            int v = 0;

            for (int i = 0; i < state.registers.Length; i++)
            {
                variableRegs.Nodes[v++].SetData(new object[] { state.registers[i].name, "register", StringRep(state.registers[i], false) });
            }

            for (int i = 0; i < state.outputs.Length; i++)
            {
                variableRegs.Nodes[v++].SetData(new object[] { state.outputs[i].name, "register", StringRep(state.outputs[i], false) });
            }

            variableRegs.EndUpdate();

            watchRegs.BeginUpdate();

            for (int i = 0; i < watchRegs.Items.Count-1; i++)
            {
                ListViewItem item = watchRegs.Items[i];
                item.SubItems[1].Text = "register";

                string reg = item.SubItems[0].Text.Trim();

                var regexp = "^([rvo])([0-9]+)(\\.[xyzwrgba]+)?(,[xfiud])?$";

                var match = Regex.Match(reg, regexp);

                if (match.Success)
                {
                    var regtype = match.Groups[1].Value;
                    var regidx = match.Groups[2].Value;
                    var swizzle = match.Groups[3].Value.Replace(".", "");
                    var regcast = match.Groups[4].Value.Replace(",", "");

                    if (regcast == "")
                    {
                        if (displayInts.Checked)
                            regcast = "i";
                        else
                            regcast = "f";
                    }

                    ShaderVariable[] vars = null;

                    if (regtype == "r")
                        vars = state.registers;
                    else if (regtype == "v")
                        vars = m_Trace.inputs;
                    else if (regtype == "o")
                        vars = state.outputs;

                    int regindex = -1;

                    if (int.TryParse(regidx, out regindex))
                    {
                        if (regindex >= 0 && regindex < vars.Length)
                        {
                            ShaderVariable vr = vars[regindex];

                            if (swizzle == "")
                            {
                                swizzle = "xyzw".Substring(0, (int)vr.columns);

                                if (regcast == "d")
                                    swizzle = "xy";
                            }

                            string val = "";

                            for (int s = 0; s < swizzle.Length; s++)
                            {
                                char swiz = swizzle[s];

                                int elindex = 0;
                                if (swiz == 'x' || swiz == 'r') elindex = 0;
                                if (swiz == 'y' || swiz == 'g') elindex = 1;
                                if (swiz == 'z' || swiz == 'b') elindex = 2;
                                if (swiz == 'w' || swiz == 'a') elindex = 3;

                                if (regcast == "i")
                                    val += vr.value.iv[elindex];
                                else if (regcast == "f")
                                    val += Formatter.Format(vr.value.fv[elindex]);
                                else if (regcast == "u")
                                    val += vr.value.uv[elindex];
                                else if (regcast == "x")
                                    val += String.Format("0x{0:X8}", vr.value.uv[elindex]);
                                else if (regcast == "d")
                                {
                                    if (elindex < 2)
                                        val += vr.value.dv[elindex];
                                    else
                                        val += "-";
                                }

                                if (s < swizzle.Length - 1)
                                    val += ", ";
                            }

                            item.SubItems[2].Text = val;

                            continue;
                        }
                    }
                }

                item.SubItems[2].Text = "Error evaluating expression";
            }

            watchRegs.EndUpdate();
        }

        public void OnLogfileClosed()
        {
            Close();
        }

        public void OnLogfileLoaded()
        {
        }

        public void OnEventSelected(UInt32 frameID, UInt32 eventID)
        {
        }

        void m_DisassemblyView_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.F10)
            {
                if (e.Control)
                {
                    RunToCursor();
                }
                else if (e.Shift)
                {
                    StepBack();
                }
                else if (!e.Alt)
                {
                    StepNext();
                }
                e.Handled = true;
            }
            if (e.KeyCode == Keys.F5 && !e.Control && !e.Shift && !e.Alt)
            {
                Run();
                e.Handled = true;
            }
        }

        private void runBack_Click(object sender, EventArgs e)
        {
            RunBack();
        }

        private void run_Click(object sender, EventArgs e)
        {
            Run();
        }

        private void stepBack_Click(object sender, EventArgs e)
        {
            StepBack();
        }

        private void stepNext_Click(object sender, EventArgs e)
        {
            StepNext();
        }

        private void runToCursor_Click(object sender, EventArgs e)
        {
            RunToCursor();
        }

        private bool StepBack()
        {
            if (CurrentStep == 0)
                return false;

            CurrentStep--;

            return true;
        }

        private bool StepNext()
        {
            if (m_Trace == null || m_Trace.states == null) return false;

            if (CurrentStep + 1 >= m_Trace.states.Length)
                return false;

            CurrentStep++;

            return true;
        }

        private void RunTo(int runToInstruction, bool forward)
        {
            if (m_Trace == null || m_Trace.states == null)
                return;

            int step = CurrentStep;

            int inc = forward ? 1 : -1;

            bool firstStep = true;

            while (true)
            {
                if (m_Trace.states[step].nextInstruction == runToInstruction)
                    break;

                if (!firstStep && m_Breakpoints.Contains((int)m_Trace.states[step].nextInstruction))
                    break;

                firstStep = false;

                if (step + inc < 0 || step + inc >= m_Trace.states.Length)
                    break;

                step += inc;
            }

            CurrentStep = step;
        }

        private void RunBack()
        {
            RunTo(-1, false);
        }

        private void Run()
        {
            if (m_Trace != null)
            {
                RunTo(-1, true);
            }
            else
            {
                CurrentStep = 0;
            }
        }

        private void RunToCursor()
        {
            int i = m_DisassemblyView.Lines.Current.Number;

            while (i < m_DisassemblyView.Lines.Count)
            {
                var trimmed = m_DisassemblyView.Lines[i].Text.Trim();

                int colon = trimmed.IndexOf(":");

                if (colon > 0)
                {
                    string start = trimmed.Substring(0, colon);

                    int runTo = -1;

                    if (int.TryParse(start, out runTo))
                    {
                        if (runTo >= 0 && runTo < m_Trace.states.Length)
                        {
                            RunTo(runTo, true);
                            break;
                        }
                    }
                }

                i++;
            }
        }

        private void autosToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ShowConstants();
        }

        private void watchToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ShowWatch();
        }

        private DockContent ShowErrors()
        {
            if (m_ErrorsDock != null)
            {
                m_ErrorsDock.Show();
                return m_ErrorsDock;
            }

            m_ErrorsDock = Helpers.WrapDockContent(dockPanel, errorsBox);
            m_ErrorsDock.HideOnClose = true;

            m_ErrorsDock.Show(dockPanel, DockState.DockBottom);

            return m_ErrorsDock;
        }

        private DockContent ShowConstants()
        {
            if (m_ConstantsDock != null)
            {
                m_ConstantsDock.Show();
                return m_ConstantsDock;
            }

            m_ConstantsDock = Helpers.WrapDockContent(dockPanel, constantBox);
            m_ConstantsDock.HideOnClose = true;

            m_ConstantsDock.Show(dockPanel, DockState.DockBottom);

            return m_ConstantsDock;
        }

        private DockContent ShowVariables()
        {
            if (m_VariablesDock != null)
            {
                m_VariablesDock.Show();
                return m_VariablesDock;
            }

            m_VariablesDock = Helpers.WrapDockContent(dockPanel, variableBox);
            m_VariablesDock.HideOnClose = true;

            if (m_ConstantsDock != null)
            {
                m_VariablesDock.Show(m_ConstantsDock.Pane, DockAlignment.Right, 0.5);
            }
            else
            {
                m_VariablesDock.Show(dockPanel, DockState.DockBottom);
            }

            return m_VariablesDock;
        }

        private DockContent ShowWatch()
        {
            if (m_WatchDock != null)
            {
                m_WatchDock.Show();
                return m_WatchDock;
            }

            m_WatchDock = Helpers.WrapDockContent(dockPanel, watchBox);
            m_WatchDock.HideOnClose = true;

            if (m_VariablesDock != null)
            {
                m_WatchDock.Show(m_VariablesDock.Pane, m_VariablesDock);
            }
            else
            {
                m_WatchDock.Show(dockPanel, DockState.DockBottom);
            }

            return m_WatchDock;
        }

        private void watch1ToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ShowVariables();
        }

        private void saveButton_Click(object sender, EventArgs e)
        {
            if (m_SaveCallback != null)
            {
                var files = new Dictionary<string, string>();
                foreach (var s in m_Scintillas)
                    files.Add(s.Tag as string, s.Text);
                m_SaveCallback(this, files);
            }
        }

        private void textureDimensionsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CurrentScintilla == null)
                return;

            CurrentScintilla.InsertText(0, "uint4 RENDERDOC_TexDim; // xyz == width, height, depth. w == # mips" + Environment.NewLine + Environment.NewLine);
            CurrentScintilla.CurrentPos = 0;
        }

        private void selectedMipGlobalToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CurrentScintilla == null)
                return;

            CurrentScintilla.InsertText(0, "uint RENDERDOC_SelectedMip; // selected mip in UI" + Environment.NewLine + Environment.NewLine);
            CurrentScintilla.CurrentPos = 0;
        }

        private void textureTypeGlobalToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CurrentScintilla == null)
                return;

            CurrentScintilla.InsertText(0, "uint RENDERDOC_TextureType; // 1 = 1D, 2 = 2D, 3 = 3D, 4 = Depth, 5 = Depth + Stencil, 6 = Depth (MS), 7 = Depth + Stencil (MS)" + Environment.NewLine + Environment.NewLine);
            CurrentScintilla.CurrentPos = 0;
        }

        private void pointLinearSamplersToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CurrentScintilla == null)
                return;

            CurrentScintilla.InsertText(0, "// Samplers" + Environment.NewLine +
                                            "SamplerState pointSampler : register(s0);" + Environment.NewLine +
                                            "SamplerState linearSampler : register(s1);" + Environment.NewLine +
                                            "// End Samplers" + Environment.NewLine + Environment.NewLine);
            CurrentScintilla.CurrentPos = 0;
        }

        private void textureResourcesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (CurrentScintilla == null)
                return;

            CurrentScintilla.InsertText(0, "// Textures" + Environment.NewLine +
                                            "Texture1DArray<float4> texDisplayTex1DArray : register(t1);" + Environment.NewLine +
                                            "Texture2DArray<float4> texDisplayTex2DArray : register(t2);" + Environment.NewLine +
                                            "Texture3D<float4> texDisplayTex3D : register(t3);" + Environment.NewLine +
                                            "Texture2DArray<float2> texDisplayTexDepthArray : register(t4);" + Environment.NewLine +
                                            "Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);" + Environment.NewLine +
                                            "Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);" + Environment.NewLine +
                                            "Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);" + Environment.NewLine +
                                            "Texture2DArray<float4> texDisplayTexCubeArray : register(t8);" + Environment.NewLine +
                                            "" + Environment.NewLine +
                                            "Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);" + Environment.NewLine +
                                            "Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);" + Environment.NewLine +
                                            "Texture3D<uint4> texDisplayUIntTex3D : register(t13);" + Environment.NewLine +
                                            "" + Environment.NewLine +
                                            "Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);" + Environment.NewLine +
                                            "Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);" + Environment.NewLine +
                                            "Texture3D<int4> texDisplayIntTex3D : register(t23);" + Environment.NewLine +
                                            "// End Textures" + Environment.NewLine + Environment.NewLine);
            CurrentScintilla.CurrentPos = 0;
        }

        private void ShaderViewer_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (m_CloseCallback != null)
                m_CloseCallback();
        }

        private void displayInts_Click(object sender, EventArgs e)
        {
            displayInts.Checked = true;
            displayFloats.Checked = false;

            UpdateDebugging();
        }

        private void displayFloats_Click(object sender, EventArgs e)
        {
            displayInts.Checked = false;
            displayFloats.Checked = true;

            UpdateDebugging();
        }

        private void watchRegs_Layout(object sender, LayoutEventArgs e)
        {
            watchRegs.Columns[watchRegs.Columns.Count - 1].Width = -2;
        }

        private void watchRegs_DoubleClick(object sender, EventArgs e)
        {
            if (watchRegs.SelectedItems.Count == 1)
                watchRegs.SelectedItems[0].BeginEdit();
        }

        private void watchRegs_Click(object sender, EventArgs e)
        {
            if (watchRegs.SelectedItems.Count == 1 && watchRegs.SelectedItems[0].Index == watchRegs.Items.Count - 1)
                watchRegs.SelectedItems[0].BeginEdit();
        }

        private void watchRegs_AfterLabelEdit(object sender, LabelEditEventArgs e)
        {
            if (e.Label == "" && e.Item < watchRegs.Items.Count - 1)
                watchRegs.Items.RemoveAt(e.Item);
            else if (e.Label != null && e.Label != "" && e.Item == watchRegs.Items.Count - 1)
                watchRegs.Items.Add(new ListViewItem(new string[] { "", "", "" }));

            this.BeginInvoke((MethodInvoker)delegate { UpdateDebugging(); });
        }

        private void watchRegs_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Delete && watchRegs.SelectedItems.Count == 1)
                watchRegs.Items.RemoveAt(watchRegs.SelectedItems[0].Index);
        }

        private void ShaderViewer_FormClosed(object sender, FormClosedEventArgs e)
        {
            m_Core.RemoveLogViewer(this);
        }
    }
}