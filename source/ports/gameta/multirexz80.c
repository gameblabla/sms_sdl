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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <SDL/SDL.h>

#include "shared.h"
#include "scaler.h"
#include "multirexz80.h"
#include "sdl12_common.h"
#include "font_drawing.h"
#include "sound_output.h"

static gamedata_t gdata;

t_config option;

static char home_path[256];

static SDL_Surface* sdl_screen, *scale2x_buf;
SDL_Surface *sms_bitmap;
static SDL_Surface *backbuffer;
extern SDL_Surface *font;
extern SDL_Surface *bigfontred;
extern SDL_Surface *bigfontwhite;

static uint8_t selectpressed = 0;
static uint8_t save_slot = 0;
static uint8_t quit = 0;

static const int8_t upscalers_available = 2
#ifdef SCALE2X_UPSCALER
+1
#endif
;

static void video_update(void)
{
	multirexz80_sdl12_view_t view;
	SDL_Rect dst;
	int screen_pitch;
	multirexz80_sdl12_get_active_view(&view);

	if (SDL_LockSurface(sdl_screen) == 0)
	{
		screen_pitch = multirexz80_sdl12_surface_pitch_pixels(sdl_screen);
		if (screen_pitch <= 0) screen_pitch = sdl_screen->w;
		SDL_FillRect(sdl_screen, NULL, 0);
		switch(option.fullscreen)
		{
			case 0:
				dst.x = (Sint16)((sdl_screen->w - view.w) / 2);
				dst.y = (Sint16)((sdl_screen->h - view.h) / 2);
				dst.w = (Uint16)view.w;
				dst.h = (Uint16)view.h;
				if (dst.x < 0 || dst.y < 0)
					multirexz80_sdl12_fit_rect(&dst, sdl_screen->w, sdl_screen->h, view.w, view.h);
				break;
			case 1:
			default:
				dst.x = 0;
				dst.y = 0;
				dst.w = (Uint16)sdl_screen->w;
				dst.h = (Uint16)sdl_screen->h;
				break;
			case 2:
				multirexz80_sdl12_fit_rect(&dst, sdl_screen->w, sdl_screen->h, view.w, view.h);
				break;
			case 3:
			case 4:
#ifdef SCALE2X_UPSCALER
				if (view.w * 2 <= scale2x_buf->w && view.h * 2 <= scale2x_buf->h)
				{
					scale2x((uint16_t *)sms_bitmap->pixels + view.y * view.pitch_pixels + view.x,
					        (uint16_t *)scale2x_buf->pixels,
					        sms_bitmap->pitch,
					        scale2x_buf->pitch,
					        view.w, view.h);
					multirexz80_sdl12_fit_rect(&dst, sdl_screen->w, sdl_screen->h, view.w * 2, view.h * 2);
					bitmap_scale(0, 0, view.w * 2, view.h * 2, dst.w, dst.h,
					             scale2x_buf->pitch >> 1, screen_pitch - dst.w,
					             (uint16_t * restrict)scale2x_buf->pixels,
					             (uint16_t * restrict)sdl_screen->pixels + dst.x + dst.y * screen_pitch);
					SDL_UnlockSurface(sdl_screen);
					SDL_Flip(sdl_screen);
					return;
				}
#endif
				multirexz80_sdl12_fit_rect(&dst, sdl_screen->w, sdl_screen->h, view.w, view.h);
				break;
		}
		bitmap_scale(view.x, view.y, view.w, view.h, dst.w, dst.h,
		             view.pitch_pixels, screen_pitch - dst.w,
		             (uint16_t * restrict)sms_bitmap->pixels,
		             (uint16_t * restrict)sdl_screen->pixels + dst.x + dst.y * screen_pitch);
		SDL_UnlockSurface(sdl_screen);
	}
	SDL_Flip(sdl_screen);
}

void smsp_state(uint8_t slot_number, uint8_t mode)
{
	multirexz80_sdl12_state_file(gdata.stdir, gdata.gamename, slot_number, mode);
}

void system_manage_sram(uint8_t *sram, uint8_t slot_number, uint8_t mode)
{
	(void)slot_number;
	multirexz80_sdl12_sram_file(gdata.sramfile, sram, mode);
}

static uint32_t sdl_controls_update_input(SDLKey k, int32_t p)
{
	multirexz80_sdl12_keymap_t map;
	multirexz80_sdl12_keymap_from_config(&map, option.config_buttons);
	return multirexz80_sdl12_update_key(k, p, &map, &selectpressed);
}



static void bios_init()
{
	FILE *fd;
	char bios_path[256];
	
	bios.rom = malloc(0x100000);
	bios.enabled = 0;
	
	snprintf(bios_path, sizeof(bios_path), "%s%s", gdata.biosdir, "BIOS.sms");

	fd = fopen(bios_path, "rb");
	if(fd)
	{
		/* Seek to end of file, and get size */
		fseek(fd, 0, SEEK_END);
		uint32_t size = ftell(fd);
		fseek(fd, 0, SEEK_SET);
		if (size < 0x4000) size = 0x4000;
		fread(bios.rom, size, 1, fd);
		bios.enabled = 2;  
		bios.pages = size / 0x4000;
		fclose(fd);
	}

	snprintf(bios_path, sizeof(bios_path), "%s%s", gdata.biosdir, "BIOS.col");
	
	fd = fopen(bios_path, "rb");
	if(fd)
	{
		/* Seek to end of file, and get size */
		fread(coleco.rom, 0x2000, 1, fd);
		fclose(fd);
	}
}


static void smsp_gamedata_set(char *filename) 
{
	// Set paths, create directories
	snprintf(home_path, sizeof(home_path), "%s/.multirexz80/", getenv("HOME"));
	
	if (mkdir(home_path, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", home_path, errno);
	}
	
	// Set the game name
	snprintf(gdata.gamename, sizeof(gdata.gamename), "%s", basename(filename));
	
	// Strip the file extension off
	for (unsigned long i = strlen(gdata.gamename) - 1; i > 0; i--) {
		if (gdata.gamename[i] == '.') {
			gdata.gamename[i] = '\0';
			break;
		}
	}
	
	// Set up the sram directory
	snprintf(gdata.sramdir, sizeof(gdata.sramdir), "%ssram/", home_path);
	if (mkdir(gdata.sramdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.sramdir, errno);
	}
	
	// Set up the sram file
	snprintf(gdata.sramfile, sizeof(gdata.sramfile), "%s%s.sav", gdata.sramdir, gdata.gamename);
	
	// Set up the state directory
	snprintf(gdata.stdir, sizeof(gdata.stdir), "%sstate/", home_path);
	if (mkdir(gdata.stdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.stdir, errno);
	}
	
	// Set up the screenshot directory
	snprintf(gdata.scrdir, sizeof(gdata.scrdir), "%sscreenshots/", home_path);
	if (mkdir(gdata.scrdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.scrdir, errno);
	}
	
	// Set up the sram directory
	snprintf(gdata.biosdir, sizeof(gdata.biosdir), "%sbios/", home_path);
	if (mkdir(gdata.biosdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.sramdir, errno);
	}
	
}

void Menu()
{
	char text[50];
    int16_t pressed = 0;
    int16_t currentselection = 1;
    uint16_t miniscreenwidth = 160;
    uint16_t miniscreenheight = 144;
    SDL_Rect dstRect;
    SDL_Event Event;
    
    SDL_Surface* miniscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, miniscreenwidth, miniscreenheight, 16, sdl_screen->format->Rmask, sdl_screen->format->Gmask, sdl_screen->format->Bmask, sdl_screen->format->Amask);
    SDL_LockSurface(miniscreen);
    {
        multirexz80_sdl12_view_t view;
        multirexz80_sdl12_get_active_view(&view);
        bitmap_scale(view.x, view.y, view.w, view.h, miniscreenwidth, miniscreenheight,
                     view.pitch_pixels, 0,
                     (uint16_t* restrict)sms_bitmap->pixels,
                     (uint16_t* restrict)miniscreen->pixels);
    }
    SDL_UnlockSurface(miniscreen);
    
    Sound_Pause();
    
    while (((currentselection != 1) && (currentselection != 6)) || (!pressed))
    {
        pressed = 0;
 		SDL_FillRect( sdl_screen, NULL, 0 );
		
        dstRect.x = HOST_WIDTH_RESOLUTION-5-miniscreenwidth;
        dstRect.y = 28;
        SDL_BlitSurface(miniscreen,NULL,sdl_screen,&dstRect);

		print_string("SMS PLUS GX", TextWhite, 0, 105, 15, sdl_screen->pixels);
		
		if (currentselection == 1) print_string("Continue", TextRed, 0, 5, 45, sdl_screen->pixels);
		else  print_string("Continue", TextWhite, 0, 5, 45, sdl_screen->pixels);
		
		snprintf(text, sizeof(text), "Load State %d", save_slot);
		
		if (currentselection == 2) print_string(text, TextRed, 0, 5, 65, sdl_screen->pixels);
		else print_string(text, TextWhite, 0, 5, 65, sdl_screen->pixels);
		
		snprintf(text, sizeof(text), "Save State %d", save_slot);
		
		if (currentselection == 3) print_string(text, TextRed, 0, 5, 85, sdl_screen->pixels);
		else print_string(text, TextWhite, 0, 5, 85, sdl_screen->pixels);
		

        if (currentselection == 4)
        {
			switch(option.fullscreen)
			{
				case 0:
					print_string("Scaling : Native", TextRed, 0, 5, 105, sdl_screen->pixels);
				break;
				case 1:
					print_string("Scaling : Stretched", TextRed, 0, 5, 105, sdl_screen->pixels);
				break;
				case 2:
					print_string("Scaling : Keep Aspect", TextRed, 0, 5, 105, sdl_screen->pixels);
				break;
				case 3:
					print_string("Scaling : EPX/Scale2x", TextRed, 0, 5, 105, sdl_screen->pixels);
				break;
			}
        }
        else
        {
			switch(option.fullscreen)
			{
				case 0:
					print_string("Scaling : Native", TextWhite, 0, 5, 105, sdl_screen->pixels);
				break;
				case 1:
					print_string("Scaling : Stretched", TextWhite, 0, 5, 105, sdl_screen->pixels);
				break;
				case 2:
					print_string("Scaling : Keep Aspect", TextWhite, 0, 5, 105, sdl_screen->pixels);
				break;
				case 3:
					print_string("Scaling : EPX/Scale2x", TextWhite, 0, 5, 105, sdl_screen->pixels);
				break;
			}
        }

		snprintf(text, sizeof(text), "Sound volume : %d", option.soundlevel);
		
		if (currentselection == 5) print_string(text, TextRed, 0, 5, 125, sdl_screen->pixels);
		else print_string(text, TextWhite, 0, 5, 125, sdl_screen->pixels);
		
		if (currentselection == 6) print_string("Quit", TextRed, 0, 5, 145, sdl_screen->pixels);
		else print_string("Quit", TextWhite, 0, 5, 145, sdl_screen->pixels);
		
		print_string("Based on SMS Plus GX / SMS Plus", TextWhite, 0, 5, 190, sdl_screen->pixels);
		print_string("Fork of MultiRexZ80 by gameblabla", TextWhite, 0, 5, 215, sdl_screen->pixels);
		print_string("Scaler : Alekmaul", TextWhite, 0, 5, 235, sdl_screen->pixels);
		print_string("Text drawing : n2DLib", TextWhite, 0, 5, 255, sdl_screen->pixels);

        while (SDL_PollEvent(&Event))
        {
            if (Event.type == SDL_KEYDOWN)
            {
                switch(Event.key.keysym.sym)
                {
                    case SDLK_UP:
                        currentselection--;
                        if (currentselection == 0)
                            currentselection = 6;
                        break;
                    case SDLK_DOWN:
                        currentselection++;
                        if (currentselection == 7)
                            currentselection = 1;
                        break;
                    case SDLK_LCTRL:
                    case SDLK_LALT:
                    case SDLK_RETURN:
                        pressed = 1;
                        break;
                    case SDLK_LEFT:
                        switch(currentselection)
                        {
                            case 2:
                            case 3:
                                if (save_slot > 0) save_slot--;
							break;
                            case 4:
							option.fullscreen--;
							if (option.fullscreen < 0)
								option.fullscreen = upscalers_available;
							break;
							case 5:
								option.soundlevel--;
								if (option.soundlevel < 1)
									option.soundlevel = 4;
							break;
                        }
                        break;
                    case SDLK_RIGHT:
                        switch(currentselection)
                        {
                            case 2:
                            case 3:
                                save_slot++;
								if (save_slot == 10)
									save_slot = 9;
							break;
                            case 4:
                                option.fullscreen++;
                                if (option.fullscreen > upscalers_available)
                                    option.fullscreen = 0;
							break;
							case 5:
								option.soundlevel++;
								if (option.soundlevel > 4)
									option.soundlevel = 1;
							break;
                        }
                        break;
					default:
					break;
                }
            }
            else if (Event.type == SDL_QUIT)
            {
				currentselection = 6;
			}
        }

        if (pressed)
        {
            switch(currentselection)
            {
				case 5:
					option.soundlevel++;
					if (option.soundlevel > 4)
						option.soundlevel = 1;
				break;
                case 4 :
                    option.fullscreen++;
                    if (option.fullscreen > upscalers_available)
                        option.fullscreen = 0;
                    break;
                case 2 :
                    smsp_state(save_slot, 1);
					currentselection = 1;
                    break;
                case 3 :
					smsp_state(save_slot, 0);
					currentselection = 1;
				break;
				default:
				break;
            }
        }

		SDL_Flip(sdl_screen);
    }
    
    SDL_FillRect(sdl_screen, NULL, 0);
    SDL_Flip(sdl_screen);
    #ifdef SDL_TRIPLEBUF
    SDL_FillRect(sdl_screen, NULL, 0);
    SDL_Flip(sdl_screen);
    #endif
    
    if (miniscreen) SDL_FreeSurface(miniscreen);
    
    if (currentselection == 6)
        quit = 1;
    else
		Sound_Unpause();
}

static void config_load()
{
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%s/config.cfg", home_path);
	FILE* fp;
	
	printf("config_path %s\n", config_path);
	
	fp = fopen(config_path, "rb");
	if (fp)
	{
		printf("caca\n");
		fread(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
	}
}

static void config_save()
{
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%s/config.cfg", home_path);
	FILE* fp;
	
	fp = fopen(config_path, "wb");
	if (fp)
	{
		fwrite(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
	}
}

static void Cleanup(void)
{
#ifdef SCALE2X_UPSCALER
	if (scale2x_buf) SDL_FreeSurface(scale2x_buf);
#endif
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	if (backbuffer) SDL_FreeSurface(backbuffer);
	if (sms_bitmap) SDL_FreeSurface(sms_bitmap);
	
	if (bios.rom) free(bios.rom);
	
	// Deinitialize audio and video output
	Sound_Close();
	
	SDL_Quit();

	// Shut down
	system_poweroff();
	system_shutdown();	
}

int main (int argc, char *argv[]) 
{
	SDL_Event event;
	
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: ./multirexz80 [FILE]\n");
		return 0;
	}
	
	smsp_gamedata_set(argv[1]);
	
	memset(&option, 0, sizeof(option));
	
	option.fullscreen = 1;
	option.fm = 1;
	option.spritelimit = 1;
	option.tms_pal = 2;
	option.console = 0;
	option.nosound = 0;
	option.soundlevel = 2;
	
	config_load();
	
	if (option.fullscreen < 0 && option.fullscreen > 2) option.fullscreen = 1;
	
	option.console = 0;
	
	strcpy(option.game_name, argv[1]);
	
	// Force Colecovision mode if extension is .col
	if (strcmp(strrchr(argv[1], '.'), ".col") == 0) option.console = 6;
	// Sometimes Game Gear games are not properly detected, force them accordingly
	else if (strcmp(strrchr(argv[1], '.'), ".gg") == 0) option.console = 3;
	
	if (sms.console == CONSOLE_SMS || sms.console == CONSOLE_SMS2)
		sms.use_fm = 1; 
	
	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		Cleanup();
		return 0;
	}
	
	SDL_Init(SDL_INIT_VIDEO);
	sdl_screen = SDL_SetVideoMode(HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, 16, SDL_HWSURFACE);
	sms_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, multirexz80_sdl12_bitmap_width(), multirexz80_sdl12_bitmap_height(), 16, 0, 0, 0, 0);
	backbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, 16, 0, 0, 0, 0);
	font_drawing_set_target((uint16_t *)backbuffer->pixels, backbuffer->pitch >> 1, backbuffer->w, backbuffer->h);
	SDL_ShowCursor(0);

#ifdef SCALE2X_UPSCALER
	scale2x_buf = SDL_CreateRGBSurface(SDL_SWSURFACE, multirexz80_sdl12_bitmap_width()*2, multirexz80_sdl12_bitmap_height()*2, 16, 0, 0, 0, 0);
#endif
	
	fprintf(stdout, "CRC : %08X\n", cart.crc);
	
	// Set parameters for internal bitmap
	bitmap.width = sms_bitmap->w;
	bitmap.height = sms_bitmap->h;
	bitmap.depth = 16;
	bitmap.data = (uint8_t *)sms_bitmap->pixels;
	bitmap.pitch = sms_bitmap->pitch;
	bitmap.viewport.w = VIDEO_WIDTH_SMS;
	bitmap.viewport.h = VIDEO_HEIGHT_SMS;
	bitmap.viewport.x = 0x00;
	bitmap.viewport.y = 0x00;
	
	//sms.territory = settings.misc_region;
	
	bios_init();

	// Initialize all systems and power on
	system_poweron();
	
	Sound_Init();
	
	// Loop until the user closes the window
	while (!quit) 
	{
		multirexz80_sdl12_frame_update();

		// Execute frame(s)
		system_frame(0);
		
		// Refresh sound data
		Sound_Update(snd.output, snd.sample_count);
		
		// Refresh video data
		video_update();

		if (selectpressed == 1)
		{
            Menu();
            SDL_FillRect(sdl_screen, NULL, 0);
            input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;
            selectpressed = 0;
		}
		
		if (SDL_PollEvent(&event)) 
		{
			switch(event.type) 
			{
				case SDL_KEYUP:
					sdl_controls_update_input(event.key.keysym.sym, 0);
				break;
				case SDL_KEYDOWN:
					sdl_controls_update_input(event.key.keysym.sym, 1);
				break;
				case SDL_QUIT:
					quit = 1;
				break;
			}
		}
	}
	
	config_save();
	Cleanup();
	
	return 0;
}
