/**
 * \file ui-birth.h
 * \brief Text-based user interface for character creation
 *
 * Copyright (c) 1987 - 2015 Angband contributors
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

#ifndef INCLUDED_UI_BIRTH_H
#define INCLUDED_UI_BIRTH_H

#include "ui-event.h"

enum birth_stage
{
	BIRTH_BACK = -1,
	BIRTH_RESET = 0,
	BIRTH_QUICKSTART,
	BIRTH_RACE_CHOICE,
	BIRTH_CLASS_CHOICE,
	BIRTH_REALM_CHOICE,
	BIRTH_ROLLER_CHOICE,
	BIRTH_POINTBASED,
	BIRTH_ROLLER,
	BIRTH_NAME_CHOICE,
	BIRTH_HISTORY_CHOICE,
	BIRTH_FINAL_CONFIRM,
	BIRTH_COMPLETE
};

/**
 * What a front end's own view of a birth stage answered (ZangbandTK, T8).
 *
 * textui_do_birth() keeps the stages, their order, stepping back, and what
 * each stage pushes.  A front end that draws character creation itself is
 * asked only for each stage's input, and answers in the stage's own terms:
 *
 * - key: a key the stage already understands.  ESCAPE steps back; '*' is a
 *   random choice and '@' finishes at random; '=' is the birth options and
 *   KTRL('X') quits; the quick start takes 'y', 'n' and 'c'; the roller 'r'
 *   and 'p'; the points '+', '-' and 'r'; and KC_ENTER accepts.
 * - choice: a menu stage's row, or at the points the stat that '+' buys or
 *   '-' sells; -1 for none.
 * - text: the name at the name stage, or at the history stage a new history,
 *   with 'n'.
 */
struct birth_answer {
	struct keypress key;
	int choice;
	char text[240];
};

/**
 * NULL for the text screens.  Otherwise called where each stage would wait
 * for its input -- with the realm slot being asked, at BIRTH_REALM_CHOICE --
 * and returns once the answer is filled in.
 */
extern void (*birth_view_hook)(enum birth_stage stage, int slot,
		struct birth_answer *answer);

const char *birth_question(enum birth_stage stage);
int birth_choices(enum birth_stage stage, const char **names, int max,
		int *cursor);
void birth_describe(enum birth_stage stage, int index);

void ui_init_birthstate_handlers(void);
int textui_do_birth(void);

//phantom
extern bool arg_force_name;

#endif
