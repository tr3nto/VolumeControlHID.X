/********************************************************************
 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the "Company") for its PIC(R) Microcontroller is intended and
 supplied to you, the Company's customer, for use solely and
 exclusively on Microchip PIC Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *******************************************************************/

#include <stdbool.h>
#include <stdint.h>

#ifndef ENCODER_H
#define ENCODER_H

/*** Encoder Definitions *********************************************/
typedef enum
{
    ENCODER_NONE,
    ENCODER_CW,     // Clockwise (volume up)
    ENCODER_CCW     // Counter-clockwise (volume down)
} ENCODER_DIRECTION;

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
void ENCODER_Initialize(void);

/*********************************************************************
* Function: ENCODER_DIRECTION ENCODER_GetDirection(void);
*
* Overview: Reads the encoder and returns the direction of rotation
*           This function should be called regularly to detect changes
*
* PreCondition: ENCODER_Initialize() must be called first
*
* Input: None
*
* Output: ENCODER_DIRECTION - Direction of encoder rotation
*         ENCODER_NONE: No movement detected
*         ENCODER_CW: Clockwise rotation (volume up)
*         ENCODER_CCW: Counter-clockwise rotation (volume down)
*
********************************************************************/
ENCODER_DIRECTION ENCODER_GetDirection(void);

/*********************************************************************
* Function: void ENCODER_Task(void);
*
* Overview: Encoder processing task that should be called regularly
*           from the main loop to update encoder state
*
* PreCondition: ENCODER_Initialize() must be called first
*
* Input: None
*
* Output: None
*
********************************************************************/
void ENCODER_Task(void);

#endif //ENCODER_H