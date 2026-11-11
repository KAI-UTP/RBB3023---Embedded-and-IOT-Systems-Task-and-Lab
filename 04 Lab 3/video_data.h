//
// 1. Chan Li Kai
// 2. William Wong Xiao Kang
// 3. Irvin Chong Hou Ceng
// 4. Liang Yan Ee
//
// video_data.h : made by lab3_video_to_lcd.py, do not edit
//
#ifndef _VIDEO_DATA_H
#define _VIDEO_DATA_H

#include <stdint.h>

#define VIDEO_HALF	1		// 1 = 64x32 drawn as 2x2 blocks, 0 = 128x64
#define VIDEO_W		64
#define VIDEO_H		32
#define VIDEO_FRAMES	91		// number of frames
#define VIDEO_FRAME_US	100000	// time per frame (us), 10 fps
#define VIDEO_BYTES	20036	// size of VIDEO[]

extern const uint8_t VIDEO[VIDEO_BYTES];

#endif
