/**
 * \file mon-aura.c
 * \brief Auras and bolt reflection (ZangbandTK, CNT-04 and CNT-09)
 *
 * Four monster flags and three object flags, built together because they are
 * three mechanisms seen from two sides.  A monster with AURA_FIRE burns whoever
 * strikes it; a character wearing SH_FIRE burns whoever strikes them.  A monster
 * with REFLECTING bounces bolts; a character with REFLECT does the same.  Doing
 * one side without the other would mean writing each rule twice.
 *
 * Two things in here are not what the names suggest, both found by reading
 * Zangband rather than guessing:
 *
 *  - **A fire aura keeps the monster out of the water.**  AURA_FIRE is not only
 *    a damage source: `cave_passable_mon()` returns a move chance of zero for
 *    shallow water, so a burning thing will not wade in
 *    ([melee2.c:311](../archive/zangband/src/melee2.c#L311)).  That was dead
 *    weight in a game with no water in it, and this one has a sea.
 *  - **Reflection is not certain and not universal.**  It fails one time in
 *    ten, it does not work against a ball or a breath, and the bolt does not go
 *    back to the caster -- it goes to a random grid beside the caster, which
 *    may miss ([spells1.c:4670](../archive/zangband/src/spells1.c#L4670)).
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
#include "mon-aura.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "player-calcs.h"
#include "player-util.h"

/**
 * One bolt in this many gets through a reflector.  Zangband's `!one_in_(10)`,
 * used on both sides.
 */
#define REFLECT_FAILS_IN 10

/**
 * Average hit points per hit die, over the 69 Zangband monsters carrying an
 * aura, used to rescale the damage below.
 *
 * Zangband rolled `damroll(1 + hdice * 2 / 26, 1 + level / 17)` and 4.2 has no
 * hdice -- BAL-06 turned every `NdM` into one average-hit-points integer, so
 * the number the original multiplied is gone.  Measured rather than guessed,
 * per BAL-08: the median is 32.5 average hit points per die across that set,
 * which puts the dice count at `1 + avg_hp / (13 * 32.5)`.  Rounded to 425.
 *
 * It lands where the original did.  A monster of 1000 average hit points rolls
 * three dice, which is what thirty hit dice gave before, and the count still
 * runs from one to five across the whole set.  The number of *sides* needs no
 * rescaling: it comes from the monster's level, which both games keep.
 */
#define AURA_HP_PER_DIE 425

/**
 * How hard a monster's aura hits.
 */
static int monster_aura_dam(const struct monster_race *race)
{
	int dice = 1 + race->avg_hp / AURA_HP_PER_DIE;
	int sides = 1 + race->level / 17;

	return damroll(dice, sides);
}

/**
 * The aura of a monster the player has just struck (CNT-04).
 *
 * Immunity stops it outright, as in the original -- resistance only reduces it,
 * which 4.2's own damage path already applies.
 */
void monster_aura_touch(struct player *p, struct monster *mon)
{
	struct monster_lore *lore;
	char m_name[80];
	bool seen;

	if (!mon || !mon->race || !p) return;

	lore = get_lore(mon->race);
	seen = monster_is_visible(mon);
	monster_desc(m_name, sizeof(m_name), mon, MDESC_DIED_FROM);

	if (rf_has(mon->race->flags, RF_AURA_FIRE) && !p->state.el_info[ELEM_FIRE].res_level) {
		if (seen) rf_on(lore->flags, RF_AURA_FIRE);
		msg("You are suddenly very hot!");
		take_hit(p, monster_aura_dam(mon->race), m_name);
	}

	if (!p->is_dead && rf_has(mon->race->flags, RF_AURA_COLD)
			&& !p->state.el_info[ELEM_COLD].res_level) {
		if (seen) rf_on(lore->flags, RF_AURA_COLD);
		msg("You are suddenly very cold!");
		take_hit(p, monster_aura_dam(mon->race), m_name);
	}

	if (!p->is_dead && rf_has(mon->race->flags, RF_AURA_ELEC)
			&& !p->state.el_info[ELEM_ELEC].res_level) {
		if (seen) rf_on(lore->flags, RF_AURA_ELEC);
		msg("You get zapped!");
		take_hit(p, monster_aura_dam(mon->race), m_name);
	}
}

/**
 * The player's own aura, on a monster that has just struck them (CNT-09).
 *
 * A flat 2d6 in the original, unscaled by anything -- it is a property of the
 * armour rather than of the wearer, so there is no level to scale by.
 */
void player_aura_touch(struct player *p, struct monster *mon)
{
	struct monster_lore *lore;
	bool seen;

	if (!mon || !mon->race || !p) return;
	if (mon->hp <= 0) return;

	lore = get_lore(mon->race);
	seen = monster_is_visible(mon);

	if (player_of_has(p, OF_SH_FIRE)) {
		equip_learn_flag(p, OF_SH_FIRE);

		if (rf_has(mon->race->flags, RF_IM_FIRE)) {
			if (seen) rf_on(lore->flags, RF_IM_FIRE);
		} else {
			char m_name[80];

			monster_desc(m_name, sizeof(m_name), mon, MDESC_CAPITAL);
			msg("%s is suddenly very hot!", m_name);
			mon_take_nonplayer_hit(damroll(2, 6), mon, MON_MSG_NONE,
								   MON_MSG_DIE);
		}
	}

	if (mon->race && mon->hp > 0 && player_of_has(p, OF_SH_ELEC)) {
		equip_learn_flag(p, OF_SH_ELEC);

		if (rf_has(mon->race->flags, RF_IM_ELEC)) {
			if (seen) rf_on(lore->flags, RF_IM_ELEC);
		} else {
			char m_name[80];

			monster_desc(m_name, sizeof(m_name), mon, MDESC_CAPITAL);
			msg("%s gets zapped!", m_name);
			mon_take_nonplayer_hit(damroll(2, 6), mon, MON_MSG_NONE,
								   MON_MSG_DIE);
		}
	}
}

/**
 * Whether a bolt bounces off whoever it just reached.
 *
 * \param has_flag is whether the target reflects at all.
 * \param rad is the radius of the effect; anything with one is a ball or a
 * breath, and those are not reflected -- only a single bolt is.
 */
bool aura_bolt_reflects(bool has_flag, int rad)
{
	if (!has_flag) return false;
	if (rad > 0) return false;

	return !one_in_(REFLECT_FAILS_IN);
}

/**
 * Where a reflected bolt goes.
 *
 * Not back at the caster: at a grid beside the caster, chosen at random and
 * checked for line of sight, which is why a reflected bolt often misses the
 * thing that fired it.  Ten attempts, then give up and let it go nowhere.
 *
 * \return false if nowhere suitable was found.
 */
bool aura_reflect_target(struct loc from, struct loc *to)
{
	int attempts = 10;

	while (attempts--) {
		struct loc try = loc(from.x + rand_range(-1, 1),
							 from.y + rand_range(-1, 1));

		if (!square_in_bounds_fully(cave, try)) continue;
		if (!los(cave, from, try)) continue;

		*to = try;
		return true;
	}

	return false;
}
