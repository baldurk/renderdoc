using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Design;
using System.Windows.Forms;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows.Forms.VisualStyles;

// Taken from http://www.codeproject.com/Articles/23746/TreeView-with-Columns with minor tweaks
// and fixes for my purposes.
namespace TreelistView
{
	[Designer(typeof(TreeListViewDesigner))]
	public class TreeListView : Control, ISupportInitialize
	{
		public event TreeViewEventHandler AfterSelect;
		protected virtual void OnAfterSelect(Node node)
		{
			raiseAfterSelect(node);
		}
		protected virtual void raiseAfterSelect(Node node)
		{
			if (AfterSelect != null && node != null)
				AfterSelect(this, new TreeViewEventArgs(null));
		}

		public delegate void NotifyBeforeExpandHandler(Node node, bool isExpanding);
		public event NotifyBeforeExpandHandler NotifyBeforeExpand;
		public virtual void OnNotifyBeforeExpand(Node node, bool isExpanding)
		{
			raiseNotifyBeforeExpand(node, isExpanding);
		}
		protected virtual void raiseNotifyBeforeExpand(Node node, bool isExpanding)
		{
			if (NotifyBeforeExpand != null)
				NotifyBeforeExpand(node, isExpanding);
		}

		public delegate void NotifyAfterHandler(Node node, bool isExpanding);
		public event NotifyAfterHandler NotifyAfterExpand;
		public virtual void OnNotifyAfterExpand(Node node, bool isExpanded)
		{
			raiseNotifyAfterExpand(node, isExpanded);
		}
		protected virtual void raiseNotifyAfterExpand(Node node, bool isExpanded)
		{
			if (NotifyAfterExpand != null)
				NotifyAfterExpand(node, isExpanded);
		}

        public delegate void NodeDoubleClickedHandler(Node node);
        public event NodeDoubleClickedHandler NodeDoubleClicked;
        public virtual void OnNodeDoubleClicked(Node node)
        {
            raiseNodeDoubleClicked(node);
        }
        protected virtual void raiseNodeDoubleClicked(Node node)
        {
            if (NodeDoubleClicked != null)
                NodeDoubleClicked(node);
        }

        public delegate void NodeClickedHandler(Node node);
        public event NodeClickedHandler NodeClicked;
        public virtual void OnNodeClicked(Node node)
        {
            raiseNodeClicked(node);
        }
        protected virtual void raiseNodeClicked(Node node)
        {
            if (NodeClicked != null)
                NodeClicked(node);
        }

		TreeListViewNodes			m_nodes;
		TreeListColumnCollection	m_columns;
		TreeList.RowSetting			m_rowSetting;
		TreeList.ViewSetting		m_viewSetting;

        Color                       m_GridLineColour = SystemColors.Control;
        Image                       m_SelectedImage = null;

		[Category("Columns")]
		[Browsable(true)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeListColumnCollection Columns
		{
			get { return m_columns; }
		}

		[Category("Options")]
		[Browsable(true)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeList.CollumnSetting ColumnsOptions
		{
            get { return m_columns.Options; }
		}

		[Category("Options")]
		[Browsable(true)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeList.RowSetting RowOptions
		{
			get { return m_rowSetting; }
		}
		
		[Category("Options")]
		[Browsable(true)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeList.ViewSetting ViewOptions
		{
			get { return m_viewSetting; }
		}

		[Category("Behavior")]
		[DefaultValue(typeof(bool), "True")]
		public bool MultiSelect
		{
			get { return m_multiSelect; }
			set { m_multiSelect = value; }
		}

        [Category("Behavior")]
        [DefaultValue(typeof(int), "0")]
        public int TreeColumn
        {
            get { return m_treeColumn; }
            set
            {
                m_treeColumn = value;

                if(value >= m_columns.Count)
                    throw new ArgumentOutOfRangeException("Tree column index invalid");
            }
        }

        private int GetTreeColumn(Node n)
        {
            if (n != null && n.TreeColumn >= 0)
                return n.TreeColumn;

            return m_treeColumn;
        }

        [Category("Behavior")]
        [DefaultValue(typeof(bool), "False")]
        public bool AlwaysDisplayVScroll
        {
            get { return m_vScrollAlways; }
            set { m_vScrollAlways = value; }
        }

        [Category("Behavior")]
        [DefaultValue(typeof(bool), "False")]
        public bool AlwaysDisplayHScroll
        {
            get { return m_hScrollAlways; }
            set { m_hScrollAlways = value; }
        }

        [Category("Appearance")]
        [DefaultValue(typeof(Image), null)]
        public Image SelectedImage
        {
            get { return m_SelectedImage; }
            set { m_SelectedImage = value; }
        }

		[DefaultValue(typeof(Color), "Window")]
		public new Color BackColor
		{
			get { return base.BackColor; }
			set { base.BackColor = value; }
		}

        [Category("Appearance")]
		[DefaultValue(typeof(Color), "Control")]
        public Color GridLineColour
		{
            get { return m_GridLineColour; }
            set { m_GridLineColour = value; }
		}

		//[Browsable(false)]
		public TreeListViewNodes Nodes
		{
			get { return m_nodes; }
		}
		public TreeListView()
		{
			this.DoubleBuffered = true;
			this.BackColor = SystemColors.Window;
			this.TabStop = true;

			m_tooltip = new ToolTip();
			m_tooltipVisible = false;
			m_tooltip.InitialDelay = 0;
			m_tooltip.UseAnimation = false;
			m_tooltip.UseFading = false;

			m_tooltipNode = null;
			m_tooltipTimer = new Timer();
			m_tooltipTimer.Stop();
			m_tooltipTimer.Interval = 500;
			m_tooltipTimer.Tick += new EventHandler(tooltipTick);

			m_rowPainter = new RowPainter();
			m_cellPainter = new CellPainter(this);

			m_nodes = new TreeListViewNodes(this);
			m_rowSetting = new TreeList.RowSetting(this);
            m_viewSetting = new TreeList.ViewSetting(this);
            m_columns = new TreeListColumnCollection(this);
			AddScrollBars();
		}

		protected override void Dispose(bool disposing)
		{
			m_tooltipTimer.Stop();
			if(m_tooltipVisible)
				m_tooltip.Hide(this);
			m_tooltip.Dispose();
			base.Dispose(disposing);
		}

		void tooltipTick(object sender, EventArgs e)
		{
			m_tooltipTimer.Stop();

			if (m_tooltipNode == null)
			{
				m_tooltip.Hide(this);
				m_tooltipVisible = false;
				return;
			}

			Node node = m_tooltipNode;

			Point p = PointToClient(Cursor.Position);

			if (!ClientRectangle.Contains(p))
			{
				m_tooltip.Hide(this);
				m_tooltipVisible = false;
				return;
			}

			int visibleRowIndex = CalcHitRow(PointToClient(Cursor.Position));

			Rectangle rowRect = CalcRowRectangle(visibleRowIndex);
			rowRect.X = RowHeaderWidth() - HScrollValue();
			rowRect.Width = Columns.ColumnsWidth;

			// draw the current node
			foreach (TreeListColumn col in Columns.VisibleColumns)
			{
				if (col.Index == GetTreeColumn(node))
				{
					Rectangle cellRect = rowRect;
					cellRect.X = col.CalculatedRect.X - HScrollValue();

					int lineindet = 10;
					// add left margin
					cellRect.X += Columns.Options.LeftMargin;

					// add indent size
					cellRect.X += GetIndentSize(node) + 5;

					cellRect.X += lineindet;

					Rectangle plusminusRect = GetPlusMinusRectangle(node, col, visibleRowIndex);

					if (!ViewOptions.ShowLine && (!ViewOptions.ShowPlusMinus || (!ViewOptions.PadForPlusMinus && plusminusRect == Rectangle.Empty)))
						cellRect.X -= (lineindet + 5);

					if (SelectedImage != null && (NodesSelection.Contains(node) || FocusedNode == node))
						cellRect.X += (SelectedImage.Width + 2);

					Image icon = GetHoverNodeBitmap(node);

					if (icon != null)
						cellRect.X += (icon.Width + 2);

					string datastring = "";

					object data = GetData(node, col);

					if(data == null)
						data = "";

					if (CellPainter.CellDataConverter != null)
						datastring = CellPainter.CellDataConverter(col, data);
					else
						datastring = data.ToString();

					if(datastring.Length > 0)
					{
						m_tooltip.Show(datastring, this, cellRect.X, cellRect.Y);
						m_tooltipVisible = true;
					}
				}
			}
		}

		public void RecalcLayout()
		{
			if (m_firstVisibleNode == null)
				m_firstVisibleNode = Nodes.FirstNode;
			if (Nodes.Count == 0)
				m_firstVisibleNode = null;

			UpdateScrollBars();
            m_columns.RecalcVisibleColumsRect();
            UpdateScrollBars();
            m_columns.RecalcVisibleColumsRect();

			int vscroll = VScrollValue();
			if (vscroll == 0)
				m_firstVisibleNode = Nodes.FirstNode;
			else
				m_firstVisibleNode = NodeCollection.GetNextNode(Nodes.FirstNode, vscroll);
			Invalidate();
		}
		void AddScrollBars()
		{
			// I was not able to get the wanted behavior by using ScrollableControl with AutoScroll enabled.
			// horizontal scrolling is ok to do it by pixels, but for vertical I want to maintain the headers
			// and only scroll the rows.
			// I was not able to manually overwrite the vscroll bar handling to get this behavior, instead I opted for
			// custom implementation of scrollbars

			// to get the 'filler' between hscroll and vscroll I dock scroll + filler in a panel
			m_hScroll = new HScrollBar();
			m_hScroll.Scroll += new ScrollEventHandler(OnHScroll);
			m_hScroll.Dock = DockStyle.Fill;

			m_vScroll = new VScrollBar();
			m_vScroll.Scroll += new ScrollEventHandler(OnVScroll);
			m_vScroll.Dock = DockStyle.Right;

			m_hScrollFiller = new Panel();
			m_hScrollFiller.BackColor = Color.Transparent;
			m_hScrollFiller.Size = new Size(m_vScroll.Width-1, m_hScroll.Height);
			m_hScrollFiller.Dock = DockStyle.Right;
			
			Controls.Add(m_vScroll);

			m_hScrollPanel = new Panel();
			m_hScrollPanel.Height = m_hScroll.Height;
			m_hScrollPanel.Dock = DockStyle.Bottom;
			m_hScrollPanel.Controls.Add(m_hScroll);
			m_hScrollPanel.Controls.Add(m_hScrollFiller);
			Controls.Add(m_hScrollPanel);

			// try and force handle creation here, as it can fail randomly
			// at runtime with weird side-effects (See github #202).
			bool handlesCreated = false;
			handlesCreated |= m_hScroll.Handle.ToInt64() > 0;
			handlesCreated |= m_vScroll.Handle.ToInt64() > 0;
			handlesCreated |= m_hScrollFiller.Handle.ToInt64() > 0;
			handlesCreated |= m_hScrollPanel.Handle.ToInt64() > 0;

			if (!handlesCreated)
				renderdoc.StaticExports.LogText("Couldn't create any handles!");
		}
		
		ToolTip     m_tooltip;
		Node        m_tooltipNode;
		Timer       m_tooltipTimer;
		bool        m_tooltipVisible;
		VScrollBar	m_vScroll;
		HScrollBar	m_hScroll;
		Panel		m_hScrollFiller;
		Panel		m_hScrollPanel;
		bool		m_multiSelect = true;
        int         m_treeColumn = 0;
        bool        m_vScrollAlways = false;
        bool        m_hScrollAlways = false;
		Node		m_firstVisibleNode = null;

		RowPainter m_rowPainter;
		CellPainter m_cellPainter;
		[Browsable(false)]
		public CellPainter CellPainter
		{
			get { return m_cellPainter; }
			set { m_cellPainter = value; }
		}

		TreeListColumn	m_resizingColumn;
		int m_resizingColumnScrollOffset;
        int m_resizingColumnLeft;

		TreeListColumn	m_movingColumn;

		NodesSelection	m_nodesSelection = new NodesSelection();
		Node			m_focusedNode = null;

		[Browsable(false)]
		public NodesSelection NodesSelection
		{
			get { return m_nodesSelection; }
		}

		public void SortNodesSelection()
		{
			m_nodesSelection.Sort();
		}

        public void SelectAll()
        {
            FocusedNode = null;
            NodesSelection.Clear();
            foreach (Node node in NodeCollection.ForwardNodeIterator(m_firstVisibleNode, true))
            {
                NodesSelection.Add(node);
            }
            Invalidate();
        }

        [Browsable(false)]
        public Node SelectedNode
        {
            get { return m_nodesSelection.Count == 0 ? FocusedNode : m_nodesSelection[0]; }
        }

		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public Node FocusedNode
		{
			get { return m_focusedNode; }
			set
			{
				Node curNode = FocusedNode;
				if (object.ReferenceEquals(curNode, value))
					return;
				if (MultiSelect == false)
					NodesSelection.Clear();
				
				int oldrow = NodeCollection.GetVisibleNodeIndex(curNode);
				int newrow = NodeCollection.GetVisibleNodeIndex(value);
				
				m_focusedNode = value;
				OnAfterSelect(value);
				InvalidateRow(oldrow);
				InvalidateRow(newrow);
				EnsureVisible(m_focusedNode);
			}
		}
		public void EnsureVisible(Node node)
		{
			int screenvisible = MaxVisibleRows() - 1;
			int visibleIndex = NodeCollection.GetVisibleNodeIndex(node);
			if (visibleIndex < VScrollValue())
			{
				SetVScrollValue(visibleIndex);
			}
			if (visibleIndex > VScrollValue() + screenvisible)
			{
				SetVScrollValue(visibleIndex - screenvisible);
			}
		}
		public Node CalcHitNode(Point mousepoint)
		{
            if (!ClientRectangle.Contains(mousepoint))
                return null;

			int hitrow = CalcHitRow(mousepoint);
			if (hitrow < 0)
				return null;
			return NodeCollection.GetNextNode(m_firstVisibleNode, hitrow);
		}
		public Node GetHitNode()
		{
			return CalcHitNode(PointToClient(Control.MousePosition));
		}
		public TreelistView.HitInfo CalcColumnHit(Point mousepoint)
		{
			return Columns.CalcHitInfo(mousepoint, HScrollValue());
		}
		public bool HitTestScrollbar(Point mousepoint)
		{
			if (m_hScroll.Visible && mousepoint.Y >= ClientRectangle.Height - m_hScroll.Height)
				return true;
			return false;
		}

		protected override void OnSizeChanged(EventArgs e)
		{
			base.OnSizeChanged(e);
			if (ClientRectangle.Width > 0 && ClientRectangle.Height > 0)
			{
				Columns.RecalcVisibleColumsRect();
				UpdateScrollBars();
				Columns.RecalcVisibleColumsRect();
			}
		}
        protected override void OnVisibleChanged(EventArgs e)
        {
            base.OnVisibleChanged(e);
            RecalcLayout();
        }
		protected virtual void BeforeShowContextMenu()
		{
		}
		protected void InvalidateRow(int absoluteRowIndex)
		{
			int visibleRowIndex = absoluteRowIndex - VScrollValue();
			Rectangle r = CalcRowRectangle(visibleRowIndex);
			if (r != Rectangle.Empty)
			{
				r.Inflate(1,1);
				Invalidate(r);
			}
		}

		void OnVScroll(object sender, ScrollEventArgs e)
		{
			int diff = e.NewValue - e.OldValue;
			//assumedScrollPos += diff;
			if (e.NewValue == 0)
			{
				m_firstVisibleNode = Nodes.FirstNode;
				diff = 0;
			}
			m_firstVisibleNode = NodeCollection.GetNextNode(m_firstVisibleNode, diff);
			Invalidate();
		}
		void OnHScroll(object sender, ScrollEventArgs e)
		{
			Invalidate();
		}
		public void SetVScrollValue(int value)
		{
			if (value < 0)
				value = 0;
			int max = m_vScroll.Maximum - m_vScroll.LargeChange + 1;
			if (value > max)
				value = max;

			if ((value >= 0 && value <= max) && (value != m_vScroll.Value))
			{
				ScrollEventArgs e = new ScrollEventArgs(ScrollEventType.ThumbPosition, m_vScroll.Value, value, ScrollOrientation.VerticalScroll);
				// setting the scroll value does not cause a Scroll event
				m_vScroll.Value = value;
				// so we have to fake it
				OnVScroll(m_vScroll, e);
			}
		}
		public int VScrollValue()
		{
			if (m_vScroll.Visible == false)
				return 0;
			return m_vScroll.Value;
		}
		int HScrollValue()
		{
			if (m_hScroll.Visible == false)
				return 0;
			return m_hScroll.Value;
		}
		void UpdateScrollBars()
		{
			if (ClientRectangle.Width < 0)
				return;
			int maxvisiblerows = MaxVisibleRows();
            int totalrows = Nodes.VisibleNodeCount;
            m_vScroll.SmallChange = 1;
            m_vScroll.LargeChange = Math.Max(1, maxvisiblerows);
            m_vScroll.Enabled = true;
            m_vScroll.Minimum = 0;
            m_vScroll.Maximum = Math.Max(1,totalrows - 1);
			if (maxvisiblerows >= totalrows)
			{
				m_vScroll.Visible = false;
				SetVScrollValue(0);

                if (m_vScrollAlways)
                {
                    m_vScroll.Visible = true;
                    m_vScroll.Enabled = false;
                }
			}
			else
			{
				m_vScroll.Visible = true;

				int maxscrollvalue = m_vScroll.Maximum - m_vScroll.LargeChange;
				if (maxscrollvalue < m_vScroll.Value)
					SetVScrollValue(maxscrollvalue);
			}

            m_hScroll.Enabled = true;
			if (ClientRectangle.Width > MinWidth())
			{
				m_hScrollPanel.Visible = false;
				m_hScroll.Value = 0;

                if (m_hScrollAlways)
                {
                    m_hScroll.Enabled = false;
                    m_hScrollPanel.Visible = true;
                    m_hScroll.Minimum = 0;
                    m_hScroll.Maximum = 0;
                    m_hScroll.SmallChange = 1;
                    m_hScroll.LargeChange = 1;
                    m_hScrollFiller.Visible = m_vScroll.Visible;
                }
			}	
			else
			{
				m_hScroll.Minimum = 0;
				m_hScroll.Maximum = Math.Max(1, MinWidth());
				m_hScroll.SmallChange = 5;
				m_hScroll.LargeChange = Math.Max(1, ClientRectangle.Width);
				m_hScrollFiller.Visible = m_vScroll.Visible;
				m_hScrollPanel.Visible = true;
			}
		}
		int m_hotrow = -1;
		int CalcHitRow(Point mousepoint)
		{
			if (mousepoint.Y <= Columns.Options.HeaderHeight)
				return -1;
			return (mousepoint.Y - Columns.Options.HeaderHeight) / RowOptions.ItemHeight;
		}
		int VisibleRowToYPoint(int visibleRowIndex)
		{
			return Columns.Options.HeaderHeight + (visibleRowIndex * RowOptions.ItemHeight);
		}
		Rectangle CalcRowRectangle(int visibleRowIndex)
		{
			Rectangle r = ClientRectangle;
			r.Y = VisibleRowToYPoint(visibleRowIndex);
			if (r.Top < Columns.Options.HeaderHeight || r.Top > ClientRectangle.Height)
				return Rectangle.Empty;
			r.Height = RowOptions.ItemHeight;
			return r;
		}

		void MultiSelectAdd(Node clickedNode, Keys modifierKeys)
		{
			if (Control.ModifierKeys == Keys.None)
			{
				foreach (Node node in NodesSelection)
				{
					int newrow = NodeCollection.GetVisibleNodeIndex(node);
					InvalidateRow(newrow);
				}
				NodesSelection.Clear();
				NodesSelection.Add(clickedNode);
			}
			if (Control.ModifierKeys == Keys.Shift)
			{
				if (NodesSelection.Count == 0)
					NodesSelection.Add(clickedNode);
				else
				{
					int startrow = NodeCollection.GetVisibleNodeIndex(NodesSelection[0]);
					int currow = NodeCollection.GetVisibleNodeIndex(clickedNode);
					if (currow > startrow)
					{
						Node startingNode = NodesSelection[0];
						NodesSelection.Clear();
						foreach (Node node in NodeCollection.ForwardNodeIterator(startingNode, clickedNode, true))
							NodesSelection.Add(node);
						Invalidate();
					}
					if (currow < startrow)
					{
						Node startingNode = NodesSelection[0];
						NodesSelection.Clear();
						foreach (Node node in NodeCollection.ReverseNodeIterator(startingNode, clickedNode, true))
							NodesSelection.Add(node);
						Invalidate();
					}
				}
			}
			if (Control.ModifierKeys == Keys.Control)
			{
				if (NodesSelection.Contains(clickedNode))
					NodesSelection.Remove(clickedNode);
				else
					NodesSelection.Add(clickedNode);
			}
			InvalidateRow(NodeCollection.GetVisibleNodeIndex(clickedNode));
			FocusedNode = clickedNode;
		}
		internal event MouseEventHandler AfterResizingColumn;
		protected override void OnMouseClick(MouseEventArgs e)
		{
			if (e.Button == MouseButtons.Left)
			{
				Point mousePoint = new Point(e.X, e.Y);
				Node clickedNode = CalcHitNode(mousePoint);
                if (clickedNode != null && Columns.Count > 0)
                {
                    int clickedRow = CalcHitRow(mousePoint);
                    Rectangle glyphRect = Rectangle.Empty;

                    int treeColumn = GetTreeColumn(clickedNode);

                    if (treeColumn >= 0)
                        glyphRect = GetPlusMinusRectangle(clickedNode, Columns[treeColumn], clickedRow);
                    if (clickedNode.HasChildren && glyphRect != Rectangle.Empty && glyphRect.Contains(mousePoint))
                        clickedNode.Expanded = !clickedNode.Expanded;

                    var columnHit = CalcColumnHit(mousePoint);

                    if (glyphRect == Rectangle.Empty && columnHit.Column != null &&
                        columnHit.Column.Index == treeColumn && GetNodeBitmap(clickedNode) != null)
                    {
                        OnNodeClicked(clickedNode);
                    }

                    if (MultiSelect)
                    {
                        MultiSelectAdd(clickedNode, Control.ModifierKeys);
                    }
                    else
                        FocusedNode = clickedNode;
                }
                    /*
                else
                {
                    FocusedNode = null;
                    NodesSelection.Clear();
                }*/
			}
			base.OnMouseClick(e);
		}
		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

            if (m_movingColumn != null)
            {
                m_movingColumn.Moving = true;
                Cursor = Cursors.SizeAll;

                var idx = m_movingColumn.VisibleIndex;
                if (idx + 1 < Columns.VisibleColumns.Length)
                {
                    var nextcol = Columns.VisibleColumns[idx + 1];
                    if (nextcol.CalculatedRect.X + (nextcol.CalculatedRect.Width * 3) / 4 < e.X)
                    {
                        Columns.SetVisibleIndex(m_movingColumn, idx + 1);
                    }
                }
                if (idx - 1 >= 0)
                {
                    var prevcol = Columns.VisibleColumns[idx - 1];
                    if (prevcol.CalculatedRect.Right - (prevcol.CalculatedRect.Width * 3) / 4 > e.X)
                    {
                        Columns.SetVisibleIndex(m_movingColumn, idx - 1);
                    }
                }

                Columns.RecalcVisibleColumsRect(true);
                Invalidate();
                return;
            }
			if (m_resizingColumn != null)
			{
                // if we've clicked on an autosize column, actually resize the next one along.
                if (m_resizingColumn.AutoSize)
                {
                    if (Columns.VisibleColumns.Length > m_resizingColumn.VisibleIndex + 1)
                    {
                        TreeListColumn realResizeColumn = Columns.VisibleColumns[m_resizingColumn.VisibleIndex + 1];

                        int right = realResizeColumn.CalculatedRect.Right - m_resizingColumnScrollOffset;
                        int width = right - e.X;
                        if (width < 10)
                            width = 10;

                        bool resize = true;

                        if (Columns.VisibleColumns.Length > realResizeColumn.VisibleIndex + 1)
                            if (Columns.VisibleColumns[realResizeColumn.VisibleIndex + 1].CalculatedRect.Width <= 10 && m_resizingColumn.Width < width)
                                resize = false;

                        if (realResizeColumn.VisibleIndex > 1)
                            if (Columns.VisibleColumns[realResizeColumn.VisibleIndex - 1].CalculatedRect.Width <= 10 && m_resizingColumn.Width < width)
                                resize = false;

                        if (resize)
                        {
                            realResizeColumn.Width = width;
                        }
                    }
                }
                else
                {
                    int left = m_resizingColumnLeft;
                    int width = e.X - left;
                    if (width < 10)
                        width = 10;

                    bool resize = true;

                    if (Columns.VisibleColumns.Length > m_resizingColumn.VisibleIndex + 1)
                        if (Columns.VisibleColumns[m_resizingColumn.VisibleIndex + 1].CalculatedRect.Width <= 10 && m_resizingColumn.Width < width)
                            resize = false;

                    if (m_resizingColumn.internalIndex > 1)
                        if (Columns.VisibleColumns[m_resizingColumn.VisibleIndex - 1].CalculatedRect.Width <= 10 && m_resizingColumn.Width < width)
                            resize = false;

                    if (resize)
                        m_resizingColumn.Width = width;
                }

				Columns.RecalcVisibleColumsRect(true);
				Invalidate();
				return;
			}

			TreeListColumn hotcol = null;
			TreelistView.HitInfo info = Columns.CalcHitInfo(new Point(e.X, e.Y), HScrollValue());
			if ((int)(info.HitType & HitInfo.eHitType.kColumnHeader) > 0)
				hotcol = info.Column;

            Node clickedNode = CalcHitNode(new Point(e.X, e.Y));

            if ((int)(info.HitType & HitInfo.eHitType.kColumnHeaderResize) > 0)
                Cursor = Cursors.VSplit;
            else if (info.Column != null &&
                     info.Column.Index == GetTreeColumn(clickedNode) &&
                     GetNodeBitmap(clickedNode) != null &&
                     m_viewSetting.HoverHandTreeColumn)
                Cursor = Cursors.Hand;
            else
                Cursor = Cursors.Arrow;

            if (!this.DesignMode && clickedNode != null && clickedNode.ClippedText)
            {
                m_tooltipNode = clickedNode;
                m_tooltipTimer.Start();
            }
            else
            {
                m_tooltipNode = null;
                m_tooltip.Hide(this);
                m_tooltipVisible = false;
                m_tooltipTimer.Stop();
            }

            if (GetHoverNodeBitmap(clickedNode) != null &&
                GetNodeBitmap(clickedNode) != GetHoverNodeBitmap(clickedNode))
                Invalidate();

			SetHotColumn(hotcol, true);

			int vScrollOffset = VScrollValue();
			
			int newhotrow = -1;
			if (hotcol == null)
			{
				int row = (e.Y - Columns.Options.HeaderHeight) / RowOptions.ItemHeight;
				newhotrow = row + vScrollOffset;
			}
			if (newhotrow != m_hotrow)
			{
				InvalidateRow(m_hotrow);
				m_hotrow = newhotrow;
				InvalidateRow(m_hotrow);
			}
		}
		protected override void OnMouseLeave(EventArgs e)
		{
			base.OnMouseLeave(e);
            SetHotColumn(null, false);
            Cursor = Cursors.Arrow;
            Invalidate();
		}
		protected override void OnMouseWheel(MouseEventArgs e)
		{
			m_tooltip.Hide(this);
			m_tooltipVisible = false;
			m_tooltipTimer.Stop();

			int value = m_vScroll.Value - (e.Delta * SystemInformation.MouseWheelScrollLines / 120);
			if (m_vScroll.Visible)
				SetVScrollValue(value);
			base.OnMouseWheel(e);
		}
		protected override void OnMouseDown(MouseEventArgs e)
		{
			m_tooltip.Hide(this);
			m_tooltipVisible = false;
			m_tooltipTimer.Stop();

			this.Focus();
			if (e.Button == MouseButtons.Right)
			{
				Point mousePoint = new Point(e.X, e.Y);
				Node clickedNode = CalcHitNode(mousePoint);
				if (clickedNode != null)
				{
					// if multi select the selection is cleard if clicked node is not in selection
					if (MultiSelect)
					{
						if (NodesSelection.Contains(clickedNode) == false)
							MultiSelectAdd(clickedNode, Control.ModifierKeys);
					}
					FocusedNode = clickedNode;
					Invalidate();
				}
				BeforeShowContextMenu();
			}

			if (e.Button == MouseButtons.Left)
            {
                TreelistView.HitInfo info = Columns.CalcHitInfo(new Point(e.X, e.Y), HScrollValue());
				if ((int)(info.HitType & HitInfo.eHitType.kColumnHeaderResize) > 0)
				{
					m_resizingColumn = info.Column;
					m_resizingColumnScrollOffset = HScrollValue();
                    m_resizingColumnLeft = m_resizingColumn.CalculatedRect.Left - m_resizingColumnScrollOffset;
					return;
                }
                if ((int)(info.HitType & HitInfo.eHitType.kColumnHeader) > 0 && m_viewSetting.UserRearrangeableColumns)
                {
                    m_movingColumn = info.Column;
                    return;
                }
			}
			base.OnMouseDown(e);
		}
		protected override void OnMouseUp(MouseEventArgs e)
		{
			if (m_resizingColumn != null)
			{
				m_resizingColumn = null;
				Columns.RecalcVisibleColumsRect();
				UpdateScrollBars();
				Invalidate();
				if (AfterResizingColumn != null)
					AfterResizingColumn(this, e);
            }
            if (m_movingColumn != null)
            {
                m_movingColumn.Moving = false;
                m_movingColumn = null;
                Cursor = Cursors.Arrow;
                Columns.RecalcVisibleColumsRect();
                UpdateScrollBars();
                Invalidate();
            }
			base.OnMouseUp(e);
		}
		protected override void OnMouseDoubleClick(MouseEventArgs e)
		{
			base.OnMouseDoubleClick(e);
			Point mousePoint = new Point(e.X, e.Y);
			Node clickedNode = CalcHitNode(mousePoint);
			if (clickedNode != null && clickedNode.HasChildren)
				clickedNode.Expanded = !clickedNode.Expanded;
            if (clickedNode != null)
                OnNodeDoubleClicked(clickedNode);
		}

		// Somewhere I read that it could be risky to do any handling in GetFocus / LostFocus. 
		// The reason is that it will throw exception incase you make a call which recreates the windows handle (e.g. 
		// change the border style. Instead one should always use OnEnter and OnLeave instead. That is why I'm using
		// OnEnter and OnLeave instead, even though I'm only doing Invalidate.
		protected override void OnEnter(EventArgs e)
		{
			base.OnEnter(e);
			Invalidate();
		}
		protected override void OnLeave(EventArgs e)
		{
			m_tooltipNode = null;
			m_tooltip.Hide(this);
			m_tooltipVisible = false;
			m_tooltipTimer.Stop();
			base.OnLeave(e);
			Invalidate();
		}

        protected override void OnLostFocus(EventArgs e)
        {
            m_tooltipNode = null;
            m_tooltip.Hide(this);
            m_tooltipVisible = false;
            m_tooltipTimer.Stop();
            base.OnLostFocus(e);
            Invalidate();
        }

		void SetHotColumn(TreeListColumn col, bool ishot)
		{
			int scrolloffset = HScrollValue();
			if (col != m_hotColumn)
			{
				if (m_hotColumn != null)
				{
					m_hotColumn.ishot = false;
					Rectangle r = m_hotColumn.CalculatedRect;
					r.X -= scrolloffset;
					Invalidate(r);
				}
				m_hotColumn = col;
				if (m_hotColumn != null)
				{
					m_hotColumn.ishot = ishot;
					Rectangle r = m_hotColumn.CalculatedRect;
					r.X -= scrolloffset;
					Invalidate(r);
				}
			}
		}
		internal int RowHeaderWidth()
		{
			if (RowOptions.ShowHeader)
				return RowOptions.HeaderWidth;
			return 0;
		}
		int MinWidth()
		{
			return RowHeaderWidth() + Columns.ColumnsWidth;
		}
		int MaxVisibleRows(out int remainder)
		{
			remainder = 0;
			if (ClientRectangle.Height < 0)
				return 0;
			int height = ClientRectangle.Height - Columns.Options.HeaderHeight;
			//return (int) Math.Ceiling((double)(ClientRectangle.Height - Columns.HeaderHeight) / (double)Nodes.ItemHeight); 
			remainder = (ClientRectangle.Height - Columns.Options.HeaderHeight) % RowOptions.ItemHeight ;
			return Math.Max(0, (ClientRectangle.Height - Columns.Options.HeaderHeight) / RowOptions.ItemHeight);
		}
		int MaxVisibleRows()
		{
			int unused;
			return MaxVisibleRows(out unused);
		}
		public void BeginUpdate()
		{
			m_nodes.BeginUpdate();
		}
		public void EndUpdate()
		{
			m_nodes.EndUpdate();
			RecalcLayout();
            Invalidate();
		}
		protected override CreateParams CreateParams
		{
			get
			{
                const int WS_BORDER = 0x00800000;
                const int WS_EX_CLIENTEDGE = 0x00000200;
				CreateParams p = base.CreateParams;
                p.Style &= ~(int)WS_BORDER;
                p.ExStyle &= ~(int)WS_EX_CLIENTEDGE;
				switch (ViewOptions.BorderStyle)
				{
					case BorderStyle.Fixed3D:
                        p.ExStyle |= (int)WS_EX_CLIENTEDGE;
						break;
					case BorderStyle.FixedSingle:
                        p.Style |= (int)WS_BORDER;
						break;
					default:
						break;
				}
				return p;
			}
		}
		
		TreeListColumn m_hotColumn = null;

		object GetDataDesignMode(Node node, TreeListColumn column)
		{
			string id = string.Empty;
			while (node != null)
			{
				id = node.Owner.GetNodeIndex(node).ToString() + ":" + id;
				node = node.Parent;
			}
			return "<temp>" + id;
		}
		protected virtual object GetData(Node node, TreeListColumn column)
		{
			if (node[column.Index] != null)
				return node[column.Index];
			return null;
		}
		public new Rectangle ClientRectangle
		{
			get
			{
				Rectangle r = base.ClientRectangle;
				if (m_vScroll.Visible)
					r.Width -= m_vScroll.Width+1;
				if (m_hScroll.Visible)
					r.Height -= m_hScroll.Height+1;
				return r;
			}
		}

		protected virtual TreelistView.TreeList.TextFormatting GetFormatting(TreelistView.Node node, TreelistView.TreeListColumn column)
		{
			return column.CellFormat;
		}
        protected virtual void PaintCellPlusMinus(Graphics dc, Rectangle glyphRect, Node node, TreeListColumn column)
        {
            CellPainter.PaintCellPlusMinus(dc, glyphRect, node, column, GetFormatting(node, column));
        }
		protected virtual void PaintCellBackground(Graphics dc, Rectangle cellRect, Node node, TreeListColumn column)
		{
			if (this.DesignMode)
                CellPainter.PaintCellBackground(dc, cellRect, node, column, GetFormatting(node, column), GetDataDesignMode(node, column));
			else
                CellPainter.PaintCellBackground(dc, cellRect, node, column, GetFormatting(node, column), GetData(node, column));
		}
        protected virtual void PaintCellText(Graphics dc, Rectangle cellRect, Node node, TreeListColumn column)
        {
            if (this.DesignMode)
                CellPainter.PaintCellText(dc, cellRect, node, column, GetFormatting(node, column), GetDataDesignMode(node, column));
            else
                CellPainter.PaintCellText(dc, cellRect, node, column, GetFormatting(node, column), GetData(node, column));
        }
		protected virtual void PaintImage(Graphics dc, Rectangle imageRect, Node node, Image image)
		{
			if (image != null)
				dc.DrawImage(image, imageRect.X, imageRect.Y, imageRect.Width, imageRect.Height);
		}
		protected virtual void PaintNode(Graphics dc, Rectangle rowRect, Node node, TreeListColumn[] visibleColumns, int visibleRowIndex)
		{
			CellPainter.DrawSelectionBackground(dc, rowRect, node);
			foreach (TreeListColumn col in visibleColumns)
			{
				if (col.CalculatedRect.Right - HScrollValue() < RowHeaderWidth())
					continue;

				Rectangle cellRect = rowRect;
				cellRect.X = col.CalculatedRect.X - HScrollValue();
				cellRect.Width = col.CalculatedRect.Width;

                dc.SetClip(cellRect);

                if (col.Index == GetTreeColumn(node))
				{
					int lineindet = 10;
					// add left margin
					cellRect.X += Columns.Options.LeftMargin;
					cellRect.Width -= Columns.Options.LeftMargin;

					// add indent size
					int indentSize = GetIndentSize(node) + 5;
					cellRect.X += indentSize;
					cellRect.Width -= indentSize;

                    // save rectangle for line drawing below
                    Rectangle lineCellRect = cellRect;

					cellRect.X += lineindet;
					cellRect.Width -= lineindet;

                    Rectangle glyphRect = GetPlusMinusRectangle(node, col, visibleRowIndex);
                    Rectangle plusminusRect = glyphRect;

					if (!ViewOptions.ShowLine && (!ViewOptions.ShowPlusMinus || (!ViewOptions.PadForPlusMinus && plusminusRect == Rectangle.Empty)))
					{
						cellRect.X -= (lineindet + 5);
						cellRect.Width += (lineindet + 5);
					}

                    Point mousePoint = PointToClient(Cursor.Position);
                    Node hoverNode = CalcHitNode(mousePoint);

					Image icon = hoverNode != null && hoverNode == node ? GetHoverNodeBitmap(node) : GetNodeBitmap(node);

                    PaintCellBackground(dc, cellRect, node, col);

                    if (ViewOptions.ShowLine)
                        PaintLines(dc, lineCellRect, node);

                    if (SelectedImage != null && (NodesSelection.Contains(node) || FocusedNode == node))
                    {
                        // center the image vertically
                        glyphRect.Y = cellRect.Y + (cellRect.Height / 2) - (SelectedImage.Height / 2);
                        glyphRect.X = cellRect.X;
                        glyphRect.Width = SelectedImage.Width;
                        glyphRect.Height = SelectedImage.Height;

                        PaintImage(dc, glyphRect, node, SelectedImage);
                        cellRect.X += (glyphRect.Width + 2);
                        cellRect.Width -= (glyphRect.Width + 2);
                    }

					if (icon != null)
					{
						// center the image vertically
						glyphRect.Y = cellRect.Y + (cellRect.Height / 2) - (icon.Height / 2);
						glyphRect.X = cellRect.X;
						glyphRect.Width = icon.Width;
						glyphRect.Height = icon.Height;

						PaintImage(dc, glyphRect, node, icon);
						cellRect.X += (glyphRect.Width + 2);
						cellRect.Width -= (glyphRect.Width + 2);
					}

					PaintCellText(dc, cellRect, node, col);

                    if (plusminusRect != Rectangle.Empty && ViewOptions.ShowPlusMinus)
                        PaintCellPlusMinus(dc, plusminusRect, node, col);
				}
				else
				{
                    PaintCellBackground(dc, cellRect, node, col);
                    PaintCellText(dc, cellRect, node, col);
                }

                dc.ResetClip();
			}
		}
		protected virtual void PaintLines(Graphics dc, Rectangle cellRect, Node node)
		{
			Pen pen = new Pen(Color.Gray);
			pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;

			int halfPoint = cellRect.Top + (cellRect.Height / 2);
			// line should start from center at first root node 
			if (node.Parent == null && node.PrevSibling == null)
			{
				cellRect.Y += (cellRect.Height / 2);
				cellRect.Height -= (cellRect.Height / 2);
			}
			if (node.NextSibling != null || node.HasChildren)	// draw full height line
				dc.DrawLine(pen, cellRect.X, cellRect.Top, cellRect.X, cellRect.Bottom);
			else
				dc.DrawLine(pen, cellRect.X, cellRect.Top, cellRect.X, halfPoint);
			dc.DrawLine(pen, cellRect.X, halfPoint, cellRect.X + 10, halfPoint);

			// now draw the lines for the parents sibling
			Node parent = node.Parent;
			while (parent != null)
			{
                Pen linePen = null;
                if (parent.TreeLineColor != Color.Transparent || parent.TreeLineWidth > 0.0f)
                    linePen = new Pen(parent.TreeLineColor, parent.TreeLineWidth);

				cellRect.X -= ViewOptions.Indent;
                dc.DrawLine(linePen != null ? linePen : pen, cellRect.X, cellRect.Top, cellRect.X, cellRect.Bottom);
				parent = parent.Parent;

                if (linePen != null)
                    linePen.Dispose();
			}

			pen.Dispose();
		}
		protected virtual int GetIndentSize(Node node)
		{
			int indent = 0;
			Node parent = node.Parent;
			while (parent != null)
			{
				indent += ViewOptions.Indent;
				parent = parent.Parent;
			}
			return indent;
		}
		protected virtual Rectangle GetPlusMinusRectangle(Node node, TreeListColumn firstColumn, int visibleRowIndex)
		{
			if (node.HasChildren == false)
				return Rectangle.Empty;
			int hScrollOffset = HScrollValue();
			if (firstColumn.CalculatedRect.Right - hScrollOffset < RowHeaderWidth())
				return Rectangle.Empty;
			//System.Diagnostics.Debug.Assert(firstColumn.VisibleIndex == 0);

			Rectangle glyphRect = firstColumn.CalculatedRect;
			glyphRect.X -= hScrollOffset;
			glyphRect.X += GetIndentSize(node);
			glyphRect.X += Columns.Options.LeftMargin;
			glyphRect.Width = 10;
			glyphRect.Y = VisibleRowToYPoint(visibleRowIndex);
			glyphRect.Height = RowOptions.ItemHeight;
			return glyphRect;
		}
		protected virtual Image GetNodeBitmap(Node node)
        {
            if (node != null)
                return node.Image;
            return null;
		}
        protected virtual Image GetHoverNodeBitmap(Node node)
        {
            if (node != null)
                return node.HoverImage;
            return null;
        }
		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);
			
			int hScrollOffset = HScrollValue();
			int remainder = 0;
			int visiblerows = MaxVisibleRows(out remainder);
			if (remainder > 0)
				visiblerows++;
			
			bool drawColumnHeaders = true;
			// draw columns
			if (drawColumnHeaders)
			{
				Rectangle headerRect = e.ClipRectangle;
				Columns.Draw(e.Graphics, headerRect, hScrollOffset);
			}

			int visibleRowIndex = 0;
			TreeListColumn[] visibleColumns = this.Columns.VisibleColumns;
			int columnsWidth = Columns.ColumnsWidth;
			foreach (Node node in NodeCollection.ForwardNodeIterator(m_firstVisibleNode, true))
			{
				Rectangle rowRect = CalcRowRectangle(visibleRowIndex);
				if (rowRect == Rectangle.Empty || rowRect.Bottom <= e.ClipRectangle.Top || rowRect.Top >= e.ClipRectangle.Bottom)
				{
					if (visibleRowIndex > visiblerows)
						break;
					visibleRowIndex++;
					continue;
				}
				rowRect.X = RowHeaderWidth() - hScrollOffset;
				rowRect.Width = columnsWidth;

				// draw the current node
                PaintNode(e.Graphics, rowRect, node, visibleColumns, visibleRowIndex);
				
				// drow row header for current node
				Rectangle headerRect = rowRect;
				headerRect.X = 0;
				headerRect.Width = RowHeaderWidth();

				int absoluteRowIndex = visibleRowIndex + VScrollValue();
				headerRect.Width = RowHeaderWidth();
                m_rowPainter.DrawHeader(e.Graphics, headerRect, absoluteRowIndex == m_hotrow);
				
				visibleRowIndex++;
            }

            visibleRowIndex = 0;
            foreach (Node node in NodeCollection.ForwardNodeIterator(m_firstVisibleNode, true))
            {
                Rectangle rowRect = CalcRowRectangle(visibleRowIndex);
                // draw horizontal grid line for current node
                if (ViewOptions.ShowGridLines)
                {
                    Rectangle r = rowRect;
                    r.X = RowHeaderWidth();
                    r.Width = columnsWidth - hScrollOffset;
                    m_rowPainter.DrawHorizontalGridLine(e.Graphics, r, GridLineColour);
                }

                visibleRowIndex++;
            }

            // draw vertical grid lines
            if (ViewOptions.ShowGridLines)
            {
                // visible row count
                int remainRows = Nodes.VisibleNodeCount - m_vScroll.Value;
                if (visiblerows > remainRows)
                    visiblerows = remainRows;

                Rectangle fullRect = ClientRectangle;
                if (drawColumnHeaders)
                    fullRect.Y += Columns.Options.HeaderHeight;
                fullRect.Height = visiblerows * RowOptions.ItemHeight;
                Columns.Painter.DrawVerticalGridLines(Columns, e.Graphics, fullRect, hScrollOffset);
            }
		}

		protected override bool IsInputKey(Keys keyData)
		{
			if ((int)(keyData & Keys.Shift) > 0)
				return true;
			switch (keyData)
			{
				case Keys.Left:
				case Keys.Right:
				case Keys.Down:
				case Keys.Up:
				case Keys.PageUp:
				case Keys.PageDown:
				case Keys.Home:
				case Keys.End:
					return true;
			}
			return false;
		}
		protected override void OnKeyDown(KeyEventArgs e)
		{
			Node newnode = null;
			if (e.KeyCode == Keys.PageUp)
			{
				int remainder = 0;
				int diff = MaxVisibleRows(out remainder)-1;
				newnode = NodeCollection.GetNextNode(FocusedNode, -diff);
				if (newnode == null)
					newnode = Nodes.FirstVisibleNode();
			}
			if (e.KeyCode == Keys.PageDown)
			{
				int remainder = 0;
				int diff = MaxVisibleRows(out remainder)-1;
				newnode = NodeCollection.GetNextNode(FocusedNode, diff);
				if (newnode == null)
					newnode = Nodes.LastVisibleNode(true);
			}

			if (e.KeyCode == Keys.Down)
			{
				newnode = NodeCollection.GetNextNode(FocusedNode, 1);
			}
			if (e.KeyCode == Keys.Up)
			{
				newnode = NodeCollection.GetNextNode(FocusedNode, -1);
			}
			if (e.KeyCode == Keys.Home)
			{
				newnode = Nodes.FirstNode;
			}
			if (e.KeyCode == Keys.End)
			{
				newnode = Nodes.LastVisibleNode(true);
			}
			if (e.KeyCode == Keys.Left)
			{
				if (FocusedNode != null)
				{
					if (FocusedNode.Expanded)
					{
						FocusedNode.Collapse();
						EnsureVisible(FocusedNode);
						return;
					}
					if (FocusedNode.Parent != null)
					{
						FocusedNode = FocusedNode.Parent;
						EnsureVisible(FocusedNode);
					}
				}
			}
			if (e.KeyCode == Keys.Right)
			{
				if (FocusedNode != null)
				{
					if (FocusedNode.Expanded == false && FocusedNode.HasChildren)
					{
						FocusedNode.Expand();
						EnsureVisible(FocusedNode);
						return;
					}
					if (FocusedNode.Expanded == true && FocusedNode.HasChildren)
					{
						FocusedNode = FocusedNode.Nodes.FirstNode;
						EnsureVisible(FocusedNode);
					}
				}
			}
			if (newnode != null)
			{
				if (MultiSelect)
				{
					// tree behavior is 
					// keys none,		the selected node is added as the focused and selected node
					// keys control,	only focused node is moved, the selected nodes collection is not modified
					// keys shift,		selection from first selected node to current node is done
					if (Control.ModifierKeys == Keys.Control)
						FocusedNode = newnode;
					else
						MultiSelectAdd(newnode, Control.ModifierKeys);
				}
				else
					FocusedNode = newnode;
				EnsureVisible(FocusedNode);
			}
			base.OnKeyDown(e);
		}

		internal void internalUpdateStyles()
		{
			base.UpdateStyles();
		}

		#region ISupportInitialize Members

		public void BeginInit()
		{
			Columns.BeginInit();
		}
		public void EndInit()
		{
			Columns.EndInit();
		}

		#endregion
		internal new bool DesignMode
		{
			get { return base.DesignMode; }
		}
	}

	public class TreeListViewNodes : NodeCollection
	{
		TreeListView m_tree;
		bool m_isUpdating = false;
		public void BeginUpdate()
		{
			m_isUpdating = true;
		}
		public void EndUpdate()
		{
			m_isUpdating = false;
		}
		public TreeListViewNodes(TreeListView owner) : base(null)
		{
			m_tree = owner;
            OwnerView = owner;
		}
		protected override void UpdateNodeCount(int oldvalue, int newvalue)
		{
			base.UpdateNodeCount(oldvalue, newvalue);
			if (!m_isUpdating)
				m_tree.RecalcLayout();
		}
		public override void Clear()
		{
			base.Clear();
			m_tree.RecalcLayout();
		}
		public override void NodetifyBeforeExpand(Node nodeToExpand, bool expanding)
		{
			if (!m_tree.DesignMode)
				m_tree.OnNotifyBeforeExpand(nodeToExpand, expanding);
		}
		public override void NodetifyAfterExpand(Node nodeToExpand, bool expanded)
		{
			m_tree.OnNotifyAfterExpand(nodeToExpand, expanded);
		}
		protected override int GetFieldIndex(string fieldname)
		{
			TreeListColumn col = m_tree.Columns[fieldname];
			if (col != null)
				return col.Index;
			return -1;
		}
	}
}
