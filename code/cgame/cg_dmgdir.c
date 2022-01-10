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
// cg_hitmarker.c

#include "cg_local.h"

typedef struct hitmarker_s {
	float angle;
	int damage;
	int time;
	int clientnum;
} hitmarker_t;


#define MAX_MARKERS 32
#define HITMARKER_RADIUS 60.0f
#define HITMARKER_STAY_TIME 500
#define HITMARKER_DISSOLVE_TIME 500

static qboolean DrawHitMarker( hitmarker_t *hm ) {
	refEntity_t		ent;

	byte alpha = 255;

	int phase = cg.time - hm->time;
	if ( phase > HITMARKER_STAY_TIME + HITMARKER_DISSOLVE_TIME ) {
		return qfalse;
	}

	if ( phase > HITMARKER_STAY_TIME ) {
		float fphase = ( phase - HITMARKER_STAY_TIME ) / (float)HITMARKER_DISSOLVE_TIME;
		fphase = sinf( fphase * M_PI/2 );
		alpha = 255 - (int)( 255 * fphase );
	}

	memset( &ent, 0, sizeof( ent ) );
	float angle = ( hm->angle - cg.refdefViewAngles[1] + 180.0f ) / 180.0f * M_PI;
	ent.reType = RT_SPRITE;
	ent.origin[0] = 75.0f;
	ent.origin[1] = cosf( angle ) * HITMARKER_RADIUS;
	ent.origin[2] = -sinf( angle ) * HITMARKER_RADIUS;
	ent.radius = 40;
	ent.customShader = cgs.media.damageDirectionShader;
	ent.rotation = hm->angle - cg.refdefViewAngles[1] - 90.0f;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = alpha;

	trap_R_AddRefEntityToScene( &ent );

	return qtrue;
}

//static linked_list_t s_hitmarkers;

static hitmarker_t s_markers[MAX_MARKERS] = { 0 };
static hitmarker_t* s_hitmarkerByClientNum[MAX_CLIENTS] = { 0 };

void CG_AddDamageDir( int clientnum, int damage, vec3_t position ) {
	hitmarker_t *hm = NULL;
	int i;

	if ( !cg_damageDirection.integer ) {
		return;
	}

	if ( clientnum >= 0 && clientnum < MAX_CLIENTS ) {
		hm = s_hitmarkerByClientNum[clientnum];
	}
	if ( hm == NULL ) {
		for ( i = 0; i < MAX_MARKERS; i++ ) {
			if ( s_markers[i].damage == 0 ) {
				hm = &s_markers[i];
				break;
			}
		}
		if ( clientnum >= 0 && clientnum < MAX_CLIENTS ) {
			s_hitmarkerByClientNum[clientnum] = hm;
		}
	}
	if ( hm != NULL ) {
		hm->angle = ( atan2f( position[1] - cg.refdef.vieworg[1], position[0] - cg.refdef.vieworg[0] ) + M_PI/2 ) / M_PI * 180.0f;
		hm->damage = damage;
		hm->time = cg.time;
		hm->clientnum = clientnum;
	}
}

void CG_ClearDamageDir( void ) {
	memset( s_markers, 0, sizeof ( s_markers ) );
}

void CG_DrawDamageDir( void ) {
	hitmarker_t		*p;
	refdef_t		refdef;

	if ( !cg_damageDirection.integer ) {
		return;
	}

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );
	if ( cg.refdef.width > cg.refdef.height ) {
		refdef.width = cg.refdef.height;
		refdef.height = cg.refdef.height;
		refdef.x = ( cg.refdef.width - cg.refdef.height ) / 2;
	}
	else {
		refdef.width = cg.refdef.width;
		refdef.height = cg.refdef.width;
		refdef.y = ( cg.refdef.height - cg.refdef.width ) / 2;
	}
	refdef.fov_y = 90;
	refdef.fov_x = 90;
	refdef.time = cg.time;

	trap_R_ClearScene();

	for ( p = s_markers; p - s_markers < MAX_MARKERS; p++ ) {
		if ( p->damage == 0 ) {
			continue;
		}
		if ( !DrawHitMarker( p ) ) {
			if ( p->clientnum >= 0 && p->clientnum < MAX_CLIENTS ) {
				s_hitmarkerByClientNum[p->clientnum] = NULL;
			}
			p->damage = 0;
		}
	}

	trap_R_RenderScene( &refdef );
}
