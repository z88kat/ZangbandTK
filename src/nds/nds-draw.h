#ifndef _NDS_DRAW_H
#define _NDS_DRAW_H

#include "../h-basic.h"


#ifdef __3DS__

/* RGBA8 */
typedef uint32_t nds_pixel;
#define NDS_WHITE_PIXEL 0xffffffff
#define NDS_BLACK_PIXEL 0x000000ff
#define NDS_CURSOR_COLOR 0xffff00ff

#define NDS_SCREEN_WIDTH 400
#define NDS_SCREEN_HEIGHT 240

/* The top screen holds all 80 columns of the term. */
#define NDS_TERM_WIDTH NDS_SCREEN_WIDTH

#define NDS_X_PITCH	NDS_SCREEN_HEIGHT
#define NDS_Y_PITCH	-1

#else

typedef uint16_t nds_pixel;
#define NDS_WHITE_PIXEL 0xffff
#define NDS_BLACK_PIXEL 0x0000
#define NDS_CURSOR_COLOR 0x83ff

#define NDS_SCREEN_WIDTH 256
#define NDS_SCREEN_HEIGHT 192

/*
 * The term is 80 columns of the 5x8 font, 400 pixels, and the screen is 256.
 * So the top screen's bitmap is 512 wide and the screen is a window onto it,
 * which the hardware pans: see nds_video_show().  The bottom screen's bitmap
 * is the width of the screen.
 */
#define NDS_TERM_WIDTH 400
#define NDS_TOP_PITCH 512

#define NDS_X_PITCH	1

#endif

typedef struct {
	uint8_t width;
	uint8_t height;
	void (*draw_char)(char c, nds_pixel *pixels, int y_pitch, nds_pixel clr_fg, nds_pixel clr_bg);
} nds_font_handle;

extern const nds_font_handle *nds_font;

extern const nds_font_handle nds_font_3x8;
extern const nds_font_handle nds_font_5x8;

/* Columns the screen shows, and columns the term has: the same on the 3DS. */
#define NDS_SCREEN_COLS (NDS_SCREEN_WIDTH / nds_font->width)
#define NDS_TERM_COLS (NDS_TERM_WIDTH / nds_font->width)
#define NDS_SCREEN_LINES (NDS_SCREEN_HEIGHT / nds_font->height)

void nds_video_init();
void nds_video_vblank();

/* Pan the top screen to keep pixels x to x + w - 1 of the term in view. */
void nds_video_show(int x, int w);
/* Pan the top screen to put pixels x to x + w - 1 of the term in its middle. */
void nds_video_centre(int x, int w);
/* Pan the top screen a step left (-1) or right (1). */
void nds_video_pan_step(int dir);
/*
 * Keep the top and bottom lines of the top screen showing the term from pixel
 * top and bottom when the rest pans, or -1 for either to pan with it.
 */
void nds_video_split(int top, int bottom);
/* How many frames have been shown; it only matters that it changes. */
unsigned int nds_video_frames(void);

void nds_draw_pixel(uint16_t x, uint16_t y, nds_pixel data);

/* Same as nds_draw_char, but x/y is pixels instead of tiles */
void nds_draw_char_px(uint16_t x, uint16_t y, char c, nds_pixel clr_fg, nds_pixel clr_bg);
void nds_draw_char(uint8_t x, uint8_t y, char c, nds_pixel clr_fg, nds_pixel clr_bg);

void nds_draw_str_px(uint16_t x, uint16_t y, const char *str, nds_pixel clr_fg, nds_pixel clr_bg);
void nds_draw_str(uint8_t x, uint8_t y, const char *str, nds_pixel clr_fg, nds_pixel clr_bg);

void nds_draw_cursor(int x, int y);

void nds_pixel_to_square(int *const x, int *const y, const int ox,
                         const int oy);

void nds_log(const char *msg);
void nds_logf(const char* format, ...);
void nds_raw_print(const char *str);

#endif
