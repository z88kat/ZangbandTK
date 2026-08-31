/* monster/aura
 *
 * Auras and bolt reflection, both sides (ZangbandTK, CNT-04 and CNT-09).
 *
 * Four monster flags and the three object flags that are the same mechanisms
 * seen from the wearer's end. Two things here are worth defending because
 * neither is what the names suggest: reflection is a nine-in-ten chance against
 * bolts only, and a fire aura is also a movement restriction -- a burning thing
 * will not wade into water, which is a rule with nothing to do with damage and
 * everything to do with there now being a sea to walk beside.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-aura.h"
#include "mon-util.h"
#include "monster.h"
#include "object.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();

	(void) test_seed_rng_reported(suite_name);
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * The monster flags reached the bestiary, in the numbers the import reported.
 */
static int test_the_flags_are_there(void *state) {
	int i, reflect = 0, fire = 0, cold = 0, elec = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rf_has(race->flags, RF_REFLECTING)) reflect++;
		if (rf_has(race->flags, RF_AURA_FIRE)) fire++;
		if (rf_has(race->flags, RF_AURA_COLD)) cold++;
		if (rf_has(race->flags, RF_AURA_ELEC)) elec++;
	}

	eq(reflect, 22);
	eq(fire, 21);
	eq(cold, 17);
	eq(elec, 21);

	ok;
}

/**
 * And the wearer's side reached the ego items.
 *
 * One each in Zangband, which is few enough that a conversion quietly dropping
 * them would look exactly like a conversion that never had them.
 */
static int test_the_ego_flags_are_there(void *state) {
	int i, reflect = 0, fire = 0, elec = 0;

	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];

		if (!ego->name) continue;
		if (of_has(ego->flags, OF_REFLECT)) reflect++;
		if (of_has(ego->flags, OF_SH_FIRE)) fire++;
		if (of_has(ego->flags, OF_SH_ELEC)) elec++;
	}

	require(reflect > 0);
	require(fire > 0);
	require(elec > 0);

	ok;
}

/**
 * A ball or a breath is never reflected; a bolt usually is.
 *
 * Only the chance is checked here.  Where the bounce lands needs a live cave to
 * ask about line of sight, which a unit test has not got -- the same reason the
 * quest suite checks its predicates rather than killing anything.
 *
 * The radius is the whole of the distinction, and it is the half of the rule a
 * reader would not guess -- "reflects bolt spells" is in Zangband's own lore
 * text, but nothing says so in the flag's name.
 */
static int test_only_bolts_bounce(void *state) {
	int i, bounced = 0, through = 0;

	/* Anything with a radius is a ball or a breath: never. */
	for (i = 0; i < 200; i++) {
		require(!aura_bolt_reflects(true, 1));
		require(!aura_bolt_reflects(true, 3));
	}

	/* Without the flag, never, whatever the radius. */
	for (i = 0; i < 200; i++) {
		require(!aura_bolt_reflects(false, 0));
	}

	/* With it, a bolt bounces most of the time but not always. */
	for (i = 0; i < 400; i++) {
		if (aura_bolt_reflects(true, 0)) bounced++;
		else through++;
	}

	require(bounced > 0);
	require(through > 0);

	/* Nine in ten, so the bounces should be much the commoner outcome. */
	require(bounced > through);

	ok;
}

/**
 * A fire aura keeps a monster out of the water (CNT-04).
 *
 * The behaviour nobody would guess from the flag's name, and the reason it is
 * worth having at all now: Zangband's version was written for a game whose only
 * water was decoration.
 */
static int test_a_burning_thing_will_not_wade_in(void *state) {
	int i, aquatic_and_burning = 0, burning = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_AURA_FIRE)) continue;

		burning++;

		/*
		 * And none of them is a fish, which would be a contradiction the
		 * movement rule could not resolve: it would refuse every grid the
		 * monster is allowed to stand on.
		 */
		if (monster_is_aquatic(race)) aquatic_and_burning++;
	}

	require(burning > 0);
	eq(aquatic_and_burning, 0);

	ok;
}

const char *suite_name = "monster/aura";
struct test tests[] = {
	{ "the-flags-are-there", test_the_flags_are_there },
	{ "the-ego-flags-are-there", test_the_ego_flags_are_there },
	{ "only-bolts-bounce", test_only_bolts_bounce },
	{ "a-burning-thing-will-not-wade-in",
	  test_a_burning_thing_will_not_wade_in },
	{ NULL, NULL }
};
