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
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace renderdocui.Controls
{
    public partial class DoubleClickSplitter : SplitContainer
    {
        public DoubleClickSplitter()
        {
            InitializeComponent();

            SplitterMoving += new SplitterCancelEventHandler(OnSplitterMoving);
        }

        private int m_PanelMinsize = 0;
        private int m_SplitterDistance = 0;

        private bool m_Panel1Collapse = true;

        [Description("If the first panel should be the one to collapse"), Category("Behavior")]
        [DefaultValue(typeof(bool), "true")]
        public bool Panel1Collapse { get { return m_Panel1Collapse; } set { m_Panel1Collapse = value; } }

        private bool m_Collapsed = false;
        [Browsable(false)]
        public bool Collapsed
        {
            get
            {
                return m_Collapsed;
            }
            set
            {
                if (m_Collapsed != value)
                {
                    if (value)
                    {
                        m_PanelMinsize = Panel1Collapse ? Panel1MinSize : Panel2MinSize;
                        m_SplitterDistance = SplitterDistance;

                        if (Panel1Collapse)
                        {
                            Panel1MinSize = 0;
                            SplitterDistance = 0;
                        }
                        else
                        {
                            Panel2MinSize = 0;
                            SplitterDistance = 10000;
                        }
                    }
                    else
                    {
                        if (Panel1Collapse)
                            Panel1MinSize = m_PanelMinsize;
                        else
                            Panel2MinSize = m_PanelMinsize;

                        SplitterDistance = m_SplitterDistance;
                    }
                }

                m_Collapsed = value;
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            try
            {
                if (m_Collapsed && Panel1Collapse)
                    SplitterDistance = Panel1MinSize;
                else if (m_Collapsed && !Panel1Collapse)
                {
                    if (Orientation == Orientation.Horizontal)
                        SplitterDistance = Height - Panel2MinSize;
                    else if(Orientation == Orientation.Vertical)
                        SplitterDistance = Width - Panel2MinSize;
                }
            }
            catch (System.Exception)
            {
                // non fatal
            }

            // arrow
            {
                var arrow_centre = new Point(SplitterRectangle.X + SplitterRectangle.Width / 2, SplitterRectangle.Y + SplitterRectangle.Height / 2);

                var oldmode = e.Graphics.SmoothingMode;
                e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

                Point[] dots = null;

                using(var brush = new SolidBrush(ForeColor))
                {
                    int dot_diameter = 0;

                    if (Orientation == Orientation.Horizontal)
                    {
                        dot_diameter = SplitterRectangle.Height - Math.Max(0, SplitterRectangle.Height / 4);

                        var arrow_size = new Size(SplitterRectangle.Height * 2, dot_diameter);
                        var arrow_pos = new Point(arrow_centre.X - arrow_size.Width / 2, arrow_centre.Y - arrow_size.Height / 2);

                        if ((Panel1Collapse && !Collapsed) || (!Panel1Collapse && Collapsed))
                        {
                            e.Graphics.FillPolygon(brush, new Point[] {
                                new Point(arrow_pos.X, arrow_pos.Y + arrow_size.Height),
                                new Point(arrow_pos.X +arrow_size.Width, arrow_pos.Y + arrow_size.Height),
                                new Point(arrow_pos.X + arrow_size.Width/2, arrow_pos.Y),
                            });
                        }
                        else
                        {
                            e.Graphics.FillPolygon(brush, new Point[] {
                                new Point(arrow_pos.X, arrow_pos.Y),
                                new Point(arrow_pos.X +arrow_size.Width, arrow_pos.Y),
                                new Point(arrow_pos.X + arrow_size.Width/2, arrow_pos.Y + arrow_size.Height),
                            });
                        }

                        dots = new Point[] {
                        new Point(arrow_centre.X - SplitterRectangle.Width/4 - (int)(dot_diameter*1.5), arrow_centre.Y),
                        new Point(arrow_centre.X - SplitterRectangle.Width/4, arrow_centre.Y),
                        new Point(arrow_centre.X - SplitterRectangle.Width/4 + (int)(dot_diameter*1.5), arrow_centre.Y),
                        
                        new Point(arrow_centre.X + SplitterRectangle.Width/4 - (int)(dot_diameter*1.5), arrow_centre.Y),
                        new Point(arrow_centre.X + SplitterRectangle.Width/4, arrow_centre.Y),
                        new Point(arrow_centre.X + SplitterRectangle.Width/4 + (int)(dot_diameter*1.5), arrow_centre.Y),
                    };
                    }
                    else
                    {
                        dot_diameter = SplitterRectangle.Width - Math.Max(0, SplitterRectangle.Width / 4);

                        var arrow_size = new Size(dot_diameter, SplitterRectangle.Width * 2);
                        var arrow_pos = new Point(arrow_centre.X - arrow_size.Width / 2, arrow_centre.Y - arrow_size.Height / 2);

                        if ((Panel1Collapse && !Collapsed) || (!Panel1Collapse && Collapsed))
                        {
                            e.Graphics.FillPolygon(brush, new Point[] {
                                new Point(arrow_pos.X + arrow_size.Width, arrow_pos.Y),
                                new Point(arrow_pos.X +arrow_size.Width, arrow_pos.Y + arrow_size.Height),
                                new Point(arrow_pos.X, arrow_pos.Y + arrow_size.Height/2),
                            });
                        }
                        else
                        {
                            e.Graphics.FillPolygon(brush, new Point[] {
                                new Point(arrow_pos.X, arrow_pos.Y),
                                new Point(arrow_pos.X, arrow_pos.Y +arrow_size.Height),
                                new Point(arrow_pos.X + arrow_size.Width, arrow_pos.Y + arrow_size.Height/2),
                            });
                        }

                        dots = new Point[] {
                        new Point(arrow_centre.X, arrow_centre.Y - SplitterRectangle.Height/4 - (int)(dot_diameter*1.5)),
                        new Point(arrow_centre.X, arrow_centre.Y - SplitterRectangle.Height/4),
                        new Point(arrow_centre.X, arrow_centre.Y - SplitterRectangle.Height/4 + (int)(dot_diameter*1.5)),
                        
                        new Point(arrow_centre.X, arrow_centre.Y + SplitterRectangle.Height/4 - (int)(dot_diameter*1.5)),
                        new Point(arrow_centre.X, arrow_centre.Y + SplitterRectangle.Height/4),
                        new Point(arrow_centre.X, arrow_centre.Y + SplitterRectangle.Height/4 + (int)(dot_diameter*1.5)),
                    };
                    }

                    if (dots != null)
                    {
                        foreach (var d in dots)
                        {
                            var rect = new Rectangle(new Point(d.X - dot_diameter / 2, d.Y - dot_diameter / 2), new Size(dot_diameter, dot_diameter));

                            e.Graphics.FillPie(brush, rect, 0, 360.0f);
                        }
                    }
                }

                e.Graphics.SmoothingMode = oldmode;
            }
        }

        protected override void OnDoubleClick(EventArgs e)
        {
            if (SplitterRectangle.Contains(PointToClient(Control.MousePosition)))
            {
                Collapsed = !Collapsed;

                return;
            }

            base.OnDoubleClick(e);
        }

        void OnSplitterMoving(object sender, SplitterCancelEventArgs e)
        {
            if (Collapsed)
                e.Cancel = true;
        }
    }
}
