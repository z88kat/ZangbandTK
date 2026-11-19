/**
 * \file tcl/edges.c
 * \brief Terrain drawn from its neighbours: shorelines, roads and roofs
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
 * A tile set may carry an edges.txt beside its sheet (only the Gervais 2001
 * set does, written by scripts/tiles/build-gervais2001.py).  It says, for
 * some kinds of square, which picture to draw given which of the eight
 * squares around it are of some other kind: a grass square by water gets a
 * bank, a road joins the road beside it, a town building gets a roof.
 *
 * Every decision is in the file.  This only works out the mask and looks it
 * up, so the rules can be read, and checked, in the script that writes them.
 *
 * What counts is what the character remembers -- player->cave, the map as
 * drawn -- so a bank never gives away water nobody has seen.  And because a
 * square's picture depends on its neighbours, a square that changes has its
 * neighbours looked at again: the game redraws only the square that changed,
 * and a bank that appeared beside a newly seen pond would otherwise wait for
 * the next full redraw.
 */

#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "player.h"
#include "ui-prefs.h"
#include "ui-term.h"
#include "tcl/main-tcl.h"

enum { WHERE_ANY, WHERE_SURFACE, WHERE_DUNGEON };

#define EDGE_MASKS 512		/* eight neighbours, and the ridge bit */
#define EDGE_RIDGE 256

struct edge_rule {
	int where;
	int around;		/* 0, 4 or 8 neighbours */
	bool ridge;
	bool *grid;		/* per feature: a square this rule redraws */
	bool *near;		/* per feature: a neighbour that counts */
	uint16_t tile[EDGE_MASKS];	/* attr << 8 | char, or 0 for none */
	struct edge_rule *next;
};

static struct edge_rule *rules;

/*
 * Per grid of the current level, what was last drawn there instead of the
 * pref's picture (attr << 8 | char), or 0.  Sized to the level, and thrown
 * away when the level's size changes.
 */
static uint16_t *drawn;
static int drawn_w, drawn_h;

static const struct loc around[8] = {
	{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },	/* N E S W: bits 0-3 */
	{ 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 }	/* NE SE SW NW: 4-7 */
};

static void rules_free(void)
{
	while (rules) {
		struct edge_rule *next = rules->next;

		mem_free(rules->grid);
		mem_free(rules->near);
		mem_free(rules);
		rules = next;
	}
	mem_free(drawn);
	drawn = NULL;
	drawn_w = drawn_h = 0;
}

/* Each feature code after the first colon of `line`, set in `set`. */
static bool parse_feats(char *line, bool *set, const char *path)
{
	char *code = strtok(line, ":");

	for (code = strtok(NULL, ":"); code; code = strtok(NULL, ":")) {
		int feat = lookup_feat_code(code);

		if (feat < 0) {
			plog_fmt("Tcl/Tk: %s: no feature %s", path, code);
			return false;
		}
		set[feat] = true;
	}
	return true;
}

void tcl_edges_load(const char *dir)
{
	char path[1024], line[256];
	ang_file *fh;
	struct edge_rule *rule = NULL, **tail = &rules;
	bool ok = true;

	rules_free();
	if (!dir) return;

	path_build(path, sizeof(path), dir, "edges.txt");
	if (!file_exists(path)) return;
	fh = file_open(path, MODE_READ, FTYPE_TEXT);
	if (!fh) return;

	while (ok && file_getl(fh, line, sizeof(line))) {
		char where[16];
		int around_n, ridge, mask;
		unsigned int a, c;

		if (!line[0] || line[0] == '#') continue;

		if (sscanf(line, "rule:%*[^:]:%15[^:]:%d:%d", where, &around_n,
				&ridge) == 3) {
			rule = mem_zalloc(sizeof(*rule));
			rule->grid = mem_zalloc(FEAT_MAX * sizeof(bool));
			rule->near = mem_zalloc(FEAT_MAX * sizeof(bool));
			rule->where = streq(where, "surface") ? WHERE_SURFACE
				: streq(where, "dungeon") ? WHERE_DUNGEON : WHERE_ANY;
			rule->around = around_n;
			rule->ridge = ridge != 0;
			*tail = rule;
			tail = &rule->next;
		} else if (rule && prefix(line, "grid:")) {
			ok = parse_feats(line, rule->grid, path);
		} else if (rule && prefix(line, "near:")) {
			ok = parse_feats(line, rule->near, path);
		} else if (rule && sscanf(line, "tile:%d:%x:%x", &mask, &a, &c)
				== 3 && mask >= 0 && mask < EDGE_MASKS) {
			rule->tile[mask] = (uint16_t)((a & 0xFF) << 8 | (c & 0xFF));
		} else {
			plog_fmt("Tcl/Tk: %s: cannot read \"%s\"", path, line);
			ok = false;
		}
	}
	file_close(fh);

	/* Half a rule set draws worse than none. */
	if (!ok) rules_free();
}

bool tcl_edges_on(void)
{
	return rules != NULL;
}

/* What the character remembers at `grid`, or FEAT_NONE. */
static int remembered(struct loc grid)
{
	if (!square_in_bounds(cave, grid) || !square_in_bounds(player->cave, grid))
		return FEAT_NONE;
	return square(player->cave, grid)->feat;
}

/* Squares of `set` in a line through `grid`, up to 16 each way. */
static int run(struct loc grid, struct loc step, const bool *set)
{
	int n = 1, dir, i;

	for (dir = -1; dir <= 1; dir += 2)
		for (i = 1; i <= 16; i++) {
			struct loc g = loc(grid.x + dir * i * step.x,
					grid.y + dir * i * step.y);

			if (!set[remembered(g)]) break;
			n++;
		}
	return n;
}

/*
 * The picture for `grid`, given that the term was asked to draw terrain tile
 * (ta, tc) there: attr << 8 | char, or 0 to draw (ta, tc) as it is.
 */
static uint16_t edge_tile(struct loc grid, int ta, int tc)
{
	struct edge_rule *rule;
	int feat, light;

	if (!rules || !player || !player->cave || !cave || !character_dungeon)
		return 0;
	feat = remembered(grid);
	if (feat == FEAT_NONE) return 0;

	/*
	 * The pref drew one of its three pictures for this feature: as seen,
	 * remembered or unlit, side by side.  Which one is how far along from the
	 * seen one, and the replacement is moved along as far.  Anything else --
	 * a hallucination, a set whose pref this file does not describe -- is
	 * left alone.
	 */
	if ((ta & 0xFF) != feat_x_attr[LIGHTING_LOS][feat]) return 0;
	light = (tc & 0xFF) - (int)feat_x_char[LIGHTING_LOS][feat];
	if (light < 0 || light > 2) return 0;

	for (rule = rules; rule; rule = rule->next) {
		int mask = 0, i;
		uint16_t tile;

		if (!rule->grid[feat]) continue;
		if (rule->where == WHERE_SURFACE && player->depth) continue;
		if (rule->where == WHERE_DUNGEON && !player->depth) continue;

		for (i = 0; i < rule->around; i++) {
			struct loc g = loc_sum(grid, around[i]);

			if (rule->near[remembered(g)]) mask |= 1 << i;
		}
		if (rule->ridge && run(grid, loc(1, 0), rule->near)
				>= run(grid, loc(0, 1), rule->near))
			mask |= EDGE_RIDGE;

		tile = rule->tile[mask];
		if (tile) return (uint16_t)(tile + light);
	}
	return 0;
}

/* Where the record for `grid` is kept, or NULL; resized with the level. */
static uint16_t *record(struct loc grid)
{
	if (!cave) return NULL;
	if (drawn_w != cave->width || drawn_h != cave->height) {
		mem_free(drawn);
		drawn_w = cave->width;
		drawn_h = cave->height;
		drawn = mem_zalloc((size_t)drawn_w * drawn_h * sizeof(*drawn));
	}
	if (grid.x < 0 || grid.y < 0 || grid.x >= drawn_w || grid.y >= drawn_h)
		return NULL;
	return &drawn[grid.y * drawn_w + grid.x];
}

bool tcl_edges_terrain(struct loc grid, int *ta, int *tc)
{
	uint16_t tile = edge_tile(grid, *ta, *tc);
	uint16_t *rec = record(grid);

	if (rec) *rec = tile;
	if (!tile) return false;
	*ta = 0x80 | (tile >> 8);
	*tc = 0x80 | (tile & 0xFF);
	return true;
}

void tcl_edges_neighbours(term_data *t, struct loc grid,
		void (*redraw)(term_data *t, struct loc grid))
{
	int i;

	if (!rules) return;
	for (i = 0; i < 8; i++) {
		struct loc g = loc_sum(grid, around[i]);
		uint16_t *rec = record(g);
		int ta, tc;

		if (!rec || !tcl_map_terrain(t, g, &ta, &tc)) continue;
		if (edge_tile(g, ta, tc) != *rec) redraw(t, g);
	}
}
