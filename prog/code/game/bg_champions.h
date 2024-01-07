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
    int ability_cooldown; // seconds
    int ability_duration; // tenths of a second
    int start_health[GT_MAX_GAME_TYPE];
    int start_armor[GT_MAX_GAME_TYPE];
    int spawn_protection[GT_MAX_GAME_TYPE];
    int speed;
    int maxspeed;
    // hitbox
    vec3_t mins, maxs;
} champion_stat_t;

extern champion_stat_t champion_stats[NUM_CHAMPIONS];
extern char *champion_names[NUM_CHAMPIONS];
extern char *champion_models[NUM_CHAMPIONS];
extern char *champion_skins[NUM_CHAMPIONS];
extern byte	blueRGBA[4], redRGBA[4];

int ParseChampionName( const char *name );
int ParseStartingWeapon( const char *name );

#define MAX_TOTEMS			3
#define MAX_GRENADES		5
#define MAX_SPITS			5
#define SPIT_DELAY			200
#define SPIT_DISTANCE       850

#define ACID_SPIT_RADIUS	80
#define ACID_DOT_TIMES		7
#define ACID_DOT_AMOUNT		10
#define ACID_DOT_TICK		1000
#define ACID_LIFETIME		10000

#define TWILIGHT_DEPLOY_TIME    250
#define WALLJUMP_VELOCITY       350
#define WALLJUMP_BOUNCE         350

//#define ANARKI_SPEED_BOOST  1.11875f       // in QC, the normal speed is 320 and 358 when injected
#define ANARKI_SPEED_BOOST  1.05

typedef enum {
	RESPAWN_TYPE_CLASSIC,
	RESPAWN_TYPE_QC,
} respawn_type_t;
