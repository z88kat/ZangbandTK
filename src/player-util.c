/**
 * \file player-util.c
 * \brief Player utility functions
 *
 * Copyright (c) 2011 The Angband Developers. See COPYING.
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
#include "cmd-core.h"
#include "game-input.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-predicate.h"
#include "obj-chest.h"
#include "mon-lore.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-mutation.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "dun-type.h"
#include "player-luck.h"
#include "player-util.h"
#include "player-virtue.h"
#include "project.h"
#include "score.h"
#include "store.h"
#include "target.h"
#include "trap.h"
#include "ui-input.h"
#include "wild.h"

/**
 * Increment to the next or decrement to the preceeding level
   accounting for the stair skip value in constants
   Keep in mind to check all intermediate level for unskippable
   quests
*/
int dungeon_get_next_level(struct player *p, int dlev, int added)
{
	int target_level, i;

	/* Get target level */
	target_level = dlev + added * z_info->stair_skip;

	/* Don't allow levels below max */
	if (target_level > z_info->max_depth - 1)
		target_level = z_info->max_depth - 1;

	/* Don't allow levels above the town */
	if (target_level < 0) target_level = 0;

	/*
	 * ZangbandTK (WLD-14): a dungeon covers a range of depths and ends at the
	 * bottom of it.  There is no way deeper from there -- to go further down
	 * you leave and find a dungeon that reaches deeper, which is what the world
	 * is for.  Going up past the top of one brings you out onto the surface.
	 */
	if (p->dungeon && target_level > 0) {
		const struct dun_type *type = dun_type_by_index(p->dungeon - 1);

		if (type) {
			if (target_level > type->max_depth)
				target_level = type->max_depth;

			/*
			 * Above the top of the dungeon means two different things
			 * depending on which way the player is travelling, and conflating
			 * them made every dungeon that does not start at depth one
			 * impossible to enter: stepping onto its mouth asked for depth 1,
			 * which is above its top, and got the surface back.
			 *
			 * Going down, it means the mouth of the dungeon -- arrive at its
			 * shallowest level.  Going up, it means out -- arrive on the
			 * surface.
			 */
			if (target_level < type->min_depth)
				target_level = (added > 0) ? type->min_depth : 0;
		}
	}

	/* Check intermediate levels for quests */
	for (i = dlev; i <= target_level; i++) {
		if (is_quest(p, i)) return i;
	}

	return target_level;
}

/**
 * Which dungeon the stairs under the player lead into (WLD-14).
 *
 * Two ways down exist on the surface.  A dungeon's own mouth leads into that
 * dungeon.  The staircase in the middle of a town leads into the shallowest
 * dungeon there is, so that a new character has somewhere to go from the first
 * turn without having to cross the world to find it.
 *
 * Answers the question without acting on it.  Going down can still be refused
 * after this -- by the force-descent confirmation, among others -- and an
 * earlier version set player->dungeon and announced the arrival here, so a
 * refused descent left the player standing on the surface with the game
 * believing they were somewhere else.  Word of recall and the depth clamp both
 * read that, so the mistake outlived the keypress.
 *
 * \return the dungeon, or NULL if there is nothing to go down into.
 */
const struct dun_type *player_dungeon_at_stairs(struct player *p)
{
	struct loc wgrid = loc(p->grid.x + p->wild_offset.x,
						   p->grid.y + p->wild_offset.y);
	int idx = wild_dungeon_at(wild, wgrid);

	if (idx >= 0) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(wild, idx);

		return dun_type_by_index(mouth->type);
	}

	/*
	 * A town staircase.  The shallowest dungeon, by the depth it starts at --
	 * not merely the first in the file, so that reordering dungeon.txt cannot
	 * quietly drop a new character into the Abyss.
	 */
	{
		const struct dun_type *best = NULL, *d;

		for (d = dun_types; d; d = d->next)
			if (!best || d->min_depth < best->min_depth)
				best = d;

		return best;
	}
}

/**
 * How deep the player has got in the dungeon they are in (WLD-14).
 *
 * Kept on the dungeon's mouth rather than on the player, because it is a fact
 * about the dungeon: each one remembers how far down it has been explored, and
 * word of recall takes the player back to their own depth in the one they were
 * last in rather than to the deepest they have ever been anywhere.
 */
void player_note_dungeon_depth(struct player *p)
{
	int i;

	if (!p->dungeon || !wild)
		return;

	for (i = 0; i < wild_dungeon_count(wild); i++) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(wild, i);

		if (mouth->type != p->dungeon - 1) continue;
		if (p->depth > mouth->max_depth) mouth->max_depth = p->depth;
		break;
	}
}

/**
 * The depth word of recall should return the player to (WLD-14).
 */
int player_dungeon_recall_depth(struct player *p)
{
	int i;

	if (!p->dungeon || !wild)
		return p->max_depth;

	for (i = 0; i < wild_dungeon_count(wild); i++) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(wild, i);

		if (mouth->type == p->dungeon - 1)
			return mouth->max_depth;
	}

	return p->max_depth;
}

/**
 * Set recall depth for a player recalling from town
 */
void player_set_recall_depth(struct player *p)
{
	/* Account for forced descent */
	if (OPT(p, birth_force_descend)) {
		/* Force descent to a lower level if allowed */
		if (p->max_depth < z_info->max_depth - 1
				&& !is_quest(p, p->max_depth)) {
			p->recall_depth = dungeon_get_next_level(p,
				p->max_depth, 1);
		}
	}

	/* Players who haven't left town before go to level 1 */
	p->recall_depth = MAX(p->recall_depth, 1);
}

/**
 * Give the player the choice of persistent level to recall to.  Note that if
 * a level greater than the player's maximum depth is chosen, we silently go
 * to the maximum depth.
 */
bool player_get_recall_depth(struct player *p)
{
	bool level_ok = false;
	int new = 0;

	/*
	 * No choice when have not entered the dungeon or descent is forced,
	 * so do not prompt.
	 */
	if (p->max_depth <= 0 || OPT(p, birth_force_descend)) {
		return true;
	}
	while (!level_ok) {
		const char *prompt =
			"Which level do you wish to return to (0 to cancel)? ";
		int i;

		/* Choose the level */
		new = get_quantity(prompt, p->max_depth);
		if (new == 0) {
			return false;
		}

		/* Is that level valid? */
		for (i = 0; i < chunk_list_max; i++) {
			if (chunk_list[i]->depth == new) {
				level_ok = true;
				break;
			}
		}
		if (!level_ok) {
			msg("You must choose a level you have previously visited.");
		}
	}
	p->recall_depth = new;
	return true;
}

/**
 * Change dungeon level - e.g. by going up stairs or with WoR.
 */
void dungeon_change_level(struct player *p, int dlev)
{
	/* New depth */
	p->depth = dlev;

	/*
	 * ZangbandTK (WLD-24): depth zero is the surface of the world.  Arriving
	 * there puts the player back where they left it -- p->wild_grid is not
	 * disturbed by going below, so coming up returns them to the staircase
	 * they went down.
	 */
	p->in_wild = (dlev == 0);

	/* If we're returning to town, update the store contents
	   according to how long we've been away */
	if (!dlev && daycount)
		store_update();

	/* Leaving, make new level */
	p->upkeep->generate_level = true;

	/* Save the game when we arrive on the new level. */
	p->upkeep->autosave = true;
}


/**
 * Returns what an incoming damage amount would be after applying a player's
 * damage reduction.
 *
 * \param p is the player of interest.
 * \param dam is the incoming damaage amount.
 * \return the damage after the player's damage reduction, if any.
 */
int player_apply_damage_reduction(struct player *p, int dam)
{
	/* Mega-Hack -- Apply "invulnerability" */
	if (p->timed[TMD_INVULN] && (dam < 9000)) return 0;

	dam -= p->state.dam_red;
	if (dam > 0 && p->state.perc_dam_red) {
		dam -= (dam * p->state.perc_dam_red) / 100 ;
	}

	return (dam < 0) ? 0 : dam;
}


/**
 * Decreases players hit points and sets death flag if necessary
 *
 * \param p is the player of interest.
 * \param dam is the amount of damage to apply.  If dam is less than
 * or equal to zero, nothing will be done.  The amount of damage should have
 * been processed with player_apply_damage_reduction(); that is not done
 * internally here so the caller can display messages that include the amount of
 * damage.
 * \param kb_str is the null-terminated string describing the cause of the
 * damage.
 *
 * This function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(struct player *p, int dam, const char *kb_str)
{
	int old_chp = p->chp;

	int warning = (p->mhp * p->opts.hitpoint_warn / 10);

	/* Paranoia */
	if (p->is_dead || dam <= 0) return;

	/* Disturb */
	disturb(p);

	/* Hurt the player */
	p->chp -= dam;

	/* Reward COMBAT_REGEN characters with mana for their lost hitpoints
	 * Unenviable task of separating what should and should not cause rage
	 * If we eliminate the most exploitable cases it should be fine.
	 * All traps and lava currently give mana, which could be exploited  */
	if (player_has(p, PF_COMBAT_REGEN)  && !streq(kb_str, "poison")
		&& !streq(kb_str, "a fatal wound") && !streq(kb_str, "starvation")) {
		/* lose X% of hitpoints get X% of spell points */
		int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 65536)
			/ (int32_t)p->mhp * dam;
		player_adjust_mana_precise(p, sp_gain);
	}

	/* Display the hitpoints */
	p->upkeep->redraw |= (PR_HP);

	/* Dead player */
	if (p->chp < 0) {
		/* From hell's heart I stab at thee */
		if (p->timed[TMD_BLOODLUST]
			&& (p->chp + p->timed[TMD_BLOODLUST] + p->lev >= 0)) {
			if (randint0(10)) {
				msg("Your lust for blood keeps you alive!");
			} else {
				msg("So great was his prowess and skill in warfare, the Elves said: ");
				msg("'The Mormegil cannot be slain, save by mischance.'");
			}
		} else {
			/*
			 * Note cause of death.  Do it here so EVENT_CHEAT_DEATH
			 * handlers or things looking for the "Die? " prompt
			 * (the borg, for instance), have access to it.
			 */
			my_strcpy(p->died_from, kb_str, sizeof(p->died_from));

			if ((p->wizard || OPT(p, cheat_live))
					&& !get_check("Die? ")) {
				event_signal(EVENT_CHEAT_DEATH);
			} else {
				/* Note death */
				msgt(MSG_DEATH, "You die.");
				event_signal(EVENT_MESSAGE_FLUSH);

				/* No longer a winner */
				p->total_winner = false;

				/* Note death */
				p->is_dead = true;

				/* Dead */
				return;
			}
		}
	}

	/* Hitpoint warning */
	if (p->chp < warning) {
		/* Bell on first notice */
		if (old_chp > warning)
			bell();

		/* Message */
		msgt(MSG_HITPOINT_WARN, "*** LOW HITPOINT WARNING! ***");
		event_signal(EVENT_MESSAGE_FLUSH);
	}
}

/**
 * Win or not, know inventory, home items and history upon death, enter score
 */
void death_knowledge(struct player *p)
{
	struct store *home = &stores[f_info[FEAT_HOME].shopnum - 1];
	struct object *obj;
	time_t death_time = (time_t)0;

	/* Retire in the town in a good state */
	if (p->total_winner) {
		p->depth = 0;
		my_strcpy(p->died_from, WINNING_HOW, sizeof(p->died_from));
		p->exp = p->max_exp;
		p->lev = p->max_lev;
		p->au += 10000000L;
	}

	player_learn_all_runes(p);
	for (obj = p->gear; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	for (obj = home->stock; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	history_unmask_unknown(p);

	/* Get time of death */
	(void)time(&death_time);
	enter_score(p, &death_time);

	/* Recalculate bonuses */
	p->upkeep->update |= (PU_BONUS);
	handle_stuff(p);
}

/**
 * Energy per move, taking extra moves into account
 */
int energy_per_move(struct player *p)
{
	int num = p->state.num_moves;
	int energy = z_info->move_energy;
	return (energy * (1 + ABS(num) - num)) / (1 + ABS(num));
}

/**
 * Modify a stat value by a "modifier", return new value
 *
 * Stats go up: 3,4,...,17,18,18/10,18/20,...,18/220
 * Or even: 18/13, 18/23, 18/33, ..., 18/220
 *
 * Stats go down: 18/220, 18/210,..., 18/10, 18, 17, ..., 3
 * Or even: 18/13, 18/03, 18, 17, ..., 3
 */
int16_t modify_stat_value(int value, int amount)
{
	int i;

	/* Reward or penalty */
	if (amount > 0) {
		/* Apply each point */
		for (i = 0; i < amount; i++) {
			/* One point at a time */
			if (value < 18) value++;

			/* Ten "points" at a time */
			else value += 10;
		}
	} else if (amount < 0) {
		/* Apply each point */
		for (i = 0; i < (0 - amount); i++) {
			/* Ten points at a time */
			if (value >= 18+10) value -= 10;

			/* Prevent weirdness */
			else if (value > 18) value = 18;

			/* One point at a time */
			else if (value > 3) value--;
		}
	}

	/* Return new value */
	return (value);
}

/**
 * Swap player's stats at random, retaining information so they can be
 * reverted to their original state.
 */
void player_scramble_stats(struct player *p)
{
	int max1, cur1, max2, cur2, i, j, swap;

	/* Fisher-Yates shuffling algorithm */
	for (i = STAT_MAX - 1; i > 0; --i) {
		j = randint0(i);

		max1 = p->stat_max[i];
		cur1 = p->stat_cur[i];
		max2 = p->stat_max[j];
		cur2 = p->stat_cur[j];

		p->stat_max[i] = max2;
		p->stat_cur[i] = cur2;
		p->stat_max[j] = max1;
		p->stat_cur[j] = cur1;

		/* Record what we did */
		swap = p->stat_map[i];
		assert(swap >= 0 && swap < STAT_MAX);
		p->stat_map[i] = p->stat_map[j];
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		p->stat_map[j] = swap;
	}

	/* Mark what else needs to be updated */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Revert all prior swaps to the player's stats.  Has no effect if the
 * stats have not been swapped.
 */
void player_fix_scramble(struct player *p)
{
	/* Figure out what stats should be */
	int new_cur[STAT_MAX];
	int new_max[STAT_MAX];
	int i;

	for (i = 0; i < STAT_MAX; ++i) {
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		new_cur[p->stat_map[i]] = p->stat_cur[i];
		new_max[p->stat_map[i]] = p->stat_max[i];
	}

	/* Apply new stats and reset stat_map */
	for (i = 0; i < STAT_MAX; ++i) {
		p->stat_cur[i] = new_cur[i];
		p->stat_max[i] = new_max[i];
		p->stat_map[i] = i;
	}

	/* Mark what else needs to be updated */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Regenerate one turn's worth of hit points
 */
void player_regen_hp(struct player *p)
{
	int32_t hp_gain;
	int percent = 0;/* max 32k -> 50% of mhp; more accurately "pertwobytes" */
	int fed_pct, old_chp = p->chp;

	/* Default regeneration */
	if (p->timed[TMD_FOOD] >= PY_FOOD_WEAK) {
		percent = PY_REGEN_NORMAL;
	} else if (p->timed[TMD_FOOD] >= PY_FOOD_FAINT) {
		percent = PY_REGEN_WEAK;
	} else if (p->timed[TMD_FOOD] >= PY_FOOD_STARVE) {
		percent = PY_REGEN_FAINT;
	}

	/* Food bonus - better fed players regenerate up to 1/3 faster */
	fed_pct = p->timed[TMD_FOOD] / z_info->food_value;
	percent *= 100 + fed_pct / 3;
	percent /= 100;

	/* Various things speed up regeneration */
	if (player_of_has(p, OF_REGEN))
		percent *= 2;
	if (player_resting_can_regenerate(p))
		percent *= 2;

	/* Some things slow it down */
	if (player_of_has(p, OF_IMPAIR_HP))
		percent /= 2;

	/* Various things interfere with physical healing */
	if (p->timed[TMD_PARALYZED]) percent = 0;
	if (p->timed[TMD_POISONED]) percent = 0;
	if (p->timed[TMD_STUN]) percent = 0;
	if (p->timed[TMD_CUT]) percent = 0;

	/* Extract the new hitpoints */
	hp_gain = (int32_t)(p->mhp * percent) + PY_REGEN_HPBASE;
	player_adjust_hp_precise(p, hp_gain);

	/* Notice changes */
	if (old_chp != p->chp) {
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_HP);
	}
}


/**
 * Regenerate one turn's worth of mana
 */
void player_regen_mana(struct player *p)
{
	int32_t sp_gain;
	int percent, old_csp = p->csp;

	/* Save the old spell points */
	old_csp = p->csp;

	/* Default regeneration */
	percent = PY_REGEN_NORMAL;

	/* Various things speed up regeneration, but shouldn't punish healthy BGs */
	if (!(player_has(p, PF_COMBAT_REGEN) && p->chp  > p->mhp / 2)) {
		if (player_of_has(p, OF_REGEN))
			percent *= 2;
		if (player_resting_can_regenerate(p))
			percent *= 2;
	}

	/* Some things slow it down */
	if (player_has(p, PF_COMBAT_REGEN)) {
		percent /= -2;
	} else if (player_of_has(p, OF_IMPAIR_MANA)) {
		percent /= 2;
	}

	/* Regenerate mana */
	sp_gain = (int32_t)(p->msp * percent);
	if (percent >= 0)
		sp_gain += PY_REGEN_MNBASE;
	sp_gain = player_adjust_mana_precise(p, sp_gain);

	/* SP degen heals BGs at double efficiency vs casting */
	if (sp_gain < 0  && player_has(p, PF_COMBAT_REGEN)) {
		convert_mana_to_hp(p, -sp_gain * 2);
	}

	/* Notice changes */
	if (old_csp != p->csp) {
		p->upkeep->redraw |= (PR_MANA);
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_MANA);
	}
}

void player_adjust_hp_precise(struct player *p, int32_t hp_gain)
{
	int16_t old_16 = p->chp;
	/* Load it all into 4 byte format */
	int32_t old_32 = ((int32_t) old_16) * 65536 + p->chp_frac, new_32;

	/* Check for overflow */
	if (hp_gain >= 0) {
		new_32 = (old_32 < INT32_MAX - hp_gain) ?
			old_32 + hp_gain : INT32_MAX;
	} else {
		new_32 = (old_32 > INT32_MIN - hp_gain) ?
			old_32 + hp_gain : INT32_MIN;
	}

	/* Break it back down */
	if (new_32 < 0) {
		/*
		 * Don't use right bitwise shift on negative values:  whether
		 * the left bits are zero or one depends on the system.
		 */
		int32_t remainder = new_32 % 65536;

		p->chp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->chp_frac = (uint16_t) (65536 + remainder);
			assert(p->chp > INT16_MIN);
			p->chp -= 1;
		} else {
			p->chp_frac = 0;
		}
	} else {
		p->chp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->chp_frac = (uint16_t)(new_32 & 0xFFFF); /* mod 65536 */
	}

	/* Fully healed */
	if (p->chp >= p->mhp) {
		p->chp = p->mhp;
		p->chp_frac = 0;
	}

	if (p->chp != old_16) {
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Accept a 4 byte signed int, divide it by 65k, and add
 * to current spell points. p->csp and csp_frac are 2 bytes each.
 */
int32_t player_adjust_mana_precise(struct player *p, int32_t sp_gain)
{
	int16_t old_16 = p->csp;
	/* Load it all into 4 byte format*/
	int32_t old_32 = ((int32_t) p->csp) * 65536 + p->csp_frac, new_32;

	if (sp_gain == 0) return 0;

	/* Check for overflow */
	if (sp_gain > 0) {
		if (old_32 < INT32_MAX - sp_gain) {
			new_32 = old_32 + sp_gain;
		} else {
			new_32 = INT32_MAX;
			sp_gain = 0;
		}
	} else if (old_32 > INT32_MIN - sp_gain) {
		new_32 = old_32 + sp_gain;
	} else {
		new_32 = INT32_MIN;
		sp_gain = 0;
	}

	/* Break it back down*/
	if (new_32 < 0) {
		/*
		 * Don't use right bitwise shift on negative values:  whether
		 * the left bits are zero or one depends on the system.
		 */
		int32_t remainder = new_32 % 65536;

		p->csp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->csp_frac = (uint16_t) (65536 + remainder);
			assert(p->csp > INT16_MIN);
			p->csp -= 1;
		} else {
			p->csp_frac = 0;
		}
	} else {
		p->csp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->csp_frac = (uint16_t)(new_32 & 0xFFFF);    /* mod 65536 */
	}

	/* Max/min SP */
	if (p->csp >= p->msp) {
		p->csp = p->msp;
		p->csp_frac = 0;
		sp_gain = 0;
	} else if (p->csp < 0) {
		p->csp = 0;
		p->csp_frac = 0;
		sp_gain = 0;
	}

	/* Notice changes */
	if (old_16 != p->csp) {
		p->upkeep->redraw |= (PR_MANA);
	}

	if (sp_gain == 0) {
		/* Recalculate */
		new_32 = ((int32_t) p->csp) * 65536 + p->csp_frac;
		sp_gain = new_32 - old_32;
	}

	return sp_gain;
}

void convert_mana_to_hp(struct player *p, int32_t sp_long) {
	int32_t hp_gain, sp_ratio;

	if (sp_long <= 0 || p->msp == 0 || p->mhp == p->chp) return;

	/* Total HP from max */
	hp_gain = ((int32_t)(p->mhp - p->chp)) * 65536;
	hp_gain -= (int32_t)p->chp_frac;

	/* Spend X% of SP get X/2% of lost HP. E.g., at 50% HP get X/4% */
	/* Gain stays low at msp<10 because MP gains are generous at msp<10 */
	/* sp_ratio is max sp to spent sp, doubled to suit target rate. */
	sp_ratio = (((int32_t)MAX(10, (int32_t)p->msp)) * 131072) / sp_long;

	/* Limit max healing to 25% of damage; ergo spending > 50% msp
	 * is inefficient */
	if (sp_ratio < 4) {sp_ratio = 4;}
	hp_gain /= sp_ratio;

	/* DAVIDTODO Flavorful comments on large gains would be fun and informative */

	player_adjust_hp_precise(p, hp_gain);
}

/**
 * Update the player's light fuel
 */
void player_update_light(struct player *p)
{
	/* Check for light being wielded */
	struct object *obj = equipped_item_by_slot_name(p, "light");

	/* Burn some fuel in the current light */
	if (obj && tval_is_light(obj)) {
		bool burn_fuel = true;

		/* Turn off the wanton burning of light during the day in the town */
		if (!p->depth && is_daytime())
			burn_fuel = false;

		/* If the light has the NO_FUEL flag, well... */
		if (of_has(obj->flags, OF_NO_FUEL))
		    burn_fuel = false;

		/* Use some fuel (except on artifacts, or during the day) */
		if (burn_fuel && obj->timeout > 0) {
			/* Decrease life-span */
			obj->timeout--;

			/* Notice interesting fuel steps */
			if ((obj->timeout < 100) || (!(obj->timeout % 100)))
				/* Redraw stuff */
				p->upkeep->redraw |= (PR_EQUIP);

			/* Special treatment when blind */
			if (p->timed[TMD_BLIND]) {
				/* Save some light for later */
				if (obj->timeout == 0) obj->timeout++;
			} else if (obj->timeout == 0) {
				/* The light is now out */
				disturb(p);
				msg("Your light has gone out!");

				/* If it's a torch, now is the time to delete it */
				if (of_has(obj->flags, OF_BURNS_OUT)) {
					bool dummy;
					struct object *burnt =
						gear_object_for_use(p, obj, 1,
						false, &dummy);
					if (burnt->known)
						object_delete(p->cave, NULL, &burnt->known);
					object_delete(cave, p->cave, &burnt);
				}
			} else if ((obj->timeout < 50) && (!(obj->timeout % 20))) {
				/* The light is getting dim */
				disturb(p);
				msg("Your light is growing faint.");
			}
		}
	}

	/* Calculate torch radius */
	p->upkeep->update |= (PU_TORCH);
}

/**
 * Find the player's best digging tool.  If forbid_stack is true, ignores
 * stacks of more than one item.
 */
struct object *player_best_digger(struct player *p, bool forbid_stack)
{
	int weapon_slot = slot_by_name(p, "weapon");
	struct object *current_weapon = slot_object(p, weapon_slot);
	struct object *obj, *best = NULL;
	/* Prefer any melee weapon over unarmed digging, i.e. best == NULL. */
	int best_score = -1;
	struct player_state local_state;

	for (obj = p->gear; obj; obj = obj->next) {
		int score, old_number;
		if (!tval_is_melee_weapon(obj)) continue;
		if (obj->number < 1 || (forbid_stack && obj->number > 1)) continue;
		/* Don't use it if it has a sticky curse. */
		if (!obj_can_takeoff(obj)) continue;

		/* Swap temporarily for the calc_bonuses() computation. */
		old_number = obj->number;
		if (obj != current_weapon) {
			obj->number = 1;
			p->body.slots[weapon_slot].obj = obj;
		}

		/*
		 * Avoid side effects from using update set to false
		 * with calc_bonuses().
		 */
		local_state.stat_ind[STAT_STR] = 0;
		local_state.stat_ind[STAT_DEX] = 0;
		calc_bonuses(p, &local_state, true, false);
		score = local_state.skills[SKILL_DIGGING];

		/* Swap back. */
		if (obj != current_weapon) {
			obj->number = old_number;
			p->body.slots[weapon_slot].obj = current_weapon;
		}

		if (score > best_score) {
			best = obj;
			best_score = score;
		}
	}

	return best;
}

/**
 * Melee a random adjacent monster
 */
bool player_attack_random_monster(struct player *p)
{
	int i, dir = randint0(8);

	/* Confused players get a free pass */
	if (p->timed[TMD_CONFUSED]) return false;

	/* Look for a monster, attack */
	for (i = 0; i < 8; i++, dir++) {
		struct loc grid = loc_sum(p->grid, ddgrid_ddd[dir % 8]);
		const struct monster *mon = square_monster(cave, grid);
		if (mon && !monster_is_camouflaged(mon)) {
			p->upkeep->energy_use = z_info->move_energy;
			msg("You angrily lash out at a nearby foe!");
			py_attack(p, grid);
			return true;
		}
	}
	return false;
}

/**
 * Have random bad stuff happen to the player from over-exertion
 *
 * This function uses the PY_EXERT_* flags
 */
void player_over_exert(struct player *p, int flag, int chance, int amount)
{
	if (chance <= 0) return;

	/* CON damage */
	if (flag & PY_EXERT_CON) {
		if (randint0(100) < chance) {
			/* Hack - only permanent with high chance (no-mana casting) */
			bool perm = (randint0(100) < chance / 2) && (chance >= 50);
			msg("You have damaged your health!");
			player_stat_dec(p, STAT_CON, perm);
		}
	}

	/* Fainting */
	if (flag & PY_EXERT_FAINT) {
		if (randint0(100) < chance) {
			msg("You faint from the effort!");

			/* Bypass free action */
			(void)player_inc_timed(p, TMD_PARALYZED,
				randint1(amount), true, true, false);
		}
	}

	/* Scrambled stats */
	if (flag & PY_EXERT_SCRAMBLE) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_SCRAMBLE,
				randint1(amount), true, true, true);
		}
	}

	/* Cut damage */
	if (flag & PY_EXERT_CUT) {
		if (randint0(100) < chance) {
			msg("Wounds appear on your body!");
			(void)player_inc_timed(p, TMD_CUT, randint1(amount),
				true, true, false);
		}
	}

	/* Confusion */
	if (flag & PY_EXERT_CONF) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_CONFUSED,
				randint1(amount), true, true, true);
		}
	}

	/* Hallucination */
	if (flag & PY_EXERT_HALLU) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_IMAGE, randint1(amount),
				true, true, true);
		}
	}

	/* Slowing */
	if (flag & PY_EXERT_SLOW) {
		if (randint0(100) < chance) {
			msg("You feel suddenly lethargic.");
			(void)player_inc_timed(p, TMD_SLOW, randint1(amount),
				true, true, false);
		}
	}

	/* HP */
	if (flag & PY_EXERT_HP) {
		if (randint0(100) < chance) {
			int dam = player_apply_damage_reduction(p,
				randint1(amount));
			char dam_text[32] = "";

			if (dam > 0 && OPT(p, show_damage)) {
				strnfmt(dam_text, sizeof(dam_text),
					" (%d)", dam);
			}
			msg("You cry out in sudden pain!%s", dam_text);
			take_hit(p, dam, "over-exertion");
		}
	}
}


/**
 * See how much damage the player will take from terrain.
 *
 * \param p is the player to check
 * \param grid is the location of the terrain
 * \param actual will, if true, cause the player to learn the appropriate
 * runes if equipment or effects mitigate the damage.
 */
int player_check_terrain_damage(struct player *p, struct loc grid, bool actual)
{
	int dam_taken = 0;

	if (square_isfiery(cave, grid)) {
		int base_dam = 100 + randint1(100);
		int res = p->state.el_info[ELEM_FIRE].res_level;

		/* Fire damage */
		dam_taken = adjust_dam(p, ELEM_FIRE, base_dam, RANDOMISE, res,
			actual);

		/* Feather fall makes one lightfooted. */
		if (player_of_has(p, OF_FEATHER)) {
			dam_taken /= 2;
			if (actual) {
				equip_learn_flag(p, OF_FEATHER);
			}
		}
	}

	/*
	 * Deep water, as in Zangband: you can wade in, but not while loaded down.
	 * Floating over it costs nothing, and a traveller carrying no more than
	 * half what they could is light enough to keep their head up.  Anyone
	 * heavier is drowning, a little at a time, for as long as they stay in.
	 */
	if (square_isdeep(cave, grid)) {
		if (player_of_has(p, OF_FEATHER)) {
			if (actual) {
				equip_learn_flag(p, OF_FEATHER);
			}
		} else if (p->upkeep->total_weight > weight_limit(&p->state) / 2) {
			dam_taken += randint1(p->depth + 1);
		}
	}

	return dam_taken;
}

/**
 * Terrain damages the player
 */
void player_take_terrain_damage(struct player *p, struct loc grid)
{
	int dam_taken = player_check_terrain_damage(p, grid, true);
	int dam_reduced;

	if (!dam_taken) {
		return;
	}

	/*
	 * Damage the player and inventory; inventory damage is based on
	 * the raw incoming damage and not the value accounting for the
	 * player's damage reduction.
	 */
	dam_reduced = player_apply_damage_reduction(p, dam_taken);
	if (square_isfiery(cave, grid)) {
		char dam_text[32] = "";

		if (dam_reduced > 0 && OPT(p, show_damage)) {
			strnfmt(dam_text, sizeof(dam_text), " (%d)",
				dam_reduced);
		}
		msg("%s%s", square_feat(cave, grid)->hurt_msg, dam_text);
		inven_damage(p, PROJ_FIRE, dam_taken);
	} else if (square_isdeep(cave, grid)) {
		char dam_text[32] = "";

		if (dam_reduced > 0 && OPT(p, show_damage)) {
			strnfmt(dam_text, sizeof(dam_text), " (%d)", dam_reduced);
		}
		msg("%s%s", square_feat(cave, grid)->hurt_msg, dam_text);
	}
	take_hit(p, dam_reduced, square_feat(cave, grid)->die_msg);
}

/**
 * Find a player shape from the name
 */
struct player_shape *lookup_player_shape(const char *name)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (streq(shape->name, name)) {
			return shape;
		}
		shape = shape->next;
	}
	msg("Could not find %s shape!", name);
	return NULL;
}

/**
 * Find a player shape index from the shape name
 */
int shape_name_to_idx(const char *name)
{
	struct player_shape *shape = lookup_player_shape(name);
	if (shape) {
		return shape->sidx;
	} else {
		return -1;
	}
}

/**
 * Find a player shape from the index
 */
struct player_shape *player_shape_by_idx(int index)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (shape->sidx == index) {
			return shape;
		}
		shape = shape->next;
	}
	msg("Could not find shape %d!", index);
	return NULL;
}

/**
 * Give shapechanged players a choice of returning to normal shape and
 * performing a command, just returning to normal shape without acting, or
 * canceling.
 *
 * \param p the player
 * \param cmd the command being performed
 * \return true if the player wants to proceed with their command
 */
bool player_get_resume_normal_shape(struct player *p, struct command *cmd)
{
	if (player_is_shapechanged(p)) {
		msg("You cannot do this while in %s form.", p->shape->name);
		char prompt[100];
		strnfmt(prompt, sizeof(prompt),
		        "Change back and %s (y/n) or (r)eturn to normal? ",
		        cmd_verb(cmd->code));
		char answer = get_char(prompt, "yrn", 3, 'n');

		// Change back to normal shape
		if (answer == 'y' || answer == 'r') {
			player_resume_normal_shape(p);
		}

		// Players may only act if they return to normal shape
		return answer == 'y';
	}

	// Normal shape players can proceed as usual
	return true;
}

/**
 * Revert to normal shape
 */
void player_resume_normal_shape(struct player *p)
{
	p->shape = lookup_player_shape("normal");
	msg("You resume your usual shape.");

	/* Kill vampire attack */
	(void) player_clear_timed(p, TMD_ATT_VAMP, true, false);

	/* Update */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->redraw |= (PR_TITLE | PR_MISC);
	handle_stuff(p);
}

/**
 * Check if the player is shapechanged
 */
bool player_is_shapechanged(const struct player *p)
{
	return streq(p->shape->name, "normal") ? false : true;
}

/**
 * Check if the player is immune from traps
 */
bool player_is_trapsafe(const struct player *p)
{
	if (p->timed[TMD_TRAPSAFE]) return true;
	if (player_of_has(p, OF_TRAP_IMMUNE)) return true;
	return false;
}

/**
 * Return true if the player can cast a spell.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
/**
 * Whether the character has a pool of spell points at all (ZangbandTK, PLR-06).
 *
 * 4.2 treats "has spellbooks" and "has mana" as the same question, because
 * until now they were.  A Mindcrafter has mana and no books, and every place
 * that asked the old question got the wrong answer about it -- the mana was
 * calculated with a zero armour allowance and then never displayed.
 */
bool player_has_mana(const struct player *p)
{
	if (p->class->magic.total_spells)
		return p->lev >= p->class->magic.spell_first;

	if (p->class->powers)
		return p->lev >= p->class->power_first;

	return false;
}

bool player_can_cast(const struct player *p, bool show_msg)
{
	if (!p->class->magic.total_spells) {
		if (show_msg) {
			msg("You cannot pray or produce magics.");
		}
		return false;
	}

	if (p->timed[TMD_BLIND] || no_light(p)) {
		if (show_msg) {
			msg("You cannot see!");
		}
		return false;
	}

	if (p->timed[TMD_CONFUSED]) {
		if (show_msg) {
			msg("You are too confused!");
		}
		return false;
	}

	/* An anti-magic shell stops casting, but not studying or browsing. */
	if (player_magic_blocked(p, show_msg)) {
		return false;
	}

	return true;
}

/**
 * Return true if the player can study a spell.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_study(const struct player *p, bool show_msg)
{
	if (!player_can_cast(p, show_msg))
		return false;

	if (!p->upkeep->new_spells) {
		if (show_msg) {
			int count;
			struct magic_realm *r = class_magic_realms(p->class, &count), *r1;
			char buf[120];

			my_strcpy(buf, r->spell_noun, sizeof(buf));
			my_strcat(buf, "s", sizeof(buf));
			r1 = r->next;
			mem_free(r);
			r = r1;
			if (count > 1) {
				while (r) {
					count--;
					if (count) {
						my_strcat(buf, ", ", sizeof(buf));
					} else {
						my_strcat(buf, " or ", sizeof(buf));
					}
					my_strcat(buf, r->spell_noun, sizeof(buf));
					my_strcat(buf, "s", sizeof(buf));
					r1 = r->next;
					mem_free(r);
					r = r1;
				}
			}
			msg("You cannot learn any new %s!", buf);
		}
		return false;
	}

	return true;
}

/**
 * Return true if the player can read scrolls or books.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_read(const struct player *p, bool show_msg)
{
	if (p->timed[TMD_BLIND]) {
		if (show_msg)
			msg("You can't see anything.");

		return false;
	}

	if (no_light(p)) {
		if (show_msg)
			msg("You have no light to read by.");

		return false;
	}

	if (p->timed[TMD_CONFUSED]) {
		if (show_msg)
			msg("You are too confused to read!");

		return false;
	}

	if (p->timed[TMD_AMNESIA]) {
		if (show_msg)
			msg("You can't remember how to read!");

		return false;
	}

	return true;
}

/**
 * Return true if the player can fire something with a launcher.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_fire(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "shooting");

	/* Require a usable launcher */
	if (!obj || !p->state.ammo_tval) {
		if (show_msg)
			msg("You have nothing to fire with.");
		return false;
	}

	return true;
}

/**
 * Return true if the player can refuel their light source.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_refuel(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "light");

	if (obj && of_has(obj->flags, OF_TAKES_FUEL)) {
		return true;
	}

	if (show_msg) {
		msg("Your light cannot be refuelled.");
	}

	return false;
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_cast_prereq(void)
{
	return player_can_cast(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_study_prereq(void)
{
	return player_can_study(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_read_prereq(void)
{
	/*
	 * Accommodate hacks elsewhere:  'r' is overloaded to mean
	 * release a commanded monster when TMD_COMMAND is active.
	 */
	return (player->timed[TMD_COMMAND]) ?
		true : player_can_read(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_fire_prereq(void)
{
	return player_can_fire(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_refuel_prereq(void)
{
	return player_can_refuel(player, true);
}

/**
 * Whether any of the game's own cheating options has been turned on.
 *
 * Each has a hidden twin of type OP_SCORE which is set when the cheat is used
 * and never unset, so this is a record of having cheated rather than of
 * cheating now.
 */
bool player_used_cheat_option(const struct player *p)
{
	int j;

	for (j = 0; j < OPT_MAX; ++j) {
		if (option_type(j) != OP_SCORE)
			continue;
		if (p->opts.opt[j])
			return true;
	}

	return false;
}

/**
 * Whether this character is disqualified from the high score list.
 *
 * There are two separate records of it -- `noscore` for wizard mode, the debug
 * commands and the borg, and the OP_SCORE options for the cheats on the options
 * screen -- and enter_score() has always checked both.  It is one question, so
 * it is asked in one place: the status indicator and the score list cannot
 * drift apart into disagreeing about whether a character was played straight.
 */
bool player_has_cheated(const struct player *p)
{
	return p->noscore != 0 || player_used_cheat_option(p);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_debug_prereq(void)
{
	if (player->noscore & NOSCORE_DEBUG) {
		return true;
	}
	if (confirm_debug()) {
		/* Mark savefile, and say so on the status line from here on */
		player->noscore |= NOSCORE_DEBUG;
		player->upkeep->redraw |= (PR_STATUS);
		return true;
	}
	return false;
}


/**
 * Return true if the player has access to a book that has unlearned spells.
 *
 * \param p is the player
 */
bool player_book_has_unlearned_spells(struct player *p)
{
	int i, j;
	int item_max = z_info->pack_size + z_info->floor_size;
	struct object **item_list = mem_zalloc(item_max * sizeof(struct object *));
	int item_num;

	/* Check if the player can learn new spells */
	if (!p->upkeep->new_spells) {
		mem_free(item_list);
		return false;
	}

	/* Check through all available books */
	item_num = scan_items(item_list, item_max, p, USE_INVEN | USE_FLOOR,
		obj_can_study);
	for (i = 0; i < item_num; i++) {
		const struct class_book *book = player_object_to_book(p, item_list[i]);
		if (!book) continue;

		/* Extract spells */
		for (j = 0; j < book->num_spells; j++)
			if (spell_okay_to_study(p, book->spells[j].sidx)) {
				/* There is a spell the player can study */
				mem_free(item_list);
				return true;
			}
	}

	mem_free(item_list);
	return false;
}

/**
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool player_confuse_dir(struct player *p, int *dp, bool too)
{
	int dir = *dp;

	if (p->timed[TMD_CONFUSED]) {
		if ((dir == 5) || (randint0(100) < 75)) {
			/* Random direction */
			dir = ddd[randint0(8)];
		}

	/* Running attempts always fail */
	if (too) {
		msg("You are too confused.");
		return true;
	}

	if (*dp != dir) {
		msg("You are confused.");
		*dp = dir;
		return true;
	}
	}

	return false;
}

/**
 * Return true if the provided count is one of the conditional REST_ flags.
 */
bool player_resting_is_special(int16_t count)
{
	switch (count) {
		case REST_COMPLETE:
		case REST_ALL_POINTS:
		case REST_SOME_POINTS:
			return true;
	}

	return false;
}

/**
 * Return true if the player is resting.
 */
bool player_is_resting(const struct player *p)
{
	return (p->upkeep->resting > 0 ||
			player_resting_is_special(p->upkeep->resting));
}

/**
 * Return the remaining number of resting turns.
 */
int16_t player_resting_count(const struct player *p)
{
	return p->upkeep->resting;
}

/**
 * In order to prevent the regeneration bonus from the first few turns, we have
 * to store the number of turns the player has rested. Otherwise, the first
 * few turns will have the bonus and the last few will not.
 */
static int player_turns_rested = 0;
static bool player_rest_disturb = false;

/**
 * Set the number of resting turns.
 *
 * \param p is the player trying to rest.
 * \param count is the number of turns to rest or one of the REST_ constants.
 */
void player_resting_set_count(struct player *p, int16_t count)
{
	/* Cancel if player is disturbed */
	if (player_rest_disturb) {
		p->upkeep->resting = 0;
		player_rest_disturb = false;
		return;
	}

	/* Ignore if the rest count is negative. */
	if ((count < 0) && !player_resting_is_special(count)) {
		p->upkeep->resting = 0;
		return;
	}

	/* Save the rest code */
	p->upkeep->resting = count;

	/* Truncate overlarge values */
	if (p->upkeep->resting > 9999) p->upkeep->resting = 9999;
}

/**
 * Cancel current rest.
 */
void player_resting_cancel(struct player *p, bool disturb)
{
	player_resting_set_count(p, 0);
	player_turns_rested = 0;
	player_rest_disturb = disturb;
}

/**
 * Return true if the player should get a regeneration bonus for the current
 * rest.
 */
bool player_resting_can_regenerate(const struct player *p)
{
	return player_turns_rested >= REST_REQUIRED_FOR_REGEN ||
		player_resting_is_special(p->upkeep->resting);
}

/**
 * Perform one turn of resting. This only handles the bookkeeping of resting
 * itself, and does not calculate any possible other effects of resting (see
 * process_world() for regeneration).
 */
void player_resting_step_turn(struct player *p)
{
	/* Timed rest */
	if (p->upkeep->resting > 0) {
		/* Reduce rest count */
		p->upkeep->resting--;

		/* Redraw the state */
		p->upkeep->redraw |= (PR_STATE);
	}

	/* Take a turn */
	p->upkeep->energy_use = z_info->move_energy;

	/* Increment the resting counters */
	p->resting_turn++;
	player_turns_rested++;
}

/**
 * Handle the conditions for conditional resting (resting with the REST_
 * constants).
 */
void player_resting_complete_special(struct player *p)
{
	/* Complete resting */
	if (!player_resting_is_special(p->upkeep->resting)) return;

	if (p->upkeep->resting == REST_ALL_POINTS) {
		if ((p->chp == p->mhp) && (p->csp == p->msp))
			/* Stop resting */
			disturb(p);
	} else if (p->upkeep->resting == REST_COMPLETE) {
		if ((p->chp == p->mhp) &&
			(p->csp == p->msp || player_has(p, PF_COMBAT_REGEN)) &&
			!p->timed[TMD_BLIND] && !p->timed[TMD_CONFUSED] &&
			!p->timed[TMD_POISONED] && !p->timed[TMD_AFRAID] &&
			!p->timed[TMD_TERROR] && !p->timed[TMD_STUN] &&
			!p->timed[TMD_CUT] && !p->timed[TMD_SLOW] &&
			!p->timed[TMD_PARALYZED] && !p->timed[TMD_IMAGE] &&
			!p->word_recall && !p->deep_descent)
			/* Stop resting */
			disturb(p);
	} else if (p->upkeep->resting == REST_SOME_POINTS) {
		if ((p->chp == p->mhp) || (p->csp == p->msp))
			/* Stop resting */
			disturb(p);
	}
}

/* Record the player's last rest count for repeating */
static int player_resting_repeat_count = 0;

/**
 * Get the number of resting turns to repeat.
 *
 * \param p The current player.
 */
int player_get_resting_repeat_count(struct player *p)
{
	return player_resting_repeat_count;
}

/**
 * Set the number of resting turns to repeat.
 *
 * \param p is the player trying to rest.
 * \param count is the number of turns requested for rest most recently.
 */
void player_set_resting_repeat_count(struct player *p, int16_t count)
{
	player_resting_repeat_count = count;
}

/**
 * Check if the player state has the given OF_ flag.
 */
bool player_of_has(const struct player *p, int flag)
{
	assert(p);
	return of_has(p->state.flags, flag);
}

/**
 * Check if the player resists (or better) an element
 */
bool player_resists(const struct player *p, int element)
{
	return (p->state.el_info[element].res_level > 0);
}

/**
 * Check if the player resists (or better) an element
 */
bool player_is_immune(const struct player *p, int element)
{
	return (p->state.el_info[element].res_level == 3);
}

/**
 * Places the player at the given coordinates in the cave.
 */
void player_place(struct chunk *c, struct player *p, struct loc grid)
{
	assert(!square_monster(c, grid));

	/* Save player location */
	p->grid = grid;

	/* Mark cave grid */
	square_set_mon(c, grid, -1);

	/* Clear stair creation */
	p->upkeep->create_down_stair = false;
	p->upkeep->create_up_stair = false;
}

/*
 * Take care of bookkeeping after moving the player with monster_swap().
 *
 * \param p is the player that was moved.
 * \param eval_trap will, if true, cause evaluation (possibly affecting the
 * player) of the traps in the grid.
 * \param is_involuntary will, if true, do appropriate actions (flush the
 * command queue) for a move not expected by the player.
 */
void player_handle_post_move(struct player *p, bool eval_trap,
		bool is_involuntary)
{
	/*
	 * ZangbandTK (WLD-23): on the overworld the player's world position is the
	 * truth and their position on the live surface is derived from it, so the
	 * surface can be rebuilt beneath them without disturbing where they are.
	 *
	 * Here rather than in move_player() because walking is not the only way to
	 * arrive somewhere.  Teleport, a trapdoor's landing, being thrown by a
	 * monster and the rest all end here, and every one of them left the world
	 * position stale when this was hooked to walking alone -- so the next
	 * rebuild snapped the player back to wherever they had last walked.
	 */
	if (p->in_wild) {
		int here = wild_town_here(wild, loc(p->grid.x + p->wild_offset.x,
											p->grid.y + p->wild_offset.y));

		wild_track_move(p, p->grid);

		/*
		 * Being somewhere finishes the errands that were about getting there
		 * (WLD-19, WLD-21).  This runs on every step taken inside the town, not
		 * only on the one that crossed the wall, and that is fine: completing a
		 * quest moves it off QUEST_TAKEN, so the second step finds nothing to
		 * do and says nothing.  Cheaper than remembering where the last step
		 * was, and it cannot miss an arrival that happened some other way --
		 * being carried there by the mages, for one.
		 */
		if (here >= 0)
			quest_check_arrival(p, here);

		/*
		 * Scroll the world when they approach the window's edge.  Flagged as a
		 * scroll as well as a rebuild: the surface has to be regenerated, but
		 * crossing an invisible line in open country is not a level change and
		 * must not cancel the player's target or hand them a free turn's worth
		 * of energy the way arriving on a new level does.
		 */
		if (wild_needs_recentre(p)) {
			p->upkeep->generate_level = true;
			p->upkeep->scroll_world = true;
		}
	}

	/*
	 * ZangbandTK (WLD-16c, WLD-18): a service building is a door with behaviour
	 * behind it, entered the way a shop is.  Signalled rather than called, since
	 * what happens next is a menu and the game side does not own menus.
	 */
	if (wild_service_at(cave, p->grid) >= 0) {
		disturb(p);
		if (is_involuntary) cmdq_flush();

		/*
		 * Cleared before the building is entered, not after.  The turn is taken
		 * by whatever the building does -- each service charges its own -- and
		 * stepping onto the door of one the player then declines costs nothing.
		 * Clearing this afterwards instead wiped the charge every service had
		 * just made, so healing, enchanting, resting and travelling all took no
		 * game time at all and nothing else in the world got a turn.
		 */
		p->upkeep->energy_use = 0;

		event_signal(EVENT_ENTER_SERVICE);
		return;
	}

	/* Handle store doors, or notice objects */
	if (square_isshop(cave, p->grid)) {
		if (player_is_shapechanged(p)) {
			if (square(cave, p->grid)->feat != FEAT_HOME) {
				msg("There is a scream and the door slams shut!");
			}
			return;
		}
		disturb(p);
		if (is_involuntary) {
			cmdq_flush();
		}
		/*
		 * ZangbandTK (WLD-16a): the shelves belong to the town the player is
		 * standing in, so ask before the display does.
		 */
		store_enter(store_at(cave, p->grid));

		event_signal(EVENT_ENTER_STORE);
		event_remove_handler_type(EVENT_ENTER_STORE);
		event_signal(EVENT_USE_STORE);
		event_remove_handler_type(EVENT_USE_STORE);
		event_signal(EVENT_LEAVE_STORE);
		event_remove_handler_type(EVENT_LEAVE_STORE);
	} else {
		if (is_involuntary) {
			cmdq_flush();
		}
		square_know_pile(cave, p->grid, NULL);
	}

	/* Discover invisible traps, set off visible ones */
	if (eval_trap && square_isplayertrap(cave, p->grid)
			&& !square_isdisabledtrap(cave, p->grid)) {
		hit_trap(p->grid, 0);
	}

	/* Update view and search */
	update_view(cave, p);
	search(p);
}

/*
 * Something has happened to disturb the player.
 *
 * All disturbance cancels repeated commands, resting, and running.
 *
 * XXX-AS: Make callers either pass in a command
 * or call cmd_cancel_repeat inside the function calling this
 */
void disturb(struct player *p)
{
	/* Cancel repeated commands */
	cmd_cancel_repeat();

	/* Cancel Resting */
	if (player_is_resting(p)) {
		player_resting_cancel(p, true);
		p->upkeep->redraw |= PR_STATE;
	}

	/* Cancel running */
	if (p->upkeep->running) {
		p->upkeep->running = 0;
		mem_free(p->upkeep->steps);
		p->upkeep->steps = NULL;

		/* Cancel queued commands */
		cmdq_flush();

		/* Check for new panel if appropriate */
		event_signal(EVENT_PLAYERMOVED);
		p->upkeep->update |= PU_TORCH;

		/* Mark the whole map to be redrawn */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	/* Flush input */
	event_signal(EVENT_INPUT_FLUSH);
}

/**
 * Search for traps or secret doors
 */
void search(struct player *p)
{
	struct loc grid;

	/* Various conditions mean no searching */
	if (p->timed[TMD_BLIND] || no_light(p) ||
		p->timed[TMD_CONFUSED] || p->timed[TMD_IMAGE])
		return;

	/* Search the nearby grids, which are always in bounds */
	for (grid.y = (p->grid.y - 1); grid.y <= (p->grid.y + 1); grid.y++) {
		for (grid.x = (p->grid.x - 1); grid.x <= (p->grid.x + 1); grid.x++) {
			struct object *obj;

			/* Secret doors */
			if (square_issecretdoor(cave, grid)) {
				msg("You have found a secret door.");
				place_closed_door(cave, grid);
				disturb(p);
			}

			/* Traps on chests */
			for (obj = square_object(cave, grid); obj; obj = obj->next) {
				if (!obj->known || ignore_item_ok(p, obj)
						|| !is_trapped_chest(obj)) {
					continue;
				}

				if (obj->known->pval != obj->pval) {
					msg("You have discovered a trap on the chest!");
					obj->known->pval = obj->pval;
					disturb(p);
				}
			}
		}
	}
}

/**
 * Test if there are any monsters the player knows about in the field of view.
 */
bool player_has_monster_in_view(const struct player *p)
{
	int n = cave_monster_max(cave), i;

	for (i = 1; i < n; ++i) {
		const struct monster *mon = cave_monster(cave, i);

		if (monster_is_obvious(mon) && monster_is_in_view(mon)) {
			return true;
		}
	}
	return false;
}

/**
 * How likely a racial power is to fail (ZangbandTK, PLR-02).
 *
 * The same shape as a spell's chance: the base rate from the data, easier the
 * further past its level the character is, harder if they are hurt or afraid,
 * and floored so nothing is ever certain.  Leaning on a stat the way Zangband
 * did -- a Draconian's breath on constitution, a Mindflayer's blast on
 * intelligence -- so the power belongs to the body it comes out of.
 */
int player_power_chance(struct player *p, const struct player_power *power)
{
	int chance, minfail;

	if (!power) return 100;

	/*
	 * Testing a power you cannot reliably make fire is not testing it.  This
	 * is a cheat and it says so: it marks the character exactly as the others
	 * on that screen do, and the status line reads Cheat from here on.
	 *
	 * It short-circuits before everything else so the menu and the roll agree
	 * -- both ask this function, so the listing shows 0% to fail rather than a
	 * number the roll then ignores.
	 */
	if (OPT(p, cheat_powers)) return 0;

	chance = power->fail;

	/* Practice tells, exactly as it does for a spell. */
	chance -= 3 * (p->lev - power->level);

	/* And so does the stat it draws on, off the same table spells use. */
	chance -= spell_stat_adjust(p, power->stat);

	/*
	 * A character short of mana is *not* penalised for it here, and that is a
	 * decision rather than an omission (DEC-56).
	 *
	 * There used to be `chance += 5 * (cost - csp)`, carried over from the way
	 * spells work.  But the condition it tested -- `csp < cost` -- is the same
	 * condition player_use_power() uses to decide the price is paid in blood,
	 * character for character.  So the penalty fell on exactly the people
	 * already paying hit points for the privilege, and on nobody else: they
	 * were charged twice for one shortfall.
	 *
	 * A spell short of mana is different and keeps its penalty
	 * (player-spell.c).  A spell is not paid for in blood at all -- it is paid
	 * for in unconsciousness, by player_over_exert(), which faints the caster
	 * and may take a point of constitution.  Blood is a price; fainting is a
	 * risk; the two do not need the same surcharge on top.
	 */

	if (p->timed[TMD_AFRAID]) chance += 20;

	/*
	 * Floor first and cap second, which is the order spell_chance() uses and
	 * matters more than it looks: a hopeless stat gives a minimum failure of
	 * 99, and clamping the other way round returns it unaltered and hands the
	 * caller a percentage above 100.
	 */
	minfail = spell_stat_minfail(p, power->stat);
	if (minfail < 5) minfail = 5;

	if (chance < minfail) chance = minfail;
	if (chance > 95) chance = 95;

	/* Stunning is applied after the floor, again as casting does it. */
	if (p->timed[TMD_STUN] > 50) chance += 25;
	else if (p->timed[TMD_STUN]) chance += 15;

	return MIN(chance, 95);
}

/**
 * Use one of the character's racial powers (ZangbandTK, PLR-02).
 *
 * \return false if it could not be attempted at all, which is different from
 * being attempted and failing -- the first costs nothing.
 */
bool player_use_power(struct player *p, struct player_power *power, int dir)
{
	struct power_effect *band;
	bool ident = false;
	bool use_hp;
	int paid;

	if (!power || !power->effects) return false;

	if (p->lev < power->level) {
		msg("You are not yet able to do that.");
		return false;
	}

	if (p->timed[TMD_CONFUSED]) {
		msg("You are too confused.");
		return false;
	}

	/*
	 * Short of mana, the price is paid in blood.  This is Zangband's rule and
	 * it is the only reason the whole feature works for everyone: the Warrior
	 * and the Mindcrafter have no spellbooks, so calc_mana() leaves the Warrior
	 * with a maximum of zero, and without this a Draconian Warrior could never
	 * once breathe.  Anyone who has spent their pool is in the same position.
	 * (The Monk had no mana either until 3.65.0, when it gained a realm.)
	 */
	use_hp = (p->csp < power->cost);

	if (use_hp && p->chp < power->cost) {
		msg("You have not the strength left.");
		return false;
	}

	/* Zangband charges a variable price, so a power is never quite budgeted. */
	paid = randint1(power->cost - power->cost / 2) + power->cost / 2;

	if (use_hp) {
		take_hit(p, paid, "concentrating too hard");

		/* Killed by the effort; there is nothing left to resolve. */
		if (p->is_dead) return true;
	} else {
		p->csp -= paid;
		p->upkeep->redraw |= (PR_MANA);
	}

	if (randint0(100) < player_power_chance(p, power)) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You try to %s, and fail.", power->name);
		return true;
	}

	/*
	 * Run every band the character has grown into.  A power the player has
	 * carried since level 2 may by now be doing four things it did not used
	 * to; the bands it has outgrown are simply skipped.
	 */
	for (band = power->effects; band; band = band->next) {
		if (p->lev < band->from) continue;
		if (band->to && p->lev > band->to) continue;

		/*
		 * The beam chance matters to BOLT_OR_BEAM, which is what a
		 * Mindcrafter's Neural Blast is: Zangband beamed it on
		 * `randint1(100) < plev * 2`, and this is that same curve handed to
		 * 4.2's own machinery rather than reimplemented beside it.
		 */
		effect_do(band->effect, source_player(), NULL, &ident, true, dir,
				  p->lev * 2, 0, NULL);
	}

	return true;
}

/**
 * Whether using this power will ask the player where to point it (PLR-02).
 *
 * Only the bands that will actually fire are consulted, because a power that
 * aims at level 9 and stops aiming at 30 must stop asking at 30 too.
 */
bool player_power_aims(struct player *p, const struct player_power *power)
{
	const struct power_effect *band;

	for (band = power ? power->effects : NULL; band; band = band->next) {
		if (p->lev < band->from) continue;
		if (band->to && p->lev > band->to) continue;

		if (effect_aim(band->effect)) return true;
	}

	return false;
}

/**
 * A Lord of Chaos takes notice of its servant (ZangbandTK, PLR-05).
 *
 * Fired on every level gained, which is the whole relationship: a Chaos-Warrior
 * does not petition its patron and cannot refuse it.  The Lord simply looks up
 * from time to time and decides how it feels.
 *
 * The roll is Zangband's, and its shape is the point.  The ladder runs worst to
 * best and the roll normally skips the bottom quarter of it, so cruelty is
 * uncommon -- except that the odds of reaching down there swing with the level
 * reached.  At thirteen it is three times as likely as usual, at every
 * thirteenth level twice, and at every fourteenth it is half.  A superstition,
 * in other words, and one the player can eventually learn.
 */
int patron_roll_slot(const struct player *p)
{
	int nasty = 6, favour;

	if (p->lev == 13) nasty = 2;
	else if (!(p->lev % 13)) nasty = 3;
	else if (!(p->lev % 14)) nasty = 12;

	/*
	 * A Lord that was never yours is half as likely to be cruel
	 * ([xtra2.c:3114](../archive/zangband/src/xtra2.c#L3114)).
	 *
	 * Read the direction carefully -- it is the opposite of what the fiction
	 * suggests.  Zangband writes `nasty_chance *= 2` for a character without
	 * the patron flag, and `nasty_chance` is the denominator of a one-in-N
	 * roll, so doubling it *halves* the cruelty.  A Lord with no claim on you
	 * is glancing over, not keeping accounts: the borrowed reward is a smaller
	 * thing in both directions.
	 */
	if (!p->patron) nasty *= 2;

	/*
	 * And what the Courts make of how you have behaved (PLR-21).
	 *
	 * The first of the two things that read a virtue, and the reason the
	 * counters exist at all. A Lord of Chaos is not impressed by a well-run
	 * life: what it recognises is Chance and Individualism, and what wearies
	 * it is Harmony and Temperance. The four are summed and the roll shifts
	 * one step for every forty points -- so a character who has lived
	 * chaotically for a long time is meaningfully safer at the Lord's hands
	 * than one who has not, and neither can move it far. A character measured
	 * against none of the four reads zero for all of them and is where they
	 * were.
	 */
	favour = virtue_value(p, V_CHANCE) + virtue_value(p, V_INDIVIDUALISM)
		- virtue_value(p, V_HARMONY) - virtue_value(p, V_TEMPERANCE);
	nasty = MAX(1, nasty + favour / 40);

	/*
	 * A generous roll cannot reach the bottom quarter of the ladder at all,
	 * which is where everything genuinely unpleasant lives; a nasty one can
	 * land anywhere.
	 */
	if (one_in_(nasty)) return randint0(PATRON_LADDER);

	return rand_range(PATRON_LADDER / 4, PATRON_LADDER - 1);
}

void patron_bestow_reward(struct player *p)
{
	const struct patron *lord = p->patron ? p->patron : patron_random();
	const struct patron_reward *reward;
	bool ident = false;
	int slot;

	if (!lord) return;

	/*
	 * One favour in six is not a favour at all (PLR-13, DEC-38).
	 *
	 * Zangband tests this before it looks at the ladder and returns straight
	 * away ([xtra2.c:3134](../archive/zangband/src/xtra2.c#L3134)), so the
	 * mutation *replaces* the reward rather than arriving beside it -- a
	 * character who was about to be healed is changed instead. Kept in that
	 * order, because a Lord that gave you a gift and a mutation would be a
	 * kinder Lord than Zangband's.
	 */
	if (one_in_(6)) {
		msg("%s rewards you with a mutation!", lord->name);
		(void) player_mutate(p);
		return;
	}

	slot = patron_roll_slot(p);

	reward = lord->ladder[slot];
	if (!reward) return;

	msg(reward->message, lord->name);

	if (reward->effect)
		effect_do(reward->effect, source_player(), NULL, &ident, true, 0,
				  0, 0, NULL);
}

/**
 * Choose which Lord of the Courts a character is sworn to (PLR-05).
 */
void patron_choose(struct player *p)
{
	p->patron = NULL;

	if (!pf_has(p->class->pflags, PF_CHAOS_PATRON)) return;

	p->patron = patron_random();
}

/**
 * One of the Lords of the Courts, chosen at random.
 *
 * Used both to swear a Chaos-Warrior at birth and to supply a Lord for someone
 * who has attracted attention without being sworn to anyone -- and in the
 * second case it is rolled afresh each time, deliberately. Zangband picked a
 * random patron on the spot for that character
 * ([xtra2.c:3117](../archive/zangband/src/xtra2.c#L3117)); the point of the
 * borrowed Lord is that it is not yours and not the same one twice.
 */
struct patron *patron_random(void)
{
	struct patron *patron;
	int count = 0, pick;

	for (patron = patrons; patron; patron = patron->next) count++;
	if (!count) return NULL;

	pick = randint0(count);
	for (patron = patrons; patron && pick; patron = patron->next) pick--;

	return patron;
}

/**
 * Whether anything is watching this character closely enough to reward them
 * for reaching a new level (PLR-05, CNT-07).
 *
 * Three ways in, and Zangband's own condition is the first two together
 * ([xtra2.c:102](../archive/zangband/src/xtra2.c#L102)): sworn to a Lord, or
 * carrying something that has drawn one's eye, or -- one level in seven --
 * simply unlucky enough to be noticed. The last of those is the fifth thing
 * `STRANGE_LUCK` does, and the only one that is not about critical hits.
 */
bool patron_owes_reward(const struct player *p)
{
	if (p->patron) return true;
	if (of_has(p->state.flags, OF_PATRON)) return true;

	return of_has(p->state.flags, OF_STRANGE_LUCK) && one_in_(7);
}

/**
 * A night's sleep at the inn, and what it shows you (PLR-41, WLD-16c).
 *
 * Zangband's inn carried a nightmare: have_nightmare() took a monster from the
 * deepest part of the bestiary, worked a power out of its hit dice, and on a
 * failed save blasted the character's sanity -- draining intelligence and wisdom,
 * inflicting amnesia, and sometimes granting a mutation.  DEC-32 dropped that
 * whole path and kept the dream, with one constraint: no insanity, no amnesia,
 * no mutation trigger.  So this is built out of what 4.2 already has.
 *
 * Three kinds of night, and which one you get depends on where you sleep:
 *
 *  - **A true dream** shows you a place you have not found, and puts it on the
 *    world map.  This is the reason to pay for a bed on a night you could have
 *    walked through.
 *  - **A dark dream** sets something hunting you through your sleep.  On a failed
 *    save you wake frightened or confused; on a made one you merely remember it.
 *  - **Dreamless sleep**, which is most nights.
 *
 * Keyed on the town's law, because the alternative -- a flat roll -- would make
 * every inn in the world the same inn, and there is a whole parameter space here
 * saying how settled a place is.  A lawful city gives visions; a town on the edge
 * of what is governed gives you a bad night.  Note that a town which has fallen
 * has no inn at all, so the truly lawless end of the range never comes up.
 *
 * The dark dream draws from monsters the player has actually met, which is a
 * change from Zangband's "deepest thing in the bestiary" and a better one: a
 * dream about something you have never encountered is a table lookup, and one
 * about the thing that nearly killed you last week is a dream.  It also scales
 * itself -- a new character dreams of what a new character has seen.
 */
void player_dream_chances(int law, int *bright, int *dark)
{
	/*
	 * Measured against the law an inn can actually stand in, which is roughly
	 * 155 to 254 -- a town below that has fallen and keeps no services at all.
	 * Over that range a true dream runs from about one night in ten to one in
	 * four, and a dark one from one in four down to almost never.  See
	 * the-inn-dreams-by-the-law in tests/game/wild.c.
	 */
	if (bright) *bright = MAX(0, (law - 100) / 6);
	if (dark) *dark = MAX(0, (255 - law) / 4);
}

void player_night_dream(struct player *p)
{
	struct wild_block *block = NULL;
	int town, law = 200, roll, true_chance, dark_chance;

	if (!wild) return;

	town = wild_town_here(wild, loc(p->grid.x + p->wild_offset.x,
									p->grid.y + p->wild_offset.y));
	if (town >= 0)
		block = wild_block_at(wild, wild->towns[town].block.x,
							  wild->towns[town].block.y);
	if (block) law = block->law;

	player_dream_chances(law, &true_chance, &dark_chance);

	/*
	 * And what the sleeper brings to it (PLR-21).
	 *
	 * The second consumer. DEC-33 reads the inn dream as a Trump-like vision,
	 * and a vision is clearer to someone who has spent their life looking:
	 * Enlightenment and Knowledge make a true dream likelier, Unlife and
	 * Chance make a dark one likelier. Applied on top of the law of the place
	 * rather than instead of it, so where you sleep still matters most and
	 * `player_dream_chances()` stays the pure function of law it was.
	 */
	true_chance = MAX(0, true_chance
					  + (virtue_value(p, V_ENLIGHTEN)
						 + virtue_value(p, V_KNOWLEDGE)) / 20);
	dark_chance = MAX(0, dark_chance
					  + (virtue_value(p, V_UNLIFE)
						 + virtue_value(p, V_CHANCE)) / 20);

	roll = randint0(100);

	if (roll < true_chance) {
		bool down = false;
		const char *name = wild_reveal_nearest(wild, p->wild_grid, &down);

		if (name) {
			if (down)
				msg("You dream of a stair going down out of the world, and wake knowing where %s lies.",
					name);
			else
				msg("You dream of walls and a gate, and wake knowing where %s stands.",
					name);
			p->upkeep->redraw |= PR_MAP;
			return;
		}

		/* Nothing left to be shown; fall through to an ordinary night. */
	}

	if (roll >= 100 - dark_chance) {
		struct monster_race *race = NULL;
		int i, draws;

		/*
		 * The deepest of three draws from what the player has met, so the dream
		 * leans towards the worst thing they know without always naming it.
		 */
		for (draws = 0; draws < 3; draws++) {
			struct monster_race *pick = NULL;
			int seen = 0;

			for (i = 1; i < z_info->r_max; i++) {
				struct monster_race *r = &r_info[i];

				if (!r->name) continue;
				if (!get_lore(r)->sights) continue;

				/* Reservoir sampling: one uniform pick in a single pass. */
				seen++;
				if (one_in_(seen)) pick = r;
			}

			if (!pick) break;
			if (!race || pick->level > race->level) race = pick;
		}

		if (race) {
			if (randint0(100) < p->state.skills[SKILL_SAVE]) {
				msg("%s chases you through your dreams.", race->name);
			} else {
				msg("You dream of %s, and wake with your heart going.",
					race->name);

				if (one_in_(2))
					player_inc_timed(p, TMD_AFRAID, 10 + randint1(10), true,
									 true, true);
				else
					player_inc_timed(p, TMD_CONFUSED, 5 + randint1(5), true,
									 true, true);
			}
			return;
		}
	}

	/* Most nights are just a night. */
}

/**
 * Forget everything the character knows (ZangbandTK, PLR-40).
 *
 * What the lotus does when its five turns are up.  Five separate kinds of
 * knowledge, because Angband keeps them in five places and there is no single
 * switch:
 *
 *  - the map of the level underfoot, grid by grid;
 *  - the world map, and every town and dungeon mouth on it, except home;
 *  - what has been learned about every monster;
 *  - what every flavoured thing is, so the potions go back to being coloured
 *    liquids and have to be drunk to find out again;
 *  - and every spell learned, which must be studied again from the book.
 *
 * All of it is recoverable by playing, and none of it is recoverable quickly.
 * That is the intended shape: the cost of eating a strange mushroom is hours,
 * not a dead character.  It takes nothing that cannot be got back -- no
 * experience, no levels, no items -- because a consumable that could end a run
 * outright is a consumable nobody ever eats twice, and this one is worth eating
 * once.
 *
 * The name is the Odyssey's, but the shape is Zelazny's: the first Amber novel
 * opens on a man with no memory who knows only that there is a place called
 * Amber and that he belongs to it.  Hence the one exception -- see
 * wild_forget_knowledge().
 */
void player_forget_the_world(struct player *p)
{
	int i;

	/* The ground you are standing on. */
	if (cave && p->cave) {
		struct loc grid;

		for (grid.y = 0; grid.y < cave->height; grid.y++)
			for (grid.x = 0; grid.x < cave->width; grid.x++) {
				if (!square_in_bounds(cave, grid)) continue;
				square_forget(cave, grid);
			}
	}

	/* The world, less the place you started from. */
	if (wild) wild_forget_knowledge(wild);

	/* Everything you had learned about what lives here. */
	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		wipe_monster_lore(race, get_lore(race));
	}

	/*
	 * And what things are.  Only the flavoured kinds: a sword is still visibly a
	 * sword to a man who cannot remember his own name, and pretending otherwise
	 * would be forgetting the language rather than the game.
	 */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		if (kind->flavor) kind->aware = false;
	}

	/* The spells, which the book will have to teach you again. */
	if (p->spell_flags) {
		int num_spells = p->class->magic.total_spells;

		for (i = 0; i < num_spells; i++) {
			p->spell_flags[i] &= ~PY_SPELL_LEARNED;
			p->spell_order[i] = 99;
		}
	}

	p->upkeep->update |= (PU_SPELLS | PU_MONSTERS | PU_UPDATE_VIEW);
	p->upkeep->redraw |= (PR_MAP | PR_STUDY | PR_OBJECT | PR_MONSTER |
						  PR_EQUIP | PR_INVEN);
}

