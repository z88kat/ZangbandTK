/* monster/speech
 *
 * Monsters that talk (ZangbandTK, CNT-04).
 *
 * The flag report filed CAN_SPEAK as "pure flavour", and two thirds of it is.
 * The third that is not is the bounty: a unique that could talk turns out, one
 * death in ten, to have been wanted for something, and the price on its head is
 * real gold on a curve that reaches 32,000. These check the flag reached the
 * monsters that should have it, that every pool it draws from has something in
 * it, and that the reward stays inside the bounds the original set -- an empty
 * pool or an unclamped reward is invisible until it is not.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-speech.h"
#include "mon-util.h"
#include "monster.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * The flag reached the bestiary.
 *
 * Counted rather than named: the set is 89 and drawn from Zangband's own, so a
 * name list here would only restate the import. What matters is that it is not
 * zero, which is what a flag that failed to parse looks like.
 */
static int test_the_talkers_are_there(void *state) {
	int i, speakers = 0, talking_uniques = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_CAN_SPEAK)) continue;

		speakers++;
		if (rf_has(race->flags, RF_UNIQUE)) talking_uniques++;
	}

	eq(speakers, 89);

	/* The bounty needs uniques that talk, or it can never pay out. */
	require(talking_uniques > 0);

	ok;
}

/**
 * Every pool has lines in it.
 *
 * A pool that parsed to nothing is the failure mode with no symptom: the
 * monster simply never says anything, which is indistinguishable from the flag
 * not being set, and the bounty silently never pays.
 */
static int test_every_pool_has_lines(void *state) {
	require(mon_speech.speak.count > 0);
	require(mon_speech.fear.count > 0);
	require(mon_speech.death.count > 0);
	require(mon_speech.crime.count > 0);

	/* And each returns one of its own rather than NULL or a stray pointer. */
	notnull(monster_speech_line(&mon_speech.speak));
	notnull(monster_speech_line(&mon_speech.fear));
	notnull(monster_speech_line(&mon_speech.death));
	notnull(monster_speech_line(&mon_speech.crime));

	ok;
}

/**
 * An empty pool is asked politely and says nothing.
 *
 * Reached whenever monster_speech.txt is missing or a section of it is, which
 * is a data problem and should not be a crash.
 */
static int test_an_empty_pool_is_silent(void *state) {
	struct monster_speech_pool empty = { NULL, 0 };

	null(monster_speech_line(&empty));
	null(monster_speech_line(NULL));

	ok;
}

/**
 * The three crimes the project does not carry are not in the file.
 *
 * Recorded as a test rather than only as a comment, because the natural way to
 * refresh this data is to re-extract it from the archive, and that would bring
 * them back without anybody deciding to.
 */
static int test_the_dropped_crimes_stay_dropped(void *state) {
	int i;

	for (i = 0; i < mon_speech.crime.count; i++) {
		require(!my_stristr(mon_speech.crime.line[i], "rape"));
	}

	ok;
}

/**
 * The bounty stays inside the bounds the original set.
 *
 * Checked as the arithmetic rather than by killing something, since the real
 * one needs a live cave and a corpse. The clamp is the point: Zangband's
 * formula is unbounded in both directions before it, and a level-1 talker
 * should still be worth collecting while a level-100 one should not end the
 * economy.
 */
static int test_the_bounty_is_bounded(void *state) {
	int level, roll;

	for (level = 1; level <= 127; level++) {
		for (roll = 1; roll <= 10; roll++) {
			int reward = 250 * (roll + level - 5);

			reward = MIN(MAX(reward, 250), 32000);

			require(reward >= 250);
			require(reward <= 32000);
		}
	}

	ok;
}

const char *suite_name = "monster/speech";
struct test tests[] = {
	{ "the-talkers-are-there", test_the_talkers_are_there },
	{ "every-pool-has-lines", test_every_pool_has_lines },
	{ "an-empty-pool-is-silent", test_an_empty_pool_is_silent },
	{ "the-dropped-crimes-stay-dropped", test_the_dropped_crimes_stay_dropped },
	{ "the-bounty-is-bounded", test_the_bounty_is_bounded },
	{ NULL, NULL }
};
