/* command/lookup
 *
 * Tests for command lookup
 *
 * Created by: myshkin
 *             1 May 2011
 */

#include "unit-test.h"
#include "obj-properties.h"
#include "object.h"
#include "cmds.h"
#include "ui-keymap.h"
#include "ui-event.h"
#include "ui-game.h"
#include "ui-input.h"
#include "z-virt.h"

int setup_tests(void **state) {
	cmd_init();
	*state = 0;
	return 0;
}

int teardown_tests(void *state) {
	mem_free(state);
	return 0;
}

/* Regression test for #1330 */
static int test_cmd_lookup_orig(void *state) {
	require(cmd_lookup('Z', KEYMAP_MODE_ORIG) == CMD_NULL);
	require(cmd_lookup('{', KEYMAP_MODE_ORIG) == CMD_INSCRIBE);
	require(cmd_lookup('u', KEYMAP_MODE_ORIG) == CMD_USE_STAFF);
	require(cmd_lookup('T', KEYMAP_MODE_ORIG) == CMD_TUNNEL);
	require(cmd_lookup('g', KEYMAP_MODE_ORIG) == CMD_PICKUP);
	require(cmd_lookup('G', KEYMAP_MODE_ORIG) == CMD_STUDY);
	require(cmd_lookup('+', KEYMAP_MODE_ORIG) == CMD_ALTER);
	
	ok;
}

/* Introduced after commit 8871070 added modes to cmd_lookup() calls */
static int test_cmd_lookup_rogue(void *state) {
	require(cmd_lookup('{', KEYMAP_MODE_ROGUE) == CMD_INSCRIBE);
	require(cmd_lookup('Z', KEYMAP_MODE_ROGUE) == CMD_USE_STAFF);
	require(cmd_lookup(KTRL('T'), KEYMAP_MODE_ROGUE) == CMD_TUNNEL);
	require(cmd_lookup('g', KEYMAP_MODE_ROGUE) == CMD_PICKUP);
	require(cmd_lookup('G', KEYMAP_MODE_ROGUE) == CMD_STUDY);
	require(cmd_lookup('+', KEYMAP_MODE_ROGUE) == CMD_ALTER);
	
	ok;
}

/*
 * Regression test: 'A' activates, and CMD_ACTIVATE can be found from it
 * (ZangbandTK).
 *
 * "Command pets" was given 'A' as well, and cmd_init() fills converted_list[]
 * by walking cmds_all in order without checking -- so the later list won and
 * CMD_ACTIVATE was left with no key in either keyset.  The visible half was
 * that 'A' opened the pet menu; the quiet half was that
 * cmd_lookup_key(CMD_ACTIVATE) returned 0, and object inscriptions are matched
 * against that value, so '@A1' stopped tagging an item for activation and '!A'
 * stopped asking before activating one.
 *
 * Both directions are checked because the reverse lookup is the one that broke
 * silently.
 */
static int test_cmd_lookup_activate(void *state) {
	require(cmd_lookup('A', KEYMAP_MODE_ORIG) == CMD_ACTIVATE);
	require(cmd_lookup('A', KEYMAP_MODE_ROGUE) == CMD_ACTIVATE);
	require(cmd_lookup_key(CMD_ACTIVATE, KEYMAP_MODE_ORIG) == 'A');
	require(cmd_lookup_key(CMD_ACTIVATE, KEYMAP_MODE_ROGUE) == 'A');

	ok;
}

const char *suite_name = "command/lookup";
struct test tests[] = {
	{ "cmd_lookup_orig",  test_cmd_lookup_orig },
	{ "cmd_lookup_rogue", test_cmd_lookup_rogue },
	{ "cmd_lookup_activate", test_cmd_lookup_activate },
	{ NULL, NULL }
};
