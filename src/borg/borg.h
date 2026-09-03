/**
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband License":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#ifndef INCLUDED_BORG_H
#define INCLUDED_BORG_H

/*
 * must be included before ALLOW_BORG to avoid empty compilation unit
 */
#include "../angband.h"
#include "../obj-ignore.h"

#ifdef ALLOW_BORG

#include "borg-trait.h"

extern bool borg_cheat_death;

/*
 * Use a simple internal random number generator
 */
extern uint32_t borg_rand_local; /* Save personal setting */

/*
 * Date of the last change
 */
extern char borg_engine_date[];

/* options from the borg.txt file */
/* IMPORTANT keep these in sync with borg_settings in borg-init.c */
enum {
    BORG_VERBOSE,
    BORG_MUNCHKIN_START,
    BORG_MUNCHKIN_LEVEL,
    BORG_MUNCHKIN_DEPTH,
    BORG_WORSHIPS_DAMAGE,
    BORG_WORSHIPS_SPEED,
    BORG_WORSHIPS_HP,
    BORG_WORSHIPS_MANA,
    BORG_WORSHIPS_AC,
    BORG_WORSHIPS_GOLD,
    BORG_PLAYS_RISKY,
    BORG_KILLS_UNIQUES,
    BORG_USES_SWAPS,
    BORG_USES_DYNAMIC_CALCS,
    BORG_STOP_DLEVEL,
    BORG_STOP_CLEVEL,
    BORG_NO_DEEPER,
    BORG_STOP_KING,
    BORG_CHEAT_DEATH,
    BORG_RESPAWN_WINNERS,
    BORG_RESPAWN_CLASS,
    BORG_RESPAWN_RACE,
    BORG_CHEST_FAIL_TOLERANCE,
    BORG_DELAY_FACTOR,
    BORG_MONEY_SCUM_AMOUNT,
    BORG_SELF_SCUM,
    BORG_LUNAL_MODE,
    BORG_SELF_LUNAL,
    BORG_ENCHANT_LIMIT,
    BORG_DUMP_LEVEL,
    BORG_SAVE_DEATH,
    BORG_STOP_ON_BELL,
    BORG_ALLOW_STRANGE_OPTS,
    BORG_AUTOSAVE,
    BORG_RESTORE_IGNORE_SETTINGS,
    BORG_MAX_SETTINGS
};
extern int *borg_cfg;

/*
 * Status variables
 */
extern bool borg_active; /* Actually active */
extern bool borg_cancel; /* Being cancelled */

/*
 * ZangbandTK (BRG-03, BRG-05): a headless run's budget and its verdict.
 *
 * The borg has exactly one way in -- `^z` then `z`, through the UI -- and
 * exactly one way out: a keypress. Neither is available to a test, and
 * injecting keys is not reproducible, because the birth menus consume a
 * number of them that depends on the roll and the borg sometimes activates
 * before `character_dungeon` is set and aborts.
 *
 * `borg_turn_limit` is the game turn at which a run stops itself; 0 means the
 * run is not bounded, which is the interactive case. `borg_abort_reason` is
 * NULL until something goes wrong and then holds `borg_oops()`'s words, so a
 * caller can tell a clean stop from an abort without reading the log --
 * `borg_oops()` merely stopped, and from outside a crash looked exactly like a
 * tidy retirement.
 */
extern int32_t borg_turn_limit;
extern const char *borg_abort_reason;

/*
 * ZangbandTK (BRG-05): a decision budget, so a wedged run ends.
 *
 * The turn budget assumes the borg advances game turns. A borg that cannot
 * see a prompt it is waiting for makes decisions forever without the clock
 * moving, and a run like that neither crashes nor aborts -- it hangs, which in
 * CI is worse than either, because a hung job is a red build with no
 * diagnosis. This counts decisions rather than turns, and running out of them
 * is a failure with its own name.
 */
extern int32_t borg_step_limit;
extern int32_t borg_step_count;

/*
 * ZangbandTK (BRG-03): there is nobody at the keyboard.
 *
 * The borg's only stop signal is a real keypress, which is right for a player
 * watching it and wrong for a scripted run: a headless front end has to feed
 * the terminal *something* or its event poll waits forever, and anything it
 * feeds is read as somebody reaching for the keyboard. Upstream exempts key
 * code 10 from the abort check, but `Term_keypress(10)` is translated to
 * `KC_ENTER` (156) before the borg ever sees it, so the exemption cannot be
 * reached from outside.
 *
 * Set for a bounded run, where the turn and step budgets are what stop it.
 */
extern bool borg_headless;

/*
 * ZangbandTK (BRG-03): start the borg the way the menu does.
 *
 * Defined in borg-commands.c beside the menu that calls it, and declared here
 * so a headless front end can reach it without going through `get_com()`.
 */
extern void borg_cmd_start(void);
extern bool borg_save; /* do a save next time we get to press a key! */

extern int16_t old_depth;
extern int16_t borg_respawning;

/*
 * Time variables
 */
extern int16_t borg_t; /* Current "time" */
extern int32_t borg_began; /* When this level began */
extern int32_t borg_time_town; /* how long it has been since I was in town */
extern int16_t borg_t_morgoth; /* Last time I saw Morgoth */

/*
 * Number of turns to (manually) step for (zero means forever)
 */
extern uint16_t borg_step;

extern int w_x; /* Current panel offset (X) */
extern int w_y; /* Current panel offset (Y) */

struct borg_save_init {

	/*
	 * KEYMAP_MODE_ROGUE or KEYMAP_MODE_ORIG
	 */
	int         key_mode;

	/*
	 * object ignore settings
	 */
	uint8_t*    kinfo_ignore;
	uint8_t     ignore_level[ITYPE_MAX];
	bool**      ego_ignore_types;
};
extern struct borg_save_init borg_init_save;

/*
 * Special "inkey_hack" hook.
 */
extern struct keypress (*inkey_hack)(int flush_first);

/*
 * Set the hook for the game.
 */
extern void borg_update_entrypoint(bool start);

#endif
#endif
