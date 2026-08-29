/* test-utils.c
 *
 * Utility functions for tests
 *
 * Created by: myshkin
 *             26 Apr 2011
 */

#include "h-basic.h"
#include "cave.h"
#include "config.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "test-utils.h"
#include "unit-test.h"
#include "z-util.h"
#include "z-form.h"
#include "z-rand.h"

#ifdef UNIX
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/**
 * This process's id, or 0 where there is no way to ask.
 */
static int test_process_id(void)
{
#ifdef UNIX
	return (int) getpid();
#elif defined(_WIN32)
	return (int) GetCurrentProcessId();
#else
	return 0;
#endif
}

uint32_t test_seed_rng(void)
{
	uint32_t seed;
	const char *forced = getenv("ZTK_TEST_SEED");

	if (forced && forced[0]) {
		seed = (uint32_t) strtoul(forced, NULL, 10);
	} else {
		/*
		 * Not the clock alone.  Two copies started in the same second would
		 * otherwise draw the same world, which is exactly the case a
		 * concurrent run is meant to spread out.
		 */
		seed = (uint32_t) time(NULL) ^ ((uint32_t) test_process_id() << 16);
	}

	/*
	 * After init_angband(), deliberately.  Rand_init() has already seeded from
	 * the clock by then, and this replaces that choice with one we can report.
	 */
	Rand_state_init(seed);

	return seed;
}

void test_savefile_name(char *buf, size_t len, const char *stem)
{
	strnfmt(buf, len, "%s-%d", stem, test_process_id());
}

#if defined(SOUND_SDL) || defined(SOUND_SDL2)
#include "sound.h"
#include "snd-sdl.h"

errr init_sound_sdl(struct sound_hooks *hooks, int argc, char **argv)
{
	return (0);
}

#endif

#if !defined(WIN32_CONSOLE_MODE) && defined(WINDOWS) && defined(SOUND) && !defined(SOUND_SDL) && !defined(SOUND_SDL2)
#include "sound.h"
#include "snd-win.h"

errr init_sound_win(struct sound_hooks *hooks, int argc, char **argv)
{
	return 0;
}
#endif

/*
 * Call this to initialise Angband's file paths before calling init_angband()
 * or similar.
 */
void set_file_paths(void) {
	char configpath[512], libpath[512], datapath[512];

	/*
	 * Allow TEST_DEFAULT_PATH to set all the paths for init_file_paths()
	 * if it is set and the user has not requested that the default paths
	 * be used.  TEST_DEFAULT_PATH would typically point to the top level
	 * of a source distribution + PATH_SEP + lib.  Could use a relative
	 * path, in which case it should be set so that it works from the
	 * working directory when a test case is run.
	 */
	my_strcpy(configpath, DEFAULT_CONFIG_PATH, sizeof(configpath));
	my_strcpy(libpath, DEFAULT_LIB_PATH, sizeof(libpath));
	my_strcpy(datapath, DEFAULT_DATA_PATH, sizeof(datapath));
#ifdef TEST_DEFAULT_PATH
	if (!forcepath) {
		my_strcpy(configpath, TEST_DEFAULT_PATH, sizeof(configpath));
		my_strcpy(libpath, TEST_DEFAULT_PATH, sizeof(libpath));
		my_strcpy(datapath, TEST_DEFAULT_PATH, sizeof(datapath));
	}
#endif /* TEST_DEFAULT_PATH */

	configpath[511] = libpath[511] = datapath[511] = '\0';

	if (!suffix(configpath, PATH_SEP))
		my_strcat(configpath, PATH_SEP, sizeof(configpath));
	if (!suffix(libpath, PATH_SEP))
		my_strcat(libpath, PATH_SEP, sizeof(libpath));
	if (!suffix(datapath, PATH_SEP))
		my_strcat(datapath, PATH_SEP, sizeof(datapath));

	init_file_paths(configpath, libpath, datapath);
}

/*
 * Call this function to simulate init_stuff() and populate the *_info arrays
 */
void read_edit_files(void) {
	set_file_paths();
	init_game_constants();
	init_arrays();
}

struct chunk *t_build_arena(int height, int width) {
	if (!height)
		height = z_info->dungeon_hgt;
	if (!width)
		width = z_info->dungeon_wid;
	struct chunk *c = cave_new(height, width);

	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			square_set_feat(c, loc(x, y), FEAT_FLOOR);

	for (int y = 0; y < height; y++) {
		square_set_feat(c, loc(0, y), FEAT_PERM);
		square_set_feat(c, loc(width - 1, y), FEAT_PERM);
	}

	for (int x = 0; x < width; x++) {
		square_set_feat(c, loc(x, 0), FEAT_PERM);
		square_set_feat(c, loc(x, height - 1), FEAT_PERM);
	}

	return c;
}

struct monster *t_add_monster(struct chunk *c, struct loc g, const char *race) {
	struct monster_race *r = lookup_monster(race);
	struct monster_group_info info = { 0, 0 };
	place_new_monster(c, g, r, false, false, info, ORIGIN_DROP);
	struct monster *m = square_monster(c, g);
	assert(m);
	return m;
}
