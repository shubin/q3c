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

static int lua_os_getenv( lua_State *L ) {
	const char *var_name = luaL_checkstring( L, 1 );
	if ( !strcmp( var_name, "WINDIR" ) ) {
		lua_pushstring( L, "C:\\WINDOWS" );
		return 1;
	}
	if ( !strcmp( var_name, "MOBDEBUG_PORT" ) ) {
		lua_pushstring( L, "8172" );
		return 1;
	}
	return 0;
}

void UI_OpenStdLibs( lua_State *L ) {
	// create os and io libs to make the mobdebug script happy
	lua_newtable( L );
	lua_pushcclosure( L, lua_os_getenv, 0 );
	lua_setfield( L, -2, "getenv" );
	lua_setglobal( L, "os" );
	lua_newtable( L );
	lua_setglobal( L, "io" );
	// add them to the loaded module registry
	luaL_getsubtable( L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE );
	lua_getglobal( L, "os" );
	lua_setfield( L, -2, "os" );
	lua_getglobal( L, "io" );
	lua_setfield( L, -2, "io" );
	lua_pop( L, 1 );
}

void UI_OpenSocketLib( lua_State *L ) {
	// open the socket lib
	luaopen_socket_core( L );
	lua_setglobal( L, "socket" );
	// add it to the loaded module registry so "require" would return it when needed
	luaL_getsubtable( L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE );
	lua_getglobal( L, "socket" );
	lua_setfield( L, -2, "socket" );
	lua_pop( L, 1 );
}
