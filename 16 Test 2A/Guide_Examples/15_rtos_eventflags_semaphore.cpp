//
// Example 15 : RTOS EventFlags and Semaphore - waking threads from interrupts
//
//   EventFlags : the joystick ISRs set a flag bit; thread_flags waits for any
//                bit and then does the slow work (printf, LCD).
//                UP = bit 0, DOWN = bit 1, CENTRE = bit 2
//   Semaphore  : a Ticker releases the semaphore every 1 s; thread_sem waits
//                (acquire) and toggles LED2 + counts seconds.
//   The threads sleep while waiting, so they use no CPU.
//
//   EventFlags.set() and Semaphore.release() are ISR safe.
//   wait_any() / acquire() are NOT: call them only from threads.
//
//   Joystick down p12, centre p14, up p15   LED1 LED2 on the mbed module
//
#include "mbed.h"
#include "lcd_c12832.h"

#define FLAG_UP		(1UL << 0)
#define FLAG_DOWN	(1UL << 1)
#define FLAG_CENTRE	(1UL << 2)

EventFlags  joy_flags;
Semaphore   second_sem(0);		// starts with 0 tokens -> acquire() waits
Ticker      tick;
InterruptIn joy_up(p15), joy_down(p12), joy_centre(p14);
DigitalOut  led1(LED1), led2(LED2);
LCD_C12832  lcd;
Mutex       lcd_mutex;

Thread thread_flags(osPriorityNormal, 2048);
Thread thread_sem(osPriorityNormal, 2048);

volatile int value = 50;		// changed by UP / DOWN
volatile int seconds = 0;

// ---- ISRs: just set a flag / release a token ----
void on_up()     { joy_flags.set(FLAG_UP); }
void on_down()   { joy_flags.set(FLAG_DOWN); }
void on_centre() { joy_flags.set(FLAG_CENTRE); }
void on_tick()   { second_sem.release(); }

void show()				// draw everything (called from both threads)
{
	char text[24];

	lcd_mutex.lock();
	lcd.clear();
	sprintf(text, "value %3d", value);
	lcd.text(0, 0, text);
	lcd.fill_box(0, 11, value * 128 / 100, 6);
	sprintf(text, "time %3d s", seconds);
	lcd.text(0, 22, text);
	lcd.update();
	lcd_mutex.unlock();
}

void flags_task()
{
	while (true) {
		// sleep until UP, DOWN or CENTRE happens; the bits are cleared on return
		uint32_t f = joy_flags.wait_any(FLAG_UP | FLAG_DOWN | FLAG_CENTRE);

		if ((f & FLAG_UP) && value < 100)  value += 10;
		if ((f & FLAG_DOWN) && value > 0)  value -= 10;
		if (f & FLAG_CENTRE)               value = 50;
		led1 = !led1;
		printf("flags 0x%lX -> value %d\r\n", (unsigned long)f, value);
		show();
		ThisThread::sleep_for(150ms);	// simple debounce
		joy_flags.clear();		// drop presses from switch bounce
	}
}

void sem_task()
{
	while (true) {
		second_sem.acquire();		// sleep until the Ticker releases
		seconds++;
		led2 = !led2;
		show();
	}
}

int main()
{
	lcd.init();
	joy_up.mode(PullDown);     joy_up.rise(&on_up);
	joy_down.mode(PullDown);   joy_down.rise(&on_down);
	joy_centre.mode(PullDown); joy_centre.rise(&on_centre);
	thread_flags.start(flags_task);
	thread_sem.start(sem_task);
	tick.attach(&on_tick, 1s);
	show();

	while (true)
		ThisThread::sleep_for(1s);
}
