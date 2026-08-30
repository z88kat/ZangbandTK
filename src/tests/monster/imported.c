/* monster/imported
 *
 * That the imported bestiary is not Angband's wearing new names (ZangbandTK,
 * CNT-01).
 *
 * CNT-11 found that Zangband renamed 26 of the object kinds it inherited from
 * Angband 2.8.1 without changing what they are, and that ten of those were
 * being imported as new content beside the Angband objects they duplicate.
 * Objects have a structural key -- a kind is its (tval, sval) slot -- so the
 * renames could be found by comparing slots.
 *
 * Monsters have no such key. Zangband renumbered the whole bestiary: of 548
 * record indices shared with 2.8.1, three carry the same monster. What does
 * survive renaming is the *description*, which is hand-written prose, and 33
 * Zangband monsters keep a 2.8.1 monster's paragraph word for word. Most of
 * those are Zangband writing a much deeper monster on inherited text -- its
 * Balrog is level 63 where 2.8.1's Lesser Balrog is 44 -- or 2.8.1 boilerplate
 * that several monsters already shared. Two were real renames.
 *
 * A shared description is therefore evidence and not proof, which is why the
 * general test below asks for the description *and* the numbers together.
 */

#include "unit-test.h"

#include "init.h"
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
 * The two renamed townsfolk are gone, and Angband's own are still here.
 *
 * Zangband's "hobo" is 2.8.1's boil-covered wretch and its "raving lunatic" is
 * the village idiot: same glyph, colour, speed, armour, depth and experience,
 * and the same description word for word. Angband still ships both under the
 * older names, so importing them put two of each in the game.
 *
 * Both halves are asserted. Dropping the rename from `renames.toml` brings the
 * duplicate back and fails the first half; a conversion that removed the wrong
 * one of the pair fails the second.
 */
static int test_the_renamed_townsfolk_are_gone(void *state) {
	null(lookup_monster("hobo"));
	null(lookup_monster("raving lunatic"));

	notnull(lookup_monster("boil-covered wretch"));
	notnull(lookup_monster("village idiot"));

	ok;
}

/**
 * No imported monster is an Angband monster under another name.
 *
 * The general form, and the check that would have caught the two above without
 * anyone having to notice them. A monster is taken to be the same monster when
 * it shares a glyph, a colour, a speed, a depth *and* a description: the prose
 * alone is not enough, because Angband gives all five mushroom patches one
 * sentence between them, and the numbers alone are not enough, because two
 * level-0 townsfolk look alike by construction.
 *
 * **Hit points and armour class are deliberately not compared**, and the first
 * version of this test failed for including them. BAL-13 scales Angband's own
 * monsters to Zangband's lethality and excludes the imported ones, which
 * already carry Zangband's numbers (mon-init.c:1889). A duplicated monster
 * therefore arrives with Angband's raw figures on one side and 73% of them on
 * the other, and comparing those two fields guarantees the duplicate is missed
 * -- the test passed with `hobo` sitting next to the boil-covered wretch it
 * copies.
 *
 * The comparison runs across the whole bestiary rather than only between the
 * two halves of it, so it catches a duplicate wherever it comes from. Two
 * pairs are exempt by name because both games ship deliberate twins; they are
 * named in the loop with the reason.
 */
static int test_no_import_repeats_an_angband_monster(void *state) {
	int i, j, repeats = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *a = &r_info[i];

		if (!a->name || !a->text) continue;

		for (j = 0; j < z_info->r_max; j++) {
			struct monster_race *b = &r_info[j];

			if (i == j || !b->name || !b->text) continue;
			if (a->d_char != b->d_char || a->d_attr != b->d_attr) continue;
			if (a->speed != b->speed || a->level != b->level) continue;
			if (!streq(a->text, b->text)) continue;

			/*
			 * Two pairs are deliberate twins rather than accidents, and
			 * each belongs to the game that shipped it.
			 *
			 * Grip and Fang are Angband's own: identical dogs with one
			 * paragraph between them. The sparrow and the chaffinch are
			 * Zangband's, alike in every number and sentence and told
			 * apart by their colour and the country they live in -- the
			 * sparrow in the towns, the chaffinch in forest, grass and
			 * swamp. Both pairs are content, not duplication.
			 */
			if (my_stristr(a->name, "Farmer Maggot")
					&& my_stristr(b->name, "Farmer Maggot")) {
				continue;
			}
			if ((streq(a->name, "sparrow") || streq(a->name, "chaffinch"))
					&& (streq(b->name, "sparrow")
						|| streq(b->name, "chaffinch"))) {
				continue;
			}

			repeats++;
		}
	}

	eq(repeats, 0);

	ok;
}

/**
 * The bestiary is the size the manual says it is.
 *
 * Removing a duplicate changes a number that is printed in four places, and
 * the number going stale is the likeliest thing to happen next. Angband's own
 * 624 plus 387 imported plus two of ZangbandTK's own; the total is what
 * README.md and the manual quote.
 */
static int test_the_bestiary_is_the_size_the_manual_says(void *state) {
	int i, named = 0;

	for (i = 0; i < z_info->r_max; i++) {
		if (r_info[i].name) named++;
	}

	eq(named, 1013);

	ok;
}

const char *suite_name = "monster/imported";
struct test tests[] = {
	{ "the-renamed-townsfolk-are-gone", test_the_renamed_townsfolk_are_gone },
	{ "no-import-repeats-an-angband-monster",
	  test_no_import_repeats_an_angband_monster },
	{ "the-bestiary-is-the-size-the-manual-says",
	  test_the_bestiary_is_the_size_the_manual_says },
	{ NULL, NULL }
};
