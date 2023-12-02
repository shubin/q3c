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

#include <hash_map>

struct VanillaStringCmp
{
	enum { bucket_size = 4, min_buckets = 8 }; // parameters for VC hash table

	size_t operator()( const char* s ) const // X31 hash
	{
		size_t h = 0;
		const unsigned char* p = (const unsigned char*)s;
		while (*p)
			h = (h << 5) - h + (*p++);
		return h;
	}

	bool operator()( const char* lhs, const char* rhs ) const
	{
		return (strcmp(lhs, rhs) < 0);
	}
};


#include "../../qcommon/vm_local.h"

extern "C" {
#include "cmdlib.h"
};


typedef stdext::hash_map< const char*, int, VanillaStringCmp > OpTable;
static OpTable aOpTable;


static char outputFilename[MAX_OSPATH];

typedef enum {
	CODESEG,
	DATASEG,	// initialized 32 bit data, will be byte swapped
	LITSEG,		// strings
	BSSSEG,		// 0 filled
	JTRGSEG,	// pseudo-segment that contains only jump table targets
	NUM_SEGMENTS
} segmentName_t;

#define MAX_SEGSIZE 0x400000

struct segment_t {
	const char* name;
	byte	image[MAX_SEGSIZE];
	int		imageUsed;
	int		segmentBase;		// only valid on second pass
};

static segment_t segment[NUM_SEGMENTS];
static segment_t* currentSegment;


struct symbol_t {
	const segment_t* segment;
	int value;
};

typedef stdext::hash_map< const char*, symbol_t*, VanillaStringCmp > SymTable;
static SymTable aSymGlobal;
static SymTable aSymImported;

static symbol_t* lastSymbol; // symbol most recently defined, used by HackToSegment


struct options_t {
	qboolean verbose;
	qboolean writeMapFile;
};

static options_t options;


#define	MAX_ASM_FILES	256
int		numAsmFiles;
char	*asmFiles[MAX_ASM_FILES];
char	*asmFileNames[MAX_ASM_FILES];

static int currentFileIndex;
static const char* currentFileName;
static int currentFileLine;

static SymTable aSymLocal[MAX_ASM_FILES];


// we need to convert arg and ret instructions to
// stores to the local stack frame, so we need to track the
// characteristics of the current function's stack frame
int		currentLocals;			// bytes of locals needed by this function
int		currentArgs;			// bytes of largest argument list called from this function
int		currentArgOffset;		// byte offset in currentArgs to store next arg, reset each call

int		passNumber;
int		instructionCount;


void QDECL Com_Error( int level, const char* error, ... )
{
	va_list va;
	va_start( va, error );
	vprintf( error, va );
	va_end( va );
	exit( level );
}

void QDECL Com_Printf( const char* fmt, ... ) {}


#ifdef _MSC_VER
#define INT64 __int64
#define atoi64 _atoi64
#else
#define INT64 long long int
#define atoi64 atoll
#endif

/*
	BYTE values are specified as signed decimal strings
	A properly functional atoi() will cap large signed values at 0x7FFFFFFF
	Negative word values are often specified by LCC as very large decimal values
	so values that should be between 0x7FFFFFFF and 0xFFFFFFFF come out as 0x7FFFFFFF

	This function is a trivial (tho clever) hack to work around this problem
*/
static int atoiNoCap( const char* s )
{
	INT64 n = atoi64(s);
	return (n < 0) ? (int)n : (unsigned)n;
}


static void report( const char* fmt, ... )
{
	if (!options.verbose)
		return;

	va_list va;
	va_start( va, fmt );
	vprintf( fmt, va );
	va_end(va);
}


static int errorCount;

static void CodeError( const char* fmt, ... )
{
	errorCount++;
	printf( "%s:%i : ", currentFileName, currentFileLine );

	va_list va;
	va_start( va, fmt );
	vprintf( fmt, va );
	va_end( va );
}


static void EmitByte( segment_t* seg, int v )
{
	if ( seg->imageUsed >= MAX_SEGSIZE ) {
		Com_Error( ERR_FATAL, "MAX_SEGSIZE" );
	}
	seg->image[ seg->imageUsed ] = v;
	seg->imageUsed++;
}


static void EmitInt( segment_t* seg, int v )
{
	if ( seg->imageUsed >= MAX_SEGSIZE - 4 ) {
		Com_Error( ERR_FATAL, "MAX_SEGSIZE" );
	}
	seg->image[ seg->imageUsed ] = v & 255;
	seg->image[ seg->imageUsed + 1 ] = ( v >> 8 ) & 255;
	seg->image[ seg->imageUsed + 2 ] = ( v >> 16 ) & 255;
	seg->image[ seg->imageUsed + 3 ] = ( v >> 24 ) & 255;
	seg->imageUsed += 4;
}


static void ExportSymbol( const char* symbol )
{
	// symbols can only be defined on pass 0
	if ( passNumber == 1 )
		return;

	SymTable::const_iterator it = aSymGlobal.find( symbol );
	if (it != aSymGlobal.end())
		return;

	const char* name = copystring( symbol );
	symbol_t* s = new symbol_t;
	s->value = 0;
	aSymGlobal[ name ] = s;
}


static void ImportSymbol( const char* symbol )
{
	// symbols can only be defined on pass 0
	if ( passNumber == 1 )
		return;

	SymTable::const_iterator it = aSymImported.find( symbol );
	if (it != aSymImported.end())
		return;

	const char* name = copystring( symbol );
	symbol_t* s = new symbol_t;
	aSymImported[ name ] = s;
}


static void DefineSymbol( const char* symbol, int value )
{
	// symbols can only be defined on pass 0
	if ( passNumber == 1 )
		return;

	const char* name = copystring( symbol );
	symbol_t* s = new symbol_t;
	s->segment = currentSegment;
	s->value = value;

	SymTable::iterator it;

	it = aSymGlobal.find( symbol );
	if (it != aSymGlobal.end()) {
		// we've encountered an "export blah" for this "proc blah"
		// so update the global's placeholder with the real data too
		delete (*it).second;
		(*it).second = s;
	}

	aSymLocal[currentFileIndex][ name ] = s;

	lastSymbol = s;
}


static int LookupSymbol( const char* symbol )
{
	// symbols can only be evaluated on pass 1
	if ( passNumber == 0 )
		return 0;

	SymTable::const_iterator it;

	it = aSymLocal[currentFileIndex].find( symbol );
	if (it != aSymLocal[currentFileIndex].end()) {
		const symbol_t* s = (*it).second;
		return (s->segment->segmentBase + s->value);
	}

	it = aSymImported.find( symbol );
	if (it == aSymImported.end()) {
		// no explicit prototype == incorrect code
		// whether the symbol is "reachable" or not
		CodeError( "Symbol %s undefined\n", symbol );
		return 0;
	}

	it = aSymGlobal.find( symbol );
	if (it == aSymGlobal.end()) {
		CodeError( "External symbol %s undefined\n", symbol );
		return 0;
	}

	const symbol_t* s = (*it).second;
	return (s->segment->segmentBase + s->value);
}


///////////////////////////////////////////////////////////////

// this stuff should probably be replaced with COM_Parse, but...


#define MAX_LINE_LENGTH 1024
static char lineBuffer[MAX_LINE_LENGTH];
static int lineParseOffset;
static char token[MAX_LINE_LENGTH];


/*
Extracts the next line from the given text block.
If a full line isn't parsed, returns NULL
Otherwise returns the updated parse pointer
*/
static const char* ExtractLine( const char* data )
{
	++currentFileLine;

	lineParseOffset = 0;
	token[0] = 0;

	if ( data[0] == 0 ) {
		lineBuffer[0] = 0;
		return NULL;
	}

	int i;
	for ( i = 0 ; i < MAX_LINE_LENGTH ; i++ ) {
		if ( data[i] == 0 || data[i] == '\n' ) {
			break;
		}
	}
	if ( i == MAX_LINE_LENGTH ) {
		CodeError( "MAX_LINE_LENGTH" );
		return data;
	}

	memcpy( lineBuffer, data, i );
	lineBuffer[i] = 0;
	data += i;

	if ( data[0] == '\n' ) {
		return ++data;
	}

	return data;
}


// parse a token out of lineBuffer

static qboolean GetToken()
{
	token[0] = 0;

	// skip whitespace
	while ( lineBuffer[ lineParseOffset ] <= ' ' ) {
		if ( lineBuffer[ lineParseOffset ] == 0 ) {
			return qfalse;
		}
		++lineParseOffset;
	}

	// skip ; comments
	int c = lineBuffer[ lineParseOffset ];
	if ( c == ';' ) {
		return qfalse;
	}

	// parse a regular word
	int len = 0;
	do {
		token[len++] = c;
		c = lineBuffer[ ++lineParseOffset ];
	} while (c > ' ');
	token[len] = 0;

	return qtrue;
}


static int ParseValue()
{
	GetToken();
	return atoiNoCap( token );
}


static int ParseExpression()
{
	// skip any leading minus
	int i = (token[0] == '-') ? 1 : 0;

	for ( ; i < MAX_LINE_LENGTH ; i++ ) {
		if ( token[i] == '+' || token[i] == '-' || token[i] == 0 ) {
			break;
		}
	}

	char s[MAX_LINE_LENGTH];
	memcpy( s, token, i );
	s[i] = 0;

	int v;
	if ( isdigit(s[0]) || (s[0] == '-') ) {
		v = atoiNoCap( s );
	} else {
		v = LookupSymbol( s );
	}

	// parse add / subtract offsets
	int j;
	while ( token[i] ) {
		for ( j = i + 1 ; j < MAX_LINE_LENGTH ; j++ ) {
			if ( token[j] == '+' || token[j] == '-' || token[j] == 0 ) {
				break;
			}
		}
		v += atoi( &token[i] );
		i = j;
	}

	return v;
}


///////////////////////////////////////////////////////////////


/*
BIG HACK: I want to put all 32 bit values in the data
segment so they can be byte swapped, and all char data in the lit
segment, but switch jump tables are emited in the lit segment and
initialized strng variables are put in the data segment.

I can change segments here, but I also need to fixup the
label that was just defined

Note that the lit segment is read-write in the VM, so strings
aren't read only as in some architectures.
*/
static void HackToSegment( segmentName_t seg )
{
	if ( currentSegment == &segment[seg] ) {
		return;
	}

	currentSegment = &segment[seg];
	if ( passNumber == 0 ) {
		lastSymbol->segment = currentSegment;
		lastSymbol->value = currentSegment->imageUsed;
	}
}


#define ASM(O) static qboolean TryAssemble##O()

// call instructions reset currentArgOffset
ASM(CALL)
{
	if ( !strncmp( token, "CALL", 4 ) ) {
		EmitByte( &segment[CODESEG], OP_CALL );
		instructionCount++;
		currentArgOffset = 0;
		return qtrue;
	}
	return qfalse;
}

// arg is converted to a reversed store
ASM(ARG)
{
	if ( !strncmp( token, "ARG", 3 ) ) {
		EmitByte( &segment[CODESEG], OP_ARG );
		instructionCount++;
		if ( 8 + currentArgOffset >= 256 ) {
			CodeError( "currentArgOffset >= 256" );
			return qtrue;
		}
		EmitByte( &segment[CODESEG], 8 + currentArgOffset );
		currentArgOffset += 4;
		return qtrue;
	}
	return qfalse;
}

// ret just leaves something on the op stack
ASM(RET)
{
	if ( !strncmp( token, "RET", 3 ) ) {
		EmitByte( &segment[CODESEG], OP_LEAVE );
		instructionCount++;
		EmitInt( &segment[CODESEG], 8 + currentLocals + currentArgs );
		return qtrue;
	}
	return qfalse;
}

// pop is needed to discard the return value of a function
ASM(POP)
{
	if ( !strncmp( token, "pop", 3 ) ) {
		EmitByte( &segment[CODESEG], OP_POP );
		instructionCount++;
		return qtrue;
	}
	return qfalse;
}

// address of a parameter is converted to OP_LOCAL
ASM(ADDRF)
{
	if ( !strncmp( token, "ADDRF", 5 ) ) {
		instructionCount++;
		GetToken();
		int v = ParseExpression() + 16 + currentArgs + currentLocals;
		EmitByte( &segment[CODESEG], OP_LOCAL );
		EmitInt( &segment[CODESEG], v );
		return qtrue;
	}
	return qfalse;
}

// address of a local is converted to OP_LOCAL
ASM(ADDRL)
{
	if ( !strncmp( token, "ADDRL", 5 ) ) {
		instructionCount++;
		GetToken();
		int v = ParseExpression() + 8 + currentArgs;
		EmitByte( &segment[CODESEG], OP_LOCAL );
		EmitInt( &segment[CODESEG], v );
		return qtrue;
	}
	return qfalse;
}

ASM(PROC)
{
	if ( !strcmp( token, "proc" ) ) {
		GetToken();	// function name
		DefineSymbol( token, instructionCount );

		currentLocals = ParseValue();	// locals
		currentLocals = ( currentLocals + 3 ) & ~3;
		currentArgs = ParseValue();		// arg marshalling
		currentArgs = ( currentArgs + 3 ) & ~3;

		if ( 8 + currentLocals + currentArgs >= 32767 ) {
			CodeError( "Locals > 32k in %s\n", token );
		}

		instructionCount++;
		EmitByte( &segment[CODESEG], OP_ENTER );
		EmitInt( &segment[CODESEG], 8 + currentLocals + currentArgs );
		return qtrue;
	}
	return qfalse;
}

ASM(ENDPROC)
{
	int		v, v2;
	if ( !strcmp( token, "endproc" ) ) {
		GetToken();				// skip the function name
		v = ParseValue();		// locals
		v2 = ParseValue();		// arg marshalling

		// all functions must leave something on the opstack
		instructionCount++;
		EmitByte( &segment[CODESEG], OP_PUSH );

		instructionCount++;
		EmitByte( &segment[CODESEG], OP_LEAVE );
		EmitInt( &segment[CODESEG], 8 + currentLocals + currentArgs );

		return qtrue;
	}
	return qfalse;
}

ASM(ADDRESS)
{
	if ( !strcmp( token, "address" ) ) {
		GetToken();
		int v = ParseExpression();
		// addresses are 32 bits wide, and therefore go into data segment
		HackToSegment( DATASEG );
		EmitInt( currentSegment, v );
		if ( passNumber == 1 && token[ 0 ] == '$' ) // crude test for labels
			EmitInt( &segment[ JTRGSEG ], v );
		return qtrue;
	}
	return qfalse;
}

ASM(EXPORT)
{
	if ( !strcmp( token, "export" ) ) {
		GetToken();	// function name
		ExportSymbol( token );
		return qtrue;
	}
	return qfalse;
}

ASM(IMPORT)
{
	if ( !strcmp( token, "import" ) ) {
		GetToken();	// function name
		ImportSymbol( token );
		return qtrue;
	}
	return qfalse;
}

ASM(CODE)
{
	if ( !strcmp( token, "code" ) ) {
		currentSegment = &segment[CODESEG];
		return qtrue;
	}
	return qfalse;
}

ASM(BSS)
{
	if ( !strcmp( token, "bss" ) ) {
		currentSegment = &segment[BSSSEG];
		return qtrue;
	}
	return qfalse;
}

ASM(DATA)
{
	if ( !strcmp( token, "data" ) ) {
		currentSegment = &segment[DATASEG];
		return qtrue;
	}
	return qfalse;
}

ASM(LIT)
{
	if ( !strcmp( token, "lit" ) ) {
		currentSegment = &segment[LITSEG];
		return qtrue;
	}
	return qfalse;
}

ASM(LINE)
{
	if ( !strcmp( token, "line" ) ) {
		return qtrue;
	}
	return qfalse;
}

ASM(FILE)
{
	if ( !strcmp( token, "file" ) ) {
		return qtrue;
	}
	return qfalse;
}

ASM(EQU)
{
	char name[1024];
	if ( !strcmp( token, "equ" ) ) {
		GetToken();
		strcpy( name, token );
		GetToken();
		ExportSymbol( name );
		DefineSymbol( name, atoiNoCap(token) );
		return qtrue;
	}
	return qfalse;
}

ASM(ALIGN)
{
	if ( !strcmp( token, "align" ) ) {
		int v = ParseValue();
		currentSegment->imageUsed = (currentSegment->imageUsed + v - 1 ) & ~( v - 1 );
		return qtrue;
	}
	return qfalse;
}

ASM(SKIP)
{
	if ( !strcmp( token, "skip" ) ) {
		int v = ParseValue();
		currentSegment->imageUsed += v;
		return qtrue;
	}
	return qfalse;
}

ASM(BYTE)
{
	if ( !strcmp( token, "byte" ) ) {
		int c = ParseValue();
		int v = ParseValue();

		if ( c == 1 ) {
			// character (1-byte) values go into lit(eral) segment
			HackToSegment( LITSEG );
		} else if ( c == 4 ) {
			// 32-bit (4-byte) values go into data segment
			HackToSegment( DATASEG );
		} else {
			CodeError( "%i-byte initialized data not supported", c );
			return qtrue;
		}

		// emit little endien
		while (c--) {
			EmitByte( currentSegment, (v & 0xFF) ); // paranoid ANDing
			v >>= 8;
		}
		return qtrue;
	}
	return qfalse;
}

// code labels are emitted as instruction counts, not byte offsets,
// because the physical size of the code will change with
// different run time compilers and we want to minimize the
// size of the required translation table
ASM(LABEL)
{
	if ( !strncmp( token, "LABEL", 5 ) ) {
		GetToken();
		if ( currentSegment == &segment[CODESEG] ) {
			DefineSymbol( token, instructionCount );
		} else {
			DefineSymbol( token, currentSegment->imageUsed );
		}
		return qtrue;
	}
	return qfalse;
}

#undef ASM


static void AssembleLine()
{
	GetToken();
	if ( !token[0] )
		return;

	OpTable::const_iterator it = aOpTable.find( token );

	if (it != aOpTable.end()) {
		int opcode = (*it).second;

		if ( opcode == OP_UNDEF ) {
			CodeError( "Undefined opcode: %s\n", token );
		}

		if ( opcode == OP_IGNORE ) {
			return;		// we ignore most conversions
		}

		// sign extensions need to check next parm
		if ( opcode == OP_SEX8 ) {
			GetToken();
			if ( token[0] == '1' ) {
				opcode = OP_SEX8;
			} else if ( token[0] == '2' ) {
				opcode = OP_SEX16;
			} else {
				CodeError( "Bad sign extension: %s\n", token );
				return;
			}
		}

		// check for expression
		GetToken();

		EmitByte( &segment[CODESEG], opcode );
		if ( token[0] && opcode != OP_CVIF && opcode != OP_CVFI ) {
			int expression = ParseExpression();
			// code like this can generate non-dword block copies:
			// auto char buf[2] = " ";
			// we are just going to round up.  This might conceivably
			// be incorrect if other initialized chars follow.
			if ( opcode == OP_BLOCK_COPY ) {
				expression = ( expression + 3 ) & ~3;
			}
			EmitInt( &segment[CODESEG], expression );
		}

		instructionCount++;
		return;
	}

// these should ideally be sorted by descending statistical frequency
// tho the difference is ms-trivial, so don't obsess over it
//#define ASM(O) if (TryAssemble##O()) { printf("STAT "#O"\n"); return; }

#define ASM(O) if (TryAssemble##O()) { return; }

	ASM(BYTE)
	ASM(ADDRL)
	ASM(LINE)
	ASM(ARG)
	ASM(IMPORT)
	ASM(LABEL)
	ASM(ADDRF)
	ASM(CALL)
	ASM(POP)
	ASM(RET)
	ASM(ALIGN)
	ASM(EXPORT)
	ASM(PROC)
	ASM(ENDPROC)
	ASM(ADDRESS)
	ASM(SKIP)
	ASM(EQU)
	ASM(CODE)
	ASM(LIT)
	ASM(FILE)
	ASM(BSS)
	ASM(DATA)

#undef ASM

	CodeError( "Unknown token: %s\n", token );
}


static void InitTables()
{
	struct SourceOp {
		const char* name;
		int opcode;
	};

	static const SourceOp aSourceOps[] = {
		#include "opstrings.h"
	};

	for (int i = 0; aSourceOps[i].name; ++i) {
		aOpTable[ aSourceOps[i].name ] = aSourceOps[i].opcode;
	}

	segment[CODESEG].name = "CS";
	segment[DATASEG].name = "DS";
	segment[LITSEG].name = "LS";
	segment[BSSSEG].name = "BS";
	segment[JTRGSEG].name = "JT";
}


static void ShowTable( const SymTable& table, const char* name )
{
	report( "%s: %d symbols\n", name, table.size() );
}


static void WriteMapFile()
{
	char imageName[MAX_OSPATH];
	COM_StripExtension( outputFilename, imageName, sizeof(imageName) );
	strcat( imageName, ".map" );

	report( "Writing %s...\n", imageName );

	FILE* f = SafeOpenWrite( imageName );
	for (int seg = CODESEG; seg <= BSSSEG; ++seg) {		
		for (SymTable::const_iterator it = aSymGlobal.begin(); it != aSymGlobal.end(); ++it) {
			const symbol_t* s = (*it).second;
			if ( &segment[seg] == s->segment ) {
				fprintf( f, "%d %8X %s\n", seg, s->value, (*it).first );
			}
		}
	}
	fclose( f );
}


static void WriteVmFile()
{
	report( "%i total errors\n", errorCount );

	char imageName[MAX_OSPATH];
	COM_StripExtension( outputFilename, imageName, sizeof(imageName) );
	strcat( imageName, ".qvm" );

	remove( imageName );

	for (int seg = 0; seg < NUM_SEGMENTS; ++seg)
		report( "%s: %7i\n", segment[seg].name, segment[seg].imageUsed );
	report( "instruction count: %i\n", instructionCount );

	if ( errorCount != 0 ) {
		report( "Not writing a file due to errors\n" );
		return;
	}

	vmHeader_t header;
	header.vmMagic = VM_MAGIC;
	// Don't write the VM_MAGIC_VER2 bits when maintaining 1.32b compatibility.
	// (I know this isn't strictly correct due to padding, but then platforms
	// that pad wouldn't be able to write a correct header anyway).
	// Note: if vmHeader_t changes, this needs to be adjusted too.
	const int headerSize = sizeof( header );

	header.instructionCount = instructionCount;
	header.codeOffset = headerSize;
	header.codeLength = segment[CODESEG].imageUsed;
	header.dataOffset = header.codeOffset + segment[CODESEG].imageUsed;
	header.dataLength = segment[DATASEG].imageUsed;
	header.litLength = segment[LITSEG].imageUsed;
	header.bssLength = segment[BSSSEG].imageUsed;

	report( "Writing to %s\n", imageName );

	CreatePath( imageName );
	FILE* f = SafeOpenWrite( imageName );
	SafeWrite( f, &header, headerSize );
	SafeWrite( f, &segment[CODESEG].image, segment[CODESEG].imageUsed );
	SafeWrite( f, &segment[DATASEG].image, segment[DATASEG].imageUsed );
	SafeWrite( f, &segment[LITSEG].image, segment[LITSEG].imageUsed );
	fclose( f );
}


static void Assemble()
{
	int i;

	report( "outputFilename: %s\n", outputFilename );

	for ( i = 0 ; i < numAsmFiles ; i++ ) {
		char filename[MAX_OSPATH];
		strcpy( filename, asmFileNames[ i ] );
		COM_DefaultExtension( filename, sizeof(filename), ".asm" );
		LoadFile( filename, (void **)&asmFiles[i] );
	}

	// assemble
	for ( passNumber = 0 ; passNumber < 2 ; passNumber++ ) {
		segment[LITSEG].segmentBase = segment[DATASEG].imageUsed;
		segment[BSSSEG].segmentBase = segment[LITSEG].segmentBase + segment[LITSEG].imageUsed;
		segment[JTRGSEG].segmentBase = segment[BSSSEG].segmentBase + segment[BSSSEG].imageUsed;
		for ( i = 0 ; i < NUM_SEGMENTS ; i++ ) {
			segment[i].imageUsed = 0;
		}
		segment[DATASEG].imageUsed = 4;		// skip the 0 byte, so NULL pointers are fixed up properly
		instructionCount = 0;

		for ( i = 0 ; i < numAsmFiles ; i++ ) {
			currentFileIndex = i;
			currentFileName = asmFileNames[ i ];
			currentFileLine = 0;
			report( "pass %i: %s\n", passNumber, currentFileName );
			fflush( NULL );
			const char* p = asmFiles[i];
			while ( p ) {
				p = ExtractLine( p );
				AssembleLine();
			}
			//ShowTable( aSymLocal[i], "Locals" );
		}

		// align all the segments
		for ( i = 0 ; i < NUM_SEGMENTS ; i++ ) {
			segment[i].imageUsed = (segment[i].imageUsed + 3) & ~3;
		}
	}

	SymTable::const_iterator it = aSymGlobal.find( "vmMain" );
	if ( (it == aSymGlobal.end()) || ((*it).second->segment != &segment[CODESEG]) || (*it).second->value ) {
		CodeError( "vmMain missing or not the first symbol in the QVM\n" );
	}

	// reserve the stack in bss
	const int stackSize = 0x10000;
	DefineSymbol( "_stackStart", segment[BSSSEG].imageUsed );
	segment[BSSSEG].imageUsed += stackSize;
	DefineSymbol( "_stackEnd", segment[BSSSEG].imageUsed );

	WriteVmFile();

	// only write the map file if there were no errors
	if ( options.writeMapFile && !errorCount ) {
		WriteMapFile();
	}
}


static void ParseOptionFile( const char* filename )
{
	char expanded[MAX_OSPATH];
	strcpy( expanded, filename );
	COM_DefaultExtension( expanded, sizeof(expanded), ".q3asm" );

	char* text;
	LoadFile( expanded, (void **)&text );
	if ( !text ) {
		return;
	}

	char* text_p = text;

	while( ( text_p = ASM_Parse( text_p ) ) != 0 ) {
		if ( !strcmp( com_token, "-o" ) ) {
			// allow output override in option file
			text_p = ASM_Parse( text_p );
			if ( text_p ) {
				strcpy( outputFilename, com_token );
			}
			continue;
		}

		asmFileNames[ numAsmFiles ] = copystring( com_token );
		numAsmFiles++;
	}
}


int main( int argc, char **argv )
{
	if ( argc < 2 ) {
		Com_Error( ERR_FATAL, "Usage: %s [OPTION]... [FILES]...\n"
				"Assemble LCC bytecode assembly to Q3VM bytecode.\n\n"
				"-o OUTPUT     Write assembled output to file OUTPUT.qvm\n"
				"-f LISTFILE   Read options and list of files to assemble from LISTFILE\n"
				"-m            Write symbols to a .map file\n"
				"-v            Verbose compilation report\n"
				, argv[0] );
	}

	float tStart = Q_FloatTime();

	// default filename is "q3asm"
	strcpy( outputFilename, "q3asm" );
	numAsmFiles = 0;

	int i;
	for (i = 1; i < argc; ++i) {
		if ( argv[i][0] != '-' ) {
			break;
		}

		if ( !strcmp( argv[i], "-o" ) ) {
			if ( i == argc - 1 ) {
				Com_Error( ERR_FATAL, "-o requires a filename" );
			}
			strcpy( outputFilename, argv[++i] );
			continue;
		}

		if ( !strcmp( argv[i], "-f" ) ) {
			if ( i == argc - 1 ) {
				Com_Error( ERR_FATAL, "-f requires a filename" );
			}
			ParseOptionFile( argv[++i] );
			continue;
		}

		// by default (no -v option), q3asm remains silent except for critical errors
		// verbosity turns on all messages, error or not
		if ( !strcmp( argv[ i ], "-v" ) ) {
			options.verbose = qtrue;
			continue;
		}

		if ( !strcmp( argv[ i ], "-m" ) ) {
			options.writeMapFile = qtrue;
			continue;
		}

		Com_Error( ERR_FATAL, "Unknown option: %s", argv[i] );
	}

	// the rest of the command line args are asm files
	for (; i < argc; ++i) {
		asmFileNames[ numAsmFiles ] = copystring( argv[ i ] );
		numAsmFiles++;
	}

#if defined(_DEBUG)
	options.writeMapFile = qtrue;
	options.verbose = qtrue;
#endif

	InitTables();
	Assemble();

	report( "%s compiled in %.3fs\n", outputFilename, Q_FloatTime() - tStart );
	ShowTable( aSymImported, "Imported" );
	ShowTable( aSymGlobal, "Globals" );

	return errorCount;
}

