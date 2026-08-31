/* test-utils.h 
 *
 * Function prototypes for test-utils
 *
 * Created by: myshkin
 *             26 Apr 2011
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "z-type.h"

void set_file_paths(void);
void read_edit_files(void);

/* Seed the RNG for a suite that depends on what the world came out like, and
 * report the seed so a failure can be run again.  Call after init_angband(),
 * which seeds from the clock and the process id and so gives a different world
 * every run.  Set ZTK_TEST_SEED to replay a reported one.  Returns the seed. */
uint32_t test_seed_rng(void);
uint32_t test_seed_rng_reported(const char *suite);

/* Build a savefile name of this process's own, so that two copies of the suite
 * running at once do not read each other's saves.  The caller owns the buffer:
 * a save and the load that checks it are far apart, and a shared one would be
 * overwritten in between. */
void test_savefile_name(char *buf, size_t len, const char *stem);

/* Build an arena - an empty level with a permanent wall around the perimeter.
 * You can pass 0 for height or width, in which case the defaults from z_info
 * will be used. */
struct chunk *t_build_arena(int height, int width);

/* Generate a monster of the named race, place it at the given location, and
 * return it. This function cannot return NULL. */
struct monster *t_add_monster(struct chunk *c, struct loc g, const char *race);

#endif /* TEST_UTIL_H */
