//
// Task 1B : read the 3x3 keypad and display the key number
//           on ONE 7-segment display (the rightmost digit)
//           the binary value is also kept on LED5~LED8 (from Task 1A)
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//               show_seven_segment(0,..) = rightmost digit
//   LEDs      : GPC12~15 = LED5~LED8 (LED5 = MSB, LED8 = LSB), output low = on
//   LCD       : line 1 key, line 2 "LED : 5 6 7 8", line 3 "NOW : 0 1 0 1"
//
//   the last key pressed stays on the display until a new key is pressed
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define SEG_DIGIT	0	// 7-segment used : 0 = rightmost, 3 = leftmost

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
	LOCKREG();			// lock register from programming

	Init_LED();			// all four LEDs off
	init_LCD();
	clear_LCD();
	print_Line(0, "Task 1B : 7-Seg ");
	print_Line(1, "Press a key...  ");
	close_seven_segment();		// 7-segment blank until a key is pressed
	OpenKeyPad();	

	while (1)
	{
		key = ScanKey();	// 1~9 when pressed, 0 = no key

		if (key != 0 && key != last_key)
		{
			LED_binary(key);
			lcd_display(key);
			close_seven_segment();
			show_seven_segment(SEG_DIGIT, key);
			last_key = key;
		}
	}
}
