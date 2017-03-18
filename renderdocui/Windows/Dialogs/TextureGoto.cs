using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace renderdocui.Windows.Dialogs
{
    public partial class TextureGoto : Form
    {
        public delegate void OnFinishedMethod(int x, int y);

        private OnFinishedMethod m_FinishedCallback;

        public TextureGoto(OnFinishedMethod callback)
        {
            InitializeComponent();

            m_FinishedCallback = callback;
        }

        public int X
        {
            get
            {
                try
                {
                    return (int)chooseX.Value;
                }
                catch (System.ArgumentOutOfRangeException)
                {
                    return 0;
                }
            }
            set
            {
                try
                {
                    chooseX.Value = (decimal)value;
                }
                catch (System.ArgumentOutOfRangeException)
                {
                    chooseX.Value = 0;
                }
            }
        }

        public int Y
        {
            get
            {
                try
                {
                    return (int)chooseY.Value;
                }
                catch (System.ArgumentOutOfRangeException)
                {
                    return 0;
                }
            }
            set
            {
                try
                {
                    chooseY.Value = (decimal)value;
                }
                catch (System.ArgumentOutOfRangeException)
                {
                    chooseY.Value = 0;
                }
            }
        }

        public void Show(Control parent, Point p)
        {
            X = p.X;
            Y = p.Y;

            Location = new Point(
                parent.PointToScreen(parent.Location).X + parent.ClientRectangle.Width / 2 - ClientRectangle.Width / 2,
                parent.PointToScreen(parent.Location).Y + parent.ClientRectangle.Height / 2 - ClientRectangle.Height / 2
                );

            Show();

            chooseY.Select();
            chooseX.Select();
        }

        private void location_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                m_FinishedCallback(X, Y);
                Hide();
                return;
            }
        }

        private void TextureGoto_Deactivate(object sender, EventArgs e)
        {
            Hide();
        }

        private void location_Enter(object sender, EventArgs e)
        {
            NumericUpDown up = sender as NumericUpDown;
            if (up != null)
            {
                up.Select(0, 100);
            }
        }
    }
}
