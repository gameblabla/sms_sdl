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
 * MC-8123 ROM decryption helper interface.
 *
 * Derived from MAME src/devices/cpu/z80/mc8123.cpp.  MAME source license:
 * BSD-3-Clause.  Original copyright-holders: Nicola Salmoria, David Widel.
 */

#ifndef MULTIREXZ80_MC8123_DECODE_H_
#define MULTIREXZ80_MC8123_DECODE_H_

#include <stdint.h>

void mc8123_generate_key(uint8_t key[0x2000], uint32_t seed, unsigned upper_bound);
void mc8123_decode(uint8_t *rom, uint8_t *opcodes, const uint8_t key[0x2000], unsigned length);

#endif
