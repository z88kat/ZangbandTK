/**
 * \file ui-map.c
 * \brief Writing level map info to the screen
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "dun-type.h"
#include "cave.h"
#include "grafmode.h"
#include "init.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-timed.h"
#include "trap.h"
#include "ui-map.h"
#include "effect-handler.h"
#include "effects.h"
#include "game-world.h"
#include "player-calcs.h"
#include "player-mutation.h"
#include "player-quest.h"
#include "player-util.h"
#include "game-input.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "wild.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-target.h"
#include "ui-term.h"


/**
 * Hallucinatory monster
 */
static void hallucinatory_monster(int *a, wchar_t *c)
{
	while (1) {
		/* Select a random monster */
		struct monster_race *race = &r_info[randint0(z_info->r_max)];
		
		/* Skip non-entries */
		if (!race->name) continue;
		
		/* Retrieve attr/char */
		*a = monster_x_attr[race->ridx];
		*c = monster_x_char[race->ridx];
		return;
	}
}


/**
 * Hallucinatory object
 */
static void hallucinatory_object(int *a, wchar_t *c)
{
	
	while (1) {
		/* Select a random object */
		struct object_kind *kind = &k_info[randint0(z_info->k_max - 1) + 1];

		/* Skip non-entries */
		if (!kind->name) continue;
		
		/* Retrieve attr/char (HACK - without flavors) */
		*a = kind_x_attr[kind->kidx];
		*c = kind_x_char[kind->kidx];
		
		/* HACK - Skip empty entries */
		if (*a == 0 || *c == 0) continue;

		return;
	}
}


/**
 * Get the graphics of a listed trap.
 *
 * We should probably have better handling of stacked traps, but that can
 * wait until we do, in fact, have stacked traps under normal conditions.
 * Return true if it's a web
 */
static bool get_trap_graphics(struct chunk *c, struct grid_data *g, int *a,
							  wchar_t *w)
{
    /* Trap is visible */
    if (trf_has(g->trap->flags, TRF_VISIBLE) ||
		trf_has(g->trap->flags, TRF_GLYPH) ||
		trf_has(g->trap->flags, TRF_WEB)) {
		/* Get the graphics */
		*a = trap_x_attr[g->lighting][g->trap->kind->tidx];
		*w = trap_x_char[g->lighting][g->trap->kind->tidx];
    }

	return trf_has(g->trap->flags, TRF_WEB);
}

/**
 * Apply text lighting effects
 */
static void grid_get_attr(struct grid_data *g, int *a)
{
	/* Save the high-bit, since it's used for attr inversion in GCU */
	int a0 = *a & 0x80;

	/* Remove the high bit so we can add it back again at the end */
	*a = (*a & 0x7F);

	/* Play with fg colours for terrain affected by torchlight */
	if (feat_is_torch(g->f_idx)) {
		/* Brighten if torchlit, darken if out of LoS, super dark for UNLIGHT */
		switch (g->lighting) {
			case LIGHTING_TORCH: *a = get_color(*a, ATTR_LIGHT, 1); break;
			case LIGHTING_LIT: *a = get_color(*a, ATTR_DARK, 1); break;
			case LIGHTING_DARK: *a = get_color(*a, ATTR_DARK, 2); break;
			default: break;
		}
	}

	/* Add the attr inversion back for GCU */
	if (a0) {
		*a = a0 | *a;
	}
	/* Hybrid or block walls */
	if (use_graphics == GRAPHICS_NONE && feat_is_wall(g->f_idx)) {
		if (OPT(player, hybrid_walls))
			*a = *a + (MULT_BG * BG_DARK);
		else if (OPT(player, solid_walls))
			*a = *a + (MULT_BG * BG_SAME);
	}
}

/**
 * This function takes a pointer to a grid info struct describing the 
 * contents of a grid location (as obtained through the function map_info)
 * and fills in the character and attr pairs for display.
 *
 * ap and cp are filled with the attr/char pair for the monster, object or 
 * floor tile that is at the "top" of the grid (monsters covering objects, 
 * which cover floor, assuming all are present).
 *
 * tap and tcp are filled with the attr/char pair for the floor, regardless
 * of what is on it.  This can be used by graphical displays with
 * transparency to place an object onto a floor tile, is desired.
 *
 * Any lighting effects are also applied to these pairs, clear monsters allow
 * the underlying colour or feature to show through (ATTR_CLEAR and
 * CHAR_CLEAR), multi-hued colour-changing (ATTR_MULTI) is applied, and so on.
 * Technically, the flag "CHAR_MULTI" is supposed to indicate that a monster 
 * looks strange when examined, but this flag is currently ignored.
 *
 * NOTES:
 * This is called pretty frequently, whenever a grid on the map display
 * needs updating, so don't overcomplicate it.
 *
 * The "zero" entry in the feature/object/monster arrays are
 * used to provide "special" attr/char codes, with "monster zero" being
 * used for the player attr/char, "object zero" being used for the "pile"
 * attr/char, and "feature zero" being used for the "darkness" attr/char.
 *
 * TODO:
 * The transformations for tile colors, or brightness for the 16x16
 * tiles should be handled differently.  One possibility would be to
 * extend feature_type with attr/char definitions for the different states.
 * This will probably be done outside of the current text->graphics mappings
 * though.
 */
void grid_data_as_text(struct grid_data *g, int *ap, wchar_t *cp, int *tap,
					   wchar_t *tcp)
{
	struct feature *feat = &f_info[g->f_idx];

	int a = feat_x_attr[g->lighting][feat->fidx];
	wchar_t c = feat_x_char[g->lighting][feat->fidx];
	bool skip_objects = false;

	/* Get the colour for ASCII */
	if (use_graphics == GRAPHICS_NONE)
		grid_get_attr(g, &a);

	/* Save the terrain info for the transparency effects */
	(*tap) = a;
	(*tcp) = c;

	/* There is a trap in this grid, and we are not hallucinating */
	if (g->trap && (!g->hallucinate)) {
	    /* Change graphics to indicate visible traps, skip objects if a web */
	    skip_objects = get_trap_graphics(cave, g, &a, &c);
	}

	if (!skip_objects) {
		/* If there's an object, deal with that. */
		if (g->unseen_money) {

			/* $$$ gets an orange star*/
			a = object_kind_attr(unknown_gold_kind);
			c = object_kind_char(unknown_gold_kind);

		} else if (g->unseen_object) {

			/* Everything else gets a red star */
			a = object_kind_attr(unknown_item_kind);
			c = object_kind_char(unknown_item_kind);

		} else if (g->first_kind) {
			if (g->hallucinate) {
				/* Just pick a random object to display. */
				hallucinatory_object(&a, &c);
			} else if (g->multiple_objects) {
				/* Get the "pile" feature instead */
				a = object_kind_attr(pile_kind);
				c = object_kind_char(pile_kind);
			} else {
				/* Normal attr and char */
				a = object_kind_attr(g->first_kind);
				c = object_kind_char(g->first_kind);
			}
		}
	}

	/* Handle monsters, the player and trap borders */
	if (g->m_idx > 0) {
		if (g->hallucinate) {
			/* Just pick a random monster to display. */
			hallucinatory_monster(&a, &c);
		} else if (!monster_is_camouflaged(cave_monster(cave, g->m_idx)))	{
			struct monster *mon = cave_monster(cave, g->m_idx);

			uint8_t da;
			wchar_t dc;

			/* Desired attr & char */
			da = monster_x_attr[mon->race->ridx];
			dc = monster_x_char[mon->race->ridx];

			/* Special handling of attrs and/or chars */
			if (da & 0x80) {
				/* Special attr/char codes */
				a = da;
				c = dc;
			} else if (OPT(player, purple_uniques) && 
					monster_is_shape_unique(mon)) {
				/* Turn uniques purple if desired (violet, actually) */
				a = COLOUR_VIOLET;
				c = dc;
			} else if (rf_has(mon->race->flags, RF_ATTR_MULTI) ||
					   rf_has(mon->race->flags, RF_ATTR_FLICKER) ||
					   rf_has(mon->race->flags, RF_ATTR_RAND)) {
				/* Multi-hued monster */
				a = mon->attr ? mon->attr : da;
				c = dc;

				/*
				 * ZangbandTK (CNT-04): and a shapechanger shows as something
				 * else entirely, redrawn each time it shimmers.
				 *
				 * Nested inside the multi-hued branch because that is where
				 * Zangband put it: SHAPECHANGER on its own does nothing at all,
				 * it only modifies a monster that was already changing colour
				 * ([maid-grf.c:1923](../archive/zangband/src/maid-grf.c#L1923)).
				 * All five of the original's shapechangers carry ATTR_MULTI, so
				 * the dependency never showed.
				 *
				 * One glyph in twenty-five is an object rather than a monster,
				 * which is the original's proportion and the thing that makes it
				 * unsettling rather than merely busy.  Only the character is
				 * taken; the colour is left to the multi-hued cycling above, as
				 * it was in the original's text mode.
				 */
				if (rf_has(mon->race->flags, RF_SHAPECHANGER)) {
					int sa;
					wchar_t sc;

					if (one_in_(25)) {
						hallucinatory_object(&sa, &sc);
					} else {
						hallucinatory_monster(&sa, &sc);
					}

					c = sc;
				}
			} else if (!flags_test(mon->race->flags, RF_SIZE,
								   RF_ATTR_CLEAR, RF_CHAR_CLEAR, FLAG_END)) {
				/* Normal monster (not "clear" in any way) */
				a = da;
				/* Desired attr & char. da is not used, should a be set to it?*/
				/*da = monster_x_attr[mon->race->ridx];*/
				dc = monster_x_char[mon->race->ridx];
				c = dc;
			} else if (a & 0x80) {
				/* Bizarre grid under monster */
				a = da;
				c = dc;
			} else if (!rf_has(mon->race->flags, RF_CHAR_CLEAR)) {
				/* Normal char, Clear attr, monster */
				c = dc;
			} else if (!rf_has(mon->race->flags, RF_ATTR_CLEAR)) {
				/* Normal attr, Clear char, monster */
				a = da;
			}

			/* Store the drawing attr so we can use it elsewhere */
			mon->attr = a;
		}
	} else if (g->is_player) {
		struct monster_race *race = &r_info[0];

		/* Get the "player" attr */
		a = monster_x_attr[race->ridx];
		if ((OPT(player, hp_changes_color)) && !(a & 0x80)) {
			switch(player->chp * 10 / player->mhp)
			{
			case 10:
			case  9: 
			{
				a = COLOUR_WHITE; 
				break;
			}
			case  8:
			case  7:
			{
				a = COLOUR_YELLOW;
				break;
			}
			case  6:
			case  5:
			{
				a = COLOUR_ORANGE;
				break;
			}
			case  4:
			case  3:
			{
				a = COLOUR_L_RED;
				break;
			}
			case  2:
			case  1:
			case  0:
			{
				a = COLOUR_RED;
				break;
			}
			default:
			{
				a = COLOUR_WHITE;
				break;
			}
			}
		}

		/* Get the "player" char */
		c = monster_x_char[race->ridx];
	}

	/* Result */
	(*ap) = a;
	(*cp) = c;
}


/**
 * Get dimensions of a small-scale map (i.e. display_map()'s result).
 * \param t Is the terminal displaying the map.
 * \param c Is the chunk to display.
 * \param tw Is the tile width in characters.
 * \param th Is the tile height in characters.
 * \param mw *mw is set to the width of the small-scale map.
 * \param mh *mh is set to the height of the small-scale map.
 */
static void get_minimap_dimensions(term *t, const struct chunk *c,
	int tw, int th, int *mw, int *mh)
{
	int map_height = t->hgt - 2;
	int map_width = t->wid - 2;
	int cave_height = c->height;
	int cave_width = c->width;
	int remainder;

	if (th > 1) {
		/*
		 * Round cave height up to a multiple of the tile height
		 * (ideally want no information truncated).
		 */
		remainder = cave_height % th;
		if (remainder > 0) {
			cave_height += th - remainder;
		}

		/*
		 * Round map height down to a multiple of the tile height
		 * (don't want partial tiles overwriting the map borders).
		 */
		map_height -= map_height % th;
	}
	if (tw > 1) {
		/* As above but for the width. */
		remainder = cave_width % tw;
		if (remainder > 0) {
			cave_width += tw - remainder;
		}
		map_width -= map_width % tw;
	}

	*mh = MIN(map_height, cave_height);
	*mw = MIN(map_width, cave_width);
}


/**
 * Move the cursor to a given map location.
 */
static void move_cursor_relative_map(int y, int x)
{
	int ky, kx;

	term *old;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if (!(window_flag[j] & (PW_MAPS))) continue;

		if (window_flag[j] & PW_MAP) {
			/* Be consistent with display_map(). */
			int map_width, map_height;

			get_minimap_dimensions(t, cave, tile_width,
				tile_height, &map_width, &map_height);

			ky = (y * map_height) / cave->height;
			if (tile_height > 1) {
				ky = ky - (ky % tile_height) + 1;
			} else {
				++ky;
			}
			kx = (x * map_width) / cave->width;
			if (tile_width > 1) {
				kx = kx - (kx % tile_width) + 1;
			} else {
				++kx;
			}
		} else {
			/* Location relative to panel */
			ky = y - t->offset_y;
			if (tile_height > 1)
				ky = tile_height * ky;

			kx = x - t->offset_x;
			if (tile_width > 1)
				kx = tile_width * kx;
		}

		/* Verify location */
		if ((ky < 0) || (ky >= t->hgt)) continue;
		if ((kx < 0) || (kx >= t->wid)) continue;

		/* Go there */
		old = Term;
		Term_activate(t);
		(void)Term_gotoxy(kx, ky);
		Term_activate(old);
	}
}


/**
 * Move the cursor to a given map location.
 *
 * The main screen will always be at least 24x80 in size.
 */
void move_cursor_relative(int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Move the cursor on map sub-windows */
	move_cursor_relative_map(y, x);

	/* Location relative to panel */
	ky = y - Term->offset_y;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT)) return;

	/* Location relative to panel */
	kx = x - Term->offset_x;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID)) return;

	/* Location in window */
	vy = ky + ROW_MAP;

	/* Location in window */
	vx = kx + COL_MAP;

	if (tile_width > 1)
		vx += (tile_width - 1) * kx;

	if (tile_height > 1)
		vy += (tile_height - 1) * ky;

	/* Go there */
	(void)Term_gotoxy(vx, vy);
}


/**
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 */
static void print_rel_map(wchar_t c, uint8_t a, int y, int x)
{
	int ky, kx;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if (!(window_flag[j] & (PW_MAPS))) continue;

		if (window_flag[j] & PW_MAP) {
			/* Be consistent with display_map(). */
			int map_width, map_height;

			get_minimap_dimensions(t, cave, tile_width,
				tile_height, &map_width, &map_height);

			kx = (x * map_width) / cave->width;
			ky = (y * map_height) / cave->height;
			if (tile_width > 1) {
				kx = kx - (kx % tile_width) + 1;
			} else {
				++kx;
			}
			if (tile_height > 1) {
				ky = ky - (ky % tile_height) + 1;
			} else {
				++ky;
			}
		} else {
			/* Location relative to panel */
			ky = y - t->offset_y;

			if (tile_height > 1) {
				ky = tile_height * ky;
				if (ky + 1 >= t->hgt) continue;
			}

			kx = x - t->offset_x;

			if (tile_width > 1) {
				kx = tile_width * kx;
				if (kx + 1 >= t->wid) continue;
			}
		}

		/* Verify location */
		if ((ky < 0) || (ky >= t->hgt)) continue;
		if ((kx < 0) || (kx >= t->wid)) continue;

		/* Queue it */
		Term_queue_char(t, kx, ky, a, c, 0, 0);

		if ((tile_width > 1) || (tile_height > 1))
			/*
			 * The overhead view can make use of the last row in
			 * the terminal.  Others leave it be.
			 */
			Term_big_queue_char(t, kx, ky, t->hgt -
				((window_flag[j] & PW_OVERHEAD) ? 0 : ROW_BOTTOM_MAP),
				a, c, 0, 0);
	}
}



/**
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void print_rel(wchar_t c, uint8_t a, int y, int x)
{
	int ky, kx;
	int vy, vx;

	/* Print on map sub-windows */
	print_rel_map(c, a, y, x);

	/* Location relative to panel */
	ky = y - Term->offset_y;

	/* Verify location */
	if ((ky < 0) || (ky >= SCREEN_HGT)) return;

	/* Location relative to panel */
	kx = x - Term->offset_x;

	/* Verify location */
	if ((kx < 0) || (kx >= SCREEN_WID)) return;

	/* Get right position */
	vx = COL_MAP + (tile_width * kx);
	vy = ROW_MAP + (tile_height * ky);

	/* Queue it */
	Term_queue_char(Term, vx, vy, a, c, 0, 0);

	if ((tile_width > 1) || (tile_height > 1))
		Term_big_queue_char(Term, vx, vy, ROW_MAP + SCREEN_ROWS,
			a, c, 0, 0);
  
}


static void prt_map_aux(void)
{
	int a, ta;
	wchar_t c, tc;
	struct grid_data g;

	int y, x;
	int vy, vx;
	int ty, tx;

	int j;

	/* Scan windows */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];
		int clipy;

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if (!(window_flag[j] & (PW_MAPS))) continue;

		if (window_flag[j] & PW_MAP) {
			term *old = Term;

			Term_activate(t);
			display_map(NULL, NULL);
			Term_activate(old);
			continue;
		}

		/* Assume screen */
		ty = t->offset_y + (t->hgt / tile_height);
		tx = t->offset_x + (t->wid / tile_width);

		/*
		 * The overhead view can use the last row of the terminal.
		 * Others can not.
		 */
		clipy = t->hgt - ((window_flag[j] & PW_OVERHEAD) ? 0 : ROW_BOTTOM_MAP);

		/* Dump the map */
		for (y = t->offset_y, vy = 0; y < ty; vy += tile_height, y++) {
			for (x = t->offset_x, vx = 0; x < tx; vx += tile_width, x++) {
				/* Check bounds */
				if (!square_in_bounds(cave, loc(x, y))) {
					Term_queue_char(t, vx, vy,
						COLOUR_WHITE, ' ',
						0, 0);
					if (tile_width > 1 || tile_height > 1) {
						Term_big_queue_char(t, vx, vy,
							clipy, COLOUR_WHITE, ' ', 0, 0);
					}
					continue;
				}

				/* Determine what is there */
				map_info(loc(x, y), &g);
				grid_data_as_text(&g, &a, &c, &ta, &tc);
				Term_queue_char(t, vx, vy, a, c, ta, tc);

				if ((tile_width > 1) || (tile_height > 1))
					Term_big_queue_char(t, vx, vy, clipy,
						255, -1, 0, 0);
			}
			/* Clear partial tile at the end of each line. */
			for (; vx < t->wid; ++vx) {
				Term_queue_char(t, vx, vy, COLOUR_WHITE,
					' ', 0, 0);
			}
		}
		/* Clear row of partial tiles at the bottom. */
		for (; vy < t->hgt; ++vy) {
			for (vx = 0; vx < t->wid; ++vx) {
				Term_queue_char(t, vx, vy, COLOUR_WHITE,
					' ', 0, 0);
			}
		}
	}
}



/**
 * Redraw (on the screen) the current map panel
 *
 * Note the inline use of "light_spot()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void prt_map(void)
{
	int a, ta;
	wchar_t c, tc;
	struct grid_data g;

	int y, x;
	int vy, vx;
	int ty, tx;
	int clipy;

	/* Redraw map sub-windows */
	prt_map_aux();

	/* Assume screen */
	ty = Term->offset_y + SCREEN_HGT;
	tx = Term->offset_x + SCREEN_WID;

	/* Avoid overwriting the last row with padding for big tiles. */
	clipy = ROW_MAP + SCREEN_ROWS;

	/* Dump the map */
	for (y = Term->offset_y, vy = ROW_MAP; y < ty; vy += tile_height, y++)
		for (x = Term->offset_x, vx = COL_MAP; x < tx; vx += tile_width, x++) {
			/* Check bounds */
			if (!square_in_bounds(cave, loc(x, y))) continue;

			/* Determine what is there */
			map_info(loc(x, y), &g);
			grid_data_as_text(&g, &a, &c, &ta, &tc);

			/* Queue it */
			Term_queue_char(Term, vx, vy, a, c, ta, tc);

			if ((tile_width > 1) || (tile_height > 1))
				Term_big_queue_char(Term, vx, vy, clipy, a, c,
					COLOUR_WHITE, L' ');
		}
}

/**
 * Display a "small-scale" map of the dungeon in the active Term.
 *
 * Note that this function must "disable" the special lighting effects so
 * that the "priority" function will work.
 *
 * Note the use of a specialized "priority" function to allow this function
 * to work with any graphic attr/char mappings, and the attempts to optimize
 * this function where possible.
 *
 * If "cy" and "cx" are not NULL, then returns the screen location at which
 * the player was displayed, so the cursor can be moved to that location,
 * and restricts the horizontal map size to SCREEN_WID.  Otherwise, nothing
 * is returned (obviously), and no restrictions are enforced.
 */
void display_map(int *cy, int *cx)
{
	int map_hgt, map_wid;
	int row, col;

	int x, y;
	struct grid_data g;

	int a, ta;
	wchar_t c, tc;

	uint8_t tp;

	struct monster_race *race = &r_info[0];

	/* Priority array */
	uint8_t **mp = mem_zalloc(cave->height * sizeof(uint8_t*));
	for (y = 0; y < cave->height; y++)
		mp[y] = mem_zalloc(cave->width * sizeof(uint8_t));

	/* Desired map height */
	get_minimap_dimensions(Term, cave, tile_width, tile_height,
		&map_wid, &map_hgt);

	/* Prevent accidents */
	if ((map_wid < 1) || (map_hgt < 1)) {
		for (y = 0; y < cave->height; y++)
			mem_free(mp[y]);
		mem_free(mp);
		return;
	}

	/* Nothing here */
	a = COLOUR_WHITE;
    c = L' ';
	ta = COLOUR_WHITE;
	tc = L' ';

	/* Draw a box around the edge of the term */
	window_make(0, 0, map_wid + 1, map_hgt + 1);

	/* Clear outside that boundary. */
	if (map_wid + 1 < Term->wid - 1) {
		for (y = 0; y < map_hgt + 1; y++) {
			Term_erase(map_wid + 2, y, Term->wid - map_wid - 2);
		}
	}
	if (map_hgt + 1 < Term->hgt - 1) {
		for (y = map_hgt + 2; y < Term->hgt; y++) {
			Term_erase(0, y, Term->wid);
		}
	}

	/* Analyze the actual map */
	for (y = 0; y < cave->height; y++) {
		row = (y * map_hgt) / cave->height;
		if (tile_height > 1) row = row - (row % tile_height);

		for (x = 0; x < cave->width; x++) {
			col = (x * map_wid) / cave->width;
			if (tile_width > 1) col = col - (col % tile_width);

			/* Get the attr/char at that map location */
			map_info(loc(x, y), &g);
			grid_data_as_text(&g, &a, &c, &ta, &tc);

			/* Get the priority of that attr/char */
			tp = f_info[g.f_idx].priority;

			/* Stuff on top of terrain gets higher priority */
			if ((a != ta) || (c != tc)) tp = 20;

			/* Save "best" */
			if (mp[row][col] < tp) {
				/* Hack - make every grid on the map lit */
				g.lighting = LIGHTING_LIT;
				grid_data_as_text(&g, &a, &c, &ta, &tc);

				Term_queue_char(Term, col + 1, row + 1, a, c, ta, tc);

				if ((tile_width > 1) || (tile_height > 1))
					Term_big_queue_char(Term, col + 1,
						row + 1, Term->hgt - 1,
						255, -1, 0, 0);

				/* Save priority */
				mp[row][col] = tp;
			}
		}
	}

	/*** Display the player ***/

	/* Player location */
	row = (player->grid.y * map_hgt / cave->height);
	col = (player->grid.x * map_wid / cave->width);

	if (tile_width > 1)
		col = col - (col % tile_width);
	if (tile_height > 1)
		row = row - (row % tile_height);

	/* Get the terrain at the player's spot. */
	map_info(player->grid, &g);
	g.lighting = LIGHTING_LIT;
	grid_data_as_text(&g, &a, &c, &ta, &tc);

	/* Get the "player" tile */
	a = monster_x_attr[race->ridx];
	c = monster_x_char[race->ridx];

	/* Draw the player */
	Term_queue_char(Term, col + 1, row + 1, a, c, ta, tc);

	if ((tile_width > 1) || (tile_height > 1))
		Term_big_queue_char(Term, col + 1, row + 1, Term->hgt - 1,
			255, -1, 0, 0);
  
	/* Return player location */
	if (cy != NULL) (*cy) = row + 1;
	if (cx != NULL) (*cx) = col + 1;

	for (y = 0; y < cave->height; y++)
		mem_free(mp[y]);
	mem_free(mp);
}


/*
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
/**
 * Draw the overhead map of the world (WLD-25).
 *
 * One character per block, which is the resolution the knowledge is kept at and
 * all a terminal can show: the world is 129 blocks across and a screen is
 * eighty columns, so it pans rather than scaling -- squeezing 129 rows into
 * twenty-two would lose the coastlines, which are the thing worth looking at.
 *
 * Only blocks the player has been near enough to see are drawn.  The rest is
 * blank, and fills in as they travel.
 *
 * \param origin is the top-left block of the view, and is moved by the caller.
 */
/**
 * How each band of place shows on the world map.
 *
 * Ordered as wild_town_bands is, smallest first, so a glance at the map says
 * which way a character would walk to find a magic shop.
 */
static const uint8_t wild_town_band_attr[] = {
	COLOUR_L_UMBER, COLOUR_YELLOW, COLOUR_L_GREEN, COLOUR_L_BLUE
};

static void display_world_map(struct loc origin)
{
	int size = z_info->wild_block_size;
	int wid = Term->wid - 2, hgt = Term->hgt - 4;
	struct loc here = loc(player->wild_grid.x / size,
						  player->wild_grid.y / size);
	int row, col;

	Term_clear();

	for (row = 0; row < hgt; row++) {
		for (col = 0; col < wid; col++) {
			int bx = origin.x + col, by = origin.y + row;
			int feat, a;
			wchar_t c;

			if (!wild_in_bounds(wild, bx, by)) continue;
			if (!wild_seen(wild, bx, by)) continue;

			feat = wild_block_feat(wild, bx, by);
			if (feat == FEAT_NONE) continue;

			a = feat_x_attr[LIGHTING_LIT][feat];
			c = feat_x_char[LIGHTING_LIT][feat];

			/*
			 * A town is worth picking out of the country around it, and how
			 * large it is worth telling from across the map: it is what
			 * decides whether the walk is worth making.
			 */
			{
				int town = wild_town_at(wild, bx, by);

				if (town >= 0) {
					a = wild_town_band_attr[wild->towns[town].band];
				} else if (wild_dungeon_in_block(wild, bx, by)) {
					a = COLOUR_L_RED;
				} else if (wild_block_at(wild, bx, by)->place) {
					/* The margin a town or a dungeon reserves around itself. */
					a = COLOUR_L_WHITE;
				}
			}

			Term_queue_char(Term, col + 1, row + 1, a, c, a, c);
		}
	}

	/* Where the player is, if that part of the world is on screen. */
	if (here.x >= origin.x && here.x < origin.x + wid &&
		here.y >= origin.y && here.y < origin.y + hgt) {
		Term_queue_char(Term, here.x - origin.x + 1, here.y - origin.y + 1,
						COLOUR_WHITE, L'@', COLOUR_WHITE, L'@');
	}

	prt(format("World map -- you are at %d, %d of %d.  "
			   "Direction keys scroll, ESC exits.",
			   player->wild_grid.x, player->wild_grid.y,
			   wild_world_grids()), Term->hgt - 1, 1);

	/* Name the colours, or they say nothing. */
	{
		int band, at = 1;

		c_put_str(COLOUR_WHITE, "Places:", Term->hgt - 2, at);
		at += 8;

		for (band = 0; band < 4; band++) {
			c_put_str(wild_town_band_attr[band], wild_band_name(band),
					  Term->hgt - 2, at);
			at += strlen(wild_band_name(band)) + 2;
		}

		c_put_str(COLOUR_L_RED, "dungeon", Term->hgt - 2, at);
	}
}

/**
 * ------------------------------------------------------------------------
 * The magetower (WLD-16c)
 * ------------------------------------------------------------------------ */

/** The most destinations the tower will offer at once. */
#define TRAVEL_MAX (WILD_TOWNS_MAX + WILD_DUNGEONS_MAX)

/**
 * Offer the player a journey, and make it if they take one.
 *
 * Zangband's magetower linked the towns a character had visited.  The rule here
 * is the same idea with the two bars WLD-16c settled on: a town has to have been
 * stood in, a dungeon mouth only seen -- and the fare rises with the distance,
 * so the network is a convenience rather than a way of skipping the world.
 *
 * Nothing here is free: a character with no gold is told the price and sent
 * away, which is the whole point of a fare.
 */
static void magetower_travel(void)
{
	struct wild_place places[TRAVEL_MAX];
	struct loc from = loc(player->grid.x + player->wild_offset.x,
						  player->grid.y + player->wild_offset.y);
	struct menu *m;
	char *labels;
	int count, chosen, i;

	count = wild_travel_places(wild, from, places, TRAVEL_MAX);

	if (!count) {
		msg("The mages can carry you to places you have already been.");
		msg("You have not been anywhere else yet.");
		return;
	}

	m = menu_dynamic_new();
	if (!m) return;

	/*
	 * A key for every destination, and 'q' to decline.
	 *
	 * This function used to hand out its own letters, because the 26 of
	 * lower_case are fewer than the destinations a well-travelled character
	 * can be offered and the "Stay here" row landed past the end of the
	 * 27-byte buffer. That is now what menu_dynamic_labels() is for, and it
	 * skips 'q' for the same reason this did: the selection scan takes the
	 * first match in the string, so a destination handed 'q' -- the
	 * seventeenth, with the old alphabet -- was chosen by the key meant to
	 * decline the journey.
	 */
	labels = menu_dynamic_labels(m);

	for (i = 0; i < count; i++) {
		char line[80];

		strnfmt(line, sizeof(line), "%-24s %-11s %6d au",
				places[i].name ? places[i].name : "somewhere",
				places[i].what, (int) places[i].cost);

		/* One more than the index, so that zero can mean "no thank you". */
		menu_dynamic_add_label(m, line, 0, i + 1, labels);
	}

	menu_dynamic_add_label(m, "Stay here", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	prt(format("The mages will carry you.  You have %d au.", (int) player->au),
		0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	mem_free(labels);
	screen_load();

	if (chosen <= 0 || chosen > count) return;

	chosen--;

	if (player->au < places[chosen].cost) {
		msg("The journey to %s costs %d au, and you have %d.",
			places[chosen].name, (int) places[chosen].cost, (int) player->au);
		return;
	}

	player->au -= places[chosen].cost;

	msg("The mages carry you to %s.", places[chosen].name);

	/*
	 * Set where in the world the player now is and have the surface rebuilt
	 * around it.  Flagged as a scroll of the window rather than a level change,
	 * as walking to the edge of the window is: this is travel across the
	 * overworld, not a descent, and it must not cancel the player's target or
	 * hand them a free turn's energy.
	 */
	player->wild_grid = places[chosen].grid;
	player->upkeep->generate_level = true;
	player->upkeep->scroll_world = true;
	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * Charge the player, if they can pay.
 *
 * \return false if they cannot, having said so.  Nothing in a town is free:
 * that is what makes the services a use for gold rather than a menu of wishes.
 */
static bool service_pay(int32_t price, const char *what)
{
	if (player->au < price) {
		msg("%s costs %d au, and you have %d.", what, (int) price,
			(int) player->au);
		return false;
	}

	player->au -= price;
	player->upkeep->redraw |= PR_GOLD;

	return true;
}

/**
 * The healer: mend what is broken, for gold (WLD-16c).
 *
 * Priced by what it is worth rather than by a flat fee -- a cut is a trifle and
 * a drained constitution is not -- and each is offered only when there is
 * something to do, so the menu says what is wrong with you as much as what it
 * will cost.
 */
static void service_healer(void)
{
	enum { HEAL_WOUNDS = 1, HEAL_AILMENTS, HEAL_STATS, HEAL_EXP };
	struct menu *m;
	char *labels;
	int chosen;
	bool ailing = false, drained = false;
	int missing = player->mhp - player->chp;
	int i;

	for (i = 0; i < TMD_MAX; i++)
		if (i == TMD_POISONED || i == TMD_CUT || i == TMD_STUN ||
			i == TMD_BLIND || i == TMD_CONFUSED)
			if (player->timed[i]) ailing = true;

	for (i = 0; i < STAT_MAX; i++)
		if (player->stat_cur[i] < player->stat_max[i]) drained = true;

	m = menu_dynamic_new();
	if (!m) return;

	labels = menu_dynamic_labels(m);

	if (missing > 0)
		menu_dynamic_add_label(m, format("%-28s %5d au", "Bind your wounds",
			(int) (missing * z_info->heal_cost)), 0, HEAL_WOUNDS, labels);
	if (ailing)
		menu_dynamic_add_label(m, format("%-28s %5d au", "Cure what ails you",
			(int) z_info->ailment_cost), 0, HEAL_AILMENTS, labels);
	if (drained)
		menu_dynamic_add_label(m, format("%-28s %5d au", "Restore your strength",
			(int) z_info->restore_cost), 0, HEAL_STATS, labels);
	if (player->exp < player->max_exp)
		menu_dynamic_add_label(m, format("%-28s %5d au",
			"Restore your lost levels",
			(int) z_info->restore_cost), 0, HEAL_EXP, labels);

	menu_dynamic_add_label(m, "Nothing today", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	prt(format("The healer looks you over.  You have %d au.",
		(int) player->au), 0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);
	screen_load();

	switch (chosen) {
		case HEAL_WOUNDS:
			if (!service_pay(missing * z_info->heal_cost, "Binding your wounds"))
				return;
			effect_simple(EF_HEAL_HP, source_player(), "1000", 0, 0, 0, 0, 0,
						  NULL);
			msg("You are made whole.");
			break;

		case HEAL_AILMENTS:
			if (!service_pay(z_info->ailment_cost, "That"))
				return;
			for (i = 0; i < TMD_MAX; i++)
				if (i == TMD_POISONED || i == TMD_CUT || i == TMD_STUN ||
					i == TMD_BLIND || i == TMD_CONFUSED)
					player_clear_timed(player, i, true, false);
			msg("You feel better.");
			break;

		case HEAL_STATS:
			if (!service_pay(z_info->restore_cost, "That"))
				return;
			for (i = 0; i < STAT_MAX; i++)
				effect_simple(EF_RESTORE_STAT, source_player(), "0", i, 0, 0,
							  0, 0, NULL);
			msg("You feel like yourself again.");
			break;

		case HEAL_EXP:
			/*
			 * Drained experience, not anything to do with memory -- which is
			 * what this was called at first, and was the wrong game's idiom.
			 * Angband calls it life force: a blow that drains it says "You feel
			 * your life draining away", and restoring it says "You feel your
			 * life energies returning".  What it costs the player is levels,
			 * since level is computed from experience, so that is what the
			 * healer offers to put back.
			 */
			if (!service_pay(z_info->restore_cost, "That"))
				return;
			effect_simple(EF_RESTORE_EXP, source_player(), "0", 0, 0, 0, 0, 0,
						  NULL);
			break;

		default:
			return;
	}

	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * Somewhere else to be sent (WLD-19, WLD-21).
 *
 * \param town is where the player is standing, which is no use as a destination.
 * \param unknown is set to a place they have *not* found, if there is one --
 * that is what a find-place errand is for, and it is the only kind of quest that
 * hands out knowledge of the world rather than asking for it back.
 * \return a town they have been to and could carry word to, or -1.
 */
static int quest_giver_somewhere_else(int town, int *unknown)
{
	int i, known = -1, blind = -1, seen = 0, unseen = 0;

	*unknown = -1;

	for (i = 0; i < wild_town_count(wild); i++) {
		if (i == town) continue;

		if (wild->towns[i].visited) {
			/* Reservoir sampling, so the same town is not always chosen. */
			if (one_in_(++seen)) known = i;
		} else if (one_in_(++unseen)) {
			blind = i;
		}
	}

	*unknown = blind;

	return known;
}

/**
 * Offer to carry word to a town the player has been to (WLD-19).
 */
static bool quest_offer_delivery(int to)
{
	struct quest *q;
	char name[80];

	if (to < 0) return false;

	strnfmt(name, sizeof(name), "word to %s",
			wild->towns[to].name ? wild->towns[to].name : "the next town");

	if (!get_check(format("\"Carry %s? \"", name)))
		return true;

	q = quest_take(player, QUEST_DELIVERY, name, NULL, 1);
	if (!q) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	q->town = to + 1;
	msg("\"They'll be expecting you.\"");

	return true;
}

/**
 * Offer to go and look at somewhere nobody here has seen (WLD-19).
 */
static bool quest_offer_place(int to)
{
	struct quest *q;
	char name[80];

    if (to < 0) return false;

	strnfmt(name, sizeof(name), "find %s",
			wild->towns[to].name ? wild->towns[to].name : "the place");

	if (!get_check(format("\"There's talk of a place called %s, and nobody's "
						  "been. Go and look? \"",
						  wild->towns[to].name ? wild->towns[to].name :
						  "somewhere")))
		return true;

	q = quest_take(player, QUEST_FIND_PLACE, name, NULL, 1);
	if (!q) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	q->town = to + 1;
	msg("\"Come back and tell us what you find.\"");

	return true;
}

/**
 * What the character still owes this trade, and the chance to give it back
 * (ZangbandTK, WLD-20).
 *
 * A quest that can be taken and never given up is a quest that follows the
 * character to the grave: take a bounty on something too deep for you and the
 * slot is gone for good.  So the fourth thing the lifecycle has to be able to say
 * is that a job ended badly, and handing it back over the counter is how.
 *
 * Every job is listed rather than only the first, because a character can carry
 * several and being able to give up only the oldest is not a choice.
 */
static bool quest_giver_owed(void)
{
	struct menu *m;
	char *labels;
	int chosen, i, listed = 0;
	int slot[16];

	m = menu_dynamic_new();
	if (!m) return true;

	labels = menu_dynamic_labels(m);

	for (i = z_info->quest_fixed;
		 i < z_info->quest_max && listed < (int) N_ELEMENTS(slot); i++) {
		struct quest *q = &player->quests[i];
		char line[80];

		if (q->state != QUEST_TAKEN || q->fixed) continue;

		if (q->type == QUEST_BOUNTY || q->type == QUEST_WILD ||
			q->type == QUEST_DUNGEON)
			strnfmt(line, sizeof(line), "%-34s %d of %d",
					q->name ? q->name : "that business", q->cur_num,
					q->max_num);
		else
			strnfmt(line, sizeof(line), "%s", q->name ? q->name :
					"that business");

		slot[listed] = i;
		menu_dynamic_add_label(m, line, 0, listed + 1, labels);
		listed++;
	}

	menu_dynamic_add_label(m, "Nothing today", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	prt("\"You're still owing us. Giving any of it up?\"", 0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);
	screen_load();

	if (chosen <= 0 || chosen > listed) return true;

	{
		struct quest *q = &player->quests[slot[chosen - 1]];

		if (!get_check(format("Give up %s? ", q->name ? q->name :
							  "that business")))
			return true;

		msg("\"Suit yourself. We'll find somebody else.\"");
		quest_hand_back(player, q);
	}

	return true;
}

/**
 * A creature the character could be sent after (WLD-19).
 *
 * \param deepest bounds how nasty it may be.
 * \param type, if given, restricts it to what dwells in that dungeon.
 */
static struct monster_race *quest_pick_race(int deepest,
											const struct dun_type *type)
{
	struct monster_race *race = NULL;
	int tries;

	for (tries = 0; tries < 400 && !race; tries++) {
		struct monster_race *pick =
			&r_info[randint0(z_info->r_max ? z_info->r_max : 1)];

		if (!pick->name) continue;
		if (rf_has(pick->flags, RF_UNIQUE)) continue;
		if (pick->level < 1 || pick->level > deepest) continue;
		if (type && !dun_type_dwells(type, pick)) continue;

		race = pick;
	}

	return race;
}

/**
 * Name a number of a creature, pluralising where the bestiary does not.
 */
static void quest_name_kills(char *buf, size_t len, int number,
							 const struct monster_race *race)
{
	if (race->plural)
		strnfmt(buf, len, "%d %s", number, race->plural);
	else
		strnfmt(buf, len, "%d %ss", number, race->name);
}

/**
 * Offer a killing sent to a particular dungeon (WLD-19, QUEST_DUNGEON).
 *
 * Only dungeons the character has found, and only depths the dungeon actually
 * reaches -- a job at a depth its dungeon does not go to could never be done,
 * which is the fault the-game-can-be-won guards the ending against.
 */
static bool quest_offer_dungeon(void)
{
	struct dun_type *type = NULL;
	struct monster_race *race;
	struct quest *q;
	char name[80], line[120];
	int i, seen = 0, depth, number;

	for (i = 0; i < dun_type_count(); i++) {
		if (!wild_dungeon_found(wild, i)) continue;
		if (one_in_(++seen)) type = dun_type_by_index(i);
	}

	if (!type) return false;

	depth = rand_range(type->min_depth, type->max_depth);
	if (depth > player->max_depth + 6) depth = player->max_depth + 6;
	if (depth < type->min_depth) depth = type->min_depth;

	race = quest_pick_race(depth + 2, type);
	if (!race) return false;

	number = 2 + randint0(3);
	quest_name_kills(name, sizeof(name), number, race);

	strnfmt(line, sizeof(line), "\"%s, down in %s. Take the work? \"", name,
			type->name);

	if (!get_check(line)) return true;

	q = quest_take(player, QUEST_DUNGEON, name, race, number);
	if (!q) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	q->level = depth;
	q->dungeon = type->index + 1;

	msg("\"%s, level %d. Mind how you go.\"", type->name, depth);

	return true;
}

/**
 * Offer a killing to be done above ground (WLD-19, QUEST_WILD).
 */
static bool quest_offer_wild(void)
{
	struct monster_race *race = quest_pick_race(player->max_depth + 3, NULL);
	struct quest *q;
	char name[80];
	int number;

	if (!race) return false;

	/* Rolled once.  Naming one number and asking for another is a swindle. */
	number = 2 + randint0(3);
	quest_name_kills(name, sizeof(name), number, race);

	if (!get_check(format("\"%s, out in the open country. Take it? \"", name)))
		return true;

	q = quest_take(player, QUEST_WILD, name, race, number);
	if (!q) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	msg("\"Above ground, mind. What dies underground is nobody's business.\"");

	return true;
}

/**
 * Offer to fetch something (WLD-19, QUEST_FIND_ITEM).
 */
static bool quest_offer_item(void)
{
	struct object_kind *kind = NULL;
	struct quest *q;
	char name[80];
	int tries;

	for (tries = 0; tries < 400 && !kind; tries++) {
		struct object_kind *pick =
			&k_info[randint0(z_info->k_max ? z_info->k_max : 1)];

		if (!pick->name) continue;
		if (pick->alloc_min > player->max_depth + 5) continue;
		if (!pick->alloc_max) continue;
		if (tval_is_money_k(pick)) continue;

		kind = pick;
	}

	if (!kind) return false;

	strnfmt(name, sizeof(name), "fetch %s", kind->name);

	if (!get_check(format("\"We're wanting %s brought in. Take it? \"",
						  kind->name)))
		return true;

	q = quest_take(player, QUEST_FIND_ITEM, name, NULL, 1);
	if (!q) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	q->kind = kind;
	msg("\"Bring it here and we'll see you right.\"");

	return true;
}

/**
 * Work offered over the bar (ZangbandTK, WLD-16d).
 *
 * Quest-giving is a property a building carries, not a building of its own, so
 * this runs before whatever the building itself does -- walk into an inn that
 * has work and you are offered it before you are offered a bed.  Nothing here
 * knows it is an inn; move the property to the magetower in
 * wild_town_quest_givers() and the magetower starts commissioning retrievals.
 *
 * All six of Zangband's kinds are offered here (WLD-19), chosen by what the
 * world can supply: an errand to another town needs another town, a job down a
 * particular dungeon needs one the character has found, and nobody is sent to
 * look at the place they are standing in.  Every kind falls back to the bounty
 * -- kill so many of a creature, come back, be paid -- which needs nothing but
 * a bestiary and so can always be offered.
 *
 * \return true if the building's own business should be skipped.
 */
static bool quest_giver_business(int town)
{
	struct quest *done = quest_carried(player, true);
	struct quest *doing = quest_carried(player, false);
	struct monster_race *race = NULL;
	char name[80];
	int number, reward, tries, unknown = -1;

	/* Pay for what has been finished. */
	if (done) {
		reward = done->max_num * (done->race ? done->race->level + 1 : 1) * 20;
		reward = MAX(reward, 20);

		msg("\"That's the %s dealt with. Here's what we promised.\"",
			done->name ? done->name : "business");
		msg("You are paid %d au.", reward);

		player->au += reward;
		player->upkeep->redraw |= PR_GOLD;

		quest_hand_back(player, done);
		return true;
	}

	/* Or ask after what is still owed, and offer to be let off it. */
	if (doing)
		return quest_giver_owed();

	/*
	 * Or offer something.  Which kind depends on what there is to offer: an
	 * errand to another town needs another town to send you to, and there is no
	 * point sending somebody to a place they are standing in.
	 */
	{
		int elsewhere = quest_giver_somewhere_else(town, &unknown);
		int roll;

		/*
		 * Six kinds, offered as the world allows: an errand to another town
		 * needs another town, a job down a particular dungeon needs one the
		 * character has found, and there is no sense sending anybody to a place
		 * they are standing in.  Each falls back to the bounty, which needs
		 * nothing but a bestiary.
		 */
		for (roll = randint0(5); roll > 0; roll--) {
			switch (roll) {
				case 4: if (unknown >= 0) return quest_offer_place(unknown);
						break;
				case 3: if (elsewhere >= 0)
							return quest_offer_delivery(elsewhere);
						break;
				case 2: if (quest_offer_dungeon()) return true;
						break;
				case 1: if (quest_offer_item()) return true;
						break;
				default: break;
			}
		}

		if (one_in_(3) && quest_offer_wild()) return true;
	}

	for (tries = 0; tries < 200 && !race; tries++) {
		struct monster_race *pick =
			&r_info[randint0(z_info->r_max ? z_info->r_max : 1)];

		if (!pick->name) continue;
		if (rf_has(pick->flags, RF_UNIQUE)) continue;
		if (pick->level < 1) continue;
		if (pick->level > player->max_depth + 4) continue;

		race = pick;
	}

	if (!race) {
		msg("\"Nothing needs doing that you could help with.\"");
		return true;
	}

	number = 3 + randint0(5);

	/*
	 * "6 small kobold" is what this said before.  monster.txt carries a plural
	 * only for the names that need one -- ninety-six of them, the irregulars --
	 * so everything else takes a plain "s", which is what the field being
	 * optional means.
	 */
	if (race->plural)
		strnfmt(name, sizeof(name), "%d %s", number, race->plural);
	else
		strnfmt(name, sizeof(name), "%d %ss", number, race->name);

	if (!get_check(format("\"There's %s wanting killing. Take the work? \"",
						  name)))
		return true;

	if (!quest_take(player, QUEST_BOUNTY, name, race, number)) {
		msg("\"You've enough on your plate already.\"");
		return true;
	}

	msg("\"Come back when it's done.\"");

	return true;
}

/**
 * The inn: a bed until morning (WLD-16c).
 *
 * Which earns its keep here in a way it would not in Angband.  Daylight is what
 * reveals the overworld, so a character who arrives somewhere at dusk either
 * gropes about by lamplight or waits -- and waiting a hundred turns at a time
 * with the rest command is not waiting, it is bookkeeping.
 */
static void service_inn(void)
{
	int32_t price = z_info->inn_cost;

	if (is_daytime()) {
		msg("The innkeeper says you would do better to travel while it is light.");
		return;
	}

	if (!get_check(format("A bed until morning for %d au? ", (int) price)))
		return;

	if (!service_pay(price, "A bed"))
		return;

	/*
	 * Sleep until the sun comes up.  Turned forward rather than rested,
	 * because resting is a thing monsters can interrupt and this is a room with
	 * a door on it.
	 *
	 * The world still has to be run as the clock passes it, on the same ten
	 * turn beat as the main loop, or the night is not a night: winding `turn`
	 * on by itself digested no food, let no poison or cut or blessing run down,
	 * and healed nothing, so a character could buy a bed while starving and
	 * poisoned and wake at dawn exactly as they lay down.
	 */
	while (!is_daytime()) {
		if (!(turn % 10)) {
			process_world(cave);

			notice_stuff(player);
			handle_stuff(player);

			/* A night can be survived or not: poison and hunger both bite. */
			if (player->is_dead || !player->upkeep->playing)
				return;
		}

		turn++;
		player->upkeep->update |= PU_BONUS;
	}

	msg("You wake to daylight.");

	/*
	 * And what the night showed you (PLR-41).  After the waking message, because
	 * the dream is what you remember once you are awake, and before the turn is
	 * charged, so that a dream which leaves you frightened has left you
	 * frightened by the time you can act.
	 */
	player_night_dream(player);

	cave_illuminate(cave, true, !wild_is_surface(cave));
	player->upkeep->redraw |= (PR_STATUS | PR_MAP);
	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * The magesmith, and the recharger (WLD-16c).
 *
 * Both are the same shape: take the fee, then run the effect Angband already
 * has, which does its own asking about which item is meant.  Zangband wrote its
 * own item prompts and its own enchanting arithmetic; there is no reason to,
 * and DEC-27 says as much -- the idea is a shop that will improve your gear,
 * and 4.2 has the machinery for improving gear.
 */
/**
 * Run one of the services that is simply an effect, for a fee (WLD-16c).
 *
 * \param dice is what the effect works from -- how many points of enchantment,
 * how much recharging strength.  It was "0" for both services here, and zero is
 * not a small amount, it is nothing: ENCHANT tests its subtype as a set of bit
 * flags and zero matches none of them, so the magesmith took the fee and did not
 * so much as ask which item; RECHARGE asked for the item and then worked at
 * strength zero, which is the worst odds the game has.  Both were reported from
 * play as taking the money and doing nothing.
 * \param subtype selects which kind, and matters for the same reason.
 */
static void service_effect(const char *what, int effect, int32_t price,
						   const char *dice, int subtype)
{
	if (!get_check(format("%s for %d au? ", what, (int) price)))
		return;

	if (!service_pay(price, what))
		return;

	effect_simple(effect, source_player(), dice, subtype, 0, 0, 0, 0, NULL);
	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * The Chaos Tower: have one mutation taken off you (DEC-24, PLR-13).
 *
 * The seventh building, and the only one that had to be written rather than
 * ported. Zangband names it in `t_info.txt` as the Mutatalist, routes it
 * through a Lua hook, and ships no script to fill the hook -- so it has a
 * building, a door and no behaviour. `spoilers/mutation.txt` lists it as one of
 * only six ways to lose a mutation, which is why DEC-24 kept it when it cut the
 * Casino and the Weaponmaster.
 *
 * The player chooses which mutation goes, which Zangband's own removal routes
 * never allow -- a potion of New Life takes all of them and the strangely
 * normal mutation takes whichever it likes. Choosing is the whole reason to
 * walk to a building and pay 2500 gold for something a potion does wholesale.
 */
static void service_chaostower(void)
{
	const struct mutation *held[MUTATION_MAX];
	struct menu *m;
	char *labels;
	int count = 0, chosen;
	const struct mutation *mut;

	for (mut = mutations; mut && count < (int) N_ELEMENTS(held);
			mut = mut->next) {
		if (player_has_mutation(player, mut)) held[count++] = mut;
	}

	if (!count) {
		msg("They look you over and shake their heads.  Nothing to do.");
		return;
	}

	m = menu_dynamic_new();
	if (!m) return;

	labels = menu_dynamic_labels(m);

	for (chosen = 0; chosen < count; chosen++) {
		menu_dynamic_add_label(m, held[chosen]->desc, 0, chosen + 1, labels);
	}
	menu_dynamic_add_label(m, "Keep them all", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	prt(format("Which will you be rid of, for %d au?",
			   (int) z_info->chaostower_cost), 0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);
	screen_load();

	if (chosen <= 0 || chosen > count) return;

	if (!service_pay(z_info->chaostower_cost, "the Chaos Tower")) return;

	if (player_lose_mutation(player, held[chosen - 1])) {
		msg("Something is taken out of you, and it does not come back.");
	}

	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * The magesmith: put magic on a weapon or a suit of armour (WLD-16c).
 *
 * Asked which before the fee is named, because the two are different work and
 * the player should know what they are buying.  A weapon gets both its to-hit
 * and its to-damage, which is why this costs twice what a scroll of one or the
 * other does.
 */
static void service_enchant(void)
{
	struct menu *m;
	char *labels;
	int chosen;

	m = menu_dynamic_new();
	if (!m) return;

	labels = menu_dynamic_labels(m);

	menu_dynamic_add_label(m, "A weapon, to hit and to wound", 0, ENCH_TOBOTH,
						   labels);
	menu_dynamic_add_label(m, "A suit of armour", 0, ENCH_TOAC, labels);
	menu_dynamic_add_label(m, "Nothing today", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	prt(format("The magesmith will work for %d au.  You have %d.",
			   (int) z_info->enchant_cost, (int) player->au), 0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);
	screen_load();

	if (chosen != ENCH_TOBOTH && chosen != ENCH_TOAC) return;

	service_effect(chosen == ENCH_TOAC ? "Enchanting your armour"
									   : "Enchanting your weapon",
				   EF_ENCHANT, z_info->enchant_cost, "1", chosen);
}

/** Somebody walked into a service building. */
void ui_enter_service(game_event_type type, game_event_data *data, void *user)
{
	int service = wild_service_at(cave, player->grid);

	/*
	 * Work first, if this building carries it (WLD-16d).  A property of the
	 * building rather than a building of its own, so this asks the town which
	 * of its doors has work behind it rather than checking for a quest hut.
	 */
	if (service >= 0 && player->in_wild && wild) {
		int town = wild_town_here(wild,
								  loc(player->grid.x + player->wild_offset.x,
									  player->grid.y + player->wild_offset.y));

		if (wild_gives_quests(wild, town, service) &&
			quest_giver_business(town))
			return;
	}

	switch (service) {
		case WILD_SERVICE_MAGETOWER: magetower_travel(); break;
		case WILD_SERVICE_HEALER:    service_healer(); break;
		case WILD_SERVICE_INN:       service_inn(); break;

		case WILD_SERVICE_ENCHANT:
			service_enchant();
			break;

		case WILD_SERVICE_CHAOSTOWER:
			service_chaostower();
			break;

		case WILD_SERVICE_RECHARGE:
			/*
			 * Strength six, the same as a scroll of Recharging, which is what
			 * the recharger is standing in for.  It can still fail and destroy
			 * the item; that is the ordinary risk and not a swindle.
			 */
			service_effect("Have a wand or staff recharged", EF_RECHARGE,
						   z_info->recharge_cost, "6", 0);
			break;

		default:
			break;
	}
}

/**
 * What the character has taken on, and how far along it is (WLD-22).
 *
 * Zangband's QUEST_FLAG_KNOWN implies a list the player can read, and until this
 * existed the only way to find out what you owed was to walk back into a
 * building that hires and be told -- which is a long way to go to answer "what
 * was I supposed to be doing?"
 *
 * The quests the game is won by are not listed.  A character is on those from
 * birth without being told, and putting "kill the Serpent of Chaos" at the top
 * of every character's to-do list from level one gives away the ending and is
 * not, in any useful sense, news.
 */
void do_cmd_quest_log(void)
{
	textblock *tb = textblock_new();
	int i, shown = 0;

	for (i = z_info->quest_fixed; i < z_info->quest_max; i++) {
		struct quest *q = &player->quests[i];
		const char *where = NULL;

		if (q->state == QUEST_UNTAKEN || q->fixed) continue;

		if (q->town && wild && q->town - 1 < wild_town_count(wild))
			where = wild->towns[q->town - 1].name;

		textblock_append_c(tb, COLOUR_L_UMBER, "%s", q->name ? q->name :
						   "Something or other");

		if (q->state == QUEST_COMPLETE) {
			textblock_append_c(tb, COLOUR_L_GREEN, "  -- done");
			textblock_append(tb, ", report it at any place that hires.");
		} else if (q->type == QUEST_BOUNTY || q->type == QUEST_WILD ||
				   q->type == QUEST_DUNGEON) {
			textblock_append(tb, "  -- %d of %d.", q->cur_num, q->max_num);
		} else if (where) {
			textblock_append(tb, "  -- %s.", where);
		}

		textblock_append(tb, "\n\n");
		shown++;
	}

	if (!shown)
		textblock_append(tb, "You have taken nothing on.\n\nSomebody in a "
						 "town may have work: the inn is the place to ask.\n");

	textui_textblock_show(tb, SCREEN_REGION, "Things you have taken on");
	textblock_free(tb);
}

/**
 * Use one of the character's racial powers (ZangbandTK, PLR-02).
 *
 * A menu rather than a key each, because a race may have several and Angband has
 * no letters to spare -- the Amberite has two, thirty levels apart.  Powers the
 * character is not yet old enough for are listed and greyed rather than hidden,
 * so a Draconian knows at level one that it will breathe eventually.
 */
void do_cmd_racial_power(void)
{
	struct player_power *powers[MUTATION_MAX];
	const struct mutation *pending[MUTATION_MAX];
	struct menu *m;
	char *labels;
	int count = 0, waiting = 0, chosen, i;
	struct player_power *power;
	const struct mutation *mut;

	/*
	 * Blood first, then training.  A Mindflayer Mindcrafter has both, and the
	 * two lists are the same kind of thing to the player even though one comes
	 * from PLR-02 and the other from PLR-06.
	 */
	for (power = player->race->powers;
			power && count < (int) N_ELEMENTS(powers); power = power->next)
		powers[count++] = power;

	for (power = player->class->powers;
			power && count < (int) N_ELEMENTS(powers); power = power->next)
		powers[count++] = power;

	/*
	 * And then chaos (PLR-16). Last, because blood and training are things a
	 * character chose and a mutation is not -- and because the list is stable
	 * that way: gaining a mutation appends rather than shuffling the letters
	 * of the powers already there.
	 */
	for (mut = mutations; mut; mut = mut->next) {
		if (!player_has_mutation(player, mut)) continue;
		if (mut->kind != MUTATION_KIND_ACTIVATABLE) continue;

		if (mut->action) {
			if (count < (int) N_ELEMENTS(powers)) powers[count++] = mut->action;
		} else if (waiting < (int) N_ELEMENTS(pending)) {
			/*
			 * Eight of the thirty-two have no 4.2 equivalent yet. They are
			 * listed anyway, and refuse when chosen: the character sheet
			 * already describes them, so a player who has grown a Midas touch
			 * will come here looking for it, and silence would read as a bug
			 * rather than as an honest gap.
			 */
			pending[waiting++] = mut;
		}
	}

	if (!count && !waiting) {
		msg("You have no power to call on.");
		return;
	}

	m = menu_dynamic_new();
	if (!m) return;

	labels = menu_dynamic_labels(m);

	for (i = 0; i < count; i++) {
		char line[80];

		if (player->lev < powers[i]->level)
			strnfmt(line, sizeof(line), "%-24s  level %d", powers[i]->name,
					powers[i]->level);
		else
			/*
			 * "hp" rather than "sp" when there is not enough mana, because
			 * that is what it will actually take out of you -- and for a
			 * Warrior or a Monk it is the only thing it ever takes.
			 */
			strnfmt(line, sizeof(line), "%-24s  %d %s, %d%% to fail",
					powers[i]->name, powers[i]->cost,
					player->csp < powers[i]->cost ? "hp" : "sp",
					player_power_chance(player, powers[i]));

		menu_dynamic_add_label(m, line, 0, i + 1, labels);
	}

	for (i = 0; i < waiting; i++) {
		char line[80];

		/*
		 * "not yet" is a promise; "dropped" is not. Eleven of these are
		 * waiting for a mechanism 4.2 has not got and one was refused
		 * (DEC-48), and a player who keeps checking back for the Midas touch
		 * deserves to be told it is not coming.
		 */
		strnfmt(line, sizeof(line), "%-24s  %s",
				pending[i]->power ? pending[i]->power : pending[i]->name,
				pending[i]->refused ? "dropped" : "not yet");
		menu_dynamic_add_label(m, line, 0, count + i + 1, labels);
	}

	menu_dynamic_add_label(m, "Nothing", 'q', 0, labels);

	screen_save();
	menu_dynamic_calc_location(m, 0, 0);
	region_erase_bordered(&m->boundary);
	if (player->msp)
		prt(format("You are a %s %s, and have %d of %d spell points.",
				   player->race->name, player->class->name,
				   player->csp, player->msp), 0, 0);
	else
		prt(format("You are a %s %s, and pay for this out of your own hide.",
				   player->race->name, player->class->name), 0, 0);

	chosen = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);
	screen_load();

	if (chosen <= 0 || chosen > count + waiting) return;

	if (chosen > count) {
		const struct mutation *waiting_on = pending[chosen - count - 1];

		if (waiting_on->refused) {
			msg("Chaos has given you that, and this game will not use it.");
		} else {
			msg("Chaos has given you that, and this game cannot yet use it.");
		}
		return;
	}


	power = powers[chosen - 1];

	if (player->lev < power->level) {
		msg("You are not yet able to do that.");
		return;
	}

	{
		int dir = 0;

		/*
		 * Ask where, if the power goes somewhere.  Asked here rather than left
		 * to the effect, because a power the player then declines to aim must
		 * not have cost them the mana.
		 */
		if (player_power_aims(player, power) && !get_aim_dir(&dir))
			return;

		if (player_use_power(player, power, dir))
			player->upkeep->energy_use = z_info->move_energy;
	}
}

/**
 * Show the overhead map of the world, and let the player scroll about it.
 */
void do_cmd_view_world_map(void)
{
	int size = z_info->wild_block_size;
	int wid = Term->wid - 2, hgt = Term->hgt - 4;
	struct loc origin;

	if (!wild) {
		msg("You are not in the world.");
		return;
	}

	/* Open on the player, as centred as the edges of the world allow. */
	origin.x = MIN(MAX(player->wild_grid.x / size - wid / 2, 0),
				   MAX(0, wild->blocks - wid));
	origin.y = MIN(MAX(player->wild_grid.y / size - hgt / 2, 0),
				   MAX(0, wild->blocks - hgt));

	screen_save();

	while (true) {
		struct keypress key;
		int dir;

		display_world_map(origin);
		Term_fresh();

		key = inkey();
		if (key.code == ESCAPE || key.code == 'q')
			break;

		dir = target_dir(key);
		if (!dir)
			continue;

		origin.x = MIN(MAX(origin.x + ddx[dir] * 8, 0),
					   MAX(0, wild->blocks - wid));
		origin.y = MIN(MAX(origin.y + ddy[dir] * 4, 0),
					   MAX(0, wild->blocks - hgt));
	}

	screen_load();
}


void do_cmd_view_map(void)
{
	int cy, cx;
	uint8_t w, h;
	const char *prompt = "Hit any key to continue";

	/*
	 * ZangbandTK: on the surface, the full map is the world map -- that is what
	 * "M" is for out there, and a scaled copy of the live window is not much use
	 * when the window is all you can see anyway.  No new key is taken: there are
	 * only four unbound letters left and none of them says "world", and W is the
	 * roguelike keyset's locate rather than a spare alias.
	 */
	if (player->in_wild && wild) {
		do_cmd_view_world_map();
		return;
	}

	if (Term->view_map_hook) {
		(*(Term->view_map_hook))(Term);
		return;
	}
	/* Save screen */
	screen_save();

	/* Note */
	prt("Please wait...", 0, 0);

	/* Flush */
	Term_fresh();

	/* Clear the screen */
	Term_clear();

	/* store the tile multipliers */
	w = tile_width;
	h = tile_height;
	tile_width = 1;
	tile_height = 1;

	/* Display the map */
	display_map(&cy, &cx);

	/* Show the prompt */
	put_str(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	/* Highlight the player */
	Term_gotoxy(cx, cy);

	/* Get any key */
	(void)anykey();

	/* Restore the tile multipliers */
	tile_width = w;
	tile_height = h;

	/* Load screen */
	screen_load();
}


