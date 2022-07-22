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
// sv_game.c -- interface to the game dll

#include "server.h"

#include "../botlib/botlib.h"

botlib_export_t	*botlib_export;

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}

sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}

playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}

svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );	
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );	
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	if (!name) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if (name[0] != '*') {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}


	ent->s.modelindex = atoi( name + 1 );

	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	SV_LinkEntity( ent );		// FIXME: remove
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS (const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t	*svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_EntityContact
==================
*/
qboolean	SV_EntityContact( vec3_t mins, vec3_t maxs, const sharedEntity_t *gEnt, int capsule ) {
	const float	*origin, *angles;
	clipHandle_t	ch;
	trace_t			trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace ( &trace, vec3_origin, vec3_origin, mins, maxs,
		ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo

===============
*/
void SV_GetServerinfo( char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO ), bufferSize );
}

/*
===============
SV_LocateGameData

===============
*/
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t,
					   playerState_t *clients, int sizeofGameClient ) {
	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd

===============
*/
void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum );
	}
	*cmd = svs.clients[clientNum].lastUsercmd;
}

//==============================================

static int	FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
intptr_t SV_GameSystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case G_PRINT:
        G_LogPrintf( "syscall: G_PRINT(%d)\n", args[0] );
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case G_ERROR:
        G_LogPrintf( "syscall: G_ERROR(%d)\n", args[0] );
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case G_MILLISECONDS:
        G_LogPrintf( "syscall: G_MILLISECONDS(%d)\n", args[0] );
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
        G_LogPrintf( "syscall: G_CVAR_REGISTER(%d)\n", args[0] );
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4] ); 
		return 0;
	case G_CVAR_UPDATE:
        G_LogPrintf( "syscall: G_CVAR_UPDATE(%d)\n", args[0] );
		Cvar_Update( VMA(1) );
		return 0;
	case G_CVAR_SET:
        G_LogPrintf( "syscall: G_CVAR_SET(%d)\n", args[0] );
		Cvar_SetSafe( (const char *)VMA(1), (const char *)VMA(2) );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
        G_LogPrintf( "syscall: G_CVAR_VARIABLE_INTEGER_VALUE(%d)\n", args[0] );
		return Cvar_VariableIntegerValue( (const char *)VMA(1) );
	case G_CVAR_VARIABLE_STRING_BUFFER:
        G_LogPrintf( "syscall: G_CVAR_VARIABLE_STRING_BUFFER(%d)\n", args[0] );
		Cvar_VariableStringBuffer( VMA(1), VMA(2), args[3] );
		return 0;
	case G_ARGC:
        G_LogPrintf( "syscall: G_ARGC(%d)\n", args[0] );
		return Cmd_Argc();
	case G_ARGV:
        G_LogPrintf( "syscall: G_ARGV(%d)\n", args[0] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
        G_LogPrintf( "syscall: G_SEND_CONSOLE_COMMAND(%d)\n", args[0] );
		Cbuf_ExecuteText( args[1], VMA(2) );
		return 0;

	case G_FS_FOPEN_FILE:
        G_LogPrintf( "syscall: G_FS_FOPEN_FILE(%d)\n", args[0] );
		return FS_FOpenFileByMode( VMA(1), VMA(2), args[3] );
	case G_FS_READ:
        G_LogPrintf( "syscall: G_FS_READ(%d)\n", args[0] );
		FS_Read( VMA(1), args[2], args[3] );
		return 0;
	case G_FS_WRITE:
        G_LogPrintf( "syscall: G_FS_WRITE(%d)\n", args[0] );
		FS_Write( VMA(1), args[2], args[3] );
		return 0;
	case G_FS_FCLOSE_FILE:
        G_LogPrintf( "syscall: G_FS_FCLOSE_FILE(%d)\n", args[0] );
		FS_FCloseFile( args[1] );
		return 0;
	case G_FS_GETFILELIST:
        G_LogPrintf( "syscall: G_FS_GETFILELIST(%d)\n", args[0] );
		return FS_GetFileList( VMA(1), VMA(2), VMA(3), args[4] );
	case G_FS_SEEK:
        G_LogPrintf( "syscall: G_FS_SEEK(%d)\n", args[0] );
		return FS_Seek( args[1], args[2], args[3] );

	case G_LOCATE_GAME_DATA:
        G_LogPrintf( "syscall: G_LOCATE_GAME_DATA(%d)\n", args[0] );
		SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );
		return 0;
	case G_DROP_CLIENT:
        G_LogPrintf( "syscall: G_DROP_CLIENT(%d)\n", args[0] );
		SV_GameDropClient( args[1], VMA(2) );
		return 0;
	case G_SEND_SERVER_COMMAND:
        G_LogPrintf( "syscall: G_SEND_SERVER_COMMAND(%d)\n", args[0] );
		SV_GameSendServerCommand( args[1], VMA(2) );
		return 0;
	case G_LINKENTITY:
        G_LogPrintf( "syscall: G_LINKENTITY(%d)\n", args[0] );
		SV_LinkEntity( VMA(1) );
		return 0;
	case G_UNLINKENTITY:
        G_LogPrintf( "syscall: G_UNLINKENTITY(%d)\n", args[0] );
		SV_UnlinkEntity( VMA(1) );
		return 0;
	case G_ENTITIES_IN_BOX:
        G_LogPrintf( "syscall: G_ENTITIES_IN_BOX(%d)\n", args[0] );
		return SV_AreaEntities( VMA(1), VMA(2), VMA(3), args[4] );
	case G_ENTITY_CONTACT:
        G_LogPrintf( "syscall: G_ENTITY_CONTACT(%d)\n", args[0] );
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qfalse );
	case G_ENTITY_CONTACTCAPSULE:
        G_LogPrintf( "syscall: G_ENTITY_CONTACTCAPSULE(%d)\n", args[0] );
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qtrue );
	case G_TRACE:
        G_LogPrintf( "syscall: G_TRACE(%d)\n", args[0] );
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		return 0;
	case G_TRACECAPSULE:
        G_LogPrintf( "syscall: G_TRACECAPSULE(%d)\n", args[0] );
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case G_POINT_CONTENTS:
        G_LogPrintf( "syscall: G_POINT_CONTENTS(%d)\n", args[0] );
		return SV_PointContents( VMA(1), args[2] );
	case G_SET_BRUSH_MODEL:
        G_LogPrintf( "syscall: G_SET_BRUSH_MODEL(%d)\n", args[0] );
		SV_SetBrushModel( VMA(1), VMA(2) );
		return 0;
	case G_IN_PVS:
        G_LogPrintf( "syscall: G_IN_PVS(%d)\n", args[0] );
		return SV_inPVS( VMA(1), VMA(2) );
	case G_IN_PVS_IGNORE_PORTALS:
        G_LogPrintf( "syscall: G_IN_PVS_IGNORE_PORTALS(%d)\n", args[0] );
		return SV_inPVSIgnorePortals( VMA(1), VMA(2) );

	case G_SET_CONFIGSTRING:
        G_LogPrintf( "syscall: G_SET_CONFIGSTRING(%d)\n", args[0] );
		SV_SetConfigstring( args[1], VMA(2) );
		return 0;
	case G_GET_CONFIGSTRING:
        G_LogPrintf( "syscall: G_GET_CONFIGSTRING(%d)\n", args[0] );
		SV_GetConfigstring( args[1], VMA(2), args[3] );
		return 0;
	case G_SET_USERINFO:
        G_LogPrintf( "syscall: G_SET_USERINFO(%d)\n", args[0] );
		SV_SetUserinfo( args[1], VMA(2) );
		return 0;
	case G_GET_USERINFO:
        G_LogPrintf( "syscall: G_GET_USERINFO(%d)\n", args[0] );
		SV_GetUserinfo( args[1], VMA(2), args[3] );
		return 0;
	case G_GET_SERVERINFO:
        G_LogPrintf( "syscall: G_GET_SERVERINFO(%d)\n", args[0] );
		SV_GetServerinfo( VMA(1), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
        G_LogPrintf( "syscall: G_ADJUST_AREA_PORTAL_STATE(%d)\n", args[0] );
		SV_AdjustAreaPortalState( VMA(1), args[2] );
		return 0;
	case G_AREAS_CONNECTED:
        G_LogPrintf( "syscall: G_AREAS_CONNECTED(%d)\n", args[0] );
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
        G_LogPrintf( "syscall: G_BOT_ALLOCATE_CLIENT(%d)\n", args[0] );
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
        G_LogPrintf( "syscall: G_BOT_FREE_CLIENT(%d)\n", args[0] );
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
        G_LogPrintf( "syscall: G_GET_USERCMD(%d)\n", args[0] );
		SV_GetUsercmd( args[1], VMA(2) );
		return 0;
	case G_GET_ENTITY_TOKEN:
        G_LogPrintf( "syscall: G_GET_ENTITY_TOKEN(%d)\n", args[0] );
		{
			const char	*s;

			s = COM_Parse( &sv.entityParsePoint );
			Q_strncpyz( VMA(1), s, args[2] );
			if ( !sv.entityParsePoint && !s[0] ) {
				return qfalse;
			} else {
				return qtrue;
			}
		}

	case G_DEBUG_POLYGON_CREATE:
        G_LogPrintf( "syscall: G_DEBUG_POLYGON_CREATE(%d)\n", args[0] );
		return BotImport_DebugPolygonCreate( args[1], args[2], VMA(3) );
	case G_DEBUG_POLYGON_DELETE:
        G_LogPrintf( "syscall: G_DEBUG_POLYGON_DELETE(%d)\n", args[0] );
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
        G_LogPrintf( "syscall: G_REAL_TIME(%d)\n", args[0] );
		return Com_RealTime( VMA(1) );
	case G_SNAPVECTOR:
        G_LogPrintf( "syscall: G_SNAPVECTOR(%d)\n", args[0] );
		Q_SnapVector(VMA(1));
		return 0;

		//====================================

	case BOTLIB_SETUP:
        G_LogPrintf( "syscall: BOTLIB_SETUP(%d)\n", args[0] );
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
        G_LogPrintf( "syscall: BOTLIB_SHUTDOWN(%d)\n", args[0] );
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
        G_LogPrintf( "syscall: BOTLIB_LIBVAR_SET(%d)\n", args[0] );
		return botlib_export->BotLibVarSet( VMA(1), VMA(2) );
	case BOTLIB_LIBVAR_GET:
        G_LogPrintf( "syscall: BOTLIB_LIBVAR_GET(%d)\n", args[0] );
		return botlib_export->BotLibVarGet( VMA(1), VMA(2), args[3] );

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
        G_LogPrintf( "syscall: BOTLIB_PC_ADD_GLOBAL_DEFINE(%d)\n", args[0] );
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case BOTLIB_PC_LOAD_SOURCE:
        G_LogPrintf( "syscall: BOTLIB_PC_LOAD_SOURCE(%d)\n", args[0] );
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case BOTLIB_PC_FREE_SOURCE:
        G_LogPrintf( "syscall: BOTLIB_PC_FREE_SOURCE(%d)\n", args[0] );
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
        G_LogPrintf( "syscall: BOTLIB_PC_READ_TOKEN(%d)\n", args[0] );
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
        G_LogPrintf( "syscall: BOTLIB_PC_SOURCE_FILE_AND_LINE(%d)\n", args[0] );
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case BOTLIB_START_FRAME:
        G_LogPrintf( "syscall: BOTLIB_START_FRAME(%d)\n", args[0] );
		return botlib_export->BotLibStartFrame( VMF(1) );
	case BOTLIB_LOAD_MAP:
        G_LogPrintf( "syscall: BOTLIB_LOAD_MAP(%d)\n", args[0] );
		return botlib_export->BotLibLoadMap( VMA(1) );
	case BOTLIB_UPDATENTITY:
        G_LogPrintf( "syscall: BOTLIB_UPDATENTITY(%d)\n", args[0] );
		return botlib_export->BotLibUpdateEntity( args[1], VMA(2) );
	case BOTLIB_TEST:
        G_LogPrintf( "syscall: BOTLIB_TEST(%d)\n", args[0] );
		return botlib_export->Test( args[1], VMA(2), VMA(3), VMA(4) );

	case BOTLIB_GET_SNAPSHOT_ENTITY:
        G_LogPrintf( "syscall: BOTLIB_GET_SNAPSHOT_ENTITY(%d)\n", args[0] );
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_GET_CONSOLE_MESSAGE(%d)\n", args[0] );
		return SV_BotGetConsoleMessage( args[1], VMA(2), args[3] );
	case BOTLIB_USER_COMMAND:
        G_LogPrintf( "syscall: BOTLIB_USER_COMMAND(%d)\n", args[0] );
		{
			int clientNum = args[1];

			if ( clientNum >= 0 && clientNum < sv_maxclients->integer ) {
				SV_ClientThink( &svs.clients[clientNum], VMA(2) );
			}
		}
		return 0;

	case BOTLIB_AAS_BBOX_AREAS:
        G_LogPrintf( "syscall: BOTLIB_AAS_BBOX_AREAS(%d)\n", args[0] );
		return botlib_export->aas.AAS_BBoxAreas( VMA(1), VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_AREA_INFO:
        G_LogPrintf( "syscall: BOTLIB_AAS_AREA_INFO(%d)\n", args[0] );
		return botlib_export->aas.AAS_AreaInfo( args[1], VMA(2) );
	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL(%d)\n", args[0] );
		return botlib_export->aas.AAS_AlternativeRouteGoals( VMA(1), args[2], VMA(3), args[4], args[5], VMA(6), args[7], args[8] );
	case BOTLIB_AAS_ENTITY_INFO:
        G_LogPrintf( "syscall: BOTLIB_AAS_ENTITY_INFO(%d)\n", args[0] );
		botlib_export->aas.AAS_EntityInfo( args[1], VMA(2) );
		return 0;

	case BOTLIB_AAS_INITIALIZED:
        G_LogPrintf( "syscall: BOTLIB_AAS_INITIALIZED(%d)\n", args[0] );
		return botlib_export->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
        G_LogPrintf( "syscall: BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX(%d)\n", args[0] );
		botlib_export->aas.AAS_PresenceTypeBoundingBox( args[1], VMA(2), VMA(3) );
		return 0;
	case BOTLIB_AAS_TIME:
        G_LogPrintf( "syscall: BOTLIB_AAS_TIME(%d)\n", args[0] );
		return FloatAsInt( botlib_export->aas.AAS_Time() );

	case BOTLIB_AAS_POINT_AREA_NUM:
        G_LogPrintf( "syscall: BOTLIB_AAS_POINT_AREA_NUM(%d)\n", args[0] );
		return botlib_export->aas.AAS_PointAreaNum( VMA(1) );
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
        G_LogPrintf( "syscall: BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX(%d)\n", args[0] );
		return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMA(1) );
	case BOTLIB_AAS_TRACE_AREAS:
        G_LogPrintf( "syscall: BOTLIB_AAS_TRACE_AREAS(%d)\n", args[0] );
		return botlib_export->aas.AAS_TraceAreas( VMA(1), VMA(2), VMA(3), VMA(4), args[5] );

	case BOTLIB_AAS_POINT_CONTENTS:
        G_LogPrintf( "syscall: BOTLIB_AAS_POINT_CONTENTS(%d)\n", args[0] );
		return botlib_export->aas.AAS_PointContents( VMA(1) );
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
        G_LogPrintf( "syscall: BOTLIB_AAS_NEXT_BSP_ENTITY(%d)\n", args[0] );
		return botlib_export->aas.AAS_NextBSPEntity( args[1] );
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
        G_LogPrintf( "syscall: BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY(%d)\n", args[0] );
		return botlib_export->aas.AAS_ValueForBSPEpairKey( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
        G_LogPrintf( "syscall: BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY(%d)\n", args[0] );
		return botlib_export->aas.AAS_VectorForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
        G_LogPrintf( "syscall: BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY(%d)\n", args[0] );
		return botlib_export->aas.AAS_FloatForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
        G_LogPrintf( "syscall: BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY(%d)\n", args[0] );
		return botlib_export->aas.AAS_IntForBSPEpairKey( args[1], VMA(2), VMA(3) );

	case BOTLIB_AAS_AREA_REACHABILITY:
        G_LogPrintf( "syscall: BOTLIB_AAS_AREA_REACHABILITY(%d)\n", args[0] );
		return botlib_export->aas.AAS_AreaReachability( args[1] );

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
        G_LogPrintf( "syscall: BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA(%d)\n", args[0] );
		return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( args[1], VMA(2), args[3], args[4] );
	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
        G_LogPrintf( "syscall: BOTLIB_AAS_ENABLE_ROUTING_AREA(%d)\n", args[0] );
		return botlib_export->aas.AAS_EnableRoutingArea( args[1], args[2] );
	case BOTLIB_AAS_PREDICT_ROUTE:
        G_LogPrintf( "syscall: BOTLIB_AAS_PREDICT_ROUTE(%d)\n", args[0] );
		return botlib_export->aas.AAS_PredictRoute( VMA(1), args[2], VMA(3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11] );

	case BOTLIB_AAS_SWIMMING:
        G_LogPrintf( "syscall: BOTLIB_AAS_SWIMMING(%d)\n", args[0] );
		return botlib_export->aas.AAS_Swimming( VMA(1) );
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
        G_LogPrintf( "syscall: BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT(%d)\n", args[0] );
		return botlib_export->aas.AAS_PredictClientMovement( VMA(1), args[2], VMA(3), args[4], args[5],
			VMA(6), VMA(7), args[8], args[9], VMF(10), args[11], args[12], args[13] );

	case BOTLIB_EA_SAY:
        G_LogPrintf( "syscall: BOTLIB_EA_SAY(%d)\n", args[0] );
		botlib_export->ea.EA_Say( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_SAY_TEAM:
        G_LogPrintf( "syscall: BOTLIB_EA_SAY_TEAM(%d)\n", args[0] );
		botlib_export->ea.EA_SayTeam( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_COMMAND:
        G_LogPrintf( "syscall: BOTLIB_EA_COMMAND(%d)\n", args[0] );
		botlib_export->ea.EA_Command( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_ACTION:
        G_LogPrintf( "syscall: BOTLIB_EA_ACTION(%d)\n", args[0] );
		botlib_export->ea.EA_Action( args[1], args[2] );
		return 0;
	case BOTLIB_EA_GESTURE:
        G_LogPrintf( "syscall: BOTLIB_EA_GESTURE(%d)\n", args[0] );
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
        G_LogPrintf( "syscall: BOTLIB_EA_TALK(%d)\n", args[0] );
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
        G_LogPrintf( "syscall: BOTLIB_EA_ATTACK(%d)\n", args[0] );
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_USE:
        G_LogPrintf( "syscall: BOTLIB_EA_USE(%d)\n", args[0] );
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
        G_LogPrintf( "syscall: BOTLIB_EA_RESPAWN(%d)\n", args[0] );
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
        G_LogPrintf( "syscall: BOTLIB_EA_CROUCH(%d)\n", args[0] );
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_UP(%d)\n", args[0] );
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_DOWN(%d)\n", args[0] );
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_FORWARD(%d)\n", args[0] );
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_BACK(%d)\n", args[0] );
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_LEFT(%d)\n", args[0] );
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE_RIGHT(%d)\n", args[0] );
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
        G_LogPrintf( "syscall: BOTLIB_EA_SELECT_WEAPON(%d)\n", args[0] );
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
        G_LogPrintf( "syscall: BOTLIB_EA_JUMP(%d)\n", args[0] );
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
        G_LogPrintf( "syscall: BOTLIB_EA_DELAYED_JUMP(%d)\n", args[0] );
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE:
        G_LogPrintf( "syscall: BOTLIB_EA_MOVE(%d)\n", args[0] );
		botlib_export->ea.EA_Move( args[1], VMA(2), VMF(3) );
		return 0;
	case BOTLIB_EA_VIEW:
        G_LogPrintf( "syscall: BOTLIB_EA_VIEW(%d)\n", args[0] );
		botlib_export->ea.EA_View( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_END_REGULAR:
        G_LogPrintf( "syscall: BOTLIB_EA_END_REGULAR(%d)\n", args[0] );
		botlib_export->ea.EA_EndRegular( args[1], VMF(2) );
		return 0;
	case BOTLIB_EA_GET_INPUT:
        G_LogPrintf( "syscall: BOTLIB_EA_GET_INPUT(%d)\n", args[0] );
		botlib_export->ea.EA_GetInput( args[1], VMF(2), VMA(3) );
		return 0;
	case BOTLIB_EA_RESET_INPUT:
        G_LogPrintf( "syscall: BOTLIB_EA_RESET_INPUT(%d)\n", args[0] );
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER:
        G_LogPrintf( "syscall: BOTLIB_AI_LOAD_CHARACTER(%d)\n", args[0] );
		return botlib_export->ai.BotLoadCharacter( VMA(1), VMF(2) );
	case BOTLIB_AI_FREE_CHARACTER:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_CHARACTER(%d)\n", args[0] );
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
        G_LogPrintf( "syscall: BOTLIB_AI_CHARACTERISTIC_FLOAT(%d)\n", args[0] );
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
        G_LogPrintf( "syscall: BOTLIB_AI_CHARACTERISTIC_BFLOAT(%d)\n", args[0] );
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
        G_LogPrintf( "syscall: BOTLIB_AI_CHARACTERISTIC_INTEGER(%d)\n", args[0] );
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
        G_LogPrintf( "syscall: BOTLIB_AI_CHARACTERISTIC_BINTEGER(%d)\n", args[0] );
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING:
        G_LogPrintf( "syscall: BOTLIB_AI_CHARACTERISTIC_STRING(%d)\n", args[0] );
		botlib_export->ai.Characteristic_String( args[1], args[2], VMA(3), args[4] );
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_ALLOC_CHAT_STATE(%d)\n", args[0] );
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_CHAT_STATE(%d)\n", args[0] );
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_AI_QUEUE_CONSOLE_MESSAGE(%d)\n", args[0] );
		botlib_export->ai.BotQueueConsoleMessage( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_AI_REMOVE_CONSOLE_MESSAGE(%d)\n", args[0] );
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_AI_NEXT_CONSOLE_MESSAGE(%d)\n", args[0] );
		return botlib_export->ai.BotNextConsoleMessage( args[1], VMA(2) );
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_AI_NUM_CONSOLE_MESSAGE(%d)\n", args[0] );
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT:
        G_LogPrintf( "syscall: BOTLIB_AI_INITIAL_CHAT(%d)\n", args[0] );
		botlib_export->ai.BotInitialChat( args[1], VMA(2), args[3], VMA(4), VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11) );
		return 0;
	case BOTLIB_AI_NUM_INITIAL_CHATS:
        G_LogPrintf( "syscall: BOTLIB_AI_NUM_INITIAL_CHATS(%d)\n", args[0] );
		return botlib_export->ai.BotNumInitialChats( args[1], VMA(2) );
	case BOTLIB_AI_REPLY_CHAT:
        G_LogPrintf( "syscall: BOTLIB_AI_REPLY_CHAT(%d)\n", args[0] );
		return botlib_export->ai.BotReplyChat( args[1], VMA(2), args[3], args[4], VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11), VMA(12) );
	case BOTLIB_AI_CHAT_LENGTH:
        G_LogPrintf( "syscall: BOTLIB_AI_CHAT_LENGTH(%d)\n", args[0] );
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
        G_LogPrintf( "syscall: BOTLIB_AI_ENTER_CHAT(%d)\n", args[0] );
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_CHAT_MESSAGE(%d)\n", args[0] );
		botlib_export->ai.BotGetChatMessage( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
        G_LogPrintf( "syscall: BOTLIB_AI_STRING_CONTAINS(%d)\n", args[0] );
		return botlib_export->ai.StringContains( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_FIND_MATCH:
        G_LogPrintf( "syscall: BOTLIB_AI_FIND_MATCH(%d)\n", args[0] );
		return botlib_export->ai.BotFindMatch( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_MATCH_VARIABLE:
        G_LogPrintf( "syscall: BOTLIB_AI_MATCH_VARIABLE(%d)\n", args[0] );
		botlib_export->ai.BotMatchVariable( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
        G_LogPrintf( "syscall: BOTLIB_AI_UNIFY_WHITE_SPACES(%d)\n", args[0] );
		botlib_export->ai.UnifyWhiteSpaces( VMA(1) );
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
        G_LogPrintf( "syscall: BOTLIB_AI_REPLACE_SYNONYMS(%d)\n", args[0] );
		botlib_export->ai.BotReplaceSynonyms( VMA(1), args[2] );
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
        G_LogPrintf( "syscall: BOTLIB_AI_LOAD_CHAT_FILE(%d)\n", args[0] );
		return botlib_export->ai.BotLoadChatFile( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_SET_CHAT_GENDER:
        G_LogPrintf( "syscall: BOTLIB_AI_SET_CHAT_GENDER(%d)\n", args[0] );
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
        G_LogPrintf( "syscall: BOTLIB_AI_SET_CHAT_NAME(%d)\n", args[0] );
		botlib_export->ai.BotSetChatName( args[1], VMA(2), args[3] );
		return 0;

	case BOTLIB_AI_RESET_GOAL_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_GOAL_STATE(%d)\n", args[0] );
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_AVOID_GOALS(%d)\n", args[0] );
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
        G_LogPrintf( "syscall: BOTLIB_AI_REMOVE_FROM_AVOID_GOALS(%d)\n", args[0] );
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_PUSH_GOAL(%d)\n", args[0] );
		botlib_export->ai.BotPushGoal( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_POP_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_POP_GOAL(%d)\n", args[0] );
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
        G_LogPrintf( "syscall: BOTLIB_AI_EMPTY_GOAL_STACK(%d)\n", args[0] );
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
        G_LogPrintf( "syscall: BOTLIB_AI_DUMP_AVOID_GOALS(%d)\n", args[0] );
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
        G_LogPrintf( "syscall: BOTLIB_AI_DUMP_GOAL_STACK(%d)\n", args[0] );
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME:
        G_LogPrintf( "syscall: BOTLIB_AI_GOAL_NAME(%d)\n", args[0] );
		botlib_export->ai.BotGoalName( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_GET_TOP_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_TOP_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotGetTopGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_SECOND_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_SECOND_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotGetSecondGoal( args[1], VMA(2) );
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
        G_LogPrintf( "syscall: BOTLIB_AI_CHOOSE_LTG_ITEM(%d)\n", args[0] );
		return botlib_export->ai.BotChooseLTGItem( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
        G_LogPrintf( "syscall: BOTLIB_AI_CHOOSE_NBG_ITEM(%d)\n", args[0] );
		return botlib_export->ai.BotChooseNBGItem( args[1], VMA(2), VMA(3), args[4], VMA(5), VMF(6) );
	case BOTLIB_AI_TOUCHING_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_TOUCHING_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotTouchingGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
        G_LogPrintf( "syscall: BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE(%d)\n", args[0] );
		return botlib_export->ai.BotItemGoalInVisButNotVisible( args[1], VMA(2), VMA(3), VMA(4) );
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_LEVEL_ITEM_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotGetLevelItemGoal( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotGetNextCampSpotGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_MAP_LOCATION_GOAL(%d)\n", args[0] );
		return botlib_export->ai.BotGetMapLocationGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_AVOID_GOAL_TIME:
        G_LogPrintf( "syscall: BOTLIB_AI_AVOID_GOAL_TIME(%d)\n", args[0] );
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
        G_LogPrintf( "syscall: BOTLIB_AI_SET_AVOID_GOAL_TIME(%d)\n", args[0] );
		botlib_export->ai.BotSetAvoidGoalTime( args[1], args[2], VMF(3));
		return 0;
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
        G_LogPrintf( "syscall: BOTLIB_AI_INIT_LEVEL_ITEMS(%d)\n", args[0] );
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
        G_LogPrintf( "syscall: BOTLIB_AI_UPDATE_ENTITY_ITEMS(%d)\n", args[0] );
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
        G_LogPrintf( "syscall: BOTLIB_AI_LOAD_ITEM_WEIGHTS(%d)\n", args[0] );
		return botlib_export->ai.BotLoadItemWeights( args[1], VMA(2) );
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_ITEM_WEIGHTS(%d)\n", args[0] );
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
        G_LogPrintf( "syscall: BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC(%d)\n", args[0] );
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
        G_LogPrintf( "syscall: BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC(%d)\n", args[0] );
		botlib_export->ai.BotSaveGoalFuzzyLogic( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
        G_LogPrintf( "syscall: BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC(%d)\n", args[0] );
		botlib_export->ai.BotMutateGoalFuzzyLogic( args[1], VMF(2) );
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_ALLOC_GOAL_STATE(%d)\n", args[0] );
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_GOAL_STATE(%d)\n", args[0] );
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_MOVE_STATE(%d)\n", args[0] );
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT:
        G_LogPrintf( "syscall: BOTLIB_AI_ADD_AVOID_SPOT(%d)\n", args[0] );
		botlib_export->ai.BotAddAvoidSpot( args[1], VMA(2), VMF(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
        G_LogPrintf( "syscall: BOTLIB_AI_MOVE_TO_GOAL(%d)\n", args[0] );
		botlib_export->ai.BotMoveToGoal( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
        G_LogPrintf( "syscall: BOTLIB_AI_MOVE_IN_DIRECTION(%d)\n", args[0] );
		return botlib_export->ai.BotMoveInDirection( args[1], VMA(2), VMF(3), args[4] );
	case BOTLIB_AI_RESET_AVOID_REACH:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_AVOID_REACH(%d)\n", args[0] );
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_LAST_AVOID_REACH(%d)\n", args[0] );
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
        G_LogPrintf( "syscall: BOTLIB_AI_REACHABILITY_AREA(%d)\n", args[0] );
		return botlib_export->ai.BotReachabilityArea( VMA(1), args[2] );
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
        G_LogPrintf( "syscall: BOTLIB_AI_MOVEMENT_VIEW_TARGET(%d)\n", args[0] );
		return botlib_export->ai.BotMovementViewTarget( args[1], VMA(2), args[3], VMF(4), VMA(5) );
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
        G_LogPrintf( "syscall: BOTLIB_AI_PREDICT_VISIBLE_POSITION(%d)\n", args[0] );
		return botlib_export->ai.BotPredictVisiblePosition( VMA(1), args[2], VMA(3), args[4], VMA(5) );
	case BOTLIB_AI_ALLOC_MOVE_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_ALLOC_MOVE_STATE(%d)\n", args[0] );
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_MOVE_STATE(%d)\n", args[0] );
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_INIT_MOVE_STATE(%d)\n", args[0] );
		botlib_export->ai.BotInitMoveState( args[1], VMA(2) );
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
        G_LogPrintf( "syscall: BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON(%d)\n", args[0] );
		return botlib_export->ai.BotChooseBestFightWeapon( args[1], VMA(2) );
	case BOTLIB_AI_GET_WEAPON_INFO:
        G_LogPrintf( "syscall: BOTLIB_AI_GET_WEAPON_INFO(%d)\n", args[0] );
		botlib_export->ai.BotGetWeaponInfo( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
        G_LogPrintf( "syscall: BOTLIB_AI_LOAD_WEAPON_WEIGHTS(%d)\n", args[0] );
		return botlib_export->ai.BotLoadWeaponWeights( args[1], VMA(2) );
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_ALLOC_WEAPON_STATE(%d)\n", args[0] );
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_FREE_WEAPON_STATE(%d)\n", args[0] );
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
        G_LogPrintf( "syscall: BOTLIB_AI_RESET_WEAPON_STATE(%d)\n", args[0] );
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
        G_LogPrintf( "syscall: BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION(%d)\n", args[0] );
		return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMA(2), VMA(3), VMA(4), VMA(5));

	case TRAP_MEMSET:
        G_LogPrintF( "syscall: TRAP_MEMSET(%d)\n", args[0] );
		Com_Memset( VMA(1), args[2], args[3] );
		return 0;

	case TRAP_MEMCPY:
        G_LogPrintF( "syscall: TRAP_MEMCPY(%d)\n", args[0] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return 0;

	case TRAP_STRNCPY:
        G_LogPrintF( "syscall: TRAP_STRNCPY(%d)\n", args[0] );
		strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_SIN:
        G_LogPrintF( "syscall: TRAP_SIN(%d)\n", args[0] );
		return FloatAsInt( sin( VMF(1) ) );

	case TRAP_COS:
        G_LogPrintF( "syscall: TRAP_COS(%d)\n", args[0] );
		return FloatAsInt( cos( VMF(1) ) );

	case TRAP_ATAN2:
        G_LogPrintF( "syscall: TRAP_ATAN2(%d)\n", args[0] );
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
        G_LogPrintF( "syscall: TRAP_SQRT(%d)\n", args[0] );
		return FloatAsInt( sqrt( VMF(1) ) );

	case TRAP_MATRIXMULTIPLY:
        G_LogPrintF( "syscall: TRAP_MATRIXMULTIPLY(%d)\n", args[0] );
		MatrixMultiply( VMA(1), VMA(2), VMA(3) );
		return 0;

	case TRAP_ANGLEVECTORS:
        G_LogPrintF( "syscall: TRAP_ANGLEVECTORS(%d)\n", args[0] );
		AngleVectors( VMA(1), VMA(2), VMA(3), VMA(4) );
		return 0;

	case TRAP_PERPENDICULARVECTOR:
        G_LogPrintF( "syscall: TRAP_PERPENDICULARVECTOR(%d)\n", args[0] );
		PerpendicularVector( VMA(1), VMA(2) );
		return 0;

	case TRAP_FLOOR:
        G_LogPrintF( "syscall: TRAP_FLOOR(%d)\n", args[0] );
		return FloatAsInt( floor( VMF(1) ) );

	case TRAP_CEIL:
        G_LogPrintF( "syscall: TRAP_CEIL(%d)\n", args[0] );
		return FloatAsInt( ceil( VMF(1) ) );


	default:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", (long int) args[0] );
	}
	return 0;
}

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
	int		i;

	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	//   now done before GAME_INIT call
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		svs.clients[i].gentity = NULL;
	}
	
	// use the current msec count for a random seed
	// init for this gamestate
	VM_Call (gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart);
}



/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart(gvm, qtrue);
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	cvar_t	*var;
	//FIXME these are temp while I make bots run in vm
	extern int	bot_enable;

	var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	// load the dll or bytecode
	gvm = VM_Create( "qagame", SV_GameSystemCalls, Cvar_VariableValue( "vm_game" ) );
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Create on game failed" );
	}

	SV_InitGameVM( qfalse );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return VM_Call( gvm, GAME_CONSOLE_COMMAND );
}

