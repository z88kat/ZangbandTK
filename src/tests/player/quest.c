/* player/quest.c
 *
 * The quest lifecycle (ZangbandTK, WLD-20).
 *
 * Angband 4.2 has two quests, both alive from birth, and marks one done by
 * zeroing the level it lives on.  That says everything there is to say about a
 * quest that can only be finished -- and nothing at all about one that can be
 * accepted, carried around unfinished, and handed back.  These defend the
 * states, and in particular the one place where adding them changes an existing
 * rule: what it takes to win.
 */

#include "unit-test.h"
#include "unit-test-data.h"

#include "init.h"
#include "player-birth.h"
#include "player-quest.h"

int setup_tests(void **state) {
	struct player *p = mem_zalloc(sizeof *p);

	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->pack_size = 23;
	z_info->quest_max = 1;
	/*
	 * One quest, and it is a fixed one: quest_fixed is how many of the list
	 * come from quest.txt, and everything past it is a slot for work taken
	 * from a building (WLD-16d).
	 */
	z_info->quest_fixed = 1;
	z_info->quest_slots = 4;
	z_info->quest_max += z_info->quest_slots;
	z_info->quiver_size = 10;
	quests = &test_quest;
	player_init(p);
	player_quests_reset(p);
	*state = p;
	return 0;
}

int teardown_tests(void *state) {
	struct player *p = state;

	player_quests_free(p);
	mem_free(z_info);
	mem_free(p->upkeep->inven);
	mem_free(p->upkeep->quiver);
	mem_free(p->upkeep);
	mem_free(p->timed);
	mem_free(p->obj_k->brands);
	mem_free(p->obj_k->slays);
	mem_free(p->obj_k->curses);
	mem_free(p->obj_k);
	mem_free(p);
	return 0;
}

/**
 * A quest from quest.txt is one the character is on from birth.
 */
static int test_a_fixed_quest_starts_taken(void *state) {
	struct player *p = state;

	player_quests_reset(p);

	require(p->quests[0].fixed);
	eq(p->quests[0].state, QUEST_TAKEN);

	/* And it holds its level, rather than the level being the state. */
	eq(p->quests[0].level, test_quest.level);

	ok;
}

/**
 * A quest's level belongs to it until the quest is finished (WLD-20).
 *
 * is_quest() decides whether a depth is a quest depth, which is what stops the
 * player walking past it and what makes the level generate as a quest level.
 * 4.2 answered by asking whether the level field was still set, which is the
 * same question only while zeroing that field is how completion is recorded.
 */
static int test_a_level_stops_being_a_quest_when_finished(void *state) {
	struct player *p = state;
	int depth = test_quest.level;

	player_quests_reset(p);

	require(is_quest(p, depth));

	/* Taken but not done: still a quest level. */
	p->quests[0].state = QUEST_TAKEN;
	require(is_quest(p, depth));

	/* Done but not handed in: still a quest level. */
	p->quests[0].state = QUEST_COMPLETE;
	require(is_quest(p, depth));

	/* Closed: no longer. */
	p->quests[0].state = QUEST_FINISHED;
	require(!is_quest(p, depth));

	/* The level itself never moved. */
	eq(p->quests[0].level, depth);

	ok;
}

/**
 * The town is never a quest, whatever the quests say.
 */
static int test_the_town_is_never_a_quest(void *state) {
	struct player *p = state;

	player_quests_reset(p);
	p->quests[0].level = 0;
	p->quests[0].state = QUEST_TAKEN;

	require(!is_quest(p, 0));

	ok;
}

/**
 * Winning counts only the quests the game ends on (WLD-20).
 *
 * This is the rule that adding the lifecycle breaks if it is left alone, and it
 * is worth being explicit about.  4.2 declares the player a winner when no quest
 * has a level left, which is the same as "all of them are done" only while every
 * quest in the game came from quest.txt and lasted the whole game.  Once a quest
 * can be taken from somebody in a town and handed back an hour later, "nothing
 * outstanding" is an ordinary state of affairs -- and a character would be told
 * they had won the game for delivering a parcel.
 *
 * So the count is over fixed quests only.  quest_check() itself needs a live
 * cave and a dead monster, which this suite has neither of; what is checked here
 * is the predicate it counts on.
 */
static int test_winning_counts_only_fixed_quests(void *state) {
	struct player *p = state;
	int i, unfinished;

	player_quests_reset(p);

	/* A taken-from-somebody quest, complete and closed. */
	p->quests[0].fixed = false;
	p->quests[0].state = QUEST_FINISHED;

	unfinished = 0;
	for (i = 0; i < (int) z_info->quest_max; i++)
		if (p->quests[i].fixed && p->quests[i].state != QUEST_FINISHED)
			unfinished++;

	/* Nothing outstanding, and nothing won: there were no fixed quests. */
	eq(unfinished, 0);

	/* Whereas an unfinished fixed quest does hold the ending open. */
	p->quests[0].fixed = true;
	p->quests[0].state = QUEST_TAKEN;

	unfinished = 0;
	for (i = 0; i < (int) z_info->quest_max; i++)
		if (p->quests[i].fixed && p->quests[i].state != QUEST_FINISHED)
			unfinished++;

	eq(unfinished, 1);

	ok;
}

/**
 * Work is taken into a slot, carried, and handed back (WLD-16d).
 *
 * The slots past quest.txt are what makes a quest something that can be
 * accepted at all.  Without them the list holds only the quests the game is won
 * by, which nobody hands out and nobody hands back.
 */
static int test_work_is_taken_and_handed_back(void *state) {
	struct player *p = state;
	struct quest *q;

	player_quests_reset(p);

	/* Nothing carried to begin with. */
	null(quest_carried(p, false));
	null(quest_carried(p, true));

	q = quest_take(p, "3 orcs", &test_r_human, 3);
	notnull(q);

	/* It went into a slot past the fixed ones, and it is not one of them. */
	require(q->index >= 0);
	require(!q->fixed);
	eq(q->state, QUEST_TAKEN);
	eq(q->max_num, 3);
	eq(q->cur_num, 0);

	/* Carried, and not yet done. */
	require(quest_carried(p, false) == q);
	null(quest_carried(p, true));

	/* Done, and waiting to be reported. */
	q->state = QUEST_COMPLETE;
	null(quest_carried(p, false));
	require(quest_carried(p, true) == q);

	/* Handed back, and the slot is free again. */
	quest_hand_back(p, q);
	eq(q->state, QUEST_UNTAKEN);
	null(q->name);
	null(quest_carried(p, false));
	null(quest_carried(p, true));

	ok;
}

/**
 * Taking work never touches the quests the game ends on (WLD-16d, WLD-20).
 *
 * The two live in one array, and the fixed ones are at the front.  A bounty
 * written over Oberon would end the game the moment the character killed three
 * orcs -- or, worse, quietly make the ending unreachable.
 */
static int test_work_never_overwrites_the_endgame(void *state) {
	struct player *p = state;
	int i, taken = 0;

	player_quests_reset(p);

	/* Fill every slot there is, and then some. */
	for (i = 0; i < (int) z_info->quest_max + 4; i++)
		if (quest_take(p, "work", &test_r_human, 1)) taken++;

	/* Never more than the slots past the fixed quests. */
	eq(taken, (int) (z_info->quest_max - z_info->quest_fixed));

	/* And the fixed ones are untouched. */
	for (i = 0; i < (int) z_info->quest_fixed; i++) {
		require(p->quests[i].fixed);
		eq(p->quests[i].state, QUEST_TAKEN);
		require(streq(p->quests[i].name, quests[i].name));
	}

	ok;
}

const char *suite_name = "player/quest";
struct test tests[] = {
	{ "a-fixed-quest-starts-taken", test_a_fixed_quest_starts_taken },
	{ "a-level-stops-being-a-quest-when-finished",
	  test_a_level_stops_being_a_quest_when_finished },
	{ "work-is-taken-and-handed-back", test_work_is_taken_and_handed_back },
	{ "work-never-overwrites-the-endgame", test_work_never_overwrites_the_endgame },
	{ "the-town-is-never-a-quest", test_the_town_is_never_a_quest },
	{ "winning-counts-only-fixed-quests", test_winning_counts_only_fixed_quests },
	{ NULL, NULL }
};
