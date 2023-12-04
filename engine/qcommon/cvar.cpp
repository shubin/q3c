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
// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"
#include "crash.h"
#include "common_help.h"
#include <float.h>

struct depCvar_t {
	const char* name;
	const char* newName;
	qbool warned;
};

static cvar_t* cvar_vars;
static cvar_t* cvar_cheats;
int cvar_modifiedFlags;

#define MAX_CVARS 2048
static cvar_t cvar_indexes[MAX_CVARS];
static int cvar_numIndexes;

#define CVAR_HASH_SIZE 256
static cvar_t* hashTable[CVAR_HASH_SIZE];

static qbool cvar_canPrintWarnings;

static depCvar_t cvar_depVars[] = {
	{ "sensitivity", "m_speed" },
	{ "r_customwidth", "r_width" },
	{ "r_customheight", "r_height" },
	{ "r_overBrightBits", "r_brightness" },
	{ "r_mapOverBrightBits", "r_mapBrightness" }
};


static long Cvar_Hash( const char* s )
{
	long hash = 0;

	for (int i = 0; s[i]; ++i) {
		hash += (long)(tolower(s[i])) * (i+119);
	}

	return (hash & (CVAR_HASH_SIZE-1));
}


static qbool Cvar_ValidateString( const char *s )
{
	if ( !s ) {
		return qfalse;
	}
	if ( strchr( s, '\\' ) ) {
		return qfalse;
	}
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}


cvar_t* Cvar_FindVar( const char* var_name )
{
	long hash = Cvar_Hash(var_name);

	for (cvar_t* var = hashTable[hash]; var; var = var->hashNext) {
		if (!Q_stricmp(var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}


float Cvar_VariableValue( const char *var_name )
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return 0;
	return var->value;
}


int Cvar_VariableIntegerValue( const char *var_name )
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return 0;
	return var->integer;
}


const char *Cvar_VariableString( const char *var_name )
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var)
		return "";
	return var->string;
}


void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize )
{
	const cvar_t* var = Cvar_FindVar(var_name);
	if (!var) {
		*buffer = 0;
		return;
	}
	Q_strncpyz( buffer, var->string, bufsize );
}


int Cvar_Flags( const char *var_name )
{
	const cvar_t* var;

	if (!(var = Cvar_FindVar(var_name)))
		return CVAR_NONEXISTENT;

	return var->flags;
}


cvarType_t Cvar_Type( const char *var_name )
{
	const cvar_t* var;

	if (!(var = Cvar_FindVar(var_name)))
		return CVART_STRING;

	return var->type;
}


void Cvar_CommandCompletion( void(*callback)(const char *s) )
{
	for (const cvar_t* cvar = cvar_vars; cvar; cvar = cvar->next) {
		if ( !(cvar->flags & CVAR_CHEAT) || cvar_cheats->integer ) {
			callback( cvar->name );
		}
	}
}


void Cvar_EnumHelp( search_callback_t callback, const char* pattern )
{
	if( pattern == NULL || *pattern == '\0' )
		return;

	cvar_t* cvar;
	for ( cvar = cvar_vars; cvar; cvar = cvar->next ) {
		if( cvar->name && !(cvar->flags & CVAR_USER_CREATED) )
			callback( cvar->name, cvar->desc, cvar->help, pattern, qtrue );
	}
}


static qbool IsHexChar( char c )
{
	return
		( c >= 'a' && c <= 'f' ) ||
		( c >= 'A' && c <= 'F' ) ||
		( c >= '0' && c <= '9' );
}


static qbool IsCPMAColorCode( char c )
{
	return
		( c >= 'a' && c <= 'z' ) ||
		( c >= 'A' && c <= 'Z' ) ||
		( c >= '0' && c <= '9' );
}


static qbool Cvar_IsValidValue( cvar_t *var, const char *value, qboolean printWarnings )
{
#define ERROR( Message , ...)	{ if ( printWarnings ) Com_Printf( "^3%s: " Message "\n", var->name, ## __VA_ARGS__ ); return qfalse; }
#define WARNING( Message, ... )	{ if ( printWarnings ) Com_Printf( "^3%s: " Message "\n", var->name, ## __VA_ARGS__ ); }

	if ( var->type == CVART_STRING )
		return qtrue;

	if ( var->type == CVART_FLOAT ) {
		float f;
		if ( sscanf(value, "%f", &f) != 1 || !isfinite(f) )
			ERROR( "not a finite floating-point value" )
		if( f < var->validator.f.min )
			ERROR( "float value too low" )
		if( f > var->validator.f.max )
			ERROR( "float value too high" )
	} else if ( var->type == CVART_INTEGER || var->type == CVART_BITMASK ) {
		int i;
		if ( sscanf(value, "%d", &i) != 1 )
			ERROR( "not a whole number (integer)" )
		if( i < var->validator.i.min )
			ERROR( "integer value too low" )
		if( i > var->validator.i.max )
			ERROR( "integer value too high" )
	} else if ( var->type == CVART_BOOL ) {
		if ( strlen(value) != 1 )
			ERROR( "must be a single char" );
		if ( value[0] != '0' && value[0] != '1' )
			ERROR( "must be 0 or 1" );
	} else if ( var->type == CVART_COLOR_RGB ) {
		if ( strlen(value) != 6 )
			ERROR( "must be 6 hex chars" );
		for ( int i = 0; i < 6; ++i ) {
			if ( !IsHexChar(value[i]) )
				ERROR( "must be 6 hex chars" );
		}
	} else if ( var->type == CVART_COLOR_RGBA ) {
		if ( strlen(value) != 8 )
			ERROR( "must be 8 hex chars" );
		for ( int i = 0; i < 8; ++i ) {
			if ( !IsHexChar(value[i]) )
				ERROR( "must be 8 hex chars" );
		}
	} else if ( var->type == CVART_COLOR_CPMA ) {
		if ( strlen(value) != 1 )
			ERROR( "must be a single char" );
		if ( !IsCPMAColorCode(value[0]) )
			ERROR( "invalid color code, must be [a-zA-Z0-9]" );
	} else if ( var->type == CVART_COLOR_CPMA_E ) {
		if ( value[0] != '\0' && !IsCPMAColorCode(value[0]) )
			ERROR( "invalid color code, must be [a-zA-Z0-9] or empty" );
	} else if ( var->type == CVART_COLOR_CHBLS ) {
		const char* const names[] = { "rail core", "head", "body", "legs", "rail spiral" };
		const int count = strlen(value);
		if ( count != 5 )
			WARNING( "should be 5 color codes [a-zA-Z0-9]" );
		for ( int i = 0, end = min(count, 5); i < end; ++i ) {
			if ( !IsCPMAColorCode(value[i]) )
				WARNING( "color code #%d (%s) is invalid, white will be used", i + 1, names[i] );
		}
		for ( int i = count; i < 5; ++i ) {
			WARNING( "color code #%d (%s) is missing, white will be used", i + 1, names[i] );
		}
	} else {
		Q_assert( !"Unsupported CVar type" );
	}

	return qtrue;

#undef ERROR
#undef WARNING
}


static void Cvar_PrintDeprecationWarning( int i )
{
	Com_Printf( "^3WARNING: " S_COLOR_CVAR "%s^7 was replaced by " S_COLOR_CVAR "%s\n",
			   cvar_depVars[i].name, cvar_depVars[i].newName );
}


static qbool Cvar_IsCreationAllowed( const char *var_name )
{
	for ( int i = 0; i < ARRAY_LEN(cvar_depVars); ++i ) {
		if ( Q_stricmp(var_name, cvar_depVars[i].name) )
			continue;

		cvar_depVars[i].warned = qtrue;
		if ( cvar_canPrintWarnings )
			Cvar_PrintDeprecationWarning( i );
		return qfalse;
	}

	return qtrue;
}


void Cvar_PrintDeprecationWarnings()
{
	cvar_canPrintWarnings = qtrue;

	for ( int i = 0; i < ARRAY_LEN(cvar_depVars); ++i ) {
		if ( cvar_depVars[i].warned )
			Cvar_PrintDeprecationWarning( i );
	}
}


cvar_t* Cvar_Set2( const char *var_name, const char *value, int cvarSetFlags )
{
	const qbool force = (cvarSetFlags & CVARSET_BYPASSLATCH_BIT) != 0;

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf( "invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

	cvar_t* var = Cvar_FindVar(var_name);
	if (!var) {
		if ( !value )
			return NULL;
		if( !Cvar_IsCreationAllowed(var_name) )
			return NULL;
		// create it
		return Cvar_Get( var_name, value, force ? 0 : CVAR_USER_CREATED );
	}

	if (!value) {
		value = var->resetString;
	}

	const qbool okValue = Cvar_IsValidValue(var, value, qtrue);
	if (okValue && !strcmp(value, var->string)) {
		if (var->latchedString) {
			Z_Free(var->latchedString);
			var->latchedString = NULL;
		}
		return var;
	}

	if (!okValue) {
		if (!Cvar_IsValidValue(var, var->resetString, qfalse))
			return var; // the default value is invalid too!
		value = var->resetString;
	}

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	if (!force)
	{
		if (var->flags & CVAR_ROM)
		{
			Com_Printf( "%s is read only.\n", var_name );
			return var;
		}

		if (var->flags & CVAR_INIT)
		{
			Com_Printf( "%s is write protected.\n", var_name );
			return var;
		}

		if ( (var->flags & CVAR_CHEAT) && !cvar_cheats->integer )
		{
			Com_Printf( "%s is cheat protected.\n", var_name );
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latchedString)
			{
				if (strcmp(value, var->latchedString) == 0)
					return var;
				Z_Free( var->latchedString );
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			Com_Printf( S_COLOR_CVAR "%s ^7will be changed upon restarting.\n", var_name );
			var->latchedString = CopyString(value);
			var->modified = qtrue;
			var->modificationCount++;
			return var;
		}

	}
	else
	{
		if (var->latchedString)
		{
			Z_Free( var->latchedString );
			var->latchedString = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = qtrue;
	var->modificationCount++;

	Z_Free( var->string );	// free the old value string

	var->string = CopyString(value);
	var->value = atof( var->string );
	var->integer = atoi( var->string );

	return var;
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t* Cvar_Get( const char *var_name, const char *var_value, int flags )
{
	if ( !var_name || !var_value ) {
		Com_Error( ERR_FATAL, "Cvar_Get: NULL parameter" );
	}

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

	cvar_t* var = Cvar_FindVar( var_name );
	if ( var ) {
		var_value = Cvar_IsValidValue( var, var_value, qfalse ) ? var_value : var->resetString;

		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value
		if ( ( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED )
			&& var_value[0] ) {
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );
			// needs to be set so that cvars the game tags as SERVERINFO get sent to clients
			cvar_modifiedFlags |= flags;
		}

		const qbool cmdLineCreated = (var->flags & CVAR_CMDLINE_CREATED) != 0;
		var->flags |= flags;
		var->flags &= ~CVAR_CMDLINE_CREATED;

		// only allow one non-empty reset string without a warning
		// KHB 071110  no, that's wrong for several reasons, notably vm changes caused by pure
		if ((flags & CVAR_ROM) || !var->resetString[0]) {
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );
		} else if ( com_developer && com_developer->integer && !var->mismatchPrinted && 
				    var_value[0] && strcmp( var->resetString, var_value ) ) {
			Com_DPrintf( "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
				var_name, var->resetString, var_value );
			var->mismatchPrinted = qtrue;
		}

		// if we have a latched string, take that value now
		if ( var->latchedString ) {
			char* s = var->latchedString;
			var->latchedString = NULL;	// otherwise cvar_set2 would free it
			Cvar_Set2( var_name, s, CVARSET_BYPASSLATCH_BIT );
			Z_Free( s );
		}

		// CVAR_INIT doesn't allow CVar registration to override the command-line value
		// (even if CVAR_ROM is also set)
		if (cmdLineCreated && (flags & CVAR_INIT) != 0) {
			return var;
		}

/* KHB  note that this is #if 0'd in the 132 code, but is actually REQUIRED for correctness
	consider a cgame that sets a CVAR_ROM client version:
	you connect to a v1 server, load the v1 cgame, and set the ROM version to v1
	you then connect to a v2 server and correctly load the v2 cgame, but
	when that registers its GENUINELY "NEW" version var, the value is ignored
	so now you have a CVAR_ROM with the wrong value in it. gg.
i'm preserving this incorrect behavior FOR NOW for compatability, because
game\ai_main.c(1352): trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
breaks every single mod except CPMA otherwise, but it IS wrong, and critically so
myT: we don't care about other mods and keeping it broken is not acceptable at all
*/
		// CVAR_ROM always overrides
		if (flags & CVAR_ROM) {
			Cvar_Set2( var_name, var_value, CVARSET_BYPASSLATCH_BIT );
		}

		return var;
	}

	//
	// allocate a new cvar
	//
	if ( cvar_numIndexes >= MAX_CVARS ) {
		Com_Error( ERR_FATAL, "MAX_CVARS" );
	}
	for ( int i = 0; i < MAX_CVARS; ++i ) {
		cvar_t* const temp = &cvar_indexes[ (cvar_numIndexes + i) % MAX_CVARS ];
		if ( temp->name == NULL ) {
			var = temp;
			break;
		}
	}
	if ( var == NULL ) {
		Com_Error( ERR_FATAL, "no free CVar found" );
	}
	cvar_numIndexes++;
	var->name = CopyString( var_name );
	var->string = CopyString( var_value );
	var->modified = qtrue;
	var->modificationCount = 1;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->resetString = CopyString( var_value );
	var->desc = NULL;
	var->help = NULL;
	var->type = CVART_STRING;
	var->firstModule = MODULE_NONE;
	var->moduleMask = 0;

	// link the variable in
	if ( cvar_vars == NULL || Q_stricmp(cvar_vars->name, var_name) > 0 ) {
		// insert as the first cvar
		cvar_t* const next = cvar_vars;
		cvar_vars = var;
		var->next = next;
	} else {
		// insert after some other cvar
		cvar_t* curr = cvar_vars;
		cvar_t* prev = cvar_vars;
		for (;;) {
			if ( Q_stricmp(curr->name, var_name) > 0 )
				break;

			prev = curr;
			if ( curr->next == NULL )
				break;

			curr = curr->next;
		}
		cvar_t* const next = prev->next;
		prev->next = var;
		var->next = next;
	}

	var->flags = flags;
	cvar_modifiedFlags |= flags; // needed so USERINFO cvars created by cgame actually get sent

	const long hash = Cvar_Hash( var_name );
	var->hashNext = hashTable[hash];
	hashTable[hash] = var;

	return var;
}


void Cvar_SetHelp( const char *var_name, const char *help )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	Help_AllocSplitText( &var->desc, &var->help, help );
}


qbool Cvar_GetHelp( const char **desc, const char **help, const char* var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
	{
		*desc = NULL;
		*help = NULL;
		return qfalse;
	}

	*desc = var->desc;
	*help = var->help;

	return qtrue;
}


void Cvar_SetRange( const char *var_name, cvarType_t type, const char *minStr, const char *maxStr )
{
#define ERROR( Message ) { assert(0); Com_Printf( "^3Cvar_SetRange on %s: " Message "\n", var_name ); return; }

	cvar_t* var = Cvar_FindVar( var_name );
	if( !var )
		ERROR( "cvar not found" );

	if( (unsigned int)type >= CVART_COUNT )
		ERROR( "invalid cvar type" );

	if ( type == CVART_BOOL ) {
		var->validator.i.min = 0;
		var->validator.i.max = 1;
	} else if ( type == CVART_INTEGER || type == CVART_BITMASK ) {
		int min = INT_MIN;
		int max = INT_MAX;
		if ( minStr && sscanf(minStr, "%d", &min) != 1 )
			ERROR( "invalid min value" )
		if ( maxStr && sscanf(maxStr, "%d", &max) != 1 )
			ERROR( "invalid max value" )
		if ( min > max )
			ERROR( "min greater than max" )
		var->validator.i.min = min;
		var->validator.i.max = max;
	} else if ( type == CVART_FLOAT ) {
		// yes, sscanf *does* recognize special strings for inf and NaN
		float min = -FLT_MAX;
		float max =  FLT_MAX;
		if ( minStr && sscanf(minStr, "%f", &min) != 1 && !isfinite(min) )
			ERROR( "invalid min value" )
		if ( maxStr && sscanf(maxStr, "%f", &max) != 1 && !isfinite(max) )
			ERROR( "invalid max value" )
		if ( min > max )
			ERROR( "min greater than max" )
		var->validator.f.min = min;
		var->validator.f.max = max;
	}
	var->type = type;

	// run a validation pass right away
	Cvar_Set( var_name, var->string );

#undef ERROR
}


void Cvar_SetDataType( const char* cvarName, cvarType_t type )
{
	cvar_t* const cvar = Cvar_FindVar( cvarName );
	if ( cvar == NULL )
		return;

#if defined(_DEBUG)
	if ( cvar->type == CVART_STRING ) {
		Q_assert(
			type == CVART_STRING ||
			type == CVART_COLOR_CPMA ||
			type == CVART_COLOR_CPMA_E ||
			type == CVART_COLOR_CHBLS ||
			type == CVART_COLOR_RGB ||
			type == CVART_COLOR_RGBA );
	} else {
		Q_assert( type == cvar->type );
	}
#endif
	cvar->type = type;
}


void Cvar_SetMenuData( const char* cvarName, int categories, const char* title, const char* desc, const char* help, const char* values )
{
	cvar_t* const cvar = Cvar_FindVar( cvarName );
	if ( cvar == NULL )
		return;

	cvarGui_t* const gui = &cvar->gui;
	gui->categories = categories;
	gui->title = title != NULL ? CopyString( title ) : NULL;
	gui->desc = desc != NULL ? CopyString( desc ) : NULL;
	gui->help = help != NULL ? CopyString( help ) : NULL;

	if ( values == NULL )
		return;

	const char* const allStrings = values;
	const char* string = NULL;
	int maxLength = 0;
	int numValues = 0;

	string = allStrings;
	for ( ; string[0] != '\0'; numValues++ ) {
		int length = strlen( string );
		maxLength = max( maxLength, length );
		string += length + 1;
		length = strlen( string );
		string += length + 1;
		length = strlen( string );
		string += length + 1;
	}

#if defined(_DEBUG)
	// make sure we have 1 set of strings for each possible value
	if ( cvar->type == CVART_INTEGER ) {
		Q_assert( numValues == cvar->validator.i.max - cvar->validator.i.min + 1 );
	} else if ( cvar->type == CVART_BITMASK ) {
		int numBits = 0;
		int test = cvar->validator.i.max + 1;
		while ( test >>= 1 ) {
			numBits++;
		}
		Q_assert( numValues == numBits );
	}
#endif

	gui->maxValueLength = maxLength;
	gui->numValues = numValues;
	gui->values = (cvarGuiValue_t*)S_Malloc( numValues * (int)sizeof( cvarGuiValue_t ) );

	string = allStrings;
	for ( int i = 0; string[0] != '\0'; i++ ) {
		int length = strlen( string );
		gui->values[i].valueLength = length;
		gui->values[i].value = CopyString( string );
		string += length + 1;
		length = strlen( string );
		gui->values[i].title = CopyString( string );
		string += length + 1;
		length = strlen( string );
		gui->values[i].desc = CopyString( string );
		string += length + 1;
	}
}


void Cvar_RegisterTable( const cvarTableItem_t* cvars, int count, module_t module )
{
	for ( int i = 0; i < count; ++i ) {
		const cvarTableItem_t* item = &cvars[i];

		if ( item->cvar )
			*item->cvar = Cvar_Get( item->name, item->reset, item->flags );
		else
			Cvar_Get( item->name, item->reset, item->flags );

		if ( item->help )
			Cvar_SetHelp( item->name, item->help );

		if ( item->min ||
			item->max ||
			item->type != CVART_STRING )
			Cvar_SetRange( item->name, item->type, item->min, item->max );

		Cvar_SetMenuData( item->name, item->categories, item->guiName, item->guiDesc, item->guiHelp, item->guiValues );

		Cvar_SetModule( item->name, module );
	}
}


void Cvar_SetModule( const char *var_name, module_t module )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	var->moduleMask |= 1 << (int)module;
	if ( var->firstModule == MODULE_NONE )
		var->firstModule = module;
}


void Cvar_GetModuleInfo( module_t *firstModule, int *moduleMask, const char *var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	*firstModule = var->firstModule;
	*moduleMask = var->moduleMask;
}


const char* Cvar_GetRegisteredName( const char *var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return NULL;

	return var->name;
}


static const char* Cvar_FormatRangeFloat( float vf )
{
	const int vi = (int)vf;
	const qbool round = vf == (float)vi;

	return round ? va( "%d.0", vi ) : va( "%.3g", vf );
}


void Cvar_PrintTypeAndRange( const char *var_name, printf_t print )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	if ( var->type == CVART_BOOL ) {
		print( S_COLOR_VAL "0^7|" S_COLOR_VAL "1" );
	} else if ( var->type == CVART_BITMASK ) {
		print( "bitmask" );
	} else if ( var->type == CVART_FLOAT ) {
		const float minV = var->validator.f.min;
		const float maxV = var->validator.f.max;
		if ( minV == -FLT_MAX && maxV == FLT_MAX ) {
			print( "float_value" );
		} else {
			const char* min = minV == -FLT_MAX ? "-inf" : Cvar_FormatRangeFloat( minV );
			const char* max = maxV == +FLT_MAX ? "+inf" : Cvar_FormatRangeFloat( maxV );
			print( S_COLOR_VAL "%s ^7to " S_COLOR_VAL "%s", min, max );
		}
	} else if ( var->type == CVART_INTEGER ) {
		const int minV = var->validator.i.min;
		const int maxV = var->validator.i.max;
		const int diff = maxV - minV;
		if( minV == INT_MIN && maxV == INT_MAX ) {
			print( "integer_value" );
		} else if ( diff == 0 ) {
			print( S_COLOR_VAL "%d", minV );
		} else if ( diff == 1 ) {
			print( S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d", minV, minV + 1 );
		} else if ( diff == 2 ) {
			print( S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d", minV, minV + 1, minV + 2 );
		} else if ( diff == 3 ) {
			print( S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d^7|" S_COLOR_VAL "%d", minV, minV + 1, minV + 2, minV + 3 );
		} else {
			const char* min = minV == INT_MIN ? "-inf" : va( "%d", minV );
			const char* max = maxV == INT_MAX ? "+inf" : va( "%d", maxV );
			print( S_COLOR_VAL "%s ^7to " S_COLOR_VAL "%s", min, max );
		}
	} else if ( var->type == CVART_COLOR_RGB ) {
		print( "RGB" );
	} else if ( var->type == CVART_COLOR_RGBA ) {
		print( "RGBA" );
	} else if ( var->type == CVART_COLOR_CPMA || var->type == CVART_COLOR_CPMA_E ) {
		print( "color code" );
	} else if ( var->type == CVART_COLOR_CHBLS ) {
		print( "CHBLS colors" );
	} else {
		print( "string" );
	}
}


void Cvar_PrintFirstHelpLine( const char *var_name, printf_t print )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	const char* const q = var->type == CVART_STRING ? "\"" : "";
	print( S_COLOR_CVAR "%s ^7<", var_name );
	Cvar_PrintTypeAndRange( var_name, print );
	print( "> (default: %s" S_COLOR_VAL "%s^7%s)\n", q, var->resetString, q );
}


void Cvar_PrintFlags( const char *var_name, printf_t print )
{
	cvar_t* var = Cvar_FindVar( var_name );
	if ( !var )
		return;

	static const char* names[] = {
		"Archived",
		"User Info",
		"Server Info",
		"System Info",
		"Init",
		"Latched",
		"Read-only",
		"User-created",
		"Temporary",
		"Cheat-protected",
		"No Reset",
		"Server-created"
	};

	const int flags = var->flags;
	int count = 0;
	for ( int i = 0; i < ARRAY_LEN(names); ++i ) {
		if ( (flags >> i) & 1 )
			++count;
	}

	print( count != 1 ? "Attributes: " : "Attribute: " );

	int printed = 0;
	for ( int i = 0; i < ARRAY_LEN(names); ++i ) {
		if ( !((flags >> i) & 1) )
			continue;

		if ( printed )
			print( ", " );
		print( names[i] );
		++printed;
	}

	if ( count )
		print( "\n" );
	else
		print( "None\n" );
}


void Cvar_Set( const char *var_name, const char *value )
{
	Cvar_Set2( var_name, value, CVARSET_BYPASSLATCH_BIT );
}


void Cvar_SetValue( const char *var_name, float value )
{
	if ( value == (int)value ) {
		Cvar_Set( var_name, va("%i", (int)value) );
	} else {
		Cvar_Set( var_name, va("%g", value) );
	}
}


void Cvar_Reset( const char *var_name )
{
	Cvar_Set2( var_name, NULL, 0 );
}


// any testing variables will be reset to the safe values

void Cvar_SetCheatState()
{
	for (cvar_t* var = cvar_vars; var; var = var->next) {
		if ( var->flags & CVAR_CHEAT ) {
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here
			// because of a different var->latchedString
			if (var->latchedString) {
				Z_Free(var->latchedString);
				var->latchedString = NULL;
			}
			if (strcmp(var->resetString,var->string)) {
				Cvar_Set( var->name, var->resetString );
			}
		}
	}
}


// handles variable inspection and changing from the console

qbool Cvar_Command()
{
	const cvar_t* v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return qfalse;

	// perform a variable print or set
	if ( Cmd_Argc() == 1 ) {
		const char* const q = v->type == CVART_STRING ? "\"" : "";
		Com_Printf( S_COLOR_CVAR "%s^7 is %s" S_COLOR_VAL "%s^7%s (", v->name, q, v->string, q );
		Cvar_PrintTypeAndRange( v->name, &Com_Printf );
		Com_Printf( "^7, default: %s" S_COLOR_VAL "%s^7%s)\n", q, v->resetString, q );
		if ( v->latchedString ) {
			Com_Printf( "latched: %s" S_COLOR_VAL "%s^7%s\n", q, v->latchedString, q );
		}
		return qtrue;
	}

	// set the value if forcing isn't required
	Cvar_Set2( v->name, Cmd_Args(), 0 );
	return qtrue;
}


static void Cvar_CompleteName( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteFrom( compArg, compArg, qfalse, qtrue );
}


// toggles a cvar for easy single key binding

static void Cvar_Toggle_f( void )
{
	const int argc = Cmd_Argc();
	if ( argc != 2 && argc < 4 ) {
		Com_Printf( "usage: toggle <variable> [<value1> <value2> [value3]..]\n" );
		return;
	}

	const char* const name = Cmd_Argv(1);
	const cvar_t* const cvar = Cvar_FindVar( name );
	if ( !cvar )
		return;

	if ( argc == 2 ) {
		const int v = !Cvar_VariableIntegerValue( name );
		Cvar_Set2( name, va("%i", v), 0 );
		return;
	}

	// set the next value - if none found, set the first value
	int index = -1;
	const int valueOffset = 2;
	const int valueCount = argc - valueOffset;
	for ( int i = 0; i < valueCount; ++i )
	{
		if ( !Q_stricmp(Cmd_Argv(i + valueOffset), cvar->string) ) {
			index = (i + 1) % valueCount;
			break;
		}
	}
	if ( index < 0 )
		index = 0;

	Cvar_Set2( name, Cmd_Argv(index + valueOffset), 0 );
}


// frees resources associated to a cvar, then clears it

static void Cvar_Nuke( cvar_t* var )
{
	// remove this cvar from the hash table so that
	// a) it doesn't show up as existing, and
	// b) it doesn't make other registered cvars not show up as existing
	//    (a zeroed cvar_t struct would cause an early exit in Cvar_FindVar)
	const long hash = Cvar_Hash( var->name );
	cvar_t** hashVarIter = &hashTable[hash];
	for ( ;; ) {
		cvar_t* const hashVar = *hashVarIter;
		if ( hashVar == NULL ) {
			Com_Printf( "ERROR: CVar '%s' not found in hash map\n", var->name );
			break;
		}
		if ( hashVar == var ) {
			*hashVarIter = var->hashNext;
			break;
		}
		hashVarIter = &hashVar->hashNext;
	}

	if ( var->name )
		Z_Free( var->name );

	if ( var->string )
		Z_Free( var->string );

	if ( var->latchedString )
		Z_Free( var->latchedString );

	if ( var->resetString )
		Z_Free( var->resetString );

	if ( var->desc )
		Z_Free( var->desc );

	if ( var->help )
		Z_Free( var->help );

	// clear the var completely
	// even though clearing the name alone would be enough
	Com_Memset( var, 0, sizeof( *var ) );

	// make sure this index can be allocated again
	cvar_numIndexes--;
}


// removes a user-created cvar

static void Cvar_Unset_f( void )
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: unset <variable>\n" );
		return;
	}

	const char* const name = Cmd_Argv(1);
	cvar_t** prev = &cvar_vars;
	while ( 1 ) {
		cvar_t* const var = *prev;
		if ( !var )
			break;

		if ( (var->flags & CVAR_USER_CREATED) != 0 && !Q_stricmp(var->name, name) ) {
			*prev = var->next;
			Cvar_Nuke( var );
			break;
		}

		prev = &var->next;
	}
}


// sets a cvar to an empty string

static void Cvar_SetEmpty_f( void )
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: setempty <string_variable>\n" );
		return;
	}

	const char* const name = Cmd_Argv(1);
	const cvar_t* const cvar = Cvar_FindVar( name );
	if ( !cvar || cvar->type != CVART_STRING )
		return;

	Cvar_Set( Cmd_Argv(1), "" );
}


static void Cvar_ExecuteOp( qbool multiply )
{
	if ( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: %s <cvar> <number>\n", Cmd_Argv(0) );
		return;
	}

	const cvar_t* const cvar = Cvar_FindVar( Cmd_Argv(1) );
	if ( !cvar ) {
		Com_Printf( "%s: variable '%s' not found\n", Cmd_Argv(0), Cmd_Argv(1) );
		return;
	}

	if ( cvar->type != CVART_INTEGER && cvar->type != CVART_FLOAT ) {
		Com_Printf( "%s: variable " S_COLOR_CVAR "%s ^7isn't an integer or float variable\n", Cmd_Argv(0), Cmd_Argv(1) );
		if ( cvar->type == CVART_BOOL )
			Com_Printf( "%s: you can use '/" S_COLOR_CMD "toggle " S_COLOR_CVAR "%s^7' to toggle between 0 and 1\n", Cmd_Argv(0), Cmd_Argv(1) );
		return;
	}

	if ( cvar->type == CVART_INTEGER ) {
		int iTemp;
		if ( sscanf( Cmd_Argv(2), "%d", &iTemp ) != 1 ) {
			Com_Printf( "%s: '%s' isn't an integer (whole number)\n", Cmd_Argv(0), Cmd_Argv(2) );
			return;
		}

		if ( (multiply && iTemp == 1) || (!multiply && iTemp == 0) )
			return;

		const int iValU = multiply ? ( cvar->integer * iTemp ) : ( cvar->integer + iTemp );
		const int iValC = Com_ClampInt( cvar->validator.i.min, cvar->validator.i.max, iValU );
		Cvar_Set( Cmd_Argv(1), va("%d", iValC) );
	} else {
		float fTemp;
		if ( sscanf( Cmd_Argv(2), "%f", &fTemp ) != 1 || !isfinite( fTemp ) ) {
			Com_Printf( "%s: '%s' isn't a finite floating-point number\n", Cmd_Argv(0), Cmd_Argv(2) );
			return;
		}

		if ( (multiply && fTemp == 1.0f) || (!multiply && fTemp == 0.0f) )
			return;

		const float fValU = multiply ? ( cvar->value * fTemp ) : ( cvar->value + fTemp );
		const float fValC = Com_Clamp( cvar->validator.f.min, cvar->validator.f.max, fValU );
		Cvar_Set( Cmd_Argv(1), va("%g", fValC) );
	}
}


static void Cvar_Add_f( void )
{
	Cvar_ExecuteOp( qfalse );
}


static void Cvar_Multiply_f( void )
{
	Cvar_ExecuteOp( qtrue );
}


// Allows setting and defining of arbitrary cvars from console
// even if they weren't declared in C code

static void Cvar_Set_f( void )
{
	int c = Cmd_Argc();
	if ( c < 3 ) {
		Com_Printf ("usage: set <variable> <value>\n");
		return;
	}

	char combined[MAX_STRING_TOKENS];
	combined[0] = 0;
	int l = 0;
	for (int i = 2; i < c; i++) {
		int len = strlen ( Cmd_Argv( i ) ) + 1;
		if ( l + len >= MAX_STRING_TOKENS - 2 ) {
			break;
		}
		strcat( combined, Cmd_Argv( i ) );
		if ( i != c-1 ) {
			strcat( combined, " " );
		}
		l += len;
	}

	Cvar_Set2( Cmd_Argv(1), combined, 0 );
}


static void Cvar_SetAndFlag( const char* cmd, int flag )
{
	if ( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: %s <variable> <value>\n", cmd );
		return;
	}

	Cvar_Set_f();
	cvar_t* v = Cvar_FindVar( Cmd_Argv(1) );
	if ( !v ) {
		Com_DPrintf( "Warning: couldn't find cvar %s\n", Cmd_Argv(1) );
		return;
	}

	v->flags |= flag;
}


static void Cvar_SetU_f( void )
{
	Cvar_SetAndFlag( "setu", CVAR_USERINFO );
}


static void Cvar_SetS_f( void )
{
	Cvar_SetAndFlag( "sets", CVAR_SERVERINFO );
}


static void Cvar_SetA_f( void )
{
	Cvar_SetAndFlag( "seta", CVAR_ARCHIVE );
}


static void Cvar_Reset_f( void )
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}


// appends lines containing "seta variable value" for all cvars with the archive flag set

void Cvar_WriteVariables( fileHandle_t f, qbool forceWrite )
{
	for (const cvar_t* var = cvar_vars; var; var = var->next) {
		if (Q_stricmp( var->name, "cl_cdkey" ) == 0)
			continue;
		if (!forceWrite && !(var->flags & CVAR_ARCHIVE))
			continue;
		// write the latched value, even if it hasn't taken effect yet
		FS_Printf( f, "seta %s \"%s\"\n", var->name, var->latchedString ? var->latchedString : var->string );
	}
}


static void Cvar_List_f( void )
{
	const char* match = (Cmd_Argc() > 1) ? Cmd_Argv(1) : NULL;

	int i = 0;
	int found = 0;
	for (const cvar_t* var = cvar_vars; var; var = var->next, ++i)
	{
		if (match && !Com_Filter(match, var->name))
			continue;

		++found;

		Com_Printf( (var->flags & CVAR_SERVERINFO) ? "S" : " " );
		Com_Printf( (var->flags & CVAR_USERINFO) ? "U" : " " );
		Com_Printf( (var->flags & CVAR_ROM) ? "R" : " " );
		Com_Printf( (var->flags & CVAR_INIT) ? "I" : " " );
		Com_Printf( (var->flags & CVAR_ARCHIVE) ? "A" : " " );
		Com_Printf( (var->flags & CVAR_LATCH) ? "L" : " " );
		Com_Printf( (var->flags & CVAR_CHEAT) ? "C" : " " );
		Com_Printf( (var->flags & CVAR_USER_CREATED) ? "?" : (var->help ? "h" : " ") );

		Com_Printf(" %s \"%s\"\n", var->name, var->string);
	}

	Com_Printf("\n%4i cvars total\n", i);
	Com_Printf("%4i cvar%s matched\n", found, found > 1 ? "s" : "");
}


static void Cvar_Restart( qbool reset )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;
	while ( 1 ) {
		var = *prev;
		if ( !var ) {
			break;
		}

		// don't mess with rom values, or some inter-module
		// communication will get broken (com_cl_running, etc)
		if ( var->flags & ( CVAR_ROM | CVAR_INIT | CVAR_NORESTART ) ) {
			prev = &var->next;
			continue;
		}

		// throw out any variables the user created
		if ( var->flags & CVAR_USER_CREATED ) {
			*prev = var->next;
			Cvar_Nuke( var );
			continue;
		}

		if ( reset ) {
			Cvar_Set( var->name, var->resetString );
		}

		prev = &var->next;
	}
}


// removes all user-created cvars
// resets all cvars to their hardcoded values

static void Cvar_Restart_f( void )
{
	Cvar_Restart( qtrue );
}


// removes all user-created cvars
// this will only accept to run when both the server and client are running unless forced

static void Cvar_Trim_f()
{
	if ( Cmd_Argc() == 2 && !Q_stricmp( Cmd_Argv(1), "-f" ) ) {
		Cvar_Restart( qfalse );
		return;
	}

	if (com_cl_running && com_cl_running->integer &&
		com_sv_running && com_sv_running->integer ) {
		Cvar_Restart( qfalse );
		return;
	}

	Com_Printf( "You're not running a listen server, so not all subsystems/VMs are loaded.\n" );
	Com_Printf( "This means you'd remove cvars that are probably best kept around.\n" );
	Com_Printf( "If you don't care, you can force the call by running '/%s -f'.\n", Cmd_Argv(0) );
	Com_Printf( "You've been warned.\n" );
}


const char* Cvar_InfoString( int bit )
{
	static char info[MAX_INFO_STRING];

	info[0] = 0;

	for ( const cvar_t* var = cvar_vars ; var ; var = var->next ) {
		if (var->flags & bit) {
			Info_SetValueForKey( info, var->name, var->string );
		}
	}

	return info;
}


// special version for very large infostrings, ie CS_SYSTEMINFO

const char* Cvar_InfoString_Big( int bit )
{
	static char info[BIG_INFO_STRING];

	info[0] = 0;

	for ( const cvar_t* var = cvar_vars ; var ; var = var->next ) {
		if (var->flags & bit) {
			Info_SetValueForKey_Big( info, var->name, var->string );
		}
	}

	return info;
}


// pointless function solely for the UI VM, which never uses it
// but since it's not TECHNICALLY defective we'll keep it for now

void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize )
{
	Q_strncpyz( buff, Cvar_InfoString(bit), buffsize );
}


// basically a slightly modified Cvar_Get for the interpreted modules

void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags )
{
	cvar_t* cv = Cvar_Get( varName, defaultValue, flags );

	if ( (flags & CVAR_SERVERINFO) && !Q_stricmp(varName, "gamename") )
		Crash_SaveModName( defaultValue );
	else if ( (flags & CVAR_SERVERINFO) && !Q_stricmp(varName, "gameversion") )
		Crash_SaveModVersion( defaultValue );
	else
		Crash_SaveQVMGitString( varName, defaultValue );

	if ( !vmCvar )
		return;

	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;
	Cvar_Update( vmCvar );
}


// update an interpreted module's version of a cvar

void Cvar_Update( vmCvar_t *vmCvar )
{
	cvar_t* cv = NULL;
	assert(vmCvar);

	if ( (unsigned int)vmCvar->handle >= MAX_CVARS ) {
		Com_Error( ERR_DROP, "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;		// variable might have been cleared by a cvar_restart
	}
	vmCvar->modificationCount = cv->modificationCount;

	if ( strlen(cv->string)+1 > MAX_CVAR_VALUE_STRING )
			Com_Error( ERR_DROP, "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING",
			cv->string, strlen(cv->string) );

	Q_strncpyz( vmCvar->string, cv->string,  MAX_CVAR_VALUE_STRING );
	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}


static const cmdTableItem_t cl_cmds[] =
{
	{ "toggle", Cvar_Toggle_f, Cvar_CompleteName, help_toggle },
	{ "set", Cvar_Set_f, Cvar_CompleteName, "creates or changes a cvar's value" },
	{ "sets", Cvar_SetS_f, Cvar_CompleteName, "like /set with the server info flag" },
	{ "setu", Cvar_SetU_f, Cvar_CompleteName, "like /set with the user info flag" },
	{ "seta", Cvar_SetA_f, Cvar_CompleteName, "like /set with the archive flag" },
	{ "reset", Cvar_Reset_f, Cvar_CompleteName, "sets a cvar back to its default value" },
	{ "unset", Cvar_Unset_f, Cvar_CompleteName, "removes a user-created cvar" },
	{ "setempty", Cvar_SetEmpty_f, Cvar_CompleteName, "sets a cvar to an empty string" },
	{ "cvar_add", Cvar_Add_f, Cvar_CompleteName, "adds a number to a cvar" },
	{ "cvar_mul", Cvar_Multiply_f, Cvar_CompleteName, "multiplies a cvar by a number" },
	{ "cvarlist", Cvar_List_f, NULL, help_cvarlist },
	{ "cvar_restart", Cvar_Restart_f, NULL, "restarts the cvar system" },
	{ "cvar_trim", Cvar_Trim_f, NULL, "removes user-created cvars" }
};


void Cvar_Init()
{
	// this cvar is the single entry point of the entire extension system
	Cvar_Get( "//trap_GetValue", "700", CVAR_INIT | CVAR_ROM );

	cvar_cheats = Cvar_Get( "sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO );
	Cvar_Get( "git_branch", com_gitBranch, CVAR_ROM );
	Cvar_Get( "git_headHash", com_gitCommit, CVAR_ROM );

	Cmd_RegisterArray( cl_cmds, MODULE_COMMON );
}


cvar_t* Cvar_GetFirst()
{
	return cvar_vars;
}
