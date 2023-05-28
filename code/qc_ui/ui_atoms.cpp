#include "ui_local.h"

static lua_State *lstate = NULL;
mspace ui_mspace = NULL;

static void *Lua_Alloc( void *msp, void *ptr, size_t osize, size_t nsize ) {
	if ( nsize == 0 ) {
		mspace_free( msp, ptr );
		return NULL;
	}
	else {
		return mspace_realloc( msp, ptr, nsize );
	}
}

static void Lua_Panic( const char *message ) {
	static char buf[1024];

	strcat( buf, message );
	strcat( buf, lua_tostring( lstate, -1 ) );
	trap_Error( buf );
}

static void Lua_DoFile( const char *filename ) {
	fileHandle_t f;
	int len;
	char *code;

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		trap_Error( S_COLOR_RED "Could not open the UI script file\n" );
		return;
	}
	code = ( char * )mspace_malloc( ui_mspace, len + 1 );
	trap_FS_Read( code, len, f );
	code[len] = 0;
	trap_FS_FCloseFile( f );

	int r = luaL_loadbuffer( lstate, code, len, filename );
	mspace_free( ui_mspace, code );
	switch ( r ) {
	case LUA_ERRSYNTAX:
		Lua_Panic( "Syntax error: " );
		break;;
	case LUA_ERRMEM:
		Lua_Panic( "Memory error: " );
		break;;
	case LUA_ERRFILE:
		Lua_Panic( "Error: " );
		break;;
	};
	switch ( lua_pcall( lstate, 0, 0, 0 ) ) {
	case LUA_ERRRUN:
		Lua_Panic( "Runtime error: " );
		break;
	case LUA_ERRMEM:
		Lua_Panic( "Memory error: " );
		break;
	case LUA_ERRERR:
		Lua_Panic( "Error: " );
		break;
	}
}

static void Lua_DoFiles( const char *path, const char *ext ) {
	const int maxfiles = 65536;
	static char buf[1024];
	char *files = (char*)mspace_malloc( ui_mspace, maxfiles );
	int nFiles = trap_FS_GetFileList( path, ext, files, maxfiles );
	char *file = files;
	for ( int i = 0; i < nFiles; i++ ) {
		snprintf( buf, ARRAY_LEN( buf ), "%s/%s", path, file );
		Lua_DoFile( buf );
		file += strlen( file ) + 1;
	}
}

static void Lua_Init( void ) {
	lstate = lua_newstate( Lua_Alloc, ui_mspace );

	luaopen_base( lstate );
	lua_newtable( lstate ); luaopen_math( lstate ); lua_setglobal( lstate, LUA_MATHLIBNAME );
	lua_newtable( lstate ); luaopen_string( lstate ); lua_setglobal( lstate, LUA_STRLIBNAME );
	lua_newtable( lstate ); luaopen_table( lstate ); lua_setglobal( lstate, LUA_TABLIBNAME );
	luaopen_trap( lstate );
}

static void Lua_Shutdown( void ) {
	lua_close( lstate );
	lstate = NULL;
}

void UI_Init( void ) {
	ui_mspace = create_mspace( 0, 0 );
	Lua_Init();
	UI_InitRML( lstate );

	Lua_DoFiles( "ui/scripts", "lua" );
	Lua_DoFiles( "ui/scripts", "luac" );

	lua_getglobal( lstate, "UI_Init" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	if ( lua_pcall( lstate, 0, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_Init" );
	}
}

void UI_Shutdown( void ) {
	lua_getglobal( lstate, "UI_Shutown" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
	} else {
		if ( lua_pcall( lstate, 0, 0, 0 ) != 0 ) {
			trap_Print( S_COLOR_RED "Error executing UI_Shutdown\n" );
		}
	}
	UI_ShutdownRML();
	Lua_Shutdown();
	destroy_mspace( ui_mspace );
	ui_mspace = NULL;
}

void UI_KeyEvent( int key, int down ) {
	lua_getglobal( lstate, "UI_KeyEvent" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	lua_pushinteger( lstate, key );
	lua_pushinteger( lstate, down );
	if ( lua_pcall( lstate, 2, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_KeyEvent" );
	}
}

void UI_MouseEvent( int dx, int dy ) {
	lua_getglobal( lstate, "UI_MouseEvent" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	lua_pushinteger( lstate, dx );
	lua_pushinteger( lstate, dy );
	if ( lua_pcall( lstate, 2, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_MouseEvent" );
	}
}

void UI_Refresh( int realtime ) {
	trap_R_SaveScissor( qtrue );

	lua_getglobal( lstate, "UI_Refresh" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	lua_pushinteger( lstate, realtime );
	if ( lua_pcall( lstate, 1, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_Refresh" );
	}

	trap_R_SaveScissor( qfalse );
	trap_R_SetMatrix( NULL );
}

qboolean UI_IsFullscreen( void ) {
	lua_getglobal( lstate, "UI_IsFullscreen" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return qtrue;
	}
	if ( lua_pcall( lstate, 0, 1, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_IsFullscreen" );
	}
	if ( !lua_isboolean( lstate, -1 ) ) {
		Lua_Panic( "UI_IsFullscreen returned non-booolean value" );
	}
	qboolean result = (qboolean)lua_toboolean( lstate, -1 );
	lua_pop( lstate, 1 );
	return result;
}

void UI_SetActiveMenu( uiMenuCommand_t menu ) {
	lua_getglobal( lstate, "UI_SetActiveMenu" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	lua_pushinteger( lstate, (int)menu );
	if ( lua_pcall( lstate, 1, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_SetActiveMenu" );
	}
}

qboolean UI_ConsoleCommand( int realTime ) {
	if ( lstate == NULL ) {
		return qfalse;
	}

	lua_getglobal( lstate, "UI_ConsoleCommand" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return qtrue;
	}
	lua_pushinteger( lstate, realTime );
	if ( lua_pcall( lstate, 1, 1, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_ConsoleCommand" );
	}
	if ( !lua_isboolean( lstate, -1 ) ) {
		Lua_Panic( "UI_ConsoleCommand returned non-booolean value" );
		return qfalse;
	}
	qboolean result = ( qboolean )lua_toboolean( lstate, -1 );
	lua_pop( lstate, 1 );
	return result;
}

void UI_DrawConnectScreen( qboolean overlay ) {
	lua_getglobal( lstate, "UI_DrawConnectScreen" );
	if ( !lua_isfunction( lstate, -1 ) ) {
		lua_pop( lstate, -1 );
		return;
	}
	lua_pushboolean( lstate, overlay );
	if ( lua_pcall( lstate, 1, 0, 0 ) != 0 ) {
		Lua_Panic( "Error executing UI_DrawConnectScreen" );
	}
}
