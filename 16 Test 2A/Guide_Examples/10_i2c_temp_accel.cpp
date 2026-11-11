//
// Example 10 : I2C sensors on the app board (extra - not in the lectures)
//
//   LM75B temperature  : 8-bit address 0x90, register 0 = 2 bytes
//                        t = ((b0 << 8) | b1) >> 5 (signed 11-bit), 0.125 C per count
//   MMA7660 accelerometer : 8-bit address 0x98
//                        write {0x07, 0x01} = active mode
//                        registers 0, 1, 2 = X, Y, Z, 6-bit signed, ~21 counts per g
//   I2C pins : SDA p28, SCL p27
//   LCD shows the temperature and X/Y/Z, serial prints them every 0.5 s
//
#include "mbed.h"
#include "lcd_c12832.h"

#define LM75B_ADDR	0x90
#define MMA7660_ADDR	0x98

I2C        i2c(p28, p27);		// SDA, SCL
LCD_C12832 lcd;

// temperature in tenths of a degree (e.g. 253 = 25.3 C)
int read_temp_x10()
{
	char reg = 0, d[2];

	i2c.write(LM75B_ADDR, &reg, 1, true);	// true = repeated start, keep the bus
	i2c.read(LM75B_ADDR, d, 2);
	int16_t raw = (int16_t)((d[0] << 8) | d[1]) >> 5;	// keeps the sign
	return raw * 10 / 8;				// 0.125 C = 1/8
}

// 6-bit two's complement -> -32 ~ +31
int six_bit(char v)
{
	v &= 0x3F;
	return (v & 0x20) ? v - 64 : v;
}

int main()
{
	char text[24];
	char cmd[2] = { 0x07, 0x01 };		// MODE register = active

	lcd.init();
	i2c.frequency(100000);
	i2c.write(MMA7660_ADDR, cmd, 2);

	while (true)
	{
		char reg = 0, xyz[3];

		i2c.write(MMA7660_ADDR, &reg, 1, true);
		i2c.read(MMA7660_ADDR, xyz, 3);
		int x = six_bit(xyz[0]), y = six_bit(xyz[1]), z = six_bit(xyz[2]);
		int t = read_temp_x10();

		lcd.clear();
		sprintf(text, "Temp %d.%d C", t / 10, abs(t % 10));
		lcd.text(0, 0, text);
		sprintf(text, "X %3d Y %3d Z %3d", x, y, z);
		lcd.text(0, 12, text);
		lcd.text(0, 24, "tilt the board");
		lcd.update();

		printf("T=%d.%d C  X=%d Y=%d Z=%d\r\n", t / 10, abs(t % 10), x, y, z);
		ThisThread::sleep_for(500ms);
	}
}
