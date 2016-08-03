/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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
            public InvokeHandle(InvokeMethod m)
            {
                method = m;
                processed = false;
            }

            public InvokeMethod method;
            volatile public bool processed;
            public Exception ex = null;
        };

        ////////////////////////////////////////////
        // variables

        private AutoResetEvent m_WakeupEvent = new AutoResetEvent(false);
        private Thread m_Thread;
        private string m_Logfile;
        private bool m_Running;
        private RemoteServer m_Remote;

        private List<InvokeHandle> m_renderQueue;

        ////////////////////////////////////////////
        // Interface

        public RenderManager()
        {
            Running = false;

            m_renderQueue = new List<InvokeHandle>();
        }

        public void Init(string logfile)
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

        public bool Running
        {
            get { return m_Running; }
            set { m_Running = value; m_WakeupEvent.Set(); }
        }

        public void ConnectToRemoteServer(string hostname)
        {
            InitException = null;

            try
            {
                m_Remote = StaticExports.CreateRemoteServer(hostname, 0);
            }
            catch (ApplicationException ex)
            {
                InitException = ex;
            }
        }

        public void DisconnectFromRemoteServer()
        {
            if (m_Remote != null)
                m_Remote.ShutdownConnection();
        }

        public ApplicationException InitException = null;

        public void CloseThreadSync()
        {
            Running = false;

            while (m_Thread != null && m_Thread.IsAlive) ; 
        }

        public float LoadProgress;

        [ThreadStatic]
        private bool CatchExceptions = false;

        public void SetExceptionCatching(bool catching)
        {
            CatchExceptions = catching;
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

        private void PushInvoke(InvokeHandle cmd)
        {
            if (m_Thread == null || !Running)
            {
                cmd.processed = true;
                return;
            }

            m_WakeupEvent.Set();

            lock (m_renderQueue)
            {
                m_renderQueue.Add(cmd);
            }
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

                    while (Running)
                    {
                        List<InvokeHandle> queue = new List<InvokeHandle>();
                        lock (m_renderQueue)
                        {
                            foreach (var cmd in m_renderQueue)
                                queue.Add(cmd);

                            m_renderQueue.Clear();
                        }

                        foreach (var cmd in queue)
                        {
                            if (cmd.method != null)
                            {
                                if (CatchExceptions)
                                {
                                    try
                                    {
                                        cmd.method(renderer);
                                    }
                                    catch (Exception ex)
                                    {
                                        cmd.ex = ex;
                                    }
                                }
                                else
                                {
                                    cmd.method(renderer);
                                }
                            }

                            cmd.processed = true;
                        }

                        m_WakeupEvent.WaitOne(10);
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
            catch (ApplicationException ex)
            {
                InitException = ex;
            }
        }
    }
}
