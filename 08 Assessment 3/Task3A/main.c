//
// Task 3A : Timer0 and Timer1 control the length of 2 bars on the LCD
//
//   LCD     : "Bar 1" and "Bar 2", each bar grows to the right inside a frame
//             bottom 2 lines show the timer period and the bar length
//   Timer0  : periodic, 100 ms -> Bar 1 grows 3 pixels  (full in 3 s)
//   Timer1  : periodic, 300 ms -> Bar 2 grows 3 pixels  (full in 9 s)
//             time out = (1/12MHz) * (11 + 1) * TCMPR
//                      = 1 us * 100000 = 100 ms  (Timer0)
//                      = 1 us * 300000 = 300 ms  (Timer1)
//   Reset   : a bar stays full for one period, then goes back to 0
//   LEDs    : GPC12 toggles on every Timer0 interrupt,
//             GPC13 toggles on every Timer1 interrupt (0 = on)
//   Keypad  : speed control, the timer period changes 50 ms per press (50 ~ 1000 ms)
//             1 = Bar 1 faster (high)   7 = Bar 1 slower (low)    -> Timer0
//             3 = Bar 2 faster (high)   9 = Bar 2 slower (low)    -> Timer1
//             the new period is written to TCMPR, shown on the bottom lines
//   HCLK    : 12MHz crystal (as in the lecture sample)
//
//   The interrupts only change the bar lengths, the main loop draws the LCD
//   and scans the keypad (GPA0~5, ScanKey() returns 1~9, 0 = no key).
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"
#include "Scankey.h"
#include "Font5x7.h"

#define ONESHOT		0	// counting and interrupt when reach TCMPR number, then stop
#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again
#define TOGGLE		2	// keep counting and interrupt when reach TCMPR number, tout toggled (between 0 and 1)
#define CONTINUOUS	3	// keep counting and interrupt when reach TCMPR number

#define T0_MS		100	// Timer0 period at the start
#define T1_MS		300	// Timer1 period at the start
#define MS_STEP		50	// change per key press
#define MS_MIN		50	// fastest
#define MS_MAX		1000	// slowest
#define STEP		3	// pixels added on each interrupt
#define BAR_MAX		90	// max length (30 steps)

#define BAR_X		36	// left side of the bar frames
#define BAR1_Y		14
#define BAR2_Y		31
#define BAR_H		11

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

volatile uint8_t bar1 = 0;	// length of Bar 1 in pixels, 0 ~ BAR_MAX
volatile uint8_t bar2 = 0;	// length of Bar 2
volatile uint8_t dirty = 1;	// 1 = LCD needs to be drawn again
volatile uint32_t led1 = 0, led2 = 0;

uint16_t t0_ms = T0_MS;		// Timer0 period now (Bar 1 speed)
uint16_t t1_ms = T1_MS;		// Timer1 period now (Bar 2 speed)

// ---------------- Timer interrupts ----------------

// Timer0 interrupt : every 100 ms, Bar 1 longer (or back to 0 when full)
void TMR0_IRQHandler(void)
{
	if (bar1 >= BAR_MAX) bar1 = 0;
	else                 bar1 += STEP;

	led1 = ~led1;
	if (led1) DrvGPIO_ClrBit(E_GPC, 12);
	else      DrvGPIO_SetBit(E_GPC, 12);

	dirty = 1;
	TIMER0->TISR.TIF = 1;		// clear interrupt flag
}

// Timer1 interrupt : every 300 ms, Bar 2 longer (or back to 0 when full)
void TMR1_IRQHandler(void)
{
	if (bar2 >= BAR_MAX) bar2 = 0;
	else                 bar2 += STEP;

	led2 = ~led2;
	if (led2) DrvGPIO_ClrBit(E_GPC, 13);
	else      DrvGPIO_SetBit(E_GPC, 13);

	dirty = 1;
	TIMER1->TISR.TIF = 1;		// clear interrupt flag
}

void InitTIMER0(void)
{
	/* Step 1. Enable and select Timer clock source */
	SYSCLK->CLKSEL1.TMR0_S = 0;	// select 12MHz for Timer0 clock source
	SYSCLK->APBCLK.TMR0_EN = 1;	// enable Timer0 clock source
	/* Step 2. Select operation mode */
	TIMER0->TCSR.MODE = PERIODIC;	// select operation mode
	/* Step 3. Time out period = (period of timer clock input) * (8-bit prescale + 1) * (24-bit TCMP) */
	TIMER0->TCSR.PRESCALE = 11;	// 12MHz / (11 + 1) = 1MHz, 1 us per count
	TIMER0->TCMPR = t0_ms * 1000;	// 1 us * 100000 = 100 ms
	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR0_IRQn);	// enable Timer0 interrupt
	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;		// reset up counter
	TIMER0->TCSR.CEN = 1;		// enable Timer0
}

void InitTIMER1(void)
{
	/* Step 1. Enable and select Timer clock source */
	SYSCLK->CLKSEL1.TMR1_S = 0;	// select 12MHz for Timer1 clock source
	SYSCLK->APBCLK.TMR1_EN = 1;	// enable Timer1 clock source
	/* Step 2. Select operation mode */
	TIMER1->TCSR.MODE = PERIODIC;	// select operation mode
	/* Step 3. Time out period = (period of timer clock input) * (8-bit prescale + 1) * (24-bit TCMP) */
	TIMER1->TCSR.PRESCALE = 11;	// 12MHz / (11 + 1) = 1MHz, 1 us per count
	TIMER1->TCMPR = t1_ms * 1000;	// 1 us * 300000 = 300 ms
	/* Step 4. Enable interrupt */
	TIMER1->TCSR.IE = 1;
	TIMER1->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR1_IRQn);	// enable Timer1 interrupt
	/* Step 5. Enable Timer module */
	TIMER1->TCSR.CRST = 1;		// reset up counter
	TIMER1->TCSR.CEN = 1;		// enable Timer1
}

// new time out period for a running timer : stop, new TCMPR, count again from 0
void set_period(TIMER_T *timer, uint16_t ms)
{
	timer->TCSR.CEN = 0;		// stop counting
	timer->TCMPR = ms * 1000;	// 1 us per count
	timer->TCSR.CRST = 1;		// reset up counter
	timer->TCSR.CEN = 1;		// start again
}

// key 1 / 7 : Timer0 faster / slower,  key 3 / 9 : Timer1 faster / slower
void speed_control(uint8_t key)
{
	switch (key)
	{
		case 1: if (t0_ms > MS_MIN) { t0_ms -= MS_STEP; set_period(TIMER0, t0_ms); } break;
		case 7: if (t0_ms < MS_MAX) { t0_ms += MS_STEP; set_period(TIMER0, t0_ms); } break;
		case 3: if (t1_ms > MS_MIN) { t1_ms -= MS_STEP; set_period(TIMER1, t1_ms); } break;
		case 9: if (t1_ms < MS_MAX) { t1_ms += MS_STEP; set_period(TIMER1, t1_ms); } break;
		default: return;		// other keys do nothing
	}
	dirty = 1;			// show the new period
}

// ---------------- drawing into screen[] ----------------

void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			set_pixel(x + i, y + j);
}

// outline of a rectangle
void rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i;

	for (i = 0; i < w; i++) { set_pixel(x + i, y); set_pixel(x + i, y + h - 1); }
	for (i = 0; i < h; i++) { set_pixel(x, y + i); set_pixel(x + w - 1, y + i); }
}

// 5x7 character, 6 pixels wide with the gap
void draw_char(int16_t x, int16_t y, char c)
{
	uint8_t i, b, col;

	if (c < 0x20 || c > 0x7F) c = ' ';
	for (i = 0; i < 5; i++)
	{
		col = Font5x7[(c - 0x20) * 5 + i];
		for (b = 0; b < 8; b++)
			if (col & (1 << b)) set_pixel(x + i, y + b);
	}
}

void draw_text(int16_t x, int16_t y, const char *s)
{
	while (*s)
	{
		draw_char(x, y, *s++);
		x += 6;
	}
}

// one bar : label, frame, and the filled part
void draw_bar(int16_t y, const char *label, uint8_t len)
{
	draw_text(0, y + 2, label);
	rect(BAR_X, y, BAR_MAX + 2, BAR_H);
	fill_rect(BAR_X + 1, y + 1, len, BAR_H - 2);
}

void lcd_display(uint8_t len1, uint8_t len2)
{
	uint16_t i;
	char text[24];

	for (i = 0; i < sizeof(screen); i++) screen[i] = 0;

	draw_text(31, 0, "LCD Display");
	for (i = 0; i < LCD_Xmax; i += 2) set_pixel(i, 9);	// dotted line under the title

	draw_bar(BAR1_Y, "Bar 1", len1);
	draw_bar(BAR2_Y, "Bar 2", len2);

	sprintf(text, "T0 %4dms %2d/%d", t0_ms, len1, BAR_MAX);
	draw_text(0, 47, text);
	sprintf(text, "T1 %4dms %2d/%d", t1_ms, len2, BAR_MAX);
	draw_text(0, 56, text);

	draw_LCD(screen);
}

int32_t main (void)
{
	uint8_t len1, len2;
	uint8_t key, last_key = 0;

	UNLOCKREG();
	SYSCLK->PWRCON.XTL12M_EN = 1;	// enable 12MHz crystal
	DrvSYS_Delay(5000);		// wait for 12MHz crystal stable
	SYSCLK->CLKSEL0.HCLK_S = 0;	// HCLK = 12MHz crystal
	LOCKREG();

	DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT);	// LEDs for the two timers
	DrvGPIO_Open(E_GPC, 13, E_IO_OUTPUT);
	DrvGPIO_SetBit(E_GPC, 12);		// LEDs off
	DrvGPIO_SetBit(E_GPC, 13);

	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad

	InitTIMER0();			// Bar 1
	InitTIMER1();			// Bar 2

	while (1)
	{
		key = ScanKey();
		if (key != 0 && last_key == 0)	// new key press only, holding does not repeat
			speed_control(key);
		last_key = key;
		DrvSYS_Delay(10000);		// 10 ms, keypad debounce

		if (dirty)
		{
			dirty = 0;
			len1 = bar1;		// copy, the interrupts may change them while drawing
			len2 = bar2;
			lcd_display(len1, len2);
		}
	}
}
