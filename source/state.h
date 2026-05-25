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
 *   Freeze State support
 *
 ******************************************************************************/

#ifndef STATE_H_
#define STATE_H_

#include <stdint.h>
#include <stdio.h>

#define STATE_VERSION   0x0104      /* Legacy raw state payload version 1.4 (BCD) */
#define STATE_HEADER    "SST\0"     /* Legacy state file header, retained for compatibility. */

#define STATE2_VERSION  0x0200      /* Wrapped/compressed state container version 2.0. */
#define STATE2_THUMB_XRGB8888 1

#ifdef __cplusplus
extern "C" {
#endif

/* Legacy raw payload entry points.  Existing ports may keep using these. */
extern uint32_t system_save_state(FILE *fd);
extern void system_load_state(FILE *fd);

/* Memory-buffer helpers used by rewind and by the state v2 wrapper. */
extern int system_save_state_buffer(uint8_t **out_data, uint32_t *out_size);
extern int system_load_state_buffer(const uint8_t *data, uint32_t size);
extern void system_free_state_buffer(void *data);

/* PNG save states.  The visible image is the thumbnail/screenshot and the
 * compressed legacy payload is stored in a private PNG chunk.  Loading still
 * accepts old raw state files and the short-lived SPGXST2 wrapper. */
extern int system_save_state_png(const char *path,
                                 const void *thumbnail_xrgb8888,
                                 uint32_t thumbnail_width,
                                 uint32_t thumbnail_height,
                                 uint32_t thumbnail_pitch);
extern int system_load_state_file(const char *path);
extern int system_state_png_read_thumbnail(const char *path,
                                           uint32_t **out_xrgb8888,
                                           uint32_t *out_width,
                                           uint32_t *out_height);
#define system_save_state_file_ex system_save_state_png

#ifdef __cplusplus
}
#endif

#endif /* _STATE_H_ */
