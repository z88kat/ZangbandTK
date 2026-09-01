/* player/realm
 *
 * The seven magic realms, and the promise that adding them changed nothing
 * about what an existing character can cast (ZangbandTK, PLR-08 to PLR-12,
 * CNT-10).
 *
 * PLR-12 asks that 4.2's existing class progressions be re-expressed as realms
 * rather than left beside them, and a re-expression of working content has one
 * way to go wrong that matters more than the rest: a character who could cast
 * something yesterday cannot today, or casts a different thing under the same
 * name.
 *
 * That failure is quiet and it is not hypothetical. `player->spell_flags[]` and
 * `spell_order[]` are indexed by a flat position across every book a class
 * carries, and the savefile writes them by that index. Insert a book anywhere
 * but the end of a class's list and every spell a saved character knows shifts
 * one place along -- the game will load, the character sheet will look
 * reasonable, and the Priest who had learned Remove Fear will find they know
 * something else. So the shape of every existing class is pinned here, exactly,
 * and any phase of this milestone that disturbs one has to come through this
 * file first.
 */

#include "unit-test.h"

#include "init.h"
#include "obj-tval.h"
#include "player.h"
#include "player-birth.h"
#include "player-spell.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	/*
	 * A real character, because three of these call player_generate() to watch
	 * what changing class does to the realms. Without one the first of them
	 * segfaults rather than failing, which reads as the suite dying and not as
	 * a test result.
	 */
	if (!player_make_simple(NULL, NULL, "Student")) {
		cleanup_angband();
		return 1;
	}

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** Find a class by name, or NULL. */
static const struct player_class *find_class(const char *name) {
	const struct player_class *c;

	for (c = classes; c; c = c->next) {
		if (streq(c->name, name)) return c;
	}

	return NULL;
}

/** Find a realm by name, or NULL. */
static const struct magic_realm *find_realm(const char *name) {
	const struct magic_realm *r;

	for (r = realms; r; r = r->next) {
		if (streq(r->name, name)) return r;
	}

	return NULL;
}

/**
 * Seven realms, and they are Zangband's seven (PLR-09).
 *
 * Asserted by name and as a count, because the count is the requirement and
 * the names are what the mapping decision turned on. Four of these are 4.2's
 * own records renamed to the name this game uses for the same thing; three are
 * new. A fifth appearing, or one of the four reverting, both mean the mapping
 * in DEC-49 has come apart.
 */
static int test_seven_realms_exist(void *state) {
	static const char *const seven[] = {
		"arcane", "life", "nature", "death", "sorcery", "chaos", "trump"
	};
	const struct magic_realm *r;
	size_t i;
	int count = 0;

	for (r = realms; r; r = r->next) count++;
	eq(count, 7);

	for (i = 0; i < N_ELEMENTS(seven); i++) {
		notnull(find_realm(seven[i]));
	}

	/* And 4.2's two old names are gone rather than lingering beside them. */
	null(find_realm("divine"));
	null(find_realm("shadow"));

	ok;
}

/**
 * Every realm carries the metadata PLR-10 asks be retained.
 *
 * A realm with no verb or no spell noun parses and then prints "You have
 * forgotten the (null) of ..." the first time a character loses a spell. The
 * three new realms are the ones at risk, since the four old ones came with
 * theirs.
 */
static int test_every_realm_is_fully_described(void *state) {
	const struct magic_realm *r;
	int checked = 0;

	for (r = realms; r; r = r->next) {
		notnull(r->name);
		notnull(r->verb);
		notnull(r->spell_noun);
		notnull(r->book_noun);
		require(r->verb[0]);
		require(r->spell_noun[0]);
		require(r->book_noun[0]);
		require(r->stat == STAT_INT || r->stat == STAT_WIS);
		checked++;
	}

	eq(checked, 7);

	ok;
}

/**
 * No existing class's progression moved (PLR-12).
 *
 * The shape of all eight casting classes 4.2 ships, as they were before the
 * realms arrived: how many books, and how many spells in total. The totals are
 * what `spell_flags[]` is sized and saved by, so a change to one is a change to
 * every savefile for that class.
 */
static int test_no_existing_class_progression_moved(void *state) {
	static const struct {
		const char *name;
		int books, spells;
	} shape[] = {
		{ "Mage", 5, 30 },
		{ "Druid", 5, 27 },
		{ "Priest", 5, 28 },
		{ "Necromancer", 5, 26 },
		{ "Paladin", 3, 16 },
		{ "Rogue", 2, 10 },
		{ "Ranger", 2, 11 },
		{ "Blackguard", 3, 15 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(shape); i++) {
		const struct player_class *c = find_class(shape[i].name);

		notnull(c);
		eq(c->magic.num_books, shape[i].books);
		eq(c->magic.total_spells, shape[i].spells);
	}

	ok;
}

/**
 * And every book is still the same book, in the same place.
 *
 * The counts above would not notice a swap. This pins each class's books in
 * order by the realm they belong to, which is the field the rename touched, and
 * by their item sub-type, which is the field `player_object_to_book()` matches
 * on -- so a book that changed identity or changed position fails here.
 *
 * The Paladin and the Priest share three books and are both listed on purpose:
 * they are the case where one edit reaches two classes.
 */
static int test_every_book_kept_its_place(void *state) {
	static const struct {
		const char *cls;
		const char *realms[5];
	} order[] = {
		{ "Mage",        { "arcane", "arcane", "arcane", "arcane", "arcane" } },
		{ "Druid",       { "nature", "nature", "nature", "nature", "nature" } },
		{ "Priest",      { "life", "life", "life", "life", "life" } },
		{ "Necromancer", { "death", "death", "death", "death", "death" } },
		{ "Paladin",     { "life", "life", "life", NULL, NULL } },
		{ "Rogue",       { "arcane", "arcane", NULL, NULL, NULL } },
		{ "Ranger",      { "nature", "nature", NULL, NULL, NULL } },
		{ "Blackguard",  { "death", "death", "death", NULL, NULL } },
	};
	size_t i;
	int j, checked = 0;

	for (i = 0; i < N_ELEMENTS(order); i++) {
		const struct player_class *c = find_class(order[i].cls);

		notnull(c);
		for (j = 0; j < c->magic.num_books; j++) {
			const struct class_book *b = &c->magic.books[j];

			require(order[i].realms[j] != NULL);
			notnull(b->realm);
			require(streq(b->realm->name, order[i].realms[j]));
			require(b->sval > 0);
			require(b->num_spells > 0);
			checked++;
		}

		/* And no book beyond the ones listed. */
		if (c->magic.num_books < 5) {
			require(order[i].realms[c->magic.num_books] == NULL);
		}
	}

	eq(checked, 30);

	ok;
}

/**
 * A Priest's spells are in the order a Priest's savefile expects.
 *
 * The flat index is the thing that breaks, so one class is pinned spell by
 * spell. The Priest rather than the Mage because the Priest shares books with
 * the Paladin, so it is the class where an edit is most likely to arrive from
 * somewhere else.
 *
 * Names only: levels and mana are the balance surface and are allowed to be
 * tuned, but a name moving between indices is a saved character forgetting one
 * prayer and remembering another.
 */
static int test_a_priests_spell_order_is_unchanged(void *state) {
	static const char *const first_five[] = {
		"Call Light", "Detect Evil", "Minor Healing", "Bless", "Sense Invisible"
	};
	const struct player_class *priest = find_class("Priest");
	const struct class_book *book;
	int i;

	notnull(priest);
	require(priest->magic.num_books > 0);

	book = &priest->magic.books[0];
	require(book->num_spells >= (int) N_ELEMENTS(first_five));

	for (i = 0; i < (int) N_ELEMENTS(first_five); i++) {
		notnull(book->spells[i].name);
		require(streq(book->spells[i].name, first_five[i]));
		eq(book->spells[i].sidx, i);
	}

	ok;
}

/**
 * Which realms each class may study, against Zangband's own table (PLR-08).
 *
 * The entitlements are Zangband's, out of `realm_choices1[]` and
 * `realm_choices2[]` ([tables.c:5329](../archive/zangband/src/tables.c#L5329)),
 * and the expectations below are transcribed from that table rather than from
 * anybody's idea of what a class should be able to study. Two of them are worth
 * knowing about before reading the list as though it were designed:
 *
 * - **A Priest's second realm is anything but Life or Death.** The first slot
 *   offers the two priestly realms and the second offers the other five, so a
 *   Priest ends up with one holy realm and one that is not.
 * - **A Ranger's first realm is Nature and there is no choice about it.** A
 *   single-entry set is an entitlement, not a choice, and the birth step has to
 *   tell those apart or it will ask a question with one answer.
 *
 * Angband's Druid, Necromancer and Blackguard have no Zangband counterpart, so
 * each is entitled to the one realm it already studies. That keeps what they can
 * cast exactly as it is, which is the constraint DEC-49 set.
 */
static int test_class_realm_entitlements_match_zangband(void *state) {
	static const struct {
		const char *cls;
		int count;
		const char *slot1;
		const char *slot2;
	} expect[] = {
		{ "Mage",        2, "arcane life nature death sorcery chaos trump",
		                    "arcane life nature death sorcery chaos trump" },
		{ "Priest",      2, "life death", "arcane nature sorcery chaos trump" },
		{ "Rogue",       1, "arcane death sorcery trump", NULL },
		{ "Ranger",      2, "nature", "arcane death sorcery chaos trump" },
		{ "Paladin",     1, "life death", NULL },
		/* Angband's own three: one realm each, and nothing to choose. */
		{ "Druid",       1, "nature", NULL },
		{ "Necromancer", 1, "death", NULL },
		{ "Blackguard",  1, "death", NULL },
	};
	size_t i;
	int checked = 0;

	for (i = 0; i < N_ELEMENTS(expect); i++) {
		const struct player_class *c = find_class(expect[i].cls);
		const struct magic_realm *r;
		int slot;

		notnull(c);
		eq(c->magic.realm_count, expect[i].count);

		for (slot = 0; slot < REALM_CHOICES; slot++) {
			const char *want = slot ? expect[i].slot2 : expect[i].slot1;

			for (r = realms; r; r = r->next) {
				bool allowed = c->magic.realm_allowed[slot][r->ridx];
				bool wanted = want && strstr(want, r->name) != NULL;

				require(allowed == wanted);
				checked++;
			}
		}
	}

	/* Eight classes, two slots, seven realms. */
	eq(checked, 8 * REALM_CHOICES * REALM_MAX);

	ok;
}

/**
 * Every realm has an index, and they are the savefile's order.
 *
 * A character's realm choice is written by index, so the indices have to be
 * dense, unique and in file order -- appending a realm must be safe and
 * inserting one must not silently renumber a saved character's choice into a
 * different realm.
 */
static int test_realms_are_numbered_in_file_order(void *state) {
	const struct magic_realm *r;
	unsigned int expected = 0;

	static const char *const order[] = {
		"arcane", "life", "nature", "death", "sorcery", "chaos", "trump"
	};

	for (r = realms; r; r = r->next) {
		eq(r->ridx, expected);
		require(streq(r->name, order[expected]));
		expected++;
	}

	eq(expected, REALM_MAX);

	ok;
}

/**
 * A character is always studying something they can read (PLR-08).
 *
 * `player_generate()` defaults the realms from the class, and the case that
 * matters is *changing* class at birth: a character who was a Priest and picks
 * Rogue must not keep Life in a slot the Rogue does not have, or they would be
 * carrying a realm whose books they cannot open.
 *
 * Asserted by walking a character through three classes with different
 * entitlements, which is what the birth menu does when somebody browses.
 */
static int test_changing_class_leaves_no_stale_realm(void *state) {
	const struct player_class *priest = find_class("Priest");
	const struct player_class *rogue = find_class("Rogue");
	const struct player_class *warrior = find_class("Warrior");
	int slot;

	notnull(priest);
	notnull(rogue);
	notnull(warrior);

	/* A Priest studies two, and both are ones a Priest may. */
	player_generate(player, NULL, priest, false);
	notnull(player->realm[0]);
	notnull(player->realm[1]);
	for (slot = 0; slot < REALM_CHOICES; slot++) {
		require(priest->magic.realm_allowed[slot][player->realm[slot]->ridx]);
	}

	/* A Rogue studies one, and the second slot is emptied rather than kept. */
	player_generate(player, NULL, rogue, false);
	notnull(player->realm[0]);
	null(player->realm[1]);
	require(rogue->magic.realm_allowed[0][player->realm[0]->ridx]);

	/* And a Warrior studies nothing at all. */
	player_generate(player, NULL, warrior, false);
	null(player->realm[0]);
	null(player->realm[1]);

	ok;
}

/**
 * A single-entry entitlement is not a choice.
 *
 * The birth step has to tell an entitlement from a choice or it will ask a
 * Ranger which of the one realm they would like. `player_realm_choices()`
 * returns the count so the caller can skip a slot with nothing to decide, and
 * this pins the three shapes it has to distinguish: none, exactly one, and
 * several.
 */
static int test_a_single_entitlement_is_not_a_choice(void *state) {
	const struct magic_realm *got[REALM_MAX];

	/* None: a Warrior chooses nothing in either slot. */
	eq(player_realm_choices(find_class("Warrior"), 0, got, REALM_MAX), 0);
	eq(player_realm_choices(find_class("Warrior"), 1, got, REALM_MAX), 0);

	/* One: a Ranger's first realm is Nature and there is no question. */
	eq(player_realm_choices(find_class("Ranger"), 0, got, REALM_MAX), 1);
	require(streq(got[0]->name, "nature"));

	/* Several: a Ranger's second is a genuine choice of five. */
	eq(player_realm_choices(find_class("Ranger"), 1, got, REALM_MAX), 5);

	/* And a Mage chooses from all seven, twice. */
	eq(player_realm_choices(find_class("Mage"), 0, got, REALM_MAX), REALM_MAX);
	eq(player_realm_choices(find_class("Mage"), 1, got, REALM_MAX), REALM_MAX);

	/* A slot beyond the last is answered rather than read past. */
	eq(player_realm_choices(find_class("Mage"), REALM_CHOICES, got,
							REALM_MAX), 0);
	eq(player_realm_choices(NULL, 0, got, REALM_MAX), 0);

	ok;
}

/**
 * Studying a realm is asked of the character, not the class.
 *
 * `player_studies_realm()` is what the spell machinery will gate on once the
 * books arrive, so it has to answer for the realms a character actually chose
 * rather than the ones their class allows. A Priest may study Chaos and mostly
 * does not.
 */
static int test_studying_is_a_property_of_the_character(void *state) {
	const struct player_class *priest = find_class("Priest");
	const struct magic_realm *life = find_realm("life");
	const struct magic_realm *chaos = find_realm("chaos");

	notnull(priest);
	notnull(life);
	notnull(chaos);

	player_generate(player, NULL, priest, false);

	/* Defaulted to the first of each slot: Life, and then Arcane. */
	require(player_studies_realm(player, life));
	require(!player_studies_realm(player, chaos));

	/* Chaos is allowed in the second slot, and taking it is what counts. */
	require(priest->magic.realm_allowed[1][chaos->ridx]);
	player->realm[1] = chaos;
	require(player_studies_realm(player, chaos));

	require(!player_studies_realm(player, NULL));
	require(!player_studies_realm(NULL, life));

	ok;
}

const char *suite_name = "player/realm";
struct test tests[] = {
	{ "seven-realms-exist", test_seven_realms_exist },
	{ "every-realm-is-fully-described",
	  test_every_realm_is_fully_described },
	{ "no-existing-class-progression-moved",
	  test_no_existing_class_progression_moved },
	{ "every-book-kept-its-place", test_every_book_kept_its_place },
	{ "a-priests-spell-order-is-unchanged",
	  test_a_priests_spell_order_is_unchanged },
	{ "class-realm-entitlements-match-zangband",
	  test_class_realm_entitlements_match_zangband },
	{ "realms-are-numbered-in-file-order",
	  test_realms_are_numbered_in_file_order },
	{ "changing-class-leaves-no-stale-realm",
	  test_changing_class_leaves_no_stale_realm },
	{ "a-single-entitlement-is-not-a-choice",
	  test_a_single_entitlement_is_not_a_choice },
	{ "studying-is-a-property-of-the-character",
	  test_studying_is_a_property_of_the_character },
	{ NULL, NULL }
};
