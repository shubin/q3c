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

static const snd_codec_t* codecs;


static const char* S_FileExtension( const char* filename )
{
	const char* fn = filename + strlen(filename) - 1;

	while (*fn != '/' && fn != filename)
	{
		if (*fn == '.')
			return fn;
		--fn;
	}

	return NULL;
}


// select an appropriate codec for a file based on its extension

static const snd_codec_t* S_FindCodecForFile( const char* filename )
{
	const snd_codec_t *codec = codecs;
	const char* ext = S_FileExtension( filename );

	if(!ext)
	{
		// No extension - auto-detect
		while(codec)
		{
			char fn[MAX_QPATH];

			// there is no extension so we do not need to subtract 4 chars
			Q_strncpyz(fn, filename, MAX_QPATH);
			COM_DefaultExtension(fn, MAX_QPATH, codec->ext);

			// Check it exists
			if (FS_ReadFile(fn, NULL) != -1)
				return codec;

			// Nope. Next!
			codec = codec->next;
		}

		// Nothin'
		return NULL;
	}

	while(codec)
	{
		if(!Q_stricmp(ext, codec->ext))
			return codec;
		codec = codec->next;
	}

	return NULL;
}


void S_CodecInit()
{
	codecs = NULL;
	S_CodecRegister(&wav_codec);
#if USE_CODEC_VORBIS
	S_CodecRegister(&ogg_codec);
#endif
}


void S_CodecShutdown()
{
	codecs = NULL;
}


void S_CodecRegister( snd_codec_t* codec )
{
	codec->next = codecs;
	codecs = codec;
}


byte* S_CodecLoad( const char* filename, snd_info_t* info )
{
	const snd_codec_t* codec = S_FindCodecForFile( filename );
	if (!codec)
	{
		Com_Printf( "Unknown extension for %s\n", filename );
		return NULL;
	}

	char fn[MAX_QPATH];
	Com_Memset( &fn, 0, sizeof(fn) );
	
	strncpy(fn, filename, sizeof(fn)-1);
	COM_DefaultExtension(fn, sizeof(fn), codec->ext);

	return (byte*)codec->load(fn, info);
}


snd_stream_t* S_CodecOpenStream( const char* filename )
{
	const snd_codec_t* codec = S_FindCodecForFile( filename );
	if (!codec)
	{
		Com_Printf( "Unknown extension for %s\n", filename );
		return NULL;
	}

	char fn[MAX_QPATH];
	Com_Memset( &fn, 0, sizeof(fn) );
	
	strncpy(fn, filename, sizeof(fn)-1);
	COM_DefaultExtension(fn, sizeof(fn), codec->ext);

	return codec->open(fn);
}

void S_CodecCloseStream(snd_stream_t *stream)
{
	stream->codec->close(stream);
}

int S_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	return stream->codec->read(stream, bytes, buffer);
}


snd_stream_t* S_CodecUtilOpen( const char* filename, const snd_codec_t* codec )
{
	fileHandle_t fh;
	int length = FS_FOpenFileRead( filename, &fh, qtrue );

	if (!fh)
	{
		Com_Printf( "Can't read sound file %s\n", filename );
		return NULL;
	}

	snd_stream_t* stream = Z_New<snd_stream_t>();
	if (!stream)
	{
		FS_FCloseFile( fh );
		return NULL;
	}

	stream->codec = codec;
	stream->file = fh;
	stream->length = length;
	return stream;
}


void S_CodecUtilClose( snd_stream_t* stream )
{
	FS_FCloseFile( stream->file );
	Z_Free( stream );
}
