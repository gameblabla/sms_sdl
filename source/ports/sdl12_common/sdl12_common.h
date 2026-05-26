#ifndef SMSPLUS_SDL12_COMMON_H_
#define SMSPLUS_SDL12_COMMON_H_

#include <stdint.h>
#include <SDL/SDL.h>

/*
 * Shared SDL 1.2 input helpers for the legacy SDL/handheld backends.
 * The individual ports keep their menu/video setup, but normal game input,
 * arcade coin/start/service/test handling, Coleco keypad mapping, and the
 * System 1 dial fallback live here so fixes do not have to be copied across
 * a dozen frontends.
 */
typedef struct smsplus_sdl12_keymap_t
{
    SDLKey up;
    SDLKey down;
    SDLKey left;
    SDLKey right;
    SDLKey button1;     /* core INPUT_BUTTON1 */
    SDLKey button2;     /* core INPUT_BUTTON2 */
    SDLKey start;
    SDLKey select;
    SDLKey keypad[12];  /* 0..9, *, #/$-style aliases */
} smsplus_sdl12_keymap_t;

void smsplus_sdl12_keymap_defaults(smsplus_sdl12_keymap_t *map);
int smsplus_sdl12_arcade_active(void);
uint32_t smsplus_sdl12_update_key(SDLKey key, int32_t pressed,
                                  const smsplus_sdl12_keymap_t *map,
                                  uint8_t *select_pressed);
void smsplus_sdl12_update_arcade_from_key_state(const uint8_t *keys);
void smsplus_sdl12_frame_update(void);
void smsplus_sdl12_set_arcade_button(uint8_t mask, int32_t pressed);

void smsplus_sdl12_state_file(const char *stdir, const char *gamename, uint8_t slot_number, uint8_t mode);
void smsplus_sdl12_sram_file(const char *sramfile, uint8_t *sram, uint8_t mode);

static inline void smsplus_sdl12_keymap_from_config(smsplus_sdl12_keymap_t *map,
                                                    const uint32_t *config_buttons)
{
    smsplus_sdl12_keymap_defaults(map);
    if (!config_buttons) return;
#ifdef CONFIG_BUTTON_UP
    map->up = (SDLKey)config_buttons[CONFIG_BUTTON_UP];
#endif
#ifdef CONFIG_BUTTON_DOWN
    map->down = (SDLKey)config_buttons[CONFIG_BUTTON_DOWN];
#endif
#ifdef CONFIG_BUTTON_LEFT
    map->left = (SDLKey)config_buttons[CONFIG_BUTTON_LEFT];
#endif
#ifdef CONFIG_BUTTON_RIGHT
    map->right = (SDLKey)config_buttons[CONFIG_BUTTON_RIGHT];
#endif
#ifdef CONFIG_BUTTON_BUTTON1
    /* Legacy remappers named this the physical A button; existing ports map it to SMS button 2. */
    map->button2 = (SDLKey)config_buttons[CONFIG_BUTTON_BUTTON1];
#endif
#ifdef CONFIG_BUTTON_BUTTON2
    /* Legacy remappers named this the physical B button; existing ports map it to SMS button 1. */
    map->button1 = (SDLKey)config_buttons[CONFIG_BUTTON_BUTTON2];
#endif
#ifdef CONFIG_BUTTON_START
    map->start = (SDLKey)config_buttons[CONFIG_BUTTON_START];
#endif
#ifdef CONFIG_BUTTON_ONE
    map->keypad[1] = (SDLKey)config_buttons[CONFIG_BUTTON_ONE];
#endif
#ifdef CONFIG_BUTTON_TWO
    map->keypad[2] = (SDLKey)config_buttons[CONFIG_BUTTON_TWO];
#endif
#ifdef CONFIG_BUTTON_THREE
    map->keypad[3] = (SDLKey)config_buttons[CONFIG_BUTTON_THREE];
#endif
#ifdef CONFIG_BUTTON_FOUR
    map->keypad[4] = (SDLKey)config_buttons[CONFIG_BUTTON_FOUR];
#endif
#ifdef CONFIG_BUTTON_FIVE
    map->keypad[5] = (SDLKey)config_buttons[CONFIG_BUTTON_FIVE];
#endif
#ifdef CONFIG_BUTTON_SIX
    map->keypad[6] = (SDLKey)config_buttons[CONFIG_BUTTON_SIX];
#endif
#ifdef CONFIG_BUTTON_SEVEN
    map->keypad[7] = (SDLKey)config_buttons[CONFIG_BUTTON_SEVEN];
#endif
#ifdef CONFIG_BUTTON_EIGHT
    map->keypad[8] = (SDLKey)config_buttons[CONFIG_BUTTON_EIGHT];
#endif
#ifdef CONFIG_BUTTON_NINE
    map->keypad[9] = (SDLKey)config_buttons[CONFIG_BUTTON_NINE];
#endif
#ifdef CONFIG_BUTTON_DOLLARS
    map->keypad[10] = (SDLKey)config_buttons[CONFIG_BUTTON_DOLLARS];
#endif
#ifdef CONFIG_BUTTON_ASTERISK
    map->keypad[11] = (SDLKey)config_buttons[CONFIG_BUTTON_ASTERISK];
#endif
}

#endif /* SMSPLUS_SDL12_COMMON_H_ */
