#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

#define MAX_OBITUARY 16
#define OBITUARY_TIMEOUT 2500
#define OBITUARY_FADEOUTTIME 2000

typedef struct {
	int	killer;
	int victim;
	weapon_t weapon;
	meansOfDeath_t mod;
	int time;
} obituary_t;

static obituary_t hud_obituary[MAX_OBITUARY];
static int hud_numobituary;

void hud_initobituary( void ) {
	hud_numobituary = 0;
}

static void hud_purgeobituary( void ) {
	static obituary_t obituary[MAX_OBITUARY];
	int i, numobituary;
	memcpy( obituary, hud_obituary, sizeof( obituary ) );
	numobituary = 0;
	for ( i = 0; i < hud_numobituary; i++ ) {
		if ( cg.time - obituary[i].time > OBITUARY_TIMEOUT ) {
			continue;
		}
		memcpy( &hud_obituary[numobituary], &obituary[i], sizeof( obituary_t ) );
		numobituary++;
	}
	hud_numobituary = numobituary;
}

void hud_drawobituary( void ) {
	static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float wepcolor[4];
	static float shadow[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	int i;
	float dim, y, x;
	obituary_t *p;
	weapon_t weapon = WP_NONE;
	qhandle_t deathicon;
	qboolean usemodicon;

	if ( cg.showScores ) {
		return;
	}

	hud_purgeobituary();

	x = hud_bounds.left + 50;
	y = hud_bounds.top + 165;

	for ( p = hud_obituary; p < hud_obituary + hud_numobituary; p++ ) {
		deathicon = hud_media.icon_death;
		usemodicon = qtrue;
		if ( p->weapon > WP_NONE && p->weapon < WP_NUM_WEAPONS ) {
			deathicon = hud_media.icon_weapon[p->weapon];
			wepcolor[0] = hud_weapon_colors[p->weapon][0];
			wepcolor[1] = hud_weapon_colors[p->weapon][1];
			wepcolor[2] = hud_weapon_colors[p->weapon][2];
			wepcolor[3] = color[3];
			if ( p->weapon != WP_DIRE_ORB ) { // orb pseudo-weapon uses "square" icon so treat it as mod icon
				usemodicon = qfalse;
			}
		} else {
			deathicon = hud_media.icon_mod[p->mod];
			wepcolor[0] = wepcolor[1] = wepcolor[2] = 1.0f;
			wepcolor[3] = color[3];
		}

		if ( cg.time - p->time > OBITUARY_FADEOUTTIME ) {
			color[3] = 1.0f - ( (float)cg.time - p->time - OBITUARY_FADEOUTTIME ) / ( OBITUARY_TIMEOUT - OBITUARY_FADEOUTTIME );
		}
		else {
			color[3] = 1.0f;
		}
		shadow[3] = color[3];
		trap_R_SetColor( color );
		if ( p->killer == p->victim || p->killer == ENTITYNUM_WORLD ) {
			hud_drawcolorstring( x, y, 0.4f, hud_media.font_regular, va( "%s ^7%s", cgs.clientinfo[p->victim].name, "suicides" ), shadow, 2, 2, qfalse );
			y += 40;
			continue;
		}
		dim = hud_measurecolorstring( 0.4f, hud_media.font_regular, cgs.clientinfo[p->killer].name );
		hud_drawcolorstring( x, y, 0.4f, hud_media.font_regular, cgs.clientinfo[p->killer].name, shadow, 2, 2, qfalse );
		hud_drawcolorstring( x + dim + ( usemodicon ? 48 : 64 ), y, 0.4f, hud_media.font_regular, cgs.clientinfo[p->victim].name, shadow, 2, 2, qfalse );
		if ( !usemodicon ) {
			trap_R_SetColor( wepcolor );
		}
		hud_drawpic( x + dim + ( usemodicon ? 24 : 32 ), y - 8, usemodicon ? 36 : 48, usemodicon ? 36 : 48, 0.5f, 0.5f, deathicon );
		y += 40;
	}
	trap_R_SetColor( NULL );
}

void CG_AddObituary( int killer, int victim, weapon_t weapon, meansOfDeath_t mod ) {
	int i;

	if ( hud_numobituary == MAX_OBITUARY ) {
		for ( i = 0; i < MAX_OBITUARY - 1; i++ ) {
			memcpy( &hud_obituary[i], &hud_obituary[i + 1], sizeof( obituary_t ) );
		}
		hud_numobituary--;
	}
	hud_obituary[hud_numobituary].killer = killer;
	hud_obituary[hud_numobituary].victim = victim;
	hud_obituary[hud_numobituary].mod = mod;
	hud_obituary[hud_numobituary].weapon = weapon;
	hud_obituary[hud_numobituary].time = cg.time;
	hud_numobituary++;
}
