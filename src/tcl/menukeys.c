/**
 * \file tcl/menukeys.c
 * \brief The Tcl/Tk front end's menus, as AppKit sees them.
 *
 * Copyright (c) 2026 Steven Pannell
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

/*
 * A file of its own, and the only one in the front end that does not include
 * angband.h.  The Objective-C runtime's headers declare an object_copy() of
 * their own, which the game's obj-pile.h contradicts, so the two cannot meet
 * in one translation unit.  Built on the Mac only; see TCL_Frontend.cmake.
 */
#include <objc/runtime.h>

void tcl_menu_keys_literal(void);

static BOOL keys_not_localized(id self, SEL cmd, id app)
{
	(void)self;
	(void)cmd;
	(void)app;
	return NO;
}

/**
 * Stop AppKit translating the menus' key equivalents for the keyboard layout.
 *
 * Since macOS 12 a key equivalent is remapped by default to where its
 * character sits on the current layout -- right for the Command-key shortcuts
 * of an application written for a US keyboard, and wrong here, where the key
 * shown is the character the game reads.  On a German layout "{" was shown as
 * Ö, "?" as ß and ")" as 0.
 *
 * The application's delegate is asked once, as it finishes launching, whether
 * to do this for every menu it will ever have.  Tk makes its application its
 * own delegate and does not answer, so the answer is given to Tk's class here,
 * before Tk_Init launches it.  That covers the menu bar, the item menu that
 * pops up in the Choice pane and the menus of the other windows alike, none of
 * which a walk of the menu bar's items could reach.
 *
 * Plain C through the Objective-C runtime, so the front end need not become
 * Objective-C for a dozen lines.  An older macOS never asks.
 */
void tcl_menu_keys_literal(void)
{
	Class tkapp = objc_getClass("TKApplication");

	if (!tkapp) return;
	(void)class_addMethod(tkapp,
		sel_registerName("applicationShouldAutomaticallyLocalizeKeyEquivalents:"),
		(IMP)keys_not_localized, OBJC_BOOL_IS_BOOL ? "B@:@" : "c@:@");
}
