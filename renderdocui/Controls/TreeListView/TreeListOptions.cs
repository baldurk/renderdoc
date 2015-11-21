using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel;
using System.ComponentModel.Design;
using System.ComponentModel.Design.Serialization;
using System.Drawing;
using System.Windows.Forms;

// Taken from http://www.codeproject.com/Articles/23746/TreeView-with-Columns with minor tweaks
// and fixes for my purposes.
namespace TreelistView.TreeList
{
	[TypeConverterAttribute(typeof(OptionsSettingTypeConverter))]
	public class TextFormatting
	{
		ContentAlignment	m_alignment = ContentAlignment.MiddleLeft;
		Color m_foreColor = SystemColors.ControlText;
		Color m_backColor = Color.Transparent;
		Padding	m_padding = new Padding(0,0,0,0);
		public TextFormatFlags GetFormattingFlags()
		{
			TextFormatFlags	flags = 0;
			switch (TextAlignment)
			{
				case ContentAlignment.TopLeft:
					flags = TextFormatFlags.Top | TextFormatFlags.Left;
					break;
				case ContentAlignment.TopCenter:
					flags = TextFormatFlags.Top | TextFormatFlags.HorizontalCenter;
					break;
				case ContentAlignment.TopRight:
					flags = TextFormatFlags.Top | TextFormatFlags.Right;
					break;
				case ContentAlignment.MiddleLeft:
					flags = TextFormatFlags.VerticalCenter | TextFormatFlags.Left;
					break;
				case ContentAlignment.MiddleCenter:
					flags = TextFormatFlags.VerticalCenter | TextFormatFlags.HorizontalCenter;
					break;
				case ContentAlignment.MiddleRight:
					flags = TextFormatFlags.VerticalCenter | TextFormatFlags.Right;
					break;
				case ContentAlignment.BottomLeft:
					flags = TextFormatFlags.Bottom | TextFormatFlags.Left;
					break;
				case ContentAlignment.BottomCenter:
					flags = TextFormatFlags.Bottom | TextFormatFlags.HorizontalCenter;
					break;
				case ContentAlignment.BottomRight:
					flags = TextFormatFlags.Bottom | TextFormatFlags.Right;
					break;
			}
			return flags;
		}
		
		[DefaultValue(typeof(Padding), "0,0,0,0")]
		public Padding Padding
		{
			get { return m_padding; }
			set { m_padding = value; }
		}
		[DefaultValue(typeof(ContentAlignment), "MiddleLeft")]
		public ContentAlignment TextAlignment
		{
			get { return m_alignment; }
			set { m_alignment = value; }
		}
		[DefaultValue(typeof(Color), "ControlText")]
		public Color ForeColor
		{
			get { return m_foreColor; }
			set { m_foreColor = value; }
		}
		[DefaultValue(typeof(Color), "Transparent")]
		public Color BackColor
		{
			get { return m_backColor; }
			set { m_backColor = value; }
		}
		public TextFormatting()
		{
		}
		public TextFormatting(TextFormatting aCopy)
		{
			m_alignment = aCopy.m_alignment;
			m_foreColor = aCopy.m_foreColor;
			m_backColor = aCopy.m_backColor;
			m_padding	= aCopy.m_padding;
		}
	}
	
	[TypeConverterAttribute(typeof(OptionsSettingTypeConverter))]
	public class ViewSetting
	{
		TreeListView			m_owner;
		BorderStyle m_borderStyle = BorderStyle.None;
		int			m_indent = 16;
		bool		m_showLine = true;
		bool		m_showPlusMinus = true;
		bool		m_padForPlusMinus = true;
		bool		m_showGridLines = true;
        bool        m_rearrangeableColumns = false;
        bool m_hoverHand = true;

		[Category("Behavior")]
		[DefaultValue(typeof(int), "16")]
		public int Indent
		{
			get { return m_indent; }
			set
			{
				m_indent = value;
				m_owner.Invalidate();
			}
		}

		[Category("Behavior")]
		[DefaultValue(typeof(bool), "True")]
		public bool ShowLine
		{
			get { return m_showLine; }
			set
			{
				m_showLine = value;
				m_owner.Invalidate();
			}
		}

		[Category("Behavior")]
		[DefaultValue(typeof(bool), "True")]
		public bool ShowPlusMinus
		{
			get { return m_showPlusMinus; }
			set
			{
				m_showPlusMinus = value;
				m_owner.Invalidate();
			}
		}

        [Category("Behavior")]
        [DefaultValue(typeof(bool), "True")]
        public bool PadForPlusMinus
        {
            get { return m_padForPlusMinus; }
            set
            {
                m_padForPlusMinus = value;
                m_owner.Invalidate();
            }
        }
		
		[Category("Behavior")]
		[DefaultValue(typeof(bool), "True")]
		public bool ShowGridLines
		{
			get { return m_showGridLines; }
			set
			{
				m_showGridLines = value;
				m_owner.Invalidate();
			}
		}

        [Category("Behavior")]
        [DefaultValue(typeof(bool), "False")]
        public bool UserRearrangeableColumns
        {
            get { return m_rearrangeableColumns; }
            set
            {
                m_rearrangeableColumns = value;
            }
        }

        [Category("Behavior")]
        [DefaultValue(typeof(bool), "True")]
        public bool HoverHandTreeColumn
        {
            get { return m_hoverHand; }
            set
            {
                m_hoverHand = value;
            }
        }

		[Category("Appearance")]
		[DefaultValue(typeof(BorderStyle), "None")]
		public BorderStyle BorderStyle
		{
			get { return m_borderStyle; }
			set
			{
				if (m_borderStyle != value)
				{
					m_borderStyle = value;
					m_owner.internalUpdateStyles();
					m_owner.Invalidate();
				}
			}
		}

		public ViewSetting(TreeListView owner)
		{
			m_owner = owner;
		}
	}
	
	[TypeConverterAttribute(typeof(OptionsSettingTypeConverter))]
	public class CollumnSetting
	{
        bool            m_FreezeWhileResizing = false;
		int				m_leftMargin = 5;
		int				m_headerHeight = 20;
		TreeListView	m_owner;

        [DefaultValue(false)]
        public bool FreezeWhileResizing
        {
            get { return m_FreezeWhileResizing; }
            set
            {
                m_FreezeWhileResizing = value;
            }
        }
		[DefaultValue(5)]
		public int LeftMargin
		{
			get { return m_leftMargin; }
			set 
			{ 
				m_leftMargin = value; 
				m_owner.Columns.RecalcVisibleColumsRect();
				m_owner.Invalidate();
			}
		}
		[DefaultValue(20)]
		public int HeaderHeight
		{
			get { return m_headerHeight; }
			set
			{ 
				m_headerHeight = value; 
				m_owner.Columns.RecalcVisibleColumsRect();
				m_owner.Invalidate();
			}
		}
		public CollumnSetting(TreeListView owner)
		{
			m_owner = owner;
		}
	}

	[TypeConverterAttribute(typeof(OptionsSettingTypeConverter))]
	public class RowSetting
	{
		TreeListView	m_owner;
		bool			m_showHeader = true;
		bool			m_hoverHighlight = false;
		int				m_headerWidth = 15;
		int				m_itemHeight = 16;
		[DefaultValue(true)]
		public bool ShowHeader
		{
			get { return m_showHeader; }
			set
			{
				if (m_showHeader == value)
					return;
				m_showHeader = value;
				m_owner.Columns.RecalcVisibleColumsRect();
				m_owner.Invalidate();
			}
		}
        [DefaultValue(false)]
        public bool HoverHighlight
        {
            get { return m_hoverHighlight; }
            set
            {
                if (m_hoverHighlight == value)
                    return;
                m_hoverHighlight = value;
                m_owner.Invalidate();
            }
        }
		[DefaultValue(15)]
		public int HeaderWidth
		{
			get { return m_headerWidth; }
			set
			{
				if (m_headerWidth == value)
					return;
				m_headerWidth = value;
				m_owner.Columns.RecalcVisibleColumsRect();
				m_owner.Invalidate();
			}
		}

		[Category("Behavior")]
		[DefaultValue(typeof(int), "16")]
		public int ItemHeight
		{
			get { return m_itemHeight; }
			set
			{
				m_itemHeight = value;
				m_owner.Invalidate();
			}
		}
		
		public RowSetting(TreeListView owner)
		{
			m_owner = owner;
		}
	}

	class OptionsSettingTypeConverter : ExpandableObjectConverter
	{
		public override bool CanConvertTo(ITypeDescriptorContext context, Type destinationType)
		{
			if (destinationType == typeof(ViewSetting))
				return true;
			if (destinationType == typeof(RowSetting))
				return true;
			if (destinationType == typeof(CollumnSetting))
				return true;
			if (destinationType == typeof(TextFormatting))
				return true;
			return base.CanConvertTo(context, destinationType);
		}
		public override object ConvertTo(ITypeDescriptorContext context, System.Globalization.CultureInfo culture, object value, Type destinationType)
		{
			if (destinationType == typeof(string) && value.GetType() == typeof(ViewSetting))
				return "(View Options)";
			if (destinationType == typeof(string) && value.GetType() == typeof(RowSetting))
				return "(Row Header Options)";
			if (destinationType == typeof(string) && value.GetType() == typeof(CollumnSetting))
				return "(Columns Options)";
			if (destinationType == typeof(string) && value.GetType() == typeof(TextFormatting))
				return "(Formatting)";
			return base.ConvertTo(context, culture, value, destinationType);
		}
	}
}
