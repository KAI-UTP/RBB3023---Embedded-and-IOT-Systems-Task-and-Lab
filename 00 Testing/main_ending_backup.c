//
// Smpl_Ending : modern ending animation on Nu-LB-NUC140 LCD (~30 s, loops)
//               idea from SCAIL-2 (zai-org) "controlled character animation"
//
//   S1. power on  : dot -> line -> screen opens, spinning 3D cube, loading bar
//   S2. title     : "SCAIL-2" letters slide in, flash, ticker bar, blinds close
//   S3. motion    : pose skeleton (with motion trail) drives a solid figure
//   S4. fireworks : night sky, fireworks, "THANK YOU", pixel dissolve
//   S5. credits   : sliding credit bars, "THE END" drops in, TV switch off
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

#define FRAME_US	36000		// delay per frame (us), about 25 frames/s
#define GROUND_Y	52		// ground row for the dancing figures

// timeline (frame numbers)
#define S_TITLE		90		// S2 starts
#define S_MOTION	240		// S3 starts
#define S_FIRE		420		// S4 starts
#define S_CREDIT	600		// S5 starts
#define S_LEN		750		// whole animation, then loop

unsigned char screen[128 * 8];	// LCD frame buffer, 8 pages x 128 columns
uint8_t ink = 1;		// 1 = draw dark pixels, 0 = draw light pixels

// sin x 127 for a quarter turn (16 steps)
const uint8_t SIN_Q[17] = { 0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127 };

// 3D cube corners and edges
const int8_t  CUBE[8][3]  = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1} };
const uint8_t EDGE[12][2] = { {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7} };

// fireworks : start frame, x, burst height
const uint8_t FW[10][3] = {
	{0, 30, 18}, {10, 96, 14}, {24, 64, 10}, {36, 18, 24}, {48, 108, 22},
	{62, 46, 12}, {74, 84, 16}, {88, 24, 14}, {100, 104, 12}, {112, 64, 14}
};

char *TICKER = "CONTROLLED CHARACTER ANIMATION  *  SCAIL-2  *  ZAI-ORG";

char *CREDIT[3][2] = {
	{ "IDEA",     "SCAIL-2" },
	{ "MODEL BY", "ZAI-ORG" },
	{ "BOARD",    "NU-LB-NUC140" },
};

// one body pose : [0] = right side limbs, [1] = left side limbs
typedef struct {
	int16_t nx, ny;			// neck
	int16_t px, py;			// hip
	int16_t ex[2], ey[2];		// elbows
	int16_t wx[2], wy[2];		// hands
	int16_t kx[2], ky[2];		// knees
	int16_t fx[2], fy[2];		// feet
} POSE;

// ---------------- math helpers ----------------
// sin x 127, a = 0~63 (64 steps per turn)
int16_t isin(uint8_t a)
{
	a &= 63;
	if (a < 16) return  SIN_Q[a];
	if (a < 32) return  SIN_Q[32 - a];
	if (a < 48) return -SIN_Q[a - 32];
	return -SIN_Q[64 - a];
}

int16_t icos(uint8_t a)
{
	return isin(a + 16);
}

// ease-out : goes 0 -> d while u goes 0 -> n (fast start, soft stop)
int16_t ease(int16_t u, int16_t n, int16_t d)
{
	if (u <= 0) return 0;
	if (u >= n) return d;
	return (int32_t)d * (n * n - (n - u) * (n - u)) / (n * n);
}

// pseudo random number 0~255 from two numbers (same input = same output)
uint8_t hash8(uint16_t x, uint16_t y)
{
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return (h >> 16) & 0xFF;
}

// ---------------- drawing helpers (all use ink) ----------------
void plot(int16_t x, int16_t y)
{
	if (x < 0 || x > 127 || y < 0 || y > 63) return;
	if (ink) screen[x + (y >> 3) * 128] |=  (1 << (y & 7));
	else     screen[x + (y >> 3) * 128] &= ~(1 << (y & 7));
}

void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i, j;
	for (j = y; j < y + h; j++)
		for (i = x; i < x + w; i++)
			plot(i, j);
}

void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	fill_rect(x, y, w, 1);
	fill_rect(x, y + h - 1, w, 1);
	fill_rect(x, y, 1, h);
	fill_rect(x + w - 1, y, 1, h);
}

// 3x3 dot
void draw_dot(int16_t x, int16_t y)
{
	fill_rect(x - 1, y - 1, 3, 3);
}

// Bresenham line, thick = 1 draws 3 px wide
void draw_line_w(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thick)
{
	int16_t dx = (x1 > x0) ? x1 - x0 : x0 - x1;
	int16_t dy = (y1 > y0) ? y0 - y1 : y1 - y0;
	int16_t sx = (x0 < x1) ? 1 : -1;
	int16_t sy = (y0 < y1) ? 1 : -1;
	int16_t err = dx + dy, e2;

	while (1)
	{
		if (thick) draw_dot(x0, y0);
		else       plot(x0, y0);
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	draw_line_w(x0, y0, x1, y1, 0);
}

void draw_circle(int16_t cx, int16_t cy, int16_t r)
{
	int16_t x, y, d;
	for (y = -r; y <= r; y++)
		for (x = -r; x <= r; x++)
		{
			d = x * x + y * y;
			if (d <= r * r && d > (r - 1) * (r - 1)) plot(cx + x, cy + y);
		}
}

void draw_disc(int16_t cx, int16_t cy, int16_t r)
{
	int16_t x, y;
	for (y = -r; y <= r; y++)
		for (x = -r; x <= r; x++)
			if (x * x + y * y <= r * r) plot(cx + x, cy + y);
}

// draw text with 5x7 font (6 px per char)
void draw_text(int16_t x, int16_t y, char *s)
{
	uint8_t c, b, col;
	while (*s)
	{
		for (c = 0; c < 5; c++)
		{
			col = Font5x7[(*s - 0x20) * 5 + c];
			for (b = 0; b < 7; b++)
				if ((col >> b) & 1) plot(x + c, y + b);
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
				if ((col >> b) & 1) fill_rect(x + c * 2, y + b * 2, 2, 2);
		}
		x += 12;
		s++;
	}
}

// make everything outside the window (x0,y0)-(x1,y1) dark
void mask_outside(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	ink = 1;
	fill_rect(0, 0, 128, y0);				// above
	fill_rect(0, y1 + 1, 128, 63 - y1);			// below
	fill_rect(0, y0, x0, y1 - y0 + 1);			// left
	fill_rect(x1 + 1, y0, 127 - x1, y1 - y0 + 1);		// right
}

void invert_screen(void)
{
	uint16_t i;
	for (i = 0; i < sizeof(screen); i++) screen[i] ^= 0xFF;
}

// window blinds : k = 0 (open) ~ 8 (closed dark)
void draw_blinds(int16_t k)
{
	int16_t s;
	ink = 1;
	for (s = 0; s < 8; s++) fill_rect(0, s * 8, 128, k);
}

// corner brackets like a camera viewfinder
void draw_brackets(void)
{
	draw_line(0, 0, 6, 0);       draw_line(0, 0, 0, 6);
	draw_line(121, 0, 127, 0);   draw_line(127, 0, 127, 6);
	draw_line(0, 63, 6, 63);     draw_line(0, 57, 0, 63);
	draw_line(121, 63, 127, 63); draw_line(127, 57, 127, 63);
}

// spinning wireframe cube with perspective, a / b = turn angles (0~63)
void draw_cube(int16_t cx, int16_t cy, int16_t size, uint8_t a, uint8_t b, uint8_t dots)
{
	int16_t px[8], py[8], i;
	int32_t x, y, z, t;

	for (i = 0; i < 8; i++)
	{
		x = CUBE[i][0] * size;
		y = CUBE[i][1] * size;
		z = CUBE[i][2] * size;
		t = (x * icos(a) - z * isin(a)) / 127;		// turn around Y axis
		z = (x * isin(a) + z * icos(a)) / 127;
		x = t;
		t = (y * icos(b) - z * isin(b)) / 127;		// turn around X axis
		z = (y * isin(b) + z * icos(b)) / 127;
		y = t;
		px[i] = cx + x * 64 / (z + 64);			// perspective
		py[i] = cy + y * 64 / (z + 64);
	}
	for (i = 0; i < 12; i++)
		draw_line(px[EDGE[i][0]], py[EDGE[i][0]], px[EDGE[i][1]], py[EDGE[i][1]]);
	if (dots)
		for (i = 0; i < 8; i++) draw_dot(px[i], py[i]);
}

// ---------------- dancing pose ----------------
// point at distance len from (x, y) in direction d (16 steps : 0 = right, 4 = down, 8 = left, 12 = up)
void move_pt(int16_t x, int16_t y, uint8_t d, int16_t len, int16_t *ox, int16_t *oy)
{
	*ox = x + icos(d * 4) * len / 127;
	*oy = y + isin(d * 4) * len / 127;
}

// dance pose at frame t, body centred at column cx
// 4 moves, 48 frames each : wave, squat, sway, jump
void get_pose(int16_t cx, uint16_t t, POSE *p)
{
	uint8_t move = (t / 48) & 3;
	int16_t b = isin((t & 15) * 4) / 2;		// beat, -63 ~ 63
	uint8_t ar0 = 3, ar1 = 3, al0 = 5, al1 = 5;	// arm angles : upper, fore (right / left)
	uint8_t lr0 = 3, lr1 = 4, ll0 = 5, ll1 = 4;	// leg angles : thigh, shin (right / left)
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

// body, arms and legs, thick = 1 for solid figure
void draw_limbs(POSE *p, uint8_t thick)
{
	uint8_t i;
	draw_line_w(p->nx, p->ny, p->px, p->py, thick);
	for (i = 0; i < 2; i++)
	{
		draw_line_w(p->nx, p->ny + 2, p->ex[i], p->ey[i], thick);
		draw_line_w(p->ex[i], p->ey[i], p->wx[i], p->wy[i], thick);
		draw_line_w(p->px, p->py, p->kx[i], p->ky[i], thick);
		draw_line_w(p->kx[i], p->ky[i], p->fx[i], p->fy[i], thick);
	}
}

// pose skeleton : bones + joint dots + small head (the driving motion)
void draw_skeleton(POSE *p)
{
	uint8_t i;
	draw_limbs(p, 0);
	draw_dot(p->nx, p->ny);
	draw_dot(p->px, p->py);
	for (i = 0; i < 2; i++)
	{
		draw_dot(p->ex[i], p->ey[i]);
		draw_dot(p->wx[i], p->wy[i]);
		draw_dot(p->kx[i], p->ky[i]);
		draw_dot(p->fx[i], p->fy[i]);
	}
	draw_circle(p->nx, p->ny - 5, 4);
	plot(p->nx, p->ny - 5);
}

// faded copy of the joints (motion trail)
void draw_trail(POSE *p)
{
	uint8_t i;
	plot(p->nx, p->ny - 5);
	for (i = 0; i < 2; i++)
	{
		plot(p->wx[i], p->wy[i]);
		plot(p->fx[i], p->fy[i]);
		plot(p->ex[i], p->ey[i]);
		plot(p->kx[i], p->ky[i]);
	}
}

// ================= scenes =================

// S1. power on : dot -> line -> screen, cube + loading bar
void scene_power_on(uint16_t u)
{
	int16_t w, h, pct;
	char buf[16];

	draw_brackets();
	draw_cube(64, 24, 9, u, u / 2 + 8, 1);
	draw_cube(64, 24, 4, 64 - (u & 63), u, 0);	// small cube turns the other way

	pct = ease(u - 30, 55, 100);
	sprintf(buf, "LOADING %3d%%", pct);
	draw_text_at(64, 47, buf);
	draw_rect(24, 56, 80, 6);
	fill_rect(26, 58, pct * 76 / 100, 2);

	if (u < 10)					// dark, blinking dot
	{
		memset(screen, 0xFF, sizeof(screen));
		ink = 0;
		if (u & 2) fill_rect(63, 31, 2, 2);
	}
	else if (u < 25)				// dot stretches into a line
	{
		w = ease(u - 10, 15, 128);
		if (w < 2) w = 2;
		mask_outside(64 - w / 2, 31, 64 - w / 2 + w - 1, 32);
	}
	else if (u < 40)				// line opens into the screen
	{
		h = ease(u - 25, 15, 64);
		if (h < 2) h = 2;
		mask_outside(0, 32 - h / 2, 127, 32 - h / 2 + h - 1);
	}
	ink = 1;
}

// S2. kinetic title
void scene_title(uint16_t u)
{
	char   *word = "SCAIL-2";
	char   *top  = "- THE END CREDITS -";
	char    c[2] = { 0, 0 };
	char    buf[20];
	int16_t i, k, n, w, h;

	// top line types on with a cursor
	if (u > 20)
	{
		n = (u - 20) / 2;
		if (n > 19) n = 19;
		memcpy(buf, top, n);
		buf[n] = '\0';
		draw_text(7, 3, buf);
		if (n < 19 && (u & 4)) fill_rect(7 + n * 6, 3, 5, 7);
	}

	// big letters slide in from the right, one after another
	for (i = 0; i < 7; i++)
	{
		k = u - i * 4;
		if (k < 0) continue;
		c[0] = word[i];
		draw_text2(22 + i * 12 + 128 - ease(k, 14, 128), 18, c);
	}

	// underline grows from the centre
	if (u > 36)
	{
		w = ease(u - 36, 16, 42);
		draw_line(64 - w, 35, 63 + w, 35);
	}

	// ticker bar rises from the bottom, text scrolls in light ink
	if (u > 56)
	{
		h = ease(u - 56, 8, 11);
		fill_rect(0, 64 - h, 128, h);
		if (u > 64)
		{
			ink = 0;
			draw_text(128 - (u - 64) * 2, 55, TICKER);
			ink = 1;
		}
	}

	if (u == 50 || u == 51) invert_screen();	// flash

	if (u >= 126) draw_blinds((u - 126) / 3 + 1);	// blinds close
}

// S3. pose skeleton drives a solid figure
void scene_motion(uint16_t u)
{
	POSE    pose;
	int16_t k, x;

	// moving dashed floor
	for (x = 0; x < 128; x++)
		if (((x + u) & 3) < 2) plot(x, GROUND_Y);

	// motion trail of the skeleton
	for (k = 3; k >= 1; k--)
		if (u >= k * 3)
		{
			get_pose(30, u - k * 3, &pose);
			draw_trail(&pose);
		}

	get_pose(30, u, &pose);			// input : pose skeleton
	draw_skeleton(&pose);

	get_pose(98, u, &pose);			// output : solid figure with same pose
	draw_limbs(&pose, 1);
	draw_disc(pose.nx, pose.ny - 7, 6);

	// centre loader (8 dots, one big dot runs around)
	for (k = 0; k < 8; k++)
	{
		x = 64 + icos(k * 8) * 6 / 127;
		if (k == ((u / 3) & 7)) draw_dot(x, 28 + isin(k * 8) * 6 / 127);
		else                    plot(x, 28 + isin(k * 8) * 6 / 127);
	}
	draw_text_at(64, 38, "AI");

	draw_text_at(30, 2, "INPUT");
	draw_text_at(98, 2, "OUTPUT");
	draw_text_at(64, 56, "MOTION TRANSFER");

	if (u < 24) draw_blinds(8 - u / 3);		// blinds open

	if (u >= 156)					// dark circle grows to cover all
	{
		ink = 1;
		draw_disc(64, 32, ease(u - 156, 24, 75));
	}
}

// one firework : rocket goes up, then bursts into 2 rings that fall and sparkle
void draw_firework(int16_t x, int16_t by, int16_t f)
{
	int16_t y, g, r, drop, k;
	uint8_t a;

	if (f < 0) return;
	if (f < 16)					// rocket
	{
		y = 63 - ease(f, 16, 63 - by);
		plot(x, y);
		plot(x, y + 1);
		plot(x, y + 3 + (f & 1) * 2);		// flickering trail
		return;
	}

	g = f - 16;
	if (g >= 44) return;
	if (g < 3) draw_disc(x, by, 4 - g);		// flash
	r    = ease(g, 30, 18);
	drop = g * g / 120;				// gravity
	for (k = 0; k < 16; k++)
	{
		a = k * 4;
		if (g < 6)
			draw_dot(x + icos(a) * r / 127, by + isin(a) * r / 127 + drop);
		else if (g < 30)
		{
			plot(x + icos(a) * r / 127, by + isin(a) * r / 127 + drop);
			plot(x + icos(a + 2) * r * 3 / 5 / 127, by + isin(a + 2) * r * 3 / 5 / 127 + drop);
		}
		else if ((g + k) & 1)			// sparkle out
			plot(x + icos(a) * r / 127, by + isin(a) * r / 127 + drop);
	}
}

// S4. night sky fireworks
void scene_fireworks(uint16_t u)
{
	int16_t i, x, y, w;
	char    c[2] = { 0, 0 };
	char   *msg = "THANK YOU";

	memset(screen, 0xFF, sizeof(screen));		// night sky
	ink = 0;					// light ink

	for (i = 0; i < 20; i++)			// twinkling stars
		if (((u + i * 5) % 30) < 24)
			plot(hash8(i, 1) & 127, hash8(i, 2) % 44);

	for (i = 0; i < 10; i++)
		draw_firework(FW[i][1], FW[i][2], u - FW[i][0]);

	if (u >= 56)					// message box opens from the centre
	{
		w = ease(u - 56, 10, 116);
		ink = 1;
		fill_rect(64 - w / 2, 22, w, 34);
		ink = 0;
		draw_rect(64 - w / 2, 22, w, 34);
	}
	for (i = 0; i < 9; i++)				// letters pop in
		if (u >= 66 + i * 3)
		{
			c[0] = msg[i];
			draw_text2(10 + i * 12, 27, c);
		}
	if (u >= 100) draw_text_at(64, 45, "FOR WATCHING");

	if (u >= 150)					// pixel dissolve to light
		for (y = 0; y < 64; y++)
			for (x = 0; x < 128; x++)
				if (hash8(x, y) % 30 < u - 150) plot(x, y);
	ink = 1;
}

// S5. credit bars, THE END, TV switch off
void scene_credits(uint16_t u)
{
	int16_t r, k, bx, by, i, w, h, v;
	char    c[2] = { 0, 0 };
	char   *end = "THE END";

	// three bars slide in (right, left, right), then slide out
	for (r = 0; r < 3; r++)
	{
		k = u - r * 8;
		if (k < 0) continue;
		by = 6 + r * 16;
		bx = 128 - ease(k, 14, 128);			// distance still to travel
		bx += ease(u - 74 - r * 4, 12, 128);		// slide out
		if (r == 1) bx = -bx;				// middle bar comes from the left
		if (bx > 127 || bx < -127) continue;
		ink = 1;
		fill_rect(bx, by, 128, 12);
		ink = 0;
		draw_text(bx + 4, by + 3, CREDIT[r][0]);
		draw_text(bx + 124 - (int16_t)strlen(CREDIT[r][1]) * 6, by + 3, CREDIT[r][1]);
		ink = 1;
	}

	// THE END letters drop in
	for (i = 0; i < 7; i++)
	{
		k = u - 96 - i * 2;
		if (k < 0) continue;
		c[0] = end[i];
		draw_text2(22 + i * 12, -14 + ease(k, 10, 38), c);
	}
	if (u >= 112)
	{
		w = ease(u - 112, 10, 42);
		draw_line(64 - w, 41, 63 + w, 41);
	}
	if (u >= 116) draw_text_at(64, 47, "SEE YOU NEXT TIME");

	// old TV switch off : screen squeezes to a line, line to a dot, then dark
	if (u >= 126)
	{
		v = u - 126;
		if (v < 10)
		{
			h = 64 - ease(v, 10, 62);
			mask_outside(0, 32 - h / 2, 127, 32 - h / 2 + h - 1);
		}
		else if (v < 18)
		{
			w = 128 - ease(v - 10, 8, 126);
			mask_outside(64 - w / 2, 31, 64 - w / 2 + w - 1, 32);
		}
		else
			memset(screen, 0xFF, sizeof(screen));
	}
	ink = 1;
}

// ---------------- main ----------------
int32_t main (void)
{
	uint16_t t = 0;			// frame number

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	init_LCD();
	clear_LCD();

	while (1)
	{
		memset(screen, 0, sizeof(screen));
		ink = 1;

		if      (t < S_TITLE)  scene_power_on(t);
		else if (t < S_MOTION) scene_title(t - S_TITLE);
		else if (t < S_FIRE)   scene_motion(t - S_MOTION);
		else if (t < S_CREDIT) scene_fireworks(t - S_FIRE);
		else                   scene_credits(t - S_CREDIT);

		draw_LCD(screen);		// send whole frame to LCD
		DrvSYS_Delay(FRAME_US);

		if (++t >= S_LEN) t = 0;
	}
}