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
// cmodel.c -- model loading

#include "cm_local.h"


// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define	BOX_BRUSHES		1
#define	BOX_SIDES		6
#define	BOX_LEAFS		2
#define	BOX_PLANES		12


clipMap_t cm;
int c_traces, c_brush_traces, c_patch_traces, c_pointcontents;

const byte* cmod_base;


#ifndef BSPC
cvar_t* cm_noAreas;
cvar_t* cm_noCurves;
cvar_t* cm_playerCurveClip;
#endif


// set up the planes and nodes so that the six floats of a bounding box
// can just be stored out and get a proper clipping hull structure.

static cmodel_t box_model;
static cplane_t* box_planes;
static cbrush_t* box_brush;

static void CM_InitBoxHull()
{
	box_planes = &cm.planes[cm.numPlanes];

	box_brush = &cm.brushes[cm.numBrushes];
	box_brush->numsides = 6;
	box_brush->sides = cm.brushsides + cm.numBrushSides;
	box_brush->contents = CONTENTS_BODY;

	box_model.leaf.numLeafBrushes = 1;
	box_model.leaf.firstLeafBrush = cm.numLeafBrushes;
	cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

	cplane_t* p;
	for (int i = 0; i < 6; ++i)
	{
		int side = (i & 1);

		cbrushside_t* s = &cm.brushsides[cm.numBrushSides+i];
		s->plane = cm.planes + (cm.numPlanes+i*2+side);
		s->surfaceFlags = 0;

		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;

		SetPlaneSignbits( p );
	}
}


/*
===============================================================================

					MAP LOADING

===============================================================================
*/


static void CMod_LoadShaders( const lump_t* l )
{
	const dshader_t* in = (const dshader_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadShaders: funny lump size");

	cm.numShaders = l->filelen / sizeof(*in);
	if (cm.numShaders < 1)
		Com_Error(ERR_DROP, "Map has no shaders");

	cm.shaders = H_New<dshader_t>( cm.numShaders, h_high );

	Com_Memcpy( cm.shaders, in, cm.numShaders * sizeof( *cm.shaders ) );

	dshader_t* out = cm.shaders;
	for (int i = 0; i < cm.numShaders; ++i, ++out)
	{
		out->contentFlags = LittleLong( out->contentFlags );
		out->surfaceFlags = LittleLong( out->surfaceFlags );
	}
}


static void CMod_LoadSubmodels( const lump_t* l )
{
	const dmodel_t* in = (const dmodel_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size");

	cm.numSubModels = l->filelen / sizeof(*in);
	if (cm.numSubModels < 1)
		Com_Error(ERR_DROP, "Map has no models");
	if (cm.numSubModels > MAX_SUBMODELS)
		Com_Error(ERR_DROP, "MAX_SUBMODELS exceeded");

	cm.cmodels = H_New<cmodel_t>( cm.numSubModels, h_high );

	int j;
	int* indexes;

	for (int i = 0; i < cm.numSubModels; ++i, ++in)
	{
		cmodel_t* out = &cm.cmodels[i];

		// spread the mins / maxs by a unit
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}

		if (i == 0)
			continue; // world model doesn't need other info

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numLeafBrushes = LittleLong( in->numBrushes );
		indexes = H_New<int>( out->leaf.numLeafBrushes, h_high );
		out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
		for ( j = 0 ; j < out->leaf.numLeafBrushes ; j++ ) {
			indexes[j] = LittleLong( in->firstBrush ) + j;
		}

		out->leaf.numLeafSurfaces = LittleLong( in->numSurfaces );
		indexes = H_New<int>( out->leaf.numLeafSurfaces, h_high );
		out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
		for ( j = 0 ; j < out->leaf.numLeafSurfaces ; j++ ) {
			indexes[j] = LittleLong( in->firstSurface ) + j;
		}
	}
}


static void CMod_LoadNodes( const lump_t* l )
{
	const dnode_t* in = (const dnode_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size");

	cm.numNodes = l->filelen / sizeof(*in);
	if (cm.numNodes < 1)
		Com_Error(ERR_DROP, "Map has no nodes");

	cm.nodes = H_New<cNode_t>( cm.numNodes, h_high );

	cNode_t* out = cm.nodes;
	for (int i = 0; i < cm.numNodes; ++i, ++in, ++out)
	{
		out->plane = cm.planes + LittleLong( in->planeNum );
		out->children[0] = LittleLong( in->children[0] );
		out->children[1] = LittleLong( in->children[1] );
	}
}


static void CM_BoundBrush( cbrush_t* b )
{
	b->bounds[0][0] = -b->sides[0].plane->dist;
	b->bounds[1][0] = b->sides[1].plane->dist;

	b->bounds[0][1] = -b->sides[2].plane->dist;
	b->bounds[1][1] = b->sides[3].plane->dist;

	b->bounds[0][2] = -b->sides[4].plane->dist;
	b->bounds[1][2] = b->sides[5].plane->dist;
}


static void CMod_LoadBrushes( const lump_t* l )
{
	const dbrush_t* in = (const dbrush_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadBrushes: funny lump size");

	cm.numBrushes = l->filelen / sizeof(*in);

	cm.brushes = H_New<cbrush_t>( BOX_BRUSHES + cm.numBrushes, h_high );

	cbrush_t* out = cm.brushes;
	for (int i = 0; i < cm.numBrushes; ++i, ++in, ++out)
	{
		out->sides = cm.brushsides + LittleLong(in->firstSide);
		out->numsides = LittleLong(in->numSides);

		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders )
			Com_Error( ERR_DROP, "CMod_LoadBrushes: bad shaderNum: %i", out->shaderNum );

		out->contents = cm.shaders[out->shaderNum].contentFlags;
		CM_BoundBrush( out );
	}
}


static void CMod_LoadLeafs( const lump_t* l )
{
	const dleaf_t* in = (const dleaf_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size");

	cm.numLeafs = l->filelen / sizeof(*in);
	if (cm.numLeafs < 1)
		Com_Error(ERR_DROP, "Map has no leafs");

	cm.leafs = H_New<cLeaf_t>( BOX_LEAFS + cm.numLeafs, h_high );

	cLeaf_t* out = cm.leafs;
	for (int i = 0; i < cm.numLeafs; ++i, ++in, ++out)
	{
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		out->firstLeafBrush = LittleLong( in->firstLeafBrush );
		out->numLeafBrushes = LittleLong( in->numLeafBrushes );
		out->firstLeafSurface = LittleLong( in->firstLeafSurface );
		out->numLeafSurfaces = LittleLong( in->numLeafSurfaces );

		if (out->cluster >= cm.numClusters)
			cm.numClusters = out->cluster + 1;
		if (out->area >= cm.numAreas)
			cm.numAreas = out->area + 1;
	}

	cm.areas = H_New<cArea_t>( cm.numAreas, h_high );
	cm.areaPortals = H_New<int>( cm.numAreas * cm.numAreas, h_high );
}


static void CMod_LoadPlanes( const lump_t* l )
{
	const dplane_t* in = (const dplane_t*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size");

	cm.numPlanes = l->filelen / sizeof(*in);
	if (cm.numPlanes < 1)
		Com_Error(ERR_DROP, "Map has no planes");

	cm.planes = H_New<cplane_t>( BOX_PLANES + cm.numPlanes, h_high );

	cplane_t* out = cm.planes;
	for (int i = 0; i < cm.numPlanes; ++i, ++in, ++out)
	{
		out->signbits = 0;
		for (int j = 0; j < 3; ++j)
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if (out->normal[j] < 0)
				out->signbits |= (1 << j);
		}
		out->dist = LittleFloat( in->dist );
		out->type = PlaneTypeForNormal( out->normal );
	}
}


static void CMod_LoadLeafBrushes( const lump_t* l )
{
	const int* in = (const int*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadLeafBrushes: funny lump size");

	cm.numLeafBrushes = l->filelen / sizeof(*in);

	cm.leafbrushes = H_New<int>( BOX_BRUSHES + cm.numLeafBrushes, h_high );

	int* out = cm.leafbrushes;
	for (int i = 0; i < cm.numLeafBrushes; ++i, ++in, ++out)
	{
		*out = LittleLong( *in );
	}
}


static void CMod_LoadLeafSurfaces( const lump_t* l )
{
	const int* in = (const int*)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafSurfaces: funny lump size");

	cm.numLeafSurfaces = l->filelen / sizeof(*in);

	cm.leafsurfaces = H_New<int>( cm.numLeafSurfaces, h_high );

	int* out = cm.leafsurfaces;
	for (int i = 0; i < cm.numLeafSurfaces; ++i, ++in, ++out)
	{
		*out = LittleLong( *in );
	}
}


static void CMod_LoadBrushSides( const lump_t* l )
{
	const dbrushside_t* in = (const dbrushside_t*)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) )
		Com_Error (ERR_DROP, "CMod_LoadBrushSides: funny lump size");

	cm.numBrushSides = l->filelen / sizeof(*in);

	cm.brushsides = H_New<cbrushside_t>( BOX_SIDES + cm.numBrushSides, h_high );

	cbrushside_t* out = cm.brushsides;
	for (int i = 0; i < cm.numBrushSides; ++i, ++in, ++out)
	{
		out->plane = &cm.planes[ LittleLong( in->planeNum ) ];
		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders )
			Com_Error( ERR_DROP, "CMod_LoadBrushSides: bad shaderNum: %i", out->shaderNum );
		out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
	}
}


static void CMod_LoadEntityString( const lump_t* l )
{
	cm.entityString = H_New<char>( l->filelen, h_high );
	cm.numEntityChars = l->filelen;
	Com_Memcpy( cm.entityString, cmod_base + l->fileofs, l->filelen );
}


static void CMod_LoadVisibility( const lump_t* l )
{
	const int VIS_HEADER = 8;

	if ( !l->filelen ) {
		cm.clusterBytes = ( cm.numClusters + 31 ) & ~31;
		cm.visibility = H_New<byte>( cm.clusterBytes, h_high );
		Com_Memset( cm.visibility, 255, cm.clusterBytes );
		return;
	}

	const byte* buf = (const byte*)(cmod_base + l->fileofs);

	cm.vised = qtrue;
	cm.visibility = H_New<byte>( l->filelen, h_high );
	cm.numClusters = LittleLong( ((const int*)buf)[0] );
	cm.clusterBytes = LittleLong( ((const int*)buf)[1] );
	Com_Memcpy( cm.visibility, buf + VIS_HEADER, l->filelen - VIS_HEADER );
}


static void CMod_LoadPatches( const lump_t* surfs, const lump_t* verts )
{
	const int MAX_PATCH_VERTS = 1024;
	vec3_t points[MAX_PATCH_VERTS];

	const dsurface_t* in = (const dsurface_t*)(cmod_base + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadPatches: funny lump size");
	cm.numSurfaces = surfs->filelen / sizeof(*in);
	cm.surfaces = H_New<cPatch_t*>( cm.numSurfaces, h_high );

	const drawVert_t* dvBase = (const drawVert_t*)(cmod_base + verts->fileofs);
	if (verts->filelen % sizeof(*dvBase))
		Com_Error (ERR_DROP, "CMod_LoadPatches: funny lump size");

	// scan through all the surfaces, but only load patches, not planar faces
	for (int i = 0; i < cm.numSurfaces; ++i, ++in)
	{
		if ( LittleLong( in->surfaceType ) != MST_PATCH )
			continue;		// ignore other surfaces

		// FIXME: check for non-colliding patches

		cm.surfaces[i] = H_New<cPatch_t>( h_high );

		// load the full drawverts onto the stack
		int w = LittleLong( in->patchWidth );
		int h = LittleLong( in->patchHeight );
		int c = w * h;
		if ( c > MAX_PATCH_VERTS )
			Com_Error( ERR_DROP, "CMod_LoadPatches: exceeded MAX_PATCH_VERTS" );

		const drawVert_t* dv = dvBase + LittleLong( in->firstVert );
		for (int j = 0; j < c; ++j, ++dv)
		{
			points[j][0] = LittleFloat( dv->xyz[0] );
			points[j][1] = LittleFloat( dv->xyz[1] );
			points[j][2] = LittleFloat( dv->xyz[2] );
		}

		int shaderNum = LittleLong( in->shaderNum );
		cm.surfaces[i]->contents = cm.shaders[shaderNum].contentFlags;
		cm.surfaces[i]->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
		// create the internal facet structure
		cm.surfaces[i]->pc = CM_GeneratePatchCollide( w, h, points );
	}
}


void CM_ClearMap()
{
	Com_Memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();
}


// loads in the map and all submodels

void CM_LoadMap( const char* name, qbool clientload, unsigned* checksum )
{
	if ( !name || !name[0] )
		Com_Error( ERR_DROP, "CM_LoadMap: NULL name" );

	Com_DPrintf( "CM_LoadMap( %s, %i )\n", name, clientload );

	static unsigned last_checksum;
	if ( !strcmp( cm.name, name ) && clientload ) {
		*checksum = last_checksum;
		return;
	}

	CM_ClearMap();

	int length;
	byte* buf = 0;

#ifndef BSPC
	cm_noAreas = Cvar_Get("cm_noAreas", "0", CVAR_CHEAT);
	cm_noCurves = Cvar_Get("cm_noCurves", "0", CVAR_CHEAT);
	cm_playerCurveClip = Cvar_Get("cm_playerCurveClip", "1", CVAR_CHEAT);
	length = FS_ReadFile( name, (void **)&buf );
#else
	length = LoadQuakeFile((quakefile_t *) name, (void **)&buf);
#endif

	if ( !buf )
		Com_Error(ERR_DROP, "Couldn't load %s", name);

	last_checksum = LittleLong( Com_BlockChecksum( buf, length ) );
	*checksum = last_checksum;

	dheader_t header = *(dheader_t*)buf;
	for (int i = 0; i < sizeof(dheader_t) / 4; ++i)
		((int*)&header)[i] = LittleLong( ((int*)&header)[i] );

	if ( header.version != BSP_VERSION ) {
		Com_Error( ERR_DROP, "CM_LoadMap: %s has wrong version number (%i should be %i)",
		name, header.version, BSP_VERSION );
	}

	cmod_base = buf;

	CMod_LoadShaders( &header.lumps[LUMP_SHADERS] );
	CMod_LoadLeafs( &header.lumps[LUMP_LEAFS] );
	CMod_LoadLeafBrushes( &header.lumps[LUMP_LEAFBRUSHES] );
	CMod_LoadLeafSurfaces( &header.lumps[LUMP_LEAFSURFACES] );
	CMod_LoadPlanes( &header.lumps[LUMP_PLANES] );
	CMod_LoadBrushSides( &header.lumps[LUMP_BRUSHSIDES] );
	CMod_LoadBrushes( &header.lumps[LUMP_BRUSHES] );
	CMod_LoadSubmodels( &header.lumps[LUMP_MODELS] );
	CMod_LoadNodes( &header.lumps[LUMP_NODES] );
	CMod_LoadEntityString( &header.lumps[LUMP_ENTITIES] );
	CMod_LoadVisibility( &header.lumps[LUMP_VISIBILITY] );
	CMod_LoadPatches( &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS] );

	// we are NOT freeing the file, because it is cached for the ref
	FS_FreeFile(buf);

	CM_InitBoxHull();

	CM_FloodAreaConnections();

	// allow this to be cached if it is loaded by the server
	if ( !clientload ) {
		Q_strncpyz( cm.name, name, sizeof( cm.name ) );
	}
}


//=======================================================================


const cmodel_t* CM_ClipHandleToModel( clipHandle_t handle )
{
	if ( handle < 0 )
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle );

	if ( handle < cm.numSubModels )
		return &cm.cmodels[handle];

	if ( handle == BOX_MODEL_HANDLE )
		return &box_model;

	if ( handle < MAX_SUBMODELS )
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i < %i < %i", cm.numSubModels, handle, MAX_SUBMODELS );

	Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle + MAX_SUBMODELS );

	return NULL;
}


clipHandle_t CM_InlineModel( int index )
{
	if ( index < 0 || index >= cm.numSubModels )
		Com_Error(ERR_DROP, "CM_InlineModel: bad number");
	return index;
}


int CM_NumInlineModels( void )
{
	return cm.numSubModels;
}


const char* CM_EntityString()
{
	return cm.entityString;
}


int CM_LeafCluster( int leafnum )
{
	if (leafnum < 0 || leafnum >= cm.numLeafs)
		Com_Error(ERR_DROP, "CM_LeafCluster: bad number");
	return cm.leafs[leafnum].cluster;
}


int CM_LeafArea( int leafnum )
{
	if (leafnum < 0 || leafnum >= cm.numLeafs)
		Com_Error(ERR_DROP, "CM_LeafArea: bad number");
	return cm.leafs[leafnum].area;
}


//=======================================================================


/*
To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though. (that's hardly "totally uniform" then is it? :P)
*/

clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule )
{
	VectorCopy( mins, box_model.mins );
	VectorCopy( maxs, box_model.maxs );

	if ( capsule )
		return CAPSULE_MODEL_HANDLE;

	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );

	return BOX_MODEL_HANDLE;
}


void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs )
{
	const cmodel_t* cmod = CM_ClipHandleToModel( model );
	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}

