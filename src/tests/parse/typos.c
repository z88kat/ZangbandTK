/* parse/typos
 *
 * A directive that names something fails when the name is wrong (ZangbandTK,
 * DEC-102).
 *
 * This is one test for a shape rather than six tests for six directives, and
 * the shape is the point. Angband's parsers look a name up, store whatever
 * came back, and return `PARSE_ERROR_NONE` -- so a data file with a typo in it
 * loads, and the thing the line was for silently is not there. It has been
 * found and fixed three separate times now: the unknown expression base
 * (`power-expr:B:PLAYER_LEVL:...`, which evaluated against zero), the unknown
 * class name in a power band, and the six below. Each was invisible until
 * somebody went looking, because the game starts and plays.
 *
 * So the table is the mechanism. Adding a directive that resolves a name means
 * adding a row here, and the row is three strings.
 *
 * What is asserted is only that the parse *fails* -- not which error it gives.
 * The code matters to nobody: what matters is that the file is refused rather
 * than quietly accepted, and pinning the particular error would make this test
 * something to edit every time one is renamed.
 */
#include "unit-test.h"
#include "test-utils.h"
#include "init.h"
#include "datafile.h"
#include "obj-init.h"
#include "object.h"
#include "player.h"
#include "player-mutation.h"
#include "z-util.h"
#include "z-virt.h"

static void swallow(const char *str) { (void) str; }

int setup_tests(void **state) {
	/* The guards below report through plog(); this test expects them. */
	plog_aux = swallow;

	/*
	 * The real game data, because half these directives resolve a name
	 * against it -- `act:` wants the activation list and `type:sword` wants
	 * the tvals, and without them the parser walks into a null table rather
	 * than returning the error being tested.
	 */
	set_file_paths();
	if (!init_angband()) return 1;

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * One directive, the lines that set a record up, and the line with the typo.
 *
 * `setup` is terminated by NULL. Every one of those lines must parse cleanly,
 * which is itself worth asserting: a fixture that has gone stale would
 * otherwise make the last line fail for the wrong reason and the test would
 * still pass.
 */
struct typo_case {
	const char *what;
	struct file_parser *parser;
	const char *setup[8];
	const char *bad;
	/*
	 * Some names cannot be resolved at the line they are written on, because
	 * what they name is not built until the file has been read -- a `bindui:`
	 * points at a UI entry table that does not exist yet, and a race's
	 * `mutation-affinity` points into a file that is parsed later still. For
	 * those the line is allowed to parse and the *file* has to be refused, so
	 * the assertion moves to `finish`.
	 */
	bool at_finish;
};

static struct typo_case cases[] = {
	{
		"ego act: names an activation that does not exist",
		&ego_parser,
		{ "name:of Nothing", "type:sword", NULL },
		"act:No Such Activation",
		false
	},
	{
		"artifact act: names an activation that does not exist",
		&artifact_parser,
		{ "name:Nothing", "base-object:sword:Main Gauche", NULL },
		"act:No Such Activation",
		false
	},
	{
		"power-expr: with no dice to bind it to",
		&p_race_parser,
		{ "name:Tester", "power:do a thing", "power-effect:TIMED_INC:SHIELD",
		  NULL },
		"power-expr:B:PLAYER_LEVEL:+ 0",
		false
	},
	{
		"virtues: more than the record can hold",
		&class_parser,
		{ "name:Tester", NULL },
		"virtues:JUSTICE | VALOUR | HONOUR | FAITH | HARMONY",
		false
	},
	{
		/*
		 * The odd one out, and it has to be: `p_race.txt` is read before
		 * `mutation.txt`, so the name cannot be checked until the mutations
		 * exist. The guard lives at the end of the mutation parser and walks
		 * the races, which is why this case corrupts a race and then parses a
		 * mutation file.
		 */
		"mutation-affinity: names a mutation that does not exist",
		&mutation_parser,
		{ "name:TESTER", "desc:A test.", "weight:1", NULL },
		NULL,
		true
	},
	{
		"armour: a scale that counts downwards",
		&p_race_parser,
		{ "name:Tester", NULL },
		"armour:20:-5",
		false
	},
};

/**
 * Every directive in the table refuses a name it cannot resolve.
 *
 * The parser is disposed of through its own `finish`/`cleanup` pair rather
 * than `parser_destroy()`, because that is what `run_parser()` does and it is
 * the only disposal each parser is known to support: `finish` installs what
 * was parsed into the game's globals and destroys the parser, and `cleanup`
 * frees what `finish` installed.
 */
static int test_a_name_that_resolves_to_nothing_is_refused(void *state) {
	size_t n;
	char *bad_affinity = NULL;

	for (n = 0; n < N_ELEMENTS(cases); n++) {
		const struct typo_case *c = &cases[n];
		struct parser *p = c->parser->init();
		enum parser_error r;
		int i;

		notnull(p);

		for (i = 0; c->setup[i]; i++) {
			r = parser_parse(p, c->setup[i]);
			if (r != PARSE_ERROR_NONE) {
				printf("  %s: the fixture line \"%s\" no longer parses (%s); "
					   "the test is stale, not the code\n",
					   c->what, c->setup[i], parser_error_str[r]);
				require(false);
			}
		}

		/*
		 * A case with no bad line of its own puts the fault somewhere else
		 * first. Only the mutation one does, and what it breaks is a race's
		 * affinity, which is what its guard reads.
		 */
		if (!c->bad) {
			bad_affinity = (char *) races->mutation_affinity;
			races->mutation_affinity = string_make("NO_SUCH_MUTATION");
			r = PARSE_ERROR_NONE;
		} else {
			r = parser_parse(p, c->bad);
		}

		if (c->at_finish) {
			/*
			 * The line itself is fine; the file must not be. `finish`
			 * destroys the parser whatever it answers, so there is nothing
			 * left to dispose of here.
			 */
			if (r != PARSE_ERROR_NONE) {
				printf("  %s: \"%s\" was refused at the line, which this "
					   "case does not expect\n", c->what, c->bad);
				require(false);
			}
			{
				errr fin = c->parser->finish(p);

				if (bad_affinity) {
					string_free((char *) races->mutation_affinity);
					races->mutation_affinity = bad_affinity;
					bad_affinity = NULL;
				}

				if (fin == 0) {
					printf("  %s: it survived the whole file\n", c->what);
					require(false);
				}
			}
			continue;
		}

		if (r == PARSE_ERROR_NONE) {
			printf("  %s: \"%s\" was accepted\n", c->what, c->bad);
			require(false);
		}

		/*
		 * The stub record is left behind on purpose, and it is worth saying
		 * why rather than leaving somebody to find it.
		 *
		 * The supported disposal is the parser's own `finish` and `cleanup`
		 * pair: `finish` installs what was parsed into the game's globals and
		 * `cleanup` frees what it installed. Both are wrong here -- the game
		 * data is loaded, so `finish` would replace the real ego, artifact,
		 * race, class or property list with a one-record stub and `cleanup`
		 * would then free it, leaving `cleanup_angband()` to walk a dangling
		 * list. Freeing the priv by hand instead means six disposers that
		 * each have to know a different struct, and disposal written from
		 * guesswork is a worse bug than the one this test is for.
		 *
		 * So: six small records, in a binary that exits immediately. No CI
		 * job we have would see them -- LeakSanitizer is unavailable under
		 * clang on Windows, which is the only ASAN job, and on macOS, which
		 * is where `check-build` runs it locally. If somebody turns LSan on,
		 * this is what they will find, and this is why.
		 */
		parser_destroy(p);
	}

	ok;
}

const char *suite_name = "parse/typos";
struct test tests[] = {
	{ "a-name-that-resolves-to-nothing-is-refused",
	  test_a_name_that_resolves_to_nothing_is_refused },
	{ NULL, NULL }
};
