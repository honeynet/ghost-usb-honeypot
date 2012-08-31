using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Xml.Serialization;

namespace GhostGUI
{
    public partial class MainForm : Form
    {
        string LogFileName;
        private List<Ghost> Ghosts;
        private Ghost MainGhost;
        private delegate void ViewUpdater();

        public MainForm()
        {
            InitializeComponent();
            LogFileName = Path.GetDirectoryName(Application.ExecutablePath) + Path.DirectorySeparatorChar + "log.xml";
        }

        private void buttonMount_Click(object sender, EventArgs e)
        {
            MainGhost = new Ghost();
            Ghosts.Add(MainGhost);

            MainGhost.OnUpdate += ThreadSafeUpdate;
            try
            {
                MainGhost.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message);
                MainGhost = null;
                return;
            }

            buttonMount.Enabled = false;
            buttonUmount.Enabled = true;
            radioButtonAutomatic.Enabled = false;
            radioButtonManual.Enabled = false;
        }

        private void buttonUmount_Click(object sender, EventArgs e)
        {
            try
            {
                MainGhost.Stop();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message);
                return;
            }

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

                StringBuilder sb = new StringBuilder();
                sb.Append(g.MountTime.ToString());
                sb.Append(" - ");
                sb.Append(g.IncidentList.Count > 0 ? "Detection!" : "No detection");
                TreeNode NewNode = treeViewResults.Nodes.Add(sb.ToString());
                foreach (Incident i in g.IncidentList)
                {
                    TreeNode SubNode = NewNode.Nodes.Add(i.Modules.Count > 0 ? Path.GetFileName(i.Modules[0]) : "System");
                    SubNode.Nodes.Add("PID: " + i.PID.ToString());
                    SubNode.Nodes.Add("TID: " + i.TID.ToString());
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
            Properties.Settings.Default.ManualControl = radioButtonManual.Checked;
            Properties.Settings.Default.Save();

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
            Properties.Settings.Default.MountInterval = numericMountInterval.Value;
            Properties.Settings.Default.Save();
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

            try
            {
                MainGhost.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message);
                MainGhost = null;
                return;
            }

            timerUmount.Interval = (int)Properties.Settings.Default.MountDuration * 1000;
            timerUmount.Start();
        }

        private void timerUmount_Tick(object sender, EventArgs e)
        {
            timerUmount.Stop();
            try
            {
                MainGhost.Stop();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message);
                return;
            }

            MainGhost = null;

            labelAutomaticInProgress.Hide();
            radioButtonAutomatic.Enabled = true;
            radioButtonManual.Enabled = true;

            timerGhost.Start();
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            textBoxImage.Text = Ghost.ImageFileName;
            numericMountInterval.Value = Properties.Settings.Default.MountInterval;
            numericMountDuration.Value = Properties.Settings.Default.MountDuration;

            if (!Properties.Settings.Default.ManualControl)
            {
                buttonMount.Enabled = false;
                buttonUmount.Enabled = false;
                numericMountInterval.Enabled = true;
                numericMountDuration.Enabled = true;
                timerGhost.Enabled = true;
                radioButtonAutomatic.Checked = true;
            }

            TextReader stream = null;
            try
            {
                stream = new StreamReader(LogFileName);
                XmlSerializer serializer = new XmlSerializer(typeof(List<Ghost>));
                Ghosts = (List<Ghost>)serializer.Deserialize(stream);
            }
            catch
            {
                Ghosts = new List<Ghost>();
            }
            finally
            {
                if (stream != null)
                {
                    stream.Close();
                }
            }

            UpdateTreeView();
        }

        private void buttonApplyConfig_Click(object sender, EventArgs e)
        {
            string ImageFile = textBoxImage.Text;
            string FullPath = Path.GetFullPath(ImageFile);
            if (!Directory.Exists(Path.GetDirectoryName(FullPath)))
            {
                MessageBox.Show(this, "The specified directory\ndoes not exist!");
                textBoxImage.SelectAll();
                textBoxImage.Focus();
                return;
            }

            /*if (!Path.GetFileNameWithoutExtension(FullPath).Contains('0'))
            {
                MessageBox.Show(this, "The file name must\ncontain a '0'!");
                textBoxImage.SelectAll();
                textBoxImage.Focus();
                return;
            }*/

            Ghost.ImageFileName = FullPath;
            buttonApplyConfig.Enabled = false;
        }

        private void textBoxImage_TextChanged(object sender, EventArgs e)
        {
            buttonApplyConfig.Enabled = !textBoxImage.Text.Equals(Ghost.ImageFileName);
        }

        private void buttonFindImage_Click(object sender, EventArgs e)
        {
            openFileDialogImage.FileName = Ghost.ImageFileName;
            if (openFileDialogImage.ShowDialog(this) == System.Windows.Forms.DialogResult.OK)
            {
                textBoxImage.Text = openFileDialogImage.FileName;
            }
        }

        private void notifyIcon_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            this.Show();
            this.WindowState = FormWindowState.Normal;
        }

        private void MainForm_Resize(object sender, EventArgs e)
        {
            if (this.WindowState == FormWindowState.Minimized)
            {
                this.Hide();
            }
        }

        private void numericMountDuration_ValueChanged(object sender, EventArgs e)
        {
            Properties.Settings.Default.MountDuration = numericMountDuration.Value;
            Properties.Settings.Default.Save();
        }

        private void toolStripMenuItemQuit_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void toolStripMenuItemRestore_Click(object sender, EventArgs e)
        {
            this.Show();
            this.WindowState = FormWindowState.Normal;
        }

        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (MainGhost != null)
            {
                try
                {
                    MainGhost.Stop();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message);
                }
            }

            TextWriter stream = null;
            try
            {
                stream = new StreamWriter(LogFileName);
                XmlSerializer serializer = new XmlSerializer(typeof(List<Ghost>));
                serializer.Serialize(stream, Ghosts);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "Could not store the log on disk: " + ex.Message);
            }
            finally
            {
                if (stream != null)
                {
                    stream.Close();
                }
            }
        }

        private void buttonClearLog_Click(object sender, EventArgs e)
        {
            Ghosts.Clear();
            UpdateTreeView();
        }
    }
}
