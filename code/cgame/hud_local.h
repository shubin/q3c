#if !defined( HUD_LOCAL_H )
#define HUD_LOCAL_H

#include "../hudlib/hudlib.h"

#define ENABLE_GRENADEL 0

#define MAX_PICKUPS 16
#define PICKUP_TIMEOUT 1500
#define PICKUP_FADEOUTTIME 1000
#define MAX_OBITUARY 16
//#define OBITUARY_TIMEOUT 1500
//#define OBITUARY_FADEOUTTIME 1000

extern vec4_t       hud_weapon_colors[WP_NUM_WEAPONS];
extern const char   *hud_weapon_icons[WP_NUM_WEAPONS];
extern const char	*hud_mod_icons[MOD_NUM];

typedef struct {
	font_t* font_qcde;
	font_t* font_regular;
	qhandle_t	icon_health;	// for player status & item stats
	qhandle_t	icon_armor;		// for player status & item stats
	qhandle_t	icon_yellow_armor; // for item stats
	qhandle_t	icon_hourglass; // for item stats
	qhandle_t	icon_death; // for generic kill message

	qhandle_t	icon_weapon[WP_NUM_WEAPONS];	// weapon icons for the vertical ammo status bar
	qhandle_t	icon_mod[MOD_NUM];				// means of death icons

	qhandle_t	ammobar_background, ammobar_full, ammobar_empty; // various graphic for the vertical ammo status bar
	qhandle_t	ringgauge, ringglow, abbg;
	qhandle_t	skillicon[NUM_CHAMPIONS];
	qhandle_t	bigface[NUM_CHAMPIONS];
	qhandle_t	smallface[NUM_CHAMPIONS];
	qhandle_t	gradient, radgrad;
	qhandle_t	itemicons[MAX_ITEMS];
} hud_media_t;

extern hud_media_t hud_media;

// dir: gradient direction, 0: left to right, 1: right to left, 2: top to bottom, 3: bottom to top
void hud_drawgradient( float x, float y, float w, float h, int dir );
// draw timer
void hud_drawtimer( float x, float y );

// get all the required assets
void hud_initmedia( void );
// draws ammo information, big one in the bottom right corner displays the current weapon ammo, and there's also vertical bar with all the weapons
void hud_drawammo( void );
// draws player status (bottom left corner)
void hud_drawstatus( void );
// brief score bar for FFA, along with the timer
void hud_drawscores_brief_ffa( void );
// ffa scores
void hud_drawscores_ffa( void );
// brief tournament scores with timer
void hud_drawscores_brief_tournament( void );
// tournament scores with weapon usage stats, item stats etc.
void hud_drawscores_tournament( void );
//
void hud_drawscores_brief_tdm( void );
//
void hud_drawscores_tdm( void );
// ability status
void hud_draw_ability( void );
// crosshair
void hud_drawcrosshair( void );

void hud_initpickups( void );
void hud_drawpickups( void );

void hud_initobituary( void );
void hud_drawobituary( void );

void hud_drawfragmessage( void );
void hud_drawdeathmessage( void );

#endif
