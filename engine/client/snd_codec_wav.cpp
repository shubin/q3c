/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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
#include "snd_codec.h"


static int FGetLittleLong( fileHandle_t f )
{
	int v;
	FS_Read( &v, sizeof(v), f );
	return LittleLong(v);
}


static short FGetLittleShort( fileHandle_t f )
{
	short v;
	FS_Read( &v, sizeof(v), f );
	return LittleShort(v);
}


static int S_ReadChunkInfo( fileHandle_t f, char* name )
{
	if (FS_Read( name, 4, f ) != 4)
		return -1;

	int n = FGetLittleLong(f);
	if ( n < 0 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Negative chunk length\n" );
		return -1;
	}

	return n;
}


// returns the length of the data in the chunk, or -1 if not found

static int S_FindRIFFChunk( fileHandle_t f, const char* chunkname )
{
	char	name[4];
	int		len;

	while ( ( len = S_ReadChunkInfo(f, name) ) >= 0 )
	{
		if ( !memcmp( name, chunkname, 4 ) )
			return len;
		// not the right chunk - skip it
		len = PAD( len, 2 );
		FS_Seek( f, len, FS_SEEK_CUR );
	}

	return -1;
}


static void S_ByteSwapRawSamples( int samples, int width, int s_channels, const byte* data )
{
	if ( width != 2 ) {
		return;
	}
	if ( LittleShort( 256 ) == 256 ) {
		return;
	}

	if ( s_channels == 2 ) {
		samples <<= 1;
	}

	for (int i = 0; i < samples; ++i) {
		((short *)data)[i] = LittleShort( ((short *)data)[i] );
	}
}


static qbool S_ReadRIFFHeader( fileHandle_t f, snd_info_t* info )
{
	// skip the riff wav header
	FS_Seek( f, 12, FS_SEEK_CUR );

	// scan for the format chunk
	int fmtlen;
	if ((fmtlen = S_FindRIFFChunk(f, "fmt ")) < 0) {
		Com_Printf( S_COLOR_RED "ERROR: Couldn't find \"fmt\" chunk\n" );
		return qfalse;
	}

	// save the parts of the RIFF data we care about
	FGetLittleShort(f); // format (1: PCM)
	info->channels = FGetLittleShort(f);
	info->rate = FGetLittleLong(f);
	FGetLittleLong(f); // byte rate
	FGetLittleShort(f); // block align
	int bits = FGetLittleShort(f);

	if ( bits < 8 ) {
		Com_Printf( S_COLOR_RED "ERROR: Less than 8 bit sound is not supported\n");
		return qfalse;
	}

	info->width = bits / 8;
	info->dataofs = 0;

	// skip the rest of the format chunk if we need to
	if (fmtlen > 16)
		FS_Seek( f, fmtlen - 16, FS_SEEK_CUR );

	// scan for the data chunk
	if ((info->size = S_FindRIFFChunk(f, "data")) < 0)
	{
		Com_Printf( S_COLOR_RED "ERROR: Couldn't find \"data\" chunk\n");
		return qfalse;
	}
	info->samples = (info->size / info->width) / info->channels;

	return qtrue;
}


static byte* S_WAV_CodecLoad( const char* filename, snd_info_t* info )
{
	fileHandle_t f;
	FS_FOpenFileRead( filename, &f, qtrue );
	if (!f) {
		Com_Printf( S_COLOR_RED "ERROR: Could not open \"%s\"\n", filename );
		return NULL;
	}

	if (!S_ReadRIFFHeader( f, info )) {
		FS_FCloseFile( f );
		Com_Printf( S_COLOR_RED "ERROR: Incorrect/unsupported format in \"%s\"\n", filename );
		return NULL;
	}

	byte* buffer = (byte*)Z_Malloc( info->size );
	if (!buffer) {
		FS_FCloseFile( f );
		Com_Printf( S_COLOR_RED "ERROR: Out of memory reading \"%s\"\n", filename );
		return NULL;
	}

	FS_Read( buffer, info->size, f );
	S_ByteSwapRawSamples( info->samples, info->width, info->channels, buffer );

	FS_FCloseFile( f );
	return buffer;
}


static snd_stream_t* S_WAV_CodecOpenStream( const char* filename )
{
	snd_stream_t* rv = S_CodecUtilOpen( filename, &wav_codec );
	if (!rv)
		return NULL;

	if (!S_ReadRIFFHeader( rv->file, &rv->info )) {
		S_CodecUtilClose(rv);
		return NULL;
	}

	return rv;
}


static void S_WAV_CodecCloseStream( snd_stream_t* stream )
{
	S_CodecUtilClose( stream );
}


static int S_WAV_CodecReadStream( snd_stream_t* stream, int bytes, void* buffer )
{
	int remaining = stream->info.size - stream->pos;

	if (remaining <= 0)
		return 0;
	if (bytes > remaining)
		bytes = remaining;
	stream->pos += bytes;

	int samples = (bytes / stream->info.width) / stream->info.channels;
	FS_Read( buffer, bytes, stream->file );
	S_ByteSwapRawSamples( samples, stream->info.width, stream->info.channels, (byte*)buffer );
	return bytes;
}


snd_codec_t wav_codec =
{
	".wav",
	S_WAV_CodecLoad,
	S_WAV_CodecOpenStream,
	S_WAV_CodecReadStream,
	S_WAV_CodecCloseStream,
	NULL
};

