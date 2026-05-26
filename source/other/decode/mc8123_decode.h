#ifndef SMSPLUS_MC8123_DECODE_H_
#define SMSPLUS_MC8123_DECODE_H_

#include <stdint.h>

void mc8123_generate_key(uint8_t key[0x2000], uint32_t seed, unsigned upper_bound);
void mc8123_decode(uint8_t *rom, uint8_t *opcodes, const uint8_t key[0x2000], unsigned length);

#endif
