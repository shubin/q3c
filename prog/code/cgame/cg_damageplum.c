/*
===========================================================================
Copyright (C) 2021 Sergei Shubin

This file is part of Quake III Champions source code.

Quake III Champions source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Champions source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Champions source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_damageplum.c

#include "cg_local.h"

/* 
* Some math taken from other parts of the source code
* It's all needed to project damage indicator position from the world space onto the screen
*/

static
void TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix, vec4_t eye, vec4_t dst ) {
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

static
void TransformClipToWindow( const vec4_t clip, const refdef_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->width;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->height;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}

static
void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

static float s_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

static float s_modelMatrix[16];
static float s_projectionMatrix[16];

static
void CalcModelMatrix( refdef_t *fd, float *modelMatrix ) {
	float viewerMatrix[16];

	viewerMatrix[0] = fd->viewaxis[0][0];
	viewerMatrix[4] = fd->viewaxis[0][1];
	viewerMatrix[8] = fd->viewaxis[0][2];
	viewerMatrix[12] = -fd->vieworg[0] * viewerMatrix[0] + -fd->vieworg[1] * viewerMatrix[4] + -fd->vieworg[2] * viewerMatrix[8];

	viewerMatrix[1] = fd->viewaxis[1][0];
	viewerMatrix[5] = fd->viewaxis[1][1];
	viewerMatrix[9] = fd->viewaxis[1][2];
	viewerMatrix[13] = -fd->vieworg[0] * viewerMatrix[1] + -fd->vieworg[1] * viewerMatrix[5] + -fd->vieworg[2] * viewerMatrix[9];

	viewerMatrix[2] =  fd->viewaxis[2][0];
	viewerMatrix[6] =  fd->viewaxis[2][1];
	viewerMatrix[10] = fd->viewaxis[2][2];
	viewerMatrix[14] = -fd->vieworg[0] * viewerMatrix[2] + -fd->vieworg[1] * viewerMatrix[6] + -fd->vieworg[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	myGlMultMatrix( viewerMatrix, s_flipMatrix, modelMatrix );
}

static
void CalcProjectionMatrix( refdef_t *fd, float *projectionMatrix ) {
	float	xmin, xmax, ymin, ymax;
	float	width, height, stereoSep = 0;//r_stereoSeparation->value;
	float zProj = 4;

	ymax = zProj * tan(fd->fov_y * M_PI / 360.0f);
	ymin = -ymax;

	xmax = zProj * tan(fd->fov_x * M_PI / 360.0f);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	
	projectionMatrix[0] = 2 * zProj / width;
	projectionMatrix[4] = 0;
	projectionMatrix[8] = (xmax + xmin + 2 * stereoSep) / width;
	projectionMatrix[12] = 2 * zProj * stereoSep / width;

	projectionMatrix[1] = 0;
	projectionMatrix[5] = 2 * zProj / height;
	projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	projectionMatrix[13] = 0;

	projectionMatrix[3] = 0;
	projectionMatrix[7] = 0;
	projectionMatrix[11] = -1;
	projectionMatrix[15] = 0;
}

static
qboolean ProjectPoint( refdef_t *fd, vec3_t pos, vec3_t screenpos ) {
	vec4_t eye, clip, normalized, window;
	int i;

	TransformModelToClip( pos, s_modelMatrix, s_projectionMatrix, eye, clip );
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return qfalse;
		}
	}

	TransformClipToWindow( clip, fd, normalized, window );

	if ( window[0] < 0 || window[0] >= fd->width
		|| window[1] < 0 || window[1] >= fd->height ) {
		return qfalse;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}
	screenpos[0] = window[0];
	screenpos[1] = window[1];
	screenpos[2] = window[2];
	return qtrue;
}

typedef struct damage_plum_data_s {
	vec3_t origin;
	int damage;
	int time;
	int clientnum;
} damage_plum_data_t;

#define MAX_DAMAGE_PLUMS 64

#define DAMAGE_PLUM_WIDTH	6
#define DAMAGE_PLUM_HEIGHT	6

#define DAMAGE_PLUM_MIN_SIZE_FACTOR 0.0f
#define DAMAGE_PLUM_MAX_SIZE_FACTOR 3.0f

#define DAMAGE_PLUM_STAY_TIME				200
#define DAMAGE_PLUM_PULSE_SCALE				1.5f
#define DAMAGE_PLUM_DEFAULT_SCALE			1.0f
#define DAMAGE_PLUM_DISSOLVE_TIME			2500
#define DAMAGE_PLUM_WORLD_OFFSET			50.0f
#define DAMAGE_PLUM_TRAVEL_DISTANCE			60.0f
#define DAMAGE_PLUM_ACCUMULATION_THRESHOLD	500
#define DAMAGE_CROSS_TIME					200

static damage_plum_data_t s_damage_plums[MAX_DAMAGE_PLUMS] = { 0 };
static int s_last_damage_time = -1;

void CG_InitDamagePlums( void ) {
	// Just in case I want to load some custom font etc.
}

static
damage_plum_data_t *GetDamagePlumData( int clientnum, int time ) {
	damage_plum_data_t *data = NULL;
	damage_plum_data_t *freeslot = NULL;
	damage_plum_data_t *p;

	if ( cg_damagePlum.integer == 2 ) {
		for ( p = s_damage_plums; p - s_damage_plums < MAX_DAMAGE_PLUMS; p++) {
			if ( freeslot == NULL && p->damage == 0 ) {
				freeslot = p;
			}
			if ( p->clientnum == clientnum && time - p->time < DAMAGE_PLUM_ACCUMULATION_THRESHOLD ) {
				data = p;
				break;
			}
		}
	} else {
		for ( p = s_damage_plums; p - s_damage_plums < MAX_DAMAGE_PLUMS; p++) {
			if ( freeslot == NULL && p->damage == 0 ) {
				freeslot = p;
			}
		}
	}

	if ( data == NULL ) {
		data = freeslot;
		if ( data != NULL ) {
			memset( data, 0, sizeof( damage_plum_data_t ) );
			data->clientnum = clientnum;
		}
	}
	return data;
}

static
void AddDamagePlum( int clientnum, vec3_t origin, int damage, int time ) {
	damage_plum_data_t *data = GetDamagePlumData( clientnum, time );
	int sound = 0;

	if ( data != NULL ) {
		VectorCopy( origin, data->origin );
		data->damage += damage;
		data->time = time;
		sound = MIN( MAX( 0, data->damage ), 98 )/33;
	}
	trap_S_StartLocalSound( cgs.media.hitSound[sound], CHAN_LOCAL_SOUND );
}

static
float PlumDrawChar( float x, float y, float width, float height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return x + width;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	trap_R_DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cgs.media.charsetShader );
	return x + width;
}

static
float PlumDrawString( float x, float y, float width, float height, const char *string, int align ) {
	float string_width = strlen( string ) * width;
	float pos = x;
	const char *ch;

	if ( align < 0 ) {
		pos -= string_width;
	}
	else if ( align == 0 ) {
		pos -= string_width/2.0f;
	}
	for ( ch = string; *ch; ch++ ) {
		pos = PlumDrawChar( pos, y, width, height, *ch );
	}
	return pos;
}

static
void DrawDamagePlum( damage_plum_data_t* dn ) {
	vec3_t pos;
	int dmgtime;
	float k;
	vec3_t screenpos;

	float char_width, char_height;
	float x, y;
	float w, h;
	float pulsezoom;
	float damagePlumSizeFactor;

	float color[4];

	VectorCopy( dn->origin, pos );
	pos[2] += DAMAGE_PLUM_WORLD_OFFSET;

	dmgtime = cg.time - dn->time;
	k = 0.0f;

	if ( dmgtime > DAMAGE_PLUM_STAY_TIME ) {
		k = ( dmgtime - DAMAGE_PLUM_STAY_TIME ) / (float)( DAMAGE_PLUM_DISSOLVE_TIME );
		pos[2] += k * DAMAGE_PLUM_TRAVEL_DISTANCE;
	}

	CalcModelMatrix( &cg.refdef, s_modelMatrix );
	CalcProjectionMatrix( &cg.refdef, s_projectionMatrix );

	if ( ProjectPoint( &cg.refdef, pos, screenpos ) ) {
		screenpos[1] = cg.refdef.height - screenpos[1];

		damagePlumSizeFactor = MIN( DAMAGE_PLUM_MAX_SIZE_FACTOR, MAX( DAMAGE_PLUM_MIN_SIZE_FACTOR, cg_damagePlumSizeFactor.value ) );

		char_width  = DAMAGE_PLUM_WIDTH  * damagePlumSizeFactor * cg.refdef.height / 480.0f;
		char_height = DAMAGE_PLUM_HEIGHT * damagePlumSizeFactor * cg.refdef.height / 480.0f;

		pulsezoom = DAMAGE_PLUM_DEFAULT_SCALE;
		if ( dmgtime < DAMAGE_PLUM_STAY_TIME && cg_damagePlumPulse.integer != 0 ) {
			pulsezoom = DAMAGE_PLUM_DEFAULT_SCALE + sin( (float)dmgtime / DAMAGE_PLUM_STAY_TIME * M_PI ) * ( DAMAGE_PLUM_PULSE_SCALE - DAMAGE_PLUM_DEFAULT_SCALE ) * ( dn->damage / 100.0f );
		}

		x = screenpos[0];
		y = screenpos[1];
		w = char_width;
		h = char_height;

		if ( x < char_width ) x = char_width;
		if ( y < char_height ) y = char_height;
		if ( x > cg.refdef.width - char_width*2 ) x = cg.refdef.width - char_width*2;
		if ( y > cg.refdef.height - char_height*2 ) y = cg.refdef.height - char_height*2;
		if ( k > 1.0f ) k = 1.0f;
#if 1
		y -= char_height * pulsezoom / 2.0f;
#endif
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
		color[3] = 1.0f - k;
		trap_R_SetColor( color );
#if 1
		PlumDrawString( x, y, char_width * pulsezoom, char_height * pulsezoom, va( "%d", dn->damage ), 0 );
#else
		hud_drawdamageplum( x, y, pulsezoom, dn->damage );
#endif
	}
}

void CG_AddDamagePlum( int clientnum, int damage, vec3_t position ) {
	if ( damage > 0 ) {
		AddDamagePlum( clientnum, position, damage, cg.time );
		s_last_damage_time = cg.time;
	}
}

void CG_DrawDamagePlums( void ) {
	damage_plum_data_t *p;
	float color[4];
	float x, y, w, h;
	float phase;

	if ( cg_damagePlum.integer ) {
		for ( p = s_damage_plums; p - s_damage_plums < MAX_DAMAGE_PLUMS; p++ ) {
			if ( p->damage > 0 ) {
				DrawDamagePlum( p );
				if ( cg.time - p->time > DAMAGE_PLUM_STAY_TIME + DAMAGE_PLUM_DISSOLVE_TIME ) {
					p->damage = 0;
				}
			}
		}
	}
	if ( cg.time - s_last_damage_time < DAMAGE_CROSS_TIME && cg_hitCross.integer ) {
		x = cg.refdef.width / 2.0f;
		y = cg.refdef.height / 2.0f;
		w = 43.0f / 480.0f * cg.refdef.height;
		h = w;
		x -= 0.5f * w;
		y -= 0.5f * h;
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
		phase = (float)( cg.time - s_last_damage_time ) / DAMAGE_CROSS_TIME;
		color[3] = cos( phase * (M_PI / 2.0f + M_PI / 6.0f) - M_PI / 6.0f );
		trap_R_SetColor( color );
		trap_R_DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, cgs.media.hitCrossShader );
	}
}
