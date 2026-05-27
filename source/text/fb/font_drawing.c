/*
 * MultiRexZ80
 *
 * Multi-system Z80 emulator based on SMS Plus GX by Eke-Eke, itself based on
 * SMS Plus by Charles MacDonald.
 *
 * Default project license: GPL-2.0-or-later.  File-specific notices below
 * are retained and take precedence for imported or derived components,
 * including MAME-derived code and other third-party modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "multirexz80.h"
#include "font_drawing.h"
#include "font_menudata.h"

#ifndef HOST_WIDTH_RESOLUTION
#define HOST_WIDTH_RESOLUTION 320
#endif
#ifndef HOST_HEIGHT_RESOLUTION
#define HOST_HEIGHT_RESOLUTION 240
#endif

typedef struct font_target_t
{
	font_pixel_t *buffer;
	int32_t pitch;
	int32_t width;
	int32_t height;
} font_target_t;

#define FONT_TARGET_MAX 8
static font_target_t font_targets[FONT_TARGET_MAX];
static int32_t font_target_count = 0;

void font_drawing_set_target(font_pixel_t *buffer, int32_t pitch_pixels, int32_t width, int32_t height)
{
	int32_t i;
	if (!buffer) return;
	for (i = 0; i < font_target_count; i++)
	{
		if (font_targets[i].buffer == buffer)
		{
			font_targets[i].pitch = (pitch_pixels > 0) ? pitch_pixels : width;
			font_targets[i].width = (width > 0) ? width : HOST_WIDTH_RESOLUTION;
			font_targets[i].height = (height > 0) ? height : HOST_HEIGHT_RESOLUTION;
			return;
		}
	}
	if (font_target_count < FONT_TARGET_MAX)
	{
		font_targets[font_target_count].buffer = buffer;
		font_targets[font_target_count].pitch = (pitch_pixels > 0) ? pitch_pixels : width;
		font_targets[font_target_count].width = (width > 0) ? width : HOST_WIDTH_RESOLUTION;
		font_targets[font_target_count].height = (height > 0) ? height : HOST_HEIGHT_RESOLUTION;
		font_target_count++;
	}
}

static font_target_t *find_target(font_pixel_t *buffer)
{
	int32_t i;
	for (i = 0; i < font_target_count; i++)
		if (font_targets[i].buffer == buffer)
			return &font_targets[i];
	return NULL;
}

static int32_t target_pitch(font_pixel_t *buffer)
{
	font_target_t *target = find_target(buffer);
	return target ? target->pitch : HOST_WIDTH_RESOLUTION;
}

static int32_t target_width(font_pixel_t *buffer)
{
	font_target_t *target = find_target(buffer);
	return target ? target->width : HOST_WIDTH_RESOLUTION;
}

static int32_t target_height(font_pixel_t *buffer)
{
	font_target_t *target = find_target(buffer);
	return target ? target->height : HOST_HEIGHT_RESOLUTION;
}

static inline void setPixel(font_pixel_t *buffer, int32_t x, int32_t y, font_pixel_t c)
{
	if (!buffer) return;
	if (x < 0 || y < 0) return;
	if (x >= target_width(buffer) || y >= target_height(buffer)) return;
	buffer[x + y * target_pitch(buffer)] = c;
}

static int32_t isOutlinePixel(uint8_t* charfont, int32_t x, int32_t y)
{
	int32_t xis0 = !x, xis7 = x == 7, yis0 = !y, yis7 = y == 7;
	
	if(xis0)
	{
		if(yis0)
		{
			return !(*charfont & 0x80) && ((*charfont & 0x40) || (charfont[1] & 0x80) || (charfont[1] & 0x40));
		}
		else if(yis7)
		{
			return !(charfont[7] & 0x80) && ((charfont[7] & 0x40) || (charfont[6] & 0x80) || (charfont[6] & 0x40));
		}
		else
		{
			return !(charfont[y] & 0x80) && (
				(charfont[y - 1] & 0x80) || (charfont[y - 1] & 0x40) ||
				(charfont[y] & 0x40) ||
				(charfont[y + 1] & 0x80) || (charfont[y + 1] & 0x40));
		}
	}
	else if(xis7)
	{
		if(yis0)
		{
			return !(*charfont & 0x01) && ((*charfont & 0x02) || (charfont[1] & 0x01) || (charfont[1] & 0x02));
		}
		else if(yis7)
		{
			return !(charfont[7] & 0x01) && ((charfont[7] & 0x02) || (charfont[6] & 0x01) || (charfont[6] & 0x02));
		}
		else
		{
			return !(charfont[y] & 0x01) && (
				(charfont[y - 1] & 0x01) || (charfont[y - 1] & 0x02) ||
				(charfont[y] & 0x02) ||
				(charfont[y + 1] & 0x01) || (charfont[y + 1] & 0x02));
		}
	}
	else
	{
		int32_t b = 1 << (7 - x);
		if(yis0)
		{
			return !(*charfont & b) && (
				(*charfont & (b << 1)) || (*charfont & (b >> 1)) ||
				(charfont[1] & (b << 1)) || (charfont[1] & b) || (charfont[1] & (b >> 1)));
		}
		else if(yis7)
		{
			return !(charfont[7] & b) && (
				(charfont[7] & (b << 1)) || (charfont[7] & (b >> 1)) ||
				(charfont[6] & (b << 1)) || (charfont[6] & b) || (charfont[6] & (b >> 1)));
		}
		else
		{
			return !(charfont[y] & b) && (
				(charfont[y] & (b << 1)) || (charfont[y] & (b >> 1)) ||
				(charfont[y - 1] & (b << 1)) || (charfont[y - 1] & b) || (charfont[y - 1] & (b >> 1)) ||
				(charfont[y + 1] & (b << 1)) || (charfont[y + 1] & b) || (charfont[y + 1] & (b >> 1)));
		}
	}
}

static void drawChar(font_pixel_t* restrict buffer, int32_t *x, int32_t *y, int32_t margin, unsigned char ch, font_pixel_t fc, font_pixel_t olc)
{
	int32_t i, j;
	uint8_t *charSprite;
	if (ch == '\n')
	{
		*x = margin;
		*y += 8;
	}
	else if(*y < target_height(buffer)-1)
	{
		charSprite = ch * 8 + n2DLib_font;
		// Draw charSprite as monochrome 8*8 image using given color
		for(i = 0; i < 8; i++)
		{
			for(j = 7; j >= 0; j--)
			{
				if((charSprite[i] >> j) & 1)
				{
					setPixel(buffer, *x + (7 - j), *y + i, fc);
				}
				else if(isOutlinePixel(charSprite, 7 - j, i))
				{
					setPixel(buffer, *x + (7 - j), *y + i, olc);
				}
			}
		}
		*x += 8;
	}
}

static void drawString(font_pixel_t* restrict buffer, int32_t *x, int32_t *y, int32_t _x, const char *str, font_pixel_t fc, font_pixel_t olc)
{
	unsigned long i, max = strlen(str);
	for(i = 0; i < max; i++)
		drawChar(buffer, x, y, _x, (unsigned char)str[i], fc, olc);
}

void print_string(const char *s, font_pixel_t fg_color, font_pixel_t bg_color, int32_t x, int32_t y, font_pixel_t* restrict buffer) 
{
	drawString(buffer, &x, &y, 0, s, fg_color, bg_color);
}
