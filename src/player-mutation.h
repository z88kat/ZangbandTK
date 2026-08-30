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

#endif /* !PLAYER_MUTATION_H */
