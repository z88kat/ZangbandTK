/**
 * \file player-mutation.h
 * \brief Chaos mutations (PLR-13 to PLR-17, PLR-34 to PLR-38)
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

#ifndef PLAYER_MUTATION_H
#define PLAYER_MUTATION_H

#include "object.h"
#include "player.h"

/**
 * What a mutation does to the character carrying it.
 *
 * Zangband's three groups are a bitfield layout -- 32 flags is what fits in a
 * `u32b`, and nothing more is meant by it. These four are what the player
 * meets, and are the spoiler's own grouping. See DEC-44.
 */
enum mutation_kind {
	MUTATION_KIND_NONE = 0,
	MUTATION_KIND_ACTIVATABLE,	/**< A power, invoked and paid for */
	MUTATION_KIND_RANDOM,		/**< Fires on its own, rarely */
	MUTATION_KIND_CONTINUOUS,	/**< Simply true of the character */
	MUTATION_KIND_MELEE,		/**< An extra blow, in the attack round */
	MUTATION_KIND_MAX
};

/** What a mutation needs before it can be given at all. */
enum mutation_gate {
	MUTATION_GATE_NONE = 0,
	MUTATION_GATE_GOLD,		/**< A thousand gold per level, in hand */
	MUTATION_GATE_MUTATIONS	/**< This many mutations already */
};

struct mutation {
	struct mutation *next;
	char *name;			/**< Code, as the data file and savefile write it */
	char *desc;			/**< What the character sheet says */
	char *gain;			/**< What is printed on gaining it */
	char *lose;			/**< What is printed on losing it */
	char *power;		/**< Short label for the power list */

	unsigned int midx;	/**< Index, assigned on load */
	int kind;

	int level;			/**< Activatable: earliest usable level */
	int cost;			/**< Activatable: mana, or hit points */
	int stat;			/**< Activatable: the stat rolled against */
	int difficulty;		/**< Activatable: how hard that roll is */

	int chance;			/**< Random: one turn in this many */
	int weight;			/**< How often selection lands on it */

	int gate;			/**< A prerequisite, or none */
	int gate_value;

	/** What a continuous mutation is simply true of (PLR-15). */
	int modifiers[OBJ_MOD_MAX];
	int armour;		/**< Armour class, which is not a modifier in 4.2 */
	int save;		/**< Saving throw, likewise */
	int save_scale;	/**< ...plus level over this, if set */

	/**
	 * What an activatable mutation does, if 4.2 can express it (PLR-16).
	 *
	 * The same `struct player_power` a race or a class carries, because to
	 * the player it is the same kind of thing -- it appears in the same list,
	 * costs mana or blood the same way, and fails against a stat the same
	 * way. Null for the nine whose Zangband implementation has no 4.2
	 * equivalent; those are still gained, described and saved.
	 */
	struct player_power *action;

	/**
	 * What a random mutation does on the turn its roll comes up (PLR-14).
	 *
	 * The same structure again, and for the same reason -- it is an effect
	 * chain with a level band -- but it is never chosen, so its level, cost
	 * and failure go unread. Null for the six with no 4.2 equivalent.
	 */
	struct player_power *fires;

	/** An extra attack in the melee round (PLR-35). */
	random_value blow;
	int blow_weight;	/**< For the critical-hit table, in tenth-pounds */
	int blow_element;	/**< An element it carries, or -1 */
	char *blow_verb;	/**< "You hit it with your tail." */

	/**
	 * True when this mutation's power was turned down rather than queued.
	 *
	 * Eleven activatable and random mutations have no effect chain because
	 * 4.2 has no mechanism for them yet; one has none because the mechanism
	 * was considered and refused (DEC-48). The power menu lists both, and
	 * needs to tell them apart -- "not yet" is a promise, and it is one this
	 * game is not going to keep for the Midas touch.
	 */
	bool refused;
	int16_t el_info[ELEM_MAX];
	bitflag flags[OF_SIZE];

	/**
	 * What this mutation takes away, whoever it came from (PLR-15).
	 *
	 * Zangband's `player_flags()` accumulates race, equipment and mutations
	 * together and then *clears* three flags: rotting flesh stops a character
	 * regenerating, and the panic-hit and warning mutations stop them
	 * resisting fear. Written as `flags[n] &= ~(TRn_X)` rather than as a
	 * SET_FLAG, which is why the converter read nothing here for five
	 * releases and all three mutations were kinder than Zangband's.
	 *
	 * These beat the grants, so a character wearing a ring of regeneration
	 * loses it to rotting flesh -- which is the point of the mutation.
	 */
	bitflag suppress[OF_SIZE];
};

extern struct mutation *mutations;

const struct mutation *mutation_by_name(const char *name);
const struct mutation *mutation_by_index(int index);
int mutation_count(void);

bool player_has_mutation(const struct player *p, const struct mutation *mut);
int player_mutation_total(const struct player *p);
bool player_gain_mutation(struct player *p, const struct mutation *mut);
bool player_lose_mutation(struct player *p, const struct mutation *mut);
bool player_lose_random_mutation(struct player *p);
const struct mutation *mutation_roll(const struct player *p);
bool player_mutate(struct player *p);
int mutation_regen_penalty(const struct player *p);

/* Light radius contributed by the character's mutations. */
int mutation_light_bonus(const struct player *p);
void player_apply_mutations(struct player *p, struct player_state *state,
							bool vuln[ELEM_MAX]);
void player_mutation_turn(struct player *p);
void player_mutation_blows(struct player *p, struct monster *mon,
						   bool *fear, bool *dead);

#endif /* !PLAYER_MUTATION_H */
