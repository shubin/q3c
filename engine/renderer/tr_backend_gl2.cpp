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
// OpenGL 2.0+ rendering back-end

#include "tr_local.h"
#include "GL/glew.h"


#define GL_INDEX_TYPE GL_UNSIGNED_INT


// the renderer front end should never modify glstate_t
typedef struct {
	int			currenttmu;
	int			texID[MAX_TMUS];
	int			texEnv[MAX_TMUS];
	qbool		finishCalled;
	int			faceCulling;
	unsigned	glStateBits;
} glstate_t;

static glstate_t glState;


static void GL_Bind( const image_t* image );
static void GL_SelectTexture( int unit );
static void GL_State( unsigned long stateVector );
static void GL_TexEnv( int env );
static void GL_Cull( int cullType );
static void R_BindAnimatedImage( const textureBundle_t* bundle );
static void RB_FogPass();
static void GAL_Begin2D();
static GLint GetTexEnv( texEnv_t texEnv );

void GL_GetRenderTargetFormat( GLenum* internalFormat, GLenum* format, GLenum* type, int cnq3Format );


struct GLSL_Program {
	GLuint p;  // linked program
	GLuint vs; // vertex shader
	GLuint fs; // fragment shader
};


static GLuint progCurrent;

static void GL_Program( const GLSL_Program& prog )
{
	assert( prog.p );

	if ( prog.p != progCurrent ) {
		glUseProgram( prog.p );
		progCurrent = prog.p;
		backEnd.pc3D[ RB_SHADER_CHANGES ]++;
	}
}


static GLSL_Program genericProg;

struct GLSL_GenericProgramAttribs {
	// pixel shader:
	GLint texture1;		// 2D texture
	GLint texture2;		// 2D texture
	GLint texEnv;		// 1i
	GLint greyscale;	// 1f
};

static GLSL_GenericProgramAttribs genericProgAttribs;

static const char* sharedFS =
"vec4 MakeGreyscale(vec4 color, float amount)\n"
"{\n"
"	float grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"
"	vec4 result = mix(color, vec4(grey, grey, grey, color.a), amount);\n"
"	return result;\n"
"}\n"
"\n";

static const char* genericVS =
"void main()\n"
"{\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"	gl_TexCoord[1] = gl_MultiTexCoord1;\n"
"	gl_TexCoord[2] = gl_Color;\n"
"}\n";

static const char* genericFS =
"uniform sampler2D texture1;\n"
"uniform sampler2D texture2;\n"
"uniform int texEnv;\n"
"uniform float greyscale;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 p = texture2D(texture1, gl_TexCoord[0].xy);\n"
"	vec4 s = texture2D(texture2, gl_TexCoord[1].xy);\n"
"	vec4 r;\n"
"	vec4 c = gl_TexCoord[2];"
"	if(texEnv == 1)\n"
"		r = c * s * p;\n"
"	else if(texEnv == 2)\n"
"		r = s; // use input.color or not?\n"
"	else if(texEnv == 3)\n"
"		r = c * vec4(p.rgb * (1.0 - s.a) + s.rgb * s.a, p.a);\n"
"	else if(texEnv == 4)\n"
"		r = c * vec4(p.rgb + s.rgb, p.a * s.a);\n"
"	else // texEnv == 0\n"
"		r = c * p;\n"
"\n"
"	gl_FragColor = MakeGreyscale(r, greyscale);\n"
"}\n";


static GLSL_Program dynLightProg;

struct GLSL_DynLightProgramAttribs {
	// vertex shader:
	GLint osEyePos;		// 4f, object-space
	GLint osLightPos;	// 4f, object-space

	// pixel shader:
	GLint texture;			// 2D texture
	GLint lightColorRadius;	// 4f, w = 1 / (r^2)
	GLint opaqueIntensity;	// 2f
	GLint greyscale;		// 1f
};

static GLSL_DynLightProgramAttribs dynLightProgAttribs;


static void GAL_BeginDynamicLight()
{
	GL_Program( dynLightProg );

	const dlight_t* dl = tess.light;
	vec3_t lightColor;
	VectorCopy( dl->color, lightColor );

	glUniform4f( dynLightProgAttribs.osLightPos, dl->transformed[0], dl->transformed[1], dl->transformed[2], 0.0f );
	glUniform4f( dynLightProgAttribs.osEyePos, backEnd.orient.viewOrigin[0], backEnd.orient.viewOrigin[1], backEnd.orient.viewOrigin[2], 0.0f );
	glUniform4f( dynLightProgAttribs.lightColorRadius, lightColor[0], lightColor[1], lightColor[2], 1.0f / Square(dl->radius) );
	glUniform1i( dynLightProgAttribs.texture, 0 ); // we use texture unit 0
}


static void DrawDynamicLight()
{
	GL_Cull( tess.shader->cullType );

	if ( tess.shader->polygonOffset )
		glEnable( GL_POLYGON_OFFSET_FILL );

	glUniform2f( dynLightProgAttribs.opaqueIntensity, backEnd.dlOpaque ? 1.0f : 0.0f, backEnd.dlIntensity );

	const int stage = tess.shader->lightingStages[ST_DIFFUSE];
	const shaderStage_t* pStage = tess.xstages[ stage ];

	// since this is guaranteed to be a single pass, fill and lock all the arrays

	glDisableClientState( GL_COLOR_ARRAY );

	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, tess.svars[stage].texcoordsptr );

	glEnableClientState( GL_NORMAL_ARRAY );
	glNormalPointer( GL_FLOAT, 16, tess.normal );

	glVertexPointer( 3, GL_FLOAT, 16, tess.xyz );
	glLockArraysEXT( 0, tess.numVertexes );

	GL_State( backEnd.dlStateBits );

	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle );

	glUniform1f( dynLightProgAttribs.greyscale, tess.greyscale );

	glDrawElements( GL_TRIANGLES, tess.dlNumIndexes, GL_INDEX_TYPE, tess.dlIndexes );
	backEnd.pc3D[ RB_DRAW_CALLS ]++;

	glUnlockArraysEXT();

	glDisableClientState( GL_NORMAL_ARRAY );

	if ( tess.shader->polygonOffset )
		glDisable( GL_POLYGON_OFFSET_FILL );
}


// returns qtrue if needs to break early
static qbool GL2_StageIterator_MultitextureStage( int stage )
{
	const shaderStage_t* const pStage = tess.xstages[stage];

	GL_SelectTexture( 1 );
	glEnable( GL_TEXTURE_2D );
	GL_TexEnv( GetTexEnv( pStage->mtEnv ) );
	R_BindAnimatedImage( &pStage->bundle );

	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, tess.svars[stage].texcoordsptr );

	glDrawElements( GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, tess.indexes );
	backEnd.pc3D[ RB_DRAW_CALLS ]++;

	glDisable( GL_TEXTURE_2D );
	GL_SelectTexture( 0 );

	return qfalse;
}


static void DrawGeneric()
{
	GL_Program( genericProg );

	glUniform1i( genericProgAttribs.texture1, 0 );
	glUniform1i( genericProgAttribs.texture2, 1 );
	glUniform1f( genericProgAttribs.greyscale, tess.greyscale );

	GL_Cull( tess.shader->cullType );

	if ( tess.shader->polygonOffset )
		glEnable( GL_POLYGON_OFFSET_FILL );

	// geometry is per-shader and can be compiled
	// color and tc are per-stage, and can't

	glDisableClientState( GL_COLOR_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 16, tess.xyz );	// padded for SIMD
	glLockArraysEXT( 0, tess.numVertexes );

	glEnableClientState( GL_COLOR_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	for ( int stage = 0; stage < tess.shader->numStages; ++stage ) {
		const shaderStage_t* pStage = tess.xstages[stage];
		glColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars[stage].colors );
		glTexCoordPointer( 2, GL_FLOAT, 0, tess.svars[stage].texcoordsptr );

		R_BindAnimatedImage( &pStage->bundle );
		GL_State( pStage->stateBits );

		if ( pStage->mtStages ) {
			// we can't really cope with massive collapses, so
			assert( pStage->mtStages == 1 );
			glUniform1i( genericProgAttribs.texEnv, tess.xstages[stage + 1]->mtEnv );
			if ( GL2_StageIterator_MultitextureStage( stage + 1 ) )
				break;
			stage += pStage->mtStages;
			continue;
		}

		glUniform1i( genericProgAttribs.texEnv, TE_DISABLED );
		glDrawElements( GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, tess.indexes );
		backEnd.pc3D[ RB_DRAW_CALLS ]++;
	}

	if ( tess.drawFog )
		RB_FogPass();

	glUnlockArraysEXT();

	if ( tess.shader->polygonOffset )
		glDisable( GL_POLYGON_OFFSET_FILL );
}


static void GAL_Draw( drawType_t type )
{
	if ( type == DT_DYNAMIC_LIGHT )
		DrawDynamicLight();
	else
		DrawGeneric();
}


static qbool GL2_CreateShader( GLuint* shaderPtr, GLenum shaderType, const char* shaderSource )
{
	const char* const sourceArray[] =
	{
		shaderType == GL_FRAGMENT_SHADER ? sharedFS : "",
		shaderSource
	};

	GLuint shader = glCreateShader( shaderType );
	glShaderSource( shader, ARRAY_LEN(sourceArray), sourceArray, NULL );
	glCompileShader( shader );

	GLint result = GL_FALSE;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &result );
	if ( result == GL_TRUE ) {
		*shaderPtr = shader;
		return qtrue;
	}

	GLint logLength = 0;
	glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logLength );

	static char log[4096]; // I've seen logs over 3 KB in size.
	glGetShaderInfoLog( shader, sizeof(log), NULL, log );
	ri.Printf( PRINT_ERROR, "%s shader: %s\n", shaderType == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log );

	return qfalse;
}


static qbool GL2_CreateProgram( GLSL_Program& prog, const char* vs, const char* fs )
{
	if ( !GL2_CreateShader( &prog.vs, GL_VERTEX_SHADER, vs ) )
		return qfalse;

	if ( !GL2_CreateShader( &prog.fs, GL_FRAGMENT_SHADER, fs ) )
		return qfalse;

	prog.p = glCreateProgram();
	glAttachShader( prog.p, prog.vs );
	glAttachShader( prog.p, prog.fs );
	glLinkProgram( prog.p );

	GLint result = GL_FALSE;
	glGetProgramiv( prog.p, GL_LINK_STATUS, &result );
	if ( result == GL_TRUE )
		return qtrue;

	GLint logLength = 0;
	glGetProgramiv( prog.p, GL_INFO_LOG_LENGTH, &logLength );

	static char log[4096]; // I've seen logs over 3 KB in size.
	glGetProgramInfoLog( prog.p, sizeof(log), NULL, log );
	ri.Printf( PRINT_ERROR, "Program link failed: %s\n", log );

	return qfalse;
}


// We don't use "gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
// because most everything is rendered using the fixed function pipeline and
// ftransform makes sure we get matching results (and thus avoid Z-fighting).

static const char* dynLightVS =
"uniform vec4 osLightPos;\n"
"uniform vec4 osEyePos;\n"
"varying vec4 L;\n"		// object-space light vector
"varying vec4 V;\n"		// object-space view vector
"varying vec3 nN;\n"	// normalized object-space normal vector
"\n"
"void main()\n"
"{\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"	L = osLightPos - gl_Vertex;\n"
"	V = osEyePos - gl_Vertex;\n"
"	nN = gl_Normal;\n"
"}\n"
"";

static const char* dynLightFS =
"uniform sampler2D texture;\n"
"uniform vec4 lightColorRadius;\n"	// w = 1 / (r^2)
"uniform vec2 opaqueIntensity;\n"
"uniform float greyscale;\n"
"varying vec4 L;\n"		// object-space light vector
"varying vec4 V;\n"		// object-space view vector
"varying vec3 nN;\n"	// normalized object-space normal vector
"\n"
"float BezierEase(float t)\n"
"{\n"
"	return t * t * (3.0 - 2.0 * t);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	vec4 base = MakeGreyscale(texture2D(texture, gl_TexCoord[0].xy), greyscale);\n"
"	vec3 nL = normalize(L.xyz);\n"	// normalized light vector
"	vec3 nV = normalize(V.xyz);\n"	// normalized view vector
	// light intensity
"	float intensFactor = min(dot(L.xyz, L.xyz) * lightColorRadius.w, 1.0);"
"	vec3 intens = lightColorRadius.rgb * BezierEase(1.0 - sqrt(intensFactor));\n"
	// specular reflection term (N.H)
"	float specFactor = min(abs(dot(nN, normalize(nL + nV))), 1.0);\n"
"	float spec = pow(specFactor, 16.0) * 0.25;\n"
	// Lambertian diffuse reflection term (N.L)
"	float diffuse = min(abs(dot(nN, nL)), 1.0);\n"
"	vec3 color = (base.rgb * vec3(diffuse) + vec3(spec)) * intens * opaqueIntensity.y;\n"
"	float alpha = mix(opaqueIntensity.x, 1.0, base.a);\n"
"	gl_FragColor = vec4(color.rgb * alpha, alpha);\n"
"}\n"
"";


struct FrameBuffer {
	GLuint fbo;
	GLuint color;			// texture if SS, renderbuffer if MS
	GLuint depthStencil;	// texture if SS, renderbuffer if MS
	qbool multiSampled;
	qbool hasDepthStencil;
};

static FrameBuffer frameBufferMain;
static FrameBuffer frameBuffersPostProcess[2];
static unsigned int frameBufferReadIndex = 0; // read this for the latest color/depth data
static qbool frameBufferMultiSampling = qfalse;


#define GL( call )		call; GL2_CheckError( #call, __FUNCTION__, __FILE__, __LINE__ )


static const char* GL2_GetErrorString( GLenum ec )
{
#define CASE( x )		case x: return #x
	switch ( ec )
	{
		CASE( GL_NO_ERROR );
		CASE( GL_INVALID_ENUM );
		CASE( GL_INVALID_VALUE );
		CASE( GL_INVALID_OPERATION );
		CASE( GL_INVALID_FRAMEBUFFER_OPERATION );
		CASE( GL_OUT_OF_MEMORY );
		CASE( GL_STACK_UNDERFLOW );
		CASE( GL_STACK_OVERFLOW );
		default: return "?";
	}
#undef CASE
}


static void GL2_CheckError( const char* call, const char* function, const char* file, int line )
{
	const GLenum ec = glGetError();
	if ( ec == GL_NO_ERROR )
		return;

	const char* fileName = file;
	while ( *file )
	{
		if ( *file == '/' || *file == '\\' )
			fileName = file + 1;

		++file;
	}

	ri.Printf( PRINT_ERROR, "%s failed\n", call );
	ri.Printf( PRINT_ERROR, "%s:%d in %s\n", fileName, line, function );
	ri.Printf( PRINT_ERROR, "GL error code: 0x%X (%d)\n", (unsigned int)ec, (int)ec );
	ri.Printf( PRINT_ERROR, "GL error message: %s\n", GL2_GetErrorString(ec) );
}


static void GL2_CreateColorRenderBufferStorageMS( int* samples )
{
	GLenum internalFormat, format, type;
	GL_GetRenderTargetFormat( &internalFormat, &format, &type, r_rtColorFormat->integer );

	int sampleCount = r_msaa->integer;
	while ( glGetError() != GL_NO_ERROR ) {} // clear the error queue

	if ( GLEW_VERSION_4_2 || GLEW_ARB_internalformat_query )
	{
		GLint maxSampleCount = 0;
		glGetInternalformativ( GL_RENDERBUFFER, internalFormat, GL_SAMPLES, 1, &maxSampleCount );
		if ( glGetError() == GL_NO_ERROR )
			sampleCount = min(sampleCount, (int)maxSampleCount);
	}

	GLenum errorCode = GL_NO_ERROR;
	for ( ;; )
	{
		// @NOTE: when the sample count is invalid, the error code is GL_INVALID_OPERATION
		glRenderbufferStorageMultisample( GL_RENDERBUFFER, sampleCount, internalFormat, glConfig.vidWidth, glConfig.vidHeight );
		errorCode = glGetError();
		if ( errorCode == GL_NO_ERROR || sampleCount == 0 )
			break;

		--sampleCount;
	}

	if ( errorCode != GL_NO_ERROR )
		ri.Error( ERR_FATAL, "Failed to create multi-sampled render buffer storage (error 0x%X)\n", (unsigned int)errorCode );

	GLint realSampleCount = 0;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &realSampleCount );
	if ( glGetError() == GL_NO_ERROR && realSampleCount > 0 )
		sampleCount = realSampleCount;

	*samples = sampleCount;
}


static qbool GL2_FBO_CreateSS( FrameBuffer& fb, qbool depthStencil )
{
	while ( glGetError() != GL_NO_ERROR ) {} // clear the error queue

	if ( depthStencil )
	{
		GL(glGenTextures( 1, &fb.depthStencil ));
		GL(glBindTexture( GL_TEXTURE_2D, fb.depthStencil ));
		GL(glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ));
		GL(glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ));
		GL(glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, glConfig.vidWidth, glConfig.vidHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL ));
	}

	GLenum internalFormat, format, type;
	GL_GetRenderTargetFormat( &internalFormat, &format, &type, r_rtColorFormat->integer );
	GL(glGenTextures( 1, &fb.color ));
	GL(glBindTexture( GL_TEXTURE_2D, fb.color ));
	GL(glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ));
	GL(glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ));
	GL(glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, glConfig.vidWidth, glConfig.vidHeight, 0, format, type, NULL ));

	GL(glGenFramebuffers( 1, &fb.fbo ));
	GL(glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo ));
	GL(glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.color, 0 ));
	if ( depthStencil )
		GL(glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fb.depthStencil, 0 ));

	const GLenum fboStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		ri.Error( ERR_FATAL, "Failed to create FBO (status 0x%X, error 0x%X)\n", (unsigned int)fboStatus, (unsigned int)glGetError() );
		return qfalse;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	fb.multiSampled = qfalse;
	fb.hasDepthStencil = depthStencil;

	return qtrue;
}


static qbool GL2_FBO_CreateMS( int* sampleCount, FrameBuffer& fb )
{
	while ( glGetError() != GL_NO_ERROR ) {} // clear the error queue

	GL(glGenFramebuffers( 1, &fb.fbo ));
	GL(glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo ));

	GL(glGenRenderbuffers( 1, &fb.color ));
	GL(glBindRenderbuffer( GL_RENDERBUFFER, fb.color ));
	GL2_CreateColorRenderBufferStorageMS( sampleCount );
	GL(glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb.color ));

	GL(glGenRenderbuffers( 1, &fb.depthStencil ));
	GL(glBindRenderbuffer( GL_RENDERBUFFER, fb.depthStencil ));
	GL(glRenderbufferStorageMultisample( GL_RENDERBUFFER, *sampleCount, GL_DEPTH24_STENCIL8, glConfig.vidWidth, glConfig.vidHeight ));

	GL(glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb.color ));
	GL(glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.depthStencil ));

	const GLenum fboStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		ri.Error( ERR_FATAL, "Failed to create FBO (status 0x%X, error 0x%X)\n", (unsigned int)fboStatus, (unsigned int)glGetError() );
		return qfalse;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	fb.multiSampled = qtrue;
	fb.hasDepthStencil = qtrue;

	return qtrue;
}


static qbool GL2_FBO_Init()
{
	const qbool enable = r_msaa->integer >= 2;
	frameBufferMultiSampling = enable;
	int finalSampleCount = 1;
	qbool result = qfalse;

	if ( enable ) {
		result =
			GL2_FBO_CreateMS( &finalSampleCount, frameBufferMain ) &&
			GL2_FBO_CreateSS( frameBuffersPostProcess[0], qfalse ) &&
			GL2_FBO_CreateSS( frameBuffersPostProcess[1], qfalse );
	} else {
		result =
			GL2_FBO_CreateSS( frameBuffersPostProcess[0], qtrue ) &&
			GL2_FBO_CreateSS( frameBuffersPostProcess[1], qtrue );
	}

	glInfo.msaaSampleCount = finalSampleCount;

	return result;
}


static void GL2_FBO_Bind( const FrameBuffer& fb )
{
	glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
	glDrawBuffer( GL_COLOR_ATTACHMENT0 );
}


static void GL2_FBO_Bind()
{
	GL2_FBO_Bind( frameBuffersPostProcess[frameBufferReadIndex] );
}


static void GL2_FBO_Swap()
{
	frameBufferReadIndex ^= 1;
}


static void GL2_FBO_BlitSSToBackBuffer()
{
	// fixing up the blit mode here to avoid unnecessary glClear calls
	int blitMode = r_blitMode->integer;
	if ( r_mode->integer != VIDEOMODE_UPSCALE )
		blitMode = BLITMODE_STRETCHED;

	if ( blitMode != BLITMODE_STRETCHED ) {
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );
	}

	const FrameBuffer& fbo = frameBuffersPostProcess[frameBufferReadIndex];
	glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo.fbo );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
	glDrawBuffer( GL_BACK );

	const int sw = glConfig.vidWidth;
	const int sh = glConfig.vidHeight;
	const int dw = glInfo.winWidth;
	const int dh = glInfo.winHeight;
	if ( blitMode == BLITMODE_STRETCHED ) {
		glBlitFramebuffer( 0, 0, sw, sh, 0, 0, dw, dh, GL_COLOR_BUFFER_BIT, GL_LINEAR );
	} else if ( blitMode == BLITMODE_CENTERED ) {
		const int dx = ( dw - sw ) / 2;
		const int dy = ( dh - sh ) / 2;
		glBlitFramebuffer( 0, 0, sw, sh, dx, dy, dx + sw, dy + sh, GL_COLOR_BUFFER_BIT, GL_LINEAR );
	} else { // blitMode == BLITMODE_ASPECT
		const float rx = (float)dw / (float)sw;
		const float ry = (float)dh / (float)sh;
		const float ar = min( rx, ry );
		const int w = (int)( sw * ar );
		const int h = (int)( sh * ar );
		const int x = ( dw - w ) / 2;
		const int y = ( dh - h ) / 2;
		glBlitFramebuffer( 0, 0, sw, sh, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_LINEAR );
	}
}


static void GL2_FBO_BlitMSToSS()
{
	const FrameBuffer& r = frameBufferMain;
	const FrameBuffer& d = frameBuffersPostProcess[frameBufferReadIndex];
	glBindFramebuffer( GL_READ_FRAMEBUFFER, r.fbo );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, d.fbo );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
	glDrawBuffer( GL_COLOR_ATTACHMENT0 );

	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;
	glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR );
}


static void GL2_FullScreenQuad()
{
	const float w = glConfig.vidWidth;
	const float h = glConfig.vidHeight;
	glBegin( GL_QUADS );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex2f( 0.0f, h );
		glTexCoord2f( 0.0f, 1.0f );
		glVertex2f( 0.0f, 0.0f );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex2f( w, 0.0f );
		glTexCoord2f( 1.0f, 0.0f );
		glVertex2f( w, h );
	glEnd();
}


static GLSL_Program gammaProg;

struct GLSL_GammaProgramAttribs
{
	int texture;
	int gammaOverbright;
};

static GLSL_GammaProgramAttribs gammaProgAttribs;


static void GL2_PostProcessGamma()
{
	const float brightness = r_brightness->value;
	const float gamma = 1.0f / r_gamma->value;

	if ( gamma == 1.0f && brightness == 1.0f )
		return;

	GL2_FBO_Swap();
	GL2_FBO_Bind();

	GL_Program( gammaProg );
	glUniform1i( gammaProgAttribs.texture, 0 ); // we use texture unit 0
	glUniform4f( gammaProgAttribs.gammaOverbright, gamma, gamma, gamma, brightness );
	GL_SelectTexture( 0 );
	glBindTexture( GL_TEXTURE_2D, frameBuffersPostProcess[frameBufferReadIndex ^ 1].color );
	GL2_FullScreenQuad();
}


static const char* gammaVS =
"void main()\n"
"{\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"}\n"
"";

static const char* gammaFS =
"uniform sampler2D texture;\n"
"uniform vec4 gammaOverbright;\n"
"\n"
"void main()\n"
"{\n"
"	vec3 base = texture2D(texture, gl_TexCoord[0].xy).rgb;\n"
"	gl_FragColor = vec4(pow(base, gammaOverbright.xyz) * gammaOverbright.w, 1.0);\n"
"}\n"
"";


static GLSL_Program greyscaleProg;

struct GLSL_GreyscaleProgramAttribs
{
	int texture;
	int greyscale;
};

static GLSL_GreyscaleProgramAttribs greyscaleProgAttribs;
static qbool greyscaleProgramValid = qfalse;


static void GL2_PostProcessGreyscale()
{
	if ( !greyscaleProgramValid )
		return;

	const float greyscale = Com_Clamp( 0.0f, 1.0f, r_greyscale->value );
	if ( greyscale == 0.0f )
		return;

	GL2_FBO_Swap();
	GL2_FBO_Bind();

	GL_Program( greyscaleProg );
	glUniform1i( greyscaleProgAttribs.texture, 0 ); // we use texture unit 0
	glUniform1f( greyscaleProgAttribs.greyscale, greyscale );
	GL_SelectTexture( 0 );
	glBindTexture( GL_TEXTURE_2D, frameBuffersPostProcess[frameBufferReadIndex ^ 1].color );
	GL2_FullScreenQuad();
}


static const char* greyscaleVS =
"void main()\n"
"{\n"
"	gl_Position = ftransform();\n"
"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"}\n"
"";

static const char* greyscaleFS =
"uniform sampler2D texture;\n"
"uniform float greyscale;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 final = MakeGreyscale(texture2D(texture, gl_TexCoord[0].xy), greyscale);\n"
"	gl_FragColor = vec4(final.rgb, 1.0);\n"
"}\n"
"";


static qbool GL2_Init()
{
	if ( !GL2_FBO_Init() ) {
		ri.Printf( PRINT_ERROR, "Failed to create FBOs\n" );
		return qfalse;
	}

	if ( !GL2_CreateProgram( genericProg, genericVS, genericFS ) ) {
		ri.Error( PRINT_ERROR, "Failed to compile generic shaders\n" );
		return qfalse;
	}
	genericProgAttribs.texture1 = glGetUniformLocation( genericProg.p, "texture1" );
	genericProgAttribs.texture2 = glGetUniformLocation( genericProg.p, "texture2" );
	genericProgAttribs.texEnv = glGetUniformLocation( genericProg.p, "texEnv" );
	genericProgAttribs.greyscale = glGetUniformLocation( genericProg.p, "greyscale" );

	if ( !GL2_CreateProgram( dynLightProg, dynLightVS, dynLightFS ) ) {
		ri.Printf( PRINT_ERROR, "Failed to compile dynamic light shaders\n" );
		return qfalse;
	}
	dynLightProgAttribs.osEyePos = glGetUniformLocation( dynLightProg.p, "osEyePos" );
	dynLightProgAttribs.osLightPos = glGetUniformLocation( dynLightProg.p, "osLightPos" );
	dynLightProgAttribs.texture = glGetUniformLocation( dynLightProg.p, "texture" );
	dynLightProgAttribs.lightColorRadius = glGetUniformLocation( dynLightProg.p, "lightColorRadius" );
	dynLightProgAttribs.opaqueIntensity = glGetUniformLocation( dynLightProg.p, "opaqueIntensity" );
	dynLightProgAttribs.greyscale = glGetUniformLocation( dynLightProg.p, "greyscale" );

	if ( !GL2_CreateProgram( gammaProg, gammaVS, gammaFS ) ) {
		ri.Printf( PRINT_ERROR, "Failed to compile gamma correction shaders\n" );
		return qfalse;
	}
	gammaProgAttribs.texture = glGetUniformLocation( gammaProg.p, "texture" );
	gammaProgAttribs.gammaOverbright = glGetUniformLocation( gammaProg.p, "gammaOverbright" );

	greyscaleProgramValid = GL2_CreateProgram( greyscaleProg, greyscaleVS, greyscaleFS );
	if ( greyscaleProgramValid ) {
		greyscaleProgAttribs.texture = glGetUniformLocation( greyscaleProg.p, "texture" );
		greyscaleProgAttribs.greyscale = glGetUniformLocation( greyscaleProg.p, "greyscale" );
	} else {
		ri.Printf( PRINT_ERROR, "Failed to compile greyscale shaders\n" );
	}

	return qtrue;
}


static void GL2_BeginFrame()
{
	if ( frameBufferMultiSampling )
		GL2_FBO_Bind( frameBufferMain );
	else
		GL2_FBO_Bind();
}


static void GL2_EndFrame()
{
	if ( frameBufferMultiSampling )
		GL2_FBO_BlitMSToSS();

	// this call is needed because there is no insurance for
	// what the state might be right now
	// we disable depth test, depth write and blending
	GL_State( GLS_DEPTHTEST_DISABLE );

	glViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	glScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );

	GL2_PostProcessGamma();
	GL2_PostProcessGreyscale();

	// needed for later calls to GL_Bind because
	// the above functions use glBindTexture directly
	glState.texID[glState.currenttmu] = 0;

	glViewport (0, 0, glInfo.winWidth, glInfo.winHeight );
	glScissor( 0, 0, glInfo.winWidth, glInfo.winHeight );

	GL2_FBO_BlitSSToBackBuffer();
}


static void ID_INLINE R_DrawElements( int numIndexes, const unsigned int* indexes )
{
	glDrawElements( GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes );
	backEnd.pc3D[ RB_DRAW_CALLS ]++;
}


static void UpdateAnimatedImage( image_t* image, int w, int h, const byte* data, qbool dirty )
{
	GL_Bind( image );
	if ( w != image->width || h != image->height ) {
		// if the scratchImage isn't in the format we want, specify it as a new texture
		image->width = w;
		image->height = h;
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	} else if ( dirty ) {
		// otherwise, just update it
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}


static void R_BindAnimatedImage( const textureBundle_t* bundle )
{
	GL_Bind( R_UpdateAndGetBundleImage( bundle, &UpdateAnimatedImage ) );
}


// blend a fog texture on top of everything else
static void RB_FogPass()
{
	glEnableClientState( GL_COLOR_ARRAY );
	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svarsFog.colors );

	glEnableClientState( GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer( 2, GL_FLOAT, 0, tess.svarsFog.texcoordsptr );

	GL_Bind( tr.fogImage );

	GL_State( tess.fogStateBits );

	R_DrawElements( tess.numIndexes, tess.indexes );
}


static void GL_Bind( const image_t* image )
{
	GLuint texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = (GLuint)tr.defaultImage->texnum;
	} else {
		texnum = (GLuint)image->texnum;
	}

	if ( glState.texID[glState.currenttmu] != texnum ) {
		glState.texID[glState.currenttmu] = texnum;
		glBindTexture( GL_TEXTURE_2D, texnum );
	}
}


static void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
		return;

	if ( unit >= MAX_TMUS )
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );

	glActiveTextureARB( GL_TEXTURE0_ARB + unit );
	glClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currenttmu = unit;
}


static void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED )
	{
		glDisable( GL_CULL_FACE );
	}
	else
	{
		glEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				glCullFace( GL_FRONT );
			}
			else
			{
				glCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				glCullFace( GL_BACK );
			}
			else
			{
				glCullFace( GL_FRONT );
			}
		}
	}
}


static GLint GetTexEnv( texEnv_t texEnv )
{
	switch ( texEnv )
	{
		case TE_MODULATE: return GL_MODULATE;
		case TE_REPLACE: return GL_REPLACE;
		case TE_DECAL: return GL_DECAL;
		case TE_ADD: return GL_ADD;
		default: return GL_MODULATE;
	}
}


static void GL_TexEnv( GLint env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}


static void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			glDepthFunc( GL_EQUAL );
		}
		else
		{
			glDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			glEnable( GL_BLEND );
			glBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			glDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			glDepthMask( GL_TRUE );
		}
		else
		{
			glDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			glDisable( GL_DEPTH_TEST );
		}
		else
		{
			glEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			glDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}


static void ApplyViewportAndScissor()
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf( backEnd.viewParms.projectionMatrix );
	glMatrixMode(GL_MODELVIEW);

	// set the window clipping
	glViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	glScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}


// any mirrored or portaled views have already been drawn
// so prepare to actually render the visible surfaces for this view

static void GAL_Begin3D()
{
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		glFinish();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	//
	// set the modelview matrix for the viewer
	//
	ApplyViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		const float c = RB_HyperspaceColor();
		clearBits |= GL_COLOR_BUFFER_BIT;
		glClearColor( c, c, c, 1.0f );
	} else if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		clearBits |= GL_COLOR_BUFFER_BIT;
		// tr.sunLight could have colored fastsky properly for the last 9 years,
		// ... if the code had actually been right >:(  but, it's a bad idea to trust mappers anyway
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}
	glClear( clearBits );

	glState.faceCulling = -1;		// force face culling to set next time

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.orient.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.orient.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.orient.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.orient.origin) - plane[3];

		glLoadMatrixf( s_flipMatrix );
		glClipPlane (GL_CLIP_PLANE0, plane2);
		glEnable (GL_CLIP_PLANE0);
	} else {
		glDisable (GL_CLIP_PLANE0);
	}
}


static void GAL_BeginSkyAndClouds( double depth )
{
	glDepthRange( depth, depth );
}


static void GAL_EndSkyAndClouds()
{
	// back to normal depth range
	glDepthRange( 0.0, 1.0 );
}


static GLint GL_TextureWrapMode( textureWrap_t w )
{
	switch ( w ) {
		case TW_REPEAT: return GL_REPEAT;
		case TW_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
		default: return GL_REPEAT;
	}
}


static GLint GL_TextureInternalFormat( textureFormat_t f )
{
	switch ( f ) {
		case TF_RGBA8:
		default: return GL_RGBA8;
	}
}


static GLenum GL_TextureFormat( textureFormat_t f )
{
	switch ( f ) {
		case TF_RGBA8:
		default: return GL_RGBA;
	}
}


static void GL_CheckErrors()
{
	const int err = glGetError();
	if (err == GL_NO_ERROR || r_ignoreGLErrors->integer)
		return;

	char s[64];
	switch( err ) {
		case GL_INVALID_ENUM:
			strcpy( s, "GL_INVALID_ENUM" );
			break;
		case GL_INVALID_VALUE:
			strcpy( s, "GL_INVALID_VALUE" );
			break;
		case GL_INVALID_OPERATION:
			strcpy( s, "GL_INVALID_OPERATION" );
			break;
		case GL_STACK_OVERFLOW:
			strcpy( s, "GL_STACK_OVERFLOW" );
			break;
		case GL_STACK_UNDERFLOW:
			strcpy( s, "GL_STACK_UNDERFLOW" );
			break;
		case GL_OUT_OF_MEMORY:
			strcpy( s, "GL_OUT_OF_MEMORY" );
			break;
		default:
			Com_sprintf( s, sizeof(s), "%i", err);
			break;
	}

	ri.Error( ERR_FATAL, "GL_CheckErrors: %s", s );
}


static int GetMaxAnisotropy( image_t* image )
{
	if ( (image->flags & IMG_NOAF) == 0 && glInfo.maxAnisotropy >= 2 && r_ext_max_anisotropy->integer >= 2 )
		return min( r_ext_max_anisotropy->integer, glInfo.maxAnisotropy );

	return 1;
}


static void GAL_CreateTexture( image_t* image, int mipCount, int w, int h )
{
	GLuint id;
	glGenTextures( 1, &id );
	image->texnum = (textureHandle_t)id;

	GL_Bind( image );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GetMaxAnisotropy(image) );
	if ( image->flags & IMG_LMATLAS ) {
		glTexImage2D( GL_TEXTURE_2D, 0, GL_TextureInternalFormat(image->format), w, h, 0, GL_TextureFormat(image->format), GL_UNSIGNED_BYTE, NULL );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
		GL_CheckErrors();
		return;
	}
	
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_TextureWrapMode(image->wrapClampMode) );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_TextureWrapMode(image->wrapClampMode) );

	if ( Q_stricmp( r_textureMode->string, "GL_NEAREST" ) == 0 &&
		( image->flags & (IMG_LMATLAS | IMG_EXTLMATLAS | IMG_NOPICMIP )) == 0 ) {
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	} else if ( image->flags & IMG_NOMIPMAP ) {
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	} else {
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();
}


static void GAL_UpdateTexture( image_t* image, int mip, int x, int y, int w, int h, const void* data )
{
	GL_Bind( image );
	if ( image->flags & IMG_LMATLAS )
		glTexSubImage2D( GL_TEXTURE_2D, (GLint)mip, x, y, w, h, GL_TextureFormat(image->format), GL_UNSIGNED_BYTE, data );
	else
		glTexImage2D( GL_TEXTURE_2D, (GLint)mip, GL_TextureInternalFormat(image->format), w, h, 0, GL_TextureFormat(image->format), GL_UNSIGNED_BYTE, data );

	GL_CheckErrors();
}


static void GAL_UpdateScratch( image_t* image, int w, int h, const void* data, qbool dirty )
{
	GL_Bind( image );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( w != image->width || h != image->height ) {
		image->width = w;
		image->height = h;
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	} else if ( dirty ) {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}


static void GAL_CreateTextureEx( image_t* image, int mipCount, int mipOffset, int w, int h, const void* mip0 )
{
	assert(0); // should never be called!
}


static void GL_SetDefaultState()
{
	glClearDepth( 1.0f );

	glCullFace( GL_FRONT );

	glColor4f( 1,1,1,1 );

	for ( int i = 0; i < MAX_TMUS; ++i ) {
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		glDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	glEnable( GL_TEXTURE_2D );

	glShadeModel( GL_SMOOTH );
	glDepthFunc( GL_LEQUAL );

	glPolygonOffset( -1, -1 );

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	glEnableClientState( GL_VERTEX_ARRAY );

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glDepthMask( GL_TRUE );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );

	// Needed for some of our glReadPixels calls.
	// The default alignment is 4.
	// RGB with width 1366 -> not a multiple of 4!
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );

	if ( r_depthClamp->integer )
		glEnable( GL_DEPTH_CLAMP );
	else
		glDisable( GL_DEPTH_CLAMP );
}


static void InitGLConfig()
{
	Q_strncpyz( glConfig.vendor_string, (const char*)glGetString( GL_VENDOR ), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (const char*)glGetString( GL_RENDERER ), sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.version_string, (const char*)glGetString( GL_VERSION ), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (const char*)glGetString( GL_EXTENSIONS ), sizeof( glConfig.extensions_string ) );
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.unused_maxTextureSize );
	glConfig.unused_maxActiveTextures = 0;
	glConfig.unused_driverType = 0;		// ICD
	glConfig.unused_hardwareType = 0;	// generic
	glConfig.unused_deviceSupportsGamma = qtrue;
	glConfig.unused_textureCompression = 0;	// no compression
	glConfig.unused_textureEnvAddAvailable = qtrue;
	glConfig.unused_displayFrequency = 0;
	glConfig.unused_isFullscreen = !!r_fullscreen->integer;
	glConfig.unused_stereoEnabled = qfalse;
	glConfig.unused_smpActive = qfalse;
}


static void InitGLInfo()
{
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &glInfo.maxTextureSize );

	if ( strstr( glConfig.extensions_string, "GL_EXT_texture_filter_anisotropic" ) )
		glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glInfo.maxAnisotropy );
	else
		glInfo.maxAnisotropy = 0;

	glInfo.depthFadeSupport = qfalse;
	glInfo.mipGenSupport = qfalse;
	glInfo.alphaToCoverageSupport = qfalse;
}


static void GAL_ReadPixels( int x, int y, int w, int h, int alignment, colorSpace_t colorSpace, void* out )
{
	const GLenum format = colorSpace == CS_BGR ? GL_BGR : GL_RGBA;
	glPixelStorei( GL_PACK_ALIGNMENT, alignment );
	glReadPixels( x, y, w, h, format, GL_UNSIGNED_BYTE, out );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
}


static void RB_DeleteTextures()
{
	for ( int i = 0; i < tr.numImages; ++i )
		glDeleteTextures( 1, (const GLuint*)&tr.images[i]->texnum );

	tr.numImages = 0;
	Com_Memset( tr.images, 0, sizeof( tr.images ) );
	Com_Memset( glState.texID, 0, sizeof( glState.texID ) );

	for ( int i = MAX_TMUS - 1; i >= 0; --i ) {
		GL_SelectTexture( i );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}


static void GAL_Begin2D()
{
	// set 2D virtual screen size
	glViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	glScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 );
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	GL_State( GLS_DEFAULT_2D );

	glDisable( GL_CULL_FACE );
	glDisable( GL_CLIP_PLANE0 );
}


static void GAL_SetModelViewMatrix( const float* matrix )
{
	glLoadMatrixf( matrix );
}


static void GAL_SetDepthRange( double zNear, double zFar )
{
	glDepthRange( zNear, zFar );
}


static qbool GAL_Init()
{
	if ( glConfig.vidWidth == 0 )
	{
		// the order of these calls can not be changed
		Sys_V_Init( GAL_GL2 );
		if ( !GLEW_VERSION_2_0 )
			ri.Error( ERR_FATAL, "OpenGL 2.0 required!\n" );
		if ( !GLEW_VERSION_3_0 && !GLEW_ARB_framebuffer_object )
			ri.Error( ERR_FATAL, "Need at least OpenGL 3.0 or GL_ARB_framebuffer_object\n" );
		InitGLConfig();
		InitGLInfo();
		GL2_Init();

		// apply the current V-Sync option after the first rendered frame
		r_swapInterval->modified = qtrue;
	}

	GL_SetDefaultState();

	const int err = glGetError();
	if (err != GL_NO_ERROR && !r_ignoreGLErrors->integer)
		ri.Printf( PRINT_ALL, "glGetError() = 0x%x\n", err );

	return qtrue;
}


static void GAL_ShutDown( qbool fullShutDown )
{
	RB_DeleteTextures();
	memset( &glState, 0, sizeof( glState ) );
}


static void GAL_BeginFrame()
{
	glState.finishCalled = qfalse;

	if ( !r_ignoreGLErrors->integer ) {
		int err;
		if ( ( err = glGetError() ) != GL_NO_ERROR ) {
			ri.Error( ERR_FATAL, "RE_BeginFrame() - glGetError() failed (0x%x)!\n", err );
		}
	}

	GL2_BeginFrame();

	if ( r_clear->integer )
		glClearColor( 1.0f, 0.0f, 0.5f, 1.0f );
	else
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}


static void GAL_EndFrame()
{
	if ( !backEnd.projection2D )
		GAL_Begin2D();

	GL2_EndFrame();

	if ( !glState.finishCalled )
		glFinish();
}


static void GAL_PrintInfo()
{
}


qbool GAL_GetGL2( graphicsAPILayer_t* rb )
{
	rb->Init = &GAL_Init;
	rb->ShutDown = &GAL_ShutDown;
	rb->BeginSkyAndClouds = &GAL_BeginSkyAndClouds;
	rb->EndSkyAndClouds = &GAL_EndSkyAndClouds;
	rb->ReadPixels = &GAL_ReadPixels;
	rb->BeginFrame = &GAL_BeginFrame;
	rb->EndFrame = &GAL_EndFrame;
	rb->CreateTexture = &GAL_CreateTexture;
	rb->UpdateTexture = &GAL_UpdateTexture;
	rb->UpdateScratch = &GAL_UpdateScratch;
	rb->CreateTextureEx = &GAL_CreateTextureEx;
	rb->Draw = &GAL_Draw;
	rb->Begin2D = &GAL_Begin2D;
	rb->Begin3D = &GAL_Begin3D;
	rb->SetModelViewMatrix = &GAL_SetModelViewMatrix;
	rb->SetDepthRange = &GAL_SetDepthRange;
	rb->BeginDynamicLight = &GAL_BeginDynamicLight;
	rb->PrintInfo = &GAL_PrintInfo;

	return qtrue;
}
