using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Text.RegularExpressions;

namespace renderdocui.Windows.Dialogs
{
    public partial class FindAllDialog : Form
    {
        public delegate void OnFindMethod();

        private OnFindMethod m_FindCallback = null;

        public bool Regexs { get { return regexsearch.Checked; } }

        public string Search { get { return findtext.Text; } set { findtext.Text = value; findtext.Focus(); } }
        public ScintillaNET.SearchFlags FindAllOptions
        {
            get
            {
                return
                    (matchcase.Checked ? ScintillaNET.SearchFlags.MatchCase : 0) |
                    (wholeword.Checked ? ScintillaNET.SearchFlags.WholeWord : 0) |
                    (wordstart.Checked ? ScintillaNET.SearchFlags.WordStart : 0);
            }
        }

        public Regex SearchRegex
        {
            get
            {
                return new Regex(findtext.Text, regexOptions);
            }
        }
        private RegexOptions regexOptions
        {
            get
            {
                return
                    (ignorecase.Checked ? RegexOptions.IgnoreCase : 0) |
                    (multi.Checked ? RegexOptions.Multiline : 0) |
                    (explicitcap.Checked ? RegexOptions.ExplicitCapture : 0) |
                    (compiled.Checked ? RegexOptions.Compiled : 0) |
                    (singleline.Checked ? RegexOptions.Singleline : 0) |
                    (ignorewspace.Checked ? RegexOptions.IgnorePatternWhitespace : 0) |
                    (rtl.Checked ? RegexOptions.RightToLeft : 0) |
                    (ecma.Checked ? RegexOptions.ECMAScript : 0) |
                    (invariant.Checked ? RegexOptions.CultureInvariant : 0);
            }
        }

        public FindAllDialog(OnFindMethod cb)
        {
            InitializeComponent();

            m_FindCallback = cb;

            regexoptions.Location = options.Location;
            regexoptions.Visible = false;
            normalsearch.Checked = true;
            regexsearch.Checked = false;

            Icon = global::renderdocui.Properties.Resources.icon;
        }

        private void regexsearch_CheckedChanged(object sender, EventArgs e)
        {
            if (regexsearch.Checked)
            {
                options.Visible = false;
                regexoptions.Visible = true;
            }
        }

        private void normalsearch_CheckedChanged(object sender, EventArgs e)
        {
            if (normalsearch.Checked)
            {
                options.Visible = true;
                regexoptions.Visible = false;
            }
        }

        private void dofind_Click(object sender, EventArgs e)
        {
            if(!findtext.Items.Contains(findtext.Text))
                findtext.Items.Add(findtext.Text);
            m_FindCallback();
        }

        private void FindAllDialog_Shown(object sender, EventArgs e)
        {
            findtext.Focus();
        }

        private void findtext_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
                dofind_Click(null, null);
        }
    }
}
