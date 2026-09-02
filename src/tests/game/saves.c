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
#include "savefile.h"
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

/**
 * One savefile: it loads and survives a round trip, or it is refused and listed.
 *
 * Both directions are failures, which is the whole point of the manifest. A
 * file that will not load and is not on the list is a break. A file that loads
 * and *is* on the list has outlived its entry, and leaving it there would go on
 * excusing the next genuine break of that same file.
 */
static int test_one_saved_character(void *state) {
	const char *roundtrip = "saves-roundtrip.tmp";
	const char *name;
	char path[1024];
	bool listed;

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

const char *suite_name = "game/saves";

/*
 * Three invariant tests plus one per savefile, and the NULL the harness stops
 * on.
 */
struct test tests[MAX_SAVES + 4];

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
	tests[i].name = "the-manifest-has-no-dead-entries";
	tests[i].func = test_the_manifest_has_no_dead_entries;
	i++;
	tests[i].name = "every-file-was-tried";
	tests[i].func = test_every_file_was_tried;
	i++;
	tests[i].name = NULL;
	tests[i].func = NULL;
}
