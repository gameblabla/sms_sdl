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
 * 93C46 serial EEPROM support for MultiRexZ80.
 *
 * Copyright (C) 2026 gameblabla
 *
 * This 93C46 mapper/eeprom implementation is largely derived from CrabEmu's
 * consoles/sms/mapper-93c46.c.
 *
 * CrabEmu 93C46 mapper implementation:
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2011, 2012 Lawrence Sebald
 *
 * CrabEmu is licensed under the GNU General Public License version 2.
 * MultiRexZ80 is distributed under the GNU General Public License; this file
 * remains compatible with that licensing.
 */

#include "shared.h"

eeprom93c46_t eeprom93c46;

#define LINE_DI 0x01
#define LINE_CLK 0x02
#define LINE_CS 0x04
#define LINE_DO 0x08

void eeprom93c46_init(void)
{
    memset(&eeprom93c46, 0, sizeof(eeprom93c46));
    for (int i = 0; i < EEPROM93C46_WORDS; i++) eeprom93c46.data[i] = 0xffff;
    eeprom93c46.do_value = LINE_DO;
}

void eeprom93c46_reset(void)
{
    uint16_t saved[EEPROM93C46_WORDS];
    memcpy(saved, eeprom93c46.data, sizeof(saved));
    memset(&eeprom93c46, 0, sizeof(eeprom93c46));
    memcpy(eeprom93c46.data, saved, sizeof(saved));
    eeprom93c46.do_value = LINE_DO;
}

int eeprom93c46_is_enabled(void)
{
    return eeprom93c46.enabled != 0;
}

uint8_t eeprom93c46_control_read(void)
{
    return eeprom93c46.enabled ? 0x08 : 0x00;
}

void eeprom93c46_control_write(uint8_t data)
{
    if (data & 0x80) eeprom93c46_reset();
    eeprom93c46.enabled = (data & 0x08) ? 1 : 0;
}

static void finish_command(uint8_t cmd)
{
    uint8_t op = (cmd >> 6) & 3;
    uint8_t addr = cmd & 0x3f;
    uint8_t sub = (cmd >> 4) & 3;

    eeprom93c46.reading = 0;

    switch (op)
    {
        case 0: /* extended commands: EWDS/WRAL/ERAL/EWEN */
            if (sub == 0) eeprom93c46.write_enabled = 0;
            else if (sub == 3) eeprom93c46.write_enabled = 1;
            else if (eeprom93c46.write_enabled && sub == 2)
            {
                for (int i = 0; i < EEPROM93C46_WORDS; i++) eeprom93c46.data[i] = 0xffff;
                sms.save = 1;
            }
            break;
        case 1: /* WRITE: next 16 data bits */
            eeprom93c46.opcode = cmd;
            eeprom93c46.shift = 0;
            eeprom93c46.bit_count = 0;
            break;
        case 2: /* READ */
            eeprom93c46.shift = eeprom93c46.data[addr];
            eeprom93c46.bit_count = 0;
            eeprom93c46.reading = 1;
            eeprom93c46.do_value = (eeprom93c46.shift & 0x8000) ? LINE_DO : 0;
            break;
        case 3: /* ERASE */
            if (eeprom93c46.write_enabled)
            {
                eeprom93c46.data[addr] = 0xffff;
                sms.save = 1;
            }
            break;
    }
}

uint8_t eeprom93c46_read(void)
{
    if (!eeprom93c46.enabled) return 0xff;
    return (uint8_t)((eeprom93c46.lines & (LINE_CS | LINE_CLK)) | ((eeprom93c46.do_value & LINE_DO) >> 3));
}

void eeprom93c46_write(uint8_t data)
{
    uint8_t old = eeprom93c46.lines;
    uint8_t rising = ((old & LINE_CLK) == 0) && (data & LINE_CLK);
    uint8_t cs = data & LINE_CS;
    uint8_t di = data & LINE_DI;

    eeprom93c46.lines = data & (LINE_DI | LINE_CLK | LINE_CS);

    if (!eeprom93c46.enabled) return;
    if (!cs)
    {
        eeprom93c46.bit_count = 0;
        eeprom93c46.opcode = 0;
        eeprom93c46.reading = 0;
        eeprom93c46.shift = 0;
        eeprom93c46.do_value = LINE_DO;
        return;
    }
    if (!rising) return;

    if (eeprom93c46.reading)
    {
        eeprom93c46.shift <<= 1;
        eeprom93c46.bit_count++;
        eeprom93c46.do_value = (eeprom93c46.shift & 0x8000) ? LINE_DO : 0;
        return;
    }

    if (eeprom93c46.opcode && ((eeprom93c46.opcode >> 6) & 3) == 1)
    {
        eeprom93c46.shift = (uint16_t)((eeprom93c46.shift << 1) | (di ? 1 : 0));
        if (++eeprom93c46.bit_count == 16)
        {
            if (eeprom93c46.write_enabled)
            {
                eeprom93c46.data[eeprom93c46.opcode & 0x3f] = eeprom93c46.shift;
                sms.save = 1;
            }
            eeprom93c46.opcode = 0;
            eeprom93c46.bit_count = 0;
            eeprom93c46.shift = 0;
        }
        return;
    }

    /* Wait for the start bit, then collect the 8 command bits. */
    if (eeprom93c46.bit_count == 0)
    {
        if (!di) return;
        eeprom93c46.bit_count = 1;
        eeprom93c46.shift = 0;
        return;
    }

    eeprom93c46.shift = (uint16_t)((eeprom93c46.shift << 1) | (di ? 1 : 0));
    if (++eeprom93c46.bit_count == 9)
    {
        uint8_t cmd = (uint8_t)eeprom93c46.shift;
        eeprom93c46.bit_count = 0;
        eeprom93c46.shift = 0;
        finish_command(cmd);
    }
}

uint8_t eeprom93c46_direct_read(uint16_t address)
{
    uint16_t offs = (uint16_t)(address - 0x8008);
    uint8_t word = (uint8_t)(offs >> 1);
    if (word >= EEPROM93C46_WORDS) return 0xff;
    return (offs & 1) ? (uint8_t)(eeprom93c46.data[word] >> 8) : (uint8_t)(eeprom93c46.data[word] & 0xff);
}

void eeprom93c46_direct_write(uint16_t address, uint8_t data)
{
    uint16_t offs = (uint16_t)(address - 0x8008);
    uint8_t word = (uint8_t)(offs >> 1);
    if (word >= EEPROM93C46_WORDS) return;
    if (offs & 1) eeprom93c46.data[word] = (uint16_t)((eeprom93c46.data[word] & 0x00ff) | (data << 8));
    else eeprom93c46.data[word] = (uint16_t)((eeprom93c46.data[word] & 0xff00) | data);
    sms.save = 1;
}

void eeprom93c46_load_from_sram(const uint8_t *sram)
{
    int all_zero = 1;
    for (int i = 0; i < EEPROM93C46_BYTES; i++)
    {
        if (sram[i] != 0x00)
        {
            all_zero = 0;
            break;
        }
    }

    /* Ports historically clear absent save RAM to 00.  A fresh 93C46 is erased
     * to FF, so keep the erased default when no EEPROM image appears present. */
    if (all_zero)
    {
        for (int i = 0; i < EEPROM93C46_WORDS; i++) eeprom93c46.data[i] = 0xffff;
        return;
    }

    for (int i = 0; i < EEPROM93C46_WORDS; i++)
        eeprom93c46.data[i] = (uint16_t)(sram[i * 2] | (sram[i * 2 + 1] << 8));
}

void eeprom93c46_save_to_sram(uint8_t *sram)
{
    memset(sram, 0, 0x8000);
    for (int i = 0; i < EEPROM93C46_WORDS; i++)
    {
        sram[i * 2] = (uint8_t)(eeprom93c46.data[i] & 0xff);
        sram[i * 2 + 1] = (uint8_t)(eeprom93c46.data[i] >> 8);
    }
}
