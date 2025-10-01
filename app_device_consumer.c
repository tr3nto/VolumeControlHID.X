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
#include "buttons.h"
#include "io_mapping.h"
#include "leds.h"

#if defined(__XC8)
    #define PACKED
#else
    #define PACKED __attribute__((packed))
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Device Mode Definitions
// *****************************************************************************
// *****************************************************************************

typedef enum {
    DEVICE_MODE_VOLUME_CONTROL,
    DEVICE_MODE_SCROLL_WHEEL
} DEVICE_MODE;

static DEVICE_MODE currentMode = DEVICE_MODE_VOLUME_CONTROL;
static bool lastButtonState = false;
static bool buttonPressed = false;

// *****************************************************************************
// *****************************************************************************
// Section: File Scope or Global Constants
// *****************************************************************************
// *****************************************************************************

//Class specific descriptor - HID Consumer Device (ORIGINAL WORKING VERSION)
const struct{uint8_t report[HID_RPT01_SIZE];}hid_rpt01={
{   0x05, 0x0C, /*  Usage Page (Consumer Devices)      */
    0x09, 0x01, /*  Usage (Consumer Control)           */
    0xA1, 0x01, /*  Collection (Application)           */
    0x85, 0x01, /*  Report ID=1                        */
    0x05, 0x0C, /*  Usage Page (Consumer Devices)      */
    0x15, 0x00, /*  Logical Minimum (0)                */
    0x25, 0x01, /*  Logical Maximum (1)                */
    0x75, 0x01, /*  Report Size (1)                    */
    0x95, 0x07, /*  Report Count (7)                   */
    0x09, 0xB5, /*  Usage (Scan Next Track)            */
    0x09, 0xB6, /*  Usage (Scan Previous Track)        */
    0x09, 0xB7, /*  Usage (Stop)                       */
    0x09, 0xCD, /*  Usage (Play / Pause)               */
    0x09, 0xE2, /*  Usage (Mute)                       */
    0x09, 0xE9, /*  Usage (Volume Up)                  */
    0x09, 0xEA, /*  Usage (Volume Down)                */
    0x81, 0x02, /*  Input (Data, Variable, Absolute)   */
    0x95, 0x01, /*  Report Count (1)                   */
    0x81, 0x01, /*  Input (Constant)                   */
    0xC0}       /*  End Collection                     */
};

//Standard 6-byte keyboard report descriptor  
const struct{uint8_t report[HID_RPT02_SIZE];}hid_rpt02={
{   0x05, 0x01, /*  Usage Page (Generic Desktop)       */
    0x09, 0x06, /*  Usage (Keyboard)                    */
    0xA1, 0x01, /*  Collection (Application)           */
    0x05, 0x07, /*    Usage Page (Keyboard/Keypad)      */
    0x19, 0x00, /*    Usage Minimum (0)                 */
    0x29, 0xFF, /*    Usage Maximum (255)               */
    0x15, 0x00, /*    Logical Minimum (0)               */
    0x26, 0xFF, 0x00, /*  Logical Maximum (255)           */
    0x75, 0x08, /*    Report Size (8)                   */
    0x95, 0x06, /*    Report Count (6)                  */
    0x81, 0x00, /*    Input (Data,Array,Abs)            */
    0xC0}       /*  End Collection                      */
};


// *****************************************************************************
// *****************************************************************************
// Section: File Scope Data Types
// *****************************************************************************
// *****************************************************************************


typedef struct PACKED
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

typedef struct PACKED
{
    /* Standard 6-byte keyboard report (like real keyboards)
     * 6 bytes containing scan codes of pressed keys (0 = no key)
     */
    uint8_t keys[6];  // Up to 6 simultaneous key presses
} KEYBOARD_REPORT;

typedef struct
{
    bool sentStop;
    bool lastButtonState;
    uint8_t vectorPosition;
    uint16_t movementCount;
    bool movementMode;
    bool keyPressed;           // Track if a key is currently pressed
    uint8_t keyReleaseCount;   // Counter for key release timing
    
    struct
    {
        USB_HANDLE handle;
        uint8_t idleRate;
        uint8_t idleRateSofCount;
    } inputReport[1];

} KEYBOARD;


// *****************************************************************************
// *****************************************************************************
// Section: File Scope or Global Variables
// *****************************************************************************
// *****************************************************************************

#if !defined(CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG)
    #define CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG
#endif

#if !defined(KEYBOARD_REPORT_DATA_BUFFER_ADDRESS_TAG)
    #define KEYBOARD_REPORT_DATA_BUFFER_ADDRESS_TAG
#endif

static CONSUMER_INPUT_REPORT consumerReport CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG;
static KEYBOARD_REPORT keyboardReport KEYBOARD_REPORT_DATA_BUFFER_ADDRESS_TAG;
static KEYBOARD keyboard;


// *****************************************************************************
// *****************************************************************************
// Section: Private Prototypes
// *****************************************************************************
// *****************************************************************************
static void APP_HandleVolumeControl(void);
static void APP_HandleScrollWheel(void);
static void APP_DeviceKeyboardInitialize(void);


//Exteranl variables declared in other .c files
extern volatile signed int SOFCounter;


//Application variables that need wide scope
CONSUMER_INPUT_REPORT oldconsumerReport;
KEYBOARD_REPORT oldkeyboardReport;
signed int LocalSOFCount;
static signed int OldSOFCount;

static USB_HANDLE lastConsumerTransmission;
static USB_HANDLE lastKeyboardTransmission;




// *****************************************************************************
// *****************************************************************************
// Section: Macros or Functions
// *****************************************************************************
// *****************************************************************************

/*********************************************************************
* Function: void APP_HandleModeSwitch(void);
*
* Overview: Handles mode switching between volume control and scroll wheel
*
* PreCondition: Button system must be initialized
*
* Input: None
*
* Output: None
*
********************************************************************/
void APP_HandleModeSwitch(void)
{
    bool currentButtonState = BUTTON_IsPressed(BUTTON_MODE_SWITCH);
    
    // Detect button press (transition from not pressed to pressed)
    if (currentButtonState && !lastButtonState) {
        // Toggle mode
        if (currentMode == DEVICE_MODE_VOLUME_CONTROL) {
            currentMode = DEVICE_MODE_SCROLL_WHEEL;
            // LED indicator: D3 ON = Scroll mode
            LED_On(LED_D3);
            LED_Off(LED_D2);
        } else {
            currentMode = DEVICE_MODE_VOLUME_CONTROL;
            // LED indicator: D2 ON = Volume mode  
            LED_Off(LED_D3);
            LED_On(LED_D2);
        }
    }
    
    // Update last button state for next iteration
    lastButtonState = currentButtonState;
}

void APP_ConsumerInit(void)
{
    //initialize the variable holding the handle for the last transmission
    lastConsumerTransmission = 0;
    lastKeyboardTransmission = 0;
    
    //Copy the (possibly) interrupt context SOFCounter value into a local variable.
    //Using a while() loop to do this since the SOFCounter isn't necessarily atomically
    //updated and therefore we need to read it a minimum of twice to ensure we captured the correct value.
    while(OldSOFCount != SOFCounter)
    {
        OldSOFCount = SOFCounter;
    }

    //enable the HID endpoints
    USBEnableEndpoint(HID_EP, USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);  // Consumer control
    USBEnableEndpoint(2, USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);       // Keyboard scroll wheel

    //Initialize the quadrature encoder
    ENCODER_Initialize();
    
    //Enable the mode switch button
    BUTTON_Enable(BUTTON_MODE_SWITCH);
    
    //Initialize keyboard functionality
    APP_DeviceKeyboardInitialize();
    
    //Initialize LEDs for mode indication
    LED_Enable(LED_D3);  // Scroll mode indicator
    LED_Enable(LED_D2);  // Volume mode indicator
    
    //Set initial mode indicator (starts in volume mode)
    LED_Off(LED_D3);
    LED_On(LED_D2);
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
    
    /* Handle mode switching */
    APP_HandleModeSwitch();

    /* If we are currently suspended, then we need to see if we need to
     * issue a remote wakeup.  In either case, we shouldn't process any
     * consumer commands since we aren't currently communicating to the host
     * thus just continue back to the start of the while loop. */
    if( USBIsDeviceSuspended()== true )
    {
        return;
    }
    
    /* Handle current mode functionality */
    if (currentMode == DEVICE_MODE_VOLUME_CONTROL) {
        APP_HandleVolumeControl();
    } else {
        APP_HandleScrollWheel();
    }
    
    return;		
}

/*********************************************************************
* Function: static void APP_HandleVolumeControl(void);
*
* Overview: Handles volume control mode using encoder input
*
* PreCondition: USB device must be configured
*
* Input: None
*
* Output: None
*
********************************************************************/
static void APP_HandleVolumeControl(void)
{
    unsigned char i;
    bool needToSendNewReportPacket_consumer;
    
    // DEBUG: Ensure LED_D2 is on when volume control handler runs
    LED_On(LED_D2);
    LED_Off(LED_D3);
    
    /* Check if the IN endpoint is busy, and if it isn't check if we want to send
     * consumer data to the host. */
    if(HIDTxHandleBusy(lastConsumerTransmission) == false)
    {
        /* Clear the INPUT report buffer.  Set to all zeros. */
        memset(&consumerReport, 0, sizeof(consumerReport));
        consumerReport.reportID = 0x01;  // Consumer reports use Report ID 1

        // Handle quadrature encoder for volume control
        ENCODER_DIRECTION encoder_dir = ENCODER_GetDirection();

        if(encoder_dir == ENCODER_CW)
        {
            /* Set volume up */
            consumerReport.controls.value = 0;  // Clear all bits first
            consumerReport.controls.bits.volumeUp = 1;  // Set volume up bit
        }
        else if(encoder_dir == ENCODER_CCW)
        {
            /* Set volume down */
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
}

/*********************************************************************
* Function: static void APP_HandleScrollWheel(void);
*
* Overview: Handles scroll wheel mode using encoder input
*
* PreCondition: USB device must be configured
*
* Input: None
*
* Output: None
*
********************************************************************/
static void APP_HandleScrollWheel(void)
{
    unsigned char i;
    bool needToSendNewReportPacket_keyboard;
    
    // DEBUG: Keep LED_D3 solidly on to show keyboard mode
    LED_On(LED_D3);
    LED_Off(LED_D2);
    
    /* Check if the keyboard endpoint is busy, and if it isn't check if we want to send
     * scroll wheel data to the host. */
    if(HIDTxHandleBusy(lastKeyboardTransmission) == false)
    {
        /* Clear the keyboard report buffer.  Set to all zeros. */
        memset(&keyboardReport, 0, sizeof(keyboardReport));

        // Handle quadrature encoder for arrow key scrolling with proper press/release
        ENCODER_DIRECTION encoder_dir = ENCODER_GetDirection();
        
        // DEBUG: Toggle LED_D2 when encoder movement is detected (any direction)
        if(encoder_dir != ENCODER_NONE) {
            LED_Toggle(LED_D2);   // Toggle to show encoder was detected
        }
        

        // Always clear all keys first
        memset(keyboardReport.keys, 0, sizeof(keyboardReport.keys));
        needToSendNewReportPacket_keyboard = false;
        
        if(encoder_dir == ENCODER_CW || encoder_dir == ENCODER_CCW)
        {            
            // New key press detected
            if(!keyboard.keyPressed) {
                // Send key press using standard HID scan codes
                if(encoder_dir == ENCODER_CW) {
                    keyboardReport.keys[0] = 0x52;  // Up Arrow scan code
                } else {
                    keyboardReport.keys[0] = 0x51;  // Down Arrow scan code
                }
                keyboard.keyPressed = true;
                keyboard.keyReleaseCount = 3;  // Release after a few cycles
                needToSendNewReportPacket_keyboard = true;
            }
        }
        else
        {
            // No encoder movement - handle key release timing
            if(keyboard.keyPressed) {
                if(keyboard.keyReleaseCount > 0) {
                    keyboard.keyReleaseCount--;
                    if(keyboard.keyReleaseCount == 0) {
                        // Time to send key release (all keys already cleared to 0)
                        keyboard.keyPressed = false;
                        needToSendNewReportPacket_keyboard = true;
                    }
                }
            }
        }


        //Now send the new input report packet, if it is appropriate to do so (ex: new data is present).
        if(needToSendNewReportPacket_keyboard == true)
        {
            //Save the old input report packet contents.  We do this so we can detect changes in report packet content
            //useful for determining when something has changed and needs to get re-sent to the host.
            oldkeyboardReport = keyboardReport;

            /* Send the keyboard report packet over USB to the host using endpoint 2 */
            lastKeyboardTransmission = HIDTxPacket(2, (uint8_t*)&keyboardReport, sizeof(keyboardReport));
        }
    }
}


static void APP_DeviceKeyboardInitialize(void)
{
    /* Initialize keyboard variables for scroll wheel functionality */
    keyboard.inputReport[0].handle = NULL;
    keyboard.sentStop = false;
    keyboard.movementCount = 0;
    keyboard.movementMode = true;
    keyboard.keyPressed = false;
    keyboard.keyReleaseCount = 0;
}//end UserInit

/*********************************************************************
* Function: void APP_DeviceMouseTasks(void);
*
* Overview: Placeholder function for keyboard tasks - scroll wheel 
*           functionality is now handled in APP_HandleScrollWheel()
*
* PreCondition: None
*
* Input: None
*
* Output: None
*
********************************************************************/
void APP_DeviceMouseTasks(void)
{
    // This function is no longer needed since scroll wheel functionality
    // is handled directly in APP_HandleScrollWheel()
    // Kept for compatibility with existing function calls
}//end ProcessIO

/*********************************************************************
* Function: void APP_DeviceMouseSOFHandler(void);
*
* Overview: Placeholder SOF handler for keyboard functionality
*
* PreCondition: None
*
* Input: None
*
* Output: None
*
********************************************************************/
void APP_DeviceMouseSOFHandler(void)
{
    // Placeholder function for SOF handling
    // Not needed for scroll wheel functionality
}

/*******************************************************************************
 End of File
*/
