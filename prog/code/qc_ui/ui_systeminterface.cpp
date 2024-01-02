extern "C" {
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "ui_public.h"
}
#undef DotProduct

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/SystemInterface.h>

#include "ui_local.h"

using namespace Rml;

QSystemInterface::QSystemInterface() {
}

QSystemInterface::~QSystemInterface() {
}

double QSystemInterface::GetElapsedTime() {
	return 0.001 * trap_Milliseconds();
}

bool QSystemInterface::LogMessage( Log::Type type, const String &message ) {
	String prefix;

	switch ( type ) {
		case Log::LT_ALWAYS:	prefix = "^4shell: "; break;
		case Log::LT_ERROR:		prefix = "^1shell: "; break;
		case Log::LT_ASSERT:	prefix = "^2shell: "; break;
		case Log::LT_WARNING:	prefix = "^3shell: "; break;
		case Log::LT_INFO:		prefix = "^5shell: "; break;
		case Log::LT_DEBUG:		prefix = "^6shell: "; break;
	}
	trap_Print( ( prefix + message + "\n" ).c_str());
	return true;
}

void UI_SetMousePointer( const char *cursor );

void QSystemInterface::SetMouseCursor( const String &cursor_name ) {
	UI_SetMousePointer( cursor_name.c_str() );
}

void QSystemInterface::JoinPath( String &translated_path, const String &document_path, const String &path ) {
	if ( path.size() != 0 && path[0] == '/' ) {
		translated_path = path;
	} else {
		SystemInterface::JoinPath( translated_path, document_path, path );
	}
}

/*
int QSystemInterface::TranslateString( String &translated, const String &input ) {
	//translated
}

void QSystemInterface::SetMouseCursor( const String &cursor_name ) {
}

void QSystemInterface::SetClipboardText( const String &text ) {
}

void QSystemInterface::GetClipboardText( String &text ) {
}
*/
