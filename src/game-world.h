/**
 * \file game-world.h
 * \brief Game core management of the game world
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include "cave.h"

struct level {
	int depth;
	char *name;
	char *up;
	char *down;
	struct level *next;
};

extern uint16_t daycount;
extern uint32_t seed_randart;
extern uint32_t seed_flavor;
extern int32_t turn;
extern bool character_generated;
extern bool character_dungeon;
extern const uint8_t extract_energy[200];
extern struct level *world;

const char *migrate_level_name(const char *name);
struct level *level_by_name(const char *name);
struct level *level_by_depth(int depth);
bool is_daytime(void);
int turn_energy(int speed);
void play_ambient_sound(void);
/** `nightmare_bell_at()` returns this at midnight, and 1 to 4 for the bell. */
#define NIGHTMARE_CURSE 9

/**
 * How many breeders a nightmare level will hold (BAL-18, DEC-126).
 *
 * Ours, not a port: the spoiler names 255 and we are choosing to take it, so
 * it is our design under BAL-18 rather than Zangband behaviour restored.
 * `repro_monster_cap()` in `mon-move.c` is the only thing that should read it.
 */
#define NIGHTMARE_REPRO_MAX 255

int nightmare_bell_at(int32_t at_turn);
int nightmare_recall_depth(struct player *p, int depth);
void process_world(struct chunk *c);
void on_new_level(void);
void process_player(void);
void run_game_loop(void);

#endif /* !GAME_WORLD_H */
