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
// tr_main.c -- main control flow for each frame

#include "tr_local.h"


trGlobals_t tr;

refimport_t ri;

const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to the back-end's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


static void R_LocalPointToWorld( const vec3_t local, vec3_t world )
{
	world[0] = local[0] * tr.orient.axis[0][0] + local[1] * tr.orient.axis[1][0] + local[2] * tr.orient.axis[2][0] + tr.orient.origin[0];
	world[1] = local[0] * tr.orient.axis[0][1] + local[1] * tr.orient.axis[1][1] + local[2] * tr.orient.axis[2][1] + tr.orient.origin[1];
	world[2] = local[0] * tr.orient.axis[0][2] + local[1] * tr.orient.axis[1][2] + local[2] * tr.orient.axis[2][2] + tr.orient.origin[2];
}


static void R_LocalNormalToWorld( const vec3_t local, vec3_t world )
{
	world[0] = local[0] * tr.orient.axis[0][0] + local[1] * tr.orient.axis[1][0] + local[2] * tr.orient.axis[2][0];
	world[1] = local[0] * tr.orient.axis[0][1] + local[1] * tr.orient.axis[1][1] + local[2] * tr.orient.axis[2][1];
	world[2] = local[0] * tr.orient.axis[0][2] + local[1] * tr.orient.axis[1][2] + local[2] * tr.orient.axis[2][2];
}


// returns CULL_IN, CULL_CLIP, or CULL_OUT

int R_CullLocalBox( const vec3_t bounds[2] )
{
	int		i, j;
	vec3_t	transformed[8];
	float	dists[8];
	vec3_t	v;
	cplane_t	*frust;
	int			anyBack;
	int			front, back;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	// transform into world space
	for (i = 0 ; i < 8 ; i++) {
		v[0] = bounds[i&1][0];
		v[1] = bounds[(i>>1)&1][1];
		v[2] = bounds[(i>>2)&1][2];

		VectorCopy( tr.orient.origin, transformed[i] );
		VectorMA( transformed[i], v[0], tr.orient.axis[0], transformed[i] );
		VectorMA( transformed[i], v[1], tr.orient.axis[1], transformed[i] );
		VectorMA( transformed[i], v[2], tr.orient.axis[2], transformed[i] );
	}

	// check against frustum planes
	anyBack = 0;
	for (i = 0 ; i < 4 ; i++) {
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for (j = 0 ; j < 8 ; j++) {
			dists[j] = DotProduct(transformed[j], frust->normal);
			if ( dists[j] > frust->dist ) {
				front = 1;
				if ( back ) {
					break;		// a point is in front
				}
			} else {
				back = 1;
			}
		}
		if ( !front ) {
			// all points were behind one of the planes
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if ( !anyBack ) {
		return CULL_IN;		// completely inside frustum
	}

	return CULL_CLIP;		// partially clipped
}


int R_CullPointAndRadius( const vec3_t pt, float radius )
{
	qbool mightBeClipped = qfalse;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	// check against frustum planes
	for (int i = 0; i < 4; ++i) 
	{
		const cplane_t* frust = &tr.viewParms.frustum[i];
		float dist = DotProduct( pt, frust->normal) - frust->dist;

		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius )
		{
			mightBeClipped = qtrue;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;		// completely inside frustum
}


int R_CullLocalPointAndRadius( const vec3_t pt, float radius )
{
	vec3_t transformed;
	R_LocalPointToWorld( pt, transformed );
	return R_CullPointAndRadius( transformed, radius );
}


/*
==========================
R_TransformModelToClip

==========================
*/
void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		eye[i] = 
			src[0] * modelMatrix[ i + 0 * 4 ] +
			src[1] * modelMatrix[ i + 1 * 4 ] +
			src[2] * modelMatrix[ i + 2 * 4 ] +
			1 * modelMatrix[ i + 3 * 4 ];
	}

	for ( i = 0 ; i < 4 ; i++ ) {
		dst[i] = 
			eye[0] * projectionMatrix[ i + 0 * 4 ] +
			eye[1] * projectionMatrix[ i + 1 * 4 ] +
			eye[2] * projectionMatrix[ i + 2 * 4 ] +
			eye[3] * projectionMatrix[ i + 3 * 4 ];
	}
}

/*
==========================
R_TransformClipToWindow

==========================
*/
void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->viewportWidth;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->viewportHeight;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}


void R_MultMatrix( const float *a, const float *b, float *out )
{
	for ( int i = 0 ; i < 4 ; i++ ) {
		for ( int j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ] +
				a [ i * 4 + 1 ] * b [ 1 * 4 + j ] +
				a [ i * 4 + 2 ] * b [ 2 * 4 + j ] +
				a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}


void R_InvMatrix( const float* m, float* invOut )
{
	float inv[16];

	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if ( det == 0.0f ) {
		return;
	}

	const float invDet = 1.0f / det;
	for ( int i = 0; i < 16; ++i ) {
		invOut[i] = inv[i] * invDet;
	}
}


void R_TransposeMatrix( const float* in, float* out )
{
	out[ 0] = in[ 0];
	out[ 4] = in[ 1];
	out[ 8] = in[ 2];
	out[12] = in[ 3];

	out[ 1] = in[ 4];
	out[ 5] = in[ 5];
	out[ 9] = in[ 6];
	out[13] = in[ 7];

	out[ 2] = in[ 8];
	out[ 6] = in[ 9];
	out[10] = in[10];
	out[14] = in[11];

	out[ 3] = in[12];
	out[ 7] = in[13];
	out[11] = in[14];
	out[15] = in[15];
}


void R_CameraPositionFromMatrix( const float* modelView, vec3_t cameraPos )
{
	float modelViewT[16];
	R_TransposeMatrix( modelView, modelViewT );

	// plane normals 
	vec3_t n1, n2, n3;
	VectorCopy( modelViewT + 0 * 4, n1 );
	VectorCopy( modelViewT + 1 * 4, n2 );
	VectorCopy( modelViewT + 2 * 4, n3 );

	// plane distances
	const float d1 = modelViewT[0 * 4 + 3];
	const float d2 = modelViewT[1 * 4 + 3];
	const float d3 = modelViewT[2 * 4 + 3];

	// intersection of the 3 planes
	vec3_t n2n3, n3n1, n1n2;
	CrossProduct( n2, n3, n2n3 );
	CrossProduct( n3, n1, n3n1 );
	CrossProduct( n1, n2, n1n2 );

	// top = (n2n3 * d1) + (n3n1 * d2) + (n1n2 * d3)
	vec3_t top;
	VectorMA( vec3_origin, d1, n2n3, top );
	VectorMA( top, d2, n3n1, top );
	VectorMA( top, d3, n1n2, top );
	const float denom = DotProduct( n1, n2n3 );
	VectorScale( top, -1.0f / denom, cameraPos );
}


void R_CameraAxisVectorsFromMatrix( const float* modelView, vec3_t axisX, vec3_t axisY, vec3_t axisZ )
{
	axisX[0] = modelView[ 0];
	axisX[1] = modelView[ 4];
	axisX[2] = modelView[ 8];

	axisY[0] = modelView[ 1];
	axisY[1] = modelView[ 5];
	axisY[2] = modelView[ 9];

	axisZ[0] = modelView[ 2];
	axisZ[1] = modelView[ 6];
	axisZ[2] = modelView[10];
}


void R_MakeIdentityMatrix( float* m )
{
	m[ 0] = 1.0f;
	m[ 1] = 0.0f;
	m[ 2] = 0.0f;
	m[ 3] = 0.0f;
	m[ 4] = 0.0f;
	m[ 5] = 1.0f;
	m[ 6] = 0.0f;
	m[ 7] = 0.0f;
	m[ 8] = 0.0f;
	m[ 9] = 0.0f;
	m[10] = 1.0f;
	m[11] = 0.0f;
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}


void R_MakeOrthoProjectionMatrix( float* m, float w, float h )
{
	// 2/(r-l)      0            0           0
	// 0            2/(t-b)      0           0
	// 0            0            1/(zf-zn)   0
	// (l+r)/(l-r)  (t+b)/(b-t)  zn/(zn-zf)  1
	const float n = 0.0f;
	const float f = 2.0f;
	m[ 0] = 2.0f / w;
	m[ 4] = 0.0f;
	m[ 8] = 0.0f;
	m[12] = -1.0f;
	m[ 1] = 0.0f;
	m[ 5] = -2.0f / h;
	m[ 9] = 0.0f;
	m[13] = 1.0f;
	m[ 2] = 0.0f;
	m[ 6] = 0.0f;
	m[10] = 1.0f / (f - n);
	m[14] = 0.0f;
	m[ 3] = 0.0f;
	m[ 7] = 0.0f;
	m[11] = n / (n - f);
	m[15] = 1.0f;
}


/*
=================
R_RotateForEntity

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateForEntity( const trRefEntity_t* ent, const viewParms_t* viewParms, orientationr_t* orient )
{
	float	glMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*orient = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, orient->origin );

	VectorCopy( ent->e.axis[0], orient->axis[0] );
	VectorCopy( ent->e.axis[1], orient->axis[1] );
	VectorCopy( ent->e.axis[2], orient->axis[2] );

	glMatrix[0] = orient->axis[0][0];
	glMatrix[4] = orient->axis[1][0];
	glMatrix[8] = orient->axis[2][0];
	glMatrix[12] = orient->origin[0];

	glMatrix[1] = orient->axis[0][1];
	glMatrix[5] = orient->axis[1][1];
	glMatrix[9] = orient->axis[2][1];
	glMatrix[13] = orient->origin[1];

	glMatrix[2] = orient->axis[0][2];
	glMatrix[6] = orient->axis[1][2];
	glMatrix[10] = orient->axis[2][2];
	glMatrix[14] = orient->origin[2];

	glMatrix[3] = 0;
	glMatrix[7] = 0;
	glMatrix[11] = 0;
	glMatrix[15] = 1;

	R_MultMatrix( glMatrix, viewParms->world.modelMatrix, orient->modelMatrix );

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract( viewParms->orient.origin, orient->origin, delta );

	// compensate for scale in the axes if necessary
	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0f / axisLength;
		}
	} else {
		axisLength = 1.0f;
	}

	orient->viewOrigin[0] = DotProduct( delta, orient->axis[0] ) * axisLength;
	orient->viewOrigin[1] = DotProduct( delta, orient->axis[1] ) * axisLength;
	orient->viewOrigin[2] = DotProduct( delta, orient->axis[2] ) * axisLength;
}


// sets up the modelview matrix for a given viewParm

static void R_RotateForViewer()
{
	float	viewerMatrix[16];
	vec3_t	origin;

	Com_Memset( &tr.orient, 0, sizeof(tr.orient) );
	tr.orient.axis[0][0] = 1;
	tr.orient.axis[1][1] = 1;
	tr.orient.axis[2][2] = 1;
	VectorCopy( tr.viewParms.orient.origin, tr.orient.viewOrigin );

	// transform by the camera placement
	VectorCopy( tr.viewParms.orient.origin, origin );

	viewerMatrix[0] = tr.viewParms.orient.axis[0][0];
	viewerMatrix[4] = tr.viewParms.orient.axis[0][1];
	viewerMatrix[8] = tr.viewParms.orient.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.orient.axis[1][0];
	viewerMatrix[5] = tr.viewParms.orient.axis[1][1];
	viewerMatrix[9] = tr.viewParms.orient.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.orient.axis[2][0];
	viewerMatrix[6] = tr.viewParms.orient.axis[2][1];
	viewerMatrix[10] = tr.viewParms.orient.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	// convert from our coordinate system (looking down X)
	// to the back-end's coordinate system (looking down -Z)
	R_MultMatrix( viewerMatrix, s_flipMatrix, tr.orient.modelMatrix );

	tr.viewParms.world = tr.orient;
}


static void SetFarClip()
{
	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	// set far clipping plane dynamically

	float farthestCornerDistance = 0;
	for (int i = 0; i < 8; ++i)
	{
		vec3_t v;
		v[0] = (i & 1) ? tr.viewParms.visBounds[0][0] : tr.viewParms.visBounds[1][0];
		v[1] = (i & 2) ? tr.viewParms.visBounds[0][1] : tr.viewParms.visBounds[1][1];
		v[2] = (i & 4) ? tr.viewParms.visBounds[0][2] : tr.viewParms.visBounds[1][2];

		float d = DistanceSquared( v, tr.viewParms.orient.origin );
		if ( d > farthestCornerDistance )
		{
			farthestCornerDistance = d;
		}
	}

	tr.viewParms.zFar = sqrt( farthestCornerDistance );
}


static void R_SetupProjection()
{
	float	width, height, depth;
	float	zNear, zFar;

	// dynamically compute far clip plane distance
	SetFarClip();

	//
	// set up projection matrix
	//
	zNear = 1.0f;
	zFar  = tr.viewParms.zFar;

	height = 2.0f * zNear * tan( tr.refdef.fov_y * M_PI / 360.0f );
	width  = 2.0f * zNear * tan( tr.refdef.fov_x * M_PI / 360.0f );
	depth  = zFar - zNear;

	tr.viewParms.projectionMatrix[0] = 2 * zNear / width;
	tr.viewParms.projectionMatrix[4] = 0;
	tr.viewParms.projectionMatrix[8] = 0;
	tr.viewParms.projectionMatrix[12] = 0;

	tr.viewParms.projectionMatrix[1] = 0;
	tr.viewParms.projectionMatrix[5] = 2 * zNear / height;
	tr.viewParms.projectionMatrix[9] = 0;
	tr.viewParms.projectionMatrix[13] = 0;

	tr.viewParms.projectionMatrix[2] = 0;
	tr.viewParms.projectionMatrix[6] = 0;
	tr.viewParms.projectionMatrix[10] = zNear / depth;
	tr.viewParms.projectionMatrix[14] = zFar * zNear / depth;

	tr.viewParms.projectionMatrix[3] = 0;
	tr.viewParms.projectionMatrix[7] = 0;
	tr.viewParms.projectionMatrix[11] = -1;
	tr.viewParms.projectionMatrix[15] = 0;
}


// set up the culling frustum planes for the current view

static void R_SetupFrustum()
{
	float	ang, xs, xc;

	ang = tr.viewParms.fovX / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.orient.axis[0], xs, tr.viewParms.frustum[0].normal );
	VectorMA( tr.viewParms.frustum[0].normal, xc, tr.viewParms.orient.axis[1], tr.viewParms.frustum[0].normal );

	VectorScale( tr.viewParms.orient.axis[0], xs, tr.viewParms.frustum[1].normal );
	VectorMA( tr.viewParms.frustum[1].normal, -xc, tr.viewParms.orient.axis[1], tr.viewParms.frustum[1].normal );

	ang = tr.viewParms.fovY / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.orient.axis[0], xs, tr.viewParms.frustum[2].normal );
	VectorMA( tr.viewParms.frustum[2].normal, xc, tr.viewParms.orient.axis[2], tr.viewParms.frustum[2].normal );

	VectorScale( tr.viewParms.orient.axis[0], xs, tr.viewParms.frustum[3].normal );
	VectorMA( tr.viewParms.frustum[3].normal, -xc, tr.viewParms.orient.axis[2], tr.viewParms.frustum[3].normal );

	for (int i = 0; i < 4; ++i) {
		tr.viewParms.frustum[i].type = PLANE_NON_AXIAL;
		tr.viewParms.frustum[i].dist = DotProduct (tr.viewParms.orient.origin, tr.viewParms.frustum[i].normal);
		SetPlaneSignbits( &tr.viewParms.frustum[i] );
	}
}


static void R_MirrorPoint( const vec3_t in, const orientation_t* surface, const orientation_t* camera, vec3_t out )
{
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct( local, surface->axis[i] );
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}


static void R_MirrorVector( const vec3_t in, const orientation_t* surface, const orientation_t* camera, vec3_t out )
{
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct( in, surface->axis[i] );
		VectorMA( out, d, camera->axis[i], out );
	}
}


static void R_PlaneForSurface( const surfaceType_t* surfType, cplane_t* plane )
{
	vec4_t plane4;

	if (!surfType) {
		Com_Memset( plane, 0, sizeof(*plane) );
		plane->normal[0] = 1;
		return;
	}

	switch (*surfType) {
	case SF_FACE:
		*plane = ((const srfSurfaceFace_t*)surfType)->plane;
		return;

	case SF_TRIANGLES: {
		const srfTriangles_t* tri = (const srfTriangles_t*)surfType;
		const srfVert_t* v1 = tri->verts + tri->indexes[0];
		const srfVert_t* v2 = tri->verts + tri->indexes[1];
		const srfVert_t* v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal );
		plane->dist = plane4[3];
	} return;

	case SF_POLY: {
		const srfPoly_t* poly = (const srfPoly_t*)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal );
		plane->dist = plane4[3];
	} return;

	default:
		Com_Memset( plane, 0, sizeof(*plane) );
		plane->normal[0] = 1;
		return;
	}
}

/*
=================
R_GetPortalOrientation

entityNum is the entity that the portal surface is a part of, which may
be moving and rotating.

Returns qtrue if it should be mirrored
=================
*/
qbool R_GetPortalOrientations( drawSurf_t *drawSurf, int entityNum,
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, qbool *mirror ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		transformed;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != ENTITYNUM_WORLD ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.orient );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.orient.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.orient.origin );
	} else {
		plane = originalPlane;
	}

	VectorCopy( plane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// get the pvsOrigin from the entity
		VectorCopy( e->e.oldorigin, pvsOrigin );

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*mirror = qtrue;
			return qtrue;
		}

		// project the origin onto the surface plane to get
		// an origin point we can rotate around
		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );

		// now get the camera origin and orientation
		VectorCopy( e->e.oldorigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		// optionally rotate
		if ( e->e.oldframe ) {
			// if a speed is specified
			if ( e->e.frame ) {
				// continuous rotate
				d = (tr.refdef.time/1000.0f) * e->e.frame;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			} else {
				// bobbing rotate, with skinNum being the rotation offset
				d = sin( tr.refdef.time * 0.003f );
				d = e->e.skinNum + d * 4;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			}
		}
		else if ( e->e.skinNum ) {
			d = e->e.skinNum;
			VectorCopy( camera->axis[1], transformed );
			RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
			CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
		}
		*mirror = qfalse;
		return qtrue;
	}

	// if we didn't locate a portal entity, don't render anything.
	// We don't want to just treat it as a mirror, because without a
	// portal entity the server won't have communicated a proper entity set
	// in the snapshot

	// unfortunately, with local movement prediction it is easily possible
	// to see a surface before the server has communicated the matching
	// portal surface entity, so we don't want to print anything here...

	//ri.Printf( PRINT_ALL, "Portal surface without a portal entity\n" );

	return qfalse;
}

static qbool IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	float		d;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != ENTITYNUM_WORLD )
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.orient );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.orient.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.orient.origin );
	}
	else
	{
		plane = originalPlane;
	}

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal,
	// oldorigin will be the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		const trRefEntity_t* e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// if the entity is just a mirror, don't use as a camera point
		return VectorCompare( e->e.origin, e->e.oldorigin );
	}

	return qfalse;
}


// determines if a surface is COMPLETELY offscreen

static qbool SurfIsOffscreen( const drawSurf_t* drawSurf )
{
	float shortest = 100000000;
	int entityNum;
	int numTriangles;
	const shader_t *shader;
	vec4_t clip, eye;
	int i;
	unsigned int pointAnd = (unsigned int)~0;

	R_RotateForViewer();

	R_DecomposeSort( drawSurf->sort, &entityNum, &shader );
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = shader;
	tess.xstages = (const shaderStage_t**)shader->stages;
	R_TessellateSurface( drawSurf->surface );

	assert( tess.numVertexes < 128 );

	for ( i = 0; i < tess.numVertexes; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( tess.xyz[i], tr.orient.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
	}

	// trivially reject
	if ( pointAnd )
	{
		return qtrue;
	}

	// determine if this surface is backfaced and also determine the distance
	// to the nearest vertex so we can cull based on portal range.  Culling
	// based on vertex distance isn't 100% correct (we should be checking for
	// range to the surface), but it's good enough for the types of portals
	// we have in the game right now.
	numTriangles = tess.numIndexes / 3;

	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		vec3_t normal;
		float dot;
		float len;

		VectorSubtract( tess.xyz[tess.indexes[i]], tr.viewParms.orient.origin, normal );

		len = VectorLengthSquared( normal );			// lose the sqrt
		if ( len < shortest )
		{
			shortest = len;
		}

		if ( ( dot = DotProduct( normal, tess.normal[tess.indexes[i]] ) ) >= 0 )
		{
			numTriangles--;
		}
	}
	if ( !numTriangles )
	{
		return qtrue;
	}

	// mirrors can early out at this point, since we don't do a fade over distance
	// with them (although we could)
	if ( IsMirror( drawSurf, entityNum ) )
	{
		return qfalse;
	}

	if ( shortest > (tess.shader->portalRange*tess.shader->portalRange) )
	{
		return qtrue;
	}

	return qfalse;
}


// returns true if another view has been rendered

static qbool R_MirrorViewBySurface( drawSurf_t* drawSurf, int entityNum )
{
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;

	// don't recursively mirror
	if (tr.viewParms.isPortal) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n" );
		return qfalse;
	}

	// r_fastsky's "mindless" blit over the entire screen will destroy portal views
	if ( r_fastsky->integer || r_noportals->integer ) {
		return qfalse;
	}

	// trivially reject portal/mirror
	if ( SurfIsOffscreen( drawSurf ) ) {
		return qfalse;
	}

	// save old viewParms so we can return to it after the mirror view
	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.isPortal = qtrue;
	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera,
			newParms.pvsOrigin, &newParms.isMirror ) ) {
		return qfalse;		// bad portal, no portalentity
	}

	R_MirrorPoint( oldParms.orient.origin, &surface, &camera, newParms.orient.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );

	R_MirrorVector( oldParms.orient.axis[0], &surface, &camera, newParms.orient.axis[0] );
	R_MirrorVector( oldParms.orient.axis[1], &surface, &camera, newParms.orient.axis[1] );
	R_MirrorVector( oldParms.orient.axis[2], &surface, &camera, newParms.orient.axis[2] );

	// OPTIMIZE: restrict the viewport on the mirrored view

	// render the mirror view
	R_RenderScene( &newParms );

	tr.viewParms = oldParms;

	return qtrue;
}


/*
==========================================================================================

DRAWSURF SORTING

==========================================================================================
*/

/*
===============
R_Radix
===============
*/
static ID_INLINE void R_Radix( int keyByte, int size, drawSurf_t *source, drawSurf_t *dest )
{
  int           count[ 256 ] = { 0 };
  int           index[ 256 ];
  int           i;
  unsigned char *sortKey = NULL;
  unsigned char *end = NULL;

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + keyByte;
  end = sortKey + ( size * sizeof( drawSurf_t ) );
  for( ; sortKey < end; sortKey += sizeof( drawSurf_t ) )
    ++count[ *sortKey ];

  index[ 0 ] = 0;

  for( i = 1; i < 256; ++i )
    index[ i ] = index[ i - 1 ] + count[ i - 1 ];

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + keyByte;
  for( i = 0; i < size; ++i, sortKey += sizeof( drawSurf_t ) )
    dest[ index[ *sortKey ]++ ] = source[ i ];
}

/*
===============
R_RadixSort

Radix sort with 4 byte size buckets
===============
*/
static void R_RadixSort( drawSurf_t *source, int size )
{
  static drawSurf_t scratch[ MAX_DRAWSURFS ];
#ifdef Q3_LITTLE_ENDIAN
  R_Radix( 0, size, source, scratch );
  R_Radix( 1, size, scratch, source );
  R_Radix( 2, size, source, scratch );
  R_Radix( 3, size, scratch, source );
  R_Radix( 4, size, source, scratch );
  R_Radix( 5, size, scratch, source );
#else
  R_Radix( 5, size, source, scratch );
  R_Radix( 4, size, scratch, source );
  R_Radix( 3, size, source, scratch );
  R_Radix( 2, size, scratch, source );
  R_Radix( 1, size, source, scratch );
  R_Radix( 0, size, scratch, source );
#endif //Q3_LITTLE_ENDIAN
}


// Philip Erdelsky gets all the credit for this one...

static void R_SortLitsurfs( dlight_t* dl )
{
	struct litSurf_tape {
		litSurf_t *first, *last;
		unsigned count;
	} tape[4];

	// distribute the records alternately to tape[0] and tape[1]

	tape[0].count = tape[1].count = 0;
	tape[0].first = tape[1].first = NULL;

	int base = 0;
	litSurf_t* p = dl->head;

	while (p) {
		litSurf_t* next = p->next;
		p->next = tape[base].first;
		tape[base].first = p;
		tape[base].count++;
		p = next;
		base ^= 1;
	}

	// merge from the two active tapes into the two idle ones
	// doubling the number of records and pingponging the tape sets as we go

	unsigned block_size = 1;
	for ( base = 0; tape[base+1].count; base ^= 2, block_size <<= 1 )
	{
		litSurf_tape* tape0 = tape + base;
		litSurf_tape* tape1 = tape + base + 1;
		int dest = base ^ 2;

		tape[dest].count = tape[dest+1].count = 0;
		for (; tape0->count; dest ^= 1)
		{
			litSurf_tape* output_tape = tape + dest;
			unsigned n0, n1;
			n0 = n1 = block_size;

			while (1)
			{
				litSurf_tape* chosen_tape;
				if (n0 == 0 || tape0->count == 0)
				{
					if (n1 == 0 || tape1->count == 0)
						break;
					chosen_tape = tape1;
					n1--;
				}
				else if (n1 == 0 || tape1->count == 0)
				{
					chosen_tape = tape0;
					n0--;
				}
				else if (tape0->first->sort > tape1->first->sort)
				{
					chosen_tape = tape1;
					n1--;
				}
				else
				{
					chosen_tape = tape0;
					n0--;
				}
				chosen_tape->count--;
				p = chosen_tape->first;
				chosen_tape->first = p->next;
				if (output_tape->count == 0)
					output_tape->first = p;
				else
					output_tape->last->next = p;
				output_tape->last = p;
				output_tape->count++;
			}
		}
	}

	if (tape[base].count > 1)
		tape[base].last->next = NULL;

	dl->head = tape[base].first;
}


///////////////////////////////////////////////////////////////


static float SurfGreyscaleAmount( const shader_t* shader )
{
	if (!tr.worldSurface)
		return 0.0f;
#if defined( QC )
	float greyscale = shader->greyscaleCTF ? r_mapGreyscaleCTF->value : r_mapGreyscale->value;
	return 1.0f - ( 1.0f - greyscale ) * ( 1.0f - tr.viewParms.greyscale );
#else  // QC
	return shader->greyscaleCTF ? r_mapGreyscaleCTF->value : r_mapGreyscale->value;
#endif // QC
}


void R_AddDrawSurf( const surfaceType_t* surface, const shader_t* shader, int staticGeoChunk, int zppFirstIndex, int zppIndexCount, float radiusOverZ )
{
	if (tr.refdef.numDrawSurfs >= MAX_DRAWSURFS)
		return;

	drawSurf_t* const drawSurf = &tr.refdef.drawSurfs[tr.refdef.numDrawSurfs++];
	drawSurf->sort = R_ComposeSort( tr.currentEntityNum, shader, staticGeoChunk );
	drawSurf->surface = surface;
	drawSurf->model = tr.currentModel != NULL ? tr.currentModel->index : 0;
	drawSurf->shaderNum = shader->index;
	drawSurf->greyscale = SurfGreyscaleAmount( shader );
	drawSurf->staticGeoChunk = staticGeoChunk;
	drawSurf->zppFirstIndex = zppFirstIndex;
	drawSurf->zppIndexCount = zppIndexCount;
	drawSurf->radiusOverZ = radiusOverZ;
}


void R_AddLitSurf( const surfaceType_t* surface, const shader_t* shader, int staticGeoChunk )
{
	if (tr.refdef.numLitSurfs >= MAX_DRAWSURFS)
		return;

	tr.pc[RF_LIT_SURFS]++;

	litSurf_t* const litSurf = &tr.refdef.litSurfs[tr.refdef.numLitSurfs++];
	litSurf->sort = R_ComposeLitSort( tr.currentEntityNum, shader, staticGeoChunk );
	litSurf->surface = surface;
	litSurf->shaderNum = shader->index;
	litSurf->greyscale = SurfGreyscaleAmount( shader );
	litSurf->staticGeoChunk = staticGeoChunk;

	if (!tr.light->head)
		tr.light->head = litSurf;
	if (tr.light->tail)
		tr.light->tail->next = litSurf;

	tr.light->tail = litSurf;
	tr.light->tail->next = 0;
}


uint64_t R_ComposeSort( int entityNum, const shader_t* shader, int staticGeoChunk )
{
	return
		( (uint64_t)entityNum << DRAWSORT_ENTITY_INDEX ) |
		( (uint64_t)shader->sortedIndex << DRAWSORT_SHADER_INDEX ) |
		( (uint64_t)( staticGeoChunk > 0 ? 0 : 1 ) << DRAWSORT_STATICGEO_INDEX ) |
		( (uint64_t)( shader->pipelines[0].pipeline ) << DRAWSORT_PSO_INDEX ) |
		( (uint64_t)( shader->isSky ? 1 : 0 ) << DRAWSORT_SKY_INDEX ) |
		( (uint64_t)( shader->isAlphaTestedOpaque ? 1 : 0 ) << DRAWSORT_ALPHATEST_INDEX ) |
		( (uint64_t)( shader->isOpaque ? 0 : 1 ) << DRAWSORT_OPAQUE_INDEX ) |
		( (uint64_t)( shader->sort == SS_PORTAL ? 0 : 1 ) << DRAWSORT_PORTAL_INDEX );
}


void R_DecomposeSort( uint64_t sort, int* entityNum, const shader_t** shader )
{
	*entityNum = ( sort >> DRAWSORT_ENTITY_INDEX ) & MAX_REFENTITIES;
	*shader = tr.sortedShaders[ ( sort >> DRAWSORT_SHADER_INDEX ) & (MAX_SHADERS-1) ];
}


uint32_t R_ComposeLitSort( int entityNum, const shader_t* shader, int staticGeoChunk )
{
	const int stageIndex = max( shader->lightingStages[ST_DIFFUSE], 0 );
	const int depthTestEquals = ( shader->stages[stageIndex]->stateBits & GLS_DEPTHFUNC_EQUAL ) != 0;

	return
		( (uint32_t)entityNum << LITSORT_ENTITY_INDEX ) |
		( (uint32_t)shader->sortedIndex << LITSORT_SHADER_INDEX ) |
		( (uint32_t)( staticGeoChunk > 0 ? 0 : 1 ) << LITSORT_STATICGEO_INDEX ) |
		( (uint32_t)shader->cullType << LITSORT_CULLTYPE_INDEX ) |
		( (uint32_t)shader->polygonOffset << LITSORT_POLYGONOFFSET_INDEX ) |
		( (uint32_t)depthTestEquals << LITSORT_DEPTHTEST_INDEX );
}


void R_DecomposeLitSort( uint32_t sort, int* entityNum, const shader_t** shader )
{
	*entityNum = ( sort >> LITSORT_ENTITY_INDEX ) & MAX_REFENTITIES;
	*shader = tr.sortedShaders[ ( sort >> LITSORT_SHADER_INDEX ) & (MAX_SHADERS-1) ];
}


static float R_ComputePointDepth( const vec3_t point, const float* modelMatrix )
{
	return -(
		modelMatrix[2 + 0 * 4] * point[0] +
		modelMatrix[2 + 1 * 4] * point[1] +
		modelMatrix[2 + 2 * 4] * point[2] +
		modelMatrix[2 + 3 * 4]
		);
}


static float R_ComputeEntityPointDepth( const vec3_t point, int entityNum )
{
	orientationr_t orient;
	if ( entityNum != ENTITYNUM_WORLD )
		R_RotateForEntity( &tr.refdef.entities[entityNum], &tr.viewParms, &orient );
	else
		orient = tr.viewParms.world;

	return R_ComputePointDepth( point, orient.modelMatrix );
}


static float R_ComputeSurfaceDepth( const surfaceType_t* surf, int entityNum, qhandle_t model )
{
	const float back  =  999666.0f;
	const float front = -999666.0f;

	if ( *surf == SF_ENTITY ) {
		const refEntity_t* const ent = &tr.refdef.entities[entityNum].e;
		if ( ent->reType == RT_SPRITE ) // CPMA: simple items, rocket explosions, ...
			return R_ComputeEntityPointDepth( ent->origin, entityNum );
		if ( ent->reType == RT_LIGHTNING ) // CPMA: first-person lightning gun beam
			return front;
		// note that RT_MODEL not being checked isn't an omission, it's not needed
		return back;
	}

	if ( *surf == SF_POLY ) { // CPMA: impact marks, rocket smoke, ...
		const srfPoly_t* const poly = (const srfPoly_t*)surf;
		return R_ComputeEntityPointDepth( poly->localOrigin, entityNum );
	}

	if ( *surf == SF_MD3 ) { // CPMA: spawn points, rocket projectiles, ...
		vec3_t mins, maxs, midPoint;
		R_ModelBounds( model, mins, maxs );
		VectorAdd( mins, maxs, midPoint );
		VectorScale( midPoint, 0.5f, midPoint );
		return R_ComputeEntityPointDepth( midPoint, entityNum );
	}

	// If we don't sort them,  we let "static" surfaces be drawn behind the "dynamic" ones.
	// This helps avoid inconsistent-looking results like CPMA simple items and
	// large enough transparent liquid pools (e.g. dropped weapons in the cpm18r acid).
	if ( r_transpSort->integer == 0 )
		return back;

	if ( *surf == SF_FACE ) { // cpm25 water
		const srfSurfaceFace_t* const face = (const srfSurfaceFace_t*)surf;
		return R_ComputeEntityPointDepth( face->localOrigin, entityNum );
	}

	if ( *surf == SF_GRID ) { // hektik_b3 item markers
		const srfGridMesh_t* const grid = (const srfGridMesh_t*)surf;
		return R_ComputeEntityPointDepth( grid->localOrigin, entityNum );
	}

	if ( *surf == SF_TRIANGLES ) { // cpm18r acid
		const srfTriangles_t* const tri = (const srfTriangles_t*)surf;
		return R_ComputeEntityPointDepth( tri->localOrigin, entityNum );
	}

	return back;
}


/*
A few notes on transparency handling:

a) User-specified blend modes and user-created data mean we can't robustly substitute blend modes
   with better alternatives that are commutative (e.g. pre-multiplied alpha instead of the "standard" blend).
   The amount of corner cases to handle would be a headache and textures would need to be modified.
   I'm not even sure if it's possible. I'd gladly be proven wrong, though.

b) The code currently sorts surfaces (triangle groups) back to front.
   Sorting individual triangles would obviously be better for many scenarios,
   but it would still not be correct at all times (e.g. intersecting triangles, 3-way overlaps).

c) What we really want is true order-independent transparency (OIT).
   There are numerous techniques, but this 2-step method is the most promising avenue:
   1. Render transparent surfaces into per-pixel linked lists.
   2. Do a single "full-screen" pass which sorts and resolves each pixel's list.
   This requires support for atomic shader operations.

   When atomic shader operations are not available, we could fall back to:
   - The current approach.
   - Per-pixel fixed-size arrays (there are several methods).
   - Depth peeling (there are also several methods).
*/
static int R_CompareDrawSurf( const void* aPtr, const void* bPtr )
{
	const drawSurf_t* a = ( const drawSurf_t* )aPtr;
	const drawSurf_t* b = ( const drawSurf_t* )bPtr;
	if ( a->shaderSort < b->shaderSort )
		return -1;
	if ( a->shaderSort > b->shaderSort )
		return 1;

	if ( a->depth > b->depth )
		return -1;
	if ( a->depth < b->depth )
		return 1;

	return a->index - b->index;
}


// same thing but ignoring the sort key since some maps get this so wrong
// example: a grate shader in bones_fkd_b4 has a sort key value of 16,
//          so it will draw in front of pretty much everything else
static int R_CompareDrawSurfNoKey( const void* aPtr, const void* bPtr )
{
	const drawSurf_t* a = ( const drawSurf_t* )aPtr;
	const drawSurf_t* b = ( const drawSurf_t* )bPtr;
	if ( a->depth > b->depth )
		return -1;
	if ( a->depth < b->depth )
		return 1;

	return a->index - b->index;
}


static void R_SortDrawSurfs( int firstDrawSurf, int firstLitSurf )
{
	const int numDrawSurfs = tr.refdef.numDrawSurfs - firstDrawSurf;
	drawSurf_t* const drawSurfs = tr.refdef.drawSurfs + firstDrawSurf;

	// it is possible for some views to not have any surfaces
	if ( numDrawSurfs < 1 ) {
		// we still need to add it for hyperspace cases
		R_AddDrawSurfCmd( drawSurfs, 0, 0 );
		return;
	}

	R_RadixSort( drawSurfs, numDrawSurfs );

	// check for any pass through drawing,
	// which may cause another view to be rendered first
	const shader_t* shader;
	int entityNum;
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		R_DecomposeSort( (drawSurfs + i)->sort, &entityNum, &shader );

		// if the mirror was completely clipped away, we may need to check another surface
		if ( shader->sort == SS_PORTAL && R_MirrorViewBySurface( (drawSurfs + i), entityNum ) ) {
			// this is a debug option to see exactly what is being mirrored
			if ( r_portalOnly->integer ) {
				return;
			}
		}
	}

	// compute the average camera depth of all transparent surfaces
	int numTranspSurfs = 0;
	for ( int i = numDrawSurfs - 1; i >= 0; --i ) {
		R_DecomposeSort( (drawSurfs+i)->sort, &entityNum, &shader );

		if ( shader->sort <= SS_OPAQUE ) {
			numTranspSurfs = numDrawSurfs - i - 1;
			break;
		}

		drawSurfs[i].depth = R_ComputeSurfaceDepth( drawSurfs[i].surface, entityNum, drawSurfs[i].model );
		drawSurfs[i].index = i;
		drawSurfs[i].shaderSort = shader->sort;
	}

	// sort transparent surfaces by depth
	typedef int (*sortFunc_t)( const void*, const void* );
	const sortFunc_t transpSort = r_ignoreShaderSortKey->integer ? &R_CompareDrawSurfNoKey : &R_CompareDrawSurf;
	qsort( drawSurfs + numDrawSurfs - numTranspSurfs, numTranspSurfs, sizeof(drawSurf_t), transpSort );

#if 0
	if ( r_ignoreShaderSortKey->integer == 0 ) {
		// all surfaces must be in sort order except the sky
		// because we draw it after all the other opaque surfaces
		float prevSort = -1.0f;
		for ( int i = 0; i < numDrawSurfs; ++i ) {
			R_DecomposeSort( (drawSurfs + i)->sort, &entityNum, &shader );
			Q_assert( shader->isSky || shader->sort >= prevSort );
			prevSort = shader->sort;
		}
	}
#endif

	// all the lit surfaces are in a single queue
	// but each light's surfaces are sorted within its subsection
	for ( int i = 0; i < tr.refdef.num_dlights; ++i ) {
		dlight_t* dl = &tr.refdef.dlights[i];
		if (dl->head) {
			R_SortLitsurfs( dl );
		}
	}

	R_AddDrawSurfCmd( drawSurfs, numDrawSurfs, numTranspSurfs );
}


// entities that will have procedurally generated surfaces will just
// point at this for their sorting surface
static const surfaceType_t entitySurface = SF_ENTITY;

static void R_AddEntitySurfaces()
{
	trRefEntity_t* ent;
	const shader_t* shader;

	if ( !r_drawentities->integer )
		return;

	for (tr.currentEntityNum = 0; tr.currentEntityNum < tr.refdef.num_entities; ++tr.currentEntityNum) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in mirrors,
		// because the true body position will already be drawn
		//
		if ( (ent->e.renderfx & RF_FIRST_PERSON) && tr.viewParms.isPortal ) {
			continue;
		}

		// simple generated models, like sprites and beams, are not culled
		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;		// don't draw anything
		case RT_SPRITE:
		case RT_LIGHTNING:
			// self blood sprites, talk balloons, etc should not be drawn in the primary
			// view.  We can't just do this check for all entities, because md3
			// entities may still want to cast shadows from them
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal ) {
				continue;
			}
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader );
			break;

		case RT_MODEL:
			// we must set up parts of tr.orient for model culling
			R_RotateForEntity( ent, &tr.viewParms, &tr.orient );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
			if (!tr.currentModel) {
				R_AddDrawSurf( &entitySurface, tr.defaultShader );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_MD3:
					R_AddMD3Surfaces( ent );
					break;
				case MOD_BRUSH:
					// we want doors and lifts to be considered world surfaces too
					tr.worldSurface = qtrue;
					R_AddBrushModelSurfaces( ent );
					tr.worldSurface = qfalse;
					break;
				case MOD_BAD:		// null model axis
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal)
						break;
					R_AddDrawSurf( &entitySurface, tr.defaultShader );
					break;
				default:
					ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;
		default:
			ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}
}


static void R_GenerateDrawSurfs()
{
	tr.worldSurface = qtrue;
	R_AddWorldSurfaces();
	tr.worldSurface = qfalse;

	R_AddPolygonSurfaces();

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are added,
	// because they use the projection matrix for lod calculation
	R_SetupProjection();

	R_AddEntitySurfaces();
}


// this function renders 1 scene, which has either 1 view or 2 views
// a view may be either the actual camera view (last view drawn, always present)
// or a mirror / remote location (first view drawn, only when a mirror/portal is present)
void R_RenderScene( const viewParms_t* parms )
{
	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 )
		return;

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	const int firstDrawSurf = tr.refdef.numDrawSurfs;
	const int firstLitSurf = tr.refdef.numLitSurfs;

	// set viewParms.world
	R_RotateForViewer();

	R_SetupFrustum();

	R_GenerateDrawSurfs();

	R_SortDrawSurfs( firstDrawSurf, firstLitSurf );

	R_EndScene( parms );
}


const image_t* R_UpdateAndGetBundleImage( const textureBundle_t* bundle, updateAnimatedImage_t updateImage )
{
	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic( bundle->videoMapHandle );

		int w, h, client;
		const byte* data;
		qbool dirty;
		const qbool validData = ri.CIN_GrabCinematic( bundle->videoMapHandle, &w, &h, &data, &client, &dirty );
		if ( client >= 0 && client < ARRAY_LEN( tr.scratchImage ) ) {
			image_t* const image = tr.scratchImage[client];
			if ( validData )
				(*updateImage)(image, w, h, data, dirty);

			return image;
		} else {
			return tr.whiteImage;
		}
	}

	if ( bundle->numImageAnimations <= 1 )
		return bundle->image[0];

	const double v = tess.shaderTime * bundle->imageAnimationSpeed;
	const int index = (int)v;
	if ( index < 0 ) // may happen with shader time offsets
		return bundle->image[0];

	return bundle->image[index % bundle->numImageAnimations];
}
