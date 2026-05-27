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


#ifndef FMINTF_H_
#define FMINTF_H_

enum
{
    SND_NULL,
    SND_OPLL            /* Unified OPL backend: YM2413 OPLL mode */
};

typedef struct 
{
    uint8_t latch;
    uint8_t reg[0x40];
} FM_Context;

/* Function prototypes */
void FM_Init(void);
void FM_Shutdown(void);
void FM_Reset(void);
void FM_Update(int16_t **buffer, int32_t length);
void FM_Write(uint32_t offset, uint8_t data);
void FM_GetContext(uint8_t *data);
void FM_SetContext(uint8_t *data);
uint32_t FM_GetContextSize(void);
uint8_t *FM_GetContextPtr(void);
void FM_WriteReg(uint8_t reg, uint8_t data);

#endif /* FMINTF_H_ */
