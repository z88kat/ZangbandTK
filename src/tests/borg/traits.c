/* borg/traits
 *
 * What the borg believes it is carrying (ZangbandTK, review of 3.105-3.124).
 *
 * `borg_notice()` turns the pack and the equipment into two hundred integers
 * in `borg.trait[]`, and every decision the borg makes afterwards reads those
 * integers rather than the objects. A trait that is computed wrongly is
 * therefore invisible: the borg behaves oddly, nothing crashes, and the fault
 * is four hundred lines away from the behaviour.
 *
 * Both faults covered here had exactly that shape, and both were found by
 * reading rather than by any test failing.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "init.h"
#include "object.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-util.h"
#include "player.h"
#include "obj-desc.h"
#include "player-birth.h"
#include "z-virt.h"

#include "borg/borg.h"
#include "borg/borg-init.h"
#include "borg/borg-item.h"
#include "borg/borg-item-analyze.h"
#include "borg/borg-flow-kill.h"
#include "borg/borg-item-val.h"
#include "borg/borg-trait.h"
#include "borg/borg-think.h"

int setup_tests(void **state)
{
	set_file_paths();
	init_angband();

	if (!player_make_simple(NULL, "Warrior", "Tester")) {
		printf("failed to make a character\n");
		return 1;
	}

	if (!borg.trait)
		borg_trait_init();
	if (!borg_items)
		borg_init_item();
	if (!borg_cfg) {
		int i;
		borg_cfg = mem_alloc(sizeof(int) * BORG_MAX_SETTINGS);
		for (i = 0; i < BORG_MAX_SETTINGS; i++)
			borg_cfg[i] = borg_settings[i].default_value;
	}
	borg_init_item_val();
	/*
	 * `borg_notice_player()` ends by reading `borg_race_death[]`, which is
	 * allocated here and is a null pointer until it is. Nothing below is
	 * about monsters; this is what the real borg has running by the time
	 * anything calls `borg_notice()`.
	 */
	borg_init_flow_kill();

	/*
	 * `borg_notice()` re-reads the spell list when this is set, and that
	 * path wants a game in progress. Nothing here is about spells.
	 */
	borg_do_spell = false;

	*state = NULL;
	return 0;
}

int teardown_tests(void *state)
{
	borg_free_flow_kill();
	borg_free_item();
	borg_trait_free();
	mem_free(borg_cfg);
	borg_cfg = NULL;
	cleanup_angband();
	return 0;
}

/** Nothing in the pack or the equipment, so one item can be put back. */
static void an_empty_borg(void)
{
	memset(borg_items, 0, QUIVER_END * sizeof(borg_item));
}

/** That object, in that slot of the borg's idea of the character. */
static void borg_carries(int slot, struct object *obj)
{
	char buf[320];

	object_desc(buf, sizeof(buf), obj, ODESC_FULL, player);
	borg_item_analyze(&borg_items[slot], obj, buf, false);
}

/**
 * A weapon's brand is read as the element it actually is.
 *
 * `item->brands[]` is indexed by brand index -- one-based, and in reverse of
 * `brand.txt`, because the parser prepends -- and `borg_notice_equipment()`
 * indexed it by *element*. Every one of the five answers was wrong:
 * `brands[ELEM_ACID]` is slot 0, which is never a brand, so an acid brand was
 * invisible; the other four read the tail of the file, so a weapon of Venom
 * was reported as lightning, Frost as fire, Flame as cold and Lightning as
 * poison.
 *
 * Driven by brand *code* rather than by index, because an index is what went
 * wrong: a test that said "slot 9 sets BI_WB_ACID" would have to be edited
 * whenever `brand.txt` changed and would pin the bug just as happily as the
 * fix. Each of the ten brands is tried, and the other four traits are required
 * to stay clear -- "the right one is set" alone passes for code that sets all
 * five.
 */
static int test_a_brand_is_read_as_its_own_element(void *state) {
	static const struct {
		const char *code;
		int trait;
	} want[] = {
		{ "ACID", BI_WB_ACID }, { "ELEC", BI_WB_ELEC },
		{ "FIRE", BI_WB_FIRE }, { "COLD", BI_WB_COLD },
		{ "POIS", BI_WB_POIS },
	};
	int bi, w, tried = 0;

	for (bi = 1; bi < z_info->brand_max; bi++) {
		struct object *obj = object_new();
		struct object_kind *kind = lookup_kind(TV_SWORD,
				lookup_sval(TV_SWORD, "Main Gauche"));
		int expect = -1;

		notnull(kind);
		object_prep(obj, kind, 1, RANDOMISE);
		obj->number = 1;
		obj->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
		obj->brands[bi] = true;
		obj->known = object_new();
		object_set_base_known(player, obj);
		object_flavor_aware(player, obj);

		/*
		 * The brand goes on the *known* object too, which is the part that
		 * matters. `borg_item_analyze()` reads `real_item->known` for
		 * anything short of fully identified, and a blank known object
		 * carries no brands at all -- so without this the borg sees nothing
		 * and the test passes for the wrong reason whatever the code does.
		 * A player who has learnt the brand has it recorded here, so this is
		 * the state being described rather than a convenience.
		 */
		obj->known->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
		obj->known->brands[bi] = true;

		for (w = 0; w < (int) N_ELEMENTS(want); w++)
			if (!strncmp(brands[bi].code, want[w].code, 4))
				expect = want[w].trait;
		require(expect >= 0);

		an_empty_borg();
		borg_carries(INVEN_WIELD, obj);
		borg_notice(false);

		for (w = 0; w < (int) N_ELEMENTS(want); w++) {
			int got = borg.trait[want[w].trait] ? 1 : 0;

			if (got != (want[w].trait == expect ? 1 : 0)) {
				printf("  brand %s (index %d): %s is %d\n",
						brands[bi].code, bi,
						want[w].code, got);
				require(false);
			}
		}
		tried++;

		object_delete(NULL, NULL, &obj->known);
		object_delete(NULL, NULL, &obj);
	}

	/* And it really did look at all ten, rather than at an empty list. */
	eq(tried, z_info->brand_max - 1);
	ok;
}

/**
 * A scroll of Remove Hunger is food.
 *
 * `BI_FOOD` is the sum of the two calorie counts and those were fed only by
 * `TV_FOOD` and `TV_MUSHROOM`, so a character carrying nothing but Remove
 * Hunger scrolls read as having no food at all -- which is every undead race,
 * since all five take `equip-instead:food:scroll:Remove Hunger:2:5` in place
 * of rations.
 *
 * Five of them against the threshold `borg_prepared()` actually uses, rather
 * than against zero: "more than none" would pass for a change that counted
 * each stack as one.
 */
static int test_remove_hunger_scrolls_are_food(void *state) {
	struct object *obj = object_new();
	struct object_kind *kind = lookup_kind(TV_SCROLL,
			lookup_sval(TV_SCROLL, "Remove Hunger"));

	notnull(kind);
	object_prep(obj, kind, 1, RANDOMISE);
	obj->number = 5;
	obj->known = object_new();
	object_set_base_known(player, obj);
	object_flavor_aware(player, obj);

	an_empty_borg();
	borg_notice(false);
	eq(borg.trait[BI_FOOD], 0);

	borg_carries(0, obj);
	borg_notice(false);
	require(borg.trait[BI_FOOD] >= 5);

	object_delete(NULL, NULL, &obj->known);
	object_delete(NULL, NULL, &obj);
	ok;
}

/**
 * The borg can roll every race the game has.
 *
 * `MAX_RACES` was 11 against this game's 28, so reincarnation could never
 * produce any race PLR-01 added and `borg_init()` refused to be configured
 * into one. Asserted as a property of the list rather than as the number 28,
 * which would have to be edited by the next person to add a race and would
 * then be just as wrong as the constant was.
 */
static int test_the_borg_can_roll_every_race(void *state) {
	const struct player_race *r;
	int n = 0;

	for (r = races; r; r = r->next) n++;

	/* Every race is reachable ... */
	eq(borg_player_race_count(), n);

	/* ... and every index it can roll is a race. */
	for (n = 0; n < borg_player_race_count(); n++)
		notnull(player_id2race(n));

	/* One past the end is not, so the bound is exclusive as rolled. */
	require(!player_id2race(borg_player_race_count()));

	/* And there is more here than Angband's eleven, which is the point. */
	require(borg_player_race_count() > 11);
	ok;
}

const char *suite_name = "borg/traits";
struct test tests[] = {
	{ "a-brand-is-read-as-its-own-element",
	  test_a_brand_is_read_as_its_own_element },
	{ "remove-hunger-scrolls-are-food",
	  test_remove_hunger_scrolls_are_food },
	{ "the-borg-can-roll-every-race",
	  test_the_borg_can_roll_every_race },
	{ NULL, NULL }
};
