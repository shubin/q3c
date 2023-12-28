#if defined( __x86_64__ ) || defined( _M_X64 )
	#if defined( _MSC_VER )
		#define VMCALL
	#else
		#define VMCALL __attribute__((ms_abi))
	#endif
#else
	#define VMCALL __cdecl
#endif


// New approach to pass function args to VM or get a call from VM: we use a pointer to the array of arguments!
// - easy to use
// - portable
// - caller puts and callee gets just exact number of arguments according to the particular syscall semantics
// - syscall ID is the first argument
// - C++ makes possible using the same syntax
// - can even use DLLs on Linux if they're built with no dependencies

typedef intptr_t ( VMCALL *syscall_t )    ( intptr_t *parms );
typedef syscall_t dllSyscall_t;
typedef void     ( VMCALL *dllEntry_t )   ( syscall_t syscallptr );

#if defined( TRAP_IMPL )

static syscall_t syscallptr;

template<typename ... Parms>
intptr_t syscall( intptr_t arg, Parms... parms  ) {
	intptr_t p[] = { arg, (intptr_t)parms ... };
	return syscallptr( p );
}

extern "C" Q_EXPORT void VMCALL dllEntry( syscall_t ptr ) {
	syscallptr = ptr;
}

#endif // TRAP_IMPL
