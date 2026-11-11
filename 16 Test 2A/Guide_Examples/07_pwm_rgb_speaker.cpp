//
// Example 07 : PwmOut - RGB LED brightness, speaker tone, AnalogOut
//
//   POT1 sets the red brightness, POT2 the blue brightness (RGB is active low)
//   Joystick up = 440 Hz beep for 150 ms on the speaker, down = 880 Hz
//   AnalogOut p18 outputs the POT2 voltage (0 ~ 3.3 V, 10-bit DAC)
//
//   RGB : R p23, G p24, B p25 (0 = on)   Speaker : p26
//   PWM pins on the LPC1768 : p21 ~ p26 (+ LED1 ~ LED4)
//   GOTCHA : all PwmOut pins share ONE period (one PWM timer), so the
//            speaker's period also changes the RGB period -> set it back.
//            LED1 uses the same PWM channel as p26 (speaker): don't use both.
//
#include "mbed.h"

PwmOut    red(p23), green(p24), blue(p25);
PwmOut    speaker(p26);
AnalogIn  pot1(p19), pot2(p20);
AnalogOut dac(p18);
DigitalIn joy_up(p15, PullDown);
DigitalIn joy_down(p12, PullDown);

// play a tone of freq Hz for ms milliseconds
void beep(int freq, int ms)
{
	speaker.period(1.0f / freq);	// period = 1 / frequency
	speaker = 0.5f;			// 50 % duty = loudest square wave
	ThisThread::sleep_for(std::chrono::milliseconds(ms));
	speaker = 0.0f;			// silent
	red.period_ms(10);		// shared period: back to 100 Hz for the RGB
}

int main()
{
	red.period_ms(10);		// 100 Hz PWM, no visible flicker
	green.period_ms(10);
	blue.period_ms(10);
	green = 1.0f;			// 1.0 = green fully off (active low)

	while (true)
	{
		float a = pot1.read();	// 0.0 ~ 1.0
		float b = pot2.read();

		red  = 1.0f - a;	// active low: invert
		blue = 1.0f - b;
		dac  = b;		// 0.0 ~ 1.0 -> 0 ~ 3.3 V on p18

		if (joy_up)   beep(440, 150);
		if (joy_down) beep(880, 150);
		ThisThread::sleep_for(20ms);
	}
}
