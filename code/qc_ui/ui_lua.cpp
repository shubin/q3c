#include "ui_local.h"
#include <lauxlib.h>
#include <initializer_list>

#if defined( cast )
#undef cast
#endif

#define USE_LUA_BRIDGE

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "LuaBridge/LuaBridge.h"
#include "LuaBridge/Vector.h"
using namespace luabridge;


void Q_strncpyz( char *dest, const char *src, int destsize ) {
	strncpy( dest, src, destsize - 1 );
	dest[destsize - 1] = 0;
}

//void			trap_Error( const char *string ) __attribute__( ( noreturn ) );
static void lua_Error( const char *string) {
	trap_Error( string );
}

//void			trap_Print( const char *string );
static void lua_Print( const char *string ) {
	trap_Print( string );
}

//int				trap_Milliseconds( void );
static int lua_Milliseconds() {
	return trap_Milliseconds();
}

struct Lua_vmCvar_t: vmCvar_t {
	void setString( const char *string ) {
		Q_strncpyz( this->string, string, sizeof( this->string ) );
	}
	const char *getString() const {
		return this->string;
	}
};

//void			trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
static Lua_vmCvar_t lua_Cvar_Register( const char *varName, const char *defaultValue, int flags ) {
	Lua_vmCvar_t vmCvar = {};
	trap_Cvar_Register( &vmCvar, varName, defaultValue, flags );
	return vmCvar;
}

//void			trap_Cvar_Update( vmCvar_t *vmCvar );
static void lua_Cvar_Update( Lua_vmCvar_t &vmCvar ) {
	trap_Cvar_Update( &vmCvar );
}

//void			trap_Cvar_Set( const char *var_name, const char *value );
static void lua_Cvar_Set( const char *var_name, const char *value ) {
	trap_Cvar_Set( var_name, value );
}

//float			trap_Cvar_VariableValue( const char *var_name );
static float lua_Cvar_VariableValue( const char *var_name ) {
	return trap_Cvar_VariableValue( var_name );
}

//void			trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
static const char* lua_Cvar_VariableStringBuffer( const char *var_name ) {
	static char buffer[1024];
	trap_Cvar_VariableStringBuffer( var_name, buffer, ARRAY_LEN( buffer ) );
	return buffer;
}

//void			trap_Cvar_SetValue( const char *var_name, float value );
static void lua_Cvar_SetValue( const char *var_name, float value) {
	trap_Cvar_SetValue( var_name, value );
}

//void			trap_Cvar_Reset( const char *name );
static void lua_Cvar_Reset( const char *name ) {
	trap_Cvar_Reset( name );
}

//void			trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
static void lua_Cvar_Create( const char *var_name, const char *var_value, int flags ) {
	trap_Cvar_Create( var_name, var_value, flags );
}

//void			trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
static const char* lua_Cvar_InfoStringBuffer( int bit ) {
	static char buffer[1024];
	trap_Cvar_InfoStringBuffer( bit, buffer, ARRAY_LEN( buffer ) );
	return buffer;
}

//int				trap_Argc( void );
static int lua_Argc() {
	return trap_Argc();
}

//void			trap_Argv( int n, char *buffer, int bufferLength );
static const char* lua_Argv( int n ) {
	static char buffer[1024];
	trap_Argv( n, buffer, ARRAY_LEN( buffer ) );
	return buffer;
}

//void			trap_Cmd_ExecuteText( int exec_when, const char *text );	// don't use EXEC_NOW!
static int lua_Cmd_ExecuteText( int exec_when, const char *text ) {
	trap_Cmd_ExecuteText( exec_when, text );
	return 0;
}

struct Q_File {
	fileHandle_t	handle;
	int				size;

	int	getLength() const {
		return size;
	}
};

//int				trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
static Q_File lua_FS_FOpenFile( const char *qpath, int mode ) {
	Q_File file;
	file.size = trap_FS_FOpenFile( qpath, &file.handle, (fsMode_t)mode );
	return file;
}

//void			trap_FS_Read( void *buffer, int len, fileHandle_t f );
static std::string lua_FS_Read( int len, const Q_File &file ) {
	char *buffer = (char*)dlmalloc( len );
	memset( buffer, 0, len );
	trap_FS_Read( buffer, len, file.handle );
	std::string retval( buffer, len );
	dlfree( buffer );
	return retval;
}

//void			trap_FS_Write( const void *buffer, int len, fileHandle_t f );
static void lua_FS_Write( const std::string& buffer, const Q_File &file ) {
	trap_FS_Write( buffer.c_str(), (int)buffer.size(), file.handle );
}

//void			trap_FS_FCloseFile( fileHandle_t f );
static int lua_FS_FCloseFile( const Q_File &file ) {
	trap_FS_FCloseFile( file.handle );
	return 0;
}

//int				trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
static std::vector<std::string> lua_FS_GetFileList( const char *path, const char *extension ) {
	const int bufsize = 16384;
	char *listbuf = ( char * )dlmalloc( bufsize );

	int nFiles = trap_FS_GetFileList( path, extension, listbuf, bufsize );

	std::vector<std::string> result;
	result.reserve( nFiles );

	char *bufend = listbuf;
	for ( int i = 0; i < nFiles; i++ ) {
		result.push_back( bufend );
		bufend += strlen( bufend ) + 1;
	}
	dlfree( listbuf );

	return result;
}

//int				trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
static int lua_FS_Seek( const Q_File &file, long offset, int origin ) {
	return trap_FS_Seek( file.handle, offset, origin );
}

//qhandle_t		trap_R_RegisterModel( const char *name );
static qhandle_t lua_R_RegisterModel( const char *name ) {
	return trap_R_RegisterModel( name );
}

//qhandle_t		trap_R_RegisterSkin( const char *name );
static qhandle_t lua_R_RegisterSkin( const char *name ) {
	return trap_R_RegisterSkin( name );
}

//qhandle_t		trap_R_RegisterShaderNoMip( const char *name );
static qhandle_t lua_R_RegisterShaderNoMip( const char *name ) {
	return trap_R_RegisterShaderNoMip( name );
}

//void			trap_R_ClearScene( void );
static void lua_R_ClearScene() {
	trap_R_ClearScene();
}

//void			trap_R_SetColor( const float *rgba );
static int lua_R_SetColor( lua_State *L ) {
	float rgba[4];

	if ( lua_isnil( L, 1 ) ) {
		trap_R_SetColor( NULL );
	} else {
		rgba[0] = ( float )luaL_checknumber( L, 1 );
		rgba[1] = ( float )luaL_checknumber( L, 2 );
		rgba[2] = ( float )luaL_checknumber( L, 3 );
		rgba[3] = ( float )luaL_optnumber( L, 4, 1.0 );
		trap_R_SetColor( rgba );
	}
	return 0;
}

//void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
static void lua_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}

//void			trap_UpdateScreen( void );
static void lua_UpdateScreen() {
	trap_UpdateScreen();
}

//int			trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );
static int lua_CM_LerpTag( lua_State *L ) {
	// not implemented
	return 0;
}

//void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
static void lua_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	trap_S_StartLocalSound( sfx, channelNum );
}

//sfxHandle_t		trap_S_RegisterSound( const char *sample, qboolean compressed );
static sfxHandle_t lua_S_RegisterSound( const char *sample, bool compressed ) {
	return trap_S_RegisterSound( sample, compressed ? qtrue : qfalse );
}


//void			trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
static std::string lua_Key_KeynumToStringBuf( int keynum ) {
	static char buf[32];
	trap_Key_KeynumToStringBuf( keynum, buf, ARRAY_LEN( buf ) );
	return std::string( buf );
}

//void			trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
static const char* lua_Key_GetBindingBuf( int keynum ) {
	static char buf[32];
	trap_Key_GetBindingBuf( keynum, buf, ARRAY_LEN( buf ) );
	return buf;
}

//void			trap_Key_SetBinding( int keynum, const char *binding );
static void lua_Key_SetBinding( int keynum, const char *binding ) {
	trap_Key_SetBinding( keynum, binding );
}

//qboolean		trap_Key_IsDown( int keynum );
static bool lua_Key_IsDown( int keynum ) {
	return trap_Key_IsDown( keynum );
}

//qboolean		trap_Key_GetOverstrikeMode( void );
static bool lua_Key_GetOverstrikeMode() {
	return trap_Key_GetOverstrikeMode();
}

//void			trap_Key_SetOverstrikeMode( qboolean state );
static void lua_Key_SetOverstrikeMode( bool state ) {
	trap_Key_SetOverstrikeMode( state ? qtrue: qfalse );
}

//void			trap_Key_ClearStates( void );
static void lua_Key_ClearStates() {
	trap_Key_ClearStates();
}

//int				trap_Key_GetCatcher( void );
static int lua_Key_GetCatcher() {
	return trap_Key_GetCatcher();
}

//void			trap_Key_SetCatcher( int catcher );
static void lua_Key_SetCatcher( int catcher ) {
	trap_Key_SetCatcher( catcher );
}

//void			trap_GetClipboardData( char *buf, int bufsize );
static const char* lua_GetClipboardData() {
	static char buf[1024];
	trap_GetClipboardData( buf, ARRAY_LEN( buf ) );
	return buf;
}

//int				trap_GetConfigString( int index, char *buff, int buffsize );
static const char* lua_GetConfigString( int index ) {
	static char buf[MAX_CONFIGSTRINGS];
	int result = trap_GetConfigString( index, buf, ARRAY_LEN( buf ) );
	if ( !result ) {
		buf[0] = '\0';
	}
	return buf;
}

//int				trap_LAN_GetServerCount( int source );
static int lua_LAN_GetServerCount( int source ) {
	return trap_LAN_GetServerCount( source );
}

//void			trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
static const char* lua_LAN_GetServerAddressString( int source, int n ) {
	static char buf[1024];
	trap_LAN_GetServerAddressString( source, n, buf, ARRAY_LEN( buf ) );
	return buf;
}

//void			trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
static const char* lua_LAN_GetServerInfo( int source, int n ) {
	static char buf[1024];
	trap_LAN_GetServerInfo( source, n, buf, ARRAY_LEN( buf ) );
	return buf;
}

//int				trap_LAN_GetPingQueueCount( void );
static int lua_LAN_GetPingQueueCount() {
	return trap_LAN_GetPingQueueCount();
}

//int				trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
static const char* lua_LAN_ServerStatus( const char *serverAddress ) {
	static char serverStatus[1024];
	trap_LAN_ServerStatus( serverAddress, serverStatus, ARRAY_LEN( serverStatus ) );
	return serverStatus;
}

//void			trap_LAN_ClearPing( int n );
static void lua_LAN_ClearPing( int n ) {
	trap_LAN_ClearPing( n );
}

//void			trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
static int lua_LAN_GetPing( lua_State *L ) {
	int n = luaL_checkinteger( L, 1 );
	static char buf[1024];
	int pingtime;
	trap_LAN_GetPing( n, buf, ARRAY_LEN( buf ), &pingtime );
	lua_pushstring( L, buf );
	lua_pushinteger( L, pingtime );
	return 2;
}

//void			trap_LAN_GetPingInfo( int n, char *buf, int buflen );
static const char* lua_LAN_GetPingInfo( int n ) {
	static char buf[1024];
	trap_LAN_GetPingInfo( n, buf, ARRAY_LEN( buf ) );
	return buf;
}

//int				trap_MemoryRemaining( void );
static int lua_MemoryRemaining() {
	return trap_MemoryRemaining();
}

//void			trap_GetCDKey( char *buf, int buflen );
static const char* lua_GetCDKey() {
	static char buf[1024];
	trap_GetCDKey( buf, ARRAY_LEN( buf ) );
	return buf;
}

//void			trap_SetCDKey( char *buf );
static void lua_SetCDKey( const char *buf ) {
	trap_SetCDKey( buf );
}

//qboolean        trap_VerifyCDKey( const char *key, const char *chksum );
static bool lua_VerifyCDKey( const char *key, const char *chksum ) {
	return trap_VerifyCDKey( key, chksum );
}

//void			trap_SetPbClStatus( int status );
static void lua_SetPbClStatus( int status ) {
	trap_SetPbClStatus( status );
}

//void trap_R_SetMatrix( const float *matrix )
static int lua_R_SetMatrix( lua_State *L ) {
	if ( lua_isnil( L, 1 ) ) {
		trap_R_SetMatrix( NULL );
	} else {
		// todo: pass the actual matrix
		trap_R_SetMatrix( NULL );
	}
	return 0;
}




typedef struct {
	const char *name;
	int value;
} luaL_Integer;

static const luaL_Integer uilib_intconst[] = {
	{ "KEYCATCH_CONSOLE", KEYCATCH_CONSOLE },
	{ "KEYCATCH_UI", KEYCATCH_UI },
	{ "KEYCATCH_MESSAGE", KEYCATCH_MESSAGE },
	{ "KEYCATCH_CGAME", KEYCATCH_CGAME },
	{ "EXEC_NOW", EXEC_NOW },
	{ "EXEC_INSERT", EXEC_INSERT },
	{ "EXEC_APPEND", EXEC_APPEND },
	{ "FS_READ", FS_READ },
	{ "FS_WRITE", FS_WRITE },
	{ "FS_APPEND", FS_APPEND },
	{ "FS_APPEND_SYNC", FS_APPEND_SYNC },
	{ "FS_SEEK_CUR", FS_SEEK_CUR },
	{ "FS_SEEK_END", FS_SEEK_END },
	{ "FS_SEEK_SET", FS_SEEK_SET },
	{ "CHAN_AUTO", CHAN_AUTO },
	{ "CHAN_LOCAL", CHAN_LOCAL },
	{ "CHAN_WEAPON", CHAN_WEAPON },
	{ "CHAN_VOICE", CHAN_VOICE },
	{ "CHAN_ITEM", CHAN_ITEM },
	{ "CHAN_BODY", CHAN_BODY },
	{ "CHAN_LOCAL_SOUND", CHAN_LOCAL_SOUND },
	{ "CHAN_ANNOUNCER", CHAN_ANNOUNCER },
	{ "CA_UNINITIALIZED", CA_UNINITIALIZED },
	{ "CA_DISCONNECTED", CA_DISCONNECTED },
	{ "CA_AUTHORIZING", CA_AUTHORIZING },
	{ "CA_CONNECTING", CA_CONNECTING },
	{ "CA_CHALLENGING", CA_CHALLENGING },
	{ "CA_CONNECTED", CA_CONNECTED },
	{ "CA_LOADING", CA_LOADING },
	{ "CA_PRIMED", CA_PRIMED },
	{ "CA_ACTIVE", CA_ACTIVE },
	{ "CA_CINEMATIC", CA_CINEMATIC },
	{ "GLDRV_ICD", GLDRV_ICD },
	{ "GLDRV_STANDALONE", GLDRV_STANDALONE },
	{ "GLDRV_VOODOO", GLDRV_VOODOO },
	{ "GLHW_GENERIC", GLHW_GENERIC },
	{ "GLHW_3DFX_2D3D", GLHW_3DFX_2D3D },
	{ "GLHW_RIVA128", GLHW_RIVA128 },
	{ "GLHW_RAGEPRO" ,GLHW_RAGEPRO },
	{ "GLHW_PERMEDIA2", GLHW_PERMEDIA2 },
	{ "AS_LOCAL", AS_LOCAL },
	{ "AS_MPLAYER", AS_MPLAYER },
	{ "AS_GLOBAL", AS_GLOBAL },
	{ "AS_FAVORITES", AS_FAVORITES },

// refEntityType_t
	{ "RT_MODEL", RT_MODEL },
	{ "RT_POLY", RT_POLY },
	{ "RT_SPRITE", RT_SPRITE },
	{ "RT_BEAM", RT_BEAM },
	{ "RT_RAIL_CORE", RT_RAIL_CORE },
	{ "RT_RAIL_RINGS", RT_RAIL_RINGS },
	{ "RT_LIGHTNING", RT_LIGHTNING },
	{ "RT_PORTALSURFACE", RT_PORTALSURFACE },
	{ "RT_MAX_REF_ENTITY_TYPE", RT_MAX_REF_ENTITY_TYPE },

// Key codes
	{ "K_TAB", K_TAB },
	{ "K_ENTER", K_ENTER },
	{ "K_ESCAPE", K_ESCAPE },
	{ "K_SPACE", K_SPACE },

	{ "K_BACKSPACE", K_BACKSPACE },

	{ "K_COMMAND", K_COMMAND },
	{ "K_CAPSLOCK", K_CAPSLOCK },
	{ "K_POWER", K_POWER },
	{ "K_PAUSE", K_PAUSE },

	{ "K_UPARROW", K_UPARROW },
	{ "K_DOWNARROW", K_DOWNARROW },
	{ "K_LEFTARROW", K_LEFTARROW },
	{ "K_RIGHTARROW", K_RIGHTARROW },

	{ "K_ALT", K_ALT },
	{ "K_CTRL", K_CTRL },
	{ "K_SHIFT", K_SHIFT },
	{ "K_INS", K_INS },
	{ "K_DEL", K_DEL },
	{ "K_PGDN", K_PGDN },
	{ "K_PGUP", K_PGUP },
	{ "K_HOME", K_HOME },
	{ "K_END", K_END },

	{ "K_F1", K_F1 },
	{ "K_F2", K_F2 },
	{ "K_F3", K_F3 },
	{ "K_F4", K_F4 },
	{ "K_F5", K_F5 },
	{ "K_F6", K_F6 },
	{ "K_F7", K_F7 },
	{ "K_F8", K_F8 },
	{ "K_F9", K_F9 },
	{ "K_F10", K_F10 },
	{ "K_F11", K_F11 },
	{ "K_F12", K_F12 },
	{ "K_F13", K_F13 },
	{ "K_F14", K_F14 },
	{ "K_F15", K_F15 },

	{ "K_KP_HOME", K_KP_HOME },
	{ "K_KP_UPARROW", K_KP_UPARROW },
	{ "K_KP_PGUP", K_KP_PGUP },
	{ "K_KP_LEFTARROW", K_KP_LEFTARROW },
	{ "K_KP_5", K_KP_5 },
	{ "K_KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "K_KP_END", K_KP_END },
	{ "K_KP_DOWNARROW", K_KP_DOWNARROW },
	{ "K_KP_PGDN", K_KP_PGDN },
	{ "K_KP_ENTER", K_KP_ENTER },
	{ "K_KP_INS", K_KP_INS },
	{ "K_KP_DEL", K_KP_DEL },
	{ "K_KP_SLASH", K_KP_SLASH },
	{ "K_KP_MINUS", K_KP_MINUS },
	{ "K_KP_PLUS", K_KP_PLUS },
	{ "K_KP_NUMLOCK", K_KP_NUMLOCK },
	{ "K_KP_STAR", K_KP_STAR },
	{ "K_KP_EQUALS", K_KP_EQUALS },

	{ "K_MOUSE1", K_MOUSE1 },
	{ "K_MOUSE2", K_MOUSE2 },
	{ "K_MOUSE3", K_MOUSE3 },
	{ "K_MOUSE4", K_MOUSE4 },
	{ "K_MOUSE5", K_MOUSE5 },

	{ "K_MWHEELDOWN", K_MWHEELDOWN },
	{ "K_MWHEELUP", K_MWHEELUP },

	{ "K_JOY1", K_JOY1 },
	{ "K_JOY2", K_JOY2 },
	{ "K_JOY3", K_JOY3 },
	{ "K_JOY4", K_JOY4 },
	{ "K_JOY5", K_JOY5 },
	{ "K_JOY6", K_JOY6 },
	{ "K_JOY7", K_JOY7 },
	{ "K_JOY8", K_JOY8 },
	{ "K_JOY9", K_JOY9 },
	{ "K_JOY10", K_JOY10 },
	{ "K_JOY11", K_JOY11 },
	{ "K_JOY12", K_JOY12 },
	{ "K_JOY13", K_JOY13 },
	{ "K_JOY14", K_JOY14 },
	{ "K_JOY15", K_JOY15 },
	{ "K_JOY16", K_JOY16 },
	{ "K_JOY17", K_JOY17 },
	{ "K_JOY18", K_JOY18 },
	{ "K_JOY19", K_JOY19 },
	{ "K_JOY20", K_JOY20 },
	{ "K_JOY21", K_JOY21 },
	{ "K_JOY22", K_JOY22 },
	{ "K_JOY23", K_JOY23 },
	{ "K_JOY24", K_JOY24 },
	{ "K_JOY25", K_JOY25 },
	{ "K_JOY26", K_JOY26 },
	{ "K_JOY27", K_JOY27 },
	{ "K_JOY28", K_JOY28 },
	{ "K_JOY29", K_JOY29 },
	{ "K_JOY30", K_JOY30 },
	{ "K_JOY31", K_JOY31 },
	{ "K_JOY32", K_JOY32 },

	{ "K_AUX1", K_AUX1 },
	{ "K_AUX2", K_AUX2 },
	{ "K_AUX3", K_AUX3 },
	{ "K_AUX4", K_AUX4 },
	{ "K_AUX5", K_AUX5 },
	{ "K_AUX6", K_AUX6 },
	{ "K_AUX7", K_AUX7 },
	{ "K_AUX8", K_AUX8 },
	{ "K_AUX9", K_AUX9 },
	{ "K_AUX10", K_AUX10 },
	{ "K_AUX11", K_AUX11 },
	{ "K_AUX12", K_AUX12 },
	{ "K_AUX13", K_AUX13 },
	{ "K_AUX14", K_AUX14 },
	{ "K_AUX15", K_AUX15 },
	{ "K_AUX16", K_AUX16 },

	{ "K_WORLD_0", K_WORLD_0 },
	{ "K_WORLD_1", K_WORLD_1 },
	{ "K_WORLD_2", K_WORLD_2 },
	{ "K_WORLD_3", K_WORLD_3 },
	{ "K_WORLD_4", K_WORLD_4 },
	{ "K_WORLD_5", K_WORLD_5 },
	{ "K_WORLD_6", K_WORLD_6 },
	{ "K_WORLD_7", K_WORLD_7 },
	{ "K_WORLD_8", K_WORLD_8 },
	{ "K_WORLD_9", K_WORLD_9 },
	{ "K_WORLD_10", K_WORLD_10 },
	{ "K_WORLD_11", K_WORLD_11 },
	{ "K_WORLD_12", K_WORLD_12 },
	{ "K_WORLD_13", K_WORLD_13 },
	{ "K_WORLD_14", K_WORLD_14 },
	{ "K_WORLD_15", K_WORLD_15 },
	{ "K_WORLD_16", K_WORLD_16 },
	{ "K_WORLD_17", K_WORLD_17 },
	{ "K_WORLD_18", K_WORLD_18 },
	{ "K_WORLD_19", K_WORLD_19 },
	{ "K_WORLD_20", K_WORLD_20 },
	{ "K_WORLD_21", K_WORLD_21 },
	{ "K_WORLD_22", K_WORLD_22 },
	{ "K_WORLD_23", K_WORLD_23 },
	{ "K_WORLD_24", K_WORLD_24 },
	{ "K_WORLD_25", K_WORLD_25 },
	{ "K_WORLD_26", K_WORLD_26 },
	{ "K_WORLD_27", K_WORLD_27 },
	{ "K_WORLD_28", K_WORLD_28 },
	{ "K_WORLD_29", K_WORLD_29 },
	{ "K_WORLD_30", K_WORLD_30 },
	{ "K_WORLD_31", K_WORLD_31 },
	{ "K_WORLD_32", K_WORLD_32 },
	{ "K_WORLD_33", K_WORLD_33 },
	{ "K_WORLD_34", K_WORLD_34 },
	{ "K_WORLD_35", K_WORLD_35 },
	{ "K_WORLD_36", K_WORLD_36 },
	{ "K_WORLD_37", K_WORLD_37 },
	{ "K_WORLD_38", K_WORLD_38 },
	{ "K_WORLD_39", K_WORLD_39 },
	{ "K_WORLD_40", K_WORLD_40 },
	{ "K_WORLD_41", K_WORLD_41 },
	{ "K_WORLD_42", K_WORLD_42 },
	{ "K_WORLD_43", K_WORLD_43 },
	{ "K_WORLD_44", K_WORLD_44 },
	{ "K_WORLD_45", K_WORLD_45 },
	{ "K_WORLD_46", K_WORLD_46 },
	{ "K_WORLD_47", K_WORLD_47 },
	{ "K_WORLD_48", K_WORLD_48 },
	{ "K_WORLD_49", K_WORLD_49 },
	{ "K_WORLD_50", K_WORLD_50 },
	{ "K_WORLD_51", K_WORLD_51 },
	{ "K_WORLD_52", K_WORLD_52 },
	{ "K_WORLD_53", K_WORLD_53 },
	{ "K_WORLD_54", K_WORLD_54 },
	{ "K_WORLD_55", K_WORLD_55 },
	{ "K_WORLD_56", K_WORLD_56 },
	{ "K_WORLD_57", K_WORLD_57 },
	{ "K_WORLD_58", K_WORLD_58 },
	{ "K_WORLD_59", K_WORLD_59 },
	{ "K_WORLD_60", K_WORLD_60 },
	{ "K_WORLD_61", K_WORLD_61 },
	{ "K_WORLD_62", K_WORLD_62 },
	{ "K_WORLD_63", K_WORLD_63 },
	{ "K_WORLD_64", K_WORLD_64 },
	{ "K_WORLD_65", K_WORLD_65 },
	{ "K_WORLD_66", K_WORLD_66 },
	{ "K_WORLD_67", K_WORLD_67 },
	{ "K_WORLD_68", K_WORLD_68 },
	{ "K_WORLD_69", K_WORLD_69 },
	{ "K_WORLD_70", K_WORLD_70 },
	{ "K_WORLD_71", K_WORLD_71 },
	{ "K_WORLD_72", K_WORLD_72 },
	{ "K_WORLD_73", K_WORLD_73 },
	{ "K_WORLD_74", K_WORLD_74 },
	{ "K_WORLD_75", K_WORLD_75 },
	{ "K_WORLD_76", K_WORLD_76 },
	{ "K_WORLD_77", K_WORLD_77 },
	{ "K_WORLD_78", K_WORLD_78 },
	{ "K_WORLD_79", K_WORLD_79 },
	{ "K_WORLD_80", K_WORLD_80 },
	{ "K_WORLD_81", K_WORLD_81 },
	{ "K_WORLD_82", K_WORLD_82 },
	{ "K_WORLD_83", K_WORLD_83 },
	{ "K_WORLD_84", K_WORLD_84 },
	{ "K_WORLD_85", K_WORLD_85 },
	{ "K_WORLD_86", K_WORLD_86 },
	{ "K_WORLD_87", K_WORLD_87 },
	{ "K_WORLD_88", K_WORLD_88 },
	{ "K_WORLD_89", K_WORLD_89 },
	{ "K_WORLD_90", K_WORLD_90 },
	{ "K_WORLD_91", K_WORLD_91 },
	{ "K_WORLD_92", K_WORLD_92 },
	{ "K_WORLD_93", K_WORLD_93 },
	{ "K_WORLD_94", K_WORLD_94 },
	{ "K_WORLD_95", K_WORLD_95 },

	{ "K_SUPER", K_SUPER },
	{ "K_COMPOSE", K_COMPOSE },
	{ "K_MODE", K_MODE },
	{ "K_HELP", K_HELP },
	{ "K_PRINT", K_PRINT },
	{ "K_SYSREQ", K_SYSREQ },
	{ "K_SCROLLOCK", K_SCROLLOCK },
	{ "K_BREAK", K_BREAK },
	{ "K_MENU", K_MENU },
	{ "K_EURO", K_EURO },
	{ "K_UNDO", K_UNDO },

	// Gamepad controls
	// Ordered to match SDL2 game controller buttons and axes
	// Do not change this order without also changing IN_GamepadMove() in SDL_input.c
	{ "K_PAD0_A", K_PAD0_A },
	{ "K_PAD0_B", K_PAD0_B },
	{ "K_PAD0_X", K_PAD0_X },
	{ "K_PAD0_Y", K_PAD0_Y },
	{ "K_PAD0_BACK", K_PAD0_BACK },
	{ "K_PAD0_GUIDE", K_PAD0_GUIDE },
	{ "K_PAD0_START", K_PAD0_START },
	{ "K_PAD0_LEFTSTICK_CLICK", K_PAD0_LEFTSTICK_CLICK },
	{ "K_PAD0_RIGHTSTICK_CLICK", K_PAD0_RIGHTSTICK_CLICK },
	{ "K_PAD0_LEFTSHOULDER", K_PAD0_LEFTSHOULDER },
	{ "K_PAD0_RIGHTSHOULDER", K_PAD0_RIGHTSHOULDER },
	{ "K_PAD0_DPAD_UP", K_PAD0_DPAD_UP },
	{ "K_PAD0_DPAD_DOWN", K_PAD0_DPAD_DOWN },
	{ "K_PAD0_DPAD_LEFT", K_PAD0_DPAD_LEFT },
	{ "K_PAD0_DPAD_RIGHT", K_PAD0_DPAD_RIGHT },

	{ "K_PAD0_LEFTSTICK_LEFT", K_PAD0_LEFTSTICK_LEFT },
	{ "K_PAD0_LEFTSTICK_RIGHT", K_PAD0_LEFTSTICK_RIGHT },
	{ "K_PAD0_LEFTSTICK_UP", K_PAD0_LEFTSTICK_UP },
	{ "K_PAD0_LEFTSTICK_DOWN", K_PAD0_LEFTSTICK_DOWN },
	{ "K_PAD0_RIGHTSTICK_LEFT", K_PAD0_RIGHTSTICK_LEFT },
	{ "K_PAD0_RIGHTSTICK_RIGHT", K_PAD0_RIGHTSTICK_RIGHT },
	{ "K_PAD0_RIGHTSTICK_UP", K_PAD0_RIGHTSTICK_UP },
	{ "K_PAD0_RIGHTSTICK_DOWN", K_PAD0_RIGHTSTICK_DOWN },
	{ "K_PAD0_LEFTTRIGGER", K_PAD0_LEFTTRIGGER },
	{ "K_PAD0_RIGHTTRIGGER", K_PAD0_RIGHTTRIGGER },

	// Pseudo-key that brings the console down
	{ "K_CONSOLE", K_CONSOLE },

	{ "MAX_KEYS", MAX_KEYS },

	{ "K_CHAR_FLAG", K_CHAR_FLAG },
// End of key codes
	{ "CVAR_ARCHIVE", CVAR_ARCHIVE },
	{ "CVAR_USERINFO", CVAR_USERINFO },
	{ "CVAR_SERVERINFO", CVAR_SERVERINFO },
	{ "CVAR_SYSTEMINFO", CVAR_SYSTEMINFO },
	{ "CVAR_INIT", CVAR_INIT },
	{ "CVAR_LATCH", CVAR_LATCH },
	{ "CVAR_ROM", CVAR_ROM },
	{ "CVAR_USER_CREATED", CVAR_USER_CREATED },
	{ "CVAR_TEMP", CVAR_TEMP },
	{ "CVAR_CHEAT", CVAR_CHEAT },
	{ "CVAR_NORESTART", CVAR_NORESTART },

	{ "CVAR_SERVER_CREATED", CVAR_SERVER_CREATED },
	{ "CVAR_VM_CREATED", CVAR_VM_CREATED },
	{ "CVAR_PROTECTED", CVAR_PROTECTED },
	{ "CVAR_MODIFIED", CVAR_MODIFIED },
	{ "CVAR_NONEXISTENT", (int)CVAR_NONEXISTENT },

	{ "UIMENU_NONE", UIMENU_NONE },
	{ "UIMENU_MAIN", UIMENU_MAIN },
	{ "UIMENU_INGAME", UIMENU_INGAME },
	{ "UIMENU_NEED_CD", UIMENU_NEED_CD },
	{ "UIMENU_BAD_CD_KEY", UIMENU_BAD_CD_KEY },
	{ "UIMENU_TEAM", UIMENU_TEAM },
	{ "UIMENU_POSTGAME", UIMENU_POSTGAME },

#if defined( QC )
	{ "UIMENU_DEATH", UIMENU_DEATH },
	{ "UIMENU_CHAMPIONS", UIMENU_CHAMPIONS },
	{ "UIMENU_CHAMPIONS_INGAME", UIMENU_CHAMPIONS_INGAME },
#endif

	{ "RDF_NOWORLDMODEL", RDF_NOWORLDMODEL },
	{ "RDF_HYPERSPACE", RDF_HYPERSPACE },
	{ NULL, 0 },
};

static void lua_setintconstants( lua_State *L, const luaL_Integer *l ) {
	for ( ; l->name != NULL; l++ ) {
		lua_pushinteger( L, l->value );
		lua_setfield( L, -2, l->name );
	}
}

struct Lua_vec2_t {
	vec2_t value;

	Lua_vec2_t() {
		value[0] = value[1] = 0.0f;
	}
	Lua_vec2_t( const vec2_t &value ) {
		memcpy( this->value, value, sizeof( this->value ) );
	}
	template<unsigned index> float get() const { return value[index]; }
	template<unsigned index>  void set( float x ) { value[index] = x; }
};

struct Lua_vec3_t {
	vec3_t value;

	Lua_vec3_t() {
		value[0] = value[1] = value[2] = 0.0f;
	}
	Lua_vec3_t( const vec3_t &value ) {
		memcpy( this->value, value, sizeof( this->value ) );
	}

	template<unsigned index> float get() const { 
		return value[index];
	}
	template<unsigned index>  void set( float x ) { 
		value[index] = x;
	}

	std::string toString() const {
		std::ostringstream  os;
		os << "{" << value[0] << ", " << value[1] << ", " << value[2] << "}";
		return os.str();
		//return std::format( "({}, {}, {})", value[0], value[1], value[2] );
	}
};

struct Lua_color4ub {
	byte rgba[4];
	Lua_color4ub() {
		memset( rgba, 0, sizeof( rgba ) );
	}
	Lua_color4ub( const byte rgba[4] ) {
		memcpy( this->rgba, rgba, sizeof( this->rgba ) );
	}
	template<unsigned index> byte get() const { return rgba[index]; }
	template<unsigned index> void set( byte r ) { rgba[index] = r; }
};

struct Lua_polyVert_t: polyVert_t {
	Lua_polyVert_t() {
		memset( this, 0, sizeof( *this) );
	}
	Lua_polyVert_t( const Lua_polyVert_t &other ) {
		memcpy( this, &other, sizeof( *this) );
	}
	Lua_polyVert_t( const polyVert_t &other ) {
		memcpy( this, &other, sizeof( *this) );
	}
};

struct Lua_glconfig_t: glconfig_t {
	const char* get_renderer_string() const { return renderer_string; }
	void set_renderer_string( const char *v) { Q_strncpyz( renderer_string, v, sizeof( renderer_string )); }

	const char* get_vendor_string() const { return vendor_string; }
	void set_vendor_string( const char *v) { Q_strncpyz( vendor_string, v, sizeof( vendor_string )); }

	const char* get_version_string() const { return version_string; }
	void set_version_string( const char *v) { Q_strncpyz( version_string, v, sizeof( version_string )); }

	const char* get_extensions_string() const { return extensions_string; }
	void set_extensions_string( const char *v) { Q_strncpyz( extensions_string, v, sizeof( extensions_string )); }

	bool getDeviceSupportsGamma() const { return deviceSupportsGamma; }
	void setDeviceSupportsGamma( bool value ) { deviceSupportsGamma = value ? qtrue : qfalse; }

	bool getTextureEnvAddAvailable() const { return textureEnvAddAvailable; }
	void setTextureEnvAddAvailable( bool value ) { textureEnvAddAvailable= value ? qtrue : qfalse; }

	bool getIsFullscreen() const { return isFullscreen; }
	void setIsFullscreen( bool value ) { isFullscreen= value ? qtrue : qfalse; }

	bool getStereoEnabled() const { return stereoEnabled; }
	void setStereoEnabled( bool value ) { stereoEnabled= value ? qtrue : qfalse; }

	bool getSmpActive() const { return smpActive; }
	void setSmpActive( bool value ) { smpActive= value ? qtrue : qfalse; }
};

struct Lua_refEntity_t: refEntity_t {

	Lua_refEntity_t() {
		memset( this, 0, sizeof( Lua_refEntity_t ) );
	}

	Lua_refEntity_t( const Lua_refEntity_t& other ) {
		memcpy( this, &other, sizeof( Lua_refEntity_t ) );
	}

	bool getNonNormalizedAxes() const {
		return this->nonNormalizedAxes; 
	}

	void setNonNormalizedAxes( bool value ) {
		this->nonNormalizedAxes = value ? qtrue : qfalse;
	}
};

struct Lua_uiClientState_t: uiClientState_t {
	const char *getServerName() const { return this->servername; }
	void setServerName( const char *value ) { Q_strncpyz( this->servername, value, sizeof( this->servername ) ); }

	const char *getUpdateInfoString() const { return this->updateInfoString; }
	void setUpdateInfoString( const char *value)  { Q_strncpyz( this->updateInfoString, value, sizeof( this->updateInfoString ) ); }

	const char *getMessageString() const { return this->messageString; }
	void setMessageString( const char *value ) { Q_strncpyz( this->messageString, value, sizeof( this->messageString ) ); }

};

struct Lua_refdef_t : refdef_t {
	Lua_refdef_t() {
		memset( this, 0, sizeof( *this ) );
	}
};

//void			trap_R_AddRefEntityToScene( const refEntity_t *re );
static void lua_R_AddRefEntityToScene( const Lua_refEntity_t &re ) {
	trap_R_AddRefEntityToScene( &re );
}

//void			trap_R_RenderScene( const refdef_t *fd );
static void lua_R_RenderScene( const Lua_refdef_t &fd ) {
	trap_R_RenderScene( &fd );
}

//void			trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
static void lua_R_AddPolyToScene( qhandle_t hShader, const std::vector<polyVert_t> &verts ) {
	trap_R_AddPolyToScene( hShader, ( int )verts.size(), &verts[0] );
}

//void			trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
static void lua_R_AddLightToScene( const Lua_vec3_t org, float intensity, float r, float g, float b ) {
	trap_R_AddLightToScene( org.value, intensity, r, g, b );
}

//void			trap_GetClientState( uiClientState_t *state );
static const Lua_uiClientState_t &lua_GetClientState() {
	Lua_uiClientState_t state;
	trap_GetClientState( &state );
	return state;
}

//void			trap_GetGlconfig( glconfig_t *glconfig );
static Lua_glconfig_t lua_GetGlconfig() {
	Lua_glconfig_t glconfig;
	trap_GetGlconfig( &glconfig );
	return glconfig;
}


LUAMOD_API int luaopen_trap( lua_State *L ) {
	luabridge::Namespace global = getGlobalNamespace( L );
	luabridge::Namespace ns = global.beginNamespace( "trap" );

	ns = ns.beginClass<Lua_vmCvar_t>( "vmCvar_t" )
		.addConstructor<void(*)()>()
		.addProperty( "handle", &Lua_vmCvar_t::handle )
		.addProperty( "value", &Lua_vmCvar_t::value )
		.addProperty( "integer", &Lua_vmCvar_t::integer )
		.addProperty( "string", &Lua_vmCvar_t::getString, &Lua_vmCvar_t::setString )
	.endClass()
	.beginClass<Q_File>( "Q_File" )
		.addConstructor<void(*)()>()
		.addProperty( "length", &Q_File::getLength )
	.endClass()
	.beginClass<Lua_vec2_t>( "vec2_t" )
		.addConstructor<void(*)()>()
		.addProperty( "x", &Lua_vec2_t::get<0>, &Lua_vec2_t::set<0> )
		.addProperty( "y", &Lua_vec2_t::get<1>, &Lua_vec2_t::set<1> )
	.endClass()
	.beginClass<Lua_vec3_t>( "vec3_t" )
		.addConstructor<void(*)()>()
		.addProperty( "x", &Lua_vec3_t::get<0>, &Lua_vec3_t::set<0> )
		.addProperty( "y", &Lua_vec3_t::get<1>, &Lua_vec3_t::set<1> )
		.addProperty( "z", &Lua_vec3_t::get<2>, &Lua_vec3_t::set<2> )
		.addFunction( "__tostring", &Lua_vec3_t::toString )
	.endClass()
	.beginClass<Lua_color4ub>( "color4ub" )
		.addConstructor<void(*)()>()
		.addProperty( "r", &Lua_color4ub::get<0>, &Lua_color4ub::set<0> )
		.addProperty( "g", &Lua_color4ub::get<1>, &Lua_color4ub::set<1> )
		.addProperty( "b", &Lua_color4ub::get<2>, &Lua_color4ub::set<2> )
		.addProperty( "a", &Lua_color4ub::get<3>, &Lua_color4ub::set<3> )
	.endClass()
	.beginClass<Lua_refdef_t>("refdef_t")
		.addConstructor<void(*)()>()
		.addProperty( "x", &Lua_refdef_t::x )
		.addProperty( "y", &Lua_refdef_t::y )
		.addProperty( "width", &Lua_refdef_t::width )
		.addProperty( "height", &Lua_refdef_t::height )
		.addProperty( "fov_x", &Lua_refdef_t::fov_x )
		.addProperty( "fov_y", &Lua_refdef_t::fov_y )
		.addProperty( "vieworg",
			std::function<Lua_vec3_t( const Lua_refdef_t * )>( []( const Lua_refdef_t *rd ) { return Lua_vec3_t( rd->vieworg ); } ),
			std::function<void( Lua_refdef_t *, Lua_vec3_t )>( []( Lua_refdef_t *rd, Lua_vec3_t org ) { memcpy( rd->vieworg, org.value, sizeof( rd->vieworg ) ); } )
		)
		.addProperty( "viewaxis0",
			std::function<Lua_vec3_t( const Lua_refdef_t * )>( []( const Lua_refdef_t *rd ) { return Lua_vec3_t( rd->viewaxis[0] ); } ),
			std::function<void( Lua_refdef_t *, Lua_vec3_t )>( []( Lua_refdef_t *rd, Lua_vec3_t ax ) { memcpy( rd->viewaxis[0], ax.value, sizeof( rd->viewaxis[0] ) ); } )
		)
		.addProperty( "viewaxis1",
			std::function<Lua_vec3_t( const Lua_refdef_t * )>( []( const Lua_refdef_t *rd ) { return Lua_vec3_t( rd->viewaxis[1] ); } ),
			std::function<void( Lua_refdef_t *, Lua_vec3_t )>( []( Lua_refdef_t *rd, Lua_vec3_t ax ) { memcpy( rd->viewaxis[1], ax.value, sizeof( rd->viewaxis[1] ) ); } )
		)
		.addProperty( "viewaxis2",
			std::function<Lua_vec3_t( const Lua_refdef_t * )>( []( const Lua_refdef_t *rd ) { return Lua_vec3_t( rd->viewaxis[2] ); } ),
			std::function<void( Lua_refdef_t *, Lua_vec3_t )>( []( Lua_refdef_t *rd, Lua_vec3_t ax ) { memcpy( rd->viewaxis[2], ax.value, sizeof( rd->viewaxis[2] ) ); } )
		)
		.addProperty( "time", &Lua_refdef_t::time )
		.addProperty( "rdflags", &Lua_refdef_t::rdflags )
	.endClass()
	.beginClass<Lua_refEntity_t>( "refEntity_t" )
		.addConstructor<void(*)()>()
		.addProperty( "reType", &Lua_refEntity_t::reType )
		.addProperty( "renderfx", &Lua_refEntity_t::renderfx )
		.addProperty( "hModel", &Lua_refEntity_t::hModel )
		.addProperty( "lightingOrigin", 
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->lightingOrigin ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->lightingOrigin, lorig.value, sizeof( re->lightingOrigin ) ); } )
		)
		.addProperty( "shadowPlane", &Lua_refEntity_t::shadowPlane )
		.addProperty( "axis0", 
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->axis[0] ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->axis[0], lorig.value, sizeof( re->axis[0]) ); } )
		)
		.addProperty( "axis1",
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->axis[1] ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->axis[1], lorig.value, sizeof( re->axis[1] ) ); } )
		)
		.addProperty( "axis2",
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->axis[2] ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->axis[2], lorig.value, sizeof( re->axis[2] ) ); } )
		)
		.addProperty( "nonNormalizedAxes", &Lua_refEntity_t::getNonNormalizedAxes, &Lua_refEntity_t::setNonNormalizedAxes )
		.addProperty( "origin", 
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->origin ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->origin, lorig.value, sizeof( re->origin ) ); } )
		)
		.addProperty( "frame", &Lua_refEntity_t::frame )
		.addProperty( "oldorigin",
			std::function<Lua_vec3_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec3_t( re->oldorigin ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec3_t )>( []( Lua_refEntity_t *re, Lua_vec3_t lorig ) { memcpy( re->oldorigin, lorig.value, sizeof( re->oldorigin ) ); } )
		)
		.addProperty( "oldframe", &Lua_refEntity_t::frame )
		.addProperty( "skinNum", &Lua_refEntity_t::skinNum )
		.addProperty( "customSkin", &Lua_refEntity_t::customSkin )
		.addProperty( "customShader", &Lua_refEntity_t::customShader )
		.addProperty( "shaderRGBA",
			std::function<Lua_color4ub( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_color4ub( re->shaderRGBA ); } ),
			std::function<void( Lua_refEntity_t *, Lua_color4ub )>( []( Lua_refEntity_t *re, Lua_color4ub shaderRGBA ) { memcpy( re->shaderRGBA, shaderRGBA.rgba, sizeof( re->shaderRGBA ) ); } )
		)
		.addProperty( "shaderTexCoord",
			std::function<Lua_vec2_t( const Lua_refEntity_t * )>( []( const Lua_refEntity_t *re ) { return Lua_vec2_t( re->shaderTexCoord ); } ),
			std::function<void( Lua_refEntity_t *, Lua_vec2_t )>( []( Lua_refEntity_t *re, Lua_vec2_t texCoord) { memcpy( re->shaderTexCoord, texCoord.value, sizeof( re->shaderTexCoord ) ); } )
		)
		.addProperty( "shaderTime", &Lua_refEntity_t::shaderTime )
		.addProperty( "radius", &Lua_refEntity_t::radius )
		.addProperty( "rotation", &Lua_refEntity_t::rotation )
	.endClass()
	.beginClass<Lua_polyVert_t>( "polyVert_t" )
		.addConstructor<void(*)()>()
		.addProperty( "xyz", 
			std::function<Lua_vec3_t( const Lua_polyVert_t * )>( []( const Lua_polyVert_t *pv ) { return Lua_vec3_t( pv->xyz ); } ),
			std::function<void( Lua_polyVert_t *, Lua_vec3_t )>( []( Lua_polyVert_t *pv, Lua_vec3_t xyz ) { memcpy( pv->xyz, xyz.value, sizeof( pv->xyz ) ); } )
		)
		.addProperty( "st",
			std::function<Lua_vec2_t( const Lua_polyVert_t * )>( []( const Lua_polyVert_t *pv ) { return Lua_vec2_t( pv->st ); } ),
			std::function<void( Lua_polyVert_t *, Lua_vec2_t )>( []( Lua_polyVert_t *pv, Lua_vec2_t st ) { memcpy( pv->st, st.value, sizeof( pv->st ) ); } )
		)
		.addProperty( "modulate",
			std::function<Lua_color4ub( const Lua_polyVert_t * )>( []( const Lua_polyVert_t *pv ) { return Lua_color4ub( pv->modulate ); } ),
			std::function<void( Lua_polyVert_t *, Lua_color4ub )>( []( Lua_polyVert_t *pv, Lua_color4ub mod ) { memcpy( pv->modulate, mod.rgba, sizeof( pv->modulate ) ); } )
		)
	.endClass()
	.beginClass<Lua_glconfig_t>( "glconfig_t" )
		.addConstructor<void(*)()>()
		.addProperty( "renderer_string", &Lua_glconfig_t::get_renderer_string )
		.addProperty( "vendor_string", &Lua_glconfig_t::get_vendor_string )
		.addProperty( "version_string", &Lua_glconfig_t::get_version_string )
		.addProperty( "extensions_string", &Lua_glconfig_t::get_extensions_string )
		.addProperty( "colorBits", &Lua_glconfig_t::colorBits )
		.addProperty( "depthBits", &Lua_glconfig_t::depthBits )
		.addProperty( "stencilBits", &Lua_glconfig_t::stencilBits )
		.addProperty( "drtiverType", &Lua_glconfig_t::driverType )
		.addProperty( "hardwareType", &Lua_glconfig_t::hardwareType )
		.addProperty( "deviceSupportsGamma", &Lua_glconfig_t::getDeviceSupportsGamma )
		.addProperty( "textureCompression_t", &Lua_glconfig_t::textureCompression )
		.addProperty( "textureEnvAddAvailable", &Lua_glconfig_t::getTextureEnvAddAvailable )
		.addProperty( "vidWidth", &Lua_glconfig_t::vidWidth )
		.addProperty( "vidHeight", &Lua_glconfig_t::vidHeight )
		.addProperty( "windowAspect", &Lua_glconfig_t::windowAspect )
		.addProperty( "displayFrequency", &Lua_glconfig_t::displayFrequency )
		.addProperty( "isFullscreen", &Lua_glconfig_t::getIsFullscreen )
		.addProperty( "stereoEnabled", &Lua_glconfig_t::getStereoEnabled )
		.addProperty( "smpActive", &Lua_glconfig_t::getSmpActive )
	.endClass()
	.beginClass<Lua_uiClientState_t>( "uiClientState_t" )
		.addProperty( "connstate", &Lua_uiClientState_t::connState )
		.addProperty( "connectPacketCount", &Lua_uiClientState_t::connectPacketCount )
		.addProperty( "clientNum", &Lua_uiClientState_t::clientNum )
		.addProperty( "servername", &Lua_uiClientState_t::getServerName, &Lua_uiClientState_t::setServerName )
		.addProperty( "updateInfoString", &Lua_uiClientState_t::getUpdateInfoString, &Lua_uiClientState_t::setUpdateInfoString )
		.addProperty( "servername", &Lua_uiClientState_t::getMessageString, &Lua_uiClientState_t::setMessageString )
	.endClass();

	for ( const luaL_Integer *l = uilib_intconst; l->name != NULL; l++ ) {
		ns.addConstant( l->name, l->value );
	}
	ns.addFunction( "Print", lua_Print );
	ns.addFunction( "Error", lua_Error );
	ns.addFunction( "Milliseconds", lua_Milliseconds );
	ns.addFunction( "Cvar_Register", lua_Cvar_Register );
	ns.addFunction( "Cvar_Update", lua_Cvar_Update );
	ns.addFunction( "Cvar_Set", lua_Cvar_Set );
	ns.addFunction( "Cvar_VariableValue", lua_Cvar_VariableValue );
	ns.addFunction( "Cvar_VariableStringBuffer", lua_Cvar_VariableStringBuffer );
	ns.addFunction( "Cvar_SetValue", lua_Cvar_SetValue );
	ns.addFunction( "Cvar_Reset", lua_Cvar_Reset );
	ns.addFunction( "Cvar_Create", lua_Cvar_Create );
	ns.addFunction( "Cvar_InfoStringBuffer", lua_Cvar_InfoStringBuffer );
	ns.addFunction( "Argc", lua_Argc );
	ns.addFunction( "Argv", lua_Argv );
	ns.addFunction( "Cmd_ExecuteText", lua_Cmd_ExecuteText );
	ns.addFunction( "GetClientState", lua_GetClientState );
	ns.addFunction( "FS_FOpenFile", lua_FS_FOpenFile );
	ns.addFunction( "FS_Read", lua_FS_Read );
	ns.addFunction( "FS_Write", lua_FS_Write );
	ns.addFunction( "FS_FCloseFile", lua_FS_FCloseFile );
	ns.addFunction( "FS_GetFileList", lua_FS_GetFileList );
	ns.addFunction( "FS_Seek", lua_FS_Seek );
	ns.addFunction( "R_RegisterModel", lua_R_RegisterModel );
	ns.addFunction( "R_RegisterSkin", lua_R_RegisterSkin );
	ns.addFunction( "R_RegisterShaderNoMip", lua_R_RegisterShaderNoMip );
	ns.addFunction( "R_ClearScene", lua_R_ClearScene );
	ns.addFunction( "R_AddRefEntityToScene", lua_R_AddRefEntityToScene );
	ns.addFunction( "R_AddPolyToScene", lua_R_AddPolyToScene );
	ns.addFunction( "R_AddLightToScene", lua_R_AddLightToScene );
	ns.addFunction( "R_RenderScene", lua_R_RenderScene );
	ns.addFunction( "R_SetColor", lua_R_SetColor );
	ns.addFunction( "R_DrawStretchPic", lua_R_DrawStretchPic );
	ns.addFunction( "UpdateScreen", lua_UpdateScreen );
	//ns.addFunction( "CM_LerpTag", lua_CM_LerpTag );
	ns.addFunction( "S_StartLocalSound", lua_S_StartLocalSound );
	ns.addFunction( "S_RegisterSound", lua_S_RegisterSound );
	ns.addFunction( "Key_KeynumToStringBuf", lua_Key_KeynumToStringBuf );
	ns.addFunction( "Key_GetBindingBuf", lua_Key_GetBindingBuf );
	ns.addFunction( "Key_SetBinding", lua_Key_SetBinding );
	ns.addFunction( "Key_IsDown", lua_Key_IsDown );
	ns.addFunction( "Key_GetOverstrikeMode", lua_Key_GetOverstrikeMode );
	ns.addFunction( "Key_SetOverstrikeMode", lua_Key_SetOverstrikeMode );
	ns.addFunction( "Key_ClearStates", lua_Key_ClearStates );
	ns.addFunction( "Key_GetCatcher", lua_Key_GetCatcher );
	ns.addFunction( "Key_SetCatcher", lua_Key_SetCatcher );
	ns.addFunction( "GetClipboardData", lua_GetClipboardData );
	ns.addFunction( "GetGlconfig", lua_GetGlconfig );
	ns.addFunction( "GetConfigString", lua_GetConfigString );
	ns.addFunction( "LAN_GetServerCount", lua_LAN_GetServerCount );
	ns.addFunction( "LAN_GetServerAddressString", lua_LAN_GetServerAddressString );
	ns.addFunction( "LAN_GetServerInfo", lua_LAN_GetServerInfo );
	ns.addFunction( "LAN_GetPingQueueCount", lua_LAN_GetPingQueueCount );
	ns.addFunction( "LAN_ServerStatus", lua_LAN_ServerStatus );
	ns.addFunction( "LAN_ClearPing", lua_LAN_ClearPing );
	ns.addFunction( "LAN_GetPing", lua_LAN_GetPing );
	ns.addFunction( "LAN_GetPingInfo", lua_LAN_GetPingInfo );
	ns.addFunction( "MemoryRemaining", lua_MemoryRemaining );
	ns.addFunction( "GetCDKey", lua_GetCDKey );
	ns.addFunction( "SetCDKey", lua_SetCDKey );
	ns.addFunction( "VerifyCDKey", lua_VerifyCDKey );
	ns.addFunction( "SetPbClStatus", lua_SetPbClStatus );
	ns.addFunction( "R_SetMatrix", lua_R_SetMatrix );

	ns.endNamespace();

	return 1;
}
