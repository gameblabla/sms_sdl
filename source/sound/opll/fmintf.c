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
    fmintf.c --
    Master System FM Unit interface.

    The old standalone YM2413 backend has been replaced with the unified
    OPL/OPLL core in source/sound/opl1/opl.c.
*/
#include "shared.h"

static FM_Context fm_context;
static opl_chip_t *fmm;

/* Only the 1st Master System supports FM sound. Because it is still processed even though it's not used,
* don't process FM sound if it can't be used in any way.
*/
static uint32_t isfm_used = 0;

void FM_Init(void)
{
    isfm_used = 0;
    if (fmm)
        OPL_Destroy(fmm);
    fmm = OPL_Create(OPL_CHIP_YM2413, snd.fm_clock, snd.sample_rate);
}


void FM_Shutdown(void)
{
    isfm_used = 0;
    OPL_Destroy(fmm);
    fmm = NULL;
}


void FM_Reset(void)
{
    isfm_used = 0;
    if (!fmm)
        fmm = OPL_Create(OPL_CHIP_YM2413, snd.fm_clock, snd.sample_rate);
    else
        OPL_Reset(fmm);
    memset(&fm_context, 0, sizeof(fm_context));
}


void FM_Update(int16_t **buffer, int32_t length)
{
    if (isfm_used && fmm) OPL_UpdateStereo(fmm, buffer[0], buffer[1], length);
}

void FM_WriteReg(uint8_t reg, uint8_t data)
{
    FM_Write(0, reg);
    FM_Write(1, data);
}

void FM_Write(uint32_t offset, uint8_t data)
{
    if(offset & 1)
    {
        fm_context.reg[fm_context.latch & 0x3f] = data;
    }
    else
    {
        fm_context.latch = data & 0x3f;
    }

    if (fmm)
        OPL_Write(fmm, offset & 1, data);
    isfm_used = 1;
}


void FM_GetContext(uint8_t *data)
{
    memcpy(data, &fm_context, sizeof(FM_Context));
}

void FM_SetContext(uint8_t *data)
{
    uint8_t i;
    uint8_t reg[0x40];

    memcpy(&fm_context, data, sizeof(FM_Context));
    memcpy(reg, fm_context.reg, sizeof(reg));

    /* If we are loading a save state, update the register image but avoid
       touching the chip while sound/FM is disabled. */
    if(!snd.enabled || !sms.use_fm)
        return;

    FM_Reset();

    FM_Write(0, 0x0E);
    FM_Write(1, reg[0x0E]);

    for(i = 0x00; i <= 0x07; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x10; i <= 0x18; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x20; i <= 0x28; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x30; i <= 0x38; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    FM_Write(0, fm_context.latch);
}

uint32_t FM_GetContextSize(void)
{
    return sizeof(FM_Context);
}

uint8_t *FM_GetContextPtr(void)
{
    return (uint8_t *)&fm_context;
}
