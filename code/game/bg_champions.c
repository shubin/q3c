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
        9999, // maxspeed
        { -15, -15, -24 }, // mins
        {  15,  15,  40 }, // maxs
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
        320, // speed
        640, // maxspeed
        { -12, -12, -24 }, // mins
        {  12,  12,  40 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
    },
    // nyx
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        { 15, 15, 32 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        { 15, 15, 40 }, // maxs
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
        { -15, -15, -24 }, // mins
        { 15, 15, 40 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        2000, // maxspeed
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
        { -15, -15, -24 }, // mins
        { 15, 15, 32 }, // maxs
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
        { -15, -15, -24 }, // mins
        {  15,  15,  40 }, // maxs
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
        { -18, -18, -24 }, // mins
        {  18,  18,  32 }, // maxs
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
        750, // maxspeed
        { -15, -15, -24 }, // mins
        {  15,  15,  32 }, // maxs
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
