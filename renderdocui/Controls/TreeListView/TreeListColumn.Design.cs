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
using System.Windows.Forms.Design;

// Taken from http://www.codeproject.com/Articles/23746/TreeView-with-Columns with minor tweaks
// and fixes for my purposes.
namespace TreelistView
{
	// http://msdn2.microsoft.com/en-us/library/9zky1t4k.aspx

	// Extending Design-Time Support
	// ms-help://MS.VSCC.v80/MS.MSDN.v80/MS.VisualStudio.v80.en/dv_fxdeveloping/html/d6ac8a6a-42fd-4bc8-bf33-b212811297e2.htm
	// http://msdn2.microsoft.com/en-us/library/37899azc.aspx

	// description of the property grid
	// http://msdn2.microsoft.com/en-us/library/aa302326.aspx
	// http://msdn2.microsoft.com/en-us/library/aa302334.aspx

	// another good one explaining 
	// http://www.codeproject.com/KB/cs/dzcollectioneditor.aspx?print=true
    public class ColumnCollectionEditor : CollectionEditor
	{
		public ColumnCollectionEditor(Type type) : base(type)
		{
		}
		protected override bool CanSelectMultipleInstances()
		{
			return false;
		}
		protected override Type CreateCollectionItemType()
		{
			return base.CreateCollectionItemType();
		}
		protected override object CreateInstance(Type itemType)
		{
			TreeListView owner = this.Context.Instance as TreeListView;
			// create new default fieldname
			string fieldname;
			string caption;
			int cnt = owner.Columns.Count;
			do
			{
				fieldname = "fieldname" + cnt.ToString();
				caption = "Column_" + cnt.ToString();
				cnt++;
			}
			while (owner.Columns[fieldname] != null);
			return new TreeListColumn(fieldname, caption);
		}
		protected override string GetDisplayText(object value)
        {
            string Caption = (string)value.GetType().GetProperty("Caption").GetGetMethod().Invoke(value, null);
            string Fieldname = (string)value.GetType().GetProperty("Fieldname").GetGetMethod().Invoke(value, null);

            if (Caption.Length > 0)
                return string.Format("{0} ({1})", Caption, Fieldname);
			return base.GetDisplayText(value);
		}
		public override object EditValue(ITypeDescriptorContext context, IServiceProvider provider, object value)
		{
			object result = base.EditValue(context, provider, value);
			TreeListView owner = this.Context.Instance as TreeListView;
			owner.Invalidate();
			return result;
		}
	}

	internal class ColumnConverter : ExpandableObjectConverter
	{
		public override bool CanConvertTo(ITypeDescriptorContext context, Type destType)
        {
            if (destType == typeof(InstanceDescriptor) || destType == typeof(string))
                return true;
            else
                return base.CanConvertTo(context, destType);
		}
		public override object ConvertTo(ITypeDescriptorContext context, System.Globalization.CultureInfo info, object value, Type destType)
		{
            if (destType == typeof(string))
            {
                string Caption = (string)value.GetType().GetProperty("Caption").GetGetMethod().Invoke(value, null);
                string Fieldname = (string)value.GetType().GetProperty("Fieldname").GetGetMethod().Invoke(value, null);

                return String.Format("{0}, {1}", Caption, Fieldname);
            }
            if (destType == typeof(InstanceDescriptor))
			{
                ConstructorInfo cinfo = typeof(TreeListColumn).GetConstructor(new Type[] { typeof(string), typeof(string) });
				
                string Caption = (string)value.GetType().GetProperty("Caption").GetGetMethod().Invoke(value, null);
                string Fieldname = (string)value.GetType().GetProperty("Fieldname").GetGetMethod().Invoke(value, null);

				return new InstanceDescriptor(cinfo, new object[] {Fieldname, Caption}, false);
            }
			return base.ConvertTo(context, info, value, destType);
		}
	}
	class ColumnsTypeConverter : ExpandableObjectConverter
	{
		public override bool CanConvertTo(ITypeDescriptorContext context, Type destinationType)
		{
			if (destinationType == typeof(TreeListColumnCollection))
				return true;
			return base.CanConvertTo(context, destinationType);
		}
		public override object ConvertTo(ITypeDescriptorContext context, System.Globalization.CultureInfo culture, object value, Type destinationType)
		{
			if (destinationType == typeof(string))
				return "(Columns Collection)";
			return base.ConvertTo(context, culture, value, destinationType);
		}
	}

	/// <summary>
	/// Designer for the tree view control.
	/// </summary>
	class TreeListViewDesigner : ControlDesigner
	{
		IComponentChangeService onChangeService;
		public override void Initialize(IComponent component)
		{
			base.Initialize(component);
			
			onChangeService = (IComponentChangeService)GetService(typeof(IComponentChangeService));
			if (onChangeService != null)
				onChangeService.ComponentChanged += new ComponentChangedEventHandler(OnComponentChanged);
			
			// we need to be notified when columsn have been resized.
			TreeListView tree = Control as TreeListView;
			tree.AfterResizingColumn += new MouseEventHandler(OnAfterResizingColumn);
		}
		void OnAfterResizingColumn(object sender, MouseEventArgs e)
		{
			// This is to notify that component has changed. 
			// This is causing the code InitializeComponent code to be updated
			RaiseComponentChanged(null, null, null);
		}
		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);
		}
		void OnComponentChanged(object sender, ComponentChangedEventArgs e)
		{
			// repaint the control when any properties have changed
            if(Control != null)
                Control.Invalidate();
		}
		protected override bool GetHitTest(Point point)
		{
			// if mouse is over node, columns or scrollbar then return true
			// which will cause the mouse event to be forwarded to the control
			TreeListView tree = Control as TreeListView;
			point = tree.PointToClient(point);

			Node node = tree.CalcHitNode(point);
			if (node != null)
				return true;

			TreelistView.HitInfo colinfo = tree.CalcColumnHit(point);
			if ((int)(colinfo.HitType & HitInfo.eHitType.kColumnHeader) > 0)
				return true;

			if (tree.HitTestScrollbar(point))
				return true;
			return base.GetHitTest(point);
		}

		protected override void PostFilterProperties(IDictionary properties)
		{
			//properties.Remove("Cursor");
			base.PostFilterProperties(properties);
		}
	}
}
