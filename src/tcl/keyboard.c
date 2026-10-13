/**
 * \file tcl/keyboard.c
 * \brief The keyboard as the player has it, for the Keymaps window.
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
 * The Keymaps window draws a keyboard, and one drawn with US labels is wrong
 * on most of the world's Macs: on a German one the key beside L types an
 * umlaut and the one beside Shift is < and >.  This asks macOS what each key
 * of the main block types on the layout in use, plain and shifted, and what
 * shape the keyboard is -- ANSI, ISO with its extra key, or JIS.
 *
 * Keys are macOS virtual key codes, the kVK_ constants of Carbon's Events.h,
 * which name a key by where it is rather than by what it types.
 *
 * A file of its own, like menukeys.c, because Carbon's headers do not mix
 * with the game's.  Built on the Mac only; see TCL_Frontend.cmake.
 */
#include <Carbon/Carbon.h>
#include <string.h>

void tcl_keyboard_char(int vk, int shifted, char *out, size_t len);
const char *tcl_keyboard_type(void);

/*
 * What key vk types on the current layout, shifted or not, as UTF-8 in out;
 * "" when it types nothing.  A dead key is given as the accent it would put
 * on the next letter, which is what its keycap shows.
 */
void tcl_keyboard_char(int vk, int shifted, char *out, size_t len)
{
	TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
	CFDataRef data;

	if (len) out[0] = 0;
	if (!source) return;

	data = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
	if (data) {
		const UCKeyboardLayout *layout =
			(const UCKeyboardLayout *)CFDataGetBytePtr(data);
		UniChar chars[4];
		UniCharCount n = 0;
		UInt32 dead = 0;

		if (UCKeyTranslate(layout, (UInt16)vk, kUCKeyActionDisplay,
				shifted ? (shiftKey >> 8) & 0xFF : 0, LMGetKbdType(),
				kUCKeyTranslateNoDeadKeysMask, &dead, 4, &n, chars) == noErr
				&& n > 0) {
			CFStringRef s = CFStringCreateWithCharacters(NULL, chars, n);

			if (s) {
				if (!CFStringGetCString(s, out, (CFIndex)len,
						kCFStringEncodingUTF8) && len) {
					out[0] = 0;
				}
				CFRelease(s);
			}
		}
	}
	CFRelease(source);
}

/* "ansi", "iso" or "jis": which physical keyboard this is. */
const char *tcl_keyboard_type(void)
{
	switch (KBGetLayoutType(LMGetKbdType())) {
		case kKeyboardISO: return "iso";
		case kKeyboardJIS: return "jis";
		default: return "ansi";
	}
}
