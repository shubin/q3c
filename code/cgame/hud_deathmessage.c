#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

/*
===================
hud_drawdeathmessage
===================
*/
void hud_drawdeathmessage( void ) {
	float dim, dim1, dim2, centerx, top;
	char *text;
	static float bg[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
	static float bgline[4] = { 0.76f, 0.24f, 0.26f, 0.5f };
	static float gray[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
	static float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	int i;

	if ( cg.killerInfo.clientNum < 0 || 
		cg.killerInfo.clientNum >= MAX_CLIENTS || 
		cg.predictedPlayerState.pm_type != PM_DEAD || 
		cg.showScores ||
		cg.killerInfo.clientNum == cg.clientNum
		) 
	{
		return;
	}

	// measure the text
	centerx = ( hud_bounds.left + hud_bounds.right ) * 0.5f;
	top = hud_bounds.top + 250;

	text = va( "FRAGGED BY ^1%s", cgs.clientinfo[cg.killerInfo.clientNum].name );
	dim = hud_measurecolorstring( 0.5f, hud_media.font_regular, text );
	dim += 80; // weapon icon spacing
	// draw the background
	hud_drawbar( centerx - dim/2 - 12, top, dim + 24, 56, 0.0f, 0.0f, bg );
	hud_drawbar( centerx - dim/2 - 12, top - 3, dim + 24, 3, 0.0f, 0.0f, bgline );
	hud_drawbar( centerx - dim/2 - 12, top + 56, dim + 24, 3, 0.0f, 0.0f, bgline );
	trap_R_SetColor( bg );
	hud_drawgradient( centerx - dim/2 - 150, top, 138, 56, 1 );
	hud_drawgradient( centerx + dim/2 + 12, top, 138, 56, 0 );
	trap_R_SetColor( bgline );
	hud_drawgradient( centerx - dim/2 - 150, top - 3, 138, 3, 1 );
	hud_drawgradient( centerx - dim/2 - 150, top + 56, 138, 3, 1 );
	hud_drawgradient( centerx + dim/2 + 12, top - 3, 138, 3, 0 );
	hud_drawgradient( centerx + dim/2 + 12, top + 56, 138, 3, 0 );
	black[3] = 0.6f;
	trap_R_SetColor( black );
	hud_drawpic_ex( centerx - 384, top + 56 + 3, 768, 256, 0, 0.5f, 1.0f, 1.0f, hud_media.radgrad );
	hud_drawpic_ex( centerx - 384, top - 3 - 384, 768, 384, 0, 0, 1.0f, 0.5f, hud_media.radgrad );
	// draw the killer face
	trap_R_SetColor( NULL );
	if ( cg.killerInfo.clientNum >= 0 && cg.killerInfo.clientNum < cgs.maxclients ) {
		hud_drawpic( centerx, top - 3, 128, 128, 0.5f, 1.0f, hud_media.face[cgs.clientinfo[cg.killerInfo.clientNum].champion] );
	}
	// draw the text
	trap_R_SetColor( NULL );
	hud_drawcolorstring( centerx - dim/2, top + 40, 0.5f, hud_media.font_regular, text, NULL, 0, 0, qfalse );
	// draw means of death icon (weapon icon, ability icon, etc)
	trap_R_SetColor( hud_weapon_colors[cg.killerInfo.weapon] ) ;
	hud_drawpic( centerx + dim/2 - 80 + 12, top + 30, 80, 64,  0.0f, 0.5f, hud_media.icon_weapon[cg.killerInfo.weapon] );
	trap_R_SetColor( NULL );
	if ( cg.killerInfo.health != -9999 ) {
		// draw stack left
		text = va( "STACK LEFT", cg.killerInfo.health, cg.killerInfo.armor );
		dim = hud_measurestring( 0.3f, hud_media.font_regular, text );
		trap_R_SetColor( gray );
		hud_drawstring( centerx - dim/2, top + 90, 0.3f, hud_media.font_regular, text, NULL, 0, 0 );
		trap_R_SetColor( NULL );
		dim1 = hud_measurestring( 0.4f, hud_media.font_regular, va( "%d", cg.killerInfo.health ) );
		dim2 = hud_measurestring( 0.4f, hud_media.font_regular, va( "%d", cg.killerInfo.armor ) );
		dim = dim1 + dim2 + 2 * 24 + 2 * 4 + 44;
		dim = centerx - dim/2;
		hud_drawpic( dim, top + 124, 24, 24, 0.0f, 0.5f, hud_media.icon_health );
		dim += 24 + 4;
		dim += hud_drawstring( dim, top + 133, 0.4f, hud_media.font_regular, va( "%d", cg.killerInfo.health ), NULL, 0, 0 );
		dim += 44;
		hud_drawpic( dim, top + 124, 24, 24, 0.0f, 0.5f, hud_media.icon_armor );
		dim += 24 + 4;
		hud_drawstring( dim, top + 133, 0.4f, hud_media.font_regular, va( "%d", cg.killerInfo.armor ), NULL, 0, 0 );
	}
}
