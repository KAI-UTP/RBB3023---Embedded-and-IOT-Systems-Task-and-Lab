//
// Smpl_7seg_Keypad_Shift : press 3x3 keypad, digits shift in from the right
//                          e.g. press 1 -> "   1", press 2 -> "  12",
//                               press 3 -> " 123", press 4 -> "1234",
//                               press 5 -> "2345" (oldest digit drops off)
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "Seven_Segment.h"
#include "Scankey.h"

uint8_t digit[4];	// digit[0] = rightmost 7-segment, digit[3] = leftmost
uint8_t count = 0;	// number of digits entered so far (0~4)

// shift all digits one place to the left and put new key at the right
void shift_in(uint8_t key)
{
	digit[3] = digit[2];
	digit[2] = digit[1];
	digit[1] = digit[0];
	digit[0] = key;
	if (count < 4) count++;
}

// refresh the four 7-segment LEDs once (only the digits entered so far)
void seg_display(void)
{
	int8_t i;

	for (i = 3; i >= 0; i--)
	{
		close_seven_segment();
		if (i < count)			// blank digits not entered yet
			show_seven_segment(i, digit[i]);
		DrvSYS_Delay(5000);
	}
}

int32_t main (void)
{
	uint8_t key;
	uint8_t last_key = 0;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	OpenKeyPad();			// initialize 3x3 keypad pins

	while (1)
	{
		key = ScanKey();	// 1~9 when pressed, 0 when released

		// accept key only on a new press, so holding it adds one digit
		if (key != 0 && last_key == 0)
			shift_in(key);
		last_key = key;

		seg_display();		// keep refreshing the display
	}
}
