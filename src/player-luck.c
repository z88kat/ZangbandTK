/**
 * \file player-luck.c
 * \brief Zangband's weird luck, psi-powered criticals and anti-magic
 *
 * Three object flags the object import needed (CNT-11), kept together because
 * each is a one-line change at two or four scattered call sites and the
 * reasoning is worth having in one place.
 *
 * `STRANGE_LUCK` is the interesting one, and its name is a trap. The Ring of
 * Fate does not make you lucky. It multiplies every melee critical by 3/2 --
 * *including the ones monsters land on you* -- and gives a one-in-thirteen
 * chance that the next monster generated comes from up to forty levels deeper
 * than it should. Zangband's own comment at the third of those sites reads
 * "Luck isn't always good for you...". Reading the flag as a saving-throw
 * bonus, the way `LUCK_10` genuinely is, would produce a quite different item.
 *
 * `PSI_CRIT` spends mana to land and to deepen criticals, and `NO_MAGIC` stops
 * the wearer casting at all.
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

#include "player-luck.h"

#include "init.h"
#include "obj-knowledge.h"
#include "player-calcs.h"
#include "player-timed.h"

/**
 * Numerator and denominator of the weird-luck critical scale.
 *
 * Zangband writes `power = power * 3 / 2` at all three of its critical sites
 * ([cmd1.c:115](../archive/zangband/src/cmd1.c#L115),
 * [cmd1.c:215](../archive/zangband/src/cmd1.c#L215) and
 * [melee1.c:28](../archive/zangband/src/melee1.c#L28)). Taken unchanged
 * rather than rederived: it is a ratio applied to 4.2's own critical numbers,
 * not a constant calibrated against Zangband's scale, so there is nothing here
 * that a measurement would settle.
 */
#define LUCK_CRIT_NUM 3
#define LUCK_CRIT_DEN 2

/**
 * Weird luck's out-of-depth generation.
 *
 * One monster in thirteen is generated deeper than the level calls for, and
 * one of those in seven is deeper still
 * ([monster2.c:782](../archive/zangband/src/monster2.c#L782)).
 */
#define LUCK_OOD_CHANCE 13
#define LUCK_OOD_DEEP 7
#define LUCK_OOD_SHALLOW_MAX 10
#define LUCK_OOD_DEEP_MAX 40

/**
 * The mana a psi-powered critical costs, per level of bonus.
 *
 * `PSI_COST` in [defines.h:789](../archive/zangband/src/defines.h#L789).
 */
#define PSI_COST 1

/** Chance in 100 that a psiblade is armed for the blow. */
#define PSI_ARM_CHANCE 80

/** Chance in 100 that an armed psiblade turns a blow into a critical. */
#define PSI_CRIT_CHANCE 20

/**
 * Scale a critical hit's chance or power for weird luck.
 *
 * Applied to both halves in 4.2, as in Zangband: the roll that decides whether
 * a critical happens, and the figure that decides how good it is.
 */
int luck_crit_scale(const struct player *p, int value)
{
	if (!p || !of_has(p->state.flags, OF_STRANGE_LUCK)) return value;

	return value * LUCK_CRIT_NUM / LUCK_CRIT_DEN;
}

/**
 * Scale a critical a monster has landed on the player.
 *
 * The half of the flag nobody would guess from the name. Zangband applies the
 * same 3/2 to the monster's critical as to the player's, so the Ring of Fate
 * cuts in both directions.
 */
int luck_monster_crit(const struct player *p, int dam)
{
	if (!p || !of_has(p->state.flags, OF_STRANGE_LUCK)) return dam;

	return dam * LUCK_CRIT_NUM / LUCK_CRIT_DEN;
}

/**
 * Extra depth to generate a monster at, for weird luck.
 *
 * Returns 0 almost always. The player wearing the ring meets the occasional
 * thing that has no business being on the level.
 */
int luck_depth_boost(const struct player *p)
{
	if (!p || !of_has(p->state.flags, OF_STRANGE_LUCK)) return 0;
	if (!one_in_(LUCK_OOD_CHANCE)) return 0;

	return randint1(one_in_(LUCK_OOD_DEEP)
					? LUCK_OOD_DEEP_MAX : LUCK_OOD_SHALLOW_MAX);
}

/**
 * Whether a psi-powered weapon is armed for this blow.
 *
 * Needs mana in the pool and an 80% roll. Costs nothing on its own -- the mana
 * is spent only if the blow actually becomes a critical, which is why arming
 * and spending are two calls.
 */
bool psi_crit_armed(const struct player *p)
{
	if (!p || !of_has(p->state.flags, OF_PSI_CRIT)) return false;
	if (p->csp < PSI_COST) return false;

	return randint0(100) < PSI_ARM_CHANCE;
}

/**
 * Whether an armed psiblade turns this blow into a critical it would not
 * otherwise have been.
 *
 * Zangband tests this alongside the ordinary critical roll rather than
 * instead of it, so the psiblade only ever adds criticals.
 */
bool psi_crit_fires(void)
{
	return randint0(100) < PSI_CRIT_CHANCE;
}

/**
 * Spend mana on a critical an armed psiblade is powering, and say by how much
 * to multiply it.
 *
 * One in twelve blows buys a triple, one in three a double, the rest a single,
 * each only if the mana is there for it.
 */
int psi_crit_spend(struct player *p)
{
	int bonus;

	if (!p) return 1;

	if (one_in_(12) && p->csp >= PSI_COST * 3) {
		bonus = 3;
	} else if (one_in_(3) && p->csp >= PSI_COST * 2) {
		bonus = 2;
	} else {
		bonus = 1;
	}

	p->csp -= PSI_COST * bonus;
	p->upkeep->redraw |= (PR_MANA);

	return bonus;
}

/**
 * Whether an anti-magic shell is stopping the player casting.
 *
 * Zangband names the thing being disrupted after the caster's class -- magic,
 * prayer or psionic powers ([dungeon.c:2240](../archive/zangband/src/dungeon.c#L2240)).
 * 4.2 keeps the same vocabulary on the magic realm, so the message can be
 * built from the realm rather than from a list of class names.
 */
bool player_magic_blocked(const struct player *p, bool show_msg)
{
	if (!p || !of_has(p->state.flags, OF_NO_MAGIC)) return false;

	if (show_msg) {
		const struct class_magic *magic = &p->class->magic;
		const char *noun = "magic";

		if (magic->num_books && magic->books[0].realm
				&& magic->books[0].realm->spell_noun) {
			noun = magic->books[0].realm->spell_noun;
		}

		msg("An anti-magic shell disrupts your %s!", noun);
	}

	return true;
}
