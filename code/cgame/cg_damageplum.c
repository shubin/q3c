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
//#include "../q3c/linked_list.h"
//#include "../q3c/qcCamera.h"

static struct qcCamera *cam = NULL;

typedef struct damage_plum_data_s {
	vec3_t origin;
	int damage;
	int time;
	int clientnum;
} damage_plum_data_t;

#define DAMAGE_PLUM_WIDTH	12
#define DAMAGE_PLUM_HEIGHT	12

#define DAMAGE_PLUM_STAY_TIME				200
#define DAMAGE_PLUM_PULSE_SCALE				1.5f
#define DAMAGE_PLUM_DEFAULT_SCALE			1.0f
#define DAMAGE_PLUM_DISSOLVE_TIME			2500
#define DAMAGE_PLUM_WORLD_OFFSET			30.0f;
#define DAMAGE_PLUM_TRAVEL_DISTANCE			60.0f
#define DAMAGE_PLUM_ACCUMULATION_THRESHOLD	300
#define DAMAGE_CROSS_TIME					200

static qboolean s_damage_plum_initialized = qfalse;
//static linked_list_t s_damage_plums = { 0 };
static int s_last_damage_time = -1;

void CG_InitDamagePlums( void ) {
	//linked_list_create( &s_damage_plums, sizeof( damage_plum_data_t ) );
	s_damage_plum_initialized = qtrue;
	//cam = qcCreateCamera();
}

void CG_ShutdownDamagePlums( void ) {
	//linked_list_clear( &s_damage_plums );
	s_damage_plum_initialized = qfalse;
	//qcDestroyCamera( cam );
}

static
damage_plum_data_t *GetDamagePlumData( int clientnum, int time ) {
	damage_plum_data_t *data = NULL;

	if ( cg_damagePlum.integer == 2 ) {
		//linked_list_iter_t iter;
		//for ( damage_plum_data_t* p = (damage_plum_data_t*)linked_list_iterate( &s_damage_plums, &iter ); p; p = (damage_plum_data_t*)linked_list_advance( &iter ) ) {
		//	if ( p->clientnum == clientnum && time - p->time < DAMAGE_PLUM_ACCUMULATION_THRESHOLD ) {
		//		data = p;
		//		break;
		//	}
		//}
	}

	if ( data == NULL ) {
		//data = (damage_plum_data_t*)linked_list_push_back( &s_damage_plums );
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
	if ( data != NULL ) {
		VectorCopy( origin, data->origin );
		data->damage += damage;
		data->time = time;
		int damage = min( max( 0, data->damage ), 99 );
#if 0
		trap_S_StartLocalSound( cgs.media.hitSound[damage/10], CHAN_LOCAL_SOUND );
#else
		damage /= 33;
		//trap_S_StartLocalSound( cgs.media.hitSound[damage*2], CHAN_LOCAL_SOUND );
#endif
	}
}

void ReleaseDamagePlums( void ) {
	//linked_list_iter_t iter;
	//for ( damage_plum_data_t* p = (damage_plum_data_t*)linked_list_iterate( &s_damage_plums, &iter ); p; p = (damage_plum_data_t*)linked_list_advance( &iter ) ) {
	//	if ( cg.time - p->time > DAMAGE_PLUM_STAY_TIME + DAMAGE_PLUM_DISSOLVE_TIME ) {
	//		linked_list_remove_item(&s_damage_plums, iter.current);
	//	}
	//}
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
	if ( align < 0 ) {
		pos -= string_width;
	}
	else if ( align == 0 ) {
		pos -= string_width/2.0f;
	}
	for ( const char* ch = string; *ch; ch++ ) {
		pos = PlumDrawChar( pos, y, width, height, *ch );
	}
	return pos;
}

static
void DrawDamagePlum( damage_plum_data_t* dn ) {
	vec3_t pos;
	VectorCopy( dn->origin, pos );
	pos[2] += DAMAGE_PLUM_WORLD_OFFSET;

	int dmgtime = cg.time - dn->time;
	float k = 0.0f;

	if ( dmgtime > DAMAGE_PLUM_STAY_TIME ) {
		k = ( dmgtime - DAMAGE_PLUM_STAY_TIME ) / (float)( DAMAGE_PLUM_DISSOLVE_TIME );
		pos[2] += k * DAMAGE_PLUM_TRAVEL_DISTANCE;
	}

	qcUpdateCamera( cam, cg.refdef.vieworg, cg.refdef.viewaxis, cg.refdef.fov_x, 
		0.0f, 0.0f, cg.refdef.width, cg.refdef.height );

	vec3_t screenpos;
	if ( qcProjectPoint( cam, pos, screenpos ) ) {
 		float char_width = DAMAGE_PLUM_WIDTH * cg.refdef.height / 480.0f;
		float char_height = DAMAGE_PLUM_HEIGHT * cg.refdef.height / 480.0f;

		float pulsezoom = DAMAGE_PLUM_DEFAULT_SCALE;
		if ( dmgtime < DAMAGE_PLUM_STAY_TIME && cg_damagePlumPulse.integer != 0 ) {
			pulsezoom = DAMAGE_PLUM_DEFAULT_SCALE + sinf( (float)dmgtime / DAMAGE_PLUM_STAY_TIME * M_PI ) * ( DAMAGE_PLUM_PULSE_SCALE - DAMAGE_PLUM_DEFAULT_SCALE ) * ( dn->damage / 100.0f );
		}

		float x = screenpos[0];
		float y = screenpos[1];
		float w = char_width;
		float h = char_height;

		if ( x < char_width ) x = char_width;
		if ( y < char_height ) y = char_height;
		if ( x > cg.refdef.width - char_width*2 ) x = cg.refdef.width - char_width*2;
		if ( y > cg.refdef.height - char_height*2 ) y = cg.refdef.height - char_height*2;
		if ( k > 1.0f ) k = 1.0f;
		y -= char_height * pulsezoom / 2.0f;
		float color[] = { 1, 1, 1, 1.0f - k };
		trap_R_SetColor( color );
		PlumDrawString( x, y, char_width * pulsezoom, char_height * pulsezoom, va( "%d", dn->damage ), 0 );
	}
}

void CG_AddDamagePlum( int clientnum, int damage, vec3_t position ) {
	if ( damage > 0 ) {
		AddDamagePlum( clientnum, position, damage, cg.time );
		s_last_damage_time = cg.time;
	}
}

void CG_DrawDamagePlums( void ) {
	//if ( s_damage_plums.first != NULL && cg_damagePlum.integer > 0 ) {
	//	linked_list_iter_t iter;
	//	for ( damage_plum_data_t *p = (damage_plum_data_t*)linked_list_iterate( &s_damage_plums, &iter ); p; p = (damage_plum_data_t*)linked_list_advance( &iter ) ) {
	//		DrawDamagePlum( p );
	//	}
	//	ReleaseDamagePlums();
	//}
	if ( cg.time - s_last_damage_time < DAMAGE_CROSS_TIME && cg_hitCross.integer ) {
		float x = cg.refdef.width / 2.0f;
		float y = cg.refdef.height / 2.0f;
		float w = 43.0f / 480.0f * cg.refdef.height;
		float h = w;
		x -= 0.5f * w;
		y -= 0.5f * h;
		float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float phase = (float)( cg.time - s_last_damage_time ) / DAMAGE_CROSS_TIME;
		color[3] = cosf( phase * (M_PI / 2.0f + M_PI / 6.0f) - M_PI / 6.0f );
		trap_R_SetColor( color );
		trap_R_DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, cgs.media.hitCrossShader );
	}
}
