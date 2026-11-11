//
// Lab 6 : 4-channel ADC (ADC4 ~ ADC7), polling or interrupt, SW_INT switches
//
//   Inputs  : ADC4 = GPA4, ADC5 = GPA5, ADC6 = GPA6, ADC7 = GPA7 = VR1
//             12-bit result 0 ~ 4095  =  0 ~ 3.3V  (Vref = 3.3V on the board)
//             ADC4 ~ 6 have nothing connected, so they float (random values)
//             until you wire a voltage 0 ~ 3.3V to the pin (header ADC0~7)
//   ADC clk : 22MHz RC / (1 + 1) = 11MHz (max 16MHz)
//
//   Mode P  : polling, register level (Smpl_ADC_VR1 way)
//             single-cycle scan mode (ADMD = 2): one start converts all
//             4 channels once, then ADF = 1
//   Mode I  : interrupt, BSP driver (Smpl_ADC_IRQ way)
//             DrvADC_Open continuous scan of ADC4 ~ 7 (0xF0),
//             BSP ADC_IRQHandler calls AdcIntCallback after each scan
//
//   LCD     : line 0 ~ 3 = ADC4 ~ ADC7, value and voltage
//             last char of line 0 = mode letter (P / I)
//   Button  : SW_INT (GPB15, 0 = pressed) switches mode P <-> I
//   Keypad  : not used, it shares GPA0 ~ 5 with ADC0 ~ 5
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "ADC.h"
#include "LCD.h"

#define CH_MASK		0xF0	// ADC4 ~ ADC7
#define CH_FIRST	4

volatile uint8_t gu8AdcIntFlag = 0;	// set by the ADC callback

// ADC interrupt callback (called from BSP ADC_IRQHandler)
void AdcIntCallback(uint32_t u32UserData)
{
	gu8AdcIntFlag = 1;
}

// GPA4 ~ 7 as analog inputs
void adc_pin_init(void)
{
	int i;

	for (i = 4; i <= 7; i++)
		DrvGPIO_Open(E_GPA, i, E_IO_INPUT);	// input, no pull-up
	GPIOA->OFFD |= 0x00F00000;		// disable digital input path of GPA4 ~ 7
	SYS->GPAMFP.ADC4_AD9 = 1;		// GPA4 = ADC4
	SYS->GPAMFP.ADC5_AD8 = 1;		// GPA5 = ADC5
	SYS->GPAMFP.ADC6_AD7 = 1;		// GPA6 = ADC6
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7 (VR1)
}

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
	/* clear the flag, start the first scan */
	ADC->ADSR.ADF = 1;
	ADC->ADCR.ADST = 1;
}

// Mode I : interrupt setup, BSP driver
void InitADC_IRQ(void)
{
	DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, CH_MASK, INTERNAL_RC22MHZ, 1);
	gu8AdcIntFlag = 0;
	DrvADC_EnableADCInt(AdcIntCallback, 0);
	DrvADC_StartConvert();
}

// Mode P : wait for one scan, read 4 channels, start the next scan
void read_polling(uint16_t *value)
{
	int i;

	while (ADC->ADSR.ADF == 0);		// wait till the scan is done
	ADC->ADSR.ADF = 1;			// write 1 to clear the flag
	for (i = 0; i < 4; i++)
		value[i] = ADC->ADDR[CH_FIRST + i].RSLT;
	ADC->ADCR.ADST = 1;			// start the next scan
}

// Mode I : wait for the callback flag, then read 4 channels
void read_irq(uint16_t *value)
{
	int i;

	while (gu8AdcIntFlag == 0);		// wait till the callback sets it
	gu8AdcIntFlag = 0;
	for (i = 0; i < 4; i++)
		value[i] = DrvADC_GetConversionData(CH_FIRST + i);
}

int32_t main(void)
{
	char TEXT[17];				// 16 chars + end zero
	uint8_t mode = 'P', btn, last_btn = 1;
	uint16_t adc[4];
	uint32_t mv;
	int i;

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
		// SW_INT pressed (1 -> 0) switches the mode
		btn = DrvGPIO_GetBit(E_GPB, 15);
		if (btn == 0 && last_btn == 1)
		{
			if (mode == 'P')	{ mode = 'I'; InitADC_IRQ(); }
			else			{ mode = 'P'; InitADC(); }
		}
		last_btn = btn;

		if (mode == 'P')	read_polling(adc);
		else			read_irq(adc);

		for (i = 0; i < 4; i++)
		{
			mv = adc[i] * 3300 / 4095;	// 0 ~ 3300 mV
			sprintf(TEXT, "A%d:%4d %d.%03dV  ", CH_FIRST + i, adc[i], mv / 1000, mv % 1000);
			if (i == 0) TEXT[15] = mode;	// mode letter on line 0
			print_Line(i, TEXT);
		}

		DrvSYS_Delay(50000);		// 50 ms
	}
}
