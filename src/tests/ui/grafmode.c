/* ui/grafmode
 *
 * Rotating a tile's colour without changing its picture (ZangbandTK).
 *
 * A tileset ordinarily paints colour into its art, so a monster's colour is
 * settled the moment its tile is chosen. That is why `do_animation()`'s work
 * is thrown away in graphics mode: `ui-map.c` short-circuits on `da & 0x80`
 * and a shimmering monster does not shimmer. Angband has always been that way
 * and none of its tilesets could be otherwise.
 *
 * The Neon set can, because its sheet has the colour on the row and the shape
 * on the column, in blocks of hues x tones -- so the same drawing exists in
 * every colour and moving between them is arithmetic on the row alone. A set
 * says so with `cycle:` in list.txt, and a set that does not say so must be
 * left exactly as it was.
 *
 * These tests are built against the four ways the arithmetic can be wrong, all
 * of them silent. The renderer takes the cell index modulo the sheet size
 * rather than complaining, so a row that walks off the end of a block draws
 * some other picture instead of failing. Rotating the tone as well as the hue
 * takes a bright monster into the tones meant for remembered terrain, where it
 * is nearly invisible. Walking the whole block rather than the declared span
 * takes the rainbow through grey, which reads as the colour failing rather
 * than moving. And a set that declares nothing must come back untouched, or
 * every other tileset in the game changes behaviour.
 *
 * The first test is here for a different reason: it is the only thing that
 * checks list.txt still parses. A malformed `cycle:` line loses the whole
 * mode, and the front end then falls back to ASCII without saying why.
 */

#include "unit-test.h"

#include "grafmode.h"
#include "init.h"
#include "test-utils.h"

/* Hues, tones and span as lib/tiles/list.txt declares them for the Neon set. */
#define NEON_ID    7
#define NEON_HUES  10
#define NEON_TONES 4
#define NEON_SPAN  9

int setup_tests(void **state) {
	set_file_paths();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	close_graphics_modes();
	return 0;
}

/**
 * list.txt parses, the Neon set declares its layout, and nothing else claims
 * one.
 *
 * The second half matters as much as the first. `graf_cycle_attr()` trusts the
 * declaration, so a set that claimed a layout it did not have would rotate a
 * monster's row into a picture of something else entirely.
 */
static int test_the_neon_set_declares_its_layout(void *state) {
	graphics_mode *mode;
	int seen = 0;

	require(init_graphics_modes());

	mode = get_graphics_mode(NEON_ID);
	notnull(mode);
	eq(mode->cycleHues, NEON_HUES);
	eq(mode->cycleTones, NEON_TONES);
	eq(mode->cycleSpan, NEON_SPAN);
	require(mode->cycleSpan <= mode->cycleHues);

	for (mode = graphics_modes; mode; mode = mode->pNext) {
		if (mode->grafID == NEON_ID || mode->grafID == GRAPHICS_NONE) continue;
		eq(mode->cycleHues, 0);
		eq(mode->cycleTones, 0);
		eq(mode->cycleSpan, 0);
		seen++;
	}

	/* The four inherited Angband sets, none of which is laid out this way. */
	eq(seen, 4);

	ok;
}

/* A set laid out the way the Neon one is, without needing its files. */
static graphics_mode *use_layout(graphics_mode *scratch, int hues, int tones,
								 int span) {
	memset(scratch, 0, sizeof(*scratch));
	scratch->grafID = 200;
	scratch->cycleHues = hues;
	scratch->cycleTones = tones;
	scratch->cycleSpan = span;
	current_graphics_mode = scratch;
	return scratch;
}

/**
 * The hue moves and the tone stays where it was.
 */
static int test_a_hue_moves_and_a_tone_does_not(void *state) {
	graphics_mode scratch;
	int tone, hue, step;

	use_layout(&scratch, NEON_HUES, NEON_TONES, NEON_SPAN);

	for (tone = 0; tone < NEON_TONES; tone++) {
		for (hue = 0; hue < NEON_SPAN; hue++) {
			for (step = 0; step < NEON_SPAN; step++) {
				uint8_t in = 0x80 | (hue * NEON_TONES + tone);
				uint8_t out = graf_cycle_attr(in, step);
				int row = out & 0x7f;

				/* Still a tile, still the same tone. */
				require(out & 0x80);
				eq(row % NEON_TONES, tone);
				/* And the hue is exactly where the step put it. */
				eq(row / NEON_TONES, (hue + step) % NEON_SPAN);
			}
		}
	}

	ok;
}

/**
 * The rainbow stops before grey.
 *
 * Grey is the tenth hue and outside the declared span, so a monster whose own
 * colour is grey keeps it. A rotation that ran to `cycleHues` instead would
 * take every shimmering monster through it, and a rainbow that passes through
 * grey looks like the colour failing rather than moving on.
 */
static int test_the_rainbow_stops_before_grey(void *state) {
	graphics_mode scratch;
	int tone, step;

	use_layout(&scratch, NEON_HUES, NEON_TONES, NEON_SPAN);

	for (tone = 0; tone < NEON_TONES; tone++) {
		uint8_t in = 0x80 | (NEON_SPAN * NEON_TONES + tone);

		for (step = 0; step < NEON_HUES * 3; step++) {
			eq(graf_cycle_attr(in, step), in);
		}
	}

	ok;
}

/**
 * Every rotation lands on a row the sheet actually has.
 *
 * The renderer wraps an out-of-range cell with `% pict_rows` rather than
 * complaining, so this going wrong draws a different picture in silence.
 */
static int test_the_result_stays_in_the_sheet(void *state) {
	graphics_mode scratch;
	int row, step;

	use_layout(&scratch, NEON_HUES, NEON_TONES, NEON_SPAN);

	for (row = 0; row < NEON_HUES * NEON_TONES; row++) {
		for (step = -NEON_HUES; step <= NEON_HUES * 2; step++) {
			uint8_t out = graf_cycle_attr(0x80 | row, step);

			require((out & 0x7f) < NEON_HUES * NEON_TONES);
			/* And never out of the block it started in. */
			eq((out & 0x7f) / (NEON_HUES * NEON_TONES),
			   row / (NEON_HUES * NEON_TONES));
		}
	}

	ok;
}

/**
 * A set that declares no layout is left exactly alone, and so is anything
 * that is not a tile.
 *
 * This is the test that protects the other four tilesets: they say nothing in
 * list.txt, so nothing about how they draw may change.
 */
static int test_a_set_that_says_nothing_is_left_alone(void *state) {
	graphics_mode scratch;
	int step;

	/* No declaration at all. */
	use_layout(&scratch, 0, 0, 0);
	require(!graf_cycles());
	for (step = 0; step < 12; step++) {
		eq(graf_cycle_attr(0x80 | 0x1f, step), 0x80 | 0x1f);
	}

	/* A span of one is not a rainbow either. */
	use_layout(&scratch, NEON_HUES, NEON_TONES, 1);
	require(!graf_cycles());
	eq(graf_cycle_attr(0x80 | 0x04, 3), 0x80 | 0x04);

	/* An ASCII attribute is not a tile and has no row to move. */
	use_layout(&scratch, NEON_HUES, NEON_TONES, NEON_SPAN);
	require(graf_cycles());
	for (step = 0; step < 12; step++) {
		eq(graf_cycle_attr(0x0c, step), 0x0c);
	}

	/* And with no mode at all, rather than a mode that declares nothing. */
	current_graphics_mode = NULL;
	require(!graf_cycles());
	eq(graf_cycle_attr(0x80 | 0x04, 3), 0x80 | 0x04);

	ok;
}

const char *suite_name = "ui/grafmode";
struct test tests[] = {
	{ "the-neon-set-declares-its-layout",
	  test_the_neon_set_declares_its_layout },
	{ "a-hue-moves-and-a-tone-does-not",
	  test_a_hue_moves_and_a_tone_does_not },
	{ "the-rainbow-stops-before-grey",
	  test_the_rainbow_stops_before_grey },
	{ "the-result-stays-in-the-sheet",
	  test_the_result_stays_in_the_sheet },
	{ "a-set-that-says-nothing-is-left-alone",
	  test_a_set_that_says_nothing_is_left_alone },
	{ NULL, NULL }
};
