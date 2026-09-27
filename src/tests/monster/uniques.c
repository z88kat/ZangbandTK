/* monster/uniques
 *
 * What a unique monster cannot be made to do (ZangbandTK, DEC-107).
 *
 * Zangband gates sleep, slow, confusion and fear on `RF_UNIQUE` before any
 * other test, so a unique is never subject to them however strong the effect
 * or however high the character's level. 4.2 replaced those flat immunities
 * with a graded saving throw -- a unique gets a second roll and nothing more
 * -- and the port inherited 4.2's model without anybody choosing it. The
 * project owner ruled for Zangband's.
 *
 * **"Immune" and "resisted" look identical from one attempt**, which is the
 * whole difficulty in testing this: a unique that saved and a unique that was
 * never eligible both come back unaffected. So every test here measures across
 * many attempts and pairs the unique with a *control* -- a non-unique of the
 * same depth, carrying none of the resist flags either -- driven through the
 * same call in the same loop. Immunity is "the unique never, the control
 * often". A build with no gate at all shows both often; a build that broke the
 * effect outright shows both never; only the real thing separates them.
 *
 * Bullroarer the Hobbit and the large kobold are both depth 5 and neither has
 * NO_SLEEP, NO_CONF, NO_FEAR or NO_SLOW -- which matters, because most low
 * uniques do have some of them and would be immune for the wrong reason.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "mon-timed.h"
#include "monster.h"
#include "player-birth.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Warrior", "Tester")) return 1;
	player->depth = 1;
	prepare_next_level(player);
	on_new_level();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Everything off the level, so a placement has room. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/**
 * That race, somewhere near the player, or NULL.
 *
 * Regenerates rather than retrying on the same level: a level whose free grids
 * are all taken is not a level a retry will fix, and this suite has no opinion
 * about which level it runs on.
 */
static struct monster *summon(const char *name) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int attempt;

	if (!race) return NULL;

	for (attempt = 0; attempt < 20; attempt++) {
		struct loc grid;

		clear_the_level();

		if (scatter_ext(cave, &grid, 1, player->grid, 8, true,
						square_isempty) > 0
				&& place_new_monster(cave, grid, race, false, false, info,
									 ORIGIN_DROP)) {
			return square_monster(cave, grid);
		}

		prepare_next_level(player);
		on_new_level();
	}

	return NULL;
}

/**
 * How many of `tries` attempts left a mark on this race.
 *
 * The timer is deliberately large. The saving throw is
 * `MIN(90, level + MAX(0, 25 - timer/2))`, so a big timer removes the second
 * term and leaves the monster's own level -- five, for both of these -- which
 * is the *weakest* the graded model ever is. If flat immunity were quietly not
 * firing, this is the setting that would show it most loudly.
 */
static int marks(const char *name, int effect, int tries)
{
	int i, hit = 0;

	for (i = 0; i < tries; i++) {
		struct monster *mon = summon(name);

		if (!mon) return -1;

		mon_clear_timed(mon, effect, MON_TMD_FLG_NOMESSAGE);
		mon_inc_timed(mon, effect, 200, MON_TMD_FLG_NOMESSAGE);
		if (mon->m_timed[effect]) hit++;
	}

	return hit;
}

/** One effect, the unique against its control. */
static bool immune_to(const char *what, int effect, int tries)
{
	int uniq = marks("Bullroarer the Hobbit", effect, tries);
	int control = marks("large kobold", effect, tries);

	if (uniq < 0 || control < 0) {
		printf("  %s: could not place a monster; the test did not run\n", what);
		return false;
	}

	printf("  %s: unique %d/%d, control %d/%d\n", what, uniq, tries,
		   control, tries);

	/* The control proves the effect works at all and the loop is honest. */
	if (control < tries / 2) {
		printf("  %s: the control was only affected %d times in %d, so "
			   "nothing here is measuring immunity\n", what, control, tries);
		return false;
	}

	/* And the unique is never touched. Once is once too many. */
	if (uniq != 0) {
		printf("  %s: the unique was affected %d times in %d\n", what, uniq,
			   tries);
		return false;
	}

	return true;
}

static int test_a_unique_cannot_be_slept(void *state) {
	require(immune_to("sleep", MON_TMD_SLEEP, 40));
	ok;
}

static int test_a_unique_cannot_be_slowed(void *state) {
	require(immune_to("slow", MON_TMD_SLOW, 40));
	ok;
}

static int test_a_unique_cannot_be_confused(void *state) {
	require(immune_to("confusion", MON_TMD_CONF, 40));
	ok;
}

static int test_a_unique_cannot_be_frightened(void *state) {
	require(immune_to("fear", MON_TMD_FEAR, 40));
	ok;
}

/**
 * But a unique still loses its nerve when it is hurt.
 *
 * The immunity is to the *effect*, not to morale. `monster_scared_by_damage()`
 * passes `MON_TMD_FLG_NOFAIL`, and the gate sits after the check for it
 * precisely so that path still works -- uniques flee when badly wounded in
 * both games, and a gate placed one line earlier would have stopped that
 * silently. Nothing else in the tree passes `NOFAIL`, so this is the whole of
 * the exception.
 */
static int test_a_hurt_unique_still_flees(void *state) {
	struct monster *mon = summon("Bullroarer the Hobbit");

	notnull(mon);
	require(monster_is_unique(mon));

	mon_clear_timed(mon, MON_TMD_FEAR, MON_TMD_FLG_NOMESSAGE);
	mon_inc_timed(mon, MON_TMD_FEAR, 50,
				  MON_TMD_FLG_NOMESSAGE | MON_TMD_FLG_NOFAIL);

	require(mon->m_timed[MON_TMD_FEAR] > 0);
	ok;
}

/**
 * And the effects the archive does not gate are still gradeable on a unique.
 *
 * Zangband's four are sleep, slow, confusion and fear. Stunning is not one of
 * them and neither is holding, so a unique remains subject to both -- which is
 * worth pinning, because "uniques are immune to status" is the obvious wrong
 * generalisation of this change and nothing else would catch it.
 */
static int test_a_unique_can_still_be_stunned(void *state) {
	int hit = marks("Bullroarer the Hobbit", MON_TMD_STUN, 40);

	require(hit > 0);
	printf("  stun: unique %d/40, which is not gated and should not be\n", hit);
	ok;
}

const char *suite_name = "monster/uniques";
struct test tests[] = {
	{ "a-unique-cannot-be-slept", test_a_unique_cannot_be_slept },
	{ "a-unique-cannot-be-slowed", test_a_unique_cannot_be_slowed },
	{ "a-unique-cannot-be-confused", test_a_unique_cannot_be_confused },
	{ "a-unique-cannot-be-frightened", test_a_unique_cannot_be_frightened },
	{ "a-hurt-unique-still-flees", test_a_hurt_unique_still_flees },
	{ "a-unique-can-still-be-stunned", test_a_unique_can_still_be_stunned },
	{ NULL, NULL }
};
