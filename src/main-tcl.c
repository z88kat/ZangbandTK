/**
 * \file main-tcl.c
 * \brief The Tcl/Tk front end.
 *
 * Copyright (c) 2026 Steven Pannell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

#ifdef USE_TCL

#include "buildid.h"
#include "cmd-core.h"
#include "game-event.h"
#include "cmds.h"
#include "game-input.h"
#include "game-world.h"
#include "player-calcs.h"
#include "grafmode.h"
#include "init.h"
#include "mon-lore.h"
#include "obj-desc.h"
#include "obj-info.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-gear.h"
#include "obj-power.h"
#include "obj-knowledge.h"
#include "obj-util.h"
#include "player-history.h"
#include "player-timed.h"
#include "player-util.h"
#include "player-mutation.h"
#include "obj-ignore.h"
#include "player-spell.h"
#include "store.h"
#include "player-virtue.h"
#include "savefile.h"
#include "score.h"
#include "trap.h"
#include "ui-knowledge.h"
#include "ui-birth.h"
#include "ui-command.h"
#include "ui-death.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-help.h"
#include "main.h"
#include "option.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-menu.h"
#include "ui-mon-lore.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-spell.h"
#include "ui-term.h"
#include "wild.h"
#include "z-textblock.h"
#ifdef SOUND
#include "sound.h"
#endif

#include <tcl.h>
#include <tk.h>

#ifdef __APPLE__
/* In tcl/menukeys.c, which has to reach past Tk to AppKit. */
extern void tcl_menu_keys_literal(void);
#ifdef SOUND_COCOA
/* In tcl/music.m, which plays the player's own music through NSSound. */
extern bool tcl_music_play(const char *path, double volume);
extern void tcl_music_stop(void);
extern void tcl_music_pause(bool pause);
extern void tcl_music_volume(double volume);
extern const char *tcl_music_state(double *position, double *length);
#endif
/* In tcl/keyboard.c, which asks Carbon what the keyboard types. */
extern void tcl_keyboard_char(int vk, int shifted, char *out, size_t len);
extern const char *tcl_keyboard_type(void);
#endif

/*
 * This is ZangbandTK/Tk -- the front end the whole of Phase 3 is about.  It is
 * a second macOS application beside the Cocoa one rather than a replacement
 * for it; see .claude/plans/phase3-tcl-tk-frontend.md, decision 14.
 *
 * Milestone T0, which is all that is here: embed a Tcl interpreter, bring up
 * Tk, open one window, and let the game's main loop drive it.  The term hooks
 * below are deliberately the smallest thing that keeps ui-term.c happy -- T1
 * gives them a real grid to draw on.  Nothing here draws the map yet.
 *
 * Two structural points that outlive T0:
 *
 *   1. The game keeps its main loop (DEC-14).  Tk's event loop is not entered;
 *      instead Tcl_DoOneEvent is pumped from Term_xtra, which is the seam that
 *      lets a Tk window coexist with a program that was written to block on
 *      getch().  Never call Tk_MainLoop here.
 *
 *   2. The front end links the whole text UI and overrides its hooks one at a
 *      time, after textui_init() has installed them.  That is what makes the
 *      later milestones independent of each other: each native dialog is one
 *      hook reassignment, and anything not yet converted keeps working.
 */

/**
 * The one window, for now.
 */
static Tcl_Interp *interp = NULL;
static Tk_Window mainwin = NULL;

/**
 * A term and the canvas it draws into.
 *
 * The grid is one canvas text item per cell, created once and reconfigured as
 * the game writes.  That is 1,920 items for an 80x24 term, which Tk handles
 * without complaint, and it keeps T1 entirely inside core Tk -- no custom
 * canvas item, no private header.  T2 revisits rendering anyway when tiles
 * arrive, so the cheapest correct thing now is the right thing now.
 */
typedef struct term_data term_data;
struct term_data {
	term t;
	char path[64];		/* the canvas widget, e.g. ".pw.top.main.c" */
	char font[32];		/* the pane's font, which decides its cell size */
	int cols;
	int rows;
	int cw;			/* cell width in pixels */
	int ch;			/* cell height */
	int ascent;
	int *item;		/* canvas item id per cell, row-major */
	int cursor_item;
	int image_item;		/* the tile layer, one canvas image under the text */
	Tk_PhotoHandle screen;	/* what that image shows */
	char screen_name[64];	/* the photo's name, to delete it by */
	bool cursor_visible;
};

/*
 * One per term.  Slot 0 is the map; the rest are the game's subwindows, whose
 * contents textui_init() has already chosen -- messages, inventory, monster
 * list, item list, recall, overhead map.  main.tcl decides how many there are
 * and where they go, and hands back the list of canvases.
 */
static term_data td[ANGBAND_TERM_MAX];
static int td_count = 0;

/*
 * Which of the game's subwindows each pane is, by pane order.
 *
 * textui_init() decides what a subwindow contains from its *index* -- 1 is
 * messages, 2 inventory, 3 the monster list, 4 the item list, 5 recall, 6 the
 * overhead map -- and it does so after the front end has been initialised,
 * overwriting anything set beforehand.  Rather than fight that, each pane is
 * given the index whose content it wants, and the indices nobody asked for
 * are left without a term at all: subwindows_set_flags skips those, and the
 * game is used to front ends with fewer windows than the maximum.
 *
 * The alternative was to set window_flag ourselves once textui_init had
 * finished.  That was tried: the flags took, and the panes still drew their
 * old contents, so there is more to it than the flags.  This way there is
 * nothing to apply, nothing to time, and only one opinion about what a pane
 * holds.
 */
static int pane_index[ANGBAND_TERM_MAX];

/*
 * The window flags each pane wants, applied once the game has settled.
 *
 * For most panes this is exactly what textui_init() would have chosen from
 * the index anyway, and applying it changes nothing.  The minimap is the
 * exception and the reason this exists: PW_MAP is the scaled whole-level map
 * -- update_maps() hands that one straight to display_map() -- and no
 * subwindow carries it by default, so it cannot be had by placement alone.
 *
 * It has to be applied after textui_init(), which runs once the front end is
 * up and rewrites window_flag wholesale, and after the splash screen, whose
 * drawing is the first thing to reach Term_xtra.  The first request for input
 * is safely past both.
 */
static uint32_t want_flag[ANGBAND_TERM_MAX];
static bool flags_applied = false;

/*
 * The tile sheet, and a scratch cell to scale into.
 *
 * Tk 9 reads PNG itself, so there is no Img extension to find and no file
 * format to decode by hand -- one of the things core Tk does now that the 2001
 * front end needed a binary extension for.
 */
/* Defined below, beside the rest of the tile code. */
static void clear_tiles(term_data *td, int x, int y, int n);

/*
 * The tile sheet, as our own RGBA pixels rather than a Tk photo.
 *
 * Tk decodes a PNG with any transparency in it very slowly -- measured on this
 * machine, David Gervais' 4096x992 sheet takes 8.6 seconds, and the same file
 * with every alpha byte set to 255 takes 36.  It is the transparency and not
 * the size: the Neon sheet is fully opaque and loads in 10ms, and snapping
 * Gervais' alpha to 0 or 255 changes nothing, so it is any non-opaque pixel at
 * all that costs.
 *
 * We never display the sheet -- it is only ever a source of pixels to copy
 * from -- so it does not need to be a Tk image.  Decoding it ourselves avoids
 * the whole problem, and it is also what generated darkening will need when
 * that arrives, since gamma-adjusting a tile means having its pixels to hand.
 */
static unsigned char *sheet = NULL;	/* RGBA, sheet_w * sheet_h * 4 */
static int sheet_w = 0, sheet_h = 0;
static unsigned char *cell_buf = NULL;
static int cell_buf_size = 0;

/**
 * Turn a role named in lib/tcl/main.tcl into the subwindow that holds it.
 *
 * The names are the front end's vocabulary and the numbers are the game's;
 * this is the only place the two meet.  An unknown name is reported rather
 * than quietly dropped, because a pane with no term behind it is just black
 * and looks exactly like a broken one.
 */
static uint32_t role_to_flag(const char *role)
{
	if (streq(role, "messages")) return PW_MESSAGE;
	if (streq(role, "inventory")) return PW_INVEN;
	if (streq(role, "monsters")) return PW_MONLIST;
	if (streq(role, "objects")) return PW_ITEMLIST;
	if (streq(role, "recall")) return PW_MONSTER | PW_OBJECT;
	if (streq(role, "overhead")) return PW_OVERHEAD;
	if (streq(role, "minimap")) return PW_MAP;
	if (streq(role, "player")) return PW_PLAYER_2;

	return 0;
}

static int role_to_index(const char *role)
{
	if (streq(role, "map")) return 0;
	if (streq(role, "messages")) return 1;
	if (streq(role, "inventory")) return 2;
	if (streq(role, "monsters")) return 3;
	if (streq(role, "objects")) return 4;
	if (streq(role, "recall")) return 5;
	if (streq(role, "overhead")) return 6;
	if (streq(role, "minimap")) return 6;	/* same term, different flag */
	if (streq(role, "player")) return 7;

	plog_fmt("Tcl/Tk: main.tcl asked for a pane role I do not know: %s", role);
	return -1;
}

/**
 * "#rrggbb" for each of the game's colours, rebuilt on TERM_XTRA_REACT.
 */
static char colour_name[MAX_COLORS][8];

/**
 * Cached objects for the inner loop.
 *
 * Every cell the game writes becomes one `.term itemconfigure <id> -text <s>
 * -fill <colour>`.  Going through Tcl_EvalObjv with prebuilt objects rather
 * than formatting a command string for Tcl to parse is the difference between
 * a redraw costing microseconds and costing milliseconds, and a full screen is
 * 1,920 of them.
 *
 * Only the four constant words are cached.  The three that change are made
 * fresh each call, because Tcl takes a reference to everything passed to
 * Tcl_EvalObjv: reusing and mutating them panics with "Tcl_SetStringObj called
 * with shared object" the moment the canvas has kept one.
 */
static Tcl_Obj *cfg_itemconfigure;
static Tcl_Obj *cfg_dash_text;
static Tcl_Obj *cfg_dash_fill;

static void colours_init(void)
{
	int i;

	for (i = 0; i < MAX_COLORS; i++) {
		strnfmt(colour_name[i], sizeof(colour_name[i]), "#%02x%02x%02x",
				angband_color_table[i][1],
				angband_color_table[i][2],
				angband_color_table[i][3]);
	}
}

/**
 * Reconfigure one cell.
 */
static void cell_set(term_data *td, int x, int y, const char *ch, int attr)
{
	Tcl_Obj *objv[7];
	int idx = y * td->cols + x;
	int i;

	if (x < 0 || y < 0 || x >= td->cols || y >= td->rows) return;

	objv[0] = Tcl_NewStringObj(td->path, -1);
	objv[1] = cfg_itemconfigure;
	objv[2] = Tcl_NewIntObj(td->item[idx]);
	objv[3] = cfg_dash_text;
	objv[4] = Tcl_NewStringObj(ch, -1);
	objv[5] = cfg_dash_fill;
	objv[6] = Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1);

	for (i = 0; i < 7; i++) Tcl_IncrRefCount(objv[i]);

	if (Tcl_EvalObjv(interp, 7, objv, TCL_EVAL_GLOBAL) != TCL_OK) {
		plog_fmt("Tcl/Tk: drawing failed: %s", Tcl_GetStringResult(interp));
	}

	for (i = 0; i < 7; i++) Tcl_DecrRefCount(objv[i]);
}

static errr Term_text_tcl(int x, int y, int n, int a, const wchar_t *s)
{
	term_data *td = (term_data *)(Term->data);
	char buf[MB_LEN_MAX + 1];
	int i;

	clear_tiles(td, x, y, n);

	for (i = 0; i < n; i++) {
		int len = wctomb(buf, s[i]);

		if (len <= 0) {
			buf[0] = ' ';
			len = 1;
		}
		buf[len] = '\0';
		cell_set(td, x + i, y, buf, a);
	}

	return 0;
}

static errr Term_wipe_tcl(int x, int y, int n)
{
	term_data *td = (term_data *)(Term->data);
	int i;

	clear_tiles(td, x, y, n);
	for (i = 0; i < n; i++) cell_set(td, x + i, y, " ", COLOUR_WHITE);

	return 0;
}

/**
 * Show or hide a term's cursor.
 *
 * The game asks for this through Term_xtra(TERM_XTRA_SHAPE, 0 or 1), which
 * ui-term.c sends whenever the cursor becomes invisible or visible again --
 * and it is invisible for nearly all of normal play, because ui-init.c turns
 * it off at startup.  Ignoring that signal is why a yellow rectangle used to
 * sit wherever the cursor was last placed and stay there.
 */
static void cursor_show(term_data *td, bool show)
{
	Tcl_Obj *cmd;

	if (!td->cursor_item) return;
	td->cursor_visible = show;

	cmd = Tcl_ObjPrintf("%s itemconfigure %d -state %s", td->path,
			td->cursor_item, show ? "normal" : "hidden");
	Tcl_IncrRefCount(cmd);
	Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(cmd);
}

/**
 * Move the cursor, which is a plain canvas rectangle -- one of the four item
 * types the 2001 widget library implemented by hand and Tk has had all along.
 *
 * Moving it does not show it: whether it is visible is the game's to say, and
 * it says so separately.
 */
static errr Term_curs_tcl(int x, int y)
{
	term_data *td = (term_data *)(Term->data);
	char cmd[256];

	strnfmt(cmd, sizeof(cmd), "%s coords %d %d %d %d %d",
			td->path, td->cursor_item,
			x * td->cw + 1, y * td->ch + 1,
			(x + tile_width) * td->cw - 1,
			(y + tile_height) * td->ch - 1);
	Tcl_Eval(interp, cmd);

	/*
	 * Moving the cursor does not show it, and nothing here shows it either.
	 *
	 * ZangbandTK/Tk does not draw a terminal cursor.  What the game uses it
	 * for on the map screen is parking it on the status line, and the status
	 * line is being replaced rather than reproduced -- decision 9 and the T4
	 * panels.  A bright rectangle sitting in a corner for a whole game is a
	 * terminal's habit, not a feature to carry across.
	 *
	 * The machinery stays: the item exists, cursor_show works, and
	 * TERM_XTRA_SHAPE is honoured, so anything that genuinely asks for a
	 * cursor still gets one.  Measured across a startup and several menus the
	 * game never asks, which is why this reads as "removed" in play.
	 */

	return 0;
}

/**
 * Read a PNG into RGBA pixels of our own.
 *
 * Supports what the tile sheets actually are -- eight bits a channel, RGB or
 * RGBA, not interlaced -- and says so plainly rather than half-supporting the
 * rest.  Inflation is Tcl's, so there is no new dependency: Tcl has zlib built
 * in and exposes it to C.
 */
static bool png_load(const char *path, unsigned char **out, int *ow, int *oh)
{
	ang_file *fh;
	unsigned char *file = NULL, *idat = NULL, *raw, *img = NULL;
	Tcl_Obj *in = NULL, *flat = NULL;
	size_t flen = 0, ilen = 0;
	int w = 0, h = 0, depth, ctype, inter, bpp, stride, y, x;
	Tcl_Size rawlen;
	bool ok = false;

	fh = file_open(path, MODE_READ, FTYPE_RAW);
	if (!fh) return false;

	/* Read it whole, growing as we go: there is no file_size to ask. */
	{
		size_t cap = 1 << 16;
		int got;

		file = mem_alloc(cap);
		while ((got = file_read(fh, (char *)file + flen,
				cap - flen)) > 0) {
			flen += (size_t)got;
			if (flen == cap) {
				unsigned char *bigger = mem_alloc(cap * 2);

				memcpy(bigger, file, flen);
				mem_free(file);
				file = bigger;
				cap *= 2;
			}
		}
	}
	file_close(fh);

	if (flen < 33 || memcmp(file, "\x89PNG\r\n\x1a\n", 8) != 0) goto done;

	w = (file[16] << 24) | (file[17] << 16) | (file[18] << 8) | file[19];
	h = (file[20] << 24) | (file[21] << 16) | (file[22] << 8) | file[23];
	depth = file[24];
	ctype = file[25];
	inter = file[28];

	if (depth != 8 || (ctype != 2 && ctype != 6) || inter != 0) {
		plog_fmt("Tcl/Tk: %s is a PNG shape I do not read"
				" (depth %d, colour type %d, interlace %d).",
				path, depth, ctype, inter);
		goto done;
	}
	bpp = (ctype == 6) ? 4 : 3;
	stride = w * bpp + 1;

	/* Gather every IDAT: a large PNG is usually split across several. */
	{
		size_t p = 8;

		idat = mem_alloc(flen);
		while (p + 8 <= flen) {
			size_t len = ((size_t)file[p] << 24) | ((size_t)file[p+1] << 16)
					| ((size_t)file[p+2] << 8) | file[p+3];

			if (p + 12 + len > flen) break;
			if (memcmp(file + p + 4, "IDAT", 4) == 0) {
				memcpy(idat + ilen, file + p + 8, len);
				ilen += len;
			}
			p += 12 + len;
		}
	}
	if (!ilen) goto done;

	in = Tcl_NewByteArrayObj(idat, (Tcl_Size)ilen);
	Tcl_IncrRefCount(in);

	/*
	 * Note where the answer comes back: Tcl_ZlibInflate leaves the inflated
	 * bytes in the interpreter result, and its last argument is a gzip header
	 * dictionary rather than an output object.  Passing an object there and
	 * expecting it filled in fails quietly, which is exactly what happened
	 * the first time.
	 */
	if (Tcl_ZlibInflate(interp, TCL_ZLIB_FORMAT_ZLIB, in,
			(Tcl_Size)(stride * h), NULL) != TCL_OK) {
		plog_fmt("Tcl/Tk: %s would not inflate: %s", path,
				Tcl_GetStringResult(interp));
		goto done;
	}
	flat = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(flat);
	raw = Tcl_GetByteArrayFromObj(flat, &rawlen);
	if (rawlen < (Tcl_Size)stride * h) {
		plog_fmt("Tcl/Tk: %s inflated to %d bytes, expected %d.", path,
				(int)rawlen, stride * h);
		goto done;
	}

	/*
	 * Undo the per-row filters.  Every sheet we ship uses filter 0, but the
	 * others cost a few lines and a PNG from anywhere else may well use them.
	 */
	img = mem_zalloc((size_t)w * h * 4);
	for (y = 0; y < h; y++) {
		const unsigned char *src = raw + (size_t)y * stride;
		int ft = src[0];
		unsigned char *cur = img + (size_t)y * w * 4;
		const unsigned char *up = (y > 0) ? img + (size_t)(y - 1) * w * 4 : NULL;

		src++;
		for (x = 0; x < w; x++) {
			int ch;

			for (ch = 0; ch < bpp; ch++) {
				int rv = src[x * bpp + ch];
				int a = (x > 0) ? cur[(x - 1) * 4 + ch] : 0;
				int b = up ? up[x * 4 + ch] : 0;
				int c = (up && x > 0) ? up[(x - 1) * 4 + ch] : 0;
				int v;

				switch (ft) {
					case 1: v = rv + a; break;
					case 2: v = rv + b; break;
					case 3: v = rv + ((a + b) >> 1); break;
					case 4: {
						int p = a + b - c;
						int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);

						v = rv + ((pa <= pb && pa <= pc) ? a
								: (pb <= pc) ? b : c);
						break;
					}
					default: v = rv; break;
				}
				cur[x * 4 + ch] = (unsigned char)(v & 0xff);
			}
			if (bpp == 3) cur[x * 4 + 3] = 255;
		}
	}

	*out = img;
	*ow = w;
	*oh = h;
	img = NULL;
	ok = true;

done:
	if (in) Tcl_DecrRefCount(in);
	if (flat) Tcl_DecrRefCount(flat);
	if (file) mem_free(file);
	if (idat) mem_free(idat);
	if (img) mem_free(img);

	return ok;
}

/**
 * Draw one tile into a term's tile layer, scaled to the cell.
 *
 * The sheet is addressed the way every other front end addresses it: the row
 * comes from the attribute and the column from the character, each masked to
 * seven bits.
 *
 * Scaling is nearest-neighbour, done here rather than by Tk, because a Tk
 * photo can only be scaled by whole-number zoom and subsample factors and a
 * 16x16 tile very rarely divides a text cell exactly.  Nearest-neighbour is
 * the honest choice for pixel art in any case -- smoothing a 16x16 tile is
 * how you get mud.
 */
/**
 * How much of a tile's brightness survives when the grid is only remembered.
 *
 * Not invented: measured from the tilesets themselves.  Four of the five
 * declare separate terrain tiles for "in line of sight" and "seen before, not
 * now", and across 114 features the median brightness of the second is 0.81 of
 * the first -- 0.81 in the old set, 0.81 in Adam Bolt, 0.80 in Gervais, 0.88 in
 * Nomad.  So this is the number four sets of artists independently agreed on,
 * applied to the things that have only one tile each.
 *
 * In 1/256ths, so the arithmetic below is a multiply and a shift.
 */
#define DIM_REMEMBERED	207

/**
 * Darken a tile as it is copied, or not.
 *
 * Terrain needs none of this: every one of the five sets declares all four
 * lighting states for all forty-two features, so the game has already picked
 * the right tile before it reaches us.  Monsters, objects, traps and flavours
 * have one tile each, which is why an object the character merely remembers
 * used to be drawn as brightly as one under their torch.
 */
static void blit_tile(term_data *td, int x, int y, int a, int c, int comp,
		bool dim)
{
	Tk_PhotoImageBlock dst;
	int tw, th, sx, sy, px, py, dw, dh;

	if (!sheet || !td->screen || !current_graphics_mode) return;

	tw = current_graphics_mode->cell_width;
	th = current_graphics_mode->cell_height;
	sx = (c & 0x7f) * tw;
	sy = (a & 0x7f) * th;

	if (sx + tw > sheet_w || sy + th > sheet_h) return;

	/*
	 * A tile may span more than one text cell.  tile_width and tile_height
	 * are the game's own answer to a square tile in a tall, narrow character
	 * cell: it sends one tile and expects it drawn across that many columns,
	 * and the map simply shows fewer of them.
	 */
	dw = td->cw * tile_width;
	dh = td->ch * tile_height;

	if (cell_buf_size < dw * dh * 4) {
		if (cell_buf) mem_free(cell_buf);
		cell_buf_size = dw * dh * 4;
		cell_buf = mem_zalloc(cell_buf_size);
	}

	for (py = 0; py < dh; py++) {
		int ty = sy + (py * th) / dh;

		for (px = 0; px < dw; px++) {
			int tx = sx + (px * tw) / dw;
			const unsigned char *s = sheet + ((size_t)ty * sheet_w + tx) * 4;
			unsigned char *d = cell_buf + (py * dw + px) * 4;

			if (dim) {
				d[0] = (unsigned char)((s[0] * DIM_REMEMBERED) >> 8);
				d[1] = (unsigned char)((s[1] * DIM_REMEMBERED) >> 8);
				d[2] = (unsigned char)((s[2] * DIM_REMEMBERED) >> 8);
			} else {
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
			}
			/* Alpha is the sheet's own: dimming must not dissolve a tile. */
			d[3] = s[3];
		}
	}

	dst.pixelPtr = cell_buf;
	dst.width = dw;
	dst.height = dh;
	dst.pitch = dw * 4;
	dst.pixelSize = 4;
	dst.offset[0] = 0;
	dst.offset[1] = 1;
	dst.offset[2] = 2;
	dst.offset[3] = 3;

	Tk_PhotoPutBlock(interp, td->screen, &dst, x * td->cw, y * td->ch,
			dw, dh, comp);
}

/**
 * Clear the tile layer behind some cells, so text is not drawn over a tile.
 *
 * The text items sit above the tile layer and canvas text has no background of
 * its own, so whatever the layer holds shows through every gap in a glyph.
 */
static void clear_tiles(term_data *td, int x, int y, int n)
{
	Tk_PhotoImageBlock blk;
	int len = n * td->cw * td->ch * 4;

	if (!td->screen || n <= 0) return;

	if (cell_buf_size < len) {
		if (cell_buf) mem_free(cell_buf);
		cell_buf_size = len;
		cell_buf = mem_zalloc(cell_buf_size);
	}
	memset(cell_buf, 0, len);

	blk.pixelPtr = cell_buf;
	blk.width = n * td->cw;
	blk.height = td->ch;
	blk.pitch = n * td->cw * 4;
	blk.pixelSize = 4;
	blk.offset[0] = 0;
	blk.offset[1] = 1;
	blk.offset[2] = 2;
	blk.offset[3] = 3;

	Tk_PhotoPutBlock(interp, td->screen, &blk, x * td->cw, y * td->ch,
			n * td->cw, td->ch, TK_PHOTO_COMPOSITE_SET);
}

/**
 * Draw tiles.  The game calls this only for cells whose attribute has the
 * high bit set, because the term below sets higher_pict.
 */
/**
 * Is the grid behind this cell one the character can see right now?
 *
 * The conversion is ui-target.h's, and it only means anything for the map
 * term: a tile in the recall pane is a picture of a monster, not a place, so
 * every other term answers yes and is left alone.
 *
 * square_isseen, rather than map_info's lighting: map_info calls
 * square_memorize, and a drawing routine has no business changing what the
 * character remembers.  Seen against remembered is also the distinction that
 * matters here -- the states map_info actually produces in play are LOS,
 * TORCH and LIT, and LIT is precisely "known, but not in view".
 */
static bool cell_is_seen(const term_data *t, int x, int y)
{
	struct loc grid;

	/*
	 * Named t, not td.  The first version took a parameter called td, which
	 * shadows the array of the same name -- so "td != &td[0]", the test meant
	 * to let only the map term through, compared the parameter with itself and
	 * was always false.  Every term reached the conversion below, including
	 * the recall pane, whose tiles are pictures of monsters rather than places.
	 */
	if (!player || !character_dungeon || !cave) return true;
	if (td_count < 1 || t != &td[0]) return true;
	if (y < ROW_MAP || x < COL_MAP) return true;

	grid.y = (y - ROW_MAP) / tile_height + Term->offset_y;
	grid.x = (x - COL_MAP) / tile_width + Term->offset_x;

	if (!square_in_bounds_fully(cave, grid)) return true;

	return square_isseen(cave, grid);
}

static errr Term_pict_tcl(int x, int y, int n, const int *ap,
		const wchar_t *cp, const int *tap, const wchar_t *tcp)
{
	term_data *td = (term_data *)(Term->data);
	int i;

	for (i = 0; i < n; i++) {
		/*
		 * The terrain tile goes down first and the thing standing on it
		 * second, which is what makes a monster on grass look like a monster
		 * on grass.  Transparency comes from the sheet's own alpha.
		 */
		if (tap && (tap[i] != ap[i] || tcp[i] != cp[i])) {
			/*
			 * Something is standing on the terrain.  The terrain tile is the
			 * set's own choice for this lighting; the thing on top has only
			 * one tile, so it is dimmed here when the grid is remembered
			 * rather than seen.  Without this a detected monster two rooms
			 * away, or an object left behind in the dark, is drawn exactly as
			 * brightly as what is in front of you.
			 */
			blit_tile(td, x + i, y, tap[i], tcp[i], TK_PHOTO_COMPOSITE_SET,
					false);
			blit_tile(td, x + i, y, ap[i], cp[i], TK_PHOTO_COMPOSITE_OVERLAY,
					!cell_is_seen(td, x + i, y));
		} else {
			blit_tile(td, x + i, y, ap[i], cp[i], TK_PHOTO_COMPOSITE_SET,
					false);
		}

		/*
		 * Blank the text of every cell the tile covers, not just the one it
		 * was queued at.
		 *
		 * A tile spanning tile_width cells is queued once, and the term marks
		 * the cells after it with attribute 255 and then skips them --
		 * "2nd byte of bigtile" in ui-term.c, which `continue`s without
		 * calling any hook.  So nothing ever tells us to clear those cells,
		 * and a character left there by a full-screen display stays put with
		 * the tile drawn behind it.  That is what made every other column of
		 * a store or a character sheet survive being closed.
		 */
		{
			int dx, dy;

			for (dy = 0; dy < tile_height; dy++)
				for (dx = 0; dx < tile_width; dx++)
					cell_set(td, x + i + dx, y + dy, " ", COLOUR_WHITE);
		}
	}

	return 0;
}

static void grid_clear(term_data *td)
{
	int x, y;

	for (y = 0; y < td->rows; y++)
		for (x = 0; x < td->cols; x++)
			cell_set(td, x, y, " ", COLOUR_WHITE);
}

/**
 * Where a packaged game keeps Tcl's or Tk's script library `lib`: in tcltk/,
 * beside the game's own lib directory, which is where scripts/pkg_macos_tcltk
 * puts it in a Mac bundle (Contents/Resources) and the Linux and Windows
 * packages put it too.  ANGBAND_DIR_TCL is <there>/lib/tcl, so it is found
 * from that, and one rule serves every platform.
 *
 * Made absolute, because Tk reads its library long after this and a relative
 * one would mean whatever the working directory was by then.  False when
 * `probe` is not in it -- a build tree, whose repository-root tcltk/ holds
 * sources and not a library.
 */
static bool packaged_script_library(const char *lib, const char *probe,
		char *buf, size_t len)
{
	char path[1024];
	Tcl_Obj *obj, *norm;

	strnfmt(path, sizeof(path), "%s/../../tcltk/%s", ANGBAND_DIR_TCL, lib);
	obj = Tcl_NewStringObj(path, -1);
	Tcl_IncrRefCount(obj);
	norm = Tcl_FSGetNormalizedPath(NULL, obj);
	if (norm) my_strcpy(buf, Tcl_GetString(norm), len);
	Tcl_DecrRefCount(obj);
	if (!norm) return false;

	strnfmt(path, sizeof(path), "%s/%s", buf, probe);
	return file_exists(path);
}

/**
 * Point Tcl and Tk at their own script libraries.
 *
 * Tcl_Init finds init.tcl by walking up from the executable, which works for a
 * tclsh installed beside its lib directory and does not work for a game
 * executable sitting in a cmake build tree.  Rather than discover that as
 * "invalid command name" from the first script we source, say where they are.
 *
 * A packaged game carries them in tcltk/, beside its own lib directory, and
 * takes them from there; a build tree has no such directory and takes them
 * from the build's TCLTK_PREFIX.  Either way the environment wins if it is
 * already set, so a developer can point a build at a different toolchain
 * without reconfiguring.
 */
static void set_script_library_paths(void)
{
	static char tclbuf[1024];
	static char tkbuf[1024];
	char dir[1024];

	if (!getenv("TCL_LIBRARY")) {
		if (packaged_script_library("tcl9.0", "init.tcl", dir, sizeof(dir)))
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s", dir);
		else
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s/lib/tcl9.0",
					TCLTK_PREFIX_PATH);
		putenv(tclbuf);
	}

	if (!getenv("TK_LIBRARY")) {
		if (packaged_script_library("tk9.0", "tk.tcl", dir, sizeof(dir)))
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s", dir);
		else
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s/lib/tk9.0",
					TCLTK_PREFIX_PATH);
		putenv(tkbuf);
	}
}

/**
 * Handle a request from the game to do something outside drawing.
 *
 * TERM_XTRA_EVENT is the important one: it is where the game hands control
 * back, and therefore the only place Tk gets to process anything.  Blocking
 * there when the game says it is willing to wait is what keeps the application
 * from spinning at 100% while the player thinks.
 */
static void hooks_apply(void);
/* True while a command started from the menus is running; see
 * command_may_start(), which is where the reasoning lives. */
static bool command_inside = false;

/*
 * Holding the door shut, in a way that cannot forget to open it again.
 *
 * This flag is the whole of the defence described at command_may_start():
 * while it is true the menus are greyed, menu dispatch is refused, and the
 * Items window's gear actions are refused, because 4.2's ui-* prompts keep
 * their state in file statics and a second command started inside the first
 * frees the first's lists out from under it.
 *
 * It was set and cleared as five separate pairs of bare assignments.  Nothing
 * was wrong with any of them -- every one of the five is entered through a
 * command_may_start() check so they cannot nest, there is no setjmp or
 * longjmp anywhere in the game, and no early return sits between a set and
 * its clear -- so this is not fixing a path that leaks the flag today.  It is
 * that the failure mode if one ever did is silent and permanent: nothing
 * clears the flag on any other path, so command_may_start() would answer
 * false for the rest of the process and every menu command, gear action and
 * book command would be refused for ever, with no message and nothing in a
 * log.  An invariant whose breach cannot be noticed should not be maintained
 * by hand in five places.
 *
 * So the clear is attached to the scope instead.  Open a block, declare the
 * guard, and the flag is true until the block ends however it ends.
 *
 * Deliberately a hard error rather than a silent no-op when the attribute is
 * missing: ATTRIBUTE() in h-basic.h expands to nothing off GCC and Clang, and
 * a guard that quietly stops guarding is worse than no guard.  The front end
 * is built against a Tcl/Tk this project compiles itself, by GCC or Clang.
 */
#if defined(__GNUC__) || defined(__clang__)
static void command_inside_leave(const bool *guard)
{
	(void)guard;
	command_inside = false;
}
#define COMMAND_INSIDE_HELD						\
	bool command_inside_guard_					\
		ATTRIBUTE((cleanup(command_inside_leave)))		\
		= (command_inside = true);				\
	(void)command_inside_guard_
#else
# error "the Tcl front end needs __attribute__((cleanup)); see command_inside"
#endif

static void command_nudge(void);
static bool in_play(void);
static bool command_may_start(void);

/**
 * Finish a command that has no answer to give.
 *
 * A Tcl command returns whatever is in the interpreter's result, and the
 * drawing path puts things there: cell_set evaluates a canvas itemconfigure
 * for every cell it touches, graphics_load builds an image, and the game's
 * own redraw runs both. A command that does its work and then returns TCL_OK
 * without clearing the result hands that leftover back as its value.
 *
 * It is not theoretical and it is not small. Before this, `angband_tileset 1`
 * returned two megabytes of pixel data as its result, and
 * `angband_resize 0 80 24` returned "4228", a canvas item id. Neither command
 * has anything to say; both said whatever Tk had said last.
 *
 * objcmd_refont already called Tcl_ResetResult for exactly this reason. This
 * is that, named, so the next command that answers with nothing says so.
 */
static int command_done(Tcl_Interp *ip)
{
	Tcl_ResetResult(ip);
	return TCL_OK;
}

static void choice_install(void);
static void store_notice(game_event_type type, game_event_data *data,
		void *user);
static void last_sight_notice(game_event_type type, game_event_data *data,
		void *user);
static void birth_view_tk(enum birth_stage stage, int slot,
		struct birth_answer *answer);
static void birth_points_notice(game_event_type type, game_event_data *data,
		void *user);
static int chooser_view_tk(const char **entries, const char **names,
		int count, int cursor, bool retry);
static int death_view_tk(const menu_action *actions, int count);
static void events_register(void);
static void sound_seen(game_event_type type, game_event_data *data,
		void *user);
static void bridge_reinstall_if_due(void);
static void bridge_first_wait(void);
static void bridge_reinit(void);
static void (*bridge_reinit_prev)(void);
static bool bridge_reinstall_due;
static bool store_inside;

/**
 * A key the bridge sends the game itself -- the nudge after a push, the Escape
 * that ends a prompt a click has answered -- as opposed to one the player
 * typed.
 *
 * Term_flush() asks the front end to flush and then forgets every queued key,
 * and this front end's flush runs Tk's pending events.  So a menu click or a
 * timer handled there pushed its command, sent its key, and lost the key a
 * moment later: the command sat on the queue until the player pressed
 * something else.  Measured at the start of a game, where the first level's
 * flushes come thick and fast: a command pushed as the level appeared waited
 * for a second nudge.  A key sent while a flush is running is kept back and
 * sent at the next wait instead.  The player's own keys are not -- discarding
 * those is what a flush is for.
 *
 * Counted, not set: flushes nest.  A menu action handled inside one prompts,
 * and its prompts flush in turn, and a flag cleared by the inner flush let
 * the nudge after the action through to a queue the outer one then emptied --
 * the Retire chosen from the menu, asked, confirmed and then never done.
 */
static int flushing;
static keycode_t keys_owed[8];
static int keys_owed_n;

static void bridge_key(keycode_t key)
{
	if (flushing) {
		if (keys_owed_n < (int)N_ELEMENTS(keys_owed))
			keys_owed[keys_owed_n++] = key;
		return;
	}
	Term_keypress(key, 0);
}

/*
 * The first time the game waits is past everything that would otherwise
 * overwrite these: textui_init() has assigned the subwindows by index and
 * textui_input_init() has filled the game's input hooks.  That is usually a
 * request for a key, but not always: a key typed ahead of the splash screen
 * takes the game straight on to choosing a character, and the first wait is
 * then the Choose Game window's -- so the views call this too.
 */
static void bridge_first_wait(void)
{
	if (flags_applied) return;
	flags_applied = true;
	subwindows_set_flags(want_flag, ANGBAND_TERM_MAX);
	hooks_apply();
	choice_install();
}

static errr Term_xtra_tcl(int n, int v)
{
	term_data *td = (term_data *)(Term->data);

	if (n == TERM_XTRA_EVENT) {
		bridge_first_wait();
		bridge_reinstall_if_due();
	}

	switch (n) {
		case TERM_XTRA_EVENT:
			/* What was owed since the last flush, before waiting again. */
			if (keys_owed_n) {
				int i;

				for (i = 0; i < keys_owed_n; i++)
					Term_keypress(keys_owed[i], 0);
				keys_owed_n = 0;
				return 0;
			}
			if (v) {
				Tcl_DoOneEvent(TCL_ALL_EVENTS);
			} else {
				while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
					;
			}
			return 0;

		case TERM_XTRA_FLUSH:
			flushing++;
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
				;
			flushing--;
			return 0;

		case TERM_XTRA_CLEAR:
			grid_clear(td);
			return 0;

		case TERM_XTRA_FRESH:
			/* Let Tk repaint, but do not block: the game is mid-turn. */
			while (Tcl_DoOneEvent(TCL_WINDOW_EVENTS | TCL_IDLE_EVENTS
					| TCL_DONT_WAIT))
				;
			return 0;

		case TERM_XTRA_REACT:
			/* The palette may have changed under us. */
			colours_init();
			return 0;

		case TERM_XTRA_DELAY:
			if (v > 0) Tcl_Sleep(v);
			return 0;

		case TERM_XTRA_SHAPE:
			cursor_show(td, v != 0);
			return 0;

		case TERM_XTRA_NOISE:
			return 0;
	}

	return 1;
}

static void Term_init_tcl(term *t)
{
	(void)t;
}

static void Term_nuke_tcl(term *t)
{
	(void)t;
}

/**
 * angband_key -- every keystroke arrives here from Tk's binding.
 *
 * Arguments are Tk's %N (keysym as a number), %s (modifier state) and %A (the
 * character it produces, empty for a bare modifier or a function key).  The
 * job is to turn that into what ui-event.h calls a keycode plus mods, which is
 * the same representation the term build uses -- so a keymap written in one
 * works in the other, which §6 decision 13 of the plan asks for.
 *
 * tk_keypress() does the turning, false for a key the game has no use for;
 * the Keymaps window's capture goes through it too, so a key bound there is
 * the key that typing it sends.
 */
/*
 * A key of the numeric keypad, by X11's keysym, as the game's keycode; 0 for
 * any other key.  The digits and operators are what Tk reports on the Mac,
 * where Num Lock is always on; the rest are a PC keypad with it off.
 */
static keycode_t tk_keypad_code(int keysym)
{
	if (keysym >= 0xFFB0 && keysym <= 0xFFB9)
		return (keycode_t)('0' + (keysym - 0xFFB0));
	switch (keysym) {
		case 0xFFAA: return '*';
		case 0xFFAB: return '+';
		case 0xFFAD: return '-';
		case 0xFFAE: return '.';
		case 0xFFAF: return '/';
		case 0xFFBD: return '=';
		case 0xFF8D: return KC_ENTER;
		case 0xFF95: return KC_HOME;
		case 0xFF96: return ARROW_LEFT;
		case 0xFF97: return ARROW_UP;
		case 0xFF98: return ARROW_RIGHT;
		case 0xFF99: return ARROW_DOWN;
		case 0xFF9A: return KC_PGUP;
		case 0xFF9B: return KC_PGDOWN;
		case 0xFF9C: return KC_END;
		case 0xFF9D: return KC_BEGIN;
		case 0xFF9E: return KC_INSERT;
		case 0xFF9F: return KC_DELETE;
		default: return 0;
	}
}

static bool tk_keypress(int keysym, int state, const char *ch,
		struct keypress *kp)
{
	keycode_t k = 0;
	uint8_t mods = 0;

	/* X11 modifier bits, which Tk reports on every platform. */
	if (state & 0x01) mods |= KC_MOD_SHIFT;
	if (state & 0x04) mods |= KC_MOD_CONTROL;
	if (state & 0x08) mods |= KC_MOD_ALT;

	/*
	 * The function keys, F1 to F15, which X11 numbers from 0xFFBE; and the
	 * keypad, which the game tells apart from the keys it copies by
	 * KC_MOD_KEYPAD, as the Cocoa front end sets it.  pref.prf walks on the
	 * keypad's digits and runs on them with Shift, so Shift and Control stay
	 * on a keypad key where the game keeps them on any other -- which used
	 * to arrive as a plain digit, so none of those keymaps ever matched.
	 */
	if (keysym >= 0xFFBE && keysym <= 0xFFCC) {
		k = (keycode_t)(KC_F1 + (keysym - 0xFFBE));
	} else if ((k = tk_keypad_code(keysym)) != 0) {
		mods |= KC_MOD_KEYPAD;
		if (!MODS_INCLUDE_SHIFT(k)) mods &= ~KC_MOD_SHIFT;
	} else switch (keysym) {
		case 0xFF1B: k = ESCAPE; break;
		case 0xFF0D: k = KC_ENTER; break;
		case 0xFF09: k = KC_TAB; break;
		case 0xFF08: k = KC_BACKSPACE; break;
		case 0xFF7F: k = KC_DELETE; break;
		case 0xFF51: k = ARROW_LEFT; break;
		case 0xFF52: k = ARROW_UP; break;
		case 0xFF53: k = ARROW_RIGHT; break;
		case 0xFF54: k = ARROW_DOWN; break;
		case 0xFF50: k = KC_HOME; break;
		case 0xFF57: k = KC_END; break;
		case 0xFF55: k = KC_PGUP; break;
		case 0xFF56: k = KC_PGDOWN; break;
		default:
			/*
			 * An ordinary character.  Tk has already applied shift and the
			 * keyboard layout, so %A is what the player meant to type, and
			 * reporting KC_MOD_SHIFT as well would make the game see it twice.
			 */
			if (ch[0]) {
				k = (unsigned char)ch[0];
				mods &= ~KC_MOD_SHIFT;
			} else if ((mods & KC_MOD_CONTROL) && keysym > 0x20
					&& keysym < 0x7f) {
				/*
				 * Control and a key that has a character.  On the Mac,
				 * Tk reports no character for it at all -- %A is empty
				 * for Control-F -- so it was taken for a bare modifier
				 * and dropped, and none of the game's ^ commands could
				 * be typed.  The keysym is the key's own character.
				 */
				k = (keycode_t)keysym;
			} else {
				/* A bare modifier, or a key we do not map.  Ignore it. */
				return false;
			}
			break;
	}

	/*
	 * Control as ui-event.h says a front end sends it: on a letter it is part
	 * of the code, KTRL('F') with no KC_MOD_CONTROL, which is what the game's
	 * command table and keymaps look for.  X11's Tk has already made the
	 * control character, and only the modifier has to go.
	 */
	if ((mods & KC_MOD_CONTROL) && ENCODE_KTRL(k)) {
		k = KTRL(k);
		mods &= ~KC_MOD_CONTROL;
	} else if (!MODS_INCLUDE_CONTROL(k)) {
		mods &= ~KC_MOD_CONTROL;
	}

	kp->type = EVT_KBRD;
	kp->code = k;
	kp->mods = mods;
	return true;
}

static int objcmd_key(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int keysym, state;
	struct keypress kp;

	(void)dummy;

	if (objc != 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "keysym state char");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &keysym) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[2], &state) != TCL_OK) return TCL_ERROR;
	if (tk_keypress(keysym, state, Tcl_GetString(objv[3]), &kp))
		Term_keypress(kp.code, kp.mods);

	return TCL_OK;
}

/*
 * The game's events, by name, for binding from Tcl.
 *
 * Generated from the enum in game-event.h and kept honest by the assertion
 * below: adding an event upstream without naming it here fails the build
 * rather than producing a virtual event nobody can bind to.
 */
static const char *game_event_name[] = {
	"MAP",
	"STATS",
	"HP",
	"MANA",
	"AC",
	"EXPERIENCE",
	"PLAYERLEVEL",
	"PLAYERTITLE",
	"GOLD",
	"MONSTERHEALTH",
	"DUNGEONLEVEL",
	"PLAYERSPEED",
	"RACE_CLASS",
	"STUDYSTATUS",
	"STATUS",
	"DETECTIONSTATUS",
	"FEELING",
	"LIGHT",
	"STATE",
	"PLAYERMOVED",
	"SEEFLOOR",
	"EXPLOSION",
	"BOLT",
	"MISSILE",
	"INVENTORY",
	"EQUIPMENT",
	"ITEMLIST",
	"MONSTERLIST",
	"MONSTERTARGET",
	"OBJECTTARGET",
	"MESSAGE",
	"SOUND",
	"BELL",
	"USE_STORE",
	"STORECHANGED",
	"INPUT_FLUSH",
	"MESSAGE_FLUSH",
	"CHECK_INTERRUPT",
	"REFRESH",
	"NEW_LEVEL_DISPLAY",
	"COMMAND_REPEAT",
	"ANIMATE",
	"CHEAT_DEATH",
	"INITSTATUS",
	"BIRTHPOINTS",
	"ENTER_INIT",
	"LEAVE_INIT",
	"ENTER_BIRTH",
	"LEAVE_BIRTH",
	"ENTER_GAME",
	"LEAVE_GAME",
	"ENTER_WORLD",
	"LEAVE_WORLD",
	"ENTER_STORE",
	"ENTER_SERVICE",
	"USE_SERVICE",
	"LEAVE_SERVICE",
	"LEAVE_STORE",
	"ENTER_DEATH",
	"LEAVE_DEATH",
	"GEN_LEVEL_START",
	"GEN_LEVEL_END",
	"GEN_ROOM_START",
	"GEN_ROOM_CHOOSE_SIZE",
	"GEN_ROOM_CHOOSE_SUBTYPE",
	"GEN_ROOM_END",
	"GEN_TUNNEL_FINISHED",
	"END",
};

/*
 * The build breaks here, with "size of array is negative", if an event is
 * added to game_event_type and not named above.  C99 has no static_assert.
 */
typedef char game_event_name_is_complete[
		(N_ELEMENTS(game_event_name) == N_GAME_EVENTS) ? 1 : -1];

/**
 * A game event reaching Tcl.
 *
 * Every event becomes a Tk virtual event on ".", named <<Angband_MAP>> and so
 * on, which a script binds to like any other.  That is the whole of the "up"
 * seam: the game says what changed, the interface decides what to redraw, and
 * neither knows anything about the other.
 *
 * This is what the archive's qebind-dll.c was for -- 1,211 lines providing
 * "event-like messages to scripts, and the ability to bind Tcl commands to
 * them" -- and Tk has had it since 8.5.
 */
static Tcl_Obj *event_script[N_GAME_EVENTS];

static void event_to_tcl(game_event_type type, game_event_data *data,
		void *user)
{
	(void)data;
	(void)user;

	if (type < 0 || type >= (int)N_ELEMENTS(event_script)) return;
	if (!event_script[type]) return;

	/*
	 * The events are queued, not dispatched here -- see the -when tail in
	 * the command built below.
	 *
	 * Dispatching them where they are signalled puts a Tcl script inside the
	 * game's own call stack, part-way through whatever it was updating, and
	 * it nests: measured during startup, the first version of this reached
	 * Tcl's thousand-deep evaluation limit before the game had finished
	 * reading its data files.  Queued, each binding runs from the event loop
	 * -- which is the game asking for input, so the state it reads is
	 * settled, and a command it pushes is one the game will take next.
	 */
	if (Tcl_EvalObjEx(interp, event_script[type], TCL_EVAL_GLOBAL) != TCL_OK) {
		/*
		 * Be precise about what this catches, because it is narrower than it
		 * looks.  The script here is `event generate . <<Angband_X>> -when
		 * tail`, so this branch is that *command* failing -- which is rare and
		 * structural, a window gone or an event name the build never
		 * registered.  A failing *binding* does not come through here at all:
		 * -when tail means Tk runs it from the event loop later, where an
		 * error is already a background error and reaches the sink through
		 * Tk's own path and the handler lib/tcl/errorInfo.tcl installs.
		 *
		 * Tcl_BackgroundException is still right for the narrow case, and for
		 * the same reason: this function has no caller to return a failure to,
		 * and the report below goes to the stderr Tk redirects to /dev/null
		 * for a bundled application.  Without it, the one failure that would
		 * stop every binding in the front end from ever running is the one
		 * failure nobody would see.
		 *
		 * plog_fmt stays for a terminal run, where stderr is real and a person
		 * may well not have a window open.  It reports errorInfo rather than
		 * the bare result so the two paths say the same thing.
		 */
		Tcl_BackgroundException(interp, TCL_ERROR);
		plog_fmt("Tcl/Tk: <<Angband_%s>>: %s", game_event_name[type],
				Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
	}
}

/**
 * Subscribe to everything the game can tell us.
 *
 * All of it, rather than a chosen few: a script that binds nothing pays
 * nothing beyond one function call per event, and the alternative is editing C
 * every time the interface wants to notice something new.
 */
static void events_init(void)
{
	int i;

	/*
	 * The command is built once per event rather than once per signal.  These
	 * fire thousands of times in a session, and the objects are ours alone:
	 * we hold the only reference and never write to them, which is what the
	 * "called with shared object" panic earlier in this file was about.
	 */
	for (i = 0; i < (int)N_ELEMENTS(game_event_name); i++) {
		event_script[i] = Tcl_ObjPrintf(
				"event generate . <<Angband_%s>> -when tail",
				game_event_name[i]);
		Tcl_IncrRefCount(event_script[i]);
	}
	events_register();
}

/* The handlers that pass each event on -- again after a reset sweeps them. */
static void events_register(void)
{
	int i;

	for (i = 0; i < (int)N_ELEMENTS(game_event_name); i++)
		event_add_handler((game_event_type)i, event_to_tcl, NULL);
	event_add_handler(EVENT_SOUND, sound_seen, NULL);
}

/*
 * The game's commands, by name, for pushing from Tcl.
 *
 * Generated from the enum in cmd-core.h and kept in step by the negative-size
 * typedef below: adding a command upstream without naming it here breaks the
 * build rather than leaving a hole in the middle of the table, which is what a
 * name-to-code lookup by index would do.
 */
static const char *cmd_code_name[] = {
	"CMD_NULL",
	"CMD_LOADFILE",
	"CMD_NEWGAME",
	"CMD_BIRTH_INIT",
	"CMD_BIRTH_RESET",
	"CMD_CHOOSE_RACE",
	"CMD_CHOOSE_CLASS",
	"CMD_CHOOSE_REALM",
	"CMD_BUY_STAT",
	"CMD_SELL_STAT",
	"CMD_RESET_STATS",
	"CMD_REFRESH_STATS",
	"CMD_ROLL_STATS",
	"CMD_PREV_STATS",
	"CMD_NAME_CHOICE",
	"CMD_HISTORY_CHOICE",
	"CMD_ACCEPT_CHARACTER",
	"CMD_GO_UP",
	"CMD_GO_DOWN",
	"CMD_WALK",
	"CMD_JUMP",
	"CMD_PATHFIND",
	"CMD_INSCRIBE",
	"CMD_UNINSCRIBE",
	"CMD_AUTOINSCRIBE",
	"CMD_TAKEOFF",
	"CMD_WIELD",
	"CMD_DROP",
	"CMD_BROWSE_SPELL",
	"CMD_STUDY",
	"CMD_CAST",
	"CMD_USE_STAFF",
	"CMD_USE_WAND",
	"CMD_USE_ROD",
	"CMD_ACTIVATE",
	"CMD_EAT",
	"CMD_QUAFF",
	"CMD_READ_SCROLL",
	"CMD_REFILL",
	"CMD_USE",
	"CMD_FIRE",
	"CMD_THROW",
	"CMD_PICKUP",
	"CMD_AUTOPICKUP",
	"CMD_IGNORE",
	"CMD_DISARM",
	"CMD_REST",
	"CMD_TUNNEL",
	"CMD_OPEN",
	"CMD_CLOSE",
	"CMD_RUN",
	"CMD_EXPLORE",
	"CMD_NAVIGATE_UP",
	"CMD_NAVIGATE_DOWN",
	"CMD_HOLD",
	"CMD_ALTER",
	"CMD_STEAL",
	"CMD_SLEEP",
	"CMD_SELL",
	"CMD_BUY",
	"CMD_STASH",
	"CMD_RETRIEVE",
	"CMD_SPOIL_ARTIFACT",
	"CMD_SPOIL_MON",
	"CMD_SPOIL_MON_BRIEF",
	"CMD_SPOIL_OBJ",
	"CMD_WIZ_ACQUIRE",
	"CMD_WIZ_ADVANCE",
	"CMD_WIZ_BANISH",
	"CMD_WIZ_CHANGE_ITEM_QUANTITY",
	"CMD_WIZ_COLLECT_DISCONNECT_STATS",
	"CMD_WIZ_COLLECT_OBJ_MON_STATS",
	"CMD_WIZ_COLLECT_PIT_STATS",
	"CMD_WIZ_CREATE_ALL_ARTIFACT",
	"CMD_WIZ_CREATE_ALL_ARTIFACT_FROM_TVAL",
	"CMD_WIZ_CREATE_ALL_OBJ",
	"CMD_WIZ_CREATE_ALL_OBJ_FROM_TVAL",
	"CMD_WIZ_CREATE_ARTIFACT",
	"CMD_WIZ_CREATE_OBJ",
	"CMD_WIZ_CREATE_TRAP",
	"CMD_WIZ_CURE_ALL",
	"CMD_WIZ_CURSE_ITEM",
	"CMD_WIZ_DETECT_ALL_LOCAL",
	"CMD_WIZ_DETECT_ALL_MONSTERS",
	"CMD_WIZ_DUMP_LEVEL_MAP",
	"CMD_WIZ_EDIT_PLAYER_EXP",
	"CMD_WIZ_EDIT_PLAYER_GOLD",
	"CMD_WIZ_GAIN_GOLD",
	"CMD_WIZ_GAIN_HP",
	"CMD_WIZ_GAIN_PET",
	"CMD_WIZ_KNOW_PLACES",
	"CMD_WIZ_EDIT_PLAYER_START",
	"CMD_WIZ_EDIT_PLAYER_STAT",
	"CMD_WIZ_HIT_ALL_LOS",
	"CMD_WIZ_INCREASE_EXP",
	"CMD_WIZ_JUMP_LEVEL",
	"CMD_WIZ_LEARN_OBJECT_KINDS",
	"CMD_WIZ_MAGIC_MAP",
	"CMD_WIZ_PEEK_NOISE_SCENT",
	"CMD_WIZ_PERFORM_EFFECT",
	"CMD_WIZ_PLAY_ITEM",
	"CMD_WIZ_PUSH_OBJECT",
	"CMD_WIZ_QUERY_FEATURE",
	"CMD_WIZ_QUERY_SQUARE_FLAG",
	"CMD_WIZ_QUIT_NO_SAVE",
	"CMD_WIZ_RECALL_MONSTER",
	"CMD_WIZ_RERATE",
	"CMD_WIZ_REROLL_ITEM",
	"CMD_WIZ_STAT_ITEM",
	"CMD_WIZ_SUMMON_NAMED",
	"CMD_WIZ_SUMMON_RANDOM",
	"CMD_WIZ_SET_ALLEGIANCE",
	"CMD_WIZ_TELEPORT_RANDOM",
	"CMD_WIZ_TELEPORT_TO",
	"CMD_WIZ_TWEAK_ITEM",
	"CMD_WIZ_WIPE_RECALL",
	"CMD_WIZ_WIZARD_LIGHT",
	"CMD_RETIRE",
	"CMD_HELP",
	"CMD_REPEAT",
	"CMD_COMMAND_MONSTER",
};

/*
 * cmd_code has no terminator to count against, so the check is against the
 * last command instead: inserting one anywhere above it, or adding one after
 * it, breaks the build here with "size of array is negative".
 */
typedef char cmd_code_name_is_complete[
		(N_ELEMENTS(cmd_code_name) == CMD_COMMAND_MONSTER + 1) ? 1 : -1];

/**
 * The typed arguments a command carries, by the name the game knows them by.
 *
 * struct command holds four slots of a tagged union and the setters are typed,
 * so a caller has to say which setter to use.  Rather than make every script
 * spell out the type, the names are listed here: surveyed across every
 * cmd_set_arg_* call in the game, no argument name is used with two different
 * types, so the name is enough.  If that ever stops being true the answer is
 * to name the type explicitly, not to guess.
 *
 * "item" is missing on purpose.  It wants a struct object *, and a script has
 * no way to name one until the read accessors land in T6; pushing a command
 * that needs an item is a T6 problem, and saying so is better than accepting
 * an integer that means nothing.
 */
static const struct {
	const char *name;
	enum { ARG_NUMBER, ARG_CHOICE, ARG_DIRECTION, ARG_TARGET, ARG_POINT,
			ARG_STRING } type;
} cmd_arg_type[] = {
	{ "choice",		ARG_CHOICE },
	{ "changed",	ARG_CHOICE },
	{ "all_prop",	ARG_CHOICE },
	{ "quantity",	ARG_NUMBER },
	{ "index",		ARG_NUMBER },
	{ "tval",		ARG_NUMBER },
	{ "range",		ARG_NUMBER },
	{ "level",		ARG_NUMBER },
	{ "power",		ARG_NUMBER },
	{ "depth",		ARG_NUMBER },
	{ "depth_min",	ARG_NUMBER },
	{ "depth_max",	ARG_NUMBER },
	{ "direction",	ARG_DIRECTION },
	{ "target",		ARG_TARGET },
	{ "point",		ARG_POINT },
	{ "name",		ARG_STRING },
	{ "history",	ARG_STRING },
};

/**
 * angband_push -- put a command on the game's queue, with its arguments.
 *
 *    angband_push CMD_WALK direction 6
 *    angband_push CMD_PATHFIND point {12 34}
 *    angband_push -count 99 CMD_TUNNEL direction 4
 *
 * This is the "down" seam in full: the command table above covers what a menu
 * offers, and this covers everything else the game can be asked to do,
 * including the commands that have no key at all.
 *
 * It is the same queue the keyboard feeds.  Nothing here synthesises a
 * keystroke, so a command with arguments arrives complete rather than as a key
 * followed by the prompts the game would have raised to fill it in.
 */
static int objcmd_push(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	/*
	 * Everything is parsed before anything is pushed.
	 *
	 * There is no way to take a command back off the queue -- cmdq_release
	 * empties the whole thing -- and a command sitting there with half its
	 * arguments is one the game will stop and prompt for, which is the very
	 * thing this seam exists to avoid.  So a bad argument has to be an error
	 * before the push, not after it.
	 */
	struct {
		const char *name;
		int type;
		int value;
		struct loc grid;
		const char *str;
	} arg[CMD_MAX_ARGS];
	struct command *cmd;
	const char *name;
	int code = -1, count = 1, i, nargs = 0;
	Tcl_Size first = 1;

	(void)dummy;

	if (objc >= 3 && strcmp(Tcl_GetString(objv[1]), "-count") == 0) {
		if (Tcl_GetIntFromObj(ip, objv[2], &count) != TCL_OK) return TCL_ERROR;
		first = 3;
	}

	if (objc < first + 1 || ((objc - first - 1) & 1)) {
		Tcl_WrongNumArgs(ip, 1, objv, "?-count n? command ?name value ...?");
		return TCL_ERROR;
	}

	name = Tcl_GetString(objv[first]);
	for (i = 0; i < (int)N_ELEMENTS(cmd_code_name); i++) {
		if (strcmp(name, cmd_code_name[i]) == 0) {
			code = i;
			break;
		}
	}
	if (code < 0) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such command: %s", name));
		return TCL_ERROR;
	}
	if (code == CMD_NULL) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("CMD_NULL is the absence of a command", -1));
		return TCL_ERROR;
	}

	if ((objc - first - 1) / 2 > CMD_MAX_ARGS) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"a command carries at most %d arguments", CMD_MAX_ARGS));
		return TCL_ERROR;
	}

	for (i = (int)first + 1; i < (int)objc; i += 2) {
		const char *a = Tcl_GetString(objv[i]);
		Tcl_Obj *val = objv[i + 1];
		int n, found = -1;

		for (n = 0; n < (int)N_ELEMENTS(cmd_arg_type); n++) {
			if (strcmp(a, cmd_arg_type[n].name) == 0) {
				found = n;
				break;
			}
		}
		if (found < 0) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"%s: unknown argument, or one that needs an object", a));
			return TCL_ERROR;
		}

		arg[nargs].name = a;
		arg[nargs].type = cmd_arg_type[found].type;

		if (arg[nargs].type == ARG_POINT) {
			Tcl_Obj **xy;
			Tcl_Size np;
			int x, y;

			if (Tcl_ListObjGetElements(ip, val, &np, &xy) != TCL_OK
					|| np != 2
					|| Tcl_GetIntFromObj(ip, xy[0], &x) != TCL_OK
					|| Tcl_GetIntFromObj(ip, xy[1], &y) != TCL_OK) {
				Tcl_SetObjResult(ip,
						Tcl_ObjPrintf("%s: wanted a grid as {x y}", a));
				return TCL_ERROR;
			}
			arg[nargs].grid = loc(x, y);
		} else if (arg[nargs].type == ARG_STRING) {
			arg[nargs].str = Tcl_GetString(val);
		} else if (Tcl_GetIntFromObj(ip, val, &arg[nargs].value) != TCL_OK) {
			return TCL_ERROR;
		}

		nargs++;
	}

	/*
	 * One return value, three meanings: the game has no handler for that code
	 * (CMD_REPEAT and the other user-interface-only entries), the queue is
	 * full, or a repeat was asked for where none is allowed.  cmd_idx is
	 * static in cmd-core.c, so the message says all three rather than
	 * guessing at one.
	 */
	/*
	 * The queue itself would survive this, but the nudge below would put a
	 * keystroke into whatever prompt is open.  Same rule.
	 */
	if (player && !inkey_flag) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				"the game is in the middle of something else", -1));
		return TCL_ERROR;
	}

	if (cmdq_push_repeat((cmd_code)code, count) != 0) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"%s was not queued: the game has no handler for it, or the"
				" queue is full", name));
		return TCL_ERROR;
	}

	cmd = cmdq_peek();

	for (i = 0; i < nargs; i++) {
		switch (arg[i].type) {
		case ARG_POINT:
			cmd_set_arg_point(cmd, arg[i].name, arg[i].grid);
			break;
		case ARG_STRING:
			cmd_set_arg_string(cmd, arg[i].name, arg[i].str);
			break;
		case ARG_CHOICE:
			cmd_set_arg_choice(cmd, arg[i].name, arg[i].value);
			break;
		case ARG_DIRECTION:
			cmd_set_arg_direction(cmd, arg[i].name, arg[i].value);
			break;
		case ARG_TARGET:
			cmd_set_arg_target(cmd, arg[i].name, arg[i].value);
			break;
		default:
			cmd_set_arg_number(cmd, arg[i].name, arg[i].value);
			break;
		}
	}

	command_nudge();

	return command_done(ip);
}

/**
 * The sideways seam: the game's input hooks, answerable from Tcl.
 *
 * game-input.h holds eighteen function pointers through which the game asks
 * the interface a question -- how many, which direction, are you sure -- and
 * textui_input_init() fills every one of them with the curses answer.  This
 * lets a script take one over and give it back.
 *
 * One at a time is the point.  T4 to T8 each replace a single hook with a
 * native dialog; the rest keep answering the way they always did, so the game
 * is playable at every step rather than only at the end.  That is why the
 * original is kept and restored rather than overwritten, and why a script that
 * fails falls through to it: a dialog with a bug in it must not be able to
 * wedge the game at a prompt.
 *
 * A script is a command prefix.  The hook's own arguments are appended to it,
 * and it answers with a Tcl list: the empty list means the player cancelled,
 * and anything else carries the answer in its first element.  Uniform across
 * all of them, because "" is a legitimate answer to some of these and a
 * refusal in others.
 */
enum hook_id {
	HOOK_STRING, HOOK_QUANTITY, HOOK_CHECK, HOOK_COM,
	HOOK_REP_DIR, HOOK_AIM_DIR, HOOK_POINT, HOOK_CONFIRM_DEBUG,
	HOOK_MAX
};

/*
 * Any hook, held as a function pointer of no particular type and cast back to
 * its own at the call.  Not void *: ISO C has no conversion between function
 * and object pointers, and GCC says so under -pedantic -- which the Linux and
 * Windows builds compile with -- where clang, on the Mac, never did.
 */
typedef void (*hook_fn)(void);

static struct hook_slot {
	const char *name;
	hook_fn *pointer;	/* the game's variable */
	hook_fn trampoline;	/* ours, or NULL if not answerable yet */
	hook_fn original;	/* textui's, kept so it can be given back */
	Tcl_Obj *script;	/* the command prefix, or NULL */
} hooks[HOOK_MAX];

static bool hooks_ready;	/* textui_input_init has run */

/**
 * Run a hook's script, with the hook's own arguments appended.
 *
 * Returns the result as a list, or NULL if the script failed -- in which case
 * the caller hands the question to the original hook.  A failure is reported
 * where it can be seen rather than swallowed: a dialog that silently stops
 * answering looks like the game hanging.
 */
static Tcl_Obj *hook_eval(struct hook_slot *h, int n, Tcl_Obj **extra)
{
	Tcl_Obj *cmd;
	Tcl_Obj *res;
	int i;

	cmd = Tcl_DuplicateObj(h->script);
	Tcl_IncrRefCount(cmd);
	for (i = 0; i < n; i++) {
		if (Tcl_ListObjAppendElement(interp, cmd, extra[i]) != TCL_OK) {
			Tcl_DecrRefCount(cmd);
			return NULL;
		}
	}

	if (Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL) != TCL_OK) {
		/*
		 * Unlike the event dispatcher above, this one is load-bearing: the
		 * script here is the front end's own hook, run directly, so a failure
		 * in it arrives nowhere else.  Before this, a native dialog that threw
		 * silently handed the question back to textui -- the game carried on
		 * with the terminal prompt and the only account of why went to a
		 * stderr that is /dev/null for a bundled application.  The caller's
		 * fallback is still right; it just should not be quiet.
		 */
		Tcl_BackgroundException(interp, TCL_ERROR);
		plog_fmt("Tcl/Tk: %s hook: %s", h->name,
				Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
		Tcl_DecrRefCount(cmd);
		return NULL;
	}

	Tcl_DecrRefCount(cmd);

	res = Tcl_GetObjResult(interp);

	/*
	 * The result object belongs to the interpreter and the next evaluation
	 * will overwrite it, so take a reference the caller can read at leisure.
	 * This is the same trap as the "called with shared object" panic earlier
	 * in this file, approached from the other side.
	 */
	Tcl_IncrRefCount(res);

	return res;
}

/**
 * The answer, or nothing.
 *
 * Pulls the first element out of the list a script returned, releasing the
 * result either way.  A one-line wrapper, but every trampoline below needs it
 * and each one getting the reference counting right by itself is how the
 * earlier crash happened.
 */
static bool hook_answer(Tcl_Obj *res, Tcl_Obj **out)
{
	Tcl_Obj **elem;
	Tcl_Size n;

	*out = NULL;

	if (!res) return false;

	if (Tcl_ListObjGetElements(interp, res, &n, &elem) != TCL_OK || n < 1) {
		Tcl_DecrRefCount(res);
		return false;
	}

	*out = elem[0];
	Tcl_IncrRefCount(*out);
	Tcl_DecrRefCount(res);

	return true;
}

/**
 * A script's yes or no.
 *
 * Not Tcl_GetBooleanFromObj: in Tcl 9 that is a macro whose unused branch is a
 * GNU statement expression, and the game builds with -pedantic.  This is the
 * call it expands to for an int target -- the flags word carries the size of
 * the destination -- said plainly instead.
 */
static bool obj_true(Tcl_Obj *o)
{
	int yes = 0;

	if (Tcl_GetBoolFromObj(NULL, o, (int)sizeof(yes), (char *)&yes) != TCL_OK)
		return false;

	return yes ? true : false;
}

static bool tcl_get_string(const char *prompt, char *buf, size_t len)
{
	struct hook_slot *h = &hooks[HOOK_STRING];
	Tcl_Obj *arg[2], *answer;
	Tcl_Obj *res;

	arg[0] = Tcl_NewStringObj(prompt, -1);
	arg[1] = Tcl_NewStringObj(buf, -1);
	res = hook_eval(h, 2, arg);

	if (!res && h->original)
		return ((bool (*)(const char *, char *, size_t))h->original)
				(prompt, buf, len);

	if (!hook_answer(res, &answer)) return false;

	my_strcpy(buf, Tcl_GetString(answer), len);
	Tcl_DecrRefCount(answer);

	return true;
}

static int tcl_get_quantity(const char *prompt, int max)
{
	struct hook_slot *h = &hooks[HOOK_QUANTITY];
	Tcl_Obj *arg[2], *answer, *res;
	int amt = 0;

	arg[0] = Tcl_NewStringObj(prompt ? prompt : "", -1);
	arg[1] = Tcl_NewIntObj(max);
	res = hook_eval(h, 2, arg);

	if (!res && h->original)
		return ((int (*)(const char *, int))h->original)(prompt, max);

	if (!hook_answer(res, &answer)) return 0;

	if (Tcl_GetIntFromObj(NULL, answer, &amt) != TCL_OK) amt = 0;
	Tcl_DecrRefCount(answer);

	/* The game trusts this: get_quantity's callers index with it. */
	if (amt < 0) amt = 0;
	if (amt > max) amt = max;

	return amt;
}

static bool tcl_get_check(const char *prompt)
{
	struct hook_slot *h = &hooks[HOOK_CHECK];
	Tcl_Obj *arg[1], *answer, *res;
	bool yes;

	arg[0] = Tcl_NewStringObj(prompt, -1);
	res = hook_eval(h, 1, arg);

	if (!res && h->original)
		return ((bool (*)(const char *))h->original)(prompt);

	/* No answer is "no": that is what escape means at this prompt. */
	if (!hook_answer(res, &answer)) return false;

	yes = obj_true(answer);
	Tcl_DecrRefCount(answer);

	return yes;
}

static bool tcl_get_com(const char *prompt, char *command)
{
	struct hook_slot *h = &hooks[HOOK_COM];
	Tcl_Obj *arg[1], *answer, *res;
	const char *s;

	arg[0] = Tcl_NewStringObj(prompt, -1);
	res = hook_eval(h, 1, arg);

	if (!res && h->original)
		return ((bool (*)(const char *, char *))h->original)(prompt, command);

	if (!hook_answer(res, &answer)) return false;

	s = Tcl_GetString(answer);
	*command = s[0];
	Tcl_DecrRefCount(answer);

	return *command ? true : false;
}

static bool tcl_get_rep_dir(int *dir, bool allow_none)
{
	struct hook_slot *h = &hooks[HOOK_REP_DIR];
	Tcl_Obj *arg[1], *answer, *res;
	int d;

	arg[0] = Tcl_NewBooleanObj(allow_none);
	res = hook_eval(h, 1, arg);

	if (!res && h->original)
		return ((bool (*)(int *, bool))h->original)(dir, allow_none);

	if (!hook_answer(res, &answer)) return false;

	if (Tcl_GetIntFromObj(NULL, answer, &d) != TCL_OK) {
		Tcl_DecrRefCount(answer);
		return false;
	}
	Tcl_DecrRefCount(answer);

	if (d < 0 || d > 9) return false;

	*dir = d;

	return true;
}

static bool tcl_get_aim_dir(int *dir)
{
	struct hook_slot *h = &hooks[HOOK_AIM_DIR];
	Tcl_Obj *answer, *res;
	int d;

	res = hook_eval(h, 0, NULL);

	if (!res && h->original)
		return ((bool (*)(int *))h->original)(dir);

	if (!hook_answer(res, &answer)) return false;

	if (Tcl_GetIntFromObj(NULL, answer, &d) != TCL_OK) {
		Tcl_DecrRefCount(answer);
		return false;
	}
	Tcl_DecrRefCount(answer);

	*dir = d;

	return true;
}

static bool tcl_get_point(struct loc *grid)
{
	struct hook_slot *h = &hooks[HOOK_POINT];
	Tcl_Obj *answer, *res, **xy;
	Tcl_Size n;
	int x, y;

	res = hook_eval(h, 0, NULL);

	if (!res && h->original)
		return ((bool (*)(struct loc *))h->original)(grid);

	if (!hook_answer(res, &answer)) return false;

	if (Tcl_ListObjGetElements(interp, answer, &n, &xy) != TCL_OK || n != 2
			|| Tcl_GetIntFromObj(NULL, xy[0], &x) != TCL_OK
			|| Tcl_GetIntFromObj(NULL, xy[1], &y) != TCL_OK) {
		Tcl_DecrRefCount(answer);
		return false;
	}
	Tcl_DecrRefCount(answer);

	*grid = loc(x, y);

	return true;
}

static bool tcl_confirm_debug(void)
{
	struct hook_slot *h = &hooks[HOOK_CONFIRM_DEBUG];
	Tcl_Obj *answer, *res;
	bool yes;

	res = hook_eval(h, 0, NULL);

	if (!res && h->original)
		return ((bool (*)(void))h->original)();

	if (!hook_answer(res, &answer)) return false;

	yes = obj_true(answer);
	Tcl_DecrRefCount(answer);

	return yes;
}

/**
 * Fill in the table.
 *
 * Not a static initialiser: the hook variables are not compile-time constants
 * everywhere, and the casts through hook_fn want to be in one place where they
 * can be read against game-input.h line by line.
 */
static void hooks_init(void)
{
	struct hook_slot *h = hooks;

	h[HOOK_STRING].name = "string";
	h[HOOK_STRING].pointer = (hook_fn *)&get_string_hook;
	h[HOOK_STRING].trampoline = (hook_fn)tcl_get_string;

	h[HOOK_QUANTITY].name = "quantity";
	h[HOOK_QUANTITY].pointer = (hook_fn *)&get_quantity_hook;
	h[HOOK_QUANTITY].trampoline = (hook_fn)tcl_get_quantity;

	h[HOOK_CHECK].name = "check";
	h[HOOK_CHECK].pointer = (hook_fn *)&get_check_hook;
	h[HOOK_CHECK].trampoline = (hook_fn)tcl_get_check;

	h[HOOK_COM].name = "com";
	h[HOOK_COM].pointer = (hook_fn *)&get_com_hook;
	h[HOOK_COM].trampoline = (hook_fn)tcl_get_com;

	h[HOOK_REP_DIR].name = "rep_dir";
	h[HOOK_REP_DIR].pointer = (hook_fn *)&get_rep_dir_hook;
	h[HOOK_REP_DIR].trampoline = (hook_fn)tcl_get_rep_dir;

	h[HOOK_AIM_DIR].name = "aim_dir";
	h[HOOK_AIM_DIR].pointer = (hook_fn *)&get_aim_dir_hook;
	h[HOOK_AIM_DIR].trampoline = (hook_fn)tcl_get_aim_dir;

	h[HOOK_POINT].name = "point";
	h[HOOK_POINT].pointer = (hook_fn *)&get_point_hook;
	h[HOOK_POINT].trampoline = (hook_fn)tcl_get_point;

	h[HOOK_CONFIRM_DEBUG].name = "confirm_debug";
	h[HOOK_CONFIRM_DEBUG].pointer = (hook_fn *)&confirm_debug_hook;
	h[HOOK_CONFIRM_DEBUG].trampoline = (hook_fn)tcl_confirm_debug;
}

/**
 * Put a slot's trampoline in place, or take it out again.
 *
 * Only ever called once textui_input_init() has run, so that what is saved as
 * the original is textui's answer and not a null pointer.  A script asking for
 * a hook before then is remembered and installed at that point; see
 * hooks_apply.
 */
static void hook_install(struct hook_slot *h)
{
	if (h->script && !h->original) {
		h->original = *h->pointer;
		*h->pointer = h->trampoline;
	} else if (!h->script && h->original) {
		*h->pointer = h->original;
		h->original = NULL;
	}
}

/**
 * Install everything a script asked for before the game's own hooks existed.
 */
static void hooks_apply(void)
{
	int i;

	hooks_ready = true;

	for (i = 0; i < HOOK_MAX; i++)
		hook_install(&hooks[i]);
}

/**
 * angband_hook -- take over one of the game's prompts, or give it back.
 *
 *    angband_hook                      the table, as {name taken}
 *    angband_hook check                the script answering it, or ""
 *    angband_hook check my_yes_no      answer it with "my_yes_no <prompt>"
 *    angband_hook check {}             give it back to the game
 */
static int objcmd_hook(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	struct hook_slot *h = NULL;
	const char *name;
	int i;

	(void)dummy;

	if (objc == 1) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < HOOK_MAX; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(hooks[i].name, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(hooks[i].script != NULL));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);

		return TCL_OK;
	}

	if (objc > 3) {
		Tcl_WrongNumArgs(ip, 1, objv, "?name? ?script?");
		return TCL_ERROR;
	}

	name = Tcl_GetString(objv[1]);
	for (i = 0; i < HOOK_MAX; i++) {
		if (strcmp(name, hooks[i].name) == 0) {
			h = &hooks[i];
			break;
		}
	}
	if (!h) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"no such hook: %s -- angband_hook with no arguments lists them",
				name));
		return TCL_ERROR;
	}

	if (objc == 2) {
		Tcl_SetObjResult(ip, h->script ? h->script : Tcl_NewObj());
		return TCL_OK;
	}

	if (h->script) {
		Tcl_DecrRefCount(h->script);
		h->script = NULL;
	}

	if (Tcl_GetCharLength(objv[2]) > 0) {
		h->script = objv[2];
		Tcl_IncrRefCount(h->script);
	}

	/*
	 * Before textui_input_init() has run there is nothing to save as the
	 * original, so the request is remembered and hooks_apply puts it in at the
	 * same point the subwindow flags go in -- the game's first request for
	 * input, which is past everything that would otherwise overwrite it.
	 */
	if (hooks_ready) hook_install(h);

	return TCL_OK;
}

/**
 * angband_ask -- ask a question the way the game asks it.
 *
 *    angband_ask check "Are you sure? "
 *    angband_ask quantity "How many? " 40
 *    angband_ask point
 *
 * This goes through game-input.c's get_check() and friends, which is to say
 * through whatever hook is currently installed.  With a script on the hook it
 * is a round trip -- Tcl asks the game to ask the interface, and the interface
 * is Tcl -- which is how the sideways seam gets tested without a character to
 * play.  It is also how a dialog gets looked at during development without
 * having to reach the point in the game that raises it.
 *
 * With no script on the hook it is the curses prompt, which waits for a key.
 * That is the honest answer to "what would the game do", and it is why the
 * test only asks about hooks it has taken over.
 */
static int objcmd_ask(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "hook ?argument ...?");
		return TCL_ERROR;
	}

	what = Tcl_GetString(objv[1]);

	if (strcmp(what, "check") == 0 && objc == 3) {
		Tcl_SetObjResult(ip,
				Tcl_NewBooleanObj(get_check(Tcl_GetString(objv[2]))));
		return TCL_OK;
	}

	if (strcmp(what, "string") == 0 && (objc == 3 || objc == 4)) {
		char buf[256];

		my_strcpy(buf, objc == 4 ? Tcl_GetString(objv[3]) : "", sizeof(buf));
		if (!get_string(Tcl_GetString(objv[2]), buf, sizeof(buf)))
			Tcl_SetObjResult(ip, Tcl_NewObj());
		else
			Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, -1));
		return TCL_OK;
	}

	if (strcmp(what, "quantity") == 0 && objc == 4) {
		int max;

		if (Tcl_GetIntFromObj(ip, objv[3], &max) != TCL_OK) return TCL_ERROR;
		Tcl_SetObjResult(ip,
				Tcl_NewIntObj(get_quantity(Tcl_GetString(objv[2]), max)));
		return TCL_OK;
	}

	if (strcmp(what, "com") == 0 && objc == 3) {
		char c = 0;

		if (!get_com(Tcl_GetString(objv[2]), &c))
			Tcl_SetObjResult(ip, Tcl_NewObj());
		else
			Tcl_SetObjResult(ip, Tcl_NewStringObj(&c, 1));
		return TCL_OK;
	}

	if (strcmp(what, "rep_dir") == 0 && (objc == 2 || objc == 3)) {
		int dir = 0, none = 0;

		if (objc == 3 && !obj_true(objv[2])) none = 0;
		else if (objc == 3) none = 1;

		if (!get_rep_dir(&dir, none ? true : false))
			Tcl_SetObjResult(ip, Tcl_NewObj());
		else
			Tcl_SetObjResult(ip, Tcl_NewIntObj(dir));
		return TCL_OK;
	}

	if (strcmp(what, "aim_dir") == 0 && objc == 2) {
		int dir = 0;

		if (!get_aim_dir(&dir))
			Tcl_SetObjResult(ip, Tcl_NewObj());
		else
			Tcl_SetObjResult(ip, Tcl_NewIntObj(dir));
		return TCL_OK;
	}

	if (strcmp(what, "point") == 0 && objc == 2) {
		struct loc grid = loc(0, 0);

		if (!get_point(&grid)) {
			Tcl_SetObjResult(ip, Tcl_NewObj());
		} else {
			Tcl_Obj *xy = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, xy, Tcl_NewIntObj(grid.x));
			Tcl_ListObjAppendElement(ip, xy, Tcl_NewIntObj(grid.y));
			Tcl_SetObjResult(ip, xy);
		}
		return TCL_OK;
	}

	if (strcmp(what, "confirm_debug") == 0 && objc == 2) {
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(confirm_debug()));
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"cannot ask that, or not with those arguments: %s", what));

	return TCL_ERROR;
}

/**
 * angband_player -- read the character.
 *
 *    angband_player                 every field, as a name/value dict
 *    angband_player level           one field
 *    angband_player fields          the names, so a script can discover them
 *
 * The first of the big read families, and the shape the rest follow.  Three
 * decisions are settled here rather than argued again in T5 to T9:
 *
 * **The names are the old ones.**  Section 3.3's rule -- keep the original's
 * command names wherever this tree has the same concept -- applies to the
 * field names too, so `armor_class` rather than `ac` and `blows_per_round`
 * rather than `num_blows`.  Measured across the original's Tcl, `angband
 * player <thing>` is 434 calls with 48 distinct things; naming them anything
 * else turns an adaptation back into a rewrite.
 *
 * **One call per field, and one call for all of them.**  A status line reads
 * one thing; a character sheet reads forty.  Making the second of those forty
 * round trips through Tcl is the sort of thing that is invisible until a
 * window redraws on every game event.
 *
 * **Reads only.**  Everything here is derived by the game and recomputed by
 * it; the accessor never calculates.  `armor_class` is `state.ac + state.to_a`
 * because that is what the game's own display does, not because this file has
 * an opinion about armour.
 */
enum player_field_type {
	PF_INT,		/* a number */
	PF_STR,		/* a string */
	PF_BOOL,	/* a truth */
	PF_REAL,	/* a number with a fraction in it */
	PF_PAIR,	/* two numbers, like {current maximum} or {x y} */
	PF_LIST		/* however many the field has */
};

enum player_field_id {
	PFI_NAME, PFI_RACE, PFI_CLASS, PFI_TITLE, PFI_HISTORY,
	PFI_LEVEL, PFI_MAX_LEV, PFI_EXP, PFI_MAX_EXP, PFI_EXP_TO_ADVANCE,
	PFI_NEXT_LEVEL,
	PFI_GOLD, PFI_DEPTH, PFI_PLACE, PFI_MAX_DEPTH, PFI_POSITION,
	PFI_IN_WILD,
	PFI_HITPOINTS, PFI_MANA, PFI_ARMOR_CLASS, PFI_TO_HIT, PFI_TO_DAM,
	PFI_BLOWS_PER_ROUND, PFI_SHOTS_PER_ROUND, PFI_SPEED, PFI_INFRAVISION,
	PFI_LIGHT, PFI_AGE, PFI_HEIGHT, PFI_WEIGHT, PFI_TOTAL_WEIGHT,
	PFI_NEW_SPELLS, PFI_RUNNING, PFI_RESTING, PFI_IS_DEAD, PFI_DIED_FROM,
	PFI_INSIDE_ARENA, PFI_WIZARD, PFI_DEBUG, PFI_TURN, PFI_METERS,
	PFI_MAX
};

static const struct {
	const char *name;
	enum player_field_type type;
} player_field[] = {
	{ "name",				PF_STR },
	{ "race",				PF_STR },
	{ "class",				PF_STR },
	{ "title",				PF_STR },
	{ "history",			PF_STR },
	{ "level",				PF_INT },
	{ "max_lev",			PF_INT },
	{ "exp",				PF_INT },
	{ "max_exp",			PF_INT },
	{ "exp_to_advance",		PF_INT },
	{ "next_level",			PF_PAIR },
	{ "gold",				PF_INT },
	{ "depth",				PF_INT },
	{ "place",				PF_STR },
	{ "max_depth",			PF_INT },
	{ "position",			PF_PAIR },
	{ "in_wild",			PF_BOOL },
	{ "hitpoints",			PF_PAIR },
	{ "mana",				PF_PAIR },
	{ "armor_class",		PF_INT },
	{ "to_hit",				PF_INT },
	{ "to_dam",				PF_INT },
	{ "blows_per_round",	PF_REAL },
	{ "shots_per_round",	PF_REAL },
	{ "speed",				PF_INT },
	{ "infravision",		PF_INT },
	{ "light",				PF_INT },
	{ "age",				PF_INT },
	{ "height",				PF_INT },
	{ "weight",				PF_INT },
	{ "total_weight",		PF_INT },
	{ "new_spells",			PF_INT },
	{ "running",			PF_BOOL },
	{ "resting",			PF_BOOL },
	{ "is_dead",			PF_BOOL },
	{ "died_from",			PF_STR },
	{ "inside_arena",		PF_BOOL },
	{ "wizard",				PF_BOOL },
	{ "debug",				PF_BOOL },
	{ "turn",				PF_INT },
	{ "meters",				PF_LIST },
};

typedef char player_field_table_is_complete[
		(N_ELEMENTS(player_field) == PFI_MAX) ? 1 : -1];

/**
 * The class title for the character's level.
 *
 * Ten titles over fifty levels, so one every five, and the last one has to
 * survive a character who has reached level 50 exactly.
 */
static const char *player_title(void)
{
	int i;

	if (!player->class) return "";

	i = (player->lev - 1) / 5;
	if (i < 0) i = 0;
	if (i > 9) i = 9;

	return player->class->title[i] ? player->class->title[i] : "";
}

/**
 * How much experience the next level wants, or zero at the top.
 *
 * The game keeps the table and the expfact; doing the arithmetic anywhere but
 * here would be a second opinion about levelling.
 */
static int player_exp_to_advance(void)
{
	if (player->lev >= PY_MAX_LEVEL) return 0;

	return (int)(player_exp[player->lev - 1] * player->expfact / 100L)
			- player->exp;
}

/*
 * Progress through the current level: what has been gained since the level
 * began, and what the next one wants on top of that.  Both zero at the top of
 * the table.
 */
static void player_level_progress(long *gained, long *wanted)
{
	long base = (player->lev > 1)
			? player_exp[player->lev - 2] * player->expfact / 100L : 0;
	long want = (player->lev < PY_MAX_LEVEL)
			? player_exp[player->lev - 1] * player->expfact / 100L : base;

	*gained = (player->lev < PY_MAX_LEVEL) ? player->exp - base : 0;
	*wanted = want - base;
}

/*
 * One of the Progress pane's meters: {name value current maximum colour
 * state}.  The state is the colour's meaning -- good, fair or poor -- for a
 * bar, which has to be drawn in colours that read on grey rather than the
 * game's, which are for black.
 */
static Tcl_Obj *player_meter(const char *name, long value, long cur, long max,
		uint8_t attr)
{
	Tcl_Obj *row = Tcl_NewListObj(0, NULL);
	const char *state;

	switch (attr) {
		case COLOUR_GREEN: case COLOUR_L_GREEN:		state = "good"; break;
		case COLOUR_YELLOW: case COLOUR_ORANGE:		state = "fair"; break;
		case COLOUR_RED: case COLOUR_L_RED:			state = "poor"; break;
		default:									state = ""; break;
	}

	Tcl_ListObjAppendElement(NULL, row, Tcl_NewStringObj(name, -1));
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewWideIntObj(value));
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewWideIntObj(cur));
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewWideIntObj(max));
	Tcl_ListObjAppendElement(NULL, row,
			Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1));
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewStringObj(state, -1));
	return row;
}

static Tcl_Obj *player_field_obj(enum player_field_id id)
{
	Tcl_Obj *pair;

	switch (id) {
	case PFI_NAME:		return Tcl_NewStringObj(player->full_name, -1);
	case PFI_RACE:		return Tcl_NewStringObj(
								player->race ? player->race->name : "", -1);
	case PFI_CLASS:		return Tcl_NewStringObj(
								player->class ? player->class->name : "", -1);
	case PFI_TITLE:		return Tcl_NewStringObj(player_title(), -1);
	case PFI_HISTORY:	return Tcl_NewStringObj(
								player->history ? player->history : "", -1);

	case PFI_LEVEL:		return Tcl_NewIntObj(player->lev);
	case PFI_MAX_LEV:	return Tcl_NewIntObj(player->max_lev);
	case PFI_EXP:		return Tcl_NewIntObj((int)player->exp);
	case PFI_MAX_EXP:	return Tcl_NewIntObj((int)player->max_exp);
	case PFI_EXP_TO_ADVANCE:	return Tcl_NewIntObj(player_exp_to_advance());

	/*
	 * Progress through the current level, as {gained wanted} -- what a meter
	 * needs and what exp_to_advance on its own cannot give, because the bar
	 * has to start from the threshold the level began at rather than from
	 * zero.  At the top of the table there is nothing left to gain, and the
	 * pair is {0 0}, which the meter draws as an empty track.
	 */
	case PFI_NEXT_LEVEL: {
		long gained, wanted;

		player_level_progress(&gained, &wanted);
		pair = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj((int)gained));
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj((int)wanted));
		return pair;
	}

	/*
	 * The Progress pane's four meters, a {name value current maximum colour
	 * state} row each: hp, sp, food and exp.  The colours are the game's judgement
	 * of how things stand, so they are made here rather than in a script --
	 * player_hp_attr() and player_sp_attr() as the sidebar has them, the
	 * hunger grade's colour as the status line has it, and experience green
	 * unless it has been drained, as prt_exp() has it.  Food's maximum is
	 * PY_FOOD_MAX, the top of the scale the grades are cut from.  Experience's
	 * value is the total and its bar is the way through this level.
	 */
	case PFI_METERS: {
		struct timed_grade *grade = timed_effects[TMD_FOOD].grade;
		int food = player->timed[TMD_FOOD];
		long gained, wanted;
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		while (grade->next && food > grade->max) grade = grade->next;
		player_level_progress(&gained, &wanted);

		Tcl_ListObjAppendElement(NULL, list, player_meter("hp",
				player->chp, player->chp, player->mhp,
				player_hp_attr(player)));
		Tcl_ListObjAppendElement(NULL, list, player_meter("sp",
				player->csp, player->csp, player->msp,
				player_sp_attr(player)));
		Tcl_ListObjAppendElement(NULL, list, player_meter("food",
				food, food, PY_FOOD_MAX, grade->color));
		Tcl_ListObjAppendElement(NULL, list, player_meter("exp",
				player->exp, gained, wanted,
				player->exp >= player->max_exp
					? COLOUR_L_GREEN : COLOUR_YELLOW));
		return list;
	}
	case PFI_GOLD:		return Tcl_NewIntObj((int)player->au);
	case PFI_DEPTH:		return Tcl_NewIntObj(player->depth);
	case PFI_MAX_DEPTH:	return Tcl_NewIntObj(player->max_depth);

	/*
	 * Where the player is, as the sidebar says it: a dungeon depth, or at
	 * depth zero the town they are standing in or "wilderness".  Depth zero is
	 * the whole world here, so a window that turned it into "Town" would be
	 * wrong a thousand grids out in open country.
	 */
	case PFI_PLACE: {
		char buf[64];

		fmt_depth(buf, sizeof(buf));
		return Tcl_NewStringObj(buf, -1);
	}
	case PFI_IN_WILD:	return Tcl_NewBooleanObj(player->in_wild);

	case PFI_POSITION:
		pair = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->grid.x));
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->grid.y));
		return pair;

	/* Current first, then the maximum: the order they are read aloud in. */
	case PFI_HITPOINTS:
		pair = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->chp));
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->mhp));
		return pair;

	case PFI_MANA:
		pair = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->csp));
		Tcl_ListObjAppendElement(NULL, pair, Tcl_NewIntObj(player->msp));
		return pair;

	/*
	 * The displayed armour class, which is the base plus the bonus -- what
	 * the sidebar shows and what the old scripts meant by armor_class.
	 */
	case PFI_ARMOR_CLASS:
		return Tcl_NewIntObj(player->state.ac + player->state.to_a);

	case PFI_TO_HIT:			return Tcl_NewIntObj(player->state.to_h);
	case PFI_TO_DAM:			return Tcl_NewIntObj(player->state.to_d);
	/*
	 * Blows are stored times a hundred and shots times ten, which is the
	 * game's way of carrying a fraction in an int.  That is storage, not
	 * meaning: a script asking how many blows a round gets 1.33, the same
	 * number ui-player.c prints, and the trick stays in here.
	 */
	case PFI_BLOWS_PER_ROUND:
		return Tcl_NewDoubleObj(player->state.num_blows / 100.0);
	case PFI_SHOTS_PER_ROUND:
		return Tcl_NewDoubleObj(player->state.num_shots / 10.0);
	case PFI_SPEED:				return Tcl_NewIntObj(player->state.speed);
	case PFI_INFRAVISION:		return Tcl_NewIntObj(player->state.see_infra);
	case PFI_LIGHT:				return Tcl_NewIntObj(player->state.cur_light);

	case PFI_AGE:				return Tcl_NewIntObj(player->age);
	case PFI_HEIGHT:			return Tcl_NewIntObj(player->ht);
	case PFI_WEIGHT:			return Tcl_NewIntObj(player->wt);
	case PFI_TOTAL_WEIGHT:
		return Tcl_NewIntObj(player->upkeep->total_weight);

	case PFI_NEW_SPELLS:	return Tcl_NewIntObj(player->upkeep->new_spells);
	case PFI_RUNNING:		return Tcl_NewBooleanObj(player->upkeep->running);
	case PFI_RESTING:		return Tcl_NewBooleanObj(player->upkeep->resting);
	case PFI_IS_DEAD:		return Tcl_NewBooleanObj(player->is_dead);
	case PFI_DIED_FROM:		return Tcl_NewStringObj(player->died_from, -1);
	case PFI_INSIDE_ARENA:
		return Tcl_NewBooleanObj(player->upkeep->arena_level);
	/*
	 * Two different things, and conflating them is why the Debug menu did not
	 * appear for a character in wizard mode.
	 *
	 * `wizard` is player->wizard, toggled by ^W: it is what puts [=-WIZARD-=]
	 * in the sidebar and "Cheat" on the status line, and it is what the player
	 * means by "wizard mode is on".
	 *
	 * `debug` is NOSCORE_DEBUG, which only confirm_debug() sets -- the first
	 * time ^A is used -- and which is what player_can_debug_prereq reads.  ^W
	 * never touches it.
	 */
	case PFI_WIZARD:
		return Tcl_NewBooleanObj(player->wizard);
	case PFI_DEBUG:
		return Tcl_NewBooleanObj((player->noscore & NOSCORE_DEBUG) != 0);
	case PFI_TURN:			return Tcl_NewIntObj((int)turn);

	case PFI_MAX:
		break;
	}

	return Tcl_NewObj();
}

static int objcmd_player(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i;

	(void)dummy;

	if (objc > 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "?field?");
		return TCL_ERROR;
	}

	/* The names alone, which needs no character to answer. */
	if (objc == 2 && strcmp(Tcl_GetString(objv[1]), "fields") == 0) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < PFI_MAX; i++)
			Tcl_ListObjAppendElement(ip, list,
					Tcl_NewStringObj(player_field[i].name, -1));
		Tcl_SetObjResult(ip, list);

		return TCL_OK;
	}

	/*
	 * The name is checked before the character is, so a typo is reported as a
	 * typo whether or not a game is in progress.  The table is static; only
	 * the values need somebody to be playing.
	 */
	if (objc == 2) {
		what = Tcl_GetString(objv[1]);

		for (i = 0; i < PFI_MAX; i++)
			if (strcmp(what, player_field[i].name) == 0) break;

		if (i == PFI_MAX) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"no such field: %s -- \"angband_player fields\" lists them",
					what));
			return TCL_ERROR;
		}
	}

	/*
	 * Everything else reads the character, and player->upkeep and
	 * player->state with it.  Those exist from birth, but the front end is
	 * answering questions well before that.
	 */
	if (!player || !player->upkeep) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	if (objc == 1) {
		Tcl_Obj *dict = Tcl_NewListObj(0, NULL);

		for (i = 0; i < PFI_MAX; i++) {
			Tcl_ListObjAppendElement(ip, dict,
					Tcl_NewStringObj(player_field[i].name, -1));
			Tcl_ListObjAppendElement(ip, dict,
					player_field_obj((enum player_field_id)i));
		}
		Tcl_SetObjResult(ip, dict);

		return TCL_OK;
	}

	/* Checked above, so i is the field. */
	Tcl_SetObjResult(ip, player_field_obj((enum player_field_id)i));

	return TCL_OK;
}

/**
 * angband_monster -- the monster knowledge the character has.
 *
 *    angband_monster max              how many races there are
 *    angband_monster list ?pattern?   {index name level unique kills} rows
 *    angband_monster info <index>     one race, as a name/value dict
 *    angband_monster lore <index>     the recall text, as the game writes it
 *
 * The second read family, and it is a different shape from angband_player on
 * purpose: this is a table of 700-odd rows, not one record, so it is queried
 * rather than fetched.  That shape is the original's too -- `angband r_info`
 * took info, find, set and max, and the scripts asked it for name, unique and
 * icon 74 times between them -- which is the naming rule from section 3.3
 * applied to a command's arguments rather than to its name.
 *
 * `set` is not here and will not be: the original let a script write to
 * r_info, and a front end that can edit the monster list is a front end that
 * can cheat.  Reading is the whole job.
 *
 * What it reports is what the character knows.  A race nobody has met has no
 * lore, and `list` says so by leaving it out rather than by naming it and
 * showing zeroes.
 */

/**
 * A textblock as a Tcl string.
 *
 * The game writes its recall into wide characters with a colour per
 * character; the colours are dropped here, because the windows set the
 * passage in one colour and the alternative is a tagged text widget nobody
 * has asked for yet.
 */
static Tcl_Obj *textblock_to_obj(textblock *tb)
{
	const wchar_t *text = textblock_text(tb);
	Tcl_DString utf;
	Tcl_Obj *result;
	size_t i;

	Tcl_DStringInit(&utf);

	for (i = 0; text[i]; i++) {
		char buf[TCL_UTF_MAX + 1];
		Tcl_Size n = Tcl_UniCharToUtf((int)text[i], buf);

		Tcl_DStringAppend(&utf, buf, n);
	}

	result = Tcl_NewStringObj(Tcl_DStringValue(&utf),
			Tcl_DStringLength(&utf));
	Tcl_DStringFree(&utf);

	return result;
}

/**
 * Add `glyph` and `colour` to an info dict: the character the game prints for
 * a thing in text mode, and its colour as the map's "#rrggbb".
 *
 * The d_ fields, not the x_ ones.  With a tile set in use the x_ fields hold a
 * tile's coordinates rather than a character, and printed as one they are a
 * random letter in a random colour.
 */
static void glyph_put(Tcl_Interp *ip, Tcl_Obj *d, wchar_t ch, uint8_t attr)
{
	char buf[TCL_UTF_MAX + 1];
	Tcl_Size n = Tcl_UniCharToUtf((int)ch, buf);

	Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj("glyph", -1));
	Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(buf, n));
	Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj("colour", -1));
	Tcl_ListObjAppendElement(ip, d,
			Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1));
}

/**
 * Does an object kind look like its flavour?  The way the map decides it: yes,
 * except for a scroll that has been identified, which has a glyph of its own.
 * That rule is use_flavor_glyph() in ui-object.c, which is static.
 */
static bool kind_shows_flavour(const struct object_kind *kind)
{
	return kind->flavor && !(kind->tval == TV_SCROLL && kind->aware);
}

/**
 * An object kind's glyph, chosen the way the map chooses it.
 */
static void kind_glyph_put(Tcl_Interp *ip, Tcl_Obj *d,
		const struct object_kind *kind)
{
	if (kind_shows_flavour(kind)) {
		glyph_put(ip, d, kind->flavor->d_char, kind->flavor->d_attr);
	} else {
		glyph_put(ip, d, kind->d_char, kind->d_attr);
	}
}

/**
 * Has the character met this one?
 *
 * Sights or kills, in this life or a previous one -- which is what the
 * knowledge screens use, and means a race read about in a previous character's
 * lore stays read about.
 */
static bool monster_known(const struct monster_lore *lore)
{
	return lore->sights > 0 || lore->tkills > 0;
}

static Tcl_Obj *monster_info_obj(Tcl_Interp *ip, int idx)
{
	struct monster_race *race = &r_info[idx];
	struct monster_lore *lore = get_lore(race);
	Tcl_Obj *d = Tcl_NewListObj(0, NULL);

#define PUT(key, value) \
	do { \
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj((key), -1)); \
		Tcl_ListObjAppendElement(ip, d, (value)); \
	} while (0)

	PUT("index", Tcl_NewIntObj(idx));
	PUT("name", Tcl_NewStringObj(race->name ? race->name : "", -1));
	PUT("plural", Tcl_NewStringObj(race->plural ? race->plural : "", -1));
	PUT("base", Tcl_NewStringObj(
			(race->base && race->base->name) ? race->base->name : "", -1));
	PUT("unique", Tcl_NewBooleanObj(rf_has(race->flags, RF_UNIQUE)));
	PUT("level", Tcl_NewIntObj(race->level));
	PUT("rarity", Tcl_NewIntObj(race->rarity));
	PUT("speed", Tcl_NewIntObj(race->speed));
	PUT("avg_hp", Tcl_NewIntObj(race->avg_hp));
	PUT("armor_class", Tcl_NewIntObj(race->ac));
	PUT("exp", Tcl_NewIntObj(race->mexp));
	glyph_put(ip, d, race->d_char, race->d_attr);

	/* What this character, and this character's line, has learned. */
	PUT("known", Tcl_NewBooleanObj(monster_known(lore)));
	PUT("sights", Tcl_NewIntObj(lore->sights));
	PUT("kills", Tcl_NewIntObj(lore->tkills));
	PUT("kills_this_life", Tcl_NewIntObj(lore->pkills));
	PUT("deaths", Tcl_NewIntObj(lore->deaths));

#undef PUT

	return d;
}

static int objcmd_monster(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int idx;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "max|list|info|lore ?argument ...?");
		return TCL_ERROR;
	}

	what = Tcl_GetString(objv[1]);

	/*
	 * r_info is read at startup and is there before a character is, so max
	 * and list answer without one.  info and lore read the lore, which is
	 * per character.
	 */
	/*
	 * l_list as well as the other two: the lore is allocated with the monster
	 * list but the front end answers questions before init_angband has read
	 * either, and get_lore indexes straight into it.
	 */
	if (!r_info || !z_info || !l_list) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the monster list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (strcmp(what, "max") == 0 && objc == 2) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj((int)z_info->r_max));
		return TCL_OK;
	}

	/*
	 * The list a browser draws: one row per race the character knows, as
	 * {index name level unique kills}.  A pattern filters by name, matched
	 * the way Tcl matches, because a browser's search box is the caller's
	 * problem and glob is what a Tcl caller already has.
	 */
	if (strcmp(what, "list") == 0 && (objc == 2 || objc == 3)) {
		const char *pattern = (objc == 3) ? Tcl_GetString(objv[2]) : NULL;
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);
		int i;

		for (i = 1; i < (int)z_info->r_max; i++) {
			struct monster_race *race = &r_info[i];
			struct monster_lore *lore;
			Tcl_Obj *row;

			if (!race->name) continue;

			lore = get_lore(race);
			if (!monster_known(lore)) continue;
			/*
			 * Name or base, and case-insensitively: the player types what
			 * they remember, which is as often "canine" as it is "jackal".
			 */
			if (pattern
					&& !Tcl_StringCaseMatch(race->name, pattern,
							TCL_MATCH_NOCASE)
					&& !(race->base && race->base->name
						&& Tcl_StringCaseMatch(race->base->name, pattern,
							TCL_MATCH_NOCASE)))
				continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(race->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(race->level));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(rf_has(race->flags, RF_UNIQUE)));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(lore->tkills));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if ((strcmp(what, "info") == 0 || strcmp(what, "lore") == 0)
			&& objc == 3) {
		if (Tcl_GetIntFromObj(ip, objv[2], &idx) != TCL_OK) return TCL_ERROR;

		if (idx < 1 || idx >= (int)z_info->r_max || !r_info[idx].name) {
			Tcl_SetObjResult(ip,
					Tcl_ObjPrintf("no such monster: %d", idx));
			return TCL_ERROR;
		}

		if (strcmp(what, "info") == 0) {
			Tcl_SetObjResult(ip, monster_info_obj(ip, idx));
			return TCL_OK;
		}

		/*
		 * The game's own recall, not a rewrite of it.  lore_description is
		 * what the "look" command and the knowledge screens both use, so a
		 * window built on this cannot drift from what the game says -- and
		 * spoilers stay false, because the front end has no business knowing
		 * more than the character does.
		 */
		{
			struct monster_race *race = &r_info[idx];
			textblock *tb = textblock_new();
			Tcl_Obj *text;

			lore_description(tb, race, get_lore(race), false);
			text = textblock_to_obj(tb);
			textblock_free(tb);

			Tcl_SetObjResult(ip, text);
			return TCL_OK;
		}
	}

	/*
	 * A verb this does know, given the wrong number of arguments, is an arity
	 * complaint and not an unknown verb.  Saying so matters because the
	 * sentence below otherwise comes out as "expected max, list, info or
	 * lore, not: max", which contradicts itself and sends the reader looking
	 * for a spelling mistake that is not there.
	 */
	if (strcmp(what, "max") == 0 || strcmp(what, "list") == 0
			|| strcmp(what, "info") == 0 || strcmp(what, "lore") == 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "max | list ?pattern? | info n | lore n");
		return TCL_ERROR;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected max, list, info or lore, not: %s", what));

	return TCL_ERROR;
}

/**
 * angband_object -- the object knowledge the character has.
 *
 *    angband_object max              how many kinds there are
 *    angband_object list ?pattern?   {index name kind aware level cost} rows
 *    angband_object info <index>     one kind, as a name/value dict
 *    angband_object lore <index>     what the game says about it
 *
 * The same shape as angband_monster, for the same reason: a table of six
 * hundred rows is queried, not fetched.  `set` is absent here too.
 *
 * Flavours are the thing that makes this different from monsters.  An
 * unidentified potion is "a cloudy potion" and not "a Potion of Speed", and
 * object_kind_name is given the kind's own `aware` flag so it says whichever
 * of those the character has earned.  A knowledge browser that spoiled the
 * flavours would be worse than no browser.
 */

/**
 * Build the object the game would describe, for a kind that has no instance.
 *
 * This is desc_obj_fake()'s recipe from ui-knowledge.c, without its side
 * effects: that function also re-points the recall tracker and calls
 * handle_stuff, which is right for a screen the player asked for and wrong for
 * an accessor somebody read in passing.
 *
 * The caller owns both objects and must delete them.
 */
static struct object *object_fake(struct object_kind *kind,
		struct object **known)
{
	struct object *obj = object_new();

	*known = object_new();

	object_prep(obj, kind, 0, EXTREMIFY);

	/* A flavoured kind the character has not identified stays unknown. */
	if (kind->aware || !kind->flavor) object_copy(*known, obj);
	obj->known = *known;

	return obj;
}

static void object_fake_free(struct object *obj, struct object *known)
{
	object_delete(NULL, NULL, &known);
	object_delete(NULL, NULL, &obj);
}

/**
 * Does this kind belong in a knowledge browser?
 *
 * The game's own test, from textui_browse_object_knowledge: seen before, or
 * flavoured -- a flavoured kind is listed from the start so its flavour shows
 * until the character works out what it is -- and never an instant artifact,
 * which would give away that one exists.
 *
 * The first version of this used `everseen || aware`, which sounds like the
 * same thing and is not: a kind with no flavour is aware from birth, because
 * nobody needs to identify a wooden torch.  It listed 256 kinds for a
 * character who had seen a couple of dozen.
 *
 * tval zero is the placeholder kinds -- <unknown item>, <unknown treasure>,
 * <curse object> -- which are markers the game draws with, not things.  The
 * knowledge screen drops them through obj_group_order, which is static in
 * ui-knowledge.c, so this drops them by their tval instead.
 */
static bool object_kind_known(const struct object_kind *kind)
{
	if (kind->tval <= 0) return false;
	if (kf_has(kind->kind_flags, KF_INSTA_ART)) return false;

	return kind->everseen || kind->flavor != NULL;
}

static int objcmd_object(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	char name[120];
	int idx;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "max|list|info|lore ?argument ...?");
		return TCL_ERROR;
	}

	what = Tcl_GetString(objv[1]);

	if (!k_info || !z_info) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the object list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (strcmp(what, "max") == 0 && objc == 2) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj((int)z_info->k_max));
		return TCL_OK;
	}

	if (strcmp(what, "list") == 0 && (objc == 2 || objc == 3)) {
		const char *pattern = (objc == 3) ? Tcl_GetString(objv[2]) : NULL;
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);
		int i;

		for (i = 1; i < (int)z_info->k_max; i++) {
			struct object_kind *kind = &k_info[i];
			const char *tval;
			Tcl_Obj *row;

			if (!kind->name) continue;
			if (!object_kind_known(kind)) continue;

			object_kind_name(name, sizeof(name), kind, kind->aware);
			tval = tval_find_name(kind->tval);

			/*
			 * Name or kind.  An unidentified potion is called "Green" and
			 * nothing else -- the flavour is the whole name -- so searching
			 * for "potion" would find none of the forty-six this character
			 * has seen unless the kind counts too.
			 */
			if (pattern
					&& !Tcl_StringCaseMatch(name, pattern, TCL_MATCH_NOCASE)
					&& !(tval && Tcl_StringCaseMatch(tval, pattern,
							TCL_MATCH_NOCASE)))
				continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(tval ? tval : "", -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(kind->aware));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(kind->level));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if ((strcmp(what, "info") == 0 || strcmp(what, "lore") == 0)
			&& objc == 3) {
		struct object_kind *kind;

		if (Tcl_GetIntFromObj(ip, objv[2], &idx) != TCL_OK) return TCL_ERROR;

		if (idx < 1 || idx >= (int)z_info->k_max || !k_info[idx].name) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such object: %d", idx));
			return TCL_ERROR;
		}

		kind = &k_info[idx];
		object_kind_name(name, sizeof(name), kind, kind->aware);

		if (strcmp(what, "info") == 0) {
			Tcl_Obj *d = Tcl_NewListObj(0, NULL);

#define PUT(key, value) \
	do { \
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj((key), -1)); \
		Tcl_ListObjAppendElement(ip, d, (value)); \
	} while (0)

			PUT("index", Tcl_NewIntObj(idx));
			PUT("name", Tcl_NewStringObj(name, -1));
			PUT("kind", Tcl_NewStringObj(tval_find_name(kind->tval) ?
					tval_find_name(kind->tval) : "", -1));
			PUT("aware", Tcl_NewBooleanObj(kind->aware));
			PUT("everseen", Tcl_NewBooleanObj(kind->everseen));
			PUT("flavoured", Tcl_NewBooleanObj(kind->flavor != NULL));
			PUT("level", Tcl_NewIntObj(kind->level));
			PUT("cost", Tcl_NewIntObj(kind->cost));
			PUT("weight", Tcl_NewIntObj(kind->weight));
			kind_glyph_put(ip, d, kind);

#undef PUT

			Tcl_SetObjResult(ip, d);
			return TCL_OK;
		}

		/*
		 * The game's own description of the kind, through object_info with
		 * OINFO_FAKE -- the same path the knowledge screen uses.
		 */
		{
			struct object *known = NULL;
			struct object *obj = object_fake(kind, &known);
			textblock *tb = object_info(obj, OINFO_FAKE);
			Tcl_Obj *text = textblock_to_obj(tb);

			textblock_free(tb);
			object_fake_free(obj, known);

			Tcl_SetObjResult(ip, text);
			return TCL_OK;
		}
	}

	/*
	 * A verb this does know, given the wrong number of arguments, is an arity
	 * complaint and not an unknown verb.  Saying so matters because the
	 * sentence below otherwise comes out as "expected max, list, info or
	 * lore, not: max", which contradicts itself and sends the reader looking
	 * for a spelling mistake that is not there.
	 */
	if (strcmp(what, "max") == 0 || strcmp(what, "list") == 0
			|| strcmp(what, "info") == 0 || strcmp(what, "lore") == 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "max | list ?pattern? | info n | lore n");
		return TCL_ERROR;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected max, list, info or lore, not: %s", what));

	return TCL_ERROR;
}

/**
 * angband_artifact -- the artifacts the character knows exist.
 *
 *    angband_artifact max            how many there are
 *    angband_artifact list ?pattern? {index name kind level} rows
 *    angband_artifact info <index>   one of them, as a name/value dict
 *    angband_artifact lore <index>   what the game says about it
 *
 * The third table of the same shape.  What makes artifacts different is that
 * knowing one exists is itself a spoiler, so the filter is the game's own
 * artifact_is_known() rather than anything reasoned out here: created, and not
 * sitting unidentified in the character's own pack.
 */

/**
 * Build the object the game would describe, for an artifact that may be lost.
 *
 * desc_art_fake()'s recipe from ui-knowledge.c, without its side effects and
 * without its first branch: that function describes the real object when the
 * artifact still exists somewhere, which needs find_artifact and is private to
 * that file.  A knowledge browser wants the artifact as a thing that exists in
 * the world, not as the particular copy in somebody's pack, so the fake is the
 * right answer here rather than a compromise.
 *
 * Both objects are the caller's stack, and both must be wiped.
 */
static bool artifact_fake(const struct artifact *art, struct object *obj,
		struct object *known)
{
	if (!make_fake_artifact(obj, art)) return false;

	obj->known = known;
	known->artifact = obj->artifact;
	known->kind = obj->kind;

	/* If it was fully known before it was lost, say so in full. */
	if (history_is_artifact_known(player, obj->artifact))
		object_copy(known, obj);

	return true;
}

/**
 * An artifact's whole name.
 *
 * a_info stores the suffix -- "of Galadriel", "'Angrist'" -- because the game
 * appends it to whatever it is an artifact *of*: a Phial, a Main Gauche.  On
 * its own it reads as a fragment, and capitalising it produces "Of Galadriel".
 * So the base kind goes in front of it, which is how the game says it too.
 */
static void artifact_full_name(const struct artifact *art, char *buf,
		size_t len)
{
	struct object_kind *kind = lookup_kind(art->tval, art->sval);
	char base[80];

	if (!kind) {
		my_strcpy(buf, art->name, len);
		return;
	}

	/*
	 * object_kind_name, not kind->name.  The raw field carries the game's
	 * templating -- "& Phial~", where & is the article and ~ the plural mark
	 * -- and printing it gives "& Phial~ of Galadriel".  This is the same
	 * call the object list uses, and it hands back what a player would read.
	 */
	object_kind_name(base, sizeof(base), kind, true);
	strnfmt(buf, len, "%s %s", base, art->name);
}

static int objcmd_artifact(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int idx;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "max|list|info|lore ?argument ...?");
		return TCL_ERROR;
	}

	what = Tcl_GetString(objv[1]);

	/*
	 * player as well as the tables: artifact_is_known reads the character's
	 * wizard flag and their pack, and history_is_artifact_known reads their
	 * history.
	 */
	if (!a_info || !z_info || !player) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the artifact list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (strcmp(what, "max") == 0 && objc == 2) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj((int)z_info->a_max));
		return TCL_OK;
	}

	if (strcmp(what, "list") == 0 && (objc == 2 || objc == 3)) {
		const char *pattern = (objc == 3) ? Tcl_GetString(objv[2]) : NULL;
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);
		int i;

		for (i = 0; i < (int)z_info->a_max; i++) {
			struct artifact *art = &a_info[i];
			const char *tval;
			char name[120];
			Tcl_Obj *row;

			/*
			 * The game's own test, cheat option and all: collect_known_artifacts
			 * shows everything when cheat_xtra is on, and a browser that
			 * disagreed with the screen beside it would be the more confusing
			 * of the two.
			 */
			if (!art->name) continue;
			if (!OPT(player, cheat_xtra) && !artifact_is_known(i)) continue;

			tval = tval_find_name(art->tval);
			artifact_full_name(art, name, sizeof(name));

			if (pattern
					&& !Tcl_StringCaseMatch(name, pattern, TCL_MATCH_NOCASE)
					&& !(tval && Tcl_StringCaseMatch(tval, pattern,
							TCL_MATCH_NOCASE)))
				continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(tval ? tval : "", -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(art->level));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if ((strcmp(what, "info") == 0 || strcmp(what, "lore") == 0)
			&& objc == 3) {
		struct artifact *art;

		if (Tcl_GetIntFromObj(ip, objv[2], &idx) != TCL_OK) return TCL_ERROR;

		if (idx < 0 || idx >= (int)z_info->a_max || !a_info[idx].name) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such artifact: %d", idx));
			return TCL_ERROR;
		}

		/*
		 * And it has to be one the character knows about.  An index is a
		 * number anybody can type, and answering for an artifact they have
		 * never found would give away that it exists.
		 */
		if (!OPT(player, cheat_xtra) && !artifact_is_known(idx)) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"artifact %d is not known to this character", idx));
			return TCL_ERROR;
		}

		art = &a_info[idx];

		if (strcmp(what, "info") == 0) {
			Tcl_Obj *d = Tcl_NewListObj(0, NULL);
			const char *tval = tval_find_name(art->tval);

#define PUT(key, value) \
	do { \
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj((key), -1)); \
		Tcl_ListObjAppendElement(ip, d, (value)); \
	} while (0)

			char name[120];

			artifact_full_name(art, name, sizeof(name));

			PUT("index", Tcl_NewIntObj(idx));
			PUT("name", Tcl_NewStringObj(name, -1));
			PUT("kind", Tcl_NewStringObj(tval ? tval : "", -1));
			PUT("level", Tcl_NewIntObj(art->level));
			PUT("cost", Tcl_NewIntObj(art->cost));
			PUT("weight", Tcl_NewIntObj(art->weight));
			PUT("armor_class", Tcl_NewIntObj(art->ac));
			PUT("to_hit", Tcl_NewIntObj(art->to_h));
			PUT("to_dam", Tcl_NewIntObj(art->to_d));
			PUT("to_ac", Tcl_NewIntObj(art->to_a));
			PUT("fully_known",
					Tcl_NewBooleanObj(history_is_artifact_known(player, art)));

			/* An artifact looks like the object it is made from. */
			{
				const struct object_kind *kind =
					lookup_kind(art->tval, art->sval);
				if (kind) kind_glyph_put(ip, d, kind);
			}

#undef PUT

			Tcl_SetObjResult(ip, d);
			return TCL_OK;
		}

		{
			struct object body, known_body;
			Tcl_Obj *text;
			textblock *tb;

			memset(&body, 0, sizeof(body));
			memset(&known_body, 0, sizeof(known_body));

			if (!artifact_fake(art, &body, &known_body)) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"Nothing is recorded about this one.", -1));
				return TCL_OK;
			}

			tb = object_info(&body, OINFO_NONE);
			text = textblock_to_obj(tb);
			textblock_free(tb);

			object_wipe(&known_body);
			object_wipe(&body);

			Tcl_SetObjResult(ip, text);
			return TCL_OK;
		}
	}

	/*
	 * A verb this does know, given the wrong number of arguments, is an arity
	 * complaint and not an unknown verb.  Saying so matters because the
	 * sentence below otherwise comes out as "expected max, list, info or
	 * lore, not: max", which contradicts itself and sends the reader looking
	 * for a spelling mistake that is not there.
	 */
	if (strcmp(what, "max") == 0 || strcmp(what, "list") == 0
			|| strcmp(what, "info") == 0 || strcmp(what, "lore") == 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "max | list ?pattern? | info n | lore n");
		return TCL_ERROR;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected max, list, info or lore, not: %s", what));

	return TCL_ERROR;
}

/**
 * The four smaller knowledge tables: ego items, runes, features and traps.
 *
 * Each takes the same four verbs as angband_monster, and each is a few lines
 * because the verb parsing is shared.  They are separate commands rather than
 * one with a table argument because their fields have nothing in common: an
 * ego item has a cost and a depth range, a rune has neither.
 */
enum table_verb {
	TABLE_BAD, TABLE_MAX, TABLE_LIST, TABLE_INFO, TABLE_LORE
};

/**
 * Which verb, and its argument.
 *
 * Sets `pattern` for list (NULL when none was given) and `idx` for info and
 * lore.  Returns TABLE_BAD having already set the interpreter's result, so a
 * caller can return TCL_ERROR without saying anything else.
 */
static enum table_verb table_verb_parse(Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[], const char **pattern, int *idx)
{
	const char *what;

	*pattern = NULL;
	*idx = -1;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "max|list|info|lore ?argument ...?");
		return TABLE_BAD;
	}

	what = Tcl_GetString(objv[1]);

	if (strcmp(what, "max") == 0 && objc == 2) return TABLE_MAX;

	if (strcmp(what, "list") == 0 && (objc == 2 || objc == 3)) {
		if (objc == 3) *pattern = Tcl_GetString(objv[2]);
		return TABLE_LIST;
	}

	if ((strcmp(what, "info") == 0 || strcmp(what, "lore") == 0)
			&& objc == 3) {
		if (Tcl_GetIntFromObj(ip, objv[2], idx) != TCL_OK) return TABLE_BAD;
		return (strcmp(what, "info") == 0) ? TABLE_INFO : TABLE_LORE;
	}

	/*
	 * A verb this does know, given the wrong number of arguments, is an arity
	 * complaint and not an unknown verb.  Saying so matters because the
	 * sentence below otherwise comes out as "expected max, list, info or
	 * lore, not: max", which contradicts itself and sends the reader looking
	 * for a spelling mistake that is not there.
	 */
	if (strcmp(what, "max") == 0 || strcmp(what, "list") == 0
			|| strcmp(what, "info") == 0 || strcmp(what, "lore") == 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "max | list ?pattern? | info n | lore n");
		return TABLE_BAD;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected max, list, info or lore, not: %s", what));

	return TABLE_BAD;
}

/**
 * Does this name match what was typed?  Nothing typed matches everything.
 */
static bool table_matches(const char *name, const char *pattern)
{
	if (!pattern) return true;
	if (!name) return false;

	return Tcl_StringCaseMatch(name, pattern, TCL_MATCH_NOCASE) != 0;
}

static void table_put(Tcl_Interp *ip, Tcl_Obj *d, const char *key,
		Tcl_Obj *value)
{
	Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(key, -1));
	Tcl_ListObjAppendElement(ip, d, value);
}

/**
 * angband_ego -- the ego item types the character has seen.
 */
static int objcmd_ego(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *pattern;
	enum table_verb verb;
	int idx, i;

	(void)dummy;

	verb = table_verb_parse(ip, objc, objv, &pattern, &idx);
	if (verb == TABLE_BAD) return TCL_ERROR;

	if (!e_info || !z_info || !player) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the ego list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (verb == TABLE_MAX) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj((int)z_info->e_max));
		return TCL_OK;
	}

	if (verb == TABLE_LIST) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < (int)z_info->e_max; i++) {
			struct ego_item *ego = &e_info[i];
			Tcl_Obj *row;

			if (!ego->name) continue;
			if (!ego->everseen && !OPT(player, cheat_xtra)) continue;
			if (!table_matches(ego->name, pattern)) continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(ego->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(ego->alloc_min));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (idx < 0 || idx >= (int)z_info->e_max || !e_info[idx].name) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such ego item: %d", idx));
		return TCL_ERROR;
	}
	if (!e_info[idx].everseen && !OPT(player, cheat_xtra)) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"ego item %d is not known to this character", idx));
		return TCL_ERROR;
	}

	if (verb == TABLE_INFO) {
		struct ego_item *ego = &e_info[idx];
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		table_put(ip, d, "index", Tcl_NewIntObj(idx));
		table_put(ip, d, "name", Tcl_NewStringObj(ego->name, -1));
		table_put(ip, d, "cost", Tcl_NewIntObj(ego->cost));
		table_put(ip, d, "rating", Tcl_NewIntObj(ego->rating));
		table_put(ip, d, "level", Tcl_NewIntObj(ego->alloc_min));
		table_put(ip, d, "deepest", Tcl_NewIntObj(ego->alloc_max));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	{
		textblock *tb = object_info_ego(&e_info[idx]);
		Tcl_Obj *text = textblock_to_obj(tb);

		textblock_free(tb);
		Tcl_SetObjResult(ip, text);
	}

	return TCL_OK;
}

/**
 * angband_rune -- the runes the character has learned.
 *
 * The only one of the four with no table of its own: 4.2 keeps runes behind
 * max_runes(), rune_name() and rune_desc(), and what the character knows
 * behind player_knows_rune().  So this reads exactly like the others and is
 * backed by four function calls rather than an array.
 */
static int objcmd_rune(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *pattern;
	enum table_verb verb;
	int idx, i, n;

	(void)dummy;

	verb = table_verb_parse(ip, objc, objv, &pattern, &idx);
	if (verb == TABLE_BAD) return TCL_ERROR;

	if (!player) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the rune list is not loaded yet", -1));
		return TCL_ERROR;
	}

	n = max_runes();

	if (verb == TABLE_MAX) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj(n));
		return TCL_OK;
	}

	if (verb == TABLE_LIST) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < n; i++) {
			const char *name = rune_name(i);
			Tcl_Obj *row;

			if (!name) continue;
			if (!player_knows_rune(player, i)) continue;
			if (!table_matches(name, pattern)) continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("rune", -1));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (idx < 0 || idx >= n || !rune_name(idx)) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such rune: %d", idx));
		return TCL_ERROR;
	}
	if (!player_knows_rune(player, idx)) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"rune %d is not known to this character", idx));
		return TCL_ERROR;
	}

	if (verb == TABLE_INFO) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		table_put(ip, d, "index", Tcl_NewIntObj(idx));
		table_put(ip, d, "name", Tcl_NewStringObj(rune_name(idx), -1));
		table_put(ip, d, "kind", Tcl_NewStringObj("rune", -1));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_NewStringObj(
			rune_desc(idx) ? rune_desc(idx) : "", -1));

	return TCL_OK;
}

/**
 * angband_feature -- the terrain.
 *
 * No knowledge filter, because the game has none: every feature is listed from
 * the start, mimics excepted -- a secret door that looks like a wall is listed
 * as the wall it pretends to be, and listing it twice would give the game
 * away.
 */
static int objcmd_feature(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *pattern;
	enum table_verb verb;
	int idx, i;

	(void)dummy;

	verb = table_verb_parse(ip, objc, objv, &pattern, &idx);
	if (verb == TABLE_BAD) return TCL_ERROR;

	if (!f_info) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the feature list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (verb == TABLE_MAX) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj(FEAT_MAX));
		return TCL_OK;
	}

	if (verb == TABLE_LIST) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		/*
		 * From one, not zero.  FEAT_NONE is "unknown grid", the marker the
		 * game draws where nothing is known, and it is the only feature of the
		 * forty-one with no description -- measured, along with trap zero,
		 * which is "no trap" and the same idea.
		 */
		for (i = 1; i < FEAT_MAX; i++) {
			struct feature *feat = &f_info[i];
			Tcl_Obj *row;

			if (!feat->name || feat->mimic) continue;
			if (!table_matches(feat->name, pattern)) continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(feat->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("terrain", -1));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (idx < 1 || idx >= FEAT_MAX || !f_info[idx].name) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such feature: %d", idx));
		return TCL_ERROR;
	}

	if (verb == TABLE_INFO) {
		struct feature *feat = &f_info[idx];
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		table_put(ip, d, "index", Tcl_NewIntObj(idx));
		table_put(ip, d, "name", Tcl_NewStringObj(feat->name, -1));
		table_put(ip, d, "kind", Tcl_NewStringObj("terrain", -1));
		table_put(ip, d, "digging", Tcl_NewIntObj(feat->dig));
		table_put(ip, d, "shop", Tcl_NewIntObj(feat->shopnum));
		glyph_put(ip, d, feat->d_char, feat->d_attr);
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_NewStringObj(
			f_info[idx].desc ? f_info[idx].desc : "", -1));

	return TCL_OK;
}

/**
 * angband_trap -- the traps.
 */
static int objcmd_trap(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *pattern;
	enum table_verb verb;
	int idx, i;

	(void)dummy;

	verb = table_verb_parse(ip, objc, objv, &pattern, &idx);
	if (verb == TABLE_BAD) return TCL_ERROR;

	if (!trap_info || !z_info) {
		Tcl_SetObjResult(ip,
				Tcl_NewStringObj("the trap list is not loaded yet", -1));
		return TCL_ERROR;
	}

	if (verb == TABLE_MAX) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj((int)z_info->trap_max));
		return TCL_OK;
	}

	if (verb == TABLE_LIST) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		/* From one: trap zero is "no trap".  See the feature list above. */
		for (i = 1; i < (int)z_info->trap_max; i++) {
			struct trap_kind *trap = &trap_info[i];
			Tcl_Obj *row;

			if (!trap->name) continue;
			if (!table_matches(trap->desc ? trap->desc : trap->name, pattern))
				continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			/* The short description is the readable one: "a pit" rather
			 * than the internal name. */
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
					trap->desc ? trap->desc : trap->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("trap", -1));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (idx < 1 || idx >= (int)z_info->trap_max || !trap_info[idx].name) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such trap: %d", idx));
		return TCL_ERROR;
	}

	if (verb == TABLE_INFO) {
		struct trap_kind *trap = &trap_info[idx];
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		table_put(ip, d, "index", Tcl_NewIntObj(idx));
		table_put(ip, d, "name", Tcl_NewStringObj(
				trap->desc ? trap->desc : trap->name, -1));
		table_put(ip, d, "kind", Tcl_NewStringObj("trap", -1));
		glyph_put(ip, d, trap->d_char, trap->d_attr);
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_NewStringObj(
			trap_info[idx].text ? trap_info[idx].text : "", -1));

	return TCL_OK;
}

/**
 * The character window's other pages: virtues, mutations and the timeline.
 *
 * Three small lists rather than three tables: each is about *this* character,
 * so there is nothing to search and nothing to look up by index.  One verb,
 * `list`, and the caller draws it.
 */

/**
 * angband_virtue list -- the eight a character is measured against.
 *
 *    {index name value description sentence}
 *
 * A character holds eight of the eighteen, chosen at birth from their race,
 * class and realms, so this is their eight and not the whole table.  The
 * sentence is virtue_line()'s, which is what the game itself prints; the parts
 * are there as well because a table wants columns and a paragraph does not.
 */
static int objcmd_virtue(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	Tcl_Obj *list;
	int i;

	(void)dummy;

	if (objc != 2 || strcmp(Tcl_GetString(objv[1]), "list") != 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "list");
		return TCL_ERROR;
	}
	if (!player) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	list = Tcl_NewListObj(0, NULL);

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		int v = player->vir_types[i];
		char line[120];
		Tcl_Obj *row;

		/* An empty slot: a character with fewer than eight. */
		if (!v) continue;

		row = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(v));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewStringObj(virtue_name(v) ? virtue_name(v) : "", -1));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewIntObj(virtue_value(player, v)));
		Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
				virtue_describe(virtue_value(player, v)), -1));
		Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
				virtue_line(player, i, line, sizeof(line)) ? line : "", -1));
		Tcl_ListObjAppendElement(ip, list, row);
	}

	Tcl_SetObjResult(ip, list);

	return TCL_OK;
}

/**
 * angband_mutation list -- what has been done to this character.
 *
 *    {index name description power}
 *
 * The whole mutation list is walked and the ones the character does not have
 * are dropped, rather than reading their flag array directly: player_has_
 * mutation is the game's own test and the flags are a bitfield whose indices
 * are assigned at load.
 */
static int objcmd_mutation(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const struct mutation *mut;
	Tcl_Obj *list;

	(void)dummy;

	if (objc != 2 || strcmp(Tcl_GetString(objv[1]), "list") != 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "list");
		return TCL_ERROR;
	}
	if (!player) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	list = Tcl_NewListObj(0, NULL);

	for (mut = mutations; mut; mut = mut->next) {
		Tcl_Obj *row;

		if (!player_has_mutation(player, mut)) continue;

		row = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj((int)mut->midx));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewStringObj(mut->name ? mut->name : "", -1));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewStringObj(mut->desc ? mut->desc : "", -1));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewStringObj(mut->power ? mut->power : "", -1));
		Tcl_ListObjAppendElement(ip, list, row);
	}

	Tcl_SetObjResult(ip, list);

	return TCL_OK;
}

/**
 * angband_history list -- what has happened to this character.
 *
 *    {turn depth level text}
 *
 * 4.2's history is richer than the original had anywhere to put it: every
 * artifact found, every level gained, every death survived, with the turn and
 * the depth it happened at.  That is the Notes page.
 */
static int objcmd_history(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	struct history_info *entries = NULL;
	Tcl_Obj *list;
	size_t n, i;

	(void)dummy;

	if (objc != 2 || strcmp(Tcl_GetString(objv[1]), "list") != 0) {
		Tcl_WrongNumArgs(ip, 1, objv, "list");
		return TCL_ERROR;
	}
	if (!player) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	n = history_get_list(player, &entries);
	list = Tcl_NewListObj(0, NULL);

	for (i = 0; i < n; i++) {
		Tcl_Obj *row = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewIntObj((int)entries[i].turn));
		Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(entries[i].dlev));
		Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(entries[i].clev));
		Tcl_ListObjAppendElement(ip, row,
				Tcl_NewStringObj(entries[i].event, -1));
		Tcl_ListObjAppendElement(ip, list, row);
	}

	Tcl_SetObjResult(ip, list);

	return TCL_OK;
}

/**
 * angband_gear -- what the character is carrying and wearing.
 *
 *    angband_gear inventory     the pack, a row per item
 *    angband_gear equipment     what is worn, plus a row per empty slot
 *    angband_gear quiver        the quiver, same shape as the pack
 *    angband_gear slots         {used size}: the pack's slots, quiver included
 *    angband_gear info <where> <n>   one item, as a name/value dict
 *    angband_gear lore <where> <n>   what the game says about it
 *    angband_gear recall <where> <n> show it in the Recall pane
 *    angband_gear burden             {carried slow-at capacity}, in tenths
 *    angband_gear fits <where> <n>   the equipment slots it could go in
 *    angband_gear inscribe <where> <n> <text>   note it, or "" to erase
 *    angband_gear wield <where> <n> ?slot?  put it on
 *    angband_gear takeoff <where> <n>   take it off
 *    angband_gear drop <where> <n> ?q?  put it down
 *
 * One command for all three because they are one thing: the gear list, which
 * 4.2 keeps as `struct object *` chains and presents through three views of
 * it.  `where` is inventory, equipment or quiver, and `n` is the position in
 * that view -- not a gear index, which is not stable across a turn.
 *
 * The label is the letter the player types, from gear_to_label(), so a window
 * built on this says the same thing as the game's own prompts.
 */

/**
 * The object at a position in one of the three views, or NULL.
 *
 * Equipment is addressed by slot rather than by occupancy: the empty slots
 * are part of what the view shows, so "the fourth slot" has to keep meaning
 * the fourth slot whether or not anything is in it.
 */
/**
 * The object's symbol, as the game's data file writes it.
 *
 * Not object_char(), which is what the *display* would draw: that goes through
 * x_char, and with a tile set chosen it is a tile index rather than a letter,
 * so a window asking for it gets a box.  The paper doll wants the ASCII
 * register whatever the map is drawn in, and d_char is where that lives.
 */
static wchar_t object_glyph(const struct object *obj)
{
	if (kind_shows_flavour(obj->kind)) return obj->kind->flavor->d_char;
	return obj->kind->d_char;
}

/**
 * And its colour, by the same rule, as the map's "#rrggbb".
 */
static const char *object_colour(const struct object *obj)
{
	uint8_t attr = kind_shows_flavour(obj->kind)
		? obj->kind->flavor->d_attr : obj->kind->d_attr;

	return colour_name[attr % MAX_COLORS];
}

/**
 * An equipment slot type, lower-cased: weapon, bow, ring, body_armor.
 *
 * The table in obj-gear.c is static and keeps only the mention strings, which
 * are sentences ("On your right hand") rather than names.  These come from the
 * same header, so a slot type gains a name here the moment it gains one there.
 */
static const char *slot_type_name(int type)
{
	static const char *names[] = {
		#define EQUIP(a, b, c, d, e, f) #a,
		#include "list-equip-slots.h"
		#undef EQUIP
	};
	static char lower[32];
	size_t i;

	if (type < 0 || type >= (int)N_ELEMENTS(names)) return "";

	my_strcpy(lower, names[type], sizeof(lower));
	for (i = 0; lower[i]; i++) lower[i] = tolower((unsigned char)lower[i]);
	return lower;
}

/**
 * Describe an object in the Recall pane, the way the game's own item menu does
 * as its cursor moves: track it and redraw the subwindows, so that what the
 * pane shows is the game's recall, origin and all, and a monster looked at
 * afterwards takes the pane back the same way.  The original's windows did this
 * for whatever was pointed at or chosen in them -- the Choice and Inventory
 * windows, the store, the equipment icons -- and so do these.
 *
 * Not while the game is part way through drawing something else.  A store's
 * stock can be tracked as safely as the pack: object_delete() stops tracking
 * whatever it deletes, and that is how a store disposes of what it sells out.
 */
static void recall_object(struct object *obj)
{
	if (!obj || command_inside || !player || !player->upkeep) return;
	if (tracked_object_is(player->upkeep, obj)) return;
	track_object(player->upkeep, obj);
	redraw_stuff(player);
}

static struct object *gear_at(const char *where, int n, const char **slotname)
{
	if (slotname) *slotname = NULL;

	if (streq(where, "inventory")) {
		if (n < 0 || n >= player->upkeep->inven_cnt) return NULL;
		return player->upkeep->inven[n];
	}

	if (streq(where, "quiver")) {
		if (n < 0 || n >= (int)z_info->quiver_size) return NULL;
		return player->upkeep->quiver[n];
	}

	if (streq(where, "equipment")) {
		if (n < 0 || n >= (int)player->body.count) return NULL;
		if (slotname) *slotname = equip_mention(player, n);
		return slot_object(player, n);
	}

	return NULL;
}

static int gear_count(const char *where)
{
	if (streq(where, "inventory")) return player->upkeep->inven_cnt;
	if (streq(where, "quiver")) return (int)z_info->quiver_size;
	if (streq(where, "equipment")) return (int)player->body.count;

	return -1;
}

static int objcmd_gear(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *where;
	char name[120];
	int n, i;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv,
				"inventory|equipment|quiver|info|lore ?argument ...?");
		return TCL_ERROR;
	}

	where = Tcl_GetString(objv[1]);

	/*
	 * A character and their upkeep, not in_play(): the pack exists from birth
	 * and reading it needs no level under the character's feet.  in_play()
	 * also wants character_dungeon, which is the guard the *command* table
	 * needs -- its prereqs read the map -- and is too strong here.
	 */
	if (!player || !player->upkeep) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	/* The three list views.  Any other single word falls through. */
	if (objc == 2 && gear_count(where) >= 0) {
		int count = gear_count(where);
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < count; i++) {
			const char *slotname = NULL;
			struct object *obj = gear_at(where, i, &slotname);
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);
			char label[2];

			/*
			 * An empty equipment slot is still a row: the paper doll and the
			 * wear/wield prompt both need to show what is *not* filled.  The
			 * pack and the quiver simply stop.
			 */
			if (!obj && !streq(where, "equipment")) continue;

			label[0] = obj ? gear_to_label(player, obj) : ' ';
			label[1] = 0;

			if (obj) {
				object_desc(name, sizeof(name), obj,
						ODESC_PREFIX | ODESC_FULL, player);
			} else {
				my_strcpy(name, "", sizeof(name));
			}

			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(label[0] == ' ' ? "" : label, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(slotname ? slotname : "", -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewIntObj(obj ? obj->number : 0));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewIntObj(obj ? obj->weight * obj->number : 0));

			/*
			 * The item's own ASCII symbol, which is what the paper doll draws
			 * instead of an icon sheet.  An empty slot has none; the window
			 * fills that in from the slot's type, because "what glyph stands
			 * for a ring slot" is a presentation question and not the game's.
			 */
			{
				char glyph[8];
				Tcl_Size n2 = obj
					? Tcl_UniCharToUtf((int)object_glyph(obj), glyph) : 0;

				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(glyph, n2));
			}

			/*
			 * The slot's type and its body part, as the game names them:
			 * "ring" and "right hand".  Between them they give a window both
			 * the label to print and the answer to "are these two slots the
			 * same kind of slot", without it having to guess from either.
			 */
			if (streq(where, "equipment")) {
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
						slot_type_name(player->body.slots[i].type), -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
						player->body.slots[i].name
							? player->body.slots[i].name : "", -1));
			} else {
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("", -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("", -1));
			}

			/*
			 * The symbol's colour, last so that a caller reading the first
			 * nine fields is unaffected.  Empty for an empty slot, like the
			 * symbol.
			 */
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(obj ? object_colour(obj) : "", -1));

			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	/* info, lore and recall, which take a view and a position. */
	if ((streq(where, "info") || streq(where, "lore")
			|| streq(where, "recall")) && objc == 4) {
		const char *view = Tcl_GetString(objv[2]);
		const char *slotname = NULL;
		struct object *obj;

		if (Tcl_GetIntFromObj(ip, objv[3], &n) != TCL_OK) return TCL_ERROR;

		if (gear_count(view) < 0) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"expected inventory, equipment or quiver, not: %s", view));
			return TCL_ERROR;
		}

		obj = gear_at(view, n, &slotname);
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"nothing at %s %d", view, n));
			return TCL_ERROR;
		}

		if (streq(where, "recall")) {
			recall_object(obj);
			return TCL_OK;
		}

		if (streq(where, "info")) {
			Tcl_Obj *d = Tcl_NewListObj(0, NULL);
			char label[2];

			label[0] = gear_to_label(player, obj);
			label[1] = 0;
			object_desc(name, sizeof(name), obj,
					ODESC_PREFIX | ODESC_FULL, player);

			table_put(ip, d, "index", Tcl_NewIntObj(n));
			table_put(ip, d, "label", Tcl_NewStringObj(label, -1));
			table_put(ip, d, "name", Tcl_NewStringObj(name, -1));
			table_put(ip, d, "slot",
					Tcl_NewStringObj(slotname ? slotname : "", -1));
			table_put(ip, d, "kind", Tcl_NewStringObj(
					(obj->tval && tval_find_name(obj->tval))
						? tval_find_name(obj->tval) : "", -1));
			table_put(ip, d, "number", Tcl_NewIntObj(obj->number));
			table_put(ip, d, "weight", Tcl_NewIntObj(obj->weight));
			table_put(ip, d, "value", Tcl_NewIntObj(object_value(obj, 1)));
			table_put(ip, d, "known",
					Tcl_NewBooleanObj(object_fully_known(obj)));
			table_put(ip, d, "inscription", Tcl_NewStringObj(
					obj->note ? quark_str(obj->note) : "", -1));

			Tcl_SetObjResult(ip, d);
			return TCL_OK;
		}

		/*
		 * The game's own description of this very object, not of its kind:
		 * object_info reads the runes the character has learned, so a sword
		 * whose brand is still unknown does not list the brand.
		 */
		{
			textblock *tb = object_info(obj, OINFO_NONE);
			Tcl_Obj *text = textblock_to_obj(tb);

			textblock_free(tb);
			Tcl_SetObjResult(ip, text);
			return TCL_OK;
		}
	}

	/*
	 * How full the pack is, as the game counts it: pack_slots_used() includes
	 * the slots the quiver takes, which is why it is not the length of the
	 * inventory list.
	 */
	if (streq(where, "slots") && objc == 2) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(ip, d,
				Tcl_NewIntObj(pack_slots_used(player)));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(z_info->pack_size));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	/*
	 * The three numbers the footer shows, in tenths of a pound, which is how
	 * the game stores every weight: carried, the weight at which speed starts
	 * to drop, and the most that can be carried at all.  Formatting is the
	 * window's business.
	 */
	if (streq(where, "burden") && objc == 2) {
		int cap = weight_limit(&player->state);
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(ip, d,
				Tcl_NewIntObj(player->upkeep->total_weight));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(cap / 2));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(cap));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	/*
	 * Which equipment slots this item could go in, as slot numbers.
	 *
	 * The game's answer, not a type string compared in a script: wield_slot()
	 * is what the wear command itself uses, and asking it means a window's
	 * idea of "this fits here" cannot drift from the game's.  A ring names
	 * both hands, because both are the same kind of slot.
	 */
	if (streq(where, "fits") && objc == 4) {
		const char *view = Tcl_GetString(objv[2]);
		struct object *obj;
		Tcl_Obj *list;
		int base;

		if (Tcl_GetIntFromObj(ip, objv[3], &n) != TCL_OK) return TCL_ERROR;

		obj = gear_at(view, n, NULL);
		list = Tcl_NewListObj(0, NULL);
		base = obj ? wield_slot(obj) : -1;

		if (base >= 0) {
			for (i = 0; i < (int)player->body.count; i++) {
				if (slot_type_is(player, i, player->body.slots[base].type))
					Tcl_ListObjAppendElement(ip, list, Tcl_NewIntObj(i));
			}
		}

		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	/*
	 * The inscription, which is a note and not a command: it changes nothing
	 * about the object and costs no turn.  An empty string removes it, which
	 * is a different command rather than an inscription of nothing.
	 */
	if (streq(where, "inscribe") && objc == 5) {
		const char *view = Tcl_GetString(objv[2]);
		const char *note;
		struct object *obj;

		if (Tcl_GetIntFromObj(ip, objv[3], &n) != TCL_OK) return TCL_ERROR;

		obj = gear_at(view, n, NULL);
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("nothing at %s %d", view, n));
			return TCL_ERROR;
		}

		if (!inkey_flag || command_inside) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}

		note = Tcl_GetString(objv[4]);
		if (*note) {
			cmdq_push(CMD_INSCRIBE);
			cmd_set_arg_item(cmdq_peek(), "item", obj);
			cmd_set_arg_string(cmdq_peek(), "inscription", note);
		} else {
			cmdq_push(CMD_UNINSCRIBE);
			cmd_set_arg_item(cmdq_peek(), "item", obj);
		}

		command_nudge();

		return TCL_OK;
	}

	/*
	 * Acting on an item, by view and position.
	 *
	 * These exist because angband_push cannot take one: a command argument of
	 * type item wants a struct object *, and a script has no way to name one.
	 * Resolving it here keeps object pointers on this side of the seam, where
	 * they are safe from a script holding one across a turn.
	 *
	 * The game is still the one that acts.  Each of these pushes the command
	 * the keyboard would have pushed, so the turn cost, the curse checks and
	 * the messages are all the game's.
	 */
	if ((streq(where, "wield") || streq(where, "takeoff")
				|| streq(where, "drop"))
			&& (objc == 4 || objc == 5)) {
		const char *view = Tcl_GetString(objv[2]);
		struct object *obj;
		int quantity = 1;

		if (Tcl_GetIntFromObj(ip, objv[3], &n) != TCL_OK) return TCL_ERROR;
		if (objc == 5 && Tcl_GetIntFromObj(ip, objv[4], &quantity) != TCL_OK)
			return TCL_ERROR;

		if (gear_count(view) < 0) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"expected inventory, equipment or quiver, not: %s", view));
			return TCL_ERROR;
		}

		obj = gear_at(view, n, NULL);
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("nothing at %s %d", view, n));
			return TCL_ERROR;
		}

		if (!inkey_flag || command_inside) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}

		if (streq(where, "wield")) {
			cmdq_push(CMD_WIELD);
			cmd_set_arg_item(cmdq_peek(), "item", obj);

			/*
			 * A target slot, when the caller named one and something is
			 * already in it.  do_cmd_wield() works out the slot itself from
			 * wield_slot(), and only asks which one to displace when the
			 * chosen slot is occupied and the item is a ring -- so this is
			 * the one place a drop can say "that hand, not the other one".
			 * Dropping a ring on an empty hand while the other hand is also
			 * empty goes wherever the game prefers; there is no argument for
			 * saying otherwise, and the ring is worn either way.
			 */
			if (objc == 5) {
				struct object *held;
				int slot = quantity;

				if (slot >= 0 && slot < (int)player->body.count
						&& (held = slot_object(player, slot)) != NULL)
					cmd_set_arg_item(cmdq_peek(), "replace", held);
			}
		} else if (streq(where, "takeoff")) {
			cmdq_push(CMD_TAKEOFF);
			cmd_set_arg_item(cmdq_peek(), "item", obj);
		} else {
			cmdq_push(CMD_DROP);
			cmd_set_arg_item(cmdq_peek(), "item", obj);
			cmd_set_arg_number(cmdq_peek(), "quantity",
					MIN(quantity, obj->number));
		}

		command_nudge();

		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected inventory, equipment, quiver, slots, burden, info,"
			" lore, recall, fits, inscribe, wield, takeoff or drop, not: %s",
			where));

	return TCL_ERROR;
}

/**
 * The Choice pane: the item prompt, answerable with the mouse.
 *
 * When the game asks for an item -- "Quaff which potion?" -- the pane lists
 * what it will accept, and clicking one answers the question.  The game's own
 * prompt is not replaced but wrapped: it runs exactly as it always did, so the
 * letters, '/' for the equipment, '-' for the floor and @q1-style inscriptions
 * all keep working from the keyboard.
 *
 * Before the prompt opens, the wrapper records what it offers, through
 * scan_items() with the prompt's own tester and mode, which is how the game
 * itself gathers items from the pack, the body, the quiver and the floor -- so
 * the pane cannot offer something the prompt would refuse.  A click records the
 * object and presses Escape; when the game's prompt then comes back
 * empty-handed, the wrapper answers with the recorded object instead.  So no
 * letter is ever synthesised for the item, and a click does not depend on
 * which of the prompt's lists happens to be on screen.
 */
static bool (*choice_original)(struct object **choice, const char *pmt,
		const char *str, cmd_code cmd, item_tester tester, int mode);
/*
 * What the pane is offering, if anything: the game's item prompt, its spell
 * prompt, one of its dynamic menus -- the racial, class and mutation powers,
 * pet orders, the town's services and the rest -- or a yes-or-no question.
 * One at a time; a prompt that opens inside another is the game's alone.
 */
enum choice_kind {
	CHOICE_NONE, CHOICE_ITEM, CHOICE_SPELL, CHOICE_MENU, CHOICE_CHECK
};
static const char *choice_kind_name[] = {
	"", "item", "spell", "menu", "check"
};
static enum choice_kind choice_kind;
static int choice_count;
static char choice_prompt[160];

/* An item prompt's offer, and what was clicked. */
static struct object **choice_offered;
static struct object *choice_picked;

/* A spell prompt's. */
static int (*spell_original)(struct player *p, const char *verb,
		struct object *book, const char *error,
		bool (*spell_filter)(const struct player *p, int spell));
static int *choice_spells;
static bool (*choice_spell_ok)(const struct player *p, int spell);
static int choice_spell_picked;

/* A dynamic menu's. */
static struct menu *choice_menu;
static int choice_menu_picked;
static bool choice_menu_has_pick;

/* A yes-or-no question's: -1 until a click answers it, then 1 or 0. */
static bool (*check_original)(const char *prompt);
static int choice_check_picked = -1;

/* Tell the pane to look again, queued like every other event. */
static void choice_signal(void)
{
	if (Tcl_Eval(interp, "event generate . <<Angband_CHOOSE>> -when tail")
			!= TCL_OK)
		Tcl_BackgroundException(interp, TCL_ERROR);
}

static bool choice_get_item(struct object **choice, const char *pmt,
		const char *str, cmd_code cmd, item_tester tester, int mode)
{
	int max = z_info->pack_size + z_info->quiver_size
		+ player->body.count + z_info->floor_size;
	bool ok;

	/* Paranoia: a prompt inside a prompt is the game's alone. */
	if (choice_kind != CHOICE_NONE)
		return choice_original(choice, pmt, str, cmd, tester, mode);

	choice_offered = mem_zalloc(max * sizeof(*choice_offered));
	choice_count = scan_items(choice_offered, max, player, mode, tester);
	my_strcpy(choice_prompt, pmt ? pmt : "", sizeof(choice_prompt));
	choice_picked = NULL;
	choice_kind = CHOICE_ITEM;
	choice_signal();

	ok = choice_original(choice, pmt, str, cmd, tester, mode);

	if (!ok && choice_picked) {
		*choice = choice_picked;
		ok = true;
	}

	choice_kind = CHOICE_NONE;
	choice_picked = NULL;
	choice_count = 0;
	mem_free(choice_offered);
	choice_offered = NULL;
	choice_signal();

	return ok;
}

/**
 * The spell prompt, wrapped the same way: the book's spells are recorded
 * before the game's own spell menu opens, and a click answers with one of
 * them.  The game asks "which spell" through get_spell_from_book_hook --
 * including from inside textui_get_spell(), now that ui-spell.c asks through
 * the hook rather than calling past it.
 */
static int choice_get_spell(struct player *p, const char *verb,
		struct object *book, const char *error,
		bool (*spell_filter)(const struct player *p, int spell))
{
	const struct class_book *cb = player_object_to_book(p, book);
	int spell;

	if (choice_kind != CHOICE_NONE || !cb)
		return spell_original(p, verb, book, error, spell_filter);

	choice_count = spell_collect_from_book(p, book, &choice_spells);
	choice_spell_ok = spell_filter;
	choice_spell_picked = -1;
	strnfmt(choice_prompt, sizeof(choice_prompt), "%s which %s?", verb,
			cb->realm->spell_noun);
	my_strcap(choice_prompt);
	choice_kind = CHOICE_SPELL;
	choice_signal();

	spell = spell_original(p, verb, book, error, spell_filter);
	if (spell < 0 && choice_spell_picked >= 0) spell = choice_spell_picked;

	choice_kind = CHOICE_NONE;
	choice_count = 0;
	mem_free(choice_spells);
	choice_spells = NULL;
	choice_spell_ok = NULL;
	choice_signal();

	return spell;
}

/*
 * The top line of the main screen.  A dynamic menu has no title; whoever opens
 * one prints what it is about there first -- "You are a Human Mindcrafter,
 * and have 5 of 10 spell points." -- and the pane uses that.
 */
static void screen_top_line(char *buf, size_t len)
{
	term *old = Term;
	Tcl_DString ds;
	int x, a;
	wchar_t c;
	size_t n;

	Tcl_DStringInit(&ds);
	Term_activate(term_screen);
	for (x = 0; x < Term->wid; x++) {
		char u[TCL_UTF_MAX + 1];

		if (Term_what(x, 0, &a, &c)) break;
		Tcl_DStringAppend(&ds, u, Tcl_UniCharToUtf((int)c, u));
	}
	Term_activate(old);

	my_strcpy(buf, Tcl_DStringValue(&ds), len);
	Tcl_DStringFree(&ds);
	for (n = strlen(buf); n > 0 && buf[n - 1] == ' '; n--) buf[n - 1] = 0;
}

/**
 * Every dynamic menu, wrapped through the hook ui-menu.c keeps for it: the
 * entries are read off the menu itself, the menu runs as it always did, and a
 * click answers with an entry's value.  That one wrapper covers the racial,
 * class and mutation powers -- the Mindcrafter's psionics among them -- pet
 * orders, the town's buildings and the game's other menus of the same kind.
 */
static int choice_menu_select(struct menu *m)
{
	int value;

	if (choice_kind != CHOICE_NONE) return menu_dynamic_select_default(m);

	choice_menu = m;
	choice_count = menu_dynamic_count(m);
	choice_menu_has_pick = false;
	screen_top_line(choice_prompt, sizeof(choice_prompt));
	if (!choice_prompt[0])
		my_strcpy(choice_prompt, "Choose one.", sizeof(choice_prompt));
	choice_kind = CHOICE_MENU;
	choice_signal();

	value = menu_dynamic_select_default(m);
	if (value == -1 && choice_menu_has_pick) value = choice_menu_picked;

	choice_kind = CHOICE_NONE;
	choice_count = 0;
	choice_menu = NULL;
	choice_menu_has_pick = false;
	choice_signal();

	return value;
}

/**
 * The game's yes-or-no question, wrapped the same way: the question is
 * recorded, the game's own prompt runs and still takes 'y' from the keyboard,
 * and a click on Yes or No answers it.  The town's buildings ask this way --
 * "A bed until morning for 50 au?" -- and so do the people in them who hand
 * out work, and so does much of the rest of the game.
 *
 * Pending messages are flushed first, as the game's prompt does before it
 * shows the question.  Done here as well so that the flush happens before the
 * pane offers Yes and No: otherwise the -more- that ends a run of messages
 * would still be waiting for its key while the question was on offer, and the
 * click meant for the question would go to the -more-.
 */
static bool choice_get_check(const char *prompt)
{
	size_t n;
	bool yes;

	if (choice_kind != CHOICE_NONE) return check_original(prompt);

	event_signal(EVENT_MESSAGE_FLUSH);

	my_strcpy(choice_prompt, prompt ? prompt : "", sizeof(choice_prompt));
	for (n = strlen(choice_prompt); n > 0 && choice_prompt[n - 1] == ' '; n--)
		choice_prompt[n - 1] = 0;
	choice_count = 2;
	choice_check_picked = -1;
	choice_kind = CHOICE_CHECK;
	choice_signal();

	yes = check_original(prompt);
	if (!yes && choice_check_picked >= 0) yes = (choice_check_picked == 1);

	choice_kind = CHOICE_NONE;
	choice_count = 0;
	choice_check_picked = -1;
	choice_signal();

	return yes;
}

/**
 * The service building being visited, or -1 -- for the Building window, which
 * shows a building while it is asking something and closes when the visit is
 * over.
 *
 * A visit is the whole of EVENT_USE_SERVICE's handler, ui_enter_service():
 * every menu and question the building puts is asked from inside it.
 * player-util.c signals EVENT_ENTER_SERVICE just before it and
 * EVENT_LEAVE_SERVICE just after, as a store has three, and this follows
 * those two.
 */
static int service_visit = -1;

static void service_notice(game_event_type type, game_event_data *data,
		void *user)
{
	(void)data;
	(void)user;

	service_visit = (type == EVENT_ENTER_SERVICE && player && cave)
		? wild_service_at(cave, player->grid) : -1;
}

/* Once, after textui_input_init(), so what is wrapped is textui's prompt. */
/* The game's prompts, wrapped -- once per set the game puts in. */
static void choice_hooks_install(void)
{
	choice_original = get_item_hook;
	get_item_hook = choice_get_item;

	spell_original = get_spell_from_book_hook;
	get_spell_from_book_hook = choice_get_spell;

	menu_dynamic_select_hook = choice_menu_select;

	check_original = get_check_hook;
	get_check_hook = choice_get_check;
}

/**
 * The game's ? -- the help -- opens the Help window, which reads the manual,
 * as the original's did.  Unlike the views below it asks nothing of the
 * game: the window opens beside the map and the command is over.
 */
static void help_view_tk(void)
{
	if (Tcl_Eval(interp, "help_window") != TCL_OK) {
		plog_fmt("Tcl/Tk: the Help window: %s", Tcl_GetStringResult(interp));
	}
}

/*
 * The front end's own views of the game's screens: character creation, the
 * choice of savefile, the death screen and the help.  Nothing of the game's is wrapped,
 * so they go in at the start rather than at the first wait -- with a key
 * typed ahead of the splash screen, the choice of savefile came before any
 * wait, found no view and drew the text menu.  Nothing the game resets
 * touches them.
 */
static void views_install(void)
{
	birth_view_hook = birth_view_tk;
	select_savefile_hook = chooser_view_tk;
	death_view_hook = death_view_tk;
	help_view_hook = help_view_tk;
}

/* What this file follows itself -- again whenever a reset sweeps them. */
static void choice_events_register(void)
{
	event_add_handler(EVENT_BIRTHPOINTS, birth_points_notice, NULL);
	event_add_handler(EVENT_LEAVE_WORLD, store_notice, NULL);
	event_add_handler(EVENT_LEAVE_WORLD, last_sight_notice, NULL);
	event_add_handler(EVENT_ENTER_WORLD, store_notice, NULL);
	event_add_handler(EVENT_ENTER_SERVICE, service_notice, NULL);
	event_add_handler(EVENT_LEAVE_SERVICE, service_notice, NULL);
}

static void choice_install(void)
{
	if (choice_original || !get_item_hook) return;
	choice_hooks_install();
	choice_events_register();

	if (reinit_hook != bridge_reinit) {
		bridge_reinit_prev = reinit_hook;
		reinit_hook = bridge_reinit;
	}
}

/**
 * Starting again after New Game.
 *
 * The death screen's New Game does not leave the process: play_game() calls
 * cleanup_angband(), which takes every handler off every event -- this front
 * end's along with the game's -- and then textui_init(), which sets every
 * subwindow back to the game's own assignment by index.  So the second
 * character played in a session had a front end that heard nothing: the Birth
 * window was not told the character was made and stayed open, and the
 * Progress strip went on showing the last character.  (The prompt hooks
 * survive: textui_init() puts the game's own back only on the first start.)
 *
 * The game calls reinit_hook just before textui_init(), so this wraps it to
 * note that a reinstall is due, and the reinstall happens at the next point
 * the game waits -- a request for a key, or the Birth window's question --
 * which is after textui_init().  The event handlers and the subwindows are put
 * back; the prompt wrappers only if the game did replace them, since wrapping
 * a wrapper would have it call itself.
 */
static void bridge_reinit(void)
{
	if (bridge_reinit_prev) bridge_reinit_prev();
	bridge_reinstall_due = true;
}

static void bridge_reinstall_if_due(void)
{
	int i;

	if (!bridge_reinstall_due) return;
	bridge_reinstall_due = false;

	events_register();
	choice_events_register();
	subwindows_set_flags(want_flag, ANGBAND_TERM_MAX);

	if (get_item_hook != choice_get_item) {
		/* A prompt a script took over is taken over again from the new one. */
		for (i = 0; i < HOOK_MAX; i++) {
			if (hooks[i].script) {
				hooks[i].original = NULL;
				hook_install(&hooks[i]);
			}
		}
		choice_hooks_install();
	}

	store_inside = false;
	service_visit = -1;
}

/**
 * A spell's state as the game's own spell menu shows it, spell_menu_display()
 * in ui-spell.c: a word or the spell's information, and the colour for it.
 * `state` is the word whatever the comment is, so a window can sort and colour
 * by it.
 */
static const char *spell_state(int sidx, char *help, size_t len,
		const char **state, int *attr)
{
	const struct class_spell *spell = spell_by_index(player, sidx);
	const char *comment = "";

	help[0] = 0;
	*state = "";
	*attr = COLOUR_WHITE;
	if (!spell) return "";

	if (spell->slevel >= 99) {
		comment = *state = "illegible";
		*attr = COLOUR_L_DARK;
	} else if (player->spell_flags[sidx] & PY_SPELL_FORGOTTEN) {
		comment = *state = "forgotten";
		*attr = COLOUR_YELLOW;
	} else if (player->spell_flags[sidx] & PY_SPELL_LEARNED) {
		if (player->spell_flags[sidx] & PY_SPELL_WORKED) {
			get_spell_info(sidx, help, len);
			comment = help + (help[0] == ' ');
			*state = "known";
			*attr = COLOUR_WHITE;
		} else {
			comment = *state = "untried";
			*attr = COLOUR_L_GREEN;
		}
	} else if (spell->slevel <= player->lev) {
		comment = *state = "unknown";
		*attr = COLOUR_L_BLUE;
	} else {
		comment = *state = "difficult";
		*attr = COLOUR_RED;
	}
	return comment;
}

/**
 * A spell as the pane lists it, in the state and colour the game's own spell
 * menu gives it (spell_menu_display() in ui-spell.c):
 *    {n label name level mana fail comment colour valid}
 */
static Tcl_Obj *choice_spell_row(Tcl_Interp *ip, int n, int sidx)
{
	const struct class_spell *spell = spell_by_index(player, sidx);
	Tcl_Obj *row = Tcl_NewListObj(0, NULL);
	char help[30] = "";
	char label[2];
	const char *comment, *state;
	int attr;

	if (!spell) return row;

	comment = spell_state(sidx, help, sizeof(help), &state, &attr);
	(void)state;

	label[0] = (n < (int) strlen(all_letters_nohjkl))
		? all_letters_nohjkl[n] : 0;
	label[1] = 0;

	Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(n));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(label, -1));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(spell->name, -1));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(spell->slevel));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(spell->smana));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(spell_chance(sidx)));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(comment, -1));
	Tcl_ListObjAppendElement(ip, row,
			Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(
			choice_spell_ok ? choice_spell_ok(player, sidx) : true));
	return row;
}

/**
 * One row of the pane:
 *    {n view label name weight glyph glyph-colour name-colour}
 *
 * The name's colour is the one the game's own item list gives it, its kind's
 * base colour, with an unreadable book in slate (show_obj() in ui-object.c);
 * the symbol's is the map's.
 */
static Tcl_Obj *choice_row(Tcl_Interp *ip, int n, struct object *obj)
{
	Tcl_Obj *row = Tcl_NewListObj(0, NULL);
	const char *view;
	char name[120];
	char label[2];
	char glyph[8];
	Tcl_Size glen;
	uint8_t attr = obj->kind->base->attr;

	if (object_is_equipped(player->body, obj)) view = "equipment";
	else if (object_is_in_quiver(player, obj)) view = "quiver";
	else if (object_is_carried(player, obj)) view = "inventory";
	else view = "floor";

	label[0] = streq(view, "floor") ? '-' : gear_to_label(player, obj);
	label[1] = 0;
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	glen = Tcl_UniCharToUtf((int)object_glyph(obj), glyph);
	if (tval_is_book_k(obj->kind) && !player_object_to_book(player, obj))
		attr = COLOUR_SLATE;

	Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(n));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(view, -1));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(label, -1));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
	Tcl_ListObjAppendElement(ip, row,
			Tcl_NewIntObj(obj->weight * obj->number));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(glyph, glen));
	Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(object_colour(obj), -1));
	Tcl_ListObjAppendElement(ip, row,
			Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1));
	return row;
}

/**
 * What can be done with an item, as the game's own item menu decides it.
 *
 * context_menu_object() in ui-context.c, test for test and in its order, so
 * the pane's menu offers what the game's would and greys what the game's
 * greys: a wand with no charges, a scroll with no light to read it by.  The
 * two entries that are not commands get ids of their own.
 */
#define ACTION_INSPECT  (-1)
#define ACTION_DROP_ALL (-2)

struct item_action {
	int id;				/* a cmd_code, or one of the two above */
	const char *label;
	bool valid;
};

static int item_actions(struct object *obj, struct item_action *out, int max)
{
	int n = 0;
	bool equipped = object_is_equipped(player->body, obj);

#define ACT(i, l, v) \
	do { \
		if (n < max) { \
			out[n].id = (i); \
			out[n].label = (l); \
			out[n].valid = (v); \
			n++; \
		} \
	} while (0)

	ACT(ACTION_INSPECT, "Inspect", true);

	if (obj_can_browse(obj)) {
		if (obj_can_cast_from(obj) && player_can_cast(player, false))
			ACT(CMD_CAST, "Cast", true);
		if (obj_can_study(obj) && player_can_study(player, false))
			ACT(CMD_STUDY, "Study", true);
		if (player_can_read(player, false))
			ACT(CMD_BROWSE_SPELL, "Browse", true);
	} else if (obj_is_useable(obj)) {
		if (tval_is_wand(obj))
			ACT(CMD_USE_WAND, "Aim", obj_has_charges(obj));
		else if (tval_is_rod(obj))
			ACT(CMD_USE_ROD, "Zap", obj_can_zap(obj));
		else if (tval_is_staff(obj))
			ACT(CMD_USE_STAFF, "Use", obj_has_charges(obj));
		else if (tval_is_scroll(obj))
			ACT(CMD_READ_SCROLL, "Read", player_can_read(player, false));
		else if (tval_is_potion(obj))
			ACT(CMD_QUAFF, "Quaff", true);
		else if (tval_is_edible(obj))
			ACT(CMD_EAT, "Eat", true);
		else if (obj_is_activatable(obj))
			ACT(CMD_ACTIVATE, "Activate",
					equipped && obj_can_activate(obj));
		else if (obj_can_fire(obj))
			ACT(CMD_FIRE, "Fire", true);
		else
			ACT(CMD_USE, "Use", true);
	}

	if (obj_can_refill(obj))
		ACT(CMD_REFILL, "Refill", true);

	if (equipped && obj_can_takeoff(obj))
		ACT(CMD_TAKEOFF, "Take off", true);
	else if (!equipped && obj_can_wear(obj))
		ACT(CMD_WIELD, "Equip", true);

	if (object_is_carried(player, obj)) {
		if (!square_isshop(cave, player->grid)
				|| square(cave, player->grid)->feat == FEAT_HOME) {
			ACT(CMD_DROP, "Drop", true);
			if (obj->number > 1)
				ACT(ACTION_DROP_ALL, "Drop All", true);
		} else if (store_will_buy_tester(obj)) {
			ACT(CMD_DROP, "Sell", true);
		}
	} else {
		ACT(CMD_PICKUP, "Pick up", inven_carry_okay(obj));
	}

	if (obj_can_throw(obj))
		ACT(CMD_THROW, "Throw", true);

	ACT(CMD_INSCRIBE, "Inscribe", true);
	if (obj_has_inscrip(obj))
		ACT(CMD_UNINSCRIBE, "Uninscribe", true);

	ACT(CMD_IGNORE, object_is_ignored(obj) ? "Unignore" : "Ignore", true);

#undef ACT
	return n;
}

/* The key the player would press for it, as the menu bar shows keys. */
static Tcl_Obj *action_key(int id)
{
	int mode = OPT(player, rogue_like_commands)
		? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
	unsigned char key;
	char buf[3] = "";

	if (id == ACTION_INSPECT) key = 'I';
	else if (id == ACTION_DROP_ALL) return Tcl_NewStringObj("", -1);
	else key = cmd_lookup_key((cmd_code)id, mode);

	if (key && key < 0x20) {
		buf[0] = '^';
		buf[1] = (char)UN_KTRL_CAP(key);
	} else if (key) {
		buf[0] = (char)key;
	}
	return Tcl_NewStringObj(buf, -1);
}

/**
 * Do one of them, as context_menu_object() does once something is chosen:
 * the inscription checks first (!d, !q and the rest, which can ask "Really
 * ...?"), then the command with the item set, or for the two that are menus
 * of the game's own, that menu.
 */
static bool item_act(struct object *obj, int id)
{
	int mode = OPT(player, rogue_like_commands)
		? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
	unsigned char cmdkey = cmd_lookup_key((cmd_code)id, mode);
	bool pushed = false;

	if (id == ACTION_DROP_ALL) {
		cmdq_push(CMD_DROP);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", obj->number);
		return true;
	}

	if (!key_confirm_command(cmdkey)
			|| !get_item_allow(obj, cmdkey, (cmd_code)id, false))
		return false;

	if (id == CMD_IGNORE) {
		textui_cmd_ignore_menu(obj);
	} else if (id == CMD_BROWSE_SPELL) {
		textui_book_browse(obj);
	} else if (id == CMD_CAST) {
		cmdq_push(CMD_CAST);
		cmd_set_arg_item(cmdq_peek(), "book", obj);
		pushed = true;
	} else {
		cmdq_push((cmd_code)id);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		if (id == CMD_DROP && square_isshop(cave, player->grid)) {
			struct command *gc = cmdq_peek();
			gc->code = (square(cave, player->grid)->feat == FEAT_HOME)
				? CMD_STASH : CMD_SELL;
		}
		pushed = true;
	}

	return pushed;
}

/**
 * angband_choice -- what the Choice pane shows, and what it can do.
 *
 *    angband_choice           {open prompt kind}: whether the game is
 *                             asking, and whether for an item, a spell, from
 *                             a menu, or yes or no
 *    angband_choice list      the rows: what the prompt offers while it is
 *                             open, the pack otherwise.  An item row is
 *                             {n view label name weight glyph glyph-colour
 *                             name-colour}, a spell row {n label name level
 *                             mana fail comment colour valid}, a menu row
 *                             {n label text valid}, and a question's two
 *                             rows are menu rows, Yes and then No
 *    angband_choice burden    the header the game's own inventory window has
 *    angband_choice pick <n>  answer the open prompt with row n
 *    angband_choice recall <n>        show item row n in the Recall pane
 *    angband_choice actions <n>       what can be done with pack item n, as
 *                                     {id label key valid}
 *    angband_choice act <n> <id>      do it
 */
static int objcmd_choice(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i, n;

	(void)dummy;

	if (!player || !player->upkeep) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	if (objc == 1) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(ip, d,
				Tcl_NewBooleanObj(choice_kind != CHOICE_NONE));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(
				choice_kind != CHOICE_NONE ? choice_prompt : "", -1));
		Tcl_ListObjAppendElement(ip, d,
				Tcl_NewStringObj(choice_kind_name[choice_kind], -1));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	what = Tcl_GetString(objv[1]);

	if (streq(what, "list") && objc == 2) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		if (choice_kind == CHOICE_ITEM) {
			for (i = 0; i < choice_count; i++)
				Tcl_ListObjAppendElement(ip, list,
						choice_row(ip, i, choice_offered[i]));
		} else if (choice_kind == CHOICE_SPELL) {
			for (i = 0; i < choice_count; i++)
				Tcl_ListObjAppendElement(ip, list,
						choice_spell_row(ip, i, choice_spells[i]));
		} else if (choice_kind == CHOICE_MENU) {
			for (i = 0; i < choice_count; i++) {
				Tcl_Obj *row = Tcl_NewListObj(0, NULL);
				const char *text;
				int value;
				bool valid;
				char label[2] = "";

				text = menu_dynamic_entry(choice_menu, i, &value, &valid,
						&label[0]);
				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(label, -1));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(text ? text : "", -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(valid));
				Tcl_ListObjAppendElement(ip, list, row);
			}
		} else if (choice_kind == CHOICE_CHECK) {
			static const char *answer[2][2] = { { "y", "Yes" }, { "n", "No" } };

			for (i = 0; i < 2; i++) {
				Tcl_Obj *row = Tcl_NewListObj(0, NULL);

				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(answer[i][0], -1));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(answer[i][1], -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(true));
				Tcl_ListObjAppendElement(ip, list, row);
			}
		} else {
			for (i = 0; i < z_info->pack_size; i++) {
				struct object *obj = player->upkeep->inven[i];

				if (!obj) break;
				Tcl_ListObjAppendElement(ip, list, choice_row(ip, i, obj));
			}
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	/*
	 * Built as show_obj_list() builds it, weight_remaining() and all.  That
	 * counts down to 60% of the weight limit while speed starts to drop at
	 * 50%, which is the game's inconsistency and not one to fix in a header
	 * that is meant to say what the game says.
	 */
	if (streq(what, "burden") && objc == 2) {
		int diff = weight_remaining(player);

		Tcl_SetObjResult(ip, Tcl_ObjPrintf("Burden %d.%d lb (%d.%d lb %s)",
				player->upkeep->total_weight / 10,
				player->upkeep->total_weight % 10,
				abs(diff) / 10, abs(diff) % 10,
				(diff < 0 ? "overweight" : "remaining")));
		return TCL_OK;
	}

	if (streq(what, "pick") && objc == 3) {
		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		if (choice_kind == CHOICE_NONE) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is not asking for anything", -1));
			return TCL_ERROR;
		}
		if (n < 0 || n >= choice_count) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such choice: %d", n));
			return TCL_ERROR;
		}

		/* The game's own answer to "may this one be chosen", asked again. */
		if (choice_kind == CHOICE_SPELL) {
			if (choice_spell_ok && !choice_spell_ok(player, choice_spells[n])) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"that cannot be chosen", -1));
				return TCL_ERROR;
			}
			choice_spell_picked = choice_spells[n];
			bridge_key(ESCAPE);
			return TCL_OK;
		}
		if (choice_kind == CHOICE_MENU) {
			bool valid;
			int value;

			if (!menu_dynamic_entry(choice_menu, n, &value, &valid, NULL)
					|| !valid) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"that cannot be chosen", -1));
				return TCL_ERROR;
			}
			choice_menu_picked = value;
			choice_menu_has_pick = true;
			bridge_key(ESCAPE);
			return TCL_OK;
		}
		if (choice_kind == CHOICE_CHECK) {
			choice_check_picked = (n == 0);
			bridge_key(ESCAPE);
			return TCL_OK;
		}

		/*
		 * Escape ends the game's prompt as a cancel, and the wrapper turns
		 * the cancel into this answer.
		 */
		choice_picked = choice_offered[n];
		bridge_key(ESCAPE);
		return TCL_OK;
	}

	/* The Recall pane follows the pointer: see recall_object(). */
	if (streq(what, "recall") && objc == 3) {
		struct object *obj = NULL;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		if (choice_kind == CHOICE_ITEM) {
			if (n >= 0 && n < choice_count) obj = choice_offered[n];
		} else if (choice_kind == CHOICE_NONE) {
			if (n >= 0 && n < z_info->pack_size)
				obj = player->upkeep->inven[n];
		}
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no item at row %d", n));
			return TCL_ERROR;
		}
		recall_object(obj);
		return TCL_OK;
	}

	/* The item menu, between prompts, for an item in the pack. */
	if ((streq(what, "actions") && objc == 3)
			|| (streq(what, "act") && objc == 4)) {
		struct item_action acts[24];
		struct object *obj;
		int count, id = 0, k;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		if (objc == 4 && Tcl_GetIntFromObj(ip, objv[3], &id) != TCL_OK)
			return TCL_ERROR;

		obj = (n >= 0 && n < z_info->pack_size)
			? player->upkeep->inven[n] : NULL;
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("nothing at inventory %d", n));
			return TCL_ERROR;
		}
		count = item_actions(obj, acts, (int)N_ELEMENTS(acts));

		if (objc == 3) {
			Tcl_Obj *list = Tcl_NewListObj(0, NULL);

			for (k = 0; k < count; k++) {
				Tcl_Obj *row = Tcl_NewListObj(0, NULL);

				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(acts[k].id));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(acts[k].label, -1));
				Tcl_ListObjAppendElement(ip, row, action_key(acts[k].id));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewBooleanObj(acts[k].valid));
				Tcl_ListObjAppendElement(ip, list, row);
			}
			Tcl_SetObjResult(ip, list);
			return TCL_OK;
		}

		/*
		 * Asked again rather than trusted: the menu was built a moment ago
		 * and the game has had a turn since if anything else happened.
		 */
		for (k = 0; k < count; k++)
			if (acts[k].id == id) break;
		if (k == count || !acts[k].valid || id == ACTION_INSPECT) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"that cannot be done with it just now", -1));
			return TCL_ERROR;
		}
		if (!command_may_start()) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}

		/*
		 * Inside a command for as long as this runs: the inscription checks
		 * and the ignore and browse menus can all prompt, and a prompt pumps
		 * Tk, so the door is held shut as it is for a menu's hook.
		 */
		{
			COMMAND_INSIDE_HELD;
			if (item_act(obj, id)) command_nudge();
		}
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected list, burden, pick <n>, actions <n> or act <n> <id>,"
			" not: %s", what));
	return TCL_ERROR;
}

/**
 * angband_book -- the Spell Book window's view of the character's magic.
 *
 *    angband_book books           {n name realm} for each book in the pack the
 *                                 character can read, n its pack position
 *    angband_book spells <n>      the spells of book n, as {sidx name level
 *                                 mana fail info state castable learnable}
 *    angband_book text <sidx>     the spell's own description
 *    angband_book chooses         whether this class picks the spell it
 *                                 learns, or learns one at random from a book
 *    angband_book cast <sidx>     cast it
 *    angband_book study <n> ?sidx?  learn from book n: that spell for a class
 *                                 that chooses, a random one otherwise
 *
 * Read from the game's own tables, and acted on by pushing the game's own
 * commands with their arguments filled in -- do_cmd_cast() takes a "spell",
 * do_cmd_study() a "spell" or an "item" -- so the turn, the "not enough mana,
 * attempt it anyway?" question and the aiming are the game's, exactly as from
 * the keyboard.  `state` is the word from spell_menu_display(): known,
 * untried, unknown, difficult, forgotten or illegible.
 */
static int objcmd_book(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i, n, sidx;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "books|spells|text|chooses|cast|study ?arg ...?");
		return TCL_ERROR;
	}
	if (!player || !player->upkeep || !player->class) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}
	what = Tcl_GetString(objv[1]);

	if (streq(what, "books") && objc == 2) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);
		char name[120];

		for (i = 0; i < z_info->pack_size; i++) {
			struct object *obj = player->upkeep->inven[i];
			const struct class_book *cb;
			Tcl_Obj *row;

			if (!obj) break;
			if (!obj_can_browse(obj)) continue;
			cb = player_object_to_book(player, obj);
			if (!cb) continue;

			object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL,
					player);
			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
					cb->realm && cb->realm->name ? cb->realm->name : "", -1));
			Tcl_ListObjAppendElement(ip, list, row);
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (streq(what, "chooses") && objc == 2) {
		Tcl_SetObjResult(ip,
				Tcl_NewBooleanObj(player_has(player, PF_CHOOSE_SPELLS)));
		return TCL_OK;
	}

	if ((streq(what, "spells") && objc == 3)
			|| (streq(what, "study") && (objc == 3 || objc == 4))) {
		struct object *obj;
		const struct class_book *cb;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		obj = (n >= 0 && n < z_info->pack_size)
			? player->upkeep->inven[n] : NULL;
		cb = obj && obj_can_browse(obj)
			? player_object_to_book(player, obj) : NULL;
		if (!cb) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no book at inventory %d", n));
			return TCL_ERROR;
		}

		if (streq(what, "spells")) {
			Tcl_Obj *list = Tcl_NewListObj(0, NULL);

			for (i = 0; i < cb->num_spells; i++) {
				const struct class_spell *spell = &cb->spells[i];
				Tcl_Obj *row = Tcl_NewListObj(0, NULL);
				char help[30];
				const char *info, *state;
				int attr;

				sidx = spell->sidx;
				info = spell_state(sidx, help, sizeof(help), &state, &attr);

				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(sidx));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(spell->name, -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(spell->slevel));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(spell->smana));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewIntObj(spell_chance(sidx)));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(streq(state, "known") ? info : "", -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(state, -1));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewBooleanObj(spell_okay_to_cast(player, sidx)));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewBooleanObj(spell_okay_to_study(player, sidx)));
				Tcl_ListObjAppendElement(ip, list, row);
			}
			Tcl_SetObjResult(ip, list);
			return TCL_OK;
		}

		/* study */
		if (!command_may_start()) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}
		/*
		 * Whether the character can study at all just now -- blind, no
		 * light, nothing left to learn -- is the game's to say, and it says
		 * why as the command starts; only the spell itself is checked here.
		 */
		if (player_has(player, PF_CHOOSE_SPELLS)) {
			if (objc != 4 || Tcl_GetIntFromObj(ip, objv[3], &sidx) != TCL_OK
					|| !spell_okay_to_study(player, sidx)) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"that cannot be learned just now", -1));
				return TCL_ERROR;
			}
			cmdq_push(CMD_STUDY);
			cmd_set_arg_choice(cmdq_peek(), "spell", sidx);
		} else {
			cmdq_push(CMD_STUDY);
			cmd_set_arg_item(cmdq_peek(), "item", obj);
		}
		command_nudge();
		return command_done(ip);
	}

	if ((streq(what, "text") || streq(what, "cast")) && objc == 3) {
		const struct class_spell *spell;

		if (Tcl_GetIntFromObj(ip, objv[2], &sidx) != TCL_OK) return TCL_ERROR;
		spell = spell_by_index(player, sidx);
		if (!spell) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such spell: %d", sidx));
			return TCL_ERROR;
		}

		if (streq(what, "text")) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(spell->text ? spell->text : "",
					-1));
			return TCL_OK;
		}

		if (!command_may_start()) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}
		/* As for study: the game says why it cannot be cast just now. */
		if (!spell_okay_to_cast(player, sidx)) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"that cannot be cast just now", -1));
			return TCL_ERROR;
		}
		cmdq_push(CMD_CAST);
		cmd_set_arg_choice(cmdq_peek(), "spell", sidx);
		command_nudge();
		return command_done(ip);
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected books, spells, text, chooses, cast or study, not: %s",
			what));
	return TCL_ERROR;
}

/**
 * Whether the character is inside a store, as the Store window needs to know.
 *
 * Not from the store's own three events.  cmd-cave.c signals ENTER_STORE,
 * USE_STORE and LEAVE_STORE and then removes every handler of each type, so a
 * subscription to them -- including the front end's own, which turns every
 * event into a Tk one -- lasts exactly one visit.  enter_store() and
 * leave_store() in ui-store.c signal LEAVE_WORLD and ENTER_WORLD, which are
 * never removed, so those are what this follows.
 */
static bool store_inside;

static void store_notice(game_event_type type, game_event_data *data,
		void *user)
{
	(void)data;
	(void)user;

	if (type == EVENT_LEAVE_WORLD)
		store_inside = player && cave && store_at(cave, player->grid);
	else
		store_inside = false;
}

/* What the store would buy or take: the game's own sell prompt's list. */
static int store_wares(struct store *store, struct object **list, int max)
{
	return scan_items(list, max, player,
			USE_EQUIP | USE_INVEN | USE_FLOOR | USE_QUIVER,
			store->feat == FEAT_HOME ? NULL : store_will_buy_tester);
}

/* Row n of the shelves, or of the wares, as angband_store lists them. */
static struct object *store_row(struct store *store, bool stock, int n)
{
	int max = stock ? z_info->store_inven_max
		: z_info->pack_size + z_info->quiver_size + player->body.count
			+ z_info->floor_size;
	struct object **list = mem_zalloc(max * sizeof(*list));
	struct object *obj;
	int count;

	if (stock) {
		store_stock_list(store, list, max);
		count = max;
	} else {
		count = store_wares(store, list, max);
	}
	obj = (n >= 0 && n < count) ? list[n] : NULL;
	mem_free(list);
	return obj;
}

/**
 * angband_store -- the store the character is in, for the Store window.
 *
 *    angband_store             {} outside a store, otherwise {name owner
 *                              purse home gold}
 *    angband_store stock       {n name number weight price glyph colour
 *                              afford}, price for one, 0 at home
 *    angband_store wares       what the store would buy -- or the home take --
 *                              as {n name number weight price glyph colour
 *                              worn}, price offered for one
 *    angband_store lore <n>    what the game says about stock item n
 *    angband_store recall stock|wares <n>  show row n in the Recall pane
 *    angband_store buy <n> <quantity>     buy, or take from the home
 *    angband_store sell <n> <quantity>    sell, or leave in the home
 *
 * The lists are the store screen's: store_stock_list() for the shelves, the
 * sell prompt's own items and filter for the wares, price_item() for both
 * prices.  Buying and selling push CMD_BUY, CMD_RETRIEVE, CMD_SELL or
 * CMD_STASH with the item and quantity filled in, which is what the store
 * screen pushes once it has asked, and then run the queue as the store screen
 * does after a key -- cmdq_pop(CTX_STORE) and the notice and handle that
 * follow.  The usual nudge does not do it here: the store's menu passes its
 * handler only the keys it knows, so a key it does not know never reaches the
 * line that runs the queue.
 */
static int objcmd_store(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	struct store *store;
	const char *what;
	struct object **list;
	char name[120];
	int i, n, count, max, qty;
	bool home, more;
	term *old;

	(void)dummy;

	if (!player || !player->upkeep) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}
	store = (store_inside && cave) ? store_at(cave, player->grid) : NULL;

	if (objc == 1) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		if (store) {
			Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(
					f_info[store->feat].name ? f_info[store->feat].name : "",
					-1));
			Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(
					store->owner && store->owner->name
						? store->owner->name : "", -1));
			Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(
					store->owner ? store->owner->max_cost : 0));
			Tcl_ListObjAppendElement(ip, d,
					Tcl_NewBooleanObj(store->feat == FEAT_HOME));
			Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj((int)player->au));
		}
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	if (!store) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("you are not in a store", -1));
		return TCL_ERROR;
	}
	what = Tcl_GetString(objv[1]);
	home = (store->feat == FEAT_HOME);

	if ((streq(what, "stock") || streq(what, "wares")) && objc == 2) {
		Tcl_Obj *rows = Tcl_NewListObj(0, NULL);
		bool stock = streq(what, "stock");

		max = stock ? z_info->store_inven_max
			: z_info->pack_size + z_info->quiver_size + player->body.count
				+ z_info->floor_size;
		list = mem_zalloc(max * sizeof(*list));
		if (stock) {
			store_stock_list(store, list, max);
			count = max;
		} else {
			count = store_wares(store, list, max);
		}

		for (i = 0; i < count; i++) {
			struct object *obj = list[i];
			Tcl_Obj *row;
			char glyph[8];
			int price;

			if (!obj) continue;
			object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL
					| (stock ? ODESC_STORE : 0), player);
			price = home ? 0 : price_item(store, obj, !stock, 1);

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(obj->number));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(obj->weight));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(price));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(glyph,
					Tcl_UniCharToUtf((int)object_glyph(obj), glyph)));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(object_colour(obj), -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(stock
					? (home || player->au >= price)
					: object_is_equipped(player->body, obj)));
			Tcl_ListObjAppendElement(ip, rows, row);
		}
		mem_free(list);
		Tcl_SetObjResult(ip, rows);
		return TCL_OK;
	}

	/* As the original's store did for whatever was chosen in it. */
	if (streq(what, "recall") && objc == 4) {
		const char *side = Tcl_GetString(objv[2]);
		struct object *obj;

		if (!streq(side, "stock") && !streq(side, "wares")) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"expected stock or wares, not: %s", side));
			return TCL_ERROR;
		}
		if (Tcl_GetIntFromObj(ip, objv[3], &n) != TCL_OK) return TCL_ERROR;
		obj = store_row(store, streq(side, "stock"), n);
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("nothing at %d", n));
			return TCL_ERROR;
		}
		recall_object(obj);
		return TCL_OK;
	}

	if ((streq(what, "lore") && objc == 3)
			|| ((streq(what, "buy") || streq(what, "sell")) && objc == 4)) {
		bool stock = !streq(what, "sell");
		struct object *obj;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		if (objc == 4 && Tcl_GetIntFromObj(ip, objv[3], &qty) != TCL_OK)
			return TCL_ERROR;

		obj = store_row(store, stock, n);
		if (!obj) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("nothing at %d", n));
			return TCL_ERROR;
		}

		if (streq(what, "lore")) {
			textblock *tb = object_info(obj, OINFO_NONE);
			Tcl_Obj *text = textblock_to_obj(tb);

			textblock_free(tb);
			Tcl_SetObjResult(ip, text);
			return TCL_OK;
		}

		if (qty < 1 || qty > obj->number) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"there are %d to choose from", obj->number));
			return TCL_ERROR;
		}
		if (stock && !home && player->au < price_item(store, obj, false, qty)) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"You do not have enough gold for this item.", -1));
			return TCL_ERROR;
		}
		if (stock && qty > inven_carry_num(player, obj)) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"You cannot carry that many items.", -1));
			return TCL_ERROR;
		}

		/*
		 * The store screen is the one waiting, not the main loop, so
		 * inkey_flag is not the test; command_inside still is.
		 */
		if (command_inside) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}

		if (stock)
			cmdq_push(home ? CMD_RETRIEVE : CMD_BUY);
		else
			cmdq_push(home ? CMD_STASH : CMD_SELL);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", qty);

		/*
		 * As the store screen does: store_purchase() and store_sell() clear
		 * the message line before a trade, and their prompts write over
		 * what was on it, so a trade's message does not stop at -more-
		 * behind the last one or land on the last one's leftovers.  Then
		 * as store_menu_handle() does once it has pushed a command.
		 *
		 * With auto_more on for the trade itself.  A trade says two or three
		 * things -- "Agreed!", what was bought for how much, what is now in
		 * the pack -- and when they do not fit on the line together the
		 * game stops at -more- for a key.  Here that wait would be inside
		 * this command, with the Store window in front taking the keys, so
		 * the game would sit at -more- until the player thought to click
		 * the main window and press one.  Every message still goes to the
		 * message log and the Messages pane.
		 *
		 * And the main screen brought up to date at the end, which the
		 * store screen's wait for its next key would otherwise leave until
		 * that key: the last of the trade's messages on the top line, and
		 * the shelves as they are now.
		 */
		old = Term;
		Term_activate(term_screen);
		msg_flag = false;
		prt("", 0, 0);
		more = OPT(player, auto_more);
		OPT(player, auto_more) = true;
		{
			COMMAND_INSIDE_HELD;
			cmdq_pop(CTX_STORE);
			event_signal(EVENT_INVENTORY);
			event_signal(EVENT_EQUIPMENT);
			notice_stuff(player);
			handle_stuff(player);
		}
		OPT(player, auto_more) = more;
		Term_fresh();
		Term_activate(old);
		return command_done(ip);
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected stock, wares, lore, recall, buy or sell, not: %s", what));
	return TCL_ERROR;
}

/**
 * angband_service -- the town's service building being visited, for the
 * Building window.
 *
 *    angband_service    {} outside one, and inside {service name town gold}:
 *                       the building's number (enum wild_service), what the
 *                       game calls it -- "healer", "magetower" -- the town it
 *                       stands in, and the character's gold
 *
 * What the building is asking is angband_choice's, as the Choice pane shows
 * it, and is answered the same way; this says only whose question it is.
 */
static int objcmd_service(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	Tcl_Obj *d;
	const char *town = "";

	(void)dummy;

	if (!player || !player->upkeep) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}
	if (objc != 1) {
		Tcl_WrongNumArgs(ip, 1, objv, "");
		return TCL_ERROR;
	}
	if (service_visit < 0) return TCL_OK;

	if (wild && player->in_wild) {
		int t = wild_town_here(wild, loc(player->grid.x + player->wild_offset.x,
				player->grid.y + player->wild_offset.y));

		if (t >= 0 && wild->towns[t].name) town = wild->towns[t].name;
	}

	d = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(service_visit));
	Tcl_ListObjAppendElement(ip, d,
			Tcl_NewStringObj(wild_service_name(service_visit), -1));
	Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(town, -1));
	Tcl_ListObjAppendElement(ip, d, Tcl_NewWideIntObj(player->au));
	Tcl_SetObjResult(ip, d);
	return TCL_OK;
}

/**
 * Character creation in a window of its own (T8).
 *
 * ui-birth.c keeps the stages -- quick start, race, class, realms, how the
 * stats are made, the points or the roller, name, history, confirmation --
 * their order, stepping back, and what each pushes.  It asks its view for
 * each stage's input through birth_view_hook, and this is the view: it tells
 * the Birth window which stage is asking and runs Tk until the window has
 * answered, which is what inkey() would have been doing while it waited for a
 * key.  The answer is in the stage's own terms (struct birth_answer), so
 * whatever the window does, the stage does the same as it would for the key.
 */
static enum birth_stage birth_stage_now;
static int birth_slot_now;
static bool birth_asking;
static struct birth_answer *birth_pending;

/* The point costs, as the game last reported them (EVENT_BIRTHPOINTS). */
static int birth_spent[STAT_MAX], birth_cost[STAT_MAX], birth_left;

static void birth_points_notice(game_event_type type, game_event_data *data,
		void *user)
{
	int i;

	(void)type;
	(void)user;

	for (i = 0; i < STAT_MAX; i++) {
		birth_spent[i] = data->birthpoints.points[i];
		birth_cost[i] = data->birthpoints.inc_points[i];
	}
	birth_left = data->birthpoints.remaining;
}

static void birth_view_tk(enum birth_stage stage, int slot,
		struct birth_answer *answer)
{
	/* After New Game, the first thing the game waits for is this. */
	bridge_first_wait();
	bridge_reinstall_if_due();

	birth_stage_now = stage;
	birth_slot_now = slot;
	birth_pending = answer;
	birth_asking = true;

	/* The screen behind the window says what the text one would. */
	Term_fresh();

	if (Tcl_Eval(interp, "event generate . <<Angband_BIRTHVIEW>> -when tail")
			!= TCL_OK)
		Tcl_BackgroundException(interp, TCL_ERROR);

	while (birth_asking)
		Tcl_DoOneEvent(TCL_ALL_EVENTS);

	birth_pending = NULL;
}

static const char *birth_stage_word(enum birth_stage stage)
{
	switch (stage) {
		case BIRTH_QUICKSTART:		return "quickstart";
		case BIRTH_RACE_CHOICE:		return "race";
		case BIRTH_CLASS_CHOICE:	return "class";
		case BIRTH_REALM_CHOICE:	return "realm";
		case BIRTH_ROLLER_CHOICE:	return "method";
		case BIRTH_POINTBASED:		return "points";
		case BIRTH_ROLLER:			return "roller";
		case BIRTH_NAME_CHOICE:		return "name";
		case BIRTH_HISTORY_CHOICE:	return "history";
		case BIRTH_FINAL_CONFIRM:	return "confirm";
		default:					return "";
	}
}

/* What the describing functions write, gathered rather than drawn. */
static Tcl_DString birth_text;

static void birth_text_out(uint8_t attr, const char *str)
{
	(void)attr;
	Tcl_DStringAppend(&birth_text, str, -1);
}

/* A stat's value as the game prints it: "18/50", trimmed. */
static Tcl_Obj *birth_stat_obj(int value)
{
	char buf[32];
	char *p = buf;

	cnv_stat(value, buf, sizeof(buf));
	while (*p == ' ') p++;
	return Tcl_NewStringObj(p, -1);
}

/**
 * angband_birth -- character creation, for the Birth window.
 *
 *    angband_birth             {} unless the game is asking, and then
 *                              {stage slot}: quickstart, race, class, realm
 *                              (slot 0 or 1), method, points, roller, name,
 *                              history or confirm
 *    angband_birth question    what a menu stage asks, as the screen puts it
 *    angband_birth choices     {cursor rows}: a menu stage's choices in its
 *                              own order, and the one it starts on
 *    angband_birth describe <n>      a race or a class, in the screen's
 *                              words, or a realm and this class's books in it
 *    angband_birth prose race|class|realm <name>
 *                              one described in words, from its data file's
 *                              description, or "" where there is none
 *    angband_birth names race|class|realm   every one's name, in the game's
 *                              order
 *    angband_birth stats       {name self race class best}, a row a stat
 *    angband_birth points      {left rows}: what is left to spend, and a
 *                              {spent cost can-buy can-sell} row a stat
 *    angband_birth character   the character so far, as a name/value list
 *    angband_birth random_name a name, from the game's own generator
 *    angband_birth answer <verb> ?arg?
 *                              answer the stage: choose <row>, back, random,
 *                              finish, options, quit, use, redo, change,
 *                              reroll, previous, buy <stat>, sell <stat>,
 *                              reset, accept, startover, name <text>,
 *                              history <text>
 */
static int objcmd_birth(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i, n;

	(void)dummy;

	if (objc == 1) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		if (birth_asking) {
			Tcl_ListObjAppendElement(ip, d,
					Tcl_NewStringObj(birth_stage_word(birth_stage_now), -1));
			Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(birth_slot_now));
		}
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	what = Tcl_GetString(objv[1]);

	/* Everything else is about the character being made. */
	if (!player || !player->race || !player->class) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	if (streq(what, "random_name") && objc == 2) {
		char name[PLAYER_NAME_LEN];

		player_random_name(name, sizeof(name));
		Tcl_SetObjResult(ip, Tcl_NewStringObj(name, -1));
		return TCL_OK;
	}

	if (streq(what, "stats") && objc == 2) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < STAT_MAX; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);
			char name[16];

			my_strcpy(name, stat_names[i], sizeof(name));
			for (n = (int)strlen(name); n > 0
					&& (name[n - 1] == ' ' || name[n - 1] == ':'); n--)
				name[n - 1] = 0;
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(name, -1));
			Tcl_ListObjAppendElement(ip, row,
					birth_stat_obj(player->stat_max[i]));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewIntObj(player->race->r_adj[i]));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewIntObj(player->class->c_adj[i]));
			Tcl_ListObjAppendElement(ip, row,
					birth_stat_obj(player->state.stat_top[i]));
			Tcl_ListObjAppendElement(ip, list, row);
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (streq(what, "points") && objc == 2) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);
		Tcl_Obj *rows = Tcl_NewListObj(0, NULL);

		for (i = 0; i < STAT_MAX; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(birth_spent[i]));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(birth_cost[i]));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(birth_cost[i] <= birth_left));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(birth_spent[i] > 0));
			Tcl_ListObjAppendElement(ip, rows, row);
		}
		Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(birth_left));
		Tcl_ListObjAppendElement(ip, d, rows);
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	if (streq(what, "character") && objc == 2) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);
		Tcl_Obj *realms = Tcl_NewListObj(0, NULL);

#define BIRTH_PUT(key, obj) do { \
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(key, -1)); \
		Tcl_ListObjAppendElement(ip, d, obj); \
	} while (0)
		BIRTH_PUT("name", Tcl_NewStringObj(player->full_name, -1));
		BIRTH_PUT("race", Tcl_NewStringObj(player->race->name, -1));
		BIRTH_PUT("class", Tcl_NewStringObj(player->class->name, -1));
		for (i = 0; i < REALM_CHOICES; i++) {
			if (player->realm[i])
				Tcl_ListObjAppendElement(ip, realms,
						Tcl_NewStringObj(player->realm[i]->name, -1));
		}
		BIRTH_PUT("realms", realms);
		BIRTH_PUT("patron", Tcl_NewStringObj(
				player->patron ? player->patron->name : "", -1));
		BIRTH_PUT("patron_title", Tcl_NewStringObj(
				(player->patron && player->patron->title)
					? player->patron->title : "", -1));
		BIRTH_PUT("patron_text", Tcl_NewStringObj(
				(player->patron && player->patron->text)
					? player->patron->text : "", -1));
		BIRTH_PUT("history", Tcl_NewStringObj(
				player->history ? player->history : "", -1));
		BIRTH_PUT("hitpoints", Tcl_NewIntObj(player->mhp));
		BIRTH_PUT("mana", Tcl_NewIntObj(player->msp));
#undef BIRTH_PUT
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	/*
	 * A race, a class or a realm in words, which the text screen has no room
	 * for and the original's Birth window had: the description in its data
	 * file, paragraphs and all.  By name, since a realm's row in the window
	 * is its place among those this class is offered, not in the game.
	 */
	if ((streq(what, "prose") && objc == 4)
			|| (streq(what, "names") && objc == 3)) {
		const char *kind = Tcl_GetString(objv[2]);
		const char *want = (objc == 4) ? Tcl_GetString(objv[3]) : NULL;
		Tcl_Obj *names = Tcl_NewListObj(0, NULL);
		const char *desc = NULL;
		bool found = false;

		if (streq(kind, "race")) {
			struct player_race *r;

			for (r = races; r; r = r->next) {
				Tcl_ListObjAppendElement(ip, names,
						Tcl_NewStringObj(r->name, -1));
				if (want && streq(r->name, want)) {
					found = true;
					desc = r->desc;
				}
			}
		} else if (streq(kind, "class")) {
			struct player_class *c;

			for (c = classes; c; c = c->next) {
				Tcl_ListObjAppendElement(ip, names,
						Tcl_NewStringObj(c->name, -1));
				if (want && streq(c->name, want)) {
					found = true;
					desc = c->desc;
				}
			}
		} else if (streq(kind, "realm")) {
			struct magic_realm *m;

			for (m = realms; m; m = m->next) {
				Tcl_ListObjAppendElement(ip, names,
						Tcl_NewStringObj(m->name, -1));
				if (want && streq(m->name, want)) {
					found = true;
					desc = m->desc;
				}
			}
		} else {
			Tcl_DecrRefCount(names);
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"expected race, class or realm, not: %s", kind));
			return TCL_ERROR;
		}

		if (!want) {
			Tcl_SetObjResult(ip, names);
			return TCL_OK;
		}
		Tcl_DecrRefCount(names);
		if (!found) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no %s called %s", kind, want));
			return TCL_ERROR;
		}
		Tcl_SetObjResult(ip, Tcl_NewStringObj(desc ? desc : "", -1));
		return TCL_OK;
	}

	/* The rest are about the stage being asked. */
	if (!birth_asking) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				"the game is not asking anything about the character", -1));
		return TCL_ERROR;
	}

	if (streq(what, "question") && objc == 2) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				birth_question(birth_stage_now), -1));
		return TCL_OK;
	}

	if (streq(what, "choices") && objc == 2) {
		const char *names[64];
		int cursor;
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);
		Tcl_Obj *rows = Tcl_NewListObj(0, NULL);

		n = birth_choices(birth_stage_now, names, (int)N_ELEMENTS(names),
				&cursor);
		for (i = 0; i < n; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(names[i] ? names[i] : "", -1));
			Tcl_ListObjAppendElement(ip, rows, row);
		}
		Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(cursor));
		Tcl_ListObjAppendElement(ip, d, rows);
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	/*
	 * A realm has no description of its own -- the text screen offers the
	 * names alone -- so it is described from what the game knows of it for
	 * this character: what its magic is called and which stat it draws on,
	 * and the class's books in it, spell by spell, with the level each is
	 * learned at.
	 */
	if (streq(what, "describe") && objc == 3
			&& birth_stage_now == BIRTH_REALM_CHOICE) {
		const struct magic_realm *offered[REALM_MAX];
		const struct magic_realm *realm;
		const struct class_magic *magic = &player->class->magic;
		Tcl_Obj *text = Tcl_NewObj();
		char stat[16];
		int b, k;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		k = player_realm_offer(player, birth_slot_now, offered, REALM_MAX);
		if (n < 0 || n >= k) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no realm at row %d", n));
			return TCL_ERROR;
		}
		realm = offered[n];

		my_strcpy(stat, stat_names[realm->stat], sizeof(stat));
		for (k = (int)strlen(stat); k > 0
				&& (stat[k - 1] == ' ' || stat[k - 1] == ':'); k--)
			stat[k - 1] = 0;
		Tcl_AppendPrintfToObj(text, "You %s %ss from %ss, and study them"
				" with %s.\n", realm->verb, realm->spell_noun,
				realm->book_noun, stat);

		for (b = 0; b < magic->num_books; b++) {
			const struct class_book *book = &magic->books[b];
			struct object_kind *kind = lookup_kind(book->tval, book->sval);
			char name[80];

			if (book->realm != realm || !kind) continue;
			object_kind_name(name, sizeof(name), kind, true);
			Tcl_AppendPrintfToObj(text, "\n%s%s\n", name,
					book->dungeon ? " (found in the dungeon)" : "");
			for (k = 0; k < book->num_spells; k++)
				Tcl_AppendPrintfToObj(text, "  %2d  %s\n",
						book->spells[k].slevel, book->spells[k].name);
		}
		Tcl_SetObjResult(ip, text);
		return TCL_OK;
	}

	if (streq(what, "describe") && objc == 3) {
		void (*old)(uint8_t, const char *) = text_out_hook;
		int indent = text_out_indent;
		Tcl_Obj *text;

		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;

		Tcl_DStringInit(&birth_text);
		text_out_hook = birth_text_out;
		text_out_indent = 0;
		birth_describe(birth_stage_now, n);
		text_out_hook = old;
		text_out_indent = indent;

		text = Tcl_NewStringObj(Tcl_DStringValue(&birth_text), -1);
		Tcl_DStringFree(&birth_text);
		Tcl_SetObjResult(ip, text);
		return TCL_OK;
	}

	if (streq(what, "answer") && (objc == 3 || objc == 4)) {
		static const struct {
			const char *verb;
			int key;
			int arg;		/* 0 none, 1 a row or stat, 2 text */
		} verbs[] = {
			{ "choose",		0,				1 },
			{ "back",		ESCAPE,			0 },
			{ "random",		'*',			0 },
			{ "finish",		'@',			0 },
			{ "options",	'=',			0 },
			{ "quit",		KTRL('X'),		0 },
			{ "use",		'y',			0 },
			{ "redo",		'n',			0 },
			{ "change",		'c',			0 },
			{ "reroll",		'r',			0 },
			{ "previous",	'p',			0 },
			{ "buy",		'+',			1 },
			{ "sell",		'-',			1 },
			{ "reset",		'r',			0 },
			{ "accept",		KC_ENTER,		0 },
			{ "startover",	'S',			0 },
			{ "name",		KC_ENTER,		2 },
			{ "history",	'n',			2 },
		};
		const char *verb = Tcl_GetString(objv[2]);
		struct birth_answer *a = birth_pending;
		size_t v;

		for (v = 0; v < N_ELEMENTS(verbs); v++)
			if (streq(verbs[v].verb, verb)) break;
		if (v == N_ELEMENTS(verbs)) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such answer: %s", verb));
			return TCL_ERROR;
		}
		if ((verbs[v].arg != 0) != (objc == 4)) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(verbs[v].arg
					? "%s wants one more argument"
					: "%s takes no argument", verb));
			return TCL_ERROR;
		}
		if (verbs[v].arg == 1
				&& Tcl_GetIntFromObj(ip, objv[3], &a->choice) != TCL_OK)
			return TCL_ERROR;
		if (verbs[v].arg == 2)
			my_strcpy(a->text, Tcl_GetString(objv[3]), sizeof(a->text));

		a->key.type = EVT_KBRD;
		a->key.code = (keycode_t)verbs[v].key;
		a->key.mods = 0;

		/* The quick start takes its "use as it is" as 'y', not Enter. */
		if (streq(verb, "accept") && birth_stage_now == BIRTH_QUICKSTART)
			a->key.code = 'y';

		birth_asking = false;
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected question, choices, describe,"
			" prose, names, stats, points, character, random_name or answer,"
			" not: %s",
			what));
	return TCL_ERROR;
}

/**
 * Choosing a character (T9).
 *
 * play_game() asks which savefile to play when it is started with -c -- as the
 * bundle now is -- and again after New Game.  select_savefile() in ui-game.c
 * gathers the choices, a new character and every savefile in the save
 * directory, and asks its view through select_savefile_hook; this is the view.
 * It tells the Choose Game window the game is asking and runs Tk until the
 * window has answered, as the Birth window's view does.  Which savefile is the
 * default, what a new character is saved as and what happens to one that will
 * not load stay in ui-game.c.
 */
static bool chooser_asking;
static int chooser_answer;
static const char **chooser_names;
static int chooser_count, chooser_cursor;
static bool chooser_retry;

static int chooser_view_tk(const char **entries, const char **names,
		int count, int cursor, bool retry)
{
	(void)entries;

	/* At the start, and after New Game, the first thing the game waits for. */
	bridge_first_wait();
	bridge_reinstall_if_due();

	chooser_names = names;
	chooser_count = count;
	chooser_cursor = cursor;
	chooser_retry = retry;
	chooser_answer = -1;
	chooser_asking = true;

	Term_fresh();

	if (Tcl_Eval(interp, "event generate . <<Angband_CHOOSEGAME>> -when tail")
			!= TCL_OK)
		Tcl_BackgroundException(interp, TCL_ERROR);

	while (chooser_asking)
		Tcl_DoOneEvent(TCL_ALL_EVENTS);

	chooser_names = NULL;
	return chooser_answer;
}

/* The last place in [start, end) that `what` begins, or NULL. */
static const char *chooser_last(const char *start, const char *end,
		const char *what)
{
	size_t n = strlen(what);
	const char *p;

	for (p = end - n; p >= start; p--) {
		if (!strncmp(p, what, n)) return p;
	}
	return NULL;
}

/**
 * A savefile's description as wr_description() in save.c writes it, in its
 * fields: {state name level race class depth killer}, the state being alive,
 * dead, invalid or unknown.  The two forms are "Name, L12 Race Class, at DL5"
 * and "Name, dead (killer)".  A race or a class can have a space in its name,
 * so the pair is split where it matches a race and a class the game knows.
 */
static void chooser_fields(Tcl_Obj *row, const char *desc)
{
	const char *state = "unknown";
	Tcl_Obj *name = Tcl_NewObj(), *race = Tcl_NewObj(), *cls = Tcl_NewObj();
	Tcl_Obj *killer = Tcl_NewObj();
	int level = 0, depth = 0;
	const char *end = desc ? desc + strlen(desc) : NULL;
	const char *p, *q;

	if (!desc || !desc[0]) {
		/* Nothing said: an old savefile, or not one at all. */
	} else if (streq(desc, "Invalid savefile")) {
		state = "invalid";
	} else if ((p = strstr(desc, ", dead (")) != NULL && suffix(desc, ")")) {
		state = "dead";
		Tcl_SetStringObj(name, desc, (Tcl_Size)(p - desc));
		Tcl_SetStringObj(killer, p + 8, (Tcl_Size)(end - 1 - (p + 8)));
	} else if ((q = chooser_last(desc, end, ", at DL")) != NULL) {
		depth = atoi(q + 7);
		for (p = chooser_last(desc, q, ", L"); p;
				p = chooser_last(desc, p, ", L")) {
			if (isdigit((unsigned char)p[3])) break;
		}
		if (p) {
			const char *rc = p + 3;
			struct player_race *r;
			struct player_class *c;
			bool split = false;

			state = "alive";
			Tcl_SetStringObj(name, desc, (Tcl_Size)(p - desc));
			level = atoi(rc);
			while (isdigit((unsigned char)*rc)) rc++;
			if (*rc == ' ') rc++;
			for (r = races; r && !split; r = r->next) {
				size_t n = strlen(r->name);

				if (strncmp(rc, r->name, n) || rc[n] != ' ') continue;
				for (c = classes; c && !split; c = c->next) {
					if ((size_t)(q - (rc + n + 1)) == strlen(c->name)
							&& !strncmp(rc + n + 1, c->name,
							strlen(c->name))) {
						Tcl_SetStringObj(race, r->name, -1);
						Tcl_SetStringObj(cls, c->name, -1);
						split = true;
					}
				}
			}
			if (!split && rc < q)
				Tcl_SetStringObj(race, rc, (Tcl_Size)(q - rc));
		}
	}

	Tcl_ListObjAppendElement(NULL, row, Tcl_NewStringObj(state, -1));
	Tcl_ListObjAppendElement(NULL, row, name);
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewIntObj(level));
	Tcl_ListObjAppendElement(NULL, row, race);
	Tcl_ListObjAppendElement(NULL, row, cls);
	Tcl_ListObjAppendElement(NULL, row, Tcl_NewIntObj(depth));
	Tcl_ListObjAppendElement(NULL, row, killer);
}

/**
 * angband_savefile -- which character to play, for the Choose Game window.
 *
 *    angband_savefile              {} unless the game is asking, and then
 *                                  {cursor retry}: the choice it starts on, and
 *                                  whether the one chosen last would not load
 *    angband_savefile list         a row per choice, in the game's order:
 *                                  {n file path state name level race class
 *                                  depth killer}, the state being new for a
 *                                  new character and otherwise alive, dead,
 *                                  invalid or unknown
 *    angband_savefile answer <n>   play choice n; -1 quits
 */
static int objcmd_savefile(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i;

	(void)dummy;

	if (objc == 1) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		if (chooser_asking) {
			Tcl_ListObjAppendElement(ip, d, Tcl_NewIntObj(chooser_cursor));
			Tcl_ListObjAppendElement(ip, d, Tcl_NewBooleanObj(chooser_retry));
		}
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	what = Tcl_GetString(objv[1]);
	if (!chooser_asking) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("the game is not asking which"
				" character to play", -1));
		return TCL_ERROR;
	}

	if (streq(what, "list")) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		if (objc != 2) {
			Tcl_WrongNumArgs(ip, 2, objv, "");
			return TCL_ERROR;
		}
		for (i = 0; i < chooser_count; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(i));
			if (chooser_names[i]) {
				char path[1024];

				path_build(path, sizeof(path), ANGBAND_DIR_SAVE,
						chooser_names[i]);
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(chooser_names[i], -1));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(path, -1));
				chooser_fields(row, savefile_get_description(path));
			} else {
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("new", -1));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(0));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
				Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(0));
				Tcl_ListObjAppendElement(ip, row, Tcl_NewObj());
			}
			Tcl_ListObjAppendElement(ip, list, row);
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (streq(what, "answer")) {
		int n;

		if (objc != 3) {
			Tcl_WrongNumArgs(ip, 2, objv, "n");
			return TCL_ERROR;
		}
		if (Tcl_GetIntFromObj(ip, objv[2], &n) != TCL_OK) return TCL_ERROR;
		if (n < -1 || n >= chooser_count) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no choice %d", n));
			return TCL_ERROR;
		}
		chooser_answer = n;
		chooser_asking = false;
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected list or answer, not: %s",
			what));
	return TCL_ERROR;
}

/**
 * The death screen (T9).
 *
 * death_screen() in ui-death.c draws the tombstone -- or the retirement -- on
 * the main screen and then asks its view, through death_view_hook, which of
 * its actions to take; this is the view, for the Tomb window.  An action other
 * than New Game and Quit runs as it does from the game's own menu, with its
 * screens on the main window, and the window is asked again once it is done.
 */
static Tcl_Obj *score_fields(const struct high_score *sc, int rank, bool you);
static unsigned char *last_sight;
static int last_sight_w, last_sight_h;
static bool death_asking;
static int death_answer;
static const menu_action *death_actions_now;
static int death_count;

static int death_view_tk(const menu_action *actions, int count)
{
	bridge_first_wait();

	death_actions_now = actions;
	death_count = count;
	death_answer = -1;
	death_asking = true;

	/* The tombstone, on the screen behind the window. */
	Term_fresh();

	if (Tcl_Eval(interp, "event generate . <<Angband_DEATHVIEW>> -when tail")
			!= TCL_OK)
		Tcl_BackgroundException(interp, TCL_ERROR);

	/* Quitting from the window's menu or the application leaves as Quit. */
	while (death_asking && !terms_disconnecting)
		Tcl_DoOneEvent(TCL_ALL_EVENTS);

	death_asking = false;
	death_actions_now = NULL;
	return death_answer;
}

/**
 * angband_death -- the death screen, for the Tomb window.
 *
 *    angband_death              {} unless the game is asking, and then
 *                               {how actions}: how is dead, retired or winner,
 *                               and actions the menu's, a {key name} each --
 *                               or crown, with none, while a winner's crown is
 *                               on the screen, answered with continue
 *    angband_death tomb         the main screen as the game drew it, a string
 *                               a row, with the blank rows at the end left off
 *    angband_death answer <key> take the action with that key, as from the
 *                               menu; New Game and Quit are taken as confirmed
 *    angband_death entry        the character's score, as the Hall of Fame
 *                               has it (score_fields), and `scored`: whether
 *                               enter_score() put it on the table
 *    angband_death photo <img>  the last thing the character saw, into a
 *                               photo image; 0 when there is none to give
 */
static int objcmd_death(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;
	int i;

	(void)dummy;

	if (objc == 1) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);

		if (death_asking && player) {
			Tcl_Obj *acts = Tcl_NewListObj(0, NULL);
			const char *how = !death_actions_now ? "crown"
				: player->total_winner ? "winner"
				: streq(player->died_from, "Retiring") ? "retired" : "dead";

			for (i = 0; i < death_count; i++) {
				const menu_action *a = &death_actions_now[i];
				char key[2] = { a->tag, 0 };
				Tcl_Obj *row = Tcl_NewListObj(0, NULL);

				if (a->flags & MN_ACT_HIDDEN) continue;
				Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(key, -1));
				Tcl_ListObjAppendElement(ip, row,
						Tcl_NewStringObj(a->name, -1));
				Tcl_ListObjAppendElement(ip, acts, row);
			}
			Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj(how, -1));
			Tcl_ListObjAppendElement(ip, d, acts);
		}
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	what = Tcl_GetString(objv[1]);
	if (!death_asking) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("the game is not at the death"
				" screen", -1));
		return TCL_ERROR;
	}

	if (streq(what, "tomb")) {
		Tcl_Obj *rows = Tcl_NewListObj(0, NULL);
		Tcl_Size n, used = 0;
		term *old = Term;
		int x, y;

		if (objc != 2) {
			Tcl_WrongNumArgs(ip, 2, objv, "");
			return TCL_ERROR;
		}
		Term_activate(term_screen);
		for (y = 0; y < Term->hgt; y++) {
			Tcl_DString ds;
			int a;
			wchar_t c;
			int len;

			Tcl_DStringInit(&ds);
			for (x = 0; x < Term->wid; x++) {
				char u[TCL_UTF_MAX + 1];

				if (Term_what(x, y, &a, &c)) break;
				Tcl_DStringAppend(&ds, u, Tcl_UniCharToUtf((int)c, u));
			}
			len = Tcl_DStringLength(&ds);
			while (len > 0 && Tcl_DStringValue(&ds)[len - 1] == ' ') len--;
			Tcl_ListObjAppendElement(ip, rows,
					Tcl_NewStringObj(Tcl_DStringValue(&ds), len));
			Tcl_DStringFree(&ds);
			if (len) used = y + 1;
		}
		Term_activate(old);

		Tcl_ListObjLength(ip, rows, &n);
		if (used < n) Tcl_ListObjReplace(ip, rows, used, n - used, 0, NULL);
		Tcl_SetObjResult(ip, rows);
		return TCL_OK;
	}

	if (streq(what, "answer")) {
		const char *key;

		if (objc != 3) {
			Tcl_WrongNumArgs(ip, 2, objv, "key");
			return TCL_ERROR;
		}
		key = Tcl_GetString(objv[2]);

		/* The crown has one answer, which is to go on to the tombstone. */
		if (!death_actions_now) {
			if (!streq(key, "continue")) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected continue, not: %s",
						key));
				return TCL_ERROR;
			}
			death_answer = -1;
			death_asking = false;
			return TCL_OK;
		}
		for (i = 0; i < death_count; i++) {
			if (key[0] && !key[1] && death_actions_now[i].tag == key[0]) break;
		}
		if (i == death_count) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no action %s", key));
			return TCL_ERROR;
		}
		death_answer = i;
		death_asking = false;
		return TCL_OK;
	}

	if (streq(what, "entry")) {
		struct high_score e;
		Tcl_Obj *d;
		bool scored;

		if (objc != 2) {
			Tcl_WrongNumArgs(ip, 2, objv, "");
			return TCL_ERROR;
		}
		build_score(&e, player, player->died_from, NULL);
		d = score_fields(&e, 0, true);

		/* enter_score()'s own tests, so the record and the table agree. */
		scored = !player_used_cheat_option(player)
			&& !(player->noscore & (NOSCORE_WIZARD | NOSCORE_DEBUG))
			&& (player->total_winner
				|| (!streq(player->died_from, "Interrupting")
					&& !streq(player->died_from, "Retiring")));
#ifdef ALLOW_BORG
#ifndef SCORE_BORGS
		if (player->noscore & NOSCORE_BORG) scored = false;
#endif
#endif
		Tcl_ListObjAppendElement(ip, d, Tcl_NewStringObj("scored", -1));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewBooleanObj(scored));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	if (streq(what, "photo")) {
		Tk_PhotoHandle photo;
		Tk_PhotoImageBlock blk;

		if (objc != 3) {
			Tcl_WrongNumArgs(ip, 2, objv, "photo");
			return TCL_ERROR;
		}
		photo = Tk_FindPhoto(ip, Tcl_GetString(objv[2]));
		if (!photo) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no photo image named %s",
					Tcl_GetString(objv[2])));
			return TCL_ERROR;
		}
		if (!last_sight) {
			Tcl_SetObjResult(ip, Tcl_NewBooleanObj(false));
			return TCL_OK;
		}
		blk.pixelPtr = last_sight;
		blk.width = last_sight_w;
		blk.height = last_sight_h;
		blk.pitch = last_sight_w * 4;
		blk.pixelSize = 4;
		blk.offset[0] = 0;
		blk.offset[1] = 1;
		blk.offset[2] = 2;
		blk.offset[3] = 3;
		Tk_PhotoBlank(photo);
		if (Tk_PhotoSetSize(ip, photo, last_sight_w, last_sight_h) != TCL_OK
				|| Tk_PhotoPutBlock(ip, photo, &blk, 0, 0, last_sight_w,
					last_sight_h, TK_PHOTO_COMPOSITE_SET) != TCL_OK)
			return TCL_ERROR;
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(true));
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected tomb, answer, entry or"
			" photo, not: %s", what));
	return TCL_ERROR;
}

/**
 * The Hall of Fame (T9).
 *
 * A score's fields as a {name value} list: what the Hall of Fame shows, and
 * what a Character Record is matched to its score by.  The race and the class
 * by name, the numbers as numbers, and `you` for the character being played
 * -- or just finished.
 */
static Tcl_Obj *score_fields(const struct high_score *sc, int rank, bool you)
{
	Tcl_Obj *d = Tcl_NewListObj(0, NULL);
	const struct player_race *r = player_id2race(atoi(sc->p_r));
	const struct player_class *c = player_id2class(atoi(sc->p_c));
	char who[sizeof(sc->who)], day[sizeof(sc->day)];
	size_t n;

	my_strcpy(who, sc->who, sizeof(who));
	for (n = strlen(who); n > 0 && who[n - 1] == ' '; n--) who[n - 1] = 0;
	my_strcpy(day, sc->day, sizeof(day));

#define SCORE_FIELD(name, obj) do { \
		Tcl_ListObjAppendElement(NULL, d, Tcl_NewStringObj(name, -1)); \
		Tcl_ListObjAppendElement(NULL, d, obj); \
	} while (0)
	SCORE_FIELD("rank", Tcl_NewIntObj(rank));
	SCORE_FIELD("pts", Tcl_NewWideIntObj(atol(sc->pts)));
	SCORE_FIELD("gold", Tcl_NewWideIntObj(atol(sc->gold)));
	SCORE_FIELD("turns", Tcl_NewWideIntObj(atol(sc->turns)));
	SCORE_FIELD("day", Tcl_NewStringObj(day, -1));
	SCORE_FIELD("who", Tcl_NewStringObj(who, -1));
	SCORE_FIELD("race", Tcl_NewStringObj(r ? r->name : "", -1));
	SCORE_FIELD("class", Tcl_NewStringObj(c ? c->name : "", -1));
	SCORE_FIELD("cur_lev", Tcl_NewIntObj(atoi(sc->cur_lev)));
	SCORE_FIELD("max_lev", Tcl_NewIntObj(atoi(sc->max_lev)));
	SCORE_FIELD("cur_dun", Tcl_NewIntObj(atoi(sc->cur_dun)));
	SCORE_FIELD("max_dun", Tcl_NewIntObj(atoi(sc->max_dun)));
	SCORE_FIELD("how", Tcl_NewStringObj(sc->how, -1));
	SCORE_FIELD("you", Tcl_NewBooleanObj(you));
#undef SCORE_FIELD

	return d;
}

/**
 * angband_scores -- the Hall of Fame (T9).
 *
 *    angband_scores     every score on the table, best first, as score_fields
 *                       lists.  The character being played is among them, where
 *                       its score would put it, killed by "nobody (yet!)" --
 *                       the game's own prediction, predict_score() -- and a
 *                       character at its death screen is found as itself.
 *                       Either is marked `you`.
 */
static int objcmd_scores(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	struct high_score scores[MAX_HISCORES];
	size_t i, you = MAX_HISCORES;
	Tcl_Obj *list;

	(void)dummy;

	if (objc != 1) {
		Tcl_WrongNumArgs(ip, 1, objv, "");
		return TCL_ERROR;
	}

	highscore_read(scores, N_ELEMENTS(scores));

	if (player && player->upkeep && character_generated && player->race
			&& player->class) {
		if (!player->is_dead) {
			struct high_score live;

			build_score(&live, player, "nobody (yet!)", NULL);
			you = highscore_add(&live, scores, N_ELEMENTS(scores));
		} else {
			struct high_score dead;

			build_score(&dead, player, player->died_from, NULL);
			for (i = 0; i < N_ELEMENTS(scores) && scores[i].what[0]; i++) {
				if (streq(scores[i].who, dead.who)
						&& streq(scores[i].turns, dead.turns)
						&& streq(scores[i].how, dead.how)
						&& streq(scores[i].pts, dead.pts)) {
					you = i;
					break;
				}
			}
		}
	}

	list = Tcl_NewListObj(0, NULL);
	for (i = 0; i < N_ELEMENTS(scores) && scores[i].what[0]; i++)
		Tcl_ListObjAppendElement(ip, list,
				score_fields(&scores[i], (int)i + 1, i == you));
	Tcl_SetObjResult(ip, list);
	return TCL_OK;
}

/**
 * angband_messages ?count? -- the message log, oldest first: the last `count`
 * messages, or all of them, each with its repeat count as the game's own
 * message screen shows it.  For a Character Record (T9).
 */
static int objcmd_messages(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int count = -1, n, age;
	Tcl_Obj *list;

	(void)dummy;

	if (objc > 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "?count?");
		return TCL_ERROR;
	}
	if (objc == 2 && Tcl_GetIntFromObj(ip, objv[1], &count) != TCL_OK)
		return TCL_ERROR;

	n = messages_num();
	if (count >= 0 && count < n) n = count;

	list = Tcl_NewListObj(0, NULL);
	for (age = n - 1; age >= 0; age--) {
		uint16_t times = message_count((uint16_t)age);

		Tcl_ListObjAppendElement(ip, list, times > 1
			? Tcl_ObjPrintf("%s <%dx>", message_str((uint16_t)age), times)
			: Tcl_NewStringObj(message_str((uint16_t)age), -1));
	}
	Tcl_SetObjResult(ip, list);
	return TCL_OK;
}

/**
 * The last thing the character saw: the map's tiles, kept at the moment the
 * game leaves the world of a character who has died -- before the tombstone
 * is drawn over them.  A Character Record's photo (T9, OBS-46); the original
 * kept one too.  Only the tile layer, so a game played in letters has none.
 */
static void last_sight_notice(game_event_type type, game_event_data *data,
		void *user)
{
	Tk_PhotoImageBlock b;
	int x, y, x0, y0, x1 = -1, y1 = -1;

	(void)type;
	(void)data;
	(void)user;

	if (last_sight) mem_free(last_sight);
	last_sight = NULL;

	if (!player || !player->is_dead || !td[0].screen) return;
	if (!Tk_PhotoGetImage(td[0].screen, &b) || b.width <= 0 || b.height <= 0)
		return;

	/* Only where there are tiles: not the sidebar's margin around them. */
	x0 = b.width;
	y0 = b.height;
	for (y = 0; y < b.height; y++) {
		for (x = 0; x < b.width; x++) {
			if (!b.pixelPtr[y * b.pitch + x * b.pixelSize + b.offset[3]])
				continue;
			x0 = MIN(x0, x);
			y0 = MIN(y0, y);
			x1 = MAX(x1, x);
			y1 = MAX(y1, y);
		}
	}
	if (x1 < 0) return;

	last_sight_w = x1 - x0 + 1;
	last_sight_h = y1 - y0 + 1;
	last_sight = mem_zalloc((size_t)last_sight_w * last_sight_h * 4);
	for (y = 0; y < last_sight_h; y++) {
		for (x = 0; x < last_sight_w; x++) {
			const unsigned char *src = b.pixelPtr + (y + y0) * b.pitch
				+ (x + x0) * b.pixelSize;
			unsigned char *dst = last_sight
				+ ((size_t)y * last_sight_w + x) * 4;

			dst[0] = src[b.offset[0]];
			dst[1] = src[b.offset[1]];
			dst[2] = src[b.offset[2]];
			dst[3] = src[b.offset[3]];
		}
	}
}


/**
 * angband_option -- read and write the game's options.
 *
 *    angband_option                     every option, as
 *                                       {name type desc value changeable}
 *    angband_option use_old_target      its value
 *    angband_option use_old_target 1    set it
 *    angband_option save interface|birth      a page's options as the
 *                                       defaults for new characters
 *    angband_option restore interface|birth   back to those defaults
 *    angband_option reset interface|birth     back to the game's own
 *    angband_option scoring             {cheated wizard eligible}: whether a
 *                                       cheat or wizard mode has been used,
 *                                       and whether the character can still
 *                                       go on the high-score list
 *
 * The first of the read accessors, and the pattern the rest follow: the names
 * are the game's own -- list-options.h through option_name() -- rather than a
 * second set kept in step by hand, and writing goes through option_set(), so
 * the birth-option and cheat-option rules apply to a script exactly as they
 * apply to the options screen.
 *
 * The three numeric settings in struct player_options are not in that enum but
 * are what an options dialog shows beside it, so they are named here as well.
 * They are marked with the type "value" rather than a page, which is how a
 * dialog knows to draw a slider instead of a checkbox.
 *
 * What option_set() does not do is refuse a birth option once there is a
 * character: the game's own options screen enforces that by greying the page,
 * and this said option_set() did it, which let a script change a birth option
 * in the middle of a game.  Refused here now, and reported in the list as
 * whether each option can be changed at all, so a window can grey it too.
 *
 * Four cheats are things done rather than settings -- take gold, take hit
 * points, gain a pet, know every place -- and the options screen does each
 * once and switches it back off, leaving the score option that records it.
 * Setting one here does the same.  They act on the game, so only while it is
 * waiting for a command; each asks what it wants through get_string, which a
 * window can answer through angband_hook.
 */
static bool option_changeable(int opt)
{
	return option_type(opt) != OP_BIRTH || !character_generated;
}

static const struct {
	const char *name;
	void (*act)(struct command *cmd);
} option_cheat_action[] = {
	{ "cheat_gold", do_cmd_wiz_gain_gold },
	{ "cheat_hp", do_cmd_wiz_gain_hp },
	{ "cheat_pet", do_cmd_wiz_gain_pet },
	{ "cheat_places", do_cmd_wiz_know_places },
};
static const struct {
	const char *name;
	const char *desc;
	int max;
} option_value[] = {
	{ "hitpoint_warn", "Hitpoint warning", 9 },
	{ "lazymove_delay", "Movement delay factor", 9 },
	{ "delay_factor", "Base delay factor", 9 },
};

/*
 * Kept beside the table above rather than as a pointer in it: the fields live
 * in player->opts, which does not exist until a character does, so the address
 * has to be taken when it is asked for and not when the table is written.
 */

static uint8_t *option_value_at(int i)
{
	if (!player) return NULL;

	switch (i) {
	case 0: return &player->opts.hitpoint_warn;
	case 1: return &player->opts.lazymove_delay;
	case 2: return &player->opts.delay_factor;
	}

	return NULL;
}

static int objcmd_option(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *name;
	int i, val;

	(void)dummy;

	if (objc > 3) {
		Tcl_WrongNumArgs(ip, 1, objv, "?name? ?value?");
		return TCL_ERROR;
	}

	if (!player) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				"the options belong to a character, and there is not one yet",
				-1));
		return TCL_ERROR;
	}

	if (objc == 1) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		for (i = 0; i < OPT_MAX; i++) {
			Tcl_Obj *row;

			/*
			 * OPT_none is the first entry in list-options.h and carries an
			 * empty description; it is a placeholder for index zero, not
			 * something to put in front of a player.
			 */
			if (!option_name(i) || !option_desc(i) || !option_desc(i)[0])
				continue;

			row = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(option_name(i), -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(option_type_name(option_type(i)), -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(option_desc(i), -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(player->opts.opt[i]));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(option_changeable(i)));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		for (i = 0; i < (int)N_ELEMENTS(option_value); i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);
			uint8_t *at = option_value_at(i);

			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(option_value[i].name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj("value", -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(option_value[i].desc, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj(at ? *at : 0));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewBooleanObj(true));
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);

		return TCL_OK;
	}

	name = Tcl_GetString(objv[1]);

	/* By enter_score()'s own tests, so the window cannot disagree with it. */
	if (objc == 2 && streq(name, "scoring")) {
		Tcl_Obj *d = Tcl_NewListObj(0, NULL);
		bool cheated = player_used_cheat_option(player);
		bool wizard = (player->noscore
				& (NOSCORE_WIZARD | NOSCORE_DEBUG)) != 0;
#ifdef ALLOW_BORG
		if (player->noscore & NOSCORE_BORG) wizard = true;
#endif

		Tcl_ListObjAppendElement(ip, d, Tcl_NewBooleanObj(cheated));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewBooleanObj(wizard));
		Tcl_ListObjAppendElement(ip, d, Tcl_NewBooleanObj(!cheated && !wizard));
		Tcl_SetObjResult(ip, d);
		return TCL_OK;
	}

	/* A page's defaults, as the options screen's s, r and x keys. */
	if (objc == 3 && (streq(name, "save") || streq(name, "restore")
			|| streq(name, "reset"))) {
		const char *which = Tcl_GetString(objv[2]);
		int page;
		bool done = true;

		if (streq(which, "interface")) {
			page = OP_INTERFACE;
		} else if (streq(which, "birth")) {
			page = OP_BIRTH;
		} else {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"expected interface or birth, not: %s", which));
			return TCL_ERROR;
		}
		if (!streq(name, "save") && page == OP_BIRTH && character_generated) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"birth options are fixed once a character is made", -1));
			return TCL_ERROR;
		}

		if (streq(name, "save"))
			done = options_save_custom(&player->opts, page);
		else if (streq(name, "restore"))
			done = options_restore_custom(&player->opts, page);
		else
			options_restore_maintainer(&player->opts, page);

		if (!done) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("the %s defaults could not be"
					" %s", which, streq(name, "save") ? "saved" : "read"));
			return TCL_ERROR;
		}
		return TCL_OK;
	}

	for (i = 0; i < (int)N_ELEMENTS(option_value); i++) {
		if (strcmp(name, option_value[i].name) == 0) {
			uint8_t *at = option_value_at(i);

			if (objc == 2) {
				Tcl_SetObjResult(ip, Tcl_NewIntObj(at ? *at : 0));
				return TCL_OK;
			}
			if (Tcl_GetIntFromObj(ip, objv[2], &val) != TCL_OK)
				return TCL_ERROR;
			if (val < 0 || val > option_value[i].max) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf("%s runs from 0 to %d",
						name, option_value[i].max));
				return TCL_ERROR;
			}
			if (at) *at = (uint8_t)val;
			return TCL_OK;
		}
	}

	for (i = 0; i < OPT_MAX; i++) {
		if (option_name(i) && strcmp(name, option_name(i)) == 0) {
			if (objc == 2) {
				Tcl_SetObjResult(ip, Tcl_NewBooleanObj(player->opts.opt[i]));
				return TCL_OK;
			}

			size_t c;

			val = obj_true(objv[2]) ? 1 : 0;

			if (!option_changeable(i)) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"birth options are fixed once a character is made",
						-1));
				return TCL_ERROR;
			}

			for (c = 0; c < N_ELEMENTS(option_cheat_action); c++)
				if (streq(name, option_cheat_action[c].name)) break;
			if (val && c < N_ELEMENTS(option_cheat_action)
					&& !command_may_start()) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"the game is in the middle of something else", -1));
				return TCL_ERROR;
			}

			/*
			 * option_set, not a write straight into the array: it is what
			 * marks a character as a cheater when a cheat option goes on.  A
			 * script has no business getting round that.
			 */
			if (!option_set(name, val)) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf(
						"%s cannot be changed now", name));
				return TCL_ERROR;
			}

			if (val && c < N_ELEMENTS(option_cheat_action)) {
				{
					COMMAND_INSIDE_HELD;
					option_cheat_action[c].act(NULL);
				}
				player->opts.opt[i] = false;
				handle_stuff(player);
			}

			return TCL_OK;
		}
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such option: %s", name));

	return TCL_ERROR;
}

/**
 * Is there a game to ask questions of?
 *
 * The prereq predicates read the character and the level, and they do it
 * without checking: player_can_cast_prereq goes straight to p->class, and
 * several of the others end up in square_in_bounds(cave, ...), which asserts
 * on a NULL cave.  A menu can be built at any time -- the front end is up long
 * before a character is -- so this is the gate in front of all of them.
 *
 * Measured, not guessed: a script that called angband_commands from a binding
 * during startup aborted in square_in_bounds while the data files were still
 * loading, because player exists well before cave does.
 */
static bool in_play(void)
{
	return player != NULL && character_dungeon;
}

/**
 * Whether the interface may start a command right now.
 *
 * Two conditions, and the first one alone is not enough.
 *
 * inkey_flag is 4.2's own "the main loop is waiting for a command", and
 * main-win.c guards its menus with it.  What it does *not* mean is "no command
 * is running": ui-input.c sets it before the main loop's inkey() and clears it
 * only once that inkey() has a key to return, so it stays true for the whole
 * wait -- including the nested wait inside a prompt that a command opened.
 *
 * That gap is a heap corruption and not a cosmetic one.  A menu command runs
 * inside Tcl_DoOneEvent, which is reached from the very inkey() that is
 * waiting; if that command opens an item prompt, the prompt's own inkey()
 * pumps Tk again, the menu bar is still live, and a second command starts
 * inside the first.  textui_get_item keeps its floor and throwing lists in
 * file statics, so the inner call allocates over them and frees them on the
 * way out, and the outer call then frees the same two pointers -- which aborts
 * in malloc with POINTER_BEING_FREED_WAS_NOT_ALLOCATED.  It is not only that
 * function: most of 4.2's ui-* prompts keep their state in statics, because
 * nothing in a keyboard interface can call one of them twice at once.
 *
 * So the front end counts for itself.  command_inside is true from the moment
 * a menu-invoked hook is entered until it returns, and nothing the interface
 * offers will start while it is.  It is declared beside command_nudge,
 * because the gear commands need it too and they come first in this file.
 */
static bool command_may_start(void)
{
	return in_play() && inkey_flag && !command_inside;
}

/**
 * Can the character do this, asked quietly?
 *
 * The command table's prereq field is not a predicate.  It is the thing the
 * game runs when the player has already pressed the key, and five of the six
 * in cmds_all *print a message* explaining the refusal -- they are
 * player_can_cast(p, true) and friends, where the second argument is
 * show_msg.  The sixth, player_can_debug_prereq, goes further and puts up a
 * confirmation dialog.
 *
 * Calling them to decide what a menu should grey out therefore does not work.
 * Measured: asking all 132 in a row got as far as "Gain new spells" and stopped
 * dead, because a run of messages reaches the -more- prompt and -more- waits
 * for a keypress.  cmd->prereq() is called in exactly one place in the whole
 * game -- ui-game.c's dispatch -- and 4.2's own command menu greys nothing.
 *
 * So the quiet half of each pair is called instead.  They already exist;
 * only the wrappers are noisy.
 *
 * An unknown prereq is reported as *available* rather than called.  A new one
 * upstream would otherwise hang the menu the first time it was opened, and
 * "offered, then refused with a message" is what the game does anyway.
 */
static bool command_available(const struct cmd_info *c)
{
	if (!command_may_start()) return false;

	if (!c->prereq) return true;

	if (c->prereq == player_can_cast_prereq)
		return player_can_cast(player, false);
	if (c->prereq == player_can_study_prereq)
		return player_can_study(player, false);
	if (c->prereq == player_can_fire_prereq)
		return player_can_fire(player, false);
	if (c->prereq == player_can_refuel_prereq)
		return player_can_refuel(player, false);
	if (c->prereq == player_can_read_prereq) {
		/* 'r' is overloaded while a monster is commanded; see the original. */
		return player->timed[TMD_COMMAND]
			? true : player_can_read(player, false);
	}
	if (c->prereq == player_can_debug_prereq) {
		/*
		 * The real one asks confirm_debug() and marks the savefile.  A menu
		 * may not do either, so it reports what is already true.
		 */
		return (player->noscore & NOSCORE_DEBUG) != 0;
	}

	return true;
}

/**
 * angband_commands -- the game's own command table, for building menus from.
 *
 * Returns one row per command as
 * {groupindex group index label key enabled level code opens}, where
 * and index address it again for angband_command below.  The label, the key
 * and whether it is currently allowed are all the game's: ui-input.h's
 * struct cmd_info carries a description, up to two keys, a cmd_code and a
 * prereq predicate, and cmds_all[] groups those into named lists.
 *
 * T7 builds its menu bar from this rather than hand-authoring one, which is
 * what keeps the menus and the keyboard from drifting apart.
 */
static int objcmd_commands(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	Tcl_Obj *list;
	size_t g;

	(void)dummy;
	(void)objc;
	(void)objv;

	list = Tcl_NewListObj(0, NULL);

	/*
	 * cmds_all[] is declared extern with no size, so N_ELEMENTS is not
	 * available here; the table ends with a NULL name, which is how
	 * cmd_list_lookup_by_name finds the end too.
	 */
	for (g = 0; cmds_all[g].name; g++) {
		struct command_list *group = &cmds_all[g];
		size_t i;

		for (i = 0; i < group->len; i++) {
			struct cmd_info *c = &group->list[i];
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);
			unsigned char key;
			char keybuf[8];

			if (!c->desc) continue;

			/*
			 * player is NULL until a character exists, and a menu
			 * can be built before one does, so fall back to the
			 * original keyset rather than dereferencing it.
			 */
			int mode = (in_play() && OPT(player, rogue_like_commands))
					? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

			/*
			 * Written for a menu to show beside the label, so a control
			 * key is spelled the way the game spells it, "^X".
			 *
			 * Note cmd_lookup_key, not cmd_lookup_key_unktrl: the latter
			 * turns a missing key -- zero -- into "@", and every key here
			 * is missing until textui_init has called cmd_init, which is
			 * after the front end starts.  A menu built at startup
			 * therefore carries no keys; one rebuilt on <<Angband_ENTER_WORLD>>
			 * carries them all.
			 */
			/*
			 * A command with a cmd_code is looked up through the keymap, so
			 * the accelerator follows whatever the player has bound.  A
			 * user-interface action has no code -- it is a hook -- and
			 * cmd_lookup_key cannot find it, so its declared key is used
			 * instead.  Without the fallback, Look around, Rest and the rest
			 * of the hooks showed no key at all.
			 */
			key = c->cmd ? cmd_lookup_key(c->cmd, mode) : 0;
			if (!key) key = (unsigned char)c->key[mode];
			/*
			 * The keys above ASCII are the game's own codes for
			 * keys with no character, KC_TAB being the roguelike
			 * keyset's Fire at nearest target.  Passed through as
			 * a byte they were not even valid UTF-8, so Tab and
			 * Enter, which a keymap can also bind, are named the
			 * way Tk names them, and any other is left out.
			 */
			if (key == KC_TAB) {
				my_strcpy(keybuf, "Tab", sizeof(keybuf));
			} else if (key == KC_ENTER) {
				my_strcpy(keybuf, "Return", sizeof(keybuf));
			} else if (key >= 0x7f) {
				key = 0;
			} else if (key && key < 0x20) {
				keybuf[0] = '^';
				keybuf[1] = (char)UN_KTRL_CAP(key);
				keybuf[2] = 0;
			} else {
				keybuf[0] = (char)key;
				keybuf[1] = 0;
			}

			/*
			 * The group's index as well as its name.  angband_command
			 * addresses a command by two numbers, and without the first of
			 * them a caller would have to count the groups itself to turn
			 * a row back into something it can run.
			 */
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj((int)g));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(group->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj((int)i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(c->desc, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(key ? keybuf : "", -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(command_available(c)));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewIntObj(group->menu_level));
			/*
			 * The cmd_code by name, so a script can push the same command
			 * through angband_push with arguments of its own rather than
			 * letting the game prompt for them.  Empty for the entries that
			 * are user-interface actions with a hook and no command.
			 */
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
					(c->cmd > 0 && c->cmd < (int)N_ELEMENTS(cmd_code_name))
						? cmd_code_name[c->cmd] : "", -1));
			/*
			 * The group this entry opens, for the ones that are access points
			 * rather than commands: the Debug group's nine entries have no
			 * code and no key and exist to lead somewhere else.  Without the
			 * link, a caller can see the readable name ("Player") or the group
			 * it opens ("DbgPlayer") but has no way to join them up.
			 */
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(
					c->nested_name ? c->nested_name : "", -1));
			Tcl_ListObjAppendElement(ip, list, row);
		}
	}

	Tcl_SetObjResult(ip, list);

	return TCL_OK;
}

/**
 * Wake the game up after putting something on its queue.
 *
 * play_game() calls cmd_get_hook() unconditionally every turn, and that ends
 * in inkey(), which blocks until a *key* arrives.  A command pushed from a Tk
 * event handler -- a menu item, a button -- lands on the queue while the game
 * is still sitting in there waiting, so nothing happens until the player
 * presses something else, and then two commands run at once.  Reported from
 * play: choosing "Throw an item" did nothing until a direction key was
 * pressed.
 *
 * So the queue push is followed by a keypress the game treats as no command at
 * all.  textui_process_key() returns true for \0, escape, space and \a with
 * the key left unset, and textui_process_command() returns silently on
 * "if (!key && done)".  Of those four, \a is the one nothing else means:
 * escape cancels a prompt and space pages one.
 */
static void command_nudge(void)
{
	bridge_key('\a');
}

/**
 * angband_command -- run one entry from that table.
 *
 * Dispatched exactly as textui_process_command does it: check the prereq, call
 * the hook if it is a user-interface action, otherwise push the command code
 * onto the queue.  Going through the same two branches is what stops a menu
 * item and its key doing subtly different things.
 *
 * This is the "down" seam, and note what it is not: no keystroke is
 * synthesised.  The original had no choice about that; we do.
 */
static int objcmd_command(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	struct cmd_info *c;
	int g, i, count = 1;

	(void)dummy;

	if (objc < 3 || objc > 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "group index ?count?");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &g) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[2], &i) != TCL_OK) return TCL_ERROR;
	if (objc == 4 && Tcl_GetIntFromObj(ip, objv[3], &count) != TCL_OK)
		return TCL_ERROR;

	{
		int n;
		for (n = 0; cmds_all[n].name; n++) ;
		if (g < 0 || g >= n) {
			Tcl_SetObjResult(ip,
					Tcl_NewStringObj("no such command group", -1));
			return TCL_ERROR;
		}
	}
	if (i < 0 || i >= (int)cmds_all[g].len) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("no such command", -1));
		return TCL_ERROR;
	}

	c = &cmds_all[g].list[i];

	/*
	 * Two refusals, not one: "there is no character" and "the game is busy"
	 * are different answers to different questions, and a caller that cannot
	 * tell them apart cannot say anything useful about either.
	 */
	if (!in_play()) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("not allowed just now", -1));
		return TCL_ERROR;
	}

	/*
	 * Refused outright unless the game is idle.  The greyed menu is the
	 * courtesy; this is the guarantee, and it is the one that matters -- a
	 * command started inside another one corrupts the heap, not merely the
	 * display.  See command_may_start().
	 */
	if (!command_may_start()) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				"the game is in the middle of something else", -1));
		return TCL_ERROR;
	}

	/*
	 * The real prereq here, not the quiet one: by this point the player has
	 * chosen the command, and its message is how they are told why nothing
	 * happened.  Menus ask command_available(); dispatch asks the game.
	 */
	if (c->prereq && !c->prereq()) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("not allowed just now", -1));
		return TCL_ERROR;
	}
	if (c->hook) {
		/*
		 * A user-interface action runs here and now, inside the event handler
		 * -- which is where the game would have run it too.  It can prompt,
		 * and its prompt pumps Tk, so the door is held shut for as long as it
		 * runs.
		 *
		 * And then the game is woken, as for a push.  This used to say there
		 * was nothing to wake, but most of these actions end by queueing the
		 * command they asked about -- Retire queues CMD_RETIRE once the '@' is
		 * typed, Rest queues CMD_REST with its count -- and the game, back in
		 * the wait the handler interrupted, left it on the queue until the
		 * player pressed another key.  When nothing was queued the nudge is no
		 * command at all.
		 */
		{
			COMMAND_INSIDE_HELD;
			c->hook();
		}
		command_nudge();
	} else if (c->cmd) {
		cmdq_push_repeat(c->cmd, count);
		command_nudge();
	} else if (c->nested_prompt) {
		/*
		 * Not a command but a door to some: "Debug mode commands" is what
		 * the keyboard's ^A opens -- the game confirms debug mode, marks the
		 * character, and asks which debug command.  It has neither a hook
		 * nor a command to push, so this did nothing at all.  The commands
		 * behind it have a menu of their own; a click here does what the key
		 * does, by pressing it, and the game's own prompt does the rest.
		 */
		keycode_t key = c->key[OPT(player, rogue_like_commands) ? 1 : 0];

		if (!key) key = c->key[0];
		if (key) bridge_key(key);
	}

	return TCL_OK;
}

/**
 * angband_rename <name> -- what 'c' does on the game's character screen.
 *
 * That is all it does: do_cmd_change_name() copies the new name into
 * full_name and nothing else, so the savefile keeps its name and nothing on
 * the main screen shows it to redraw.  The game's refusal is kept, in the
 * game's words, for a name fixed on the command line with -n.  The length is
 * the game's too: PLAYER_NAME_LEN, the size of full_name and of the buffer
 * get_character_name() is given.
 */
static int objcmd_rename(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *name;
	Tcl_Size len;

	(void)dummy;

	if (objc != 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "name");
		return TCL_ERROR;
	}
	if (!in_play()) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}
	if (arg_force_name) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				"You are not allowed to change your name!", -1));
		return TCL_ERROR;
	}

	name = Tcl_GetStringFromObj(objv[1], &len);
	if (len == 0) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("a name cannot be empty", -1));
		return TCL_ERROR;
	}
	if (len >= PLAYER_NAME_LEN) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf(
				"a name can be at most %d characters", PLAYER_NAME_LEN - 1));
		return TCL_ERROR;
	}

	my_strcpy(player->full_name, name, sizeof(player->full_name));
	Tcl_SetObjResult(ip, Tcl_NewStringObj(player->full_name, -1));

	return TCL_OK;
}

/**
 * angband_version -- the game's name and version, as its own Version info
 * command gives them: for the About window.
 */
static int objcmd_version(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	(void)dummy;

	if (objc != 1) {
		Tcl_WrongNumArgs(ip, 1, objv, "");
		return TCL_ERROR;
	}
	Tcl_SetObjResult(ip, Tcl_NewStringObj(buildid, -1));
	return TCL_OK;
}

/**
 * angband_path -- where the game keeps things, for what the front end keeps
 * beside them.
 *
 *    angband_path user      the player's own folder: pref files, character
 *                           dumps, and the front end's settings
 */
static int objcmd_path(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;

	(void)dummy;

	if (objc != 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "user");
		return TCL_ERROR;
	}
	what = Tcl_GetString(objv[1]);
	if (streq(what, "user")) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(ANGBAND_DIR_USER, -1));
		return TCL_OK;
	}
	Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected user, not: %s", what));
	return TCL_ERROR;
}

/**
 * angband_dump -- what 'f' does on the game's character screen.
 *
 *    angband_dump suggest       the file the game would offer, in the user
 *                               directory and named for the character
 *    angband_dump write <path>  write the character dump there
 *
 * The dump is the game's own, dump_save(), so the file is the same one the
 * character screen writes.  It is not a pure read: write_character_dump()
 * draws the character screen onto the main term and reads it back off the
 * cells, so the screen is saved around it exactly as the character screen
 * saves it, and the main term is made current for the length of it.  Drawing
 * on the screen in the middle of another command's prompt is what
 * command_may_start() exists to refuse, and while it runs the door is held
 * shut, because screen_save() can stop at a -more- prompt.
 */
static int objcmd_dump(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what;

	(void)dummy;

	if (objc < 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "suggest|write ?path?");
		return TCL_ERROR;
	}
	what = Tcl_GetString(objv[1]);

	if (!in_play()) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("there is no character yet", -1));
		return TCL_ERROR;
	}

	if (strcmp(what, "suggest") == 0 && objc == 2) {
		char fname[80];
		char path[1024];

		player_safe_name(fname, sizeof(fname), player->full_name, false);
		my_strcat(fname, ".txt", sizeof(fname));
		path_build(path, sizeof(path), ANGBAND_DIR_USER, fname);
		Tcl_SetObjResult(ip, Tcl_NewStringObj(path, -1));
		return TCL_OK;
	}

	if (strcmp(what, "write") == 0 && objc == 3) {
		term *old = Term;
		bool ok;

		/*
		 * The death screen waits on the Tomb window and nothing else, so
		 * a dump from there is the one its own File dump would write.
		 */
		if (!command_may_start() && !death_asking) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the game is in the middle of something else", -1));
			return TCL_ERROR;
		}

		{
			COMMAND_INSIDE_HELD;
			Term_activate(term_screen);
			screen_save();
			ok = dump_save(Tcl_GetString(objv[2]));
			screen_load();
			Term_activate(old);
		}

		if (!ok) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("could not write %s",
					Tcl_GetString(objv[2])));
			return TCL_ERROR;
		}
		return TCL_OK;
	}

	Tcl_SetObjResult(ip, Tcl_ObjPrintf(
			"expected suggest or write <path>, not: %s", what));
	return TCL_ERROR;
}

/**
 * Keep the game's own animations running while it waits for a key.
 *
 * idle_update() is what makes shimmering things shimmer.  Every other front
 * end calls it from its event loop when nothing else is happening -- Cocoa on
 * a 0.2 second timer, X11 and Windows every tenth pass of a 0.02 second wait
 * -- and ours had no equivalent, so a tileset that declares `cycle:` sat
 * still.
 *
 * A Tcl timer is the natural shape for it here: the game blocks inside
 * Tcl_DoOneEvent waiting for a keystroke, and a timer is an event like any
 * other, so this runs without a polling loop and without waking the process
 * when there is nothing to draw.
 *
 * The game decides whether anything actually moves -- idle_update() returns
 * immediately unless the player has turned on animate_flicker, which is off
 * by default, and unless the tileset says its colours are laid out to be
 * cycled.
 */
#define ANIMATION_MS 200

static void animation_tick(void *dummy)
{
	(void)dummy;

	idle_update();
	Tcl_CreateTimerHandler(ANIMATION_MS, animation_tick, NULL);
}

/**
 * Source one of the front end's scripts, by name, from ANGBAND_DIR_TCL.
 *
 * Everything that reads a script goes through here.  The point is that no
 * script path is ever written down anywhere else: a development build resolves
 * ANGBAND_DIR_TCL to lib/tcl in the source tree, a bundle to the copy in
 * Contents/Resources, and a release will eventually resolve it inside zipfs --
 * and none of that reaches the scripts or their callers.
 */
static bool tcl_source(const char *name)
{
	char path[1024];

	path_build(path, sizeof(path), ANGBAND_DIR_TCL, name);

	if (Tcl_EvalFile(interp, path) != TCL_OK) {
		plog_fmt("Tcl/Tk: %s: %s", path, Tcl_GetStringResult(interp));
		return false;
	}

	return true;
}

/**
 * Load one graphics mode's sheet, replacing whatever is loaded.
 *
 * GRAPHICS_NONE is a legitimate choice and means text: the sheet is dropped
 * and Term_pict is simply never called again, because the game stops setting
 * the high bit on the attribute.
 */
/*
 * How big a tile is drawn, as a multiple of the smallest: Small, Medium and
 * Large on the Display page are 1, 2 and 3.
 */
static int tile_scale = 1;

static bool graphics_load(graphics_mode *mode)
{
	char path[1024];

	if (sheet) {
		mem_free(sheet);
		sheet = NULL;
		sheet_w = sheet_h = 0;
	}

	if (!mode || mode->grafID == GRAPHICS_NONE) {
		current_graphics_mode = get_graphics_mode(GRAPHICS_NONE);
		use_graphics = GRAPHICS_NONE;
		tile_width = 1;
		tile_height = 1;
		return true;
	}

	path_build(path, sizeof(path), mode->path, mode->file);
	if (!file_exists(path)) {
		plog_fmt("Tcl/Tk: %s is missing.", path);
		return false;
	}

	if (!png_load(path, &sheet, &sheet_w, &sheet_h)) {
		plog_fmt("Tcl/Tk: could not read %s.", path);
		return false;
	}

	current_graphics_mode = mode;
	use_graphics = mode->grafID;

	/*
	 * Two cells across, one down, whatever the set -- times the tile scale.
	 *
	 * The arithmetic is the same for every square tileset: we want the drawn
	 * tile to be about as wide as it is tall, so cw*tile_width should be near
	 * ch, and 20/11 rounds to 2 regardless of whether the tile is 8, 16 or 32
	 * pixels.  The tile's own size only decides how much detail survives the
	 * scaling.  The scale is the Display page's tile size: 2x2 cells, 4x2,
	 * 6x3, as the game's other front ends multiply theirs.
	 */
	tile_width = 2 * tile_scale;
	tile_height = tile_scale;

	return true;
}

/**
 * angband_tilesets -- the sets that are installed, as {id name} pairs.
 */
static int objcmd_tilesets(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	graphics_mode *mode;
	Tcl_Obj *list;

	(void)dummy;
	(void)objc;
	(void)objv;

	list = Tcl_NewListObj(0, NULL);
	for (mode = graphics_modes; mode; mode = mode->pNext) {
		Tcl_Obj *pair = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(ip, pair, Tcl_NewIntObj(mode->grafID));
		Tcl_ListObjAppendElement(ip, pair,
				Tcl_NewStringObj(mode->menuname, -1));
		Tcl_ListObjAppendElement(ip, list, pair);
	}
	Tcl_SetObjResult(ip, list);

	return TCL_OK;
}

/**
 * One of the current set's tiles, into a photo image for a window to show (T9).
 *
 * Scaled to `size` pixels on its longer side, nearest-neighbour as the panes
 * scale, with the sheet's own alpha -- so a tile drawn over black in a pane
 * is drawn over whatever the window puts behind it.  False, and the photo left
 * alone, when the game is drawing letters rather than tiles, or the glyph is
 * not a tile at all.
 */
static bool tile_to_photo(Tk_PhotoHandle photo, int a, int c, int size)
{
	Tk_PhotoImageBlock blk;
	unsigned char *buf;
	int tw, th, sx, sy, dw, dh, px, py;

	if (!sheet || !current_graphics_mode || use_graphics == GRAPHICS_NONE)
		return false;
	if (!(a & 0x80)) return false;

	tw = current_graphics_mode->cell_width;
	th = current_graphics_mode->cell_height;
	sx = (c & 0x7f) * tw;
	sy = (a & 0x7f) * th;
	if (tw <= 0 || th <= 0 || sx + tw > sheet_w || sy + th > sheet_h)
		return false;

	if (size <= 0) {
		dw = tw;
		dh = th;
	} else {
		dw = MAX(1, size * tw / MAX(tw, th));
		dh = MAX(1, size * th / MAX(tw, th));
	}

	buf = mem_zalloc((size_t)dw * dh * 4);
	for (py = 0; py < dh; py++) {
		int ty = sy + (py * th) / dh;

		for (px = 0; px < dw; px++) {
			int tx = sx + (px * tw) / dw;

			memcpy(buf + ((size_t)py * dw + px) * 4,
				   sheet + ((size_t)ty * sheet_w + tx) * 4, 4);
		}
	}

	blk.pixelPtr = buf;
	blk.width = dw;
	blk.height = dh;
	blk.pitch = dw * 4;
	blk.pixelSize = 4;
	blk.offset[0] = 0;
	blk.offset[1] = 1;
	blk.offset[2] = 2;
	blk.offset[3] = 3;

	Tk_PhotoBlank(photo);
	if (Tk_PhotoSetSize(interp, photo, dw, dh) != TCL_OK
			|| Tk_PhotoPutBlock(interp, photo, &blk, 0, 0, dw, dh,
				TK_PHOTO_COMPOSITE_SET) != TCL_OK) {
		mem_free(buf);
		return false;
	}
	mem_free(buf);
	return true;
}

/**
 * angband_tile -- the current set's tiles, for the windows (T9).
 *
 *    angband_tile player <photo> ?size?
 *        the character's own tile into a photo image: the one the map draws,
 *        which the set's prefs choose by race and class.  Scaled to `size`
 *        pixels on its longer side, or the tile's own size.  1, or 0 when
 *        the game is drawing letters, so that a window can show the letter.
 *
 * The windows showed an "@" where the original showed the character's tile --
 * on the Character window, and on the tombstone (OBS-46).
 */
static int objcmd_tile(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	Tk_PhotoHandle photo;
	const char *what;
	int size = 0;

	(void)dummy;

	if (objc < 3 || objc > 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "player photo ?size?");
		return TCL_ERROR;
	}
	what = Tcl_GetString(objv[1]);
	if (!streq(what, "player")) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("expected player, not: %s", what));
		return TCL_ERROR;
	}
	photo = Tk_FindPhoto(ip, Tcl_GetString(objv[2]));
	if (!photo) {
		Tcl_SetObjResult(ip, Tcl_ObjPrintf("no photo image named %s",
				Tcl_GetString(objv[2])));
		return TCL_ERROR;
	}
	if (objc == 4 && Tcl_GetIntFromObj(ip, objv[3], &size) != TCL_OK)
		return TCL_ERROR;

	Tcl_SetObjResult(ip, Tcl_NewBooleanObj(monster_x_attr && monster_x_char
			&& tile_to_photo(photo, monster_x_attr[0], monster_x_char[0],
				size)));
	return TCL_OK;
}

/**
 * angband_tileset -- read or change the tile set, by id.
 *
 * Changing it goes through the game: reset_visuals reloads the set's pref
 * file, which is what maps every feature, monster and object onto a tile, and
 * do_cmd_redraw puts the result on screen.  Doing it that way rather than
 * redrawing ourselves is what makes the preview honest -- what you see after
 * choosing is what you get, because it went through the same path.
 */
static int objcmd_tileset(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	graphics_mode *mode;
	int id;

	(void)dummy;

	if (objc == 1) {
		Tcl_SetObjResult(ip, Tcl_NewIntObj(
				current_graphics_mode ? current_graphics_mode->grafID : 0));
		return TCL_OK;
	}
	if (objc != 2) {
		Tcl_WrongNumArgs(ip, 1, objv, "?id?");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &id) != TCL_OK) return TCL_ERROR;

	mode = get_graphics_mode((uint8_t)id);
	if (!mode) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("no such tile set", -1));
		return TCL_ERROR;
	}

	if (!graphics_load(mode)) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("could not load that set", -1));
		return TCL_ERROR;
	}

	/*
	 * Both of these need more than a tile sheet, and the Display page is
	 * reachable from the Choose Game window, before the game has read
	 * anything.
	 *
	 * reset_visuals() walks f_info, k_info, r_info and trap_info to put the
	 * default attr and char back; none of them exists until init_angband
	 * has run, and `angband_tileset 1` before that dereferenced a null
	 * f_info and took the process down with SIGSEGV at offset 0x28 -- the
	 * first read of feat->d_attr. Checked per array, the way every other
	 * command in this file checks the list it is about to walk.
	 *
	 * do_cmd_redraw() wants a dungeon to draw, and is guarded here the same
	 * way angband_tilescale guards its own.
	 *
	 * Loading the sheet above is not conditional: choosing a tile set with
	 * no game in play is a reasonable thing to do, and it takes effect when
	 * one starts. It is only these two that have to wait.
	 */
	if (f_info && k_info && r_info && trap_info && z_info)
		reset_visuals(true);
	if (character_dungeon)
		do_cmd_redraw();

	return command_done(ip);
}

static void build_cells(term_data *td);

/**
 * angband_tilescale ?n? -- read or change how big a tile is drawn: n times
 * the smallest, two cells across and one down.  1 to 4; the Display page
 * offers 1, 2 and 3 as Small, Medium and Large.
 *
 * The game's own setting, tile_width and tile_height, as the other front
 * ends' tile multipliers are, so the map's layout follows it -- and the
 * overhead map's with it, which lays itself out by the same two numbers.
 * Drawn again through do_cmd_redraw, which centres the map for the new size.
 * Without tiles there is nothing to scale, and the scale waits for a set.
 */
static int objcmd_tilescale(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int n;

	(void)dummy;

	if (objc == 2) {
		if (Tcl_GetIntFromObj(ip, objv[1], &n) != TCL_OK) return TCL_ERROR;
		if (n < 1 || n > 4) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the tile scale runs from 1 to 4", -1));
			return TCL_ERROR;
		}
		tile_scale = n;
		if (use_graphics != GRAPHICS_NONE) {
			tile_width = 2 * tile_scale;
			tile_height = tile_scale;
			if (character_dungeon) do_cmd_redraw();
		}
	} else if (objc != 1) {
		Tcl_WrongNumArgs(ip, 1, objv, "?scale?");
		return TCL_ERROR;
	}
	Tcl_SetObjResult(ip, Tcl_NewIntObj(tile_scale));
	return TCL_OK;
}

/**
 * angband_refont -- measure every pane again from its font, after the Display
 * page has changed one.
 *
 * A pane's cell is its font's, measured when the pane was made; a font
 * configured afterwards would draw its letters at the new size in the old
 * cells.  So each pane is measured again, its cells rebuilt for however many
 * now fit its canvas, the game told the new size, and everything drawn again.
 */
static int objcmd_refont(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int i;

	(void)dummy;
	(void)objv;

	if (objc != 1) {
		Tcl_WrongNumArgs(ip, 1, objv, "");
		return TCL_ERROR;
	}

	for (i = 0; i < td_count; i++) {
		term_data *t = &td[i];
		int cw = 0, ch = 0, w = 0, h = 0, cols, rows;
		Tcl_Obj *q = Tcl_ObjPrintf("list [font measure %s W]"
				" [font metrics %s -linespace]"
				" [winfo width %s] [winfo height %s]",
				t->font, t->font, t->path, t->path);
		Tcl_Obj **e;
		Tcl_Size ne;

		Tcl_IncrRefCount(q);
		if (Tcl_EvalObjEx(ip, q, TCL_EVAL_GLOBAL) == TCL_OK
				&& Tcl_ListObjGetElements(NULL, Tcl_GetObjResult(ip), &ne, &e)
					== TCL_OK && ne == 4) {
			Tcl_GetIntFromObj(NULL, e[0], &cw);
			Tcl_GetIntFromObj(NULL, e[1], &ch);
			Tcl_GetIntFromObj(NULL, e[2], &w);
			Tcl_GetIntFromObj(NULL, e[3], &h);
		}
		Tcl_DecrRefCount(q);
		if (cw <= 0 || ch <= 0) continue;

		cols = w / cw;
		rows = h / ch;
		if (t == &td[0]) {
			if (cols < 80) cols = 80;
			if (rows < 24) rows = 24;
		} else {
			if (cols < 1) cols = 1;
			if (rows < 1) rows = 1;
		}

		t->cw = cw;
		t->ch = ch;
		t->cols = cols;
		t->rows = rows;
		build_cells(t);
		{
			term *old = Term;

			Term_activate(&t->t);
			Term_resize(cols, rows);
			Term_activate(old);
		}
	}

	/* Everything again, in every pane: the map, the sidebar, the lists. */
	do_cmd_redraw();
	Tcl_ResetResult(ip);
	return TCL_OK;
}

/**
 * angband_palette -- the game's colours, by name.
 *
 *    angband_palette                 {name #rrggbb} for each of the game's
 *                                    named colours, in the game's order
 *    angband_palette set {name #rrggbb ...}
 *                                    change some, and draw everything again
 *
 * The game's own colour table, which its pref files' color: lines change
 * too, so a palette from the Display page is the same thing those are --
 * and it colours the map in letters, the sidebar, the messages and the
 * lists, and the numbers the Progress strip reads from the game.  Tiles have
 * their colours in the sheet and keep them.
 */
static int objcmd_palette(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int i;

	(void)dummy;

	if (objc == 1) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

		/* The named ones: Shade, the last basic colour, has no name. */
		for (i = 0; i < BASIC_COLORS && color_table[i].name[0]; i++) {
			Tcl_Obj *row = Tcl_NewListObj(0, NULL);

			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(color_table[i].name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_ObjPrintf("#%02x%02x%02x",
					angband_color_table[i][1], angband_color_table[i][2],
					angband_color_table[i][3]));
			Tcl_ListObjAppendElement(ip, list, row);
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (objc == 3 && streq(Tcl_GetString(objv[1]), "set")) {
		Tcl_Obj **e;
		Tcl_Size ne, k;

		if (Tcl_ListObjGetElements(ip, objv[2], &ne, &e) != TCL_OK)
			return TCL_ERROR;
		if (ne % 2) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"expected name colour pairs", -1));
			return TCL_ERROR;
		}
		/* Every pair checked before any is applied: all or nothing. */
		for (k = 0; k < ne; k += 2) {
			const char *name = Tcl_GetString(e[k]);
			const char *hex = Tcl_GetString(e[k + 1]);
			unsigned int r, g, b;
			char tail;

			for (i = 0; i < BASIC_COLORS && color_table[i].name[0]; i++)
				if (my_stricmp(name, color_table[i].name) == 0) break;
			if (i == BASIC_COLORS || !color_table[i].name[0]) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such colour: %s",
						name));
				return TCL_ERROR;
			}
			if (sscanf(hex, "#%02x%02x%02x%c", &r, &g, &b, &tail) != 3
					|| strlen(hex) != 7) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf(
						"not a colour: %s -- expected #rrggbb", hex));
				return TCL_ERROR;
			}
		}
		for (k = 0; k < ne; k += 2) {
			const char *name = Tcl_GetString(e[k]);
			unsigned int r = 0, g = 0, b = 0;

			for (i = 0; i < BASIC_COLORS && color_table[i].name[0]; i++)
				if (my_stricmp(name, color_table[i].name) == 0) break;
			sscanf(Tcl_GetString(e[k + 1]), "#%02x%02x%02x", &r, &g, &b);
			angband_color_table[i][1] = (uint8_t)r;
			angband_color_table[i][2] = (uint8_t)g;
			angband_color_table[i][3] = (uint8_t)b;
		}
		colours_init();
		if (character_dungeon) do_cmd_redraw();
		return command_done(ip);
	}

	Tcl_WrongNumArgs(ip, 1, objv, "?set {name colour ...}?");
	return TCL_ERROR;
}

/*
 * Sound.  The game plays its own: message.c's sound() signals EVENT_SOUND when
 * the character's use_sound option is on, and sound-core.c plays what
 * sound.prf binds to the message through the module main.c started -- here,
 * on the Mac, the Cocoa front end's, snd-cocoa.m.  This is what the front
 * end needs around that:
 *
 *   angband_sound ready          1 if a module has the game's sounds loaded
 *   angband_sound volume ?0-100? how loud from now on; kept by sound-core.c
 *                                across a restart, so it can be set first
 *   angband_sound play NAME      a message's sound, now, whatever use_sound
 *                                says: the Options window's Try button
 *   angband_sound last           the message whose sound the game last asked
 *                                for, by its MSG_ name, or ""
 */
static const char *sound_msg_names[] = {
	#define MSG(x, s) #x,
	#include "list-message.h"
	#undef MSG
};
static int sound_last = -1;
static int sound_volume = 100;

static void sound_seen(game_event_type type, game_event_data *data,
		void *user)
{
	(void)type;
	(void)user;
	sound_last = data->message.type;
}

static int objcmd_sound(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what = objc > 1 ? Tcl_GetString(objv[1]) : "";

	(void)dummy;

	if (objc == 2 && streq(what, "ready")) {
#ifdef SOUND
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(is_sound_inited()));
#else
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(0));
#endif
		return TCL_OK;
	}
	if ((objc == 2 || objc == 3) && streq(what, "volume")) {
		if (objc == 3) {
			int v;

			if (Tcl_GetIntFromObj(ip, objv[2], &v) != TCL_OK) return TCL_ERROR;
			if (v < 0 || v > 100) {
				Tcl_SetObjResult(ip, Tcl_NewStringObj(
						"the volume runs from 0 to 100", -1));
				return TCL_ERROR;
			}
			sound_volume = v;
#ifdef SOUND
			(void)sound_set_volume(v);
#endif
		}
		Tcl_SetObjResult(ip, Tcl_NewIntObj(sound_volume));
		return TCL_OK;
	}
	if (objc == 3 && streq(what, "play")) {
		int type = message_lookup_by_name(Tcl_GetString(objv[2]));

		if (type <= MSG_GENERIC || type >= MSG_MAX) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("no such message: %s",
					Tcl_GetString(objv[2])));
			return TCL_ERROR;
		}
		event_signal_message(EVENT_SOUND, type, NULL);
		return command_done(ip);
	}
	if (objc == 2 && streq(what, "last")) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj(
				(sound_last > MSG_GENERIC && sound_last < MSG_MAX)
				? sound_msg_names[sound_last] : "", -1));
		return TCL_OK;
	}

	Tcl_WrongNumArgs(ip, 1, objv, "ready | volume ?0-100? | play NAME | last");
	return TCL_ERROR;
}

/*
 * Keymaps, for the Keymaps window (keymap.tcl).  Everything is in 4.2's own
 * text form, what keymap-act: and keymap-input: lines in a pref file hold, so
 * a key bound here is bound for the text front end too and the other way
 * round.  Always the keyset in use: a keymap belongs to the original keys or
 * to the roguelike ones.
 *
 *   angband_keymap list                {trigger action user} for each keymap,
 *                                      the game's own as well as the player's
 *   angband_keymap trigger KEYSYM STATE CHAR
 *                                      a key as Tk reports it, as a trigger --
 *                                      tk_keypress(), as typing it would send
 *                                      -- or "" for a key the game ignores
 *   angband_keymap readable TEXT       the keys of TEXT, as a person reads them
 *   angband_keymap command TEXT        what the first key of TEXT does in the
 *                                      keyset, by the command's name, or ""
 *   angband_keymap action GROUP INDEX  the key that runs a command, as an
 *                                      action; angband_commands' numbering
 *   angband_keymap set TRIGGER ACTION  bind, as the player's own
 *   angband_keymap remove TRIGGER      unbind; 1 if there was a keymap
 *   angband_keymap save                write the player's keymaps into the
 *                                      user folder's user.prf, which every
 *                                      character loads, under the title the
 *                                      game's options screen uses
 *
 * Each key's text is the trigger form, backslash doubled.  keymap_dump()
 * writes actions without the doubling, which its parser then reads as an
 * escape; built a key at a time, the text here round-trips exactly.
 */
static int keymap_mode_now(void)
{
	return (player && OPT(player, rogue_like_commands))
		? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
}

static void keymap_key_text(char *buf, size_t len, struct keypress kp)
{
	struct keypress one[2] = { KEYPRESS_NULL, KEYPRESS_NULL };

	one[0] = kp;
	one[0].type = EVT_KBRD;
	buf[0] = 0;
	keypress_to_text(buf, len, one, true);
}

static void keymap_text(char *buf, size_t len, const struct keypress *keys)
{
	size_t n;

	buf[0] = 0;
	for (n = 0; keys[n].type == EVT_KBRD; n++) {
		char one[64];

		keymap_key_text(one, sizeof(one), keys[n]);
		my_strcat(buf, one, len);
	}
}

struct keymap_listing {
	Tcl_Interp *ip;
	Tcl_Obj *list;
};

static void keymap_list_one(struct keypress trigger,
		const struct keypress *actions, bool user, void *data)
{
	struct keymap_listing *l = data;
	Tcl_Obj *row = Tcl_NewListObj(0, NULL);
	char buf[1024];

	keymap_key_text(buf, sizeof(buf), trigger);
	Tcl_ListObjAppendElement(l->ip, row, Tcl_NewStringObj(buf, -1));
	keymap_text(buf, sizeof(buf), actions);
	Tcl_ListObjAppendElement(l->ip, row, Tcl_NewStringObj(buf, -1));
	Tcl_ListObjAppendElement(l->ip, row, Tcl_NewBooleanObj(user));
	Tcl_ListObjAppendElement(l->ip, l->list, row);
}

/* The keys of a text, and how many; 0 for a text that is not keys. */
static size_t keymap_parse(const char *text, struct keypress *keys, size_t max)
{
	size_t n = 0;

	keypress_from_text(keys, max, text);
	while (n < max && keys[n].type == EVT_KBRD) n++;
	return n;
}

static int objcmd_keymap(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what = objc > 1 ? Tcl_GetString(objv[1]) : "";
	int mode = keymap_mode_now();
	struct keypress keys[KEYMAP_ACTION_MAX + 2];
	char buf[1024];

	(void)dummy;

	if (objc == 2 && streq(what, "list")) {
		struct keymap_listing l = { ip, Tcl_NewListObj(0, NULL) };

		keymap_visit(mode, keymap_list_one, &l);
		Tcl_SetObjResult(ip, l.list);
		return TCL_OK;
	}

	if (objc == 5 && streq(what, "trigger")) {
		int keysym, state;
		struct keypress kp;

		if (Tcl_GetIntFromObj(ip, objv[2], &keysym) != TCL_OK) return TCL_ERROR;
		if (Tcl_GetIntFromObj(ip, objv[3], &state) != TCL_OK) return TCL_ERROR;
		buf[0] = 0;
		if (tk_keypress(keysym, state, Tcl_GetString(objv[4]), &kp))
			keymap_key_text(buf, sizeof(buf), kp);
		Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, -1));
		return TCL_OK;
	}

	if (objc == 3 && streq(what, "readable")) {
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);
		size_t i, n = keymap_parse(Tcl_GetString(objv[2]), keys,
				N_ELEMENTS(keys));

		for (i = 0; i < n; i++) {
			buf[0] = 0;
			keypress_to_readable(buf, sizeof(buf), keys[i]);
			Tcl_ListObjAppendElement(ip, list, Tcl_NewStringObj(buf, -1));
		}
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}

	if (objc == 3 && streq(what, "command")) {
		const char *desc = "";
		size_t g, i;

		if (keymap_parse(Tcl_GetString(objv[2]), keys, N_ELEMENTS(keys))
				&& !keys[0].mods) {
			if (keys[0].code == KC_ENTER) desc = "Show the command menu";
			/* The top-level groups, which are the ones keys reach. */
			for (g = 0; cmds_all[g].name && !desc[0]; g++) {
				if (cmds_all[g].keymap) continue;
				for (i = 0; i < cmds_all[g].len; i++) {
					const struct cmd_info *c = &cmds_all[g].list[i];
					keycode_t k = c->key[mode] ? c->key[mode] : c->key[0];

					if (k && k == keys[0].code) {
						desc = c->desc;
						break;
					}
				}
			}
		}
		Tcl_SetObjResult(ip, Tcl_NewStringObj(desc, -1));
		return TCL_OK;
	}

	if (objc == 4 && streq(what, "action")) {
		int g, i, ng = 0;
		const struct cmd_info *c;
		struct keypress kp = KEYPRESS_NULL;

		if (Tcl_GetIntFromObj(ip, objv[2], &g) != TCL_OK) return TCL_ERROR;
		if (Tcl_GetIntFromObj(ip, objv[3], &i) != TCL_OK) return TCL_ERROR;
		while (cmds_all[ng].name) ng++;
		if (g < 0 || g >= ng || i < 0 || (size_t)i >= cmds_all[g].len) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj("no such command", -1));
			return TCL_ERROR;
		}
		c = &cmds_all[g].list[i];
		/* As angband_commands finds it: the keyset's key, or the hook's. */
		kp.code = c->cmd ? cmd_lookup_key(c->cmd, mode) : 0;
		if (!kp.code) kp.code = c->key[mode] ? c->key[mode] : c->key[0];
		buf[0] = 0;
		if (kp.code) keymap_key_text(buf, sizeof(buf), kp);
		Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, -1));
		return TCL_OK;
	}

	if (objc == 4 && streq(what, "set")) {
		struct keypress trigger[4];
		size_t n;

		/* Room for more than one, or a second key would be cut off unseen. */
		if (keymap_parse(Tcl_GetString(objv[2]), trigger, 4) != 1) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("not one key: %s",
					Tcl_GetString(objv[2])));
			return TCL_ERROR;
		}
		n = keymap_parse(Tcl_GetString(objv[3]), keys, N_ELEMENTS(keys));
		if (n < 1 || n > KEYMAP_ACTION_MAX) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf(
					"an action is 1 to %d keys", KEYMAP_ACTION_MAX));
			return TCL_ERROR;
		}
		keys[n].type = EVT_NONE;
		keymap_add(mode, trigger[0], keys, true);
		return TCL_OK;
	}

	if (objc == 3 && streq(what, "remove")) {
		struct keypress trigger[4];

		if (keymap_parse(Tcl_GetString(objv[2]), trigger, 4) != 1) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("not one key: %s",
					Tcl_GetString(objv[2])));
			return TCL_ERROR;
		}
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(keymap_remove(mode,
				trigger[0])));
		return TCL_OK;
	}

	if (objc == 2 && streq(what, "save")) {
		if (!player) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"there is no character yet", -1));
			return TCL_ERROR;
		}
		path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "user.prf");
		if (!prefs_save(buf, keymap_dump, "Dump keymaps")) {
			Tcl_SetObjResult(ip, Tcl_ObjPrintf("could not write %s", buf));
			return TCL_ERROR;
		}
		Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, -1));
		return TCL_OK;
	}

	Tcl_WrongNumArgs(ip, 1, objv, "list | trigger KEYSYM STATE CHAR | "
			"readable TEXT | command TEXT | action GROUP INDEX | "
			"set TRIGGER ACTION | remove TRIGGER | save");
	return TCL_ERROR;
}

/*
 * The keyboard, for the Keymaps window's picture of it.
 *
 *   angband_keyboard type               ansi, iso or jis
 *   angband_keyboard char VK SHIFTED    what the key with macOS virtual key
 *                                       code VK types on the layout in use
 *
 * Off the Mac there is no asking, and the window draws US keycaps: type is
 * ansi and char is "".
 */
static int objcmd_keyboard(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what = objc > 1 ? Tcl_GetString(objv[1]) : "";

	(void)dummy;

	if (objc == 2 && streq(what, "type")) {
#ifdef __APPLE__
		Tcl_SetObjResult(ip, Tcl_NewStringObj(tcl_keyboard_type(), -1));
#else
		Tcl_SetObjResult(ip, Tcl_NewStringObj("ansi", -1));
#endif
		return TCL_OK;
	}
	if (objc == 4 && streq(what, "char")) {
		int vk, shifted;
		char buf[16] = "";

		if (Tcl_GetIntFromObj(ip, objv[2], &vk) != TCL_OK) return TCL_ERROR;
		/* An int, 0 or 1: Tcl 9's Tcl_GetBooleanFromObj is a GNU
		 * statement expression, which -pedantic refuses. */
		if (Tcl_GetIntFromObj(ip, objv[3], &shifted) != TCL_OK)
			return TCL_ERROR;
#ifdef __APPLE__
		tcl_keyboard_char(vk, shifted, buf, sizeof(buf));
#else
		(void)vk;
#endif
		Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, -1));
		return TCL_OK;
	}

	Tcl_WrongNumArgs(ip, 1, objv, "type | char VK SHIFTED");
	return TCL_ERROR;
}

/*
 * The player's own music (music.tcl), one track at a time.
 *
 *   angband_music play PATH VOLUME   from the start, at 0 to 100; 1 if it
 *                                    plays, 0 if the file cannot be played
 *   angband_music stop | pause | resume
 *   angband_music volume 0-100       the track playing, from now
 *   angband_music state              {stopped|paused|playing|ended position
 *                                    length}, in seconds
 *
 * Only where the Cocoa sound module is (music.m); elsewhere play is always 0
 * and state stopped, and the Sound page says there is no music here.
 */
static int objcmd_music(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	const char *what = objc > 1 ? Tcl_GetString(objv[1]) : "";
	int v = 0;

	(void)dummy;

	if ((objc == 4 && streq(what, "play")) || (objc == 3 && streq(what, "volume"))) {
		if (Tcl_GetIntFromObj(ip, objv[objc - 1], &v) != TCL_OK) return TCL_ERROR;
		if (v < 0 || v > 100) {
			Tcl_SetObjResult(ip, Tcl_NewStringObj(
					"the volume runs from 0 to 100", -1));
			return TCL_ERROR;
		}
	}
	if (objc == 4 && streq(what, "play")) {
#ifdef SOUND_COCOA
		Tcl_SetObjResult(ip, Tcl_NewIntObj(tcl_music_play(
				Tcl_GetString(objv[2]), v / 100.0)));
#else
		Tcl_SetObjResult(ip, Tcl_NewIntObj(0));
#endif
		return TCL_OK;
	}
	if (objc == 3 && streq(what, "volume")) {
#ifdef SOUND_COCOA
		tcl_music_volume(v / 100.0);
#endif
		return TCL_OK;
	}
	if (objc == 2 && (streq(what, "stop") || streq(what, "pause")
			|| streq(what, "resume"))) {
#ifdef SOUND_COCOA
		if (streq(what, "stop")) tcl_music_stop();
		else tcl_music_pause(streq(what, "pause"));
#endif
		return TCL_OK;
	}
	if (objc == 2 && streq(what, "state")) {
		double position = 0, length = 0;
		const char *state = "stopped";
		Tcl_Obj *list = Tcl_NewListObj(0, NULL);

#ifdef SOUND_COCOA
		state = tcl_music_state(&position, &length);
#endif
		Tcl_ListObjAppendElement(ip, list, Tcl_NewStringObj(state, -1));
		Tcl_ListObjAppendElement(ip, list, Tcl_NewDoubleObj(position));
		Tcl_ListObjAppendElement(ip, list, Tcl_NewDoubleObj(length));
		Tcl_SetObjResult(ip, list);
		return TCL_OK;
	}
	if (objc == 2 && streq(what, "here")) {
#ifdef SOUND_COCOA
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(1));
#else
		Tcl_SetObjResult(ip, Tcl_NewBooleanObj(0));
#endif
		return TCL_OK;
	}

	Tcl_WrongNumArgs(ip, 1, objv, "play PATH VOLUME | stop | pause | resume | "
			"volume 0-100 | state | here");
	return TCL_ERROR;
}

static bool tcl_get_int(const char *name, int *out);

/**
 * Find the tile sets.  Before main.tcl, which lists them in its menus.
 */
static bool graphics_modes_known = false;

static void graphics_modes_init(void)
{
	graphics_modes_known = init_graphics_modes();
	if (!graphics_modes_known)
		plog("Tcl/Tk: no graphics modes; running without tiles.");
}

/**
 * Load the set main.tcl asked for, and at the size it asked for.
 *
 * Those are the player's choices from the Display page, kept in the front
 * end's settings and read by main.tcl into angband(graphics) and
 * angband(tilescale).  Without them it is mode 7, the Neon set, which is the
 * one this project uses -- it is generated from text shapes rather than
 * drawn, so the sheet is never edited by hand.  Falling back to Neon and then
 * to no graphics rather than failing is deliberate: a missing tile sheet
 * should cost you tiles, not the game.
 */
static void graphics_init(void)
{
	graphics_mode *mode = NULL;
	int want = 7, scale = 1;

	if (tcl_get_int("tilescale", &scale) && scale >= 1 && scale <= 4)
		tile_scale = scale;
	if (!graphics_modes_known) return;
	tcl_get_int("graphics", &want);

	if (want == GRAPHICS_NONE) {
		graphics_load(NULL);
		return;
	}
	mode = get_graphics_mode((uint8_t)want);
	if (!mode || !graphics_load(mode)) {
		mode = get_graphics_mode(7);
		if (!mode || !graphics_load(mode)) {
			plog("Tcl/Tk: running without tiles.");
			graphics_load(NULL);
		}
	}
}

/**
 * Build the canvas and its cells, and wire one term to it.
 *
 * Returns false if the window could not be built.  The caller must not ignore
 * that: without Term_activate there is no Term at all, and the first thing the
 * game does is Term_clear, which dereferences it.  That is a segfault in
 * init_angband with nothing on screen and nothing on stderr -- Tk closes
 * stderr on a bundled application -- so a silent failure here is about the
 * least debuggable outcome available.
 */
/**
 * Create one canvas text item per cell, plus the cursor rectangle.
 *
 * Called again on every resize.  The items are destroyed and remade rather
 * than moved: at 80x24 that is 1,920 items and it happens only when the player
 * drags the window edge, so the simpler code wins over the cleverer one.
 */
static void build_cells(term_data *td)
{
	int x, y;

	{
		Tcl_Obj *del = Tcl_ObjPrintf("%s delete all", td->path);

		Tcl_IncrRefCount(del);
		Tcl_EvalObjEx(interp, del, TCL_EVAL_GLOBAL);
		Tcl_DecrRefCount(del);
	}

	if (td->item) mem_free(td->item);
	td->item = mem_zalloc(td->cols * td->rows * sizeof(int));

	/*
	 * The last build's photo goes with its canvas item.  It used to be left
	 * behind: deleting the item does not delete the image, so every resize
	 * kept a photo the size of the pane -- megabytes for the map, and a drag
	 * resizes many times.
	 */
	if (td->screen_name[0]) {
		Tcl_Obj *gone = Tcl_ObjPrintf("image delete %s", td->screen_name);

		Tcl_IncrRefCount(gone);
		Tcl_EvalObjEx(interp, gone, TCL_EVAL_GLOBAL);
		Tcl_DecrRefCount(gone);
		td->screen_name[0] = '\0';
		td->screen = NULL;
	}

	/*
	 * The tile layer: one photo the size of the grid, shown by one canvas
	 * image item, created before the text items so that it sits underneath
	 * all of them.  Tiles are blitted into the photo rather than becoming
	 * canvas items of their own -- 1,920 image items would be a different
	 * proposition from 1,920 text items.
	 */
	{
		Tcl_Obj *mk = Tcl_ObjPrintf("image create photo -width %d -height %d",
				td->cols * td->cw, td->rows * td->ch);

		Tcl_IncrRefCount(mk);
		if (Tcl_EvalObjEx(interp, mk, TCL_EVAL_GLOBAL) == TCL_OK) {
			/* Copy the name out: the next evaluation replaces the result. */
			char name[64];
			Tcl_Obj *item;

			my_strcpy(name, Tcl_GetStringResult(interp), sizeof(name));
			td->screen = Tk_FindPhoto(interp, name);
			my_strcpy(td->screen_name, name, sizeof(td->screen_name));

			item = Tcl_ObjPrintf("%s create image 0 0 -anchor nw -image %s",
					td->path, name);
			Tcl_IncrRefCount(item);
			if (Tcl_EvalObjEx(interp, item, TCL_EVAL_GLOBAL) == TCL_OK) {
				Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(interp),
						&td->image_item);
			}
			Tcl_DecrRefCount(item);
		}
		Tcl_DecrRefCount(mk);
	}

	for (y = 0; y < td->rows; y++) {
		for (x = 0; x < td->cols; x++) {
			Tcl_Obj *mk = Tcl_ObjPrintf(
					"%s create text %d %d -anchor nw -font %s"
					" -fill white -text { }",
					td->path, x * td->cw, y * td->ch, td->font);

			Tcl_IncrRefCount(mk);
			if (Tcl_EvalObjEx(interp, mk, TCL_EVAL_GLOBAL) == TCL_OK) {
				Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(interp),
						&td->item[y * td->cols + x]);
			}
			Tcl_DecrRefCount(mk);
		}
	}

	/* The cursor: a rectangle, hidden until the game places it. */
	{
		Tcl_Obj *cur = Tcl_ObjPrintf(
				"%s create rectangle 0 0 0 0 -outline yellow -width 2"
				" -state hidden", td->path);

		Tcl_IncrRefCount(cur);
		if (Tcl_EvalObjEx(interp, cur, TCL_EVAL_GLOBAL) == TCL_OK) {
			Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(interp),
					&td->cursor_item);
		}
		Tcl_DecrRefCount(cur);
	}
}

/**
 * angband_resize -- the canvas changed size, so the term should too.
 *
 * main.tcl works out how many whole cells now fit and calls this; doing the
 * arithmetic there rather than here keeps the cell size, which the script
 * already owns, in one place.
 *
 * The game has a floor of 80x24 (ui-init.c warns below it), so anything
 * smaller is ignored rather than passed on -- the window can be dragged
 * smaller, it just stops giving the game less than it can use.
 */
static int objcmd_resize(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	term_data *t;
	int which, cols, rows;

	(void)dummy;

	if (objc != 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "term cols rows");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &which) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[2], &cols) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[3], &rows) != TCL_OK) return TCL_ERROR;

	if (which < 0 || which >= td_count) return TCL_OK;
	t = &td[which];

	/*
	 * The map has a floor of 80x24 -- ui-init.c warns below it -- but a
	 * subwindow has none: a messages pane three rows high is a reasonable
	 * thing to want, and the game is happy to draw into it.
	 */
	if (which == 0) {
		if (cols < 80) cols = 80;
		if (rows < 24) rows = 24;
	} else {
		if (cols < 1) cols = 1;
		if (rows < 1) rows = 1;
	}
	if (cols == t->cols && rows == t->rows) return TCL_OK;

	t->cols = cols;
	t->rows = rows;
	build_cells(t);

	/*
	 * Tell the game last, and about the right term: Term_resize works on
	 * whichever is active, so this has to be bracketed rather than just
	 * called.  The items it redraws into have to exist first, which is why
	 * build_cells comes before it.
	 */
	{
		term *old = Term;

		Term_activate(&t->t);
		Term_resize(cols, rows);
		Term_activate(old);
	}

	/*
	 * And a subwindow drawn again at its new size.  Nothing else asks for
	 * that: Term_resize() leaves its EVT_RESIZE in the resized term's own
	 * key queue, which for the map is the game's -- that is how the map is
	 * redrawn -- and for a subwindow is read by nobody.  So a pane dragged
	 * to a new size stayed blank until what it shows next changed; reported
	 * from play with the minimap.  Asked for through the game's own redraw
	 * flags, for what this pane shows and not the map, which would draw
	 * over a store's screen; and then the end of an update, which is when
	 * the minimap and the overhead view draw themselves.
	 */
	if (which > 0 && player && player->upkeep && character_generated) {
		uint32_t flags = 0;
		int j;

		for (j = 0; j < ANGBAND_TERM_MAX; j++)
			if (angband_term[j] == &t->t) flags = window_flag[j];

		/* The messages pane is drawn on EVENT_STATE, not EVENT_MESSAGE. */
		if (flags & PW_MESSAGE) player->upkeep->redraw |= PR_STATE;
		if (flags & PW_MONSTER) player->upkeep->redraw |= PR_MONSTER;
		if (flags & PW_OBJECT) player->upkeep->redraw |= PR_OBJECT;
		if (flags & PW_MONLIST) player->upkeep->redraw |= PR_MONLIST;
		if (flags & PW_ITEMLIST) player->upkeep->redraw |= PR_ITEMLIST;
		if (flags & (PW_INVEN | PW_EQUIP))
			player->upkeep->redraw |= PR_INVEN | PR_EQUIP;
		redraw_stuff(player);
		if ((flags & (PW_MAP | PW_OVERHEAD)) && character_dungeon)
			event_signal(EVENT_END);
	}

	return command_done(ip);
}

/**
 * Read one integer out of the script's angband() array.
 */
static bool tcl_get_int(const char *name, int *out)
{
	Tcl_Obj *v = Tcl_GetVar2Ex(interp, "angband", name, TCL_GLOBAL_ONLY);

	return (v && Tcl_GetIntFromObj(interp, v, out) == TCL_OK);
}

static bool term_data_link(term_data *t, int which, const char *path,
		const char *font)
{
	term *tt = &t->t;
	Tcl_Obj *q;

	my_strcpy(t->path, path, sizeof(t->path));
	my_strcpy(t->font, font, sizeof(t->font));

	/*
	 * A pane's cell size is its font's, measured rather than assumed.  It is
	 * per pane and not global because the minimap wants small cells: more of
	 * them fit, so more of the level fits, and the tiles drawn into them come
	 * out correspondingly smaller.
	 */
	q = Tcl_ObjPrintf("list [font measure %s W] [font metrics %s -linespace]",
			font, font);
	Tcl_IncrRefCount(q);
	if (Tcl_EvalObjEx(interp, q, TCL_EVAL_GLOBAL) == TCL_OK) {
		Tcl_Obj **e;
		Tcl_Size ne;

		if (Tcl_ListObjGetElements(NULL, Tcl_GetObjResult(interp), &ne, &e)
				== TCL_OK && ne == 2) {
			Tcl_GetIntFromObj(NULL, e[0], &t->cw);
			Tcl_GetIntFromObj(NULL, e[1], &t->ch);
		}
	}
	Tcl_DecrRefCount(q);

	if (t->cw <= 0 || t->ch <= 0) {
		plog_fmt("Tcl/Tk: font %s gives no usable cell size.", font);
		return false;
	}

	/*
	 * How many cells fit, from the size the layout gave this canvas.  The
	 * script has already run `update`, so these are real numbers rather than
	 * the 1x1 an unmapped widget reports.
	 */
	{
		Tcl_Obj *q = Tcl_ObjPrintf(
				"list [winfo width %s] [winfo height %s]", path, path);
		int w = 0, h = 0;
		Tcl_Obj **e;
		Tcl_Size n;

		Tcl_IncrRefCount(q);
		if (Tcl_EvalObjEx(interp, q, TCL_EVAL_GLOBAL) == TCL_OK
				&& Tcl_ListObjGetElements(NULL, Tcl_GetObjResult(interp), &n, &e)
					== TCL_OK && n == 2) {
			Tcl_GetIntFromObj(NULL, e[0], &w);
			Tcl_GetIntFromObj(NULL, e[1], &h);
		}
		Tcl_DecrRefCount(q);

		t->cols = (t->cw > 0) ? w / t->cw : 0;
		t->rows = (t->ch > 0) ? h / t->ch : 0;
	}

	if (which == 0) {
		if (t->cols < 80) t->cols = 80;
		if (t->rows < 24) t->rows = 24;
	} else {
		if (t->cols < 1) t->cols = 1;
		if (t->rows < 1) t->rows = 1;
	}

	build_cells(t);

	term_init(tt, t->cols, t->rows, 256);

	tt->soft_cursor = true;
	/* Only attribute/character pairs with the high bit set are tiles. */
	tt->higher_pict = true;

	tt->init_hook = Term_init_tcl;
	tt->nuke_hook = Term_nuke_tcl;
	tt->xtra_hook = Term_xtra_tcl;
	tt->curs_hook = Term_curs_tcl;
	tt->wipe_hook = Term_wipe_tcl;
	tt->text_hook = Term_text_tcl;
	tt->pict_hook = Term_pict_tcl;

	tt->data = t;

	angband_term[which] = tt;

	return true;
}

/**
 * Build the window, then a term for each canvas the script laid out.
 */
static bool terms_init(void)
{
	Tcl_Obj *list, **elem;
	Tcl_Size n;
	int cw, ch, i;

	/*
	 * The tile sets have to be known before the script runs: main.tcl builds
	 * a menu of them, and asking for a list that has not been read yet gets
	 * an empty menu rather than an error.
	 */
	colours_init();
	graphics_modes_init();

	if (!tcl_source("main.tcl")) return false;

	/* The set the player chose, which main.tcl has read from the settings. */
	graphics_init();
	Tcl_SetVar2Ex(interp, "angband", "tileset", Tcl_NewIntObj(
			current_graphics_mode ? current_graphics_mode->grafID : 0),
			TCL_GLOBAL_ONLY);

	if (!tcl_get_int("cellw", &cw) || !tcl_get_int("cellh", &ch)
			|| cw <= 0 || ch <= 0) {
		plog("Tcl/Tk: main.tcl did not report a usable cell size.");
		return false;
	}

	list = Tcl_GetVar2Ex(interp, "angband", "terms", TCL_GLOBAL_ONLY);
	if (!list || Tcl_ListObjGetElements(interp, list, &n, &elem) != TCL_OK
			|| n < 1) {
		plog("Tcl/Tk: main.tcl did not lay out any terms.");
		return false;
	}
	if (n > ANGBAND_TERM_MAX) n = ANGBAND_TERM_MAX;

	/* The words of the drawing loop that never change. */
	cfg_itemconfigure = Tcl_NewStringObj("itemconfigure", -1);
	cfg_dash_text = Tcl_NewStringObj("-text", -1);
	cfg_dash_fill = Tcl_NewStringObj("-fill", -1);
	Tcl_IncrRefCount(cfg_itemconfigure);
	Tcl_IncrRefCount(cfg_dash_text);
	Tcl_IncrRefCount(cfg_dash_fill);

	td_count = (int)n;
	for (i = 0; i < td_count; i++) {
		Tcl_Obj **pair;
		Tcl_Size np;

		if (Tcl_ListObjGetElements(interp, elem[i], &np, &pair) != TCL_OK
				|| np != 3) {
			plog("Tcl/Tk: angband(terms) entries must be {canvas role font}.");
			return false;
		}

		pane_index[i] = role_to_index(Tcl_GetString(pair[1]));
		if (pane_index[i] < 0) return false;
		want_flag[pane_index[i]] = role_to_flag(Tcl_GetString(pair[1]));

		if (!term_data_link(&td[i], pane_index[i], Tcl_GetString(pair[0]),
				Tcl_GetString(pair[2])))
			return false;
	}

	/*
	 * Now that every term exists, ask the layout what size each pane actually
	 * ended up.  The sizes read above were taken before any term existed, so
	 * the <Configure> events the paned window fired while it settled had
	 * nowhere to go -- without this pass the map term keeps whatever it
	 * measured first and draws part of itself off the edge of its own pane.
	 */
	Tcl_Eval(interp, "angband_resize_now");

	/* Start the animation clock now that there is something to animate. */
	Tcl_CreateTimerHandler(ANIMATION_MS, animation_tick, NULL);

	/*
	 * The map is the active term when the game starts, and stays the one the
	 * player is looking at.  Activating it last is what makes that true.
	 */
	Term_activate(&td[0].t);

	/*
	 * The scripted-session hook, from section 9 of the Phase 3 plan.
	 *
	 * A script named by ZANGBAND_TCL_SCRIPT is sourced once everything above
	 * exists -- the window, the terms, the commands, the event handlers -- so
	 * a test can drive the game from outside without a keyboard, and quit at
	 * the end with a status the runner can read.  The variable is not set in
	 * ordinary play, and a script that fails is a hard error rather than a
	 * warning, because a test that silently did not run is worse than none.
	 */
	{
		const char *script = getenv("ZANGBAND_TCL_SCRIPT");

		if (script && *script) {
			if (Tcl_EvalFile(interp, script) != TCL_OK) {
				plog_fmt("Tcl/Tk: %s: %s", script,
						Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
				return false;
			}
		}
	}

	return true;
}

/**
 * angband_quit -- the Tcl side of closing the window, and of Quit.
 *
 * This used to call quit() and nothing else, on the understanding that quit()
 * unwinds through the game's own shutdown and that is what saves the
 * character.  It does not: quit() tears down, and saving is the game's to do
 * before that.  So closing the window threw away everything since the last
 * save, and a new character that had not yet been saved was lost outright.
 * Reported from play as a character that could not be continued.
 *
 * Now a closing window is what it is to the SDL2 front end: the game's own
 * disconnect.  With terms_disconnecting set, every wait for a key returns at
 * once; prompts and menus take that as Escape, so the game backs out of
 * whatever it was asking -- a store, an item prompt, a -more- -- to its main
 * loop, which saves the character and ends the game, and close_game() leaves
 * out its "Press Return" for a display that is going away.  A character who
 * has just died is the same: the death screen takes the disconnect as leaving
 * it, and the game goes on to record the death -- the savefile the next start
 * offers its quick start from.  Before there is a character -- the splash,
 * character creation -- there is nothing to save, and it quits as before.
 *
 * Note the signature: this is Tcl_ObjCmdProc2, which takes Tcl_Size.  Tcl 9
 * keeps the older int-based Tcl_ObjCmdProc and Tcl_CreateObjCommand for
 * compatibility, so a command written the modern way has to be registered with
 * Tcl_CreateObjCommand2 or the prototypes quietly disagree.
 */
static int objcmd_quit(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	(void)dummy;
	(void)ip;
	(void)objc;
	(void)objv;

	if (!player || !player->upkeep || !character_generated) {
		quit(NULL);
		return TCL_OK;
	}

	terms_disconnecting = 1;
	return TCL_OK;
}

const char help_tcl[] = "Tcl/Tk front end";

/**
 * Where plog() goes on this front end.
 *
 * plog's default is a labelled fprintf to stderr, and for a bundled
 * application Tk has already pointed stderr at /dev/null -- so every one of
 * this file's plog and plog_fmt calls was being written to nowhere.  Two of
 * them matter more than the rest: tcl_source() reports a front end script that
 * failed to source, and an interpreter that could not be created or
 * initialised reports that, and in both cases the game then carries on with no
 * user interface and nothing said about why.
 *
 * lib/tcl/errorInfo.tcl already exists to solve exactly this, so this routes
 * into it rather than opening a second channel: the sink buffers records made
 * before its window exists, which is the case that matters here, since almost
 * everything plog has to say happens during startup.
 *
 * stderr is still written.  A terminal run has a real one, and the sink's own
 * window is no use to somebody reading a log after the fact.  This is the same
 * argument the <<Angband_*>> path above makes for keeping both.
 *
 * Three guards, none of them theoretical:
 *
 *   - The sink is a script, sourced by lib/tcl/main.tcl, so for the whole of
 *     startup before that line it does not exist.  Tcl_FindCommand is the
 *     question "is it there yet", and the answer is no for Tcl_Init and
 *     Tk_Init failures, which keep the old stderr-only behaviour by default.
 *
 *   - plog is called during shutdown, including from quit(), so the
 *     interpreter may be gone or going.
 *
 *   - If the sink itself fails, Tcl will report that error, and reporting it
 *     must not re-enter here and recurse.  stderr already has the message.
 *
 * The interpreter's result is saved and restored around the call.  Callers
 * here log and then return a failure the caller above reads out of the
 * interpreter, so logging must not be able to change what they read.
 */
static void hook_plog(const char *str)
{
	static bool inside = false;

	/* Say it on stderr whatever happens, exactly as the default would. */
	(void)fprintf(stderr, "%s: %s\n", argv0 ? argv0 : "?", str ? str : "");

	if (!str || !interp || Tcl_InterpDeleted(interp) || inside) return;

	/* Before lib/tcl/main.tcl has sourced the sink there is nowhere to put it. */
	if (!Tcl_FindCommand(interp, "::errorsink::record", NULL, TCL_GLOBAL_ONLY))
		return;

	inside = true;
	{
		Tcl_InterpState  state = Tcl_SaveInterpState(interp, TCL_OK);
		Tcl_Obj         *av[3];
		int              i;

		av[0] = Tcl_NewStringObj("::errorsink::record", -1);
		av[1] = Tcl_NewStringObj("plog", -1);
		av[2] = Tcl_NewStringObj(str, -1);
		for (i = 0; i < 3; i++) Tcl_IncrRefCount(av[i]);

		(void)Tcl_EvalObjv(interp, 3, av, TCL_EVAL_GLOBAL);

		for (i = 0; i < 3; i++) Tcl_DecrRefCount(av[i]);
		(void)Tcl_RestoreInterpState(interp, state);
	}
	inside = false;
}

/**
 * Bring up the interpreter and the window.
 */
errr init_tcl(int argc, char **argv)
{
	(void)argc;

	/*
	 * Before anything that can fail.  The hook falls back to stderr on its
	 * own until the sink exists, so installing it this early costs nothing
	 * and means no failure between here and there is routed by accident.
	 */
	plog_aux = hook_plog;

	/*
	 * Tcl insists on knowing where the executable is before an interpreter
	 * exists -- it is how zipfs and the library paths are resolved later.
	 */
	Tcl_FindExecutable(argv[0]);
	set_script_library_paths();

	interp = Tcl_CreateInterp();
	if (!interp) {
		plog("Tcl/Tk: could not create an interpreter.");
		return 1;
	}

	if (Tcl_Init(interp) != TCL_OK) {
		plog_fmt("Tcl/Tk: Tcl_Init failed: %s", Tcl_GetStringResult(interp));
		return 1;
	}

	/*
	 * Declare the Tk we are already linked against, before initialising it, so
	 * that `package require Tk` is satisfied by this copy and never goes
	 * looking for one to dlopen.
	 *
	 * It is worth doing because Tcl's auto_path keeps the prefix the toolchain
	 * was *built* in: it is compiled into libtcl as TCL_PACKAGE_PATH and no
	 * environment variable removes it.  A bundled application can therefore
	 * still see the developer's tcltk/local/lib and, if anything asked for the
	 * package, would load a second Tk from there -- working on the machine that
	 * built it and nowhere else.  This closes that off rather than relying on
	 * nothing ever asking.
	 */
	Tcl_StaticLibrary(interp, "Tk", Tk_Init, Tk_SafeInit);

	/*
	 * Tell Tcl what our startup script is, before Tk starts.  This is not
	 * bookkeeping: TkpInit on macOS decides whether to open a Tcl console
	 * window by asking whether stdin is "nullish" and no startup script is
	 * set (tkMacOSXInit.c, around the Tk_CreateConsoleWindow call).  A
	 * bundled application launched from the Finder satisfies both, so Tk
	 * builds a console -- and building it inside Tk_Init is where the
	 * application hangs, in TkpInit's own event loop, before it has ever
	 * returned to us.  The symptom is an application that starts, shows
	 * nothing, and never exits.
	 *
	 * Naming the script we are about to source is both true and sufficient:
	 * nothing runs it on our behalf, because we are not Tcl_Main.
	 */
	{
		char path[1024];
		Tcl_Obj *startup;

		path_build(path, sizeof(path), ANGBAND_DIR_TCL, "main.tcl");
		startup = Tcl_NewStringObj(path, -1);
		Tcl_IncrRefCount(startup);
		Tcl_SetStartupScript(startup, NULL);
		Tcl_DecrRefCount(startup);
	}

#ifdef __APPLE__
	/*
	 * The menus' keys are the characters the game reads, not shortcuts for
	 * AppKit to move about the keyboard; it has to be said before Tk_Init,
	 * which is where the application finishes launching and is asked.
	 */
	tcl_menu_keys_literal();
#endif

	if (Tk_Init(interp) != TCL_OK) {
		plog_fmt("Tcl/Tk: Tk_Init failed: %s", Tcl_GetStringResult(interp));
		return 1;
	}

	mainwin = Tk_MainWindow(interp);
	if (!mainwin) {
		plog("Tcl/Tk: Tk started but there is no main window.");
		return 1;
	}

	Tcl_CreateObjCommand2(interp, "angband_quit", objcmd_quit, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_key", objcmd_key, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_resize", objcmd_resize, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_tilesets", objcmd_tilesets, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_tile", objcmd_tile, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_tileset", objcmd_tileset, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_commands", objcmd_commands, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_command", objcmd_command, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_push", objcmd_push, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_hook", objcmd_hook, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_ask", objcmd_ask, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_option", objcmd_option, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_player", objcmd_player, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_monster", objcmd_monster, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_object", objcmd_object, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_artifact", objcmd_artifact, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_ego", objcmd_ego, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_rune", objcmd_rune, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_feature", objcmd_feature, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_trap", objcmd_trap, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_virtue", objcmd_virtue, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_mutation", objcmd_mutation, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_history", objcmd_history, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_gear", objcmd_gear, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_rename", objcmd_rename, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_dump", objcmd_dump, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_path", objcmd_path, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_version", objcmd_version, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_tilescale", objcmd_tilescale, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_refont", objcmd_refont, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_palette", objcmd_palette, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_sound", objcmd_sound, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_keymap", objcmd_keymap, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_keyboard", objcmd_keyboard, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_music", objcmd_music, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_choice", objcmd_choice, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_book", objcmd_book, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_store", objcmd_store, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_service", objcmd_service, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_birth", objcmd_birth, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_savefile", objcmd_savefile, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_death", objcmd_death, NULL, NULL);
	Tcl_CreateObjCommand2(interp, "angband_scores", objcmd_scores, NULL,
			NULL);
	Tcl_CreateObjCommand2(interp, "angband_messages", objcmd_messages, NULL,
			NULL);

	hooks_init();
	views_install();

	/* The other direction: what the game tells us, as Tk virtual events. */
	events_init();

	/*
	 * The window itself, its bindings and its font are lib/tcl/main.tcl's --
	 * term_data_link measures the font, tells the script how big the grid is,
	 * and sources it.
	 */
	if (!terms_init()) {
		/*
		 * Say so where it can be seen.  stderr is gone by now -- Tk redirects
		 * it to /dev/null for a bundled application, which is why the first
		 * version of this failure produced a crash report and no message
		 * anywhere -- but Tk itself is up, so it can show a dialog.
		 */
		Tcl_Eval(interp,
				"catch {tk_messageBox -icon error -title \"ZangbandTK\""
				" -message \"The game window could not be built.\""
				" -detail $errorInfo}");
		return 1;
	}

	/*
	 * Put the window on the screen now, before returning.
	 *
	 * Tk creates a toplevel but does not map it until the event loop runs, and
	 * ours does not run until the game asks for input -- which is after
	 * init_angband() has loaded every gamedata file.  Without this the
	 * application launches, shows nothing at all for a second or two, and then
	 * produces a window behind whatever the player was looking at.  From the
	 * Finder that is indistinguishable from a launch that failed.
	 */
	Tcl_Eval(interp, "update\nraise .\nfocus -force .\n");

	return 0;
}

#endif /* USE_TCL */