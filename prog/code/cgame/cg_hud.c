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

static float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };

qboolean hud_drawfollow( void ) {	
	const char *text;
	float centerx, dim;
	if ( cg.snap->ps.clientNum != cg.clientNum && !cg.showScores && ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		centerx = ( hud_bounds.left + hud_bounds.right ) / 2;
		text = va( "Following ^7%s", cgs.clientinfo[cg.snap->ps.clientNum].name );
		dim = hud_measurecolorstring( 0.6f, hud_media.font_regular, text );
		hud_drawcolorstring( centerx - dim / 2, hud_bounds.bottom + cg_followingOffset.value, 0.6f, hud_media.font_regular, text, black, 2, 2, qfalse );
		return qtrue;
	}return qfalse;
}

/*
=================
hud_drawspeedometer
=================
*/

void hud_drawspeedometer( void ) {
	const int icon_width = 48;
	const int icon_height = 24;

	float pivot_x, pivot_y;
	float icon_x, icon_y;
	float text_x, text_y, text_w;
	float crosshairRadius;
	int offset;
	char *text;
	static float color[] = { 0.96f, 0.85f, 0.37f, 1.0f };

	if ( !cg_speedometer.integer ) {
		return;
	}

	text = va( "%d", cg.playerSpeed );
	text_w = hud_measurestring( 0.5f, hud_media.font_regular, text );
	offset = cg_speedometerOffset.integer;

	if ( cg_speedometer.integer == 1 ) { // around the crosshair
		pivot_x = 0.5f * ( hud_bounds.right + hud_bounds.left );
		crosshairRadius = 0.5f * cg_crosshairSize.value * 2.25f; // 2.25 to adjust from 480p to 1080p
		if ( offset < 0 ) {
			if ( -offset < crosshairRadius ) {
				offset = -crosshairRadius;
			}
		} else {
			if ( offset < crosshairRadius ) {
				offset = crosshairRadius;
			}
		}
		pivot_y = 0.5f * ( hud_bounds.bottom + hud_bounds.top ) + offset;
		if ( offset < 0 ) {
			pivot_y -= icon_height + 8;
		}
		icon_x = pivot_x - icon_width * 0.5f;
		icon_y = pivot_y;
		text_x = pivot_x - text_w * 0.5f;
		text_y = pivot_y + icon_height + 8;
		if ( offset >= 0 ) {
			text_y += 24;
		} else {
			icon_y -= icon_height;
		}
	} else { // under the portrait
		pivot_x = hud_bounds.left + 100;
		pivot_y = hud_bounds.bottom - 40;
		icon_x = pivot_x;
		icon_y = pivot_y - icon_height;
		text_x = pivot_x + icon_width + 8;
		text_y = pivot_y;
	}

	trap_R_SetColor( black );
	hud_drawpic( icon_x + 2, icon_y + 2, icon_width, icon_height, 0.0f, 0.0f, hud_media.icon_speedometer ); // icon shadow
	trap_R_SetColor( color );
	hud_drawpic( icon_x, icon_y, icon_width, icon_height, 0.0f, 0.0f, hud_media.icon_speedometer );
	hud_drawstring( text_x, text_y, 0.5f, hud_media.font_regular, text, black, 2, 2 );
}

/*
=================
CG_Draw2DQC
=================
*/

void CG_DrawVote( void ); // cg_draw.c
void CG_DrawCenterString( void ); // cg_draw.c
void CG_DrawWarmup( void ); // cg_draw.c

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

	hud_drawspeedometer();

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
	;
	if ( !hud_drawfollow() && !cg.showScores ) {
		hud_drawwarmup();
	}
	if ( !cg.showScores ) {
		hud_drawcenterstring();
	}

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
