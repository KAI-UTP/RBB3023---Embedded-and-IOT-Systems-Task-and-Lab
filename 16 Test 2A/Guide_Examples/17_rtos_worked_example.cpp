//
// Example 17 : Worked example 11 (Test 1 paper) rewritten with RTOS threads
//
//   One thread per task, each with its own timing:
//     t_count : count down 77 -> 5 every 500 ms, then 77 again   (task 1)
//     t_leds  : running light LED1 -> LED4, delay = led_delay    (task 2)
//     t_input : reads the joystick every 20 ms                   (tasks 2 + 4)
//               up = +50 ms delay, down = -50 ms, centre = add a box
//     t_bar   : bar on the left grows 1 px every 300 ms          (task 3)
//     main    : redraws the LCD every 50 ms (the only thread using the LCD)
//   Shared numbers are protected by data_mutex; only main draws, so the LCD
//   needs no mutex. Same LCD layout as example 11.
//
//   Joystick down p12, centre p14, up p15   LCD p5 p6 p7 p8 p11   LED1 ~ LED4
//
#include "mbed.h"
#include "lcd_c12832.h"

#define COUNT_START	77
#define COUNT_END	5
#define MAX_BOXES	12

DigitalOut leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                       DigitalOut(LED3), DigitalOut(LED4) };
DigitalIn  joy_up(p15, PullDown), joy_down(p12, PullDown), joy_centre(p14, PullDown);
LCD_C12832 lcd;
Mutex      data_mutex;		// protects the 4 shared values below

int number    = COUNT_START;	// task 1
int led_delay = 300;		// task 2 (ms)
int bar_h     = 0;		// task 3
int boxes     = 0;		// task 4

Thread t_count(osPriorityNormal, 1024);
Thread t_leds(osPriorityNormal, 1024);
Thread t_input(osPriorityAboveNormal, 1024);	// inputs react first
Thread t_bar(osPriorityNormal, 1024);

void count_task()
{
	while (true) {
		ThisThread::sleep_for(500ms);
		data_mutex.lock();
		number = (number > COUNT_END) ? number - 1 : COUNT_START;
		data_mutex.unlock();
	}
}

void leds_task()
{
	int pos = 0;

	while (true) {
		for (int i = 0; i < 4; i++)
			leds[i] = (i == pos);
		pos = (pos + 1) % 4;

		data_mutex.lock();
		int d = led_delay;		// copy, then unlock before sleeping
		data_mutex.unlock();
		ThisThread::sleep_for(std::chrono::milliseconds(d));
	}
}

void input_task()
{
	bool last_up = false, last_down = false, last_centre = false;

	while (true) {
		bool up = joy_up, down = joy_down, centre = joy_centre;

		data_mutex.lock();
		if (up && !last_up && led_delay < 1000)  led_delay += 50;
		if (down && !last_down && led_delay > 50) led_delay -= 50;
		if (centre && !last_centre)
			boxes = (boxes < MAX_BOXES) ? boxes + 1 : 0;
		data_mutex.unlock();

		last_up = up; last_down = down; last_centre = centre;
		ThisThread::sleep_for(20ms);
	}
}

void bar_task()
{
	while (true) {
		ThisThread::sleep_for(300ms);
		data_mutex.lock();
		bar_h = (bar_h < 32) ? bar_h + 1 : 0;
		data_mutex.unlock();
	}
}

int main()
{
	char text[24];

	lcd.init();
	t_count.start(count_task);
	t_leds.start(leds_task);
	t_input.start(input_task);
	t_bar.start(bar_task);

	while (true) {
		data_mutex.lock();		// take a copy of everything at once
		int n = number, d = led_delay, h = bar_h, b = boxes;
		data_mutex.unlock();

		lcd.clear();
		lcd.box(0, 0, 10, 32);				// task 3 bar
		lcd.fill_box(1, 32 - h, 8, h);
		for (int i = 0; i < b; i++)			// task 4 boxes
			lcd.box(14 + (i % 6) * 10, 4 + (i / 6) * 12, 7, 7);
		sprintf(text, "%2d", n);			// task 1 number
		lcd.text(88, 0, text, 3);
		sprintf(text, "%4dms", d);			// task 2 delay
		lcd.text(14, 25, text);
		lcd.update();
		ThisThread::sleep_for(50ms);
	}
}
