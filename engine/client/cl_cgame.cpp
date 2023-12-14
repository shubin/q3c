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
// cl_cgame.c  -- client system interaction with client game

#include "client.h"
#include "../qcommon/vm_local.h"
#include "../qcommon/vm_shim.h"


static const byte*	interopBufferIn;
static int			interopBufferInSize;
static byte*		interopBufferOut;
static int			interopBufferOutSize;


static void CL_CallVote_f()
{
	CL_ForwardCommandToServer( Cmd_Cmd() );
}


static void CL_CompleteCallVote_f( int startArg, int compArg )
{
	if ( compArg == startArg + 2 && !Q_stricmp( Cmd_Argv( startArg + 1 ), "map" ) )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteMapName );
}


static const cmdTableItem_t cl_cmds[] =
{
	// we use these until we get proper handling on the mod side
	{ "cv", CL_CallVote_f, CL_CompleteCallVote_f, "calls a vote" },
	{ "callvote", CL_CallVote_f, CL_CompleteCallVote_f, "calls a vote" }
};


static void CL_GetGameState( gameState_t* gs )
{
	*gs = cl.gameState;
}


static void CL_GetGlconfig( glconfig_t* glconfig )
{
	*glconfig = cls.glconfig;
}


static qbool CL_GetUserCmd( int cmdNumber, usercmd_t* ucmd )
{
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cmdNumber > cl.cmdNumber ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cmdNumber <= cl.cmdNumber - CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}


static int CL_GetCurrentCmdNumber()
{
	return cl.cmdNumber;
}


static void CL_GetCurrentSnapshotNumber( int* snapshotNumber, int* serverTime )
{
	if ( clc.newDemoPlayer ) {
		CL_NDP_GetCurrentSnapshotNumber( snapshotNumber, serverTime );
		return;
	}

	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}


static qbool CL_GetSnapshot( int snapshotNumber, snapshot_t* snapshot )
{
	if ( clc.newDemoPlayer ) {
		return CL_NDP_GetSnapshot( snapshotNumber, snapshot );
	}

	int i, count;

	if ( snapshotNumber > cl.snap.messageNum ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum" );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnapshot_t* clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	// write the snapshot
	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	Com_Memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = clSnap->ps;
	count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	for ( i = 0 ; i < count ; i++ ) {
		snapshot->entities[i] = cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ];
	}

	// FIXME: configstring changes and server commands!!!

	return qtrue;
}


static void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale )
{
	cl.cgameUserCmdValue = userCmdValue;
	cl.cgameSensitivity = sensitivityScale;
}


void CL_ConfigstringModified()
{
	int index = atoi( Cmd_Argv(1) );
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	}
	// get everything after "cs <num>"
	const char* s = Cmd_ArgsFrom(2);

	const char* old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	gameState_t oldGs = cl.gameState;

	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;

	const char* dup;
	for ( int i = 0; i < MAX_CONFIGSTRINGS; ++i ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		// leave with the default empty string
		}

		int len = strlen( dup );
		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged();
	}
}


// set up argc/argv for the given command

static qbool CL_GetServerCommand( int serverCommandNumber )
{
	if ( clc.newDemoPlayer ) {
		return CL_NDP_GetServerCommand( serverCommandNumber );
	}

	static char bigConfigString[BIG_INFO_STRING];

	// if we have irretrievably lost a reliable command, drop the connection
	if ( serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( clc.demoplaying )
			return qfalse;
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( serverCommandNumber > clc.serverCommandSequence ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	const int index = serverCommandNumber & (MAX_RELIABLE_COMMANDS - 1);
	if ( clc.serverCommandsBad[ index ] )
		return qfalse;

	const char* s = clc.serverCommands[ index ];
	clc.lastExecutedServerCommand = serverCommandNumber;
	Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );

rescan:
	Cmd_TokenizeString( s );
	const int argc = Cmd_Argc();
	const char* const cmd = Cmd_Argv(0);

	if ( !strcmp( cmd, "disconnect" ) ) {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=552
		// allow server to indicate why they were disconnected
		if ( argc >= 2 )
			Com_Error (ERR_SERVERDISCONNECT, va( "Server Disconnected - %s", Cmd_Argv( 1 ) ) );
		else
			Com_Error (ERR_SERVERDISCONNECT,"Server disconnected\n");
	}

	if ( !strcmp( cmd, "bcs0" ) ) {
		Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs1" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs2" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		strcat( bigConfigString, "\"" );
		s = bigConfigString;
		goto rescan;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	if ( !strcmp( cmd, "map_restart" ) ) {
		// clear notify lines and outgoing commands before passing
		// the restart to the cgame
		Con_ClearNotify();
		Com_Memset( cl.cmds, 0, sizeof( cl.cmds ) );
		return qtrue;
	}

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


// just adds default parameters that cgame doesn't need to know about

static void CL_CM_LoadMap( const char* mapname )
{
	unsigned checksum;
	CM_LoadMap( mapname, qtrue, &checksum );
}


void CL_ShutdownCGame()
{
	Com_Memset( cls.cgvmCalls, 0, sizeof(cls.cgvmCalls) );
	cls.keyCatchers &= ~KEYCATCH_CGAME;
	cls.cgameStarted = qfalse;
	cls.cgameForwardInput = 0;
	cls.cgameCharEvents = 0;
	CL_MapDownload_Cancel();
	Cmd_UnregisterArray( cl_cmds );
	if ( !cgvm ) {
		return;
	}
	VM_Call( cgvm, CG_SHUTDOWN );
	VM_Free( cgvm );
	cgvm = NULL;
	Cmd_UnregisterModule( MODULE_CGAME );
}


static qbool CL_CG_GetValue( char* value, int valueSize, const char* key )
{
	struct syscall_t { const char* name; int number; };
	static const syscall_t syscalls[] = {
		// syscalls
		{ "trap_LocateInteropData", CG_EXT_LOCATEINTEROPDATA },
		{ "trap_R_AddRefEntityToScene2", CG_EXT_R_ADDREFENTITYTOSCENE2 },
		{ "trap_R_ForceFixedDLights", CG_EXT_R_FORCEFIXEDDLIGHTS },
		{ "trap_SetInputForwarding", CG_EXT_SETINPUTFORWARDING },
		{ "trap_Cvar_SetRange", CG_EXT_CVAR_SETRANGE },
		{ "trap_Cvar_SetHelp", CG_EXT_CVAR_SETHELP },
		{ "trap_Cmd_SetHelp", CG_EXT_CMD_SETHELP },
		{ "trap_MatchAlertEvent", CG_EXT_MATCHALERTEVENT },
		{ "trap_Error2", CG_EXT_ERROR2 },
		{ "trap_IsRecordingDemo", CG_EXT_ISRECORDINGDEMO },
		{ "trap_CNQ3_NDP_Enable", CG_EXT_NDP_ENABLE },
		{ "trap_CNQ3_NDP_Seek", CG_EXT_NDP_SEEK },
		{ "trap_CNQ3_NDP_ReadUntil", CG_EXT_NDP_READUNTIL },
		{ "trap_CNQ3_NDP_StartVideo", CG_EXT_NDP_STARTVIDEO },
		{ "trap_CNQ3_NDP_StopVideo", CG_EXT_NDP_STOPVIDEO },
		{ "trap_CNQ3_R_RenderScene", CG_EXT_R_RENDERSCENE },
		{ "trap_CNQ3_NK_CreateFontAtlas", CG_EXT_NK_CREATEFONTATLAS },
		{ "trap_CNQ3_NK_Upload", CG_EXT_NK_UPLOAD },
		{ "trap_CNQ3_NK_Draw", CG_EXT_NK_DRAW },
		{ "trap_CNQ3_SetCharEvents", CG_EXT_SETCHAREVENTS },
		// commands
		{ "screenshotnc", 1 },
		{ "screenshotncJPEG", 1 },
		// capabilities
		{ "cap_ExtraColorCodes", 1 }
	};

	if ( Q_stricmp(key, "cap_DepthClamp") == 0 ) {
		value[0] = re.DepthClamp() ? '1' : '0';
		value[1] = '\0';
		return qtrue;
	}

	for ( int i = 0; i < ARRAY_LEN( syscalls ); ++i ) {
		if ( Q_stricmp(key, syscalls[i].name) == 0 ) {
			Com_sprintf( value, valueSize, "%d", syscalls[i].number );
			return qtrue;
		}
	}

	return qfalse;
}


///////////////////////////////////////////////////////////////


// the cgame module is making a system call

#if defined( QC )
static intptr_t VMCALL CL_CgameSystemCalls( intptr_t *args )
#else
static intptr_t CL_CgameSystemCalls( intptr_t *args )
#endif
{
	switch( args[0] ) {
	case CG_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case CG_ERROR:
		Com_ErrorExt( ERR_DROP, EXT_ERRMOD_CGAME, qtrue, "%s", (const char*)VMA(1) );
		return 0;
	case CG_MILLISECONDS:
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4] );
		Cvar_SetModule( VMA(2), MODULE_CGAME );
		return 0;
	case CG_CVAR_UPDATE:
		Cvar_Update( VMA(1) );
		return 0;
	case CG_CVAR_SET:
		Cvar_Set( VMA(1), VMA(2) );
		return 0;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		Cvar_VariableStringBuffer( VMA(1), VMA(2), args[3] );
		return 0;
	case CG_ARGC:
		return Cmd_Argc();
	case CG_ARGV:
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case CG_ARGS:
		Cmd_ArgsBuffer( VMA(1), args[2] );
		return 0;
	case CG_FS_FOPENFILE:
		return FS_FOpenFileByMode( VMA(1), VMA(2), (fsMode_t)args[3] );
	case CG_FS_READ:
		FS_Read2( VMA(1), args[2], args[3] );
		return 0;
	case CG_FS_WRITE:
		FS_Write( VMA(1), args[2], args[3] );
		return 0;
	case CG_FS_FCLOSEFILE:
		FS_FCloseFile( args[1] );
		return 0;
	case CG_FS_SEEK:
		return FS_Seek( args[1], args[2], args[3] );
	case CG_SENDCONSOLECOMMAND:
		Cbuf_AddText( VMA(1) );
		return 0;
	case CG_ADDCOMMAND:
		Cmd_AddCommand( VMA(1), NULL );
		Cmd_SetModule( VMA(1), MODULE_CGAME );
		return 0;
	case CG_REMOVECOMMAND:
		Cmd_RemoveCommand( VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( VMA(1) );
		return 0;
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
//		Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
// if there is a map change while we are downloading at pk3.
// ZOID
		SCR_UpdateScreen();
		return 0;
	case CG_CM_LOADMAP:
		CL_CM_LoadMap( VMA(1) );
		return 0;
	case CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL:
		return CM_InlineModel( args[1] );
	case CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qfalse );
	case CG_CM_TEMPCAPSULEMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qtrue );
	case CG_CM_POINTCONTENTS:
		return CM_PointContents( VMA(1), args[2] );
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( VMA(1), args[2], VMA(3), VMA(4) );
	case CG_CM_BOXTRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		return 0;
	case CG_CM_CAPSULETRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qfalse );
		return 0;
	case CG_CM_TRANSFORMEDCAPSULETRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qtrue );
		return 0;
	case CG_CM_MARKFRAGMENTS:
		return re.MarkFragments( args[1], VMA(2), VMA(3), args[4], VMA(5), args[6], VMA(7) );

	case CG_S_STARTSOUND:
		S_StartSound( VMA(1), args[2], args[3], args[4] );
		return 0;
	case CG_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds();
		return 0;
	case CG_S_ADDLOOPINGSOUND:
#if defined( QC )
	case DO_NOT_WANT_CG_S_ADDREALLOOPINGSOUND:
		S_AddLoopingSound(args[1], VMA(2), VMA(3), args[4]);
#else  // QC
		S_AddLoopingSound( args[1], VMA(2), /*unused args[3],*/ args[4] );
#endif // QC
		return 0;
	case CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], VMA(2) );
		return 0;
	case CG_S_RESPATIALIZE:
		S_Respatialize( args[1], VMA(2), (const vec3_t*)VMA(3), args[4] );
		return 0;
	case CG_S_REGISTERSOUND:
		return S_RegisterSound( VMA(1) );
	case CG_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2) );
		return 0;

	case CG_R_LOADWORLDMAP:
		re.LoadWorld( VMA(1) );
		return 0;
	case CG_R_REGISTERMODEL:
		return re.RegisterModel( VMA(1) );
	case CG_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );
	case CG_R_REGISTERSHADER:
		return re.RegisterShader( VMA(1) );
	case CG_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( VMA(1) );
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( VMA(1), qfalse );
		return 0;
	case CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), 1 );
		return 0;
#if defined( QC )
	case DO_NOT_WANT_CG_R_ADDDEFECTIVELIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
#endif
	case CG_R_ADDPOLYSTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), args[4] );
		return 0;
	case CG_R_LIGHTFORPOINT:
		return re.LightForPoint( VMA(1), VMA(2), VMA(3), VMA(4) );
	case CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_RENDERSCENE:
		re.RenderScene( VMA(1), 0 );
		return 0;
	case CG_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;
	case CG_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;
	case CG_R_MODELBOUNDS:
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;
	case CG_R_LERPTAG:
		return re.LerpTag( VMA(1), args[2], args[3], args[4], VMF(5), VMA(6) );
	case CG_GETGLCONFIG:
		CL_GetGlconfig( VMA(1) );
		return 0;
	case CG_GETGAMESTATE:
		CL_GetGameState( VMA(1) );
		return 0;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		CL_GetCurrentSnapshotNumber( VMA(1), VMA(2) );
		return 0;
	case CG_GETSNAPSHOT:
		return CL_GetSnapshot( args[1], VMA(2) );
	case CG_GETSERVERCOMMAND:
		return CL_GetServerCommand( args[1] );
	case CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case CG_GETUSERCMD:
		return CL_GetUserCmd( args[1], VMA(2) );
	case CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2) );
		return 0;
	case CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
	case CG_KEY_ISDOWN:
		return Key_IsDown( args[1] );
	case CG_KEY_GETCATCHER:
		return Key_GetCatcher();
	case CG_KEY_SETCATCHER:
		Key_SetCatcher( args[1] );
		return 0;
	case CG_KEY_GETKEY:
		return Key_GetKey( VMA(1) );

	case CG_MEMSET:
		Com_Memset( VMA(1), args[2], args[3] );
		return 0;
	case CG_MEMCPY:
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return 0;
	case CG_STRNCPY:
		strncpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case CG_SIN:
		return PASSFLOAT( sin( VMF(1) ) );
	case CG_COS:
		return PASSFLOAT( cos( VMF(1) ) );
	case CG_ATAN2:
		return PASSFLOAT( atan2( VMF(1), VMF(2) ) );
	case CG_SQRT:
		return PASSFLOAT( sqrt( VMF(1) ) );
	case CG_FLOOR:
		return PASSFLOAT( floor( VMF(1) ) );
	case CG_CEIL:
		return PASSFLOAT( ceil( VMF(1) ) );
	case CG_ACOS:
		return PASSFLOAT( Q_acos( VMF(1) ) );

	case CG_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;

	case CG_REAL_TIME:
		return Com_RealTime( VMA(1) );

	case CG_CIN_PLAYCINEMATIC:
		return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

	case CG_CIN_STOPCINEMATIC:
		return CIN_StopCinematic(args[1]);

	case CG_CIN_RUNCINEMATIC:
		return CIN_RunCinematic(args[1]);

	case CG_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic(args[1]);
		return 0;

	case CG_CIN_SETEXTENTS:
		CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
		return 0;

#if defined( QC )
	case DO_NOT_WANT_CG_S_STOPLOOPINGSOUND:
		return 0;
#endif // QC

	case CG_R_INPVS:
		return re.inPVS( VMA(1), VMA(2) );

#if defined( QC )
	case CG_GET_ADVERTISEMENTS:
		re.GetAdvertisements( VMA(1), VMA(2), VMA(3) );
		return 0;
		
	case CG_R_DRAWTRIANGLE:
		re.DrawTriangle( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), VMF(9), VMF(10), VMF(11), VMF(12), args[13] );
		return 0;

	case CG_CM_PROJECTDECAL:
		return re.ProjectDecal( VMA(1), VMA( 2 ), VMF( 3 ), VMF( 4 ), VMF( 5 ), args[6], VMA( 7 ), VMA( 8 ), args[9], VMA( 10 ) );
#endif

	// engine extensions

	case CG_EXT_GETVALUE:
		return CL_CG_GetValue( VMA(1), args[2], VMA(3) );

	case CG_EXT_LOCATEINTEROPDATA:
		interopBufferIn = VMA(1);
		interopBufferInSize = args[2];
		interopBufferOut = VMA(3);
		interopBufferOutSize = args[4];
		return 0;

	case CG_EXT_R_ADDREFENTITYTOSCENE2:
		re.AddRefEntityToScene( VMA(1), qtrue );
		return 0;

	case CG_EXT_R_FORCEFIXEDDLIGHTS:
		return 0; // already the case for CNQ3

	case CG_EXT_SETINPUTFORWARDING:
		cls.cgameForwardInput = (int)args[1];
		return 0;

	case CG_EXT_CVAR_SETRANGE:
		Cvar_SetRange( VMA(1), (cvarType_t)args[2], VMA(3), VMA(4) );
		return 0;

	case CG_EXT_CVAR_SETHELP:
		Cvar_SetHelp( VMA(1), VMA(2) );
		return 0;

	case CG_EXT_CMD_SETHELP:
		Cmd_SetHelp( VMA(1), VMA(2) );
		return 0;

	case CG_EXT_MATCHALERTEVENT:
		Sys_MatchAlert( (sysMatchAlertEvent_t)args[1] );
		return 0;

	case CG_EXT_ERROR2:
		Com_ErrorExt( ERR_DROP, EXT_ERRMOD_CGAME, (qbool)args[2], "%s", (const char*)VMA(1) );
		return 0;

	case CG_EXT_ISRECORDINGDEMO:
		return clc.demorecording;

	case CG_EXT_NDP_ENABLE:
		if( clc.demoplaying && cl_demoPlayer->integer ) {
			cls.cgameNewDemoPlayer = qtrue;
			cls.cgvmCalls[CGVM_NDP_ANALYZE_COMMAND] = args[1];
			cls.cgvmCalls[CGVM_NDP_GENERATE_COMMANDS] = args[2];
			cls.cgvmCalls[CGVM_NDP_IS_CS_NEEDED] = args[3];
			cls.cgvmCalls[CGVM_NDP_ANALYZE_SNAPSHOT] = args[4];
			cls.cgvmCalls[CGVM_NDP_END_ANALYSIS] = args[5];
			return qtrue;
		} else {
			return qfalse;
		}

	case CG_EXT_NDP_SEEK:
		return CL_NDP_Seek( args[1] );

	case CG_EXT_NDP_READUNTIL:
		CL_NDP_ReadUntil( args[1] );
		return 0;

	case CG_EXT_NDP_STARTVIDEO:
		Cvar_Set( cl_aviFrameRate->name, va( "%d", (int)args[2] ) );
		return CL_OpenAVIForWriting( VMA(1) );

	case CG_EXT_NDP_STOPVIDEO:
		CL_CloseAVI();
		return 0;

	case CG_EXT_R_RENDERSCENE:
		re.RenderScene( VMA(1), args[2] );
		return 0;

	case CG_EXT_NK_CREATEFONTATLAS:
		return RE_NK_CreateFontAtlas( VM_UI, interopBufferOut, interopBufferOutSize, interopBufferIn, interopBufferInSize );

	case CG_EXT_NK_UPLOAD:
		RE_UploadNuklear( VMA(1), args[2], VMA(3), args[4] );
		return 0;

	case CG_EXT_NK_DRAW:
		RE_DrawNuklear( args[1], args[2], args[3], VMA(4) );
		return 0;

	case CG_EXT_SETCHAREVENTS:
		cls.cgameCharEvents = (qbool)args[1];
		return 0;

	default:
		Com_Error( ERR_DROP, "Bad cgame system trap: %i", args[0] );
	}
	return 0;
}


static void CL_SetMaxFPS( int maxFPS )
{
	if ( maxFPS > 0 && cls.maxFPS == 0 ) {
		cls.maxFPS = maxFPS;
		cls.nextFrameTimeMS = Sys_Milliseconds() + 1000 / maxFPS;
	} else {
		cls.maxFPS = 0;
	}
}


void CL_DisableFramerateLimiter()
{
	CL_SetMaxFPS( 0 );
}


void CL_InitCGame()
{
	int t = Sys_Milliseconds();

	cls.cgameForwardInput = 0;
	cls.cgameCharEvents = 0;
	cls.cgameNewDemoPlayer = qfalse;

	// put away the console
	Con_Close();

	// find the current mapname
	const char* info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	const char* mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );

	// if sv_pure is set we only allow qvms to be loaded
	const vmInterpret_t interpret = cl_connectedToPureServer ? VMI_COMPILED : (vmInterpret_t)Cvar_VariableIntegerValue( "vm_cgame" );

	cgvm = VM_Create( VM_CGAME, CL_CgameSystemCalls, interpret );
	if ( !cgvm ) {
		Com_Error( ERR_DROP, "VM_Create on cgame failed" );
	}
	cls.state = CA_LOADING;

	Cmd_RegisterArray( cl_cmds, MODULE_CLIENT );

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	CL_SetMaxFPS( 20 );
	VM_Call( cgvm, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum );
	CL_SetMaxFPS( 0 );
	CL_SetMenuData( qtrue );

	// send a usercmd this frame, which will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;

	Com_Printf( "CL_InitCGame: %5.2f seconds\n", (Sys_Milliseconds() - t) / 1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// make sure everything is paged in
	if (!Sys_LowPhysicalMemory()) {
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify();
}


// see if the current console command is claimed by the cgame

qbool CL_GameCommand()
{
	return (cgvm && VM_Call( cgvm, CG_CONSOLE_COMMAND ));
}


void CL_CGameRendering( stereoFrame_t stereo )
{
	VM_Call( cgvm, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying );
}


///////////////////////////////////////////////////////////////


/*
Adjust the client's view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
*/

static void CL_AdjustTimeDelta()
{
	cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( clc.demoplaying ) {
		return;
	}

	int newDelta = cl.snap.serverTime - cls.realtime;
	int deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > 500 ) {
		// current time is WAY off, just correct to the current value
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;	// FIXME: is this a problem for cgame?
		cl.serverTime = cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}


static void CL_FirstSnapshot()
{
	// ignore snapshots that don't have entities
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	cls.state = CA_ACTIVE;

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.oldServerTime = cl.snap.serverTime;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// traditionally used for thinshaft scripts
	const cvar_t* cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP );
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cvar_Set( "activeAction", "" );
	}
}


void CL_SetCGameTime()
{
	if ( clc.newDemoPlayer ) {
		CL_NDP_SetCGameTime();
		return;
	}

	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		if ( clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !cl.snap.valid ) {
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && CL_Paused() && com_sv_running->integer ) {
		return;
	}

	if ( cl.snap.serverTime < cl.oldFrameServerTime ) {
		Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
	}
	cl.oldFrameServerTime = cl.snap.serverTime;


	// get our current view of time

	if (clc.demoplaying && (com_timescale->value == 0)) {
		cl.serverTimeDelta -= cls.frametime;
	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness (except it's crap)
		// int tn = Com_Clamp( -30, 30, cl_timeNudge->value );
		cl.serverTime = cls.realtime + cl.serverTimeDelta;// - tn;

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( cl.serverTime < cl.oldServerTime ) {
			cl.serverTime = cl.oldServerTime;
		}
		cl.oldServerTime = cl.serverTime;

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try and adjust back a bit when the next snapshot arrives
		if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
			cl.extrapolatedSnapshot = qtrue;
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would cause a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}

	// we are playing back a demo, so we can just keep reading
	// messages from the demo file until the cgame definitely
	// has valid snapshots to interpolate between

	// a timedemo will always use a deterministic set of time samples
	// no matter what speed machine it is run on,
	// while a normal demo may have different time samples
	// each time it is played back
	if ( cl_timedemo->integer ) {
		if (!clc.timeDemoStart) {
			clc.timeDemoStart = Sys_Milliseconds();
		}
		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
	}

	while ( cl.serverTime >= cl.snap.serverTime ) {
		// feed another message, which should change the contents of cl.snap
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return;		// end of demo
		}
	}

}


void CL_CGNDP_AnalyzeCommand( int serverTime )
{
	Q_assert(cls.cgameNewDemoPlayer);
	VM_Call(cgvm, cls.cgvmCalls[CGVM_NDP_ANALYZE_COMMAND], serverTime);
}


void CL_CGNDP_GenerateCommands( const char** commands, int* numCommandBytes )
{
	Q_assert(cls.cgameNewDemoPlayer);
	Q_assert(commands);
	Q_assert(numCommandBytes);
	VM_Call(cgvm, cls.cgvmCalls[CGVM_NDP_GENERATE_COMMANDS]);
	*numCommandBytes = *(int*)interopBufferIn;
	*commands = (const char*)interopBufferIn + 4;
}


qbool CL_CGNDP_IsConfigStringNeeded( int csIndex )
{
	Q_assert(cls.cgameNewDemoPlayer);
	Q_assert(csIndex >= 0 && csIndex < MAX_CONFIGSTRINGS);
	return (qbool)VM_Call(cgvm, cls.cgvmCalls[CGVM_NDP_IS_CS_NEEDED], csIndex);
}


qbool CL_CGNDP_AnalyzeSnapshot( int progress )
{
	Q_assert(cls.cgameNewDemoPlayer);
	Q_assert(progress >= 0 && progress < 100);
	return (qbool)VM_Call(cgvm, cls.cgvmCalls[CGVM_NDP_ANALYZE_SNAPSHOT], progress);
}


void CL_CGNDP_EndAnalysis( const char* filePath, int firstServerTime, int lastServerTime, qbool videoRestart )
{
	Q_assert(cls.cgameNewDemoPlayer);
	Q_assert(lastServerTime > firstServerTime);
	Q_strncpyz((char*)interopBufferOut, filePath, interopBufferOutSize);
	VM_Call(cgvm, cls.cgvmCalls[CGVM_NDP_END_ANALYSIS], firstServerTime, lastServerTime, videoRestart);
}
