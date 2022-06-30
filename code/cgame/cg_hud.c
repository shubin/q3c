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

#define ENABLE_GRENADEL 0

// virtual dimensions for HUD drawing
#define VIRTUAL_WIDTH 1920
#define VIRTUAL_HEIGHT 1080
#define VIRTUAL_ASPECT (VIRTUAL_WIDTH/(float)(VIRTUAL_HEIGHT))

#define MAX_PICKUPS 8
#define PICKUP_TIMEOUT 1500
#define PICKUP_FADEOUTTIME 1000

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

// weapon sideways icons
static char* hud_weapon_icons[] = {
	"hud/weapon/machinegun",
	"hud/weapon/shotgun",
	"hud/weapon/plasmagun",
	"hud/weapon/tribolt",
	"hud/weapon/rocket",
#if ENABLE_GRENADEL
	"hud/weapon/grenade",
#endif
	"hud/weapon/lightning",
	"hud/weapon/railgun",
};

// weapon colors for icon coloring and other needs
static float hud_weapon_colors[][4] = {
	{ 1.0f, 1.0f, 0.0f, 1.0f },	// machinegun
	{ 1.0f, 0.5f, 0.0f, 1.0f }, // shotgun
	{ 0.8f, 0.0f, 0.9f, 1.0f }, // plasmagun
	{ 1.0f, 1.0f, 0.0f, 1.0f }, // tribolt
	{ 1.0f, 0.0f, 0.0f, 1.0f }, // rocket launcher
#if ENABLE_GRENADEL
	{ 0.0f, 0.5f, 0.0f, 1.0f }, // grenade launcher
#endif
	{ 0.94f, 0.94f, 0.7f, 1.0f }, // lightning
	{ 0.0f, 1.0f, 0.0f, 1.0f }, // railgun
};

#define NUM_HUD_WEAPONS (sizeof( hud_weapons ) / sizeof( hud_weapons[0] ) )

static struct {
	float left, top, right, bottom;			// visible area, use these values to stick to appripriate screen edges
	float xscale, yscale, xoffs, yoffs;		// needed to convert virtual coordinates to the real ones
} bounds;

static struct {
	qhandle_t	icon_health;	// for player status
	qhandle_t	icon_armor;		// for player status
	qhandle_t	icon_weapon[NUM_HUD_WEAPONS];	// weapon icons for the vertical ammo status bar
	qhandle_t	ammobar_background, ammobar_full, ammobar_empty; // various graphic for the vertical ammo status bar
	qhandle_t	ringgauge, ringglow, abbg;
	qhandle_t	skillicon[NUM_CHAMPIONS];
	qhandle_t	gradient, radgrad;
	qhandle_t	itemicons[MAX_ITEMS];
} media;

typedef struct {
	const char *name;
	int line_height;
	int base;
	int scale_w, scale_h;
	int num_chars;
} fonthdr_t;

typedef struct {
	int id;
	int x, y;
	int width, height;
	int xoffset, yoffset;
	int xadvance;
} chardesc_t;

// font description, has to be generated from the XML produced by the BMFont utility
typedef struct {
	fonthdr_t hdr;
	chardesc_t *charmap[65536]; // TODO: optimize some day
	qhandle_t shader;
	chardesc_t chars[];
} font_t;

// I'm gonna handle UTF8 someday
//#include "utf8.inc"

// these *.inc files are generated from the XML produced by the BMFont utility
#include "font_regular.inc"
//#include "font_large.inc"
#include "font_qcde.inc"

static char hud_fragmessage[256];
static char hud_rankmessage[64];
static int hud_fragtime = -1;
//cg.centerPrintTime = cg.time;

typedef struct {
	int		itemNum;
	int		time;
} pickup_t;

static pickup_t hud_pickups[MAX_PICKUPS];
static int hud_numpickups;

void hud_initbounds( void );
void hud_initmedia( void );
void hud_initfonts( void );

void CG_InitQCHUD( void ) {
	hud_initbounds();
	hud_initmedia();
	hud_initfonts();
	hud_numpickups = 0;
}

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

void CG_AddPickup( int itemNum ) {
	int i;

	if ( hud_numpickups == MAX_PICKUPS ) { // remove the top pickup if the list is full
		for ( i = 0; i < MAX_PICKUPS - 1; i++ ) {
			memcpy( &hud_pickups[i], &hud_pickups[i + 1], sizeof( pickup_t ) );
		}
		hud_numpickups--;
	}
	for ( i = 0; i < hud_numpickups; i++ ) {
		if ( hud_pickups[i].itemNum == itemNum ) { // update item pickup time if i's already in the list
			hud_pickups[i].time = cg.time;
			return;
		}
	}
	hud_pickups[hud_numpickups].itemNum = itemNum;
	hud_pickups[hud_numpickups].time = cg.time;
	hud_numpickups++;
	hud_numpickups++;
}

// setup virtual coordinates
static void hud_initbounds( void ) {
	float aspv = VIRTUAL_ASPECT;
	float asp = ((float)cgs.glconfig.vidWidth)/((float)cgs.glconfig.vidHeight);
	float zz;
	if ( asp > aspv ) {
		bounds.top = 0.0f;
		bounds.bottom = VIRTUAL_HEIGHT;
		zz = 0.5f * VIRTUAL_HEIGHT * asp;
		bounds.left = floorf(0.5f * VIRTUAL_WIDTH - zz);
		bounds.right = VIRTUAL_WIDTH - bounds.left;
	} else {
		bounds.left = 0.0f;
		bounds.right = VIRTUAL_WIDTH;;
		zz = 0.5f * VIRTUAL_WIDTH / asp;
		bounds.top = floorf(0.5f * VIRTUAL_HEIGHT - zz);
		bounds.bottom = VIRTUAL_HEIGHT - bounds.top;
	}
	bounds.xscale = cgs.glconfig.vidWidth / ( bounds.right - bounds.left );
	bounds.yscale = cgs.glconfig.vidHeight / ( bounds.bottom - bounds.top );
	bounds.xoffs = bounds.xscale * bounds.left;
	bounds.yoffs = bounds.yscale * bounds.top;
}

// get all the required assets
static void hud_initmedia( void ) {
	int i;

	media.icon_health = cg_items[BG_FindItemByClass( "item_health_mega" ) - bg_itemlist].icon;
	media.icon_armor = cg_items[BG_FindItemByClass( "item_armor_body" ) - bg_itemlist].icon;
	for ( i = 0; i < NUM_HUD_WEAPONS; i++ ) {
		media.icon_weapon[i] = trap_R_RegisterShader( hud_weapon_icons[i] );
	}
	media.ammobar_background = trap_R_RegisterShader( "hud/ammobar/background" );
	media.ammobar_full = trap_R_RegisterShader( "hud/ammobar/full" );
	media.ammobar_empty = trap_R_RegisterShader( "hud/ammobar/empty" );
	media.ringgauge = trap_R_RegisterShader( "hud/ring" );
	media.ringglow = trap_R_RegisterShader( "hud/ring_glow" );
	media.abbg = trap_R_RegisterShader( "hud/abbg" );
	for ( i = 0; i < NUM_CHAMPIONS; i++ ) {
		media.skillicon[i] = trap_R_RegisterShader( va("hud/skill/%s", champion_names[i] ) );
	}
	media.gradient = trap_R_RegisterShaderNoMip( "hud/gradient" );
	media.radgrad = trap_R_RegisterShaderNoMip( "hud/radgrad" );
	for ( i = 0; i < bg_numItems; i++ ) {
		if ( bg_itemlist[i].icon != NULL && bg_itemlist[i].icon[0] )
		{
			media.itemicons[i] = trap_R_RegisterShader( va("hud/item%s", bg_itemlist[i].icon ) );
		} else {
			media.itemicons[i] = -1;
		}
	}
}

static void hud_initfont( font_t *font ) {
	int i;

	for ( i = 0; i < sizeof( font->charmap ) / sizeof( font->charmap[0] ); i++ ) {
		font->charmap[i] = NULL;
	}
	font->shader = trap_R_RegisterShader( font->hdr.name );
}

static void hud_initfonts( void ) {
	hud_initfont( &font_regular );
	//hud_initfont( &font_large );
	hud_initfont( &font_qcde );
}

// convert point coordinates from virtual to the real ones
void hud_translate_point( float *x, float *y ) {
	*x = *x * bounds.xscale - bounds.xoffs;
	*y = *y * bounds.yscale - bounds.yoffs;
}

// helper function to handle rectangles
void hud_translate_rect( float *x, float *y, float *w, float *h ) {
	float r, b;
	r = *x + *w;
	b = *y + *h;
	hud_translate_point( x, y );
	hud_translate_point( &r, &b );
	*w = r - *x;
	*h = b - *y;
}

// draw rectangular picture and use specified texture coordinates
void hud_drawpic_ex( 
	float x, float y, float width, float height, 
	float s1, float t1, float s2, float t2,
	qhandle_t hShader )
{
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, hShader );
}

// draw rectangular picture
// cx, cy are alignment control, i.e. 0.5 and 0.5 mean that (x,y) will point to the picture center
void hud_drawpic( float x, float y, float width, float height, float cx, float cy, qhandle_t hShader ) {
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x - cx * width, y - cy * height, width, height, 0, 0, 1, 1, hShader );
}

// draw bar of specified color
// cx, cy - see hud_drawpic()
void hud_drawbar( float x, float y, float width, float height, float cx, float cy, float *color ) {
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_SetColor( color );
	trap_R_DrawStretchPic( x - cx * width, y - cy * height, width, height, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_SetColor( NULL );
}

// find character by its id
static chardesc_t* find_char( font_t *font, int c ) {
	if ( c >= 0 && c < sizeof( font->charmap ) / sizeof( font->charmap[0] ) ) {
		if ( font->charmap[c] != NULL ) {
			return font->charmap[c];
		}
		for ( int i = 0; i < font->hdr.num_chars; i++ ) {
			if ( font->chars[i].id == c ) {
				font->charmap[c] = &font->chars[i];
				return &font->chars[i];
			}
		}
	}
	return NULL;
}

// returns horizontal advance
static float hud_drawchar( float x, float y, float scale, font_t *font, int c ) {
	chardesc_t *chr;
	float tx, ty;
	hud_translate_point( &x, &y );
	chr = find_char( font, c );
	if ( chr != NULL ) {
		tx = chr->x / (float)font->hdr.scale_w;
		ty = chr->y / (float)font->hdr.scale_h;
		trap_R_DrawStretchPic( 
			x, y - ( font->hdr.base - chr->yoffset ) * bounds.yscale * scale,
			chr->width * bounds.xscale * scale, chr->height * bounds.yscale * scale,
			tx, 1.0f - ty, 
			tx + chr->width / (float)font->hdr.scale_w, 1.0f - ( ty + chr->height / (float)font->hdr.scale_h ), 
			font->shader
		);
		return chr->xadvance * scale;
	}
	return 0.0f;
}

// returns string width if it should be rendered using the specified font
static float hud_measurestring( float scale, font_t *font, const char *string ) {
	float advance;
	const char *c;
	chardesc_t *chr;

	advance = 0;
	for ( c = string; *c; c++ ) {
		chr = find_char( font, *c );
		if ( chr != NULL ) {
			advance += chr->xadvance * scale;
		}
	}
	return advance;
}

// draws the string using specified font
static float hud_drawstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy ) {
	float advance;
	float color[4];
	const char *c;

	if ( shadow != NULL ) {
		memcpy( color, cg.lastColor, sizeof( color ) );
		trap_R_SetColor( shadow );
		advance = x + dx;
		for ( c = string; *c; c++ ) {
			advance += hud_drawchar( advance, y + dy, scale, font, *c );
		}
		trap_R_SetColor( color );
	}
	advance = x;
	for ( c = string; *c; c++ ) {
		advance += hud_drawchar( advance, y, scale, font, *c );
	}
	return advance - x;
}

static float hud_measurecolorstring(float scale, font_t *font, const char *string ) {
	chardesc_t *chr;
	const char	*s;
	int			xx;
	int			cnt;

	// draw the colored text
	s = string;
	xx = 0;
	cnt = 0;
	while ( *s && cnt < 32768 ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		chr = find_char( font, *s );
		xx += chr->xadvance * scale;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
	return xx;
}

// advanced version of hud_drawstring, it handles q3 color codes
static float hud_drawcolorstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy ) {
	vec4_t		color, shad;
	const char	*s;
	int			xx;
	int			cnt;
	float		alpha;

	alpha = cg.lastColor[3];

	// draw the shadow
	if ( shadow != NULL ) {
		memcpy( shad, shadow, sizeof( float ) * 3 );
		shad[3] = alpha;
		trap_R_SetColor( shad );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < 32768 ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			xx += hud_drawchar( xx + dx, y + dy, scale, font, *s );
			cnt++;
			s++;
		}
	}
	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	color[0] = color[1] = color[2] = 1.0f;
	color[3] = alpha;
	trap_R_SetColor( color );
	while ( *s && cnt < 32768 ) {
		if ( Q_IsColorString( s ) ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( float ) * 3 );
				color[3] = alpha;
				trap_R_SetColor( color );
			s += 2;
			continue;
		}
		xx += hud_drawchar( xx, y, scale, font, *s );
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
	return xx - x;
}

// horizontal bar gauge used to indicate player's health and ammo
void hud_bargauge(
	int value,
	int basevalue,
	int barvalue,
	float x, float y,
	float width, float height,
	float space,
	float* whitecolor,
	float* redcolor,
	float* bluecolor
	)
{
	int i;
	int whitebars, redbars, bluebars;

	if ( value < 0 ) {
		value = 0;
	}

	redbars = basevalue / barvalue;
	whitebars = value / barvalue;
	bluebars = 0;
	if ( whitebars > redbars ) {
		whitebars = redbars;
		bluebars = (int)ceilf(  ( value - basevalue ) / (float)barvalue );
	}
	redbars -= whitebars;
	for ( i = 0; i < whitebars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, whitecolor );
		x += ( width + space );
	}
	for ( i = 0; i < redbars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, redcolor );
		x += ( width + space );
	}
	for ( i = 0; i < bluebars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, bluecolor );
		x += ( width + space );
	}
}

// draws player status (bottom left corner)
void hud_drawstatus( void ) {
	clientInfo_t *ci;
	champion_stat_t *cs;
	playerState_t *ps;
	centity_t *cent;

	float x;

	static float whitecolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float redcolor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	static float bluecolor[] = { 0.25f, 0.8f, 1.0f, 1.0f };
	static float greencolor[] = { 0.4f, 1.0f, 0.26f, 1.0f };

	ps = &cg.snap->ps;
	cent = &cg_entities[cg.snap->ps.clientNum];
	ci = &cgs.clientinfo[ ps->clientNum ];
	cs = &champion_stats[ ps->champion ];

	// draw head
	hud_drawpic( bounds.left + 160, bounds.bottom - 160, 160, 160, 0.5f, 0.5f, ci->modelIcon );

	// draw health
	hud_drawpic( bounds.left + 256, bounds.bottom - 174, 40, 40, 0.5f, 0.5f, media.icon_health );
	hud_bargauge( ps->stats[STAT_HEALTH], cs->base_health, 25, 
		bounds.left + 280, bounds.bottom - 182, 
		26, 16, 5, 
		whitecolor, redcolor, bluecolor );
	x = hud_drawstring( bounds.left + 277, bounds.bottom - 190, 0.78f, &font_qcde, va( "%d", ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0 ), NULL, 0, 0 );
	hud_drawstring( bounds.left + 277 + x, bounds.bottom - 190, 0.45f, &font_qcde, va( "/%d", cs->base_health ), NULL, 0, 0 );

	// draw armor
	hud_drawpic( bounds.left + 230, bounds.bottom - 84, 40, 40, 0.5f, 0.5f, media.icon_armor );
	hud_bargauge( ps->stats[STAT_ARMOR], cs->base_armor, 25,
		bounds.left + 253, bounds.bottom - 92,
		26, 16, 5,
		whitecolor, redcolor, greencolor );
	x = hud_drawstring( bounds.left + 253, bounds.bottom - 99, 0.78f, &font_qcde, va( "%d", ps->stats[STAT_ARMOR] > 0 ? ps->stats[STAT_ARMOR] : 0 ), NULL, 0, 0 );
	hud_drawstring( bounds.left + 253 + x, bounds.bottom - 99, 0.45f, &font_qcde, va( "/%d", cs->base_armor ), NULL, 0, 0 );
}

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

	// currently selected weapon
	weapon = cent->currentState.weapon;
	if ( weapon ) {
		if ( weapon != WP_GAUNTLET ) {
			icon = cg_weapons[weapon].ammoIcon;
			value = ps->ammo[cent->currentState.weapon];
			if ( value < 0 ) {
				value = 0;
			}
			ammo = va( "%d", value );
			ammow = hud_measurestring( 1.15f, &font_qcde, ammo );
			hud_drawstring( bounds.right - 185 - ammow, bounds.bottom - 95, 1.15f, &font_qcde, va( "%d", value ), NULL, 0, 0 );
		}
		else {
			icon = cg_weapons[weapon].weaponIcon;
		}
		hud_drawpic( bounds.right - 135, bounds.bottom - 128, 64, 64, 0.5f, 0.5f, icon );
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
			color[0] = hud_weapon_colors[i][0];
			color[1] = hud_weapon_colors[i][1];
			color[2] = hud_weapon_colors[i][2];
		} else {
			color[0] = color[1] = color[2] = 1.0f;
		}

		icon = media.icon_weapon[i]; //cg_weapons[weapon].weaponIcon;
		value = ps->ammo[weapon];
		color[3] = has_weapon ? ( weapon == current_weapon ? 1.0f : 0.3f ) : 0.15f;
		trap_R_SetColor( color );
		hud_drawpic( bounds.right - 145, bounds.bottom - y, 96, 96, 0.5f, 0.5f, icon );
		if ( weapon == current_weapon ) {
			hud_drawpic( bounds.right - 126, bounds.bottom - y - 48, 96, 96, 0.0f, 0.0f, media.ammobar_background );
		}
		color[0] = color[1] = color[2] = 0.7f;
		trap_R_SetColor( color );
		if ( has_weapon ) {
			hud_drawpic( bounds.right - 126, bounds.bottom - y - 48, 96, 96, 0.0f, 0.0f, media.ammobar_empty );
			percentage = value / (float)bg_maxAmmo[weapon];
			if ( percentage < 0.0f ) {
				percentage = 0.0f;
			}
			if ( percentage > 1.0f ) {
				percentage = 1.0f;
			}
			percentage *= 75.0f/128;
			percentage += 28.0f/128;
			hud_drawpic_ex( bounds.right - 126, bounds.bottom - y + 48 - 96 * percentage, 96, 96 * percentage, 
				0.0f, percentage, 1.0f, 0.0f,
				media.ammobar_full
			);
		}
		if ( has_weapon ) {
			ammo = va( "%d", value );
			fontsize = ( weapon == current_weapon ? 0.4f : 0.32f );
			ammow = hud_measurestring( fontsize, &font_regular, ammo );
			hud_drawstring( bounds.right - 78 - ammow, bounds.bottom - y + ( weapon == current_weapon ? 40 : 36 ), fontsize, &font_regular, ammo, NULL, 0, 0 );
		}
		y -= 70;
	}
	trap_R_SetColor( NULL );
}

// single brief score bar for FFA
void hud_drawscorebar_ffa( float y, float *color, const char *playername, int rank, int score ) {
	float centerx, dim, mdim;

	centerx = 0.5f * ( bounds.left + bounds.right );

	trap_R_SetColor( color );
	hud_drawpic( centerx - 107, y, 43, 26, 0.0f, 0.0f, cgs.media.whiteShader );
	hud_drawpic( centerx - 63, y, 228, 26, 0.0f, 0.0f, cgs.media.whiteShader );
	hud_drawpic( centerx + 166, y, 43, 26, 0.0f, 0.0f, cgs.media.whiteShader );

	trap_R_SetColor( NULL );
	hud_drawcolorstring( centerx - 59, y + 21, 0.35f, &font_regular, playername, NULL, 0, 0 );
	dim = hud_measurestring( 0.35f, &font_regular, va( "%d", score ) );
	mdim = score < 0 ? hud_measurestring( 0.35f, &font_regular, "-" ) : 0;
	hud_drawstring( centerx + 188 - dim/2 - mdim / 2, y + 21, 0.35f, &font_regular, va( "%d", score ), NULL, 0, 0 );
	dim = hud_measurestring( 0.35f, &font_regular, va( "%d", rank) );
	hud_drawstring( centerx - 84 - dim/2, y + 21, 0.35f, &font_regular, va( "%d", rank ), NULL, 0, 0 );
}

// brief score bar for FFA, along with the timer
void hud_drawscores_brief_ffa( void ) {
	static float color1[] = { 0.85f/1.5f, 0.46f/1.5f, 0.18f/1.5f, 0.5f };
	static float color2[] = { 0.26f/1.5f, 0.70f/1.5f, 0.89f/1.5f, 0.5f };
	static float shadow[] = { 0.0f, 0.0f, 0.0f, 1.0f };


	playerState_t *ps;

	int	mins, seconds, tens, msec;
	float dim, centerx;
	int rank, other, otherscore;

	ps = &cg.snap->ps;
	rank = ps->persistant[PERS_RANK] & ( RANK_TIED_FLAG - 1 );

	centerx = 0.5f * ( bounds.left + bounds.right );
	msec = cg.time - cgs.levelStartTime;
	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	if ( cg.predictedPlayerState.pm_type != PM_DEAD ) {
		if ( rank == 0 ) {
			// if we have rank 0, then always draw our name on top
			hud_drawscorebar_ffa( bounds.top + 54, color1, cgs.clientinfo[cg.clientNum].name, 1, ps->persistant[PERS_SCORE] );
			// choose the second leader
			other = cgs.leader1;
			otherscore = cgs.scores1;
			if ( other == cg.clientNum ) { // any of leader1 and leader2 can be us, so choose the one which is not us
				other = cgs.leader2;
				otherscore = cgs.scores2;
			}
			if ( other != -1 ) {
				hud_drawscorebar_ffa( bounds.top + 54 + 28, color2, cgs.clientinfo[other].name, 2, otherscore );
			}
		} else {
			hud_drawscorebar_ffa( bounds.top + 54, color2, cgs.clientinfo[cgs.leader1].name, 1, cgs.scores1 );
			hud_drawscorebar_ffa( bounds.top + 54 + 28, color1, cgs.clientinfo[cg.clientNum].name, rank + 1, ps->persistant[PERS_SCORE] );
		}
	}

	dim = hud_measurestring( 0.65f, &font_qcde, va( "%d", mins ) );	
	trap_R_SetColor( NULL );
	hud_drawstring( centerx - 185 - dim, bounds.top + 100, 0.65f, &font_qcde, va( "%d", mins ), shadow, 1, 1 );
	hud_drawstring( centerx - 185, bounds.top + 100, 0.65f, &font_qcde, va( ":%d%d", tens, seconds ), shadow, 1, 1 );
}

void calc_sector_point( float angle, float *x, float *y ) {
	if ( ( angle > 315.0f ) || ( angle <= 45.0f ) ) {
		*x = 1.0f;
		*y = tanf( DEG2RAD( angle ) );
		return;
	}
	if ( ( angle > 45.0f ) && ( angle <= 135.0f ) ) {
		*x = tanf( DEG2RAD( 90.0f - angle ) );
		*y = 1.0f;
		return;
	}
	if ( ( angle > 135.0f ) && ( angle <= 225.0f ) ) {
		*x = -1.0f;
		*y = -tanf( DEG2RAD( angle ) );
		return;
	}
	*x = -tanf( DEG2RAD( 90.0f - angle ) );
	*y = -1.0f;
}

int get_next_corner( int angle ) {
	angle /= 45;
	angle *= 45;
	angle += 45;
	if ( angle % 90 == 0 ) {
		angle += 45;
	}
	angle %= 360;
	return angle;
}

void hud_draw_ability( void ) {
	playerState_t *ps;
	static float yellow[] = { 1.0f, 0.8f, 0.0f, 1.0f };
	static float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	ps = &cg.snap->ps;
	float overall = champion_stats[ps->champion].ability_cooldown;
	float current = ps->ab_time;
	float dim;
	int sec_left;

	float gaugex, gaugey, gaugesize;

	float x[16];
	float y[16];
	int num, pos;

	int start_angle = 270;
	int end_angle = (int)( 270 + current/overall * 360);
	int angle, ta;

	gaugex = ( bounds.left + bounds.right ) / 2;
	gaugey = bounds.bottom - 160;
	gaugesize = 128;

	// ability gauge background
	white[3] = 0.5f;
	trap_R_SetColor( white );
	hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.abbg );
	trap_R_SetColor( NULL );

	if ( ps->ab_time == champion_stats[ps->champion].ability_cooldown) {
		// ability is fully charged, draw the ring and its glow
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringgauge );
		trap_R_SetColor( yellow );
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringglow );
		trap_R_SetColor( NULL );
		// draw the ability icon
		hud_drawpic( gaugex, gaugey, gaugesize * 0.75f, gaugesize * 0.75f, 0.5f, 0.5f, media.skillicon[ps->champion] );
		return;
	} else {
		// ability is not fully charged, draw the full ring semi-transparently
		white[3] = 0.5f;
		trap_R_SetColor( white) ;
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringgauge );
		trap_R_SetColor( NULL );
	}

	if ( ps->ab_flags & ABF_ENGAGED ) {
		// ability is enaged, so draw the icon semi-transparently
		trap_R_SetColor( white );
		hud_drawpic( gaugex, gaugey, gaugesize * 0.75f, gaugesize * 0.75f, 0.5f, 0.5f, media.skillicon[ps->champion] );
		trap_R_SetColor( NULL );
		return;
	}

	// calculate points along the rectangle perimeter which represent the needed ring sector
	num = 0;
	angle = start_angle;
	if ( end_angle < start_angle ) {
		end_angle += 360;
	}
	while( 1 ) {
		calc_sector_point( angle % 360, &x[num], &y[num] );
		num++;
		ta = get_next_corner( angle );
		if ( ta < angle ) {
			ta += 360;
		}
		angle = ta;
		if ( angle >= end_angle ) {
			calc_sector_point( end_angle % 360, &x[num], &y[num] );
			num++;
			break;
		}
	}

	// convert the points calculated above into real coordinates and render them in groups of 4
	float scalex = gaugesize / 2, scaley = gaugesize / 2, offsetx = gaugex, offsety = gaugey;
	float x0, y0, x1, y1, x2, y2, x3, y3;
	float s0, t0, s1, t1, s2, t2, s3, t3;

	pos = 0;
	while ( 1 ) {
		x0 = offsetx; y0 = offsety; s0 = 0.5f; t0 = 0.5f;
		hud_translate_point( &x0, &y0 );
		x1 = x[pos] * scalex + offsetx; s1 = x[pos] / 2.0f + 0.5f;
		y1 = y[pos] * scaley + offsety; t1 = y[pos] / 2.0f + 0.5f;
		hud_translate_point( &x1, &y1 );
		x2 = x[pos + 1] * scalex + offsetx; s2 = x[pos + 1] / 2.0f + 0.5f;
		y2 = y[pos + 1] * scaley + offsety;	t2 = y[pos + 1] / 2.0f + 0.5f;
		hud_translate_point( &x2, &y2 );
		if ( pos + 2 >= num ) {
			// not enough geometry, so the last quad is a degenerate
			x3 = x2;
			y3 = y2;
			s3 = s2;
			t3 = t2;
		} else {
			x3 = x[pos + 2] * scalex + offsetx; s3 = x[pos + 2] / 2.0f + 0.5f;
			y3 = y[pos + 2] * scaley + offsety;	t3 = y[pos + 2] / 2.0f + 0.5f;
			hud_translate_point( &x3, &y3 );
		}
		trap_R_DrawQuad( x0, y0, s0, t0, x1, y1, s1, t1, x2, y2, s2, t2, x3, y3, s3, t3, media.ringgauge );
		pos += 2;
		if ( pos >= num - 1 ) {
			break;
		}
	}
	// draw the ability timer (how many seconds left to the full recharge)
	sec_left = champion_stats[ps->champion].ability_cooldown - ps->ab_time;
	dim = hud_measurestring( 0.5f, &font_qcde, va( "%d", sec_left ) );
	hud_drawstring( gaugex - dim / 2, gaugey + 15, 0.5f, &font_qcde, va( "%d", sec_left ), NULL, 0, 0 );
}

static void hud_purgepickups( void ) {
	static pickup_t pickups[MAX_PICKUPS];
	int i, numpickups;
	memcpy( pickups, hud_pickups, sizeof ( pickups ) );
	numpickups = 0;
	for ( i = 0; i < hud_numpickups; i ++ ) {
		if ( cg.time - pickups[i].time > PICKUP_TIMEOUT ) {
			continue;
		}
		memcpy( &hud_pickups[numpickups], &pickups[i], sizeof( pickup_t ) );
		numpickups++;
	}
	hud_numpickups = numpickups;
}

static void hud_drawpickups( void ) {
	static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float shadow[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	int i;
	float dim, y;
	pickup_t *p;

	hud_purgepickups();

	y = 80;

	for ( p = hud_pickups; p < hud_pickups + hud_numpickups; p++ ) {
		if ( cg.time - p->time > PICKUP_FADEOUTTIME ) {
			color[3] = 1.0f - ( (float)cg.time - p->time ) / ( PICKUP_TIMEOUT - PICKUP_FADEOUTTIME );
		}
		else {
			color[3] = 1.0f;
		}
		shadow[3] = color[3];
		trap_R_SetColor( color );
		hud_drawpic( bounds.right - 60, y, 32, 32, 0.5f, 0.5f, media.itemicons[p->itemNum] );
		dim = hud_measurestring( 0.4f, &font_regular, bg_itemlist[p->itemNum].pickup_name );
		hud_drawstring( bounds.right - 88 - dim, y + 8, 0.4f, &font_regular, bg_itemlist[p->itemNum].pickup_name, shadow, 2, 2 );
		y += 40;
	}
	trap_R_SetColor( NULL );
}

/*
===================
hud_drawcenterstring
===================
*/
static void hud_drawfragmessage( void ) {
	int		x, y, w;
	float	*color;
	float	shadow[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	if ( hud_fragtime == -1 ) {
		return;
	}

	color = CG_FadeColor( hud_fragtime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}
	shadow[3] = color[3];
	y = bounds.top + 436;
	w = hud_measurecolorstring( 0.5f, &font_regular, hud_fragmessage );
	x = ( bounds.right - bounds.left - w ) / 2.0f;

	trap_R_SetColor( color );
	hud_drawcolorstring( x, y, 0.5f, &font_regular, hud_fragmessage, shadow, 3, 3 );
	w = hud_measurestring( 0.4f, &font_regular, hud_rankmessage );
	x = ( bounds.right - bounds.left -w ) / 2.0f;
	color[0] = color[1] = color[2] = 0.75f;

	trap_R_SetColor( color );
	hud_drawstring( x, y + 26, 0.4f, &font_regular, hud_rankmessage, shadow, 2, 2 );
	trap_R_SetColor( NULL );
}

// dir: gradient direction, 0: left to right, 1: right to left, 2: top to bottom, 3: bottom to top
static void hud_drawgradient( float x, float y, float w, float h, int dir ) {
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
		media.gradient 
	);
}

/*
===================
hud_drawdeathmessage
===================
*/
static void hud_drawdeathmessage( void ) {
	float dim, dim1, dim2, centerx, top;
	char *text;
	static float bg[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
	static float bgline[4] = { 0.76f, 0.24f, 0.26f, 0.5f };
	static float gray[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
	static float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	int i;

	if ( cg.killerInfo.clientNum < 0 || cg.killerInfo.clientNum >= MAX_CLIENTS || cg.predictedPlayerState.pm_type != PM_DEAD ) {
		return;
	}

	// measure the text
	centerx = ( bounds.left + bounds.right ) * 0.5f;
	top = bounds.top + 250;

	text = va( "FRAGGED BY ^1%s", cg.killerInfo.clientNum >= 0 ? cgs.clientinfo[cg.killerInfo.clientNum].name : "DJIGURDA" );
	dim = hud_measurecolorstring( 0.5f, &font_regular, text );
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
	hud_drawpic_ex( centerx - 384, top + 56 + 3, 768, 256, 0, 0.5f, 1.0f, 1.0f, media.radgrad );
	hud_drawpic_ex( centerx - 384, top - 3 - 384, 768, 384, 0, 0, 1.0f, 0.5f, media.radgrad );
	// draw the killer face
	trap_R_SetColor( NULL );
	if ( cg.killerInfo.clientNum >= 0 && cg.killerInfo.clientNum < cgs.maxclients ) {
		hud_drawpic( centerx, top - 3, 128, 128, 0.5f, 1.0f, cgs.clientinfo[cg.killerInfo.clientNum].modelIcon );
	}
	// draw the text
	trap_R_SetColor( NULL );
	hud_drawcolorstring( centerx - dim/2, top + 40, 0.5f, &font_regular, text, NULL, 0, 0 );
	// draw means of death icon (weapon icon, ability icon, etc)
	for ( i = 0; i < NUM_HUD_WEAPONS; i++ ) {
		if ( hud_weapons[i] == cg.killerInfo.weapon ) {
			trap_R_SetColor( hud_weapon_colors[i] ) ;
			hud_drawpic( centerx + dim/2 - 80 + 12, top + 30, 80, 64,  0.0f, 0.5f, media.icon_weapon[i] );
			trap_R_SetColor( NULL );
			break;
		}
	}
	// draw stack left
	text = va( "STACK LEFT", cg.killerInfo.health, cg.killerInfo.armor );
	dim = hud_measurestring( 0.3f, &font_regular, text );
	trap_R_SetColor( gray );
	hud_drawstring( centerx - dim/2, top + 90, 0.3f, &font_regular, text, NULL, 0, 0 );
	trap_R_SetColor( NULL );
	dim1 = hud_measurestring( 0.4f, &font_regular, va( "%d", cg.killerInfo.health ) );
	dim2 = hud_measurestring( 0.4f, &font_regular, va( "%d", cg.killerInfo.armor ) );
	dim = dim1 + dim2 + 2 * 24 + 2 * 4 + 44;
	dim = centerx - dim/2;
	hud_drawpic( dim, top + 124, 24, 24, 0.0f, 0.5f, media.icon_health );
	dim += 24 + 4;
	dim += hud_drawstring( dim, top + 133, 0.4f, &font_regular, va( "%d", cg.killerInfo.health ), NULL, 0, 0 );
	dim += 44;
	hud_drawpic( dim, top + 124, 24, 24, 0.0f, 0.5f, media.icon_armor );
	dim += 24 + 4;
	hud_drawstring( dim, top + 133, 0.4f, &font_regular, va( "%d", cg.killerInfo.armor ), NULL, 0, 0 );
}

/*
=================
hud_drawcrosshair
=================
*/
static void hud_drawcrosshair(void)
{
	float		w, h;
	qhandle_t	hShader;
	float		f;
	int			ca;

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

	hud_drawpic( ( bounds.left + bounds.right ) / 2.0f, ( bounds.top + bounds.bottom ) / 2.0f, w, h, 0.5f, 0.5f, hShader );

	//trap_R_DrawStretchPic( x + cg.refdef.x + 0.5 * (cg.refdef.width - w), 
	//	y + cg.refdef.y + 0.5 * (cg.refdef.height - h), 
	//	w, h, 0, 0, 1, 1, hShader );

	trap_R_SetColor( NULL );
}


/*
=================
CG_Draw2DQC
=================
*/

void CG_Draw2DQC( stereoFrame_t stereoFrame ) {
	//CG_DrawPic( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, cgs.media.armorIcon );
	//qch_drawpic( bounds.left + 100, bounds.top + 100, 100, 100, cgs.media.armorIcon );

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
#if 0x0
		CG_DrawSpectator();

		if(stereoFrame == STEREO_CENTER)
			CG_DrawCrosshair();
		CG_DrawCrosshairNames();
#endif
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if ( !cg.showScores && ( cg.predictedPlayerState.pm_type != PM_DEAD ) ) {
#if 0x0
			//CG_DrawStatusBar();

			CG_DrawAmmoWarning();
			if(stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();
			CG_DrawCrosshairNames();
			CG_DrawWeaponSelect();
			CG_DrawHoldableItem();
			CG_DrawReward();
#endif
			trap_R_SetColor( NULL );
			hud_draw_ability();
			hud_drawstatus();
			hud_drawammo();
			hud_drawfragmessage();
			hud_drawpickups();

			if ( stereoFrame == STEREO_CENTER ) {
				hud_drawcrosshair();
			}
		}
	}
	if ( !cg.showScores ) {
		hud_drawdeathmessage();
		hud_drawscores_brief_ffa();
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


	//hud_drawstring( bounds.left, bounds.bottom, 0.0f, 1.0f, &font_large, "Amazing ARCADII" );

	//CG_DrawUpperRight(stereoFrame);
	//CG_DrawLowerRight();
	//CG_DrawLowerLeft();


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
