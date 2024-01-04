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
// cmd.c -- Quake script command processing module

#include "q_shared.h"
#include "qcommon.h"
#include "common_help.h"

#define MAX_CMD_BUFFER	32768
#define MAX_CMD_LINE	1024

typedef struct {
	byte	*data;
	int		maxsize;
	int		cursize;
} cmd_t;

static int		cmd_wait;		// how many more frames to wait for
static int		cmd_wait_stop;	// the time when the wait ends
static cmd_t	cmd_text;
static byte		cmd_text_buf[MAX_CMD_BUFFER];


// delay execution of the remainder of the command buffer until next frame
// so that scripts etc can work around race conditions

static void Cmd_Wait_f( void )
{
	if ( Cmd_Argc() == 2 ) {
		cmd_wait = atoi( Cmd_Argv( 1 ) );
	} else {
		cmd_wait = 1;
	}
}


static void Cmd_WaitMS_f( void )
{
	const char* const arg0 = Cmd_Argv( 0 );
	const int duration = atoi( Cmd_Argv( 1 ) );
	if ( Cmd_Argc() != 2 || duration <= 0 ) {
		Com_Printf( "usage: %s milliseconds\n", arg0 );
		return;
	}

	cmd_wait_stop = Sys_Milliseconds() + duration;
}


static void Cmd_Help_f()
{
	const char* const arg0 = Cmd_Argv( 0 );
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: %s cvarname|cmdname\n", arg0 );
		return;
	}
	
	const char* const arg1 = Cmd_Argv(1);
	if ( !Q_stricmp( arg0, "man" ) && !Q_stricmp( arg1, "woman" ) ) {
		Com_Printf( "yeah... no\n" );
		return;
	}

	Com_PrintHelp( arg1, &Com_Printf, qtrue, qtrue, qtrue );
}


static void Cmd_PrintSearchResult( const char *name, const char *desc, const char *help, const char *pattern, qbool cvar )
{
	const char* const color = cvar ? S_COLOR_CVAR : S_COLOR_CMD;
	if ( Q_stristr(name, pattern) || (desc && Q_stristr(desc, pattern)) || (help && Q_stristr(help, pattern)) ) {
		if ( desc )
			Com_Printf( "    %s%s ^7- " S_COLOR_HELP "%s\n", color, name, desc );
		else
			Com_Printf( "    %s%s\n", color, name );
	}
}


static void Cmd_SearchHelp_f()
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: %s string_pattern\n", Cmd_Argv( 0 ) );
		return;
	}

	Cmd_EnumHelp( Cmd_PrintSearchResult, Cmd_Argv( 1 ) );
	Cvar_EnumHelp( Cmd_PrintSearchResult, Cmd_Argv( 1 ) );
}


static void Cmd_CompleteHelp_f( int startArg, int compArg )
{
	if ( compArg == startArg + 1 )
		Field_AutoCompleteFrom( compArg, compArg, qtrue, qtrue );
}


void Cbuf_Init()
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
}


// adds command text at the end of the buffer, does NOT add a final \n

void Cbuf_AddText( const char* text )
{
	int l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Com_Printf( "Cbuf_AddText: data was ignored to avoid a buffer overflow\n" );
		return;
	}

	Com_Memcpy( &cmd_text.data[cmd_text.cursize], text, l );
	cmd_text.cursize += l;
}


// adds command text (and \n) immediately after the current command
// should be (and now is) only EVER used by config file chaining

static void Cbuf_InsertText( const char* text )
{
	int len = strlen( text ) + 1;
	if ( len + cmd_text.cursize > cmd_text.maxsize ) {
		Com_Printf( "Cbuf_InsertText: data was ignored to avoid a buffer overflow\n" );
		return;
	}

	// move the existing command text
	for ( int i = cmd_text.cursize - 1; i >= 0; --i ) {
		cmd_text.data[ i + len ] = cmd_text.data[ i ];
	}

	Com_Memcpy( cmd_text.data, text, len - 1 );
	cmd_text.data[ len - 1 ] = '\n';

	cmd_text.cursize += len;
}


void Cbuf_Execute()
{
	int		i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int		quotes;

	while (cmd_text.cursize)
	{
		if ( cmd_wait ) {
			// skip out and leave the buffer alone until next frame
			cmd_wait--;
			break;
		}

		if ( cmd_wait_stop > Sys_Milliseconds() )
			break;
		cmd_wait_stop = 0;

		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes&1) && text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n' || text[i] == '\r')
				break;
		}

		if( i >= (MAX_CMD_LINE - 1)) {
			i = MAX_CMD_LINE - 1;
		}

		Com_Memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

// execute the command line

		Cmd_ExecuteString (line);
	}
}


///////////////////////////////////////////////////////////////
// SCRIPT COMMANDS


static void Cmd_Exec_f()
{
	char	*f;
	int		len;
	char	filename[MAX_QPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf( "exec <filename> : execute a script file\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	len = FS_ReadFile( filename, (void **)&f );
	if (!f) {
		Com_Printf( "couldn't exec %s\n", Cmd_Argv(1) );
		return;
	}
	Com_Printf( "execing %s\n", Cmd_Argv(1) );

	Cbuf_InsertText(f);

	FS_FreeFile(f);
}


static void Cmd_CompleteExec_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteConfigName );
}


// inserts the current value of a variable as command text

static void Cmd_Vstr_f()
{
	if (Cmd_Argc() != 2) {
		Com_Printf( "vstr <variablename> : execute a variable command\n" );
		return;
	}
	Cbuf_InsertText( Cvar_VariableString( Cmd_Argv( 1 ) ) );
}


static void Cmd_CompleteVstr_f( int startArg, int compArg )
{
	if ( compArg == startArg + 1 )
		Field_AutoCompleteFrom( compArg, compArg, qfalse, qtrue );
}


// just prints the rest of the line to the console

void Cmd_Echo_f()
{
	for (int i = 1; i < Cmd_Argc(); ++i)
		Com_Printf( "%s ",Cmd_Argv(i) );
	Com_Printf("\n");
}


///////////////////////////////////////////////////////////////
// COMMAND EXECUTION


typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	char					*desc;
	char					*help;
	xcommand_t				function;
	xcommandCompletion_t	completion;
	module_t				firstModule;
	int						moduleMask;
} cmd_function_t;


static	int		cmd_argc;
static	char*	cmd_argv[MAX_STRING_TOKENS];		// points into cmd_tokenized
static	int		cmd_argoffsets[MAX_STRING_TOKENS];	// offsets into cmd_cmd
static	qbool	cmd_quoted[MAX_STRING_TOKENS];		// set to 1 if the input was quoted
static	char	cmd_tokenized[BIG_INFO_STRING+MAX_STRING_TOKENS];	// will have 0 bytes inserted
static	char	cmd_cmd[BIG_INFO_STRING]; // the original command we received (no token processing)

static	cmd_function_t	*cmd_functions;		// possible commands to execute


int Cmd_Argc()
{
	return cmd_argc;
}


const char* Cmd_Argv( int arg )
{
	if ((unsigned)arg >= cmd_argc)
		return "";
	return cmd_argv[arg];
}


// returns a single string containing argv(arg) to argv(argc()-1)

const char* Cmd_ArgsFrom( int arg )
{
	static char cmd_args[BIG_INFO_STRING];
	cmd_args[0] = 0;

	if (arg < 0)
		Com_Error( ERR_FATAL, "Cmd_ArgsFrom: arg < 0" );

	for (int i = arg; i < cmd_argc; ++i) {
		strcat( cmd_args, cmd_argv[i] );
		if (i != cmd_argc-1) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}


qbool Cmd_ArgQuoted( int arg )
{
	if ((unsigned)arg >= cmd_argc)
		return qfalse;

	return cmd_quoted[arg];
}


int Cmd_ArgOffset( int arg )
{
	if ((unsigned)arg >= cmd_argc)
		return 0;

	return cmd_argoffsets[arg];
}


int Cmd_ArgIndexFromOffset( int offset )
{
	for (int i = 0; i < cmd_argc; ++i) {
		const int start = cmd_argoffsets[i];
		const int end = start + strlen( cmd_argv[i] );
		if (offset >= start && offset <= end)
			return i;
	}

	return -1;
}


const char* Cmd_Args()
{
	return Cmd_ArgsFrom(1);
}


// the interpreted versions use these because they can't have pointers returned to them

void Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) {
#if defined( QC )
	if ( arg == -1 ) {
		Q_strncpyz( buffer, cmd_cmd, bufferLength );
	} else {
		Q_strncpyz( buffer, Cmd_Argv( arg ), bufferLength );
	}
#else
	Q_strncpyz( buffer, Cmd_Argv( arg ), bufferLength );
#endif
}

void Cmd_ArgsBuffer( char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, Cmd_Args(), bufferLength );
}


/*
Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
*/
const char* Cmd_Cmd()
{
	return cmd_cmd;
}


/*
parses the given string into command line tokens
the text is copied to a separate buffer with NULs inserted after each token
the argv array will point into this new buffer
*/
static void Cmd_TokenizeString2( const char* text, qbool ignoreQuotes )
{
	// clear previous args
	cmd_argc = 0;

	if ( !text )
		return;

	Q_strncpyz( cmd_cmd, text, sizeof(cmd_cmd) );

	char* out = cmd_tokenized;
	const char* const textStart = text;

	while ( 1 ) {
		if ( cmd_argc == MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// skip whitespace
			while ( *text && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// all tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// all tokens parsed
				}
				text += 2;
			} else {
				break;			// we are ready to parse a token
			}
		}

		// handle quoted strings - NOTE: this doesn't handle \" escaping
		if ( !ignoreQuotes && *text == '"' ) {
			cmd_argv[cmd_argc] = out;
			cmd_argoffsets[cmd_argc] = text + 1 - textStart; // jump past the opening quote
			cmd_quoted[cmd_argc] = qtrue;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) {
				*out++ = *text++;
			}
			*out++ = 0;
			if ( !*text ) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = out;
		cmd_argoffsets[cmd_argc] = text - textStart;
		cmd_quoted[cmd_argc] = qfalse;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' ) {
			if ( !ignoreQuotes && text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*out++ = *text++;
		}

		*out++ = 0;

		if ( !*text ) {
			return;		// all tokens parsed
		}
	}
}


void Cmd_TokenizeString( const char* text )
{
	Cmd_TokenizeString2( text, qfalse );
}


void Cmd_TokenizeStringIgnoreQuotes( const char* text )
{
	Cmd_TokenizeString2( text, qtrue );
}


void Cmd_AddCommand( const char* cmd_name, xcommand_t function )
{
	cmd_function_t* cmd;

	// fail if the command already exists
	for ( cmd = cmd_functions ; cmd ; cmd = cmd->next ) {
		if ( !Q_stricmp( cmd_name, cmd->name ) ) {
			// allow completion-only commands to be silently doubled
			if ( function ) {
				Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
			}
			return;
		}
	}

	// use a small malloc to avoid zone fragmentation
	cmd = (cmd_function_t*)S_Malloc(sizeof(cmd_function_t));
	cmd->name = CopyString( cmd_name );
	cmd->function = function;
	cmd->completion = NULL;
	cmd->firstModule = MODULE_NONE;
	cmd->moduleMask = 0;
	cmd->desc = NULL;
	cmd->help = NULL;

	// add the command
	if ( cmd_functions == NULL || Q_stricmp(cmd_functions->name, cmd_name) > 0 ) {
		// insert as the first command
		cmd_function_t* const next = cmd_functions;
		cmd_functions = cmd;
		cmd->next = next;
	} else {
		// insert after some other command
		cmd_function_t* curr = cmd_functions;
		cmd_function_t* prev = cmd_functions;
		for (;;) {
			if ( Q_stricmp(curr->name, cmd_name) > 0 )
				break;

			prev = curr;
			if ( curr->next == NULL )
				break;

			curr = curr->next;
		}
		cmd_function_t* const next = prev->next;
		prev->next = cmd;
		cmd->next = next;
	}
}


static void Cmd_Free( cmd_function_t* cmd )
{
	if ( cmd->name )
		Z_Free( cmd->name );
	if ( cmd->desc )
		Z_Free( cmd->desc );
	if ( cmd->help )
		Z_Free( cmd->help );
	Z_Free( cmd );
}


void Cmd_RemoveCommand( const char* cmd_name )
{
	cmd_function_t** back = &cmd_functions;

	for(;;) {
		cmd_function_t* cmd = *back;
		if ( !cmd ) {
			// command wasn't active
			return;
		}
		if ( !Q_stricmp( cmd_name, cmd->name ) ) {
			*back = cmd->next;
			Cmd_Free( cmd );
			return;
		}
		back = &cmd->next;
	}
}


static cmd_function_t* Cmd_FindCommand( const char* cmd_name )
{
	cmd_function_t* cmd;
	for ( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		if ( !Q_stricmp( cmd_name, cmd->name ) ) {
			return cmd;
		}
	}

	return NULL;
}


void Cmd_SetHelp( const char* cmd_name, const char* cmd_help )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( cmd )
		Help_AllocSplitText( &cmd->desc, &cmd->help, cmd_help );
}


qbool Cmd_GetHelp( const char** desc, const char** help, const char* cmd_name )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( !cmd ) {
		*desc = NULL;
		*help = NULL;
		return qfalse;
	}

	*desc = cmd->desc;
	*help = cmd->help;
	return qtrue;
}


void Cmd_SetAutoCompletion( const char* cmd_name, xcommandCompletion_t completion )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( cmd )
		cmd->completion = completion;
}


void Cmd_AutoCompleteArgument( const char* cmd_name, int startArg, int compArg )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( cmd && cmd->completion )
		cmd->completion( startArg, compArg );
}


void Cmd_CommandCompletion( void(*callback)(const char* s) )
{
	const cmd_function_t* cmd;
	for ( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		callback( cmd->name );
	}
}


void Cmd_EnumHelp( search_callback_t callback, const char* pattern )
{
	if ( pattern == NULL || *pattern == '\0' )
		return;

	cmd_function_t* cmd;
	for ( cmd = cmd_functions; cmd; cmd = cmd->next) {
		callback( cmd->name, cmd->desc, cmd->help, pattern, qfalse );
	}
}


// a complete command line has been parsed, so try to execute it

void Cmd_ExecuteString( const char* text )
{
	Cmd_TokenizeString( text );
	if ( !Cmd_Argc() ) {
		return;		// no tokens
	}

	// check registered command functions
	cmd_function_t *cmd, **prev;
	for ( prev = &cmd_functions ; *prev ; prev = &cmd->next ) {
		cmd = *prev;
		if ( !Q_stricmp( cmd_argv[0],cmd->name ) ) {
			// perform the action
			if ( !cmd->function ) {
				// let the cgame or game handle it
				break;
			} else {
				cmd->function();
			}
			return;
		}
	}

	// check cvars
	if ( Cvar_Command() ) {
		return;
	}

#ifndef DEDICATED
	// check client game commands
	if ( com_cl_running && com_cl_running->integer && CL_GameCommand() ) {
		return;
	}
#endif

	// check server game commands
	if ( com_sv_running && com_sv_running->integer && SV_GameCommand() ) {
		return;
	}

#ifndef DEDICATED
	// check ui commands
	if ( com_cl_running && com_cl_running->integer && UI_GameCommand() ) {
		return;
	}

	// send it as a server command if we are connected
	// this will usually result in a chat message
	CL_ForwardCommandToServer( text );
#endif
}


static void Cmd_List_f()
{
	const char* match = (Cmd_Argc() > 1) ? Cmd_Argv(1) : NULL;

	int i = 0;
	for (const cmd_function_t* cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (match && !Com_Filter(match, cmd->name))
			continue;
		const char h = cmd->help != NULL ? 'h' : ' ';
		Com_Printf( " %c  %s\n", h, cmd->name );
		++i;
	}

	Com_Printf( "%i commands\n", i );
}


static const cmdTableItem_t cl_cmds[] =
{
	{ "cmdlist", Cmd_List_f, NULL, help_cmdlist },
	{ "exec", Cmd_Exec_f, Cmd_CompleteExec_f, "executes all commands in a text file" },
	{ "vstr", Cmd_Vstr_f, Cmd_CompleteVstr_f, "executes the string value of a cvar" },
	{ "echo", Cmd_Echo_f, NULL, "prints the arguments to the console" },
	{ "wait", Cmd_Wait_f, NULL, "delays command execution by N frames" },
	{ "waitms", Cmd_WaitMS_f, NULL, "delays command execution by N milliseconds" },
	{ "help", Cmd_Help_f, Cmd_CompleteHelp_f, "displays the help of a cvar or command" },
	{ "man", Cmd_Help_f, Cmd_CompleteHelp_f, "displays the help of a cvar or command" },
	{ "searchhelp", Cmd_SearchHelp_f, NULL, "lists all cvars+cmds whose help matches" }
};


void Cmd_Init()
{
	Cmd_RegisterArray( cl_cmds, MODULE_COMMON );
}


void Cmd_RegisterTable( const cmdTableItem_t* cmds, int count, module_t module )
{
	for ( int i = 0; i < count; ++i ) {
		const cmdTableItem_t* item = &cmds[i];

		Cmd_AddCommand( item->name, item->function );

		if ( item->completion )
			Cmd_SetAutoCompletion( item->name, item->completion );

		if ( item->help )
			Cmd_SetHelp( item->name, item->help );

		Cmd_SetModule( item->name, module );
	}
}


void Cmd_UnregisterTable( const cmdTableItem_t* cmds, int count )
{
	for ( int i = 0; i < count; ++i ) {
		Cmd_RemoveCommand( cmds[i].name );
	}
}


void Cmd_SetModule( const char* cmd_name, module_t module )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( !cmd )
		return;

	cmd->moduleMask |= 1 << (int)module;
	if ( cmd->firstModule == MODULE_NONE )
		cmd->firstModule = module;
}


void Cmd_UnregisterModule( module_t module )
{
	if ( module <= MODULE_NONE || module >= MODULE_COUNT )
		return;

	cmd_function_t* cmd = cmd_functions;
	while( cmd ) {
		if ( cmd->firstModule == module && cmd->moduleMask == 1 << (int)module ) {
			cmd_function_t* next;
			next = cmd->next;
			Cmd_RemoveCommand( cmd->name );
			cmd = next;
		} else {
			cmd = cmd->next;
		}
	}
}


void Cmd_GetModuleInfo( module_t* firstModule, int* moduleMask, const char* cmd_name )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( !cmd )
		return;

	*firstModule = cmd->firstModule;
	*moduleMask = cmd->moduleMask;
}


const char* Cmd_GetRegisteredName( const char* cmd_name )
{
	cmd_function_t* cmd = Cmd_FindCommand( cmd_name );
	if ( !cmd )
		return NULL;

	return cmd->name;
}
