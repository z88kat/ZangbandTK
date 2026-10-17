/**
 * \file buildid.h
 * \brief Compile in build details
 *
 * Copyright (c) 2011 Andi Sidwell
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

#ifndef BUILDID
#define BUILDID

#define VERSION_NAME	"ZangbandTK"

/*
 * The application's name, which is not the project's (DEC-143).
 *
 * Two applications are built from this source: ZangbandTK, the Tcl/Tk front
 * end, and Zangband, which is every other front end -- the Cocoa application,
 * the terminal builds, Windows, Linux, the Nintendo and DOS builds and the
 * browser.  APP_NAME is what a player sees on a title bar, in a window menu or
 * over the Hall of Fame.  The Tk build is the one with USE_TCL, which
 * CMakeLists.txt gives the core library as well as the front end, so that the
 * Hall of Fame in ui-score.c is named for the application it is shown in.
 *
 * VERSION_NAME above stays the project's, and must.  It names the folder every
 * front end keeps its characters in -- ~/Documents/ZangbandTK, ~/.angband/
 * ZangbandTK -- so changing it would lose every character made before the
 * change, and it is the "ZangbandTK 3.x" written into character dumps and the
 * scores, where "Zangband 3.x" would read as the 2005 original's line.
 */
#ifdef USE_TCL
#define APP_NAME	"ZangbandTK"
#else
#define APP_NAME	"Zangband"
#endif

/*
 * ZangbandTK's own version, which is not Angband's.
 *
 * Zangband's last release was 2.7.5-pre1 in 2005, and this line continues from
 * there rather than from the Angband 4.2.6 the code is built on.  Showing 4.2.6
 * would claim to be the twenty-somethingth release of a game we are not.
 *
 * Deliberately not taken from BUILD_ID.  Both build systems set that from
 * `git describe`, which here produces "angband-base-52-g7d0df8b19" -- a useful
 * description of a source tree and no kind of version number, and it was what
 * the title screen had been displaying.
 *
 * This is the only place it is written down.  src/Makefile.version seds it back
 * out of this file, and that is what reaches Info.plist and so the About panel,
 * so there is nothing to keep in step by hand.
 */
#define VERSION_STRING	"3.135.1"

/*
 * The Angband release this is built on.  Shown alongside our own version so it
 * is clear both what we are and what we came from.  Read back out of here by
 * src/Makefile.version, the same way as the line above.
 */
#define BASE_VERSION_STRING	"4.2.6"

extern const char *buildid;
extern const char *buildver;
extern const char *copyright;
extern const char *copyright_brief;
extern const char *homepage;

#endif /* BUILDID */
