/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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
using System.Linq;
using System.Text;
using System.Threading;
using System.Reflection;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using renderdoc;

namespace renderdocui.Code
{
    public delegate void InvokeMethod(ReplayRenderer r);

    // this class owns the thread that interacts with the main library, to ensure that we don't
    // have to worry elsewhere about threading access. Elsewhere in the UI you can do Invoke or
    // BeginInvoke and get a ReplayRenderer reference back to access through
    public class RenderManager
    {
        private class InvokeHandle
        {
            public InvokeHandle(string t, InvokeMethod m)
            {
                tag = t;
                method = m;
                processed = false;
            }

            public InvokeHandle(InvokeMethod m)
            {
                tag = "";
                method = m;
                processed = false;
            }

            public string tag;
            public InvokeMethod method;
            public bool paintInvoke = false;
            volatile public bool processed;
            public Exception ex = null;
        };

        ////////////////////////////////////////////
        // variables

        private AutoResetEvent m_WakeupEvent = new AutoResetEvent(false);
        private Thread m_Thread;
        private string m_Logfile;
        private bool m_Running;
        private RemoteHost m_RemoteHost = null;
        private RemoteServer m_Remote = null;

        private List<InvokeHandle> m_renderQueue;
        private InvokeHandle m_current = null;

        ////////////////////////////////////////////
        // Interface

        public RenderManager()
        {
            Running = false;

            m_renderQueue = new List<InvokeHandle>();
        }

        public void OpenCapture(string logfile)
        {
            if(Running)
                return;

            m_Logfile = logfile;

            LoadProgress = 0.0f;

            InitException = null;

            m_Thread = Helpers.NewThread(new ThreadStart(this.RunThread));
            m_Thread.Priority = ThreadPriority.Highest;
            m_Thread.Start();

            while (m_Thread.IsAlive && !Running) ;
        }

        public UInt32 ExecuteAndInject(string app, string workingDir, string cmdLine, EnvironmentModification[] env, string logfile, CaptureOptions opts)
        {
            if (m_Remote == null)
            {
                return StaticExports.ExecuteAndInject(app, workingDir, cmdLine, env, logfile, opts);
            }
            else
            {
                UInt32 ret = 0;

                lock (m_Remote)
                {
                    ret = m_Remote.ExecuteAndInject(app, workingDir, cmdLine, env, opts);
                }

                return ret;
            }
        }

        public void DeleteCapture(string logfile, bool local)
        {
            if (Running)
            {
                BeginInvoke((ReplayRenderer r) => { DeleteCapture(logfile, local); });
                return;
            }

            if (local)
            {
                try
                {
                    System.IO.File.Delete(logfile);
                }
                catch (Exception)
                {
                }
            }
            else
            {
                // this will be cleaned up automatically when the remote connection
                // is closed.
                if (m_Remote != null)
                    m_Remote.TakeOwnershipCapture(logfile);
            }
        }

        public string[] GetRemoteSupport()
        {
            string[] ret = new string[0];

            if (m_Remote != null && !Running)
            {
                lock (m_Remote)
                {
                    ret = m_Remote.RemoteSupportedReplays();
                }
            }

            return ret;
        }

        public delegate void DirectoryBrowseMethod(string path, DirectoryFile[] contents);

        public void GetHomeFolder(DirectoryBrowseMethod cb)
        {
            if (m_Remote != null)
            {
                if (Running && m_Thread != Thread.CurrentThread)
                {
                    BeginInvoke((ReplayRenderer r) => { cb(m_Remote.GetHomeFolder(), null); });
                    return;
                }

                string home = "";

                // prevent pings while fetching remote FS data
                lock (m_Remote)
                {
                    home = m_Remote.GetHomeFolder();
                }

                cb(home, null);
            }
        }

        public bool ListFolder(string path, DirectoryBrowseMethod cb)
        {
            if (m_Remote != null)
            {
                if (Running && m_Thread != Thread.CurrentThread)
                {
                    BeginInvoke((ReplayRenderer r) => { cb(path, m_Remote.ListFolder(path)); });
                    return true;
                }

                DirectoryFile[] contents = new DirectoryFile[0];

                // prevent pings while fetching remote FS data
                lock(m_Remote)
                {
                    contents = m_Remote.ListFolder(path);
                }

                cb(path, contents);

                return true;
            }

            return false;
        }

        public string CopyCaptureToRemote(string localpath, Form window)
        {
            if (m_Remote != null)
            {
                bool copied = false;
                float progress = 0.0f;

                renderdocui.Windows.ProgressPopup modal =
                    new renderdocui.Windows.ProgressPopup(
                        (renderdocui.Windows.ModalCloseCallback)(() => { return copied; }),
                        true);
                modal.SetModalText("Transferring...");

                Thread progressThread = Helpers.NewThread(new ThreadStart(() =>
                {
                    modal.LogfileProgressBegin();

                    while (!copied)
                    {
                        Thread.Sleep(2);

                        modal.LogfileProgress(progress);
                    }
                }));
                progressThread.Start();

                string remotepath = "";

                // we should never have the thread running at this point, but let's be safe.
                if (Running)
                {
                    BeginInvoke((ReplayRenderer r) =>
                    {
                        remotepath = m_Remote.CopyCaptureToRemote(localpath, ref progress);

                        copied = true;
                    });
                }
                else
                {
                    Helpers.NewThread(new ThreadStart(() =>
                    {
                        // prevent pings while copying off-thread
                        lock (m_Remote)
                        {
                            remotepath = m_Remote.CopyCaptureToRemote(localpath, ref progress);
                        }

                        copied = true;
                    })).Start();
                }

                modal.ShowDialog(window);

                return remotepath;
            }

            // if we don't have a remote connection we can't copy
            throw new ApplicationException();
        }

        public void CopyCaptureFromRemote(string remotepath, string localpath, Form window)
        {
            if (m_Remote != null)
            {
                bool copied = false;
                float progress = 0.0f;

                renderdocui.Windows.ProgressPopup modal =
                    new renderdocui.Windows.ProgressPopup(
                        (renderdocui.Windows.ModalCloseCallback)(() => { return copied; }),
                        true);
                modal.SetModalText("Transferring...");

                Thread progressThread = Helpers.NewThread(new ThreadStart(() =>
                {
                    modal.LogfileProgressBegin();

                    while (!copied)
                    {
                        Thread.Sleep(2);

                        modal.LogfileProgress(progress);
                    }
                }));
                progressThread.Start();

                if (Running)
                {
                    BeginInvoke((ReplayRenderer r) =>
                    {
                        m_Remote.CopyCaptureFromRemote(remotepath, localpath, ref progress);

                        copied = true;
                    });
                }
                else
                {
                    Helpers.NewThread(new ThreadStart(() =>
                    {
                        // prevent pings while copying off-thread
                        lock (m_Remote)
                        {
                            m_Remote.CopyCaptureFromRemote(remotepath, localpath, ref progress);
                        }

                        copied = true;
                    })).Start();
                }

                modal.ShowDialog(window);

                // if the copy didn't succeed, throw
                if (!System.IO.File.Exists(localpath))
                    throw new System.IO.FileNotFoundException("File couldn't be transferred from remote host", remotepath);
            }
            else
            {
                System.IO.File.Copy(remotepath, localpath, true);
            }
        }

        public bool Running
        {
            get { return m_Running; }
            set { m_Running = value; m_WakeupEvent.Set(); }
        }

        public RemoteHost Remote
        {
            get { return m_RemoteHost; }
        }

        public void ConnectToRemoteServer(RemoteHost host)
        {
            InitException = null;

            try
            {
                m_Remote = StaticExports.CreateRemoteServer(host.Hostname, 0);
                m_RemoteHost = host;
                m_RemoteHost.Connected = true;
            }
            catch (ReplayCreateException ex)
            {
                InitException = ex;
            }
        }

        public void DisconnectFromRemoteServer()
        {
            if (m_RemoteHost != null)
                m_RemoteHost.Connected = false;

            if (m_Remote != null)
                m_Remote.ShutdownConnection();

            m_RemoteHost = null;
            m_Remote = null;
        }

        public void ShutdownServer()
        {
            if(m_Remote != null)
                m_Remote.ShutdownServerAndConnection();

            m_Remote = null;
        }

        public void PingRemote()
        {
            if (m_Remote == null)
                return;

            if (Monitor.TryEnter(m_Remote))
            {
                try
                {
                    // must only happen on render thread if running
                    if ((!Running || m_Thread == Thread.CurrentThread) && m_Remote != null)
                    {
                        if (!m_Remote.Ping())
                            m_RemoteHost.ServerRunning = false;
                    }
                }
                finally
                {
                    Monitor.Exit(m_Remote);
                }
            }
        }

        public ReplayCreateException InitException = null;

        public void CloseThreadSync()
        {
            Running = false;

            while (m_Thread != null && m_Thread.IsAlive) ;

            m_renderQueue = new List<InvokeHandle>();
            m_current = null;
        }

        public float LoadProgress;

        [ThreadStatic]
        private bool CatchExceptions = false;

        public void SetExceptionCatching(bool catching)
        {
            CatchExceptions = catching;
        }

        // this tagged version is for cases when we might send a request - e.g. to pick a vertex or pixel
        // - and want to pre-empt it with a new request before the first has returned. Either because some
        // other work is taking a while or because we're sending requests faster than they can be
        // processed.
        // the manager processes only the request on the top of the queue, so when a new tagged invoke
        // comes in, we remove any other requests in the queue before it that have the same tag
        public void BeginInvoke(string tag, InvokeMethod m)
        {
            InvokeHandle cmd = new InvokeHandle(tag, m);

            if (tag != "")
            {
                lock (m_renderQueue)
                {
                    bool added = false;

                    for (int i = 0; i < m_renderQueue.Count;)
                    {
                        if (m_renderQueue[i].tag == tag)
                        {
                            m_renderQueue[i].processed = true;
                            if (!added)
                            {
                                m_renderQueue[i] = cmd;
                                added = true;
                            }
                            else
                            {
                                m_renderQueue.RemoveAt(i);
                            }
                        }
                        else
                        {
                            i++;
                        }
                    }

                    if (!added)
                        m_renderQueue.Add(cmd);
                }

                m_WakeupEvent.Set();
            }
            else
            {
                PushInvoke(cmd);
            }
        }

        public void BeginInvoke(InvokeMethod m)
        {
            InvokeHandle cmd = new InvokeHandle(m);

            PushInvoke(cmd);
        }

        public void Invoke(InvokeMethod m)
        {
            InvokeHandle cmd = new InvokeHandle(m);

            PushInvoke(cmd);

            while (!cmd.processed) ;

            if (cmd.ex != null)
                throw cmd.ex;
        }

        public void InvokeForPaint(string tag, InvokeMethod m)
        {
            if (m_Thread == null || !Running)
                return;

            // special logic for painting invokes. Normally we want these to
            // go off immediately, but if we have a remote connection active
            // there could be slow operations on the pipe or currently being
            // processed.
            // So we check to see if the paint is likely to finish soon
            // (0, or only other paint invokes on the queue, nothing active)
            // and if so do it synchronously. Otherwise we just append to the
            // queue and return immediately.

            bool waitable = true;

            InvokeHandle cmd = new InvokeHandle(tag, m);
            cmd.paintInvoke = true;

            lock (m_renderQueue)
            {
                InvokeHandle current = m_current;

                if (current != null && !current.paintInvoke)
                    waitable = false;

                // any non-painting commands on the queue? can't wait
                for (int i = 0; waitable && i < m_renderQueue.Count; i++)
                    if (!m_renderQueue[i].paintInvoke)
                        waitable = false;

                // remove any duplicated paints if we have a tag
                bool added = false;

                if (tag != "")
                {
                    for (int i = 0; i < m_renderQueue.Count;)
                    {
                        if (m_renderQueue[i].tag == tag)
                        {
                            m_renderQueue[i].processed = true;
                            if (!added)
                            {
                                m_renderQueue[i] = cmd;
                                added = true;
                            }
                            else
                            {
                                m_renderQueue.RemoveAt(i);
                            }
                        }
                        else
                        {
                            i++;
                        }
                    }
                }

                if (!added)
                    m_renderQueue.Add(cmd);
            }

            m_WakeupEvent.Set();

            if (!waitable)
                return;

            while (!cmd.processed) ;

            if (cmd.ex != null)
                throw cmd.ex;
        }

        private void PushInvoke(InvokeHandle cmd)
        {
            if (m_Thread == null || !Running)
            {
                cmd.processed = true;
                return;
            }

            lock (m_renderQueue)
            {
                m_renderQueue.Add(cmd);
            }

            m_WakeupEvent.Set();
        }

        ////////////////////////////////////////////
        // Internals

        private ReplayRenderer CreateReplayRenderer()
        {
            if (m_Remote != null)
                return m_Remote.OpenCapture(-1, m_Logfile, ref LoadProgress);
            else
                return StaticExports.CreateReplayRenderer(m_Logfile, ref LoadProgress);
        }

        private void DestroyReplayRenderer(ReplayRenderer renderer)
        {
            if (m_Remote != null)
                m_Remote.CloseCapture(renderer);
            else
                renderer.Shutdown();
        }

        private void RunThread()
        {
            try
            {
                ReplayRenderer renderer = CreateReplayRenderer();
                if(renderer != null)
                {
                    System.Diagnostics.Debug.WriteLine("Renderer created");
                    
                    Running = true;

                    m_current = null;

                    while (Running)
                    {
                        lock (m_renderQueue)
                        {
                            if (m_renderQueue.Count > 0)
                            {
                                m_current = m_renderQueue[0];
                                m_renderQueue.RemoveAt(0);
                            }
                        }

                        if(m_current == null)
                        {
                            m_WakeupEvent.WaitOne(10);
                            continue;
                        }

                        if (m_current.method != null)
                        {
                            if (CatchExceptions)
                            {
                                try
                                {
                                    m_current.method(renderer);
                                }
                                catch (Exception ex)
                                {
                                    m_current.ex = ex;
                                }
                            }
                            else
                            {
                                m_current.method(renderer);
                            }
                        }

                        m_current.processed = true;
                        m_current = null;
                    }

                    lock (m_renderQueue)
                    {
                        foreach (var cmd in m_renderQueue)
                            cmd.processed = true;

                        m_renderQueue.Clear();
                    }

                    DestroyReplayRenderer(renderer);
                }
            }
            catch (ReplayCreateException ex)
            {
                InitException = ex;
            }
        }
    }
}
