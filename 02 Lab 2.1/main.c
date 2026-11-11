//
// Lab 2.1 : press 3x3 keypad, show the number on 7-segment and LCD
//           digits are entered from LEFT to RIGHT
//           e.g. press 1 -> "1   ", press 2 -> "12  ",
//                press 3 -> "123 ", press 4 -> "1234",
//                press 5 -> "5   " (display full, start again from left)
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//               show_seven_segment(3,..) = leftmost, (0,..) = rightmost
//   LCD       : 128x64 via SPI3, print_Line(0~3, text) 16 chars per line
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

uint8_t digit[4];	// digit[0] = leftmost 7-segment, digit[3] = rightmost
uint8_t count = 0;	// number of digits entered so far (0~4)
uint32_t presses = 0;	// total number of key presses


void enter_digit(uint8_t key)
{
	if (count == 4) count = 0;	
	digit[count] = key;
	count++;
}


void seg_display(void)
{
	int8_t i;

	for (i = 0; i < 4; i++)
	{
		close_seven_segment();
		if (i < count)			// blank digits not entered yet
			show_seven_segment(3 - i, digit[i]);	// i = 0 -> leftmost
		DrvSYS_Delay(5000);
	}
}

// show last key, the 4-digit number and press count on the LCD
void lcd_display(uint8_t key)
{
	char TEXT[17];
	char num[5];
	int8_t i;

	// build the number string, blank for digits not entered yet
	for (i = 0; i < 4; i++)
		num[i] = (i < count) ? ('0' + digit[i]) : ' ';
	num[4] = '\0';

	sprintf(TEXT, "Key   : %d       ", key);
	print_Line(1, TEXT);
	sprintf(TEXT, "Number: %s    ", num);
	print_Line(2, TEXT);
	sprintf(TEXT, "Press : %-8d", presses);
	print_Line(3, TEXT);
}

int32_t main (void)
{
	uint8_t key;
	uint8_t last_key = 0;

	UNLOCKREG();		
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();	
	clear_LCD();
	print_Line(0, "Lab 2.1 L -> R  ");
	print_Line(1, "Press a key...  ");

	OpenKeyPad();	

	while (1)
	{
		key = ScanKey();

		// accept key only on a new press, so holding it adds one digit
		if (key != 0 && last_key == 0)
		{
			enter_digit(key);
			presses++;
			close_seven_segment();	// blank 7-seg while LCD is updated
			lcd_display(key);	// update LCD only when a key is pressed
		}
		last_key = key;

		seg_display();		// keep refreshing the 7-segment display
	}
}
