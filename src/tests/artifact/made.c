/*
 * artifact/made
 *
 * The Scroll of Artifact Creation (DEC-170): Zangband's artifact_scroll(),
 * which turns a weapon or piece of armour into an artifact during play, as
 * strong as one found at twice the reader's level, and asks what to call it.
 *
 * What is checked is what can go wrong quietly.  An artifact that is made but
 * not counted, not known, or still able to turn up in the dungeon looks fine
 * on the character sheet; so does one whose name another artifact has, until
 * the save comes back with the wrong one.  game/roundtrip covers the save.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmd-core.h"
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "message.h"
#include "mon-make.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-randart.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-history.h"
#include "source.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Warrior", "Tester")) {
		cleanup_angband();
		return 1;
	}
	(void) test_seed_rng_reported(suite_name);
	prepare_next_level(player);
	on_new_level();
	return 0;
}

int teardown_tests(void *state) {
	get_string_hook = NULL;
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** The first kind of this tval, or the named one. */
static struct object_kind *kind_of(int tval, const char *name) {
	int k;

	if (name) return lookup_kind(tval, lookup_sval(tval, name));
	for (k = 1; k < z_info->k_max; k++)
		if (k_info[k].tval == tval && k_info[k].name) return &k_info[k];
	return NULL;
}

/** A plain object of that kind, known as anything carried is. */
static struct object *make(int tval, const char *name) {
	struct object_kind *kind = kind_of(tval, name);
	struct object *obj;

	if (!kind) return NULL;
	obj = object_new();
	object_prep(obj, kind, 1, MINIMISE);
	obj->known = object_new();
	object_set_base_known(player, obj);
	object_touch(player, obj);
	return obj;
}

static void unmake(struct object *obj) {
	if (obj->known) object_delete(NULL, NULL, &obj->known);
	object_delete(NULL, NULL, &obj);
}

static void drop_from_pack(struct object *obj) {
	bool none_left = false;

	unmake(gear_object_for_use(player, obj, obj->number, false, &none_left));
}

/** Read the scroll with `obj` as the answer to "Enchant which item?". */
static bool read_on(struct object *obj) {
	struct effect effect;
	struct command cmd;
	bool ident = false, used;

	memset(&effect, 0, sizeof(effect));
	effect.index = EF_CREATE_ARTIFACT;
	effect.dice = dice_new();
	dice_parse_string(effect.dice, "0");
	memset(&cmd, 0, sizeof(cmd));
	if (obj) cmd_set_arg_item(&cmd, "tgtitem", obj);
	used = effect_do(&effect, source_player(), NULL, &ident, true, DIR_NONE,
		0, 0, &cmd);
	dice_free(effect.dice);
	return used;
}

/* What the player types when asked for a name, in order; NULL is Escape */
static const char *answers[4];
static int answered;

static bool answer(const char *prompt, char *buf, size_t len) {
	const char *a = answers[answered];

	if (!a) return false;
	answered++;
	my_strcpy(buf, a, len);
	return true;
}

static void will_answer(const char *first, const char *second) {
	answers[0] = first;
	answers[1] = second;
	answers[2] = NULL;
	answered = 0;
	get_string_hook = first ? answer : NULL;
}

/** Whether any of the last `n` messages contains `text`. */
static bool said(const char *text, int n) {
	int i;

	for (i = 0; i < n && i < messages_num(); i++)
		if (strstr(message_str(i), text)) return true;
	return false;
}

/** The power the randart designer gives an artifact, as it measures one. */
static int power_of(const struct artifact *art) {
	struct object *obj = object_new(), *known = object_new();
	int power;

	make_fake_artifact(obj, art);
	object_copy(known, obj);
	obj->known = known;
	power = object_power(obj, false, NULL);
	object_delete(NULL, NULL, &known);
	object_delete(NULL, NULL, &obj);
	return power;
}

/**
 * A long sword read over becomes an artifact: a new entry in a_info, counted,
 * never to be placed in the dungeon, known to have been made and seen, and
 * the object is what that entry says -- the four base elements ignored as
 * Zangband's IGNORE flags had it, every rune known as its identify_fully_aux()
 * left it, and the history saying it was made rather than found.
 */
static int test_a_sword_becomes_an_artifact(void *state) {
	struct object *obj = make(TV_SWORD, "Long Sword");
	const struct artifact *art;
	struct history_info *list = NULL;
	size_t n, i;
	bool logged = false;
	int before = z_info->a_max, e;

	notnull(obj);
	inven_carry(player, obj, false, false);
	will_answer(NULL, NULL);

	require(read_on(obj));
	art = obj->artifact;
	notnull(art);
	require(artifact_is_made(art));
	eq(z_info->a_max, before + 1);
	eq((int) art->aidx, before);
	require(&a_info[art->aidx] == art);
	eq(art->alloc_prob, 0);
	require(is_artifact_created(art));
	require(is_artifact_seen(art));
	require(is_artifact_everseen(art));
	require(art->name[0] == '\'' || !strncmp(art->name, "of ", 3));
	require(lookup_artifact_name(art->name) == art);
	require(said("radiates a blinding light!", 30));

	/* The object is what the artifact says */
	eq(obj->to_h, art->to_h);
	eq(obj->to_d, art->to_d);
	eq(obj->dd, art->dd);
	eq(obj->ds, art->ds);
	require(of_is_equal(obj->flags, art->flags));
	for (e = ELEM_BASE_MIN; e < ELEM_HIGH_MIN; e++)
		require(obj->el_info[e].flags & EL_INFO_IGNORE);

	/* And it is known through and through */
	require(obj->known->artifact == art);
	require(object_runes_known(obj));

	n = history_get_list(player, &list);
	for (i = 0; i < n; i++) {
		if (list[i].a_idx == art->aidx && !strncmp(list[i].event, "Made ", 5))
			logged = true;
		require(strncmp(list[i].event, "Found ", 6) ||
			list[i].a_idx != art->aidx);
	}
	require(logged);

	drop_from_pack(obj);
	ok;
}

/** The name typed goes in single quotes, as Zangband's did. */
static int test_the_name_typed_is_its_name(void *state) {
	struct object *obj = make(TV_SHIELD, NULL);

	notnull(obj);
	inven_carry(player, obj, false, false);
	will_answer("Glimmerward", NULL);
	require(read_on(obj));
	notnull(obj->artifact);
	require(streq(obj->artifact->name, "'Glimmerward'"));
	require(lookup_artifact_name("'Glimmerward'") == obj->artifact);
	will_answer(NULL, NULL);
	drop_from_pack(obj);
	ok;
}

/**
 * A name another artifact has is refused and asked for again.  Objects are
 * saved by their artifact's name and found again by it, so a second 'Ringil'
 * would come back from the savefile as the first.
 */
static int test_a_name_already_taken_is_asked_again(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");
	const struct artifact *ringil = lookup_artifact_name("'Ringil'");

	notnull(obj);
	notnull(ringil);
	inven_carry(player, obj, false, false);
	will_answer("Ringil", "Needlefang");
	require(read_on(obj));
	notnull(obj->artifact);
	require(obj->artifact != ringil);
	require(streq(obj->artifact->name, "'Needlefang'"));
	require(said("There is already an artifact called 'Ringil'.", 10));
	eq(answered, 2);
	will_answer(NULL, NULL);
	drop_from_pack(obj);
	ok;
}

/**
 * What Zangband refuses, it refuses: dragon armour is "already magical", an
 * artifact or an ego item "already a powerful item".  The scroll is still
 * used, and nothing is made.
 */
static int test_what_has_powers_already_is_refused(void *state) {
	struct object *dsm = make(TV_DRAG_ARMOR, NULL);
	struct object *ego = make(TV_SWORD, "Long Sword");
	struct object *art = make(TV_SWORD, "Long Sword");
	int before = z_info->a_max, i;

	notnull(dsm);
	notnull(ego);
	notnull(art);
	for (i = 1; i < z_info->e_max && !ego->ego; i++)
		if (e_info[i].name) ego->ego = &e_info[i];
	notnull(ego->ego);
	art->artifact = lookup_artifact_name("'Ringil'");
	notnull(art->artifact);
	inven_carry(player, dsm, false, false);
	inven_carry(player, ego, false, false);
	inven_carry(player, art, false, false);

	require(read_on(dsm));
	require(said("is already magical!", 5));
	null(dsm->artifact);
	require(read_on(ego));
	require(said("is already a powerful item!", 5));
	null(ego->artifact);
	require(read_on(art));
	require(said("is already a powerful item!", 5));
	require(art->artifact == lookup_artifact_name("'Ringil'"));
	require(said("The enchantment failed.", 5));
	eq(z_info->a_max, before);

	drop_from_pack(dsm);
	drop_from_pack(ego);
	art->artifact = NULL;
	drop_from_pack(art);
	ok;
}

/** Of a stack, one is enchanted and the rest are destroyed. */
static int test_a_stack_keeps_only_one(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	obj->number = 3;
	obj->known->number = 3;
	inven_carry(player, obj, false, false);
	require(read_on(obj));
	eq(obj->number, 1);
	eq(obj->known->number, 1);
	notnull(obj->artifact);
	require(said("Not enough energy to enchant more than one object!", 10));
	require(said("2 of your Daggers were destroyed!", 10));
	drop_from_pack(obj);
	ok;
}

/**
 * Only swords to gloves: a ring, a digger and arrows are not offered, and
 * with nothing chosen the scroll is put away unread.
 */
static int test_only_weapons_and_armour_are_offered(void *state) {
	struct object *ring = make(TV_RING, NULL);
	struct object *digger = make(TV_DIGGING, NULL);
	struct object *arrows = make(TV_ARROW, NULL);
	int before = z_info->a_max;

	notnull(ring);
	notnull(digger);
	notnull(arrows);
	inven_carry(player, ring, false, false);
	inven_carry(player, digger, false, false);
	inven_carry(player, arrows, false, false);
	require(!read_on(ring));
	require(!read_on(digger));
	require(!read_on(arrows));
	require(!read_on(NULL));
	eq(z_info->a_max, before);
	drop_from_pack(ring);
	drop_from_pack(digger);
	drop_from_pack(arrows);
	ok;
}

/**
 * Twice the reader's level is the depth it is made for, and the depth sets
 * the power.  Measured on a long sword at a first-level and a fiftieth-level
 * reader, by the median of each, since one draw can land anywhere.  Over 40
 * runs of 31 draws the shallow median stayed within 79 to 88 and the deep one
 * within 167 to 278, so twice the shallow one failed one run in twenty; 61
 * draws and half again keep well clear of that, and a depth that made no
 * difference would still come out near one.
 */
static int compare_ints(const void *a, const void *b) {
	return *(const int *) a - *(const int *) b;
}

static int median_power(int depth, int count) {
	int powers[64];
	int i;

	assert(count <= (int) N_ELEMENTS(powers));
	for (i = 0; i < count; i++) {
		struct object *obj = make(TV_SWORD, "Long Sword");
		const struct artifact *art = randart_make_from_object(obj, depth);

		powers[i] = art ? power_of(art) : -1;
		unmake(obj);
	}
	artifact_made_reset();
	qsort(powers, count, sizeof(powers[0]), compare_ints);
	return powers[count / 2];
}

static int test_the_power_follows_the_level(void *state) {
	int shallow = median_power(2, 61);
	int deep = median_power(100, 61);

	printf("MEDIAN power: depth 2 %d, depth 100 %d\n", shallow, deep);
	require(shallow > 0);
	require(deep * 2 > shallow * 3);
	ok;
}

/** Never placed in the dungeon, even taken for lost. */
static int test_a_made_artifact_is_never_placed(void *state) {
	struct object *obj = make(TV_SWORD, "Long Sword");
	const struct artifact *art = randart_make_from_object(obj, 60);
	int i, old_depth = player->depth;

	notnull(art);
	unmake(obj);
	mark_artifact_created(art, false);
	player->depth = 60;
	for (i = 0; i < 2000; i++) {
		struct object *try = make(TV_SWORD, "Long Sword");

		apply_magic(try, 60, true, true, true, false);
		require(try->artifact != art);
		if (try->artifact) mark_artifact_created(try->artifact, false);
		unmake(try);
	}
	player->depth = old_depth;
	artifact_made_reset();
	ok;
}

/** When the spare entries are all taken, the enchantment fails and nothing
 * is lost. */
static int test_the_spare_entries_can_run_out(void *state) {
	struct object *obj = make(TV_SWORD, "Long Sword");
	struct artifact *art;
	int n = 0;

	notnull(obj);
	while ((art = artifact_made_claim())) {
		art->name = string_make(format("'Filler %d'", n++));
		art->tval = obj->tval;
		art->sval = obj->sval;
		artifact_made_commit(art);
	}
	eq(artifact_made_count(), ART_MADE_MAX);
	inven_carry(player, obj, false, false);
	require(read_on(obj));
	null(obj->artifact);
	require(said("The enchantment failed.", 5));
	eq(artifact_made_count(), ART_MADE_MAX);

	artifact_made_reset();
	eq(artifact_made_count(), 0);
	eq(z_info->a_max, artifact_standard_count());
	drop_from_pack(obj);
	ok;
}

/**
 * The history log kept an artifact's index in a byte, which the 189 standard
 * artifacts never outgrew and made ones do: the 67th made artifact is entry
 * 256.  Its line must name it, not whatever is at its index less 256.
 */
static int test_an_artifact_past_the_255th_is_logged_as_itself(void *state) {
	struct object *obj = make(TV_SWORD, "Long Sword");
	struct artifact *filler;
	struct history_info *list = NULL;
	size_t n, i;
	bool logged = false;
	int k = 0;

	notnull(obj);
	while (z_info->a_max <= 256) {
		filler = artifact_made_claim();
		notnull(filler);
		filler->name = string_make(format("'Filler %d'", k++));
		filler->tval = obj->tval;
		filler->sval = obj->sval;
		artifact_made_commit(filler);
	}
	inven_carry(player, obj, false, false);
	require(read_on(obj));
	notnull(obj->artifact);
	require(obj->artifact->aidx > 256);

	n = history_get_list(player, &list);
	for (i = 0; i < n; i++)
		if (list[i].a_idx == obj->artifact->aidx &&
				!strncmp(list[i].event, "Made ", 5))
			logged = true;
	require(logged);

	drop_from_pack(obj);
	artifact_made_reset();
	ok;
}

/** The scroll says what it does, and is a level-100 scroll worth 300000. */
static int test_the_scroll_is_in_the_game(void *state) {
	struct object_kind *kind = kind_of(TV_SCROLL, "Artifact Creation");

	notnull(kind);
	notnull(kind->effect);
	eq(kind->effect->index, EF_CREATE_ARTIFACT);
	eq(kind->level, 100);
	eq(kind->cost, 300000);
	ok;
}

const char *suite_name = "artifact/made";
struct test tests[] = {
	{ "the-scroll-is-in-the-game", test_the_scroll_is_in_the_game },
	{ "a-sword-becomes-an-artifact", test_a_sword_becomes_an_artifact },
	{ "the-name-typed-is-its-name", test_the_name_typed_is_its_name },
	{ "a-name-already-taken-is-asked-again",
		test_a_name_already_taken_is_asked_again },
	{ "what-has-powers-already-is-refused",
		test_what_has_powers_already_is_refused },
	{ "a-stack-keeps-only-one", test_a_stack_keeps_only_one },
	{ "only-weapons-and-armour-are-offered",
		test_only_weapons_and_armour_are_offered },
	{ "the-power-follows-the-level", test_the_power_follows_the_level },
	{ "a-made-artifact-is-never-placed", test_a_made_artifact_is_never_placed },
	{ "the-spare-entries-can-run-out", test_the_spare_entries_can_run_out },
	{ "an-artifact-past-the-255th-is-logged-as-itself",
		test_an_artifact_past_the_255th_is_logged_as_itself },
	{ NULL, NULL }
};
