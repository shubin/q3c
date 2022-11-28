/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"

#define	MISSILE_PRESTEP_TIME	50

#if defined( QC )
#define MAX_TOTEMS				3
#endif

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

	if ( ent->s.eFlags & EF_BOUNCE_HALF ) {
		VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
		// check for stop
		if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 ) {
			G_SetOrigin( ent, trace->endpos );
			ent->s.time = level.time / 4;
			return;
		}
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

	ent->freeAfterEvent = qtrue;

	// splash damage
	if ( ent->splashDamage ) {
#if defined( QC )
		if( G_RadiusDamage( ent->r.currentOrigin, ent, ent->splashDamage, ent->splashRadius, ent
			, ent->splashMethodOfDeath ) ) {
#else
		if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent
			, ent->splashMethodOfDeath ) ) {
#endif
			g_entities[ent->r.ownerNum].client->accuracy_hits++;
	#if defined( QC )
			g_entities[ent->r.ownerNum].client->wepstat[ent->s.weapon].hits++;
	#endif
		}
	}

	trap_LinkEntity( ent );
}


#ifdef MISSIONPACK
/*
================
ProximityMine_Explode
================
*/
static void ProximityMine_Explode( gentity_t *mine ) {
	G_ExplodeMissile( mine );
	// if the prox mine has a trigger free it
	if (mine->activator) {
		G_FreeEntity(mine->activator);
		mine->activator = NULL;
	}
}

/*
================
ProximityMine_Die
================
*/
static void ProximityMine_Die( gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	ent->think = ProximityMine_Explode;
	ent->nextthink = level.time + 1;
}

/*
================
ProximityMine_Trigger
================
*/
void ProximityMine_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace ) {
	vec3_t		v;
	gentity_t	*mine;

	if( !other->client ) {
		return;
	}

	// trigger is a cube, do a distance test now to act as if it's a sphere
	VectorSubtract( trigger->s.pos.trBase, other->s.pos.trBase, v );
	if( VectorLength( v ) > trigger->parent->splashRadius ) {
		return;
	}


	if ( g_gametype.integer >= GT_TEAM ) {
		// don't trigger same team mines
		if (trigger->parent->s.generic1 == other->client->sess.sessionTeam) {
			return;
		}
	}

	// ok, now check for ability to damage so we don't get triggered through walls, closed doors, etc...
	if( !CanDamage( other, trigger->s.pos.trBase ) ) {
		return;
	}

	// trigger the mine!
	mine = trigger->parent;
	mine->s.loopSound = 0;
	G_AddEvent( mine, EV_PROXIMITY_MINE_TRIGGER, 0 );
	mine->nextthink = level.time + 500;

	G_FreeEntity( trigger );
}

/*
================
ProximityMine_Activate
================
*/
static void ProximityMine_Activate( gentity_t *ent ) {
	gentity_t	*trigger;
	float		r;

	ent->think = ProximityMine_Explode;
	ent->nextthink = level.time + g_proxMineTimeout.integer;

	ent->takedamage = qtrue;
	ent->health = 1;
	ent->die = ProximityMine_Die;

	ent->s.loopSound = G_SoundIndex( "sound/weapons/proxmine/wstbtick.wav" );

	// build the proximity trigger
	trigger = G_Spawn ();

	trigger->classname = "proxmine_trigger";

	r = ent->splashRadius;
	VectorSet( trigger->r.mins, -r, -r, -r );
	VectorSet( trigger->r.maxs, r, r, r );

	G_SetOrigin( trigger, ent->s.pos.trBase );

	trigger->parent = ent;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->touch = ProximityMine_Trigger;

	trap_LinkEntity (trigger);

	// set pointer to trigger so the entity can be freed when the mine explodes
	ent->activator = trigger;
}

/*
================
ProximityMine_ExplodeOnPlayer
================
*/
static void ProximityMine_ExplodeOnPlayer( gentity_t *mine ) {
	gentity_t	*player;

	player = mine->enemy;
	player->client->ps.eFlags &= ~EF_TICKING;

	if ( player->client->invulnerabilityTime > level.time ) {
		G_Damage( player, mine->parent, mine->parent, vec3_origin, mine->s.origin, 1000, DAMAGE_NO_KNOCKBACK, MOD_JUICED );
		player->client->invulnerabilityTime = 0;
		G_TempEntity( player->client->ps.origin, EV_JUICED );
	}
	else {
		G_SetOrigin( mine, player->s.pos.trBase );
		// make sure the explosion gets to the client
		mine->r.svFlags &= ~SVF_NOCLIENT;
		mine->splashMethodOfDeath = MOD_PROXIMITY_MINE;
		G_ExplodeMissile( mine );
	}
}

/*
================
ProximityMine_Player
================
*/
static void ProximityMine_Player( gentity_t *mine, gentity_t *player ) {
	if( mine->s.eFlags & EF_NODRAW ) {
		return;
	}

	G_AddEvent( mine, EV_PROXIMITY_MINE_STICK, 0 );

	if( player->s.eFlags & EF_TICKING ) {
		player->activator->splashDamage += mine->splashDamage;
		player->activator->splashRadius *= 1.50;
		mine->think = G_FreeEntity;
		mine->nextthink = level.time;
		return;
	}

	player->client->ps.eFlags |= EF_TICKING;
	player->activator = mine;

	mine->s.eFlags |= EF_NODRAW;
	mine->r.svFlags |= SVF_NOCLIENT;
	mine->s.pos.trType = TR_LINEAR;
	VectorClear( mine->s.pos.trDelta );

	mine->enemy = player;
	mine->think = ProximityMine_ExplodeOnPlayer;
	if ( player->client->invulnerabilityTime > level.time ) {
		mine->nextthink = level.time + 2 * 1000;
	}
	else {
		mine->nextthink = level.time + 10 * 1000;
	}
}
#endif

#if defined( QC )

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
static void AnnihilateTotem( gentity_t *ent ) {
	G_FreeEntity( ent );
}
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

static int GetTotemArray( gclient_t *client, gentity_t *totems[MAX_TOTEMS+1] ) {
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
}

static int BuildTotemList( gclient_t *client, gentity_t *totems[MAX_TOTEMS+1] ) {
	int i;

	if ( totems[0] == NULL ) {
		client->ps.ab_num = 0;
		return;
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
	gentity_t *totems[MAX_TOTEMS+1];
	gentity_t *newtotems[MAX_TOTEMS+1];
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

void G_SpawnTotem( gentity_t *ent, trace_t *trace ) {
	gentity_t *newtotem;

	if ( ent == NULL || ent->parent == NULL || ent->parent->client == NULL ) {
		G_FreeEntity( ent );
		return;
	}

	ent->parent->client->ps.ab_flags = 0;
	ent->parent->client->ps.ab_time = 0;

	newtotem = G_Spawn();
	newtotem->classname = "totem";
	newtotem->s.eType = ET_TOTEM;
	newtotem->takedamage = qtrue;
	newtotem->health = 25;
	newtotem->parent = ent->parent;
	newtotem->s.generic1 = ent->parent->client->ps.persistant[PERS_TEAM];
	newtotem->r.contents = CONTENTS_CORPSE;
	newtotem->die = totem_die;
	VectorSet( newtotem->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
	VectorSet( newtotem->r.maxs,  ITEM_RADIUS,  ITEM_RADIUS, 2 * ITEM_RADIUS );

	G_SetOrigin( newtotem, trace->endpos );	
	newtotem->s.angles[YAW] = ent->s.generic1; // player view YAW at the moment of throwing the totem ball

	trap_LinkEntity( newtotem );

	// the totem egg is not needed anymore
	G_FreeEntity( ent );

	if ( InsertTotem( newtotem->parent->client, newtotem ) == 3 ) {
		G_Printf( "NEST IS COMPLETE, OVERHEAL\n" );
	}
}

#endif

/*
================
G_MissileImpact
================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t *other;
	qboolean		hitClient = qfalse;
#ifdef MISSIONPACK
	vec3_t			forward, impactpoint, bouncedir;
	int				eFlags;
#endif
	other = &g_entities[trace->entityNum];

#if defined( QC )
	if ( !other->takedamage && !strcmp( ent->classname, "totem egg" ) ) {
		if ( trace->plane.normal[2] > 0.5f ) {
			G_Printf( "totem landed\n" );
			VectorCopy( ent->s.pos.trDelta, ent->s.angles2 ); // cgame infers missile orientation from trDelta, so keep it in the angles2
			G_SetOrigin( ent, trace->endpos );
			ent->s.pos.trDelta[0] = ent->s.pos.trDelta[1] = ent->s.pos.trDelta[2] = 0.1f;
			G_AddEvent( ent, EV_BOLT_HIT, 0 );
			G_SpawnTotem( ent, trace );
			return;
		}
	}
#endif

	// check for bounce
	if ( !other->takedamage &&
		( ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF ) ) ) {
		G_BounceMissile( ent, trace );
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
		return;
	}

#if defined( QC )
	if ( !other->takedamage && !strcmp( ent->classname, "bolt") ) {
		// bolt sticks into walls/ground
		VectorCopy( ent->s.pos.trDelta, ent->s.angles2 ); // cgame infers missile orientation from trDelta, so keep it in the angles2
		G_SetOrigin( ent, trace->endpos );
		ent->s.pos.trDelta[0] = ent->s.pos.trDelta[1] = ent->s.pos.trDelta[2] = 0.1f;
		G_AddEvent( ent, EV_BOLT_HIT, 0 );
		return;
	}
#endif

#ifdef MISSIONPACK
	if ( other->takedamage ) {
		if ( ent->s.weapon != WP_PROX_LAUNCHER ) {
			if ( other->client && other->client->invulnerabilityTime > level.time ) {
				//
				VectorCopy( ent->s.pos.trDelta, forward );
				VectorNormalize( forward );
				if (G_InvulnerabilityEffect( other, forward, ent->s.pos.trBase, impactpoint, bouncedir )) {
					VectorCopy( bouncedir, trace->plane.normal );
					eFlags = ent->s.eFlags & EF_BOUNCE_HALF;
					ent->s.eFlags &= ~EF_BOUNCE_HALF;
					G_BounceMissile( ent, trace );
					ent->s.eFlags |= eFlags;
				}
				ent->target_ent = other;
				return;
			}
		}
	}
#endif
	// impact damage
	if (other->takedamage) {
		// FIXME: wrong damage direction?
		if ( ent->damage ) {
			vec3_t	velocity;

			if( LogAccuracyHit( other, &g_entities[ent->r.ownerNum] ) ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
	#if defined( QC )
				g_entities[ent->r.ownerNum].client->wepstat[ent->s.weapon].hits++;
	#endif
				hitClient = qtrue;
			}
			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
			if ( VectorLength( velocity ) == 0 ) {
				velocity[2] = 1;	// stepped on a grenade
			}
			G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
				ent->s.origin, ent->damage, 
				0, ent->methodOfDeath);
		}
	}

#ifdef MISSIONPACK
	if( ent->s.weapon == WP_PROX_LAUNCHER ) {
		if( ent->s.pos.trType != TR_GRAVITY ) {
			return;
		}

		// if it's a player, stick it on to them (flag them and remove this entity)
		if( other->s.eType == ET_PLAYER && other->health > 0 ) {
			ProximityMine_Player( ent, other );
			return;
		}

		SnapVectorTowards( trace->endpos, ent->s.pos.trBase );
		G_SetOrigin( ent, trace->endpos );
		ent->s.pos.trType = TR_STATIONARY;
		VectorClear( ent->s.pos.trDelta );

		G_AddEvent( ent, EV_PROXIMITY_MINE_STICK, trace->surfaceFlags );

		ent->think = ProximityMine_Activate;
		ent->nextthink = level.time + 2000;

		vectoangles( trace->plane.normal, ent->s.angles );
		ent->s.angles[0] += 90;

		// link the prox mine to the other entity
		ent->enemy = other;
		ent->die = ProximityMine_Die;
		VectorCopy(trace->plane.normal, ent->movedir);
		VectorSet(ent->r.mins, -4, -4, -4);
		VectorSet(ent->r.maxs, 4, 4, 4);
		trap_LinkEntity(ent);

		return;
	}
#endif

	if (!strcmp(ent->classname, "hook")) {
		gentity_t *nent;
		vec3_t v;

		nent = G_Spawn();
		if ( other->takedamage && other->client ) {

			G_AddEvent( nent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
			nent->s.otherEntityNum = other->s.number;

			ent->enemy = other;

			v[0] = other->r.currentOrigin[0] + (other->r.mins[0] + other->r.maxs[0]) * 0.5;
			v[1] = other->r.currentOrigin[1] + (other->r.mins[1] + other->r.maxs[1]) * 0.5;
			v[2] = other->r.currentOrigin[2] + (other->r.mins[2] + other->r.maxs[2]) * 0.5;

			SnapVectorTowards( v, ent->s.pos.trBase );	// save net bandwidth
		} else {
			VectorCopy(trace->endpos, v);
			G_AddEvent( nent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
			ent->enemy = NULL;
		}

		SnapVectorTowards( v, ent->s.pos.trBase );	// save net bandwidth

		nent->freeAfterEvent = qtrue;
		// change over to a normal entity right at the point of impact
		nent->s.eType = ET_GENERAL;
		ent->s.eType = ET_GRAPPLE;

		G_SetOrigin( ent, v );
		G_SetOrigin( nent, v );

		ent->think = Weapon_HookThink;
		ent->nextthink = level.time + FRAMETIME;

		ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;
		VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);

		trap_LinkEntity( ent );
		trap_LinkEntity( nent );

		return;
	}

	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if ( other->takedamage && other->client ) {
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
		ent->s.otherEntityNum = other->s.number;
	} else if( trace->surfaceFlags & SURF_METALSTEPS ) {
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
	} else {
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	}

	ent->freeAfterEvent = qtrue;

	// change over to a normal entity right at the point of impact
	ent->s.eType = ET_GENERAL;

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	// splash damage (doesn't apply to person directly hit)
	if ( ent->splashDamage ) {
#if defined( QC )
		if( G_RadiusDamage( trace->endpos, ent, ent->splashDamage, ent->splashRadius, 
#else
		if( G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, 
#endif
			other, ent->splashMethodOfDeath ) ) {
			if( !hitClient ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
	#if defined( QC )
				g_entities[ent->r.ownerNum].client->wepstat[ent->s.weapon].hits++;
	#endif
			}
		}
	}

#if defined( QC )
	if ( !strcmp( ent->classname, "orb" ) ) {
		// unlink the orb from its owner
		ent->parent->client->ps.ab_num = 0;
		ent->parent->client->ps.ab_flags = 0;
		ent->parent->client->ps.ab_time = 0;
	}
#endif

	trap_LinkEntity( ent );
}

#if defined( QC )
/*
================
G_OrbImpact
================
*/
void G_OrbPassThroughImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t		*other, *owner;
	qboolean		hitClient = qfalse;
	float			quadfactor;
	other = &g_entities[trace->entityNum];
	owner = &g_entities[ent->r.ownerNum];

	if ( owner && owner->client && owner->client->ps.powerups[PW_QUAD] ) {
		quadfactor = g_quadfactor.value;
	}
	else {
		quadfactor = 1.0f;
	}

	if ( !other->client || !other->takedamage ) {
		return;
	}

	// impact damage
	if ( other->takedamage ) {
		if ( ent->damage && ( level.time - other->client->orbPassThroughTime > 1000 ) ) { // don't let the same orb damage an enemy twice in less than 1000ms
			vec3_t	velocity;
			velocity[0] = 0.0f;
			velocity[1] = 0.0f;
			velocity[2] = 0.0f;

			G_Damage( other, ent, &g_entities[ent->r.ownerNum], velocity, ent->s.origin, 75 * quadfactor, 0, ent->methodOfDeath );
			other->client->orbEntityNum = ent - g_entities;
			other->client->orbPassThroughTime = level.time;
		}
	}

}
#endif

/*
================
G_RunMissile
================
*/
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			passent;
#if defined( QC )
	trace_t		tr_orb;
	vec3_t		stoppos;
#endif

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if ( ent->target_ent ) {
		passent = ent->target_ent->s.number;
	}
#ifdef MISSIONPACK
	// prox mines that left the owner bbox will attach to anything, even the owner
	else if (ent->s.weapon == WP_PROX_LAUNCHER && ent->count) {
		passent = ENTITYNUM_NONE;
	}
#endif
	else {
		// ignore interactions with the missile owner
		passent = ent->r.ownerNum;
	}
	// trace a line from the previous position to the current position
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );

	if ( tr.startsolid || tr.allsolid ) {
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
		tr.fraction = 0;
	}
	else {
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	trap_LinkEntity( ent );

	if ( tr.fraction != 1 ) {
#if defined( QC )
		if ( !strcmp( ent->classname, "orb" ) ) {
			// run the trace again with different mask for the orb, as the orb should pass through an enemy but inflict some damage
			trap_Trace( &tr_orb, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, MASK_SOLID );
			if ( tr_orb.startsolid || tr_orb.allsolid ) {
				// make sure the tr.entityNum is set to the entity we're stuck in
				trap_Trace( &tr_orb, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
				tr_orb.fraction = 0;
			}
			else {
				VectorCopy( tr_orb.endpos, ent->r.currentOrigin );
			}
			if ( tr_orb.entityNum != tr.entityNum ) {
				// orb is passing through something
				G_OrbPassThroughImpact( ent, &tr );
				//return;
			}
			if ( tr_orb.fraction != 1 ) {
				vec3_t dir;
				// stop the orb
				VectorCopy( ent->r.currentOrigin, stoppos );
				VectorSubtract( stoppos, ent->s.pos.trBase, dir);
				VectorNormalize(dir);
				VectorMA( stoppos, -10, dir, stoppos );
				SnapVectorTowards( stoppos, ent->s.pos.trBase );
				G_SetOrigin( ent, stoppos );
				ent->speed = 0;
				ent->nextthink = level.time + 500; // explode in 0.5s
				return;
			}
		} else {
#endif
		// never explode or bounce on sky
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			// If grapple, reset owner
			if (ent->parent && ent->parent->client && ent->parent->client->hook == ent) {
				ent->parent->client->hook = NULL;
			}
#if defined( QC )
			// if totem ball, reset the ability timer
			if ( ent->parent && ent->parent->client && !strcmp( ent->classname, "totem egg" ) ) {
				ent->parent->client->ps.ab_flags = 0;
				ent->parent->client->ps.ab_time = 0;
			}
#endif
			G_FreeEntity( ent );
			return;
		}
		G_MissileImpact( ent, &tr );
		if ( ent->s.eType != ET_MISSILE ) {
			return;		// exploded
		}
	}
#if defined( QC )
	}
#endif
#ifdef MISSIONPACK
	// if the prox mine wasn't yet outside the player body
	if (ent->s.weapon == WP_PROX_LAUNCHER && !ent->count) {
		// check if the prox mine is outside the owner bbox
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ENTITYNUM_NONE, ent->clipmask );
		if (!tr.startsolid || tr.entityNum != ent->r.ownerNum) {
			ent->count = 1;
		}
	}
#endif
	// check think function after bouncing
	G_RunThink( ent );
}


//=============================================================================

/*
=================
fire_plasma

=================
*/
gentity_t *fire_plasma (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "plasma";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_PLASMAGUN;
#if defined( QC )
//	bolt->s.constantLight = 0x1F7F7F3F;
#endif
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 20;
	bolt->splashDamage = 15;
	bolt->splashRadius = 20;
	bolt->methodOfDeath = MOD_PLASMA;
	bolt->splashMethodOfDeath = MOD_PLASMA_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 2000, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}	

#if defined( QC )
/*
=================
fire_lousy_plasma

=================
*/
gentity_t *fire_lousy_plasma (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "plasma";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_LOUSY_PLASMAGUN;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 12;
	bolt->splashDamage = 8;
	bolt->splashRadius = 15;
	bolt->methodOfDeath = MOD_PLASMA;
	bolt->splashMethodOfDeath = MOD_PLASMA_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 2000, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}	

/*
=================
fire_bolt
=================
*/
gentity_t *fire_bolt (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "bolt";
	bolt->nextthink = level.time + 600;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_TRIBOLT;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 35;
	bolt->splashDamage = 35;
	bolt->splashRadius = 80;
	bolt->methodOfDeath = MOD_TRIBOLT;
	bolt->splashMethodOfDeath = MOD_TRIBOLT_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trGravity = 400; // flat trajectory for bolts
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 1600, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}
#endif

//=============================================================================


/*
=================
fire_grenade
=================
*/
gentity_t *fire_grenade (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "grenade";
	bolt->nextthink = level.time + 2500;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_GRENADE_LAUNCHER;
	bolt->s.eFlags = EF_BOUNCE_HALF;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 100;
	bolt->splashDamage = 100;
	bolt->splashRadius = 150;
	bolt->methodOfDeath = MOD_GRENADE;
	bolt->splashMethodOfDeath = MOD_GRENADE_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
#if defined( QC )
	bolt->s.pos.trGravity = DEFAULT_GRAVITY;
#endif
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 700, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

//=============================================================================


/*
=================
fire_bfg
=================
*/
gentity_t *fire_bfg (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "bfg";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_BFG;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 100;
	bolt->splashDamage = 100;
	bolt->splashRadius = 120;
	bolt->methodOfDeath = MOD_BFG;
	bolt->splashMethodOfDeath = MOD_BFG_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 2000, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

//=============================================================================


/*
=================
fire_rocket
=================
*/
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "rocket";
	bolt->nextthink = level.time + 15000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_ROCKET_LAUNCHER;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 100;
	bolt->splashDamage = 100;
	bolt->splashRadius = 120;
	bolt->methodOfDeath = MOD_ROCKET;
	bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
#if defined( QC )
	VectorScale( dir, 1000, bolt->s.pos.trDelta );
#else
	VectorScale( dir, 900, bolt->s.pos.trDelta );
#endif
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

/*
=================
fire_grapple
=================
*/
gentity_t *fire_grapple (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*hook;
#if defined( UNLAGGED ) //unlagged - grapple
	int hooktime;
#endif
	VectorNormalize (dir);

	hook = G_Spawn();
	hook->classname = "hook";
	hook->nextthink = level.time + 10000;
	hook->think = Weapon_HookFree;
	hook->s.eType = ET_MISSILE;
	hook->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	hook->s.weapon = WP_GRAPPLING_HOOK;
	hook->r.ownerNum = self->s.number;
	hook->methodOfDeath = MOD_GRAPPLE;
	hook->clipmask = MASK_SHOT;
	hook->parent = self;
	hook->target_ent = NULL;
#if defined( UNLAGGED ) //unlagged - grapple
	// we might want this later
	hook->s.otherEntityNum = self->s.number;

	// setting the projectile base time back makes the hook's first
	// step larger

	if ( self->client ) {
		hooktime = self->client->pers.cmd.serverTime + 50;
	}
	else {
		hooktime = level.time - MISSILE_PRESTEP_TIME;
	}

	hook->s.pos.trTime = hooktime;
#endif
	hook->s.pos.trType = TR_LINEAR;
#if !defined( UNLAGGED ) //unlagged - grapple
	hook->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
#endif
	hook->s.otherEntityNum = self->s.number; // use to match beam in client
	VectorCopy( start, hook->s.pos.trBase );
	VectorScale( dir, 800, hook->s.pos.trDelta );
	SnapVector( hook->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, hook->r.currentOrigin);

	self->client->hook = hook;

	return hook;
}


#ifdef MISSIONPACK
/*
=================
fire_nail
=================
*/
#define NAILGUN_SPREAD	500

gentity_t *fire_nail( gentity_t *self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up ) {
	gentity_t	*bolt;
	vec3_t		dir;
	vec3_t		end;
	float		r, u, scale;

	bolt = G_Spawn();
	bolt->classname = "nail";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_NAILGUN;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED )//unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 20;
	bolt->methodOfDeath = MOD_NAIL;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time;
	VectorCopy( start, bolt->s.pos.trBase );

	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * NAILGUN_SPREAD * 16;
	r = cos(r) * crandom() * NAILGUN_SPREAD * 16;
	VectorMA( start, 8192 * 16, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);
	VectorSubtract( end, start, dir );
	VectorNormalize( dir );

	scale = 555 + random() * 1800;
	VectorScale( dir, scale, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );

	VectorCopy( start, bolt->r.currentOrigin );

	return bolt;
}	


/*
=================
fire_prox
=================
*/
gentity_t *fire_prox( gentity_t *self, vec3_t start, vec3_t dir ) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "prox mine";
	bolt->nextthink = level.time + 3000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_PROX_LAUNCHER;
	bolt->s.eFlags = 0;
	bolt->r.ownerNum = self->s.number;
#if defined( UNLAGGED )//unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->parent = self;
	bolt->damage = 0;
	bolt->splashDamage = 100;
	bolt->splashRadius = 150;
	bolt->methodOfDeath = MOD_PROXIMITY_MINE;
	bolt->splashMethodOfDeath = MOD_PROXIMITY_MINE;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;
	// count is used to check if the prox mine left the player bbox
	// if count == 1 then the prox mine left the player bbox and can attack to it
	bolt->count = 0;

	//FIXME: we prolly wanna abuse another field
	bolt->s.generic1 = self->client->sess.sessionTeam;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 700, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}
#endif
