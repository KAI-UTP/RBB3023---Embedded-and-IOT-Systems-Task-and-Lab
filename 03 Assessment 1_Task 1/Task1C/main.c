//
// Task 1C : read 4 keypad entries and show one entry on each
//           of the four 7-segment displays, entered from the RIGHT and
//           shifted LEFT (like a calculator)
//           e.g. press 7 -> "   7", press 5 -> "  75",
//                press 3 -> " 753", press 1 -> "7531",
//                press 8 -> "5318" (display full, oldest digit drops off)
//           the binary of the last key is also shown on LED5~LED8 and LCD
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//               show_seven_segment(3,..) = leftmost, (0,..) = rightmost
//   LEDs      : GPC12~15 = LED5~LED8 (LED5 = MSB, LED8 = LSB), output low = on
//   LCD       : line 1 key, line 2 binary "NOW : 0 1 0 1",
//               line 3 the 4 entries "7SEG: _ 7 5 3"
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

uint8_t digit[4];	// digit[0] = leftmost 7-segment, digit[3] = rightmost
uint8_t count = 0;	// number of entries so far (0~4)

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

// shift all digits one place left and put the new entry on the right
void enter_digit(uint8_t key)
{
	int8_t i;

	for (i = 0; i < 3; i++)
		digit[i] = digit[i + 1];
	digit[3] = key;
	if (count < 4) count++;
}

// scan the four 7-segment displays one at a time (call continuously)
void seg_display(void)
{
	int8_t i;

	for (i = 0; i < 4; i++)
	{
		close_seven_segment();
		if (i >= 4 - count)		// blank digits not entered yet
			show_seven_segment(3 - i, digit[i]);	// i = 0 -> leftmost
		DrvSYS_Delay(5000);
	}
	close_seven_segment();
}

// show key, its binary and the 4 entries on the LCD, e.g.
//   Key : 3
//   NOW : 0 0 1 1
//   7SEG: _ 7 5 3
void lcd_display(uint8_t key)
{
	char TEXT[17];
	char d[4];
	int8_t i;

	for (i = 0; i < 4; i++)		// '_' for digits not entered yet
		d[i] = (i >= 4 - count) ? ('0' + digit[i]) : '_';

	sprintf(TEXT, "Key : %d         ", key);
	print_Line(1, TEXT);
	sprintf(TEXT, "NOW : %d %d %d %d   ",
		(key >> 3) & 1, (key >> 2) & 1, (key >> 1) & 1, key & 1);
	print_Line(2, TEXT);
	sprintf(TEXT, "7SEG: %c %c %c %c   ", d[0], d[1], d[2], d[3]);
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
	print_Line(0, "Task 1C : 4 Keys");
	print_Line(1, "Press a key...  ");
	close_seven_segment();
	OpenKeyPad();			// initialize 3x3 keypad

	while (1)
	{
		key = ScanKey();	// 1~9 when pressed, 0 = no key

		if (key != 0 && last_key == 0)
		{
			enter_digit(key);
			LED_binary(key);
			lcd_display(key);	// update LCD only when a key is pressed
		}
		last_key = key;

		seg_display();		// keep refreshing the 7-segment displays
	}
}
