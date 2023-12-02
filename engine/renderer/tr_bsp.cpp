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

/*
Loads and prepares a map file for scene rendering.
A single entry point: void RE_LoadWorldMap( const char* name );
*/

static world_t s_worldData;
static byte* fileBase;

static const int LMVirtPageSize	= 128;
static const int LMBorderSize	= 1;	// bilinear filtering on, mip-mapping and anisotropic filtering off
static const int LMPhysPageSize	= 128 + LMBorderSize * 2;

static vec2_t lightmapBiases[MAX_LIGHTMAPS];
static float lightmapScale[2];
static int lightmapsPerAtlas;


static void R_ColorShiftLightingBytesRGB( const byte* in, byte* out )
{
	// scale based on brightness
	const int scale16 = (int)( r_mapBrightness->value * 65536.0f );
	int r = ( (int)in[0] * scale16 ) >> 16;
	int g = ( (int)in[1] * scale16 ) >> 16;
	int b = ( (int)in[2] * scale16 ) >> 16;

	// desaturate by moving the channels towards the grey "midpoint"
	// credit for the following snippet goes to Jakub 'kubaxvx' Matraszek
	const int grey = (r + g + b) / 3; // could use the Rec. 601 or 709 coefficients instead
	const int greyscale16 = (int)( r_lightmapGreyscale->value * 65536.0f );
	r = ( ( r << 16 ) + greyscale16 * ( grey - r ) ) >> 16;
	g = ( ( g << 16 ) + greyscale16 * ( grey - g ) ) >> 16;
	b = ( ( b << 16 ) + greyscale16 * ( grey - b ) ) >> 16;
	
	// normalize by color instead of saturating to white
	if ( ( r | g | b ) > 255 ) {
		int max;
		max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
}


void R_ColorShiftLightingBytes( const byte in[4], byte out[4] )
{
	R_ColorShiftLightingBytesRGB( in, out );
	out[3] = in[3];
}


static void R_ComputeAtlasSize( int* sizeX, int* sizeY, int tileSize, int tileCount, int maxTextureSize )
{
	int h = 128;

	while ( h <= maxTextureSize ) {
		int w = 128;

		while ( w <= maxTextureSize ) {
			const int countX = w / tileSize;
			const int countY = h / tileSize;
			const int count = countX * countY;

			if ( count >= tileCount ) {
				*sizeX = w;
				*sizeY = h;
				return;
			}

			w *= 2;
		}

		h *= 2;
	}

	*sizeX = maxTextureSize;
	*sizeY = maxTextureSize;
}


static void R_LerpPixels( byte* dst, const byte* src1, const byte* src2 )
{
	dst[0] = (byte)(((uint16_t)src1[0] + (uint16_t)src2[0]) / 2);
	dst[1] = (byte)(((uint16_t)src1[1] + (uint16_t)src2[1]) / 2);
	dst[2] = (byte)(((uint16_t)src1[2] + (uint16_t)src2[2]) / 2);
	dst[3] = (byte)(((uint16_t)src1[3] + (uint16_t)src2[3]) / 2);
}


static void R_FillBorderTexels( byte* image )
{
	const int stride = LMPhysPageSize * 4;

	const byte* const topSrc = image + 4 + stride;
	byte* const topDst = image + 4;
	memcpy( topDst, topSrc, LMVirtPageSize * 4 );

	const byte* const bottomSrc = image + 4 + (LMPhysPageSize - 2) * stride;
	byte* const bottomDst = image + 4 + (LMPhysPageSize - 1) * stride;
	memcpy( bottomDst, bottomSrc, LMVirtPageSize * 4 );

	const uint32_t* leftSrc = (const uint32_t*)(image + 4 + stride);
	uint32_t* leftDst = (uint32_t*)(image + stride);
	const uint32_t* rightSrc = (const uint32_t*)(image + 2 * stride - 8);
	uint32_t* rightDst = (uint32_t*)(image + 2 * stride - 4);
	for ( int i = 0; i < LMVirtPageSize; ++i ) {
		*leftDst = *leftSrc;
		*rightDst = *rightSrc;
		leftSrc += LMPhysPageSize;
		leftDst += LMPhysPageSize;
		rightSrc += LMPhysPageSize;
		rightDst += LMPhysPageSize;
	}

	R_LerpPixels( image, image + 4, image + stride );
	R_LerpPixels( image + stride - 4, image + stride - 8, image + 2 * stride - 4 );
	R_LerpPixels( image + (LMPhysPageSize - 1) * stride, image + (LMPhysPageSize - 2) * stride, image + (LMPhysPageSize - 1) * stride + 4 );
	R_LerpPixels( image + LMPhysPageSize * stride - 4, image + LMPhysPageSize * stride - 8, image + (LMPhysPageSize - 1) * stride - 4 );
}


static void R_LoadLightmaps( const lump_t* l )
{
	// set this now as the default to avoid divisions by 0
	lightmapsPerAtlas = 1;

	const int fileBytes = l->filelen;
	if ( !fileBytes )
		return;

	byte* p = fileBase + l->fileofs;

	int numFileLightmaps = fileBytes / (LMVirtPageSize * LMVirtPageSize * 3);
	if ( numFileLightmaps >= MAX_LIGHTMAPS ) {
		ri.Printf( PRINT_WARNING, "WARNING: number of lightmaps > MAX_LIGHTMAPS\n" );
		numFileLightmaps = MAX_LIGHTMAPS;
	}

	int sizeX;
	int sizeY;
	R_ComputeAtlasSize( &sizeX, &sizeY, LMPhysPageSize, numFileLightmaps, glInfo.maxTextureSize );

	static byte image[LMPhysPageSize * LMPhysPageSize * 4];
	const float scaleX = (float)LMVirtPageSize / (float)sizeX;
	const float scaleY = (float)LMVirtPageSize / (float)sizeY;
	const int numTilesPerAtlas = (sizeX / LMPhysPageSize) * (sizeY / LMPhysPageSize);
	const int numAtlases = (numFileLightmaps + numTilesPerAtlas - 1) / numTilesPerAtlas;
	const int countX = sizeX / LMPhysPageSize;

	int i = 0; // lightmapNum
	for ( int a = 0; a < numAtlases; ++a ) {
		tr.lightmaps[a] = R_CreateImage( va("*lightmapatlas%i", a), NULL, sizeX, sizeY, TF_RGBA8, IMG_LMATLAS, TW_CLAMP_TO_EDGE );

		for ( int t = 0; t < numTilesPerAtlas && i < numFileLightmaps; ++t ) {
			for ( int y = 0; y < LMVirtPageSize; ++y ) {
				const byte* s = p + y * LMVirtPageSize * 3;
				byte* d = image + (LMBorderSize + LMPhysPageSize * (LMBorderSize + y)) * 4;

				for ( int x = 0; x < LMVirtPageSize; ++x ) {
					R_ColorShiftLightingBytesRGB(s, d);
					d[3] = 255;
					d += 4;
					s += 3;
				}
			}

			R_FillBorderTexels( image );

			const int offX = (t % countX) * LMPhysPageSize;
			const int offY = (t / countX) * LMPhysPageSize;
			R_UploadLightmapTile( tr.lightmaps[a], image, offX, offY, LMPhysPageSize, LMPhysPageSize );

			lightmapBiases[i][0] = (float)(offX + LMBorderSize) / (float)sizeX;
			lightmapBiases[i][1] = (float)(offY + LMBorderSize) / (float)sizeY;

			p += LMVirtPageSize * LMVirtPageSize * 3;
			++i;
		}
	}

	tr.numLightmaps = numAtlases;
	lightmapsPerAtlas = numTilesPerAtlas;
	lightmapScale[0] = scaleX;
	lightmapScale[1] = scaleY;
}


static void R_GetLightmapTransform( int* number, vec2_t scale, vec2_t bias )
{
	const int i = *number;

	if ( i >= 0 ) {
		scale[0] = lightmapScale[0];
		scale[1] = lightmapScale[1];
		bias[0] = lightmapBiases[i][0];
		bias[1] = lightmapBiases[i][1];
		*number /= lightmapsPerAtlas;
	} else {
		scale[0] = 1.0f;
		scale[1] = 1.0f;
		bias[0] = 0.0f;
		bias[1] = 0.0f;
		if ( *number <= LIGHTMAP_2D || *number >= tr.numLightmaps )
			*number = LIGHTMAP_BROKEN;
	}
}


static void R_SaveLightmapTransform( shader_t* shader, const vec2_t scale, const vec2_t bias )
{
	shader->lmScale[0] = scale[0];
	shader->lmScale[1] = scale[1];
	shader->lmBias[0] = bias[0];
	shader->lmBias[1] = bias[1];
}


// called by the clipmodel subsystem so we can share the PVS

void RE_SetWorldVisData( const byte* vis )
{
	tr.externalVisData = vis;
}


static void R_LoadVisibility( const lump_t* l )
{
	int len = ( s_worldData.numClusters + 63 ) & ~63;
	s_worldData.novis = RI_New<byte>( len );
	Com_Memset( s_worldData.novis, 0xff, len );

	len = l->filelen;
	if ( !len )
		return;

	byte* p = fileBase + l->fileofs;

	s_worldData.numClusters = LittleLong( ((int *)p)[0] );
	s_worldData.clusterBytes = LittleLong( ((int *)p)[1] );

	// CM_Load should have given us the vis data to share, so
	// we don't need to allocate another copy
	if ( tr.externalVisData ) {
		s_worldData.vis = tr.externalVisData;
	} else {
		byte* dest = RI_New<byte>( len - 8 );
		Com_Memcpy( dest, p + 8, len - 8 );
		s_worldData.vis = dest;
	}
}


///////////////////////////////////////////////////////////////


static shader_t* ShaderForShaderNum( int shaderNum, int lightmapNum )
{
	shaderNum = LittleLong( shaderNum );
	if ( shaderNum < 0 || shaderNum >= s_worldData.numShaders )
		ri.Error( ERR_DROP, "ShaderForShaderNum: bad num %i", shaderNum );

	const dshader_t* dsh = &s_worldData.shaders[ shaderNum ];

	int flags = FINDSHADER_MIPRAWIMAGE_BIT;
	if ( r_vertexLight->integer != 0 || lightmapNum == LIGHTMAP_BY_VERTEX )
		flags |= FINDSHADER_VERTEXLIGHT_BIT;
	shader_t* shader = R_FindShader( dsh->shader, lightmapNum, flags );

	if ( r_singleShader->integer && (shader->sort != SS_ENVIRONMENT) )
		return tr.defaultShader;

	// if the shader had errors, just use default shader
	if ( shader->defaultShader )
		return tr.defaultShader;

	return shader;
}


template <class T> T* AllocSurface( int numVerts, int numIndexes )
{
	T* surf = (T*)RI_New<byte>
		( sizeof(T) + (numVerts * sizeof(*surf->verts)) + (numIndexes * sizeof(*surf->indexes)) );
	surf->verts = (srfVert_t*)(surf + 1);
	surf->indexes = (int*)(surf->verts + numVerts);
	surf->numVerts = numVerts;
	surf->numIndexes = numIndexes;
	return surf;
}


static void ParseFace( const dsurface_t* ds, const drawVert_t* verts, msurface_t* surf, const int* indexes )
{
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	int lightmapNum = LittleLong( ds->lightmapNum );
	vec2_t lmScale, lmBias;
	R_GetLightmapTransform( &lightmapNum, lmScale, lmBias );
	shader_t* const shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );
	surf->shader = shader;
	R_SaveLightmapTransform( shader, lmScale, lmBias );

	int numVerts = LittleLong( ds->numVerts );
	if (numVerts > MAX_FACE_POINTS) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_FACE_POINTS exceeded: %i\n", numVerts );
		numVerts = MAX_FACE_POINTS;
		surf->shader = tr.defaultShader;
	}

	const int numIndexes = LittleLong( ds->numIndexes );

	srfSurfaceFace_t* cv = AllocSurface<srfSurfaceFace_t>( numVerts, numIndexes );
	cv->surfaceType = SF_FACE;

	surf->data = (surfaceType_t*)cv;

	verts += LittleLong( ds->firstVert );
	for ( int i = 0 ; i < numVerts ; i++ ) {
		for ( int j = 0 ; j < 3 ; j++ ) {
			cv->verts[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
		}
		for ( int j = 0 ; j < 2 ; j++ ) {
			cv->verts[i].st[j] = LittleFloat( verts[i].st[j] );
			cv->verts[i].st2[j] = lmBias[j] + lmScale[j] * LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color, cv->verts[i].rgba );
	}

	indexes += LittleLong( ds->firstIndex );
	for ( int i = 0 ; i < numIndexes ; i++ ) {
		cv->indexes[i] = LittleLong( indexes[i] );
	}

	// take the plane information from the lightmap vector
	for ( int i = 0 ; i < 3 ; i++ ) {
		cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
	cv->plane.dist = DotProduct( cv->verts[0].xyz, cv->plane.normal );
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	if ( numVerts >= 3 && numIndexes >= 3 && VectorLengthSquared(cv->plane.normal) < 0.01f ) {
		// this face's normal is messed up, so we copy something new
		vec3_t v1, v2, normal;
		const int i0 = cv->indexes[0];
		const int i1 = cv->indexes[1];
		const int i2 = cv->indexes[2];
		VectorSubtract(cv->verts[i2].xyz, cv->verts[i0].xyz, v1);
		VectorSubtract(cv->verts[i1].xyz, cv->verts[i0].xyz, v2);
		CrossProduct(v1, v2, normal);
		VectorNormalize(normal);

		for ( int i = 0; i < numVerts; ++i ) {
			VectorCopy(normal, cv->verts[i].normal);
		}
	} else {
		// copy the normal we already have
		for ( int i = 0; i < numVerts; ++i ) {
			VectorCopy(cv->plane.normal, cv->verts[i].normal);
		}
	}

	vec3_t mins, maxs;
	ClearBounds(mins, maxs);
	for ( int i = 0 ; i < numVerts ; i++ ) {
		AddPointToBounds(cv->verts[i].xyz, mins, maxs);
	}
	VectorAdd(mins, maxs, cv->localOrigin);
	VectorScale(cv->localOrigin, 0.5f, cv->localOrigin);
}


static void ParseMesh( const dsurface_t* ds, const drawVert_t* verts, msurface_t* surf )
{
	static surfaceType_t skipData = SF_SKIP;
	drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];

	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	int lightmapNum = LittleLong(ds->lightmapNum);
	vec2_t lmScale, lmBias;
	R_GetLightmapTransform( &lightmapNum, lmScale, lmBias );
	shader_t* const shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );
	surf->shader = shader;
	R_SaveLightmapTransform( shader, lmScale, lmBias );

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( s_worldData.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	int w = LittleLong( ds->patchWidth );
	int h = LittleLong( ds->patchHeight );
	int numPoints = w * h;

	verts += LittleLong( ds->firstVert );

	for (int i = 0; i < numPoints; ++i) {
		for (int j = 0; j < 3; ++j) {
			points[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			points[i].normal[j] = LittleFloat( verts[i].normal[j] );
		}
		points[i].st[0] = LittleFloat( verts[i].st[0] );
		points[i].lightmap[0] = lmBias[0] + lmScale[0] * LittleFloat( verts[i].lightmap[0] );
		points[i].st[1] = LittleFloat( verts[i].st[1] );
		points[i].lightmap[1] = lmBias[1] + lmScale[1] * LittleFloat( verts[i].lightmap[1] );
		R_ColorShiftLightingBytes( verts[i].color, points[i].color );
	}

	// pre-tesseleate
	srfGridMesh_t* grid = R_SubdividePatchToGrid( w, h, points );
	surf->data = (surfaceType_t*)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking
	vec3_t bounds[2], v;
	for (int i = 0; i < 3; ++i) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, v );
	grid->lodRadius = VectorLength( v );
}


static void ParseTriSurf( const dsurface_t* ds, const drawVert_t* verts, msurface_t* surf, const int* indexes )
{
	int i, j;

	shader_t* const shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	surf->shader = shader;

	int lightmapNum = LittleLong( ds->lightmapNum );
	vec2_t lmScale, lmBias;
	R_GetLightmapTransform( &lightmapNum, lmScale, lmBias );
	R_SaveLightmapTransform( shader, lmScale, lmBias );

	int numVerts = LittleLong( ds->numVerts );
	int numIndexes = LittleLong( ds->numIndexes );

	srfTriangles_t* tri = AllocSurface<srfTriangles_t>( numVerts, numIndexes );
	tri->surfaceType = SF_TRIANGLES;

	surf->data = (surfaceType_t*)tri;

	ClearBounds( tri->bounds[0], tri->bounds[1] );
	verts += LittleLong( ds->firstVert );
	for ( i = 0 ; i < numVerts ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tri->verts[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
			tri->verts[i].normal[j] = LittleFloat( verts[i].normal[j] );
		}
		AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );
			tri->verts[i].st2[j] = lmBias[j] + lmScale[j] * LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color, tri->verts[i].rgba );
	}

	VectorAdd( tri->bounds[0], tri->bounds[1], tri->localOrigin );
	VectorScale( tri->localOrigin, 0.5f, tri->localOrigin );

	indexes += LittleLong( ds->firstIndex );
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = LittleLong( indexes[i] );
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			ri.Error( ERR_DROP, "Bad index in triangle surface" );
		}
	}
}


static void ParseFlare( const dsurface_t* ds, msurface_t* surf )
{
	static surfaceType_t flare = SF_FLARE;
	surf->fogIndex = LittleLong( ds->fogNum ) + 1;
	surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );
	surf->data = &flare;
}


/*
=================
R_MergedWidthPoints

returns qtrue if there are grid points merged on a width edge
=================
*/
static int R_MergedWidthPoints(srfGridMesh_t *grid, int offset) {
	int i, j;

	for (i = 1; i < grid->width-1; i++) {
		for (j = i + 1; j < grid->width-1; j++) {
			if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_MergedHeightPoints

returns qtrue if there are grid points merged on a height edge
=================
*/
static int R_MergedHeightPoints(srfGridMesh_t *grid, int offset) {
	int i, j;

	for (i = 1; i < grid->height-1; i++) {
		for (j = i + 1; j < grid->height-1; j++) {
			if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
static void R_FixSharedVertexLodError_r( int start, srfGridMesh_t *grid1 ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfGridMesh_t *grid2;

	for ( j = start; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r ( start, grid2 );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}

/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
static void R_FixSharedVertexLodError( void ) {
	int i;
	srfGridMesh_t *grid1;

	for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
		//
		grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid1->surfaceType != SF_GRID )
			continue;
		//
		if ( grid1->lodFixed )
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r( i + 1, grid1);
	}
}


/*
===============
R_StitchPatches
===============
*/
static int R_StitchPatches( int grid1num, int grid2num ) {
	float *v1, *v2;
	srfGridMesh_t *grid1, *grid2;
	int k, l, m, n, offset1, offset2, row, column;

	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	grid2 = (srfGridMesh_t *) s_worldData.surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width-2; k += 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height-2; k += 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width-1; k > 1; k -= 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height-1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//ri.Printf( PRINT_ALL, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (surfaceType_t*)grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertice cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
static int R_TryStitchingPatch( int grid1num ) {
	int j, numstitches;
	srfGridMesh_t *grid1, *grid2;

	numstitches = 0;
	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	for ( j = 0; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		while (R_StitchPatches(grid1num, j))
		{
			numstitches++;
		}
	}
	return numstitches;
}

/*
===============
R_StitchAllPatches
===============
*/
static void R_StitchAllPatches( void ) {
	int i, stitched, numstitches;
	srfGridMesh_t *grid1;

	numstitches = 0;
	do
	{
		stitched = qfalse;
		for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
			//
			grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
			// if this surface is not a grid
			if ( grid1->surfaceType != SF_GRID )
				continue;
			//
			if ( grid1->lodStitched )
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch( i );
		}
	}
	while (stitched);
	ri.Printf( PRINT_ALL, "stitched %d LoD cracks\n", numstitches );
}


static void R_MovePatchSurfacesToHunk()
{
	for (int i = 0; i < s_worldData.numsurfaces; ++i) {
		srfGridMesh_t* grid = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		if ( grid->surfaceType != SF_GRID )
			continue;

		int size = (grid->width * grid->height - 1) * sizeof( drawVert_t ) + sizeof( *grid );
		srfGridMesh_t* hunkgrid = (srfGridMesh_t*)RI_New<byte>( size );
		Com_Memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = RI_New<float>( grid->width );
		Com_Memcpy( hunkgrid->widthLodError, grid->widthLodError, grid->width * 4 );

		hunkgrid->heightLodError = RI_New<float>( grid->height );
		Com_Memcpy( hunkgrid->heightLodError, grid->heightLodError, grid->height * 4 );

		R_FreeSurfaceGridMesh( grid );

		s_worldData.surfaces[i].data = (surfaceType_t*)hunkgrid;
	}
}


// !!! the CM code duplicates virtually all of these functions
// they really should be shared, especially since unpacking etc is so clunky

template <typename T> const T* ReadLump( const lump_t* lump, int* c )
{
	const T* p = (const T*)(fileBase + lump->fileofs);
	if (lump->filelen % sizeof(T))
		ri.Error( ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name );
	if (c)
		*c = lump->filelen / sizeof(T);
	return p;
}


static void R_LoadSurfaces( const lump_t* surfs, const lump_t* verts, const lump_t* indexLump )
{
	const dsurface_t* in = ReadLump<dsurface_t>( surfs, &s_worldData.numsurfaces );
	const drawVert_t* dv = ReadLump<drawVert_t>( verts, NULL );
	const int* indexes = ReadLump<int>( indexLump, NULL );

	msurface_t* out = RI_New<msurface_t>( s_worldData.numsurfaces );

	int numFaces = 0, numMeshes = 0, numTriSurfs = 0, numFlares = 0;

	s_worldData.surfaces = out;
	for (int i = 0 ; i < s_worldData.numsurfaces; ++i, ++in, ++out)
	{
		switch ( LittleLong( in->surfaceType ) ) {
		case MST_PATCH:
			ParseMesh( in, dv, out );
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv, out, indexes );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace( in, dv, out, indexes );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( in, out );
			numFlares++;
			break;
		default:
			ri.Error( ERR_DROP, "Bad surfaceType" );
		}
	}

#ifdef PATCH_STITCHING
	R_StitchAllPatches();
#endif

	R_FixSharedVertexLodError();

#ifdef PATCH_STITCHING
	R_MovePatchSurfacesToHunk();
#endif

	ri.Printf( PRINT_ALL, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n", 
		numFaces, numMeshes, numTriSurfs, numFlares );
}


static void R_LoadSubmodels( const lump_t* l )
{
	int count;
	const dmodel_t* in = ReadLump<dmodel_t>( l, &count );

	s_worldData.bmodels = RI_New<bmodel_t>( count );

	bmodel_t* out = s_worldData.bmodels;
	for (int i = 0; i < count; ++i, ++in, ++out) {
		model_t* model = R_AllocModel();
		assert( model ); // we should always have enough space for these

		model->type = MOD_BRUSH;
		model->bmodel = out;
		Com_sprintf( model->name, sizeof( model->name ), "*%d", i );

		for (int j = 0; j < 3; ++j) {
			out->bounds[0][j] = LittleFloat( in->mins[j] );
			out->bounds[1][j] = LittleFloat( in->maxs[j] );
		}

		out->firstSurface = s_worldData.surfaces + LittleLong( in->firstSurface );
		out->numSurfaces = LittleLong( in->numSurfaces );
	}
}


///////////////////////////////////////////////////////////////


static void R_SetParent( mnode_t* node, mnode_t* parent )
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	R_SetParent( node->children[0], node );
	R_SetParent( node->children[1], node );
}


static void R_LoadNodesAndLeafs( const lump_t* nodeLump, const lump_t* leafLump )
{
	int numNodes, numLeafs;
	const dnode_t* in = ReadLump<dnode_t>( nodeLump, &numNodes );
	const dleaf_t* inLeaf = ReadLump<dleaf_t>( leafLump, &numLeafs );

	s_worldData.numnodes = numNodes + numLeafs;
	s_worldData.nodes = RI_New<mnode_t>( s_worldData.numnodes );

	mnode_t* out = s_worldData.nodes;
	for (int i = 0; i < numNodes; ++i, ++in, ++out)
	{
		for (int j = 0; j < 3; ++j) {
			out->mins[j] = LittleLong( in->mins[j] );
			out->maxs[j] = LittleLong( in->maxs[j] );
		}

		out->contents = CONTENTS_NODE;	// differentiate from leafs

		int p = LittleLong( in->planeNum );
		out->plane = s_worldData.planes + p;

		p = LittleLong( in->children[0] );
		out->children[0] = s_worldData.nodes + ((p >= 0) ? p : numNodes + (-1 - p));
		p = LittleLong( in->children[1] );
		out->children[1] = s_worldData.nodes + ((p >= 0) ? p : numNodes + (-1 - p));
	}

	for (int i = 0; i < numLeafs; ++i, ++inLeaf, ++out)
	{
		for (int j = 0; j < 3; ++j) {
			out->mins[j] = LittleLong( inLeaf->mins[j] );
			out->maxs[j] = LittleLong( inLeaf->maxs[j] );
		}

		out->cluster = LittleLong( inLeaf->cluster );
		out->area = LittleLong( inLeaf->area );

		if ( out->cluster >= s_worldData.numClusters )
			s_worldData.numClusters = out->cluster + 1;

		out->firstmarksurface = s_worldData.marksurfaces + LittleLong( inLeaf->firstLeafSurface );
		out->nummarksurfaces = LittleLong( inLeaf->numLeafSurfaces );
	}

	// chain descendants
	R_SetParent( s_worldData.nodes, NULL );
}


///////////////////////////////////////////////////////////////


static void R_LoadShaders( const lump_t* l )
{
	const dshader_t* in = ReadLump<dshader_t>( l, &s_worldData.numShaders );

	s_worldData.shaders = RI_New<dshader_t>( s_worldData.numShaders );

	dshader_t* out = s_worldData.shaders;
	Com_Memcpy( out, in, s_worldData.numShaders * sizeof(*out) );

	for (int i = 0; i < s_worldData.numShaders; ++i) {
		out[i].surfaceFlags = LittleLong( out[i].surfaceFlags );
		out[i].contentFlags = LittleLong( out[i].contentFlags );
	}
}


static void R_LoadMarksurfaces( const lump_t* l )
{
	const int* in = ReadLump<int>( l, &s_worldData.nummarksurfaces );

	s_worldData.marksurfaces = RI_New<msurface_t*>( s_worldData.nummarksurfaces );

	for (int i = 0; i < s_worldData.nummarksurfaces; ++i)
		s_worldData.marksurfaces[i] = s_worldData.surfaces + LittleLong( in[i] );
}


static void R_LoadPlanes( const lump_t* l )
{
	const dplane_t* in = ReadLump<dplane_t>( l, &s_worldData.numplanes );

	s_worldData.planes = RI_New<cplane_t>( s_worldData.numplanes );

	cplane_t* out = s_worldData.planes;
	for (int i = 0; i < s_worldData.numplanes; ++i, ++in, ++out)
	{
		out->signbits = 0;
		for (int j = 0; j < 3; ++j) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if (out->normal[j] < 0) {
				out->signbits |= (1 << j);
			}
		}
		out->dist = LittleFloat( in->dist );
		out->type = PlaneTypeForNormal( out->normal );
	}
}


static unsigned ColorBytes4( float r, float g, float b, float a )
{
	unsigned i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;
	( (byte *)&i )[3] = a * 255;

	return i;
}


static void R_LoadFogs( const lump_t* l, const lump_t* brushesLump, const lump_t* sidesLump )
{
	const dfog_t* fogs = ReadLump<dfog_t>( l, &s_worldData.numfogs );

	// fog[0] is an empty but referenced element, so we have to alloc that even if the map has none
	// ideally, someone would fix all the callers to not just blindly deref s_worldData.fogs, but >:(
	++s_worldData.numfogs;
	s_worldData.fogs = RI_New<fog_t>( s_worldData.numfogs );
	if (s_worldData.numfogs == 1)
		return;

	fog_t* out = s_worldData.fogs + 1;

	int numBrushes, numSides;
	const dbrush_t* brushes = ReadLump<dbrush_t>( brushesLump, &numBrushes );
	const dbrushside_t* sides = ReadLump<dbrushside_t>( sidesLump, &numSides );

	for (int i = 1; i < s_worldData.numfogs; ++i, ++fogs, ++out)
	{
		out->originalBrushNumber = LittleLong( fogs->brushNum );

		if ( out->originalBrushNumber >= numBrushes )
			ri.Error( ERR_DROP, "fog brushNumber out of range" );

		const dbrush_t* brush = brushes + out->originalBrushNumber;

		int firstSide = LittleLong( brush->firstSide );
		if ( firstSide > numSides - 6 )
			ri.Error( ERR_DROP, "fog brush sideNumber out of range" );
		int sideNum = firstSide, planeNum;

		// brushes are always sorted with the axial sides first
		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[0][0] = -s_worldData.planes[ planeNum ].dist;
		++sideNum;

		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[1][0] = s_worldData.planes[ planeNum ].dist;
		++sideNum;

		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[0][1] = -s_worldData.planes[ planeNum ].dist;
		++sideNum;

		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[1][1] = s_worldData.planes[ planeNum ].dist;
		++sideNum;

		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[0][2] = -s_worldData.planes[ planeNum ].dist;
		++sideNum;

		planeNum = LittleLong( sides[ sideNum ].planeNum );
		out->bounds[1][2] = s_worldData.planes[ planeNum ].dist;
		++sideNum;

		// get information from the shader for fog parameters
		shader_t* shader = R_FindShader( fogs->shader, LIGHTMAP_NONE, FINDSHADER_MIPRAWIMAGE_BIT );

		out->parms = shader->fogParms;

		out->colorInt = ColorBytes4(
			shader->fogParms.color[0] * tr.identityLight,
			shader->fogParms.color[1] * tr.identityLight,
			shader->fogParms.color[2] * tr.identityLight,
			1.0 );

		out->tcScale = 1.0f / (8 * max( 1.0f, shader->fogParms.depthForOpaque ));

		// set the gradient vector
		sideNum = LittleLong( fogs->visibleSide );

		if ( sideNum == -1 ) {
			out->hasSurface = qfalse;
		} else {
			out->hasSurface = qtrue;
			planeNum = LittleLong( sides[ firstSide + sideNum ].planeNum );
			VectorSubtract( vec3_origin, s_worldData.planes[ planeNum ].normal, out->surface );
			out->surface[3] = -s_worldData.planes[ planeNum ].dist;
		}
	}
}


static void R_LoadLightGrid( const lump_t* l )
{
	int i;
	world_t* w = &s_worldData;

	w->lightGridInverseSize[0] = 1.0f / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0f / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0f / w->lightGridSize[2];

	float* wMins = w->bmodels[0].bounds[0];
	float* wMaxs = w->bmodels[0].bounds[1];

	vec3_t maxs;
	for (i = 0; i < 3; ++i) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceil( wMins[i] / w->lightGridSize[i] );
		maxs[i] = w->lightGridSize[i] * floor( wMaxs[i] / w->lightGridSize[i] );
		w->lightGridBounds[i] = (maxs[i] - w->lightGridOrigin[i])/w->lightGridSize[i] + 1;
	}

	int numGridPoints = w->lightGridBounds[0] * w->lightGridBounds[1] * w->lightGridBounds[2];

	if ( l->filelen != numGridPoints * 8 ) {
		ri.Printf( PRINT_WARNING, "WARNING: light grid mismatch\n" );
		w->lightGridData = NULL;
		return;
	}

	w->lightGridData = RI_New<byte>( l->filelen );
	Com_Memcpy( w->lightGridData, fileBase + l->fileofs, l->filelen );

	// deal with overbright bits
	for (i = 0; i < numGridPoints; ++i) {
		R_ColorShiftLightingBytes( &w->lightGridData[i*8], &w->lightGridData[i*8] );
		R_ColorShiftLightingBytes( &w->lightGridData[i*8+3], &w->lightGridData[i*8+3] );
	}
}


static void R_LoadEntities( const lump_t* l )
{
	world_t* w = &s_worldData;
	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	const char* p = (const char*)(fileBase + l->fileofs);

	// store for reference by the cgame
	w->entityString = RI_New<char>( l->filelen + 1 );
	strcpy( w->entityString, p );
	w->entityParsePoint = w->entityString;

	const char* token = COM_ParseExt( &p, qtrue );
	if (!*token || *token != '{')
		return;

	// only parse the world spawn
	while ( 1 ) {
		// parse key
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}

		char keyname[MAX_TOKEN_CHARS];
		Q_strncpyz(keyname, token, sizeof(keyname));

		// parse value
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}

		char value[MAX_TOKEN_CHARS];
		Q_strncpyz(value, token, sizeof(value));

		// check for a different grid size
		if (!Q_stricmp(keyname, "gridsize")) {
			sscanf(value, "%f %f %f", &w->lightGridSize[0], &w->lightGridSize[1], &w->lightGridSize[2] );
			continue;
		}
	}
}


qbool R_GetEntityToken( char* buffer, int size )
{
	const char* s = COM_Parse( &s_worldData.entityParsePoint );
	Q_strncpyz( buffer, s, size );

	if ( !s_worldData.entityParsePoint || !s[0] ) {
		s_worldData.entityParsePoint = s_worldData.entityString;
		return qfalse;
	}

	return qtrue;
}


// called directly from cgame

void RE_LoadWorldMap( const char* name )
{
	int i;

	if ( tr.worldMapLoaded )
		ri.Error( ERR_DROP, "ERROR: attempted to redundantly load world map\n" );

	tr.worldMapLoaded = qtrue;

	byte* buffer;
	int pakChecksum = 0;
	ri.FS_ReadFilePak( name, (void**)&buffer, &pakChecksum );
	if ( !buffer )
		ri.Error( ERR_DROP, "RE_LoadWorldMap: %s not found", name );

	// clear tr.world so if the level fails to load, the next
	// try will not look at the partially loaded version
	tr.world = NULL;

	Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );
	Q_strncpyz( s_worldData.name, name, sizeof( s_worldData.name ) );

	Q_strncpyz( s_worldData.baseName, COM_SkipPath( s_worldData.name ), sizeof( s_worldData.name ) );
	COM_StripExtension(s_worldData.baseName, s_worldData.baseName, sizeof(s_worldData.baseName));

	dheader_t* header = (dheader_t*)buffer;
	fileBase = (byte*)header;

	i = LittleLong(header->version);
	if ( i != BSP_VERSION )
		ri.Error( ERR_DROP, "RE_LoadWorldMap: %s has wrong version number (%i should be %i)", name, i, BSP_VERSION );

	// swap all the lumps
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++) {
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);
	}

	byte* startMarker = (byte*)ri.Hunk_Alloc( 0, h_low );

	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS] );
	R_LoadPlanes (&header->lumps[LUMP_PLANES]);
	R_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_DRAWVERTS], &header->lumps[LUMP_DRAWINDEXES] );
	R_LoadMarksurfaces (&header->lumps[LUMP_LEAFSURFACES]);
	R_LoadNodesAndLeafs (&header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS]);
	R_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	R_LoadEntities( &header->lumps[LUMP_ENTITIES] );
	R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );

	s_worldData.dataSize = (byte*)ri.Hunk_Alloc( 0, h_low ) - startMarker;

	// only set tr.world now that we know the entire level has loaded properly
	tr.world = &s_worldData;

	ri.FS_FreeFile( buffer );

	// invertedpenguin: replace the blood pool behind the RA because it severely impedes legibility
	// (yes, r_mapGreyscale can be an effective work-around)
	if ( pakChecksum == 1472072794 &&
	     s_worldData.numsurfaces == 3064 &&
	     !Q_stricmp( R_GetMapName(), "invertedpenguin" ) &&
	     !Q_stricmp( s_worldData.surfaces[2859].shader->name, "textures/inpe/m_liq/meat_liquid_bloodpool_A" ) ) {
		s_worldData.surfaces[2859].shader = R_FindShader( "textures/liquids/calm_poollight", LIGHTMAP_NONE, 0 );
	}
}


const char* R_GetMapName()
{
	return s_worldData.baseName[0] != '\0' ? s_worldData.baseName : "";
}

