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

	if ( cg.clientNum == ps->clientNum && cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) { // we're playing
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
	} else {
		hud_drawscorebar_ffa( hud_bounds.top + 54, color2, cgs.clientinfo[cgs.leader1].name, 1, cgs.scores1 );
		hud_drawscorebar_ffa( hud_bounds.top + 54 + 28, color1, cgs.clientinfo[cgs.leader2].name, 2, cgs.scores2 );
	}
}

void hud_drawscores_ffa( void ) {
	char *text;
	float centerx, y, dim;
	score_t *scores;
	clientInfo_t *ci;
	int d1, d2;
	static float mapnameColor[] = { 0.79f, 0.5f, 0.33f, 1.0f };
	static float black[] = { 0.0f, 0.0f, 0.0f, 0.8f };
	static float gray[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	static float gray2[] = { 0.15f, 0.15f, 0.15f, 0.8f };
	static float translucent[] = { 1, 1, 1, 0.15f };
	static float header[] = { 0.15f, 0.05f, 0.05f, 0.8f };
	static float myColor[] = { 0.26f/1.5f, 0.70f/1.5f, 0.89f/1.5f, 0.8f };

	if ( !cg.showScores || cgs.gametype != GT_FFA ) {
		return;
	}

	centerx = ( hud_bounds.right + hud_bounds.left ) / 2;
	y = hud_bounds.top + 36;

	hud_drawbar( centerx, y, 768, 110, 0.5f, 0.0f, header );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 244, y + 53, 0.75f, hud_media.font_regular, "DEATHMATCH", NULL, 0, 0 );
	trap_R_SetColor( mapnameColor );
	hud_drawstring( centerx - 244, y + 87, 0.5f, hud_media.font_regular, CG_ConfigString( CS_MESSAGE ), NULL, 0, 0 );
	trap_R_SetColor( NULL );
	hud_drawpic( centerx - 318, y + 55, 80, 80, 0.5f, 0.5f, trap_R_RegisterShader( "menu/art/skill5" ) );

	y += 120;

	hud_drawbar( centerx, y, 768, 45, 0.5f, 0.0f, black );
	trap_R_SetColor( gray );
	hud_drawstring( centerx - 290, y + 32, 0.4f, hud_media.font_regular, "PLAYER", NULL, 0, 0 );
	hud_drawstring( centerx +  92, y + 32, 0.4f, hud_media.font_regular, "SCORE", NULL, 0, 0 );
	hud_drawstring( centerx + 208, y + 32, 0.4f, hud_media.font_regular, "KDR", NULL, 0, 0 );
	hud_drawstring( centerx + 298, y + 32, 0.4f, hud_media.font_regular, "PING", NULL, 0, 0 );
	y += 45;

	for ( int i = 0; i < cg.numScores; i++ ) {
		scores = &cg.scores[i];
		if ( scores->client == cg.clientNum ) {
			if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR )
				continue;
			if ( cg.snap->ps.clientNum != cg.clientNum ) {
				continue;
			}
		}
		ci = &cgs.clientinfo[scores->client];

		hud_drawbar( centerx, y, 768, 72, 0.5f, 0.0f, black );
		hud_drawbar( centerx, y, 760, 68, 0.5f, 0.0f, scores->client == cg.clientNum ? myColor : gray2 );
		hud_drawbar( centerx + 124, y + 34, 68, 68, 0.5f, 0.5f, translucent );
		trap_R_SetColor( NULL );
		hud_drawpic( centerx - 346, y + 34, 58, 58, 0.5f, 0.5f, hud_media.face[ ci->champion ] );
		hud_drawcolorstring( centerx - 290, y + 46, 0.5f, hud_media.font_regular, ci->name, NULL, 0, 0, qfalse );

		text = va( "%d", ci->score );
		dim = hud_measurestring( 0.5f, hud_media.font_regular, text );
		hud_drawstring( centerx + 126 - dim / 2, y + 46, 0.5f, hud_media.font_regular, text, NULL, 0, 0 );

		if ( scores->deaths == 0 ) {
			text = va( "%d", scores->kills );
		} else {
			d1 = scores->kills / scores->deaths;
			d2 = 10 * ( scores->kills % scores->deaths ) / scores->deaths;
			if ( d2 == 0 ) { 
				text = va( "%d", d1 );
			} else {
				text = va( "%d.%d", d1, d2 );
			}
		}

		dim = hud_measurestring( 0.5f, hud_media.font_regular, text );
		hud_drawstring( centerx + 228 - dim / 2, y + 46, 0.5f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d", scores->ping );
		dim = hud_measurestring( 0.5f, hud_media.font_regular, text );
		hud_drawstring( centerx + 322 - dim / 2, y + 46, 0.5f, hud_media.font_regular, text, NULL, 0, 0 );

		y += 72;
	}
	trap_R_SetColor( NULL );
}
