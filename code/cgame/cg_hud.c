/*
===========================================================================
Copyright (C) 2022 Sergei Shubin

This file is part of Quake III Champions source code.

Quake III Champions source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Champions source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Champions source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

//
// cg_hud.c - QC-like HUD
//

#include "cg_local.h"
#include "../hudlib/hudlib.h"
#include "hud_local.h"

void CG_InitQCHUD( void ) {
	hud_init();
	hud_initmedia();
	hud_initpickups();
	hud_initobituary();
}

void hud_drawfollow( void ) {
	float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const char *text;
	float centerx, dim;
	if ( cg.snap->ps.clientNum != cg.clientNum && !cg.showScores ) {
		centerx = ( hud_bounds.left + hud_bounds.right ) / 2;
		text = va( "Following ^7%s", cgs.clientinfo[cg.snap->ps.clientNum].name );
		dim = hud_measurecolorstring( 0.6f, hud_media.font_regular, text );
		hud_drawcolorstring( centerx - dim / 2, hud_bounds.bottom - 48, 0.6f, hud_media.font_regular, text, black, 2, 2, qfalse );
	}
}

/*
=================
CG_Draw2DQC
=================
*/

void CG_DrawVote( void ); // cg_draw.c

void CG_Draw2DQC( stereoFrame_t stereoFrame ) {
	//CG_DrawPic( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, cgs.media.armorIcon );
	//qch_drawpic( hud_bounds.left + 100, hud_bounds.top + 100, 100, 100, cgs.media.armorIcon );

	trap_R_SetColor( NULL );

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
#if 0x0
		CG_DrawSpectator();

		if(stereoFrame == STEREO_CENTER)
			CG_DrawCrosshair();
		CG_DrawCrosshairNames();
#endif
		hud_drawscores_brief_tournament();
		hud_drawscores_brief_ffa();
		hud_drawscores_brief_tdm();
		hud_drawscores_tournament();
		hud_drawscores_ffa();
		hud_drawscores_tdm();
		return;
	}

	CG_DrawLagometer();

	hud_draw_ability();
	hud_drawstatus();
	hud_drawammo();
	hud_drawfragmessage();
	hud_drawpickups();
	hud_drawobituary();
	hud_drawdeathmessage();
	hud_drawscores_brief_tournament();
	hud_drawscores_brief_ffa();
	hud_drawscores_brief_tdm();
	CG_DrawVote();
	hud_drawscores_tournament();
	hud_drawscores_ffa();
	hud_drawscores_tdm();
	if ( stereoFrame == STEREO_CENTER ) {
		hud_drawcrosshair();
	}
	hud_drawfollow();

	if ( cgs.gametype >= GT_TEAM ) {
#if 0x0
		CG_DrawTeamInfo();
#endif
	}

#if 0x0
	CG_DrawVote();
	CG_DrawTeamVote();
	CG_DrawLagometer();
#endif


#if 0x0
	if ( !CG_DrawFollow() ) {
		CG_DrawWarmup();
	}
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if ( !cg.scoreBoardShowing) {
		CG_DrawCenterString();
	}
#endif
}


void hud_drawdamageplum( int x, int y, float pulse, int value ) {
	const char *text;
	float dim;
	// untranslate
	x = ( x + hud_bounds.xoffs ) / hud_bounds.xscale;
	y = ( y + hud_bounds.yoffs ) / hud_bounds.yscale;
	//measure
	text = va( "%d", value );
	dim = hud_measurestring( 0.5f * pulse, hud_media.font_regular, text );
	x -= dim/2;
	y -= hud_media.font_regular->hdr.line_height;
	hud_drawstring( x, y, 0.5f * pulse, hud_media.font_regular, text, NULL, 0, 0 );
}
