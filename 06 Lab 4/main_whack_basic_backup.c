//
// Lab 4 : Whack-a-Mole (Timer0 interrupt + keypad + LCD + 7-segment)
//
//   LCD (left)  : 3x3 grid of holes laid out like the keypad
//                   1 2 3
//                   4 5 6
//                   7 8 9
//                 a mole pops up in a random hole, press that key to whack it
//   LCD (right) : SCORE, MISS, TIME left, BEST score
//   7-segment   : left 2 digits = time left, right 2 digits = score
//   RGB LED     : green flash = hit, red flash = miss (wrong key or mole escaped)
//
//   game  : press 5 to start, 30 s per game
//           the mole stays up 1500 ms at the start, 50 ms shorter per hit
//           (never shorter than 450 ms), after a hit or escape the hole is
//           empty for a random 150~500 ms before the next mole
//
//   Timer0    : periodic mode, 1 ms tick, clock from 12MHz crystal
//               time out = (1/12MHz) * (11 + 1) * 1000 = 1 ms
//               the interrupt counts ms_tick (all game timing) and scans one
//               7-segment digit per tick, so the display never flickers
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//               scanned every 20 ms, only new presses count (last_key)
//   LCD       : 128x64 via SPI3, grid drawn from field[] into columns 0~63,
//               text with printS() in columns 64~127
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//   RGB LED   : GPA12 blue, GPA13 green, GPA14 red (0 = on)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again

#define GAME_MS		30000	// length of one game
#define UP_START	1500	// mole up time at the start (ms)
#define UP_STEP		50	// mole up time shorter by this after every hit
#define UP_MIN		450	// shortest mole up time
#define BONK_MS		250	// how long the whacked mole is shown
#define GAP_MIN		150	// empty hole time before the next mole (random)
#define GAP_MAX		500
#define FLASH_MS	150	// RGB LED flash length
#define KEY_MS		20	// keypad scan period

#define FIELD_W		64	// grid area on the LCD is 64 x 64
#define CELL		21	// one hole is 21 x 21 pixels

enum { ST_READY, ST_PLAY, ST_OVER };		// game state
enum { MOLE_HIDDEN, MOLE_UP, MOLE_BONK };	// mole state
enum { LOOK_EMPTY, LOOK_MOLE, LOOK_BONK };	// what a hole shows

// LCD driver functions without a prototype in LCD.h
extern void lcdSetAddr(unsigned char PA, unsigned char CA);
extern void lcdWriteData(unsigned char temp);

unsigned char field[FIELD_W * LCD_Ymax / 8];	// left half of the LCD, 8 pages x 64 columns

// filled / used by the Timer0 interrupt
volatile uint32_t ms_tick = 0;			// ms since power on
volatile uint8_t  seg_digit[4] = {0, 0, 0, 0};	// [3] = leftmost ... [0] = rightmost

uint8_t  state = ST_READY;
uint8_t  mole_state;
int8_t   mole = -1;		// hole with the mole (0~8), -1 = none
uint32_t mole_time;		// time of the next mole event
uint32_t end_time;		// time the game ends
uint32_t flash_end = 0;		// time the RGB LED goes off
uint16_t up_ms;			// how long the mole stays up now
uint16_t score, miss, best = 0;
int16_t  time_left;

// Timer0 interrupt : every 1 ms
void TMR0_IRQHandler(void)
{
	static uint8_t d = 0;

	ms_tick++;

	close_seven_segment();			// show one digit per tick
	show_seven_segment(d, seg_digit[d]);
	d = (d + 1) & 3;

	TIMER0->TISR.TIF = 1;			// clear interrupt flag
}

void InitTIMER0(void)
{
	/* Step 1. Enable and select Timer clock source */
	SYSCLK->CLKSEL1.TMR0_S = 0;	// select 12MHz for Timer0 clock source
	SYSCLK->APBCLK.TMR0_EN = 1;	// enable Timer0 clock source
	/* Step 2. Select operation mode */
	TIMER0->TCSR.MODE = PERIODIC;
	/* Step 3. Time out period = (1/12MHz) * (PRESCALE + 1) * TCMPR */
	TIMER0->TCSR.PRESCALE = 11;	// 12MHz / 12 = 1MHz
	TIMER0->TCMPR = 1000;		// 1000 / 1MHz = 1 ms
	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR0_IRQn);
	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;		// reset up counter
	TIMER0->TCSR.CEN = 1;		// enable Timer0
}

// 1 when time t has been reached (works across the 32-bit wrap)
uint8_t reached(uint32_t t)
{
	return (int32_t)(ms_tick - t) >= 0;
}

// ---------------- RGB LED ----------------

void Init_RGB(void)
{
	uint8_t i;

	for (i = 12; i <= 14; i++)
	{
		DrvGPIO_Open(E_GPA, i, E_IO_OUTPUT);
		DrvGPIO_SetBit(E_GPA, i);	// off
	}
}

// pin 13 = green (hit), 14 = red (miss)
void flash(uint8_t pin)
{
	DrvGPIO_SetBit(E_GPA, 13);
	DrvGPIO_SetBit(E_GPA, 14);
	DrvGPIO_ClrBit(E_GPA, pin);
	flash_end = ms_tick + FLASH_MS;
}

// ---------------- drawing ----------------

void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= FIELD_W || y < 0 || y >= LCD_Ymax) return;
	field[x + (y >> 3) * FIELD_W] |= 1 << (y & 7);
}

void clr_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= FIELD_W || y < 0 || y >= LCD_Ymax) return;
	field[x + (y >> 3) * FIELD_W] &= ~(1 << (y & 7));
}

// horizontal line from x1 to x2
void hline(int16_t x1, int16_t x2, int16_t y)
{
	for (; x1 <= x2; x1++) set_pixel(x1, y);
}

// filled circle
void fill_circle(int16_t xc, int16_t yc, int16_t r)
{
	int16_t x, y;

	for (y = -r; y <= r; y++)
		for (x = -r; x <= r; x++)
			if (x * x + y * y <= r * r + r)
				set_pixel(xc + x, yc + y);
}

// small "+" spark used around a whacked mole
void spark(int16_t x, int16_t y)
{
	set_pixel(x, y);
	set_pixel(x - 1, y);
	set_pixel(x + 1, y);
	set_pixel(x, y - 1);
	set_pixel(x, y + 1);
}

// grid lines between the holes (dotted) and the line next to the text
void draw_grid_lines(void)
{
	int16_t i;

	for (i = 0; i < 63; i += 2)
	{
		set_pixel(CELL, i);
		set_pixel(CELL * 2, i);
		set_pixel(i, CELL);
		set_pixel(i, CELL * 2);
	}
	for (i = 0; i < LCD_Ymax; i++) set_pixel(FIELD_W - 1, i);
}

// draw one hole (0~8) as empty, with a mole, or with a whacked mole
void draw_cell(uint8_t cell, uint8_t look)
{
	const uint8_t hole_w[5] = {4, 7, 8, 7, 4};	// half width of the hole rows
	int16_t x0 = (cell % 3) * CELL;
	int16_t y0 = (cell / 3) * CELL;
	int16_t cx = x0 + 10;
	int16_t x, y, ex;

	for (y = y0 + 1; y < y0 + CELL; y++)		// clear inside of the cell
		for (x = x0 + 1; x < x0 + CELL; x++)
			clr_pixel(x, y);

	if (look != LOOK_EMPTY)
	{
		fill_circle(cx, y0 + 9, 6);		// head
		for (y = y0 + 9; y <= y0 + 15; y++)	// body down into the hole
			hline(cx - 6, cx + 6, y);

		if (look == LOOK_MOLE)
		{
			clr_pixel(cx - 3, y0 + 7);	// eyes
			clr_pixel(cx - 3, y0 + 8);
			clr_pixel(cx + 3, y0 + 7);
			clr_pixel(cx + 3, y0 + 8);
			clr_pixel(cx - 1, y0 + 12);	// teeth
			clr_pixel(cx + 1, y0 + 12);
		}
		else
		{
			for (ex = cx - 3; ex <= cx + 3; ex += 6)	// X X eyes
			{
				clr_pixel(ex - 1, y0 + 6);
				clr_pixel(ex + 1, y0 + 6);
				clr_pixel(ex,     y0 + 7);
				clr_pixel(ex - 1, y0 + 8);
				clr_pixel(ex + 1, y0 + 8);
			}
			for (x = cx - 2; x <= cx + 2; x++)	// open mouth
				clr_pixel(x, y0 + 12);
			spark(x0 + 2,  y0 + 3);			// stars around the head
			spark(x0 + 18, y0 + 3);
			spark(x0 + 2,  y0 + 12);
			spark(x0 + 18, y0 + 12);
		}
	}

	for (y = 0; y < 5; y++)				// the hole
		hline(cx - hole_w[y], cx + hole_w[y], y0 + 15 + y);
}

// send field[] to LCD columns 0~63 (same addressing as draw_LCD)
void show_field(void)
{
	uint8_t x, p;

	for (x = 0; x < FIELD_W; x++)
		for (p = 0; p < LCD_Ymax / 8; p++)
		{
			lcdSetAddr(p, LCD_Xmax + 1 - x);
			lcdWriteData(field[x + p * FIELD_W]);
		}
}

// redraw all 9 holes, the mole hole shows look
void draw_board(uint8_t look)
{
	uint8_t i;

	for (i = 0; i < 9; i++)
		draw_cell(i, (i == mole) ? look : LOOK_EMPTY);
	show_field();
}

// text on the right half, 8 characters per line
void panel_display(void)
{
	char TEXT[9];

	sprintf(TEXT, "SCORE%3d", score);
	printS(FIELD_W, 0, TEXT);
	sprintf(TEXT, "MISS %3d", miss);
	printS(FIELD_W, 16, TEXT);
	sprintf(TEXT, "TIME %3d", time_left);
	printS(FIELD_W, 32, TEXT);
	sprintf(TEXT, "BEST %3d", best);
	printS(FIELD_W, 48, TEXT);
}

// 7-segment : time left on the left 2 digits, score on the right 2 digits
void seg_display(int16_t t, uint16_t s)
{
	seg_digit[3] = (t / 10) % 10;
	seg_digit[2] = t % 10;
	seg_digit[1] = (s / 10) % 10;
	seg_digit[0] = s % 10;
}

// ---------------- game ----------------

void show_ready(void)
{
	char TEXT[17];

	state = ST_READY;
	clear_LCD();
	print_Line(0, "  WHACK-A-MOLE  ");
	print_Line(1, "hit the key of  ");
	print_Line(2, "the mole, 30 sec");
	sprintf(TEXT, "Best%3d  5=PLAY ", best);
	print_Line(3, TEXT);
	seg_display(GAME_MS / 1000, best);
}

void start_game(void)
{
	srand(ms_tick);			// seed from how long the player waited
	score = 0;
	miss = 0;
	up_ms = UP_START;
	mole = -1;
	mole_state = MOLE_HIDDEN;
	end_time = ms_tick + GAME_MS;
	mole_time = ms_tick + 800;	// first mole after 0.8 s
	time_left = GAME_MS / 1000;
	state = ST_PLAY;

	clear_LCD();
	memset(field, 0, sizeof(field));
	draw_grid_lines();
	draw_board(LOOK_EMPTY);
	panel_display();
	seg_display(time_left, score);
}

void game_over(void)
{
	char TEXT[17];
	uint8_t new_best = 0;

	if (score > best)
	{
		best = score;
		new_best = 1;
	}
	state = ST_OVER;
	mole = -1;
	clear_LCD();
	print_Line(0, "   GAME  OVER   ");
	sprintf(TEXT, "Score%3d Miss%3d", score, miss);
	print_Line(1, TEXT);
	if (new_best) print_Line(2, "  NEW BEST !!!  ");
	else
	{
		sprintf(TEXT, "Best  %3d       ", best);
		print_Line(2, TEXT);
	}
	print_Line(3, "Press 5 to play ");
	seg_display(0, score);
}

void miss_one(void)
{
	miss++;
	flash(14);			// red
}

// one step of the running game, key = new key press (0 = none)
void play_step(uint8_t key)
{
	int16_t t;
	uint8_t changed = 0;
	int8_t next;

	if (reached(end_time))
	{
		game_over();
		return;
	}

	t = (end_time - ms_tick + 999) / 1000;		// seconds left, rounded up
	if (t != time_left)
	{
		time_left = t;
		changed = 1;
	}

	switch (mole_state)
	{
		case MOLE_HIDDEN:			// empty holes, wait for the next mole
			if (key) { miss_one(); changed = 1; }
			if (reached(mole_time))
			{
				do next = rand() % 9;	// a different hole every time
				while (next == mole);
				mole = next;
				mole_state = MOLE_UP;
				mole_time = ms_tick + up_ms;
				draw_board(LOOK_MOLE);
			}
			break;

		case MOLE_UP:				// mole is waiting to be whacked
			if (key == mole + 1)
			{
				score++;
				if (up_ms > UP_MIN + UP_STEP) up_ms -= UP_STEP;
				else up_ms = UP_MIN;
				flash(13);		// green
				mole_state = MOLE_BONK;
				mole_time = ms_tick + BONK_MS;
				draw_board(LOOK_BONK);
				changed = 1;
			}
			else if (key)			// wrong hole
			{
				miss_one();
				changed = 1;
			}
			else if (reached(mole_time))	// too slow, the mole escaped
			{
				miss_one();
				mole_state = MOLE_HIDDEN;
				mole_time = ms_tick + GAP_MIN + rand() % (GAP_MAX - GAP_MIN);
				draw_board(LOOK_EMPTY);
				changed = 1;
			}
			break;

		case MOLE_BONK:				// show the whacked mole for a moment
			if (reached(mole_time))
			{
				mole_state = MOLE_HIDDEN;
				mole_time = ms_tick + GAP_MIN + rand() % (GAP_MAX - GAP_MIN);
				draw_board(LOOK_EMPTY);
			}
			break;
	}

	if (changed)
	{
		panel_display();
		seg_display(time_left, score);
	}
}

int32_t main (void)
{
	uint8_t key, new_key, last_key = 0;
	uint32_t key_time = 0;
	uint32_t over_time = 0;

	UNLOCKREG();			// unlock register for programming
	SYSCLK->PWRCON.XTL12M_EN = 1;	// 12MHz crystal for Timer0
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	OpenKeyPad();
	Init_RGB();
	init_LCD();
	InitTIMER0();

	show_ready();

	while (1)
	{
		// keypad every 20 ms, only a new press counts
		new_key = 0;
		if (reached(key_time))
		{
			key_time = ms_tick + KEY_MS;
			key = ScanKey();
			if (key != last_key) new_key = key;
			last_key = key;
		}

		// RGB LED off after the flash
		if (flash_end && reached(flash_end))
		{
			DrvGPIO_SetBit(E_GPA, 13);
			DrvGPIO_SetBit(E_GPA, 14);
			flash_end = 0;
		}

		switch (state)
		{
			case ST_READY:
				if (new_key == 5) start_game();
				break;

			case ST_PLAY:
				play_step(new_key);
				if (state == ST_OVER) over_time = ms_tick + 1000;
				break;

			case ST_OVER:		// ignore keys for 1 s so a late whack doesn't restart
				if (new_key == 5 && reached(over_time)) start_game();
				break;
		}
	}
}
