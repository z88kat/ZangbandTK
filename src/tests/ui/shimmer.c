/* ui/shimmer
 *
 * What a shimmering monster's tile is rotated by (ZangbandTK).
 *
 * `ui/grafmode` covers the arithmetic. This covers the thing that actually
 * went wrong, twice, which was not the arithmetic but what got fed into it.
 *
 * `mon->attr` looks like the obvious carrier for the rotation: `do_animation()`
 * has just written this frame's colour into it, and in text mode that is
 * exactly what gets drawn. But `grid_data_as_text()` writes the *drawing*
 * attribute back into that same field a few lines further down, so from the
 * second frame onward it holds a tile row rather than a step. The monster then
 * settles on one wrong hue and stays there -- no error, no shimmer, and a
 * colour stable enough to look deliberate.
 *
 * So the test is not "does it rotate" but "is the rotation independent of a
 * field that this function itself overwrites". Corrupting `mon->attr` between
 * two draws must change nothing.
 *
 * The second test is the other half of the same bargain: the shape must never
 * move. Colour is the row and shape is the column, so a rotation that touched
 * the column would silently redraw the monster as some other creature.
 */

#include "unit-test.h"

#include "cave.h"
/* ui-map.h uses game_event_type without including it. */
#include "game-event.h"
#include "grafmode.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "game-world.h"
#include "generate.h"
#include "player-birth.h"
#include "player-util.h"
#include "test-utils.h"
#include "ui-map.h"
#include "ui-prefs.h"

/* Matches lib/tiles/list.txt's declaration for the Neon set. */
#define HUES  10
#define TONES 4
#define SPAN  9

/* A row in the middle of the sheet, on a hue inside the rainbow. */
#define TEST_HUE  3
#define TEST_TONE 1
#define TEST_ROW  (TEST_HUE * TONES + TEST_TONE)
#define TEST_COL  17

static graphics_mode cycling_mode;
static graphics_mode *saved_mode;

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	if (!player_make_simple(NULL, "Warrior", "Tester")) return 1;
	/* grid_data_as_text() reads the global cave, so there has to be one. */
	prepare_next_level(player);
	on_new_level();
	/* The per-race attr/char tables are the front end's, not init_angband's. */
	textui_prefs_init();

	saved_mode = current_graphics_mode;
	memset(&cycling_mode, 0, sizeof(cycling_mode));
	cycling_mode.grafID = 200;
	cycling_mode.cycleHues = HUES;
	cycling_mode.cycleTones = TONES;
	cycling_mode.cycleSpan = SPAN;
	current_graphics_mode = &cycling_mode;

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	current_graphics_mode = saved_mode;
	if (cave) wipe_mon_list(cave, player);
	textui_prefs_free();
	cleanup_angband();
	return 0;
}

/**
 * Put a shimmering monster on an empty level with a tile assigned, and hand
 * back the grid description the map would build for it.
 */
static struct monster *shimmering_monster(struct grid_data *g) {
	/* Multi-hued, and every version of the game has had one. */
	struct monster *mon = t_add_monster(cave, loc(5, 5),
										"baby multi-hued dragon");

	/* As the Neon set's preference file would leave it. */
	monster_x_attr[mon->race->ridx] = 0x80 | TEST_ROW;
	monster_x_char[mon->race->ridx] = (wchar_t) (0x80 | TEST_COL);

	memset(g, 0, sizeof(*g));
	g->m_idx = mon->midx;
	g->f_idx = FEAT_FLOOR;
	g->lighting = LIGHTING_LIT;
	g->in_view = true;

	return mon;
}

/**
 * The colour drawn does not depend on mon->attr, which this function
 * overwrites.
 */
static int test_the_rotation_ignores_the_field_it_overwrites(void *state) {
	struct grid_data g;
	struct monster *mon = shimmering_monster(&g);
	int a = 0, tap = 0;
	wchar_t ch = 0, tcp = 0;
	int first;

	notnull(mon);
	require(rf_has(mon->race->flags, RF_ATTR_MULTI));

	grid_data_as_text(&g, &a, &ch, &tap, &tcp);
	first = a;
	require(first & 0x80);

	/* Whatever the previous draw left behind must not steer the next one. */
	mon->attr = 0;
	a = 0;
	grid_data_as_text(&g, &a, &ch, &tap, &tcp);
	eq(a, first);

	mon->attr = 0x80 | 0x27;
	a = 0;
	grid_data_as_text(&g, &a, &ch, &tap, &tcp);
	eq(a, first);

	mon->attr = 7;
	a = 0;
	grid_data_as_text(&g, &a, &ch, &tap, &tcp);
	eq(a, first);

	ok;
}

/**
 * The shape never moves, and the tone never moves.
 *
 * Colour is the row and shape is the column. A rotation that reached the
 * column would draw a different creature; one that reached the tone would take
 * a lit monster into the tones meant for remembered terrain.
 */
static int test_only_the_hue_moves(void *state) {
	struct grid_data g;
	int a = 0, tap = 0;
	wchar_t ch = 0, tcp = 0;
	int row;

	notnull(shimmering_monster(&g));

	grid_data_as_text(&g, &a, &ch, &tap, &tcp);

	/* The column is the shape and is untouched. */
	eq((int) (ch & 0x7f), TEST_COL);

	row = a & 0x7f;
	/* Same block, same tone, and a hue inside the rainbow. */
	eq(row / (HUES * TONES), TEST_ROW / (HUES * TONES));
	eq(row % TONES, TEST_TONE);
	require((row % (HUES * TONES)) / TONES < SPAN);

	ok;
}

/**
 * A tileset that declares no layout draws the monster exactly as its
 * preference file says, which is what every other set in the game relies on.
 */
static int test_a_set_that_does_not_cycle_is_untouched(void *state) {
	struct grid_data g;
	graphics_mode plain;
	int a = 0, tap = 0;
	wchar_t ch = 0, tcp = 0;

	notnull(shimmering_monster(&g));

	memset(&plain, 0, sizeof(plain));
	plain.grafID = 201;
	current_graphics_mode = &plain;

	grid_data_as_text(&g, &a, &ch, &tap, &tcp);
	eq(a, 0x80 | TEST_ROW);
	eq((int) (ch & 0x7f), TEST_COL);

	current_graphics_mode = &cycling_mode;
	ok;
}

const char *suite_name = "ui/shimmer";
struct test tests[] = {
	{ "the-rotation-ignores-the-field-it-overwrites",
	  test_the_rotation_ignores_the_field_it_overwrites },
	{ "only-the-hue-moves", test_only_the_hue_moves },
	{ "a-set-that-does-not-cycle-is-untouched",
	  test_a_set_that_does_not_cycle_is_untouched },
	{ NULL, NULL }
};
