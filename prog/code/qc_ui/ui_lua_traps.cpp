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

#define Vector2Copy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1])

struct QVec2 {
	vec2_t vec;
	QVec2() { vec[0] = vec[1] = 0.0f; }
	QVec2( float x, float y ) { vec[0] = x; vec[1] = y; }
	QVec2( const vec2_t a ) { Vector2Copy( a, vec ); }
};

struct QVec3 {
	vec3_t vec;
	QVec3() { vec[0] = vec[1] = vec[2] = 0.0f; }
	QVec3( float x, float y, float z ) { vec[0] = x; vec[1] = y; vec[2] = z; }
	QVec3( const vec3_t a ) { VectorCopy( a, vec ); }
};

struct QRGBA {
	byte rgba[4];
	QRGBA() { rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255; }
	QRGBA( int r, int g, int b ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = 255; }
	QRGBA( int r, int g, int b, int a ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = a; }
	QRGBA( const byte *v ) { Vector4Copy( v, rgba ); }
};

struct QColor {
	float rgba[4];
	QColor() { rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0f; }
	QColor( float r, float g, float b ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = 1.0f; }
	QColor( float r, float g, float b, float a ) { rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = a; }
	QColor( const float *v ) { Vector4Copy( v, rgba ); }
};

static int lua_readfile( lua_State *L ) {	
	long len;
	std::string filecontents;
	fileHandle_t fh;
	const char *qpath;

	qpath = luaL_checkstring( L, 1 );
	len = trap_FS_FOpenFile( qpath, &fh, FS_READ );
	if ( fh == 0 ) {
		return 0;
	}
	filecontents.resize( len );
	trap_FS_Read( filecontents.data(), len, fh );
	trap_FS_FCloseFile( fh );
	lua_pushlstring( L, filecontents.data(), filecontents.size() );
	return 1;
}

void UI_BindTraps( lua_State *L ) {
	if ( ui_luadebug.string[0] ) {
		luabridge::getGlobalNamespace( L )
			.addFunction( "exec",
				+[]( const std::string &str ) {
				return Rml::Lua::Interpreter::DoString( str, "<exec>" );
				}
			);
	}
	luabridge::getGlobalNamespace( L )
		.addFunction( "readfile", lua_readfile )
		.addFunction( "MapKey", UI_MapKey )
		.beginClass<QVec2>( "vec2_t" )
			.addConstructor( []( void *ptr ) { return new ( ptr ) QVec2(); } )
			.addStaticFunction( "new", []() { return QVec2(); } )
			.addStaticFunction( "new", []( float x, float y ) { return QVec2( x, y ); } )
			.addProperty( "x" , +[] ( const QVec2 *qv ) { return qv->vec[0]; }, +[] ( QVec2 *qv, float value ) { qv->vec[0] = value; } )
			.addProperty( "y" , +[] ( const QVec2 *qv ) { return qv->vec[1]; }, +[] ( QVec2 *qv, float value ) { qv->vec[1] = value; } )
		.endClass()
		.beginClass<QVec3>( "vec3_t" )
			.addConstructor( []( void *ptr ) { return new ( ptr ) QVec3(); } )
			.addStaticFunction( "new", []() { return QVec3(); } )
			.addStaticFunction( "new", []( float x, float y, float z ) { return QVec3( x, y, z ); } )
			.addProperty( "x" , +[] ( const QVec3 *qv ) { return qv->vec[0]; }, +[] ( QVec3 *qv, float value ) { qv->vec[0] = value; } )
			.addProperty( "y" , +[] ( const QVec3 *qv ) { return qv->vec[1]; }, +[] ( QVec3 *qv, float value ) { qv->vec[1] = value; } )
			.addProperty( "z" , +[] ( const QVec3 *qv ) { return qv->vec[2]; }, +[] ( QVec3 *qv, float value ) { qv->vec[2] = value; } )
		.endClass()
		.beginClass<QColor>( "color_t" )
			.addConstructor( []( void *ptr ) { return new ( ptr ) QColor(); } )
			.addStaticFunction( "new", []() { return QColor(); } )
			.addStaticFunction( "new", []( float r, float g, float b, float a ) { return QColor( r, g, b, a ); } )
			.addStaticFunction( "new", []( float r, float g, float b ) { return QColor( r, g, b ); } )
			.addProperty( "r" , +[] ( const QColor *qc ) { return qc->rgba[0]; }, +[] ( QColor *qc, float value ) { qc->rgba[0] = value; } )
			.addProperty( "g" , +[] ( const QColor *qc ) { return qc->rgba[1]; }, +[] ( QColor *qc, float value ) { qc->rgba[1] = value; } )
			.addProperty( "b" , +[] ( const QColor *qc ) { return qc->rgba[2]; }, +[] ( QColor *qc, float value ) { qc->rgba[2] = value; } )
			.addProperty( "a" , +[] ( const QColor *qc ) { return qc->rgba[3]; }, +[] ( QColor *qc, float value ) { qc->rgba[3] = value; } )
		.endClass()
		.beginClass<QRGBA>( "rgba_t" )
			.addConstructor( []( void *ptr ) { return new ( ptr ) QRGBA(); } )
			.addStaticFunction( "new", []() { return QRGBA(); } )
			.addStaticFunction( "new", []( int r, int g, int b, int a ) { return QRGBA( r, g, b, a ); } )
			.addStaticFunction( "new", []( int r, int g, int b ) { return QRGBA( r, g, b ); } )
			.addProperty( "r" , +[] ( const QRGBA *qc ) { return qc->rgba[0]; }, +[] ( QRGBA *qc, int value ) { qc->rgba[0] = value; } )
			.addProperty( "g" , +[] ( const QRGBA *qc ) { return qc->rgba[1]; }, +[] ( QRGBA *qc, int value ) { qc->rgba[1] = value; } )
			.addProperty( "b" , +[] ( const QRGBA *qc ) { return qc->rgba[2]; }, +[] ( QRGBA *qc, int value ) { qc->rgba[2] = value; } )
			.addProperty( "a" , +[] ( const QRGBA *qc ) { return qc->rgba[3]; }, +[] ( QRGBA *qc, int value ) { qc->rgba[3] = value; } )
		.endClass()
		.beginClass<glconfig_t>( "glconfig_t" )
			.addProperty( "renderer_string", +[] ( const glconfig_t *glc ) { return glc->renderer_string; } )
			.addProperty( "vendor_string", +[]( const glconfig_t *glc ) { return glc->vendor_string; } )
			.addProperty( "version_string", +[]( const glconfig_t *glc ) { return glc->version_string; } )
			.addProperty( "extensions_string", +[]( const glconfig_t *glc ) { return glc->extensions_string; } )
			.addProperty( "maxTextureSize", &glconfig_t::maxTextureSize, false )
			.addProperty( "numTextureUnits", &glconfig_t::numTextureUnits, false )
			.addProperty( "colorBits", &glconfig_t::colorBits, false )
			.addProperty( "depthBits", &glconfig_t::depthBits, false )
			.addProperty( "stencilBits", &glconfig_t::stencilBits, false )
			.addProperty( "deviceSupportsGamma", &glconfig_t::deviceSupportsGamma, false )
			.addProperty( "textureCompression", &glconfig_t::textureCompression, false )
			.addProperty( "textureEnvAddAvailable", &glconfig_t::textureEnvAddAvailable, false )
			.addProperty( "vidWidth", &glconfig_t::vidWidth, false )
			.addProperty( "vidHeight", &glconfig_t::vidHeight, false )
			.addProperty( "windowAspect", &glconfig_t::windowAspect, false )
			.addProperty( "displayFrequency", &glconfig_t::displayFrequency, false )
			.addProperty( "isFullscreen", &glconfig_t::isFullscreen, false )
			.addProperty( "stereoEnabled", &glconfig_t::stereoEnabled, false )			
		.endClass()
		.beginClass<vmCvar_t>( "vmCvar_t" )
			.addConstructor( []( void *ptr ) { 
					vmCvar_t *cv = new ( ptr ) vmCvar_t( );
					memset( cv, 0, sizeof( vmCvar_t ) );
					return cv;
				}
			)
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
			.addConstructor( []( void *ptr ) { 
					refEntity_t *re = new ( ptr ) refEntity_t( );
					memset( re, 0, sizeof( refEntity_t ) );
					return re;
				}
			)
			.addProperty( "reType", &refEntity_t::reType )
			.addProperty( "renderfx", &refEntity_t::renderfx )
			.addProperty( "hModel", &refEntity_t::hModel )
			.addProperty( "lightingOrigin",
				+ []( const refEntity_t *re ) { return QVec3( re->lightingOrigin ); },
				+ [] ( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->lightingOrigin ); }
			)
			.addProperty( "shadowPlane", &refEntity_t::shadowPlane )
			.addProperty( "axis0", 
				+[]( const refEntity_t *re ) { return QVec3( re->axis[0]); },
				+[]( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->axis[0] ); }
			)
			.addProperty( "axis1", 
				+[]( const refEntity_t *re ) { return QVec3( re->axis[1]); },
				+[]( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->axis[1] ); }
			)
			.addProperty( "axis2", 
				+[]( const refEntity_t *re ) { return QVec3( re->axis[2]); },
				+[]( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->axis[2] ); }
			)
			.addProperty( "nonNormalizedAxes", &refEntity_t::nonNormalizedAxes )
			.addProperty( "origin", 
				+[]( const refEntity_t *re ) { return QVec3( re->origin ); },
				+[]( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->origin ); }
			)
			.addProperty( "frame", &refEntity_t::frame )
			.addProperty( "oldorigin", 
				+[]( const refEntity_t *re ) { return QVec3( re->oldorigin ); },
				+[]( refEntity_t *re, const QVec3 *value ) { VectorCopy( value->vec, re->oldorigin ); }
			)
			.addProperty( "oldframe", &refEntity_t::oldframe )
			.addProperty( "backlerp", &refEntity_t::backlerp )
			.addProperty( "skinNum", &refEntity_t::skinNum )
			.addProperty( "customSkin", &refEntity_t::customSkin )
			.addProperty( "customShader", &refEntity_t::customShader )
			.addProperty( "shaderRGBA", 
				+[]( const refEntity_t *re ) { return QRGBA( re->shaderRGBA ); },
				+[]( refEntity_t *re, const QRGBA *value ) { Vector4Copy( value->rgba, re->shaderRGBA ); }
			)
			.addProperty( "shaderTexCoord", 
				+[]( const refEntity_t *re ) { return QVec2( re->shaderTexCoord ); },
				+[]( refEntity_t *re, const QVec2 *value ) { Vector2Copy( value->vec , re->shaderTexCoord ); }
			)
			.addProperty( "shaderTime", &refEntity_t::shaderTime )
			.addProperty( "radius", &refEntity_t::radius )
			.addProperty( "rotation", &refEntity_t::rotation )
		.endClass()
		.beginClass<refdef_t>( "refdef_t" )
			.addConstructor( []( void *ptr ) { 
					refdef_t *rd = new ( ptr ) refdef_t( );
					memset( rd, 0, sizeof( refdef_t ) );
					return rd;
				}
			)
			.addProperty( "x", &refdef_t::x )
			.addProperty( "y", &refdef_t::y )
			.addProperty( "width", &refdef_t::width )
			.addProperty( "height", &refdef_t::height )
			.addProperty( "fov_x", &refdef_t::fov_x )
			.addProperty( "fov_y", &refdef_t::fov_y )
			.addProperty( "vieworg", 
				+[]( const refdef_t *rd ) { return QVec3( rd->vieworg ); },
				+[]( refdef_t *rd, const QVec3 *value ) { VectorCopy( value->vec, rd->vieworg ); }
			)
			.addProperty( "viewaxis0", 
				+[]( const refdef_t *rd ) { return QVec3( rd->viewaxis[0] ); },
				+[]( refdef_t *rd, const QVec3 *value ) { VectorCopy( value->vec, rd->viewaxis[0] ); }
			)
			.addProperty( "viewaxis1", 
				+[]( const refdef_t *rd ) { return QVec3( rd->viewaxis[1] ); },
				+[]( refdef_t *rd, const QVec3 *value ) { VectorCopy( value->vec, rd->viewaxis[1] ); }
			)
			.addProperty( "viewaxis2", 
				+[]( const refdef_t *rd ) { return QVec3( rd->viewaxis[2] ); },
				+[]( refdef_t *rd, const QVec3 *value ) { VectorCopy( value->vec, rd->viewaxis[2] ); }
			)
			.addProperty( "time", &refdef_t::time )
			.addProperty( "rdflags", &refdef_t::rdflags )
			// no support of areamask and those "deform text shaders"
			.addProperty( "forcedGreyscale", &refdef_t::forcedGreyscale )
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
				result.resize( strlen( result.data() ) );
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
				result.resize( strlen( result.data() ) );
				return result;
			})
		.addFunction( "trap_Argc", trap_Argc )
		.addFunction( "trap_Argv",
			+[]( int n ) {
				std::string result;
				result.resize( 1024 );
				trap_Argv( n, result.data(), result.size() );
				result.resize( strlen( result.data() ) );
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
		// trap_R_AddPolyToScene skipped
		.addFunction( "trap_R_AddLightToScene", +[]( const QVec3 *org, float intensity, float r, float g, float b ) { trap_R_AddLightToScene( org->vec, intensity, r, g, b ); } )
		.addFunction( "trap_R_RenderScene", trap_R_RenderScene )
		.addFunction( "trap_R_SetColor", 
			+[]() { trap_R_SetColor( NULL ); },
			+[]( const QColor *color ) { trap_R_SetColor( color->rgba ); }
		)
		.addFunction( "trap_R_DrawStretchPic", trap_R_DrawStretchPic )
		.addFunction( "trap_R_DrawTriangle", trap_R_DrawTriangle )
		.addFunction( "trap_R_GetShaderImageDimensions",
			+[]( qhandle_t shader, int nstage, int nimage ) {
				int width, height;
				trap_R_GetShaderImageDimensions( shader, nstage, nimage, &width, &height );
				return QVec2( width, height );
			}
		)
		.addFunction( "trap_R_ModelBounds", +[]( clipHandle_t model, QVec3 *mins, QVec3 *maxs ) { trap_R_ModelBounds( model, mins->vec, maxs->vec ); } )
		.addFunction( "trap_UpdateScreen", trap_UpdateScreen )
		.beginClass<orientation_t>( "orientation_t" )
			.addConstructor( []( void *ptr ) {
					orientation_t *orient = new (ptr) orientation_t();
					memset( orient, 0, sizeof( orientation_t ) );
					return orient;
				}
			)
			.addProperty( "origin", 
				+[]( const orientation_t *orient ) { return QVec3( orient->origin ); },
				+[]( orientation_t *orient, const QVec3 *value ) { VectorCopy( value->vec, orient->origin ); }
			)
			.addProperty( "axis0", 
				+[]( const orientation_t *orient ) { return QVec3( orient->axis[0] ); },
				+[]( orientation_t *orient, const QVec3 *value ) { VectorCopy( value->vec, orient->axis[0] ); }
			)
			.addProperty( "axis1", 
				+[]( const orientation_t *orient ) { return QVec3( orient->axis[1] ); },
				+[]( orientation_t *orient, const QVec3 *value ) { VectorCopy( value->vec, orient->axis[1] ); }
			)
			.addProperty( "axis2", 
				+[]( const orientation_t *orient ) { return QVec3( orient->axis[2] ); },
				+[]( orientation_t *orient, const QVec3 *value ) { VectorCopy( value->vec, orient->axis[2] ); }
			)
		.endClass()
		.addFunction( "trap_CM_LerpTag", trap_CM_LerpTag )
		.addFunction( "trap_S_StartLocalSound", trap_S_StartLocalSound )
		.addFunction( "trap_S_RegisterSound", +[] ( const char *sample, bool compressed ) { trap_S_RegisterSound( sample, (qboolean)compressed ); })
		.addFunction( "trap_Key_KeynumToString", 
			+[] ( int keynum ) {
				std::string result;
				result.resize( 32 );
				trap_Key_KeynumToStringBuf( keynum, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_Key_GetBinding", 
			+[] ( int keynum ) {
				std::string result;
				result.resize( 32 );
				trap_Key_GetBindingBuf( keynum, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_Key_SetBinding", trap_Key_SetBinding )
		.addFunction( "trap_Key_IsDown", +[]( int keynum ) { return (bool)trap_Key_IsDown( keynum ); })
		.addFunction( "trap_Key_GetOverstrikeMode", +[]() { return (bool)trap_Key_GetOverstrikeMode(); })
		.addFunction( "trap_Key_SetOverstrikeMode", +[]( bool state ) { trap_Key_SetOverstrikeMode( (qboolean)state ); })
		.addFunction( "trap_Key_ClearStates", trap_Key_ClearStates  )
		.addFunction( "trap_Key_GetCatcher", trap_Key_GetCatcher )
		.addFunction( "trap_Key_SetCatcher", trap_Key_SetCatcher )
		// trap_GetClipboardData skipped
		.beginClass<uiClientState_t>( "uiClientState_t" )
			.addProperty( "connstate", &uiClientState_t::connState, false )
			.addProperty( "connectPacketCount", &uiClientState_t::connectPacketCount, false )
			.addProperty( "clientNum", &uiClientState_t::clientNum, false )
			.addProperty( "servername", +[] ( const uiClientState_t *uics ) { return uics->servername; } )
			.addProperty( "updateInfoString", +[] ( const uiClientState_t *uics ) { return uics->updateInfoString; } )
			.addProperty( "messageString", +[] ( const uiClientState_t *uics ) { return uics->messageString; } )
		.endClass()
		.addFunction( "trap_GetClientState", 
			+[] () {
				uiClientState_t uics = { 0 };
				trap_GetClientState( &uics );
				return uics;
			}
		)
		.addFunction( "trap_GetGlconfig", 
			+[] () {
				glconfig_t glc = { 0 };
				trap_GetGlconfig( &glc );
				return glc;
			}
		)
		.addFunction( "trap_GetConfigString",
			+[] ( int index ) {
				std::string result;
				result.resize( 1024 );
				trap_GetConfigString( index, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_LAN_GetServerCount", trap_LAN_GetServerCount )
		.addFunction( "trap_LAN_GetServerAddressString", 
			+[] ( int source, int n ) {
				std::string result;
				result.resize( 1024 );
				trap_LAN_GetServerAddressString( source, n, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_LAN_GetServerInfo", 
			+[] ( int source, int n ) {
				std::string result;
				result.resize( 1024 );
				trap_LAN_GetServerInfo( source, n, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_LAN_GetServerPing", trap_LAN_GetServerPing )
		.addFunction( "trap_LAN_GetPingQueueCount", trap_LAN_GetPingQueueCount )
		.addFunction( "trap_LAN_ServerStatus",
			+[]( const char *serverAddress ) {
				std::string result;
				result.resize( 1024 );
				trap_LAN_ServerStatus( serverAddress, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_LAN_SaveCachedServers", trap_LAN_SaveCachedServers )
		.addFunction( "trap_LAN_LoadCachedServers", trap_LAN_LoadCachedServers )
		.addFunction( "trap_LAN_ResetPings", trap_LAN_ResetPings )
		.addFunction( "trap_LAN_ClearPing", trap_LAN_ClearPing )
		.addFunction( "trap_LAN_GetPing",
			+[]( lua_State *L ) {
				auto n = luabridge::Stack<int>::get( L, 1 );
				std::string result;
				int pingtime;
				result.resize( 1024 );
				trap_LAN_GetPing( n.value(), result.data(), result.size(), &pingtime);
				result.resize( strlen( result.c_str() ) );
				auto retval = luabridge::Stack<std::string>::push( L, result );
				retval = luabridge::Stack<int>::push( L, pingtime );
				return 2;
			}
		)
		.addFunction( "trap_LAN_GetPingInfo",
			+[]( int n ) {
				std::string result;
				result.resize( 1024 );
				trap_LAN_GetPingInfo( n, result.data(), result.size() );
				result.resize( strlen( result.c_str() ) );
				return result;
			}
		)
		.addFunction( "trap_LAN_MarkServerVisible", +[]( int source, int n, bool visible ) { trap_LAN_MarkServerVisible( source, n, (qboolean)visible ); } )
		.addFunction( "trap_LAN_ServerIsVisible", +[]( int source, int n ) { return (bool)trap_LAN_ServerIsVisible( source, n ); } )
		.addFunction( "trap_LAN_UpdateVisiblePings", trap_LAN_UpdateVisiblePings )
		.addFunction( "trap_LAN_AddServer", trap_LAN_AddServer )
		.addFunction( "trap_LAN_RemoveServer", trap_LAN_RemoveServer )
		.addFunction( "trap_LAN_CompareServers", trap_LAN_CompareServers )
		.addFunction( "trap_MemoryRemaining", trap_MemoryRemaining )
		// CD-key and PC-functions skipped
		.addFunction( "trap_S_StopBackgroundTrack", trap_S_StopBackgroundTrack )
		.addFunction( "trap_S_StartBackgroundTrack", trap_S_StartBackgroundTrack )
		.beginClass<qtime_t>( "qtime_t" )
			.addProperty( "tm_sec", &qtime_t::tm_sec, false )
			.addProperty( "tm_min", &qtime_t::tm_min, false )
			.addProperty( "tm_hour", &qtime_t::tm_hour, false )
			.addProperty( "tm_mday", &qtime_t::tm_mday, false )
			.addProperty( "tm_mon", &qtime_t::tm_mon, false )
			.addProperty( "tm_year", &qtime_t::tm_year, false )
			.addProperty( "tm_wday", &qtime_t::tm_wday, false )
			.addProperty( "tm_yday", &qtime_t::tm_yday, false )
			.addProperty( "tm_isdst", &qtime_t::tm_isdst, false )
		.endClass()
		.addFunction( "trap_RealTime",
			+[] () {
				qtime_t qt = { 0 };
				trap_RealTime( &qt );
				return qt;
			}
		)
		// Cinematic handling functions skipped
		.addFunction( "trap_R_RemapShader", trap_R_RemapShader )
		.addFunction( "trap_Cvar_Watch",
			+[] ( const char *var_name, bool watch ) {
				trap_Cvar_Watch( var_name, (qboolean)watch );
			}
		)
		// CD-key, punkbuster and extension functions skipped
	;
}
