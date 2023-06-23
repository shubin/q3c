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
CG_Decal

origin should be a point within a unit of the plane
dir should be the plane normal
QC TODO: write a proper description here

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
	byte			colors[4];
	int				i, j;
	int				numFragments;
	static markFragment_t	decalFragments[MAX_DECAL_FRAGMENTS];
	markFragment_t	*mf;
	static vec3_t	decalPoints[MAX_DECAL_POINTS];
	static vec3_t	decalAttribs[MAX_DECAL_POINTS];

	if ( !cg_addMarks.integer ) {
		return;
	}

	if ( radius <= 0 ) {
		CG_Error( "CG_Decal called with <= 0 radius" );
	}

	numFragments = trap_CM_ProjectDecal( origin, dir, radius, radius * 1.5f, orientation,
		MAX_DECAL_POINTS, decalPoints, decalAttribs,
		MAX_DECAL_FRAGMENTS, decalFragments );

	colors[0] = red * 255;
	colors[1] = green * 255;
	colors[2] = blue * 255;
	colors[3] = alpha * 255;

	for ( i = 0, mf = decalFragments ; i < numFragments ; i++, mf++ ) {
		polyVert_t	*v;
		polyVert_t	verts[MAX_VERTS_ON_DECAL_POLY];
		float		vertAlpha[MAX_VERTS_ON_DECAL_POLY];
		decalPoly_t	*decal;

		// we have an upper limit on the complexity of polygons
		// that we store persistantly
		if ( mf->numPoints > MAX_VERTS_ON_DECAL_POLY ) {
			mf->numPoints = MAX_VERTS_ON_DECAL_POLY;
		}
		for ( j = 0, v = verts ; j < mf->numPoints ; j++, v++ ) {
			vec3_t		delta;

			VectorCopy( decalPoints[mf->firstPoint + j], v->xyz );
			v->st[0] = decalAttribs[mf->firstPoint + j][0];
			v->st[1] = decalAttribs[mf->firstPoint + j][1];
			*(int *)v->modulate = *(int *)colors;
			v->modulate[3] = decalAttribs[mf->firstPoint + j][2] * 255;
			vertAlpha[j] = decalAttribs[mf->firstPoint + j][2];
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
		memcpy( decal->vertAlpha, vertAlpha, mf->numPoints * sizeof( vertAlpha[0] ) );
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
						mp->verts[j].modulate[3] = mp->vertAlpha[j] * 255.0f;
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
					mp->verts[j].modulate[3] = fade * mp->vertAlpha[j];
				}
			} else {
				for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
					mp->verts[j].modulate[0] = mp->color[0] * fade;
					mp->verts[j].modulate[1] = mp->color[1] * fade;
					mp->verts[j].modulate[2] = mp->color[2] * fade;
					mp->verts[j].modulate[3] = mp->vertAlpha[j] * fade;
				}
			}
		}


		trap_R_AddPolyToScene( mp->shader, mp->poly.numVerts, mp->verts );
	}
}
