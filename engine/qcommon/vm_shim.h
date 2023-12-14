
#pragma once

// arQon:
// rather than have to cast the hell out of every single arg in every single trap
// (the "no implict conversion from void*" rule is so fkn annoying on PODs)  >:(
// we provide an explicit conversion to a VM_Arg, which then implicitly converts
// via operators to all the types we share with the VMs. CUNNING LIKE FOX  :P

// myT:
// Yeah... except for one rather important issue.
// When the type can't be deduced, no conversion operator is invoked at all.
// In a case like `va("Say hello to %s", VMA(1))`, `VMA(1)` doesn't return a pointer
// but a VM_Arg struct by value, which could be larger than just the intptr_t member.
// The simplest fix I could think of was to use #pragma pack for this.
// However, calling code should force an implicit conversion with a cast like so:
// `va("Say hello to %s", (const char*)VMA(1))`

#define VMA_CONVOP(T) operator T*() const { return (T*)p; }

#pragma pack(push, 1)
struct VM_Arg {
	VM_Arg( intptr_t x ) : p(x) {}
	intptr_t p;

	VMA_CONVOP( const char );
	VMA_CONVOP( char );
	VMA_CONVOP( void );
	VMA_CONVOP( byte );
	VMA_CONVOP( const byte );

	VMA_CONVOP( vmCvar_t );

	VMA_CONVOP( const vec3_t* );
	VMA_CONVOP( const vec3_t );
	VMA_CONVOP( const vec_t );
	VMA_CONVOP( vec3_t );
	VMA_CONVOP( vec_t );

	VMA_CONVOP( fileHandle_t );
	VMA_CONVOP( fontInfo_t );
	VMA_CONVOP( gameState_t );
	VMA_CONVOP( markFragment_t );
	VMA_CONVOP( orientation_t );
	VMA_CONVOP( playerState_t );
	VMA_CONVOP( qtime_t );
	VMA_CONVOP( trace_t );
	VMA_CONVOP( usercmd_t );

// known types are, understandably, a little inconsistent between vm's

#if defined(__TR_TYPES_H)
	VMA_CONVOP( glconfig_t );
	VMA_CONVOP( const polyVert_t );
	VMA_CONVOP( const refdef_t );
	VMA_CONVOP( const refEntity_t );
	VMA_CONVOP( snapshot_t );
#endif

#if defined(GAME_API_VERSION) // g_public, therefore game or cgame
	VMA_CONVOP( sharedEntity_t );
#else
	VMA_CONVOP( uiClientState_t );
#endif

	// all the bot stuff and the retarded crap that TA added
#if defined(GAME_API_VERSION) // g_public, therefore game or cgame
	VMA_CONVOP( struct aas_altroutegoal_s );
	VMA_CONVOP( struct aas_areainfo_s );
	VMA_CONVOP( struct aas_clientmove_s );
	VMA_CONVOP( struct aas_entityinfo_s );
	VMA_CONVOP( struct aas_predictroute_s );
	VMA_CONVOP( struct bot_consolemessage_s );
	VMA_CONVOP( struct bot_goal_s );
	VMA_CONVOP( struct bot_initmove_s );
	VMA_CONVOP( struct bot_match_s );
	VMA_CONVOP( struct bot_moveresult_s );
	VMA_CONVOP( struct weaponinfo_s );
	VMA_CONVOP( bot_entitystate_t );
	VMA_CONVOP( bot_input_t );
	VMA_CONVOP( pc_token_t );
#endif

#if !defined( QC )
	// this is not strictly necessary as it will fail otherwise,
	// but it improves the error message
#if __cplusplus >= 201103L
	template<typename T>
	operator T*() const
	{
		static_assert( false, "VMA_CONVOP missing for this type" );
		return (T*)p;
	}
#endif
#endif

};
#pragma pack(pop)

#undef VMA_CONVOP

#if __cplusplus >= 201103L
static_assert( sizeof(VM_Arg) == sizeof(intptr_t), "Invalid VM_Arg struct packing" );
#endif

#define VMA(x) VM_Arg(VM_ArgPtr(args[x]))

