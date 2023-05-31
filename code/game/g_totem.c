/*
===========================================================================
Copyright (C) 2021 Sergei Shubin

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
// g_totem.c - Galena's totem implementation
//

#include "g_local.h"
#include "bg_local.h"
#include "bg_champions.h"

#define MAX_TOTEMS				3

typedef struct {
	int entnum;
	int chargetime[MAX_CLIENTS];
} totem_t;

typedef struct {
	totem_t	totems[MAX_TOTEMS];
	int num_totems;
} totem_bush_t;

static totem_bush_t	s_playertotems[MAX_CLIENTS];

static void AnnihilateTotem( gentity_t *ent ) {
	G_FreeEntity( ent );
}

void G_ResetPlayerTotems( int clientNum ) {
	int i;
	totem_bush_t *bush;

	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		bush = &s_playertotems[clientNum];
		for ( i = 0; i < bush->num_totems; i++ ) {
			AnnihilateTotem( &g_entities[bush->totems[i].entnum] );
		}
		memset( bush, 0, sizeof( totem_bush_t ) );
	}
}

void totem_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod );

int InsertTotem( gclient_t *player, gentity_t *totem );

void G_SpawnTotem( gentity_t *ent, trace_t *trace ) {
	gentity_t *newtotem;
	int team;

	if ( ent->parent == NULL || ent->parent->client == NULL ) {
		G_FreeEntity( ent );
		return;
	}

	ent->parent->client->ps.ab_flags = 0;
	ent->parent->client->ps.ab_time = 0;

	team = ent->parent->client->ps.persistant[PERS_TEAM];

	newtotem = G_Spawn();
	newtotem->classname = "totem";
	newtotem->s.eType = ET_TOTEM;
	newtotem->takedamage = qtrue;
	newtotem->health = 25;
	newtotem->parent = ent->parent;
	newtotem->s.generic1 = ( team == TEAM_BLUE || team == TEAM_RED ) ? team : ent->parent->client->ps.clientNum;
	newtotem->r.contents = CONTENTS_CORPSE;
	newtotem->s.eFlags = EF_NOFF;
	newtotem->die = totem_die;
	newtotem->spawnflags = ent->parent->client->sess.sessionTeam;
	VectorSet( newtotem->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
	VectorSet( newtotem->r.maxs, ITEM_RADIUS, ITEM_RADIUS, 3 * ITEM_RADIUS );

	G_SetOrigin( newtotem, trace->endpos );
	newtotem->s.angles[YAW] = ent->s.generic1; // player view YAW at the moment of throwing the totem ball

	trap_LinkEntity( newtotem );

	// the totem egg is not needed anymore
	G_FreeEntity( ent );

	if ( InsertTotem( newtotem->parent->client, newtotem ) == MAX_TOTEMS ) {
		G_Printf( "BUSH IS COMPLETE, OVERHEAL\n" );
	}
}

/*
static void RemoveExcessiveTotems( gclient_t *client ) {
	int i;
	int num = 0;
	int next;

	for ( i = client->ps.ab_num; i != 0; i = next ) {
		next = g_entities[i].s.otherEntityNum;
		if ( num == 1 && next != 0 ) {
			G_FreeEntity( &g_entities[next] );
			g_entities[i].s.otherEntityNum = 0;
			return;
		}
		num++;
	}
}
*/
/*
static int CountTotems( gclient_t *client ) {
	int num_totems = 0;
	int num;
	for ( num = client->ps.ab_num; num; num = g_entities[num].s.otherEntityNum ) {
		if ( num != 0 ) {
			num_totems++;
		}
	}
	return num_totems;
}

// removes totem from the linked list of totems
static void RemoveTotem( gentity_t *ent ) {
	gclient_t *player;
	int num, prev;

	if ( ent == NULL || ent->parent == NULL ) {
		AnnihilateTotem( ent );
		return;
	}

	player = ent->parent->client;
	if ( ent->s.number == player->ps.ab_num ) {
		player->ps.ab_num = 0;
		AnnihilateTotem( ent );
		return;
	}

	prev = player->ps.ab_num;
	while ( prev ) {
		num = g_entities[prev].s.otherEntityNum;
		if ( num == ent->s.number ) {
			g_entities[prev].s.otherEntityNum = g_entities[num].s.otherEntityNum;
			AnnihilateTotem( &g_entities[num] );
			break;
		}
		prev = num;
	}
	//for ( num = player->ps.ab_num; num; num = g_entities[num].s.otherEntityNum ) {
	//	next = g_entities[num].s.otherEntityNum;
	//	if ( next == ent->s.number ) {
	//		g_entities[num].s.otherEntityNum = g_entities[next].s.otherEntityNum;
	//		AnnihilateTotem( &g_entities[next] );
	//		break;
	//	}
	//}
}

*/

static int GetTotemArray( gclient_t *client, gentity_t *totems[MAX_TOTEMS + 1] ) {
	int num, p;
	if ( client->ps.ab_num == 0 ) {
		totems[0] = NULL;
		return 0;
	}
	p = 0;
	for ( num = client->ps.ab_num; num; num = g_entities[num].s.otherEntityNum ) {
		if ( num != 0 ) {
			totems[p] = &g_entities[num];
			p++;
		}
	}
	totems[p] = NULL;
	return p;
}

static int PrintTotems( const char *prompt, gclient_t *client ) {
	int i, num;
	gentity_t *totems[MAX_TOTEMS + 1];
	num = GetTotemArray( client, totems );
	G_Printf( "%s\n", prompt );
	for ( i = 0; i < num; i++ ) {
		G_Printf( "   %d: number = %d  other = %d\n", i, totems[i]->s.number, totems[i]->s.otherEntityNum );
	}
	return 0;
}

static int BuildTotemList( gclient_t *client, gentity_t *totems[MAX_TOTEMS + 1] ) {
	int i;

	if ( totems[0] == NULL ) {
		client->ps.ab_num = 0;
		return 0;
	}

	for ( i = 0; i < MAX_TOTEMS; i++ ) {
		if ( totems[i + 1] == NULL ) {
			totems[i]->s.otherEntityNum = 0;
			break;
		} else {
			totems[i]->s.otherEntityNum = totems[i + 1]->s.number;
		}
	}
	client->ps.ab_num = totems[0]->s.number;
	return i;
}

void RemoveTotem( gentity_t *ent ) {
	gclient_t *player;
	gentity_t *totems[MAX_TOTEMS + 1];
	gentity_t *newtotems[MAX_TOTEMS + 1];
	int num_totems, i, p;

	if ( ent == NULL || ent->parent == NULL ) {
		AnnihilateTotem( ent );
		return;
	}

	player = ent->parent->client;

	PrintTotems( "Remove totems before", player );

	num_totems = GetTotemArray( player, totems );
	p = 0;
	for ( i = 0; i < num_totems; i++ ) {
		if ( totems[i] != ent ) {
			newtotems[p] = totems[i];
			p++;
		}
	}
	newtotems[p] = NULL;

	BuildTotemList( player, newtotems );

	PrintTotems( "Remove totems after", player );


	AnnihilateTotem( ent );
}

void RemoveExcessiveTotems( gclient_t *player ) {
	gentity_t *totems[MAX_TOTEMS + 1];
	gentity_t *newtotems[MAX_TOTEMS + 1];
	int num_totems, i, p;

	num_totems = GetTotemArray( player, totems );
	if ( num_totems < MAX_TOTEMS ) {
		return;
	}
	p = 0;
	for ( i = 1; i < MAX_TOTEMS; i++ ) {
		newtotems[p] = totems[i];
		p++;
	}
	newtotems[p] = NULL;

	BuildTotemList( player, newtotems );
}

int CountTotems( gclient_t *player ) {
	gentity_t *totems[MAX_TOTEMS + 1];

	return GetTotemArray( player, totems );
}

int InsertTotem( gclient_t *player, gentity_t *totem ) {
	gentity_t *totems[MAX_TOTEMS + 1];
	gentity_t *newtotems[MAX_TOTEMS + 1];
	int num_totems, i, p;

	PrintTotems( "Insert totem before", player );

	num_totems = GetTotemArray( player, totems );
	if ( num_totems == MAX_TOTEMS ) {
		AnnihilateTotem( totems[MAX_TOTEMS - 1] );
		num_totems--;
	}

	p = 0;
	newtotems[p] = totem;
	p++;
	for ( i = 0; i < num_totems; i++ ) {
		newtotems[p] = totems[i];
		p++;
	}
	newtotems[p] = NULL;
	p = BuildTotemList( player, newtotems );
	PrintTotems( "Insert totem after", player );
	return p;
}

void totem_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	RemoveTotem( self );
}
