/**
 * \file main-test.c
 * \brief Pseudo-UI for end-to-end testing.
 *
 * Copyright (c) 2011 Elly <elly+angband@leptoquark.net>
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
#include "buildid.h"
#include "cave.h"
#include "dun-type.h"
#include "generate.h"
#include "wild.h"
#include "mon-make.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "game-world.h"
#include "main.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-util.h"
#include "savefile.h"
#include "ui-game.h"

#ifdef ALLOW_BORG
#include "borg/borg.h"
#include "borg/borg-init.h"
#include "borg/borg-flow-kill.h"
#include "borg/borg-io.h"
#include "borg/borg-flow-misc.h"
#endif

#ifdef USE_TEST

static int prompt = 0;
static int verbose = 0;
static int nextkey = 0;

#ifdef ALLOW_BORG
/**
 * Whether anything in this run has failed (BRG-05).
 *
 * The whole point of a borg run in CI is that it fails loudly. `borg_oops()`
 * merely stops, so from outside an abort looked exactly like a tidy
 * retirement, and nothing in the borg ever set an exit status. This does, and
 * `quit` carries it out of the process.
 */
static int run_failed = 0;

/** The seed this run used, so a failure can be repeated (BRG-04). */
static uint32_t run_seed = 0;

/* How many times the character died during the run (BRG-22). */
static int run_deaths = 0;

/**
 * Turns requested before the game was ready to play them (BRG-03).
 *
 * The front end reads its commands from the terminal's event hook, which the
 * game calls whenever it wants input -- and it wants input *before* it starts:
 * the splash screen asks for a keypress, and how many requests come before the
 * game loop begins is not fixed. One `key enter` was not enough and three
 * were, which is precisely the unreproducibility that made a scripted borg run
 * impossible.
 *
 * So `borg-run` does not require the game to be ready. It records what was
 * asked for, feeds a keypress to move whatever prompt is up along, and the
 * event hook starts the run on the first request after `character_dungeon`
 * becomes true. Nothing counts keys.
 */
static int run_pending = 0;

/**
 * Set while the borg is being started up (BRG-03).
 *
 * `borg_init()` asks for input itself, and the front end answered those
 * requests out of the *command script* -- so a run's `borg-status?` and `quit`
 * were consumed from inside `borg_init()`, and the process exited before the
 * run had begun. The script belongs to the harness, not to the borg's
 * prompts, so command reading stops for the duration and prompts are
 * dismissed with ESCAPE.
 *
 * Once the borg is properly active it steals `inkey_hack`, which is consulted
 * before the terminal is polled, so this hook stops being asked for keys and
 * the script is safe again -- but only from that moment.
 */
static int borg_starting = 0;

/*
 * Cheats and a jump requested before the game existed (BRG-22).
 *
 * The front end reads its script before `start_game()` runs, so a command that
 * needs a live character cannot act when it is read -- the same reason
 * `borg-run` defers. These are applied from the event hook on the first
 * request for input after `character_dungeon` becomes true, in order: cheat,
 * then jump, then run.
 *
 * Guarded while applying, because `prepare_next_level()` asks for input and
 * the front end would otherwise answer it out of the *script* -- which put a
 * `borg-status?` in the middle of a level generation on the first attempt.
 */
static int pending_cheat_lev  = 0;
static int pending_cheat_gold = 0;
static int pending_jump       = 0;
static int applying_setup     = 0;
static int want_deathless     = 0;

static void borg_begin_pending(void);
#endif

static void c_key(char *rest) {
	if (streq(rest, "left")) {
		nextkey = ARROW_LEFT;
	} else if (streq(rest, "right")) {
		nextkey = ARROW_RIGHT;
	} else if (streq(rest, "up")) {
		nextkey = ARROW_UP;
	} else if (streq(rest, "down")) {
		nextkey = ARROW_DOWN;
	} else if (streq(rest, "space")) {
		nextkey = ' ';
	} else if (streq(rest, "enter")) {
		nextkey = '\n';
	} else if (rest[0] == 'C' && rest[1] == '-') {
		nextkey = KTRL(rest[2]);
	} else {
		nextkey = rest[0];
	}
}

static void c_noop(char *rest) {

}

static void c_quit(char *rest) {
#ifdef ALLOW_BORG
	/*
	 * A failed borg run leaves the process with a non-zero status (BRG-05).
	 *
	 * This is the whole value of B0: a crash or an abort has to be visible to
	 * whatever ran the binary, not merely present in a log. A segfault gives
	 * 139 by itself; an abort or a bad round trip would otherwise exit 0 and
	 * look like success.
	 */
	if (run_failed) {
		printf("borg: run FAILED, ZTK_TEST_SEED=%u\n", run_seed);
		fflush(stdout);
		exit(1);
	}
#endif
	quit(NULL);
}

static void c_verbose(char *rest) {
	if (rest && streq(rest, "0")) {
		printf("cmd-verbose: off\n");
		verbose = 0;
	} else {
		printf("cmd-verbose: on\n");
		verbose = 1;
	}
}

static void c_version(char *rest) {
	printf("cmd-version: %s\n", buildid);
}

/**
 * Player commands
 */
static void c_player_birth(char *rest) {
	const char *race = strtok(rest, " ");
	const char *class = strtok(NULL, " ");
	struct player_class *c;
	struct player_race *r;

	if (!race) race = "Human";
	if (!class) class = "Warrior";

	for (r = races; r; r = r->next)
		if (streq(race, r->name))
			break;
	if (!r) {
		printf("player-birth: bad race '%s'\n", race);
		return;
	}

	for (c = classes; c; c = c->next)
		if (streq(class, c->name))
			break;

	if (!c) {
		printf("player-birth: bad class '%s'\n", class);
		return;
	}

	player_generate(player, r, c, false);
}

/*
 * ZangbandTK: these two crashed on a character that has none yet.
 *
 * Called before `player-birth`, or at the birth prompt before a race and class
 * have been chosen, `player->class` is NULL and this dereferenced it -- the
 * front end used to test the game exiting on signal 11. Harmless in the four
 * existing frontend tests, which always birth first, and exactly the sort of
 * thing a borg harness trips over while working out what state it is in.
 */
static void c_player_class(char *rest) {
	printf("player-class: %s\n",
		   (player && player->class) ? player->class->name : "(none)");
}

static void c_player_race(char *rest) {
	printf("player-race: %s\n",
		   (player && player->race) ? player->race->name : "(none)");
}

#ifdef ALLOW_BORG

/**
 * borg-seed [N] -- seed the RNG for a reproducible run (BRG-04).
 *
 * Without an argument it reads `ZTK_TEST_SEED`, which is the variable the unit
 * suites and `scripts/check-flakes` already use, so a borg failure is repeated
 * the same way a suite failure is. Without either it takes a value from the
 * clock and *prints it*, which is the part that matters: an unrepeatable
 * failure is a rumour.
 */
static void c_borg_seed(char *rest)
{
	const char *env = getenv("ZTK_TEST_SEED");

	if (rest && *rest) {
		run_seed = (uint32_t) strtoul(rest, NULL, 10);
	} else if (env && *env) {
		run_seed = (uint32_t) strtoul(env, NULL, 10);
	} else {
		run_seed = (uint32_t) time(NULL);
	}

	Rand_init();
	Rand_quick = false;
	Rand_state_init(run_seed);

	printf("borg-seed: ZTK_TEST_SEED=%u\n", run_seed);
	fflush(stdout);
}

/**
 * borg-run N -- play for N game turns and hand control back (BRG-03).
 *
 * The borg's only entry point is `^z` then `z` through the UI, and its only
 * exit is a keypress. This is the headless equivalent of both. It refuses to
 * start before `character_dungeon`, which is the *"reincarnation failure"*
 * abort that made key injection unreproducible.
 *
 * It returns immediately. The borg plays by stealing `inkey_hack`, so the play
 * happens inside the game's own input loop after this returns, and stops when
 * `borg_turn_limit` is reached -- at which point the hook is removed and this
 * frontend's command reader gets input back.
 */
static void c_borg_run(char *rest)
{
	int turns = (rest && *rest) ? atoi(rest) : 1000;

	if (turns < 1) turns = 1;
	run_pending = turns;

	/*
	 * If the game is not playing yet, move the prompt along and come back.
	 * `nextkey` is the front end's own way of supplying a keypress, and the
	 * event hook below tries again on every subsequent request.
	 */
	if (!character_dungeon) {
		printf("borg-run: waiting for the game to start\n");
		fflush(stdout);
		nextkey = '\r';
		return;
	}

	borg_begin_pending();
}

/**
 * Actually start the run, once there is a game to run in (BRG-03).
 */
static void borg_begin_pending(void)
{
	int turns = run_pending;

	run_pending = 0;

	borg_abort_reason = NULL;
	borg_starting     = 1;
	if (!borg_initialized) borg_init();
	borg_starting     = 0;

	if (borg_init_failure) {
		printf("borg-run: FAILED borg_init ZTK_TEST_SEED=%u\n", run_seed);
		run_failed = 1;
		return;
	}

	/*
	 * Start it the way the menu does (BRG-03).
	 *
	 * `borg_cmd_start()` is what `^z z` reaches, and calling it rather than
	 * setting `borg_active` and installing the hook by hand is the difference
	 * between a borg that runs and one that only looks started: the ritual
	 * also calls `borg_reinit_options()`, which allocates the arrays that
	 * `borg_reset_ignore()` frees on the way out. Setting the flags directly
	 * skipped the allocation and the first deactivation dereferenced NULL.
	 */
	borg_turn_limit = turn + turns;

	/*
	 * A decision budget as well, so the run cannot hang (BRG-05).
	 *
	 * Generous against the turn budget -- the borg spends many decisions per
	 * game turn, resting, walking and reading the screen -- but finite, which
	 * is the point. `ZTK_BORG_STEPS` overrides it for a run that legitimately
	 * needs more.
	 */
	{
		const char *env = getenv("ZTK_BORG_STEPS");

		borg_step_count = 0;
		borg_step_limit = (env && *env) ? strtol(env, NULL, 10)
			: (int32_t) turns * 200 + 2000;
	}

	borg_headless = true;

	if (want_deathless && borg_cfg) {
		borg_cfg[BORG_CHEAT_DEATH] = 1;
		option_set("cheat_live", true);
		borg_note("# ZangbandTK: death is cheated; deaths are still counted");
	}

	borg_cmd_start();

	printf("borg-run: started for %d turns at turn %d\n", turns, (int) turn);
	fflush(stdout);
}

/**
 * borg-status? -- one machine-readable line about the run (BRG-05).
 *
 * Turns, depth, character level and why it stopped, on one line, because a
 * regression signal nobody can grep is a log nobody reads. The depth and level
 * are BRG-18's signal in miniature: a build where every class stops getting
 * past depth 3 has broken something no assertion catches.
 */
static void c_borg_status(char *rest)
{
	const char *why = borg_abort_reason ? borg_abort_reason
		: (borg_active ? "still running" : "budget spent");
	char weapon_desc[80] = "(none)";

	if (player && player->body.slots) {
		struct object *w = slot_object(player, slot_by_name(player, "weapon"));

		if (w) {
			object_desc(weapon_desc, sizeof(weapon_desc), w,
						ODESC_BASE, player);
		}
	}

	/*
	 * A dead character is not a failure (BRG-05, BRG-17).
	 *
	 * The borg calls `borg_oops("death")` when the character dies, which is
	 * the same channel it uses for defects -- and dying is *ordinary play*.
	 * A level-one Warrior sent into a dungeon dies often, so treating it as a
	 * failure would leave B4's nightly job permanently red and teach everyone
	 * to ignore it. What CI should fail on is a crash, an abort the borg did
	 * not intend, or a wedge.
	 *
	 * Recorded distinctly rather than hidden: a build where every class dies
	 * at depth 1 is worth seeing, and that is what `maxdepth` and `clevel` on
	 * the summary line are for.
	 */
	if (borg_abort_reason && !streq(borg_abort_reason, "death")) {
		run_failed = 1;
	}

	/*
	 * `maxdepth` and `dungeon` are BRG-18's regression signal (and B2's
	 * measurement). Current depth alone says nothing about a run: a borg that
	 * reached depth 12 and walked back up to sell reports 0, and a build where
	 * every class suddenly stops getting past depth 3 is invisible without
	 * the high-water mark.
	 */
	/*
	 * The wielded weapon and the armour total, because "is it upgrading?" is
	 * a different question from "how deep did it get?" and the answer changes
	 * what to fix. A character at depth 6 still holding the dagger it was born
	 * with has not been picking things up, which would depress every class
	 * equally and is not a fighting problem at all.
	 */
	/*
	 * Count the deaths from the message log rather than from a hook, because
	 * with `cheat_live` on the character does not stay dead and there is no
	 * single place that notices. "Reached depth 30, died fourteen times" is
	 * the useful result; the depth alone is not.
	 */
	{
		int k, n = (int) messages_num();

		run_deaths = 0;
		for (k = 0; k < n; k++) {
			const char *m = message_str((int16_t) k);

			if (m && prefix(m, "You die")) run_deaths++;
		}
	}

	printf("borg-status: turn=%d depth=%d maxdepth=%d dungeon=%s clevel=%d "
		   "hp=%d/%d ac=%d gold=%d deaths=%d weapon=%s "
		   "grid=%d,%d level=%dx%d ready=%d seed=%u result=%s reason=%s\n",
		   (int) turn, player ? player->depth : -1,
		   player ? player->max_depth : -1,
		   (player && player->dungeon
			&& dun_type_by_index(player->dungeon - 1))
			   ? dun_type_by_index(player->dungeon - 1)->name : "-",
		   player ? player->lev : -1,
		   player ? player->chp : -1, player ? player->mhp : -1,
		   player ? player->state.ac + player->state.to_a : -1,
		   player ? (int) player->au : -1,
		   run_deaths,
		   weapon_desc,
		   player ? player->grid.y : -1, player ? player->grid.x : -1,
		   cave ? cave->height : -1, cave ? cave->width : -1,
		   character_dungeon ? 1 : 0, run_seed,
		   run_failed ? "FAILED"
			: ((borg_abort_reason && streq(borg_abort_reason, "death"))
			   ? "died" : "ok"),
		   why);
	fflush(stdout);
}

/**
 * borg-notes? [N] -- the last N things the borg said (BRG-05).
 *
 * `borg_note()` puts its reasoning into the game's message log and, if the
 * right setting is on, into a file nobody reads. A run that fails in CI is
 * useless without the last few notes: they say whether the borg was fighting,
 * lost, or waiting for a prompt that never came. Twenty lines beside the
 * summary turns "it crashed" into a diagnosis.
 */
static void c_borg_notes(char *rest)
{
	int want = (rest && *rest) ? atoi(rest) : 20;
	int have = (int) messages_num();
	int i;

	if (want > have) want = have;

	for (i = want - 1; i >= 0; i--) {
		printf("borg-note: %s\n", message_str((int16_t) i));
	}
	fflush(stdout);
}

/**
 * borg-pet <race> -- put a pet beside the player (BRG-15).
 *
 * The borg has never had a concept of allegiance, so a pet was simply a
 * monster to it: something to target, to be frightened of, and to walk across
 * a level to kill. That is behaviour which goes visibly wrong the moment
 * anybody runs the borg against a game with pets in it, and it needs a way to
 * be tested rather than reasoned about.
 */
static void c_borg_pet(char *rest)
{
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race;
	int i;

	if (!character_dungeon || !cave) {
		printf("borg-pet: FAILED no level yet\n");
		run_failed = 1;
		return;
	}

	race = lookup_monster((rest && *rest) ? rest : "soldier");
	if (!race) {
		printf("borg-pet: FAILED no such race <%s>\n", rest ? rest : "");
		run_failed = 1;
		return;
	}

	for (i = 0; i < 8; i++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;

		monster_set_allegiance(square_monster(cave, grid),
							   MON_ALLEGIANCE_PET);
		printf("borg-pet: %s at %d,%d\n", race->name, grid.y, grid.x);
		fflush(stdout);
		return;
	}

	printf("borg-pet: FAILED nowhere to put it\n");
	run_failed = 1;
}

/**
 * borg-pets? -- how many pets are still alive and still ours (BRG-15).
 *
 * Counts both, because the two failure modes read differently: a pet the borg
 * killed is gone, and a pet the borg merely *hit* has turned hostile and is
 * still on the level (PLR-33).
 */
static void c_borg_pets(char *rest)
{
	int i, pets = 0, hostile = 0;

	if (cave) {
		for (i = 1; i < cave_monster_max(cave); i++) {
			struct monster *mon = cave_monster(cave, i);

			if (!mon->race) continue;
			if (monster_is_pet(mon)) pets++;
			else if (monster_is_hostile(mon)) hostile++;
		}
	}

	printf("borg-pets: pets=%d hostiles=%d\n", pets, hostile);
	fflush(stdout);
}

/**
 * borg-kills? -- the borg's target list, and whether any ally is on it.
 *
 * The direct measurement for BRG-15, and it had to be direct: asking whether
 * pets *survive* a run measures the wrong thing, because the borg changes
 * level constantly and a pet that was left behind looks exactly like one that
 * was killed.
 *
 * `allies` must be zero. `borg_kills[]` is not just a target list --
 * `borg_danger()` is computed over it and `borg_flow_kill()` walks toward its
 * entries -- so an ally on it is a pet the borg is afraid of, chasing, and
 * swinging at.
 */
static void c_borg_kills(char *rest)
{
	int i, tracked = 0, allies = 0;

	for (i = 1; i < borg_kills_nxt; i++) {
		const borg_kill *kill = &borg_kills[i];
		struct monster *mon;

		if (!kill->r_idx) continue;
		tracked++;

		if (!cave) continue;
		mon = square_monster(cave, loc(kill->pos.x, kill->pos.y));
		if (mon && mon->race && !monster_is_hostile(mon)) {
			printf("borg-kill-ally: %s at %d,%d\n", mon->race->name,
				   kill->pos.y, kill->pos.x);
			allies++;
		}
	}

	printf("borg-kills: tracked=%d allies=%d\n", tracked, allies);
	fflush(stdout);
}

/**
 * borg-mouths? -- every dungeon mouth, its band, and whether it is reachable
 * without crossing the world (BRG-13).
 *
 * The question this answers decides how much work depth 30 is. The town
 * staircase leads into the Vaults of Amber, which ends at 15, so reaching 30
 * means walking to the mouth of a dungeon whose band goes deeper. The surface
 * is a 144x144 window onto a much larger world, rebuilt as the player crosses
 * it -- so a mouth inside the current window is a short walk to a known grid,
 * and one outside it needs an overworld navigator.
 *
 * Reports both, per mouth, so the answer is a table rather than an opinion.
 */
static void c_borg_mouths(char *rest)
{
	int i, n, inside = 0, deeper_inside = 0;
	struct loc off;

	if (!character_dungeon || !wild || !cave) {
		printf("borg-mouths: FAILED no world yet\n");
		run_failed = 1;
		return;
	}

	off = player->wild_offset;
	n   = wild_dungeon_count(wild);

	printf("borg-mouths: player at world %d,%d  window %dx%d at offset %d,%d\n",
		   player->grid.y + off.y, player->grid.x + off.x,
		   cave->height, cave->width, off.y, off.x);

	for (i = 0; i < n; i++) {
		struct wild_dungeon *m = wild_dungeon_by_index(wild, i);
		const struct dun_type *t;
		int ly, lx;
		bool in;

		if (!m) continue;
		t = dun_type_by_index(m->type);

		/* Where it would sit in the current surface chunk */
		ly = m->grid.y - off.y;
		lx = m->grid.x - off.x;
		in = (ly >= 0 && ly < cave->height && lx >= 0 && lx < cave->width);

		if (in) inside++;
		if (in && t && t->max_depth > 15) deeper_inside++;

		printf("borg-mouth: %-28s band %2d-%-3d world %4d,%-4d "
			   "local %5d,%-5d %-9s road:%s\n",
			   t ? t->name : "(unknown)", t ? t->min_depth : -1,
			   t ? t->max_depth : -1, m->grid.y, m->grid.x, ly, lx,
			   in ? "IN WINDOW" : "outside",
			   wild_road_at(wild, m->grid.x / z_info->wild_block_size,
							m->grid.y / z_info->wild_block_size)
				   ? "yes" : "NO");
	}

	printf("borg-mouths: %d of %d in the window, %d of those reach past "
		   "depth 15\n", inside, n, deeper_inside);
	fflush(stdout);
}

/**
 * borg-cheat <clevel> <gold> -- the scoped route's four levers (BRG-22).
 *
 * The project owner's direction: *"The borg reaching depth 30 unaided using a
 * single character class... This is why we added the cheats, in order to be
 * able to test better by giving us higher levels, more experience, more HP,
 * and yes, cheating death."*
 *
 * The point is that the early-game grind is **not what is being verified**.
 * Three attempts at making the borg survive it made it worse, and none of the
 * milestones the run exists to exercise -- M8's mutations, M9's realms, M10's
 * pets -- live at character level 6 anyway.
 *
 * **The cheats remove attrition, not decisions.** The borg still chooses what
 * to buy, what to wield, which spell to cast, what to fight, when to flee and
 * when to descend, and every level is still generated and played. Two lines
 * are held deliberately:
 *
 *   - Grants happen **here, at the start**, never reactively. A borg healed
 *     whenever it is about to die tells us nothing about combat.
 *   - Gold rather than granted equipment, so the borg *shops* -- which
 *     exercises M5's stores and keeps a real decision in the loop.
 */
static void c_borg_cheat(char *rest)
{
	int want_lev = 30, want_gold = 200000;
	char *arg;

	if (rest && *rest) {
		arg = strtok(rest, " ");
		if (arg) want_lev = atoi(arg);
		arg = strtok(NULL, " ");
		if (arg) want_gold = atoi(arg);
	}

	/* Deferred: the character does not exist when the script is read */
	if (!character_dungeon) {
		pending_cheat_lev  = want_lev;
		pending_cheat_gold = want_gold;
		printf("borg-cheat: queued clevel=%d gold=%d\n", want_lev, want_gold);
		fflush(stdout);
		return;
	}

	/*
	 * Experience to the requested character level. `clevel >= depth` is the
	 * borg's own gate on descending, so without this no amount of anything
	 * else reaches depth 30.
	 */
	while (player->lev < want_lev && player->exp < PY_MAX_EXP) {
		int32_t need = (int32_t) player_exp[player->lev - 1]
			* player->expfact / 100L;

		player_exp_gain(player, MAX(need - player->exp + 1, 1));
		if (player->lev >= want_lev) break;
	}

	/*
	 * Heal after granting levels, or the grant is a trap.
	 *
	 * `player_exp_gain()` raises *maximum* hit points and leaves the current
	 * total where it was, so a character granted level 30 wakes with the
	 * fourteen hit points it had at level 2 and a maximum of 244. Dropped at
	 * depth 25 it read that correctly as mortal danger and fled to depth 1 --
	 * "Leaving (low hit-points)" -- which looked like the borg refusing to
	 * descend and was the harness handing it an invalid character.
	 */
	player->chp = player->mhp;
	player->csp = player->msp;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);

	player->au = want_gold;

	/*
	 * Cheat death (BRG-22). The borg already has the setting and the plumbing
	 * -- `BORG_CHEAT_DEATH` turns on the game's `cheat_live` -- it is simply
	 * off by default and `borg.txt` is not installed, so the default stands.
	 *
	 * Deaths stay a **reported metric** rather than being hidden by this:
	 * "reached depth 30, died fourteen times" says far more than "reached
	 * depth 30", and it keeps the signal that dying is what the run is
	 * ultimately measuring.
	 */
	want_deathless = 1;

	/* Mutation and racial powers never fail; cheap, and it exercises M8. */
	option_set(option_name(OPT_cheat_powers), true);

	printf("borg-cheat: clevel=%d exp=%d gold=%d hp=%d/%d\n",
		   player->lev, (int) player->exp, (int) player->au,
		   player->chp, player->mhp);
	fflush(stdout);
}

/**
 * borg-jump <depth> -- put the borg at a depth, using the map we already have.
 *
 * Teleport to a *dungeon*, not to the target depth. Arriving at 30 directly
 * generates one level and proves nothing; arriving at a dungeon whose band
 * contains 30 and descending by play is the thing worth measuring.
 */
static void c_borg_jump(char *rest)
{
	int depth = (rest && *rest) ? atoi(rest) : 25;

	/* Deferred, as above */
	if (!character_dungeon) {
		pending_jump = depth;
		printf("borg-jump: queued depth=%d\n", depth);
		fflush(stdout);
		return;
	}

	/* Pick the shallowest dungeon whose band contains the depth wanted */
	{
		const struct dun_type *d, *best = NULL;

		for (d = dun_types; d; d = d->next) {
			if (depth < d->min_depth || depth > d->max_depth) continue;
			if (!best || d->min_depth < best->min_depth) best = d;
		}

		if (best) {
			int i;

			for (i = 0; i < dun_type_count(); i++) {
				if (dun_type_by_index(i) == best) {
					player->dungeon = i + 1;
					break;
				}
			}
			printf("borg-jump: %s (band %d-%d)\n", best->name,
				   best->min_depth, best->max_depth);
		}
	}

	player->depth = depth;
	if (player->max_depth < depth) player->max_depth = depth;
	prepare_next_level(player);
	on_new_level();

	printf("borg-jump: at depth %d\n", player->depth);
	fflush(stdout);
}

/**
 * borg-terrain? -- what lies on the straight line to each deeper mouth
 * (BRG-13).
 *
 * The question that decides whether crossing the world is a bearing or a
 * route. A bearing plus local obstacle avoidance is a small piece of work; if
 * open sea or mountain sits across the line, it becomes a pathfinder, and that
 * is worth knowing before building either.
 *
 * Sampled at block resolution -- the wilderness is generated per block of
 * `wild:block-size` grids, so that is the granularity at which terrain
 * actually varies.
 */
static void c_borg_terrain(char *rest)
{
	int i, n, size;

	if (!character_dungeon || !wild) {
		printf("borg-terrain: FAILED no world yet\n");
		run_failed = 1;
		return;
	}

	size = z_info->wild_block_size;
	n    = wild_dungeon_count(wild);

	for (i = 0; i < n; i++) {
		struct wild_dungeon *m = wild_dungeon_by_index(wild, i);
		const struct dun_type *t;
		int fy, fx, ty, tx, steps, k;
		int blocked = 0, water = 0, samples = 0;

		if (!m) continue;
		t = dun_type_by_index(m->type);
		if (!t || t->max_depth <= 15) continue;

		fy = (player->grid.y + player->wild_offset.y) / size;
		fx = (player->grid.x + player->wild_offset.x) / size;
		ty = m->grid.y / size;
		tx = m->grid.x / size;

		steps = MAX(ABS(ty - fy), ABS(tx - fx));
		if (steps < 1) steps = 1;

		for (k = 0; k <= steps; k++) {
			int by = fy + (ty - fy) * k / steps;
			int bx = fx + (tx - fx) * k / steps;
			int feat = wild_block_feat(wild, bx, by);

			samples++;

			/*
			 * A town block and a dungeon block are markers rather than
			 * terrain -- `wild_block_feat()` returns FEAT_PERM for one and
			 * FEAT_DUNGEON for the other, and a character walks through both.
			 * Counting them as walls made every line look obstructed.
			 */
			if (feat == FEAT_PERM || feat == FEAT_DUNGEON) continue;

			if (feat == FEAT_WATER) {
				water++;
			} else if (!feat_is_passable(feat)) {
				blocked++;
				if (blocked == 1) {
					printf("borg-terrain-block: %s at block %d,%d on the line "
						   "to %s\n", f_info[feat].name, by, bx, t->name);
				}
			}
		}

		printf("borg-terrain: %-28s band %2d-%-3d  %3d blocks  "
			   "water %2d  impassable %2d  %s\n",
			   t->name, t->min_depth, t->max_depth, samples, water, blocked,
			   (blocked == 0) ? "CLEAR LINE" : "obstructed");
	}

	fflush(stdout);
}

/**
 * borg-shops? -- which shops the borg has found (BRG-12).
 *
 * The borg learns the map a panel at a time, so on a 144x144 wilderness
 * surface it only knows about the shops it has walked past. Whether it knows
 * about any at all is the difference between a borg that restocks and one that
 * stair-scums forever saying it needs food.
 */
static void c_borg_shops(char *rest)
{
	int i, known = 0;

	for (i = 0; i < (int) z_info->store_max; i++) {
		if (track_shop_x[i] > 0 || track_shop_y[i] > 0) {
			printf("borg-shop: %d at %d,%d\n", i, track_shop_y[i],
				   track_shop_x[i]);
			known++;
		}
	}

	printf("borg-shops: %d of %d known\n", known, (int) z_info->store_max);
	fflush(stdout);
}

/**
 * borg-roundtrip -- save, reload and compare (BRG-19, pulled into B0).
 *
 * BRG-19 schedules mid-run invariants for the last phase. This one comes
 * forward because it is the cheapest of them and the likeliest to catch
 * something: the borg walks a character through wilderness, dungeons, stores
 * and level changes for thousands of turns, which is far more savefile states
 * than any fixture covers, and a corrupt save is exactly the failure that is
 * invisible until much later.
 *
 * Compares depth, level, experience and turn across the round trip. Not a deep
 * comparison -- `game/roundtrip` does that against fixtures -- but enough that
 * a save which drops the character's progress cannot pass.
 */
static void c_borg_roundtrip(char *rest)
{
	char name[128];
	int was_depth, was_lev, was_turn;
	int32_t was_exp;

	if (!character_dungeon) {
		printf("borg-roundtrip: FAILED no character\n");
		run_failed = 1;
		return;
	}

	was_depth = player->depth;
	was_lev   = player->lev;
	was_exp   = player->exp;
	was_turn  = (int) turn;

	strnfmt(name, sizeof(name), "borg-roundtrip-%d", (int) getpid());

	if (!savefile_save(name)) {
		printf("borg-roundtrip: FAILED save ZTK_TEST_SEED=%u\n", run_seed);
		run_failed = 1;
		return;
	}

	if (!savefile_load(name, false)) {
		printf("borg-roundtrip: FAILED load ZTK_TEST_SEED=%u\n", run_seed);
		run_failed = 1;
		file_delete(name);
		return;
	}
	file_delete(name);

	if (player->depth != was_depth || player->lev != was_lev
			|| player->exp != was_exp || (int) turn != was_turn) {
		printf("borg-roundtrip: FAILED mismatch "
			   "depth %d->%d lev %d->%d exp %d->%d turn %d->%d "
			   "ZTK_TEST_SEED=%u\n",
			   was_depth, player->depth, was_lev, player->lev,
			   (int) was_exp, (int) player->exp, was_turn, (int) turn,
			   run_seed);
		run_failed = 1;
		return;
	}

	printf("borg-roundtrip: ok depth=%d lev=%d turn=%d\n",
		   player->depth, player->lev, (int) turn);
	fflush(stdout);
}

#endif /* ALLOW_BORG */

typedef struct {
	const char *name;
	void (*func)(char *args);
} test_cmd;

static test_cmd cmds[] = {
	{ "#", c_noop },
	{ "key", c_key },
	{ "noop", c_noop },
	{ "quit", c_quit },
	{ "verbose", c_verbose },
	{ "version?", c_version },

	{ "player-birth", c_player_birth },
	{ "player-class?", c_player_class },
	{ "player-race?", c_player_race },

#ifdef ALLOW_BORG
	{ "borg-seed", c_borg_seed },
	{ "borg-run", c_borg_run },
	{ "borg-status?", c_borg_status },
	{ "borg-notes?", c_borg_notes },
	{ "borg-shops?", c_borg_shops },
	{ "borg-mouths?", c_borg_mouths },
	{ "borg-terrain?", c_borg_terrain },
	{ "borg-cheat", c_borg_cheat },
	{ "borg-jump", c_borg_jump },
	{ "borg-pet", c_borg_pet },
	{ "borg-pets?", c_borg_pets },
	{ "borg-kills?", c_borg_kills },
	{ "borg-roundtrip", c_borg_roundtrip },
#endif

	{ NULL, NULL }
};

static errr test_docmd(void) {
	char buf[1024];
	char *cmd;
	char *rest;
	int i;

	memset(buf, 0, sizeof(buf));

	if (prompt) {
		printf("test> ");
		fflush(stdout);
	}
	if (!fgets(buf, sizeof(buf), stdin)) {
		return -1;
	}
	if (strchr(buf, '\n')) {
		*strchr(buf, '\n') = '\0';
	}

	if (verbose) printf("test-docmd: %s\n", buf);
	cmd = strtok(buf, " ");
	if (!cmd) return 0;
	rest = strtok(NULL, "");

	for (i = 0; cmds[i].name; i++) {
		if (streq(cmds[i].name, cmd)) {
			cmds[i].func(rest);
			return 0;
		}
	}

	return 0;
}

typedef struct term_data term_data;
struct term_data {
	term t;
};

static term_data td;
typedef struct {
	int key;
	errr (*func)(int v);
} term_xtra_func;

static void term_init_test(term *t) {
	if (verbose) printf("term-init %s\n", buildid);
}

static void term_nuke_test(term *t) {
	if (verbose) printf("term-end\n");
}

static errr term_xtra_clear(int v) {
	if (verbose) printf("term-xtra-clear %d\n", v);
	return 0;
}

static errr term_xtra_noise(int v) {
	if (verbose) printf("term-xtra-noise %d\n", v);
	return 0;
}

static errr term_xtra_fresh(int v) {
	if (verbose) printf("term-xtra-fresh %d\n", v);
	return 0;
}

static errr term_xtra_shape(int v) {
	if (verbose) printf("term-xtra-shape %d\n", v);
	return 0;
}

static errr term_xtra_alive(int v) {
	if (verbose) printf("term-xtra-alive %d\n", v);
	return 0;
}

static errr term_xtra_event(int v) {
	if (verbose) printf("term-xtra-event %d\n", v);
	if (nextkey) {
		Term_keypress(nextkey, 0);
		nextkey = 0;
	}

#ifdef ALLOW_BORG
	/*
	 * A run asked for before the game was ready starts here (BRG-03).
	 *
	 * This hook runs on every request for input, so it is the first place that
	 * can see `character_dungeon` become true. Starting the borg here rather
	 * than counting keypresses in the input script is what makes a run
	 * reproducible from a seed alone.
	 */
	/*
	 * While the borg is starting or driving, this hook does nothing at all.
	 *
	 * Not the script -- those lines belong to the harness, and answering
	 * `borg_init()`'s prompts out of them exited the process from inside
	 * startup. And not a keypress either, which was the next mistake: the
	 * borg's only stop signal is *real user input*, so an injected ESCAPE
	 * read as somebody reaching for the keyboard and the run aborted at turn
	 * one with "user abort". The borg feeds itself through `inkey_hack`; this
	 * hook is only being asked whether anything has arrived, and the honest
	 * answer is no.
	 */
	if (borg_starting || borg_active) {
		/*
		 * Keypress 10 specifically, and it is upstream's own exemption:
		 * `internal_borg_inkey()`'s abort check tests
		 * `ch_evt.key.code != 10`, so a line feed is the one key that
		 * satisfies the terminal's need for an event without reading as
		 * somebody reaching for the keyboard. ESCAPE aborted the run at turn
		 * one; injecting nothing at all hung it, because the terminal poll
		 * waits for an event and the borg's own hook sits above that poll.
		 */
		nextkey = 10;
		return 0;
	}

	/*
	 * Setup first: cheats, then the jump, then the run (BRG-22). Guarded so
	 * that input requested during level generation is not answered out of the
	 * harness's own script.
	 */
	if (!applying_setup && character_dungeon
			&& (pending_cheat_lev || pending_jump)) {
		applying_setup = 1;

		if (pending_cheat_lev) {
			char buf[64];

			strnfmt(buf, sizeof(buf), "%d %d", pending_cheat_lev,
					pending_cheat_gold);
			pending_cheat_lev = 0;
			c_borg_cheat(buf);
		}

		if (pending_jump) {
			char buf[32];

			strnfmt(buf, sizeof(buf), "%d", pending_jump);
			pending_jump = 0;
			c_borg_jump(buf);
		}

		applying_setup = 0;
		return 0;
	}

	/*
	 * While the setup is running, answer its prompts with a keypress rather
	 * than with nothing. Returning no event leaves the terminal's poll
	 * waiting for ever -- level generation asks for input, and the harness
	 * must not answer it out of the script.
	 */
	if (applying_setup) {
		nextkey = '\r';
		return 0;
	}

	if (run_pending && character_dungeon) {
		borg_begin_pending();
		return 0;
	}
	if (run_pending) {
		nextkey = '\r';
		return 0;
	}
#endif

	return test_docmd();
}

static errr term_xtra_flush(int v) {
	if (verbose) printf("term-xtra-flush %d\n", v);
	return 0;
}

static errr term_xtra_delay(int v) {
	if (verbose) printf("term-xtra-delay %d\n", v);
	return 0;
}

static errr term_xtra_react(int v) {
	if (verbose) printf("term-xtra-react\n");
	return 0;
}

static term_xtra_func xtras[] = {
	{ TERM_XTRA_CLEAR, term_xtra_clear },
	{ TERM_XTRA_NOISE, term_xtra_noise },
	{ TERM_XTRA_FRESH, term_xtra_fresh },
	{ TERM_XTRA_SHAPE, term_xtra_shape },
	{ TERM_XTRA_ALIVE, term_xtra_alive },
	{ TERM_XTRA_EVENT, term_xtra_event },
	{ TERM_XTRA_FLUSH, term_xtra_flush },
	{ TERM_XTRA_DELAY, term_xtra_delay },
	{ TERM_XTRA_REACT, term_xtra_react },
	{ 0, NULL },
};

static errr term_xtra_test(int n, int v) {
	int i;
	for (i = 0; xtras[i].func; i++) {
		if (xtras[i].key == n) {
			return xtras[i].func(v);
		}
	}
	if (verbose) printf("term-xtra-unknown %d %d\n", n, v);
	return 0;
}

static errr term_curs_test(int x, int y) {
	if (verbose) printf("term-curs %d %d\n", x, y);
	return 0;
}

static errr term_wipe_test(int x, int y, int n) {
	if (verbose) printf("term-wipe %d %d %d\n", x, y, n);
	return 0;
}

static errr term_text_test(int x, int y, int n, int a, const wchar_t *s) {
	if (verbose) {
		char str[256];
		wcstombs(str, s, 256);
		printf("term-text %d %d %d %02x %s\n", x, y, n, a, str);
	}
	return 0;
}

static void term_data_link(int i) {
	term *t = &td.t;

	{
		/*
		 * Eighty by twenty-four unless asked otherwise.  The world map draws as
		 * much of the world as the terminal will hold, so capturing all of it in
		 * one frame wants a terminal no player would use.
		 */
		const char *w = getenv("ZTK_TERM_W"), *h = getenv("ZTK_TERM_H");

		term_init(t, w ? atoi(w) : 80, h ? atoi(h) : 24, 256);
	}

	t->init_hook = term_init_test;
	t->nuke_hook = term_nuke_test;

	t->xtra_hook = term_xtra_test;
	t->curs_hook = term_curs_test;
	t->wipe_hook = term_wipe_test;
	t->text_hook = term_text_test;

	t->data = &td;

	Term_activate(t);

	angband_term[i] = t;
}

const char help_test[] = "Test mode, subopts -p(rompt)";

errr init_test(int argc, char *argv[]) {
	int i;

	/* Skip over argv[0] */
	for (i = 1; i < argc; i++) {
		if (streq(argv[i], "-p")) {
			prompt = 1;
			continue;
		}
		printf("init-test: bad argument '%s'\n", argv[i]);
	}

	/*
	 * Reset savefile set by main.c:  don't want it to interfere with the
	 * test.
	 */
	/*
	 * Normally cleared so a stray savefile cannot interfere with a test.  Kept
	 * when ZTK_SAVEFILE is set, which is how scripts/screenshot drives a real
	 * character headlessly to capture the manual's pictures.
	 */
	if (!getenv("ZTK_SAVEFILE")) savefile[0] = '\0';

	term_data_link(0);
	return 0;
}
#endif
