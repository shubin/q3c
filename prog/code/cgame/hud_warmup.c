#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

void hud_drawcenterstring( void ) {
	char *start;
	int		l;
	int		x, y, w;
	float *color;
	float fontscale = 0.05f, lh;

	if ( !cg.centerPrintTime ) {
		return;
	}

	color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}

	trap_R_SetColor( color );

	start = cg.centerPrint;

	lh = hud_media.font_regular->hdr.line_height * 0.005 * cg.centerPrintCharWidth;
	//y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;
	//hud_translate_point( &x, &y );
	y = 350;

	while ( 1 ) {
		char linebuffer[1024];
		
		for ( l = 0; l < 50; l++ ) {
			if ( !start[l] || start[l] == '\n' ) {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = hud_measurecolorstring( fontscale * cg.centerPrintCharWidth, hud_media.font_regular, linebuffer );
		//w = cg.centerPrintCharWidth * CG_DrawStrlen( linebuffer );
		x = 0.5f * ( hud_bounds.left + hud_bounds.right - w );

		//CG_DrawStringExt( x, y, linebuffer, color, qfalse, qtrue,
		//	cg.centerPrintCharWidth, ( int )( cg.centerPrintCharWidth * 1.5 ), 0 );
		hud_drawcolorstring( x, y, fontscale * cg.centerPrintCharWidth, hud_media.font_regular, linebuffer, colorBlack, 4, 4, qfalse );

		y += lh * 1.5;

		while ( *start && ( *start != '\n' ) ) {
			start++;
		}
		if ( !*start ) {
			break;
		}
		start++;
	}

	trap_R_SetColor( NULL );
}

/*
=================
hud_drawwarmup
=================
*/
void hud_drawwarmup( void ) {
	int			w;
	int			sec;
	int			i;
	int			cw;
	clientInfo_t *ci1, *ci2;
	const char *s;
	int			numready, y;
	float		fscale, fx, fy, lh;
	float		cx;

	numready = 0;

	sec = cg.warmup;
	if ( !sec ) {
		return;
	}

	fscale = 0.05f;
	lh = hud_media.font_regular->hdr.line_height * fscale * BIGCHAR_WIDTH;
	cx = 0.5f * ( hud_bounds.left + hud_bounds.right );
	fy = 170;

	if ( sec < 0 ) {
		s = "Waiting for players";
		//w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		w = hud_measurecolorstring( fscale * BIGCHAR_WIDTH, hud_media.font_regular, s );
		fx = cx - 0.5f * w;
		//CG_DrawBigString( 320 - w / 2, 48, s, 1.0F );
		hud_drawcolorstring( fx, fy, fscale * BIGCHAR_HEIGHT, hud_media.font_regular, s, colorBlack, 3, 3, qfalse );

		cg.warmupCount = 0;
		return;
	}

	if ( cgs.gametype == GT_TOURNAMENT ) {
		// find the two active players
		ci1 = NULL;
		ci2 = NULL;
		for ( i = 0; i < cgs.maxclients; i++ ) {
			if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE ) {
				if ( cgs.clientinfo[i].ready ) {
					numready++;
				}
				if ( !ci1 ) {
					ci1 = &cgs.clientinfo[i];
				} else {
					ci2 = &cgs.clientinfo[i];
				}
			}
		}

		if ( ci1 && ci2 ) {
			s = va( "%s^7 vs %s", ci1->name, ci2->name );
			//w = CG_DrawStrlen( s );
			w = hud_measurecolorstring( fscale * BIGCHAR_WIDTH, hud_media.font_regular, s );
			//if ( w > 640 / GIANT_WIDTH ) {
			//	cw = 640 / w;
			//} else {
			//	cw = GIANT_WIDTH;
			//}
			//CG_DrawStringExt( 320 - w * cw / 2, 44, s, colorWhite,
			//	qfalse, qtrue, cw, ( int )( cw * 1.5f ), 0 );
			hud_drawcolorstring( ( hud_bounds.left + hud_bounds.right - w ) * 0.5f, fy, fscale * BIGCHAR_WIDTH, hud_media.font_regular, s, colorBlack, 3, 3, qfalse );
		}
	} else {
		//if ( cgs.gametype == GT_FFA ) {
		//	s = "Free For All";
		//} else if ( cgs.gametype == GT_TEAM ) {
		//	s = "Team Deathmatch";
		//} else if ( cgs.gametype == GT_CTF ) {
		//	s = "Capture the Flag";
		//} else {
		//	s = "";
		//}
		//w = CG_DrawStrlen( s );
		//if ( w > 640 / GIANT_WIDTH ) {
		//	cw = 640 / w;
		//} else {
		//	cw = GIANT_WIDTH;
		//}
		//CG_DrawStringExt( 320 - w * cw / 2, 49, s, colorWhite,
		//		qfalse, qtrue, cw, ( int )( cw * 1.1f ), 0 );
	}

	sec = ( sec - cg.time ) / 1000;
	if ( sec < 0 ) {
		cg.warmup = 0;
		sec = 0;
	}
	s = va( "Starts in: %i", sec + 1 );
	if ( sec != cg.warmupCount ) {
		cg.warmupCount = sec;
		switch ( sec ) {
		case 0:
			trap_S_StartLocalSound( cgs.media.count1Sound, CHAN_ANNOUNCER );
			break;
		case 1:
			trap_S_StartLocalSound( cgs.media.count2Sound, CHAN_ANNOUNCER );
			break;
		case 2:
			trap_S_StartLocalSound( cgs.media.count3Sound, CHAN_ANNOUNCER );
			break;
		default:
			break;
		}
	}

	//switch ( cg.warmupCount ) {
	//case 0:
	//	cw = 28;
	//	break;
	//case 1:
	//	cw = 24;
	//	break;
	//case 2:
	//	cw = 20;
	//	break;
	//default:
	//	cw = 16;
	//	break;
	//}

	//y = 94;
	fy = 300;
	if ( numready == 2 ) {
		//w = CG_DrawStrlen( "All players ready" );
		w = hud_measurestring( fscale * BIGCHAR_WIDTH, hud_media.font_regular, "All players ready" );
		hud_drawstring( cx - 0.5f * w, fy, fscale * BIGCHAR_WIDTH, hud_media.font_regular, "All players ready", colorBlack, 3, 3 );
		//CG_DrawStringExt( 320 - w, y, "All players ready", colorWhite,
		//		qfalse, qtrue, cw, cw, 0 );
		//y += 25;
		fy += 75;
	}
	//w = CG_DrawStrlen( s );
	//CG_DrawStringExt( 320 - w * cw / 2, y, s, colorWhite,
	//		qfalse, qtrue, cw, ( int )( cw * 1.5 ), 0 );
	w = hud_measurecolorstring( fscale * BIGCHAR_WIDTH, hud_media.font_regular, s );
	hud_drawcolorstring( cx - 0.5f * w, fy, fscale *BIGCHAR_WIDTH, hud_media.font_regular, s, colorBlack, 3, 3, qfalse );
}
