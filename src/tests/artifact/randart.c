/* artifact/randart
 *
 * The random artifact set is designed in place over the standard one, entry
 * by entry.  4.2's designer skipped its fixed artifacts with a test that went
 * on looking at the first one's kind, so the first quest artifact skipped all
 * the rest, and the next pass designed past the end of a_info.  Angband's two
 * quest artifacts are its last two, so nobody noticed; Zangband's artifacts
 * come after them here.  The spare entries made artifacts live in (DEC-170)
 * are zeroed, which turned that overrun into a crash on the first randart
 * game.  These tests pin the set to the table.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "init.h"
#include "obj-init.h"
#include "obj-randart.h"
#include "obj-util.h"
#include "object.h"
#include "parser.h"
#include "player.h"
#include "player-birth.h"
#include "z-rand.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Warrior", "Tester")) {
		cleanup_angband();
		return 1;
	}
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** The standard names, by index, before a set is designed over them. */
static char **standard_names(void) {
	char **names = mem_zalloc(z_info->a_max * sizeof(*names));
	int i;

	for (i = 1; i < z_info->a_max; i++) {
		names[i] = string_make(a_info[i].name);
	}
	return names;
}

static void free_names(char **names, int count) {
	int i;

	for (i = 0; i < count; i++) string_free(names[i]);
	mem_free(names);
}

/**
 * Put the standard set back, as birth does before it designs one.  Each test
 * starts with this too, so one that fails part way leaves the next a clean
 * table.
 */
static bool restore_standard_set(void) {
	cleanup_parser(&artifact_parser);
	return run_parser(&artifact_parser) == PARSE_ERROR_NONE;
}

/** Whether the designer leaves this one alone, by its own test. */
static bool is_fixed(const struct artifact *art) {
	const struct object_kind *kind = lookup_kind(art->tval, art->sval);

	return strstr(art->name, "The One Ring")
		|| (kind && kf_has(kind->kind_flags, KF_QUEST_ART));
}

static int test_the_set_stays_inside_the_table(void *state) {
	uint32_t seeds[] = { 0x0170abcd, 0x00000001, 0x0badf00d };
	size_t s;

	require(restore_standard_set());
	for (s = 0; s < N_ELEMENTS(seeds); s++) {
		int a_max = z_info->a_max;
		const struct artifact *spare = &a_info[a_max];

		null(spare->name);
		do_randart(seeds[s], false);
		eq(z_info->a_max, a_max);
		eq(artifact_made_count(), 0);
		/* The first entry past the set is still empty */
		null(spare->name);
		eq(spare->tval, 0);
		eq(spare->aidx, 0);
		require(restore_standard_set());
	}
	ok;
}

static int test_every_artifact_but_the_fixed_ones_is_designed(void *state) {
	char **before;
	int count, fixed = 0, designed = 0, last_fixed = 0, last;
	int i;

	require(restore_standard_set());
	before = standard_names();
	count = z_info->a_max;
	last = count - 1;

	for (i = 1; i < count; i++) {
		if (!is_fixed(&a_info[i])) continue;
		fixed++;
		last_fixed = i;
	}
	/*
	 * The One Ring, Grond and Morgoth's crown, and Zangband's crown of
	 * Chaos, which is made on the same quest kind as Morgoth's
	 */
	eq(fixed, 4);
	/* Zangband's artifacts come after Angband's, the quest ones included */
	require(last_fixed < last - 1);

	do_randart(0x0170abcd, false);
	for (i = 1; i < count; i++) {
		if (streq(a_info[i].name, before[i])) {
			/* Only the fixed ones keep their names */
			require(streq(before[i], "'The One Ring'")
				|| streq(before[i], "'Grond'")
				|| streq(before[i], "of Morgoth")
				|| streq(before[i], "of Chaos"));
		} else {
			designed++;
		}
	}
	eq(designed, count - 1 - fixed);
	/* The last one too, which 4.2's second pass stopped short of */
	require(!streq(a_info[last].name, before[last]));

	free_names(before, count);
	require(restore_standard_set());
	ok;
}

static int test_the_same_seed_designs_the_same_set(void *state) {
	char **first;
	int count;
	int i;

	require(restore_standard_set());
	count = z_info->a_max;
	do_randart(0x00c0ffee, false);
	first = standard_names();
	require(restore_standard_set());
	do_randart(0x00c0ffee, false);
	for (i = 1; i < count; i++) {
		require(streq(a_info[i].name, first[i]));
	}
	free_names(first, count);
	require(restore_standard_set());
	ok;
}

const char *suite_name = "artifact/randart";
struct test tests[] = {
	{ "the-set-stays-inside-the-table", test_the_set_stays_inside_the_table },
	{ "every-artifact-but-the-fixed-ones-is-designed",
		test_every_artifact_but_the_fixed_ones_is_designed },
	{ "the-same-seed-designs-the-same-set",
		test_the_same_seed_designs_the_same_set },
	{ NULL, NULL }
};
