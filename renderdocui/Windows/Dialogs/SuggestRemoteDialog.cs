using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using renderdocui.Code;

namespace renderdocui.Windows.Dialogs
{
    public partial class SuggestRemoteDialog : Form
    {
        public enum SuggestRemoteResult
        {
            Cancel,
            Local,
            Remote,
        }

        string m_WarningStart;

        public SuggestRemoteDialog(string driver, string machineIdent)
        {
            InitializeComponent();

            icon.Image = SystemIcons.Exclamation.ToBitmap();

            m_WarningStart =
                "This " + driver + " capture was originally created on a\n" +
                "'" + machineIdent.Trim() + "' machine.\n\n";

            warning.Text =
                m_WarningStart +
                "Currently you have no remote context selected or configured\n" +
                "to replay on. Would you like to load the capture locally or\n" +
                "back out to configure one in Tools > Manage Remote Servers?";

            remote.Enabled = false;
            remote.Image = null;
            remote.Text = "No Remote";
        }

        private void remoteDropDown_ItemAdded(object sender, ToolStripItemEventArgs e)
        {
            // update text and buttons to reflect that remote hosts are configured
            warning.Text =
                m_WarningStart +
                "Currently you have no remote context selected, would you like\n" +
                "to choose a remote context to replay on, or continue and load\n" +
                "the capture locally?";

            remote.Enabled = true;
            remote.Image = global::renderdocui.Properties.Resources.down_arrow;
            remote.Text = "Remote    ";
        }

        private void alwaysLocal_CheckedChanged(object sender, EventArgs e)
        {
            remote.Enabled = RemoteItems.Count > 0 && !alwaysLocal.Checked;
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == Keys.Escape)
            {
                this.Close();
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        private void cancel_Click(object sender, EventArgs e)
        {
            m_Result = SuggestRemoteResult.Cancel;
            Close();
        }

        private void local_Click(object sender, EventArgs e)
        {
            m_Result = SuggestRemoteResult.Local;
            Close();
        }

        private void remoteDropDown_ItemClicked(object sender, ToolStripItemClickedEventArgs e)
        {
            m_Result = SuggestRemoteResult.Remote;
        }

        private void remote_Click(object sender, EventArgs e)
        {
            if(remote.Checked)
                return;

            Point showPos = remote.PointToScreen(remote.ClientRectangle.Location);

            showPos.Y = showPos.Y + remote.ClientRectangle.Height;

            // gives the appearance that the button stays pressed
            remote.Checked = true;

            remoteDropDown.Show(showPos);
        }

        private void remoteDropDown_Closed(object sender, ToolStripDropDownClosedEventArgs e)
        {
            remote.Checked = false;

            // if the result is Remote now then one of the items was clicked on
            if (m_Result == SuggestRemoteResult.Remote)
                Close();
        }

        private SuggestRemoteResult m_Result = SuggestRemoteResult.Cancel;
        public SuggestRemoteResult Result
        {
            get
            {
                return m_Result;
            }
        }

        public ToolStripItemCollection RemoteItems
        {
            get
            {
                return remoteDropDown.Items;
            }
        }

        public bool AlwaysReplayLocally
        {
            get
            {
                return alwaysLocal.Checked;
            }
        }
    }
}
