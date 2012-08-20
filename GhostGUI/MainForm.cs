using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace GhostGUI
{
    public partial class MainForm : Form
    {
        private Ghost MainGhost = new Ghost();

        public MainForm()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            MainGhost.Start();
        }

        private void button2_Click(object sender, EventArgs e)
        {
            MainGhost.Stop();
            foreach (Incident i in MainGhost.IncidentList)
            {
                MessageBox.Show(i.ToString());
            }
        }
    }
}
