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

#ifndef FONT_DRAWING_H
#define FONT_DRAWING_H

#include <stdint.h>
#include <string.h>

#ifdef MULTIREXZ80_RENDER_32BPP
typedef uint32_t font_pixel_t;
#define TextWhite 0xFFFFFFFFu
#define TextRed   0xFFFF0000u
#define TextBlue  0xFF0000FFu
#else
typedef uint16_t font_pixel_t;
#define TextWhite 65535
#define TextRed ((255>>3)<<11) + ((0>>2)<<5) + (0>>3)
#define TextBlue ((0>>3)<<11) + ((0>>2)<<5) + (255>>3)
#endif

void font_drawing_set_target(font_pixel_t *buffer, int32_t pitch_pixels, int32_t width, int32_t height);
void print_string(const char *s, font_pixel_t fg_color, font_pixel_t bg_color, int32_t x, int32_t y, font_pixel_t* restrict buffer);

#endif
