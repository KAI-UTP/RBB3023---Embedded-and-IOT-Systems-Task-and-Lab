//
// Example 11 : Worked example - the Test 1 paper redone for the LPC1768
//
//   Task 1 : Ticker t_count (0.5 s) counts DOWN from 77 to 5, then starts at 77
//            again; shown big on the right of the LCD
//   Task 2 : running light on LED1 -> LED4 (polling with a Timer in the main loop)
//            joystick up   = increase delay (+50 ms, slower)
//            joystick down = decrease delay (-50 ms, faster), 50 ~ 1000 ms
//   Task 3 : Ticker t_bar (0.3 s) makes the bar on the LEFT of the LCD 1 px
//            higher each time; starts again from 0 when it is full (32 px)
//   Task 4 : joystick centre adds a box beside the bar (starts with 0 boxes,
//            max 12, then back to 0)
//   All tasks run at the same time: Tickers only change variables,
//   the main loop reads the joystick, moves the LEDs and redraws the LCD.
//
//   Joystick : down p12, centre p14, up p15 (1 = pressed)
//   LCD      : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11 (lcd_c12832.h)
//   LED1 ~ LED4 on the mbed module (1 = on)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define COUNT_START	77
#define COUNT_END	5
#define MAX_BOXES	12

DigitalOut leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                       DigitalOut(LED3), DigitalOut(LED4) };
DigitalIn  joy_up(p15, PullDown);
DigitalIn  joy_down(p12, PullDown);
DigitalIn  joy_centre(p14, PullDown);
LCD_C12832 lcd;
Ticker     t_count, t_bar;
Timer      t_led;

volatile int number = COUNT_START;	// task 1, changed by t_count
volatile int bar_h  = 0;		// task 3, changed by t_bar

// task 1 : Ticker callback, interrupt context -> variables only
void count_down()
{
	if (number > COUNT_END)
		number--;
	else
		number = COUNT_START;
}

// task 3 : Ticker callback
void grow_bar()
{
	bar_h = (bar_h < 32) ? bar_h + 1 : 0;
}

int main()
{
	int  delay_ms = 300;		// task 2 LED step
	int  led_pos = 0;
	int  boxes = 0;			// task 4
	bool last_up = false, last_down = false, last_centre = false;
	char text[24];

	lcd.init();
	t_count.attach(&count_down, 500ms);
	t_bar.attach(&grow_bar, 300ms);
	t_led.start();

	while (true)
	{
		// ---- inputs (edge detection = one action per press) ----
		bool up = joy_up, down = joy_down, centre = joy_centre;

		if (up && !last_up && delay_ms < 1000)	delay_ms += 50;	// increase delay
		if (down && !last_down && delay_ms > 50) delay_ms -= 50;	// decrease delay
		if (centre && !last_centre)
			boxes = (boxes < MAX_BOXES) ? boxes + 1 : 0;
		last_up = up;
		last_down = down;
		last_centre = centre;

		// ---- task 2 : running LEDs, polling a Timer ----
		if (t_led.elapsed_time() >= std::chrono::milliseconds(delay_ms))
		{
			t_led.reset();
			led_pos = (led_pos + 1) % 4;
			for (int i = 0; i < 4; i++)
				leds[i] = (i == led_pos);
		}

		// ---- LCD : draw everything, then one update ----
		lcd.clear();

		// task 3 : bar on the left, grows upward
		lcd.box(0, 0, 10, 32);
		lcd.fill_box(1, 32 - bar_h, 8, bar_h);

		// task 4 : boxes beside the bar, 2 rows of 6 (7 x 7 px each)
		for (int i = 0; i < boxes; i++)
			lcd.box(14 + (i % 6) * 10, 4 + (i / 6) * 12, 7, 7);

		// task 1 : count-down number, scale 3 = 18 x 24 px per digit
		sprintf(text, "%2d", (int)number);
		lcd.text(88, 0, text, 3);

		// task 2 : the current LED delay
		sprintf(text, "%4dms", delay_ms);
		lcd.text(14, 25, text);

		lcd.update();
		ThisThread::sleep_for(10ms);
	}
}
