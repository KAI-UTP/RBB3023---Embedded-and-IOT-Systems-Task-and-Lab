//
// Example 18 : five code modules put together (section 15)
//
//   POT1 sets the speed of a running light on LED1 ~ LED4 (100 ~ 1000 ms)
//   LCD line 1 : "Speed 450 ms", line 2 : a bar for POT1, line 3 : LED number
//   Modules used : M01 LEDs, M04 LCD text, M05 bar, M07 Timer task,
//                  M08 running LEDs, M09 pots
//
#include "mbed.h"
#include "lcd_c12832.h"

// ---- M01 : LED1 ~ LED4 ----
DigitalOut led[4] = { DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3), DigitalOut(LED4) };

void led_one(int n)
{
	for (int i = 0; i < 4; i++)
		led[i] = (i == n);
}

// ---- M04 : LCD ----
LCD_C12832 lcd;
char lcd_text[24];

// ---- M05 : bar that grows right ----
void lcd_hbar(int y, int value, int max)
{
	lcd.box(0, y, 128, 6);
	lcd.fill_box(1, y + 1, value * 126 / max, 4);
}

// ---- M07 : Timer task ----
Timer task_timer;
int task_ms = 300;

// ---- M08 : running LEDs ----
int run_pos = 0;

void run_forward() { run_pos = (run_pos + 1) % 4;  led_one(run_pos); }

// ---- M09 : pots ----
AnalogIn pot1(p19);

int read_pot(AnalogIn &pot)
{
	int sum = 0;
	for (int i = 0; i < 8; i++)
		sum += pot.read_u16() >> 4;
	return sum / 8;
}

int map_pot(int raw, int lo, int hi) { return lo + (hi - lo) * raw / 4095; }

int main()
{
	lcd.init();				// M04 setup
	task_timer.start();			// M07 setup

	while (true)
	{
		int p1 = read_pot(pot1);		// M09
		task_ms = map_pot(p1, 1000, 100);	// pot up = faster

		if (task_timer.elapsed_time() >= std::chrono::milliseconds(task_ms))	// M07
		{
			task_timer.reset();
			run_forward();			// M08
		}

		lcd.clear();				// M04 + M05
		sprintf(lcd_text, "Speed %d ms", task_ms);
		lcd.text(0, 0, lcd_text);
		lcd_hbar(11, p1, 4095);
		sprintf(lcd_text, "LED%d on", run_pos + 1);
		lcd.text(0, 22, lcd_text);
		lcd.update();

		ThisThread::sleep_for(20ms);
	}
}
