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
        private List<Ghost> Ghosts = new List<Ghost>();
        private Ghost MainGhost;
        private delegate void ViewUpdater();

        public MainForm()
        {
            InitializeComponent();
        }

        private void buttonMount_Click(object sender, EventArgs e)
        {
            MainGhost = new Ghost();
            Ghosts.Add(MainGhost);

            MainGhost.OnUpdate += ThreadSafeUpdate;
            MainGhost.Start();

            buttonMount.Enabled = false;
            buttonUmount.Enabled = true;
            radioButtonAutomatic.Enabled = false;
            radioButtonManual.Enabled = false;
        }

        private void buttonUmount_Click(object sender, EventArgs e)
        {
            MainGhost.Stop();
            MainGhost = null;

            buttonUmount.Enabled = false;
            buttonMount.Enabled = true;
            radioButtonManual.Enabled = true;
            radioButtonAutomatic.Enabled = true;
        }

        private void ThreadSafeUpdate()
        {
            if (treeViewResults.InvokeRequired)
            {
                ViewUpdater Updater = UpdateTreeView;
                treeViewResults.Invoke(Updater);
            }
            else
            {
                UpdateTreeView();
            }
        }

        private void UpdateTreeView()
        {
            treeViewResults.BeginUpdate();
            treeViewResults.Nodes.Clear();

            foreach (Ghost g in Ghosts)
            {
                if (g.MountTime == null)
                {
                    continue;
                }

                TreeNode NewNode = treeViewResults.Nodes.Add(g.MountTime.ToString());
                foreach (Incident i in g.IncidentList)
                {
                    TreeNode SubNode = NewNode.Nodes.Add("PID: " + i.PID.ToString() + ", TID: " + i.TID.ToString());
                    if (i.Modules.Count != 0)
                    {
                        TreeNode Modules = SubNode.Nodes.Add("Loaded Modules");
                        foreach (String m in i.Modules)
                        {
                            Modules.Nodes.Add(m);
                        }
                    }
                }
            }

            treeViewResults.EndUpdate();
        }

        private void radioButtonManual_CheckedChanged(object sender, EventArgs e)
        {
            if (radioButtonManual.Checked)
            {
                buttonMount.Enabled = true;
                buttonUmount.Enabled = false;
                numericMountInterval.Enabled = false;
                numericMountDuration.Enabled = false;
                timerGhost.Enabled = false;
            }
            else
            {
                buttonMount.Enabled = false;
                buttonUmount.Enabled = false;
                numericMountInterval.Enabled = true;
                numericMountDuration.Enabled = true;
                timerGhost.Enabled = true;
            }
        }

        private void numericMountInterval_ValueChanged(object sender, EventArgs e)
        {
            timerGhost.Interval = (int) numericMountInterval.Value * 60000;
        }

        private void timerGhost_Tick(object sender, EventArgs e)
        {
            timerGhost.Stop();

            radioButtonAutomatic.Enabled = false;
            radioButtonManual.Enabled = false;
            labelAutomaticInProgress.Show();

            MainGhost = new Ghost();
            Ghosts.Add(MainGhost);
            MainGhost.OnUpdate += ThreadSafeUpdate;

            MainGhost.Start();

            timerUmount.Interval = (int)numericMountDuration.Value * 1000;
            timerUmount.Start();
        }

        private void timerUmount_Tick(object sender, EventArgs e)
        {
            timerUmount.Stop();
            MainGhost.Stop();

            labelAutomaticInProgress.Hide();
            radioButtonAutomatic.Enabled = true;
            radioButtonManual.Enabled = true;

            timerGhost.Start();
        }
    }
}
