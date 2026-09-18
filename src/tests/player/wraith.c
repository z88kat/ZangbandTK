/* player/wraith — the incorporeal state, and what it costs (PLR-16)
 *
 * Wraith form is the WRAITH mutation's effect and the strongest thing chaos
 * hands out by accident: it passes walls, turns nine tenths of every blow
 * aside, adds a hundred points of armour and reflection, and is fed by the
 * dark.  It was deferred from 3.45.0 with the note that "4.2 has no wraith
 * form and no player-passes-walls state at all", which stopped being true when
 * the Spectre work (DEC-74) built `OF_PASS_WALL`.
 *
 * Two things this suite exists to catch, because neither looks like a failure:
 *
 * The first is a timed effect that grants nothing.  A `flag-synonym` line lost
 * to an edit leaves a mutation that fires, prints its message, shows on the
 * status bar and does not let you through a wall — so every test here asserts
 * the same thing twice, once with the form and once without it.
 *
 * The second is the expiry.  The form is timed and the flag is what the wall
 * damage reads, so a character whose form runs out inside rock is solid again
 * with rock on every side.  Zangband crushes them
 * ([dungeon.c:1202](../archive/zangband/src/dungeon.c#L1202)) and lifts the
 * hit-point floor to do it.  Before this was built that branch was unreachable
 * and left out; the test below is the reason it can never quietly go away
 * again, because without it the character is entombed alive instead.
 */
#include "unit-test.h"

#include "cave.h"
#include "cmd-core.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "obj-knowledge.h"
#include "obj-gear.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-mutation.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	/* Several tests here generate a level, so an odd failure can be replayed */
	(void) test_seed_rng_reported(suite_name);

	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** Put `name` on a fresh level at `depth` and recalculate. */
static bool stand_up(const char *name, int depth)
{
	if (!player_make_simple(name, NULL, "Tester")) return false;
	player->depth = depth;
	prepare_next_level(player);
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);
	return true;
}

/** Recompute the derived state after changing a timed effect by hand. */
static void resettle(void)
{
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);
}

/**
 * Find a neighbouring grid that is in bounds and unoccupied, and its direction.
 *
 * Returns -1 if there is none.  A monster standing next to the player is the
 * trap here: `move_player()` into an occupied grid attacks rather than moves,
 * so a walk that never happened reads as a wall that held.  Measured at about
 * one run in twenty when this was first hit in `player/race`.
 */
static int free_neighbour(struct loc *target)
{
	int d;

	for (d = 0; d < 8; d++) {
		*target = loc_sum(player->grid, ddgrid_ddd[d]);
		if (!square_in_bounds_fully(cave, *target)) continue;
		if (square_monster(cave, *target)) continue;
		return d;
	}
	return -1;
}

/**
 * Set the neighbouring grid to `feat` and try to walk into it.  Did we move?
 */
static bool steps_into(const char *feat_name)
{
	struct loc target;
	int feat = lookup_feat_code(feat_name);
	int d;

	/*
	 * An unknown code is a failing test, not a passing one:
	 * `square_set_feat()` would index `f_info` with -1, which scribbles
	 * quietly without ASAN and leaves the move succeeding for the wrong
	 * reason.
	 */
	if (feat < 0) return false;

	d = free_neighbour(&target);
	if (d < 0) return false;

	square_set_feat(cave, target, feat);
	move_player(ddd[d], false);
	return loc_eq(player->grid, target);
}

/**
 * The mutation grants the form, for as long as Zangband says.
 *
 * `rand_range(lev / 2, lev)` ([mutation.c:1579](../archive/zangband/src/mutation.c#L1579)),
 * carried as `$B+d$S` with the two halves bound separately.  Checked by firing
 * the chain many times and holding every roll inside the band *and* seeing both
 * ends of it -- a chain that always returned the minimum would sit inside the
 * range and be wrong, and a dice string that lost its expression would come
 * back as a constant.
 */
static int test_the_form_lasts_as_long_as_the_archive_says(void *state) {
	const struct mutation *m;
	const struct power_effect *band;
	int lo = 0, hi = 0, i, seen_low = 0, seen_high = 0;

	require(stand_up("Human", 1));
	player->lev = 20;

	m = mutation_by_name("WRAITH");
	notnull(m);
	notnull(m->fires);				/* it is no longer deferred */

	lo = player->lev / 2;			/* 10 */
	hi = player->lev;				/* 20 */

	for (i = 0; i < 200; i++) {
		bool ident = false;

		player_clear_timed(player, TMD_WRAITH, false, false);
		for (band = m->fires->effects; band; band = band->next) {
			effect_do(band->effect, source_player(), NULL, &ident, true,
					  0, 0, 0, NULL);
		}
		require(player->timed[TMD_WRAITH] >= lo);
		require(player->timed[TMD_WRAITH] <= hi);
		if (player->timed[TMD_WRAITH] <= lo + 1) seen_low++;
		if (player->timed[TMD_WRAITH] >= hi - 1) seen_high++;
	}

	/* And it is a roll rather than a constant at either end */
	require(seen_low > 0);
	require(seen_high > 0);
	ok;
}

/**
 * A wraith walks through rock, and an ordinary character does not.
 *
 * The movement is not reimplemented: the timed effect grants `OF_PASS_WALL`
 * through `flag-synonym`, so `player_can_pass_walls()` sees a Spectre.  What
 * comes with that is the permanent-wall exception, asserted here because it is
 * the half that would let a wraith walk out of the world.
 */
static int test_a_wraith_walks_through_rock(void *state) {
	require(stand_up("Human", 1));
	require(!steps_into("GRANITE"));		/* solid, and stays solid */

	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	require(player_of_has(player, OF_PASS_WALL));
	require(steps_into("GRANITE"));

	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	require(steps_into("MAGMA"));

	/* But not through what a Spectre cannot pass either */
	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	require(!steps_into("PERM"));

	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	require(!steps_into("WORLD_EDGE"));

	/* And the flag goes when the form does */
	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	require(!player_of_has(player, OF_PASS_WALL));
	ok;
}

/**
 * Wraith form suppresses the wall damage rather than causing it.
 *
 * The deferral note in `mutmap.toml` had this the wrong way round for two
 * years -- "they pass through walls and take damage from doing it".  The
 * archive tests `!p_ptr->tim.wraith_form` before charging anything, so a wraith
 * pays nothing where a Spectre pays `1 + depth / 10`.  Both sides here,
 * because a build that simply never charged anyone would pass the first half.
 */
static int test_a_wraith_pays_nothing_for_the_rock(void *state) {
	int before, i;

	/* A Spectre in the same grid pays */
	require(stand_up("Spectre", 10));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));
	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	require(player->chp < before);

	/*
	 * The same Spectre in wraith form does not -- over two hundred turns,
	 * because one turn cannot tell the difference.
	 *
	 * At depth 10 the charge is two points, and the wraith's own nine tenths
	 * would turn that into nothing nine times out of ten all by itself.  A
	 * single turn therefore passes about ninety per cent of the time against a
	 * build that has lost the suppression entirely -- measured, by removing it
	 * and watching this test pass.  Two hundred turns of an untouched hit
	 * point total is a suppression; the same two hundred without it cost about
	 * twenty.
	 */
	player_set_timed(player, TMD_WRAITH, 100000, false, false);
	resettle();
	player->chp = player->mhp;
	before = player->chp;
	for (i = 0; i < 200; i++) process_world(cave);
	eq(before - player->chp, 0);
	require(player->timed[TMD_WRAITH] > 0);		/* it did not just expire */

	/* And pays again the moment it ends */
	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	require(player->chp < before);
	ok;
}

/**
 * When the form runs out inside rock, the rock closes (PLR-16).
 *
 * This is the case the whole feature turns on.  A Spectre grinds down to
 * `depth / 10` and stops, because it belongs there; an ordinary character
 * whose form has expired is being crushed, and the archive lifts that floor
 * for exactly this case -- `(chp > depth / 10) || !PASS_WALL`.
 *
 * Asserted from both ends and at one hit point, because a clamp that never
 * fires looks exactly like a clamp that works: the Spectre must survive five
 * hundred turns at 1 hp and the ex-wraith must not.
 */
static int test_the_rock_closes_when_the_form_runs_out(void *state) {
	int i;

	/* A Spectre in rock survives indefinitely */
	require(stand_up("Spectre", 20));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);

	/* A character whose wraith form has gone does not */
	require(stand_up("Human", 20));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));
	player_set_timed(player, TMD_WRAITH, 3, false, false);
	resettle();

	/* Untouched while it lasts */
	player->chp = player->mhp;
	i = player->chp;
	process_world(cave);
	eq(i - player->chp, 0);

	/* Then the form goes, and the rock does not wait */
	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	require(!player_of_has(player, OF_PASS_WALL));
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(player->is_dead);
	ok;
}

/**
 * A hundred points of armour and reflection, and neither without the form.
 */
static int test_the_form_armours_and_reflects(void *state) {
	int bare;

	require(stand_up("Human", 1));
	bare = player->state.to_a;
	require(!of_has(player->state.flags, OF_REFLECT));

	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	eq(player->state.to_a - bare, 100);
	require(of_has(player->state.flags, OF_REFLECT));

	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	eq(player->state.to_a, bare);
	require(!of_has(player->state.flags, OF_REFLECT));
	ok;
}

/**
 * Nine tenths of every blow goes through a wraith.
 *
 * `damage /= 10`, and a tenth of what rounds to nothing comes back as a single
 * point ([effects.c:3285](../archive/zangband/src/effects.c#L3285)).  The small
 * blow is the half that distinguishes this from `perc_dam_red = 90`, which
 * rounds the other way and would return 1 every time rather than mostly 0.
 */
static int test_a_wraith_takes_a_tenth(void *state) {
	int i, scratches = 0;

	require(stand_up("Human", 1));
	eq(player_apply_damage_reduction(player, 100), 100);

	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	eq(player_apply_damage_reduction(player, 100), 10);
	eq(player_apply_damage_reduction(player, 250), 25);

	/* A five point blow mostly stops entirely, and sometimes scratches */
	for (i = 0; i < 400; i++) {
		int got = player_apply_damage_reduction(player, 5);

		require(got == 0 || got == 1);
		if (got == 1) scratches++;
	}
	require(scratches > 0);			/* the one-in-ten is there */
	require(scratches < 200);		/* and it is not every time */

	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	eq(player_apply_damage_reduction(player, 100), 100);
	ok;
}

/**
 * Light ends the form, and does so even when the light is resisted.
 *
 * The archive cancels outside every branch of `GF_LITE`, after the damage and
 * regardless of resistance or immunity
 * ([spells1.c:3595](../archive/zangband/src/spells1.c#L3595)).  A version that
 * cancelled only on unresisted light would pass the first half of this.
 */
static int test_light_forces_a_wraith_back(void *state) {
	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();

	project(source_player(), 0, player->grid, 10, PROJ_LIGHT,
			PROJECT_PLAY | PROJECT_SELF | PROJECT_HIDE, 0, 0, NULL);
	eq(player->timed[TMD_WRAITH], 0);

	/*
	 * And again for a character who resists the light entirely.  The
	 * resistance is written straight into the derived state rather than worn,
	 * because `resettle()` would recompute it away -- and it is `state` that
	 * `player_resists()` reads, which is the thing the handler branches on.
	 */
	require(stand_up("Human", 1));
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	player->state.el_info[ELEM_LIGHT].res_level = 1;
	require(player_resists(player, ELEM_LIGHT));

	project(source_player(), 0, player->grid, 10, PROJ_LIGHT,
			PROJECT_PLAY | PROJECT_SELF | PROJECT_HIDE, 0, 0, NULL);
	eq(player->timed[TMD_WRAITH], 0);
	ok;
}

/**
 * Darkness feeds a wraith instead of hurting it.
 *
 * Both sides, because a build that had simply made the player immune to dark
 * would pass the first assertion: without the form the same projection must
 * still take hit points off.
 */
static int test_darkness_feeds_a_wraith(void *state) {
	int before;

	require(stand_up("Human", 5));

	/* Without the form, dark hurts */
	player->chp = player->mhp / 2;
	before = player->chp;
	project(source_player(), 0, player->grid, 20, PROJ_DARK,
			PROJECT_PLAY | PROJECT_SELF | PROJECT_HIDE, 0, 0, NULL);
	require(player->chp < before);

	/* With it, the same projection heals */
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	player->chp = player->mhp / 2;
	before = player->chp;
	project(source_player(), 0, player->grid, 20, PROJ_DARK,
			PROJECT_PLAY | PROJECT_SELF | PROJECT_HIDE, 0, 0, NULL);
	require(player->chp > before);
	require(player->chp <= player->mhp);
	ok;
}

/**
 * The form and a pass-wall ego on top of each other stay coherent (BAL-08).
 *
 * `PASS_WALL` is in the `OFT_MISC` pool that `KF_RAND_POWER` draws from, so a
 * character can hold the flag permanently from an ego item and be in wraith
 * form at the same time.  Nothing here should double up or cancel: the flag is
 * a flag, the damage suppression is the form's, and when the form ends the ego
 * is still carrying the flag -- so that character is a Spectre again rather
 * than a corpse, and can walk out.
 */
static int test_the_form_and_a_worn_pass_wall_agree(void *state) {
	struct object *obj;
	int before, i;

	require(stand_up("Human", 20));

	/* Give the flag the way an ego would: on something worn */
	obj = object_new();
	object_prep(obj, lookup_kind(TV_AMULET,
			lookup_sval(TV_AMULET, "Wisdom")), 0, RANDOMISE);
	of_on(obj->flags, OF_PASS_WALL);
	obj->known = object_new();
	object_set_base_known(player, obj);
	inven_carry(player, obj, true, false);
	inven_wield(obj, wield_slot(obj));
	resettle();
	require(player_of_has(player, OF_PASS_WALL));

	/* In rock, it grinds and floors, as a Spectre's does */
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);

	/* The form on top suppresses even that */
	player_set_timed(player, TMD_WRAITH, 100, false, false);
	resettle();
	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	eq(before - player->chp, 0);

	/*
	 * And when it ends the ego is still there, so the rock does not close --
	 * this is the character the crushing branch must *not* kill.
	 */
	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	require(player_of_has(player, OF_PASS_WALL));
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);
	ok;
}

/**
 * The status bar says so, and the mutation is no longer on the deferred list.
 *
 * Cheap, and it is the assertion that catches the data half going missing: a
 * timed effect with no grade never shows, and a player who cannot see that
 * they are incorporeal cannot make the decision the form exists for.
 */
static int test_the_form_is_visible_and_no_longer_deferred(void *state) {
	const struct mutation *m = mutation_by_name("WRAITH");
	int idx = timed_name_to_idx("WRAITH");

	notnull(m);
	notnull(m->fires);
	eq(m->kind, MUTATION_KIND_RANDOM);
	require(m->chance > 0);

	require(idx >= 0);
	notnull(timed_effects[idx].grade);
	notnull(timed_effects[idx].grade->next);		/* a real grade, not just "none" */
	eq(timed_effects[idx].oflag_dup, OF_PASS_WALL);
	require(!timed_effects[idx].oflag_syn);
	ok;
}

/**
 * A Spectre outlives its own wraith form, because the race carries the flag.
 *
 * The crushing branch reads `OF_PASS_WALL` rather than the timed effect, so a
 * race that has the flag permanently keeps it when the form ends and drops back
 * to the floored density damage instead of being crushed.  This is the case
 * that would be a bug if it went the other way -- a Spectre killed by a
 * mutation for doing the thing its race does -- so it is asserted directly on
 * the race rather than through a worn item.
 */
static int test_a_spectre_outlives_its_own_wraith_form(void *state) {
	int i, before;

	require(stand_up("Spectre", 30));
	require(player_of_has(player, OF_PASS_WALL));

	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));

	/* In the form, and out of it, and still standing either way */
	player_set_timed(player, TMD_WRAITH, 5, false, false);
	resettle();
	player->chp = 1;
	for (i = 0; i < 200 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);

	player_clear_timed(player, TMD_WRAITH, false, false);
	resettle();
	require(player_of_has(player, OF_PASS_WALL));

	/*
	 * It does pay -- proved before the survival is asserted, because a grid
	 * that charged nothing at all would pass "it did not die" without the
	 * race doing any of the work.
	 */
	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	require(player->chp < before);

	/* And it pays without dying, from one hit point, which is the point */
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);
	require(player->chp >= 0);
	ok;
}

/**
 * The death says what killed you (PLR-16).
 *
 * `died_from` is what the tomb prints after "by" and what the character dump
 * prints after "Killed by", so it is the whole of what a player is told after
 * the fact.  "solid rock" would read as a bug rather than as the risk the
 * mutation carries.
 *
 * Both branches, because the generic one is deliberately still there: a
 * character who has never had the mutation and is somehow inside a wall should
 * report something that reads like the bug it would be.
 */
static int test_the_crushing_death_names_the_mutation(void *state) {
	const struct mutation *wraith = mutation_by_name("WRAITH");
	int i;

	notnull(wraith);

	/* With the mutation: the mutation is named */
	require(stand_up("Human", 30));
	require(player_gain_mutation(player, wraith));
	require(player_has_mutation(player, wraith));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));

	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(player->is_dead);
	require(streq(player->died_from, "an expired wraith form"));

	/* Without it: the canary keeps its own killer */
	require(stand_up("Human", 30));
	require(!player_has_mutation(player, wraith));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	require(!square_ispassable(cave, player->grid));

	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(player->is_dead);
	require(streq(player->died_from, "solid rock"));

	/* And a Spectre ground down in rock is not dying of either */
	require(stand_up("Spectre", 30));
	square_set_feat(cave, player->grid, lookup_feat_code("GRANITE"));
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++) process_world(cave);
	require(!player->is_dead);
	ok;
}

const char *suite_name = "player/wraith";
struct test tests[] = {
	{ "the-form-lasts-as-long-as-the-archive-says",
	  test_the_form_lasts_as_long_as_the_archive_says },
	{ "a-wraith-walks-through-rock", test_a_wraith_walks_through_rock },
	{ "a-wraith-pays-nothing-for-the-rock",
	  test_a_wraith_pays_nothing_for_the_rock },
	{ "the-rock-closes-when-the-form-runs-out",
	  test_the_rock_closes_when_the_form_runs_out },
	{ "the-form-armours-and-reflects", test_the_form_armours_and_reflects },
	{ "a-wraith-takes-a-tenth", test_a_wraith_takes_a_tenth },
	{ "light-forces-a-wraith-back", test_light_forces_a_wraith_back },
	{ "darkness-feeds-a-wraith", test_darkness_feeds_a_wraith },
	{ "the-form-and-a-worn-pass-wall-agree",
	  test_the_form_and_a_worn_pass_wall_agree },
	{ "the-form-is-visible-and-no-longer-deferred",
	  test_the_form_is_visible_and_no_longer_deferred },
	{ "a-spectre-outlives-its-own-wraith-form",
	  test_a_spectre_outlives_its_own_wraith_form },
	{ "the-crushing-death-names-the-mutation",
	  test_the_crushing_death_names_the_mutation },
	{ NULL, NULL }
};
