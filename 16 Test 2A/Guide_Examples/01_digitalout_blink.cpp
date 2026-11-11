//
// Example 01 : DigitalOut - blink and running light on LED1 ~ LED4
//
//   LED1 ~ LED4 are the 4 blue LEDs on the mbed module (1 = on)
//   One LED at a time moves LED1 -> LED4 and back, 200 ms per step
//
#include "mbed.h"

DigitalOut leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                       DigitalOut(LED3), DigitalOut(LED4) };

// turn on only LED number n (0 ~ 3)
void show_one(int n)
{
	for (int i = 0; i < 4; i++)
		leds[i] = (i == n);
}

int main()
{
	int pos = 0, dir = 1;

	while (true)
	{
		show_one(pos);
		ThisThread::sleep_for(200ms);	// Mbed OS 6: no wait(0.2)

		pos += dir;
		if (pos == 3 || pos == 0)	// bounce at both ends
			dir = -dir;
	}
}
