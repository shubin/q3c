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
// tr_shade_calc.c

#include "tr_local.h"


// input's in the [0;1] range
typedef double (*waveGenFunc_t)( double );


static double WaveGenSine( double x )
{
	return sin(x * 2.0 * M_PI);
}


static double WaveGenSquare( double x )
{
	return x < 0.5 ? 1.0 : -1.0;
}


static double WaveGenTriangle( double xs )
{
	// the original table shifts the triangle for whatever reason...
	const double x = (xs < 0.75) ? (xs + 0.25) : (xs - 0.75);
	const double l = x * 4.0 - 1.0;
	const double r = 1.0 - (x - 0.5) * 4.0;
	const double f = (x < 0.5) ? l : r;

	return f;
}


static double WaveGenSawTooth( double x )
{
	return x;
}


static double WaveGenInverseSawTooth( double x )
{
	return 1.0 - x;
}


static double WaveGenInvalid( double x )
{
	ri.Error( ERR_DROP, "WaveGenInvalid called in shader '%s'\n", tess.shader->name );
	return 0.0;
}


static double WaveValue( genFunc_t func , double base, double amplitude, double phase, double freq )
{
	static const waveGenFunc_t waveGenFunctions[GF_COUNT] =
	{
		WaveGenInvalid, // GF_NULL
		WaveGenSine,
		WaveGenSquare,
		WaveGenTriangle,
		WaveGenSawTooth,
		WaveGenInverseSawTooth,
		WaveGenInvalid // GF_NOISE
	};

	// fmod doesn't behave how we want with negative numerators
	// fmod(-2.25, 1) returns -0.25 but we really want 0.75
	const double xo = phase + tess.shaderTime * freq;
	const double xr = (double)(int)xo; // rounded towards 0
	const double x = xo >= 0.0 ? (xo - xr) : (xo - xr + 1.0);
	const double r = base + (waveGenFunctions[func])(x) * amplitude;

	return r;
}


// Evaluates a given waveForm_t, referencing backEnd.refdef.time directly
static float EvalWaveForm( const waveForm_t *wf )
{
	return WaveValue( wf->func, wf->base, wf->amplitude, wf->phase, wf->frequency );
}


static float EvalWaveFormClamped( const waveForm_t *wf )
{
	float glow  = EvalWaveForm( wf );

	if ( glow < 0 )
	{
		return 0;
	}

	if ( glow > 1 )
	{
		return 1;
	}

	return glow;
}


static void RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *st, int numVertexes )
{
	int i;

	for ( i = 0; i < numVertexes; i++, st += 2 )
	{
		float s = st[0];
		float t = st[1];

		st[0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
		st[1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
	}
}


static void RB_CalcStretchTexCoords( const waveForm_t *wf, float *st, int numVertexes )
{
	float p;
	texModInfo_t tmi;

	p = 1.0f / EvalWaveForm( wf );

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords( &tmi, st, numVertexes );
}


static void RB_CalcDeformVertexes( const deformStage_t* ds, int firstVertex, int numVertexes )
{
	float* xyz = (float*)&tess.xyz[firstVertex];
	float* normal = (float*)&tess.normal[firstVertex];
	vec3_t offset;

	if ( ds->deformationWave.frequency == 0 )
	{
		const float scale = EvalWaveForm( &ds->deformationWave );

		for ( int i = 0; i < numVertexes; i++, xyz += 4, normal += 4 )
		{
			VectorScale( normal, scale, offset );
			
			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
	else
	{
		for ( int i = 0; i < numVertexes; i++, xyz += 4, normal += 4 )
		{
			const float off = ( xyz[0] + xyz[1] + xyz[2] ) * ds->deformationSpread;
			const float scale = WaveValue( ds->deformationWave.func, ds->deformationWave.base, 
				ds->deformationWave.amplitude,
				ds->deformationWave.phase + off,
				ds->deformationWave.frequency );

			VectorScale( normal, scale, offset );
			
			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
}


// wiggle the normals for wavy environment mapping
static void RB_CalcDeformNormals( const deformStage_t* ds, int firstVertex, int numVertexes )
{
	int i;
	float scale;
	const float *xyz = ( const float * ) &tess.xyz[firstVertex];
	float *normal = ( float * ) &tess.normal[firstVertex];

	for ( i = 0; i < numVertexes; i++, xyz += 4, normal += 4 ) {
		scale = 0.98f;
		scale = R_NoiseGet4f( xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 0 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 100 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 1 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 200 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 2 ] += ds->deformationWave.amplitude * scale;

		VectorNormalizeFast( normal );
	}
}


static void RB_CalcBulgeVertexes( const deformStage_t* ds, int firstVertex, int numVertexes )
{
	const float* st = (const float*)&tess.texCoords[firstVertex];
	float* xyz = (float*)&tess.xyz[firstVertex];
	float* normal = (float*)&tess.normal[firstVertex];

	const float now = (backEnd.refdef.time / 1000.0f) * ds->bulgeSpeed;

	for ( int i = 0; i < numVertexes; i++, xyz += 4, st += 2, normal += 4 ) {
		const float scale = ds->bulgeHeight * sin(st[0] * ds->bulgeWidth + now);
			
		xyz[0] += normal[0] * scale;
		xyz[1] += normal[1] * scale;
		xyz[2] += normal[2] * scale;
	}
}


// a deformation that can move an entire surface along a wave path
static void RB_CalcMoveVertexes( const deformStage_t* ds, int firstVertex, int numVertexes )
{
	const double scale = WaveValue(
		ds->deformationWave.func,
		ds->deformationWave.base, 
		ds->deformationWave.amplitude,
		ds->deformationWave.phase,
		ds->deformationWave.frequency );

	vec3_t offset;
	VectorScale( ds->moveVector, scale, offset );

	float* xyz = (float*)&tess.xyz[firstVertex];
	for ( int i = 0; i < numVertexes; i++, xyz += 4 ) {
		VectorAdd( xyz, offset, xyz );
	}
}


// @TODO:
// Change a polygon into a bunch of text polygons
static void DeformText( const char *text ) {
	int		i;
	vec3_t	origin, width, height;
	int		len;
	int		ch;
	byte	color[4];
	float	bottom, top;
	vec3_t	mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	CrossProduct( tess.normal[0], height, width );

	// find the midpoint of the box
	VectorClear( mid );
	bottom = 999999;
	top = -999999;
	for ( i = 0 ; i < 4 ; i++ ) {
		VectorAdd( tess.xyz[i], mid, mid );
		if ( tess.xyz[i][2] < bottom ) {
			bottom = tess.xyz[i][2];
		}
		if ( tess.xyz[i][2] > top ) {
			top = tess.xyz[i][2];
		}
	}
	VectorScale( mid, 0.25f, origin );

	// determine the individual character size
	height[0] = 0;
	height[1] = 0;
	height[2] = ( top - bottom ) * 0.5f;

	VectorScale( width, height[2] * -0.75f, width );

	// determine the starting position
	len = strlen( text );
	VectorMA( origin, (len-1), width, origin );

	// clear the shader indexes
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color[0] = color[1] = color[2] = color[3] = 255;

	// draw each character
	for ( i = 0 ; i < len ; i++ ) {
		ch = text[i];
		ch &= 255;

		if ( ch != ' ' ) {
			int		row, col;
			float	frow, fcol, size;

			row = ch>>4;
			col = ch&15;

			frow = row*0.0625f;
			fcol = col*0.0625f;
			size = 0.0625f;

			RB_AddQuadStampExt( origin, width, height, color, fcol, frow, fcol + size, frow + size );
		}
		VectorMA( origin, -2, width, origin );
	}
}


static void GlobalVectorToLocal( const vec3_t in, vec3_t out )
{
	out[0] = DotProduct( in, backEnd.orient.axis[0] );
	out[1] = DotProduct( in, backEnd.orient.axis[1] );
	out[2] = DotProduct( in, backEnd.orient.axis[2] );
}


// assuming all the triangles for this shader are independant quads,
// rebuild them as forward facing sprites
static void AutospriteDeform( int firstVertex, int numVertexes, int firstIndex, int numIndexes )
{
	int		i;
	float	*xyz;
	vec3_t	mid, delta;
	float	radius;
	vec3_t	left, up;
	vec3_t	leftDir, upDir;

	if ( numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd vertex count", tess.shader->name );
	}
	if ( numIndexes != ( numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd index count", tess.shader->name );
	}

	tess.numVertexes = firstVertex;
	tess.numIndexes = firstIndex;

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.orient.axis[1], leftDir );
		GlobalVectorToLocal( backEnd.viewParms.orient.axis[2], upDir );
	} else {
		VectorCopy( backEnd.viewParms.orient.axis[1], leftDir );
		VectorCopy( backEnd.viewParms.orient.axis[2], upDir );
	}

	for ( i = firstVertex ; i < firstVertex + numVertexes ; i+=4 ) {
		// find the midpoint
		xyz = tess.xyz[i];

		mid[0] = 0.25f * (xyz[0] + xyz[4] + xyz[8] + xyz[12]);
		mid[1] = 0.25f * (xyz[1] + xyz[5] + xyz[9] + xyz[13]);
		mid[2] = 0.25f * (xyz[2] + xyz[6] + xyz[10] + xyz[14]);

		VectorSubtract( xyz, mid, delta );
		radius = VectorLength( delta ) * 0.707f;		// / sqrt(2)

		VectorScale( leftDir, radius, left );
		VectorScale( upDir, radius, up );

		if ( backEnd.viewParms.isMirror ) {
			VectorSubtract( vec3_origin, left, left );
		}

		// compensate for scale in the axes if necessary
		if ( backEnd.currentEntity->e.nonNormalizedAxes ) {
			float axisLength = VectorLength( backEnd.currentEntity->e.axis[0] );
			axisLength = axisLength ? (1.0f / axisLength) : 1.0f;
			VectorScale(left, axisLength, left);
			VectorScale(up, axisLength, up);
		}

		RB_AddQuadStamp( mid, left, up, tess.vertexColors[i] );
	}
}


// Autosprite2 will pivot a rectangular quad along the center of its long axis
static void Autosprite2Deform( int firstVertex, int numVertexes, int firstIndex, int numIndexes ) {
	int		i, j, k;
	int		indexes;
	float	*xyz;
	vec3_t	forward;

	const int edgeVerts[6][2] = {
		{ 0, 1 },
		{ 0, 2 },
		{ 0, 3 },
		{ 1, 2 },
		{ 1, 3 },
		{ 2, 3 }
	};

	if ( numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd vertex count", tess.shader->name );
	}
	if ( numIndexes != ( numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd index count", tess.shader->name );
	}

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.orient.axis[0], forward );
	} else {
		VectorCopy( backEnd.viewParms.orient.axis[0], forward );
	}

	// this is a lot of work for two triangles...
	// we could precalculate a lot of it is an issue, but it would mess up
	// the shader abstraction
	for ( i = firstVertex, indexes = firstIndex ; i < firstVertex + numVertexes ; i+=4, indexes+=6 ) {
		float	lengths[2];
		int		nums[2];
		vec3_t	mid[2];
		vec3_t	major, minor;
		float	*v1, *v2;

		// find the midpoint
		xyz = tess.xyz[i];

		// identify the two shortest edges
		nums[0] = nums[1] = 0;
		lengths[0] = lengths[1] = 999999;

		for ( j = 0 ; j < 6 ; j++ ) {
			float	l;
			vec3_t	temp;

			v1 = xyz + 4 * edgeVerts[j][0];
			v2 = xyz + 4 * edgeVerts[j][1];

			VectorSubtract( v1, v2, temp );
			
			l = DotProduct( temp, temp );
			if ( l < lengths[0] ) {
				nums[1] = nums[0];
				lengths[1] = lengths[0];
				nums[0] = j;
				lengths[0] = l;
			} else if ( l < lengths[1] ) {
				nums[1] = j;
				lengths[1] = l;
			}
		}

		for ( j = 0 ; j < 2 ; j++ ) {
			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		// find the vector of the major axis
		VectorSubtract( mid[1], mid[0], major );

		// cross this with the view direction to get minor axis
		CrossProduct( major, forward, minor );
		VectorNormalize( minor );
		
		// re-project the points
		for ( j = 0 ; j < 2 ; j++ ) {
			float	l;

			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			l = 0.5 * sqrt( lengths[j] );

			// we need to see which direction this edge
			// is used to determine direction of projection
			for ( k = 0 ; k < 5 ; k++ ) {
				if ( tess.indexes[ indexes + k ] == i + edgeVerts[nums[j]][0]
					&& tess.indexes[ indexes + k + 1 ] == i + edgeVerts[nums[j]][1] ) {
					break;
				}
			}

			if ( k == 5 ) {
				VectorMA( mid[j], l, minor, v1 );
				VectorMA( mid[j], -l, minor, v2 );
			} else {
				VectorMA( mid[j], -l, minor, v1 );
				VectorMA( mid[j], l, minor, v2 );
			}
		}
	}
}


void RB_DeformTessGeometry( int firstVertex, int numVertexes, int firstIndex, int numIndexes )
{
	int		i;
	const deformStage_t* ds;

	for ( i = 0 ; i < tess.shader->numDeforms ; i++ ) {
		ds = &tess.shader->deforms[ i ];

		switch ( ds->deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_NORMALS:
			RB_CalcDeformNormals( ds, firstVertex, numVertexes );
			break;
		case DEFORM_WAVE:
			RB_CalcDeformVertexes( ds, firstVertex, numVertexes );
			break;
		case DEFORM_BULGE:
			RB_CalcBulgeVertexes( ds, firstVertex, numVertexes );
			break;
		case DEFORM_MOVE:
			RB_CalcMoveVertexes( ds, firstVertex, numVertexes );
			break;
		case DEFORM_PROJECTION_SHADOW:
			//RB_ProjectionShadowDeform();
			break;
		case DEFORM_AUTOSPRITE:
			AutospriteDeform( firstVertex, numVertexes, firstIndex, numIndexes );
			break;
		case DEFORM_AUTOSPRITE2:
			Autosprite2Deform( firstVertex, numVertexes, firstIndex, numIndexes );
			break;
		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
			// @TODO:
			DeformText( backEnd.refdef.text[ds->deformation - DEFORM_TEXT0] );
			break;
		}
	}
}


static void RB_CalcColorFromEntity( unsigned char *dstColors, int numVertexes )
{
	int	i;
	int *pColors = ( int * ) dstColors;
	int c;

	if ( !backEnd.currentEntity )
		return;

	c = * ( int * ) backEnd.currentEntity->e.shaderRGBA;

	for ( i = 0; i < numVertexes; i++, pColors++ )
	{
		*pColors = c;
	}
}


static void RB_CalcColorFromOneMinusEntity( unsigned char *dstColors, int numVertexes )
{
	int	i;
	int *pColors = ( int * ) dstColors;
	unsigned char invModulate[4];
	int c;

	if ( !backEnd.currentEntity )
		return;

	invModulate[0] = 255 - backEnd.currentEntity->e.shaderRGBA[0];
	invModulate[1] = 255 - backEnd.currentEntity->e.shaderRGBA[1];
	invModulate[2] = 255 - backEnd.currentEntity->e.shaderRGBA[2];
	invModulate[3] = 255 - backEnd.currentEntity->e.shaderRGBA[3];	// this trashes alpha, but the AGEN block fixes it

	c = * ( int * ) invModulate;

	for ( i = 0; i < numVertexes; i++, pColors++ )
	{
		*pColors = * ( int * ) invModulate;
	}
}


static void RB_CalcAlphaFromEntity( unsigned char *dstColors, int numVertexes )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < numVertexes; i++, dstColors += 4 )
	{
		*dstColors = backEnd.currentEntity->e.shaderRGBA[3];
	}
}


static void RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors, int numVertexes )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < numVertexes; i++, dstColors += 4 )
	{
		*dstColors = 0xff - backEnd.currentEntity->e.shaderRGBA[3];
	}
}


static void RB_CalcWaveColor( const waveForm_t *wf, unsigned char *dstColors, int numVertexes )
{
	int i;
	int v;
	float glow;
	int *colors = ( int * ) dstColors;
	byte	color[4];

  if ( wf->func == GF_NOISE ) {
		glow = wf->base + R_NoiseGet4f( 0, 0, 0, ( tess.shaderTime + wf->phase ) * wf->frequency ) * wf->amplitude;
	} else {
		glow = EvalWaveForm( wf ) * tr.identityLight;
	}
	
	if ( glow < 0 ) {
		glow = 0;
	}
	else if ( glow > 1 ) {
		glow = 1;
	}

	v = (int)( 255 * glow );
	color[0] = color[1] = color[2] = v;
	color[3] = 255;
	v = *(int *)color;
	
	for ( i = 0; i < numVertexes; i++, colors++ ) {
		*colors = v;
	}
}


static void RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors, int numVertexes )
{
	int i;
	int v;
	float glow;

	glow = EvalWaveFormClamped( wf );

	v = 255 * glow;

	for ( i = 0; i < numVertexes; i++, dstColors += 4 )
	{
		dstColors[3] = v;
	}
}


static void RB_CalcEnvironmentTexCoords( float *st, int firstVertex, int numVertexes ) 
{
	int			i;
	float		*v, *normal;
	vec3_t		viewer, reflected;
	float		d;

	v = tess.xyz[firstVertex];
	normal = tess.normal[firstVertex];
	st += firstVertex * 2;

	for (i = 0 ; i < numVertexes ; i++, v += 4, normal += 4, st += 2 ) 
	{
		VectorSubtract (backEnd.orient.viewOrigin, v, viewer);
		VectorNormalizeFast (viewer);

		d = DotProduct (normal, viewer);

		reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}


static void RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *st, int firstVertex, int numVertexes )
{
	vec4_t* const v = &tess.xyz[firstVertex];
	const double now = ( wf->phase + tess.shaderTime * wf->frequency );

	for ( int i = 0; i < numVertexes; i++, st += 2 )
	{
		st[0] += sin( ( ( v[i][0] + v[i][2] )* 1.0 / 128 * 0.125 + now ) * 2.0 * M_PI ) * wf->amplitude;
		st[1] += sin( ( v[i][1] * 1.0 / 128 * 0.125 + now ) * 2.0 * M_PI ) * wf->amplitude;
	}
}


static void RB_CalcScaleTexCoords( const float scale[2], float *st, int numVertexes )
{
	int i;

	for ( i = 0; i < numVertexes; i++, st += 2 )
	{
		st[0] *= scale[0];
		st[1] *= scale[1];
	}
}


static void RB_CalcScrollTexCoords( const float scrollSpeed[2], float *st, int numVertexes )
{
	int i;
	double timeScale = tess.shaderTime;
	double adjustedScrollS, adjustedScrollT;

	adjustedScrollS = (double)scrollSpeed[0] * timeScale;
	adjustedScrollT = (double)scrollSpeed[1] * timeScale;

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScrollS = adjustedScrollS - floor( adjustedScrollS );
	adjustedScrollT = adjustedScrollT - floor( adjustedScrollT );

	for ( i = 0; i < numVertexes; i++, st += 2 )
	{
		st[0] += adjustedScrollS;
		st[1] += adjustedScrollT;
	}
}


static void RB_CalcRotateTexCoords( float degsPerSecond, float *st, int numVertexes )
{
	const double x = -degsPerSecond * (tess.shaderTime / 360.0) * 2.0 * M_PI;
	const double sinValue = sin(x);
	const double cosValue = cos(x);

    texModInfo_t tmi;
	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords( &tmi, st, numVertexes );
}


/*
** RB_CalcSpecularAlpha
**
** Calculates specular coefficient and places it in the alpha channel
*/
vec3_t lightOrigin = { -960, 1980, 96 };		// FIXME: track dynamically

void RB_CalcSpecularAlpha( unsigned char *alphas, int firstVertex, int numVertexes ) {
	int			i;
	float		*v, *normal;
	vec3_t		viewer,  reflected;
	float		l, d;
	int			b;
	vec3_t		lightDir;

	v = tess.xyz[firstVertex];
	normal = tess.normal[firstVertex];
	alphas += (firstVertex * 4) + 3;

	for (i = 0 ; i < numVertexes ; i++, v += 4, normal += 4, alphas += 4) {
		float ilength;

		VectorSubtract( lightOrigin, v, lightDir );
		VectorNormalizeFast( lightDir );

		// calculate the specular color
		d = DotProduct (normal, lightDir);

		// we don't optimize for the d < 0 case since this tends to
		// cause visual artifacts such as faceted "snapping"
		reflected[0] = normal[0]*2*d - lightDir[0];
		reflected[1] = normal[1]*2*d - lightDir[1];
		reflected[2] = normal[2]*2*d - lightDir[2];

		VectorSubtract (backEnd.orient.viewOrigin, v, viewer);
		ilength = Q_rsqrt( DotProduct( viewer, viewer ) );
		l = DotProduct (reflected, viewer);
		l *= ilength;

		if (l < 0) {
			b = 0;
		} else {
			l = l*l;
			l = l*l;
			b = l * 255;
			if (b > 255) {
				b = 255;
			}
		}

		*alphas = b;
	}
}


static void RB_CalcDiffuseColor( unsigned char *colors, int firstVertex, int numVertexes )
{
	trRefEntity_t* const ent = backEnd.currentEntity;
	if (!ent || !numVertexes)
		return;

	int ambientLightInt = ent->ambientLightInt;
	vec3_t ambientLight, lightDir, directedLight;
	VectorCopy( ent->ambientLight, ambientLight );
	VectorCopy( ent->directedLight, directedLight );
	VectorCopy( ent->lightDir, lightDir );

	float* v = tess.xyz[firstVertex];
	float* normal = tess.normal[firstVertex];

#if defined( QC )
	const float t = 1.0f - ( 1.0f - r_mapGreyscale->value ) * ( 1.0f - tr.viewParms.greyscale );
#else // QC
	const float t = r_mapGreyscale->value;
#endif // QC
	const float ti = 1.0f - t;

	// fix up ambientLightInt for the case where the dot product is 0 or negative
	vec3_t ambientLightMixed;
	const float ambientGrey = 0.299f * ambientLight[0] + 0.587f * ambientLight[1] + 0.114f * ambientLight[2];
	const float ambientGrey_t = ambientGrey * t;
	ambientLightMixed[0] = ambientLight[0] * ti + ambientGrey_t;
	ambientLightMixed[1] = ambientLight[1] * ti + ambientGrey_t;
	ambientLightMixed[2] = ambientLight[2] * ti + ambientGrey_t;
	((byte*)&ambientLightInt)[0] = ambientLightMixed[0];
	((byte*)&ambientLightInt)[1] = ambientLightMixed[1];
	((byte*)&ambientLightInt)[2] = ambientLightMixed[2];
	((byte*)&ambientLightInt)[3] = 0xFF;

	for (int i = 0 ; i < numVertexes ; i++, v += 4, normal += 4) {
		const float incoming = DotProduct( normal, lightDir );
		if ( incoming <= 0 ) {
			*(int*)&colors[i * 4] = ambientLightInt;
			continue;
		} 

		const float inR = ambientLight[0] + incoming * directedLight[0];
		const float inG = ambientLight[1] + incoming * directedLight[1];
		const float inB = ambientLight[2] + incoming * directedLight[2];
		const float grey = 0.299f * inR + 0.587f * inG + 0.114f * inB;
		const float grey_t = grey * t;
		const float outR = inR * ti + grey_t;
		const float outG = inG * ti + grey_t;
		const float outB = inB * ti + grey_t;
		colors[i * 4 + 0] = (byte)min( outR, 255.0f );
		colors[i * 4 + 1] = (byte)min( outG, 255.0f );
		colors[i * 4 + 2] = (byte)min( outB, 255.0f );
		colors[i * 4 + 3] = 255;
	}
}


void R_ComputeColors( const shaderStage_t* pStage, stageVars_t& svars, int firstVertex, int numVertexes )
{
	//
	// rgbGen
	//
	switch ( pStage->rgbGen )
	{
	case CGEN_IDENTITY:
		Com_Memset( &svars.colors[firstVertex], 0xff, numVertexes * 4 );
		break;
	default:
	case CGEN_IDENTITY_LIGHTING:
		Com_Memset( &svars.colors[firstVertex], tr.identityLightByte, numVertexes * 4 );
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( ( unsigned char * ) &svars.colors[firstVertex], firstVertex, numVertexes );
		break;
	case CGEN_CONST:
		for (int i = firstVertex; i < firstVertex + numVertexes; i++) {
			*(int *)svars.colors[i] = *(int *)pStage->constantColor;
		}
		break;
	case CGEN_VERTEX:
		if ( tr.identityLight == 1 )
		{
			Com_Memcpy( &svars.colors[firstVertex], &tess.vertexColors[firstVertex], numVertexes * sizeof( tess.vertexColors[0] ) );
		}
		else
		{
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ )
			{
				svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
				svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
				svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
				svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case CGEN_EXACT_VERTEX:
		Com_Memcpy( &svars.colors[firstVertex], &tess.vertexColors[firstVertex], numVertexes * sizeof( tess.vertexColors[0] ) );
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ )
			{
				svars.colors[i][0] = 255 - tess.vertexColors[i][0];
				svars.colors[i][1] = 255 - tess.vertexColors[i][1];
				svars.colors[i][2] = 255 - tess.vertexColors[i][2];
			}
		}
		else
		{
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ )
			{
				svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
				svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
				svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
			}
		}
		break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &pStage->rgbWave, ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case CGEN_DEBUG_ALPHA:
		for ( int i = firstVertex; i < firstVertex + numVertexes; i++ )
		{
			const byte alpha = tess.vertexColors[i][3];
			svars.colors[i][0] = alpha;
			svars.colors[i][1] = alpha;
			svars.colors[i][2] = alpha;
			svars.colors[i][3] = 255;
		}
		break;
	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( pStage->rgbGen != CGEN_IDENTITY ) {
			if ( ( pStage->rgbGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				 pStage->rgbGen != CGEN_VERTEX ) {
				for ( int i = firstVertex; i < firstVertex + numVertexes; i++ ) {
					svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_CONST:
		if ( pStage->rgbGen != CGEN_CONST ) {
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ ) {
				svars.colors[i][3] = pStage->constantColor[3];
			}
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( ( unsigned char * ) svars.colors, firstVertex, numVertexes );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( ( unsigned char * ) &svars.colors[firstVertex], numVertexes );
		break;
	case AGEN_VERTEX:
		if ( pStage->rgbGen != CGEN_VERTEX ) {
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ ) {
				svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( int i = firstVertex; i < firstVertex + numVertexes; i++ ) {
			svars.colors[i][3] = 255 - tess.vertexColors[i][3];
		}
		break;
	case AGEN_PORTAL:
		{
			for ( int i = firstVertex; i < firstVertex + numVertexes; i++ ) {
				vec3_t v;
				VectorSubtract( tess.xyz[i], backEnd.viewParms.orient.origin, v );
				float len = VectorLength( v ) / tess.shader->portalRange;
				svars.colors[i][3] = (byte)Com_Clamp( 0, 255, len * 255 );
			}
		}
		break;
	}
}


void R_ComputeTexCoords( const shaderStage_t* pStage, stageVars_t& svars, int firstVertex, int numVertexes, qbool ptrOpt )
{
	svars.texcoordsptr = svars.texcoords;

	// generate the base texture coordinates

	switch ( pStage->tcGen )
	{
	case TCGEN_IDENTITY:
		Com_Memset( svars.texcoords + firstVertex, 0, sizeof( float ) * 2 * numVertexes );
		break;

	case TCGEN_TEXTURE:
		if ( !ptrOpt || pStage->numTexMods > 0 || pStage->type == ST_LIGHTMAP )
			Com_Memcpy( svars.texcoords[firstVertex], tess.texCoords[firstVertex], numVertexes * sizeof(vec2_t) );
		else
			svars.texcoordsptr = tess.texCoords;
		break;

	case TCGEN_LIGHTMAP:
		if ( !ptrOpt || pStage->numTexMods > 0 )
			Com_Memcpy( svars.texcoords[firstVertex], tess.texCoords2[firstVertex], numVertexes * sizeof(vec2_t) );
		else
			svars.texcoordsptr = tess.texCoords2;
		break;

	case TCGEN_VECTOR:
		for ( int i = firstVertex ; i < firstVertex + numVertexes ; i++ ) {
			svars.texcoords[i][0] = DotProduct( tess.xyz[i], pStage->tcGenVectors[0] );
			svars.texcoords[i][1] = DotProduct( tess.xyz[i], pStage->tcGenVectors[1] );
		}
		break;

	case TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords( ( float * ) svars.texcoords, firstVertex, numVertexes );
		break;

	case TCGEN_BAD:
		return;
	}

	// then alter for any tcmods

	for ( int i = 0; i < pStage->numTexMods; ++i ) {
		switch ( pStage->texMods[i].type )
		{
		case TMOD_NONE:
			i = TR_MAX_TEXMODS;		// break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords( &pStage->texMods[i].wave, (float*)&svars.texcoords[firstVertex], firstVertex, numVertexes );
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord, (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexCoords( pStage->texMods[i].scroll, (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexCoords( pStage->texMods[i].scale, (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexCoords( &pStage->texMods[i].wave, (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexCoords( &pStage->texMods[i], (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexCoords( pStage->texMods[i].rotateSpeed, (float*)&svars.texcoords[firstVertex], numVertexes );
			break;

		default:
			ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->texMods[i].type, tess.shader->name );
			break;
		}
	}

	// fix up uncorrected lightmap texture coordinates

	if ( pStage->type == ST_LIGHTMAP && pStage->tcGen != TCGEN_LIGHTMAP )
	{
		const shader_t* const shader = tess.shader;

		for ( int i = firstVertex; i < firstVertex + numVertexes; ++i )
		{
			svars.texcoords[i][0] = svars.texcoords[i][0] * shader->lmScale[0] + shader->lmBias[0];
			svars.texcoords[i][1] = svars.texcoords[i][1] * shader->lmScale[1] + shader->lmBias[1];
		}
	}
}

