//
// Smpl_7seg : count 0 ~ 9999 on the four 7-segment LEDs
//             GPC4~7 select digit (1 = on), GPE0~7 drive segments (0 = on)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "Seven_Segment.h"

// display 4-digit value (0~9999) on 7-segment LEDs
void seg_display(int16_t value)
{
	int8_t digit;

	digit = value / 1000;			// thousands digit
	close_seven_segment();
	show_seven_segment(3, digit);
	DrvSYS_Delay(5000);

	value = value - digit * 1000;
	digit = value / 100;			// hundreds digit
	close_seven_segment();
	show_seven_segment(2, digit);
	DrvSYS_Delay(5000);

	value = value - digit * 100;
	digit = value / 10;			// tens digit
	close_seven_segment();
	show_seven_segment(1, digit);
	DrvSYS_Delay(5000);

	value = value - digit * 10;
	digit = value;				// ones digit
	close_seven_segment();
	show_seven_segment(0, digit);
	DrvSYS_Delay(5000);
}

int32_t main (void)
{
	int32_t i = 0;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	while (i < 10000)
	{
		seg_display(i);		// display i on 7-segment LEDs
		DrvSYS_Delay(10000);	// delay for keeping display
		i++;			// increment i
	}
	close_seven_segment();		// turn off display when finished
	while (1);
}
