/* monster/charm
 *
 * Where pets come from (ZangbandTK, PLR-28, PLR-32).
 *
 * PLR-28 lists the documented paths: "some magic realms offer the ability to
 * summon pets magically or to charm the creatures you meet. Mindcrafters may
 * 'dominate' their opponents. Chaos patrons may grant pets as a gift to their
 * devotees. Magical figurines can be thrown to create a pet and wands of charm
 * monster may be used as their name suggests."
 *
 * Underneath all of them are two mechanisms: charming something that is already
 * there, and summoning something that arrives on your side. This suite is about
 * those two, and about PLR-32's rule that a summon inherits its summoner's
 * side -- which is the reason PLR-30's upkeep is charged on the *sum* of pet
 * levels and the reason Zangband's documentation warns about "a pet which can
 * summon or otherwise produce more pets".
 *
 * The charm saving throw is Zangband's, with DEC-61's measured identity
 * substituted: `level > randint1(power * 3)` where the source writes
 * `hdice * 2 > randint1(dam * 3)`. Three things are immune outright -- uniques,
 * questors, and (for the general charm only) minds that cannot be confused --
 * and aggravation refuses separately, after the roll succeeds, because it is
 * not a resistance. It tells the player what to take off.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "project.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Ranger", "Tester")) return 1;
	prepare_next_level(player);
	on_new_level();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** A hostile monster of that race beside the player, or NULL. */
static struct monster *put_beside(const char *name) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int i;

	if (!race) return NULL;

	for (i = 0; i < 8; i++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		return square_monster(cave, grid);
	}

	return NULL;
}

/** Project a charm of that type and power at one monster. */
static void charm_at(struct monster *mon, int type, int power) {
	int flags = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY;

	(void) project(source_player(), 0, mon->grid, power, type, flags, 0, 0,
				   NULL);
}

/** How many pets are on the level. */
static int pets_here(void) {
	int i, n = 0;

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race && monster_is_pet(mon)) n++;
	}

	return n;
}

/**
 * A charm at overwhelming power takes.
 *
 * The floor. Power 200 against a shallow monster beats the saving throw
 * essentially always, so a failure here is the mechanism not working rather
 * than a bad roll.
 */
static int test_a_strong_charm_takes(void *state) {
	struct monster *mon;

	clear_the_level();
	mon = put_beside("kobold");
	require(mon);
	require(monster_is_hostile(mon));

	charm_at(mon, PROJ_MON_CHARM, 200);

	require(monster_is_pet(mon));

	ok;
}

/**
 * A unique cannot be charmed, however hard you try.
 *
 * The same overwhelming power. Without this a summoner could simply take the
 * most dangerous thing on the level into service, which is the failure mode
 * every one of Zangband's charm effects guards against first.
 *
 * Mughash rather than Grip, and the difference matters: Grip carries NO_CONF,
 * which the general charm also refuses, so a test using it passed against a
 * build with the unique check deliberately removed. It was testing the wrong
 * rule and only the falsification showed it. Mughash is a unique and nothing
 * else, so the only thing that can refuse him is being a unique.
 */
static int test_a_unique_cannot_be_charmed(void *state) {
	struct monster *mon;

	clear_the_level();
	mon = put_beside("Mughash the Kobold Lord");
	require(mon);
	require(monster_is_unique(mon));
	require(!rf_has(mon->race->flags, RF_NO_CONF));
	require(!rf_has(mon->race->flags, RF_QUESTOR));

	charm_at(mon, PROJ_MON_CHARM, 200);

	require(monster_is_hostile(mon));

	ok;
}

/**
 * A mind that cannot be confused cannot be talked round either.
 *
 * NO_CONF is a resistance to the general charm and to neither of the other
 * two, which is Zangband's arrangement: an animal is tamed and the dead are
 * commanded, and neither of those is persuasion.
 */
static int test_no_conf_resists_the_general_charm(void *state) {
	struct monster *mon;

	clear_the_level();
	mon = put_beside("grave wight");
	require(mon);
	require(rf_has(mon->race->flags, RF_NO_CONF));

	charm_at(mon, PROJ_MON_CHARM, 200);
	require(monster_is_hostile(mon));

	/* But the undead charm reaches it, because it is undead */
	require(rf_has(mon->race->flags, RF_UNDEAD));
	charm_at(mon, PROJ_MON_CHARM_UNDEAD, 200);
	require(monster_is_pet(mon));

	ok;
}

/**
 * The animal charm only works on animals, and the undead charm on the undead.
 *
 * Each tested against something of the wrong kind at a power that would
 * otherwise be overwhelming, so the refusal is the eligibility rule and not a
 * saving throw.
 */
static int test_the_narrow_charms_are_narrow(void *state) {
	struct monster *animal, *person;

	clear_the_level();
	animal = put_beside("large white snake");
	require(animal);
	require(rf_has(animal->race->flags, RF_ANIMAL));
	require(!rf_has(animal->race->flags, RF_UNDEAD));

	/* The undead charm will not have it */
	charm_at(animal, PROJ_MON_CHARM_UNDEAD, 200);
	require(monster_is_hostile(animal));

	/* The animal charm will */
	charm_at(animal, PROJ_MON_CHARM_ANIMAL, 200);
	require(monster_is_pet(animal));

	clear_the_level();
	person = put_beside("apprentice");
	require(person);
	require(!rf_has(person->race->flags, RF_ANIMAL));

	charm_at(person, PROJ_MON_CHARM_ANIMAL, 200);
	require(monster_is_hostile(person));

	ok;
}

/**
 * Aggravation refuses after the roll, not before it.
 *
 * Not a resistance: the charm worked and the creature will still not have you.
 * Zangband's own wording -- "hates you too much" -- and the reason it is worth
 * keeping separate is that it tells the player the problem is something they
 * are carrying rather than something the target is.
 */
static int test_aggravation_refuses_the_charm(void *state) {
	struct monster *mon;

	clear_the_level();
	mon = put_beside("kobold");
	require(mon);

	of_on(player->state.flags, OF_AGGRAVATE);
	charm_at(mon, PROJ_MON_CHARM, 200);
	of_off(player->state.flags, OF_AGGRAVATE);

	require(monster_is_hostile(mon));

	/* And without it, the same charm takes */
	charm_at(mon, PROJ_MON_CHARM, 200);
	require(monster_is_pet(mon));

	ok;
}

/**
 * A weak charm mostly fails, and a strong one mostly works.
 *
 * The saving throw is `level > randint1(power * 3)`, so the odds are a
 * property of the two numbers and not a constant. Measured over many attempts
 * rather than asserted on one roll: a single trial cannot distinguish a
 * working formula from a coin.
 *
 * Against a level 12 monster, power 4 gives a 12-in-12 chance of the monster
 * winning the roll... but `randint1(12)` beats 12 never, so it fails always;
 * power 40 gives it one chance in ten. The bands here are loose enough that no
 * plausible run trips them and tight enough that a constant would.
 */
static int test_the_saving_throw_scales(void *state) {
	int weak = 0, strong = 0, i;

	for (i = 0; i < 60; i++) {
		struct monster *mon;

		clear_the_level();
		mon = put_beside("priest");
		require(mon);
		require(mon->race->level == 12);

		charm_at(mon, PROJ_MON_CHARM, 2);
		if (monster_is_pet(mon)) weak++;
	}

	for (i = 0; i < 60; i++) {
		struct monster *mon;

		clear_the_level();
		mon = put_beside("priest");
		require(mon);

		charm_at(mon, PROJ_MON_CHARM, 40);
		if (monster_is_pet(mon)) strong++;
	}

	/* A power of 2 cannot beat level 12: randint1(6) is never above it */
	eq(weak, 0);

	/* A power of 40 beats it about nine times in ten */
	require(strong > 40);

	ok;
}

/**
 * A summoned pet arrives on the player's side, and an ordinary summon does not.
 *
 * Both halves, because an effect that made everything a pet would satisfy the
 * first.
 */
static int test_a_summoned_pet_is_a_pet(void *state) {
	int before, after_pet, after_plain;

	clear_the_level();
	before = pets_here();

	effect_simple(EF_SUMMON_PET, source_player(), "3", summon_name_to_idx("ANIMAL"),
				  0, 0, 0, 0, NULL);
	after_pet = pets_here();
	require(after_pet > before);

	clear_the_level();
	effect_simple(EF_SUMMON, source_player(), "3", summon_name_to_idx("ANIMAL"),
				  0, 0, 0, 0, NULL);
	after_plain = pets_here();
	eq(after_plain, 0);

	ok;
}

/**
 * A pet's summons are pets (PLR-32).
 *
 * The rule that makes the upkeep matter: without it a summoning pet builds the
 * player a free army of hostile monsters, and with it the army costs. Driven
 * through `summon_specific()` with `cave->mon_current` set, which is what the
 * game does when a monster takes its turn.
 */
static int test_a_summons_side_is_its_summoners(void *state) {
	struct monster *summoner;
	int i, pets, hostiles;

	clear_the_level();
	summoner = put_beside("large white snake");
	require(summoner);
	monster_set_allegiance(summoner, MON_ALLEGIANCE_PET);

	/* Summon as the game does: the summoner is the monster taking its turn */
	cave->mon_current = summoner->midx;
	for (i = 0; i < 6; i++) {
		(void) summon_specific(summoner->grid, 5,
							   summon_name_to_idx("ANIMAL"), false, false);
	}
	cave->mon_current = -1;

	pets = pets_here();
	require(pets > 1);

	/* Now the same from a hostile summoner */
	clear_the_level();
	summoner = put_beside("large white snake");
	require(summoner);
	require(monster_is_hostile(summoner));

	cave->mon_current = summoner->midx;
	for (i = 0; i < 6; i++) {
		(void) summon_specific(summoner->grid, 5,
							   summon_name_to_idx("ANIMAL"), false, false);
	}
	cave->mon_current = -1;

	hostiles = pets_here();
	eq(hostiles, 0);

	ok;
}

/**
 * The spells and powers that were waiting on this are wired up.
 *
 * Five realm spells, one wand and three mutations came off the deferred list
 * when allegiance arrived. Checked as data rather than as behaviour: each
 * either carries an effect chain or it does not, and the ones that do are the
 * ones this milestone was supposed to unblock.
 */
static int test_the_deferred_content_arrived(void *state) {
	static const char *const spells[] = {
		"Day of the Dove", "Animal Taming", "Animal Friendship",
		"Summon Animal", "Enslave Undead", "Summon Demon"
	};
	const struct player_class *c;
	int found = 0;
	size_t i;

	for (c = classes; c; c = c->next) {
		int j, k;

		for (j = 0; j < c->magic.num_books; j++) {
			const struct class_book *b = &c->magic.books[j];

			for (k = 0; k < b->num_spells; k++) {
				const struct class_spell *sp = &b->spells[k];

				for (i = 0; i < N_ELEMENTS(spells); i++) {
					if (!streq(sp->name, spells[i])) continue;

					/* Every copy of every one of them does something now */
					notnull(sp->effect);
					found++;
				}
			}
		}
	}

	/* Six spells across the classes entitled to their realms */
	require(found >= (int) N_ELEMENTS(spells));

	ok;
}

const char *suite_name = "monster/charm";
struct test tests[] = {
	{ "a-strong-charm-takes", test_a_strong_charm_takes },
	{ "a-unique-cannot-be-charmed", test_a_unique_cannot_be_charmed },
	{ "no-conf-resists-the-general-charm",
	  test_no_conf_resists_the_general_charm },
	{ "the-narrow-charms-are-narrow", test_the_narrow_charms_are_narrow },
	{ "aggravation-refuses-the-charm", test_aggravation_refuses_the_charm },
	{ "the-saving-throw-scales", test_the_saving_throw_scales },
	{ "a-summoned-pet-is-a-pet", test_a_summoned_pet_is_a_pet },
	{ "a-summons-side-is-its-summoners",
	  test_a_summons_side_is_its_summoners },
	{ "the-deferred-content-arrived", test_the_deferred_content_arrived },
	{ NULL, NULL }
};
