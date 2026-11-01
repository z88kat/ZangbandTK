/**
 * \file tcl/iso.c
 * \brief The Tcl/Tk front end's isometric map view.
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

/*
 * The map pane drawn as ZAngbandTk 2.4's isometric mode drew it in 2001: each
 * square a diamond, walls standing up off the floor, the character in or near
 * the middle.  None of that version's code or art is used.  The terrain comes
 * from lib/tiles/iso, which scripts/tiles/make-iso.py draws in three styles;
 * what stands on it -- monsters, objects, the character, a bolt in flight -- is
 * the current tile set's own tile, stood up on the diamond, or a counter where
 * the set has no picture.  A fourth style, ASCII, draws no terrain at all and
 * stands the text map's own characters up instead.
 *
 * It cannot ride on Term_pict as the flat map does.  The term holds an upright
 * rectangle of the level and the view is a rotated one, and a wall's drawing
 * depends on its neighbours.  So while the view is on, the map's cells only
 * say that something changed, and the view is drawn again whole, back to
 * front, from the character's memory of the level.  What stands on a square
 * is read from the term where the game drew it there, which brings bolts,
 * hallucination and detection along unasked, and asked of map_info() -- as
 * the overhead map asks it -- where the square is outside the term's
 * rectangle.  A bolt or a blast there, which the term never holds, is told
 * to this file by print_rel_hook and kept until the square is drawn again.
 *
 * main-tcl.c owns the term and its pixels; it calls in here through the
 * tcl_iso_ functions at the end, and lends this file what it needs back
 * through tcl/main-tcl.h.
 */

#include <math.h>

#include "angband.h"
#include "cave.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "grafmode.h"
#include "init.h"
#include "player-calcs.h"
#include "trap.h"
#include "ui-map.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-term.h"
#include "tcl/main-tcl.h"

enum { ISO_NONE, ISO_TILE, ISO_WALL, ISO_DOOR, ISO_SHOP };

#define ISO_HW 32		/* half a diamond's width */
#define ISO_HH 16		/* half its height */
#define ISO_FADE 120		/* of 256: how far a remembered square fades */
#define ISO_EFFECTS 256		/* as many squares as a ball can cover */
#define ISO_FLAT .8		/* how much of its square a thing on the floor covers */

/*
 * Where a row of a tile has anything in it, [x0, x1), and whether every pixel
 * between is drawn, so that the row can be copied whole.
 */
struct iso_span {
	uint16_t x0, x1;
	bool solid;
};

static struct {
	bool on;
	bool dirty;
	/*
	 * Out of the world: in a shop, or the game over, where the map term
	 * shows the shop or the scores and not the map.
	 */
	bool away;
	char style[16];
	unsigned char *lit, *dim;	/* the sheet as seen, and as remembered */
	int w, h;
	struct iso_span *span;		/* per row of every cell of the sheet */
	int cells;
	int tw, th, cols;
	uint8_t kind[FEAT_MAX];
	int cell[FEAT_MAX];
	int cut[FEAT_MAX];		/* cut away, in front of the character */
	/* How many drawings a wall has, one after another; see iso_variant(). */
	int var[FEAT_MAX];
	int wall[FEAT_MAX], wall_cut[FEAT_MAX];	/* a shop's building */
	/* On the surface, where permanent walls are buildings and not vaults. */
	uint8_t skind[FEAT_MAX];
	int scell[FEAT_MAX], scut[FEAT_MAX], svar[FEAT_MAX];
	unsigned char paper[3], ink[3];
	/*
	 * The ASCII view draws no sheet: it stands the map's own characters on
	 * the lattice.  hw and hh are half a square across and down, in every
	 * view; rise is how far each character in a stack stands above the last.
	 */
	bool text;
	int hw, hh, rise;
	/* The map's term, for the game's questions about what is on screen. */
	const term_data *map;
	/*
	 * The cursor, when the game shows one on the map -- looking, aiming --
	 * and the square the view is held on while the cursor is somewhere the
	 * view would not otherwise show.  shown is the middle of
	 * the view as last drawn, which is what a point on the pane is measured
	 * from.
	 */
	bool cursor_on, cursor_map, held;
	struct loc cursor, hold, shown;
	/*
	 * The square the view is centred on when the cursor does not hold it:
	 * the character's, or where the character stood when the view last
	 * scrolled, on the level it stood on.  See iso_scroll().
	 */
	bool anchored;
	struct loc anchor;
	const struct chunk *anchor_cave;
	/* The game's answers to "what is on screen?", given back when the view goes. */
	void (*old_get_panel)(int *min_y, int *min_x, int *max_y, int *max_x);
	bool (*old_panel_contains)(unsigned int y, unsigned int x);
	/* The letters for counters: white on clear, side by side. */
	unsigned char *glyph;
	int gw, gh, gfirst, gcount, gstride;
	/* Per cell of the map term: the map's own letter, drawn as a counter. */
	unsigned char *letter;
	size_t letter_n;
	/*
	 * The map's part of the term's pixels as Tk was last given them, and
	 * where that part is, so that only what a frame changes is handed over.
	 * Kept by tcl_iso_shown() as each block goes, whoever drew it; NULL when
	 * the view has not been drawn since it came on.
	 */
	unsigned char *shown_pix;
	int shown_l, shown_tp, shown_r, shown_b;
	/*
	 * What the game has drawn on squares outside the term's rectangle -- a
	 * bolt in flight, an arrow, a ball's blast -- until it draws them again.
	 */
	struct {
		struct loc grid;
		int a, c;
	} effect[ISO_EFFECTS];
	int effect_n;
} iso;

static void iso_get_panel(int *min_y, int *min_x, int *max_y, int *max_x);
static bool iso_panel_contains(unsigned int y, unsigned int x);

static void iso_free(void)
{
	mem_free(iso.lit);
	mem_free(iso.span);
	iso.span = NULL;
	iso.cells = 0;
	mem_free(iso.dim);
	mem_free(iso.glyph);
	iso.lit = iso.dim = iso.glyph = NULL;
	iso.on = false;
	iso.held = false;
	iso.anchored = false;
	iso.effect_n = 0;
	mem_free(iso.shown_pix);
	iso.shown_pix = NULL;
	if (get_panel_hook == iso_get_panel) get_panel_hook = iso.old_get_panel;
	if (panel_contains_hook == iso_panel_contains)
		panel_contains_hook = iso.old_panel_contains;
}

static bool iso_load(const char *style)
{
	char dir[1024], path[1024], name[64], line[256];
	unsigned char *px = NULL;
	int w, h, i, floor_cell = -1;
	bool have_style = false;
	ang_file *fh;

	path_build(dir, sizeof(dir), ANGBAND_DIR_TILES, "iso");
	path_build(path, sizeof(path), dir, "iso.txt");
	fh = file_open(path, MODE_READ, FTYPE_TEXT);
	if (!fh) return false;
	iso_free();
	memset(iso.kind, 0, sizeof(iso.kind));
	memset(iso.skind, 0, sizeof(iso.skind));
	iso.tw = 64;
	iso.th = 80;
	iso.cols = 16;
	iso.text = false;
	iso.hw = ISO_HW;
	iso.hh = ISO_HH;
	iso.rise = 0;
	while (file_getl(fh, line, sizeof(line))) {
		/*
		 * feat:<terrain>:<kind>:<cell>:<cut away>, with
		 * :<variants> after a wall's, or for a shop
		 * feat:<terrain>:shop:<overlays>:<wall>:<wall cut away>, and
		 * surface: lines the same, used instead on the surface.
		 */
		char *field[6], *p = line, sname[16], gname[64];
		int nf = 0, kind, pr, pg, pb, ir, ig, ib, gw, gh, gfirst, gcount;
		bool surface;

		if (sscanf(line, "size:%d:%d:%d", &iso.tw, &iso.th, &iso.cols) == 3)
			continue;
		/* text:<style>:<half width>:<half height>:<rise> */
		if (sscanf(line, "text:%15[^:]:%d:%d:%d", sname, &gw, &gh, &ir) == 4) {
			if (streq(sname, style)) {
				iso.text = true;
				iso.hw = gw;
				iso.hh = gh;
				iso.rise = ir;
				memset(iso.paper, 0, sizeof(iso.paper));
				memset(iso.ink, 192, sizeof(iso.ink));
				have_style = true;
			}
			continue;
		}
		/* glyphs:<file>:<width>:<height>:<first char>:<count> */
		if (sscanf(line, "glyphs:%63[^:]:%d:%d:%d:%d", gname, &gw, &gh,
				&gfirst, &gcount) == 5) {
			char gpath[1024];
			int sw, sh;

			path_build(gpath, sizeof(gpath), dir, gname);
			if (tcl_png_load(gpath, &iso.glyph, &sw, &sh)
					&& sw >= gw * gcount && sh >= gh) {
				iso.gw = gw;
				iso.gh = gh;
				iso.gfirst = gfirst;
				iso.gcount = gcount;
				iso.gstride = sw;
			} else {
				mem_free(iso.glyph);
				iso.glyph = NULL;
			}
			continue;
		}
		/* style:<name>:<paper r:g:b>:<ink r:g:b> */
		if (sscanf(line, "style:%15[^:]:%d:%d:%d:%d:%d:%d", sname, &pr, &pg,
				&pb, &ir, &ig, &ib) == 7) {
			if (streq(sname, style)) {
				iso.paper[0] = (unsigned char)pr;
				iso.paper[1] = (unsigned char)pg;
				iso.paper[2] = (unsigned char)pb;
				iso.ink[0] = (unsigned char)ir;
				iso.ink[1] = (unsigned char)ig;
				iso.ink[2] = (unsigned char)ib;
				have_style = true;
			}
			continue;
		}
		if (prefix(line, "feat:")) surface = false;
		else if (prefix(line, "surface:")) surface = true;
		else continue;
		while (nf < 6) {
			field[nf++] = p;
			p = strchr(p, ':');
			if (!p) break;
			*p++ = '\0';
		}
		if (nf < 4) continue;
		kind = streq(field[2], "wall") ? ISO_WALL :
			streq(field[2], "door") ? ISO_DOOR :
			streq(field[2], "shop") ? ISO_SHOP : ISO_TILE;
		/* Two terrains may share a name; each of them gets the drawing. */
		for (i = 1; i < FEAT_MAX; i++) {
			int cell = atoi(field[3]);
			int cut = nf > 4 ? atoi(field[4]) : cell;
			int var = (kind == ISO_WALL && nf > 5) ? atoi(field[5]) : 1;

			if (!f_info[i].name || !streq(f_info[i].name, field[1]))
				continue;
			if (var < 1) var = 1;
			if (surface) {
				iso.skind[i] = (uint8_t)kind;
				iso.scell[i] = cell;
				iso.scut[i] = cut;
				iso.svar[i] = var;
				continue;
			}
			iso.kind[i] = (uint8_t)kind;
			iso.cell[i] = cell;
			iso.cut[i] = cut;
			iso.var[i] = var;
			if (kind == ISO_SHOP) {
				iso.wall[i] = cut;
				iso.wall_cut[i] = nf > 5 ? atoi(field[5]) : cut;
			}
			if (i == FEAT_FLOOR) floor_cell = cell;
		}
	}
	file_close(fh);

	/* The drawn views have a sheet; the ASCII view needs only the letters. */
	if (!iso.text) {
		strnfmt(name, sizeof(name), "iso-%s.png", style);
		path_build(path, sizeof(path), dir, name);
		if (!tcl_png_load(path, &px, &w, &h)) {
			iso_free();
			return false;
		}
		iso.lit = px;
		iso.w = w;
		iso.h = h;

		/*
		 * A map that does not say what its paper is gets the middle of a
		 * bare floor for it, and a dark grey for its ink.
		 */
		if (!have_style && floor_cell >= 0) {
			const unsigned char *pp = px + ((size_t)((floor_cell / iso.cols)
				* iso.th + iso.th - ISO_HH) * w + (floor_cell % iso.cols)
				* iso.tw + iso.tw / 2) * 4;

			memcpy(iso.paper, pp, 3);
			memset(iso.ink, 40, sizeof(iso.ink));
		}

		/* Each row of each cell: where it has anything, and if it is whole. */
		iso.cells = iso.cols * (h / iso.th);
		iso.span = mem_zalloc((size_t)iso.cells * iso.th
			* sizeof(*iso.span));
		for (i = 0; i < iso.cells && (i % iso.cols + 1) * iso.tw <= w; i++) {
			int y;

			for (y = 0; y < iso.th; y++) {
				const unsigned char *row = px + ((size_t)((i / iso.cols)
					* iso.th + y) * w + (i % iso.cols) * iso.tw) * 4;
				struct iso_span *sp = &iso.span[(size_t)i * iso.th + y];
				int x, x0 = iso.tw, x1 = 0;

				for (x = 0; x < iso.tw; x++)
					if (row[x * 4 + 3]) {
						if (x0 > x) x0 = x;
						x1 = x + 1;
					}
				if (x1 <= x0) continue;
				sp->x0 = (uint16_t)x0;
				sp->x1 = (uint16_t)x1;
				sp->solid = true;
				for (x = x0; x < x1; x++)
					if (row[x * 4 + 3] != 255) sp->solid = false;
			}
		}

		/* Remembered rather than seen: the same drawing, in pencil. */
		iso.dim = mem_alloc((size_t)w * h * 4);
		for (i = 0; i < w * h; i++) {
			int k;

			for (k = 0; k < 3; k++)
				iso.dim[i * 4 + k] = (unsigned char)(px[i * 4 + k]
					+ ((iso.paper[k] - px[i * 4 + k]) * ISO_FADE >> 8));
			iso.dim[i * 4 + 3] = px[i * 4 + 3];
		}
	} else if (!iso.glyph) {
		/* Characters with nothing to draw them with: no view. */
		iso_free();
		return false;
	}

	my_strcpy(iso.style, style, sizeof(iso.style));
	iso.on = true;
	iso.dirty = true;
	return true;
}

/* The map's part of the pane, in pixels, and the middle of it. */
static void iso_view(const term_data *t, int *l, int *tp, int *r, int *b,
		int *vx, int *vy)
{
	const term *tm = &t->t;

	*l = col_map[tm->sidebar_mode] * t->cw;
	*tp = row_top_map[tm->sidebar_mode] * t->ch;
	*r = t->pix_w;
	*b = t->pix_h - row_bottom_map[tm->sidebar_mode] * t->ch;
	*vx = (*l + *r) / 2;
	*vy = (*tp + *b) / 2;
}

/* Is this cell of the map term part of the map, with the view showing? */
bool tcl_iso_map_cell(const term_data *t, int x, int y)
{
	const term *tm = &t->t;

	if (!iso.on || iso.away) return false;
	return y >= row_top_map[tm->sidebar_mode] && x >= col_map[tm->sidebar_mode]
		&& y < tm->hgt - row_bottom_map[tm->sidebar_mode];
}

static int iso_known_feat(struct loc grid)
{
	int feat;

	if (!square_in_bounds(cave, grid)) return FEAT_NONE;
	feat = square(player->cave, grid)->feat;
	if (f_info[feat].mimic) feat = (int)(f_info[feat].mimic - f_info);
	return feat;
}

/*
 * Which of a wall's n drawings a square has: always the same one for the same
 * square, and with no pattern to which, so that a run of wall is not one
 * square's stones over and over.
 */
static int iso_variant(struct loc grid, int n)
{
	uint32_t h = (uint32_t)grid.x * 0x9E3779B1u ^ (uint32_t)grid.y * 0x85EBCA77u;

	h ^= h >> 15;
	h *= 0x2C1B3C6Du;
	h ^= h >> 12;
	return n > 1 ? (int)(h % (uint32_t)n) : 0;
}

/*
 * Whether a square is one of the three whose walls would stand in front of
 * the character, and so is cut down to its footings.
 */
static bool iso_cut(struct loc grid)
{
	int pdx = grid.x - player->grid.x, pdy = grid.y - player->grid.y;

	return pdx >= 0 && pdy >= 0 && pdx + pdy > 0 && pdx < 2 && pdy < 2;
}

/* Walls join up with walls, and a shop's door is part of its building's. */
static bool iso_is_wall(struct loc grid)
{
	int kind = iso.kind[iso_known_feat(grid)];

	return kind == ISO_WALL || kind == ISO_SHOP;
}

/*
 * A cell of the sheet, its top left at (ox, oy).  Most of a tile is clear --
 * a floor is a diamond at the foot of a cell three times its height -- so
 * each row is drawn only where it has anything, and copied whole where every
 * pixel of it is drawn.
 */
static void iso_blit(term_data *t, const unsigned char *src, int cell,
		int ox, int oy, int l, int tp, int r, int b)
{
	int sx = (cell % iso.cols) * iso.tw, sy = (cell / iso.cols) * iso.th;
	int x, y;

	if (cell < 0 || cell >= iso.cells || sx + iso.tw > iso.w
			|| sy + iso.th > iso.h)
		return;
	for (y = 0; y < iso.th; y++) {
		const struct iso_span *sp = &iso.span[(size_t)cell * iso.th + y];
		const unsigned char *s;
		unsigned char *d;
		int x0 = MAX(sp->x0, l - ox), x1 = MIN(sp->x1, r - ox);

		if (x1 <= x0 || oy + y < tp || oy + y >= b) continue;
		s = src + ((size_t)(sy + y) * iso.w + sx + x0) * 4;
		d = t->pix + ((size_t)(oy + y) * t->pix_w + ox + x0) * 4;
		if (sp->solid) {
			memcpy(d, s, (size_t)(x1 - x0) * 4);
			continue;
		}
		for (x = x0; x < x1; x++, s += 4, d += 4) {
			if (!s[3]) continue;
			d[0] = s[0];
			d[1] = s[1];
			d[2] = s[2];
			d[3] = 255;
		}
	}
}

/*
 * Where tile a, c of the current set is on its sheet, and the colour of the
 * outline every figure gets, faded when dim; the sheet, or NULL.  A set with
 * no transparency -- Neon draws on black -- has its black taken as the
 * background, and *opaque says so.
 *
 * Every figure is outlined, one pixel all round.  The sets were drawn to be
 * seen on black, and on paper their palest figures -- all of Neon, and any
 * set's white monsters -- otherwise all but vanish.  A set drawn on black
 * keeps a rim of its own black, like a sign cut out of its board; the rest
 * are outlined in the style's ink.
 */
static const unsigned char *iso_tile(int a, int c, int *sx, int *sy,
		int *tw, int *th, int *sheet_w, bool *opaque, unsigned char *edge,
		bool dim)
{
	static const unsigned char *checked = NULL;
	static bool all_opaque = false;
	int sheet_h, k;
	const unsigned char *sheet = tcl_tile_sheet(sheet_w, &sheet_h);

	if (!sheet || !current_graphics_mode) return NULL;
	*tw = current_graphics_mode->cell_width;
	*th = current_graphics_mode->cell_height;
	*sx = (c & 0x7f) * *tw;
	*sy = (a & 0x7f) * *th;
	if (*sx + *tw > *sheet_w || *sy + *th > sheet_h) return NULL;

	if (checked != sheet) {
		size_t i, n = (size_t)*sheet_w * sheet_h;

		all_opaque = true;
		for (i = 0; i < n && all_opaque; i++)
			if (sheet[i * 4 + 3] != 255) all_opaque = false;
		checked = sheet;
	}
	*opaque = all_opaque;

	for (k = 0; k < 3; k++) {
		edge[k] = all_opaque ? 0 : iso.ink[k];
		if (dim)
			edge[k] = (unsigned char)(edge[k]
				+ ((iso.paper[k] - edge[k]) * ISO_FADE >> 8));
	}
	return sheet;
}

/* Whether a pixel of the sheet is part of the figure, not its background. */
static bool iso_figure(const unsigned char *s, bool opaque)
{
	return opaque ? s[0] + s[1] + s[2] >= 48 : s[3] >= 128;
}

/* A pixel of a figure, faded to the paper when it is only remembered. */
static void iso_figure_pixel(unsigned char *d, const unsigned char *s,
		bool dim)
{
	int k;

	for (k = 0; k < 3; k++)
		d[k] = dim ? (unsigned char)(s[k]
			+ ((iso.paper[k] - s[k]) * ISO_FADE >> 8)) : s[k];
	d[3] = 255;
}

/*
 * A tile of the current set, stood on the diamond whose middle is (cx, cy),
 * 32 pixels whatever the set's own size.
 */
static void iso_sprite(term_data *t, int a, int c, int cx, int cy, bool dim,
		int l, int tp, int r, int b)
{
	int tw, th, sx, sy, x, y, ox = cx - 16, oy = cy + 6 - 32, sheet_w;
	bool shape[34][34], opaque;
	unsigned char edge[3];
	const unsigned char *sheet = iso_tile(a, c, &sx, &sy, &tw, &th,
		&sheet_w, &opaque, edge, dim);

	if (!sheet) return;

	/* Where the figure is, with a pixel's margin all round for its outline. */
	memset(shape, 0, sizeof(shape));
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++)
			shape[y + 1][x + 1] = iso_figure(sheet + ((size_t)(sy
				+ y * th / 32) * sheet_w + sx + x * tw / 32) * 4, opaque);

	for (y = 0; y < 34; y++) {
		int py = oy + y - 1;

		if (py < tp || py >= b) continue;
		for (x = 0; x < 34; x++) {
			int px = ox + x - 1;
			unsigned char *d;

			if (px < l || px >= r) continue;
			d = t->pix + ((size_t)py * t->pix_w + px) * 4;
			if (shape[y][x]) {
				iso_figure_pixel(d, sheet + ((size_t)(sy
					+ (y - 1) * th / 32) * sheet_w + sx
					+ (x - 1) * tw / 32) * 4, dim);
			} else if (((y > 0 && shape[y - 1][x])
					|| (y < 33 && shape[y + 1][x])
					|| (x > 0 && shape[y][x - 1])
					|| (x < 33 && shape[y][x + 1]))) {
				memcpy(d, edge, 3);
				d[3] = 255;
			}
		}
	}
}

enum { ISO_FG_NONE, ISO_FG_TILE, ISO_FG_LETTER };

/* Which of the kept effects is on a square, or -1. */
static int iso_effect_at(struct loc grid)
{
	int i;

	for (i = 0; i < iso.effect_n; i++)
		if (loc_eq(iso.effect[i].grid, grid)) return i;
	return -1;
}

/*
 * What the map says stands on a square: a, c over ta, tc.  True when that is
 * a trap or a rune the character knows of, with nothing on it, which lies on
 * the floor rather than standing on it.  A web stands: it hangs across the
 * square.
 */
static bool iso_map_says(struct loc grid, int *a, int *c, int *ta, int *tc)
{
	struct grid_data g;
	wchar_t ch, tch;

	map_info(grid, &g);
	grid_data_as_text(&g, a, &ch, ta, &tch);
	*c = ch;
	*tc = tch;
	return g.trap && !g.hallucinate && !g.m_idx && !g.is_player
		&& !g.first_kind && !g.unseen_object && !g.unseen_money
		&& !trf_has(g.trap->flags, TRF_WEB)
		&& (trf_has(g.trap->flags, TRF_VISIBLE)
			|| trf_has(g.trap->flags, TRF_GLYPH));
}

/*
 * What stands on a square, if anything: a tile drawn over its terrain, or a
 * letter, which is how the game draws what the tile set has no picture for.
 *
 * Inside the term's rectangle it is read from the term, where the game drew
 * it -- which is how a bolt in flight gets here.  A letter there counts only
 * if it is the one the map would draw: anything else is text the game has
 * written over the square, and the map's answer is used instead.  Outside
 * it, a bolt's tile is one of the kept effects, and a bolt's letter is drawn
 * over the square as the term's are.  *cell is the term cell a letter of the
 * map's own came from, or -1, and *flat says it is a trap or a rune, lying on
 * the floor.
 */
static int iso_fg(const term_data *t, struct loc grid, int *a, int *c,
		int *cell, bool *flat)
{
	const term *tm = &t->t;
	int left = col_map[tm->sidebar_mode], top = row_top_map[tm->sidebar_mode];
	int rows = (tm->hgt - top - row_bottom_map[tm->sidebar_mode]) / tile_height;
	int cols = (tm->wid - left) / tile_width;
	int gx = grid.x - tm->offset_x, gy = grid.y - tm->offset_y;
	int ta, tc;

	*cell = -1;
	if (tm->scr && gx >= 0 && gy >= 0 && gx < cols && gy < rows) {
		int x = left + gx * tile_width, y = top + gy * tile_height;
		int ma, mc, mta, mtc;
		bool lies = iso_map_says(grid, &ma, &mc, &mta, &mtc);

		*a = tm->scr->a[y][x];
		*c = tm->scr->c[y][x];
		ta = tm->scr->ta[y][x];
		tc = tm->scr->tc[y][x];
		*flat = lies && *a == ma && *c == mc;
		if (!(*a & 0x80)) {
			/*
			 * Text the map would not draw is something drawn over the
			 * square -- the path an aimed bolt would take -- which the
			 * view draws over it in turn; what stands there is still the
			 * map's.
			 */
			ta = mta;
			tc = mtc;
			*flat = lies;
			if (ma != *a || mc != *c) {
				*a = ma;
				*c = mc;
			} else {
				*cell = y * t->cols + x;
			}
		}
	} else {
		int e = iso_effect_at(grid);

		*flat = iso_map_says(grid, a, c, &ta, &tc);
		if (e >= 0 && (iso.effect[e].a & 0x80)) {
			*a = iso.effect[e].a;
			*c = iso.effect[e].c;
			*flat = false;
		}
	}
	if (*a == ta && *c == tc) return ISO_FG_NONE;
	return (*a & 0x80) ? ISO_FG_TILE : ISO_FG_LETTER;
}

/*
 * Is the text in this cell of the map term the map's own -- what the flat map
 * would draw for that square, terrain or what stands on it -- rather than
 * something the game has written over the map?
 */
static bool iso_map_text(const term_data *t, int x, int y)
{
	const term *tm = &t->t;
	struct loc grid;
	int a, c, ta, tc;

	if (!tm->scr || !character_dungeon || !cave || !player || !player->cave)
		return false;
	grid.x = (x - col_map[tm->sidebar_mode]) / tile_width + tm->offset_x;
	grid.y = (y - row_top_map[tm->sidebar_mode]) / tile_height + tm->offset_y;
	if (!square_in_bounds(cave, grid)) return false;
	iso_map_says(grid, &a, &c, &ta, &tc);
	return tm->scr->a[y][x] == a && tm->scr->c[y][x] == c;
}

/*
 * A trap or a rune: a tile laid on the floor rather than stood on it.  Its
 * picture is drawn onto the square's diamond, a little in from the grid, as
 * something painted on the floor is seen from here -- its top towards the
 * square's north side -- and outlined as the standing figures are.
 */
static void iso_flat(term_data *t, int a, int c, int cx, int cy, bool dim,
		int l, int tp, int r, int b)
{
	enum { W = 2 * ISO_HW + 2, H = 2 * ISO_HH + 2 };
	const unsigned char *at[H][W];
	bool shape[H][W], opaque;
	int tw, th, sx, sy, x, y, sheet_w;
	unsigned char edge[3];
	const unsigned char *sheet = iso_tile(a, c, &sx, &sy, &tw, &th,
		&sheet_w, &opaque, edge, dim);

	if (!sheet) return;

	/*
	 * Each pixel's place on the diamond, and so in the picture: the
	 * diamond's top corner is the picture's top left, its right corner the
	 * top right.
	 */
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++) {
			double da = (x - W / 2 + .5) / (ISO_HW * ISO_FLAT);
			double db = (y - H / 2 + .5) / (ISO_HH * ISO_FLAT);
			double u = (da + db + 1) / 2, v = (db - da + 1) / 2;

			shape[y][x] = false;
			if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
			at[y][x] = sheet + ((size_t)(sy + (int)(v * th)) * sheet_w
				+ sx + (int)(u * tw)) * 4;
			shape[y][x] = iso_figure(at[y][x], opaque);
		}

	for (y = 0; y < H; y++) {
		int py = cy - H / 2 + y;

		if (py < tp || py >= b) continue;
		for (x = 0; x < W; x++) {
			int px = cx - W / 2 + x;
			unsigned char *d;

			if (px < l || px >= r) continue;
			d = t->pix + ((size_t)py * t->pix_w + px) * 4;
			if (shape[y][x]) {
				iso_figure_pixel(d, at[y][x], dim);
			} else if ((y > 0 && shape[y - 1][x])
					|| (y < H - 1 && shape[y + 1][x])
					|| (x > 0 && shape[y][x - 1])
					|| (x < W - 1 && shape[y][x + 1])) {
				memcpy(d, edge, 3);
				d[3] = 255;
			}
		}
	}
}

/*
 * A character of the game's, twice the font's width and high pixels high, its
 * top left at (x0, y0): in the game's colour attr, at bright/256 of its
 * brightness, and faded towards the paper when what it stands for is only
 * remembered.
 */
static void iso_glyph_high(term_data *t, int ch, int attr, int x0, int y0,
		int high, int bright, bool dim, int l, int tp, int r, int b)
{
	unsigned char rgb[3];
	int colour = attr % MAX_COLORS, x, y, k;

	if (!iso.glyph || ch < iso.gfirst || ch >= iso.gfirst + iso.gcount) return;
	for (k = 0; k < 3; k++) {
		rgb[k] = (unsigned char)(angband_color_table[colour][k + 1]
			* bright >> 8);
		if (dim)
			rgb[k] = (unsigned char)(rgb[k]
				+ ((iso.paper[k] - rgb[k]) * ISO_FADE >> 8));
	}
	for (y = 0; y < high; y++) {
		if (y0 + y < tp || y0 + y >= b) continue;
		for (x = 0; x < iso.gw * 2; x++) {
			const unsigned char *s = iso.glyph + ((size_t)(y * iso.gh / high)
				* iso.gstride + (ch - iso.gfirst) * iso.gw + x / 2) * 4;
			unsigned char *d;

			if (s[3] < 128 || x0 + x < l || x0 + x >= r) continue;
			d = t->pix + ((size_t)(y0 + y) * t->pix_w + x0 + x) * 4;
			memcpy(d, rgb, 3);
			d[3] = 255;
		}
	}
}

/* The same at twice the font's size. */
static void iso_glyph(term_data *t, int ch, int attr, int x0, int y0,
		int bright, bool dim, int l, int tp, int r, int b)
{
	iso_glyph_high(t, ch, attr, x0, y0, iso.gh * 2, bright, dim, l, tp, r, b);
}

/*
 * A counter: the letter the game prints for something the tile set has no
 * picture of, in its colour, on a black disc standing on the square -- as
 * tabletop games marked what they had no figure for, and as the flat map
 * shows it, on black.
 */
static void iso_counter(term_data *t, int a, int c, int cx, int cy, bool dim,
		int l, int tp, int r, int b)
{
	unsigned char disc[3] = { 24, 24, 28 }, rim[3];
	int x, y, k;
	int ox = cx - 11, oy = cy - 22;

	for (k = 0; k < 3; k++) {
		rim[k] = iso.ink[k];
		if (dim) {
			disc[k] = (unsigned char)(disc[k]
				+ ((iso.paper[k] - disc[k]) * ISO_FADE >> 8));
			rim[k] = (unsigned char)(rim[k]
				+ ((iso.paper[k] - rim[k]) * ISO_FADE >> 8));
		}
	}

	for (y = 0; y < 22; y++) {
		if (oy + y < tp || oy + y >= b) continue;
		for (x = 0; x < 22; x++) {
			double dx = x + .5 - 11, dy = y + .5 - 11, d2 = dx * dx + dy * dy;
			unsigned char *d;

			if (d2 > 121 || ox + x < l || ox + x >= r) continue;
			d = t->pix + ((size_t)(oy + y) * t->pix_w + ox + x) * 4;
			memcpy(d, d2 > 100 ? rim : disc, 3);
			d[3] = 255;
		}
	}

	/* The letter, in the middle of the disc. */
	iso_glyph(t, c, a, cx - iso.gw, cy - 11 - iso.gh, 256, dim, l, tp, r, b);
}

/*
 * A counter lying on the floor, for a trap or a rune the set has no picture
 * of: the disc seen from here, half as deep as it is wide, and the letter on
 * it at three quarters of its height -- at half it can't be read.
 */
static void iso_flat_counter(term_data *t, int a, int c, int cx, int cy,
		bool dim, int l, int tp, int r, int b)
{
	unsigned char disc[3] = { 24, 24, 28 }, rim[3];
	int x, y, k;

	for (k = 0; k < 3; k++) {
		rim[k] = iso.ink[k];
		if (dim) {
			disc[k] = (unsigned char)(disc[k]
				+ ((iso.paper[k] - disc[k]) * ISO_FADE >> 8));
			rim[k] = (unsigned char)(rim[k]
				+ ((iso.paper[k] - rim[k]) * ISO_FADE >> 8));
		}
	}

	for (y = -8; y < 8; y++) {
		if (cy + y < tp || cy + y >= b) continue;
		for (x = -15; x < 15; x++) {
			double dx = (x + .5) / 15, dy = (y + .5) / 8;
			double e = dx * dx + dy * dy;
			unsigned char *d;

			if (e > 1 || cx + x < l || cx + x >= r) continue;
			d = t->pix + ((size_t)(cy + y) * t->pix_w + cx + x) * 4;
			memcpy(d, e > .7 ? rim : disc, 3);
			d[3] = 255;
		}
	}

	iso_glyph_high(t, c, a, cx - iso.gw, cy - iso.gh * 3 / 4, iso.gh * 3 / 2,
		256, dim, l, tp, r, b);
}

/*
 * The characters the map shows on a square, a and c over the terrain's ta and
 * tc: from the term where the game drew them, which brings bolts with them,
 * and from the map and the kept effects beyond its rectangle.  True when what
 * is shown is a trap or a rune, lying on the floor.
 */
static bool iso_text_of(const term_data *t, struct loc grid, int *a, int *c,
		int *ta, int *tc)
{
	const term *tm = &t->t;
	int left = col_map[tm->sidebar_mode], top = row_top_map[tm->sidebar_mode];
	int rows = (tm->hgt - top - row_bottom_map[tm->sidebar_mode]) / tile_height;
	int cols = (tm->wid - left) / tile_width;
	int gx = grid.x - tm->offset_x, gy = grid.y - tm->offset_y;

	if (tm->scr && gx >= 0 && gy >= 0 && gx < cols && gy < rows) {
		int x = left + gx * tile_width, y = top + gy * tile_height;

		int ma, mc, mta, mtc;
		bool lies = iso_map_says(grid, &ma, &mc, &mta, &mtc);

		*a = tm->scr->a[y][x];
		*c = tm->scr->c[y][x];
		*ta = tm->scr->ta[y][x];
		*tc = tm->scr->tc[y][x];
		return lies && *a == ma && *c == mc;
	} else {
		int e = iso_effect_at(grid);
		bool lies = iso_map_says(grid, a, c, ta, tc);

		if (e >= 0) {
			*a = iso.effect[e].a;
			*c = iso.effect[e].c;
			return false;
		}
		return lies;
	}
}

/*
 * A square of the ASCII view: its terrain's character, stood up as high as
 * the terrain is -- a wall three deep, darkest at the foot, so that a run of
 * them reads as height -- and on it whatever the map shows there, raised.
 * Floors sit back, dim, so that what stands on them does not.  A trap or a
 * rune is not raised: it takes the floor's place.
 */
static void iso_text_square(term_data *t, struct loc grid, int cx, int cy,
		bool seen, bool bright, bool cut, int l, int tp, int r, int b)
{
	static const int shade3[] = { 115, 180, 256 }, shade2[] = { 154, 256 };
	int a, c, ta, tc, feat = iso_known_feat(grid), n = 0, i;
	int x0 = cx - iso.gw, y0 = cy - iso.gh;
	bool flat = iso_text_of(t, grid, &a, &c, &ta, &tc);

	if (feat != FEAT_NONE) {
		const struct feature *f = &f_info[feat];

		if (tf_has(f->flags, TF_WALL) || iso.kind[feat] == ISO_SHOP) n = 3;
		else if (!tf_has(f->flags, TF_PASSABLE)) n = 2;
		else n = 1;
		if (cut) n = 1;
		for (i = 0; i < n - flat; i++) {
			int k = (n == 3) ? shade3[i] : (n == 2) ? shade2[i]
				: tf_has(f->flags, TF_FLOOR) ? 115 : 256;

			iso_glyph(t, tc, ta, x0, y0 - i * iso.rise, k, !bright,
				l, tp, r, b);
		}
	}

	if (flat)
		iso_glyph(t, c, a, x0, y0 - (n ? n - 1 : 0) * iso.rise, 256, !seen,
			l, tp, r, b);
	else if (a != ta || c != tc)
		iso_glyph(t, c, a, x0, y0 - (n ? n : 1) * iso.rise, 256, !seen,
			l, tp, r, b);
}

/* The square in the middle of the view when the cursor does not hold it. */
static struct loc iso_home(void)
{
	return (iso.anchored && iso.anchor_cave == cave) ? iso.anchor
		: player->grid;
}

/* The square in the middle of the view. */
static struct loc iso_centre(void)
{
	return iso.held ? iso.hold : iso_home();
}

/*
 * Whether a square's middle falls on the map pane, margin pixels in from its
 * edges, with the view centred on centre.
 */
static bool iso_shows(const term_data *t, struct loc centre, struct loc grid,
		int margin)
{
	int l, tp, r, b, vx, vy, cx, cy;
	int dx = grid.x - centre.x, dy = grid.y - centre.y;

	iso_view(t, &l, &tp, &r, &b, &vx, &vy);
	cx = vx + (dx - dy) * iso.hw;
	cy = vy + (dx + dy) * iso.hh;
	return cx >= l + margin && cx < r - margin
		&& cy >= tp + margin && cy < b - margin;
}

/*
 * The game's question "what is on screen?" -- asked when it offers targets,
 * draws the path a bolt would take, and decides whether a monster can be
 * named -- answered for this view rather than for the term's upright
 * rectangle.  Any other term is answered as it always was.
 */
static void iso_get_panel(int *min_y, int *min_x, int *max_y, int *max_x)
{
	int l, tp, r, b, vx, vy, reach;
	struct loc c;

	if (!iso.on || !iso.map || !player) {
		if (iso.old_get_panel) iso.old_get_panel(min_y, min_x, max_y, max_x);
		return;
	}
	iso_view(iso.map, &l, &tp, &r, &b, &vx, &vy);
	reach = ((r - l) / 2 / iso.hw + (b - tp) / 2 / iso.hh) / 2 + 1;
	c = iso_centre();
	*min_y = c.y - reach;
	*min_x = c.x - reach;
	*max_y = c.y + reach + 1;
	*max_x = c.x + reach + 1;
}

static bool iso_panel_contains(unsigned int y, unsigned int x)
{
	if (!iso.on || !iso.map || !player || Term != term_screen)
		return iso.old_panel_contains ? iso.old_panel_contains(y, x) : true;
	return iso_shows(iso.map, iso_centre(), loc((int)x, (int)y), 0);
}

/* A letter the game drew over a square, standing above the square. */
static void iso_over(term_data *t, struct loc grid, struct loc centre, int c,
		int a, int l, int tp, int r, int b)
{
	int vx, vy, ll, ttp, rr, bb, cx, cy;
	int dx = grid.x - centre.x, dy = grid.y - centre.y;

	iso_view(t, &ll, &ttp, &rr, &bb, &vx, &vy);
	cx = vx + (dx - dy) * iso.hw;
	cy = vy + (dx + dy) * iso.hh;
	iso_glyph(t, c, a, cx - iso.gw, cy - iso.gh - 8, 256, false, l, tp, r, b);
}

/*
 * Text the game drew into a cell of the map that is not the map's own -- the
 * path an aimed bolt would take, a bolt in flight when there are no tiles --
 * drawn on its square instead of at the flat map's place for it.  False when
 * the cell is not a square of the level on the view.
 */
static bool iso_text_glyph(term_data *t, int x, int y, struct loc centre,
		int l, int tp, int r, int b)
{
	const term *tm = &t->t;
	struct loc grid;

	if (!tm->scr || !character_dungeon || !cave || !player) return false;
	grid.x = (x - col_map[tm->sidebar_mode]) / tile_width + tm->offset_x;
	grid.y = (y - row_top_map[tm->sidebar_mode]) / tile_height + tm->offset_y;
	if (!square_in_bounds(cave, grid) || !iso_shows(t, centre, grid, 0))
		return false;
	/* The ASCII view has already drawn what the term holds there. */
	if (iso.text) return true;
	iso_over(t, grid, centre, tm->scr->c[y][x], tm->scr->a[y][x],
		l, tp, r, b);
	return true;
}

/*
 * Whether a cell of the map's term is part of something the game has put
 * over the map -- text the map would not draw there, or the space cleared
 * for it -- rather than the map.  A tile is always the map's.  Outside the
 * map's own part of the term (its last column is never map) and beyond the
 * edge of the level only a letter counts, since the map's blank there is a
 * blank too.
 */
static bool iso_overlay_cell(const term_data *t, int x, int y)
{
	const term *tm = &t->t;
	int left = col_map[tm->sidebar_mode], top = row_top_map[tm->sidebar_mode];
	int cols = (tm->wid - left - 1) / tile_width;
	int rows = (tm->hgt - top - row_bottom_map[tm->sidebar_mode]) / tile_height;
	struct loc grid;

	if (!tm->scr || (tm->scr->a[y][x] & 0x80)) return false;
	grid.x = (x - left) / tile_width + tm->offset_x;
	grid.y = (y - top) / tile_height + tm->offset_y;
	if ((x - left) / tile_width >= cols || (y - top) / tile_height >= rows
			|| !square_in_bounds(cave, grid))
		return tm->scr->c[y][x] != L' ';
	return !iso_map_text(t, x, y);
}

/* The cursor: a ring round its square's diamond, in the flat map's yellow. */
static void iso_mark(term_data *t, struct loc grid, struct loc centre,
		int l, int tp, int r, int b)
{
	static const unsigned char yellow[3] = { 255, 255, 0 };
	int vx, vy, ll, ttp, rr, bb, cx, cy, x, y;
	int dx = grid.x - centre.x, dy = grid.y - centre.y;

	iso_view(t, &ll, &ttp, &rr, &bb, &vx, &vy);
	cx = vx + (dx - dy) * iso.hw;
	cy = vy + (dx + dy) * iso.hh;
	for (y = cy - iso.hh - 1; y <= cy + iso.hh + 1; y++) {
		if (y < tp || y >= b) continue;
		for (x = cx - iso.hw - 1; x <= cx + iso.hw + 1; x++) {
			double e = fabs(x + .5 - cx) / iso.hw + fabs(y + .5 - cy) / iso.hh;
			unsigned char *d;

			if (e < .86 || e > 1.04 || x < l || x >= r) continue;
			d = t->pix + ((size_t)y * t->pix_w + x) * 4;
			memcpy(d, yellow, 3);
			d[3] = 255;
		}
	}
}

/*
 * A frame is drawn: hand Tk only the box round what it changed.  Every frame
 * is drawn whole, but a monster taking a step changes a few hundred pixels of
 * it, and Tk's work -- taking the block, and above all painting it, which on a
 * Retina screen means resampling it -- goes with the size of the box: the
 * whole pane cost some 25 ms a frame.  A frame that changes nothing is not
 * handed over at all.
 */
static void iso_hand_over(term_data *t, int l, int tp, int r, int b)
{
	size_t row = (size_t)(r - l) * 4;
	int y, y0 = -1, y1 = -1, x0 = r, x1 = l;

	if (!iso.shown_pix || iso.shown_l != l || iso.shown_tp != tp
			|| iso.shown_r != r || iso.shown_b != b) {
		mem_free(iso.shown_pix);
		iso.shown_pix = mem_zalloc(row * (b - tp));
		iso.shown_l = l;
		iso.shown_tp = tp;
		iso.shown_r = r;
		iso.shown_b = b;
		tcl_tiles_dirty(t, l, tp, r - l, b - tp);
		return;
	}

	for (y = tp; y < b; y++) {
		const unsigned char *p = t->pix + ((size_t)y * t->pix_w + l) * 4;
		const unsigned char *q = iso.shown_pix + (size_t)(y - tp) * row;
		int a = 0, z = r - l - 1;

		if (!memcmp(p, q, row)) continue;
		while (!memcmp(p + a * 4, q + a * 4, 4)) a++;
		while (!memcmp(p + z * 4, q + z * 4, 4)) z--;
		if (y0 < 0) y0 = y;
		y1 = y;
		x0 = MIN(x0, l + a);
		x1 = MAX(x1, l + z + 1);
	}
	if (y0 >= 0) tcl_tiles_dirty(t, x0, y0, x1 - x0, y1 + 1 - y0);
}

static void iso_render(term_data *t)
{
	int l, tp, r, b, vx, vy, s, d, x, y, smax, dmax, i;
	struct loc centre = loc(0, 0);

	iso_view(t, &l, &tp, &r, &b, &vx, &vy);
	if (r <= l || b <= tp) return;

	/* Which of the term's cells hold a letter the map drew: none yet. */
	if (iso.letter_n != (size_t)t->cols * t->rows) {
		mem_free(iso.letter);
		iso.letter_n = (size_t)t->cols * t->rows;
		iso.letter = mem_zalloc(iso.letter_n);
	}
	memset(iso.letter, 0, iso.letter_n);

	/*
	 * With no level in play -- the splash screen after New Game, a game not
	 * yet loaded -- or out of the world, in a shop or at the scores when the
	 * game ends, there is no map to draw, and what the term holds is text:
	 * shown as the flat map shows it, on black, and not on paper with a black
	 * box behind each letter.
	 */
	if (iso.away || !character_dungeon || !player || !cave || !player->cave) {
		for (y = tp; y < b; y++)
			memset(t->pix + ((size_t)y * t->pix_w + l) * 4, 0,
				(size_t)(r - l) * 4);
		iso_hand_over(t, l, tp, r, b);
		return;
	}

	/* Paper, everywhere: what is not known is not drawn yet. */
	for (y = tp; y < b; y++) {
		unsigned char *p = t->pix + ((size_t)y * t->pix_w + l) * 4;

		for (x = l; x < r; x++, p += 4) {
			memcpy(p, iso.paper, 3);
			p[3] = 255;
		}
	}

	centre = iso_centre();
	iso.shown = centre;

	/*
	 * Back to front: by distance down the screen, s = dx + dy.  The
	 * squares on one such line sit side by side and never overlap.
	 */
	smax = (b - tp) / 2 / iso.hh + 6;
	dmax = (r - l) / 2 / iso.hw + 3;
	for (s = -smax; s <= smax; s++) {
		for (d = -dmax; d <= dmax; d++) {
			struct loc grid;
			int dx, dy, cx, cy, feat, a, c, fg, tcell;
			bool seen, bright, cut, flat;

			if ((s + d) & 1) continue;
			dx = (s + d) / 2;
			dy = (s - d) / 2;
			grid = loc(centre.x + dx, centre.y + dy);
			if (!square_in_bounds(cave, grid)) continue;

			cut = iso_cut(grid);

			cx = vx + (dx - dy) * iso.hw;
			cy = vy + (dx + dy) * iso.hh;
			/*
			 * A square fades to pencil when it is neither in view
			 * nor lit, as the flat tiles darken it: by day the whole
			 * town is lit, and a building half in view is still one
			 * colour.  What stands on a square fades whenever it is
			 * out of sight, because that is only a memory of it.
			 */
			seen = square_isseen(cave, grid);
			bright = seen || square_isglow(cave, grid);
			feat = iso_known_feat(grid);

			if (iso.text) {
				iso_text_square(t, grid, cx, cy, seen, bright, cut,
					l, tp, r, b);
				continue;
			}

			if (feat != FEAT_NONE && iso.kind[feat]) {
				const unsigned char *src = bright ? iso.lit : iso.dim;
				int kind = iso.kind[feat];
				int cell = cut ? iso.cut[feat] : iso.cell[feat];
				int var = iso.var[feat];
				int mask = iso_is_wall(loc(grid.x - 1, grid.y)) * 1
					| iso_is_wall(loc(grid.x, grid.y - 1)) * 2
					| iso_is_wall(loc(grid.x + 1, grid.y)) * 4
					| iso_is_wall(loc(grid.x, grid.y + 1)) * 8;
				bool we = mask & (1 | 4), ns = mask & (2 | 8);
				/*
				 * A wall beside one that is cut down is no longer
				 * joined to it: it shows the face, and the edge of
				 * its top, that the cut-down one no longer covers.
				 * Which way a door or a shop's door faces is still
				 * the walls' to say.
				 */
				int high = cut ? mask : mask
					& ~(iso_cut(loc(grid.x - 1, grid.y)) * 1
						| iso_cut(loc(grid.x, grid.y - 1)) * 2
						| iso_cut(loc(grid.x + 1, grid.y)) * 4
						| iso_cut(loc(grid.x, grid.y + 1)) * 8);
				int ox = cx - iso.tw / 2, oy = cy + ISO_HH - iso.th;

				if (!player->depth && iso.skind[feat]) {
					kind = iso.skind[feat];
					cell = cut ? iso.scut[feat] : iso.scell[feat];
					var = iso.svar[feat];
				}

				if (kind == ISO_WALL) {
					if (!cut) cell += 16 * iso_variant(grid, var);
					iso_blit(t, src, cell + high, ox, oy, l, tp, r, b);
				} else if (kind == ISO_DOOR) {
					iso_blit(t, src, cell + (ns && !we), ox, oy, l, tp,
						r, b);
				} else if (kind == ISO_SHOP) {
					/*
					 * The building's wall, then the door on whichever
					 * face of it looks out -- or, in the far side of
					 * the building, no door but the sign.
					 */
					bool south = !(mask & 8), east = !(mask & 4);
					int side = (ns && !we)
						? (east ? 1 : south ? 0 : 2)
						: (south ? 0 : east ? 1 : 2);

					iso_blit(t, src, (cut ? iso.wall_cut[feat]
						: iso.wall[feat]) + high, ox, oy, l, tp, r, b);
					if (!cut)
						iso_blit(t, src, iso.cell[feat] + side, ox, oy,
							l, tp, r, b);
				} else {
					iso_blit(t, src, cell, ox, oy, l, tp, r, b);
				}
			}

			fg = iso_fg(t, grid, &a, &c, &tcell, &flat);
			if (fg == ISO_FG_TILE && flat) {
				iso_flat(t, a, c, cx, cy, !seen, l, tp, r, b);
			} else if (fg == ISO_FG_TILE) {
				iso_sprite(t, a, c, cx, cy, !seen, l, tp, r, b);
			} else if (fg == ISO_FG_LETTER) {
				if (flat)
					iso_flat_counter(t, a, c, cx, cy, !seen, l, tp, r, b);
				else
					iso_counter(t, a, c, cx, cy, !seen, l, tp, r, b);
				if (tcell >= 0) iso.letter[tcell] = 1;
			}
		}
	}

	/* The letters of bolts beyond the term's rectangle, over their squares. */
	for (i = 0; i < iso.effect_n && !iso.text; i++)
		if (!(iso.effect[i].a & 0x80))
			iso_over(t, iso.effect[i].grid, centre, iso.effect[i].c,
				iso.effect[i].a, l, tp, r, b);

	/*
	 * The map's own text is drawn in the view now -- its letters standing on
	 * it as counters, and with no tile set its terrain too, as the view's own
	 * drawings -- so the term's copies of it, in the flat map's places, are
	 * put away.  So is anything else the game drew on a square of the map,
	 * which is drawn on that square.  Only while the game has saved the
	 * screen to put something over the map -- a list of what lies on the
	 * floor, a menu -- is it an overlay, and an overlay is shown as the flat
	 * map shows it: every cell of it black, the space cleared for it as well
	 * as its letters, so it reads as a panel on the map and not as letters
	 * in boxes on paper.
	 */
	for (y = row_top_map[t->t.sidebar_mode];
			y < t->rows - row_bottom_map[t->t.sidebar_mode]; y++)
		for (x = col_map[t->t.sidebar_mode]; x < t->cols; x++) {
			int row, idx = y * t->cols + x;

			if (t->t.saved && iso_overlay_cell(t, x, y)) {
				for (row = 0; row < t->ch; row++)
					memset(t->pix + ((size_t)(y * t->ch + row) * t->pix_w
						+ x * t->cw) * 4, 0, (size_t)t->cw * 4);
				continue;
			}
			if (!t->shown || !t->shown[idx]) continue;
			if (iso.letter[idx] || iso_map_text(t, x, y)
					|| (!t->t.saved
						&& iso_text_glyph(t, x, y, centre, l, tp, r, b))) {
				tcl_cell_set(t, x, y, " ", COLOUR_WHITE);
				continue;
			}
			for (row = 0; row < t->ch; row++)
				memset(t->pix + ((size_t)(y * t->ch + row) * t->pix_w
					+ x * t->cw) * 4, 0, (size_t)t->cw * 4);
		}

	if (iso.cursor_on && iso.cursor_map && character_dungeon && cave
			&& square_in_bounds(cave, iso.cursor))
		iso_mark(t, iso.cursor, centre, l, tp, r, b);

	iso_hand_over(t, l, tp, r, b);
}

/*
 * Keep the cursor on the view: hold the view on it when the view would not
 * otherwise show it, and let go when it would.
 */
static void iso_follow(const term_data *map)
{
	if (!iso.cursor_on || !iso.cursor_map || !player || !cave
			|| !square_in_bounds(cave, iso.cursor))
		return;
	if (iso_shows(map, iso_centre(), iso.cursor, iso.hw)) return;
	if (iso_shows(map, iso_home(), iso.cursor, iso.hw)) {
		iso.held = false;
	} else {
		iso.held = true;
		iso.hold = iso.cursor;
	}
}

/* ---- What the front end asks of the view ---------------------------------- */

bool tcl_iso_on(void)
{
	return iso.on;
}

/* The style being drawn, or "off". */
const char *tcl_iso_style(void)
{
	return iso.on ? iso.style : "off";
}

/*
 * Draw the view in a style -- "ink", "blue", "colour" or "ascii" -- or stop,
 * with "off".  False, and why in err, when the style cannot be had: it is not
 * installed, or it is the ASCII view and the game is drawing tiles.  Either
 * way of failing leaves the view off.
 */
bool tcl_iso_set(const char *style, const term_data *map, char *err,
		size_t len)
{
	if (streq(style, "off")) {
		iso_free();
		return true;
	}
	if (!f_info || !iso_load(style)) {
		my_strcpy(err, "could not load that isometric set", len);
		return false;
	}
	if (iso.text && use_graphics != GRAPHICS_NONE) {
		iso_free();
		my_strcpy(err, "the ASCII view needs the text tile set (None)", len);
		return false;
	}
	iso.map = map;
	if (get_panel_hook != iso_get_panel) {
		iso.old_get_panel = get_panel_hook;
		get_panel_hook = iso_get_panel;
	}
	if (panel_contains_hook != iso_panel_contains) {
		iso.old_panel_contains = panel_contains_hook;
		panel_contains_hook = iso_panel_contains;
	}
	/* The term's rectangle follows the character, as below. */
	if (character_dungeon && player) center_panel();
	return true;
}

/* Draw the view again, whole, at the next flush. */
void tcl_iso_redraw(void)
{
	iso.dirty = true;
}

/*
 * The tile set has changed.  The ASCII view stands the text map up, and once
 * the game draws tiles there is no text map to stand up, so it goes.
 */
void tcl_iso_tiles_changed(int graf_id)
{
	if (iso.on && iso.text && graf_id != GRAPHICS_NONE) iso_free();
}

/*
 * Which square of the level is under a point of the map pane, in squares and
 * fractions of one: the square is the nearest whole one.
 */
bool tcl_iso_point(const term_data *map, double px, double py, double *fx,
		double *fy)
{
	int l, tp, r, b, vx, vy;
	double u, v;

	if (!iso.on || !player) return false;
	iso_view(map, &l, &tp, &r, &b, &vx, &vy);
	if (px < l || px >= r || py < tp || py >= b) return false;
	u = (px - vx) / iso.hw;
	v = (py - vy) / iso.hh;
	*fx = iso.shown.x + (u + v) / 2;
	*fy = iso.shown.y + (v - u) / 2;
	return true;
}

/*
 * The game put the cursor on this cell of the map's term.  The term's cursor
 * is a soft one: the game draws it, by calling here, at every refresh it is
 * to be seen in -- looking, aiming -- and simply does not call when it is
 * not, so whether it is showing is tcl_iso_cursor_show()'s, at the refresh.
 */
void tcl_iso_cursor(const term_data *map, int x, int y)
{
	const term *tm = &map->t;
	struct loc grid;
	bool on_map = tcl_iso_map_cell(map, x, y);

	grid.x = (x - col_map[tm->sidebar_mode]) / tile_width + tm->offset_x;
	grid.y = (y - row_top_map[tm->sidebar_mode]) / tile_height + tm->offset_y;
	if (on_map == iso.cursor_map && loc_eq(grid, iso.cursor)) return;
	iso.cursor_map = on_map;
	iso.cursor = grid;
	iso_follow(map);
	iso.dirty = true;
}

/* At a refresh: whether the cursor was drawn in it, so is showing. */
void tcl_iso_cursor_show(const term_data *map, bool show)
{
	if (show == iso.cursor_on) return;
	iso.cursor_on = show;
	if (show) iso_follow(map);
	else iso.held = false;
	iso.dirty = true;
}

/*
 * The character has moved: does the view go with it?  As the flat map does
 * (verify_panel), it does at every step while "Center map continuously" is
 * on, except while running; otherwise it stays put until the character
 * leaves the middle half of the pane, and then centres on it again.  A view
 * that moves changes every pixel of the pane, and Tk must paint all of it; a
 * view that stays put changes only the squares round the character.
 */
static void iso_scroll(void)
{
	int l, tp, r, b, vx, vy, dx, dy;
	struct loc home;

	if (!iso.map || !player || !cave) return;
	home = iso_home();
	dx = player->grid.x - home.x;
	dy = player->grid.y - home.y;
	iso_view(iso.map, &l, &tp, &r, &b, &vx, &vy);
	if (!iso.anchored || iso.anchor_cave != cave
			|| (OPT(player, center_player) && !player->upkeep->running)
			|| abs(dx - dy) * iso.hw > (r - l) / 4
			|| abs(dx + dy) * iso.hh > (b - tp) / 4) {
		iso.anchor = player->grid;
		iso.anchor_cave = cave;
		iso.anchored = true;
	}
}

/*
 * Something on the level changed, the character moved, or the game left the
 * world or came back to it.  A square drawn again is drawn without whatever
 * effect was kept for it.  When the character moved, the view may follow it,
 * and the term's rectangle is centred on it again, as center_player has the
 * flat map do: the view shows what the term holds round the character, and
 * the path an aimed bolt would take is drawn only there.
 */
static void iso_notice(game_event_type type, game_event_data *data, void *user)
{
	(void)user;
	iso.dirty = true;
	if (type == EVENT_LEAVE_WORLD || type == EVENT_ENTER_WORLD)
		iso.away = (type == EVENT_LEAVE_WORLD);
	if (type == EVENT_MAP) {
		int e = iso_effect_at(data->point);

		if (data->point.x < 0) iso.effect_n = 0;
		else if (e >= 0) iso.effect[e] = iso.effect[--iso.effect_n];
	}
	if (type == EVENT_PLAYERMOVED && iso.on) {
		iso_scroll();
		if (!iso.held) center_panel();
	}
}

/*
 * The game drew a bolt, an arrow or a ball's blast on a square.  The term
 * holds it if the square is inside its rectangle, as print_rel() reckons
 * that; otherwise it is kept here, while there is room.
 */
static void iso_effect(wchar_t c, uint8_t a, int y, int x)
{
	int ky = y - Term->offset_y, kx = x - Term->offset_x, e;

	if (!iso.on) return;
	if (Term == term_screen && ky >= 0 && ky < SCREEN_HGT && kx >= 0
			&& kx < SCREEN_WID)
		return;
	e = iso_effect_at(loc(x, y));
	if (e < 0) {
		if (iso.effect_n == ISO_EFFECTS) return;
		e = iso.effect_n++;
	}
	iso.effect[e].grid = loc(x, y);
	iso.effect[e].a = a;
	iso.effect[e].c = c;
	iso.dirty = true;
}

/*
 * The game says when a square changes, or the character moves, wherever it
 * is -- not only inside the term's rectangle, which is all the term hears of.
 * It says too when it leaves the world for a shop or the end of the game, and
 * those are never removed, as main-tcl.c's store_notice explains.
 */
void tcl_iso_events_init(void)
{
	event_add_handler(EVENT_MAP, iso_notice, NULL);
	event_add_handler(EVENT_PLAYERMOVED, iso_notice, NULL);
	event_add_handler(EVENT_LEAVE_WORLD, iso_notice, NULL);
	event_add_handler(EVENT_ENTER_WORLD, iso_notice, NULL);
	print_rel_hook = iso_effect;
}

/* The map term is about to be handed to Tk: draw the view, if it changed. */
void tcl_iso_flush(term_data *map)
{
	if (iso.on && iso.dirty) {
		iso.dirty = false;
		iso_render(map);
	}
}

/*
 * Tk has just been given this block of the map term's pixels.  Whatever drew
 * them -- the view, or the term clearing a cell -- it is what the pane now
 * shows, and what the next frame is measured against.
 */
void tcl_iso_shown(const term_data *map, int x, int y, int w, int h)
{
	int x0, x1, y0, y1, row;

	if (!iso.on || !iso.shown_pix) return;
	x0 = MAX(x, iso.shown_l);
	x1 = MIN(x + w, iso.shown_r);
	y0 = MAX(y, iso.shown_tp);
	y1 = MIN(y + h, iso.shown_b);
	if (x1 <= x0) return;
	for (row = y0; row < y1; row++)
		memcpy(iso.shown_pix + ((size_t)(row - iso.shown_tp)
				* (iso.shown_r - iso.shown_l) + (x0 - iso.shown_l)) * 4,
			map->pix + ((size_t)row * map->pix_w + x0) * 4,
			(size_t)(x1 - x0) * 4);
}

/* Something was drawn on, or cleared from, this cell of the map term. */
void tcl_iso_touched(const term_data *map, int x, int y)
{
	if (tcl_iso_map_cell(map, x, y)) iso.dirty = true;
}
