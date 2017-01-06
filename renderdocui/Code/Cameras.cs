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
using System.Windows.Forms;
using System.Drawing;
using renderdoc;

namespace renderdocui.Code
{
    class TimedUpdate
    {
        public delegate void UpdateMethod();

        public TimedUpdate(int msCount, UpdateMethod up)
        {
            m_Rate = msCount;
            m_Update = up;
            Start();
        }

        private int m_Rate;
        private UpdateMethod m_Update = null;
        private System.Threading.Timer m_CameraTick = null;

        private static void TickCB(object state)
        {
            if (!(state is TimedUpdate)) return;

            var me = (TimedUpdate)state;

            if (me.m_Update != null) me.m_Update();
            if (me.m_CameraTick != null) me.m_CameraTick.Change(me.m_Rate, System.Threading.Timeout.Infinite);
        }

        public void Start()
        {
            m_CameraTick = new System.Threading.Timer(TickCB, this as object, m_Rate, System.Threading.Timeout.Infinite);
        }

        public void Stop()
        {
            m_CameraTick.Dispose();
            m_CameraTick = null;
        }
    }

    abstract class CameraControls
    {
        protected CameraControls()
        {
        }

        abstract public bool Update();

        abstract public Camera Camera { get; }

        abstract public void MouseWheel(object sender, MouseEventArgs e);

        virtual public void MouseClick(object sender, MouseEventArgs e)
        {
            m_DragStartPos = e.Location;
        }

        virtual public void MouseMove(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.None)
            {
                m_DragStartPos = new Point(-1, -1);
            }
            else
            {
                if (m_DragStartPos.X < 0)
                {
                    m_DragStartPos = e.Location;
                }

                m_DragStartPos = e.Location;
            }
        }

        virtual public void KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.A || e.KeyCode == Keys.D)
                m_CurrentMove[0] = 0;
            if (e.KeyCode == Keys.Q || e.KeyCode == Keys.E)
                m_CurrentMove[1] = 0;
            if (e.KeyCode == Keys.W || e.KeyCode == Keys.S)
                m_CurrentMove[2] = 0;

            if (e.Shift)
                m_CurrentSpeed = 3.0f;
            else
                m_CurrentSpeed = 1.0f;
        }

        virtual public void KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.W)
                m_CurrentMove[2] = 1;
            if (e.KeyCode == Keys.S)
                m_CurrentMove[2] = -1;
            if (e.KeyCode == Keys.Q)
                m_CurrentMove[1] = 1;
            if (e.KeyCode == Keys.E)
                m_CurrentMove[1] = -1;
            if (e.KeyCode == Keys.D)
                m_CurrentMove[0] = 1;
            if (e.KeyCode == Keys.A)
                m_CurrentMove[0] = -1;

            if (e.Shift)
                m_CurrentSpeed = 3.0f;
            else
                m_CurrentSpeed = 1.0f;
        }

        private float m_CurrentSpeed = 1.0f;
        private int[] m_CurrentMove = new int[3] { 0, 0, 0 };

        public float SpeedMultiplier = 0.05f;

        protected int[] CurrentMove { get { return m_CurrentMove; } }
        protected float CurrentSpeed { get { return m_CurrentSpeed * SpeedMultiplier; } }

        private Point m_DragStartPos = new Point(-1, -1);
        protected Point DragStartPos { get { return m_DragStartPos; } }
    }

    class ArcballCamera : CameraControls
    {
        private Camera m_Camera = null;

        public override Camera Camera { get { return m_Camera; } }

        public ArcballCamera()
        {
            m_Camera = Camera.InitArcball();
        }

        public void Reset(Vec3f pos, float dist)
        {
            m_LookAt = pos;
            m_Distance = Math.Abs(dist);

            m_Camera.ResetArcball();
            m_Camera.SetPosition(m_LookAt);
            m_Camera.SetArcballDistance(m_Distance);
        }

        public void SetDistance(float dist)
        {
            m_Distance = Math.Abs(dist);
            m_Camera.SetArcballDistance(m_Distance);
        }

        public override bool Update()
        {
            return false;
        }

        public override void MouseWheel(object sender, MouseEventArgs e)
        {
            float mod = (1.0f - (float)e.Delta / 2500.0f);

            m_Distance = Math.Max(1e-6f, m_Distance * mod);

            m_Camera.SetArcballDistance(m_Distance);

            ((HandledMouseEventArgs)e).Handled = true;
        }

        public override void MouseMove(object sender, MouseEventArgs e)
        {
            if (DragStartPos.X > 0)
            {
                if (e.Button == MouseButtons.Middle ||
                    (e.Button == MouseButtons.Left && (Control.ModifierKeys & Keys.Alt) == Keys.Alt)
                    )
                {
                    float xdelta = (float)(e.X - DragStartPos.X) / 300.0f;
                    float ydelta = (float)(e.Y - DragStartPos.Y) / 300.0f;

                    xdelta *= Math.Max(1.0f, m_Distance);
                    ydelta *= Math.Max(1.0f, m_Distance);

                    Vec3f pos, fwd, right, up;
                    m_Camera.GetBasis(out pos, out fwd, out right, out up);

                    m_LookAt.x -= right.x * xdelta;
                    m_LookAt.y -= right.y * xdelta;
                    m_LookAt.z -= right.z * xdelta;

                    m_LookAt.x += up.x * ydelta;
                    m_LookAt.y += up.y * ydelta;
                    m_LookAt.z += up.z * ydelta;

                    m_Camera.SetPosition(m_LookAt);
                }
                else if (e.Button == MouseButtons.Left)
                {
                    Control c = sender as Control;
                    if (c != null)
                        m_Camera.RotateArcball(DragStartPos, e.Location, c.ClientRectangle.Size);
                }
            }

            base.MouseMove(sender, e);
        }

        private float m_Distance = 10.0f;
        private Vec3f m_LookAt = new Vec3f();
        public Vec3f LookAtPos
        {
            get { return m_LookAt; }
            set { m_LookAt = value; m_Camera.SetPosition(m_LookAt); }
        }
    }

    class FlyCamera : CameraControls
    {
        private Camera m_Camera = null;

        public override Camera Camera { get { return m_Camera; } }

        public FlyCamera()
        {
            m_Camera = Camera.InitFPSLook();
        }

        public void Reset(Vec3f position)
        {
            m_Position = position;
            m_Rotation = new Vec3f();

            Camera.SetPosition(m_Position);
            Camera.SetFPSRotation(m_Rotation);
        }

        public override bool Update()
        {
            Vec3f pos, fwd, right, up;
            m_Camera.GetBasis(out pos, out fwd, out right, out up);

            if (CurrentMove[0] != 0)
            {
                Vec3f dir = right;
                dir.Mul((float)CurrentMove[0]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;
            }
            if (CurrentMove[1] != 0)
            {
                Vec3f dir = new Vec3f(0.0f, 1.0f, 0.0f);
                //dir = up;
                dir.Mul((float)CurrentMove[1]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;
            }
            if (CurrentMove[2] != 0)
            {
                Vec3f dir = fwd;
                dir.Mul((float)CurrentMove[2]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;
            }

            if (CurrentMove[0] != 0 || CurrentMove[1] != 0 || CurrentMove[2] != 0)
            {
                Camera.SetPosition(m_Position);
                return true;
            }

            return false;
        }

        public override void MouseWheel(object sender, MouseEventArgs e)
        {
        }

        public override void MouseMove(object sender, MouseEventArgs e)
        {
            if (DragStartPos.X > 0 && e.Button == MouseButtons.Left)
            {
                m_Rotation.y -= (float)(e.X - DragStartPos.X) / 300.0f;
                m_Rotation.x -= (float)(e.Y - DragStartPos.Y) / 300.0f;

                Camera.SetFPSRotation(m_Rotation);
            }

            base.MouseMove(sender, e);
        }

        private Vec3f m_Position = new Vec3f();
        private Vec3f m_Rotation = new Vec3f();
    }
}
