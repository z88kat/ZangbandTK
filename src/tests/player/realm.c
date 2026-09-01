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
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

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
	{ NULL, NULL }
};
