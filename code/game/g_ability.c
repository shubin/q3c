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
#include "bg_champions.h"

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
		r = sqrt( 1.0f - y * y );
		x = cos( PHI * i ) * r;
		z = sin( PHI * i ) * r;
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

	self->client->ps.ab_time = 0;

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
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	orb->s.otherEntityNum = self->s.number;
#endif
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
	G_TraceEx( self->s.clientNum, &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
	if ( !tr.startsolid && tr.fraction == 1.0f ) { // yes we can
		TeleportPlayer( self, destination, self->client->ps.viewangles );
		VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
		return;
	}

	// now we're going to get stuck, so let's find some other spot to teleport to
	// step back and trace the final section as if we're the orb to see which surface we run into
	VectorMA( destination, -50, dir, probe );
	VectorMA( probe, 1000, dir, q );
	G_TraceEx( self->s.clientNum, &tr, probe, orb->r.mins, orb->r.maxs, q, self->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid ) {
		// I can't image why this might happen as the orb already flew to the destination
		// just in case I assume it's going to be too tight for Ranger so skip the normal backtracing
		goto skip_normal_backtrace;
	}
	VectorCopy( tr.plane.normal, normal );
	// step back and trace the final section along the normal as if we're actually the ranger
	VectorMA( destination, diameter, normal, probe );
	G_TraceEx( self->s.clientNum, &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid || tr.fraction == 0.0f ) { // too tight for Ranger
		goto skip_normal_backtrace;
	}

	VectorCopy( tr.endpos, destination );

	// check the destination once again (safety first)
	VectorMA( destination, -1, dir, probe );
	G_TraceEx( self->s.clientNum, &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
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
		if ( DotProduct( orb_probes[i], dir ) < 0 )
		{
			VectorMA( destination, diameter, orb_probes[i], v );
			G_TraceEx( self->s.clientNum, &tr, v, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
			if ( !tr.startsolid ) {
				VectorMA( s, tr.fraction, orb_probes[i], s );
			}
		}
	}
	// it might happen that there's no usable direction actually
	if ( VectorNormalize( s ) ) {
		VectorMA( destination, diameter, s, v );
		G_TraceEx( self->s.clientNum, &tr, v, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
		if ( !tr.startsolid ) {
			TeleportPlayer( self, tr.endpos, self->client->ps.viewangles );
			// TeleportPlayer pushes the player forward, I don't like it
			VectorScale( self->client->ps.velocity, 0.0f, self->client->ps.velocity );
			return;
		}
	}
//trace_along_dir:
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
		G_TraceEx( self->s.clientNum, &tr, probe, pmins, pmaxs, destination, self->s.number, MASK_PLAYERSOLID );
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

#if 1
static
gentity_t *ThrowAcidSpit( gentity_t *self, vec3_t start, vec3_t dir ) {
	gentity_t	*spit;

	VectorNormalize (dir);

	spit = G_Spawn();
	spit->classname = "spit";
	spit->nextthink = level.time + 2500;
	spit->think = G_AcidThink;
	spit->s.eType = ET_MISSILE;
	spit->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	spit->s.weapon = WP_ACID_SPIT;
	spit->s.eFlags = 0;
	spit->r.ownerNum = self->s.number;
	spit->parent = self;
#if defined( UNLAGGED ) //unlagged - projectile nudge
	// we'll need this for nudging projectiles later
	spit->s.otherEntityNum = self->s.number;
#endif
	spit->damage = 15;
	spit->splashDamage = 0;
	spit->splashRadius = 0;
	spit->methodOfDeath = MOD_UNKNOWN;// MOD_ACID_SPIT;
	spit->splashMethodOfDeath = MOD_UNKNOWN;// MOD_ACID_SPIT;
	spit->clipmask = MASK_SHOT;
	spit->target_ent = NULL;

	spit->s.pos.trType = TR_GRAVITY;
	spit->s.pos.trTime = level.time - 50;		// move a bit on the very first frame
	spit->s.pos.trGravity = DEFAULT_GRAVITY;
	VectorCopy( start, spit->s.pos.trBase );
	VectorScale( dir, 550, spit->s.pos.trDelta );
	SnapVector( spit->s.pos.trDelta );			// save net bandwidth

	VectorCopy( start, spit->r.currentOrigin );

	return spit;
}
#endif


static
 void ActivateInjection( gentity_t *ent ) {
	ent->client->ps.ab_flags &= ~ABF_READY;
	ent->client->ps.ab_time = 0;
	ent->health += 50;

	if ( ent->health > champion_stats[CHAMP_ANARKI].max_health ) {
		ent->health = champion_stats[CHAMP_ANARKI].max_health;
	}
	ent->client->ps.powerups[PW_SCOUT] = level.time + champion_stats[CHAMP_ANARKI].ability_duration * 100;
	ent->client->ps.baseHealth++;
	ent->client->pers.baseHealth = ent->client->ps.baseHealth;
	G_RemoveDOT( ent, DOT_ALL );
 }

static void ThrowGrenade( gentity_t *ent, vec3_t muzzle, vec3_t forward ) {
	playerState_t *ps;
	gentity_t	*m;
	int quadFactor;

	ps = &ent->client->ps;

	if ( !BG_CanAbilityBeActivated( ps ) ) {
		return;
	}
	ps->ab_time -= 9;
	ps->ab_misctime = 400;
	ps->ab_flags &= ~ABF_ENGAGED;
	ps->ab_flags &= ~ABF_READY;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	m = fire_grenade( ent, muzzle, forward );

	// NERF this grenade a bit
	m->damage = 75;
	m->splashDamage = 75;
	m->splashRadius = 120;
	m->s.constantLight = 255 | ( 16 << 24 ); // red glow

	quadFactor = ps->powerups[PW_QUAD] ? g_quadfactor.value : 1;
	m->damage *= quadFactor;
	m->splashDamage *= quadFactor;
}

void G_ActivateAbility( gentity_t *ent ) {
	int champ;
	vec3_t forward, right, up, muzzle;

	champ = ent->client->ps.champion;

	if ( !( ent->client->ps.ab_flags & ABF_ENGAGED ) ) {
		ent->client->ps.ab_flags |= ABF_ENGAGED;
#if 0
		if ( champ == CHAMP_SORLAG ) {
			pm->ps->ab_time = 150; // throw some spit each 0.15 sec
			pm->ps->ab_num = 5;    // do it five times
			pm->ps->ab_flags &= ~ABF_READY;
		}
#endif
	}


 	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );
	//trap_SendServerCommand( -1, va( "print \"Ability engaged: %d\n\"", champ ) );
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
		case CHAMP_KEEL:
			ThrowGrenade( ent, muzzle, forward );
			break;
		case CHAMP_SORLAG:
			ent->client->ps.ab_num = MAX_SPITS - 1;
			ent->client->ps.ab_misctime = level.time + SPIT_DELAY;
			ThrowAcidSpit( ent, muzzle, forward );
			break;
		case CHAMP_GALENA:
			G_ThrowTotem( ent, muzzle, forward );
			break;
		case CHAMP_VISOR:
			ent->client->ps.ab_time = level.time + champion_stats[CHAMP_VISOR].ability_duration * 100;
			ent->client->ps.ab_misctime = level.time;
			ent->r.piercingSightMask = -1;
			break;
	}
}


static void UpdateInjection( gclient_t *client ) {
	if ( !( client->ps.ab_flags & ABF_ENGAGED ) ) {
		return;
	}

	client->ps.ab_time++;
	if ( client->ps.ab_time > champion_stats[CHAMP_ANARKI].ability_duration ) {
		client->ps.ab_flags = 0;
		client->ps.ab_time = 0;
	}
}

static void UpdateKeelGrenades( gclient_t *client ) {
	if ( client->ps.ab_misctime != 0 ) {
		client->ps.ab_misctime -= 100;
		if ( client->ps.ab_misctime < 0 ) {
			client->ps.ab_misctime = 0;
		}
	}
}

void G_AbilityTickTenth( gclient_t *client ) {
	switch ( client->ps.champion ) {
		case CHAMP_ANARKI: UpdateInjection( client ); break;
		case CHAMP_KEEL: UpdateKeelGrenades( client ); break;
	}
}

void G_AbilityTickSecond( gclient_t *client ) {
	// ability regeneration timer
	if ( !( client->ps.ab_flags & ABF_READY ) && !( client->ps.ab_flags & ABF_ENGAGED ) ) {
		if ( client->ps.ab_time < champion_stats[client->ps.champion].ability_cooldown ) {
			client->ps.ab_time++;
		}
		if ( client->ps.ab_time == champion_stats[client->ps.champion].ability_cooldown && !( client->ps.ab_flags & ABF_READY ) ) {
			client->ps.ab_flags |= ABF_READY;
		}
	}
}

void G_AbilityTickFrame( gclient_t *client ) {
	vec3_t		forward, right, up, muzzle;
	int			damage;
	gclient_t	*other;

	// visor
	if ( client->ps.champion == CHAMP_VISOR && ( client->ps.ab_flags & ABF_ENGAGED ) ) {
		if ( level.time > client->ps.ab_time ) {
			client->ps.ab_flags = 0;
			client->ps.ab_time = 0;
			g_entities[client->ps.clientNum].r.piercingSightMask = 0;
		}
	}
	// sorlag
	if ( client->ps.champion == CHAMP_SORLAG && client->ps.ab_num > 0 && level.time > client->ps.ab_misctime ) {
		client->ps.ab_num--;
		client->ps.ab_misctime += 150;
		AngleVectors( client->ps.viewangles, forward, right, up );
		CalcMuzzlePointOrigin( &g_entities[client->ps.clientNum], client->oldOrigin, forward, right, up, muzzle );
		ThrowAcidSpit( &g_entities[client->ps.clientNum], muzzle, forward );
	}

	if ( client->ps.dotAcidNum > 0 && level.time > client->ps.dotAcidTime ) {
		other = &g_clients[client->ps.dotAcidOwner];
		client->ps.dotAcidNum--;
		client->ps.dotAcidTime = level.time + ACID_DOT_TICK;
		damage = ACID_DOT_AMOUNT;
		damage *= other->ps.powerups[PW_QUAD] ? g_quadfactor.value : 1;
		G_Damage( 
			&g_entities[client->ps.clientNum], NULL, 
			&g_entities[other->ps.clientNum],
			NULL, NULL, damage, DAMAGE_NO_KNOCKBACK, MOD_SLIME
		);
		//G_Printf( "ACID DOT\n" );
	}
}

static void G_PoisonPlayer( gentity_t *ent, gentity_t *other, qboolean direct ) {
	playerState_t *ps;

	ps = &other->client->ps;
	if ( ps->dotAcidNum == 0 ) {
		ps->dotAcidNum = ACID_DOT_TIMES;
		ps->dotAcidTime = level.time + 50; // first tick soon
	}
}

static void AcidSpit_Think( gentity_t *ent ) {
	G_FreeEntity( ent );
}

static void AcidSpit_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace ) {
	vec3_t		lower, upper;
	vec3_t		v1, v2;
//
	if ( !other->client ) {
		return;
	}
	if ( G_IsEntityFriendly( other->client->ps.clientNum, trigger->s.number ) ) {
		//return;
	}

	// two-point check
	VectorCopy( other->s.pos.trBase, lower );
	VectorCopy( lower, upper );
	upper[2] += 50;

	VectorSubtract( trigger->s.pos.trBase, lower, v1 );
	VectorSubtract( trigger->s.pos.trBase, upper, v2 );
	if ( VectorLength( v1 ) > ACID_SPIT_RADIUS && VectorLength( v2 ) > ACID_SPIT_RADIUS ) {
		return;
	}
	if ( !CanDamage( other, trigger->s.pos.trBase ) ) {
		return;
	}
	G_PoisonPlayer( trigger, other, qfalse );
}

void G_SpitHitWall( gentity_t *ent, trace_t *trace ) {
	gentity_t *trigger;
	gclient_t *client;
	int r;

	if ( ent->parent == NULL || ent->parent->client == NULL ) {
		return;
	}

	// build the totem trigger
	trigger = G_Spawn();

	trigger->classname = "spit trigger";

	r = ACID_SPIT_RADIUS;
	VectorSet( trigger->r.mins, -r, -r, -r );
	VectorSet( trigger->r.maxs, r, r, r );

	G_SetOrigin( trigger, trace->endpos );

	client = ent->parent->client;

	trigger->s.eType = ET_ACID_TRIGGER;
	trigger->s.eFlags = EF_NOFF;
	trigger->s.affiliation = ent->s.affiliation; // same affiliation as the totem
	trigger->parent = ent->parent;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->touch = AcidSpit_Trigger;
	trigger->nextthink = level.time + ACID_LIFETIME;
	trigger->think = AcidSpit_Think;

	// Copy hit direction and position to the trigger entity so we can restore the decal on the client side if needed.
	// There may be cases when user does not receive the impact event so we should make sure thet
	// we'll not have invisible acid spit triggers.
	VectorNegate( ent->s.angles2, trigger->s.angles2 );
	VectorCopy( trace->endpos, trigger->s.origin2 );
	trigger->s.time2 = level.time;
	trigger->s.loopSoundDist = ( int )( random() * 360 );
#if defined( _DEBUG )
	trigger->s.generic1 = r;
#endif // _DEBUG
	trap_LinkEntity( trigger );

	return trigger;
}

void G_RemoveDOT( gentity_t *ent, int flags ) {
	if ( !ent->client ) {
		return;
	}
	if ( DOT_ACID & flags ) {
		ent->client->ps.dotAcidNum = 0;
	}
	if ( DOT_FIRE & flags ) {
		ent->client->ps.dotFireNum = 0;
	}
}
