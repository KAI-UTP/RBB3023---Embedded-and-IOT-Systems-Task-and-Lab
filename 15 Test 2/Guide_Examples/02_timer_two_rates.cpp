//
// Example 02 : Timer - scheduled programming by polling (lecture 08a, Exercise 1 + 2)
//
//   task1 toggles LED1 every 200 ms  (timer1)
//   task2 toggles LED2 every 1000 ms (timer2)
//   The main loop keeps reading both timers and runs a task when it is due.
//
//   Lecture (mbed 2)         ->  Mbed OS 6
//   timer1.read_ms() >= 200  ->  timer1.elapsed_time() >= 200ms
//   DigitalOut output1(p5)   ->  LED1  (p5 is the LCD on the app board)
//
#include "mbed.h"

Timer timer1, timer2;		// define timer objects
DigitalOut output1(LED1);
DigitalOut output2(LED2);

void task1(void);		// task function prototypes
void task2(void);

int main()
{
	timer1.start();		// start timers counting
	timer2.start();

	while (true)
	{
		if (timer1.elapsed_time() >= 200ms)	// read time
		{
			task1();
			timer1.reset();			// back to 0
		}
		if (timer2.elapsed_time() >= 1000ms)
		{
			task2();
			timer2.reset();
		}
	}
}

void task1(void)
{
	output1 = !output1;	// toggle LED1
}

void task2(void)
{
	output2 = !output2;	// toggle LED2
}
