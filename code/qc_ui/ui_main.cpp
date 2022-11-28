#include "ui_local.h"

Q_EXPORT intptr_t vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {
  switch ( command ) {
	  case UI_GETAPIVERSION:
		  return UI_API_VERSION;

	  case UI_INIT:
		  UI_Init();
		  return 0;

	  case UI_SHUTDOWN:
		  UI_Shutdown();
		  return 0;

	  case UI_KEY_EVENT:
		  UI_KeyEvent( arg0, arg1 );
		  return 0;

	  case UI_MOUSE_EVENT:
		  UI_MouseEvent( arg0, arg1 );
		  return 0;

	  case UI_REFRESH:
		  UI_Refresh( arg0 );
		  return 0;

	  case UI_IS_FULLSCREEN:
		  return UI_IsFullscreen() ? 1 : 0;

	  case UI_SET_ACTIVE_MENU:
		  UI_SetActiveMenu( (uiMenuCommand_t)arg0 );
		  return 0;

	  case UI_CONSOLE_COMMAND:
		  return UI_ConsoleCommand( arg0 ) ? 1 : 0;

	  case UI_DRAW_CONNECT_SCREEN:
		  UI_DrawConnectScreen( (qboolean)arg0 );
		  return 0;
	  case UI_HASUNIQUECDKEY: 
	    return qfalse;

	}
	return -1;
}
