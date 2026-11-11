//
// Task 1A : read the 3x3 keypad and show the key number in BINARY
//           on the four LEDs, arranged left to right as LED5 LED6 LED7 LED8
//
//   Keypad : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LEDs   : GPC12 = LED5 (bit 3, MSB)  <- leftmost
//            GPC13 = LED6 (bit 2)
//            GPC14 = LED7 (bit 1)
//            GPC15 = LED8 (bit 0, LSB)  <- rightmost
//            output low to turn LED on
//
//   e.g. key 5 = 0101 -> LED5 off, LED6 on,  LED7 off, LED8 on
//        key 8 = 1000 -> LED5 on,  LED6 off, LED7 off, LED8 off
//   LCD    : line 0 title, line 1 key, line 2 "LED : 5 6 7 8",
//   the last key pressed stays on the LEDs until a new key is pressed
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Scankey.h"

// Initial GPIO pins (GPC 12,13,14,15) to Output mode
void Init_LED(void)
{
	int32_t i;

	for (i = 12; i <= 15; i++)
	{
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);	// GPC12~15 set to output mode
		DrvGPIO_SetBit(E_GPC, i);		// output Hi to turn off LED
	}
}

// show value (0~15) in binary on LED5~LED8 (LED5 = MSB, LED8 = LSB)
void LED_binary(uint8_t value)
{
	int32_t bit;

	for (bit = 0; bit < 4; bit++)
	{
		if (value & (1 << bit))
			DrvGPIO_ClrBit(E_GPC, 15 - bit);	// bit = 1 -> LED on
		else
			DrvGPIO_SetBit(E_GPC, 15 - bit);	// bit = 0 -> LED off
	}
}


void lcd_display(uint8_t key)
{
	char TEXT[17];

	sprintf(TEXT, "Key : %d         ", key);
	print_Line(1, TEXT);
	print_Line(2, "LED : 5 6 7 8   ");
	sprintf(TEXT, "NOW : %d %d %d %d   ",
		(key >> 3) & 1, (key >> 2) & 1, (key >> 1) & 1, key & 1);
	print_Line(3, TEXT);
}

int32_t main (void)
{
	uint8_t key;
	uint8_t last_key = 0;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);	
	LOCKREG();			// 

	Init_LED();	
	init_LCD();
	clear_LCD();
	print_Line(0, "Task 1A : Binary");
	print_Line(1, "Press a key...  ");
	OpenKeyPad();	

	while (1)
	{
		key = ScanKey();	// 1~9 when pressed, 0 = no key
		if (key != 0 && key != last_key)
		{
			LED_binary(key);
			lcd_display(key);
			last_key = key;
		}
	}
}
