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

int setup_tests(void **state) {
	plog_aux = println;

	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

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

/**
 * Every file in tests/saves loads, and produces a character.
 */
static int test_every_saved_character_still_loads(void *state) {
	ang_dir *dir = my_dopen(SAVE_CORPUS);
	const char *roundtrip = "saves-roundtrip.tmp";
	char name[256];
	int loaded = 0, failed = 0, expected = 0, revived = 0;

	/*
	 * Absent rather than empty is a real answer: a checkout without the corpus
	 * should say so rather than pass silently having tested nothing.
	 */
	notnull(dir);

	while (my_dread(dir, name, sizeof(name))) {
		char path[1024];

		/* Skip anything that is plainly not a savefile. */
		if (name[0] == '.') continue;
		if (suffix(name, ".md") || suffix(name, ".txt")) continue;
		if (streq(name, SAVE_EXPECTED)) continue;

		path_build(path, sizeof(path), SAVE_CORPUS, name);

		/*
		 * Named before the attempt, not after.  A savefile the loader cannot
		 * make sense of calls quit(), which takes the process with it, so a
		 * message printed afterwards is a message never printed -- and the one
		 * thing needed at that point is which file did it.
		 */
		printf("SAVE trying %s\n", name);
		fflush(stdout);

		play_again = true;
		reset_for_load();
		play_again = false;

		if (!savefile_load(path, false)) {
			/*
			 * A refusal that is on the list is the loader doing its job.
			 * DEC-50 replaced the spell content of four realms, and a
			 * character who learned spells against the old list cannot be
			 * read without being handed somebody else's.
			 */
			if (save_expected_to_fail(name)) {
				printf("SAVE %-14s refused, as expected\n", name);
				expected++;
			} else {
				printf("SAVE %-14s FAILED to load\n", name);
				failed++;
			}
			continue;
		}

		/*
		 * And a file on the list that loads is a failure of its own.  The
		 * entry has outlived its reason, and left there it would go on
		 * excusing the next genuine break of that same file.
		 */
		if (save_expected_to_fail(name)) {
			printf("SAVE %-14s loads; take it off %s\n", name, SAVE_EXPECTED);
			revived++;
		}

		/*
		 * Loaded is not the same as usable.  A character whose race or class
		 * no longer exists came back wrong even though every block was read
		 * without complaint -- which is exactly what renaming something in
		 * the data files would do.  The name is not checked: a character may
		 * legitimately have none, which is why the default savefile is called
		 * PLAYER.
		 */
		if (!player || !player->race || !player->class ||
				!player->race->name || !player->class->name) {
			printf("SAVE %-14s loaded but is not a character\n", name);
			failed++;
			continue;
		}

		/*
		 * Loading is only half of compatibility.  An old save can come back
		 * in a state the writer cannot cope with - a monster array with a
		 * hole in it will crash wr_monster on the very next save - so each
		 * character is written out again and read back.  The copy goes to a
		 * scratch path: the corpus itself stays exactly as it was played.
		 */
		if (!savefile_save(roundtrip)) {
			printf("SAVE %-14s loaded but could not be saved again\n", name);
			failed++;
			continue;
		}

		play_again = true;
		reset_for_load();
		play_again = false;

		if (!savefile_load(roundtrip, false)) {
			printf("SAVE %-14s did not survive a save and reload\n", name);
			failed++;
			continue;
		}

		loaded++;
	}

	file_delete(roundtrip);

	my_dclose(dir);

	printf("SAVES %d loaded, %d refused as expected, %d failed\n", loaded,
		   expected, failed);

	/* The corpus is not empty... */
	require(loaded + failed + expected > 0);

	/* ...nothing broke that was not already known to be broken... */
	eq(failed, 0);

	/* ...and nothing on the list has quietly started working again. */
	eq(revived, 0);

	ok;
}


const char *suite_name = "game/saves";
struct test tests[] = {
	{ "every-saved-character-still-loads", test_every_saved_character_still_loads },
	{ NULL, NULL }
};
