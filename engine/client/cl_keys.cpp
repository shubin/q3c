/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "client.h"
#include "client_help.h"

/*

key up events are sent even if in console mode

*/

history_t	g_history;
field_t		g_consoleField;
field_t		chatField;
qbool		chat_team;
int			chat_playerNum;
int			anykeydown;


static qbool key_overstrikeMode;


typedef struct {
	qbool	down;
	qbool	sendMinusCmd;	// if +cmd was sent on first key press
	int		repeats;		// if > 1, it is autorepeating
	char	*binding;
} qkey_t;

#define MAX_KEYS 256
static qkey_t keys[MAX_KEYS];


typedef struct {
	const char* name;
	int keynum;
} keyname_t;


// names not in this list can either be lowercase ascii, or '0xnn' hex sequences
static const keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"COMMAND", K_COMMAND},

	{"CAPSLOCK", K_CAPSLOCK},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},
	{"F13", K_F13},
	{"F14", K_F14},
	{"F15", K_F15},
	{"F16", K_F16},
	{"F17", K_F17},
	{"F18", K_F18},
	{"F19", K_F19},
	{"F20", K_F20},
	{"F21", K_F21},
	{"F22", K_F22},
	{"F23", K_F23},
	{"F24", K_F24},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},
	{"MOUSE9", K_MOUSE9},

	{"MWHEELUP",	K_MWHEELUP },
	{"MWHEELDOWN",	K_MWHEELDOWN },

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_PLUS",			K_KP_PLUS },
	{"KP_NUMLOCK",		K_KP_NUMLOCK },
	{"KP_STAR",			K_KP_STAR },
	{"KP_EQUALS",		K_KP_EQUALS },

	{"PAUSE", K_PAUSE},
	
	{"SEMICOLON", ';'},	// because a raw semicolon separates commands

	{"BACKSLASH", K_BACKSLASH},

	{NULL,0}
};


/*
=============================================================================

EDIT FIELDS

=============================================================================
*/


// handles horizontal scrolling and cursor blinking
// position and char sizes are in pixels

void Field_Draw( field_t* edit, int x, int y, int cw, int ch, qbool extColors, qbool drawCaret )
{
	int		len;
	int		drawLen;
	int		prestep;
	int		cursorChar;
	char	str[MAX_STRING_CHARS];
	int		i;
	int		colorCode;
	const float* firstColor;

	drawLen = edit->widthInChars + 1;
	len = strlen( edit->buffer );

	// guarantee that cursor will be visible
	if ( len <= drawLen ) {
		prestep = 0;
	} else {
		if ( edit->scroll + drawLen > len ) {
			edit->scroll = len - drawLen;
			if ( edit->scroll < 0 ) {
				edit->scroll = 0;
			}
		}
		prestep = edit->scroll;
	}

	if ( prestep + drawLen > len ) {
		drawLen = len - prestep;
	}

	// extract <drawLen> characters from the field at <prestep>
	if ( drawLen >= MAX_STRING_CHARS ) {
		Com_Error( ERR_DROP, "drawLen >= MAX_STRING_CHARS" );
	}

	colorCode = COLOR_WHITE;
	for ( i = prestep - 1; i >= 0; --i ) {
		if ( Q_IsColorString( &edit->buffer[i] ) ) {
			colorCode = edit->buffer[i + 1];
			break;
		}
	}

	Com_Memcpy( str, edit->buffer + prestep, drawLen );
	str[ drawLen ] = 0;

	firstColor = extColors ? ConsoleColorFromChar( colorCode ) : ColorFromChar( colorCode );
	SCR_DrawStringEx( x, y, cw, ch, str, extColors ? DSC_CONSOLE : DSC_NORMAL, qtrue, firstColor );

	if ( !drawCaret || Sys_Milliseconds() % 500 < 250 ) {
		return;
	}

	if ( key_overstrikeMode ) {
		cursorChar = 11;
	} else {
		cursorChar = 10;
	}

	i = drawLen - strlen( str );
	firstColor = extColors ? ConsoleColorFromChar( COLOR_WHITE ) : ColorFromChar( COLOR_WHITE );
	re.SetColor( firstColor );
	SCR_DrawChar( x + ( edit->cursor - prestep - i ) * cw, y, cw, ch, cursorChar );
	re.SetColor( NULL );
}


///////////////////////////////////////////////////////////////


static void Field_CharEvent( field_t *edit, int ch );


static void Field_Paste( field_t *edit )
{
	char* cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		return;
	}

	// send as if typed, so insert / overstrike works properly
	for (int i = 0; cbd[i]; ++i) {
		Field_CharEvent( edit, cbd[i] );
	}

	Z_Free( cbd );
}


static void Field_CharEvent( field_t *edit, int ch )
{
	int		len;

	if ( ch == 'v' - 'a' + 1 ) {	// ctrl-v is paste
		Field_Paste( edit );
		return;
	}

	if ( ch == 'c' - 'a' + 1 ) {	// ctrl-c clears the field
		Field_Clear( edit );
		return;
	}

	len = strlen( edit->buffer );

	if ( ch == 'h' - 'a' + 1 ) {	// ctrl-h is backspace
		if ( edit->cursor > 0 ) {
			memmove( edit->buffer + edit->cursor - 1, 
				edit->buffer + edit->cursor, len + 1 - edit->cursor );
			edit->cursor--;
			if ( edit->cursor < edit->scroll )
			{
				edit->scroll--;
			}
		}
		return;
	}

	if ( ch == 'a' - 'a' + 1 ) {	// ctrl-a is home
		edit->cursor = 0;
		edit->scroll = 0;
		return;
	}

	if ( ch == 'e' - 'a' + 1 ) {	// ctrl-e is end
		edit->cursor = len;
		edit->scroll = edit->cursor - edit->widthInChars;
		return;
	}

	//
	// ignore any other non printable chars
	//
	if ( ch < 32 ) {
		return;
	}

	if ( key_overstrikeMode ) {
		if ( edit->cursor == MAX_EDIT_LINE - 1 )
			return;
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	} else {	// insert mode
		if ( len == MAX_EDIT_LINE - 1 ) {
			return; // all full
		}
		memmove( edit->buffer + edit->cursor + 1, 
			edit->buffer + edit->cursor, len + 1 - edit->cursor );
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}

	if ( edit->cursor >= edit->widthInChars ) {
		edit->scroll++;
	}

	if ( edit->cursor == len + 1) {
		edit->buffer[edit->cursor] = 0;
	}
}


/*
Performs the basic line editing functions for the console, in-game talk, and menu fields
Key events are used for non-printable characters, others are gotten from char events.
*/
static void Field_KeyDownEvent( field_t *edit, int key )
{
	int		len;

	// shift-insert is paste
	if ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && keys[K_SHIFT].down ) {
		Field_Paste( edit );
		return;
	}

	len = strlen( edit->buffer );
	if ( len == 0 && ( key == K_LEFTARROW || key == K_RIGHTARROW ) )
		return;

	if ( key == K_DEL ) {
		if ( edit->cursor < len )
			memmove( edit->buffer + edit->cursor, edit->buffer + edit->cursor + 1, len - edit->cursor );
	} else if ( key == K_RIGHTARROW ) {
		if ( keys[K_CTRL].down ) {
			const char* c = edit->buffer + edit->cursor;
			if ( *c != ' ' )
				while ( *c != '\0' && *c != ' ' ) c++;
			while ( *c != '\0' && *c == ' ' ) c++;
			edit->cursor = (int)( c - edit->buffer );
		} else if ( keys[K_SHIFT].down ) {
			Cmd_TokenizeString( edit->buffer );
			const int currentArg = Cmd_ArgIndexFromOffset( edit->cursor );
			const int lastArg = Cmd_Argc() - 1;
			if ( currentArg == lastArg )
				edit->cursor = len;
			else if ( currentArg >= 0 && currentArg < lastArg )
				edit->cursor = Cmd_ArgOffset( currentArg + 1 );
		} else {
			if ( edit->cursor < len )
				edit->cursor++;
		}
	} else if ( key == K_LEFTARROW ) {
		if ( keys[K_CTRL].down ) {
			const char* const s = edit->buffer;
			const char* c = s + edit->cursor;
			if ( c > s && c[-1] == ' ' )
				while ( c > s && c[-1] == ' ' ) c--;
			while ( c > s && c[-1] != ' ' ) c--;
			edit->cursor = (int)( c - edit->buffer );
		} else if ( keys[K_SHIFT].down ) {
			Cmd_TokenizeString( edit->buffer );
			int currentArg = Cmd_ArgIndexFromOffset( edit->cursor );
			if ( currentArg == -1 ) // past the last argument?
				currentArg = Cmd_Argc() - 1;
			const int currentArgOff = Cmd_ArgOffset( currentArg );
			if ( currentArgOff < edit->cursor )
				edit->cursor = currentArgOff;
			else if ( currentArg > 0 )
				edit->cursor = Cmd_ArgOffset( currentArg - 1 );
		} else {
			if ( edit->cursor > 0 )
				edit->cursor--;
		}
	} else if ( key == K_HOME || ( tolower(key) == 'a' && keys[K_CTRL].down ) ) {
		edit->cursor = 0;
	} else if ( key == K_END || ( tolower(key) == 'e' && keys[K_CTRL].down ) ) {
		edit->cursor = len;
	} else if ( key == K_INS ) {
		key_overstrikeMode = !key_overstrikeMode;
	}

	// fix up the scroll after we're done changing the cursor position
	if ( edit->cursor < edit->scroll ) {
		edit->scroll = edit->cursor;
	} else if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len ) {
		edit->scroll = edit->cursor - edit->widthInChars + 1;
	}
}


/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/


// handles history and console scrollback

static void Console_Key( int key )
{
	if ( Con_HandleMarkMode( keys[K_CTRL].down, keys[K_SHIFT].down, key ) ) {
		return;
	}

	// clear auto-completion buffer when not pressing tab
	if ( key != K_TAB )
		g_consoleField.acOffset = 0;

	// ctrl-L clears screen
	if ( key == 'l' && keys[K_CTRL].down ) {
		Cbuf_AddText( "clear\n" );
		return;
	}

	// enter finishes the line
	if ( key == K_ENTER || key == K_KP_ENTER ) {
		g_consoleField.acOffset = 0;

		// if not in the game explicitly prepend a slash if needed
		if ( (cls.state != CA_ACTIVE) && g_consoleField.buffer[0]
				&& (g_consoleField.buffer[0] != '\\') && (g_consoleField.buffer[0] != '/') ) {
			char temp[MAX_STRING_CHARS];
			Q_strncpyz( temp, g_consoleField.buffer, sizeof( temp ) );
			Com_sprintf( g_consoleField.buffer, sizeof( g_consoleField.buffer ), "\\%s", temp );
			g_consoleField.cursor++;
		}

		Com_Printf( "]%s\n", g_consoleField.buffer );

		// leading slash is an explicit command
		if ( g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/' ) {
			Cbuf_AddText( g_consoleField.buffer+1 );
			Cbuf_AddText( "\n" );
		} else {
			// other text will be chat messages
			if ( !g_consoleField.buffer[0] ) {
				return;	// empty lines just scroll the console without adding to history
			} else {
				Cbuf_AddText( "cmd say " );
				Cbuf_AddText( g_consoleField.buffer );
				Cbuf_AddText( "\n" );
			}
		}

		History_SaveCommand( &g_history, &g_consoleField );

		Field_Clear( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;

		if ( cls.state == CA_DISCONNECTED ) {
			SCR_UpdateScreen(); // force an update, because the command may take some time
		}

		return;
	}

	// command completion

	if (key == K_TAB) {
		Field_AutoComplete( &g_consoleField, qtrue );
		return;
	}

	// command history (ctrl-p ctrl-n for unix style)

	if ( ( key == K_UPARROW ) ||
			( ( tolower(key) == 'p' ) && keys[K_CTRL].down ) ) {
		g_consoleField.acOffset = 0;
		History_GetPreviousCommand( &g_consoleField, &g_history );
		return;
	}

	if ( ( key == K_DOWNARROW ) ||
			( ( tolower(key) == 'n' ) && keys[K_CTRL].down ) ) {
		g_consoleField.acOffset = 0;
		History_GetNextCommand( &g_consoleField, &g_history, g_console_field_width );
		return;
	}

	// console scrolling (faster if +SHIFT)
	if ( key == K_MWHEELUP ) {
		Con_ScrollLines( keys[K_SHIFT].down ? -6 : -2 );
		return;
	}

	if ( key == K_MWHEELDOWN ) {
		Con_ScrollLines( keys[K_SHIFT].down ? 6 : 2 );
		return;
	}

	if ( key == K_PGUP ) {
		if ( keys[K_SHIFT].down )
			Con_ScrollPages( -1 );
		else
			Con_ScrollLines( -6 );
		return;
	}

	if ( key == K_PGDN ) {
		if ( keys[K_SHIFT].down )
			Con_ScrollPages( 1 );
		else
			Con_ScrollLines( 6 );
		return;
	}

	// ctrl-home = top of console
	if ( key == K_HOME && keys[K_CTRL].down ) {
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if ( key == K_END && keys[K_CTRL].down ) {
		Con_Bottom();
		return;
	}

	// ctrl-i = insert CVar's current value
	if ( keys[K_CTRL].down && tolower(key) == 'i' ) {
		Field_InsertValue( &g_consoleField, qfalse );
		return;
	}

	// ctrl-d = insert CVar's default value
	if ( keys[K_CTRL].down && tolower(key) == 'd' ) {
		Field_InsertValue( &g_consoleField, qtrue );
		return;
	}

	// ctrl-f begins a new search
	if ( keys[K_CTRL].down && tolower(key) == 'f' ) {
		Q_strncpyz( g_consoleField.buffer, "\\searchconsole ", sizeof( g_consoleField.buffer ) );
		g_consoleField.cursor = strlen( g_consoleField.buffer );
		return;
	}

	// (shift-)f3 finds the next (or previous) match
	if ( key == K_F3 ) {
		Con_ContinueSearch( !keys[K_SHIFT].down );
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &g_consoleField, key );
}


///////////////////////////////////////////////////////////////


// ingame talk message - this is fairly crap and belongs in cgame, not the engine

static void Message_Key( int key )
{

	if (key == K_ESCAPE) {
		cls.keyCatchers &= ~KEYCATCH_MESSAGE;
		Field_Clear( &chatField );
		return;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if ( chatField.buffer[0] && cls.state == CA_ACTIVE ) {
			char buffer[MAX_STRING_CHARS];

			if (chat_playerNum != -1 )
				Com_sprintf( buffer, sizeof( buffer ), "tell %i \"%s\"\n", chat_playerNum, chatField.buffer );
			else if (chat_team)
				Com_sprintf( buffer, sizeof( buffer ), "say_team \"%s\"\n", chatField.buffer );
			else
				Com_sprintf( buffer, sizeof( buffer ), "say \"%s\"\n", chatField.buffer );
			CL_AddReliableCommand( buffer );
		}
		cls.keyCatchers &= ~KEYCATCH_MESSAGE;
		Field_Clear( &chatField );
		return;
	}

	Field_KeyDownEvent( &chatField, key );
}


///////////////////////////////////////////////////////////////


qbool Key_GetOverstrikeMode()
{
	return key_overstrikeMode;
}


void Key_SetOverstrikeMode( qbool state )
{
	key_overstrikeMode = state;
}


qbool Key_IsDown( int keynum )
{
	if ( keynum == -1 ) {
		return qfalse;
	}

	return keys[keynum].down;
}


/*
Returns a key number to be used to index keys[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.

0x11 will be interpreted as raw hex, which will allow new controlers
to be configured even if they don't have defined names.
*/
static int Key_StringToKeynum( const char* str )
{
	if ( !str || !str[0] ) {
		return -1;
	}
	if ( !str[1] ) {
		return tolower( str[0] );
	}

	// check for hex code
	if ( str[0] == '0' && str[1] == 'x' && strlen( str ) == 4) {
		int		n1, n2;
		
		n1 = tolower( str[2] );
		if ( n1 >= '0' && n1 <= '9' ) {
			n1 -= '0';
		} else if ( n1 >= 'a' && n1 <= 'f' ) {
			n1 = n1 - 'a' + 10;
		} else {
			n1 = 0;
		}

		n2 = tolower( str[3] );
		if ( n2 >= '0' && n2 <= '9' ) {
			n2 -= '0';
		} else if ( n2 >= 'a' && n2 <= 'f' ) {
			n2 = n2 - 'a' + 10;
		} else {
			n2 = 0;
		}

		return n1 * 16 + n2;
	}

	// scan for a text match
	for (const keyname_t* kn = keynames; kn->name; ++kn) {
		if ( !Q_stricmp( str,kn->name ) )
			return kn->keynum;
	}

	return -1;
}


// returns a single ascii char, a K_* name, or a 0x11 hex string

const char* Key_KeynumToString( int keynum )
{
	static char tinystr[5];
	int i, j;

	if ( keynum == -1 ) {
		return "<KEY NOT FOUND>";
	}

	if ( keynum < 0 || keynum > 255 ) {
		return "<OUT OF RANGE>";
	}

	// check for printable ascii (don't use quote)
	if ( keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';' ) {
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	// check for a key string
	for (const keyname_t* kn = keynames; kn->name; ++kn) {
		if (keynum == kn->keynum) {
			return kn->name;
		}
	}

	// make a hex string
	i = keynum >> 4;
	j = keynum & 15;

	tinystr[0] = '0';
	tinystr[1] = 'x';
	tinystr[2] = i > 9 ? i - 10 + 'A' : i + '0';
	tinystr[3] = j > 9 ? j - 10 + 'A' : j + '0';
	tinystr[4] = 0;

	return tinystr;
}


void Key_SetBinding( int keynum, const char* binding )
{
	if ( keynum == -1 ) {
		return;
	}

	// free old bindings
	if ( keys[ keynum ].binding ) {
		Z_Free( keys[ keynum ].binding );
	}

	// allocate memory for new binding
	keys[keynum].binding = CopyString( binding );

	// consider this like modifying an archived cvar, so the
	// file write will be triggered at the next oportunity
	cvar_modifiedFlags |= CVAR_ARCHIVE;
}


const char* Key_GetBinding( int keynum )
{
	if ( keynum == -1 ) {
		return "";
	}

	return keys[ keynum ].binding;
}


int Key_GetKey( const char* binding )
{
	if (!binding)
		return -1;

	for ( int i = 0; i < MAX_KEYS; ++i ) {
		if (keys[i].binding && Q_stricmp(binding, keys[i].binding) == 0) {
			return i;
		}
	}

	return -1;
}


void Key_KeyNameCompletion( void (*callback)(const char *s) )
{
	for( int i = 0; keynames[i].name != NULL; i++ )
		callback( keynames[i].name );
}


// write bindings to a config file as "bind key value" so they can be exec'ed later

void Key_WriteBindings( fileHandle_t f )
{
	FS_Printf( f, "unbindall\n" );

	for ( int i = 0; i < MAX_KEYS; ++i ) {
		if ( keys[i].binding && keys[i].binding[0] ) {
			FS_Printf( f, "bind %s \"%s\"\n", Key_KeynumToString(i), keys[i].binding );
		}
	}
}


///////////////////////////////////////////////////////////////


static void Key_Unbind_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	int k = Key_StringToKeynum( Cmd_Argv(1) );
	if (k == -1) {
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv(1) );
		return;
	}

	Key_SetBinding( k, "" );
}


static void Key_Unbindall_f()
{
	for ( int i = 0; i < MAX_KEYS; ++i ) {
		if (keys[i].binding) {
			Key_SetBinding( i, "" );
		}
	}
}


static void Key_Bind_f()
{
	int c = Cmd_Argc();

	if (c < 2) {
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	int k = Key_StringToKeynum( Cmd_Argv(1) );
	if (k == -1) {
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv(1) );
		return;
	}

	if (c == 2) {
		if (keys[k].binding)
			Com_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv(1), keys[k].binding );
		else
			Com_Printf( "\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}

	Key_SetBinding( k, Cmd_ArgsFrom(2) );
}


static void Key_CompleteBind_f( int startArg, int compArg )
{
	if ( compArg == startArg + 1 )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteKeyName );
	else if ( compArg >= startArg + 2 )
		Field_AutoCompleteFrom( compArg, compArg, qtrue, qtrue );
}


static void Key_CompleteUnbind_f( int startArg, int compArg )
{
	if ( compArg == startArg + 1 )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteKeyName );
}


static void Key_Bindlist_f()
{
	for ( int i = 0; i < MAX_KEYS; ++i ) {
		if ( keys[i].binding && keys[i].binding[0] ) {
			Com_Printf( "%s \"%s\"\n", Key_KeynumToString(i), keys[i].binding );
		}
	}
}


// returns qtrue when the key should be printed by Key_BindKeyList_f

static qboolean Key_IsPrintedKeyName( const char* n )
{
	return ( strstr( n, "AUX" ) != n ) && ( strstr( n, "WORLD_" ) != n );
}


// prints the list of key names usable with /bind

void Key_BindKeyList_f( void )
{
	// the keynames array ends with a "NULL" entry
	const int count = ( sizeof( keynames ) / sizeof( keynames[0] ) ) - 1;
	
	int maxLength = 0;
	for ( int keyIdx = 0; keyIdx < count; keyIdx++ ) 
	{
		const char* const name = keynames[keyIdx].name;
		if ( !name || !Key_IsPrintedKeyName( name ) )
			continue;

		const int l = strlen( name );
		maxLength = max( maxLength, l );
	}
	const int columnCount = 64 / (maxLength + 1);

	for ( int keyIdx = 0, colIdx = 0; keyIdx < count; keyIdx++ )
	{
		const char* const name = keynames[keyIdx].name;
		if ( !name || !Key_IsPrintedKeyName( name ) )
			continue;

		if ( colIdx == 0 )
		{
			Com_Printf("  ");
		}
		Com_Printf( "%s ", name );

		colIdx++;
		if ( colIdx == columnCount )
		{
			colIdx = 0;
			Com_Printf( "\n" );
		}
		else
		{
			const int l = strlen( name );
			for ( int spaceIdx = 0; spaceIdx < maxLength - l; spaceIdx++ )
			{
				Com_Printf( " " );
			}
		}
	}
	Com_Printf("\n");
}


static const cmdTableItem_t cl_cmds[] =
{
	{ "bind", Key_Bind_f, Key_CompleteBind_f, help_bind },
	{ "unbind", Key_Unbind_f, Key_CompleteUnbind_f, help_unbind },
	{ "unbindall", Key_Unbindall_f, NULL, "unbinds all keys" },
	{ "bindlist", Key_Bindlist_f, NULL, "prints all bound keys" },
	{ "bindkeylist", Key_BindKeyList_f, NULL, "prints all bindable keys" }
};


void CL_InitKeyCommands()
{
	COMPILE_TIME_ASSERT( K_LAST_KEY <= MAX_KEYS );

	Cmd_RegisterArray( cl_cmds, MODULE_CLIENT );
}


///////////////////////////////////////////////////////////////


static void CL_AddKeyUpCommands( int key, const char* kb, unsigned time )
{
	int i;
	char button[1024], *buttonPtr;
	qbool keyevent;

	if ( !kb ) {
		return;
	}
	keyevent = qfalse;
	buttonPtr = button;
	for ( i = 0; ; i++ ) {
		if ( kb[i] == ';' || !kb[i] ) {
			*buttonPtr = '\0';
			if ( button[0] == '+' && keys[key].sendMinusCmd ) {
				// button commands add keynum and time as parms so that multiple
				// sources can be discriminated and subframe corrected
				const char* const cmd = va ("-%s %i %i\n", button + 1, key, (int)time);
				Cbuf_AddText (cmd);
				keyevent = qtrue;
			} else {
				if (keyevent) {
					// down-only command
					Cbuf_AddText (button);
					Cbuf_AddText ("\n");
				}
			}
			buttonPtr = button;
			while ( (kb[i] <= ' ' || kb[i] == ';') && kb[i] != 0 ) {
				i++;
			}
		}
		*buttonPtr++ = kb[i];
		if ( !kb[i] ) {
			break;
		}
	}
}

/*
===================
CL_KeyEvent

Called by the system for both key up and key down events
===================
*/
void CL_KeyEvent( int key, qbool down, unsigned time )
{
	// update auto-repeat status and BUTTON_ANY status
	keys[key].down = down;

	if (down) {
		keys[key].repeats++;
		if ( keys[key].repeats == 1) {
			const qbool cgameForwarding = (cls.cgameForwardInput & 2) && cgvm;
			const qbool disconnected = cls.state == CA_DISCONNECTED;
			keys[key].sendMinusCmd = !cls.keyCatchers && !cgameForwarding && !disconnected;
			anykeydown++;
		}
	} else {
		keys[key].repeats = 0;
		anykeydown--;
		if (anykeydown < 0) {
			anykeydown = 0;
		}
	}

#ifndef _WIN32
	if (key == K_ENTER)
	{
		if (down)
		{
			if (keys[K_ALT].down)
			{
				Key_ClearStates();
				Cvar_SetValue( "r_fullscreen", !Cvar_VariableIntegerValue( "r_fullscreen" ) );
				Cbuf_AddText( "vid_restart\n" );
				return;
			}
		}
	}
#endif

	// console key is hardcoded, so the user can never unbind it
	if (key == '`' || key == '~') {
		if (!down) {
			return;
		}
		Con_ToggleConsole_f();
		return;
	}

	// escape is always handled special
	if ( key == K_ESCAPE && down ) {
		if ( cls.keyCatchers & KEYCATCH_MESSAGE ) {
			// clear message mode
			Message_Key( key );
			return;
		}

		// escape always gets out of CGAME stuff
		if (cls.keyCatchers & KEYCATCH_CGAME) {
			cls.keyCatchers &= ~KEYCATCH_CGAME;
			VM_Call( cgvm, CG_EVENT_HANDLING, CGAME_EVENT_NONE );
			return;
		}

		if (cls.keyCatchers & KEYCATCH_UI) {
			VM_Call( uivm, UI_KEY_EVENT, key, down );
			return;
		}

		if ( cls.state == CA_ACTIVE && !clc.demoplaying ) {
			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_INGAME );
		}
		else if ( !clc.demoplaying || cl_escapeAbortsDemo->integer ) {
			CL_Disconnect_f();
			S_StopAllSounds();
			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
		}
		return;
	}

	//
	// key up events only perform actions if the game key binding is
	// a button command (leading + sign).  These will be processed even in
	// console mode and menu mode, to keep the character from continuing
	// an action started before a mode switch.
	//
    const char* kb;
	if (!down) {
		kb = keys[key].binding;

		CL_AddKeyUpCommands( key, kb, time );

		if ( (cls.keyCatchers & KEYCATCH_UI) && uivm ) {
			VM_Call( uivm, UI_KEY_EVENT, key, down );
		} else if ( (cls.keyCatchers & KEYCATCH_CGAME) && cgvm ) {
			VM_Call( cgvm, CG_KEY_EVENT, key, down );
		} else if ( (cls.cgameForwardInput & 2) && cgvm ) {
			VM_Call( cgvm, CG_KEY_EVENT, key, down );
		}

		return;
	}


	// distribute the key down event to the apropriate handler
	if ( cls.keyCatchers & KEYCATCH_CONSOLE ) {
		Console_Key( key );
	} else if ( cls.keyCatchers & KEYCATCH_UI ) {
		if ( uivm ) {
			VM_Call( uivm, UI_KEY_EVENT, key, down );
		}
	} else if ( cls.keyCatchers & KEYCATCH_CGAME ) {
		if ( cgvm ) {
			VM_Call( cgvm, CG_KEY_EVENT, key, down );
		}
	} else if ( cls.keyCatchers & KEYCATCH_MESSAGE ) {
		Message_Key( key );
	} else if ( (cls.cgameForwardInput & 2) && cgvm ) {
		VM_Call( cgvm, CG_KEY_EVENT, key, down );
	} else if ( cls.state == CA_DISCONNECTED ) {
		Console_Key( key );
	} else {
		// send the bound action
		kb = keys[key].binding;
		if ( !kb ) {
			// unbound
		} else if (kb[0] == '+') {
			int i;
			char button[1024], *buttonPtr;
			buttonPtr = button;
			for ( i = 0; ; i++ ) {
				if ( kb[i] == ';' || !kb[i] ) {
					*buttonPtr = '\0';
					if ( button[0] == '+') {
						// button commands add keynum and time as parms so that multiple
						// sources can be discriminated and subframe corrected
						const char* const cmd = va ("%s %i %i\n", button, key, (int)time);
						Cbuf_AddText (cmd);
					} else {
						// down-only command
						Cbuf_AddText (button);
						Cbuf_AddText ("\n");
					}
					buttonPtr = button;
					while ( (kb[i] <= ' ' || kb[i] == ';') && kb[i] != 0 ) {
						i++;
					}
				}
				*buttonPtr++ = kb[i];
				if ( !kb[i] ) {
					break;
				}
			}
		} else {
			// down-only command
			Cbuf_AddText (kb);
			Cbuf_AddText ("\n");
		}
	}
}


/*
===================
CL_CharEvent

Normal keyboard characters, already shifted / capslocked / etc
===================
*/
void CL_CharEvent( int key ) {
	// the console key should never be used as a char
	if ( key == '`' || key == '~' ) {
		return;
	}

	// delete is not a printable character and is
	// otherwise handled by Field_KeyDownEvent
	if ( key == 127 ) {
		return;
	}

	// distribute the key down event to the appropriate handler
	if ( cls.keyCatchers & KEYCATCH_CONSOLE )
	{
		if ( !Con_IsInMarkMode() )
		{
			Field_CharEvent( &g_consoleField, key );
		}
	}
	else if ( cls.keyCatchers & KEYCATCH_UI )
	{
		VM_Call( uivm, UI_KEY_EVENT, key | K_CHAR_FLAG, qtrue );
	}
	else if ( cls.keyCatchers & KEYCATCH_MESSAGE )
	{
		Field_CharEvent( &chatField, key );
	}
	else if ( cls.state == CA_DISCONNECTED )
	{
		Field_CharEvent( &g_consoleField, key );
	}
}


void Key_ClearStates()
{
	for ( int i = 0; i < MAX_KEYS; ++i ) {
		if ( keys[i].down ) {
			CL_KeyEvent( i, qfalse, 0 );
		}
		keys[i].down = qfalse;
		keys[i].sendMinusCmd = qfalse;
		keys[i].repeats = 0;
	}

	anykeydown = 0;
}
