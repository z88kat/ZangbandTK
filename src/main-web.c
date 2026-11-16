/**
 * \file main-web.c
 * \brief The browser front end: the game in a Web Worker, the page in HTML.
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
 *
 * This is the version zangbandtk.com/play/ serves.  It replaced the SDL2
 * front end in a canvas, whose windows, menus and prompts were pixels the page
 * could do nothing with.
 *
 * The game runs in a Web Worker (web/worker.js) and the page (web/ui.js) draws
 * what this file tells it.  Three kinds of message go up:
 *
 *   - the main term's cells, row by row, at each Term_fresh.  The page crops
 *     them to the map and draws tiles on a canvas; when the game is showing a
 *     text screen of its own (a store, the character sheet, the options) the
 *     page shows the whole term instead, as the classic screen.
 *   - what the game's events say, as JSON: the character, the pack, the
 *     equipment, the messages.  The page draws those as HTML panels.
 *   - questions.  The item prompt, yes-or-no, quantity, text and the game's
 *     small menus are asked here and answered by an HTML dialog, and each
 *     stage of character creation by the page's own birth view.  The game
 *     waits for the answer exactly as it waits for a key: ASYNCIFY unwinds the
 *     stack, the worker goes back to its event loop, and the answer resumes it.
 *
 * Keys and clicks come down the other way and go into the term as usual.
 */

#include "angband.h"

#ifdef USE_WEB

#include <stdarg.h>
#include <emscripten.h>

#include "main.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "grafmode.h"
#include "init.h"
#include "message.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-util.h"
#include "ui-birth.h"
#include "ui-command.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-term.h"
#include "z-textblock.h"

/* ------------------------------------------------------------------------
 * The JavaScript side, in web/pre.js
 * ------------------------------------------------------------------------ */

EM_JS(void, web_js_post, (const char *json), {
	Module.zbPostJson(UTF8ToString(json));
});

EM_JS(void, web_js_rows, (const int *cells, const int *ys, int nrows, int wid,
		int hgt, int cx, int cy, int cv), {
	const data = HEAP32.slice(cells >> 2, (cells >> 2) + nrows * wid * 4);
	const rows = HEAP32.slice(ys >> 2, (ys >> 2) + nrows);
	Module.zbPost({ type: 'rows', wid: wid, hgt: hgt, ys: rows, data: data,
		cx: cx, cy: cy, cv: cv }, [data.buffer, rows.buffer]);
});

EM_JS(void, web_js_send_file, (const char *path, const char *what), {
	Module.zbSendFile(UTF8ToString(path), UTF8ToString(what));
});

EM_JS(void, web_js_term_size, (int *cols, int *rows, int *tw, int *th,
		int *graf), {
	HEAP32[cols >> 2] = Module.zbCols | 0;
	HEAP32[rows >> 2] = Module.zbRows | 0;
	HEAP32[tw >> 2] = Module.zbTileW | 0;
	HEAP32[th >> 2] = Module.zbTileH | 0;
	HEAP32[graf >> 2] = Module.zbGraf | 0;
});

/* Blocks (block != 0) until there is input, or only yields to the page. */
EM_ASYNC_JS(int, web_js_wait, (int block), {
	return await Module.zbWait(block);
});

/* One queued input: 1 key (code, mods), 2 mouse (x, y, button); 0 if none. */
EM_JS(int, web_js_pop, (int *ev), {
	const e = Module.zbQueue.shift();
	if (!e) return 0;
	HEAP32[(ev >> 2) + 0] = e[1] | 0;
	HEAP32[(ev >> 2) + 1] = e[2] | 0;
	HEAP32[(ev >> 2) + 2] = e[3] | 0;
	return e[0] | 0;
});

EM_JS(void, web_js_flush_input, (void), {
	Module.zbQueue.length = 0;
});

/* Puts a question to the page and waits: the answer is a number. */
EM_ASYNC_JS(int, web_js_ask, (const char *json), {
	const answer = await Module.zbAsk(UTF8ToString(json));
	return (typeof answer === 'number') ? answer : -1;
});

/* The same, for an answer that is text: 1 and the text in buf, or 0. */
EM_ASYNC_JS(int, web_js_ask_text, (const char *json, char *buf, int len), {
	const answer = await Module.zbAsk(UTF8ToString(json));
	if (typeof answer !== 'string') return 0;
	stringToUTF8(answer, buf, len);
	return 1;
});

EM_ASYNC_JS(void, web_js_sleep, (int ms), {
	await new Promise(function (r) { setTimeout(r, ms); });
});

/* The tileset the page has asked for since the last look, or 0. */
EM_JS(int, web_js_take_graf, (void), {
	const g = Module.zbWantGraf | 0;
	Module.zbWantGraf = 0;
	return g;
});

EM_JS(void, web_js_quit, (const char *why), {
	Module.zbQuit(UTF8ToString(why));
});

/* ------------------------------------------------------------------------
 * A little JSON
 * ------------------------------------------------------------------------ */

struct jb {
	char *buf;
	size_t len, size;
};

static void jb_init(struct jb *b)
{
	b->size = 4096;
	b->buf = mem_alloc(b->size);
	b->len = 0;
	b->buf[0] = 0;
}

static void jb_free(struct jb *b)
{
	mem_free(b->buf);
	b->buf = NULL;
}

static void jb_raw(struct jb *b, const char *s, size_t n)
{
	if (b->len + n + 1 > b->size) {
		while (b->len + n + 1 > b->size) b->size *= 2;
		b->buf = mem_realloc(b->buf, b->size);
	}
	memcpy(b->buf + b->len, s, n);
	b->len += n;
	b->buf[b->len] = 0;
}

static void jb_put(struct jb *b, const char *s)
{
	jb_raw(b, s, strlen(s));
}

static void jb_fmt(struct jb *b, const char *fmt, ...)
{
	char tmp[256];
	va_list vp;
	int n;

	va_start(vp, fmt);
	n = vsnprintf(tmp, sizeof(tmp), fmt, vp);
	va_end(vp);
	if (n < 0) return;
	jb_raw(b, tmp, MIN((size_t)n, sizeof(tmp) - 1));
}

/* A string, quoted.  The game's text is UTF-8 already. */
static void jb_str(struct jb *b, const char *s)
{
	jb_put(b, "\"");
	for (; s && *s; s++) {
		unsigned char ch = (unsigned char)*s;

		if (ch == '"') jb_put(b, "\\\"");
		else if (ch == '\\') jb_put(b, "\\\\");
		else if (ch < 0x20) jb_fmt(b, "\\u%04x", ch);
		else jb_raw(b, s, 1);
	}
	jb_put(b, "\"");
}

static void jb_key(struct jb *b, const char *key)
{
	if (b->len && b->buf[b->len - 1] != '{' && b->buf[b->len - 1] != '[')
		jb_put(b, ",");
	jb_str(b, key);
	jb_put(b, ":");
}

static void jb_kstr(struct jb *b, const char *key, const char *val)
{
	jb_key(b, key);
	jb_str(b, val);
}

static void jb_kint(struct jb *b, const char *key, long val)
{
	jb_key(b, key);
	jb_fmt(b, "%ld", val);
}

static void jb_kbool(struct jb *b, const char *key, bool val)
{
	jb_key(b, key);
	jb_put(b, val ? "true" : "false");
}

/* A comma before the next element of an array, unless it is the first. */
static void jb_next(struct jb *b)
{
	if (b->len && b->buf[b->len - 1] != '[') jb_put(b, ",");
}

/* ------------------------------------------------------------------------
 * The term
 * ------------------------------------------------------------------------ */

static term term_main;

/* Which rows Term_fresh drew since they were last sent. */
static bool *row_dirty;
static bool rows_any_dirty;
static int *row_cells;
static int *row_ys;

/* What the page was last told about the layout, so it is told only changes. */
static char last_state[256];

static bool status_dirty = true;
static bool gear_dirty = true;

static double last_yield;

/*
 * Has the game drawn over the map since it last saved the screen?  A saved
 * screen alone does not say so: the inventory command saves it and then asks
 * for an item, which is a dialog here, and the map behind is untouched.  Text
 * drawn across the map's columns -- a store, a list, a description -- is the
 * sign the page has to show the whole term to make sense of it.
 */
static bool drawn_over_map;

static void mark_row(int y)
{
	if (y >= 0 && y < term_main.hgt) {
		row_dirty[y] = true;
		rows_any_dirty = true;
	}
}

static void note_draw(int x, int y, int n)
{
	mark_row(y);
	if (screen_save_depth > 0 && y >= ROW_MAP && y < Term->hgt - 1
			&& x + n > COL_MAP)
		drawn_over_map = true;
}

static errr web_text(int x, int y, int n, int a, const wchar_t *s)
{
	(void)a; (void)s;
	note_draw(x, y, n);
	return 0;
}

static errr web_pict(int x, int y, int n, const int *ap, const wchar_t *cp,
		const int *tap, const wchar_t *tcp)
{
	(void)ap; (void)cp; (void)tap; (void)tcp;
	note_draw(x, y, n);
	return 0;
}

static errr web_wipe(int x, int y, int n)
{
	note_draw(x, y, n);
	return 0;
}

static errr web_curs(int x, int y)
{
	(void)x; (void)y;
	return 0;
}

/*
 * Send the rows Term_fresh has just drawn, read back from what the term holds
 * as displayed, with the cursor.  Called from TERM_XTRA_FRESH, which comes at
 * the end of Term_fresh, so every row it drew has been marked by then.
 */
static void send_rows(void)
{
	term *t = &term_main;
	term_win *w = t->old;
	int n = 0, y, x, k = 0;
	bool cv = !w->cu && w->cv;

	for (y = 0; y < t->hgt; y++) {
		if (!row_dirty[y]) continue;
		row_ys[n++] = y;
		for (x = 0; x < t->wid; x++) {
			row_cells[k++] = w->a[y][x];
			row_cells[k++] = (int)w->c[y][x];
			row_cells[k++] = w->ta[y][x];
			row_cells[k++] = (int)w->tc[y][x];
		}
		row_dirty[y] = false;
	}
	rows_any_dirty = false;

	web_js_rows(row_cells, row_ys, n, t->wid, t->hgt, w->cx, w->cy, cv);
}

/*
 * Is the game showing a screen of its own, which the page cannot crop to a
 * map?  Then the page shows the whole term: before there is a dungeon there is
 * no map at all, and once the game has drawn over the map it is showing a
 * store, a list or the character sheet.
 */
static bool showing_text_screen(void)
{
	if (screen_save_depth == 0) drawn_over_map = false;
	if (!character_dungeon) return true;
	if (player && player->is_dead) return true;
	return drawn_over_map;
}

static void send_state(void)
{
	char buf[256];
	term *old = Term;

	Term_activate(&term_main);
	strnfmt(buf, sizeof(buf),
		"{\"type\":\"state\",\"overlay\":%s,\"mapRow\":%d,\"mapCol\":%d,"
		"\"mapW\":%d,\"mapH\":%d,\"tw\":%d,\"th\":%d}",
		showing_text_screen() ? "true" : "false", ROW_MAP, COL_MAP,
		SCREEN_WID * tile_width, SCREEN_HGT * tile_height,
		tile_width, tile_height);
	Term_activate(old);

	if (streq(buf, last_state)) return;
	my_strcpy(last_state, buf, sizeof(last_state));
	web_js_post(buf);
}

static void send_palette(void)
{
	struct jb b;
	int i;

	jb_init(&b);
	jb_put(&b, "{\"type\":\"palette\",\"colors\":[");
	for (i = 0; i < MAX_COLORS; i++) {
		jb_next(&b);
		jb_fmt(&b, "\"#%02x%02x%02x\"", angband_color_table[i][1],
			angband_color_table[i][2], angband_color_table[i][3]);
	}
	jb_put(&b, "]}");
	web_js_post(b.buf);
	jb_free(&b);
}

/* ------------------------------------------------------------------------
 * The panels: the character, the pack and the equipment
 * ------------------------------------------------------------------------ */

static void send_status(void)
{
	struct jb b;
	char buf[80];
	const char *title;
	int i;

	if (!character_dungeon || !player || !player->race || !player->class)
		return;

	if (player->wizard) title = "[=-WIZARD-=]";
	else if (player->total_winner || player->lev > PY_MAX_LEVEL)
		title = "***WINNER***";
	else if (player_is_shapechanged(player)) title = player->shape->name;
	else title = player->class->title[(player->lev - 1) / 5];

	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "type", "status");
	jb_kstr(&b, "name", player->full_name);
	jb_kstr(&b, "race", player->race->name);
	jb_kstr(&b, "cls", player->class->name);
	jb_kstr(&b, "title", title);
	jb_kint(&b, "lev", player->lev);
	jb_kint(&b, "maxLev", player->max_lev);
	jb_kint(&b, "exp", player->exp);
	jb_kint(&b, "maxExp", player->max_exp);
	jb_kint(&b, "next", player->lev < PY_MAX_LEVEL
		? (long)(player_exp[player->lev - 1] * player->expfact / 100L) : 0);
	jb_kint(&b, "gold", player->au);
	jb_kint(&b, "hp", player->chp);
	jb_kint(&b, "mhp", player->mhp);
	jb_kint(&b, "hpColor", player_hp_attr(player));
	jb_kbool(&b, "hasMana", player_has_mana(player));
	jb_kint(&b, "sp", player->csp);
	jb_kint(&b, "msp", player->msp);
	jb_kint(&b, "ac", player->known_state.ac + player->known_state.to_a);
	jb_kint(&b, "speed", player->state.speed - 110);
	fmt_depth(buf, sizeof(buf));
	jb_kstr(&b, "depth", buf);
	jb_kint(&b, "dlev", player->depth);
	jb_kint(&b, "turn", (long)(turn / 10));

	jb_key(&b, "stats");
	jb_put(&b, "[");
	for (i = 0; i < STAT_MAX; i++) {
		char name[16];
		size_t n;

		cnv_stat(player->state.stat_use[i], buf, sizeof(buf));
		my_strcpy(name, stat_names[i], sizeof(name));
		for (n = strlen(name); n > 0 && (name[n - 1] == ' '
				|| name[n - 1] == ':'); n--)
			name[n - 1] = 0;

		jb_next(&b);
		jb_put(&b, "{");
		jb_kstr(&b, "name", name);
		jb_kstr(&b, "value", buf);
		jb_kbool(&b, "reduced",
			player->stat_cur[i] < player->stat_max[i]);
		jb_kbool(&b, "maxed", player->stat_max[i] == 18 + 100);
		jb_put(&b, "}");
	}
	jb_put(&b, "]}");

	web_js_post(b.buf);
	jb_free(&b);
}

/*
 * An object as the page draws it: its letter, name, colour, weight and tile.
 * The tile is a cell of the tile sheet, row and column, or failing that a
 * character in a colour, which is what object_attr() and object_char() give
 * when there are no tiles for it.
 */
static void jb_object(struct jb *b, struct object *obj, const char *label)
{
	char name[160];
	uint8_t a = object_attr(obj);
	wchar_t c = object_char(obj);
	uint8_t colour = obj->kind->base->attr;

	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	if (tval_is_book_k(obj->kind) && !player_object_to_book(player, obj))
		colour = COLOUR_SLATE;

	jb_put(b, "{");
	jb_kstr(b, "l", label);
	jb_kstr(b, "n", name);
	jb_kint(b, "c", colour);
	jb_kint(b, "w", obj->weight * obj->number);
	if ((a & 0x80) && (c & 0x80)) {
		jb_key(b, "t");
		jb_fmt(b, "[%d,%d]", a & 0x7f, (int)(c & 0x7f));
	} else {
		char glyph[8] = "";

		wctomb(glyph, c);
		jb_kstr(b, "g", glyph);
		jb_kint(b, "ga", a);
	}
	jb_put(b, "}");
}

static void send_gear(void)
{
	struct jb b;
	char label[2] = "";
	int i;

	if (!character_dungeon || !player || !player->upkeep) return;

	jb_init(&b);
	jb_put(&b, "{\"type\":\"gear\",\"inven\":[");
	for (i = 0; i < z_info->pack_size; i++) {
		struct object *obj = player->upkeep->inven[i];

		if (!obj) break;
		label[0] = gear_to_label(player, obj);
		jb_next(&b);
		jb_object(&b, obj, label);
	}
	jb_put(&b, "],\"equip\":[");
	for (i = 0; i < player->body.count; i++) {
		struct object *obj = slot_object(player, i);

		jb_next(&b);
		if (obj) {
			label[0] = gear_to_label(player, obj);
			jb_put(&b, "{\"slot\":");
			jb_str(&b, player->body.slots[i].name);
			jb_put(&b, ",\"obj\":");
			jb_object(&b, obj, label);
			jb_put(&b, "}");
		} else {
			jb_put(&b, "{\"slot\":");
			jb_str(&b, player->body.slots[i].name);
			jb_put(&b, "}");
		}
	}
	jb_put(&b, "],\"quiver\":[");
	for (i = 0; i < z_info->quiver_size; i++) {
		struct object *obj = player->upkeep->quiver[i];

		if (!obj) continue;
		label[0] = gear_to_label(player, obj);
		jb_next(&b);
		jb_object(&b, obj, label);
	}
	jb_fmt(&b, "],\"burden\":%d}", (int)player->upkeep->total_weight);

	web_js_post(b.buf);
	jb_free(&b);
}

/* Everything the page should know before it is asked anything. */
static void sync_page(void)
{
	if (rows_any_dirty) send_rows();
	send_state();
	if (status_dirty) {
		send_status();
		status_dirty = false;
	}
	if (gear_dirty) {
		send_gear();
		gear_dirty = false;
	}
}

static void notice_status(game_event_type type, game_event_data *data,
		void *user)
{
	(void)type; (void)data; (void)user;
	status_dirty = true;
}

static void notice_gear(game_event_type type, game_event_data *data,
		void *user)
{
	(void)type; (void)data; (void)user;
	gear_dirty = true;
	status_dirty = true;
}

static void notice_message(game_event_type type, game_event_data *data,
		void *user)
{
	struct jb b;

	(void)type; (void)user;
	if (!data || !data->message.msg) return;

	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "type", "msg");
	jb_kstr(&b, "text", data->message.msg);
	jb_kint(&b, "color", message_type_color(data->message.type));
	jb_put(&b, "}");
	web_js_post(b.buf);
	jb_free(&b);
}

/* ------------------------------------------------------------------------
 * Tilesets
 * ------------------------------------------------------------------------ */

/*
 * The page draws every tile square, so a set whose cells are not square --
 * Nomad's are 8 by 16, made to sit in a text cell -- is never offered.
 * Makefile.web leaves Nomad out of the data, so it is not even listed; this
 * is for a set added to list.txt later.
 */
static bool tileset_usable(const graphics_mode *mode)
{
	return mode && mode->grafID != GRAPHICS_NONE
		&& mode->cell_width == mode->cell_height;
}

static void send_tilesets(void)
{
	struct jb b;
	graphics_mode *mode;

	jb_init(&b);
	jb_put(&b, "{\"type\":\"tilesets\",\"list\":[");
	for (mode = graphics_modes; mode; mode = mode->pNext) {
		if (mode->grafID == GRAPHICS_NONE) continue;
		jb_next(&b);
		jb_put(&b, "{");
		jb_kint(&b, "id", mode->grafID);
		jb_kstr(&b, "name", mode->menuname);
		jb_kbool(&b, "ok", tileset_usable(mode));
		jb_put(&b, "}");
	}
	jb_fmt(&b, "],\"current\":%d}",
		current_graphics_mode ? current_graphics_mode->grafID : 0);
	web_js_post(b.buf);
	jb_free(&b);
}

/* Make a tileset the current one and hand its sheet to the page. */
static void use_tileset(graphics_mode *mode)
{
	char path[1024];
	char head[256];

	current_graphics_mode = mode;
	use_graphics = mode->grafID;

	strnfmt(head, sizeof(head),
		"{\"type\":\"tileinfo\",\"id\":%d,\"w\":%d,\"h\":%d}",
		mode->grafID, mode->cell_width, mode->cell_height);
	web_js_post(head);
	path_build(path, sizeof(path), mode->path, mode->file);
	web_js_send_file(path, "tiles");
}

/*
 * A tileset chosen on the page while the game runs.  It waits for the map:
 * redrawing everything from inside a store or a menu would draw the map over
 * whatever the game has on the screen.
 */
static int pending_graf;

static void switch_tileset(void)
{
	graphics_mode *mode;
	int want = web_js_take_graf();

	if (want) pending_graf = want;
	if (!pending_graf || !character_dungeon || screen_save_depth > 0) return;

	mode = get_graphics_mode((uint8_t)pending_graf);
	pending_graf = 0;
	if (!tileset_usable(mode) || mode == current_graphics_mode) return;

	use_tileset(mode);
	reset_visuals(true);
	do_cmd_redraw();
	send_tilesets();
	gear_dirty = true;
}

/* ------------------------------------------------------------------------
 * Input
 * ------------------------------------------------------------------------ */

static void take_input(void)
{
	int ev[3];
	int kind;

	while ((kind = web_js_pop(ev)) != 0) {
		if (kind == 1) {
			Term_keypress((keycode_t)ev[0], (uint8_t)ev[1]);
		} else if (kind == 2) {
			Term_mousepress(ev[0], ev[1], (char)ev[2]);
		}
	}
}

static void install_hooks(void);

static errr web_event(int wait)
{
	install_hooks();
	sync_page();

	if (wait) {
		web_js_wait(1);
		last_yield = emscripten_get_now();
	} else if (emscripten_get_now() - last_yield > 40.0) {
		/*
		 * The game asks without waiting while it rests or runs, to see
		 * whether a key should stop it.  A worker hears about a key only
		 * from its event loop, so let it go round now and then.
		 */
		web_js_wait(0);
		last_yield = emscripten_get_now();
	}

	take_input();
	switch_tileset();
	return 0;
}

static errr web_xtra(int n, int v)
{
	switch (n) {
		case TERM_XTRA_EVENT:
			return web_event(v);
		case TERM_XTRA_FLUSH:
			web_js_flush_input();
			return 0;
		case TERM_XTRA_CLEAR: {
			int y;

			for (y = 0; y < term_main.hgt; y++) mark_row(y);
			if (screen_save_depth > 0) drawn_over_map = true;
			return 0;
		}
		case TERM_XTRA_FRESH:
			send_rows();
			return 0;
		case TERM_XTRA_DELAY:
			if (v > 0) {
				if (rows_any_dirty) send_rows();
				web_js_sleep(v);
			}
			return 0;
		case TERM_XTRA_REACT:
			send_palette();
			return 0;
		default:
			return 0;
	}
}

/* ------------------------------------------------------------------------
 * The game's questions, asked as dialogs
 * ------------------------------------------------------------------------ */

/* The top line of the term, which is where the game puts its prompts. */
static void top_line(char *buf, size_t len)
{
	term_win *w = term_main.old;
	size_t n = 0;
	int x;

	for (x = 0; x < term_main.wid && n + 5 < len; x++) {
		char u[8];
		int k = wctomb(u, w->c[0][x]);

		if (k <= 0) continue;
		memcpy(buf + n, u, k);
		n += k;
	}
	buf[n] = 0;
	while (n > 0 && buf[n - 1] == ' ') buf[--n] = 0;
}

/* Before a dialog: messages read, the screen current, the panels too. */
static void before_asking(void)
{
	event_signal(EVENT_MESSAGE_FLUSH);
	Term_fresh();
	sync_page();
}

static bool web_get_check(const char *prompt)
{
	struct jb b;
	int answer;

	before_asking();
	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "check");
	jb_kstr(&b, "prompt", prompt ? prompt : "");
	jb_put(&b, "}");
	answer = web_js_ask(b.buf);
	jb_free(&b);

	return answer == 1;
}

static int web_get_quantity(const char *prompt, int max)
{
	struct jb b;
	int answer;

	if (max == 1) return 1;

	before_asking();
	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "quantity");
	jb_kstr(&b, "prompt", prompt ? prompt : "Quantity");
	jb_kint(&b, "max", max);
	jb_put(&b, "}");
	answer = web_js_ask(b.buf);
	jb_free(&b);

	if (answer < 0) return 0;
	return MIN(answer, max);
}

static bool web_get_string(const char *prompt, char *buf, size_t len)
{
	struct jb b;
	int ok;

	before_asking();
	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "string");
	jb_kstr(&b, "prompt", prompt ? prompt : "");
	jb_kstr(&b, "value", buf);
	jb_kint(&b, "max", (long)len - 1);
	jb_put(&b, "}");
	ok = web_js_ask_text(b.buf, buf, (int)len);
	jb_free(&b);

	return ok != 0;
}

/* Every dynamic menu: the item actions, the powers, the town's services. */
static int web_menu_select(struct menu *m)
{
	struct jb b;
	char prompt[160];
	int count = menu_dynamic_count(m);
	int i, answer, value = -1;
	bool valid = false;

	before_asking();
	top_line(prompt, sizeof(prompt));

	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "menu");
	jb_kstr(&b, "prompt", prompt[0] ? prompt : "Choose one");
	jb_key(&b, "items");
	jb_put(&b, "[");
	for (i = 0; i < count; i++) {
		char label[2] = "";
		int v;
		bool ok;
		const char *text = menu_dynamic_entry(m, i, &v, &ok, &label[0]);

		jb_next(&b);
		jb_put(&b, "{");
		jb_kstr(&b, "l", label);
		jb_kstr(&b, "n", text ? text : "");
		jb_kbool(&b, "ok", ok);
		jb_put(&b, "}");
	}
	jb_put(&b, "]}");
	answer = web_js_ask(b.buf);
	jb_free(&b);

	if (answer < 0 || answer >= count) return -1;
	menu_dynamic_entry(m, answer, &value, &valid, NULL);
	if (!valid) return -1;
	m->cursor = answer;
	return value;
}

static const char *item_where(struct object *obj)
{
	if (object_is_equipped(player->body, obj)) return "equip";
	if (object_is_in_quiver(player, obj)) return "quiver";
	if (object_is_carried(player, obj)) return "inven";
	return "floor";
}

static bool web_get_item(struct object **choice, const char *pmt,
		const char *str, cmd_code cmd, item_tester tester, int mode)
{
	int max = z_info->pack_size + z_info->quiver_size + player->body.count
		+ z_info->floor_size;
	struct object **offered = mem_zalloc(max * sizeof(*offered));
	int count = scan_items(offered, max, player, mode, tester);
	int i, answer, floor_n = 0;
	const char *start = NULL;
	struct jb b;
	bool ok = false;

	*choice = NULL;
	if (!count) {
		mem_free(offered);
		if (str) msg("%s", str);
		return false;
	}

	/* Where the game's own prompt would have started */
	if (player->upkeep->command_wrk == USE_EQUIP) start = "equip";
	else if (player->upkeep->command_wrk == USE_QUIVER) start = "quiver";
	else if (player->upkeep->command_wrk == USE_FLOOR) start = "floor";
	else if ((mode & QUIVER_TAGS) && cmd != CMD_USE) start = "quiver";

	before_asking();
	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "item");
	jb_kstr(&b, "prompt", pmt ? pmt : "Which item?");
	if (start) jb_kstr(&b, "start", start);
	jb_key(&b, "items");
	jb_put(&b, "[");
	for (i = 0; i < count; i++) {
		const char *where = item_where(offered[i]);
		char label[2] = "";

		label[0] = streq(where, "floor") ? (char)('a' + floor_n++)
			: gear_to_label(player, offered[i]);
		jb_next(&b);
		jb_object(&b, offered[i], label);
		/* Close the object early to add where it is */
		b.len--;
		b.buf[b.len] = 0;
		jb_kstr(&b, "where", where);
		jb_put(&b, "}");
	}
	jb_put(&b, "]}");
	answer = web_js_ask(b.buf);
	jb_free(&b);

	if (answer >= 0 && answer < count) {
		int kmode = OPT(player, rogue_like_commands)
			? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

		if (get_item_allow(offered[answer], cmd_lookup_key(cmd, kmode), cmd,
				(mode & IS_HARMLESS) ? true : false)) {
			*choice = offered[answer];
			ok = true;
		}
	}

	player->upkeep->command_wrk = 0;
	mem_free(offered);
	return ok;
}

/*
 * The text front end sets its own prompts in textui_input_init(), after this
 * file's init has run, so ours go in at the first wait for input -- and again
 * should anything put the text ones back.
 */
static void install_hooks(void)
{
	if (get_item_hook) get_item_hook = web_get_item;
	if (get_check_hook) get_check_hook = web_get_check;
	if (get_quantity_hook) get_quantity_hook = web_get_quantity;
	if (get_string_hook) get_string_hook = web_get_string;
	menu_dynamic_select_hook = web_menu_select;
}

/* ------------------------------------------------------------------------
 * Character creation
 * ------------------------------------------------------------------------ */

/*
 * ui-birth.c keeps the stages, their order, stepping back and what each one
 * pushes, and asks its view for each stage's answer through birth_view_hook
 * -- the hook the Tk front end's Birth window answers.  This view sends the
 * page everything the stage is about and turns the page's reply, a verb and
 * perhaps an argument, into the answer the stage would have had from a key.
 */

/* The point costs, as the game last reported them (EVENT_BIRTHPOINTS). */
static int birth_spent[STAT_MAX], birth_cost[STAT_MAX], birth_left;

static void notice_birth_points(game_event_type type, game_event_data *data,
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
static struct jb *birth_capture;

static void birth_text_out(uint8_t attr, const char *str)
{
	(void)attr;
	jb_put(birth_capture, str);
}

/* A race or class described in the words the text screen puts beside it. */
static void jb_birth_describe(struct jb *b, enum birth_stage stage, int row)
{
	struct jb text;
	void (*old)(uint8_t, const char *) = text_out_hook;
	int indent = text_out_indent;

	jb_init(&text);
	birth_capture = &text;
	text_out_hook = birth_text_out;
	text_out_indent = 0;
	birth_describe(stage, row);
	text_out_hook = old;
	text_out_indent = indent;
	birth_capture = NULL;

	jb_kstr(b, "d", text.buf);
	jb_free(&text);
}

/*
 * A realm has nothing beside it on the text screen, so it is described as
 * the Tk Birth window describes it: what its magic is and which stat it
 * draws on, then this class's books in it, spell by spell.
 */
static void jb_realm_describe(struct jb *b, const struct magic_realm *realm)
{
	const struct class_magic *magic = &player->class->magic;
	struct jb text;
	char stat[16], line[160];
	int bk, k;

	jb_init(&text);
	my_strcpy(stat, stat_names[realm->stat], sizeof(stat));
	for (k = (int)strlen(stat); k > 0
			&& (stat[k - 1] == ' ' || stat[k - 1] == ':'); k--)
		stat[k - 1] = 0;
	strnfmt(line, sizeof(line), "You %s %ss from %ss, and study them with %s.\n",
		realm->verb, realm->spell_noun, realm->book_noun, stat);
	jb_put(&text, line);

	for (bk = 0; bk < magic->num_books; bk++) {
		const struct class_book *book = &magic->books[bk];
		struct object_kind *kind = lookup_kind(book->tval, book->sval);
		char name[80];

		if (book->realm != realm || !kind) continue;
		object_kind_name(name, sizeof(name), kind, true);
		strnfmt(line, sizeof(line), "\n%s%s\n", name,
			book->dungeon ? " (found in the dungeon)" : "");
		jb_put(&text, line);
		for (k = 0; k < book->num_spells; k++) {
			strnfmt(line, sizeof(line), "  %2d  %s\n",
				book->spells[k].slevel, book->spells[k].name);
			jb_put(&text, line);
		}
	}
	jb_kstr(b, "d", text.buf);
	jb_free(&text);
	jb_kstr(b, "p", realm->desc ? realm->desc : "");
}

static const char *race_prose(const char *name)
{
	struct player_race *r;

	for (r = races; r; r = r->next)
		if (streq(r->name, name)) return r->desc ? r->desc : "";
	return "";
}

static const char *class_prose(const char *name)
{
	struct player_class *c;

	for (c = classes; c; c = c->next)
		if (streq(c->name, name)) return c->desc ? c->desc : "";
	return "";
}

/* A stat as the game prints it, "18/50", without its padding. */
static void jb_kstat(struct jb *b, const char *key, int value)
{
	char buf[32];
	char *p = buf;

	cnv_stat(value, buf, sizeof(buf));
	while (*p == ' ') p++;
	jb_kstr(b, key, p);
}

static void jb_birth_character(struct jb *b)
{
	int i;

	jb_key(b, "character");
	jb_put(b, "{");
	jb_kstr(b, "name", player->full_name);
	jb_kstr(b, "race", player->race->name);
	jb_kstr(b, "cls", player->class->name);
	jb_key(b, "realms");
	jb_put(b, "[");
	for (i = 0; i < REALM_CHOICES; i++) {
		if (!player->realm[i]) continue;
		jb_next(b);
		jb_str(b, player->realm[i]->name);
	}
	jb_put(b, "]");
	jb_kstr(b, "patron", player->patron ? player->patron->name : "");
	jb_kstr(b, "patronTitle", (player->patron && player->patron->title)
		? player->patron->title : "");
	jb_kstr(b, "patronText", (player->patron && player->patron->text)
		? player->patron->text : "");
	jb_kstr(b, "history", player->history ? player->history : "");
	jb_kint(b, "hp", player->mhp);
	jb_kint(b, "sp", player->msp);
	jb_kint(b, "age", player->age);
	jb_kint(b, "ht", player->ht);
	jb_kint(b, "wt", player->wt);

	jb_key(b, "stats");
	jb_put(b, "[");
	for (i = 0; i < STAT_MAX; i++) {
		char name[16];
		size_t n;

		my_strcpy(name, stat_names[i], sizeof(name));
		for (n = strlen(name); n > 0 && (name[n - 1] == ' '
				|| name[n - 1] == ':'); n--)
			name[n - 1] = 0;
		jb_next(b);
		jb_put(b, "{");
		jb_kstr(b, "name", name);
		jb_kstat(b, "base", player->stat_max[i]);
		jb_kint(b, "race", player->race->r_adj[i]);
		jb_kint(b, "cls", player->class->c_adj[i]);
		jb_kstat(b, "best", player->state.stat_top[i]);
		jb_kint(b, "spent", birth_spent[i]);
		jb_kint(b, "cost", birth_cost[i]);
		jb_put(b, "}");
	}
	jb_put(b, "]");
	jb_kint(b, "left", birth_left);
	jb_put(b, "}");
}

/* The page's reply, a verb and perhaps an argument, as the stage's answer. */
static void birth_reply(enum birth_stage stage, const char *reply,
		struct birth_answer *a)
{
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
	char verb[16];
	const char *arg = strchr(reply, ' ');
	size_t v, n = arg ? (size_t)(arg - reply) : strlen(reply);

	my_strcpy(verb, reply, MIN(n + 1, sizeof(verb)));
	if (arg) arg++;

	a->key.type = EVT_KBRD;
	a->key.code = ESCAPE;
	a->key.mods = 0;
	for (v = 0; v < N_ELEMENTS(verbs); v++) {
		if (!streq(verbs[v].verb, verb)) continue;
		a->key.code = (keycode_t)verbs[v].key;
		if (verbs[v].arg == 1 && arg) a->choice = atoi(arg);
		if (verbs[v].arg == 2 && arg) my_strcpy(a->text, arg, sizeof(a->text));
		break;
	}
	/* The quick start takes its "use as it is" as 'y', not Enter. */
	if (streq(verb, "accept") && stage == BIRTH_QUICKSTART) a->key.code = 'y';
}

static bool history_given;

static void web_birth_view(enum birth_stage stage, int slot,
		struct birth_answer *answer)
{
	struct jb b;
	char reply[300];
	const char *names[64];
	int n, i, cursor;

	/*
	 * A new history has gone in and the stage asks again; the page's Next
	 * meant accept it as well, as the Tk window's does.
	 */
	if (stage == BIRTH_HISTORY_CHOICE && history_given) {
		history_given = false;
		birth_reply(stage, "accept", answer);
		return;
	}
	history_given = false;

	install_hooks();
	Term_fresh();
	sync_page();

	jb_init(&b);
	jb_put(&b, "{");
	jb_kstr(&b, "kind", "birth");
	jb_kstr(&b, "stage", birth_stage_word(stage));
	jb_kint(&b, "slot", slot);
	jb_kstr(&b, "question", birth_question(stage));

	n = birth_choices(stage, names, (int)N_ELEMENTS(names), &cursor);
	jb_kint(&b, "cursor", cursor);
	jb_key(&b, "choices");
	jb_put(&b, "[");
	if (stage == BIRTH_REALM_CHOICE && player && player->class) {
		const struct magic_realm *offered[REALM_MAX];
		int k = player_realm_offer(player, slot, offered, REALM_MAX);

		for (i = 0; i < n && i < k; i++) {
			jb_next(&b);
			jb_put(&b, "{");
			jb_kstr(&b, "n", names[i] ? names[i] : "");
			jb_realm_describe(&b, offered[i]);
			jb_put(&b, "}");
		}
	} else {
		for (i = 0; i < n; i++) {
			const char *name = names[i] ? names[i] : "";

			jb_next(&b);
			jb_put(&b, "{");
			jb_kstr(&b, "n", name);
			if (stage == BIRTH_RACE_CHOICE || stage == BIRTH_CLASS_CHOICE) {
				jb_birth_describe(&b, stage, i);
				jb_kstr(&b, "p", stage == BIRTH_RACE_CHOICE
					? race_prose(name) : class_prose(name));
			}
			jb_put(&b, "}");
		}
	}
	jb_put(&b, "]");

	if (player && player->race && player->class) jb_birth_character(&b);

	if (stage == BIRTH_NAME_CHOICE) {
		jb_key(&b, "names");
		jb_put(&b, "[");
		for (i = 0; i < 12; i++) {
			char name[PLAYER_NAME_LEN];

			player_random_name(name, sizeof(name));
			jb_next(&b);
			jb_str(&b, name);
		}
		jb_put(&b, "]");
	}
	jb_put(&b, "}");

	if (!web_js_ask_text(b.buf, reply, sizeof(reply)))
		my_strcpy(reply, "back", sizeof(reply));
	jb_free(&b);

	birth_reply(stage, reply, answer);
	if (stage == BIRTH_HISTORY_CHOICE && answer->key.code == 'n')
		history_given = true;
}

/* ------------------------------------------------------------------------
 * Starting and stopping
 * ------------------------------------------------------------------------ */

static void web_quit(const char *s)
{
	web_js_quit(s ? s : "");
}

const char help_web[] = "Browser front end (a Web Worker and an HTML page)";

errr init_web(int argc, char **argv)
{
	term *t = &term_main;
	int cols, rows, tw, th, graf;
	graphics_mode *mode;

	(void)argc;
	(void)argv;

	web_js_term_size(&cols, &rows, &tw, &th, &graf);
	if (cols < 80) cols = 80;
	if (rows < 24) rows = 24;
	if (tw < 1) tw = 1;
	if (th < 1) th = 1;

	row_dirty = mem_zalloc(rows * sizeof(*row_dirty));
	row_cells = mem_zalloc(rows * cols * 4 * sizeof(*row_cells));
	row_ys = mem_zalloc(rows * sizeof(*row_ys));

	term_init(t, cols, rows, 1024);
	t->higher_pict = true;
	t->soft_cursor = true;
	t->complex_input = true;
	t->never_frosh = true;
	t->xtra_hook = web_xtra;
	t->curs_hook = web_curs;
	t->bigcurs_hook = web_curs;
	t->wipe_hook = web_wipe;
	t->text_hook = web_text;
	t->pict_hook = web_pict;
	Term_activate(t);
	angband_term[0] = t;

	/*
	 * Tiles, sized in cells so that a tile is square on the page.  Neon
	 * unless the page remembers another choice.
	 */
	if (!init_graphics_modes()) quit("Could not read the tile list");
	mode = get_graphics_mode((uint8_t)graf);
	if (!tileset_usable(mode)) mode = get_graphics_mode(7);
	if (!tileset_usable(mode)) quit("Could not find the Neon tiles");
	tile_width = tw;
	tile_height = th;
	use_tileset(mode);
	send_tilesets();

	event_add_handler(EVENT_STATS, notice_status, NULL);
	event_add_handler(EVENT_HP, notice_status, NULL);
	event_add_handler(EVENT_MANA, notice_status, NULL);
	event_add_handler(EVENT_AC, notice_status, NULL);
	event_add_handler(EVENT_EXPERIENCE, notice_status, NULL);
	event_add_handler(EVENT_PLAYERLEVEL, notice_status, NULL);
	event_add_handler(EVENT_PLAYERTITLE, notice_status, NULL);
	event_add_handler(EVENT_GOLD, notice_status, NULL);
	event_add_handler(EVENT_DUNGEONLEVEL, notice_status, NULL);
	event_add_handler(EVENT_PLAYERSPEED, notice_status, NULL);
	event_add_handler(EVENT_RACE_CLASS, notice_status, NULL);
	event_add_handler(EVENT_STATUS, notice_status, NULL);
	event_add_handler(EVENT_PLAYERMOVED, notice_status, NULL);
	event_add_handler(EVENT_INVENTORY, notice_gear, NULL);
	event_add_handler(EVENT_EQUIPMENT, notice_gear, NULL);
	event_add_handler(EVENT_ENTER_WORLD, notice_gear, NULL);
	event_add_handler(EVENT_MESSAGE, notice_message, NULL);
	event_add_handler(EVENT_BIRTHPOINTS, notice_birth_points, NULL);

	/* Character creation is a page of its own from the start. */
	birth_view_hook = web_birth_view;

	quit_aux = web_quit;
	send_palette();

	return 0;
}

#endif /* USE_WEB */
