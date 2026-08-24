/**
 * \file player-quest.c
 * \brief All quest-related code
 *
 * Copyright (c) 2013 Angband developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "datafile.h"
#include "init.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-quest.h"

/**
 * Array of quests
 */
struct quest *quests;

/**
 * Parsing functions for quest.txt
 */
static enum parser_error parse_quest_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct quest *h = parser_priv(p);

	struct quest *q = mem_zalloc(sizeof(*q));
	q->next = h;
	parser_setpriv(p, q);
	q->name = string_make(name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_quest_level(struct parser *p) {
	struct quest *q = parser_priv(p);
	assert(q);

	q->level = parser_getuint(p, "level");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_quest_race(struct parser *p) {
	struct quest *q = parser_priv(p);
	const char *name = parser_getstr(p, "race");
	assert(q);

	q->race = lookup_monster(name);
	if (!q->race)
		return PARSE_ERROR_INVALID_MONSTER;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_quest_number(struct parser *p) {
	struct quest *q = parser_priv(p);
	assert(q);

	q->max_num = parser_getuint(p, "number");
	return PARSE_ERROR_NONE;
}

struct parser *init_parse_quest(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_quest_name);
	parser_reg(p, "level uint level", parse_quest_level);
	parser_reg(p, "race str race", parse_quest_race);
	parser_reg(p, "number uint number", parse_quest_number);
	return p;
}

static errr run_parse_quest(struct parser *p) {
	return parse_file_quit_not_found(p, "quest");
}

static errr finish_parse_quest(struct parser *p) {
	struct quest *quest, *next = NULL;
	int count;

	/* Count the entries */
	z_info->quest_max = 0;
	quest = parser_priv(p);
	while (quest) {
		z_info->quest_max++;
		quest = quest->next;
	}

	/* Allocate the direct access list and copy the data to it */
	quests = mem_zalloc(z_info->quest_max * sizeof(*quest));
	count = z_info->quest_max - 1;
	for (quest = parser_priv(p); quest; quest = next, count--) {
		memcpy(&quests[count], quest, sizeof(*quest));
		quests[count].index = count;
		next = quest->next;
		if (count < z_info->quest_max - 1)
			quests[count].next = &quests[count + 1];
		else
			quests[count].next = NULL;

		mem_free(quest);
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_quest(void)
{
	int idx;
	for (idx = 0; idx < z_info->quest_max; idx++)
		string_free(quests[idx].name);
	mem_free(quests);
}

struct file_parser quests_parser = {
	"quest",
	init_parse_quest,
	run_parse_quest,
	finish_parse_quest,
	cleanup_quest
};

/**
 * Check if the given level is a quest level.
 */
bool is_quest(struct player *p, int level)
{
	size_t i;

	/* Town is never a quest */
	if (!level) return false;

	/*
	 * ZangbandTK (WLD-20): a finished quest no longer holds its level.  4.2 said
	 * the same thing by zeroing the level, which worked while completion was the
	 * only state a quest could reach.
	 */
	for (i = 0; i < z_info->quest_max; i++)
		if (p->quests[i].level == level &&
			p->quests[i].state != QUEST_FINISHED)
			return true;

	return false;
}

/**
 * Copy all the standard quests to the player quest history
 */
void player_quests_reset(struct player *p)
{
	size_t i;

	if (p->quests)
		player_quests_free(p);
	p->quests = mem_zalloc(z_info->quest_max * sizeof(struct quest));

	for (i = 0; i < z_info->quest_max; i++) {
		p->quests[i].name = string_make(quests[i].name);
		p->quests[i].level = quests[i].level;
		p->quests[i].race = quests[i].race;
		p->quests[i].max_num = quests[i].max_num;

		/*
		 * The quests from quest.txt are the ones the game is won by finishing,
		 * and a character is on them from birth -- there is nobody to accept
		 * them from (ZangbandTK, WLD-20).
		 */
		p->quests[i].fixed = true;
		p->quests[i].state = QUEST_TAKEN;
	}
}

/**
 * Free the player quests
 */
void player_quests_free(struct player *p)
{
	size_t i;

	for (i = 0; i < z_info->quest_max; i++)
		string_free(p->quests[i].name);
	mem_free(p->quests);
}

/**
 * Creates magical stairs after finishing a quest monster.
 */
static void build_quest_stairs(struct player *p, struct loc grid)
{
	struct loc new_grid = p->grid;

	/* Stagger around */
	while (!square_changeable(cave, grid) &&
		   square_ispassable(cave, grid) &&
		   !square_isdoor(cave, grid)) {
		/* Pick a location */
		scatter(cave, &new_grid, grid, 1, false);

		/* Stagger */
		grid = new_grid;
	}

	/* Push any objects */
	push_object(grid);

	/* Explain the staircase */
	msg("A magical staircase appears...");

	/* Create stairs down */
	square_set_feat(cave, grid, FEAT_MORE);

	/* Update the visuals */
	p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
}

/**
 * Check if this (now dead) monster is a quest monster, and act appropriately
 */
bool quest_check(struct player *p, const struct monster *m)
{
	int i, unfinished = 0;
	bool completed = false;

	/* Mark quests as complete */
	for (i = 0; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		/* Note completed quests */
		if (cave->depth == q->level && m->race == q->race &&
			q->state == QUEST_TAKEN) {
			q->cur_num++;

			if (q->cur_num == q->max_num) {
				/*
				 * ZangbandTK (WLD-20): the objective is met.  A fixed quest has
				 * nobody to report to, so it closes here; a quest taken from
				 * somebody waits at COMPLETE until they are told.
				 */
				q->state = q->fixed ? QUEST_FINISHED : QUEST_COMPLETE;
				completed = true;
			}
		}

		/*
		 * Count only the quests the game ends on.  This was "quests with a level
		 * left", which was the same set while every quest came from quest.txt and
		 * lasted for the whole game.  Once a quest can be taken from a townsman
		 * and handed back, that count reaches zero on an ordinary afternoon --
		 * and the player would be told they had won for delivering a parcel
		 * (ZangbandTK, WLD-20).
		 */
		if (q->fixed && q->state != QUEST_FINISHED) unfinished++;
	}

	if (completed) {
		/* Build magical stairs */
		build_quest_stairs(p, m->grid);

		/* Nothing left, game over... */
		if (unfinished == 0) {
			p->total_winner = true;
			p->upkeep->redraw |= (PR_TITLE);
			msg("*** CONGRATULATIONS ***");
			msg("You have won the game!");
			msg("You may retire (key is shift-q) when you are ready.");
		}

		return true;
	} else {
		return false;
	}
}
