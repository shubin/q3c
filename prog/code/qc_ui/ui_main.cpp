#include <RmlUi/Core.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>

extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"
#undef DotProduct

float r_brightness;
glconfig_t g_glConfig;
Rml::QRenderInterface g_renderInterface;
Rml::QFileInterface g_fileInterface;
Rml::QSystemInterface g_systemInterface;

void UI_InitLua( void );
void UI_ShutdownLua( void );
void UI_CvarChangedLua( void );

// ui_localisation.cpp
qboolean UI_LoadLocalisation( const char *path );

void UI_Init( void ) {
	glconfig_t glc;

	r_brightness = trap_Cvar_VariableValue( "r_brightness" );
	trap_Cvar_Watch( "r_brightness", qtrue );
	trap_Cvar_Register( &ui_shell, "ui_shell", "shell", CVAR_INIT );
	trap_Cvar_Register( &ui_language, "ui_language", "english", CVAR_ARCHIVE );
	UI_LoadLocalisation( va( "%s/locale/%s.po", ui_shell.string, ui_language.string ) );
	trap_GetGlconfig( &glc );
	g_renderInterface.Initialize( glc.vidWidth, glc.vidHeight );
	Rml::SetRenderInterface( &g_renderInterface );
	Rml::SetFileInterface( &g_fileInterface );
	Rml::SetSystemInterface( &g_systemInterface );
	Rml::Initialise();
	UI_InitLua();
}

void UI_Shutdown( void ) {
	UI_ShutdownLua();
	Rml::Shutdown();
	g_renderInterface.Shutdown();
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *QDECL va( char *format, ... ) {
	va_list		argptr;
	static char string[2][32000]; // in case va is called by nested functions
	static int	index = 0;
	char *buf;

	buf = string[index & 1];
	index++;

	va_start( argptr, format );
	vsnprintf( buf, sizeof( *string ), format, argptr );
	va_end( argptr );

	return buf;
}

const char *UI_ConvertPath( const char *path ) {
	static char qpath[MAX_QPATH];

	if ( path[0] == '/' ) {
		strncpy( qpath, path, MAX_QPATH - 1 );
	} else {
		strncpy( qpath, ui_shell.string, MAX_QPATH - 1 );
		strcat_s( qpath, "/" );
		strcat_s( qpath, path );
	}
	qpath[MAX_QPATH - 1] = '\0';
	return qpath;
}

void UI_CvarChanged( void ) {
	char var_name[64];
	trap_Argv( 0, var_name, 63 );
	var_name[63] = 0;
	if ( !strcmp( var_name, "r_brightness" ) ) {
		r_brightness = trap_Cvar_VariableValue( "r_brightness" );
	}
	UI_CvarChangedLua();
}
