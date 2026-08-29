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
#include "dun-type.h"
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

/**
 * Which dungeon the quest is in (ZangbandTK, WLD-21).
 */
static enum parser_error parse_quest_dungeon(struct parser *p) {
	struct quest *h = parser_priv(p);
	const char *name = parser_getstr(p, "name");
	struct dun_type *type;

	if (!h) return PARSE_ERROR_MISSING_RECORD_HEADER;

	type = dun_type_by_name(name);
	if (!type) return PARSE_ERROR_INVALID_VALUE;

	h->dungeon = type->index + 1;

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
	parser_reg(p, "dungeon str name", parse_quest_dungeon);
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

	/*
	 * ZangbandTK (WLD-16d): and room for the quests that are taken rather than
	 * shipped.  The list is a fixed array, so the slots have to exist before
	 * anybody can be given anything; the ones past the file are empty and
	 * untaken until a building fills one in.
	 */
	z_info->quest_fixed = z_info->quest_max;
	z_info->quest_max += z_info->quest_slots;

	/* Allocate the direct access list and copy the data to it */
	quests = mem_zalloc(z_info->quest_max * sizeof(*quest));
	count = z_info->quest_fixed - 1;
	for (quest = parser_priv(p); quest; quest = next, count--) {
		memcpy(&quests[count], quest, sizeof(*quest));
		quests[count].index = count;
		next = quest->next;
		if (count < z_info->quest_fixed - 1)
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
	for (i = 0; i < z_info->quest_max; i++) {
		const struct quest *q = &p->quests[i];

		if (q->level != level) continue;
		if (q->state == QUEST_FINISHED) continue;

		/*
		 * And in the right dungeon (ZangbandTK, WLD-21).  Without this, the
		 * hundredth level of the Abyss would hold the player the way the
		 * hundredth level of the Courts does, with nothing on it to kill.
		 */
		if (q->dungeon && q->dungeon != p->dungeon) continue;

		return true;
	}

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
		/*
		 * The slots past the file are empty until something is taken into one
		 * (WLD-16d).  string_make(NULL) would be a name of its own.
		 */
		if (i >= z_info->quest_fixed) {
			p->quests[i].state = QUEST_UNTAKEN;
			p->quests[i].fixed = false;
			continue;
		}

		p->quests[i].name = string_make(quests[i].name);
		p->quests[i].level = quests[i].level;
		p->quests[i].race = quests[i].race;
		p->quests[i].max_num = quests[i].max_num;
		p->quests[i].dungeon = quests[i].dungeon;

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
/**
 * Take a quest into the first free slot (ZangbandTK, WLD-16d).
 *
 * \return the quest, or NULL when the character is already carrying as many as
 * the list has room for.
 */
struct quest *quest_take(struct player *p, int type, const char *name,
						 struct monster_race *race, int number)
{
	int i;

	for (i = z_info->quest_fixed; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		if (q->state != QUEST_UNTAKEN) continue;

		string_free(q->name);
		q->name = string_make(name);
		q->type = type;
		q->race = race;
		q->kind = NULL;
		q->cur_num = 0;
		q->max_num = number;
		q->level = 0;
		q->dungeon = 0;
		q->town = 0;
		q->fixed = false;
		q->state = QUEST_TAKEN;

		return q;
	}

	return NULL;
}

/**
 * Somebody came back carrying something (ZangbandTK, WLD-19, QUEST_FIND_ITEM).
 *
 * Zangband's FIND_OBJECT trigger.  Checked as the thing enters the pack rather
 * than off the floor, so buying it, or taking it out of a chest, is fetching it
 * just as much as finding it lying about.
 *
 * \param obj is what has just been picked up.
 * \return true if anything was completed.
 */
bool quest_check_item(struct player *p, const struct object *obj)
{
	bool any = false;
	int i;

	if (!obj || !obj->kind) return false;

	for (i = z_info->quest_fixed; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		if (q->state != QUEST_TAKEN) continue;
		if (q->type != QUEST_FIND_ITEM) continue;
		if (q->kind != obj->kind) continue;

		q->cur_num += obj->number;
		if (q->cur_num >= q->max_num) {
			q->cur_num = q->max_num;
			q->state = QUEST_COMPLETE;
			any = true;
			msg("%s: done.", q->name ? q->name : "That errand");
		}
	}

	return any;
}

/**
 * Somebody arrived somewhere (ZangbandTK, WLD-19, WLD-21).
 *
 * Zangband's WILD_ENTER and FIND_SHOP triggers, which are the same event here:
 * the player is standing in a town they were sent to.  A delivery is done when
 * the word arrives, and finding a place is done by being in it -- neither of
 * them is a thing you kill, and neither can be noticed by quest_check(), which
 * only ever sees a monster die.
 *
 * \param town is the index of the town the player is standing in, or -1.
 * \return true if anything was completed.
 */
bool quest_check_arrival(struct player *p, int town)
{
	bool any = false;
	int i;

	if (town < 0) return false;

	for (i = z_info->quest_fixed; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		if (q->state != QUEST_TAKEN) continue;
		if (q->type != QUEST_DELIVERY && q->type != QUEST_FIND_PLACE) continue;
		if (q->town != town + 1) continue;

		q->cur_num = q->max_num;
		q->state = QUEST_COMPLETE;
		any = true;

		msg("%s: done.", q->name ? q->name : "That errand");
	}

	return any;
}

/**
 * A quest the character has taken and not yet handed back (WLD-16d).
 *
 * \param done selects between one that is finished and waiting to be reported
 * and one still being worked on.
 */
struct quest *quest_carried(struct player *p, bool done)
{
	int i;

	for (i = z_info->quest_fixed; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		if (q->fixed) continue;
		if (q->state == (done ? QUEST_COMPLETE : QUEST_TAKEN)) return q;
	}

	return NULL;
}

/**
 * Report a quest done, and free its slot (WLD-16d).
 */
void quest_hand_back(struct player *p, struct quest *q)
{
	(void) p;

	if (!q) return;

	string_free(q->name);
	q->name = NULL;
	q->race = NULL;
	q->cur_num = 0;
	q->max_num = 0;
	q->state = QUEST_UNTAKEN;
}

bool quest_check(struct player *p, const struct monster *m)
{
	int i, unfinished = 0;
	bool completed = false;

	/* Mark quests as complete */
	for (i = 0; i < z_info->quest_max; i++) {
		struct quest *q = &p->quests[i];

		/*
		 * Note completed quests.  A fixed quest is a place as much as a
		 * monster -- the Serpent is at the bottom of the Courts of Chaos and
		 * nowhere else -- but a bounty taken from a townsman is about the
		 * creature, and it counts wherever you find one (WLD-16d).
		 */
		/*
		 * A kill completes the kinds of quest that are about killing, and only
		 * those: a delivery is not finished by killing the man who asked for it
		 * (ZangbandTK, WLD-19).
		 */
		if (!q->fixed && q->type != QUEST_BOUNTY && q->type != QUEST_DUNGEON &&
			q->type != QUEST_WILD)
			continue;

		/* A wild bounty counts only what is killed above ground. */
		if (!q->fixed && q->type == QUEST_WILD && p->depth > 0)
			continue;

		if (m->race == q->race && q->state == QUEST_TAKEN &&
			(!(q->fixed || q->type == QUEST_DUNGEON) ||
			 (cave->depth == q->level &&
			  (!q->dungeon || q->dungeon == p->dungeon)))) {
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
