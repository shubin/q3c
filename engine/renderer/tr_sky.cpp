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
// tr_sky.c
#include "tr_local.h"

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];

/*
===================================================================================

POLYGON TO BOX SIDE PROJECTION

===================================================================================
*/


static vec2_t sky_mins_st[6], sky_maxs_st[6];


/*
================
AddSkyPolygon
================
*/
static void AddSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;
	// s = [0]/[2], t = [1]/[2]
	static int	vec_to_st[6][3] =
	{
		{-2,3,1},
		{2,3,-1},

		{1,3,2},
		{-1,3,-2},

		{-2,-1,3},
		{-2,1,-3}

	//	{-1,2,3},
	//	{1,2,-3}
	};

	// decide which face it maps to
	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001f)
			continue;	// don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (sky_mins_st[axis][0] > s)
			sky_mins_st[axis][0] = s;
		if (sky_mins_st[axis][1] > t)
			sky_mins_st[axis][1] = t;
		if (sky_maxs_st[axis][0] < s)
			sky_maxs_st[axis][0] = s;
		if (sky_maxs_st[axis][1] < t)
			sky_maxs_st[axis][1] = t;
	}
}

#define	ON_EPSILON		0.1f			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64

static const vec3_t sky_clip[6] =
{
	{  1,  1,  0 }, // R
	{  1, -1,  0 }, // L
	{  0, -1,  1 }, // B
	{  0,  1,  1 }, // F
	{  1,  0,  1 }, // U
	{ -1,  0,  1 }  // D
};

static void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	const float* norm;
	float	*v;
	qbool	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		ri.Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		AddSkyPolygon (nump, vecs);
		return;
	}

	front = back = qfalse;
	norm = sky_clip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = qtrue;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = qtrue;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}


static void ClearSkyBox()
{
	for (int i = 0; i < 6; ++i) {
		sky_mins_st[i][0] = sky_mins_st[i][1] = 9999;
		sky_maxs_st[i][0] = sky_maxs_st[i][1] = -9999;
	}
}


static void RB_ClipSkyPolygons()
{
	vec3_t p[4];	// need one extra point for clipping

	ClearSkyBox();

	for ( int i = 0; i < tess.numIndexes; i += 3 ) {
		VectorSubtract( tess.xyz[tess.indexes[i+0]], backEnd.viewParms.orient.origin, p[0] );
		VectorSubtract( tess.xyz[tess.indexes[i+1]], backEnd.viewParms.orient.origin, p[1] );
		VectorSubtract( tess.xyz[tess.indexes[i+2]], backEnd.viewParms.orient.origin, p[2] );
		ClipSkyPolygon( 3, p[0], 0 );
	}
}


/*
===================================================================================

CLOUD VERTEX GENERATION

===================================================================================
*/


static vec3_t s_skyPoints[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];
static vec2_t s_skyTexCoords[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];


// s, t range from -1 to 1

static void MakeSkyVec( float s, float t, int axis, vec2_t st, vec3_t xyz )
{
	// 1 = s, 2 = t, 3 = zfar
	static const int st_to_vec[6][3] =
	{
		{  3, -1,  2 },
		{ -3,  1,  2 },

		{  1,  3,  2 },
		{ -1, -3,  2 },

		{ -2, -1,  3 },		// 0 degrees yaw, look straight up
		{  2, -1, -3 }		// look straight down
	};

	vec3_t b;
	float boxSize = backEnd.viewParms.zFar;
	b[0] = boxSize * s;
	b[1] = boxSize * t;
	b[2] = boxSize;

	for (int i = 0; i < 3; ++i) {
		int k = st_to_vec[axis][i];
		xyz[i] = (k < 0) ? -b[-k - 1] : b[k - 1];
	}

	// convert our -1:1 range (and inverted t) into GL TCs
	if ( st ) {
		st[0] = Com_Clamp( 0, 1, (s+1) * 0.5 );
		st[1] = 1.0 - Com_Clamp( 0, 1, (t+1) * 0.5 );
	}
}


void RB_CalcSkyBounds()
{
	for (int i = 0; i < 6; ++i) {
		sky_mins_st[i][0] = floor( sky_mins_st[i][0] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins_st[i][1] = floor( sky_mins_st[i][1] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs_st[i][0] =  ceil( sky_maxs_st[i][0] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs_st[i][1] =  ceil( sky_maxs_st[i][1] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
	}
}


static void FillCloudySkySide( const int mins[2], const int maxs[2], qbool addIndexes )
{
	const int vertexStart = tess.numVertexes;
	const uint32_t vertexColor =
		(uint32_t)tr.identityLightByte |
		((uint32_t)tr.identityLightByte << 8) |
		((uint32_t)tr.identityLightByte << 16) |
		((uint32_t)255 << 24);

	for ( int t = mins[1]+HALF_SKY_SUBDIVISIONS; t <= maxs[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		for ( int s = mins[0]+HALF_SKY_SUBDIVISIONS; s <= maxs[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			VectorAdd( s_skyPoints[t][s], backEnd.viewParms.orient.origin, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0] = s_skyTexCoords[t][s][0];
			tess.texCoords[tess.numVertexes][1] = s_skyTexCoords[t][s][1];
			*(uint32_t*)&tess.vertexColors[tess.numVertexes] = vertexColor;

			tess.numVertexes++;

			if ( tess.numVertexes >= SHADER_MAX_VERTEXES )
			{
				ri.Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()\n" );
			}
		}
	}

	// only add indexes for one pass, otherwise it would draw multiple times for each pass
	if ( !addIndexes )
		return;

	int tHeight = maxs[1] - mins[1] + 1;
	int sWidth = maxs[0] - mins[0] + 1;

	for ( int t = 0; t < tHeight-1; t++ )
	{
		for ( int s = 0; s < sWidth-1; s++ )
		{
			tess.indexes[tess.numIndexes] = vertexStart + s + t * ( sWidth );
			tess.numIndexes++;
			tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
			tess.numIndexes++;
			tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
			tess.numIndexes++;

			tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
			tess.numIndexes++;
			tess.indexes[tess.numIndexes] = vertexStart + s + 1 + ( t + 1 ) * ( sWidth );
			tess.numIndexes++;
			tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
			tess.numIndexes++;
		}
	}

	for ( int i = 0; i < tess.shader->numStages; ++i )
	{
		R_ComputeColors( tess.shader->stages[i], tess.svars[i], 0, tess.numVertexes );
		R_ComputeTexCoords( tess.shader->stages[i], tess.svars[i], 0, tess.numVertexes, qfalse );
	}
}


static void FillCloudBox( const shader_t* shader, int stage )
{
	// skybox surfs are ordered RLBFUD, so don't draw clouds on the last one

	for (int i = 0; i < 5; ++i)
	{
		int s, t;
		int sky_mins_subd[2], sky_maxs_subd[2];

		if ( ( sky_mins_st[i][0] >= sky_maxs_st[i][0] ) || ( sky_mins_st[i][1] >= sky_maxs_st[i][1] ) ) {
			//ri.Printf( PRINT_ALL, "clipped cloudside %i\n", i );
			continue;
		}

		sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_mins_st[i][0] );
		sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_mins_st[i][1] );
		sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_maxs_st[i][0] );
		sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_maxs_st[i][1] );

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							NULL,
							s_skyPoints[t][s] );

				s_skyTexCoords[t][s][0] = s_cloudTexCoords[i][t][s][0];
				s_skyTexCoords[t][s][1] = s_cloudTexCoords[i][t][s][1];
			}
		}

		// only add indexes for first stage
		FillCloudySkySide( sky_mins_subd, sky_maxs_subd, (qbool)(stage == 0) );
	}
}


void R_BuildCloudData()
{
	Q_assert( tess.shader->isSky );

	// set up for drawing
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	for (int i = 0; (i < MAX_SHADER_STAGES) && tess.xstages[i]; ++i) {
		FillCloudBox( tess.shader, i );
	}
}


// called when a sky shader is parsed

void R_InitSkyTexCoords( float heightCloud )
{
	int i, s, t;
	float radiusWorld = 4096;
	float p;
	float sRad, tRad;
	vec3_t skyVec;
	vec3_t v;

	// init zfar so MakeSkyVec works even though
	// a world hasn't been bounded
	backEnd.viewParms.zFar = 1024;

	for ( i = 0; i < 6; i++ )
	{
		for ( t = 0; t <= SKY_SUBDIVISIONS; t++ )
		{
			for ( s = 0; s <= SKY_SUBDIVISIONS; s++ )
			{
				// compute vector from view origin to sky side integral point
				MakeSkyVec(
						(float)(s - HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS,
						(float)(t - HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS,
						i, NULL, skyVec
				);

				// compute parametric value 'p' that intersects with cloud layer
				p = ( 1.0f / ( 2 * DotProduct( skyVec, skyVec ) ) ) *
					( -2 * skyVec[2] * radiusWorld +
					   2 * sqrt( Square( skyVec[2] ) * Square( radiusWorld ) +
								 2 * Square( skyVec[0] ) * radiusWorld * heightCloud +
								 Square( skyVec[0] ) * Square( heightCloud ) +
								 2 * Square( skyVec[1] ) * radiusWorld * heightCloud +
								 Square( skyVec[1] ) * Square( heightCloud ) +
								 2 * Square( skyVec[2] ) * radiusWorld * heightCloud +
								 Square( skyVec[2] ) * Square( heightCloud ) ) );

				// compute intersection point based on p
				VectorScale( skyVec, p, v );
				v[2] += radiusWorld;

				// compute vector from world origin to intersection point 'v'
				VectorNormalize( v );

				sRad = Q_acos( v[0] );
				tRad = Q_acos( v[1] );

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}
}


static void DrawSkyBox()
{
	image_t* const* skyImages = &tess.shader->sky.outerbox[0];
	RB_PushSingleStageShader( 0, CT_TWO_SIDED );
	shaderStage_t* const stage = tess.shader->stages[0];
	stage->rgbGen = CGEN_IDENTITY_LIGHTING;
	((shader_t*)tess.shader)->isSky = qtrue;

	for (int i = 0; i < 6; ++i)
	{
		if ( ( sky_mins_st[i][0] >= sky_maxs_st[i][0] ) || ( sky_mins_st[i][1] >= sky_maxs_st[i][1] ) ) {
			continue;
		}

		int sky_mins_subd[2];
		int sky_maxs_subd[2];
		sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_mins_st[i][0] );
		sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_mins_st[i][1] );
		sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_maxs_st[i][0] );
		sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS * Com_Clamp( -1, 1, sky_maxs_st[i][1] );

		// iterate through the subdivisions
		for (int t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; ++t)
		{
			for (int s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; ++s)
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i, s_skyTexCoords[t][s], s_skyPoints[t][s] );
			}
		}

		// write to tess and draw
		stage->bundle.image[0] = skyImages[i];
		tess.numVertexes = 0;
		tess.numIndexes = 0;
		FillCloudySkySide( sky_mins_subd, sky_maxs_subd, qtrue );
		renderPipeline->DrawSkyBox();
		
	}

	RB_PopShader();
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}


void RB_DrawSky()
{
	if (r_fastsky->integer)
		return;

	// project all the polygons onto the sky box
	// to see which blocks on each side need to be drawn
	RB_ClipSkyPolygons();
	RB_CalcSkyBounds();

	if (tess.shader->sky.outerbox[0] && tess.shader->sky.outerbox[0] != tr.defaultImage)
		DrawSkyBox();

	if (tess.shader->sky.cloudHeight > 0.0f) {
		R_BuildCloudData();
		if (tess.numVertexes > 0 && tess.numIndexes > 0)
			renderPipeline->DrawClouds();
	}
}
