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

// license:GPL-2.0+
// copyright-holders:Jarek Burczynski
/*
**
** File: ymdeltat.c
**
** YAMAHA DELTA-T adpcm sound emulation subroutine
** used by fmopl.c (Y8950) and fm.c (YM2608 and YM2610/B)
**
** Base program is YM2610 emulator by Hiromitsu Shioya.
** Written by Tatsuyuki Satoh
** Improvements by Jarek Burczynski (bujar at mame dot net)
**
**
** History:
**
** 03-08-2003 Jarek Burczynski:
**  - fixed BRDY flag implementation.
**
** 24-07-2003 Jarek Burczynski, Frits Hilderink:
**  - fixed default value for deltat->control2 in YM_DELTAT_ADPCM_Reset
**
** 22-07-2003 Jarek Burczynski, Frits Hilderink:
**  - fixed external deltat->memory support
**
** 15-06-2003 Jarek Burczynski:
**  - implemented CPU -> AUDIO ADPCM synthesis (via writes to the ADPCM data deltat->reg $08)
**  - implemented support for the Limit address register
**  - supported two bits from the control register 2 ($01): RAM TYPE (x1 bit/x8 bit), ROM/RAM
**  - implemented external deltat->memory access (read/write) via the ADPCM data deltat->reg reads/writes
**    Thanks go to Frits Hilderink for the example code.
**
** 14-06-2003 Jarek Burczynski:
**  - various fixes to enable proper support for status register flags: BSRDY, PCM BSY, ZERO
**  - modified EOS handling
**
** 05-04-2003 Jarek Burczynski:
**  - implemented partial support for external/processor deltat->memory on sample replay
**
** 01-12-2002 Jarek Burczynski:
**  - fixed first missing sound in gigandes thanks to previous fix (interpolator) by ElSemi
**  - renamed/removed some YM_DELTAT struct fields
**
** 28-12-2001 Acho A. Tang
**  - added EOS status report on ADPCM playback.
**
** 05-08-2001 Jarek Burczynski:
**  - deltat->now_step is initialized with 0 at the deltat->start of play.
**
** 12-06-2001 Jarek Burczynski:
**  - corrected deltat->end of sample bug in YM_DELTAT_ADPCM_CALC.
**    Checked on real YM2610 chip - address register is 24 bits wide.
**    Thanks go to Stefan Jokisch (stefan.jokisch@gmx.de) for tracking down the problem.
**
** TO DO:
**      Check size of the address register on the other chips....
**
** Version 0.72
**
** sound chips that have deltat unit:
** YM2608   OPNA
** YM2610/B OPNB
** Y8950    MSX AUDIO
**
*/

#include <stdint.h>
#include "mame_fmopl.h"

#define logerror(...) ((void)0)

#define YM_DELTAT_SHIFT    (16)

#define YM_DELTAT_DELTA_MAX (24576)
#define YM_DELTAT_DELTA_MIN (127)
#define YM_DELTAT_DELTA_DEF (127)

#define YM_DELTAT_DECODE_RANGE 32768
#define YM_DELTAT_DECODE_MIN (-(YM_DELTAT_DECODE_RANGE))
#define YM_DELTAT_DECODE_MAX ((YM_DELTAT_DECODE_RANGE)-1)


/* Forecast to next Forecast (rate = *8) */
/* 1/8 , 3/8 , 5/8 , 7/8 , 9/8 , 11/8 , 13/8 , 15/8 */
static const int32_t ym_deltat_decode_tableB1[16] = {
	1,   3,   5,   7,   9,  11,  13,  15,
	-1,  -3,  -5,  -7,  -9, -11, -13, -15,
};
/* deltat->delta to next deltat->delta (rate= *64) */
/* 0.9 , 0.9 , 0.9 , 0.9 , 1.2 , 1.6 , 2.0 , 2.4 */
static const int32_t ym_deltat_decode_tableB2[16] = {
	57,  57,  57,  57, 77, 102, 128, 153,
	57,  57,  57,  57, 77, 102, 128, 153
};


static inline uint32_t YM_DELTAT_memory_index(const YM_DELTAT *deltat, uint32_t byte_address)
{
	if (!deltat->memory_size)
		return 0;
	if (deltat->memory_mask)
		return byte_address & deltat->memory_mask;
	return byte_address % deltat->memory_size;
}

uint8_t YM_DELTAT_ADPCM_Read(YM_DELTAT *deltat)
{
	uint8_t v = 0;

	/* external deltat->memory read */
	if ((deltat->portstate & 0xe0) == 0x20)
	{
		/* two dummy reads */
		if (deltat->memread)
		{
			deltat->now_addr = deltat->start << 1;
			deltat->memread--;
			return 0;
		}


		if (deltat->now_addr != (deltat->end << 1))
		{
			v = deltat->memory[YM_DELTAT_memory_index(deltat, deltat->now_addr >> 1)];

			/*logerror("YM Delta-T deltat->memory read  $%08x, v=$%02x\n", deltat->now_addr >> 1, v);*/

			deltat->now_addr += 2; /* two nibbles at a time */

			/* reset BRDY bit in status register, which means we are reading the deltat->memory now */
			if (deltat->status_reset_handler && deltat->status_change_BRDY_bit)
				(deltat->status_reset_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);

			/* setup a timer that will callback us in 10 master clock cycles for Y8950
			* in the callback set the BRDY flag to 1 , which means we have another data ready.
			* For now, we don't really do deltat; we simply reset and set the flag in zero time, so that the IRQ will work.
			*/
			/* set BRDY bit in status register */
			if (deltat->status_set_handler && deltat->status_change_BRDY_bit)
				(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);
		}
		else
		{
			/* set EOS bit in status register */
			if (deltat->status_set_handler && deltat->status_change_EOS_bit)
				(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_EOS_bit);
		}
	}

	return v;
}


/* 0-DRAM x1, 1-ROM, 2-DRAM x8, 3-ROM (3 is bad setting - not allowed by the manual) */
static const uint8_t dram_rightshift[4]={3,0,0,0};

/* DELTA-T ADPCM write register */
void YM_DELTAT_ADPCM_Write(YM_DELTAT *deltat, int r, int v)
{
	if (r >= 0x10) return;
	deltat->reg[r] = v; /* stock data */

	switch (r)
	{
	case 0x00:
/*
START:
    Accessing *external* deltat->memory is started when START bit (D7) is set to "1", so
    you must set all conditions needed for recording/playback before starting.
    If you access *CPU-managed* deltat->memory, recording/playback starts after
    read/write of ADPCM data register $08.

REC:
    0 = ADPCM synthesis (playback)
    1 = ADPCM analysis (record)

MEMDATA:
    0 = processor (*CPU-managed*) deltat->memory (means: using register $08)
    1 = external deltat->memory (using deltat->start/deltat->end/deltat->limit registers to access deltat->memory: RAM or ROM)


SPOFF:
    controls output pin that should disable the speaker while ADPCM analysis

RESET and REPEAT only work with external deltat->memory.


some examples:
value:   START, REC, MEMDAT, REPEAT, SPOFF, x,x,RESET   meaning:
  C8     1      1    0       0       1      0 0 0       Analysis (recording) from AUDIO to CPU (to deltat->reg $08), sample rate in PRESCALER register
  E8     1      1    1       0       1      0 0 0       Analysis (recording) from AUDIO to EXT.MEMORY,       sample rate in PRESCALER register
  80     1      0    0       0       0      0 0 0       Synthesis (playing) from CPU (from deltat->reg $08) to AUDIO,sample rate in DELTA-N register
  a0     1      0    1       0       0      0 0 0       Synthesis (playing) from EXT.MEMORY to AUDIO,        sample rate in DELTA-N register

  60     0      1    1       0       0      0 0 0       External deltat->memory write via ADPCM data register $08
  20     0      0    1       0       0      0 0 0       External deltat->memory read via ADPCM data register $08

*/
		/* handle emulation mode */
		if (deltat->emulation_mode == YM_DELTAT_EMULATION_MODE_YM2610)
		{
			v |= 0x20;      /*  YM2610 always uses external deltat->memory and doesn't even have deltat->memory flag bit. */
		}

		deltat->portstate = v & (0x80|0x40|0x20|0x10|0x01); /* deltat->start, rec, deltat->memory mode, repeat flag copy, reset(bit0) */

		if (deltat->portstate & 0x80)/* START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET */
		{
			/* set PCM BUSY bit */
			deltat->PCM_BSY = 1;

			/* deltat->start ADPCM */
			deltat->now_step = 0;
			deltat->acc      = 0;
			deltat->prev_acc = 0;
			deltat->adpcml   = 0;
			deltat->adpcmd   = YM_DELTAT_DELTA_DEF;
			deltat->now_data = 0;

		}

		if (deltat->portstate & 0x20) /* do we access external deltat->memory? */
		{
			deltat->now_addr = deltat->start << 1;
			deltat->memread = 2;    /* two dummy reads needed before accessing external deltat->memory via register $08*/

			/* if yes, then let's check if ADPCM deltat->memory is mapped and big enough */
			if (!deltat->memory)
			{
				logerror("YM Delta-T log suppressed\n");
				deltat->portstate = 0x00;
				deltat->PCM_BSY = 0;
			}
			else
			{
				if (!deltat->memory_mask && deltat->end >= deltat->memory_size)    /* Check End in Range */
				{
					logerror("YM Delta-T log suppressed\n");
					deltat->end = deltat->memory_size - 1;
				}
				if (!deltat->memory_mask && deltat->start >= deltat->memory_size)  /* Check Start in Range */
				{
					logerror("YM Delta-T log suppressed\n");
					deltat->portstate = 0x00;
					deltat->PCM_BSY = 0;
				}
			}
		}
		else    /* we access CPU deltat->memory (ADPCM data register $08) so we only reset deltat->now_addr here */
		{
			deltat->now_addr = 0;
		}

		if (deltat->portstate & 0x01)
		{
			deltat->portstate = 0x00;

			/* clear PCM BUSY bit (in status register) */
			deltat->PCM_BSY = 0;

			/* set BRDY flag */
			if (deltat->status_set_handler && deltat->status_change_BRDY_bit)
				(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);
		}
		break;

	case 0x01:  /* L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM */
		/* handle emulation mode */
		if (deltat->emulation_mode == YM_DELTAT_EMULATION_MODE_YM2610)
		{
			v |= 0x01;      /*  YM2610 always uses ROM as an external deltat->memory and doesn't tave ROM/RAM deltat->memory flag bit. */
		}

		deltat->pan = &deltat->output_pointer[(v >> 6) & 0x03];
		if ((deltat->control2 & 3) != (v & 3))
		{
			/*0-DRAM x1, 1-ROM, 2-DRAM x8, 3-ROM (3 is bad setting - not allowed by the manual) */
			if (deltat->DRAMportshift != dram_rightshift[v & 3])
			{
				deltat->DRAMportshift = dram_rightshift[v & 3];

				/* final shift value depends on chip type and deltat->memory type selected:
				        8 for YM2610 (ROM only),
				        5 for ROM for Y8950 and YM2608,
				        5 for x8bit DRAMs for Y8950 and YM2608,
				        2 for x1bit DRAMs for Y8950 and YM2608.
				*/

				/* refresh addresses */
				deltat->start  = (deltat->reg[0x3] * 0x0100 | deltat->reg[0x2]) << (deltat->portshift - deltat->DRAMportshift);
				deltat->end    = (deltat->reg[0x5] * 0x0100 | deltat->reg[0x4]) << (deltat->portshift - deltat->DRAMportshift);
				deltat->end   += (1 << (deltat->portshift - deltat->DRAMportshift)) - 1;
				deltat->limit  = (deltat->reg[0xd]*0x0100 | deltat->reg[0xc]) << (deltat->portshift - deltat->DRAMportshift);
			}
		}
		deltat->control2 = v;
		break;

	case 0x02:  /* Start Address L */
	case 0x03:  /* Start Address H */
		deltat->start  = (deltat->reg[0x3] * 0x0100 | deltat->reg[0x2]) << (deltat->portshift - deltat->DRAMportshift);
		/*logerror("DELTAT deltat->start: 02=%2x 03=%2x addr=%8x\n",deltat->reg[0x2], deltat->reg[0x3],deltat->start );*/
		break;

	case 0x04:  /* Stop Address L */
	case 0x05:  /* Stop Address H */
		deltat->end    = (deltat->reg[0x5]*0x0100 | deltat->reg[0x4]) << (deltat->portshift - deltat->DRAMportshift);
		deltat->end   += (1 << (deltat->portshift - deltat->DRAMportshift)) - 1;
		/*logerror("DELTAT deltat->end  : 04=%2x 05=%2x addr=%8x\n",deltat->reg[0x4], deltat->reg[0x5],deltat->end   );*/
		break;

	case 0x06:  /* Prescale L (ADPCM and Record frq) */
	case 0x07:  /* Prescale H */
		break;

	case 0x08:  /* ADPCM data */
/*
some examples:
value:   START, REC, MEMDAT, REPEAT, SPOFF, x,x,RESET   meaning:
  C8     1      1    0       0       1      0 0 0       Analysis (recording) from AUDIO to CPU (to deltat->reg $08), sample rate in PRESCALER register
  E8     1      1    1       0       1      0 0 0       Analysis (recording) from AUDIO to EXT.MEMORY,       sample rate in PRESCALER register
  80     1      0    0       0       0      0 0 0       Synthesis (playing) from CPU (from deltat->reg $08) to AUDIO,sample rate in DELTA-N register
  a0     1      0    1       0       0      0 0 0       Synthesis (playing) from EXT.MEMORY to AUDIO,        sample rate in DELTA-N register

  60     0      1    1       0       0      0 0 0       External deltat->memory write via ADPCM data register $08
  20     0      0    1       0       0      0 0 0       External deltat->memory read via ADPCM data register $08

*/

		/* external deltat->memory write */
		if ((deltat->portstate & 0xe0) == 0x60)
		{
			if (deltat->memread)
			{
				deltat->now_addr = deltat->start << 1;
				deltat->memread = 0;
			}

			/*logerror("YM Delta-T deltat->memory write $%08x, v=$%02x\n", deltat->now_addr >> 1, v);*/

			if (deltat->now_addr != (deltat->end << 1))
			{
				deltat->memory[YM_DELTAT_memory_index(deltat, deltat->now_addr >> 1)] = v;
				deltat->now_addr += 2; /* two nybbles at a time */

				/* reset BRDY bit in status register, which means we are processing the write */
				if (deltat->status_reset_handler && deltat->status_change_BRDY_bit)
					(deltat->status_reset_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);

				/* setup a timer that will callback us in 10 master clock cycles for Y8950
				* in the callback set the BRDY flag to 1 , which means we have written the data.
				* For now, we don't really do deltat; we simply reset and set the flag in zero time, so that the IRQ will work.
				*/
				/* set BRDY bit in status register */
				if (deltat->status_set_handler && deltat->status_change_BRDY_bit)
					(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);

			}
			else
			{
				/* set EOS bit in status register */
				if (deltat->status_set_handler && deltat->status_change_EOS_bit)
					(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_EOS_bit);
			}

			return;
		}

		/* ADPCM synthesis from CPU */
		if ((deltat->portstate & 0xe0) == 0x80)
		{
			deltat->CPU_data = v;

			/* Reset BRDY bit in status register, which means we are full of data */
			if (deltat->status_reset_handler && deltat->status_change_BRDY_bit)
				(deltat->status_reset_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);
			return;
		}

		break;

	case 0x09:  /* DELTA-N L (ADPCM Playback Prescaler) */
	case 0x0a:  /* DELTA-N H */
		deltat->delta  = (deltat->reg[0xa] * 0x0100 | deltat->reg[0x9]);
		deltat->step = (uint32_t)((double)deltat->delta * deltat->freqbase);
		/*logerror("DELTAT deltan:09=%2x 0a=%2x\n",deltat->reg[0x9], deltat->reg[0xa]);*/
		break;

	case 0x0b:  /* Output level control (deltat->volume, linear) */
		{
			const int32_t oldvol = deltat->volume;
			deltat->volume = (v & 0xff) * (deltat->output_range / 256) / YM_DELTAT_DECODE_RANGE;
/*                              v     *     ((1<<16)>>8)        >>  15;
*                       thus:   v     *     (1<<8)              >>  15;
*                       thus: deltat->output_range must be (1 << (15+8)) at least
*                               v     *     ((1<<23)>>8)        >>  15;
*                               v     *     (1<<15)             >>  15;
*/
			/*logerror("DELTAT vol = %2x\n",v&0xff);*/
			if (oldvol != 0)
			{
				deltat->adpcml = (int)((double)(deltat->adpcml) / (double)(oldvol) * (double)(deltat->volume));
			}
		}
		break;

	case 0x0c:  /* Limit Address L */
	case 0x0d:  /* Limit Address H */
		deltat->limit  = (deltat->reg[0xd] * 0x0100 | deltat->reg[0xc]) << (deltat->portshift - deltat->DRAMportshift);
		/*logerror("DELTAT deltat->limit: 0c=%2x 0d=%2x addr=%8x\n",deltat->reg[0xc], deltat->reg[0xd],deltat->limit );*/
		break;
	}
}

void YM_DELTAT_ADPCM_Reset(YM_DELTAT *deltat, int panidx, int mode)
{
	deltat->now_addr  = 0;
	deltat->now_step  = 0;
	deltat->step      = 0;
	deltat->start     = 0;
	deltat->end       = 0;
	deltat->limit     = ~0; /* deltat way YM2610 and Y8950 (both of which don't have deltat->limit address deltat->reg) will still work */
	deltat->volume    = 0;
	deltat->pan       = &deltat->output_pointer[panidx];
	deltat->acc       = 0;
	deltat->prev_acc  = 0;
	deltat->adpcmd    = 127;
	deltat->adpcml    = 0;
	deltat->emulation_mode = (uint8_t)mode;
	if (!deltat->memory_size)
		deltat->memory_mask = 0;
	deltat->portstate = (deltat->emulation_mode == YM_DELTAT_EMULATION_MODE_YM2610) ? 0x20 : 0;
	deltat->control2  = (deltat->emulation_mode == YM_DELTAT_EMULATION_MODE_YM2610) ? 0x01 : 0; /* default setting depends on the emulation mode. MSX demo called "facdemo_4" doesn't setup deltat->control2 register at all and still works */
	deltat->DRAMportshift = dram_rightshift[deltat->control2 & 3];

	/* The flag mask register disables the BRDY after the reset, however
	** as soon as the mask is enabled the flag needs to be set. */

	/* set BRDY bit in status register */
	if (deltat->status_set_handler && deltat->status_change_BRDY_bit)
		(deltat->status_set_handler)(deltat->status_change_which_chip, deltat->status_change_BRDY_bit);
}

#define YM_DELTAT_Limit(val,max,min)    \
{                                       \
	if ( val > max ) val = max;         \
	else if ( val < min ) val = min;    \
}

static inline void YM_DELTAT_synthesis_from_external_memory(YM_DELTAT *DELTAT)
{
	uint32_t step;
	int data;

	DELTAT->now_step += DELTAT->step;
	if ( DELTAT->now_step >= (1<<YM_DELTAT_SHIFT) )
	{
		step = DELTAT->now_step >> YM_DELTAT_SHIFT;
		DELTAT->now_step &= (1<<YM_DELTAT_SHIFT)-1;
		do{
			if ( DELTAT->now_addr == (DELTAT->limit<<1) )
				DELTAT->now_addr = 0;

			if ( DELTAT->now_addr == (DELTAT->end<<1) ) {   /* 12-06-2001 JB: corrected comparison. Was > instead of == */
				if( DELTAT->portstate&0x10 ){
					/* repeat deltat->start */
					DELTAT->now_addr = DELTAT->start<<1;
					DELTAT->acc      = 0;
					DELTAT->adpcmd   = YM_DELTAT_DELTA_DEF;
					DELTAT->prev_acc = 0;
				}else{
					/* set EOS bit in status register */
					if(DELTAT->status_set_handler)
						if(DELTAT->status_change_EOS_bit)
							(DELTAT->status_set_handler)(DELTAT->status_change_which_chip, DELTAT->status_change_EOS_bit);

					/* clear PCM BUSY bit (reflected in status register) */
					DELTAT->PCM_BSY = 0;

					DELTAT->portstate = 0;
					DELTAT->adpcml = 0;
					DELTAT->prev_acc = 0;
					return;
				}
			}

			if( DELTAT->now_addr&1 ) data = DELTAT->now_data & 0x0f;
			else
			{
				DELTAT->now_data = DELTAT->memory[YM_DELTAT_memory_index(DELTAT, DELTAT->now_addr >> 1)];
				data = DELTAT->now_data >> 4;
			}

			DELTAT->now_addr++;
			/* 12-06-2001 JB: */
			/* YM2610 address register is 24 bits wide.*/
			/* The "+1" is there because we use 1 bit more for nibble calculations.*/
			/* WARNING: */
			/* Side effect: we should take the size of the mapped ROM into account */
			DELTAT->now_addr &= ( (1<<(24+1))-1);

			/* store accumulator value */
			DELTAT->prev_acc = DELTAT->acc;

			/* Forecast to next Forecast */
			DELTAT->acc += (ym_deltat_decode_tableB1[data] * DELTAT->adpcmd / 8);
			YM_DELTAT_Limit(DELTAT->acc,YM_DELTAT_DECODE_MAX, YM_DELTAT_DECODE_MIN);

			/* deltat->delta to next deltat->delta */
			DELTAT->adpcmd = (DELTAT->adpcmd * ym_deltat_decode_tableB2[data] ) / 64;
			YM_DELTAT_Limit(DELTAT->adpcmd,YM_DELTAT_DELTA_MAX, YM_DELTAT_DELTA_MIN );

			/* ElSemi: Fix interpolator. */
			/*DELTAT->prev_acc = deltat->prev_acc + ((DELTAT->acc - deltat->prev_acc) / 2 );*/

		}while(--step);

	}

	/* ElSemi: Fix interpolator. */
	DELTAT->adpcml = DELTAT->prev_acc * (int)((1<<YM_DELTAT_SHIFT)-DELTAT->now_step);
	DELTAT->adpcml += (DELTAT->acc * (int)DELTAT->now_step);
	DELTAT->adpcml = (DELTAT->adpcml>>YM_DELTAT_SHIFT) * (int)DELTAT->volume;

	/* output for work of output channels (outd[OPNxxxx])*/
	*(DELTAT->pan) += DELTAT->adpcml;
}



static inline void YM_DELTAT_synthesis_from_CPU_memory(YM_DELTAT *DELTAT)
{
	uint32_t step;
	int data;

	DELTAT->now_step += DELTAT->step;
	if ( DELTAT->now_step >= (1<<YM_DELTAT_SHIFT) )
	{
		step = DELTAT->now_step >> YM_DELTAT_SHIFT;
		DELTAT->now_step &= (1<<YM_DELTAT_SHIFT)-1;
		do{
			if( DELTAT->now_addr&1 )
			{
				data = DELTAT->now_data & 0x0f;

				DELTAT->now_data = DELTAT->CPU_data;

				/* after we used deltat->CPU_data, we set BRDY bit in status register,
				* which means we are ready to accept another byte of data */
				if(DELTAT->status_set_handler)
					if(DELTAT->status_change_BRDY_bit)
						(DELTAT->status_set_handler)(DELTAT->status_change_which_chip, DELTAT->status_change_BRDY_bit);
			}
			else
			{
				data = DELTAT->now_data >> 4;
			}

			DELTAT->now_addr++;

			/* store accumulator value */
			DELTAT->prev_acc = DELTAT->acc;

			/* Forecast to next Forecast */
			DELTAT->acc += (ym_deltat_decode_tableB1[data] * DELTAT->adpcmd / 8);
			YM_DELTAT_Limit(DELTAT->acc,YM_DELTAT_DECODE_MAX, YM_DELTAT_DECODE_MIN);

			/* deltat->delta to next deltat->delta */
			DELTAT->adpcmd = (DELTAT->adpcmd * ym_deltat_decode_tableB2[data] ) / 64;
			YM_DELTAT_Limit(DELTAT->adpcmd,YM_DELTAT_DELTA_MAX, YM_DELTAT_DELTA_MIN );


		}while(--step);

	}

	/* ElSemi: Fix interpolator. */
	DELTAT->adpcml = DELTAT->prev_acc * (int)((1<<YM_DELTAT_SHIFT)-DELTAT->now_step);
	DELTAT->adpcml += (DELTAT->acc * (int)DELTAT->now_step);
	DELTAT->adpcml = (DELTAT->adpcml>>YM_DELTAT_SHIFT) * (int)DELTAT->volume;

	/* output for work of output channels (outd[OPNxxxx])*/
	*(DELTAT->pan) += DELTAT->adpcml;
}



/* ADPCM B (Delta-T control type) */
void YM_DELTAT_ADPCM_CALC(YM_DELTAT *deltat)
{
/*
some examples:
value:   START, REC, MEMDAT, REPEAT, SPOFF, x,x,RESET   meaning:
  80     1      0    0       0       0      0 0 0       Synthesis (playing) from CPU (from deltat->reg $08) to AUDIO,sample rate in DELTA-N register
  a0     1      0    1       0       0      0 0 0       Synthesis (playing) from EXT.MEMORY to AUDIO,        sample rate in DELTA-N register
  C8     1      1    0       0       1      0 0 0       Analysis (recording) from AUDIO to CPU (to deltat->reg $08), sample rate in PRESCALER register
  E8     1      1    1       0       1      0 0 0       Analysis (recording) from AUDIO to EXT.MEMORY,       sample rate in PRESCALER register

  60     0      1    1       0       0      0 0 0       External deltat->memory write via ADPCM data register $08
  20     0      0    1       0       0      0 0 0       External deltat->memory read via ADPCM data register $08

*/

	if ( (deltat->portstate & 0xe0)==0xa0 )
	{
		YM_DELTAT_synthesis_from_external_memory(deltat);
		return;
	}

	if ( (deltat->portstate & 0xe0)==0x80 )
	{
		/* ADPCM synthesis from CPU-managed deltat->memory (from deltat->reg $08) */
		YM_DELTAT_synthesis_from_CPU_memory(deltat);    /* change output based on data in ADPCM data deltat->reg ($08) */
		return;
	}

//todo: ADPCM analysis
//  if ( (deltat->portstate & 0xe0)==0xc0 )
//  if ( (deltat->portstate & 0xe0)==0xe0 )

	return;
}
