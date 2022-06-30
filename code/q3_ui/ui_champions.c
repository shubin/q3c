
/*
=======================================================================

CHAMPIONS & STARTING WEAPONS MENU

=======================================================================
*/


#include "ui_local.h"

#define MODEL_SELECT		"menu/art/opponents_select"
#define MODEL_SELECTED		"menu/art/opponents_selected"

#define ID_RESPAWN		1
#define ID_CHAMPION		100
#define ID_WEAPON		200

static int weapons[] = {
	WP_LOUSY_MACHINEGUN,
	WP_LOUSY_SHOTGUN,
	WP_LOUSY_PLASMAGUN,
};

static char* weapon_names[] = {
	"menu/art/machinegun",
	"menu/art/shotgun",
	"menu/art/plasmagun",
};

static qboolean champ_locked[NUM_CHAMPIONS] = {
    qtrue, // sarge
    qfalse, // anarki
    qtrue, // athena
    qtrue, // nyx
    qtrue, // slash
    qtrue, // bj
    qtrue, // dk
    qtrue, // doom
    qtrue, // eisen
    qtrue, // galena
    qfalse, // ranger
    qtrue, // strogg
    qtrue, // visor
    qtrue, // clutch
    qfalse, // keel
    qtrue, // scalebearer
    qtrue, // sorlag
};

typedef struct {
	menuframework_s	menu;
	menutext_s		item_respawn;
	char			names[NUM_CHAMPIONS][MAX_QPATH];
	menubitmap_s	pics[NUM_CHAMPIONS];
	menubitmap_s	picbuttons[NUM_CHAMPIONS];

	menubitmap_s	wpics[ARRAY_LEN(weapons)];
	menubitmap_s	wpicbuttons[ARRAY_LEN(weapons)];

	int champion, weapon;
} championsmenu_t;

static championsmenu_t	s_championsmenu;

void ChampionsMenu_Cache( void );

/*
=================
ChampionsMenu_Event
=================
*/
void ChampionsMenu_Event( void *ptr, void *next, int notification ) {
	if ( notification == QM_CLOSED ) {
		if ( next == NULL ) {
			trap_Cmd_ExecuteText( EXEC_NOW, "respawn" );
		}
	}
}

static void UI_ChampionsMenu_RespawnEvent( void* ptr, int notification ) {
	if (notification != QM_ACTIVATED) {
		return;
	}
	UI_SetActiveMenu( UIMENU_NONE );
}

void ChampionsMenu_PicEvent( void *ptr, int notification ) {
	int i;

	if ( notification != QM_ACTIVATED )
		return;

	for ( i = 1; i < NUM_CHAMPIONS; i++)
	{
		// reset
 		s_championsmenu.pics[i].generic.flags       &= ~QMF_HIGHLIGHT;
 		s_championsmenu.picbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected
	i = ((menucommon_s*)ptr)->id - ID_CHAMPION;

	if ( i >= 0 && i < NUM_CHAMPIONS ) {
		s_championsmenu.champion = i;
		s_championsmenu.pics[i].generic.flags       |= QMF_HIGHLIGHT;
		s_championsmenu.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		trap_Cvar_Set( "champion", champion_names[i] );
	}
}

void ChampionsMenu_WPicEvent( void *ptr, int notification ) {
	static char *wep[] = { "machinegun", "shotgun", "plasmagun" };
	int i;

	if ( notification != QM_ACTIVATED )
		return;

	for ( i = 0; i < 3; i++ ) {
		// reset
		s_championsmenu.wpics[i].generic.flags		&= ~QMF_HIGHLIGHT;
		s_championsmenu.wpicbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected
	i = ((menucommon_s*)ptr)->id - ID_WEAPON;

	if ( i >= 0 && i < 3 ) {
		s_championsmenu.weapon = i;
		s_championsmenu.wpics[i].generic.flags       |= QMF_HIGHLIGHT;
		s_championsmenu.wpicbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		trap_Cvar_Set( "starting_weapon", wep[i] );
	}
}

static void ChampionsMenu_Draw( void ) {
	static float shade[] = { 0.0f, 0.0f, 0.0f, 0.5f };
	static char buf[32];
	UI_FillRect( -640, 280, 1920, 200, shade );
	Menu_Draw( &s_championsmenu.menu );	
	UI_DrawString( 320, 290, "CHOOSE CHAMPION", UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, color_white );
	Q_strncpyz( buf, champion_names[s_championsmenu.champion], sizeof( buf ) );
	Q_strupr( buf );
	UI_DrawString( 20 + 40 * ( s_championsmenu.champion - 1 ), 342, buf, UI_CENTER|UI_SMALLFONT, color_red );
	UI_DrawString( 320, 370, "CHOOSE WEAPON", UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, color_white );
}

/*
=================
ChampionsMenu_MenuInit
=================
*/
void ChampionsMenu_MenuInit( void ) {
	int		x, y;
	uiClientState_t	cs;
	char	info[MAX_INFO_STRING];
	int		i;
	char	buf[32];

	memset( &s_championsmenu, 0 ,sizeof(championsmenu_t) );

	ChampionsMenu_Cache();

	s_championsmenu.menu.wrapAround = qtrue;
	s_championsmenu.menu.fullscreen = qfalse;
	s_championsmenu.menu.callback = ChampionsMenu_Event;
	s_championsmenu.menu.draw = ChampionsMenu_Draw;

	y = 440;
	s_championsmenu.item_respawn.generic.type			= MTYPE_PTEXT;
	s_championsmenu.item_respawn.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_championsmenu.item_respawn.generic.x				= 320;
	s_championsmenu.item_respawn.generic.y				= y;
	s_championsmenu.item_respawn.generic.id				= ID_RESPAWN;
	s_championsmenu.item_respawn.generic.callback		= UI_ChampionsMenu_RespawnEvent; 
	s_championsmenu.item_respawn.string					= "RESPAWN";
	s_championsmenu.item_respawn.color					= color_red;
	s_championsmenu.item_respawn.style					= UI_CENTER|UI_SMALLFONT;

	Menu_AddItem( &s_championsmenu.menu, &s_championsmenu.item_respawn );

	x = 4;
	y = 310;

	trap_Cvar_VariableStringBuffer( "champion", buf, sizeof( buf ) );
	s_championsmenu.champion = ParseChampionName( buf );

	for ( i = 1; i < NUM_CHAMPIONS; i++ ) {
		Q_strncpyz( s_championsmenu.names[i], va( "models/players/%s/icon_default", champion_models[i] ), sizeof( s_championsmenu.names[i] ) );

		s_championsmenu.pics[i].generic.type	= MTYPE_BITMAP;
		s_championsmenu.pics[i].generic.name	= s_championsmenu.names[i];
		s_championsmenu.pics[i].generic.flags   = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
		s_championsmenu.pics[i].generic.x		= x;
		s_championsmenu.pics[i].generic.y		= y;
		s_championsmenu.pics[i].width  			= 32;
		s_championsmenu.pics[i].height  		= 32;
		s_championsmenu.pics[i].focuspic        = MODEL_SELECTED;
		s_championsmenu.pics[i].focuscolor      = colorRed;

		s_championsmenu.picbuttons[i].generic.type			= MTYPE_BITMAP;
		s_championsmenu.picbuttons[i].generic.flags			= QMF_LEFT_JUSTIFY|QMF_NODEFAULTINIT|QMF_PULSEIFFOCUS;
		s_championsmenu.picbuttons[i].generic.id			= ID_CHAMPION + i;
		s_championsmenu.picbuttons[i].generic.callback		= ChampionsMenu_PicEvent;
		s_championsmenu.picbuttons[i].generic.x    			= x - 8;
		s_championsmenu.picbuttons[i].generic.y				= y - 8;
		s_championsmenu.picbuttons[i].generic.left			= x;
		s_championsmenu.picbuttons[i].generic.top			= y;
		s_championsmenu.picbuttons[i].generic.right			= x + 32;
		s_championsmenu.picbuttons[i].generic.bottom		= y + 32;
		s_championsmenu.picbuttons[i].width  				= 64;
		s_championsmenu.picbuttons[i].height  				= 64;
		s_championsmenu.picbuttons[i].focuspic  			= MODEL_SELECT;
		s_championsmenu.picbuttons[i].focuscolor  			= colorRed;

		if ( champ_locked[i] ) {
			s_championsmenu.picbuttons[i].generic.flags |= QMF_INACTIVE;
		}

		if ( i == s_championsmenu.champion ) {
			s_championsmenu.pics[i].generic.flags       |= QMF_HIGHLIGHT;
			s_championsmenu.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		}

		x += 40;

		Menu_AddItem( &s_championsmenu.menu,	&s_championsmenu.pics[i] );
		Menu_AddItem( &s_championsmenu.menu,	&s_championsmenu.picbuttons[i] );
	}

	x = 220;
	y = 388;

	trap_Cvar_VariableStringBuffer( "starting_weapon", buf, sizeof( buf ) );
	s_championsmenu.weapon = ParseStartingWeapon( buf );

	for ( i = 0; i < ARRAY_LEN(weapons); i++ ) {
		s_championsmenu.wpics[i].generic.type		= MTYPE_BITMAP;
		s_championsmenu.wpics[i].generic.name		= weapon_names[i];
		s_championsmenu.wpics[i].generic.flags		= QMF_LEFT_JUSTIFY|QMF_INACTIVE;
		s_championsmenu.wpics[i].generic.x			= x;
		s_championsmenu.wpics[i].generic.y			= y;
		s_championsmenu.wpics[i].width				= 64;
		s_championsmenu.wpics[i].height				= 32;
		s_championsmenu.wpics[i].focuspic			= MODEL_SELECTED;
		s_championsmenu.wpics[i].focuscolor			= colorRed;

		s_championsmenu.wpicbuttons[i].generic.type			= MTYPE_BITMAP;
		s_championsmenu.wpicbuttons[i].generic.flags		= QMF_LEFT_JUSTIFY|QMF_NODEFAULTINIT|QMF_PULSEIFFOCUS;
		s_championsmenu.wpicbuttons[i].generic.id			= ID_WEAPON + i;
		s_championsmenu.wpicbuttons[i].generic.callback		= ChampionsMenu_WPicEvent;
		s_championsmenu.wpicbuttons[i].generic.x    		= x - 16;
		s_championsmenu.wpicbuttons[i].generic.y			= y - 8;
		s_championsmenu.wpicbuttons[i].generic.left			= x;
		s_championsmenu.wpicbuttons[i].generic.top			= y;
		s_championsmenu.wpicbuttons[i].generic.right		= x + 64;
		s_championsmenu.wpicbuttons[i].generic.bottom		= y + 32;
		s_championsmenu.wpicbuttons[i].width  				= 128;
		s_championsmenu.wpicbuttons[i].height  				= 64;
		s_championsmenu.wpicbuttons[i].focuspic  			= MODEL_SELECT;
		s_championsmenu.wpicbuttons[i].focuscolor  			= colorRed;

		if ( weapons[i] == s_championsmenu.weapon ) {
			s_championsmenu.wpics[i].generic.flags       |= QMF_HIGHLIGHT;
			s_championsmenu.wpicbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		}

		x += 68;
		Menu_AddItem( &s_championsmenu.menu, &s_championsmenu.wpics[i] );
		Menu_AddItem( &s_championsmenu.menu, &s_championsmenu.wpicbuttons[i] );
	}
}

/*
=================
ChampionsMenu_Cache
=================
*/
void ChampionsMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( MODEL_SELECT );
	trap_R_RegisterShaderNoMip( MODEL_SELECTED );
}


/*
=================
UI_ChampionsMenu
=================
*/
void UI_ChampionsMenu( void ) {
	// force as top level menu
	uis.menusp = 0;  

	// set menu cursor to a nice location
	uis.cursorx = 319;
	uis.cursory = 80;

	ChampionsMenu_MenuInit();
	UI_PushMenu( &s_championsmenu.menu );
}
