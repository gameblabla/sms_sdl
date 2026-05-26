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
 *   Sega Master System manager
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * March 13th 2019 : Minor fixes as part of the CrabZ80's revert. (mostly whitepacing but the TMS code was also broken to some extent)
 * March 7th 2019 : Some whitepacing and changing variables to c99 datatypes.
 * Feb 19th 2019 : Minor whitepacing fix.
 * August 12th 2018 : Minor fixes. (mostly changing variables to c99 datatypes and whitepacing)
*/

#include "shared.h"

bitmap_t bitmap;
cart_t cart;
input_t input;

extern int32_t z80_cycle_count;

int32_t system_cycles_per_line(void)
{
	if (sms.console == CONSOLE_SYSTEME) return SYSTEME_CYCLES_PER_LINE;
	if (sms.console == CONSOLE_SYSTEM1) return SYSTEM1_CYCLES_PER_LINE;
	if (sms.console == CONSOLE_SNKPSYCHOS) return SNK_PSYCHOS_CYCLES_PER_LINE;
	return CYCLES_PER_LINE;
}

int32_t system_hcounter_index(void)
{
	int32_t cycles_per_line = system_cycles_per_line();
	int32_t dot = z80_get_elapsed_cycles() % cycles_per_line;

	if (sms.console == CONSOLE_SYSTEME)
		dot = (dot * CYCLES_PER_LINE) / SYSTEME_CYCLES_PER_LINE;
	else if (sms.console == CONSOLE_SYSTEM1)
		dot = (dot * CYCLES_PER_LINE) / SYSTEM1_CYCLES_PER_LINE;
	else if (sms.console == CONSOLE_SNKPSYCHOS)
		dot = (dot * CYCLES_PER_LINE) / SNK_PSYCHOS_CYCLES_PER_LINE;

	if (dot < 0) dot = 0;
	if (dot >= CYCLES_PER_LINE) dot = CYCLES_PER_LINE - 1;
	return dot;
}

static void lightgun_update_dpad_cursor(void)
{
	int32_t port;
	int32_t max_y = (vdp.height > 0) ? (vdp.height - 1) : 191;
	int32_t speed = option.lightgun_dpad_speed > 0 ? option.lightgun_dpad_speed : 3;

	for (port = 0; port < 2; port++)
	{
		if (sms.device[port] != DEVICE_LIGHTGUN) continue;

		int32_t x = input.analog[port][0];
		int32_t y = input.analog[port][1];
		int32_t step = (input.pad[port] & INPUT_BUTTON2) ? speed * 2 : speed;

		if (input.pad[port] & INPUT_LEFT)  x -= step;
		if (input.pad[port] & INPUT_RIGHT) x += step;
		if (input.pad[port] & INPUT_UP)    y -= step;
		if (input.pad[port] & INPUT_DOWN)  y += step;

		if (x < 0) x = 0;
		if (x > 255) x = 255;
		if (y < 0) y = 0;
		if (y > max_y) y = max_y;
		input.analog[port][0] = x;
		input.analog[port][1] = y;
	}
}

/* Run the virtual console emulation for one frame */
void system_frame(uint32_t skip_render)
{
	int32_t iline = 0, line_z80 = 0;
	const int32_t cycles_per_line = system_cycles_per_line();

	if (sms.console == CONSOLE_SYSTEM1)
	{
		system1_frame(skip_render);
		return;
	}

	if (sms.console == CONSOLE_SNKPSYCHOS)
	{
		snk_psychos_frame(skip_render);
		return;
	}

	/* Debounce pause key.  Arcade drivers use dedicated coin/service/start bits;
	 * do not assert the SMS pause/NMI path while an arcade game is running. */
	if((sms.console != CONSOLE_SYSTEME) && (sms.console != CONSOLE_SYSTEM1) && (sms.console != CONSOLE_SNKPSYCHOS) && (input.system & INPUT_PAUSE))
	{
		if(!sms.paused)
		{
			sms.paused = 1;
			CPUIRQ_Pause();
		}
	}
	else
	{
		sms.paused = 0;
	}

	/* Reset TMS Text offset counter */
	text_counter = 0;

	/* Light Phaser d-pad fallback for ports without mouse/touch input. */
	lightgun_update_dpad_cursor();

	/* 3D glasses faking */
	if (sms.glasses_3d) skip_render = sms.wram[0x1ffb];

	/* VDP register 9 is latched during VBLANK */
	vdp.vscroll = vdp.reg[9];

	/* Reload Horizontal Interrupt counter */
	vdp.left = vdp.reg[0x0A];

	/* Reset collision flag infos */
	vdp.spr_col = 0xff00;
	if (sms.console == CONSOLE_SYSTEME) systeme_vdp_frame_start();

	/* Line processing */
	for(vdp.line = 0; vdp.line < vdp.lpf; vdp.line++)
	{
		if (sms.console == CONSOLE_SYSTEME) systeme_vdp_set_line(vdp.line);
		iline = vdp.height;

		/* VDP line rendering */
		if(!skip_render) render_line(vdp.line);

		/* Horizontal Interrupt */
		if (sms.console >= CONSOLE_SMS)
		{
			if(vdp.line <= iline)
			{
				if(--vdp.left < 0)
				{
					vdp.left = vdp.reg[0x0A];
					vdp.hint_pending = 1;
					if(vdp.reg[0x00] & 0x10)
					{
						/* IRQ line is latched between instructions, on instruction last cycle          */
						/* This means that if Z80 cycle count is exactly a multiple of CYCLES_PER_LINE, */
						/* interrupt should be triggered AFTER the next instruction.                    */
						if (!(z80_get_elapsed_cycles()%cycles_per_line))
							z80_execute(1);
						z80_set_irq_line(0, ASSERT_LINE);
					}
				}
				if(sms.console == CONSOLE_SYSTEME && --vdp2.left < 0)
				{
					vdp2.left = vdp2.reg[0x0A];
					vdp2.hint_pending = 1;
					if(vdp2.reg[0x00] & 0x10)
					{
						if (!(z80_get_elapsed_cycles()%cycles_per_line))
							z80_execute(1);
						z80_set_irq_line(0, ASSERT_LINE);
					}
				}
			}
		}

		/* Run Z80 CPU */
		line_z80 += cycles_per_line;
		z80_execute((line_z80 - z80_cycle_count));
#ifdef SORDM5_EMU
		if (sms.console == CONSOLE_SORDM5)
			sordm5_ctc_tick(cycles_per_line);
#endif
		
		/* Vertical Interrupt */
		if(vdp.line == iline)
		{
			vdp.status |= 0x80;
			vdp.vint_pending = 1;
			if (sms.console == CONSOLE_SYSTEME)
			{
				vdp2.status |= 0x80;
				vdp2.vint_pending = 1;
			}
			if(vdp.reg[0x01] & 0x20)
			{
				#ifdef SORDM5_EMU
				if (sms.console == CONSOLE_SORDM5)
					sordm5_ctc_vdp_interrupt();
				else
				#endif
					z80_set_irq_line(vdp.irq, ASSERT_LINE);
			}
			if((sms.console == CONSOLE_SYSTEME) && (vdp2.reg[0x01] & 0x20))
				z80_set_irq_line(vdp.irq, ASSERT_LINE);
		}

		/* Run sound chips */
		SMSPLUS_sound_update(vdp.line);
	}

	/* Adjust Z80 cycle count for next frame */
	z80_cycle_count -= line_z80;
}

void system_init(void)
{
	sms_init();
	pio_init();
	vdp_init();
	render_init();
	SMSPLUS_sound_init();
}

void system_shutdown(void)
{
	sms_shutdown();
	pio_shutdown();
	vdp_shutdown();
	render_shutdown();
	SMSPLUS_sound_shutdown();
	free_rom();
}

void system_reset(void)
{
	sms_reset();
	pio_reset();
	vdp_reset();
	render_reset();
	if (sms.console == CONSOLE_SYSTEM1) system1_reset();
	if (sms.console == CONSOLE_SNKPSYCHOS) snk_psychos_reset();
	SMSPLUS_sound_reset();
	system_manage_sram(cart.sram, SLOT_CART, SRAM_LOAD);
	if (cart.mapper == MAPPER_93C46) eeprom93c46_load_from_sram(cart.sram);
}


void system_poweron(void)
{
	system_init();
	system_reset();
}

void system_poweroff(void)
{
	if (cart.mapper == MAPPER_93C46)
	{
		eeprom93c46_save_to_sram(cart.sram);
		sms.save = 1;
	}
	system_manage_sram(cart.sram, SLOT_CART, SRAM_SAVE);
}
