//
// Example 03 : Ticker - interrupt-driven timing (lecture 08a, Exercise 3)
//
//   flipper1 toggles LED1 every 200 ms, flipper2 toggles LED2 every 1 s
//   flipper3 runs a light along LED3 -> LED4, its speed is changed with
//   the joystick: up = slower (+50 ms), down = faster (-50 ms), 50 ~ 1000 ms
//   Serial (115200) prints the new speed after each change.
//
//   RULE: a Ticker callback runs in interrupt context.
//         Only change DigitalOut / volatile variables there.
//         printf, LCD update, BusOut, mutexes -> do them in main().
//
//   Joystick : up p15, down p12 (1 = pressed)
//
#include "mbed.h"

Ticker flipper1, flipper2, flipper3;
DigitalOut led1(LED1), led2(LED2), led3(LED3), led4(LED4);
DigitalIn  joy_up(p15, PullDown);
DigitalIn  joy_down(p12, PullDown);

volatile int step_ms = 300;		// flipper3 period, changed by the joystick

void flip1() { led1 = !led1; }
void flip2() { led2 = !led2; }
void flip3()				// LED3 and LED4 take turns
{
	led3 = !led3;
	led4 = !led3;
}

// detach + attach is how the period of a Ticker is changed
void set_speed(int ms)
{
	flipper3.detach();
	flipper3.attach(&flip3, std::chrono::milliseconds(ms));
}

int main()
{
	bool last_up = false, last_down = false;

	led1 = 0;
	led2 = 0;
	flipper1.attach(&flip1, 200ms);		// lecture: attach(&flip1, 0.2)
	flipper2.attach(&flip2, 1s);		// lecture: attach(&flip2, 1.0)
	set_speed(step_ms);
	printf("\r\nTicker demo, LED3/LED4 step = %d ms\r\n", step_ms);

	while (true)				// main is free: the Tickers interrupt it
	{
		bool up = joy_up, down = joy_down;

		if (up && !last_up && step_ms < 1000)		// edge = once per press
		{
			step_ms += 50;
			set_speed(step_ms);
			printf("slower: %d ms\r\n", step_ms);
		}
		if (down && !last_down && step_ms > 50)
		{
			step_ms -= 50;
			set_speed(step_ms);
			printf("faster: %d ms\r\n", step_ms);
		}
		last_up = up;
		last_down = down;
		ThisThread::sleep_for(20ms);		// also gives ~20 ms debounce
	}
}
