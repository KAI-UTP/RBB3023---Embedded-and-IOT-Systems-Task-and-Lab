//
// Smpl_GPIO_RGBLED : GPA12,13,14 output control RGB LED
//                    output low to enable LEDs
//                    run forward (12->14) then backward (14->12)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"

// Initial GPIO pins (GPA 12,13,14) to Output mode
void Init_LED()
{
	// initialize GPIO pins
	DrvGPIO_Open(E_GPA, 12, E_IO_OUTPUT);	// GPA12 pin set to output mode
	DrvGPIO_Open(E_GPA, 13, E_IO_OUTPUT);	// GPA13 pin set to output mode
	DrvGPIO_Open(E_GPA, 14, E_IO_OUTPUT);	// GPA14 pin set to output mode
	// set GPIO pins output Hi to disable LEDs
	DrvGPIO_SetBit(E_GPA, 12);	// GPA12 pin output Hi to turn off Blue  LED
	DrvGPIO_SetBit(E_GPA, 13);	// GPA13 pin output Hi to turn off Green LED
	DrvGPIO_SetBit(E_GPA, 14);	// GPA14 pin output Hi to turn off Red   LED
}

int main (void)
{
	int32_t i;		// LED pin number

	UNLOCKREG();		// unlock register for programming
	DrvSYS_Open(50000000);	// set System Clock to run at 50MHz
	LOCKREG();		// lock register from programming

	Init_LED();		// Initialize RGB LED

	while (1)		// forever loop
	{
		// GPA12 = Blue, GPA13 = Green, GPA14 = Red,  0 : on, 1 : off

		// forward : Blue -> Green -> Red
		for (i = 12; i <= 14; i++)
		{
			DrvGPIO_ClrBit(E_GPA, i);	// output Low to turn on LED
			DrvSYS_Delay(300000);		// delay
			DrvGPIO_SetBit(E_GPA, i);	// output Hi to turn off LED
		}

		// backward : Red -> Green -> Blue
		for (i = 14; i >= 12; i--)
		{
			DrvGPIO_ClrBit(E_GPA, i);	// output Low to turn on LED
			DrvSYS_Delay(300000);		// delay
			DrvGPIO_SetBit(E_GPA, i);	// output Hi to turn off LED
		}
	}
}
