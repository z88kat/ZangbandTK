/**
 * \file tcl/keyboard-pc.c
 * \brief The keyboard as the player has it, on Windows and on X11.
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
 * keyboard.c's counterpart off the Mac: the same two functions, for the
 * Keymaps window's picture of the keyboard.  The picture names its keys by
 * macOS virtual key code, which says where a key is rather than what it
 * types, so each is first turned into the PC key in the same place -- its
 * scan code, which Windows takes as it is and X11 takes plus 8, as evdev's
 * codes for the main block are the scan codes -- and the system is then
 * asked what that key types on the layout in use: on Windows through
 * MapVirtualKeyExW and ToUnicodeEx, on X11 through XKB.
 *
 * The shape is worked out from the layout, since neither system says how
 * many keys the keyboard has: a layout that puts a character of its own on
 * the key beside the left Shift -- the one an ISO keyboard has and an ANSI one
 * lacks -- was made for an ISO keyboard.  US layouts give that key either the
 * backslash key's characters (Windows) or < and > (XKB's pc symbols), and
 * both are read as ANSI.
 */
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#endif

void tcl_keyboard_char(int vk, int shifted, char *out, size_t len);
const char *tcl_keyboard_type(void);
#ifdef _WIN32
int tcl_keyboard_layout(const char *klid);
#endif

/*
 * The main block, macOS virtual key code to PC scan code (set 1).  Two keys
 * are not where their names say: on a Mac ISO keyboard the key at the top
 * left is kVK_ISO_Section (0x0A) and kVK_ANSI_Grave (0x32) moves beside the
 * left Shift, so 0x32 is the top left key on an ANSI keyboard and the ISO
 * extra key, scan code 0x56, on an ISO one.
 */
static const unsigned char mac_to_pc[0x33] = {
	/* 0x00 */ 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x22, 0x2C, 0x2D,
	/* 0x08 */ 0x2E, 0x2F, 0x29, 0x30, 0x10, 0x11, 0x12, 0x13,
	/* 0x10 */ 0x15, 0x14, 0x02, 0x03, 0x04, 0x05, 0x07, 0x06,
	/* 0x18 */ 0x0D, 0x0A, 0x08, 0x0C, 0x09, 0x0B, 0x1B, 0x18,
	/* 0x20 */ 0x16, 0x1A, 0x17, 0x19, 0x00, 0x26, 0x24, 0x28,
	/* 0x28 */ 0x25, 0x27, 0x2B, 0x33, 0x35, 0x31, 0x32, 0x34,
	/* 0x30 */ 0x00, 0x00, 0x29,
};

#define SC_BACKSLASH	0x2B
#define SC_ISO_EXTRA	0x56

/* The shape last reported, which decides where 0x32 is. */
static int iso;

#ifdef _WIN32

/* A layout asked about in place of the one in use; see tcl_keyboard_layout. */
static HKL layout_instead;

/*
 * Read the keys as the layout klid -- "00000407", German -- would have them,
 * without making it the one in use; "" goes back to the one in use.  For the
 * bridge test, which has a CI runner's US layout and wants to see a German
 * keyboard read.  Windows loads any of its layouts by number, installed for
 * the user or not.  False if it will not.
 */
int tcl_keyboard_layout(const char *klid)
{
	if (!klid || !klid[0]) {
		layout_instead = NULL;
		return 1;
	}
	layout_instead = LoadKeyboardLayoutA(klid, KLF_NOTELLSHELL);
	return layout_instead != NULL;
}

/* What the key with scan code sc types, shifted or not, as UTF-8. */
static void pc_char(unsigned sc, int shifted, char *out, size_t len)
{
	HKL layout = layout_instead ? layout_instead : GetKeyboardLayout(0);
	UINT vk = MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK_EX, layout);
	BYTE state[256];
	WCHAR w[8];
	int n;

	if (len) out[0] = 0;
	if (!vk || len < 2) return;

	memset(state, 0, sizeof(state));
	if (shifted) state[VK_SHIFT] = 0x80;

	/*
	 * Flag 4: do not change the keyboard's state, so that a dead key asked
	 * about here is not waiting to accent the player's next letter.  It is
	 * honoured from Windows 10 1607; before that the space below clears it.
	 * A dead key comes back as -1 with its accent in w[0], which is what
	 * its keycap shows.
	 */
	n = ToUnicodeEx(vk, sc, state, w, 8, 4, layout);
	if (n < 0) {
		WCHAR junk[8];

		memset(state, 0, sizeof(state));
		(void)ToUnicodeEx(VK_SPACE, MapVirtualKeyExW(VK_SPACE,
				MAPVK_VK_TO_VSC, layout), state, junk, 8, 4, layout);
		n = 1;
	}
	if (n <= 0 || w[0] < 0x20) return;

	n = WideCharToMultiByte(CP_UTF8, 0, w, n, out, (int)len - 1, NULL, NULL);
	out[n > 0 ? n : 0] = 0;
}

#else /* X11 */

/*
 * A connection of this file's own, made afresh whenever the shape is asked
 * for -- which the window does first, each time it reads the keyboard -- so
 * that a layout changed since is what is read.  Tk's connection keeps its
 * keymap up to date by handling events, and this one handles none.
 */
static Display *display;

static void pc_open(void)
{
	if (display) XCloseDisplay(display);
	display = XOpenDisplay(NULL);
}

/*
 * What the key with scan code sc types, shifted or not, as UTF-8: its keysym
 * in the group in use, which XKB turns into the locale's encoding -- UTF-8,
 * which the game insists on -- and a dead key into its spacing accent.
 */
static void pc_char(unsigned sc, int shifted, char *out, size_t len)
{
	XkbStateRec state;
	KeySym sym;
	int n, extra = 0;

	if (len) out[0] = 0;
	if (!display) pc_open();
	if (!display || len < 2) return;

	if (XkbGetState(display, XkbUseCoreKbd, &state) != Success) return;
	sym = XkbKeycodeToKeysym(display, (KeyCode)(sc + 8), state.group,
			shifted ? 1 : 0);
	if (sym == NoSymbol) return;

	n = XkbTranslateKeySym(display, &sym, 0, out, (int)len - 1, &extra);
	out[n > 0 ? n : 0] = 0;
	if ((unsigned char)out[0] < 0x20) out[0] = 0;
}

#endif /* _WIN32 */

/*
 * What key vk types on the current layout, shifted or not, as UTF-8 in out;
 * "" for a key off the main block, or one that types nothing.
 */
void tcl_keyboard_char(int vk, int shifted, char *out, size_t len)
{
	unsigned sc;

	if (len) out[0] = 0;
	if (vk < 0 || vk >= (int)sizeof(mac_to_pc)) return;
	sc = (vk == 0x32 && iso) ? SC_ISO_EXTRA : mac_to_pc[vk];
	if (sc) pc_char(sc, shifted, out, len);
}

/* "ansi" or "iso", read from the layout, for the reason at the top. */
const char *tcl_keyboard_type(void)
{
	char extra[16], backslash[16];

#ifndef _WIN32
	pc_open();
#endif
	pc_char(SC_ISO_EXTRA, 0, extra, sizeof(extra));
	pc_char(SC_BACKSLASH, 0, backslash, sizeof(backslash));
	iso = extra[0] && strcmp(extra, backslash) != 0
			&& !(strcmp(extra, "<") == 0 && strcmp(backslash, "\\") == 0);
	return iso ? "iso" : "ansi";
}
