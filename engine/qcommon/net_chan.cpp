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

#include "q_shared.h"
#include "qcommon.h"
#include "common_help.h"

/*

packet header
-------------
4	outgoing sequence.  high bit will be set if this is a fragmented message
[2	qport (only for client to server)]
[2	fragment start byte]
[2	fragment length. if < FRAGMENT_SIZE, this is the last fragment]

if the sequence number is -1, the packet should be handled as an out-of-band
message instead of as part of a netcon.

All fragments will have the same sequence numbers.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.

*/


#define	MAX_PACKETLEN			1400		// max size of a network packet

#define	FRAGMENT_SIZE			(MAX_PACKETLEN - 100)
#define	PACKET_HEADER			10			// two ints and a short

#define	FRAGMENT_BIT	(1<<31)

cvar_t* showpackets;
cvar_t* showdrop;
cvar_t* qport;

static const char* netsrcString[2] = {
	"client",
	"server"
};


void Netchan_Init( int port )
{
	port &= 0xffff;
	showpackets = Cvar_Get( "showpackets", "0", CVAR_TEMP );
	Cvar_SetRange( "showpackets", CVART_BOOL, NULL, NULL );
	Cvar_SetHelp( "showpackets", "prints received and sent packets" );
	showdrop = Cvar_Get( "showdrop", "0", CVAR_TEMP );
	Cvar_SetRange( "showdrop", CVART_BOOL, NULL, NULL );
	Cvar_SetHelp( "showdrop", "prints dropped packets" );
	qport = Cvar_Get( "net_qport", va("%i", port), CVAR_INIT );
	Cvar_SetRange( "net_qport", CVART_INTEGER, "0", "65535" );
	Cvar_SetHelp( "net_qport", help_qport );
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int port ) {
	Com_Memset (chan, 0, sizeof(*chan));

	chan->sock = sock;
	chan->remoteAddress = adr;
	chan->qport = port;
	chan->incomingSequence = 0;
	chan->outgoingSequence = 1;
}


/*
=================
Netchan_TransmitNextFragment

Send one fragment of the current message
=================
*/
void Netchan_TransmitNextFragment( netchan_t *chan ) {
	msg_t		send;
	byte		send_buf[MAX_PACKETLEN];
	int			fragmentLength;

	// write the packet header
	MSG_InitOOB (&send, send_buf, sizeof(send_buf));				// <-- only do the oob here

	MSG_WriteLong( &send, chan->outgoingSequence | FRAGMENT_BIT );

	// send the qport if we are a client
	if ( chan->sock == NS_CLIENT ) {
		MSG_WriteShort( &send, qport->integer );
	}

	// copy the reliable message to the packet first
	fragmentLength = FRAGMENT_SIZE;
	if ( chan->unsentFragmentStart  + fragmentLength > chan->unsentLength ) {
		fragmentLength = chan->unsentLength - chan->unsentFragmentStart;
	}

	MSG_WriteShort( &send, chan->unsentFragmentStart );
	MSG_WriteShort( &send, fragmentLength );
	MSG_WriteData( &send, chan->unsentBuffer + chan->unsentFragmentStart, fragmentLength );

	// send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remoteAddress );

	if ( showpackets->integer ) {
		Com_Printf ("%s send %4i : s=%i fragment=%i,%i\n"
			, netsrcString[ chan->sock ]
			, send.cursize
			, chan->outgoingSequence
			, chan->unsentFragmentStart, fragmentLength);
	}

	chan->unsentFragmentStart += fragmentLength;

	// this exit condition is a little tricky, because a packet
	// that is exactly the fragment length still needs to send
	// a second packet of zero length so that the other side
	// can tell there aren't more to follow
	if ( chan->unsentFragmentStart == chan->unsentLength && fragmentLength != FRAGMENT_SIZE ) {
		chan->outgoingSequence++;
		chan->unsentFragments = qfalse;
	}
}


/*
===============
Netchan_Transmit

Sends a message to a connection, fragmenting if necessary
A 0 length will still generate a packet.
================
*/
void Netchan_Transmit( netchan_t *chan, int length, const byte *data ) {

	if ( length > MAX_MSGLEN ) {
		Com_Error( ERR_DROP, "Netchan_Transmit: length = %i", length );
	}
	chan->unsentFragmentStart = 0;

	// fragment large reliable messages
	if ( length >= FRAGMENT_SIZE ) {
		chan->unsentFragments = qtrue;
		chan->unsentLength = length;
		Com_Memcpy( chan->unsentBuffer, data, length );

		// only send the first fragment now
		Netchan_TransmitNextFragment( chan );
		return;
	}

	msg_t		send;
	byte		send_buf[MAX_PACKETLEN];
	// write the packet header
	MSG_InitOOB (&send, send_buf, sizeof(send_buf));

	MSG_WriteLong( &send, chan->outgoingSequence );
	chan->outgoingSequence++;

	// send the qport if we are a client
	if ( chan->sock == NS_CLIENT ) {
		MSG_WriteShort( &send, qport->integer );
	}

	MSG_WriteData( &send, data, length );

	// send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remoteAddress );

	if ( showpackets->integer ) {
		Com_Printf( "%s send %4i : s=%i ack=%i\n"
			, netsrcString[ chan->sock ]
			, send.cursize
			, chan->outgoingSequence - 1
			, chan->incomingSequence );
	}
}


/*
Returns false if the message should not be processed due to being
out of order or a fragment.

Msg must be large enough to hold MAX_MSGLEN, because if this is the
final fragment of a multi-part message, the entire thing will be
copied out.
*/
qbool Netchan_Process( netchan_t *chan, msg_t *msg )
{
	int		sequence;
	int		fragmentStart, fragmentLength;
	qbool	fragmented;

	// get sequence numbers
	MSG_BeginReadingOOB( msg );
	sequence = MSG_ReadLong( msg );

	// check for fragment information
	if ( sequence & FRAGMENT_BIT ) {
		sequence &= ~FRAGMENT_BIT;
		fragmented = qtrue;
	} else {
		fragmented = qfalse;
	}

	// read the port if we are a server
	if ( chan->sock == NS_SERVER ) {
		MSG_ReadShort( msg );
	}

	// read the fragment information
	if ( fragmented ) {
		fragmentStart = MSG_ReadShort( msg );
		fragmentLength = MSG_ReadShort( msg );
	} else {
		fragmentStart = 0;		// stop warning message
		fragmentLength = 0;
	}

	if ( showpackets->integer ) {
		if ( fragmented ) {
			Com_Printf( "%s recv %4i : s=%i fragment=%i,%i\n"
				, netsrcString[ chan->sock ]
				, msg->cursize
				, sequence
				, fragmentStart, fragmentLength );
		} else {
			Com_Printf( "%s recv %4i : s=%i\n"
				, netsrcString[ chan->sock ]
				, msg->cursize
				, sequence );
		}
	}

	//
	// discard out of order or duplicated packets
	//
	if ( sequence <= chan->incomingSequence ) {
		if ( showdrop->integer || showpackets->integer ) {
			Com_Printf( "%s:Out of order packet %i at %i\n"
				, NET_AdrToString( chan->remoteAddress )
				,  sequence
				, chan->incomingSequence );
		}
		return qfalse;
	}

	//
	// dropped packets don't keep the message from being used
	//
	chan->dropped = sequence - (chan->incomingSequence+1);
	if ( chan->dropped > 0 ) {
		if ( showdrop->integer || showpackets->integer ) {
			Com_Printf( "%s:Dropped %i packets at %i\n"
			, NET_AdrToString( chan->remoteAddress )
			, chan->dropped
			, sequence );
		}
	}

	//
	// if this is the final fragment of a reliable message,
	// bump incoming_reliable_sequence
	//
	if ( fragmented ) {
		// TTimo
		// make sure we add the fragments in correct order
		// either a packet was dropped, or we received this one too soon
		// we don't reconstruct the fragments. we will wait till this fragment gets to us again
		// (NOTE: we could probably try to rebuild by out of order chunks if needed)
		if ( sequence != chan->fragmentSequence ) {
			chan->fragmentSequence = sequence;
			chan->fragmentLength = 0;
		}

		// if we missed a fragment, dump the message
		if ( fragmentStart != chan->fragmentLength ) {
			if ( showdrop->integer || showpackets->integer ) {
				Com_Printf( "%s:Dropped a message fragment at %i\n"
				, NET_AdrToString( chan->remoteAddress )
				, sequence);
			}
			// we can still keep the part that we have so far,
			// so we don't need to clear chan->fragmentLength
			return qfalse;
		}

		// copy the fragment to the fragment buffer
		if ( fragmentLength < 0 || msg->readcount + fragmentLength > msg->cursize ||
			chan->fragmentLength + fragmentLength > sizeof( chan->fragmentBuffer ) ) {
			if ( showdrop->integer || showpackets->integer ) {
				Com_Printf ("%s:illegal fragment length\n"
				, NET_AdrToString (chan->remoteAddress ) );
			}
			return qfalse;
		}

		Com_Memcpy( chan->fragmentBuffer + chan->fragmentLength, 
			msg->data + msg->readcount, fragmentLength );

		chan->fragmentLength += fragmentLength;

		// if this wasn't the last fragment, don't process anything
		if ( fragmentLength == FRAGMENT_SIZE ) {
			return qfalse;
		}

		if ( chan->fragmentLength > msg->maxsize ) {
			Com_Printf( "%s:fragmentLength %i > msg->maxsize\n"
				, NET_AdrToString (chan->remoteAddress ),
				chan->fragmentLength );
			return qfalse;
		}

		// copy the full message over the partial fragment

		// make sure the sequence number is still there
		*(int *)msg->data = LittleLong( sequence );

		Com_Memcpy( msg->data + 4, chan->fragmentBuffer, chan->fragmentLength );
		msg->cursize = chan->fragmentLength + 4;
		msg->readcount = 4;	// past the sequence number
		msg->bit = 32;	// past the sequence number

		chan->fragmentLength = 0;
		chan->incomingSequence = sequence;

		return qtrue;
	}

	//
	// the message can now be read from the current message pointer
	//
	chan->incomingSequence = sequence;

	return qtrue;
}


//==============================================================================


// compares without the port

qbool NET_CompareBaseAdr( const netadr_t& a, const netadr_t& b )
{
	if (a.type != b.type)
		return qfalse;

	if (a.type == NA_LOOPBACK)
		return qtrue;

	if (a.type == NA_IP)
		return (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3]);

	Com_Printf ("NET_CompareBaseAdr: bad address type\n");
	return qfalse;
}


qbool NET_CompareAdr( const netadr_t& a, const netadr_t& b )
{
	return (NET_CompareBaseAdr(a, b) && (a.port == b.port));
}


const char* NET_AdrToString( const netadr_t& a )
{
	static char s[64];

	if (a.type == NA_LOOPBACK) {
		return "loopback";
	} else if (a.type == NA_BOT) {
		return "bot";
	} else if (a.type == NA_IP) {
		Com_sprintf (s, sizeof(s), "%i.%i.%i.%i:%hu",
			a.ip[0], a.ip[1], a.ip[2], a.ip[3], BigShort(a.port));
		return s;
	}

	Com_Printf( "NET_AdrToString: bad address type\n" );
	return "???";
}


qbool NET_IsLocalAddress( const netadr_t& a )
{
	return (a.type == NA_LOOPBACK);
}



/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/

// there needs to be enough loopback messages to hold a complete
// gamestate of maximum size
#define	MAX_LOOPBACK	16

typedef struct {
	byte	data[MAX_PACKETLEN];
	int		datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];


qbool NET_GetLoopPacket( netsrc_t sock, netadr_t *net_from, msg_t *net_message)
{
	loopback_t* loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return qfalse;

	int i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	Com_Memcpy( net_message->data, loop->msgs[i].data, loop->msgs[i].datalen );
	net_message->cursize = loop->msgs[i].datalen;
	Com_Memset( net_from, 0, sizeof(*net_from) );
	net_from->type = NA_LOOPBACK;
	return qtrue;
}


static void NET_SendLoopPacket( netsrc_t sock, int length, const void* data, const netadr_t& to )
{
	loopback_t* loop = &loopbacks[sock^1];

	int i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	Com_Memcpy( loop->msgs[i].data, data, length );
	loop->msgs[i].datalen = length;
}


//=============================================================================

typedef struct packetQueue_s {
	struct packetQueue_s* next;
	int length;
	byte* data;
	netadr_t to;
	int release;
} packetQueue_t;

static packetQueue_t* packetQueue = NULL;

static void NET_QueuePacket( int length, const void* data, netadr_t to, int offset )
{
	packetQueue_t* pq = (packetQueue_t*)S_Malloc(sizeof(packetQueue_t));
	pq->data = (byte*)S_Malloc(length);
	Com_Memcpy( pq->data, data, length );
	pq->length = length;
	pq->to = to;
	pq->release = Sys_Milliseconds() + ((float)min(offset, 999) / com_timescale->value);
	pq->next = NULL;

	if (!packetQueue) {
		packetQueue = pq;
		return;
	}

	packetQueue_t* next = packetQueue;
	while (next) {
		if (!next->next) {
			next->next = pq;
			return;
		}
		next = next->next;
	}
}


void NET_FlushPacketQueue()
{
	packetQueue_t* last;

	while (packetQueue) {
		if (packetQueue->release >= Sys_Milliseconds())
			break;
		Sys_SendPacket( packetQueue->length, packetQueue->data, packetQueue->to );
		last = packetQueue;
		packetQueue = packetQueue->next;
		Z_Free(last->data);
		Z_Free(last);
	}
}


void NET_SendPacket( netsrc_t sock, int length, const void* data, const netadr_t& to )
{
	// sequenced packets are shown in netchan, so just show OOB
	if ( showpackets->integer && *(int *)data == -1 ) {
		Com_Printf ("send packet %4i\n", length);
	}

	if ( to.type == NA_LOOPBACK ) {
		NET_SendLoopPacket( sock, length, data, to );
		return;
	}
	if ( to.type == NA_BOT ) {
		return;
	}
	if ( to.type == NA_BAD ) {
		return;
	}

	if ( sock == NS_CLIENT && cl_packetdelay->integer > 0 ) {
		NET_QueuePacket( length, data, to, cl_packetdelay->integer );
	}
	else if ( sock == NS_SERVER && sv_packetdelay->integer > 0 ) {
		NET_QueuePacket( length, data, to, sv_packetdelay->integer );
	}
	else {
		Sys_SendPacket( length, data, to );
	}
}


// sends a text message in an out-of-band datagram

void QDECL NET_OutOfBandPrint( netsrc_t sock, const netadr_t& adr, PRINTF_FORMAT_STRING const char* format, ... )
{
	char string[MAX_MSGLEN];

	// set the header
	string[0] = -1;
	string[1] = -1;
	string[2] = -1;
	string[3] = -1;

	va_list argptr;
	va_start( argptr, format );
	int n = 4 + vsnprintf( string+4, sizeof(string)-4, format, argptr );
	va_end( argptr );

	NET_SendPacket( sock, n, string, adr );
}


// sends a data message in an out-of-band datagram (only used for "connect")

void QDECL NET_OutOfBandData( netsrc_t sock, const netadr_t& adr, const byte* data, int len )
{
	assert( len+4 < MAX_MSGLEN );

	byte string[MAX_MSGLEN];

	// set the header
	string[0] = 0xff;
	string[1] = 0xff;
	string[2] = 0xff;
	string[3] = 0xff;

	for (int i = 0; i < len; ++i)
		string[i+4] = data[i];

	msg_t mbuf;
	mbuf.data = string;
	mbuf.cursize = len+4;
	DynHuff_Compress( &mbuf, 12 );

	NET_SendPacket( sock, mbuf.cursize, mbuf.data, adr );
}


// traps "localhost" for loopback, passes everything else to system

qbool NET_StringToAdr( const char* s, netadr_t* a )
{
	if (!strcmp(s, "localhost")) {
		Com_Memset( a, 0, sizeof(*a) );
		a->type = NA_LOOPBACK;
		return qtrue;
	}

	char base[MAX_STRING_CHARS];
	Q_strncpyz( base, s, sizeof( base ) );

	char* port = strstr( base, ":" );
	if (port)
		*port++ = 0;

	if (!Sys_StringToAdr( base, a )) {
		a->type = NA_BAD;
		return qfalse;
	}

	// inet_addr returns this if out of range
	if ( a->ip[0] == 255 && a->ip[1] == 255 && a->ip[2] == 255 && a->ip[3] == 255 ) {
		a->type = NA_BAD;
		return qfalse;
	}

	if ( port ) {
		a->port = BigShort( (short)atoi( port ) );
	} else {
		a->port = BigShort( PORT_SERVER );
	}

	return qtrue;
}

