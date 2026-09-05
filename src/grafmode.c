/**
 * \file grafmode.c
 * \brief Load a list of possible graphics modes.
 *
 * Copyright (c) 2011 Brett Reid
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband license":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "datafile.h"
#include "grafmode.h"
#include "init.h"

graphics_mode *graphics_modes;
graphics_mode *current_graphics_mode = NULL;
int graphics_mode_high_id;

static enum parser_error parse_graf_name(struct parser *p) {
	graphics_mode *list = parser_priv(p);
	graphics_mode *mode = mem_zalloc(sizeof(graphics_mode));
	if (!mode) {
		return PARSE_ERROR_OUT_OF_MEMORY;
	}
	mode->pNext = list;
	mode->grafID = parser_getuint(p, "index");
	my_strcpy(mode->menuname, parser_getstr(p, "menuname"), 32);

	mode->alphablend = 0;
	mode->overdrawRow = 0;
	mode->overdrawMax = 0;
	mode->cycleHues = 0;
	mode->cycleTones = 0;
	mode->cycleSpan = 0;
	my_strcpy(mode->file, "", 32);
	my_strcpy(mode->pref, "none", 32);
	
	parser_setpriv(p, mode);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_graf_directory(struct parser *p) {
	graphics_mode *mode = parser_priv(p);
	const char *dir = parser_getsym(p, "dirname");
	if (!mode) {
		return PARSE_ERROR_INVALID_VALUE;
	}

	/* Build a usable path */
	path_build(mode->path, sizeof(mode->path), ANGBAND_DIR_TILES, dir);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_graf_size(struct parser *p) {
	graphics_mode *mode = parser_priv(p);
	if (!mode) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	mode->cell_width = parser_getuint(p, "wid");
	mode->cell_height = parser_getuint(p, "hgt");
	my_strcpy(mode->file, parser_getstr(p, "filename"), 32);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_graf_pref(struct parser *p) {
	graphics_mode *mode = parser_priv(p);
	if (!mode) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	my_strcpy(mode->pref, parser_getstr(p, "prefname"), 32);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_graf_extra(struct parser *p) {
	graphics_mode *mode = parser_priv(p);
	if (!mode) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	mode->alphablend = parser_getuint(p, "alpha");
	mode->overdrawRow = parser_getuint(p, "row");
	mode->overdrawMax = parser_getuint(p, "max");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_graf_cycle(struct parser *p) {
	graphics_mode *mode = parser_priv(p);
	if (!mode) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	mode->cycleHues = parser_getuint(p, "hues");
	mode->cycleTones = parser_getuint(p, "tones");
	mode->cycleSpan = parser_getuint(p, "span");
	if (mode->cycleSpan > mode->cycleHues) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_grafmode(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);

	parser_reg(p, "name uint index str menuname", parse_graf_name);
	parser_reg(p, "directory sym dirname", parse_graf_directory);
	parser_reg(p, "size uint wid uint hgt str filename", parse_graf_size);
	parser_reg(p, "pref str prefname", parse_graf_pref);
	parser_reg(p, "extra uint alpha uint row uint max", parse_graf_extra);
	parser_reg(p, "cycle uint hues uint tones uint span", parse_graf_cycle);

	return p;
}

/**
 * Test whether a parsed graphics mode's tileset is actually on disk.
 *
 * list.txt describes the tilesets a full installation has, which is not always
 * the set a particular build ships: a small build may leave the larger ones out,
 * and the WebAssembly Makefile names the ones it stages.  A mode whose image is
 * missing used to be offered in the menu like any other and then kill the game
 * when chosen -- load_graphics() reaches the missing file and quits -- so the
 * check belongs here, once, before the mode is offered at all.
 *
 * It is also what makes a *removed* tileset harmless.  Shockbolt's set was
 * modes 5 and 6 until 3.95.0 (see LICENSE.md); a saved preference still naming
 * one finds no such mode, and the front ends fall back to ASCII rather than
 * looking for a file that is not there.
 */
static bool grafmode_is_installed(const graphics_mode *mode)
{
	char path[1024];

	if (mode->grafID == GRAPHICS_NONE) {
		return true;
	}
	path_build(path, sizeof(path), mode->path, mode->file);
	return file_exists(path);
}

static errr finish_parse_grafmode(struct parser *p) {
	graphics_mode *mode, *n;
	int max = 0;
	int count = 0;
	int i;
	
	/*
	 * See how many usable graphics modes we have and what the highest index
	 * is.  The highest index counts every mode that was described, installed
	 * or not, because graphics_mode_high_id sizes arrays that other front
	 * ends index by mode id.
	 */
	if (p) {
		mode = parser_priv(p);
		while (mode) {
			if (mode->grafID > max) {
				max = mode->grafID;
			}
			if (grafmode_is_installed(mode)) {
				count++;
			} else {
				plog_fmt("Tileset '%s' is not installed; "
					"not offering it.", mode->menuname);
			}
			mode = mode->pNext;
		}
	}

	/* Copy the loaded modes to the global variable */
	if (graphics_modes) {
		close_graphics_modes();
	}

	graphics_modes = mem_zalloc(sizeof(graphics_mode) * (count+1));
	if (p) {
		/*
		 * Filled back to front because the parsed list is in reverse
		 * order.  The index steps only for a mode that is kept, so a
		 * missing tileset leaves no gap.
		 */
		i = count - 1;
		mode = parser_priv(p);
		while (mode) {
			if (grafmode_is_installed(mode)) {
				assert(i >= 0);
				memcpy(&(graphics_modes[i]), mode,
					sizeof(graphics_mode));
				graphics_modes[i].pNext =
					&(graphics_modes[i+1]);
				--i;
			}
			mode = mode->pNext;
		}
	}

	/* Hardcode the no graphics option */
	graphics_modes[count].pNext = NULL;
	graphics_modes[count].grafID = GRAPHICS_NONE;
	graphics_modes[count].alphablend = 0;
	graphics_modes[count].overdrawRow = 0;
	graphics_modes[count].overdrawMax = 0;
	graphics_modes[count].cycleHues = 0;
	graphics_modes[count].cycleTones = 0;
	graphics_modes[count].cycleSpan = 0;
	my_strcpy(graphics_modes[count].pref, "none", 8);
	my_strcpy(graphics_modes[count].path, "", 32);
	my_strcpy(graphics_modes[count].file, "", 32);
	my_strcpy(graphics_modes[count].menuname, "None", 32);

	graphics_mode_high_id = max;

	/* Set the default graphics mode to be no graphics */
	current_graphics_mode = &(graphics_modes[count]);

	if (p) {
		mode = parser_priv(p);
		while (mode) {
			n = mode->pNext;
			mem_free(mode);
			mode = n;
		}
	
		parser_setpriv(p, NULL);
		parser_destroy(p);
	}
	return PARSE_ERROR_NONE;
}

static void print_error(const char *name, struct parser *p) {
	struct parser_state s;
	parser_getstate(p, &s);
	plog_fmt("Parse error in %s line %d column %d: %s: %s", name,
	           s.line, s.col, s.msg, parser_error_str[s.error]);
	event_signal(EVENT_MESSAGE_FLUSH);
}

bool init_graphics_modes(void) {
	char buf[1024], line[1024];
	ang_file *f;
	struct parser *p;
	int maxe, counte;
	bool result;

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_TILES, "list.txt");

	f = file_open(buf, MODE_READ, FTYPE_TEXT);
	if (!f) {
		plog_fmt("Cannot open '%s'.", buf);
		finish_parse_grafmode(NULL);
		return true;
	}
	result = true;
	maxe = get_parser_error_limit();
	counte = 0;
	p = init_parse_grafmode();
	while (file_getl(f, line, sizeof(line))) {
		errr e = parser_parse(p, line);

		if (e != PARSE_ERROR_NONE) {
			result = false;
			print_error(buf, p);
			if (maxe) {
				if (counte >= maxe - 1) {
					break;
				}
				++counte;
			}
		}
	}

	finish_parse_grafmode(p);
	file_close(f);

	return result;
}

void close_graphics_modes(void) {
	if (graphics_modes) {
		mem_free(graphics_modes);
		graphics_modes = NULL;
	}
}

graphics_mode *get_graphics_mode(uint8_t id) {
	graphics_mode *test = graphics_modes;
	while (test) {
		if (test->grafID == id) {
			return test;
		}
		test = test->pNext;
	}
	return NULL;
}

/**
 * Test for whether an attribute/character pair corresponds to a double-height
 * tile.
 * \param a Is the attribute.
 * \param c Is the character.
 * Intended for use as struct term's dblh_hook field.
 */
int is_dh_tile(int a, wchar_t c)
{
	int tileset_row;

	/*
	 * If it's not a tile (assumes tiles have high-bit set on the
	 * attribute), graphics aren't enabled, or the graphics mode doesn't
	 * use double-height tiles, it can't be double-height.
	 */
	if (!(a & 0x80) || !current_graphics_mode ||
			!current_graphics_mode->overdrawRow) {
		return 0;
	}
	/* Test the row for the tile. */
	tileset_row = a & 0x7f;
	return tileset_row >= current_graphics_mode->overdrawRow &&
		tileset_row <= current_graphics_mode->overdrawMax;
}


/**
 * Test for whether the current graphics mode can have a tile's colour
 * changed without changing its picture.
 */
bool graf_cycles(void)
{
	return current_graphics_mode &&
		current_graphics_mode->cycleHues > 1 &&
		current_graphics_mode->cycleTones > 0 &&
		current_graphics_mode->cycleSpan > 1;
}


/**
 * Rotate a graphic tile's colour, holding its shape.
 * \param attr Is the tile's attribute, which in graphics mode is its row.
 * \param step Is how many hues to move along.
 * \return The attribute of the same shape in another colour, or `attr`
 * unchanged if the current mode has not declared a layout this works on.
 *
 * Ordinarily a tileset paints colour into its art, so a monster's colour is
 * fixed the moment its tile is chosen and a shimmering monster cannot shimmer
 * -- which is why do_animation()'s work is thrown away in graphics mode.
 *
 * The Neon tileset lays its sheet out with the colour on the row and the shape
 * on the column, in blocks of `cycleHues` x `cycleTones` rows, so the same
 * drawing exists in every colour and moving between them is arithmetic on the
 * row alone.  A set that does not say so in list.txt is left alone.
 *
 * The tone is held rather than rotated: walking through every slot in the
 * block would take a bright monster into the tones meant for remembered
 * terrain, where it is nearly invisible.
 *
 * Only the first `cycleSpan` hues are walked.  The set's palette is ordered so
 * those read as a spectrum and the ones past the span do not belong in one --
 * grey, for the Neon set, because a rainbow that passes through grey looks
 * like the colour failing rather than moving.  A monster whose own colour is
 * out there keeps it and does not shimmer.
 *
 * Staying inside the row's own block keeps the result on the sheet.  A set
 * declaring `cycle:` is laid out in whole blocks by construction -- the
 * generator emits them and its --check verifies every coordinate lands in the
 * sheet -- so every slot in a block that a valid row belongs to is valid too.
 */
uint8_t graf_cycle_attr(uint8_t attr, int step)
{
	int hues, tones, span, per_block, row, block, slot, hue, tone;

	if (!(attr & 0x80) || !graf_cycles()) {
		return attr;
	}

	hues = current_graphics_mode->cycleHues;
	tones = current_graphics_mode->cycleTones;
	span = current_graphics_mode->cycleSpan;
	per_block = hues * tones;

	row = attr & 0x7f;
	block = row / per_block;
	slot = row % per_block;
	hue = slot / tones;
	tone = slot % tones;

	if (hue >= span) {
		return attr;
	}

	hue = (hue + step) % span;
	if (hue < 0) {
		hue += span;
	}

	return 0x80 | (block * per_block + hue * tones + tone);
}


/**
 * How many tones from the top of a hue a glint moves through.
 *
 * Not all of them: the last tone in a set laid out this way is for terrain the
 * player is only remembering, and it is dark enough that a glint reaching it
 * would read as the object blinking out rather than catching the light.
 */
#define GLINT_TONES 3


/**
 * Move a tile's tone, holding its hue and its shape.
 * \param attr Is the tile's attribute, which in graphics mode is its row.
 * \param step Is how many tones to move along.
 *
 * The counterpart to graf_cycle_attr(), for things that should catch the light
 * rather than change colour.  Silver is not a rainbow; it is white, then
 * slate, then white again, which is what visuals.txt has always said in text
 * mode -- `flicker:s:slate white light-dark`.  Rotating its hue instead would
 * be wrong even where it is possible, and for silver it is not possible at
 * all: grey sits outside the rainbow, so graf_cycle_attr() leaves it exactly
 * where it was and the metal simply does not move.
 *
 * Works on every hue, inside the span or out of it, because a tone is
 * something every hue has.
 */
uint8_t graf_glint_attr(uint8_t attr, int step)
{
	int hues, tones, span, per_block, row, block, slot, hue, tone;

	if (!(attr & 0x80) || !graf_cycles()) {
		return attr;
	}

	hues = current_graphics_mode->cycleHues;
	tones = current_graphics_mode->cycleTones;
	span = MIN(GLINT_TONES, tones);
	per_block = hues * tones;

	row = attr & 0x7f;
	block = row / per_block;
	slot = row % per_block;
	hue = slot / tones;
	tone = slot % tones;

	/* A tile already below the glint's range stays where it is. */
	if (tone >= span) {
		return attr;
	}

	tone = (tone + step) % span;
	if (tone < 0) {
		tone += span;
	}

	return 0x80 | (block * per_block + hue * tones + tone);
}
