/* ui/menu-labels
 *
 * The bound on a dynamic menu's shortcut keys (ZangbandTK).
 *
 * `menu_dynamic_add_label()` writes the row's shortcut key into the caller's
 * label buffer at the row's own index, and for a long time it did so without
 * knowing how long that buffer was. Every caller in the tree allocated it with
 * `string_make(lower_case)` -- twenty-six letters and a terminator -- so the
 * twenty-seventh row overwrote the terminator and the twenty-eighth ran off the
 * end of the allocation.
 *
 * No menu in Angband is long enough to reach it. The largest is eleven rows and
 * every row of every fixed list supplies its own key, so the letters the buffer
 * was pre-filled with are entirely overwritten and its length never matters.
 * It became reachable in this game, where a menu can be as long as the number
 * of places a character has travelled to (up to 48) or the number of mutations
 * they are carrying (up to 89).
 *
 * These tests are built against the two ways the fix could be wrong: a bound
 * that is off by one, and a bound that drops rows rather than dropping their
 * keys. The second is the more dangerous -- a shortened list is a list that
 * does not contain the thing the player came for, and nothing says so.
 */

#include "unit-test.h"

#include "ui-menu.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	return 0;
}

/**
 * The buffer is as long as it says it is, and holds no 'q'.
 *
 * Fifty-one keys, not fifty-two: these menus conventionally end with a "no
 * thank you" row keyed 'q', and the selection scan takes the first match in
 * the string -- so a data row handed 'q' is chosen by the key meant to decline.
 * That was a real defect in the magetower, where picking the seventeenth
 * destination declined the journey instead of taking it.
 */
static int test_the_label_buffer_says_how_long_it_is(void *state) {
	struct menu *m = menu_dynamic_new();
	char *labels;
	size_t i;

	notnull(m);
	labels = menu_dynamic_labels(m);
	notnull(labels);

	/* Fifty-two letters less 'q'. */
	eq(m->selections_size, 51);
	eq(strlen(labels), 51);
	require(m->selections == labels);

	for (i = 0; i < strlen(labels); i++) require(labels[i] != 'q');

	/* And no duplicates, or two rows would answer to one key. */
	for (i = 0; i < strlen(labels); i++) {
		require(strchr(labels + i + 1, labels[i]) == NULL);
	}

	mem_free(labels);
	menu_free(m);

	ok;
}

/**
 * The fifty-first row gets a key and the fifty-second does not.
 *
 * The off-by-one, asserted from both sides. A bound of `<=` would write one
 * past the last letter, over the terminator -- which is the original defect
 * moved by twenty-five places rather than fixed.
 *
 * Reading `labels[51]` is reading the terminator, which has to still be there.
 */
static int test_the_bound_is_where_it_should_be(void *state) {
	struct menu *m = menu_dynamic_new();
	char *labels;
	int i;

	notnull(m);
	labels = menu_dynamic_labels(m);

	/* Fifty rows, then the fifty-first, then one too many. */
	for (i = 0; i < 50; i++) {
		menu_dynamic_add_label(m, "row", 'Z', i, labels);
	}
	eq(labels[49], 'Z');

	menu_dynamic_add_label(m, "the last one with a key", 'Y', 50, labels);
	eq(labels[50], 'Y');

	menu_dynamic_add_label(m, "one too many", 'X', 51, labels);
	eq(labels[51], '\0');
	eq(strlen(labels), 51);

	mem_free(labels);
	menu_free(m);

	ok;
}

/**
 * A row past the bound is still a row.
 *
 * The distinction that matters. Refusing the key is right; refusing the row is
 * not -- a player with sixty mutations must still be able to see and choose the
 * sixtieth, with the cursor rather than a letter. A fix that returned early
 * would pass the bounds test above and silently shorten every long menu.
 */
static int test_a_row_past_the_bound_is_still_added(void *state) {
	struct menu *m = menu_dynamic_new();
	char *labels;
	int i;

	notnull(m);
	labels = menu_dynamic_labels(m);

	for (i = 0; i < 90; i++) {
		menu_dynamic_add_label(m, "a mutation", 'A', i, labels);
	}

	eq(m->count, 90);
	eq(strlen(labels), 51);

	mem_free(labels);
	menu_free(m);

	ok;
}

/**
 * A menu that never asked for a label buffer cannot be written into.
 *
 * `selections_size` is zero until `menu_dynamic_labels()` sets it, so the
 * guard refuses every write. This is what makes the fix hold for a caller
 * that has not been written yet: the unsafe thing is now the thing that does
 * nothing, rather than the thing that corrupts the heap.
 */
static int test_a_menu_with_no_labels_writes_nothing(void *state) {
	struct menu *m = menu_dynamic_new();
	char buffer[8];
	int i;

	notnull(m);
	eq(m->selections_size, 0);

	memset(buffer, '.', sizeof(buffer));
	m->selections = buffer;

	for (i = 0; i < 4; i++) {
		menu_dynamic_add_label(m, "row", 'Z', i, buffer);
	}

	eq(m->count, 4);
	for (i = 0; i < (int) sizeof(buffer); i++) eq(buffer[i], '.');

	m->selections = NULL;
	menu_free(m);

	ok;
}

const char *suite_name = "ui/menu-labels";
struct test tests[] = {
	{ "the-label-buffer-says-how-long-it-is",
	  test_the_label_buffer_says_how_long_it_is },
	{ "the-bound-is-where-it-should-be",
	  test_the_bound_is_where_it_should_be },
	{ "a-row-past-the-bound-is-still-added",
	  test_a_row_past_the_bound_is_still_added },
	{ "a-menu-with-no-labels-writes-nothing",
	  test_a_menu_with_no_labels_writes_nothing },
	{ NULL, NULL }
};
