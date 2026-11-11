//
// Lab 4 : Whack-a-Mole with Timer0 + Timer1 + RTC + Watchdog
//         (keypad + LCD + 7-segment + RGB LED)
//
//   LCD (left)  : 3x3 grid of holes laid out like the keypad
//                   1 2 3
//                   4 5 6
//                   7 8 9
//                 a mole pops up in a random hole, press that key to whack it
//   LCD (right) : SCORE, MISS, TIME left, BEST score
//   7-segment   : title screen = clock HH MM, game = time left | score
//   RGB LED     : green flash = hit, red flash = miss, purple = watchdog warning
//
//   title : line 1 = date and clock from the RTC (updated every second)
//           line 2 = best score and the time it was made
//           5 = play, 1 = hour + 1, 2 = minute + 1 (seconds to 0)
//           7 = freeze the main loop -> watchdog resets the board
//   game  : 30 s, the mole stays up 1500 ms at the start, 50 ms shorter per
//           hit (never shorter than 450 ms), empty for 150~500 ms between moles
//   over  : score, miss, best (NEW BEST with the time), fastest whack in ms
//
//   Timer0    : PERIODIC, 1 ms tick = (1/12MHz) * (11 + 1) * 1000
//               counts ms_tick (all game timing) and scans one 7-segment digit
//   Timer1    : ONESHOT, 150 ms = (1/12MHz) * (11 + 1) * 150000
//               started by every LED flash, its interrupt turns the LED off
//   RTC       : 32.768kHz crystal, 24-hour mode, tick interrupt every 1 s
//               loaded with the build date/time (__DATE__, __TIME__) only when
//               the RTC is not running yet (power on), so a reset keeps the clock
//               the best score + its time are kept in the alarm register TAR
//               (alarm interrupt off), so they are still there after a reset
//   Watchdog  : 10kHz clock, WTIS = 5 -> interrupt after 2^14 clocks = 1.6 s,
//               reset 1024 clocks later; the main loop feeds it every 100 ms
//               after a watchdog reset the LCD says so (SYS->RSTSRC.RSTS_WDT)
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//               scanned every 20 ms, only new presses count (last_key)
//   LCD       : 128x64 via SPI3, grid drawn from field[] into columns 0~63,
//               text with printS() in columns 64~127
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//   RGB LED   : GPA12 blue, GPA13 green, GPA14 red (0 = on), written through the
//               GPIO bit registers so the interrupts can switch them safely
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

#define ONESHOT		0	// counting and interrupt when reach TCMPR number, then stop
#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again

#define GAME_MS		30000	// length of one game
#define UP_START	1500	// mole up time at the start (ms)
#define UP_STEP		50	// mole up time shorter by this after every hit
#define UP_MIN		450	// shortest mole up time
#define BONK_MS		250	// how long the whacked mole is shown
#define GAP_MIN		150	// empty hole time before the next mole (random)
#define GAP_MAX		500
#define FLASH_MS	150	// RGB LED flash length (Timer1 one shot)
#define KEY_MS		20	// keypad scan period
#define FEED_MS		100	// watchdog feeding period

#define FIELD_W		64	// grid area on the LCD is 64 x 64
#define CELL		21	// one hole is 21 x 21 pixels

// GPA pin n through the GPIO bit register (one write, no read-modify-write)
#define GPA_PIN(n)	(*((volatile uint32_t *)(GPIO_BIT_ADDR_BASE + (n) * 4)))
#define LED_BLUE	12
#define LED_GREEN	13
#define LED_RED		14

enum { ST_READY, ST_PLAY, ST_OVER, ST_WDT };	// game state
enum { MOLE_HIDDEN, MOLE_UP, MOLE_BONK };	// mole state
enum { LOOK_EMPTY, LOOK_MOLE, LOOK_BONK };	// what a hole shows

// LCD driver functions without a prototype in LCD.h
extern void lcdSetAddr(unsigned char PA, unsigned char CA);
extern void lcdWriteData(unsigned char temp);

unsigned char field[FIELD_W * LCD_Ymax / 8];	// left half of the LCD, 8 pages x 64 columns

// filled / used by the interrupts
volatile uint32_t ms_tick = 0;			// ms since power on (Timer0)
volatile uint8_t  seg_digit[4] = {0, 0, 0, 0};	// [3] = leftmost ... [0] = rightmost
volatile uint8_t  rtc_tick = 0;			// 1 = a new second (RTC)

uint8_t  state = ST_READY;
uint8_t  mole_state;
int8_t   mole = -1;		// hole with the mole (0~8), -1 = none
uint32_t mole_time;		// time of the next mole event
uint32_t pop_time;		// time the mole came up
uint32_t end_time;		// time the game ends
uint16_t up_ms;			// how long the mole stays up now
uint16_t score, miss;
uint16_t fastest;		// fastest whack in this game (ms)
int16_t  time_left;
uint8_t  best = 0, best_hh = 0, best_mm = 0;	// kept in RTC->TAR

// ---------------- Timer0 : 1 ms tick ----------------

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

// ---------------- Timer1 : one shot LED flash ----------------

void TMR1_IRQHandler(void)
{
	GPA_PIN(LED_GREEN) = 1;			// flash over, LED off
	GPA_PIN(LED_RED) = 1;
	TIMER1->TISR.TIF = 1;			// clear interrupt flag
}

void InitTIMER1(void)
{
	SYSCLK->CLKSEL1.TMR1_S = 0;	// select 12MHz for Timer1 clock source
	SYSCLK->APBCLK.TMR1_EN = 1;	// enable Timer1 clock source
	TIMER1->TCSR.MODE = ONESHOT;	// count once, interrupt, stop
	TIMER1->TCSR.PRESCALE = 11;	// 12MHz / 12 = 1MHz
	TIMER1->TCMPR = FLASH_MS * 1000;	// 150000 / 1MHz = 150 ms
	TIMER1->TCSR.IE = 1;
	TIMER1->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR1_IRQn);
	// not started here, flash() starts it
}

void Init_RGB(void)
{
	uint8_t i;

	for (i = LED_BLUE; i <= LED_RED; i++)
	{
		DrvGPIO_Open(E_GPA, i, E_IO_OUTPUT);
		GPA_PIN(i) = 1;			// off
	}
}

// turn on one LED (green or red), Timer1 turns it off after 150 ms
void flash(uint8_t pin)
{
	GPA_PIN(LED_GREEN) = 1;
	GPA_PIN(LED_RED) = 1;
	GPA_PIN(pin) = 0;
	TIMER1->TCSR.CRST = 1;		// count again from 0
	TIMER1->TCSR.CEN = 1;		// start the one shot
}

// ---------------- RTC ----------------

void RTC_IRQHandler(void)
{
	if (RTC->RIIR.TIF) rtc_tick = 1;
	RTC->RIIR.TIF = 1;		// write 1 to clear
	RTC->RIIR.AIF = 1;
}

uint8_t bcd(uint8_t v)		{ return ((v / 10) << 4) | (v % 10); }
uint8_t unbcd(uint32_t b)	{ return ((b >> 4) & 0x0F) * 10 + (b & 0x0F); }

// open the RTC register write window (about 15 ms)
void rtc_access(void)
{
	do
		RTC->AER.AER = 0xA965;
	while (RTC->AER.ENF == 0);
}

void rtc_get_time(uint8_t *hh, uint8_t *mm, uint8_t *ss)
{
	uint32_t t = RTC->u32TLR;

	*ss = unbcd(t & 0x7F);
	*mm = unbcd((t >> 8) & 0x7F);
	*hh = unbcd((t >> 16) & 0x3F);
}

void rtc_set_time(uint8_t hh, uint8_t mm, uint8_t ss)
{
	rtc_access();
	RTC->u32TLR = (bcd(hh) << 16) | (bcd(mm) << 8) | bcd(ss);
}

// best score in the seconds bits, time of the best score as HH:MM
void save_best(void)
{
	rtc_access();
	RTC->u32TAR = (bcd(best_hh) << 16) | (bcd(best_mm) << 8) | (best & 0x7F);
}

void load_best(void)
{
	uint32_t t;

	rtc_access();
	t = RTC->u32TAR;
	best    = t & 0x7F;
	best_mm = unbcd((t >> 8) & 0x7F);
	best_hh = unbcd((t >> 16) & 0x3F);
	if (best_hh > 23 || best_mm > 59)	// not a valid time -> no best yet
		best = best_hh = best_mm = 0;
}

// date / time when this program was built, e.g. "Oct 12 2026" "14:05:32"
void rtc_set_build_time(void)
{
	const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	const char *d = __DATE__;
	const char *t = __TIME__;
	char mon_name[4];
	uint8_t mon, day, year;

	memcpy(mon_name, d, 3);
	mon_name[3] = '\0';
	mon  = (strstr(months, mon_name) - months) / 3 + 1;
	day  = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
	year = (d[9] - '0') * 10 + (d[10] - '0');

	rtc_access();
	RTC->u32CLR = (bcd(year) << 16) | (bcd(mon) << 8) | bcd(day);
	rtc_set_time((t[0] - '0') * 10 + (t[1] - '0'),
	             (t[3] - '0') * 10 + (t[4] - '0'),
	             (t[6] - '0') * 10 + (t[7] - '0'));
}

void InitRTC(void)
{
	if (RTC->INIR != 1)		// RTC not running yet (power on)
	{
		/* start Real-Time Clock */
		do
			RTC->INIR = 0xa5eb1357;
		while (RTC->INIR != 1);

		rtc_access();
		RTC->TSSR.HR24_HR12 = 1;	// 24-hour mode
		rtc_set_build_time();
		best = best_hh = best_mm = 0;
		save_best();
	}
	else
		load_best();		// clock and best score survived the reset

	/* time tick interrupt every 1 s, no alarm interrupt */
	rtc_access();
	RTC->TTR.TTR = 0;		// 1 tick per second
	RTC->RIIR.TIF = 1;
	RTC->RIIR.AIF = 1;
	RTC->RIER.AIER = 0;
	RTC->RIER.TIER = 1;
	NVIC_EnableIRQ(RTC_IRQn);
}

// ---------------- Watchdog ----------------

// watchdog interrupt : main loop has not fed it for 1.6 s, reset comes next
void WDT_IRQHandler(void)
{
	GPA_PIN(LED_BLUE)  = 0;		// purple = warning
	GPA_PIN(LED_GREEN) = 1;
	GPA_PIN(LED_RED)   = 0;
	UNLOCKREG();
	WDT->WTCR.WTIF = 1;		// clear flag, do NOT feed (WTR) so the reset happens
	LOCKREG();
}

void InitWDT(void)
{
	UNLOCKREG();
	/* Step 1. Enable and select WDT clock source */
	SYSCLK->CLKSEL1.WDT_S = 3;	// 10kHz for WDT clock source
	SYSCLK->APBCLK.WDT_EN = 1;	// enable WDT clock source
	/* Step 2. Select timeout interval */
	WDT->WTCR.WTIS = 5;		// 2^14 / 10kHz = 1.6 s
	/* Step 3. Enable Watchdog Timer reset function */
	WDT->WTCR.WTRE = 1;		// reset 1024 clocks after the interrupt
	/* Step 4. Enable WDT interrupt */
	WDT->WTCR.WTIF = 1;		// write 1 to clear for safety
	WDT->WTCR.WTIE = 1;
	NVIC_EnableIRQ(WDT_IRQn);
	/* Step 5. Enable WDT module */
	WDT->WTCR.WTE = 1;
	WDT->WTCR.WTR = 1;		// clear WDT counter
	LOCKREG();
}

void feed_wdt(void)
{
	UNLOCKREG();
	WDT->WTCR.WTR = 1;		// clear WDT counter
	LOCKREG();
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

// 7-segment : two 2-digit numbers, a on the left, b on the right
void seg_display(int16_t a, uint16_t b)
{
	seg_digit[3] = (a / 10) % 10;
	seg_digit[2] = a % 10;
	seg_digit[1] = (b / 10) % 10;
	seg_digit[0] = b % 10;
}

// "Best 12 @14:05" line, 16 characters
void best_line(char *TEXT)
{
	if (best) sprintf(TEXT, "Best%3d @%02d:%02d  ", best, best_hh, best_mm);
	else      sprintf(TEXT, "Best  0         ");
}

// ---------------- screens ----------------

// title screen line 1 (date + clock) and 7-segment HH MM
void title_clock(void)
{
	char TEXT[17];
	uint8_t hh, mm, ss;
	uint32_t c = RTC->u32CLR;

	rtc_get_time(&hh, &mm, &ss);
	sprintf(TEXT, " %02d/%02d  %02d:%02d:%02d", unbcd(c & 0x3F), unbcd((c >> 8) & 0x1F), hh, mm, ss);
	print_Line(1, TEXT);
	seg_display(hh, mm);
}

void show_ready(void)
{
	char TEXT[17];

	state = ST_READY;
	clear_LCD();
	print_Line(0, "  WHACK-A-MOLE  ");
	title_clock();
	best_line(TEXT);
	print_Line(2, TEXT);
	print_Line(3, "5=Go 7=WDT 1H 2M");
}

void show_wdt_reset(void)
{
	state = ST_WDT;
	clear_LCD();
	print_Line(0, "!! WATCHDOG !!  ");
	print_Line(1, "main loop froze ");
	print_Line(2, "board was reset ");
	print_Line(3, "press any key   ");
	seg_display(0, 0);
}

// title key 7 : stop feeding the watchdog and hang here
void freeze_demo(void)
{
	print_Line(1, "main loop frozen");
	print_Line(2, "no WDT feeding..");
	print_Line(3, "reset in 1.7 s  ");
	while (1);			// Timer0 still runs, the watchdog does the rest
}

// ---------------- game ----------------

void start_game(void)
{
	srand(ms_tick);			// seed from how long the player waited
	score = 0;
	miss = 0;
	fastest = 0xFFFF;
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
	uint8_t ss;

	state = ST_OVER;
	mole = -1;
	clear_LCD();
	print_Line(0, "   GAME  OVER   ");
	sprintf(TEXT, "Score%3d Miss%3d", score, miss);
	print_Line(1, TEXT);
	if (score > best)
	{
		best = (score > 127) ? 127 : score;
		rtc_get_time(&best_hh, &best_mm, &ss);
		save_best();
		sprintf(TEXT, "NEW BEST! @%02d:%02d", best_hh, best_mm);
	}
	else
		best_line(TEXT);
	print_Line(2, TEXT);
	if (fastest == 0xFFFF) sprintf(TEXT, "Fast  ---  5=Go ");
	else                   sprintf(TEXT, "Fast%4dms  5=Go", fastest);
	print_Line(3, TEXT);
	seg_display(0, score);
}

void miss_one(void)
{
	miss++;
	flash(LED_RED);
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
				pop_time = ms_tick;
				mole_time = ms_tick + up_ms;
				draw_board(LOOK_MOLE);
			}
			break;

		case MOLE_UP:				// mole is waiting to be whacked
			if (key == mole + 1)
			{
				score++;
				if (ms_tick - pop_time < fastest) fastest = ms_tick - pop_time;
				if (up_ms > UP_MIN + UP_STEP) up_ms -= UP_STEP;
				else up_ms = UP_MIN;
				flash(LED_GREEN);
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

// ScanKey() changes GPA DOUT with read-modify-write, so keep the
// interrupts that switch the RGB LED (also on GPA) away while it runs
uint8_t read_key(void)
{
	uint8_t key;

	NVIC_DisableIRQ(TMR1_IRQn);
	NVIC_DisableIRQ(WDT_IRQn);
	key = ScanKey();
	NVIC_EnableIRQ(TMR1_IRQn);
	NVIC_EnableIRQ(WDT_IRQn);
	return key;
}

int32_t main (void)
{
	uint8_t key, new_key, last_key = 0;
	uint8_t hh, mm, ss;
	uint8_t wdt_reset;
	uint32_t key_time = 0, feed_time = 0, over_time = 0;

	UNLOCKREG();			// unlock register for programming
	SYSCLK->PWRCON.XTL12M_EN = 1;	// 12MHz crystal for Timer0/1
	SYSCLK->PWRCON.XTL32K_EN = 1;	// 32.768kHz crystal for RTC
	SYSCLK->APBCLK.RTC_EN = 1;	// enable RTC clock
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	wdt_reset = SYS->RSTSRC.RSTS_WDT;	// was the last reset by the watchdog?
	SYS->u32RSTSRC = SYS->u32RSTSRC;	// write 1 to clear the reset flags
	LOCKREG();			// lock register from programming

	OpenKeyPad();
	Init_RGB();
	init_LCD();
	InitTIMER0();
	InitTIMER1();
	InitRTC();
	InitWDT();

	if (wdt_reset) show_wdt_reset();
	else           show_ready();

	while (1)
	{
		// feed the watchdog, if this stops for 1.6 s the board resets
		if (reached(feed_time))
		{
			feed_time = ms_tick + FEED_MS;
			feed_wdt();
		}

		// keypad every 20 ms, only a new press counts
		new_key = 0;
		if (reached(key_time))
		{
			key_time = ms_tick + KEY_MS;
			key = read_key();
			if (key != last_key) new_key = key;
			last_key = key;
		}

		switch (state)
		{
			case ST_READY:
				if (new_key == 5) start_game();
				else if (new_key == 7) freeze_demo();
				else if (new_key == 1 || new_key == 2)	// set the clock
				{
					rtc_get_time(&hh, &mm, &ss);
					if (new_key == 1) rtc_set_time((hh + 1) % 24, mm, ss);
					else              rtc_set_time(hh, (mm + 1) % 60, 0);
					rtc_tick = 1;		// show it now
				}
				if (rtc_tick && state == ST_READY)
				{
					rtc_tick = 0;
					title_clock();
				}
				break;

			case ST_PLAY:
				play_step(new_key);
				if (state == ST_OVER) over_time = ms_tick + 1000;
				break;

			case ST_OVER:		// ignore keys for 1 s so a late whack doesn't restart
				if (new_key == 5 && reached(over_time)) start_game();
				break;

			case ST_WDT:
				if (new_key) show_ready();
				break;
		}
	}
}
