//
// Example 04 : Joystick - DigitalIn polling, InterruptIn and Timeout
//
//   Polling (DigitalIn) : left / right move a lit LED along LED1 ~ LED4
//   Interrupt (InterruptIn) : centre press counts presses (debounced 200 ms)
//   Timeout : after a centre press the green RGB LED stays on for 1 s
//   Serial (115200) prints every press
//
//   Joystick : down p12, left p13, centre p14, up p15, right p16 (1 = pressed)
//   RGB LED  : green p24 (0 = on)
//
#include "mbed.h"

DigitalOut  leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                        DigitalOut(LED3), DigitalOut(LED4) };
DigitalOut  green(p24, 1);		// start off (active low)
DigitalIn   joy_left(p13, PullDown);
DigitalIn   joy_right(p16, PullDown);
InterruptIn joy_centre(p14);
Timeout     green_off;
Timer       debounce;

volatile int  presses = 0;
volatile bool pressed_flag = false;

void led_off()				// Timeout callback (interrupt context)
{
	green = 1;
}

void on_centre()			// InterruptIn callback (interrupt context)
{
	if (debounce.elapsed_time() < 200ms)	// ignore switch bounce
		return;
	debounce.reset();
	presses++;
	pressed_flag = true;		// main() does the printf
	green = 0;			// green on ...
	green_off.attach(&led_off, 1s);	// ... and off again in 1 s
}

int main()
{
	int pos = 0;
	bool last_left = false, last_right = false;

	debounce.start();
	joy_centre.mode(PullDown);
	joy_centre.rise(&on_centre);	// rise = pressed, fall = released

	while (true)
	{
		bool l = joy_left, r = joy_right;

		if (l && !last_left && pos > 0)  pos--;	// edge detection
		if (r && !last_right && pos < 3) pos++;
		last_left = l;
		last_right = r;

		for (int i = 0; i < 4; i++)
			leds[i] = (i == pos);

		if (pressed_flag)
		{
			pressed_flag = false;
			printf("centre pressed %d times\r\n", presses);
		}
		ThisThread::sleep_for(20ms);
	}
}
