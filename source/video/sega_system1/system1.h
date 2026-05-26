#ifndef SYSTEM1_H_
#define SYSTEM1_H_

#include <stdint.h>

#define SYSTEM1_CYCLES_PER_LINE 256
#define SYSTEM1_RAW_WIDTH       512
#define SYSTEM1_VISIBLE_WIDTH   256
#define SYSTEM1_VISIBLE_HEIGHT  224
#define SYSTEM1_LINES_PER_FRAME 262

#define SYSTEM1_ROTATE_NONE 0
#define SYSTEM1_ROTATE_CW   1
#define SYSTEM1_ROTATE_CCW  2

#define SYSTEM1_REGION_TILES    0
#define SYSTEM1_REGION_SPRITES  1
#define SYSTEM1_REGION_SOUND    2
#define SYSTEM1_REGION_PROM     3
#define SYSTEM1_REGION_OPCODES  4
#define SYSTEM1_REGION_COLOR    5

int system1_alloc(void);
void system1_free(void);
void system1_clear_roms(void);
int system1_set_region(int region, uint32_t offset, const uint8_t *data, uint32_t size);
void system1_set_game_blockgal(void);
void system1_set_game_blockgal_mc8123(void);
void system1_set_game_choplifter(void);
void system1_set_game_flicky(void);
void system1_set_game_teddybb(void);
void system1_set_game_wboy(void);
void system1_set_game_wbml(void);
void system1_set_game_gardia(void);
void system1_set_game_ufosensi(void);
void system1_set_game_brain(void);
void system1_set_game_starjack(void);
void system1_set_game_upndown(void);
void system1_set_game_swat(void);
void system1_set_game_wmatch(void);
void system1_set_game_spatter(void);
void system1_set_game_pitfall2(void);
void system1_set_game_seganinj(void);
void system1_set_game_imsorry(void);
void system1_set_game_myhero(void);
void system1_set_game_nob(void);
int system1_uses_dial(void);
void system1_memory_map(int clear_ram);
void system1_bank_w(uint8_t data);
uint8_t system1_readmem(uint16_t address);
void system1_writemem(uint16_t address, uint8_t data);
void system1_port_w(uint16_t port, uint8_t data);
uint8_t system1_port_r(uint16_t port);
void system1_reset(void);
void system1_frame(uint32_t skip_render);

#endif
