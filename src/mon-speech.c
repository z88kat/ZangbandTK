/**
 * \file mon-speech.c
 * \brief What a monster says (ZangbandTK, CNT-04)
 *
 * Zangband's CAN_SPEAK, which the flag report filed as "pure flavour" and which
 * is not quite that.  Three things hang off it:
 *
 *  - a line in a fight, one turn in eight, and a different line if the monster
 *    has turned and run ([melee2.c:3050](../archive/zangband/src/melee2.c#L3050))
 *  - a line when it dies ([xtra2.c:884](../archive/zangband/src/xtra2.c#L884))
 *  - a **bounty**: a unique that could talk turns out, one death in ten, to have
 *    been wanted for something, and the price on its head is real gold
 *    ([xtra2.c:895](../archive/zangband/src/xtra2.c#L895))
 *
 * The third is the one the name does not suggest and the one worth having: it
 * pays out `250 * (1d10 + level - 5)`, up to 32,000, which is a serious sum, and
 * it gives a reason to read the message rather than dismiss it.
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
#include "mon-desc.h"
#include "mon-predicate.h"
#include "mon-speech.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "player-calcs.h"

/**
 * One turn in this many, a monster in a fight says something.  Zangband's
 * SPEAK_CHANCE ([melee2.c:20](../archive/zangband/src/melee2.c#L20)).
 */
#define SPEAK_CHANCE 8

/**
 * One death in this many carries a bounty.  Zangband's REWARD_CHANCE
 * ([xtra2.c:15](../archive/zangband/src/xtra2.c#L15)).
 */
#define REWARD_CHANCE 10

struct monster_speech_pool *monster_speech_pool(struct monster_speech *s,
												const char *key)
{
	if (streq(key, "speak")) return &s->speak;
	if (streq(key, "fear")) return &s->fear;
	if (streq(key, "death")) return &s->death;
	if (streq(key, "crime")) return &s->crime;

	return NULL;
}

static void pool_free(struct monster_speech_pool *pool)
{
	int i;

	for (i = 0; i < pool->count; i++) {
		string_free(pool->line[i]);
	}

	mem_free(pool->line);
	pool->line = NULL;
	pool->count = 0;
}

void monster_speech_free(struct monster_speech *s)
{
	pool_free(&s->speak);
	pool_free(&s->fear);
	pool_free(&s->death);
	pool_free(&s->crime);
}

const char *monster_speech_line(const struct monster_speech_pool *pool)
{
	if (!pool || pool->count <= 0) return NULL;

	return pool->line[randint0(pool->count)];
}

/**
 * A monster says something in the middle of a fight.
 *
 * Only when the player can see it: a voice from somewhere in the dark, with
 * nothing to attach it to, reads as a bug rather than as atmosphere.  Zangband
 * asked the same question, in line of sight and on the grid the monster is
 * standing on.
 */
void monster_speak(struct monster *mon)
{
	const struct monster_speech_pool *pool;
	const char *line;
	char m_name[80];

	if (!mon || !mon->race) return;
	if (!rf_has(mon->race->flags, RF_CAN_SPEAK)) return;
	if (!monster_is_visible(mon)) return;
	if (!square_isview(cave, mon->grid)) return;
	if (!one_in_(SPEAK_CHANCE)) return;

	/* What it says depends on whether it is still winning. */
	pool = mon->m_timed[MON_TMD_FEAR] ? &mon_speech.fear : &mon_speech.speak;

	line = monster_speech_line(pool);
	if (!line) return;

	monster_desc(m_name, sizeof(m_name), mon, MDESC_CAPITAL);
	msg("%s %s", m_name, line);
}

/**
 * And what it says on the way out.
 *
 * Said whether or not the player can see it -- a death cry carries, and the
 * player did after all just cause it.
 */
void monster_speak_death(struct monster *mon, const char *name)
{
	const char *line;

	if (!mon || !mon->race) return;
	if (!rf_has(mon->race->flags, RF_CAN_SPEAK)) return;

	line = monster_speech_line(&mon_speech.death);
	if (!line) return;

	msg("%s says: %s", name, line);
}

/**
 * There was a price on its head (ZangbandTK, CNT-04).
 *
 * Uniques only, and only those that could talk -- which is the join that makes
 * this work as flavour rather than as a payout table: the things with a bounty
 * on them are the things somebody could have complained about.
 */
void monster_claim_bounty(struct player *p, struct monster *mon,
						  const char *name)
{
	const char *crime;
	int reward;

	if (!mon || !mon->race) return;
	if (!rf_has(mon->race->flags, RF_CAN_SPEAK)) return;
	if (!rf_has(mon->race->flags, RF_UNIQUE)) return;
	if (!one_in_(REWARD_CHANCE)) return;

	crime = monster_speech_line(&mon_speech.crime);
	if (!crime) return;

	/*
	 * Zangband's formula, clamped at both ends the way the original clamps it:
	 * a low-level unique should still be worth collecting, and a deep one
	 * should not hand over a fortune that ends the economy.
	 */
	reward = 250 * (randint1(10) + mon->race->level - 5);
	reward = MIN(MAX(reward, 250), 32000);

	msg("There was a price on %s's head.", name);
	msg("%s was wanted for %s", name, crime);
	msg("You collect a reward of %d gold pieces.", reward);

	p->au += reward;
	p->upkeep->redraw |= PR_GOLD;
}
