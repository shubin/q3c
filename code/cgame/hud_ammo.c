#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

// list of enabled weapons, grenade launcher can also be included
static int hud_weapons[] = { 
		WP_MACHINEGUN,
		WP_SHOTGUN,
		WP_PLASMAGUN,
		WP_TRIBOLT,
		WP_ROCKET_LAUNCHER,
#if ENABLE_GRENADEL
		WP_GRENADE_LAUNCHER,
#endif
		WP_LIGHTNING,
		WP_RAILGUN,		
};

#define NUM_HUD_WEAPONS (ARRAY_LEN(hud_weapons))

// draws ammo information, big one in the bottom right corner displays the current weapon ammo, and there's also vertical bar with all the weapons
void hud_drawammo( void ) {
	playerState_t *ps;
	centity_t *cent;
	qhandle_t	icon;
	int weapon, value, i, current_weapon;
	qboolean has_weapon;
	char *ammo;
	float ammow, y, fontsize, percentage;
	float color[] = { 1.0f, 0.0f, 1.0f, 1.0f };

	ps = &cg.snap->ps;
	cent = &cg_entities[cg.snap->ps.clientNum];

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return;
	}

	// currently selected weapon
	weapon = cent->currentState.weapon;
	if ( weapon ) {
		icon = cg_weapons[weapon].weaponIcon;
		if ( weapon != WP_GAUNTLET ) {			
			value = ps->ammo[cent->currentState.weapon];
			if ( value < 0 ) {
				value = 0;
			}
			ammo = va( "%d", value );
			ammow = hud_measurestring( 1.15f, hud_media.font_qcde, ammo );
			hud_drawstring( hud_bounds.right - 185 - ammow, hud_bounds.bottom - 95, 1.15f, hud_media.font_qcde, va( "%d", value ), NULL, 0, 0 );
		}
		hud_drawpic( hud_bounds.right - 135, hud_bounds.bottom - 128, 64, 64, 0.5f, 0.5f, icon );
	}

	// vertical bar of all weapons

	y = 180.0f + NUM_HUD_WEAPONS * 70;

	current_weapon = cent->currentState.weapon;
	if ( current_weapon == WP_LOUSY_MACHINEGUN ) {
		current_weapon = WP_MACHINEGUN;
	}
	if ( current_weapon == WP_LOUSY_PLASMAGUN ) {
		current_weapon = WP_PLASMAGUN;
	}
	if ( current_weapon == WP_LOUSY_SHOTGUN ) {
		current_weapon = WP_SHOTGUN;
	}

	for ( i = 0; i < NUM_HUD_WEAPONS; i++ ) {
		weapon = hud_weapons[i];
		has_weapon = ( ps->stats[STAT_WEAPONS] & ( 1 << weapon ) ) != 0;
		if ( weapon == WP_SHOTGUN && ( ps->stats[STAT_WEAPONS] & ( 1 << WP_LOUSY_SHOTGUN ) ) != 0 ) {
			has_weapon = qtrue;
		} else
		if ( weapon == WP_PLASMAGUN && ( ps->stats[STAT_WEAPONS] & ( 1 << WP_LOUSY_PLASMAGUN ) ) != 0 ) {
			has_weapon = qtrue;
		} else
		if ( weapon == WP_MACHINEGUN && ( ps->stats[STAT_WEAPONS] & ( 1 << WP_LOUSY_MACHINEGUN ) ) != 0 ) {
			has_weapon = qtrue;
		}
		if ( has_weapon ) {
			color[0] = hud_weapon_colors[weapon][0];
			color[1] = hud_weapon_colors[weapon][1];
			color[2] = hud_weapon_colors[weapon][2];
		} else {
			color[0] = color[1] = color[2] = 1.0f;
		}

		icon = hud_media.icon_weapon[weapon]; //cg_weapons[weapon].weaponIcon;
		value = ps->ammo[weapon];
		color[3] = has_weapon ? ( weapon == current_weapon ? 1.0f : 0.3f ) : 0.15f;
		trap_R_SetColor( color );
		hud_drawpic( hud_bounds.right - 145, hud_bounds.bottom - y, 96, 96, 0.5f, 0.5f, icon );
		if ( weapon == current_weapon ) {
			hud_drawpic( hud_bounds.right - 126, hud_bounds.bottom - y - 48, 96, 96, 0.0f, 0.0f, hud_media.ammobar_background );
		}
		color[0] = color[1] = color[2] = 0.7f;
		trap_R_SetColor( color );
		if ( has_weapon ) {
			hud_drawpic( hud_bounds.right - 126, hud_bounds.bottom - y - 48, 96, 96, 0.0f, 0.0f, hud_media.ammobar_empty );
			percentage = value / (float)bg_maxAmmo[weapon];
			if ( percentage < 0.0f ) {
				percentage = 0.0f;
			}
			if ( percentage > 1.0f ) {
				percentage = 1.0f;
			}
			percentage *= 75.0f/128;
			percentage += 28.0f/128;
			hud_drawpic_ex( hud_bounds.right - 126, hud_bounds.bottom - y + 48 - 96 * percentage, 96, 96 * percentage, 
				0.0f, percentage, 1.0f, 0.0f,
				hud_media.ammobar_full
			);
		}
		if ( has_weapon ) {
			ammo = va( "%d", value );
			fontsize = ( weapon == current_weapon ? 0.4f : 0.32f );
			ammow = hud_measurestring( fontsize, hud_media.font_regular, ammo );
			hud_drawstring( hud_bounds.right - 78 - ammow, hud_bounds.bottom - y + ( weapon == current_weapon ? 40 : 36 ), fontsize, hud_media.font_regular, ammo, NULL, 0, 0 );
		}
		y -= 70;
	}
	trap_R_SetColor( NULL );
}
