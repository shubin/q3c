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

// MAX_TOTEMS is defined in bg_champions.h
#define MAX_TOTEM_USERS			8  // only first 8 players can use totems

#define TOTEM_HEAL_AMOUNT		50
#define TOTEM_DAMAGE			50
#define TOTEM_THINK_INTERVAL	100
#define TOTEM_HEAL_RADIUS		120
#define TOTEM_HURT_RADIUS		75
#define TOTEM_EGG_RADIUS		6
#define TOTEM_THROW_SPEED		580
#if defined( DEBUG )
#define TOTEM_COOLDOWN			4000
#else // DEBUG
#define TOTEM_COOLDOWN			40000
#endif // DEBUG

typedef struct {
	int entnum;
	int chargetime[MAX_TOTEM_USERS];
} totem_t;

typedef struct {
	totem_t	totems[MAX_TOTEMS];
	int num_totems;
} totem_bush_t;

static totem_bush_t	s_playertotems[MAX_TOTEM_USERS];

static void UpdateTotemCount( int clientNum ) {
#if defined( DEBUG )
	assert( clientNum >= 0 );
	assert( clientNum < MAX_CLIENTS );
#endif // DEBUG
	g_clients[clientNum].ps.ab_num = s_playertotems[clientNum].num_totems;
}

static void DestroyTotem( gentity_t *ent ) {
	gentity_t *decay;

	decay = G_TempEntity( ent->r.currentOrigin, EV_TOTEM_DECAY );
	decay->s.eFlags = ent->s.eFlags;
	decay->r.svFlags |= SVF_BROADCAST;
	decay->s.affiliation = ent->s.affiliation;
	VectorCopy( ent->s.angles, decay->s.angles );

	if ( ent->activator ) {
		G_FreeEntity( ent->activator );
		ent->activator = NULL;
	}

	UpdateTotemCount( ent->parent->s.clientNum );
	G_FreeEntity( ent );
}

void G_ResetPlayerTotems( int clientNum ) {
	int i;
	totem_bush_t *bush;

	if ( clientNum >= 0 && clientNum < MAX_TOTEM_USERS ) {
		bush = &s_playertotems[clientNum];
		for ( i = 0; i < bush->num_totems; i++ ) {
			DestroyTotem( &g_entities[bush->totems[i].entnum] );
		}
		memset( bush, 0, sizeof( totem_bush_t ) );
	}
	UpdateTotemCount( clientNum );
}

static void ChargeTotem( totem_t *totem ) {
	int i;
	gentity_t *ent;

	ent = &g_entities[totem->entnum];
	ent->s.totemcharge = 0;
	for ( i = 0; i < MAX_TOTEM_USERS; i++ ) {
		totem->chargetime[i] = level.time;
		ent->s.totemcharge |= ( 1 << i ); // fresh totem is charged for everyone
	}
}

static void RegisterTotem( totem_t *totem, gentity_t *ent ) {
	totem->entnum = ent->s.number;
	ChargeTotem( totem );
}

static totem_bush_t *FindBush( gentity_t *ent ) {
#if defined( DEBUG )
	assert( ent != NULL );
	assert( ent->parent != NULL );
	assert( ent->parent->client != NULL );
	assert( ent->parent->client->ps.clientNum >= 0 );
	assert( ent->parent->client->ps.clientNum < MAX_TOTEM_USERS );
#endif
	return &s_playertotems[ent->parent->client->ps.clientNum];
}

static totem_t *FindTotem( gentity_t *ent ) {
#if defined( DEBUG )
	assert( ent != NULL );
	assert( ent->parent != NULL );
	assert( ent->parent->client != NULL );
	assert( ent->parent->client->ps.clientNum >= 0 );
	assert( ent->parent->client->ps.clientNum < MAX_TOTEM_USERS );
#endif
	totem_bush_t *bush;
	int i;
	
	bush = FindBush( ent );
	for ( i = 0; i < MAX_TOTEMS; i++ ) {
		if ( bush->totems[i].entnum == ent->s.number ) {
			return &bush->totems[i];
		}
	}
	return NULL;
}

static void AddTotem( gclient_t *player, gentity_t *totem ) {
	int i, playerNum;
	totem_bush_t *bush;
	totem_t totems[MAX_TOTEMS];

	playerNum = player->ps.clientNum;
	bush = &s_playertotems[playerNum];
	if ( bush->num_totems == MAX_TOTEMS ) {
		DestroyTotem( &g_entities[bush->totems[0].entnum] );
		for ( i = 1; i < MAX_TOTEMS; i++ ) {
			totems[i - 1] = bush->totems[i];
		}
		RegisterTotem( &totems[MAX_TOTEMS - 1], totem );
	} else {
		for ( i = 0; i < bush->num_totems; i++ ) {
			totems[i] = bush->totems[i];
		}
		RegisterTotem( &totems[i], totem );
		bush->num_totems++;
	}
	memcpy( bush->totems, totems, sizeof( totems ) );
	if ( bush->num_totems == MAX_TOTEMS ) {
		// Turn on all totems for all users
		for ( i = 0; i < bush->num_totems; i++ ) {
			ChargeTotem( &bush->totems[i] );
		}
	}
	UpdateTotemCount( player->ps.clientNum );
}

void RemoveTotem( gentity_t *ent ) {
	gclient_t *player;
	totem_bush_t *bush;
	totem_t totems[MAX_TOTEMS], *t;
	int i, num_totems;

	if ( ent == NULL || ent->parent == NULL ) {
		DestroyTotem( ent );
		return;
	}

	player = ent->parent->client;
	bush = &s_playertotems[player->ps.clientNum];

	num_totems = 0; //give hourglass;
	memset( &totems, 0, sizeof( totems ) );
	for ( i = 0; i < bush->num_totems; i++ ) {
		t = &bush->totems[i];
		if ( t->entnum != ent->s.number ) {
			memcpy( &totems[num_totems], t, sizeof( totem_t ) );
			num_totems++;
		}
	};
	bush->num_totems = num_totems;
	memcpy( bush->totems, totems, sizeof( totems ) );

	DestroyTotem( ent );
}

static void Totem_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	RemoveTotem( self );
}

static void Totem_Think( gentity_t *ent ) {
	totem_t *totem;
	int i;

	totem = FindTotem( ent );
	ent->s.totemcharge = 0;
	for ( i = 0; i < MAX_TOTEM_USERS; i++ ) {
		if ( totem->chargetime[i] < level.time ) {
			ent->s.totemcharge|= 1 << i;
		}
	}

	ent->nextthink = level.time + TOTEM_THINK_INTERVAL;
}

static void TotemEgg_Think( gentity_t *ent ) {
	if ( ent->parent->client ) {
		// just in case don't let the totem logic be stuck
		ent->parent->client->ps.ab_flags = 0;
		ent->parent->client->ps.ab_time = 0;
	}
	G_FreeEntity( ent );
}

static void Totem_Heal( gentity_t *trigger, gentity_t *other, trace_t *trace ) {
	vec3_t		v;
	int r, clientNum;
	int max, health;
	totem_bush_t *bush;
	totem_t *totem;

	clientNum = other->client->ps.clientNum;

	r = trigger->r.maxs[0];

	// trigger is a cube, do a distance test now to act as if it's a sphere
	VectorSubtract( trigger->s.pos.trBase, other->s.pos.trBase, v );
	if ( VectorLength( v ) > TOTEM_HEAL_RADIUS ) {
		return;
	}

	bush = FindBush( trigger->parent );
	totem = FindTotem( trigger->parent );

	if ( totem->chargetime[clientNum] > level.time ) {
		// not charged yet
		return;
	}

	if ( bush->num_totems == MAX_TOTEMS ) {
		max = other->client->ps.stats[STAT_MAX_HEALTH];
	} else {
		max = other->client->ps.baseHealth;
	}

	if ( other->health < max ) {
		health = other->health + TOTEM_HEAL_AMOUNT;
		if ( health > max ) {
			health = max;
		}
		other->health = health;
		other->client->ps.stats[STAT_HEALTH] = health;
		totem->chargetime[clientNum] = level.time + TOTEM_COOLDOWN;
		G_RemoveDOT( other, DOT_ALL );
	} else if ( ( other->client->ps.dotAcidNum > 0 ) || ( other->client->ps.dotFireNum > 0 ) ) {
		totem->chargetime[clientNum] = level.time + TOTEM_COOLDOWN;
		G_RemoveDOT( other, DOT_ALL );
	}
}

static void Totem_Hurt( gentity_t *trigger, gentity_t *other, trace_t *trace ) {
	vec3_t		v;
	gentity_t	*attacker;
	int			damage;
	VectorSubtract( trigger->s.pos.trBase, other->s.pos.trBase, v );
	if ( VectorLength( v ) > TOTEM_HURT_RADIUS ) {
		return;
	}
	if ( !CanDamage( other, trigger->s.pos.trBase ) ) {
		return;
	}
	damage = TOTEM_DAMAGE;
	attacker = trigger->parent->parent;
	if ( attacker->client->ps.powerups[PW_QUAD] ) {
		damage *= g_quadfactor.value;
	}
	G_Damage( other, trigger->parent, attacker, NULL, NULL, damage, DAMAGE_RADIUS, MOD_TOTEM_SPLASH );
	RemoveTotem( trigger->parent );
	// boom
	G_Sound( other, CHAN_ITEM, G_SoundIndex( "sound/abilities/totem_boom.wav" ) );
}

static void Totem_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) {
		return;
	}

	if ( G_IsEntityFriendly( other->client->ps.clientNum, trigger->s.number ) ) {
		Totem_Heal( trigger, other, trace );
	} else {
		Totem_Hurt( trigger, other, trace );
	}
}

static gentity_t* CreateTotemEntity( gentity_t *eggent, const vec3_t pos ) {
	gentity_t *newtotem;
	gclient_t *client;

	client = eggent->parent->client;

	newtotem = G_Spawn();
	newtotem->classname = "totem";
	newtotem->s.eType = ET_TOTEM;
	newtotem->takedamage = qtrue;
	newtotem->health = 25;
	newtotem->parent = eggent->parent;
	newtotem->s.time2 = level.time;

	// s.affiliation is set to the team of the owner for team game modes and to the owner
	// client index for FFA modes.
	// It is needed to perform checking for friendliness, see IsEntityFriendly() in cg_predict.c
	// and G_IsEntityFriendly() in g_misc.c
	newtotem->s.affiliation = G_ClientAffiliation( client );
	//newtotem->s.affiliation = 9;
	// s.totemcharge is a bit set, for each client number the corresponding bit shows whether the totem is
	// charged and ready to heal.
	newtotem->s.totemcharge = 0; // bit set indicating if the totem is charged for specific client	
	newtotem->r.contents = CONTENTS_CORPSE;
	// No Friendly Fire flag, entities with this flag should be ignored by friendly players and their weapons.
	// Basically it's needed for the G_IsEntityFriendly to ensure that friendship check should be performed.
	// As long as the entity affiliation is encoded into s.affiliation as above, the G_IsEntityFriendly
	// function should work well for a variety of entities, not` only for totems.
	newtotem->s.eFlags = EF_NOFF;
	newtotem->die = Totem_Die;
	newtotem->think = Totem_Think;
	newtotem->nextthink = level.time + TOTEM_THINK_INTERVAL;
	newtotem->spawnflags = client->sess.sessionTeam;
	VectorSet( newtotem->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
	VectorSet( newtotem->r.maxs, ITEM_RADIUS, ITEM_RADIUS, 3 * ITEM_RADIUS );

#if 1
	newtotem->s.loopSound = G_SoundIndex( "sound/world/x_bobber.wav" );
#else 
	newtotem->s.loopSound = G_SoundIndex( "sound/abilities/totem_ambient.wav" );
#endif
	newtotem->s.loopSoundDist = 200;

	G_SetOrigin( newtotem, pos );
	newtotem->s.angles[YAW] = eggent->s.generic1; // player view YAW at the moment of throwing the totem ball

	trap_LinkEntity( newtotem );
	return newtotem;
}

gentity_t *CreateTotemTrigger( gentity_t *totem ) {
	gentity_t *trigger;
	gclient_t *client;
	int r;

	// build the totem trigger
	trigger = G_Spawn();

	trigger->classname = "totem trigger";

	r = TOTEM_HEAL_RADIUS;
	VectorSet( trigger->r.mins, -r, -r, -r );
	VectorSet( trigger->r.maxs, r, r, r );

	G_SetOrigin( trigger, totem->s.pos.trBase );

	client = totem->parent->client;

	trigger->s.eFlags = EF_NOFF;
	trigger->s.affiliation = totem->s.affiliation; // same affiliation as the totem
	trigger->parent = totem;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->touch = Totem_Trigger;

	trap_LinkEntity( trigger );

	// set pointer to trigger so the entity can be freed when totem is destroyed
	totem->activator = trigger;

	return trigger;
}


qboolean G_CanPutTotemHere( const vec3_t pos, const vec3_t range );

qboolean G_BounceTotemEgg( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	vec3_t	pos;
	float	dot;
	int		hitTime;

	if ( trace->plane.normal[2] > 0.5f ) {
		VectorCopy( ent->s.pos.trDelta, ent->s.angles2 ); // cgame infers missile orientation from trDelta, so keep it in the angles2
		G_SetOrigin( ent, trace->endpos );
		ent->s.pos.trDelta[0] = ent->s.pos.trDelta[1] = ent->s.pos.trDelta[2] = 0.1f;
		ent->freeAfterEvent = qtrue;
		ent->s.eType = ET_GENERAL;
		G_AddEvent( ent, EV_BOLT_HIT, 0 );

		VectorCopy( trace->endpos, pos );
		pos[2] -= TOTEM_EGG_RADIUS;
		G_SpawnTotem( ent, pos );
		return qfalse;
	}

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

	VectorScale( ent->s.pos.trDelta, 0.08, ent->s.pos.trDelta );

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	return qtrue;
}

void G_SpawnTotem( gentity_t *ent, const vec3_t pos ) {
	gentity_t *newtotem, *trigger, *te;
	gclient_t *client;
	vec3_t	plantpos, range;

	if ( ent->parent == NULL || ent->parent->client == NULL ) {
		return;
	}

	// We keep totem readyness indicator in the s.totemcharge  field so we can track up to 32 users only.
	// This should be changed in case if somebody wants to support more than 32 players in-game simultaneously.
	// I assume that we can only have high number of clients if there are a lot of spectators,
	// and for spectators in the free flight mode we can hardcode the totem appearance.
	// QC TODO: ensure that we allocate spectators at the indices above 32.
	if ( ent->parent->client->ps.clientNum >= MAX_TOTEM_USERS ) {
		return;
	}

	client = ent->parent->client;
	client->ps.ab_flags = 0;
	client->ps.ab_time = 0;

	VectorSet( range, 5, 5, 20 );
	VectorCopy( pos, plantpos );
	plantpos[2] += range[2];
	if ( !G_CanPutTotemHere( plantpos, range ) ) {
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
	te->s.eventParm = G_SoundIndex( "sound/abilities/totem_deploy.wav" );
	te->r.svFlags |= SVF_BROADCAST;	

	newtotem = CreateTotemEntity( ent, pos );
	trigger = CreateTotemTrigger( newtotem );
	AddTotem( client, newtotem );
	// 
}

void G_ThrowTotem( gentity_t *ent, vec3_t muzzle, vec3_t forward ) {
	playerState_t *ps;
	gentity_t *egg;
	int quadFactor;
	trace_t tr;

	ps = &ent->client->ps;

	ps->ab_flags &= ~ABF_READY;
	ps->ab_time = 0;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	egg = fire_grenade( ent, muzzle, forward ); // QC TODO: need to get rid of this considering a lot of params are being overriden below

	egg->think = TotemEgg_Think;

	egg->r.mins[0] = egg->r.mins[1] = egg->r.mins[2] = -TOTEM_EGG_RADIUS;
	egg->r.maxs[0] = egg->r.maxs[1] = egg->r.maxs[2] = TOTEM_EGG_RADIUS;

	// check if we throw it right up to a wall

	trap_Trace( &tr, ent->s.pos.trBase, egg->r.mins, egg->r.maxs, muzzle, ent->s.number, MASK_SHOT );

	if ( tr.startsolid || tr.fraction < 1.0f ) {
		// the egg is stuck right at the start
		// let the egg just fall down
		VectorCopy( tr.endpos, egg->s.pos.trBase );
		VectorCopy( tr.endpos, egg->r.currentOrigin );
		VectorSet( egg->s.pos.trDelta, 0, 0, 0 );
		egg->s.pos.trTime = level.time;
	}

	egg->classname = "totem egg";
	egg->s.weapon = WP_TOTEM_EGG;
	egg->methodOfDeath = MOD_TOTEM;
	egg->splashMethodOfDeath = MOD_TOTEM_SPLASH;
	egg->s.eFlags = EF_NOFF;
	egg->s.affiliation = G_ClientAffiliation( ent->client );
	// NERF this grenade a bit
	egg->damage = 75;
	egg->splashDamage = 0;
	egg->splashRadius = 0;
	egg->s.generic1 = (int)ps->viewangles[YAW]; // pass player view angle in order to fix totem orientation later
	//
	VectorScale( forward, TOTEM_THROW_SPEED, egg->s.pos.trDelta ); // slower than the actual grenade
	SnapVector( egg->s.pos.trDelta );			// save net bandwidth

	quadFactor = ps->powerups[PW_QUAD] ? g_quadfactor.value : 1;
	egg->damage *= quadFactor;
	egg->splashDamage *= quadFactor;
}

// QC TODO: optimize this function, it looks somehow clumsy
qboolean G_CanPutTotemHere( const vec3_t pos, const vec3_t range ) {
	vec3_t		mins, maxs;
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	qboolean	result;

	VectorSubtract( pos, range, mins );
	VectorAdd( pos, range, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	result = qtrue;
	for ( i = 0; i < num; i++ ) {
		hit = &g_entities[touch[i]];
		if ( !trap_EntityContact( mins, maxs, hit ) ) {
			continue;
		}
		if ( hit->areaflags & AREA_ALLOWTOTEMS ) {
			return qtrue; // any area with ALLOWTOTEMS flag overrides all other checks
		}
		if ( hit->areaflags & AREA_NOTOTEMS ) {
			result = qfalse;
		}
		if ( hit->r.contents & ( CONTENTS_NODROP | CONTENTS_NOTOTEM ) ) {
			result = qfalse;
		}
	}
	return result;
}
