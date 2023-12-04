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


// returns true if the grid is completely culled away.
// also sets the clipped hint bit in tess

static qbool R_CullTriSurf( const srfTriangles_t* cv )
{
	return ( R_CullLocalBox( cv->bounds ) == CULL_OUT );
}


// returns true if the grid is completely culled away.
// also sets the clipped hint bit in tess

static qbool R_CullGrid( const srfGridMesh_t* cv )
{
	int sphereCull;

	if ( r_nocurves->integer ) {
		return qtrue;
	}

	if ( tr.currentEntityNum != ENTITYNUM_WORLD ) {
		sphereCull = R_CullLocalPointAndRadius( cv->localOrigin, cv->meshRadius );
	} else {
		sphereCull = R_CullPointAndRadius( cv->localOrigin, cv->meshRadius );
	}

	// check for trivial reject
	if ( sphereCull == CULL_OUT )
	{
		tr.pc[RF_BEZ_CULL_S_OUT]++;
		return qtrue;
	}

	// check bounding box if necessary
	if ( sphereCull == CULL_CLIP )
	{
		tr.pc[RF_BEZ_CULL_S_CLIP]++;

		int boxCull = R_CullLocalBox( cv->meshBounds );

		if ( boxCull == CULL_OUT )
		{
			tr.pc[RF_BEZ_CULL_B_OUT]++;
			return qtrue;
		}
		else if ( boxCull == CULL_IN )
		{
			tr.pc[RF_BEZ_CULL_B_IN]++;
		}
		else
		{
			tr.pc[RF_BEZ_CULL_B_CLIP]++;
		}
	}
	else
	{
		tr.pc[RF_BEZ_CULL_S_IN]++;
	}

	return qfalse;
}


// try to cull surfaces before they are added to the draw list
// this code will also allow mirrors on both sides of a model without recursion

static qbool R_CullSurface( const surfaceType_t* surface, const shader_t* shader )
{
	if ( r_nocull->integer ) {
		return qfalse;
	}

	if ( *surface == SF_GRID ) {
		return R_CullGrid( (const srfGridMesh_t*)surface );
	}

	if ( *surface == SF_TRIANGLES ) {
		return R_CullTriSurf( (const srfTriangles_t*)surface );
	}

	if ( *surface != SF_FACE ) {
		return qfalse;
	}

	if ( shader->cullType == CT_TWO_SIDED ) {
		return qfalse;
	}

	const srfSurfaceFace_t* face = (const srfSurfaceFace_t*)surface;
	float d = DotProduct( tr.orient.viewOrigin, face->plane.normal );

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here 
	if ( shader->cullType == CT_FRONT_SIDED ) {
		if ( d < face->plane.dist - 8 ) {
			return qtrue;
		}
	} else {
		if ( d > face->plane.dist + 8 ) {
			return qtrue;
		}
	}

	return qfalse;
}


///////////////////////////////////////////////////////////////


static qbool R_LightCullBounds( const dlight_t* dl, const vec3_t mins, const vec3_t maxs )
{
	if (dl->transformed[0] - dl->radius > maxs[0])
		return qtrue;
	if (dl->transformed[0] + dl->radius < mins[0])
		return qtrue;

	if (dl->transformed[1] - dl->radius > maxs[1])
		return qtrue;
	if (dl->transformed[1] + dl->radius < mins[1])
		return qtrue;

	if (dl->transformed[2] - dl->radius > maxs[2])
		return qtrue;
	if (dl->transformed[2] + dl->radius < mins[2])
		return qtrue;

	return qfalse;
}


static qbool R_LightCullFace( const srfSurfaceFace_t* face, const dlight_t* dl )
{
	const float d = DotProduct( dl->transformed, face->plane.normal ) - face->plane.dist;
	if ( (d < -dl->radius) || (d > dl->radius) )
		return qtrue;

	return qfalse;
}


static qbool R_LightCullSurface( const surfaceType_t* surface, const dlight_t* dl )
{
	switch (*surface) {
	case SF_FACE:
		return R_LightCullFace( (const srfSurfaceFace_t*)surface, dl );
	case SF_GRID: {
		const srfGridMesh_t* grid = (const srfGridMesh_t*)surface;
		return R_LightCullBounds( dl, grid->meshBounds[0], grid->meshBounds[1] );
		}
	case SF_TRIANGLES: {
		const srfTriangles_t* tris = (const srfTriangles_t*)surface;
		return R_LightCullBounds( dl, tris->bounds[0], tris->bounds[1] );
		}
	default:
		return qfalse;
	}
}


static void R_AddWorldSurface( msurface_t* surf )
{
	if ( surf->vcBSP == tr.viewCount )
		return;		// already checked during this BSP walk

	surf->vcBSP = tr.viewCount;

	// surfaces that don't ever draw anything are not considered visible
	if ( surf->shader->numStages == 0 && !surf->shader->isSky )
		return;

	if ( R_CullSurface( surf->data, surf->shader ) )
		return;

	surf->vcVisible = tr.viewCount;

	float radiusOverZ = 666.0f;
	if ( *surf->data == SF_GRID ) {
		const srfGridMesh_t* const grid = (const srfGridMesh_t*)surf->data;
		const float dist = Distance( grid->localOrigin, tr.refdef.vieworg );
		radiusOverZ = grid->meshRadius / max( dist, 0.001f );
	} else if ( *surf->data == SF_TRIANGLES ) {
		const srfTriangles_t* const triangles = (const srfTriangles_t*)surf->data;
		const float dist = Distance( triangles->localOrigin, tr.refdef.vieworg );
		radiusOverZ = triangles->radius / max( dist, 0.001f );
	}

	R_AddDrawSurf( surf->data, surf->shader, surf->staticGeoChunk, surf->zppFirstIndex, surf->zppIndexCount, radiusOverZ );
}


/*
=============================================================

	WORLD MODEL

=============================================================
*/


static void R_RecursiveWorldNode( mnode_t *node, int planeBits )
{

	do {
		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount)
			return;

		// if the bounding volume is completely outside the frustum, dump it

		if ( !r_nocull->integer ) {
			int r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

		}

		if (node->contents != CONTENTS_NODE)
			break;

		// recurse down the children, front side first
		R_RecursiveWorldNode( node->children[0], planeBits );

		// tail recurse
		node = node->children[1];

	} while ( 1 );

	// leaf node, so add mark surfaces
	tr.pc[RF_LEAFS]++;

	// add to z buffer bounds
	if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
		tr.viewParms.visBounds[0][0] = node->mins[0];
	}
	if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
		tr.viewParms.visBounds[0][1] = node->mins[1];
	}
	if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
		tr.viewParms.visBounds[0][2] = node->mins[2];
	}

	if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
		tr.viewParms.visBounds[1][0] = node->maxs[0];
	}
	if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
		tr.viewParms.visBounds[1][1] = node->maxs[1];
	}
	if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
		tr.viewParms.visBounds[1][2] = node->maxs[2];
	}

	// add the individual surfaces
	int c = node->nummarksurfaces;
	msurface_t** mark = node->firstmarksurface;
	while (c--) {
		// the surface may have already been added if it spans multiple leafs
		msurface_t* surf = *mark;
		R_AddWorldSurface( surf );
		mark++;
	}

}


///////////////////////////////////////////////////////////////


static void R_AddLitSurface( msurface_t* surf, const dlight_t* light )
{
	// since we're not worried about offscreen lights casting into the frustum (ATM !!!)
	// only add the "lit" version of this surface if it was already added to the view
	//if ( surf->viewCount != tr.viewCount )
	//	return;

	// surfaces that were faceculled will still have the current viewCount in vcBSP
	// because that's set to indicate that it's BEEN vis tested at all, to avoid
	// repeated vis tests, not whether it actually PASSED the vis test or not
	// only light surfaces that are GENUINELY visible, as opposed to merely in a visible LEAF
	if ( surf->vcVisible != tr.viewCount )
		return;

	if ( surf->shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) )
		return;

	// reject mirrors, portals, sky boxes, etc.
	if ( surf->shader->sort < SS_OPAQUE )
		return;

	// lighting a fog box' surface looks absolutely terrible
	if ( (surf->shader->contentFlags & CONTENTS_FOG) != 0 )
		return;

	if ( surf->lightCount == tr.lightCount )
		return; // already in the lit list (or already culled) for this light

	const int stageIndex = surf->shader->lightingStages[ST_DIFFUSE];
	if ( stageIndex < 0 || stageIndex >= surf->shader->numStages )
		return;

	const shaderStage_t* const stage = surf->shader->stages[stageIndex];
	const int srcBits = stage->stateBits & GLS_SRCBLEND_BITS;
	const int dstBits = stage->stateBits & GLS_DSTBLEND_BITS;

	// we can't use a texture that was used with such a blend mode
	// since the final color could look nothing like the texture itself
	if ( srcBits == GLS_SRCBLEND_ONE_MINUS_DST_COLOR ||
	     dstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR )
		return;

	surf->lightCount = tr.lightCount;

	if ( R_LightCullSurface( surf->data, light ) ) {
		tr.pc[RF_LIT_CULLS]++;
		return;
	}

	R_AddLitSurf( surf->data, surf->shader, surf->staticGeoChunk );
}


static void R_RecursiveLightNode( mnode_t* node )
{
	do {
		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount)
			return;

		if (node->contents != CONTENTS_NODE)
			break;

		qbool children[2];
		children[0] = children[1] = qfalse;

		float d = DotProduct( tr.light->origin, node->plane->normal ) - node->plane->dist;
		if ( d > -tr.light->radius ) {
			children[0] = qtrue;
		}
		if ( d < tr.light->radius ) {
			children[1] = qtrue;
		}

		if ( children[0] && children[1] ) {
			R_RecursiveLightNode( node->children[0] );
			node = node->children[1];
		}
		else if ( children[0] ) {
			node = node->children[0];
		}
		else if ( children[1] ) {
			node = node->children[1];
		}
		else {
			return;
		}

	} while ( 1 );

	tr.pc[RF_LIT_LEAFS]++;

	// add the individual surfaces
	int c = node->nummarksurfaces;
	msurface_t** mark = node->firstmarksurface;
	while (c--) {
		// the surface may have already been added if it spans multiple leafs
		msurface_t* surf = *mark;
		R_AddLitSurface( surf, tr.light );
		mark++;
	}
}


///////////////////////////////////////////////////////////////
// BRUSH MODELS


void R_AddBrushModelSurfaces( const trRefEntity_t* re )
{
	const model_t* model = R_GetModelByHandle( re->e.hModel );
	const bmodel_t* bmodel = model->bmodel;

	if ( R_CullLocalBox( bmodel->bounds ) == CULL_OUT )
		return;

	for ( int s = 0; s < bmodel->numSurfaces; ++s ) {
		R_AddWorldSurface( bmodel->firstSurface + s );
	}

	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.orient );

	for ( int i = 0; i < tr.refdef.num_dlights; ++i ) {
		dlight_t* dl = &tr.refdef.dlights[i];
		if (R_LightCullBounds( dl, bmodel->bounds[0], bmodel->bounds[1] ))
			continue;
		++tr.lightCount;
		tr.light = dl;
		for ( int s = 0; s < bmodel->numSurfaces; ++s ) {
			R_AddLitSurface( bmodel->firstSurface + s, dl );
		}
	}

}


///////////////////////////////////////////////////////////////


static mnode_t* R_PointInLeaf( const vec3_t p )
{
	if ( !tr.world ) {
		ri.Error( ERR_DROP, "R_PointInLeaf: no world" );
	}

	mnode_t* node = tr.world->nodes;
	while (node->contents == CONTENTS_NODE) {
		const cplane_t* plane = node->plane;
		float d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}

	return node;
}


static const byte* R_ClusterPVS( int cluster )
{
	if ( !tr.world || !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters )
		return tr.world->novis;
	return tr.world->vis + cluster * tr.world->clusterBytes;
}


qbool R_inPVS( const vec3_t p1, const vec3_t p2 )
{
	const mnode_t* leaf;

	leaf = R_PointInLeaf( p1 );
	const byte* vis = CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return qfalse;
	}
	return qtrue;
}


// mark the leaves and nodes that are in the PVS for the current cluster

static void R_MarkLeaves()
{
	mnode_t* leaf;
	int i, cluster;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if ( r_lockpvs->integer ) {
		return;
	}

	// current viewcluster
	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	tr.pc[RF_LEAF_CLUSTER] = cluster;
	tr.pc[RF_LEAF_AREA] = leaf->area;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again
	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified ) {
		return;
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 ) {
		for ( i = 0; i < tr.world->numnodes; ++i ) {
			if (tr.world->nodes[i].contents != CONTENTS_SOLID) {
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	const byte* vis = R_ClusterPVS( tr.viewCluster );

	for ( i = 0, leaf = tr.world->nodes; i < tr.world->numnodes; ++i, ++leaf ) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		// check general pvs
		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		// check for door connection
		if ( (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7)) ) ) {
			continue;		// not visible
		}

		mnode_t* parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}


void R_AddWorldSurfaces()
{
	if ( !r_drawworld->integer ) {
		return;
	}

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = ENTITYNUM_WORLD;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves();

	// add all the visible surfaces and regenerate the visible min/max
	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );
	R_RecursiveWorldNode( tr.world->nodes, 15 );

	if ( tr.refdef.num_dlights > MAX_DLIGHTS )
		tr.refdef.num_dlights = MAX_DLIGHTS;

	// "transform" all the dlights so that dl->transformed is actually populated
	// (even though HERE it's == dl->origin) so we can always use R_LightCullBounds
	// instead of having copypasted versions for both world and local cases
	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.viewParms.world );

	for ( int i = 0; i < tr.refdef.num_dlights; ++i ) {
		dlight_t* dl = &tr.refdef.dlights[i];
		dl->head = dl->tail = 0;
		if ( R_CullPointAndRadius( dl->origin, dl->radius ) == CULL_OUT ) {
			tr.pc[RF_LIGHT_CULL_OUT]++;
			continue;
		}
		tr.pc[RF_LIGHT_CULL_IN]++;
		++tr.lightCount;
		tr.light = dl;
		R_RecursiveLightNode( tr.world->nodes );
	}
}
