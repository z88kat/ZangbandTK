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
#define VERSION_STRING	"3.72.0"

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
