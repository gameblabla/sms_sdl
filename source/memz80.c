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
 *   Z80 memory handlers
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * March 15th 2019 : Fix some clang warnings.
 * March 13th 2019 : Partial revert due to CrabZ80. The switching to C99 datatypes had been done again.
 * March 7th 2019 : Clean up, plus switching more variables to c99 datatypes.
*/

#include "shared.h"
/*
 * Sord M5 CTC/IO behavior notes:
 * The lightweight CTC and keyboard/port model added for Sord M5 compatibility
 * follows MAME's src/mame/sord/m5.cpp hardware mapping and behavior, but is
 * an independent, reduced implementation for SMS Plus GX rather than copied
 * MAME device code.
 */


/* Pull-up resistors on data bus */
uint8_t data_bus_pullup = 0x00;
uint8_t data_bus_pulldown = 0x00;

/* Read unmapped memory */
uint8_t z80_read_unmapped(void)
{
	int32_t pc = Z80.pc.w.l;
	uint8_t data;
	pc = (pc - 1) & 0xFFFF;
	data = cpu_readmap[pc >> 13][pc & 0x03FF];
	return ((data | data_bus_pullup) & ~data_bus_pulldown);
}

/* Port $3E (Memory Control Port) */
static void memctrl_w (uint8_t data)
{
	/* detect CARTRIDGE/BIOS enabled/disabled */
	if (IS_SMS)
	{
		/* autodetect loaded BIOS ROM */
		if (!(bios.enabled & 2) && ((data & 0xE8) == 0xE8))
		{
			bios.enabled = option.use_bios | 2;
			memcpy(bios.rom, cart.rom, cart.size);
			memcpy(bios.fcr, cart.fcr, 4);
			bios.pages = cart.pages;
			cart.loaded = 0;
		}

		/* disables CART & BIOS by default */
		slot.rom = NULL;
		slot.mapper = MAPPER_NONE;

		switch (data & 0x48)
		{
			case 0x00:  /* BIOS & CART enabled */
			case 0x08:  /* BIOS disabled, CART enabled */
			if (cart.loaded)
			{
				slot.rom    = cart.rom;
				slot.pages  = cart.pages;
				slot.mapper = cart.mapper;
				slot.fcr    = &cart.fcr[0];
			}
			break;
			case 0x40:  /* BIOS enabled, CART disabled */
			slot.rom    = bios.rom;
			slot.pages  = bios.pages;
			slot.mapper = MAPPER_SEGA;
			slot.fcr    = &bios.fcr[0];
			break;
			default:
			break;
		}

		mapper_reset();

		/* reset SLOT mapping */
		if (slot.rom)
		{
			cpu_readmap[0]  = &slot.rom[0];
			if (slot.mapper != MAPPER_KOREA_MSX)
			{
				mapper_16k_w(0,slot.fcr[0]);
				mapper_16k_w(1,slot.fcr[1]);
				mapper_16k_w(2,slot.fcr[2]);
				mapper_16k_w(3,slot.fcr[3]);
			}
			else
			{
				mapper_8k_w(0,slot.fcr[0]);
				mapper_8k_w(1,slot.fcr[1]);
				mapper_8k_w(2,slot.fcr[2]);
				mapper_8k_w(3,slot.fcr[3]);
			}
		}
		else
		{
			uint8_t  i;
			for(i = 0x00; i <= 0x2F; i++)
			{
				cpu_readmap[i]  = dummy_read;
				cpu_writemap[i] = dummy_write;
			}
		}
	}
	
	/* update register value */
	sms.memctrl = data;  
}

/*--------------------------------------------------------------------------*/
/* Sega Master System port handlers                                         */
/*--------------------------------------------------------------------------*/
void sms_port_w(uint16_t port, uint8_t data)
{
	port &= 0xFF;
	/* access FM unit */
	if(port >= 0xF0)
	{
		switch(port)
		{
		case 0xF0:
			fmunit_write(0, data);
		return;
		case 0xF1:
			fmunit_write(1, data);
		return;
		case 0xF2:
			fmunit_detect_w(data);
		return;
		}
	}

	switch(port & 0xC1)
	{
		case 0x00:
			memctrl_w(data);
		return;
		case 0x01:
			pio_ctrl_w(data);
		return;
		case 0x40:
		case 0x41:
			psg_write(data);
		return;
		case 0x80:
		case 0x81:
			vdp_write(port, data);
		return;
		case 0xC0:
		case 0xC1:
		return;
	}
}

uint8_t sms_port_r(uint16_t port)
{
	port &= 0xFF;
	/* FM unit */
	if (port == 0xF2)
		return fmunit_detect_r() & pio_port_r(port);

	switch(port & 0xC0)
	{
		case 0x00:
		return z80_read_unmapped();
		case 0x40:
		return vdp_counter_r(port);
		case 0x80:
		return vdp_read(port);
		case 0xC0:
		return pio_port_r(port);
	}
	/* Just to please the compiler */
	return 0;
}


/*--------------------------------------------------------------------------*/
/* Sega System E port handlers                                              */
/*--------------------------------------------------------------------------*/

static uint8_t systeme_joy_r(int32_t port)
{
	uint8_t temp = 0xFF;
	uint8_t pad = input.pad[port & 1];

	if (pad & INPUT_UP)      temp &= ~0x01;
	if (pad & INPUT_DOWN)    temp &= ~0x02;
	if (pad & INPUT_LEFT)    temp &= ~0x04;
	if (pad & INPUT_RIGHT)   temp &= ~0x08;
	if (pad & INPUT_BUTTON1) temp &= ~0x10;
	if (pad & INPUT_BUTTON2) temp &= ~0x20;
	return temp;
}

static uint8_t systeme_input_e0_r(void)
{
	uint8_t temp = 0xFF;
	uint8_t arcade = input.arcade;

	/*
	 * Active-low Sega System E edge connector inputs.  Keep the old
	 * input.system aliases as a compatibility path for existing headless
	 * playback scripts made before dedicated arcade input bits existed.
	 */
	if (input.system & INPUT_PAUSE) arcade |= INPUT_ARCADE_COIN1;
	if (input.system & INPUT_START) arcade |= INPUT_ARCADE_START1;
	if (input.system & INPUT_RESET) arcade |= INPUT_ARCADE_START2;

	if (arcade & INPUT_ARCADE_COIN1)   temp &= ~0x01;
	if (arcade & INPUT_ARCADE_COIN2)   temp &= ~0x02;
	if (arcade & INPUT_ARCADE_TEST)    temp &= ~0x04;
	if (arcade & INPUT_ARCADE_SERVICE) temp &= ~0x08;
	if (arcade & INPUT_ARCADE_START1)  temp &= ~0x40;
	if (arcade & INPUT_ARCADE_START2)  temp &= ~0x80;
	return temp;
}

void systeme_port_w(uint16_t port, uint8_t data)
{
	port &= 0xFF;

	switch (port)
	{
		case 0x7B: /* VDP 1 PSG */
			psg_write_chip(0, data);
		return;

		case 0x7E: /* VDP 2 PSG */
		case 0x7F:
			psg_write_chip(1, data);
		return;

		case 0xBA: /* VDP 1/back-layer data */
		case 0xBB: /* VDP 1/back-layer control */
			systeme_vdp_write(0, port, data);
		return;

		case 0xBE: /* VDP 2/front-layer data */
		case 0xBF: /* VDP 2/front-layer control */
			systeme_vdp_write(1, port, data);
		return;

		case 0xF7:
			systeme_bank_w(data);
		return;

		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
			/* i8255/coin counters/analog controls: not needed by Tetris. */
		return;
	}
}

uint8_t systeme_port_r(uint16_t port)
{
	port &= 0xFF;

	switch (port)
	{
		case 0x7E:
			return systeme_vdp_counter_r(0);
		case 0x7F:
			return systeme_vdp_counter_r(1);

		case 0xBA:
		case 0xBB:
			return systeme_vdp_read(0, port);

		case 0xBE:
		case 0xBF:
			return systeme_vdp_read(1, port);

		case 0xE0:
			return systeme_input_e0_r();
		case 0xE1:
			return systeme_joy_r(0);
		case 0xE2:
			return systeme_joy_r(1);

		case 0xF2:
			return 0xFF;
		case 0xF3:
			return 0xFD; /* default Tetris DSWB: demo sounds on, normal difficulty */

		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
			return 0xFF;
	}

	return z80_read_unmapped();
}

/*--------------------------------------------------------------------------*/
/* Game Gear port handlers                                                  */
/*--------------------------------------------------------------------------*/

void gg_port_w(uint16_t port, uint8_t data)
{
	port &= 0xFF;
	if(port <= 0x20) {
		sio_w(port, data);
		return;
	}
	switch(port & 0xC1)
	{
		case 0x00:
			memctrl_w(data);
		return;
		case 0x01:
			pio_ctrl_w(data);
		return;
		case 0x40:
		case 0x41:
			psg_write(data);
		return;
		case 0x80:
		case 0x81:
			gg_vdp_write(port, data);
		return;
	}
}


uint8_t gg_port_r(uint16_t port)
{
	port &= 0xFF;
	if(port <= 0x20)
		return sio_r(port);

	switch(port & 0xC0)
	{
		case 0x00:
		return z80_read_unmapped();
		case 0x40:
		return vdp_counter_r(port);
		case 0x80:
		return vdp_read(port);

		case 0xC0:
		switch(port)
		{
			case 0xC0:
			case 0xC1:
			case 0xDC:
			case 0xDD:
			return pio_port_r(port);
		}
		return z80_read_unmapped();
	}

	/* Just to please the compiler */
	return 0;
}

/*--------------------------------------------------------------------------*/
/* Game Gear (MS) port handlers                                             */
/*--------------------------------------------------------------------------*/

void ggms_port_w(uint16_t port, uint8_t data)
{
	port &= 0xFF;
	if(port <= 0x20) 
	{
		sio_w(port, data);
		return;
	}

	switch(port & 0xC1)
	{
		case 0x00:
			memctrl_w(data);
		return;
		case 0x01:
			pio_ctrl_w(data);
		return;
		case 0x40:
		case 0x41:
			psg_write(data);
		return;
		case 0x80:
		case 0x81:
		vdp_write(port, data); /* fixed */
		return;
	}
}

uint8_t  ggms_port_r(uint16_t port)
{
	port &= 0xFF;
	if(port <= 0x20)
		return sio_r(port);
	switch(port & 0xC0)
	{
		case 0x00:
		return z80_read_unmapped();
		case 0x40:
		return vdp_counter_r(port);
		case 0x80:
		return vdp_read(port);
		case 0xC0:
		switch(port)
		{
			case 0xC0:
			case 0xC1:
			case 0xDC:
			case 0xDD:
			  return pio_port_r(port);
		}
		return z80_read_unmapped();
	}
	/* Just to please the compiler */
	return 0;
}

/*--------------------------------------------------------------------------*/
/* MegaDrive / Genesis port handlers                                        */
/*--------------------------------------------------------------------------*/

void md_port_w(uint16_t port, uint8_t data)
{
	switch(port & 0xC1)
	{
		case 0x00:
			/* No memory control register */
		return;
		case 0x01:
			pio_ctrl_w(data);
		return;
		case 0x40:
		case 0x41:
			psg_write(data);
		return;
		case 0x80:
		case 0x81:
			md_vdp_write(port, data);
		return;
	}
}


uint8_t  md_port_r(uint16_t port)
{
	switch(port & 0xC0)
	{
		case 0x00:
		return z80_read_unmapped();
		case 0x40:
		return vdp_counter_r(port);
		case 0x80:
		return vdp_read(port);
		case 0xC0:
		switch(port)
		{
			case 0xC0:
			case 0xC1:
			case 0xDC:
			case 0xDD:
			return pio_port_r(port);
		}
		return z80_read_unmapped();
	}
	/* Just to please the compiler */
	return 0;
}

/*--------------------------------------------------------------------------*/
/* SG1000,SC3000,SF7000 port handlers                                       */
/*--------------------------------------------------------------------------*/

void tms_port_w(uint16_t port, uint8_t data)
{
	switch(port & 0xC0)
	{
		case 0x40:
			psg_write(data);
		return;
		case 0x80:
			tms_write(port, data);
		return;
		default:
		return;
	}
}

uint8_t  tms_port_r(uint16_t port)
{
  switch(port & 0xC0)
  {
		case 0x80:
			return vdp_read(port);
		case 0xC0:
			return pio_port_r(port);
		default:
			return 0xff;
  }
}

/* ColecoVision port handlers live in source/platform/coleco/coleco.c. */

/* Sord M5 port handlers live in source/platform/sord_m5/sord_m5.c. */

