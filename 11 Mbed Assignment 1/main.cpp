//
// Assignment 1 : read the two variable resistors POT1 and POT2 on the mbed
//                Application Board and show the readings on the LCD
//
//   POT1 : p19 (AnalogIn)      POT2 : p20 (AnalogIn)
//          LPC1768 ADC is 12-bit, Vref = 3.3V
//          read_u16() gives 0 ~ 65535, >> 4 = 0 ~ 4095 (the real 12 bits)
//          8 readings are averaged so the numbers do not jitter
//
//   LCD  : 128x32 (lcd_c12832.h), for each pot
//            text  "POT1 2048 1.65V  50%"
//            bar   0 ~ 128 px = 0 ~ 100 %
//   LED1 / LED2 : brightness follows POT1 / POT2 (PwmOut)
//   Serial monitor (USB, 115200 baud, see mbed_app.json) : both readings
//          every 0.5 s
//
//   Turn POT1 / POT2 clockwise = bigger value, longer bar, brighter LED
//
//   mbed Application Board pins
//     LCD : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//
#include "mbed.h"
#include "lcd_c12832.h"

#define SAMPLES		8		// readings averaged per pot
#define LOOP_TIME	100ms		// LCD refresh
#define PRINT_EVERY	5		// serial print every 5 loops = 0.5 s

AnalogIn pot1(p19);
AnalogIn pot2(p20);
PwmOut   led1(LED1);
PwmOut   led2(LED2);

LCD_C12832 lcd;			// 128x32 LCD on the application board

// average of SAMPLES readings, 0 ~ 4095
uint16_t read_pot(AnalogIn &pot)
{
	uint32_t sum = 0;

	for (int i = 0; i < SAMPLES; i++)
		sum += pot.read_u16() >> 4;	// 16-bit -> 12-bit
	return sum / SAMPLES;
}

// one pot on the LCD : text line at y, bar below it
void show_pot(int y, int num, uint16_t value)
{
	char text[24];
	uint32_t mv = value * 3300 / 4095;	// 0 ~ 3300 mV

	sprintf(text, "POT%d %4d %d.%02dV %3d%%", num, value,
	        (int)(mv / 1000), (int)(mv % 1000 / 10), (int)(value * 100 / 4095));
	lcd.text(0, y, text);

	lcd.box(0, y + 9, 128, 6);			// bar frame
	lcd.fill_box(1, y + 10, value * 126 / 4095, 4);	// bar 0 ~ 126 px
}

int main()
{
	uint16_t v1, v2;
	int loops = 0;

	lcd.init();
	led1.period_ms(1);
	led2.period_ms(1);

	printf("\r\nAssignment 1 : POT1 (p19) and POT2 (p20)\r\n");

	while (true)
	{
		v1 = read_pot(pot1);
		v2 = read_pot(pot2);

		led1 = v1 / 4095.0f;			// LED brightness 0 ~ 100 %
		led2 = v2 / 4095.0f;

		lcd.clear();
		show_pot(0, 1, v1);
		show_pot(16, 2, v2);
		lcd.update();

		if (++loops >= PRINT_EVERY)
		{
			loops = 0;
			printf("POT1 = %4d (%3d%%)   POT2 = %4d (%3d%%)\r\n",
			       v1, (int)(v1 * 100 / 4095), v2, (int)(v2 * 100 / 4095));
		}

		ThisThread::sleep_for(LOOP_TIME);
	}
}
