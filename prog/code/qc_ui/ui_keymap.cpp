#include <RmlUi/Core.h>

extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"
#include "keycodes.h"
#undef DotProduct

#define kmap(qkey, rmlkey)					if ( key == qkey ) { ki = rmlkey; }
#define krange(qlow, qhigh, rmllow)			if ( key >= qlow && key <= qhigh ) { ki = rmllow + ( key - qlow ); }

using namespace Rml::Input;

int UI_MapKey( int key ) {
	int ki = KI_UNKNOWN;
	
	krange( '0', '9', KI_0 );
	krange( 'A', 'Z', KI_A );
	krange( 'a', 'z', KI_A );
	krange( K_F1, K_F24, KI_F1 );
	kmap( K_TAB, KI_TAB );
	kmap( K_ENTER, KI_RETURN);
	kmap( K_LEFTARROW, KI_LEFT );
	kmap( K_RIGHTARROW, KI_RIGHT );
	kmap( K_UPARROW, KI_UP );
	kmap( K_DOWNARROW, KI_DOWN );
	kmap( K_ESCAPE, KI_ESCAPE );
	kmap( K_BACKSPACE, KI_BACK );
	kmap( K_CAPSLOCK, KI_CAPITAL );
	kmap( K_INS, KI_INSERT );
	kmap( K_HOME, KI_HOME );
	kmap( K_PGUP, KI_PRIOR );
	kmap( K_DEL, KI_DELETE );
	kmap( K_END, KI_END );
	kmap( K_PGDN, KI_NEXT );
	return ki;
}
