/**
 * \file load.c
 * \brief Individual loading functions
 *
 * Copyright (c) 1997 Ben Harrison, and others
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
#include "cave.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-init.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-randart.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "savefile.h"
#include "store.h"
#include "trap.h"
#include "dun-type.h"
#include "wild.h"
#include "ui-term.h"

/**
 * Setting this to 1 and recompiling gives a chance to recover a savefile 
 * where the object list has become corrupted.  Don't forget to reset to 0
 * and recompile again as soon as the savefile is viable again.
 */
#define OBJ_RECOVER 0

/**
 * Dungeon constants
 *
 * How many SQUARE_* info planes each terrain chunk in this savefile was written
 * with.  Recorded by the "dungeon" block and needed again by "chunks", which
 * follows it, so it has to outlive the call that reads it.  Every chunk reader
 * is handed the count explicitly rather than reading it from here: a block that
 * decodes a chunk *before* the dungeon block has set this -- as the wilderness
 * block does, since it is written first -- would otherwise skip the info planes
 * entirely and read terrain out of the middle of them.
 */
static uint8_t square_size = 0;

/**
 * Player constants
 */
static uint8_t hist_size = 0;

/**
 * Object constants
 */
static uint8_t obj_mod_max = 0;
static uint8_t of_size = 0;
static uint8_t elem_max = 0;
static uint8_t brand_max;
static uint8_t slay_max;
static uint8_t curse_max;

/**
 * Monster constants
 */
static uint8_t mflag_size = 0;

/**
 * Trap constants
 */
static uint8_t trf_size = 0;

/**
 * Shorthand function pointer for rd_item version
 */
typedef struct object *(*rd_item_t)(void);

/**
 * Read an object.
 */
static struct object *rd_item(void)
{
	struct object *obj = object_new();

	uint8_t tmp8u;
	uint16_t tmp16u;
	uint8_t effect;
	size_t i;
	char buf[128];
	uint8_t ver = 1;

	rd_u16b(&tmp16u);
	rd_byte(&ver);
	if (tmp16u != 0xffff)
		return NULL;

	rd_u16b(&obj->oidx);

	/* Location */
	rd_byte(&tmp8u);
	obj->grid.y = tmp8u;
	rd_byte(&tmp8u);
	obj->grid.x = tmp8u;

	/* Type/Subtype */
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->tval = tval_find_idx(buf);
	}
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->sval = lookup_sval(obj->tval, buf);
	}
	rd_s16b(&obj->pval);

	rd_byte(&obj->number);
	rd_s16b(&obj->weight);

	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->artifact = lookup_artifact_name(buf);
		if (!obj->artifact) {
			note(format("Couldn't find artifact %s!", buf));
			return NULL;
		}
	}
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->ego = lookup_ego_item(buf, obj->tval, obj->sval);
		if (!obj->ego) {
			note(format("Couldn't find ego item %s!", buf));
			return NULL;
		}
	}
	rd_byte(&effect);

	rd_s16b(&obj->timeout);

	rd_s16b(&obj->to_h);
	rd_s16b(&obj->to_d);
	rd_s16b(&obj->to_a);

	rd_s16b(&obj->ac);

	rd_byte(&obj->dd);
	rd_byte(&obj->ds);

	rd_byte(&obj->origin);
	rd_byte(&obj->origin_depth);
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->origin_race = lookup_monster(buf);
	}
	rd_byte(&obj->notice);

	for (i = 0; i < of_size; i++)
		rd_byte(&obj->flags[i]);

	for (i = 0; i < obj_mod_max; i++) {
		rd_s16b(&obj->modifiers[i]);
	}

	/* Read brands */
	rd_byte(&tmp8u);
	if (tmp8u) {
		obj->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
		for (i = 0; i < brand_max; i++) {
			rd_byte(&tmp8u);
			obj->brands[i] = tmp8u ? true : false;
		}
	}

	/* Read slays */
	rd_byte(&tmp8u);
	if (tmp8u) {
		obj->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
		for (i = 0; i < slay_max; i++) {
			rd_byte(&tmp8u);
			obj->slays[i] = tmp8u ? true : false;
		}
	}

	/* Read curses */
	rd_byte(&tmp8u);
	if (tmp8u) {
		obj->curses = mem_zalloc(z_info->curse_max * sizeof(struct curse_data));
		for (i = 0; i < curse_max; i++) {
			rd_byte(&tmp8u);
			obj->curses[i].power = tmp8u;
			rd_u16b(&tmp16u);
			obj->curses[i].timeout = tmp16u;
		}
	}

	for (i = 0; i < elem_max; i++) {
		rd_s16b(&obj->el_info[i].res_level);
		rd_byte(&obj->el_info[i].flags);
	}

	/* Monster holding object */
	rd_s16b(&obj->held_m_idx);

	rd_s16b(&obj->mimicking_m_idx);

	/* Activation */
	rd_u16b(&tmp16u);
	if (tmp16u)
		obj->activation = &activations[tmp16u];
	rd_u16b(&tmp16u);
	obj->time.base = tmp16u;
	rd_u16b(&tmp16u);
	obj->time.dice = tmp16u;
	rd_u16b(&tmp16u);
	obj->time.sides = tmp16u;

	/* Save the inscription */
	rd_string(buf, sizeof(buf));
	if (buf[0]) obj->note = quark_add(buf);

	/* Lookup item kind */
	obj->kind = lookup_kind(obj->tval, obj->sval);

	/* Check we have a kind */
	if ((!obj->tval && !obj->sval) || !obj->kind) {
		object_delete(NULL, NULL, &obj);
		return NULL;
	}

	/* Set effect */
	if (effect)
		obj->effect = obj->kind->effect;

	/* Success */
	return obj;
}


/**
 * Read a monster
 */
static bool rd_monster(struct chunk *c, struct monster *mon)
{
	uint8_t tmp8u;
	uint16_t tmp16u;
	char race_name[80];
	size_t j;
	bool delete = false;

	/* Read the monster race */
	rd_u16b(&tmp16u);
	mon->midx = tmp16u;
	rd_string(race_name, sizeof(race_name));
	mon->race = lookup_monster(race_name);
	if (!mon->race) {
		note(format("Monster race %s no longer exists!", race_name));
		return false;
	}
	rd_string(race_name, sizeof(race_name));
	if (streq(race_name, "none")) {
		mon->original_race = NULL;
	} else {
		mon->original_race = lookup_monster(race_name);
	}

	/* Read the other information */
	rd_byte(&tmp8u);
	mon->grid.y = tmp8u;
	rd_byte(&tmp8u);
	mon->grid.x = tmp8u;
	rd_s16b(&mon->hp);
	rd_s16b(&mon->maxhp);
	rd_byte(&mon->mspeed);
	rd_byte(&mon->energy);
	rd_byte(&tmp8u);

	for (j = 0; j < tmp8u; j++)
		rd_s16b(&mon->m_timed[j]);

	/* Read and extract the flag */
	for (j = 0; j < mflag_size; j++)
		rd_byte(&mon->mflag[j]);

	for (j = 0; j < of_size; j++)
		rd_byte(&mon->known_pstate.flags[j]);

	for (j = 0; j < elem_max; j++)
		rd_s16b(&mon->known_pstate.el_info[j].res_level);

	rd_u16b(&tmp16u);

	if (tmp16u) {
		/* Find and set the mimicked object */
		struct object *square_obj = square_object(c, mon->grid);

		/* Try and find the mimicked object; if we fail, delete the monster */
		while (square_obj) {
			if (square_obj->mimicking_m_idx == tmp16u) break;
			square_obj = square_obj->next;
		}
		if (square_obj) {
			mon->mimicked_obj = square_obj;
		} else {
			delete = true;
		}
	}

	/* Read all the held objects (order is unimportant) */
	while (true) {
		struct object *obj = rd_item();
		if (!obj)
			break;

		pile_insert(&mon->held_obj, obj);
		assert(obj->oidx);
		assert(c->objects[obj->oidx] == NULL);
		c->objects[obj->oidx] = obj;
	}

	/* Read group info */
	rd_u16b(&tmp16u);
	mon->group_info[PRIMARY_GROUP].index = tmp16u;
	rd_byte(&tmp8u);
	mon->group_info[PRIMARY_GROUP].role = tmp8u;
	rd_u16b(&tmp16u);
	mon->group_info[SUMMON_GROUP].index = tmp16u;
	rd_byte(&tmp8u);
	mon->group_info[SUMMON_GROUP].role = tmp8u;

	/* Now delete the monster if necessary */
	if (delete) {
		delete_monster(c, mon->grid);
	}

	return true;
}


/**
 * Read a trap record
 */
static void rd_trap(struct trap *trap)
{
	int i;
	uint8_t tmp8u;
	char buf[80];

	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		trap->kind = lookup_trap(buf);
		trap->t_idx = trap->kind->tidx;
	}
	rd_byte(&tmp8u);
	trap->grid.y = tmp8u;
	rd_byte(&tmp8u);
	trap->grid.x = tmp8u;
	rd_byte(&trap->power);
	rd_byte(&trap->timeout);

	for (i = 0; i < trf_size; i++)
		rd_byte(&trap->flags[i]);
}

/**
 * Read RNG state
 *
 * There were originally 64 bytes of randomizer saved. Now we only need
 * 32 + 5 bytes saved, so we'll read an extra 27 bytes at the end which won't
 * be used.
 */
int rd_randomizer(void)
{
	int i;
	uint32_t noop;

	/* current value for the simple RNG */
	rd_u32b(&Rand_value);

	/* state index */
	rd_u32b(&state_i);

	/* for safety, make sure state_i < RAND_DEG */
	state_i = state_i % RAND_DEG;
    
	/* NULL padding for compatibility with previous versions */
	rd_u32b(&noop);
	rd_u32b(&noop);
	rd_u32b(&noop);
    
	/* RNG state */
	for (i = 0; i < RAND_DEG; i++)
		rd_u32b(&STATE[i]);

	/* NULL padding */
	for (i = 0; i < 59 - RAND_DEG; i++)
		rd_u32b(&noop);

	Rand_quick = false;

	return 0;
}


/**
 * Read options.
 */
int rd_options(void)
{
	uint8_t b;

	/*** Special info */

	/* Read "delay_factor" */
	rd_byte(&b);
	player->opts.delay_factor = b;

	/* Read "hitpoint_warn" */
	rd_byte(&b);
	player->opts.hitpoint_warn = b;

	/* Read lazy movement delay */
	rd_byte(&b);
	player->opts.lazymove_delay = b;

	/* Read sidebar mode (if it's an actual game) */
	if (angband_term[0]) {
		rd_byte(&b);
		if (b >= SIDEBAR_MAX) b = SIDEBAR_LEFT;
		SIDEBAR_MODE = b;
	} else {
		strip_bytes(1);
	}


	/* Read options */
	while (1) {
		uint8_t value;
		char name[40];
		rd_string(name, sizeof name);

		if (!name[0])
			break;

		rd_byte(&value);
		option_set(name, !!value);
	}

	return 0;
}

/**
 * Read the saved messages
 */
int rd_messages(void)
{
	int i;
	char buf[128];
	uint16_t tmp16u;

	int16_t num;

	/* Total */
	rd_s16b(&num);

	/* Read the messages */
	for (i = 0; i < num; i++) {
		/* Read the message */
		rd_string(buf, sizeof(buf));

		/* Read the message type */
		rd_u16b(&tmp16u);

		/* Save the message */
		message_add(buf, tmp16u);
	}

	return 0;
}

/**
 * Read monster memory.
 */
int rd_monster_memory(void)
{
	uint16_t nkill, ntheft;
	char buf[128];
	int i;

	/* Monster temporary flags */
	rd_byte(&mflag_size);

	/* Incompatible save files */
	if (mflag_size > MFLAG_SIZE) {
	        note(format("Too many (%u) monster temporary flags!", mflag_size));
		return (-1);
	}

	/* Reset maximum numbers per level */
	for (i = 1; z_info && i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		race->max_num = 100;
		if (rf_has(race->flags, RF_UNIQUE))
			race->max_num = 1;
	}

	rd_string(buf, sizeof(buf));
	while (!streq(buf, "No more monsters")) {
		struct monster_race *race = lookup_monster(buf);

		/* Get the kill and theft counts, skip if monster invalid */
		rd_u16b(&nkill);
		rd_u16b(&ntheft);
		if (!race) continue;

		/* Store the kill count, ensure dead uniques stay dead */
		l_list[race->ridx].pkills = nkill;
		if (rf_has(race->flags, RF_UNIQUE) && nkill)
			race->max_num = 0;

		/* Store the theft count */
		l_list[race->ridx].thefts = ntheft;

		/* Look for the next monster */
		rd_string(buf, sizeof(buf));
	}

	return 0;
}


int rd_object_memory(void)
{
	size_t i;
	uint16_t tmp16u;

	/* Object Memory */
	rd_u16b(&tmp16u);
	if (tmp16u > z_info->k_max) {
		note(format("Too many (%u) object kinds!", tmp16u));
		return (-1);
	}

	/* Object flags */
	rd_byte(&of_size);
	if (of_size > OF_SIZE) {
	        note(format("Too many (%u) object flags!", of_size));
		return (-1);
	}

	/* Object modifiers */
	rd_byte(&obj_mod_max);
	if (obj_mod_max > OBJ_MOD_MAX) {
	        note(format("Too many (%u) object modifiers allowed!",
						obj_mod_max));
		return (-1);
	}

	/* Elements */
	rd_byte(&elem_max);
	if (elem_max > ELEM_MAX) {
	        note(format("Too many (%u) elements allowed!", elem_max));
		return (-1);
	}

	/* Brands */
	rd_byte(&brand_max);
	if (brand_max > z_info->brand_max) {
	        note(format("Too many (%u) brands allowed!", brand_max));
		return (-1);
	}

	/* Slays */
	rd_byte(&slay_max);
	if (slay_max > z_info->slay_max) {
	        note(format("Too many (%u) slays allowed!", slay_max));
		return (-1);
	}

	/* Curses */
	rd_byte(&curse_max);
	if (curse_max > z_info->curse_max) {
	        note(format("Too many (%u) curses allowed!", curse_max));
		return (-1);
	}

	/* Read the kind knowledge */
	for (i = 0; i < tmp16u; i++) {
		uint8_t tmp8u;
		struct object_kind *kind = &k_info[i];

		rd_byte(&tmp8u);

		kind->aware = (tmp8u & 0x01) ? true : false;
		kind->tried = (tmp8u & 0x02) ? true : false;
		kind->everseen = (tmp8u & 0x08) ? true : false;

		if (tmp8u & 0x04) kind_ignore_when_aware(kind);
		if (tmp8u & 0x10) kind_ignore_when_unaware(kind);
	}

	return 0;
}



/**
 * \param with_state is whether the savefile carries each quest's lifecycle state
 * (ZangbandTK, WLD-20).  Version 1 savefiles do not: there, a quest with its
 * level zeroed is one that was completed, and every other quest is one the
 * character is still on.  That is exactly the information version 1 could hold,
 * and reading it back this way loses nothing it ever knew.
 */
static int rd_quests_aux(bool with_state, bool with_type)
{
	int i;
	uint16_t tmp16u;

	/* Load the Quests */
	rd_u16b(&tmp16u);
	if (tmp16u > z_info->quest_max) {
		note(format("Too many (%u) quests!", tmp16u));
		return (-1);
	}

	/* Load the Quests */
	player_quests_reset(player);
	for (i = 0; i < tmp16u; i++) {
		uint16_t cur_num;
		rd_byte(&player->quests[i].level);
		rd_u16b(&cur_num);
		player->quests[i].cur_num = cur_num;

		if (with_state) {
			uint8_t state, fixed;

			rd_byte(&state);
			rd_byte(&fixed);
			player->quests[i].state = state;
			player->quests[i].fixed = (fixed != 0);
		}

		if (with_type) {
			uint8_t type, town;
			char name[80];

			rd_byte(&type);
			rd_byte(&town);
			rd_string(name, sizeof(name));

			player->quests[i].type = type;
			player->quests[i].town = town;

			/*
			 * Work taken from a building is named when it is taken, so its name
			 * lives in the savefile rather than in quest.txt.
			 */
			if (!player->quests[i].fixed) {
				string_free(player->quests[i].name);
				player->quests[i].name = name[0] ? string_make(name) : NULL;
			}
		}

		if (!with_state) {
			player->quests[i].fixed = true;
			player->quests[i].state = player->quests[i].level
				? QUEST_TAKEN : QUEST_FINISHED;
		}
	}

	return 0;
}

int rd_quests(void) { return rd_quests_aux(true, true); }
int rd_quests_2(void) { return rd_quests_aux(true, false); }
int rd_quests_1(void) { return rd_quests_aux(false, false); }


/**
 * Read the player information
 */
int rd_player(void)
{
	int i;
	uint8_t tmp8u, num;
	uint8_t stat_max = 0;
	char buf[80];
	struct player_race *r;
	struct player_shape *s;
	struct player_class *c;

	rd_string(player->full_name, sizeof(player->full_name));
	rd_string(player->died_from, 80);
	player->history = mem_zalloc(250);
	rd_string(player->history, 250);

	/* Player race */
	rd_string(buf, sizeof(buf));
	for (r = races; r; r = r->next) {
		if (streq(r->name, buf)) {
			player->race = r;
			break;
		}
	}

	/* Verify player race */
	if (!player->race) {
		note(format("Invalid player race (%s).", buf));
		return -1;
	}

	/* Player shape */
	rd_string(buf, sizeof(buf));
	for (s = shapes; s; s = s->next) {
		if (streq(s->name, buf)) {
			player->shape = s;
			break;
		}
	}

	/* If no player shape recorded, set to normal and hope for the best */
	if (!player->shape) {
		note(format("Invalid player shape (%s).", buf));
		return -1;
	}

	/* Player class */
	rd_string(buf, sizeof(buf));
	for (c = classes; c; c = c->next) {
		if (streq(c->name, buf)) {
			player->class = c;
			break;
		}
	}

	if (!player->class) {
		note(format("Invalid player class (%s).", buf));
		return -1;
	}

	/* Numeric name suffix */
	rd_byte(&player->opts.name_suffix);

	/* Special Race/Class info */
	rd_byte(&player->hitdie);
	rd_byte(&player->expfact);

	/* Age/Height/Weight */
	rd_s16b(&player->age);
	rd_s16b(&player->ht);
	rd_s16b(&player->wt);

	/* Read the stat info */
	rd_byte(&stat_max);
	if (stat_max > STAT_MAX) {
		note(format("Too many stats (%d).", stat_max));
		return -1;
	}

	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_max[i]);
	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_cur[i]);
	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_map[i]);
	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_birth[i]);

	rd_s16b(&player->ht_birth);
	rd_s16b(&player->wt_birth);
	strip_bytes(2);
	rd_s32b(&player->au_birth);

	/* Player body */
	rd_string(buf, sizeof(buf));
	player->body.name = string_make(buf);
	rd_u16b(&player->body.count);
	if (player->body.count > z_info->equip_slots_max) {
		note(format("Too many (%u) body parts!", player->body.count));
		return (-1);
	}

	player->body.slots = mem_zalloc(player->body.count *
									sizeof(struct equip_slot));
	for (i = 0; i < player->body.count; i++) {
		rd_u16b(&player->body.slots[i].type);
		rd_string(buf, sizeof(buf));
		player->body.slots[i].name = string_make(buf);
	}

	strip_bytes(4);

	rd_s32b(&player->au);

	rd_s32b(&player->max_exp);
	rd_s32b(&player->exp);
	rd_u16b(&player->exp_frac);

	rd_s16b(&player->lev);

	/* Verify player level */
	if ((player->lev < 1) || (player->lev > PY_MAX_LEVEL)) {
		note(format("Invalid player level (%d).", player->lev));
		return (-1);
	}

	rd_s16b(&player->mhp);
	rd_s16b(&player->chp);
	rd_u16b(&player->chp_frac);

	rd_s16b(&player->msp);
	rd_s16b(&player->csp);
	rd_u16b(&player->csp_frac);

	rd_s16b(&player->max_lev);
	rd_s16b(&player->max_depth);
	rd_s16b(&player->recall_depth);

	/* Repair maximum player level */
	if (player->max_lev < player->lev) player->max_lev = player->lev;

	/* Repair maximum dungeon level */
	if (player->max_depth < 0) player->max_depth = 1;
	if (player->recall_depth <= 0) player->recall_depth = player->max_depth;

	/* Reset cause of death */
	if (player->chp >= 0)
		my_strcpy(player->died_from, "(alive and well)",
				  sizeof(player->died_from));

	/* More info */
	rd_byte(&tmp8u);
	player->old_grid.y = tmp8u;
	rd_byte(&tmp8u);
	player->old_grid.x = tmp8u;
	strip_bytes(4);
	rd_byte(&player->skip_cmd_coercion);
	rd_byte(&player->unignoring);
	rd_s16b(&player->deep_descent);

	/* Read the flags */
	rd_s16b(&player->energy);
	rd_s16b(&player->word_recall);

	/* Find the number of timed effects */
	rd_byte(&num);

	if (num <= TMD_MAX) {
		/* Read all the effects */
		for (i = 0; i < num; i++)
			rd_s16b(&player->timed[i]);

		/* Initialize any entries not read */
		if (num < TMD_MAX)
			memset(player->timed + num, 0, (TMD_MAX - num) * sizeof(int16_t));
	} else {
		/* Probably in trouble anyway */
		for (i = 0; i < TMD_MAX; i++)
			rd_s16b(&player->timed[i]);

		/* Discard unused entries */
		strip_bytes(2 * (num - TMD_MAX));
		note("Discarded unsupported timed effects");
	}

	/* Total energy used so far */
	rd_u32b(&player->total_energy);
	/* # of turns spent resting */
	rd_u32b(&player->resting_turn);

	/* Future use */
	strip_bytes(32);

	return 0;
}


/**
 * Read ignore and autoinscription submenu for all known objects
 */
int rd_ignore(void)
{
	size_t i, j;
	uint8_t tmp8u = 24;
	uint16_t file_e_max;
	uint16_t itype_size;
	uint16_t inscriptions;

	/* Read how many ignore bytes we have */
	rd_byte(&tmp8u);

	/* Check against current number */
	if (tmp8u != ignore_size) {
		strip_bytes(tmp8u);
	} else {
		for (i = 0; i < ignore_size; i++)
			rd_byte(&ignore_level[i]);
	}

	/* Read the number of saved ego-item */
	rd_u16b(&file_e_max);
	rd_u16b(&itype_size);
	if (itype_size > ITYPE_SIZE) {
		note(format("Too many (%u) ignore bytes!", itype_size));
		return (-1);
	}

	for (i = 0; i < file_e_max; i++) {
		if (i < z_info->e_max) {
			bitflag flags, itypes[ITYPE_SIZE];
			
			/* Read and extract the everseen flag */
			rd_byte(&flags);
			e_info[i].everseen = (flags & 0x02) ? true : false;

			/* Read and extract the ignore flags */
			for (j = 0; j < itype_size; j++)
				rd_byte(&itypes[j]);

			/* If number of ignore types has changed, don't set anything */
			if (itype_size == ITYPE_SIZE) {
				for (j = ITYPE_NONE; j < ITYPE_MAX; j++)
					if (itype_has(itypes, j))
						ego_ignore_toggle(i, j);
			}
		}
	}

	/* Read the current number of aware object auto-inscriptions */
	rd_u16b(&inscriptions);

	/* Read the aware object autoinscriptions array */
	for (i = 0; i < inscriptions; i++) {
		char tmp[80];
		uint8_t tval, sval;
		struct object_kind *k;

		rd_string(tmp, sizeof(tmp));
		tval = tval_find_idx(tmp);
		rd_string(tmp, sizeof(tmp));
		sval = lookup_sval(tval, tmp);
		k = lookup_kind(tval, sval);
		if (!k)
			quit_fmt("lookup_kind(%d, %d) failed", tval, sval);
		rd_string(tmp, sizeof(tmp));
		k->note_aware = quark_add(tmp);
	}

	/* Read the current number of unaware object auto-inscriptions */
	rd_u16b(&inscriptions);

	/* Read the unaware object autoinscriptions array */
	for (i = 0; i < inscriptions; i++) {
		char tmp[80];
		uint8_t tval, sval;
		struct object_kind *k;

		rd_string(tmp, sizeof(tmp));
		tval = tval_find_idx(tmp);
		rd_string(tmp, sizeof(tmp));
		sval = lookup_sval(tval, tmp);
		k = lookup_kind(tval, sval);
		if (!k)
			quit_fmt("lookup_kind(%d, %d) failed", tval, sval);
		rd_string(tmp, sizeof(tmp));
		k->note_unaware = quark_add(tmp);
	}

	/* Read the current number of rune auto-inscriptions */
	rd_u16b(&inscriptions);

	/* Read the rune autoinscriptions array */
	for (i = 0; i < inscriptions; i++) {
		char tmp[80];
		int16_t runeid;

		rd_s16b(&runeid);
		rd_string(tmp, sizeof(tmp));
		rune_set_note(runeid, tmp);
	}

	return 0;
}


int rd_misc(void)
{
	size_t i;
	uint8_t tmp8u;
	
	/* Read the randart seed */
	rd_u32b(&seed_randart);

	/* Read the flavors seed */
	rd_u32b(&seed_flavor);
	flavor_init();

	/* Special stuff */
	rd_u16b(&player->total_winner);
	rd_u16b(&player->noscore);


	/* Read "death" */
	rd_byte(&tmp8u);
	player->is_dead = tmp8u;

	/* Current turn */
	rd_s32b(&turn);

	//if (player->is_dead)
	//	return 0;

	/* Handle randart file parsing */
	if (OPT(player, birth_randarts)) {
		if (randart_file_exists()) {
			cleanup_parser(&artifact_parser);
			activate_randart_file();
			if (run_parser(&randart_parser)) {
				quit("Could not parse random artifacts.");
			}
		} else {
			do_randart(seed_randart, true);
		}
		deactivate_randart_file();
	}

	/* Property knowledge */
	/* Flags */
	for (i = 0; i < OF_SIZE; i++)
		rd_byte(&player->obj_k->flags[i]);

	/* Modifiers */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		rd_s16b(&player->obj_k->modifiers[i]);
	}

	/* Elements */
	for (i = 0; i < ELEM_MAX; i++) {
		rd_s16b(&player->obj_k->el_info[i].res_level);
		rd_byte(&player->obj_k->el_info[i].flags);
	}

	/* Read brands */
	for (i = 0; i < brand_max; i++) {
		rd_byte(&tmp8u);
		player->obj_k->brands[i] = tmp8u ? true : false;
	}

	/* Read slays */
	for (i = 0; i < slay_max; i++) {
		rd_byte(&tmp8u);
		player->obj_k->slays[i] = tmp8u ? true : false;
	}

	/* Read curses */
	for (i = 0; i < curse_max; i++) {
		rd_byte(&tmp8u);
		player->obj_k->curses[i].power = tmp8u;
	}

	/* Combat data */
	rd_s16b(&player->obj_k->ac);
	rd_s16b(&player->obj_k->to_a);
	rd_s16b(&player->obj_k->to_h);
	rd_s16b(&player->obj_k->to_d);
	rd_byte(&player->obj_k->dd);
	rd_byte(&player->obj_k->ds);
	return 0;
}

int rd_artifacts(void)
{
	int i;
	uint16_t tmp16u;

	/* Load the Artifacts */
	rd_u16b(&tmp16u);
	if (tmp16u > z_info->a_max) {
		note(format("Too many (%u) artifacts!", tmp16u));
		return (-1);
	}

	/* Read the artifact flags */
	for (i = 0; i < tmp16u; i++) {
		uint8_t tmp8u;

		rd_byte(&tmp8u);
		aup_info[i].created = tmp8u ? true : false;
		rd_byte(&tmp8u);
		aup_info[i].seen = tmp8u ? true : false;
		rd_byte(&tmp8u);
		aup_info[i].everseen = tmp8u ? true : false;
		rd_byte(&tmp8u);
	}

	return 0;
}



int rd_player_hp(void)
{
	int i;
	uint16_t tmp16u;

	/* Read the player_hp array */
	rd_u16b(&tmp16u);
	if (tmp16u > PY_MAX_LEVEL) {
		note(format("Too many (%u) hitpoint entries!", tmp16u));
		return (-1);
	}

	/* Read the player_hp array */
	for (i = 0; i < tmp16u; i++)
		rd_s16b(&player->player_hp[i]);

	return 0;
}


/**
 * Read the player spells
 */
int rd_player_spells(void)
{
	int i;
	uint16_t tmp16u;
	
	int cnt;
	
	/* Read the number of spells */
	rd_u16b(&tmp16u);
	if (tmp16u > player->class->magic.total_spells) {
		note(format("Too many player spells (%d).", tmp16u));
		return (-1);
	}

	/* Initialise */
	player_spells_init(player);
	
	/* Read the spell flags */
	for (i = 0; i < tmp16u; i++)
		rd_byte(&player->spell_flags[i]);
	
	/* Read the spell order */
	for (i = 0, cnt = 0; i < tmp16u; i++, cnt++)
		rd_byte(&player->spell_order[cnt]);
	
	/* Success */
	return (0);
}




/**
 * Read the player gear
 */
static int rd_gear_aux(rd_item_t rd_item_version, struct object **gear)
{
	uint8_t code;
	struct object *last_gear_obj = NULL;

	/* Get the first item code */
	rd_byte(&code);

	/* Read until done */
	while (code != FINISHED_CODE) {
		struct object *obj = (*rd_item_version)();

		/* Read the item */
		if (!obj) {
			note("Error reading item");
			return (-1);
		}

		/* Append the object */
		obj->prev = last_gear_obj;
		if (last_gear_obj)
			last_gear_obj->next = obj;
		else
			*gear = obj;
		last_gear_obj = obj;

		/* If it's equipment, wield it */
		if (code < player->body.count) {
			player->body.slots[code].obj = obj;
			player->upkeep->equip_cnt++;
		}

		/* Get the next item code */
		rd_byte(&code);
	}

	/* Success */
	return (0);
}

/**
 * Read the player gear - wrapper functions
 */
int rd_gear(void)
{
	struct object *obj, *known_obj;

	/* Get real gear */
	if (rd_gear_aux(rd_item, &player->gear))
		return -1;

	/* Get known gear */
	if (rd_gear_aux(rd_item, &player->gear_k))
		return -1;

	/* Align the two, add weight */
	for (obj = player->gear, known_obj = player->gear_k; obj;
		 obj = obj->next, known_obj = known_obj->next) {
		obj->known = known_obj;
		player->upkeep->total_weight +=
			obj->number * object_weight_one(obj);
	}

	calc_inventory(player);

	return 0;
}


/**
 * Read store contents
 */
/**
 * \param with_quality is whether the savefile carries which town each shop's
 * stock belongs to and what tier it was stocked at (ZangbandTK, WLD-16a).
 * Version 1 savefiles do not; those shops keep their stock and are treated as
 * belonging to the starting village, so the first visit to a shop anywhere else
 * restocks it, which is what would have happened anyway.
 */
static int rd_stores_aux(rd_item_t rd_item_version, bool with_quality)
{
	int i;
	uint16_t tmp16u;

	/* Read the stores */
	rd_u16b(&tmp16u);
	if (tmp16u != z_info->store_max) {
		note(format("The number of stores in the savefile (%u) is "
			"different than expected (%u).", tmp16u,
			z_info->store_max));
	}
	for (i = 0; i < tmp16u; i++) {
		struct store *store = (i < z_info->store_max) ?
			 &stores[i] : NULL;
		uint8_t own, num;

		/* Read the basic info */
		rd_byte(&own);
		rd_byte(&num);

		/* XXX: refactor into store.c */
		if (store) {
			store->owner = store_ownerbyidx(store, own);
			store->stock_town = 0;
			store->quality = 0;
		}

		if (with_quality) {
			uint16_t town;
			uint8_t quality;

			rd_u16b(&town);
			rd_byte(&quality);

			if (store) {
				store->stock_town = town;
				store->quality = quality;
			}
		}

		/* Read the items */
		for (; num; num--) {
			/* Read the known item */
			struct object *obj, *known_obj = (*rd_item_version)();
			if (!known_obj) {
				note("Error reading known item");
				return (-1);
			}

			/* Read the item */
			obj = (*rd_item_version)();
			if (!obj) {
				note("Error reading item");
				return (-1);
			}
			obj->known = known_obj;

			/* Accept any valid items */
			if (store && store->stock_num
					< z_info->store_inven_max
					&& obj->kind) {
				if (store->feat == FEAT_HOME) {
					home_carry(obj);
				} else if (!store_carry(store, obj, false)) {
					if (obj->known) {
						object_delete(NULL, NULL,
							&obj->known);
					}
					object_delete(NULL, NULL, &obj);
				}
			} else {
				if (obj->known) {
					object_delete(NULL, NULL, &obj->known);
				}
				object_delete(NULL, NULL, &obj);
			}
		}
	}

	return 0;
}

/**
 * Read the stores - wrapper functions
 */
int rd_stores(void) { return rd_stores_aux(rd_item, true); }
int rd_stores_1(void) { return rd_stores_aux(rd_item, false); }


/**
 * Read the dungeon
 *
 * The monsters/objects must be loaded in the same order
 * that they were stored, since the actual indexes matter.
 *
 * Note that the size of the dungeon is now the currrent dimensions of the
 * cave global variable.
 *
 * Note that dungeon objects, including objects held by monsters, are
 * placed directly into the dungeon, using "object_copy()", which will
 * copy "iy", "ix", and "held_m_idx", leaving "next_o_idx" blank for
 * objects held by monsters, since it is not saved in the savefile.
 *
 * After loading the monsters, the objects being held by monsters are
 * linked directly into those monsters.
 */
static int rd_dungeon_aux(struct chunk **c, uint8_t planes)
{
	struct chunk *c1;
	int i, n, y, x;

	uint16_t height, width;

	uint8_t count;
	uint8_t tmp8u;
	uint16_t tmp16u;
	char name[100];

	/*
	 * A chunk is never written with no info planes, so being asked to read one
	 * that way means the caller does not know how the file was written and the
	 * decode below would silently take info bytes for terrain.  Refuse instead.
	 */
	if (!planes) {
		note("Savefile chunk read with no square info planes");
		return -1;
	}

	/* Header info */
	rd_string(name, sizeof(name));
	if (streq(name, "arena") && (*c == cave)) {
		player->upkeep->arena_level = true;
	}
	rd_u16b(&height);
	rd_u16b(&width);

	/* We need a cave struct */
	c1 = cave_new(height, width);
	c1->name = string_make(name);

    /* Run length decoding of cave->squares[y][x].info */
	for (n = 0; n < planes; n++) {
		/* Load the dungeon data */
		for (x = y = 0; y < c1->height; ) {
			/* Grab RLE info */
			rd_byte(&count);
			rd_byte(&tmp8u);

			/* Apply the RLE info */
			for (i = count; i > 0; i--) {
				/* Extract "info" */
				c1->squares[y][x].info[n] = tmp8u;

				/* Advance/Wrap */
				if (++x >= c1->width) {
					/* Wrap */
					x = 0;

					/* Advance/Wrap */
					if (++y >= c1->height) break;
				}
			}
		}
	}

	/* Run length decoding of dungeon data */
	for (x = y = 0; y < c1->height; ) {
		/* Grab RLE info */
		rd_byte(&count);
		rd_byte(&tmp8u);

		/* Apply the RLE info */
		for (i = count; i > 0; i--) {
			/* Extract "feat" */
			square_set_feat(c1, loc(x, y), tmp8u);

			/* Advance/Wrap */
			if (++x >= c1->width) {
				/* Wrap */
				x = 0;

				/* Advance/Wrap */
				if (++y >= c1->height) break;
			}
		}
	}


	/* Read "feeling" */
	rd_byte(&tmp8u);
	c1->feeling = tmp8u;
	rd_u16b(&tmp16u);
	c1->feeling_squares = tmp16u;
	rd_s32b(&c1->turn);

	/* Read connector info */
	if (OPT(player, birth_levels_persist)) {
		rd_byte(&tmp8u);
		while (tmp8u != 0xff) {
			struct connector *current = mem_zalloc(sizeof *current);
			current->info = mem_zalloc(planes * sizeof(bitflag));
			current->grid.x = tmp8u;
			rd_byte(&tmp8u);
			current->grid.y = tmp8u;
			rd_byte(&current->feat);
			for (n = 0; n < planes; n++) {
				rd_byte(&current->info[n]);
			}
			current->next = c1->join;
			c1->join = current;
			rd_byte(&tmp8u);
		}
	}

	/* Assign */
	*c = c1;

	return 0;
}

/**
 * Read the floor object list
 */
static int rd_objects_aux(rd_item_t rd_item_version, struct chunk *c)
{
	int i;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Make the object list */
	rd_u16b(&c->obj_max);
	c->objects = mem_realloc(c->objects,
							 (c->obj_max + 1) * sizeof(struct object*));
	for (i = 0; i <= c->obj_max; i++)
		c->objects[i] = NULL;

	/* Read the dungeon items until one isn't returned */
	while (true) {
		struct object *obj = (*rd_item_version)();
		if (!obj)
			break;
#if OBJ_RECOVER
		if (square_in_bounds_fully(c, obj->grid) && c == cave) {
#else
		if (square_in_bounds_fully(c, obj->grid)) {
#endif
			pile_insert_end(&c->squares[obj->grid.y][obj->grid.x].obj, obj);
		}
		assert(obj->oidx);
		assert(c->objects[obj->oidx] == NULL);
		c->objects[obj->oidx] = obj;
	}

	return 0;
}

/**
 * Read monsters
 */
static int rd_monsters_aux(struct chunk *c)
{
	int i;
	uint16_t limit;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Read the monster count */
	rd_u16b(&limit);
	/*
	 * Validate against this chunk's own capacity, not the global maximum.
	 * ZangbandTK sizes the monster array per chunk (WLD-26), so the global is
	 * no longer a safe bound on what will fit here — a savefile claiming more
	 * monsters than the chunk can hold would overflow place_monster().
	 */
	if (limit > c->mon_size) {
		note(format("Too many (%d) monster entries!", limit));
		return (-1);
	}

	/* Read the monsters */
	for (i = 1; i < limit; i++) {
		struct monster *mon;
		struct monster monster_body;

		/* Get local monster */
		mon = &monster_body;
		memset(mon, 0, sizeof(*mon));

		/* Read the monster */
		if (!rd_monster(c, mon)) {
			note(format("Cannot read monster %d", i));
			return (-1);
		}

		/* Place monster in dungeon */
		if (place_monster(c, mon->grid, mon, 0) != i) {
			note(format("Cannot place monster %d", i));
			return (-1);
		}
	}

	return 0;
}

static int rd_traps_aux(struct chunk *c)
{
	struct loc grid;
	struct trap *trap;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	rd_byte(&trf_size);

	/* Read traps until one has no location */
	while (true) {
		trap = mem_zalloc(sizeof(*trap));
		rd_trap(trap);
		grid = trap->grid;
		if (loc_is_zero(grid))
			break;
		else {
			/* Put the trap at the front of the grid trap list */
			trap->next = square_trap(c, grid);
			square_set_trap(c, grid, trap);

			/* Set decoy if appropriate */
			if ((trap->kind == lookup_trap("decoy")) &&
			    (c == cave)) {
				c->decoy = grid;
			}
		}
	}

	mem_free(trap);
	return 0;
}

/**
 * Read the base of the wilderness block: where the player stands in the world.
 *
 * Shared by both versions of the block, because it has never changed.
 */
static void rd_wilderness_base(void)
{
	uint8_t in_wild;
	uint16_t x, y, ox, oy;

	rd_byte(&in_wild);
	rd_u16b(&x);
	rd_u16b(&y);
	rd_u16b(&ox);
	rd_u16b(&oy);

	player->in_wild = (in_wild != 0);
	player->wild_grid = loc(x, y);
	player->wild_offset = loc(ox, oy);

	/*
	 * The world is rebuilt here rather than left to the next level change: the
	 * surface itself comes back from the "dungeon" block and the player can
	 * walk on it straight away, which needs the world map in place to know when
	 * the window should scroll.
	 */
	wild_ensure(seed_flavor);

	/*
	 * And tell the wilderness which window the player is standing in.  The
	 * surface comes back from the "dungeon" block rather than being generated,
	 * so wild_surface() is never called on load and would otherwise not learn
	 * the offset until the first rebuild -- by which time it has already
	 * re-anchored both axes and told the display the window had not moved.
	 */
	if (player->in_wild)
		wild_adopt_window(player->wild_offset);

	/*
	 * The starting village always counts as visited (WLD-16c).  Every character
	 * begins on its staircase, so this is a fact about the game rather than
	 * something to be recorded and possibly missed -- and it was being missed:
	 * the flag was only ever set by taking a step, so a character whose steps
	 * were taken before the flag existed was told by the magetower that they
	 * had been nowhere, their own village included.
	 *
	 * Set here rather than only where the surface is generated, because loading
	 * a character standing on the surface does not generate one: the level comes
	 * back from the savefile.
	 */
	if (wild_town_count(wild) > 0)
		wild->towns[0].visited = 1;
}

/**
 * Version 1 of the wilderness block.
 *
 * Version 1 shipped in three different layouts -- the base alone, then with
 * relics appended, then with uniques -- all of them called version 1, which was
 * a mistake: a savefile's shape cannot be told from its version, and reading
 * past the end of a block calls quit() and takes the application down rather
 * than declining the file.  Every character made before today did exactly that.
 *
 * Reading the base and stopping is safe for all three, because the base comes
 * first in each and load_block() does not mind a loader leaving bytes unread.
 * What is given up is a version-1 character's dropped items and wounded
 * uniques, which is a far better trade than the savefile not opening.
 *
 * Length alone cannot separate them, incidentally: blocks are padded to a
 * multiple of four bytes, and a section holding a count of zero is two bytes,
 * so "are there bytes left" cannot distinguish a missing section from padding.
 */
int rd_wilderness_1(void)
{
	rd_wilderness_base();

	note("Reading an older wilderness record; forgetting what was left lying about.");

	return 0;
}

/**
 * Read back where the player stands in the world, and what they have left in it
 * (ZangbandTK, WLD-23, WLD-04, WLD-04b, WLD-25).
 *
 * The world map itself is not stored -- it regenerates exactly from the seed the
 * savefile already carries, which is the point of WLD-03.  What cannot be
 * recomputed is where the player went and what they did, so that is what is
 * here.
 *
 * Anything added to this block in future goes on the *end*, and bumps the block
 * version.  Inserting a field in the middle is what broke version 1.
 */
/**
 * The body shared by versions 2 and 3 of the block.
 *
 * Version 3 appends what the player knows of the surface, so everything before
 * it is read the same way and the older savefile simply stops sooner.
 */
static int rd_wilderness_body(void)
{
	uint16_t count;
	int i;

	rd_wilderness_base();

	/* What the player left lying about (WLD-04). */
	rd_u16b(&count);
	for (i = 0; i < count; i++) {
		struct wild_relic *relic;
		struct object *obj;
		uint16_t rx, ry;
		int32_t left;

		rd_u16b(&rx);
		rd_u16b(&ry);
		rd_s32b(&left);

		obj = rd_item();
		if (!obj) {
			note("Error reading a wilderness object");
			return -1;
		}

		/* It belongs to no chunk, and the player knows of it only by memory. */
		obj->oidx = 0;
		obj->known = NULL;

		relic = mem_zalloc(sizeof *relic);
		relic->grid = loc(rx, ry);
		relic->turn = left;
		relic->obj = obj;
		relic->next = wild->relics;
		wild->relics = relic;
	}

	/* The uniques met and not finished (WLD-04b). */
	rd_u16b(&count);
	for (i = 0; i < count; i++) {
		struct wild_unique *seen;
		struct monster_race *race;
		char name[80];
		uint16_t rx, ry;
		int16_t hp;
		int32_t left;

		rd_string(name, sizeof(name));
		rd_u16b(&rx);
		rd_u16b(&ry);
		rd_s16b(&hp);
		rd_s32b(&left);

		/*
		 * A monster that no longer exists in the game data is dropped rather
		 * than refused: the world has simply lost track of it, which is a
		 * great deal better than declining to load the savefile.
		 */
		race = lookup_monster(name);
		if (!race) {
			note(format("Forgetting an unknown wilderness monster (%s)", name));
			continue;
		}

		seen = mem_zalloc(sizeof *seen);
		seen->race = race;
		seen->grid = loc(rx, ry);
		seen->hp = hp;
		seen->turn = left;
		seen->next = wild->uniques;
		wild->uniques = seen;
	}

	/* And which blocks of the world the player has seen (WLD-25). */
	{
		uint16_t blocks;

		rd_u16b(&blocks);
		if (blocks) {
			int total = (int) blocks * (int) blocks;

			for (i = 0; i < total; i += 8) {
				uint8_t byte, b;

				rd_byte(&byte);
				for (b = 0; b < 8 && i + b < total; b++) {
					/*
					 * Dropped rather than refused if the world has been
					 * resized in constants.txt since the save: the map is a
					 * convenience, and losing it is a far better outcome than
					 * declining to load the character.
					 */
					if ((byte & (1 << b)) && blocks == wild->blocks)
						wild->map[i + b].info |= WILD_INFO_SEEN;
				}
			}
			if (blocks != wild->blocks)
				note("The world has changed size; forgetting the map.");
		}
	}

	return 0;
}

int rd_wilderness_2(void)
{
	return rd_wilderness_body();
}

/**
 * Read what the player knows of the surface, held while they are off it.
 *
 * Appended by version 3 of the block, and last in every version since, so that
 * anything added later goes in front of it rather than after.
 */
static int rd_wilderness_knowledge(uint8_t planes)
{
	uint8_t held;

	rd_byte(&held);

	if (held) {
		struct chunk *known = NULL;
		uint16_t ox, oy;

		rd_u16b(&ox);
		rd_u16b(&oy);

		if (rd_dungeon_aux(&known, planes)) {
			note("Error reading the remembered surface");
			return -1;
		}

		wild_keep_knowledge(known, loc(ox, oy));
	}

	return 0;
}

/**
 * Read how far the player has got down each dungeon (WLD-14).
 *
 * Where the dungeons open is not stored: it follows from the world seed and
 * dungeon.txt by the same scoring every time.  What is stored is the progress,
 * by name, so that adding a dungeon does not move a character into another one.
 */
static void rd_wilderness_dungeons(void)
{
	char name[80];
	uint16_t count;
	int i;

	rd_string(name, sizeof(name));
	if (name[0]) {
		struct dun_type *type = dun_type_by_name(name);

		/*
		 * A dungeon that is no longer in the game data leaves the character on
		 * the surface rather than inside something that does not exist.
		 */
		if (type) {
			player->dungeon = type->index + 1;
		} else {
			note(format("Forgetting an unknown dungeon (%s)", name));
			player->dungeon = 0;
		}
	}

	rd_u16b(&count);
	for (i = 0; i < count; i++) {
		struct dun_type *type;
		uint8_t depth;
		int j;

		rd_string(name, sizeof(name));
		rd_byte(&depth);

		type = dun_type_by_name(name);
		if (!type) continue;

		for (j = 0; j < wild_dungeon_count(wild); j++) {
			struct wild_dungeon *mouth = wild_dungeon_by_index(wild, j);

			if (mouth->type == type->index) {
				mouth->max_depth = depth;
				break;
			}
		}
	}
}

/**
 * Which towns the player has stood in (WLD-16c).
 *
 * By name, so that adding a name to town.txt cannot hand a character somewhere
 * they have never been.  A name the world no longer has is dropped: the map is a
 * convenience and losing part of it beats refusing to load the character.
 */
static void rd_wilderness_visits(void)
{
	uint16_t count;
	int i;

	rd_u16b(&count);

	for (i = 0; i < count; i++) {
		char name[80];
		int j;

		rd_string(name, sizeof(name));

		for (j = 0; j < wild_town_count(wild); j++)
			if (wild->towns[j].name && streq(wild->towns[j].name, name)) {
				wild->towns[j].visited = 1;
				break;
			}
	}
}

int rd_wilderness_3(void)
{
	if (rd_wilderness_body())
		return -1;

	return rd_wilderness_knowledge(SQUARE_SIZE);
}

/**
 * Work out which towns an older character must have been to (WLD-16c).
 *
 * Savefiles before version 5 carry no visited flags, so a character loaded from
 * one had been nowhere as far as the magetower was concerned -- including their
 * own village, which they had certainly been to.  Reported from play: standing
 * in a tower with two towns behind them and neither offered.
 *
 * Recovered from what the savefile does hold.  The starting village is where
 * every character begins, so that one is certain.  Beyond it, a town whose
 * ground the player has seen is one they have plausibly been to, which is a
 * more generous reading than the rule for new travel -- but the alternative is
 * telling somebody who has crossed half a world that they have been nowhere.
 */
static void rd_wilderness_visits_before_5(void)
{
	int i;

	if (!wild) return;

	if (wild_town_count(wild) > 0)
		wild->towns[0].visited = 1;

	for (i = 1; i < wild_town_count(wild); i++)
		if (wild_seen(wild, wild->towns[i].block.x, wild->towns[i].block.y))
			wild->towns[i].visited = 1;
}

int rd_wilderness_4(void)
{
	if (rd_wilderness_body())
		return -1;

	rd_wilderness_dungeons();

	if (rd_wilderness_knowledge(SQUARE_SIZE))
		return -1;

	rd_wilderness_visits_before_5();

	return 0;
}

/**
 * Version 5 of the block, which did not record how the held surface was written.
 *
 * It was always written with the compile-time SQUARE_SIZE planes -- the count
 * simply went unrecorded, because the reader took it from the "dungeon" block,
 * which is written *after* this one and so had not been read yet.  Passing
 * SQUARE_SIZE here reads those files correctly; version 6 states the count so
 * that changing SQUARE_SIZE does not quietly invalidate saves.
 */
int rd_wilderness_5(void)
{
	if (rd_wilderness_body())
		return -1;

	rd_wilderness_dungeons();
	rd_wilderness_visits();

	return rd_wilderness_knowledge(SQUARE_SIZE);
}

int rd_wilderness(void)
{
	uint8_t planes;

	if (rd_wilderness_body())
		return -1;

	rd_wilderness_dungeons();
	rd_wilderness_visits();

	rd_byte(&planes);

	return rd_wilderness_knowledge(planes);
}

int rd_dungeon(void)
{
	uint16_t depth;
	uint16_t py, px;

	/* Header info */
	rd_u16b(&depth);
	rd_u16b(&daycount);
	rd_u16b(&py);
	rd_u16b(&px);
	rd_byte(&square_size);

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Ignore illegal dungeons */
	if (depth >= z_info->max_depth) {
		note(format("Ignoring illegal dungeon depth (%d)", depth));
		return (0);
	}

	if (rd_dungeon_aux(&cave, square_size))
		return 1;

	/* Ignore illegal dungeons */
	if ((px >= cave->width) || (py >= cave->height)) {
		note(format("Ignoring illegal player location (%d,%d).", py, px));
		return (1);
	}

	/* Load player depth */
	player->depth = depth;
	cave->depth = depth;

	/* Place player in dungeon */
	player_place(cave, player, loc(px, py));

	/* The dungeon is ready */
	character_dungeon = true;

	/* Read known cave */
	if (rd_dungeon_aux(&player->cave, square_size)) {
		return 1;
	}
	player->cave->depth = depth;

	return 0;
}


/**
 * Read the objects - wrapper functions
 */
int rd_objects(void)
{
	if (rd_objects_aux(rd_item, cave))
		return -1;
	if (rd_objects_aux(rd_item, player->cave))
		return -1;

	return 0;
}

/**
 * Read the monster list - wrapper functions
 */
int rd_monsters(void)
{
	int i;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	if (rd_monsters_aux(cave))
		return -1;
	if (rd_monsters_aux(player->cave))
		return -1;

#if OBJ_RECOVER
	player->cave->objects = mem_zalloc((cave->obj_max + 1) * sizeof(struct object*));
	player->cave->obj_max = cave->obj_max;
	for (i = 0; i <= cave->obj_max; i++) {
		struct object *obj = cave->objects[i], *known_obj;
		if (!obj) continue;
		known_obj = object_new();
		obj->known = known_obj;
		object_copy(known_obj, obj);
		player->cave->objects[i] = known_obj;
	}
#else
	/* Associate known objects */
	for (i = 0; i < player->cave->obj_max; i++)
		if (cave->objects[i] && player->cave->objects[i])
			cave->objects[i]->known = player->cave->objects[i];
#endif
	return 0;
}

/**
 * Read the traps - wrapper functions
 */
int rd_traps(void)
{
	if (rd_traps_aux(cave))
		return -1;
	if (rd_traps_aux(player->cave))
		return -1;
	return 0;
}

/**
 * Read the chunk list
 */
int rd_chunks(void)
{
	int j;
	uint16_t chunk_max;

	if (player->is_dead)
		return 0;

	rd_u16b(&chunk_max);
	for (j = 0; j < chunk_max; j++) {
		struct chunk *c = NULL;

		/* Read the dungeon */
		if (rd_dungeon_aux(&c, square_size))
			return -1;

		/* Read the objects */
		if (rd_objects_aux(rd_item, c))
			return -1;

		/* Read the monsters */
		if (rd_monsters_aux(c))
			return -1;

		/* Read traps */
		if (rd_traps_aux(c))
			return -1;


		/* Read other chunk info */
		if (OPT(player, birth_levels_persist)) {
			char buf[80];
			int i;
			uint8_t tmp8u;
			uint16_t tmp16u;

			rd_string(buf, sizeof(buf));
			string_free(c->name);
			c->name = string_make(buf);
			rd_s32b(&c->turn);
			rd_u16b(&tmp16u);
			c->depth = tmp16u;
			rd_byte(&c->feeling);
			rd_u32b(&c->obj_rating);
			rd_u32b(&c->mon_rating);
			rd_byte(&tmp8u);
			c->good_item  = tmp8u ? true : false;
			rd_u16b(&tmp16u);
			c->height = tmp16u;
			rd_u16b(&tmp16u);
			c->width = tmp16u;
			rd_u16b(&c->feeling_squares);
			for (i = 0; i < FEAT_MAX + 1; i++) {
				rd_u16b(&tmp16u);
				c->feat_count[i] = tmp16u;
			}
		} else if (c->name) {
			struct level *lev = level_by_name(c->name);

			if (lev) {
				c->depth = lev->depth;
			} else if (suffix(c->name, " known")) {
				size_t offset = strlen(c->name) -
					strlen(" known");
				c->name[offset] = '\0';
				lev = level_by_name(c->name);
				if (lev) {
					c->depth = lev->depth;
				}
				c->name[offset] = ' ';
			}
		}

		chunk_list_add(c);
	}

#if OBJ_RECOVER
	for (j = 0; j < chunk_max; j++) {
		if (j == 0 && streq(chunk_list[j].name, "Town")) continue;
		chunk_list[j] = 0;
	}
	if (streq(chunk_list[0].name, "Town")) {
		chunk_list_max = 1;
	} else {
		chunk_list_max = 0;
	}
#endif

	return 0;
}


int rd_history(void)
{
	uint32_t tmp32u;
	size_t i, j;
	
	history_clear(player);

	/* History type flags */
	rd_byte(&hist_size);
	if (hist_size > HIST_SIZE) {
	        note(format("Too many (%u) history types!", hist_size));
		return (-1);
	}

	rd_u32b(&tmp32u);
	for (i = 0; i < tmp32u; i++) {
		int32_t turnno;
		int16_t dlev, clev;
		bitflag type[HIST_SIZE];
		const struct artifact *art = NULL;
		int aidx = 0;
		char name[80];
		char text[80];

		for (j = 0; j < hist_size; j++)		
			rd_byte(&type[j]);
		rd_s32b(&turnno);
		rd_s16b(&dlev);
		rd_s16b(&clev);
		rd_string(name, sizeof(name));
		if (name[0]) {
			art = lookup_artifact_name(name);
			if (art) {
				aidx = art->aidx;
			}
		}
		rd_string(text, sizeof(text));
		if (name[0] && !art) {
			note(format("Couldn't find artifact %s!", name));
			continue;
		}

		history_add_full(player, type, aidx, dlev, clev, turnno, text);
	}

	return 0;
}

/**
 * For blocks that don't need loading anymore.
 */
int rd_null(void) {
	return 0;
}
