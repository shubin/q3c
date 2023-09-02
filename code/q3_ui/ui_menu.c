/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
/*
=======================================================================

MAIN MENU

=======================================================================
*/


#include "ui_local.h"


#if !defined( QC )
#define ID_SINGLEPLAYER			10
#define ID_MULTIPLAYER			11
#endif
#if defined( QC )
#define ID_HOST_GAME			10
#define ID_JOIN_GAME			11
#define ID_CHAMPIONS			14
#endif
#define ID_SETUP				12
#define ID_DEMOS				13
#if defined( QC )
#define ID_CREDITS				15
#endif // QC
#if !defined( QC )
#define ID_CINEMATICS			14
#define ID_TEAMARENA			15
#define ID_MODS					16
#endif // QC
#define ID_EXIT					17

#if defined( QC )
#define MAIN_BANNER_MODEL				"models/mapobjects/banner/banner_champions.md3"
#define MAIN_BANNER_SHADER				"menubanner"
#else
#define MAIN_BANNER_MODEL				"models/mapobjects/banner/banner5.md3"
#endif
#define MAIN_MENU_VERTICAL_SPACING		34


typedef struct {
	menuframework_s	menu;

#if !defined( QC )
	menutext_s		singleplayer;
	menutext_s		multiplayer;
#endif
#if defined( QC )
	menutext_s		hostgame;
	menutext_s		joingame;
	menutext_s		champions;
#endif
	menutext_s		setup;
	menutext_s		demos;
#if !defined( QC )
	menutext_s		cinematics;
	menutext_s		teamArena;
	menutext_s		mods;
#endif
#if defined( QC )
	menutext_s		credits;
#endif // QC
	menutext_s		exit;

#if defined( QC )
	qhandle_t		bannerShader;
#else // QC
	qhandle_t		bannerModel;
#endif // QC
} mainmenu_t;


static mainmenu_t s_main;

typedef struct {
	menuframework_s menu;	
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;

/*
=================
MainMenu_ExitAction
=================
*/
static void MainMenu_ExitAction( qboolean result ) {
	if( !result ) {
		return;
	}
#if defined( QC )
	trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
#else // QC
	UI_PopMenu();
	UI_CreditMenu();
#endif // QC
}



/*
=================
Main_MenuEvent
=================
*/
void Main_MenuEvent (void* ptr, int event) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
#if !defined( QC )
	case ID_SINGLEPLAYER:
		UI_SPLevelMenu();
		break;

	case ID_MULTIPLAYER:
		UI_ArenaServersMenu();
		break;
#endif
#if defined( QC )
	case ID_HOST_GAME:
		UI_StartServerMenu( qtrue );
		break;

	case ID_JOIN_GAME:
		UI_ArenaServersMenu();
		break;
#endif
	case ID_SETUP:
		UI_SetupMenu();
		break;

	case ID_DEMOS:
		UI_DemosMenu();
		break;
#if defined( QC )
	case ID_CHAMPIONS:
		UI_ChampionsMenu( qfalse );
		break;
#endif
#if !defined( QC )
	case ID_CINEMATICS:
		UI_CinematicsMenu();
		break;

	case ID_MODS:
		UI_ModsMenu();
		break;

	case ID_TEAMARENA:
		trap_Cvar_Set( "fs_game", BASETA);
		trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		break;
#endif

#if defined( QC )
	case ID_CREDITS:
		UI_CreditMenu();
		break;
#endif // QC

	case ID_EXIT:
		UI_ConfirmMenu( "EXIT GAME?", 0, MainMenu_ExitAction );
		break;
	}
}


/*
===============
MainMenu_Cache
===============
*/
void MainMenu_Cache( void ) {
#if defined( QC )
	s_main.bannerShader = trap_R_RegisterShaderNoMip( MAIN_BANNER_SHADER );
#else // QC
	s_main.bannerModel = trap_R_RegisterModel( MAIN_BANNER_MODEL );
#endif // QC
}

sfxHandle_t ErrorMessage_Key(int key)
{
	trap_Cvar_Set( "com_errorMessage", "" );
	UI_MainMenu();
	return (menu_null_sound);
}

/*
===============
Main_MenuDraw
TTimo: this function is common to the main menu and errorMessage menu
===============
*/

static void Main_MenuDraw( void ) {
#if !defined( QC )
	refdef_t		refdef;
	refEntity_t		ent;
#endif // QC
#if !defined( QC )
	vec3_t			origin;
	vec3_t			angles;
	float			adjust;
	float			x, y, w, h;
#endif // QC
	vec4_t			color = {0.5, 0, 0, 1};

#if !defined( QC )
	// setup the refdef

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	x = 0;
	y = 0;
	w = 640;
	h = 120;
	UI_AdjustFrom640( &x, &y, &w, &h );
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	adjust = 0; // JDC: Kenneth asked me to stop this 1.0 * sin( (float)uis.realtime / 1000 );
	refdef.fov_x = 60 + adjust;
	refdef.fov_y = 19.6875 + adjust;

	refdef.time = uis.realtime;

	origin[0] = 300;
	origin[1] = 0;
	origin[2] = -32;

	trap_R_ClearScene();

	// add the model

	memset( &ent, 0, sizeof(ent) );

	adjust = 5.0 * sin( (float)uis.realtime / 5000 );
	VectorSet( angles, 0, 180 + adjust, 0 );
	AnglesToAxis( angles, ent.axis );
	ent.hModel = s_main.bannerModel;
	VectorCopy( origin, ent.origin );
	VectorCopy( origin, ent.lightingOrigin );
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
	VectorCopy( ent.origin, ent.oldorigin );

	trap_R_AddRefEntityToScene( &ent );

	trap_R_RenderScene( &refdef );
#else // QC
	UI_DrawHandlePic( 0, 30, 640, 120, s_main.bannerShader );
#endif // QC
	
	if (strlen(s_errorMessage.errorMessage))
	{
		UI_DrawProportionalString_AutoWrapped( 320, 192, 600, 20, s_errorMessage.errorMessage, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );
	}
	else
	{
		// standard menu drawing
		Menu_Draw( &s_main.menu );		
	}

#if defined( QC )
		UI_DrawString( 320, 450, PRODUCT_NAME " Ver. " PRODUCT_VERSION " " PRODUCT_DATE, UI_CENTER|UI_SMALLFONT, color );
#else
	if (uis.demoversion) {
		UI_DrawProportionalString( 320, 372, "DEMO      FOR MATURE AUDIENCES      DEMO", UI_CENTER|UI_SMALLFONT, color );
		UI_DrawString( 320, 400, "Quake III Arena(c) 1999-2000, Id Software, Inc.  All Rights Reserved", UI_CENTER|UI_SMALLFONT, color );
	} else {
		UI_DrawString( 320, 450, "Quake III Arena(c) 1999-2000, Id Software, Inc.  All Rights Reserved", UI_CENTER|UI_SMALLFONT, color );
	}
#endif
}


#if !defined( QC )
/*
===============
UI_TeamArenaExists
===============
*/
static qboolean UI_TeamArenaExists( void ) {
	int		numdirs;
	char	dirlist[2048];
	char	*dirptr;
  char  *descptr;
	int		i;
	int		dirlen;

	numdirs = trap_FS_GetFileList( "$modlist", "", dirlist, sizeof(dirlist) );
	dirptr  = dirlist;
	for( i = 0; i < numdirs; i++ ) {
		dirlen = strlen( dirptr ) + 1;
    descptr = dirptr + dirlen;
		if (Q_stricmp(dirptr, BASETA) == 0) {
			return qtrue;
		}
    dirptr += dirlen + strlen(descptr) + 1;
	}
	return qfalse;
}
#endif

/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu( void ) {
	int		y;
	qboolean teamArena = qfalse;
	int		style = UI_CENTER | UI_DROPSHADOW;

	trap_Cvar_Set( "sv_killserver", "1" );

#if !defined( QC )
	if( !uis.demoversion && !ui_cdkeychecked.integer ) {
		char	key[17];

		trap_GetCDKey( key, sizeof(key) );
		if( trap_VerifyCDKey( key, NULL ) == qfalse ) {
			UI_CDKeyMenu();
			return;
		}
	}
#endif
	
	memset( &s_main, 0 ,sizeof(mainmenu_t) );
	memset( &s_errorMessage, 0 ,sizeof(errorMessage_t) );

	// com_errorMessage would need that too
	MainMenu_Cache();
	
	trap_Cvar_VariableStringBuffer( "com_errorMessage", s_errorMessage.errorMessage, sizeof(s_errorMessage.errorMessage) );
	if (strlen(s_errorMessage.errorMessage))
	{	
		s_errorMessage.menu.draw = Main_MenuDraw;
		s_errorMessage.menu.key = ErrorMessage_Key;
		s_errorMessage.menu.fullscreen = qtrue;
		s_errorMessage.menu.wrapAround = qtrue;
		s_errorMessage.menu.showlogo = qtrue;		

		trap_Key_SetCatcher( KEYCATCH_UI );
		uis.menusp = 0;
		UI_PushMenu ( &s_errorMessage.menu );
		
		return;
	}

	s_main.menu.draw = Main_MenuDraw;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;

#if defined( QC )
	y = 180;
	s_main.hostgame.generic.type		= MTYPE_PTEXT;
	s_main.hostgame.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.hostgame.generic.x			= 320;
	s_main.hostgame.generic.y			= y;
	s_main.hostgame.generic.id			= ID_HOST_GAME;
	s_main.hostgame.generic.callback	= Main_MenuEvent; 
	s_main.hostgame.string				= "START GAME";
	s_main.hostgame.color				= color_red;
	s_main.hostgame.style				= style;

	y += MAIN_MENU_VERTICAL_SPACING;

	s_main.joingame.generic.type		= MTYPE_PTEXT;
	s_main.joingame.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.joingame.generic.x			= 320;
	s_main.joingame.generic.y			= y;
	s_main.joingame.generic.id			= ID_JOIN_GAME;
	s_main.joingame.generic.callback	= Main_MenuEvent; 
	s_main.joingame.string				= "JOIN GAME";
	s_main.joingame.color				= color_red;
	s_main.joingame.style				= style;

	y += MAIN_MENU_VERTICAL_SPACING;

	s_main.champions.generic.type		= MTYPE_PTEXT;
	s_main.champions.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.champions.generic.x			= 320;
	s_main.champions.generic.y			= y;
	s_main.champions.generic.id			= ID_CHAMPIONS;
	s_main.champions.generic.callback	= Main_MenuEvent; 
	s_main.champions.string				= "CHAMPIONS";
	s_main.champions.color				= color_red;
	s_main.champions.style				= style;

	y += MAIN_MENU_VERTICAL_SPACING;
#else
	y = 134;
	s_main.singleplayer.generic.type		= MTYPE_PTEXT;
	s_main.singleplayer.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.singleplayer.generic.x			= 320;
	s_main.singleplayer.generic.y			= y;
	s_main.singleplayer.generic.id			= ID_SINGLEPLAYER;
	s_main.singleplayer.generic.callback	= Main_MenuEvent; 
	s_main.singleplayer.string				= "SINGLE PLAYER";
	s_main.singleplayer.color				= color_red;
	s_main.singleplayer.style				= style;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.multiplayer.generic.type			= MTYPE_PTEXT;
	s_main.multiplayer.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.multiplayer.generic.x			= 320;
	s_main.multiplayer.generic.y			= y;
	s_main.multiplayer.generic.id			= ID_MULTIPLAYER;
	s_main.multiplayer.generic.callback		= Main_MenuEvent; 
	s_main.multiplayer.string				= "MULTIPLAYER";
	s_main.multiplayer.color				= color_red;
	s_main.multiplayer.style				= style;

	y += MAIN_MENU_VERTICAL_SPACING;
#endif
	s_main.setup.generic.type				= MTYPE_PTEXT;
	s_main.setup.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.setup.generic.x					= 320;
	s_main.setup.generic.y					= y;
	s_main.setup.generic.id					= ID_SETUP;
	s_main.setup.generic.callback			= Main_MenuEvent; 
	s_main.setup.string						= "SETUP";
	s_main.setup.color						= color_red;
	s_main.setup.style						= style;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.demos.generic.type				= MTYPE_PTEXT;
	s_main.demos.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.demos.generic.x					= 320;
	s_main.demos.generic.y					= y;
	s_main.demos.generic.id					= ID_DEMOS;
	s_main.demos.generic.callback			= Main_MenuEvent; 
	s_main.demos.string						= "DEMOS";
	s_main.demos.color						= color_red;
	s_main.demos.style						= style;

#if !defined( QC )
	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.cinematics.generic.type			= MTYPE_PTEXT;
	s_main.cinematics.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.cinematics.generic.x				= 320;
	s_main.cinematics.generic.y				= y;
	s_main.cinematics.generic.id			= ID_CINEMATICS;
	s_main.cinematics.generic.callback		= Main_MenuEvent; 
	s_main.cinematics.string				= "CINEMATICS";
	s_main.cinematics.color					= color_red;
	s_main.cinematics.style					= style;

	if ( !uis.demoversion && UI_TeamArenaExists() ) {
		teamArena = qtrue;
		y += MAIN_MENU_VERTICAL_SPACING;
		s_main.teamArena.generic.type			= MTYPE_PTEXT;
		s_main.teamArena.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
		s_main.teamArena.generic.x				= 320;
		s_main.teamArena.generic.y				= y;
		s_main.teamArena.generic.id				= ID_TEAMARENA;
		s_main.teamArena.generic.callback		= Main_MenuEvent; 
		s_main.teamArena.string					= "TEAM ARENA";
		s_main.teamArena.color					= color_red;
		s_main.teamArena.style					= style;
	}

	if ( !uis.demoversion ) {
		y += MAIN_MENU_VERTICAL_SPACING;
		s_main.mods.generic.type			= MTYPE_PTEXT;
		s_main.mods.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
		s_main.mods.generic.x				= 320;
		s_main.mods.generic.y				= y;
		s_main.mods.generic.id				= ID_MODS;
		s_main.mods.generic.callback		= Main_MenuEvent; 
		s_main.mods.string					= "MODS";
		s_main.mods.color					= color_red;
		s_main.mods.style					= style;
	}
#endif
#if defined( QC )
	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.credits.generic.type = MTYPE_PTEXT;
	s_main.credits.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_main.credits.generic.x = 320;
	s_main.credits.generic.y = y;
	s_main.credits.generic.id = ID_CREDITS;
	s_main.credits.generic.callback = Main_MenuEvent;
	s_main.credits.string = "CREDITS";
	s_main.credits.color = color_red;
	s_main.credits.style = style;
#endif // QC

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.exit.generic.type				= MTYPE_PTEXT;
	s_main.exit.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.exit.generic.x					= 320;
	s_main.exit.generic.y					= y;
	s_main.exit.generic.id					= ID_EXIT;
	s_main.exit.generic.callback			= Main_MenuEvent; 
	s_main.exit.string						= "EXIT";
	s_main.exit.color						= color_red;
	s_main.exit.style						= style;

#if defined( QC )
	Menu_AddItem( &s_main.menu,	&s_main.hostgame);
	Menu_AddItem( &s_main.menu,	&s_main.joingame );
	Menu_AddItem( &s_main.menu, &s_main.champions );
	Menu_AddItem( &s_main.menu,	&s_main.setup );
	Menu_AddItem( &s_main.menu,	&s_main.demos );
	Menu_AddItem( &s_main.menu, &s_main.credits );
	Menu_AddItem( &s_main.menu,	&s_main.exit );             
#else
	Menu_AddItem( &s_main.menu,	&s_main.multiplayer );
	Menu_AddItem( &s_main.menu,	&s_main.singleplayer );
	Menu_AddItem( &s_main.menu,	&s_main.multiplayer );
	Menu_AddItem( &s_main.menu,	&s_main.setup );
	Menu_AddItem( &s_main.menu,	&s_main.demos );
	Menu_AddItem( &s_main.menu,	&s_main.cinematics );
	if (teamArena) {
		Menu_AddItem( &s_main.menu,	&s_main.teamArena );
	}
	if ( !uis.demoversion ) {
		Menu_AddItem( &s_main.menu,	&s_main.mods );
	}
	Menu_AddItem( &s_main.menu,	&s_main.exit );             
#endif

	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );
		
}
