/**
 * \file player-luck.h
 * \brief Zangband's weird luck, psi-powered criticals and anti-magic
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

#ifndef PLAYER_LUCK_H
#define PLAYER_LUCK_H

#include "player.h"

int luck_crit_scale(const struct player *p, int value);
int luck_monster_crit(const struct player *p, int dam);
int luck_depth_boost(const struct player *p);
bool psi_crit_armed(const struct player *p);
bool psi_crit_fires(void);
int psi_crit_spend(struct player *p);
bool player_magic_blocked(const struct player *p, bool show_msg);

#endif /* !PLAYER_LUCK_H */
