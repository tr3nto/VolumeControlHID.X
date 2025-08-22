/*******************************************************************************
  Quadrature encoder driver for volume control

  Company:
    Microchip Technology Inc.

  File Name:
    encoder.c

  Summary:
    Provides quadrature encoder interface for volume up/down control

  Description:
    This module implements a quadrature encoder state machine to detect
    clockwise and counter-clockwise rotation for volume control.
    
    Hardware connections:
    - Channel A: RB4 (previously BUTTON_S2)
    - Channel B: RB5 (previously BUTTON_S3)
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>
#include "encoder.h"

// *****************************************************************************
// *****************************************************************************
// Section: File Scope or Global Constants
// *****************************************************************************
// *****************************************************************************

// Hardware pin definitions
#define ENCODER_A_PORT  PORTBbits.RB4      // Channel A
#define ENCODER_B_PORT  PORTBbits.RB5      // Channel B

#define ENCODER_A_TRIS  TRISBbits.TRISB4
#define ENCODER_B_TRIS  TRISBbits.TRISB5

#define PIN_INPUT       1
#define PIN_OUTPUT      0

// Encoder state machine constants
#define ENCODER_STATES  4
#define DEBOUNCE_COUNT  2   // Number of consistent readings required

// *****************************************************************************
// *****************************************************************************
// Section: File Scope Variables
// *****************************************************************************
// *****************************************************************************

// Quadrature encoder state machine lookup table
// This table maps state transitions to direction
// Format: [previous_state][current_state] = direction
static const int8_t encoder_table[ENCODER_STATES][ENCODER_STATES] = {
    // Current state:  00  01  10  11
    {  0, -1,  1,  0}, // Previous state: 00
    {  1,  0,  0, -1}, // Previous state: 01  
    { -1,  0,  0,  1}, // Previous state: 10
    {  0,  1, -1,  0}  // Previous state: 11
};

static uint8_t encoder_prev_state = 0;
static int8_t encoder_count = 0;
static uint8_t debounce_count = 0;
static uint8_t last_stable_state = 0;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/*********************************************************************
* Function: void ENCODER_Initialize(void);
*
* Overview: Initializes the encoder pins and state machine
*
* PreCondition: None
*
* Input: None
*
* Output: None
*
********************************************************************/
void ENCODER_Initialize(void)
{
    // Configure pins as inputs
    ENCODER_A_TRIS = PIN_INPUT;
    ENCODER_B_TRIS = PIN_INPUT;
    
    // Enable weak pull-ups for encoder pins
    // Note: RB4 and RB5 have weak pull-ups enabled by default on most PIC18s
    // If your specific chip requires explicit pull-up configuration, add it here
    INTCON2bits.RBPU = 0;  // Enable PORTB weak pull-ups
    
    // Initialize state machine
    encoder_prev_state = (ENCODER_A_PORT << 1) | ENCODER_B_PORT;
    last_stable_state = encoder_prev_state;
    encoder_count = 0;
    debounce_count = 0;
}

/*********************************************************************
* Function: ENCODER_DIRECTION ENCODER_GetDirection(void);
*
* Overview: Reads the encoder and returns the direction of rotation
*
* PreCondition: ENCODER_Initialize() must be called first
*
* Input: None
*
* Output: ENCODER_DIRECTION - Direction of encoder rotation
*
********************************************************************/
ENCODER_DIRECTION ENCODER_GetDirection(void)
{
    ENCODER_DIRECTION direction = ENCODER_NONE;
    
    // Check if we have accumulated enough movement
    if (encoder_count >= 4) {
        direction = ENCODER_CW;
        encoder_count -= 4;  // Consume the count
    }
    else if (encoder_count <= -4) {
        direction = ENCODER_CCW;
        encoder_count += 4;  // Consume the count
    }
    
    return direction;
}

/*********************************************************************
* Function: void ENCODER_Task(void);
*
* Overview: Encoder processing task that should be called regularly
*
* PreCondition: ENCODER_Initialize() must be called first
*
* Input: None
*
* Output: None
*
********************************************************************/
void ENCODER_Task(void)
{
    uint8_t current_state;
    int8_t direction;
    
    // Read current encoder state (A is MSB, B is LSB)
    current_state = (ENCODER_A_PORT << 1) | ENCODER_B_PORT;
    
    // Simple debouncing - require consistent readings
    if (current_state == last_stable_state) {
        debounce_count = 0;
        return;  // No change, nothing to do
    }
    
    debounce_count++;
    if (debounce_count < DEBOUNCE_COUNT) {
        return;  // Not stable yet
    }
    
    // State has been stable for required count, process it
    debounce_count = 0;
    
    // Look up the direction from the state transition table
    direction = encoder_table[encoder_prev_state][current_state];
    
    // Accumulate the direction changes
    encoder_count += direction;
    
    // Prevent overflow/underflow
    if (encoder_count > 20) {
        encoder_count = 20;
    }
    else if (encoder_count < -20) {
        encoder_count = -20;
    }
    
    // Update state for next iteration
    encoder_prev_state = current_state;
    last_stable_state = current_state;
}

/*******************************************************************************
 End of File
*/