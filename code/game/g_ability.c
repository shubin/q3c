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
// g_ability.c - champion ability implementation
//

#include "g_local.h"
#include "bg_local.h"

/* g_weapon.c */
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint );

/*
================
G_GenerateDireOrbProbes

Generate probe directions for the Dire Orb teleportation using the Fibonacci sphere method
================
*/

#define NUM_PROBES 50
#define PHI 2.39996322973f

static vec3_t	orb_probes[NUM_PROBES];

void G_GenerateDireOrbProbes( void ) {
	int i;
	float x, y, z, r;

	for ( i = 0; i < NUM_PROBES; i++ ) {
		y = 1.0f - ( i / (float)( NUM_PROBES - 1 ) ) * 2.0f;
		r = sqrtf( 1.0f - y * y );
		x = cosf( PHI * i ) * r;
		z = sinf( PHI * i ) * r;
		orb_probes[i][0] = x;
		orb_probes[i][1] = y;
		orb_probes[i][2] = z;
	}
}

/*
================
G_ExplodeOrb

Explode the orb
================
*/
static
void ExplodeOrb( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	ent->parent->client->ps.ab_num = 0;
	ent->parent->client->ps.ab_flags = 0;
	ent->parent->client->ps.ab_time = 0;

	VectorCopy( ent->movedir, dir );
	VectorNormalize( dir );

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );
	ent->freeAfterEvent = qtrue;
	trap_LinkEntity( ent );
}

static
gentity_t *ThrowDireOrb( gentity_t *self, vec3_t start, vec3_t dir ) {
	gentity_t	*orb;

	VectorNormalize (dir);

	orb = G_Spawn();
	orb->classname = "orb";
	orb->nextthink = level.time + 15000;
	orb->think = ExplodeOrb;
	orb->s.eType = ET_MISSILE;
	orb->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	orb->s.weapon = WP_DIRE_ORB;
	orb->r.ownerNum = self->s.number;
	orb->parent = self;
	orb->damage = 100;
	orb->splashDamage = 100;
	orb->splashRadius = 120;
	orb->methodOfDeath = MOD_ROCKET;
	orb->splashMethodOfDeath = MOD_ROCKET_SPLASH;
	orb->clipmask = MASK_PLAYERSOLID;
	orb->target_ent = NULL;

	orb->s.pos.trType = TR_LINEAR;
	orb->s.pos.trTime = level.time - 50;		// move a bit on the very first frame
	VectorCopy( dir, orb->movedir );			// need it to backtrace the destination point when the orb is stuck in a surface
	VectorCopy( start, orb->s.pos.trBase );
	VectorCopy( start, orb->s.origin ); // ???
	VectorScale( dir, 900, orb->s.pos.trDelta );
	SnapVector( orb->s.pos.trDelta );			// save net bandwidth
	VectorCopy( start, orb->r.currentOrigin );

	self->client->ps.ab_num = orb->s.number; // store the orb entity number to know where to teleport
	self->client->ps.ab_flags |= ABF_ENGAGED;

	return orb;
}

static
void TeleportToTheOrb( gentity_t *self ) {
	gentity_t	*orb;
	vec3_t		dir;
	vec3_t		destination;
	vec3_t		probe;
	vec3_t		normal;
	vec3_t		v, q, s;
	vec3_t		pmins;
	vec3_t		pmaxs;
	float		diameter;
	float		distance, d;
	int			i;
	trace_t		tr;

	// get the orb entity
	orb = &g_entities[self->client->ps.ab_num];
	// store its position and direction
	VectorCopy( orb->r.currentOrigin, destination );
	VectorCopy( orb->movedir, dir );
	//VectorNormalize( dir );

	// done with the orb
	ExplodeOrb( orb );

#if 1
	VectorCopy( pm->mins, pmins );
	VectorCopy( pm->maxs, pmaxs );
	pmins[0] -= 1.0f;
	pmins[1] -= 1.0f;
	pmins[2] -= 1.0f;
	pmaxs[0] += 1.0f;
	pmaxs[1] += 1.0f;
	pmaxs[2] += 1.0f;
#else
	VectorScale( pm->mins, 1.2f, pmins );
	VectorScale(p m->maxs, 1.2f, pmaxs );
#endif
	VectorSubtract( pmaxs, pmins, v );
	diameter = VectorLength( v );

	// check if we can teleport right there freely

	VectorMA( destination, -1, dir, probe ); // fly the last unit with Ranger's measures
	trap_Trace( &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
	if ( !tr.startsolid && tr.fraction == 1.0f ) { // yes we can
		TeleportPlayer( self, destination, self->client->ps.viewangles );
		VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
		return;
	}

	// now we're going to get stuck, so let's find some other spot to teleport to
	// step back and trace the final section as if we're the orb to see which surface we run into
	VectorMA( destination, -50, dir, probe );
	VectorMA( probe, 1000, dir, q );
	trap_Trace( &tr, probe, orb->r.mins, orb->r.maxs, q, self->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid ) {
		// I can't image why this might happen as the orb already flew to the destination
		// just in case I assume it's going to be too tight for Ranger so skip the normal backtracing
		goto skip_normal_backtrace;
	}
	VectorCopy( tr.plane.normal, normal );
	// step back and trace the final section along the normal as if we're actually the ranger
	VectorMA( destination, diameter, normal, probe );
	trap_Trace( &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid || tr.fraction == 0.0f ) { // too tight for Ranger
		goto skip_normal_backtrace;
	}

	VectorCopy( tr.endpos, destination );

	// check the destination once again (safety first)
	VectorMA( destination, -1, dir, probe );
	trap_Trace( &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid || ( 1.0f - tr.fraction ) > 0.001f ) { // shit
		goto skip_normal_backtrace;
	}
	TeleportPlayer( self, destination, self->client->ps.viewangles );
	// TeleportPlayer pushes the player forward, I don't like it
	VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
	return;

skip_normal_backtrace:
	VectorCopy( orb->r.currentOrigin, destination );
//	VectorScale( dir, -2.0f, s ); // give some preference to the orb flying direction
	s[0] = s[1] = s[2] = 0.0f;

	// iterate over evenly distributed direction vectors and calculate which direction around
	// the destination point is the "freest"
	for ( i = 0; i < NUM_PROBES; i++ ) {
		//if ( DotProduct( orb_probes[i], dir) < 0 )
		{
			VectorMA( destination, diameter, orb_probes[i], v );
			trap_Trace( &tr, v, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
			if ( !tr.startsolid ) {
				VectorMA( s, tr.fraction, orb_probes[i], s );
			}
		}
	}
	// it might happen that there's no usable direction actually
	if ( VectorNormalize( s ) ) {
		VectorMA( destination, diameter, s, v );
		trap_Trace( &tr, v, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
		if ( !tr.startsolid ) {
			TeleportPlayer( self, tr.endpos, self->client->ps.viewangles );
			// TeleportPlayer pushes the player forward, I don't like it
			VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
			return;
		}
	}
trace_along_dir:
	VectorCopy( orb->r.currentOrigin, destination );
	VectorScale( dir, -diameter, v ); // stepping back along orb movement direction using Ranger's diameter
	VectorCopy( destination, probe );	
	VectorSubtract( orb->s.origin, destination, q );
	distance = VectorLength( q );
	d = 0.0f;
	do {
		VectorAdd( probe, v, probe );
		VectorSubtract( probe, destination, q );
		d += diameter;
		if ( d > distance ) {
			VectorCopy( orb->s.origin, probe );
		}
		trap_Trace( &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
		if ( !tr.startsolid && tr.fraction != 0.0f ) {
			TeleportPlayer( self, tr.endpos, self->client->ps.viewangles );
			VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
			break;
		}
	} while ( d < distance );
	return;
}


void G_AcidThink( gentity_t *ent ) {
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
	//if ( ent->splashDamage ) {
	//	if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent
	//		, ent->splashMethodOfDeath ) ) {
	//		g_entities[ent->r.ownerNum].client->accuracy_hits++;
	//	}
	//}

	trap_LinkEntity( ent );
}

#if 0
static
gentity_t *ThrowAcidSpit( gentity_t *self, vec3_t start, vec3_t dir ) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "spit";
	bolt->nextthink = level.time + 2500;
	bolt->think = G_AcidThink;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_ACID_SPIT;
	bolt->s.eFlags = EF_BOUNCE_HALF;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = 15;
	bolt->splashDamage = 0;
	bolt->splashRadius = 0;
	bolt->methodOfDeath = MOD_ACID_SPIT;
	bolt->splashMethodOfDeath = MOD_ACID_SPIT;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time - 50;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, 700, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

void G_PoisonPlayer(gentity_t* ent, gentity_t* other, qboolean direct) {
	G_Printf("Poisoned\n");
}
#endif


static
 void ActivateInjection( gentity_t *ent ) {
	ent->health += 50;
	if ( ent->health > champion_stats[CHAMP_ANARKI].max_health ) {
		ent->health = champion_stats[CHAMP_ANARKI].max_health;
	}
	ent->client->ps.powerups[PW_SCOUT] = level.time + champion_stats[CHAMP_ANARKI].ability_duration * 100;
	ent->client->ps.baseHealth++;
 }

void G_ActivateAbility( gentity_t *ent ) {
	int champ;
	vec3_t forward, right, up, muzzle;

	champ = ent->client->ps.champion;
 	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );

	trap_SendServerCommand( -1, va( "print \"Ability engaged: %d\n\"", champ ) );
	switch ( champ ) {		
		case CHAMP_ANARKI:
			ActivateInjection( ent );
			break;
		case CHAMP_RANGER:
			if ( ent->client->ps.ab_num != 0 ) {
				TeleportToTheOrb( ent );
			} else {
				ThrowDireOrb( ent, muzzle, forward );
			}
			break;
		//case CHAMP_SORLAG:
		//	ThrowAcidSpit( ent, muzzle, forward );
		//	break;
	}
}
