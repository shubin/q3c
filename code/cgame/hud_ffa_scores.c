#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

// single brief score bar for FFA
void hud_drawscorebar_ffa( float y, float *color, const char *playername, int rank, int score ) {
	float centerx, dim, mdim;

	centerx = 0.5f * ( hud_bounds.left + hud_bounds.right );

	trap_R_SetColor( color );
	hud_drawpic( centerx - 107, y, 43, 26, 0.0f, 0.0f, cgs.media.whiteShader );
	hud_drawpic( centerx - 63, y, 228, 26, 0.0f, 0.0f, cgs.media.whiteShader );
	hud_drawpic( centerx + 166, y, 43, 26, 0.0f, 0.0f, cgs.media.whiteShader );

	trap_R_SetColor( NULL );
	hud_drawcolorstring( centerx - 59, y + 21, 0.35f, hud_media.font_regular, playername, NULL, 0, 0, qfalse );
	dim = hud_measurestring( 0.35f, hud_media.font_regular, va( "%d", score ) );
	mdim = score < 0 ? hud_measurestring( 0.35f, hud_media.font_regular, "-" ) : 0;
	hud_drawstring( centerx + 188 - dim/2 - mdim / 2, y + 21, 0.35f, hud_media.font_regular, va( "%d", score ), NULL, 0, 0 );
	dim = hud_measurestring( 0.35f, hud_media.font_regular, va( "%d", rank) );
	hud_drawstring( centerx - 84 - dim/2, y + 21, 0.35f, hud_media.font_regular, va( "%d", rank ), NULL, 0, 0 );
}

// brief score bar for FFA, along with the timer
void hud_drawscores_brief_ffa( void ) {
	static float color1[] = { 0.85f/1.5f, 0.46f/1.5f, 0.18f/1.5f, 0.5f };
	static float color2[] = { 0.26f/1.5f, 0.70f/1.5f, 0.89f/1.5f, 0.5f };

	playerState_t *ps;

	float centerx;
	int rank, other, otherscore;

	if ( cgs.gametype != GT_FFA ) {
		return;
	}

	if ( cg.showScores ) {
		return;
	}

	centerx = 0.5f * ( hud_bounds.left + hud_bounds.right );

	if ( cg.predictedPlayerState.pm_type == PM_DEAD ) {
		hud_drawtimer( centerx - 8, hud_bounds.top + 100 );
		return;
	}
	hud_drawtimer( centerx - 185, hud_bounds.top + 100 );

	ps = &cg.snap->ps;
	rank = ps->persistant[PERS_RANK] & ( RANK_TIED_FLAG - 1 );

	if ( rank == 0 ) {
		// if we have rank 0, then always draw our name on top
		hud_drawscorebar_ffa( hud_bounds.top + 54, color1, cgs.clientinfo[cg.clientNum].name, 1, ps->persistant[PERS_SCORE] );
		// choose the second leader
		other = cgs.leader1;
		otherscore = cgs.scores1;
		if ( other == cg.clientNum ) { // any of leader1 and leader2 can be us, so choose the one which is not us
			other = cgs.leader2;
			otherscore = cgs.scores2;
		}
		if ( other != -1 ) {
			hud_drawscorebar_ffa( hud_bounds.top + 54 + 28, color2, cgs.clientinfo[other].name, 2, otherscore );
		}
	} else {
		hud_drawscorebar_ffa( hud_bounds.top + 54, color2, cgs.clientinfo[cgs.leader1].name, 1, cgs.scores1 );
		hud_drawscorebar_ffa( hud_bounds.top + 54 + 28, color1, cgs.clientinfo[cg.clientNum].name, rank + 1, ps->persistant[PERS_SCORE] );
	}
}
