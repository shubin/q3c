#include "../qcommon/q_shared.h"
#include "ui_public.h"
#include "ui_local.h"

#undef DotProduct
#include <RmlUi/Core.h>

Q_EXPORT intptr_t vmMain( intptr_t *args ) {
	switch ( args[0] ) {
		case UI_GETAPIVERSION:
			return UI_API_VERSION;

		case UI_INIT:
			UI_Init();
			return 0;

		case UI_SHUTDOWN:
			UI_Shutdown();
			return 0;

		case UI_KEY_EVENT:
			UI_KeyEvent( args[1], args[2] );
			return 0;

		case UI_MOUSE_EVENT:
			UI_MouseEvent( args[1], args[2] );
			return 0;

		case UI_REFRESH:
			UI_Refresh( args[1] );
			return 0;

		case UI_IS_FULLSCREEN:
			return UI_IsFullscreen();

		case UI_SET_ACTIVE_MENU:
			UI_SetActiveMenu( (uiMenuCommand_t)args[1] );
			return 0;

		case UI_CONSOLE_COMMAND:
			return UI_ConsoleCommand( args[1] );

		case UI_DRAW_CONNECT_SCREEN:
			UI_DrawConnectScreen( (qboolean)args[1] );
			return 0;

		case UI_HASUNIQUECDKEY:
			return qfalse;

		case UI_CVAR_CHANGED:
			UI_CvarChanged();
			return 0;
	}

	return -1;
}
