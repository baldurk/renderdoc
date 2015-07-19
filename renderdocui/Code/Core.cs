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
using System.Diagnostics;
using System.Linq;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Threading;
using renderdocui.Windows;
using renderdocui.Windows.Dialogs;
using renderdocui.Windows.PipelineState;
using renderdoc;

namespace renderdocui.Code
{
    // Single core class. Between this and the RenderManager these classes govern the interaction
    // between the UI and the actual implementation.
    //
    // This class primarily controls things that need to be propogated globally, it keeps a list of
    // ILogViewerForms which are windows that would like to be notified of changes to the current event,
    // when a log is opened or closed, etc. It also contains data that potentially every window will
    // want access to - like a list of all buffers in the log and their properties, etc.
    public class Core
    {
        #region Privates

        private RenderManager m_Renderer = new RenderManager();

        private PersistantConfig m_Config = null;

        private bool m_LogLoaded = false;

        private FileSystemWatcher m_LogWatcher = null;

        private string m_LogFile = "";

        private UInt32 m_FrameID = 0;
        private UInt32 m_EventID = 0;
        private UInt32 m_DeferredEvent = 0;

        private APIProperties m_APIProperties = null;

        private FetchFrameInfo[] m_FrameInfo = null;
        private FetchDrawcall[][] m_DrawCalls = null;
        private FetchBuffer[] m_Buffers = null;
        private FetchTexture[] m_Textures = null;

        private D3D11PipelineState m_D3D11PipelineState = null;
        private GLPipelineState m_GLPipelineState = null;
        private CommonPipelineState m_PipelineState = new CommonPipelineState();

        private List<ILogViewerForm> m_LogViewers = new List<ILogViewerForm>();
        private List<ILogLoadProgressListener> m_ProgressListeners = new List<ILogLoadProgressListener>();

        private MainWindow m_MainWindow = null;
        private EventBrowser m_EventBrowser = null;
        private APIInspector m_APIInspector = null;
        private DebugMessages m_DebugMessages = null;
        private TimelineBar m_TimelineBar = null;
        private TextureViewer m_TextureViewer = null;
        private PipelineStateViewer m_PipelineStateViewer = null;

        #endregion

        #region Properties

        public static string ConfigDirectory
        {
            get
            {
                string appdata = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
                return Path.Combine(appdata, "renderdoc");
            }
        }

        public static string ConfigFilename
        {
            get
            {
                return Path.Combine(ConfigDirectory, "UI.config");
            }
        }

        public PersistantConfig Config { get { return m_Config; } }
        public bool LogLoaded { get { return m_LogLoaded; } }
        public bool LogLoading { get { return m_LogLoadingInProgress; } }
        public string LogFileName { get { return m_LogFile; } set { if (LogLoaded) m_LogFile = value; } }

        public FetchFrameInfo[] FrameInfo { get { while (m_FrameInfo == null); return m_FrameInfo; } }

        public APIProperties APIProps { get { return m_APIProperties; } }

        // typically 0 right now as we haven't supported multiple frames in logs for a loooong time.
        public UInt32 CurFrame { get { return m_FrameID; } }
        public UInt32 CurEvent { get { return m_DeferredEvent > 0 ? m_DeferredEvent : m_EventID; } }

        public FetchDrawcall[] CurDrawcalls { get { return GetDrawcalls(CurFrame); } }

        public FetchDrawcall CurDrawcall { get { return GetDrawcall(CurFrame, CurEvent); } }

        public FetchTexture[] CurTextures { get { return m_Textures; } }
        public FetchBuffer[] CurBuffers { get { return m_Buffers; } }

        public List<DebugMessage> DebugMessages = new List<DebugMessage>();
        public int UnreadMessageCount = 0;
        public void AddMessages(DebugMessage[] msgs)
        {
            UnreadMessageCount += msgs.Length;
            foreach(var msg in msgs)
                DebugMessages.Add(msg);
        }

        // the RenderManager can be used when you want to perform an operation, it will let you Invoke or
        // BeginInvoke onto the thread that's used to access the renderdoc project.
        public RenderManager Renderer { get { return m_Renderer; } }

        public Form AppWindow { get { return m_MainWindow; } }

        #endregion

        #region Pipeline State

        // direct access (note that only one of these will be valid for a log, check APIProps.pipelineType)
        public D3D11PipelineState CurD3D11PipelineState { get { return m_D3D11PipelineState; } }
        public GLPipelineState CurGLPipelineState { get { return m_GLPipelineState; } }
        public CommonPipelineState CurPipelineState { get { return m_PipelineState; } }

        #endregion

        #region Init and Shutdown

        public Core(string paramFilename, string remoteHost, uint remoteIdent, bool temp, PersistantConfig config)
        {
            if (!Directory.Exists(ConfigDirectory))
                Directory.CreateDirectory(ConfigDirectory);

            m_Config = config;
            m_MainWindow = new MainWindow(this, paramFilename, remoteHost, remoteIdent, temp);
        }

        public void Shutdown()
        {
            if (m_Renderer != null)
                m_Renderer.CloseThreadSync();
        }

        #endregion

        #region Log Loading & Capture

        private bool m_LogLoadingInProgress = false;
        
        private bool LogLoadCallback()
        {
            return !m_LogLoadingInProgress;
        }

        // used to determine if two drawcalls can be considered in the same 'pass',
        // ie. writing to similar targets, same type of call, etc.
        //
        // When a log has no markers, this is used to group up drawcalls into fake markers
        private bool PassEquivalent(FetchDrawcall a, FetchDrawcall b)
        {
            // executing command lists can have children
            if(a.children.Length > 0 || b.children.Length > 0)
                return false;

            // don't group draws and compute executes
            if ((a.flags & DrawcallFlags.Dispatch) != (b.flags & DrawcallFlags.Dispatch))
                return false;

            // don't group present with anything
            if ((a.flags & DrawcallFlags.Present) != (b.flags & DrawcallFlags.Present))
                return false;

            // don't group things run on different multithreaded contexts
            if(a.context != b.context)
                return false;

            // don't group things with different depth outputs
            if (a.depthOut != b.depthOut)
                return false;

            int numAOuts = 0, numBOuts = 0;
            for (int i = 0; i < 8; i++)
            {
                if (a.outputs[i] != ResourceId.Null) numAOuts++;
                if (b.outputs[i] != ResourceId.Null) numBOuts++;
            }

            int numSame = 0;

            if (a.depthOut != ResourceId.Null)
            {
                numAOuts++;
                numBOuts++;
                numSame++;
            }

            for (int i = 0; i < 8; i++)
            {
                if (a.outputs[i] != ResourceId.Null)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        if (a.outputs[i] == b.outputs[j])
                        {
                            numSame++;
                            break;
                        }
                    }
                }
                else if (b.outputs[i] != ResourceId.Null)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        if (a.outputs[j] == b.outputs[i])
                        {
                            numSame++;
                            break;
                        }
                    }
                }
            }

            // use a kind of heuristic to group together passes where the outputs are similar enough.
            // could be useful for example if you're rendering to a gbuffer and sometimes you render
            // without one target, but the draws are still batched up.
            if (numSame > Math.Max(numAOuts, numBOuts) / 2 && Math.Max(numAOuts, numBOuts) > 1)
                return true;

            if (numSame == Math.Max(numAOuts, numBOuts))
                return true;

            return false;
        }

        private bool ContainsMarker(FetchDrawcall[] draws)
        {
            bool ret = false;

            foreach (var d in draws)
            {
                ret |= (d.flags & (DrawcallFlags.PushMarker | DrawcallFlags.SetMarker)) > 0 && (d.flags & DrawcallFlags.CmdList) == 0;
                ret |= ContainsMarker(d.children);
            }

            return ret;
        }

        // if a log doesn't contain any markers specified at all by the user, then we can
        // fake some up by determining batches of draws that are similar and giving them a
        // pass number
        private FetchDrawcall[] FakeProfileMarkers(int frameID, FetchDrawcall[] draws)
        {
            if (ContainsMarker(draws))
                return draws;

            var ret = new List<FetchDrawcall>();

            int depthpassID = 1;
            int computepassID = 1;
            int passID = 1;

            int start = 0;

            int counter = 1;

            for (int i = 1; i < draws.Length; i++)
            {
                if (PassEquivalent(draws[i], draws[start]))
                    continue;

                int end = i-1;

                if (end - start < 2 ||
                    draws[i].children.Length > 0 || draws[start].children.Length > 0 ||
                    draws[i].context != m_FrameInfo[frameID].immContextId ||
                    draws[start].context != m_FrameInfo[frameID].immContextId)
                {
                    for (int j = start; j <= end; j++)
                    {
                        ret.Add(draws[j]);
                        counter++;
                    }

                    start = i;
                    continue;
                }

                int minOutCount = 100;
                int maxOutCount = 0;

                for (int j = start; j <= end; j++)
                {
                    int outCount = 0;
                    foreach (var o in draws[j].outputs)
                        if (o != ResourceId.Null)
                            outCount++;
                    minOutCount = Math.Min(minOutCount, outCount);
                    maxOutCount = Math.Max(maxOutCount, outCount);
                }

                FetchDrawcall mark = new FetchDrawcall();
                
                mark.eventID = draws[end].eventID;
                mark.drawcallID = draws[end].drawcallID;

                mark.context = draws[end].context;
                mark.flags = DrawcallFlags.PushMarker;
                mark.outputs = draws[end].outputs;
                mark.depthOut = draws[end].depthOut;

                mark.name = "Guessed Pass";


                if((draws[end].flags & DrawcallFlags.Dispatch) != 0)
                    mark.name = String.Format("Compute Pass #{0}", computepassID++);
                else if (maxOutCount == 0)
                    mark.name = String.Format("Depth-only Pass #{0}", depthpassID++);
                else if(minOutCount == maxOutCount)
                    mark.name = String.Format("Colour Pass #{0} ({1} Targets{2})", passID++, minOutCount, draws[end].depthOut == ResourceId.Null ? "" : " + Depth");
                else
                    mark.name = String.Format("Colour Pass #{0} ({1}-{2} Targets{3})", passID++, minOutCount, maxOutCount, draws[end].depthOut == ResourceId.Null ? "" : " + Depth");

                mark.children = new FetchDrawcall[end - start + 1];

                for (int j = start; j <= end; j++)
                {
                    mark.children[j - start] = draws[j];
                    draws[j].parent = mark;
                }

                ret.Add(mark);

                start = i;
                counter++;
            }

            if (start < draws.Length)
                ret.Add(draws[start]);

            return ret.ToArray();
        }

        // loading a local log, no remote replay
        public void LoadLogfile(string logFile, bool temporary)
        {
            LoadLogfile(-1, "", logFile, temporary);
        }

        // when loading a log while replaying remotely, provide the proxy renderer that will be used
        // as well as the hostname to replay on.
        public void LoadLogfile(int proxyRenderer, string replayHost, string logFile, bool temporary)
        {
            m_LogFile = logFile;

            m_LogLoadingInProgress = true;

            if (File.Exists(Core.ConfigFilename))
                m_Config.Serialize(Core.ConfigFilename);

            float postloadProgress = 0.0f;

            bool progressThread = true;

            // start a modal dialog to prevent the user interacting with the form while the log is loading.
            // We'll close it down when log loading finishes (whether it succeeds or fails)
            ModalPopup modal = new ModalPopup(LogLoadCallback, true);

            Thread modalThread = Helpers.NewThread(new ThreadStart(() =>
            {
                modal.SetModalText(string.Format("Loading Log {0}.", m_LogFile));

                AppWindow.BeginInvoke(new Action(() =>
                {
                    modal.ShowDialog(AppWindow);
                }));
            }));
            modalThread.Start();

            // this thread continually ticks and notifies any threads of the progress, through a float
            // that is updated by the main loading code
            Thread thread = Helpers.NewThread(new ThreadStart(() =>
            {
                modal.LogfileProgressBegin();

                foreach (var p in m_ProgressListeners)
                    p.LogfileProgressBegin();

                while (progressThread)
                {
                    Thread.Sleep(2);

                    float progress = 0.8f * m_Renderer.LoadProgress + 0.19f * postloadProgress + 0.01f;

                    modal.LogfileProgress(progress);

                    foreach (var p in m_ProgressListeners)
                        p.LogfileProgress(progress);
                }
            }));
            thread.Start();

            // this function call will block until the log is either loaded, or there's some failure
            m_Renderer.Init(proxyRenderer, replayHost, logFile);

            // if the renderer isn't running, we hit a failure case so display an error message
            if (!m_Renderer.Running)
            {
                string errmsg = "Unknown error message";
                if (m_Renderer.InitException.Data.Contains("status"))
                    errmsg = ((ReplayCreateStatus)m_Renderer.InitException.Data["status"]).Str();

                if(proxyRenderer >= 0)
                    MessageBox.Show(String.Format("{0}\nFailed to transfer and replay on remote host {1}: {2}.\n\n" +
                                                    "Check diagnostic log in Help menu for more details.", logFile, replayHost, errmsg),
                                    "Error opening log", MessageBoxButtons.OK, MessageBoxIcon.Error);
                else
                    MessageBox.Show(String.Format("{0}\nFailed to open logfile for replay: {1}.\n\n" +
                                                    "Check diagnostic log in Help menu for more details.", logFile, errmsg),
                                    "Error opening log", MessageBoxButtons.OK, MessageBoxIcon.Error);

                progressThread = false;
                thread.Join();

                m_LogLoadingInProgress = false;

                modal.LogfileProgress(-1.0f);

                foreach (var p in m_ProgressListeners)
                    p.LogfileProgress(-1.0f);

                return;
            }

            if (!temporary)
            {
                m_Config.AddRecentFile(m_Config.RecentLogFiles, logFile, 10);

                if (File.Exists(Core.ConfigFilename))
                    m_Config.Serialize(Core.ConfigFilename);
            }

            m_FrameID = 0;
            m_EventID = 0;

            m_FrameInfo = null;
            m_APIProperties = null;

            // fetch initial data like drawcalls, textures and buffers
            m_Renderer.Invoke((ReplayRenderer r) =>
            {
                m_FrameInfo = r.GetFrameInfo();

                m_APIProperties = r.GetAPIProperties();

                postloadProgress = 0.2f;

                m_DrawCalls = new FetchDrawcall[m_FrameInfo.Length][];

                postloadProgress = 0.4f;

                for (int i = 0; i < m_FrameInfo.Length; i++)
                    m_DrawCalls[i] = FakeProfileMarkers(i, r.GetDrawcalls((UInt32)i));

                postloadProgress = 0.7f;

                m_Buffers = r.GetBuffers();

                postloadProgress = 0.8f;
                var texs = new List<FetchTexture>(r.GetTextures());
                m_Textures = texs.OrderBy(o => o.name).ToArray();

                postloadProgress = 0.9f;

                m_D3D11PipelineState = r.GetD3D11PipelineState();
                m_GLPipelineState = r.GetGLPipelineState();
                m_PipelineState.SetStates(m_APIProperties, m_D3D11PipelineState, m_GLPipelineState);

                UnreadMessageCount = 0;
                AddMessages(m_FrameInfo[0].debugMessages);

                postloadProgress = 1.0f;
            });

            Thread.Sleep(20);

            DateTime today = DateTime.Now;
            DateTime compare = today.AddDays(-21);

            if (compare.CompareTo(Config.DegradedLog_LastUpdate) >= 0 && m_APIProperties.degraded)
            {
                Config.DegradedLog_LastUpdate = today;

                MessageBox.Show(String.Format("{0}\nThis log opened with degraded support - " +
                                                "this could mean missing hardware support caused a fallback to software rendering.\n\n" +
                                                "This warning will not appear every time this happens, " +
                                                "check debug errors/warnings window for more details.", logFile),
                                "Degraded support of log", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
            }

            m_LogLoaded = true;
            progressThread = false;

            m_LogWatcher = new FileSystemWatcher(Path.GetDirectoryName(m_LogFile), Path.GetFileName(m_LogFile));
            m_LogWatcher.EnableRaisingEvents = true;
            m_LogWatcher.NotifyFilter = NotifyFilters.Size | NotifyFilters.FileName | NotifyFilters.LastAccess | NotifyFilters.LastWrite;
            m_LogWatcher.Created += new FileSystemEventHandler(OnLogfileChanged);
            m_LogWatcher.Changed += new FileSystemEventHandler(OnLogfileChanged);
            m_LogWatcher.SynchronizingObject = m_MainWindow; // callbacks on UI thread please

            List<ILogViewerForm> logviewers = new List<ILogViewerForm>();
            logviewers.AddRange(m_LogViewers);

            // notify all the registers log viewers that a log has been loaded
            foreach (var logviewer in logviewers)
            {
                if (logviewer == null || !(logviewer is Control)) continue;

                Control c = (Control)logviewer;
                if (c.InvokeRequired)
                {
                    if (!c.IsDisposed)
                    {
                        c.Invoke(new Action(() => {
                            try
                            {
                                logviewer.OnLogfileLoaded();
                            }
                            catch (Exception ex)
                            {
                                throw new AccessViolationException("Rethrown from Invoke:\n" + ex.ToString());
                            }
                        }));
                    }
                }
                else if (!c.IsDisposed)
                    logviewer.OnLogfileLoaded();
            }

            m_LogLoadingInProgress = false;

            modal.LogfileProgress(1.0f);

            foreach (var p in m_ProgressListeners)
                p.LogfileProgress(1.0f);
        }

        void OnLogfileChanged(object sender, FileSystemEventArgs e)
        {
            m_Renderer.Invoke((ReplayRenderer r) =>
            {
                r.FileChanged();
                r.SetFrameEvent(m_FrameID, m_EventID > 0 ? m_EventID-1 : 1);
            });

            SetEventID(null, CurFrame, CurEvent);
        }

        public void CloseLogfile()
        {
            if (!m_LogLoaded) return;

            m_LogFile = "";

            m_Renderer.CloseThreadSync();
            m_Renderer = new RenderManager();

            m_APIProperties = null;
            m_FrameInfo = null;
            m_DrawCalls = null;
            m_Buffers = null;
            m_Textures = null;

            m_D3D11PipelineState = null;
            m_GLPipelineState = null;
            m_PipelineState.SetStates(null, null, null);

            DebugMessages.Clear();
            UnreadMessageCount = 0;

            m_LogLoaded = false;

            if (m_LogWatcher != null)
                m_LogWatcher.EnableRaisingEvents = false;
            m_LogWatcher = null;

            foreach (var logviewer in m_LogViewers)
            {
                Control c = (Control)logviewer;
                if (c.InvokeRequired)
                    c.Invoke(new Action(() => logviewer.OnLogfileClosed()));
                else
                    logviewer.OnLogfileClosed();
            }
        }

        public String TempLogFilename(String appname)
        {
            string folder = Config.CaptureSavePath;
            try
            {
                if (folder.Length == 0 || !Directory.Exists(folder))
                    folder = Path.GetTempPath();
            }
            catch (ArgumentException)
            {
                // invalid path or similar
                folder = Path.GetTempPath();
            }
            return Path.Combine(folder, appname + "_" + DateTime.Now.ToString(@"yyyy.MM.dd_HH.mm.ss") + ".rdc");
        }

        #endregion

        #region Log drawcalls

        public FetchDrawcall[] GetDrawcalls(UInt32 frameIdx)
        {
            if (m_DrawCalls == null) return null;
            return m_DrawCalls[frameIdx];
        }

        private FetchDrawcall GetDrawcall(FetchDrawcall[] draws, UInt32 eventID)
        {
            foreach (var d in draws)
            {
                if (d.children != null && d.children.Length > 0)
                {
                    var draw = GetDrawcall(d.children, eventID);
                    if (draw != null) return draw;
                }

                if (d.eventID == eventID)
                    return d;
            }

            return null;
        }

        public FetchDrawcall GetDrawcall(UInt32 frameID, UInt32 eventID)
        {
            if (frameID < 0 || m_DrawCalls == null || frameID >= m_DrawCalls.Length)
                return null;

            return GetDrawcall(m_DrawCalls[frameID], eventID);
        }

        #endregion

        #region Viewers
        
        // Some viewers we only allow one to exist at once, so we keep the instance here.

        public EventBrowser GetEventBrowser()
        {
            if (m_EventBrowser == null || m_EventBrowser.IsDisposed)
            {
                m_EventBrowser = new EventBrowser(this);
                AddLogViewer(m_EventBrowser);
            }

            return m_EventBrowser;
        }

        public TextureViewer GetTextureViewer()
        {
            if (m_TextureViewer == null || m_TextureViewer.IsDisposed)
            {
                m_TextureViewer = new TextureViewer(this);
                AddLogViewer(m_TextureViewer);
            }

            return m_TextureViewer;
        }

        public PipelineStateViewer GetPipelineStateViewer()
        {
            if (m_PipelineStateViewer == null || m_PipelineStateViewer.IsDisposed)
            {
                m_PipelineStateViewer = new PipelineStateViewer(this);
                AddLogViewer(m_PipelineStateViewer);
            }

            return m_PipelineStateViewer;
        }

        public APIInspector GetAPIInspector()
        {
            if (m_APIInspector == null || m_APIInspector.IsDisposed)
            {
                m_APIInspector = new APIInspector(this);
                AddLogViewer(m_APIInspector);
            }

            return m_APIInspector;
        }

        public DebugMessages GetDebugMessages()
        {
            if (m_DebugMessages == null || m_DebugMessages.IsDisposed)
            {
                m_DebugMessages = new DebugMessages(this);
                AddLogViewer(m_DebugMessages);
            }

            return m_DebugMessages;
        }

        public TimelineBar TimelineBar
        {
            get
            {
                if (m_TimelineBar == null || m_TimelineBar.IsDisposed)
                    return null;

                return m_TimelineBar;
            }
        }

        private CaptureDialog m_CaptureDialog = null;
        public CaptureDialog CaptureDialog
        {
            get
            {
                return m_CaptureDialog == null || m_CaptureDialog.IsDisposed ? null : m_CaptureDialog;
            }
            set
            {
                if (m_CaptureDialog == null || m_CaptureDialog.IsDisposed)
                    m_CaptureDialog = value;
            }
        }

        public TimelineBar GetTimelineBar()
        {
            if (m_TimelineBar == null || m_TimelineBar.IsDisposed)
            {
                m_TimelineBar = new TimelineBar(this);
                AddLogViewer(m_TimelineBar);
            }

            return m_TimelineBar;
        }

        public void AddLogProgressListener(ILogLoadProgressListener p)
        {
            m_ProgressListeners.Add(p);
        }

        public void AddLogViewer(ILogViewerForm f)
        {
            m_LogViewers.Add(f);

            if (LogLoaded)
            {
                f.OnLogfileLoaded();
                f.OnEventSelected(CurFrame, CurEvent);
            }
        }

        public void RemoveLogViewer(ILogViewerForm f)
        {
            m_LogViewers.Remove(f);
        }

        #endregion

        #region Log Browsing

        // setting a context filter allows replaying of deferred events. You can set the deferred
        // events to replay in a context, after replaying up to a given event on the main thread
        public void SetContextFilter(ILogViewerForm exclude, UInt32 frameID, UInt32 eventID,
                                     ResourceId ctx, UInt32 firstDeferred, UInt32 lastDeferred)
        {
            m_FrameID = frameID;
            m_EventID = eventID;

            m_DeferredEvent = lastDeferred;

            m_Renderer.Invoke((ReplayRenderer r) => { r.SetContextFilter(ctx, firstDeferred, lastDeferred); });
            m_Renderer.Invoke((ReplayRenderer r) => {
                r.SetFrameEvent(m_FrameID, m_EventID);
                m_D3D11PipelineState = r.GetD3D11PipelineState();
                m_GLPipelineState = r.GetGLPipelineState();
                m_PipelineState.SetStates(m_APIProperties, m_D3D11PipelineState, m_GLPipelineState);
            });

            foreach (var logviewer in m_LogViewers)
            {
                if (logviewer == exclude)
                    continue;

                Control c = (Control)logviewer;
                if (c.InvokeRequired)
                    c.BeginInvoke(new Action(() => logviewer.OnEventSelected(frameID, eventID)));
                else
                    logviewer.OnEventSelected(frameID, eventID);
            }
        }

        public void SetEventID(ILogViewerForm exclude, UInt32 frameID, UInt32 eventID)
        {
            m_FrameID = frameID;
            m_EventID = eventID;

            m_DeferredEvent = 0;

            m_Renderer.Invoke((ReplayRenderer r) => { r.SetContextFilter(ResourceId.Null, 0, 0); });
            m_Renderer.Invoke((ReplayRenderer r) =>
            {
                r.SetFrameEvent(m_FrameID, m_EventID);
                m_D3D11PipelineState = r.GetD3D11PipelineState();
                m_GLPipelineState = r.GetGLPipelineState();
                m_PipelineState.SetStates(m_APIProperties, m_D3D11PipelineState, m_GLPipelineState);
            });

            foreach (var logviewer in m_LogViewers)
            {
                if(logviewer == exclude)
                    continue;

                Control c = (Control)logviewer;
                if (c.InvokeRequired)
                    c.BeginInvoke(new Action(() => logviewer.OnEventSelected(frameID, eventID)));
                else
                    logviewer.OnEventSelected(frameID, eventID);
            }
        }

        #endregion
    }
}
