/**
 * \file player-virtue.h
 * \brief The virtues a character is measured against (PLR-18 to PLR-21)
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

#ifndef PLAYER_VIRTUE_H
#define PLAYER_VIRTUE_H

#include "player.h"

/** The eighteen, as an enum over list-virtues.h. */
enum {
	V_NONE = 0,
	#define V(a, b) V_##a,
	#include "list-virtues.h"
	#undef V
	V_MAX
};

/** How far a virtue can run in either direction (avatar.c:344). */
#define VIRTUE_CAP 125

const char *virtue_name(int virtue);
const char *virtue_code(int virtue);
void virtues_select(struct player *p);
void virtue_change(struct player *p, int virtue, int amount);
int virtue_value(const struct player *p, int virtue);
bool player_has_virtue(const struct player *p, int virtue);
const char *virtue_describe(int value);

/* One of a character's eight virtues, as a sentence. False for an empty slot. */
bool virtue_line(const struct player *p, int slot, char *buf, size_t len);
void virtue_note_kill(struct player *p, const struct monster_race *race,
					  int depth);
void virtue_note_timed(struct player *p, int idx, int old, int new_value);

#endif /* !PLAYER_VIRTUE_H */
