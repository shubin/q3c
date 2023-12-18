// More portable way to pass the parameter to the native VM:
//  - all the syscalls accept a pointer to intptr array
//  - the very first element is the syscall id (callnum)
//  - C++ made it possible to keep the existing syntax with ellipsis

#if !defined( Q3_VM )

#if defined( __x86_64__ ) || defined( _M_X64 )
	#if defined( _MSC_VER )
		#define VMDECL
	#else
		#define VMDECL __attribute__((ms_abi)) // use MS ABI for all the callbacks/syscalls to make possible using DLLs on linux
	#endif
#else
	#define VMDECL __cdecl
#endif

typedef intptr_t ( VMDECL *syscall_t )    ( intptr_t *parms );
typedef syscall_t dllSyscall_t;
typedef void     ( VMDECL *dllEntry_t )   ( syscall_t syscallptr );

#if defined( TRAP_IMPL )

static syscall_t syscallptr;

template<typename ... Parms>
intptr_t syscall( intptr_t arg, Parms... parms  ) {
	intptr_t p[] = { arg, (intptr_t)parms ... };
	return syscallptr( p );
}

Q_EXPORT void dllEntry( syscall_t ptr ) {
	syscallptr = ptr;
}

#endif // TRAP_IMPL

#endif
