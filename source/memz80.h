/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additionnal code by Eke-Eke (SMS Plus GX)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   Z80 Memory handlers
 *
 ******************************************************************************/

#ifndef MEMZ80_H_
#define MEMZ80_H_

#include "build_features.h"

/* Global data */
extern uint8_t data_bus_pullup;
extern uint8_t data_bus_pulldown;
extern uint8_t z80_data_operand_fetch;

/* Function prototypes */
extern uint8_t z80_read_unmapped(void);
extern void gg_port_w(uint16_t port, uint8_t data);
extern uint8_t gg_port_r(uint16_t port);
extern void ggms_port_w(uint16_t port, uint8_t data);
extern uint8_t ggms_port_r(uint16_t port);
extern void sms_port_w(uint16_t port, uint8_t data);
extern uint8_t sms_port_r(uint16_t port);
extern void systeme_port_w(uint16_t port, uint8_t data);
extern uint8_t systeme_port_r(uint16_t port);
extern void system1_port_w(uint16_t port, uint8_t data);
extern uint8_t system1_port_r(uint16_t port);
extern void smsj_port_w(uint16_t port, uint8_t data);
extern uint8_t smsj_port_r(uint16_t port);
extern void md_port_w(uint16_t port, uint8_t data);
extern uint8_t md_port_r(uint16_t port);
extern void tms_port_w(uint16_t port, uint8_t data);
extern uint8_t tms_port_r(uint16_t port);
#if SMSPLUS_ENABLE_COLECO
#include "platform/coleco/coleco.h"
#endif

#if SMSPLUS_ENABLE_SORDM5
#include "platform/sord_m5/sord_m5.h"
#endif

#endif /* _MEMZ80_H_ */
