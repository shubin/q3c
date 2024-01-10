extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"
#include "ui_poparser.h"

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, std::string> msgmap;
static msgmap messages;

bool UI_LocaliseString( std::string &translated, const std::string &input ) {
	auto it = messages.find( std::string( input ) );
	if ( it == messages.end() ) {
		return false;
	}
	translated = it->second;
	return true;
}

static void addmsg( void *storage, const char *msgid, const char *msgstr ) {
	(*((msgmap*)storage))[std::string( msgid )] = std::string( msgstr );
}

static void read( void *file, void *buffer, int amount ) {
	trap_FS_Read( buffer, amount, (fileHandle_t)(intptr_t)file );
}

qboolean UI_LoadLocalisation( const char *path ) {
	fileHandle_t handle;
	int len;
	if ( ( len = trap_FS_FOpenFile( path, &handle, FS_READ ) ) < 0 ) {
		trap_Print( va( "^1Error: cannot open localisation file: %s\n", path ) );
		return qfalse;
	}
	
	std::unordered_map<std::string, std::string> messages;

	bool success = po_parse( read, len, (void*)( intptr_t )handle, addmsg, &messages, trap_Print );

	trap_FS_FCloseFile( handle );

	if ( success ) {
		::messages.swap( messages );
		return qtrue;
	}
	trap_Print( "^1Error parsing localisation file\n" );
	return qfalse;
}
