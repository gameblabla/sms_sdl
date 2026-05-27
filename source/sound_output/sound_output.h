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

#ifndef SOUND_OUTPUT_H
#define SOUND_OUTPUT_H

#include <stdint.h>

extern void Sound_Init(void);
//extern void Sound_Update(void);
extern void Sound_Update(int16_t* sound_buffer, unsigned long len);
extern void Sound_Close(void);
extern void Sound_Pause(void);
extern void Sound_Unpause(void);

#endif
