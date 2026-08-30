/**
 * \file player-mutation.c
 * \brief Chaos mutations (PLR-13 to PLR-17, PLR-34 to PLR-38)
 *
 * Zangband's mutations are the one system it built with no data file at all:
 * 96 of them, as a C array in `tables.c`, with a selection weighting that
 * exists only as the widths of the case runs in a switch statement. Both are
 * read by `zconv mutations` and land in `mutation.txt`, which is what this
 * works from.
 *
 * Two things reading the source rather than the documentation turned up.
 * Three mutations have prerequisites the spoiler never mentions -- the Midas
 * touch wants a thousand gold per level in hand, and a silly voice and
 * elemental vulnerability want three mutations already -- and the
 * regeneration penalty the spoiler warns about was gone from the source by
 * 2.7.5 (DEC-45).
 *
 * Copyright (c) 2026 ZangbandTK contributors
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

#include "player-mutation.h"

#include "cave.h"
#include "effects.h"
#include "init.h"
#include "message.h"
#include "mon-make.h"
#include "mon-util.h"
#include "player-attack.h"
#include "project.h"
#include "player-calcs.h"
#include "player-util.h"
#include "player-virtue.h"

/**
 * Mutations that drive each other out (PLR-37).
 *
 * Pairs, and it has to be pairs. PLR-37 lists seven groups and the third is
 * not a group of mutually exclusive things -- the spoiler is precise about it:
 * "Iron skin and either scales, rotting flesh or warts (getting iron skin
 * removes all the other three, getting one of the others removes iron skin)".
 *
 * Iron skin sits at the centre of a star, not in a clique. Scales and warts
 * are perfectly happy together; it is only iron skin that is incompatible with
 * each of them. Written as a set of four, gaining rotting flesh would strip
 * the scales off a character who should have kept them, and nothing about the
 * result would look wrong.
 *
 * So: nine pairs for the seven groups, applied in whichever direction the
 * character gains them.
 */
static const char *const mutation_cancels[][2] = {
	{ "HYPER_INT", "MORONIC" },
	{ "PUNY", "HYPER_STR" },
	{ "IRON_SKIN", "SCALES" },
	{ "IRON_SKIN", "FLESH_ROT" },
	{ "IRON_SKIN", "WART_SKIN" },
	{ "FLESH_ROT", "REGEN" },
	{ "COWARDICE", "FEARLESS" },
	{ "LIMBER", "ARTHRITIS" },
	{ "BEAK", "TRUNK" },
};

const struct mutation *mutation_by_name(const char *name)
{
	const struct mutation *m;

	if (!name) return NULL;

	for (m = mutations; m; m = m->next) {
		if (streq(m->name, name)) return m;
	}

	return NULL;
}

const struct mutation *mutation_by_index(int index)
{
	const struct mutation *m;

	for (m = mutations; m; m = m->next) {
		if ((int) m->midx == index) return m;
	}

	return NULL;
}

int mutation_count(void)
{
	const struct mutation *m;
	int n = 0;

	for (m = mutations; m; m = m->next) n++;

	return n;
}

bool player_has_mutation(const struct player *p, const struct mutation *mut)
{
	if (!p || !mut) return false;

	return flag_has(p->mutations, MUT_SIZE, mut->midx + 1);
}

int player_mutation_total(const struct player *p)
{
	const struct mutation *m;
	int n = 0;

	if (!p) return 0;

	for (m = mutations; m; m = m->next) {
		if (player_has_mutation(p, m)) n++;
	}

	return n;
}

/** Shed whatever this one drives out, and say how many went. */
static int mutation_shed_opposites(struct player *p,
								   const struct mutation *mut)
{
	size_t g;
	int shed = 0;

	for (g = 0; g < N_ELEMENTS(mutation_cancels); g++) {
		const char *opposite = NULL;
		const struct mutation *other;

		if (streq(mutation_cancels[g][0], mut->name)) {
			opposite = mutation_cancels[g][1];
		} else if (streq(mutation_cancels[g][1], mut->name)) {
			opposite = mutation_cancels[g][0];
		}
		if (!opposite) continue;

		other = mutation_by_name(opposite);
		if (!other || !player_has_mutation(p, other)) continue;

		if (player_lose_mutation(p, other)) shed++;
	}

	return shed;
}

bool player_gain_mutation(struct player *p, const struct mutation *mut)
{
	if (!p || !mut) return false;
	if (player_has_mutation(p, mut)) return false;

	mutation_shed_opposites(p, mut);

	flag_on(p->mutations, MUT_SIZE, mut->midx + 1);

	if (mut->gain) msg("%s", mut->gain);

	/*
	 * Courting chaos, and the game notices (PLR-20). Zangband writes this at
	 * the moment a mutation is gained, whatever brought it on
	 * ([mutation.c:533](../archive/zangband/src/mutation.c#L533)).
	 */
	virtue_change(p, V_CHANCE, 1);

	p->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	p->upkeep->redraw |= (PR_STATS | PR_HP | PR_MANA);

	return true;
}

bool player_lose_mutation(struct player *p, const struct mutation *mut)
{
	if (!p || !mut) return false;
	if (!player_has_mutation(p, mut)) return false;

	flag_off(p->mutations, MUT_SIZE, mut->midx + 1);

	if (mut->lose) msg("%s", mut->lose);

	p->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	p->upkeep->redraw |= (PR_STATS | PR_HP | PR_MANA);

	return true;
}

bool player_lose_random_mutation(struct player *p)
{
	const struct mutation *m;
	int total = player_mutation_total(p), pick;

	if (total <= 0) return false;

	pick = randint0(total);
	for (m = mutations; m; m = m->next) {
		if (!player_has_mutation(p, m)) continue;
		if (pick--) continue;

		return player_lose_mutation(p, m);
	}

	return false;
}

/** Whether this character may be given this one at all. */
static bool mutation_allowed(const struct player *p, const struct mutation *mut)
{
	if (player_has_mutation(p, mut)) return false;

	switch (mut->gate) {
		case MUTATION_GATE_GOLD:
			/* Only the rich turn things to gold. */
			return p->au >= (int32_t) p->lev * 1000L;

		case MUTATION_GATE_MUTATIONS:
			return player_mutation_total(p) >= mut->gate_value;

		default:
			return true;
	}
}

/**
 * Pick a mutation for this character (PLR-38).
 *
 * Weighted, and then overridden. Zangband rolls 1d193 across a switch whose
 * case runs are the weighting -- reproduced here from the `weight` the
 * converter read out of it -- and *then* asks whether the character is one of
 * five races with a mutation of their own, replacing the roll if so
 * ([mutation.c:535](../archive/zangband/src/mutation.c#L535)).
 *
 * The affinity chances differ, and the spoiler does not say so: a Vampire
 * takes hypnotic gaze six times in ten, and a Beastman polymorph self only one
 * time in ten. Both figures live in `p_race.txt` beside the mutation they
 * favour.
 */
const struct mutation *mutation_roll(const struct player *p)
{
	const struct mutation *m, *chosen = NULL;
	int total = 0, roll;

	if (!p) return NULL;

	for (m = mutations; m; m = m->next) {
		if (!mutation_allowed(p, m)) continue;
		total += m->weight;
	}
	if (total <= 0) return NULL;

	roll = randint0(total);
	for (m = mutations; m; m = m->next) {
		if (!mutation_allowed(p, m)) continue;
		if (roll < m->weight) {
			chosen = m;
			break;
		}
		roll -= m->weight;
	}

	/* And what this race tends towards, if it tends towards anything. */
	if (p->race && p->race->mutation_affinity) {
		const struct mutation *favoured =
			mutation_by_name(p->race->mutation_affinity);

		if (favoured && mutation_allowed(p, favoured)
				&& randint1(10) <= p->race->mutation_chance) {
			chosen = favoured;
		}
	}

	return chosen;
}

bool player_mutate(struct player *p)
{
	const struct mutation *mut = mutation_roll(p);

	if (!mut) {
		msg("You feel normal.");
		return false;
	}

	msg("You mutate!");

	return player_gain_mutation(p, mut);
}

/**
 * What the continuous mutations make of the character (PLR-15).
 *
 * Modelled on `calc_shapechange()`, and for the same reason: a mutation is a
 * standing change to the body rather than something worn, so it lands on the
 * state directly and not through an equipment slot. Vulnerabilities are
 * deferred into `vuln[]` exactly as a shape's are, because `calc_bonuses()`
 * applies them after every resistance is in and a vulnerability applied early
 * would be cancelled by a resistance found later.
 *
 * The values are read out of `mutation_effect()`
 * ([mutation.c:1771](../archive/zangband/src/mutation.c#L1771)) rather than
 * out of the spoiler, which gives only the headline of each. Hyper-strength is
 * "+4 STR" in the documentation and +4 STR, -1 INT, -1 WIS in the source; a
 * moronic mind is "-4 INT/WIS" and never mentioned as making the character
 * immune to fear and confusion. Building from the documentation would have
 * made every bad mutation kinder than Zangband's and every good one better.
 *
 * Two mutations do nothing here and are meant to. A silly voice and an
 * illusory normal appearance moved nothing but charisma, which 4.2 removed in
 * 4.2.0; both are still gained, described and saved, and both are inert.
 */
void player_apply_mutations(struct player *p, struct player_state *state,
							bool vuln[ELEM_MAX])
{
	const struct mutation *m;
	int i;

	for (m = mutations; m; m = m->next) {
		if (!player_has_mutation(p, m)) continue;

		state->to_a += m->armour;

		state->skills[SKILL_SAVE] += m->save;
		if (m->save_scale) {
			state->skills[SKILL_SAVE] += p->lev / m->save_scale;
		}

		for (i = 0; i < STAT_MAX; i++) {
			state->stat_add[i] += m->modifiers[i];
		}

		state->skills[SKILL_STEALTH] += m->modifiers[OBJ_MOD_STEALTH];
		state->skills[SKILL_SEARCH] += m->modifiers[OBJ_MOD_SEARCH] * 5;
		state->see_infra += m->modifiers[OBJ_MOD_INFRA];
		state->speed += m->modifiers[OBJ_MOD_SPEED];

		of_union(state->flags, m->flags);

		for (i = 0; i < ELEM_MAX; i++) {
			if (m->el_info[i] == -1) {
				vuln[i] = true;
			} else if (m->el_info[i] > state->el_info[i].res_level) {
				state->el_info[i].res_level = m->el_info[i];
			}
		}
	}
}

/**
 * The mutations that fire on their own (PLR-14, PLR-34).
 *
 * Once per turn, per mutation, at whatever rarity Zangband gave it -- one turn
 * in a thousand for a warning, one in twelve thousand for walking through
 * shadow. `mutation_random_aux()`
 * ([mutation.c:1342](../archive/zangband/src/mutation.c#L1342)) rolls each one
 * separately rather than picking between them, so a character with three of
 * these has three independent chances every turn, and that is kept.
 *
 * Every one of them is suppressed by NO_MAGIC. Zangband tests it inside each
 * mutation's own block, sixteen times over, and the one it *doesn't* test is
 * cowardice -- being too frightened to act is not magic. That reading is
 * followed here rather than tidied into a blanket rule.
 *
 * Six have no 4.2 equivalent and simply never fire; the reasons are in
 * `mutmap.toml` and in the manual.
 */
void player_mutation_turn(struct player *p)
{
	const struct mutation *m;
	bool suppressed;

	if (!p || p->is_dead || !p->upkeep->playing) return;

	suppressed = of_has(p->state.flags, OF_NO_MAGIC);

	for (m = mutations; m; m = m->next) {
		const struct power_effect *band;
		bool ident = false;

		if (m->kind != MUTATION_KIND_RANDOM) continue;
		if (!m->fires || m->chance <= 0) continue;
		if (!player_has_mutation(p, m)) continue;

		/* Cowardice is fear, not sorcery, and fires regardless. */
		if (suppressed && !streq(m->name, "COWARDICE")) continue;

		if (!one_in_(m->chance)) continue;

		disturb(p);

		for (band = m->fires->effects; band; band = band->next) {
			if (p->lev < band->from) continue;
			if (band->to && p->lev > band->to) continue;

			effect_do(band->effect, source_player(), NULL, &ident, true, 0,
					  0, 0, NULL);
		}
	}
}

/**
 * The extra attacks chaos has given the character (PLR-35).
 *
 * Five of them, each once per melee round, after the weapon blows and in the
 * order Zangband lists them ([cmd1.c:1981](../archive/zangband/src/cmd1.c#L1981)).
 * A dead monster stops the rest, which is why `dead` is checked between each.
 *
 * The dice are the source's and not the description's. `natural_attack()`
 * fills in `dss` and `ddd` and then calls `damroll(ddd, dss)`, whose parameters
 * are `(num, sides)` -- so a scorpion tail rolls 7d3 where its own text says
 * "3d7". All five are written the wrong way round, and all five hit harder in
 * the code than in the documentation.
 */
void player_mutation_blows(struct player *p, struct monster *mon,
						   bool *fear, bool *dead)
{
	const struct mutation *m;

	if (!p || !mon) return;

	for (m = mutations; m; m = m->next) {
		int dam;

		if (m->kind != MUTATION_KIND_MELEE) continue;
		if (!player_has_mutation(p, m)) continue;
		if (*dead) return;

		/*
		 * The same to-hit the weapon used, because it is the same character
		 * swinging -- Zangband builds the chance out of SKILL_THN and to_h
		 * and nothing else, with no penalty for the limb being a mutation.
		 */
		if (!test_hit(chance_of_melee_hit_base(p, NULL),
					  mon->race->ac)) {
			msg("You miss %s with your %s.", "it", m->blow_verb);
			continue;
		}

		dam = randcalc(m->blow, 0, RANDOMISE);
		dam = critical_melee(p, NULL, m->blow_weight, p->state.to_h, dam,
							 NULL);
		dam += p->state.to_d;
		if (dam < 0) dam = 0;

		msg("You hit %s with your %s.", "it", m->blow_verb);

		if (m->blow_element >= 0) {
			project(source_player(), 0, mon->grid, dam, m->blow_element,
					PROJECT_KILL, 0, 0, NULL);
			*dead = !square_monster(cave, mon->grid);
		} else {
			*dead = mon_take_hit(mon, p, dam, fear, NULL);
		}
	}
}

/**
 * The regeneration penalty, which is zero (DEC-45).
 *
 * `spoilers/mutation.txt` warns that mutations past the first few slow a
 * character's healing. That was true of Zangband 2.2.2d and had been taken out
 * again by 2.7.5-pre1: `count_mutations()` survives in the source with two
 * callers, both of them prerequisite checks, and nothing scales regeneration
 * by it.
 *
 * Kept as a function returning zero rather than left out, because the question
 * "does carrying twelve mutations cost anything" is one somebody will ask of
 * this file, and the answer wants to be here with its reasons rather than in a
 * decision log they have not read.
 */
int mutation_regen_penalty(const struct player *p)
{
	(void) p;

	return 0;
}
