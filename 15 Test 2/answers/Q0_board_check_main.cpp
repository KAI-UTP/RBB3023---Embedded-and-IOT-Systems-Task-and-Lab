#include "mbed.h"
#include "lcd_c12832.h"

AnalogIn   pot1(p19);
AnalogIn   pot2(p20);
DigitalOut led1(LED1);
DigitalIn  joy_down(p12, PullDown);
DigitalIn  joy_left(p13, PullDown);
DigitalIn  joy_centre(p14, PullDown);
DigitalIn  joy_up(p15, PullDown);
DigitalIn  joy_right(p16, PullDown);

LCD_C12832 lcd;
Ticker     tick;
volatile bool tick_flag = false;

// Ticker callback runs in interrupt context: only set a flag here
void on_tick()
{
	tick_flag = true;
}

int main()
{
	char text[32];

	lcd.init();
	tick.attach(&on_tick, 500ms);
	printf("\r\nTest 2 board check\r\n");

	while (true)
	{
		uint16_t v1 = pot1.read_u16() >> 4;	// 0 ~ 4095
		uint16_t v2 = pot2.read_u16() >> 4;

		lcd.clear();
		lcd.text(0, 0, "Test 2 ready");
		sprintf(text, "POT1 %4d POT2 %4d", v1, v2);
		lcd.text(0, 11, text);
		sprintf(text, "JOY %c %c %c %c %c",
		        joy_up ? 'U' : '-', joy_down ? 'D' : '-',
		        joy_left ? 'L' : '-', joy_right ? 'R' : '-',
		        joy_centre ? 'C' : '-');
		lcd.text(0, 22, text);
		lcd.update();

		if (tick_flag)
		{
			tick_flag = false;
			led1 = !led1;
			printf("POT1 %4d  POT2 %4d\r\n", v1, v2);
		}
		wait_us(20000);				// ~50 LCD refreshes per second
	}
}
