#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

void hud_progressbar( float gauge, float left, float top, float width, float height, float thickness, float *color, float *colorEmpty ) {
	float split = ( gauge > 0 ? gauge : gauge + 1 ) * ( width - 2.0f * thickness );

	hud_drawbar( left, top, width, thickness, 0.0f, 0.0f, color );
	hud_drawbar( left, top + height - thickness, width, thickness, 0.0f, 0.0f, color );
	hud_drawbar( left, top + thickness, thickness, height - 2 * thickness, 0.0f, 0.0f, color );
	hud_drawbar( left + width - thickness, top + thickness, thickness, height - 2 * thickness, 0.0f, 0.0f, color );
	hud_drawbar( left + thickness, top + thickness,
		split, height - 2 * thickness, 0.0f, 0.0f, 
		gauge > 0 ? color : colorEmpty
	);
	hud_drawbar( left + thickness + split, top + thickness,
		( width - 2.0f * thickness ) - split, height - 2 * thickness, 0.0f, 0.0f, 
		gauge > 0 ? colorEmpty : color
	);
}

// brief tdm scores with timer
void hud_drawscores_brief_tdm( void ) {
	playerState_t *ps;
	static float color1[] = { 0.12f, 0.38f, 0.53f, 1.0f };
	static float color2[] = { 0.42f, 0.07f, 0.07f, 1.0f };
	static float color1e[] = { 0.12f/2, 0.38f/2, 0.53f/2, 1.0f };
	static float color2e[] = { 0.42f/2, 0.07f/2, 0.07f/2, 1.0f };
	static float black[] = { 0.0f, 0.0f, 0.0f, 0.8f };
	static float gray[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	const char *text;
	float centerx, dim;
	float gauge1, gauge2;
//	clientInfo_t *enemyInfo;

	if ( cgs.gametype != GT_TEAM2V2 && cgs.gametype != GT_TEAM ) {
		return;
	}

	if ( cg.showScores ) {
		return;
	}

	centerx = ( hud_bounds.left + hud_bounds.right ) / 2;
	hud_drawtimer( centerx - 8, hud_bounds.top + 84 );
	
	//if ( cg.predictedPlayerState.pm_type == PM_DEAD ) {
	//	return;
	//}

	ps = &cg.snap->ps;

	// big red bars with scores

/*
	hud_drawbar( centerx - 280, hud_bounds.top + 44, 220, 48, 0.0f, 0.0f, color1 );
	hud_drawbar( centerx + 80,  hud_bounds.top + 44, 220, 48, 0.0f, 0.0f, color2 );
*/
	gauge1 = gauge2 = 0.0f;
	if ( cgs.scores1 != SCORE_NOT_PRESENT && cgs.fraglimit > 0 ) {
		gauge1 = cgs.scores1 / ( float )cgs.fraglimit;
	}
	if ( cgs.scores2 != SCORE_NOT_PRESENT && cgs.fraglimit > 0 ) {
		gauge2 = cgs.scores2 / ( float )cgs.fraglimit;
	}

	hud_progressbar( gauge2, centerx - 280, hud_bounds.top + 42, 220, 50, 2, color1, color1e );
	hud_progressbar( -gauge1, centerx + 80, hud_bounds.top + 42, 220, 50, 2, color2, color2e );
	// small black bars for team names
	hud_drawbar( centerx - 280, hud_bounds.top + 92, 220, 21, 0.0f, 0.0f, black );
	hud_drawbar( centerx + 80,  hud_bounds.top + 92, 220, 21, 0.0f, 0.0f, black );

	black[3] = 1.0f;

	if ( cgs.scores2 != SCORE_NOT_PRESENT ) {
		// blue team score
		text = va( "%d", cgs.scores2 );
		dim = hud_measurestring( 0.6f, hud_media.font_qcde, text );
		trap_R_SetColor( NULL );
		hud_drawstring( centerx - 270, hud_bounds.top + 84, 0.6f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	if ( cgs.scores1 != SCORE_NOT_PRESENT ) {
		// red team score
		text = va( "%d", cgs.scores1 );
		dim = hud_measurestring( 0.6f, hud_media.font_qcde, text );
		trap_R_SetColor( NULL );
		hud_drawstring( centerx + 290 - dim, hud_bounds.top + 84, 0.6f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	if ( cgs.scores2 != SCORE_NOT_PRESENT ) {
		trap_R_SetColor( gray );
		hud_drawcolorstring( centerx - 278, hud_bounds.top + 108, 0.3f, hud_media.font_regular, "BLUE TEAM", NULL, 0, 0, qtrue );
	}

	if ( cgs.scores1 != SCORE_NOT_PRESENT ) {
		dim = hud_measurecolorstring( 0.3f, hud_media.font_regular, "RED TEAM" );
		trap_R_SetColor( gray );
		hud_drawcolorstring(centerx + 298 - dim, hud_bounds.top + 108, 0.3f, hud_media.font_regular, "RED TEAM", NULL, 0, 0, qtrue );
	}
}

// tournament scores with weapon usage stats, item stats etc.
void hud_drawscores_tdm( void ) {
	char *text;
	float centerx, y, dim;
	score_t *scores;
	clientInfo_t *ci;
	team_t team;
	int d1, d2;
	static float mapnameColor[] = { 0.79f, 0.5f, 0.33f, 1.0f };
	static float black[] = { 0.0f, 0.0f, 0.0f, 0.8f };
	static float gray[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	static float gray2[] = { 0.15f, 0.15f, 0.15f, 0.8f };
	static float translucent[] = { 1, 1, 1, 0.15f };
	static float header[] = { 0.15f, 0.05f, 0.05f, 0.8f };
	static float myColor[] = { 0.26f / 1.5f, 0.70f / 1.5f, 0.89f / 1.5f, 0.8f };
	static float color1[] = { 0.12f, 0.38f, 0.53f, 1.0f };
	static float color2[] = { 0.42f, 0.07f, 0.07f, 1.0f };

	if ( !cg.showScores || ( cgs.gametype != GT_TEAM && cgs.gametype != GT_TEAM2V2 ) ) {
		return;
	}

	centerx = ( hud_bounds.right + hud_bounds.left ) / 2;
	y = hud_bounds.top + 36;

	hud_drawbar( centerx - 770, y, 1540, 110, 0.0f, 0.0f, header );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 630, y + 53, 0.75f, hud_media.font_regular, 
		cgs.gametype == GT_TEAM2V2 ? "TEAM DEATHMATCH 2v2" : "TEAM DEATHMATCH",
		NULL, 0, 0 );
	trap_R_SetColor( mapnameColor );
	hud_drawstring( centerx - 630, y + 87, 0.5f, hud_media.font_regular, CG_ConfigString( CS_MESSAGE ), NULL, 0, 0 );
	trap_R_SetColor( NULL );
	hud_drawpic( centerx - 703, y + 55, 80, 80, 0.5f, 0.5f, trap_R_RegisterShader( "menu/art/skill5" ) );

	centerx -= 390;

	y = hud_bounds.top + 190;
	hud_drawbar( centerx - 384, y, 768, 38, 0.0f, 0.0f, color1 );
	hud_drawbar( centerx + 396, y, 768, 38, 0.0f, 0.0f, color2 );
	trap_R_SetColor( color1 );
	hud_drawquad(
		centerx + 384 - 130, y - 20, 0, 0,
		centerx + 384, y - 20, 0, 1,
		centerx + 384, y, 1, 1,
		centerx + 384 - 150, y, 0, 1,
		cgs.media.whiteShader
	);
	//trap_R_SetColor( black );
	//hud_drawquad(
	//	centerx - 384, y - 20, 0, 0,
	//	centerx + 384 - 130, y - 20, 0, 1,
	//	centerx + 384 - 150, y, 1, 1,
	//	centerx - 384, y, 1, 0,
	//	cgs.media.whiteShader
	//	);
	trap_R_SetColor( color2 );
	hud_drawquad(
		centerx + 396 + 130, y - 20, 0, 0,
		centerx + 396 + 150, y, 0, 1,
		centerx + 396, y, 1, 1,
		centerx + 396, y - 20, 0, 1,
		cgs.media.whiteShader
	);

	text = va( "%d", cgs.scores2 );
	dim = hud_measurestring( 0.8f, hud_media.font_qcde, text );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx + 384 - 4 - dim, y + 30, 0.8f, hud_media.font_qcde, text, NULL, 0, 0 );
	hud_drawstring( centerx + 406, y + 30, 0.8f, hud_media.font_qcde, va( "%d", cgs.scores1 ), NULL, 0, 0 );

	hud_drawstring( centerx - 384 + 16, y + 30, 0.5f, hud_media.font_regular, "BLUE TEAM", NULL, 0, 0 );
	dim = hud_measurestring( 0.5f, hud_media.font_regular, "RED TEAM" );
	hud_drawstring( centerx + 396 + 768 - 16 - dim, y + 30, 0.5f, hud_media.font_regular, "RED TEAM", NULL, 0, 0 );

	team = TEAM_BLUE;

draw_again:

	y = hud_bounds.top + 240;

	hud_drawbar( centerx, y, 768, 45, 0.5f, 0.0f, black );
	trap_R_SetColor( gray );
	hud_drawstring( centerx - 290, y + 32, 0.4f, hud_media.font_regular, "PLAYER", NULL, 0, 0 );
	hud_drawstring( centerx + 92, y + 32, 0.4f, hud_media.font_regular, "SCORE", NULL, 0, 0 );
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
		if ( ci->team != team ) {
			continue;
		}
		hud_drawbar( centerx, y, 768, 72, 0.5f, 0.0f, black );
		hud_drawbar( centerx, y, 760, 68, 0.5f, 0.0f, scores->client == cg.clientNum ? myColor : gray2 );
		hud_drawbar( centerx + 124, y + 34, 68, 68, 0.5f, 0.5f, translucent );
		trap_R_SetColor( NULL );
		hud_drawpic( centerx - 346, y + 34, 58, 58, 0.5f, 0.5f, hud_media.face[ci->champion] );
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
	if ( team == TEAM_BLUE ) {
		team = TEAM_RED;
		centerx += 780;
		goto draw_again;
	}
}
