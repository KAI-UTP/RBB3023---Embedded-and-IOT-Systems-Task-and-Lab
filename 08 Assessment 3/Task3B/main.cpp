//
// Task 3B : 3 Tickers on the mbed NXP LPC1768 control the blink rate of LED1~LED3
//
//   Ticker 1 : every 200 ms  toggle LED1  (2.5 blinks / s)
//   Ticker 2 : every 500 ms  toggle LED2  (1 blink / s)
//   Ticker 3 : every 1000 ms toggle LED3  (0.5 blink / s)
//             each Ticker interrupt only toggles its LED and adds 1 to its count
//
//   Serial monitor (USB, 115200 baud, see mbed_app.json) :
//     every change   "  1200 ms | Ticker 2 | LED2 ON  | count   3 | bar   6/118"
//     every second   count rate of each Ticker (changes in the last 1 s)
//                    and the total counts
//   LCD (application board 128x32) : for each Ticker its period, count and
//                    a box copying the LED, and below it a bar that grows
//                    2 pixels per tick (full at 118 px, then back to 0)
//                    -> the 3 bars grow at 3 different speeds
//
//   Mbed OS 6 : no printf inside the Ticker interrupt, the main loop prints
//
//   mbed Application Board pins
//     LED1~LED3 : on the mbed module (1 = on)
//     LCD       : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11 (lcd_c12832.h)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define TICKERS		3
#define LOOP_TIME	10ms		// main loop checks the counts
#define RATE_TIME	1000ms		// count rate printed every 1 s
#define LCD_TIME	100ms		// LCD redraw period

#define ROW_H		11		// LCD row for one Ticker : 7 text + 1 gap + 2 bar + 1 gap
#define BAR_H		2		// bar height in pixels
#define BAR_STEP	2		// pixels added on each tick
#define BAR_MAX		118		// full bar (59 ticks), then back to 0

DigitalOut leds[TICKERS] = {DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3)};

LCD_C12832 lcd;			// 128x32 LCD on the application board

Ticker ticker1, ticker2, ticker3;
Timer  run_timer;		// time since start, for the serial monitor
Timer  rate_timer;		// 1 s window for the count rate
Timer  lcd_timer;		// LCD redraw

const std::chrono::milliseconds PERIOD[TICKERS] = {200ms, 500ms, 1000ms};

volatile uint32_t ticks[TICKERS] = {0, 0, 0};	// times each Ticker has fired

// Ticker interrupts : toggle the LED, count
void tick1()
{
	leds[0] = !leds[0];
	ticks[0]++;
}

void tick2()
{
	leds[1] = !leds[1];
	ticks[1]++;
}

void tick3()
{
	leds[2] = !leds[2];
	ticks[2]++;
}

// bar length for a count : grows BAR_STEP pixels per tick, back to 0 after full
int bar_len(uint32_t n)
{
	return (n % (BAR_MAX / BAR_STEP + 1)) * BAR_STEP;
}

// draw the whole LCD screen
void show_lcd(const uint32_t *now)
{
	char text[24];
	lcd.clear();

	// each Ticker, 11 pixel row : text + LED box, bar below it
	for (int i = 0; i < TICKERS; i++)
	{
		int y = i * ROW_H;
		snprintf(text, sizeof(text), "LED%d %4dms n=%lu", i + 1,
			(int)PERIOD[i].count(), (unsigned long)now[i]);
		lcd.text(0, y, text);
		if (leds[i]) lcd.fill_box(120, y, 7, 7);
		else         lcd.box(120, y, 7, 7);
		lcd.fill_box(0, y + 8, bar_len(now[i]), BAR_H);
	}

	lcd.update();
}

int main()
{
	uint32_t now[TICKERS];
	uint32_t last[TICKERS] = {0, 0, 0};	// counts already printed
	uint32_t start[TICKERS] = {0, 0, 0};	// counts at the start of the 1 s window

	lcd.init();

	printf("\r\n=== Task 3B : 3 Tickers on LPC1768 ===\r\n");
	for (int i = 0; i < TICKERS; i++)
		printf("Ticker %d : LED%d toggles every %4d ms\r\n", i + 1, i + 1, (int)PERIOD[i].count());
	printf("\r\n");

	for (int i = 0; i < TICKERS; i++) leds[i] = 0;

	run_timer.start();
	rate_timer.start();
	lcd_timer.start();
	ticker1.attach(&tick1, PERIOD[0]);	// the address of the function and the interval
	ticker2.attach(&tick2, PERIOD[1]);
	ticker3.attach(&tick3, PERIOD[2]);

	while (1)
	{
		for (int i = 0; i < TICKERS; i++) now[i] = ticks[i];	// copy, the Tickers keep counting

		// every change of a Ticker
		for (int i = 0; i < TICKERS; i++)
		{
			if (now[i] != last[i])
			{
				printf("%6lu ms | Ticker %d | LED%d %s | count %3lu | bar %3d/%d\r\n",
					(unsigned long)(run_timer.elapsed_time() / 1ms), i + 1, i + 1,
					leds[i] ? "ON " : "OFF", (unsigned long)now[i], bar_len(now[i]), BAR_MAX);
				last[i] = now[i];
			}
		}

		// count rate of each Ticker in the last second
		if (rate_timer.elapsed_time() >= RATE_TIME)
		{
			rate_timer.reset();
			printf("------ rate (counts / s) :");
			for (int i = 0; i < TICKERS; i++)
			{
				printf("  T%d = %lu", i + 1, (unsigned long)(now[i] - start[i]));
				start[i] = now[i];
			}
			printf("   | total : %lu  %lu  %lu ------\r\n",
				(unsigned long)now[0], (unsigned long)now[1], (unsigned long)now[2]);
		}

		// redraw the LCD
		if (lcd_timer.elapsed_time() >= LCD_TIME)
		{
			lcd_timer.reset();
			show_lcd(now);
		}

		ThisThread::sleep_for(LOOP_TIME);
	}
}
