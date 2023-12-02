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
//
// q_shared.c -- stateless support routines that are included in each code dll

#include "q_shared.h"

#if defined(_DEBUG) && defined(_MSC_VER)
#include "../win32/windows.h"
#endif


#if defined(__cplusplus)
extern "C" {
#endif


float Com_Clamp( float min, float max, float value )
{
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


int Com_ClampInt( int min, int max, int value )
{
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


const char* COM_SkipPath( const char* pathname )
{
	const char* last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname+1;
		++pathname;
	}
	return last;
}


void COM_StripExtension( const char* in, char* out, int destsize )
{
	char* p;

	Q_strncpyz( out, in, destsize );
	p = out + strlen(out) - 1;

	while (p-- > out) {
		if (*p == '.') {
			*p = 0;
			return;		// nuke it and bail
		}
		if (*p == '/') {
			return;		// no extension
		}
	}
}


// if path doesn't have a .EXT, append ext (which should include the .)

void COM_DefaultExtension( char* path, int maxSize, const char* ext )
{
	char oldPath[MAX_QPATH];
	const char* src = path + strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.') {
			return;		// already has an extension
		}
		--src;
	}

	Q_strncpyz( oldPath, path, sizeof( oldPath ) );
	Com_sprintf( path, maxSize, "%s%s", oldPath, ext );
}


/*
============================================================================

BYTE ORDER FUNCTIONS
these don't belong in here and should never be used by a VM

============================================================================
*/

#ifndef Q3_VM

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

typedef union {
    float	f;
    unsigned int i;
} _FloatByteUnion;

float FloatSwap (const float *f) {
	_FloatByteUnion out;

	out.f = *f;
	out.i = LongSwap(out.i);

	return out.f;
}

#endif


/*
============================================================================

PARSING

============================================================================
*/

static	char	com_token[MAX_TOKEN_CHARS];


const char* COM_Parse( const char** data_p )
{
	return COM_ParseExt( data_p, qtrue );
}


static const char* SkipWhitespace( const char* data, qbool* hasNewLines )
{
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}


int COM_Compress( char* p )
{
	int c;
	char* in = p;
	char* out = p;
	int wschar = 0;

	assert(in);
	if (!in)
		return 0;

	while (c = *in++) {
		// skip double slash comments
		if ( c == '/' && *in == '/' ) {
			while (*in && *in != '\n')
				++in;

		// skip /* */ comments (note: id code fails to handle "/*/" properly so we have to get it wrong too)
		} else if ( c == '/' && *in == '*' ) {
			while (*in && (*in != '*' || in[1] != '/'))
				++in;
			if ( *in )
				in += 2;

		// record when we hit a newline
		} else if ( c == '\n' || c == '\r' ) {
			wschar = '\n';

		// record when we hit whitespace
		} else if ( c == ' ' || c == '\t' ) {
			if (!wschar)
				wschar = ' ';

		// an actual token
		} else {
			// emit any pending separator
			if (wschar) {
				*out++ = wschar;
				wschar = 0;
			}
			// copy quoted strings unmolested
			if (c == '"') {
				*out++ = c;
				do {
					*out++ = *in++;
				} while ((c = *in) && (c != '"'));
				++in;
			}
			*out++ = c;
		}
	}

	*out = 0;
	return out - p;
}


// parse a token out of a string - will never return NULL, just empty strings
// if "allowLineBreaks" is true then an empty string will be returned if the next token is a newline

const char* COM_ParseExt( const char** data_p, qbool allowLineBreaks )
{
	int c = 0, len = 0;
	qbool hasNewLines = qfalse;
	const char* data = *data_p;

	com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
*/
void SkipBracedSection( const char** data )
{
	int depth = 0;
	const char* token;

	do {
		token = COM_ParseExt( data, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) {
				depth++;
			}
			else if( token[0] == '}' ) {
				depth--;
			}
		}
	} while (depth && *data);
}


void SkipRestOfLine( const char** data )
{
	int c;
	const char* p = *data;

	while ( (c = *p++) && (c != '\n') )
		;

	*data = p;
}


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int Q_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}

int Q_islower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}

int Q_isupper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}

int Q_isalpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}

char* Q_strrchr( const char* string, int c )
{
	char cc = c;
	char *s;
	char *sp=(char *)0;

	s = (char*)string;

	while (*s)
	{
		if (*s == cc)
			sp = s;
		s++;
	}
	if (cc == 0)
		sp = s;

	return sp;
}


const char *Q_stristr( const char *s, const char *find )
{
	char c;
	if ((c = *find++) != 0) {
		const size_t len = strlen(find);

		if (c >= 'a' && c <= 'z')
			c -= ('a' - 'A');

		do {
			char sc;
			do {
				if ((sc = *s++) == 0)
					return NULL;
				if (sc >= 'a' && sc <= 'z')
					sc -= ('a' - 'A');
			} while (sc != c);
		} while (Q_stricmpn(s, find, len) != 0);
		s--;
	}

	return s;
}


// safe strncpy that ensures a trailing zero

void Q_strncpyz( char *dest, const char *src, int destsize )
{
	if ( !dest ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" );
	}
	if ( !src ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" );
	}
	if ( destsize < 1 ) {
		Com_Error( ERR_FATAL,"Q_strncpyz: destsize < 1" );
	}

	strncpy( dest, src, destsize-1 );
	dest[destsize-1] = 0;
}

int Q_stricmpn( const char *s1, const char *s2, int n )
{
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);

	return 0;		// strings are equal
}

int Q_strncmp( const char *s1, const char *s2, int n )
{
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}

		if (c1 != c2) {
			return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	return 0;		// strings are equal
}

int Q_stricmp( const char *s1, const char *s2 )
{
	return (s1 && s2) ? Q_stricmpn (s1, s2, 99999) : -1;
}


char *Q_strlwr( char *s1 )
{
	char* s = s1;
	while ( *s ) {
		*s = tolower(*s);
		s++;
	}
	return s1;
}

char *Q_strupr( char *s1 )
{
	char* s = s1;
	while ( *s ) {
		*s = toupper(*s);
		s++;
	}
	return s1;
}


// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src )
{
	int		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}


int Q_PrintStrlen( const char *string )
{
	int len = 0;
	const char* p = string;

	if (!p)
		return 0;

	while ( *p ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


int Q_PrintStroff( const char *string, int charOffset )
{
	int len = 0;
	const char* p = string;

	if (!p)
		return 0;

	while ( *p && len < charOffset ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return p - string;
}


char *Q_CleanStr( char *string ) {
	char*	d;
	char*	s;
	int		c;

	s = string;
	d = string;
	while ((c = *s) != 0 ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		}		
		else if ( c >= 0x20 && c <= 0x7E ) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}


void QDECL Com_sprintf( char *dest, int size, PRINTF_FORMAT_STRING const char *fmt, ...) {
	int		len;
	va_list		argptr;
	char	bigbuffer[32000];	// big, but small enough to fit in PPC stack

	va_start (argptr,fmt);
	len = vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);
	if ( len >= sizeof( bigbuffer ) ) {
		Com_Error( ERR_FATAL, "Com_sprintf: overflowed bigbuffer" );
	}
	// don't spam overflow warnings to end-users, especially since it's a perfectly normal
	// thing to happen with command and cvar auto-completion
#if defined(_DEBUG) || defined(DEBUG)
	if (len >= size) {
		Com_Printf ("Com_sprintf: overflow of %i in %i\n", len, size);
	}
#endif
	Q_strncpyz (dest, bigbuffer, size );
}


/*
============
does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
const char* QDECL va( PRINTF_FORMAT_STRING const char* format, ... )
{
	static char string[4][32000];	// in case va is called by nested functions
	static int index = 0;
	char* buf = string[index++ & 3];
	va_list argptr;

	va_start( argptr, format );
	vsprintf( buf, format, argptr );
	va_end( argptr );

	return buf;
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/


// some characters are illegal in info strings because they can mess up the server's parsing

qbool Info_Validate( const char* s )
{
	if ( strchr( s, '\"' ) )
		return qfalse;
	if ( strchr( s, ';' ) )
		return qfalse;
	return qtrue;
}

/*
===============
Info_ValueForKey

Searches the string for the given key and returns the associated value, or an empty string.
FIXME: overflow check?
===============
*/
const char* Info_ValueForKey( const char *s, const char *key )
{
	char	pkey[BIG_INFO_KEY];
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	char	*o;

	if ( !s || !key ) {
		return "";
	}

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring" );
	}

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_stricmp(key, pkey))
			return value[valueindex];

		if (!*s)
			break;
		s++;
	}

	return "";
}


/*
===================
Info_NextPair

Used to iterate through all the key/value pairs in an info string
===================
*/
void Info_NextPair( const char **head, char *key, char *value ) {
	char	*o;
	const char	*s;

	s = *head;

	if ( *s == '\\' ) {
		s++;
	}
	key[0] = 0;
	value[0] = 0;

	o = key;
	while ( *s != '\\' ) {
		if ( !*s ) {
			*o = 0;
			*head = s;
			return;
		}
		*o++ = *s++;
	}
	*o = 0;
	s++;

	o = value;
	while ( *s != '\\' && *s ) {
		*o++ = *s++;
	}
	*o = 0;

	*head = s;
}


/*
===================
Info_RemoveKey
===================
*/
void Info_RemoveKey( char *s, const char *key ) {
	char	*start;
	char	pkey[MAX_INFO_KEY];
	char	value[MAX_INFO_VALUE];
	char	*o;

	if ( strlen( s ) >= MAX_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_RemoveKey: oversize infostring" );
	}

	if (strchr (key, '\\')) {
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

/*
===================
Info_RemoveKey_Big
===================
*/
void Info_RemoveKey_Big( char *s, const char *key ) {
	char	*start;
	char	pkey[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];
	char	*o;

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_RemoveKey_Big: oversize infostring" );
	}

	if (strchr (key, '\\')) {
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


// changes or adds a key/value pair

qbool Info_SetValueForKey( char* s, const char* key, const char* value )
{
	char newi[MAX_INFO_STRING];

	if ( strlen( s ) >= MAX_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	if (strchr(key, '\\') || strchr(value, '\\'))
	{
		Com_Printf( "Can't use keys or values with a \\\n" );
		return qfalse;
	}

	if (strchr(key, ';') || strchr(value, ';'))
	{
		Com_Printf( "Can't use keys or values with a semicolon\n" );
		return qfalse;
	}

	if (strchr(key, '\"') || strchr(value, '\"'))
	{
		Com_Printf( "Can't use keys or values with a \"\n" );
		return qfalse;
	}

	Info_RemoveKey( s, key );
	if (!value || !value[0])
		return qtrue;

	Com_sprintf( newi, sizeof(newi), "\\%s\\%s", key, value );

	if (strlen(newi) + strlen(s) >= MAX_INFO_STRING)
	{
		Com_Printf( "Info string length exceeded\n" );
		return qfalse;
	}

	strcat( s, newi );
	return qtrue;
}


/*
==================
Info_SetValueForKey_Big

Changes or adds a key/value pair
==================
*/
void Info_SetValueForKey_Big( char *s, const char *key, const char *value ) {
	char	newi[BIG_INFO_STRING];

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	if (strchr (key, '\\') || strchr (value, '\\'))
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strchr (key, ';') || strchr (value, ';'))
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strchr (key, '\"') || strchr (value, '\"'))
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	Info_RemoveKey_Big (s, key);
	if (!value || !value[0])
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) >= BIG_INFO_STRING)
	{
		Com_Printf ("BIG Info string length exceeded\n");
		return;
	}

	strcat (s, newi);
}


#if defined(__cplusplus)
};
#endif
