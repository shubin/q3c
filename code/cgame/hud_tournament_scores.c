#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

static int hud_stat_weapons[] = {
	WP_GAUNTLET,
	WP_LOUSY_MACHINEGUN,
	WP_MACHINEGUN,
	WP_LOUSY_SHOTGUN,
	WP_SHOTGUN,
	WP_LOUSY_PLASMAGUN,
	WP_PLASMAGUN,
	WP_ROCKET_LAUNCHER,
#if ENABLE_GRENADEL
	WP_GRENADE_LAUNCHER,
#endif
	WP_LIGHTNING,
	WP_RAILGUN,
	WP_TRIBOLT,
};

#define NUM_STAT_WEAPONS (ARRAY_LEN(hud_stat_weapons))

// brief tournament scores with timer
void hud_drawscores_brief_tournament( void ) {
	playerState_t *ps;
	static float color1[] = { 0.12f, 0.38f, 0.53f, 0.8f };
	static float color2[] = { 0.42f, 0.07f, 0.07f, 0.8f };
	static float black[] = { 0.0f, 0.0f, 0.0f, 0.8f };
	static float gray[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	const char *text;
	float centerx, dim;
	int enemyNum, enemyScore, myScore;
//	clientInfo_t *enemyInfo;

	if ( cgs.gametype != GT_TOURNAMENT ) {
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
	if ( cgs.leader1 == cg.clientNum ) {
		enemyNum = cgs.leader2;
		enemyScore = cgs.scores2;
		myScore = cgs.scores1;
	} else {
		enemyNum = cgs.leader1;
		enemyScore = cgs.scores1;
		myScore = cgs.scores2;
	}

	// big red bars with scores
	hud_drawbar( centerx - 209, hud_bounds.top + 23, 129, 69, 0.0f, 0.0f, color1 );
	hud_drawbar( centerx + 80,  hud_bounds.top + 23, 129, 69, 0.0f, 0.0f, color2 );
	// small black bars for nicknames
	hud_drawbar( centerx - 301, hud_bounds.top + 92, 221, 21, 0.0f, 0.0f, black );
	hud_drawbar( centerx + 80,  hud_bounds.top + 92, 221, 21, 0.0f, 0.0f, black );

	black[3] = 1.0f;
	// left player icon backgound
	hud_drawbar( centerx - 250, hud_bounds.top + 55, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx - 250, hud_bounds.top + 55, 58, 58, 0.5f, 0.5f, black );

	// right player icon background
	hud_drawbar( centerx + 250, hud_bounds.top + 55, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx + 250, hud_bounds.top + 55, 58, 58, 0.5f, 0.5f, black );

	// player icons
	hud_drawpic( centerx - 250, hud_bounds.top + 55, 58, 58, 0.5f, 0.5f, hud_media.face[cg.predictedPlayerState.champion] );
	if ( enemyScore != SCORE_NOT_PRESENT ) {
		hud_drawpic( centerx + 250, hud_bounds.top + 55, 58, 58, 0.5f, 0.5f, hud_media.face[cgs.clientinfo[enemyNum].champion] );
	}

	// player score
	text = va( "%d", myScore );
	dim = hud_measurestring( 1.0f, hud_media.font_qcde, text );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 143 - dim/2, hud_bounds.top + 84, 1.0f, hud_media.font_qcde, text, NULL, 0, 0 );

	if ( enemyScore != SCORE_NOT_PRESENT ) {
		// enemy score
		text = va( "%d", enemyScore );
		dim = hud_measurestring( 1.0f, hud_media.font_qcde, text );
		trap_R_SetColor( NULL );
		hud_drawstring( centerx + 143 - dim/2, hud_bounds.top + 84, 1.0f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	// player name
	dim = hud_measurecolorstring( 0.3f, hud_media.font_regular, cgs.clientinfo[cg.clientNum].name );
	trap_R_SetColor( gray );
	hud_drawcolorstring(centerx - 88 - dim, hud_bounds.top + 108, 0.3f, hud_media.font_regular, cgs.clientinfo[cg.clientNum].name, NULL, 0, 0, qtrue );

	if ( enemyScore != SCORE_NOT_PRESENT ) {
		// enemy name
		trap_R_SetColor( gray );
		hud_drawcolorstring(centerx + 88, hud_bounds.top + 108, 0.3f, hud_media.font_regular, cgs.clientinfo[enemyNum].name, NULL, 0, 0, qtrue );
	}
}

// tournament scores with weapon usage stats, item stats etc.
void hud_drawscores_tournament( void ) {
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
	int i, enemyNum, enemyScore, myScore;
	int hits, shots, acc, score, damage;
	score_t *myScores, *enemyScores;
	int totalHits = 0, totalShots = 0, totalDamage = 0;
	int enemyTotalHits = 0, enemyTotalShots = 0, enemyTotalDamage = 0;

	if ( !cg.showScores || cgs.gametype != GT_TOURNAMENT ) {
		return;
	}

	if ( cgs.leader1 == cg.clientNum ) {
		enemyNum = cgs.leader2;
		enemyScore = cgs.scores2;
		myScore = cgs.scores1;
	} else {
		enemyNum = cgs.leader1;
		enemyScore = cgs.scores1;
		myScore = cgs.scores2;
	}

	myScores = enemyScores = NULL;
	for ( i = 0; i < cg.numScores; i++ ) {
		if ( cg.scores[i].client == cg.clientNum ) {
			myScores = &cg.scores[i];
		}
		if ( cg.scores[i].client == enemyNum ) {
			enemyScores = &cg.scores[i];
		}
	}

	centerx = ( hud_bounds.right + hud_bounds.left ) / 2;

	hud_drawbar( centerx, hud_bounds.top + 90, 1880, 110, 0.5f, 0.5f, header );
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 800, hud_bounds.top + 88, 0.75f, hud_media.font_regular, "TOURNAMENT", NULL, 0, 0 );
	trap_R_SetColor( mapnameColor );
	hud_drawstring( centerx - 800, hud_bounds.top + 122, 0.5f, hud_media.font_regular, cgs.mapname, NULL, 0, 0 );
	trap_R_SetColor( NULL );
	hud_drawpic( centerx - 874, hud_bounds.top + 90, 80, 80, 0.5f, 0.5f, trap_R_RegisterShader( "menu/art/skill5" ) );

	hud_drawbar( centerx - 160, hud_bounds.top + 90, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx + 160, hud_bounds.top + 90, 62, 62, 0.5f, 0.5f, gray );
	hud_drawbar( centerx - 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, black );
	hud_drawbar( centerx + 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, black );
	hud_drawpic( centerx - 160, hud_bounds.top + 90, 58, 58, 0.5f, 0.5f, hud_media.face[cg.predictedPlayerState.champion] );
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

	text = va( "%d", myScore );
	dim = hud_measurestring( 0.7f, hud_media.font_qcde, text );
	hud_drawstring( centerx - 20 - dim, hud_bounds.top + 202, 0.7f, hud_media.font_qcde, text, NULL, 0, 0 );

	if ( enemyScore != SCORE_NOT_PRESENT ) {
		text = va( "%d", enemyScore ) ;
		hud_drawstring( centerx + 20, hud_bounds.top + 202, 0.7f, hud_media.font_qcde, text, NULL, 0, 0 );
	}

	dim = hud_measurecolorstring( 0.5f, hud_media.font_regular, cgs.clientinfo[cg.clientNum].name );
	trap_R_SetColor( nameColor1 );
	hud_drawcolorstring( centerx - 130 - dim, hud_bounds.top + 194, 0.5f, hud_media.font_regular, cgs.clientinfo[cg.clientNum].name, NULL, 0, 0, qtrue );
	if ( enemyScore != SCORE_NOT_PRESENT ) {
		trap_R_SetColor( nameColor2 );
		hud_drawcolorstring( centerx + 130, hud_bounds.top + 194, 0.5f, hud_media.font_regular, cgs.clientinfo[enemyNum].name, NULL, 0, 0, qtrue );
	}

	// table header
	hud_drawbar( centerx, hud_bounds.top + 240, 1870, 45, 0.5f, 0.0f, black );
	trap_R_SetColor( gray );

	dim = hud_measurestring( 0.4f, hud_media.font_regular, "PING" );
	hud_drawstring( centerx - 500 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "PING", NULL, 0, 0 );
	hud_drawstring( centerx + 500 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "PING", NULL, 0, 0 );
	dim = hud_measurestring( 0.4f, hud_media.font_regular, "HITS" );
	hud_drawstring( centerx - 400 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "HITS", NULL, 0, 0 );
	hud_drawstring( centerx + 400 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "HITS", NULL, 0, 0 );
	dim = hud_measurestring( 0.4f, hud_media.font_regular, "ACC %" );
	hud_drawstring( centerx - 300 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "ACC %", NULL, 0, 0 );
	hud_drawstring( centerx + 300 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "ACC %", NULL, 0, 0 );
	dim = hud_measurestring( 0.4f, hud_media.font_regular, "DMG" );
	hud_drawstring( centerx - 200 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "DMG", NULL, 0, 0 );
	hud_drawstring( centerx + 200 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "DMG", NULL, 0, 0 );
	dim = hud_measurestring( 0.4f, hud_media.font_regular, "SCORE" );
	hud_drawstring( centerx - 100 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "SCORE", NULL, 0, 0 );
	hud_drawstring( centerx + 100 - dim/2, hud_bounds.top + 272, 0.4f, hud_media.font_regular, "SCORE", NULL, 0, 0 );

	for ( i = 0; i < ARRAY_LEN( hud_stat_weapons ); i++ ) { 
		hud_drawbar( centerx, hud_bounds.top + 295 + i * 60, 1870, 60, 0.5f, 0.0f, black );
		//
		trap_R_SetColor( hud_weapon_colors[hud_stat_weapons[i]] );
		hud_drawpic( centerx, hud_bounds.top + 295 + i * 60 + 30, 60, 60, 0.5f, 0.5f, hud_media.icon_weapon[hud_stat_weapons[i]] );
	}

	trap_R_SetColor( NULL );
	if ( myScores != NULL ) {
		text = va( "%d", myScores->ping );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx - 500 - dim/2, hud_bounds.top + 295 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
	}
	if ( enemyScores != NULL ) {
		text = va( "%d", enemyScores->ping );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 500 - dim/2, hud_bounds.top + 295 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
	}
	for ( i = 0; i < ARRAY_LEN( hud_stat_weapons ); i++ ) { 
		if ( myScores != NULL ) {
			hits = myScores->wepstat[hud_stat_weapons[i]].hits;
			shots = myScores->wepstat[hud_stat_weapons[i]].shots;
			acc = shots > 0 ? 100 * hits / shots : 0;
			score = myScores->wepstat[hud_stat_weapons[i]].score;
			damage = myScores->wepstat[hud_stat_weapons[i]].damage;

			totalHits += hits;
			totalShots += shots;
			totalDamage += damage;

			if ( hud_stat_weapons[i] != WP_GAUNTLET ) {
				text = va( "%d/%d", hits, shots );
				dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
				hud_drawstring( centerx - 400 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

				text = va( "%d%c", acc, '%' );
				dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
				hud_drawstring( centerx - 300 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
			}

			text = va( "%d", damage );
			dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
			hud_drawstring( centerx - 200 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

			text = va( "%d", score );
			dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
			hud_drawstring( centerx - 100 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
		}
		if ( enemyScores != NULL ) {
			hits = enemyScores->wepstat[hud_stat_weapons[i]].hits;
			shots = enemyScores->wepstat[hud_stat_weapons[i]].shots;
			acc = shots > 0 ? 100 * hits / shots : 0;
			score = enemyScores->wepstat[hud_stat_weapons[i]].score;
			damage = enemyScores->wepstat[hud_stat_weapons[i]].damage;

			enemyTotalHits += hits;
			enemyTotalShots += shots;
			enemyTotalDamage += damage;

			if ( hud_stat_weapons[i] != WP_GAUNTLET ) {
				text = va( "%d/%d", hits, shots );
				dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
				hud_drawstring( centerx + 400 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

				text = va( "%d%c", acc, '%' );
				dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
				hud_drawstring( centerx + 300 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
			}

			text = va( "%d", damage );
			dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
			hud_drawstring( centerx + 200 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

			text = va( "%d", score );
			dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
			hud_drawstring( centerx + 100 - dim/2, hud_bounds.top + 295 + i * 60 + 40, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
		}
	}
	// table footer
	y = hud_bounds.top + 295 + ARRAY_LEN(hud_stat_weapons) * 60 + 10;
	hud_drawbar( centerx, y, 1870, 45, 0.5f, 0.0f, black );
	trap_R_SetColor( gray );
	dim = hud_measurestring( 0.4f, hud_media.font_regular, "TOTAL" );
	hud_drawstring( centerx - dim/2, y + 32, 0.4f, hud_media.font_regular, "TOTAL", NULL, 0, 0 );

	if ( myScores != NULL ) {
		acc = totalShots ? 100 * totalHits / totalShots : 0;

		text = va("%d/%d", totalHits, totalShots );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx - 400 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d%c", acc, '%' );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx - 300 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d", totalDamage );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx - 200 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d", myScore );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx - 100 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
	}
	if ( enemyScores != NULL ) {
		acc = enemyTotalShots ? 100 * enemyTotalHits / enemyTotalShots : 0;

		text = va("%d/%d", enemyTotalHits, enemyTotalShots );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 400 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d%c", acc, '%' );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 300 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d", totalDamage );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 200 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );

		text = va( "%d", enemyScore );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 100 - dim/2, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0 );
	}
	trap_R_SetColor( NULL );
	// item stats
	if ( myScores != NULL) { 
		y = hud_bounds.top + 295 + 3 * 60 + 10;
		hud_drawpic( centerx - 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_health );
		hud_drawstring( centerx - 800 + 32, y + 32, 0.4f, hud_media.font_regular, va( "%d", myScores->itemstat.mega ), NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx - 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_armor );
		hud_drawstring( centerx - 800 + 32, y + 32, 0.4f, hud_media.font_regular, va( "%d", myScores->itemstat.red ), NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx - 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_yellow_armor );
		hud_drawstring( centerx - 800 + 32, y + 32, 0.4f, hud_media.font_regular, va( "%d", myScores->itemstat.yellow ), NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx - 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_hourglass );
		hud_drawstring( centerx - 800 + 32, y + 32, 0.4f, hud_media.font_regular, va( "%d", myScores->itemstat.hourglass ), NULL, 0, 0);
	}
	if ( enemyScores != NULL) { 
		y = hud_bounds.top + 295 + 3 * 60 + 10;
		hud_drawpic( centerx + 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_health );
		text = va( "%d", enemyScores->itemstat.mega );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 800 - 32 - dim, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx + 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_armor );
		text = va( "%d", enemyScores->itemstat.red );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 800 - 32 - dim, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx + 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_yellow_armor );
		text = va( "%d", enemyScores->itemstat.yellow );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 800 - 32 - dim, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0);
		y += 60;
		hud_drawpic( centerx + 800, y + 6, 32, 32, 0.5f, 0.0f, hud_media.icon_hourglass );
		text = va( "%d", enemyScores->itemstat.hourglass );
		dim = hud_measurestring( 0.4f, hud_media.font_regular, text );
		hud_drawstring( centerx + 800 - 32 - dim, y + 32, 0.4f, hud_media.font_regular, text, NULL, 0, 0);
	}
}
