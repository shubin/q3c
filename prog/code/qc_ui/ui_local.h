#ifndef UI_LOCAL_H
#define UI_LOCAL_H

#define UI_API_VERSION	4

void UI_Init( void );
void UI_Shutdown( void );
void UI_KeyEvent( int key, int down );
void UI_MouseEvent( int dx, int dy );
void UI_Refresh( int realtime );
qboolean UI_IsFullscreen( void );
void UI_SetActiveMenu( uiMenuCommand_t menu );
qboolean UI_ConsoleCommand( int realTime );
void UI_DrawConnectScreen( qboolean overlay );

#endif // UI_LOCAL_H
