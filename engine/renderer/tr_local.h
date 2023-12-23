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


#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "tr_public.h"


extern const float s_flipMatrix[16];

#pragma pack(push, 1)
typedef struct {
	unsigned char	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;
#pragma pack(pop)


// a trRefEntity_t has all the information passed in by the cgame
// as well as some locally derived info

struct trRefEntity_t {
	refEntity_t	e;

	float	axisLength;			// compensate for non-normalized axis

	qbool	lightingCalculated;
	vec3_t	lightDir;			// normalized direction towards light
	vec3_t	ambientLight;		// color normalized to 0-255
	int		ambientLightInt;	// 32 bit rgba packed
	vec3_t	directedLight;

	qbool	intShaderTime;		// is the shaderTime member an integer?
};


struct orientationr_t {
	vec3_t		origin;			// in world coordinates
	vec3_t		axis[3];		// orientation in world
	vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
	float		modelMatrix[16];
};


enum textureFormat_t {
	TF_RGBA8,
	TF_COUNT
};


enum textureWrap_t {
	TW_REPEAT,
	TW_CLAMP_TO_EDGE,
	TW_COUNT
};


#define MAX_TEXTURE_SIZE 2048


struct shader_t;

struct image_t {
	image_t* next;

	int		width, height;		// actual, ie after power of two, picmip, and clamp to MAX_TEXTURE_SIZE
	int		flags;				// IMG_ bits

	RHI::HTexture	texture;
	uint32_t		textureIndex;
	textureFormat_t	format;
	textureWrap_t	wrapClampMode;

	char	name[MAX_QPATH];	// game path, including extension

	int		pakChecksum;
	int		index;				// indexes tr.images
	int		numShaders;			// number of shaders referencing this image
	int		flags0;				// flags requested to be 0 by at least 1 shader
	int		flags1;				// flags requested to be 1 by at least 1 shader
};


///////////////////////////////////////////////////////////////


typedef enum {
	SS_BAD,
	SS_PORTAL,			// mirrors, portals, viewscreens
	SS_ENVIRONMENT,		// sky box
	SS_OPAQUE,			// opaque

	SS_DECAL,			// scorch marks, etc.
	SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
						// in addition to alpha test
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER,		// for items that should be drawn in front of the water plane

	SS_BLEND0,			// regular transparency and filters
	SS_BLEND1,			// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	// gun smoke puffs

	SS_NEAREST			// blood blobs
} shaderSort_t;


#define MAX_SHADER_STAGES 8

typedef enum {
	GF_NONE,
	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,
	GF_NOISE,
	GF_COUNT
} genFunc_t;

typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
} deform_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	// tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_CONST,				// fixed color
	CGEN_DEBUG_ALPHA		// debug only: replicate the alpha channel
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_VECTOR			// S and T from world coordinates
} texCoordGen_t;

typedef struct {
	genFunc_t	func;

	double base;
	double amplitude;
	double phase;
	double frequency;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct {
	deform_t	deformation;			// vertex coordinate modification type

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct {
	texMod_t	type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t	wave;

	// used for TMOD_TRANSFORM
	float		matrix[2][2];	// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float		translate[2];	// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
	float		scale[2];		// s *= scale[0], t *= scale[1]

	// used for TMOD_SCROLL
	float		scroll[2];		// s' = s + scroll[0] * time, t' = t + scroll[1] * time

	// + = clockwise
	// - = counterclockwise
	float		rotateSpeed;

} texModInfo_t;


#define	MAX_IMAGE_ANIMATIONS	8

typedef struct {
	image_t*		image[MAX_IMAGE_ANIMATIONS];
	int				numImageAnimations;
	double			imageAnimationSpeed;
	qbool			isVideoMap;		// shit code - no reason to have both of these
	int				videoMapHandle;
} textureBundle_t;

typedef enum {
	ST_DIFFUSE,
	ST_LIGHTMAP,
	ST_MAX
} stageType_t;

typedef enum {
	TE_DISABLED,
	TE_MODULATE,
	TE_REPLACE,
	TE_DECAL,
	TE_ADD
} texEnv_t;

typedef struct {
	qbool		active;

	textureBundle_t	bundle;

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];

	int				numTexMods;
	texModInfo_t	*texMods;

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	byte			constantColor[4];			// for CGEN_CONST and AGEN_CONST

	unsigned		stateBits;					// GLS_xxxx mask

	qbool			isDetail;
	stageType_t		type;
} shaderStage_t;


#define LIGHTMAP_BROKEN		-4		// invalid data in the .bsp file
#define LIGHTMAP_2D			-3		// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX	-2		// pre-lit triangle models
#define LIGHTMAP_NONE		-1

typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED,
	CT_COUNT
} cullType_t;

typedef struct {
	float		cloudHeight;
	image_t *outerbox[6], *innerbox[6]; // innerbox was never actually used by Q3
} skyParms_t;

typedef struct {
	vec3_t	color;
	float	depthForOpaque;
} fogParms_t;

typedef enum {
	DFT_NONE,   // disabled
	DFT_BLEND,  // standard alpha blend
	DFT_ADD,    // additive
	DFT_MULT,   // multiplicative
	DFT_PMA,    // pre-multiplied alpha
	DFT_TBD,    // to be determined, i.e. fix up later
	DFT_COUNT
} depthFadeType_t;

extern const uint8_t r_depthFadeScaleAndBias[DFT_COUNT];

struct pipeline_t {
	int firstStage;
	int numStages;
	int pipeline;
	int mirrorPipeline;
};


struct shader_t {
	char		name[MAX_QPATH];		// game path, including extension
	int			lightmapIndex;			// for a shader to match, both name and lightmapIndex must match

	int			index;					// this shader == tr.shaders[index]
	int			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]

	float		sort;					// lower numbered shaders draw before higher numbered

	qbool		defaultShader;			// we want to return index 0 if the shader failed to load,
										// but R_FindShader should still keep a name allocated for it,
										// so if something calls RE_RegisterShader again with
										// the same name, we don't try looking for it again

	qbool		explicitlyDefined;		// found in a .shader file

	int			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
	int			contentFlags;

	qbool		entityMergable;			// multiple refentities can be combined in one batch (smoke, blood)

	qbool		isSky;
	skyParms_t	sky;
	fogParms_t	fogParms;

	float		portalRange;			// distance to fog out at

	cullType_t	cullType;				// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	qbool		polygonOffset;			// set for decals and other items that must be offset
#if defined( QC )
	qboolean	noVLCollapse;			// prevent ignoring lightmaps if vertex lighting is enabled
#endif
	int imgflags;	// nopicmip, nomipmaps, etc

	int			numDeforms;
	deformStage_t	deforms[MAX_SHADER_DEFORMS];

	int			numStages;
	shaderStage_t	*stages[MAX_SHADER_STAGES];
	int lightingStages[ST_MAX];

	double clampTime;				// time this shader is clamped to
	double timeOffset;				// current time offset for this shader

	// lightmap texture coordinates transform
	vec2_t lmScale;
	vec2_t lmBias;

	// depth fade rendering settings
	depthFadeType_t dfType;
	float dfInvDist;
	float dfBias;

	qbool greyscaleCTF;

	// extra info for /shaderinfo
	int fileIndex;
	const char* text;

	// vertex lighting
	qbool vlWanted;					// just a request, can be denied
	qbool vlApplied;				// qtrue if request accepted AND shader modified

	qbool isOpaque;					// no alpha blending, alpha test is OK if opaque
	qbool isAlphaTestedOpaque;		// no alpha blending, first stage is alpha tested
	qbool isDynamic;				// at least one vertex attribute must generated on the fly

	pipeline_t pipelines[MAX_SHADER_STAGES];
	int numPipelines;

	shader_t* next;
};


// skins allow models to be retextured without modifying the model file

struct skinSurface_t {
	char name[MAX_QPATH];
	shader_t* shader;
};

struct skin_t {
	char name[MAX_QPATH];
	int numSurfaces;
	skinSurface_t* surfaces[MD3_MAX_SURFACES];
};


typedef struct {
	int			originalBrushNumber;
	vec3_t		bounds[2];

	unsigned	colorInt;				// in packed byte format
	float		tcScale;				// texture coordinate vector scales
	fogParms_t	parms;

	// for clipping distance in fog when outside
	qbool		hasSurface;
	float		surface[4];
} fog_t;


typedef struct {
	orientationr_t	orient;
	orientationr_t	world;
	vec3_t		pvsOrigin;			// may be different than or.origin for portals
	qbool		isPortal;			// true if this view is through a portal
	qbool		isMirror;			// the portal is a mirror, invert the face culling
	int			frameSceneNum;		// copied from tr.frameSceneNum
	int			frameCount;			// copied from tr.frameCount
	cplane_t	portalPlane;		// clip anything behind this if mirroring
	int			viewportX, viewportY, viewportWidth, viewportHeight;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[4];
	vec3_t		visBounds[2];
	float		zFar;
#if defined( QC )
	float		greyscale;
	int32_t		pad0;
#endif // QC
} viewParms_t;


/*
==============================================================================

SURFACES

==============================================================================
*/

typedef byte color4ub_t[4];

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	SF_FLARE,
	SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff			// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

struct msurface_t;

struct drawSurf_t {
	// we keep the sort key at the top instead of the pointer
	// to allow for slightly cleaner code gen in the radix sort code
	uint64_t				sort;		// sort key for scene rendering
	const surfaceType_t*	surface;	// any of surface*_t
	float					depth;		// transparent surface's midpoint's depth
	qhandle_t				model;		// MD3 model handle, can be 0
	int						index;		// transparent surface's registration order
	float					shaderSort;	// transparent surface's shader sort
	int						shaderNum;	// unsorted shader index, for when we need to do fix-ups
	float					greyscale;  // how monochrome to draw all the stages
	int						staticGeoChunk;
	int						zppFirstIndex;
	int						zppIndexCount;
	float					radiusOverZ;
};

void R_TessellateSurface( const surfaceType_t* surfType );
void R_ComputeTessellatedSize( int* numVertexes, int* numIndexes, const surfaceType_t* surfType );
// R_ComputeTessellatedSize is unused for now but might be of use a bit later
// we can use it to compute the required size of the static geometry buffers in the GRP


struct litSurf_t {
	unsigned sort;						// bit combination for fast compares
	int shaderNum;						// unsorted shader index, for when we need to do fix-ups
	const surfaceType_t* surface;		// any of surface*_t
	litSurf_t* next;
	float greyscale;
	int staticGeoChunk;
};

struct dlight_t {
	vec3_t	origin;
	vec3_t	color;			// range from 0.0 to 1.0, should be color normalized
	float	radius;
	vec3_t	transformed;	// origin in local coordinate system
	qbool	active;			// actually shines into the frustum rather than just pvs
	litSurf_t* head;
	litSurf_t* tail;
};


#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory


// for cgame to add raw polys to a scene
struct srfPoly_t {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogIndex;
	int				numVerts;
	polyVert_t*		verts;
	vec3_t			localOrigin;
};


struct srfGridMesh_t {
	surfaceType_t	surfaceType;

	// culling information
	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;
	int				lodFixed;
	int				lodStitched;

	// vertexes
	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
	drawVert_t		verts[1];		// variable sized
};


// a srfVert_t is essentially a "fully featured" drawVert_t
// in some cases, eg srfSurfaceFace_t, the normal is common to the plane
// and doesn't actually HAVE to be populated, but...
struct srfVert_t {
	vec3_t xyz;
	vec3_t normal;
	vec2_t st;   // diffuse TC
	vec2_t st2;  // lightmap TC
	color4ub_t rgba;
};

struct srfSurfaceFace_t {
	surfaceType_t	surfaceType;
	cplane_t		plane;

	int				numIndexes;
	int				*indexes;

	int				numVerts;
	srfVert_t		*verts;

	vec3_t			localOrigin;
};


// misc_models in maps are turned into direct geometry by q3map
struct srfTriangles_t {
	surfaceType_t	surfaceType;

	vec3_t			bounds[2];
	vec3_t			localOrigin;
	float			radius;

	int				numIndexes;
	int				*indexes;

	int				numVerts;
	srfVert_t		*verts;
};


///////////////////////////////////////////////////////////////


// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information

struct trRefdef_t : public refdef_t {
	qbool		areamaskModified;	// qtrue if areamask changed since last scene
	int			microSeconds;		// [0;999] micro-seconds to add to the timestamp
	double		floatTime;			// tr.refdef.time / 1000.0

	int			num_entities;
	trRefEntity_t	*entities;

	int			num_dlights;
	dlight_t*	dlights;

	int numPolys;
	srfPoly_t* polys;

	int numDrawSurfs;
	drawSurf_t* drawSurfs;

	int numLitSurfs;
	litSurf_t* litSurfs;
};


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//

#define SIDE_FRONT	0
#define SIDE_BACK	1
#define SIDE_ON		2

struct msurface_t {
	int vcBSP;		// if == tr.viewCount, is in the PVS and BSP of this frame
	int vcVisible;	// if == tr.viewCount, is actually VISIBLE in this frame, i.e. passed facecull and has been added to the drawsurf list
	int lightCount;	// if == tr.lightCount, already added to the litsurf list for the current light
	const shader_t* shader;
	int fogIndex;
	int staticGeoChunk;
	int zppFirstIndex;
	int zppIndexCount;

	const surfaceType_t* data; // any of srf*_t
};


#define CONTENTS_NODE -1

struct mnode_t {
	// common with leaf and node
	int			contents;		// -1 for nodes, to differentiate from leafs
	int			visframe;		// node needs to be traversed (is in PVS) if == tr.visCount
	vec3_t		mins, maxs;		// for bounding box culling
	struct mnode_t* parent;

	// node specific
	const cplane_t* plane;
	struct mnode_t* children[2];

	// leaf specific
	int			cluster;
	int			area;
	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
};

typedef struct {
	vec3_t		bounds[2];		// for culling
	msurface_t	*firstSurface;
	int			numSurfaces;
} bmodel_t;

typedef struct {
	char		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
	char		baseName[MAX_QPATH];	// ie: tim_dm2

	int			dataSize;

	int			numShaders;
	dshader_t	*shaders;

	bmodel_t	*bmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numnodes;		// includes leafs
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numfogs;
	fog_t		*fogs;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];
	byte		*lightGridData;

	int			numClusters;
	int			clusterBytes;
	const byte	*vis;			// may be passed in by CM_LoadMap to save space
	byte		*novis;			// clusterBytes of 0xff

	char		*entityString;
	const char* entityParsePoint;
} world_t;


///////////////////////////////////////////////////////////////


typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MD3,
} modtype_t;

struct model_t {
	char		name[MAX_QPATH];
	int			index;				// model = tr.models[model->index]
	modtype_t	type;
	bmodel_t*		bmodel;				// type == MOD_BRUSH
	md3Header_t*	md3[MD3_MAX_LODS];	// type == MOD_MD3
	int			numLods;
	int			dataSize;			// just for listing purposes
};


// unfortunately, MAX_*NET*_MODELS is incorrectly already defined as "MAX_MODELS"
#define MAX_RENDERER_MODELS 1024

void R_ModelInit();
const model_t* R_GetModelByHandle( qhandle_t hModel );
int R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName );
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs );
void R_Modellist_f( void );


///////////////////////////////////////////////////////////////


extern	refimport_t		ri;

#define	MAX_DRAWIMAGES			2048
#define	MAX_LIGHTMAPS			256
#define	MAX_SKINS				1024
#define	MAX_DRAWSURFS			0x10000
#define MAX_SHADERS				16384	// 14 bits, must match the length in the sort key above

enum drawSurfGeneralSort_t {
	// dimensions - the sum should stay within 48 bits
	DRAWSORT_ENTITY_BITS = 10, // GENTITYNUM_BITS
	DRAWSORT_SHADER_BITS = 14, // log2 MAX_SHADERS
	DRAWSORT_STATICGEO_BITS = 1,
	DRAWSORT_PSO_BITS = 19,
	DRAWSORT_SKY_BITS = 1,
	DRAWSORT_ALPHATEST_BITS = 1,
	DRAWSORT_OPAQUE_BITS = 1,
	DRAWSORT_PORTAL_BITS = 1,
	// offsets
	DRAWSORT_ENTITY_INDEX = 0,
	DRAWSORT_SHADER_INDEX = DRAWSORT_ENTITY_INDEX + DRAWSORT_ENTITY_BITS,
	DRAWSORT_STATICGEO_INDEX = DRAWSORT_SHADER_INDEX + DRAWSORT_SHADER_BITS,
	DRAWSORT_PSO_INDEX = DRAWSORT_STATICGEO_INDEX + DRAWSORT_STATICGEO_BITS,
	DRAWSORT_SKY_INDEX = DRAWSORT_PSO_INDEX + DRAWSORT_PSO_BITS,
	DRAWSORT_ALPHATEST_INDEX = DRAWSORT_SKY_INDEX + DRAWSORT_SKY_BITS,
	DRAWSORT_OPAQUE_INDEX = DRAWSORT_ALPHATEST_INDEX + DRAWSORT_ALPHATEST_BITS,
	DRAWSORT_PORTAL_INDEX = DRAWSORT_OPAQUE_INDEX + DRAWSORT_OPAQUE_BITS
};


enum litSurfGeneralSort_t {
	// dimensions - the sum should stay within 32 bits
	LITSORT_ENTITY_BITS = 10, // GENTITYNUM_BITS
	LITSORT_SHADER_BITS = 14, // log2 MAX_SHADERS
	LITSORT_STATICGEO_BITS = 1,
	LITSORT_CULLTYPE_BITS = 2,		// PSO part 1
	LITSORT_POLYGONOFFSET_BITS = 1,	// PSO part 2
	LITSORT_DEPTHTEST_BITS = 1,		// PSO part 3
	// offsets
	LITSORT_ENTITY_INDEX = 0,
	LITSORT_SHADER_INDEX = LITSORT_ENTITY_INDEX + LITSORT_ENTITY_BITS,
	LITSORT_STATICGEO_INDEX = LITSORT_SHADER_INDEX + LITSORT_SHADER_BITS,
	LITSORT_CULLTYPE_INDEX = LITSORT_STATICGEO_INDEX + LITSORT_STATICGEO_BITS,
	LITSORT_POLYGONOFFSET_INDEX = LITSORT_CULLTYPE_INDEX + LITSORT_CULLTYPE_BITS,
	LITSORT_DEPTHTEST_INDEX = LITSORT_POLYGONOFFSET_INDEX + LITSORT_POLYGONOFFSET_BITS
};


#define MAX_TMUS 4


// all state modified by the back end is separated from the front end state

typedef struct {
	trRefdef_t		refdef;
	viewParms_t		viewParms;
	orientationr_t	orient;
	trRefEntity_t*	currentEntity;

	qbool			projection2D;	// if qtrue, drawstretchpic doesn't need to change modes
	byte			color2D[4];
	trRefEntity_t	entity2D;		// currentEntity will point at this when doing 2D rendering

	qbool			renderFrame;

	int* pc; // current stats set, depending on projection2D
	int pc2D[RB_STATS_MAX];
	int pc3D[RB_STATS_MAX];
} backEndState_t;


enum renderMode_t {
	RM_NONE,
	RM_UI,
	RM_3D,
	RM_NK
};


struct shaderParseMessage_t {
	char message[256];
};


/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
typedef struct {
	qbool	registered;		// cleared at shutdown, set at beginRegistration

	int		visCount;		// incremented every time a new vis cluster is entered
	int		frameCount;		// incremented every frame
	int		sceneCount;		// incremented every scene
	int		viewCount;		// incremented every view (twice a scene if portaled) and every R_MarkFragments call
	int		lightCount;		// incremented for each dlight in the view

	int		frameSceneNum;	// zeroed at RE_BeginFrame

	qbool			worldMapLoaded;
	world_t*		world;

	const byte*		externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t*		defaultImage;
	image_t*		whiteImage;			// { 255, 255, 255, 255 }
	image_t*		fullBrightImage;	// RGB scale based on r_mapBrightness, alpha 255
	image_t*		scratchImage[16];	// MAX_VIDEO_HANDLES

	shader_t*		defaultShader;
	shader_t*		scratchShader;	// used for cinematic playback

	int						numLightmaps;
	image_t					*lightmaps[MAX_LIGHTMAPS];

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;		// point currentEntity at this when rendering world
	int						currentEntityNum;
	const model_t* currentModel;

	viewParms_t				viewParms;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	int						identityLightByte;	// identityLight * 255

	orientationr_t			orient;					// for current entity

	trRefdef_t				refdef;

	int						viewCluster;
	dlight_t* light;	// current light during R_RecursiveLightNode

	int pc[RF_STATS_MAX];

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	int numModels;
	model_t* models[MAX_RENDERER_MODELS];

	int numImages;
	image_t* images[MAX_DRAWIMAGES];

	// associative array of image_t and shader_t indices where each entry is unique
	// the indices index into tr.images and tr.shaders
	int			imageShaders[2048]; // shader index in high 16 bits, image index in low 16 bits
	int			numImageShaders;

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int			numShaders;
	shader_t*	shaders[MAX_SHADERS];
	shader_t*	sortedShaders[MAX_SHADERS];

	qbool		traceWorldShader;
	int			tracedWorldShaderIndex;

	int			numSkins;
	skin_t*		skins[MAX_SKINS];

	float		mipFilter[4]; // only used by the GPU generators

	qbool		worldSurface; // is the currently added draw surface a world surface?

	renderMode_t	renderMode;

	// the following are only to be used after calling R_UpdateShader()
	// the save state boolean is needed because otherwise, a delayed shader load
	// could change the error and warning messages of the edited shader
	shaderParseMessage_t	shaderParseError;
	shaderParseMessage_t	shaderParseWarnings[16];
	qbool					shaderParseSaveState;
	qbool					shaderParseFailed;
	int						shaderParseNumWarnings;
	
} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;


//
// cvars
//

// r_mode
#define VIDEOMODE_DESKTOPRES	0	// no mode change, render size = desktop size
#define VIDEOMODE_UPSCALE		1	// no mode change, render size < desktop size
#define VIDEOMODE_CHANGE		2	// mode change - only makes sense for CRT users
#define VIDEOMODE_MAX			2

// r_blitMode
#define BLITMODE_ASPECT		0	// aspect-ratio preserving stretch
#define BLITMODE_CENTERED	1	// no stretch, displayed at the center
#define BLITMODE_STRETCHED	2	// dumb stretch, takes the full screen
#define BLITMODE_MAX		2

// r_rtColorFormat
#define RTCF_R8G8B8A8		0
#define RTCF_R10G10B10A2	1
#define RTCF_R16G16B16A16	2
#define RTCF_MAX			2
#define RTCF_COUNT			3

// r_gpuPreference
#define GPUPREF_HIGHPERF		0
#define GPUPREF_LOWPOWER		1
#define GPUPREF_NONE			2
#define GPUPREF_MAX				2


extern cvar_t	*r_lodbias;				// push/pull LOD transitions
extern cvar_t	*r_lodscale;

extern cvar_t	*r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t	*r_noportals;			// controls portal/mirror "second view" drawing
extern cvar_t	*r_dynamiclight;		// dynamic lights enabled/disabled

extern	cvar_t	*r_norefresh;			// bypasses the ref rendering
extern	cvar_t	*r_drawentities;		// disable/enable entity rendering
extern	cvar_t	*r_drawworld;			// disable/enable world rendering
extern  cvar_t	*r_detailTextures;		// enables/disables detail texturing stages
extern	cvar_t	*r_novis;				// disable/enable usage of PVS
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_nocurves;

extern cvar_t	*r_mode;				// see VIDEOMODE_*
extern cvar_t	*r_blitMode;			// see BLITMODE_*
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_displayRefresh;		// optional display refresh option

extern cvar_t	*r_intensity;
extern cvar_t	*r_gamma;
extern cvar_t	*r_greyscale;
extern cvar_t	*r_ditherStrength;		// the strength of the dithering noise
extern cvar_t	*r_transpSort;			// transparency sorting mode
extern cvar_t	*r_lightmap;			// render lightmaps only
extern cvar_t	*r_lightmapGreyscale;	// how monochrome the lightmap looks
extern cvar_t	*r_mapGreyscale;		// how monochrome the map looks
extern cvar_t	*r_mapGreyscaleCTF;		// how monochrome CTF map surfaces look
extern cvar_t	*r_teleporterFlash;		// 1 is default Q3 behavior, 0 is pure black
extern cvar_t	*r_sleepThreshold;		// time cushion in us for a call to Sleep(1+)
extern cvar_t	*r_shadingRate;			// variable-rate shading (VRS) mode
extern cvar_t	*r_guiFont;				// Dear ImGui font
extern cvar_t	*r_guiFontFile;			// Dear ImGui font, custom .ttf
extern cvar_t	*r_guiFontHeight;		// Dear ImGui font, custom height
extern cvar_t	*r_fullbright;			// avoid lightmap pass
extern cvar_t	*r_depthFade;			// fades marked shaders based on depth
extern cvar_t	*r_dither;				// enables dithering
extern cvar_t	*r_rtColorFormat;		// color render target format, see RTCF_*
extern cvar_t	*r_depthClamp;			// disables clipping vertices against the near and far clip planes
extern cvar_t	*r_gpuPreference;		// shall we use high-performance or low-power devices?

extern cvar_t	*r_mipGenFilter;		// if the string is invalid, Lanczos 4 is used
extern cvar_t	*r_mipGenGamma;			// what gamma-space do we consider the textures to be in
extern cvar_t	*r_ext_max_anisotropy;
extern cvar_t	*r_smaa;

extern	cvar_t	*r_singleShader;		// make most world faces use default shader
extern	cvar_t	*r_roundImagesDown;
extern	cvar_t	*r_colorMipLevels;		// development aid to see texture mip usage
extern	cvar_t	*r_picmip;				// controls picmip values

extern	cvar_t	*r_vsync;
extern	cvar_t	*r_lego;

extern	cvar_t	*r_vertexLight;			// vertex lighting mode for better performance
extern	cvar_t	*r_uiFullScreen;		// ui is running fullscreen

extern	cvar_t	*r_clear;				// clear to violet instead of black for debugging

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;

extern	cvar_t	*r_ignoreShaderSortKey;

extern	cvar_t	*r_brightness;
extern	cvar_t	*r_mapBrightness;

extern	cvar_t	*r_debugSurface;

extern	cvar_t	*r_debugSort;

extern	cvar_t	*r_debugUI;
extern	cvar_t	*r_debugInput;


void  R_NoiseInit();
double R_NoiseGet4f( double x, double y, double z, double t );

void R_RenderScene( const viewParms_t* parms );

void R_AddMD3Surfaces( trRefEntity_t *e );

void R_AddPolygonSurfaces();

void R_AddDrawSurf( const surfaceType_t* surface, const shader_t* shader, int staticGeoChunk = 0, int zppFirstIndex = 0, int zppIndexCount = 0, float radiusOverZ = 666.0f );
void R_AddLitSurf( const surfaceType_t* surface, const shader_t* shader, int staticGeoChunk );
uint64_t R_ComposeSort( int entityNum, const shader_t* shader, int staticGeoChunk );
void R_DecomposeSort( uint64_t sort, int* entityNum, const shader_t** shader );
uint32_t R_ComposeLitSort( int entityNum, const shader_t* shader, int staticGeoChunk );
void R_DecomposeLitSort( uint32_t sort, int* entityNum, const shader_t** shader );


#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes

int R_CullLocalBox( const vec3_t bounds[2] );
int R_CullPointAndRadius( const vec3_t origin, float radius );
int R_CullLocalPointAndRadius( const vec3_t origin, float radius );

void R_RotateForEntity( const trRefEntity_t* ent, const viewParms_t* viewParms, orientationr_t* orient );

typedef void (*updateAnimatedImage_t)( image_t* image, int w, int h, const byte* data, qbool dirty );
const image_t* R_UpdateAndGetBundleImage( const textureBundle_t* bundle, updateAnimatedImage_t updateImage );


#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		GLS_SRCBLEND_BITS					0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		GLS_DSTBLEND_BITS					0x000000f0

#define		GLS_BLEND_BITS						0x000000ff

#define GLS_DEPTHMASK_TRUE						0x00000100	// enable depth writes

#define GLS_POLYMODE_LINE						0x00001000	// wireframe polygon filling, not line rendering

#define GLS_DEPTHTEST_DISABLE					0x00010000	// disable depth tests
#define GLS_DEPTHFUNC_EQUAL						0x00020000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define		GLS_ATEST_BITS						0x70000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE
#define GLS_DEFAULT_2D		(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)

void		RE_LoadWorldMap( const char *mapname );
void		RE_SetWorldVisData( const byte *vis );
qhandle_t	RE_RegisterModel( const char *name );
qhandle_t	RE_RegisterSkin( const char *name );

const char*	R_GetMapName();

void		R_ColorShiftLightingBytes( const byte in[4], byte out[4] );

qbool	R_GetEntityToken( char *buffer, int size );

model_t* R_AllocModel();

void	R_Init();
void	R_ConfigureVideoMode( int desktopWidth, int desktopHeight );	// writes to glConfig and glInfo

#define IMG_NOPICMIP    0x0001  // images that must never be down-sampled
#define IMG_NOMIPMAP    0x0002  // 2D elements that will never be "distant" - implies IMG_NOPICMIP
#define IMG_NOIMANIP    0x0004  // used for math by shaders (normal maps etc) so don't image-process them
#define IMG_LMATLAS     0x0008  // lightmap atlas => RGBA, no initial data, no mip-mapping, no anisotropic, upload with R_UploadLightmapTile
#define IMG_EXTLMATLAS  0x0010  // external lightmap atlas => no mip-mapping, no anisotropic
#define IMG_NOAF        0x0020  // never enable anisotropic filtering

int		R_ComputeMipCount( int width, int height );

image_t* R_FindImageFile( const char* name, int flags, textureWrap_t glWrapClampMode );
image_t* R_CreateImage( const char* name, byte* pic, int width, int height, textureFormat_t format, int flags, textureWrap_t wrapClampMode );

void	R_SetColorMappings();

void	R_ImageList_f( void );
void	R_ImageInfo_f( void );
void	R_SkinList_f( void );

void	R_AddImageShader( image_t* image, shader_t* shader );

void	R_InitImages();
void	R_InitSkins();
const skin_t* R_GetSkinByHandle( qhandle_t hSkin );

extern const vec4_t r_mipBlendColors[16];

//
// tr_image_scale.cpp
//
void R_ResampleImage( byte** outD, int outW, int outH, const byte* inD, int inW, int inH, textureWrap_t tw );
void R_MipMap( byte** outD, const byte* inD, int inW, int inH, textureWrap_t tw );

//
// tr_renderdoc.cpp
//
void R_RenderDoc_CaptureFrames( int numFrames );

//
// tr_gui.cpp
//
void R_DrawGUI();
void R_ShutDownGUI();

//
// tr_shader.c
//
qhandle_t RE_RegisterShader( const char* name );
qhandle_t RE_RegisterShaderNoMip( const char* name );
qhandle_t RE_RegisterShaderFromImage( const char* name, image_t* image );

#define FINDSHADER_MIPRAWIMAGE_BIT 1
#define FINDSHADER_VERTEXLIGHT_BIT 2

shader_t	*R_FindShader( const char *name, int lightmapIndex, int flags );
const shader_t* R_GetShaderByHandle( qhandle_t hShader );
void		R_InitShaders();
void		R_ShaderList_f( void );
void		R_ShaderInfo_f();
void		R_ShaderMixedUse_f();
void		R_CompleteShaderName_f( int startArg, int compArg );
const char*	R_GetShaderPath( const shader_t* shader );
qbool		R_EditShader( shader_t* sh, const shader_t* original, const char* shaderText );
void		R_SetShaderData( shader_t* sh, const shader_t* original );

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

// Video initialization:
// - creating a window and changing video mode if needed,
// - respecting r_fullscreen, r_mode, r_width, r_height
// - filling up the right glconfig fields (see glconfig_t definition)
void Sys_V_Init();

// Video shutdown:
// - destroying the window
// - resetting the proper video mode if necessary
void Sys_V_Shutdown();

// Swaps buffers and applies r_vsync if the back-end can't already do it.
void Sys_V_EndFrame();

// Used to know if we must sleep ourselves to maintain the frame-rate cap.
// When unsure (e.g. API calls failed), return qfalse.
qbool Sys_V_IsVSynced();


/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/


struct stageVars_t
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[SHADER_MAX_VERTEXES];
	vec2_t*		texcoordsptr;
};


struct shaderCommands_t
{
	unsigned int	indexes[SHADER_MAX_INDEXES];
	vec4_t			xyz[SHADER_MAX_VERTEXES];
	vec4_t			normal[SHADER_MAX_VERTEXES];
	vec2_t			texCoords[SHADER_MAX_VERTEXES];
	vec2_t			texCoords2[SHADER_MAX_VERTEXES];
	color4ub_t		vertexColors[SHADER_MAX_VERTEXES];
	stageVars_t		svars[MAX_SHADER_STAGES];

	enum { TP_BASE, TP_LIGHT } pass;

	const shader_t* shader;
	double		shaderTime;

	int			numIndexes;
	int			numVertexes;

	const dlight_t* light;

	const shaderStage_t** xstages;

	// when > 0, only soft sprites are allowed in this batch
	depthFadeType_t depthFade;

	// when qtrue, RB_EndSurface doesn't need to compute deforms, colors, texture coordinates
	qbool deformsPreApplied;

	// how to process the colors of the current batch
	float greyscale;
};

extern shaderCommands_t tess;

void RB_CheckOverflow( int verts, int indexes );

void R_ComputeColors( const shaderStage_t* pStage, stageVars_t& svars, int firstVertex, int numVertexes );
void R_ComputeTexCoords( const shaderStage_t* pStage, stageVars_t& svars, int firstVertex, int numVertexes, qbool ptrOpt );

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color );
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 );


/*
============================================================

WORLD MAP

============================================================
*/

void R_AddBrushModelSurfaces( const trRefEntity_t* re );
void R_AddWorldSurfaces();
qbool R_inPVS( const vec3_t p1, const vec3_t p2 );


/*
============================================================

LIGHTS

============================================================
*/

void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void R_TransformDlights( int count, dlight_t* dl, const orientationr_t* orient );
qbool R_LightForPoint( const vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );


/*
============================================================

SKIES

============================================================
*/

void R_InitSkyTexCoords( float cloudLayerHeight );


/*
============================================================

CURVE TESSELATION

============================================================
*/

#define PATCH_STITCHING

srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] );
srfGridMesh_t *R_GridInsertColumn( srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror );
srfGridMesh_t *R_GridInsertRow( srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror );
void R_FreeSurfaceGridMesh( srfGridMesh_t *grid );

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
		int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

/*
============================================================

SCENE GENERATION

============================================================
*/

// clears counters and back-end commands
void R_ClearFrame();

void RE_ClearScene();
void RE_AddRefEntityToScene( const refEntity_t *ent, qbool intShaderTime );
void RE_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
void RE_AddLightToScene( const vec3_t org, float radius, float r, float g, float b );
void RE_RenderScene( const refdef_t *fd, int us );


/*
=============================================================

ANIMATED MODELS

=============================================================
*/

void	R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( int firstVertex, int numVertexes, int firstIndex, int numIndexes );


/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x80000

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} renderCommandList_t;

#pragma pack(push, 8)

struct renderCommandBase_t {
	int		commandId;
	int		padding;
};

typedef renderCommandBase_t beginFrameCommand_t;
typedef renderCommandBase_t swapBuffersCommand_t;
typedef renderCommandBase_t beginUICommand_t;
typedef renderCommandBase_t endUICommand_t;
typedef renderCommandBase_t begin3DCommand_t;
typedef renderCommandBase_t end3DCommand_t;
typedef renderCommandBase_t beginNuklearCommand_t;
typedef renderCommandBase_t endNuklearCommand_t;

struct uiSetColorCommand_t : renderCommandBase_t {
	float	color[4];
};

struct endFrameCommand_t : renderCommandBase_t {
	int		buffer;
};

struct uiDrawQuadCommand_t : renderCommandBase_t {
	const shader_t* shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
};

struct uiDrawTriangleCommand_t : renderCommandBase_t {
	const shader_t* shader;
	float	x0, y0;
	float	x1, y1;
	float	x2, y2;
	float	s0, t0;
	float	s1, t1;
	float	s2, t2;
};

struct drawSceneViewCommand_t : renderCommandBase_t {
	trRefdef_t refdef;
	viewParms_t	viewParms;
	int numDrawSurfs;
	int numTranspSurfs;
	drawSurf_t* drawSurfs;
	qbool shouldClearColor;
	qbool shouldDrawScene;
	vec4_t clearColor;
};

struct endSceneCommand_t : renderCommandBase_t {
	viewParms_t viewParms;
	uint32_t padding2;
};

struct nuklearUploadCommand_t : renderCommandBase_t {
	void* vertexes;
	void* indexes;
	int numVertexBytes;
	int numIndexBytes;
};

struct nuklearDrawCommand_t : renderCommandBase_t {
	int scissorRect[4]; // x y w h
	int firstIndex;
	int numIndexes;
	qhandle_t shader;
	int padding2;
};

#pragma pack(pop)

#define RENDER_COMMAND_LIST(Cmd) \
	Cmd(RC_END_OF_LIST, renderCommandBase_t) \
	Cmd(RC_BEGIN_UI, beginUICommand_t) \
	Cmd(RC_END_UI, endUICommand_t) \
	Cmd(RC_UI_SET_COLOR, uiSetColorCommand_t) \
	Cmd(RC_UI_DRAW_QUAD, uiDrawQuadCommand_t) \
	Cmd(RC_UI_DRAW_TRIANGLE, uiDrawTriangleCommand_t) \
	Cmd(RC_BEGIN_3D, begin3DCommand_t) \
	Cmd(RC_END_3D, end3DCommand_t) \
	Cmd(RC_DRAW_SCENE_VIEW, drawSceneViewCommand_t) \
	Cmd(RC_END_SCENE, endSceneCommand_t) \
	Cmd(RC_BEGIN_FRAME, beginFrameCommand_t) \
	Cmd(RC_SWAP_BUFFERS, swapBuffersCommand_t) \
	Cmd(RC_BEGIN_NK, beginNuklearCommand_t) \
	Cmd(RC_END_NK, endNuklearCommand_t) \
	Cmd(RC_NK_UPLOAD, nuklearUploadCommand_t) \
	Cmd(RC_NK_DRAW, nuklearDrawCommand_t)

#define RC(Enum, Type) Enum,
enum renderCommand_t {
	RENDER_COMMAND_LIST(RC)
	RC_COUNT
};
#undef RC

extern const int renderCommandSizes[RC_COUNT + 1];

struct readbackCommandBase_t {
	qbool requested;
};

struct screenshotCommand_t : readbackCommandBase_t {
	int width;
	int height;
	const char* fileName;
	enum ss_type { SS_TGA, SS_JPG } type;
	float conVis;	// if > 0, this is a delayed screenshot and we need to 
					// restore the console visibility to that value
};

struct videoFrameCommand_t : readbackCommandBase_t {
	int		width;
	int		height;
	byte	*captureBuffer;
	byte	*encodeBuffer;
	qbool	motionJpeg;
};

struct readbackCommands_t {
	screenshotCommand_t screenshot;
	videoFrameCommand_t videoFrame;
};

#define MAX_DLIGHTS		32			// completely arbitrary now  :D
#define MAX_REFENTITIES	1023		// can't be increased without changing drawsurf bit packing

// all of the information needed by the back-end must be
// contained in a backEndData_t instance
typedef struct {
	drawSurf_t	drawSurfs[MAX_DRAWSURFS];
	litSurf_t	litSurfs[MAX_DRAWSURFS];
	dlight_t	dlights[MAX_DLIGHTS];
	trRefEntity_t	entities[MAX_REFENTITIES];
	srfPoly_t	*polys;
	polyVert_t	*polyVerts;
	renderCommandList_t	commands;
	readbackCommands_t readbackCommands;
} backEndData_t;

#define SKY_SUBDIVISIONS		8
#define HALF_SKY_SUBDIVISIONS	(SKY_SUBDIVISIONS/2)

typedef enum {
	CS_RGBA,
	CS_BGR,
	CS_COUNT
} colorSpace_t;

extern	int		max_polys;
extern	int		max_polyverts;

extern	backEndData_t*		backEndData;

void RB_TakeScreenshotCmd( const screenshotCommand_t* cmd );
void RB_TakeVideoFrameCmd( const videoFrameCommand_t* cmd );

void RB_PushSingleStageShader( int stateBits, cullType_t cullType );
void RB_PopShader();

float RB_HyperspaceColor();

void RB_DrawSky();
void R_BuildCloudData();

void R_IssueRenderCommands();
byte* R_AllocateRenderCommand( int bytes, int commandId, qbool endFrame );

void R_AddDrawSurfCmd( drawSurf_t* drawSurfs, int numDrawSurfs, int numTranspSurfs );

void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( qbool render );
void RE_SetColor( const float* rgba );
void RE_StretchPic( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_DrawTriangle( float x0, float y0, float x1, float y1, float x2, float y2,
		float s0, float t0, float s1, float t1, float s2, float t2, qhandle_t hShader );

int SaveJPGToBuffer( byte* out, int quality, int image_width, int image_height, byte* image_buffer );
void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, qbool motionJpeg );
#if defined( QC )
qhandle_t RE_CreateTextureFromMemory( int width, int height, const byte *rgba );
void RE_GetShaderImageDimensions( qhandle_t shader, int nstage, int nimage, int *width, int *height );
int R_ProjectDecal(
		const vec3_t origin, const vec3_t dir,
		vec_t radius, vec_t depth, vec_t orientation,
		int maxPoints, vec3_t pointBuffer, vec3_t attribBuffer,
		int maxFragments, markFragment_t *fragmentBuffer );
void RE_GetAdvertisements( int *num, float *verts, void *shaders );
#endif

void R_EndScene( const viewParms_t* viewParms );

void R_MultMatrix( const float *a, const float *b, float *out );
void R_InvMatrix( const float* in, float* out );
void R_TransposeMatrix( const float* in, float* out );
void R_CameraPositionFromMatrix( const float* modelView, vec3_t cameraPos );
void R_CameraAxisVectorsFromMatrix( const float* modelView, vec3_t axisX, vec3_t axisY, vec3_t axisZ );
void R_MakeIdentityMatrix( float* m );
void R_MakeOrthoProjectionMatrix( float* m, float w, float h );


///////////////////////////////////////////////////////////////


// the "public" glconfig: screen size etc
extern glconfig_t glConfig;

// the "private" glconfig: implementation specifics for the renderer
struct glinfo_t {
	// used by platform layer
	qbool	winFullscreen;			// the window takes the entire screen
	qbool	vidFullscreen;			// change the video mode
	int		displayFrequency;
	int		winWidth, winHeight;

	// used by renderer
	int		maxTextureSize;
	int		maxAnisotropy;
	qbool	depthFadeSupport;
};

extern glinfo_t glInfo;


// renderer allocs are always on the low heap
template <class T> T* RI_New() { return (T*)ri.Hunk_Alloc(sizeof(T), h_low); }
template <class T> T* RI_New( size_t c ) { return static_cast<T*>(ri.Hunk_Alloc(sizeof(T) * c, h_low)); }

struct RI_AutoPtr {
	RI_AutoPtr() : mp(0) {}
	RI_AutoPtr( size_t c ) { mp = (byte*)ri.Hunk_AllocateTempMemory(c); }
	~RI_AutoPtr() { if (mp) ri.Hunk_FreeTempMemory(mp); }
	void* Alloc( size_t c ) { assert(!mp); mp = (byte*)ri.Hunk_AllocateTempMemory(c); return mp; }
	operator byte*() const { return mp; }
	template <typename T> T* Get() const { return (T*)mp; }
private:
	RI_AutoPtr( const RI_AutoPtr& rhs );
	RI_AutoPtr& operator=( const RI_AutoPtr& rhs );
	byte* mp;
};


extern screenshotCommand_t	r_delayedScreenshot;
extern qbool				r_delayedScreenshotPending;
extern int					r_delayedScreenshotFrame;


struct IRenderPipeline
{
	virtual void Init() = 0;
	virtual void ShutDown(bool fullShutDown) = 0;

	virtual void ProcessWorld(world_t& world) = 0;
	virtual void ProcessModel(model_t& model) = 0;
	virtual void ProcessShader(shader_t& shader) = 0;

	virtual void CreateTexture(image_t* image, int mipCount, int width, int height) = 0;
	virtual void UpoadTextureAndGenerateMipMaps(image_t* image, const byte* data) = 0;
	virtual void BeginTextureUpload(RHI::MappedTexture& mappedTexture, image_t* image) = 0;
	virtual void EndTextureUpload() = 0;

	virtual void ExecuteRenderCommands(const byte* data, bool readbackRequested) = 0;

	virtual void UISetColor(const uiSetColorCommand_t& cmd) = 0;
	virtual void UIDrawQuad(const uiDrawQuadCommand_t& cmd) = 0;
	virtual void UIDrawTriangle(const uiDrawTriangleCommand_t& cmd) = 0;
	virtual void DrawSceneView(const drawSceneViewCommand_t& cmd) = 0;
	virtual void TessellationOverflow() = 0;
	virtual void DrawSkyBox() = 0;
	virtual void DrawClouds() = 0;

	virtual void ReadPixels(int w, int h, int alignment, colorSpace_t colorSpace, void* out) = 0;
};

extern IRenderPipeline* renderPipeline;

struct RHIExport
{
	uint32_t renderToPresentUS;
	uint32_t inputToRenderUS;
	uint32_t inputToPresentUS;
	uint32_t presentToPresentUS;
	float monitorFrameDurationMS;
	float targetFrameDurationMS;
};

extern RHIExport rhie;

struct RHIInfo
{
	char name[MAX_QPATH];
	char adapter[MAX_QPATH];
	qbool hasTearing;
	qbool hasBaseVRS;
	qbool hasExtendedVRS;
	qbool isUMA;
	qbool isCacheCoherentUMA;
};

extern RHIInfo rhiInfo;


#endif //TR_LOCAL_H
