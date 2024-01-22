// More portable way to pass the parameter to the native VM:
//  - all the syscalls accept a pointer to intptr array
//  - the very first element is the syscall id (callnum)
//  - C++ made it possible to keep the existing syntax with ellipsis

#if !defined( Q3_VM )

#if defined( __x86_64__ ) || defined( _M_X64 )
	#if defined( _MSC_VER )
		#define VMCALL
	#else
		// use MS ABI for all the callbacks/syscalls to make possible using DLLs on linux
		#define VMCALL __attribute__((ms_abi))
	#endif
#else
	#define VMCALL __cdecl
#endif

typedef intptr_t ( VMCALL *syscall_t )    ( intptr_t *parms );
typedef syscall_t dllSyscall_t;
typedef void     ( VMCALL *dllEntry_t )   ( syscall_t syscallptr );

#if defined( TRAP_IMPL )

static syscall_t syscallptr;

template<typename T>
intptr_t sysarg( T a ) {
	return (intptr_t)a;
}

inline intptr_t sysarg( float a ) {
	union {
		float f;
		int i;
	} floatint;
	floatint.f = a;
	return (intptr_t)floatint.i;
}

template<typename ... Parms>
intptr_t syscall( intptr_t arg, Parms... parms  ) {
	intptr_t p[] = { arg, sysarg( parms ) ... };

	return syscallptr( p );
}

extern "C" Q_EXPORT void VMCALL dllEntry( syscall_t ptr ) {
	syscallptr = ptr;
}

#endif // TRAP_IMPL

#endif
