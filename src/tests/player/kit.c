/* player/kit — what each class begins the game holding (PLR-03, PLR-08)
 *
 * Nothing pinned starting equipment before this file, and it cost two classes
 * their spellbook without anybody noticing. `d1eadd951` and `0847955a7` each
 * renamed a book for one class during the realm import and deleted another
 * class's line in the same edit: the Priest lost `[Novice's Handbook]` and the
 * Mage lost `[First Spells]`. Both commits are about realms and read as
 * correct; the loss is one deleted line each, in a file of several thousand.
 *
 * So the tests here are deliberately blunt. They do not check that the kit is
 * *good*, only that every class still has one and that every caster can cast.
 * A test that would have caught that regression is worth more than one that
 * describes the kit beautifully.
 */
#include "unit-test.h"

#include "init.h"
#include "object.h"
#include "player.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "player-birth.h"
#include "player-spell.h"
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

/** Make a character of this class, run through the real birth commands. */
static bool born_as(const char *class_name) {
	return player_make_simple(NULL, class_name, "Tester");
}

/** How many books of this realm the character is carrying. */
static int books_of_realm(const struct magic_realm *realm) {
	int i, n = 0;

	for (i = 0; i < z_info->pack_size; i++) {
		struct object *obj = player->upkeep->inven[i];
		const struct class_book *book;

		if (!obj) continue;

		book = player_object_to_book(player, obj);
		if (book && book->realm == realm) n += obj->number;
	}

	return n;
}

/** Everything the character is holding or wearing. */
static int things_held(void) {
	int i, n = 0;

	for (i = 0; i < z_info->pack_size; i++)
		if (player->upkeep->inven[i]) n++;

	for (i = 0; i < player->body.count; i++)
		if (slot_object(player, i)) n++;

	return n;
}

/**
 * Every class begins with something.
 *
 * The floor, not the shape. A class whose `equip:` lines were lost wholesale
 * would arrive here naked and this would say so.
 */
static int test_every_class_starts_with_a_kit(void *state) {
	const struct player_class *c;
	int checked = 0;

	for (c = classes; c; c = c->next) {
		require(born_as(c->name));

		if (things_held() < 3) {
			printf("  %s starts with %d things\n", c->name, things_held());
			require(things_held() >= 3);
		}

		checked++;
	}

	require(checked >= 14);
	ok;
}

/**
 * Every caster begins able to cast (PLR-08).
 *
 * The invariant the regression broke. A character that studies a realm and
 * holds no book of it is told "Study (1)" on the status bar with nothing to
 * study from -- which is what a level 1 Mage did for fifty releases.
 *
 * Asked per realm slot rather than "holds at least one book", because Zangband
 * gives Mage, Priest, Ranger and Warrior-Mage one for each of their two, and
 * "at least one" would pass while half the character was missing.
 */
static int test_every_caster_starts_with_a_book_of_each_realm(void *state) {
	const struct player_class *c;

	for (c = classes; c; c = c->next) {
		int slot;

		require(born_as(c->name));

		for (slot = 0; slot < REALM_CHOICES; slot++) {
			const struct magic_realm *realm = player->realm[slot];

			if (!realm) continue;

			if (books_of_realm(realm) < 1) {
				printf("  %s studies %s and holds no book of it\n",
					   c->name, realm->name);
				require(books_of_realm(realm) >= 1);
			}
		}
	}

	ok;
}

/**
 * And the book is the one for the realm they actually took.
 *
 * A fixed `equip:` line cannot do this: it names one object kind, and the
 * realm is not known until the player picks it. The Rogue's line said
 * `[Cantrips for Beginners]`, so a Rogue who chose Death started with an
 * arcane book it could not open -- wrong in exactly the way the six bookless
 * classes were missing.
 *
 * Every book held must belong to a realm this character studies. Nothing else
 * is a spellbook as far as they are concerned.
 */
static int test_no_class_starts_with_a_book_it_cannot_read(void *state) {
	const struct player_class *c;

	for (c = classes; c; c = c->next) {
		int i;

		require(born_as(c->name));

		for (i = 0; i < z_info->pack_size; i++) {
			struct object *obj = player->upkeep->inven[i];

			if (!obj || !tval_is_book_k(obj->kind)) continue;

			/* `player_object_to_book()` applies the realm test itself */
			if (!player_object_to_book(player, obj)) {
				char name[80];
				object_desc(name, sizeof(name), obj, ODESC_BASE, player);
				printf("  %s starts with %s, which it cannot open\n",
					   c->name, name);
				require(player_object_to_book(player, obj));
			}
		}
	}

	ok;
}

/**
 * A class with two realm slots takes two different realms.
 *
 * Zangband excludes the first realm from the second list
 * ([birth.c:963](../archive/zangband/src/birth.c#L963)); ours could not,
 * because the function answering "what may I pick" saw only the class. A Mage
 * defaulted to Arcane in both slots, which is thirty-two spells where it
 * should be sixty-four, and one book where Zangband gives two.
 */
static int test_two_realm_slots_take_two_realms(void *state) {
	static const char *const two[] = {
		"Mage", "Priest", "Ranger", "Warrior-Mage"
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(two); i++) {
		require(born_as(two[i]));

		notnull(player->realm[0]);
		notnull(player->realm[1]);
		require(player->realm[0] != player->realm[1]);

		/* And therefore two books */
		eq(books_of_realm(player->realm[0])
		   + books_of_realm(player->realm[1]), 2);
	}

	ok;
}

/**
 * The classes that study nothing carry no book.
 *
 * The other direction, and not a formality: granting by realm slot would give
 * a book to anything with a realm set, and a Warrior having one would mean the
 * slot was being filled for a class that should have none.
 */
static int test_the_bookless_classes_stay_bookless(void *state) {
	static const char *const none[] = { "Warrior", "Mindcrafter" };
	size_t i;
	int j;

	for (i = 0; i < N_ELEMENTS(none); i++) {
		require(born_as(none[i]));

		for (j = 0; j < REALM_CHOICES; j++)
			null(player->realm[j]);

		for (j = 0; j < z_info->pack_size; j++) {
			struct object *obj = player->upkeep->inven[j];
			if (obj) require(!tval_is_book_k(obj->kind));
		}
	}

	ok;
}

const char *suite_name = "player/kit";
struct test tests[] = {
	{ "every-class-starts-with-a-kit",
	  test_every_class_starts_with_a_kit },
	{ "every-caster-starts-with-a-book-of-each-realm",
	  test_every_caster_starts_with_a_book_of_each_realm },
	{ "no-class-starts-with-a-book-it-cannot-read",
	  test_no_class_starts_with_a_book_it_cannot_read },
	{ "two-realm-slots-take-two-realms",
	  test_two_realm_slots_take_two_realms },
	{ "the-bookless-classes-stay-bookless",
	  test_the_bookless_classes_stay_bookless },
	{ NULL, NULL }
};
