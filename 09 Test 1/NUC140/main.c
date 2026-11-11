//
// Lab Test 1 : RBB3023 Embedded and IoT Systems (Nu-LB-NUC140)
//
//   Task 1 : Timer1, periodic 0.5 s, 7-segment counts DOWN 77 -> 5, then starts at 77 again
//            shown on the right 2 digits (GPC4 = ones, GPC5 = tens)
//            time out = (1/12MHz) * (11 + 1) * 500000 = 0.5 s
//   Task 2 : MSB (left) 7-segment digit, one segment runs clockwise
//            a -> b -> c -> d -> e -> f -> a ...
//            keypad (polling in the main loop) : 1 = increase delay (slower)
//                                                7 = decrease delay (faster)
//            delay 50 ~ 1000 ms, 50 ms per press, starts at 200 ms
//   Task 3 : Timer2, periodic 250 ms, bar on the left side of the LCD grows 2 px
//            higher every period, from 10 px up to 64 px (full), then back to 10 px
//   Task 4 : push button SW_INT (GPB15, 0 = pressed), every press adds one 8x8 box
//            beside the bar, starts with 0 boxes (6 boxes per column, max 36, then 0)
//
//   Timer0 : periodic 2 ms, scans the 4 digits of the 7-segment (multiplexing)
//            and counts milliseconds for the Task 2 timing
//   7-seg  : GPC4~7 digit select (1 = on), GPE0~7 segments (0 = on)
//            GPE0 c, GPE1 dot, GPE2 f, GPE3 a, GPE4 b, GPE5 d, GPE6 e, GPE7 g
//   LCD    : 128x64, frame buffer screen[] -> draw_LCD()
//   Keypad : GPA0~5, ScanKey() returns 1~9, 0 = no key
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"
#include "Font5x7.h"

#define ONESHOT		0	// counting and interrupt when reach TCMPR number, then stop
#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again
#define TOGGLE		2	// keep counting and interrupt when reach TCMPR number, tout toggled
#define CONTINUOUS	3	// keep counting and interrupt when reach TCMPR number

// Task 1
#define T1_US		500000	// Timer1 period 0.5 s 
#define CNT_START	77
#define CNT_END		5

// Task 2
#define KEY_SLOWER	1	// increase delay
#define KEY_FASTER	7	// decrease delay
#define DLY_START	200
#define DLY_STEP	50
#define DLY_MIN		50
#define DLY_MAX		1000

// Task 3
#define T2_US		250000	// Timer2 period 250 ms
#define BAR_W		10	// bar width, left side of LCD
#define BAR_START	10	// initial height
#define BAR_STEP	2	// pixels higher every Timer2 period
#define BAR_MAX		64	// full height

// Task 4
#define BOX_X		16	// first box column, beside the bar
#define BOX_SIZE	8
#define BOX_GAP		10	// box + 2 px gap
#define BOX_ROWS	6	// boxes per column (64 / 10)
#define BOX_COLS	6
#define BOX_MAX		(BOX_ROWS * BOX_COLS)

extern unsigned char SEG_BUF[16];	// digit patterns 0~9 from Seven_Segment.c

// segment bits (0 = on), clockwise order a, b, c, d, e, f
const uint8_t CW[6] = {0x08, 0x10, 0x01, 0x20, 0x40, 0x04};

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer

volatile uint32_t ms = 0;		// milliseconds, Timer0
volatile uint8_t  count = CNT_START;	// Task 1 counter, Timer1
volatile uint8_t  bar = BAR_START;	// Task 3 bar height, Timer2
volatile uint8_t  step = 0;		// Task 2 running segment 0~5
volatile uint8_t  dirty = 1;		// 1 = LCD needs to be drawn again
uint8_t  boxes = 0;			// Task 4 number of boxes
uint16_t delay_ms = DLY_START;		// Task 2 delay between steps

// ---------------- Timer interrupts ----------------

// Timer0 : every 2 ms, show the next 7-segment digit
void TMR0_IRQHandler(void)
{
	static uint8_t digit = 0;
	uint8_t seg;

	ms += 2;

	if      (digit == 0) seg = SEG_BUF[count % 10];	// ones
	else if (digit == 1) seg = (count >= 10) ? SEG_BUF[count / 10] : 0xFF;	// tens, no leading 0
	else if (digit == 2) seg = 0xFF;			// blank
	else                 seg = ~CW[step];		// MSB : running segment

	GPIOC->DOUT &= ~0x00F0;				// all digits off
	GPIOE->DOUT = (GPIOE->DOUT & 0xFF00) | seg;	// segments
	GPIOC->DOUT |= 1 << (4 + digit);		// this digit on

	digit = (digit + 1) & 3;
	TIMER0->TISR.TIF = 1;		// clear interrupt flag
}

// Timer1 : every 0.5 s, count down 77 -> 5
void TMR1_IRQHandler(void)
{
	if (count <= CNT_END) count = CNT_START;
	else                  count--;
	dirty = 1;
	TIMER1->TISR.TIF = 1;
}

// Timer2 : every 250 ms, bar higher
void TMR2_IRQHandler(void)
{
	if (bar >= BAR_MAX) bar = BAR_START;
	else                bar += BAR_STEP;
	dirty = 1;
	TIMER2->TISR.TIF = 1;
}

void InitTIMER0(void)
{
	SYSCLK->CLKSEL1.TMR0_S = 0;	// 12MHz crystal
	SYSCLK->APBCLK.TMR0_EN = 1;
	TIMER0->TCSR.MODE = PERIODIC;
	TIMER0->TCSR.PRESCALE = 11;	// 12MHz / 12 = 1MHz, 1 us per count
	TIMER0->TCMPR = 2000;		// 2 ms
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;
	NVIC_EnableIRQ(TMR0_IRQn);
	TIMER0->TCSR.CRST = 1;
	TIMER0->TCSR.CEN = 1;
}

void InitTIMER1(void)
{
	SYSCLK->CLKSEL1.TMR1_S = 0;	// 12MHz crystal
	SYSCLK->APBCLK.TMR1_EN = 1;
	TIMER1->TCSR.MODE = PERIODIC;
	TIMER1->TCSR.PRESCALE = 11;	// 1 us per count
	TIMER1->TCMPR = T1_US;		// (1/12MHz) * 12 * 500000 = 0.5 s
	TIMER1->TCSR.IE = 1;
	TIMER1->TISR.TIF = 1;
	NVIC_EnableIRQ(TMR1_IRQn);
	TIMER1->TCSR.CRST = 1;
	TIMER1->TCSR.CEN = 1;
}

void InitTIMER2(void)
{
	SYSCLK->CLKSEL1.TMR2_S = 0;	// 12MHz crystal
	SYSCLK->APBCLK.TMR2_EN = 1;
	TIMER2->TCSR.MODE = PERIODIC;
	TIMER2->TCSR.PRESCALE = 11;	// 1 us per count
	TIMER2->TCMPR = T2_US;		// 250 ms
	TIMER2->TCSR.IE = 1;
	TIMER2->TISR.TIF = 1;
	NVIC_EnableIRQ(TMR2_IRQn);
	TIMER2->TCSR.CRST = 1;
	TIMER2->TCSR.CEN = 1;
}

// ---------------- LCD drawing into screen[] ----------------

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

void lcd_display(void)
{
	uint16_t i;
	uint8_t h = bar, n = count;
	char text[12];

	for (i = 0; i < sizeof(screen); i++) screen[i] = 0;

	// Task 3 : bar on the left, grows up from the bottom
	fill_rect(0, LCD_Ymax - h, BAR_W, h);

	// Task 4 : boxes beside the bar, bottom to top, then next column
	for (i = 0; i < boxes; i++)
		fill_rect(BOX_X + (i / BOX_ROWS) * BOX_GAP,
		          LCD_Ymax - BOX_SIZE - (i % BOX_ROWS) * BOX_GAP,
		          BOX_SIZE, BOX_SIZE);

	// status text on the right
	draw_text(80, 0, "Test 1");
	sprintf(text, "Cnt %2d", n);
	draw_text(80, 14, text);
	sprintf(text, "Dly%4d", delay_ms);
	draw_text(80, 26, text);
	sprintf(text, "Box %2d", boxes);
	draw_text(80, 38, text);
	sprintf(text, "Bar %2d", h);
	draw_text(80, 50, text);

	draw_LCD(screen);
}

// ---------------- main ----------------

int32_t main(void)
{
	uint8_t i, key, last_key = 0;
	uint8_t btn, last_btn = 1;
	uint32_t last_step = 0;

	UNLOCKREG();
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();

	for (i = 12; i <= 15; i++)	// LEDs GPC12~15 off
	{
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
		DrvGPIO_SetBit(E_GPC, i);
	}
	for (i = 4; i <= 7; i++)	// 7-segment digit select
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
	for (i = 0; i <= 7; i++)	// 7-segment segments
		DrvGPIO_Open(E_GPE, i, E_IO_OUTPUT);
	close_seven_segment();

	DrvGPIO_Open(E_GPB, 15, E_IO_QUASI);	// push button SW_INT, pulled high
	DrvGPIO_SetBit(E_GPB, 15);

	OpenKeyPad();
	init_LCD();
	clear_LCD();

	InitTIMER0();			// 7-segment scan + ms
	InitTIMER1();			// Task 1
	InitTIMER2();			// Task 3

	while (1)			// permanent loop, polling
	{
		// Task 2 : keypad changes the delay
		key = ScanKey();
		if (key != 0 && last_key == 0)	// new key press only
		{
			if (key == KEY_SLOWER && delay_ms < DLY_MAX) { delay_ms += DLY_STEP; dirty = 1; }
			if (key == KEY_FASTER && delay_ms > DLY_MIN) { delay_ms -= DLY_STEP; dirty = 1; }
		}
		last_key = key;

		// Task 2 : next segment clockwise when the delay has passed
		if (ms - last_step >= delay_ms)
		{
			last_step = ms;
			step = (step + 1) % 6;
		}

		// Task 4 : push button, count each press (1 -> 0 edge)
		btn = DrvGPIO_GetBit(E_GPB, 15);
		if (btn == 0 && last_btn == 1)
		{
			boxes++;
			if (boxes > BOX_MAX) boxes = 0;
			dirty = 1;
		}
		last_btn = btn;

		if (dirty)
		{
			dirty = 0;
			lcd_display();
		}

		DrvSYS_Delay(10000);		// 10 ms, keypad / button debounce
	}
}
