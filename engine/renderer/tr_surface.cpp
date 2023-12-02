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
// tr_surf.c
#include "tr_local.h"

#if idSSE2
#include <emmintrin.h>
#include <stddef.h> // offsetof macro
static byte check_srfVertTC[(offsetof(srfVert_t, st2) == offsetof(srfVert_t, st) + 8) ? 1 : -1];
static byte check_drawVertTC[(offsetof(drawVert_t, lightmap) == offsetof(drawVert_t, st) + 8) ? 1 : -1];
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


///////////////////////////////////////////////////////////////


// a single SURFACE that exceeds MAX_VERTEXES or MAX_INDEXES is a fatal error
// a single BATCH that exceeds them will just be broken down into multiple batches

void RB_CheckOverflow( int verts, int indexes )
{
	if (tess.numVertexes + verts < SHADER_MAX_VERTEXES
		&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
		return;
	}

	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	}
	if ( indexes >= SHADER_MAX_INDEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	}

	RB_BeginSurface( tess.shader, tess.fogNum );
}


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

	RB_CHECKOVERFLOW( 4, 6 );

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[ tess.numIndexes ] = ndx;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 3;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx+1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx+1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx+1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx+2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx+2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx+2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx+3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx+3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx+3][2] = origin[2] + left[2] - up[2];


	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.orient.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx+1][0] = tess.normal[ndx+2][0] = tess.normal[ndx+3][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx+1][1] = tess.normal[ndx+2][1] = tess.normal[ndx+3][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx+1][2] = tess.normal[ndx+2][2] = tess.normal[ndx+3][2] = normal[2];
	
	// standard square texture coordinates
	tess.texCoords[ndx][0] = tess.texCoords2[ndx][0] = s1;
	tess.texCoords[ndx][1] = tess.texCoords2[ndx][1] = t1;

	tess.texCoords[ndx+1][0] = tess.texCoords2[ndx+1][0] = s2;
	tess.texCoords[ndx+1][1] = tess.texCoords2[ndx+1][1] = t1;

	tess.texCoords[ndx+2][0] = tess.texCoords2[ndx+2][0] = s2;
	tess.texCoords[ndx+2][1] = tess.texCoords2[ndx+2][1] = t2;

	tess.texCoords[ndx+3][0] = tess.texCoords2[ndx+3][0] = s1;
	tess.texCoords[ndx+3][1] = tess.texCoords2[ndx+3][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	* ( unsigned int * ) &tess.vertexColors[ndx] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+1] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+2] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+3] = 
		* ( unsigned int * )color;


	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}


static void RB_SurfaceSprite()
{
	vec3_t left, up;
	float radius = backEnd.currentEntity->e.radius;

	// calculate the xyz locations for the four corners
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.orient.axis[1], radius, left );
		VectorScale( backEnd.viewParms.orient.axis[2], radius, up );
	} else {
		float ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		float s = sin( ang );
		float c = cos( ang );

		VectorScale( backEnd.viewParms.orient.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.orient.axis[2], left );

		VectorScale( backEnd.viewParms.orient.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.orient.axis[1], up );
	}
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}


static void RB_SurfacePolychain( const srfPoly_t* p )
{
	int i;

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

	int numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; ++i ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[numv][0] = p->verts[i].st[0];
		tess.texCoords[numv][1] = p->verts[i].st[1];
		*(unsigned*)&tess.vertexColors[numv] = *(unsigned*)p->verts[i].modulate;
		++numv;
	}

	for ( i = 0; i < p->numVerts-2; ++i ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}


static void RB_SurfaceTriangles( srfTriangles_t* surf )
{
	int i, ndx;

	RB_CHECKOVERFLOW( surf->numVerts, surf->numIndexes );

	unsigned int* tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = 0; i < surf->numIndexes; ++i )
		tessIndexes[i] = tess.numVertexes + surf->indexes[i];
	tess.numIndexes += surf->numIndexes;

	const srfVert_t* v = surf->verts;
	for ( i = 0, ndx = tess.numVertexes; i < surf->numVerts; ++i, ++v, ++ndx ) {
		VectorCopy( v->xyz, tess.xyz[ndx] );
		VectorCopy( v->normal, tess.normal[ndx] );
		tess.texCoords[ndx][0] = v->st[0];
		tess.texCoords[ndx][1] = v->st[1];
		tess.texCoords2[ndx][0] = v->st2[0];
		tess.texCoords2[ndx][1] = v->st2[1];
		*(unsigned int *)&tess.vertexColors[ndx] = *(unsigned int *)v->rgba;
	}

	tess.numVertexes += surf->numVerts;
}


///////////////////////////////////////////////////////////////


static void RB_LightningBoltFace( const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth )
{
	float t = len / 256.0f;
	int vbase = tess.numVertexes;

	// FIXME: use quad stamp?
	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0] = 0;
	tess.texCoords[tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;
	tess.numVertexes++;

	VectorMA( start, -spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0] = 0;
	tess.texCoords[tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0] = t;
	tess.texCoords[tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA( end, -spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0] = t;
	tess.texCoords[tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}


static void RB_SurfaceLightningBolt()
{
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	const refEntity_t* e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.orient.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.orient.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t temp;
#if defined( QC )
		RB_LightningBoltFace( start, end, right, len, e->radius ? e->radius : 8 );
#else // QC
		RB_LightningBoltFace( start, end, right, len, 8 );
#endif // QC
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}


/*
** VectorArrayNormalize
*
* The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
* This means that we don't have to worry about zero length or enormously long vectors.
*/
static void VectorArrayNormalize(vec4_t *normals, unsigned int count)
{
//    assert(count);
        
#if idppc
    {
        register float half = 0.5;
        register float one  = 1.0;
        float *components = (float *)normals;
        
        // Vanilla PPC code, but since PPC has a reciprocal square root estimate instruction,
        // runs *much* faster than calling sqrt().  We'll use a single Newton-Raphson
        // refinement step to get a little more precision.  This seems to yeild results
        // that are correct to 3 decimal places and usually correct to at least 4 (sometimes 5).
        // (That is, for the given input range of about 0.6 to 2.0).
        do {
            float x, y, z;
            float B, y0, y1;
            
            x = components[0];
            y = components[1];
            z = components[2];
            components += 4;
            B = x*x + y*y + z*z;

#ifdef __GNUC__            
            asm("frsqrte %0,%1" : "=f" (y0) : "f" (B));
#else
			y0 = __frsqrte(B);
#endif
            y1 = y0 + half*y0*(one - B*y0*y0);

            x = x * y1;
            y = y * y1;
            components[-4] = x;
            z = z * y1;
            components[-3] = y;
            components[-2] = z;
        } while(count--);
    }
#else // No assembly version for this architecture, or C_ONLY defined
	// given the input, it's safe to call VectorNormalizeFast
    while (count--) {
        VectorNormalizeFast(normals[0]);
        normals++;
    }
#endif

}


static void DecompressNormalVector( vec3_t output, const short* input )
{
	const float lat = ((input[0] >> 8) & 0xFF) * ((2.0f * M_PI) / 256.0f);
	const float lon = ( input[0]       & 0xFF) * ((2.0f * M_PI) / 256.0f);
	const float cosLat = cos( lat );
	const float sinLat = sin( lat );
	const float cosLon = cos( lon );
	const float sinLon = sin( lon );
	output[0] = cosLat * sinLon;
	output[1] = sinLat * sinLon;
	output[2] = cosLon;
}


static void LerpMeshVertexes( md3Surface_t* surf, float backlerp )
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{
			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			DecompressNormalVector( outNormal, newNormals );
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			DecompressNormalVector( uncompressedNewNormal, newNormals );
			DecompressNormalVector( uncompressedOldNormal, oldNormals );
			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;
		}
		VectorArrayNormalize((vec4_t *)tess.normal[tess.numVertexes], numVerts);
	}
}


/*
=============
RB_SurfaceMesh
=============
*/
void RB_SurfaceMesh(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Bob, Doug;

	if ( backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	RB_CHECKOVERFLOW( surface->numVerts, surface->numTriangles*3 );

	LerpMeshVertexes (surface, backlerp);

	triangles = (int *) ((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for (j = 0 ; j < indexes ; j++) {
		tess.indexes[Bob + j] = Doug + triangles[j];
	}
	tess.numIndexes += indexes;

	texCoords = (float *) ((byte *)surface + surface->ofsSt);

	for ( j = 0; j < surface->numVerts; j++ ) {
		tess.texCoords[Doug + j][0] = texCoords[j*2+0];
		tess.texCoords[Doug + j][1] = texCoords[j*2+1];
		// FIXME: fill in lightmapST for completeness?
	}

	tess.numVertexes += surface->numVerts;
}


static void RB_SurfaceFace( srfSurfaceFace_t* surf )
{
	RB_CHECKOVERFLOW( surf->numVerts, surf->numIndexes );

	const int tessNumVertexes = tess.numVertexes;
	const int* surfIndexes = surf->indexes;
	unsigned int* tessIndexes = tess.indexes + tess.numIndexes;
	unsigned int* const tessIndexesEnd = tessIndexes + surf->numIndexes;
#if idSSE2
	unsigned int* const tessIndexesEndSIMD = tessIndexesEnd - 3;
	const __m128i xmmNumVerts = _mm_set1_epi32( tess.numVertexes );
	while ( tessIndexes < tessIndexesEndSIMD ) {
		const __m128i xmmIn = _mm_loadu_si128( (const __m128i*)surfIndexes );
		const __m128i xmmOut = _mm_add_epi32( xmmIn, xmmNumVerts );
		_mm_storeu_si128( (__m128i*)tessIndexes, xmmOut );
		tessIndexes += 4;
		surfIndexes += 4;
	}
#endif
	while ( tessIndexes < tessIndexesEnd ) {
		*tessIndexes++ = *surfIndexes++ + tessNumVertexes;
	}
	tess.numIndexes += surf->numIndexes;

	const srfVert_t* v = surf->verts;
	int i = 0;
	int ndx = tess.numVertexes;
	const int end = surf->numVerts;
#if idSSE2
	const int endSIMD = end - 1;
	for ( ; i < endSIMD; i += 2, v += 2, ndx += 2 ) {
		const __m128i xmmP0 = _mm_loadu_si128((const __m128i*)(v + 0)->xyz);
		const __m128i xmmN0 = _mm_loadu_si128((const __m128i*)(v + 0)->normal);
		const __m128i xmmT0 = _mm_loadu_si128((const __m128i*)(v + 0)->st); // tc2_0.y tc2_0.x tc_0.y tc_0.x
		const __m128i xmmP1 = _mm_loadu_si128((const __m128i*)(v + 1)->xyz);
		const __m128i xmmN1 = _mm_loadu_si128((const __m128i*)(v + 1)->normal);
		const __m128i xmmT1 = _mm_loadu_si128((const __m128i*)(v + 1)->st); // tc2_1.y tc2_1.x tc_1.y tc_1.x
		const __m128i xmmTC0 = _mm_unpacklo_epi64(xmmT0, xmmT1); // tc_1.y tc_1.x tc_0.y tc_0.x
		const __m128i xmmTC1 = _mm_unpackhi_epi64(xmmT0, xmmT1); // tc2_1.y tc2_1.x tc2_0.y tc2_0.x
		_mm_storeu_si128((__m128i*)tess.xyz[ndx + 0], xmmP0);
		_mm_storeu_si128((__m128i*)tess.xyz[ndx + 1], xmmP1);
		_mm_storeu_si128((__m128i*)tess.normal[ndx + 0], xmmN0);
		_mm_storeu_si128((__m128i*)tess.normal[ndx + 1], xmmN1);
		_mm_storeu_si128((__m128i*)tess.texCoords[ndx], xmmTC0);
		_mm_storeu_si128((__m128i*)tess.texCoords2[ndx], xmmTC1);
		*(uint32_t*)&tess.vertexColors[ndx + 0] = *(uint32_t*)(v + 0)->rgba;
		*(uint32_t*)&tess.vertexColors[ndx + 1] = *(uint32_t*)(v + 1)->rgba;
	}
#endif
	for ( ; i < end; ++i, ++v, ++ndx ) {
#if idSSE2
		const __m128i xmmP = _mm_loadu_si128((const __m128i*)v->xyz);
		const __m128i xmmN = _mm_loadu_si128((const __m128i*)v->normal);
		const __m128i xmmT1 = _mm_loadu_si128((const __m128i*)v->st);
		const __m128i xmmT2 = _mm_shuffle_epi32(xmmT1, (2 << 0) | (3 << 2) | (0 << 4) | (1 << 6));
		_mm_storeu_si128((__m128i*)tess.xyz[ndx], xmmP);
		_mm_storeu_si128((__m128i*)tess.normal[ndx], xmmN);
		_mm_storel_epi64((__m128i*)tess.texCoords[ndx], xmmT1);
		_mm_storel_epi64((__m128i*)tess.texCoords2[ndx], xmmT2);
		*(uint32_t*)&tess.vertexColors[ndx] = *(uint32_t*)v->rgba;
#elif defined Q3_LITTLE_ENDIAN
		*(uint64_t*)&tess.xyz[ndx][0] = *(uint64_t*)&v->xyz[0];
		tess.xyz[ndx][2] = v->xyz[2];
		*(uint64_t*)&tess.normal[ndx][0] = *(uint64_t*)&v->normal[0];
		tess.normal[ndx][2] = v->normal[2];
		*(uint64_t*)&tess.texCoords[ndx][0] = *(uint64_t*)&v->st[0];
		*(uint64_t*)&tess.texCoords2[ndx][0] = *(uint64_t*)&v->st2[0];
		*(uint32_t*)&tess.vertexColors[ndx] = *(uint32_t*)v->rgba;
#else
		VectorCopy( v->xyz, tess.xyz[ndx] );
		VectorCopy( v->normal, tess.normal[ndx] );
		tess.texCoords[ndx][0] = v->st[0];
		tess.texCoords[ndx][1] = v->st[1];
		tess.texCoords2[ndx][0] = v->st2[0];
		tess.texCoords2[ndx][1] = v->st2[1];
		tess.vertexColors[ndx][0] = v->rgba[0];
		tess.vertexColors[ndx][1] = v->rgba[1];
		tess.vertexColors[ndx][2] = v->rgba[2];
		tess.vertexColors[ndx][3] = v->rgba[3];
#endif
	}

	tess.numVertexes += surf->numVerts;
}


static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.orient.axis[0][0] + local[1] * backEnd.orient.axis[1][0] +
		local[2] * backEnd.orient.axis[2][0] + backEnd.orient.origin[0];
	world[1] = local[0] * backEnd.orient.axis[0][1] + local[1] * backEnd.orient.axis[1][1] +
		local[2] * backEnd.orient.axis[2][1] + backEnd.orient.origin[1];
	world[2] = local[0] * backEnd.orient.axis[0][2] + local[1] * backEnd.orient.axis[1][2] +
		local[2] * backEnd.orient.axis[2][2] + backEnd.orient.origin[2];

	VectorSubtract( world, backEnd.viewParms.orient.origin, world );
	d = DotProduct( world, backEnd.viewParms.orient.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j;
	float	*xyz;
	float	*texCoords;
	float	*texCoords2;
	float	*normal;
	unsigned char *color;
	drawVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;

	// determine the allowable discrepance
	lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;


	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				RB_EndSurface();
				RB_BeginSurface(tess.shader, tess.fogNum );
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		texCoords = tess.texCoords[numVertexes];
		texCoords2 = tess.texCoords2[numVertexes];
		color = ( unsigned char * ) &tess.vertexColors[numVertexes];

		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];
#if idSSE2
				const __m128i xmmP = _mm_loadu_si128((const __m128i*)dv->xyz);
				const __m128i xmmT1 = _mm_loadu_si128((const __m128i*)dv->st);
				const __m128i xmmN = _mm_loadu_si128((const __m128i*)dv->normal);
				const __m128i xmmT2 = _mm_shuffle_epi32(xmmT1, (2 << 0) | (3 << 2) | (0 << 4) | (1 << 6));
				_mm_storeu_si128((__m128i*)xyz, xmmP);
				_mm_storeu_si128((__m128i*)normal, xmmN);
				_mm_storel_epi64((__m128i*)texCoords, xmmT1);
				_mm_storel_epi64((__m128i*)texCoords2, xmmT2);
				*(uint32_t*)color = *(uint32_t*)dv->color;
#elif defined Q3_LITTLE_ENDIAN
				*(uint64_t*)&xyz[0] = *(uint64_t*)&dv->xyz[0];
				xyz[2] = dv->xyz[2];
				*(uint64_t*)&texCoords[0] = *(uint64_t*)&dv->st[0];
				*(uint64_t*)&texCoords2[0] = *(uint64_t*)&dv->lightmap[0];
				*(uint64_t*)&normal[0] = *(uint64_t*)&dv->normal[0];
				normal[2] = dv->normal[2];
				*(uint32_t*)color = *(uint32_t*)dv->color;
#else
				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];
				texCoords2[0] = dv->lightmap[0];
				texCoords2[1] = dv->lightmap[1];
				normal[0] = dv->normal[0];
				normal[1] = dv->normal[1];
				normal[2] = dv->normal[2];
				color[0] = dv->color[0];
				color[1] = dv->color[1];
				color[2] = dv->color[2];
				color[3] = dv->color[3];
#endif
				xyz += 4;
				normal += 4;
				texCoords += 2;
				texCoords2 += 2;
				color += 4;
			}
		}


		// add the indexes
		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;
					
					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/


static void RB_SurfaceBad( const surfaceType_t* surfType )
{
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

static void RB_SurfaceSkip( const void* surf )
{
}


// entities that have a single procedurally generated surface

static void RB_SurfaceEntity( surfaceType_t* surfType )
{
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt();
		break;
	default:
		// invalid or deprecated
		break;
	}
}


void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( const void* ) = {
	(void(*)( const void* ))RB_SurfaceBad,			// SF_BAD
	(void(*)( const void* ))RB_SurfaceSkip,			// SF_SKIP
	(void(*)( const void* ))RB_SurfaceFace,			// SF_FACE
	(void(*)( const void* ))RB_SurfaceGrid,			// SF_GRID
	(void(*)( const void* ))RB_SurfaceTriangles,	// SF_TRIANGLES
	(void(*)( const void* ))RB_SurfacePolychain,	// SF_POLY
	(void(*)( const void* ))RB_SurfaceMesh,			// SF_MD3
	(void(*)( const void* ))RB_SurfaceSkip,			// SF_FLARE
	(void(*)( const void* ))RB_SurfaceEntity,		// SF_ENTITY
};
