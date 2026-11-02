/**
 * \file tcl/fonts.c
 * \brief The fonts the Tcl/Tk front end brings with it.
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
 * lib/tcl/fonts holds a few monospaced fonts under the SIL Open Font
 * Licence, so that the Display page can offer them whatever the player has
 * installed.  Each is made this process's own before Tk starts, and nothing
 * is installed on the player's system: Tk then finds them as it finds the
 * system's.  A font that will not register is simply not offered.
 *
 * A file of its own, without angband.h, because each platform's way of doing
 * this comes with its own headers: CoreText on the Mac, the Windows API, and
 * fontconfig under X11, which is what Tk's Xft draws with.  main-tcl.c reads
 * the directory and calls in here for each file.
 */
#include <stdbool.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

bool tcl_font_add(const char *path);

/* Make the font in the file at path (UTF-8) this process's own. */
bool tcl_font_add(const char *path)
{
#if defined(__APPLE__)
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
		(const UInt8 *)path, (CFIndex)strlen(path), false);
	bool ok;

	if (!url) return false;
	ok = CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess,
		NULL);
	CFRelease(url);
	return ok;
#elif defined(_WIN32)
	WCHAR wide[MAX_PATH];

	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, MAX_PATH))
		return false;
	return AddFontResourceExW(wide, FR_PRIVATE, NULL) > 0;
#elif defined(HAVE_FONTCONFIG)
	return FcConfigAppFontAddFile(NULL, (const FcChar8 *)path) == FcTrue;
#else
	(void)path;
	return false;
#endif
}
