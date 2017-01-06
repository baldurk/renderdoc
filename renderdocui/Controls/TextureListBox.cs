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
using renderdoc;
using renderdocui.Code;

namespace renderdocui.Controls
{
    public partial class TextureListBox : ListBox
    {
        private List<FetchTexture> m_FilteredTextures = new List<FetchTexture>();

        private static readonly object GoIconClickEvent = new object();
        public event EventHandler<GoIconClickEventArgs> GoIconClick
        {
            add { Events.AddHandler(GoIconClickEvent, value); }
            remove { Events.RemoveHandler(GoIconClickEvent, value); }
        }
        protected virtual void OnGoIconClick(GoIconClickEventArgs e)
        {
            EventHandler<GoIconClickEventArgs> handler = (EventHandler<GoIconClickEventArgs>)Events[GoIconClickEvent];
            if (handler != null)
                handler(this, e);
        }

        public Core m_Core = null;

        public TextureListBox()
        {
            DrawMode = DrawMode.OwnerDrawFixed;
            DrawItem += new DrawItemEventHandler(TextureListBox_DrawItem);

            this.DoubleBuffered = true;
            SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint, true);

            Items.Clear();
            Items.Add("foobar");
        }

        void TextureListBox_DrawItem(object sender, DrawItemEventArgs e)
        {
            if (Items.Count > 0 && e.Index >= 0)
            {
                Rectangle stringBounds = e.Bounds;

                var image = global::renderdocui.Properties.Resources.action;

                if (m_HoverHighlight == e.Index)
                {
                    image = global::renderdocui.Properties.Resources.action_hover;
                    e.Graphics.DrawRectangle(Pens.LightGray, e.Bounds);
                }

                e.Graphics.DrawImage(image, e.Bounds.Width - 16, e.Bounds.Y, 16, 16);

                stringBounds.Width -= 18;

                var sf = new StringFormat(StringFormat.GenericDefault);

                sf.Trimming = StringTrimming.EllipsisCharacter;
                sf.FormatFlags |= StringFormatFlags.NoWrap;

                using (Brush b = new SolidBrush(ForeColor))
                {
                    e.Graphics.DrawString(Items[e.Index].ToString(),
                                            Font, b, stringBounds, sf);
                }
            }
        }

        private int m_HoverHighlight = -1;

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
        }

        protected override void OnMouseClick(MouseEventArgs e)
        {
            base.OnMouseClick(e);

            if (Items.Count > 0 && m_HoverHighlight >= 0)
            {
                var rect = GetItemRectangle(m_HoverHighlight);

                if (rect.Contains(e.Location))
                {
                    OnGoIconClick(new GoIconClickEventArgs(m_FilteredTextures[m_HoverHighlight].ID));
                }
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);

            bool curhover = m_HoverHighlight != -1;
            bool hover = false;

            for(int i=0; i < Items.Count; i++)
            {
                var rect = GetItemRectangle(i);

                bool highlight = rect.Contains(e.Location);

                hover |= highlight;

                if (m_HoverHighlight != i && highlight)
                {
                    m_HoverHighlight = i;
                    Invalidate();
                }
            }

            if (hover)
            {
                Cursor = Cursors.Hand;
            }
            else
            {
                Cursor = Cursors.Arrow;
                m_HoverHighlight = -1;
            }

            if (hover != curhover)
                Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);

            Cursor = Cursors.Arrow;
            if (Items.Count > 0 && m_HoverHighlight >= 0)
                Invalidate(GetItemRectangle(m_HoverHighlight));

            m_HoverHighlight = -1;
        }

        protected override void OnVisibleChanged(EventArgs e)
        {
            base.OnVisibleChanged(e);

            if (Visible)
            {
                FillTextureList("", true, true);
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            for (int i = 0; i < Items.Count; i++)
            {
                TextureListBox_DrawItem(this, new DrawItemEventArgs(e.Graphics, Font, GetItemRectangle(i), i, DrawItemState.Default));
            }
        }

        public void FillTextureList(string filter, bool RTs, bool Texs)
        {
            m_FilteredTextures.Clear();
            Items.Clear();

            if (m_Core == null ||!m_Core.LogLoaded)
            {
                return;
            }

            for (int i = 0; i < m_Core.CurTextures.Length; i++)
            {
                bool include = false;
                include |= (RTs && ((m_Core.CurTextures[i].creationFlags & TextureCreationFlags.RTV) > 0 ||
                                    (m_Core.CurTextures[i].creationFlags & TextureCreationFlags.DSV) > 0));
                include |= (Texs && (m_Core.CurTextures[i].creationFlags & TextureCreationFlags.RTV) == 0 &&
                                    (m_Core.CurTextures[i].creationFlags & TextureCreationFlags.DSV) == 0);
                include |= (filter.Length > 0 && (m_Core.CurTextures[i].name.ToUpperInvariant().Contains(filter.ToUpperInvariant())));
                include |= (!RTs && !Texs && filter.Length == 0);

                if (include)
                {
                    m_FilteredTextures.Add(m_Core.CurTextures[i]);
                    Items.Add(m_Core.CurTextures[i].name);
                }
            }
        }
    }

    public class GoIconClickEventArgs : EventArgs
    {
        private ResourceId id;

        public GoIconClickEventArgs(ResourceId i)
        {
            id = i;
        }

        public ResourceId ID
        {
            get { return id; }
        }
    }
}
