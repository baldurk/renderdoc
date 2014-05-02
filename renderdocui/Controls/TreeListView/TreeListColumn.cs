using System;
using System.Collections;
using System.Collections.Generic;
using System.Drawing;
using System.Text;
using System.Diagnostics;

using System.ComponentModel;
using System.ComponentModel.Design;
using System.ComponentModel.Design.Serialization;
using System.Reflection;
using System.Windows.Forms;

// Taken from http://www.codeproject.com/Articles/23746/TreeView-with-Columns with minor tweaks
// and fixes for my purposes.
namespace TreelistView
{
	public class HitInfo
	{
		public enum eHitType
		{
			kColumnHeader			= 0x0001,
			kColumnHeaderResize		= 0x0002,
		}

		public eHitType			HitType = 0;
		public TreeListColumn	Column = null;
	}
	
	/// <summary>
	/// DesignTimeVisible(false) prevents the columns from showing in the component tray (bottom of screen)
	/// If the class implement IComponent it must also implement default (void) constructor and when overriding
	/// the collection editors CreateInstance the return object must be used (if implementing IComponent), the reason
	/// is that ISite is needed, and ISite is set when base.CreateInstance is called.
	/// If no default constructor then the object will not be added to the collection in the initialize.
	/// In addition if implementing IComponent then name and generatemember is shown in the property grid
	/// Columns should just be added to the collection, no need for member, so no need to implement IComponent 
	/// </summary>
	[DesignTimeVisible(false)]
	[TypeConverter(typeof(ColumnConverter))]
	public class TreeListColumn
	{
		TreeList.TextFormatting	m_headerFormat = new TreeList.TextFormatting();
		TreeList.TextFormatting	m_cellFormat = new TreeList.TextFormatting();
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeList.TextFormatting HeaderFormat
		{
			get { return m_headerFormat; }
		}
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
		public TreeList.TextFormatting CellFormat
		{
			get { return m_cellFormat; }
		}

		TreeListColumnCollection m_owner = null;
		Rectangle	m_calculatedRect;
		int			m_visibleIndex = -1;
		int			m_colIndex = -1;
		int			m_width = 50;
		string		m_fieldName = string.Empty;
		string		m_caption = string.Empty;

        bool m_Moving = false;

		internal TreeListColumnCollection Owner
		{
			get { return m_owner; }
			set { m_owner = value; }
		}
		internal Rectangle internalCalculatedRect
		{
			get { return m_calculatedRect; }
			set { m_calculatedRect = value; }
		}
		internal int internalVisibleIndex
		{
			get { return m_visibleIndex; }
			set { m_visibleIndex = value; }
		}
		internal int internalIndex
		{
			get { return m_colIndex; }
			set { m_colIndex = value; }
		}
        
		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public bool Moving
		{
			get { return m_Moving; }
			set 
			{ 
				m_Moving = value; 
			}
		}

		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public Rectangle CalculatedRect
		{
			get { return internalCalculatedRect; }
		}

		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public TreeListView TreeList
		{
			get 
			{ 
				if (Owner == null)
					return null;
				return Owner.Owner;
			}
		}
		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public Font Font
		{
			get { return m_owner.Font; }
		}
		public int Width
		{
			get { return m_width; }
			set 
			{ 
				if (m_width == value)
					return;
				m_width = value; 
				if (m_owner != null && m_owner.DesignMode)
					m_owner.RecalcVisibleColumsRect();
			}
		}
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public string Caption
		{
			get { return m_caption; }
			set 
			{ 
				m_caption = value; 
				if (m_owner != null)
					m_owner.Invalidate();
			}
		}
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public string Fieldname
		{
			get { return m_fieldName; }
			set
			{
				if (m_owner == null || m_owner.DesignMode == false)
					throw new Exception("Fieldname can only be set at design time, Use Constructor to set programatically");
				if (value.Length == 0)
					throw new Exception("empty Fieldname not value");
				if (m_owner[value] != null)
					throw new Exception("fieldname already exist in collection");
				m_fieldName = value;
			}
		}

		public TreeListColumn(string fieldName)
		{
			m_fieldName = fieldName;
		}
		public TreeListColumn(string fieldName, string caption)
		{
			m_fieldName = fieldName;
			m_caption = caption;
		}
		public TreeListColumn(string fieldName, string caption, int width)
		{
			m_fieldName = fieldName;
			m_caption = caption;
			m_width = width;
		}
		
		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public int VisibleIndex
		{
			get { return internalVisibleIndex; }
			set	
			{
				if (m_owner != null)
					m_owner.SetVisibleIndex(this, value);
			}
		}
		
		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public int Index
		{
			get { return internalIndex; }
		}
        public bool ishot = false;
		public virtual void Draw(Graphics dc, ColumnHeaderPainter painter, Rectangle r)
		{
            painter.DrawHeader(dc, r, this, this.HeaderFormat, ishot, m_Moving);
		}

		bool	m_autoSize = false;
		float	m_autoSizeRatio = 100;
		int		m_autoSizeMinSize;
		
		[DefaultValue(false)]
		public bool AutoSize
		{
			get { return m_autoSize; } 
			set { m_autoSize = value; }
		}
		[DefaultValue(100f)]
		public float AutoSizeRatio
		{
			get { return m_autoSizeRatio; } 
			set { m_autoSizeRatio = value; }
		}
		public int AutoSizeMinSize
		{
			get { return m_autoSizeMinSize; } 
			set { m_autoSizeMinSize = value; }
		}
		int m_calculatedAutoSize;
		internal int CalculatedAutoSize
		{
			get { return m_calculatedAutoSize; }
			set { m_calculatedAutoSize = value; }
		}
	}

	[Description("This is the columns collection")]
	//[TypeConverterAttribute(typeof(ColumnsTypeConverter))]
	[Editor(typeof(ColumnCollectionEditor),typeof(System.Drawing.Design.UITypeEditor))]
	public class TreeListColumnCollection : IList<TreeListColumn>, IList
	{
		ColumnHeaderPainter		m_painter;
		TreeList.CollumnSetting m_options;
		TreeListView			m_owner;
		List<TreeListColumn>	m_columns = new List<TreeListColumn>();
		List<TreeListColumn>	m_visibleCols = new List<TreeListColumn>();
		Dictionary<string, TreeListColumn>	m_columnMap = new Dictionary<string,TreeListColumn>();

		[Browsable(false)]
		public TreeList.CollumnSetting Options
		{
			get { return m_options; }
		}
		[Browsable(false)]
		public ColumnHeaderPainter Painter
		{
			get { return m_painter; }
			set { m_painter = value; }
		}
		[Browsable(false)]
		public TreeListView Owner
		{
			get { return m_owner; }
		}
		[Browsable(false)]
		public Font Font
		{
			get { return m_owner.Font; }
		}
		[Browsable(false)]
		public TreeListColumn[] VisibleColumns
		{
			get { return m_visibleCols.ToArray(); }
		}
		[Browsable(false)]
		public int ColumnsWidth
		{
			get
			{
				int width = 0;
				foreach (TreeListColumn col in m_visibleCols)
				{
					if (col.AutoSize)
						width += col.CalculatedAutoSize;
					else
						width += col.Width;
				}
				return width;
			}
		}
		public TreeListColumnCollection(TreeListView owner)
		{
			m_owner = owner;
			m_options = new TreeList.CollumnSetting(owner);
            m_painter = new ColumnHeaderPainter(owner);
		}
		public TreeListColumn this[int index]
		{
			get
			{
				return m_columns[index];
			}
			set
			{
				m_columns[index] = value;
			}
		}
		public TreeListColumn this[string fieldname]
		{
			get 
			{ 
				TreeListColumn col;
				m_columnMap.TryGetValue(fieldname, out col);
				return col;
			}
		}
		public void SetVisibleIndex(TreeListColumn col, int index)
		{
			m_visibleCols.Remove(col);
			if (index >= 0)
			{
				if (index < m_visibleCols.Count)
					m_visibleCols.Insert(index, col);
				else
					m_visibleCols.Add(col);
			}
			RecalcVisibleColumsRect();
		}
		public HitInfo CalcHitInfo(Point point, int horzOffset)
		{
			HitInfo info = new HitInfo();
			info.Column = CalcHitColumn(point, horzOffset);
			if ((info.Column != null) && (point.Y < Options.HeaderHeight))
			{
				info.HitType |= HitInfo.eHitType.kColumnHeader;
				int right = info.Column.CalculatedRect.Right - horzOffset;
				if (info.Column.AutoSize == false || info.Column.internalIndex+1 < m_columns.Count)
				{
					if (point.X >= right - 4 && point.X <= right)
						info.HitType |= HitInfo.eHitType.kColumnHeaderResize;
				}
			}
			return info;
		}
		public TreeListColumn CalcHitColumn(Point point, int horzOffset)
		{
			if (point.X < Options.LeftMargin)
				return null;
			foreach (TreeListColumn col in m_visibleCols)
			{
				int left = col.CalculatedRect.Left - horzOffset;
				int right = col.CalculatedRect.Right - horzOffset;
				if (point.X >= left && point.X <= right)
					return col;
			}
			return null;
		}
		public void RecalcVisibleColumsRect()
		{
			RecalcVisibleColumsRect(false);
		}
		public void RecalcVisibleColumsRect(bool isColumnResizing)
		{
			if (IsInitializing)
				return;
			int x = 0;//m_leftMargin;
			if (m_owner.RowOptions.ShowHeader)
				x = m_owner.RowOptions.HeaderWidth;
			int y = 0;
			int h = Options.HeaderHeight;
			int index = 0;
			foreach(TreeListColumn col in m_columns)
			{
				col.internalVisibleIndex = -1;
				col.internalIndex = index++;
			}

			// calculate size requierd by fix columns and auto adjusted columns
			// at the same time calculate total ratio value
			int widthFixedColumns = 0;
			int widthAutoSizeColumns = 0;
			float totalRatio = 0;
			foreach (TreeListColumn col in m_visibleCols)
			{
				if (col.AutoSize)
				{
					widthAutoSizeColumns += col.AutoSizeMinSize;
					totalRatio += col.AutoSizeRatio;
				}
				else
					widthFixedColumns += col.Width;
			}

			int clientWidth = m_owner.ClientRectangle.Width - m_owner.RowHeaderWidth();
			// find ratio 'unit' value
			float remainingWidth = clientWidth - (widthFixedColumns + widthAutoSizeColumns);
			float ratioUnit = 0;
			if (totalRatio > 0 && remainingWidth > 0)
				ratioUnit = remainingWidth / totalRatio;

			for (index = 0; index < m_visibleCols.Count; index++)
			{
				TreeListColumn col = m_visibleCols[index];
				int width = col.Width;
				if (col.AutoSize)
				{
					// if doing column resizing then keep adjustable columns fixed at last width
                    if (m_options.FreezeWhileResizing && isColumnResizing)
						width = col.CalculatedAutoSize;
					else
						width = Math.Max(10, col.AutoSizeMinSize + (int)Math.Round(ratioUnit * col.AutoSizeRatio - 1.0f));
					col.CalculatedAutoSize = width;
				}
				col.internalCalculatedRect = new Rectangle(x, y, width, h);
				col.internalVisibleIndex = index;
				x += width;
			}
			Invalidate();
		}
		public void Draw(Graphics dc, Rectangle rect, int horzOffset)
		{
			foreach (TreeListColumn col in m_visibleCols)
			{
				Rectangle r = col.CalculatedRect;
				r.X -= horzOffset;
				if (r.Left > rect.Right)
					break;
				col.Draw(dc, m_painter, r);
			}
			// drwa row header filler
			if (m_owner.RowOptions.ShowHeader)
			{
				Rectangle r = new Rectangle(0, 0, m_owner.RowOptions.HeaderWidth, Options.HeaderHeight);
				m_painter.DrawHeaderFiller(dc, r);
			}
		}
		public void AddRange(IEnumerable<TreeListColumn> columns)
		{
			foreach (TreeListColumn col in columns)
				Add(col);
		}
		/// <summary>
		/// AddRange(Item[]) is required for the designer.
		/// </summary>
		/// <param name="columns"></param>
		public void AddRange(TreeListColumn[] columns)
		{
			foreach (TreeListColumn col in columns)
				Add(col);
		}
		public void Add(TreeListColumn item)
		{
			bool designmode = Owner.DesignMode;
			if (!designmode)
			{
				Debug.Assert(m_columnMap.ContainsKey(item.Fieldname) == false);
				Debug.Assert(item.Owner == null, "column.Owner == null");
			}
			else
			{
				m_columns.Remove(item);
				m_visibleCols.Remove(item);
			}

			item.Owner = this;
			m_columns.Add(item);
			m_visibleCols.Add(item);
			m_columnMap[item.Fieldname] = item;
			RecalcVisibleColumsRect();
			//return item;
		}
		public void Clear()
		{
			m_columnMap.Clear();
			m_columns.Clear();
			m_visibleCols.Clear();
		}
		public bool Contains(TreeListColumn item)
		{
			return m_columns.Contains(item);
		}
		[Browsable(false)]
		public int Count
		{
			get { return m_columns.Count; }
		}
		[Browsable(false)]
		public bool IsReadOnly
		{
			get { return false; }
		}
		#region IList<TreeListColumn> Members

		public int IndexOf(TreeListColumn item)
		{
			return m_columns.IndexOf(item);
		}

		public void Insert(int index, TreeListColumn item)
		{
			m_columns.Insert(index, item);
		}

		public void RemoveAt(int index)
		{
			if (index >= 0 && index < m_columns.Count)
			{
				TreeListColumn col = m_columns[index];
				SetVisibleIndex(col, -1);
				m_columnMap.Remove(col.Fieldname);
			}
			m_columns.RemoveAt(index);
		}

		#endregion
		#region ICollection<TreeListColumn> Members


		public void CopyTo(TreeListColumn[] array, int arrayIndex)
		{
			m_columns.CopyTo(array, arrayIndex);
		}

		public bool Remove(TreeListColumn item)
		{
			SetVisibleIndex(item, -1);
			m_columnMap.Remove(item.Fieldname);
			return m_columns.Remove(item);
		}

		#endregion
		#region IEnumerable<TreeListColumn> Members

		public IEnumerator<TreeListColumn> GetEnumerator()
		{
			return m_columns.GetEnumerator();
		}

		#endregion
		#region IEnumerable Members
		IEnumerator IEnumerable.GetEnumerator()
		{
			return GetEnumerator();
		}
		#endregion
		#region IList Members
		int IList.Add(object value)
		{
			Add((TreeListColumn)value);
			return Count - 1;
		}
		bool IList.Contains(object value)
		{
			return Contains((TreeListColumn)value);
		}
		int IList.IndexOf(object value)
		{
			return IndexOf((TreeListColumn)value);
		}
		void IList.Insert(int index, object value)
		{
			Insert(index, (TreeListColumn)value);
		}

		bool IList.IsFixedSize
		{
			get { return false; }
		}

		void IList.Remove(object value)
		{
			Remove((TreeListColumn)value);
		}

		object IList.this[int index]
		{
			get
			{
				throw new Exception("The method or operation is not implemented.");
			}
			set
			{
				throw new Exception("The method or operation is not implemented.");
			}
		}
		#endregion
		#region ICollection Members

		public void CopyTo(Array array, int index)
		{
			throw new Exception("The method or operation is not implemented.");
		}

		public bool IsSynchronized
		{
			get { throw new Exception("The method or operation is not implemented."); }
		}

		public object SyncRoot
		{
			get { throw new Exception("The method or operation is not implemented."); }
		}

		#endregion

		internal bool DesignMode
		{
			get
			{
				if (m_owner != null)
					return m_owner.DesignMode;
				return false;
			}
		}
		internal void Invalidate()
		{
			if (m_owner != null)
				m_owner.Invalidate();
		}
		bool IsInitializing = false;
		internal void BeginInit()
		{
			IsInitializing = true;
		}
		internal void EndInit()
		{
			IsInitializing = false;
			RecalcVisibleColumsRect();
		}
	}
}
