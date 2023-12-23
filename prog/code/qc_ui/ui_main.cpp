#include "../qcommon/q_shared.h"
#include "ui_public.h"
#include "ui_local.h"

#undef DotProduct
#include <RmlUi/Core.h>

glconfig_t g_glConfig;

void UI_Init( void ) {
	trap_GetGlconfig( &g_glConfig );
	Rml::Initialise();
}

void UI_Shutdown( void ) {
	Rml::Shutdown();
}

void UI_KeyEvent( int key, int down ) {
}

void UI_MouseEvent( int dx, int dy ) {
}

void UI_Refresh( int realtime ) {

}

qboolean UI_IsFullscreen( void ) {
	return qtrue;
}

void UI_SetActiveMenu( uiMenuCommand_t menu ) {
}

qboolean UI_ConsoleCommand( int realTime ) {
	return qtrue;
}

void UI_DrawConnectScreen( qboolean overlay ) {
}
