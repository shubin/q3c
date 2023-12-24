#include <RmlUi/Core.h>

extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"
#include "keycodes.h"
#undef DotProduct

#define kmap(qkey, rmlkey)					if ( key == qkey ) { *ki = rmlkey; }
#define krange(qlow, qhigh, rmllow)			if ( key >= qlow && key <= qhigh ) { *ki = rmllow + ( key - qlow ); }
#define cmap(qkey, ch)						if ( key == qkey ) *chr = ch;
#define crange(qlow, qhigh, chlow)			if ( key >= qlow && key <= qhigh ) { *chr = chlow + ( key - qlow ); }		
#define ckmap(qkey, rmlkey, ch)				if ( key == qkey ) { *ki = rmlkey; *chr = ch; }
#define ckrange(qlow, qhigh, rmllow, chlow)	if ( key>= qlow && key <= qhigh ) { *ki = rmllow + ( key - qlow ); *chr = chlow + ( key - qlow ); }

using namespace Rml::Input;

void UI_MapKey( int key, int *ki, int *chr ) {
	*ki = KI_UNKNOWN;
	*chr = 0;
	
	ckrange( '0', '9', KI_0, '0' );
	ckrange( 'A', 'Z', KI_A, 'A' );
	ckrange( 'a', 'z', KI_A, 'a' );
	crange( 0x20, 0x7E, 0x20 );
	kmap( K_TAB, KI_TAB );
	ckmap( K_ENTER, KI_RETURN, '\n');
	kmap( K_LEFTARROW, KI_LEFT );
	kmap( K_RIGHTARROW, KI_RIGHT );
	kmap( K_UPARROW, KI_UP );
	kmap( K_DOWNARROW, KI_DOWN );
	kmap( K_ESCAPE, KI_ESCAPE );
	kmap( K_BACKSPACE, KI_BACK );
}
