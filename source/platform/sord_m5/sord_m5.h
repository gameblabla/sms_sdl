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

#ifndef MULTIREXZ80_SORD_M5_PLATFORM_H_
#define MULTIREXZ80_SORD_M5_PLATFORM_H_

#include <stdint.h>

void sordm5_port_w(uint16_t port, uint8_t data);
uint8_t sordm5_port_r(uint16_t port);
void sordm5_ctc_reset(void);
void sordm5_ctc_vdp_interrupt(void);
void sordm5_ctc_tick(int32_t cycles);
int32_t sordm5_ctc_irq_callback(void);

#endif /* MULTIREXZ80_SORD_M5_PLATFORM_H_ */
