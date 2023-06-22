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

/*
===========================================================================
Copyright (C) 2003 Sergei Shubin

This file is part of Quake III Champions source code.
Quake and Quake III Arena are trademarks of id Software, Inc.
===========================================================================
*/

//
// cg_decals.c -- wall decals

#include "cg_local.h"

/*
===================================================================

DECAL POLYS

===================================================================
*/


decalPoly_t		cg_activeDecalPolys;	// double linked list
decalPoly_t		*cg_freeDecalPolys;		// single linked list
decalPoly_t		cg_decalPolys[MAX_DECAL_POLYS];
static			int	decalTotal;

/*
===================
CG_InitDecalPolys

This is called at startup and for tournement restarts
===================
*/
void	CG_InitDecalPolys( void ) {
	int		i;

	memset( cg_decalPolys, 0, sizeof(cg_decalPolys) );

	cg_activeDecalPolys.nextDecal = &cg_activeDecalPolys;
	cg_activeDecalPolys.prevDecal = &cg_activeDecalPolys;
	cg_freeDecalPolys = cg_decalPolys;
	for ( i = 0 ; i < MAX_DECAL_POLYS - 1 ; i++ ) {
		cg_decalPolys[i].nextDecal = &cg_decalPolys[i+1];
	}
}


/*
==================
CG_FreeDecalPoly
==================
*/
void CG_FreeDecalPoly( decalPoly_t *le ) {
	if ( !le->prevDecal || !le->nextDecal ) {
		CG_Error( "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prevDecal->nextDecal = le->nextDecal;
	le->nextDecal->prevDecal = le->prevDecal;

	// the free list is only singly linked
	le->nextDecal = cg_freeDecalPolys;
	cg_freeDecalPolys = le;
}

/*
===================
CG_AllocDecal

Will allways succeed, even if it requires freeing an old active decal
===================
*/
decalPoly_t	*CG_AllocDecal( void ) {
	decalPoly_t	*le;
	int time;

	if ( !cg_freeDecalPolys ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		time = cg_activeDecalPolys.prevDecal->time;
		while (cg_activeDecalPolys.prevDecal && time == cg_activeDecalPolys.prevDecal->time) {
			CG_FreeDecalPoly( cg_activeDecalPolys.prevDecal );
		}
	}

	le = cg_freeDecalPolys;
	cg_freeDecalPolys = cg_freeDecalPolys->nextDecal;

	memset( le, 0, sizeof( *le ) );

	// link into the active list
	le->nextDecal = cg_activeDecalPolys.nextDecal;
	le->prevDecal = &cg_activeDecalPolys;
	cg_activeDecalPolys.nextDecal->prevDecal = le;
	cg_activeDecalPolys.nextDecal = le;
	return le;
}



/*
=================
CG_ImpactDecal

origin should be a point within a unit of the plane
dir should be the plane normal

temporary decals will not be stored or randomly oriented, but immediately
passed to the renderer.
=================
*/
#define	MAX_DECAL_FRAGMENTS		512
#define	MAX_DECAL_POINTS		1536

void CG_Decal(
	qhandle_t decalShader,
	const vec3_t origin,
	const vec3_t dir, 
	float orientation,
	float red, float green, float blue, float alpha,
	qboolean alphaFade,
	float radius,
	qboolean temporary
)
{
	vec3_t			axis[3];
	float			texCoordScale;
	vec3_t			originalPoints[4];
	byte			colors[4];
	int				i, j;
	int				numFragments;
	static markFragment_t	markFragments[MAX_DECAL_FRAGMENTS];
	markFragment_t	*mf;
	static vec3_t	markPoints[MAX_DECAL_POINTS];
	vec3_t			projection;

	if ( !cg_addMarks.integer ) {
		return;
	}

	if ( radius <= 0 ) {
		CG_Error( "CG_ImpactDecal called with <= 0 radius" );
	}

	//if ( decalTotal >= MAX_MARK_POLYS ) {
	//	return;
	//}

	// create the texture axis
	VectorNormalize2( dir, axis[0] );
	VectorInverse( axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orientation );
	CrossProduct( axis[0], axis[2], axis[1] );

	texCoordScale = 0.5 * 1.0 / radius;

	//// create the full polygon
	//for ( i = 0 ; i < 3 ; i++ ) {
	//	originalPoints[0][i] = origin[i] - radius * axis[1][i] - radius * axis[2][i];
	//	originalPoints[1][i] = origin[i] + radius * axis[1][i] - radius * axis[2][i];
	//	originalPoints[2][i] = origin[i] + radius * axis[1][i] + radius * axis[2][i];
	//	originalPoints[3][i] = origin[i] - radius * axis[1][i] + radius * axis[2][i];
	//}

	// get the fragments
	//VectorScale( dir, -60, projection );
#if 0
	numFragments = trap_CM_ProjectDecal( 4, (void *)originalPoints,
					projection, MAX_DECAL_POINTS, markPoints[0],
					MAX_DECAL_FRAGMENTS, markFragments );
#else
	numFragments = trap_CM_ProjectDecal( origin, dir, radius, 0, orientation, //0, ( void * )xpoints, NULL,
		MAX_DECAL_POINTS, markPoints, MAX_DECAL_FRAGMENTS, markFragments );
#endif

	colors[0] = red * 255;
	colors[1] = green * 255;
	colors[2] = blue * 255;
	colors[3] = alpha * 255;

	for ( i = 0, mf = markFragments ; i < numFragments ; i++, mf++ ) {
		polyVert_t	*v;
		polyVert_t	verts[MAX_VERTS_ON_DECAL_POLY];
		decalPoly_t	*decal;

		// we have an upper limit on the complexity of polygons
		// that we store persistantly
		if ( mf->numPoints > MAX_VERTS_ON_DECAL_POLY ) {
			mf->numPoints = MAX_VERTS_ON_DECAL_POLY;
		}
		for ( j = 0, v = verts ; j < mf->numPoints ; j++, v++ ) {
			vec3_t		delta;

			VectorCopy( markPoints[mf->firstPoint + j], v->xyz );

			VectorSubtract( v->xyz, origin, delta );
			v->st[0] = 0.5 + DotProduct( delta, axis[1] ) * texCoordScale;
			v->st[1] = 0.5 + DotProduct( delta, axis[2] ) * texCoordScale;
			*(int *)v->modulate = *(int *)colors;
		}

		// if it is a temporary (shadow) mark, add it immediately and forget about it
		if ( temporary ) {
			trap_R_AddPolyToScene( decalShader, mf->numPoints, verts );
			continue;
		}

		// otherwise save it persistantly
		decal = CG_AllocDecal();
		decal->time = cg.time;
		decal->alphaFade = alphaFade;
		decal->shader = decalShader;
		decal->poly.numVerts = mf->numPoints;
		decal->color[0] = red;
		decal->color[1] = green;
		decal->color[2] = blue;
		decal->color[3] = alpha;
		memcpy( decal->verts, verts, mf->numPoints * sizeof( verts[0] ) );
		decalTotal++;
	}
}


/*
===============
CG_AddMarks
===============
*/
#define	MARK_TOTAL_TIME		10000
#define	MARK_FADE_TIME		1000

void CG_AddDecals( void ) {
	int			j;
	decalPoly_t	*mp, *next;
	int			t;
	int			fade;

	if ( !cg_addMarks.integer ) {
		return;
	}

	mp = cg_activeDecalPolys.nextDecal;
	for ( ; mp != &cg_activeDecalPolys ; mp = next ) {
		// grab next now, so if the local entity is freed we
		// still have it
		next = mp->nextDecal;

		// see if it is time to completely remove it
		if ( cg.time > mp->time + MARK_TOTAL_TIME ) {
			CG_FreeDecalPoly( mp );
			continue;
		}

		// fade out the energy bursts
		if ( mp->shader == cgs.media.energyMarkShader ) {

			fade = 450 - 450 * ( (cg.time - mp->time ) / 3000.0 );
			if ( fade < 255 ) {
				if ( fade < 0 ) {
					fade = 0;
				}
				if ( mp->verts[0].modulate[0] != 0 ) {
					for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
						mp->verts[j].modulate[0] = mp->color[0] * fade;
						mp->verts[j].modulate[1] = mp->color[1] * fade;
						mp->verts[j].modulate[2] = mp->color[2] * fade;
					}
				}
			}
		}

		// fade all marks out with time
		t = mp->time + MARK_TOTAL_TIME - cg.time;
		if ( t < MARK_FADE_TIME ) {
			fade = 255 * t / MARK_FADE_TIME;
			if ( mp->alphaFade ) {
				for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
					mp->verts[j].modulate[3] = fade;
				}
			} else {
				for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
					mp->verts[j].modulate[0] = mp->color[0] * fade;
					mp->verts[j].modulate[1] = mp->color[1] * fade;
					mp->verts[j].modulate[2] = mp->color[2] * fade;
				}
			}
		}


		trap_R_AddPolyToScene( mp->shader, mp->poly.numVerts, mp->verts );
	}
}
