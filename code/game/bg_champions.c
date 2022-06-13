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

champion_stat_t champion_stats[NUM_CHAMPIONS] = {
    // sarge
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // anarki
    {
        .base_health = 100,
        .base_armor = 75,
        .max_health = 175,
        .max_armor = 175,
        .start_health = {
            100, // GT_FFA
            100, // GT_TOURNAMENT
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
        }
    },
    // athena
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // nyx
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // slash
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // bj
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // dk
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // doomguy
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // eisen
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // galena
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // ranger
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // strogg
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // visor
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // clutch
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // keel
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // scalebearer
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
    },
    // sorlag
    {
        .base_health = 100,
        .base_armor = 100,
        .max_health = 200,
        .max_armor = 200,
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
        }
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
    "sarge",
    "anarki",
    "hunter", //"athena",
    "mynx", //"nyx",
    "slash",
    "tim", //"bj",
    "klesk", //"dk",
    "doom",
    "razor", //"eisen",
    "major", //"galena",
    "ranger",
    "xian", //"strogg",
    "visor",
    "tankjr",//"clutch",
    "keel",
    "brandon", //"scalebearer",
    "sorlag",
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
