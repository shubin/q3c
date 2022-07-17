#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

/*
=================
hud_drawcrosshair
=================
*/
void hud_drawcrosshair( void ) {
	float		w, h;
	qhandle_t	hShader;
	float		f;
	int			ca;

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return;
	}

	if ( !cg_drawCrosshair.integer ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if ( cg.renderingThirdPerson ) {
		return;
	}

	// set color based on health
	if ( cg_crosshairHealth.integer ) {
		vec4_t		hcolor;

		CG_ColorForHealth( hcolor );
		trap_R_SetColor( hcolor );
	} else {
		trap_R_SetColor( NULL );
	}

	w = h = cg_crosshairSize.value * 2.25f;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
		h *= ( 1 + f );
	}

	//x = cg_crosshairX.integer;
	//y = cg_crosshairY.integer;
	//CG_AdjustFrom640( &x, &y, &w, &h );

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ ca % NUM_CROSSHAIRS ];

	hud_drawpic( ( hud_bounds.left + hud_bounds.right ) / 2.0f, ( hud_bounds.top + hud_bounds.bottom ) / 2.0f, w, h, 0.5f, 0.5f, hShader );

	//trap_R_DrawStretchPic( x + cg.refdef.x + 0.5 * (cg.refdef.width - w), 
	//	y + cg.refdef.y + 0.5 * (cg.refdef.height - h), 
	//	w, h, 0, 0, 1, 1, hShader );

	trap_R_SetColor( NULL );
}
