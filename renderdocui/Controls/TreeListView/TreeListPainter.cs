using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Text;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using System.Runtime.InteropServices;

// Taken from http://www.codeproject.com/Articles/23746/TreeView-with-Columns with minor tweaks
// and fixes for my purposes.
namespace TreelistView
{
	public class VisualStyleItemBackground // can't find system provided visual style for this.
	{
		[StructLayout(LayoutKind.Sequential)]
		public class RECT
		{
			public int left;
			public int top;
			public int right;
			public int bottom;
			public RECT()
			{
			}

			public RECT(Rectangle r)
			{
				this.left = r.X;
				this.top = r.Y;
				this.right = r.Right;
				this.bottom = r.Bottom;
			}
		}

		[DllImport("uxtheme.dll", CharSet=CharSet.Auto)]
		public static extern int DrawThemeBackground(IntPtr hTheme, IntPtr hdc, int partId, int stateId, [In] RECT pRect, [In] RECT pClipRect);

		[DllImport("uxtheme.dll", CharSet=CharSet.Auto)]
		public static extern IntPtr OpenThemeData(IntPtr hwnd, [MarshalAs(UnmanagedType.LPWStr)] string pszClassList);
 
		[DllImport("uxtheme.dll", CharSet=CharSet.Auto)]
		public static extern int CloseThemeData(IntPtr hTheme);

		//http://www.ookii.org/misc/vsstyle.h
		//http://msdn2.microsoft.com/en-us/library/bb773210(VS.85).aspx
		enum ITEMSTATES
		{
			LBPSI_HOT = 1,
			LBPSI_HOTSELECTED = 2,
			LBPSI_SELECTED = 3,
			LBPSI_SELECTEDNOTFOCUS = 4,
		};

		enum LISTBOXPARTS
		{
			LBCP_BORDER_HSCROLL = 1,
			LBCP_BORDER_HVSCROLL = 2,
			LBCP_BORDER_NOSCROLL = 3,
			LBCP_BORDER_VSCROLL = 4,
			LBCP_ITEM = 5,
		};

		public enum Style
		{
			Normal,
			Inactive,	// when not focused
		}
		Style m_style;
		public VisualStyleItemBackground(Style style)
		{
			m_style = style;
		}
		public void DrawBackground(Control owner, Graphics dc, Rectangle r, Color col)
		{
            /*
			IntPtr themeHandle = OpenThemeData(owner.Handle, "Explorer");
			if (themeHandle != IntPtr.Zero)
			{
				DrawThemeBackground(themeHandle, dc.GetHdc(), (int)LISTBOXPARTS.LBCP_ITEM, (int)ITEMSTATES.LBPSI_SELECTED, new RECT(r), new RECT(r));
				dc.ReleaseHdc();
				CloseThemeData(themeHandle);
				return;
			}
            */

            Pen pen = new Pen(col);
			GraphicsPath path = new GraphicsPath();
			path.AddLine(r.Left + 2, r.Top, r.Right - 2, r.Top);
			path.AddLine(r.Right, r.Top + 2, r.Right, r.Bottom - 2);
			path.AddLine(r.Right - 2, r.Bottom, r.Left + 2, r.Bottom);
			path.AddLine(r.Left, r.Bottom - 2, r.Left, r.Top + 2);
			path.CloseFigure();
			dc.DrawPath(pen, path);

			//r.Inflate(-1, -1);
            LinearGradientBrush brush = new LinearGradientBrush(r, Color.FromArgb(120, col), col, 90);
			dc.FillRectangle(brush, r);
			// for some reason in some cases the 'white' end of the gradient brush is drawn with the starting color
			// therefore this redraw of the 'top' line of the rectangle
			//dc.DrawLine(Pens.White, r.Left + 1, r.Top, r.Right - 1, r.Top);

			pen.Dispose();
			brush.Dispose();
			path.Dispose();
		}
	}

    public delegate string CellDataToString(TreeListColumn column, object data);

	public class CellPainter
    {
        public static Rectangle AdjustRectangle(Rectangle r, Padding padding)
        {
            r.X      += padding.Left;
            r.Width  -= padding.Left + padding.Right;
            r.Y      += padding.Top;
            r.Height -= padding.Top + padding.Bottom;
            return r;
        }
		protected TreeListView m_owner;
        protected CellDataToString m_converter = null;

        public CellDataToString CellDataConverter
        {
            get { return m_converter; }
            set { m_converter = value; }
        }

		public CellPainter(TreeListView owner)
		{
			m_owner = owner;
		}
		public virtual void DrawSelectionBackground(Graphics dc, Rectangle nodeRect, Node node)
        {
            Point mousePoint = m_owner.PointToClient(Cursor.Position);
            Node hoverNode = m_owner.CalcHitNode(mousePoint);

			if (m_owner.NodesSelection.Contains(node) || m_owner.FocusedNode == node)
			{
                Color col = m_owner.Focused ? SystemColors.Highlight : SystemColors.Control;

				if (!Application.RenderWithVisualStyles)
				{
					// have to fill the solid background only before the node is painted
                    dc.FillRectangle(SystemBrushes.FromSystemColor(col), nodeRect);
				}
				else
                {
                    col = m_owner.Focused ? SystemColors.Highlight : Color.FromArgb(180, SystemColors.ControlDark);

					// have to draw the transparent background after the node is painted
					VisualStyleItemBackground.Style style = VisualStyleItemBackground.Style.Normal;
					if (m_owner.Focused == false)
						style = VisualStyleItemBackground.Style.Inactive;
					VisualStyleItemBackground rendere = new VisualStyleItemBackground(style);
                    rendere.DrawBackground(m_owner, dc, nodeRect, col);
				}
			}
            else if (hoverNode == node && m_owner.RowOptions.HoverHighlight)
            {
                Color col = SystemColors.ControlLight;

                if (SystemInformation.HighContrast)
                {
                    col = SystemColors.ButtonHighlight;
                }

                if (!Application.RenderWithVisualStyles)
                {
                    // have to fill the solid background only before the node is painted
                    dc.FillRectangle(SystemBrushes.FromSystemColor(col), nodeRect);
                }
                else
                {
                    // have to draw the transparent background after the node is painted
                    VisualStyleItemBackground.Style style = VisualStyleItemBackground.Style.Normal;
                    if (m_owner.Focused == false)
                        style = VisualStyleItemBackground.Style.Inactive;
                    VisualStyleItemBackground rendere = new VisualStyleItemBackground(style);
                    rendere.DrawBackground(m_owner, dc, nodeRect, col);
                }
            }

			if (m_owner.Focused && (m_owner.FocusedNode == node))
			{
				nodeRect.Height += 1;
				nodeRect.Inflate(-1,-1);
				ControlPaint.DrawFocusRectangle(dc, nodeRect);
			}
		}
        public virtual void PaintCellBackground(Graphics dc,
            Rectangle cellRect,
            Node node,
            TreeListColumn column,
            TreeList.TextFormatting format,
            object data)
        {
            Color c = Color.Transparent;

            Point mousePoint = m_owner.PointToClient(Cursor.Position);
            Node hoverNode = m_owner.CalcHitNode(mousePoint);

            if (format.BackColor != Color.Transparent)
                c = format.BackColor;

            if (!m_owner.NodesSelection.Contains(node) && m_owner.FocusedNode != node &&
                !(hoverNode == node && m_owner.RowOptions.HoverHighlight) &&
                node.DefaultBackColor != Color.Transparent)
                c = node.DefaultBackColor;

            if (node.BackColor != Color.Transparent && !m_owner.NodesSelection.Contains(node) && m_owner.SelectedNode != node)
                c = node.BackColor;

            if (column.Index < node.IndexedBackColor.Length && node.IndexedBackColor[column.Index] != Color.Transparent)
                c = node.IndexedBackColor[column.Index];
            
            if (c != Color.Transparent)
            {
                Rectangle r = cellRect;
                r.X -= Math.Max(0, column.CalculatedRect.Width - cellRect.Width);
                r.Width += Math.Max(0, column.CalculatedRect.Width - cellRect.Width);
                SolidBrush brush = new SolidBrush(c);
                dc.FillRectangle(brush, r);
                brush.Dispose();
            }
        }

		public virtual void PaintCellText(Graphics dc, 
			Rectangle cellRect, 
			Node node, 
			TreeListColumn column, 
			TreeList.TextFormatting format, 
			object data)
		{
			if (data != null)
			{
				cellRect = AdjustRectangle(cellRect, format.Padding);
				//dc.DrawRectangle(Pens.Black, cellRect);

                Color color = format.ForeColor;
                if (node.ForeColor != Color.Transparent)
                    color = node.ForeColor;
                if (m_owner.FocusedNode == node && Application.RenderWithVisualStyles == false && m_owner.Focused)
					color = SystemColors.HighlightText;
				TextFormatFlags flags= TextFormatFlags.EndEllipsis | format.GetFormattingFlags();

                Font f = m_owner.Font;
                Font disposefont = null;

                if(node.Bold && node.Italic)
                    disposefont = f = new Font(f, FontStyle.Bold|FontStyle.Italic);
                else if (node.Bold)
                    disposefont = f = new Font(f, FontStyle.Bold);
                else if (node.Italic)
                    disposefont = f = new Font(f, FontStyle.Italic);

                string datastring = "";
                
                if(m_converter != null)
                    datastring = m_converter(column, data);
                else
                    datastring = data.ToString();

                TextRenderer.DrawText(dc, datastring, f, cellRect, color, flags);

                Size sz = TextRenderer.MeasureText(dc, datastring, f, new Size(1000000, 10000), flags);

                int treecolumn = node.TreeColumn;
                if (treecolumn < 0)
                    treecolumn = node.OwnerView.TreeColumn;

                if (column.Index == treecolumn)
                    node.ClippedText = (sz.Width > cellRect.Width || sz.Height > cellRect.Height);

                if (disposefont != null) disposefont.Dispose();
			}
		}
        public virtual void PaintCellPlusMinus(Graphics dc, Rectangle glyphRect, Node node, TreeListColumn column, TreeList.TextFormatting format)
		{
			if (!Application.RenderWithVisualStyles)
            {
                // find square rect first
                int diff = glyphRect.Height-glyphRect.Width;
                glyphRect.Y += diff/2;
                glyphRect.Height -= diff;

                // draw 8x8 box centred
                while (glyphRect.Height > 8)
                {
                    glyphRect.Height -= 2;
                    glyphRect.Y += 1;
                    glyphRect.X += 1;
                }

                // make a box
                glyphRect.Width = glyphRect.Height;

                // clear first
                SolidBrush brush = new SolidBrush(format.BackColor);
                if (format.BackColor == Color.Transparent)
                    brush = new SolidBrush(m_owner.BackColor);
                dc.FillRectangle(brush, glyphRect);
                brush.Dispose();

                // draw outline
                Pen p = new Pen(SystemColors.ControlDark);
                dc.DrawRectangle(p, glyphRect);
                p.Dispose();

                p = new Pen(SystemColors.ControlText);

                // reduce box for internal lines
                glyphRect.X += 2; glyphRect.Y += 2;
                glyphRect.Width -= 4; glyphRect.Height -= 4;

                // draw horizontal line always
                dc.DrawLine(p, glyphRect.X, glyphRect.Y + glyphRect.Height / 2, glyphRect.X + glyphRect.Width, glyphRect.Y + glyphRect.Height / 2);

                // draw vertical line if this should be a +
                if(!node.Expanded)
                    dc.DrawLine(p, glyphRect.X + glyphRect.Width / 2, glyphRect.Y, glyphRect.X + glyphRect.Width / 2, glyphRect.Y + glyphRect.Height);

                p.Dispose();
				return;
			}

			VisualStyleElement element = VisualStyleElement.TreeView.Glyph.Closed;
			if (node.Expanded)
				element = VisualStyleElement.TreeView.Glyph.Opened;

			if (VisualStyleRenderer.IsElementDefined(element))
			{
				VisualStyleRenderer renderer = new VisualStyleRenderer(element);
				renderer.DrawBackground(dc, glyphRect);
			}
		}
	}
	public class ColumnHeaderPainter
    {
        TreeListView m_owner;
        public ColumnHeaderPainter(TreeListView owner)
        {
            m_owner = owner;
        }

        public static Rectangle AdjustRectangle(Rectangle r, Padding padding)
        {
            r.X      += padding.Left;
            r.Width  -= padding.Left + padding.Right;
            r.Y      += padding.Top;
            r.Height -= padding.Top + padding.Bottom;
            return r;
        }
		public virtual void DrawHeaderFiller(Graphics dc, Rectangle r)
		{
			if (!Application.RenderWithVisualStyles)
			{
				ControlPaint.DrawButton(dc, r, ButtonState.Flat);
				return;
			}
			VisualStyleElement element = VisualStyleElement.Header.Item.Normal;
			if (VisualStyleRenderer.IsElementDefined(element))
			{
				VisualStyleRenderer renderer = new VisualStyleRenderer(element);
				renderer.DrawBackground(dc, r);
			}
		}
        public void DrawHeaderText(Graphics dc, Rectangle cellRect, TreeListColumn column, TreeList.TextFormatting format)
        {
            Color color = format.ForeColor;
            TextFormatFlags flags = TextFormatFlags.EndEllipsis | format.GetFormattingFlags();
            TextRenderer.DrawText(dc, column.Caption, column.Font, cellRect, color, flags);
        }
		public virtual void DrawHeader(Graphics dc, Rectangle cellRect, TreeListColumn column, TreeList.TextFormatting format, bool isHot, bool highlight)
        {
            Rectangle textRect = AdjustRectangle(cellRect, format.Padding);
			if (!Application.RenderWithVisualStyles)
			{
                ControlPaint.DrawButton(dc, cellRect,
                        m_owner.ViewOptions.UserRearrangeableColumns && highlight ? ButtonState.Pushed : ButtonState.Flat);
                DrawHeaderText(dc, textRect, column, format);
                return;
			}
			VisualStyleElement element = VisualStyleElement.Header.Item.Normal;
            if (isHot || highlight)
				element = VisualStyleElement.Header.Item.Hot;
			if (VisualStyleRenderer.IsElementDefined(element))
			{
				VisualStyleRenderer renderer = new VisualStyleRenderer(element);
				renderer.DrawBackground(dc, cellRect);

				if (format.BackColor != Color.Transparent)
				{
					SolidBrush brush = new SolidBrush(format.BackColor);
					dc.FillRectangle(brush, cellRect);
					brush.Dispose();
				}
                //dc.DrawRectangle(Pens.Black, cellRect);

                DrawHeaderText(dc, textRect, column, format);
			}
		}
		public virtual void DrawVerticalGridLines(TreeListColumnCollection columns, Graphics dc, Rectangle r, int hScrollOffset)
		{
			foreach (TreeListColumn col in columns.VisibleColumns)
			{
				int rightPos = col.CalculatedRect.Right - hScrollOffset;
				if (rightPos < 0)
					continue;
                Pen p = new Pen(columns.Owner.GridLineColour);
				dc.DrawLine(p, rightPos, r.Top, rightPos, r.Bottom);
                p.Dispose();
			}
		}
	}
	public class RowPainter
	{
		public void DrawHeader(Graphics dc, Rectangle r, bool isHot)
		{
			if (!Application.RenderWithVisualStyles)
			{
                if (r.Width > 0 && r.Height > 0)
                {
                    ControlPaint.DrawButton(dc, r, ButtonState.Flat);
                }
				return;
			}

			VisualStyleElement element = VisualStyleElement.Header.Item.Normal;
			if (isHot)
				element = VisualStyleElement.Header.Item.Hot;
			if (VisualStyleRenderer.IsElementDefined(element))
			{
				VisualStyleRenderer renderer = new VisualStyleRenderer(element);
				renderer.DrawBackground(dc, r);
			}
		}
		public void DrawHorizontalGridLine(Graphics dc, Rectangle r, Color col)
		{
            Pen p = new Pen(col);
            dc.DrawLine(p, r.Left, r.Bottom, r.Right, r.Bottom);
            p.Dispose();
		}
	}
}
