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
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"
#include "crash.h"
#include "common_help.h"

opcode_info_t ops[ OP_MAX ] =
{
	{ 0, 0, 0, 0 }, // undef
	{ 0, 0, 0, 0 }, // ignore
	{ 0, 0, 0, 0 }, // break

	{ 4, 0, 0, 0 }, // enter
	{ 4,-4, 0, 0 }, // leave
	{ 0, 0, 1, 0 }, // call
	{ 0, 4, 0, 0 }, // push
	{ 0,-4, 1, 0 }, // pop

	{ 4, 4, 0, 0 }, // const
	{ 4, 4, 0, 0 }, // local
	{ 0,-4, 1, 0 }, // jump

	{ 4,-8, 2, VM_OF_JUMP }, // eq
	{ 4,-8, 2, VM_OF_JUMP }, // ne

	{ 4,-8, 2, VM_OF_JUMP }, // lti
	{ 4,-8, 2, VM_OF_JUMP }, // lei
	{ 4,-8, 2, VM_OF_JUMP }, // gti
	{ 4,-8, 2, VM_OF_JUMP }, // gei

	{ 4,-8, 2, VM_OF_JUMP }, // ltu
	{ 4,-8, 2, VM_OF_JUMP }, // leu
	{ 4,-8, 2, VM_OF_JUMP }, // gtu
	{ 4,-8, 2, VM_OF_JUMP }, // geu

	{ 4,-8, 2, VM_OF_JUMP }, // eqf
	{ 4,-8, 2, VM_OF_JUMP }, // nef

	{ 4,-8, 2, VM_OF_JUMP }, // ltf
	{ 4,-8, 2, VM_OF_JUMP }, // lef
	{ 4,-8, 2, VM_OF_JUMP }, // gtf
	{ 4,-8, 2, VM_OF_JUMP }, // gef

	{ 0, 0, 1, 0 }, // load1
	{ 0, 0, 1, 0 }, // load2
	{ 0, 0, 1, 0 }, // load4
	{ 0,-8, 2, 0 }, // store1
	{ 0,-8, 2, 0 }, // store2
	{ 0,-8, 2, 0 }, // store4
	{ 1,-4, 1, 0 }, // arg
	{ 4,-8, 2, 0 }, // bcopy

	{ 0, 0, 1, 0 }, // sex8
	{ 0, 0, 1, 0 }, // sex16

	{ 0, 0, 1, 0 }, // negi
	{ 0,-4, 3, 0 }, // add
	{ 0,-4, 3, 0 }, // sub
	{ 0,-4, 3, 0 }, // divi
	{ 0,-4, 3, 0 }, // divu
	{ 0,-4, 3, 0 }, // modi
	{ 0,-4, 3, 0 }, // modu
	{ 0,-4, 3, 0 }, // muli
	{ 0,-4, 3, 0 }, // mulu

	{ 0,-4, 3, 0 }, // band
	{ 0,-4, 3, 0 }, // bor
	{ 0,-4, 3, 0 }, // bxor
	{ 0, 0, 1, 0 }, // bcom

	{ 0,-4, 3, 0 }, // lsh
	{ 0,-4, 3, 0 }, // rshi
	{ 0,-4, 3, 0 }, // rshu

	{ 0, 0, 1, 0 }, // negf
	{ 0,-4, 3, 0 }, // addf
	{ 0,-4, 3, 0 }, // subf
	{ 0,-4, 3, 0 }, // divf
	{ 0,-4, 3, 0 }, // mulf

	{ 0, 0, 1, 0 }, // cvif
	{ 0, 0, 1, 0 } // cvfi
};


vm_t	*currentVM = NULL;
vm_t	*lastVM    = NULL;

#define	MAX_VM		3
vm_t	vmTable[MAX_VM];

static const char *vmName[ VM_COUNT ] = {
	"qagame",
	"cgame",
	"ui"
};

#if !defined( QC )
static const cvarTableItem_t vm_cvars[] =
{
	{ NULL, "vm_cgame", "2", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", "how to load the cgame VM" help_vm_load },
	{ NULL, "vm_game", "2", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", "how to load the qagame VM" help_vm_load },
	{ NULL, "vm_ui", "2", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", "how to load the ui VM" help_vm_load }
};
#endif

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
#if !defined( QC )
	Cvar_RegisterArray( vm_cvars, MODULE_COMMON );
#endif

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


/*
===============
ParseHex
===============
*/
static int ParseHex( const char* text )
{
	int value = 0, c;

	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
		Com_Error( ERR_DROP, "Invalid hex value" );
	}

	return value;
}

/*
===============
VM_LoadSymbols
===============
*/
void VM_LoadSymbols( vm_t *vm ) {
	union {
		char	*c;
		void	*v;
	}			mapfile;
	const char	*text_p;
	const char	*token;
	char		name[MAX_QPATH];
	char		symbols[MAX_QPATH];
	vmSymbol_t	**prev, *sym;
	int			count;
	int			value;
	int			chars;
	int			segment;
	int			numInstructions;

	// don't load symbols if not developer
	if ( !com_developer->integer ) {
		return;
	}

	COM_StripExtension(vm->name, name, sizeof(name));
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	FS_ReadFile( symbols, &mapfile.v );
	if ( !mapfile.c ) {
		Com_Printf( "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	numInstructions = vm->instructionCount;

	// parse the symbols
	text_p = mapfile.c;
	prev = &vm->symbols;
	count = 0;

	while ( 1 ) {
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		segment = ParseHex( token );
		if ( segment ) {
			COM_Parse( &text_p );
			COM_Parse( &text_p );
			continue;		// only load code segment values
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		value = ParseHex( token );

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		chars = strlen( token );
		sym = (vmSymbol_t*)Hunk_Alloc( sizeof( *sym ) + chars, h_high );
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		// convert value from an instruction number to a code offset
		if ( vm->instructionPointers && value >= 0 && value < numInstructions ) {
			value = vm->instructionPointers[value];
		}

		sym->symValue = value;
		Q_strncpyz( sym->symName, token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Com_Printf( "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile.v );
}


/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.

============
*/

#if defined( QC )

#include "../qcommon/vm_syscall.h"

intptr_t VMCALL VM_DllSyscall( intptr_t *args ) {
	return currentVM->systemCall( args );
}

#else
intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
  // rcg010206 - see commentary above
  intptr_t args[16];
  int i;
  va_list ap;

  args[0] = arg;

  va_start(ap, arg);
  for (i = 1; i < ARRAY_LEN (args); i++)
    args[i] = va_arg(ap, intptr_t);
  va_end(ap);

  return currentVM->systemCall( args );
#else // original id code
	return currentVM->systemCall( &arg );
#endif
}
#endif


/*
=================
VM_ValidateHeader
=================
*/
static char *VM_ValidateHeader( vmHeader_t *header, int fileSize )
{
	static char errMsg[128];

	// truncated
	if ( fileSize < ( sizeof( vmHeader_t ) - sizeof( int ) ) ) {
		sprintf( errMsg, "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}

	// bad magic
	if ( LittleLong( header->vmMagic ) != VM_MAGIC ) {
		sprintf( errMsg, "bad VM header tag %08x, expected %08x", LittleLong( header->vmMagic ), LittleLong( VM_MAGIC ) );
		return errMsg;
	}

	// truncated
	if ( fileSize < sizeof( vmHeader_t ) ) {
		sprintf( errMsg, "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}

	const int n = ( sizeof( vmHeader_t ) - sizeof( int ) ) / sizeof( int );

	// byte swap the header
	for ( int i = 0 ; i < n ; i++ ) {
		((int *)header)[i] = LittleLong( ((int *)header)[i] );
	}

	// bad code offset
	if ( header->codeOffset >= fileSize ) {
		sprintf( errMsg, "bad code segment offset %i", header->codeOffset );
		return errMsg;
	}

	// bad code length
	if ( header->codeLength <= 0 || header->codeOffset + header->codeLength > fileSize ) {
		sprintf( errMsg, "bad code segment length %i", header->codeLength );
		return errMsg;
	}

	// bad data offset
	if ( header->dataOffset >= fileSize || header->dataOffset != header->codeOffset + header->codeLength ) {
		sprintf( errMsg, "bad data segment offset %i", header->dataOffset );
		return errMsg;
	}

	// bad data length
	if ( header->dataOffset + header->dataLength > fileSize )  {
		sprintf( errMsg, "bad data segment length %i", header->dataLength );
		return errMsg;
	}

	// bad lit length
	if ( header->dataOffset + header->dataLength + header->litLength != fileSize )
	{
		sprintf( errMsg, "bad lit segment length %i", header->litLength );
		return errMsg;
	}

	return NULL;
}


/*
=================
VM_LoadQVM

Load a .qvm file

if ( alloc )
 - Validate header, swap data
 - Alloc memory for data/instructions
 - Alloc memory for instructionPointers - NOT NEEDED
 - Load instructions
 - Clear/load data
else
 - Check for header changes
 - Clear/load data

=================
*/
vmHeader_t *VM_LoadQVM( vm_t *vm, qboolean alloc ) {
	int					length;
	int					dataLength;
	int					i;
	char				filename[MAX_QPATH], *errorMsg;
	vmHeader_t			*header;

	// load the image
	Com_sprintf( filename, sizeof(filename), "vm/%s.qvm", vm->name );
	Com_Printf( "Loading vm file %s...\n", filename );
	length = FS_ReadFile( filename, (void **)&header );
	if ( !header ) {
		Com_Printf( "Failed.\n" );
		VM_Free( vm );
		return NULL;
	}

	// will also swap header
	errorMsg = VM_ValidateHeader( header, length );
	if ( errorMsg ) {
		VM_Free( vm );
		FS_FreeFile( header );
		Com_Error( ERR_FATAL, "%s", errorMsg );
		return NULL;
	}

	dataLength = header->dataLength + header->litLength + header->bssLength;
	vm->dataLength = dataLength;

	// round up to next power of 2 so all data operations can
	// be mask protected
	for ( i = 0 ; dataLength > ( 1 << i ) ; i++ ) {
	}
	dataLength = 1 << i;

	if( alloc ) {
		// allocate zero filled space for initialized and uninitialized data
		vm->dataBase = (byte *)Hunk_Alloc( dataLength, h_high );
		vm->dataMask = dataLength - 1;
	} else {
		// clear the data, but make sure we're not clearing more than allocated
		if( vm->dataMask + 1 != dataLength ) {
			VM_Free( vm );
			FS_FreeFile( header );
			Com_Printf( S_COLOR_YELLOW "Warning: Data region size of %s not matching after"
					"VM_Restart()\n", filename );
			return NULL;
		}
		Com_Memset( vm->dataBase, 0, dataLength );
	}

	// copy the intialized data
	Com_Memcpy( vm->dataBase, (byte *)header + header->dataOffset, header->dataLength + header->litLength );

	// byte swap the longs
	for ( i = 0 ; i < header->dataLength ; i += 4 ) {
		*(int *)(vm->dataBase + i) = LittleLong( *(int *)(vm->dataBase + i ) );
	}

	unsigned int crc32;
	CRC32_Begin( &crc32 );
	CRC32_ProcessBlock( &crc32, header, length );
	CRC32_End( &crc32 );
	Crash_SaveQVMChecksum( vm->index, crc32 );

	return header;
}


/*
=================
VM_LoadInstructions

loads instructions in structured format
=================
*/
const char *VM_LoadInstructions( const vmHeader_t *header, instruction_t *buf )
{
	static char errBuf[ 128 ];
	byte *code_pos, *code_start, *code_end;
	int i, n, op0, op1, opStack;
	instruction_t *ci;

	code_pos = (byte *) header + header->codeOffset;
	code_start = code_pos; // for printing
	code_end =  (byte *) header + header->codeOffset + header->codeLength;

	ci = buf;
	opStack = 0;
	op1 = OP_UNDEF;

	// load instructions and perform some initial calculations/checks
	for ( i = 0; i < header->instructionCount; i++, ci++, op1 = op0 ) {
		op0 = *code_pos;
		if ( op0 < 0 || op0 >= OP_MAX ) {
			sprintf( errBuf, "bad opcode %02X at offset %lld", (unsigned int)op0, (long long)(code_pos - code_start) );
			return errBuf;
		}
		n = ops[ op0 ].size;
		if ( code_pos + 1 + n  > code_end ) {
			sprintf( errBuf, "code_pos > code_end" );
			return errBuf;
		}
		code_pos++;
		ci->op = op0;
		if ( n == 4 ) {
			ci->value = LittleLong( *((int*)code_pos) );
			code_pos += 4;
		} else if ( n == 1 ) {
			ci->value = *((unsigned char*)code_pos);
			code_pos += 1;
		} else {
			ci->value = 0;
		}

		// setup jump value from previous const
		if ( op0 == OP_JUMP && op1 == OP_CONST ) {
			ci->value = (ci-1)->value;
		}

		ci->opStack = opStack;
		opStack += ops[ op0 ].stack;
	}

	return NULL;
}


/*
===============================
VM_CheckInstructions

performs additional consistency and security checks
===============================
*/
const char *VM_CheckInstructions( instruction_t *buf,
								 int instructionCount,
								 const byte *jumpTableTargets,
								 int numJumpTableTargets,
								 int dataLength )
{
	static char errBuf[ 128 ];
	int i, n, v, op0, op1, opStack, pstack;
	instruction_t *ci, *proc;
	int startp, endp;

	ci = buf;
	opStack = 0;

	// opstack checks
	for ( i = 0; i < instructionCount; i++, ci++ ) {
		opStack += ops[ ci->op ].stack;
		if ( opStack < 0 ) {
			sprintf( errBuf, "opStack underflow at %i", i );
			return errBuf;
		}
		if ( opStack >= PROC_OPSTACK_SIZE * 4 ) {
			sprintf( errBuf, "opStack overflow at %i", i );
			return errBuf;
		}
	}

	ci = buf;
	pstack = 0;
	op1 = OP_UNDEF;
	proc = NULL;

	startp = 0;
	endp = instructionCount - 1;

	// Additional security checks

	for ( i = 0; i < instructionCount; i++, ci++, op1 = op0 ) {
		op0 = ci->op;

		// function entry
		if ( op0 == OP_ENTER ) {
			// missing block end
			if ( proc || ( pstack && op1 != OP_LEAVE ) ) {
				sprintf( errBuf, "missing proc end before %i", i );
				return errBuf;
			}
			if ( ci->opStack != 0 ) {
				v = ci->opStack;
				sprintf( errBuf, "bad entry opstack %i at %i", v, i );
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				sprintf( errBuf, "bad entry programStack %i at %i", v, i );
				return errBuf;
			}

			pstack = ci->value;

			// mark jump target
			ci->jused = 1;
			proc = ci;
			startp = i + 1;

			// locate endproc
			for ( endp = 0, n = i+1 ; n < instructionCount; n++ ) {
				if ( buf[n].op == OP_PUSH && buf[n+1].op == OP_LEAVE ) {
					endp = n;
					break;
				}
			}

			if ( endp == 0 ) {
				sprintf( errBuf, "missing end proc for %i", i );
				return errBuf;
			}

			continue;
		}

		// proc opstack will carry max.possible opstack value
		if ( proc && ci->opStack > proc->opStack )
			proc->opStack = ci->opStack;

		// function return
		if ( op0 == OP_LEAVE ) {
			// bad return programStack
			if ( pstack != ci->value ) {
				v = ci->value;
				sprintf( errBuf, "bad programStack %i at %i", v, i );
				return errBuf;
			}
			// bad opStack before return
			if ( ci->opStack != 4 ) {
				v = ci->opStack;
				sprintf( errBuf, "bad opStack %i at %i", v, i );
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				sprintf( errBuf, "bad return programStack %i at %i", v, i );
				return errBuf;
			}
			if ( op1 == OP_PUSH ) {
				if ( proc == NULL ) {
					sprintf( errBuf, "unexpected proc end at %i", i );
					return errBuf;
				}
				proc = NULL;
				startp = i + 1; // next instruction
				endp = instructionCount - 1; // end of the image
			}
			continue;
		}

		// conditional jumps
		if ( ops[ ci->op ].flags & VM_OF_JUMP ) {
			v = ci->value;
			// conditional jumps should have opStack == 8
			if ( ci->opStack != 8 ) {
				sprintf( errBuf, "bad jump opStack %i at %i", ci->opStack, i );
				return errBuf;
			}
			//if ( v >= header->instructionCount ) {
			// allow only local proc jumps
			if ( v < startp || v > endp ) {
				sprintf( errBuf, "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
				return errBuf;
			}
			if ( buf[v].opStack != 0 ) {
				n = buf[v].opStack;
				sprintf( errBuf, "jump target %i has bad opStack %i", v, n );
				return errBuf;
			}
			// mark jump target
			buf[v].jused = 1;
			continue;
		}

		// unconditional jumps
		if ( op0 == OP_JUMP ) {
			// jumps should have opStack == 4
			if ( ci->opStack != 4 ) {
				sprintf( errBuf, "bad jump opStack %i at %i", ci->opStack, i );
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// allow only local jumps
				if ( v < startp || v > endp ) {
					sprintf( errBuf, "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
					return errBuf;
				}
				if ( buf[v].opStack != 0 ) {
					n = buf[v].opStack;
					sprintf( errBuf, "jump target %i has bad opStack %i", v, n );
					return errBuf;
				}
				if ( buf[v].op == OP_ENTER ) {
					n = buf[v].op;
					sprintf( errBuf, "jump target %i has bad opcode %i", v, n );
					return errBuf;
				}
				if ( v == (i-1) ) {
					sprintf( errBuf, "self loop at %i", v );
					return errBuf;
				}
				// mark jump target
				buf[v].jused = 1;
			} else {
				if ( proc )
					proc->swtch = 1;
				else
					ci->swtch = 1;
			}
			continue;
		}

		if ( op0 == OP_CALL ) {
			if ( ci->opStack < 4 ) {
				sprintf( errBuf, "bad call opStack at %i", i );
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// analyse only local function calls
				if ( v >= 0 ) {
					if ( v >= instructionCount ) {
						sprintf( errBuf, "call target %i is out of range", v );
						return errBuf;
					}
					if ( buf[v].op != OP_ENTER ) {
						n = buf[v].op;
						sprintf( errBuf, "call target %i has bad opcode %i", v, n );
						return errBuf;
					}
					if ( v == 0 ) {
						sprintf( errBuf, "explicit vmMain call inside VM" );
						return errBuf;
					}
					// mark jump target
					buf[v].jused = 1;
				}
			}
			continue;
		}

		if ( ci->op == OP_ARG ) {
			v = ci->value & 255;
			// argument can't exceed programStack frame
			if ( v < 8 || v > pstack - 4 || (v & 3) ) {
				sprintf( errBuf, "bad argument address %i at %i", v, i );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_LOCAL ) {
			v = ci->value;
			if ( proc == NULL ) {
				sprintf( errBuf, "missing proc frame for local %i at %i", v, i );
				return errBuf;
			}
			if ( (ci+1)->op == OP_LOAD1 || (ci+1)->op == OP_LOAD2 || (ci+1)->op == OP_LOAD4 ) {
				// FIXME: alloc 256 bytes of programStack in VM_CallCompiled()?
				if ( v < 8 || v >= proc->value + 256 ) {
					sprintf( errBuf, "bad local address %i at %i", v, i );
					return errBuf;
				}
			}
		}

		if ( ci->op == OP_LOAD4 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 4 ) {
				sprintf( errBuf, "bad load4 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}

		if ( ci->op == OP_LOAD2 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 2 ) {
				sprintf( errBuf, "bad load2 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}

		if ( ci->op == OP_LOAD1 && op1 == OP_CONST ) {
			v =  (ci-1)->value;
			if ( v < 0 || v > dataLength - 1 ) {
				sprintf( errBuf, "bad load1 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}
	}

	if ( op1 != OP_UNDEF && op1 != OP_LEAVE ) {
		sprintf( errBuf, "missing return instruction at the end of the image" );
		return errBuf;
	}

	// ensure that the optimization pass knows about all the jump table targets
	if ( jumpTableTargets ) {
		for( i = 0; i < numJumpTableTargets; i++ ) {
			n = *(int *)(jumpTableTargets + ( i * sizeof( int ) ) );
			if ( n < 0 || n >= instructionCount ) {
				sprintf( errBuf, "jump target %i at %i is out of range [0..%i]", n, i, instructionCount - 1 );
				return errBuf;
			}
			if ( buf[n].opStack != 0 ) {
				opStack = buf[n].opStack;
				sprintf( errBuf, "jump target set on instruction %i with bad opStack %i", n, opStack );
				return errBuf;
			}
			buf[n].jused = 1;
		}
	} else {
		v = 0;
		// instructions with opStack > 0 can't be jump labels so its safe to optimize/merge
		for ( i = 0, ci = buf; i < instructionCount; i++, ci++ ) {
			if ( ci->op == OP_ENTER ) {
				v = ci->swtch;
				continue;
			}
			// if there is a switch statement in function -
			// mark all potential jump labels
			if ( ci->swtch )
				v = ci->swtch;
			if ( ci->opStack > 0 )
				ci->jused = 0;
			else if ( v )
				ci->jused = 1;
		}
	}

	return NULL;
}


/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	vmHeader_t	*header;

	// DLL's can't be restarted in place
	if ( vm->dllHandle ) {
		char		name[MAX_QPATH];
		syscall_t	systemCall;

		systemCall = vm->systemCall;
		Q_strncpyz( name, vm->name, sizeof( name ) );

		VM_Free( vm );

		vm = VM_Create( vm->index, systemCall, VMI_NATIVE );
		return vm;
	}

	// load the image
	Com_Printf( "VM_Restart()\n" );

	if( ( header = VM_LoadQVM( vm, qfalse ) ) == NULL ) {
		Com_Error( ERR_DROP, "VM_Restart failed" );
		return NULL;
	}

	// free the original file
	FS_FreeFile( header );

	return vm;
}


/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/
vm_t *VM_Create( vmIndex_t index, syscall_t systemCalls, vmInterpret_t interpret ) {
	int			remaining;
	const char	*name;
	vmHeader_t	*header;
	vm_t		*vm;

	if ( !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		Com_Error( ERR_FATAL, "VM_Create: bad vm index %i", index );
	}

	remaining = Hunk_MemoryRemaining();

	vm = &vmTable[ index ];

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			Com_Error( ERR_FATAL, "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		return vm;
	}

	name = vmName[ index ];

#if defined( QC )
	// if we have scriptable ui, load another VM for that
	cvar_t* ui_shell =  Cvar_FindVar( "ui_shell" );
	if ( index == VM_UI && (
		( ui_shell != NULL && FS_FileExists( va( "%s/main.lua", ui_shell->string ) ) )
		|| FS_FileExists( "shell/main.lua" )
		) ) {
		name = "qcui";
	}
#endif

	vm->name = name;
	vm->index = index;
	vm->systemCall = systemCalls;

	// never allow dll loading with a demo
	if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Printf( "Loading dll file %s.\n", name );
		vm->dllHandle = Sys_LoadDll( name, &vm->entryPoint, VM_DllSyscall );
		if ( vm->dllHandle ) {
			return vm;
		}

		Com_Printf( "Failed to load dll, looking for qvm.\n" );
		interpret = VMI_COMPILED;
	}

	// load the image
	if( ( header = VM_LoadQVM( vm, qtrue ) ) == NULL ) {
		return NULL;
	}

	// allocate space for the jump targets, which will be filled in by the compile/prep functions
	vm->instructionCount = header->instructionCount;
	//vm->instructionPointers = Hunk_Alloc(vm->instructionCount * sizeof(*vm->instructionPointers), h_high);
	vm->instructionPointers = NULL;

	// copy or compile the instructions
	vm->codeLength = header->codeLength;

	// the stack is implicitly at the end of the image
	vm->programStack = vm->dataMask + 1;
	vm->stackBottom = vm->programStack - PROGRAM_STACK_SIZE;

	vm->compiled = qfalse;

#ifdef NO_VM_COMPILED
	if(interpret >= VMI_COMPILED) {
		Com_Printf("Architecture doesn't have a bytecode compiler, using interpreter\n");
		interpret = VMI_BYTECODE;
	}
#else
	if ( interpret >= VMI_COMPILED ) {
		vm->compiled = qtrue;
		if ( !VM_Compile( vm, header ) ) {
			FS_FreeFile( header );	// free the original file
			VM_Free( vm );
			return NULL;
		}
	}
#endif
	// VM_Compile may have reset vm->compiled if compilation failed
	if ( !vm->compiled ) {
		if ( !VM_PrepareInterpreter2( vm, header ) ) {
			FS_FreeFile( header );	// free the original file
			VM_Free( vm );
			return NULL;
		}
	}

	// free the original file
	FS_FreeFile( header );

	// load the map file
	VM_LoadSymbols( vm );

	Crash_SaveQVMPointer( index, vm );

	Com_Printf( "%s loaded in %d bytes on the hunk\n", vm->name, remaining - Hunk_MemoryRemaining() );

	return vm;
}


/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if( !vm ) {
		return;
	}

	Crash_SaveQVMPointer( vm->index, NULL );

	if ( vm->destroy )
		vm->destroy( vm );

	if ( vm->dllHandle )
		Sys_UnloadDll( vm->dllHandle );

	Com_Memset( vm, 0, sizeof( *vm ) );

	currentVM = NULL;
	lastVM = NULL;
}


void VM_Clear( void ) {
	int i;
	for ( i = 0; i < VM_COUNT; i++ ) {
		VM_Free( &vmTable[ i ] );
	}
}


intptr_t VM_ArgPtr( intptr_t intValue )
{
	if (!intValue || !currentVM)
		return 0;

	if ( currentVM->entryPoint ) {
		return (intptr_t)(currentVM->dataBase + intValue);
	}
	else {
		return (intptr_t)(currentVM->dataBase + (intValue & currentVM->dataMask));
	}
}


intptr_t VM_ExplicitArgPtr( const vm_t* vm, intptr_t intValue )
{
	if (!intValue || !vm)
		return 0;

	// bk010124 - currentVM is missing on reconnect here as well?
	if (!currentVM)
	  return 0;

	if ( vm->entryPoint ) {
		return (intptr_t)(vm->dataBase + intValue);
	}
	else {
		return (intptr_t)(vm->dataBase + (intValue & vm->dataMask));
	}
}


/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/

#if !defined( QC )
intptr_t QDECL VM_Call( vm_t *vm, int callnum, ... )
{
	if ( !vm ) {
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

	vm_t *oldVM = currentVM;
	currentVM = vm;
	lastVM = vm;

	++vm->callLevel;

	intptr_t r;
	// if we have a dll loaded, call it directly
	if ( vm->entryPoint )
	{
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		int args[VMMAIN_CALL_ARGS-1];
		va_list ap;
		va_start( ap, callnum );
		for ( int i = 0; i < ARRAY_LEN( args ); i++ ) {
			args[i] = va_arg(ap, int);
		}
		va_end(ap);

		r = vm->entryPoint( callnum, args[0], args[1], args[2], args[3],
			args[4], args[5], args[6], args[7],
			args[8], args[9], args[10], args[11] );
	} else {
#if id386 && !defined __clang__ // calling convention doesn't need conversion in some cases
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, (int*)&callnum );
		else
#endif
			r = VM_CallInterpreted2( vm, (int*)&callnum );
#else
		struct {
			int callnum;
			int args[VMMAIN_CALL_ARGS-1];
		} a;
		va_list ap;

		a.callnum = callnum;
		va_start(ap, callnum);
		for (int i = 0; i < ARRAY_LEN( a.args ); i++ ) {
			a.args[i] = va_arg( ap, int );
		}
		va_end(ap);
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, &a.callnum );
		else
#endif
			r = VM_CallInterpreted2( vm, &a.callnum );
#endif
	}
	--vm->callLevel;

	if ( oldVM != NULL )
	  currentVM = oldVM;
	return r;
}
#else // QC
void VM_IncCallLevel( vm_t *vm, int delta ) {
	vm->callLevel += delta;
}

dllSyscall_t VM_EntryPoint( vm_t *vm ) {
	return vm->entryPoint;
}

qboolean VM_Compiled( vm_t *vm ) {
	return vm->compiled;
}
#endif // QC
