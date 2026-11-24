/* Modified by Titus Engine contributors, 2026-11-24.
 * DOS parity changes and input-source hashes: ../UPSTREAM.json. */
/*   
 * Copyright (C) 2008 - 2011 The OpenTitus team
 *
 * Authors:
 * Eirik Stople
 *
 * "Titus the Fox: To Marrakech and Back" (1992) and
 * "Lagaf': Les Aventures de Moktar - Vol 1: La Zoubida" (1991)
 * was developed by, and is probably copyrighted by Titus Software,
 * which, according to Wikipedia, stopped buisness in 2005.
 *
 * OpenTitus is not affiliated with Titus Software.
 *
 * OpenTitus is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 3  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

/* draw.c
 * Draw functions
 *
 * Global functions:
 * int TFR_SCREENM(): Draw tiles on the backbuffer (copy from the tile screen)
 * int viewstatus(TITUS_level *level, fox_bool countbonus): View status screen (F4)
 * int flip_screen(fox_bool slow): Flips the screen and a short delay
 * int INIT_SCREENM(TITUS_level *level): Initialize backbuffer
 * int DISPLAY_COUNT(TITUS_level *level): Draw energy
 * int fadeout(): Fade the screen to black
 * int view_password(TITUS_level *level, uint8 level_index): Display the password
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "globals.h"
#include "backbuffer.h"
#include "sprites.h"
#include "draw.h"
#include "settings.h"
#include "common.h"
#include "tituserror.h"
#include "original.h"

SDL_Surface *sprite_from_cache(TITUS_level *level, TITUS_sprite *spr);

int TFR_SCREENM() { //Draw tiles on the backbuffer (copy from the tile screen)
    SDL_Rect src, dest;

	//First of all: make the screen black, at least the lower part of the screen
    dest.x = 0;
    dest.y = screen_height * 16;
    dest.w = screen_width * 16;
    dest.h = resheight - screen_height * 16;
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));

    // Tile screen:  | Output screen:
    //               |
    // D | C         | A | B
    // -   -         | -   -
    // B | A         | C | D
    //
    // The screens are splitted in 4 parts by BITMAP_XM and BITMAP_YM
    // The code below will move the 4 rectangles with tiles to their right place on the output screen
    
    
    //Upper left on screen (A)
    src.x = BITMAP_XM * 16;
    src.y = BITMAP_YM * 16;
    src.w = (screen_width - BITMAP_XM) * 16;
    src.h = (screen_height - BITMAP_YM) * 16;
    dest.x = 0;
    dest.y = 0;
    dest.w = src.w;
    dest.h = src.h;
    SDL_BlitSurface(tilescreen, &src, screen, &dest);

    //Upper right on screen (B)
    src.x = 0;
    src.y = BITMAP_YM * 16;
    src.w = BITMAP_XM * 16;
    src.h = (screen_height - BITMAP_YM) * 16;
    dest.x = (screen_width - BITMAP_XM) * 16;
    dest.y = 0;
    dest.w = src.w;
    dest.h = src.h;
    SDL_BlitSurface(tilescreen, &src, screen, &dest);

    //Lower left on screen (C)
    src.x = BITMAP_XM * 16;
    src.y = 0;
    src.w = (screen_width - BITMAP_XM) * 16;
    src.h = BITMAP_YM * 16;
    dest.x = 0;
    dest.y = (screen_height - BITMAP_YM) * 16;
    dest.w = src.w;
    dest.h = src.h;
    SDL_BlitSurface(tilescreen, &src, screen, &dest);

    //Lower right on screen (D)
    src.x = 0;
    src.y = 0;
    src.w = BITMAP_XM * 16;
    src.h = BITMAP_YM * 16;
    dest.x = (screen_width - BITMAP_XM) * 16;
    dest.y = (screen_height - BITMAP_YM) * 16;
    dest.w = src.w;
    dest.h = src.h;
    SDL_BlitSurface(tilescreen, &src, screen, &dest);
}


//Loop through all sprites, and draw the sprites that should be visible on the screen (NOT by using the visible flag, it uses the coordinates)
//If the flash bit is set, the first 3 planes will be 0, the last plane will be normal (colour & 0x01, odd colors gets white, even colours gets black)


DISPLAY_SPRITES(TITUS_level *level) {
    int16 i;
    char buffer[7]; //xxx ms

    for (i = level->elevatorcount - 1; i >= 0; i--) {
        display_sprite(level, &(level->elevator[i].sprite));
    }

    for (i = level->trashcount - 1; i >= 0; i--) {
        display_sprite(level, &(level->trash[i]));
    }

    for (i = level->enemycount - 1; i >= 0; i--) {
        display_sprite(level, &(level->enemy[i].sprite));
    }

    for (i = level->objectcount - 1; i >= 0; i--) {
        display_sprite(level, &(level->object[i].sprite));
    }

    display_sprite(level, &(level->player.sprite3));
    display_sprite(level, &(level->player.sprite2));
    display_sprite(level, &(level->player.sprite));

    if (GODMODE) {
        SDL_Print_Text("GODMODE", 30 * 8, 0 * 12);
    }
    if (NOCLIP) {
        SDL_Print_Text("NOCLIP", 30 * 8, 1 * 12);
    }

#ifdef DEBUG_VERSION
    if (DISPLAYLOOPTIME) {
        sprintf(buffer, "%3u ms", LOOPTIME);
        SDL_Print_Text(buffer, 30 * 8, 2 * 12); //Loop time in ms

        sprintf(buffer, "FPS %u", FPS_LAST);
        SDL_Print_Text(buffer, 30 * 8, 4 * 12); //Last second's FPS count

        sprintf(buffer, "CL %d", LAST_CLOCK);
        SDL_Print_Text(buffer, 30 * 8, 6 * 12); //Clock

        sprintf(buffer, "CORR %d", LAST_CLOCK_CORR);
        SDL_Print_Text(buffer, 30 * 8, 8 * 12); //Correction to the clock


		for (i = 0; i <= 15; i++) {
			sprintf(buffer, "%d %3u", i, SUBTIME[i]);
			SDL_Print_Text(buffer, 0 * 8, i * 12); //Sub times from main loop in ms
		}

		sprintf(buffer, "%d %3u", i, SUBTIME[i]);
		SDL_Print_Text(buffer, 0 * 8, i * 12);
	}
	
#endif

}

display_sprite(TITUS_level *level, TITUS_sprite *spr) {
    SDL_Surface *image;
    SDL_Rect src, dest;
    if (!spr->enabled) {
        return;
    }
    if (spr->invisible) {
        return;
    }
    spr->visible = fox_false;
    //At this point, the buffer should be the correct size

    if (!spr->flipped) {
        dest.x = spr->x - spr->spritedata->refwidth - (BITMAP_X << 4);
    } else {
        dest.x = spr->x + spr->spritedata->refwidth - spr->spritedata->data->w - (BITMAP_X << 4);
    }
    dest.y = spr->y + spr->spritedata->refheight - spr->spritedata->data->h + 1 - (BITMAP_Y << 4);
    
    if ((dest.x >= screen_width * 16) || //Right for the screen
      (dest.x + spr->spritedata->data->w < 0) || //Left for the screen
      (dest.y + spr->spritedata->data->h < 0) || //Above the screen
      (dest.y >= screen_height * 16)) { //Below the screen
        return;
    }
	
	image = sprite_from_cache(level, spr);
/*	
    if ((spr->flipped != spr->flipped_last) ||
      (spr->flash != spr->flash_last)) {
        SDL_FreeSurface(spr->buffer);
        spr->buffer = copysurface(spr->spritedata->data, spr->flipped, spr->flash);
    }
    
    spr->flipped_last = spr->flipped;
    spr->flash_last = spr->flash;
    
    SDL_Surface *image = spr->buffer;
*/
    src.x = 0;
    src.y = 0;
    src.w = image->w;
    src.h = image->h;

    if (dest.x < 0) {
        src.x = 0 - dest.x;
        src.w -= src.x;
        dest.x = 0;
    }
    if (dest.y < 0) {
        src.y = 0 - dest.y;
        src.h -= src.y;
        dest.y = 0;
    }
    if (dest.x + src.w > screen_width * 16) {
        src.w = screen_width * 16 - dest.x;
    }
    if (dest.y + src.h > screen_height * 16) {
        src.h = screen_height * 16 - dest.y;
    }
    
    SDL_BlitSurface(image, &src, screen, &dest);



    spr->visible = fox_true;
    spr->flash = fox_false;

}

SDL_Surface *sprite_from_cache(TITUS_level *level, TITUS_sprite *spr) {
	TITUS_spritecache *cache = level->spritecache;
	TITUS_spritedata *spritedata = level->spritedata[spr->number];
	TITUS_spritebuffer *spritebuffer;
	uint8 index;
	int16 i;

	if (spr->flipped) {index = 1;} else {index = 0;};

    if (spr->flash) {
		for (i = cache->count - cache->tmpcount; i < cache->count; i++) {
			spritebuffer = cache->spritebuffer[i];
			if (spritebuffer != NULL) {
				if ((spritebuffer->spritedata == spritedata) &&
				  (spritebuffer->index == index + 2)) {
				    return spritebuffer->data; //Already in buffer
				}
			}
		}
		//Not found, load into buffer
		cache->cycle2++;
		if (cache->cycle2 >= cache->count) { //The last 3 buffer surfaces is temporary (reserved for flash)
			cache->cycle2 = cache->count - cache->tmpcount;
		}
		spritebuffer = cache->spritebuffer[cache->cycle2];
		SDL_FreeSurface(spritebuffer->data); //Free old surface
		spritebuffer->data = copysurface(spritedata->data, spr->flipped, spr->flash);
		spritebuffer->spritedata = spritedata;
		spritebuffer->index = index + 2;
        return spritebuffer->data;
    } else {
        if (spritedata->spritebuffer[index] == NULL) {
		    cache->cycle++;
			if (cache->cycle + cache->tmpcount >= cache->count) { //The last 3 buffer surfaces is temporary (reserved for flash)
			    cache->cycle = 0;
		    }
			spritebuffer = cache->spritebuffer[cache->cycle];
			if (spritebuffer->spritedata != NULL) {
				spritebuffer->spritedata->spritebuffer[spritebuffer->index] = NULL; //Remove old link
			}
			SDL_FreeSurface(spritebuffer->data); //Free old surface
			spritebuffer->data = copysurface(spritedata->data, spr->flipped, spr->flash);
			spritebuffer->spritedata = spritedata;
			spritebuffer->index = index;
			spritedata->spritebuffer[index] = spritebuffer;
		}
		return spritedata->spritebuffer[index]->data;
	}
}
			
		
int flip_screen(fox_bool unused) { return 0; }

/*
int NO_FAST_CPU(fox_bool unused) { return 0; }
*/

int NO_FAST_CPU(fox_bool unused) { return 0; }


int viewstatus(TITUS_level *level, fox_bool countbonus){
    int retval, i, j;
    char tmpchars[10];
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

    if (game == 0) { //Titus
        SDL_Print_Text("LEVEL", 13 * 8, 12 * 5);
        SDL_Print_Text("EXTRA BONUS", 10 * 8, 10 * 12);
        SDL_Print_Text("LIVES", 10 * 8, 11 * 12);
    } else if (game == 1) { //Moktar
        SDL_Print_Text("ETAPE", 13 * 8, 12 * 5);
        SDL_Print_Text("EXTRA BONUS", 10 * 8, 10 * 12);
        SDL_Print_Text("VIE", 10 * 8, 11 * 12);
    }


    sprintf(tmpchars, "%d", level->levelnumber + 1);
    SDL_Print_Text(tmpchars, 25 * 8 - strlen(tmpchars) * 8, 12 * 5);

    SDL_Print_Text(leveltitle[level->levelnumber], 0, 12 * 5 + 16);
    sprintf(tmpchars, "%d", level->extrabonus);
    SDL_Print_Text(tmpchars, 28 * 8 - strlen(tmpchars) * 8, 10 * 12);

    sprintf(tmpchars, "%d", level->lives);
    SDL_Print_Text(tmpchars, 28 * 8 - strlen(tmpchars) * 8, 11 * 12);

    SDL_Flip(screen);

    if (countbonus && (level->extrabonus >= 10)) {
        retval = waitforbutton();
        if (retval < 0) {
            return retval;
        }
        while (level->extrabonus >= 10) {
            for (i = 0; i < 10; i++) {
                level->extrabonus--;
                sprintf(tmpchars, "%2d", level->extrabonus);
                SDL_Print_Text(tmpchars, 28 * 8 - strlen(tmpchars) * 8, 10 * 12);
                SDL_Flip(screen);
                for (j = 0; j < 15; j++) {
                    NO_FAST_CPU(fox_false);
                }
            }
            level->lives++;
            sprintf(tmpchars, "%d", level->lives);
            SDL_Print_Text(tmpchars, 28 * 8 - strlen(tmpchars) * 8, 11 * 12);
            SDL_Flip(screen);
            for (j = 0; j < 10; j++) {
                NO_FAST_CPU(fox_false);
            }
        }
    }

    retval = waitforbutton();
    if (retval < 0)
        return retval;

    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_Flip(screen);

    return (0);
}

int INIT_SCREENM(TITUS_level *level) {
    CLOSE_SCREEN();
    BITMAP_X = 0;
    BITMAP_Y = 0;
    BITMAP_XM = 0;
    BITMAP_YM = 0;
    uint8 i, j;
    for (i = 0; i < 20; i++) {
        for (j = 0; j < 12; j++) {
            DISPLAY_CHAR(level, level->tilemap[BITMAP_Y + j][BITMAP_X + i], j, i);
        }
    }
    do {
        scroll(level);
    } while (YSCROLL_CENTER || XSCROLL_CENTER);
    OPEN_SCREEN();
}


int DISPLAY_COUNT(TITUS_level *level) {
    subto0(&(BAR_FLAG));
    if (BAR_FLAG != 0) {
        DISPLAY_ENERGY(level);
    }
}


DISPLAY_ENERGY(TITUS_level *level) {
    uint8 offset = 96;
    uint8 i;
    SDL_Rect dest;
    for (i = 0; i < level->player.hp; i++) { //Draw big bars (4px*16px, spacing 4px)
        dest.x = offset;
        dest.y = 9;
        dest.w = 4;
        dest.h = 16;
        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
        offset += 8;
    }
    for (i = 0; i < MAXIMUM_ENERGY - level->player.hp; i++) { //Draw small bars (4px*4px, spacing 4px)
        dest.x = offset;
        dest.y = 15;
        dest.w = 4;
        dest.h = 4;
        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
        offset += 8;
    }
}

int fadeout() {
    SDL_Surface *image;
    int activedelay = 1;
    SDL_Event event;
    unsigned int fade_time = 1000;
    unsigned int tick_start = 0;
    unsigned int image_alpha = 0;
    SDL_Rect src, dest;

    src.x = 0;
    src.y = 0;
    src.w = screen->w;
    src.h = screen->h;
	
    dest.x = 0;
    dest.y = 0;
    dest.w = screen->w;
    dest.h = screen->h;

    image = SDL_ConvertSurface(screen, screen->format, SDL_SWSURFACE);

    SDL_BlitSurface(screen, &src, image, &dest);
    tick_start = SDL_GetTicks();
    while (image_alpha < 255) //Fade to black
    {
        if (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_FreeSurface(image);
                return (-1);
            }

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    SDL_FreeSurface(image);
                    return (-1);
                }
#ifdef AUDIO_ENABLED
                if (event.key.keysym.sym == KEY_MUSIC) {
					AUDIOMODE++;
					if (AUDIOMODE > 1) {
						AUDIOMODE = 0;
					}
					if (AUDIOMODE == 1) {
						startmusic();
					}
                }
#endif
            }
        }

        image_alpha = (SDL_GetTicks() - tick_start) * 256 / fade_time;

        if (image_alpha > 255)
            image_alpha = 255;
            
        SDL_SetAlpha(image, SDL_SRCALPHA, 255 - image_alpha);
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
        SDL_BlitSurface(image, &src, screen, &dest);
        SDL_Flip(screen);

#ifdef AUDIO_MIKMOD_SINGLETHREAD
        checkmodule();
#endif

#ifdef AUDIO_SDL_MIXER
        checkaudio();
#endif

        titus_sleep();
    }
    SDL_FreeSurface(image);

}

int view_password(TITUS_level *level, uint8 level_index) {
    //Display the password !
    char tmpchars[10];
    int retval;

    CLOSE_SCREEN();
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_Flip(screen);

    if (game == 0) { //Titus
        SDL_Print_Text("LEVEL", 13 * 8, 13 * 8);
    } else if (game == 1) { //Moktar
        SDL_Print_Text("ETAPE", 13 * 8, 13 * 8);
    }
    sprintf(tmpchars, "%d", level_index + 1);
    SDL_Print_Text(tmpchars, 25 * 8 - strlen(tmpchars) * 8, 13 * 8);

    SDL_Print_Text("CODE", 14 * 8, 10 * 8);
    SDL_Print_Text(levelcode[level_index], 20 * 8, 10 * 8);

    SDL_Flip(screen);
    retval = waitforbutton();
    if (retval < 0)
        return retval;

    //SDL_Flip(screen);
    OPEN_SCREEN();
    return (0);
}

int loadpixelformat(SDL_PixelFormat **pixelformat){*pixelformat=SDL_AllocFormat(SDL_PIXELFORMAT_INDEX8);if(!*pixelformat)return -1;SDL_Palette*palette=SDL_AllocPalette(256);if(!palette)return -1;SDL_SetPixelFormatPalette(*pixelformat,palette);SDL_FreePalette(palette);SDL_SetPaletteColors((*pixelformat)->palette,orig_palette_colour,0,16);return 0;}

int loadpixelformat_font(SDL_PixelFormat **pixelformat){*pixelformat=SDL_AllocFormat(SDL_PIXELFORMAT_INDEX8);if(!*pixelformat)return -1;SDL_Palette*palette=SDL_AllocPalette(256);if(!palette)return -1;SDL_SetPixelFormatPalette(*pixelformat,palette);SDL_FreePalette(palette);SDL_SetPaletteColors((*pixelformat)->palette,orig_palette_font_colour,0,16);return 0;}

int freepixelformat(SDL_PixelFormat **pixelformat){SDL_FreeFormat(*pixelformat);*pixelformat=NULL;return 0;}
