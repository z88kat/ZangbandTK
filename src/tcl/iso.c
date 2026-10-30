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
 * square a diamond, walls standing up off the floor, the character in the
 * middle.  None of that version's code or art is used.  The terrain comes
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
 * rectangle.
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
#include "ui-map.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-term.h"
#include "tcl/main-tcl.h"

enum { ISO_NONE, ISO_TILE, ISO_WALL, ISO_DOOR, ISO_SHOP };

#define ISO_HW 32		/* half a diamond's width */
#define ISO_HH 16		/* half its height */
#define ISO_FADE 120		/* of 256: how far a remembered square fades */

static struct {
	bool on;
	bool dirty;
	char style[16];
	unsigned char *lit, *dim;	/* the sheet as seen, and as remembered */
	int w, h;
	int tw, th, cols;
	uint8_t kind[FEAT_MAX];
	int cell[FEAT_MAX];
	int cut[FEAT_MAX];		/* cut away, in front of the character */
	int wall[FEAT_MAX], wall_cut[FEAT_MAX];	/* a shop's building */
	/* On the surface, where permanent walls are buildings and not vaults. */
	uint8_t skind[FEAT_MAX];
	int scell[FEAT_MAX], scut[FEAT_MAX];
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
	 * view centred on the character would not show.  shown is the middle of
	 * the view as last drawn, which is what a point on the pane is measured
	 * from.
	 */
	bool cursor_on, cursor_map, held;
	struct loc cursor, hold, shown;
	/* The game's answers to "what is on screen?", given back when the view goes. */
	void (*old_get_panel)(int *min_y, int *min_x, int *max_y, int *max_x);
	bool (*old_panel_contains)(unsigned int y, unsigned int x);
	/* The letters for counters: white on clear, side by side. */
	unsigned char *glyph;
	int gw, gh, gfirst, gcount, gstride;
	/* Per cell of the map term: the map's own letter, drawn as a counter. */
	unsigned char *letter;
	size_t letter_n;
} iso;

static void iso_get_panel(int *min_y, int *min_x, int *max_y, int *max_x);
static bool iso_panel_contains(unsigned int y, unsigned int x);

static void iso_free(void)
{
	mem_free(iso.lit);
	mem_free(iso.dim);
	mem_free(iso.glyph);
	iso.lit = iso.dim = iso.glyph = NULL;
	iso.on = false;
	iso.held = false;
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
		 * feat:<terrain>:<kind>:<cell>:<cut away>, or for a shop
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

			if (!f_info[i].name || !streq(f_info[i].name, field[1]))
				continue;
			if (surface) {
				iso.skind[i] = (uint8_t)kind;
				iso.scell[i] = cell;
				iso.scut[i] = cut;
				continue;
			}
			iso.kind[i] = (uint8_t)kind;
			iso.cell[i] = cell;
			iso.cut[i] = cut;
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

	if (!iso.on) return false;
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

/* Walls join up with walls, and a shop's door is part of its building's. */
static bool iso_is_wall(struct loc grid)
{
	int kind = iso.kind[iso_known_feat(grid)];

	return kind == ISO_WALL || kind == ISO_SHOP;
}

static void iso_blit(term_data *t, const unsigned char *src, int cell,
		int ox, int oy, int l, int tp, int r, int b)
{
	int sx = (cell % iso.cols) * iso.tw, sy = (cell / iso.cols) * iso.th;
	int x, y;

	if (sx + iso.tw > iso.w || sy + iso.th > iso.h) return;
	for (y = 0; y < iso.th; y++) {
		const unsigned char *s;

		if (oy + y < tp || oy + y >= b) continue;
		s = src + ((size_t)(sy + y) * iso.w + sx) * 4;
		for (x = 0; x < iso.tw; x++, s += 4) {
			unsigned char *d;

			if (!s[3] || ox + x < l || ox + x >= r) continue;
			d = t->pix + ((size_t)(oy + y) * t->pix_w + ox + x) * 4;
			d[0] = s[0];
			d[1] = s[1];
			d[2] = s[2];
			d[3] = 255;
		}
	}
}

/*
 * A tile of the current set, stood on the diamond whose middle is (cx, cy),
 * 32 pixels whatever the set's own size.  A set with no transparency -- Neon
 * draws on black -- has its black taken as the background.
 *
 * Every figure is outlined, one pixel all round.  The sets were drawn to be
 * seen on black, and on paper their palest figures -- all of Neon, and any
 * set's white monsters -- otherwise all but vanish.  A set drawn on black
 * keeps a rim of its own black, like a sign cut out of its board; the rest
 * are outlined in the style's ink.
 */
static void iso_sprite(term_data *t, int a, int c, int cx, int cy, bool dim,
		int l, int tp, int r, int b)
{
	int tw, th, sx, sy, x, y, k, ox = cx - 16, oy = cy + 6 - 32;
	static const unsigned char *checked = NULL;
	static bool opaque = false;
	bool shape[34][34];
	unsigned char edge[3];
	int sheet_w, sheet_h;
	const unsigned char *sheet = tcl_tile_sheet(&sheet_w, &sheet_h);

	if (!sheet || !current_graphics_mode) return;
	tw = current_graphics_mode->cell_width;
	th = current_graphics_mode->cell_height;
	sx = (c & 0x7f) * tw;
	sy = (a & 0x7f) * th;
	if (sx + tw > sheet_w || sy + th > sheet_h) return;

	if (checked != sheet) {
		size_t i, n = (size_t)sheet_w * sheet_h;

		opaque = true;
		for (i = 0; i < n && opaque; i++)
			if (sheet[i * 4 + 3] != 255) opaque = false;
		checked = sheet;
	}

	/* Where the figure is, with a pixel's margin all round for its outline. */
	memset(shape, 0, sizeof(shape));
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			const unsigned char *s = sheet + ((size_t)(sy + y * th / 32)
				* sheet_w + sx + x * tw / 32) * 4;

			shape[y + 1][x + 1] = opaque ? s[0] + s[1] + s[2] >= 48
				: s[3] >= 128;
		}

	for (k = 0; k < 3; k++) {
		edge[k] = opaque ? 0 : iso.ink[k];
		if (dim)
			edge[k] = (unsigned char)(edge[k]
				+ ((iso.paper[k] - edge[k]) * ISO_FADE >> 8));
	}

	for (y = 0; y < 34; y++) {
		int py = oy + y - 1;

		if (py < tp || py >= b) continue;
		for (x = 0; x < 34; x++) {
			int px = ox + x - 1;
			unsigned char *d;

			if (px < l || px >= r) continue;
			d = t->pix + ((size_t)py * t->pix_w + px) * 4;
			if (shape[y][x]) {
				const unsigned char *s = sheet + ((size_t)(sy
					+ (y - 1) * th / 32) * sheet_w + sx
					+ (x - 1) * tw / 32) * 4;

				for (k = 0; k < 3; k++)
					d[k] = dim ? (unsigned char)(s[k]
						+ ((iso.paper[k] - s[k]) * ISO_FADE >> 8)) : s[k];
				d[3] = 255;
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

/* What the map says stands on a square: a, c over ta, tc. */
static void iso_map_says(struct loc grid, int *a, int *c, int *ta, int *tc)
{
	struct grid_data g;
	wchar_t ch, tch;

	map_info(grid, &g);
	grid_data_as_text(&g, a, &ch, ta, &tch);
	*c = ch;
	*tc = tch;
}

/*
 * What stands on a square, if anything: a tile drawn over its terrain, or a
 * letter, which is how the game draws what the tile set has no picture for.
 *
 * Inside the term's rectangle it is read from the term, where the game drew
 * it -- which is how a bolt in flight gets here.  A letter there counts only
 * if it is the one the map would draw: anything else is text the game has
 * written over the square, and the map's answer is used instead.  *cell is
 * the term cell a letter of the map's own came from, or -1.
 */
static int iso_fg(const term_data *t, struct loc grid, int *a, int *c,
		int *cell)
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

		*a = tm->scr->a[y][x];
		*c = tm->scr->c[y][x];
		ta = tm->scr->ta[y][x];
		tc = tm->scr->tc[y][x];
		if (!(*a & 0x80)) {
			int ma, mc;

			/*
			 * Text the map would not draw is something drawn over the
			 * square -- the path an aimed bolt would take -- which the
			 * view draws over it in turn; what stands there is still the
			 * map's.
			 */
			iso_map_says(grid, &ma, &mc, &ta, &tc);
			if (ma != *a || mc != *c) {
				*a = ma;
				*c = mc;
			} else {
				*cell = y * t->cols + x;
			}
		}
	} else {
		iso_map_says(grid, a, c, &ta, &tc);
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
 * A character of the game's, at twice the font's size, its top left at
 * (x0, y0): in the game's colour attr, at bright/256 of its brightness, and
 * faded towards the paper when what it stands for is only remembered.
 */
static void iso_glyph(term_data *t, int ch, int attr, int x0, int y0,
		int bright, bool dim, int l, int tp, int r, int b)
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
	for (y = 0; y < iso.gh * 2; y++) {
		if (y0 + y < tp || y0 + y >= b) continue;
		for (x = 0; x < iso.gw * 2; x++) {
			const unsigned char *s = iso.glyph + ((size_t)(y / 2)
				* iso.gstride + (ch - iso.gfirst) * iso.gw + x / 2) * 4;
			unsigned char *d;

			if (s[3] < 128 || x0 + x < l || x0 + x >= r) continue;
			d = t->pix + ((size_t)(y0 + y) * t->pix_w + x0 + x) * 4;
			memcpy(d, rgb, 3);
			d[3] = 255;
		}
	}
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
 * The characters the map shows on a square, a and c over the terrain's ta and
 * tc: from the term where the game drew them, which brings bolts with them,
 * and from the map beyond its rectangle.
 */
static void iso_text_of(const term_data *t, struct loc grid, int *a, int *c,
		int *ta, int *tc)
{
	const term *tm = &t->t;
	int left = col_map[tm->sidebar_mode], top = row_top_map[tm->sidebar_mode];
	int rows = (tm->hgt - top - row_bottom_map[tm->sidebar_mode]) / tile_height;
	int cols = (tm->wid - left) / tile_width;
	int gx = grid.x - tm->offset_x, gy = grid.y - tm->offset_y;

	if (tm->scr && gx >= 0 && gy >= 0 && gx < cols && gy < rows) {
		int x = left + gx * tile_width, y = top + gy * tile_height;

		*a = tm->scr->a[y][x];
		*c = tm->scr->c[y][x];
		*ta = tm->scr->ta[y][x];
		*tc = tm->scr->tc[y][x];
	} else {
		iso_map_says(grid, a, c, ta, tc);
	}
}

/*
 * A square of the ASCII view: its terrain's character, stood up as high as
 * the terrain is -- a wall three deep, darkest at the foot, so that a run of
 * them reads as height -- and on it whatever the map shows there, raised.
 * Floors sit back, dim, so that what stands on them does not.
 */
static void iso_text_square(term_data *t, struct loc grid, int cx, int cy,
		bool seen, bool bright, bool cut, int l, int tp, int r, int b)
{
	static const int shade3[] = { 115, 180, 256 }, shade2[] = { 154, 256 };
	int a, c, ta, tc, feat = iso_known_feat(grid), n = 0, i;
	int x0 = cx - iso.gw, y0 = cy - iso.gh;

	iso_text_of(t, grid, &a, &c, &ta, &tc);

	if (feat != FEAT_NONE) {
		const struct feature *f = &f_info[feat];

		if (tf_has(f->flags, TF_WALL) || iso.kind[feat] == ISO_SHOP) n = 3;
		else if (!tf_has(f->flags, TF_PASSABLE)) n = 2;
		else n = 1;
		if (cut) n = 1;
		for (i = 0; i < n; i++) {
			int k = (n == 3) ? shade3[i] : (n == 2) ? shade2[i]
				: tf_has(f->flags, TF_FLOOR) ? 115 : 256;

			iso_glyph(t, tc, ta, x0, y0 - i * iso.rise, k, !bright,
				l, tp, r, b);
		}
	}

	if (a != ta || c != tc)
		iso_glyph(t, c, a, x0, y0 - (n ? n : 1) * iso.rise, 256, !seen,
			l, tp, r, b);
}

/* The square in the middle of the view: the character's, unless held. */
static struct loc iso_centre(void)
{
	return iso.held ? iso.hold : player->grid;
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
	int vx, vy, cx, cy, dx, dy, ll, ttp, rr, bb;

	if (!tm->scr || !character_dungeon || !cave || !player) return false;
	grid.x = (x - col_map[tm->sidebar_mode]) / tile_width + tm->offset_x;
	grid.y = (y - row_top_map[tm->sidebar_mode]) / tile_height + tm->offset_y;
	if (!square_in_bounds(cave, grid) || !iso_shows(t, centre, grid, 0))
		return false;
	/* The ASCII view has already drawn what the term holds there. */
	if (iso.text) return true;
	iso_view(t, &ll, &ttp, &rr, &bb, &vx, &vy);
	dx = grid.x - centre.x;
	dy = grid.y - centre.y;
	cx = vx + (dx - dy) * iso.hw;
	cy = vy + (dx + dy) * iso.hh;
	iso_glyph(t, tm->scr->c[y][x], tm->scr->a[y][x], cx - iso.gw,
		cy - iso.gh - 8, 256, false, l, tp, r, b);
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

static void iso_render(term_data *t)
{
	int l, tp, r, b, vx, vy, s, d, x, y, smax, dmax;
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
	 * yet loaded -- there is no map to draw, and what the term holds is text:
	 * shown as the flat map shows it, on black, and not on paper with a black
	 * box behind each letter.
	 */
	if (!character_dungeon || !player || !cave || !player->cave) {
		for (y = tp; y < b; y++)
			memset(t->pix + ((size_t)y * t->pix_w + l) * 4, 0,
				(size_t)(r - l) * 4);
		tcl_tiles_dirty(t, l, tp, r - l, b - tp);
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
			int dx, dy, cx, cy, feat, a, c, fg, cell, pdx, pdy;
			bool seen, bright, cut;

			if ((s + d) & 1) continue;
			dx = (s + d) / 2;
			dy = (s - d) / 2;
			grid = loc(centre.x + dx, centre.y + dy);
			if (!square_in_bounds(cave, grid)) continue;

			/*
			 * The three squares whose walls would stand in front of
			 * the character are cut down to their footings.
			 */
			pdx = grid.x - player->grid.x;
			pdy = grid.y - player->grid.y;
			cut = pdx >= 0 && pdy >= 0 && pdx + pdy > 0 && pdx < 2
				&& pdy < 2;

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
				int mask = iso_is_wall(loc(grid.x - 1, grid.y)) * 1
					| iso_is_wall(loc(grid.x, grid.y - 1)) * 2
					| iso_is_wall(loc(grid.x + 1, grid.y)) * 4
					| iso_is_wall(loc(grid.x, grid.y + 1)) * 8;
				bool we = mask & (1 | 4), ns = mask & (2 | 8);
				int ox = cx - iso.tw / 2, oy = cy + ISO_HH - iso.th;

				if (!player->depth && iso.skind[feat]) {
					kind = iso.skind[feat];
					cell = cut ? iso.scut[feat] : iso.scell[feat];
				}

				if (kind == ISO_WALL) {
					iso_blit(t, src, cell + mask, ox, oy, l, tp, r, b);
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
						: iso.wall[feat]) + mask, ox, oy, l, tp, r, b);
					if (!cut)
						iso_blit(t, src, iso.cell[feat] + side, ox, oy,
							l, tp, r, b);
				} else {
					iso_blit(t, src, cell, ox, oy, l, tp, r, b);
				}
			}

			fg = iso_fg(t, grid, &a, &c, &cell);
			if (fg == ISO_FG_TILE) {
				iso_sprite(t, a, c, cx, cy, !seen, l, tp, r, b);
			} else if (fg == ISO_FG_LETTER) {
				iso_counter(t, a, c, cx, cy, !seen, l, tp, r, b);
				if (cell >= 0) iso.letter[cell] = 1;
			}
		}
	}

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

	tcl_tiles_dirty(t, l, tp, r - l, b - tp);
}

/*
 * Keep the cursor on the view: hold the view on it when the view centred on
 * the character would not show it, and let go when that view would.
 */
static void iso_follow(const term_data *map)
{
	if (!iso.cursor_on || !iso.cursor_map || !player || !cave
			|| !square_in_bounds(cave, iso.cursor))
		return;
	if (iso_shows(map, iso_centre(), iso.cursor, iso.hw)) return;
	if (iso_shows(map, player->grid, iso.cursor, iso.hw)) {
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
 * Something on the level changed, or the character moved.  When it moved,
 * the term's rectangle is centred on it again, as center_player has the flat
 * map do: a bolt is drawn only inside that rectangle, and the view, which is
 * centred on the character, shows what the term holds.
 */
static void iso_notice(game_event_type type, game_event_data *data, void *user)
{
	(void)data;
	(void)user;
	iso.dirty = true;
	if (type == EVENT_PLAYERMOVED && iso.on && !iso.held) center_panel();
}

/*
 * The game says when a square changes, or the character moves, wherever it
 * is -- not only inside the term's rectangle, which is all the term hears of.
 */
void tcl_iso_events_init(void)
{
	event_add_handler(EVENT_MAP, iso_notice, NULL);
	event_add_handler(EVENT_PLAYERMOVED, iso_notice, NULL);
}

/* The map term is about to be handed to Tk: draw the view, if it changed. */
void tcl_iso_flush(term_data *map)
{
	if (iso.on && iso.dirty) {
		iso.dirty = false;
		iso_render(map);
	}
}

/* Something was drawn on, or cleared from, this cell of the map term. */
void tcl_iso_touched(const term_data *map, int x, int y)
{
	if (tcl_iso_map_cell(map, x, y)) iso.dirty = true;
}
