//
// Smpl_Dino_Game : Chrome offline dinosaur game on Nu-LB-NUC140
//
//   LCD    : 128x64 via SPI3, whole frame drawn in screen[] then draw_LCD()
//   Keypad : GPA0~5, ScanKey() returns 1~9
//            key 1~6 = JUMP   (press 2 to start / jump)
//            key 7~9 = DUCK   (press 8 to duck, fast drop in air)
//   7-seg  : turned off (score is shown on LCD)
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Font5x7.h"
#include "Seven_Segment.h"
#include "Scankey.h"

// ---------------- game settings ----------------
#define FRAME_US	25000		// delay per frame (us), about 30 frames/s
#define GROUND_Y	60		// ground line row
#define DINO_X		8		// dino left position
#define JUMP_V		96		// jump speed      (x16 fixed point)
#define GRAVITY		8		// gravity         (x16 fixed point)
#define FAST_DROP	16		// extra gravity when duck key held in air
#define SPEED_START	36		// 2.25 px/frame   (x16 fixed point)
#define SPEED_MAX	88		// 5.5 px/frame    (x16 fixed point)
#define SPEED_STEP	8		// speed +1 every SPEED_STEP points
#define MAX_OBS		3		// obstacles on screen at the same time

#define STATE_START	0
#define STATE_PLAY	1
#define STATE_OVER	2

// ---------------- sprites ('#' = pixel on) ----------------
const char *dino_body[13] = {
	"........#######.",
	".......##.######",
	".......#########",
	".......#########",
	".......#####....",
	".......########.",
	"#.....#####.....",
	"#....#######....",
	"##..#########...",
	"###########.#...",
	".#########......",
	"..#######.......",
	"...#####........",
};
const char *dino_legs[3][3] = {
	{ "....##..##......", "....#....#......", "....##...##....." },	// standing
	{ "....##..##......", "....#...........", "....##.........." },	// run frame 1
	{ "....##..##......", ".........#......", ".........##....." },	// run frame 2
};
const char *dino_duck[2][9] = {
	{
	"..............######",
	"#............##.####",
	"##...###############",
	"###################.",
	".################...",
	"..###########.#.....",
	"...#########........",
	"....##..##..........",
	"....#.....##........",
	},{
	"..............######",
	"#............##.####",
	"##...###############",
	"###################.",
	".################...",
	"..###########.#.....",
	"...#########........",
	"....##..##..........",
	"....##...#..........",
	}
};
const char *cactus_small[15] = {		// bold: 5 px trunk, 2 px arms
	"....###....",
	"...#####...",
	"...#####.##",
	"##.#####.##",
	"##.#####.##",
	"##.########",
	"##.#######.",
	"##########.",
	".########..",
	"...#####...",
	"...#####...",
	"...#####...",
	"...#####...",
	"...#####...",
	"...#####...",
};
const char *cactus_big[20] = {		// bold: 6 px trunk, 2 px arms
	"....####....",
	"...######...",
	"...######.##",
	"##.######.##",
	"##.######.##",
	"##.######.##",
	"##.#########",
	"##.########.",
	"##.######...",
	"#########...",
	".########...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
	"...######...",
};
const char *bird[2][10] = {
	{
	".....#..........",
	".....##.........",
	".....###........",
	"..#..####.......",
	".##..#####......",
	"###############.",
	"......#########.",
	".......#######..",
	"................",
	"................",
	},{
	"................",
	"................",
	"..#.............",
	".##.............",
	"###############.",
	"......#########.",
	".....#######....",
	".....###........",
	".....##.........",
	".....#..........",
	}
};
const char *cloud[4] = {
	".....####....",
	"..###....##..",
	".#.........#.",
	"#############",
};

// ---------------- game variables ----------------
typedef struct {
	int32_t x16;		// x position (x16 fixed point)
	int8_t  y;		// top row
	uint8_t type;		// 0 small cactus, 1 big cactus, 2 double cactus, 3 bird
	uint8_t active;
} OBSTACLE;

unsigned char screen[128 * 8];	// LCD frame buffer, 8 pages x 128 columns
OBSTACLE obs[MAX_OBS];
int32_t  dino_y16;		// height above ground (x16 fixed point)
int32_t  dino_v16;		// vertical speed      (x16 fixed point)
uint8_t  ducking;
int32_t  speed16;
int32_t  gap16;			// distance until next obstacle spawns
int32_t  ground_off;
int32_t  cloud_x16[2];
int8_t   cloud_y[2];
uint32_t score, hi_score = 0;
uint32_t frame = 0;
uint32_t seed = 12345;

// ---------------- helpers ----------------
uint32_t rnd(void)
{
	seed = seed * 1103515245 + 12345;
	return (seed >> 16) & 0x7FFF;
}

void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x > 127 || y < 0 || y > 63) return;
	screen[x + (y >> 3) * 128] |= (1 << (y & 7));
}

void clr_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x > 127 || y < 0 || y > 63) return;
	screen[x + (y >> 3) * 128] &= ~(1 << (y & 7));
}

uint8_t get_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x > 127 || y < 0 || y > 63) return 0;
	return (screen[x + (y >> 3) * 128] >> (y & 7)) & 1;
}

// draw a sprite, return 1 if any of its pixels lands on a pixel already on
uint8_t draw_sprite(int16_t x, int16_t y, const char *rows[], uint8_t h)
{
	uint8_t r, c, w, hit = 0;
	for (r = 0; r < h; r++)
	{
		w = strlen(rows[r]);
		for (c = 0; c < w; c++)
			if (rows[r][c] == '#')
			{
				if (get_pixel(x + c, y + r)) hit = 1;
				set_pixel(x + c, y + r);
			}
	}
	return hit;
}

// draw text with 5x7 font (6 px per char) into the frame buffer
void draw_text(int16_t x, int16_t y, char *s)
{
	uint8_t c, b, col;
	while (*s)
	{
		for (c = 0; c < 5; c++)
		{
			col = Font5x7[(*s - 0x20) * 5 + c];
			for (b = 0; b < 7; b++)
				if ((col >> b) & 1) set_pixel(x + c, y + b);
		}
		x += 6;
		s++;
	}
}

// clear a box then draw centred text inside it
void draw_text_box(int16_t y, char *s)
{
	int16_t w = strlen(s) * 6;
	int16_t x = (128 - w) / 2;
	int16_t i, j;
	for (j = y - 2; j < y + 9; j++)
		for (i = x - 3; i < x + w + 2; i++)
			clr_pixel(i, j);
	draw_text(x, y, s);
}

// ---------------- game logic ----------------
void reset_game(void)
{
	uint8_t i;
	for (i = 0; i < MAX_OBS; i++) obs[i].active = 0;
	dino_y16 = 0;
	dino_v16 = 0;
	ducking  = 0;
	speed16  = SPEED_START;
	gap16    = 60 * 16;
	score    = 0;
	cloud_x16[0] = 40 * 16;  cloud_y[0] = 12;
	cloud_x16[1] = 110 * 16; cloud_y[1] = 22;
}

void spawn_obstacle(void)
{
	uint8_t i;
	for (i = 0; i < MAX_OBS; i++)
		if (!obs[i].active) break;
	if (i == MAX_OBS) return;

	obs[i].active = 1;
	obs[i].x16 = 130 * 16;
	obs[i].type = rnd() % ((score > 150) ? 4 : 3);	// birds after score 150

	switch (obs[i].type)
	{
		case 0:  obs[i].y = GROUND_Y - 15; break;		// small cactus
		case 1:  obs[i].y = GROUND_Y - 20; break;		// big cactus
		case 2:  obs[i].y = GROUND_Y - 15; break;		// double cactus
		default:						// bird
			switch (rnd() % 3)
			{
				case 0:  obs[i].y = GROUND_Y - 12; break;	// low : jump
				case 1:  obs[i].y = GROUND_Y - 20; break;	// mid : duck
				default: obs[i].y = GROUND_Y - 34; break;	// high: safe
			}
	}
	// distance to next obstacle grows with speed
	gap16 = (50 + (speed16 >> 4) * 10 + (rnd() % 60)) * 16;
}

// move everything one frame, return 1 when dino hits an obstacle
uint8_t update_and_draw(uint8_t jump_key, uint8_t duck_key, uint8_t running)
{
	uint8_t i, hit = 0, legs;
	int16_t x, dy, gx, v;
	char TEXT[20];

	memset(screen, 0, sizeof(screen));

	if (running)
	{
		// ---- dino physics ----
		if (jump_key && dino_y16 == 0 && !duck_key)
			dino_v16 = JUMP_V;
		if (dino_y16 > 0 || dino_v16 > 0)
		{
			dino_y16 += dino_v16;
			dino_v16 -= GRAVITY;
			if (duck_key) dino_v16 -= FAST_DROP;	// fast drop
			if (dino_y16 <= 0) { dino_y16 = 0; dino_v16 = 0; }
		}
		ducking = (duck_key && dino_y16 == 0);

		// ---- move obstacles ----
		for (i = 0; i < MAX_OBS; i++)
			if (obs[i].active)
			{
				obs[i].x16 -= speed16;
				if ((obs[i].x16 >> 4) < -20) obs[i].active = 0;
			}
		gap16 -= speed16;
		if (gap16 <= 0) spawn_obstacle();

		// ---- clouds and ground scroll ----
		for (i = 0; i < 2; i++)
		{
			cloud_x16[i] -= speed16 >> 2;
			if ((cloud_x16[i] >> 4) < -14)
			{
				cloud_x16[i] = (130 + rnd() % 40) * 16;
				cloud_y[i] = 6 + rnd() % 20;
			}
		}
		ground_off += speed16;

		// ---- score and speed ----
		frame++;
		if ((frame % 3) == 0) score++;
		speed16 = SPEED_START + score / SPEED_STEP;
		if (speed16 > SPEED_MAX) speed16 = SPEED_MAX;
	}

	// ---- 1. draw obstacles first ----
	for (i = 0; i < MAX_OBS; i++)
		if (obs[i].active)
		{
			x = obs[i].x16 >> 4;
			switch (obs[i].type)
			{
				case 0: draw_sprite(x, obs[i].y, cactus_small, 15); break;
				case 1: draw_sprite(x, obs[i].y, cactus_big, 20); break;
				case 2: draw_sprite(x, obs[i].y, cactus_small, 15);
					draw_sprite(x + 10, obs[i].y, cactus_small, 15); break;
				default: draw_sprite(x, obs[i].y, bird[(frame >> 3) & 1], 10);
			}
		}

	// ---- 2. draw dino, any overlap with obstacle pixels = hit ----
	dy = dino_y16 >> 4;
	if (ducking)
		hit |= draw_sprite(DINO_X, GROUND_Y - 9, dino_duck[(frame >> 2) & 1], 9);
	else
	{
		legs = (!running || dy > 0) ? 0 : 1 + ((frame >> 2) & 1);
		hit |= draw_sprite(DINO_X, GROUND_Y - 16 - dy, dino_body, 13);
		hit |= draw_sprite(DINO_X, GROUND_Y - 3 - dy, dino_legs[legs], 3);
	}

	// ---- 3. draw ground, clouds, score (not part of collision) ----
	gx = (ground_off >> 4) % 29;
	for (x = 0; x < 128; x++)
	{
		set_pixel(x, GROUND_Y);
		v = (x + gx) % 29;
		if (v == 3)  set_pixel(x, GROUND_Y + 2);
		if (v == 11) { set_pixel(x, GROUND_Y + 1); set_pixel(x + 1, GROUND_Y + 1); }
		if (v == 20) set_pixel(x, GROUND_Y + 3);
	}
	for (i = 0; i < 2; i++)
		draw_sprite(cloud_x16[i] >> 4, cloud_y[i], cloud, 4);

	if (hi_score > 0) sprintf(TEXT, "HI %05u %05u", hi_score, score);
	else              sprintf(TEXT, "%05u", score);
	draw_text(127 - strlen(TEXT) * 6, 1, TEXT);

	return hit;
}

// ---------------- main ----------------
int32_t main (void)
{
	uint8_t key, last_key = 0;
	uint8_t jump_key, duck_key, new_press;
	uint8_t state = STATE_START;
	uint8_t wait = 0;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	init_LCD();
	clear_LCD();
	OpenKeyPad();
	reset_game();

	while (1)
	{
		key = ScanKey();
		jump_key  = (key >= 1 && key <= 6);
		duck_key  = (key >= 7 && key <= 9);
		new_press = (key != 0 && last_key == 0);
		last_key  = key;

		switch (state)
		{
			case STATE_START:
				rnd();				// mix random seed while waiting
				update_and_draw(0, 0, 0);
				draw_text_box(10, "DINO RUN");
				draw_text_box(22, "2=JUMP  8=DUCK");
				draw_text_box(34, "PRESS 2 TO START");
				if (new_press && jump_key)
				{
					seed += frame + key;
					reset_game();
					state = STATE_PLAY;
				}
				frame++;
				break;

			case STATE_PLAY:
				if (update_and_draw(jump_key, duck_key, 1))
				{
					if (score > hi_score) hi_score = score;
					state = STATE_OVER;
					wait = 20;		// ignore keys for a short time
				}
				break;

			case STATE_OVER:
				update_and_draw(0, 0, 0);	// freeze last frame
				draw_text_box(22, "GAME OVER");
				draw_text_box(34, "PRESS ANY KEY");
				if (wait > 0) wait--;
				else if (new_press)
				{
					reset_game();
					state = STATE_PLAY;
				}
				break;
		}

		draw_LCD(screen);		// send whole frame to LCD
		DrvSYS_Delay(FRAME_US);
	}
}
