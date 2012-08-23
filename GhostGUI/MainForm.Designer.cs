namespace GhostGUI
{
    partial class MainForm
    {
        /// <summary>
        /// Erforderliche Designervariable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Verwendete Ressourcen bereinigen.
        /// </summary>
        /// <param name="disposing">True, wenn verwaltete Ressourcen gelöscht werden sollen; andernfalls False.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Vom Windows Form-Designer generierter Code

        /// <summary>
        /// Erforderliche Methode für die Designerunterstützung.
        /// Der Inhalt der Methode darf nicht mit dem Code-Editor geändert werden.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            this.buttonMount = new System.Windows.Forms.Button();
            this.buttonUmount = new System.Windows.Forms.Button();
            this.treeViewResults = new System.Windows.Forms.TreeView();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.numericMountInterval = new System.Windows.Forms.NumericUpDown();
            this.numericMountDuration = new System.Windows.Forms.NumericUpDown();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.groupBox3 = new System.Windows.Forms.GroupBox();
            this.labelAutomaticInProgress = new System.Windows.Forms.Label();
            this.radioButtonManual = new System.Windows.Forms.RadioButton();
            this.radioButtonAutomatic = new System.Windows.Forms.RadioButton();
            this.groupBox4 = new System.Windows.Forms.GroupBox();
            this.timerGhost = new System.Windows.Forms.Timer(this.components);
            this.timerUmount = new System.Windows.Forms.Timer(this.components);
            this.groupBox5 = new System.Windows.Forms.GroupBox();
            this.buttonFindImage = new System.Windows.Forms.Button();
            this.buttonApplyConfig = new System.Windows.Forms.Button();
            this.label3 = new System.Windows.Forms.Label();
            this.textBoxImage = new System.Windows.Forms.TextBox();
            this.openFileDialogImage = new System.Windows.Forms.OpenFileDialog();
            this.notifyIcon = new System.Windows.Forms.NotifyIcon(this.components);
            this.contextMenuStripIcon = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.toolStripMenuItemRestore = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItemQuit = new System.Windows.Forms.ToolStripMenuItem();
            this.groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericMountInterval)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericMountDuration)).BeginInit();
            this.groupBox2.SuspendLayout();
            this.groupBox3.SuspendLayout();
            this.groupBox4.SuspendLayout();
            this.groupBox5.SuspendLayout();
            this.contextMenuStripIcon.SuspendLayout();
            this.SuspendLayout();
            // 
            // buttonMount
            // 
            this.buttonMount.Location = new System.Drawing.Point(9, 29);
            this.buttonMount.Name = "buttonMount";
            this.buttonMount.Size = new System.Drawing.Size(187, 28);
            this.buttonMount.TabIndex = 0;
            this.buttonMount.Text = "Mount";
            this.buttonMount.UseVisualStyleBackColor = true;
            this.buttonMount.Click += new System.EventHandler(this.buttonMount_Click);
            // 
            // buttonUmount
            // 
            this.buttonUmount.Enabled = false;
            this.buttonUmount.Location = new System.Drawing.Point(9, 63);
            this.buttonUmount.Name = "buttonUmount";
            this.buttonUmount.Size = new System.Drawing.Size(187, 28);
            this.buttonUmount.TabIndex = 1;
            this.buttonUmount.Text = "Umount";
            this.buttonUmount.UseVisualStyleBackColor = true;
            this.buttonUmount.Click += new System.EventHandler(this.buttonUmount_Click);
            // 
            // treeViewResults
            // 
            this.treeViewResults.Location = new System.Drawing.Point(6, 19);
            this.treeViewResults.Name = "treeViewResults";
            this.treeViewResults.Size = new System.Drawing.Size(406, 398);
            this.treeViewResults.TabIndex = 3;
            this.treeViewResults.Tag = "";
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.buttonUmount);
            this.groupBox1.Controls.Add(this.buttonMount);
            this.groupBox1.Location = new System.Drawing.Point(12, 95);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(208, 103);
            this.groupBox1.TabIndex = 4;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Manual control";
            // 
            // numericMountInterval
            // 
            this.numericMountInterval.Enabled = false;
            this.numericMountInterval.Location = new System.Drawing.Point(152, 29);
            this.numericMountInterval.Maximum = new decimal(new int[] {
            120,
            0,
            0,
            0});
            this.numericMountInterval.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.numericMountInterval.Name = "numericMountInterval";
            this.numericMountInterval.Size = new System.Drawing.Size(44, 20);
            this.numericMountInterval.TabIndex = 5;
            this.numericMountInterval.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.numericMountInterval.Value = new decimal(new int[] {
            10,
            0,
            0,
            0});
            this.numericMountInterval.ValueChanged += new System.EventHandler(this.numericMountInterval_ValueChanged);
            // 
            // numericMountDuration
            // 
            this.numericMountDuration.Enabled = false;
            this.numericMountDuration.Location = new System.Drawing.Point(152, 58);
            this.numericMountDuration.Maximum = new decimal(new int[] {
            50,
            0,
            0,
            0});
            this.numericMountDuration.Minimum = new decimal(new int[] {
            5,
            0,
            0,
            0});
            this.numericMountDuration.Name = "numericMountDuration";
            this.numericMountDuration.Size = new System.Drawing.Size(44, 20);
            this.numericMountDuration.TabIndex = 6;
            this.numericMountDuration.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
            this.numericMountDuration.Value = new decimal(new int[] {
            30,
            0,
            0,
            0});
            this.numericMountDuration.ValueChanged += new System.EventHandler(this.numericMountDuration_ValueChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(6, 31);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(122, 13);
            this.label1.TabIndex = 7;
            this.label1.Text = "Mount interval (minutes):";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 60);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(130, 13);
            this.label2.TabIndex = 8;
            this.label2.Text = "Mount duration (seconds):";
            // 
            // groupBox2
            // 
            this.groupBox2.Controls.Add(this.label2);
            this.groupBox2.Controls.Add(this.label1);
            this.groupBox2.Controls.Add(this.numericMountDuration);
            this.groupBox2.Controls.Add(this.numericMountInterval);
            this.groupBox2.Location = new System.Drawing.Point(12, 204);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(208, 103);
            this.groupBox2.TabIndex = 9;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "Automatic control";
            // 
            // groupBox3
            // 
            this.groupBox3.Controls.Add(this.labelAutomaticInProgress);
            this.groupBox3.Controls.Add(this.radioButtonManual);
            this.groupBox3.Controls.Add(this.radioButtonAutomatic);
            this.groupBox3.Location = new System.Drawing.Point(12, 12);
            this.groupBox3.Name = "groupBox3";
            this.groupBox3.Size = new System.Drawing.Size(208, 71);
            this.groupBox3.TabIndex = 10;
            this.groupBox3.TabStop = false;
            this.groupBox3.Text = "Mode";
            // 
            // labelAutomaticInProgress
            // 
            this.labelAutomaticInProgress.AutoSize = true;
            this.labelAutomaticInProgress.Location = new System.Drawing.Point(17, 48);
            this.labelAutomaticInProgress.Name = "labelAutomaticInProgress";
            this.labelAutomaticInProgress.Size = new System.Drawing.Size(164, 13);
            this.labelAutomaticInProgress.TabIndex = 2;
            this.labelAutomaticInProgress.Text = "Automatic operation in progress...";
            this.labelAutomaticInProgress.Visible = false;
            // 
            // radioButtonManual
            // 
            this.radioButtonManual.AutoSize = true;
            this.radioButtonManual.Checked = true;
            this.radioButtonManual.Location = new System.Drawing.Point(20, 28);
            this.radioButtonManual.Name = "radioButtonManual";
            this.radioButtonManual.Size = new System.Drawing.Size(60, 17);
            this.radioButtonManual.TabIndex = 1;
            this.radioButtonManual.TabStop = true;
            this.radioButtonManual.Text = "Manual";
            this.radioButtonManual.UseVisualStyleBackColor = true;
            this.radioButtonManual.CheckedChanged += new System.EventHandler(this.radioButtonManual_CheckedChanged);
            // 
            // radioButtonAutomatic
            // 
            this.radioButtonAutomatic.AutoSize = true;
            this.radioButtonAutomatic.Location = new System.Drawing.Point(109, 28);
            this.radioButtonAutomatic.Name = "radioButtonAutomatic";
            this.radioButtonAutomatic.Size = new System.Drawing.Size(72, 17);
            this.radioButtonAutomatic.TabIndex = 0;
            this.radioButtonAutomatic.Text = "Automatic";
            this.radioButtonAutomatic.UseVisualStyleBackColor = true;
            // 
            // groupBox4
            // 
            this.groupBox4.Controls.Add(this.treeViewResults);
            this.groupBox4.Location = new System.Drawing.Point(226, 12);
            this.groupBox4.Name = "groupBox4";
            this.groupBox4.Size = new System.Drawing.Size(418, 423);
            this.groupBox4.TabIndex = 11;
            this.groupBox4.TabStop = false;
            this.groupBox4.Text = "Results";
            // 
            // timerGhost
            // 
            this.timerGhost.Interval = 600000;
            this.timerGhost.Tick += new System.EventHandler(this.timerGhost_Tick);
            // 
            // timerUmount
            // 
            this.timerUmount.Tick += new System.EventHandler(this.timerUmount_Tick);
            // 
            // groupBox5
            // 
            this.groupBox5.Controls.Add(this.buttonFindImage);
            this.groupBox5.Controls.Add(this.buttonApplyConfig);
            this.groupBox5.Controls.Add(this.label3);
            this.groupBox5.Controls.Add(this.textBoxImage);
            this.groupBox5.Location = new System.Drawing.Point(12, 313);
            this.groupBox5.Name = "groupBox5";
            this.groupBox5.Size = new System.Drawing.Size(208, 122);
            this.groupBox5.TabIndex = 12;
            this.groupBox5.TabStop = false;
            this.groupBox5.Text = "Configuration";
            // 
            // buttonFindImage
            // 
            this.buttonFindImage.Location = new System.Drawing.Point(171, 39);
            this.buttonFindImage.Name = "buttonFindImage";
            this.buttonFindImage.Size = new System.Drawing.Size(25, 23);
            this.buttonFindImage.TabIndex = 3;
            this.buttonFindImage.Text = "...";
            this.buttonFindImage.UseVisualStyleBackColor = true;
            this.buttonFindImage.Click += new System.EventHandler(this.buttonFindImage_Click);
            // 
            // buttonApplyConfig
            // 
            this.buttonApplyConfig.Enabled = false;
            this.buttonApplyConfig.Location = new System.Drawing.Point(9, 77);
            this.buttonApplyConfig.Name = "buttonApplyConfig";
            this.buttonApplyConfig.Size = new System.Drawing.Size(187, 28);
            this.buttonApplyConfig.TabIndex = 2;
            this.buttonApplyConfig.Text = "Apply changes";
            this.buttonApplyConfig.UseVisualStyleBackColor = true;
            this.buttonApplyConfig.Click += new System.EventHandler(this.buttonApplyConfig_Click);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(6, 25);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(121, 13);
            this.label3.TabIndex = 1;
            this.label3.Text = "Image file for device #0:";
            // 
            // textBoxImage
            // 
            this.textBoxImage.Location = new System.Drawing.Point(9, 41);
            this.textBoxImage.Name = "textBoxImage";
            this.textBoxImage.Size = new System.Drawing.Size(156, 20);
            this.textBoxImage.TabIndex = 0;
            this.textBoxImage.TextChanged += new System.EventHandler(this.textBoxImage_TextChanged);
            // 
            // openFileDialogImage
            // 
            this.openFileDialogImage.Filter = "All files|*.*";
            this.openFileDialogImage.Title = "Open image file";
            // 
            // notifyIcon
            // 
            this.notifyIcon.ContextMenuStrip = this.contextMenuStripIcon;
            this.notifyIcon.Icon = ((System.Drawing.Icon)(resources.GetObject("notifyIcon.Icon")));
            this.notifyIcon.Text = "Ghost USB honeypot";
            this.notifyIcon.Visible = true;
            this.notifyIcon.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.notifyIcon_MouseDoubleClick);
            // 
            // contextMenuStripIcon
            // 
            this.contextMenuStripIcon.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripMenuItemRestore,
            this.toolStripMenuItemQuit});
            this.contextMenuStripIcon.Name = "contextMenuStripIcon";
            this.contextMenuStripIcon.ShowImageMargin = false;
            this.contextMenuStripIcon.Size = new System.Drawing.Size(99, 48);
            // 
            // toolStripMenuItemRestore
            // 
            this.toolStripMenuItemRestore.Name = "toolStripMenuItemRestore";
            this.toolStripMenuItemRestore.Size = new System.Drawing.Size(127, 22);
            this.toolStripMenuItemRestore.Text = "Restore";
            this.toolStripMenuItemRestore.Click += new System.EventHandler(this.toolStripMenuItemRestore_Click);
            // 
            // toolStripMenuItemQuit
            // 
            this.toolStripMenuItemQuit.Name = "toolStripMenuItemQuit";
            this.toolStripMenuItemQuit.Size = new System.Drawing.Size(127, 22);
            this.toolStripMenuItemQuit.Text = "Quit";
            this.toolStripMenuItemQuit.Click += new System.EventHandler(this.toolStripMenuItemQuit_Click);
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(656, 447);
            this.Controls.Add(this.groupBox5);
            this.Controls.Add(this.groupBox4);
            this.Controls.Add(this.groupBox3);
            this.Controls.Add(this.groupBox2);
            this.Controls.Add(this.groupBox1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "MainForm";
            this.Text = "Ghost";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MainForm_FormClosing);
            this.Load += new System.EventHandler(this.MainForm_Load);
            this.Resize += new System.EventHandler(this.MainForm_Resize);
            this.groupBox1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.numericMountInterval)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericMountDuration)).EndInit();
            this.groupBox2.ResumeLayout(false);
            this.groupBox2.PerformLayout();
            this.groupBox3.ResumeLayout(false);
            this.groupBox3.PerformLayout();
            this.groupBox4.ResumeLayout(false);
            this.groupBox5.ResumeLayout(false);
            this.groupBox5.PerformLayout();
            this.contextMenuStripIcon.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button buttonMount;
        private System.Windows.Forms.Button buttonUmount;
        private System.Windows.Forms.TreeView treeViewResults;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.NumericUpDown numericMountInterval;
        private System.Windows.Forms.NumericUpDown numericMountDuration;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.GroupBox groupBox2;
        private System.Windows.Forms.GroupBox groupBox3;
        private System.Windows.Forms.RadioButton radioButtonManual;
        private System.Windows.Forms.RadioButton radioButtonAutomatic;
        private System.Windows.Forms.GroupBox groupBox4;
        private System.Windows.Forms.Timer timerGhost;
        private System.Windows.Forms.Label labelAutomaticInProgress;
        private System.Windows.Forms.Timer timerUmount;
        private System.Windows.Forms.GroupBox groupBox5;
        private System.Windows.Forms.Button buttonApplyConfig;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.TextBox textBoxImage;
        private System.Windows.Forms.Button buttonFindImage;
        private System.Windows.Forms.OpenFileDialog openFileDialogImage;
        private System.Windows.Forms.NotifyIcon notifyIcon;
        private System.Windows.Forms.ContextMenuStrip contextMenuStripIcon;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemRestore;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemQuit;
    }
}

