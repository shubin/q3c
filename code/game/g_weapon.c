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
// g_weapon.c 
// perform the server side effects of a weapon firing

#include "g_local.h"

static	float	s_quadFactor;
static	vec3_t	forward, right, up;
static	vec3_t	muzzle;

#define NUM_NAILSHOTS 15

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
	vec3_t v, newv;
	float dot;

	VectorSubtract( impact, start, v );
	dot = DotProduct( v, dir );
	VectorMA( v, -2*dot, dir, newv );

	VectorNormalize(newv);
	VectorMA(impact, 8192, newv, endout);
}


/*
======================================================================

GAUNTLET

======================================================================
*/

void Weapon_Gauntlet( gentity_t *ent ) {

}

/*
===============
CheckGauntletAttack
===============
*/
qboolean CheckGauntletAttack( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	CalcMuzzlePoint ( ent, forward, right, up, muzzle );

	VectorMA (muzzle, 32, forward, end);

	trap_Trace (&tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT);
	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return qfalse;
	}

	if ( ent->client->noclip ) {
		return qfalse;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// send blood impact
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
	}

	if ( !traceEnt->takedamage) {
		return qfalse;
	}

	if (ent->client->ps.powerups[PW_QUAD] ) {
		G_AddEvent( ent, EV_POWERUP_QUAD, 0 );
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}
#ifdef MISSIONPACK
	if( ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER ) {
		s_quadFactor *= 2;
	}
#endif

	damage = 50 * s_quadFactor;
	G_Damage( traceEnt, ent, ent, forward, tr.endpos,
		damage, 0, MOD_GAUNTLET );

	return qtrue;
}


/*
======================================================================

MACHINEGUN

======================================================================
*/

#if !defined( UNLAGGED ) //unlagged - attack prediction #3
// moved to q_shared.c

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating 
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
			v[i] = floor(v[i]);
		} else {
			v[i] = ceil(v[i]);
		}
	}
}
#endif

#ifdef MISSIONPACK
#define CHAINGUN_SPREAD		600
#define CHAINGUN_DAMAGE		7
#endif
#if defined( QC ) // WARNING: important to keep it coherent with the same definition in cg_unlagged.c 
#define MACHINEGUN_SPREAD	300
#else
#define MACHINEGUN_SPREAD	200
#endif
#define	MACHINEGUN_DAMAGE	10
#define	MACHINEGUN_TEAM_DAMAGE	7		// wimpier MG in teamplay
#if defined( QC )
#define MACHINEGUN_ZOOM_SPREAD	0
#define MACHINEGUN_ZOOM_DAMAGE	15
#define MACHINEGUN_ZOOM_TEAM_DAMAGE	10
#define LOUSY_MACHINEGUN_SPREAD 300
#define LOUSY_MACHINEGUN_DAMAGE 7
#define LOUSY_MACHINEGUN_TEAM_DAMAGE 5
#endif

void Bullet_Fire (gentity_t *ent, float spread, int damage, int mod ) {
	trace_t		tr;
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t		impactpoint, bouncedir;
#endif
	float		r;
	float		u;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			i, passent;

#if defined( UNLAGGED ) //unlagged - attack prediction #2
	// we have to use something now that the client knows in advance
	int			seed = ent->client->attackTime % 256;
#endif

	damage *= s_quadFactor;

#if defined( UNLAGGED )
//unlagged - attack prediction #2
	// this has to match what's on the client
	r = Q_random(&seed) * M_PI * 2.0f;
	u = sin(r) * Q_crandom(&seed) * spread * 16;
	r = cos(r) * Q_crandom(&seed) * spread * 16;
#else
	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * spread * 16;
	r = cos(r) * crandom() * spread * 16;
#endif
	VectorMA (muzzle, 8192*16, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
		// backward-reconcile the other clients
		G_DoTimeShiftFor( ent );
#endif
		trap_Trace (&tr, muzzle, NULL, NULL, end, passent, MASK_SHOT);
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
		// put them back
		G_UndoTimeShiftFor( ent );
#endif
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		// snap the endpos to integers, but nudged towards the line
		SnapVectorTowards( tr.endpos, muzzle );

		// send bullet impact
		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
			tent->s.eventParm = traceEnt->s.number;
#if defined( UNLAGGED ) //unlagged - attack prediction #2
			// we need the client number to determine whether or not to
			// suppress this event
			tent->s.clientNum = ent->s.clientNum;
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				ent->client->accuracy_hits++;
	#if defined( QC )
				ent->client->wepstat[ent->s.weapon].hits++;
	#endif
			}
		} else {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
			tent->s.eventParm = DirToByte( tr.plane.normal );
#if defined( UNLAGGED ) //unlagged - attack prediction #2
			// we need the client number to determine whether or not to
			// suppress this event
			tent->s.clientNum = ent->s.clientNum;
#endif
		}
		tent->s.otherEntityNum = ent->s.number;

		if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
			else {
#endif
				G_Damage( traceEnt, ent, ent, forward, tr.endpos,
					damage, 0, mod);
#ifdef MISSIONPACK
			}
#endif
		}
		break;
	}
}


/*
======================================================================

BFG

======================================================================
*/

void BFG_Fire ( gentity_t *ent ) {
	gentity_t	*m;

	m = fire_bfg (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

SHOTGUN

======================================================================
*/

#if defined( QC )

typedef struct client_damage_s {
	gentity_t *targ;
	gentity_t *inflictor;
	gentity_t *attacker;
	vec3_t dir;
	vec3_t point;
	int damage;
} client_damage_t;

static client_damage_t s_client_damage[MAX_CLIENTS];

static
void SG_ResetDamage( void ) {
	memset( s_client_damage, 0, sizeof( s_client_damage ) );
}

static
void SG_AddDamage( gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage ) {
	if ( targ->client == NULL ) { // not a player, so process the damage regularily
		G_Damage( targ, inflictor, attacker, dir, point, damage, 0, MOD_SHOTGUN );
	} else {
		client_damage_t *cd = &s_client_damage[targ->client->ps.clientNum];
		cd->targ = targ;
		cd->inflictor = inflictor;
		cd->attacker = attacker;
		VectorCopy( dir, cd->dir );
		VectorCopy( point, cd->point );
		cd->damage += damage;
	}
}

static
void SG_ExecuteClientDamage( void ) {
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		client_damage_t *cd = &s_client_damage[i];
		if ( cd->targ != NULL && cd->inflictor != NULL && cd->attacker != NULL ) {
			G_Damage( cd->targ, cd->inflictor, cd->attacker, cd->dir, cd->point, cd->damage, 0, MOD_SHOTGUN );
		}
	}		
}

#endif

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT	are in bg_public.h, because
// client predicts same spreads
#if defined( QC )
#define	DEFAULT_SHOTGUN_DAMAGE	4
#else
#define	DEFAULT_SHOTGUN_DAMAGE	10
#endif

qboolean ShotgunPellet( vec3_t start, vec3_t end, gentity_t *ent ) {
	trace_t		tr;
	int			damage, i, passent;
	gentity_t	*traceEnt;
#ifdef MISSIONPACK
	vec3_t		impactpoint, bouncedir;
#endif
	vec3_t		tr_start, tr_end;
	qboolean	hitClient = qfalse;

	passent = ent->s.number;
	VectorCopy( start, tr_start );
	VectorCopy( end, tr_end );
	for (i = 0; i < 10; i++) {
		trap_Trace (&tr, tr_start, NULL, NULL, tr_end, passent, MASK_SHOT);
		traceEnt = &g_entities[ tr.entityNum ];

		// send bullet impact
		if (  tr.surfaceFlags & SURF_NOIMPACT ) {
			return qfalse;
		}

		if ( traceEnt->takedamage) {
			damage = DEFAULT_SHOTGUN_DAMAGE * s_quadFactor;
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( tr_start, impactpoint, bouncedir, tr_end );
					VectorCopy( impactpoint, tr_start );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, tr_start );
					passent = traceEnt->s.number;
				}
				continue;
			}
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				hitClient = qtrue;
			}
#if defined( QC )
            SG_AddDamage( traceEnt, ent, ent, forward, tr.endpos, damage );
#else
			G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN);
#endif
			return hitClient;
		}
		return qfalse;
	}
	return qfalse;
}

#if defined( QC )
// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int			i, k;
	float		r, u, radius;
	vec3_t		end;
	vec3_t		forward, right, up;
	qboolean	hitClient = qfalse, hit = qfalse;

#if defined( UNLAGGED ) //unlagged - attack prediction #2
	// use this for testing
	//Com_Printf( "Server seed: %d\n", seed );
#endif

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

	VectorMA( origin, 8192 * 16, forward, end);
	if ( ShotgunPellet( origin, end, ent ) && !hitClient ) {
		hitClient = qtrue;
		ent->client->accuracy_hits++;
		hit = qtrue;
	}

#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// backward-reconcile the other clients
	G_DoTimeShiftFor( ent );
#endif
	radius = SHOTGUN_SPREAD / 3.0f;
	for ( k = 0; k < 3; k++, radius += SHOTGUN_SPREAD / 3.0f ) {
		for ( i = 0 ; i < 9; i++ ) {
			r = cos( i * M_PI / 4.5f ) * radius * 16;
			u = sin( i * M_PI / 4.5f ) * radius * 16;
			VectorMA( origin, 8192 * 16, forward, end);
			VectorMA (end, r, right, end);
			VectorMA (end, u, up, end);
			if( ShotgunPellet( origin, end, ent ) && !hitClient ) {
				hitClient = qtrue;
				ent->client->accuracy_hits++;
				hit = qtrue;
			}
		}
	}
	if ( hit ) {
		ent->client->wepstat[ent->s.weapon].hits++;
	}
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// put them back
	G_UndoTimeShiftFor( ent );
#endif
}

void LousyShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int			i, k;
	float		r, u, radius;
	vec3_t		end;
	vec3_t		forward, right, up;
	qboolean	hitClient = qfalse, hit = qfalse;

#if defined( UNLAGGED ) //unlagged - attack prediction #2
	// use this for testing
	//Com_Printf( "Server seed: %d\n", seed );
#endif

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// backward-reconcile the other clients
	G_DoTimeShiftFor( ent );
#endif

	VectorMA( origin, 8192 * 16, forward, end);
	if ( ShotgunPellet( origin, end, ent ) && !hitClient ) {
		hitClient = qtrue;
		ent->client->accuracy_hits++;
		hit = qtrue;
	}

	radius = SHOTGUN_SPREAD / 3.0f;
	for ( k = 0; k < 2; k++, radius += SHOTGUN_SPREAD / 2.0f ) { // TODO: check the actual QC pattern
		for ( i = 0 ; i < 9; i++ ) {
			r = cos( i * M_PI / 4.5f ) * radius * 16;
			u = sin( i * M_PI / 4.5f ) * radius * 16;
			VectorMA( origin, 8192 * 16, forward, end);
			VectorMA (end, r, right, end);
			VectorMA (end, u, up, end);
			if( ShotgunPellet( origin, end, ent ) && !hitClient ) {
				hitClient = qtrue;
				ent->client->accuracy_hits++;
				hit = qtrue;
			}
		}
	}
	if ( hit ) {
		ent->client->wepstat[ent->s.weapon].hits++;
	}
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// put them back
	G_UndoTimeShiftFor( ent );
#endif
}

#else
// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		forward, right, up;
	qboolean	hitClient = qfalse;

#if defined( UNLAGGED ) //unlagged - attack prediction #2
	// use this for testing
	//Com_Printf( "Server seed: %d\n", seed );
#endif

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// backward-reconcile the other clients
	G_DoTimeShiftFor( ent );
#endif
	// generate the "random" spread pattern
	for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		u = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		VectorMA( origin, 8192 * 16, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);
		if( ShotgunPellet( origin, end, ent ) && !hitClient ) {
			hitClient = qtrue;
			ent->client->accuracy_hits++;
		}
	}
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// put them back
	G_UndoTimeShiftFor( ent );
#endif
}
#endif


void weapon_supershotgun_fire (gentity_t *ent) {
	gentity_t		*tent;

	// send shotgun blast
	tent = G_TempEntity( muzzle, EV_SHOTGUN );
	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
#if defined( QC ) // already predictable, no need to modify for the unlagged
	tent->s.eventParm = 0; // 0 -> super shotgun pattern
#elif defined( UNLAGGED ) //unlagged - attack prediction #2
	// this has to be something the client can predict now
	tent->s.eventParm = ent->client->attackTime % 256; // seed for spread pattern
#else
	tent->s.eventParm = rand() & 255;		// seed for spread pattern
#endif
	tent->s.otherEntityNum = ent->s.number;

#if defined( QC )
    SG_ResetDamage();
#endif
	ShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
#if defined( QC )
    SG_ExecuteClientDamage();
#endif
}

#if defined( QC )
void weapon_lousyshotgun_fire (gentity_t *ent) {
	gentity_t		*tent;

	// send shotgun blast
	tent = G_TempEntity( muzzle, EV_SHOTGUN );
	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
	tent->s.eventParm = 1;		// 1 -> lousy shotgun pattern, no need to modify for the unlagged
	tent->s.otherEntityNum = ent->s.number;

    SG_ResetDamage();
	LousyShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
    SG_ExecuteClientDamage();
}
#endif

#if defined( QC )
void weapon_heavymachinegun_fire (gentity_t* ent) {
	float spread;
	int damage;

	if ( ent->client->pers.cmd.buttons & BUTTON_ZOOM ) {
		spread = MACHINEGUN_ZOOM_SPREAD;
		damage = ( g_gametype.integer != GT_TEAM && g_gametype.integer != GT_TEAM2V2 ) ? MACHINEGUN_ZOOM_DAMAGE : MACHINEGUN_ZOOM_TEAM_DAMAGE;
	} else {
		spread = MACHINEGUN_SPREAD;
		damage = ( g_gametype.integer != GT_TEAM && g_gametype.integer != GT_TEAM2V2 ) ? MACHINEGUN_DAMAGE : MACHINEGUN_TEAM_DAMAGE;
	}
	Bullet_Fire( ent, spread, damage, MOD_MACHINEGUN );
}

void weapon_lousymachinegun_fire (gentity_t* ent) {
	float spread;
	int damage;

	spread = LOUSY_MACHINEGUN_SPREAD;
	damage = ( g_gametype.integer != GT_TEAM && g_gametype.integer != GT_TEAM2V2 ) ? LOUSY_MACHINEGUN_DAMAGE : LOUSY_MACHINEGUN_TEAM_DAMAGE;
	Bullet_Fire( ent, spread, damage, MOD_MACHINEGUN );
}
#endif

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void weapon_grenadelauncher_fire (gentity_t *ent) {
	gentity_t	*m;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	m = fire_grenade (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_rocket (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

#if defined( QC )
/*
======================================================================

TRIBOLT

======================================================================
*/

void Weapon_Tribolt_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_bolt (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}
#endif

/*
======================================================================

PLASMA GUN

======================================================================
*/

void Weapon_Plasmagun_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_plasma (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

#if defined( QC )
void Weapon_LousyPlasmagun_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_lousy_plasma (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}
#endif

/*
======================================================================

RAILGUN

======================================================================
*/


/*
=================
weapon_railgun_fire
=================
*/
#define	MAX_RAIL_HITS	4
void weapon_railgun_fire (gentity_t *ent) {
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t impactpoint, bouncedir;
#endif
	trace_t		trace;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;
	int			i;
	int			hits;
	int			unlinked;
	int			passent;
	gentity_t	*unlinkedEntities[MAX_RAIL_HITS];

#if defined( QC )
	damage = 90 * s_quadFactor;
#else
	damage = 100 * s_quadFactor;
#endif

	VectorMA (muzzle, 8192, forward, end);

#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// backward-reconcile the other clients
	G_DoTimeShiftFor( ent );
#endif
	// trace only against the solids, so the railgun will go through people
	unlinked = 0;
	hits = 0;
	passent = ent->s.number;
	do {
		trap_Trace (&trace, muzzle, NULL, NULL, end, passent, MASK_SHOT );
		if ( trace.entityNum >= ENTITYNUM_MAX_NORMAL ) {
			break;
		}
		traceEnt = &g_entities[ trace.entityNum ];
		if ( traceEnt->takedamage ) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if ( G_InvulnerabilityEffect( traceEnt, forward, trace.endpos, impactpoint, bouncedir ) ) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					// snap the endpos to integers to save net bandwidth, but nudged towards the line
					SnapVectorTowards( trace.endpos, muzzle );
					// send railgun beam effect
					tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );
					// set player number for custom colors on the railtrail
					tent->s.clientNum = ent->s.clientNum;
					VectorCopy( muzzle, tent->s.origin2 );
					// move origin a bit to come closer to the drawn gun muzzle
					VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
					VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );
					tent->s.eventParm = 255;	// don't make the explosion at the end
					//
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
			}
			else {
				if( LogAccuracyHit( traceEnt, ent ) ) {
					hits++;
				}
				G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
			}
#else
				if( LogAccuracyHit( traceEnt, ent ) ) {
					hits++;
				}
				G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
#endif
		}
		if ( trace.contents & CONTENTS_SOLID ) {
			break;		// we hit something solid enough to stop the beam
		}
		// unlink this entity, so the next trace will go past it
		trap_UnlinkEntity( traceEnt );
		unlinkedEntities[unlinked] = traceEnt;
		unlinked++;
	} while ( unlinked < MAX_RAIL_HITS );
#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
	// put them back
	G_UndoTimeShiftFor( ent );
#endif
	// link back in any entities we unlinked
	for ( i = 0 ; i < unlinked ; i++ ) {
		trap_LinkEntity( unlinkedEntities[i] );
	}

	// the final trace endpos will be the terminal point of the rail trail

	// snap the endpos to integers to save net bandwidth, but nudged towards the line
	SnapVectorTowards( trace.endpos, muzzle );

	// send railgun beam effect
	tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );

	// set player number for custom colors on the railtrail
	tent->s.clientNum = ent->s.clientNum;

	VectorCopy( muzzle, tent->s.origin2 );
	// move origin a bit to come closer to the drawn gun muzzle
	VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
	VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );

	// no explosion at end if SURF_NOIMPACT, but still make the trail
	if ( trace.surfaceFlags & SURF_NOIMPACT ) {
		tent->s.eventParm = 255;	// don't make the explosion at the end
	} else {
		tent->s.eventParm = DirToByte( trace.plane.normal );
	}
	tent->s.clientNum = ent->s.clientNum;

	// give the shooter a reward sound if they have made two railgun hits in a row
	if ( hits == 0 ) {
		// complete miss
		ent->client->accurateCount = 0;
	} else {
		// check for "impressive" reward sound
		ent->client->accurateCount += hits;
		if ( ent->client->accurateCount >= 2 ) {
			ent->client->accurateCount -= 2;
			ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
			// add the sprite over the player's head
			ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
			ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
			ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
		}
		ent->client->accuracy_hits++;
#if defined( QC )
		ent->client->wepstat[ent->s.weapon].hits++;
#endif
	}

}


/*
======================================================================

GRAPPLING HOOK

======================================================================
*/

void Weapon_GrapplingHook_Fire (gentity_t *ent)
{
	if (!ent->client->fireHeld && !ent->client->hook)
		fire_grapple (ent, muzzle, forward);

	ent->client->fireHeld = qtrue;
}

void Weapon_HookFree (gentity_t *ent)
{
	ent->parent->client->hook = NULL;
	ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
	G_FreeEntity( ent );
}

void Weapon_HookThink (gentity_t *ent)
{
	if (ent->enemy) {
		vec3_t v, oldorigin;

		VectorCopy(ent->r.currentOrigin, oldorigin);
		v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
		v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
		v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
		SnapVectorTowards( v, oldorigin );	// save net bandwidth

		G_SetOrigin( ent, v );
	}

	VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);
}

/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void Weapon_LightningFire( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t impactpoint, bouncedir;
#endif
	gentity_t	*traceEnt, *tent;
	int			damage, i, passent;

#if defined( QC )
	damage = 6 * s_quadFactor;
#elif defined( UNLAGGED )
	damage = g_lightningDamage.value * s_quadFactor;
#else
	damage = 8 * s_quadFactor;
#endif

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {
		VectorMA( muzzle, LIGHTNING_RANGE, forward, end );

#if defined( UNLAGGED ) //unlagged - backward reconciliation #2
		// backward-reconcile the other clients
		G_DoTimeShiftFor( ent );
#endif
		trap_Trace( &tr, muzzle, NULL, NULL, end, passent, MASK_SHOT );
#if defined( UNLAGGED )//unlagged - backward reconciliation #2
		// put them back
		G_UndoTimeShiftFor( ent );
#endif
#ifdef MISSIONPACK
		// if not the first trace (the lightning bounced of an invulnerability sphere)
		if (i) {
			// add bounced off lightning bolt temp entity
			// the first lightning bolt is a cgame only visual
			//
			tent = G_TempEntity( muzzle, EV_LIGHTNINGBOLT );
			VectorCopy( tr.endpos, end );
			SnapVector( end );
			VectorCopy( end, tent->s.origin2 );
		}
#endif
		if ( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					VectorSubtract( end, impactpoint, forward );
					VectorNormalize(forward);
					// the player can hit him/herself with the bounced lightning
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				ent->client->accuracy_hits++;
#if defined( QC )
				ent->client->wepstat[ent->s.weapon].hits++;
#endif
			}
			G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_LIGHTNING);
		}

		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
			tent->s.otherEntityNum = traceEnt->s.number;
			tent->s.eventParm = DirToByte( tr.plane.normal );
			tent->s.weapon = ent->s.weapon;
		} else if ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}

		break;
	}
}

#ifdef MISSIONPACK
/*
======================================================================

NAILGUN

======================================================================
*/

void Weapon_Nailgun_Fire (gentity_t *ent) {
	gentity_t	*m;
	int			count;

	for( count = 0; count < NUM_NAILSHOTS; count++ ) {
		m = fire_nail (ent, muzzle, forward, right, up );
		m->damage *= s_quadFactor;
		m->splashDamage *= s_quadFactor;
	}

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

PROXIMITY MINE LAUNCHER

======================================================================
*/

void weapon_proxlauncher_fire (gentity_t *ent) {
	gentity_t	*m;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	m = fire_prox (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

#endif

//======================================================================


/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if( !target->takedamage ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if( !target->client ) {
		return qfalse;
	}

	if( !attacker->client ) {
		return qfalse;
	}

	if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, forward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, forward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}



/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent ) {
	if (ent->client->ps.powerups[PW_QUAD] ) {
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}
#ifdef MISSIONPACK
	if( ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER ) {
		s_quadFactor *= 2;
	}
#endif

	// track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
	if( ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET ) {
#ifdef MISSIONPACK
		if( ent->s.weapon == WP_NAILGUN ) {
			ent->client->accuracy_shots += NUM_NAILSHOTS;
		} else {
			ent->client->accuracy_shots++;
		}
#else
		ent->client->accuracy_shots++;
#if defined( QC )
		ent->client->wepstat[ent->s.weapon].shots++;
#endif
#endif
	}

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );

	// fire the specific weapon
	switch( ent->s.weapon ) {
	case WP_GAUNTLET:
		Weapon_Gauntlet( ent );
		break;
	case WP_LIGHTNING:
		Weapon_LightningFire( ent );
		break;
	case WP_SHOTGUN:
		weapon_supershotgun_fire( ent );
		break;
#if defined( QC )
	case WP_LOUSY_SHOTGUN:
		weapon_lousyshotgun_fire( ent );
		break;
#endif
	case WP_MACHINEGUN:
#if defined( QC )
		weapon_heavymachinegun_fire( ent );
#else
		if ( g_gametype.integer != GT_TEAM ) {
			Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE, MOD_MACHINEGUN );
		} else {
			Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_TEAM_DAMAGE, MOD_MACHINEGUN );
		}
#endif
		break;
#if defined( QC )
	case WP_LOUSY_MACHINEGUN:
		weapon_lousymachinegun_fire( ent );
		break;
#endif
	case WP_GRENADE_LAUNCHER:
		weapon_grenadelauncher_fire( ent );
		break;
	case WP_ROCKET_LAUNCHER:
		Weapon_RocketLauncher_Fire( ent );
		break;
	case WP_PLASMAGUN:
		Weapon_Plasmagun_Fire( ent );
		break;
#if defined( QC )
	case WP_LOUSY_PLASMAGUN:
		Weapon_LousyPlasmagun_Fire( ent );
		break;
#endif
	case WP_RAILGUN:
		weapon_railgun_fire( ent );
		break;
	case WP_BFG:
		BFG_Fire( ent );
		break;
	case WP_GRAPPLING_HOOK:
		Weapon_GrapplingHook_Fire( ent );
		break;
#if defined( QC )
	case WP_TRIBOLT:
		Weapon_Tribolt_Fire( ent );
		break;
#endif
#ifdef MISSIONPACK
	case WP_NAILGUN:
		Weapon_Nailgun_Fire( ent );
		break;
	case WP_PROX_LAUNCHER:
		weapon_proxlauncher_fire( ent );
		break;
	case WP_CHAINGUN:
		Bullet_Fire( ent, CHAINGUN_SPREAD, CHAINGUN_DAMAGE, MOD_CHAINGUN );
		break;
#endif
	default:
// FIXME		G_Error( "Bad ent->s.weapon" );
		break;
	}
}


#ifdef MISSIONPACK

/*
===============
KamikazeRadiusDamage
===============
*/
static void KamikazeRadiusDamage( vec3_t origin, gentity_t *attacker, float damage, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (!ent->takedamage) {
			continue;
		}

		// don't hit things we have already hit
		if( ent->kamikazeTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			ent->kamikazeTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeShockWave
===============
*/
static void KamikazeShockWave( vec3_t origin, gentity_t *attacker, float damage, float push, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 )
		radius = 1;

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		// don't hit things we have already hit
		if( ent->kamikazeShockTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			//
			dir[2] = 0;
			VectorNormalize(dir);
			if ( ent->client ) {
				ent->client->ps.velocity[0] = dir[0] * push;
				ent->client->ps.velocity[1] = dir[1] * push;
				ent->client->ps.velocity[2] = 100;
			}
			ent->kamikazeShockTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeDamage
===============
*/
static void KamikazeDamage( gentity_t *self ) {
	int i;
	float t;
	gentity_t *ent;
	vec3_t newangles;

	self->count += 100;

	if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
		// shockwave push back
		t = self->count - KAMI_SHOCKWAVE_STARTTIME;
		KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400,	(int) (float) t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME) );
	}
	//
	if (self->count >= KAMI_EXPLODE_STARTTIME) {
		// do our damage
		t = self->count - KAMI_EXPLODE_STARTTIME;
		KamikazeRadiusDamage( self->s.pos.trBase, self->activator, 400,	(int) (float) t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME) );
	}

	// either cycle or kill self
	if( self->count >= KAMI_SHOCKWAVE_ENDTIME ) {
		G_FreeEntity( self );
		return;
	}
	self->nextthink = level.time + 100;

	// add earth quake effect
	newangles[0] = crandom() * 2;
	newangles[1] = crandom() * 2;
	newangles[2] = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;

		if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
			ent->client->ps.velocity[0] += crandom() * 120;
			ent->client->ps.velocity[1] += crandom() * 120;
			ent->client->ps.velocity[2] = 30 + random() * 25;
		}

		ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
		ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
		ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
	}
	VectorCopy(newangles, self->movedir);
}

/*
===============
G_StartKamikaze
===============
*/
void G_StartKamikaze( gentity_t *ent ) {
	gentity_t	*explosion;
	gentity_t	*te;
	vec3_t		snapped;

	// start up the explosion logic
	explosion = G_Spawn();

	explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
	explosion->eventTime = level.time;

	if ( ent->client ) {
		VectorCopy( ent->s.pos.trBase, snapped );
	}
	else {
		VectorCopy( ent->activator->s.pos.trBase, snapped );
	}
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( explosion, snapped );

	explosion->classname = "kamikaze";
	explosion->s.pos.trType = TR_STATIONARY;

	explosion->kamikazeTime = level.time;

	explosion->think = KamikazeDamage;
	explosion->nextthink = level.time + 100;
	explosion->count = 0;
	VectorClear(explosion->movedir);

	trap_LinkEntity( explosion );

	if (ent->client) {
		//
		explosion->activator = ent;
		//
		ent->s.eFlags &= ~EF_KAMIKAZE;
		// nuke the guy that used it
		G_Damage( ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE );
	}
	else {
		if ( !strcmp(ent->activator->classname, "bodyque") ) {
			explosion->activator = &g_entities[ent->activator->r.ownerNum];
		}
		else {
			explosion->activator = ent->activator;
		}
	}

	// play global sound at all clients
	te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = GTS_KAMIKAZE;
}
#endif
