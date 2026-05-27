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
 * Public interface for the MultiRexZ80 MAME-derived SN76489 backend.
 *
 * Based on MAME sn76496.cpp behavior. MAME source license: BSD-3-Clause;
 * copyright-holders include Nicola Salmoria and later MAME contributors.
 * MultiRexZ80 integration Copyright (C) 2026 gameblabla.
 */

#ifndef SN76489_DEFINE
#define SN76489_DEFINE

#include <stdint.h>

/*
 * Small C wrapper around MAME's sn76496_base_device model.
 * The public shape is intentionally the same as the old MultiRexZ80 MAME_PSG
 * backend so save states and the generic mixer can keep using a single global.
 */
typedef struct sn76489_struct
{
    uint32_t m_feedback_mask;
    uint32_t m_whitenoise_tap1;
    uint32_t m_whitenoise_tap2;
    uint8_t  m_negate;
    uint8_t  m_stereo;
    uint8_t  m_clock_divider;
    uint8_t  m_ncr_style_psg;
    uint8_t  m_sega_style_psg;

    uint8_t  m_last_register;
    uint8_t  m_ready_state;

    int32_t  m_cycles_to_ready;
    int32_t  m_stereo_mask;

    int32_t  m_output[4];
    int32_t  m_period[4];
    int32_t  m_count[4];
    int32_t  m_register[8];
    int32_t  m_volume[4];
    int32_t  m_vol_table[16];
    uint32_t m_RNG;

    int32_t  m_current_clock;
    double   m_internal_samples_per_output;
    double   m_internal_sample_phase;
} sn76489_t;

extern sn76489_t PSG;

void SN76489_Init(uint32_t machine, uint32_t clock, uint32_t sample_rate);
void SN76489_GGStereoWrite(uint8_t st);
void SN76489_Write(uint8_t data);
void SN76489_Update(int16_t **outputs, int samples);

#endif
