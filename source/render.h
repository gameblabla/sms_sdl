/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additionnal code by Eke-Eke (SMS Plus GX)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   VDP rendering core
 *
 ******************************************************************************/

#ifndef RENDER_H_
#define RENDER_H_

/*
 * Native renderer output format.
 *
 * Legacy handheld/SDL 1.2 ports normally keep the old RGB565 path.  Modern
 * frontends can compile with -DSMSPLUS_RENDER_32BPP to render directly to
 * XRGB8888/ARGB8888-style pixels, avoiding the lossy RGB565 remap and the
 * extra conversion most 32 bpp GPUs perform internally.
 */
#ifdef SMSPLUS_RENDER_32BPP
#define SMSPLUS_RENDER_DEPTH 32
#define SMSPLUS_RENDER_BYTES_PER_PIXEL 4
#define MAKE_PIXEL(r,g,b)   (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#else
#define SMSPLUS_RENDER_DEPTH 16
#define SMSPLUS_RENDER_BYTES_PER_PIXEL 2
/* Pack RGB data into a 16-bit RGB 5:6:5 format */
#define MAKE_PIXEL(r,g,b)   (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F))
#endif

/* Used for blanking a line in whole or in part */
#define BACKDROP_COLOR      (0x10 | (vdp.reg[7] & 0x0F))

extern void (*render_bg)(int32_t line);
extern void (*render_obj)(int32_t line);
extern uint8_t *linebuf;
extern uint8_t sms_cram_expand_table[4];
extern uint8_t gg_cram_expand_table[16];
extern uint8_t bg_name_dirty[0x200];
extern uint16_t bg_name_list[0x200];
extern uint16_t bg_list_index;

extern void render_shutdown(void);
extern void render_init(void);
extern void render_reset(void);
extern void render_line(int32_t line);
extern void render_bg_sms(int32_t line);
extern void render_obj_sms(int32_t line);
extern void palette_sync(int32_t index);

#endif /* _RENDER_H_ */
