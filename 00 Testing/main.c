//
// 3D Racer : pseudo-3D car race (OutRun style) on the 128x64 LCD
//
//   road  : track of 1165 segments (200 units long) with curves and hills,
//           every frame the next 80 segments are projected to the screen
//           (far road gets small, curves bend the road, hills lift it),
//           rumble strips / grass bands / centre dashes alternate every
//           3 segments so the speed can be seen
//   race  : you + 5 computer cars, 3 laps, start at the back of the grid
//           bump into a car = you slow down behind it
//           off the road = slow + shaking, hit a tree / sign = crash
//           curves throw the car to the outside, faster = stronger
//
//   VR1 (GPA7 = ADC7) : throttle, turn up = faster (0 ~ 300 km/h)
//   Keypad : 4 = steer left, 6 = steer right, 8 = brake (hold)
//            2 = nitro (2 s boost, 3 at the start, +1 every lap, max 4)
//            5 = start / pause / race again
//            (keys are #defines below, title screen shows the key you press)
//
//   LCD       : page 0 = position, lap, race time, throttle bar
//               128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   7-segment : leftmost digit = lap, right 3 digits = speed km/h
//               scanned by Timer0 interrupt (1 ms, one digit per tick)
//   LEDs      : GPC12~15 = nitro left, start lights during the countdown
//   RGB LED   : green = racing, yellow = off road, red = crash,
//               blue = nitro, magenta = pause, cyan = finished
//   Buzzer    : GPB11 kept off (no sound)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Font5x7.h"
#include "Scankey.h"

#define FAST_LCD	1	// 1 = send each LCD page in one go, 0 = use draw_LCD() (slower)

#define KEY_LEFT	4
#define KEY_RIGHT	6
#define KEY_BRAKE	8
#define KEY_NITRO	2
#define KEY_START	5

// ---- 3D view ----
#define HORIZON		27		// screen row of the horizon
#define FOCAL		100		// screen y = HORIZON + height * FOCAL / distance
#define SEG_LEN		200		// world units per segment
#define DRAW_SEGS	80		// segments drawn ahead
#define NEAR		500		// nearer than this is under the screen
#define ROAD_W		1500		// half road width (2 lanes)
#define CAM_H		1000		// camera height over the road
#define PLAYER_DZ	2800		// camera -> player car (car sits on screen row 63)
#define RUMBLE		3		// segments per stripe
#define MAX_SEGS	1200
#define START_SEG	20		// start / finish line

// ---- car physics (world units, per second) ----
#define MAX_SPEED	12000		// 300 km/h
#define NITRO_SPEED	15000
#define ACCEL		2600
#define BRAKE		9000
#define DECEL		1800		// throttle let off
#define OFF_MAX		3500		// fastest on the grass
#define OFF_DECEL	7000
#define STEER		3200		// sideways speed at full speed
#define CENTRI		3000		// pushed out in the hardest curve at full speed
#define CAR_W		860		// car width
#define CAR_LEN		500
#define NITRO_MS	2000
#define LAPS		3
#define NUM_AI		5

#define ST_TITLE	0
#define ST_COUNT	1
#define ST_RACE		2
#define ST_PAUSE	3
#define ST_FINISH	4

// LCD driver functions without a prototype in LCD.h
extern void lcdSetAddr(unsigned char PA, unsigned char CA);
extern void lcdWriteData(unsigned char temp);
extern unsigned char SEG_BUF[16];	// 7-segment patterns 0~9 (0 = segment on)

// track : segments, curve (-6 left ~ +6 right), height change over the section
const int16_t TRACK[][3] = {
	{ 60,  0,     0},	// start / finish straight
	{ 50,  2,     0},
	{ 40,  0,  1600},	// climb
	{ 50, -4,     0},	// left on the top
	{ 40,  0, -1600},	// down
	{ 30,  0,     0},
	{ 60,  6,   800},	// hard right going up
	{ 30,  0,     0},
	{ 50, -3,  -800},
	{ 25,  0,  2200},	// big crest
	{ 25,  0, -2200},
	{ 40,  0,     0},
	{ 45,  4,     0},	// S bends
	{ 45, -4,     0},
	{ 45,  4,     0},
	{ 40,  0,  1000},
	{ 70, -6,     0},	// hairpin left
	{ 40,  0, -1000},
	{ 60,  0,     0},	// back straight with bumps
	{ 25,  0,   900},
	{ 25,  0,  -900},
	{ 25,  0,   900},
	{ 25,  0,  -900},
	{ 50,  3,     0},
	{ 50,  5,   600},
	{ 40, -2,  -600},
	{ 80,  0,     0}	// home straight
};
#define SECTIONS	(sizeof(TRACK) / sizeof(TRACK[0]))

// sprites : 'X' = dark, 'o' = clear, '.' = see through, 'L' = brake light
// sym = 1 : only the left half is stored, the right half is the mirror
typedef struct {
	const char *const *rows;
	uint8_t w, h, sym;
} sprite_t;

const char *const CAR_ROWS[14] = {
	"..........oooooo",
	".........oXXXXXX",
	"........oXoooooo",
	".......oXooooooo",
	"......oXXXXXXXXX",
	"..oooooXXXXXXXXX",
	".oXXXXXXXXXXXXXX",
	".oXXoXXXXXXXXXXX",
	".oXLLLLXXXXXXXXX",
	".oXLLLLXXXXooooo",
	".oXXXXXXXXXooooo",
	".oXXXXXXXXXXXXXX",
	".oXXXXo.........",
	"..XXXX.........."
};
const char *const TREE_ROWS[22] = {
	"......oX", ".....oXX", "....oXXX", "...oXXXX", "..oXXXXX",
	"...ooXXX", "....oXXX", "...oXXXX", "..oXXXXX", ".oXXXXXX", "oXXXXXXX",
	".ooooXXX", "...oXXXX", "..oXXXXX", ".oXXXXXX", "oXXXXXXX", "oXXXXXXX",
	".ooooooX", "......oX", "......oX", "......oX", "......oX"
};
const char *const SIGN_ROWS[14] = {	// ">>" curve sign, flipped for left curves
	"XXXXXXXXXXXXXXXX",
	"XooooooooooooooX",
	"XoXXooooXXoooooX",
	"XooXXooooXXooooX",
	"XoooXXooooXXoooX",
	"XooooXXooooXXooX",
	"XoooXXooooXXoooX",
	"XooXXooooXXooooX",
	"XoXXooooXXoooooX",
	"XooooooooooooooX",
	"XXXXXXXXXXXXXXXX",
	"......oXXo......",
	"......oXXo......",
	"......oXXo......"
};
const sprite_t CAR  = {CAR_ROWS, 32, 14, 1};
const sprite_t TREE = {TREE_ROWS, 16, 22, 1};
const sprite_t SIGN = {SIGN_ROWS, 16, 14, 0};

// AI cars : top speed of each
const int32_t AI_MAX[NUM_AI] = {11700, 11400, 11100, 10800, 10400};

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

// track data
int8_t   curve4[MAX_SEGS];		// curve x 4 (eased in and out)
int16_t  hill[MAX_SEGS + 1];		// road height at the start of each segment
int32_t  n_segs, track_len;
uint8_t  mtn[128];			// mountain heights

// per drawn segment, kept for the sprites
int32_t  seg_s[DRAW_SEGS];		// scale x 65536 at the near edge
int16_t  seg_sx[DRAW_SEGS];		// road centre on screen
int16_t  seg_sy[DRAW_SEGS];		// road on screen (near edge)
int16_t  seg_clip[DRAW_SEGS];		// rows from here down are hidden by nearer road
int32_t  seg_x4[DRAW_SEGS];		// curve shift x 4 at the near edge
int32_t  seg_dx4[DRAW_SEGS];
uint8_t  seg_ok[DRAW_SEGS];

// player
int32_t  p_dist;			// distance driven (not wrapped)
int32_t  p_x;				// sideways, 0 = road centre, +-ROAD_W = road edges
int32_t  p_speed;
int32_t  dist_rem;
int32_t  cam_y;
uint8_t  braking, offroad;
uint8_t  nitro_left;
int16_t  nitro_ms, crash_ms;

// computer cars
int32_t  ai_dist[NUM_AI], ai_x[NUM_AI], ai_lane[NUM_AI], ai_speed[NUM_AI], ai_rem[NUM_AI];

// race
uint8_t  state = ST_TITLE;
uint8_t  laps_done, position, finish_pos;
int32_t  race_ms, lap_start, best_lap, finish_ms;
int16_t  count_ms, go_ms, msg_ms;
char     msg[17];
int32_t  sky_off;			// mountain scroll x 256
int32_t  throttle;			// 0 ~ 1000 from VR1
uint8_t  last_key = 0;

// 7-segment, filled by main, shown by the Timer0 interrupt
volatile uint32_t ms = 0;
volatile uint8_t  seg_pat[4] = {0xFF, 0xFF, 0xFF, 0xFF};	// [0] = rightmost

// ---------------- Timer0 : 1 ms tick + 7-segment scan ----------------

void TMR0_IRQHandler(void)
{
	static uint8_t digit = 0;

	ms++;
	GPIOC->DOUT &= ~0x00F0;					// all digits off
	GPIOE->DOUT = (GPIOE->DOUT & 0xFF00) | seg_pat[digit];	// segments
	GPIOC->DOUT |= 1 << (4 + digit);			// this digit on
	digit = (digit + 1) & 3;
	TIMER0->TISR.TIF = 1;		// clear interrupt flag
}

void InitTIMER0(void)
{
	SYSCLK->CLKSEL1.TMR0_S = 0;	// 12MHz crystal
	SYSCLK->APBCLK.TMR0_EN = 1;
	TIMER0->TCSR.MODE = 1;		// periodic
	TIMER0->TCSR.PRESCALE = 11;	// 12MHz / 12 = 1MHz, 1 us per count
	TIMER0->TCMPR = 1000;		// 1 ms
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;
	NVIC_EnableIRQ(TMR0_IRQn);
	TIMER0->TCSR.CRST = 1;
	TIMER0->TCSR.CEN = 1;
}

// ---------------- VR1 throttle (ADC7, polling) ----------------

void InitADC(void)
{
	GPIOA->OFFD |= 0x00800000;		// digital input path of GPA7 off
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
	SYSCLK->CLKSEL1.ADC_S = 3;		// 22MHz RC
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;
	ADC->ADCR.ADEN = 1;
	ADC->ADCR.DIFFEN = 0;			// single-end
	ADC->ADCR.ADMD = 0;			// single mode
	ADC->ADCHER.CHEN = 0x80;		// channel 7 only
	ADC->ADSR.ADF = 1;
	ADC->ADCR.ADST = 1;			// first conversion
}

// take the last result (if ready) and start the next one, throttle 0 ~ 1000
void read_throttle(void)
{
	int32_t t;

	if (ADC->ADSR.ADF == 0) return;
	ADC->ADSR.ADF = 1;
	t = ADC->ADDR[7].RSLT;			// 0 ~ 4095
	ADC->ADCR.ADST = 1;
	t = (t - 150) * 1000 / 3800;		// small dead zone at both ends
	if (t < 0) t = 0;
	if (t > 1000) t = 1000;
	throttle = (throttle * 3 + t) / 4;	// smooth
}

// ---------------- LEDs ----------------

// RGB LED GPA12 B / 13 G / 14 R, low = on
void rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if (r) DrvGPIO_ClrBit(E_GPA, 14); else DrvGPIO_SetBit(E_GPA, 14);
	if (g) DrvGPIO_ClrBit(E_GPA, 13); else DrvGPIO_SetBit(E_GPA, 13);
	if (b) DrvGPIO_ClrBit(E_GPA, 12); else DrvGPIO_SetBit(E_GPA, 12);
}

// LEDs GPC12~15, n of them on from GPC12 (low = on)
void leds(uint8_t n)
{
	uint32_t on = (0x0F >> (4 - n)) & 0x0F;

	__disable_irq();		// the Timer0 interrupt also writes GPIOC->DOUT
	GPIOC->DOUT = (GPIOC->DOUT | 0xF000) & ~(on << 12);
	__enable_irq();
}

// 7-segment : lap on the left digit, speed on the right 3 digits
void seg_show(int32_t lap, int32_t kmh)
{
	seg_pat[3] = (lap > 0) ? SEG_BUF[lap % 10] : 0xFD;	// 0xFD = '-'
	seg_pat[2] = (kmh >= 100) ? SEG_BUF[(kmh / 100) % 10] : 0xFF;
	seg_pat[1] = (kmh >= 10) ? SEG_BUF[(kmh / 10) % 10] : 0xFF;
	seg_pat[0] = SEG_BUF[kmh % 10];
}

// ---------------- track ----------------

void build_track(void)
{
	int32_t s, k, len, c, y0, dh, ramp, t, sm;
	int32_t i = 0, y = 0;

	for (s = 0; s < SECTIONS; s++)
	{
		len = TRACK[s][0];
		c = TRACK[s][1] * 4;
		dh = TRACK[s][2];
		ramp = len / 4;
		y0 = y;
		for (k = 0; k < len && i < MAX_SEGS; k++, i++)
		{
			// curve eases in over the first quarter and out over the last
			if (k < ramp)              curve4[i] = c * k / ramp;
			else if (k >= len - ramp)  curve4[i] = c * (len - k) / ramp;
			else                       curve4[i] = c;
			// height follows a smooth S (smoothstep)
			t = k * 1024 / len;
			sm = t * t / 1024 * (3072 - 2 * t) / 1024;
			hill[i] = y0 + dh * sm / 1024;
		}
		y = y0 + dh;
	}
	n_segs = i;
	hill[n_segs] = hill[0];		// the track joins up
	track_len = n_segs * SEG_LEN;

	// mountains : 3 triangle waves that repeat every 128 pixels
	for (i = 0; i < 128; i++)
	{
		k = 3 + abs((i % 64) - 32) * 10 / 32
		      + abs(((i + 20) % 32) - 16) * 4 / 16
		      + abs(((i + 5) % 16) - 8) * 2 / 8;
		mtn[i] = (k > 17) ? 17 : k;
	}
}

// distance -> 0 ~ track_len - 1
int32_t wrap(int32_t d)
{
	d %= track_len;
	if (d < 0) d += track_len;
	return d;
}

// road height anywhere on the track
int32_t road_y(int32_t d)
{
	int32_t i, f;

	d = wrap(d);
	i = d / SEG_LEN;
	f = d % SEG_LEN;
	return hill[i] + (hill[i + 1] - hill[i]) * f / SEG_LEN;
}

// roadside object of a segment : 0 none, 1 tree, 2 sign ; side -1 left / +1 right
uint8_t scenery(int32_t i, int8_t *side, int32_t *off)
{
	int32_t c = curve4[i];
	uint32_t h = (i * 2654435761u) >> 24;		// cheap random 0~255

	if (i == START_SEG) return 0;
	if ((c >= 12 || c <= -12) && i % 6 == 0)	// curve signs on the outside
	{
		*side = (c > 0) ? -1 : 1;
		*off = ROAD_W + 600;
		return 2;
	}
	if (i % 8 == 0 || (((i / 150) & 1) && i % 4 == 0))	// trees, thicker in places
	{
		*side = (h & 1) ? 1 : -1;
		*off = ROAD_W + 1000 + (h & 7) * 200;
		return 1;
	}
	if (i % 8 == 4)
	{
		*side = (h & 1) ? -1 : 1;
		*off = ROAD_W + 1000 + ((h >> 3) & 7) * 200;
		return 1;
	}
	return 0;
}

// ---------------- drawing into screen[] ----------------

void set_px(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

void clr_px(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] &= ~(1 << (y & 7));
}

// filled box, mode 0 = clear, 1 = dark, 2+ = checker with cells of (mode - 1) px
void box(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t mode, int16_t clip)
{
	int32_t x, y, cell = mode - 1;
	int32_t xa = (x0 < 0) ? 0 : x0, xb = (x1 >= LCD_Xmax) ? LCD_Xmax - 1 : x1;
	int32_t ya = (y0 < 0) ? 0 : y0;

	for (y = ya; y <= y1 && y < clip && y < LCD_Ymax; y++)
		for (x = xa; x <= xb; x++)
		{
			if (mode == 0) clr_px(x, y);
			else if (mode == 1) set_px(x, y);
			else if (((x - x0) / cell + (y - y0) / cell) & 1) set_px(x, y);
			else clr_px(x, y);
		}
}

// write text with the 5x7 font into one page of screen[] (clears behind it)
void text_5x7(int16_t x, int8_t page, char *text)
{
	int8_t i;

	if (x > 0) screen[x - 1 + page * LCD_Xmax] = 0;
	while (*text && x <= LCD_Xmax - 6)
	{
		for (i = 0; i < 5; i++)
			screen[x + i + page * LCD_Xmax] = Font5x7[(*text - 0x20) * 5 + i];
		screen[x + 5 + page * LCD_Xmax] = 0;
		x += 6;
		text++;
	}
}

void text_centre(int8_t page, char *text)
{
	text_5x7((LCD_Xmax - 6 * (int16_t)strlen(text)) / 2, page, text);
}

// big text : font pixels drawn as size x size blocks, white box behind
void text_big(int16_t y, char *text, uint8_t size)
{
	int16_t n = strlen(text), w = n * 6 * size, x0 = (LCD_Xmax - w) / 2;
	int16_t c, i, j;
	uint8_t col;

	box(x0 - 3, y - 3, x0 + w + 1, y + 7 * size + 2, 0, LCD_Ymax);
	for (c = 0; c < n; c++)
		for (i = 0; i < 5; i++)
		{
			col = Font5x7[(text[c] - 0x20) * 5 + i];
			for (j = 0; j < 7; j++)
				if (col & (1 << j))
					box(x0 + (c * 6 + i) * size, y + j * size,
					    x0 + (c * 6 + i) * size + size - 1, y + j * size + size - 1, 1, LCD_Ymax);
		}
}

// scaled sprite, bottom centre at (cx, by), dw pixels wide, rows >= clip hidden
void draw_sprite(const sprite_t *s, int16_t cx, int16_t by, int32_t dw, int16_t clip, uint8_t flip, uint8_t lights)
{
	int32_t dh, x0, y0, xs, ys, i, j, x, y, c;
	const char *row;
	char ch;

	if (dw < 2 || dw > 250) return;
	dh = dw * s->h / s->w;
	if (dh < 1) return;
	x0 = cx - dw / 2;
	y0 = by - dh;
	xs = (s->w << 8) / dw;		// source step x 256
	ys = (s->h << 8) / dh;
	for (j = 0; j < dh; j++)
	{
		y = y0 + j;
		if (y < 8) continue;		// keep the HUD clear
		if (y >= clip || y >= LCD_Ymax) break;
		row = s->rows[(j * ys) >> 8];
		for (i = 0; i < dw; i++)
		{
			x = x0 + i;
			if (x < 0) continue;
			if (x >= LCD_Xmax) break;
			c = (i * xs) >> 8;
			if (flip) c = s->w - 1 - c;
			if (s->sym && c >= s->w / 2) c = s->w - 1 - c;
			ch = row[c];
			if (ch == '.') continue;
			if (ch == 'X' || (ch == 'L' && lights)) set_px(x, y);
			else clr_px(x, y);
		}
	}
}

// sky with mountains, distant grass under the horizon
void draw_background(void)
{
	int16_t x, y, h;

	memset(screen, 0, sizeof(screen));
	for (x = 0; x < LCD_Xmax; x++)
	{
		h = mtn[(x + (sky_off >> 8)) & 127];
		set_px(x, HORIZON - h);				// mountain outline
		for (y = HORIZON - h + 2; y < HORIZON; y += 2)	// light hatching
			if (((x + y) & 3) == 0) set_px(x, y);
		for (y = HORIZON; y < LCD_Ymax; y++)		// far grass
			if (((x + y) & 3) == 0) set_px(x, y);
	}
	// sun : round dot
	box(103, 11, 107, 15, 1, LCD_Ymax);
	box(104, 10, 106, 16, 1, LCD_Ymax);
	box(102, 12, 108, 14, 1, LCD_Ymax);
}

// one road row : grass bands, rumble strips, road, centre dashes, finish line
void draw_row(int16_t y, int32_t cx, int32_t w, uint8_t alt, uint8_t finish)
{
	uint8_t *p = &screen[(y >> 3) * LCD_Xmax];
	uint8_t bit = 1 << (y & 7);
	int32_t x, d, rw = w / 6 + 1, lw = w / 40, on;

	for (x = 0; x < LCD_Xmax; x++)
	{
		d = x - cx;
		if (d < 0) d = -d;
		if (d > w)				// grass : dark / light bands
			on = alt ? ((x + y) & 1) == 0 : ((x + y) & 3) == 0;
		else if (d > w - rw)			// rumble strip
			on = alt;
		else if (finish)			// chequered line
			on = (((x - cx + 1024) >> 2) + (y >> 1)) & 1;
		else					// road, centre dash
			on = alt && d <= lw;
		if (on) p[x] |= bit;
		else    p[x] &= ~bit;
	}
}

// project and draw the road from near to far, remember each segment for sprites
void draw_road(int32_t cam)
{
	int32_t base, frac, n, i, dz1, dz2, s1, s2, x4, dx4;
	int32_t sx1, sy1, w1, sx2, sy2, w2, y, ya, yb, t;
	int16_t maxy = LCD_Ymax;

	base = cam / SEG_LEN;
	frac = cam % SEG_LEN;
	x4 = 0;
	dx4 = -(curve4[base] * frac) / SEG_LEN;

	for (n = 0; n < DRAW_SEGS; n++)
	{
		i = base + n;
		if (i >= n_segs) i -= n_segs;
		dz1 = n * SEG_LEN - frac;
		dz2 = dz1 + SEG_LEN;
		seg_ok[n] = 0;
		seg_clip[n] = maxy;
		seg_x4[n] = x4;
		seg_dx4[n] = dx4;

		if (dz1 > NEAR)
		{
			s1 = (FOCAL << 16) / dz1;
			s2 = (FOCAL << 16) / dz2;
			sy1 = HORIZON + (((cam_y - hill[i]) * s1) >> 16);
			sy2 = HORIZON + (((cam_y - hill[i + 1]) * s2) >> 16);
			sx1 = 64 + ((((x4 >> 2) - p_x) * s1) >> 16);
			sx2 = 64 + (((((x4 + dx4) >> 2) - p_x) * s2) >> 16);
			w1 = (ROAD_W * s1) >> 16;
			w2 = (ROAD_W * s2) >> 16;

			seg_ok[n] = 1;
			seg_s[n] = s1;
			seg_sx[n] = (sx1 < -30000) ? -30000 : (sx1 > 30000) ? 30000 : sx1;
			seg_sy[n] = (sy1 > 30000) ? 30000 : sy1;

			if (sy2 < sy1 && sy2 < maxy)
			{
				ya = (sy2 < 8) ? 8 : sy2;
				yb = (sy1 < maxy) ? sy1 : maxy;
				for (y = ya; y < yb; y++)
				{
					t = (sy1 - y) * 256 / (sy1 - sy2);
					draw_row(y, sx1 + (((sx2 - sx1) * t) >> 8), w1 + (((w2 - w1) * t) >> 8),
					         (i / RUMBLE) & 1, i == START_SEG);
				}
				maxy = (sy2 < 8) ? 8 : sy2;
			}
		}
		x4 += dx4;
		dx4 += curve4[i];
	}
}

// trees, signs, the start gantry and the other cars, far to near
void draw_sprites(int32_t cam)
{
	int32_t base = cam / SEG_LEN, frac = cam % SEG_LEN;
	int32_t n, i, k, s, off, rel, sx, sy, x4, post, top, band;
	int8_t side;
	uint8_t type;
	int16_t ai_n[NUM_AI];

	// which segment each car is in (-1 = not in view)
	for (k = 0; k < NUM_AI; k++)
	{
		rel = wrap(ai_dist[k] - cam);
		ai_n[k] = (rel > NEAR && rel < (DRAW_SEGS - 1) * SEG_LEN) ? (rel + frac) / SEG_LEN : -1;
	}

	for (n = DRAW_SEGS - 1; n > 0; n--)
	{
		i = base + n;
		if (i >= n_segs) i -= n_segs;

		if (seg_ok[n])
		{
			s = seg_s[n];
			type = scenery(i, &side, &off);
			if (type == 1)
				draw_sprite(&TREE, seg_sx[n] + ((side * off * s) >> 16), seg_sy[n],
				            (1400 * s) >> 16, seg_clip[n], 0, 0);
			else if (type == 2)
				draw_sprite(&SIGN, seg_sx[n] + ((side * off * s) >> 16), seg_sy[n],
				            (900 * s) >> 16, seg_clip[n], side > 0, 0);

			if (i == START_SEG)		// start / finish gantry
			{
				post = ((ROAD_W + 300) * s) >> 16;
				top = seg_sy[n] - ((2800 * s) >> 16);
				band = (700 * s) >> 16;
				k = (150 * s) >> 16;
				box(seg_sx[n] - post - k, top, seg_sx[n] - post, seg_sy[n] - 1, 1, seg_clip[n]);
				box(seg_sx[n] + post, top, seg_sx[n] + post + k, seg_sy[n] - 1, 1, seg_clip[n]);
				box(seg_sx[n] - post, top, seg_sx[n] + post, top + band, 2 + band / 3, seg_clip[n]);
			}
		}

		// cars in this segment, at their own distance
		for (k = 0; k < NUM_AI; k++)
			if (ai_n[k] == n)
			{
				rel = wrap(ai_dist[k] - cam);
				s = (FOCAL << 16) / rel;
				x4 = seg_x4[n] + seg_dx4[n] * (rel + frac - n * SEG_LEN) / SEG_LEN;
				sx = 64 + ((((x4 >> 2) + ai_x[k] - p_x) * s) >> 16);
				sy = HORIZON + (((cam_y - road_y(ai_dist[k])) * s) >> 16);
				draw_sprite(&CAR, sx, sy, (CAR_W * s) >> 16, seg_clip[n], 0, 0);
			}
	}
}

// race time "m:ss.t"
void time_text(char *buf, int32_t t)
{
	sprintf(buf, "%d:%02d.%d", t / 60000, (t / 1000) % 60, (t / 100) % 10);
}

void draw_hud(void)
{
	char TEXT[22], T[10];
	int16_t x, w;

	time_text(T, race_ms);
	sprintf(TEXT, "P%d/%d L%d/%d %s", position, NUM_AI + 1,
	        (laps_done < LAPS) ? laps_done + 1 : LAPS, LAPS, T);
	text_5x7(0, 0, TEXT);
	// throttle bar
	w = throttle * 24 / 1000;
	for (x = 102; x <= 127; x++)
	{
		set_px(x, 1);
		set_px(x, 6);
	}
	for (x = 2; x <= 5; x++)
	{
		set_px(102, x);
		set_px(127, x);
	}
	box(103, 2, 103 + w, 5, 1, LCD_Ymax);
}

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

// ---------------- game ----------------

void reset_race(void)
{
	int32_t k;

	p_dist = 0;
	p_x = ROAD_W / 2;		// right lane, back of the grid
	p_speed = 0;
	dist_rem = 0;
	nitro_left = 3;
	nitro_ms = crash_ms = 0;
	for (k = 0; k < NUM_AI; k++)
	{
		ai_dist[k] = 700 * (k + 1);
		ai_lane[k] = (k & 1) ? ROAD_W / 2 : -ROAD_W / 2;
		ai_x[k] = ai_lane[k];
		ai_speed[k] = 0;
		ai_rem[k] = 0;
	}
	laps_done = 0;
	race_ms = lap_start = 0;
	best_lap = 0;
	position = NUM_AI + 1;
	msg_ms = 0;
}

// move a value towards a target by at most step
int32_t towards(int32_t v, int32_t target, int32_t step)
{
	if (v < target) return (v + step > target) ? target : v + step;
	return (v - step < target) ? target : v - step;
}

// any car (player = index NUM_AI) in lane x, from d to d + ahead?
int32_t blocker(int32_t me, int32_t d, int32_t x, int32_t ahead)
{
	int32_t k, rel;

	for (k = 0; k <= NUM_AI; k++)
	{
		if (k == me) continue;
		rel = wrap(((k == NUM_AI) ? p_dist : ai_dist[k]) - d);
		if (rel > track_len / 2) rel -= track_len;		// behind = negative
		if (rel > -CAR_LEN && rel < ahead
		    && abs(((k == NUM_AI) ? p_x : ai_x[k]) - x) < CAR_W)
			return k;
	}
	return -1;
}

void ai_step(int32_t dt)
{
	int32_t k, b, target, c, gap;

	for (k = 0; k < NUM_AI; k++)
	{
		c = curve4[wrap(ai_dist[k]) / SEG_LEN];
		target = AI_MAX[k] - abs(c) * 110;		// slower in curves
		gap = ai_dist[k] - p_dist;			// keep the race close
		if (gap > track_len / 4) target -= 1500;
		if (gap < -track_len / 4) target += 1000;

		// car ahead in my lane : change lane, or follow it
		b = blocker(k, ai_dist[k], ai_lane[k], 2000);
		if (b >= 0)
		{
			if (blocker(k, ai_dist[k] - CAR_LEN, -ai_lane[k], 2000) < 0)
				ai_lane[k] = -ai_lane[k];
			else if (target > ((b == NUM_AI) ? p_speed : ai_speed[b]))
				target = (b == NUM_AI) ? p_speed : ai_speed[b];
		}

		ai_speed[k] = towards(ai_speed[k], target, ((ai_speed[k] < target) ? 2200 : 5000) * dt / 1000);
		ai_x[k] = towards(ai_x[k], ai_lane[k], 1300 * dt / 1000);
		ai_rem[k] += ai_speed[k] * dt;
		ai_dist[k] += ai_rem[k] / 1000;
		ai_rem[k] %= 1000;
	}
}

void player_step(int32_t dt, uint8_t key, uint8_t new_key)
{
	int32_t i, k, rel, target, lat, off;
	int8_t side;

	i = wrap(p_dist) / SEG_LEN;
	braking = (key == KEY_BRAKE);
	offroad = (p_x < -ROAD_W || p_x > ROAD_W);

	if (new_key == KEY_NITRO && nitro_left && nitro_ms == 0)
	{
		nitro_left--;
		nitro_ms = NITRO_MS;
	}

	// speed from VR1, brake key, grass
	target = MAX_SPEED * throttle / 1000;
	if (nitro_ms > 0)
	{
		target = NITRO_SPEED;
		nitro_ms = (nitro_ms > dt) ? nitro_ms - dt : 0;
	}
	if (braking)
		p_speed -= BRAKE * dt / 1000;
	else if (p_speed < target)
		p_speed = towards(p_speed, target, ACCEL * ((nitro_ms > 0) ? 2 : 1) * dt / 1000);
	else
		p_speed = towards(p_speed, target, DECEL * dt / 1000);
	if (offroad && p_speed > OFF_MAX)
		p_speed -= OFF_DECEL * dt / 1000;
	if (p_speed < 0) p_speed = 0;

	// steering (needs speed) and the curve pushing outwards
	lat = 0;
	if (key == KEY_LEFT)  lat -= STEER * p_speed / MAX_SPEED;
	if (key == KEY_RIGHT) lat += STEER * p_speed / MAX_SPEED;
	lat -= CENTRI * (p_speed / 100) / (MAX_SPEED / 100) * curve4[i] / 24;
	p_x += lat * dt / 1000;
	if (p_x < -ROAD_W * 5 / 2) p_x = -ROAD_W * 5 / 2;
	if (p_x >  ROAD_W * 5 / 2) p_x =  ROAD_W * 5 / 2;

	// move
	dist_rem += p_speed * dt;
	p_dist += dist_rem / 1000;
	dist_rem %= 1000;

	// hit a tree / sign beside the road
	if (offroad && crash_ms == 0)
		for (k = 0; k < 2; k++)
			if (scenery((i + k) % n_segs, &side, &off))
				if (abs(side * off - p_x) < CAR_W / 2 + 250)
				{
					p_speed /= 5;
					p_x = towards(p_x, 0, 600);
					crash_ms = 700;
				}

	// bump into a car
	for (k = 0; k < NUM_AI; k++)
	{
		rel = wrap(ai_dist[k] - p_dist);
		if (abs(ai_x[k] - p_x) >= CAR_W * 3 / 4) continue;
		if (rel < CAR_LEN && p_speed > ai_speed[k])		// I run into it
		{
			p_speed = ai_speed[k] * 3 / 4;
			p_dist -= CAR_LEN - rel;
			crash_ms = 300;
		}
		else if (rel > track_len - CAR_LEN && ai_speed[k] > p_speed)	// it runs into me
		{
			ai_speed[k] = p_speed * 9 / 10;
			ai_dist[k] -= rel - (track_len - CAR_LEN);
		}
	}
	if (crash_ms > 0) crash_ms = (crash_ms > dt) ? crash_ms - dt : 0;

	// mountains scroll with the curves
	sky_off += curve4[i] * (p_speed / 100) * dt / 560;
}

void race_progress(int32_t dt)
{
	int32_t k, lap;
	char T[10];

	race_ms += dt;
	position = 1;
	for (k = 0; k < NUM_AI; k++)
		if (ai_dist[k] > p_dist) position++;

	if (p_dist >= START_SEG * SEG_LEN + (laps_done + 1) * track_len)
	{
		laps_done++;
		lap = race_ms - lap_start;
		lap_start = race_ms;
		if (best_lap == 0 || lap < best_lap) best_lap = lap;
		if (nitro_left < 4) nitro_left++;
		time_text(T, lap);
		if (laps_done >= LAPS)
		{
			state = ST_FINISH;
			finish_pos = position;
			finish_ms = race_ms;
		}
		else
		{
			sprintf(msg, (laps_done == LAPS - 1) ? "FINAL LAP %s" : "LAP %s", T);
			msg_ms = 2000;
		}
	}
}

void render(void)
{
	int32_t cam = wrap(p_dist - PLAYER_DZ);
	int16_t bump = 0;

	cam_y = CAM_H + road_y(p_dist);
	draw_background();
	draw_road(cam);
	draw_sprites(cam);
	if (offroad && p_speed > 500) bump = (ms >> 6) & 1;
	if (crash_ms > 0) bump = (ms >> 5) & 1;
	draw_sprite(&CAR, 64 + ((crash_ms > 0) ? (int16_t)((ms >> 5) & 2) - 1 : 0), LCD_Ymax - bump,
	            32, LCD_Ymax, 0, braking);
}

// ---------------- main ----------------

int32_t main(void)
{
	uint8_t key, new_key, i;
	uint32_t last_ms;
	int32_t dt;
	char TEXT[22], T[10];

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	DrvGPIO_Open(E_GPB, 11, E_IO_OUTPUT);	// buzzer kept off (no sound)
	DrvGPIO_SetBit(E_GPB, 11);
	for (i = 12; i <= 14; i++)		// RGB LED
	{
		DrvGPIO_Open(E_GPA, i, E_IO_OUTPUT);
		DrvGPIO_SetBit(E_GPA, i);
	}
	for (i = 12; i <= 15; i++)		// LEDs
	{
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
		DrvGPIO_SetBit(E_GPC, i);
	}
	for (i = 4; i <= 7; i++)		// 7-segment digits
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
	for (i = 0; i <= 7; i++)		// 7-segment segments
		DrvGPIO_Open(E_GPE, i, E_IO_OUTPUT);

	init_LCD();
	clear_LCD();
	OpenKeyPad();
	InitADC();
	InitTIMER0();
	build_track();
	reset_race();

	last_ms = ms;
	while (1)
	{
		// ~40 frames per second at most, dt = real time of the last frame
		while (ms - last_ms < 25);
		dt = ms - last_ms;
		last_ms += dt;
		if (dt > 60) dt = 60;

		read_throttle();
		key = ScanKey();
		new_key = (key != last_key) ? key : 0;
		last_key = key;

		switch (state)
		{
			case ST_TITLE:			// demo drive behind the title
				p_speed = 7000;
				p_x = towards(p_x, 0, 40);
				dist_rem += p_speed * dt;
				p_dist += dist_rem / 1000;
				dist_rem %= 1000;
				ai_step(dt);
				sky_off += curve4[wrap(p_dist) / SEG_LEN] * 70 * dt / 560;
				render();
				if ((ms / 2500) & 1)
				{
					text_big(11, "3D RACER", 2);
					text_centre(5, " VR1 = THROTTLE ");
				}
				else
				{
					text_centre(2, " 4/6 STEER ");
					text_centre(3, " 8 BRAKE 2 NITRO ");
					sprintf(TEXT, " KEY PRESSED: %d ", key);
					text_centre(4, TEXT);
				}
				if ((ms >> 9) & 1) text_centre(7, " PRESS 5 TO START ");
				seg_show(0, throttle * 300 / 1000);
				rgb(0, 0, 0);
				leds(0);
				if (new_key == KEY_START)
				{
					reset_race();
					count_ms = 3000;
					state = ST_COUNT;
				}
				break;

			case ST_COUNT:			// 3 - 2 - 1 - GO, LEDs are the start lights
				render();
				draw_hud();
				count_ms -= dt;
				sprintf(TEXT, "%d", count_ms / 1000 + 1);
				text_big(10, TEXT, 3);
				text_centre(6, " SET VR1 ! ");
				leds(3 - count_ms / 1000);
				rgb(1, 0, 0);
				seg_show(1, 0);
				if (count_ms <= 0)
				{
					go_ms = 800;
					state = ST_RACE;
				}
				break;

			case ST_RACE:
				player_step(dt, key, new_key);
				ai_step(dt);
				race_progress(dt);
				render();
				draw_hud();
				if (go_ms > 0)
				{
					go_ms -= dt;
					text_big(12, "GO!", 3);
				}
				else if (msg_ms > 0)
				{
					msg_ms -= dt;
					text_centre(2, msg);
				}
				if (crash_ms > 0)            rgb(1, 0, 0);
				else if (nitro_ms > 0)       rgb(0, 0, 1);
				else if (offroad)            rgb(1, 1, 0);
				else                         rgb(0, 1, 0);
				leds(nitro_left);
				seg_show(laps_done + 1, p_speed / 40);
				if (new_key == KEY_START) state = ST_PAUSE;
				break;

			case ST_PAUSE:
				render();
				draw_hud();
				text_big(14, "PAUSE", 2);
				text_centre(6, " 5 = GO ON ");
				rgb(1, 0, 1);
				if (new_key == KEY_START) state = ST_RACE;
				break;

			default:			// ST_FINISH : car rolls on, results
				throttle = 400;
				p_x = towards(p_x, ROAD_W / 2, 30);
				p_speed = towards(p_speed, 5000, 3000 * dt / 1000);
				dist_rem += p_speed * dt;
				p_dist += dist_rem / 1000;
				dist_rem %= 1000;
				ai_step(dt);
				render();
				sprintf(TEXT, "P%d", finish_pos);
				text_big(10, TEXT, 2);
				time_text(T, finish_ms);
				sprintf(TEXT, " TIME %s ", T);
				text_centre(4, TEXT);
				time_text(T, best_lap);
				sprintf(TEXT, " BEST %s ", T);
				text_centre(5, TEXT);
				text_centre(7, (finish_pos == 1) ? " WINNER! 5=AGAIN " : " 5 = RACE AGAIN ");
				rgb(0, 1, 1);
				leds(0);
				seg_show(LAPS, p_speed / 40);
				if (new_key == KEY_START)
				{
					reset_race();
					count_ms = 3000;
					state = ST_COUNT;
				}
				break;
		}
		lcd_update();
	}
}
