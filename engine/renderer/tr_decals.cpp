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

This file is part of Blood Run source code.
Quake and Quake III Arena are trademarks of id Software, Inc.
===========================================================================
*/

// tr_decals.c -- polygon projection on the world polygons, based on tr_marks.cpp

#include "tr_local.h"

#define SIDE_CROSS				3
#define MAX_VERTS_ON_POLY		128
#define MARKER_OFFSET			0	// 1

#define ANGLE_CUTOFF			(-0.1f)


/*
=============
R_SplitPolyWithPlane

Splits the input polygon with the specified plane.
You can can pass NULL as numBackPoints if you don't want the back part.

Out must have space for two more vertexes than in
=============
*/
static int R_SplitPolyWithPlane( 
	int numInPoints, vec3_t inPoints[MAX_VERTS_ON_POLY],
	int *numOutPoints, vec3_t outPoints[MAX_VERTS_ON_POLY],
	int *numBackPoints, vec3_t backPoints[MAX_VERTS_ON_POLY],
	const vec4_t clipPlane, vec_t epsilon
)
{
	float		dists[MAX_VERTS_ON_POLY + 4];
	int			sides[MAX_VERTS_ON_POLY + 4];
	int			counts[3];
	float		dot;
	float		*p1, *p2, *clip;
	float		d;

	// don't clip if it might overflow
	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		*numOutPoints = 0;
		return -1;
	}

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( int i = 0; i < numInPoints; i++ ) {
		dot = DotProduct( inPoints[i], clipPlane );
		dot -= clipPlane[3];
		dists[i] = dot;
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[numInPoints] = sides[0];
	dists[numInPoints] = dists[0];

	*numOutPoints = 0;
	if ( numBackPoints != NULL ) {
		*numBackPoints = 0;
	}

	if ( !counts[SIDE_BACK] ) {
		*numOutPoints = numInPoints;
		Com_Memcpy( outPoints, inPoints, numInPoints * sizeof( vec3_t ) );
		return SIDE_FRONT;
	}

	if ( !counts[SIDE_FRONT] ) {
		if ( numBackPoints ) {
			*numBackPoints = numInPoints;
			Com_Memcpy( backPoints, inPoints, numInPoints * sizeof( vec3_t ) );
		}
		return SIDE_BACK;
	}

	for ( int i = 0; i < numInPoints; i++ ) {
		p1 = inPoints[i];
		clip = outPoints[*numOutPoints];

		if ( sides[i] == SIDE_ON ) {
			VectorCopy( p1, clip );
			( *numOutPoints )++;
			if ( numBackPoints != NULL ) {
				VectorCopy( p1, backPoints[*numBackPoints] );
				( *numBackPoints )++;
			}
			continue;
		}

		if ( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, clip );
			( *numOutPoints )++;
			clip = outPoints[*numOutPoints];
		}

		if ( sides[i] == SIDE_BACK && numBackPoints != NULL ) {
			VectorCopy( p1, backPoints[*numBackPoints]);
			( *numBackPoints )++;			
		}


		if ( sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] ) {
			continue;
		}

		// generate a split point
		p2 = inPoints[( i + 1 ) % numInPoints];

		d = dists[i] - dists[i + 1];
		if ( d == 0 ) {
			dot = 0;
		} else {
			dot = dists[i] / d;
		}

		// clip xyz
		for ( int j = 0; j < 3; j++ ) {
			// avoid round off error when possible
			if ( clipPlane[j] == 1 ) {
				clip[j] = clipPlane[3];
			} else if ( clipPlane[j] == -1 ) {
				clip[j] = -clipPlane[3];
			}
			clip[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		( *numOutPoints )++;

		if ( numBackPoints != NULL ) {
			VectorCopy( clip, backPoints[*numBackPoints] );
			( *numBackPoints )++;
		}
	}
	return SIDE_CROSS;
}

typedef struct {
	vec3_t	origin;
	vec3_t	direction;
	vec4_t	clipPlanes[8];
	vec3_t	mins, maxs;
	vec3_t	axis[3];
	vec_t	texCoordScale;
	vec_t	depth;
	vec_t	fadeDepth;
} projectionParams_t;

typedef struct {
	// output
	int				maxPoints;
	vec_t			*pointBuffer;
	vec_t			*attribBuffer;
	int				maxFragments;
	markFragment_t	*fragmentBuffer;
	// scratchpad
	int				returnedFragments;
	int				returnedPoints;
	vec3_t			clipPoints[2][MAX_VERTS_ON_POLY];
	int				numClipPoints;
} projectionResults_t;

/*
=================
R_CreateProjectionParams

=================
*/

qboolean R_CreateProjectionParams(
	const vec3_t		origin,
	const vec3_t		projectionDir,
	vec_t				radius,
	vec_t				depth,
	vec_t				orientation,
	projectionParams_t	*params
)
{
	VectorCopy( origin, params->origin );
	VectorCopy( projectionDir, params->direction );
	params->depth = depth;
	params->fadeDepth = depth * 0.5f;

	// create the texture axis
	VectorNormalize2( projectionDir, params->axis[0] );
	VectorNegate( params->axis[0], params->axis[0] );

	PerpendicularVector( params->axis[1], params->axis[0] );
	RotatePointAroundVector( params->axis[2], params->axis[0], params->axis[1], orientation );
	CrossProduct( params->axis[0], params->axis[2], params->axis[1] );

	params->texCoordScale = 0.5f / radius;

	// create the decal rectangle
	vec3_t points[4];

	for ( int i = 0; i < 3; i++ ) {
		points[0][i] = origin[i] - radius * params->axis[1][i] - radius * params->axis[2][i];
		points[1][i] = origin[i] + radius * params->axis[1][i] - radius * params->axis[2][i];
		points[2][i] = origin[i] + radius * params->axis[1][i] + radius * params->axis[2][i];
		points[3][i] = origin[i] - radius * params->axis[1][i] + radius * params->axis[2][i];
	}

	// get the projection bounds
	vec3_t projection;

	VectorScale( projectionDir, depth, projection );
	ClearBounds( params->mins, params->maxs );
	for ( int i = 0; i < 4; i++ ) {
		vec3_t	temp;

		AddPointToBounds( points[i], params->mins, params->maxs );
		VectorAdd( points[i], projection, temp );
		AddPointToBounds( temp, params->mins, params->maxs );
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		VectorMA( points[i], -20, projectionDir, temp );
		AddPointToBounds( temp, params->mins, params->maxs );
	}

	// create bounding planes for the projection volume
	vec3_t v1, v2;

	for ( int i = 0; i < 4; i++ ) {
		VectorSubtract( points[( i + 1 ) % 4], points[i], v1 );
		VectorAdd( points[i], projection, v2 );
		VectorSubtract( points[i], v2, v2 );
		CrossProduct( v1, v2, params->clipPlanes[i] );
		VectorNormalizeFast( params->clipPlanes[i] );
		params->clipPlanes[i][3] = DotProduct( params->clipPlanes[i], points[i] );
	}
	// add near and far clipping planes for the projection volume
	VectorCopy( projectionDir, params->clipPlanes[4] );
	params->clipPlanes[4][3] = DotProduct( params->clipPlanes[4], points[0] ) - params->depth;

	VectorCopy( projectionDir, params->clipPlanes[5] );
	VectorInverse( params->clipPlanes[5] );
	params->clipPlanes[5][3] = DotProduct( params->clipPlanes[5], points[0] ) - params->depth;

	// add near and far fading planes
	VectorCopy( projectionDir, params->clipPlanes[6] );
	params->clipPlanes[6][3] = DotProduct( params->clipPlanes[6], points[0] ) - params->fadeDepth;

	VectorCopy( projectionDir, params->clipPlanes[7] );
	VectorInverse( params->clipPlanes[7] );
	params->clipPlanes[7][3] = DotProduct( params->clipPlanes[7], points[0] ) - params->fadeDepth;

	return qtrue;
}

/*
=================
R_AddMarkFragments

=================
*/
static void R_AddMarkFragments( const projectionParams_t *params, projectionResults_t *results ) {
	int pingPong;
	markFragment_t *mf;

	// chop the surface by all the bounding planes of the projection volume
	pingPong = 0;
	for ( int i = 0; i < 6; i++ ) {
		R_SplitPolyWithPlane( results->numClipPoints, results->clipPoints[pingPong],
							&results->numClipPoints, results->clipPoints[!pingPong],
							NULL, NULL,
							params->clipPlanes[i],
							0.1f );
		pingPong ^= 1;
		if ( results->numClipPoints == 0 ) {
			break;
		}
	}

	// completely clipped away?
	if ( results->numClipPoints == 0 ) {
		return;
	}

	// clip the depth faded parts
	int numFadeClipPoints = results->numClipPoints, numBackPoints;
	static vec3_t	backPoints[MAX_VERTS_ON_POLY];

	for ( int plane = 6; plane < 8; plane++ ) {
		R_SplitPolyWithPlane(
			results->numClipPoints, results->clipPoints[pingPong],
			&numFadeClipPoints, results->clipPoints[!pingPong],
			&numBackPoints, backPoints,
			params->clipPlanes[plane],
			0.1f
		);

		if ( numBackPoints > 0 ) {
			pingPong ^= 1;
			results->numClipPoints = numFadeClipPoints;

			// add the depth faded fragment to the returned list
			if ( numBackPoints + ( results->returnedPoints ) > results->maxPoints ) {
				return; // not enough space for this polygon
			}
			mf = results->fragmentBuffer + results->returnedFragments;
			mf->firstPoint = results->returnedPoints;
			mf->numPoints = numBackPoints;
			Com_Memcpy( results->pointBuffer + results->returnedPoints * 3, backPoints, numBackPoints * sizeof( vec3_t ) );

			// write attributes
			for ( int i = 0; i < numBackPoints; i++ ) {
				vec_t *point = &results->pointBuffer[( results->returnedPoints + i ) * 3];
				vec_t *attrib = &results->attribBuffer[( results->returnedPoints + i ) * 3];
				vec_t dist = DotProduct( point, params->clipPlanes[plane] ) - params->clipPlanes[plane][3];
				vec_t d = params->depth - params->fadeDepth;
				vec_t fade = Com_Clamp( 0.0f, 1.0f, ( d + dist ) / d );
				vec3_t delta;

				VectorSubtract( point, params->origin, delta );
				VectorSet( attrib,
					0.5f + DotProduct( delta, params->axis[1] ) * params->texCoordScale,
					0.5f + DotProduct( delta, params->axis[2] ) * params->texCoordScale,
					fade
				);
			}
			results->returnedPoints += numBackPoints;
			results->returnedFragments++;
		}

		if ( results->numClipPoints == 0 ) {
			return;
		}
	}

	// add this fragment to the returned list
	if ( results->numClipPoints + results->returnedPoints > results->maxPoints ) {
		return;	// not enough space for this polygon
	}
	mf = results->fragmentBuffer + results->returnedFragments;
	mf->firstPoint = results->returnedPoints;
	mf->numPoints = results->numClipPoints;
	Com_Memcpy( results->pointBuffer + results->returnedPoints * 3, results->clipPoints[pingPong], results->numClipPoints * sizeof( vec3_t ) );

	// write attributes
	for ( int i = 0; i < results->numClipPoints; i++ ) {
		vec_t *point = &results->pointBuffer[( results->returnedPoints + i ) * 3];
		vec_t *attrib = &results->attribBuffer[( results->returnedPoints + i ) * 3];
		vec3_t delta;
		VectorSubtract( point, params->origin, delta );
		VectorSet( attrib,
			0.5f + DotProduct( delta, params->axis[1] ) * params->texCoordScale,
			0.5f + DotProduct( delta, params->axis[2] ) * params->texCoordScale,
			1.0f
		);
	}

	results->returnedPoints += results->numClipPoints;
	results->returnedFragments++;
}

/*
=================
R_ProjectDecalOntoSurface

=================
*/

void R_ProjectDecalOntoSurface( const surfaceType_t *surfType, const projectionParams_t *params, projectionResults_t *results ) {
	srfSurfaceFace_t	*surf;
	srfGridMesh_t		*cv;
	drawVert_t			*dv;

	if ( *surfType == SF_GRID ) {
		cv = ( srfGridMesh_t * )surfType;
		for ( int m = 0; m < cv->height - 1; m++ ) {
			for ( int n = 0; n < cv->width - 1; n++ ) {
				// see tr_marks.cpp for the proper explanation on what's going on below
				int numClipPoints = 3;

				dv = cv->verts + m * cv->width + n;

				VectorCopy( dv[0].xyz, results->clipPoints[0][0] );
				VectorMA( results->clipPoints[0][0], MARKER_OFFSET, dv[0].normal, results->clipPoints[0][0] );
				VectorCopy( dv[cv->width].xyz, results->clipPoints[0][1] );
				VectorMA( results->clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, results->clipPoints[0][1] );
				VectorCopy( dv[1].xyz, results->clipPoints[0][2] );
				VectorMA( results->clipPoints[0][2], MARKER_OFFSET, dv[1].normal, results->clipPoints[0][2] );
				// check the normal of this triangle
				vec3_t v1, v2, normal;
				VectorSubtract( results->clipPoints[0][0], results->clipPoints[0][1], v1 );
				VectorSubtract( results->clipPoints[0][2], results->clipPoints[0][1], v2 );
				CrossProduct( v1, v2, normal );
				VectorNormalizeFast( normal );
				if ( DotProduct( normal, params->direction ) < ANGLE_CUTOFF ) {
					// add the fragments of this triangle
					results->numClipPoints = numClipPoints;
					R_AddMarkFragments( params, results );
					if ( results->returnedFragments == results->maxFragments ) {
						return;
					}
				}

				VectorCopy( dv[1].xyz, results->clipPoints[0][0] );
				VectorMA( results->clipPoints[0][0], MARKER_OFFSET, dv[1].normal, results->clipPoints[0][0] );
				VectorCopy( dv[cv->width].xyz, results->clipPoints[0][1] );
				VectorMA( results->clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, results->clipPoints[0][1] );
				VectorCopy( dv[cv->width + 1].xyz, results->clipPoints[0][2] );
				VectorMA( results->clipPoints[0][2], MARKER_OFFSET, dv[cv->width + 1].normal, results->clipPoints[0][2] );
				// check the normal of this triangle
				VectorSubtract( results->clipPoints[0][0], results->clipPoints[0][1], v1 );
				VectorSubtract( results->clipPoints[0][2], results->clipPoints[0][1], v2 );
				CrossProduct( v1, v2, normal );
				VectorNormalizeFast( normal );
				if ( DotProduct( normal, params->direction ) < ANGLE_CUTOFF ) {
					// add the fragments of this triangle
					results->numClipPoints = numClipPoints;
					R_AddMarkFragments( params, results );
					if ( results->returnedFragments == results->maxFragments ) {
						return;
					}
				}
			}
		}
	} else if ( *surfType == SF_FACE ) {
		surf = ( srfSurfaceFace_t * )surfType;
		// check the normal of this face
		if ( DotProduct( surf->plane.normal, params->direction ) > ANGLE_CUTOFF ) {
			return; // skipping backfacing/sharp angle faces
		}
		int *indexes = surf->indexes;
		for ( int k = 0; k < surf->numIndexes; k += 3 ) {
			for ( int j = 0; j < 3; j++ ) {
				const vec3_t &v = surf->verts[indexes[k + j]].xyz;
				VectorMA( v, MARKER_OFFSET, surf->plane.normal, results->clipPoints[0][j] );
			}
			// add the fragments of this face
			results->numClipPoints = 3;
			R_AddMarkFragments( params, results );
			if ( results->returnedFragments == results->maxFragments ) {
				return;
			}
		}
		return;
	}
}

/*
=================
R_ProjectDecalOntoSurfaces_r

=================
*/
static void R_ProjectDecalOntoSurfaces_r( const projectionParams_t *params, const mnode_t *node, projectionResults_t *results ) {
	int			s, c;
	msurface_t *surf, **mark;

	// do the tail recursion in a loop
	while ( node->contents == -1 ) {
		s = BoxOnPlaneSide( params->mins, params->maxs, node->plane );
		if ( s == 1 ) {
			node = node->children[0];
		} else if ( s == 2 ) {
			node = node->children[1];
		} else {
			R_ProjectDecalOntoSurfaces_r( params, node->children[0], results );
			node = node->children[1];
		}
	}

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while ( c-- ) {
		if ( results->returnedFragments == results->maxFragments ) {
			return; // not enough space for more fragments
		}
		surf = *mark;
		// check if the surface has NOIMPACT or NOMARKS set
		if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
			|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
			surf->vcBSP = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if ( *( surf->data ) == SF_FACE ) {
			// the face plane should go through the box
			s = BoxOnPlaneSide( params->mins, params->maxs, &( (srfSurfaceFace_t*)surf->data )->plane );
			if ( s == 1 || s == 2 ) {
				surf->vcBSP = tr.viewCount;
			} else if ( DotProduct( ( (srfSurfaceFace_t*)surf->data )->plane.normal, params->direction ) > ANGLE_CUTOFF ) {
				// don't add faces that make sharp angles with the projection direction
				surf->vcBSP = tr.viewCount;
			}
		} else if ( *((surfaceType_t*)surf->data ) != SF_GRID ) surf->vcBSP = tr.viewCount;
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if ( surf->vcBSP != tr.viewCount ) {
			surf->vcBSP = tr.viewCount;
			R_ProjectDecalOntoSurface( (surfaceType_t*)surf->data, params, results );
		}
		mark++;
	}
}

/*
=================
R_ProjectDecal

=================
*/
int R_ProjectDecal(
	const vec3_t	origin,
	const vec3_t	projectionDir,

	vec_t			radius,
	vec_t			depth,
	vec_t			orientation,

	int				maxPoints,
	vec3_t			pointBuffer,
	vec3_t			attribBuffer,
	int				maxFragments,
	markFragment_t	*fragmentBuffer
)
{
	projectionParams_t	projectionParams;
	projectionResults_t projectionResults;

	if ( !R_CreateProjectionParams( origin, projectionDir, radius, depth, orientation, &projectionParams ) ) {
		return 0;
	}

	projectionResults.maxPoints = maxPoints;
	projectionResults.pointBuffer = pointBuffer;
	projectionResults.attribBuffer = attribBuffer;
	projectionResults.maxFragments = maxFragments;
	projectionResults.fragmentBuffer = fragmentBuffer;
	projectionResults.returnedPoints = 0;
	projectionResults.returnedFragments = 0;

	//increment view count for double check prevention
	tr.viewCount++;
	R_ProjectDecalOntoSurfaces_r( &projectionParams, tr.world->nodes, &projectionResults );

	return projectionResults.returnedFragments;
}
