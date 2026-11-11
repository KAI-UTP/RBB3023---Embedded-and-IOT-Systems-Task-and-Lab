//
// Lab 6 (1) : Smpl_ADC_VR1 - read the variable resistor VR1 with ADC7 (polling)
//
//   VR1     : on-board variable resistor on GPA7 = ADC7
//   ADC     : 12-bit SAR, single-end, single mode, channel 7 only
//             clock 22MHz / (1 + 1) = 11MHz (max 16MHz)
//             result 0 ~ 4095  =  0 ~ 3.3V  (Vref = 3.3V on the board)
//   How     : start a conversion (ADST = 1), wait for the flag ADF = 1,
//             write 1 to ADF to clear it, read ADDR[7], start again
//   LCD     : line 1 = ADC value, line 2 = voltage, line 3 = percent
//   Turn VR1 to change the numbers
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"

void InitADC(void)
{
	/* Step 1. GPIO initial */
	GPIOA->OFFD |= 0x00800000;		// disable digital input path of GPA7
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
	/* Step 2. ADC clock source, then enable ADC */
	SYSCLK->CLKSEL1.ADC_S = 3;		// 3 = 22MHz RC (2 = HCLK, too fast at 48MHz)
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;		// enable ADC clock
	ADC->ADCR.ADEN = 1;			// enable ADC module
	/* Step 3. operation mode */
	ADC->ADCR.DIFFEN = 0;			// single-end input
	ADC->ADCR.ADMD = 0;			// single mode
	/* Step 4. channel */
	ADC->ADCHER.CHEN = 0x80;		// channel 7 only (bit 7)
	/* Step 5. clear the flag (polling, so no NVIC interrupt) */
	ADC->ADSR.ADF = 1;
	/* Step 6. start the first conversion */
	ADC->ADCR.ADST = 1;
}

int32_t main(void)
{
	char TEXT[17];			// 16 chars + end zero
	uint32_t adc, mv;

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	InitADC();
	init_LCD();
	clear_LCD();
	print_Line(0, "Smpl_ADC_VR1    ");

	while (1)
	{
		while (ADC->ADSR.ADF == 0);	// wait till conversion done
		ADC->ADSR.ADF = 1;		// write 1 to clear the flag
		adc = ADC->ADDR[7].RSLT;	// 0 ~ 4095

		mv = adc * 3300 / 4095;		// 0 ~ 3300 mV
		sprintf(TEXT, "ADC Value: %4d ", adc);
		print_Line(1, TEXT);
		sprintf(TEXT, "Volt : %d.%03d V ", mv / 1000, mv % 1000);
		print_Line(2, TEXT);
		sprintf(TEXT, "Level: %3d %%    ", adc * 100 / 4095);
		print_Line(3, TEXT);

		DrvSYS_Delay(20000);		// 20 ms
		ADC->ADCR.ADST = 1;		// start the next conversion
	}
}
