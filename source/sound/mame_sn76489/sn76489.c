/*
 * Texas Instruments / Sega SN76489-family PSG emulation.
 *
 * This is the compact C backend used by SMS Plus GX when MAME_PSG is enabled.
 * It follows MAME's sn76496_base_device variants and register semantics, but
 * writes directly into SMS Plus GX's existing signed 16-bit stereo buffers.
 */

#include <stdint.h>
#include <string.h>
#include "sn76489.h"

#define MAX_OUTPUT 0x7fff

sn76489_t PSG;

static int in_noise_mode(void)
{
    return (PSG.m_register[6] & 4) != 0;
}

static void countdown_cycles(void)
{
    if (PSG.m_cycles_to_ready > 0)
    {
        PSG.m_cycles_to_ready--;
        PSG.m_ready_state = 0;
    }
    else
    {
        PSG.m_ready_state = 1;
    }
}

void SN76489_Init(uint32_t machine, uint32_t clock, uint32_t sample_rate)
{
    uint32_t i;
    int32_t gain = 0;
    double out;

    memset(&PSG, 0, sizeof(PSG));

    switch (machine)
    {
        default:
        case 0: /* Sega VDP PSG: Master System / Mark III */
            PSG.m_feedback_mask = 0x8000;
            PSG.m_whitenoise_tap1 = 0x01;
            PSG.m_whitenoise_tap2 = 0x08;
            PSG.m_negate = 1;
            PSG.m_stereo = 0;
            PSG.m_clock_divider = 8;
            PSG.m_ncr_style_psg = 0;
            PSG.m_sega_style_psg = 1;
            break;

        case 1: /* Game Gear PSG: Sega VDP PSG plus stereo register */
            PSG.m_feedback_mask = 0x8000;
            PSG.m_whitenoise_tap1 = 0x01;
            PSG.m_whitenoise_tap2 = 0x08;
            PSG.m_negate = 1;
            PSG.m_stereo = 1;
            PSG.m_clock_divider = 8;
            PSG.m_ncr_style_psg = 0;
            PSG.m_sega_style_psg = 1;
            break;

        case 2: /* ColecoVision / TI SN76489 */
            PSG.m_feedback_mask = 0x4000;
            PSG.m_whitenoise_tap1 = 0x01;
            PSG.m_whitenoise_tap2 = 0x02;
            PSG.m_negate = 1;
            PSG.m_stereo = 0;
            PSG.m_clock_divider = 8;
            PSG.m_ncr_style_psg = 0;
            PSG.m_sega_style_psg = 0;
            break;

        case 3: /* SG-1000 / SC-3000 / TI SN76489A-style */
            PSG.m_feedback_mask = 0x10000;
            PSG.m_whitenoise_tap1 = 0x04;
            PSG.m_whitenoise_tap2 = 0x08;
            PSG.m_negate = 0;
            PSG.m_stereo = 0;
            PSG.m_clock_divider = 8;
            PSG.m_ncr_style_psg = 0;
            PSG.m_sega_style_psg = 0;
            break;
    }

    for (i = 0; i < 4; i++)
        PSG.m_volume[i] = 0;

    PSG.m_last_register = PSG.m_sega_style_psg ? 3 : 0;
    for (i = 0; i < 8; i += 2)
    {
        PSG.m_register[i] = 0;
        PSG.m_register[i + 1] = PSG.m_sega_style_psg ? 0x0f : 0x00;
    }

    for (i = 0; i < 4; i++)
    {
        PSG.m_output[i] = 0;
        PSG.m_period[i] = (PSG.m_sega_style_psg || i == 3) ? 0 : 0x400;
        PSG.m_count[i] = 0.0;
    }

    PSG.m_RNG = PSG.m_feedback_mask;
    PSG.m_output[3] = PSG.m_RNG & 1;

    PSG.m_cycles_to_ready = 1;
    PSG.m_stereo_mask = 0xff;
    PSG.m_ready_state = 1;

    gain &= 0xff;
    /* SMS Plus GX applies option.soundlevel in the final mixer.  MAME's
     * original per-channel table is MAX_OUTPUT/4, but that can overflow this
     * 16-bit mixer at the default level 2 when several channels are high.
     * Keep MAME's 2 dB ladder and relative levels, but leave mixer headroom. */
    out = MAX_OUTPUT / 8.0;
    while (gain-- > 0)
        out *= 1.023292992; /* 0.2 dB */

    for (i = 0; i < 15; i++)
    {
        PSG.m_vol_table[i] = (out > (MAX_OUTPUT / 8.0)) ? (MAX_OUTPUT / 8) : (int32_t)out;
        out /= 1.258925412; /* 2 dB */
    }
    PSG.m_vol_table[15] = 0;
    for (i = 0; i < 4; i++)
        PSG.m_volume[i] = PSG.m_vol_table[PSG.m_register[i * 2 + 1] & 0x0f];

    /* MAME clocks the internal PSG stream at clock/2, then applies the chip's
     * variant divider (8 for SN76489/Sega PSG).  This gives the usual
     * clock/(32*N) tone frequency for the SMS/GG PSG. */
    if (sample_rate == 0 || PSG.m_clock_divider == 0)
        PSG.m_clocks_per_sample = 0.0;
    else
        PSG.m_clocks_per_sample = ((double)clock / 2.0) /
                                  ((double)PSG.m_clock_divider * (double)sample_rate);
}

void SN76489_GGStereoWrite(uint8_t st)
{
    if (PSG.m_stereo)
        PSG.m_stereo_mask = st;
}

void SN76489_Write(uint8_t data)
{
    int32_t n, r, c;

    PSG.m_cycles_to_ready = 1;

    if (data & 0x80)
    {
        r = (data & 0x70) >> 4;
        PSG.m_last_register = (uint8_t)r;
        if (PSG.m_ncr_style_psg && r == 6 && ((data & 0x04) != (PSG.m_register[6] & 0x04)))
            PSG.m_RNG = PSG.m_feedback_mask;
        PSG.m_register[r] = (PSG.m_register[r] & 0x3f0) | (data & 0x0f);
    }
    else
    {
        r = PSG.m_last_register;
        if (PSG.m_ncr_style_psg && ((r & 1) || (r == 6)))
            return;
    }

    c = r >> 1;
    switch (r)
    {
        case 0:
        case 2:
        case 4:
            if ((data & 0x80) == 0)
                PSG.m_register[r] = (PSG.m_register[r] & 0x0f) | ((data & 0x3f) << 4);

            if ((PSG.m_register[r] != 0) || PSG.m_sega_style_psg)
                PSG.m_period[c] = PSG.m_register[r];
            else
                PSG.m_period[c] = 0x400;

            if (r == 4 && ((PSG.m_register[6] & 0x03) == 0x03))
                PSG.m_period[3] = PSG.m_period[2] << 1;
            break;

        case 1:
        case 3:
        case 5:
        case 7:
            PSG.m_volume[c] = PSG.m_vol_table[data & 0x0f];
            if ((data & 0x80) == 0)
                PSG.m_register[r] = (PSG.m_register[r] & 0x3f0) | (data & 0x0f);
            break;

        case 6:
            if ((data & 0x80) == 0)
                PSG.m_register[r] = (PSG.m_register[r] & 0x3f0) | (data & 0x0f);
            n = PSG.m_register[6];
            PSG.m_period[3] = ((n & 3) == 3) ? (PSG.m_period[2] << 1) : (1 << (5 + (n & 3)));
            if (!PSG.m_ncr_style_psg)
                PSG.m_RNG = PSG.m_feedback_mask;
            PSG.m_output[3] = PSG.m_RNG & 1;
            break;
    }
}

static void step_generators(double clocks)
{
    int i;
    if (clocks <= 0.0)
        return;

    countdown_cycles();

    for (i = 0; i < 3; i++)
    {
        PSG.m_count[i] -= clocks;
        while (PSG.m_count[i] <= 0.0)
        {
            PSG.m_output[i] ^= 1;
            if (PSG.m_period[i] <= 0)
            {
                PSG.m_count[i] = 1.0;
                break;
            }
            PSG.m_count[i] += (double)PSG.m_period[i];
        }
    }

    PSG.m_count[3] -= clocks;
    while (PSG.m_count[3] <= 0.0)
    {
        int tap1 = (PSG.m_RNG & PSG.m_whitenoise_tap1) != 0;
        int tap2;
        uint32_t feedback;

        if (PSG.m_ncr_style_psg)
            tap2 = ((PSG.m_RNG & PSG.m_whitenoise_tap2) != PSG.m_whitenoise_tap2);
        else
            tap2 = (PSG.m_RNG & PSG.m_whitenoise_tap2) != 0;

        feedback = (tap1 != (tap2 && in_noise_mode())) ? PSG.m_feedback_mask : 0;
        PSG.m_RNG >>= 1;
        PSG.m_RNG |= feedback;
        PSG.m_output[3] = PSG.m_RNG & 1;

        if (PSG.m_period[3] <= 0)
        {
            PSG.m_count[3] = 1.0;
            break;
        }
        PSG.m_count[3] += (double)PSG.m_period[3];
    }
}

void SN76489_Update(int16_t **outputs, int samples)
{
    int16_t *lbuffer = outputs[0];
    int16_t *rbuffer = PSG.m_stereo ? outputs[1] : outputs[0];

    while (samples-- > 0)
    {
        int32_t out = 0;
        int32_t out2 = 0;

        step_generators(PSG.m_clocks_per_sample);

        if (PSG.m_stereo)
        {
            out = ((((PSG.m_stereo_mask & 0x10) != 0) && PSG.m_output[0]) ? PSG.m_volume[0] : 0)
                + ((((PSG.m_stereo_mask & 0x20) != 0) && PSG.m_output[1]) ? PSG.m_volume[1] : 0)
                + ((((PSG.m_stereo_mask & 0x40) != 0) && PSG.m_output[2]) ? PSG.m_volume[2] : 0)
                + ((((PSG.m_stereo_mask & 0x80) != 0) && PSG.m_output[3]) ? PSG.m_volume[3] : 0);

            out2 = ((((PSG.m_stereo_mask & 0x01) != 0) && PSG.m_output[0]) ? PSG.m_volume[0] : 0)
                 + ((((PSG.m_stereo_mask & 0x02) != 0) && PSG.m_output[1]) ? PSG.m_volume[1] : 0)
                 + ((((PSG.m_stereo_mask & 0x04) != 0) && PSG.m_output[2]) ? PSG.m_volume[2] : 0)
                 + ((((PSG.m_stereo_mask & 0x08) != 0) && PSG.m_output[3]) ? PSG.m_volume[3] : 0);
        }
        else
        {
            out = (PSG.m_output[0] ? PSG.m_volume[0] : 0)
                + (PSG.m_output[1] ? PSG.m_volume[1] : 0)
                + (PSG.m_output[2] ? PSG.m_volume[2] : 0)
                + (PSG.m_output[3] ? PSG.m_volume[3] : 0);
            out2 = out;
        }

        if (PSG.m_negate)
        {
            out = -out;
            out2 = -out2;
        }

        *(lbuffer++) = (int16_t)out;
        if (PSG.m_stereo)
            *(rbuffer++) = (int16_t)out2;
    }
}
