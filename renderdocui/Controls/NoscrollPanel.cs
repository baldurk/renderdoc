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
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace renderdocui.Controls
{
    public class NoScrollPanel : Panel
    {
        public MouseEventHandler MouseWheelHandler = null;
        public KeyEventHandler KeyHandler = null;

        public NoScrollPanel() { }

        protected override void OnMouseWheel(MouseEventArgs e)
        {
            if (MouseWheelHandler != null)
                MouseWheelHandler(this, e);

            base.OnMouseWheel(e);
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (Focused && KeyHandler != null)
            {
                var args = new KeyEventArgs(keyData);
                KeyHandler(this, args);
                if(args.Handled)
                    return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        public bool Painting = false;

        protected override void OnPaintBackground(PaintEventArgs pevent)
        {
            if (Controls.Count == 0 && !Painting)
            {
                pevent.Graphics.Clear(BackColor);
            }
            else if (Controls.Count == 1)
            {
                var pos = Controls[0].Location;
                var size = Controls[0].Size;

                using (var brush = new SolidBrush(BackColor))
                {
                    var rightRect = new Rectangle(pos.X + size.Width, 0, ClientRectangle.Width - (pos.X + size.Width), ClientRectangle.Height);
                    var leftRect = new Rectangle(0, 0, pos.X, ClientRectangle.Height);
                    var topRect = new Rectangle(pos.X, 0, size.Width, pos.Y);
                    var bottomRect = new Rectangle(pos.X, pos.Y + size.Height, size.Width, ClientRectangle.Height - (pos.Y + size.Height));

                    pevent.Graphics.FillRectangle(brush, topRect);
                    pevent.Graphics.FillRectangle(brush, bottomRect);
                    pevent.Graphics.FillRectangle(brush, leftRect);
                    pevent.Graphics.FillRectangle(brush, rightRect);
                }
            }
        }

        protected override Point ScrollToControl(Control activeControl)
        {
            // Returning the current location prevents the panel from
            // scrolling to the active control when the panel loses and regains focus
            return this.DisplayRectangle.Location;
        }
    }
}
