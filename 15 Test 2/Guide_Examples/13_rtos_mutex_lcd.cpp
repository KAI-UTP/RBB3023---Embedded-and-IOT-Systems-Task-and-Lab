//
// Example 13 : RTOS Mutex - two threads sharing the LCD safely
//
//   thread_pot draws the top half of the LCD (POT1 bar) every 100 ms
//   thread_joy draws the bottom half (joystick direction) every 50 ms
//   Both use the SAME lcd buffer and SPI bus, so only one may draw at a time:
//   lcd_mutex.lock() ... lcd_mutex.unlock() around every LCD use.
//
//   Without the mutex, one thread can clear() or update() while the other is
//   half way through drawing -> flicker, missing parts or garbage on the LCD.
//
//   Mutex rules: lock as short as possible, always unlock, never in an ISR.
//
//   POT1 p19   Joystick down p12, left p13, centre p14, up p15, right p16
//
#include "mbed.h"
#include "lcd_c12832.h"

AnalogIn   pot1(p19);
DigitalIn  joy_up(p15, PullDown), joy_down(p12, PullDown);
DigitalIn  joy_left(p13, PullDown), joy_right(p16, PullDown);
DigitalIn  joy_centre(p14, PullDown);
LCD_C12832 lcd;
Mutex      lcd_mutex;			// protects lcd (buffer + SPI)

Thread thread_pot(osPriorityNormal, 2048);
Thread thread_joy(osPriorityNormal, 2048);

void pot_task()
{
	char text[24];

	while (true) {
		int v = pot1.read_u16() >> 4;

		lcd_mutex.lock();			// ---- only this thread draws now
		lcd.fill_box(0, 0, 128, 16, false);	// erase the top half only
		sprintf(text, "POT1 %4d", v);
		lcd.text(0, 0, text);
		lcd.box(60, 0, 68, 7);
		lcd.fill_box(61, 1, v * 66 / 4095, 5);
		lcd.update();
		lcd_mutex.unlock();			// ---- other threads may draw
		ThisThread::sleep_for(100ms);
	}
}

void joy_task()
{
	while (true) {
		const char *dir = "-";

		if (joy_up)     dir = "UP";
		if (joy_down)   dir = "DOWN";
		if (joy_left)   dir = "LEFT";
		if (joy_right)  dir = "RIGHT";
		if (joy_centre) dir = "CENTRE";

		lcd_mutex.lock();
		lcd.fill_box(0, 16, 128, 16, false);	// erase the bottom half only
		lcd.text(0, 20, "Joystick:");
		lcd.text(60, 20, dir);
		lcd.update();
		lcd_mutex.unlock();
		ThisThread::sleep_for(50ms);
	}
}

int main()
{
	lcd.init();				// before the threads start
	thread_pot.start(pot_task);
	thread_joy.start(joy_task);

	while (true)				// main has nothing else to do
		ThisThread::sleep_for(1s);
}
