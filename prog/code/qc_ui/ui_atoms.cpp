#include "../qcommon/q_shared.h"
#include "ui_public.h"
#include "ui_local.h"

const char *UI_Argv( int n ) {
	static char	buffer[MAX_STRING_CHARS];
	trap_Argv( n, buffer, sizeof( buffer ) );
	return buffer;
}
