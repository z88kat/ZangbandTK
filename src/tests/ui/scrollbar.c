/* ui/scrollbar — the scroll thumb on a menu too long for its region
 *
 * The birth menus draw twenty races into a region sized by a constant that
 * ignored the terminal, so five were written to rows the terminal does not
 * have and `Term_gotoxy()` discarded them. The region is derived from the
 * screen now and the list scrolls, which raises the question the project owner
 * asked: how does a player know it is scrolling?
 *
 * A proportional thumb answers both halves -- that there is more, and roughly
 * where in it you are. This pins the arithmetic. The drawing itself is one
 * `Term_putstr` per row and can only be checked by looking.
 */
#include "unit-test.h"

#include "ui-menu.h"

int setup_tests(void **state) { *state = NULL; return 0; }
int teardown_tests(void *state) { return 0; }

/**
 * A list that fits shows nothing at all.
 *
 * The floor: a menu that never scrolls must look exactly as it did, or every
 * short menu in the game grows a decoration it does not need.
 */
static int test_a_list_that_fits_has_no_thumb(void *state) {
	int top, len;

	require(!menu_scroll_thumb(10, 14, 0, &top, &len));
	eq(len, 0);

	/* Exactly full is still not scrolling */
	require(!menu_scroll_thumb(14, 14, 0, &top, &len));
	eq(len, 0);

	/* And a degenerate region does not divide by zero */
	require(!menu_scroll_thumb(10, 0, 0, &top, &len));

	ok;
}

/**
 * The thumb is shorter than the track, and shrinks as the list grows.
 *
 * If it filled the track it would say "this all fits" while the list scrolled,
 * which is worse than drawing nothing.
 */
static int test_the_thumb_shrinks_as_the_list_grows(void *state) {
	int top, len20, len32, len200;

	require(menu_scroll_thumb(20, 14, 0, &top, &len20));
	require(menu_scroll_thumb(32, 14, 0, &top, &len32));
	require(menu_scroll_thumb(200, 14, 0, &top, &len200));

	require(len20 < 14);
	require(len32 < len20);
	require(len200 < len32);

	/* Never vanishes, however long the list */
	require(len200 >= 1);

	ok;
}

/**
 * It reaches both ends, and only at the ends.
 *
 * The property a player actually reads: the thumb at the bottom means there is
 * nothing below. Proportional arithmetic alone leaves a gap at the bottom for
 * most list lengths, so this is pinned rather than assumed.
 */
static int test_the_thumb_reaches_both_ends(void *state) {
	int rows = 14, n, top, len, pos;

	for (n = rows + 1; n <= 64; n++) {
		int last = n - rows;

		/* At the top */
		require(menu_scroll_thumb(n, rows, 0, &pos, &len));
		eq(pos, 0);

		/* At the bottom, flush with the end of the track */
		require(menu_scroll_thumb(n, rows, last, &pos, &len));
		eq(pos + len, rows);

		/* One row in from the top is not the top any more */
		if (last > 1) {
			require(menu_scroll_thumb(n, rows, 1, &top, &len));
			require(top >= 0);
			require(top + len <= rows);
		}
	}

	ok;
}

/**
 * It never runs off the track, at any position of any list.
 *
 * The bug this guards is a thumb drawn past the region and into whatever is
 * beside it -- which for the race menu is the class menu, three columns over.
 */
static int test_the_thumb_stays_inside_the_track(void *state) {
	int rows, n, top, pos, len;

	for (rows = 1; rows <= 24; rows++) {
		for (n = rows + 1; n <= 80; n++) {
			for (top = 0; top <= n - rows; top++) {
				require(menu_scroll_thumb(n, rows, top, &pos, &len));
				require(len >= 1);
				require(len <= rows);
				require(pos >= 0);
				require(pos + len <= rows);
			}
		}
	}

	ok;
}

/**
 * It moves downwards as you scroll, and never backwards.
 */
static int test_the_thumb_only_moves_down(void *state) {
	int rows = 14, n = 32, top, pos, len, prev = -1;

	for (top = 0; top <= n - rows; top++) {
		require(menu_scroll_thumb(n, rows, top, &pos, &len));
		require(pos >= prev);
		prev = pos;
	}

	ok;
}

/**
 * The letters the status line offers are the letters on the screen.
 *
 * The birth menus tag by absolute position, so which letters are on show moves
 * with the list: naming a fixed "a-y" would send a player to a row that is no
 * longer there. Thirty-four races in a twenty-one row region, at the top of the
 * list and at the bottom of it.
 */
static int test_the_tag_range_follows_the_list(void *state) {
	char first, last;

	require(menu_tag_range(all_letters_nohjkl, 34, 0, 21, &first, &last));
	eq(first, 'a');
	eq(last, 'y');

	require(menu_tag_range(all_letters_nohjkl, 34, 13, 21, &first, &last));
	eq(first, 'r');
	eq(last, 'L');

	/* A list shorter than its region ends at the last row it has */
	require(menu_tag_range(all_letters_nohjkl, 5, 0, 21, &first, &last));
	eq(first, 'a');
	eq(last, 'e');

	ok;
}

/**
 * It stops at the end of the alphabet, not the end of the list.
 *
 * `selections` is a fixed string and the caller passes a count that has nothing
 * to do with its length. A list longer than the letters available must name the
 * last letter there is rather than read past the terminator to find one -- and
 * a row with no letter cannot be picked directly whatever the line says.
 */
static int test_the_tag_range_stays_inside_the_alphabet(void *state) {
	int len = (int)strlen(all_letters_nohjkl);
	char first, last;
	int rows, n, top;

	require(menu_tag_range(all_letters_nohjkl, 200, 0, 200, &first, &last));
	eq(first, 'a');
	eq(last, all_letters_nohjkl[len - 1]);

	/* Scrolled past the letters entirely, there is nothing to offer */
	require(!menu_tag_range(all_letters_nohjkl, 200, len, 20, &first, &last));

	for (rows = 1; rows <= 24; rows++) {
		for (n = 1; n <= 200; n++) {
			for (top = 0; top < n; top++) {
				if (!menu_tag_range(all_letters_nohjkl, n, top, rows,
						&first, &last)) {
					continue;
				}
				require(first != 0);
				require(last != 0);
				require(strchr(all_letters_nohjkl, first) != NULL);
				require(strchr(all_letters_nohjkl, last) != NULL);
			}
		}
	}

	ok;
}

/**
 * One row is not a range, and no list has no letters.
 *
 * The status line reads "a-y to pick directly"; with one row on show that
 * would be "a-a", which says less than nothing. The caller drops the clause,
 * so this has to be the answer it gets.
 */
static int test_a_single_row_is_not_a_range(void *state) {
	char first, last;

	require(!menu_tag_range(all_letters_nohjkl, 1, 0, 21, &first, &last));
	require(!menu_tag_range(all_letters_nohjkl, 34, 0, 1, &first, &last));
	require(!menu_tag_range(NULL, 34, 0, 21, &first, &last));
	require(!menu_tag_range(all_letters_nohjkl, 0, 0, 21, &first, &last));
	require(!menu_tag_range(all_letters_nohjkl, 34, 0, 0, &first, &last));

	ok;
}

const char *suite_name = "ui/scrollbar";
struct test tests[] = {
	{ "a-list-that-fits-has-no-thumb", test_a_list_that_fits_has_no_thumb },
	{ "the-thumb-shrinks-as-the-list-grows",
	  test_the_thumb_shrinks_as_the_list_grows },
	{ "the-thumb-reaches-both-ends", test_the_thumb_reaches_both_ends },
	{ "the-thumb-stays-inside-the-track",
	  test_the_thumb_stays_inside_the_track },
	{ "the-thumb-only-moves-down", test_the_thumb_only_moves_down },
	{ "the-tag-range-follows-the-list", test_the_tag_range_follows_the_list },
	{ "the-tag-range-stays-inside-the-alphabet",
	  test_the_tag_range_stays_inside_the_alphabet },
	{ "a-single-row-is-not-a-range", test_a_single_row_is_not_a_range },
	{ NULL, NULL }
};
