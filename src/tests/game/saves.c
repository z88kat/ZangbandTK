/* game/saves.c
 *
 * Every savefile in tests/saves must still load.
 *
 * These are real characters, played and saved by hand over the course of
 * development, and they are here because savefile compatibility is the one
 * thing this project can break without noticing.  A parse error is loud; a
 * savefile that stops loading is silent until somebody loses a character.
 *
 * Each block in a savefile carries its own version and the loader keeps a
 * reader for every version it has ever written, so an old file is supposed to
 * load into a new build unchanged.  "Supposed to" is what this checks: the
 * files span several block versions -- some predate quests v4, the player block
 * v2 that carries a chaos patron, the wilderness block versions, and the fish
 * base -- and nothing but loading them proves the old readers still work.
 *
 * They are opened read-only and never written back, so the corpus cannot be
 * quietly rewritten into whatever the current format happens to be, which
 * would defeat the whole purpose.
 *
 * **What this suite proves changed on 2 September 2026, and not for the
 * better.** DEC-50 gave the Life realm Zangband's four prayer books in place of
 * Angband's five, which deleted the object kinds of the old ones -- and a
 * savefile names an object by tval and sval as text, so every one of these
 * thirty-five files became unreadable at a stroke. Every save carries the town
 * temple's stock, and the temple sells prayer books.
 *
 * So all thirty-five are now listed in EXPECTED-FAILURES, and what is left
 * checks two things and not the one it was built for: that every historical
 * file is refused *gracefully* rather than crashing the process, which is the
 * failure mode the four faults of 30 August actually had; and that the manifest
 * stays honest, because a listed file that starts loading again fails the suite
 * too.
 *
 * It no longer proves that a character can be saved and loaded at all.
 * **`game/roundtrip` does that now**: it builds its own caster, saves it, loads
 * it back, and proves the spell-list fingerprint refuses a character whose
 * class list has moved and accepts one whose has not. A fresh corpus of played
 * characters is still worth having -- these files span block versions no
 * generated character will reproduce -- but the live guard is no longer
 * missing.
 */

#include "unit-test.h"
#include "unit-test-data.h"
#include "test-utils.h"

#include "game-world.h"
#include "init.h"
#include "mon-make.h"
#include "player.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-ignore.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-quest.h"
#include "savefile.h"
#include "store.h"
#include "z-file.h"
#include "z-util.h"
#include "z-virt.h"

/* Where the corpus lives, relative to the directory the suite runs in. */
#define SAVE_CORPUS "tests/saves"

/** Send the loader's notes to stdout, so a refusal says why. */
static void println(const char *str) {
	printf("%s\n", str);
}

static void build_test_list(void);

int setup_tests(void **state) {
	plog_aux = println;

	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	/*
	 * The test list is built here rather than written out, because it has one
	 * entry per savefile and the corpus is whatever is on disk. setup_tests()
	 * runs before the harness walks tests[], which is what makes this legal.
	 */
	build_test_list();

	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

#define SAVE_EXPECTED "EXPECTED-FAILURES"

/**
 * Whether this savefile is on the list of ones known not to load.
 *
 * Kept as a file beside the corpus rather than a list in here, so that adding
 * or clearing an entry is a change to the thing it describes and carries its
 * reason with it.  A line is "name: reason"; '#' comments and blank lines are
 * ignored.
 */
static bool save_expected_to_fail(const char *name)
{
	char path[1024];
	ang_file *f;
	char line[256];
	bool found = false;

	path_build(path, sizeof(path), SAVE_CORPUS, SAVE_EXPECTED);
	f = file_open(path, MODE_READ, FTYPE_TEXT);
	if (!f) return false;

	while (!found && file_getl(f, line, sizeof(line))) {
		char *colon;

		if (line[0] == '#' || line[0] == '\0') continue;

		colon = strchr(line, ':');
		if (colon) *colon = '\0';

		found = streq(line, name);
	}

	file_close(f);
	return found;
}


/**
 * Put the game back to a state a savefile can be loaded into.
 *
 * The same dance the wilderness suite does around its own save and load: the
 * loader expects to be building a game from nothing, so everything the last
 * load left behind has to go first.
 */
static void reset_for_load(void)
{
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	/*
	 * `cleanup_angband()` releases the data paths, so they have to be put back
	 * before the next `init_angband()` rather than only at startup.
	 */
	set_file_paths();
	init_angband();
}



/*
 * One test per savefile, built at run time.
 *
 * The corpus used to be a single test: thirty-five files loaded in a loop, three
 * counters, and four assertions at the end. It reported "0/1 passed" and a
 * tally, which is exactly what it did on Windows CI for two runs -- and from
 * that log it was impossible to tell *which* file had gone wrong, or in which
 * direction, or (as it turned out) that no file had been read at all.
 *
 * So each file is now its own named test. `game/saves 34/35 passed` says how
 * many, and the runner echoes the failing assertion, which names the file.
 * `the-corpus-is-present` runs first and says the path it looked in, because
 * "the directory is not there" is a different answer from "a save broke" and
 * the old suite could not tell them apart.
 *
 * The harness takes a static `tests[]`, so it is sized once and filled in
 * `setup_tests()`, which runs before the first test. The names have to outlive
 * setup, so they are the strings in `corpus[]` rather than a stack buffer.
 */
#define MAX_SAVES 128

static char corpus[MAX_SAVES][256];
static int corpus_count;
static bool corpus_found;
static char corpus_path[1024];

/* Which entry the next per-save test should read. */
static int next_save;

/**
 * The corpus directory is where it should be, and not empty.
 *
 * Runs first, and is the reason the rest can be terse. An absent corpus is not
 * a broken savefile and must not read like one: two Windows CI runs failed here
 * and the log said only "0/1 passed", which sent the search after line endings
 * and path separators when the directory simply had not been staged.
 */
static int test_the_corpus_is_present(void *state) {
	if (!corpus_found) {
		printf("SAVES no corpus at %s -- nothing was tested\n", corpus_path);
		printf("SAVES   the unit tests read tests/ from the build's game\n");
		printf("SAVES   directory; if it is missing, that staging is missing\n");
	}
	require(corpus_found);

	/* Absent and empty are both real answers, and neither is a pass. */
	require(corpus_count > 0);

	printf("SAVES %d files in %s\n", corpus_count, corpus_path);

	ok;
}

/** The byte the object memory block records for a kind. */
static uint8_t kind_knowledge_bits(const struct object_kind *kind)
{
	uint8_t bits = 0;

	if (kind->aware) bits |= 0x01;
	if (kind->tried) bits |= 0x02;
	if (kind_is_ignored_aware(kind)) bits |= 0x04;
	if (kind->everseen) bits |= 0x08;
	if (kind_is_ignored_unaware(kind)) bits |= 0x10;
	return bits;
}

/**
 * One savefile: it loads and survives a round trip, or it is refused and listed.
 *
 * Both directions are failures, which is the whole point of the manifest. A
 * file that will not load and is not on the list is a break. A file that loads
 * and *is* on the list has outlived its entry, and leaving it there would go on
 * excusing the next genuine break of that same file.
 */
static int test_one_saved_character(void *state) {
	/*
	 * Per process, not a fixed name.
	 *
	 * This was `"saves-roundtrip.tmp"` for everybody, so any two runs of this
	 * suite at the same time wrote and deleted the same file and one of them
	 * lost -- reliably, eight times out of eight when run in parallel. The
	 * suites are run sequentially by `check-flakes` and by the CMake runner,
	 * so nothing in the harness collides; a person running a suite by hand
	 * while a verification job is going does, and it looks exactly like an
	 * intermittent savefile bug. `test_savefile_name()` was written for this
	 * and three other suites already use it.
	 */
	char roundtrip[64];
	const char *name;
	char path[1024];
	bool listed;
	uint8_t *known;
	int k;

	test_savefile_name(roundtrip, sizeof(roundtrip), "saves-roundtrip");

	require(next_save < corpus_count);
	name = corpus[next_save++];
	listed = save_expected_to_fail(name);

	path_build(path, sizeof(path), SAVE_CORPUS, name);

	/*
	 * Named before the attempt, not after. A savefile the loader cannot make
	 * sense of calls quit(), which takes the process with it, so a message
	 * printed afterwards is a message never printed -- and the one thing
	 * needed at that point is which file did it.
	 */
	printf("SAVE trying %s\n", name);
	fflush(stdout);

	play_again = true;
	reset_for_load();
	play_again = false;

	if (!savefile_load(path, false)) {
		if (listed) {
			printf("SAVE %-14s refused, as expected\n", name);
			ok;
		}
		printf("SAVE %-14s FAILED to load, and is not in %s\n",
			   name, SAVE_EXPECTED);
		require(listed);
	}

	if (listed) {
		printf("SAVE %-14s loads; take it out of %s\n", name, SAVE_EXPECTED);
		require(!listed);
	}

	/*
	 * Loaded is not the same as usable. A character whose race or class no
	 * longer exists came back wrong even though every block was read without
	 * complaint -- which is exactly what renaming something in the data files
	 * would do. The name is not checked: a character may legitimately have
	 * none, which is why the default savefile is called PLAYER.
	 */
	notnull(player);
	notnull(player->race);
	notnull(player->class);
	notnull(player->race->name);
	notnull(player->class->name);

	/*
	 * Every shop has a keeper, the Statue Store included, which none of these
	 * savefiles had (DEC-145): a shop without one crashes the first time its
	 * door is used.
	 */
	for (k = 0; k < z_info->store_max; k++)
		notnull(stores[k].owner);

	/* What the character knows of each kind, to compare after the trip */
	known = mem_zalloc(z_info->k_max);
	for (k = 0; k < z_info->k_max; k++)
		known[k] = kind_knowledge_bits(&k_info[k]);

	/*
	 * Loading is only half of compatibility. An old save can come back in a
	 * state the writer cannot cope with -- a monster array with a hole in it
	 * will crash wr_monster on the very next save -- so each character is
	 * written out again and read back. The copy goes to a scratch path: the
	 * corpus itself stays exactly as it was played.
	 */
	require(savefile_save(roundtrip));

	play_again = true;
	reset_for_load();
	play_again = false;

	require(savefile_load(roundtrip, false));
	file_delete(roundtrip);

	/*
	 * And the trip kept it.  The corpus is all object memory version 1, read
	 * by position; the copy is version 2, written and read by name -- so this
	 * is the check that the two agree, kind for kind, on every character.
	 */
	for (k = 0; k < z_info->k_max; k++) {
		if (known[k] == kind_knowledge_bits(&k_info[k])) continue;
		printf("SAVE %-14s %s came back as %02x, went in as %02x\n", name,
			   k_info[k].name ? k_info[k].name : "(no name)",
			   kind_knowledge_bits(&k_info[k]), known[k]);
		mem_free(known);
		require(false);
	}
	mem_free(known);

	printf("SAVE %-14s loaded, and survived a save and reload\n", name);

	ok;
}

/**
 * Every name in the manifest is a file that is actually there.
 *
 * The other way a manifest goes stale. A name that matches nothing excuses
 * nothing, so it never fails anything -- it just sits there, and the next
 * reader takes it as evidence about a file that no longer exists. Found by
 * falsifying the suite: adding an entry for a file not in the corpus passed
 * 37/37, which is the definition of a check that is not being made.
 */
static int test_the_manifest_has_no_dead_entries(void *state) {
	char path[1024];
	ang_file *f;
	char line[256];
	int dead = 0;

	require(corpus_found);

	path_build(path, sizeof(path), SAVE_CORPUS, SAVE_EXPECTED);
	f = file_open(path, MODE_READ, FTYPE_TEXT);
	notnull(f);

	while (file_getl(f, line, sizeof(line))) {
		char *colon;
		int i;
		bool present = false;

		if (line[0] == '#' || line[0] == '\0' || line[0] == '\r') continue;

		colon = strchr(line, ':');
		if (!colon) continue;
		*colon = '\0';

		for (i = 0; i < corpus_count; i++) {
			if (streq(line, corpus[i])) {
				present = true;
				break;
			}
		}

		if (!present) {
			printf("SAVES %s names %s, which is not in the corpus\n",
				   SAVE_EXPECTED, line);
			dead++;
		}
	}

	file_close(f);

	eq(dead, 0);

	ok;
}

/**
 * Every file in the corpus was actually offered to a test.
 *
 * The per-save tests read `corpus[]` in order, one entry each, because the
 * harness hands a test no way of knowing which one it is. That is only sound
 * if the harness runs them all, in order, exactly once -- so this checks the
 * counter arrived where it should. It is the guard on the mechanism rather than
 * on the savefiles.
 */
static int test_every_file_was_tried(void *state) {
	eq(next_save, corpus_count);

	ok;
}


/**
 * A taken quest still knows what it is about after a save (WLD-19, WLD-21).
 *
 * `struct quest` carries `race`, `kind` and `dungeon`; the writer persisted
 * none of them, and `player_quests_reset()` puts them back only for the quests
 * that come from `quest.txt`. So a bounty or a fetch came back pointing at
 * nothing, and the two places that advance a quest compare against exactly
 * those fields -- `m->race == q->race` is never true against a NULL, and
 * `q->kind != obj->kind` is always true. The count could not move, the quest
 * could not complete, and the slot stayed occupied for the rest of the run.
 *
 * Asserted through `quest_check()` and `quest_check_item()` rather than by
 * reading the fields back, because the fields being present is not the thing
 * that was broken -- the counting is. A version that persisted them and
 * restored them to the wrong slot would pass a field comparison.
 */
static int test_a_taken_quest_survives_a_save(void *state) {
	const char *seed = NULL;
	char tmp[1024];
	struct monster_race *race;
	struct object_kind *kind = NULL;
	struct quest *bounty, *fetch;
	int i, k;

	/* Any corpus character will do; it just has to be a real game state. */
	for (i = 0; i < corpus_count && !seed; i++)
		if (!save_expected_to_fail(corpus[i])) seed = corpus[i];
	require(seed);

	path_build(tmp, sizeof(tmp), SAVE_CORPUS, seed);
	set_file_paths();
	reset_for_load();
	play_again = false;
	require(savefile_load(tmp, false));

	race = lookup_monster("soldier");
	notnull(race);
	/*
	 * Any ordinary kind will do, found by substring: `k_info[].name` carries
	 * the data file's plural markers, so an exact match on the name a person
	 * would write returns nothing.
	 */
	for (k = 1; k < z_info->k_max && !kind; k++)
		if (k_info[k].name && strstr(k_info[k].name, "Ration"))
			kind = &k_info[k];
	notnull(kind);

	bounty = quest_take(player, QUEST_BOUNTY, "three soldiers", race, 3);
	notnull(bounty);
	bounty->dungeon = 2;

	fetch = quest_take(player, QUEST_FIND_ITEM, "a flask", NULL, 1);
	notnull(fetch);
	fetch->kind = kind;

	path_build(tmp, sizeof(tmp), ".", "questroundtrip.sav");
	require(savefile_save(tmp));

	play_again = true;
	set_file_paths();
	reset_for_load();
	play_again = false;
	require(savefile_load(tmp, false));
	file_delete(tmp);

	/* Find them again by name -- the slot order is not the assertion. */
	bounty = fetch = NULL;
	for (i = 0; i < (int) z_info->quest_max; i++) {
		struct quest *q = &player->quests[i];

		if (!q->name) continue;
		if (streq(q->name, "three soldiers")) bounty = q;
		if (streq(q->name, "a flask")) fetch = q;
	}
	notnull(bounty);
	notnull(fetch);

	eq(bounty->state, QUEST_TAKEN);
	eq(fetch->state, QUEST_TAKEN);

	/*
	 * The bounty counts a kill of its own race, which is the whole point.
	 *
	 * The race and the kind are looked up *again* here. `reset_for_load()`
	 * frees and rebuilds `r_info` and `k_info`, so the pointers taken before
	 * the save are dangling by now -- and under a plain allocator they often
	 * come back at the same address, so comparing against them passes by luck.
	 * ASAN does not reuse the block and said so.
	 */
	race = lookup_monster("soldier");
	notnull(race);
	kind = NULL;
	for (k = 1; k < z_info->k_max && !kind; k++)
		if (k_info[k].name && strstr(k_info[k].name, "Ration"))
			kind = &k_info[k];
	notnull(kind);

	{
		struct monster mon;

		memset(&mon, 0, sizeof(mon));
		mon.race = race;
		eq(bounty->cur_num, 0);
		(void) quest_check(player, &mon);
		eq(bounty->cur_num, 1);
	}

	/* And the fetch notices the thing it asked for. */
	{
		struct object obj;

		memset(&obj, 0, sizeof(obj));
		obj.kind = kind;
		obj.number = 1;
		eq(fetch->cur_num, 0);
		(void) quest_check_item(player, &obj);
		eq(fetch->cur_num, 1);
	}

	/* And which dungeon it was set in, checked after the counting. */
	eq(bounty->dungeon, 2);

	ok;
}

const char *suite_name = "game/saves";

/*
 * Three invariant tests plus one per savefile, and the NULL the harness stops
 * on.
 */
struct test tests[MAX_SAVES + 7];

/** The kind of that object base and name, or NULL. */
static struct object_kind *kind_called(const char *tval, const char *name)
{
	int sv = lookup_sval(tval_find_idx(tval), name);

	return sv < 0 ? NULL : lookup_kind(tval_find_idx(tval), sv);
}

/**
 * An old character's knowledge is read by name, not by position (3.129.0).
 *
 * Every save here wrote its object memory as version 1: a byte per kind, in
 * k_info order.  The magical figurine went into that order part-way down, so
 * read by position every kind after it would take its predecessor's byte.
 * Ardormor is a Chaos-Warrior who identified an Amulet of Anti-Teleportation
 * and owns a tanto -- both Zangband kinds, both after the figurine -- and
 * whose neighbours in the list he never saw.  So the byte that lands on each
 * says which way it was read.
 */
static int test_old_knowledge_is_read_by_name(void *state) {
	char path[1024];
	struct object_kind *amulet, *tanto, *figurine;

	path_build(path, sizeof(path), SAVE_CORPUS, "Ardormor");
	play_again = true;
	reset_for_load();
	play_again = false;
	require(savefile_load(path, false));

	amulet = kind_called("amulet", "Anti-Teleportation");
	tanto = kind_called("sword", "Tanto");
	figurine = kind_called("figurine", "Magical Figurine");
	notnull(amulet);
	notnull(tanto);
	notnull(figurine);

	/* Aware, tried and seen; and seen */
	eq(kind_knowledge_bits(amulet), 0x0b);
	eq(kind_knowledge_bits(tanto), 0x09);

	/* And the kind he could not have known of is as new as it is */
	require(!figurine->tried);
	require(!figurine->everseen);
	require(!kind_is_ignored_aware(figurine));
	require(!kind_is_ignored_unaware(figurine));
	ok;
}

/**
 * Every kind list-kinds-v1.h names is still a kind, or has a rename or a
 * removal recorded for it.
 *
 * That list is what a version 1 savefile's positions mean, and it is frozen.
 * A kind in it that the game no longer has, with nothing in rename.txt to say
 * so, takes every version 1 character's knowledge of it away in silence --
 * which is the moment to write the rename, and this is what says so.
 */
static int test_the_old_kind_list_names_real_kinds(void *state) {
	static const struct { const char *tval, *name; } v1[] = {
		#define KIND_V1(t, n) { t, n },
		#include "list-kinds-v1.h"
		#undef KIND_V1
	};
	size_t i;
	int missing = 0;

	for (i = 0; i < N_ELEMENTS(v1); i++) {
		const struct rename_entry *r;
		bool renamed = false;

		if (!v1[i].name || kind_called(v1[i].tval, v1[i].name)) continue;
		for (r = renames; r; r = r->next) {
			if (r->kind == RENAME_OBJECT && r->tval == tval_find_idx(v1[i].tval)
					&& !my_stricmp(r->from, v1[i].name))
				renamed = true;
		}
		if (renamed) continue;
		printf("SAVES position %d, %s %s, is no kind any more\n", (int) i,
			   v1[i].tval, v1[i].name);
		missing++;
	}
	eq(missing, 0);
	eq((int) N_ELEMENTS(v1), 506);
	ok;
}

static void build_test_list(void) {
	ang_dir *dir;
	char name[256];
	int i, n = 0;

	path_build(corpus_path, sizeof(corpus_path), ".", SAVE_CORPUS);

	dir = my_dopen(SAVE_CORPUS);
	if (dir) {
		corpus_found = true;
		while (n < MAX_SAVES && my_dread(dir, name, sizeof(name))) {
			/* Skip anything that is plainly not a savefile. */
			if (name[0] == '.') continue;
			if (suffix(name, ".md") || suffix(name, ".txt")) continue;
			if (streq(name, SAVE_EXPECTED)) continue;

			my_strcpy(corpus[n], name, sizeof(corpus[n]));
			n++;
		}
		my_dclose(dir);
	}
	corpus_count = n;

	i = 0;
	tests[i].name = "the-corpus-is-present";
	tests[i].func = test_the_corpus_is_present;
	i++;
	for (n = 0; n < corpus_count; n++) {
		tests[i].name = corpus[n];
		tests[i].func = test_one_saved_character;
		i++;
	}
	tests[i].name = "a-taken-quest-survives-a-save";
	tests[i].func = test_a_taken_quest_survives_a_save;
	i++;
	tests[i].name = "old-knowledge-is-read-by-name";
	tests[i].func = test_old_knowledge_is_read_by_name;
	i++;
	tests[i].name = "the-old-kind-list-names-real-kinds";
	tests[i].func = test_the_old_kind_list_names_real_kinds;
	i++;
	tests[i].name = "the-manifest-has-no-dead-entries";
	tests[i].func = test_the_manifest_has_no_dead_entries;
	i++;
	tests[i].name = "every-file-was-tried";
	tests[i].func = test_every_file_was_tried;
	i++;
	tests[i].name = NULL;
	tests[i].func = NULL;
}
