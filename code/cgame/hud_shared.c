#include "../qcommon/q_shared.h"
#include "cg_local.h"
#include "hud_local.h"

vec4_t hud_weapon_colors[WP_NUM_WEAPONS] = {
    { 1.0f, 1.0f, 1.0f, 1.0f },     // WP_NONE,
	{ 0.0f, 0.85f, 1.0f, 1.0f },    // WP_GAUNTLET,
	{ 1.0f, 1.0f, 0.0f, 1.0f },     // WP_MACHINEGUN,
	{ 1.0f, 0.5f, 0.0f, 1.0f },     // WP_SHOTGUN,
	{ 0.0f, 0.5f, 0.0f, 1.0f },     // WP_GRENADE_LAUNCHER,
	{ 1.0f, 0.0f, 0.0f, 1.0f },     // WP_ROCKET_LAUNCHER,
	{ 0.94f, 0.94f, 0.7f, 1.0f },   // WP_LIGHTNING,
	{ 0.0f, 1.0f, 0.0f, 1.0f },     // WP_RAILGUN,
	{ 0.8f, 0.0f, 0.9f, 1.0f },     // WP_PLASMAGUN,
	{ 1.0f, 1.0f, 1.0f, 1.0f },     // WP_BFG,
	{ 1.0f, 1.0f, 1.0f ,1.0f },     // WP_GRAPPLING_HOOK,
#ifdef MISSIONPACK
	{ 1.0f, 1.0f, 1.0f ,1.0f },     // WP_NAILGUN,
	{ 1.0f, 1.0f, 1.0f ,1.0f },     // WP_PROX_LAUNCHER,
	{ 1.0f, 1.0f, 1.0f ,1.0f },     // WP_CHAINGUN,
#endif
	{ 0.8f, 0.92f, 0.19f, 1.0f },   // WP_TRIBOLT,
	{ 1.0f, 1.0f, 0.0f, 1.0f },     // WP_LOUSY_MACHINEGUN,
	{ 1.0f, 0.5f, 0.0f, 1.0f },     // WP_LOUSY_SHOTGUN,
	{ 0.8f, 0.0f, 0.9f, 1.0f },     // WP_LOUSY_PLASMAGUN,
	// abilities
	{ 1.0f, 1.0f, 1.0f, 1.0f },     // WP_DIRE_ORB,
	//WP_ACID_SPIT,
};

const char *hud_weapon_icons[WP_NUM_WEAPONS] = {
    "", // WP_NONE,
    "hud/weapon/gauntlet",
    "hud/weapon/machinegun",
    "hud/weapon/shotgun",
    "hud/weapon/grenade",
    "hud/weapon/rocket",
    "hud/weapon/lightning",
    "hud/weapon/railgun",
    "hud/weapon/plasmagun",
    "", // WP_BFG,
    "", // WP_GRAPPLING_HOOK,
#ifdef MISSIONPACK
    "", // WP_NAILGUN,
    "", // WP_PROX_LAUNCHER,
    "", // WP_CHAINGUN,
#endif
    "hud/weapon/tribolt",
    "hud/weapon/machinegunl",    // WP_LOUSY_MACHINEGUN,
    "hud/weapon/shotgunl",       // WP_LOUSY_SHOTGUN,
    "hud/weapon/plasmagunl",     // WP_LOUSY_PLASMAGUN
	"hud/mod/direorb",			 // WP_DIRE_ORB
};

const char *hud_mod_icons[ MOD_NUM ] = {
	"hud/mod/generic",		// MOD_UNKNOWN
	"hud/weapon/shotgun",	// MOD_SHOTGUN
	"hud/weapon/gauntlet",  // MOD_GAUNTLET
	"hud/weapon/machinegun",// MOD_MACHINEGUN
	"hud/weapon/grenade",	// MOD_GRENADE
	"hud/weapon/grenade",   // MOD_GRENADE_SPLASH
	"hud/weapon/rocket",	// MOD_ROCKET
	"hud/weapon/rocket",	// MOD_ROCKET_SPLASH
	"hud/weapon/plasmagun", // MOD_PLASMA
	"hud/weapon/plasmagun",	// MOD_PLASMA_SPLASH
	"hud/weapon/railgun",	// MOD_RAILGUN
	"hud/weapon/lightning",	// MOD_LIGHTNING
	"hud/weapon/bfg",		// MOD_BFG
	"hud/weapon/bfg",		// MOD_BFG_SPLASH
	"hud/weapon/tribolt",	// MOD_TRIBOLT
	"hud/weapon/tribolt",	// MOD_TRIBOLT_SPLASH
	"hud/mod/direorb",		// MOD_DIRE_ORB
	"hud/mod/generic",		// MOD_WATER
	"hud/mod/generic",		// MOD_SLIME
	"hud/mod/generic",		// MOD_LAVA
	"hud/mod/generic",		// MOD_CRUSH
	"hud/mod/telefrag",		// MOD_TELEFRAG
	"hud/mod/generic",		// MOD_FALLING
	"hud/mod/generic",		// MOD_SUICIDE
	"hud/mod/generic",		// MOD_TARGET_LASER
	"hud/mod/generic",		// MOD_TRIGGER_HURT
#ifdef MISSIONPACK
	MOD_NAIL,
	MOD_CHAINGUN,
	MOD_PROXIMITY_MINE,
	MOD_KAMIKAZE,
	MOD_JUICED,
#endif
	"hud/mod/generic",		// MOD_GRAPPLE
};

hud_media_t hud_media;

// get all the required assets
void hud_initmedia( void ) {
	int i;

	hud_media.font_qcde = hud_font( "hud/font_qcde" );
	hud_media.font_regular = hud_font( "hud/font_regular" );
	hud_media.icon_health = cg_items[BG_FindItemByClass( "item_health_mega" ) - bg_itemlist].icon;
	hud_media.icon_armor = cg_items[BG_FindItemByClass( "item_armor_body" ) - bg_itemlist].icon;
	hud_media.icon_yellow_armor = cg_items[BG_FindItemByClass( "item_armor_combat" ) - bg_itemlist].icon;
	hud_media.icon_hourglass = cg_items[BG_FindItemByClass( "item_hourglass" ) - bg_itemlist].icon;
	for ( i = 0; i < WP_NUM_WEAPONS; i++ ) {
		hud_media.icon_weapon[i] = trap_R_RegisterShader( hud_weapon_icons[i] );
	}
	for ( i = 0; i < MOD_NUM; i++ ) {
		hud_media.icon_mod[i] = trap_R_RegisterShader( hud_mod_icons[i] );
	}
	hud_media.ammobar_background = trap_R_RegisterShader( "hud/ammobar/background" );
	hud_media.ammobar_full = trap_R_RegisterShader( "hud/ammobar/full" );
	hud_media.ammobar_empty = trap_R_RegisterShader( "hud/ammobar/empty" );
	hud_media.ringgauge = trap_R_RegisterShader( "hud/ring");
	hud_media.ringglow = trap_R_RegisterShader( "hud/ring_glow" );
	hud_media.abbg = trap_R_RegisterShader( "hud/abbg" );
	for ( i = 0; i < NUM_CHAMPIONS; i++ ) {
		hud_media.skillicon[i] = trap_R_RegisterShader( va( "hud/skill/%s", champion_names[i] ) );
		hud_media.face[i] = trap_R_RegisterShader( va( "gfx/champions/faces/%s", champion_names[i] ) );
	}
	hud_media.gradient = trap_R_RegisterShaderNoMip( "hud/gradient" );
	hud_media.radgrad = trap_R_RegisterShaderNoMip( "hud/radgrad" );
	for ( i = 0; i < bg_numItems; i++ ) {
		if ( bg_itemlist[i].icon != NULL && bg_itemlist[i].icon[0] )
		{
			hud_media.itemicons[i] = trap_R_RegisterShader( bg_itemlist[i].icon );
		}
		else {
			hud_media.itemicons[i] = -1;
		}
	}
}

// dir: gradient direction, 0: left to right, 1: right to left, 2: top to bottom, 3: bottom to top
void hud_drawgradient( float x, float y, float w, float h, int dir ) {
	float px[4], py[4], ps[4], pt[4];

	px[0] = x; py[0] = y;
	hud_translate_point( &px[0], &py[0] );
	px[1] = x + w; py[1] = y;
	hud_translate_point( &px[1], &py[1] );
	px[2] = x + w; py[2] = y + h;
	hud_translate_point( &px[2], &py[2] );
	px[3] = x; py[3] = y + h;
	hud_translate_point( &px[3], &py[3] );
	switch ( dir ) {
		case 1:
			ps[0] = 1; pt[0] = 0; ps[1] = 0; pt[1] = 0; ps[2] = 0; pt[2] = 1; ps[3] = 1; pt[3] = 1;
			break;
		case 2:
			ps[0] = 0; pt[0] = 0; ps[1] = 0; pt[1] = 1; ps[2] = 1; pt[2] = 1;  ps[3] = 1; pt[3] = 0;
			break;
		case 3:
			ps[0] = 1; pt[0] = 0; ps[1] = 1; pt[1] = 1; ps[2] = 0; pt[2] = 1;  ps[3] = 0; pt[3] = 0;
			break;
		default:
			ps[0] = 0; pt[0] = 0; ps[1] = 1; pt[1] = 0; ps[2] = 1; pt[2] = 1; ps[3] = 0; pt[3] = 1;
			break;

	}
	trap_R_DrawQuad( 
		px[0], py[0], ps[0], pt[0],
		px[1], py[1], ps[1], pt[1],
		px[2], py[2], ps[2], pt[2],
		px[3], py[3], ps[3], pt[3],
		hud_media.gradient 
	);
}
