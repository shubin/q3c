/*
===========================================================================
Copyright (C) 2022 Sergei Shubin

This file is part of Quake III Champions source code.

Quake III Champions source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Champions source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Champions source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// bg_champions.h

typedef enum {
    CHAMP_SARGE, // regular Q3 guy, no champion

    // light champions
    CHAMP_ANARKI,
    CHAMP_ATHENA,
    CHAMP_NYX,
    CHAMP_SLASH,
    
    // medium champions
    CHAMP_BJ,
    CHAMP_DK,
    CHAMP_DOOMGUY,
    CHAMP_EISEN,
    CHAMP_GALENA,
    CHAMP_RANGER,
    CHAMP_STROGG,
    CHAMP_VISOR,
    
    // heavy champions
    CHAMP_CLUTCH,
    CHAMP_KEEL,
    CHAMP_SCALEBEARER,
    CHAMP_SORLAG,

    NUM_CHAMPIONS,
} champion_t;

typedef struct {
    int base_health;
    int base_armor;
    int max_health;
    int max_armor;
    int start_health[GT_MAX_GAME_TYPE];
    int start_armor[GT_MAX_GAME_TYPE];
    int spawn_protection[GT_MAX_GAME_TYPE];
} champion_stat_t;

extern champion_stat_t champion_stats[NUM_CHAMPIONS];
extern char *champion_names[NUM_CHAMPIONS];
extern char *champion_models[NUM_CHAMPIONS];

int ParseChampionName( const char *name );
int ParseStartingWeapon( const char *name );
