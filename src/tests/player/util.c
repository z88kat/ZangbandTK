/* player/birth */

#include "unit-test.h"
#include "unit-test-data.h"
#include "player-birth.h"
#include "player-quest.h"
#include "option.h"
#include "player-util.h"

int setup_tests(void **state) {
	struct player *p = mem_zalloc(sizeof *p);
	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->pack_size = 23;
	z_info->quest_max = 1;
	z_info->quest_fixed = 1;
	z_info->quiver_size = 10;
	quests = &test_quest;
	player_init(p);
	*state = p;
	return 0;
}

int teardown_tests(void *state) {
	struct player *p = state;
	player_quests_free((struct player *)state);
	mem_free(z_info);
	mem_free(p->upkeep->inven);
	mem_free(p->upkeep->quiver);
	mem_free(p->upkeep);
	mem_free(p->timed);
	mem_free(p->obj_k->brands);
	mem_free(p->obj_k->slays);
	mem_free(p->obj_k->curses);
	mem_free(p->obj_k);
	mem_free(state);
	return 0;
}

static int test_adjust_hp_precise(void *state) {
	struct player *p = state;
	struct {
		int16_t curr_in;
		uint16_t frac_in;
		int16_t max_in;
		int32_t gain_in;
		int16_t curr_out;
		uint16_t frac_out;
		bool signaled_out;
	} cases[] = {
		/* Check adding zero. */
		{ 0, 0, 50, 0, 0, 0, false },
		{ 0, 891, 50, 0, 0, 891, false },
		{ 5, 0, 50, 0, 5, 0, false },
		{ 15, 750, 50, 0, 15, 750, false },
		{ -10, 0, 50, 0, -10, 0, false },
		{ -12, 131, 50, 0, -12, 131, false },
		/* Check adding to zero hp. */
		{ 0, 0, 50, 65536, 1, 0, true },
		{ 0, 0, 50, 7561, 0, 7561, false },
		{ 0, 0, 50, 262222, 4, 78, true },
		{ 0, 0, 50, 3276800, 50, 0, true },
		{ 0, 0, 50, 3302230, 50, 0, true },
		{ 0, 0, 50, 3932160, 50, 0, true },
		{ 0, 0, 50, -37, -1, 65499, true },
		{ 0, 0, 50, -131072, -2, 0, true },
		{ 0, 0, 50, -196533, -3, 75, true },
		/* Check adding to a positive fraction but no whole part. */
		{ 0, 1034, 50, 196608, 3, 1034, true },
		{ 0, 1034, 50, 5345, 0, 6379, false },
		{ 0, 1034, 50, 64504, 1, 2, true },
		{ 0, 1034, 50, 64502, 1, 0, true },
		{ 0, 1034, 50, 262222, 4, 1112, true },
		{ 0, 1034, 50, 3275766, 50, 0, true },
		{ 0, 1034, 50, 3276800, 50, 0, true },
		{ 0, 1034, 50, 3302230, 50, 0, true },
		{ 0, 1034, 50, 3604480, 50, 0, true },
		{ 0, 1034, 50, -997, 0, 37, false },
		{ 0, 1034, 50, -2048, -1, 64522, true },
		{ 0, 1034, 50, -262144, -4, 1034, true },
		{ 0, 1034, 50, -324263, -5, 4451, true },
		/* Check adding to a positive fraction and whole part. */
		{ 45, 45637, 50, 262144, 49, 45637, true },
		{ 45, 45637, 50, 75, 45, 45712, false },
		{ 45, 45637, 50, 19899, 46, 0, true },
		{ 45, 45637, 50, 21000, 46, 1101, true },
		{ 45, 45637, 50, 282040, 49, 65533, true },
		{ 45, 45637, 50, 282043, 50, 0, true },
		{ 45, 45637, 50, 283000, 50, 0, true },
		{ 45, 45637, 50, 458822, 50, 0, true },
		{ 45, 45637, 50, -4512, 45, 41125, false },
		{ 45, 45637, 50, -45637, 45, 0, false },
		{ 45, 45637, 50, -59331, 44, 51842, true },
		{ 45, 45637, 50, -393216, 39, 45637, true },
		{ 45, 45637, 50, -462233, 38, 42156, true },
		{ 45, 45637, 50, -2994757, 0, 0, true },
		{ 45, 45637, 50, -2995000, -1, 65293, true },
		{ 45, 45637, 50, -3060293, -1, 0, true },
		{ 45, 45637, 50, -3932160, -15, 45637, true },
		/* Check adding to a negative value with no fractional part. */
		{ -10, 0, 50, 567, -10, 567, false },
		{ -10, 0, 50, 65536, -9, 0, true },
		{ -10, 0, 50, 128701, -9, 63165, true },
		{ -10, 0, 50, 655360, 0, 0, true },
		{ -10, 0, 50, 658360, 0, 3000, true },
		{ -10, 0, 50, 796888, 2, 10456, true },
		{ -10, 0, 50, -1, -11, 65535, true },
		{ -10, 0, 50, -65536, -11, 0, true },
		{ -10, 0, 50, -172827, -13, 23781, true },
		{ -10, 0, 50, 3932160, 50, 0, true },
		{ -10, 0, 50, 3933160, 50, 0, true },
		/* Check adding to a negative value with a fractional part. */
		{ -8, 53471, 50, 3871, -8, 57342, false },
		{ -8, 53471, 50, 12065, -7, 0, true },
		{ -8, 53471, 50, 65536, -7, 53471, true },
		{ -8, 53471, 50, 470817, 0, 0, true },
		{ -8, 53471, 50, 473817, 0, 3000, true },
		{ -8, 53471, 50, 655360, 2, 53471, true },
		{ -8, 53471, 50, 3747617, 50, 0, true },
		{ -8, 53471, 50, 3751617, 50, 0, true },
		{ -8, 53471, 50, 3932160, 50, 0, true }, 
		{ -8, 53471, 50, -198, -8, 53273, false },
		{ -8, 53471, 50, -53471, -8, 0, false },
		{ -8, 53471, 50, -60000, -9, 59007, true },
		{ -8, 53471, 50, -131072, -10, 53471, true },
		{ -8, 53471, 50, -201708, -11, 48371, true },
		/* Check overflow handling. */
		{ INT16_MAX, 0, INT16_MAX, 100000, INT16_MAX, 0, false },
		{ INT16_MAX, 65535, 50, 10, 50, 0, true },
		{ INT16_MIN, 0, 50, -131072, INT16_MIN, 0, false },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(cases); ++i) {
		p->chp = cases[i].curr_in;
		p->chp_frac = cases[i].frac_in;
		p->mhp = cases[i].max_in;
		p->upkeep->redraw &= ~(PR_HP);
		player_adjust_hp_precise(p, cases[i].gain_in);
		eq(p->chp, cases[i].curr_out);
		eq(p->chp_frac, cases[i].frac_out);
		eq(p->mhp, cases[i].max_in);
		if (cases[i].signaled_out) {
			require(p->upkeep->redraw & (PR_HP));
		} else {
			require(!(p->upkeep->redraw & (PR_HP)));
		}
	}
	ok;
}

static int test_adjust_sp_precise(void *state) {
	struct player *p = state;
	struct {
		int16_t curr_in;
		uint16_t frac_in;
		int16_t max_in;
		int32_t gain_in;
		int16_t curr_out;
		uint16_t frac_out;
		int32_t rtn;
		bool signaled_out;
	} cases[] = {
		/* Check adding zero. */
		{ 0, 0, 50, 0, 0, 0, 0, false },
		{ 0, 891, 50, 0, 0, 891, 0, false },
		{ 5, 0, 50, 0, 5, 0, 0, false },
		{ 15, 750, 50, 0, 15, 750, 0, false },
		/* Check adding to zero spell points. */
		{ 0, 0, 50, 65536, 1, 0, 65536, true },
		{ 0, 0, 50, 7561, 0, 7561, 7561, false },
		{ 0, 0, 50, 262222, 4, 78, 262222, true },
		{ 0, 0, 50, 3276800, 50, 0, 3276800, true },
		{ 0, 0, 50, 3302230, 50, 0, 3276800, true },
		{ 0, 0, 50, 3932160, 50, 0, 3276800, true },
		{ 0, 0, 50, -37, 0, 0, 0, false },
		{ 0, 0, 50, -131072, 0, 0, 0, false },
		{ 0, 0, 50, -196533, 0, 0, 0, false },
		/* Check adding to a positive fraction but no whole part. */
		{ 0, 1034, 50, 196608, 3, 1034, 196608, true },
		{ 0, 1034, 50, 5345, 0, 6379, 5345, false },
		{ 0, 1034, 50, 64504, 1, 2, 64504, true },
		{ 0, 1034, 50, 64502, 1, 0, 64502, true },
		{ 0, 1034, 50, 262222, 4, 1112, 262222, true },
		{ 0, 1034, 50, 3275766, 50, 0, 3275766, true },
		{ 0, 1034, 50, 3276800, 50, 0, 3275766, true },
		{ 0, 1034, 50, 3302230, 50, 0, 3275766, true },
		{ 0, 1034, 50, 3604480, 50, 0, 3275766, true },
		{ 0, 1034, 50, -997, 0, 37, -997, false },
		{ 0, 1034, 50, -2048, 0, 0, -1034, false },
		{ 0, 1034, 50, -262144, 0, 0, -1034, false },
		{ 0, 1034, 50, -324263, 0, 0, -1034, false },
		/* Check adding to a positive fraction and whole part. */
		{ 45, 45637, 50, 262144, 49, 45637, 262144, true },
		{ 45, 45637, 50, 75, 45, 45712, 75, false },
		{ 45, 45637, 50, 19899, 46, 0, 19899, true },
		{ 45, 45637, 50, 21000, 46, 1101, 21000, true },
		{ 45, 45637, 50, 282040, 49, 65533, 282040, true },
		{ 45, 45637, 50, 282043, 50, 0, 282043, true },
		{ 45, 45637, 50, 283000, 50, 0, 282043, true },
		{ 45, 45637, 50, 458822, 50, 0, 282043, true },
		{ 45, 45637, 50, -4512, 45, 41125, -4512, false },
		{ 45, 45637, 50, -45637, 45, 0, -45637, false },
		{ 45, 45637, 50, -59331, 44, 51842, -59331, true },
		{ 45, 45637, 50, -393216, 39, 45637, -393216, true },
		{ 45, 45637, 50, -462233, 38, 42156, -462233, true },
		{ 45, 45637, 50, -2994757, 0, 0, -2994757, true },
		{ 45, 45637, 50, -2995000, 0, 0, -2994757, true },
		{ 45, 45637, 50, -3060293, 0, 0, -2994757, true },
		{ 45, 45637, 50, -3932160, 0, 0, -2994757, true },
		/* Check overflow handling. */
		{ INT16_MAX, 0, INT16_MAX, 196608, INT16_MAX, 0, 0, false },
		{ INT16_MAX, 65535, 50, 10, 50, 0,
			(50 - INT16_MAX) * 65536 - 65535, true },
		/*
		 * Skip this one; trigger compiler warnings about overflowing
		 * an int32_t on systems that use two's complement.
		 */
		/* { INT16_MIN, 0, 50, -131072, 0, 0, INT16_MIN * (-65536), true }, */
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(cases); ++i) {
		int32_t result;

		p->csp = cases[i].curr_in;
		p->csp_frac = cases[i].frac_in;
		p->msp = cases[i].max_in;
		p->upkeep->redraw &= ~(PR_MANA);
		result = player_adjust_mana_precise(p, cases[i].gain_in);
		eq(p->csp, cases[i].curr_out);
		eq(p->csp_frac, cases[i].frac_out);
		eq(result, cases[i].rtn);
		eq(p->msp, cases[i].max_in);
		if (cases[i].signaled_out) {
			require(p->upkeep->redraw & (PR_MANA));
		} else {
			require(!(p->upkeep->redraw & (PR_MANA)));
		}
	}
	ok;
}


/**
 * A character that has cheated says so, by either of the two records of it.
 *
 * There are two, kept separately and for different reasons: `noscore` for
 * wizard mode, the debug commands and the borg, and the hidden OP_SCORE twin of
 * each option on the cheat screen.  enter_score() has always consulted both, and
 * the status indicator asks the same question through the same function, so this
 * pins the pair together.  If a third way to cheat is ever added and only one of
 * them learns about it, a character can be barred from the score list while the
 * screen still says they were playing straight -- or, worse, the other way
 * round.
 */
static int test_player_has_cheated(void *state) {
	struct player *p = state;
	int j, score_opt = -1;

	/* Straight, to start with. */
	p->noscore = 0;
	for (j = 0; j < OPT_MAX; ++j)
		if (option_type(j) == OP_SCORE)
			p->opts.opt[j] = false;
	require(!player_has_cheated(p));

	/* Wizard mode marks the character, and turning it back off does not
	 * unmark them -- the record is of having used it. */
	p->noscore |= NOSCORE_WIZARD;
	require(player_has_cheated(p));
	p->noscore = 0;
	require(!player_has_cheated(p));

	/* So do the debug commands. */
	p->noscore |= NOSCORE_DEBUG;
	require(player_has_cheated(p));
	p->noscore = 0;

	/* And so does any one of the cheat options, by its score twin. */
	for (j = 0; j < OPT_MAX; ++j) {
		if (option_type(j) != OP_SCORE) continue;
		score_opt = j;

		require(!player_has_cheated(p));
		p->opts.opt[j] = true;
		require(player_used_cheat_option(p));
		require(player_has_cheated(p));
		p->opts.opt[j] = false;
	}

	/* There is at least one, or the loop above tested nothing. */
	require(score_opt >= 0);

	ok;
}

const char *suite_name = "player/util";
struct test tests[] = {
	{ "adjust_hp_precise", test_adjust_hp_precise },
	{ "adjust_sp_precise", test_adjust_sp_precise },
	{ "player_has_cheated", test_player_has_cheated },
	{ NULL, NULL }
};
