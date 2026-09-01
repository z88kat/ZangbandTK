/* player/virtue
 *
 * What a character is measured against, and what reads it (ZangbandTK,
 * PLR-18 to PLR-21).
 *
 * Zangband left this half-built: 168 places wrote a virtue and none read one,
 * and the screen that would have shown them was commented out. So the failure
 * this suite is built against is not a crash -- it is the feature going quietly
 * inert again. A virtue nothing writes, a virtue no character is ever given, a
 * consumer that reads a counter which never moves: each of those looks exactly
 * like a working system from the outside, which is how the original shipped
 * for seven years with nobody noticing.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "player-util.h"
#include "player-virtue.h"
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

	*state = NULL;
	return 0;
}

/**
 * Point the character at a class and race and choose again.
 *
 * The selection is what is under test, so it is called directly rather than
 * through a birth: `player_make_simple()` builds a whole character and is not
 * meant to be called two hundred times in a row.
 */
static void reselect(const char *race_name, const char *class_name)
{
	struct player_race *r;
	struct player_class *c;

	for (r = races; r; r = r->next) {
		if (streq(r->name, race_name)) player->race = r;
	}
	for (c = classes; c; c = c->next) {
		if (streq(c->name, class_name)) player->class = c;
	}

	virtues_select(player);
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * Every character is measured against eight distinct virtues.
 *
 * Checked across every class against every race -- 240 characters -- because
 * the selection runs class, then race, then realm, then a weighted pad, and
 * the ways it can go wrong are all in the joins. A class and a race that name
 * the same virtue must not spend two slots on it; a class naming four must
 * still leave room; the pad must fill whatever is left and must not repeat
 * what is already there.
 */
static int test_every_character_gets_eight_distinct_virtues(void *state) {
	struct player_class *c;
	struct player_race *r;
	int checked = 0;

	for (c = classes; c; c = c->next) {
		for (r = races; r; r = r->next) {
			int i, j;

			reselect(r->name, c->name);
			checked++;

			for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
				require(player->vir_types[i] > V_NONE);
				require(player->vir_types[i] < V_MAX);
				eq(player->virtues[i], 0);

				for (j = i + 1; j < MAX_PLAYER_VIRTUES; j++) {
					require(player->vir_types[i] != player->vir_types[j]);
				}
			}
		}
	}

	require(checked >= 200);

	ok;
}

/**
 * Between them, characters can be measured against all eighteen.
 *
 * The rule the writers were sized against, and the reason it is worth pinning:
 * a virtue that some character can be given but nothing in play can move is a
 * dead counter for whoever draws it, and dead counters are what the original
 * was made of. Eighteen reachable virtues means eighteen that need a writer.
 *
 * This half is checkable here; the other half -- that each of the eighteen has
 * a writer -- lives in DEC-43 and in the hook sites, because writers are code
 * rather than data. If this count ever drops, a virtue has fallen out of
 * class.txt, p_race.txt or realm.txt and nothing else would say so.
 */
static int test_all_eighteen_are_reachable(void *state) {
	struct player_class *c;
	struct player_race *r;
	bool seen[V_MAX];
	int i, reachable = 0;

	memset(seen, 0, sizeof(seen));

	for (c = classes; c; c = c->next) {
		for (r = races; r; r = r->next) {
			reselect(r->name, c->name);

			for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
				seen[player->vir_types[i]] = true;
			}
		}
	}

	for (i = 1; i < V_MAX; i++) {
		if (seen[i]) reachable++;
	}

	eq(reachable, V_MAX - 1);

	ok;
}

/**
 * The selection is the character's, not a roll.
 *
 * Zangband's own tables, and the part of the system that did real work: two
 * Warriors are asked about the same things and a Warrior and a Necromancer are
 * not. Named classes rather than a property, because the point is the specific
 * judgement -- a Necromancer measured against Unlife is the table saying
 * something about Necromancers.
 */
static int test_the_selection_follows_the_character(void *state) {
	reselect("Human", "Warrior");
	require(player_has_virtue(player, V_VALOUR));
	require(player_has_virtue(player, V_HONOUR));

	reselect("Human", "Necromancer");
	require(player_has_virtue(player, V_UNLIFE));

	reselect("Human", "Chaos-Warrior");
	require(player_has_virtue(player, V_CHANCE));
	require(player_has_virtue(player, V_INDIVIDUALISM));

	/* Race contributes too: a Yeek is asked about Sacrifice whatever it does. */
	reselect("Yeek", "Warrior");
	require(player_has_virtue(player, V_SACRIFICE));

	ok;
}

/**
 * A virtue the character was never asked about reads as zero, not as absent.
 *
 * What lets a consumer name any of the eighteen without checking first. If
 * this returned something else -- or if the lookup fell off the end of the
 * array -- every consumer would need a guard, and the one that forgot it would
 * be reading whatever was next in memory.
 */
static int test_an_unheld_virtue_reads_zero(void *state) {
	int i;

	reselect("Human", "Warrior");

	for (i = 1; i < V_MAX; i++) {
		if (player_has_virtue(player, i)) continue;

		eq(virtue_value(player, i), 0);
		virtue_change(player, i, 50);
		eq(virtue_value(player, i), 0);
	}

	ok;
}

/**
 * A standing moves, and stops at the cap.
 */
static int test_a_standing_moves_and_stops(void *state) {
	reselect("Human", "Warrior");

	eq(virtue_value(player, V_VALOUR), 0);
	virtue_change(player, V_VALOUR, 10);
	eq(virtue_value(player, V_VALOUR), 10);
	virtue_change(player, V_VALOUR, -25);
	eq(virtue_value(player, V_VALOUR), -15);

	virtue_change(player, V_VALOUR, 10000);
	eq(virtue_value(player, V_VALOUR), VIRTUE_CAP);
	virtue_change(player, V_VALOUR, -10000);
	eq(virtue_value(player, V_VALOUR), -VIRTUE_CAP);

	ok;
}

/**
 * The Courts notice how a character has lived.
 *
 * The first consumer, and the one that makes the counters worth keeping. A
 * Chaos-Warrior who has lived chaotically should be treated more gently by
 * their Lord than one who has lived in order -- so the same character, at the
 * same level, with the same patron, is sampled twice with the virtues moved
 * between.
 *
 * Sampled rather than asserted once, because the roll is a roll. The margin is
 * wide: at level 20 the nasty chance runs one in six by default, and the four
 * virtues at full stretch move it two steps either way.
 */
static int test_the_courts_notice_how_you_lived(void *state) {
	int i, chaotic_low = 0, orderly_low = 0;
	int floor_slot = PATRON_LADDER / 4;

	reselect("Human", "Chaos-Warrior");
	patron_choose(player);
	notnull(player->patron);
	player->lev = 20;

	require(player_has_virtue(player, V_CHANCE));
	require(player_has_virtue(player, V_INDIVIDUALISM));

	/* Lived chaotically: the Lord is easier on them. */
	virtue_change(player, V_CHANCE, VIRTUE_CAP);
	virtue_change(player, V_INDIVIDUALISM, VIRTUE_CAP);
	for (i = 0; i < 4000; i++) {
		if (patron_roll_slot(player) < floor_slot) chaotic_low++;
	}

	/* And the reverse. */
	virtue_change(player, V_CHANCE, -2 * VIRTUE_CAP);
	virtue_change(player, V_INDIVIDUALISM, -2 * VIRTUE_CAP);
	for (i = 0; i < 4000; i++) {
		if (patron_roll_slot(player) < floor_slot) orderly_low++;
	}

	require(chaotic_low > 0);
	require(orderly_low * 2 > chaotic_low * 3);

	ok;
}

/**
 * And the dream is clearer to someone who has been looking.
 *
 * The second consumer. Read through the arithmetic rather than by sleeping,
 * because the dream needs a wilderness, an inn and a night; what is being
 * checked is that the virtue term exists and points the right way, which is
 * the part that would be wrong.
 */
static int test_a_dream_is_clearer_to_the_enlightened(void *state) {
	int base_true, base_dark, learned, benighted;

	reselect("Human", "Mindcrafter");
	player_dream_chances(200, &base_true, &base_dark);

	/*
	 * The same sum the consumer applies, against the same virtues. A
	 * Mindcrafter is measured against Enlightenment by its class table, so
	 * this moves a counter the character actually has.
	 */
	require(player_has_virtue(player, V_ENLIGHTEN));
	virtue_change(player, V_ENLIGHTEN, VIRTUE_CAP);
	learned = base_true + (virtue_value(player, V_ENLIGHTEN)
						   + virtue_value(player, V_KNOWLEDGE)) / 20;

	virtue_change(player, V_ENLIGHTEN, -2 * VIRTUE_CAP);
	benighted = base_true + (virtue_value(player, V_ENLIGHTEN)
							 + virtue_value(player, V_KNOWLEDGE)) / 20;

	require(learned > base_true);
	require(benighted < base_true);

	ok;
}

/**
 * Killing a thing moves what killing that thing should move.
 *
 * The densest writer, and the one that most needed a test: this hook shipped
 * with a pointer of the wrong type in it -- `monster_is_living()` takes a
 * monster and was handed a race -- and every one of the suite's other tests
 * passed anyway, because nothing called it. Clang built it, the undefined
 * behaviour ran, and only GCC with -Werror on a Linux runner objected.
 *
 * So this exercises the substitution rather than the surrounding arithmetic:
 * a living unique should push a character towards Unlife and away from
 * Vitality, an undead one the other way. The virtues are set directly rather
 * than birthed, so the test says what it means regardless of which eight the
 * tables would have chosen.
 */
static int test_killing_things_moves_the_right_virtues(void *state) {
	struct monster_race *living = lookup_monster("Farmer Maggot");
	struct monster_race *undead = lookup_monster("Kharis the Powerslave");
	int i;

	notnull(living);
	notnull(undead);
	require(rf_has(living->flags, RF_UNIQUE));
	require(rf_has(undead->flags, RF_UNIQUE));
	require(rf_has(undead->flags, RF_UNDEAD));

	reselect("Human", "Warrior");
	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) player->vir_types[i] = V_NONE;
	player->vir_types[0] = V_UNLIFE;
	player->vir_types[1] = V_VITALITY;
	player->virtues[0] = 0;
	player->virtues[1] = 0;

	/* Killing something alive leans towards unlife. */
	virtue_note_kill(player, living, 1);
	require(virtue_value(player, V_UNLIFE) > 0);
	require(virtue_value(player, V_VITALITY) < 0);

	/* And killing the undead leans back. */
	player->virtues[0] = 0;
	player->virtues[1] = 0;
	virtue_note_kill(player, undead, 1);
	eq(virtue_value(player, V_UNLIFE), 0);
	require(virtue_value(player, V_VITALITY) > 0);

	ok;
}

const char *suite_name = "player/virtue";
struct test tests[] = {
	{ "every-character-gets-eight-distinct-virtues",
	  test_every_character_gets_eight_distinct_virtues },
	{ "all-eighteen-are-reachable", test_all_eighteen_are_reachable },
	{ "the-selection-follows-the-character",
	  test_the_selection_follows_the_character },
	{ "an-unheld-virtue-reads-zero", test_an_unheld_virtue_reads_zero },
	{ "a-standing-moves-and-stops", test_a_standing_moves_and_stops },
	{ "the-courts-notice-how-you-lived",
	  test_the_courts_notice_how_you_lived },
	{ "a-dream-is-clearer-to-the-enlightened",
	  test_a_dream_is_clearer_to_the_enlightened },
	{ "killing-things-moves-the-right-virtues",
	  test_killing_things_moves_the_right_virtues },
	{ NULL, NULL }
};
