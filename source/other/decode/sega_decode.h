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

/*
 * Sega encrypted Z80 load-time decoder interface.
 *
 * Derived from MAME segacrpt_device.cpp and segacrp2_device.cpp.  MAME
 * source license: BSD-3-Clause.  Original copyright-holders: Nicola Salmoria,
 * David Haywood.
 */

#ifndef SEGA_DECODE_H_
#define SEGA_DECODE_H_

#include <stdint.h>

void sega_decode_315_5051(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5135(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5155(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5006(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5177(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5176(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5162(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5178(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5179(uint8_t *data, uint8_t *opcodes);
void sega_decode_317_0006(uint8_t *data, uint8_t *opcodes);
void sega_decode_317_0007(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5098(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5048(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5064(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5093(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5102(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5110(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5132(uint8_t *data, uint8_t *opcodes, uint32_t size);

#endif
