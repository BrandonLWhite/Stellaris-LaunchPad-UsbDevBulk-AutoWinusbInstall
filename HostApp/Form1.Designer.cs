namespace UsbDevBulkHostApp
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.transmitButton = new System.Windows.Forms.Button();
            this.richTextBoxLog = new System.Windows.Forms.RichTextBox();
            this.textBoxToTransmit = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // transmitButton
            // 
            this.transmitButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.transmitButton.Enabled = false;
            this.transmitButton.Location = new System.Drawing.Point(623, 12);
            this.transmitButton.Name = "transmitButton";
            this.transmitButton.Size = new System.Drawing.Size(75, 23);
            this.transmitButton.TabIndex = 1;
            this.transmitButton.Text = "Transmit";
            this.transmitButton.UseVisualStyleBackColor = true;
            this.transmitButton.Click += new System.EventHandler(this.transmitButton_Click);
            // 
            // richTextBoxLog
            // 
            this.richTextBoxLog.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.richTextBoxLog.BackColor = System.Drawing.SystemColors.ControlLightLight;
            this.richTextBoxLog.DetectUrls = false;
            this.richTextBoxLog.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.richTextBoxLog.Location = new System.Drawing.Point(12, 41);
            this.richTextBoxLog.Name = "richTextBoxLog";
            this.richTextBoxLog.ReadOnly = true;
            this.richTextBoxLog.Size = new System.Drawing.Size(686, 209);
            this.richTextBoxLog.TabIndex = 1;
            this.richTextBoxLog.TabStop = false;
            this.richTextBoxLog.Text = "";
            // 
            // textBoxToTransmit
            // 
            this.textBoxToTransmit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.textBoxToTransmit.Location = new System.Drawing.Point(12, 13);
            this.textBoxToTransmit.Name = "textBoxToTransmit";
            this.textBoxToTransmit.Size = new System.Drawing.Size(605, 20);
            this.textBoxToTransmit.TabIndex = 0;
            this.textBoxToTransmit.Text = "Enter some text to transmit.";
            // 
            // Form1
            // 
            this.AcceptButton = this.transmitButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(710, 262);
            this.Controls.Add(this.textBoxToTransmit);
            this.Controls.Add(this.richTextBoxLog);
            this.Controls.Add(this.transmitButton);
            this.Name = "Form1";
            this.Text = "Stellaris LaunchPad USB Bulk Device Demo";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Form1_FormClosed);
            this.Shown += new System.EventHandler(this.Form1_Shown);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button transmitButton;
        private System.Windows.Forms.RichTextBox richTextBoxLog;
        private System.Windows.Forms.TextBox textBoxToTransmit;
    }
}

