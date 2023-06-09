#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

static
void calc_sector_point( float angle, float *x, float *y ) {
	if ( ( angle > 315.0f ) || ( angle <= 45.0f ) ) {
		*x = 1.0f;
		*y = tan( DEG2RAD( angle ) );
		return;
	}
	if ( ( angle > 45.0f ) && ( angle <= 135.0f ) ) {
		*x = tan( DEG2RAD( 90.0f - angle ) );
		*y = 1.0f;
		return;
	}
	if ( ( angle > 135.0f ) && ( angle <= 225.0f ) ) {
		*x = -1.0f;
		*y = -tan( DEG2RAD( angle ) );
		return;
	}
	*x = -tan( DEG2RAD( 90.0f - angle ) );
	*y = -1.0f;
}

static
int get_next_corner( int angle ) {
	angle /= 45;
	angle *= 45;
	angle += 45;
	if ( angle % 90 == 0 ) {
		angle += 45;
	}
	angle %= 360;
	return angle;
}

#define TOTEM_ANGLE 60
#define TOTEM_RADIUS 70
#define TOTEM_BLINK_CYCLE 500

static
void LerpColor( vec4_t a, vec4_t b, vec4_t c, float t ) {
	int i;

	// lerp and clamp each component
	for ( i = 0; i < 4; i++ ) {
		c[i] = a[i] + t * ( b[i] - a[i] );
		if ( c[i] < 0 )
			c[i] = 0;
		else if ( c[i] > 1.0 )
			c[i] = 1.0;
	}
}

static
void hud_draw_totems( float cx, float cy, int num_totems ) {
	float angle, step, x, y;
	int i;
	float yellow[] = { 1.0f, 0.75f, 0.4f, 1.0f };
	float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float color[4];

	angle = ( - TOTEM_ANGLE / 2.0f - 90 ) / 180.0f * 3.14159265359f;
	step = ( TOTEM_ANGLE / (float)( MAX_TOTEMS - 1 ) ) / 180.0f * 3.14159265359f;
	for ( i = 0; i < MAX_TOTEMS; i++, angle += step ) {
		x = cx + TOTEM_RADIUS * cosf( angle );
		y = cy + TOTEM_RADIUS * sinf( angle );
		white[3] = 1.0f;
		trap_R_SetColor( black );
		hud_drawpic( x, y, 32, 32, 0.5, 0.5, hud_media.totemshadows[i] );
		if ( i < num_totems ) {
			if ( num_totems == MAX_TOTEMS ) {
				if ( ( cg.time / TOTEM_BLINK_CYCLE ) % 2 ) {
					LerpColor( yellow, white, color, (float)( cg.time % TOTEM_BLINK_CYCLE ) / TOTEM_BLINK_CYCLE );
				} else {
					LerpColor( white, yellow, color, (float)( cg.time % TOTEM_BLINK_CYCLE ) / TOTEM_BLINK_CYCLE );
				}
				trap_R_SetColor( color );
			} else {
				trap_R_SetColor( yellow );
			}
		} else {
			white[3] = 0.5f;
			trap_R_SetColor( white );
		}
		hud_drawpic( x, y, 32, 32, 0.5, 0.5, hud_media.totems[i] );
	}
	trap_R_SetColor( NULL );
}

// ability status
void hud_draw_ability( void ) {
	centity_t *cent;
	playerState_t *ps;
	clientInfo_t *ci;
	float yellow[] = { 1.0f, 0.8f, 0.0f, 1.0f };
	float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	float overall, current, dim;
	int sec_left;
	float gaugex, gaugey, gaugesize;
	float x[16], y[16];
	int num, pos;
	int start_angle, end_angle, angle, ta;
	float scalex, scaley, offsetx, offsety;
	float x0, y0, x1, y1, x2, y2;
	float s0, t0, s1, t1, s2, t2;

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return;
	}

	ps = &cg.snap->ps;
	cent = &cg_entities[cg.snap->ps.clientNum];
	ci = &cgs.clientinfo[cg.snap->ps.clientNum];

	overall = champion_stats[ps->champion].ability_cooldown;
	current = ps->ab_time;

	start_angle = 270;
	end_angle = (int)( 270 + current/overall * 360);

	gaugex = ( hud_bounds.left + hud_bounds.right ) / 2;
	gaugey = hud_bounds.bottom - 160;
	gaugesize = 128;

	if ( ps->champion == CHAMP_GALENA ) {
		hud_draw_totems( gaugex, gaugey, ps->ab_num );
	}

	// ability gauge background
	white[3] = 0.5f;
	trap_R_SetColor( white );
	hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, hud_media.abbg );
	trap_R_SetColor( NULL );

	if ( ps->ab_time == champion_stats[ps->champion].ability_cooldown) {
		// ability is fully charged, draw the ring and its glow
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, hud_media.ringgauge );
		trap_R_SetColor( yellow );
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, hud_media.ringglow );
		trap_R_SetColor( NULL );
		// draw the ability icon
		hud_drawpic( gaugex, gaugey, gaugesize * 0.6f, gaugesize * 0.6f, 0.5f, 0.5f, hud_media.skillicon[ps->champion] );
		return;
	} else {
		// ability is not fully charged, draw the full ring semi-transparently
		white[3] = 0.5f;
		trap_R_SetColor( white) ;
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, hud_media.ringgauge );
		trap_R_SetColor( NULL );
	}

	if ( ps->ab_flags & ABF_ENGAGED ) {
		// ability is enaged, so draw the icon semi-transparently
		trap_R_SetColor( white );
		hud_drawpic( gaugex, gaugey, gaugesize * 0.6f, gaugesize * 0.6f, 0.5f, 0.5f, hud_media.skillicon[ps->champion] );
		trap_R_SetColor( NULL );
		if ( ps->champion == CHAMP_ANARKI && ci->abilityActivationTime != 0 ) {
			current = cg.time - ci->abilityActivationTime;
			overall = champion_stats[CHAMP_ANARKI].ability_duration * 100;
			if ( current < 0 || current > overall ) {
				return;
			}
			start_angle = 270;
			end_angle = (int)( 270 + ( 1.0f - current/overall ) * 360);

			if ( current > 0 && current < 1000 ) {
				cg.blurFactor = 1.0f - current/1000;
			}
			else {
				cg.blurFactor = 0.0f;
			}
		} if ( ps->champion == CHAMP_VISOR ) {
			current = cg.time - cg.snap->ps.ab_misctime;
			overall = champion_stats[CHAMP_VISOR].ability_duration * 100;
			if ( current < 0 || current > overall ) {
				return;
			}
			start_angle = 270;
			end_angle = ( int )( 270 + ( 1.0f - current / overall ) * 360 );
		} else {
			return;
		}
	}

	// calculate points along the rectangle perimeter which represent the needed ring sector
	num = 0;
	angle = start_angle;
	if ( end_angle < start_angle ) {
		end_angle += 360;
	}
	while( 1 ) {
		calc_sector_point( angle % 360, &x[num], &y[num] );
		num++;
		ta = get_next_corner( angle );
		if ( ta < angle ) {
			ta += 360;
		}
		angle = ta;
		if ( angle >= end_angle ) {
			calc_sector_point( end_angle % 360, &x[num], &y[num] );
			num++;
			break;
		}
	}

	// convert the points calculated above into real coordinates and render them in groups of 4
	scalex = gaugesize / 2;
	scaley = gaugesize / 2;
	offsetx = gaugex;
	offsety = gaugey;

	pos = 0;
	x0 = offsetx; y0 = offsety; s0 = 0.5f; t0 = 0.5f;
	hud_translate_point( &x0, &y0 );
	while ( 1 ) {
		x1 = x[pos] * scalex + offsetx; s1 = x[pos] / 2.0f + 0.5f;
		y1 = y[pos] * scaley + offsety; t1 = y[pos] / 2.0f + 0.5f;
		hud_translate_point( &x1, &y1 );
	
		x2 = x[pos + 1] * scalex + offsetx; s2 = x[pos + 1] / 2.0f + 0.5f;
		y2 = y[pos + 1] * scaley + offsety;	t2 = y[pos + 1] / 2.0f + 0.5f;
		hud_translate_point( &x2, &y2 );

		trap_R_DrawTriangle( x0, y0, x1, y1, x2, y2, s0, t0, s1, t1, s2, t2, hud_media.ringgauge );
		pos += 1;
		if ( pos >= num - 1 ) {
			break;
		}
	}

	if ( !( ps->ab_flags & ABF_ENGAGED ) ) {
		// draw the ability timer (how many seconds left to the full recharge)
		sec_left = champion_stats[ps->champion].ability_cooldown - ps->ab_time;
		dim = hud_measurestring( 0.5f, hud_media.font_qcde, va( "%d", sec_left ) );
		hud_drawstring( gaugex - dim / 2, gaugey + 15, 0.5f, hud_media.font_qcde, va( "%d", sec_left ), NULL, 0, 0 );
	}
}
