/* player/patron-rewards
 *
 * What a Lord of the Courts does to a servant, as Zangband wrote it (DEC-167).
 *
 * The rewards were first built from the patron spoiler, which named the
 * thirty-one things a Lord can do and said little about what each did, and
 * most came out milder than the source: a WRATH that only hurt, a PISS_OFF
 * that only frightened, an experience reward worth level x 50 at any age, and
 * a "worthy blade" that was whatever acquirement made.  Every test here pins
 * a piece of gain_level_reward() ([xtra2.c:3092]) that the first version got
 * wrong or left out, so that the next reading of the spoiler cannot quietly
 * undo it.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "z-dice.h"
#include "z-util.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Warrior", "Tester")) {
		cleanup_angband();
		return 1;
	}
	prepare_next_level(player);
	on_new_level();
	(void) test_seed_rng_reported(suite_name);
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** A reward by its code, found on whichever ladder carries it. */
static const struct patron_reward *reward_named(const char *code) {
	const struct patron *lord;
	int slot;

	for (lord = patrons; lord; lord = lord->next)
		for (slot = 0; slot < PATRON_LADDER; slot++)
			if (lord->ladder[slot] && streq(lord->ladder[slot]->code, code))
				return lord->ladder[slot];

	return NULL;
}

static bool chain_has(const struct effect *e, int index) {
	for (; e; e = e->next)
		if (e->index == index) return true;
	return false;
}

static void run_reward(const struct patron_reward *reward) {
	bool ident = false;

	(void) effect_do(reward->effect, source_player(), NULL, &ident, true, 0,
					 0, 0, NULL);
}

static void healthy(int lev) {
	player->lev = player->max_lev = lev;
	player->upkeep->update |= (PU_BONUS | PU_HP);
	update_stuff(player);
	player->mhp = 30000;
	player->chp = 30000;
	player_clear_timed(player, TMD_CUT, false, false);
}

/**
 * The ladders carry Zangband's rewards and no others but DEC-38's shapes.
 *
 * Zangband has thirty-one.  TY_CURSE has no rung because WRATH and PISS_OFF
 * both lay the curse, and DO_HAVOC is Call Chaos, which aims, and a reward
 * cannot.  POLY_BEAST and POLY_WOLF are two more 4.2 shapes, under DEC-38.
 */
static int test_every_reward_is_zangbands(void *state) {
	static const char *expected[] = {
		"POLY_SLF", "GAIN_EXP", "LOSE_EXP", "GOOD_OBJ", "GREA_OBJ",
		"CHAOS_WP", "GOOD_OBS", "GREA_OBS", "SUMMON_M", "H_SUMMON",
		"GAIN_ABL", "LOSE_ABL", "RUIN_ABL", "AUGM_ABL", "POLY_WND",
		"HEAL_FUL", "HURT_LOT", "CURSE_WP", "CURSE_AR", "PISS_OFF",
		"WRATH", "DESTRUCT", "GENOCIDE", "MASS_GEN", "DISPEL_C", "IGNORE",
		"SER_UNDE", "SER_DEMO", "SER_MONS", "POLY_BEAST", "POLY_WOLF",
	};
	const struct patron *lord;
	size_t i;
	int slot;

	for (i = 0; i < N_ELEMENTS(expected); i++) {
		if (!reward_named(expected[i]))
			printf("no Lord gives %s\n", expected[i]);
		notnull(reward_named(expected[i]));
	}

	for (lord = patrons; lord; lord = lord->next) {
		for (slot = 0; slot < PATRON_LADDER; slot++) {
			const char *code = lord->ladder[slot]->code;
			bool known = false;

			for (i = 0; i < N_ELEMENTS(expected); i++)
				if (streq(code, expected[i])) known = true;
			if (!known) printf("%s gives %s\n", lord->name, code);
			require(known);
		}
	}

	ok;
}

/**
 * An angry Lord can lay the Ancient Curse.
 *
 * In Zangband every one of the sixteen could, fourteen of them through WRATH.
 * Until DEC-167 none of our nine could: WRATH was a plain hit.  Now WRATH and
 * PISS_OFF both carry it, and every Lord with either can curse you -- seven of
 * the nine.  Mandor and Gramble punish without it, Mandor by taking what you
 * rely on and Gramble by hitting you.
 */
static int test_an_angry_lord_can_lay_the_ancient_curse(void *state) {
	const struct patron *lord;
	int cursers = 0;

	require(chain_has(reward_named("WRATH")->effect, EF_ANCIENT_CURSE));
	require(chain_has(reward_named("WRATH")->effect, EF_HI_SUMMON));
	require(chain_has(reward_named("WRATH")->effect, EF_RUIN_STATS));
	require(chain_has(reward_named("PISS_OFF")->effect, EF_ANCIENT_CURSE));

	for (lord = patrons; lord; lord = lord->next) {
		int slot;
		bool can = false;

		for (slot = 0; slot < PATRON_LADDER; slot++)
			if (chain_has(lord->ladder[slot]->effect, EF_ANCIENT_CURSE))
				can = true;
		if (can) cursers++;
	}

	printf("PATRON %d of 9 Lords can lay the Ancient Curse\n", cursers);
	eq(cursers, 7);

	ok;
}

/**
 * Experience is a fraction of what you have, both ways.
 *
 * Zangband's favour is half your experience plus ten, never more than
 * 100,000, and its displeasure takes a sixth; ours were level x 100 and
 * level x 200 before the effect halved them, which at level forty is a
 * rounding error.  Hold Life does not keep what a Lord takes back.
 */
static int test_experience_is_a_fraction_of_yours(void *state) {
	const struct patron_reward *gain = reward_named("GAIN_EXP");
	const struct patron_reward *lose = reward_named("LOSE_EXP");

	healthy(30);

	player->exp = player->max_exp = 10000;
	run_reward(gain);
	eq(player->exp, 10000 + 10000 / 2 + 10);

	/* Half of three million is far past the cap. */
	player->exp = player->max_exp = 3000000;
	run_reward(gain);
	eq(player->exp, 3000000 + 100000);

	/* A sixth, and Hold Life is no defence against it. */
	player->exp = player->max_exp = 60000;
	of_on(player->state.flags, OF_HOLD_LIFE);
	run_reward(lose);
	of_off(player->state.flags, OF_HOLD_LIFE);
	eq(player->exp, 60000 - 60000 / 6);

	player->exp = player->max_exp = 0;
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);

	ok;
}

/**
 * A Lord's ruin is permanent and WRATH's is a drain.
 *
 * Both lower every stat at once and neither cares about a sustain; the
 * difference is whether restoration can mend it.
 */
static int test_ruin_is_permanent_and_wrath_drains(void *state) {
	const struct patron_reward *ruin = reward_named("RUIN_ABL");
	int stat;
	bool ident = false;

	healthy(30);

	for (stat = 0; stat < STAT_MAX; stat++)
		player->stat_cur[stat] = player->stat_max[stat] = 18 + 50;

	/* WRATH's half: the current values only */
	effect_simple(EF_RUIN_STATS, source_player(), "0", 0, 0, 0, 0, 0,
				  &ident);
	for (stat = 0; stat < STAT_MAX; stat++) {
		require(player->stat_cur[stat] < 18 + 50);
		eq(player->stat_max[stat], 18 + 50);
	}

	for (stat = 0; stat < STAT_MAX; stat++)
		player->stat_cur[stat] = player->stat_max[stat] = 18 + 50;

	/* RUIN_ABL: the maximum goes too */
	run_reward(ruin);
	for (stat = 0; stat < STAT_MAX; stat++) {
		require(player->stat_cur[stat] < 18 + 50);
		require(player->stat_max[stat] < 18 + 50);
	}

	ok;
}

/**
 * A Lord's gift of a stat restores it first, as do_inc_stat() does.
 */
static int test_a_gift_of_a_stat_restores_it(void *state) {
	bool ident = false;

	player->stat_max[STAT_STR] = 18 + 100;
	player->stat_cur[STAT_STR] = 12;

	/* Already at the limit, so nothing to add -- but it is put back */
	effect_simple(EF_GAIN_STAT, source_player(), "0", STAT_STR, 0, 1, 0, 0,
				  &ident);
	eq(player->stat_cur[STAT_STR], 18 + 100);

	/* And a plain gain, which a potion is, still does not restore */
	player->stat_max[STAT_STR] = 18 + 50;
	player->stat_cur[STAT_STR] = 12;
	effect_simple(EF_GAIN_STAT, source_player(), "0", STAT_STR, 0, 0, 0, 0,
				  &ident);
	eq(player->stat_cur[STAT_STR], 13);

	player->stat_cur[STAT_STR] = player->stat_max[STAT_STR];

	ok;
}

/**
 * A polymorphed wound heals, and one time in five opens a new one.
 *
 * With hit points to heal and no cut, the gentle outcome leaves no cut and
 * the nasty one leaves one -- so the cut is the signature of the one in five,
 * and over a thousand tries it should land near two hundred.
 */
static int test_a_wound_is_polymorphed(void *state) {
	static const int trials = 1000;
	int i, nasty = 0;

	for (i = 0; i < trials; i++) {
		int before;

		healthy(30);
		player->chp = before = 10000;

		effect_simple(EF_POLY_WOUNDS, source_player(), "0", 0, 0, 0, 0, 0,
					  NULL);

		/* At least half the heal is always kept */
		require(player->chp > before);

		if (player->timed[TMD_CUT]) nasty++;
	}

	printf("PATRON %d of %d polymorphed wounds opened a new one\n", nasty,
		   trials);

	/* 200 expected, sd 12.6: five sd either side */
	require(nasty > 137);
	require(nasty < 263);

	/* Nothing to heal and no cut: four times in five, nothing at all */
	nasty = 0;
	for (i = 0; i < trials; i++) {
		healthy(30);
		effect_simple(EF_POLY_WOUNDS, source_player(), "0", 0, 0, 0, 0, 0,
					  NULL);
		if (player->timed[TMD_CUT]) nasty++;
		else eq(player->chp, player->mhp);
	}
	require(nasty > 137);
	require(nasty < 263);

	player_clear_timed(player, TMD_CUT, false, false);

	ok;
}

/**
 * ONE_IN runs the effect after it one time in N, and the chain carries on.
 */
static int test_one_in_decides_the_next_effect_only(void *state) {
	struct effect *one = mem_zalloc(sizeof(*one));
	struct effect *restore = mem_zalloc(sizeof(*restore));
	struct effect *gain = mem_zalloc(sizeof(*gain));
	bool ident = false;

	one->index = EF_ONE_IN;
	one->next = restore;
	restore->index = EF_RESTORE_EXP;
	restore->next = gain;
	gain->index = EF_GAIN_EXP;
	gain->dice = dice_new();
	require(dice_parse_string(gain->dice, "100"));

	/* No dice: never.  The restore is skipped, the gain is not. */
	player->max_exp = 1000;
	player->exp = 500;
	(void) effect_do(one, source_player(), NULL, &ident, true, 0, 0, 0, NULL);
	eq(player->exp, 550);

	/* One in one: always. */
	one->dice = dice_new();
	require(dice_parse_string(one->dice, "1"));
	player->max_exp = 1000;
	player->exp = 500;
	(void) effect_do(one, source_player(), NULL, &ident, true, 0, 0, 0, NULL);
	eq(player->exp, 1050);

	free_effect(one);
	player->exp = player->max_exp = 0;
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);

	ok;
}

/** A hostile monster of that race within a few paces of the player. */
static bool put_near(const char *name, struct loc *where) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	struct loc grid;

	if (!race) return false;

	for (grid.y = player->grid.y - 5; grid.y <= player->grid.y + 5; grid.y++) {
		for (grid.x = player->grid.x - 5; grid.x <= player->grid.x + 5;
			 grid.x++) {
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isempty(cave, grid)) continue;
			if (place_new_monster(cave, grid, race, false, false, info,
								  ORIGIN_DROP)) {
				*where = grid;
				return true;
			}
		}
	}

	return false;
}

/**
 * A Lord's banishment costs the servant nothing; a caster's costs the caster.
 */
static int test_a_patrons_banishment_is_free(void *state) {
	const struct patron_reward *mass = reward_named("MASS_GEN");
	struct loc ants[4];
	bool ident = false;
	int i;

	healthy(30);
	for (i = 0; i < 4; i++) require(put_near("soldier ant", &ants[i]));
	run_reward(mass);
	eq(player->chp, 30000);
	for (i = 0; i < 4; i++) null(square_monster(cave, ants[i]));

	/* The spell's own path is untouched */
	for (i = 0; i < 4; i++) require(put_near("soldier ant", &ants[i]));
	effect_simple(EF_MASS_BANISH, source_player(), "0", 0, 0, 0, 0, 0,
				  &ident);
	require(player->chp < 30000);
	for (i = 0; i < 4; i++) null(square_monster(cave, ants[i]));

	player->chp = player->mhp;

	ok;
}

/**
 * A Lord's horde arrives.
 *
 * H_SUMMON was a plain summon of dangerous kin until DEC-167.  Zangband's
 * activate_hi_summon() calls between one and nine groups, each of a type
 * picked by depth; even in an open space some fail for want of room, so ten
 * calls are asked to bring something rather than a particular number.
 */
static int test_a_lords_horde_arrives(void *state) {
	int before = cave_monster_count(cave), i;

	for (i = 0; i < 10; i++)
		effect_simple(EF_HI_SUMMON, source_player(), "0", 0, 0, 0, 0, 0,
					  NULL);

	printf("PATRON %d monsters called by ten hordes\n",
		   cave_monster_count(cave) - before);
	require(cave_monster_count(cave) > before);

	ok;
}

/** The newest sword within a few paces of the player, or NULL. */
static struct object *sword_nearby(void) {
	struct loc grid;
	struct object *found = NULL;

	for (grid.y = player->grid.y - 3; grid.y <= player->grid.y + 3; grid.y++) {
		for (grid.x = player->grid.x - 3; grid.x <= player->grid.x + 3;
			 grid.x++) {
			struct object *obj;

			if (!square_in_bounds(cave, grid)) continue;
			for (obj = square_object(cave, grid); obj; obj = obj->next)
				if (obj->tval == TV_SWORD) found = obj;
		}
	}

	return found;
}

/**
 * The worthy blade is a (Chaotic) sword, and a young servant's is a dagger.
 *
 * It was a single acquirement until DEC-167, which made it the same reward as
 * GOOD_OBJ under a message that promised a blade.
 */
static int test_the_worthy_blade_is_chaotic(void *state) {
	struct object *blade;

	healthy(1);
	effect_simple(EF_CHAOS_BLADE, source_player(), "0", 0, 0, 0, 0, 0,
				  NULL);
	blade = sword_nearby();
	notnull(blade);
	notnull(blade->ego);
	require(streq(blade->ego->name, "(Chaotic)"));
	eq(blade->sval, lookup_sval(TV_SWORD, "Dagger"));
	require(blade->to_h >= 3 && blade->to_h <= 12);
	require(blade->to_d >= 3 && blade->to_d <= 12);

	ok;
}

const char *suite_name = "player/patron-rewards";
struct test tests[] = {
	{ "every-reward-is-zangbands", test_every_reward_is_zangbands },
	{ "an-angry-lord-can-lay-the-ancient-curse",
	  test_an_angry_lord_can_lay_the_ancient_curse },
	{ "experience-is-a-fraction-of-yours",
	  test_experience_is_a_fraction_of_yours },
	{ "ruin-is-permanent-and-wrath-drains",
	  test_ruin_is_permanent_and_wrath_drains },
	{ "a-gift-of-a-stat-restores-it", test_a_gift_of_a_stat_restores_it },
	{ "a-wound-is-polymorphed", test_a_wound_is_polymorphed },
	{ "one-in-decides-the-next-effect-only",
	  test_one_in_decides_the_next_effect_only },
	{ "a-patrons-banishment-is-free", test_a_patrons_banishment_is_free },
	{ "a-lords-horde-arrives", test_a_lords_horde_arrives },
	{ "the-worthy-blade-is-chaotic", test_the_worthy_blade_is_chaotic },
	{ NULL, NULL }
};
