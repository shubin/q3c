extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "luasocket.h"
#include "../qcommon/q_shared.h"
}
#undef DotProduct
#include "LuaBridge.h"
#include <RmlUi/Core.h>
#include <RmlUi/Lua.h>

#include "ui_public.h"
#include "ui_local.h"

void UI_BindTraps( lua_State *L );
void UI_OpenStdLibs( lua_State *L );
void UI_OpenSocketLib( lua_State *L );

vmCvar_t ui_shell;
vmCvar_t ui_luadebug; // ui_luadebug cvar should be set to the "host:port" of the debugger server

static int checkload( lua_State *L, int stat, const char *filename ) {
	if ( luai_likely( stat ) ) {  /* module loaded successfully? */
		lua_pushstring( L, filename );  /* will be 2nd argument to module */
		return 2;  /* return open function and file name */
	} else
		return luaL_error( L, "error loading module '%s' from file '%s':\n\t%s",
							  lua_tostring( L, 1 ), filename, lua_tostring( L, -1 ) );
}

extern "C" char g_mobdebug_src_start;
extern "C" char g_mobdebug_src_end;

extern "C" int searcher_Quake( lua_State * L ) {
	const char *module;
	std::string qpath;
	long len;
	fileHandle_t fh;
	std::string source;
	const char *pSource;
	size_t szSource;

	module = luaL_checkstring( L, 1 );

	if ( strcmp( module, "mobdebug" ) ) {
		qpath = ui_shell.string;
		qpath += ".";
		qpath += module;
		std::replace( qpath.begin(), qpath.end(), '.', '/' );
		qpath += ".lua";
		len = trap_FS_FOpenFile( qpath.c_str(), &fh, FS_READ );

		if ( fh == 0 ) {
			qpath = module;
			std::replace( qpath.begin(), qpath.end(), '.', '/' );
			qpath += ".lua";
			len = trap_FS_FOpenFile( qpath.c_str(), &fh, FS_READ );
			if ( fh == 0 ) {
				return 1; // cannot open file
			}
		}

		source.resize( len );
		trap_FS_Read( source.data(), len, fh );
		trap_FS_FCloseFile( fh );
		pSource = source.data();
		szSource = source.size();
	} else {
		pSource = &g_mobdebug_src_start;
		szSource = &g_mobdebug_src_end - &g_mobdebug_src_start;
	}

	return checkload( L, luaL_loadbuffer( L, pSource, szSource, qpath.c_str() ) == LUA_OK, qpath.c_str() );
}

static int ErrorHandler( lua_State *L ) {
	const char *msg = lua_tostring( L, 1 );
	if ( msg == NULL ) {
		if ( luaL_callmeta( L, 1, "__tostring" ) && lua_type( L, -1 ) == LUA_TSTRING )
			return 1;
		else
			msg = lua_pushfstring( L, "(error object is a %s value)", luaL_typename( L, 1 ) );
	}
	luaL_traceback( L, L, msg, 1 );
	return 1;
}

static bool LuaCall( lua_State *L, int nargs, int nresults ) {
	int errfunc = -2 - nargs;
	lua_pushcfunction( L, ErrorHandler );
	lua_insert( L, errfunc );
	if ( lua_pcall( L, nargs, nresults, errfunc ) != LUA_OK ) {
		trap_Print( va( "^1Lua Error\n^1%s\n", lua_tostring( L, -1 ) ) );
		lua_pop( L, 2 );
		return false;
	}
	lua_remove( L, -1 - nresults );
	return true;
}

static bool LuaRequire( lua_State *L, const char *modname, int nresults = 0 ) {
	lua_getglobal( L, "require" );
	lua_pushstring( L, modname );
	return LuaCall( L, 1, nresults );
}

static void ParseHostPort( const char *hostport, std::string &host, std::string &port ) {
	const char *c, *p = NULL;
	for ( c = hostport; *c; c++ ) {
		if ( *c == ':' ) {
			p = c;
		}
	}
	if ( p == NULL ) {
		host = hostport;
		port = "";
	} else {
		host.assign( hostport, p );
		port.assign( p + 1, c );
	}
}

void UI_InitDebugger( lua_State *L ) {
	trap_Cvar_Register( &ui_luadebug, "ui_luadebug", "", CVAR_INIT );
	if ( ui_luadebug.string[0] ) {
		trap_Print( va( "^3*** Connecting debugger to %s ***\n", ui_luadebug.string ) );
		// register the socket module for mobdebug.lua
		int top = lua_gettop( L );
		UI_OpenStdLibs( L );
		UI_OpenSocketLib( L );;
		if ( LuaRequire( L, "mobdebug", 1 ) ) {
			int nargs = 0;
			std::string host, port;
			ParseHostPort( ui_luadebug.string, host, port );
			lua_getfield( L, -1, "start" );
			lua_pushlstring( L, host.data(), host.size() ); nargs++;
			if ( port.size() != 0 ) {
				lua_pushinteger( L, atoi( port.c_str() ) ); nargs++;
			}
			LuaCall( L, nargs, 0 );
		}
		lua_settop( L, top );
	}
}

void UI_InitLua( void ) {
	trap_Cvar_Register( &ui_shell, "ui_shell", "shell", CVAR_INIT );
	Rml::Lua::Initialise();

	lua_State *L = Rml::Lua::Interpreter::GetLuaState();	
	UI_InitDebugger( L );
	UI_BindTraps( L );
	LuaRequire( L, "main" );
}

void UI_ShutdownLua( void ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_Shutdown" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

void UI_KeyEvent( int key, int down ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_KeyEvent" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, key, (bool)down );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

void UI_MouseEvent( int dx, int dy ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_MouseEvent" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, dx, dy );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

void UI_Refresh( int realtime ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_Refresh" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, realtime );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

qboolean UI_IsFullscreen( void ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_IsFullscreen" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
			return qtrue;
		}
		return (qboolean)result[0].cast<bool>().value();;
	}
	return qtrue;
}

void UI_SetActiveMenu( uiMenuCommand_t menu ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_SetActiveMenu" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, (int)menu );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

qboolean UI_ConsoleCommand( int realTime ) {
	lua_State *L = Rml::Lua::Interpreter::GetLuaState();
	if ( L == NULL ) {
		return qfalse;
	}
	auto func = luabridge::getGlobal( L, "UI_ConsoleCommand" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, realTime );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
			return qfalse;
		}
		 return (qboolean)result[0].cast<bool>().value();;
	}
	return qfalse;
}

void UI_DrawConnectScreen( qboolean overlay ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_DrawConnectScreen" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, (bool)overlay );
		if ( result.hasFailed() ) { 
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

void UI_CvarChangedLua( void ) {
	std::string var_name;
	var_name.resize( 128 );
	trap_Argv( 0, var_name.data(), var_name.size() );
	var_name.resize( strlen( var_name.data() ) );
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_CvarChanged" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, var_name );
		if ( result.hasFailed() ) { 
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}

void UI_SetMousePointer( const char *pointer ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_SetMousePointer" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func, pointer );
		if ( result.hasFailed() ) {
			trap_Print( va( "^1" __FUNCTION__ ": % s\n", result.errorMessage().c_str() ) );
		}
	}
}
