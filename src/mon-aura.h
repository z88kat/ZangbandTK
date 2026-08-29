/**
 * \file mon-aura.h
 * \brief Auras and bolt reflection (ZangbandTK, CNT-04 and CNT-09)
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

#ifndef MON_AURA_H
#define MON_AURA_H

#include "monster.h"
#include "player.h"

void monster_aura_touch(struct player *p, struct monster *mon);
void player_aura_touch(struct player *p, struct monster *mon);

bool aura_bolt_reflects(bool has_flag, int rad);
bool aura_reflect_target(struct loc from, struct loc *to);

#endif /* MON_AURA_H */
