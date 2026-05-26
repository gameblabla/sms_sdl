#include "shared.h"
#include "sdl12_common.h"

#define DIAL_STEP 5

static void set_mask8(uint8_t *value, uint8_t mask, int32_t pressed)
{
    if (pressed) *value |= mask;
    else *value &= (uint8_t)~mask;
}

void smsplus_sdl12_keymap_defaults(smsplus_sdl12_keymap_t *map)
{
    if (!map) return;
    memset(map, 0, sizeof(*map));
    map->up = SDLK_UP;
    map->down = SDLK_DOWN;
    map->left = SDLK_LEFT;
    map->right = SDLK_RIGHT;
    map->button1 = SDLK_LALT;
    map->button2 = SDLK_LCTRL;
    map->start = SDLK_RETURN;
    map->select = SDLK_ESCAPE;
    map->keypad[0] = SDLK_0;
    map->keypad[1] = SDLK_1;
    map->keypad[2] = SDLK_2;
    map->keypad[3] = SDLK_3;
    map->keypad[4] = SDLK_4;
    map->keypad[5] = SDLK_5;
    map->keypad[6] = SDLK_6;
    map->keypad[7] = SDLK_7;
    map->keypad[8] = SDLK_8;
    map->keypad[9] = SDLK_9;
    map->keypad[10] = SDLK_DOLLAR;
    map->keypad[11] = SDLK_ASTERISK;
}

int smsplus_sdl12_arcade_active(void)
{
    return sms.console == CONSOLE_SYSTEME || sms.console == CONSOLE_SYSTEM1 || sms.console == CONSOLE_SNKPSYCHOS;
}

void smsplus_sdl12_set_arcade_button(uint8_t mask, int32_t pressed)
{
    if (!smsplus_sdl12_arcade_active())
    {
        input.arcade = 0;
        return;
    }
    set_mask8(&input.arcade, mask, pressed);
}

static int handle_arcade_key(SDLKey key, int32_t pressed, const smsplus_sdl12_keymap_t *map)
{
    if (!smsplus_sdl12_arcade_active()) return 0;

    if (key == SDLK_5 || key == SDLK_KP5)
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_COIN1, pressed);
        return 1;
    }
    if (key == SDLK_6 || key == SDLK_KP6)
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_COIN2, pressed);
        return 1;
    }
    if (key == SDLK_1 || key == SDLK_KP1 || (map && key == map->start))
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_START1, pressed);
        return 1;
    }
    if (key == SDLK_2 || key == SDLK_KP2)
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_START2, pressed);
        return 1;
    }
    if (key == SDLK_9 || key == SDLK_KP9)
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_SERVICE, pressed);
        return 1;
    }
    if (key == SDLK_F2)
    {
        smsplus_sdl12_set_arcade_button(INPUT_ARCADE_TEST, pressed);
        return 1;
    }
    return 0;
}

static void set_pad_key(SDLKey key, int32_t pressed, const smsplus_sdl12_keymap_t *map)
{
    if (key == map->up) set_mask8(&input.pad[0], INPUT_UP, pressed);
    else if (key == map->down) set_mask8(&input.pad[0], INPUT_DOWN, pressed);
    else if (key == map->left) set_mask8(&input.pad[0], INPUT_LEFT, pressed);
    else if (key == map->right) set_mask8(&input.pad[0], INPUT_RIGHT, pressed);
    else if (key == map->button1) set_mask8(&input.pad[0], INPUT_BUTTON1, pressed);
    else if (key == map->button2) set_mask8(&input.pad[0], INPUT_BUTTON2, pressed);
    else if (key == map->start && !smsplus_sdl12_arcade_active())
    {
        uint8_t mask = (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
        set_mask8(&input.system, mask, pressed);
    }
}

static void set_coleco_keypad(SDLKey key, const smsplus_sdl12_keymap_t *map)
{
    int i;
    coleco.keypad[0] = 0xff;
    coleco.keypad[1] = 0xff;
    for (i = 0; i < 10; i++)
    {
        if (key == map->keypad[i] || key == (SDLKey)(SDLK_KP0 + i))
        {
            coleco.keypad[0] = (uint8_t)i;
            return;
        }
    }
    if (key == map->keypad[10] || key == SDLK_KP_MULTIPLY || key == SDLK_KP_MINUS)
        coleco.keypad[0] = 10;
    else if (key == map->keypad[11] || key == SDLK_KP_DIVIDE || key == SDLK_KP_PLUS)
        coleco.keypad[0] = 11;
}

uint32_t smsplus_sdl12_update_key(SDLKey key, int32_t pressed,
                                  const smsplus_sdl12_keymap_t *map,
                                  uint8_t *select_pressed)
{
    smsplus_sdl12_keymap_t default_map;
    if (!map)
    {
        smsplus_sdl12_keymap_defaults(&default_map);
        map = &default_map;
    }

    if (key == map->select)
    {
        if (select_pressed) *select_pressed = pressed ? 1 : 0;
        return 1;
    }

    if (handle_arcade_key(key, pressed, map))
        return 1;

    set_pad_key(key, pressed, map);

    if (sms.console == CONSOLE_COLECO)
    {
        set_coleco_keypad(key, map);
        input.system = 0;
    }
    return 1;
}

static int key_down(const uint8_t *keys, SDLKey key)
{
    if (!keys || key <= 0) return 0;
    return keys[key] != 0;
}

void smsplus_sdl12_update_arcade_from_key_state(const uint8_t *keys)
{
    uint8_t arcade = 0;
    if (!smsplus_sdl12_arcade_active())
    {
        input.arcade = 0;
        return;
    }
    if (key_down(keys, SDLK_5) || key_down(keys, SDLK_KP5)) arcade |= INPUT_ARCADE_COIN1;
    if (key_down(keys, SDLK_6) || key_down(keys, SDLK_KP6)) arcade |= INPUT_ARCADE_COIN2;
    if (key_down(keys, SDLK_1) || key_down(keys, SDLK_KP1) || key_down(keys, SDLK_RETURN)) arcade |= INPUT_ARCADE_START1;
    if (key_down(keys, SDLK_2) || key_down(keys, SDLK_KP2)) arcade |= INPUT_ARCADE_START2;
    if (key_down(keys, SDLK_9) || key_down(keys, SDLK_KP9)) arcade |= INPUT_ARCADE_SERVICE;
    if (key_down(keys, SDLK_F2)) arcade |= INPUT_ARCADE_TEST;
    input.arcade = arcade;
}

void smsplus_sdl12_state_file(const char *stdir, const char *gamename, uint8_t slot_number, uint8_t mode)
{
    char stpath[PATH_MAX];
    FILE *fd;

    if (!stdir || !gamename) return;
    snprintf(stpath, sizeof(stpath), "%s%s.st%d", stdir, gamename, slot_number);

    switch (mode)
    {
        case 0:
            fd = fopen(stpath, "wb");
            if (fd)
            {
                system_save_state(fd);
                fclose(fd);
            }
            break;

        case 1:
            fd = fopen(stpath, "rb");
            if (fd)
            {
                system_load_state(fd);
                fclose(fd);
            }
            break;
    }
}

void smsplus_sdl12_sram_file(const char *sramfile, uint8_t *sram, uint8_t mode)
{
    FILE *fd;

    if (!sramfile || !sram) return;

    switch (mode)
    {
        case SRAM_SAVE:
            if (sms.save)
            {
                fd = fopen(sramfile, "wb");
                if (fd)
                {
                    fwrite(sram, 0x8000, 1, fd);
                    fclose(fd);
                }
            }
            break;

        case SRAM_LOAD:
            fd = fopen(sramfile, "rb");
            if (fd)
            {
                sms.save = 1;
                fread(sram, 0x8000, 1, fd);
                fclose(fd);
            }
            else
            {
                memset(sram, 0x00, 0x8000);
            }
            break;
    }
}

void smsplus_sdl12_frame_update(void)
{
    if (!smsplus_sdl12_arcade_active())
        input.arcade = 0;

    if (sms.console == CONSOLE_SYSTEM1 && system1_uses_dial())
    {
        if ((input.pad[0] & INPUT_LEFT) && !(input.pad[0] & INPUT_RIGHT))
            input.analog[0][0] = (input.analog[0][0] - DIAL_STEP) & 0xff;
        else if ((input.pad[0] & INPUT_RIGHT) && !(input.pad[0] & INPUT_LEFT))
            input.analog[0][0] = (input.analog[0][0] + DIAL_STEP) & 0xff;
    }
}
