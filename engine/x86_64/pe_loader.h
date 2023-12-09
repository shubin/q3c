#pragma once

enum {
	PE_OK						= 0,
	PE_INVALID_ARGUMENT			= 1,
	PE_BAD_DOS_HEADER			= 2,
	PE_BAD_NT_HEADER			= 3,
	PE_CANT_RELOCATE			= 4,
	PE_CANT_ALLOCATE			= 5,
	PE_RELOC_MISSING			= 6,
	PE_RELOC_NOT_SUPPORTED		= 7,
	PE_MRPROTECT_FAILED			= 8,
	PE_CANT_OPEN_FILE			= 9,
	PE_CANT_STAT_FILE			= 10,
	PE_CANT_MAP_FILE			= 11,
	PE_PROC_NOT_FOUND			= 12,
};

typedef void *PEHandle;
typedef void *PEProc;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

PEHandle	PE_LoadLibrary( const char *filename );
PEHandle	PE_LoadLibraryFromMemory( const void *mem, unsigned int size );
int			PE_FreeLibrary( PEHandle handle );
PEProc		PE_GetProcAddress( PEHandle handle, const char *procname );
int			PE_GetLastError( void );
const char* PE_ErrorMessage( int error );

#ifdef __cplusplus
}
#endif // __cplusplus
