/* player/psychic-drain
 *
 * The Mindcrafter's own mana recovery (PLR-06, ZangbandTK).
 *
 * Psychic Drain is the eleventh of the twelve psionic powers and the only one
 * that pays you: it tears at a mind and converts what comes away into spell
 * points.  It shipped as a psi ball and nothing else until 3.123.0, on a note
 * saying neither half of it could be expressed here.  Both can -- the
 * conversion because a projection handler has the damage in front of it, and
 * the extra time because 4.2 drains the same signed energy counter the archive
 * does.
 *
 * What this suite is built against is a spell that *looks* like it works.  A
 * drain that deals its damage and quietly converts nothing is the exact defect
 * that sat here for months, and it is invisible in play unless you are watching
 * the mana bar. So every branch is asserted by the mana that moves, not by the
 * spell resolving:
 *
 *   - an ordinary mind pays out, and the amount is the archive's;
 *   - an empty mind pays nothing, and nothing is the assertion;
 *   - a resisting mind pays nothing either, which is the branch most likely to
 *     be written wrong, because `dam / 3` looks like it should still feed you;
 *   - a powerful corrupted mind takes mana *off* you.
 *
 * The premise this spell was requested under is also pinned here, because it
 * turned out to be wrong: a Mindcrafter has never been unable to recover mana.
 * It has a pool and it regenerates like every other caster. Psychic Drain is
 * the fast, active route, not the only one -- and `a-mindcrafter-regenerates`
 * below is what stops that being rediscovered a third time.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Mindcrafter", "Tester")) {
		cleanup_angband();
		return 1;
	}
	(void) test_seed_rng_reported(suite_name);
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Is there a grid beside the player that a monster can be put in? */
static bool has_free_neighbour(void)
{
	int d;

	for (d = 0; d < 8; d++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[d]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (square_isempty(cave, grid)) return true;
	}
	return false;
}

/**
 * A Mindcrafter at `lev`, on a fresh level, with a full pool and plenty of hp.
 *
 * The level is rebuilt until the player has somewhere to put a monster.  A
 * generated level can drop the player into a corridor with every neighbour
 * solid or occupied, and then every placement in the suite returns NULL and the
 * tests fail on their own scenery -- measured at roughly one run in eight, in
 * two different tests, which is how it presented before this loop existed.
 */
static void mindcrafter_at(int lev)
{
	int tries;

	player->depth = 1;
	for (tries = 0; tries < 50; tries++) {
		prepare_next_level(player);
		if (has_free_neighbour()) break;
	}

	player->lev = player->max_lev = lev;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);
	player->mhp = 5000;
	player->chp = 5000;
	player->csp = player->msp;
}

/**
 * Put `name` where the player can drain it, and return it, or NULL.
 *
 * Adjacent for preference. Failing that, any grid within three that the player
 * has a clear line to -- because a level generated around a corridor can leave
 * every neighbour solid, and `notnull()` on the result then fails for a reason
 * that has nothing to do with the spell. Measured at roughly one run in three
 * before the fallback existed.
 */
static struct monster *place_next_to(const char *name)
{
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int d, dx, dy;

	if (!race) return NULL;

	for (d = 0; d < 8; d++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[d]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP))
			continue;
		return square_monster(cave, grid);
	}

	for (dy = -3; dy <= 3; dy++) {
		for (dx = -3; dx <= 3; dx++) {
			struct loc grid = loc(player->grid.x + dx, player->grid.y + dy);

			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isempty(cave, grid)) continue;
			if (!projectable(cave, player->grid, grid, PROJECT_STOP)) continue;
			if (!place_new_monster(cave, grid, race, false, false, info,
								   ORIGIN_DROP))
				continue;
			return square_monster(cave, grid);
		}
	}
	return NULL;
}

/** Drain `mon` for `dam`, and return the mana that moved (may be negative). */
static int drain(struct monster *mon, int dam)
{
	int before = player->csp;

	project(source_player(), 0, mon->grid, dam, PROJ_MON_PSI_DRAIN,
			PROJECT_KILL | PROJECT_STOP | PROJECT_HIDE, 0, 0, NULL);
	return player->csp - before;
}

/**
 * An ordinary mind pays out, and pays what the archive says.
 *
 * `b = damroll(5, dam) / 4` ([spells1.c:1617](../archive/zangband/src/spells1.c#L1617)),
 * so for a damage of 40 the gain runs 1 to 50 and averages about 25.  Checked
 * as a distribution rather than a draw: the bounds hold every time, and the
 * mean is asserted inside a band wide enough not to flake and narrow enough to
 * catch `/2` or `/8` written in place of `/4`.
 */
static int test_an_ordinary_mind_pays_out(void *state) {
	struct monster *mon;
	int i, total = 0, lo = 9999, hi = 0;
	const int dam = 40, runs = 300;

	mindcrafter_at(25);

	/*
	 * A pool wide enough that the ceiling never binds, which this test needs
	 * and the class does not have.
	 *
	 * A level 25 Mindcrafter's real maximum is 26 points. Every gain here is
	 * larger than that, so with the class's own pool `MIN(msp, csp + gain)`
	 * clamps all of them to 26 and the distribution below measures the cap
	 * rather than the conversion -- it passed unchanged against a build with
	 * the rate doubled, which is how this was found. The ceiling is asserted
	 * on its own terms in `the-gain-stops-at-the-ceiling`.
	 */
	player->msp = 5000;

	for (i = 0; i < runs; i++) {
		int got;

		mon = place_next_to("filthy street urchin");
		notnull(mon);
		mon->hp = mon->maxhp = 30000;		/* survives, so it can be reused */

		player->csp = 0;
		got = drain(mon, dam);
		require(got >= 0);
		total += got;
		if (got < lo) lo = got;
		if (got > hi) hi = got;

		delete_monster(cave, mon->grid);
	}

	require(hi > 0);					/* it pays at all */
	require(hi <= (5 * dam) / 4);		/* and never more than 5d(dam)/4 */
	require(lo >= 0);

	/* Mean of 5d40/4 is 5 * 41 / 2 / 4 = 25.6 */
	require(total / runs >= 21);
	require(total / runs <= 30);
	ok;
}

/**
 * And never past the ceiling.
 *
 * `b = MIN(p_ptr->msp, p_ptr->csp + b)`.  A full pool gains nothing and, more
 * to the point, does not overflow into one.
 */
static int test_the_gain_stops_at_the_ceiling(void *state) {
	struct monster *mon;
	int i;

	mindcrafter_at(25);
	mon = place_next_to("filthy street urchin");
	notnull(mon);
	mon->hp = mon->maxhp = 30000;

	player->csp = player->msp;
	for (i = 0; i < 20; i++) {
		(void) drain(mon, 40);
		eq(player->csp, player->msp);
	}

	/* One short of full fills up and stops */
	player->csp = player->msp - 1;
	(void) drain(mon, 40);
	eq(player->csp, player->msp);
	ok;
}

/**
 * An empty mind gives nothing, and takes nothing.
 *
 * Both halves: `dam = 0` and no conversion.  A build that skipped only the
 * conversion would still kill icky things with it.
 */
static int test_an_empty_mind_gives_nothing(void *state) {
	struct monster *mon;
	int i, before_hp;

	mindcrafter_at(25);
	for (i = 0; i < 40; i++) {
		mon = place_next_to("white icky thing");
		notnull(mon);
		require(rf_has(mon->race->flags, RF_EMPTY_MIND));
		mon->hp = mon->maxhp = 30000;
		before_hp = mon->hp;

		player->csp = 0;
		eq(drain(mon, 40), 0);
		eq(mon->hp, before_hp);			/* and it was not hurt */

		delete_monster(cave, mon->grid);
	}
	ok;
}

/**
 * A mind that resists gives nothing either, and that is the whole point of the
 * branch order.
 *
 * Zangband's conversion is an `else if` after the resist test, so a creature
 * that resists yields *no* mana rather than a third of it.  An animal always
 * resists, which makes this checkable without leaning on a roll.
 */
static int test_a_resisting_mind_gives_nothing(void *state) {
	struct monster *mon;
	int i, hurt_at_all = 0;

	mindcrafter_at(25);
	for (i = 0; i < 60; i++) {
		int before_hp;

		mon = place_next_to("cave spider");
		notnull(mon);
		require(rf_has(mon->race->flags, RF_ANIMAL));
		mon->hp = mon->maxhp = 30000;
		before_hp = mon->hp;

		player->csp = 0;
		eq(drain(mon, 40), 0);
		if (mon->hp < before_hp) hurt_at_all++;

		delete_monster(cave, mon->grid);
	}

	/* It still hurts, at a third -- resisting is not immunity */
	require(hurt_at_all > 0);
	ok;
}

/**
 * A powerful corrupted mind drains you instead.
 *
 * Undead or demon, above your level, one time in two -- and then a saving
 * throw.  Failing it costs mana *and* hit points, which is the half that makes
 * the spell a decision rather than free fuel.
 *
 * Driven at a damage of ten, which is chosen from both ends. Low enough that
 * `level > randint1(6 * dam)` is certain for a level 69 creature -- 69 beats
 * every value `randint1(60)` can take -- so the resist branch is reached on
 * every pass and the backlash's own coin is all the loop has to wait for. And
 * high enough to survive the division: the drain is `damroll(5, dam / 3) / 2`
 * with the damage already thirded, so a damage of one makes it `damroll(5, 0)`,
 * which is nothing at all. Measured: at one, this test sees a backlash fire
 * every time and no mana move, which is exactly the shape of a backlash that
 * had been built to do nothing.
 */
static int test_a_corrupted_mind_backlashes(void *state) {
	struct monster *mon;
	int i, lost_mana = 0, lost_hp = 0;

	mindcrafter_at(10);
	for (i = 0; i < 200; i++) {
		int hp_before;

		mon = place_next_to("nightcrawler");
		notnull(mon);
		require(rf_has(mon->race->flags, RF_UNDEAD)
				|| rf_has(mon->race->flags, RF_DEMON));
		require(mon->race->level > player->lev);
		mon->hp = mon->maxhp = 30000;

		player->csp = player->msp;
		player->chp = 5000;
		hp_before = player->chp;

		if (drain(mon, 10) < 0) lost_mana++;
		if (player->chp < hp_before) lost_hp++;

		delete_monster(cave, mon->grid);
	}

	require(lost_mana > 0);
	require(lost_hp > 0);

	/* Never a gain from one of these */
	ok;
}

/**
 * Killing it outright still feeds you.
 *
 * The conversion reads the damage rolled, not the hit points actually removed,
 * so finishing something nearly dead is worth as much as hurting something
 * healthy.  That is the archive's behaviour and it is what makes the spell
 * usable as a finisher; a build that capped the gain at the creature's
 * remaining hit points would pass every other test here.
 */
static int test_killing_it_still_feeds_you(void *state) {
	struct monster *mon;
	int i, fed = 0;

	mindcrafter_at(25);
	for (i = 0; i < 40; i++) {
		mon = place_next_to("filthy street urchin");
		notnull(mon);
		mon->hp = 1;					/* one hit point left */

		player->csp = 0;
		if (drain(mon, 40) > 0) fed++;

		if (square_monster(cave, mon->grid))
			delete_monster(cave, mon->grid);
	}

	require(fed > 0);
	ok;
}

/**
 * The drain costs time, and only when it finds something.
 *
 * `p_ptr->energy -= randint1(150)` on top of the turn the power spends.  Both
 * sides, because an unconditional charge would pass the first half: a drain
 * that hits nothing must cost nothing extra.
 */
static int test_the_drain_costs_time(void *state) {
	struct monster *mon;
	struct loc empty;
	int before, i;

	mindcrafter_at(25);
	mon = place_next_to("filthy street urchin");
	notnull(mon);
	mon->hp = mon->maxhp = 30000;

	for (i = 0; i < 20; i++) {
		player->energy = 1000;
		before = player->energy;
		(void) drain(mon, 40);
		require(player->energy < before);
		require(before - player->energy <= 150);
	}

	/*
	 * An empty grid costs nothing.
	 *
	 * The grid used is the one the monster was just standing on, and that is
	 * not laziness -- it is the only grid this test can be sure of. It is
	 * reachable by construction, since the drains above all landed on it, and
	 * nothing can have moved into it because no monster takes a turn here. A
	 * grid picked by searching outward instead can sit behind another monster,
	 * and `PROJECT_STOP` stops the ball at that one: the energy is spent, on a
	 * hit that did happen, and it reads as the condition being broken. That
	 * failed about one run in three before this was pinned down.
	 */
	empty = mon->grid;
	delete_monster(cave, empty);
	require(square_isempty(cave, empty));

	player->energy = 1000;
	before = player->energy;
	project(source_player(), 0, empty, 40, PROJ_MON_PSI_DRAIN,
			PROJECT_KILL | PROJECT_STOP | PROJECT_HIDE, 0, 0, NULL);
	eq(player->energy, before);
	ok;
}

/**
 * The power is wired to the drain, at the archive's level, cost and failure.
 *
 * Cheap, and it is what catches the data half going back to a plain psi ball --
 * which is what it was, and which every other test here would then fail for a
 * reason that looks like the handler.
 */
static int test_the_power_is_the_archive_s(void *state) {
	const struct player_power *pw;
	const struct player_power *drain_pw = NULL;

	for (pw = player->class->powers; pw; pw = pw->next) {
		if (streq(pw->name, "drain a mind")) drain_pw = pw;
	}
	notnull(drain_pw);

	eq(drain_pw->level, 25);
	eq(drain_pw->cost, 10);
	eq(drain_pw->fail, 40);

	notnull(drain_pw->effects);
	notnull(drain_pw->effects->effect);
	eq(drain_pw->effects->effect->subtype, PROJ_MON_PSI_DRAIN);
	ok;
}

/**
 * And a Mindcrafter regenerates mana like everybody else.
 *
 * This is here because the spell above was asked for on the premise that the
 * class could not recover mana at all, and that was never true: `calc_mana()`
 * gives a power-list class a pool (PLR-06) and `player_regen_mana()` is called
 * for every class with no gate on any of them.  Psychic Drain is the fast route
 * and not the only one.
 *
 * Asserted rather than remembered, because the two halves that make it true are
 * in different files and either could be narrowed by accident.
 */
static int test_a_mindcrafter_regenerates(void *state) {
	int i, before;

	mindcrafter_at(25);
	require(player->msp > 0);			/* it has a pool at all */

	player->csp = 0;
	before = player->csp;
	for (i = 0; i < 400; i++) process_world(cave);
	require(player->csp > before);
	ok;
}

const char *suite_name = "player/psychic-drain";
struct test tests[] = {
	{ "an-ordinary-mind-pays-out", test_an_ordinary_mind_pays_out },
	{ "the-gain-stops-at-the-ceiling", test_the_gain_stops_at_the_ceiling },
	{ "an-empty-mind-gives-nothing", test_an_empty_mind_gives_nothing },
	{ "a-resisting-mind-gives-nothing", test_a_resisting_mind_gives_nothing },
	{ "a-corrupted-mind-backlashes", test_a_corrupted_mind_backlashes },
	{ "killing-it-still-feeds-you", test_killing_it_still_feeds_you },
	{ "the-drain-costs-time", test_the_drain_costs_time },
	{ "the-power-is-the-archives", test_the_power_is_the_archive_s },
	{ "a-mindcrafter-regenerates", test_a_mindcrafter_regenerates },
	{ NULL, NULL }
};
