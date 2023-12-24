extern "C" {
  #include "lua.h"
  #include "lauxlib.h"
}
#include "LuaBridge.h"
#include <RmlUi/Core.h>
#include <RmlUi/Lua.h>

#include "../qcommon/q_shared.h"
#include "ui_public.h"
#include "ui_local.h"

struct QVec3 {
	vec3_t vec;
	QVec3() { vec[0] = vec[1] = vec[2] = 0.0f; }
	QVec3( float x, float y, float z ) { vec[0] = x; vec[1] = y; vec[2] = z; }
};

struct QRGBA {
	byte rgba[4];
	QRGBA() { rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255; }
	QRGBA( int r, int g, int b ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = 255; }
	QRGBA( int r, int g, int b, int a ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = a; }
};

struct QColor {
	float rgba[4];
	QColor() { rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0f; }
	QColor( float r, float g, float b ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = 1.0f; }
	QColor( float r, float g, float b, float a ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = a; }
};

void UI_BindLua( lua_State *L ) {
	luabridge::getGlobalNamespace( L )
		.beginClass<QVec3>( "vec3_t" )
			.addConstructor<void(*)()>()
			.addConstructor<void(*)( float, float, float )>()
			.addProperty( "x" ,
				+[] ( const QVec3 *qv ) { return qv->vec[0]; },
				+[] ( QVec3 *qv, float value ) { qv->vec[0] = value; }
				)
			.addProperty( "y" ,
				+[] ( const QVec3 *qv ) { return qv->vec[1]; },
				+[] ( QVec3 *qv, float value ) { qv->vec[1] = value; }
				)
			.addProperty( "z" ,
				+[] ( const QVec3 *qv ) { return qv->vec[2]; },
				+[] ( QVec3 *qv, float value ) { qv->vec[2] = value; }
				)
		.endClass()
		.beginClass<QColor>( "color_t" )
			.addConstructor<void(*)()>()
			.addConstructor<void(*)( float, float, float)>()
			.addConstructor<void(*)( float, float, float, float )>()
		.endClass()
		.beginClass<glconfig_t>( "glconfig_t" )
			.addConstructor<void(*)()>()
			// TODO: implement all the fields
			.addProperty( "vidWidth", &glconfig_t::vidWidth )
			.addProperty( "vidHeight", &glconfig_t::vidHeight )
		.endClass()
		.beginClass<vmCvar_t>( "vmCvar_t" )
			.addConstructor<void(*)()>()
			.addProperty( "handle", &vmCvar_t::handle )
			.addProperty( "modificationCount", &vmCvar_t::modificationCount )
			.addProperty( "value", &vmCvar_t::value )
			.addProperty( "integer", &vmCvar_t::integer )
			.addProperty( "string",
				+[] ( const vmCvar_t *cvar ) { return cvar->string; },
				+[] ( vmCvar_t *cvar, const char *value ) { strncpy( cvar->string, value, MAX_CVAR_VALUE_STRING ); }
				)
		.endClass()
		.beginClass<refEntity_t>( "refEntity_t" )
			.addConstructor<void(*)()>()
			// TODO: implement all the fields
			.addProperty( "reType", &refEntity_t::reType )
		.endClass()
		.beginClass<refdef_t>( "refdef_t" )
			.addConstructor<void(*)()>()
			// TODO: implement all the fields
			.addProperty( "x", &refdef_t::x )
			.addProperty( "y", &refdef_t::y )
			.addProperty( "width", &refdef_t::width )
			.addProperty( "height", &refdef_t::height )
		.endClass()
		.addFunction( "trap_Print", trap_Print )
		.addFunction( "trap_Error", trap_Error )
		.addFunction( "trap_Milliseconds", trap_Milliseconds )
		.addFunction( "trap_Cvar_Register", trap_Cvar_Register )
		.addFunction( "trap_Cvar_Update", trap_Cvar_Update )
		.addFunction( "trap_Cvar_Set", trap_Cvar_Set )
		.addFunction( "trap_Cvar_VariableValue", trap_Cvar_VariableValue )
		.addFunction( "trap_Cvar_VariableString",
			+[]( const char *var_name ) {
				std::string result;
				result.resize( MAX_CVAR_VALUE_STRING );
				trap_Cvar_VariableStringBuffer( var_name, result.data(), result.size() );
				return result;
			})
		.addFunction( "trap_Cvar_SetValue", trap_Cvar_SetValue )
		.addFunction( "trap_Cvar_Reset", trap_Cvar_Reset )
		.addFunction( "trap_Cvar_Create", trap_Cvar_Create )
		.addFunction( "trap_Cvar_InfoString",
			+[]( int bit ) {
				std::string result;
				result.resize( MAX_INFO_STRING );
				trap_Cvar_InfoStringBuffer( bit, result.data(), result.size() );
				return result;
			})
		.addFunction( "trap_Argc", trap_Argc )
		.addFunction( "trap_Argv",
			+[]( int n ) {
				std::string result;
				result.resize( 1024 );
				trap_Argv( n, result.data(), result.size() );
				return result;
			})
		.addFunction( "trap_Cmd_ExecuteText", trap_Cmd_ExecuteText )
		// file api skipped
		.addFunction( "trap_R_RegisterModel", trap_R_RegisterModel )
		.addFunction( "trap_R_RegisterSkin", trap_R_RegisterSkin )
		// register font skipped
		.addFunction( "trap_R_RegisterShaderNoMip", trap_R_RegisterShaderNoMip )
		.addFunction( "trap_R_ClearScene", trap_R_ClearScene )
		.addFunction( "trap_R_AddRefEntityToScene", trap_R_AddRefEntityToScene )
		.addFunction( "trap_R_RenderScene", trap_R_RenderScene )
		.addFunction( "trap_R_SetColor", +[] () { trap_R_SetColor( NULL ); } )
		.addFunction( "trap_R_SetColor", +[] ( const QColor *color ) { trap_R_SetColor( color->rgba ); } )
		.addFunction( "trap_R_DrawStretchPic", trap_R_DrawStretchPic )
		// ...
		.addFunction( "trap_GetGlconfig", trap_GetGlconfig )
	;
}

void UI_InitLua( void ) {
	Rml::Lua::Initialise();
	UI_BindLua( Rml::Lua::Interpreter::GetLuaState() );
	Rml::Lua::Interpreter::LoadFile( "ui/main.lua" );
}

void UI_ShutdownLua( void ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_Shutdown" );
	if ( func.isCallable() ) {
		luabridge::call( func );
	}
}

void UI_KeyEvent( int key, int down ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_KeyEvent" );
	if ( func.isCallable() ) {
		luabridge::call( func, key, down );
	}
}

void UI_MouseEvent( int dx, int dy ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_MouseEvent" );
	if ( func.isCallable() ) {
		luabridge::call( func, dx, dy );
	}
}

void UI_Refresh( int realtime ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_Refresh" );
	if ( func.isCallable() ) {
		luabridge::call( func, realtime );
	}
}

qboolean UI_IsFullscreen( void ) {
	auto func = luabridge::getGlobal( Rml::Lua::Interpreter::GetLuaState(), "UI_IsFullscreen" );
	if ( func.isCallable() ) {
		auto result = luabridge::call( func );
		if ( result.hasFailed() ) {
			return qtrue;
		}
		return (qboolean)(bool)result;
	}
	return qtrue;
}

void UI_SetActiveMenu( uiMenuCommand_t menu ) {
	if ( menu == UIMENU_NONE ) {
		trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
	} else {
		trap_Key_SetCatcher( KEYCATCH_UI );
	}
}

qboolean UI_ConsoleCommand( int realTime ) {
	return qfalse;
}

void UI_DrawConnectScreen( qboolean overlay ) {
}
