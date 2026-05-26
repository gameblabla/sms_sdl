/*
 * 93C46 serial EEPROM support for SMS Plus GX.
 *
 * Copyright (C) 2026 SMS Plus GX contributors
 * Mapper behavior derived from CrabEmu's 93C46 mapper implementation.
 * Copyright (C) 2005-2012 Lawrence Sebald
 * Licensed under the GNU General Public License version 2 or compatible terms.
 */

#ifndef EEPROM93C46_H_
#define EEPROM93C46_H_

#include <stdint.h>

#define EEPROM93C46_BYTES 128
#define EEPROM93C46_WORDS 64

typedef struct
{
    uint16_t data[EEPROM93C46_WORDS];
    uint16_t shift;
    uint8_t lines;
    uint8_t bit_count;
    uint8_t opcode;
    uint8_t reading;
    uint8_t enabled;
    uint8_t write_enabled;
    uint8_t do_value;
} eeprom93c46_t;

extern eeprom93c46_t eeprom93c46;

void eeprom93c46_init(void);
void eeprom93c46_reset(void);
uint8_t eeprom93c46_read(void);
void eeprom93c46_write(uint8_t data);
void eeprom93c46_control_write(uint8_t data);
uint8_t eeprom93c46_control_read(void);
int eeprom93c46_is_enabled(void);
uint8_t eeprom93c46_direct_read(uint16_t address);
void eeprom93c46_direct_write(uint16_t address, uint8_t data);
void eeprom93c46_load_from_sram(const uint8_t *sram);
void eeprom93c46_save_to_sram(uint8_t *sram);

#endif
