#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

void hud_drawtimer( float x, float y ) {
	static float shadow[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	int	mins, seconds, tens, msec;
	float dim;

	msec = cg.time - cgs.levelStartTime;
	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	dim = hud_measurestring( 0.65f, hud_media.font_qcde, va( "%d", mins ) );	
	trap_R_SetColor( NULL );
	hud_drawstring( x - dim, y, 0.65f, hud_media.font_qcde, va( "%d", mins ), shadow, 1, 1 );
	hud_drawstring( x, y, 0.65f, hud_media.font_qcde, va( ":%d%d", tens, seconds ), shadow, 1, 1 );
}
