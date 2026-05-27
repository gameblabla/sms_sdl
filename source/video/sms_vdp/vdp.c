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

/******************************************************************************
 *  Sega Master System / GameGear Emulator
 *  Copyright (C) 1998-2007  Charles MacDonald
 *
 *  additional code by Eke-Eke (SMS Plus GX)
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
 *  Video Display Processor (VDP) emulation.
 *
 ******************************************************************************/
/*
 * See git commit history for more information.
 * - Gameblabla
 * March 15th 2019 : Fix yet more issues with datatypes in vdp.c
 * March 14th 2019 : Fix issues as reported by Clang. (in vdp_write)
 * March 13th 2019 : Minor fixes as part of the CrabZ80's revert. (mostly whitepacing)
 * March 11th 2019 : Fixed scrolling issues with Gauntlet. Fixed PAL issues too.
 * March 9th 2019 : Set VDP register to 0xE0 after multiple testings against BIOSes. Fixes Sonic's Edusoft and i think California Games 2.
 * March 7th 2019 : Whitepacing and minor fixes.
*/

#include "shared.h"
#include "hvc.h"

/* Mark a pattern as dirty */
#define MARK_BG_DIRTY(addr) render_mark_bg_dirty_chip(0, (addr))


void vdp_vram_direct_write(uint16_t address, uint8_t data)
{
	int32_t index;
	int32_t cycles_per_line = system_cycles_per_line();

	if (((z80_get_elapsed_cycles() + 1) / cycles_per_line) > vdp.line)
	{
		render_line((vdp.line + 1) % vdp.lpf);
	}

	index = address & 0x3FFF;
	if (data != vdp.vram[index])
	{
		vdp.vram[index] = data;
		render_mark_bg_dirty_chip(0, index);
	}
}

/*** Vertical Counter Tables ***/
extern uint8_t *vc_table[2][3];

/* VDP context */
vdp_t vdp;
vdp_t *vdp2_ptr = NULL;

static uint8_t *systeme_vram_banks = NULL;
static uint8_t systeme_active_vram_bank[2];

#define SYSTEME_VRAM_BANK_SIZE 0x4000
#define SYSTEME_VRAM_BANK_COUNT 4

static uint8_t *systeme_vram_bank_ptr(int chip, int bank)
{
	return systeme_vram_banks + (((chip & 1) << 1) | (bank & 1)) * SYSTEME_VRAM_BANK_SIZE;
}

static int systeme_vdp_runtime_alloc(void)
{
	if (!vdp2_ptr)
	{
		vdp2_ptr = (vdp_t *)calloc(1, sizeof(vdp_t));
		if (!vdp2_ptr)
			return 0;
	}

	if (!systeme_vram_banks)
	{
		systeme_vram_banks = (uint8_t *)calloc(SYSTEME_VRAM_BANK_COUNT, SYSTEME_VRAM_BANK_SIZE);
		if (!systeme_vram_banks)
		{
			free(vdp2_ptr);
			vdp2_ptr = NULL;
			return 0;
		}
	}

	return 1;
}

static void systeme_vdp_runtime_free(void)
{
	if (systeme_vram_banks)
	{
		free(systeme_vram_banks);
		systeme_vram_banks = NULL;
	}
	if (vdp2_ptr)
	{
		free(vdp2_ptr);
		vdp2_ptr = NULL;
	}
}

static vdp_t *systeme_vdp_context(int chip)
{
	return chip ? vdp2_ptr : &vdp;
}

static void systeme_vdp_irq_refresh(void)
{
	int asserted = 0;

	if ((vdp.vint_pending && (vdp.reg[1] & 0x20)) ||
	    (vdp.hint_pending && (vdp.reg[0] & 0x10)) ||
	    (vdp2.vint_pending && (vdp2.reg[1] & 0x20)) ||
	    (vdp2.hint_pending && (vdp2.reg[0] & 0x10)))
		asserted = 1;

	z80_set_irq_line(vdp.irq, asserted ? ASSERT_LINE : CLEAR_LINE);
}

static void systeme_vdp_update_mode(vdp_t *ctx)
{
	int32_t m1 = (ctx->reg[1] >> 4) & 1;
	int32_t m3 = (ctx->reg[1] >> 3) & 1;
	int32_t m2 = (ctx->reg[0] >> 1) & 1;
	int32_t m4 = (ctx->reg[0] >> 2) & 1;
	ctx->mode = (m4 << 3 | m3 << 2 | m2 << 1 | m1 << 0);

	if (sms.console >= CONSOLE_SMS2)
	{
		switch (ctx->mode)
		{
			case 0x0B:
				ctx->height = 224;
				ctx->extended = 1;
				ctx->ntab = ((ctx->reg[2] << 10) & 0x3000) | 0x0700;
				break;
			case 0x0E:
				ctx->height = 240;
				ctx->extended = 2;
				ctx->ntab = ((ctx->reg[2] << 10) & 0x3000) | 0x0700;
				break;
			default:
				ctx->height = 192;
				ctx->extended = 0;
				ctx->ntab = (ctx->reg[2] << 10) & 0x3800;
				if ((ctx->mode & 0x0B) == 0x09) ctx->mode = 1;
				break;
		}
	}
	else
	{
		ctx->height = 192;
		ctx->extended = 0;
		ctx->ntab = (ctx->reg[2] << 10) & 0x3800;
		if ((ctx->mode & 0x09) == 0x09) ctx->mode = 1;
	}

	ctx->pn = (ctx->reg[2] << 10) & 0x3C00;
}

static void systeme_vdp_select_visible_bank(int chip, uint8_t bank)
{
	vdp_t *ctx = systeme_vdp_context(chip);
	uint8_t *src;

	if (!ctx || !systeme_vram_banks)
		return;

	bank &= 1;
	if (systeme_active_vram_bank[chip] == bank)
		return;

	src = systeme_vram_bank_ptr(chip, bank);
	systeme_active_vram_bank[chip] = bank;
	memcpy(ctx->vram, src, SYSTEME_VRAM_BANK_SIZE);
	render_invalidate_bg_cache();
}

void systeme_vdp_bank_w(uint8_t data)
{
	systeme_vdp_select_visible_bank(0, (data >> 7) & 1);
	systeme_vdp_select_visible_bank(1, (data >> 6) & 1);
}


/* Initialize VDP emulation */
void vdp_init(void)
{
  /* display area */
	if ((sms.console == CONSOLE_GG) && (!option.extra_gg))
	{
		bitmap.viewport.w = 160;
		bitmap.viewport.x = 48;
	}
	else
	{
		bitmap.viewport.w = 256;
		bitmap.viewport.x = 0;
	}

	/* number of scanlines */
	vdp.lpf = sms.display ? 313 : 262;

	/* reset viewport */
	viewport_check();
	bitmap.viewport.changed = 1;
}

void vdp_shutdown(void)
{
	systeme_vdp_runtime_free();
}

  
/* Reset VDP emulation */
void vdp_reset(void)
{
	/* reset VDP structure */
	memset(&vdp, 0, sizeof(vdp_t));
	if (sms.console != CONSOLE_SYSTEME)
		systeme_vdp_runtime_free();

	/* number of scanlines */
	vdp.lpf = sms.display ? 313 : 262;

	/* VDP registers default values (usually set by BIOS) */
	/* Tested on Megadrive and it does not initiliaze the VDP registers. */
	if ((sms.console != CONSOLE_SYSTEME) && IS_SMS && (bios.enabled != 3))
	{
		vdp.reg[0]  = 0x36; 
		vdp.reg[1]  = 0xE0;
		vdp.reg[2]  = 0xFF;
		vdp.reg[3]  = 0xFF;
		vdp.reg[4]  = 0xFF;
		vdp.reg[5]  = 0xFF;
		vdp.reg[6]  = 0xFB;
		vdp.reg[10] = 0xFF;
	}

	/* VDP interrupt */
	if (sms.console == CONSOLE_COLECO)
		vdp.irq = INPUT_LINE_NMI;
	else
		vdp.irq = INPUT_LINE_IRQ0;

	/* reset VDP viewport */
	viewport_check();

	/* reset VDP internals */
	vdp.ct    = (vdp.reg[3] <<  6) & 0x3FC0;
	vdp.pg    = (vdp.reg[4] << 11) & 0x3800;
	vdp.satb  = (vdp.reg[5] << 7) & 0x3F00;
	vdp.sa    = (vdp.reg[5] <<  7) & 0x3F80;
	vdp.sg    = (vdp.reg[6] << 11) & 0x3800;
	vdp.bd    = (vdp.reg[7] & 0x0F);

	bitmap.viewport.changed = 1;

	if (sms.console == CONSOLE_SYSTEME)
	{
		if (!systeme_vdp_runtime_alloc())
		{
			fprintf(stderr, "System E VDP allocation failed\n");
			abort();
		}
		memcpy(&vdp2, &vdp, sizeof(vdp_t));
		memset(systeme_vram_banks, 0, SYSTEME_VRAM_BANK_COUNT * SYSTEME_VRAM_BANK_SIZE);
		systeme_active_vram_bank[0] = 0;
		systeme_active_vram_bank[1] = 0;
		memcpy(systeme_vram_bank_ptr(0, 0), vdp.vram, SYSTEME_VRAM_BANK_SIZE);
		memcpy(systeme_vram_bank_ptr(1, 0), vdp2.vram, SYSTEME_VRAM_BANK_SIZE);
		render_invalidate_bg_cache();
	}
}


void viewport_check(void)
{
	int32_t i;
	int32_t m1 = (vdp.reg[1] >> 4) & 1;
	int32_t m3 = (vdp.reg[1] >> 3) & 1;
	int32_t m2 = (vdp.reg[0] >> 1) & 1;
	int32_t m4 = (vdp.reg[0] >> 2) & 1;
	vdp.mode = (m4 << 3 | m3 << 2 | m2 << 1 | m1 << 0);
	
	/* Check for extended modes */
	if (sms.console >= CONSOLE_SMS2)
	{
		switch (vdp.mode)
		{
		  case 0x0B:  /* Mode 4 extended (224 lines) */
			vdp.height = 224;
			vdp.extended = 1;
			vdp.ntab = ((vdp.reg[2] << 10) & 0x3000) | 0x0700;
			break;

		  case 0x0E:  /* Mode 4 extended (240 lines) */
			vdp.height = 240;
			vdp.extended = 2;
			vdp.ntab = ((vdp.reg[2] << 10) & 0x3000) | 0x0700;
			break;

		  default:  /* Mode 4 (192 lines) */
			vdp.height = 192;
			vdp.extended = 0;
			vdp.ntab = (vdp.reg[2] << 10) & 0x3800;

			/* invalid text mode (Mode 4) */
			if ((vdp.mode & 0x0B) == 0x09) vdp.mode = 1;
			break;
		}
	}
	else
	{
		/* always use Mode 4 (192 lines) */
		vdp.height = 192;
		vdp.extended = 0;
		vdp.ntab = (vdp.reg[2] << 10) & 0x3800;
		/* invalid text mode (Mode 4) */
		if ((vdp.mode & 0x09) == 0x09) vdp.mode = 1;
	}

	/* update display area */
	if ((sms.console != CONSOLE_GG) || option.extra_gg)
	{
		if(bitmap.viewport.h != vdp.height)
		{
			bitmap.viewport.oh = bitmap.viewport.h;
			bitmap.viewport.h = vdp.height;
			bitmap.viewport.changed = 1;
		}
	}
	else
	{
		/* GG display area is fixed */
		bitmap.viewport.h = 144;
	}

	/* update border area */
	bitmap.viewport.y = 0;

	/* check if this is switching in/out of tms */
	if (IS_SMS || IS_GG)
	{
		/* Restore palette */
		for(i = 0; i < PALETTE_SIZE; i++)
		{
			palette_sync(i);
		}
	}

	vdp.pn = (vdp.reg[2] << 10) & 0x3C00;

	if (vdp.mode & 8)
	{
		render_bg  = render_bg_sms;
		render_obj = render_obj_sms;
	}
	else
	{
		render_bg  = render_bg_tms;
		render_obj = render_obj_tms;
	}
}


static void vdp_reg_w(uint8_t r, uint8_t d)
{
	/* Store register data */
	vdp.reg[r] = d;
	switch(r)
	{
	case 0x00: /* Mode Control No. 1 */
		if(vdp.hint_pending)
		{
			if(d & 0x10) z80_set_irq_line(0, ASSERT_LINE);
			else z80_set_irq_line(0, CLEAR_LINE);
		}
		viewport_check();
	break;
    case 0x01: /* Mode Control No. 2 */
		if(vdp.vint_pending)
		{
			#ifdef SORDM5_EMU
			if (sms.console == CONSOLE_SORDM5)
			{
				if(d & 0x20) sordm5_ctc_vdp_interrupt();
			}
			else
			#endif
			{
				if(d & 0x20) z80_set_irq_line(vdp.irq, ASSERT_LINE);
				else z80_set_irq_line(vdp.irq, CLEAR_LINE);
			}
		}
		viewport_check();
	break;

    case 0x02: /* Name Table A Base Address */
		viewport_check();
	break;

    case 0x03:
		vdp.ct = (d <<  6) & 0x3FC0;
	break;

    case 0x04:
		vdp.pg = (d << 11) & 0x3800;
	break;

    case 0x05: /* Sprite Attribute Table Base Address */
		vdp.satb = (d << 7) & 0x3F00;
		vdp.sa = (d <<  7) & 0x3F80;
	break;

    case 0x06:
		vdp.sg = (d << 11) & 0x3800;
	break;

    case 0x07:
		vdp.bd = (d & 0x0F);
	break;
  }
}


static void systeme_vdp_reg_w(int chip, uint8_t r, uint8_t d)
{
	vdp_t *ctx = systeme_vdp_context(chip);

	ctx->reg[r] = d;
	switch(r)
	{
		case 0x00:
			if(ctx->hint_pending)
			{
				if(d & 0x10) z80_set_irq_line(vdp.irq, ASSERT_LINE);
				else systeme_vdp_irq_refresh();
			}
			if (chip == 0) viewport_check();
			else systeme_vdp_update_mode(ctx);
		break;

		case 0x01:
			if(ctx->vint_pending)
			{
				if(d & 0x20) z80_set_irq_line(vdp.irq, ASSERT_LINE);
				else systeme_vdp_irq_refresh();
			}
			if (chip == 0) viewport_check();
			else systeme_vdp_update_mode(ctx);
		break;

		case 0x02:
			if (chip == 0) viewport_check();
			else systeme_vdp_update_mode(ctx);
		break;

		case 0x03:
			ctx->ct = (d <<  6) & 0x3FC0;
		break;

		case 0x04:
			ctx->pg = (d << 11) & 0x3800;
		break;

		case 0x05:
			ctx->satb = (d << 7) & 0x3F00;
			ctx->sa = (d <<  7) & 0x3F80;
		break;

		case 0x06:
			ctx->sg = (d << 11) & 0x3800;
		break;

		case 0x07:
			ctx->bd = (d & 0x0F);
		break;
	}
}

void systeme_vdp_direct_write(uint8_t bank_select, uint16_t address, uint8_t data)
{
	int chip, bank;
	uint16_t index = address & 0x3FFF;
	uint8_t *bank_ptr;

	if (!systeme_vram_banks)
		return;

	/* Port F7 bits 5-7 select one of eight write windows.  This follows
	 * MAME's System E bank table: odd entries write VDP1/back VRAM and
	 * even entries write VDP2/front VRAM.  The selected write bank is the
	 * opposite polarity of the visible-bank bits latched by bits 7 and 6. */
	if (bank_select & 1)
	{
		chip = 0;
		bank = (bank_select & 4) ? 0 : 1;
	}
	else
	{
		chip = 1;
		bank = (bank_select & 2) ? 0 : 1;
	}

	bank_ptr = systeme_vram_bank_ptr(chip, bank);
	if (data == bank_ptr[index])
		return;

	if (((z80_get_elapsed_cycles() + 1) / system_cycles_per_line()) > vdp.line)
		render_line((vdp.line + 1) % vdp.lpf);

	bank_ptr[index] = data;
	if (systeme_active_vram_bank[chip] == bank)
	{
		vdp_t *ctx = systeme_vdp_context(chip);
		if (ctx)
		{
			ctx->vram[index] = data;
			render_mark_bg_dirty_chip(chip, index);
		}
	}
}

void systeme_vdp_write(int chip, int32_t offset, uint8_t data)
{
	vdp_t *ctx = systeme_vdp_context(chip);
	int32_t index;

	MULTIREXZ80_TRACE_VDP_WRITE(chip ? "systeme-vdp2" : "systeme-vdp1", offset, data);

	if (((z80_get_elapsed_cycles() + 1) / system_cycles_per_line()) > vdp.line)
		render_line((vdp.line + 1) % vdp.lpf);

	switch(offset & 1)
	{
		case 0:
			ctx->pending = 0;
			switch(ctx->code)
			{
				case 0:
				case 1:
				case 2:
					index = (ctx->addr & 0x3FFF);
					if(data != ctx->vram[index])
					{
						ctx->vram[index] = data;
						if (systeme_vram_banks) systeme_vram_bank_ptr(chip, systeme_active_vram_bank[chip])[index] = data;
						render_mark_bg_dirty_chip(chip, ctx->addr);
					}
					ctx->buffer = data;
				break;

				case 3:
					index = (ctx->addr & 0x1F);
					if(data != ctx->cram[index])
					{
						ctx->cram[index] = data;
						palette_sync_chip(chip, index);
					}
					ctx->buffer = data;
				break;
			}
			ctx->addr = (ctx->addr + 1) & 0x3FFF;
		return;

		case 1:
			if(ctx->pending == 0)
			{
				ctx->addr = (ctx->addr & 0x3F00) | (data & 0xFF);
				ctx->latch = data;
				ctx->pending = 1;
			}
			else
			{
				ctx->pending = 0;
				ctx->code = (data >> 6) & 3;
				ctx->addr = (data << 8 | ctx->latch) & 0x3FFF;

				if(ctx->code == 0)
				{
					ctx->buffer = ctx->vram[ctx->addr & 0x3FFF];
					ctx->addr = (ctx->addr + 1) & 0x3FFF;
				}

				if(ctx->code == 2)
				{
					uint8_t r = (data & 0x0F);
					uint8_t d = ctx->latch;
					systeme_vdp_reg_w(chip, r, d);
				}
			}
		return;
	}
}

static void systeme_vdp_update_vint_flag_for_now(vdp_t *ctx)
{
	int32_t cyc = z80_get_elapsed_cycles();
	int32_t line = (cyc / system_cycles_per_line()) % ctx->lpf;
	int32_t dot = cyc % system_cycles_per_line();
	int32_t flag_cycle = 25;

	if ((line == ctx->height) && (dot >= flag_cycle))
	{
		ctx->status |= 0x80;
		ctx->vint_pending = 1;
		if (ctx->reg[0x01] & 0x20)
			z80_set_irq_line(vdp.irq, ASSERT_LINE);
	}
}

uint8_t systeme_vdp_read(int chip, int32_t offset)
{
	vdp_t *ctx = systeme_vdp_context(chip);
	uint8_t temp;

	switch(offset & 1)
	{
		case 0:
			ctx->pending = 0;
			temp = ctx->buffer;
			ctx->buffer = ctx->vram[ctx->addr & 0x3FFF];
			ctx->addr = (ctx->addr + 1) & 0x3FFF;
			return temp;

		case 1:
			systeme_vdp_update_vint_flag_for_now(ctx);
			temp = ctx->status | 0x1f;
			ctx->status = 0;
			ctx->pending = 0;
			ctx->vint_pending = 0;
			ctx->hint_pending = 0;
			systeme_vdp_irq_refresh();
			return temp;
	}

	return 0;
}

uint8_t systeme_vdp_counter_r(int32_t offset)
{
	return vdp_counter_r(offset);
}

void systeme_vdp_frame_start(void)
{
	vdp2.vscroll = vdp2.reg[9];
	vdp2.left = vdp2.reg[0x0A];
	vdp2.spr_col = 0xff00;
}

void systeme_vdp_set_line(int32_t line)
{
	vdp2.line = line;
}


void vdp_write(int32_t offset, uint8_t data)
{
	MULTIREXZ80_TRACE_VDP_WRITE("sms", offset, data);
	int32_t index;
	if (((z80_get_elapsed_cycles() + 1) / system_cycles_per_line()) > vdp.line)
	{
		/* render next line now BEFORE updating register */
		render_line((vdp.line+1)%vdp.lpf);
	}

	switch(offset & 1)
	{
    case 0: /* Data port */

      vdp.pending = 0;

      switch(vdp.code)
      {
        case 0: /* VRAM write */
        case 1: /* VRAM write */
        case 2: /* VRAM write */
          index = (vdp.addr & 0x3FFF);
          if(data != vdp.vram[index])
          {
            vdp.vram[index] = data;
            MARK_BG_DIRTY(vdp.addr);
          }
          vdp.buffer = data;
          break;
    
        case 3: /* CRAM write */
          index = (vdp.addr & 0x1F);
          if(data != vdp.cram[index])
          {
            vdp.cram[index] = data;
            palette_sync(index);
          }
          vdp.buffer = data;
          break;
      }
      vdp.addr = (vdp.addr + 1) & 0x3FFF;
      return;

    case 1: /* Control port */
      if(vdp.pending == 0)
      {
        vdp.addr = (vdp.addr & 0x3F00) | (data & 0xFF);
        vdp.latch = data;
        vdp.pending = 1;
      }
      else
      {
        vdp.pending = 0;
        vdp.code = (data >> 6) & 3;
        vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;

        if(vdp.code == 0)
        {
          vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
          vdp.addr = (vdp.addr + 1) & 0x3FFF;
        }
    
        if(vdp.code == 2)
        {
          uint8_t r = (data & 0x0F);
          uint8_t d = vdp.latch;
          vdp_reg_w(r, d);
        }
      }
      return;
  }
}

static int vdp_vcounter_line_for_now(void)
{
    int32_t cyc = z80_get_elapsed_cycles();
    int32_t line = (cyc / system_cycles_per_line()) % vdp.lpf;
    int32_t dot = cyc % system_cycles_per_line();
    /* Gearsystem's Madou fix uses separate GG timings: VCOUNT/FLAG_VINT at
     * cycle 28/27 instead of the SMS-era 25/25. */
    int32_t vcount_cycle = (sms.console == CONSOLE_GG) ? 28 : 25;
    if (dot >= vcount_cycle) line = (line + 1) % vdp.lpf;
    return line;
}

static void vdp_update_vint_flag_for_now(void)
{
    int32_t cyc = z80_get_elapsed_cycles();
    int32_t line = (cyc / system_cycles_per_line()) % vdp.lpf;
    int32_t dot = cyc % system_cycles_per_line();
    int32_t flag_cycle = (sms.console == CONSOLE_GG) ? 27 : 25;

    if ((line == vdp.height) && (dot >= flag_cycle))
    {
        vdp.status |= 0x80;
        vdp.vint_pending = 1;
        if (vdp.reg[0x01] & 0x20)
        {
#ifdef SORDM5_EMU
            if (sms.console == CONSOLE_SORDM5)
                sordm5_ctc_vdp_interrupt();
            else
#endif
                z80_set_irq_line(vdp.irq, ASSERT_LINE);
        }
    }
}

uint8_t vdp_read(int32_t offset)
{
	uint8_t temp;

	switch(offset & 1)
	{
		case 0: /* CPU <-> VDP data buffer */
		  vdp.pending = 0;
		  temp = vdp.buffer;
		  vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
		  vdp.addr = (vdp.addr + 1) & 0x3FFF;
		return temp;
		case 1: /* Status flags */
		{
			vdp_update_vint_flag_for_now();
			/* cycle-accurate SPR_OVR and INT flags */
			int32_t cyc   = z80_get_elapsed_cycles();
			int32_t line  = vdp.line;
			/*
			 * This is needed for :
			 * - Fantastic Dizzy (otherwise, top bar will flicker)
			 * - Madou Monogatari I GG (otherwise, sprite will be displayed incorrectly)
			*/
			if ((cyc / system_cycles_per_line()) > line)
			{
				if (line == vdp.height) vdp.status |= 0x80;
				line = (line + 1)%vdp.lpf;
				render_line(line);
			}

			/* low 5 bits return non-zero data (fixes PGA Tour Golf course map introduction) */
			temp = vdp.status | 0x1f;

			/* clear flags */
			vdp.status = 0;
			vdp.pending = 0;
			vdp.vint_pending = 0;
			vdp.hint_pending = 0;
#ifdef SORDM5_EMU
			if (sms.console != CONSOLE_SORDM5)
#endif
				z80_set_irq_line(vdp.irq, CLEAR_LINE);

			/* cycle-accurate SPR_COL flag */
			if (temp & 0x20)
			{
				uint8_t hc = hc_256[system_hcounter_index()];
				if ((line == (vdp.spr_col >> 8)) && ((hc < (vdp.spr_col & 0xff)) || (hc > 0xf3)))
				{
					vdp.status |= 0x20;
					temp &= ~0x20;
				}
			}
			return temp;
		}
	}

	/* Just to please the compiler */
	return 0;
}

uint8_t vdp_counter_r(int32_t offset)
{
	switch(offset & 1)
	{
		case 0: /* V Counter */
			return vc_table[sms.display][vdp.extended][vdp_vcounter_line_for_now()];
		case 1: /* H Counter -- return previously latched values or ZERO */
			return sms.hlatch;
	}

	/* Just to please the compiler */
	return 0;
}


/*--------------------------------------------------------------------------*/
/* Game Gear VDP handlers                           */
/*--------------------------------------------------------------------------*/

void gg_vdp_write(int32_t offset, uint8_t data)
{
	MULTIREXZ80_TRACE_VDP_WRITE("gg", offset, data);
	int32_t index;

	if (((z80_get_elapsed_cycles() + 1) / system_cycles_per_line()) > vdp.line)
	{
		/* render next line now BEFORE updating register */
		render_line((vdp.line+1)%vdp.lpf);
	}

	switch(offset & 1)
	{
		case 0: /* Data port */
		vdp.pending = 0;
		switch(vdp.code)
		{
			case 0: /* VRAM write */
			case 1: /* VRAM write */
			case 2: /* VRAM write */
				index = (vdp.addr & 0x3FFF);
				if(data != vdp.vram[index])
				{
					vdp.vram[index] = data;
					MARK_BG_DIRTY(vdp.addr);
				}
				vdp.buffer = data;
			break;
			case 3: /* CRAM write */
				if(vdp.addr & 1)
				{
					vdp.cram_latch = (vdp.cram_latch & 0x00FF) | ((data & 0xFF) << 8);
					vdp.cram[(vdp.addr & 0x3E) | (0)] = (vdp.cram_latch >> 0) & 0xFF;
					vdp.cram[(vdp.addr & 0x3E) | (1)] = (vdp.cram_latch >> 8) & 0xFF;
					palette_sync((vdp.addr >> 1) & 0x1F);
				}
				else
				{
					vdp.cram_latch = (vdp.cram_latch & 0xFF00) | ((data & 0xFF) << 0);
				}
				vdp.buffer = data;
			break;
		}
		vdp.addr = (vdp.addr + 1) & 0x3FFF;
		return;

		case 1: /* Control port */
		if(vdp.pending == 0)
		{
			vdp.addr = (vdp.addr & 0x3F00) | (data & 0xFF);
			vdp.latch = data;
			vdp.pending = 1;
		}
		else
		{
			vdp.pending = 0;
			vdp.code = (data >> 6) & 3;
			vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;
			if(vdp.code == 0)
			{
				vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
				vdp.addr = (vdp.addr + 1) & 0x3FFF;
			}
			if(vdp.code == 2)
			{
				uint8_t r = (data & 0x0F);
				uint8_t d = vdp.latch;
				vdp_reg_w(r, d);
			}
		}
		return;
	}
}

/*--------------------------------------------------------------------------*/
/* MegaDrive / Genesis VDP handlers                     */
/*--------------------------------------------------------------------------*/

void md_vdp_write(int32_t offset, uint8_t data)
{
	MULTIREXZ80_TRACE_VDP_WRITE("md", offset, data);
	int32_t index;
	switch(offset & 1)
	{
		case 0: /* Data port */
		vdp.pending = 0;
		switch(vdp.code)
		{
			case 0: /* VRAM write */
			case 1: /* VRAM write */
			index = (vdp.addr & 0x3FFF);
			if(data != vdp.vram[index])
			{
				vdp.vram[index] = data;
				MARK_BG_DIRTY(vdp.addr);
			}
			break;
			case 2: /* CRAM write */
			case 3: /* CRAM write */
				index = (vdp.addr & 0x1F);
				if(data != vdp.cram[index])
				{
					vdp.cram[index] = data;
					palette_sync(index);
				}
			break;
		}
		vdp.addr = (vdp.addr + 1) & 0x3FFF;
		return;

		case 1: /* Control port */
		if(vdp.pending == 0)
		{
			vdp.latch = data;
			vdp.pending = 1;
		}
		else
		{
			vdp.pending = 0;
			vdp.code = (data >> 6) & 3;
			vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;

			if(vdp.code == 0)
			{
				vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
				vdp.addr = (vdp.addr + 1) & 0x3FFF;
			}
    
			if(vdp.code == 2)
			{
				uint8_t r = (data & 0x0F);
				uint8_t d = vdp.latch;
				vdp_reg_w(r, d);
			}
		}
	return;
  }
}

/*--------------------------------------------------------------------------*/
/* TMS9918 VDP handlers                           */
/*--------------------------------------------------------------------------*/

void tms_write(int32_t offset, uint8_t data)
{
	MULTIREXZ80_TRACE_VDP_WRITE("tms9918", offset, data);
	int32_t index;
	switch(offset & 1)
	{
		case 0: /* Data port */
			vdp.pending = 0;

			switch(vdp.code)
			{
				case 0: /* VRAM write */
				case 1: /* VRAM write */
				case 2: /* VRAM write */
				case 3: /* VRAM write */
				index = (vdp.addr & 0x3FFF);
				if(data != vdp.vram[index])
				{
					vdp.vram[index] = data;
					MARK_BG_DIRTY(vdp.addr);
				}
				break;
			}
			vdp.addr = (vdp.addr + 1) & 0x3FFF;
		return;

		case 1: /* Control port */
		if(vdp.pending == 0)
		{
			vdp.latch = data;
			vdp.pending = 1;
		}
		else
		{
			vdp.pending = 0;
			vdp.code = (data >> 6) & 3;
			vdp.addr = (data << 8 | vdp.latch) & 0x3FFF;
			if(vdp.code == 0)
			{
				vdp.buffer = vdp.vram[vdp.addr & 0x3FFF];
				vdp.addr = (vdp.addr + 1) & 0x3FFF;
			}
    
			if(vdp.code == 2)
			{
				uint8_t r = (data & 0x07);
				uint8_t d = vdp.latch;
				vdp_reg_w(r, d);
			}
		}
		return;
	}
}
