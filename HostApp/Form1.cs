using System;
using System.Windows.Forms;

namespace UsbDevBulkHostApp
{
/// <summary>
/// This simple form allows the user to input a test string and transmit it to a USB Bulk device EP1.
/// Whatever the device sends back on EP1 is receieved and displayed in a text box.
/// 
/// Device connect/disconnect events are subscribed to, allowing the application to automatically connect
/// to a USB device with the VID/PID of interest.
/// 
/// The LibUsbDotNet C# USB Library (http://sourceforge.net/projects/libusbdotnet/) is used to facilitate communication
/// with the device.
/// 
/// </summary>
public partial class Form1 : Form
{
    /// <summary>
    /// USB device instance.  This is the 'handle' to our device.  It is used to create reade and write objects.
    /// </summary>
    LibUsbDotNet.UsbDevice m_usbDevice;

    /// <summary>
    /// This USB reader is setup to asynchronously recieve data from our device.
    /// </summary>
    LibUsbDotNet.UsbEndpointReader m_usbReader;

    /// <summary>
    /// The notifier is used to subscribe to event based notifications regarding device connect/disconnect events.
    /// We use it recognize and respond to our USB device connecting or disconnecting.
    /// </summary>
    LibUsbDotNet.DeviceNotify.IDeviceNotifier m_usbNotifier;

    /// <summary>
    /// The USB device finder specifies the criteria that LibUsbDotNet will use to find and connect to a USB device.
    /// The VID/PID used here matches the TI Stellaris LaunchPad usb_dev_bulk example.  You may change these to match
    /// the values for your particular device.  They reside in the Settings.settings XML file.
    /// </summary>
    LibUsbDotNet.Main.UsbDeviceFinder m_usbFinder = new LibUsbDotNet.Main.UsbDeviceFinder(
        Properties.Settings.Default.UsbVid, Properties.Settings.Default.UsbPid);

    /// <summary>
    /// Upon construction, the USB notifier is started and an attempt is made to connect to a USB device.
    /// </summary>
    public Form1()
    {
        InitializeComponent();

        // Bail if we are running inside of the Visual Studio designer view.
        //
        if (System.ComponentModel.LicenseManager.UsageMode == System.ComponentModel.LicenseUsageMode.Designtime) return;

        m_usbNotifier = LibUsbDotNet.DeviceNotify.DeviceNotifier.OpenDeviceNotifier();
        m_usbNotifier.OnDeviceNotify += new EventHandler<LibUsbDotNet.DeviceNotify.DeviceNotifyEventArgs>(UsbNotifier_OnDeviceNotify);
    }

    /// <summary>
    /// Event handler for USB device enumeration changes.
    /// This will look for connections (DeviceArrival) for our VID/PID and open the device if needed,
    /// or for disconnections it will close/release the instance.
    /// </summary>
    void UsbNotifier_OnDeviceNotify(object sender, LibUsbDotNet.DeviceNotify.DeviceNotifyEventArgs eventArgs)
    {
        Log("Device notification event: {0} VID={1:X4} PID={2:X4}", 
            eventArgs.EventType, eventArgs.Device.IdVendor, eventArgs.Device.IdProduct);

        // Ignore events for other devices
        //
        if (m_usbFinder.Vid != eventArgs.Device.IdVendor) return;
        if (m_usbFinder.Pid != eventArgs.Device.IdProduct) return;

        if (m_usbDevice == null && eventArgs.EventType == LibUsbDotNet.DeviceNotify.EventType.DeviceArrival)
        {
            // Grab this device
            //
            OpenUsbDevice();
        }
        // If this was our device being removed, then close the handle.  The only thing I could glean from inspecting 
        // m_usbDevice after a remove event on that actual device was that the number of Configs went from non-zero
        // to zero.
        //
        else if(eventArgs.EventType == LibUsbDotNet.DeviceNotify.EventType.DeviceRemoveComplete && 
                m_usbDevice.Configs.Count == 0)
        {
            CloseUsbDevice();
        }
    }

    /// <summary>
    /// Open a new instance of a USB device using the finder criteria.  (Innocuous if device instance already exists).
    /// This will also enable the Transmit button as well as setup the asynchronous reader and event.
    /// </summary>
    void OpenUsbDevice()
    {
        if (m_usbDevice != null) return;

        m_usbDevice = LibUsbDotNet.UsbDevice.OpenUsbDevice(m_usbFinder);
        
        if (m_usbDevice == null)
        {
            Log("No device found with VID={0:X4} PID={1:X4}", m_usbFinder.Vid, m_usbFinder.Pid);
            return;
        }
        
        transmitButton.Enabled = true;
        
        m_usbReader = m_usbDevice.OpenEndpointReader(LibUsbDotNet.Main.ReadEndpointID.Ep01);
        m_usbReader.DataReceived += new EventHandler<LibUsbDotNet.Main.EndpointDataEventArgs>(UsbReader_DataReceived);
        m_usbReader.DataReceivedEnabled = true;

        // UsbRegistryInfo is null in Linux, so only include it if defined.
        //
        string sSymbolicInfo = m_usbDevice.UsbRegistryInfo == null ? string.Empty :
            string.Format(" ({0})", m_usbDevice.UsbRegistryInfo.SymbolicName);
        Log("Opened device VID={0:X4} PID={1:X4}{2}.",
            m_usbDevice.Info.Descriptor.VendorID, m_usbDevice.Info.Descriptor.ProductID, sSymbolicInfo);
    }

    /// <summary>
    /// Event handler for USB data received from the device.  All that this really does is decode the bytes
    /// into a string and log it for your viewing pleasure.
    /// </summary>
    void UsbReader_DataReceived(object sender, LibUsbDotNet.Main.EndpointDataEventArgs eventArgs)
    {
        string sReceived = System.Text.Encoding.ASCII.GetString(eventArgs.Buffer, 0, eventArgs.Count);
        Log("RX: {0}", sReceived);
    }

    /// <summary>
    /// Gracefully dispose and close the USB device and the related reader.
    /// </summary>
    void CloseUsbDevice()
    {
        transmitButton.Enabled = false;

        if (m_usbReader != null)
        {
            m_usbReader.Abort();
            m_usbReader.Dispose();
            m_usbReader = null;
        }

        if (null == m_usbDevice) return;

        m_usbDevice.Close();
        m_usbDevice = null;

        Log("Closed device.");
    }

    /// <summary>
    /// Synchronously send the argument string to the device, encoded as ASCII bytes.
    /// </summary>
    /// <remarks>
    /// In your application you likely want to send some kind of binary data, maybe like Google Protocol Buffers.
    /// But for the usb_dev_bulk firmware, all that the device does is take a string, flip the character case,
    /// and send it back.  So this is why a string is the argument here.
    /// In a production application you'd probably want to asynchronously send data to the device, or even consider
    /// a buffering stream.
    /// Also, there's no error checking in this example.  You'll want to check the return value of the Write call, at the 
    /// very least.</remarks>
    void TransmitToDevice(string sData)
    {
        using (LibUsbDotNet.UsbEndpointWriter writer = m_usbDevice.OpenEndpointWriter(LibUsbDotNet.Main.WriteEndpointID.Ep01))
        {
            int iBytesTransmitted;
            string sTestData = textBoxToTransmit.Text;

            Log("TX: {0}", sTestData);

            byte[] abyTestData = System.Text.Encoding.ASCII.GetBytes(sTestData);

            writer.Write(abyTestData, 2000, out iBytesTransmitted);
        }
    }

    /// <summary>
    /// When the Transmit button is clicked, grab the text from the edit box and send it to the USB device.
    /// </summary>
    void transmitButton_Click(object sender, EventArgs e)
    {
        TransmitToDevice(textBoxToTransmit.Text);
    }

    /// <summary>
    /// Simple output logger. This takes the argument text, formats it, and appends it to the the rich edit box.
    /// </summary>
    void Log(string sText, params object[] values)
    {
        // If coming in from a thread other than the GUI thread (which is the case with async receives), then post this to the GUI
        // thread queue to avoid exceptions that would otherwise occur when attemping to manipulate a control.
        //
        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => Log(sText, values)));
            return;
        }

        richTextBoxLog.AppendText(string.Format(sText, values) + "\n");
        richTextBoxLog.ScrollToCaret();
    }

    void Form1_Shown(object sender, EventArgs e)
    {
        OpenUsbDevice();
    }

    void Form1_FormClosed(object sender, FormClosedEventArgs e)
    {
        CloseUsbDevice();
        m_usbNotifier.Enabled = false;
        LibUsbDotNet.UsbDevice.Exit();        
    }
}
}
