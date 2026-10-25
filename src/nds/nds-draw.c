#include "nds-draw.h"

#ifdef __3DS__
# include <3ds.h>
#else
# include <nds.h>
#endif

/*
 * The 5x8 font, on both machines.  The DS had a 3x8 one, which gets 85 columns
 * into 256 pixels by colouring each glyph's pixels to light single red, green
 * and blue stripes of the LCD.  Nobody could read it, in an emulator least of
 * all, so the DS pans across a wider term instead.
 */
const nds_font_handle *nds_font = &nds_font_5x8;

#ifndef __3DS__
/*
 * The top screen's window onto the term stops in three places, at the left,
 * the middle and the right, so that what the screen shows is never far from
 * one of three familiar views.  It slides between them rather than jumping, so
 * that the eye can follow where it went.  Over the map it does not stop but
 * keeps the player in the middle.
 */
#define NDS_PAN_MAX	(NDS_TERM_WIDTH - NDS_SCREEN_WIDTH)
#define NDS_PAN_STEP	(NDS_PAN_MAX / 2)
#define NDS_PAN_SPEED	12
/* How close to an edge of the screen something may come before it pans. */
#define NDS_PAN_MARGIN	30

/* Where the window is, and where it is sliding to, in pixels. */
static volatile int nds_pan, nds_pan_target;

/* Frames shown, counted between them. */
static volatile u32 nds_frames;

/* Where the top and bottom lines stay while the rest pans, or -1 to pan. */
static volatile int nds_pan_top = -1, nds_pan_bottom = -1;

/*
 * Over the map, the message line at the top and the status line at the bottom
 * stay put while the rows between them pan: the game writes each from a fixed
 * column, and centring the player would take the start of every message out of
 * view.  The scroll register is changed partway down the frame, as the second
 * line and the last begin and as the frame ends, from the interrupt for
 * reaching a scanline.  If a change lands a scanline late, it is on the top row
 * of a glyph's pixels, which two glyphs in the font use.
 */
static void nds_video_line(void)
{
	int line = REG_VCOUNT, last = NDS_SCREEN_HEIGHT - nds_font->height;
	int pan = nds_pan;

	if (line < last) {
		REG_BG2X = pan << 8;
		SetYtrigger(last);
	} else if (line < NDS_SCREEN_HEIGHT) {
		REG_BG2X = (nds_pan_bottom < 0 ? pan : nds_pan_bottom) << 8;
		SetYtrigger(NDS_SCREEN_HEIGHT);
	} else {
		/*
		 * Between frames, slide towards where the window is going: here
		 * rather than in the main loop, so that it keeps moving while the game
		 * is busy, as it is after rebuilding the surface.
		 */
		int target = nds_pan_target;

		if (pan < target)
			nds_pan = MIN(pan + NDS_PAN_SPEED, target);
		else if (pan > target)
			nds_pan = MAX(pan - NDS_PAN_SPEED, target);

		REG_BG2X = (nds_pan_top < 0 ? nds_pan : nds_pan_top) << 8;
		SetYtrigger(nds_font->height);
		nds_frames++;
	}
}
#endif

void nds_video_init() {
#ifdef __3DS__
	gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSetDoubleBuffering(GFX_TOP, false);
#else
	powerOn(POWER_ALL_2D | POWER_SWAP_LCDS);
	videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	/* A and B are the top screen's 512x256 bitmap, 128 KB each */
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	/* D to I are heap, when there is no RAM in slot 2: nds-slot2-virt.c */
	vramSetBankD(VRAM_D_LCD);
	vramSetBankE(VRAM_E_LCD);
	vramSetBankF(VRAM_F_LCD);
	vramSetBankG(VRAM_G_LCD);
	vramSetBankH(VRAM_H_LCD);
	vramSetBankI(VRAM_I_LCD);
	/* The term does not reach the bitmap's right edge; keep it black */
	memset(BG_GFX, 0, NDS_TOP_PITCH * NDS_SCREEN_HEIGHT * sizeof(nds_pixel));
	REG_BG2CNT = BG_BMP16_512x256;
	REG_BG2PA = 1 << 8;
	REG_BG2PB = 0;
	REG_BG2PC = 0;
	REG_BG2PD = 1 << 8;
	REG_BG2Y = 0;
	REG_BG2X = 0;
	irqSet(IRQ_VCOUNT, nds_video_line);
	SetYtrigger(nds_font->height);
	irqEnable(IRQ_VCOUNT);
	REG_BG2CNT_SUB = BG_BMP16_256x256 | BG_BMP_BASE(2);
	REG_BG2PA_SUB = 1 << 8;
	REG_BG2PB_SUB = 0;
	REG_BG2PC_SUB = 0;
	REG_BG2PD_SUB = 1 << 8;
	REG_BG2Y_SUB = 0;
	REG_BG2X_SUB = 0;
#endif
}

void nds_video_vblank() {
#ifdef __3DS__
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
#else
	swiWaitForVBlank();
#endif
}

void nds_video_show(int x, int w)
{
#ifndef __3DS__
	/* The margin does not apply at the ends of the term */
	int left = nds_pan_target ? nds_pan_target + NDS_PAN_MARGIN : 0;
	int right = nds_pan_target < NDS_PAN_MAX ?
		nds_pan_target + NDS_SCREEN_WIDTH - NDS_PAN_MARGIN : NDS_TERM_WIDTH;
	int want;

	/* Leave the window alone while it already shows them */
	if (x >= left && x + w <= right)
		return;

	/* Otherwise, the stop which puts them nearest the middle */
	want = x + w / 2 - NDS_SCREEN_WIDTH / 2;
	want = MAX(0, MIN(want, NDS_PAN_MAX));
	nds_pan_target = (want + NDS_PAN_STEP / 2) / NDS_PAN_STEP * NDS_PAN_STEP;
#endif
}

void nds_video_centre(int x, int w)
{
#ifndef __3DS__
	int want = x + w / 2 - NDS_SCREEN_WIDTH / 2;

	nds_pan_target = MAX(0, MIN(want, NDS_PAN_MAX));
#endif
}

unsigned int nds_video_frames(void)
{
#ifdef __3DS__
	return 0;
#else
	return nds_frames;
#endif
}

void nds_video_split(int top, int bottom)
{
#ifndef __3DS__
	nds_pan_top = top;
	nds_pan_bottom = bottom;
#endif
}

void nds_video_pan_step(int dir)
{
#ifndef __3DS__
	int stop = (nds_pan_target + NDS_PAN_STEP / 2) / NDS_PAN_STEP + dir;

	nds_pan_target = MAX(0, MIN(stop * NDS_PAN_STEP, NDS_PAN_MAX));
#endif
}

/*
 * The framebuffer that row y is on, with y made relative to it, and the
 * distance in pixels from one row of it to the next.
 */
static inline nds_pixel *nds_get_framebuffer(uint16_t *y, int *y_pitch) {
#ifdef __3DS__
	nds_pixel *fb = (nds_pixel *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	*y_pitch = NDS_Y_PITCH;
#else
	nds_pixel *fb = BG_GFX;
	*y_pitch = NDS_TOP_PITCH;
#endif

	/* Bottom screen? */
	if (*y >= NDS_SCREEN_HEIGHT) {
#ifdef __3DS__
		fb = (nds_pixel *) gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
#else
		fb = &BG_GFX_SUB[16 * 1024];
		*y_pitch = NDS_SCREEN_WIDTH;
#endif
		*y -= NDS_SCREEN_HEIGHT;
	}

	return fb;
}

void nds_draw_pixel(uint16_t x, uint16_t y, nds_pixel data) {
	int y_pitch;
	nds_pixel *fb = nds_get_framebuffer(&y, &y_pitch);

#ifdef __3DS__
	fb[x * NDS_SCREEN_HEIGHT + (NDS_SCREEN_HEIGHT - y - 1)] = data;
#else
	fb[y * y_pitch + x] = data;
#endif
}

void nds_draw_char_px(uint16_t x, uint16_t y, char c, nds_pixel clr_fg, nds_pixel clr_bg)
{
	int y_pitch;
	nds_pixel *fb = nds_get_framebuffer(&y, &y_pitch);

#ifdef __3DS__
	nds_font->draw_char(c, fb + (x * NDS_SCREEN_HEIGHT) + (NDS_SCREEN_HEIGHT - y - 1), y_pitch, clr_fg, clr_bg);
#else
	nds_font->draw_char(c, fb + (y * y_pitch) + x, y_pitch, clr_fg, clr_bg);
#endif
}

void nds_draw_str_px(uint16_t x, uint16_t y, const char *str, nds_pixel clr_fg, nds_pixel clr_bg)
{
	while (*str != '\0') {
		nds_draw_char_px(x, y, *(str++), clr_fg, clr_bg);
		x += nds_font->width;
	}
}

void nds_draw_char(uint8_t x, uint8_t y, char c, nds_pixel clr_fg, nds_pixel clr_bg)
{
	nds_draw_char_px(x * nds_font->width, y * nds_font->height, c, clr_fg, clr_bg);
}

void nds_draw_str(uint8_t x, uint8_t y, const char *str, nds_pixel clr_fg, nds_pixel clr_bg)
{
	nds_draw_str_px(x * nds_font->width, y * nds_font->height, str, clr_fg, clr_bg);
}

void nds_draw_cursor(int x, int y) {
	for (uint8_t xx = 0; xx < nds_font->width; xx++) {
		nds_draw_pixel(x * nds_font->width + xx,
		               y * nds_font->height,
		               NDS_CURSOR_COLOR);
		nds_draw_pixel(x * nds_font->width + xx,
		               y * nds_font->height + (nds_font->height - 1),
		               NDS_CURSOR_COLOR);
	}
	for (uint8_t yy = 0; yy < nds_font->height; yy++) {
		nds_draw_pixel(x * nds_font->width,
		               y * nds_font->height + yy,
		               NDS_CURSOR_COLOR);
		nds_draw_pixel(x * nds_font->width + (nds_font->width - 1),
		               y * nds_font->height + yy,
		               NDS_CURSOR_COLOR);
	}
}

/*
 * Find the square a particular pixel is part of.
 */
void nds_pixel_to_square(int *const x, int *const y, const int ox,
                         const int oy)
{
	(*x) = ox / nds_font->width;
	(*y) = oy / nds_font->height;
}

void nds_log(const char *msg)
{
	static uint8_t x = 2, y = 1;

	/* This is how the game says why it stopped, so it must be seen */
	nds_video_show(0, nds_font->width);

	for (uint8_t i = 0; msg[i] != '\0'; i++) {
		nds_draw_char(x, y, msg[i], NDS_WHITE_PIXEL, NDS_BLACK_PIXEL);
		x++;
		if (msg[i] == '\n' || x > NDS_SCREEN_COLS - 2) {
			x = 2;
			y++;
		}
	}
}

void nds_logf(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	int len = vsnprintf(NULL, 0, format, args);

	char buf[len];

	vsprintf(buf, format, args);
	nds_log(buf);

	va_end(args);
}

void nds_raw_print(const char *str)
{
	static uint16_t x = 0, y = 32;
	while (*str) {
		nds_draw_char(x, y, *(str++), NDS_WHITE_PIXEL, NDS_BLACK_PIXEL);
		x++;
		if (x > NDS_SCREEN_COLS - 2) {
			x = 0;
			y++;
			if (y > 34)
				y = 32;
		}
	}
	nds_draw_char(x, y, 219, NDS_WHITE_PIXEL, NDS_BLACK_PIXEL);
}
