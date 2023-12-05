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
// cl_main.c  -- client main loop

#include "client.h"
#include "client_help.h"


cvar_t	*cl_debugMove;

cvar_t	*rconPassword;
cvar_t	*rconAddress;

cvar_t* cl_timeout;
cvar_t* cl_maxpackets;
cvar_t* cl_packetdup;
cvar_t* cl_showTimeDelta;
cvar_t* cl_serverStatusResendTime;
cvar_t* cl_shownet;
cvar_t* cl_showSend;

cvar_t	*cl_timedemo;
cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;
cvar_t	*cl_ffmpeg;
cvar_t	*cl_ffmpegCommand;
cvar_t	*cl_ffmpegExePath;
cvar_t	*cl_ffmpegOutPath;
cvar_t	*cl_ffmpegOutExt;
cvar_t	*cl_ffmpegLog;

cvar_t	*cl_allowDownload;
cvar_t	*cl_inGameVideo;

cvar_t	*cl_matchAlerts;
cvar_t	*cl_demoPlayer;
cvar_t	*cl_escapeAbortsDemo;

cvar_t	*net_proxy;

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
vm_t				*cgvm;

refexport_t re; // functions exported from refresh DLL

#define RETRANSMIT_TIMEOUT	3000	// time between connection packet retransmits


/*
===============
CL_CDDialog

Called by Com_Error when a cd is needed
===============
*/
void CL_CDDialog( void ) {
	cls.cddialog = qtrue;	// start it next frame
}


/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand( const char *cmd ) {
	if ( clc.demoplaying ) {
		return;
	}

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if ( clc.reliableSequence - clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS ) {
		Com_Error( ERR_DROP, "Client command overflow" );
	}
	clc.reliableSequence++;
	const int index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}


/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/


// saves the current net message to file, prefixed by the length

static void CL_WriteDemoMessage ( msg_t *msg, int headerBytes )
{
	int len, swlen;

	// write the packet sequence
	len = clc.serverMessageSequence;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.demofile );

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write( &swlen, 4, clc.demofile );
	FS_Write( msg->data + headerBytes, len, clc.demofile );
}


static void CL_StopRecord_f( void )
{
	if ( !clc.demorecording ) {
		Com_Printf ("Not recording a demo.\n");
		return;
	}

	// finish up
	int len = -1;
	FS_Write (&len, 4, clc.demofile);
	FS_Write (&len, 4, clc.demofile);
	FS_FCloseFile (clc.demofile);
	clc.demofile = 0;
	clc.demorecording = qfalse;
	clc.showAnnoyingDemoRecordMessage = qfalse;
	Com_Printf ("Stopped demo.\n");
}


static const char* CL_DemoFilename()
{
	static char s[MAX_OSPATH];

	qtime_t t;
	Com_RealTime( &t );
	Com_sprintf( s, sizeof(s), "demos/%d_%02d_%02d-%02d_%02d_%02d.dm_%d",
			1900+t.tm_year, 1+t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, PROTOCOL_VERSION );

	return s;
}


// record [demoname]
// starts recording a demo from the current position

static void CL_Record_f()
{
	if ( Cmd_Argc() > 2 ) {
		Com_Printf ("record [demoname]\n");
		return;
	}

	if ( clc.demorecording ) {
		if (!clc.showAnnoyingDemoRecordMessage) {
			Com_Printf ("Already recording.\n");
		}
		return;
	}

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

    char name[MAX_OSPATH];
    int i;
	const char* s;
	if ( Cmd_Argc() == 2 ) {
		Com_sprintf( name, sizeof(name), "demos/%s.dm_%d", Cmd_Argv(1), PROTOCOL_VERSION );
		s = name;
	} else {
		s = CL_DemoFilename();
	}

	clc.demofile = FS_FOpenFileWrite( s );
	if ( !clc.demofile ) {
		Com_Printf( "ERROR: couldn't open %s\n", s );
		return;
	}

	Com_Printf( "recording to %s\n", s );
	clc.demorecording = qtrue;
	clc.showAnnoyingDemoRecordMessage = !Cvar_VariableValue("ui_recordSPDemo");

	Q_strncpyz( clc.demoName, s, sizeof( clc.demoName ) );

	// don't start saving messages until a non-delta compressed message is received
	clc.demowaiting = qtrue;

	// write out the gamestate message
	msg_t msg;
	byte buf[MAX_MSGLEN];
	MSG_Init( &msg, buf, sizeof(buf) );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clc.reliableSequence );

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, clc.serverCommandSequence );

	// configstrings
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !cl.gameState.stringOffsets[i] )
			continue;
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, i );
		MSG_WriteBigString( &msg, cl.gameState.stringData + cl.gameState.stringOffsets[i] );
	}

	// baselines
	entityState_t nullstate;
	Com_Memset( &nullstate, 0, sizeof(nullstate) );
	for ( i = 0; i < MAX_GENTITIES ; i++ ) {
		entityState_t* ent = &cl.entityBaselines[i];
		if (!ent->number)
			continue;
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, ent, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF ); // finished writing the gamestate stuff

	MSG_WriteLong( &msg, clc.clientNum );
	MSG_WriteLong( &msg, clc.checksumFeed );

	MSG_WriteByte( &msg, svc_EOF ); // finished writing the client packet

	int len;
	// write it to the demo file
	len = LittleLong( clc.serverMessageSequence - 1 );
	FS_Write( &len, 4, clc.demofile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.demofile );
	FS_Write( msg.data, msg.cursize, clc.demofile );

	// the rest of the demo file will be copied from net messages
}


static void CL_CompleteDemoRecord_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteDemoNameWrite );
}


/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/


void CL_DemoCompleted()
{
	if (cl_timedemo && cl_timedemo->integer) {
		int time = Sys_Milliseconds() - clc.timeDemoStart;
		if ( time > 0 ) {
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", clc.timeDemoFrames,
			time/1000.0, clc.timeDemoFrames*1000.0 / time);
		}
	}

	CL_Disconnect( qtrue );
	CL_NextDemo();
}

/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage( void ) {

	if ( !clc.demofile ) {
		CL_DemoCompleted();
		return;
	}

	// get the sequence number
    int	s;
	int r = FS_Read( &s, 4, clc.demofile);
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	clc.serverMessageSequence = LittleLong( s );

    msg_t		buf;
    byte		bufData[ MAX_MSGLEN ];
	// init the message
	MSG_Init( &buf, bufData, sizeof( bufData ) );

	// get the length
	r = FS_Read (&buf.cursize, 4, clc.demofile);
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize == -1 ) {
		CL_DemoCompleted();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Error (ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read( buf.data, buf.cursize, clc.demofile );
	if ( r != buf.cursize ) {
		Com_Printf( "Demo file was truncated.\n");
		CL_DemoCompleted();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;
	CL_ParseServerMessage( &buf );
}


static void CL_WalkDemoExt( const char* path, fileHandle_t* fh )
{
	const int protocols[] = { 68, 67, 66 };
	char fullPath[MAX_OSPATH];

	*fh = 0;

	if (Sys_IsAbsolutePath( path )) {
		FS_FOpenAbsoluteRead( path, fh );
		return;
	}

	for (int i = 0; i < ARRAY_LEN(protocols); ++i) {
		Com_sprintf( fullPath, sizeof( fullPath ), "demos/%s.dm_%d", path, protocols[i] );
		FS_FOpenFileRead( fullPath, fh, qtrue );
		if (*fh) {
			Com_Printf( "Demo file: %s\n", fullPath );
			return;
		}
	}

	Com_Printf( "No match: demos/%s.dm_*\n", path );
}


static void CL_PlayDemo( qbool videoRestart )
{
	if (Cmd_Argc() != 2) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	
	const char* const demoPath = Cmd_Argv(1);
	fileHandle_t fh;
	CL_WalkDemoExt( demoPath, &fh );
	if ( fh == 0 ) {
		Com_Printf( "Couldn't open demo %s\n", demoPath );
		CL_NextDemo();
		return;
	}

	// CL_Disconnect uses the tokenizer, so we save the demo path now
	char shortPath[MAX_OSPATH];
	Q_strncpyz( shortPath, demoPath, sizeof( shortPath ) );

	// CL_Disconnect closes clc.demofile, so we set it after the call
	SV_Shutdown( "closing for demo playback" );
	CL_Disconnect( qfalse );
	clc.demofile = fh;

	Q_strncpyz( clc.demoName, shortPath, sizeof( clc.demoName ) );
	Con_Close();
	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, shortPath, sizeof( cls.servername ) );

	if ( cl_demoPlayer->integer ) {
		while ( CL_MapDownload_Active() ) {
			Sys_Sleep( 50 );
		}
		CL_NDP_PlayDemo( videoRestart );
		return;
	}

	// read demo messages until connected
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED && !CL_MapDownload_Active() ) {
		CL_ReadDemoMessage();
	}

	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	clc.firstDemoFrameSkipped = qfalse;
}


static void CL_CompleteDemoPlay_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteDemoNameRead );
}


/*
====================
CL_StartDemoLoop

Closing the main menu will restart the demo loop
====================
*/
void CL_StartDemoLoop( void ) {
	// start the demo loop again
	Cbuf_AddText ("d1\n");
	cls.keyCatchers = 0;
}

/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
void CL_NextDemo( void ) {
	char	v[MAX_STRING_CHARS];

	Q_strncpyz( v, Cvar_VariableString ("nextdemo"), sizeof(v) );
	v[MAX_STRING_CHARS-1] = 0;
	Com_DPrintf("CL_NextDemo: %s\n", v );
	if (!v[0]) {
		return;
	}

	Cvar_Set ("nextdemo","");
	Cbuf_AddText (v);
	Cbuf_AddText ("\n");
	Cbuf_Execute();
}


///////////////////////////////////////////////////////////////


/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll(void)
{
	cls.fullClientShutDown = qtrue;

	// clear sounds
	S_DisableSounds();
	// shutdown CGame
	CL_ShutdownCGame();
	// shutdown UI
	CL_ShutdownUI();

	// shutdown the renderer
	if ( re.Shutdown ) {
		re.Shutdown( qfalse );
	}

	cls.uiStarted = qfalse;
	cls.cgameStarted = qfalse;
	cls.rendererStarted = qfalse;
	cls.soundRegistered = qfalse;
}


/*
Authorization server protocol
-----------------------------

All commands are text in Q3 out of band packets (leading 0xff 0xff 0xff 0xff).

Whenever the client tries to get a challenge from the server it wants to
connect to, it also blindly fires off a packet to the authorize server:

getKeyAuthorize <challenge> <cdkey>

cdkey may be "demo"


#OLD The authorize server returns a:
#OLD 
#OLD keyAthorize <challenge> <accept | deny>
#OLD 
#OLD A client will be accepted if the cdkey is valid and it has not been used by any other IP
#OLD address in the last 15 minutes.


The server sends a:

getIpAuthorize <challenge> <ip>

The authorize server returns a:

ipAuthorize <challenge> <accept | deny | demo | unknown >

A client will be accepted if a valid cdkey was sent by that ip (only) in the last 15 minutes.
If no response is received from the authorize server after two tries, the client will be let
in anyway.
*/
static void CL_RequestAuthorization()
{
	char key[CDKEY_LEN + 1];

	if ( !cls.authorizeServer.port ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &cls.authorizeServer  ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}

		cls.authorizeServer.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			cls.authorizeServer.ip[0], cls.authorizeServer.ip[1],
			cls.authorizeServer.ip[2], cls.authorizeServer.ip[3],
			BigShort( cls.authorizeServer.port ) );
	}
	if ( cls.authorizeServer.type == NA_BAD ) {
		return;
	}

	int i;
	for (i = 0; (i < CDKEY_LEN) && cl_cdkey[i]; ++i) {
		// the cd key should have already been checked for validity, but just in case:
		if ( ( cl_cdkey[i] >= '0' && cl_cdkey[i] <= '9' )
				|| ( cl_cdkey[i] >= 'a' && cl_cdkey[i] <= 'z' )
				|| ( cl_cdkey[i] >= 'A' && cl_cdkey[i] <= 'Z' )
				) {
			key[i] = cl_cdkey[i];
		} else {
			Com_Error( ERR_DROP, "Invalid CD Key" );
		}
	}
	key[i] = 0;

	int anonymous = 0; // previously set by cl_anonymous
	NET_OutOfBandPrint( NS_CLIENT, cls.authorizeServer, va("getKeyAuthorize %i %s", anonymous, key) );
}


// resend a connect message if the last one has timed out

static void CL_CheckForResend()
{
	// don't send anything if playing back a demo
	if ( clc.demoplaying ) {
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) {
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
		return;
	}

	clc.connectTime = cls.realtime;	// for retransmit requests
	clc.connectPacketCount++;


	switch ( cls.state ) {
	case CA_CONNECTING:
		// requesting a challenge
#if !defined( QC )
		if ( !Sys_IsLANAddress( clc.serverAddress ) ) {
			CL_RequestAuthorization();
		}
#endif
		NET_OutOfBandPrint(NS_CLIENT, clc.serverAddress, "getchallenge");
		break;

	case CA_CHALLENGING:
	    {
            int		port, i;

            char	info[MAX_INFO_STRING];
            char	data[MAX_INFO_STRING];
		    // sending back the challenge
		    port = Cvar_VariableIntegerValue ("net_qport");

		    Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO ), sizeof( info ) );
		    Info_SetValueForKey( info, "protocol", va("%i", PROTOCOL_VERSION ) );
		    Info_SetValueForKey( info, "qport", va("%i", port ) );
		    Info_SetValueForKey( info, "challenge", va("%i", clc.challenge ) );

		    strcpy(data, "connect ");
		    // TTimo adding " " around the userinfo string to avoid truncated userinfo on the server
		    //   (Com_TokenizeString tokenizes around spaces)
		    data[8] = '"';

		    for(i=0;i<strlen(info);i++) {
			    data[9+i] = info[i];	// + (clc.challenge)&0x3;
		    }
		    data[9+i] = '"';
		    data[10+i] = 0;

		    NET_OutOfBandData( NS_CLIENT, clc.serverAddress, (byte *) &data[0], i+10 );
		    // the most current userinfo has been sent, so watch for any
		    // newer changes to userinfo variables
		    cvar_modifiedFlags &= ~CVAR_USERINFO;
		}
		break;

	default:
		Com_Error( ERR_FATAL, "CL_CheckForResend: bad cls.state" );
	}
}


/*
Called by CL_MapLoading, CL_Connect_f, CL_PlayDemo_f, and CL_ParseGamestate, the only
ways a client gets into a game
Also called by Com_Error
*/
void CL_FlushMemory()
{
	// shutdown all the client stuff
	CL_ShutdownAll();

	// if not running a server clear the whole hunk
	if ( !com_sv_running->integer ) {
		// clear the whole hunk
		Hunk_Clear();
		// clear collision map data
		CM_ClearMap();
	}
	else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}

	CL_StartHunkUsers();
}

/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading( void ) {
	if ( !com_cl_running->integer ) {
		return;
	}

	Con_Close();
	cls.keyCatchers = 0;

	// if we are already connected to the local host, stay connected
	if ( cls.state >= CA_CONNECTED && !Q_stricmp( cls.servername, "localhost" ) ) {
		cls.state = CA_CONNECTED;		// so the connect screen is drawn
		Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		Com_Memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		clc.lastPacketSentTime = -9999;
		SCR_UpdateScreen();
	} else {
		CL_Disconnect( qtrue );
		Q_strncpyz( cls.servername, "localhost", sizeof(cls.servername) );
		cls.state = CA_CHALLENGING;		// so the connect screen is drawn
		cls.keyCatchers = 0;
		SCR_UpdateScreen();
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress);
		// we don't need a challenge on the localhost

		CL_CheckForResend();
	}
}


// called before parsing a gamestate

void CL_ClearState()
{
	//S_StopAllSounds();
	Com_Memset( &cl, 0, sizeof( cl ) );
}


/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect( qbool showMainMenu ) {
	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	// shutting down the client so enter full screen ui mode
	Cvar_Set("r_uiFullScreen", "1");

	if ( clc.demorecording ) {
		CL_StopRecord_f ();
	}

	if (clc.download) {
		FS_FCloseFile( clc.download );
		clc.download = 0;
	}
	*clc.downloadTempName = *clc.downloadName = 0;
	Cvar_Set( "cl_downloadName", "" );

	if ( clc.demofile ) {
		FS_FCloseFile( clc.demofile );
		clc.demofile = 0;
	}
	clc.demoplaying = qfalse;

	if ( uivm && showMainMenu ) {
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE );
	}

	SCR_StopCinematic ();
	S_ClearSoundBuffer();

	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( cls.state >= CA_CONNECTED ) {
		CL_AddReliableCommand( "disconnect" );
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
	}

	CL_ClearState();

	// wipe the client connection
	Com_Memset( &clc, 0, sizeof( clc ) );

	cls.state = CA_DISCONNECTED;

	// allow cheats locally
	Cvar_Set( "sv_cheats", "1" );

	// not connected to a pure server anymore
	cl_connectedToPureServer = qfalse;

	// let the FS know we're not connected to a pure server
	// and clear all pak references except to the game QVM
	FS_PureServerSetLoadedPaks( "" );
	FS_PureServerSetReferencedPaks( "", "" );
	FS_ClearPakReferences( FS_CGAME_REF | FS_UI_REF | FS_GENERAL_REF );

	// Stop recording any video
	CL_CloseAVI();
}


/*
adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
*/
void CL_ForwardCommandToServer( const char *string )
{
	const char* cmd = Cmd_Argv(0);

	// ignore key up commands
	if ( cmd[0] == '-' ) {
		return;
	}

	if ( clc.demoplaying || cls.state < CA_CONNECTED || cmd[0] == '+' ) {
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( string );
	} else {
		CL_AddReliableCommand( cmd );
	}
}


/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/


static void CL_ForwardToServer_f()
{
	if ( cls.state != CA_ACTIVE || clc.demoplaying ) {
		Com_Printf ("Not connected to a server.\n");
		return;
	}
	// don't forward the first argument
	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( Cmd_Args() );
	}
}


void CL_Disconnect_f( void )
{
	SCR_StopCinematic();
	if ( cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC ) {
		Com_ErrorExt( ERR_DISCONNECT, EXT_ERRMOD_ENGINE, qfalse, "Disconnected from server" );
	}
}


static void CL_Reconnect( qbool whileConnectingToProxy )
{
	if ( !strlen( cls.servername ) || !strcmp( cls.servername, "localhost" ) ) {
		Com_Printf( "Can't reconnect to localhost.\n" );
		return;
	}

	// Reconnecting via the same proxy doesn't seem to work with QWFWD
	// ("illegible server message" drop error),
	// so we disconnect and wait a bit first to make it work.
	// One second seems to work well enough but let's pad it a bit.
	if ( !whileConnectingToProxy &&
		net_proxy->string[0] != '\0' &&
		!Q_stricmp( net_proxy->string, cls.proxyname ) ) {
		Cbuf_AddText( va( "disconnect\nwaitms 1500\nconnect %s\n", cls.servername ) );
	} else {
		Cbuf_AddText( va( "connect %s\n", cls.servername ) );
	}
}


static void CL_Reconnect_f()
{
	CL_Reconnect( qfalse );
}


static void CL_Connect_f()
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: connect <server>\n");
		return;
	}

	// clear any previous "server full" type messages
	clc.serverMessage[0] = 0;

	const char* const requestedAddress = Cmd_Argv(1);
	const char* connectionAddress = requestedAddress;

	// ye old switcheroo
	if ( net_proxy->string[0] != '\0' && Q_stricmp( net_proxy->string, requestedAddress ) != 0 ) {
		Cvar_Get( "prx", requestedAddress, CVAR_USERINFO );
		Cvar_Set( "prx", requestedAddress );
		connectionAddress = net_proxy->string;
		Q_strncpyz( cls.proxyname, net_proxy->string, sizeof(cls.proxyname) );
	} else {
		// it's not really necessary but it's nice to reflect the changes
		Cvar_Get( "prx", "", CVAR_USERINFO );
		Cvar_Set( "prx", "" );
		cls.proxyname[0] = '\0';
	}

	if ( com_sv_running->integer && !strcmp( requestedAddress, "localhost" ) ) {
		// if running a local server, kill it
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	CL_Disconnect( qtrue );
	Con_Close();

	Q_strncpyz( cls.servername, requestedAddress, sizeof(cls.servername) );

	if (!NET_StringToAdr( connectionAddress, &clc.serverAddress) ) {
		Com_Printf ("Bad server address\n");
		cls.state = CA_DISCONNECTED;
		return;
	}
	if (clc.serverAddress.port == 0) {
		clc.serverAddress.port = BigShort( PORT_SERVER );
	}
	Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", connectionAddress,
		clc.serverAddress.ip[0], clc.serverAddress.ip[1],
		clc.serverAddress.ip[2], clc.serverAddress.ip[3],
		BigShort( clc.serverAddress.port ) );

	// if we aren't playing on a lan, we need to authenticate
	// with the cd key
	if ( NET_IsLocalAddress( clc.serverAddress ) ) {
		cls.state = CA_CHALLENGING;
	} else {
		cls.state = CA_CONNECTING;
	}

	cls.keyCatchers = 0;
	clc.connectTime = -99999;	// CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	// server connection string
	Cvar_Set( "cl_currentServerAddress", connectionAddress );
}


#define MAX_RCON_MESSAGE 1024

// send the rest of the command line over as an unconnected command

static void CL_Rcon_f( void )
{
	if ( !rconPassword->string ) {
		Com_Printf ("You must set 'rconpassword' before\n"
					"issuing an rcon command.\n");
		return;
	}

	char message[MAX_RCON_MESSAGE];
	message[0] = -1;
	message[1] = -1;
	message[2] = -1;
	message[3] = -1;
	message[4] = 0;

	Q_strcat (message, MAX_RCON_MESSAGE, "rcon ");

	Q_strcat (message, MAX_RCON_MESSAGE, rconPassword->string);
	Q_strcat (message, MAX_RCON_MESSAGE, " ");

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
	Q_strcat (message, MAX_RCON_MESSAGE, Cmd_Cmd()+5);

	netadr_t to;
	if ( cls.state >= CA_CONNECTED ) {
		to = clc.netchan.remoteAddress;
	} else {
		if (!strlen(rconAddress->string)) {
			Com_Printf ("You must either be connected,\n"
						"or set the 'rconAddress' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rconAddress->string, &to);
		if (to.port == 0) {
			to.port = BigShort (PORT_SERVER);
		}
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}


static void CL_CompleteRcon_f( int startArg, int compArg )
{
	if ( startArg < compArg )
		Field_AutoCompleteFrom( startArg + 1, compArg, qtrue, qtrue );
}


// if we are pure we need to send back a command with our referenced pk3 checksums

static void CL_SendPureChecksums()
{
	char cMsg[MAX_INFO_VALUE];
	Com_sprintf( cMsg, sizeof(cMsg), "cp " );
	Q_strcat( cMsg, sizeof(cMsg), va("%d ", cl.serverId) );
	Q_strcat( cMsg, sizeof(cMsg), FS_ReferencedPakPureChecksums() );
	CL_AddReliableCommand( cMsg );
}


static void CL_ResetPureClientAtServer()
{
	CL_AddReliableCommand( "vdr" );
}


static void CL_ReferencedPK3List_f( void )
{
	Com_Printf( "Referenced PK3 Names: %s\n", FS_ReferencedPakNames() );
}


static void CL_Configstrings_f( void )
{
	int		i;
	int		ofs;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n");
		return;
	}

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}


static void CL_Clientinfo_f( void )
{
	Com_Printf( "--------- Client Information ---------\n" );
	Com_Printf( "state: %i\n", cls.state );
	Com_Printf( "Server: %s\n", cls.servername );
	Com_Printf( "User info settings:\n" );
	Info_Print( Cvar_InfoString_Big( CVAR_USERINFO ) ); // 1024 chars + 'userinfo '
	Com_Printf( "--------------------------------------\n" );
}


///////////////////////////////////////////////////////////////


void CL_DownloadsComplete()
{
	// if we downloaded files we need to restart the file system
	if (clc.downloadRestart) {
		clc.downloadRestart = qfalse;

		FS_Restart(clc.checksumFeed); // we possibly downloaded a pak, restart the file system to load it

		// inform the server so we get new gamestate info
		CL_AddReliableCommand( "donedl" );

		// by sending the donedl command we request a new gamestate
		// so we don't want to load stuff yet
		return;
	}

	// let the client game init and load data
	cls.state = CA_LOADING;

	// Pump the loop, this may change gamestate!
	Com_EventLoop();

	// if the gamestate was changed by calling Com_EventLoop
	// then we loaded everything already and we don't want to do it again.
	if ( cls.state != CA_LOADING ) {
		return;
	}

	// starting to load a map so we get out of full screen ui mode
	Cvar_Set("r_uiFullScreen", "0");

	// flush client memory and start loading stuff
	// this will also (re)load the UI
	// if this is a local client then only the client part of the hunk
	// will be cleared, note that this is done after the hunk mark has been set
	CL_FlushMemory();

	// initialize the CGame
	cls.cgameStarted = qtrue;
	CL_InitCGame();

	// set pure checksums
	CL_SendPureChecksums();

	CL_WritePacket();
	CL_WritePacket();
	CL_WritePacket();
}


// requests a file to download from the server
// stores it in the current game directory

static void CL_BeginDownload( const char *localName, const char *remoteName )
{
	Com_DPrintf("***** CL_BeginDownload *****\n"
				"Localname: %s\n"
				"Remotename: %s\n"
				"****************************\n", localName, remoteName);

	Q_strncpyz ( clc.downloadName, localName, sizeof(clc.downloadName) );
	Com_sprintf( clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName );

	// Set so UI gets access to it
	Cvar_Set( "cl_downloadName", remoteName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetValue( "cl_downloadTime", cls.realtime );

	clc.downloadBlock = 0;
	clc.downloadCount = 0;

	CL_AddReliableCommand( va("download %s", remoteName) );
}


/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload(void) {
	char *s;
	char *remoteName, *localName;

	// We are looking to start a download here
	if (*clc.downloadList) {
		s = clc.downloadList;

		// format is:
		//  @remotename@localname@remotename@localname, etc.

		if (*s == '@')
			s++;
		remoteName = s;

		if ( (s = strchr(s, '@')) == NULL ) {
			CL_DownloadsComplete();
			return;
		}

		*s++ = 0;
		localName = s;
		if ( (s = strchr(s, '@')) != NULL )
			*s++ = 0;
		else
			s = localName + strlen(localName); // point at the nul byte

		if( cl_allowDownload->integer != -1 ) {
			Com_Error(ERR_DROP, "The id download system is disabled (cl_allowDownload must be -1)");
			return;
		} else {
			CL_BeginDownload( localName, remoteName );
		}

		clc.downloadRestart = qtrue;

		// move over the rest
		memmove( clc.downloadList, s, strlen(s) + 1);

		return;
	}

	CL_DownloadsComplete();
}


// qtrue if found in either "$(fs_gamedir)" or in "baseq3"
static qbool MapDL_FileExists( const char* path )
{
	return FS_FileExistsEx( path, qtrue ) || FS_FileExistsEx( path, qfalse );
}


// returns qtrue if a download started
static qbool CL_StartDownloads()
{
	static unsigned int pakChecksums[MAX_PAKFILES];

	// never launch a download when starting a listen server...
	if (com_sv_running->integer)
		return qfalse;

	// note that CL_SystemInfoChanged won't change our local values of "sv_pure" and "sv_currentPak",
	// so we have to read them from the server's gamestate data
	char mapName[MAX_NAME_LENGTH];
	char mapPath[MAX_NAME_LENGTH];
	const char* const systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO];
	const char* const serverInfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	const qbool pureServer = atoi(Info_ValueForKey(systemInfo, "sv_pure")) == 1;
	const qbool exactMatch = !clc.demoplaying && pureServer;
	Q_strncpyz(mapName, Info_ValueForKey(serverInfo, "mapname"), sizeof(mapName));
	Q_strncpyz(mapPath, va("maps/%s.bsp", mapName), sizeof(mapPath));
	const int mode = cl_allowDownload->integer;
	unsigned int pakChecksum = 0;
	if (sscanf(Info_ValueForKey(serverInfo, "sv_currentPak"), "%d", &pakChecksum) != 1)
		pakChecksum = 0;

	// downloads disabled
	if (mode == 0) {
		if (exactMatch && pakChecksum != 0) {
			// don't allow connections to pure CNQ3 servers if we don't have the right file
			int pakCount;
			FS_MissingPaks(pakChecksums, &pakCount, ARRAY_LEN(pakChecksums));
			for (int i = 0; i < pakCount; ++i) {
				if (pakChecksums[i] == pakChecksum) {
					Com_Printf("\nERROR: You are missing the PK3 file the pure server has loaded the current map from. "
							   "To enable downloads, set cl_allowDownload to 1 (new) or -1 (old).\n\n");
					Com_Error(ERR_DROP, "Missing map! Try cl_allowDownload 1");
					break;
				}
			}
		} else if (exactMatch) {
			// we can't be sure whether we're about to break sv_pure or not :-(
			// it's possible that some referenced files on the server are missing
			char missingfiles[1024];
			if (FS_ComparePaks(missingfiles, sizeof(missingfiles), qfalse)) {
				// NOTE TTimo I would rather have that printed as a modal message box
				// but at this point while joining the game we don't know whether we will successfully join or not
				Com_Printf("\nWARNING: You are missing some files referenced by the server:\n%s\n"
							"To enable downloads, set cl_allowDownload to 1 (new) or -1 (old)\n\n", missingfiles);
			}
		}
		return qfalse;
	}

	// legacy id downloads
	if (mode == -1) {
		if (FS_ComparePaks(clc.downloadList, sizeof(clc.downloadList), qtrue)) {
			Com_Printf("Need paks: %s\n", clc.downloadList);
			if (*clc.downloadList) {
				cls.state = CA_CONNECTED;
				CL_NextDownload();
				return qtrue;
			}
		}
		return qfalse;
	}

	//
	// CNQ3 downloads
	//

	// handle the non-exact case first
	if (!exactMatch) {
		// if we have something of the same name, we're good (enough)
		if (FS_FileIsInPAK(mapPath, NULL, NULL) || MapDL_FileExists(mapPath))
			return qfalse;

		// if we don't, we look for anything of the same name
		if (CL_MapDownload_Start(mapName, qfalse)) {
			cls.state = CA_CONNECTED;
			return qtrue;
		}

		// the above code will trigger a drop error if it fails
		return qfalse;
	}

	// let's first see if we can assume we have the right file
	if (pakChecksum != 0) {
		// CNQ3 pure server: we can directly look for a specific pak file and be 100% sure
		if (FS_PakExists(pakChecksum)) {
			return qfalse;
		}
	} else {
		// other server: no way to 100% prove we have the pak file the server loaded the map from,
		// unless we make sure we have every single pak file the server has...

		// let's suppose we have the following scenario:
		// - the server has bad_a.pk3 and bad_b.pk3 in baseq3
		// - both paks contains "maps/bad.bsp"
		// - bad_a.pk3 and bad_b.pk3 are different files with different (Quake 3 FS) checksums
		// - the server loads the map "bad" from bad_b.pk3 because of search path ordering
		// - the client only has bad_a.pk3 and tries to connect
		// - the client decides everything is fine and connects anyway... oops

		// This happens with the original quake3.exe too and is a design flaw of the pure system.
		// The alternative is for the client to decide not to connect whenever a single pak is missing,
		// but that's just going to unjustifiably antagonize users for the sake of an infrequent issue.
		// Server owners forgetting to delete old paks certainly isn't rare, however.
		// The simplest solutions are diligence and having servers check and warn about duplicate maps.
		if (FS_FileIsInPAK(mapPath, NULL, NULL))
			return qfalse;
	}

	// generate a checksum list of all the pure paks we're missing
	int pakCount;
	if (pakChecksum != 0) {
		pakChecksums[0] = pakChecksum;
		pakCount = 1;
	} else {
		FS_MissingPaks(pakChecksums, &pakCount, ARRAY_LEN(pakChecksums));
	}

	if (pakCount > 0) {
		const qbool dlStarted = pakCount == 1 ?
			// we know exactly which pk3 we need, we don't send a map name to the server
			// (but we use one for error messages)
			CL_PakDownload_Start(pakChecksums[0], qfalse, mapName) :
			// we send the map's name and a list of pk3 checksums (qmd4) to the server
			CL_MapDownload_Start_PakChecksums(mapName, pakChecksums, pakCount, exactMatch);

		if (dlStarted) {
			cls.state = CA_CONNECTED;
			return qtrue;
		}
		return qfalse;
	}

	return qfalse;
}


/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads(void) {
	
	if(!CL_StartDownloads())
	{
		CL_DownloadsComplete();
	}
}


/*
Sometimes the server can drop the client and the netchan based
disconnect can be lost.  If the client continues to send packets
to the server, the server will send out of band disconnect packets
to the client so it doesn't have to wait for the full timeout period.
*/
static void CL_DisconnectPacket( const netadr_t& from )
{
	if ( cls.state < CA_AUTHORIZING ) {
		return;
	}

	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) ) {
		return;
	}

	// if we have received packets within three seconds, ignore it
	// (it might be a malicious spoof)
	if ( cls.realtime - clc.lastPacketTime < 3000 ) {
		return;
	}

	// drop the connection
	Com_Printf( "Server disconnected for unknown reason\n" );
	Cvar_Set("com_errorMessage", "Server disconnected for unknown reason\n" );
	CL_Disconnect( qtrue );
}


// responses to broadcasts, etc

static void CL_ConnectionlessPacket( const netadr_t& from, msg_t* msg )
{
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );	// skip the -1

	char* s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	const char* c = Cmd_Argv(0);
	Com_DPrintf ("CL packet %s: %s\n", NET_AdrToString(from), c);

	// challenge from the server we are connecting to
	if ( !Q_stricmp(c, "challengeResponse") ) {
		if ( cls.state != CA_CONNECTING ) {
			Com_Printf( "Unwanted challenge response received.  Ignored.\n" );
		} else {
			// start sending challenge repsonse instead of challenge request packets
			clc.challenge = atoi(Cmd_Argv(1));
			cls.state = CA_CHALLENGING;
			clc.connectPacketCount = 0;
			clc.connectTime = -99999;

			// take this address as the new server address.  This allows
			// a server proxy to hand off connections to multiple servers
			clc.serverAddress = from;
			Com_DPrintf ("challengeResponse: %d\n", clc.challenge);
		}
		return;
	}

	// server connection
	if ( !Q_stricmp(c, "connectResponse") ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf ("connectResponse packet while not connecting.  Ignored.\n");
			return;
		}
		if ( !NET_CompareBaseAdr( from, clc.serverAddress ) ) {
			Com_Printf( "connectResponse from a different address.  Ignored.\n" );
			Com_Printf( "%s should have been %s\n", NET_AdrToString( from ), 
				NET_AdrToString( clc.serverAddress ) );
			return;
		}
		Netchan_Setup (NS_CLIENT, &clc.netchan, from, Cvar_VariableIntegerValue( "net_qport" ) );
		cls.state = CA_CONNECTED;
		clc.lastPacketSentTime = -9999;		// send first packet immediately
		return;
	}

	// server responding to an info broadcast
	if ( !Q_stricmp(c, "infoResponse") ) {
		CL_ServerInfoPacket( from, msg );
		return;
	}

	// server responding to a get playerlist
	if ( !Q_stricmp(c, "statusResponse") ) {
		CL_ServerStatusResponse( from, msg );
		return;
	}

	// a disconnect message from the server, which will happen if the server
	// dropped the connection but it is still getting packets from us
	if (!Q_stricmp(c, "disconnect")) {
		CL_DisconnectPacket( from );
		return;
	}

	// echo request from server
	if ( !Q_stricmp(c, "echo") ) {
		NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
		return;
	}

	// cd check
	if ( !Q_stricmp(c, "keyAuthorize") ) {
		// we don't use these now, so dump them on the floor
		return;
	}

	// global MOTD from id
	if ( !Q_stricmp(c, "motd") ) {
		return;
	}

	// echo request from server
	if ( !Q_stricmp(c, "print") ) {
		s = MSG_ReadString( msg );
		Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
		Com_Printf( "%s", s );
		if ( !Q_stricmpn(s, "/reconnect ASAP!", 16) ) {
			CL_Reconnect( qtrue );
		}
		return;
	}

	// echo request from server
	if ( !Q_strncmp(c, "getserversResponse", 18) ) {
		CL_ServersResponsePacket( from, msg );
		return;
	}

	Com_DPrintf ("Unknown connectionless packet command.\n");
}


// a packet has arrived from the main event loop

void CL_PacketEvent( netadr_t from, msg_t* msg )
{
	clc.lastPacketTime = cls.realtime;

	if ( msg->cursize >= 4 && *(int *)msg->data == -1 ) {
		CL_ConnectionlessPacket( from, msg );
		return;
	}

	if ( cls.state < CA_CONNECTED ) {
		return;		// can't be a valid sequenced packet
	}

	if ( msg->cursize < 4 ) {
		Com_Printf( "%s: Runt packet\n", NET_AdrToString( from ) );
		return;
	}

	//
	// packet from server
	//
	if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) ) {
		Com_DPrintf( "%s:sequenced packet without connection\n", NET_AdrToString( from ) );
		// FIXME: send a client disconnect?
		return;
	}

	if (!CL_Netchan_Process( &clc.netchan, msg )) {
		return;		// out of order, duplicated, etc
	}

	// the header is different lengths for reliable and unreliable messages
	int headerBytes = msg->readcount;

	// track the last message received so it can be returned in 
	// client messages, allowing the server to detect a dropped
	// gamestate
	clc.serverMessageSequence = LittleLong( *(int *)msg->data );

	clc.lastPacketTime = cls.realtime;
	CL_ParseServerMessage( msg );

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if ( clc.demorecording && !clc.demowaiting ) {
		CL_WriteDemoMessage( msg, headerBytes );
	}
}


static void CL_CheckTimeout()
{
	if (clc.demoplaying && (com_timescale->value == 0)) {
		cl.timeoutcount = 0;
		return;
	}

	const int timeout = clc.download ? 5000 : (cl_timeout->integer * 1000);
	if ( ( !CL_Paused() || !sv_paused->integer )
		&& cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC
		&& cls.realtime - clc.lastPacketTime > timeout) {
		if (++cl.timeoutcount > 5) {	// timeoutcount saves debugger
			Com_Error( ERR_DROP, "Server connection timed out" );
			return;
		}
	} else {
		cl.timeoutcount = 0;
	}
}


static void CL_CheckUserinfo()
{
	// don't add reliable commands when not yet connected
	if ( cls.state < CA_CHALLENGING ) {
		return;
	}
	// don't overflow the reliable command buffer when paused
	if ( CL_Paused() ) {
		return;
	}
	// send a reliable userinfo update if needed
	if ( cvar_modifiedFlags & CVAR_USERINFO ) {
		cvar_modifiedFlags &= ~CVAR_USERINFO;
		CL_AddReliableCommand( va("userinfo \"%s\"", Cvar_InfoString( CVAR_USERINFO ) ) );
	}
}


void CL_Frame( int msec )
{
	if ( !com_cl_running->integer ) {
		return;
	}

	if ( cls.cddialog ) {
		// bring up the cd error dialog if needed
		cls.cddialog = qfalse;
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NEED_CD );
	} else if ( cls.state == CA_DISCONNECTED && !( cls.keyCatchers & KEYCATCH_UI )
		&& !com_sv_running->integer ) {
		// if disconnected, bring up the menu
		S_StopAllSounds();
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
	}

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && cl_aviFrameRate->integer && msec ) {
		// save the current screen
		CL_TakeVideoFrame();
		// fixed time for next frame
		msec = (int)ceil( (1000.0f / cl_aviFrameRate->value) * com_timescale->value );
		if (msec == 0) {
			msec = 1;
		}
	}

	// save the msec before checking pause
	cls.realFrametime = msec;

	// decide the simulation time
	cls.frametime = msec;

	cls.realtime += cls.frametime;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph ( cls.realFrametime * 0.25, 0 );
	}

	// update the client's own GUI
	CL_IMGUI_Frame();

	// advance the current map download, if any
	CL_MapDownload_Continue();

	// see if we need to update any userinfo
	CL_CheckUserinfo();

	// if we haven't gotten a packet in a long time,
	// drop the connection
	CL_CheckTimeout();

	// send intentions now
	CL_SendCmd();

	// resend a connection request if necessary
	CL_CheckForResend();

	// decide on the serverTime to render
	CL_SetCGameTime();

	// update the screen
	SCR_UpdateScreen();

	// update audio
	S_Update();

	// advance local effects for next frame
	SCR_RunCinematic();

	Con_RunConsole();

	cls.framecount++;
}


qbool CL_ShouldSleep()
{
	return re.ShouldSleep();
}


///////////////////////////////////////////////////////////////


int CL_ScaledMilliseconds()
{
	return Sys_Milliseconds()*com_timescale->value;
}


qbool CL_Paused()
{
	// Summary: We keep the client pause active until we get a new server time through a snapshot.
	//
	// Without this fix, after the client pause ends and before we get a new server snapshot,
	// CL_AdjustTimeDelta will update cl.serverTime to a higher value
	// and that value is sent in the ucmd_t data to the server (CL_FinishMove).
	// Since the timestamp is "from the future", anything that follows needs to be
	// at least as high, which won't happen for a while because cl.serverTime will get reset to
	// something correct (i.e. lower) upon reception of the next snapshot from the server.
	// In other words: After a client pause of X seconds, the server ignores the client's input
	// for X seconds and the client is basically timing out. Oops.
	return cl_paused->integer || cl_paused->modified;
}


static void CL_InitRenderer()
{
	// this sets up the renderer and calls R_Init
	re.BeginRegistration( &cls.glconfig );

	// load character sets
	cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" );
	cls.whiteShader = re.RegisterShader( "white" );
	g_console_field_width = CONSOLE_WIDTH;
	g_consoleField.widthInChars = g_console_field_width;
}


// after the server has cleared the hunk, these will need to be restarted
// this is the only place that any of these functions are called from

void CL_StartHunkUsers( void ) {
	if (!com_cl_running) {
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	if ( !cls.rendererStarted ) {
		cls.rendererStarted = qtrue;
		CL_InitRenderer();
	}

	if ( !cls.soundStarted ) {
		cls.soundStarted = qtrue;
		S_Init();
	}

	if ( !cls.soundRegistered ) {
		cls.soundRegistered = qtrue;
		S_BeginRegistration();
	}

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;
		CL_InitUI();
	}
}


static void QDECL CL_RefPrintf( printParm_t print_level, PRINTF_FORMAT_STRING const char* fmt, ... )
{
	va_list va;
	char msg[MAXPRINTMSG];

	va_start( va, fmt );
	Q_vsnprintf( msg, sizeof(msg), fmt, va );
	va_end( va );

	if ( print_level == PRINT_ALL ) {
		Com_Printf( "%s", msg );
	} else if ( print_level == PRINT_WARNING ) {
		Com_Printf( S_COLOR_YELLOW "%s", msg );
	} else if ( print_level == PRINT_ERROR ) {
		Com_Printf( S_COLOR_RED "%s", msg );
	} else if ( print_level == PRINT_DEVELOPER ) {
		Com_DPrintf( S_COLOR_CYAN "%s", msg );
	}
}


static void* CL_RefMalloc( int size )
{
	return Z_TagMalloc( size, TAG_RENDERER );
}


static void CL_ShutdownRef()
{
	if ( !re.Shutdown ) {
		return;
	}
	re.Shutdown( cls.fullClientShutDown );
	Com_Memset( &re, 0, sizeof( re ) );
}


static void RI_Cmd_RegisterTable( const cmdTableItem_t* cmds, int count )
{
	Cmd_RegisterTable( cmds, count, MODULE_RENDERER );
}


static void RI_Cvar_RegisterTable( const cvarTableItem_t* cvars, int count )
{
	Cvar_RegisterTable( cvars, count, MODULE_RENDERER );
}


static void RI_Cmd_UnregisterModule()
{
	Cmd_UnregisterModule( MODULE_RENDERER );
}


static void CL_InitRef()
{
	refimport_t ri;

	ri.SetConsoleVisibility = Con_SetConsoleVisibility;
	ri.Cmd_RegisterTable = RI_Cmd_RegisterTable;
	ri.Cmd_UnregisterModule = RI_Cmd_UnregisterModule;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Printf = CL_RefPrintf;
	ri.Error = Com_Error;
	ri.Milliseconds = CL_ScaledMilliseconds;
	ri.Microseconds = Sys_Microseconds;
	ri.Malloc = CL_RefMalloc;
	ri.Free = Z_Free;
#ifdef HUNK_DEBUG
	ri.Hunk_AllocDebug = Hunk_AllocDebug;
#else
	ri.Hunk_Alloc = Hunk_Alloc;
#endif
	ri.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;
	ri.CM_DrawDebugSurface = CM_DrawDebugSurface;
	ri.FS_ReadFile = FS_ReadFile;
	ri.FS_ReadFilePak = FS_ReadFilePak;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_WriteFile = FS_WriteFile;
	ri.FS_FreeFileList = FS_FreeFileList;
	ri.FS_ListFiles = FS_ListFiles;
	ri.FS_GetPakPath = FS_GetPakPath;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_SetHelp = Cvar_SetHelp;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_RegisterTable = RI_Cvar_RegisterTable;

	// cinematic stuff

	ri.CIN_GrabCinematic = CIN_GrabCinematic;
	ri.CIN_PlayCinematic = CIN_PlayCinematic;
	ri.CIN_RunCinematic = CIN_RunCinematic;

	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;

	re = *(GetRefAPI(&ri));

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );
}


/*
Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
*/
static void CL_Vid_Restart_f()
{
	cls.fullClientShutDown = qfalse;

	// Settings may have changed so stop recording now
	CL_CloseAVI();

	S_StopAllSounds(); // don't let them loop during the restart

	CL_ShutdownUI();
	CL_ShutdownCGame();
	CL_ShutdownInput();
	CL_ShutdownRef();

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );
	// reinitialize the filesystem if the game directory or checksum has changed
	FS_ConditionalRestart( clc.checksumFeed );

	cls.rendererStarted = qfalse;
	cls.uiStarted = qfalse;
	cls.cgameStarted = qfalse;
	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	// if not running a server clear the whole hunk
	if ( !com_sv_running->integer ) {
		Hunk_Clear();
	}
	else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}

	// initialize the renderer interface
	CL_InitRef();

	// initialize input
	CL_InitInput();

	// startup all the client stuff
	CL_StartHunkUsers();

	// we don't really technically need to run everything again,
	// but trying to optimize parts out is very likely to lead to nasty bugs
	if ( clc.demoplaying && clc.newDemoPlayer ) {
		Cmd_TokenizeString( va("demo \"%s\"", clc.demoName) );
		CL_PlayDemo( qtrue );
	}
	// start the cgame if connected
	else if ( cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC ) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}
}


// restart the sound subsystem
// the cgame must also be forced to restart because handles will be invalid

static void CL_Snd_Restart_f()
{
	S_Shutdown();
	S_Init();

	CL_Vid_Restart_f();
}


//===========================================================================================

#if defined( QC )
void CL_SetChampion_f( void ) {
	const char	*arg;
	char	name[256];

	arg = Cmd_Argv( 1 );
	if ( arg[0] ) {
		Cvar_Set( "champion", arg );
	} else {
		Cvar_VariableStringBuffer( "champion", name, sizeof(name) );
		Com_Printf("champion is set to %s\n", name);
	}
}
#endif

///////////////////////////////////////////////////////////////


static void CL_Video_f()
{
	char s[ MAX_OSPATH ];

	if( !clc.demoplaying || clc.newDemoPlayer )
	{
		Com_Printf( "^3ERROR: ^7/%s is only enabled in the old demo player\n", Cmd_Argv( 0 ) );
		return;
	}

	if( Cmd_Argc( ) == 2 )
	{
		Q_strncpyz( s, Cmd_Argv( 1 ), sizeof( s ) );
	}
	else
	{
		qtime_t t;
		Com_RealTime( &t );
		Com_sprintf( s, sizeof( s ), "%d_%02d_%02d-%02d_%02d_%02d",
				1900+t.tm_year, 1+t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );
	}

	CL_OpenAVIForWriting( s );
}


static void CL_StopVideo_f()
{
	if( !clc.demoplaying || clc.newDemoPlayer )
	{
		Com_Printf( "^3ERROR: ^7/%s is only enabled in the old demo player\n", Cmd_Argv( 0 ) );
		return;
	}

	if( !CL_VideoRecording() )
	{
		Com_Printf( "No video is being recorded\n" );
		return;
	}

	CL_CloseAVI();
}


///////////////////////////////////////////////////////////////


static void CL_ShowIP_f()
{
	Sys_ShowIP();
}


qbool CL_CDKeyValidate( const char *key, const char *checksum )
{
	char	ch;
	byte	sum;
	char	chs[3];
	int i, len;

	len = strlen(key);
	if( len != CDKEY_LEN ) {
		return qfalse;
	}

	if( checksum && strlen( checksum ) != CDCHKSUM_LEN ) {
		return qfalse;
	}

	sum = 0;
	// for loop gets rid of conditional assignment warning
	for (i = 0; i < len; i++) {
		ch = *key++;
		if (ch>='a' && ch<='z') {
			ch -= 32;
		}
		switch( ch ) {
		case '2':
		case '3':
		case '7':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'G':
		case 'H':
		case 'J':
		case 'L':
		case 'P':
		case 'R':
		case 'S':
		case 'T':
		case 'W':
			sum += ch;
			continue;
		default:
			return qfalse;
		}
	}

	sprintf(chs, "%02x", sum);

	if (checksum && !Q_stricmp(chs, checksum)) {
		return qtrue;
	}

	if (!checksum) {
		return qtrue;
	}

	return qfalse;
}


static void CL_PrintDownloadPakUsage()
{
	Com_Printf( "Usage: %s checksum (signed decimal, '0x' or '0X' prefix for hex)\n", Cmd_Argv(0) );
}


static void CL_DownloadPak_f()
{
	unsigned int checksum;
	if ( Cmd_Argc() != 2 ) {
		CL_PrintDownloadPakUsage();
		return;
	}

	const char* const arg1 = Cmd_Argv(1);
	const int length = strlen( arg1 );
	if ( length > 2 && arg1[0] == '0' && (arg1[1] == 'x' || arg1[1] == 'X') ) {
		if ( sscanf(arg1 + 2, "%x", &checksum) != 1 ) {
			CL_PrintDownloadPakUsage();
			return;
		}
	} else {
		int crc32;
		if ( sscanf(arg1, "%d", &crc32) != 1 ) {
			CL_PrintDownloadPakUsage();
			return;
		}
		checksum = (unsigned int)crc32;
	}

	if ( checksum == 0 ) {
		Com_Printf( "%s: invalid checksum 0\n", Cmd_Argv(0) );
		return;
	}

	if ( FS_PakExists(checksum) ) {
		Com_Printf( "%s: pk3 with checksum 0x%08x (%d) already present\n", Cmd_Argv(0), checksum, (int)checksum );
		return;
	}

	CL_PakDownload_Start( checksum, qtrue, NULL );
}


static void CL_DownloadMap( qbool forceDL )
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: %s mapname\n", Cmd_Argv(0) );
		return;
	}

	const char* const mapName = Cmd_Argv(1);
	if ( !forceDL ) {
		const char* const mapPath = va( "maps/%s.bsp", mapName );
		if ( MapDL_FileExists(mapPath) || FS_FileIsInPAK(mapPath, NULL, NULL) ) {
			Com_Printf( "Map already exists! To force the download, use /%sf\n", Cmd_Argv(0) );
			return;
		}
	}

	CL_MapDownload_Start(mapName, qtrue);
}


static void CL_DownloadMap_f()
{
	CL_DownloadMap( qfalse );
}


static void CL_ForceDownloadMap_f()
{
	CL_DownloadMap( qtrue );
}


static void CL_CancelDownload_f()
{
	CL_MapDownload_Cancel();
}


static void CL_PlayDemo_f()
{
	CL_PlayDemo( qfalse );
}


static const cvarTableItem_t cl_cvars[] =
{
	{ &cl_timeout, "cl_timeout", "200", 0, CVART_INTEGER, "30", "300", "connection time-out, in seconds" },
	{ NULL, "cl_timeNudge", "0", CVAR_TEMP, CVART_INTEGER, NULL, NULL, help_cl_timeNudge },
	{ &cl_shownet, "cl_shownet", "0", CVAR_TEMP, CVART_INTEGER, "-2", "4", help_cl_shownet },
	{ &cl_showSend, "cl_showSend", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, help_cl_showSend },
	{ &cl_showTimeDelta, "cl_showTimeDelta", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "prints delta adjustment values and events" },
	{ &rconPassword, "rconPassword", "", CVAR_TEMP, CVART_STRING, NULL, NULL, help_rconPassword },
	{ &cl_timedemo, "timedemo", "0", 0, CVART_BOOL, NULL, NULL, "demo benchmarking mode" },
	{
		&cl_aviFrameRate, "cl_aviFrameRate", "50", CVAR_ARCHIVE, CVART_INTEGER, "24", "250", help_cl_aviFrameRate,
		"AVI video framerate", CVARCAT_DEMO, "", ""
	},
	{
		&cl_aviMotionJpeg, "cl_aviMotionJpeg", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_cl_aviMotionJpeg,
		"AVI motion JPEG", CVARCAT_DEMO, "", ""
	},
	{
		&cl_ffmpeg, "cl_ffmpeg", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL,
			"use FFmpeg for video export\n"
			"Pipes video through FFmpeg instead of writing raw .avi files.",
		"Use FFmpeg", CVARCAT_DEMO, "Uses FFmpeg instead of writing raw .avi files", ""
	},
	{
		&cl_ffmpegCommand, "cl_ffmpegCommand", "-movflags faststart -bf 2 -c:v libx264 -preset medium -crf 23 -vf format=yuv420p -c:a aac -b:a 320k",
			CVAR_ARCHIVE, CVART_STRING, NULL, NULL,
			"FFmpeg encode settings\n"
			"The full command-line options for the output file.",
		"FFmpeg encode settings", CVARCAT_DEMO, "Command-line options for the output file", ""
	},
	{
		&cl_ffmpegExePath, "cl_ffmpegExePath", "ffmpeg", CVAR_ARCHIVE, CVART_STRING, NULL, NULL,
			"FFmpeg executable path\n"
			"The path cannot contain spaces.",
		"FFmpeg executable path", CVARCAT_DEMO, "The path cannot contain spaces", ""
	},
	{
		&cl_ffmpegOutPath, "cl_ffmpegOutPath", "", CVAR_ARCHIVE, CVART_STRING, NULL, NULL,
			"FFmpeg output directory\n"
			"Leave empty to write to cpma/videos as with .avi files.",
		"FFmpeg output directory", CVARCAT_DEMO, "Leave empty to write to cpma/videos as with .avi files", ""
	},
	{
		&cl_ffmpegOutExt, "cl_ffmpegOutExt", "mp4", CVAR_ARCHIVE, CVART_STRING, NULL, NULL, "FFmpeg output file extension",
		"FFmpeg output file extension", CVARCAT_DEMO, "", ""
	},
	{
		&cl_ffmpegLog, "cl_ffmpegLog", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL,
			"FFmpeg log file creation\n"
			"Creates 1 log file per capture.",
		"FFmpeg log file creation", CVARCAT_DEMO, "Creates 1 log file per capture", ""
	},
	{ &rconAddress, "rconAddress", "", 0, CVART_STRING, NULL, NULL, help_rconAddress },
	{
		&cl_maxpackets, "cl_maxpackets", "125", CVAR_ARCHIVE, CVART_INTEGER, "15", "125", "max. packet upload rate",
		"Max. packets", CVARCAT_NETWORK, "", ""
	},
	{
		&cl_packetdup, "cl_packetdup", "1", CVAR_ARCHIVE, CVART_INTEGER, "0", "5", "number of extra transmissions per packet",
		"Packet duplication", CVARCAT_NETWORK, "Sets the number of extra copies per packet", "Use if you experience packet loss"
	},
	{
		&cl_allowDownload, "cl_allowDownload", "1", CVAR_ARCHIVE, CVART_INTEGER, "-1", "1", help_cl_allowDownload,
		"PK3 download mode", CVARCAT_NETWORK, "", "",
		CVAR_GUI_VALUE("-1", "Quake 3", "Can always find PK3 files\nServer must allow it\nVery slow")
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("1", "CNQ3", "Map server may not have the file\nIndependent of server\nFast")
	},
	{ &cl_inGameVideo, "r_inGameVideo", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "enables roq video playback" },
	{ &cl_serverStatusResendTime, "cl_serverStatusResendTime", "750", 0, CVART_INTEGER, "500", "1000", "milli-seconds to wait before resending getstatus" },
	{ NULL, "cl_maxPing", "999", CVAR_ARCHIVE, CVART_INTEGER, "80", "999", "max. ping for the server browser" },
	{
		NULL, "name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE, CVART_STRING, NULL, NULL, "your name",
		"Player name", CVARCAT_GENERAL, "", ""
	},
	{
		NULL, "rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE, CVART_INTEGER, "4000", "99999", "network transfer rate",
		"Transfer rate", CVARCAT_NETWORK, "You'll generally want 25K to 30K", ""
	},
	{ NULL, "snaps", "30", CVAR_USERINFO | CVAR_ARCHIVE, CVART_INTEGER }, // documented by the mod
	{ NULL, "password", "", CVAR_USERINFO, CVART_STRING, NULL, NULL, "used by /" S_COLOR_CMD "connect" },
	{
		&cl_matchAlerts, "cl_matchAlerts", "7", CVAR_ARCHIVE, CVART_BITMASK, "0", XSTRING(MAF_MAX), help_cl_matchAlerts,
		"Match alerts", CVARCAT_GENERAL, "Lets you know when a match is starting", "",
		CVAR_GUI_VALUE("0", "When unfocused", "Otherwise only when minimized")
		CVAR_GUI_VALUE("1", "Flash the task bar", "")
		CVAR_GUI_VALUE("2", "Beep once", "")
		CVAR_GUI_VALUE("3", "Unmute", "")
	},
	{ &net_proxy, "net_proxy", "", CVAR_TEMP, CVART_STRING, NULL, NULL, help_net_proxy },
	{
		&cl_demoPlayer, "cl_demoPlayer", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_cl_demoPlayer,
		"New demo player", CVARCAT_DEMO, "Enables the new demo player with rewind support", ""
	},
	{
		&cl_escapeAbortsDemo, "cl_escapeAbortsDemo", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "pressing escape aborts demo playback",
		"Escape aborts demo", CVARCAT_DEMO, "Pressing escape aborts demo playback", ""
	},
};


static const cmdTableItem_t cl_cmds[] =
{
	{ "cmd", CL_ForwardToServer_f, NULL, "forwards all arguments as a command to the server" },
	{ "configstrings", CL_Configstrings_f, NULL, "prints all non-empty config strings" },
	{ "clientinfo", CL_Clientinfo_f, NULL, "prints some client settings" },
	{ "snd_restart", CL_Snd_Restart_f, NULL, "restarts the sound system" },
	{ "vid_restart", CL_Vid_Restart_f, NULL, "restarts the video system" },
	{ "disconnect", CL_Disconnect_f, NULL, "disconnects from the current server" },
	{ "record", CL_Record_f, CL_CompleteDemoRecord_f, "starts recording a demo" },
	{ "demo", CL_PlayDemo_f, CL_CompleteDemoPlay_f, "starts demo playback" },
	{ "cinematic", CL_PlayCinematic_f, NULL, "starts playback of a .roq video file" },
	{ "stoprecord", CL_StopRecord_f, NULL, "stops demo recording" },
	{ "connect", CL_Connect_f, NULL, "connects to a server" },
	{ "reconnect", CL_Reconnect_f, NULL, "reconnects to the current or last server" },
	{ "localservers", CL_LocalServers_f, NULL, "finds and prints local LAN servers" },
	{ "globalservers", CL_GlobalServers_f, NULL, "requests server lists from master servers" },
	{ "rcon", CL_Rcon_f, CL_CompleteRcon_f, "executes the arguments as a command on the server" },
	{ "ping", CL_Ping_f, NULL, "pings a server" },
	{ "serverstatus", CL_ServerStatus_f, NULL, "prints server status and player list" },
	{ "showip", CL_ShowIP_f, NULL, "shows your open IP address(es)" },
	{ "fs_referencedList", CL_ReferencedPK3List_f, NULL, "prints the names of referenced pak files" },
	{ "video", CL_Video_f, NULL, "starts writing a .avi file" },
	{ "stopvideo", CL_StopVideo_f, NULL, "stops writing the .avi file" },
	{ "dlpak", CL_DownloadPak_f, NULL, "starts a pk3 download by checksum if not existing" },
	{ "dlmap", CL_DownloadMap_f, NULL, "starts a pk3 download by map name if not existing" },
	{ "dlmapf", CL_ForceDownloadMap_f, NULL, "start a pk3 download by map name" },
	{ "dlstop", CL_CancelDownload_f, NULL, "stops the current pk3 download" },
#if defined( QC )
	{ "champion", CL_SetChampion_f, NULL, "changes the selected champion" },
#endif
};


struct cvarTableItemCPMA_t {
	const char*		name;
	cvarType_t		type;
	const char*		guiName;
	int				categories;
	const char*		guiDesc;
	const char*		guiHelp;
	const char*		guiValues;
};


static const cvarTableItemCPMA_t cpma_cvars[] =
{
	{
		"cg_damageDraw", CVART_BOOL,
		"Screen blood", CVARCAT_GRAPHICS, "Draws blood when taking damage", ""
	},
	{
		"cg_deadBodyDarken", CVART_BOOL,
		"Grey corpses", CVARCAT_GRAPHICS, "Renders dead players in dark grey", ""
	},
	{
		"cg_redTeamColors", CVART_COLOR_CHBLS,
		"Red player colors", CVARCAT_GRAPHICS, "Red team player colors", ""
	},
	{
		"cg_blueTeamColors", CVART_COLOR_CHBLS,
		"Blue player colors", CVARCAT_GRAPHICS, "Blue team player colors", ""
	},
	{
		"cg_forceTeamColors", CVART_BITMASK,
		"Force team player colors", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("0", "Own team in first-person", "")
		CVAR_GUI_VALUE("1", "Enemy team in first-person", "")
		CVAR_GUI_VALUE("2", "Free-float camera", "")
		CVAR_GUI_VALUE("3", "Only for CTF/NTF/CTFS", "")
	},
	{
		"cg_teamModel", CVART_STRING,
		"Team model", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_forceTeamModel", CVART_BOOL,
		"Force team model", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_enemyColors", CVART_COLOR_CHBLS,
		"Enemy player colors", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_enemyModel", CVART_STRING,
		"Enemy model", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_fallKick", CVART_BOOL,
		"Fall kick", CVARCAT_GAMEPLAY, "Makes the camera bounce after a fall", ""
	},
	{
		"cg_forceColors", CVART_BOOL,
		"Force colors on teammates", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_gunOffset", CVART_STRING,
		"View weapon position", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_itemFX", CVART_BITMASK,
		"Item effects", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("0", "Bob up and down", "")
		CVAR_GUI_VALUE("1", "Rotate", "Asymmetric items will always rotate")
		CVAR_GUI_VALUE("2", "Scale up on respawn", "")
	},
	{
		"cg_lagHax", CVART_INTEGER,
		"Adaptive prediction", CVARCAT_NETWORK, "Also does a bit of lag compensation", "",
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("-1", "As much as allowed", "")
	},
	{
		"cg_lightningImpact", CVART_INTEGER,
		"LG impact style", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("0", "Nothing", "")
		CVAR_GUI_VALUE("1", "Impact", "")
		CVAR_GUI_VALUE("2", "Impact and sparks", "")
	},
	{
		"cg_muzzleFlash", CVART_BOOL,
		"Muzzle flash", CVARCAT_GRAPHICS, "Draws a muzzle flash when firing the view weapon", ""
	},
	{
		"cg_noAmmoChange", CVART_INTEGER,
		"No-ammo switch", CVARCAT_GAMEPLAY, "Allows from/to out-of-ammo weapon changes", "",
		CVAR_GUI_VALUE("0", "Switch, disallow 0 ammo", "Switches to the next weapon. Can't select a weapon with no ammo")
		CVAR_GUI_VALUE("1", "Switch, allow 0 ammo", "Switches to the next weapon. Can select a weapon with no ammo")
		CVAR_GUI_VALUE("2", "No switch, allow 0 ammo", "Keeps the current weapon. Can select a weapon with no ammo")
	},
	{
		"cg_noHitBeep", CVART_BOOL,
		"Hit beeps", CVARCAT_SOUND | CVARCAT_GAMEPLAY, "", "",
		CVAR_GUI_VALUE("1", "Silence", "")
		CVAR_GUI_VALUE("0", "Hit beeps", "")
	},
	{
		"cg_nudge", CVART_INTEGER,
		"Time nudge", CVARCAT_NETWORK, "", ""
	},
	{
		"cg_optimiseBW", CVART_BITMASK,
		"Bandwidth optimizations", CVARCAT_NETWORK, "", "",
		CVAR_GUI_VALUE("0", "Server-to-client", "Usually forced by servers")
		CVAR_GUI_VALUE("1", "Client-to-server", "Only use if your upload rate is dial-up level")
	},
	{
		"cg_railCoreWidth", CVART_INTEGER,
		"Railgun core width", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_railRingStep", CVART_INTEGER,
		"Railgun ring step", CVARCAT_GRAPHICS, "Distance between the rail rings", ""
	},
	{
		"cg_railRingWidth", CVART_INTEGER,
		"Railgun ring width", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_railStyle", CVART_BITMASK,
		"Railgun trail style", CVARCAT_GRAPHICS, "Visual elements of a railgun trail", "",
		CVAR_GUI_VALUE("0", "Core", "")
		CVAR_GUI_VALUE("1", "Spiral", "")
		CVAR_GUI_VALUE("2", "Rings", "")
	},
	{
		"cg_smoke_SG", CVART_BOOL,
		"Shotgun smoke", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_smokeRadius_RL", CVART_FLOAT,
		"Rocket launcher smoke radius", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_smokeRadius_GL", CVART_FLOAT,
		"Grenade launcher smoke radius", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_xerpClients", CVART_INTEGER,
		"Extrapolate clients", CVARCAT_NETWORK, "", "",
		CVAR_GUI_VALUE("-1", "Hacked extrapolation", "Intended for high ping")
		CVAR_GUI_VALUE("0", "No extrapolation", "Fine for low ping")
		CVAR_GUI_VALUE("1", "Original Q3 behavior", "Just like the original cg_smoothClients code")
	},
	{
		"cg_trueLightning", CVART_FLOAT,
		"Lightning gun beam prediction", CVARCAT_GRAPHICS | CVARCAT_NETWORK | CVARCAT_GAMEPLAY, "",
		"LG beam prediction control\n" \
		"   -1 = Pure client-side (no sway, no beam drawn)\n"
		"    0 = Pure server-side (sways the most)\n"
		"    1 = Pure client-side (no sway)\n"
		"Fractional values: mix between server and client rendering\n"
		"Negative   values: the beam isn't drawn\n"
		"Negative fractional values still affect impact mark placement."
	},
	{
		"cg_viewAdjustments", CVART_BOOL,
		"Enable view bobbing", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_fragSound", CVART_STRING,
		"Kill sound", CVARCAT_SOUND, "", "",
		CVAR_GUI_VALUE("0", "No sound", "")
		CVAR_GUI_VALUE("1", "Tonal impact: D", "")
		CVAR_GUI_VALUE("2", "Tonal impact: E", "")
		CVAR_GUI_VALUE("3", "Tonal impact: F#", "")
		CVAR_GUI_VALUE("4", "Tonal impact: G", "")
		CVAR_GUI_VALUE("5", "Cork pop", "")
		CVAR_GUI_VALUE("6", "Cash register", "")
		CVAR_GUI_VALUE("7", "Hook impact", "")
	},
	{
		"cg_newModels", CVART_BOOL,
		"Better 3D models", CVARCAT_GRAPHICS, "Uses better meshes for items", ""
	},
	{
		"cg_animateChatBalloon", CVART_BOOL,
		"Animated chat sprite", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_demoMapOverrides", CVART_BOOL,
		"Override demo maps", CVARCAT_GRAPHICS, "Replaces maps for demo playback",
		"The override list is loaded from 'cfg-maps/demomaps.txt'.\n"
		"Each line has the format 'DemoName DisplayName'.\n"
		"Example: 'cpm3a cpm3b_b1' will display cpm3b_b1 instead of cpm3a."
	},
	{
		"ch_crosshairAlpha", CVART_FLOAT,
		"Crosshair opacity", CVARCAT_HUD, "", ""
	},
	{
		"ch_crosshairColor", CVART_COLOR_CPMA,
		"Crosshair color", CVARCAT_HUD, "", ""
	},
	{
		"ch_crosshairPulse", CVART_INTEGER,
		"Crosshair pulses", CVARCAT_HUD, "Crosshair pulses on events", "",
		CVAR_GUI_VALUE("0", "Never", "")
		CVAR_GUI_VALUE("1", "Item pick-ups", "")
		CVAR_GUI_VALUE("2", "Enemy damage", "")
		CVAR_GUI_VALUE("3", "Enemy frags", "")
	},
	{
		"ch_crosshairText", CVART_STRING,
		"Crosshair text", CVARCAT_HUD, "Crosshair uses this text instead of an icon", ""
	},
	{
		"ch_crosshairHitColor", CVART_COLOR_CPMA_E,
		"Crosshair hit color", CVARCAT_HUD, "", ""
	},
	{
		"ch_crosshairFragColor", CVART_COLOR_CPMA_E,
		"Crosshair kill color", CVARCAT_HUD, "", ""
	},
	{
		"ch_drawWarmup", CVART_BOOL,
		"Warm-up info", CVARCAT_HUD, "Displays gameplay settings during warm-up", ""
	},
	{
		"ch_file", CVART_STRING,
		"HUD config file", CVARCAT_HUD, "", ""
	},
	{
		"ch_playerNames", CVART_BOOL,
		"Draw player names", CVARCAT_DEMO, "Draws names above heads during demo playback", ""
	},
	{
		"ch_recordMessage", CVART_BOOL,
		"Draw demo recording", CVARCAT_HUD, "Draws a message when recording demos", ""
	},
	{
		"ch_selfOnTeamOverlay", CVART_BOOL,
		"Team overlay self row", CVARCAT_HUD, "Draw a row for yourself in the team overlays", ""
	},
	{
		"ch_shortNames", CVART_BOOL,
		"Team overlay nicknames", CVARCAT_HUD, "Team overlays uses nicknames (max. 5 characters) instead of names", ""
	},
	{
		"ch_locations", CVART_BOOL,
		"Team overlay locations", CVARCAT_HUD, "Draws locations in the team overlays", ""
	},
	{
		"ch_shortScoreboard", CVART_BOOL,
		"Short scoreboard", CVARCAT_HUD, "Disables stats display in 1v1 scoreboard", ""
	},
	{
		"ch_3waveFont", CVART_BOOL,
		"Threewave font", CVARCAT_HUD, "Uses the Threewave font in the scoreboard", ""
	},
	{
		"ch_wstatsTime", CVART_INTEGER,
		"Weapon stats time", CVARCAT_HUD, "Max. +wstats display duration in intermission", ""
	},
	{
		"ch_gauntlet", CVART_BOOL,
		"Gauntlet in weapon list", CVARCAT_HUD, "Draws the gauntlet in the WeaponList SuperHUD element", ""
	},
	{
		"ch_drawKeys", CVART_BITMASK,
		"Draw movement keys", CVARCAT_HUD, "Enables the KeyDown_* and KeyUp_* SuperHUD elements", "",
		CVAR_GUI_VALUE("0", "When playing", "")
		CVAR_GUI_VALUE("1", "When spectating", "")
		CVAR_GUI_VALUE("2", "During demo playback", "Won't work with pre-1.50 demos")
	},
	{
		"ch_hiddenElements", CVART_STRING,
		"Hidden SuperHUD elements", CVARCAT_HUD, "List of hidden SuperHUD elements", "Use /hud_hide and /hud_show to manipulate this list"
	},
	{
		"ch_timersList", CVART_STRING,
		"Item timers draw list", CVARCAT_HUD, "Space-separated list of item types to draw", "Use /itemtimerlist to see the type list"
	},
	{
		"ch_timersOrderOR", CVART_INTEGER,
		"Own/red item timers order", CVARCAT_HUD, "", "",
		CVAR_GUI_VALUE("0", "Flag -> Mid", "")
		CVAR_GUI_VALUE("1", "Mid  -> Flag", "")
		CVAR_GUI_VALUE("2", "Custom list", "")
		CVAR_GUI_VALUE("3", "Custom list, inverted", "")
	},
	{
		"ch_timersOrderEB", CVART_INTEGER,
		"Enemy/blue item timers order", CVARCAT_HUD, "", "",
		CVAR_GUI_VALUE("0", "Flag -> Mid", "")
		CVAR_GUI_VALUE("1", "Mid  -> Flag", "")
		CVAR_GUI_VALUE("2", "Custom list", "")
		CVAR_GUI_VALUE("3", "Custom list, inverted", "")
	},
	{
		"ch_timersOrderTeams", CVART_INTEGER,
		"Item timers team order", CVARCAT_HUD, "", "",
		CVAR_GUI_VALUE("0", "Own   -> Enemy", "")
		CVAR_GUI_VALUE("1", "Enemy -> Own", "")
		CVAR_GUI_VALUE("2", "Red   -> Blue", "")
		CVAR_GUI_VALUE("3", "Blue  -> Red", "")
	},
	{
		"ch_timersSync", CVART_BOOL,
		"Synchronize item timers", CVARCAT_HUD, "Synchronizes all timer updates in the HUD", ""
	},
	{
		"cg_demoSkipPauses", CVART_BOOL,
		"Skip server pauses", CVARCAT_DEMO, "Skips timeouts and referee pauses",
		"When not paused, it will skip past server pauses\n"
		"Only available with CNQ3's new demo player"
	},
	{
		"mvw_DM", CVART_STRING,
		"Multi-view 1v1 window", CVARCAT_HUD, "PiP window coordinates for the opponent in 1v1", ""
	},
	{
		"mvw_TDM1", CVART_STRING,
		"Multi-view TDM window #1", CVARCAT_HUD, "PiP window coordinates for teammate #1", ""
	},
	{
		"mvw_TDM2", CVART_STRING,
		"Multi-view TDM window #2", CVARCAT_HUD, "PiP window coordinates for teammate #2", ""
	},
	{
		"mvw_TDM3", CVART_STRING,
		"Multi-view TDM window #3", CVARCAT_HUD, "PiP window coordinates for teammate #3", ""
	},
	{
		"mvw_TDM4", CVART_STRING,
		"Multi-view TDM window #4", CVARCAT_HUD, "PiP window coordinates for teammate #4", ""
	},
	{
		"s_ambient", CVART_BOOL,
		"Ambient sounds", CVARCAT_SOUND, "Enables the maps' ambient sounds", ""
	},
	{
		"cg_drawBrightSpawns", CVART_BOOL,
		"Bright spawn points", CVARCAT_GRAPHICS, "Draws spawn points more visibly during warm-up", ""
	},
	{
		"cg_drawBrightWeapons", CVART_BITMASK,
		"Fullbright weapons", CVARCAT_GRAPHICS, "Enables fullbright weapons", "",
		CVAR_GUI_VALUE("0", "First-person, your own gun", "")
		CVAR_GUI_VALUE("1", "First-person, carried by teammates", "")
		CVAR_GUI_VALUE("2", "First-person, carried by enemies", "")
		CVAR_GUI_VALUE("3", "Weapons lying on the map", "")
		CVAR_GUI_VALUE("4", "Freecam, carried by players", "")
	},
	{
		"s_announcer", CVART_STRING,
		"Announcer", CVARCAT_SOUND, "Announcer sound pack", "",
		CVAR_GUI_VALUE("feedback", "Q3's male announcer", "")
		CVAR_GUI_VALUE("hellchick", "CPMA's female announcer", "")
	},
	{
		"ch_consoleLines", CVART_INTEGER,
		"Max console lines", CVARCAT_HUD, "Maximum number of lines in the 'Console' SuperHUD element", ""
	},
	{
		"ch_eventLines", CVART_INTEGER,
		"Max event lines", CVARCAT_HUD, "Maximum number of lines in the 'GameEvents' SuperHUD element", ""
	},
	{
		"ch_eventForceColors", CVART_BOOL,
		"Force event colors", CVARCAT_HUD, "Enables custom team colors in the 'GameEvents' SuperHUD element", ""
	},
	{
		"ch_eventOwnColor", CVART_COLOR_CPMA,
		"Own team event color", CVARCAT_HUD, "", ""
	},
	{
		"ch_eventEnemyColor", CVART_COLOR_CPMA,
		"Enemy team event color", CVARCAT_HUD, "", ""
	},
	{
		"ch_chatLines", CVART_INTEGER,
		"Max chat lines", CVARCAT_HUD, "Maximum number of lines in the 'Chat' SuperHUD element", ""
	},
	{
		"ch_animateScroll", CVART_BOOL,
		"Scrolling animations", CVARCAT_HUD, "For console, chat, game events and rewards", ""
	},
	{
		"ch_animateRewardIcons", CVART_INTEGER,
		"Animate reward icons", CVARCAT_HUD, "", "",
		CVAR_GUI_VALUE("0", "No animation", "")
		CVAR_GUI_VALUE("1", "Play animation once", "")
		CVAR_GUI_VALUE("2", "Play looped animation", "")
	},
	{
		"ui_swapMouseButtons", CVART_BOOL,
		"Swap mouse buttons", CVARCAT_GUI, "Swaps the left and right mouse buttons", ""
	},
	{
		"ui_sensitivity", CVART_FLOAT,
		"UI mouse sensitivity", CVARCAT_GUI, "Mouse sensitivity in the UI", ""
	},
	{
		"cg_mvSensitivity", CVART_FLOAT,
		"Coach view mouse sensitivity", CVARCAT_GUI, "Mouse sensitivity in the live coach view UI", ""
	},
	{
		"cg_ammoWarning", CVART_BOOL,
		"Ammo warning", CVARCAT_SOUND, "Plays a click sound when out of ammo", ""
	},
	{
		"cg_autoswitch", CVART_BOOL,
		"Weapon auto-switch", CVARCAT_GAMEPLAY, "Auto-switches to the weapon you just picked up", ""
	},
	{
		"com_blood", CVART_INTEGER,
		"Draw blood", CVARCAT_GRAPHICS, "Draws blood when players are hit", "",
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("1", "Old style", "Large, long, slow updates")
		CVAR_GUI_VALUE("2", "New style", "Small, short, fast updates")
	},
	{
		"cg_drawGun", CVART_INTEGER,
		"View weapon", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("0", "No gun", "")
		CVAR_GUI_VALUE("1", "Gun visible, sways", "")
		CVAR_GUI_VALUE("2", "Gun visible, doesn't sway", "")
	},
	{
		"cg_zoomfov", CVART_FLOAT,
		"Zoom FoV", CVARCAT_GRAPHICS | CVARCAT_GAMEPLAY, "Field of view when zoomed in", ""
	},
	{
		"cg_zoomAnimationTime", CVART_INTEGER,
		"Zoom animation time", CVARCAT_GRAPHICS | CVARCAT_GAMEPLAY, "Zoom in/out animation duration, in milliseconds", ""
	},
	{
		"cg_zoomSensitivity", CVART_FLOAT,
		"Zoom mouse sensitivity", CVARCAT_GAMEPLAY, "Relative to the main sensitivity", ""
	},
	{
		"cg_fov", CVART_FLOAT,
		"FoV", CVARCAT_GRAPHICS | CVARCAT_GAMEPLAY, "Field of view", "Enable depth clamping to raise the limit to 150"
	},
	{
		"cg_viewsize", CVART_INTEGER,
		"Gameplay window size", CVARCAT_HUD, "Percentage scale of the gameplay window", ""
	},
	{
		"cg_shadows", CVART_BOOL,
		"Blob player shadows", CVARCAT_GRAPHICS, "", ""
	},
	{
		"cg_gibs", CVART_BOOL,
		"Giblets", CVARCAT_GRAPHICS, "Enables giblets and the blood spatter effect", ""
	},
	{
		"cg_draw2D", CVART_BOOL,
		"Enable HUD", CVARCAT_HUD, "", ""
	},
	{
		"cg_draw3dIcons", CVART_BOOL,
		"3D icons", CVARCAT_HUD, "Draws 3D heads/flags in the HUD", ""
	},
	{
		"cg_drawCrosshair", CVART_INTEGER,
		"Crosshair icon", CVARCAT_HUD, "", "",
		CVAR_GUI_VALUE("0", "No crosshair", "")
		CVAR_GUI_VALUE("1", "Cross", "")
		CVAR_GUI_VALUE("2", "Exploded cross", "")
		CVAR_GUI_VALUE("3", "Disk + dot", "")
		CVAR_GUI_VALUE("4", "Circle + dot", "")
		CVAR_GUI_VALUE("5", "Big dot", "")
		CVAR_GUI_VALUE("6", "Circle + cross", "")
		CVAR_GUI_VALUE("7", "Big exploded cross", "")
		CVAR_GUI_VALUE("8", "Big exploded cross + dot", "")
		CVAR_GUI_VALUE("9", "Side curves + dot", "")
		CVAR_GUI_VALUE("10", "Circle + dot", "")
		CVAR_GUI_VALUE("11", "Cross with black outline", "")
		CVAR_GUI_VALUE("12", "Exploded cross with black outline", "")
		CVAR_GUI_VALUE("13", "Disk + dot with black outline", "")
		CVAR_GUI_VALUE("14", "Circle + dot with black outline", "")
		CVAR_GUI_VALUE("15", "Dot with black outline", "")
		CVAR_GUI_VALUE("16", "Circle + cross with black outline", "")
		CVAR_GUI_VALUE("17", "Mirrored L with black outline", "")
		CVAR_GUI_VALUE("18", "Cross + dot with black outlines", "")
		CVAR_GUI_VALUE("19", "Side curves + dot with black outline", "")
		CVAR_GUI_VALUE("20", "Circle + exploded cross + dot", "")
	},
	{
		"cg_drawCrosshairNames", CVART_INTEGER,
		"Draw crosshair names", CVARCAT_HUD, "Draws the name and status of the player under the crosshair",
		"SuperHUD elements: 'TargetName' and 'TargetStatus'",
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("1", "All players", "")
		CVAR_GUI_VALUE("2", "Teammates only", "")
	},
	{
		"cg_crosshairSize", CVART_STRING,
		"Crosshair scale", CVARCAT_HUD, "Examples: '26', '26x32'", ""
	},
	{
		"cg_crosshairHealth", CVART_BOOL,
		"Crosshair health", CVARCAT_HUD, "Sets crosshair color based on your stack", ""
	},
	{
		"cg_crosshairX", CVART_INTEGER,
		"Crosshair X offset", CVARCAT_HUD, "X-axis offset from screen center", ""
	},
	{
		"cg_crosshairY", CVART_INTEGER,
		"Crosshair Y offset", CVARCAT_HUD, "Y-axis offset from screen center", ""
	},
	{
		"cg_simpleItems", CVART_BOOL,
		"Simple items", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Shows items as sprites", ""
	},
	{
		"cg_brassTime", CVART_INTEGER,
		"Brass lifetime", CVARCAT_GRAPHICS, "MG/SG shell visibility duration, in milliseconds", ""
	},
	{
		"cg_marks", CVART_INTEGER,
		"Explosion marks lifetime", CVARCAT_GRAPHICS, "Explosion marks visibility duration, in milliseconds", ""
	},
	{
		"cg_railTrailTime", CVART_INTEGER,
		"Rail trails lifetime", CVARCAT_GRAPHICS, "Rail trails visibility duration, in milliseconds", ""
	},
	{
		"cg_errordecay", CVART_INTEGER,
		"Prediction error decay", CVARCAT_NETWORK, "Duration for client-side mispredictions smoothing, in milliseconds", ""
	},
	{
		"cg_predict", CVART_INTEGER,
		"Player movement prediction", CVARCAT_NETWORK, "", "",
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("1", "Enabled", "")
		CVAR_GUI_VALUE("2", "Optimized", "Has errors and performance gains aren't significant")
	},
	{
		"cg_noProjectileTrail", CVART_BOOL,
		"Projectile trails", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("1", "None in water", "")
		CVAR_GUI_VALUE("0", "Everywhere", "")
	},
	{
		"cg_noTaunt", CVART_BOOL,
		"Taunt sounds", CVARCAT_SOUND, "", "",
		CVAR_GUI_VALUE("1", "Silence", "")
		CVAR_GUI_VALUE("0", "Taunt sounds", "")
	},
	{
		"cg_forceModel", CVART_BOOL,
		"Force model on teammates", CVARCAT_GRAPHICS, "Draws your teammates with the same model as yours", ""
	},
	{
		"cg_predictItems", CVART_BOOL,
		"Item pick-up prediction", CVARCAT_NETWORK, "Disable if you get false pick-ups", ""
	},
	{
		"cg_deferPlayers", CVART_BOOL,
		"Delay model loads", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Delays player model loads to avoid hitches", ""
	},
	{
		"cg_drawFriend", CVART_BOOL,
		"Friend markers", CVARCAT_HUD, "Draws triangles above teammates' heads", ""
	},
	{
		"cg_teamChatsOnly", CVART_BOOL,
		"Global chat sink", CVARCAT_HUD, "To which SuperHUD element does global chat go?", "Team chat always goes to the 'Chat' SuperHUD element",
		CVAR_GUI_VALUE("0", "Chat", "")
		CVAR_GUI_VALUE("1", "Console", "")
	},
	{
		"cg_altLightning", CVART_STRING,
		"LG beam style", CVARCAT_GRAPHICS, "In order: self, enemies, teammates",
		"0 = Original (pre-TA) id LG beam\n"
		"1 = CPMA lightning\n"
		"2 = Thin lightning\n"
		"3 = Lightning using 1.44 render"
	},
	{
		"cg_altPlasma", CVART_BOOL,
		"CPMA plasma projectile", CVARCAT_GRAPHICS, "Enables the CPMA plasma gun projectile", ""
	},
	{
		"cg_autoAction", CVART_BITMASK,
		"Automated actions", CVARCAT_GENERAL, "", "",
		CVAR_GUI_VALUE("0", "Save stats to a .txt file", "")
		CVAR_GUI_VALUE("1", "Take an end-game screenshot", "")
		CVAR_GUI_VALUE("2", "Record a demo: match start", "Only if present at match start")
		CVAR_GUI_VALUE("3", "Multi-view the game", "")
		CVAR_GUI_VALUE("4", "Only if you're playing", "")
		CVAR_GUI_VALUE("5", "Follow power-up", "")
		CVAR_GUI_VALUE("6", "Follow killer", "")
		CVAR_GUI_VALUE("7", "Record a demo: late join", "When not present at match start")
	},
	{
		"cg_customLoc", CVART_BOOL,
		"Custom map locations", CVARCAT_HUD, "Uses client-local map location names", "The locations are in .cfg files in the 'locs' folder"
	},
	{
		"cg_nochatbeep", CVART_BOOL,
		"Global chat beeps", CVARCAT_SOUND, "", "",
		CVAR_GUI_VALUE("0", "Global chat beeps", "")
		CVAR_GUI_VALUE("1", "Silence", "")
	},
	{
		"cg_nomip", CVART_BITMASK,
		"Picmip overrides", CVARCAT_GRAPHICS, "Enable to keep specific visuals sharp", "",
		CVAR_GUI_VALUE("0", "Lightning beams", "")
		CVAR_GUI_VALUE("1", "Plasma balls", "")
		CVAR_GUI_VALUE("2", "Rocket and grenade explosions", "")
		CVAR_GUI_VALUE("3", "Grenade projectiles", "")
		CVAR_GUI_VALUE("4", "Bullet impact marks", "")
		CVAR_GUI_VALUE("5", "Railgun impact mark", "")
		CVAR_GUI_VALUE("6", "BFG explosion", "")
		CVAR_GUI_VALUE("7", "Blood", "")
		CVAR_GUI_VALUE("8", "Smoke", "")
		CVAR_GUI_VALUE("9", "Rocket projectiles", "")
	},
	{
		"cg_noteamchatbeep", CVART_BOOL,
		"Team chat beeps", CVARCAT_SOUND, "", "",
		CVAR_GUI_VALUE("1", "Silence", "")
		CVAR_GUI_VALUE("0", "Team chat beeps", "")
	},
	{
		"cg_oldCTFSounds", CVART_INTEGER,
		"CTF sounds", CVARCAT_SOUND, "", "",
		CVAR_GUI_VALUE("0", "Team Arena", "With voice-overs")
		CVAR_GUI_VALUE("1", "Quake 3 1.17", "Same for both teams")
		CVAR_GUI_VALUE("2", "Team-specific", "No voice-overs")
	},
	{
		"cg_showPlayerLean", CVART_BOOL,
		"Player model leaning", CVARCAT_GAMEPLAY, "Enables player model leaning", "Off is more accurate wrt collision models"
	},
	{
		"cg_useScreenShotJPEG", CVART_BOOL,
		"Automated screenshot format", CVARCAT_GENERAL, "", "",
		CVAR_GUI_VALUE("0", "TARGA (.tga)", "")
		CVAR_GUI_VALUE("1", "JPEG (.jpg)", "")
	},
	{
		"color", CVART_COLOR_CHBLS,
		"Player colors", CVARCAT_GRAPHICS, "", "",
	},
	{
		"model", CVART_STRING,
		"Player model", CVARCAT_GRAPHICS, "", ""
	},
	{
		"nick",  CVART_STRING,
		"Nickname", CVARCAT_GENERAL, "Short name (max. 5 chars) for team overlays", ""
	}
};


void CL_Init()
{
	//QSUBSYSTEM_INIT_START( "Client" );

	cls.fullClientShutDown = qfalse;

	CL_ConInit();

	CL_ClearState();

	cls.state = CA_DISCONNECTED;	// no longer CA_UNINITIALIZED

	cls.realtime = 0;

	Cvar_RegisterArray( cl_cvars, MODULE_CLIENT );
	Cmd_RegisterArray( cl_cmds, MODULE_CLIENT );

	CL_InitRef();
	CL_InitInput();

	SCR_Init();

	Cbuf_Execute();

	Cvar_Set( "cl_running", "1" );

	CL_MapDownload_Init();

	CL_IMGUI_Init();

	//QSUBSYSTEM_INIT_DONE( "Client" );
}


void CL_Shutdown()
{
	static qbool recursive = qfalse;

	// check whether the client is running at all.
	if(!(com_cl_running && com_cl_running->integer))
		return;

	Com_Printf( "----- CL_Shutdown -----\n" );

	if ( recursive ) {
		printf ("recursive shutdown\n");
		return;
	}
	recursive = qtrue;

	cls.fullClientShutDown = qtrue;

	CL_Disconnect( qtrue );

	S_Shutdown();

	CL_ShutdownInput();
	CL_ShutdownRef();

	CL_ShutdownUI();

	CL_MapDownload_Cancel();

	Cmd_UnregisterModule( MODULE_CLIENT );

	CL_ConShutdown();

	CL_IMGUI_Shutdown();

	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	Com_Memset( &cls, 0, sizeof( cls ) );

	Com_Printf( "-----------------------\n" );
}


void CL_SetMenuData( qboolean typeOnly )
{
	for ( int i = 0; i < ARRAY_LEN( cpma_cvars ); ++i ) {
		const cvarTableItemCPMA_t* const cvar = &cpma_cvars[i];
		Cvar_SetDataType( cvar->name, cvar->type );
		if ( !typeOnly ) {
			Cvar_SetMenuData( cvar->name, cvar->categories, cvar->guiName, cvar->guiDesc, cvar->guiHelp, cvar->guiValues );
		}
	}
}
