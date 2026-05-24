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

/*--------------------------------------------------------------------------*/
/* Colecovision port handlers                                               */
/*--------------------------------------------------------------------------*/
void coleco_port_w(uint16_t port, uint8_t data)
{
	/* A7 is used as enable input */
	/* A6 & A5 are used to decode the address */
	switch(port & 0xE0)
	{
		case 0x80:
			coleco.pio_mode = 0;
		return;
		case 0xa0:
			tms_write(port,data);
		return;
		case 0xc0:
			coleco.pio_mode = 1;
		return;
		case 0xe0:
			psg_write(data);
		return;
		default:
		return;
	}
}

uint8_t  coleco_port_r(uint16_t port)
{
	/* A7 is used as enable input */
	/* A6 & A5 are used to decode the address */
	switch(port & 0xE0)
	{
		case 0xa0:
			return vdp_read(port);
		case 0xe0:
			return coleco_pio_r((port>>1)&1);
		default:
			return 0xff;
	}
}

/*--------------------------------------------------------------------------*/
/* Sord M5 port handlers                                                    */
/*--------------------------------------------------------------------------*/
#ifdef SORDM5_EMU

typedef struct
{
	uint8_t vector;
	uint8_t control[4];
	uint8_t time_constant[4];
	uint8_t waiting_for_time_constant[4];
	uint8_t interrupt_enabled[4];
	uint8_t interrupt_pending;
} sordm5_ctc_t;

static sordm5_ctc_t sordm5_ctc;

void sordm5_ctc_reset(void)
{
	memset(&sordm5_ctc, 0, sizeof(sordm5_ctc));
}

static void sordm5_ctc_w(uint8_t channel, uint8_t data)
{
	channel &= 3;

	/*
	 * MAME routes the M5 VDP interrupt through a Z80 CTC daisy device,
	 * not directly to the CPU.  This lightweight CTC model implements the
	 * programming visible to BIOS/cart games: vector writes, control words,
	 * time-constant latching, and channel interrupt vectors.  It intentionally
	 * does not synthesize the unused timer/counter channels.
	 */
	if (sordm5_ctc.waiting_for_time_constant[channel])
	{
		sordm5_ctc.time_constant[channel] = data;
		sordm5_ctc.waiting_for_time_constant[channel] = 0;
		return;
	}

	if (!(data & 0x01))
	{
		/* Interrupt vector word; channel number supplies bits 1-2. */
		sordm5_ctc.vector = data & 0xF8;
		return;
	}

	sordm5_ctc.control[channel] = data;
	sordm5_ctc.interrupt_enabled[channel] = (data & 0x80) ? 1 : 0;
	if (data & 0x02)
	{
		/* Reset command clears pending interrupt for this channel. */
		sordm5_ctc.interrupt_pending &= ~(1 << channel);
		if (!sordm5_ctc.interrupt_pending)
			z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
	}
	if (data & 0x04)
		sordm5_ctc.waiting_for_time_constant[channel] = 1;
}

static uint8_t sordm5_ctc_r(uint8_t channel)
{
	channel &= 3;
	return sordm5_ctc.time_constant[channel];
}

void sordm5_ctc_vdp_interrupt(void)
{
	/* MAME connects the TMS9928A INT callback to CTC TRG3. */
	if (sordm5_ctc.interrupt_enabled[3])
	{
		sordm5_ctc.interrupt_pending |= (1 << 3);
		z80_set_irq_line(INPUT_LINE_IRQ0, ASSERT_LINE);
	}
}

int32_t sordm5_ctc_irq_callback(void)
{
	uint8_t channel;

	for (channel = 0; channel < 4; channel++)
	{
		if (sordm5_ctc.interrupt_pending & (1 << channel))
		{
			sordm5_ctc.interrupt_pending &= ~(1 << channel);
			if (!sordm5_ctc.interrupt_pending)
				z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
			return (sordm5_ctc.vector | (channel << 1)) & 0xFF;
		}
	}

	z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
	return 0xFF;
}

static uint8_t sordm5_keyboard_r(uint8_t row)
{
	uint8_t r = row & 0x07;
	uint8_t temp = 0x00;

	if (r < SORDM5_KEY_ROWS)
		temp |= input.m5_key[r];

	/*
	 * Compatibility bridge for frontends that only know about gamepad-style
	 * SMS Plus GX input.  New frontends should drive input.m5_key[] directly
	 * so full M5 keyboard state is available to software.
	 */
	switch(r)
	{
		case 0x00:
			if(input.pad[0] & INPUT_BUTTON1) temp |= 0x40;  /* Space */
			if(input.pad[0] & INPUT_BUTTON2) temp |= 0x80;  /* Enter */
		break;
		case 0x01:
			if(input.system & INPUT_START) temp |= 0x01;  /* 1 key */
			if(input.system & INPUT_PAUSE) temp |= 0x02;  /* 2 key */
		break;
		case 0x05:
			if(input.pad[0] & INPUT_DOWN) temp |= 0x20;
		break;
		case 0x06:
			if(input.pad[0] & INPUT_UP)    temp |= 0x04;
			if(input.pad[0] & INPUT_LEFT)  temp |= 0x20;
			if(input.pad[0] & INPUT_RIGHT) temp |= 0x40;
		break;
	}

	return temp;
}

static uint8_t sordm5_joy_r(void)
{
	/* M5 joystick port is active-high: bit set means direction pressed. */
	uint8_t temp = 0x00;

	if(input.pad[0] & INPUT_RIGHT) temp |= 0x01;
	if(input.pad[0] & INPUT_UP)    temp |= 0x02;
	if(input.pad[0] & INPUT_LEFT)  temp |= 0x04;
	if(input.pad[0] & INPUT_DOWN)  temp |= 0x08;

	if(input.pad[1] & INPUT_RIGHT) temp |= 0x10;
	if(input.pad[1] & INPUT_UP)    temp |= 0x20;
	if(input.pad[1] & INPUT_LEFT)  temp |= 0x40;
	if(input.pad[1] & INPUT_DOWN)  temp |= 0x80;

	return temp;
}

void sordm5_port_w(uint16_t port, uint8_t data)
{
	port &= 0xFF;

	/* MAME m5_io: global mask $ff, mirrored in 16-byte blocks. */
	switch (port & 0xF0)
	{
		case 0x00:
			sordm5_ctc_w(port & 0x03, data);
			return;
		case 0x10:
			tms_write(0x10 | (port & 0x01), data);
			return;
		case 0x20:
			psg_write(data);
			return;
		case 0x30:
			/* $30 also pages 64KBF RAM carts in MAME; standard ROM carts ignore it. */
			return;
		case 0x40:
			/* Centronics data latch; no printer attached in this lightweight model. */
			return;
		case 0x50:
			/* COM: cassette output/remote and centronics strobe; ignored headlessly. */
			return;
		default:
			return;
	}
}

uint8_t sordm5_port_r(uint16_t port)
{
	port &= 0xFF;

	switch (port & 0xF0)
	{
		case 0x00:
			return sordm5_ctc_r(port & 0x03);
		case 0x10:
			return vdp_read(0x10 | (port & 0x01));
		case 0x30:
		{
			uint8_t row = port & 0x07;
			if (row == 0x07)
				return sordm5_joy_r();
			return sordm5_keyboard_r(row);
		}
		case 0x50:
			/* MAME sts_r(): bit 0 cassette input, bit 1 printer busy, bit 7 reset key. */
			return 0x01 | (input.m5_reset ? SORDM5_KEY_RESET : 0x00);
		case 0xA0:
			/* Some ported MSX games probe $a8-$ab; MAME maps them as noprw. */
			if ((port & 0x0C) == 0x08)
				return 0xFF;
			break;
	}

	return 0xFF;
}
#endif
