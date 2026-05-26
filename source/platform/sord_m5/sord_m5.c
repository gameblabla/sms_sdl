/******************************************************************************
 * Sord M5 I/O and lightweight CTC model for SMS Plus GX.
 *
 * This follows MAME's src/mame/sord/m5.cpp hardware mapping and behavior, but
 * remains an independent reduced implementation.
 ******************************************************************************/

#include "shared.h"

/*--------------------------------------------------------------------------*/
/* Sord M5 port handlers                                                    */
/*--------------------------------------------------------------------------*/

typedef struct
{
	uint8_t vector;
	uint16_t mode[4];
	uint16_t time_constant[4];
	uint16_t down[4];
	int32_t cycles_until[4];
	uint8_t timer_active[4];
	uint8_t extclk[4];
	uint8_t interrupt_pending;
} sordm5_ctc_t;

#define SORDM5_CTC_INTERRUPT   0x80
#define SORDM5_CTC_MODE        0x40
#define SORDM5_CTC_COUNTER     0x40
#define SORDM5_CTC_PRESCALER   0x20
#define SORDM5_CTC_EDGE        0x10
#define SORDM5_CTC_TRIGGER     0x08
#define SORDM5_CTC_CONSTANT    0x04
#define SORDM5_CTC_RESET       0x02
#define SORDM5_CTC_CONTROL     0x01
#define SORDM5_CTC_WAIT_TRIG   0x100

static sordm5_ctc_t sordm5_ctc;

void sordm5_ctc_reset(void)
{
	memset(&sordm5_ctc, 0, sizeof(sordm5_ctc));
	/* TMS9928A INT is treated as an active-low pulse into CTC TRG3. */
	sordm5_ctc.extclk[3] = 1;
}

static void sordm5_ctc_raise(uint8_t channel)
{
	channel &= 3;
	if (sordm5_ctc.mode[channel] & SORDM5_CTC_INTERRUPT)
	{
		sordm5_ctc.interrupt_pending |= (1 << channel);
		z80_set_irq_line(INPUT_LINE_IRQ0, ASSERT_LINE);
	}
}

static int32_t sordm5_ctc_period_cycles(uint8_t channel)
{
	uint16_t mode = sordm5_ctc.mode[channel & 3];
	uint16_t tc = sordm5_ctc.time_constant[channel & 3];
	if (!tc) tc = 0x100;
	if (mode & SORDM5_CTC_COUNTER)
		return (int32_t)tc;
	return (int32_t)tc * ((mode & SORDM5_CTC_PRESCALER) ? 256 : 16);
}

static void sordm5_ctc_start_timer(uint8_t channel)
{
	channel &= 3;
	if ((sordm5_ctc.mode[channel] & SORDM5_CTC_COUNTER) ||
	    (sordm5_ctc.mode[channel] & SORDM5_CTC_RESET) ||
	    (sordm5_ctc.mode[channel] & SORDM5_CTC_WAIT_TRIG))
	{
		sordm5_ctc.timer_active[channel] = 0;
		return;
	}

	sordm5_ctc.cycles_until[channel] = sordm5_ctc_period_cycles(channel);
	if (sordm5_ctc.cycles_until[channel] <= 0)
		sordm5_ctc.cycles_until[channel] = 1;
	sordm5_ctc.timer_active[channel] = 1;
}

static void sordm5_ctc_w(uint8_t channel, uint8_t data)
{
	channel &= 3;

	/*
	 * MAME maps a real Z80 CTC at ports $00-$03.  Pooyan programs
	 * channel 1 as a periodic timer and channel 3 as a VDP-triggered
	 * counter; a VBlank-only shortcut reaches the title screen but leaves
	 * the game stuck at the PLAYER 1 interstitial.
	 */
	if (sordm5_ctc.mode[channel] & SORDM5_CTC_CONSTANT)
	{
		sordm5_ctc.time_constant[channel] = data ? data : 0x100;
		sordm5_ctc.down[channel] = sordm5_ctc.time_constant[channel];
		sordm5_ctc.mode[channel] &= ~(SORDM5_CTC_CONSTANT | SORDM5_CTC_RESET);

		if ((sordm5_ctc.mode[channel] & SORDM5_CTC_COUNTER) ||
		    !(sordm5_ctc.mode[channel] & SORDM5_CTC_TRIGGER))
		{
			sordm5_ctc.mode[channel] &= ~SORDM5_CTC_WAIT_TRIG;
			sordm5_ctc_start_timer(channel);
		}
		else
		{
			sordm5_ctc.mode[channel] |= SORDM5_CTC_WAIT_TRIG;
			sordm5_ctc.timer_active[channel] = 0;
		}
		return;
	}

	/* MAME's z80ctc accepts vector writes on channel 0. */
	if (!(data & SORDM5_CTC_CONTROL))
	{
		if (channel == 0)
			sordm5_ctc.vector = data & 0xF8;
		return;
	}

	if (data & SORDM5_CTC_RESET)
		sordm5_ctc.timer_active[channel] = 0;

	sordm5_ctc.mode[channel] = data;

	if (!(data & SORDM5_CTC_INTERRUPT))
	{
		sordm5_ctc.interrupt_pending &= ~(1 << channel);
		if (!sordm5_ctc.interrupt_pending)
			z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
	}
}

static uint8_t sordm5_ctc_r(uint8_t channel)
{
	channel &= 3;
	if (sordm5_ctc.timer_active[channel])
	{
		int32_t prescale = (sordm5_ctc.mode[channel] & SORDM5_CTC_PRESCALER) ? 256 : 16;
		int32_t remain = (sordm5_ctc.cycles_until[channel] + prescale - 1) / prescale;
		if (remain < 1) remain = 1;
		if (remain > 0x100) remain = 0x100;
		return (uint8_t)remain;
	}
	return (uint8_t)(sordm5_ctc.down[channel] ? sordm5_ctc.down[channel] : 0x100);
}

void sordm5_ctc_tick(int32_t cycles)
{
	uint8_t channel;
	if (cycles <= 0) return;

	for (channel = 0; channel < 4; channel++)
	{
		if (!sordm5_ctc.timer_active[channel])
			continue;

		sordm5_ctc.cycles_until[channel] -= cycles;
		while (sordm5_ctc.cycles_until[channel] <= 0)
		{
			sordm5_ctc_raise(channel);
			sordm5_ctc.down[channel] = sordm5_ctc.time_constant[channel];
			sordm5_ctc.cycles_until[channel] += sordm5_ctc_period_cycles(channel);
			if (sordm5_ctc.cycles_until[channel] <= 0)
			{
				sordm5_ctc.cycles_until[channel] = 1;
				break;
			}
		}

		if (!(sordm5_ctc.mode[channel] & SORDM5_CTC_COUNTER))
		{
			int32_t prescale = (sordm5_ctc.mode[channel] & SORDM5_CTC_PRESCALER) ? 256 : 16;
			int32_t remain = (sordm5_ctc.cycles_until[channel] + prescale - 1) / prescale;
			if (remain < 1) remain = 1;
			sordm5_ctc.down[channel] = (uint16_t)remain;
		}
	}
}

static void sordm5_ctc_trigger(uint8_t channel, uint8_t state)
{
	uint8_t active_edge;
	channel &= 3;
	state = state ? 1 : 0;

	if (state == sordm5_ctc.extclk[channel])
		return;
	sordm5_ctc.extclk[channel] = state;

	active_edge = (sordm5_ctc.mode[channel] & SORDM5_CTC_EDGE) ? state : !state;
	if (!active_edge)
		return;

	if ((sordm5_ctc.mode[channel] & SORDM5_CTC_WAIT_TRIG) &&
	    !(sordm5_ctc.mode[channel] & SORDM5_CTC_COUNTER))
	{
		sordm5_ctc.mode[channel] &= ~SORDM5_CTC_WAIT_TRIG;
		sordm5_ctc_start_timer(channel);
		return;
	}

	if (sordm5_ctc.mode[channel] & SORDM5_CTC_COUNTER)
	{
		if (sordm5_ctc.down[channel] == 0)
			sordm5_ctc.down[channel] = sordm5_ctc.time_constant[channel] ? sordm5_ctc.time_constant[channel] : 0x100;
		if (--sordm5_ctc.down[channel] == 0)
		{
			sordm5_ctc_raise(channel);
			sordm5_ctc.down[channel] = sordm5_ctc.time_constant[channel] ? sordm5_ctc.time_constant[channel] : 0x100;
		}
	}
}

void sordm5_ctc_vdp_interrupt(void)
{
	/* MAME connects the TMS9928A INT callback to CTC TRG3. */
	sordm5_ctc_trigger(3, 0);
	sordm5_ctc_trigger(3, 1);
}

int32_t sordm5_ctc_irq_callback(void)
{
	uint8_t channel;

	for (channel = 0; channel < 4; channel++)
	{
		if (sordm5_ctc.interrupt_pending & (1 << channel))
		{
			sordm5_ctc.interrupt_pending &= ~(1 << channel);
			if (sordm5_ctc.interrupt_pending)
				z80_set_irq_line(INPUT_LINE_IRQ0, ASSERT_LINE);
			else
				z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
			return (sordm5_ctc.vector | (channel << 1)) & 0xFF;
		}
	}

	z80_set_irq_line(INPUT_LINE_IRQ0, CLEAR_LINE);
	return sordm5_ctc.vector;
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
