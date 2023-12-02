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

#define	MAX_OPSTACK_SIZE	512
#define	PROC_OPSTACK_SIZE	30

#define VMMAIN_CALL_ARGS	13

// hardcoded in q3asm and reserved at end of bss
#define	PROGRAM_STACK_SIZE	0x10000

typedef enum {
	OP_UNDEF,

	OP_IGNORE,

	OP_BREAK,

	OP_ENTER,
	OP_LEAVE,
	OP_CALL,
	OP_PUSH,
	OP_POP,

	OP_CONST,
	OP_LOCAL,

	OP_JUMP,

	//-------------------

	OP_EQ,
	OP_NE,

	OP_LTI,
	OP_LEI,
	OP_GTI,
	OP_GEI,

	OP_LTU,
	OP_LEU,
	OP_GTU,
	OP_GEU,

	OP_EQF,
	OP_NEF,

	OP_LTF,
	OP_LEF,
	OP_GTF,
	OP_GEF,

	//-------------------

	OP_LOAD1,
	OP_LOAD2,
	OP_LOAD4,
	OP_STORE1,
	OP_STORE2,
	OP_STORE4,				// *(stack[top-1]) = stack[top]
	OP_ARG,

	OP_BLOCK_COPY,

	//-------------------

	OP_SEX8,
	OP_SEX16,

	OP_NEGI,
	OP_ADD,
	OP_SUB,
	OP_DIVI,
	OP_DIVU,
	OP_MODI,
	OP_MODU,
	OP_MULI,
	OP_MULU,

	OP_BAND,
	OP_BOR,
	OP_BXOR,
	OP_BCOM,

	OP_LSH,
	OP_RSHI,
	OP_RSHU,

	OP_NEGF,
	OP_ADDF,
	OP_SUBF,
	OP_DIVF,
	OP_MULF,

	OP_CVIF,
	OP_CVFI,

	OP_MAX
} opcode_t;

// macro opcode sequences
typedef enum {
	MOP_UNDEF = OP_MAX,
	MOP_IGNORE4,
	MOP_ADD4,
	MOP_SUB4,
	MOP_BAND4,
	MOP_BOR4,
	MOP_CALCF4,
} macro_op_t;

typedef struct {
	int		value;
	byte	op;
	byte	opStack;
	byte	jused;	// boolean
	byte	swtch;	// boolean
} instruction_t;

extern const char *opname[OP_MAX]; 

typedef int	vmptr_t;

typedef struct vmSymbol_s {
	struct vmSymbol_s	*next;
	int		symValue;
	int		profileCount;
	char	symName[1];		// variable sized
} vmSymbol_t;

typedef union vmFunc_u {
	byte		*ptr;
	void (*func)(void);
} vmFunc_t;

#define MAX_VM_CALL_STACK_DEPTH 64

struct vm_s {
	// DO NOT MOVE OR CHANGE THESE WITHOUT CHANGING THE VM_OFFSET_* DEFINES
	// USED BY THE ASM CODE
	int			programStack;		// the vm may be recursively entered
    syscall_t	systemCall;
	byte		*dataBase;
	int			*opStack;			// pointer to local function stack

	int			instructionCount;
	intptr_t	*instructionPointers;

	//------------------------------------

	const char	*name;

	// for dynamic linked modules
	void		*dllHandle;
	dllSyscall_t entryPoint;
	void		(*destroy)(vm_t* self);

	// for interpreted modules
	qboolean	currentlyInterpreting;

	qboolean	compiled;

	vmFunc_t	codeBase;
	int			codeLength;

	int			allocSize;			// total allocation size, in bytes

	int			dataMask;
	int			dataLength;			// exact data segment length

	int			stackBottom;		// if programStack < stackBottom, error
	int			*opStackTop;		

	int			numSymbols;
	vmSymbol_t	*symbols;

	int			callLevel;			// counts recursive VM_Call
	int			breakFunction;		// increment breakCount on function entry to this
	int			breakCount;

	byte		*jumpTableTargets;
	int			numJumpTableTargets;

	vmIndex_t	index;

	int			callStackDepth;
	int			lastCallStackDepth;
	int			callStackDepthTemp; // only for vm_x86.cpp
	int			callStack[MAX_VM_CALL_STACK_DEPTH];
};

extern	vm_t	*currentVM;

#define	VM_MAGIC		0x12721444
typedef struct {
	int		vmMagic;

	int		instructionCount;

	int		codeOffset;
	int		codeLength;

	int		dataOffset;
	int		dataLength;
	int		litLength;			// ( dataLength - litLength ) should be byteswapped on load
	int		bssLength;			// zero filled memory appended to datalength
} vmHeader_t;

qboolean VM_Compile( vm_t *vm, vmHeader_t *header );
int	VM_CallCompiled( vm_t *vm, int *args );

qboolean VM_PrepareInterpreter2( vm_t *vm, vmHeader_t *header );
int	VM_CallInterpreted2( vm_t *vm, int *args );

const char *VM_LoadInstructions( const vmHeader_t *header, instruction_t *buf );
const char *VM_CheckInstructions( instruction_t *buf, int instructionCount, 
								 const byte *jumpTableTargets, 
								 int numJumpTableTargets, 
								 int dataLength );

intptr_t VM_ArgPtr( intptr_t intValue );
intptr_t VM_ExplicitArgPtr( const vm_t* vm, intptr_t intValue );

static ID_INLINE float _vmf(intptr_t x)
{
	union {
		intptr_t l;
		float f;
	} t;
	t.l = x;
	return t.f;
}
#define VMF(x) _vmf(args[x])

#define VM_OF_JUMP	(1<<0)

typedef struct opcode_info_s 
{
	int   size; 
	int	  stack;
	int   nargs;
	int   flags;
} opcode_info_t ;

extern opcode_info_t ops[ OP_MAX ];

