//
// Lab 4 : TETRIS (Timer0 interrupt + keypad + LCD + 7-segment + RGB LED)
//
//   keypad      1 2 3        5     = rotate clockwise
//               4 5 6        1     = rotate anti-clockwise
//               7 8 9        4 / 6 = move left / right (hold to slide)
//                            8     = soft drop (hold)
//                            2     = hard drop (screen shakes)
//                            7     = pause / resume
//
//   LCD : left   SCORE, LEVEL, LINES
//         middle the well, 10 x 20 cells of 3 x 3 pixels
//                falling piece = solid, landed blocks = hollow,
//                ghost piece (4 dots) shows where the piece will land
//         right  NEXT piece, HI score, messages (DOUBLE, TRIPLE, TETRIS!, LEVEL UP)
//   7-segment : lines cleared (title screen = hi score)
//   RGB LED   : green = lines cleared, blue = TETRIS / level up, red = game over
//
//   animations : title letters drop in and bounce, cleared lines are wiped
//                from the middle outwards, 4 lines flash the whole screen,
//                hard drop shakes the well, game over fills the well bottom-up
//
//   rules : 7-bag random pieces, wall kicks, 450 ms lock delay (moving or
//           rotating on the floor restarts it, at most 15 times)
//           score 40 / 100 / 300 / 1200 x (level + 1) for 1~4 lines,
//           +1 per soft drop cell, +2 per hard drop cell
//           level + 1 every 10 lines, fall time 800 ms at level 0 down to 50 ms
//
//   Timer0    : PERIODIC, 1 ms tick = (1/12MHz) * (11 + 1) * 1000
//               counts ms_tick (gravity, lock delay, key repeat, animations)
//               and scans one 7-segment digit per tick
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//               scanned every 15 ms, 4 / 6 / 8 repeat while held
//   LCD       : 128x64 via SPI3, whole frame built in screen[] (5x7 font for
//               text) and sent one page at a time
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
#include "Font5x7.h"

#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again

#define COLS		10	// well size in cells
#define ROWS		20
#define CELL		3	// one cell = 3 x 3 pixels
#define WELL_X		49	// top-left pixel of the well inside
#define WELL_Y		2
#define FULL_ROW	0x3FF	// all 10 bits set

#define LOCK_MS		450	// time on the floor before the piece locks
#define LOCK_RESETS	15	// moves on the floor that restart the lock time
#define DAS_MS		170	// hold 4 / 6 this long before it repeats
#define ARR_MS		55	// then move every 55 ms
#define SOFT_MS		35	// soft drop repeat
#define KEY_MS		15	// keypad scan period
#define DRAW_MS		15	// fastest screen update
#define WIPE_MS		60	// line clear animation step
#define CURTAIN_MS	35	// game over fill, per row
#define TITLE_MS	25	// title animation frame
#define FLASH_MS	250	// RGB LED flash
#define POPUP_MS	1500	// message on the right

#define FAST_LCD	1	// 1 = send each page in one go, 0 = use draw_LCD()

// GPA pin n through the GPIO bit register
#define GPA_PIN(n)	(*((volatile uint32_t *)(GPIO_BIT_ADDR_BASE + (n) * 4)))
#define LED_BLUE	12
#define LED_GREEN	13
#define LED_RED		14

enum { ST_TITLE, ST_PLAY, ST_CLEAR, ST_PAUSE, ST_CURTAIN, ST_OVER };
enum { P_I, P_J, P_L, P_O, P_S, P_T, P_Z };
enum { B_SOLID, B_LOCKED, B_GHOST };

// LCD driver functions without a prototype in LCD.h
extern void lcdSetAddr(unsigned char PA, unsigned char CA);
extern void lcdWriteData(unsigned char temp);

// 7 pieces x 4 rotations, 4x4 box, bit 15 = top-left, row by row
const uint16_t SHAPES[7][4] = {
	{0x0F00, 0x2222, 0x00F0, 0x4444},	// I
	{0x8E00, 0x6440, 0x0E20, 0x44C0},	// J
	{0x2E00, 0x4460, 0x0E80, 0xC440},	// L
	{0x6600, 0x6600, 0x6600, 0x6600},	// O
	{0x6C00, 0x4620, 0x06C0, 0x8C40},	// S
	{0x4E00, 0x4640, 0x0E40, 0x4C40},	// T
	{0xC600, 0x2640, 0x0C60, 0x4C80}	// Z
};

// "TETRIS" in 3x5 blocks for the title, bit 2 = left column
const uint8_t LETTERS[6][5] = {
	{7, 2, 2, 2, 2},	// T
	{7, 4, 6, 4, 7},	// E
	{7, 2, 2, 2, 2},	// T
	{6, 5, 6, 5, 5},	// R
	{7, 2, 2, 2, 7},	// I
	{3, 4, 2, 1, 6}		// S
};

const char *HINTS[5] = {"4 6 : MOVE", "5 1 : ROTATE", "8 : SOFT DROP", "2 : HARD DROP", "7 : PAUSE"};
const uint16_t LINE_POINTS[5] = {0, 40, 100, 300, 1200};

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

// filled / used by the Timer0 interrupt
volatile uint32_t ms_tick = 0;			// ms since power on
volatile uint8_t  seg_digit[4] = {0, 0xFF, 0xFF, 0xFF};	// [0] = rightmost, 0xFF = blank

uint16_t board[ROWS];		// landed blocks, bit c = column c
int8_t   cur = -1, rot, px, py;	// falling piece, rotation, position (cell of box top-left)
int8_t   next_p;
uint8_t  bag[7], bag_left = 0;

uint32_t score, hi = 0;
uint16_t lines;
uint8_t  level;
uint16_t fall_ms;
uint32_t fall_time;
uint8_t  lock_on, lock_resets;
uint32_t lock_time;

uint8_t  state = ST_TITLE;
uint8_t  dirty = 1;		// 1 = screen needs to be drawn again
uint8_t  clear_rows[4], clear_n;
uint8_t  anim_step;
uint32_t anim_time;
uint32_t shake_end = 0, invert_end = 0, flash_end = 0, popup_end = 0;
const char *popup = "";
uint8_t  new_hi;

int16_t  title_y[6], title_v[6];	// title letters position and speed
uint32_t title_start, title_bob;

// ---------------- Timer0 : 1 ms tick ----------------

void TMR0_IRQHandler(void)
{
	static uint8_t d = 0;

	ms_tick++;

	close_seven_segment();			// show one digit per tick
	if (seg_digit[d] != 0xFF)
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

// number on the 7-segment, leading zeros blank
void seg_number(uint32_t v)
{
	uint8_t i;

	if (v > 9999) v = 9999;
	for (i = 0; i < 4; i++)
	{
		seg_digit[i] = (i == 0 || v) ? v % 10 : 0xFF;
		v /= 10;
	}
}

// ---------------- RGB LED ----------------

void Init_RGB(void)
{
	uint8_t i;

	for (i = LED_BLUE; i <= LED_RED; i++)
	{
		DrvGPIO_Open(E_GPA, i, E_IO_OUTPUT);
		GPA_PIN(i) = 1;			// off
	}
}

void flash(uint8_t pin, uint16_t ms)
{
	GPA_PIN(LED_BLUE)  = 1;
	GPA_PIN(LED_GREEN) = 1;
	GPA_PIN(LED_RED)   = 1;
	GPA_PIN(pin) = 0;
	flash_end = ms_tick + ms;
}

// ---------------- drawing into screen[] ----------------

void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

void clr_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] &= ~(1 << (y & 7));
}

void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			set_pixel(x + i, y + j);
}

void clear_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			clr_pixel(x + i, y + j);
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

// text centred across the whole LCD
void draw_text_c(int16_t y, const char *s)
{
	draw_text((LCD_Xmax - (int16_t)strlen(s) * 6 + 1) / 2, y, s);
}

// one 3x3 cell : solid (falling), hollow (landed) or 4 corner dots (ghost)
void draw_block(int16_t x, int16_t y, uint8_t style)
{
	if (style == B_GHOST)
	{
		set_pixel(x, y);
		set_pixel(x + 2, y);
		set_pixel(x, y + 2);
		set_pixel(x + 2, y + 2);
		return;
	}
	fill_rect(x, y, 3, 3);
	if (style == B_LOCKED) clr_pixel(x + 1, y + 1);
}

// pixel position of a well cell, shake moves the well 1 pixel down
int16_t cell_x(int8_t c) { return WELL_X + c * CELL; }
int16_t cell_y(int8_t r) { return WELL_Y + r * CELL + (shake_end ? 1 : 0); }

void draw_piece(int8_t p, int8_t r, int8_t x, int8_t y, uint8_t style)
{
	uint8_t i;

	for (i = 0; i < 16; i++)
		if (SHAPES[p][r] & (0x8000 >> i))
			if (y + (i >> 2) >= 0)
				draw_block(cell_x(x + (i & 3)), cell_y(y + (i >> 2)), style);
}

// next piece centred in the NEXT box, 4 pixel cells
void draw_next(void)
{
	uint8_t i, r, c;
	int8_t min_r = 3, max_r = 0, min_c = 3, max_c = 0;
	int16_t ox, oy;

	rect(84, 10, 20, 20);
	for (i = 0; i < 16; i++)
		if (SHAPES[next_p][0] & (0x8000 >> i))
		{
			r = i >> 2; c = i & 3;
			if (r < min_r) min_r = r;
			if (r > max_r) max_r = r;
			if (c < min_c) min_c = c;
			if (c > max_c) max_c = c;
		}
	ox = 86 + (16 - (max_c - min_c + 1) * 4) / 2 - min_c * 4;
	oy = 12 + (16 - (max_r - min_r + 1) * 4) / 2 - min_r * 4;
	for (i = 0; i < 16; i++)
		if (SHAPES[next_p][0] & (0x8000 >> i))
			fill_rect(ox + (i & 3) * 4, oy + (i >> 2) * 4, 3, 3);
}

// send screen[] to the LCD
void lcd_update(void)
{
#if FAST_LCD
	uint8_t p;
	int16_t x;

	// column address goes up after every byte, LCD column 2 = x 127 (see draw_LCD)
	for (p = 0; p < LCD_Ymax / 8; p++)
	{
		lcdSetAddr(p, LCD_Xmax + 1 - (LCD_Xmax - 1));
		for (x = LCD_Xmax - 1; x >= 0; x--)
			lcdWriteData(screen[x + p * LCD_Xmax]);
	}
#else
	draw_LCD(screen);
#endif
}

// ---------------- game logic ----------------

// 1 when piece p, rotation r fits at cell (x, y)
uint8_t fits(int8_t p, int8_t r, int8_t x, int8_t y)
{
	uint8_t i;
	int8_t cx, cy;

	for (i = 0; i < 16; i++)
		if (SHAPES[p][r] & (0x8000 >> i))
		{
			cx = x + (i & 3);
			cy = y + (i >> 2);
			if (cx < 0 || cx >= COLS || cy >= ROWS) return 0;
			if (cy >= 0 && (board[cy] & (1 << cx))) return 0;
		}
	return 1;
}

// 7-bag : every piece once in random order, then a new bag
int8_t bag_next(void)
{
	uint8_t i, j, t;

	if (bag_left == 0)
	{
		for (i = 0; i < 7; i++) bag[i] = i;
		for (i = 6; i > 0; i--)		// shuffle
		{
			j = rand() % (i + 1);
			t = bag[i]; bag[i] = bag[j]; bag[j] = t;
		}
		bag_left = 7;
	}
	return bag[--bag_left];
}

uint16_t fall_speed(uint8_t lv)
{
	if (lv < 10) return 800 - lv * 70;	// 800, 730 ... 170 ms
	if (lv < 15) return 100 - (lv - 10) * 10;	// 100 ... 60 ms
	return 50;
}

void show_popup(const char *s)
{
	popup = s;
	popup_end = ms_tick + POPUP_MS;
}

void start_curtain(void)
{
	cur = -1;
	state = ST_CURTAIN;
	anim_step = 0;
	anim_time = ms_tick + CURTAIN_MS;
	new_hi = (score > hi);
	if (new_hi) hi = score;
	flash(LED_RED, 1000);
	dirty = 1;
}

void spawn(void)
{
	cur = next_p;
	next_p = bag_next();
	rot = 0;
	px = 3;
	py = (cur == P_I) ? -1 : 0;
	lock_on = 0;
	lock_resets = 0;
	fall_time = ms_tick + fall_ms;
	dirty = 1;
	if (!fits(cur, rot, px, py)) start_curtain();	// no room : game over
}

void start_game(void)
{
	srand(ms_tick);			// seed from how long the title was shown
	memset(board, 0, sizeof(board));
	score = 0;
	lines = 0;
	level = 0;
	fall_ms = fall_speed(0);
	bag_left = 0;
	popup_end = 0;
	next_p = bag_next();
	state = ST_PLAY;
	seg_number(0);
	spawn();
}

uint8_t grounded(void)
{
	return !fits(cur, rot, px, py + 1);
}

// piece moved or turned : on the floor this restarts the lock time
void touched(void)
{
	dirty = 1;
	if (lock_on && lock_resets < LOCK_RESETS)
	{
		lock_time = ms_tick;
		lock_resets++;
	}
}

void try_move(int8_t dx)
{
	if (fits(cur, rot, px + dx, py))
	{
		px += dx;
		touched();
	}
}

// rotate with wall kicks : try in place, then 1 and 2 cells left / right, then 1 up
void try_rotate(int8_t dir)
{
	const int8_t kick[5] = {0, -1, 1, -2, 2};
	int8_t nr = (rot + dir) & 3;
	uint8_t k;

	for (k = 0; k < 5; k++)
		if (fits(cur, nr, px + kick[k], py))
		{
			px += kick[k];
			rot = nr;
			touched();
			return;
		}
	if (fits(cur, nr, px, py - 1))
	{
		py--;
		rot = nr;
		touched();
	}
}

// put the piece into board[], then clear lines or bring the next piece
void lock_piece(void)
{
	uint8_t i, out = 0;
	int8_t cy, r;

	for (i = 0; i < 16; i++)
		if (SHAPES[cur][rot] & (0x8000 >> i))
		{
			cy = py + (i >> 2);
			if (cy < 0) out = 1;
			else board[cy] |= 1 << (px + (i & 3));
		}
	cur = -1;
	dirty = 1;
	if (out)				// landed above the top : game over
	{
		start_curtain();
		return;
	}

	clear_n = 0;
	for (r = 0; r < ROWS; r++)
		if (board[r] == FULL_ROW) clear_rows[clear_n++] = r;

	if (clear_n)
	{
		state = ST_CLEAR;
		anim_step = 0;
		anim_time = ms_tick + WIPE_MS;
		if (clear_n == 4)
		{
			flash(LED_BLUE, FLASH_MS * 2);
			invert_end = ms_tick + 200;	// flash the whole screen
			show_popup("TETRIS!");
		}
		else
		{
			flash(LED_GREEN, FLASH_MS);
			if (clear_n == 2) show_popup("DOUBLE");
			if (clear_n == 3) show_popup("TRIPLE");
		}
	}
	else
		spawn();
}

// after the wipe : remove the lines, score, level
void finish_clear(void)
{
	uint8_t i;
	int8_t r;

	for (i = 0; i < clear_n; i++)		// rows are in top-to-bottom order
	{
		for (r = clear_rows[i]; r > 0; r--)
			board[r] = board[r - 1];
		board[0] = 0;
	}
	score += (uint32_t)LINE_POINTS[clear_n] * (level + 1);
	lines += clear_n;
	if (lines / 10 > level)
	{
		level = lines / 10;
		fall_ms = fall_speed(level);
		show_popup("LEVEL UP");
		flash(LED_BLUE, FLASH_MS * 2);
	}
	seg_number(lines);
	state = ST_PLAY;
	spawn();
}

void do_key(uint8_t key)
{
	switch (key)
	{
		case 4: try_move(-1); break;
		case 6: try_move(1);  break;
		case 5: try_rotate(1);  break;
		case 1: try_rotate(-1); break;
		case 8:					// soft drop
			if (fits(cur, rot, px, py + 1))
			{
				py++;
				score++;
				fall_time = ms_tick + fall_ms;
				dirty = 1;
			}
			break;
		case 2:					// hard drop
			while (fits(cur, rot, px, py + 1))
			{
				py++;
				score += 2;
			}
			shake_end = ms_tick + 90;
			lock_piece();
			break;
		case 7:
			state = ST_PAUSE;
			dirty = 1;
			break;
	}
}

// gravity and lock delay
void play_update(void)
{
	if (reached(fall_time))
	{
		fall_time = ms_tick + fall_ms;
		if (fits(cur, rot, px, py + 1))
		{
			py++;
			dirty = 1;
		}
	}
	if (grounded())
	{
		if (!lock_on)
		{
			lock_on = 1;
			lock_time = ms_tick;
		}
		else if (ms_tick - lock_time >= LOCK_MS)
			lock_piece();
	}
	else
		lock_on = 0;
}

// ---------------- screens ----------------

void render_game(void)
{
	char TEXT[12];
	int8_t r, c, gy;
	int16_t wy = shake_end ? 1 : 0;
	uint16_t i;

	memset(screen, 0, sizeof(screen));

	// left panel
	draw_text(2, 2, "SCORE");
	sprintf(TEXT, "%lu", (unsigned long)score);
	draw_text(2, 11, TEXT);
	draw_text(2, 23, "LEVEL");
	sprintf(TEXT, "%d", level);
	draw_text(2, 32, TEXT);
	draw_text(2, 44, "LINES");
	sprintf(TEXT, "%d", lines);
	draw_text(2, 53, TEXT);

	// right panel
	draw_text(85, 2, "NEXT");
	draw_next();
	draw_text(85, 34, "HI");
	sprintf(TEXT, "%lu", (unsigned long)hi);
	draw_text(85, 43, TEXT);
	if (popup_end) draw_text(81, 55, popup);
	else           draw_text(81, 55, "7=PAUSE");

	// well walls and floor
	for (i = 0; i <= WELL_Y + ROWS * CELL + wy; i++)
	{
		set_pixel(WELL_X - 1, i);
		set_pixel(WELL_X + COLS * CELL, i);
	}
	for (i = WELL_X - 1; i <= WELL_X + COLS * CELL; i++)
		set_pixel(i, WELL_Y + ROWS * CELL + wy);

	if (state == ST_PAUSE)
	{
		draw_text(WELL_X, 28, "PAUSE");
		lcd_update();
		return;
	}

	// landed blocks, cleared lines are wiped from the middle outwards
	for (r = 0; r < ROWS; r++)
		for (c = 0; c < COLS; c++)
			if (board[r] & (1 << c))
			{
				if (state == ST_CLEAR && board[r] == FULL_ROW
				    && c >= 4 - anim_step && c <= 5 + anim_step)
					continue;
				draw_block(cell_x(c), cell_y(r), B_LOCKED);
			}

	// game over : rows fill up from the bottom
	if (state == ST_CURTAIN || state == ST_OVER)
		for (r = ROWS - 1; r >= 0 && r >= ROWS - anim_step; r--)
			for (c = 0; c < COLS; c++)
				draw_block(cell_x(c), cell_y(r), B_SOLID);

	// ghost and falling piece
	if (cur >= 0 && state == ST_PLAY)
	{
		gy = py;
		while (fits(cur, rot, px, gy + 1)) gy++;
		if (gy != py) draw_piece(cur, rot, px, gy, B_GHOST);
		draw_piece(cur, rot, px, py, B_SOLID);
	}

	// game over box
	if (state == ST_OVER)
	{
		clear_rect(18, 14, 92, 37);
		rect(18, 14, 92, 37);
		rect(20, 16, 88, 33);
		draw_text_c(19, "GAME OVER");
		sprintf(TEXT, "SCORE %lu", (unsigned long)score);
		draw_text_c(29, TEXT);
		if (new_hi && (ms_tick / 500) % 2) draw_text_c(39, "NEW HI SCORE!");
		else                               draw_text_c(39, "5 = RETRY");
	}

	if (invert_end)				// TETRIS : whole screen flashes
		for (i = 0; i < sizeof(screen); i++) screen[i] = ~screen[i];

	lcd_update();
}

void title_reset(void)
{
	uint8_t i;

	state = ST_TITLE;
	for (i = 0; i < 6; i++)
	{
		title_y[i] = -22;
		title_v[i] = 0;
	}
	title_start = ms_tick;
	title_bob = ms_tick + 3000;
	seg_number(hi);
	GPA_PIN(LED_RED) = 1;
}

// letters fall one after another with gravity and bounce, then hop now and then
void title_update(void)
{
	uint8_t i;

	for (i = 0; i < 6; i++)
	{
		if (ms_tick - title_start < i * 180) continue;	// not dropped yet
		title_y[i] += title_v[i];
		title_v[i]++;
		if (title_y[i] >= 8)
		{
			title_y[i] = 8;
			title_v[i] = (title_v[i] > 3) ? -title_v[i] / 2 : 0;
		}
	}
	if (reached(title_bob))			// a random letter jumps
	{
		title_bob = ms_tick + 1500;
		title_v[rand() % 6] = -4;
	}
}

void render_title(void)
{
	char TEXT[17];
	uint8_t i, row, col;

	memset(screen, 0, sizeof(screen));
	for (i = 0; i < 6; i++)
		for (row = 0; row < 5; row++)
			for (col = 0; col < 3; col++)
				if (LETTERS[i][row] & (4 >> col))
					fill_rect(18 + i * 16 + col * 4, title_y[i] + row * 4, 3, 3);

	if ((ms_tick / 400) % 2) draw_text_c(33, "PRESS 5 TO PLAY");
	draw_text_c(44, HINTS[(ms_tick / 1500) % 5]);
	sprintf(TEXT, "HI %lu", (unsigned long)hi);
	draw_text_c(55, TEXT);
	lcd_update();
}

int32_t main (void)
{
	uint8_t key, new_key, last_key = 0;
	uint32_t key_time = 0, repeat_time = 0, draw_time = 0, frame_time = 0, over_time = 0;

	UNLOCKREG();			// unlock register for programming
	SYSCLK->PWRCON.XTL12M_EN = 1;	// 12MHz crystal for Timer0
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	OpenKeyPad();
	Init_RGB();
	init_LCD();
	clear_LCD();
	InitTIMER0();

	title_reset();

	while (1)
	{
		// keypad : new press, or repeat for 4 / 6 / 8 while held
		new_key = 0;
		if (reached(key_time))
		{
			key_time = ms_tick + KEY_MS;
			key = ScanKey();
			if (key != last_key)
			{
				new_key = key;
				repeat_time = ms_tick + DAS_MS;
			}
			else if ((key == 4 || key == 6 || key == 8) && reached(repeat_time))
			{
				new_key = key;
				repeat_time = ms_tick + (key == 8 ? SOFT_MS : ARR_MS);
			}
			last_key = key;
		}

		// timed effects ending
		if (flash_end && reached(flash_end))
		{
			GPA_PIN(LED_BLUE) = 1;
			GPA_PIN(LED_GREEN) = 1;
			GPA_PIN(LED_RED) = 1;
			flash_end = 0;
		}
		if (shake_end  && reached(shake_end))  { shake_end = 0;  dirty = 1; }
		if (invert_end && reached(invert_end)) { invert_end = 0; dirty = 1; }
		if (popup_end  && reached(popup_end))  { popup_end = 0;  dirty = 1; }

		switch (state)
		{
			case ST_TITLE:
				if (new_key == 5) start_game();
				else if (reached(frame_time))
				{
					frame_time = ms_tick + TITLE_MS;
					title_update();
					render_title();
				}
				break;

			case ST_PLAY:
				if (new_key) do_key(new_key);
				if (state == ST_PLAY) play_update();
				break;

			case ST_CLEAR:
				if (reached(anim_time))
				{
					anim_time = ms_tick + WIPE_MS;
					anim_step++;
					dirty = 1;
					if (anim_step > 5) finish_clear();
				}
				break;

			case ST_PAUSE:
				if (new_key == 7)
				{
					state = ST_PLAY;
					fall_time = ms_tick + fall_ms;
					lock_on = 0;
					dirty = 1;
				}
				break;

			case ST_CURTAIN:
				if (reached(anim_time))
				{
					anim_time = ms_tick + CURTAIN_MS;
					anim_step++;
					dirty = 1;
					if (anim_step >= ROWS)
					{
						state = ST_OVER;
						over_time = ms_tick + 800;	// a late key does not restart
					}
				}
				break;

			case ST_OVER:
				if (reached(over_time) && new_key == 5) start_game();
				else if (reached(over_time) && new_key == 7) title_reset();
				else if (reached(frame_time))		// blink NEW HI SCORE
				{
					frame_time = ms_tick + 250;
					dirty = 1;
				}
				break;
		}

		if (state != ST_TITLE && dirty && reached(draw_time))
		{
			draw_time = ms_tick + DRAW_MS;
			dirty = 0;
			render_game();
		}
	}
}
