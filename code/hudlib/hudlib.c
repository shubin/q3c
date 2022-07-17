#include "../cgame/cg_local.h"
#include "hudlib.h"

// I'm gonna handle UTF8 someday
//#include "utf8.inc"

hudbounds_t hud_bounds;

// these *.inc files are generated from the XML produced by the BMFont utility
#include "font_regular.inc"
//#include "font_large.inc"
#include "font_qcde.inc"

static font_t *s_fonts[] = { &font_regular, &font_qcde, NULL };

static void hud_initfont( font_t *font ) {
	for ( int i = 0; i < sizeof( font->charmap ) / sizeof( font->charmap[0] ); i++ ) {
		font->charmap[i] = NULL;
	}
	font->shader = trap_R_RegisterShader( font->hdr.name );
}

font_t *hud_font( const char *fontname ) {
    for ( font_t **f = &s_fonts[0]; *f; f++ ) {
        if ( Q_stricmp( fontname, (*f)->hdr.name ) == 0 ) {
            if ( (*f)->shader == 0 ) {
                hud_initfont( *f );
            }
            return *f;
        }
    }
    return NULL;
}

// setup virtual coordinates
static void hud_initbounds( void ) {
	float aspv = VIRTUAL_ASPECT;
	float asp = ((float)cgs.glconfig.vidWidth)/((float)cgs.glconfig.vidHeight);
	float zz;
	if ( asp > aspv ) {
		hud_bounds.top = 0.0f;
		hud_bounds.bottom = VIRTUAL_HEIGHT;
		zz = 0.5f * VIRTUAL_HEIGHT * asp;
		hud_bounds.left = floorf(0.5f * VIRTUAL_WIDTH - zz);
		hud_bounds.right = VIRTUAL_WIDTH - hud_bounds.left;
	} else {
		hud_bounds.left = 0.0f;
		hud_bounds.right = VIRTUAL_WIDTH;;
		zz = 0.5f * VIRTUAL_WIDTH / asp;
		hud_bounds.top = floorf(0.5f * VIRTUAL_HEIGHT - zz);
		hud_bounds.bottom = VIRTUAL_HEIGHT - hud_bounds.top;
	}
	hud_bounds.xscale = cgs.glconfig.vidWidth / ( hud_bounds.right - hud_bounds.left );
	hud_bounds.yscale = cgs.glconfig.vidHeight / ( hud_bounds.bottom - hud_bounds.top );
	hud_bounds.xoffs = hud_bounds.xscale * hud_bounds.left;
	hud_bounds.yoffs = hud_bounds.yscale * hud_bounds.top;
}

void hud_init( void ) {
    hud_initbounds();
}

// convert point coordinates from virtual to the real ones
void hud_translate_point( float *x, float *y ) {
	*x = *x * hud_bounds.xscale - hud_bounds.xoffs;
	*y = *y * hud_bounds.yscale - hud_bounds.yoffs;
}

// helper function to handle rectangles
void hud_translate_rect( float *x, float *y, float *w, float *h ) {
	float r, b;
	r = *x + *w;
	b = *y + *h;
	hud_translate_point( x, y );
	hud_translate_point( &r, &b );
	*w = r - *x;
	*h = b - *y;
}

// draw rectangular picture and use specified texture coordinates
void hud_drawpic_ex( 
	float x, float y, float width, float height, 
	float s1, float t1, float s2, float t2,
	qhandle_t hShader )
{
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, hShader );
}

// draw rectangular picture
// cx, cy are alignment control, i.e. 0.5 and 0.5 mean that (x,y) will point to the picture center
void hud_drawpic( float x, float y, float width, float height, float cx, float cy, qhandle_t hShader ) {
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x - cx * width, y - cy * height, width, height, 0, 0, 1, 1, hShader );
}

// draw bar of specified color
// cx, cy - see hud_drawpic()
void hud_drawbar( float x, float y, float width, float height, float cx, float cy, float *color ) {
	hud_translate_rect( &x, &y, &width, &height );
	trap_R_SetColor( color );
	trap_R_DrawStretchPic( x - cx * width, y - cy * height, width, height, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_SetColor( NULL );
}
// find character by its id
static chardesc_t* find_char( font_t *font, int c ) {
	if ( c >= 0 && c < sizeof( font->charmap ) / sizeof( font->charmap[0] ) ) {
		if ( font->charmap[c] != NULL ) {
			return font->charmap[c];
		}
		for ( int i = 0; i < font->hdr.num_chars; i++ ) {
			if ( font->chars[i].id == c ) {
				font->charmap[c] = &font->chars[i];
				return &font->chars[i];
			}
		}
	}
	return NULL;
}

// returns horizontal advance
static float hud_drawchar( float x, float y, float scale, font_t *font, int c ) {
	chardesc_t *chr;
	float tx, ty;
	hud_translate_point( &x, &y );
	chr = find_char( font, c );
	if ( chr != NULL ) {
		tx = chr->x / (float)font->hdr.scale_w;
		ty = chr->y / (float)font->hdr.scale_h;
		trap_R_DrawStretchPic( 
			x, y - ( font->hdr.base - chr->yoffset ) * hud_bounds.yscale * scale,
			chr->width * hud_bounds.xscale * scale, chr->height * hud_bounds.yscale * scale,
			tx, 1.0f - ty, 
			tx + chr->width / (float)font->hdr.scale_w, 1.0f - ( ty + chr->height / (float)font->hdr.scale_h ), 
			font->shader
		);
		return chr->xadvance * scale;
	}
	return 0.0f;
}

// returns string width if it should be rendered using the specified font
float hud_measurestring( float scale, font_t *font, const char *string ) {
	float advance;
	const char *c;
	chardesc_t *chr;

	advance = 0;
	for ( c = string; *c; c++ ) {
		chr = find_char( font, *c );
		if ( chr != NULL ) {
			advance += chr->xadvance * scale;
		}
	}
	return advance;
}

// draws the string using specified font
float hud_drawstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy ) {
	float advance;
	float color[4];
	const char *c;

	if ( shadow != NULL ) {
		memcpy( color, cg.lastColor, sizeof( color ) );
		trap_R_SetColor( shadow );
		advance = x + dx;
		for ( c = string; *c; c++ ) {
			advance += hud_drawchar( advance, y + dy, scale, font, *c );
		}
		trap_R_SetColor( color );
	}
	advance = x;
	for ( c = string; *c; c++ ) {
		advance += hud_drawchar( advance, y, scale, font, *c );
	}
	return advance - x;
}

float hud_measurecolorstring(float scale, font_t *font, const char *string ) {
	chardesc_t *chr;
	const char	*s;
	int			xx;
	int			cnt;

	// draw the colored text
	s = string;
	xx = 0;
	cnt = 0;
	while ( *s && cnt < 32768 ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		chr = find_char( font, *s );
		xx += chr->xadvance * scale;
		cnt++;
		s++;
	}
	return xx;
}

// advanced version of hud_drawstring, it handles q3 color codes
float hud_drawcolorstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy, qboolean forceColor ) {
	vec4_t		color, shad;
	const char	*s;
	int			xx;
	int			cnt;
	float		alpha;

	alpha = cg.lastColor[3];

	// draw the shadow
	if ( shadow != NULL ) {
		memcpy( shad, shadow, sizeof( float ) * 3 );
		shad[3] = alpha;
		trap_R_SetColor( shad );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < 32768 ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			xx += hud_drawchar( xx + dx, y + dy, scale, font, *s );
			cnt++;
			s++;
		}
	}
	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	if ( forceColor ) {
		memcpy( color, cg.lastColor, sizeof( float ) * 4 );
	} else {
		color[0] = color[1] = color[2] = 1.0f;
		color[3] = alpha;
	}
	trap_R_SetColor( color );
	while ( *s && cnt < 32768 ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( float ) * 3 );
				color[3] = alpha;
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		xx += hud_drawchar( xx, y, scale, font, *s );
		cnt++;
		s++;
	}
	return xx - x;
}

// horizontal bar gauge used to indicate player's health and ammo
void hud_bargauge(
	int value,
	int basevalue,
	int barvalue,
	float x, float y,
	float width, float height,
	float space,
	float* whitecolor,
	float* redcolor,
	float* bluecolor
	)
{
	int i;
	int whitebars, redbars, bluebars;

	if ( value < 0 ) {
		value = 0;
	}

	redbars = basevalue / barvalue;
	whitebars = value / barvalue;
	bluebars = 0;
	if ( whitebars > redbars ) {
		whitebars = redbars;
		bluebars = (int)ceilf(  ( value - basevalue ) / (float)barvalue );
	}
	redbars -= whitebars;
	for ( i = 0; i < whitebars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, whitecolor );
		x += ( width + space );
	}
	for ( i = 0; i < redbars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, redcolor );
		x += ( width + space );
	}
	for ( i = 0; i < bluebars; i++) {
		hud_drawbar( x, y, width, height, 0.0f, 0.0f, bluecolor );
		x += ( width + space );
	}
}
