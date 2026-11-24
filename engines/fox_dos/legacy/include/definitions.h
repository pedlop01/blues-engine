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

/* definitions.h
 * Definitions
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdint.h>
#include <unistd.h> //fox_bool

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef uint32_t uint32;
typedef signed char int8;
typedef signed short int int16;
typedef int32_t int32;

/* Native flags can contain masks (e.g. enemy.trigger == 0x2000), not just
 * zero/one. Keep the original 32-bit storage without a C++ enum range. */
typedef int32_t fox_bool;
enum {fox_false, fox_true};

#endif
