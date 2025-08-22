/*******************************************************************************
Copyright 2016 Microchip Technology Inc. (www.microchip.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

To request to license the code under the MLA license (www.microchip.com/mla_license), 
please contact mla_licensing@microchip.com
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdint.h>
#include <string.h>

#include "system.h"
#include "usb.h"
#include "usb_device_hid.h"

#include "app_led_usb_status.h"
#include "encoder.h"

#if defined(__XC8)
    #define PACKED
#else
    #define PACKED __attribute__((packed))
#endif

// *****************************************************************************
// *****************************************************************************
// Section: File Scope or Global Constants
// *****************************************************************************
// *****************************************************************************

//Class specific descriptor - HID Consumer Device
const struct{uint8_t report[HID_RPT01_SIZE];}hid_rpt01={
{   0x05, 0x0C, /*		Usage Page (Consumer Devices)		*/
	0x09, 0x01, /*		Usage (Consumer Control)			*/
	0xA1, 0x01, /*		Collection (Application)			*/
	0x85, 0x01,	/*		Report ID=1							*/
	0x05, 0x0C, /*		Usage Page (Consumer Devices)		*/
	0x15, 0x00, /*		Logical Minimum (0)					*/
	0x25, 0x01, /*		Logical Maximum (1)					*/
	0x75, 0x01, /*		Report Size (1)						*/
	0x95, 0x07, /*		Report Count (7)					*/
	0x09, 0xB5, /*		Usage (Scan Next Track)				*/
	0x09, 0xB6, /*		Usage (Scan Previous Track)			*/
	0x09, 0xB7, /*		Usage (Stop)						*/
	0x09, 0xCD, /*		Usage (Play / Pause)				*/
	0x09, 0xE2, /*		Usage (Mute)						*/
	0x09, 0xE9, /*		Usage (Volume Up)					*/
	0x09, 0xEA, /*		Usage (Volume Down)					*/
	0x81, 0x02, /*		Input (Data, Variable, Absolute)	*/
	0x95, 0x01, /*		Report Count (1)					*/
	0x81, 0x01, /*		Input (Constant)					*/
	0xC0}		/*		End Collection						*/
};


// *****************************************************************************
// *****************************************************************************
// Section: File Scope Data Types
// *****************************************************************************
// *****************************************************************************


typedef struct PACKED      // ------------------------------------------------------------------------------------------------------
{
    uint8_t reportID;        // Always 0x01 for consumer reports
    union PACKED
    {
        uint8_t value;
        struct PACKED
        {
            unsigned scanNextTrack     :1;  // Usage 0xB5
            unsigned scanPrevTrack     :1;  // Usage 0xB6
            unsigned stop              :1;  // Usage 0xB7
            unsigned playPause         :1;  // Usage 0xCD
            unsigned mute              :1;  // Usage 0xE2
            unsigned volumeUp          :1;  // Usage 0xE9
            unsigned volumeDown        :1;  // Usage 0xEA
            unsigned                   :1;  // Padding bit
        } bits;
    } controls;
} CONSUMER_INPUT_REPORT;

// *****************************************************************************
// *****************************************************************************
// Section: File Scope or Global Variables
// *****************************************************************************
// *****************************************************************************

static CONSUMER_INPUT_REPORT consumerReport CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG;


// *****************************************************************************
// *****************************************************************************
// Section: Private Prototypes
// *****************************************************************************
// *****************************************************************************


//Exteranl variables declared in other .c files
extern volatile signed int SOFCounter;


//Application variables that need wide scope
CONSUMER_INPUT_REPORT oldconsumerReport;
signed int LocalSOFCount;
static signed int OldSOFCount;

static USB_HANDLE lastConsumerTransmission;




// *****************************************************************************
// *****************************************************************************
// Section: Macros or Functions
// *****************************************************************************
// *****************************************************************************
void APP_ConsumerInit(void)
{
    //initialize the variable holding the handle for the last transmission
    lastConsumerTransmission = 0;
    
    //Copy the (possibly) interrupt context SOFCounter value into a local variable.
    //Using a while() loop to do this since the SOFCounter isn't necessarily atomically
    //updated and therefore we need to read it a minimum of twice to ensure we captured the correct value.
    while(OldSOFCount != SOFCounter)
    {
        OldSOFCount = SOFCounter;
    }

    //enable the HID endpoint (IN only for consumer device)
    USBEnableEndpoint(HID_EP, USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);

    //Initialize the quadrature encoder
    ENCODER_Initialize();
}

void APP_ConsumerTasks(void)
{
    unsigned char i;
    bool needToSendNewReportPacket_consumer;

    /* If the USB device isn't configured yet, we can't really do anything
     * else since we don't have a host to talk to.  So jump back to the
     * top of the while loop. */
    if( USBGetDeviceState() < CONFIGURED_STATE )
    {
        return;
    }
    
    /* Update encoder state machine */
    ENCODER_Task();

    /* If we are currently suspended, then we need to see if we need to
     * issue a remote wakeup.  In either case, we shouldn't process any
     * consumer commands since we aren't currently communicating to the host
     * thus just continue back to the start of the while loop. */
    if( USBIsDeviceSuspended()== true )
    {
        return;
    }

    /* Check if the IN endpoint is busy, and if it isn't check if we want to send
     * consumer data to the host. */
    if(HIDTxHandleBusy(lastConsumerTransmission) == false)
    {
        /* Clear the INPUT report buffer.  Set to all zeros. */
        memset(&consumerReport, 0, sizeof(consumerReport));
        consumerReport.reportID = 0x01;  // Consumer reports now use Report ID 1

        // Handle quadrature encoder for volume control
        ENCODER_DIRECTION encoder_dir = ENCODER_GetDirection();

        if(encoder_dir == ENCODER_CW)
        {
            /* Set volume up */
            consumerReport.reportID = 0x01;
            consumerReport.controls.value = 0;  // Clear all bits first
            consumerReport.controls.bits.volumeUp = 1;  // Set volume up bit
        }
        else if(encoder_dir == ENCODER_CCW)
        {
            /* Set volume down */
            consumerReport.reportID = 0x01;
            consumerReport.controls.value = 0;  // Clear all bits first
            consumerReport.controls.bits.volumeDown = 1;  // Set volume down bit
        }

        //Check to see if the new packet contents are somehow different from the most
        //recently sent packet contents.
        needToSendNewReportPacket_consumer = false;
        for(i = 0; i < sizeof(consumerReport); i++)
        {
            if(*((uint8_t*)&oldconsumerReport + i) != *((uint8_t*)&consumerReport + i))
            {
                needToSendNewReportPacket_consumer = true;
                break;
            }
        }

        //Now send the new input report packet, if it is appropriate to do so (ex: new data is present).
        if(needToSendNewReportPacket_consumer == true)
        {
            //Save the old input report packet contents.  We do this so we can detect changes in report packet content
            //useful for determining when something has changed and needs to get re-sent to the host.
            oldconsumerReport = consumerReport;

            /* Send the consumer report packet over USB to the host. */
            lastConsumerTransmission = HIDTxPacket(HID_EP, (uint8_t*)&consumerReport, sizeof(consumerReport));
        }
    }
    
    return;		
}



/*******************************************************************************
 End of File
*/
