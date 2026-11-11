//
// Example 06 : AnalogIn (ADC) - POT1 / POT2 to the LCD and the PC serial monitor
//
//   POT1 p19, POT2 p20 : 12-bit ADC, 0 ~ 3.3 V
//     read()      -> 0.0 ~ 1.0
//     read_u16()  -> 0 ~ 65535, >> 4 = 0 ~ 4095 (the real 12 bits)
//     millivolts  = raw * 3300 / 4095
//   Serial : one line every 0.5 s, 115200 baud (mbed_app.json)
//     POT1 2048  1650 mV  50% | POT2  410   330 mV  10%
//   LCD : value, voltage, % and a bar for each pot
//
#include "mbed.h"
#include "lcd_c12832.h"

AnalogIn   pot1(p19);
AnalogIn   pot2(p20);
LCD_C12832 lcd;

// average of 8 readings so the value does not jitter, 0 ~ 4095
int read_pot(AnalogIn &pot)
{
	int sum = 0;

	for (int i = 0; i < 8; i++)
		sum += pot.read_u16() >> 4;
	return sum / 8;
}

// one pot: text line at y, bar under it
void show_pot(int y, int num, int raw)
{
	char text[24];
	int  mv  = raw * 3300 / 4095;
	int  pct = raw * 100 / 4095;

	// printf / sprintf have no %f by default -> print volts as V.vv with integers
	sprintf(text, "POT%d %4d %d.%02dV %3d%%", num, raw, mv / 1000, mv % 1000 / 10, pct);
	lcd.text(0, y, text);
	lcd.box(0, y + 9, 128, 6);
	lcd.fill_box(1, y + 10, raw * 126 / 4095, 4);
}

int main()
{
	lcd.init();
	printf("\r\nPOT1 (p19) / POT2 (p20)\r\n");

	while (true)
	{
		int v1 = read_pot(pot1);
		int v2 = read_pot(pot2);

		lcd.clear();
		show_pot(0, 1, v1);
		show_pot(16, 2, v2);
		lcd.update();

		printf("POT1 %4d %5d mV %3d%% | POT2 %4d %5d mV %3d%%\r\n",
		       v1, v1 * 3300 / 4095, v1 * 100 / 4095,
		       v2, v2 * 3300 / 4095, v2 * 100 / 4095);
		ThisThread::sleep_for(500ms);
	}
}
