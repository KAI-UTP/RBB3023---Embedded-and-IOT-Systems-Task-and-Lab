//
// Lab 6 : 4-channel ADC with keypad view select, polling or interrupt
//
//   Channels : CH1 = ADC4 (GPA4), CH2 = ADC5 (GPA5),
//              CH3 = ADC6 (GPA6), CH4 = ADC7 (GPA7) = VR1
//              12-bit result 0 ~ 4095  =  0 ~ 3.3V  (Vref = 3.3V on the board)
//              CH1 ~ 3 have nothing connected, so they float (random values)
//              until you wire a voltage 0 ~ 3.3V to the pin (header ADC0~7)
//   ADC clk  : 22MHz RC / (1 + 1) = 11MHz (max 16MHz)
//
//   Keypad   : key 1 ~ 4 = show that one channel (value, voltage, percent)
//              key 5     = show all 4 channels together
//   Button   : SW_INT (GPB15, 0 = pressed) switches the ADC mode
//              P = polling, register level, single-cycle scan (ADMD = 2)
//              I = interrupt, BSP DrvADC continuous scan + AdcIntCallback
//              the mode letter is the last char of LCD line 0
//
//   Pin share: keypad rows GPA0 ~ 2, columns GPA3 ~ 5, but GPA4 / GPA5 are
//              also ADC4 / ADC5. key_scan() switches GPA4 / 5 to GPIO for the
//              scan only, then back to ADC. ADC is always read after that.
//              A column held low by a wire is ignored (not a key press).
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "ADC.h"
#include "LCD.h"
#include "Scankey.h"

#define CH_MASK		0xF0	// ADC4 ~ ADC7
#define CH_FIRST	4	// CH1 = ADC4

volatile uint8_t gu8AdcIntFlag = 0;	// set by the ADC callback

// ADC interrupt callback (called from BSP ADC_IRQHandler)
void AdcIntCallback(uint32_t u32UserData)
{
	gu8AdcIntFlag = 1;
}

// ---------------- pins ----------------

// GPA4 / 5 back to ADC : input (no pull-up), digital path off, ADC function
void pins_adc(void)
{
	DrvGPIO_Open(E_GPA, 4, E_IO_INPUT);
	DrvGPIO_Open(E_GPA, 5, E_IO_INPUT);
	GPIOA->OFFD |= 0x00300000;
	SYS->GPAMFP.ADC4_AD9 = 1;		// GPA4 = ADC4
	SYS->GPAMFP.ADC5_AD8 = 1;		// GPA5 = ADC5
}

// GPA4 / 5 to keypad columns : GPIO, digital path on, quasi (pull-up)
void pins_keypad(void)
{
	SYS->GPAMFP.ADC4_AD9 = 0;
	SYS->GPAMFP.ADC5_AD8 = 0;
	GPIOA->OFFD &= ~0x00300000;
	DrvGPIO_Open(E_GPA, 4, E_IO_QUASI);
	DrvGPIO_Open(E_GPA, 5, E_IO_QUASI);
}

void adc_pin_init(void)
{
	OpenKeyPad();				// GPA0 ~ 5 quasi
	pins_adc();				// GPA4 / 5 to ADC
	DrvGPIO_Open(E_GPA, 6, E_IO_INPUT);
	DrvGPIO_Open(E_GPA, 7, E_IO_INPUT);
	GPIOA->OFFD |= 0x00C00000;		// digital path off for GPA6 / 7
	SYS->GPAMFP.ADC6_AD7 = 1;		// GPA6 = ADC6
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7 (VR1)
}

// ---------------- keypad ----------------

void key_delay(void)
{
	volatile int j;
	for (j = 0; j < 1000; j++);
}

// same numbering as BSP ScanKey() : 1 ~ 9, 0 = no key
//   row GPA2 low -> 1 / 4 / 7, GPA1 low -> 2 / 5 / 8, GPA0 low -> 3 / 6 / 9
//   column GPA3 -> 1 ~ 3, GPA4 -> 4 ~ 6, GPA5 -> 7 ~ 9
uint8_t key_scan(void)
{
	uint8_t row, col, ext = 0, key = 0;

	pins_keypad();
	for (row = 0; row < 6; row++)
		DrvGPIO_SetBit(E_GPA, row);	// all rows / columns high
	key_delay();

	// a column already low with no row driven = wired to something, skip it
	for (col = 0; col < 3; col++)
		if (DrvGPIO_GetBit(E_GPA, 3 + col) == 0) ext |= 1 << col;

	for (row = 0; row < 3 && key == 0; row++)
	{
		DrvGPIO_ClrBit(E_GPA, 2 - row);	// drive one row low
		key_delay();
		for (col = 0; col < 3; col++)
		{
			if ((ext & (1 << col)) == 0 && DrvGPIO_GetBit(E_GPA, 3 + col) == 0)
			{
				key = row + 1 + col * 3;
				break;
			}
		}
		DrvGPIO_SetBit(E_GPA, 2 - row);
	}

	pins_adc();
	return key;
}

// ---------------- ADC ----------------

// Mode P : polling setup, register level
void InitADC(void)
{
	/* stop mode I if it was running */
	ADC->ADCR.ADST = 0;
	ADC->ADCR.ADIE = 0;
	NVIC_DisableIRQ(ADC_IRQn);
	/* ADC clock source, then enable ADC */
	SYSCLK->CLKSEL1.ADC_S = 3;		// 3 = 22MHz RC (2 = HCLK, too fast at 48MHz)
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;		// enable ADC clock
	ADC->ADCR.ADEN = 1;			// enable ADC module
	/* operation mode */
	ADC->ADCR.DIFFEN = 0;			// single-end input
	ADC->ADCR.ADMD = 2;			// single-cycle scan (single mode = 1 channel only)
	/* channels */
	ADC->ADCHER.CHEN = CH_MASK;		// ADC4 ~ 7
	ADC->ADSR.ADF = 1;			// clear the flag
}

// Mode I : interrupt setup, BSP driver
void InitADC_IRQ(void)
{
	DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, CH_MASK, INTERNAL_RC22MHZ, 1);
	gu8AdcIntFlag = 0;
	DrvADC_EnableADCInt(AdcIntCallback, 0);
	DrvADC_StartConvert();
}

// Mode P : start one scan, wait, read 4 channels
void read_polling(uint16_t *value)
{
	int i;

	ADC->ADSR.ADF = 1;			// clear the flag
	ADC->ADCR.ADST = 1;			// start one scan of ADC4 ~ 7
	while (ADC->ADSR.ADF == 0);		// wait till the scan is done
	ADC->ADSR.ADF = 1;
	for (i = 0; i < 4; i++)
		value[i] = ADC->ADDR[CH_FIRST + i].RSLT;
}

// Mode I : wait for 2 callbacks (so one full scan is after key_scan), read
void read_irq(uint16_t *value)
{
	int i;

	gu8AdcIntFlag = 0;
	while (gu8AdcIntFlag == 0);		// end of the scan in progress
	gu8AdcIntFlag = 0;
	while (gu8AdcIntFlag == 0);		// one full fresh scan
	for (i = 0; i < 4; i++)
		value[i] = DrvADC_GetConversionData(CH_FIRST + i);
}

// ---------------- LCD ----------------

// key 1 ~ 4 : one channel, big detail
void show_one(uint8_t ch, uint16_t value, uint8_t mode)
{
	char TEXT[17];				// 16 chars + end zero
	uint32_t mv = value * 3300 / 4095;

	sprintf(TEXT, "CH%d = ADC%d %s  ", ch, CH_FIRST + ch - 1, (ch == 4) ? "VR1" : "   ");
	TEXT[15] = mode;
	print_Line(0, TEXT);
	sprintf(TEXT, "ADC Value: %4d ", value);
	print_Line(1, TEXT);
	sprintf(TEXT, "Volt : %d.%03d V ", mv / 1000, mv % 1000);
	print_Line(2, TEXT);
	sprintf(TEXT, "Level: %3d %%    ", value * 100 / 4095);
	print_Line(3, TEXT);
}

// key 5 : all 4 channels
void show_all(uint16_t *value, uint8_t mode)
{
	char TEXT[17];
	uint32_t mv;
	int i;

	for (i = 0; i < 4; i++)
	{
		mv = value[i] * 3300 / 4095;
		sprintf(TEXT, "%d:%4d %d.%03dV   ", i + 1, value[i], mv / 1000, mv % 1000);
		if (i == 0) TEXT[15] = mode;
		print_Line(i, TEXT);
	}
}

// ---------------- main ----------------

int32_t main(void)
{
	uint8_t mode = 'P', view = 5;
	uint8_t key, last_key = 0, btn, last_btn = 1;
	uint16_t adc[4];

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();
	clear_LCD();
	DrvGPIO_Open(E_GPB, 15, E_IO_QUASI);	// SW_INT button, pulled high
	DrvGPIO_SetBit(E_GPB, 15);
	adc_pin_init();
	InitADC();

	while (1)
	{
		// keypad : 1 ~ 4 one channel, 5 all channels (new press only)
		key = key_scan();
		if (key != 0 && last_key == 0 && key <= 5 && key != view)
		{
			view = key;
			clear_LCD();
		}
		last_key = key;

		// SW_INT : switch polling <-> interrupt
		btn = DrvGPIO_GetBit(E_GPB, 15);
		if (btn == 0 && last_btn == 1)
		{
			if (mode == 'P')	{ mode = 'I'; InitADC_IRQ(); }
			else			{ mode = 'P'; InitADC(); }
		}
		last_btn = btn;

		if (mode == 'P')	read_polling(adc);
		else			read_irq(adc);

		if (view == 5)	show_all(adc, mode);
		else		show_one(view, adc[view - 1], mode);

		DrvSYS_Delay(50000);		// 50 ms
	}
}
