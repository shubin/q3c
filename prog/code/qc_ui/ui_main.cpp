#include "../qcommon/q_shared.h"
#include "ui_public.h"
#undef DotProduct
#include <RmlUi/Core.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include "ui_local.h"

glconfig_t g_glConfig;
Rml::QRenderInterface g_renderInterface;
Rml::QFileInterface g_fileInterface;
Rml::QSystemInterface g_systemInterface;

Rml::Context *g_context;
Rml::ElementDocument *g_mainDoc;

static
void UI_LoadFonts() {
	const Rml::String directory = "assets/";

	struct FontFace {
		const char *filename;
		bool fallback_face;
	};
	FontFace font_faces[] = {
		{"LatoLatin-Regular.ttf", false},
		{"LatoLatin-Italic.ttf", false},
		{"LatoLatin-Bold.ttf", false},
		{"LatoLatin-BoldItalic.ttf", false},
		{"NotoEmoji-Regular.ttf", true},
	};

	for ( const FontFace &face : font_faces )
		Rml::LoadFontFace( directory + face.filename, face.fallback_face );
}

void UI_Init( void ) {
	trap_GetGlconfig( &g_glConfig );

	g_renderInterface.Initialize( g_glConfig.vidWidth, g_glConfig.vidHeight );

	Rml::SetRenderInterface( &g_renderInterface );
	Rml::SetFileInterface( &g_fileInterface );
	Rml::SetSystemInterface( &g_systemInterface );
	Rml::Initialise();

	UI_LoadFonts();

	g_context = Rml::CreateContext( "main", Rml::Vector2i( g_glConfig.vidWidth, g_glConfig.vidHeight ) );
	if ( g_context == NULL ) {
		trap_Error( "RmlUi: Context creation failed\n" );
	}

	g_mainDoc = g_context->LoadDocument( "ui/tutorial.rml" );
	if ( g_mainDoc == NULL ) {
		trap_Error( "RmlUi: Cannot load main menu\n" );
	}
	g_mainDoc->Show();
}

void UI_Shutdown( void ) {
	Rml::Shutdown();
	g_renderInterface.Shutdown();
}

void UI_KeyEvent( int key, int down ) {
}

void UI_MouseEvent( int dx, int dy ) {
}

void UI_Refresh( int realtime ) {
	if ( !( trap_Key_GetCatcher() & KEYCATCH_UI ) ) {
		return;
	}
	g_context->Update();
	g_context->Render();
	g_renderInterface.Flush();
}

qboolean UI_IsFullscreen( void ) {
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
