//
// Assignment 2 : LPC1768 + mbed Application Board
//
//   1. Serial link to a Serial Port Monitor on the PC
//          USB serial, 115200 baud (see mbed_app.json)
//   2. ADC readings of POT1 and POT2 are sent to the serial monitor
//          every 0.5 s, e.g.
//          POT1 = 2048 ( 50%) 1.65V | POT2 = 1000 ( 24%) 0.80V | POT1 -> LEDs 3
//   3. Number of LEDs on (LED1 ~ LED4) follows the selected pot
//          < 25 %  : 1 LED     < 50 % : 2 LEDs
//          < 75 %  : 3 LEDs   >= 75 % : 4 LEDs
//   4. LCD shows a bar graph for each pot (0 ~ 100 %)
//          small ticks under the bar mark 25 / 50 / 75 % (the LED steps)
//   5. LCD also shows the reading, voltage and percentage of each pot
//          '>' in front = the pot that drives the LEDs
//
//   The LEDs follow the pot that was turned last (moved more than ~1.5 %),
//   so both pots control the LEDs without touching anything else
//   Joystick : left = POT1 drives the LEDs, right = POT2, centre = swap
//
//   POT1 : p19 (AnalogIn)      POT2 : p20 (AnalogIn)
//          LPC1768 ADC is 12-bit, Vref = 3.3V
//          read_u16() gives 0 ~ 65535, >> 4 = 0 ~ 4095 (the real 12 bits)
//          8 readings are averaged so the numbers do not jitter
//
//   mbed Application Board pins
//     LCD      : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//     Joystick : left p13, centre p14, right p16 (high = pressed)
//     LED1 ~ LED4 are the 4 blue LEDs on the LPC1768 module (high = on)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define SAMPLES		8		// readings averaged per pot
#define LOOP_TIME	100ms		// LCD / LED refresh
#define PRINT_EVERY	5		// serial print every 5 loops = 0.5 s
#define MOVE_STEP	60		// ~1.5 % change = "this pot was turned"

AnalogIn   pot1(p19);
AnalogIn   pot2(p20);
DigitalOut leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                       DigitalOut(LED3), DigitalOut(LED4) };
DigitalIn  joy_left(p13, PullDown);
DigitalIn  joy_centre(p14, PullDown);
DigitalIn  joy_right(p16, PullDown);

LCD_C12832 lcd;			// 128x32 LCD on the application board

// average of SAMPLES readings, 0 ~ 4095
uint16_t read_pot(AnalogIn &pot)
{
	uint32_t sum = 0;

	for (int i = 0; i < SAMPLES; i++)
		sum += pot.read_u16() >> 4;	// 16-bit -> 12-bit
	return sum / SAMPLES;
}

int percent(uint16_t value)
{
	return value * 100 / 4095;		// 0 ~ 100 %
}

// number of LEDs for a percentage (task 3)
int led_count(int pct)
{
	if (pct < 25) return 1;
	if (pct < 50) return 2;
	if (pct < 75) return 3;
	return 4;
}

// LED1 ~ LEDn on, the rest off
void show_leds(int n)
{
	for (int i = 0; i < 4; i++)
		leds[i] = (i < n);
}

// one pot on the LCD (tasks 4 and 5) : text line at y, bar below it
//   ">POT1 2048 1.65V  50%"
//   [##########          ]
//         '    '    '        <- 25 / 50 / 75 % ticks
void show_pot(int y, int num, uint16_t value, bool selected)
{
	char text[32];
	uint32_t mv = value * 3300 / 4095;	// 0 ~ 3300 mV

	sprintf(text, "%cPOT%d %4d %d.%02dV %3d%%", selected ? '>' : ' ', num,
	        value, (int)(mv / 1000), (int)(mv % 1000 / 10), percent(value));
	lcd.text(0, y, text);

	lcd.box(0, y + 9, 128, 6);			// bar frame
	lcd.fill_box(1, y + 10, value * 126 / 4095, 4);	// bar 0 ~ 126 px
	for (int q = 1; q <= 3; q++)			// LED step ticks
		lcd.pixel(1 + q * 126 / 4, y + 15);
}

int main()
{
	uint16_t v1, v2, sel_value;
	int  sel = 1;				// pot that drives the LEDs
	bool last_centre = false;
	int  loops = 0;
	Timer t;

	lcd.init();
	t.start();

	// reference points to detect which pot is being turned
	int ref1 = read_pot(pot1), ref2 = read_pot(pot2);

	printf("\r\nAssignment 2 : POT1 (p19) / POT2 (p20) -> serial, LEDs, LCD\r\n");
	printf("Joystick left = POT1, right = POT2, centre = swap\r\n");

	while (true)
	{
		t.reset();

		// joystick picks the pot for the LEDs
		if (joy_left)  sel = 1;
		if (joy_right) sel = 2;
		if (joy_centre && !last_centre) sel = 3 - sel;	// edge = one swap per press
		last_centre = joy_centre;

		// task 2 : read both ADC channels
		v1 = read_pot(pot1);
		v2 = read_pot(pot2);

		// the pot turned last drives the LEDs (bigger move wins if both)
		int d1 = abs(v1 - ref1), d2 = abs(v2 - ref2);
		if (d1 > MOVE_STEP || d2 > MOVE_STEP)
		{
			sel = (d1 >= d2) ? 1 : 2;
			if (d1 > MOVE_STEP) ref1 = v1;
			if (d2 > MOVE_STEP) ref2 = v2;
		}
		sel_value = (sel == 1) ? v1 : v2;

		// task 3 : 1 ~ 4 LEDs
		show_leds(led_count(percent(sel_value)));

		// tasks 4 and 5 : bar + percentage on the LCD
		lcd.clear();
		show_pot(0, 1, v1, sel == 1);
		show_pot(16, 2, v2, sel == 2);
		lcd.update();

		// tasks 1 and 2 : readings to the PC serial monitor
		if (++loops >= PRINT_EVERY)
		{
			uint32_t mv1 = v1 * 3300 / 4095, mv2 = v2 * 3300 / 4095;

			loops = 0;
			printf("POT1 = %4d (%3d%%) %d.%02dV | POT2 = %4d (%3d%%) %d.%02dV | POT%d -> LEDs %d\r\n",
			       v1, percent(v1), (int)(mv1 / 1000), (int)(mv1 % 1000 / 10),
			       v2, percent(v2), (int)(mv2 / 1000), (int)(mv2 % 1000 / 10),
			       sel, led_count(percent(sel_value)));
		}

		// pace the loop with a Timer (sleep_for only woke once per second
		// on the board with the old interface firmware)
		while (t.elapsed_time() < LOOP_TIME) { }
	}
}
