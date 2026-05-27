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

#ifndef MULTIREXZ80_COLECO_PLATFORM_H_
#define MULTIREXZ80_COLECO_PLATFORM_H_

#include <stdint.h>

void coleco_port_w(uint16_t port, uint8_t data);
uint8_t coleco_port_r(uint16_t port);

#endif /* MULTIREXZ80_COLECO_PLATFORM_H_ */
