#include <RmlUi/Core.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include "../qcommon/q_shared.h"
#include "ui_public.h"
#include "ui_local.h"
#undef DotProduct

glconfig_t g_glConfig;
Rml::QRenderInterface g_renderInterface;
Rml::QFileInterface g_fileInterface;
Rml::QSystemInterface g_systemInterface;

void UI_InitLua( void );
void UI_ShutdownLua( void );

void UI_Init( void ) {
	glconfig_t glc;

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
