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

void hud_initbounds( void );
void hud_initmedia( void );
void hud_initfonts( void );

void CG_InitQCHUD( void ) {
	hud_initbounds();
	hud_initmedia();
	hud_initfonts();
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
	//media.playernamebg_brief = trap_R_RegisterShader( "hud/scoreline_brief" );
	//media.scoreline_brief = trap_R_RegisterShader( "hud/playernamebg_brief" );
}

static void hud_initfont( font_t *font ) {
	int i;
	chardesc_t *chr;

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
static float hud_drawchar( float x, float y, float valign, float scale, font_t *font, int c ) {
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
static float hud_drawstring( float x, float y, float valign, float scale, font_t *font, const char *string ) {
	float advance;
	const char *c;

	advance = x;
	for ( c = string; *c; c++ ) {
		advance += hud_drawchar( advance, y, valign, scale, font, *c );
	}
	return advance - x;
}

// advanced version of hud_drawstring, it handles q3 color codes
static float hud_drawstring_ex( float x, float y, float valign, float scale, font_t *font, const char *string, const float *setColor, qboolean forceColor ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	int			cnt;

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < 32768 ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor == NULL ? 1.0f : setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		//CG_DrawChar( xx, y, charWidth, charHeight, *s );
		xx += hud_drawchar( xx, y, valign, scale, font, *s );
		//xx += charWidth;
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

	int i;
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
	x = hud_drawstring( bounds.left + 277, bounds.bottom - 190, 0.0f, 0.78f, &font_qcde, va( "%d", ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0 ) );
	hud_drawstring( bounds.left + 277 + x, bounds.bottom - 190, 0.0f, 0.45f, &font_qcde, va( "/%d", cs->base_health ) );

	// draw armor
	hud_drawpic( bounds.left + 230, bounds.bottom - 84, 40, 40, 0.5f, 0.5f, media.icon_armor );
	hud_bargauge( ps->stats[STAT_ARMOR], cs->base_armor, 25,
		bounds.left + 253, bounds.bottom - 92,
		26, 16, 5,
		whitecolor, redcolor, greencolor );
	x = hud_drawstring( bounds.left + 253, bounds.bottom - 99, 0.0f, 0.78f, &font_qcde, va( "%d", ps->stats[STAT_ARMOR] > 0 ? ps->stats[STAT_ARMOR] : 0 ) );
	hud_drawstring( bounds.left + 253 + x, bounds.bottom - 99, 0.0f, 0.45f, &font_qcde, va( "/%d", cs->base_armor ) );
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
			hud_drawstring( bounds.right - 185 - ammow, bounds.bottom - 95, 0.0f, 1.15f, &font_qcde, va( "%d", value ));
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
			hud_drawstring( bounds.right - 78 - ammow, bounds.bottom - y + ( weapon == current_weapon ? 40 : 36 ), 0.0f, fontsize, &font_regular, ammo );
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
	hud_drawstring_ex( centerx - 59, y + 21, 0.0f, 0.35f, &font_regular, playername, NULL, qfalse );
	dim = hud_measurestring( 0.35f, &font_regular, va( "%d", score ) );
	mdim = score < 0 ? hud_measurestring( 0.35f, &font_regular, "-" ) : 0;
	hud_drawstring( centerx + 188 - dim/2 - mdim / 2, y + 21, 0.0f, 0.35f, &font_regular, va( "%d", score ) );
	dim = hud_measurestring( 0.35f, &font_regular, va( "%d", rank) );
	hud_drawstring( centerx - 84 - dim/2, y + 21, 0.0f, 0.35f, &font_regular, va( "%d", rank ) );
}

// brief score bar for FFA, along with the timer
void hud_drawscores_brief_ffa( void ) {
	static float color1[] = { 0.85f/1.5f, 0.46f/1.5f, 0.18f/1.5f, 0.5f };
	static float color2[] = { 0.26f/1.5f, 0.70f/1.5f, 0.89f/1.5f, 0.5f };

	playerState_t *ps;

	char *time;
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
	dim = hud_measurestring( 0.65f, &font_qcde, va( "%d", mins ) );	
	hud_drawstring( centerx - 185 - dim, bounds.top + 100, 0.0f, 0.65f, &font_qcde, va( "%d", mins ) );
	hud_drawstring( centerx - 185, bounds.top + 100, 0.0f, 0.65f, &font_qcde, va( ":%d%d", tens, seconds ) );
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
	int sec_left, tens;

	float gaugex, gaugey, gaugesize;

	float x[16];
	float y[16];
	float s[16];
	float t[16];
	int num, pos;

	int start_angle = 270;
	int end_angle = (int)( 270 + current/overall * 360);
	int angle, ta;

	gaugex = ( bounds.left + bounds.right ) / 2;
	gaugey = bounds.bottom - 160;
	gaugesize = 128;

	white[3] = 0.5f;
	trap_R_SetColor( white );
	hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.abbg );
	trap_R_SetColor( NULL );

	if ( ps->ab_time == champion_stats[ps->champion].ability_cooldown) {
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringgauge );
		trap_R_SetColor( yellow );
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringglow );
		trap_R_SetColor( NULL );
		hud_drawpic( gaugex, gaugey, gaugesize * 0.75f, gaugesize * 0.75f, 0.5f, 0.5f, media.skillicon[ps->champion] );
		return;
	} else {
		white[3] = 0.5f;
		trap_R_SetColor( white) ;
		hud_drawpic( gaugex, gaugey, gaugesize, gaugesize, 0.5f, 0.5f, media.ringgauge );
		trap_R_SetColor( NULL );
	}

	if ( ps->ab_flags & ABF_ENGAGED ) {
		trap_R_SetColor( white );
		hud_drawpic( gaugex, gaugey, gaugesize * 0.75f, gaugesize * 0.75f, 0.5f, 0.5f, media.skillicon[ps->champion] );
		trap_R_SetColor( NULL );
		return;
	}

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
	
	sec_left = champion_stats[ps->champion].ability_cooldown - ps->ab_time;
	dim = hud_measurestring( 0.5f, &font_qcde, va( "%d", sec_left ) );
	hud_drawstring( gaugex - dim / 2, gaugey + 15, 0, 0.5f, &font_qcde, va( "%d", sec_left ) );
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
	float		x, y;
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
		if ( !cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0 ) {
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
		}
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

	trap_R_SetColor( NULL );

	hud_draw_ability();


	hud_drawstatus();
	hud_drawammo();
	hud_drawscores_brief_ffa();
	if ( stereoFrame == STEREO_CENTER ) {
		hud_drawcrosshair();
	}

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
