using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace renderdocui.Windows.Dialogs
{
    public partial class TipsDialog : Form
    {
        struct Tip
        {
            public Tip(string tt, string tx, Bitmap im)
            {
                title = tt;
                text = tx;
                image = im;
            }

            public string title;
            public string text;
            public Bitmap image; 
        }
        private List<Tip> m_Tips = new List<Tip>();

        private Random m_Rand = new Random();
        private int m_CurrentTip = 0;

        public TipsDialog()
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            ///////////////////////////////////////////////////////////
            // This section of code is auto-generated. Modifications //
            // will be lost if made by hand.                         //
            //                                                       //
            // If you have a tip you'd like to add, email it to me.  //
            ///////////////////////////////////////////////////////////

            // Tip 1
            m_Tips.Add(new Tip(
                "Talk to me!",
                "RenderDoc is a labour of love and developed entirely in my spare time. If you run into a bug, have a" +
                "feature request or just have a question about it, please feel free to get in touch and I'm always" +
                "happy to talk and help out in any way I can - baldurk@baldurk.org. " +
                "",
                null));

            // Tip 2
            m_Tips.Add(new Tip(
                "Quick channel toggling",
                "You can quickly toggle between RGB and Alpha in the Texture Viewer by right clicking on A - alpha." +
                "In general right clicking on a channel will toggle between only showing that channel, and showing" +
                "all other channels except it. " +
                "",
                null));

            // Tip 3
            m_Tips.Add(new Tip(
                "Quick autofit follow",
                "If you right click on the 'autofit' wand in the texture viewer, it will be locked on until you right" +
                "click again. This means when you change texture or step through events, it will re-fit the texture" +
                "continuously. This can be useful if you are e.g. jumping between a texture with a very compressed" +
                "range like a depth texture and a normal render target. " +
                "",
                null));

            ///////////////////////////////////////////////////////////
            // This section of code is auto-generated. Modifications //
            // will be lost if made by hand.                         //
            //                                                       //
            // If you have a tip you'd like to add, email it to me.  //
            ///////////////////////////////////////////////////////////

        }

        private void LoadRandomTip(object sender, EventArgs e)
        {
            int cur = m_CurrentTip;

            // ensure we at least switch to a different tip
            while(m_CurrentTip == cur)
                m_CurrentTip = m_Rand.Next(m_Tips.Count);

            LoadTip();
        }

        private void LoadTip()
        {
            Tip tip = m_Tips[m_CurrentTip];

            tipBox.Text = String.Format("Tip #{0}", m_CurrentTip + 1);
            tipTitle.Text = String.Format("Tip #{0}: {1}", m_CurrentTip + 1, tip.title);
            tipText.Text = tip.text;
            tipLink.Text = String.Format("https://renderdoc.org/tips/{0}", m_CurrentTip + 1);

            if (tip.image == null)
            {
                tipPicture.Image = null;
                tipPicture.Visible = false;
            }
            else
            {
                tipPicture.Image = tip.image;
                tipPicture.Visible = true;
            }
        }

        private void LoadNextTip(object sender, EventArgs e)
        {
            m_CurrentTip++;

            if (m_CurrentTip >= m_Tips.Count)
                m_CurrentTip = 0;

            LoadTip();
        }

        private void tipLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            Process.Start(tipLink.Text);
        }
    }
}
