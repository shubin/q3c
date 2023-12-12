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
#ifndef __Q_SHARED_H
#define __Q_SHARED_H

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

#if defined( QC )
#define Q3_VERSION				"Blood Run PB3 (CNQ3 1.54)"
#define CLIENT_WINDOW_TITLE		"Blood Run"
#define CONSOLE_WINDOW_TITLE	"Blood Run"
#else
#define Q3_VERSION				"CNQ3 1.54"
#define CLIENT_WINDOW_TITLE		"CNQ3"
#define CONSOLE_WINDOW_TITLE	"CNQ3 Console"
#endif

#define BASEGAME			"baseq3"
#if defined( QC )
#define APEXGAME			"base"
#else
#define APEXGAME			"cpma"
#endif


#if defined(_DEBUG)
#define COMPILE_TIME_ASSERT( pred ) switch(0) { case 0: case pred: ; }
#else
#define COMPILE_TIME_ASSERT( pred )
#endif


#ifdef _MSC_VER

#if (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#pragma warning(disable : 4018)		// signed/unsigned mismatch
#pragma warning(disable : 4389)		// also signed/unsigned mismatch
#pragma warning(disable : 4244)		// conversion to smaller type
#pragma warning(disable : 4100)		// unreferenced formal parameter
#pragma warning(disable : 4127)		// conditional expression is constant
#pragma warning(disable : 4706)		// assignment within conditional (nanny mode)
//#pragma intrinsic( memset, memcpy )

#endif // _MSC_VER


#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif


#if defined(__cplusplus) && !defined(min)
	template <typename T> __inline T min( T a, T b ) { return (a < b) ? a : b; }
	template <typename T> __inline T max( T a, T b ) { return (a > b) ? a : b; }
#elif defined(Q3_VM) // #elif !defined(min) doesn't work here, because the VC headers are shit
	#define min( a, b ) ((a) < (b) ? (a) : (b))
	#define max( a, b ) ((a) > (b) ? (a) : (b))
#endif


#if defined(_MSC_VER)
#include <sal.h>
#define PRINTF_FORMAT_STRING _Printf_format_string_
#else
#define PRINTF_FORMAT_STRING
#endif


#if defined(__cplusplus)
extern "C" {
#endif


// technically-correct form, handy for catching sloppy code that mismixes bool and int, *cough* JPVW  :P
#if defined(Q3_VM)
#define assert(x) { if (!(x)) Com_Error(ERR_FATAL, "ASSERT "__FILE__"(%d): %s", __LINE__, #x); }
typedef enum { qfalse, qtrue } qbool;
#else
// this is a shitty shitty hack - the offending code should be cleaned up so we CAN use the typedef
typedef int qbool;
// and we can't even do this, because of vanilla C's multiple definition rule  :(
//const qbool qfalse = 0;
//const qbool qtrue = !0;
#define qfalse (qbool)(0)
#define qtrue (qbool)(!0)
#endif
typedef qbool qboolean;


/**********************************************************************
  VM Considerations

  The VM can not use the standard system headers because we aren't really
  using the compiler they were meant for.  We use bg_lib.h which contains
  prototypes for the functions we define for our own use in bg_lib.c.

  When writing mods, please add needed headers HERE, do not start including
  stuff like <stdio.h> in the various .c files that make up each of the VMs
  since you will be including system headers files can will have issues.

  Remember, if you use a C library function that is not defined in bg_lib.c,
  you will have to add your own version for support in the VM.

 **********************************************************************/

#if defined( Q3_VM ) || defined( NO_CRT )

#define QDECL
#define ID_INLINE
#include "bg_lib.h"

#define PASSFLOAT( x ) (*(const int*)&x)

#else

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "../qcommon/q_platform.h"

ID_INLINE int PASSFLOAT( float x ) { return (*(const int*)&x); }

#endif


#if defined( Q3_VM ) || defined( NO_CRT )
typedef int intptr_t;
#elif defined( NO_CRT )
#	if defined( __x86_64__) || defined( _M_X64 )
#		if defined( _MSC_VER )
		typedef __int64 intptr_t;
#		else
		typedef long long int intptr_t;
#		endif
#	else
	typedef int intptr_t;
#	endif
#else
	#include <stdint.h>
#endif

typedef unsigned char		byte;

typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

#define PAD(base, alignment)	(((base)+(alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment)	(PAD((base), (alignment)) - (base))
#define PADP(base, alignment)	((void *) PAD((intptr_t) (base), (alignment)))

#if !defined(ALIGN)
	#if defined(_MSC_VER)
		#define ALIGN(x) __declspec( align(x) )
	#else
		#define ALIGN(x)
	#endif
#endif

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))

// #define VALUE 42
// STRINGIZE_NE(VALUE) -> "VALUE"
// STRINGIZE(VALUE)    -> "42"
#define STRINGIZE_NE(x)		#x					// no expansion
#define STRINGIZE(x)		STRINGIZE_NE(x)		// with expansion

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

// the game guarantees that no string from the network will ever exceed MAX_STRING_CHARS
#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	1024	// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		1024
#define	MAX_INFO_VALUE		1024

#define	BIG_INFO_STRING		8192  // used for system info key only
#define	BIG_INFO_KEY		8192
#define	BIG_INFO_VALUE		8192


#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			256		// max length of a filesystem pathname

#define	MAX_NAME_LENGTH		32		// max length of a client name


//
// these aren't needed by any of the VMs.  put in another header?
//
#define	MAX_MAP_AREA_BYTES		32		// bit vector of area visibility


// parameters to the main Error routine
typedef enum {
	ERR_FATAL,					// exit the entire game with a popup window
	ERR_DROP,					// print to console and disconnect from game
	ERR_DROP_NDP,				// same but the NDP can run its own handler when active
	ERR_SERVERDISCONNECT,		// don't kill server
	ERR_DISCONNECT,				// client disconnected from the server
	ERR_NEED_CD					// pop up the need-cd dialog
} errorParm_t;


#if defined(_DEBUG) && !defined(BSPC)
	#define HUNK_DEBUG
#endif

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc( size, preference ) Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line );
#else
void *Hunk_Alloc( int size, ha_pref preference );
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define CIN_system	1
#define CIN_loop	2
#define CIN_hold	4
#define CIN_silent	8
#define CIN_shader	16

/*
==============================================================

MATHLIB

==============================================================
*/


typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
extern const vec3_t vec3_origin;
extern const vec4_t vec4_zero;

#ifndef M_PI
#define M_PI 3.14159265358979323846f // matches value in gcc v2 math.h
#endif


// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH		640
#define SCREEN_HEIGHT		480

#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT	16

#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		16

extern const vec4_t colorBlack;
extern const vec4_t colorRed;
extern const vec4_t colorGreen;
extern const vec4_t colorYellow;
extern const vec4_t colorBlue;
extern const vec4_t colorPink;
extern const vec4_t colorCyan;
extern const vec4_t colorWhite;

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE )

const /* vec4_t */ float* ColorFromChar( char ccode );

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define COLOR_CVAR		'\x20'
#define COLOR_CMD		'\x21'
#define COLOR_VAL		'\x22'
#define COLOR_HELP		'\x23'
#define COLOR_WAR		'\x24'
#define COLOR_ERR		'\x25'

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"
#define S_COLOR_CVAR	"^\x20"
#define S_COLOR_CMD		"^\x21"
#define S_COLOR_VAL		"^\x22"
#define S_COLOR_HELP	"^\x23"
#define S_COLOR_WAR		"^\x24"
#define S_COLOR_ERR		"^\x25"

#define MAKERGB( v, r, g, b ) { v[0]=r;v[1]=g;v[2]=b; }
#define MAKERGBA( v, r, g, b, a ) { v[0]=r;v[1]=g;v[2]=b;v[3]=a; }

#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0f )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )


#if idppc

static ID_INLINE float Q_rsqrt( float number ) {
	float x = 0.5f * number;
	float y;
#ifdef __GNUC__
	asm("frsqrte %0,%1" : "=f" (y) : "f" (number));
#else
	y = __frsqrte( number );
#endif
	return y * (1.5f - (x * y * y));
}

#ifdef __GNUC__
static ID_INLINE float Q_fabs(float x) {
	float abs_x;
	asm("fabs %0,%1" : "=f" (abs_x) : "f" (x));
	return abs_x;
}
#else
#define Q_fabs __fabsf
#endif

#else
float Q_fabs( float f );
float Q_rsqrt( float f );		// reciprocal square root
#endif


#define Square(x) ((x)*(x))

int DirToByte( const vec3_t dir );	// this isn't a real cheap function to call!
void ByteToDir( int b, vec3_t dir );

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))

#ifdef Q3_VM
#undef VectorCopy
// this is a little hack to get more efficient copies in our interpreter
typedef struct {
	float	v[3];
} vec3struct_t;
#define VectorCopy(a,b) (*(vec3struct_t *)b=*(vec3struct_t *)a)
#endif

#define VectorClear(a)				((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)			((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)		((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define Vector4Copy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Clear(a)				((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define Vector4Set(v, x, y, z, w)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z), (v)[3]=(w))

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );


#if !defined( Q3_VM )

#ifdef _MSC_VER
#pragma warning(disable : 4514)		// unreferenced inline
#endif

static ID_INLINE qboolean VectorCompare( const vec3_t v1, const vec3_t v2 )
{
	return (v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2]);
}

static ID_INLINE vec_t VectorLength( const vec3_t v )
{
	return (vec_t)sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t VectorLengthSquared( const vec3_t v )
{
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t Distance( const vec3_t p1, const vec3_t p2 )
{
	vec3_t	v;
	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static ID_INLINE vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 )
{
	vec3_t	v;
	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast( vec3_t v )
{
	float ilength = Q_rsqrt( DotProduct( v, v ) );
	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static ID_INLINE void VectorInverse( vec3_t v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static ID_INLINE void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross )
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

#else

qboolean VectorCompare( const vec3_t v1, const vec3_t v2 );
vec_t VectorLength( const vec3_t v );
vec_t VectorLengthSquared( const vec3_t v );
vec_t Distance( const vec3_t p1, const vec3_t p2 );
vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 );
void VectorNormalizeFast( vec3_t v ); // uses rsqrt approximation and does NOT validate length
void VectorInverse( vec3_t v );
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );

#endif

vec_t VectorNormalize( vec3_t v );		// returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out );

float Q_acos(float c);

int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand() & 0x7FFF) / ((float)0x8000))
#define crandom()	(2.0 * (((rand() & 0x7FFF) / ((float)0x7FFF)) - 0.5))

void vectoangles( const vec3_t value1, vec3_t angles );
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );

void AxisClear( vec3_t axis[3] );
#if defined(Q3_VM) // lcc can't cope with "const vec3_t []"
extern vec3_t axisDefault[3];
void AxisCopy( vec3_t in[3], vec3_t out[3] );
#else
extern const vec3_t axisDefault[3];
void AxisCopy( const vec3_t in[3], vec3_t out[3] );
#endif

struct cplane_s;

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const struct cplane_s* p );

float AngleMod( float a );
float LerpAngle( float from, float to, float frac );
float AngleSubtract( float a1, float a2 );
void AnglesSubtract( const vec3_t v1, const vec3_t v2, vec3_t out );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void RotateAroundDirection( vec3_t axis[3], float yaw );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
// perpendicular vector could be replaced by this

void MatrixMultiply( float in1[3][3], float in2[3][3], float out[3][3] );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
void PerpendicularVector( vec3_t dst, const vec3_t src );


///////////////////////////////////////////////////////////////


// error extension
typedef enum {
	EXT_ERRMOD_ENGINE,
	EXT_ERRMOD_CGAME,
	EXT_ERRMOD_GAME,
	EXT_ERRMOD_UI
} extErrorModule_t;

// error extension
typedef enum {
	EXT_ERRLEV_DROP,	// ERR_DROP
	EXT_ERRLEV_DISC,	// ERR_DISCONNECT
	EXT_ERRLEV_SVDISC	// ERR_SERVERDISCONNECT
} extErrorLevel_t;

const char* QDECL va( PRINTF_FORMAT_STRING const char* format, ... );
const char* v3tos( const vec3_t v );
const char* v4tos( const vec4_t v );

void QDECL Com_Error( int level, PRINTF_FORMAT_STRING const char* error, ... );
void QDECL Com_ErrorExt( int level, int module, qbool realError, PRINTF_FORMAT_STRING const char* error, ... );
void QDECL Com_Printf( PRINTF_FORMAT_STRING const char* msg, ... );
void QDECL Com_sprintf( char *dest, int size, PRINTF_FORMAT_STRING const char *fmt, ... );

float Com_Clamp( float min, float max, float value );
int Com_ClampInt( int min, int max, int value );

const char* COM_SkipPath( const char* pathname );
void COM_StripExtension( const char* in, char* out, int destsize );
void COM_DefaultExtension( char* path, int maxSize, const char* ext );

const char* COM_Parse( const char** data_p );
const char* COM_ParseExt( const char** data_p, qbool allowLineBreak );
int		COM_Compress( char *data_p );

void SkipBracedSection( const char** data );
void SkipRestOfLine( const char** data );

#define QSUBSYSTEM_INIT_START(X) Com_Printf( ">>> Initializing "X"\n" );
#define QSUBSYSTEM_INIT_DONE(X)  Com_Printf( "<<< "X" Initialization Complete\n" );


// botlib crap hacked into the engine for no good reason

#ifndef TT_STRING
#define TT_STRING			1
#define TT_LITERAL			2
#define TT_NUMBER			3
#define TT_NAME				4
#define TT_PUNCTUATION		5
#endif

typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKEN_CHARS];
} pc_token_t;


// mode parm for FS_FOpenFile
typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;


///////////////////////////////////////////////////////////////


int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );

// portable case insensitive compare
int		Q_stricmp( const char *s1, const char *s2 );
int		Q_strncmp( const char *s1, const char *s2, int n );
int		Q_stricmpn( const char *s1, const char *s2, int n );
char	*Q_strlwr( char *s1 );
char	*Q_strupr( char *s1 );
char	*Q_strrchr( const char* string, int c );
const char	*Q_stristr( const char *s, const char *find );

// buffer size safe library replacements
void	Q_strncpyz( char *dest, const char *src, int destsize );
void	Q_strcat( char *dest, int size, const char *src );

// strlen that discounts Quake color sequences
int Q_PrintStrlen( const char *string );
// gets the byte offset of the Nth printable character
int Q_PrintStroff( const char *string, int charOffset );
// removes color sequences from string
char *Q_CleanStr( char *string );

typedef intptr_t ( *syscall_t )( intptr_t *parms );
typedef intptr_t ( QDECL *dllSyscall_t )( intptr_t callNum, ... );
typedef void ( QDECL *dllEntry_t )( dllSyscall_t syscallptr );


//
// key / value info strings
//
qbool Info_Validate( const char* s );
const char* Info_ValueForKey( const char *s, const char *key );
void Info_NextPair( const char **s, char *key, char *value );
void Info_RemoveKey( char *s, const char *key );
qbool Info_SetValueForKey( char* s, const char* key, const char* value );
void Info_RemoveKey_Big( char *s, const char *key );
void Info_SetValueForKey_Big( char *s, const char *key, const char *value );


/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

#define	CVAR_ARCHIVE		1	// set to cause it to be saved to vars.rc
								// used for system variables, not for player
								// specific configurations
#define	CVAR_USERINFO		2	// sent to server on connect or change
#define	CVAR_SERVERINFO		4	// sent in response to front end requests
#define	CVAR_SYSTEMINFO		8	// these cvars will be duplicated on all clients
#define	CVAR_INIT			16	// don't allow change from console at all,
								// but can be set from the command line
#define	CVAR_LATCH			32	// will only change when C code next does
								// a Cvar_Get(), so it can't be changed
								// without proper initialization.  modified
								// will be set, even though the value hasn't
								// changed yet
#define	CVAR_ROM			64	// display only, cannot be set by user at all
#define	CVAR_USER_CREATED	128	// created by a set command
#define	CVAR_TEMP			256	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT			512	// can not be changed if cheats are disabled
#define CVAR_NORESTART		1024	// do not clear when a cvar_restart is issued

#define CVAR_SERVER_CREATED		2048	// cvar was created by a server the client connected to
#define CVAR_CMDLINE_CREATED	4096	// cvar was created through the command-line (+set)

#define CVAR_NONEXISTENT	0xFFFFFFFF	// Cvar doesn't exist.

#define	MAX_CVAR_VALUE_STRING	256

#define CVAR_GUI_VALUE( Value, Title, Desc )	Value "\0" Title "\0" Desc "\0"

// CVar categories
#define CVARCAT_GENERAL			1
#define CVARCAT_GAMEPLAY		2
#define CVARCAT_NETWORK			4
#define CVARCAT_DISPLAY			8
#define CVARCAT_GRAPHICS		16
#define CVARCAT_SOUND			32
#define CVARCAT_CONSOLE			64
#define CVARCAT_HUD				128
#define CVARCAT_GUI				256
#define CVARCAT_PERFORMANCE		512
#define CVARCAT_DEBUGGING		1024
#define CVARCAT_INPUT			2048
#define CVARCAT_DEMO			4096

typedef enum {
	CVART_STRING,		// no validation
	CVART_FLOAT,		// uses floating-point min/max bounds
	CVART_INTEGER,		// uses integer min/max bounds
	CVART_BITMASK,		// uses integer min/max bounds
	CVART_BOOL,			// uses integer min/max bounds, min=0 and max=1
	// extended data types (not currently used by the CPMA QVMs)
	CVART_COLOR_CPMA,	// CPMA color code (0-9 A-Z a-z)
	CVART_COLOR_CPMA_E,	// CPMA color code or empty
	CVART_COLOR_CHBLS,	// CPMA color codes: rail Core, Head, Body, Legs, rail Spiral
	CVART_COLOR_RGB,	// as hex, e.g. FF00FF
	CVART_COLOR_RGBA,	// as hex, e.g. FF00FF00
	CVART_COUNT			// always last in the enum
} cvarType_t;

typedef int	cvarHandle_t;

// the modules that run in the virtual machine can't access the cvar_t directly,
// so they must ask for structured updates
typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

#include "../qcommon/surfaceflags.h"	// shared with the q3map utility

// plane types are used to speed some tests
// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
#define	PLANE_NON_AXIAL	3

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )

// plane_t structure
// ! if this is changed, it must be changed in asm code too !
typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;


// a trace is returned when a box is swept through the world
typedef struct {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact, transformed to world space
	int			surfaceFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			entityNum;	// entity the contacted sirface is a part of
} trace_t;

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD


// markfragments are returned by CM_MarkFragments()
typedef struct {
	int		firstPoint;
	int		numPoints;
} markFragment_t;


typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;


///////////////////////////////////////////////////////////////


// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE	0x0001
#define KEYCATCH_UI			0x0002
#define KEYCATCH_MESSAGE	0x0004
#define KEYCATCH_CGAME		0x0008


// sound channels
// channel 0 never willingly overrides
// other channels will always override a playing sound on that channel
typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,		// hit sounds, menu sounds
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,	// chat messages, etc
	CHAN_ANNOUNCER,		// announcer voices, etc
} soundChannel_t;


/*
========================================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

========================================================================
*/

#define ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define SHORT2ANGLE(x)	((x)*(360.0/65536))

#define SNAPFLAG_RATE_DELAYED	1
#define SNAPFLAG_NOT_ACTIVE		2	// snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT	4	// toggled every map_restart so transitions can be detected

//
// per-level limits
//
#define	MAX_CLIENTS			64		// absolute limit

#define	GENTITYNUM_BITS		10		// don't need to send any more
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communcated over the net need to
// also be in this range
#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)


#define	MAX_MODELS			256		// these are sent over the net as 8 bits
#define	MAX_SOUNDS			256		// so they cannot be blindly increased


#define	MAX_CONFIGSTRINGS	1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define	CS_SERVERINFO		0		// an info string with all the serverinfo cvars
#define	CS_SYSTEMINFO		1		// an info string for server system to client system configuration (timescale, etc)

#define	RESERVED_CONFIGSTRINGS	2	// game can't modify below this, only the system can

#define	MAX_GAMESTATE_CHARS	16000
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;


///////////////////////////////////////////////////////////////


// bit field limits
#define	MAX_STATS				16
#define	MAX_PERSISTANT			16
#define	MAX_POWERUPS			16
#if defined( QC )
#define MAX_WEAPONS				24
#else // QC
#define	MAX_WEAPONS				16
#endif // QC

#define	MAX_PS_EVENTS			2

#define PS_PMOVEFRAMECOUNTBITS	6

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// you can't add anything to this without modifying the code in msg.c

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
typedef struct playerState_s {
	int			commandTime;	// cmd->serverTime of last executed command
	int			pm_type;
	int			bobCycle;		// for view bobbing and footstep generation
	int			pm_flags;		// ducked, jump_held, etc
	int			pm_time;

	vec3_t		origin;
	vec3_t		velocity;
	int			weaponTime;
	int			gravity;
	int			speed;
	int			delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters

	int			groundEntityNum;// ENTITYNUM_NONE = in air

	int			legsTimer;		// don't change low priority animations until this runs out
	int			legsAnim;		// mask off ANIM_TOGGLEBIT

	int			torsoTimer;		// don't change low priority animations until this runs out
	int			torsoAnim;		// mask off ANIM_TOGGLEBIT

	int			movementDir;	// a number 0 to 7 that represents the reletive angle
								// of movement to the view angle (axial and diagonals)
								// when at rest, the value will remain unchanged
								// used to twist the legs during strafing

	vec3_t		grapplePoint;	// location of grapple to pull towards if PMF_GRAPPLE_PULL

	int			eFlags;			// copied to entityState_t->eFlags

	int			eventSequence;	// pmove generated events
	int			events[MAX_PS_EVENTS];
	int			eventParms[MAX_PS_EVENTS];

	int			externalEvent;	// events set on player from another source
	int			externalEventParm;
	int			externalEventTime;

	int			clientNum;		// ranges from 0 to MAX_CLIENTS-1
	int			weapon;			// copied to entityState_t->weapon
	int			weaponstate;

#if defined( QC )
	int			weaponFiringState; // for complex weapon behaviors (like tribolt)
	int			champion;		// selected champion
	int			baseHealth;
	int			baseArmor;

	int			jumpTime;
	int			landTime;
	int			walljumps;
	int			crouchSlideTime;

	int			ab_time;		// ability timer, semantic depends on the flags
	int			ab_misctime;	// additional timer for various purposes (i.e. grenade timing for Keel)
	int			ab_num;			// id of the entity associated with the champion (orb for Ranger), number of active totems, etc
	int			ab_flags;		// some bits to know whats happening with ability progressing
	qboolean	overbounce;		// overbounce bug handling
	// damage over time
	int			dotAcidTime;
	int			dotAcidNum;
	int			dotAcidOwner;
	int			dotFireTime;
	int			dotFireNum;
	int			dotFireOwner;
#endif

	vec3_t		viewangles;		// for fixed views
	int			viewheight;

	// damage feedback
	int			damageEvent;	// when it changes, latch the other parms
	int			damageYaw;
	int			damagePitch;
	int			damageCount;

	int			stats[MAX_STATS];
	int			persistant[MAX_PERSISTANT];	// stats that aren't cleared on death
	int			powerups[MAX_POWERUPS];	// level.time that the powerup runs out
	int			ammo[MAX_WEAPONS];

	int			generic1;
	int			loopSound;
	int			jumppad_ent;	// jumppad entity hit this frame

	// not communicated over the net at all
	int			ping;			// server to game info for scoreboard
	int			pmove_framecount;	// FIXME: don't transmit over the network
	int			jumppad_frame;
	int			entityEventSequence;
#if defined( QC )
	int			airTime;
	int			attackerNum;
	int			attackerTime;
	int			ringoutKiller;
#endif
} playerState_t;


///////////////////////////////////////////////////////////////


//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//
#define BUTTON_ATTACK		1
#define BUTTON_TALK			2			// displays talk balloon and disables actions
#define BUTTON_USE_HOLDABLE	4
#define BUTTON_GESTURE		8
#define BUTTON_WALKING		16			// walking can't just be inferred from player speed
										// because a key pressed late in the frame will
										// only generate a small move value for that frame
										// walking will use different animations and
										// won't generate footsteps

// TA buttons that are useless but still welded into the bot code
#define BUTTON_AFFIRMATIVE	32
#define BUTTON_NEGATIVE		64
#define BUTTON_GETFLAG		128
#define BUTTON_GUARDBASE	256
#define BUTTON_PATROL		512
#define BUTTON_FOLLOWME		1024

#if defined( QC )
#define BUTTON_ABILITY		2048
#define BUTTON_ZOOM			4096
#define BUTTON_ANY			8192		// any key whatsoever
#else
#define	BUTTON_ANY			2048		// any key whatsoever
#endif

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	int			serverTime;
	int			angles[3];
	int			buttons;
	byte		weapon;
	signed char	forwardmove, rightmove, upmove;
} usercmd_t;


///////////////////////////////////////////////////////////////


// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define	SOLID_BMODEL	0xffffff

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,				// non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					// value = base + sin( time / duration ) * delta
	TR_GRAVITY
} trType_t;

typedef struct {
	trType_t	trType;
	int		trTime;
	int		trDuration;			// if non 0, trTime + trDuration = stop time
	vec3_t	trBase;
	vec3_t	trDelta;			// velocity, etc
#if defined( QC )
	int		trGravity;
#endif
} trajectory_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large

typedef struct entityState_s {
	int		number;			// entity index
	int		eType;			// entityType_t
	int		eFlags;

	trajectory_t	pos;	// for calculating position
	trajectory_t	apos;	// for calculating angles

	int		time;
	int		time2;

	vec3_t	origin;
	vec3_t	origin2;

	vec3_t	angles;
	vec3_t	angles2;

	int		otherEntityNum;	// shotgun sources, etc
	int		otherEntityNum2;

	int		groundEntityNum;	// -1 = in air

	int		constantLight;	// r + (g<<8) + (b<<16) + (intensity<<24)
	int		loopSound;		// constantly loop this sound
#if defined( QC )
	int		loopSoundDist;
#endif // QC

	int		modelindex;
	int		modelindex2;
	int		clientNum;		// 0 to (MAX_CLIENTS - 1), for players and corpses
	int		frame;

	int		solid;			// for client side prediction, trap_linkentity sets this properly

	int		event;			// impulse events -- muzzle flashes, footsteps, etc
	int		eventParm;

	// for players
	int		powerups;		// bit flags
	int		weapon;			// determines weapon and flash model, etc
	int		legsAnim;		// mask off ANIM_TOGGLEBIT
	int		torsoAnim;		// mask off ANIM_TOGGLEBIT

	int		generic1;
#if defined( QC )
	int		affiliation;	// team for game modes or player index for FFA
	int		totemcharge;	// for totems
#endif // QC
} entityState_t;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	// not talking to a server
	CA_AUTHORIZING,		// not used any more, was checking cd key 
	CA_CONNECTING,		// sending request packets to the server
	CA_CHALLENGING,		// sending challenge packets to the server
	CA_CONNECTED,		// netchan_t established, getting gamestate
	CA_LOADING,			// only during cgame initialization, never during main loop
	CA_PRIMED,			// got gamestate, waiting for first frame
	CA_ACTIVE,			// game views should be displayed
	CA_CINEMATIC		// playing a cinematic or a static pic, not connected to a server
} connstate_t;


#define GLYPH_START 32
#define GLYPH_END 126
#define GLYPHS_PER_FONT (GLYPH_END - GLYPH_START + 1)

typedef struct {
	int height;		// ACTUAL resolution-independent height (ie "not random garbage like pointsize")
	int maxpitch;
	qhandle_t shader;
	int pitches[GLYPHS_PER_FONT];	// in texels
	int vpitch;						// in texels
	vec3_t ltr[GLYPHS_PER_FONT];	// tc's of glyph (left, top, right)
	float tcheight;
} fontInfo_t;


// real time
typedef struct {
	int tm_sec;     // seconds after the minute - [0,59]
	int tm_min;     // minutes after the hour - [0,59]
	int tm_hour;    // hours since midnight - [0,23]
	int tm_mday;    // day of the month - [1,31]
	int tm_mon;     // months since January - [0,11]
	int tm_year;    // years since 1900
	int tm_wday;    // days since Sunday - [0,6]
	int tm_yday;    // days since January 1 - [0,365]
	int tm_isdst;   // daylight savings time flag
} qtime_t;


// cinematic states
typedef enum {
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;


// server browser sources
// TTimo: AS_MPLAYER is no longer used
#define AS_LOCAL		0
#define AS_MPLAYER		1
#define AS_GLOBAL		2
#define AS_FAVORITES	3

#define	MAX_GLOBAL_SERVERS			4096
#define	MAX_OTHER_SERVERS			128
#define MAX_PINGREQUESTS			32
#define MAX_SERVERSTATUSREQUESTS	16

#define CDKEY_LEN 16
#define CDCHKSUM_LEN 2


// #define ANSWER 42
// STRING(ANSWER)  -> "ANSWER"
// XSTRING(ANSWER) -> "42"
#define STRING(x)	#x			// stringifies x
#define XSTRING(x)	STRING(x)	// expands x and then stringifies the result


#if defined(__cplusplus)
};
#endif

#endif	// __Q_SHARED_H
