#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

static char hud_fragmessage[256];
static char hud_rankmessage[64];
static int hud_fragtime = -1;

void CG_SetFragMessage( const char *who ) {
	int rank, score;
	char suffix[3];

	score = cg.snap->ps.persistant[PERS_SCORE];
	rank = ( cg.snap->ps.persistant[PERS_RANK] & ( RANK_TIED_FLAG - 1 ) ) + 1;

	Com_sprintf( hud_fragmessage, sizeof( hud_fragmessage ), "YOU FRAGGED %s", who );
	switch ( rank ) {
		case 1:
			Q_strncpyz( suffix, "st", sizeof( suffix ) );
			break;
		case 2:
			Q_strncpyz( suffix, "nd", sizeof( suffix ) );
			break;
		case 3:
			Q_strncpyz( suffix, "rd", sizeof( suffix ) );
			break;
		default:
			Q_strncpyz( suffix, "th", sizeof( suffix ) );
			break;
	};
	//Com_sprintf( hud_rankmessage,  );
	//Q_strncpyz( hud_rankmessage, va( "%d%s place with %d", rank, suffix, score ), sizeof( hud_rankmessage ) );
	Com_sprintf( hud_rankmessage, sizeof( hud_rankmessage ), "%d%s place with %d", rank, suffix, score );
	hud_fragtime = cg.time;
}

/*
===================
hud_drawcenterstring
===================
*/
void hud_drawfragmessage( void ) {
	int		x, y, w;
	float	*color;
	float	shadow[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	if ( hud_fragtime == -1 ) {
		return;
	}

	if ( cg.predictedPlayerState.pm_type == PM_DEAD || cg.showScores ) {
		return;
	}

	color = CG_FadeColor( hud_fragtime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}
	shadow[3] = color[3];
	y = hud_bounds.top + 436;
	w = hud_measurecolorstring( 0.5f, hud_media.font_regular, hud_fragmessage );
	x = ( hud_bounds.right + hud_bounds.left - w ) / 2.0f;

	trap_R_SetColor( color );
	hud_drawcolorstring( x, y, 0.5f, hud_media.font_regular, hud_fragmessage, shadow, 3, 3, qfalse );
	w = hud_measurestring( 0.4f, hud_media.font_regular, hud_rankmessage );
	x = ( hud_bounds.right + hud_bounds.left - w ) / 2.0f;
	color[0] = color[1] = color[2] = 0.75f;

	trap_R_SetColor( color );
	hud_drawstring( x, y + 26, 0.4f, hud_media.font_regular, hud_rankmessage, shadow, 2, 2 );
	trap_R_SetColor( NULL );
}
