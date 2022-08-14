#if !defined( HUDLIB_H )
#define HUDLIB_H

// virtual dimensions for HUD drawing
#define VIRTUAL_WIDTH 1920
#define VIRTUAL_HEIGHT 1080
#define VIRTUAL_ASPECT (VIRTUAL_WIDTH/(float)(VIRTUAL_HEIGHT))

typedef struct {
	float left, top, right, bottom;			// visible area, use these values to stick to appripriate screen edges
	float xscale, yscale, xoffs, yoffs;		// needed to convert virtual coordinates to the real ones
} hudbounds_t;

typedef struct {
	const char *name;
	int line_height;
	int base;
	int scale_w, scale_h;
	int num_chars;
} fonthdr_t;

typedef struct {
	int id;
	int x, y;
	int width, height;
	int xoffset, yoffset;
	int xadvance;
} chardesc_t;

// font description, has to be generated from the XML produced by the BMFont utility
typedef struct {
	fonthdr_t hdr;
	chardesc_t *charmap[65536]; // TODO: optimize some day
	qhandle_t shader;
	chardesc_t chars[];
} font_t;

extern hudbounds_t hud_bounds;

// init the library
void hud_init( void );
// get the font specified, initializes it if needed
font_t *hud_font( const char *fontname );
// convert point coordinates from virtual to the real ones
void hud_translate_point( float *x, float *y );
// helper function to handle rectangles
void hud_translate_rect( float *x, float *y, float *w, float *h );
// draw rectangular picture and use specified texture coordinates
void hud_drawpic_ex( float x, float y, float width, float height, float s1, float t1, float s2, float t2, qhandle_t hShader );
// draw rectangular picture
// cx, cy are alignment control, i.e. 0.5 and 0.5 mean that (x,y) will point to the picture center
void hud_drawpic( float x, float y, float width, float height, float cx, float cy, qhandle_t hShader );
// draw bar of specified color
// cx, cy - see hud_drawpic()
void hud_drawbar( float x, float y, float width, float height, float cx, float cy, float *color );
// draw quad
void hud_drawquad(
	float x0, float y0, float s0, float t0,
	float x1, float y1, float s1, float t1,
	float x2, float y2, float s2, float t2,
	float x3, float y3, float s3, float t3,
	qhandle_t hShader
);
// returns string width if it should be rendered using the specified font
float hud_measurestring( float scale, font_t *font, const char *string );
float hud_measurecolorstring( float scale, font_t *font, const char *string );
// draws the string using specified font
float hud_drawstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy );
// advanced version of hud_drawstring, it handles q3 color codes
float hud_drawcolorstring( float x, float y, float scale, font_t *font, const char *string, float *shadow, float dx, float dy, qboolean forceColor );
// horizontal bar gauge used to indicate player's health and ammo
void hud_bargauge( int value, int basevalue, int barvalue,
	float x, float y,
	float width, float height,
	float space,
	float* whitecolor,
	float* redcolor,
	float* bluecolor );



#endif
