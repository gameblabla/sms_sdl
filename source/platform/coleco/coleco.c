/******************************************************************************
 * ColecoVision I/O handlers for SMS Plus GX.
 ******************************************************************************/

#include "shared.h"

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

