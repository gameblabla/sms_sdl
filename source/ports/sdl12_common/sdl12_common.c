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

#include "shared.h"
#include "sdl12_common.h"

#define DIAL_STEP 5

static void set_mask8(uint8_t *value, uint8_t mask, int32_t pressed)
{
    if (pressed) *value |= mask;
    else *value &= (uint8_t)~mask;
}

void multirexz80_sdl12_keymap_defaults(multirexz80_sdl12_keymap_t *map)
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

int multirexz80_sdl12_arcade_active(void)
{
    return sms.console == CONSOLE_SYSTEME || sms.console == CONSOLE_SYSTEM1 || sms.console == CONSOLE_SNKPSYCHOS;
}

void multirexz80_sdl12_set_arcade_button(uint8_t mask, int32_t pressed)
{
    if (!multirexz80_sdl12_arcade_active())
    {
        input.arcade = 0;
        return;
    }
    set_mask8(&input.arcade, mask, pressed);
}

static int handle_arcade_key(SDLKey key, int32_t pressed, const multirexz80_sdl12_keymap_t *map)
{
    if (!multirexz80_sdl12_arcade_active()) return 0;

    if (key == SDLK_5 || key == SDLK_KP5)
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_COIN1, pressed);
        return 1;
    }
    if (key == SDLK_6 || key == SDLK_KP6)
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_COIN2, pressed);
        return 1;
    }
    if (key == SDLK_1 || key == SDLK_KP1 || (map && key == map->start))
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_START1, pressed);
        return 1;
    }
    if (key == SDLK_2 || key == SDLK_KP2)
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_START2, pressed);
        return 1;
    }
    if (key == SDLK_9 || key == SDLK_KP9)
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_SERVICE, pressed);
        return 1;
    }
    if (key == SDLK_F2)
    {
        multirexz80_sdl12_set_arcade_button(INPUT_ARCADE_TEST, pressed);
        return 1;
    }
    return 0;
}

static void set_pad_key(SDLKey key, int32_t pressed, const multirexz80_sdl12_keymap_t *map)
{
    if (key == map->up) set_mask8(&input.pad[0], INPUT_UP, pressed);
    else if (key == map->down) set_mask8(&input.pad[0], INPUT_DOWN, pressed);
    else if (key == map->left) set_mask8(&input.pad[0], INPUT_LEFT, pressed);
    else if (key == map->right) set_mask8(&input.pad[0], INPUT_RIGHT, pressed);
    else if (key == map->button1) set_mask8(&input.pad[0], INPUT_BUTTON1, pressed);
    else if (key == map->button2) set_mask8(&input.pad[0], INPUT_BUTTON2, pressed);
    else if (key == map->start && !multirexz80_sdl12_arcade_active())
    {
        uint8_t mask = (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
        set_mask8(&input.system, mask, pressed);
    }
}

static void set_coleco_keypad(SDLKey key, const multirexz80_sdl12_keymap_t *map)
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

uint32_t multirexz80_sdl12_update_key(SDLKey key, int32_t pressed,
                                  const multirexz80_sdl12_keymap_t *map,
                                  uint8_t *select_pressed)
{
    multirexz80_sdl12_keymap_t default_map;
    if (!map)
    {
        multirexz80_sdl12_keymap_defaults(&default_map);
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

void multirexz80_sdl12_update_arcade_from_key_state(const uint8_t *keys)
{
    uint8_t arcade = 0;
    if (!multirexz80_sdl12_arcade_active())
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


void multirexz80_sdl12_get_active_view(multirexz80_sdl12_view_t *view)
{
    int x, y, w, h, bpp;
    if (!view) return;

    x = bitmap.viewport.x;
    y = bitmap.viewport.y;
    w = bitmap.viewport.w;
    h = bitmap.viewport.h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w <= 0) w = 256;
    if (h <= 0) h = vdp.height ? vdp.height : 192;

    if (!multirexz80_sdl12_arcade_active() && sms.console != CONSOLE_GG &&
        (vdp.reg[0] & 0x20) && x == 0 && w >= 256)
    {
        x += 8;
        w -= 8;
    }

    if (x > (int)bitmap.width) x = (int)bitmap.width;
    if (y > (int)bitmap.height) y = (int)bitmap.height;
    if (x + w > (int)bitmap.width) w = (int)bitmap.width - x;
    if (y + h > (int)bitmap.height) h = (int)bitmap.height - y;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    bpp = (bitmap.depth == 32) ? 4 : (bitmap.depth == 16) ? 2 : 1;
    view->x = x;
    view->y = y;
    view->w = w;
    view->h = h;
    view->bytes_per_pixel = bpp;
    view->pitch_pixels = bitmap.pitch ? (int)(bitmap.pitch / (uint32_t)bpp) : (int)bitmap.width;
}

void multirexz80_sdl12_fit_rect(SDL_Rect *dst, int dst_w, int dst_h, int src_w, int src_h)
{
    int w, h;
    if (!dst) return;
    if (dst_w < 1) dst_w = 1;
    if (dst_h < 1) dst_h = 1;
    if (src_w < 1) src_w = 1;
    if (src_h < 1) src_h = 1;

    w = dst_w;
    h = (int)(((int64_t)dst_w * src_h + src_w / 2) / src_w);
    if (h > dst_h)
    {
        h = dst_h;
        w = (int)(((int64_t)dst_h * src_w + src_h / 2) / src_h);
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    dst->x = (Sint16)((dst_w - w) / 2);
    dst->y = (Sint16)((dst_h - h) / 2);
    dst->w = (Uint16)w;
    dst->h = (Uint16)h;
}

int multirexz80_sdl12_bitmap_width(void)
{
    return 400;
}

int multirexz80_sdl12_bitmap_height(void)
{
    return 400;
}

int multirexz80_sdl12_surface_pitch_pixels(const SDL_Surface *surface)
{
    if (!surface || !surface->format || surface->format->BytesPerPixel == 0)
        return 0;
    return surface->pitch / surface->format->BytesPerPixel;
}

SDL_Surface *multirexz80_sdl12_create_rgb565_surface(int width, int height)
{
    if (width < 1) width = 1;
    if (height < 1) height = 1;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
                                0x001f, 0x07e0, 0xf800, 0x0000);
#else
    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
                                0xf800, 0x07e0, 0x001f, 0x0000);
#endif
}

void multirexz80_sdl12_state_file(const char *stdir, const char *gamename, uint8_t slot_number, uint8_t mode)
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

void multirexz80_sdl12_sram_file(const char *sramfile, uint8_t *sram, uint8_t mode)
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

void multirexz80_sdl12_frame_update(void)
{
    if (!multirexz80_sdl12_arcade_active())
        input.arcade = 0;

    if (sms.console == CONSOLE_SYSTEM1 && system1_uses_dial())
    {
        if ((input.pad[0] & INPUT_LEFT) && !(input.pad[0] & INPUT_RIGHT))
            input.analog[0][0] = (input.analog[0][0] - DIAL_STEP) & 0xff;
        else if ((input.pad[0] & INPUT_RIGHT) && !(input.pad[0] & INPUT_LEFT))
            input.analog[0][0] = (input.analog[0][0] + DIAL_STEP) & 0xff;
    }
}
