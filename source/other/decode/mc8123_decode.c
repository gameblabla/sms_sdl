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
 * MC-8123 ROM decryption helper.
 *
 * Derived from MAME's src/devices/cpu/z80/mc8123.cpp, licensed BSD-3-Clause.
 * Original copyright-holders: Nicola Salmoria, David Widel.
 * This version is load-time-only and does not emulate the protected Z80 at runtime.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "other/decode/mc8123_decode.h"

#define MC8123_BIT(x,n) (((x) >> (n)) & 1u)
#define MC8123_B1(a) (1u << (a))
#define MC8123_B2(a,b) (MC8123_B1(a) | MC8123_B1(b))
#define MC8123_B3(a,b,c) (MC8123_B2(a,b) | MC8123_B1(c))
#define MC8123_B4(a,b,c,d) (MC8123_B3(a,b,c) | MC8123_B1(d))
#define MC8123_B5(a,b,c,d,e) (MC8123_B4(a,b,c,d) | MC8123_B1(e))
#define MC8123_B6(a,b,c,d,e,f) (MC8123_B5(a,b,c,d,e) | MC8123_B1(f))
#define MC8123_B7(a,b,c,d,e,f,g) (MC8123_B6(a,b,c,d,e,f) | MC8123_B1(g))
#define MC8123_B8(a,b,c,d,e,f,g,h) (MC8123_B7(a,b,c,d,e,f,g) | MC8123_B1(h))
#define MC8123_BITS_SELECT(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME
#define MC8123_BITS(...) MC8123_BITS_SELECT(__VA_ARGS__,MC8123_B8,MC8123_B7,MC8123_B6,MC8123_B5,MC8123_B4,MC8123_B3,MC8123_B2,MC8123_B1)(__VA_ARGS__)

static uint8_t mc8123_bitswap8(uint8_t v, int b7, int b6, int b5, int b4, int b3, int b2, int b1, int b0)
{
    return (uint8_t)((((v >> b7) & 1u) << 7) | (((v >> b6) & 1u) << 6) |
                     (((v >> b5) & 1u) << 5) | (((v >> b4) & 1u) << 4) |
                     (((v >> b3) & 1u) << 3) | (((v >> b2) & 1u) << 2) |
                     (((v >> b1) & 1u) << 1) | (((v >> b0) & 1u) << 0));
}

static uint16_t mc8123_bitswap12(uint32_t v, int b11, int b10, int b9, int b8, int b7, int b6, int b5, int b4, int b3, int b2, int b1, int b0)
{
    return (uint16_t)((((v >> b11) & 1u) << 11) | (((v >> b10) & 1u) << 10) |
                      (((v >> b9) & 1u) << 9) | (((v >> b8) & 1u) << 8) |
                      (((v >> b7) & 1u) << 7) | (((v >> b6) & 1u) << 6) |
                      (((v >> b5) & 1u) << 5) | (((v >> b4) & 1u) << 4) |
                      (((v >> b3) & 1u) << 3) | (((v >> b2) & 1u) << 2) |
                      (((v >> b1) & 1u) << 1) | (((v >> b0) & 1u) << 0));
}

static uint8_t mc8123_decrypt_type0(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,7,5,3,1,2,0,6,4);
	if (swap == 1) val = mc8123_bitswap8(val,5,3,7,2,1,0,4,6);
	if (swap == 2) val = mc8123_bitswap8(val,0,3,4,6,7,1,5,2);
	if (swap == 3) val = mc8123_bitswap8(val,0,7,3,2,6,4,1,5);

	if (MC8123_BIT(param,3) && MC8123_BIT(val,7))
		val ^= MC8123_BITS(5,3,0);

	if (MC8123_BIT(param,2) && MC8123_BIT(val,6))
		val ^= MC8123_BITS(7,2,1);

	if (MC8123_BIT(val,6)) val ^= MC8123_BITS(7);

	if (MC8123_BIT(param,1) && MC8123_BIT(val,7))
		val ^= MC8123_BITS(6);

	if (MC8123_BIT(val,2)) val ^= MC8123_BITS(5,0);

	val ^= MC8123_BITS(4,3,1);

	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(5,2,0);
	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(7,6);
	if (MC8123_BIT(param,0)) val ^= MC8123_BITS(5,0);

	if (MC8123_BIT(param,0)) val = mc8123_bitswap8(val,7,6,5,1,4,3,2,0);

	return val;
}


static uint8_t mc8123_decrypt_type1a(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,4,2,6,5,3,7,1,0);
	if (swap == 1) val = mc8123_bitswap8(val,6,0,5,4,3,2,1,7);
	if (swap == 2) val = mc8123_bitswap8(val,2,3,6,1,4,0,7,5);
	if (swap == 3) val = mc8123_bitswap8(val,6,5,1,3,2,7,0,4);

	if (MC8123_BIT(param,2)) val = mc8123_bitswap8(val,7,6,1,5,3,2,4,0);

	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(0);
	if (MC8123_BIT(val,6)) val ^= MC8123_BITS(3);
	if (MC8123_BIT(val,7)) val ^= MC8123_BITS(6,3);
	if (MC8123_BIT(val,2)) val ^= MC8123_BITS(6,3,1);
	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(7,6,2);

	if (MC8123_BIT(val,7) ^ MC8123_BIT(val,2))
		val ^= MC8123_BITS(4);

	val ^= MC8123_BITS(6,3,1,0);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(7,2);
	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(6,3);

	if (MC8123_BIT(param,0)) val = mc8123_bitswap8(val,7,6,1,4,3,2,5,0);

	return val;
}

static uint8_t mc8123_decrypt_type1b(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,1,0,3,2,5,6,4,7);
	if (swap == 1) val = mc8123_bitswap8(val,2,0,5,1,7,4,6,3);
	if (swap == 2) val = mc8123_bitswap8(val,6,4,7,2,0,5,1,3);
	if (swap == 3) val = mc8123_bitswap8(val,7,1,3,6,0,2,5,4);

	if (MC8123_BIT(val,2) && MC8123_BIT(val,0))
		val ^= MC8123_BITS(7,4);

	if (MC8123_BIT(val,7)) val ^= MC8123_BITS(2);
	if (MC8123_BIT(val,5)) val ^= MC8123_BITS(7,2);
	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(5);
	if (MC8123_BIT(val,6)) val ^= MC8123_BITS(1);
	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(6,5);
	if (MC8123_BIT(val,0)) val ^= MC8123_BITS(6,2,1);
	if (MC8123_BIT(val,3)) val ^= MC8123_BITS(7,6,2,1,0);

	val ^= MC8123_BITS(6,4,0);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(4,1);
	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(7,6,3,0);
	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(4,3);
	if (MC8123_BIT(param,0)) val ^= MC8123_BITS(6,2,1,0);

	return val;
}

static uint8_t mc8123_decrypt_type2a(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,0,1,4,3,5,6,2,7);
	if (swap == 1) val = mc8123_bitswap8(val,6,3,0,5,7,4,1,2);
	if (swap == 2) val = mc8123_bitswap8(val,1,6,4,5,0,3,7,2);
	if (swap == 3) val = mc8123_bitswap8(val,4,6,7,5,2,3,1,0);

	if (MC8123_BIT(val,3) || (MC8123_BIT(param,1) && MC8123_BIT(val,2)))
		val = mc8123_bitswap8(val,6,0,7,4,3,2,1,5);

	if (MC8123_BIT(val,5)) val ^= MC8123_BITS(7);
	if (MC8123_BIT(val,6)) val ^= MC8123_BITS(5);
	if (MC8123_BIT(val,0)) val ^= MC8123_BITS(6);
	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(3,0);
	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(2);

	val ^= MC8123_BITS(7,6,5,4,1);

	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(4,3,2,1,0);

	if (MC8123_BIT(param,3))
	{
		if (MC8123_BIT(param,0))
			val = mc8123_bitswap8(val,7,6,5,3,4,1,2,0);
		else
			val = mc8123_bitswap8(val,7,6,5,1,2,4,3,0);
	}
	else if (MC8123_BIT(param,0))
	{
		val = mc8123_bitswap8(val,7,6,5,2,1,3,4,0);
	}

	return val;
}

static uint8_t mc8123_decrypt_type2b(uint8_t val, uint8_t param, unsigned swap)
{
	// only 0x20 possible encryptions for this method - all others have 0x40
	// this happens because MC8123_BIT(param,2) cancels the other three

	if (swap == 0) val = mc8123_bitswap8(val,1,3,4,6,5,7,0,2);
	if (swap == 1) val = mc8123_bitswap8(val,0,1,5,4,7,3,2,6);
	if (swap == 2) val = mc8123_bitswap8(val,3,5,4,1,6,2,0,7);
	if (swap == 3) val = mc8123_bitswap8(val,5,2,3,0,4,7,6,1);

	if (MC8123_BIT(val,7) && MC8123_BIT(val,3))
		val ^= MC8123_BITS(6,4,0);

	if (MC8123_BIT(val,7)) val ^= MC8123_BITS(2);
	if (MC8123_BIT(val,5)) val ^= MC8123_BITS(7,3);
	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(5);
	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(7,5,3,1);

	if (MC8123_BIT(val,7) && MC8123_BIT(val,5))
		val ^= MC8123_BITS(4,0);

	if (MC8123_BIT(val,5) && MC8123_BIT(val,1))
		val ^= MC8123_BITS(4,0);

	if (MC8123_BIT(val,6)) val ^= MC8123_BITS(7,5);
	if (MC8123_BIT(val,3)) val ^= MC8123_BITS(7,6,5,1);
	if (MC8123_BIT(val,2)) val ^= MC8123_BITS(3,1);

	val ^= MC8123_BITS(7,3,2,1);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(6,3,1);
	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(7,6,5,3,2,1); // same as the other three combined
	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(7);
	if (MC8123_BIT(param,0)) val ^= MC8123_BITS(5,2);

	return val;
}

static uint8_t mc8123_decrypt_type3a(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,5,3,1,7,0,2,6,4);
	if (swap == 1) val = mc8123_bitswap8(val,3,1,2,5,4,7,0,6);
	if (swap == 2) val = mc8123_bitswap8(val,5,6,1,2,7,0,4,3);
	if (swap == 3) val = mc8123_bitswap8(val,5,6,7,0,4,2,1,3);

	if (MC8123_BIT(val,2)) val ^= MC8123_BITS(7,5,4);
	if (MC8123_BIT(val,3)) val ^= MC8123_BITS(0);

	if (MC8123_BIT(param,0)) val = mc8123_bitswap8(val,7,2,5,4,3,1,0,6);

	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(6,0);
	if (MC8123_BIT(val,3)) val ^= MC8123_BITS(4,2,1);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(4,3);

	if (MC8123_BIT(val,3)) val = mc8123_bitswap8(val,5,6,7,4,3,2,1,0);

	if (MC8123_BIT(val,5)) val ^= MC8123_BITS(2,1);

	val ^= MC8123_BITS(6,5,4,3);

	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(7);
	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(4);
	if (MC8123_BIT(param,0)) val ^= MC8123_BITS(0);

	return val;
}

static uint8_t mc8123_decrypt_type3b(uint8_t val, uint8_t param, unsigned swap)
{
	if (swap == 0) val = mc8123_bitswap8(val,3,7,5,4,0,6,2,1);
	if (swap == 1) val = mc8123_bitswap8(val,7,5,4,6,1,2,0,3);
	if (swap == 2) val = mc8123_bitswap8(val,7,4,3,0,5,1,6,2);
	if (swap == 3) val = mc8123_bitswap8(val,2,6,4,1,3,7,0,5);

	if (MC8123_BIT(val,2)) val ^= MC8123_BITS(7);

	if (MC8123_BIT(val,7)) val = mc8123_bitswap8(val,7,6,3,4,5,2,1,0);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(7);

	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(6);
	if (MC8123_BIT(val,1)) val ^= MC8123_BITS(6,4,2);

	if (MC8123_BIT(val,7) && MC8123_BIT(val,6))
		val ^= MC8123_BITS(1);

	if (MC8123_BIT(val,7)) val ^= MC8123_BITS(1);

	if (MC8123_BIT(param,3)) val ^= MC8123_BITS(7);
	if (MC8123_BIT(param,2)) val ^= MC8123_BITS(0);

	if (MC8123_BIT(param,3)) val = mc8123_bitswap8(val,4,6,3,2,5,0,1,7);

	if (MC8123_BIT(val,4)) val ^= MC8123_BITS(1);
	if (MC8123_BIT(val,5)) val ^= MC8123_BITS(4);
	if (MC8123_BIT(val,7)) val ^= MC8123_BITS(2);

	val ^= MC8123_BITS(5,3,2);

	if (MC8123_BIT(param,1)) val ^= MC8123_BITS(7);
	if (MC8123_BIT(param,0)) val ^= MC8123_BITS(3);

	return val;
}

static uint8_t mc8123_decrypt_internal(uint8_t val, uint8_t key, bool opcode)
{
	unsigned type = 0;
	unsigned swap = 0;
	uint8_t param = 0;

	key ^= 0xff;

	// no encryption
	if (key == 0x00)
		return val;

	type ^= MC8123_BIT(key,0) << 0;
	type ^= MC8123_BIT(key,2) << 0;
	type ^= MC8123_BIT(key,0) << 1;
	type ^= MC8123_BIT(key,1) << 1;
	type ^= MC8123_BIT(key,2) << 1;
	type ^= MC8123_BIT(key,4) << 1;
	type ^= MC8123_BIT(key,4) << 2;
	type ^= MC8123_BIT(key,5) << 2;

	swap ^= MC8123_BIT(key,0) << 0;
	swap ^= MC8123_BIT(key,1) << 0;
	swap ^= MC8123_BIT(key,2) << 1;
	swap ^= MC8123_BIT(key,3) << 1;

	param ^= MC8123_BIT(key,0) << 0;
	param ^= MC8123_BIT(key,0) << 1;
	param ^= MC8123_BIT(key,2) << 1;
	param ^= MC8123_BIT(key,3) << 1;
	param ^= MC8123_BIT(key,0) << 2;
	param ^= MC8123_BIT(key,1) << 2;
	param ^= MC8123_BIT(key,6) << 2;
	param ^= MC8123_BIT(key,1) << 3;
	param ^= MC8123_BIT(key,6) << 3;
	param ^= MC8123_BIT(key,7) << 3;

	if (!opcode)
	{
		param ^= MC8123_BITS(0);
		type ^= MC8123_BITS(0);
	}

	switch (type)
	{
		default:
		case 0: return mc8123_decrypt_type0(val, param, swap);
		case 1: return mc8123_decrypt_type0(val, param, swap);
		case 2: return mc8123_decrypt_type1a(val, param, swap);
		case 3: return mc8123_decrypt_type1b(val, param, swap);
		case 4: return mc8123_decrypt_type2a(val, param, swap);
		case 5: return mc8123_decrypt_type2b(val, param, swap);
		case 6: return mc8123_decrypt_type3a(val, param, swap);
		case 7: return mc8123_decrypt_type3b(val, param, swap);
	}
}


static uint8_t mc8123_decrypt_byte(const uint8_t *key, uint32_t addr, uint8_t val, bool opcode)
{
	// pick the translation table from bits fd57 of the address
	const uint32_t tbl_num = mc8123_bitswap12(addr,15,14,13,12,11,10,8,6,4,2,1,0);

	return mc8123_decrypt_internal(val, key[tbl_num | (opcode ? 0x0000 : 0x1000)], opcode);
}



void mc8123_generate_key(uint8_t key[0x2000], uint32_t seed, unsigned upper_bound)
{
    unsigned i;
    for (i = 0; i < upper_bound && i < 0x2000u; ++i)
    {
        uint8_t byteval;
        do
        {
            seed = (uint32_t)(seed * 0x29u);
            seed = (uint32_t)(seed + (seed << 16));
            byteval = (uint8_t)((~seed >> 16) & 0xffu);
        }
        while (byteval == 0xffu);
        key[i] = byteval;
    }
    for (; i < 0x2000u; ++i)
        key[i] = 0xffu;
}

void mc8123_decode(uint8_t *rom, uint8_t *opcodes, const uint8_t key[0x2000], unsigned length)
{
    unsigned i;
    if (!rom || !opcodes || !key) return;
    for (i = 0; i < length; i++)
    {
        const uint32_t adr = (i >= 0xc000u) ? ((i & 0x3fffu) | 0x8000u) : i;
        const uint8_t src = rom[i];
        opcodes[i] = mc8123_decrypt_byte(key, adr, src, true);
        rom[i] = mc8123_decrypt_byte(key, adr, src, false);
    }
}
