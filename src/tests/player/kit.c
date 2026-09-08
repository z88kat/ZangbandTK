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
#include <stdio.h>

#include "init.h"
#include "object.h"
#include "player.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-birth.h"
#include "player-spell.h"
#include "generate.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	/*
	 * A level, so that births after the first have somewhere to draw on.
	 *
	 * `calc_light()` sets PU_MONSTERS when a character's light differs from
	 * the last one's, and the Vampire is the first race to carry a light of
	 * its own -- so the birth *after* a Vampire's is the first that ever
	 * needed a cave to update. Real play always has one; this suite did not.
	 */
	if (!player_make_simple(NULL, NULL, "Tester")) return 1;
	prepare_next_level(player);
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

/** How many of this object kind the character is carrying. */
static int carried(int tval, const char *sval_name) {
	int sval = lookup_sval(tval, sval_name);
	int i, n = 0;

	if (sval < 0) return -1;

	for (i = 0; i < z_info->pack_size; i++) {
		struct object *obj = player->upkeep->inven[i];
		if (obj && obj->tval == tval && obj->sval == sval) n += obj->number;
	}

	return n;
}

/** Everything of this tval the character is carrying. */
static int carried_tval(int tval) {
	int i, n = 0;

	for (i = 0; i < z_info->pack_size; i++) {
		struct object *obj = player->upkeep->inven[i];
		if (obj && obj->tval == tval) n += obj->number;
	}

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

/*
 * A race can take something else in place of part of the kit (PLR-01).
 *
 * Zangband decides food and light by race before it looks at the class
 * ([birth.c:557](../archive/zangband/src/birth.c#L557)). A Vampire gets scrolls
 * of Satisfy Hunger instead of rations -- food is worth a tenth to it -- and
 * scrolls of Darkness instead of torches, which are the only shelter it has
 * from the sun. A Golem takes the food swap and keeps its torches.
 *
 * Checked as a substitution rather than an addition: the displaced item must be
 * *gone*. A version that granted the scrolls and left the rations would look
 * right in an inventory listing and be wrong.
 */
static int test_a_race_can_pack_something_else(void *state) {
	require(player_make_simple("Vampire", "Warrior", "Tester"));
	require(carried(TV_SCROLL, "Remove Hunger") >= 2);
	require(carried(TV_SCROLL, "Darkness") >= 2);
	require(carried_tval(TV_FOOD) == 0);
	require(carried_tval(TV_LIGHT) == 0);

	/* The Golem trades food only */
	require(player_make_simple("Golem", "Warrior", "Tester"));
	require(carried(TV_SCROLL, "Remove Hunger") >= 2);
	require(carried_tval(TV_FOOD) == 0);
	require(carried_tval(TV_LIGHT) > 0);
	require(carried(TV_SCROLL, "Darkness") == 0);

	/* And nobody else is touched */
	require(player_make_simple("Human", "Warrior", "Tester"));
	require(carried_tval(TV_FOOD) > 0);
	require(carried_tval(TV_LIGHT) > 0);
	require(carried(TV_SCROLL, "Remove Hunger") == 0);
	require(carried(TV_SCROLL, "Darkness") == 0);
	ok;
}

/*
 * And the grant does not depend on the class having had one to replace.
 *
 * The Necromancer is the only class with no light in its kit -- it is the
 * unlight class and starts without one on purpose. Zangband's grant is by race
 * and never consults the class, so a Vampire Necromancer still gets its
 * scrolls. Under a substitution that only edited existing entries it would get
 * none, and the one class/race pairing that most needs them would be the one
 * pairing that silently missed out.
 */
static int test_the_substitution_does_not_need_something_to_replace(void *state) {
	require(player_make_simple("Human", "Necromancer", "Tester"));
	require(carried_tval(TV_LIGHT) == 0);

	require(player_make_simple("Vampire", "Necromancer", "Tester"));
	require(carried(TV_SCROLL, "Darkness") >= 2);
	ok;
}

/*
 * Every race that cannot use food carries something that works instead.
 *
 * Driven from the data rather than a list written here, because the list is
 * the thing that goes stale: six races take this substitution today and a
 * seventh would be a data block with no code change, so a hand-written roster
 * would pass while the new race quietly starved. Reading `CANT_EAT` and
 * `BLOOD_DIET` off the race and then checking the pack is the version that
 * fails when somebody forgets the line -- which is exactly how this test came
 * to be written, a falsification having removed the Ghoul's and gone unnoticed.
 */
static int test_no_race_starves_for_want_of_a_kit_line(void *state) {
	struct player_race *r;
	int checked = 0;

	for (r = races; r; r = r->next) {
		bool cannot_eat = of_has(r->flags, OF_CANT_EAT);
		bool wants_blood = pf_has(r->pflags, PF_BLOOD_DIET);

		if (!cannot_eat && !wants_blood) continue;

		require(player_make_simple(r->name, "Warrior", "Tester"));
		if (carried(TV_SCROLL, "Remove Hunger") < 2) {
			printf("%s starts with no hunger scrolls\n", r->name);
		}
		require(carried(TV_SCROLL, "Remove Hunger") >= 2);
		eq(carried_tval(TV_FOOD), 0);
		checked++;
	}

	/* Golem, Vampire, Skeleton, Zombie, Spectre, Ghoul */
	require(checked >= 6);
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
	{ "no-race-starves-for-want-of-a-kit-line",
			test_no_race_starves_for_want_of_a_kit_line },
	{ "a-race-can-pack-something-else",
			test_a_race_can_pack_something_else },
	{ "the-substitution-does-not-need-something-to-replace",
			test_the_substitution_does_not_need_something_to_replace },
	{ "the-bookless-classes-stay-bookless",
	  test_the_bookless_classes_stay_bookless },
	{ NULL, NULL }
};
