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

#include "cmd-core.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "player-calcs.h"
#include "grafmode.h"
#include "init.h"
#include "ui-command.h"
#include "ui-display.h"
#include "ui-game.h"
#include "main.h"
#include "option.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-prefs.h"
#include "ui-term.h"

#include <tcl.h>
#include <tk.h>

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
 * Make the bundled fonts available to Tk, without installing anything.
 *
 * The design system this front end is drawn to names three faces --
 * Cormorant Garamond, Lora and Courier Prime -- and none of them is on a
 * stock macOS.  Tk has no API for loading a font file: it asks the platform
 * for a family by name and takes what it is given.  So the platform is told
 * about them first.
 *
 * Process scope, so nothing is installed for the user and nothing survives
 * the program exiting.  All three are SIL Open Font Licence 1.1, which is why
 * they can be shipped at all; lib/fonts carries each licence beside its font.
 *
 * A font that will not register is not fatal.  lib/tcl/classical.tcl names a
 * fallback for every face, and a sheet set in Baskerville is a lesser thing
 * than one set in Cormorant, not a broken one.
 */
/*
 * __APPLE__, not MACH_O_CARBON: the latter is the Makefile build's marker for
 * the Cocoa front end and the cmake build does not define it, so guarding on
 * it compiled the do-nothing branch and the fonts never appeared.
 */
#ifdef __APPLE__

#include <ApplicationServices/ApplicationServices.h>

static void fonts_register(void)
{
	char path[1024];
	ang_dir *dir;
	char name[256];

	if (!ANGBAND_DIR_FONTS) return;

	dir = my_dopen(ANGBAND_DIR_FONTS);
	if (!dir) return;

	while (my_dread(dir, name, sizeof(name))) {
		CFURLRef url;
		CFStringRef str;
		CFErrorRef err = NULL;

		if (!suffix_i(name, ".ttf") && !suffix_i(name, ".otf")) continue;

		path_build(path, sizeof(path), ANGBAND_DIR_FONTS, name);

		str = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
		if (!str) continue;

		url = CFURLCreateWithFileSystemPath(NULL, str,
				kCFURLPOSIXPathStyle, false);
		CFRelease(str);
		if (!url) continue;

		if (!CTFontManagerRegisterFontsForURL(url,
				kCTFontManagerScopeProcess, &err)) {
			/*
			 * Worth saying once, quietly: the usual cause is the same file
			 * registered twice, which is harmless, and the alternative is a
			 * dialog about typography in front of somebody trying to play.
			 */
			plog_fmt("Tcl/Tk: could not register the font %s.", name);
			if (err) CFRelease(err);
		}

		CFRelease(url);
	}

	my_dclose(dir);
}

#else

static void fonts_register(void)
{
	/*
	 * Windows wants AddFontResourceEx and Linux FcConfigAppFontAddFile.
	 * Neither is written yet -- decision 13 puts those platforms after macOS
	 * -- and until they are, the fallbacks in classical.tcl are what those
	 * builds get.
	 */
}

#endif /* __APPLE__ */

/**
 * Point Tcl and Tk at their own script libraries.
 *
 * Tcl_Init finds init.tcl by walking up from the executable, which works for a
 * tclsh installed beside its lib directory and does not work for a game
 * executable sitting in a cmake build tree.  Rather than discover that as
 * "invalid command name" from the first script we source, say where they are.
 *
 * Inside a bundle they come from Contents/Resources/tcltk, which is the copy
 * scripts/pkg_macos_tcltk puts there; outside one they come from the build's
 * TCLTK_PREFIX.  Either way the environment wins if it is already set, so a
 * developer can point a build at a different toolchain without reconfiguring.
 */
static void set_script_library_paths(void)
{
	static char tclbuf[1024];
	static char tkbuf[1024];
	const char *resources = NULL;

#ifdef __APPLE__
	resources = macos_bundle_resources();
#endif

	if (!getenv("TCL_LIBRARY")) {
		if (resources) {
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s/tcltk/tcl9.0",
					resources);
		} else {
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s/lib/tcl9.0",
					TCLTK_PREFIX_PATH);
		}
		putenv(tclbuf);
	}

	if (!getenv("TK_LIBRARY")) {
		if (resources) {
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s/tcltk/tk9.0",
					resources);
		} else {
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s/lib/tk9.0",
					TCLTK_PREFIX_PATH);
		}
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

static errr Term_xtra_tcl(int n, int v)
{
	term_data *td = (term_data *)(Term->data);

	/*
	 * The first request for input is past everything that would otherwise
	 * overwrite these: textui_init() has assigned the subwindows by index and
	 * textui_input_init() has filled the game's input hooks.
	 */
	if (!flags_applied && n == TERM_XTRA_EVENT) {
		flags_applied = true;
		subwindows_set_flags(want_flag, ANGBAND_TERM_MAX);
		hooks_apply();
	}

	switch (n) {
		case TERM_XTRA_EVENT:
			if (v) {
				Tcl_DoOneEvent(TCL_ALL_EVENTS);
			} else {
				while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
					;
			}
			return 0;

		case TERM_XTRA_FLUSH:
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
				;
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
 */
static int objcmd_key(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int keysym, state;
	const char *ch;
	keycode_t k = 0;
	uint8_t mods = 0;

	(void)dummy;

	if (objc != 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "keysym state char");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &keysym) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[2], &state) != TCL_OK) return TCL_ERROR;
	ch = Tcl_GetString(objv[3]);

	/* X11 modifier bits, which Tk reports on every platform. */
	if (state & 0x01) mods |= KC_MOD_SHIFT;
	if (state & 0x04) mods |= KC_MOD_CONTROL;
	if (state & 0x08) mods |= KC_MOD_ALT;

	switch (keysym) {
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
			} else {
				/* A bare modifier, or a key we do not map.  Ignore it. */
				return TCL_OK;
			}
			break;
	}

	Term_keypress(k, mods);

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
		plog_fmt("Tcl/Tk: <<Angband_%s>>: %s", game_event_name[type],
				Tcl_GetStringResult(interp));
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
		event_add_handler((game_event_type)i, event_to_tcl, NULL);
	}
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

	return TCL_OK;
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

static struct hook_slot {
	const char *name;
	void **pointer;		/* the game's variable */
	void *trampoline;	/* ours, or NULL if not answerable yet */
	void *original;		/* textui's, kept so it can be given back */
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
 * everywhere, and the casts through void * want to be in one place where they
 * can be read against game-input.h line by line.
 */
static void hooks_init(void)
{
	struct hook_slot *h = hooks;

	h[HOOK_STRING].name = "string";
	h[HOOK_STRING].pointer = (void **)&get_string_hook;
	h[HOOK_STRING].trampoline = (void *)tcl_get_string;

	h[HOOK_QUANTITY].name = "quantity";
	h[HOOK_QUANTITY].pointer = (void **)&get_quantity_hook;
	h[HOOK_QUANTITY].trampoline = (void *)tcl_get_quantity;

	h[HOOK_CHECK].name = "check";
	h[HOOK_CHECK].pointer = (void **)&get_check_hook;
	h[HOOK_CHECK].trampoline = (void *)tcl_get_check;

	h[HOOK_COM].name = "com";
	h[HOOK_COM].pointer = (void **)&get_com_hook;
	h[HOOK_COM].trampoline = (void *)tcl_get_com;

	h[HOOK_REP_DIR].name = "rep_dir";
	h[HOOK_REP_DIR].pointer = (void **)&get_rep_dir_hook;
	h[HOOK_REP_DIR].trampoline = (void *)tcl_get_rep_dir;

	h[HOOK_AIM_DIR].name = "aim_dir";
	h[HOOK_AIM_DIR].pointer = (void **)&get_aim_dir_hook;
	h[HOOK_AIM_DIR].trampoline = (void *)tcl_get_aim_dir;

	h[HOOK_POINT].name = "point";
	h[HOOK_POINT].pointer = (void **)&get_point_hook;
	h[HOOK_POINT].trampoline = (void *)tcl_get_point;

	h[HOOK_CONFIRM_DEBUG].name = "confirm_debug";
	h[HOOK_CONFIRM_DEBUG].pointer = (void **)&confirm_debug_hook;
	h[HOOK_CONFIRM_DEBUG].trampoline = (void *)tcl_confirm_debug;
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
	PFI_GOLD, PFI_DEPTH, PFI_MAX_DEPTH, PFI_POSITION, PFI_IN_WILD,
	PFI_HITPOINTS, PFI_MANA, PFI_ARMOR_CLASS, PFI_TO_HIT, PFI_TO_DAM,
	PFI_BLOWS_PER_ROUND, PFI_SHOTS_PER_ROUND, PFI_SPEED, PFI_INFRAVISION,
	PFI_LIGHT, PFI_AGE, PFI_HEIGHT, PFI_WEIGHT, PFI_TOTAL_WEIGHT,
	PFI_NEW_SPELLS, PFI_RUNNING, PFI_RESTING, PFI_IS_DEAD, PFI_DIED_FROM,
	PFI_INSIDE_ARENA, PFI_TURN,
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
	{ "turn",				PF_INT },
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
		long base = (player->lev > 1)
				? player_exp[player->lev - 2] * player->expfact / 100L : 0;
		long want = (player->lev < PY_MAX_LEVEL)
				? player_exp[player->lev - 1] * player->expfact / 100L : base;

		pair = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, pair,
				Tcl_NewIntObj((int)(player->exp - base)));
		Tcl_ListObjAppendElement(NULL, pair,
				Tcl_NewIntObj((int)(want - base)));
		return pair;
	}
	case PFI_GOLD:		return Tcl_NewIntObj((int)player->au);
	case PFI_DEPTH:		return Tcl_NewIntObj(player->depth);
	case PFI_MAX_DEPTH:	return Tcl_NewIntObj(player->max_depth);
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
 * angband_option -- read and write the game's options.
 *
 *    angband_option                     every option, as {name type desc value}
 *    angband_option use_old_target      its value
 *    angband_option use_old_target 1    set it
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
 */
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
			Tcl_ListObjAppendElement(ip, list, row);
		}

		Tcl_SetObjResult(ip, list);

		return TCL_OK;
	}

	name = Tcl_GetString(objv[1]);

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

			val = obj_true(objv[2]) ? 1 : 0;

			/*
			 * option_set, not a write straight into the array: it is what
			 * refuses a birth option after birth and what marks a character as
			 * a cheater when a cheat option goes on.  A script has no business
			 * getting round either.
			 */
			if (!option_set(name, val)) {
				Tcl_SetObjResult(ip, Tcl_ObjPrintf(
						"%s cannot be changed now", name));
				return TCL_ERROR;
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
 * angband_commands -- the game's own command table, for building menus from.
 *
 * Returns one row per command as {group index label key enabled level code},
 * where
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
			char keybuf[3];

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
			key = c->cmd ? cmd_lookup_key(c->cmd, mode) : 0;
			if (key && key < 0x20) {
				keybuf[0] = '^';
				keybuf[1] = (char)UN_KTRL_CAP(key);
				keybuf[2] = 0;
			} else {
				keybuf[0] = (char)key;
				keybuf[1] = 0;
			}

			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(group->name, -1));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewIntObj((int)i));
			Tcl_ListObjAppendElement(ip, row, Tcl_NewStringObj(c->desc, -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewStringObj(key ? keybuf : "", -1));
			Tcl_ListObjAppendElement(ip, row,
					Tcl_NewBooleanObj(in_play()
						&& (!c->prereq || c->prereq())));
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
			Tcl_ListObjAppendElement(ip, list, row);
		}
	}

	Tcl_SetObjResult(ip, list);

	return TCL_OK;
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

	if (!in_play() || (c->prereq && !c->prereq())) {
		Tcl_SetObjResult(ip, Tcl_NewStringObj("not allowed just now", -1));
		return TCL_ERROR;
	}
	if (c->hook) {
		c->hook();
	} else if (c->cmd) {
		cmdq_push_repeat(c->cmd, count);
	}

	return TCL_OK;
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
	 * Two cells across, one down, whatever the set.
	 *
	 * The arithmetic is the same for every square tileset: we want the drawn
	 * tile to be about as wide as it is tall, so cw*tile_width should be near
	 * ch, and 20/11 rounds to 2 regardless of whether the tile is 8, 16 or 32
	 * pixels.  The tile's own size only decides how much detail survives the
	 * scaling.
	 */
	tile_width = 2;
	tile_height = 1;

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

	reset_visuals(true);
	do_cmd_redraw();

	return TCL_OK;
}

/**
 * Find the tile sets, choose one, and load its sheet.
 *
 * Mode 7 is the Neon set, which is the one this project uses -- it is
 * generated from text shapes rather than drawn, so the sheet is never edited
 * by hand.  Falling back to no graphics rather than failing is deliberate: a
 * missing tile sheet should cost you tiles, not the game.
 */
static void graphics_init(void)
{
	graphics_mode *mode;

	if (!init_graphics_modes()) {
		plog("Tcl/Tk: no graphics modes; running without tiles.");
		return;
	}

	mode = get_graphics_mode(7);
	if (!mode) {
		plog("Tcl/Tk: the Neon tile set is not installed; running without tiles.");
		return;
	}

	if (!graphics_load(mode)) {
		plog("Tcl/Tk: running without tiles.");
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

	return TCL_OK;
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
	graphics_init();

	if (!tcl_source("main.tcl")) return false;

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
 * angband_quit -- the Tcl side of closing the window.
 *
 * The game owns the main loop, so a Tk event handler cannot end the program by
 * returning.  quit() unwinds through the game's own shutdown, which is what
 * saves the character, so the window manager's close button routes here.
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

	quit(NULL);

	/* Not reached. */
	return TCL_OK;
}

const char help_tcl[] = "Tcl/Tk front end";

/**
 * Bring up the interpreter and the window.
 */
errr init_tcl(int argc, char **argv)
{
	(void)argc;

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

	/*
	 * Before Tk, because Tk asks the platform for its font list as it starts
	 * and a face registered afterwards is a face the first "font create" will
	 * not find.
	 */
	fonts_register();

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

	hooks_init();

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
