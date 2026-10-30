/**
 * \file tcl/main-tcl.h
 * \brief What the Tcl/Tk front end's files share.
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

#ifndef INCLUDED_TCL_MAIN_TCL_H
#define INCLUDED_TCL_MAIN_TCL_H

#include "ui-term.h"
#include <tk.h>

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

	/*
	 * The tile layer's pixels, as the photo holds them, drawn here by
	 * blit_tile() and clear_tiles() and handed to the photo by tiles_flush()
	 * in one block: see there for why.  dirty_* is the rectangle drawn
	 * since, in pixels; empty when dirty_x1 is not past dirty_x0.
	 */
	unsigned char *pix;	/* pix_w * pix_h * 4, RGBA */
	int pix_w, pix_h;
	int dirty_x0, dirty_y0, dirty_x1, dirty_y1;

	/* Per cell: whether its text item is drawn, or hidden as blank. */
	unsigned char *shown;

	/* Per cell: the colour its text item was last given, or -1 if not known. */
	short *fill;
};

/*
 * main-tcl.c's, lent to tcl/iso.c: a cell's text, the rectangle of the pane
 * to hand to Tk at the next flush, the PNG reader, and the current tile set's
 * sheet as RGBA pixels (NULL when the game draws text).
 */
void tcl_cell_set(term_data *t, int x, int y, const char *ch, int attr);
void tcl_tiles_dirty(term_data *t, int x, int y, int w, int h);
bool tcl_png_load(const char *path, unsigned char **out, int *w, int *h);
const unsigned char *tcl_tile_sheet(int *w, int *h);

/* tcl/iso.c: the isometric map view.  Each takes the map's term. */
bool tcl_iso_on(void);
const char *tcl_iso_style(void);
bool tcl_iso_set(const char *style, const term_data *map, char *err,
		size_t len);
void tcl_iso_redraw(void);
void tcl_iso_tiles_changed(int graf_id);
bool tcl_iso_map_cell(const term_data *map, int x, int y);
bool tcl_iso_point(const term_data *map, double px, double py, double *fx,
		double *fy);
void tcl_iso_cursor(const term_data *map, int x, int y);
void tcl_iso_cursor_show(const term_data *map, bool show);
void tcl_iso_events_init(void);
void tcl_iso_flush(term_data *map);
void tcl_iso_touched(const term_data *map, int x, int y);

#endif /* INCLUDED_TCL_MAIN_TCL_H */
