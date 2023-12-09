#if defined( _WIN32 )

#include <memory.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

#endif

#include <stdint.h>
#include "pe_loader.h"
#include "pe_local.h"

#if defined( _WIN32 )

#define PROT_READ	1
#define PROT_WRITE	2
#define PROT_EXEC	4

static DWORD sProtect[8] = {
	0,						// 0
	PAGE_READONLY,			// 1 PROT_READ
	PAGE_READWRITE,			// 2 PROT_WRITE
	PAGE_READWRITE,			// 3 PROT_READ|PROT_WRITE
	PAGE_EXECUTE_READ,		// 4 PROT_EXEC
	PAGE_EXECUTE_READ,		// 5 PROT_READ|PROT_EXEC
	PAGE_EXECUTE_READWRITE, // 6 PROT_WRITE|PROT_EXEC
	PAGE_EXECUTE_READWRITE, // 7 PROT_READ|PROT_WRITE|PROT_EXEC
};

#define thread_local __declspec( thread )

#else 

#define thread_local __thread

#endif

static thread_local int sError;

typedef struct {
	void	*pImage;
	size_t	szImage;	
} PEDESC;

PEHandle PE_LoadLibraryFromMemory( const void *mem, unsigned int size ) {
	PE_DOS_HEADER		*pDosHeader;
	PE_NT_HEADERS		*pNtHeader;
	PE_SECTION_HEADER	*pSectionHeader, *pSec;
	size_t					szSecSize;
	
	void					*pImage;
	size_t					szImage;
	
	int						prot;
	PEDESC					*pDesc;
#if defined( _WIN32 )
	DWORD					dwProt;
#endif // _WIN32

	if ( mem == NULL || size == 0 ) {
		sError = PE_INVALID_ARGUMENT;
		return NULL;
	}

	// check file headers
	pDosHeader = (PE_DOS_HEADER*)mem;
	if ( pDosHeader->e_magic != PE_DOS_SIGNATURE || pDosHeader->e_lfanew == 0 ) {
		sError = PE_BAD_DOS_HEADER;
		return NULL;
	}

	pNtHeader = (PE_NT_HEADERS*)( (uintptr_t)mem + pDosHeader->e_lfanew );
	if ( pNtHeader->Signature != PE_NT_SIGNATURE || pNtHeader->OptionalHeader.Magic != PE_NT_OPTIONAL_HDR_MAGIC || pNtHeader->FileHeader.NumberOfSections == 0 ) {
		sError = PE_BAD_NT_HEADER;
		return NULL;
	}

	// calc the image size based on section sizes
	pSectionHeader = (PE_SECTION_HEADER*)( (uintptr_t)pNtHeader + 4 + sizeof( PE_FILE_HEADER ) + pNtHeader->FileHeader.SizeOfOptionalHeader );

	szImage = 0;
	for ( pSec = pSectionHeader; pSec - pSectionHeader < pNtHeader->FileHeader.NumberOfSections; pSec++ ) {		
		if ( ( pSec->VirtualAddress != 0 ) && ( pSec->VirtualAddress + pSec->VirtualSize > szImage ) ) {
			szImage = pSec->VirtualAddress + pSec->VirtualSize;
		}
	}

	if ( szImage < size ) {
		szImage = size;
	}

	// allocate the image memory, trying to allocate it on the requested image base first
#if defined( _WIN32 )
	pImage = VirtualAlloc( (void*)(pNtHeader->OptionalHeader.ImageBase), szImage, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if ( pImage == NULL ) { // it could've failed on allocating at the specified address, see if we can relocate
#else
	pImage = mmap( (void*)( pNtHeader->OptionalHeader.ImageBase ), szImage, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0 );
	if ( pImage == MAP_FAILED ) { // it could've failed on allocating at the specified address, see if we can relocate
#endif
		if ( pNtHeader->FileHeader.Characteristics & PE_FILE_RELOCS_STRIPPED ) { // we can't relocate
			sError = PE_CANT_RELOCATE;
			return NULL;			
		}
#if defined( _WIN32 )
		pImage = VirtualAlloc( NULL, szImage, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE );
		if ( pImage == NULL ) {
#else
		pImage = mmap( NULL, szImage, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
		if ( pImage == MAP_FAILED ) {
#endif
			sError = PE_CANT_ALLOCATE;
			return NULL;
		}
	}

	// copy everything before sections
	memcpy( pImage, mem, pSectionHeader->VirtualAddress );
	
	// retarget pointers to the image copy
	pDosHeader = (PE_DOS_HEADER*)pImage;
	pNtHeader = (PE_NT_HEADERS*)( (uintptr_t)pImage + pDosHeader->e_lfanew );
	pSectionHeader = (PE_SECTION_HEADER*)( (uintptr_t)pNtHeader + 4 + sizeof( PE_FILE_HEADER ) + pNtHeader->FileHeader.SizeOfOptionalHeader );

	// copy sections
	for ( pSec = pSectionHeader; pSec - pSectionHeader < pNtHeader->FileHeader.NumberOfSections; pSec++ ) {
		szSecSize = pSec->VirtualSize < pSec->SizeOfRawData ? pSec->VirtualSize : pSec->SizeOfRawData;
		if ( szSecSize != 0 ) {
			memcpy( (void*)( (uintptr_t)pImage + pSec->VirtualAddress ), (void*)( (uintptr_t)mem + pSec->PointerToRawData ), szSecSize );
		}
	}	
	
	if ( (uintptr_t)pImage != pNtHeader->OptionalHeader.ImageBase ) {
		// let's relocate
		PE_DATA_DIRECTORY		*pRelocDataDir;
		intptr_t				iOffset;
		PE_BASE_RELOCATION		*pReloc;

		pRelocDataDir = &pNtHeader->OptionalHeader.DataDirectory[PE_DIRECTORY_ENTRY_BASERELOC];
		if ( pRelocDataDir->VirtualAddress == 0 || pRelocDataDir->Size == 0 ) {
#if defined( _WIN32 )
			VirtualFree( pImage, 0, MEM_RELEASE );
#else
			munmap( pImage, szImage );
#endif
			sError = PE_RELOC_MISSING;
			return NULL;
		}
		iOffset = (uintptr_t)pImage - pNtHeader->OptionalHeader.ImageBase;
		pReloc = (PE_BASE_RELOCATION*)( (uintptr_t)pImage + pRelocDataDir->VirtualAddress);
		while ( pReloc->SizeOfBlock != 0 ) {
			int i, num;
			uint16_t fixup;
		
			num = ( pReloc->SizeOfBlock - sizeof( PE_BASE_RELOCATION ) ) / sizeof( uint16_t );
			for ( i = 0; i < num; i++ ) {
				fixup = pReloc->Fixups[i];
				switch ( ( fixup >> 12 ) & 0xF ) {
					case PE_REL_BASED_ABSOLUTE:
						break;
					case PE_REL_BASED_HIGHLOW:
					case PE_REL_BASED_DIR64:
						*(uintptr_t*)( (uintptr_t)pImage + pReloc->VirtualAddress + ( fixup & 0xFFF ) ) += iOffset;
						break;
					default:
#if defined( _WIN32 )
						VirtualFree( pImage, 0, MEM_RELEASE );
#else
						munmap( pImage, szImage );
#endif
						sError = PE_RELOC_NOT_SUPPORTED;
						return NULL;
				}
			}

			pReloc = (PE_BASE_RELOCATION*)( (uintptr_t)pReloc + pReloc->SizeOfBlock );
		}
	}

	// setup memory protection over sections
	for ( pSec = pSectionHeader; pSec - pSectionHeader < pNtHeader->FileHeader.NumberOfSections; pSec++ ) {
		prot = 0;
		if ( pSec->Characteristics & PE_SCN_CNT_CODE )		prot |= ( PROT_READ | PROT_EXEC );
		if ( pSec->Characteristics & PE_SCN_MEM_READ )		prot |= PROT_READ;
		if ( pSec->Characteristics & PE_SCN_MEM_WRITE )		prot |= PROT_WRITE;
		if ( pSec->Characteristics & PE_SCN_MEM_EXECUTE )	prot |= PROT_EXEC;
#if defined( _WIN32 )
		if ( VirtualProtect( (void*)( (uintptr_t)pImage + pSec->VirtualAddress ), pSec->VirtualSize, sProtect[prot], &dwProt ) == 0 ) {
			VirtualFree( pImage, 0, MEM_RELEASE );
			sError = PE_MRPROTECT_FAILED;
			return NULL;
		}
#else
		if ( mprotect( (void*)( (uintptr_t)pImage + pSec->VirtualAddress ), pSec->VirtualSize, prot ) ) {
			munmap( pImage, szImage );
			sError = PE_MRPROTECT_FAILED;
			return NULL;
	    }
#endif
	}

	// all good
#if defined( _WIN32 )
	pDesc = (PEDESC*)LocalAlloc( 0, sizeof( PEDESC ) );
#else
	pDesc = (PEDESC*)malloc( sizeof( PEDESC ) );
#endif
	pDesc->pImage = pImage;
	pDesc->szImage = szImage;

	sError = PE_OK;
	return pDesc;
}

int PE_FreeLibrary( PEHandle handle ) {
	PEDESC *pDesc;
	
	if ( handle != NULL ) {
		pDesc = (PEDESC*)handle;
#if defined( _WIN32 )
		VirtualFree( pDesc->pImage, 0, MEM_RELEASE );
		LocalFree( handle );
#else
		munmap( pDesc->pImage, pDesc->szImage );
		free( handle );
		sError = PE_OK;
#endif
		return 1;
	} else {
		sError = PE_INVALID_ARGUMENT;
		return 0;
	}
}

PEHandle PE_LoadLibrary( const char *filename ) {
#if defined( _WIN32 )
	HANDLE				hFile, hMap;
	DWORD				dwFileSize;
#else
	int					fd;
	struct stat			st;
#endif // _WIN32
	void				*mem;
	PEHandle			result;

	if ( filename == NULL ) {
		sError = PE_INVALID_ARGUMENT;
		return NULL;
	}

#if defined( _WIN32 )
	hFile = CreateFileA( filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		sError = PE_CANT_OPEN_FILE;
		return NULL;
	}

	dwFileSize = GetFileSize( hFile, NULL );
	if ( dwFileSize == INVALID_FILE_SIZE ) {
		sError = PE_CANT_STAT_FILE;
		return NULL;
	}

	hMap = CreateFileMappingA( hFile, NULL, PAGE_READONLY, 0 ,0, NULL );
	if ( hMap == NULL ) {
		CloseHandle( hFile );
		sError = PE_CANT_MAP_FILE;
		return NULL;
	}

	mem = MapViewOfFile( hMap, FILE_MAP_READ, 0, 0, 0 );
	if ( mem == NULL ) {
		CloseHandle( hMap );
		CloseHandle( hFile );
		sError = PE_CANT_MAP_FILE;
		return NULL;
	}
	result = PE_LoadLibraryFromMemory( mem, dwFileSize );

	UnmapViewOfFile( mem );
	CloseHandle( hMap );
	CloseHandle( hFile );
#else
	fd = open( filename, O_RDONLY );
	if ( fd < 0 ) {
		sError = PE_CANT_OPEN_FILE;
		return NULL;
	}

	if ( fstat( fd, &st ) < 0 ) {
		close( fd );
		sError = PE_CANT_STAT_FILE;
		return NULL;
	}

	mem = mmap( NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
	if ( mem == MAP_FAILED ) {
		close( fd );
		sError = PE_CANT_MAP_FILE;
		return NULL;
	}
	result = PE_LoadLibraryFromMemory( mem, st.st_size );

	munmap( mem, st.st_size );
	close( fd );
#endif // _WIN32

	return result;
}

PEProc PE_GetProcAddress( PEHandle handle, const char *procname ) {
	PEDESC					*pDesc;
	PE_DOS_HEADER			*pDosHeader;
	PE_NT_HEADERS			*pNtHeader;
	PE_DATA_DIRECTORY		*pExportDataDir;
	PE_EXPORT_DIRECTORY		*pExportTable;
	size_t					i;
	const char				*name;
	uint32_t				*pNameTable, *pFuncTable;

	if ( handle == NULL || procname == NULL ) {
		sError = PE_INVALID_ARGUMENT;
		return NULL;
	}

	pDesc = (PEDESC*)handle;

	pDosHeader = (PE_DOS_HEADER*)pDesc->pImage;
	pNtHeader = (PE_NT_HEADERS*)( (uintptr_t)pDesc->pImage + pDosHeader->e_lfanew );
	pExportDataDir = &pNtHeader->OptionalHeader.DataDirectory[PE_DIRECTORY_ENTRY_EXPORT];

	if ( pExportDataDir->Size == 0 ) {
		sError = PE_PROC_NOT_FOUND;
		return NULL;
	}

	pExportTable = (PE_EXPORT_DIRECTORY*)( (uintptr_t)pDesc->pImage + pExportDataDir->VirtualAddress );
	pNameTable = (uint32_t*)( (uintptr_t)pDesc->pImage + pExportTable->AddressOfNames );
	pFuncTable = (uint32_t*)( (uintptr_t)pDesc->pImage + pExportTable->AddressOfFunctions );

	for ( i = 0; i < pExportTable->NumberOfNames; i++ ) {
		name = (char*)( (uintptr_t)pDesc->pImage + pNameTable[i] );
		if ( !strcmp( name, procname ) ) {
			sError = PE_OK;
			return (void*)( (uintptr_t)pDesc->pImage + pFuncTable[i] );
		}
	}

	sError = PE_PROC_NOT_FOUND;
	return NULL;
}

const char* PE_ErrorMessage( int error ) {
	switch ( error ) {
		case PE_OK: return "Success";
		case PE_INVALID_ARGUMENT: return "Invalid argument";
		case PE_BAD_DOS_HEADER: return "Bad DOS header";
		case PE_BAD_NT_HEADER: return "Bad NT header";
		case PE_CANT_RELOCATE: return "Relocation info stripped";
		case PE_CANT_ALLOCATE: return "Can't allocate memory";
		case PE_RELOC_MISSING: return "Relocation info missing";
		case PE_RELOC_NOT_SUPPORTED: return "Relocation type not supported";
		case PE_MRPROTECT_FAILED: return "Cannot set memory protection";
		case PE_CANT_OPEN_FILE: return "Cannot open file";
		case PE_CANT_STAT_FILE: return "Cannot get file info";
		case PE_CANT_MAP_FILE: return "Cannot map file to memory";
		case PE_PROC_NOT_FOUND: return "Requested procedure not found";
	}
	return "Unknown error";
}

int PE_GetLastError( void ) {
	return sError;
}
