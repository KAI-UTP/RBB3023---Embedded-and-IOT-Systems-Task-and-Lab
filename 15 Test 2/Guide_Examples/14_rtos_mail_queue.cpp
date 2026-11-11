//
// Example 14 : RTOS Mail and Queue - sending data between threads (and from an ISR)
//
//   producer thread : reads POT1 + POT2 every 200 ms and sends them as Mail
//   joystick ISR    : puts the pressed button (1 ~ 5) into a Queue
//   consumer (main) : waits for Mail, shows the values on the LCD,
//                     prints them, and also shows the last joystick button
//
//   Mail<T, N>  : a box of N messages of type T (struct). The sender asks for an
//                 empty slot (try_alloc), fills it, put()s it; the receiver
//                 gets it (try_get_for) and must free() it.
//   Queue<T, N> : passes POINTERS (or small numbers cast to pointers).
//   try_alloc, put, try_put are ISR safe -> a good way to get data OUT of an ISR.
//
//   POT1 p19, POT2 p20   Joystick down p12, left p13, centre p14, up p15, right p16
//
#include "mbed.h"
#include "lcd_c12832.h"

typedef struct {
	int pot1;
	int pot2;
	int seq;			// message number
} pot_msg_t;

Mail<pot_msg_t, 8>  pot_mail;		// up to 8 messages waiting
Queue<void, 8>      joy_queue;		// button numbers from the ISR

AnalogIn    pot1(p19), pot2(p20);
InterruptIn joy_up(p15), joy_down(p12), joy_centre(p14);
LCD_C12832  lcd;
Thread      producer(osPriorityNormal, 1024);

void producer_task()
{
	int seq = 0;

	while (true) {
		pot_msg_t *m = pot_mail.try_alloc();	// get an empty slot
		if (m != NULL) {			// NULL = mailbox full
			m->pot1 = pot1.read_u16() >> 4;
			m->pot2 = pot2.read_u16() >> 4;
			m->seq  = ++seq;
			pot_mail.put(m);		// send
		}
		ThisThread::sleep_for(200ms);
	}
}

// ISR functions: only try_put a number into the queue
void on_up()     { joy_queue.try_put((void *)1); }
void on_down()   { joy_queue.try_put((void *)2); }
void on_centre() { joy_queue.try_put((void *)3); }

int main()
{
	char text[24];
	int  last_button = 0;

	lcd.init();
	joy_up.mode(PullDown);     joy_up.rise(&on_up);
	joy_down.mode(PullDown);   joy_down.rise(&on_down);
	joy_centre.mode(PullDown); joy_centre.rise(&on_centre);
	producer.start(producer_task);

	while (true) {
		// wait up to 1 s for the next pot message
		pot_msg_t *m = pot_mail.try_get_for(1s);

		// empty the button queue without waiting (0ms)
		void *b;
		while (joy_queue.try_get_for(0ms, &b))
			last_button = (int)b;

		lcd.clear();
		if (m != NULL) {
			sprintf(text, "#%d", m->seq);
			lcd.text(0, 0, text);
			sprintf(text, "POT1 %4d POT2 %4d", m->pot1, m->pot2);
			lcd.text(0, 11, text);
			printf("#%d POT1 %d POT2 %d\r\n", m->seq, m->pot1, m->pot2);
			pot_mail.free(m);		// ALWAYS give the slot back
		} else {
			lcd.text(0, 11, "no mail for 1 s");
		}
		sprintf(text, "button %d (1U 2D 3C)", last_button);
		lcd.text(0, 22, text);
		lcd.update();
	}
}
