# USB HID Consumer Device Implementation Guide

## Current Implementation Analysis

Your HID report descriptor already includes **both** keyboard and consumer device functionality:
- **Keyboard collection**: No Report ID (default ID 0)
- **Consumer device collection**: Uses Report ID 2 (`0x85, 0x02` at line 84 in app_device_keyboard.c)

Current memory layout (fixed_address_memory.h):
- `0x500`: Keyboard input report data (8 bytes)
- `0x508`: Keyboard output report data (1 byte)

## Key Findings

1. **Report ID Structure**: Your descriptor correctly uses Report ID 2 for consumer controls, meaning the consumer report format is:
   - Byte 0: Report ID (0x02)
   - Byte 1: Consumer control data (7 bits for volume/media controls + 1 padding bit)

2. **Missing Consumer Data Structure**: You have the descriptor but no data structure or memory allocation for consumer device reports.

3. **Endpoint Size**: Your `HID_INT_IN_EP_SIZE` is already set to 9 bytes (was 8), suggesting preparation for larger reports.

## Required Changes

**You do NOT need to move the output data down.** Instead, you need to:

1. **Add consumer device data structure** at a new memory location (e.g., `0x509`)
2. **Create consumer report typedef** similar to `KEYBOARD_INPUT_REPORT`
3. **Modify transmission logic** to handle multiple report IDs
4. **Update the application logic** to send the appropriate report based on input type

## Memory Layout Recommendation

```
0x500: Keyboard input report (8 bytes) - Report ID 0 (implicit)
0x508: Keyboard output report (1 byte) 
0x509: Consumer input report (2 bytes) - Report ID 2 (first byte) + data (second byte)
```

## Implementation Strategy

The consumer report should be sent as a separate HID report with Report ID 2, while keyboard reports continue using the default Report ID 0. The USB stack will handle routing these to the correct device interface based on the Report ID.

This approach maintains backward compatibility while adding volume control functionality without disrupting your existing keyboard implementation.

## Implementation Steps

### Step 1: Add Consumer Report Data Structure

Add to `fixed_address_memory.h`:
```c
#define CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG     __at(0x509)
```

### Step 2: Create Consumer Report Typedef

Add to `app_device_keyboard.c`:
```c
typedef struct PACKED
{
    uint8_t reportID;        // Always 0x02 for consumer reports
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
```

### Step 3: Add Consumer Report Variable

Add to the global variables section in `app_device_keyboard.c`:
```c
static CONSUMER_INPUT_REPORT consumerReport CONSUMER_INPUT_REPORT_DATA_BUFFER_ADDRESS_TAG;
```

### Step 4: Modify Application Logic

Add consumer report handling to `APP_KeyboardTasks()`:
```c
// Add consumer device handling logic
// Check for volume/media button presses
// Set consumerReport.reportID = 0x02
// Set appropriate bits in consumerReport.controls
// Send via HIDTxPacket() when consumer buttons are pressed
```

### Step 5: Update Transmission Logic

Modify the transmission section to handle both report types:
- Keyboard reports: Send as before (8 bytes, no report ID prefix)
- Consumer reports: Send with Report ID 2 (2 bytes total)

## Notes

- Consumer reports are typically sent only when media keys are pressed/released
- Keyboard reports continue to work as before
- The USB host will automatically handle the different report IDs
- Total memory usage increases by only 2 bytes for consumer functionality