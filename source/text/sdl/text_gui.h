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

#ifndef TEXTGUI_H_
#define TEXTGUI_H_

extern SDL_Surface *font, *bigfontred, *bigfontwhite;

SDL_Surface* gfx_tex_load_tga_from_array(uint8_t* buffer);
void gfx_font_print(SDL_Surface* dest, int32_t inX, int32_t inY, SDL_Surface* inFont, char* inString);
void gfx_font_print_center(SDL_Surface* dest,int32_t inY, SDL_Surface* inFont, char* inString);
int32_t gfx_font_height(SDL_Surface* inFont);
int32_t gfx_font_width(SDL_Surface* inFont, char* inString);
void gfx_font_print_fromright(SDL_Surface* dest,int32_t inX, int32_t inY, SDL_Surface* inFont, char* inString);

#endif
