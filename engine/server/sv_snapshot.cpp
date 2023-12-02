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

#include "server.h"


struct netSliceOverhead_t
{
	uint64_t numBytesSent;
	uint64_t numBytesWritten;
	const char* name;
	qbool (*processCommand_f)( const char* cmd );
	qbool (*processEntity_f)( const entityState_t* ent );
};

struct netOverhead_t
{
	uint64_t numBytesSent; // total
	netSliceOverhead_t slices[64];
	int numSlices;
};

static netOverhead_t net_overhead;


static void SV_TrackEntityOverhead( int offset, const msg_t* msg, const entityState_t* ent )
{
	for ( int s = 0; s < net_overhead.numSlices; ++s ) {
		netSliceOverhead_t* ovh = &net_overhead.slices[s];
		if ( ovh->processEntity_f != NULL && ovh->processEntity_f( ent ) ) {
			const int sent = ( msg->bit - offset + 7 ) / 8;
			ovh->numBytesSent += sent;
			ovh->numBytesWritten += sent; // don't care enough to track this
		}
	}
}


/*
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4	sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1	svc_snapshot
4	last client reliable command
4	serverTime
1	lastframe for delta compression
1	snapFlags
1	areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/


// write a delta update of an entityState_t list to the message

static void SV_EmitPacketEntities( const clientSnapshot_t* from, clientSnapshot_t* to, msg_t* msg )
{
	entityState_t* newent = NULL;
	const entityState_t* oldent = NULL;
	int newindex = 0, oldindex = 0;
	int newnum, oldnum;
	int from_num_entities = from ? from->num_entities : 0;

	while ( newindex < to->num_entities || oldindex < from_num_entities ) {
		if ( newindex >= to->num_entities ) {
			newnum = 9999;
		} else {
			newent = &svs.snapshotEntities[(to->first_entity+newindex) % svs.numSnapshotEntities];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = 9999;
		} else {
			oldent = &svs.snapshotEntities[(from->first_entity+oldindex) % svs.numSnapshotEntities];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position: because the force parm is false,
			// no bytes will be emitted if the entity has not changed at all
			const int offset = msg->bit;
			MSG_WriteDeltaEntity( msg, oldent, newent, qfalse );
			SV_TrackEntityOverhead( offset, msg, newent );
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			const int offset = msg->bit;
			MSG_WriteDeltaEntity( msg, &sv.svEntities[newnum].baseline, newent, qtrue );
			SV_TrackEntityOverhead( offset, msg, newent );
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteDeltaEntity( msg, oldent, NULL, qtrue );
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );	// end of packetentities
}


/*
==================
SV_WriteSnapshotToClient
==================
*/
static void SV_WriteSnapshotToClient( client_t *client, msg_t *msg ) {
	clientSnapshot_t	*frame, *oldframe;
	int					lastframe;
	int					i;
	int					snapFlags;

	// this is the snapshot we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	// try to use a previous frame as the source for delta compressing the snapshot
	if ( client->deltaMessage <= 0 || client->state != CS_ACTIVE ) {
		// client is asking for a retransmit
		oldframe = NULL;
		lastframe = 0;
	} else if ( client->netchan.outgoingSequence - client->deltaMessage 
		>= (PACKET_BACKUP - 3) ) {
		// client hasn't gotten a good message through in a long time
		Com_DPrintf ("%s: Delta request from out of date packet.\n", client->name);
		oldframe = NULL;
		lastframe = 0;
	} else {
		// we have a valid snapshot to delta from
		oldframe = &client->frames[ client->deltaMessage & PACKET_MASK ];
		lastframe = client->netchan.outgoingSequence - client->deltaMessage;

		// the snapshot's entities may still have rolled off the buffer, though
		if ( oldframe->first_entity <= svs.nextSnapshotEntities - svs.numSnapshotEntities ) {
			Com_DPrintf ("%s: Delta request from out of date entities.\n", client->name);
			oldframe = NULL;
			lastframe = 0;
		}
	}

	MSG_WriteByte (msg, svc_snapshot);

	// NOTE, MRE: now sent at the start of every message from server to client
	// let the client know which reliable clientCommands we have received
	//MSG_WriteLong( msg, client->lastClientCommand );

	// send over the current server time so the client can drift
	// its view of time to try to match
	if( client->oldServerTime ) {
		// The server has not yet got an acknowledgement of the
		// new gamestate from this client, so continue to send it
		// a time as if the server has not restarted. Note from
		// the client's perspective this time is strictly speaking
		// incorrect, but since it'll be busy loading a map at
		// the time it doesn't really matter.
		MSG_WriteLong (msg, svs.time + client->oldServerTime);
	} else {
		MSG_WriteLong (msg, svs.time);
	}
    
	// what we are delta'ing from
	MSG_WriteByte (msg, lastframe);

	snapFlags = svs.snapFlagServerBit;
	if ( client->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= SNAPFLAG_NOT_ACTIVE;
	}

	MSG_WriteByte (msg, snapFlags);

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	MSG_WriteData (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	if ( oldframe ) {
		MSG_WriteDeltaPlayerstate( msg, &oldframe->ps, &frame->ps );
	} else {
		MSG_WriteDeltaPlayerstate( msg, NULL, &frame->ps );
	}

	// delta encode the entities
	SV_EmitPacketEntities (oldframe, frame, msg);

	// padding for rate debugging
	if ( sv_padPackets->integer ) {
		for ( i = 0 ; i < sv_padPackets->integer ; i++ ) {
			MSG_WriteByte (msg, svc_nop);
		}
	}
}


/*
==================
SV_UpdateServerCommandsToClient

(re)send all server commands the client hasn't acknowledged yet
==================
*/
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) {
	// write any unacknowledged serverCommands
	for ( int i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
		const char* const cmd = client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)];
		const int offset = msg->bit;

		MSG_WriteByte( msg, svc_serverCommand );
		MSG_WriteLong( msg, i );
		MSG_WriteString( msg, cmd );

		for ( int s = 0; s < net_overhead.numSlices; ++s ) {
			netSliceOverhead_t* ovh = &net_overhead.slices[s];
			if ( ovh->processCommand_f != NULL && ovh->processCommand_f( cmd ) ) {
				// don't forget the protocol headers and the string's null-terminator
				ovh->numBytesSent += ( msg->bit - offset + 7 ) / 8;
				ovh->numBytesWritten += 5 + strlen( cmd ) + 1;
			}
		}
	}
	client->reliableSent = client->reliableSequence;
}

/*
=============================================================================

Build a client snapshot structure

=============================================================================
*/

#define	MAX_SNAPSHOT_ENTITIES	1024
typedef struct {
	int		numSnapshotEntities;
	int		snapshotEntities[MAX_SNAPSHOT_ENTITIES];
} snapshotEntityNumbers_t;


static int QDECL SV_QsortEntityNumbers( const void *a, const void *b )
{
	const int *ea = (const int *)a;
	const int *eb = (const int *)b;

	if ( *ea == *eb ) {
		Com_Error( ERR_DROP, "SV_QsortEntityStates: duplicated entity" );
	}

	if ( *ea < *eb ) {
		return -1;
	}

	return 1;
}


static void SV_AddEntToSnapshot( svEntity_t *svEnt, const sharedEntity_t *gEnt, snapshotEntityNumbers_t *eNums )
{
	// if we have already added this entity to this snapshot, don't add again
	if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
		return;
	}
	svEnt->snapshotCounter = sv.snapshotCounter;

	// if we are full, silently discard entities
	if ( eNums->numSnapshotEntities == MAX_SNAPSHOT_ENTITIES ) {
		return;
	}

	eNums->snapshotEntities[ eNums->numSnapshotEntities ] = gEnt->s.number;
	eNums->numSnapshotEntities++;
}


static void SV_AddEntitiesVisibleFromPoint( const vec3_t origin,
		clientSnapshot_t *frame, snapshotEntityNumbers_t *eNums )
{
	int i, l;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specfically check for it
	if ( !sv.state ) {
		return;
	}

	int leafnum = CM_PointLeafnum( origin );
	int clientarea = CM_LeafArea( leafnum );
	int clientcluster = CM_LeafCluster( leafnum );
	const byte* clientpvs = CM_ClusterPVS( clientcluster );

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits( frame->areabits, clientarea );

	for (int e = 0; e < sv.num_entities; ++e) {
		const sharedEntity_t* ent = SV_GentityNum(e);

		// never send entities that aren't linked in
		if ( !ent->r.linked ) {
			continue;
		}

		// entities can be flagged to explicitly not be sent to the client
		if ( ent->r.svFlags & SVF_NOCLIENT ) {
			continue;
		}

		// entities can be flagged to be sent to only one client
		if ( ent->r.svFlags & SVF_SINGLECLIENT ) {
			if ( ent->r.singleClient != frame->ps.clientNum ) {
				continue;
			}
		}
		// entities can be flagged to be sent to everyone but one client
		if ( ent->r.svFlags & SVF_NOTSINGLECLIENT ) {
			if ( ent->r.singleClient == frame->ps.clientNum ) {
				continue;
			}
		}
		// entities can be flagged to be sent to a given mask of clients
		if ( ent->r.svFlags & SVF_CLIENTMASK ) {
			if (frame->ps.clientNum >= 32)
				Com_Error( ERR_DROP, "SVF_CLIENTMASK: clientNum >= 32\n" );
			if (~ent->r.singleClient & (1 << frame->ps.clientNum))
				continue;
		}

		svEntity_t* svEnt = SV_SvEntityForGentity( ent );

		// don't double add an entity through portals
		if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
			continue;
		}

#if defined( QC )
		if ( ent->s.number >= 0 && ent->s.number < MAX_CLIENTS &&
		    ( SV_GentityNum( frame->ps.clientNum )->r.piercingSightMask & ( 1 << ent->s.number ) ) )
		{
			SV_AddEntToSnapshot( svEnt, ent, eNums );
			continue;
		}
#endif // QC

		// broadcast entities are always sent
		if ( ent->r.svFlags & SVF_BROADCAST ) {
			SV_AddEntToSnapshot( svEnt, ent, eNums );
			continue;
		}

		// ignore if not touching a PV leaf
		// check area
		if ( !CM_AreasConnected( clientarea, svEnt->areanum ) ) {
			// doors can legally straddle two areas, so
			// we may need to check another one
			if ( !CM_AreasConnected( clientarea, svEnt->areanum2 ) ) {
				continue;		// blocked by a door
			}
		}

		// check individual leafs
		if ( !svEnt->numClusters ) {
			continue;
		}
		l = 0;
		for ( i=0 ; i < svEnt->numClusters ; i++ ) {
			l = svEnt->clusternums[i];
			if ( clientpvs[l >> 3] & (1 << (l&7) ) ) {
				break;
			}
		}

		// if we haven't found it to be visible,
		// check overflow clusters that couldn't be stored
		if ( i == svEnt->numClusters ) {
			if ( svEnt->lastCluster ) {
				for ( ; l <= svEnt->lastCluster ; l++ ) {
					if ( clientpvs[l >> 3] & (1 << (l&7) ) ) {
						break;
					}
				}
				if ( l == svEnt->lastCluster ) {
					continue;	// not visible
				}
			} else {
				continue;
			}
		}

		// add it
		SV_AddEntToSnapshot( svEnt, ent, eNums );

		// if its a portal entity, add everything visible from its camera position
		if ( ent->r.svFlags & SVF_PORTAL ) {
			if ( ent->s.generic1 ) {
				vec3_t dir;
				VectorSubtract(ent->s.origin, origin, dir);
				if ( VectorLengthSquared(dir) > (float) ent->s.generic1 * ent->s.generic1 ) {
					continue;
				}
			}
			SV_AddEntitiesVisibleFromPoint( ent->s.origin2, frame, eNums );
		}

	}
}


/*
=============
SV_BuildClientSnapshot

Decides which entities are going to be visible to the client, and
copies off the playerstate and areabits.

This properly handles multiple recursive portals, but the render
currently doesn't.

For viewing through other player's eyes, clent can be something other than client->gentity
=============
*/
static void SV_BuildClientSnapshot( client_t *client ) {
	vec3_t						org;
	clientSnapshot_t			*frame;
	snapshotEntityNumbers_t		entityNumbers;
	int							i;
	sharedEntity_t				*ent;
	entityState_t				*state;
	svEntity_t					*svEnt;
	sharedEntity_t				*clent;
	int							clientNum;
	playerState_t				*ps;

	// bump the counter used to prevent double adding
	sv.snapshotCounter++;

	// this is the frame we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	// clear everything in this snapshot
	entityNumbers.numSnapshotEntities = 0;
	Com_Memset( frame->areabits, 0, sizeof( frame->areabits ) );
	frame->num_entities = 0;

	clent = client->gentity;
	if ( !clent || client->state == CS_ZOMBIE ) {
		return;
	}

	// grab the current playerState_t
	ps = SV_GameClientNum( client - svs.clients );
	frame->ps = *ps;

	// never send client's own entity, because it can
	// be regenerated from the playerstate
	clientNum = frame->ps.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	svEnt = &sv.svEntities[ clientNum ];

	svEnt->snapshotCounter = sv.snapshotCounter;

	// find the client's viewpoint
	VectorCopy( ps->origin, org );
	org[2] += ps->viewheight;

	// add all the entities directly visible to the eye,
	// which may include portal entities that merge other viewpoints
	SV_AddEntitiesVisibleFromPoint( org, frame, &entityNumbers );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	qsort( entityNumbers.snapshotEntities, entityNumbers.numSnapshotEntities, 
		sizeof( entityNumbers.snapshotEntities[0] ), SV_QsortEntityNumbers );

	// now that all viewpoint's areabits have been OR'd together, invert
	// all of them to make it a mask vector, which is what the renderer wants
	for ( i = 0 ; i < MAX_MAP_AREA_BYTES/4 ; i++ ) {
		((int *)frame->areabits)[i] = ((int *)frame->areabits)[i] ^ -1;
	}

	// copy the entity states out
	frame->num_entities = 0;
	frame->first_entity = svs.nextSnapshotEntities;
	for ( i = 0 ; i < entityNumbers.numSnapshotEntities ; i++ ) {
		ent = SV_GentityNum(entityNumbers.snapshotEntities[i]);
		state = &svs.snapshotEntities[svs.nextSnapshotEntities % svs.numSnapshotEntities];
		*state = ent->s;
		svs.nextSnapshotEntities++;
		// this should never hit, map should always be restarted first in SV_Frame
		if ( svs.nextSnapshotEntities >= 0x7FFFFFFE ) {
			Com_Error(ERR_FATAL, "svs.nextSnapshotEntities wrapped");
		}
		frame->num_entities++;
	}
}


/*
====================
SV_RateMsec

Return the number of msec a given size message is supposed
to take to clear, based on the current rate
====================
*/
#define	HEADER_RATE_BYTES	48		// include our header, IP header, and some overhead
static int SV_RateMsec( client_t *client, int messageSize )
{
	// individual messages will never be larger than fragment size
	// FIXME - use MAX_PACKETLEN or FRAGMENT_SIZE here, not random numbers...
	if ( messageSize > 1500 )
		messageSize = 1500;

	int rate = client->rate;
	if ( sv_maxRate->integer ) {
		if ( sv_maxRate->integer < 1000 ) {
			Cvar_Set( "sv_MaxRate", "1000" );
		}
		if ( sv_maxRate->integer < rate ) {
			rate = sv_maxRate->integer;
		}
	}
	if ( sv_minRate->integer ) {
		if ( sv_minRate->integer < 1000 )
			Cvar_Set( "sv_minRate", "1000" );
		if ( sv_minRate->integer > rate )
			rate = sv_minRate->integer;
	}

	return (( messageSize + HEADER_RATE_BYTES ) * 1000 / rate);
}

/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
void SV_SendMessageToClient( msg_t *msg, client_t *client ) {
	int			rateMsec;

	// record information about the message
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = Sys_Milliseconds();
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = -1;

	// send the datagram
	SV_Netchan_Transmit( client, msg );	//msg->cursize, msg->data );

	net_overhead.numBytesSent += msg->cursize;

	// set nextSnapshotTime based on rate and requested number of updates

	// local clients get snapshots every frame
	// TTimo - https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=491
	// added sv_lanForceRate check
	if ( client->netchan.remoteAddress.type == NA_LOOPBACK || (sv_lanForceRate->integer && Sys_IsLANAddress(client->netchan.remoteAddress)) ) {
		// we do NOT currently have snapshots every frame because of SV_Frame's NET_Sleep call
		// but we can avoid piling up snapshots in the same milli-second before sleeping roughly 1000/sv_fps milli-seconds...
		client->nextSnapshotTime = svs.time + 1;
		return;
	}

	// normal rate / snapshotMsec calculation
	rateMsec = SV_RateMsec( client, msg->cursize );

	if ( rateMsec < client->snapshotMsec ) {
		// never send more packets than this, no matter what the rate is at
		rateMsec = client->snapshotMsec;
		client->rateDelayed = qfalse;
	} else {
		client->rateDelayed = qtrue;
	}

	client->nextSnapshotTime = svs.time + rateMsec;

	// don't pile up empty snapshots while connecting
	if ( client->state != CS_ACTIVE ) {
		// a gigantic connection message may have already put the nextSnapshotTime
		// more than a second away, so don't shorten it
		// do shorten if client is downloading
		if ( !*client->downloadName && client->nextSnapshotTime < svs.time + 1000 ) {
			client->nextSnapshotTime = svs.time + 1000;
		}
	}
}


/*
SV_SendClientSnapshot
Also called by SV_FinalMessage
*/
void SV_SendClientSnapshot( client_t *client ) 
{

	// build the snapshot
	SV_BuildClientSnapshot( client );

	// bots need to have their snapshots built, but
	// then query them directly without needing to be sent
	if ( client->gentity && client->gentity->r.svFlags & SVF_BOT ) {
		return;
	}

    byte		msg_buf[MAX_MSGLEN];
    msg_t		msg;
    
	MSG_Init (&msg, msg_buf, sizeof(msg_buf));
	msg.allowoverflow = qtrue;

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// (re)send any reliable server commands
	SV_UpdateServerCommandsToClient( client, &msg );

	// send over all the relevant entityState_t
	// and the playerState_t
	SV_WriteSnapshotToClient( client, &msg );

	// Add any download data if the client is downloading
	SV_WriteDownloadToClient( client, &msg );

	// check for overflow
	if ( msg.overflowed ) {
		Com_Printf ("WARNING: msg overflowed for %s\n", client->name);
		MSG_Clear (&msg);
	}

	SV_SendMessageToClient( &msg, client );

/* this works fine on lan (160K/s dl, yay) and SEEMS okay over the net, but needs more testing
#define UNSUCK_DOWNLOADS
#if defined( UNSUCK_DOWNLOADS )
	// KHB 071111  the whole reason Q3 dl's SUCK is that there's a "secret" artificial cap
	// based on UDP packet sizes. so the server "sends" a 2K block each snap, but it's actually
	// cut down to 1300 bytes, MINUS the game traffic, so the ABSOLUTE peak dl speed is roughly
	// 1K * 30 snaps, EVEN ON LAN; and the realistic dl speed from an active server is about
	// 400 bytes * svfps, depending on how many random newbs are spamming plasma and chat etc
	while (client->netchan.unsentFragments) {
		SV_Netchan_TransmitNextFragment( client );
	}
#endif
*/
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void ) {
	int			i;
	client_t	*c;

	// send a message to each connected client
	for (i=0, c = svs.clients ; i < sv_maxclients->integer ; i++, c++) {
		// yes, we keep sending data to CS_ZOMBIE clients
		// if we don't, kicked clients never get the reliable "disconnect" command
		// and keep cgame loaded instead of dropping back to the main menu
		if (c->state == CS_FREE) {
			continue;		// not connected
		}

		if ( svs.time < c->nextSnapshotTime ) {
			continue;		// not time yet
		}

		// send additional message fragments if the last message
		// was too large to send at once
		if ( c->netchan.unsentFragments ) {
			c->nextSnapshotTime = svs.time + 
				SV_RateMsec( c, c->netchan.unsentLength - c->netchan.unsentFragmentStart );
			SV_Netchan_TransmitNextFragment( c );
			continue;
		}

		// generate and send a new message
		SV_SendClientSnapshot( c );
	}
}


void SV_PrintNetworkOverhead_f()
{
	if ( net_overhead.numBytesSent == 0 ) {
		return;
	}

	const double sentTotal = net_overhead.numBytesSent;

	double sentAll = 0.0;
	double writtenAll = 0.0;
	for ( int i = 0; i < net_overhead.numSlices; ++i ) {
		const char* name = net_overhead.slices[i].name;
		const double sent = net_overhead.slices[i].numBytesSent;
		const double written = net_overhead.slices[i].numBytesWritten;
		sentAll += sent;
		writtenAll += written;
		if ( sent == 0.0 ) {
			Com_Printf( "%s unused\n", name );
			continue;
		}

		const float overhead = sent / sentTotal;
		const float compression = written / sent;
		Com_Printf( "%s overhead:    %.2f%%\n", name, overhead * 100.0f );
		Com_Printf( "%s compression: %.2fx\n", name, compression );
	}

	if ( sentAll > 0.0 ) {
		const float overhead = sentAll / sentTotal;
		const float compression = writtenAll / sentAll;
		Com_Printf( "total overhead:    %.2f%%\n", overhead * 100.0f );
		Com_Printf( "total compression: %.2fx\n", compression );
	}
}


void SV_ClearNetworkOverhead_f()
{
	net_overhead.numBytesSent = 0;
	for ( int i = 0; i < net_overhead.numSlices; ++i ) {
		net_overhead.slices[i].numBytesSent = 0;
		net_overhead.slices[i].numBytesWritten = 0;
	}
}


/*
// simple example code to get started

static qbool IsTeamInfoCommand( const char* cmd )
{
	return Q_stricmpn( cmd, "tinfo ", 6 ) == 0;
}

void SV_InitNetworkOverhead()
{
	net_overhead.slices[0].name = "CPMA Team Info";
	net_overhead.slices[0].processCommand_f = &IsTeamInfoCommand;
	net_overhead.numSlices = 1;
}
*/


void SV_InitNetworkOverhead()
{
	// fill in the temp/debug code here
	// #define CNQ3_DEV to make the console commands available in a release build
}
