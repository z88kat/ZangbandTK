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
#include "obj-make.h"
#include "obj-util.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
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
 * Every class's progression, and each new realm went on the end (PLR-12, CNT-10).
 *
 * The shape of all eight casting classes: how many books, and how many spells in
 * total. The totals are what `spell_flags[]` is sized and saved by.
 *
 * They grew when Sorcery arrived and again when Chaos did, which DEC-50
 * licenses — but each realm's books went on the **end** of the list, never into
 * the middle, and that is the part worth having a test for. A character's known spells are recorded by flat
 * position across their class's books, so appending leaves every index that
 * already existed meaning exactly what it meant, while inserting anywhere else
 * would shift them. `every-book-kept-its-place` below asserts the prefix
 * directly.
 */
static int test_no_existing_class_progression_moved(void *state) {
	static const struct {
		const char *name;
		int books, spells;
	} shape[] = {
		/*
		 * Grown by Sorcery's four books (Mage, Priest, Rogue, Ranger)
		 * and again by Chaos's four (Mage, Priest, Ranger). The Rogue
		 * is not entitled to Chaos and did not grow the second time,
		 * which is the entitlement table doing its job.
		 *
		 * The Chaos-Warrior is here for the opposite reason: it went
		 * from no books at all to four, because Chaos is the one realm
		 * Zangband's table gives it and the realm now exists.
		 *
		 * The Mage and the Rogue moved again when **Arcane** was
		 * replaced (DEC-50), and that is a different kind of move from
		 * the two above: arcane books are the *first* in both lists, so
		 * swapping Angband's five for Zangband's four shifts every spell
		 * index after them. The Mage went 13 books to 12, the Rogue 6 to
		 * 8. DEC-50 licenses exactly this, the savefile corpus was
		 * already refused whole before it, and `game/roundtrip` proves
		 * the fingerprint still catches a character whose list moved.
		 * What this test guards is the *next* move, the one nobody meant.
		 *
		 * **Nature** moved the Druid and the Ranger the same way: the
		 * Druid's five books became four, and the Ranger, which borrowed
		 * two of the Druid's, gained the whole realm and went to twelve.
		 * The Druid is also the first class in the game to take its
		 * figures from a class Zangband never had -- DEC-55 gives it
		 * Zangband's Priest, the only Nature carrier sharing the Druid's
		 * `spell_first`, `spell_weight` and casting stat.
		 *
		 * **Death** finished it, and moved both of its classes: the
		 * Necromancer's five books became four and the Blackguard's
		 * three became four, and both cast on borrowed figures -- the
		 * Necromancer on Zangband's Mage, the Blackguard on its Paladin.
		 * Every class in the table is now on a multiple of four books,
		 * because every realm is four books of eight.
		 */
		{ "Mage", 24, 192 },
		{ "Druid", 4, 32 },
		{ "Priest", 24, 192 },
		{ "Necromancer", 4, 32 },
		{ "Paladin", 8, 64 },
		{ "Rogue", 12, 88 },
		{ "Ranger", 20, 147 },
		{ "Blackguard", 4, 32 },
		{ "Chaos-Warrior", 4, 32 },
		/* PLR-03's last two, and both are entitled to nearly everything. */
		{ "Warrior-Mage", 24, 192 },
		{ "High-Mage", 24, 192 },
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
 * The counts above would not notice a swap. This pins every class's books in
 * order, as runs of one realm, which is the form the data actually has: a
 * realm's four books are contiguous and each realm is appended after the last,
 * so a run table says the same thing as a book-by-book list and stays readable
 * at a hundred and sixty-four books. It also pins the run *lengths*, which the
 * old per-book form did not.
 *
 * Written as runs when DEC-57 completed Zangband's entitlements and the Mage,
 * the Priest, the Warrior-Mage and the High-Mage all reached twenty-four books
 * -- wider than any sensible fixed-size row. Every class is listed now, which
 * the old form could not manage.
 *
 * The Paladin and the Priest are both here on purpose: they share the Life
 * realm, so one edit reaches two classes. They do not share book *kinds* --
 * DEC-50 gave each of them Zangband's four Life books, where 4.2 had the
 * Paladin carrying three of the Priest's five.
 *
 * **The order is the savefile's order.** A character's known spells are
 * recorded by flat position across all of its class's books, so a realm
 * appearing anywhere but where it does would shift every index after it. That
 * is why DEC-57's new realms went on the *end* of each list and why this test
 * reads as a prefix: Arcane, Sorcery and Chaos still open the Mage's list in
 * that order, and the three realms it gained follow.
 */
static int test_every_book_kept_its_place(void *state) {
	struct run { const char *realm; int books; };
	static const struct {
		const char *cls;
		struct run runs[7];
	} order[] = {
		{ "Mage",        { { "arcane", 4 }, { "sorcery", 4 }, { "chaos", 4 },
		                   { "life", 4 }, { "nature", 4 }, { "death", 4 } } },
		{ "Druid",       { { "nature", 4 } } },
		{ "Priest",      { { "life", 4 }, { "sorcery", 4 }, { "chaos", 4 },
		                   { "nature", 4 }, { "death", 4 }, { "arcane", 4 } } },
		{ "Necromancer", { { "death", 4 } } },
		{ "Paladin",     { { "life", 4 }, { "death", 4 } } },
		{ "Rogue",       { { "arcane", 4 }, { "sorcery", 4 },
		                   { "death", 4 } } },
		{ "Ranger",      { { "nature", 4 }, { "sorcery", 4 }, { "chaos", 4 },
		                   { "death", 4 }, { "arcane", 4 } } },
		{ "Blackguard",  { { "death", 4 } } },
		{ "Monk",        { { "life", 4 }, { "nature", 4 }, { "death", 4 } } },
		{ "Chaos-Warrior", { { "chaos", 4 } } },
		{ "Warrior-Mage", { { "life", 4 }, { "sorcery", 4 }, { "nature", 4 },
		                    { "chaos", 4 }, { "death", 4 },
		                    { "arcane", 4 } } },
		{ "High-Mage",   { { "life", 4 }, { "sorcery", 4 }, { "nature", 4 },
		                   { "chaos", 4 }, { "death", 4 },
		                   { "arcane", 4 } } },
	};
	size_t i;
	int checked = 0;

	for (i = 0; i < N_ELEMENTS(order); i++) {
		const struct player_class *c = find_class(order[i].cls);
		int b = 0;
		size_t r;

		notnull(c);
		for (r = 0; r < N_ELEMENTS(order[i].runs); r++) {
			const char *realm = order[i].runs[r].realm;
			int n = order[i].runs[r].books;
			int k;

			if (!realm) break;
			for (k = 0; k < n; k++) {
				const struct class_book *book;

				/* The run must actually be there, not run off the end. */
				require(b < c->magic.num_books);
				book = &c->magic.books[b];

				notnull(book->realm);
				require(streq(book->realm->name, realm));
				require(book->sval > 0);
				require(book->num_spells > 0);
				b++;
				checked++;
			}
		}

		/* And no book beyond the runs listed. */
		eq(b, c->magic.num_books);
	}

	/*
	 * A hundred and sixty-four books across the twelve casting classes, and
	 * every class on a multiple of four, because every realm is four books.
	 */
	eq(checked, 164);

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
 *
 * These five were 4.2's -- Call Light, Detect Evil, Minor Healing, Bless, Sense
 * Invisible -- until DEC-50 replaced the Life realm's content with Zangband's.
 * That replacement is the whole point of CNT-10 and is the one thing this test
 * is *not* guarding against; it guards against the next accidental move. The
 * saved characters this would have protected are already refused by the
 * fingerprint on the `player spells` block, which is the honest outcome and the
 * reason that guard was built first.
 */
static int test_a_priests_spell_order_is_unchanged(void *state) {
	static const char *const first_five[] = {
		"Detect Evil", "Cure Light Wounds", "Bless", "Remove Fear",
		"Call Light"
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

	/*
	 * Several: a Ranger's second slot. Zangband entitles it to Sorcery,
	 * Chaos, Death, Trump and Arcane, and DEC-57 completed the books, so
	 * four of those five are offered -- everything but Trump, which has
	 * none. Nature is absent because slot 1 holds it, not because of books.
	 */
	eq(player_realm_choices(find_class("Ranger"), 1, got, REALM_MAX), 4);

	/*
	 * A Mage is entitled to all seven in both slots and is offered the six
	 * that have content. Before DEC-57 it was offered three, because the
	 * four mapped realms were only ever emitted into the classes that
	 * already carried those books.
	 */
	eq(player_realm_choices(find_class("Mage"), 0, got, REALM_MAX), 6);
	eq(player_realm_choices(find_class("Mage"), 1, got, REALM_MAX), 6);

	/* A slot beyond the last is answered rather than read past. */
	eq(player_realm_choices(find_class("Mage"), REALM_CHOICES, got,
							REALM_MAX), 0);
	eq(player_realm_choices(NULL, 0, got, REALM_MAX), 0);

	ok;
}

/**
 * A realm you can choose is a realm you can read.
 *
 * The entitlement table is Zangband's and is imported whole, so it allows
 * realms this game has not built yet. Offering one at birth is a trap with no
 * way out: the character is made, the realm is chosen, and the spell menu is
 * empty for the rest of that character's life. It was live between 3.54.2 and
 * 3.55.0, and Trump is in exactly that state now (DEC-54).
 *
 * So: every realm offered in either slot must be one the class has a book in,
 * and -- the half that stops the guard being satisfied by offering nothing at
 * all -- every realm the class has a book in must be offered in some slot.
 */
static int test_an_offered_realm_has_books_behind_it(void *state) {
	const struct player_class *c;
	int slot, i, offered_total = 0;

	for (c = classes; c; c = c->next) {
		bool offered[REALM_MAX] = { false };

		for (slot = 0; slot < REALM_CHOICES; slot++) {
			const struct magic_realm *got[REALM_MAX];
			int n = player_realm_choices(c, slot, got, REALM_MAX);

			for (i = 0; i < n; i++) {
				require(class_has_realm_book(c, got[i]));
				offered[got[i]->ridx] = true;
				offered_total++;
			}
		}

		for (i = 0; i < c->magic.num_books; i++) {
			const struct magic_realm *r = c->magic.books[i].realm;

			notnull(r);
			require(offered[r->ridx]);
		}
	}

	/*
	 * And Trump, which every one of the six casting classes below is
	 * entitled to, is offered to none of them because it has no books.
	 */
	for (c = classes; c; c = c->next) {
		const struct magic_realm *trump = find_realm("trump");
		const struct magic_realm *got[REALM_MAX];
		int n, found = 0;

		notnull(trump);
		for (slot = 0; slot < REALM_CHOICES; slot++) {
			n = player_realm_choices(c, slot, got, REALM_MAX);
			for (i = 0; i < n; i++) {
				if (got[i] == trump) found++;
			}
		}
		eq(found, 0);
	}

	/*
	 * Thirty-one offers across the eleven casting classes. Angband's nine
	 * account for nineteen: Mage 3+3, Priest 1+2, Rogue 2, Ranger 1+2, and
	 * one apiece for the Druid, Necromancer, Paladin, Blackguard and
	 * Chaos-Warrior. A Priest's first slot allows Life or Death and only
	 * Life is built, which is why it offers one and not two.
	 *
	 * The other sixteen are the three classes whose realms are their whole
	 * definition. A Warrior-Mage takes Arcane in the first slot and any of
	 * the seven -- Arcane included, which is Zangband's own table -- in the
	 * second, so one plus six. A High-Mage chooses one realm from all seven
	 * and has no second slot, so six. A Monk chooses one from Life, Nature
	 * and Death and likewise has no second slot, so three. Trump is entitled
	 * for the first two and offered to neither, because it has no books
	 * (DEC-54).
	 */
	eq(offered_total, 48);

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

/**
 * The filter's first real test: a Priest of Sorcery cannot read prayer books.
 *
 * Until Sorcery arrived, no class carried books from two realms, so the realm
 * filter had nothing to sort and removing it failed nothing. A Priest now
 * carries **all six realms that have content** -- Life, Sorcery, Chaos, Nature,
 * Death and Arcane, twenty-four books between them, since DEC-57 completed
 * Zangband's entitlements -- and which of them the character can open is the
 * whole point of choosing a realm.
 *
 * Three of the six are checked, in a grid, and each in both directions, because
 * a filter that refuses everything passes a test that only checks it refuses
 * something. With three realms in play a wrong answer also has somewhere to go
 * that two did not: a filter keyed on the class rather than the character, or
 * one that lets a second realm through as well as the chosen one, fails here
 * and would not have failed with Life and Sorcery alone.
 */
static int test_the_realm_filter_sorts_three_realms(void *state) {
	const struct player_class *priest = find_class("Priest");
	static const char *names[] = { "life", "sorcery", "chaos" };
	const struct magic_realm *realm[3];
	struct object *book[3];
	int i, j, k;

	notnull(priest);

	/* Six realms of books is what makes the filter live. */
	eq(class_book_realms(priest), 6);

	for (i = 0; i < 3; i++) {
		const struct class_book *first = NULL;

		realm[i] = find_realm(names[i]);
		notnull(realm[i]);

		for (j = 0; j < priest->magic.num_books; j++) {
			if (priest->magic.books[j].realm == realm[i]) {
				first = &priest->magic.books[j];
				break;
			}
		}
		notnull(first);

		book[i] = object_new();
		object_prep(book[i], lookup_kind(first->tval, first->sval), 0,
					MINIMISE);
	}

	player_generate(player, NULL, priest, false);

	/*
	 * Study one realm at a time. The book of that realm opens and the other
	 * two do not -- so every realm is asserted to work and to be excluded,
	 * and no single wrong answer satisfies the whole grid.
	 */
	for (i = 0; i < 3; i++) {
		player->realm[0] = realm[i];
		player->realm[1] = realm[i];

		for (k = 0; k < 3; k++) {
			if (k == i) {
				notnull(player_object_to_book(player, book[k]));
			} else {
				null(player_object_to_book(player, book[k]));
			}
		}
	}

	/* And two realms at once open exactly two of the three. */
	player->realm[0] = realm[0];
	player->realm[1] = realm[2];
	notnull(player_object_to_book(player, book[0]));
	null(player_object_to_book(player, book[1]));
	notnull(player_object_to_book(player, book[2]));

	for (i = 0; i < 3; i++) {
		object_delete(cave, player->cave, &book[i]);
	}

	ok;
}

/**
 * A spell with no effect must say so, and there must be exactly sixteen.
 *
 * The realms are emitted from realmmap.toml keyed by spell name, and a name
 * that does not match the table is silent: the spell is written out with its
 * level, mana and failure, no effect chain, and a generic description. It looks
 * exactly like a deliberate deferral and is not one. That happened -- Sorcery's
 * *Teleport* was keyed as "Teleport Self" and shipped in 3.55.0 doing nothing --
 * so this is the assertion that would have caught it in the game rather than in
 * the converter.
 *
 * Two halves, and both are needed. Every effectless spell must carry the
 * deferral wording, which catches a misspelt key; and the count must be exact,
 * which catches a spell quietly *gaining* the deferral wording to make the
 * first half pass.
 *
 * Thirty-four as the realms stand. Sixteen are Sorcery's four -- Identify True,
 * Detect Enchantment, Self Knowledge, Explosive Rune -- once for each of the
 * four classes entitled to the realm. Four are Life's two, Day of the Dove and
 * Bless Weapon, once each for the Priest and the Paladin: the first needs
 * monster allegiance and waits for M10 with Trump, and 4.2 has no effect that
 * blesses a weapon. Four are Arcane's two, once each for the Mage and the
 * Rogue: Phlogiston, because refuelling a light source is a command in 4.2 and
 * not an effect, and Detect Enchantment again, for the reason Sorcery's is.
 * Eighty-seven now. The Warrior-Mage and the High-Mage each carry six whole
 * realms and so seventeen deferrals apiece, and the Monk carries Life, Nature
 * and Death, which is eleven more. Of the rest, ten are
 * Nature's five, once each for the Druid and the Ranger: three of those
 * five -- Animal Taming, Summon Animal, Animal Friendship -- are the same
 * monster-allegiance wall that deferred Trump whole, and the other two are
 * whole-object identification and a rustproofing that is an object property
 * here rather than anything a spell can reach. And eight are Death's four, once
 * each for the Necromancer and the Blackguard: Enslave Undead and Raise the
 * Dead are the allegiance wall again, Wraithform is the wall the WRAITH
 * mutation waits behind, and Omnicide is a whole-level sweep with a running
 * mana cost that would otherwise duplicate this realm's own Mass Genocide.
 *
 * `scripts/check-build` also runs `zconv realms --check` now, which catches a
 * mis-keyed name itself rather than its symptom. This stays because it asks the
 * question of the *game* -- of parsed classes and parsed books -- rather than
 * of the data files.
 */
static int test_a_spell_without_an_effect_says_so(void *state) {
	const struct player_class *c;
	int j, k, effectless = 0;

	for (c = classes; c; c = c->next) {
		for (j = 0; j < c->magic.num_books; j++) {
			const struct class_book *b = &c->magic.books[j];

			for (k = 0; k < b->num_spells; k++) {
				const struct class_spell *sp = &b->spells[k];

				if (sp->effect) continue;
				effectless++;
				notnull(sp->text);
				require(strstr(sp->text,
							   "beyond what this game") != NULL);
			}
		}
	}

	eq(effectless, 120);

	ok;
}

/**
 * The Chaos-Warrior can cast, which is the whole of what it was missing.
 *
 * Zangband's spoiler is blunt about it -- "trained in Chaos magic. They are not
 * interested in any other form of magic. They can learn every Chaos spell" --
 * and until Chaos existed there was nothing to give it, so the class shipped
 * declaring `NO_MANA` and holding no books. Both of those are now wrong and
 * both are asserted against.
 *
 * The mana check is the one that matters. `NO_MANA` is also *derived* -- a
 * character with no spell points has it whether the class says so or not -- so
 * removing the declaration proves nothing on its own. A Chaos-Warrior of a
 * useful level has to actually come out with spell points.
 */
static int test_a_chaos_warrior_casts_chaos(void *state) {
	const struct player_class *cw = find_class("Chaos-Warrior");
	const struct magic_realm *chaos = find_realm("chaos");
	const struct magic_realm *got[REALM_MAX];
	const struct class_book *book;
	struct object *tome = object_new();

	notnull(cw);
	notnull(chaos);

	/* One realm, and Zangband gives it no second choice. */
	eq(class_book_realms(cw), 1);
	eq(player_realm_choices(cw, 0, got, REALM_MAX), 1);
	require(got[0] == chaos);
	eq(player_realm_choices(cw, 1, got, REALM_MAX), 0);

	/* The class no longer declares itself unable to hold mana. */
	require(!pf_has(cw->pflags, PF_NO_MANA));

	player_generate(player, NULL, cw, false);
	require(player->realm[0] == chaos);
	require(player_studies_realm(player, chaos));

	/*
	 * And has spell points at a level it could use them. `spell_first` is
	 * 2 for this class, which is Zangband's own figure, so level 1 is
	 * legitimately zero and would not tell us anything.
	 */
	player->lev = 20;
	calc_bonuses(player, &player->state, false, true);
	require(player->msp > 0);

	/* And its books open. */
	book = &cw->magic.books[0];
	object_prep(tome, lookup_kind(book->tval, book->sval), 0, MINIMISE);
	notnull(player_object_to_book(player, tome));
	object_delete(cave, player->cave, &tome);

	ok;
}

/**
 * A spell's number inside its realm, which is what wild magic is scaled by.
 *
 * `sidx` counts across the whole class, so Chaos's *Magic Missile* is spell 0
 * for a Chaos-Warrior and spell 62 for a Mage — the Mage has five Arcane books
 * and four Sorcery ones in front of it. Zangband scales a Chaos backfire by the
 * spell's place in its **realm**, so using `sidx` directly would make every
 * Chaos spell a Mage owns backfire as though it were the deepest in the game,
 * and would look right for a Chaos-Warrior while being wrong for everyone else.
 *
 * Checked for three classes and every realm each of them carries, and tied to
 * two names at the ends so the numbers cannot drift into agreeing with
 * themselves.
 */
static int test_a_spell_knows_its_place_in_its_realm(void *state) {
	static const char *const who[] = { "Chaos-Warrior", "Mage", "Priest" };
	size_t w;
	int checked = 0;

	for (w = 0; w < N_ELEMENTS(who); w++) {
		const struct player_class *c = find_class(who[w]);
		const struct magic_realm *realm = NULL;
		int b, k, expect = 0;

		notnull(c);
		player_generate(player, NULL, c, false);

		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			/* Each realm restarts at zero, and runs up without a gap. */
			if (book->realm != realm) {
				realm = book->realm;
				expect = 0;
			}

			for (k = 0; k < book->num_spells; k++) {
				const struct class_spell *sp = &book->spells[k];

				eq(spell_realm_index(player, sp), expect);
				expect++;
				checked++;
			}
		}
	}

	/* 32 + 96 + 96 spells across the three.  Each of the Mage's and the
	 * Priest's 96 is three whole realms of 32, which is what DEC-50 arriving
	 * looks like from here: 4.2's five prayer books became Zangband's four,
	 * and its five magic books became Zangband's four. */
	eq(checked, 416);

	/*
	 * And the ends, by name. A Mage's Chaos runs 0 to 31 exactly as a
	 * Chaos-Warrior's does, though its `sidx` runs 62 to 93.
	 */
	for (w = 0; w < 2; w++) {
		const struct player_class *c = find_class(who[w]);
		const struct class_spell *first = NULL, *last = NULL;
		int b, k;

		notnull(c);
		player_generate(player, NULL, c, false);

		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			if (!book->realm || !streq(book->realm->name, "chaos")) continue;
			for (k = 0; k < book->num_spells; k++) {
				const struct class_spell *sp = &book->spells[k];

				if (!first) first = sp;
				last = sp;
			}
		}

		notnull(first);
		notnull(last);
		require(streq(first->name, "Magic Missile"));
		require(streq(last->name, "Call the Void"));
		eq(spell_realm_index(player, first), 0);
		eq(spell_realm_index(player, last), 31);
	}

	/* A Mage's really does carry the higher raw index, or nothing was tested. */
	{
		const struct player_class *mage = find_class("Mage");
		const struct class_book *book;

		notnull(mage);
		book = &mage->magic.books[8];
		require(streq(book->realm->name, "chaos"));
		eq(book->spells[0].sidx, 64);
	}

	ok;
}

/**
 * Arcane is bought in town, and the other realms are not (CNT-10, DEC-49).
 *
 * The spoiler's sentence about Arcane has two halves and the second pays for
 * the first: it "has no ultra-powerful high level spells" *and* "all Arcane
 * spellbooks can be bought in town". A realm you can buy outright is worth
 * choosing even when its ceiling is low, and one you cannot is a different
 * bargain. Zangband's own numbers agree -- its four Arcane books cost 100 to
 * 2500 where Sorcery's fourth costs 100,000.
 *
 * The flag is not cosmetic, which is why this is asserted rather than trusted:
 * `store.c` puts every town book in the shop's permanent stock and never a
 * dungeon one, and `init.c` gives a dungeon book the ignore-element flags and
 * marks it good. Emitted with the generic two-town rule, half of Arcane was
 * unbuyable and the realm lost the half of its character that makes it a
 * choice.
 */
static int test_arcane_is_bought_in_town(void *state) {
	static const struct {
		const char *realm;
		int town, dungeon;
	} shape[] = {
		{ "arcane", 4, 0 },
		{ "sorcery", 2, 2 },
		{ "chaos", 2, 2 },
		{ "life", 2, 2 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(shape); i++) {
		const struct magic_realm *realm = find_realm(shape[i].realm);
		const struct player_class *c;
		int town = 0, dungeon = 0;
		bool seen[64] = { false };

		notnull(realm);

		/*
		 * Counted over distinct books rather than over classes, because a
		 * book shared by two classes is one book in the shop.
		 */
		for (c = classes; c; c = c->next) {
			int b;

			for (b = 0; b < c->magic.num_books; b++) {
				const struct class_book *book = &c->magic.books[b];

				if (book->realm != realm) continue;
				require(book->sval > 0 && book->sval < 64);
				if (seen[book->sval]) continue;
				seen[book->sval] = true;

				if (book->dungeon) dungeon++; else town++;
			}
		}

		eq(town, shape[i].town);
		eq(dungeon, shape[i].dungeon);
	}

	/*
	 * And Arcane's prices and depths, by number.
	 *
	 * Pinned because nothing else notices them. The converter check proves
	 * class.txt matches what the generator produces; it cannot tell that the
	 * generator was asked for the wrong ladder. Dropping Arcane's own
	 * `book-tiers` and falling back to the generic one passed every test and
	 * the converter check both -- and would have put a 2500-gold town book in
	 * the shop at 50,000 and made it unfindable above depth 75.
	 *
	 * The numbers are Zangband's own costs, and its own allocation depths
	 * rescaled from its 128-level dungeon to Angband's hundred. The first book
	 * stays at depth 1 because a Mage casts from it at level 1.
	 */
	{
		static const struct {
			const char *title;
			int cost, alloc_min;
		} priced[] = {
			{ "[Cantrips for Beginners]", 100, 1 },
			{ "[Minor Arcana]", 250, 12 },
			{ "[Major Arcana]", 1000, 16 },
			{ "[Manual of Mastery]", 2500, 27 },
		};
		const struct player_class *mage = find_class("Mage");
		size_t k;
		int b, found = 0;

		notnull(mage);
		for (b = 0; b < mage->magic.num_books; b++) {
			const struct class_book *book = &mage->magic.books[b];
			const struct object_kind *kind;

			if (!book->realm || !streq(book->realm->name, "arcane")) continue;
			kind = lookup_kind(book->tval, book->sval);
			notnull(kind);

			for (k = 0; k < N_ELEMENTS(priced); k++) {
				if (!streq(kind->name, priced[k].title)) continue;
				eq(kind->cost, priced[k].cost);
				eq(kind->alloc_min, priced[k].alloc_min);
				found++;
			}
		}
		eq(found, 4);
	}

	ok;
}

/**
 * The Druid casts on Zangband's Priest figures (DEC-55).
 *
 * The Druid, the Necromancer and the Blackguard post-date Zangband, so
 * `magic_info[]` has no row for any of them and DEC-50 replaces their realms'
 * content regardless. Each borrows the figures of the Zangband class it matches
 * on `spell_first`, `spell_weight` and casting stat -- the constants six of the
 * nine classes match exactly *and* by name, which is what makes the key
 * evidence rather than a guess.
 *
 * The Druid's are 1 / 350 / WIS, which among the six Zangband classes carrying
 * Nature is the Priest's and only the Priest's. So this is a derivation, and
 * these numbers are what it derived.
 *
 * Pinned because a donor is one word in the converter and nothing else would
 * notice it changing: every other donor gives a different first or last level
 * (Mage 1 and 40, Monk 1 and 48, High-Mage 1 and 39, Warrior-Mage 2 and 50,
 * Ranger 3 and 42, against the Priest's 2 and 44). The Ranger is checked
 * alongside for contrast -- it has its own row and must *not* share the Druid's.
 */
static int test_the_druid_casts_on_priest_figures(void *state) {
	static const struct {
		const char *cls, *first, *last;
		int first_level, first_mana, last_level, last_mana;
	} shape[] = {
		{ "Druid", "Detect Creatures", "Nature's Wrath", 2, 1, 44, 80 },
		{ "Ranger", "Detect Creatures", "Nature's Wrath", 3, 1, 42, 80 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(shape); i++) {
		const struct player_class *c = find_class(shape[i].cls);
		const struct class_spell *first = NULL, *last = NULL;
		int b, k, counted = 0;

		notnull(c);
		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			if (!book->realm || !streq(book->realm->name, "nature")) continue;
			for (k = 0; k < book->num_spells; k++) {
				if (!first) first = &book->spells[k];
				last = &book->spells[k];
				counted++;
			}
		}

		notnull(first);
		notnull(last);
		eq(counted, 32);
		require(streq(first->name, shape[i].first));
		require(streq(last->name, shape[i].last));
		eq(first->slevel, shape[i].first_level);
		eq(first->smana, shape[i].first_mana);
		eq(last->slevel, shape[i].last_level);
		eq(last->smana, shape[i].last_mana);
	}

	/*
	 * And the two are not the same slice. A donor bug that pointed the Druid
	 * at the Ranger would satisfy the shape above if both rows were read from
	 * one place, so the difference is asserted directly.
	 */
	require(shape[0].first_level != shape[1].first_level);

	ok;
}

/**
 * Every realm is four books of eight, and the Necromancer casts on Mage figures.
 *
 * The end of DEC-50, asserted as one shape rather than nine. Zangband holds
 * every realm as four books, so after Life, Arcane, Nature and Death were
 * replaced, **every casting class in the game is on a multiple of four books**.
 * A realm that came out with three, or a class left holding one of Angband's
 * five-book sets, fails here without anybody having to know which.
 *
 * Books of eight, but not always eight *learnable*: a class whose figures put a
 * spell above level 50 does not get it, so the Ranger's Chaos runs 8, 8, 7, 5
 * and its last book is genuinely short. That is Zangband's own arithmetic --
 * `usable()` drops what cannot be reached -- so the assertion is on the book
 * count and on no book exceeding eight, with the total pinned per class below.
 *
 * And the two Death classes are pinned by number, for the reason the Druid is:
 * both cast on borrowed figures under DEC-55, the donor is one word in the
 * converter, and the converter check cannot tell a wrong donor from a right
 * one. The Necromancer takes Zangband's Mage and the Blackguard its Paladin,
 * and their first and last spells differ, which is what proves the two are not
 * reading one row.
 */
static int test_every_realm_is_four_books_of_eight(void *state) {
	static const struct {
		const char *cls;
		int first_level, first_mana, last_level, last_mana;
	} borrowed[] = {
		{ "Necromancer", 1, 1, 47, 100 },
		{ "Blackguard", 1, 1, 50, 111 },
	};
	const struct player_class *c;
	size_t i;
	int classes_seen = 0;

	for (c = classes; c; c = c->next) {
		int b, realms_here = 0;
		bool counted[REALM_MAX] = { false };

		if (!c->magic.num_books) continue;
		classes_seen++;

		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			notnull(book->realm);
			require(book->num_spells > 0 && book->num_spells <= 8);
			if (!counted[book->realm->ridx]) {
				counted[book->realm->ridx] = true;
				realms_here++;
			}
		}

		eq(c->magic.num_books, realms_here * 4);

		/*
		 * And no more than the realm holds. Fewer is legitimate and means a
		 * spell out of reach at level 50; more would mean a book emitted
		 * twice.
		 */
		require(c->magic.total_spells <= realms_here * 32);
	}

	/*
	 * Twelve casting classes: Angband's nine, the Warrior-Mage and the
	 * High-Mage that PLR-03 carried over, and the Monk, whose casting DEC-36
	 * deferred until realm selection existed and which is the last of them.
	 */
	eq(classes_seen, 12);

	for (i = 0; i < N_ELEMENTS(borrowed); i++) {
		const struct player_class *k = find_class(borrowed[i].cls);
		const struct class_spell *first, *last;

		notnull(k);
		eq(k->magic.num_books, 4);
		first = &k->magic.books[0].spells[0];
		last = &k->magic.books[3].spells[k->magic.books[3].num_spells - 1];

		require(streq(first->name, "Detect Unlife"));
		require(streq(last->name, "Wraithform"));
		eq(first->slevel, borrowed[i].first_level);
		eq(first->smana, borrowed[i].first_mana);
		eq(last->slevel, borrowed[i].last_level);
		eq(last->smana, borrowed[i].last_mana);
	}

	/* Different rows, not one row read twice. */
	require(borrowed[0].last_level != borrowed[1].last_level);
	require(borrowed[0].last_mana != borrowed[1].last_mana);

	ok;
}

/**
 * The Warrior-Mage and the High-Mage, and what makes them different (PLR-03).
 *
 * M7 built three of Zangband's five extra classes and deferred these two,
 * because each is *defined* by which realms it may choose and realm choice did
 * not exist yet. It does, so here they are — and the thing worth testing is not
 * that they exist but that the difference between them survived the import,
 * because it lives entirely in the two `realm-choice:` lines.
 *
 * A **Warrior-Mage** always studies Arcane, in the first slot, and picks
 * anything else in the second: a fighter who casts, whose magic is the
 * generalist realm plus one specialism. A **High-Mage** picks one realm from
 * everything and gets no second slot at all — the specialist that gives up
 * breadth for the better figures Zangband's table hands it, which is the same
 * trade DEC-55 used to break the Necromancer's donor tie.
 *
 * Both are entitled to Trump, and neither is offered it, because it has no
 * books (DEC-54).
 */
static int test_the_two_carried_over_classes(void *state) {
	const struct player_class *wm = find_class("Warrior-Mage");
	const struct player_class *hm = find_class("High-Mage");
	const struct magic_realm *arcane = find_realm("arcane");
	const struct magic_realm *trump = find_realm("trump");
	const struct magic_realm *got[REALM_MAX];
	int n;

	notnull(wm);
	notnull(hm);
	notnull(arcane);
	notnull(trump);

	/* A Warrior-Mage's first realm is Arcane and there is no question. */
	eq(player_realm_choices(wm, 0, got, REALM_MAX), 1);
	require(got[0] == arcane);

	/*
	 * And its second is a genuine choice of six -- every built realm,
	 * Arcane included. Zangband lists Arcane in both of this class's slots,
	 * so a Warrior-Mage may take it twice and study one realm; that is the
	 * table's own answer and is left alone.
	 */
	eq(player_realm_choices(wm, 1, got, REALM_MAX), 6);
	for (n = 0; n < 6; n++) {
		require(got[n] != trump);
	}

	/* A High-Mage chooses one realm from six, and has no second slot. */
	eq(player_realm_choices(hm, 0, got, REALM_MAX), 6);
	eq(player_realm_choices(hm, 1, got, REALM_MAX), 0);

	/* Which is the difference, stated as a difference. */
	require(player_realm_choices(wm, 1, NULL, 0) > 0);
	eq(player_realm_choices(hm, 1, NULL, 0), 0);

	/*
	 * And the High-Mage's figures are better than the Warrior-Mage's, which
	 * is what it is buying with that missing slot. Zangband's spell_weight
	 * is the armour a caster tolerates, so lower is stricter -- the
	 * comparison that matters is the first spell's mana and the last
	 * spell's level in a realm they share.
	 */
	eq(wm->magic.spell_first, 1);
	eq(hm->magic.spell_first, 1);
	{
		const struct class_spell *w = NULL, *h = NULL;
		int b, k;

		for (b = 0; b < wm->magic.num_books; b++) {
			if (!streq(wm->magic.books[b].realm->name, "arcane")) continue;
			for (k = 0; k < wm->magic.books[b].num_spells; k++) {
				w = &wm->magic.books[b].spells[k];
			}
		}
		for (b = 0; b < hm->magic.num_books; b++) {
			if (!streq(hm->magic.books[b].realm->name, "arcane")) continue;
			for (k = 0; k < hm->magic.books[b].num_spells; k++) {
				h = &hm->magic.books[b].spells[k];
			}
		}
		notnull(w);
		notnull(h);
		require(streq(w->name, h->name));

		/* The specialist reaches the realm's last spell sooner. */
		require(h->slevel < w->slevel);
	}

	ok;
}

/**
 * The Monk punches and casts, and neither disables the other (PLR-03, PLR-08).
 *
 * DEC-36 built the Monk in M7 and deferred its casting: "In Zangband it casts,
 * choosing between Life and Nature. Realm selection is PLR-08, two milestones
 * out, so it ships as a pure martial class." Realm selection exists, so this is
 * the deferral discharged — and the prose was one realm short. `realm_choices1`
 * gives the Monk **Life, Nature *and* Death**, and `realm_choices2` gives it
 * nothing, so it picks one of three and gets no second slot.
 *
 * The interesting part is the coexistence. A Monk is the only class in the game
 * that is both `MARTIAL_ARTS` and a caster, and the two meet in one place:
 * Zangband weighs the *same six armour slots* for both, at
 * `100 + level * 4` for the martial penalty (`xtra1.c:2466`) and at
 * `spell_weight` for the mana penalty. The Monk's `spell_weight` is 300 — the
 * Mage's figure, not the Priest's 350 — so a Monk in heavy armour loses its
 * unarmed bonuses *and* its spell points together. That coupling is Zangband's
 * and this checks both halves are present rather than one having quietly
 * disabled the other.
 *
 * What is deliberately **not** here: `CHOOSE_SPELLS`. See
 * `the-monk-learns-at-random` below.
 *
 * The **spell points** are the substantive half. Dropping `NO_MANA` from the
 * class is hygiene rather than the thing that enables casting: the flag is
 * derived output, set by `calc_bonuses()` whenever `msp` is zero and read only
 * by monster AI deciding whether draining mana is worth it. It is never an
 * input to `calc_mana()`, so a class declaring it while carrying twelve books
 * merely contradicts itself until the next recalculation. Putting it back is
 * therefore caught by nothing, and correctly so -- which is why this asks the
 * game for a Monk's actual spell points instead.
 */
static int test_the_monk_punches_and_casts(void *state) {
	const struct player_class *monk = find_class("Monk");
	const struct magic_realm *got[REALM_MAX];
	int b, n;
	bool seen_life = false, seen_nature = false, seen_death = false;

	notnull(monk);

	/* Still a martial artist, and still has its blow ladder. */
	require(pf_has(monk->pflags, PF_MARTIAL_ARTS));
	notnull(monk->blows);

	/* And a caster: twelve books over exactly the three realms. */
	eq(monk->magic.num_books, 12);
	eq(class_book_realms(monk), 3);
	for (b = 0; b < monk->magic.num_books; b++) {
		const char *r = monk->magic.books[b].realm->name;

		if (streq(r, "life")) seen_life = true;
		else if (streq(r, "nature")) seen_nature = true;
		else if (streq(r, "death")) seen_death = true;
		else require(false);
	}
	require(seen_life && seen_nature && seen_death);

	/* One realm from three, and no second slot -- realm_choices2 is none. */
	n = player_realm_choices(monk, 0, got, REALM_MAX);
	eq(n, 3);
	eq(player_realm_choices(monk, 1, got, REALM_MAX), 0);

	/*
	 * Its own figures out of `magic_info[]`, not a donor's and not the
	 * Priest's: the Monk is one of Zangband's eleven classes. Pinned at both
	 * ends of each realm, because the three differ and a slice taken from the
	 * wrong row would still look plausible.
	 */
	eq(monk->magic.spell_first, 1);
	{
		static const struct {
			const char *realm, *last;
			int last_level, last_mana;
		} ends[] = {
			{ "life", "Holy Invulnerability", 50, 100 },
			{ "nature", "Nature's Wrath", 48, 85 },
			{ "death", "Wraithform", 50, 115 },
		};
		size_t i;

		for (i = 0; i < N_ELEMENTS(ends); i++) {
			const struct class_spell *first = NULL, *last = NULL;
			int k;

			for (b = 0; b < monk->magic.num_books; b++) {
				const struct class_book *book = &monk->magic.books[b];

				if (!streq(book->realm->name, ends[i].realm)) continue;
				for (k = 0; k < book->num_spells; k++) {
					if (!first) first = &book->spells[k];
					last = &book->spells[k];
				}
			}
			notnull(first);
			notnull(last);

			/* Every realm opens at level 1 for a Monk, and costs one point. */
			eq(first->slevel, 1);
			eq(first->smana, 1);

			require(streq(last->name, ends[i].last));
			eq(last->slevel, ends[i].last_level);
			eq(last->smana, ends[i].last_mana);
		}
	}

	/*
	 * The armour tolerance, which is where the two halves of the class meet.
	 * The Mage's 300 rather than the Priest's 350, so it is asserted against
	 * both to say which it is.
	 */
	eq(monk->magic.spell_weight, 300);
	{
		const struct player_class *mage = find_class("Mage");
		const struct player_class *priest = find_class("Priest");

		notnull(mage);
		notnull(priest);
		eq(monk->magic.spell_weight, mage->magic.spell_weight);
		require(monk->magic.spell_weight != priest->magic.spell_weight);
	}

	/*
	 * And it really has spell points, which is the whole claim. Asked of the
	 * game rather than of the data: a wrong `spell_first`, a `magic:` line
	 * that failed to parse, or a realm whose stat the Monk has none of would
	 * all leave the books in place and the pool empty.
	 */
	player_generate(player, NULL, monk, false);
	notnull(player->realm[0]);
	require(player_studies_realm(player, player->realm[0]));

	player->lev = player->max_lev = 20;
	calc_bonuses(player, &player->state, false, true);
	require(player->msp > 0);

	/* Still punching at that level, too: bare-handed blows, not one. */
	require(player->state.num_blows > 100);

	ok;
}

/**
 * A Monk learns at random, and a Warrior-Mage chooses (PLR-03).
 *
 * The gate nothing in the name advertises. Zangband has a `spell_type` field
 * that looks exactly like the chosen-versus-random switch -- 0 for the Monk, 1
 * for the Priest -- and **it is never read**. What actually decides is the
 * class's own `spell_book` constant: `do_cmd_study()` branches on
 * `mp_ptr->spell_book != TV_LIFE_BOOK` (`cmd5.c:339`), and the Monk's constant
 * is `TV_LIFE_BOOK` whatever realm it picks, because that field is never
 * reassigned from the chosen realm. So a Monk of Nature still learns at random,
 * like a Priest.
 *
 * The rule is trustworthy for the same reason DEC-55's key is: applied to the
 * six classes both games have, it agrees with Angband's own convention six
 * times out of six -- Priest and Paladin random, Mage, Rogue and Ranger chosen.
 *
 * It also found a defect. The Chaos-Warrior's constant is `TV_SORCERY_BOOK`,
 * so it chooses, and 3.58.0 shipped it without the flag -- learning at random
 * like a Priest for six versions.
 */
static int test_the_monk_learns_at_random(void *state) {
	static const struct {
		const char *cls;
		bool chooses;
	} shape[] = {
		/* TV_LIFE_BOOK in magic_info[] -- random. */
		{ "Monk", false },
		{ "Priest", false },
		{ "Paladin", false },
		/* TV_SORCERY_BOOK -- chosen. */
		{ "Chaos-Warrior", true },
		{ "Warrior-Mage", true },
		{ "High-Mage", true },
		{ "Mage", true },
		{ "Rogue", true },
		{ "Ranger", true },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(shape); i++) {
		const struct player_class *c = find_class(shape[i].cls);

		notnull(c);
		require(c->magic.num_books > 0);
		eq(pf_has(c->pflags, PF_CHOOSE_SPELLS), shape[i].chooses);
	}

	ok;
}

/**
 * Every entitlement a class has is backed by books (DEC-57).
 *
 * The assertion whose absence let a gap live for six versions.
 * `an-offered-realm-has-books-behind-it` checks one direction -- nothing is
 * offered that cannot be read -- and that direction is satisfied perfectly by
 * offering *less* than Zangband allows. Which is exactly what happened: the
 * three new realms were appended to the classes entitled to them, the four
 * mapped realms were only ever emitted into the classes that already carried
 * those books, and nobody compared the two. A Mage was entitled to seven realms
 * and could study three.
 *
 * So this is the other direction. The `realm-choice:` lines are Zangband's
 * `realm_choices1/2[]` transcribed verbatim -- `class-realm-entitlements-match-
 * zangband` above is what says so -- and a realm "has content" here if any
 * class in the game carries books of it. Put together: **if a class is entitled
 * to a realm and that realm has content, the class must have its books.**
 *
 * Trump is the one realm with no content, so it is entitled by six classes and
 * required of none, which is DEC-54 and is why the emptiness test is separate.
 */
static int test_every_entitlement_is_backed_by_books(void *state) {
	const struct player_class *c;
	const struct magic_realm *r;
	bool has_content[REALM_MAX] = { false };
	int required = 0, empty_realms = 0;

	/* Which realms anybody has books for. */
	for (c = classes; c; c = c->next) {
		int b;

		for (b = 0; b < c->magic.num_books; b++) {
			notnull(c->magic.books[b].realm);
			has_content[c->magic.books[b].realm->ridx] = true;
		}
	}

	for (r = realms; r; r = r->next) {
		if (!has_content[r->ridx]) empty_realms++;
	}

	/* Exactly one realm has no content, and DEC-54 says which. */
	eq(empty_realms, 1);
	{
		const struct magic_realm *trump = find_realm("trump");

		notnull(trump);
		require(!has_content[trump->ridx]);
	}

	for (c = classes; c; c = c->next) {
		int slot;

		for (slot = 0; slot < REALM_CHOICES; slot++) {
			for (r = realms; r; r = r->next) {
				if (!c->magic.realm_allowed[slot][r->ridx]) continue;
				if (!has_content[r->ridx]) continue;

				require(class_has_realm_book(c, r));
				required++;
			}
		}
	}

	/*
	 * Forty-eight entitlement-and-content pairs across the twelve casting
	 * classes -- counted so that a loop which checked nothing would pass
	 * nothing. It is the same forty-eight `an-offered-realm-has-books-
	 * behind-it` counts, because both walk slot by realm; the two tests
	 * differ in what they assert, not in what they walk.
	 *
	 * And that difference is the point. That test asks whether everything
	 * *offered* has books, which `player_realm_choices()` guarantees by
	 * construction -- it skips realms without books, so that half of it is
	 * circular. This one asks the question the filter cannot answer: of the
	 * realms Zangband *entitles* the class to, and which exist in this
	 * game, does the class have the books? Nothing asked that until DEC-57,
	 * which is how a Mage came to be entitled to seven realms and able to
	 * study three.
	 */
	eq(required, 48);

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
	{ "the-monk-punches-and-casts", test_the_monk_punches_and_casts },
	{ "the-monk-learns-at-random", test_the_monk_learns_at_random },
	{ "the-two-carried-over-classes", test_the_two_carried_over_classes },
	{ "every-realm-is-four-books-of-eight",
	  test_every_realm_is_four_books_of_eight },
	{ "the-druid-casts-on-priest-figures",
	  test_the_druid_casts_on_priest_figures },
	{ "arcane-is-bought-in-town", test_arcane_is_bought_in_town },
	{ "a-spell-knows-its-place-in-its-realm",
	  test_a_spell_knows_its_place_in_its_realm },
	{ "a-chaos-warrior-casts-chaos", test_a_chaos_warrior_casts_chaos },
	{ "every-entitlement-is-backed-by-books",
	  test_every_entitlement_is_backed_by_books },
	{ "an-offered-realm-has-books-behind-it",
	  test_an_offered_realm_has_books_behind_it },
	{ "studying-is-a-property-of-the-character",
	  test_studying_is_a_property_of_the_character },
	{ "a-spell-without-an-effect-says-so",
	  test_a_spell_without_an_effect_says_so },
	{ "the-realm-filter-sorts-three-realms",
	  test_the_realm_filter_sorts_three_realms },
	{ NULL, NULL }
};
