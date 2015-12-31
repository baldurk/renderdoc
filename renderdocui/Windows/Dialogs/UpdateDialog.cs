using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.IO;
using System.Net;
using System.Threading;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using renderdocui.Code;
using System.Diagnostics;

namespace renderdocui.Windows.Dialogs
{
    public partial class UpdateDialog : Form
    {
        // http://www.codeproject.com/Articles/18509/Add-a-UAC-shield-to-a-button-when-elevation-is-req
        [DllImport("user32")]
        public static extern UInt32 SendMessage(IntPtr hWnd, UInt32 msg, UInt32 wParam, UInt32 lParam);

        internal const int BCM_SETSHIELD = 0x160C; // Button needs elevation

        string m_URL = "";
        int m_Size = 0;

        public UpdateDialog(Core core)
        {
            InitializeComponent();

            doupdate.FlatStyle = FlatStyle.System;
            SendMessage(doupdate.Handle, BCM_SETSHIELD, 0, 0xFFFFFFFF);

            string[] response_split = core.Config.CheckUpdate_UpdateResponse.Split('\n');

            progressText.Text = "";

            updateVer.Text = String.Format("Update Available - v{0}", response_split[0]);
            m_URL = response_split[1];
            int.TryParse(response_split[2], out m_Size);

            string notes = "";
            for(int i=3; i < response_split.Length; i++)
                notes += response_split[i] + Environment.NewLine;
            updateNotes.Text = notes.Trim();

            updateNotes.Select(0, 0);
        }

        void SetDownloadProgress(int bytes_received)
        {
            progressText.Text = "Downloading Update";
            if(m_Size > 0)
                progressBar.Value = (int)(progressBar.Maximum * ((float)bytes_received / (float)m_Size));
        }

        private void doupdate_Click(object sender, EventArgs e)
        {
            DialogResult result = MessageBox.Show("This will close RenderDoc immediately - if you have any unsaved work, save it first!\nContinue?", "RenderDoc Update", MessageBoxButtons.YesNoCancel);

            if (result == DialogResult.Yes)
            {
                progressText.Text = "Connecting";

                close.Enabled = false;
                doupdate.Enabled = false;
                
                var updateThread = Helpers.NewThread(new ThreadStart(() =>
                {
                    HttpWebRequest g = (HttpWebRequest)HttpWebRequest.Create(m_URL);
                    g.Method = "GET";

                    string dstpath = Path.Combine(Path.GetTempPath(), "RenderDocUpdate");
                    string destzip = Path.Combine(dstpath, "update.zip");
                    Directory.CreateDirectory(Path.GetDirectoryName(destzip));

                    g.BeginGetResponse(new AsyncCallback((IAsyncResult asyncres) =>
                    {
                        int recvd = 0;

                        try
                        {
                            using (HttpWebResponse resp = (HttpWebResponse)g.EndGetResponse(asyncres))
                            {
                                byte[] buffer = new byte[1024];

                                FileStream strm = File.OpenWrite(destzip);
                                using (Stream input = resp.GetResponseStream())
                                {
                                    int size = input.Read(buffer, 0, buffer.Length);
                                    while (size > 0)
                                    {
                                        strm.Write(buffer, 0, size);
                                        recvd += size;

                                        size = input.Read(buffer, 0, buffer.Length);

                                        BeginInvoke((MethodInvoker)delegate
                                        {
                                            SetDownloadProgress(recvd);
                                        });
                                    }
                                }

                                strm.Flush();
                                strm.Close();
                            }
                        }
                        catch (Exception)
                        {
                            BeginInvoke((MethodInvoker)delegate
                            {
                                MessageBox.Show("Error downloading update files! Try again later", "Error downloading", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                Close();
                            });
                            return;
                        }

                        string srcpath = Path.GetDirectoryName(Application.ExecutablePath);

                        BeginInvoke((MethodInvoker)delegate
                        {
                            progressText.Text = "Installing";

                            File.Copy(Path.Combine(srcpath, "renderdoc.dll"), Path.Combine(dstpath, "renderdoc.dll"), true);
                            File.Copy(Path.Combine(srcpath, "renderdoccmd.exe"), Path.Combine(dstpath, "renderdoccmd.exe"), true);

                            var process = new Process();
                            process.StartInfo = new ProcessStartInfo(Path.Combine(dstpath, "renderdoccmd.exe"), "--update \"" + srcpath.Replace('\\', '/') + "/\"");
                            process.StartInfo.WorkingDirectory = dstpath;
                            process.StartInfo.Verb = "runas"; // need to run as admin to have write permissions

                            try
                            {
                                process.Start();
                                Environment.Exit(0);
                            }
                            catch (Exception ex)
                            {
                                // if there was an exception, display an error and don't exit.
                                MessageBox.Show(String.Format("Unknown error '{0}' encountered while trying to launch updater!", ex),
                                                "Error updating", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                Close();
                            }
                        });
                    }), g);
                }));

                updateThread.Start();
            }
        }

        private void close_Click(object sender, EventArgs e)
        {
            Close();
        }
    }
}
