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
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H


// renderfx flags
#define	RF_MINLIGHT			1		// allways have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		2		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		4		// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		8		// for view weapon Z crunching
#define	RF_NOSHADOW			64		// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	128		// use refEntity->lightingOrigin instead of refEntity->origin
									// for lighting.  This allows entities to sink into the floor
									// with their origin going solid, and allows all parts of a
									// player to get the same lighting
#define	RF_SHADOW_PLANE		256		// use refEntity->shadowPlane
#define	RF_WRAP_FRAMES		512		// mod the model frames by the maxframes to allow continuous
									// animation without needing to know the frame count

// refdef flags
#define RDF_NOWORLDMODEL	1		// used for player configuration screen
#define RDF_HYPERSPACE		4		// teleportation effect
#if defined( QC )
#define RDF_FORCEGREYSCALE	8		// lerps between r_greyscale and the number given in the refdef.forcedGreyscale
#endif // QC

typedef struct {
	vec3_t		xyz;
	float		st[2];
	byte		modulate[4];
} polyVert_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_UNUSED_BEAM,
	RT_UNUSED_RAIL_CORE,
	RT_UNUSED_RAIL_RING,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qbool	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index
	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing

	// misc
	byte		shaderRGBA[4];		// colors used by rgbgen entity shaders
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	union {
        float	fShaderTime;	    // subtracted from refdef time to control effect start times
        int     iShaderTime;        // qvm float have 23bit mantissa. After 8388607ms (02:20) we lost data.
	} shaderTime;

	// extra sprite information
	float		radius;
	float		rotation;
} refEntity_t;


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
#if defined( QC )
	float		forcedGreyscale;
#endif // QC
} refdef_t;


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;

/*
glconfig_t

Contains variables specific to the OpenGL configuration
being run right now. These are constant once the OpenGL
subsystem is initialized.

This type is exposed to VMs and the client. It is immutable because 
the layout has to match what the cgame and ui VMs expect.

There is an equivalent to this that is local to the renderer.

Fields prefixed with "unused_" are unused _by the engine_,
but might still be read by mods other than CPMA.
*/

typedef struct {
	// filled by the renderer
	char	renderer_string[MAX_STRING_CHARS];
	char	vendor_string[MAX_STRING_CHARS];
	char	version_string[MAX_STRING_CHARS];
	char	extensions_string[BIG_INFO_STRING];

	int		unused_maxTextureSize;
	int		unused_maxActiveTextures;

	// filled by the platform layer
	int		colorBits;		// 32
	int		depthBits;		// 24
	int		stencilBits;	//  8

	int		unused_driverType;		// 0 for ICD
	int		unused_hardwareType;	// 0 for generic

	qbool	unused_deviceSupportsGamma;		// qtrue

	int		unused_textureCompression;		// 0 for none
	qbool	unused_textureEnvAddAvailable;	// qtrue

	// windowAspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9

	// filled by the platform layer
	int		vidWidth, vidHeight;
	float	windowAspect;

	int		unused_displayFrequency;	// 0
	qbool	unused_isFullscreen;		// r_fullscreen->integer
	qbool	unused_stereoEnabled;		// qfalse
	qbool	unused_smpActive;			// qfalse
} glconfig_t;


#endif	// __TR_TYPES_H
