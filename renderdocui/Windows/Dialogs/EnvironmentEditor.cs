using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdoc;

namespace renderdocui.Windows.Dialogs
{
    public partial class EnvironmentEditor : Form
    {
        public EnvironmentEditor()
        {
            InitializeComponent();

            pendSeparator.Items.AddRange(new object[] {
                EnvironmentSeparator.Platform.Str(),
                EnvironmentSeparator.SemiColon.Str(),
                EnvironmentSeparator.Colon.Str(),
                EnvironmentSeparator.None.Str()
            });
        }

        private void EnvironmentEditor_Load(object sender, EventArgs e)
        {
            pendSeparator.SelectedIndex = (int)EnvironmentSeparator.Platform;

            setType.Checked = true;
            varName.Select();
            varName_TextChanged(varName, new EventArgs());

            variables.NodesSelection.Clear();
        }

        private int ExistingIndex()
        {
            int i=0;
            foreach (var n in variables.Nodes)
            {
                if ((string)n[0] == varName.Text)
                    return i;

                i++;
            }

            return -1;
        }

        private void modification_CheckedChanged(object sender, EventArgs e)
        {
            pendSeparator.Enabled = !setType.Checked;
        }

        private void varName_TextChanged(object sender, EventArgs e)
        {
            int idx = ExistingIndex();

            if (idx >= 0)
            {
                addUpdate.Text = "Update";
                delete.Enabled = true;
                variables.NodesSelection.Clear();
                variables.NodesSelection.Add(variables.Nodes[idx]);
            }
            else
            {
                addUpdate.Text = "Add";
                delete.Enabled = false;

                addUpdate.Enabled = (varName.Text.Trim() != "");
            }
        }

        private void varField_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\n' || e.KeyChar == '\r')
            {
                addUpdate.PerformClick();
            }
        }

        private void variables_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (variables.SelectedNode != null)
            {
                EnvironmentModification mod = variables.SelectedNode.Tag as EnvironmentModification;

                if (mod != null)
                {
                    varName.Text = mod.variable;
                    varValue.Text = mod.value;
                    pendSeparator.SelectedIndex = (int)mod.separator;

                    if (mod.type == EnvironmentModificationType.Set)
                        setType.PerformClick();
                    else if (mod.type == EnvironmentModificationType.Append)
                        appendType.PerformClick();
                    else if (mod.type == EnvironmentModificationType.Prepend)
                        prependType.PerformClick();
                }
            }
        }

        private void delete_Click(object sender, EventArgs e)
        {
            if (variables.SelectedNode != null)
            {
                variables.BeginUpdate();
                variables.Nodes.Remove(variables.SelectedNode);
                variables.EndUpdate();
                variables.NodesSelection.Clear();

                string text = varName.Text;
                varName.Text = "";
                varName.Text = text;
            }
        }

        private void addUpdate_Click(object sender, EventArgs e)
        {
            EnvironmentModification mod = new EnvironmentModification();
            mod.variable = varName.Text;
            mod.value = varValue.Text;
            mod.separator = (EnvironmentSeparator)pendSeparator.SelectedIndex;

            if (appendType.Checked)
                mod.type = EnvironmentModificationType.Append;
            else if (prependType.Checked)
                mod.type = EnvironmentModificationType.Prepend;
            else
                mod.type = EnvironmentModificationType.Set;

            AddModification(mod, false);

            varName.Text = "";
            varName.Text = mod.variable;
        }

        public void AddModification(EnvironmentModification mod, bool silent)
        {
            TreelistView.Node node = new TreelistView.Node();
            int idx = ExistingIndex();
            if (idx >= 0)
                node = variables.Nodes[idx];

            if (mod.variable.Trim() == "")
            {
                if(!silent)
                    MessageBox.Show("Environment variable cannot be just whitespace", "Invalid variable", MessageBoxButtons.OK, MessageBoxIcon.Error);

                return;
            }

            variables.BeginUpdate();

            if (idx < 0)
                variables.Nodes.Add(node);

            node.SetData(new object[] { mod.variable, mod.GetTypeString(), mod.value });
            node.Tag = mod;

            variables.EndUpdate();

            variables.NodesSelection.Clear();
            variables.NodesSelection.Add(node);

            varName.AutoCompleteCustomSource.Clear();
            for (int i = 0; i < variables.Nodes.Count; i++)
                varName.AutoCompleteCustomSource.Add((string)variables.Nodes[i][0]);
        }

        public EnvironmentModification[] Modifications
        {
            get
            {
                EnvironmentModification[] ret = new EnvironmentModification[variables.Nodes.Count];
                for(int i=0; i < variables.Nodes.Count; i++)
                    ret[i] = variables.Nodes[i].Tag as EnvironmentModification;
                return ret;
            }
        }

        private void EnvironmentEditor_FormClosing(object sender, FormClosingEventArgs e)
        {
            int idx = ExistingIndex();

            if(idx >= 0)
                return;

            DialogResult res = MessageBox.Show(
                "You did not add the variable modification you were editing. Add it now?",
                "Variable not added", MessageBoxButtons.YesNoCancel);

            if (res == DialogResult.Yes)
            {
                addUpdate_Click(addUpdate, new EventArgs());
            }
            else if (res == DialogResult.Cancel)
            {
                e.Cancel = true;
            }
        }

        private void variables_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Delete && delete.Enabled)
                delete.PerformClick();
        }
    }
}
