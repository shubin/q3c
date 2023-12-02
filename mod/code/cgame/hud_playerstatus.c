#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

void hud_drawstatus( void ) {
	clientInfo_t *ci;
	champion_stat_t *cs;
	playerState_t *ps;
	centity_t *cent;

	float x;

	static float whitecolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float redcolor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	static float bluecolor[] = { 0.25f, 0.8f, 1.0f, 1.0f };
	static float greencolor[] = { 0.4f, 1.0f, 0.26f, 1.0f };

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return;
	}

	ps = &cg.snap->ps;
	cent = &cg_entities[cg.snap->ps.clientNum];
	ci = &cgs.clientinfo[ ps->clientNum ];
	cs = &champion_stats[ ps->champion ];

	// draw head
	if ( hud_media.bigface[cg.predictedPlayerState.champion] ) {
		hud_drawpic( hud_bounds.left + 80 - 36, hud_bounds.bottom - 240 - 36, 196, 196, 0, 0, hud_media.bigface[cg.predictedPlayerState.champion] );
	} else {
		hud_drawpic( hud_bounds.left + 80, hud_bounds.bottom - 240, 160, 160, 0, 0, hud_media.smallface[cg.predictedPlayerState.champion] );
	}

	// draw health
	hud_drawpic( hud_bounds.left + 256, hud_bounds.bottom - 174, 40, 40, 0.5f, 0.5f, hud_media.icon_health );
	hud_bargauge( ps->stats[STAT_HEALTH], cs->base_health, 25, 
		hud_bounds.left + 280, hud_bounds.bottom - 182, 
		26, 16, 5, 
		whitecolor, redcolor, bluecolor );
	x = hud_drawstring( hud_bounds.left + 277, hud_bounds.bottom - 190, 0.78f, hud_media.font_qcde, va( "%d", ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0 ), NULL, 0, 0 );
	hud_drawstring( hud_bounds.left + 277 + x, hud_bounds.bottom - 190, 0.45f, hud_media.font_qcde, va( "/%d", ps->baseHealth ), NULL, 0, 0 );

	// draw armor
	hud_drawpic( hud_bounds.left + 230, hud_bounds.bottom - 84, 40, 40, 0.5f, 0.5f, hud_media.icon_armor );
	hud_bargauge( ps->stats[STAT_ARMOR], cs->base_armor, 25,
		hud_bounds.left + 253, hud_bounds.bottom - 92,
		26, 16, 5,
		whitecolor, redcolor, greencolor );
	x = hud_drawstring( hud_bounds.left + 253, hud_bounds.bottom - 99, 0.78f, hud_media.font_qcde, va( "%d", ps->stats[STAT_ARMOR] > 0 ? ps->stats[STAT_ARMOR] : 0 ), NULL, 0, 0 );
	hud_drawstring( hud_bounds.left + 253 + x, hud_bounds.bottom - 99, 0.45f, hud_media.font_qcde, va( "/%d", cs->base_armor ), NULL, 0, 0 );
}
