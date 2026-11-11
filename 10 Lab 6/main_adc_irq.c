//
// Lab 6 (2) : Smpl_ADC_IRQ - sample ADC0~7 continuously, interrupt callback
//
//   ADC     : 12-bit, single-end, continuous scan mode, channels 0~7 (0xFF)
//             clock 22MHz / (1 + 1) = 11MHz (max 16MHz)
//             DrvADC_Open / DrvADC_EnableADCInt from the BSP (ADC.c in project)
//   IRQ     : BSP ADC_IRQHandler calls AdcIntCallback after every scan,
//             the callback only sets gu8AdcIntFlag = 1
//   main    : wait for the flag, clear it, read ADC7 = VR1, show on LCD
//   Note    : ADC0~5 share GPA0~5 with the keypad, only ADC7 (VR1) is shown
//   To use  : copy this file over main.c (Keil builds ..\main.c)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "ADC.h"
#include "LCD.h"

volatile uint8_t gu8AdcIntFlag;

// ADC interrupt callback : set the flag
void AdcIntCallback(uint32_t u32UserData)
{
	gu8AdcIntFlag = 1;
}

int32_t main(void)
{
	int i;
	char TEXT[17];			// 16 chars + end zero
	uint16_t adc_value[8];
	uint32_t mv;

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();
	clear_LCD();
	print_Line(0, "Smpl_ADC_IRQ    ");

	// GPA7 as analog input (VR1)
	GPIOA->OFFD |= 0x00800000;
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;

	// open 8 ADC channels for continuous sampling, 22MHz / (1 + 1) = 11MHz
	DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, 0xFF, INTERNAL_RC22MHZ, 1);
	DrvADC_EnableADCInt(AdcIntCallback, 0);

	i = 7;				// ADC channel = 7
	gu8AdcIntFlag = 0;		// reset flag
	DrvADC_StartConvert();		// start ADC sampling

	while (1)
	{
		while (gu8AdcIntFlag == 0);	// wait till the callback sets it
		gu8AdcIntFlag = 0;

		adc_value[i] = DrvADC_GetConversionData(i);
		mv = adc_value[i] * 3300 / 4095;

		sprintf(TEXT, "ADC%1d : %4d     ", i, adc_value[i]);
		print_Line(2, TEXT);
		sprintf(TEXT, "Volt : %d.%03d V ", mv / 1000, mv % 1000);
		print_Line(3, TEXT);

		DrvSYS_Delay(50000);		// 50 ms, slow down LCD updates
	}
}
