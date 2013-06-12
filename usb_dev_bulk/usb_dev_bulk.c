//*****************************************************************************
//
// usb_dev_bulk.c - Main routines for the generic bulk device example.
//
// Copyright (c) 2012 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 9453 of the EK-LM4F120XL Firmware Package.
//
//*****************************************************************************

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"
#include "driverlib/usb.h"
#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "usb_bulk_structs.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>USB Generic Bulk Device (usb_dev_bulk)</h1>
//!
//! This example provides a generic USB device offering simple bulk data
//! transfer to and from the host.  The device uses a vendor-specific class ID
//! and supports a single bulk IN endpoint and a single bulk OUT endpoint.
//! Data received from the host is assumed to be ASCII text and it is
//! echoed back with the case of all alphabetic characters swapped.
//!
//! A Windows INF file for the device is provided on the installation CD and
//! in the C:/StellarisWare/windows_drivers directory of StellarisWare
//! releases.  This INF contains information required to install the WinUSB
//! subsystem on WindowsXP and Vista PCs.  WinUSB is a Windows subsystem
//! allowing user mode applications to access the USB device without the need
//! for a vendor-specific kernel mode driver.
//!
//! A sample Windows command-line application, usb_bulk_example, illustrating
//! how to connect to and communicate with the bulk device is also provided.
//! The application binary is installed as part of the ``Windows-side examples
//! for USB kits'' package (SW-USB-win) on the installation CD or via download
//! from http://www.ti.com/stellarisware .  Project files are included to allow
//! the examples to be built using Microsoft VisualStudio 2008.  Source code
//! for this application can be found in directory
//! StellarisWare/tools/usb_bulk_example.
//
//*****************************************************************************


//*****************************************************************************
//
// The system tick rate expressed both as ticks per second and a millisecond
// period.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND     100
#define SYSTICK_PERIOD_MS       (1000 / SYSTICKS_PER_SECOND)

//*****************************************************************************
//
// The global system tick counter.
//
//*****************************************************************************
volatile unsigned long g_ulSysTickCount = 0;

//*****************************************************************************
//
// Variables tracking transmit and receive counts.
//
//*****************************************************************************
volatile unsigned long g_ulTxCount = 0;
volatile unsigned long g_ulRxCount = 0;
#ifdef DEBUG
unsigned long g_ulUARTRxErrors = 0;
#endif

//*****************************************************************************
//
// Debug-related definitions and declarations.
//
// Debug output is available via UART0 if DEBUG is defined during build.
//
//*****************************************************************************
#ifdef DEBUG
//*****************************************************************************
//
// Map all debug print calls to UARTprintf in debug builds.
//
//*****************************************************************************
#define DEBUG_PRINT UARTprintf

#else

//*****************************************************************************
//
// Compile out all debug print calls in release builds.
//
//*****************************************************************************
#define DEBUG_PRINT while(0) ((int (*)(char *, ...))0)
#endif

//*****************************************************************************
//
// Flags used to pass commands from interrupt context to the main loop.
//
//*****************************************************************************
#define COMMAND_PACKET_RECEIVED 0x00000001
#define COMMAND_STATUS_UPDATE   0x00000002

volatile unsigned long g_ulFlags = 0;
char *g_pcStatus;

//*****************************************************************************
//
// Global flag indicating that a USB configuration has been set.
//
//*****************************************************************************
static volatile tBoolean g_bUSBConfigured = false;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
    UARTprintf("Error at line %d of %s\n", ulLine, pcFilename);
    while(1)
    {
    }
}
#endif

//*****************************************************************************
//
// Interrupt handler for the system tick counter.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    //
    // Update our system tick counter.
    //
    g_ulSysTickCount++;
}

//*****************************************************************************
//
// Receive new data and echo it back to the host.
//
// \param psDevice points to the instance data for the device whose data is to
// be processed.
// \param pcData points to the newly received data in the USB receive buffer.
// \param ulNumBytes is the number of bytes of data available to be processed.
//
// This function is called whenever we receive a notification that data is
// available from the host. We read the data, byte-by-byte and swap the case
// of any alphabetical characters found then write it back out to be
// transmitted back to the host.
//
// \return Returns the number of bytes of data processed.
//
//*****************************************************************************
static unsigned long
EchoNewDataToHost(tUSBDBulkDevice *psDevice, unsigned char *pcData,
                  unsigned long ulNumBytes)
{
    unsigned long ulLoop, ulSpace, ulCount;
    unsigned long ulReadIndex;
    unsigned long ulWriteIndex;
    tUSBRingBufObject sTxRing;

    //
    // Get the current buffer information to allow us to write directly to
    // the transmit buffer (we already have enough information from the
    // parameters to access the receive buffer directly).
    //
    USBBufferInfoGet(&g_sTxBuffer, &sTxRing);

    //
    // How much space is there in the transmit buffer?
    //
    ulSpace = USBBufferSpaceAvailable(&g_sTxBuffer);

    //
    // How many characters can we process this time round?
    //
    ulLoop = (ulSpace < ulNumBytes) ? ulSpace : ulNumBytes;
    ulCount = ulLoop;

    //
    // Update our receive counter.
    //
    g_ulRxCount += ulNumBytes;

    //
    // Dump a debug message.
    //
    DEBUG_PRINT("Received %d bytes\n", ulNumBytes);

    //
    // Set up to process the characters by directly accessing the USB buffers.
    //
    ulReadIndex = (unsigned long)(pcData - g_pucUSBRxBuffer);
    ulWriteIndex = sTxRing.ulWriteIndex;

    while(ulLoop)
    {
        //
        // Copy from the receive buffer to the transmit buffer converting
        // character case on the way.
        //

        //
        // Is this a lower case character?
        //
        if((g_pucUSBRxBuffer[ulReadIndex] >= 'a') &&
           (g_pucUSBRxBuffer[ulReadIndex] <= 'z'))
        {
            //
            // Convert to upper case and write to the transmit buffer.
            //
            g_pucUSBTxBuffer[ulWriteIndex] =
                (g_pucUSBRxBuffer[ulReadIndex] - 'a') + 'A';
        }
        else
        {
            //
            // Is this an upper case character?
            //
            if((g_pucUSBRxBuffer[ulReadIndex] >= 'A') &&
               (g_pucUSBRxBuffer[ulReadIndex] <= 'Z'))
            {
                //
                // Convert to lower case and write to the transmit buffer.
                //
                g_pucUSBTxBuffer[ulWriteIndex] =
                    (g_pucUSBRxBuffer[ulReadIndex] - 'Z') + 'z';
            }
            else
            {
                //
                // Copy the received character to the transmit buffer.
                //
                g_pucUSBTxBuffer[ulWriteIndex] = g_pucUSBRxBuffer[ulReadIndex];
            }
        }

        //
        // Move to the next character taking care to adjust the pointer for
        // the buffer wrap if necessary.
        //
        ulWriteIndex++;
        ulWriteIndex = (ulWriteIndex == BULK_BUFFER_SIZE) ? 0 : ulWriteIndex;

        ulReadIndex++;
        ulReadIndex = (ulReadIndex == BULK_BUFFER_SIZE) ? 0 : ulReadIndex;

        ulLoop--;
    }

    //
    // We've processed the data in place so now send the processed data
    // back to the host.
    //
    USBBufferDataWritten(&g_sTxBuffer, ulCount);

    DEBUG_PRINT("Wrote %d bytes\n", ulCount);

    //
    // We processed as much data as we can directly from the receive buffer so
    // we need to return the number of bytes to allow the lower layer to
    // update its read pointer appropriately.
    //
    return(ulCount);
}

//*****************************************************************************
//
// Handles bulk driver notifications related to the transmit channel (data to
// the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
// related to operation of the transmit data channel (the IN channel carrying
// data to the USB host).
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
TxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData)
{
    //
    // We are not required to do anything in response to any transmit event
    // in this example. All we do is update our transmit counter.
    //
    if(ulEvent == USB_EVENT_TX_COMPLETE)
    {
        g_ulTxCount += ulMsgValue;
    }

    //
    // Dump a debug message.
    //
    DEBUG_PRINT("TX complete %d\n", ulMsgValue);

    return(0);
}

//*****************************************************************************
//
// Handles bulk driver notifications related to the receive channel (data from
// the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
// related to operation of the receive data channel (the OUT channel carrying
// data from the USB host).
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
RxHandler(void *pvCBData, unsigned long ulEvent,
               unsigned long ulMsgValue, void *pvMsgData)
{
    //
    // Which event are we being sent?
    //
    switch(ulEvent)
    {
        //
        // We are connected to a host and communication is now possible.
        //
        case USB_EVENT_CONNECTED:
        {
            g_bUSBConfigured = true;
            UARTprintf("Host connected.\n");

            //
            // Flush our buffers.
            //
            USBBufferFlush(&g_sTxBuffer);
            USBBufferFlush(&g_sRxBuffer);

            break;
        }

        //
        // The host has disconnected.
        //
        case USB_EVENT_DISCONNECTED:
        {
            g_bUSBConfigured = false;
            UARTprintf("Host disconnected.\n");
            break;
        }

        //
        // A new packet has been received.
        //
        case USB_EVENT_RX_AVAILABLE:
        {
            tUSBDBulkDevice *psDevice;

            //
            // Get a pointer to our instance data from the callback data
            // parameter.
            //
            psDevice = (tUSBDBulkDevice *)pvCBData;

            //
            // Read the new packet and echo it back to the host.
            //
            return(EchoNewDataToHost(psDevice, pvMsgData, ulMsgValue));
        }

        //
        // Ignore SUSPEND and RESUME for now.
        //
        case USB_EVENT_SUSPEND:
        case USB_EVENT_RESUME:
        {
            break;
        }

        //
        // Ignore all other events and return 0.
        //
        default:
        {
            break;
        }
    }

    return(0);
}

/** @region "WinUSB auto-load routines" */

/**
Windows is going to come asking for this special string descriptor.  If we say the magic words when it does,
we can get the OS to self-install the WinUSB.sys driver for us!
*/
#define MS_OS_STRING_DESCRIPTOR 0xEE

/**
When Windows does ask for the 0xEE descriptor, in our response we must tell it what EP0 Vendor Request number
to use in a follow-up get request.
Windows doesn't care what the number is. It doesn't really matter what it is... something arbitrary.
*/
#define VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR 7

/**
Transmit the argument buffer to the host, using the smaller of the buffer size or the length specified by the host.
*/
static void SendEP0Data(unsigned char * pbySendBuffer, unsigned uBufferBytes, tUSBRequest *pUSBRequest)
{
    const unsigned long ulSize = pUSBRequest->wLength < uBufferBytes ? pUSBRequest->wLength : uBufferBytes;
    UARTprintf("Sending %u bytes\n", ulSize);

    USBDCDSendDataEP0(0, pbySendBuffer, ulSize);
}

/**
Inspect the EP0 Vendor Request details and handle according to what is recognized.
If the request is indeed handled, pbySendBuffer_out will be set to the buffer to send to the host, along with
puBufferBytes_out indicating the number of bytes to send.
Otherwise, these two values will not be changed.

We are expecting the "Microsoft Compatible ID Feature Descriptor" request, for which
we'll respond with the magic "WINUSB" descriptor, as well as the "Microsoft Extended Properties Feature Descriptor"
request that we will respond with the DeviceInterfaceGUID.
*/
static void DispatchVendorRequest(tUSBRequest *pUSBRequest, unsigned char ** pbySendBuffer_out, unsigned * puBufferBytes_out)
{
	if((pUSBRequest->bmRequestType & USB_RTYPE_TYPE_M) != USB_RTYPE_VENDOR) return;

	if(VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR == pUSBRequest->bRequest &&
		4 == pUSBRequest->wIndex &&
		USB_RTYPE_DEVICE == (pUSBRequest->bmRequestType & USB_RTYPE_RECIPIENT_M))
	{
		UARTprintf("Sending Microsoft Compatible ID Feature Descriptor 'WINUSB'\n");

		static unsigned char abyCIDFDesc[] =
		{
			0x28, 0x00, 0x00, 0x00,	// DWORD (LE)	 Descriptor length (40 bytes)
			0x00, 0x01,	 			// BCD WORD (LE)	 Version ('1.0')
			0x04, 0x00,				// WORD (LE)	 Compatibility ID Descriptor index (0x0004)
			0x01, 					// BYTE	 Number of sections (1)
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 7 BYTES	 Reserved
			0x00, //	 BYTE	 Interface Number (Interface #0)
			0x01, //	 BYTE	 Reserved
			0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, //8 BYTES ASCII String Compatible ID ("WINUSB\0\0")
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8 BYTES ASCII String	 Sub-Compatible ID (unused)
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // 6 BYTES Reserved
		};

		*pbySendBuffer_out = abyCIDFDesc;
		*puBufferBytes_out = sizeof abyCIDFDesc;
	}
	else if(VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR == pUSBRequest->bRequest &&
		5 == pUSBRequest->wIndex &&
		USB_RTYPE_INTERFACE == (pUSBRequest->bmRequestType & USB_RTYPE_RECIPIENT_M))
	{
		UARTprintf("Sending Microsoft Extended Properties Feature Descriptor\n");

		// This sends the Device Interface GUID from TI's usb_dev_bulk.inf
		//
		static unsigned char abyEPFDesc[] =
		{
			0x92, 0x00, 0x00, 0x00,	// DWORD (LE)	 Descriptor length (146 bytes)
			0x00, 0x01,	 			// BCD WORD (LE) Version ('1.0')
			0x05, 0x00,				// WORD (LE)	 Extended Property Descriptor index (0x0005)
			0x01, 0x00,				// WORD          Number of sections (1)
			0x88, 0x00, 0x00, 0x00,	// DWORD (LE)	 Size of the property section (136 bytes)
			0x07, 0x00, 0x00, 0x00,	// DWORD (LE)	 Property data type (7 = Unicode REG_MULTI_SZ)
			0x2A, 0x00,				// WORD (LE)	 Property name length (42 bytes)
									// NULL-terminated Unicode String (LE)	 Property Name (L"DeviceInterfaceGUIDs")
			'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0, 0x00, 0x00,
			0x50, 0x00, 0x00, 0x00,	// DWORD (LE)	 Property data length (80 bytes)

									// NULL-terminated Unicode String (LE), followed by another Unicode NULL
									// Property Name ("{6E45736A-2B1B-4078-B772-B3AF2B6FDE1C}")
			'{',0,'6',0,'E',0,'4',0,'5',0,'7',0,'3',0,'6',0,'A',0,'-',0,'2',0,'B',0,'1',0,'B',0,'-',0,'4',0,'0',0,'7',0,'8',0,'-',0,'B',0,'7',0,'7',0,'2',0,'-',0,'B',0,'3',0,'A',0,'F',0,'2',0,'B',0,'6',0,'F',0,'D',0,'E',0,'1',0,'C',0,'}',0, 0x00,0x00, 0x00,0x00
		};

		*pbySendBuffer_out = abyEPFDesc;
		*puBufferBytes_out = sizeof abyEPFDesc;
	}
}

/**
This handler will be invoked by usblib whenever the host performs a Vendor request.
We are expecting the "Microsoft Compatible ID Feature Descriptor" request, for which
we'll respond with the magic "WINUSB" descriptor, as well as the "Microsoft Extended Properties Feature Descriptor"
request that we will respond with the DeviceInterfaceGUID.
*/
static void VendorRequestHandler(void *pvInstance, tUSBRequest *pUSBRequest)
{
	unsigned char * pbySendBuffer = 0;
	unsigned uBufferBytes = 0;

	UARTprintf("Received Vendor request: Type=0x%X Request=0x%X Value=0x%X Index=0x%X Length=0x%X\n",
			pUSBRequest->bmRequestType,
			pUSBRequest->bRequest,
			pUSBRequest->wValue,
			pUSBRequest->wIndex,
			pUSBRequest->wLength);

	MAP_USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);

	DispatchVendorRequest(pUSBRequest, &pbySendBuffer, &uBufferBytes);
	if(pbySendBuffer && uBufferBytes)
	{
		SendEP0Data(pbySendBuffer, uBufferBytes, pUSBRequest);
	}
	else
	{
		USBDCDStallEP0(0);
	}
}

/**
This handler is invoked from usblib whenever there is a request for a string descriptor who's index
is not within the predefined table.
This callback is not a standard part of usblib, but is proposed as such for allowing an elegant solution
to responding to the 0xEE MS OS String Descriptor.

The sole purpose of this handler in this demonstration is to recognize the 0xEE MS OS String Descriptor
request and give Windows what it wants.
 */
static void GetStringDescriptorHandler(void *pvInstance, tUSBRequest *pUSBRequest)
{
	UARTprintf("Received String Descriptor request: 0x%X\n", pUSBRequest->wValue);

	if((pUSBRequest->wValue & 0xFF) != MS_OS_STRING_DESCRIPTOR)
	{
		USBDCDStallEP0(0);
		return;
	}

	UARTprintf("Sending MS OS String Descriptor 'MSFT100'\n");

    static unsigned char abyOsDescriptor[] =
    {
    	0x12, // Descriptor length (18 bytes)
    	0x03, // Descriptor type (3 = String)
    	0x4D, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, // Signature: "MSFT100"
    	VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR, // Vendor Code
    	0x00  // Padding
    };

 	SendEP0Data(abyOsDescriptor, sizeof abyOsDescriptor, pUSBRequest);
}

/**
This kludge hijacks the USB version in usblib from 1.1 to 2.0, otherwise Windows will never bother to ask for the 0xEE
OS String Descriptor.
It doesn't make sense that usblib defaults to 1.1.  Certainly you can have FS devices under 2.0 spec!
At the very least, usblib should allow a more elegant way to set this... but really I think it should just set it
to 2.0 and be done with it.
*/
static void ConfigureUsb200()
{
	unsigned short * const pwUsbVersion = (unsigned short *)(g_sBulkDeviceInfo.pDeviceDescriptor + 2);
	*pwUsbVersion = 0x200;
}

/**
This will setup the callbacks needed to handle Window's attempts to get the MS OS String Descriptor and
Microsoft Compatible ID Feature Descriptor that ultimately leads to the automatic installation of the
WinUSB.sys driver.
 */
static void ConfigureAutoWinUsbInstall()
{
	ConfigureUsb200();
	g_sBulkDeviceInfo.sCallbacks.pfnRequestHandler = VendorRequestHandler;
	g_sBulkDeviceInfo.sCallbacks.pfnGetStringDescriptor = GetStringDescriptorHandler;
}

/** @endregion */

//*****************************************************************************
//
// This is the main application entry function.
//
//*****************************************************************************
int
main(void)
{
    volatile unsigned long ulLoop;
    unsigned long ulTxCount;
    unsigned long ulRxCount;

    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    ROM_FPULazyStackingEnable();

    //
    // Set the clocking to run from the PLL at 50MHz
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    //
    // Configure the relevant pins such that UART0 owns them.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //
    // Enable the GPIO pins for the LED (PF2 & PF3).  
    //
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3|GPIO_PIN_2);

    //
    // Open UART0 and show the application name on the UART.
    //
    UARTStdioInit(0);
    UARTprintf("\033[2JStellaris USB bulk device example\n");
    UARTprintf("---------------------------------\n\n");

    //
    // Not configured initially.
    //
    g_bUSBConfigured = false;

    //
    // Enable the GPIO peripheral used for USB, and configure the USB
    // pins.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    ROM_GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    //
    // Enable the system tick.
    //
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / SYSTICKS_PER_SECOND);
    ROM_SysTickIntEnable();
    ROM_SysTickEnable();

    //
    // Tell the user what we are up to.
    //
    UARTprintf("Configuring USB\n");

    //
    // Initialize the transmit and receive buffers.
    //
    USBBufferInit((tUSBBuffer *)&g_sTxBuffer);
    USBBufferInit((tUSBBuffer *)&g_sRxBuffer);

    //
    // Set the USB stack mode to Device mode with VBUS monitoring.
    //
    USBStackModeSet(0, USB_MODE_FORCE_DEVICE, 0);

    ConfigureAutoWinUsbInstall();

    //
    // Pass our device information to the USB library and place the device
    // on the bus.
    //
    USBDBulkInit(0, (tUSBDBulkDevice *)&g_sBulkDevice);

    //
    // Wait for initial configuration to complete.
    //
    UARTprintf("Waiting for host...\n");

    //
    // Clear our local byte counters.
    //
    ulRxCount = 0;
    ulTxCount = 0;

    //
    // Main application loop.
    //
    while(1)
    {
        //
        // See if any data has been transferred.
        //
        if((ulTxCount != g_ulTxCount) || (ulRxCount != g_ulRxCount))
        {
            //
            // Has there been any transmit traffic since we last checked?
            //
            if(ulTxCount != g_ulTxCount)
            {
                //
                // Turn on the Green LED.
                //
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);

                //
                // Delay for a bit.
                //
                for(ulLoop = 0; ulLoop < 150000; ulLoop++)
                {
                }
            
                //
                // Turn off the Green LED.
                //
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);
                
                //
                // Take a snapshot of the latest transmit count.
                //
                ulTxCount = g_ulTxCount;
            }

            //
            // Has there been any receive traffic since we last checked?
            //
            if(ulRxCount != g_ulRxCount)
            {
                //
                // Turn on the Blue LED.
                //
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);

                //
                // Delay for a bit.
                //
                for(ulLoop = 0; ulLoop < 150000; ulLoop++)
                {
                }
            
                //
                // Turn off the Blue LED.
                //
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);

                //
                // Take a snapshot of the latest receive count.
                //
                ulRxCount = g_ulRxCount;
            }

            //
            // Update the display of bytes transferred.
            //
            UARTprintf("\rTx: %d  Rx: %d", ulTxCount, ulRxCount);
        }
    }
}
