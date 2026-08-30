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
	int loaded = 0, failed = 0;

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
			printf("SAVE %-14s FAILED to load\n", name);
			failed++;
			continue;
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

	printf("SAVES %d loaded, %d failed\n", loaded, failed);

	/* The corpus is not empty... */
	require(loaded + failed > 0);

	/* ...and all of it works. */
	eq(failed, 0);

	ok;
}

const char *suite_name = "game/saves";
struct test tests[] = {
	{ "every-saved-character-still-loads", test_every_saved_character_still_loads },
	{ NULL, NULL }
};
