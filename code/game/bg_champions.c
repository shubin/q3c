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
// bg_champions.c

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_champions.h"

champion_stat_t champion_stats[NUM_CHAMPIONS] = {
    // sarge
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            75, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 9999,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 40 },
    },
    // anarki
    {
        .base_health = 100,
        .base_armor = 75,
        .max_health = 150,
        .max_armor = 150,
        .ability_cooldown = 45,
        .ability_duration = 30, // tenths of a second
        .start_health = {
            125, // GT_FFA
            125, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            50, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 640,
        .mins = { -12, -12, -24 },
        .maxs = { 12, 12, 40 },
    },
    // athena
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // nyx
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // slash
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 750,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // bj
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // dk
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // doomguy
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // eisen
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // galena
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // ranger
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 175,
        .max_armor = 150,
        .ability_cooldown = 45,
        .start_health = {
            125, // GT_FFA
            125, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            50, // GT_FFA
            50, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 40 },
    },
    // strogg
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 310,
        .maxspeed = 750,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // visor
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 320,
        .maxspeed = 2000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // clutch
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 300,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
    },
    // keel
    {
        .base_health = 100,
        .base_armor = 125,
        .max_health = 175,
        .max_armor = 175,
        .ability_cooldown = 45,
        .start_health = {
            125, // GT_FFA
            125, // GT_TOURNAMENT
            125, // GT_SINGLE_PLAYER
            125, // GT_TEAM
            125, // GT_CTF
        },
        .start_armor = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 300,
        .maxspeed = 1000,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 40 },
    },
    // scalebearer
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 300,
        .maxspeed = 1000,
        .mins = { -18, -18, -24 },
        .maxs = { 18, 18, 32 },
    },
    // sorlag
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
        .ability_cooldown = 30,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
            100, // GT_SINGLE_PLAYER
            100, // GT_TEAM
            100, // GT_CTF
        },
        .start_armor = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .spawn_protection = {
            0, // GT_FFA
            0, // GT_TOURNAMENT
            0, // GT_SINGLE_PLAYER
            0, // GT_TEAM
            0, // GT_CTF
        },
        .speed = 300,
        .maxspeed = 750,
        .mins = { -15, -15, -24 },
        .maxs = { 15, 15, 32 },
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
    "major",  // galena
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
    "pm", // galena
    "pm", // ranger
    "pm", // strogg
    "pm", // visor
    "pm", // clutch
    "pm", // keel
    "pm", // scalebearer
    "pm", // sorlag
};

int ParseChampionName( const char* name ) {
    for ( int i = 0; i < NUM_CHAMPIONS; i++ ) {
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
