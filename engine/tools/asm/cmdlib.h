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

#ifndef __CMDLIB__
#define __CMDLIB__

#ifdef _MSC_VER
#pragma warning( disable : 4996 )
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>


float Q_FloatTime();

int Q_filelength (FILE *f);
int	FileTime( const char *path );

void Q_mkdir( const char* path );

FILE* SafeOpenRead( const char* filename );
FILE* SafeOpenWrite( const char* filename );
void SafeWrite( FILE* f, const void* buffer, int count );
void SafeRead( FILE* f, const void* buffer, int count );

int		LoadFile( const char *filename, void **bufferptr );
int		TryLoadFile( const char *filename, void **bufferptr );
void	SaveFile( const char *filename, const void *buffer, int count );
qboolean	FileExists( const char *filename );

void	DefaultPath( char *path, const char *basepath );
void	StripFilename( char *path );

int ParseNum( const char* s );

char *ASM_Parse (char *data);

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *copystring(const char *s);


void CRC_Init(unsigned short *crcvalue);
void CRC_ProcessByte(unsigned short *crcvalue, byte data);
unsigned short CRC_Value(unsigned short crcvalue);

void	CreatePath( const char *path );
void	QCopyFile( const char *from, const char *to );


#endif
