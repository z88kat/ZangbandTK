/* player/backstory — that every race's backstory is its own and finishes (PLR-39)
 *
 * The seventeen races imported from Zangband all carried `history:1`, which is
 * the chain Angband writes for a Human, so a Golem's character sheet described
 * its blue eyes and straight black hair and a Skeleton's gave it a complexion.
 * DEC-118 gave each of them a chain; this is what holds those chains together.
 *
 * The assertions here are structural rather than literary, because the thing
 * that breaks is structure. A chart is a linked list of alternatives, each with
 * a cumulative roll, and `get_history()` walks `succ` until it reaches NULL
 * ([player-birth.c](../../player-birth.c)). Three things can go wrong and none
 * of them is visible by reading the data file: a chain that never terminates, a
 * chart whose last alternative stops below 100 -- which does not misbehave, it
 * trips the `assert(entry)` in `get_history()` -- and a chain that ends on a
 * fragment written to be continued, so the sheet reads "You have blue eyes,"
 * and stops.
 */
#include "unit-test.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "test-utils.h"
#include "z-virt.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	(void) test_seed_rng_reported(suite_name);
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * Every race's chain reaches an end, and does so without going round.
 *
 * Walked exhaustively rather than sampled: every alternative of every chart is
 * followed, so a dead branch cannot hide behind a roll that rarely picks it.
 * The depth bound is the cycle detector -- the longest real chain is eight
 * charts, so anything past sixty-four is a loop rather than a long story.
 */
/*
 * Cycle detection by marking the path, not by bounding the depth.
 *
 * The first version of this bounded recursion at 64 and that was wrong in a
 * way only falsification showed: against a chart that points at itself the
 * branching factor is four, so it explores 4^64 paths and hangs long before
 * the bound means anything. Falsifying the cycle case did not fail the test,
 * it wedged the suite. Two marks fix it -- one for the charts on the current
 * path, which is what a cycle is, and one for charts already cleared, so a
 * shared tail is walked once rather than once per race that reaches it.
 */
#define BACKSTORY_MAX_CHARTS 512

struct walk {
	const struct history_chart *path[BACKSTORY_MAX_CHARTS];
	const struct history_chart *done[BACKSTORY_MAX_CHARTS];
	int npath, ndone;
	bool cutoffs;
};

static bool marked(const struct history_chart **a, int n,
				   const struct history_chart *c)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] == c) return true;

	return false;
}

static bool walk_ok(struct walk *w, const struct history_chart *c)
{
	const struct history_entry *e;
	int last = 0;

	if (!c) return true;
	if (marked(w->path, w->npath, c)) return false;   /* a cycle */
	if (marked(w->done, w->ndone, c)) return true;    /* already cleared */

	if (w->cutoffs) {
		for (e = c->entries; e; e = e->next) last = e->roll;
		if (last != 100) return false;
	}

	if (w->npath >= BACKSTORY_MAX_CHARTS) return false;
	w->path[w->npath++] = c;

	for (e = c->entries; e; e = e->next)
		if (!walk_ok(w, e->succ)) return false;

	w->npath--;
	if (w->ndone < BACKSTORY_MAX_CHARTS) w->done[w->ndone++] = c;

	return true;
}

static bool chain_ends(const struct history_chart *c, int unused)
{
	struct walk w;

	memset(&w, 0, sizeof(w));
	w.cutoffs = false;

	return walk_ok(&w, c);
}

static bool charts_reach_100(const struct history_chart *c, int unused)
{
	struct walk w;

	memset(&w, 0, sizeof(w));
	w.cutoffs = true;

	return walk_ok(&w, c);
}

static int test_every_race_history_terminates(void *state) {
	const struct player_race *r;
	int seen = 0;

	for (r = races; r; r = r->next) {
		notnull(r->history);
		require(chain_ends(r->history, 0));
		seen++;
	}
	require(seen >= 28);
	ok;
}

/**
 * Every chart a race can reach runs its rolls all the way to 100.
 *
 * `get_history()` rolls 1 to 100 and takes the first alternative whose cutoff
 * is not below the roll. A chart whose last cutoff is 97 therefore has three
 * rolls in a hundred that match nothing, and the function asserts rather than
 * returning short. That is a crash on one character in thirty, which is exactly
 * the kind of thing that reaches a player rather than a test.
 */
static int test_every_chart_reaches_one_hundred(void *state) {
	const struct player_race *r;

	for (r = races; r; r = r->next)
		require(charts_reach_100(r->history, 0));

	ok;
}

/**
 * Every race produces a sentence that ends like one.
 *
 * This is the test the Skeleton and the Spectre exist for. Their chains cannot
 * end on eyes, hair and complexion, so they end on a terminal line instead, and
 * the way to know that was done is that the generated string finishes on a full
 * stop. Sampled forty times per race because the chains branch -- one roll
 * proves one path.
 */
static int test_every_race_history_ends_in_a_full_stop(void *state) {
	const struct player_race *r;
	int i;

	for (r = races; r; r = r->next) {
		/*
		 * Guarded, because `get_history()` has no cycle check: against a
		 * chain that loops it appends for ever rather than returning, and
		 * this test would hang instead of failing. Found by falsification --
		 * pointing the Spectre's terminal chart at itself hung the suite
		 * rather than reporting it, which is the failure mode that eats a CI
		 * job for six hours.
		 */
		require(chain_ends(r->history, 0));

		for (i = 0; i < 40; i++) {
			char *h = get_history(r->history);
			size_t n;

			notnull(h);
			n = strlen(h);
			require(n > 0);
			require(h[n - 1] == '.');
			string_free(h);
		}
	}
	ok;
}

/**
 * Only the two races that should share chart 1 still do.
 *
 * Human and Dunadan share it in Angband's own design and are correct. Every
 * other race landing there is the defect DEC-118 fixed, and it is the shape the
 * defect would take again: a race added without a chain gets whatever the
 * default is, and the default reads as a human.
 */
static int test_only_human_and_dunadan_use_chart_one(void *state) {
	const struct player_race *r;
	int on_one = 0;

	for (r = races; r; r = r->next) {
		if (r->history && r->history->idx == 1) {
			on_one++;
			require(streq(r->name, "Human") || streq(r->name, "Dunadan"));
		}
	}

	/* And both of them really are still there. */
	eq(on_one, 2);
	ok;
}

const char *suite_name = "player/backstory";
struct test tests[] = {
	{ "every-race-history-terminates",
	  test_every_race_history_terminates },
	{ "every-chart-reaches-one-hundred",
	  test_every_chart_reaches_one_hundred },
	{ "every-race-history-ends-in-a-full-stop",
	  test_every_race_history_ends_in_a_full_stop },
	{ "only-human-and-dunadan-use-chart-one",
	  test_only_human_and_dunadan_use_chart_one },
	{ NULL, NULL }
};
