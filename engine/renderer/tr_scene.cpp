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

#include "tr_local.h"


static int r_firstSceneDrawSurf;
static int r_firstSceneLitSurf;

static int r_numdlights;
static int r_firstSceneDlight;

static int r_numentities;
static int r_firstSceneEntity;

static int r_numpolys;
static int r_firstScenePoly;

static int r_numpolyverts;


void R_ClearFrame()
{
	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;
	r_firstSceneLitSurf = 0;

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


void RE_ClearScene()
{
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
}


/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/


// adds all the scene's polys into this view's drawsurf list

void R_AddPolygonSurfaces()
{
	tr.currentEntityNum = ENTITYNUM_WORLD;

	const srfPoly_t* poly = tr.refdef.polys;
	for (int i = 0; i < tr.refdef.numPolys; ++i, ++poly) {
		R_AddDrawSurf( (const surfaceType_t*)poly, R_GetShaderByHandle( poly->hShader ) );
	}
}


void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t* verts, int numPolys )
{
	if ( !tr.registered ) {
		return;
	}

	if ( !hShader ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_AddPolyToScene: NULL poly shader\n" );
		return;
	}

	// make sure the entire set will fit, rather than taking as many as we can
	// both ways have downsides, but if we ARE hitting the cap we're already screwed
	// and it's better to avoid something degenerate than to squeeze in 3 extra snowflakes
	if ( r_numpolyverts + numPolys * numVerts > max_polyverts || r_numpolys + numPolys > max_polys ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n" );
		return;
	}

	for ( int j = 0; j < numPolys; j++ ) {
		srfPoly_t* poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];

		Com_Memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );

		r_numpolys++;
		r_numpolyverts += numVerts;

		vec3_t bounds[2];
		VectorCopy( poly->verts[0].xyz, bounds[0] );
		VectorCopy( poly->verts[0].xyz, bounds[1] );
		for ( int i = 1 ; i < poly->numVerts ; i++ ) {
			AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
		}
		VectorAdd(bounds[0], bounds[1], poly->localOrigin);
		VectorScale(poly->localOrigin, 0.5f, poly->localOrigin);

		poly->fogIndex = 0;
		// find which fog volume the poly is in (if any)
		if (tr.world && (tr.world->numfogs > 1)) {
			for ( int i = 1 ; i < tr.world->numfogs ; i++ ) {
				const fog_t* fog = &tr.world->fogs[i];
				if ( bounds[1][0] >= fog->bounds[0][0]
						&& bounds[1][1] >= fog->bounds[0][1]
						&& bounds[1][2] >= fog->bounds[0][2]
						&& bounds[0][0] <= fog->bounds[1][0]
						&& bounds[0][1] <= fog->bounds[1][1]
						&& bounds[0][2] <= fog->bounds[1][2] ) {
					poly->fogIndex = i;
					break;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////


void RE_AddRefEntityToScene( const refEntity_t* ent, qbool intShaderTime )
{
	if ( !tr.registered ) {
		return;
	}
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=402
	if ( r_numentities >= ENTITYNUM_WORLD ) {
		return;
	}

	if ( ent->reType < 0 || ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	trRefEntity_t* const trEnt = &backEndData->entities[r_numentities];
	trEnt->e = *ent;
	trEnt->lightingCalculated = qfalse;
	trEnt->intShaderTime = intShaderTime;
	r_numentities++;
}


void RE_AddLightToScene( const vec3_t org, float radius, float r, float g, float b )
{
	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= MAX_DLIGHTS ) {
		return;
	}
	if ( radius <= 0 ) {
		return;
	}

	dlight_t* dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = radius;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


// draw a 3D view into a part of the window, then return to 2D drawing
// rendering a scene may require multiple views to be rendered to handle mirrors

void RE_RenderScene( const refdef_t* fd, int us )
{
	if ( !tr.registered ) {
		return;
	}

	if ( r_norefresh->integer ) {
		return;
	}

	const int64_t startTime = ri.Microseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	Com_Memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.microSeconds = us;
	tr.refdef.rdflags = fd->rdflags;
#if defined( QC )
	tr.refdef.forcedGreyscale = fd->forcedGreyscale;
#endif //

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( !(tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		// compare the area bits
		int areaDiff = 0;
		for (int i = 0; i < MAX_MAP_AREA_BYTES/4; ++i) {
			areaDiff |= ((int*)tr.refdef.areamask)[i] ^ ((const int*)fd->areamask)[i];
			((int*)tr.refdef.areamask)[i] = ((const int*)fd->areamask)[i];
		}
		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}

	// derived info

	tr.refdef.floatTime =
		(double)tr.refdef.time / 1000.0 +
		(double)tr.refdef.microSeconds / 1000000.0;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the dlights if it needs to be disabled
	if (!r_dynamiclight->integer) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates
	//
	viewParms_t parms;
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = tr.refdef.y;
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;
	parms.isPortal = qfalse;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

#if defined( QC )
	if ( fd->rdflags & RDF_FORCEGREYSCALE ) {
		parms.greyscale = fd->forcedGreyscale;
	} else {
		parms.greyscale = 0.0f;
	}
#endif // QC

	VectorCopy( fd->vieworg, parms.orient.origin );
	VectorCopy( fd->viewaxis[0], parms.orient.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.orient.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.orient.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

	R_RenderScene( &parms );

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	tr.pc[RF_USEC] += (int)( ri.Microseconds() - startTime );
}
