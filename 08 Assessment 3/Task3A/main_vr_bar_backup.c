//
// Task 3A : the variable resistor VR1 controls the length of a bar on the LCD
//
//   VR1     : on-board variable resistor on GPA7 = ADC7
//             12-bit result 0 ~ 4095 = 0 ~ 3.3V
//   ADC     : single-end, single mode, clock 22MHz / (1 + 1) = 11MHz
//             16 readings are averaged so the bar does not jitter
//   LCD     : framed bar across the screen, length = ADC * 122 / 4095 pixels
//             scale marks at 0 / 25 / 50 / 75 / 100 %
//             bottom 2 lines show the ADC value, voltage and percent
//   Turn VR1 clockwise = longer bar, anticlockwise = shorter bar
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Font5x7.h"

#define SAMPLES		16	// ADC readings averaged for one value

#define BAR_X		2	// bar frame
#define BAR_Y		15
#define BAR_W		124
#define BAR_H		16
#define BAR_MAX		(BAR_W - 2)	// longest bar inside the frame

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

// ---------------- ADC ----------------

void InitADC(void)
{
	/* Step 1. GPIO initial */
	GPIOA->OFFD |= 0x00800000;		// disable GPA7 digital input path
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
	/* Step 2. ADC clock source, then enable ADC */
	SYSCLK->CLKSEL1.ADC_S = 2;		// 22MHz for ADC
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;		// enable ADC clock
	ADC->ADCR.ADEN = 1;			// enable ADC module
	/* Step 3. Operation mode */
	ADC->ADCR.DIFFEN = 0;			// single-end input
	ADC->ADCR.ADMD = 0;			// single mode
	/* Step 4. Channel */
	ADC->ADCHER.CHEN = 0x80;		// channel 7 only
	/* Step 5. Clear flag (polling, no interrupt) */
	ADC->ADSR.ADF = 1;
	/* Step 6. Start conversion */
	ADC->ADCR.ADST = 1;
}

// read ADC7 : wait for the conversion, take the result, start the next one
uint16_t read_adc7(void)
{
	uint16_t value;

	while (ADC->ADSR.ADF == 0);		// wait till conversion done
	ADC->ADSR.ADF = 1;			// write 1 to clear the flag
	value = ADC->ADDR[7].RSLT;
	ADC->ADCR.ADST = 1;			// start next conversion
	return value;
}

// average of SAMPLES readings
uint16_t read_vr1(void)
{
	uint8_t i;
	uint32_t sum = 0;

	for (i = 0; i < SAMPLES; i++) sum += read_adc7();
	return sum / SAMPLES;
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

void lcd_display(uint16_t adc)
{
	uint16_t i;
	int16_t x;
	uint16_t len = (uint32_t)adc * BAR_MAX / 4095;		// bar length in pixels
	uint32_t mv  = (uint32_t)adc * 3300 / 4095;		// 0 ~ 3300 mV
	uint16_t pct = ((uint32_t)adc * 100 + 2047) / 4095;	// 0 ~ 100 %
	char text[24];

	for (i = 0; i < sizeof(screen); i++) screen[i] = 0;

	draw_text(19, 0, "VR1 BAR DISPLAY");
	for (i = 0; i < LCD_Xmax; i += 2) set_pixel(i, 9);	// dotted line under the title

	rect(BAR_X, BAR_Y, BAR_W, BAR_H);
	fill_rect(BAR_X + 1, BAR_Y + 1, len, BAR_H - 2);

	// scale marks under the bar : 0, 25, 50, 75, 100 %
	for (i = 0; i <= 4; i++)
	{
		x = BAR_X + 1 + i * (BAR_MAX - 1) / 4;
		fill_rect(x, BAR_Y + BAR_H, 1, (i % 2) ? 2 : 4);
	}
	draw_text(BAR_X, BAR_Y + BAR_H + 5, "0");
	draw_text(57, BAR_Y + BAR_H + 5, "50");
	draw_text(108, BAR_Y + BAR_H + 5, "100");

	sprintf(text, "ADC7 : %4d", adc);
	draw_text(0, 47, text);
	sprintf(text, "%d.%02d V    %3d %%", mv / 1000, (mv % 1000) / 10, pct);
	draw_text(0, 56, text);

	draw_LCD(screen);
}

int32_t main (void)
{
	uint16_t adc;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();

	init_LCD();
	clear_LCD();

	InitADC();

	while (1)
	{
		adc = read_vr1();
		lcd_display(adc);
	}
}
