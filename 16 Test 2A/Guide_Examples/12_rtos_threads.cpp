//
// Example 12 : RTOS Thread - three tasks running at the same time
//
//   thread_led1 : LED1 toggles every 200 ms
//   thread_led2 : LED2 toggles every 500 ms
//   thread_pot  : reads POT1 every 100 ms
//   main()      : is also a thread; it shows the counters and POT1 on the LCD
//                 every 250 ms
//
//   Each thread is a normal function with its own while (true) loop.
//   ThisThread::sleep_for() gives the CPU to the other threads (no busy waiting).
//   Stack size: 1024 bytes is enough for a small loop, use 2048 if the
//   thread calls printf / sprintf.
//
//   POT1 p19   LCD p5 p6 p7 p8 p11   LED1 LED2 on the mbed module
//
#include "mbed.h"
#include "lcd_c12832.h"

DigitalOut led1(LED1), led2(LED2);
AnalogIn   pot1(p19);
LCD_C12832 lcd;

Thread thread_led1(osPriorityNormal, 1024);	// priority, stack size (bytes)
Thread thread_led2(osPriorityNormal, 1024);
Thread thread_pot(osPriorityNormal, 1024);

volatile int n1 = 0, n2 = 0;		// how many times each LED thread ran
volatile int pot_value = 0;		// written by thread_pot, read by main

void led1_task()
{
	while (true) {
		led1 = !led1;
		n1++;
		ThisThread::sleep_for(200ms);
	}
}

void led2_task()
{
	while (true) {
		led2 = !led2;
		n2++;
		ThisThread::sleep_for(500ms);
	}
}

void pot_task()
{
	while (true) {
		pot_value = pot1.read_u16() >> 4;	// 0 ~ 4095
		ThisThread::sleep_for(100ms);
	}
}

int main()
{
	char text[24];

	lcd.init();
	thread_led1.start(led1_task);		// threads start running now
	thread_led2.start(led2_task);
	thread_pot.start(pot_task);

	while (true) {				// main is the 4th thread
		lcd.clear();
		lcd.text(0, 0, "3 threads + main");
		sprintf(text, "LED1 %4d LED2 %4d", n1, n2);
		lcd.text(0, 11, text);
		sprintf(text, "POT1 %4d", pot_value);
		lcd.text(0, 22, text);
		lcd.update();
		ThisThread::sleep_for(250ms);
	}
}
