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
            m_CameraTick = new System.Threading.Timer(TickCB, this as object, m_Rate, System.Threading.Timeout.Infinite);
        }

        private int m_Rate;
        private UpdateMethod m_Update;
        private System.Threading.Timer m_CameraTick = null;

        private static void TickCB(object state)
        {
            var me = (TimedUpdate)state;
            me.m_Update();
            me.m_CameraTick.Change(me.m_Rate, System.Threading.Timeout.Infinite);
        }
    }

    abstract class CameraControls
    {
        protected CameraControls(Camera c)
        {
            m_Camera = c;
        }

        abstract public void MouseWheel(object sender, MouseEventArgs e);

        abstract public void Reset(Vec3f pos);
        abstract public void Update();
        abstract public void Apply();

        abstract public bool Dirty { get; }
        abstract public Vec3f Position { get; }
        abstract public Vec3f Rotation { get; }

        virtual public void MouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                m_DragStartPos = e.Location;
            }
        }

        virtual public void MouseMove(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                if (m_DragStartPos.X < 0)
                {
                    m_DragStartPos = e.Location;
                }

                m_DragStartPos = e.Location;
            }
            else
            {
                m_DragStartPos = new Point(-1, -1);
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

        protected Camera m_Camera;
    }

    class ArcballCamera : CameraControls
    {
        public ArcballCamera(Camera c)
            : base(c)
        {
        }

        public override void Reset(Vec3f dist)
        {
            m_Distance = Math.Abs(dist.z);
            m_Rotation = new Vec3f();
        }

        public override void Update()
        {
        }

        public override void Apply()
        {
            m_Camera.Arcball(m_Distance, Rotation);
        }

        public override void MouseWheel(object sender, MouseEventArgs e)
        {
            float mod = (1.0f - (float)e.Delta / 2500.0f);

            m_Distance = Math.Max(1.0f, m_Distance * mod);

            ((HandledMouseEventArgs)e).Handled = true;

            m_Dirty = true;
        }

        public override void MouseMove(object sender, MouseEventArgs e)
        {
            if (DragStartPos.X > 0 && e.Button == MouseButtons.Left)
            {
                m_Rotation.y += (float)(e.X - DragStartPos.X) / 300.0f;
                m_Rotation.x += (float)(e.Y - DragStartPos.Y) / 300.0f;

                m_Dirty = true;
            }

            base.MouseMove(sender, e);
        }

        bool m_Dirty = false;
        public override bool Dirty
        {
            get
            {
                bool ret = m_Dirty;
                m_Dirty = false;
                return ret;
            }
        }

        private float m_Distance = 10.0f;
        private Vec3f m_Rotation = new Vec3f();
        public override Vec3f Position { get { return m_Camera.Position; } }
        public override Vec3f Rotation { get { return m_Rotation; } }
    }

    class FlyCamera : CameraControls
    {
        public FlyCamera(Camera c)
            : base(c)
        {
        }

        public override void Reset(Vec3f pos)
        {
            m_Position = pos;
            m_Rotation = new Vec3f();
        }

        public override void Update()
        {
            if (CurrentMove[0] != 0)
            {
                Vec3f dir = m_Camera.Right;
                dir.Mul((float)CurrentMove[0]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;

                m_Dirty = true;
            }
            if (CurrentMove[1] != 0)
            {
                Vec3f dir = new Vec3f(0.0f, 1.0f, 0.0f);
                //dir = m_Camera.GetUp();
                dir.Mul((float)CurrentMove[1]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;

                m_Dirty = true;
            }
            if (CurrentMove[2] != 0)
            {
                Vec3f dir = m_Camera.Forward;
                dir.Mul((float)CurrentMove[2]);

                m_Position.x += dir.x * CurrentSpeed;
                m_Position.y += dir.y * CurrentSpeed;
                m_Position.z += dir.z * CurrentSpeed;

                m_Dirty = true;
            }
        }

        public override void Apply()
        {
            m_Camera.fpsLook(m_Position, m_Rotation);
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

                m_Dirty = true;
            }

            base.MouseMove(sender, e);
        }

        bool m_Dirty = false;
        public override bool Dirty
        {
            get
            {
                bool ret = m_Dirty;
                m_Dirty = false;
                return ret;
            }
        }

        private Vec3f m_Position = new Vec3f(),
                      m_Rotation = new Vec3f();
        public override Vec3f Position { get { return m_Position; } }
        public override Vec3f Rotation { get { return m_Rotation; } }
    }
}
