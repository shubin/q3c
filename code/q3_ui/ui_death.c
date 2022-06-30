
/*
=======================================================================

DEATH MENU

=======================================================================
*/


#include "ui_local.h"

#define ID_RESPAWN		10
#define ID_CHAMPIONS	20

typedef struct {
	menuframework_s	menu;
	menutext_s		item_respawn;
	menutext_s		item_champions;
	qboolean		exit_to_champions_menu;
} deathmenu_t;

static deathmenu_t	s_deathmenu;

void DeathMenu_Cache( void );

/*
=================
DeathMenu_Event
=================
*/
void DeathMenu_Event( void *ptr, void *next, int notification ) {
	if ( notification == QM_CLOSED ) {
		if ( next == NULL && !s_deathmenu.exit_to_champions_menu ) {
			trap_Cmd_ExecuteText( EXEC_NOW, "respawn" );
		}
	}
}

static void UI_DeathMenu_RespawnEvent( void* ptr, int notification ) {
	if (notification != QM_ACTIVATED) {
		return;
	}
	UI_SetActiveMenu( UIMENU_NONE );
}

static void UI_DeathMenu_ChampionsEvent( void* ptr, int notification ) {
	if (notification != QM_ACTIVATED) {
		return;
	}
	s_deathmenu.exit_to_champions_menu = qtrue;
	UI_SetActiveMenu( UIMENU_CHAMPIONS );
}

/*
=================
DeathMenu_MenuInit
=================
*/
void DeathMenu_MenuInit( void ) {
	int		y;
	uiClientState_t	cs;
	char	info[MAX_INFO_STRING];
	int		team;

	memset( &s_deathmenu, 0 ,sizeof(deathmenu_t) );

	DeathMenu_Cache();

	s_deathmenu.menu.wrapAround = qtrue;
	s_deathmenu.menu.fullscreen = qfalse;
	s_deathmenu.menu.callback = DeathMenu_Event;

	y = 360;
	s_deathmenu.item_champions.generic.type			= MTYPE_PTEXT;
	s_deathmenu.item_champions.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_deathmenu.item_champions.generic.x			= 320;
	s_deathmenu.item_champions.generic.y			= y;
	s_deathmenu.item_champions.generic.id			= ID_RESPAWN;
	s_deathmenu.item_champions.generic.callback		= UI_DeathMenu_ChampionsEvent; 
	s_deathmenu.item_champions.string				= "CHAMPIONS";
	s_deathmenu.item_champions.color				= color_red;
	s_deathmenu.item_champions.style				= UI_CENTER|UI_SMALLFONT;

	y += 32;
	s_deathmenu.item_respawn.generic.type			= MTYPE_PTEXT;
	s_deathmenu.item_respawn.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_deathmenu.item_respawn.generic.x				= 320;
	s_deathmenu.item_respawn.generic.y				= y;
	s_deathmenu.item_respawn.generic.id				= ID_RESPAWN;
	s_deathmenu.item_respawn.generic.callback		= UI_DeathMenu_RespawnEvent; 
	s_deathmenu.item_respawn.string					= "RESPAWN";
	s_deathmenu.item_respawn.color					= color_red;
	s_deathmenu.item_respawn.style					= UI_CENTER|UI_SMALLFONT;

	Menu_AddItem( &s_deathmenu.menu, &s_deathmenu.item_respawn );
	Menu_AddItem( &s_deathmenu.menu, &s_deathmenu.item_champions );
}

/*
=================
DeathMenu_Cache
=================
*/
void DeathMenu_Cache( void ) {
}


/*
=================
UI_DeathMenu
=================
*/
void UI_DeathMenu( void ) {
	// force as top level menu
	uis.menusp = 0;  

	// set menu cursor to a nice location
	uis.cursorx = 319;
	uis.cursory = 80;

	DeathMenu_MenuInit();
	UI_PushMenu( &s_deathmenu.menu );
}
