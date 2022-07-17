#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

#define MAX_PICKUPS 16
#define PICKUP_TIMEOUT 1500
#define PICKUP_FADEOUTTIME 1000

typedef struct {
	int		itemNum;
	int		time;
} pickup_t;

static pickup_t hud_pickups[MAX_PICKUPS];
static int hud_numpickups;

void hud_initpickups( void ) {
	hud_numpickups = 0;
}

void hud_purgepickups( void ) {
	static pickup_t pickups[MAX_PICKUPS];
	int i, numpickups;
	memcpy( pickups, hud_pickups, sizeof( pickups ) );
	numpickups = 0;
	for ( i = 0; i < hud_numpickups; i ++ ) {
		if ( cg.time - pickups[i].time > PICKUP_TIMEOUT ) {
			continue;
		}
		memcpy( &hud_pickups[numpickups], &pickups[i], sizeof( pickup_t ) );
		numpickups++;
	}
	hud_numpickups = numpickups;
}

void hud_drawpickups( void ) {
	static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float shadow[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float dim, y;
	pickup_t *p;

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return;
	}

	hud_purgepickups();

	y = 80;

	for ( p = hud_pickups; p < hud_pickups + hud_numpickups; p++ ) {
		if ( cg.time - p->time > PICKUP_FADEOUTTIME ) {
			color[3] = 1.0f - ( (float)cg.time - p->time ) / ( PICKUP_TIMEOUT - PICKUP_FADEOUTTIME );
		}
		else {
			color[3] = 1.0f;
		}
		shadow[3] = color[3];
		trap_R_SetColor( color );
		hud_drawpic( hud_bounds.right - 60, y, 32, 32, 0.5f, 0.5f, hud_media.itemicons[p->itemNum] );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, bg_itemlist[p->itemNum].pickup_name );
		hud_drawstring( hud_bounds.right - 88 - dim, y + 8, 0.4f, hud_media.font_regular, bg_itemlist[p->itemNum].pickup_name, shadow, 2, 2 );
		y += 40;
	}
	trap_R_SetColor( NULL );
}

void CG_AddPickup( int itemNum ) {
	int i;

	if ( hud_numpickups == MAX_PICKUPS ) { // remove the top pickup if the list is full
		for ( i = 0; i < MAX_PICKUPS - 1; i++ ) {
			memcpy( &hud_pickups[i], &hud_pickups[i + 1], sizeof( pickup_t ) );
		}
		hud_numpickups--;
	}
	for ( i = 0; i < hud_numpickups; i++ ) {
		if ( hud_pickups[i].itemNum == itemNum ) { // update item pickup time if i's already in the list
			hud_pickups[i].time = cg.time;
			return;
		}
	}
	hud_pickups[hud_numpickups].itemNum = itemNum;
	hud_pickups[hud_numpickups].time = cg.time;
	hud_numpickups++;
}
