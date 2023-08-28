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
// g_acidspit.c - Sorlag's acid spit implementation
//

#include "g_local.h"
#include "bg_local.h"
#include "bg_champions.h"

void G_PoisonPlayer( gentity_t *ent, gentity_t *other, qboolean direct ) {
	playerState_t *ps;

	ps = &other->client->ps;

	if ( ps->champion == CHAMP_SORLAG ) {
		return;
	}

	if ( ps->dotAcidNum == 0 ) {
		ps->dotAcidNum = ACID_DOT_TIMES;
		ps->dotAcidTime = level.time + 50; // first tick soon
	}
}

static void AcidSpitTrigger_Think( gentity_t *ent ) {
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
		return;
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

	// build the acid spit trigger
	trigger = G_Spawn();

	trigger->classname = "spit trigger";

	r = ACID_SPIT_RADIUS;
	VectorSet( trigger->r.mins, -r, -r, -r );
	VectorSet( trigger->r.maxs, r, r, r );

	G_SetOrigin( trigger, trace->endpos );

	client = ent->parent->client;

	trigger->s.eType = ET_ACID_TRIGGER;
	trigger->s.eFlags = EF_NOFF | EF_FMUTE;
	trigger->s.affiliation = ent->s.affiliation; // same affiliation as the spit projectile
	trigger->parent = ent->parent;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->touch = AcidSpit_Trigger;
	trigger->nextthink = level.time + ACID_LIFETIME;
	trigger->think = AcidSpitTrigger_Think;

	// Copy hit direction and position to the trigger entity so we can restore the decal on the client side if needed.
	// There may be cases when user does not receive the impact event so we should make sure thet
	// we'll not have invisible acid spit triggers.
	VectorNegate( ent->s.angles2, trigger->s.angles2 );
	VectorCopy( trace->endpos, trigger->s.origin2 );
	trigger->s.time2 = level.time;
	trigger->s.modelindex2 = ( int )( random() * 180 );
	trigger->s.generic1 = r; // trigger radius for bounding box display

	trigger->s.loopSound = level.snd_fry;
	trigger->s.loopSoundDist = 200;

	trap_LinkEntity( trigger );
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

void G_ThrowAcidSpit( gentity_t *self, vec3_t start, vec3_t dir ) {
	gentity_t *spit;

	VectorNormalize( dir );

	spit = G_Spawn();
	spit->classname = "spit";
	spit->nextthink = level.time + 2500;
	spit->think = G_AcidThink;
	spit->s.eType = ET_MISSILE;
	spit->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	spit->s.weapon = WP_ACID_SPIT;
	spit->s.eFlags = 0;
	spit->s.affiliation = G_ClientAffiliation( self->client );
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
	spit->splashMethodOfDeath = MOD_ACID_SPIT;
	spit->clipmask = MASK_SHOT;
	spit->target_ent = NULL;

	spit->s.pos.trType = TR_GRAVITY;
	spit->s.pos.trTime = level.time - 50;		// move a bit on the very first frame
	spit->s.pos.trGravity = DEFAULT_GRAVITY;
	VectorCopy( start, spit->s.pos.trBase );
	VectorScale( dir, 550, spit->s.pos.trDelta );
	SnapVector( spit->s.pos.trDelta );			// save net bandwidth

	VectorCopy( start, spit->r.currentOrigin );
}
