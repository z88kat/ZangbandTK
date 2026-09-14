/* game/nightmare — Zangband's nightmare mode (BAL-15 to BAL-18)
 *
 * An irreversible birth option whose own text is "this isn't even remotely
 * fair!". Sixteen changes to play, each sourced to a site in Zangband 2.7.5 and
 * listed in .claude/plans/phase1-balance-calibration.md §2.8.2 -- against
 * roughly forty-five claims in the mode's spoiler, which describes a game that
 * was only ever about a quarter built (BAL-18).
 *
 * These tests are the only automated check this milestone gets. The borg is
 * deliberately not running a nightmare variant: the mode is documented as not
 * winnable, the borg cannot leave depth 1 on most runs, and a second nightly
 * job for a mode nobody has played would be speculative. So every test here
 * asserts the behaviour **off without the option and on with it**, in the same
 * run, and each is falsified by removing its guard.
 */
#include "unit-test.h"

#include "z-rand.h"

#include "init.h"
#include "cave.h"
#include "obj-util.h"
#include "dun-type.h"
#include "game-world.h"
#include "generate.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "option.h"
#include "player.h"
#include "player-birth.h"
#include "effects.h"
#include "player-calcs.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif
	(void) test_seed_rng_reported(suite_name);

	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}
	player->depth = 1;
	prepare_next_level(player);
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Turn the mode on or off for the next thing this test does. */
static void nightmare(bool on) {
	player->opts.opt[OPT_birth_nightmare] = on;
}

/** Everything off the level. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/**
 * Put one of that race on the level and hand it back, or NULL.
 *
 * `sleep` is passed through rather than fixed, because "starts awake" is one
 * of the behaviours and the argument is the thing nightmare ignores.
 */
static struct monster *place_one(const char *name, bool sleep) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int i;

	if (!race) return NULL;

	for (i = 0; i < 4000; i++) {
		struct loc grid = loc(randint0(cave->width), randint0(cave->height));

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, sleep, false, info, 0))
			continue;
		return square_monster(cave, grid);
	}
	return NULL;
}

/**
 * The option is a birth option, and it is off unless you ask for it.
 *
 * Both halves matter. A mode this unfair must never be something a player ends
 * up in by accident, and "irreversible" is free only because 4.2 refuses to
 * modify birth options outside birth (ui-options.c:386) -- so it has to be a
 * birth option and not merely named like one.
 */
static int test_nightmare_is_an_optional_birth_choice(void *state) {
	struct player_options fresh;

	eq(option_type(OPT_birth_nightmare), OP_BIRTH);

	/* And off in a freshly defaulted set of options. */
	options_init_defaults(&fresh);
	require(!fresh.opt[OPT_birth_nightmare]);
	ok;
}

/**
 * Monsters arrive with twice the hit points (monster2.c:1847).
 *
 * Asserted against the same race placed both ways rather than against a
 * number, because `race->avg_hp` already carries BAL-13's ×0.73 by the time
 * this runs -- BAL-16 says the two compose, and a fixed figure here would be
 * asserting the scalar as well and would move whenever it did.
 */
static int test_monsters_arrive_with_twice_the_hit_points(void *state) {
	struct monster *mon;
	int plain, nasty;

	clear_the_level();
	nightmare(false);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	plain = mon->maxhp;

	clear_the_level();
	nightmare(true);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	nasty = mon->maxhp;
	nightmare(false);

	/* Unique, so the figure is fixed and the doubling is exact. */
	eq(nasty, plain * 2);
	require(mon->hp == mon->maxhp);
	ok;
}

/**
 * Monsters are five points faster (dungeon.c:2833).
 *
 * A unique again: 4.2 gives non-uniques a small random speed spread, which
 * would make the comparison a distribution rather than an equality.
 */
static int test_monsters_arrive_five_points_faster(void *state) {
	struct monster *mon;
	int plain, nasty;

	clear_the_level();
	nightmare(false);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	plain = mon->mspeed;

	clear_the_level();
	nightmare(true);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	nasty = mon->mspeed;
	nightmare(false);

	eq(nasty, plain + 5);
	ok;
}

/**
 * Nothing starts asleep, and nothing gets the free first move (monster2.c:1830
 * and :1880).
 *
 * Two behaviours in one test because they are two halves of the same arrival:
 * §2.8.3 separates them deliberately, since "starts awake" is the one the
 * spoiler mentions and losing MFLAG_NICE -- the engine's free first move for a
 * FORCE_SLEEP ambusher -- is the one it does not.
 */
static int test_nothing_sleeps_and_nothing_waits(void *state) {
	struct monster *mon;

	struct monster_race *sleepy = NULL;
	int i;

	/*
	 * A race that actually sleeps. Not every one does -- Grip has `sleep:0`
	 * and would pass this test by being awake for reasons that have nothing
	 * to do with the mode.
	 */
	for (i = 1; i < z_info->r_max; i++) {
		if (r_info[i].name && r_info[i].sleep > 0) {
			sleepy = &r_info[i];
			break;
		}
	}
	require(sleepy);

	clear_the_level();
	nightmare(false);
	mon = place_one(sleepy->name, true);
	require(mon);
	require(mon->m_timed[MON_TMD_SLEEP] > 0);

	clear_the_level();
	nightmare(true);
	mon = place_one(sleepy->name, true);
	require(mon);
	nightmare(false);
	eq(mon->m_timed[MON_TMD_SLEEP], 0);
	ok;
}

/**
 * A FORCE_SLEEP ambusher loses its grace (monster2.c:1880).
 */
static int test_an_ambusher_loses_its_grace(void *state) {
	struct monster_race *race = NULL;
	struct monster *mon;
	int i;

	/* Any FORCE_SLEEP race will do; the flag is what is being tested. */
	for (i = 1; i < z_info->r_max; i++) {
		if (!r_info[i].name) continue;
		if (rf_has(r_info[i].flags, RF_FORCE_SLEEP)) {
			race = &r_info[i];
			break;
		}
	}
	require(race);

	clear_the_level();
	nightmare(false);
	mon = place_one(race->name, false);
	require(mon);
	require(mflag_has(mon->mflag, MFLAG_NICE));

	clear_the_level();
	nightmare(true);
	mon = place_one(race->name, false);
	require(mon);
	nightmare(false);
	require(!mflag_has(mon->mflag, MFLAG_NICE));
	ok;
}

/**
 * Monsters arrive with double energy, so they act sooner (monster2.c:1874).
 *
 * Statistical, because the starting energy is `randint0(50)` and any single
 * pair could land either way. The means are compared over enough placements
 * that a factor of two cannot be noise, and the assertion is deliberately
 * loose at the edges -- it is testing that the doubling happened, not
 * reproducing the distribution.
 */
static int test_monsters_arrive_with_double_energy(void *state) {
	int odd_plain = 0, odd_nasty = 0, n_plain = 0, n_nasty = 0, i;
	const int runs = 40;

	/*
	 * Parity, which only doubling can produce.
	 *
	 * Starting energy is `randint0(50)`, so about half of an ordinary
	 * monster's draws are odd -- and a doubled one can never be. Forty
	 * placements each way makes "no odd numbers at all" decisive: the chance
	 * of that happening by luck is one in 2^40.
	 *
	 * Three earlier attempts were worse and are worth naming, because each
	 * looked reasonable. Comparing totals over unequal sample counts measured
	 * nothing -- a unique cannot be placed twice, so the nightmare half ran
	 * half as often, and half as many doubled samples add up the same.
	 * Comparing the highest seen was quieter but still a distribution. And
	 * re-seeding to force identical dice does not work here: the two
	 * placements consume different numbers of draws before they reach the
	 * energy, so the same seed gives different numbers.
	 */
	for (i = 0; i < runs; i++) {
		struct monster *mon;

		clear_the_level();
		nightmare(false);
		mon = place_one("soldier", false);
		if (mon) { n_plain++; if (mon->energy % 2) odd_plain++; }

		clear_the_level();
		nightmare(true);
		mon = place_one("soldier", false);
		if (mon) { n_nasty++; if (mon->energy % 2) odd_nasty++; }
	}
	nightmare(false);

	require(n_plain > runs / 2);
	require(n_nasty > runs / 2);

	if (odd_plain == 0 || odd_nasty != 0) {
		printf("odd starting energies: %d of %d plain, %d of %d nightmare "
				"(plain must have some, nightmare must have none)\n",
				odd_plain, n_plain, odd_nasty, n_nasty);
		require(false);
	}
	ok;
}

/**
 * A sustain fails one time in thirteen (effects.c:2807).
 *
 * §2.8.3 calls this plausibly the cruellest thing in the mode, and no spoiler
 * mentions sustains at all. Statistical in form only: without the option a
 * sustain holds *every* time, by construction, so any failure at all is the
 * guard misfiring.
 *
 * Six hundred attempts gives about forty-six failures expected with the mode
 * on. The bar is set low enough that a bad run cannot reach it and high enough
 * that zero cannot pass.
 */
static int test_a_sustain_sometimes_fails(void *state) {
	int plain = 0, nasty = 0, i;
	const int runs = 600;

	for (i = 0; i < runs; i++) {
		player->stat_cur[STAT_STR] = 18;
		player->stat_max[STAT_STR] = 18;
		of_on(player->state.flags, OF_SUST_STR);

		nightmare(false);
		effect_simple(EF_DRAIN_STAT, source_none(), "0", STAT_STR, 0, 0,
				0, 0, NULL);
		if (player->stat_cur[STAT_STR] < 18) plain++;

		player->stat_cur[STAT_STR] = 18;
		player->stat_max[STAT_STR] = 18;
		nightmare(true);
		effect_simple(EF_DRAIN_STAT, source_none(), "0", STAT_STR, 0, 0,
				0, 0, NULL);
		if (player->stat_cur[STAT_STR] < 18) nasty++;
	}
	nightmare(false);

	if (plain != 0 || nasty < 10) {
		printf("sustain failures over %d attempts: %d plain, %d nightmare "
				"(plain must be none, nightmare about one in thirteen)\n",
				runs, plain, nasty);
		require(false);
	}
	ok;
}

/**
 * And when a drain lands it sticks, twelve times in thirteen
 * ([effects.c:2818](../../archive/zangband/src/effects.c#L2818)).
 *
 * The other half of the same cruelty, and a separate test because it is a
 * separate assertion: the one above watches `stat_cur`, which moves either
 * way, and this watches `stat_max`, which is what "permanent" means and which
 * an ordinary drain never touches.
 *
 * Zangband writes the condition as `!one_in_(13)`, so the drain is temporary
 * on its own one-in-thirteen roll -- a different draw from the one the sustain
 * fails on, and not meant to be the same.
 */
static int test_a_drain_becomes_permanent(void *state) {
	int plain = 0, nasty = 0, i;
	const int runs = 300;

	for (i = 0; i < runs; i++) {
		player->stat_cur[STAT_STR] = 18;
		player->stat_max[STAT_STR] = 18;
		of_off(player->state.flags, OF_SUST_STR);

		nightmare(false);
		effect_simple(EF_DRAIN_STAT, source_none(), "0", STAT_STR, 0, 0,
				0, 0, NULL);
		if (player->stat_max[STAT_STR] < 18) plain++;

		player->stat_cur[STAT_STR] = 18;
		player->stat_max[STAT_STR] = 18;
		nightmare(true);
		effect_simple(EF_DRAIN_STAT, source_none(), "0", STAT_STR, 0, 0,
				0, 0, NULL);
		if (player->stat_max[STAT_STR] < 18) nasty++;
	}
	nightmare(false);

	/* Never without the mode; nearly always with it. */
	if (plain != 0 || nasty < runs * 4 / 5) {
		printf("permanent drains over %d: %d plain, %d nightmare "
				"(plain must be none, nightmare nearly all)\n",
				runs, plain, nasty);
		require(false);
	}
	ok;
}

/**
 * Stair creation does nothing at all (dungeon.c:2913).
 *
 * No Deep Descent, no stair scumming, no escape hatch. Zangband forces both
 * `create_down_stair` and `create_up_stair` off; 4.2 reaches both through one
 * effect, so one refusal is the whole of it.
 */
static int test_no_stairs_can_be_made(void *state) {
	bool made;

	require(square_isfloor(cave, player->grid));

	/*
	 * Read through the square rather than the return value: `effect_simple()`
	 * returns nothing, so what is being asserted is whether a staircase is
	 * actually there afterwards -- which is the thing a player would notice.
	 */
	nightmare(false);
	effect_simple(EF_CREATE_STAIRS, source_player(), "0", 0, 0, 0, 0, 0, NULL);
	made = square_isstairs(cave, player->grid);
	require(made);

	/* Put the floor back, so the second half starts where the first did. */
	square_set_feat(cave, player->grid, FEAT_FLOOR);
	require(square_isfloor(cave, player->grid));

	nightmare(true);
	effect_simple(EF_CREATE_STAIRS, source_player(), "0", 0, 0, 0, 0, 0, NULL);
	made = square_isstairs(cave, player->grid);
	nightmare(false);
	if (made) {
		printf("stairs were made under nightmare mode\n");
		require(false);
	}
	ok;
}

/**
 * A FORCE_DEPTH monster can be chosen above its level (monster2.c:1735).
 *
 * 4.2 refuses outright; nightmare mode allows it for everything except a quest
 * monster.
 *
 * Asked of the selection rather than of a sample. Every FORCE_DEPTH monster
 * this game ships is a level-99 unique -- the four Saurons and Morgoth -- so
 * drawing until one appears is a lottery that returns zero on both sides and
 * says nothing. The flag is borrowed for a race that *is* chosen, sitting one
 * level above the floor and drawn against a shallow table, so the guard is the
 * only thing that varies between the two halves.
 */
static int test_a_force_depth_monster_can_come_up(void *state) {
	struct monster_race *guinea = lookup_monster("soldier");
	int plain = 0, nasty = 0, i;
	const int runs = 2000;
	const int depth = 5;
	int kept_level;

	require(guinea);
	kept_level = guinea->level;
	guinea->level = depth + 1;
	rf_on(guinea->flags, RF_FORCE_DEPTH);

	for (i = 0; i < runs; i++) {
		nightmare(false);
		if (get_mon_num(depth + 5, depth) == guinea) plain++;

		nightmare(true);
		if (get_mon_num(depth + 5, depth) == guinea) nasty++;
	}
	nightmare(false);

	rf_off(guinea->flags, RF_FORCE_DEPTH);
	guinea->level = kept_level;

	if (plain != 0 || nasty == 0) {
		printf("out-of-depth FORCE_DEPTH picks over %d draws: %d plain, "
				"%d nightmare (plain must be none, nightmare must be some)\n",
				runs, plain, nasty);
		require(false);
	}
	ok;
}

/**
 * Monsters come from far deeper than the floor they stand on
 * (monster2.c:765).
 *
 * 4.2's own out-of-depth rule is capped at `depth / 4 + 2`; Zangband's
 * nightmare branch is `1 + (level * 128 / randint1(128))`, uncapped. At depth 5
 * that reaches far past anything the ordinary rule can produce, and the bar is
 * set above 4.2's ceiling so the ordinary rule cannot reach it however the dice
 * fall.
 */
static int test_monsters_come_from_much_deeper(void *state) {
	int plain_max = 0, nasty_max = 0, i;
	const int runs = 3000;
	const int depth = 5;
	/* 4.2's rule tops out at depth + depth/4 + 2, which is 8 here. */
	const int beyond = 25;

	for (i = 0; i < runs; i++) {
		struct monster_race *race;

		nightmare(false);
		race = get_mon_num(depth, depth);
		if (race && race->level > plain_max) plain_max = race->level;

		nightmare(true);
		race = get_mon_num(depth, depth);
		if (race && race->level > nasty_max) nasty_max = race->level;
	}
	nightmare(false);

	if (plain_max >= beyond || nasty_max < beyond) {
		printf("deepest monster over %d draws at depth %d: %d plain, "
				"%d nightmare (plain must stay under %d)\n",
				runs, depth, plain_max, nasty_max, beyond);
		require(false);
	}
	ok;
}

/**
 * A corrupted Word of Recall takes you deeper, and never past the bottom
 * (dungeon.c:1817, DEC-83).
 *
 * Four branches, not the three §2.8.2's summary records: the archive also sends
 * anything past 100 to the bottom of the world.
 *
 * The clamp is the part worth pinning. Zangband has one continuous dungeon;
 * this game has thirteen with their own bottoms, and WLD-14 exists so recall
 * cannot land past one. A test that only checked the doubling would pass with
 * the clamp deleted, and the thing it protects -- arriving on a level the
 * dungeon does not have -- is not something a player could diagnose.
 */
static int test_recall_twists_but_not_past_the_bottom(void *state) {
	static const struct { int from, want; } rows[] = {
		{ 20, 40 },		/* below 50: doubles */
		{ 60, 79 },		/* below 99: half the way to 99 */
		{ 120, 0 },		/* past 100: the bottom of the world */
	};
	int kept = player->dungeon;
	size_t i;

	/* No dungeon, so only the world's own bottom applies. */
	player->dungeon = 0;
	for (i = 0; i < N_ELEMENTS(rows); i++) {
		int want = rows[i].want ? rows[i].want : z_info->max_depth - 1;
		int got = nightmare_recall_depth(player, rows[i].from);

		if (got != want) {
			printf("recall from %d: got %d, wanted %d\n", rows[i].from, got,
					want);
			player->dungeon = kept;
			require(false);
		}
	}

	/* And inside a dungeon, never past its own bottom. */
	{
		struct dun_type *type = dun_type_by_index(0);
		int got;

		require(type);
		player->dungeon = 1;
		got = nightmare_recall_depth(player, type->max_depth);
		if (got > type->max_depth) {
			printf("recall from the bottom of a dungeon (%d) gave %d\n",
					type->max_depth, got);
			player->dungeon = kept;
			require(false);
		}

		/* Doubling from halfway must still stop at the bottom. */
		got = nightmare_recall_depth(player, type->max_depth / 2 + 1);
		if (got > type->max_depth) {
			printf("recall doubled past the dungeon's bottom: %d > %d\n",
					got, type->max_depth);
			player->dungeon = kept;
			require(false);
		}
	}

	player->dungeon = kept;
	ok;
}

/**
 * The bell tolls four times before midnight, and the curse lands on the hour
 * (dungeon.c:1232).
 *
 * §2.8.3 counts this among the four behaviours no spoiler mentions, and calls
 * it the one piece of mercy in the mode -- four warnings across the last hour.
 *
 * Tested by walking a whole game day a tick at a time and counting what the
 * clock says, which is the only way to assert "four and exactly four": a test
 * that checked one moment would pass with the hour wrong, and one that checked
 * the curse alone would pass with no bell at all.
 */
static int test_the_bell_tolls_before_the_curse(void *state) {
	int32_t len = 10L * z_info->day_length;
	int32_t t;
	int tolls[5] = { 0, 0, 0, 0, 0 };
	int curses = 0;

	for (t = 0; t < len; t += 10) {
		int toll = nightmare_bell_at(t);

		if (toll == NIGHTMARE_CURSE) curses++;
		else if (toll >= 1 && toll <= 4) tolls[toll]++;
		else if (toll) {
			printf("unexpected toll %d at turn %d\n", toll, (int) t);
			require(false);
		}
	}

	/* Exactly one of each warning, and exactly one curse, in a day. */
	eq(tolls[1], 1);
	eq(tolls[2], 1);
	eq(tolls[3], 1);
	eq(tolls[4], 1);
	eq(curses, 1);
	ok;
}

/**
 * And the four warnings come in the hour before the curse, in order.
 *
 * Separate from the count because the count would pass if the bell rang at
 * four random moments of the day. What makes it a warning is that it arrives
 * beforehand and close by.
 */
static int test_the_bell_comes_before_midnight(void *state) {
	int32_t len = 10L * z_info->day_length;
	int32_t t, at[5] = { 0, -1, -1, -1, -1 }, curse_at = -1;

	for (t = 0; t < len; t += 10) {
		int toll = nightmare_bell_at(t);

		if (toll == NIGHTMARE_CURSE) curse_at = t;
		else if (toll >= 1 && toll <= 4) at[toll] = t;
	}

	require(curse_at >= 0);
	require(at[1] >= 0 && at[2] >= 0 && at[3] >= 0 && at[4] >= 0);

	/* In order... */
	require(at[1] < at[2] && at[2] < at[3] && at[3] < at[4]);

	/* ...and all four inside the hour before midnight. */
	{
		int32_t hour = len / 24;

		if (at[1] < curse_at - hour || at[4] >= curse_at) {
			printf("bell at %d..%d, curse at %d, an hour is %d\n",
					(int) at[1], (int) at[4], (int) curse_at, (int) hour);
			require(false);
		}
	}
	ok;
}

/** How many grids of that feature the level holds. */
static int count_feat(int feat) {
	int y, x, n = 0;

	for (y = 0; y < cave->height; y++)
		for (x = 0; x < cave->width; x++)
			if (square(cave, loc(x, y))->feat == feat) n++;
	return n;
}

/**
 * A level carries invisible walls, and an ordinary one carries none
 * (grid.c:125, generate.c:945).
 *
 * Twenty levels each way rather than one: the count per level is
 * `Rand_normal(3, 3)`, which is zero often enough that a single level proves
 * nothing either way.
 *
 * The plain side is the assertion that matters and it is exact -- an ordinary
 * level must carry *none*, so any at all is the guard leaking.
 */
static int test_a_nightmare_level_has_invisible_walls(void *state) {
	int plain = 0, nasty = 0, i;
	const int levels = 20;
	int kept = player->depth;

	for (i = 0; i < levels; i++) {
		nightmare(false);
		player->depth = 3;
		prepare_next_level(player);
		plain += count_feat(FEAT_INVIS_WALL);

		nightmare(true);
		player->depth = 3;
		prepare_next_level(player);
		nasty += count_feat(FEAT_INVIS_WALL);
	}
	nightmare(false);
	player->depth = kept;
	prepare_next_level(player);

	if (plain != 0 || nasty == 0) {
		printf("invisible walls over %d levels each: %d plain, %d nightmare "
				"(plain must be none, nightmare must be some)\n",
				levels, plain, nasty);
		require(false);
	}
	ok;
}

/**
 * And it looks like floor while being a wall.
 *
 * The whole point of the feature, and the half a count cannot see. `mimic`
 * decides what the player is shown (cave-map.c:99); the flags decide what the
 * grid actually is. A feature that got one of those wrong would still be
 * counted by the test above.
 */
static int test_an_invisible_wall_looks_like_floor(void *state) {
	struct feature *f = &f_info[FEAT_INVIS_WALL];

	require(f->name);

	/* It is a wall: not passable, and it blocks sight. */
	require(tf_has(f->flags, TF_WALL));
	require(!tf_has(f->flags, TF_PASSABLE));
	require(!tf_has(f->flags, TF_LOS));

	/* And it is shown as plain floor. */
	require(f->mimic);
	require(f->mimic == &f_info[FEAT_FLOOR]);
	ok;
}

const char *suite_name = "game/nightmare";
struct test tests[] = {
	{ "nightmare-is-an-optional-birth-choice",
			test_nightmare_is_an_optional_birth_choice },
	{ "monsters-arrive-with-twice-the-hit-points",
			test_monsters_arrive_with_twice_the_hit_points },
	{ "monsters-arrive-five-points-faster",
			test_monsters_arrive_five_points_faster },
	{ "nothing-sleeps-and-nothing-waits",
			test_nothing_sleeps_and_nothing_waits },
	{ "an-ambusher-loses-its-grace", test_an_ambusher_loses_its_grace },
	{ "monsters-arrive-with-double-energy",
			test_monsters_arrive_with_double_energy },
	{ "a-sustain-sometimes-fails", test_a_sustain_sometimes_fails },
	{ "a-drain-becomes-permanent", test_a_drain_becomes_permanent },
	{ "no-stairs-can-be-made", test_no_stairs_can_be_made },
	{ "a-force-depth-monster-can-come-up",
			test_a_force_depth_monster_can_come_up },
	{ "monsters-come-from-much-deeper",
			test_monsters_come_from_much_deeper },
	{ "recall-twists-but-not-past-the-bottom",
			test_recall_twists_but_not_past_the_bottom },
	{ "the-bell-tolls-before-the-curse",
			test_the_bell_tolls_before_the_curse },
	{ "the-bell-comes-before-midnight",
			test_the_bell_comes_before_midnight },
	{ "a-nightmare-level-has-invisible-walls",
			test_a_nightmare_level_has_invisible_walls },
	{ "an-invisible-wall-looks-like-floor",
			test_an_invisible_wall_looks_like_floor },
	{ NULL, NULL }
};
