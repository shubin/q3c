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
	// small black bars for nicknames
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
//	playerState_t *ps;
	static float color1[] = { 0.12f, 0.38f, 0.53f, 0.8f };
	static float color2[] = { 0.42f, 0.07f, 0.07f, 0.8f };
	static float nameColor1[] = { 0.0f, 0.5f, 1.0f, 1.0f };
	static float nameColor2[] = { 1.0f, 0.22f, 0.22f, 1.0f };
	static float black[] = { 0.0f, 0.0f, 0.0f, 0.8f };
	static float gray[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	static float header[] = { 0.15f, 0.05f, 0.05f, 0.8f };
	static float mapnameColor[] = { 0.79f, 0.5f, 0.33f, 1.0f };
	const char *text;
	float centerx, y, dim;
	int i, enemyNum, enemyScore, myNum, myScore;
	int hits, shots, acc, score, damage;
	score_t *myScores, *enemyScores;
	int totalHits = 0, totalShots = 0, totalDamage = 0;
	int enemyTotalHits = 0, enemyTotalShots = 0, enemyTotalDamage = 0;

	if ( !cg.showScores || ( cgs.gametype != GT_TEAM && cgs.gametype != GT_TEAM2V2  ) ) {
		return;
	}

	if ( cgs.leader1 == cg.clientNum ) {
		myNum = cgs.leader1;
		myScore = cgs.scores1;
		enemyNum = cgs.leader2;
		enemyScore = cgs.scores2;
	} else if ( cgs.leader2 == cg.clientNum ) {
		myNum = cgs.leader2;
		myScore = cgs.scores2;
		enemyNum = cgs.leader1;
		enemyScore = cgs.scores1;
	} else {
		if ( cgs.leader1 < cgs.leader2 ) {
			myNum = cgs.leader1;
			myScore = cgs.scores1;
			enemyNum = cgs.leader2;
			enemyScore = cgs.scores2;
		} else {
			myNum = cgs.leader2;
			myScore = cgs.scores2;
			enemyNum = cgs.leader1;
			enemyScore = cgs.scores1;
		}
	}

	myScores = enemyScores = NULL;
	for ( i = 0; i < cg.numScores; i++ ) {
		if ( cg.scores[i].client == myNum ) {
			myScores = &cg.scores[i];
		}
		if ( cg.scores[i].client == enemyNum ) {
			enemyScores = &cg.scores[i];
		}
	}

	centerx = ( hud_bounds.right + hud_bounds.left ) / 2;

	hud_drawbar( centerx, hud_bounds.top + 90, 1880, 110, 0.5f, 0.5f, header );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 800, hud_bounds.top + 88, 0.75f, hud_media.font_regular, cgs.gametype == GT_TEAM ? "TDM" : "TDM 2v2", NULL, 0, 0);
	trap_R_SetColor( mapnameColor );
	hud_drawstring( centerx - 800, hud_bounds.top + 122, 0.5f, hud_media.font_regular, CG_ConfigString( CS_MESSAGE ), NULL, 0, 0 );
	trap_R_SetColor( NULL );
	hud_drawpic( centerx - 874, hud_bounds.top + 90, 80, 80, 0.5f, 0.5f, trap_R_RegisterShader( "menu/art/skill5" ) );

	hud_drawbar( centerx - 160, hud_bounds.top + 90, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx + 160, hud_bounds.top + 90, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx - 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, black );
	hud_drawbar( centerx + 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, black );
	if ( myScore != SCORE_NOT_PRESENT ) {
		hud_drawpic( centerx - 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, hud_media.face[ cgs.clientinfo[myNum].champion ] );
	}
	if ( enemyScore != SCORE_NOT_PRESENT ) {
		hud_drawpic( centerx + 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, hud_media.face[cgs.clientinfo[enemyNum].champion] );
	}
	
	hud_drawbar( centerx - 5, hud_bounds.top + 152, 935, 60, 1.0f, 0.0f, header );
	trap_R_SetColor( color1 );
	hud_drawpic( centerx - 5, hud_bounds.top + 152, -400, 60, 0.0f, 0.0f, hud_media.gradient );

	hud_drawbar( centerx + 5, hud_bounds.top + 152, 935, 60, 0.0f, 0.0f, header );
	trap_R_SetColor( color2 );
	hud_drawpic( centerx + 5, hud_bounds.top + 152, 400, 60, 0.0f, 0.0f, hud_media.gradient );

	trap_R_SetColor( NULL );

	if ( myScore != SCORE_NOT_PRESENT ) {
		text = va( "%d", myScore );
		dim = hud_measurestring( 0.7f, hud_media.font_qcde, text );
		hud_drawstring( centerx - 20 - dim, hud_bounds.top + 202, 0.7f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	if ( enemyScore != SCORE_NOT_PRESENT ) {
		text = va( "%d", enemyScore ) ;
		hud_drawstring( centerx + 20, hud_bounds.top + 202, 0.7f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	if ( myScore != SCORE_NOT_PRESENT ) {
		dim = hud_measurecolorstring( 0.5f, hud_media.font_regular, cgs.clientinfo[myNum].name );
		trap_R_SetColor( nameColor1 );
		hud_drawcolorstring( centerx - 130 - dim, hud_bounds.top + 194, 0.5f, hud_media.font_regular, cgs.clientinfo[myNum].name, NULL, 0, 0, qtrue );
	}
	if ( enemyScore != SCORE_NOT_PRESENT ) {
		trap_R_SetColor( nameColor2 );
		hud_drawcolorstring( centerx + 130, hud_bounds.top + 194, 0.5f, hud_media.font_regular, cgs.clientinfo[enemyNum].name, NULL, 0, 0, qtrue );
	}
}
