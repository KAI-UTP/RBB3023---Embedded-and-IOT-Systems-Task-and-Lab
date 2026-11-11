//
// Smpl_Cartoon : simple cartoon animation on Nu-LB-NUC140 LCD
//
//   Story (about 15 s):
//     1. Bobo walks in from the left
//     2. stops, waves and says "HI!"
//     3. jumps two times
//     4. a heart floats up
//     5. walks out to the right
//   Background : spinning sun, moving clouds, grass
//
//   Ending (about 30 s), idea from SCAIL-2 (zai-org) "controlled character animation":
//     a pose skeleton dances and the character copies its motion
//     E1. title card "SCAIL-2"
//     E2. COPY : skeleton dances, Bobo copies the same pose
//     E3. SWAP : same dance, character swaps Bobo -> Kitty -> Robo
//     E4. credits scroll up
//     E5. "THE END", Bobo waves, iris closes
//   Then the story starts again.
//
//   LCD    : 128x64 via SPI3, whole frame drawn in screen[] then draw_LCD()
//   7-seg  : turned off
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Font5x7.h"
#include "Seven_Segment.h"

#define FRAME_US	40000		// delay per frame (us), about 25 frames/s
#define GROUND_Y	56		// ground line row

// story timeline (frame numbers)
#define T_WAVE		80		// stop walking, wave
#define T_JUMP		160		// jump two times
#define T_HEART		220		// heart floats up
#define T_OUT		280		// walk out to the right
#define T_END		360		// story finished, ending starts

// ending timeline (frame numbers counted from start of ending)
#define E_COPY		100		// skeleton -> Bobo motion copy
#define E_SWAP		300		// skeleton -> swapping characters
#define E_CREDIT	500		// credits scroll
#define E_THEEND	660		// THE END
#define E_IRIS		700		// iris starts closing
#define E_LEN		750		// ending length (~30 s), then story restarts

#define CHAR_BOBO	0
#define CHAR_KITTY	1
#define CHAR_ROBO	2

// sin x 64 for 16 angles (22.5 deg each), cos(a) = SIN16[(a + 4) & 15]
const int8_t SIN16[16] = { 0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59, -64, -59, -45, -24 };

// ---------------- sprites ('#' = pixel on) ----------------
const char *cloud[4] = {
	".....####....",
	"..###....##..",
	".#.........#.",
	"#############",
};
const char *heart[6] = {
	".##.##.",
	"#######",
	"#######",
	".#####.",
	"..###..",
	"...#...",
};

// ---------------- ending data ----------------
// one body pose : [0] = right side limbs, [1] = left side limbs
typedef struct {
	int16_t nx, ny;			// neck
	int16_t px, py;			// hip
	int16_t ex[2], ey[2];		// elbows
	int16_t wx[2], wy[2];		// hands
	int16_t kx[2], ky[2];		// knees
	int16_t fx[2], fy[2];		// feet
} POSE;

char *char_name[3] = { "BOBO", "KITTY", "ROBO" };

char *credits[12] = {
	"BOBO'S DAY", "", "STARRING", "BOBO KITTY ROBO", "",
	"MOTION IDEA", "SCAIL-2 ZAI-ORG", "",
	"MADE ON", "NU-LB-NUC140", "", "THANK YOU!"
};

// star positions for the title card
const uint8_t star_xy[10][2] = {
	{6,8}, {120,6}, {14,52}, {112,50}, {30,58}, {98,60}, {4,30}, {122,30}, {60,58}, {64,48}
};

unsigned char screen[128 * 8];	// LCD frame buffer, 8 pages x 128 columns

// ---------------- drawing helpers ----------------
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

// Bresenham line
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	int16_t dx = (x1 > x0) ? x1 - x0 : x0 - x1;
	int16_t dy = (y1 > y0) ? y0 - y1 : y1 - y0;
	int16_t sx = (x0 < x1) ? 1 : -1;
	int16_t sy = (y0 < y1) ? 1 : -1;
	int16_t err = dx + dy, e2;

	while (1)
	{
		set_pixel(x0, y0);
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

// circle outline, fill = 1 clears the inside first (so it hides what is behind)
void draw_circle(int16_t cx, int16_t cy, int16_t r, uint8_t fill)
{
	int16_t x, y, d;
	for (y = -r; y <= r; y++)
		for (x = -r; x <= r; x++)
		{
			d = x * x + y * y;
			if (d <= r * r && d > (r - 1) * (r - 1)) set_pixel(cx + x, cy + y);
			else if (fill && d <= r * r)            clr_pixel(cx + x, cy + y);
		}
}

// rectangle outline, fill = 1 clears the inside
void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t fill)
{
	int16_t i, j;
	if (fill)
		for (j = y + 1; j < y + h - 1; j++)
			for (i = x + 1; i < x + w - 1; i++)
				clr_pixel(i, j);
	draw_line(x, y, x + w - 1, y);
	draw_line(x, y + h - 1, x + w - 1, y + h - 1);
	draw_line(x, y, x, y + h - 1);
	draw_line(x + w - 1, y, x + w - 1, y + h - 1);
}

// 3x3 dot
void draw_dot(int16_t x, int16_t y)
{
	int16_t i, j;
	for (j = -1; j <= 1; j++)
		for (i = -1; i <= 1; i++)
			set_pixel(x + i, y + j);
}

void draw_sprite(int16_t x, int16_t y, const char *rows[], uint8_t h)
{
	uint8_t r, c, w;
	for (r = 0; r < h; r++)
	{
		w = strlen(rows[r]);
		for (c = 0; c < w; c++)
			if (rows[r][c] == '#') set_pixel(x + c, y + r);
	}
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

// draw text centred on column cx
void draw_text_at(int16_t cx, int16_t y, char *s)
{
	draw_text(cx - (int16_t)strlen(s) * 3, y, s);
}

// draw text with 5x7 font scaled x2 (12 px per char)
void draw_text2(int16_t x, int16_t y, char *s)
{
	uint8_t c, b, col;
	while (*s)
	{
		for (c = 0; c < 5; c++)
		{
			col = Font5x7[(*s - 0x20) * 5 + c];
			for (b = 0; b < 7; b++)
				if ((col >> b) & 1)
				{
					set_pixel(x + c * 2,     y + b * 2);
					set_pixel(x + c * 2 + 1, y + b * 2);
					set_pixel(x + c * 2,     y + b * 2 + 1);
					set_pixel(x + c * 2 + 1, y + b * 2 + 1);
				}
		}
		x += 12;
		s++;
	}
}

// ---------------- scene parts ----------------
void draw_sun(uint16_t t)
{
	uint8_t k, a;
	draw_circle(14, 11, 5, 0);
	for (k = 0; k < 8; k++)				// 8 rays, slowly spinning
	{
		a = (k * 2 + (t >> 2)) & 15;
		draw_line(14 + SIN16[(a + 4) & 15] * 7 / 64, 11 + SIN16[a] * 7 / 64,
		          14 + SIN16[(a + 4) & 15] * 10 / 64, 11 + SIN16[a] * 10 / 64);
	}
}

void draw_ground(void)
{
	int16_t x;
	for (x = 0; x < 128; x++)
	{
		set_pixel(x, GROUND_Y);
		if ((x % 9) == 2) { set_pixel(x, GROUND_Y - 1); set_pixel(x + 1, GROUND_Y - 2); }	// grass
		if ((x % 13) == 5) set_pixel(x, GROUND_Y + 3);						// stones
	}
}

// speech bubble with tail pointing down-left
void draw_bubble(int16_t x, int16_t y, char *s)
{
	int16_t w = strlen(s) * 6 + 5;
	draw_line(x + 1, y, x + w - 1, y);			// top
	draw_line(x + 1, y + 10, x + w - 1, y + 10);		// bottom
	draw_line(x, y + 1, x, y + 9);				// left
	draw_line(x + w, y + 1, x + w, y + 9);			// right
	draw_line(x + 2, y + 10, x - 2, y + 14);		// tail
	draw_line(x + 6, y + 10, x - 2, y + 14);
	draw_text(x + 3, y + 2, s);
}

// Bobo's face on a head centred at (x, hy)
void draw_face(int16_t x, int16_t hy, uint8_t blink, uint8_t happy)
{
	int16_t i;

	// eyes
	if (blink)
	{
		draw_line(x - 4, hy - 2, x - 2, hy - 2);
		draw_line(x + 2, hy - 2, x + 4, hy - 2);
	}
	else
	{
		set_pixel(x - 3, hy - 3); set_pixel(x - 2, hy - 3);
		set_pixel(x - 3, hy - 2); set_pixel(x - 2, hy - 2);
		set_pixel(x + 2, hy - 3); set_pixel(x + 3, hy - 3);
		set_pixel(x + 2, hy - 2); set_pixel(x + 3, hy - 2);
	}

	// mouth
	if (happy)					// open "D" smile
	{
		draw_line(x - 4, hy + 1, x + 4, hy + 1);
		draw_line(x - 3, hy + 2, x + 3, hy + 2);
		draw_line(x - 2, hy + 3, x + 2, hy + 3);
		for (i = -1; i <= 1; i++) clr_pixel(x + i, hy + 2);	// open mouth gap
	}
	else						// small smile
	{
		set_pixel(x - 3, hy + 1);
		set_pixel(x - 2, hy + 2);
		draw_line(x - 1, hy + 3, x + 1, hy + 3);
		set_pixel(x + 2, hy + 2);
		set_pixel(x + 3, hy + 1);
	}

	// cheeks
	set_pixel(x - 6, hy + 1);
	set_pixel(x + 6, hy + 1);
}

// Bobo : big round head, stick body
//   x     = body centre, lift = height above ground (jump)
//   step  = leg swing (-3~3), wave = 1 right arm waving
//   blink = 1 eyes closed, happy = 1 big open smile
void draw_bobo(int16_t x, int16_t lift, int8_t step, uint8_t wave, uint16_t t, uint8_t blink, uint8_t happy)
{
	int16_t hip  = GROUND_Y - 9 - lift;	// hip row
	int16_t neck = hip - 10;		// neck row
	int16_t hy   = neck - 8;		// head centre row

	// legs (feet tuck up while in the air)
	if (lift > 0)
	{
		draw_line(x, hip, x - 3, hip + 6);
		draw_line(x, hip, x + 3, hip + 6);
	}
	else
	{
		draw_line(x, hip, x + step, GROUND_Y - 1);
		draw_line(x, hip, x - step, GROUND_Y - 1);
		set_pixel(x + step + 1, GROUND_Y - 1);	// little feet
		set_pixel(x - step + 1, GROUND_Y - 1);
	}

	// body
	draw_line(x, neck, x, hip);

	// left arm swings opposite to legs
	draw_line(x, neck + 2, x - 5 - step, neck + 9);

	// right arm: wave above head, arms up when jumping, or swing
	if (wave)
	{
		draw_line(x, neck + 2, x + 8, neck + 1);			// upper arm
		draw_line(x + 8, neck + 1, x + 11 + SIN16[(t * 2) & 15] * 3 / 64, neck - 8);
	}
	else if (lift > 0)
	{
		draw_line(x, neck + 2, x + 8, neck + 1);
		draw_line(x + 8, neck + 1, x + 11, neck - 4);
	}
	else
		draw_line(x, neck + 2, x + 5 + step, neck + 9);

	// head (drawn last so it covers the arm/body behind it)
	draw_circle(x, hy, 8, 1);
	draw_face(x, hy, blink, happy);
}

// ---------------- ending : pose-driven characters ----------------
// point at distance len from (x, y) in direction a (0 = right, 4 = down, 8 = left, 12 = up)
void move_pt(int16_t x, int16_t y, uint8_t a, int16_t len, int16_t *ox, int16_t *oy)
{
	*ox = x + SIN16[(a + 4) & 15] * len / 64;
	*oy = y + SIN16[a & 15] * len / 64;
}

// dance pose at frame t, body centred at column cx
// 4 moves, 48 frames each : wave, squat, sway, jump
void get_pose(int16_t cx, uint16_t t, POSE *p)
{
	uint8_t move = (t / 48) & 3;
	int16_t b = SIN16[t & 15];			// beat, -64 ~ 64
	uint8_t ar0 = 3, ar1 = 3, al0 = 5, al1 = 5;	// arm angles  : upper, fore (right / left)
	uint8_t lr0 = 3, lr1 = 4, ll0 = 5, ll1 = 4;	// leg angles  : thigh, shin (right / left)
	int16_t lean = 0, lift = 0, u, dy, i;

	switch (move)
	{
		case 0:					// wave right hand, left hand on hip
			ar0 = 15; ar1 = 13 + (b + 64) / 43;
			al0 = 6;  al1 = 3;
			if (b > 24)  lr0 = 2;
			if (b < -24) ll0 = 6;
			break;
		case 1:					// squat + arm pump
			ar0 = 0; al0 = 8;
			if (b > 0) { ar1 = 0;  al1 = 8;  }
			else       { ar1 = 13; al1 = 11; }
			if (b > 32 || b < -32) { lr0 = 1; ll0 = 7; }
			break;
		case 2:					// sway with arms up in V
			lean = b * 3 / 64;
			ar0 = ar1 = 14;
			al0 = al1 = 10;
			break;
		default:				// jump
			u = t % 24;
			lift = u * (24 - u) / 12;	// max 12 px
			lr0 = 2; lr1 = 5; ll0 = 6; ll1 = 3;
			if (lift > 2) { ar0 = ar1 = 14; al0 = al1 = 10; }
			else          { ar0 = ar1 = 2;  al0 = al1 = 6;  }
	}

	p->px = cx;         p->py = 0;
	p->nx = cx + lean;  p->ny = -10;
	move_pt(p->nx, p->ny + 2, ar0, 7, &p->ex[0], &p->ey[0]);
	move_pt(p->ex[0], p->ey[0], ar1, 7, &p->wx[0], &p->wy[0]);
	move_pt(p->nx, p->ny + 2, al0, 7, &p->ex[1], &p->ey[1]);
	move_pt(p->ex[1], p->ey[1], al1, 7, &p->wx[1], &p->wy[1]);
	move_pt(p->px, p->py, lr0, 5, &p->kx[0], &p->ky[0]);
	move_pt(p->kx[0], p->ky[0], lr1, 5, &p->fx[0], &p->fy[0]);
	move_pt(p->px, p->py, ll0, 5, &p->kx[1], &p->ky[1]);
	move_pt(p->kx[1], p->ky[1], ll1, 5, &p->fx[1], &p->fy[1]);

	// move the whole body so the lowest foot stands on the ground (minus jump height)
	dy = GROUND_Y - 1 - lift - ((p->fy[0] > p->fy[1]) ? p->fy[0] : p->fy[1]);
	p->ny += dy;
	p->py += dy;
	for (i = 0; i < 2; i++)
	{
		p->ey[i] += dy; p->wy[i] += dy;
		p->ky[i] += dy; p->fy[i] += dy;
	}
}

// arms and legs (and body line if body = 1)
void draw_limbs(POSE *p, uint8_t body)
{
	uint8_t i;
	if (body) draw_line(p->nx, p->ny, p->px, p->py);
	for (i = 0; i < 2; i++)
	{
		draw_line(p->nx, p->ny + 2, p->ex[i], p->ey[i]);
		draw_line(p->ex[i], p->ey[i], p->wx[i], p->wy[i]);
		draw_line(p->px, p->py, p->kx[i], p->ky[i]);
		draw_line(p->kx[i], p->ky[i], p->fx[i], p->fy[i]);
	}
}

// pose skeleton : bones + joint dots + small head (the "driving" motion)
void draw_skeleton(POSE *p)
{
	uint8_t i;
	draw_limbs(p, 1);
	draw_dot(p->nx, p->ny);
	draw_dot(p->px, p->py);
	for (i = 0; i < 2; i++)
	{
		draw_dot(p->ex[i], p->ey[i]);
		draw_dot(p->wx[i], p->wy[i]);
		draw_dot(p->kx[i], p->ky[i]);
		draw_dot(p->fx[i], p->fy[i]);
	}
	draw_circle(p->nx, p->ny - 5, 4, 0);
	set_pixel(p->nx, p->ny - 5);			// nose joint
}

// character that follows the pose
void draw_character(POSE *p, uint8_t who, uint16_t t, uint8_t blink)
{
	int16_t hx = p->nx, hy, i;

	switch (who)
	{
		case CHAR_BOBO:
			draw_limbs(p, 1);
			hy = p->ny - 8;
			draw_circle(hx, hy, 8, 1);
			draw_face(hx, hy, blink, 1);
			break;

		case CHAR_KITTY:
			i = SIN16[t & 15] * 3 / 64;			// tail swing
			draw_line(p->px, p->py, p->px - 6, p->py - 2);
			draw_line(p->px - 6, p->py - 2, p->px - 8 + i, p->py - 9);
			draw_limbs(p, 1);
			hy = p->ny - 7;
			draw_circle(hx, hy, 7, 1);
			draw_line(hx - 6, hy - 3, hx - 5, hy - 11);	// ears
			draw_line(hx - 5, hy - 11, hx - 1, hy - 7);
			draw_line(hx + 6, hy - 3, hx + 5, hy - 11);
			draw_line(hx + 5, hy - 11, hx + 1, hy - 7);
			if (blink)
			{
				draw_line(hx - 4, hy - 1, hx - 2, hy - 1);
				draw_line(hx + 2, hy - 1, hx + 4, hy - 1);
			}
			else					// tall cat eyes
			{
				draw_line(hx - 3, hy - 2, hx - 3, hy);
				draw_line(hx + 3, hy - 2, hx + 3, hy);
			}
			set_pixel(hx, hy + 1);				// nose
			set_pixel(hx - 2, hy + 3);			// "w" mouth
			set_pixel(hx - 1, hy + 4);
			set_pixel(hx,     hy + 3);
			set_pixel(hx + 1, hy + 4);
			set_pixel(hx + 2, hy + 3);
			draw_line(hx - 4, hy + 1, hx - 10, hy);		// whiskers
			draw_line(hx - 4, hy + 2, hx - 10, hy + 3);
			draw_line(hx + 4, hy + 1, hx + 10, hy);
			draw_line(hx + 4, hy + 2, hx + 10, hy + 3);
			break;

		default:					// CHAR_ROBO
			draw_limbs(p, 0);
			i = (p->nx + p->px) / 2;			// box body
			draw_rect(i - 4, p->ny, 9, p->py - p->ny + 1, 1);
			if (t & 8) draw_dot(i, p->ny + 4);		// chest light
			hy = p->ny - 7;
			draw_line(hx, hy - 6, hx, hy - 9);		// antenna
			draw_circle(hx, hy - 11, 2, 1);
			if (t & 4) set_pixel(hx, hy - 11);
			draw_rect(hx - 7, hy - 6, 15, 13, 1);		// square head
			if (blink)
			{
				draw_line(hx - 5, hy - 2, hx - 2, hy - 2);
				draw_line(hx + 2, hy - 2, hx + 5, hy - 2);
			}
			else
			{
				draw_dot(hx - 3, hy - 2);
				draw_dot(hx + 3, hy - 2);
			}
			draw_line(hx - 3, hy + 3, hx + 3, hy + 3);	// mouth grill
			for (i = -3; i <= 3; i += 2) set_pixel(hx + i, hy + 2);
	}
}

// dashed arrow from skeleton to character, dashes flow to the right
void draw_arrow(uint16_t t, char *label)
{
	int16_t x;
	for (x = 50; x <= 76; x++)
		if ((((x - (int16_t)(t & 0xFF)) % 6) + 6) % 6 < 3) set_pixel(x, 36);
	draw_line(73, 33, 77, 36);
	draw_line(73, 39, 77, 36);
	draw_text_at(64, 26, label);
}

// cartoon "iris out" : fill everything outside a circle
void draw_iris(int16_t cx, int16_t cy, int16_t r)
{
	int16_t x, y;
	int32_t rr = (int32_t)r * r;
	for (y = 0; y < 64; y++)
		for (x = 0; x < 128; x++)
			if ((int32_t)(x - cx) * (x - cx) + (int32_t)(y - cy) * (y - cy) > rr)
				set_pixel(x, y);
}

// draw ending frame e (0 ~ E_LEN-1)
void draw_ending(uint16_t e, uint32_t frame)
{
	POSE    pose;
	int16_t u, i, y;
	uint8_t who, blink = ((frame % 60) < 3);
	char    buf[8];

	if (e < E_COPY)					// E1. title card
	{
		for (i = 0; i < 10; i++)		// twinkling stars
		{
			u = (e + i * 7) % 20;
			if (u < 12) set_pixel(star_xy[i][0], star_xy[i][1]);
			if (u < 6)
			{
				set_pixel(star_xy[i][0] - 1, star_xy[i][1]);
				set_pixel(star_xy[i][0] + 1, star_xy[i][1]);
				set_pixel(star_xy[i][0], star_xy[i][1] - 1);
				set_pixel(star_xy[i][0], star_xy[i][1] + 1);
			}
		}
		draw_text_at(64, 2, "ENDING INSPIRED BY");
		u = e / 6 + 1;				// type letters one by one
		if (u > 7) u = 7;
		memcpy(buf, "SCAIL-2", u);
		buf[u] = '\0';
		draw_text2(22, 14, buf);
		if (e > 42)				// underline grows
		{
			u = (e - 42) * 3;
			if (u > 84) u = 84;
			draw_line(22, 31, 22 + u - 1, 31);
		}
		if (e > 60) draw_text_at(64, 36, "CHARACTER ANIMATION");
	}
	else if (e < E_CREDIT)				// E2 / E3. motion copy and swap
	{
		u = e - E_COPY;
		get_pose(28, u, &pose);			// driving skeleton (left)
		draw_skeleton(&pose);
		get_pose(100, u, &pose);		// same pose for character (right)
		who = (e < E_SWAP) ? CHAR_BOBO : ((e - E_SWAP) / 67) % 3;
		draw_character(&pose, who, u, blink);

		draw_line(0, GROUND_Y, 127, GROUND_Y);
		draw_text_at(28, 1, "POSE");
		draw_text_at(100, 1, char_name[who]);
		draw_arrow(u, (e < E_SWAP) ? "COPY" : "SWAP");

		if (e >= E_SWAP)			// "poof" ring when character changes
		{
			u = (e - E_SWAP) % 67;
			if (u < 10) draw_circle(100, 34, 6 + u * 2, 0);
		}
	}
	else if (e < E_THEEND)				// E4. credits roll up
	{
		u = e - E_CREDIT;
		if (u > 146) u = 146;			// stop when last line reaches the middle
		for (i = 0; i < 12; i++)
		{
			y = 64 + i * 10 - u;
			if (y > -8 && y < 64) draw_text_at(64, y, credits[i]);
		}
	}
	else						// E5. THE END + iris close
	{
		draw_text2(22, 3, "THE END");
		draw_line(0, GROUND_Y, 127, GROUND_Y);
		draw_bobo(64, 0, 0, 1, e, blink, 1);
		if (e >= E_IRIS)
		{
			u = e - E_IRIS;			// 0 ~ 49
			draw_iris(64, 29, (u < 40) ? 80 - u * 2 : 0);
		}
	}
}

// ---------------- main ----------------
int32_t main (void)
{
	uint16_t t = 0;			// frame number inside story + ending
	uint32_t frame = 0;		// total frames (for background)
	int16_t  x = -15, lift, cloud_x = 140, u;
	int8_t   step;
	uint8_t  wave, happy, blink;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	init_LCD();
	clear_LCD();

	while (1)
	{
		memset(screen, 0, sizeof(screen));

		if (t < T_END)
		{
			// ---- background ----
			draw_sun(frame);
			if ((frame & 1) == 0) cloud_x--;
			if (cloud_x < -14) cloud_x = 130;
			draw_sprite(cloud_x, 6, cloud, 4);
			draw_sprite((cloud_x + 70) % 144 - 14, 18, cloud, 4);
			draw_ground();

			// ---- story ----
			step  = 0;
			wave  = 0;
			happy = 0;
			lift  = 0;
			blink = ((frame % 60) < 3);		// blink every ~2.4 s

			if (t < T_WAVE)				// 1. walk in
			{
				x = -15 + t * 4 / 5;
				step = SIN16[(t * 2) & 15] * 3 / 64;
			}
			else if (t < T_JUMP)			// 2. wave + say HI
			{
				wave = 1;
				if (t > T_WAVE + 10) draw_bubble(x + 14, 6, "HI!");
			}
			else if (t < T_HEART)			// 3. jump two times
			{
				u = (t - T_JUMP) % 30;
				lift = u * (30 - u) / 14;	// parabola, max 16 px
				happy = 1;
			}
			else if (t < T_OUT)			// 4. hearts float up
			{
				u = t - T_HEART;
				happy = 1;
				draw_sprite(x + 8, 18 - u / 3, heart, 6);
				if (u > 30) draw_sprite(x - 14, 24 - (u - 30) / 3, heart, 6);
			}
			else					// 5. walk out
			{
				x = 49 + (t - T_OUT) * 6 / 5;
				step = SIN16[(t * 2) & 15] * 3 / 64;
			}

			draw_bobo(x, lift, step, wave, t, blink, happy);

			// title at the start of each loop
			if (t < 40) draw_text(34, 1, "BOBO'S DAY");
		}
		else
			draw_ending(t - T_END, frame);

		draw_LCD(screen);		// send whole frame to LCD
		DrvSYS_Delay(FRAME_US);

		frame++;
		if (++t >= T_END + E_LEN) t = 0;
	}
}