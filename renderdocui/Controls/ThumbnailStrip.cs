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
    public partial class ThumbnailStrip : UserControl
    {
        public ThumbnailStrip()
        {
            InitializeComponent();

            MouseWheel += new MouseEventHandler(ThumbnailStrip_MouseWheel);
        }

        void ThumbnailStrip_MouseWheel(object sender, MouseEventArgs e)
        {
            const int WHEEL_DELTA = 120;

            int movement = e.Delta / WHEEL_DELTA;

            if (vscroll.Visible)
                vscroll.Value = Code.Helpers.Clamp(vscroll.Value - movement * vscroll.SmallChange, vscroll.Minimum, vscroll.Maximum - vscroll.LargeChange);
            if (hscroll.Visible)
                hscroll.Value = Code.Helpers.Clamp(hscroll.Value - movement * hscroll.SmallChange, hscroll.Minimum, hscroll.Maximum - hscroll.LargeChange);

            RefreshLayout();
        }

        private List<ResourcePreview> m_Thumbnails = new List<ResourcePreview>();
        public ResourcePreview[] Thumbnails { get { return m_Thumbnails.ToArray(); } }

        public void AddThumbnail(ResourcePreview r)
        {
            panel.Controls.Add(r);
            m_Thumbnails.Add(r);
        }

        public void ClearThumbnails()
        {
            m_Thumbnails.Clear();
            panel.Controls.Clear();
        }
        
        public void RefreshLayout()
        {
            Rectangle avail = ClientRectangle;
            avail.Inflate(new Size(-6, -6));

            int numVisible = 0;
            foreach (ResourcePreview c in Thumbnails)
                if (c.Visible) numVisible++;

            // depending on overall aspect ratio, we either lay out the strip horizontally or
            // vertically. This tries to account for whether the strip is docked along one side
            // or another of the texture viewer
            if (avail.Width > avail.Height)
            {
                avail.Width += 6; // controls implicitly have a 6 margin on the right

                int aspectWidth = (int)(avail.Height * 1.3f);

                vscroll.Visible = false;

                int noscrollWidth = numVisible * (aspectWidth + 6);

                if (noscrollWidth <= avail.Width)
                {
                    hscroll.Visible = false;

                    int x = avail.X;
                    foreach (ResourcePreview c in Thumbnails)
                    {
                        if (c.Visible)
                        {
                            c.Location = new Point(x, avail.Y);
                            c.SetSize(new Size(aspectWidth, avail.Height));

                            x += aspectWidth + 6;
                        }
                    }
                }
                else
                {
                    hscroll.Visible = true;

                    avail.Height = avail.Height - SystemInformation.HorizontalScrollBarHeight;

                    aspectWidth = (int)(avail.Height * 1.3f);

                    int totalWidth = numVisible * (aspectWidth + 6);
                    hscroll.Enabled = totalWidth > avail.Width;

                    if (hscroll.Enabled)
                    {
                        hscroll.Maximum = totalWidth - avail.Width;
                        hscroll.LargeChange = Code.Helpers.Clamp(avail.Height, 1, hscroll.Maximum/2);
                        hscroll.SmallChange = Math.Max(1, hscroll.LargeChange / 2);
                    }

                    int x = avail.X - (int)(hscroll.Maximum*(float)hscroll.Value/(float)(hscroll.Maximum-hscroll.LargeChange));
                    foreach (ResourcePreview c in Thumbnails)
                    {
                        if (c.Visible)
                        {
                            c.Location = new Point(x, avail.Y);
                            c.SetSize(new Size(aspectWidth, avail.Height));

                            x += aspectWidth + 6;
                        }
                    }
                }
            }
            else
            {
                avail.Height += 6; // controls implicitly have a 6 margin on the bottom

                int aspectHeight = (int)(avail.Width / 1.3f);

                hscroll.Visible = false;

                int noscrollHeight = numVisible * (aspectHeight + 6);

                if (noscrollHeight <= avail.Height)
                {
                    vscroll.Visible = false;

                    int y = avail.Y;
                    foreach (ResourcePreview c in Thumbnails)
                    {
                        if (c.Visible)
                        {
                            c.Location = new Point(avail.X, y);
                            c.SetSize(new Size(avail.Width, aspectHeight));

                            y += aspectHeight + 6;
                        }
                    }
                }
                else
                {
                    vscroll.Visible = true;

                    avail.Width = avail.Width - SystemInformation.VerticalScrollBarWidth;

                    aspectHeight = (int)(avail.Width / 1.3f);

                    int totalHeight = numVisible * (aspectHeight + 6);
                    vscroll.Enabled = totalHeight > avail.Height;

                    if (vscroll.Enabled)
                    {
                        vscroll.Maximum = totalHeight - avail.Height;
                        vscroll.LargeChange = Code.Helpers.Clamp(avail.Width, 1, vscroll.Maximum / 2);
                        vscroll.SmallChange = Math.Max(1, vscroll.LargeChange / 2);
                    }

                    int y = avail.Y - (int)(vscroll.Maximum * (float)vscroll.Value / (float)(vscroll.Maximum - vscroll.LargeChange));
                    foreach (ResourcePreview c in Thumbnails)
                    {
                        if (c.Visible)
                        {
                            c.Location = new Point(avail.X, y);
                            c.SetSize(new Size(avail.Width, aspectHeight));

                            y += aspectHeight + 6;
                        }
                    }
                }
            }
        }

        private void ThumbnailStrip_Layout(object sender, LayoutEventArgs e)
        {
            RefreshLayout();
        }

        private void panel_ControlAddRemove(object sender, ControlEventArgs e)
        {
            RefreshLayout();
        }

        private void panel_MouseClick(object sender, MouseEventArgs e)
        {
            OnMouseClick(e);
        }

        private void hscroll_Scroll(object sender, ScrollEventArgs e)
        {
            RefreshLayout();
        }

        private void vscroll_Scroll(object sender, ScrollEventArgs e)
        {
            RefreshLayout();
        }
    }
}
