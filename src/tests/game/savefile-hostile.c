/* game/savefile-hostile
 *
 * What the loader does with a savefile it was not expecting (ZangbandTK,
 * review of 3.105-3.124).
 *
 * `game/saves` proves the corpus loads and `game/roundtrip` proves a character
 * survives a trip through the writer. Neither asks what happens when the file
 * is *wrong* -- truncated, corrupted, or simply written by a build that had
 * more of something than this one does. That is the case where the loader
 * should refuse cleanly, and it was the case where it read off the ends of
 * arrays instead.
 *
 * All three faults here are latent in ordinary play and none of them is
 * reachable from the corpus, which is why nothing had ever failed.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "monster.h"
#include "obj-util.h"
#include "object.h"
#include "player.h"
#include "player-birth.h"
#include "savefile.h"
#include "z-file.h"
#include "z-quark.h"
#include "z-util.h"
#include "z-virt.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** A scratch path inside the test's own directory. */
static const char *scratch(const char *leaf)
{
	static char path[1024];

	path_build(path, sizeof(path), ".", leaf);
	return path;
}

/** Whole file into memory; caller frees. Returns the length, or 0. */
static size_t slurp(const char *path, uint8_t **out)
{
	ang_file *f = file_open(path, MODE_READ, FTYPE_RAW);
	size_t len = 0, cap = 1 << 16;
	uint8_t *buf;

	if (!f) return 0;
	buf = mem_alloc(cap);
	for (;;) {
		int got;

		if (len == cap) {
			cap *= 2;
			buf = mem_realloc(buf, cap);
		}
		got = file_read(f, (char *) buf + len, cap - len);
		if (got <= 0) break;
		len += got;
	}
	file_close(f);
	*out = buf;
	return len;
}

static bool spit(const char *path, const uint8_t *buf, size_t len)
{
	ang_file *f = file_open(path, MODE_WRITE, FTYPE_RAW);
	bool wrote;

	/* Not `ok`: that is the unit-test macro that ends a test. */
	if (!f) return false;
	wrote = file_write(f, (const char *) buf, len);
	file_close(f);
	return wrote;
}

/**
 * A savefile block the loader has never heard of does not walk off the table.
 *
 * `find_loader()` scans `loaders[]` until it meets an entry whose name begins
 * with a NUL, and the table had no such entry -- so a block matching nothing
 * read one whole entry past the end of the array before giving up. That is the
 * case a savefile from another build produces, which is exactly when a clean
 * refusal matters. Under the ASAN build this is a global-buffer-overflow; under
 * the plain one it is a garbage `struct blockinfo` whose `loader` could in
 * principle be called.
 *
 * Written by hand rather than patched from a real save, because the file only
 * has to get as far as the first block header: eight bytes of magic, then a
 * 28-byte header naming a block nobody has.
 */
static int test_an_unknown_block_is_refused(void *state) {
	uint8_t file[8 + 28] = { 0 };
	const char *path = scratch("hostile-unknown.sav");

	/* The magic and the variant tag `check_header()` wants */
	file[0] = 83; file[1] = 97; file[2] = 118; file[3] = 101;
	file[4] = 'Z'; file[5] = 'Z'; file[6] = 'K'; file[7] = '1';

	/* A block called "nonesuch", version 1, of no length */
	my_strcpy((char *) file + 8, "nonesuch", 16);
	file[8 + 16] = 1;

	require(spit(path, file, sizeof(file)));
	require(!savefile_load(path, false));

	file_delete(path);
	ok;
}

/**
 * A savefile with more egos than this build has does not desynchronise.
 *
 * `rd_ignore()` read `1 + itype_size` bytes per ego only while the index was
 * below `z_info->e_max`, and did nothing at all for the rest -- so a file
 * written by a build with more egos left those bytes in the stream and
 * everything after them, the auto-inscriptions, was read out of the middle of
 * the ego data.
 *
 * Reproduced by shrinking `e_max` at load time rather than by inserting ego
 * records into the file. That is the same condition from the other side and it
 * uses real bytes: the file genuinely holds `file_e_max` egos and the build
 * genuinely wants fewer, which is the case the missing `else` mishandled.
 * Manufacturing the bytes would have tested the surgery instead.
 *
 * The auto-inscription is the probe because it is the first thing read after
 * the ego loop, so it is the first thing a misaligned stream destroys.
 */
static int test_a_file_with_more_egos_keeps_its_place(void *state) {
	const char *path = scratch("hostile-egos.sav");
	struct object_kind *kind = NULL;
	uint16_t kept_emax;
	int k;

	require(player_make_simple(NULL, "Warrior", "Tester"));
	prepare_next_level(player);

	for (k = 1; k < z_info->k_max && !kind; k++)
		if (k_info[k].name && strstr(k_info[k].name, "Ration"))
			kind = &k_info[k];
	notnull(kind);

	kind->note_aware = quark_add("@v1!k");
	require(savefile_save(path));

	/*
	 * Fewer egos than the file was written with, which is what a build that
	 * has dropped some looks like to `rd_ignore()`.
	 */
	kept_emax = z_info->e_max;
	require(kept_emax > 1);
	z_info->e_max = 1;

	/*
	 * And forget it first, which the first version of this test did not.
	 *
	 * `savefile_load()` does not clear `k_info[].note_aware`, so the value
	 * set above was still sitting in memory after the load and the test
	 * passed whatever the loader read. Removing the fix changed nothing,
	 * which is how the hole was found.
	 */
	for (k = 1; k < z_info->k_max; k++)
		k_info[k].note_aware = 0;
	require(!kind->note_aware);

	set_file_paths();
	require(savefile_load(path, false));
	z_info->e_max = kept_emax;

	for (k = 1; k < z_info->k_max; k++)
		if (k_info[k].name && strstr(k_info[k].name, "Ration"))
			break;
	require(k < z_info->k_max);
	require(k_info[k].note_aware);
	require(streq(quark_str(k_info[k].note_aware), "@v1!k"));

	file_delete(path);
	ok;
}

/**
 * A monster claiming more timed effects than exist does not overrun.
 *
 * `rd_monsters()` read a byte and then that many int16_ts straight into
 * `mon->m_timed[MON_TMD_MAX]`, which holds eleven. A file claiming more --
 * corrupt, truncated, or from a build with more kinds of effect -- wrote up to
 * two hundred and forty-four int16_ts past the end of the monster. A heap
 * overflow with file data in it.
 *
 * The count byte is found by its neighbours rather than by an offset: the
 * monster is given hit points nothing else on the level will have, and the
 * record puts `mspeed` and `energy` between those and the count. An offset
 * would go stale the next time a field moved, and would then be patching some
 * other byte and passing.
 *
 * The loader now refuses the file rather than clamping, which is the idiom
 * `rd_ignore()` already uses for a count it cannot honour, and it is what
 * makes this testable at all: any count large enough to overrun also
 * desynchronises the stream, and a desynchronised monster list reaches
 * `quit_fmt("Monster %d has no group")` and takes the process with it. A
 * refusal happens before any of that.
 */
static int test_a_monster_cannot_claim_more_timed_effects(void *state) {
	const char *path = scratch("hostile-timed.sav");
	struct monster *mon;
	uint8_t *buf, want[4];
	size_t len, i;
	bool patched = false;

	require(player_make_simple(NULL, "Warrior", "Tester"));
	player->depth = 1;
	prepare_next_level(player);

	mon = NULL;
	for (i = 1; i < (size_t) cave_monster_max(cave) && !mon; i++)
		if (cave_monster(cave, i)->race) mon = cave_monster(cave, i);
	require(mon);

	/* A signature no other field on the level is likely to hold */
	mon->hp = 0x5A5A;
	mon->maxhp = 0x5B5B;
	require(savefile_save(path));

	len = slurp(path, &buf);
	require(len > 0);

	/* hp then maxhp, little-endian, as `wr_s16b()` writes them */
	want[0] = 0x5A; want[1] = 0x5A; want[2] = 0x5B; want[3] = 0x5B;
	for (i = 0; i + 7 < len; i++) {
		if (memcmp(buf + i, want, 4) != 0) continue;

		/* +4 mspeed, +5 energy, +6 the count of timed effects */
		buf[i + 6] = 200;
		patched = true;
		break;
	}
	if (!patched) {
		printf("  the monster's hit points were not found in the savefile; "
			   "the record's layout has moved\n");
		mem_free(buf);
		require(false);
	}

	require(spit(path, buf, len));
	mem_free(buf);

	set_file_paths();
	require(!savefile_load(path, false));

	file_delete(path);
	ok;
}

const char *suite_name = "game/savefile-hostile";
struct test tests[] = {
	{ "an-unknown-block-is-refused", test_an_unknown_block_is_refused },
	{ "a-file-with-more-egos-keeps-its-place",
	  test_a_file_with_more_egos_keeps_its_place },
	{ "a-monster-cannot-claim-more-timed-effects",
	  test_a_monster_cannot_claim_more_timed_effects },
	{ NULL, NULL }
};
