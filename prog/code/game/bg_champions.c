/*
===========================================================================
Copyright (C) 2022 Sergei Shubin

This file is part of Quake III Champions source code.

Quake III Champions source code is free software; you can redistribute it
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
// bg_champions.c

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_champions.h"

#define LIGHT_CHAMPION_BASE_HEALTH 100
#define LIGHT_CHAMPION_BASE_ARMOUR 75

#define MEDIUM_CHAMPION_BASE_HEALTH 100
#define MEDIUM_CHAMPION_BASE_ARMOUR 100

#define HEAVY_CHAMPION_BASE_HEALTH 100
#define HEAVY_CHAMPION_BASE_ARMOUR 125

                                     // { GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER, G_TEAM2V2, GT_TEAM, GT_CTF }
#define  LIGHT_CHAMPION_STARTING_HEALTH {    125,           125,              125,       125,     125,    125 }
#define  LIGHT_CHAMPION_STARTING_ARMOUR {     25,             0,                0,        25,      25,     25 }
#define MEDIUM_CHAMPION_STARTING_HEALTH {    125,           125,              125,       125,     125,    125 }
#define MEDIUM_CHAMPION_STARTING_ARMOUR {     50,            50,               50,        50,      50,     50 }
#define  HEAVY_CHAMPION_STARTING_HEALTH {    125,           125,              125,       125,     125,    125 }
#define  HEAVY_CHAMPION_STARTING_ARMOUR {     75,            75,               75,        75,      75,     75 }

#define  LIGHT_CHAMPION_RADIUS      13
#define MEDIUM_CHAMPION_RADIUS      15
#define  HEAVY_CHAMPION_RADIUS      17
#define HITBOX_BOTTOM               -24
#define HITBOX_TOP                  45

                                        //   mins                                                                 maxs
#define  LIGHT_CHAMPION_DIMENSIONS      { -  LIGHT_CHAMPION_RADIUS, -  LIGHT_CHAMPION_RADIUS, HITBOX_BOTTOM }, {  LIGHT_CHAMPION_RADIUS,  LIGHT_CHAMPION_RADIUS, HITBOX_TOP }
#define MEDIUM_CHAMPION_DIMENSIONS      { - MEDIUM_CHAMPION_RADIUS, - MEDIUM_CHAMPION_RADIUS, HITBOX_BOTTOM }, { MEDIUM_CHAMPION_RADIUS, MEDIUM_CHAMPION_RADIUS, HITBOX_TOP }
#define  HEAVY_CHAMPION_DIMENSIONS      { -  HEAVY_CHAMPION_RADIUS, -  HEAVY_CHAMPION_RADIUS, HITBOX_BOTTOM }, {  HEAVY_CHAMPION_RADIUS,  HEAVY_CHAMPION_RADIUS, HITBOX_TOP }

#define MAX_HEALTH_INCREMENT 75
#define MAX_ARMOUR_INCREMENT 75

champion_stat_t champion_stats[NUM_CHAMPIONS] = {
    // sarge
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // G_TEAM2V2
            0, // GT_CTF
        },
        320, // speed
        0, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // anarki
    {
        LIGHT_CHAMPION_BASE_HEALTH,
        LIGHT_CHAMPION_BASE_ARMOUR,
        LIGHT_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        LIGHT_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        45, // ability_cooldown
        30, // tenths of a second // ability_duration
        LIGHT_CHAMPION_STARTING_HEALTH,
        LIGHT_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        //320, // speed
        315,
        640, // maxspeed
        LIGHT_CHAMPION_DIMENSIONS,
    },
    // athena
    {
        LIGHT_CHAMPION_BASE_HEALTH,
        LIGHT_CHAMPION_BASE_ARMOUR,
        LIGHT_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        LIGHT_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        LIGHT_CHAMPION_STARTING_HEALTH,
        LIGHT_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         320, // speed
        1000, // maxspeed
        LIGHT_CHAMPION_DIMENSIONS,
    },
    // nyx
    {
        LIGHT_CHAMPION_BASE_HEALTH,
        LIGHT_CHAMPION_BASE_ARMOUR,
        LIGHT_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        LIGHT_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        30, // ability_duration
        LIGHT_CHAMPION_STARTING_HEALTH,
        LIGHT_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         320, // speed
        1000, // maxspeed
        LIGHT_CHAMPION_DIMENSIONS,
    },
    // slash
    {
        LIGHT_CHAMPION_BASE_HEALTH,
        LIGHT_CHAMPION_BASE_ARMOUR,
        LIGHT_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        LIGHT_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        LIGHT_CHAMPION_STARTING_HEALTH,
        LIGHT_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        320, // speed
        750, // maxspeed
        LIGHT_CHAMPION_DIMENSIONS,
    },
    // bj
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // dk
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // doomguy
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // eisen
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // galena
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        45, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // ranger
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        45, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        310, // speed
        1000, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // strogg
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        310, // speed
        750, // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // visor
    {
        MEDIUM_CHAMPION_BASE_HEALTH,
        MEDIUM_CHAMPION_BASE_ARMOUR,
        MEDIUM_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        MEDIUM_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        45, // ability_cooldown
        50, // ability_duration
        MEDIUM_CHAMPION_STARTING_HEALTH,
        MEDIUM_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        320, // speed
        0,   // maxspeed
        MEDIUM_CHAMPION_DIMENSIONS,
    },
    // clutch
    {
        100, // base_health
        100, // base_armor
        200, // max_health
        200, // max_armor
        30, // ability_cooldown
        -1, // ability_duration
        { // start_health
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_TEAM2V2
            100, // GT_CTF
        },
        { // start_armor
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        300, // speed
        1000, // maxspeed
        HEAVY_CHAMPION_DIMENSIONS,
    },
    // keel
    {
        HEAVY_CHAMPION_BASE_HEALTH,
        HEAVY_CHAMPION_BASE_ARMOUR,
        HEAVY_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        HEAVY_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        45, // ability_cooldown
        -1, // ability_duration
        HEAVY_CHAMPION_STARTING_HEALTH,
        HEAVY_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         300, // speed
        1000, // maxspeed
        HEAVY_CHAMPION_DIMENSIONS,
    },
    // scalebearer
    {
        HEAVY_CHAMPION_BASE_HEALTH,
        HEAVY_CHAMPION_BASE_ARMOUR,
        HEAVY_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        HEAVY_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        HEAVY_CHAMPION_STARTING_HEALTH,
        HEAVY_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
         300, // speed
        1000, // maxspeed
        HEAVY_CHAMPION_DIMENSIONS,
    },
    // sorlag
    {
        HEAVY_CHAMPION_BASE_HEALTH,
        HEAVY_CHAMPION_BASE_ARMOUR,
        HEAVY_CHAMPION_BASE_HEALTH + MAX_HEALTH_INCREMENT,
        HEAVY_CHAMPION_BASE_ARMOUR + MAX_ARMOUR_INCREMENT,
        30, // ability_cooldown
        -1, // ability_duration
        HEAVY_CHAMPION_STARTING_HEALTH,
        HEAVY_CHAMPION_STARTING_ARMOUR,
        { // spawn_protection
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_TEAM2V2
            0, // GT_CTF
        },
        300, // speed
        600, // maxspeed
        HEAVY_CHAMPION_DIMENSIONS,
    },
};

char *champion_names[NUM_CHAMPIONS] = {
    "sarge",
    "anarki",
    "athena",
    "nyx",
    "slash",
    "bj",
    "dk",
    "doom",
    "eisen",
    "galena",
    "ranger",
    "strogg",
    "visor",
    "clutch",
    "keel",
    "scalebearer",
    "sorlag",
};

char* champion_models[NUM_CHAMPIONS] = {
    "sarge",  // sarge
    "anarki", // anarki
    "hunter", // athena
    "mynx",   // nyx
    "slash",  // slash
    "bitterman",  // bj
    "klesk",  // dk
    "doom",   // doom
    "razor",  // eisen
    "janet",  // galena
    "ranger", // ranger
    "grunt",  // strogg
    "visor",  // visor
    "tankjr", // clutch
    "keel",   // keel
    "biker",  // scalebearer
    "sorlag", // sorlag
};

char* champion_skins[NUM_CHAMPIONS] = {
    "pm", // sarge
    "pm", // anarki
    "pm", // athena
    "pm", // nyx
    "pm", // slash
    "pm", // bj
    "pm", // dk
    "pm", // doom
    "pm", // eisen
    "bright", // galena
    "pm", // ranger
    "pm", // strogg
    "pm", // visor
    "pm", // clutch
    "pm", // keel
    "pm", // scalebearer
    "pm", // sorlag
};

byte	blueRGBA[4] = { 0, 150, 255, 255 };
byte	redRGBA[4] = { 255, 40, 40, 255 };

int ParseChampionName( const char* name ) {
    int i;
    for ( i = 0; i < NUM_CHAMPIONS; i++ ) {
        if ( !Q_stricmp( name, champion_names[i] ) ) {
            return i;
        }
    }
    return CHAMP_SARGE;
}

int ParseStartingWeapon( const char *name ) {
    if ( !Q_stricmp( name, "sg") || !Q_stricmp( name, "shotgun" ) ) {
        return WP_LOUSY_SHOTGUN;
    }
    if ( !Q_stricmp( name, "pg") || !Q_stricmp( name, "plasmagun" ) || !Q_stricmp( name, "plasma" ) ) {
        return WP_LOUSY_PLASMAGUN;
    }
    return WP_LOUSY_MACHINEGUN;
}
