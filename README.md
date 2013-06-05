Stellaris-LaunchPad-UsbDevBulk-AutoWinusbInstall
================================================

This is a working demonstration of how to automatically install the generic Windows WinUSB.sys driver using only device firmware descriptors on the TI Stellaris LaunchPad.  That's right -- **NO .INF file** and **NO co-installer hassle**.  It doesn't even matter what your VID/PID are (assuming they don't match something that Windows already has a driver/INF for).  Simply plug in your device and Windows will figure out what to do.  Your application is then good to go communicating with your firmware.  

The ability to do this is somewhat recent to Windows.  For Windows 8 the support is fully baked in, but for prior versions Windows will gladly pull down what it needs via Windows Update.  I've read that this will work as far back as WinXP SP3, but I have not personally tried it.  I developed this demo on Win7 x64, using Code Composer Studio v5.2.1.00018 and StellarisWare version 9453.

This demo focuses on bulk USB transfers.  


##StellarisWare usblib Hacks
I had to make some changes internal to usblib in order for this to work.  I tried to do so as elegantly as possible and within the conventions already established within usblib.  Rather than kludge in the code to respond to the 0xEE request directly in usblib/device/usbdenum.c, I instead opted to add a callback to tCustomHandlers.  This new callback is named    `pfnGetStringDescriptor` and is invoked whenever the USB host requests a string descriptor that is not in your static string descriptor table.
 
- USB 2.0


##How it Works
....................

##How to Integrate This Into Your Project
....................
- Replace usblib. (What about prebuild .libs?)

##TODO
....................
C# host application to move some bulk traffic around.

##References
https://github.com/pbatard/libwdi/wiki/WCID-Devices#wiki-Other **Very helpful**
