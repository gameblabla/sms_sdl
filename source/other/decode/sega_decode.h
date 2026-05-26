#ifndef SEGA_DECODE_H_
#define SEGA_DECODE_H_

#include <stdint.h>

void sega_decode_315_5051(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5135(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5155(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5006(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5177(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5176(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5162(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5178(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5179(uint8_t *data, uint8_t *opcodes);
void sega_decode_317_0006(uint8_t *data, uint8_t *opcodes);
void sega_decode_317_0007(uint8_t *data, uint8_t *opcodes);
void sega_decode_315_5098(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5048(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5064(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5093(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5102(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5110(uint8_t *data, uint8_t *opcodes, uint32_t size);
void sega_decode_315_5132(uint8_t *data, uint8_t *opcodes, uint32_t size);

#endif
