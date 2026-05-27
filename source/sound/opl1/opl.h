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

#ifndef MULTIREXZ80_OPL_H
#define MULTIREXZ80_OPL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum opl_chip_type_e
{
    OPL_CHIP_YM3526 = 0,  /* OPL1 */
    OPL_CHIP_YM3812 = 1,  /* OPL2 */
    OPL_CHIP_Y8950  = 2,  /* OPL1-compatible FM plus ADPCM */
    OPL_CHIP_YM2413 = 3   /* OPLL */
} opl_chip_type_t;

typedef struct opl_chip opl_chip_t;
typedef void (*opl_irq_cb_t)(void *opaque, int state);

opl_chip_t *OPL_Create(opl_chip_type_t type, uint32_t clock, uint32_t sample_rate);
void OPL_Destroy(opl_chip_t *chip);
void OPL_Reset(opl_chip_t *chip);
void OPL_SetIRQHandler(opl_chip_t *chip, opl_irq_cb_t cb, void *opaque);
void OPL_SetADPCMROM(opl_chip_t *chip, const uint8_t *rom, uint32_t size);
uint8_t OPL_ReadStatus(opl_chip_t *chip);
void OPL_Write(opl_chip_t *chip, uint32_t offset, uint8_t data);
void OPL_UpdateMono(opl_chip_t *chip, int16_t *buffer, int32_t samples);
void OPL_UpdateStereo(opl_chip_t *chip, int16_t *left, int16_t *right, int32_t samples);

/* Native save-state helpers for wrapped OPL cores.  The payload is intentionally
 * emulator-native, matching the rest of MultiRexZ80's raw state format. */
uint32_t OPL_GetStateSize(opl_chip_t *chip);
int OPL_SaveState(opl_chip_t *chip, void *data, uint32_t size);
int OPL_LoadState(opl_chip_t *chip, const void *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* MULTIREXZ80_OPL_H */
