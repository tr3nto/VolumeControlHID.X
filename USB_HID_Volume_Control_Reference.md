# USB HID Volume Control Project - Technical Reference

## Project Overview
This is a USB HID (Human Interface Device) project for a Microchip PIC18F microcontroller that implements a consumer device for volume control using a quadrature encoder. The device appears to the host computer as a USB HID consumer control device, similar to multimedia keys on keyboards.

### Hardware Requirements
- **Microcontroller**: PIC18F series with USB support
- **Crystal**: 20MHz (configured in system.c:35-50)
- **USB**: Full-speed USB 2.0 device
- **Encoder**: Quadrature rotary encoder connected to RB3 (Channel A) and RB4 (Channel B)

### Key Project Files
- `usb_descriptors.c` - USB device, configuration, and string descriptors
- `app_device_consumer.c/h` - Main application logic and HID report descriptor
- `encoder.c/h` - Quadrature encoder driver with state machine
- `usb_config.h` - USB stack configuration parameters
- `main.c` - Main application entry point and loop

## USB Descriptors Structure

### Device Descriptor (`usb_descriptors.c:38-54`)
```c
const USB_DEVICE_DESCRIPTOR device_dsc = {
    0x12,                    // Descriptor length (18 bytes)
    USB_DESCRIPTOR_DEVICE,   // Device descriptor type
    0x0200,                  // USB 2.0 specification
    0x00, 0x00, 0x00,       // Class/Subclass/Protocol (defined at interface level)
    USB_EP0_BUFF_SIZE,       // Max packet size for endpoint 0 (8 bytes)
    MY_VID, MY_PID,         // Vendor/Product ID (0x04D8/0x0055 - Microchip)
    0x0001,                  // Device version 0.1
    0x01, 0x02, 0x00,       // String descriptor indices
    0x01                     // Number of configurations
};
```

**Key Parameters:**
- **VID/PID**: 0x04D8/0x0055 (Microchip Technology Inc.)
- **USB Version**: 2.0
- **Max Power**: 500mA (250 × 2 as defined in config descriptor)

### Configuration Descriptor (`usb_descriptors.c:57-96`)
```c
const uint8_t configDescriptor1[] = {
    /* Configuration Descriptor */
    0x09, USB_DESCRIPTOR_CONFIGURATION,
    DESC_CONFIG_WORD(0x0022),   // Total length: 34 bytes
    1,                          // Number of interfaces: 1
    1,                          // Configuration value: 1
    0,                          // Configuration string index: none
    _DEFAULT | _SELF,           // Attributes: self-powered
    250,                        // Max power: 500mA (250 × 2)

    /* Interface Descriptor */
    0x09, USB_DESCRIPTOR_INTERFACE,
    0,                          // Interface number: 0
    0,                          // Alternate setting: 0
    1,                          // Number of endpoints: 1 (excluding EP0)
    HID_INTF,                   // Interface class: HID (0x03)
    0x00,                       // Subclass: none
    0x00,                       // Protocol: none
    0,                          // Interface string index: none

    /* HID Class-Specific Descriptor */
    0x09, DSC_HID,              // HID descriptor type
    DESC_CONFIG_WORD(0x0111),   // HID version: 1.11
    0x00,                       // Country code: not supported
    HID_NUM_OF_DSC,             // Number of class descriptors: 1
    DSC_RPT,                    // Report descriptor type
    DESC_CONFIG_WORD(39),       // Report descriptor size: 39 bytes
    
    /* Endpoint Descriptor */
    0x07, USB_DESCRIPTOR_ENDPOINT,
    HID_EP | _EP_IN,            // Endpoint address: EP1 IN
    _INTERRUPT,                 // Transfer type: interrupt
    DESC_CONFIG_WORD(2),        // Max packet size: 2 bytes
    0x01,                       // Polling interval: 1ms
};
```

**Important Notes:**
- **Single Interface**: HID consumer control device
- **Single Endpoint**: EP1 IN for sending reports to host
- **Interrupt Transfer**: 1ms polling interval for responsive control
- **Report Size**: 2 bytes (Report ID + control byte)

### String Descriptors (`usb_descriptors.c:99-128`)
- **String 0**: Language descriptor (English US - 0x0409)
- **String 1**: Manufacturer - "Microchip Technology Inc."
- **String 2**: Product - "Volume Control"

## HID Report Descriptor Analysis

### Report Descriptor Structure (`app_device_consumer.c:48-69`)
```c
const struct{uint8_t report[HID_RPT01_SIZE];}hid_rpt01 = {
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
```

### Consumer Control Usages
| Usage ID | Function | Bit Position | HID Usage Name |
|----------|----------|--------------|----------------|
| 0xB5 | Next Track | 0 | Scan Next Track |
| 0xB6 | Previous Track | 1 | Scan Previous Track |
| 0xB7 | Stop | 2 | Stop |
| 0xCD | Play/Pause | 3 | Play/Pause |
| 0xE2 | Mute | 4 | Mute |
| 0xE9 | **Volume Up** | 5 | Volume Increment |
| 0xEA | **Volume Down** | 6 | Volume Decrement |
| N/A | Padding | 7 | Constant bit |

**Note**: Only Volume Up and Volume Down are currently implemented in the firmware.

## HID Report Format

### Report Structure (`app_device_consumer.c:79-97`)
```c
typedef struct PACKED {
    uint8_t reportID;        // Always 0x01 for consumer reports
    union PACKED {
        uint8_t value;       // Raw byte value
        struct PACKED {
            unsigned scanNextTrack  :1;  // Bit 0 - Usage 0xB5
            unsigned scanPrevTrack  :1;  // Bit 1 - Usage 0xB6
            unsigned stop           :1;  // Bit 2 - Usage 0xB7
            unsigned playPause      :1;  // Bit 3 - Usage 0xCD
            unsigned mute           :1;  // Bit 4 - Usage 0xE2
            unsigned volumeUp       :1;  // Bit 5 - Usage 0xE9 (ACTIVE)
            unsigned volumeDown     :1;  // Bit 6 - Usage 0xEA (ACTIVE)
            unsigned                :1;  // Bit 7 - Padding
        } bits;
    } controls;
} CONSUMER_INPUT_REPORT;
```

### Report Examples
- **Volume Up**: `{0x01, 0x20}` - Report ID 1, bit 5 set
- **Volume Down**: `{0x01, 0x40}` - Report ID 1, bit 6 set  
- **No Action**: `{0x01, 0x00}` - Report ID 1, all control bits clear

## Quadrature Encoder Implementation

### Hardware Configuration (`encoder.c:38-46`)
```c
#define ENCODER_A_PORT  PORTBbits.RB3      // Channel A
#define ENCODER_B_PORT  PORTBbits.RB4      // Channel B
#define ENCODER_A_TRIS  TRISBbits.TRISB3
#define ENCODER_B_TRIS  TRISBbits.TRISB4
```

**Pin Assignments:**
- **Channel A**: RB3 (PORTB bit 3)
- **Channel B**: RB4 (PORTB bit 4)
- **Configuration**: Both pins as inputs with weak pull-ups enabled

### Quadrature State Machine (`encoder.c:61-67`)
```c
static const int8_t encoder_table[ENCODER_STATES][ENCODER_STATES] = {
    // Current state:  00  01  10  11
    {  0, -1,  1,  0}, // Previous state: 00
    {  1,  0,  0, -1}, // Previous state: 01  
    { -1,  0,  0,  1}, // Previous state: 10
    {  0,  1, -1,  0}  // Previous state: 11
};
```

**State Encoding:**
- **State**: `(Channel_A << 1) | Channel_B`
- **Values**: 
  - +1 = Clockwise step
  - -1 = Counter-clockwise step
  - 0 = Invalid transition or no movement

### Encoder Processing Algorithm

#### Debouncing (`encoder.c:159-172`)
```c
#define DEBOUNCE_COUNT  2   // Require 2 consistent readings
```
- Filters mechanical contact bounce
- Requires 2 consecutive identical readings before accepting state change

#### Step Accumulation (`encoder.c:127-137`)
```c
// Accumulate steps until threshold reached
if (encoder_count >= 4) {
    direction = ENCODER_CW;
    encoder_count -= 4;  // Consume the count
}
else if (encoder_count <= -4) {
    direction = ENCODER_CCW;  
    encoder_count += 4;  // Consume the count
}
```
- **Threshold**: ±4 steps before reporting direction
- **Prevents**: Jitter and provides hysteresis
- **Overflow Protection**: Limited to ±20 counts

## USB Communication Flow

### Initialization Sequence (`main.c:41-72`)
```c
int main(void) {
    SYSTEM_Initialize(SYSTEM_STATE_USB_START);  // 1. System init
    __delay_ms(500);                           // 2. Stabilization delay
    USBDeviceInit();                           // 3. Initialize USB stack
    USBDeviceAttach();                         // 4. Connect to bus
    
    while(1) {
        SYSTEM_Tasks();                        // 5. System maintenance
        #if defined(USB_POLLING)
        USBDeviceTasks();                      // 6. USB stack (if polling)
        #endif
        APP_ConsumerTasks();                   // 7. Application logic
    }
}
```

### Application Task Flow (`app_device_consumer.c:154-231`)

#### State Checks
1. **Device Configuration**: `USBGetDeviceState() < CONFIGURED_STATE`
2. **Suspend State**: `USBIsDeviceSuspended() == true`
3. **Transmission Status**: `HIDTxHandleBusy(lastConsumerTransmission)`

#### Report Generation Process
```c
void APP_ConsumerTasks(void) {
    // 1. Check USB device state
    if(USBGetDeviceState() < CONFIGURED_STATE) return;
    
    // 2. Update encoder state machine  
    ENCODER_Task();
    
    // 3. Check suspend state
    if(USBIsDeviceSuspended() == true) return;
    
    // 4. Check if endpoint is free
    if(HIDTxHandleBusy(lastConsumerTransmission) == false) {
        
        // 5. Clear report and set Report ID
        memset(&consumerReport, 0, sizeof(consumerReport));
        consumerReport.reportID = 0x01;
        
        // 6. Get encoder direction
        ENCODER_DIRECTION encoder_dir = ENCODER_GetDirection();
        
        // 7. Set appropriate control bit
        if(encoder_dir == ENCODER_CW) {
            consumerReport.controls.bits.volumeUp = 1;
        }
        else if(encoder_dir == ENCODER_CCW) {
            consumerReport.controls.bits.volumeDown = 1;
        }
        
        // 8. Check for changes from previous report
        needToSendNewReportPacket_consumer = false;
        for(i = 0; i < sizeof(consumerReport); i++) {
            if(*((uint8_t*)&oldconsumerReport + i) != 
               *((uint8_t*)&consumerReport + i)) {
                needToSendNewReportPacket_consumer = true;
                break;
            }
        }
        
        // 9. Send report if changed
        if(needToSendNewReportPacket_consumer == true) {
            oldconsumerReport = consumerReport;
            lastConsumerTransmission = HIDTxPacket(HID_EP, 
                (uint8_t*)&consumerReport, sizeof(consumerReport));
        }
    }
}
```

## Configuration Parameters

### USB Configuration (`usb_config.h`)
```c
// Basic USB Settings
#define USB_EP0_BUFF_SIZE           8    // Control endpoint buffer size
#define USB_MAX_NUM_INT             1    // Maximum interfaces
#define USB_MAX_EP_NUMBER           1    // Maximum endpoint number

// USB Operating Mode
#define USB_PING_PONG_MODE USB_PING_PONG__FULL_PING_PONG
#define USB_INTERRUPT                    // Use interrupt mode (not polling)

// Device Identity
#define MY_VID 0x04D8                   // Microchip VID
#define MY_PID 0x0055                   // Product ID

// HID Configuration  
#define HID_INTF_ID             0x00    // Interface ID
#define HID_EP                  1       // HID endpoint number
#define HID_INT_IN_EP_SIZE      2       // HID report size
#define HID_NUM_OF_DSC          1       // Number of HID descriptors
#define HID_RPT01_SIZE          39      // Report descriptor size
```

### System Configuration (`system.c:35-50`)
```c
#pragma config PLLDIV   = 5         // 20MHz crystal / 5 = 4MHz to PLL
#pragma config CPUDIV   = OSC1_PLL2  // CPU clock = PLL output / 2 = 48MHz  
#pragma config USBDIV   = 2         // USB clock = 96MHz PLL / 2 = 48MHz
#pragma config FOSC     = HSPLL_HS  // High Speed PLL, High Speed OSC
#pragma config VREGEN   = ON        // USB Voltage Regulator enabled
#pragma config WDT      = OFF       // Watchdog timer disabled
#pragma config MCLRE    = ON        // MCLR pin enabled
#pragma config PBADEN   = OFF       // PORTB digital I/O
```

## Data Flow Summary
```
Hardware Input → Encoder State Machine → Debouncing → Step Accumulation → 
Direction Detection → HID Report Generation → Change Detection → 
USB Transmission → Host Operating System → Volume Control
```

## Timing Considerations

### USB Timing
- **Polling Interval**: 1ms (endpoint descriptor)
- **Report Transmission**: Only when encoder state changes
- **USB Stack**: Interrupt-driven for optimal performance

### Encoder Timing
- **Debounce Period**: 2 consecutive readings required
- **Step Threshold**: 4 accumulated steps before direction report
- **Sampling Rate**: Called every main loop iteration (~1ms)

## Troubleshooting Guide

### Common Issues

#### Device Not Recognized
- Check VID/PID values in `usb_config.h`
- Verify crystal frequency and PLL configuration
- Ensure USB voltage regulator is enabled (`VREGEN = ON`)

#### Encoder Not Responding  
- Verify pin assignments (RB3/RB4)
- Check pull-up configuration
- Monitor `encoder_count` accumulation in debugger
- Verify mechanical encoder connections

#### Erratic Volume Control
- Increase `DEBOUNCE_COUNT` for noisy encoders
- Check for EMI/noise on encoder lines
- Verify encoder step threshold (currently ±4)

#### USB Communication Issues
- Ensure `USBDeviceTasks()` called regularly if using polling mode
- Check endpoint buffer sizes
- Verify report descriptor matches report structure

### Debug Points
- `encoder_count` in `encoder.c` - Raw step accumulation
- `consumerReport` in `app_device_consumer.c` - Generated reports
- `needToSendNewReportPacket_consumer` - Change detection flag
- USB device state via `USBGetDeviceState()`

## Future Enhancements

### Additional Consumer Controls
The report descriptor already defines but doesn't implement:
- Next/Previous Track (bits 0-1)
- Stop (bit 2)  
- Play/Pause (bit 3)
- Mute (bit 4)

### Encoder Improvements
- Adjustable step threshold via configuration
- Different acceleration curves
- Push-button detection (mute function)

### USB Features
- Multiple report IDs for different control groups
- GET_REPORT/SET_REPORT support for configuration
- Idle rate handling for power management

## References
- **USB HID Usage Tables**: USB.org HID Usage Tables v1.22
- **USB 2.0 Specification**: Chapter 9 - USB Device Framework
- **Microchip USB Framework**: MLA (Microchip Libraries for Applications)
- **PIC18F USB Family**: Datasheet for specific microcontroller model