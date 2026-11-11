//
// Example 16 : RTOS EventQueue - the easy way to do slow work "from" an interrupt
//
//   An EventQueue runs on its own thread (dispatch_forever). Anything you post
//   to it runs later in THAT thread, so printf and the LCD are allowed there.
//
//   queue.call_every(500ms, f) : f every 500 ms (like a Ticker, but in a thread)
//   queue.call(f)              : run f once, as soon as possible (ISR safe)
//   ISR { queue.call(f); }     : the button interrupt posts f to the queue
//                                -> f may use printf / LCD
//   (btn.rise(queue.event(f)) does NOT compile in Mbed OS 6 unless the option
//    platform.callback-nontrivial is turned on, so use a 1-line ISR instead)
//   queue.call_in(2s, f)       : run f once, 2 s from now (like a Timeout)
//
//   Joystick up p15 = +1, down p12 = -1, centre p14 = message for 2 s
//   POT1 p19 shown every 500 ms
//
#include "mbed.h"
#include "lcd_c12832.h"

EventQueue  queue(32 * EVENTS_EVENT_SIZE);	// room for 32 events
Thread      event_thread(osPriorityNormal, 4096);
InterruptIn joy_up(p15), joy_down(p12), joy_centre(p14);
AnalogIn    pot1(p19);
DigitalOut  led1(LED1);
LCD_C12832  lcd;

int  number = 0;
bool show_msg = false;

// everything below runs in event_thread, one at a time -> no mutex needed
void redraw()
{
	char text[24];

	lcd.clear();
	sprintf(text, "number %d", number);
	lcd.text(0, 0, text);
	sprintf(text, "POT1 %4d", pot1.read_u16() >> 4);
	lcd.text(0, 11, text);
	if (show_msg)
		lcd.text(0, 22, "centre pressed!");
	lcd.update();
}

void up()       { number++; printf("up   -> %d\r\n", number); redraw(); }
void down()     { number--; printf("down -> %d\r\n", number); redraw(); }
void msg_off()  { show_msg = false; redraw(); }
void centre()
{
	show_msg = true;
	printf("centre\r\n");
	redraw();
	queue.call_in(2s, msg_off);		// hide the message after 2 s
}
void blink()    { led1 = !led1; redraw(); }

// ISRs: only post the real work to the queue (queue.call is ISR safe)
void on_up()     { queue.call(up); }
void on_down()   { queue.call(down); }
void on_centre() { queue.call(centre); }

int main()
{
	lcd.init();
	joy_up.mode(PullDown);     joy_up.rise(&on_up);
	joy_down.mode(PullDown);   joy_down.rise(&on_down);
	joy_centre.mode(PullDown); joy_centre.rise(&on_centre);
	queue.call_every(500ms, blink);

	// the queue's thread runs the events forever
	event_thread.start(callback(&queue, &EventQueue::dispatch_forever));

	while (true)
		ThisThread::sleep_for(1s);
}
